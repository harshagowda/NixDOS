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
CXX      := g++
LD       := ld
NASM     := nasm
OBJCOPY  := objcopy
PYTHON   := python3
QEMU     := qemu-system-i386

CFLAGS   := -m32 -march=i386 -std=gnu99 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
            -fno-asynchronous-unwind-tables -fno-builtin -nostdlib -mgeneral-regs-only \
            -O2 -Wall -Wextra -Wno-unused-parameter --param=min-pagesize=0
LDFLAGS  := -m elf_i386 -T $(KSRC)/linker.ld -nostdlib --no-warn-rwx-segments

C_SRCS   := $(wildcard $(KSRC)/*.c)
C_OBJS   := $(patsubst $(KSRC)/%.c,$(BUILD)/kernel/%.o,$(C_SRCS))
ASM_OBJS := $(BUILD)/kernel/entry.o
HEADERS  := $(wildcard $(KSRC)/*.h)
PROGRAMS := $(wildcard src/programs/*)

# ---- native programs (GCC + src/libc), loaded at 0x400000 ---------------------
GCC_INC     := $(shell $(CC) -print-file-name=include)
NATIVE_FLAGS := -m32 -march=i686 -mno-sse -mfpmath=387 -ffreestanding -fno-pic -fno-pie \
            -fno-stack-protector -fno-asynchronous-unwind-tables -nostdinc -isystem $(GCC_INC) \
            -Isrc/libc/include -O2 --param=min-pagesize=0
NATIVE_CFLAGS   := $(NATIVE_FLAGS) -std=gnu99 -Wall
NATIVE_CXXFLAGS := $(NATIVE_FLAGS) -nostdinc++ -fno-exceptions -fno-rtti -fno-threadsafe-statics
LIBC_OBJS := $(BUILD)/libc/crt0.o $(BUILD)/libc/libc.o $(BUILD)/libc/math.o $(BUILD)/libc/cxx.o
LIBC      := $(BUILD)/libc/libc.a
NATIVE_SRCS := $(wildcard src/native/*.c)
NATIVE_EXES := $(patsubst src/native/%.c,$(BUILD)/native/%.nxe,$(NATIVE_SRCS))
LIBC_HDRS := $(wildcard src/libc/include/*) $(wildcard src/libc/include/sys/*) src/kernel/api.h

# ---- Wolfenstein 3-D (Wolf4SDL port, GPLv2) ----------------------------------
# Game data: copy the *.WL1 (shareware 1.4) or *.WL6 files into ports/wolf3d/data/
WOLF_DATA ?= wl1
WOLF_SRC  := ports/wolf3d/src
WOLF_CPPS := dosbox/dbopl.cpp id_ca.cpp id_in.cpp id_pm.cpp id_sd.cpp id_us_1.cpp id_vh.cpp \
             id_vl.cpp signon.cpp wl_act1.cpp wl_act2.cpp wl_agent.cpp wl_atmos.cpp \
             wl_cloudsky.cpp wl_debug.cpp wl_draw.cpp wl_floorceiling.cpp wl_game.cpp \
             wl_inter.cpp wl_main.cpp wl_menu.cpp wl_parallax.cpp wl_play.cpp wl_state.cpp \
             wl_text.cpp states.cpp
WOLF_OBJS := $(patsubst %.cpp,$(BUILD)/wolf3d/%.o,$(WOLF_CPPS))
WOLF_FLAGS := $(NATIVE_CXXFLAGS) -DNIXDOS $(if $(filter wl6,$(WOLF_DATA)),-DNIXDOS_WL6) \
              -Iports/sdl/include -Wno-write-strings
SDL_OBJS  := $(BUILD)/sdl/sdl_nixdos.o $(BUILD)/sdl/sdl_mixer.o
WOLF_EXE  := $(BUILD)/wolf3d/wolf3d.nxe
WOLF_FILES := $(wildcard ports/wolf3d/data/*.$(WOLF_DATA)) $(wildcard ports/wolf3d/data/*.$(shell echo $(WOLF_DATA) | tr a-z A-Z))
# "make WOLF_TESTDATA=1": install generated placeholder data instead (for testing)
WOLF_TESTDATA_DIR := $(BUILD)/wolf3d-testdata
WOLF_TESTDATA_FILES := $(addprefix $(WOLF_TESTDATA_DIR)/,vswap.wl1 vgadict.wl1 vgahead.wl1 vgagraph.wl1 \
                       maphead.wl1 gamemaps.wl1 audiohed.wl1 audiot.wl1)
ifeq ($(WOLF_TESTDATA),1)
WOLF_FILES := $(WOLF_TESTDATA_FILES)
endif
EXTRA_FILES += $(WOLF_EXE) $(WOLF_FILES)

.PHONY: all run run-serial test clean wolf3d wolf3d-testdata

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

$(BUILD)/libc/%.o: src/libc/%.c $(LIBC_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(NATIVE_CFLAGS) -c $< -o $@

$(BUILD)/libc/%.o: src/libc/%.cpp $(LIBC_HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(NATIVE_CXXFLAGS) -c $< -o $@

$(LIBC): $(LIBC_OBJS)
	rm -f $@
	ar rcs $@ $^

$(BUILD)/native/%.o: src/native/%.c $(LIBC_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(NATIVE_CFLAGS) -c $< -o $@

NATIVE_LINK = $(LD) -m elf_i386 -T src/libc/native.ld -nostdlib --no-warn-rwx-segments

$(BUILD)/native/%.nxe: $(BUILD)/native/%.o $(LIBC) src/libc/native.ld
	$(NATIVE_LINK) -o $(@:.nxe=.elf) $(BUILD)/libc/crt0.o $< $(LIBC)
	$(OBJCOPY) -O binary $(@:.nxe=.elf) $@

$(BUILD)/sdl/%.o: ports/sdl/%.c $(wildcard ports/sdl/include/*.h) $(LIBC_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(NATIVE_CFLAGS) -Iports/sdl/include -c $< -o $@

$(BUILD)/wolf3d/%.o: $(WOLF_SRC)/%.cpp $(wildcard $(WOLF_SRC)/*.h) $(wildcard ports/sdl/include/*.h) $(LIBC_HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(WOLF_FLAGS) -c $< -o $@

$(WOLF_EXE): $(WOLF_OBJS) $(SDL_OBJS) $(LIBC) src/libc/native.ld
	$(NATIVE_LINK) -o $(@:.nxe=.elf) $(BUILD)/libc/crt0.o $(WOLF_OBJS) $(SDL_OBJS) $(LIBC)
	$(OBJCOPY) -O binary $(@:.nxe=.elf) $@

wolf3d: $(WOLF_EXE)

$(WOLF_TESTDATA_DIR)/gen: ports/wolf3d/tools/testdata.cpp $(wildcard $(WOLF_SRC)/*.h)
	@mkdir -p $(dir $@)
	g++ -O1 -w -DNIXDOS -I$(WOLF_SRC) -Iports/sdl/include -o $@ $<

$(WOLF_TESTDATA_FILES) &: $(WOLF_TESTDATA_DIR)/gen
	$< $(WOLF_TESTDATA_DIR)

wolf3d-testdata: $(WOLF_TESTDATA_FILES)

$(IMAGE): $(BUILD)/boot.bin $(BUILD)/kernel.bin tools/mkimage.py $(PROGRAMS) $(NATIVE_EXES) $(EXTRA_FILES)
	$(PYTHON) tools/mkimage.py $(BUILD)/boot.bin $(BUILD)/kernel.bin $@ src/programs $(NATIVE_EXES) $(EXTRA_FILES)

run: $(IMAGE)
	$(QEMU) -m 64 -drive file=$(IMAGE),format=raw,if=ide -serial stdio

run-serial: $(IMAGE)
	$(QEMU) -m 64 -drive file=$(IMAGE),format=raw,if=ide -display none -serial stdio

test: $(IMAGE) $(WOLF_TESTDATA_FILES)
	$(PYTHON) tests/run_tests.py $(IMAGE)

clean:
	rm -rf $(BUILD)/libc $(BUILD)/native $(BUILD)/sdl $(BUILD)/wolf3d $(BUILD)/wolf3d-testdata $(BUILD)/kernel $(BUILD)/kernel.elf $(BUILD)/kernel.bin $(BUILD)/boot.bin $(IMAGE)
