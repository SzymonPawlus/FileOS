//
// Created by szymon on 04/08/2021.
//

#include "floppy_common.h"
#include "../../fs/vfs.h"

// Digital Output Register
u8 DOR;

int get_MSR(){
    return port_byte_in(MAIN_STATUS_REGISTER);
}

u8 get_FIFO(){
    return port_byte_in(DATA_FIFO);
}

void send_FIFO(u8 byte){
    port_byte_out(DATA_FIFO, byte);
}

int floppy_write_cmd(u8 cmd){
    for (int i = 0; i < 600; ++i)
        if((get_MSR() & RQM)) {
            send_FIFO(cmd);
            return E_FLOPPY_NO_ERROR;
        }
    return E_FLOPPY_TIMEOUT | ED_FLOPPY | E_ERROR;
}

u8 floppy_read_data(){
    for (int i = 0; i < 600; ++i)
        if((get_MSR() & RQM)) return get_FIFO();
    return 0;
}

DMA lba_2_chs(u32 lba, const FLOPPY_DEVICE* drive)
{
    DMA dma = {};
    if(!drive->sectors_per_track) return dma;
    dma.cylinder    = lba / (2 * drive->sectors_per_track);
    dma.head        = ((lba % (2 * drive->sectors_per_track)) / drive->sectors_per_track);
    dma.sector      = ((lba % (2 * drive->sectors_per_track)) % drive->sectors_per_track + 1);
    return dma;
}

void set_floppy_params(enum FloppyType type, u8 drive, FLOPPY_DEVICE *drive_struct) {
    drive_struct->type         = type;
    drive_struct->drive_number = drive;
    drive_struct->heads        = 2;

    switch (type) {
        default:
        case F_NO_DRIVE:
        {
            drive_struct->cylinders         = 0;
            drive_struct->heads             = 0;
            drive_struct->sectors_per_track = 0;
            break;
        }
        case F_360KB_5_25:
        {
            drive_struct->cylinders         = 40;
            drive_struct->sectors_per_track = 9;
            break;
        }
        case F_1_2MB_5_25:
        {
            drive_struct->cylinders         = 80;
            drive_struct->sectors_per_track = 15;
            break;
        }
        case F_720KB_3_5:
        {
            drive_struct->cylinders         = 80;
            drive_struct->sectors_per_track = 9;
            break;
        }
        case F_1_44MB_3_5:
        {
            drive_struct->cylinders         = 80;
            drive_struct->sectors_per_track = 18;
            break;
        }
        case F_2_88MB_3_5:
        {
            drive_struct->cylinders         = 80;
            drive_struct->sectors_per_track = 36;
            break;
        }

    }
}

void floppy_detect_drives(FLOPPY_DEVICE *drives_to_detect) {

    port_byte_out(0x70, 0x10);
    unsigned drives = port_byte_in(0x71);

    set_floppy_params(drives >> 4, 0, &drives_to_detect[0]);
    set_floppy_params(drives & 0xf, 1, &drives_to_detect[1]);
}

// Common floppy functions
void floppy_sense_interrupt(u32 *st0, u32 *cyl){
    floppy_write_cmd(SENSE_INTERRUPT);

    *st0 = floppy_read_data();
    *cyl = floppy_read_data();
}

void floppy_motor(int on, u8 drive) {
    if(on)
        DOR = (DOR & 0x0f) | (0x1 << (drive + 4));
    else
        DOR = (DOR & 0x0f);
    port_byte_out(DIGITAL_OUTPUT_REGISTER, DOR);
}

void set_DOR(u8 data){
    DOR = (DOR & 0xf0) | (data & 0x0f);
    port_byte_out(DIGITAL_OUTPUT_REGISTER, DOR);
}

void select_drive(u8 drive_number){
    DOR = (DOR & 0xfc) | (drive_number & 0x03);
}

int calculate_error(const u8* st0, const u8* st1, const u8* st2, const u8* bps){
    int error = 0;
    if(*st0 & 0xC0)        error = E_FLOPPY_NO_DRIVE                ;
    if(*st1 & 0x80)        error = E_FLOPPY_END_OF_CYLINDER         ;
    if(*st0 & 0x08)        error = E_FLOPPY_DRIVE_NOT_READ          ;
    if(*st1 & 0x20)        error = E_FLOPPY_CRC_ERROR               ;
    if(*st1 & 0x10)        error = E_FLOPPY_CONTROLLER_TIMEOUT      ;
    if(*st1 & 0x04)        error = E_FLOPPY_NO_DATA_FOUND           ;
    if((*st1|*st2) & 0x01) error = E_FLOPPY_NO_ADDRESS_MARK_FOUND   ;
    if(*st2 & 0x40)        error = E_FLOPPY_DELETED_ADDRESS_MARK    ;
    if(*st2 & 0x20)        error = E_FLOPPY_CRC_ERROR_IN_DATA       ;
    if(*st2 & 0x10)        error = E_FLOPPY_WRONG_CYLINDER          ;
    if(*st2 & 0x04)        error = E_FLOPPY_UPD765_SECTOR_NOT_FOUND ;
    if(*st2 & 0x02)        error = E_FLOPPY_BAD_CYLINDER            ;
    if(*bps != 0x2)        error = E_FLOPPY_WRONG_SECTOR_SIZE       ;
    if(*st1 & 0x02)        error = E_FLOPPY_NOT_WRITABLE            ;

    return error;
}

