global gdt_setup

section .multiboot.text
gdt_setup:
    cli
    lgdt [gdt_descriptor]
    mov ax, 0x10      ; 0x10 is the offset in the GDT to our data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    jmp 0x08:after_gdt
after_gdt:
    ret

%include "cpu/gdt.asm"
