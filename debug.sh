#!/usr/bin/bash
pkill qemu
qemu-system-i386 -s -hda kernel.iso -drive file=floppy.img  -S --daemonize -d cpu
