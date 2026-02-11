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

## NASM build flow (single build for all needed files)

The repository now includes a NASM-based boot path where one build compiles all required parts:

- `bootloader.asm` (boot sector)
- `nixdos_shell.asm` (main shell)
- legacy command modules in `modules/*.c` (`time`, `ctime`, `date`, `cdate`, `clock`, `ccolor`, `ndedit`, `prtmsg`, `prtscr`, `equip`)

`compile.sh` assembles all of the above and packs them into one bootable 1.44MB image.

### Build everything

```bash
./compile.sh
```

Output in `build/` includes:

- `nixdos.img` (bootable floppy image)
- `bootloader.bin`
- `nixdos_shell.bin`
- one `.bin` per module

### Run in VM

```bash
./run_vm.sh
```

(Equivalent: `qemu-system-i386 -fda build/nixdos.img`)

### Write to medium / disk

```bash
./write_medium.sh /tmp/nixdos-disk.img
# or block device:
# sudo ./write_medium.sh /dev/sdX
```

### Command behavior

- `HELP`, `CLR`, `VERS`, `RBOOT`, `SDOWN` are built into `nixdos_shell.asm`.
- `TIME`, `CTIME`, `DATE`, `CDATE`, `CLOCK`, `CCOLOR`, `NDEDIT`, `PRTMSG`, `PRTSCR`, `EQUIP` are compiled from C source into separate flat module binaries.
- When those module commands are called, the shell reads the module sector from disk and executes it.

### Legacy source compatibility note

The existing `SHELL.CPP`, `COMMANDS.CPP`, `DRAW.CPP`, `DATE.CPP`, `DT&TIME.CPP`, `EQUIP.CPP`, `EDITOR.CPP`, and related files are the original multi-module shell source tree.

This NASM boot flow is a bootable VM path that now builds command modules from C sources (flat binaries) and loads them from disk sectors at runtime.
