/* NixDOS 2 - small C library for native programs (built with GCC, -m32)
 * System services come from the kernel API table (see nixdos.h). */
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <assert.h>
#include "nixdos.h"

int errno;

/* ---- kernel calls ---------------------------------------------------------- */
#define K_OPEN(n, f)      NX_CALL(open, int (*)(const char *, int))(n, f)
#define K_CLOSE(fd)       NX_CALL(close, int (*)(int))(fd)
#define K_READ(fd, b, n)  NX_CALL(read, int (*)(int, void *, int))(fd, b, n)
#define K_WRITE(fd, b, n) NX_CALL(write, int (*)(int, const void *, int))(fd, b, n)
#define K_LSEEK(fd, o, w) NX_CALL(lseek, int (*)(int, int, int))(fd, o, w)
#define K_UNLINK(n)       NX_CALL(unlink, int (*)(const char *))(n)
#define K_EXIT(c)         NX_CALL(exit, void (*)(int))(c)
#define K_CONWRITE(s, n)  NX_CALL(conwrite, void (*)(const char *, int))(s, n)
#define K_TIME()          NX_CALL(time, int (*)(void))()
#define K_GETCHAR()       NX_CALL(getchar, int (*)(void))()

/* ---- memory ------------------------------------------------------------------ */
void *memset(void *d, int c, size_t n)
{
    unsigned char *p = d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    void *r = d;
    __asm__ volatile("cld; rep movsl; movl %3, %%ecx; rep movsb"
                     : "+D"(d), "+S"(s), "=c"(n)
                     : "r"(n & 3), "2"(n >> 2)
                     : "memory");
    return r;
}

void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *dp = d;
    const unsigned char *sp = s;
    if (dp == sp || !n) return d;
    if (dp < sp || dp >= sp + n) return memcpy(d, s, n);
    dp += n;
    sp += n;
    while (n--) *--dp = *--sp;
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y) return *x - *y;
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n; n--, p++)
        if (*p == (unsigned char)c) return (void *)p;
    return NULL;
}

/* First-fit allocator over the program heap the kernel hands out. */
struct mblock {
    size_t size;            /* payload size */
    struct mblock *next;
    unsigned magic;
    unsigned free;
};
#define MB_MAGIC 0xA110C8ED
#define MB_HDR sizeof(struct mblock)

static struct mblock *heap_head;
static struct mblock *heap_rover;

static void heap_setup(void)
{
    unsigned start, end;
    NX_CALL(heapinfo, void (*)(unsigned *, unsigned *))(&start, &end);
    start = (start + 15) & ~15u;
    heap_head = (struct mblock *)start;
    heap_head->size = end - start - MB_HDR;
    heap_head->next = NULL;
    heap_head->magic = MB_MAGIC;
    heap_head->free = 1;
    heap_rover = heap_head;
}

static void heap_merge(struct mblock *b)
{
    while (b->free && b->next && b->next->free) {
        b->size += MB_HDR + b->next->size;
        b->next = b->next->next;
    }
}

void *malloc(size_t n)
{
    if (!heap_head) heap_setup();
    if (n == 0) n = 1;
    if (n > 0x7FFFFFF0) return NULL;
    n = (n + 15) & ~(size_t)15;
    struct mblock *start = heap_rover, *b = start;
    do {
        if (b->free) {
            heap_merge(b);
            if (b->size >= n) {
                if (b->size >= n + MB_HDR + 32) {
                    struct mblock *rest = (struct mblock *)((char *)b + MB_HDR + n);
                    rest->size = b->size - n - MB_HDR;
                    rest->next = b->next;
                    rest->magic = MB_MAGIC;
                    rest->free = 1;
                    b->next = rest;
                    b->size = n;
                }
                b->free = 0;
                heap_rover = b->next ? b->next : heap_head;
                return (char *)b + MB_HDR;
            }
        }
        b = b->next ? b->next : heap_head;
    } while (b != start);
    errno = ENOMEM;
    return NULL;
}

void free(void *p)
{
    if (!p) return;
    struct mblock *b = (struct mblock *)((char *)p - MB_HDR);
    if (b->magic != MB_MAGIC || b->free) return;
    b->free = 1;
    heap_merge(b);
}

void *calloc(size_t n, size_t size)
{
    if (size && n > 0x7FFFFFFF / size) return NULL;
    void *p = malloc(n * size);
    if (p) memset(p, 0, n * size);
    return p;
}

