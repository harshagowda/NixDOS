#!/usr/bin/env bash
set -euo pipefail

# Build (or import) the legacy C shell binary expected by the boot image packer.
# Output: build/nixdos_shell.bin
#
# This repository keeps the original Turbo C era sources (SHELL.CPP and friends).
# In this Linux container we don't have a Borland/Turbo C DOS toolchain, so the
# script supports importing a prebuilt flat shell binary.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
OUT_BIN="$BUILD_DIR/nixdos_shell.bin"

mkdir -p "$BUILD_DIR"

if [[ -n "${NIXDOS_LEGACY_SHELL_BIN:-}" ]]; then
  if [[ ! -f "$NIXDOS_LEGACY_SHELL_BIN" ]]; then
    echo "Error: NIXDOS_LEGACY_SHELL_BIN points to a missing file: $NIXDOS_LEGACY_SHELL_BIN" >&2
    exit 1
  fi
  cp "$NIXDOS_LEGACY_SHELL_BIN" "$OUT_BIN"
  echo "Imported legacy shell binary: $OUT_BIN"
  exit 0
fi

if [[ -f "$ROOT_DIR/SHELL.BIN" ]]; then
  cp "$ROOT_DIR/SHELL.BIN" "$OUT_BIN"
  echo "Using SHELL.BIN from repository root -> $OUT_BIN"
  exit 0
fi

cat >&2 <<'MSG'
Error: could not build/import legacy shell binary.

Provide a prebuilt flat legacy shell binary with one of:
  1) export NIXDOS_LEGACY_SHELL_BIN=/path/to/SHELL.BIN
  2) place SHELL.BIN at repository root

Notes:
- This project intentionally uses the original C shell source tree (SHELL.CPP + related C/C++ files).
- Building those Borland/Turbo C style sources requires a DOS-era toolchain environment.
MSG
exit 1
