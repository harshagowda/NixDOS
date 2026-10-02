# NixDOS

NixDOS is a tiny x86 operating system project originally built around 2002 for research and learning.
The original environment was Windows 98 SE + Turbo C, and the codebase still preserves that style and layout.

This repository keeps the historical C/C++ shell sources and a modernized build wrapper that can pack a bootable floppy image.

It also contains **NixDOS 2.0** (`src/`): a complete rewrite as a 32-bit protected-mode operating
system with its own file system, editor and a **built-in C compiler**. NixDOS 2 builds with a normal
Linux toolchain and keeps the original commands (`vers`, `ctime`, `cdate`, `ccolor`, `ndedit`, `prtmsg`,
`equip`, `rboot`, `sdown`, ...). The legacy 1.0 sources below are unchanged.

---

## NixDOS 2.0 quick start

Requirements: `gcc`/`g++` (with `-m32` support), `binutils`, `nasm`, `python3`, `qemu-system-i386`.

```bash
make              # builds build/nixdos2.img (8 MiB bootable hard-disk image)
make run          # boots it in QEMU (VGA window, console also mirrored to this terminal)
make WOLF_TESTDATA=1 run   # same, with placeholder Wolfenstein 3-D data installed
make run-serial   # boots without a window; use the OS from this terminal
make test         # automated end-to-end tests in QEMU
```

Inside NixDOS:

```text
$] ls                         list files (sample programs are pre-installed)
$] cat hello.c                show a file
$] cc hello.c                 compile and run a C program
$] cc snake.c -o snake.nxe    compile to an executable file ...
$] snake                      ... and run it by name
$] edit myprog.c              full-screen editor (^S save, ^Q quit)
$] help                       everything else
```

Ctrl+C stops a running program. A program that crashes (e.g. divides by zero) is stopped
and you are returned to the shell. Files are saved to the disk and survive reboots.

### What NixDOS 2 contains

| Part | File(s) | What it does |
|---|---|---|
| Boot sector | `src/boot/boot.asm` | Enables **A20** (BIOS, fast gate, keyboard controller) and *verifies* it with a 1 MiB wrap-around test; loads the kernel with INT 13h LBA reads (with retries); switches to 32-bit protected mode |
| Kernel core | `entry.asm`, `kernel.c`, `idt.c` | Entry, interrupt table, 8259 PIC, CPU exception handling |
| Drivers | `console.c`, `serial.c`, `keyboard.c`, `timer.c`, `rtc.c`, `ata.c` | VGA text mode, COM1 serial console, PS/2 keyboard, PIT timer + PC speaker, CMOS clock, ATA hard disk (PIO) |
| Memory | `mem.c` | Memory size detection, heap allocator above 1 MiB |
| File system | `fs.c` | **NXFS v2**: up to 128 files of any size (each stored contiguously) on the boot disk |
| Shell | `shell.c` | Commands, history (Up/Down), line editing, `echo > file` |
| Editor | `editor.c` | NixEdit full-screen text editor with auto-indent |
| Compiler | `cc.c`, `api.h` | **NixC**, a one-pass C compiler that generates x86 machine code |
| Runtime | `program.c` | Loads/runs programs, library functions for programs, Ctrl+C and crash handling |
| Tools | `tools/mkimage.py`, `Makefile` | Builds the disk image and pre-installs `src/programs/*` |
| Tests | `tests/` | Boots the OS in QEMU and drives it over the serial port |

### Memory map (paging off, flat 4 GiB segments)

```text
0x00000500  pointer to the program API table
0x00010000  kernel (loaded by the boot sector)
0x00090000  kernel stack top
0x000B8000  VGA text memory
0x00100000  kernel heap (~3 MiB)         <- above 1 MiB: needs A20
0x003E0000  Sound Blaster DMA buffer
0x00400000  user program code (512 KiB)
0x00480000  user program data (512 KiB)
0x00800000  user program stack top / start of the program heap (malloc)
```

### Disk layout (512-byte sectors)

```text
LBA 0          boot sector
LBA 1..1023    kernel
LBA 1024       NXFS superblock
LBA 1025-1040  directory (128 x 64-byte entries)
LBA 1041..     file data, one contiguous extent per file
```

### NixC - the built-in C compiler

NixC compiles a practical subset of C directly to 32-bit x86 machine code in one pass;
there is no assembler or linker step. `cc file.c` compiles straight into the program area
and runs it. `cc file.c -o file.nxe` writes an executable instead (16-byte header + code + data).

Supported:

- `int`, `char`, `void`, pointers of any depth, one-dimensional arrays, `unsigned`/`long`/`const`/`static`
  are accepted (treated as `int` or ignored)
- global and local variables with initialisers (`int a[] = {1, 2, 3};`, `char s[] = "hi";`, `char *names[] = {...}`)
- functions, recursion, prototypes, calling a function before it is defined, `main(int argc, char **argv)`
- `if`/`else`, `while`, `do`/`while`, `for` (with declarations), `switch`/`case`/`default`, `break`, `continue`, `return`
- all C arithmetic, bitwise, logical, comparison and assignment operators, `?:`, `,`, prefix/postfix `++`/`--`,
  casts, `sizeof`, pointer arithmetic, `&` and `*`
- decimal/hex/octal numbers, character and string escapes, `//` and `/* */` comments
- `#define NAME <integer>`; other `#` lines such as `#include` are ignored

