#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
IMAGE="$BUILD_DIR/nixdos.img"
BOOT_BIN="$BUILD_DIR/bootloader.bin"
SHELL_BIN="$BUILD_DIR/nixdos_shell.bin"

mkdir -p "$BUILD_DIR"

if ! command -v nasm >/dev/null 2>&1; then
  echo "Error: nasm is required." >&2
  exit 1
fi

nasm -f bin "$ROOT_DIR/nixdos_shell.asm" -o "$SHELL_BIN"

shell_size=$(stat -c%s "$SHELL_BIN")
shell_sectors=$(((shell_size + 511) / 512))

if (( shell_sectors < 1 )); then
  echo "Error: shell binary is empty." >&2
  exit 1
fi

if (( shell_sectors > 18 )); then
  echo "Error: shell is too large for simple CHS loader (max 18 sectors)." >&2
  exit 1
fi

nasm -f bin -D SHELL_SECTORS="$shell_sectors" "$ROOT_DIR/bootloader.asm" -o "$BOOT_BIN"

# Build a 1.44MB floppy image and place shell from sector 2 onward.
dd if=/dev/zero of="$IMAGE" bs=512 count=2880 status=none
dd if="$BOOT_BIN" of="$IMAGE" conv=notrunc status=none

dd if="$SHELL_BIN" of="$IMAGE" bs=512 seek=1 conv=notrunc status=none

echo "Built image: $IMAGE"
echo "Shell size: $shell_size bytes ($shell_sectors sectors)"
echo "Run with: qemu-system-i386 -fda $IMAGE"
