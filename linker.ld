/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(loader)
OUTPUT_FORMAT(elf32-i386)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	. = 0x00100000;
	 _kernel_start = .;

    .dma_buffer : ALIGN(16K){
        _DMA_BUFFER_POS = .;
        *(.dma_buff)
    }

    .multiboot.data : {
        *(.multiboot)
        *(.multiboot.tables)
        *(.multiboot.gdt)
    }

    .multiboot.text : {
        *(.multiboot.text)
        _START = .;
    }


    . += 0xC0000000;
	.text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000)
	{
		*(.entry_text)
		*(.text)
	}


	/* Read-only data. */
	.rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000)
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000)
	{
		*(.data)
	}

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss)

	}

	.heap ALIGN(4K) : AT(ADDR(.heap) - 0xC0000000)
	{
	    __HEAP = .;
	    *(.heap)
	    __HEAP_END = .;
	}

    _kernel_end = .;


}
