BUILD_DIR := build

ASM := nasm
CC := clang
CXX := clang++
LD := ld.lld
OBJCOPY := llvm-objcopy

COMMON_FLAGS := -target i386-none-elf -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -m32 -nostdlib -Wall -Wextra -Wpedantic
CXXFLAGS := $(COMMON_FLAGS) -std=c++17
ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T linker.ld --nostdlib

KERNEL_OBJECTS := $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
KERNEL_SECTORS := $(BUILD_DIR)/kernel_sectors.inc
BOOT_BIN := $(BUILD_DIR)/boot.bin
IMAGE := $(BUILD_DIR)/joesos.img

.PHONY: all run run-headless clean

all: $(IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/kernel.o: kernel.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_entry.o: kernel_entry.asm | $(BUILD_DIR)
	$(ASM) $(ASFLAGS) $< -o $@

$(KERNEL_ELF): linker.ld $(KERNEL_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(KERNEL_SECTORS): $(KERNEL_BIN) | $(BUILD_DIR)
	perl -e 'my $$size = -s shift; my $$sectors = int(($$size + 511) / 512); die "kernel too large\n" if $$sectors < 1 || $$sectors > 128; print "KERNEL_SECTORS equ $$sectors\n";' $< > $@

$(BOOT_BIN): boot.asm $(KERNEL_SECTORS)
	$(ASM) -f bin $< -o $@

$(IMAGE): $(BOOT_BIN) $(KERNEL_BIN) | $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=512 count=2880
	dd if=$(BOOT_BIN) of=$@ conv=notrunc
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc

run: $(IMAGE)
	qemu-system-i386 -drive format=raw,file=$(IMAGE)

run-headless: $(IMAGE)
	qemu-system-i386 -drive format=raw,file=$(IMAGE) -display none -serial stdio

clean:
	rm -rf $(BUILD_DIR)
