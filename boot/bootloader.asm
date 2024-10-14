[bits 32]
global _loader
extern _kernel_start
extern _kernel_end
MBALIGN  equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot

; Declare a multiboot header that marks the program as a kernel. These are magic
; values that are documented in the multiboot standard. The bootloader will
; search for this signature in the first 8 KiB of the kernel file, aligned at a
; 32-bit boundary. The signature is in its own section so the header can be
; forced to be within the first 8 KiB of the kernel file.


KERNEL_VIRTUAL_BASE equ 0xC0000000                  ; 3GB
KERNEL_PAGE_NUMBER equ (KERNEL_VIRTUAL_BASE >> 22)  ; Page directory index of kernel's 4MB PTE.

; Paging
section .data
align 4096
boot_page_directory:
    resb 4096
boot_page_table1:
    resb 4096

section .multiboot
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

section .dma_buff:
    resb 0x8000


section .multiboot.text

extern setup_table
extern setup_dir
extern page_directory
extern gdt_setup
loader equ(_loader)
global loader
_loader:
    cli
    mov esp, small_stack
    call gdt_setup
    call setup_dir
    call setup_table
_set_up:

    mov ecx, page_directory
    mov cr3, ecx

    mov ecx, cr0
    or ecx, 0x80000001
    mov cr0, ecx

    lea ecx, [higher_half]
    jmp ecx

resb 4096
small_stack:

section .entry_text
higher_half:
	mov esp, stack_top
	push eax

	push ebx

	extern main
	call main
	jmp $

	cli
.hang:	hlt
	jmp .hang
.end:

; Stack
section .bss
align 16
stack_bottom:
    resb 0x40000
stack_top:

; Heap
section .heap
align 16
heap_start:
    resb 0x40000
heap_end:
