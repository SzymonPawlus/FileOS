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

void execute_command();

char sh_buffer[256];
int sh_buffer_count = 0;
char current_path[256];
volatile int bExecute = 0;

char path_buff[256];
int current_dir_pointer = 0;
VFS_NODE* current_dir;

int change_directory(char* path);
void list_directory();
void make_directory(char* name);
void touch(char* name);
void remove(char* name);
void cat(char* name);
void add(char* name);
void nano(char* name);

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

void execute_command(){
    kprint("\n");

    char* param;
    int arg_pos = str_until(sh_buffer, ' ');
    if(!arg_pos) param = 0;// kprint("No parameter!");
    else {
        arg_pos++;
        param = &sh_buffer[arg_pos];
    }

    char temp_buffer[512] = "";


    if(!str_cmp_until(sh_buffer, "cd", ' ')) change_directory(param ? param : "");
    else if(!str_cmp_until(sh_buffer, "ls", ' '))
        list_directory();
    else if(!str_cmp_until(sh_buffer, "cat", ' ')) cat(param ? param : "");
    else if(!str_cmp_until(sh_buffer, "mkdir", ' ')) make_directory(param ? param : "");
    else if(!str_cmp_until(sh_buffer, "rm", ' ')) remove(param ? param : "");
    else if(!str_cmp_until(sh_buffer, "touch", ' ')) touch(param ? param : "");
    else if(!str_cmp_until(sh_buffer, "add", ' ')) add(param ? param : "");
    else if(!str_cmp_until(sh_buffer, "nano", ' ')) nano(param ? param : "");

    k_memset(&sh_buffer, 256, 0x00);
    kprint("\n");
    kprint(&current_path[0]);
    kprint("> ");
    sh_buffer_count = 0;
    bExecute = 0;
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
    k_memset(&sh_buffer, 256, 0x00);
    initialise_multitasking();

    VFS_NODE* dir = open_dir("A:/test_fol/test1");

    kprint(&current_path[0]);
    kprint("> ");

    while(1)
        if(bExecute)
            execute_command();
}

int change_directory(char* path){
    VFS_NODE* node = open_dir(path);
}

void list_directory(){

}

void make_directory(char* name) {
}

void touch(char* name){
}

void remove(char* name){
}

void cat(char* name){
}

void add(char* name){
}

void nano(char* name){
}
