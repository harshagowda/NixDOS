/* NixDOS 2 - printf family (%d %i %u %x %X %c %s %p %%, width, '-', '0') */
#include "kernel.h"

int kformat(out_fn out, void *ctx, const char *fmt, va_list ap)
{
    int count = 0;
    char tmp[34];

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            out(*fmt, ctx);
            count++;
            continue;
        }
        fmt++;
        int left = 0, zero = 0, width = 0;
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') zero = 1;
            else break;
        }
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        while (isdigit(*fmt)) width = width * 10 + (*fmt++ - '0');
        while (*fmt == 'l' || *fmt == 'h') fmt++;

        const char *s = tmp;
        int len = 0, neg = 0;
        switch (*fmt) {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'p': {
            u32 v;
            int base = (*fmt == 'x' || *fmt == 'X' || *fmt == 'p') ? 16 : 10;
            const char *digits = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
            if (*fmt == 'd' || *fmt == 'i') {
                int sv = va_arg(ap, int);
                if (sv < 0) { neg = 1; v = (u32)(-(sv + 1)) + 1; } else v = (u32)sv;
            } else {
                v = va_arg(ap, u32);
            }
            if (*fmt == 'p') { width = 8; zero = 1; }
            char *e = tmp + sizeof(tmp);
            char *b = e;
            do { *--b = digits[v % base]; v /= base; } while (v);
            s = b;
            len = (int)(e - b);
            break;
        }
        case 'c':
            tmp[0] = (char)va_arg(ap, int);
            len = 1;
            break;
        case 's':
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            len = (int)strlen(s);
            break;
        case '%':
            tmp[0] = '%';
            len = 1;
            break;
        case 0:
            return count;
        default:
            tmp[0] = '%';
            tmp[1] = *fmt;
            len = 2;
            break;
        }

        int pad = width - len - neg;
        if (!left && !zero) while (pad-- > 0) { out(' ', ctx); count++; }
        if (neg) { out('-', ctx); count++; }
        if (!left && zero) while (pad-- > 0) { out('0', ctx); count++; }
        for (int i = 0; i < len; i++) { out(s[i], ctx); count++; }
        if (left) while (pad-- > 0) { out(' ', ctx); count++; }
    }
    return count;
}

static void out_console(char c, void *ctx)
{
    (void)ctx;
    con_putc(c);
}

int kvprintf(const char *fmt, va_list ap)
{
    return kformat(out_console, NULL, fmt, ap);
}

int kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = kvprintf(fmt, ap);
    va_end(ap);
    return n;
}

struct bufctx { char *buf; size_t n, pos; };

static void out_buf(char c, void *ctx)
{
    struct bufctx *b = ctx;
    if (b->pos + 1 < b->n) b->buf[b->pos] = c;
    b->pos++;
}

int kvsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
    struct bufctx b = { buf, n, 0 };
    int r = kformat(out_buf, &b, fmt, ap);
    if (n) buf[b.pos < n ? b.pos : n - 1] = 0;
    return r;
}

int ksnprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = kvsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}
