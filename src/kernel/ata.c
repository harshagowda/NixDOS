/* NixDOS 2 - ATA PIO driver (primary bus, master drive, 28-bit LBA) */
#include "kernel.h"

#define ATA_DATA    0x1F0
#define ATA_ERROR   0x1F1
#define ATA_COUNT   0x1F2
#define ATA_LBA0    0x1F3
#define ATA_LBA1    0x1F4
#define ATA_LBA2    0x1F5
#define ATA_DRIVE   0x1F6
#define ATA_CMD     0x1F7
#define ATA_STATUS  0x1F7
#define ATA_ALTSTAT 0x3F6

#define ST_ERR  0x01
#define ST_DRQ  0x08
#define ST_DF   0x20
#define ST_BSY  0x80

static int present;
static u32 sectors;
static char model[41];

static void delay400(void)
{
    for (int i = 0; i < 4; i++) inb(ATA_ALTSTAT);
}

static int wait_not_busy(void)
{
    for (u32 i = 0; i < 1000000; i++)
        if (!(inb(ATA_STATUS) & ST_BSY)) return 0;
    return -1;
}

static int wait_drq(void)
{
    for (u32 i = 0; i < 1000000; i++) {
        u8 s = inb(ATA_STATUS);
        if (s & (ST_ERR | ST_DF)) return -1;
        if (!(s & ST_BSY) && (s & ST_DRQ)) return 0;
    }
    return -1;
}

int ata_init(void)
{
    u16 id[256];

    present = 0;
    if (inb(ATA_STATUS) == 0xFF) return 0;      /* floating bus: no drive */

    outb(ATA_DRIVE, 0xA0);
    delay400();
    outb(ATA_COUNT, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_CMD, 0xEC);                        /* IDENTIFY */
    if (inb(ATA_STATUS) == 0) return 0;
    if (wait_not_busy()) return 0;
    if (inb(ATA_LBA1) || inb(ATA_LBA2)) return 0;   /* ATAPI/SATA, not ATA */
    if (wait_drq()) return 0;
    for (int i = 0; i < 256; i++) id[i] = inw(ATA_DATA);

    sectors = id[60] | ((u32)id[61] << 16);
    for (int i = 0; i < 20; i++) {
        model[i * 2] = (char)(id[27 + i] >> 8);
        model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }
    model[40] = 0;
    for (int i = 39; i >= 0 && model[i] == ' '; i--) model[i] = 0;
    present = sectors > 0;
    return present;
}

int ata_present(void) { return present; }
u32 ata_sectors(void) { return sectors; }
const char *ata_model(void) { return model; }

static int setup(u32 lba, u8 cmd)
{
    if (wait_not_busy()) return -1;
    outb(ATA_DRIVE, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    delay400();
    outb(ATA_COUNT, 1);
    outb(ATA_LBA0, (u8)lba);
    outb(ATA_LBA1, (u8)(lba >> 8));
    outb(ATA_LBA2, (u8)(lba >> 16));
    outb(ATA_CMD, cmd);
    delay400();
    return 0;
}

int ata_read(u32 lba, u32 count, void *buf)
{
    u16 *p = buf;
    if (!present) return -1;
    for (u32 s = 0; s < count; s++) {
        if (setup(lba + s, 0x20) || wait_drq()) return -1;
        for (int i = 0; i < 256; i++) *p++ = inw(ATA_DATA);
    }
    return 0;
}

int ata_write(u32 lba, u32 count, const void *buf)
{
    const u16 *p = buf;
    if (!present) return -1;
    for (u32 s = 0; s < count; s++) {
        if (setup(lba + s, 0x30) || wait_drq()) return -1;
        for (int i = 0; i < 256; i++) outw(ATA_DATA, *p++);
        if (wait_not_busy()) return -1;
    }
    outb(ATA_CMD, 0xE7);                        /* cache flush */
    wait_not_busy();
    return 0;
}
