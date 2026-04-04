CC      = i686-elf-gcc
CFLAGS  = -ffreestanding -O2 -Wall -m32
NASM    = nasm
QEMU    = qemu-system-i386

all: os.img

# assemble bootloader → raw binary
boot/boot.bin: boot/boot.asm
	$(NASM) -f bin $< -o $@

# compile kernel → ELF, then strip to raw binary
kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/kernel.bin: kernel/kernel.o linker.ld
	$(CC) $(CFLAGS) -T linker.ld -nostdlib $< -o kernel/kernel.elf
	objcopy -O binary kernel/kernel.elf $@

# combine bootloader + kernel into a single disk image
os.img: boot/boot.bin kernel/kernel.bin
	cat boot/boot.bin kernel/kernel.bin > os.img

# run in QEMU
run: os.img
	$(QEMU) -drive format=raw,file=os.img

clean:
	rm -f boot/boot.bin kernel/kernel.o kernel/kernel.elf kernel/kernel.bin os.img
