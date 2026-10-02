/* NixDOS 2 - kernel entry point */
#include "kernel.h"

u32 boot_drive;

void panic(const char *fmt, ...)
{
    va_list ap;
    cli();
    con_setcolor(15, 4);
    kprintf("\n*** KERNEL PANIC: ");
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf(" ***\nSystem halted.");
    for (;;) hlt();
}

void sys_reboot(void)
{
    kprintf("Rebooting...\n");
    cli();
    for (int i = 0; i < 100000 && (inb(0x64) & 0x02); i++) ;
    outb(0x64, 0xFE);                       /* pulse the CPU reset line */
    /* fall back to a triple fault */
    struct { u16 limit; u32 base; } __attribute__((packed)) null_idt = { 0, 0 };
    __asm__ volatile("lidt %0; int $3" : : "m"(null_idt));
    for (;;) hlt();
}

void sys_shutdown(void)
{
    kprintf("Wait, NixDOS is shutting down...\n");
    kprintf("NOW IT IS SAFE TO TURN OFF YOUR COMPUTER\n");
    outw(0x604, 0x2000);                    /* QEMU (q35/piix ACPI) */
    outw(0xB004, 0x2000);                   /* Bochs, old QEMU */
    outw(0x4004, 0x3400);                   /* VirtualBox */
    outb(0xF4, 0x00);                       /* QEMU isa-debug-exit, if present */
    cli();
    for (;;) hlt();
}

static void fpu_init(void)
{
    u32 cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1u << 2);              /* EM: no emulation */
    cr0 |= (1u << 1) | (1u << 5);   /* MP, NE: native x87 error reporting */
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
    __asm__ volatile("fninit");
}

static void banner(void)
{
    con_setcolor(15, 1);
    kprintf("                                                                                ");
    kprintf("   ***************  WELCOME TO NIXDOS 2.0  ***************                      ");
    kprintf("   A mini operating system - now 32-bit, with its own C compiler                ");
    kprintf("                                                                                ");
    con_setcolor(7, 0);
}

void kmain(u32 drive)
{
    boot_drive = drive;

    con_init();
    fpu_init();
    idt_init();
    mem_init();
    serial_init();
    con_clear();
    banner();
    timer_init();
    kbd_init();
    sti();

    kprintf("[ ok ] protected mode, A20 enabled, interrupts on\n");
    kprintf("[ ok ] memory: %d MiB, kernel heap at 0x%x\n", mem_total_kb() / 1024, KHEAP_START);
    kprintf("[ ok ] serial console: %s\n", serial_present() ? "COM1" : "not present");

    if (ata_init())
        kprintf("[ ok ] disk: %s (%d MiB)\n", ata_model(), ata_sectors() / 2048);
    else
        kprintf("[warn] no ATA disk found, files are kept in RAM only\n");
    if (sb_init())
        kprintf("[ ok ] sound: Sound Blaster 16 (DSP %d.%02d)\n", sb_version() >> 8, sb_version() & 0xFF);
    fs_init();
    kprintf("[ ok ] file system: %d file(s)%s\n", fs_count(), fs_on_disk() ? "" : " (RAM)");
    prog_init();

    kprintf("\nType 'help' for commands. Try:  ls   cat hello.c   cc hello.c\n\n");
    shell_run();
}
