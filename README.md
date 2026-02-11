# NixDOS

NixDOS is a tiny x86 operating system project originally built around 2002 for research and learning.
The original environment was Windows 98 SE + Turbo C, and the codebase still preserves that style and layout.

This repository keeps the historical C/C++ shell sources and a modernized build wrapper that can pack a bootable floppy image.

---

## Project goals

- Preserve the original educational DOS-era OS code.
- Keep the boot path simple and inspectable (single-sector bootloader + flat shell binary).
- Allow contributors to experiment with low-level x86 behavior (BIOS disk I/O, keyboard controller, A20, text mode output).

---

## High-level architecture

```text
+-------------------------------+
|        BIOS (real mode)       |
+---------------+---------------+
                |
                | loads sector 1 to 0000:7C00 and jumps
                v
+-------------------------------+
|     bootloader.asm (512B)     |
| - set segments/stack          |
| - enable A20                  |
| - read shell sectors via INT13|
| - jump to 0000:1000           |
+---------------+---------------+
                |
                | shell binary loaded from disk sectors 2..N
                v
+-------------------------------+
|       SHELL.BIN (legacy)      |
|  built from Turbo-C era code  |
|  (SHELL.CPP + modules)        |
+---------------+---------------+
                |
                v
+-------------------------------+
| user commands / apps / demos  |
+-------------------------------+
```

### Boot memory map (simplified)

```text
Physical Address
0x00000  ---------------------------
         | IVT / BIOS data area    |
0x07C00  ---------------------------  <- bootloader loaded/executed here
         | bootloader stack (SP)   |
0x01000  ---------------------------  <- shell load target (0000:1000)
         | SHELL.BIN image         |
         | ...                     |
0xA0000  ---------------------------  <- VGA / hardware regions
0x100000 ---------------------------  <- 1 MiB boundary (A20 relevant)
```

---

## Repository layout and file guide

> The list below describes each top-level project file so a new contributor can quickly orient and pick up work.

### Boot/build/runtime scripts

- `bootloader.asm`  
  16-bit NASM boot sector. Initializes runtime state, enables A20, reads the legacy shell from disk, and jumps to it.
- `compile.sh`  
  Main build entrypoint. Imports/builds the shell binary, computes sector count, assembles bootloader, creates `build/nixdos.img`.
- `build_legacy_shell.sh`  
  Helper for getting `build/nixdos_shell.bin`. Either imports prebuilt `SHELL.BIN` or fails with clear guidance.
- `run_vm.sh`  
  Starts a VM for `build/nixdos.img` (QEMU-based workflow).
- `write_medium.sh`  
  Writes generated image to a file/device (for testing with raw disk images or real media).

### Core legacy shell sources (Turbo-C era)

- `SHELL.CPP` — main shell loop and command dispatcher glue.
- `COMMANDS.CPP` — command handling routines.
- `STRING.CPP` — helper string routines used by shell utilities.
- `WELCOME.CPP` — startup/welcome output.
- `DRAW.CPP` — drawing/graphics related routines.
- `EDITOR.CPP` — simple editor functionality.
- `DISK.CPP` — disk/file related command helpers.
- `COPYBOOT.CPP` — boot-copy utility logic.
- `DATE.CPP`, `DT&TIME.CPP`, `DT&TIME2.CPP`, `TMPTIME.CPP` — date/time helpers and experiments.
- `EQUIP.CPP` — hardware/equipment reporting helpers.
- `ASCCI.CPP` — ASCII related utility output.

### Metadata and docs

- `README.md` — this document.
- `LICENSE` — project license terms.
- `.gitignore` — ignored local/generated files.

---

## End-to-end build flow

1. **Prepare legacy shell binary** from original C/C++ sources in a DOS/Turbo-C compatible environment.
2. **Expose binary to this repo**:

   ```bash
   export NIXDOS_LEGACY_SHELL_BIN=/path/to/SHELL.BIN
   # or copy it to repository root as ./SHELL.BIN
   ```

3. **Build bootable image**:

   ```bash
   ./compile.sh
   ```

4. **Artifacts produced**:

   - `build/bootloader.bin`
   - `build/nixdos_shell.bin`
   - `build/nixdos.img`

5. **Run image in VM**:

   ```bash
   ./run_vm.sh
   ```

6. **Optional: write image to disk/media**:

   ```bash
   ./write_medium.sh /tmp/nixdos-disk.img
   # or, carefully, a block device
   # sudo ./write_medium.sh /dev/sdX
   ```

---

## A20 note (important for memory access)

The bootloader enables **A20** during early startup so addresses above `0xFFFFF` (1 MiB) are reachable without 20-bit wraparound behavior.

Current strategy in `bootloader.asm`:

1. Try the **fast A20 gate** (port `0x92`).
2. Fallback to **keyboard controller method** (`0x64`/`0x60`) if needed.

This preserves compatibility across emulator/older hardware combinations.

---

## How a new contributor can pick up work quickly

1. Read `bootloader.asm` first (short and central to boot flow).
2. Run `./compile.sh` and inspect `build/` outputs.
3. If shell work is needed, focus on `SHELL.CPP` + `COMMANDS.CPP` first.
4. Keep changes small and testable; verify bootloader assembles with NASM before broader testing.
5. Document any legacy assumptions (memory model, compiler behavior, BIOS dependencies) in this README as you discover them.

---

## Known constraints

- The C/C++ shell is legacy Turbo-C style and not directly buildable with a modern Linux GCC/Clang toolchain without adaptation.
- Boot path is BIOS/real-mode oriented.
- Floppy-image style layout is assumed by default scripts.

If you modernize any subsystem, prefer preserving the historical path in parallel rather than replacing it outright.
