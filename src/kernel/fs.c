/* NixDOS 2 - NXFS v2: a small flat file system with contiguous extents
 *
 * Disk layout (512-byte sectors), shared with tools/mkimage.py:
 *   LBA 0            boot sector
 *   LBA 1..1023      kernel
 *   LBA 1024         superblock  ("NXFS", version 2, max files, data LBA, total sectors)
 *   LBA 1025..1040   directory: 128 x 64-byte entries
 *   LBA 1041..       data area; every file is one contiguous run of sectors
 *
 * A file that grows beyond its run is moved to a free gap that is big enough
 * (first fit). With no ATA disk the same API works on files kept in RAM.
 */
#include "kernel.h"

#define FS_MAGIC        0x5346584E  /* "NXFS" */
#define FS_VERSION      2
#define FS_SUPER_LBA    1024
#define FS_DIR_LBA      1025
#define FS_DIR_SECTORS  16
#define FS_DATA_LBA     1041
#define FS_USED         1

struct superblock {
    u32 magic, version, max_files, data_lba, total_sectors;
    u8  pad[512 - 20];
};

static struct fs_dirent *dir;               /* FS_DIR_SECTORS * 512 bytes */
static u8 *ram_data[FS_MAX_FILES];
static int on_disk;
static u32 data_sectors;                    /* size of the data area */

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
    sb->data_lba = FS_DATA_LBA;
    sb->total_sectors = ata_sectors();
    int r = ata_write(FS_SUPER_LBA, 1, sb);
    kfree(sb);
    if (r) return -1;
    return sync_dir();
}

void fs_init(void)
{
    dir = kzalloc(FS_DIR_SECTORS * 512);
    on_disk = 0;
    if (ata_present() && ata_sectors() > FS_DATA_LBA + 64) {
        struct superblock *sb = kmalloc(512);
        on_disk = 1;
        data_sectors = ata_sectors() - FS_DATA_LBA;
        if (ata_read(FS_SUPER_LBA, 1, sb) == 0 && sb->magic == FS_MAGIC &&
            sb->version == FS_VERSION) {
            if (ata_read(FS_DIR_LBA, FS_DIR_SECTORS, dir))
                memset(dir, 0, FS_DIR_SECTORS * 512);
        } else {
            kprintf("fs: no NXFS v%d found on disk, formatting...\n", FS_VERSION);
            fs_format();
        }
        kfree(sb);
    }
}

int fs_on_disk(void) { return on_disk; }

u32 fs_free_bytes(void)
{
    if (!on_disk) return 0;
    u32 used = 0;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (dir[i].flags & FS_USED) used += dir[i].alloc;
    return (data_sectors - used) * 512;
}

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

/* Read len bytes at byte offset off of directory entry i. */
int fs_read_at(int i, u32 off, void *buf, u32 len)
{
    if (i < 0 || i >= FS_MAX_FILES || !(dir[i].flags & FS_USED)) return -1;
    if (off >= dir[i].size) return 0;
    if (len > dir[i].size - off) len = dir[i].size - off;
    if (len == 0) return 0;
    if (!on_disk) {
        memcpy(buf, ram_data[i] + off, len);
        return (int)len;
    }

    u8 *out = buf;
    u32 lba = FS_DATA_LBA + dir[i].start + off / 512;
    u32 skip = off % 512, left = len;
    u8 *tmp = NULL;

    if (skip) {                                     /* partial first sector */
        tmp = kmalloc(512);
        if (!tmp || ata_read(lba, 1, tmp)) goto fail;
        u32 n = 512 - skip < left ? 512 - skip : left;
        memcpy(out, tmp + skip, n);
        out += n; left -= n; lba++;
    }
    if (left >= 512) {                              /* whole sectors, straight in */
        u32 secs = left / 512;
        if (ata_read(lba, secs, out)) goto fail;
        out += secs * 512; left -= secs * 512; lba += secs;
    }
    if (left) {                                     /* partial last sector */
        if (!tmp) tmp = kmalloc(512);
        if (!tmp || ata_read(lba, 1, tmp)) goto fail;
        memcpy(out, tmp, left);
    }
    kfree(tmp);
    return (int)len;
fail:
    kfree(tmp);
    return -1;
}

int fs_read(const char *name, void *buf, u32 max)
{
    return fs_read_at(fs_find(name), 0, buf, max);
}

char *fs_read_alloc(const char *name, u32 *size)
{
    int i = fs_find(name);
    if (i < 0) return NULL;
    char *buf = kmalloc(dir[i].size + 1);
    if (!buf) return NULL;
    if (fs_read_at(i, 0, buf, dir[i].size) != (int)dir[i].size) {
        kfree(buf);
        return NULL;
    }
    buf[dir[i].size] = 0;
    if (size) *size = dir[i].size;
    return buf;
}

/* First-fit search for `need` free sectors, ignoring entry `self`. */
static int find_gap(u32 need, int self, u32 *start)
{
    u32 pos = 0;
    for (;;) {
        u32 next_end = 0;
        int clash = 0;
        for (int i = 0; i < FS_MAX_FILES; i++) {
            if (i == self || !(dir[i].flags & FS_USED) || dir[i].alloc == 0) continue;
            u32 s = dir[i].start, e = s + dir[i].alloc;
            if (s < pos + need && e > pos) {        /* overlaps [pos, pos+need) */
                clash = 1;
                if (e > next_end) next_end = e;
            }
        }
        if (!clash) {
            if (pos + need > data_sectors) return -1;
            *start = pos;
            return 0;
        }
        pos = next_end;
    }
}

