/* NixDOS 2 - memory detection and a first-fit heap allocator */
#include "kernel.h"

struct heap_block {
    u32 size;                   /* payload bytes */
    u32 free;
    struct heap_block *next;
    u32 magic;
};

#define BLOCK_MAGIC 0x4E584842  /* "NXHB" */
#define HDR sizeof(struct heap_block)

heap_t kheap;
static u32 total_kb;

void heap_init(heap_t *h, u32 base, u32 size)
{
    h->base = base;
    h->size = size;
    h->head = (heap_block_t *)base;
    h->head->size = size - HDR;
    h->head->free = 1;
    h->head->next = NULL;
    h->head->magic = BLOCK_MAGIC;
}

void *heap_alloc(heap_t *h, u32 n)
{
    if (n == 0) n = 1;
    n = (n + 15) & ~15u;
    for (heap_block_t *b = h->head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + HDR + 16) {
            heap_block_t *rest = (heap_block_t *)((u8 *)b + HDR + n);
            rest->size = b->size - n - HDR;
            rest->free = 1;
            rest->next = b->next;
            rest->magic = BLOCK_MAGIC;
            b->next = rest;
            b->size = n;
        }
        b->free = 0;
        return (u8 *)b + HDR;
    }
    return NULL;
}

void heap_free(heap_t *h, void *p)
{
    if (!p) return;
    heap_block_t *b = (heap_block_t *)((u8 *)p - HDR);
    if (b->magic != BLOCK_MAGIC || b->free) return;   /* ignore bad/double frees */
    b->free = 1;
    /* coalesce neighbouring free blocks */
    for (heap_block_t *c = h->head; c; c = c->next) {
        while (c->free && c->next && c->next->free) {
            c->size += HDR + c->next->size;
            c->next = c->next->next;
        }
    }
}

void heap_stats(heap_t *h, u32 *used, u32 *free_bytes)
{
    u32 u = 0, f = 0;
    for (heap_block_t *b = h->head; b; b = b->next) {
        if (b->free) f += b->size;
        else u += b->size;
    }
    *used = u;
    *free_bytes = f;
}

static u8 cmos(u8 reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

void mem_init(void)
{
    /* CMOS 0x30/0x31: KiB above 1 MiB (max 63 MiB);
       0x34/0x35: 64 KiB blocks above 16 MiB (set by QEMU/Bochs BIOSes) */
    u32 ext = cmos(0x30) | (cmos(0x31) << 8);
    u32 above16 = cmos(0x34) | (cmos(0x35) << 8);
    if (above16)
        total_kb = 16 * 1024 + above16 * 64;
    else
        total_kb = 1024 + ext;
    heap_init(&kheap, KHEAP_START, KHEAP_END - KHEAP_START);
}

u32 mem_total_kb(void) { return total_kb; }

void *kmalloc(u32 n) { return heap_alloc(&kheap, n); }
void kfree(void *p) { heap_free(&kheap, p); }

void *kzalloc(u32 n)
{
    void *p = kmalloc(n);
    if (p) memset(p, 0, n);
    return p;
}
