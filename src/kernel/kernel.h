/* NixDOS 2 - shared kernel declarations */
#ifndef NIXDOS_KERNEL_H
#define NIXDOS_KERNEL_H

#include <stdarg.h>

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed char        i8;
typedef short              i16;
typedef int                i32;
typedef unsigned int       size_t;

#define NULL ((void *)0)

#define NIXDOS_VERSION "NixDOS 2.0"

/* ---------------------------------------------------------------------------
 * Physical memory map (paging is off, everything is identity mapped)
 *
 *   0x00000500  pointer to the program API table (read by compiled code)
 *   0x00010000  kernel image (+ .bss), must end below 0x80000
 *   0x00090000  kernel stack top (grows down)
 *   0x000B8000  VGA text buffer
 *   0x00100000  kernel heap (above 1 MiB - needs A20)
 *   0x00400000  user program code
 *   0x00480000  user program data (globals, string literals)
 *   0x00800000  user program stack top (grows down)
 *   0x00800000+ user program heap (malloc), up to the end of RAM / 32 MiB
 * ------------------------------------------------------------------------- */
#define API_PTR_ADDR    0x00000500u
#define KHEAP_START     0x00100000u
#define KHEAP_END       0x00400000u
#define PROG_CODE       0x00400000u
#define PROG_CODE_MAX   0x00080000u
#define PROG_DATA       0x00480000u
#define PROG_DATA_MAX   0x00080000u
#define PROG_STACK_TOP  0x00800000u
#define PROG_HEAP_START 0x00800000u
#define PROG_HEAP_LIMIT 0x02000000u

/* ---- port I/O ------------------------------------------------------------ */
static inline void outb(u16 port, u8 v)  { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port)); }
static inline void outw(u16 port, u16 v) { __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(port)); }
static inline u8   inb(u16 port)  { u8 v;  __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }
static inline u16  inw(u16 port)  { u16 v; __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(port)); return v; }
static inline void io_wait(void)  { outb(0x80, 0); }
static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

/* ---- string.c ------------------------------------------------------------ */
void  *memset(void *d, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *d, const char *s);
char  *strncpy(char *d, const char *s, size_t n);
char  *strcat(char *d, const char *s);
char  *strchr(const char *s, int c);
int    atoi(const char *s);
int    isdigit(int c);
int    isalpha(int c);
int    isalnum(int c);
int    isspace(int c);
int    toupper(int c);
int    tolower(int c);
int    ends_with(const char *s, const char *suffix);

/* ---- printf.c ------------------------------------------------------------ */
typedef void (*out_fn)(char c, void *ctx);
int  kformat(out_fn out, void *ctx, const char *fmt, va_list ap);
int  kprintf(const char *fmt, ...);
int  kvprintf(const char *fmt, va_list ap);
int  ksnprintf(char *buf, size_t n, const char *fmt, ...);
int  kvsnprintf(char *buf, size_t n, const char *fmt, va_list ap);

/* ---- console.c (VGA text mode + serial mirror) --------------------------- */
#define CON_W 80
#define CON_H 25
void con_init(void);
void con_clear(void);
void con_putc(char c);
void con_write(const char *s);
void con_setcolor(int fg, int bg);
u8   con_getattr(void);
void con_setattr(u8 a);
void con_gotoxy(int x, int y);
int  con_getx(void);
int  con_gety(void);
void con_putat(int x, int y, char c, u8 attr);
void con_set_mirror(int on);
void con_show_cursor(int on);

/* ---- serial.c ------------------------------------------------------------ */
int  serial_init(void);
int  serial_present(void);
void serial_putc(char c);

/* ---- interrupts (idt.c) -------------------------------------------------- */
struct regs {
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags;
};
typedef void (*irq_handler_t)(struct regs *r);
void idt_init(void);
void irq_install(int irq, irq_handler_t h);
void irq_unmask(int irq);

/* ---- timer.c ------------------------------------------------------------- */
#define TIMER_HZ 100
void timer_init(void);
u32  timer_ticks(void);
u32  timer_ms(void);
void sleep_ms(u32 ms);
void speaker_beep(u32 freq, u32 ms);