void *realloc(void *p, size_t n)
{
    if (!p) return malloc(n);
    if (!n) { free(p); return NULL; }
    struct mblock *b = (struct mblock *)((char *)p - MB_HDR);
    if (b->size >= n) return p;
    if (b->next && b->next->free) {
        heap_merge(b->next);
        if (b->size + MB_HDR + b->next->size >= n) {
            b->size += MB_HDR + b->next->size;
            b->next = b->next->next;
            return p;
        }
    }
    void *q = malloc(n);
    if (!q) return NULL;
    memcpy(q, p, b->size);
    free(p);
    return q;
}

/* ---- strings ------------------------------------------------------------------- */
size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }
size_t strnlen(const char *s, size_t n) { size_t i = 0; while (i < n && s[i]) i++; return i; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }

char *strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }

char *strncat(char *d, const char *s, size_t n)
{
    char *p = d + strlen(d);
    while (n-- && *s) *p++ = *s++;
    *p = 0;
    return d;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (!*a) return 0;
    }
    return 0;
}

int strcasecmp(const char *a, const char *b)
{
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b);
        if (x != y) return x - y;
        if (!x) return 0;
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return NULL;
    }
}

char *strrchr(const char *s, int c)
{
    const char *r = NULL;
    for (;; s++) {
        if (*s == (char)c) r = s;
        if (!*s) return (char *)r;
    }
}

char *strstr(const char *h, const char *n)
{
    size_t len = strlen(n);
    if (!len) return (char *)h;
    for (; *h; h++)
        if (*h == *n && strncmp(h, n, len) == 0) return (char *)h;
    return NULL;
}

char *strdup(const char *s)
{
    char *d = malloc(strlen(s) + 1);
    return d ? strcpy(d, s) : NULL;
}

size_t strspn(const char *s, const char *a) { size_t n = 0; while (s[n] && strchr(a, s[n])) n++; return n; }
size_t strcspn(const char *s, const char *r) { size_t n = 0; while (s[n] && !strchr(r, s[n])) n++; return n; }

char *strtok(char *s, const char *delim)
{
    static char *save;
    if (!s) s = save;
    if (!s) return NULL;
    s += strspn(s, delim);
    if (!*s) { save = NULL; return NULL; }
    char *e = s + strcspn(s, delim);
    if (*e) { *e = 0; save = e + 1; } else save = NULL;
    return s;
}

char *strerror(int e)
{
    switch (e) {
    case 0: return "Success";
    case ENOENT: return "No such file";
    case ENOMEM: return "Out of memory";
    case EBADF: return "Bad file handle";
    case ENOSPC: return "Disk full";
    default: return "Error";
    }
}

int isdigit(int c) { return c >= '0' && c <= '9'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isalpha(int c) { return isupper(c) || islower(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isprint(int c) { return c >= 32 && c < 127; }
int isgraph(int c) { return c > 32 && c < 127; }
int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int toupper(int c) { return islower(c) ? c - 32 : c; }
int tolower(int c) { return isupper(c) ? c + 32 : c; }

/* ---- conversions ------------------------------------------------------------- */
unsigned long strtoul(const char *s, char **end, int base)
{
    const char *p = s;
    unsigned long v = 0;
    int neg = 0, any = 0;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '-' || *p == '+') neg = *p++ == '-';
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { p += 2; base = 16; }
    else if (base == 0 && p[0] == '0') base = 8;
    else if (base == 0) base = 10;
    for (;; p++) {
        int d;
        if (isdigit((unsigned char)*p)) d = *p - '0';
        else if (isalpha((unsigned char)*p)) d = tolower((unsigned char)*p) - 'a' + 10;
        else break;
        if (d >= base) break;
        v = v * (unsigned long)base + (unsigned long)d;
        any = 1;
    }
    if (end) *end = (char *)(any ? p : s);
    return neg ? -v : v;
}

long strtol(const char *s, char **end, int base) { return (long)strtoul(s, end, base); }
int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
long atol(const char *s) { return strtol(s, NULL, 10); }

double strtod(const char *s, char **end)
{
    const char *p = s;
    double v = 0, scale = 1;
    int neg = 0;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '-' || *p == '+') neg = *p++ == '-';
    while (isdigit((unsigned char)*p)) v = v * 10 + (*p++ - '0');
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) { scale /= 10; v += (*p++ - '0') * scale; }
    }
    if (end) *end = (char *)p;
    return neg ? -v : v;
}

