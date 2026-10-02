/* NixDOS 2 - command shell */
#include "kernel.h"

#define MAX_ARGS 16
#define HISTORY  16
#define LINE_MAX 256

static char *history[HISTORY];
static int nhistory;

typedef void (*cmd_fn)(int argc, char **argv);
struct command { const char *name; const char *alias; cmd_fn fn; const char *usage; const char *help; };
static const struct command commands[];

static void set_color(int fg) { con_setcolor(fg, 0); }

/* ---- helpers --------------------------------------------------------------- */
static int parse_args(char *line, char **argv)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '"') {
            argv[argc++] = ++p;
            while (*p && *p != '"') p++;
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ') p++;
        }
        if (*p) *p++ = 0;
    }
    return argc;
}

static void add_history(const char *line)
{
    if (!*line) return;
    if (nhistory && strcmp(history[nhistory - 1], line) == 0) return;
    if (nhistory == HISTORY) {
        kfree(history[0]);
        memmove(history, history + 1, sizeof(char *) * (HISTORY - 1));
        nhistory--;
    }
    char *copy = kmalloc((u32)strlen(line) + 1);
    if (!copy) return;
    strcpy(copy, line);
    history[nhistory++] = copy;
}

static void print_time(const struct rtc_time *t)
{
    kprintf("%02d:%02d:%02d", t->hour, t->min, t->sec);
}

static void print_date(const struct rtc_time *t)
{
    static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    kprintf("%02d-%s-%04d", t->day, (t->month >= 1 && t->month <= 12) ? months[t->month - 1] : "???", t->year);
}

static int need_args(int argc, int n, const char *usage)
{
    if (argc < n) {
        kprintf("usage: %s\n", usage);
        return 0;
    }
    return 1;
}

/* ---- commands -------------------------------------------------------------- */
static void cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    set_color(14);
    kprintf("NixDOS commands:\n");
    set_color(7);
    for (int i = 0; commands[i].name; i++) {
        char name[24];
        if (commands[i].alias)
            ksnprintf(name, sizeof(name), "%s/%s", commands[i].name, commands[i].alias);
        else
            ksnprintf(name, sizeof(name), "%s", commands[i].name);
        kprintf("  %-14s %s\n", name, commands[i].help);
    }
    kprintf("Programs: type the name of a .nxe file (with or without .nxe) to run it.\n");
    kprintf("Keys: Up/Down = history, Ctrl+C = stop a running program.\n");
}

static void cmd_clear(int argc, char **argv) { (void)argc; (void)argv; con_clear(); }

static void cmd_ver(int argc, char **argv)
{
    (void)argc; (void)argv;
    kprintf("%s - 32-bit protected mode, built-in NixC compiler\n", NIXDOS_VERSION);
    kprintf("Based on NIXDOS-1.0 (2002), Rural Engineering College, Bhalki\n");
}

static void cmd_time(int argc, char **argv)
{
    (void)argc; (void)argv;
    struct rtc_time t;
    rtc_read(&t);
    kprintf("Current time is ");
    print_time(&t);
    kprintf("\n");
}

static void cmd_date(int argc, char **argv)
{
    (void)argc; (void)argv;
    struct rtc_time t;
    rtc_read(&t);
    kprintf("Today's date is ");
    print_date(&t);
    kprintf("\n");
}

static int parse_triplet(const char *s, char sep, int *a, int *b, int *c)
{
    const char *p = s;
    int v[3];
    for (int i = 0; i < 3; i++) {
        if (!isdigit(*p)) return -1;
        v[i] = 0;
        while (isdigit(*p)) v[i] = v[i] * 10 + (*p++ - '0');
        if (i < 2 && *p++ != sep) return -1;
    }
    if (*p) return -1;
    *a = v[0]; *b = v[1]; *c = v[2];
    return 0;
}

static void cmd_settime(int argc, char **argv)
{
    struct rtc_time t;
    int h, m, s;
    if (!need_args(argc, 2, "settime HH:MM:SS")) return;
    if (parse_triplet(argv[1], ':', &h, &m, &s) || h > 23 || m > 59 || s > 59) {
        kprintf("invalid time '%s'\n", argv[1]);
        return;
    }
    rtc_read(&t);
    t.hour = h; t.min = m; t.sec = s;
    rtc_write(&t);
    kprintf("Time set to ");
    print_time(&t);
    kprintf("\n");
}