/* ---- keyboard.c ---------------------------------------------------------- */
#define KEY_UP     0x101
#define KEY_DOWN   0x102
#define KEY_LEFT   0x103
#define KEY_RIGHT  0x104
#define KEY_HOME   0x105
#define KEY_END    0x106
#define KEY_PGUP   0x107
#define KEY_PGDN   0x108
#define KEY_DEL    0x109
#define KEY_F1     0x10A
void kbd_init(void);
void kbd_push(int key);          /* also used by the serial driver */
int  kbd_haskey(void);
int  kbd_getkey(void);           /* blocking */
int  kbd_trygetkey(void);        /* -1 if none */
int  readline(char *buf, int max, char **history, int nhistory);

/* ---- rtc.c --------------------------------------------------------------- */
struct rtc_time { int sec, min, hour, day, month, year; };
void rtc_read(struct rtc_time *t);
void rtc_write(const struct rtc_time *t);
u32  rtc_unix_time(void);

/* ---- mem.c --------------------------------------------------------------- */
typedef struct heap_block heap_block_t;
typedef struct { heap_block_t *head; u32 base, size; } heap_t;
void  heap_init(heap_t *h, u32 base, u32 size);
void *heap_alloc(heap_t *h, u32 n);
void  heap_free(heap_t *h, void *p);
void  heap_stats(heap_t *h, u32 *used, u32 *free_bytes);
void  mem_init(void);
u32   mem_total_kb(void);
void *kmalloc(u32 n);
void *kzalloc(u32 n);
void  kfree(void *p);
extern heap_t kheap;

/* ---- ata.c --------------------------------------------------------------- */
int ata_init(void);
int ata_present(void);
u32 ata_sectors(void);
const char *ata_model(void);
int ata_read(u32 lba, u32 count, void *buf);
int ata_write(u32 lba, u32 count, const void *buf);

/* ---- fs.c (NXFS) --------------------------------------------------------- */
#define FS_NAME_MAX   40
#define FS_MAX_FILES  96
#define FS_MAX_SIZE   (128 * 512)
struct fs_dirent {
    char name[FS_NAME_MAX];
    u32  size;
    u32  flags;
    u32  mtime;
    u32  reserved[3];
};
void fs_init(void);
int  fs_on_disk(void);
int  fs_format(void);
int  fs_find(const char *name);
int  fs_read(const char *name, void *buf, u32 max);   /* -> bytes or -1 */
char *fs_read_alloc(const char *name, u32 *size);     /* NUL terminated, kfree() it */
int  fs_write(const char *name, const void *data, u32 len);
int  fs_delete(const char *name);
int  fs_rename(const char *from, const char *to);
const struct fs_dirent *fs_entry(int i);              /* NULL if slot unused */
int  fs_count(void);
int  fs_valid_name(const char *name);

/* ---- program.c (user programs + API) ------------------------------------- */
#define NXE_MAGIC 0x3145584E /* "NXE1" */
struct nxe_header { u32 magic, entry, code_size, data_size; };
extern volatile int prog_running;
extern volatile int abort_requested;
void prog_init(void);
int  run_program(u32 entry, int argc, char **argv);
int  run_nxe_file(const char *name, int argc, char **argv);
void check_abort(struct regs *r);
void prog_fault(struct regs *r, const char *what);
void prog_check_ctrl_c(void);

/* ---- cc.c (NixC compiler) ------------------------------------------------ */
struct cc_builtin { const char *name; int nargs; int rettype; };
extern const struct cc_builtin cc_builtins[];
extern void *api_table[];
struct cc_result { u32 entry, code_size, data_size, data_init; };
int cc_compile(const char *src, const char *filename, struct cc_result *out);

/* ---- editor.c / shell.c -------------------------------------------------- */
void editor_run(const char *filename);
void shell_run(void);

/* ---- misc ---------------------------------------------------------------- */
typedef u32 k_jmp_buf[6];
int  k_setjmp(k_jmp_buf b);
void k_longjmp(k_jmp_buf b, int v) __attribute__((noreturn));
int  call_on_stack(u32 entry, u32 stack_top, int argc, char **argv);
void panic(const char *fmt, ...) __attribute__((noreturn));
void sys_reboot(void) __attribute__((noreturn));
void sys_shutdown(void) __attribute__((noreturn));
extern u32 boot_drive;

#endif
