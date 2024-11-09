C_SOURCES = $(wildcard kernel/*.c drivers/*.c drivers/floppy/*.c drivers/ata/*.c cpu/*.c libc/*.c fs/*.c task/*.c)
HEADERS = $(wildcard kernel/*.h drivers/*.h drivers/floppy/*.h drivers/ata/*.h cpu/*.h libc/*.h fs/*.h task/*.h)

OBJ = ${C_SOURCES:.c=.o cpu/interrupt.o cpu/gdt_setup.o task/context_switch.o}

CC = /home/szymonp/opt/cross/bin/i686-elf-gcc
LD = /home/szymonp/opt/cross/bin/i686-elf-ld
GDB = /home/szymonp/opt/cross/bin/i686-elf-gdb

.PHONY: all debug clean

CFLAGS = -g

bin/os_image.bin: boot/bootloader.bin isodir/boot/kernel.bin
	cat $^ > bin/os_image.bin

isodir/boot/kernel.bin: boot/bootloader.o $(OBJ)
	$(CC) -o $@ -T linker.ld $^ -nostdlib -ffreestanding

kernel.elf: boot/bootloader.o $(OBJ)
	$(CC) -o $@ -T linker.ld $^ -nostdlib -ffreestanding
	@echo "compiling $^"

debug: isodir/boot/kernel.bin kernel.elf bin/os_image.bin
	grub-mkrescue -o kernel.iso isodir
	@echo "App built for debug"

all: isodir/boot/kernel.bin floppy.img
	grub-mkrescue -o kernel.iso isodir
	@echo "App built!"


%.o: %.c ${HEADERS}
	$(CC) ${CFLAGS} -ffreestanding -c $< -o $@ -fstack-protector-all

%.o: %.asm
	nasm $< -felf32 -o $@

%.bin: %.asm
	nasm $< -felf32 -o $@

clean:
	rm -rf bin/os_image.bin kernel.elf floppy.img *.bin *.dis *.o
	rm $(OBJ)

floppy.img:
	dd if=/dev/zero of=floppy.img bs=512 count=100000
	sudo losetup /dev/loop0 floppy.img
	sudo mkdosfs -F 32 /dev/loop0
	sudo mount /dev/loop0 /mnt -t msdos -o "fat=32"
	sudo cp -r ./fat_example_root/* /mnt/
	sudo umount /mnt
	sudo losetup -d /dev/loop0
	sudo chmod 777 floppy.img