static void cmd_setdate(int argc, char **argv)
{
    struct rtc_time t;
    int d, mo, y;
    if (!need_args(argc, 2, "setdate DD-MM-YYYY")) return;
    if (parse_triplet(argv[1], '-', &d, &mo, &y) || d < 1 || d > 31 || mo < 1 || mo > 12 ||
        y < 2000 || y > 2099) {
        kprintf("invalid date '%s' (DD-MM-YYYY, years 2000-2099)\n", argv[1]);
        return;
    }
    rtc_read(&t);
    t.day = d; t.month = mo; t.year = y;
    rtc_write(&t);
    kprintf("Date set to ");
    print_date(&t);
    kprintf("\n");
}

static void cmd_clock(int argc, char **argv)
{
    (void)argc; (void)argv;
    struct rtc_time t;
    int last = -1;
    kprintf("Real time clock - press any key to stop\n");
    while (!kbd_haskey()) {
        rtc_read(&t);
        if (t.sec != last) {
            last = t.sec;
            kprintf("\r  ");
            print_date(&t);
            kprintf("  ");
            print_time(&t);
        }
        hlt();
    }
    kbd_trygetkey();
    kprintf("\n");
}

static void cmd_color(int argc, char **argv)
{
    if (!need_args(argc, 3, "color <fg 0-15> <bg 0-7>")) return;
    int fg = atoi(argv[1]), bg = atoi(argv[2]);
    if (fg < 0 || fg > 15 || bg < 0 || bg > 7) {
        kprintf("colors: 0 black 1 blue 2 green 3 cyan 4 red 5 magenta 6 brown 7 grey,\n"
                "        8-15 bright versions (foreground only)\n");
        return;
    }
    con_setcolor(fg, bg);
    con_clear();
}

static void cpuid(u32 leaf, u32 *a, u32 *b, u32 *c, u32 *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(0));
}

static void cmd_equip(int argc, char **argv)
{
    (void)argc; (void)argv;
    u32 a, b, c, d;
    char vendor[13], brand[49];

    cpuid(0, &a, &b, &c, &d);
    memcpy(vendor, &b, 4); memcpy(vendor + 4, &d, 4); memcpy(vendor + 8, &c, 4);
    vendor[12] = 0;
    cpuid(0x80000000, &a, &b, &c, &d);
    brand[0] = 0;
    if (a >= 0x80000004) {
        u32 *bp = (u32 *)brand;
        for (u32 i = 0; i < 3; i++) cpuid(0x80000002 + i, &bp[i * 4], &bp[i * 4 + 1], &bp[i * 4 + 2], &bp[i * 4 + 3]);
        brand[48] = 0;
    }
    cpuid(1, &a, &b, &c, &d);
    char *bs = brand;
    while (*bs == ' ') bs++;

    u16 equip = *(volatile u16 *)0x410;      /* BIOS equipment word */
    set_color(14);
    kprintf("Equipment list\n");
    set_color(7);
    kprintf("  CPU         : %s %s\n", vendor, bs);
    kprintf("  CPU features: %s%s%s%s%s\n", (d & 1) ? "FPU " : "", (d & (1 << 4)) ? "TSC " : "",
            (d & (1 << 23)) ? "MMX " : "", (d & (1 << 25)) ? "SSE " : "", (d & (1 << 26)) ? "SSE2 " : "");
    kprintf("  Memory      : %d KiB (%d MiB)\n", mem_total_kb(), mem_total_kb() / 1024);
    kprintf("  A20 line    : enabled (memory above 1 MiB in use)\n");
    if (ata_present())
        kprintf("  Hard disk   : %s, %d sectors (%d MiB)\n", ata_model(), ata_sectors(), ata_sectors() / 2048);
    else
        kprintf("  Hard disk   : none\n");
    kprintf("  Floppy      : %d drive(s)\n", (equip & 1) ? ((equip >> 6) & 3) + 1 : 0);
    kprintf("  Serial ports: %d   Parallel ports: %d\n", (equip >> 9) & 7, (equip >> 14) & 3);
    kprintf("  Video       : %s\n", ((equip >> 4) & 3) == 3 ? "monochrome" : "colour (VGA text 80x25)");
    kprintf("  Boot drive  : 0x%x\n", boot_drive);
}

