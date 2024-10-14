//
// Created by szymon on 04/08/2021.
//

#ifndef FILEOS_FLOPPY_COMMON_H
#define FILEOS_FLOPPY_COMMON_H

#include "floppy_types.h"
#include "../ports.h"
#include "../screen.h"
#include "../../cpu/types.h"

// Utility functions
void floppy_detect_drives(FLOPPY_DEVICE *drives_to_detect);

int floppy_write_cmd(u8 cmd);
u8 floppy_read_data();

int get_MSR();
u8 get_FIFO();
void send_FIFO(u8 byte);

DMA lba_2_chs(u32 lba, const FLOPPY_DEVICE* drive);

// Common floppy functions
void floppy_sense_interrupt(u32 *st0, u32 *cyl);

void floppy_motor(int on, u8 drive);
void floppy_motor_kill();

void set_DOR(u8 data);

void select_drive(u8 drive_number);

static const char * drive_types[8] = {
        "none",
        "360kB 5.25\"",
        "1.2MB 5.25\"",
        "720kB 3.5\"",

        "1.44MB 3.5\"",
        "2.88MB 3.5\"",
        "unknown type",
        "unknown type"
};

int calculate_error(const u8* st0, const u8* st1, const u8* st2, const u8* bps);

#endif //FILEOS_FLOPPY_COMMON_H
