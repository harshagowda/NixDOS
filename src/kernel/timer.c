/* NixDOS 2 - PIT timer (100 Hz) and PC speaker */
#include "kernel.h"

static volatile u32 ticks;

static void timer_irq(struct regs *r)
{
    ticks++;
    check_abort(r);
}

void timer_init(void)
{
    u32 div = 1193182 / TIMER_HZ;
    outb(0x43, 0x36);
    outb(0x40, (u8)(div & 0xFF));
    outb(0x40, (u8)(div >> 8));
    irq_install(0, timer_irq);
    irq_unmask(0);
}

u32 timer_ticks(void) { return ticks; }
u32 timer_ms(void) { return ticks * (1000 / TIMER_HZ); }

void sleep_ms(u32 ms)
{
    u32 end = ticks + (ms + (1000 / TIMER_HZ) - 1) / (1000 / TIMER_HZ);
    while ((int)(end - ticks) > 0) {
        prog_check_ctrl_c();
        hlt();
    }
}

void speaker_beep(u32 freq, u32 ms)
{
    if (freq < 20) freq = 20;
    u32 div = 1193182 / freq;
    outb(0x43, 0xB6);
    outb(0x42, (u8)(div & 0xFF));
    outb(0x42, (u8)(div >> 8));
    outb(0x61, inb(0x61) | 0x03);
    sleep_ms(ms);
    outb(0x61, inb(0x61) & 0xFC);
}
