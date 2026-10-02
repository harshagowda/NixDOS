/* NixDOS 2 - NixEdit, a small full-screen text editor
 *
 *   arrows/Home/End/PgUp/PgDn  move        Ctrl+S  save
 *   Backspace/Del              delete      Ctrl+Q  quit (Esc also quits)
 *   Tab                        4 spaces    Ctrl+K  delete current line
 */
#include "kernel.h"

#define ROWS (CON_H - 1)
#define ATTR_TEXT   0x1F    /* white on blue */
#define ATTR_STATUS 0x70    /* black on grey */

static char *buf;
static int len, cap, cur, top, left, want_col, modified;
static char fname[FS_NAME_MAX];
static char msg[80];

static int line_start(int p) { while (p > 0 && buf[p - 1] != '\n') p--; return p; }
static int line_end(int p) { while (p < len && buf[p] != '\n') p++; return p; }

static int line_of(int p)
{
    int n = 0;
    for (int i = 0; i < p; i++) if (buf[i] == '\n') n++;
    return n;
}

static int pos_of_line(int line)
{
    int p = 0;
    while (line > 0 && p < len) { if (buf[p++] == '\n') line--; }
    return p;
}

static int total_lines(void) { return line_of(len) + 1; }

static void insert(const char *s, int n)
{
    if (len + n > cap - 1) { ksnprintf(msg, sizeof(msg), "File too large (max %d bytes)", cap - 1); return; }
    memmove(buf + cur + n, buf + cur, (size_t)(len - cur));
    memcpy(buf + cur, s, (size_t)n);
    len += n;
    cur += n;
    modified = 1;
}

static void delete_at(int p, int n)
{
    if (p < 0 || p + n > len || n <= 0) return;
    memmove(buf + p, buf + p + n, (size_t)(len - p - n));
    len -= n;
    modified = 1;
}

static void render(void)
{
    int cline = line_of(cur);
    int ccol = cur - line_start(cur);

    if (cline < top) top = cline;
    if (cline >= top + ROWS) top = cline - ROWS + 1;
    if (ccol < left) left = ccol;
    if (ccol >= left + CON_W) left = ccol - CON_W + 1;

    int p = pos_of_line(top);
    for (int row = 0; row < ROWS; row++) {
        int x = 0;
        if (p <= len && (p < len || row == 0 || buf[p - 1] == '\n')) {
            int e = line_end(p);
            for (int i = p + left; i < e && x < CON_W; i++)
                con_putat(x++, row, buf[i], ATTR_TEXT);
            while (x < CON_W) con_putat(x++, row, ' ', ATTR_TEXT);
            p = e < len ? e + 1 : len + 1;
        } else {
            con_putat(0, row, '~', 0x17);
            for (x = 1; x < CON_W; x++) con_putat(x, row, ' ', ATTR_TEXT);
        }
    }

    char status[CON_W + 1];
    if (msg[0])
        ksnprintf(status, sizeof(status), " %s", msg);
    else
        ksnprintf(status, sizeof(status), " NixEdit: %s%s   Ln %d/%d, Col %d   ^S Save  ^Q Quit  ^K Del line",
                  fname, modified ? " *" : "", cline + 1, total_lines(), ccol + 1);
    int i = 0;
    for (; status[i] && i < CON_W; i++) con_putat(i, CON_H - 1, status[i], ATTR_STATUS);
    for (; i < CON_W; i++) con_putat(i, CON_H - 1, ' ', ATTR_STATUS);

    con_gotoxy(ccol - left, cline - top);
}

static void move_vert(int delta)
{
    int ls = line_start(cur);
    if (delta < 0) {
        for (int i = 0; i < -delta && ls > 0; i++) ls = line_start(ls - 1);
    } else {
        for (int i = 0; i < delta; i++) {
            int e = line_end(ls);
            if (e >= len) break;
            ls = e + 1;
        }
    }
    int e = line_end(ls);
    cur = ls + want_col < e ? ls + want_col : e;
}

