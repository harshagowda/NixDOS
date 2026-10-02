/* NixDOS 2 - VGA text-mode console (80x25), mirrored to the serial port */
#include "kernel.h"

#define VGA_HW ((volatile u16 *)0xB8000)

static volatile u16 *VGA = VGA_HW;    /* or a shadow buffer while in graphics mode */

static int cx, cy;
static u8 attr = 0x07;
static int mirror = 1;

void con_set_buffer(u16 *buf)
{
    VGA = buf ? buf : VGA_HW;
}

static void update_cursor(void)
{
    if (VGA != VGA_HW) return;
    u16 pos = (u16)(cy * CON_W + cx);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)(pos >> 8));
}

void con_show_cursor(int on)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, on ? 0x0D : 0x20);
    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0E);
}

void con_init(void)
{
    cx = cy = 0;
    attr = 0x07;
    con_show_cursor(1);
    con_clear();
}

void con_clear(void)
{
    for (int i = 0; i < CON_W * CON_H; i++)
        VGA[i] = (u16)(attr << 8 | ' ');
    cx = cy = 0;
    update_cursor();
    if (mirror) {
        /* ANSI: clear screen + home, so serial terminals follow along */
        const char *s = "\x1b[2J\x1b[H";
        while (*s) serial_putc(*s++);
    }
}

static void scroll(void)
{
    for (int i = 0; i < CON_W * (CON_H - 1); i++)
        VGA[i] = VGA[i + CON_W];
    for (int i = CON_W * (CON_H - 1); i < CON_W * CON_H; i++)
        VGA[i] = (u16)(attr << 8 | ' ');
    cy = CON_H - 1;
}

static void vga_putc(char c)
{
    switch (c) {
    case '\n':
        cx = 0;
        cy++;
        break;
    case '\r':
        cx = 0;
        break;
    case '\b':
        /* non-destructive, like a terminal: erase with "\b \b" */
        if (cx > 0) cx--;
        else if (cy > 0) { cy--; cx = CON_W - 1; }
        break;
    case '\t':
        cx = (cx + 8) & ~7;
        break;
    default:
        VGA[cy * CON_W + cx] = (u16)(attr << 8 | (u8)c);
        cx++;
        break;
    }
    if (cx >= CON_W) { cx = 0; cy++; }
    if (cy >= CON_H) scroll();
}

void con_putc(char c)
{
    vga_putc(c);
    update_cursor();
    if (mirror) {
        if (c == '\n') serial_putc('\r');
        serial_putc(c);
    }
}

void con_write(const char *s)
{
    while (*s) con_putc(*s++);
}

void con_setcolor(int fg, int bg) { attr = (u8)(((bg & 0x0F) << 4) | (fg & 0x0F)); }
u8   con_getattr(void) { return attr; }
void con_setattr(u8 a) { attr = a; }

void con_gotoxy(int x, int y)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= CON_W) x = CON_W - 1;
    if (y >= CON_H) y = CON_H - 1;
    cx = x;
    cy = y;
    update_cursor();
}

int con_getx(void) { return cx; }
int con_gety(void) { return cy; }

void con_putat(int x, int y, char c, u8 a)
{
    if (x >= 0 && x < CON_W && y >= 0 && y < CON_H)
        VGA[y * CON_W + x] = (u16)(a << 8 | (u8)c);
}

void con_set_mirror(int on) { mirror = on; }