double atof(const char *s) { return strtod(s, NULL); }
int abs(int v) { return v < 0 ? -v : v; }
long labs(long v) { return v < 0 ? -v : v; }

static unsigned long rand_next = 1;
int rand(void) { rand_next = rand_next * 1103515245 + 12345; return (int)((rand_next >> 16) & RAND_MAX); }
void srand(unsigned s) { rand_next = s; }

static void swap_bytes(char *a, char *b, size_t n)
{
    while (n--) { char t = *a; *a++ = *b; *b++ = t; }
}

void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    char *b = base;
    /* simple, recursion-free insertion sort for small inputs, quicksort otherwise */
    if (n < 2) return;
    if (n <= 8) {
        for (size_t i = 1; i < n; i++)
            for (size_t j = i; j > 0 && cmp(b + (j - 1) * size, b + j * size) > 0; j--)
                swap_bytes(b + (j - 1) * size, b + j * size, size);
        return;
    }
    swap_bytes(b, b + (n / 2) * size, size);
    size_t last = 0;
    for (size_t i = 1; i < n; i++)
        if (cmp(b + i * size, b) < 0) swap_bytes(b + ++last * size, b + i * size, size);
    swap_bytes(b, b + last * size, size);
    qsort(b, last, size, cmp);
    qsort(b + (last + 1) * size, n - last - 1, size, cmp);
}

char *getenv(const char *name) { (void)name; return NULL; }
int system(const char *cmd) { (void)cmd; return -1; }

/* ---- exit ---------------------------------------------------------------------- */
static void (*atexit_fns[32])(void);
static int natexit;

int atexit(void (*fn)(void))
{
    if (natexit >= 32) return -1;
    atexit_fns[natexit++] = fn;
    return 0;
}

void fflush_all(void);

void _Exit(int code)
{
    K_EXIT(code);
    for (;;) ;
}

void exit(int code)
{
    while (natexit > 0) atexit_fns[--natexit]();
    fflush_all();
    _Exit(code);
}

void abort(void)
{
    static const char msg[] = "abort() called\n";
    K_CONWRITE(msg, sizeof(msg) - 1);
    _Exit(134);
}

void __assert_fail(const char *expr, const char *file, int line)
{
    printf("Assertion failed: %s (%s:%d)\n", expr, file, line);
    abort();
}

/* ---- POSIX-style file I/O ------------------------------------------------------ */
/* fds 0-2 are the console; kernel file handles are offset by 3. */
#define FD_BASE 3

int open(const char *name, int flags, ...)
{
    int h = K_OPEN(name, flags);
    if (h < 0) { errno = ENOENT; return -1; }
    return h + FD_BASE;
}

int close(int fd)
{
    if (fd < FD_BASE) return 0;
    return K_CLOSE(fd - FD_BASE);
}

ssize_t read(int fd, void *buf, size_t n)
{
    if (fd < FD_BASE) return 0;
    int r = K_READ(fd - FD_BASE, buf, (int)n);
    if (r < 0) errno = EBADF;
    return r;
}

ssize_t write(int fd, const void *buf, size_t n)
{
    if (fd == 1 || fd == 2) { K_CONWRITE(buf, (int)n); return (ssize_t)n; }
    if (fd < FD_BASE) return -1;
    int r = K_WRITE(fd - FD_BASE, buf, (int)n);
    if (r < 0) errno = ENOSPC;
    return r;
}

off_t lseek(int fd, off_t off, int whence)
{
    if (fd < FD_BASE) return -1;
    return K_LSEEK(fd - FD_BASE, (int)off, whence);
}

int unlink(const char *name) { return K_UNLINK(name) < 0 ? (errno = ENOENT, -1) : 0; }
int access(const char *name, int mode) { (void)mode; return nx_filesize(name) < 0 ? (errno = ENOENT, -1) : 0; }
char *getcwd(char *buf, size_t n) { if (n < 2) return NULL; strcpy(buf, "/"); return buf; }
int chdir(const char *p) { (void)p; return 0; }
unsigned sleep(unsigned s) { nx_sleep((int)s * 1000); return 0; }
int usleep(unsigned us) { nx_sleep((int)(us / 1000)); return 0; }
int mkdir(const char *p, mode_t m) { (void)p; (void)m; return 0; }

