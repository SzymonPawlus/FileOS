//
// Created by szymon on 29/07/2021.
//

#ifndef FILEOS_FLOPPY_H
#define FILEOS_FLOPPY_H

#include "../../cpu/types.h"
#include "floppy_types.h"
#include "../../fs/vfs.h"

int floppy_init(FloppyMode mode);

int floppy_read(struct VFS_DEVICE *device, uint8_t *buffer, uint32_t sectors, uint32_t lba);
int floppy_write(struct VFS_DEVICE *device, uint8_t *buffer, uint32_t sectors, uint32_t lba);

#endif //FILEOS_FLOPPY_H