Not supported: `struct`/`union`/`enum`/`typedef`, floating point, function pointers,
multi-dimensional arrays, `goto`, and macros with parameters.

Library functions programs can call:

```text
I/O       printf sprintf puts putchar getchar gets(buf, max) getkey kbhit
strings   strlen strcmp strncmp strcpy strcat strchr memset memcpy atoi
chars     isdigit isalpha isspace toupper tolower abs
memory    malloc free
time      time ticks sleep(ms) rand srand
screen    cls gotoxy(x, y) setcolor(fg, bg) putat(x, y, ch, color) wherex wherey beep(freq, ms)
files     readfile(name, buf, max) writefile(name, buf, len)
program   exit(code)
```

Constants: `NULL`, `EOF`, `true`, `false`, `KEY_UP`/`DOWN`/`LEFT`/`RIGHT`/`HOME`/`END`/`PGUP`/`PGDN`/`DEL`/`ESC`/`ENTER`,
the 16 colours (`BLACK` ... `YELLOW`, `WHITE`), `SCREEN_W`, `SCREEN_H`.

How it works: an expression's value is kept in `EAX`; the left operand of a binary operator is pushed
on the stack while the right one is computed. Locals live at `[EBP-n]` and parameters at `[EBP+8+4i]`.
Calls use the cdecl convention, so compiled programs call the kernel's library functions directly
through a table whose address is stored at `0x500`. Executables keep working when the kernel is rebuilt,
as long as new library functions are only ever added to the end of the list in `src/kernel/api.h`.

Sample programs (`src/programs/`): `hello.c`, `primes.c`, `fib.c`, `sort.c`, `strings.c`, `files.c`,
`queens.c`, `mandel.c`, `calc.c`, `guess.c`, `snake.c`.

### Native programs (GCC) and Wolfenstein 3-D

Besides NixC, NixDOS runs programs compiled on the build machine with GCC against its own small C
library (`src/libc`: stdio, stdlib, string, math with the x87 FPU, POSIX-style `open`/`read`/`lseek`,
a heap allocator, C++ `new`/`delete`, static constructors). The executables are flat images
(`NXN1` header) loaded at 0x400000. You can have up to 3.5 MiB of code and data and a 512 KiB stack,
and the heap uses the rest of RAM. Put a `.c` file in `src/native/` and `make` installs it
(see `hello_native.c`).

**Wolfenstein 3-D** runs as a native program. It is **Wolf4SDL** (the 32-bit port of id Software's
GPL-licensed source, `ports/wolf3d/src`), built against a small SDL2/SDL_mixer replacement
(`ports/sdl`):

| Piece | Where |
|---|---|
| 320x200x256 graphics (VGA mode 13h, programmed directly; the text font, palette and screen are saved and restored) | `src/kernel/vga.c` |
| Raw key-up/key-down scancodes for the game | `src/kernel/keyboard.c` |
| Sound Blaster 16 driver (16-bit stereo, auto-init DMA, IRQ 5) | `src/kernel/sb16.c` |
| Mixer for AdLib music (OPL emulation), digitised sound effects and PC speaker sounds; it works silently without a sound card | `ports/sdl/sdl_mixer.c` |
| Large files (NXFS v2 extents) and file handles | `src/kernel/fs.c` |
| x87 FPU enabled, 1 kHz timer | `src/kernel/kernel.c`, `timer.c` |
| NixDOS patches to Wolf4SDL (marked `NIXDOS`) | `version.h`, `id_vl.cpp`, `wl_menu.cpp` |

The game data is copyrighted by id Software, so it is not included. To play:

```bash
cp /path/to/shareware/*.WL1 ports/wolf3d/data/     # Wolfenstein 3-D shareware v1.4
make run                                           # in NixDOS type:  wolf3d
```

For the registered v1.4 Apogee data, copy the `.WL6` files instead and build with `make WOLF_DATA=wl6`.
For sound, add a Sound Blaster 16 to QEMU: `-audiodev pa,id=s -device sb16,audiodev=s`
(use `alsa` or `sdl` in place of `pa` depending on your system).
Controls are the original ones: arrow keys, Ctrl fires, Alt strafes, Shift runs, Space opens doors,
Esc opens the menu, and F10 quits.

Without the real data you can still try the engine: `make WOLF_TESTDATA=1 run` generates valid Wolf3D
data files with placeholder art and test levels (`ports/wolf3d/tools/testdata.cpp`).
`make test` uses that data to check the graphics, keyboard, audio and exit back to the shell.

License note: `ports/wolf3d/` is GPLv2 (see the license files there), so an image that includes
`wolf3d.nxe` must be distributed under the GPL terms. The rest of NixDOS keeps its own license.

### Tests

`make test` boots a copy of the image in QEMU and checks the shell, file system, editor, crash handling,
Ctrl+C, persistence across a reboot and the compiler. The compiler tests in `tests/c/` are also built
with the host GCC, and the two outputs must be identical.

### Limits and known gaps

- BIOS boot only (no UEFI). The boot sector needs INT 13h LBA extensions, which means a hard disk or USB
  stick, not a floppy. It has no partition table, which some real BIOSes need before they will boot a USB stick.
- Programs run in ring 0 without memory protection. A program cannot crash the OS through divide errors
  or similar faults, but a bad pointer can still overwrite kernel memory.
- One program at a time (no multitasking). At least 9 MiB of RAM is needed to run programs (QEMU: `-m 64`).
- No mouse or joystick support yet (Wolf3D is played with the keyboard).

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
