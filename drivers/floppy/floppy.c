//
// Created by szymon on 29/07/2021.
//

#include <stddef.h>
#include "floppy.h"
#include "../ports.h"
#include "../../cpu/timer.h"
#include "../../libc/memory.h"

#include "../screen.h"
#include "floppy_dma.h"
#include "floppy_pio.h"

struct FloppyModeFunctions {
    int (*read)(FLOPPY_DEVICE*, u8*, u32, u32);
    int (*write)(FLOPPY_DEVICE*, u8*, u32, u32);
};

// Floppies structs
static FLOPPY_DEVICE floppies[2];
static u8 current_drive = 0;
static struct FloppyModeFunctions current_mode;


int floppy_init(FloppyMode mode) {
    int error = 0;
    floppy_write_cmd(PERPENDICULAR_MODE);
    floppy_write_cmd(0b11 << 2);

    // Detect Version
    floppy_write_cmd(VERSION);
    u8 version = floppy_read_data();
    if(version != 0x90) error |= E_FLOPPY_OLD_DEVICE | ED_FLOPPY;

    // Set configuration
    floppy_write_cmd(CONFIGURE);
    floppy_write_cmd(0x00);
    floppy_write_cmd(0b01010010);
    floppy_write_cmd(0x00);

    // Lock config
    floppy_write_cmd(MT | LOCK);
    if(floppy_read_data() != 0x10) error |= E_FLOPPY_CANNOT_LOCK | ED_FLOPPY;

    // Init version
    switch (mode) {
        case Floppy_DMA:
            init_floppy_dma();
            current_mode.write = &write_floppy_dma;
            current_mode.read  = &read_floppy_dma;
            break;
        case Floppy_PIO:
            init_floppy_pio();
            current_mode.write = &write_floppy_pio;
            current_mode.read  = &read_floppy_pio;
            break;
        default:
            break;
    }

    floppy_detect_drives(&floppies[0]);
    return error;
}

int floppy_read(struct VFS_DEVICE *device, u8 *buffer, u32 sectors, u32 lba) {
}

int floppy_write(struct VFS_DEVICE *device, u8 *buffer, u32 sectors, u32 lba) {
}