int stat(const char *name, struct stat *st)
{
    if (!strcmp(name, ".") || !strcmp(name, "/") || !strcmp(name, "")) {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFDIR;
        return 0;
    }
    int size = nx_filesize(name);
    if (size < 0) { errno = ENOENT; return -1; }
    memset(st, 0, sizeof(*st));
    st->st_size = size;
    st->st_mode = S_IFREG | 0644;
    return 0;
}

int fstat(int fd, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    if (fd < FD_BASE) return 0;
    int cur = (int)lseek(fd, 0, SEEK_CUR);
    st->st_size = lseek(fd, 0, SEEK_END);
    lseek(fd, cur, SEEK_SET);
    st->st_mode = S_IFREG;
    return 0;
}

time_t time(time_t *t)
{
    time_t v = K_TIME();
    if (t) *t = v;
    return v;
}

clock_t clock(void) { return nx_ticks(); }

/* ---- printf ------------------------------------------------------------------- */
typedef void (*putfn)(char c, void *ctx);

static int fmt_core(putfn out, void *ctx, const char *fmt, va_list ap)
{
    int count = 0;
    char tmp[64];
#define OUT(c) do { out((c), ctx); count++; } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { OUT(*fmt); continue; }
        fmt++;
        int left = 0, zero = 0, plus = 0, space = 0, alt = 0, width = 0, prec = -1, lng = 0;
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') zero = 1;
            else if (*fmt == '+') plus = 1;
            else if (*fmt == ' ') space = 1;
            else if (*fmt == '#') alt = 1;
            else break;
        }
        if (*fmt == '*') { width = va_arg(ap, int); if (width < 0) { left = 1; width = -width; } fmt++; }
        else while (isdigit((unsigned char)*fmt)) width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else while (isdigit((unsigned char)*fmt)) prec = prec * 10 + (*fmt++ - '0');
        }
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z' || *fmt == 't' || *fmt == 'j') {
            if (*fmt == 'l') lng++;
            fmt++;
        }

        const char *s = tmp;
        int len = 0;
        char sign = 0;
        const char *prefix = "";
        switch (*fmt) {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'p': {
            unsigned long long v;
            int base = 10;
            if (*fmt == 'x' || *fmt == 'X' || *fmt == 'p') base = 16;
            else if (*fmt == 'o') base = 8;
            if (*fmt == 'd' || *fmt == 'i') {
                long long sv = lng >= 2 ? va_arg(ap, long long) : (long long)va_arg(ap, long);
                if (sv < 0) { sign = '-'; v = (unsigned long long)(-(sv + 1)) + 1; }
                else { v = (unsigned long long)sv; sign = plus ? '+' : space ? ' ' : 0; }
            } else if (*fmt == 'p') {
                v = (unsigned long)va_arg(ap, void *);
                prefix = "0x";
            } else {
                v = lng >= 2 ? va_arg(ap, unsigned long long) : (unsigned long long)va_arg(ap, unsigned long);
                if (alt && base == 16 && v) prefix = *fmt == 'X' ? "0X" : "0x";
            }
            const char *dig = *fmt == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char *e = tmp + sizeof(tmp), *b = e;
            if (v == 0 && prec != 0) *--b = '0';
            while (v) { *--b = dig[v % (unsigned)base]; v /= (unsigned)base; }
            while (prec > 0 && (e - b) < prec) *--b = '0';
            s = b;
            len = (int)(e - b);
            if (prec >= 0) zero = 0;
            break;
        }
        case 'f': case 'F': case 'g': case 'G': case 'e': {
            double d = va_arg(ap, double);
            if (prec < 0) prec = 6;
            if (prec > 9) prec = 9;
            if (d < 0) { sign = '-'; d = -d; } else sign = plus ? '+' : space ? ' ' : 0;
            double scale = 1;
            for (int i = 0; i < prec; i++) scale *= 10;
            unsigned long long whole = (unsigned long long)d;
            unsigned long long frac = (unsigned long long)((d - (double)whole) * scale + 0.5);
            if ((double)frac >= scale) { whole++; frac = 0; }
            char *e = tmp + sizeof(tmp), *b = e;
            for (int i = 0; i < prec; i++) { *--b = (char)('0' + frac % 10); frac /= 10; }
            if (prec) *--b = '.';
            do { *--b = (char)('0' + whole % 10); whole /= 10; } while (whole);
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
            len = prec >= 0 ? (int)strnlen(s, (size_t)prec) : (int)strlen(s);
            break;
        case 'n':
            *va_arg(ap, int *) = count;
            continue;
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
        }

        int plen = (int)strlen(prefix) + (sign ? 1 : 0);
        int pad = width - len - plen;
        if (!left && !zero) while (pad-- > 0) OUT(' ');
        if (sign) OUT(sign);
        for (const char *p = prefix; *p; p++) OUT(*p);
        if (!left && zero) while (pad-- > 0) OUT('0');
        for (int i = 0; i < len; i++) OUT(s[i]);
        if (left) while (pad-- > 0) OUT(' ');
    }
    return count;
