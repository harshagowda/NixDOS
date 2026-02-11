#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}" )" && pwd)"
IMAGE="$ROOT_DIR/build/nixdos.img"

if [[ ! -f "$IMAGE" ]]; then
  echo "Image not found: $IMAGE" >&2
  echo "Run ./compile.sh first." >&2
  exit 1
fi

if ! command -v qemu-system-i386 >/dev/null 2>&1; then
  echo "Error: qemu-system-i386 is not installed." >&2
  exit 1
fi

qemu-system-i386 -fda "$IMAGE"
