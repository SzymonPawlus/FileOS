#!/usr/bin/env sh
pkill qemu
make debug
qemu-system-i386 -s -hda kernel.iso -drive file=disk.img -S --daemonize -d cpu
