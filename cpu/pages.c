#include "pages.h"
#include "types.h"

extern unsigned int _START;

const uint32_t start_address = (uint32_t)&_START;

uint32_t page_directory[1024] __attribute__((section(".multiboot.tables")))  __attribute__((aligned(4096))) = { 0 };
uint32_t first_page_table[1024] __attribute__((section(".multiboot.tables"))) __attribute__((aligned(4096))) = { 0 };
uint32_t second_page_table[1024] __attribute__((section(".multiboot.tables"))) __attribute__((aligned(4096))) = { 0 };
uint32_t third_page_table[1024] __attribute__((section(".multiboot.tables"))) __attribute__((aligned(4096))) = { 0 };

void __attribute__((section(".multiboot.text"))) setup_dir(){
    for (int i = 0; i < 1024; ++i) {
        page_directory[i] = 0x02;
    }
}

void __attribute__((section(".multiboot.text"))) setup_table(){


    for (int i = 0; i < 1024; ++i) {
        first_page_table[i] = (i * 0x1000) | 3;
        second_page_table[i] = (start_address + i * 0x001000) | 3;
        third_page_table[i] = (start_address + i * 0x001000 + 0x400000) | 3;
    }

    page_directory[0]   = ((uint32_t)first_page_table)  | 3;
    page_directory[768] = ((uint32_t)second_page_table) | 3;
    page_directory[769] = ((uint32_t)third_page_table)  | 3;

}