static int save(void)
{
    if (fs_write(fname, buf, (u32)len) == 0) {
        modified = 0;
        ksnprintf(msg, sizeof(msg), "Saved %s (%d bytes)", fname, len);
        return 0;
    }
    ksnprintf(msg, sizeof(msg), "Error: could not save %s", fname);
    return -1;
}

void editor_run(const char *filename)
{
    if (!fs_valid_name(filename)) {
        kprintf("edit: invalid file name '%s'\n", filename);
        return;
    }
    cap = FS_MAX_SIZE + 1;
    buf = kmalloc((u32)cap);
    if (!buf) { kprintf("edit: out of memory\n"); return; }
    strcpy(fname, filename);
    len = cur = top = left = want_col = modified = 0;
    msg[0] = 0;

    u32 size;
    char *src = fs_read_alloc(fname, &size);
    if (src) {
        for (u32 i = 0; i < size && len < cap - 4; i++) {
            if (src[i] == '\r') continue;
            if (src[i] == '\t') { for (int k = 0; k < 4; k++) buf[len++] = ' '; continue; }
            buf[len++] = src[i];
        }
        kfree(src);
    } else {
        ksnprintf(msg, sizeof(msg), "New file: %s", fname);
    }

    con_set_mirror(0);
    u8 saved_attr = con_getattr();

    for (;;) {
        render();
        int k = kbd_getkey();
        msg[0] = 0;
        int track_col = 1;

        switch (k) {
        case 19:                    /* Ctrl+S */
            save();
            break;
        case 17:                    /* Ctrl+Q */
        case 27:                    /* Esc */
            if (modified) {
                ksnprintf(msg, sizeof(msg), "Unsaved changes! Save before quitting? (y/n, other key cancels)");
                render();
                int a = kbd_getkey();
                msg[0] = 0;
                if (a == 'y' || a == 'Y') { if (save()) break; }
                else if (a != 'n' && a != 'N') break;
            }
            goto done;
        case 11: {                  /* Ctrl+K */
            int s = line_start(cur), e = line_end(cur);
            delete_at(s, (e < len ? e + 1 : e) - s);
            cur = s > len ? len : s;
            break;
        }
        case KEY_LEFT:  if (cur > 0) cur--; break;
        case KEY_RIGHT: if (cur < len) cur++; break;
        case KEY_UP:    move_vert(-1); track_col = 0; break;
        case KEY_DOWN:  move_vert(1); track_col = 0; break;
        case KEY_PGUP:  move_vert(-(ROWS - 1)); track_col = 0; break;
        case KEY_PGDN:  move_vert(ROWS - 1); track_col = 0; break;
        case KEY_HOME:  cur = line_start(cur); break;
        case KEY_END:   cur = line_end(cur); break;
        case KEY_DEL:   delete_at(cur, 1); break;
        case '\b':
            if (cur > 0) { cur--; delete_at(cur, 1); }
            break;
        case '\t':
            insert("    ", 4);
            break;
        case '\n': {
            /* keep the indentation of the current line, indent after '{' */
            char ind[64];
            int s = line_start(cur), n = 0, b = cur;
            while (s + n < cur && buf[s + n] == ' ' && n < 59) n++;
            while (b > s && buf[b - 1] == ' ') b--;
            if (b > s && buf[b - 1] == '{') n += 4;
            ind[0] = '\n';
            for (int i = 0; i < n; i++) ind[i + 1] = ' ';
            insert(ind, n + 1);
            break;
        }
        default:
            if (k >= 32 && k < 127) {
                char c = (char)k;
                /* '}' typed on a blank indented line: outdent one level */
                int s = line_start(cur), blank = 1;
                for (int i = s; i < cur; i++) if (buf[i] != ' ') blank = 0;
                if (c == '}' && blank && cur - s >= 4) {
                    cur -= 4;
                    delete_at(cur, 4);
                }
                insert(&c, 1);
            }
            break;
        }
        if (track_col) want_col = cur - line_start(cur);
    }

done:
    kfree(buf);
    buf = NULL;
    con_setattr(saved_attr);
    con_set_mirror(1);
    con_clear();
}
