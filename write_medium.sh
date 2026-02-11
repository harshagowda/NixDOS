#!/usr/bin/env bash
set -euo pipefail

# Write bootable NixDOS image to a target medium (file or block device).
# Usage:
#   ./write_medium.sh /path/to/target
# Examples:
#   ./write_medium.sh /tmp/nixdos-disk.img
#   sudo ./write_medium.sh /dev/sdb

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="$ROOT_DIR/build/nixdos.img"
TARGET="${1:-}"

if [[ -z "$TARGET" ]]; then
  echo "Usage: $0 <target-file-or-block-device>" >&2
  exit 1
fi

if [[ ! -f "$IMAGE" ]]; then
  echo "Image not found: $IMAGE" >&2
  echo "Run ./compile.sh first." >&2
  exit 1
fi

if [[ -b "$TARGET" ]]; then
  echo "Writing to block device: $TARGET"
  dd if="$IMAGE" of="$TARGET" bs=4M conv=fsync status=progress
  sync
  echo "Done. Boot $TARGET in your VM/hardware."
else
  mkdir -p "$(dirname "$TARGET")"
  echo "Writing to disk image file: $TARGET"
  dd if="$IMAGE" of="$TARGET" bs=1M conv=fsync status=none
  echo "Done. Boot with: qemu-system-i386 -fda $TARGET"
fi
