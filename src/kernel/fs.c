/* NixDOS 2 - NXFS: a small flat file system
 *
 * Disk layout (512-byte sectors), shared with tools/mkimage.py:
 *   LBA 0            boot sector
 *   LBA 1..1023      kernel
 *   LBA 1024         superblock  ("NXFS", version, max files, slot sectors, data LBA)
 *   LBA 1025..1040   directory: 128 x 64-byte entries (FS_MAX_FILES are used)
 *   LBA 1041..       data: one fixed 64 KiB slot per directory entry
 *
 * With no ATA disk the same API works on a RAM-only directory.
 */
#include "kernel.h"

#define FS_MAGIC        0x5346584E  /* "NXFS" */
#define FS_VERSION      1
#define FS_SUPER_LBA    1024
#define FS_DIR_LBA      1025
#define FS_DIR_SECTORS  16
#define FS_DATA_LBA     1041
#define FS_SLOT_SECTORS 128
#define FS_USED         1

struct superblock {
    u32 magic, version, max_files, slot_sectors, data_lba;
    u8  pad[512 - 20];
};

static struct fs_dirent *dir;               /* FS_DIR_SECTORS * 512 bytes */
static u8 *ram_data[FS_MAX_FILES];
static int on_disk;

static u32 pack_time(void)
{
    struct rtc_time t;
    rtc_read(&t);
    return ((u32)(t.year - 2000) << 26) | ((u32)t.month << 22) | ((u32)t.day << 17) |
           ((u32)t.hour << 12) | ((u32)t.min << 6) | (u32)t.sec;
}

static int sync_dir(void)
{
    if (!on_disk) return 0;
    return ata_write(FS_DIR_LBA, FS_DIR_SECTORS, dir);
}

int fs_format(void)
{
    memset(dir, 0, FS_DIR_SECTORS * 512);
    for (int i = 0; i < FS_MAX_FILES; i++) {
        kfree(ram_data[i]);
        ram_data[i] = NULL;
    }
    if (!on_disk) return 0;
    struct superblock *sb = kzalloc(512);
    sb->magic = FS_MAGIC;
    sb->version = FS_VERSION;
    sb->max_files = FS_MAX_FILES;
    sb->slot_sectors = FS_SLOT_SECTORS;
    sb->data_lba = FS_DATA_LBA;
    int r = ata_write(FS_SUPER_LBA, 1, sb);
    kfree(sb);
    if (r) return -1;
    return sync_dir();
}

void fs_init(void)
{
    dir = kzalloc(FS_DIR_SECTORS * 512);
    on_disk = 0;
    if (ata_present() && ata_sectors() >= FS_DATA_LBA + FS_MAX_FILES * FS_SLOT_SECTORS) {
        struct superblock *sb = kmalloc(512);
        on_disk = 1;
        if (ata_read(FS_SUPER_LBA, 1, sb) == 0 && sb->magic == FS_MAGIC &&
            sb->version == FS_VERSION) {
            if (ata_read(FS_DIR_LBA, FS_DIR_SECTORS, dir))
                memset(dir, 0, FS_DIR_SECTORS * 512);
        } else {
            kprintf("fs: no NXFS found on disk, formatting...\n");
            fs_format();
        }
        kfree(sb);
    }
}

int fs_on_disk(void) { return on_disk; }

int fs_valid_name(const char *name)
{
    size_t n = strlen(name);
    if (n == 0 || n >= FS_NAME_MAX) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (!(isalnum(c) || c == '.' || c == '_' || c == '-')) return 0;
    }
    return 1;
}

int fs_find(const char *name)
{
    for (int i = 0; i < FS_MAX_FILES; i++)
        if ((dir[i].flags & FS_USED) && strcmp(dir[i].name, name) == 0)
            return i;
    return -1;
}

const struct fs_dirent *fs_entry(int i)
{
    if (i < 0 || i >= FS_MAX_FILES || !(dir[i].flags & FS_USED)) return NULL;
    return &dir[i];
}

int fs_count(void)
{
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (dir[i].flags & FS_USED) n++;
    return n;
}

static int read_slot(int i, void *buf, u32 len)
{
    if (!on_disk) {
        if (len) memcpy(buf, ram_data[i], len);
        return 0;
    }
    u32 secs = (len + 511) / 512;
    u32 lba = FS_DATA_LBA + (u32)i * FS_SLOT_SECTORS;
    u8 *tmp = kmalloc(512);
    int r = 0;
    for (u32 s = 0; s < secs && r == 0; s++) {
        u32 chunk = len - s * 512;
        if (chunk > 512) chunk = 512;
        if (chunk == 512) {
            r = ata_read(lba + s, 1, (u8 *)buf + s * 512);
        } else {
            r = ata_read(lba + s, 1, tmp);
            memcpy((u8 *)buf + s * 512, tmp, chunk);
        }
    }
    kfree(tmp);
    return r;
}

int fs_read(const char *name, void *buf, u32 max)
{
    int i = fs_find(name);
    if (i < 0) return -1;
    u32 len = dir[i].size < max ? dir[i].size : max;
    if (read_slot(i, buf, len)) return -1;
    return (int)len;
}

char *fs_read_alloc(const char *name, u32 *size)
{
    int i = fs_find(name);
    if (i < 0) return NULL;
    char *buf = kmalloc(dir[i].size + 1);
    if (!buf) return NULL;
    if (read_slot(i, buf, dir[i].size)) {
        kfree(buf);
        return NULL;
    }
    buf[dir[i].size] = 0;
    if (size) *size = dir[i].size;
    return buf;
}

int fs_write(const char *name, const void *data, u32 len)
{
    if (!fs_valid_name(name) || len > FS_MAX_SIZE) return -1;
    int i = fs_find(name);
    if (i < 0) {
        for (i = 0; i < FS_MAX_FILES; i++)
            if (!(dir[i].flags & FS_USED)) break;
        if (i == FS_MAX_FILES) return -1;
    }

    if (on_disk) {
        u32 lba = FS_DATA_LBA + (u32)i * FS_SLOT_SECTORS;
        u32 secs = (len + 511) / 512;
        u8 *tmp = kmalloc(512);
        for (u32 s = 0; s < secs; s++) {
            u32 chunk = len - s * 512;
            if (chunk > 512) chunk = 512;
            memset(tmp, 0, 512);
            memcpy(tmp, (const u8 *)data + s * 512, chunk);
            if (ata_write(lba + s, 1, tmp)) {
                kfree(tmp);
                return -1;
            }
        }
        kfree(tmp);
    } else {
        u8 *copy = kmalloc(len ? len : 1);
        if (!copy) return -1;
        memcpy(copy, data, len);
        kfree(ram_data[i]);
        ram_data[i] = copy;
    }

    memset(&dir[i], 0, sizeof(dir[i]));
    strcpy(dir[i].name, name);
    dir[i].size = len;
    dir[i].flags = FS_USED;
    dir[i].mtime = pack_time();
    return sync_dir();
}

int fs_delete(const char *name)
{
    int i = fs_find(name);
    if (i < 0) return -1;
    memset(&dir[i], 0, sizeof(dir[i]));
    kfree(ram_data[i]);
    ram_data[i] = NULL;
    return sync_dir();
}

int fs_rename(const char *from, const char *to)
{
    int i = fs_find(from);
    if (i < 0 || !fs_valid_name(to)) return -1;
    if (strcmp(from, to) == 0) return 0;
    if (fs_find(to) >= 0) fs_delete(to);
    memset(dir[i].name, 0, FS_NAME_MAX);
    strcpy(dir[i].name, to);
    return sync_dir();
}
