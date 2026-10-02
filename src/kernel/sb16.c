/* NixDOS 2 - Sound Blaster 16 driver
 *
 * 16-bit stereo playback with auto-init DMA (channel 5) from a ring buffer of
 * two halves. Every finished half raises IRQ 5; programs poll sb_halves_done()
 * and refill the half that just finished (see ports/sdl/sdl_mixer.c).
 * QEMU: -device sb16 (base 0x220, IRQ 5, DMA 1/5).
 */
#include "kernel.h"

#define SB_BASE     0x220
#define SB_RESET    (SB_BASE + 0x6)
#define SB_READ     (SB_BASE + 0xA)
#define SB_WRITE    (SB_BASE + 0xC)
#define SB_STATUS   (SB_BASE + 0xE)
#define SB_ACK16    (SB_BASE + 0xF)
#define SB_MIX_ADDR (SB_BASE + 0x4)
#define SB_MIX_DATA (SB_BASE + 0x5)
#define SB_IRQ      5

static int present, playing;
static int dsp_major, dsp_minor;
static volatile u32 halves_done;

static int dsp_write(u8 v)
{
    for (int i = 0; i < 100000; i++)
        if (!(inb(SB_WRITE) & 0x80)) { outb(SB_WRITE, v); return 0; }
    return -1;
}

static int dsp_read(void)
{
    for (int i = 0; i < 100000; i++)
        if (inb(SB_STATUS) & 0x80) return inb(SB_READ);
    return -1;
}

static void sb_irq(struct regs *r)
{
    (void)r;
    inb(SB_ACK16);              /* acknowledge the 16-bit DMA interrupt */
    halves_done++;
}

int sb_init(void)
{
    outb(SB_RESET, 1);
    for (int i = 0; i < 100; i++) io_wait();   /* >= 3 us */
    outb(SB_RESET, 0);
    if (dsp_read() != 0xAA) return 0;
    dsp_write(0xE1);
    dsp_major = dsp_read();
    dsp_minor = dsp_read();
    if (dsp_major < 4) return 0;               /* need an SB16 for 16-bit DMA */

    outb(SB_MIX_ADDR, 0x80); outb(SB_MIX_DATA, 0x02);   /* IRQ 5 */
    outb(SB_MIX_ADDR, 0x81); outb(SB_MIX_DATA, 0x22);   /* DMA 1 and 5 */
    irq_install(SB_IRQ, sb_irq);
    irq_unmask(SB_IRQ);
    present = 1;
    return 1;
}

int sb_present(void) { return present; }
int sb_version(void) { return dsp_major << 8 | dsp_minor; }
u32 sb_halves_done(void) { return halves_done; }

void sb_stop(void)
{
    if (!playing) return;
    dsp_write(0xD5);            /* pause 16-bit DMA */
    dsp_write(0xD9);            /* exit auto-init */
    outb(0xD4, 0x05);           /* mask DMA channel 5 */
    playing = 0;
}

/* Start playing the whole ring buffer (two halves) in a loop.
   Returns the size of one half in bytes, or -1. */
int sb_start(u32 rate)
{
    if (!present) return -1;
    sb_stop();
    if (rate < 5000) rate = 5000;
    if (rate > 44100) rate = 44100;

    u32 addr = SB_DMA_BUF;
    u32 words = SB_DMA_SIZE / 2;
    memset((void *)SB_DMA_BUF, 0, SB_DMA_SIZE);

    /* 16-bit DMA controller, channel 5 (= channel 1 of the slave controller) */
    outb(0xD4, 0x05);                           /* mask */
    outb(0xD8, 0x00);                           /* clear flip-flop */
    outb(0xD6, 0x59);                           /* single, auto-init, memory -> device, ch 1 */
    outb(0xC4, (u8)((addr >> 1) & 0xFF));
    outb(0xC4, (u8)((addr >> 9) & 0xFF));
    outb(0xC6, (u8)((words - 1) & 0xFF));
    outb(0xC6, (u8)((words - 1) >> 8));
    outb(0x8B, (u8)((addr >> 16) & 0xFE));      /* page register */
    outb(0xD4, 0x01);                           /* unmask */

    dsp_write(0x41);                            /* output sample rate */
    dsp_write((u8)(rate >> 8));
    dsp_write((u8)(rate & 0xFF));
    u32 half_samples = words / 2;               /* 16-bit samples per half (both channels) */
    halves_done = 0;
    dsp_write(0xB6);                            /* 16-bit, auto-init, FIFO */
    dsp_write(0x30);                            /* stereo, signed */
    dsp_write((u8)((half_samples - 1) & 0xFF));
    dsp_write((u8)((half_samples - 1) >> 8));
    dsp_write(0xD1);                            /* speaker on */
    playing = 1;
    return SB_DMA_SIZE / 2;
}
