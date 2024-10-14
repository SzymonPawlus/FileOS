//
// Created by szymon on 03/08/2021.
//

//
// Created by szymon on 29/07/2021.
//

#include "floppy.h"
#include "ports.h"
#include "../cpu/timer.h"
#include "../libc/memory.h"
#include "screen.h"

#define FLOPPY_144_SECTORS_PER_TRACK 18

void send_byte(u8 byte);
u8   read_byte();
void configure();
void reset();
int send_command(enum FloppyCommands command, u8* params, int params_size, u8* returns, int returns_size, int(*callback)());
void lba_2_chs(u32 lba, u16* cyl, u16* head, u16* sector);

u8 buf1[512];
u8 buf2[2048];
u32 floppy_buffer_index = 0;
u32 floppy_buffer_size = 0;
u8* floppy_buffer;

int read_callback(){
    if(floppy_buffer_index >= floppy_buffer_size) port_byte_in(DATA_FIFO);
    else floppy_buffer[floppy_buffer_index++] = port_byte_in(DATA_FIFO);
    return 0;
}

int write_callback() {
    if(floppy_buffer_index >= floppy_buffer_size) {
        port_byte_out(DATA_FIFO, 0xff);
        floppy_buffer_index++;
        return 1;

    }
    else port_byte_out(DATA_FIFO, floppy_buffer[floppy_buffer_index++]);
    return 0;
}

void init_floppy(){
    floppy_detect_drives();
    configure();
    // send_command(0xc0 | READ_DATA, read_params, 8, read_returns, 7, read_callback);
    k_memset(&buf1, 512, 0x5a);
    write_floppy((u8*)&buf1, 512, 0);
    // read_floppy((u8*)&buf2, 2048, 0);

}

void read_floppy(u8* buffer, u32 size, u32 lba){
    floppy_buffer_index = 0; floppy_buffer_size = size; floppy_buffer = buffer;

    u16 cyl, head, sector;
    lba_2_chs(lba, &cyl, &head, &sector);


    u8 read_params[8] = {
            (head << 2) | 0x00, // (head << 2) | drive number
            cyl,                // cylinder number
            head,               // head number (again!)
            sector,             // starting sector
            0x02,               // Sector size specification (512 bytes per sector)
            0x12,               // Number of the the last sector on the track 18 = 0x12
            0x1b,               // GAP 1 default size (no idea!)
            0xff                // Sector size specification (512 bytes per sector)
    };
    u8 read_returns[7] = { 0, 0, 0, 0, 0, 0, 0};
    send_command(MT | MF | READ_DATA, read_params, 8, read_returns, 7, read_callback);
}

void write_floppy(u8* buffer, u32 size, u32 lba){
    floppy_buffer_index = 0; floppy_buffer_size = size; floppy_buffer = buffer;

    u16 cyl, head, sector;
    lba_2_chs(lba, &cyl, &head, &sector);


    u8 read_params[8] = {
            (head << 2) | 0x00, // (head << 2) | drive number
            cyl,                // cylinder number
            head,               // head number (again!)
            sector,             // starting sector
            0x02,               // Sector size specification (512 bytes per sector)
            0x12,               // Number of the the last sector on the track 18 = 0x12
            0x1b,               // GAP 1 default size (no idea!)
            0xff                // Sector size specification (512 bytes per sector)
    };
    u8 read_returns[7] = { 0, 0, 0, 0, 0, 0, 0};
    int y = send_command(MT | MF | SK | WRITE_DATA, read_params, 8, read_returns, 7, write_callback);
}

void configure(){
    port_byte_out(DIGITAL_OUTPUT_REGISTER, 0b11110100);

    port_byte_out(DATA_FIFO, VERSION);
    if(port_byte_in(DATA_FIFO) != 0x90) return;

    // TODO - do return check

    u8 conf_params[3] = { 0x00, 0b01010000, 0x00 };
    u8 lock_returns[1];
    send_command(CONFIGURE, (u8*)&conf_params, 3, 0, 0, 0);
    send_command(MT | LOCK, 0, 0, (u8*)&lock_returns, 1, 0);

}

int send_command(enum FloppyCommands command, u8* params, int params_size, u8* returns, int returns_size, int(*callback)()){
    // 1. Read MSR and 2. Check if it's correct
    if((port_byte_in(MAIN_STATUS_REGISTER)& 0xc0) != 0x80) return -1;

    // 3. Write command to FIFO port
    port_byte_out(DATA_FIFO, command);

    // 4. Write all params
    for (int i = 0; i < params_size; ++i) {
        int n = 0;
        volatile int mbr = 0;
        while((mbr & 0xc0) != 0x80 && n < 256){
            n++;
            mbr = port_byte_in(MAIN_STATUS_REGISTER);
        }
        port_byte_out(DATA_FIFO, params[i]);
    }

    // 5. Jump to phase
    // 6. PIO Execution phase
    while((port_byte_in(MAIN_STATUS_REGISTER) & 0x20) == 0x20){
        // 7. Outer loop
        int is_full = 0;
        volatile int mbr = 0;
        // 8. Pool until MBR - RQM = 1
        while((mbr & 0x80) != 0x80){
            mbr = port_byte_in(MAIN_STATUS_REGISTER);
        }
        // 9. Byte transfers by callback
        while((mbr & (RQM | NDMA)) == (RQM | NDMA)){
            is_full = callback();
            mbr = port_byte_in(MAIN_STATUS_REGISTER);
        }
        if(is_full) break;
        // 10. Go back to check condition
    }
    // 11. Result phase
    for (int i = 0; i < returns_size; ++i) {
        int n = 0;
        volatile int mbr = 0;
        while((mbr & 0xc0) != 0xc0 && n < 256){
            n++;
            mbr = port_byte_in(MAIN_STATUS_REGISTER);
        }
        returns[i] = port_byte_in(DATA_FIFO);
    }
    // 12. Check for errors
    if((port_byte_in(MAIN_STATUS_REGISTER) & 0xd0) != 0x80) return -2;
    return 0;
}


