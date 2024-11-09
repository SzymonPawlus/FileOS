#ifndef ATA_H_
#define ATA_H_

#include "ata_types.h"
#include "../ports.h"
#include "../../fs/vfs.h"

int ata_init(enum AtaMode mode);
enum E_DEVICE ata_pio_read(struct VFS_DEVICE* device, void* buffer, uint32_t sectors, uint32_t lba);
enum E_DEVICE ata_pio_write(struct VFS_DEVICE* device, void* buffer, uint32_t sectors, uint32_t lba);


#endif // ATA_H_
