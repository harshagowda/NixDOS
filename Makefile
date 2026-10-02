# NixDOS 2 build
#
#   make          build build/nixdos2.img (bootable hard-disk image)
#   make run      boot it in QEMU (VGA window)
#   make run-serial  boot it in QEMU with the console on this terminal
#   make test     run the automated QEMU test-suite
#   make clean

BUILD    := build
KSRC     := src/kernel
IMAGE    := $(BUILD)/nixdos2.img

CC       := gcc
LD       := ld
NASM     := nasm
OBJCOPY  := objcopy
PYTHON   := python3
QEMU     := qemu-system-i386

CFLAGS   := -m32 -march=i386 -std=gnu99 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
            -fno-asynchronous-unwind-tables -fno-builtin -nostdlib -mgeneral-regs-only \
            -O2 -Wall -Wextra -Wno-unused-parameter --param=min-pagesize=0
LDFLAGS  := -m elf_i386 -T $(KSRC)/linker.ld -nostdlib

C_SRCS   := $(wildcard $(KSRC)/*.c)
C_OBJS   := $(patsubst $(KSRC)/%.c,$(BUILD)/kernel/%.o,$(C_SRCS))
ASM_OBJS := $(BUILD)/kernel/entry.o
HEADERS  := $(wildcard $(KSRC)/*.h)
PROGRAMS := $(wildcard src/programs/*)

.PHONY: all run run-serial test clean

all: $(IMAGE)

$(BUILD)/kernel/%.o: $(KSRC)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel/entry.o: $(KSRC)/entry.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf32 $< -o $@

$(BUILD)/kernel.elf: $(ASM_OBJS) $(C_OBJS) $(KSRC)/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(ASM_OBJS) $(C_OBJS)

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/boot.bin: src/boot/boot.asm $(BUILD)/kernel.bin
	$(NASM) -f bin -D KERNEL_SECTORS=$$(( ($$(stat -c%s $(BUILD)/kernel.bin) + 511) / 512 )) $< -o $@

$(IMAGE): $(BUILD)/boot.bin $(BUILD)/kernel.bin tools/mkimage.py $(PROGRAMS)
	$(PYTHON) tools/mkimage.py $(BUILD)/boot.bin $(BUILD)/kernel.bin src/programs $@

run: $(IMAGE)
	$(QEMU) -m 32 -drive file=$(IMAGE),format=raw,if=ide -serial stdio

run-serial: $(IMAGE)
	$(QEMU) -m 32 -drive file=$(IMAGE),format=raw,if=ide -display none -serial stdio

test: $(IMAGE)
	$(PYTHON) tests/run_tests.py $(IMAGE)

clean:
	rm -rf $(BUILD)/kernel $(BUILD)/kernel.elf $(BUILD)/kernel.bin $(BUILD)/boot.bin $(IMAGE)
