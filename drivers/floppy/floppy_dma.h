//
// Created by szymon on 04/08/2021.
//

#ifndef FILEOS_FLOPPY_DMA_H
#define FILEOS_FLOPPY_DMA_H

#include "floppy_common.h"
#include "floppy_types.h"

void init_floppy_dma();
int read_floppy_dma(FLOPPY_DEVICE *drive, u8* buffer, u32 sectors, u32 lba);
int write_floppy_dma(FLOPPY_DEVICE *drive, u8* buffer, u32 sectors, u32 lba);

#endif //FILEOS_FLOPPY_DMA_H
