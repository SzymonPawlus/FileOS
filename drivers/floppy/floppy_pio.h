//
// Created by szymon on 04/08/2021.
//

#ifndef FILEOS_FLOPPY_PIO_H
#define FILEOS_FLOPPY_PIO_H

#include "floppy_common.h"

void init_floppy_pio();
int read_floppy_pio(FLOPPY_DEVICE* drive, u8* buffer, u32 sectors, u32 lba);
int write_floppy_pio(FLOPPY_DEVICE* drive, u8* buffer, u32 sectors, u32 lba);

#endif //FILEOS_FLOPPY_PIO_H
