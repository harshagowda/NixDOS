# NixDOS
A tiny OS boots from a X86 (we tested on Intel 386 - Pentium II/III ) we wrote this in 2002
 It’s a small disk based operating system developed for the research purpose.
 It consists of 
           “bootstrap loader”, 
           “its own loader”, 
           “Kernel” and “Shell”.
Languages used are “Assembly” and “C", 
development environment was windows98SE with Turbo C compiler.

Black hat libraries were written in Assembly language and embedded with the Clanguage.
File formats used are “.EXE” and “.COM”
Hardware  supports and  Drivers:   1.44MB floppy  disk,  PIC,  Keyboard controller,Graphics driver and printer driver.
Memory access capacity of 1MB, with the paging size of 16KB and takes 5-6 sec toboot.

## Legacy shell first (C/C++ source, no ASM shell substitute)

This repository keeps and uses the original shell source tree (`SHELL.CPP`, `COMMANDS.CPP`, `DRAW.CPP`, `DATE.CPP`, `DT&TIME.CPP`, `EQUIP.CPP`, `EDITOR.CPP`, etc.).

The boot flow now expects a **legacy shell flat binary** (`SHELL.BIN`) built from those C/C++ sources in a DOS/Turbo-C compatible environment.

## Build flow

1) Build legacy shell binary from C/C++ sources (outside this Linux container if needed).
2) Provide that binary via one of:

```bash
export NIXDOS_LEGACY_SHELL_BIN=/path/to/SHELL.BIN
# or copy it to repository root as SHELL.BIN
```

3) Build bootable image:

```bash
./compile.sh
```

Outputs:

- `build/bootloader.bin`
- `build/nixdos_shell.bin` (imported legacy shell binary)
- `build/nixdos.img`

## Run in VM

```bash
./run_vm.sh
```

## Write to medium / disk

```bash
./write_medium.sh /tmp/nixdos-disk.img
# or block device
# sudo ./write_medium.sh /dev/sdX
```
