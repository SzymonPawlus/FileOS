#ifndef ATA_H_
#define ATA_H_

#include "ata_types.h"
#include "../ports.h"
#include "../../fs/vfs.h"

int ata_init(enum AtaMode mode);
enum E_DEVICE ata_pio_read(struct VFS_DEVICE* device, void* buffer, u32 sectors, u32 lba);
enum E_DEVICE ata_pio_write(struct VFS_DEVICE* device, void* buffer, u32 sectors, u32 lba);


#endif // ATA_H_
