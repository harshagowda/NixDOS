/* NixDOS 2 - VGA mode switching: 80x25 text <-> 320x200x256 (mode 13h)
 *
 * Done by programming the VGA registers directly (no BIOS in protected mode).
 * Mode 13h overwrites plane 2, where the text-mode font lives, and the DAC
 * palette, so both are saved before switching and restored afterwards,
 * together with the text screen contents.
 */
#include "kernel.h"

#define VGA_AC_INDEX    0x3C0
#define VGA_AC_WRITE    0x3C0
#define VGA_AC_READ     0x3C1
#define VGA_MISC_WRITE  0x3C2
#define VGA_SEQ_INDEX   0x3C4
#define VGA_SEQ_DATA    0x3C5
#define VGA_DAC_READ    0x3C7
#define VGA_DAC_WRITE   0x3C8
#define VGA_DAC_DATA    0x3C9
#define VGA_MISC_READ   0x3CC
#define VGA_GC_INDEX    0x3CE
#define VGA_GC_DATA     0x3CF
#define VGA_CRTC_INDEX  0x3D4
#define VGA_CRTC_DATA   0x3D5
#define VGA_INSTAT_READ 0x3DA

static const u8 regs_320x200x256[] = {
    0x63,                                                   /* misc */
    0x03, 0x01, 0x0F, 0x00, 0x0E,                           /* sequencer */
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,         /* crtc */
    0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF,   /* graphics controller */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,         /* attribute controller */
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x41, 0x00, 0x0F, 0x00, 0x00
};

static const u8 regs_80x25_text[] = {
    0x67,
    0x03, 0x00, 0x03, 0x00, 0x02,
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
    0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x0C, 0x00, 0x0F, 0x08, 0x00
};

static int graphics;
static u8 *saved_font;          /* 256 chars x 32 bytes from plane 2 */
static u8 saved_dac[768];
static u16 saved_text[CON_W * CON_H];

static void write_regs(const u8 *r)
{
    outb(VGA_MISC_WRITE, *r++);
    for (u8 i = 0; i < 5; i++) { outb(VGA_SEQ_INDEX, i); outb(VGA_SEQ_DATA, *r++); }
    /* unlock CRTC registers 0-7 */
    outb(VGA_CRTC_INDEX, 0x03); outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11); outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);
    u8 crtc[25];
    memcpy(crtc, r, 25);
    crtc[0x03] |= 0x80;
    crtc[0x11] &= ~0x80;
    for (u8 i = 0; i < 25; i++) { outb(VGA_CRTC_INDEX, i); outb(VGA_CRTC_DATA, crtc[i]); }
    r += 25;
    for (u8 i = 0; i < 9; i++) { outb(VGA_GC_INDEX, i); outb(VGA_GC_DATA, *r++); }
    for (u8 i = 0; i < 21; i++) {
        inb(VGA_INSTAT_READ);
        outb(VGA_AC_INDEX, i);
        outb(VGA_AC_WRITE, *r++);
    }
    inb(VGA_INSTAT_READ);
    outb(VGA_AC_INDEX, 0x20);       /* re-enable the display */
}

/* Map plane 2 at 0xA0000 for linear access; returns the old register values. */
static void plane2_begin(u8 *save)
{
    outb(VGA_SEQ_INDEX, 2); save[0] = inb(VGA_SEQ_DATA);
    outb(VGA_SEQ_INDEX, 4); save[1] = inb(VGA_SEQ_DATA);
    outb(VGA_GC_INDEX, 4);  save[2] = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 5);  save[3] = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 6);  save[4] = inb(VGA_GC_DATA);
    outb(VGA_SEQ_INDEX, 2); outb(VGA_SEQ_DATA, 0x04);   /* write plane 2 only */
    outb(VGA_SEQ_INDEX, 4); outb(VGA_SEQ_DATA, 0x06);   /* sequential, no odd/even */
    outb(VGA_GC_INDEX, 4);  outb(VGA_GC_DATA, 0x02);    /* read plane 2 */
    outb(VGA_GC_INDEX, 5);  outb(VGA_GC_DATA, 0x00);    /* write mode 0, no odd/even */
    outb(VGA_GC_INDEX, 6);  outb(VGA_GC_DATA, 0x04);    /* map 0xA0000, 64 KiB */
}

static void plane2_end(const u8 *save)
{
    outb(VGA_SEQ_INDEX, 2); outb(VGA_SEQ_DATA, save[0]);
    outb(VGA_SEQ_INDEX, 4); outb(VGA_SEQ_DATA, save[1]);
    outb(VGA_GC_INDEX, 4);  outb(VGA_GC_DATA, save[2]);
    outb(VGA_GC_INDEX, 5);  outb(VGA_GC_DATA, save[3]);
    outb(VGA_GC_INDEX, 6);  outb(VGA_GC_DATA, save[4]);
}

static void dac_read(u8 *rgb)
{
    outb(VGA_DAC_READ, 0);
    for (int i = 0; i < 768; i++) rgb[i] = inb(VGA_DAC_DATA);
}

static void dac_write(const u8 *rgb)
{
    outb(VGA_DAC_WRITE, 0);
    for (int i = 0; i < 768; i++) outb(VGA_DAC_DATA, rgb[i]);
}

int vga_is_graphics(void) { return graphics; }

void vga_set_graphics(void)
{
    if (graphics) return;
    u8 save[5];
    if (!saved_font) saved_font = kmalloc(8192);
    memcpy(saved_text, (void *)0xB8000, sizeof(saved_text));
    dac_read(saved_dac);
    if (saved_font) {
        plane2_begin(save);
        memcpy(saved_font, (void *)0xA0000, 8192);
        plane2_end(save);
    }
    con_set_buffer(saved_text);         /* console output goes here meanwhile */
    write_regs(regs_320x200x256);
    memset((void *)0xA0000, 0, 64000);
    graphics = 1;
}

void vga_set_text(void)
{
    if (!graphics) return;
    u8 save[5];
    write_regs(regs_80x25_text);
    if (saved_font) {
        plane2_begin(save);
        memset((void *)0xA0000, 0, 65536);
        memcpy((void *)0xA0000, saved_font, 8192);
        plane2_end(save);
    }
    dac_write(saved_dac);
    memcpy((void *)0xB8000, saved_text, sizeof(saved_text));
    con_set_buffer(NULL);
    con_show_cursor(1);
    graphics = 0;
}

/* rgb: count x {r, g, b}, 8 bits per component */
void vga_set_palette(const u8 *rgb, int first, int count)
{
    if (first < 0 || count <= 0 || first + count > 256) return;
    outb(VGA_DAC_WRITE, (u8)first);
    for (int i = 0; i < count * 3; i++) outb(VGA_DAC_DATA, rgb[i] >> 2);
}

void vga_wait_vsync(void)
{
    for (int i = 0; i < 100000 && (inb(VGA_INSTAT_READ) & 0x08); i++) ;
    for (int i = 0; i < 100000 && !(inb(VGA_INSTAT_READ) & 0x08); i++) ;
}

void vga_blit(const u8 *frame)
{
    if (graphics) memcpy((void *)0xA0000, frame, 64000);
}