static void cmd_mem(int argc, char **argv)
{
    (void)argc; (void)argv;
    u32 used, free_bytes;
    heap_stats(&kheap, &used, &free_bytes);
    kprintf("Total memory : %d KiB\n", mem_total_kb());
    kprintf("Kernel heap  : %d KiB used, %d KiB free (0x%x-0x%x)\n", used / 1024, free_bytes / 1024, KHEAP_START, KHEAP_END);
    kprintf("Program area : code 0x%x, data 0x%x, stack top 0x%x, heap from 0x%x\n",
            PROG_CODE, PROG_DATA, PROG_STACK_TOP, PROG_HEAP_START);
}

static void cmd_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    u32 s = timer_ticks() / TIMER_HZ;
    kprintf("up %d:%02d:%02d\n", s / 3600, (s / 60) % 60, s % 60);
}

static void cmd_ls(int argc, char **argv)
{
    (void)argc; (void)argv;
    int n = 0;
    u32 total = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        const struct fs_dirent *e = fs_entry(i);
        if (!e) continue;
        u32 t = e->mtime;
        int is_exe = ends_with(e->name, ".nxe");
        int is_c = ends_with(e->name, ".c");
        set_color(is_exe ? 10 : is_c ? 11 : 7);
        kprintf("  %-24s", e->name);
        set_color(7);
        kprintf(" %6d  %02d-%02d-%04d %02d:%02d\n", e->size, (t >> 17) & 31, (t >> 22) & 15,
                2000 + (t >> 26), (t >> 12) & 31, (t >> 6) & 63);
        n++;
        total += e->size;
    }
    kprintf("  %d file(s), %d bytes, %d free slot(s)%s\n", n, total, FS_MAX_FILES - n,
            fs_on_disk() ? "" : "  [RAM only - no disk]");
}

static void cmd_cat(int argc, char **argv)
{
    if (!need_args(argc, 2, "cat <file>")) return;
    for (int i = 1; i < argc; i++) {
        char *s = fs_read_alloc(argv[i], NULL);
        if (!s) { kprintf("%s: no such file\n", argv[i]); continue; }
        con_write(s);
        if (*s && s[strlen(s) - 1] != '\n') con_putc('\n');
        kfree(s);
    }
}

static void cmd_hexdump(int argc, char **argv)
{
    u32 size;
    if (!need_args(argc, 2, "hexdump <file>")) return;
    u8 *s = (u8 *)fs_read_alloc(argv[1], &size);
    if (!s) { kprintf("%s: no such file\n", argv[1]); return; }
    for (u32 off = 0; off < size; off += 16) {
        kprintf("%04x  ", off);
        for (u32 i = 0; i < 16; i++) {
            if (off + i < size) kprintf("%02x ", s[off + i]);
            else kprintf("   ");
        }
        kprintf(" ");
        for (u32 i = 0; i < 16 && off + i < size; i++)
            con_putc((s[off + i] >= 32 && s[off + i] < 127) ? (char)s[off + i] : '.');
        kprintf("\n");
    }
    kfree(s);
}

static void cmd_edit(int argc, char **argv)
{
    if (!need_args(argc, 2, "edit <file>")) return;
    editor_run(argv[1]);
}

static void cmd_rm(int argc, char **argv)
{
    if (!need_args(argc, 2, "rm <file>...")) return;
    for (int i = 1; i < argc; i++)
        if (fs_delete(argv[i])) kprintf("%s: no such file\n", argv[i]);
}

static void cmd_cp(int argc, char **argv)
{
    u32 size;
    if (!need_args(argc, 3, "cp <from> <to>")) return;
    char *s = fs_read_alloc(argv[1], &size);
    if (!s) { kprintf("%s: no such file\n", argv[1]); return; }
    if (fs_write(argv[2], s, size)) kprintf("cp: cannot write %s\n", argv[2]);
    kfree(s);
}

static void cmd_mv(int argc, char **argv)
{
    if (!need_args(argc, 3, "mv <from> <to>")) return;
    if (fs_find(argv[1]) < 0) { kprintf("%s: no such file\n", argv[1]); return; }
    if (fs_rename(argv[1], argv[2])) kprintf("mv: cannot rename to %s\n", argv[2]);
}

