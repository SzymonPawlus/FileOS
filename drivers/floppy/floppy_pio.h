//
// Created by szymon on 04/08/2021.
//

#ifndef FILEOS_FLOPPY_PIO_H
#define FILEOS_FLOPPY_PIO_H

#include "floppy_common.h"

void init_floppy_pio();
int read_floppy_pio(FLOPPY_DEVICE* drive, uint8_t* buffer, uint32_t sectors, uint32_t lba);
int write_floppy_pio(FLOPPY_DEVICE* drive, uint8_t* buffer, uint32_t sectors, uint32_t lba);

#endif //FILEOS_FLOPPY_PIO_H
