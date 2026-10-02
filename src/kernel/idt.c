/* NixDOS 2 - IDT, 8259 PIC remapping, exception and IRQ dispatch */
#include "kernel.h"

struct idt_entry {
    u16 base_lo;
    u16 sel;
    u8  zero;
    u8  flags;
    u16 base_hi;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

extern u32 isr_stub_table[48];

static struct idt_entry idt[256];
static irq_handler_t irq_handlers[16];

static const char *exception_names[32] = {
    "Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range",
    "Invalid opcode", "Device not available", "Double fault", "Coprocessor overrun",
    "Invalid TSS", "Segment not present", "Stack fault", "General protection fault",
    "Page fault", "Reserved", "x87 FPU error", "Alignment check", "Machine check",
    "SIMD FP error", "Virtualization", "Control protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Hypervisor injection",
    "VMM communication", "Security", "Reserved"
};

static void set_gate(int n, u32 base)
{
    idt[n].base_lo = (u16)(base & 0xFFFF);
    idt[n].base_hi = (u16)(base >> 16);
    idt[n].sel = 0x08;
    idt[n].zero = 0;
    idt[n].flags = 0x8E;    /* present, ring 0, 32-bit interrupt gate */
}

static void pic_remap(void)
{
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();   /* master -> vectors 32..39 */
    outb(0xA1, 0x28); io_wait();   /* slave  -> vectors 40..47 */
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();
    outb(0x21, 0xFB);              /* everything masked except the cascade */
    outb(0xA1, 0xFF);
}

void irq_unmask(int irq)
{
    if (irq < 8) outb(0x21, inb(0x21) & ~(1 << irq));
    else outb(0xA1, inb(0xA1) & ~(1 << (irq - 8)));
}

void irq_install(int irq, irq_handler_t h)
{
    irq_handlers[irq] = h;
}

void idt_init(void)
{
    struct idt_ptr p;
    for (int i = 0; i < 48; i++)
        set_gate(i, isr_stub_table[i]);
    pic_remap();
    p.limit = sizeof(idt) - 1;
    p.base = (u32)&idt;
    __asm__ volatile("lidt %0" : : "m"(p));
}

void isr_handler(struct regs *r)
{
    if (r->int_no < 32) {
        const char *name = exception_names[r->int_no];
        if (prog_running) {
            prog_fault(r, name);
            return;
        }
        panic("CPU exception %d (%s), error code %x\n"
              "  EIP=%p EAX=%p EBX=%p ECX=%p EDX=%p\n"
              "  ESI=%p EDI=%p EBP=%p EFLAGS=%p",
              r->int_no, name, r->err_code, r->eip, r->eax, r->ebx, r->ecx,
              r->edx, r->esi, r->edi, r->ebp, r->eflags);
    }

    int irq = (int)r->int_no - 32;

    /* spurious interrupts from the PICs carry no EOI */
    if (irq == 7) {
        outb(0x20, 0x0B);
        if (!(inb(0x20) & 0x80)) return;
    } else if (irq == 15) {
        outb(0xA0, 0x0B);
        if (!(inb(0xA0) & 0x80)) { outb(0x20, 0x20); return; }
    }

    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);

    if (irq >= 0 && irq < 16 && irq_handlers[irq])
        irq_handlers[irq](r);
}
