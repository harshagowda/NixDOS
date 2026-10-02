/* NixDOS 2 - COM1 serial driver (output mirror + interrupt-driven input) */
#include "kernel.h"

#define COM1 0x3F8

static int present;

static void serial_irq(struct regs *r)
{
    (void)r;
    while (inb(COM1 + 5) & 0x01) {
        int c = inb(COM1);
        if (c == '\r') c = '\n';
        else if (c == 0x7F) c = '\b';
        kbd_push(c);
    }
}

int serial_init(void)
{
    outb(COM1 + 1, 0x00);   /* interrupts off */
    outb(COM1 + 3, 0x80);   /* DLAB on */
    outb(COM1 + 0, 0x03);   /* 38400 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0xC7);   /* FIFO on, 14-byte threshold */
    outb(COM1 + 4, 0x1E);   /* loopback test */
    outb(COM1 + 0, 0xAE);
    if (inb(COM1 + 0) != 0xAE) {
        present = 0;
        return 0;
    }
    outb(COM1 + 4, 0x0B);   /* normal mode, OUT2 (IRQ enable), RTS, DTR */
    present = 1;
    irq_install(4, serial_irq);
    outb(COM1 + 1, 0x01);   /* interrupt on received data */
    irq_unmask(4);
    return 1;
}

int serial_present(void) { return present; }

void serial_putc(char c)
{
    if (!present) return;
    for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++) ;
    outb(COM1, (u8)c);
}
