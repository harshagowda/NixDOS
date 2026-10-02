/* NixDOS 2 - user program runtime: API for compiled programs, loader, abort/crash handling */
#include "kernel.h"
#include "api.h"

volatile int prog_running;
volatile int abort_requested;

static heap_t prog_heap;
static k_jmp_buf prog_jb;
static volatile int prog_exit_code;
static u32 rand_state = 1;

static void prog_leave(int code) __attribute__((noreturn));
static void prog_leave(int code)
{
    prog_exit_code = code;
    k_longjmp(prog_jb, 1);
}

/* ---- API functions (cdecl, called from generated code) --------------------- */
static int api_putchar(int c) { con_putc((char)c); return c & 0xFF; }

static int api_getchar(void)
{
    for (;;) {
        int k = kbd_getkey();
        if (k == '\n') { con_putc('\n'); return '\n'; }
        if (k == '\b') return '\b';
        if (k >= 0 && k < 256) {
            if (k >= 32) con_putc((char)k);
            return k;
        }
    }
}

static int api_puts(const char *s) { con_write(s); con_putc('\n'); return 1; }

static int api_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = kvprintf(fmt, ap);
    va_end(ap);
    return n;
}

static int api_sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = kvsnprintf(buf, 0x7FFFFFFF, fmt, ap);
    va_end(ap);
    return n;
}

static char *api_gets(char *buf, int max)
{
    if (max <= 0) return buf;
    if (readline(buf, max, NULL, 0) < 0) prog_leave(-2);   /* Ctrl+C */
    return buf;
}

static int api_getkey(void) { return kbd_getkey(); }
static int api_kbhit(void) { prog_check_ctrl_c(); return kbd_haskey(); }
static int api_strlen(const char *s) { return (int)strlen(s); }
static int api_strcmp(const char *a, const char *b) { return strcmp(a, b); }
static int api_strncmp(const char *a, const char *b, int n) { return strncmp(a, b, (size_t)n); }
static char *api_strcpy(char *d, const char *s) { return strcpy(d, s); }
static char *api_strcat(char *d, const char *s) { return strcat(d, s); }
static char *api_strchr(const char *s, int c) { return strchr(s, c); }
static void *api_memset(void *d, int c, int n) { return memset(d, c, (size_t)n); }
static void *api_memcpy(void *d, const void *s, int n) { return memmove(d, s, (size_t)n); }
static int api_atoi(const char *s) { return atoi(s); }
static int api_abs(int v) { return v < 0 ? -v : v; }
static int api_isdigit(int c) { return isdigit(c); }
static int api_isalpha(int c) { return isalpha(c); }
static int api_isspace(int c) { return isspace(c); }
static int api_toupper(int c) { return toupper(c); }
static int api_tolower(int c) { return tolower(c); }
static void *api_malloc(int n) { return n < 0 ? NULL : heap_alloc(&prog_heap, (u32)n); }
static void api_free(void *p) { heap_free(&prog_heap, p); }

static int api_rand(void)
{
    rand_state = rand_state * 1103515245u + 12345u;
    return (int)((rand_state >> 16) & 0x7FFF);
}

static void api_srand(int s) { rand_state = (u32)s; }
static int api_time(void) { return (int)rtc_unix_time(); }
static int api_ticks(void) { return (int)timer_ms(); }
static void api_sleep(int ms) { if (ms > 0) sleep_ms((u32)ms); }
static void api_exit(int code) { prog_leave(code); }
static void api_cls(void) { con_clear(); }
static void api_gotoxy(int x, int y) { con_gotoxy(x, y); }
static void api_setcolor(int fg, int bg) { con_setcolor(fg, bg); }
static void api_putat(int x, int y, int c, int color) { con_putat(x, y, (char)c, (u8)color); }
static int api_wherex(void) { return con_getx(); }
static int api_wherey(void) { return con_gety(); }
static void api_beep(int freq, int ms) { if (ms > 0) speaker_beep((u32)freq, (u32)ms); }
static int api_readfile(const char *name, char *buf, int max) { return max < 0 ? -1 : fs_read(name, buf, (u32)max); }
static int api_writefile(const char *name, const char *buf, int len) { return len < 0 ? -1 : fs_write(name, buf, (u32)len); }

void *api_table[] = {
#define API(name, fn, nargs, ret) (void *)fn,
    API_LIST
#undef API
};

/* ---- running programs ------------------------------------------------------ */
void prog_init(void)
{
    *(void ***)API_PTR_ADDR = api_table;
}

static void prog_abort_entry(void) __attribute__((noreturn));
static void prog_abort_entry(void)
{
    con_write("^C\n");
    prog_leave(-2);
}

static void prog_crash_entry(void) __attribute__((noreturn));
static void prog_crash_entry(void)
{
    prog_leave(-3);
}

/* Called while a program waits in the kernel (keyboard, sleep, ...) */
void prog_check_ctrl_c(void)
{
    if (prog_running && abort_requested) {
        abort_requested = 0;
        prog_abort_entry();
    }
}

/* Called from IRQ handlers: if the program is busy in its own code, redirect
 * the interrupted instruction pointer so that IRET lands in the abort path. */
void check_abort(struct regs *r)
{
    if (prog_running && abort_requested &&
        r->eip >= PROG_CODE && r->eip < PROG_CODE + PROG_CODE_MAX) {
        abort_requested = 0;
        r->eip = (u32)prog_abort_entry;
    }
}

void prog_fault(struct regs *r, const char *what)
{
    con_setcolor(12, 0);
    kprintf("\n*** program crashed: %s at %p", what, r->eip);
    if (r->eip >= PROG_CODE && r->eip < PROG_CODE + PROG_CODE_MAX)
        kprintf(" (code offset %x)", r->eip - PROG_CODE);
    kprintf(" ***\n");
    con_setcolor(7, 0);
    r->eip = (u32)prog_crash_entry;
}

int run_program(u32 entry, int argc, char **argv)
{
    volatile int ret;
    u32 heap_end = mem_total_kb() * 1024;
    if (heap_end > PROG_HEAP_LIMIT) heap_end = PROG_HEAP_LIMIT;
    if (heap_end < PROG_HEAP_START + 64 * 1024) {
        kprintf("not enough memory to run programs (need > 8 MiB)\n");
        return -1;
    }
    heap_init(&prog_heap, PROG_HEAP_START, heap_end - PROG_HEAP_START);
    prog_init();

    abort_requested = 0;
    prog_running = 1;
    if (k_setjmp(prog_jb) == 0)
        ret = call_on_stack(PROG_CODE + entry, PROG_STACK_TOP, argc, argv);
    else
        ret = prog_exit_code;
    prog_running = 0;
    abort_requested = 0;
    con_setcolor(7, 0);
    return ret;
}

int run_nxe_file(const char *name, int argc, char **argv)
{
    u32 size;
    char *img = fs_read_alloc(name, &size);
    if (!img) {
        kprintf("%s: cannot read file\n", name);
        return -1;
    }
    struct nxe_header *h = (struct nxe_header *)img;
    if (size < sizeof(*h) || h->magic != NXE_MAGIC ||
        h->code_size > PROG_CODE_MAX || h->data_size > PROG_DATA_MAX ||
        sizeof(*h) + h->code_size > size || h->entry >= h->code_size) {
        kprintf("%s: not a NixDOS executable\n", name);
        kfree(img);
        return -1;
    }
    u32 init = size - sizeof(*h) - h->code_size;
    if (init > h->data_size) init = h->data_size;
    memcpy((void *)PROG_CODE, img + sizeof(*h), h->code_size);
    memset((void *)PROG_DATA, 0, h->data_size);
    memcpy((void *)PROG_DATA, img + sizeof(*h) + h->code_size, init);
    u32 entry = h->entry;
    kfree(img);
    return run_program(entry, argc, argv);
}
