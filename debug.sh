#!/usr/bin/bash
pkill qemu
make debug
qemu-system-i386 -s -hda kernel.iso -drive file=disk.img  -S --daemonize -d cpu & gdb -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"
