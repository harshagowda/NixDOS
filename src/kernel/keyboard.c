/* NixDOS 2 - PS/2 keyboard driver (scancode set 1) and line input */
#include "kernel.h"

#define KBUF 256

static volatile int kbuf[KBUF];
static volatile u32 khead, ktail;
static int shift, ctrl, alt, caps, extended;

static const char map_normal[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ',
};

static const char map_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ',
};

void kbd_push(int key)
{
    if (key == 3 && prog_running)       /* Ctrl+C stops the running program */
        abort_requested = 1;
    u32 next = (khead + 1) % KBUF;
    if (next == ktail) return;          /* buffer full: drop */
    kbuf[khead] = key;
    khead = next;
}

static void kbd_irq(struct regs *r)
{
    u8 sc = inb(0x60);

    if (sc == 0xE0) { extended = 1; return; }

    int release = sc & 0x80;
    sc &= 0x7F;

    if (extended) {
        extended = 0;
        if (sc == 0x1D) { ctrl = !release; return; }
        if (sc == 0x38) { alt = !release; return; }
        if (release) return;
        switch (sc) {
        case 0x48: kbd_push(KEY_UP); break;
        case 0x50: kbd_push(KEY_DOWN); break;
        case 0x4B: kbd_push(KEY_LEFT); break;
        case 0x4D: kbd_push(KEY_RIGHT); break;
        case 0x47: kbd_push(KEY_HOME); break;
        case 0x4F: kbd_push(KEY_END); break;
        case 0x49: kbd_push(KEY_PGUP); break;
        case 0x51: kbd_push(KEY_PGDN); break;
        case 0x53: kbd_push(KEY_DEL); break;
        case 0x1C: kbd_push('\n'); break;   /* keypad enter */
        case 0x35: kbd_push('/'); break;
        }
        check_abort(r);
        return;
    }

    switch (sc) {
    case 0x2A: case 0x36: shift = !release; return;
    case 0x1D: ctrl = !release; return;
    case 0x38: alt = !release; return;
    case 0x3A: if (!release) caps = !caps; return;
    }
    if (release) return;
    if (sc == 0x3B) { kbd_push(KEY_F1); return; }

    char c = shift ? map_shift[sc] : map_normal[sc];
    if (!c) return;
    if (caps && isalpha(c)) c = (char)(shift ? tolower(c) : toupper(c));
    if (ctrl && isalpha(c)) c = (char)(tolower(c) - 'a' + 1);
    kbd_push(c);
    check_abort(r);
}

void kbd_init(void)
{
    while (inb(0x64) & 0x01) inb(0x60);     /* flush stale bytes */
    irq_install(1, kbd_irq);
    irq_unmask(1);
}

int kbd_haskey(void) { return khead != ktail; }

int kbd_trygetkey(void)
{
    if (khead == ktail) return -1;
    int k = kbuf[ktail];
    ktail = (ktail + 1) % KBUF;
    return k;
}

int kbd_getkey(void)
{
    for (;;) {
        prog_check_ctrl_c();
        cli();
        if (khead != ktail) {
            sti();
            return kbd_trygetkey();
        }
        /* sti; hlt is atomic: no wakeup can be lost between them */
        __asm__ volatile("sti; hlt");
    }
}

/* Line editor with history (up/down) and cursor movement (left/right). */
int readline(char *buf, int max, char **history, int nhistory)
{
    int len = 0, pos = 0, hidx = nhistory;
    buf[0] = 0;

    for (;;) {
        int k = kbd_getkey();
        if (k == '\n') {
            for (; pos < len; pos++) con_putc(buf[pos]);
            con_putc('\n');
            buf[len] = 0;
            return len;
        }
        if (k == 3) {               /* Ctrl+C: discard the line */
            con_write("^C\n");
            buf[0] = 0;
            return -1;
        }
        if ((k == KEY_UP || k == KEY_DOWN) && history) {
            int n = hidx + (k == KEY_UP ? -1 : 1);
            if (n < 0 || n > nhistory) continue;
            hidx = n;
            /* erase the current line on screen */
            for (; pos < len; pos++) con_putc(buf[pos]);
            while (len > 0) { con_write("\b \b"); len--; }
            if (hidx < nhistory) {
                strncpy(buf, history[hidx], max - 1);
                buf[max - 1] = 0;
            } else {
                buf[0] = 0;
            }
            len = pos = (int)strlen(buf);
            con_write(buf);
            continue;
        }
        if (k == KEY_LEFT) {
            if (pos > 0) { pos--; con_putc('\b'); }
            continue;
        }
        if (k == KEY_RIGHT) {
            if (pos < len) con_putc(buf[pos++]);
            continue;
        }
        if (k == '\b') {
            if (pos == 0) continue;
            memmove(buf + pos - 1, buf + pos, len - pos);
            len--;
            pos--;
            con_putc('\b');
            for (int i = pos; i < len; i++) con_putc(buf[i]);
            con_putc(' ');
            con_putc('\b');
            for (int i = pos; i < len; i++) con_putc('\b');
            continue;
        }
        if (k == '\t') k = ' ';
        if (k < 32 || k > 126 || len >= max - 1) continue;
        memmove(buf + pos + 1, buf + pos, len - pos);
        buf[pos] = (char)k;
        len++;
        for (int i = pos; i < len; i++) con_putc(buf[i]);
        pos++;
        for (int i = pos; i < len; i++) con_putc('\b');
    }
}