#undef OUT
}

struct bufout { char *buf; size_t n, pos; };

static void put_buf(char c, void *ctx)
{
    struct bufout *b = ctx;
    if (b->pos + 1 < b->n) b->buf[b->pos] = c;
    b->pos++;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
    struct bufout b = { buf, n, 0 };
    int r = fmt_core(put_buf, &b, fmt, ap);
    if (n) buf[b.pos < n ? b.pos : n - 1] = 0;
    return r;
}

int vsprintf(char *buf, const char *fmt, va_list ap) { return vsnprintf(buf, 0x7FFFFFFF, fmt, ap); }

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, 0x7FFFFFFF, fmt, ap);
    va_end(ap);
    return r;
}

/* ---- stdio streams --------------------------------------------------------------- */
struct _nix_file {
    int fd;
    int eof, err;
    int writing;
    char wbuf[512];
    int wlen;
};

static struct _nix_file std_files[3] = { { 0, 0, 0, 0, {0}, 0 }, { 1, 0, 0, 1, {0}, 0 }, { 2, 0, 0, 1, {0}, 0 } };
FILE *stdin = &std_files[0], *stdout = &std_files[1], *stderr = &std_files[2];
#define MAX_FILES 16
static FILE *open_files[MAX_FILES];

int fflush(FILE *f)
{
    if (!f) { fflush_all(); return 0; }
    if (f->wlen) {
        if (write(f->fd, f->wbuf, (size_t)f->wlen) != f->wlen) f->err = 1;
        f->wlen = 0;
    }
    return f->err ? EOF : 0;
}

void fflush_all(void)
{
    fflush(stdout);
    fflush(stderr);
    for (int i = 0; i < MAX_FILES; i++) if (open_files[i]) fflush(open_files[i]);
}

int fputc(int c, FILE *f)
{
    f->wbuf[f->wlen++] = (char)c;
    if (f->wlen == (int)sizeof(f->wbuf) || ((f == stdout || f == stderr) && c == '\n'))
        fflush(f);
    return (unsigned char)c;
}

int putc(int c, FILE *f) { return fputc(c, f); }
int putchar(int c) { return fputc(c, stdout); }

int fputs(const char *s, FILE *f)
{
    while (*s) fputc(*s++, f);
    return 0;
}

int puts(const char *s)
{
    fputs(s, stdout);
    fputc('\n', stdout);
    return 0;
}

static void put_file(char c, void *ctx) { fputc(c, (FILE *)ctx); }

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    int r = fmt_core(put_file, f, fmt, ap);
    if (f == stdout || f == stderr) fflush(f);
    return r;
}

int vprintf(const char *fmt, va_list ap) { return vfprintf(stdout, fmt, ap); }

