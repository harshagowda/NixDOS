# NixDOS
A tiny OS boots from a X86 (we tested on Intel 386 - Pentium II/III ) we wrote this in 2002
 It’s a small disk based operating system developed for the research purpose.
 It consists of 
           “bootstrap loader”, 
           “its own loader”, 
           “Kernel” and “Shell”.
Languages used are “Assembly” and “C”, 
development environment was windows98SE with Turbo C compiler.

Black hat libraries were written in Assembly language and embedded with the Clanguage.
File formats used are “.EXE” and “.COM”
Hardware  supports and  Drivers:   1.44MB floppy  disk,  PIC,  Keyboard controller,Graphics driver and printer driver.
Memory access capacity of 1MB, with the paging size of 16KB and takes 5-6 sec toboot.

## NASM build flow (new)

This repository now includes a NASM-compatible boot path for NixDOS:

- `bootloader.asm` – 512-byte boot sector that loads the shell from disk.
- `nixdos_shell.asm` – 16-bit real-mode shell stage loaded at `0x0000:0x1000`.
- `compile.sh` – build script that assembles both binaries and creates a bootable floppy image.

### Build

```bash
./compile.sh
```

Build output:

- `build/bootloader.bin`
- `build/nixdos_shell.bin`
- `build/nixdos.img`

### Run in QEMU

```bash
qemu-system-i386 -fda build/nixdos.img
```

Shell commands:

- `HELP`
- `CLS`
- `VER`
- `REBOOT`
- `HALT`

### Legacy source compatibility note

The existing `SHELL.CPP`, `COMMANDS.CPP`, `DRAW.CPP`, `DATE.CPP`, `DT&TIME.CPP`, `EQUIP.CPP`, `EDITOR.CPP`, and related files are the original multi-module shell source tree.

This NASM boot flow does **not** replace those historical sources; it provides a bootable NASM path and mirrors the same command names in `nixdos_shell.asm` (`help/clr/vers/time/ctime/date/cdate/clock/ccolor/ndedit/prtmsg/prtscr/equip/rboot/sdown`) so the command surface aligns with the legacy shell design.