int fs_write(const char *name, const void *data, u32 len)
{
    if (!fs_valid_name(name)) return -1;
    int i = fs_find(name);
    int is_new = i < 0;
    if (is_new) {
        for (i = 0; i < FS_MAX_FILES; i++)
            if (!(dir[i].flags & FS_USED)) break;
        if (i == FS_MAX_FILES) return -1;
    }

    if (on_disk) {
        u32 need = (len + 511) / 512;
        u32 start = is_new ? 0 : dir[i].start;
        u32 alloc = is_new ? 0 : dir[i].alloc;
        if (need > alloc) {
            if (find_gap(need, is_new ? -1 : i, &start)) return -1;   /* disk full */
            alloc = need;
        }
        u32 whole = len / 512;
        if (whole && ata_write(FS_DATA_LBA + start, whole, data)) return -1;
        if (len % 512) {
            u8 *tmp = kzalloc(512);
            if (!tmp) return -1;
            memcpy(tmp, (const u8 *)data + whole * 512, len % 512);
            int r = ata_write(FS_DATA_LBA + start + whole, 1, tmp);
            kfree(tmp);
            if (r) return -1;
        }
        memset(&dir[i], 0, sizeof(dir[i]));
        dir[i].start = start;
        dir[i].alloc = alloc;
    } else {
        u8 *copy = kmalloc(len ? len : 1);
        if (!copy) return -1;
        memcpy(copy, data, len);
        kfree(ram_data[i]);
        ram_data[i] = copy;
        memset(&dir[i], 0, sizeof(dir[i]));
    }

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

/* ---- file handles for programs (open/read/write/lseek/close) --------------
 * Reads go straight to the disk. Files opened for writing are kept in a
 * memory buffer and written back on close.
 */
#define MAX_HANDLES 16
#define O_ACCMODE 3
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   0x40
#define O_TRUNC   0x200
#define O_APPEND  0x400

struct handle {
    int used, writable, dirty;
    char name[FS_NAME_MAX];
    u32 pos, size, cap;
    u8 *buf;            /* write buffer (writable handles only) */
};

static struct handle handles[MAX_HANDLES];

int fs_open(const char *name, int flags)
{
    int h;
    if (!fs_valid_name(name)) return -1;
    for (h = 0; h < MAX_HANDLES && handles[h].used; h++) ;
    if (h == MAX_HANDLES) return -1;
    struct handle *f = &handles[h];
    int idx = fs_find(name);
    int acc = flags & O_ACCMODE;

    memset(f, 0, sizeof(*f));
    strcpy(f->name, name);
    if (acc == 0) {
        if (idx < 0) return -1;
        f->size = dir[idx].size;
    } else {
        if (idx < 0 && !(flags & O_CREAT)) return -1;
        f->writable = 1;
        f->dirty = idx < 0 || (flags & O_TRUNC);
        if (idx >= 0 && !(flags & O_TRUNC)) {
            f->size = dir[idx].size;
            f->cap = f->size + 4096;
            f->buf = kmalloc(f->cap);
            if (!f->buf || fs_read_at(idx, 0, f->buf, f->size) != (int)f->size) {
                kfree(f->buf);
                return -1;
            }
        }
        if (flags & O_APPEND) f->pos = f->size;
    }
    f->used = 1;
    return h;
}

static struct handle *get_handle(int h)
{
    if (h < 0 || h >= MAX_HANDLES || !handles[h].used) return NULL;
    return &handles[h];
}

int fs_hread(int h, void *buf, u32 len)
{
    struct handle *f = get_handle(h);
    if (!f) return -1;
    if (f->pos >= f->size) return 0;
    if (len > f->size - f->pos) len = f->size - f->pos;
    int n;
    if (f->writable) {
        memcpy(buf, f->buf + f->pos, len);
        n = (int)len;
    } else {
        n = fs_read_at(fs_find(f->name), f->pos, buf, len);
        if (n < 0) return -1;
    }
    f->pos += (u32)n;
    return n;
}

int fs_hwrite(int h, const void *buf, u32 len)
{
    struct handle *f = get_handle(h);
    if (!f || !f->writable) return -1;
    u32 end = f->pos + len;
    if (end > f->cap) {
        u32 cap = end + end / 2 + 4096;
        u8 *nb = kmalloc(cap);
        if (!nb) return -1;
        if (f->buf) memcpy(nb, f->buf, f->size);
        kfree(f->buf);
        f->buf = nb;
        f->cap = cap;
    }
    if (f->pos > f->size) memset(f->buf + f->size, 0, f->pos - f->size);
    memcpy(f->buf + f->pos, buf, len);
    f->pos = end;
    if (end > f->size) f->size = end;
    f->dirty = 1;
    return (int)len;
}

int fs_hseek(int h, int off, int whence)
{
    struct handle *f = get_handle(h);
    if (!f) return -1;
    int base = whence == 0 ? 0 : whence == 1 ? (int)f->pos : (int)f->size;
    if (base + off < 0) return -1;
    f->pos = (u32)(base + off);
    return (int)f->pos;
}

int fs_hclose(int h)
{
    struct handle *f = get_handle(h);
    if (!f) return -1;
    int r = 0;
    if (f->writable && f->dirty)
        r = fs_write(f->name, f->buf ? f->buf : (const u8 *)"", f->size);
    kfree(f->buf);
    memset(f, 0, sizeof(*f));
    return r;
}

void fs_close_all(void)
{
    for (int h = 0; h < MAX_HANDLES; h++)
        if (handles[h].used) fs_hclose(h);
}
