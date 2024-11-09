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

int floppy_write_cmd(uint8_t cmd);
uint8_t floppy_read_data();

int get_MSR();
uint8_t get_FIFO();
void send_FIFO(uint8_t byte);

DMA lba_2_chs(uint32_t lba, const FLOPPY_DEVICE* drive);

// Common floppy functions
void floppy_sense_interrupt(uint32_t *st0, uint32_t *cyl);

void floppy_motor(int on, uint8_t drive);
void floppy_motor_kill();

void set_DOR(uint8_t data);

void select_drive(uint8_t drive_number);

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

int calculate_error(const uint8_t* st0, const uint8_t* st1, const uint8_t* st2, const uint8_t* bps);

#endif //FILEOS_FLOPPY_COMMON_H
