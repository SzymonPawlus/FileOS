#ifndef ATA_TYPES_H_
#define ATA_TYPES_H_

#include "../../cpu/types.h"

enum AtaRegister { // Offsets
    ATA_DATA_REGISTER = 0,
    ATA_ERROR_REGISTER = 1,
    ATA_FEATURES_REGISTER = 1,
    ATA_SECTOR_COUNT_REGISTER = 2,
    ATA_LBALO_REGISTER = 3,
    ATA_LBAMID_REGISTER = 4,
    ATA_LBAHI_REGISTER = 5,
    ATA_DRIVE_REGISTER = 6,
    ATA_STATUS_REGISTER = 7,
    ATA_COMMAND_REGISTER = 7,
};

enum AtaControlRegister {
    ATA_ALTERNATE_STATUS_REGISTER = 0,
    ATA_DEVICE_CONTROL_REGISTER = 0,
    ATA_DRIVE_ADDRESS_REGISTER = 1,
};

enum AtaError {
    E_ATA_ADDRESS_MARK_NOT_FOUND = 0x01,
    E_ATA_TRACK_ZERO_NOT_FOUND = 0x02,
    E_ATA_ABORTED_COMMAND = 0x04,
    E_ATA_MEDIA_CHANGE_REQUEST = 0x08,
    E_ATA_ID_NOT_FOUND = 0x10,
    E_ATA_MEDIA_CHANGED = 0x20,
    E_ATA_UNCORRECTABLE_DATA_ERROR = 0x40,
    E_ATA_BAD_BLOCK_DETECTED = 0x80,
};

enum AtaMode {
    ATA_PIO = 0,
};

enum AtaDrive {
    ATA_MASTER = 0xA0,
    ATA_SLAVE = 0xB0,
};

enum AtaAddressingMode {
    ATA_LBA28,
    ATA_LBA48,
    ATA_CHS,
};

typedef struct {
    // Drive mode
    enum AtaMode default_mode;
    enum AtaAddressingMode addressing_mode;

    // Drive location
    u16 port_base;
    u16 control_base;
    enum AtaDrive drive;

    // Drive info
    u32 addressable_sectors;
} ATA_DEVICE;

#endif // ATA_TYPES_H_