/* echo text [> file | >> file] */
static void cmd_echo(int argc, char **argv)
{
    char out[LINE_MAX];
    int redirect = 0, append = 0;
    out[0] = 0;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) && i + 1 < argc) {
            redirect = i + 1;
            append = argv[i][1] == '>';
            break;
        }
        if (out[0]) strcat(out, " ");
        if (strlen(out) + strlen(argv[i]) + 2 < sizeof(out)) strcat(out, argv[i]);
    }
    if (!redirect) {
        kprintf("%s\n", out);
        return;
    }
    strcat(out, "\n");
    const char *name = argv[redirect];
    if (append && fs_find(name) >= 0) {
        u32 size;
        char *old = fs_read_alloc(name, &size);
        u32 n = (u32)strlen(out);
        char *both = old ? kmalloc(size + n) : NULL;
        if (!both) { kfree(old); kprintf("echo: out of memory\n"); return; }
        memcpy(both, old, size);
        memcpy(both + size, out, n);
        if (fs_write(name, both, size + n)) kprintf("echo: cannot write %s\n", name);
        kfree(old);
        kfree(both);
    } else if (fs_write(name, out, (u32)strlen(out))) {
        kprintf("echo: cannot write %s\n", name);
    }
}

static int compile_file(const char *src_name, struct cc_result *res)
{
    char *src = fs_read_alloc(src_name, NULL);
    if (!src) {
        kprintf("cc: %s: no such file\n", src_name);
        return -1;
    }
    int r = cc_compile(src, src_name, res);
    kfree(src);
    return r;
}

/* cc file.c [-o out.nxe] [args...]  - compile, then run (or save) */
static void cmd_cc(int argc, char **argv)
{
    struct cc_result res;
    if (!need_args(argc, 2, "cc <file.c> [-o <out.nxe>] [program args...]")) return;
    const char *out = NULL;
    if (argc >= 4 && strcmp(argv[2], "-o") == 0) out = argv[3];

    u32 t0 = timer_ms();
    if (compile_file(argv[1], &res)) return;
    u32 t1 = timer_ms();

    if (out) {
        u32 total = sizeof(struct nxe_header) + res.code_size + res.data_init;
        if (total > FS_MAX_SIZE) {
            kprintf("cc: executable too large for a file (%d bytes, max %d)\n", total, FS_MAX_SIZE);
            return;
        }
        u8 *img = kmalloc(total);
        if (!img) { kprintf("cc: out of memory\n"); return; }
        struct nxe_header *h = (struct nxe_header *)img;
        h->magic = NXE_MAGIC;
        h->entry = res.entry;
        h->code_size = res.code_size;
        h->data_size = res.data_size;
        memcpy(img + sizeof(*h), (void *)PROG_CODE, res.code_size);
        memcpy(img + sizeof(*h) + res.code_size, (void *)PROG_DATA, res.data_init);
        if (fs_write(out, img, total)) kprintf("cc: cannot write %s\n", out);
        else kprintf("%s: %d bytes code, %d bytes data -> %s (%d ms)\n", argv[1],
                     res.code_size, res.data_size, out, t1 - t0);
        kfree(img);
        return;
    }

    int ret = run_program(res.entry, argc - 1, argv + 1);
    if (ret != 0) kprintf("[exit code %d]\n", ret);
}

static void cmd_run(int argc, char **argv)
{
    if (!need_args(argc, 2, "run <program.nxe> [args...]")) return;
    int ret = run_nxe_file(argv[1], argc - 1, argv + 1);
    if (ret != 0) kprintf("[exit code %d]\n", ret);
}

static void cmd_format(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "yes") != 0) {
        kprintf("This erases ALL files. Type 'format yes' to confirm.\n");
        return;
    }
    if (fs_format()) kprintf("format failed\n");
    else kprintf("File system formatted.\n");
}

static void cmd_beep(int argc, char **argv)
{
    (void)argc; (void)argv;
    speaker_beep(880, 150);
}

static void cmd_reboot(int argc, char **argv) { (void)argc; (void)argv; sys_reboot(); }
static void cmd_halt(int argc, char **argv) { (void)argc; (void)argv; sys_shutdown(); }

