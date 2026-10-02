#!/usr/bin/env python3
"""Build a bootable NixDOS 2 hard-disk image.

usage: mkimage.py boot.bin kernel.bin out.img [--size MiB] DIR_OR_FILE...

Every file found in the given directories (or given directly) is installed
into the NXFS v2 file system, with its name in lower case (so DOS-style
names such as VSWAP.WL1 work as the programs expect). Layout (512-byte sectors), see src/kernel/fs.c:
  LBA 0            boot sector
  LBA 1..1023      kernel
  LBA 1024         NXFS superblock
  LBA 1025..1040   directory (128 x 64-byte entries)
  LBA 1041..       data area, one contiguous extent per file
"""
import os
import struct
import sys
import time

SECTOR = 512
KERNEL_LBA = 1
KERNEL_MAX_SECTORS = 1023
SUPER_LBA = 1024
DIR_LBA = 1025
DIR_SECTORS = 16
DATA_LBA = 1041
MAX_FILES = 128
NAME_MAX = 40
FS_MAGIC = 0x5346584E          # "NXFS"
FS_VERSION = 2


def pack_time(t):
    return (((t.tm_year - 2000) & 63) << 26 | t.tm_mon << 22 | t.tm_mday << 17 |
            t.tm_hour << 12 | t.tm_min << 6 | min(t.tm_sec, 59))


def collect(paths):
    files = {}
    for p in paths:
        if os.path.isdir(p):
            for name in sorted(os.listdir(p)):
                full = os.path.join(p, name)
                if os.path.isfile(full) and not name.startswith('.'):
                    files[name.lower()] = full
        elif os.path.isfile(p):
            files[os.path.basename(p).lower()] = p
    return files


def main():
    args = sys.argv[1:]
    size_mib = 32
    if '--size' in args:
        i = args.index('--size')
        size_mib = int(args[i + 1])
        del args[i:i + 2]
    if len(args) < 3:
        sys.exit(__doc__)
    boot_path, kernel_path, out_path = args[:3]
    image_sectors = size_mib * 2048

    boot = open(boot_path, 'rb').read()
    kernel = open(kernel_path, 'rb').read()
    if len(boot) != SECTOR or boot[510:512] != b'\x55\xaa':
        sys.exit('mkimage: boot sector must be 512 bytes ending in 55 AA')
    ksectors = (len(kernel) + SECTOR - 1) // SECTOR
    if ksectors > KERNEL_MAX_SECTORS or len(kernel) > 0x70000:
        sys.exit('mkimage: kernel too large (%d bytes)' % len(kernel))

    img = bytearray(image_sectors * SECTOR)
    img[0:SECTOR] = boot
    img[KERNEL_LBA * SECTOR:KERNEL_LBA * SECTOR + len(kernel)] = kernel
    sb = struct.pack('<5I', FS_MAGIC, FS_VERSION, MAX_FILES, DATA_LBA, image_sectors)
    img[SUPER_LBA * SECTOR:SUPER_LBA * SECTOR + len(sb)] = sb

    files = collect(args[3:])
    if len(files) > MAX_FILES:
        sys.exit('mkimage: too many files (%d, max %d)' % (len(files), MAX_FILES))
    now = pack_time(time.localtime())
    pos = 0                                       # next free sector in the data area
    for i, name in enumerate(sorted(files)):
        data = open(files[name], 'rb').read()
        if name.endswith(('.c', '.txt')):
            data = data.replace(b'\r\n', b'\n')
        if len(name) >= NAME_MAX:
            sys.exit('mkimage: file name too long: ' + name)
        alloc = (len(data) + SECTOR - 1) // SECTOR
        if DATA_LBA + pos + alloc > image_sectors:
            sys.exit('mkimage: image too small, use --size')
        entry = struct.pack('<40s6I', name.encode(), len(data), 1, now, pos, alloc, 0)
        off = DIR_LBA * SECTOR + i * 64
        img[off:off + 64] = entry
        doff = (DATA_LBA + pos) * SECTOR
        img[doff:doff + len(data)] = data
        pos += alloc

    with open(out_path, 'wb') as f:
        f.write(img)
    print('%s: %d MiB, kernel %d bytes, %d file(s), %d KiB used'
          % (out_path, size_mib, len(kernel), len(files), pos // 2))


if __name__ == '__main__':
    main()