int printf(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return r;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

void perror(const char *s)
{
    if (s && *s) fprintf(stderr, "%s: %s\n", s, strerror(errno));
    else fprintf(stderr, "%s\n", strerror(errno));
}

FILE *fopen(const char *name, const char *mode)
{
    int flags;
    if (mode[0] == 'r') flags = strchr(mode, '+') ? O_RDWR : O_RDONLY;
    else if (mode[0] == 'w') flags = (strchr(mode, '+') ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
    else if (mode[0] == 'a') flags = (strchr(mode, '+') ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
    else { errno = EINVAL; return NULL; }
    int slot;
    for (slot = 0; slot < MAX_FILES && open_files[slot]; slot++) ;
    if (slot == MAX_FILES) return NULL;
    int fd = open(name, flags);
    if (fd < 0) return NULL;
    FILE *f = calloc(1, sizeof(FILE));
    if (!f) { close(fd); return NULL; }
    f->fd = fd;
    open_files[slot] = f;
    return f;
}

int fclose(FILE *f)
{
    if (!f || f == stdin || f == stdout || f == stderr) return 0;
    fflush(f);
    int r = close(f->fd);
    for (int i = 0; i < MAX_FILES; i++) if (open_files[i] == f) open_files[i] = NULL;
    free(f);
    return r;
}

size_t fread(void *buf, size_t size, size_t n, FILE *f)
{
    if (!size || !n) return 0;
    fflush(f);
    if (f == stdin) {
        char *p = buf;
        size_t total = size * n, i;
        for (i = 0; i < total; i++) {
            int c = K_GETCHAR();
            p[i] = (char)c;
            if (c == '\n') { i++; break; }
        }
        return i / size;
    }
    ssize_t r = read(f->fd, buf, size * n);
    if (r < 0) { f->err = 1; return 0; }
    if ((size_t)r < size * n) f->eof = 1;
    return (size_t)r / size;
}

size_t fwrite(const void *buf, size_t size, size_t n, FILE *f)
{
    const char *p = buf;
    size_t total = size * n;
    if (f == stdout || f == stderr) {
        for (size_t i = 0; i < total; i++) fputc(p[i], f);
        return n;
    }
    fflush(f);
    ssize_t r = write(f->fd, buf, total);
    if (r < 0) { f->err = 1; return 0; }
    return size ? (size_t)r / size : 0;
}

int fgetc(FILE *f)
{
    unsigned char c;
    return fread(&c, 1, 1, f) == 1 ? c : EOF;
}

int getc(FILE *f) { return fgetc(f); }
int getchar(void) { return fgetc(stdin); }

char *fgets(char *buf, int n, FILE *f)
{
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF) break;
        buf[i++] = (char)c;
        if (c == '\n') break;
    }
    if (i == 0) return NULL;
    buf[i] = 0;
    return buf;
}

int fseek(FILE *f, long off, int whence)
{
    fflush(f);
    f->eof = 0;
    return lseek(f->fd, off, whence) < 0 ? -1 : 0;
}

long ftell(FILE *f) { fflush(f); return lseek(f->fd, 0, SEEK_CUR); }
void rewind(FILE *f) { fseek(f, 0, SEEK_SET); }
int feof(FILE *f) { return f->eof; }
int ferror(FILE *f) { return f->err; }
int fileno(FILE *f) { return f->fd; }
int remove(const char *name) { return unlink(name); }

int rename(const char *from, const char *to)
{
    FILE *a = fopen(from, "rb");
    if (!a) return -1;
    fclose(a);
    /* no rename call in the API yet: copy + delete */
    int in = open(from, O_RDONLY), out = open(to, O_WRONLY | O_CREAT | O_TRUNC);
    char buf[512];
    ssize_t n;
    while ((n = read(in, buf, sizeof(buf))) > 0) write(out, buf, (size_t)n);
    close(in);
    close(out);
    return unlink(from);
}

int sscanf(const char *s, const char *fmt, ...)
{
    va_list ap;
    int n = 0;
    va_start(ap, fmt);
    while (*fmt) {
        if (isspace((unsigned char)*fmt)) { while (isspace((unsigned char)*s)) s++; fmt++; continue; }
        if (*fmt != '%') { if (*s != *fmt) break; s++; fmt++; continue; }
        fmt++;
        while (isspace((unsigned char)*s)) s++;
        char *end;
        if (*fmt == 'd' || *fmt == 'i') {
            long v = strtol(s, &end, *fmt == 'd' ? 10 : 0);
            if (end == s) break;
            *va_arg(ap, int *) = (int)v; s = end; n++;
        } else if (*fmt == 'x') {
            unsigned long v = strtoul(s, &end, 16);
            if (end == s) break;
            *va_arg(ap, unsigned *) = (unsigned)v; s = end; n++;
        } else if (*fmt == 's') {
            char *d = va_arg(ap, char *);
            if (!*s) break;
            while (*s && !isspace((unsigned char)*s)) *d++ = *s++;
            *d = 0; n++;
        } else break;
        fmt++;
    }
    va_end(ap);
    return n;
}
