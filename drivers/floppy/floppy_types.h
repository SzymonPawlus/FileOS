//
// Created by szymon on 04/08/2021.
//

#ifndef FILEOS_FLOPPY_TYPES_H
#define FILEOS_FLOPPY_TYPES_H

#include "../../cpu/types.h"

enum FloppyRegisters {
    STATUS_REGISTER_A                = 0x3F0, // read-only
    STATUS_REGISTER_B                = 0x3F1, // read-only
    DIGITAL_OUTPUT_REGISTER          = 0x3F2,
    TAPE_DRIVE_REGISTER              = 0x3F3,
    MAIN_STATUS_REGISTER             = 0x3F4, // read-only
    DATARATE_SELECT_REGISTER         = 0x3F4, // write-only
    DATA_FIFO                        = 0x3F5,
    DIGITAL_INPUT_REGISTER           = 0x3F7, // read-only
    CONFIGURATION_CONTROL_REGISTER   = 0x3F7  // write-only
};

enum FloppyCommands
{
    READ_TRACK =                 2,	// generates IRQ6
    SPECIFY =                    3,      // * set drive parameters
    SENSE_DRIVE_STATUS =         4,
    WRITE_DATA =                 5,      // * write to the disk
    READ_DATA =                  6,      // * read from the disk
    RECALIBRATE =                7,      // * seek to cylinder 0
    SENSE_INTERRUPT =            8,      // * ack IRQ6, get status of last command
    WRITE_DELETED_DATA =         9,
    READ_ID =                    10,	// generates IRQ6
    READ_DELETED_DATA =          12,
    FORMAT_TRACK =               13,     // *
    DUMPREG =                    14,
    SEEK =                       15,     // * seek both heads to cylinder X
    VERSION =                    16,	// * used during initialization, once
    SCAN_EQUAL =                 17,
    PERPENDICULAR_MODE =         18,	// * used during initialization, once, maybe
    CONFIGURE =                  19,     // * set controller parameters
    LOCK =                       20,     // * protect controller params from a reset
    VERIFY =                     22,
    SCAN_LOW_OR_EQUAL =          25,
    SCAN_HIGH_OR_EQUAL =         29
};

enum MSR {
    RQM     = 0x80,
    DIO     = 0x40,
    NDMA    = 0x20,
    CB      = 0x10,
    ACTD    = 0x08,
    ACTC    = 0x04,
    ACTB    = 0x02,
    ACTA    = 0x01,
};

enum DOR {
    MOTD    = 0x80,
    MOTC    = 0x40,
    MOTb    = 0x20,
    MOTA    = 0x10,
    IRQ     = 0x08,
    RESET   = 0x04,
    DSEL3   = 0x03,
    DSEL2   = 0x02,
    DSEL1   = 0x01,
    DSEL0   = 0x00,
};

enum CommandBits {
    MT = 0x80,
    MF = 0x40,
    SK = 0x20,
};

enum FloppyTransmitDirection {
    floppy_dir_read = 1,
    floppy_dir_write = 2,
};

enum FloppyType {
    F_NO_DRIVE = 0,
    F_360KB_5_25 = 1,
    F_1_2MB_5_25 = 2,
    F_720KB_3_5 = 3,
    F_1_44MB_3_5 = 4,
    F_2_88MB_3_5 = 5
};

typedef struct{
    enum FloppyType type;
    uint8_t drive_number;
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors_per_track;
} FLOPPY_DEVICE;

typedef struct DMA {
    uint16_t cylinder, head, sector;
} DMA;

typedef enum {
    Floppy_DMA = 0,
    Floppy_PIO = 1,
} FloppyMode;

enum FloppyErrors {
    E_FLOPPY_NO_ERROR                = 0,
    E_FLOPPY_NO_DRIVE                = 1,
    E_FLOPPY_END_OF_CYLINDER         = 2,
    E_FLOPPY_DRIVE_NOT_READ          = 3,
    E_FLOPPY_CRC_ERROR               = 4,
    E_FLOPPY_CONTROLLER_TIMEOUT      = 5,
    E_FLOPPY_NO_DATA_FOUND           = 6,
    E_FLOPPY_NO_ADDRESS_MARK_FOUND   = 7,
    E_FLOPPY_DELETED_ADDRESS_MARK    = 8,
    E_FLOPPY_CRC_ERROR_IN_DATA       = 9,
    E_FLOPPY_WRONG_CYLINDER          = 10,
    E_FLOPPY_UPD765_SECTOR_NOT_FOUND = 11,
    E_FLOPPY_BAD_CYLINDER            = 12,
    E_FLOPPY_WRONG_SECTOR_SIZE       = 13,
    E_FLOPPY_NOT_WRITABLE            = 14,
    E_FLOPPY_INVALID_DATA_DIRECTION  = 15,
    E_FLOPPY_OLD_DEVICE              = 16,
    E_FLOPPY_CANNOT_LOCK             = 17,
    E_FLOPPY_EXHAUSTED               = 18,
    E_FLOPPY_TIMEOUT                 = 19,
    E_FLOPPY_DMA_BUFFER_BIG          = 20,
    E_FLOPPY_DMA_BUFFER_INVALID_LOC  = 21,
};

#endif //FILEOS_FLOPPY_TYPES_H