static const struct command commands[] = {
    { "help",     NULL,     cmd_help,     "help",               "List of commands" },
    { "cls",      "clr",    cmd_clear,    "cls",                "Clear the screen" },
    { "ver",      "vers",   cmd_ver,      "ver",                "Version of the operating system" },
    { "time",     NULL,     cmd_time,     "time",               "Show the current time (from the RTC)" },
    { "settime",  "ctime",  cmd_settime,  "settime HH:MM:SS",   "Set a new time" },
    { "date",     NULL,     cmd_date,     "date",               "Show the current date" },
    { "setdate",  "cdate",  cmd_setdate,  "setdate DD-MM-YYYY", "Set a new date" },
    { "clock",    NULL,     cmd_clock,    "clock",              "Real time clock" },
    { "color",    "ccolor", cmd_color,    "color fg bg",        "Change text colours" },
    { "equip",    NULL,     cmd_equip,    "equip",              "Equipment / hardware list" },
    { "mem",      NULL,     cmd_mem,      "mem",                "Memory usage" },
    { "uptime",   NULL,     cmd_uptime,   "uptime",             "Time since boot" },
    { "ls",       "dir",    cmd_ls,       "ls",                 "List files" },
    { "cat",      "type",   cmd_cat,      "cat file",           "Print a file" },
    { "hexdump",  NULL,     cmd_hexdump,  "hexdump file",       "Hex dump of a file" },
    { "edit",     "ndedit", cmd_edit,     "edit file",          "Full-screen text editor" },
    { "echo",     "prtmsg", cmd_echo,     "echo text [> file]", "Print a message (or write it to a file)" },
    { "cp",       "copy",   cmd_cp,       "cp from to",         "Copy a file" },
    { "mv",       "ren",    cmd_mv,       "mv from to",         "Rename a file" },
    { "rm",       "del",    cmd_rm,       "rm file",            "Delete a file" },
    { "cc",       NULL,     cmd_cc,       "cc file.c [-o x.nxe]", "Compile C: run it, or save an executable" },
    { "run",      NULL,     cmd_run,      "run x.nxe [args]",   "Run a compiled program" },
    { "format",   NULL,     cmd_format,   "format yes",         "Erase the file system" },
    { "beep",     NULL,     cmd_beep,     "beep",               "Beep the PC speaker" },
    { "reboot",   "rboot",  cmd_reboot,   "reboot",             "Restart the computer" },
    { "shutdown", "sdown",  cmd_halt,     "shutdown",           "Power off" },
    { NULL, NULL, NULL, NULL, NULL }
};

static void execute(char *line)
{
    char *argv[MAX_ARGS + 1];
    int argc = parse_args(line, argv);
    argv[argc] = NULL;
    if (argc == 0) return;

    for (int i = 0; commands[i].name; i++) {
        if (strcmp(argv[0], commands[i].name) == 0 ||
            (commands[i].alias && strcmp(argv[0], commands[i].alias) == 0)) {
            commands[i].fn(argc, argv);
            return;
        }
    }

    /* not a built-in: try an executable file */
    char name[FS_NAME_MAX + 8];
    ksnprintf(name, sizeof(name), "%s", argv[0]);
    if (fs_find(name) < 0 && strlen(argv[0]) + 4 < FS_NAME_MAX)
        ksnprintf(name, sizeof(name), "%s.nxe", argv[0]);
    if (fs_find(name) >= 0 && ends_with(name, ".nxe")) {
        int ret = run_nxe_file(name, argc, argv);
        if (ret != 0) kprintf("[exit code %d]\n", ret);
        return;
    }
    if (fs_find(argv[0]) >= 0 && ends_with(argv[0], ".c")) {
        kprintf("'%s' is C source: compile and run it with 'cc %s'\n", argv[0], argv[0]);
        return;
    }
    set_color(12);
    kprintf("Erroneous command or file name, type help\n");
    set_color(7);
}

void shell_run(void)
{
    char line[LINE_MAX];
    for (;;) {
        set_color(10);
        kprintf("$] ");
        set_color(7);
        int n = readline(line, sizeof(line), history, nhistory);
        if (n <= 0) continue;
        add_history(line);
        execute(line);
    }
}
