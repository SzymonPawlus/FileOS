#!/usr/bin/sh
pkill qemu
qemu-system-i386 -boot d -cdrom kernel.iso -hda floppy.img
