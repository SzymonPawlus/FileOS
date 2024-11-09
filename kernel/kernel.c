#include <stddef.h>
#include <stdint-gcc.h>
#include "../drivers/ports.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../drivers/floppy/floppy.h"
#include "../cpu/isr.h"
#include "../cpu/timer.h"
#include "../cpu/gdt.h"
#include "../libc/memory.h"
#include "../libc/strings.h"
#include "../libc/stdout.h"
#include "../task/task.h"
#include "../drivers/ata/ata.h"

uintptr_t __stack_chk_guard = 0x1234fedc;

__attribute__((noreturn))
void __stack_chk_fail(void){
    kprint("There was some error here in stack_chk_fail");
    __asm__ __volatile__("int $4");
}


char sh_buffer[256];
int sh_buffer_count = 0;
char current_path[256];
volatile int bExecute = 0;

char path_buff[256];
int current_dir_pointer = 0;
VFS_NODE* current_dir;


void keyboard_callback(registers_t regs){
    char key;
    read_key_buffer(&key);
    char buf[2] = { 0, 0 };
    switch (key) {
        case '\b':
            if(!sh_buffer_count) return;
            sh_buffer_count--;
            kprint("\b");
            return;
        case '\n':
            bExecute = 1;
            break;
        default:
            sh_buffer[sh_buffer_count++] = key;
            buf[0] = key;
            kprint(buf);
            break;
    }
}

void main() {
    k_malloc_init();
    gdt_install();
    isr_install();
    init_keyboard(keyboard_callback);
    clear_screen();
    kprint("Hello to File OS! \n\n");
    __asm__ __volatile__("sti");
    floppy_init(Floppy_PIO);
    ata_init(ATA_PIO);
    initialise_multitasking();

    kprint(&current_path[0]);
    kprint("> ");

    while(1);
}