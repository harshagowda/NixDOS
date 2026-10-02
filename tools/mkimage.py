#!/usr/bin/env python3
"""Build a bootable NixDOS 2 hard-disk image.

usage: mkimage.py boot.bin kernel.bin programs_dir out.img

Layout (512-byte sectors) - must match src/kernel/fs.c:
  LBA 0            boot sector
  LBA 1..1023      kernel
  LBA 1024         NXFS superblock
  LBA 1025..1040   directory (64-byte entries)
  LBA 1041..       data, one 64 KiB slot per directory entry
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
SLOT_SECTORS = 128
MAX_FILES = 96
NAME_MAX = 40
IMAGE_SECTORS = 16384          # 8 MiB
FS_MAGIC = 0x5346584E          # "NXFS"
FS_VERSION = 1


def pack_time(t):
    return (((t.tm_year - 2000) & 63) << 26 | t.tm_mon << 22 | t.tm_mday << 17 |
            t.tm_hour << 12 | t.tm_min << 6 | min(t.tm_sec, 59))


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    boot_path, kernel_path, prog_dir, out_path = sys.argv[1:]

    boot = open(boot_path, 'rb').read()
    kernel = open(kernel_path, 'rb').read()
    if len(boot) != SECTOR or boot[510:512] != b'\x55\xaa':
        sys.exit('mkimage: boot sector must be 512 bytes ending in 55 AA')
    ksectors = (len(kernel) + SECTOR - 1) // SECTOR
    if ksectors > KERNEL_MAX_SECTORS:
        sys.exit('mkimage: kernel too large (%d sectors)' % ksectors)
    if len(kernel) > 0x70000:
        sys.exit('mkimage: kernel does not fit below 0x80000 once loaded')

    img = bytearray(IMAGE_SECTORS * SECTOR)
    img[0:SECTOR] = boot
    img[KERNEL_LBA * SECTOR:KERNEL_LBA * SECTOR + len(kernel)] = kernel

    sb = struct.pack('<5I', FS_MAGIC, FS_VERSION, MAX_FILES, SLOT_SECTORS, DATA_LBA)
    img[SUPER_LBA * SECTOR:SUPER_LBA * SECTOR + len(sb)] = sb

    files = sorted(f for f in os.listdir(prog_dir)
                   if os.path.isfile(os.path.join(prog_dir, f)))
    if len(files) > MAX_FILES:
        sys.exit('mkimage: too many files')
    now = pack_time(time.localtime())
    for i, name in enumerate(files):
        data = open(os.path.join(prog_dir, name), 'rb').read().replace(b'\r\n', b'\n')
        if len(name) >= NAME_MAX:
            sys.exit('mkimage: file name too long: ' + name)
        if len(data) > SLOT_SECTORS * SECTOR:
            sys.exit('mkimage: file too large: ' + name)
        entry = struct.pack('<40s6I', name.encode(), len(data), 1, now, 0, 0, 0)
        off = DIR_LBA * SECTOR + i * 64
        img[off:off + 64] = entry
        doff = (DATA_LBA + i * SLOT_SECTORS) * SECTOR
        img[doff:doff + len(data)] = data

    with open(out_path, 'wb') as f:
        f.write(img)
    print('%s: kernel %d bytes (%d sectors), %d file(s) installed'
          % (out_path, len(kernel), ksectors, len(files)))


if __name__ == '__main__':
    main()
