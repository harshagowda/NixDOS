#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
IMAGE="$BUILD_DIR/nixdos.img"
BOOT_BIN="$BUILD_DIR/bootloader.bin"
SHELL_BIN="$BUILD_DIR/nixdos_shell.bin"

MODULE_NAMES=(time ctime date cdate clock ccolor ndedit prtmsg prtscr equip)

mkdir -p "$BUILD_DIR"

if ! command -v nasm >/dev/null 2>&1; then
  echo "Error: nasm is required." >&2
  exit 1
fi
if ! command -v gcc >/dev/null 2>&1; then
  echo "Error: gcc is required to build legacy C modules." >&2
  exit 1
fi
if ! command -v objcopy >/dev/null 2>&1; then
  echo "Error: objcopy is required to convert modules to flat binaries." >&2
  exit 1
fi

# Build shell first (bootloader needs sector count)
nasm -f bin "$ROOT_DIR/nixdos_shell.asm" -o "$SHELL_BIN"
shell_size=$(stat -c%s "$SHELL_BIN")
shell_sectors=$(((shell_size + 511) / 512))

if (( shell_sectors < 1 )); then
  echo "Error: shell binary is empty." >&2
  exit 1
fi

# Shell must fit sectors 2..3 because modules start at sector 4.
if (( shell_sectors > 2 )); then
  echo "Error: nixdos_shell.bin must be <= 2 sectors (1024 bytes)." >&2
  exit 1
fi

# Build bootloader with actual shell sector count.
nasm -f bin -D SHELL_SECTORS="$shell_sectors" "$ROOT_DIR/bootloader.asm" -o "$BOOT_BIN"

# Build legacy command modules from C into 16-bit flat binaries.
for module in "${MODULE_NAMES[@]}"; do
  src="$ROOT_DIR/modules/${module}.c"
  obj="$BUILD_DIR/${module}.o"
  out="$BUILD_DIR/${module}.bin"

  gcc -m16 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -nostdlib -c "$src" -o "$obj"
  objcopy -O binary -j .text "$obj" "$out"

  module_size=$(stat -c%s "$out")
  if (( module_size > 512 )); then
    echo "Error: module ${module}.bin is ${module_size} bytes; max is 512." >&2
    exit 1
  fi
done

# Build a 1.44MB floppy image.
dd if=/dev/zero of="$IMAGE" bs=512 count=2880 status=none

# Sector 1: bootloader
dd if="$BOOT_BIN" of="$IMAGE" conv=notrunc status=none

# Sectors 2.. : shell
dd if="$SHELL_BIN" of="$IMAGE" bs=512 seek=1 conv=notrunc status=none

# Fixed module sectors 4..13 (seek is zero-based sector index)
sector=3
for module in "${MODULE_NAMES[@]}"; do
  dd if="$BUILD_DIR/${module}.bin" of="$IMAGE" bs=512 seek="$sector" conv=sync,notrunc status=none
  sector=$((sector + 1))
done

echo "Built image: $IMAGE"
echo "Shell size: $shell_size bytes ($shell_sectors sectors)"
echo "Legacy C modules packed in sectors 4..13"
echo "Run VM with: ./run_vm.sh"
