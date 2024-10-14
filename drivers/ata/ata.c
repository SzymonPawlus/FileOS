#include "ata.h"
#include "ata_types.h"

static ATA_DEVICE devices[2]; // There can be up to 2 drives on one line

static const u32 PORT_BASE = 0x1F0;
static const u32 CONTROL_BASE = 0x3F6;

static int identify_ata(ATA_DEVICE* device, u16 port_base, u16 control_base, enum AtaDrive drive);

static void ata_write_bytes(ATA_DEVICE* device, u8* buffer, u32 size);

static void ata_read_bytes(ATA_DEVICE* device, u8* buffer, u32 size);
static void ata_read_words(ATA_DEVICE* device, u16* buffer, u32 size);

static u16 ata_read_reg(ATA_DEVICE* device, enum AtaRegister reg);
static void ata_write_reg(ATA_DEVICE* device, enum AtaRegister reg, u16 val);

static u16 ata_read_control_reg(ATA_DEVICE* device, enum AtaControlRegister reg);
static void ata_write_control_reg(ATA_DEVICE* device, enum AtaControlRegister reg, u8 val);

static u16 ata_read_data(ATA_DEVICE* device);
static void ata_write_data(ATA_DEVICE* device, u16 data);

int ata_init(enum AtaMode mode){
    VFS_DEVICE* device;
    VFS_PARTITION* _partitions;
    u32 _size;

    int d1 = identify_ata(&devices[0], PORT_BASE, CONTROL_BASE, ATA_MASTER);
    if(!d1)
        vfs_device_register(&devices[0], ata_pio_read, ata_pio_write, &device);
    vfs_partitions_find_on_device(device, PARTITION_FORMAT_FAT32, &_partitions, &_size);

    int d2 = identify_ata(&devices[1], PORT_BASE, CONTROL_BASE, ATA_SLAVE);
    if(!d2)
        vfs_device_register(&devices[1], ata_pio_read, ata_pio_write, &device);
    vfs_partitions_find_on_device(device, PARTITION_FORMAT_FAT32, &_partitions, &_size);

    return d1 && d2;
}

enum E_DEVICE ata_pio_read(struct VFS_DEVICE* device, void* buffer, u32 sectors, u32 lba){
    ATA_DEVICE* dev = vfs_device_get_data(device);

    u32 amount = sectors / 256;
    u32 reminder = sectors % 256;
    for (int k = 0; k <= amount; k++) {
        // Send LBA
        u16 slave_bit = (dev->drive & 0x10);
        ata_write_reg(dev, ATA_DRIVE_REGISTER,(0xE0 | slave_bit) | ((lba >> 24) & 0x0F));
        if(k < amount){
            ata_write_reg(dev, ATA_SECTOR_COUNT_REGISTER, 0);
        }else{
            if(reminder == 0) return 0;
            ata_write_reg(dev, ATA_SECTOR_COUNT_REGISTER, reminder);
        }
        ata_write_reg(dev, ATA_LBALO_REGISTER, (u8)lba);
        ata_write_reg(dev, ATA_LBAMID_REGISTER, (u8)(lba >> 8));
        ata_write_reg(dev, ATA_LBAHI_REGISTER, (u8)(lba >> 16));

        // READ SECTORS command
        ata_write_reg(dev, ATA_COMMAND_REGISTER, 0x20);

        for (int i = 0; i < (sectors & 0xFF); i++) {
            u16 polling = ata_read_reg(dev, ATA_STATUS_REGISTER);
            while((polling & 0x80) && !(polling & 0x20) && !(polling & 0x01))
                polling = ata_read_reg(dev, ATA_STATUS_REGISTER);
            if(polling & 0x21)
                return 1;
            ata_read_bytes(dev, buffer, 512);
            buffer += 512;

            // Wait for around 400 ms
            for (int j = 0; j < 15; j++)
                ata_read_reg(dev, ATA_STATUS_REGISTER);
        }
    }

    return 0;
}

enum E_DEVICE ata_pio_write(struct VFS_DEVICE* device, void* buffer, u32 sectors, u32 lba){
    ATA_DEVICE* dev = vfs_device_get_data(device);

    u32 amount = sectors / 256;
    u32 reminder = sectors % 256;
    for (int k = 0; k <= amount; k++) {
        // Send LBA
        u16 slave_bit = (dev->drive & 0x10);
        ata_write_reg(dev, ATA_DRIVE_REGISTER,(0xE0 | slave_bit) | ((lba >> 24) & 0x0F));
        if(k < amount){
            ata_write_reg(dev, ATA_SECTOR_COUNT_REGISTER, 0);
        }else{
            if(reminder == 0) return 0;
            ata_write_reg(dev, ATA_SECTOR_COUNT_REGISTER, reminder);
        }
        ata_write_reg(dev, ATA_LBALO_REGISTER, (u8)lba);
        ata_write_reg(dev, ATA_LBAMID_REGISTER, (u8)(lba >> 8));
        ata_write_reg(dev, ATA_LBAHI_REGISTER, (u8)(lba >> 16));

        // WRITE SECTORS command
        ata_write_reg(dev, ATA_COMMAND_REGISTER, 0x30);

        for (int i = 0; i < (sectors & 0xFF); i++) {
            u16 polling = ata_read_reg(dev, ATA_STATUS_REGISTER);
            while((polling & 0x80) && !(polling & 0x20) && !(polling & 0x01))
                polling = ata_read_reg(dev, ATA_STATUS_REGISTER);
            if(polling & 0x21)
                return 1;
            ata_write_bytes(dev, buffer, 512);
            buffer+=512;

            // Wait for around 400 ms
            for (int j = 0; j < 15; j++)
                ata_read_reg(dev, ATA_STATUS_REGISTER);
        }
    }
    return 0;
}

static int identify_ata(ATA_DEVICE* device, u16 port_base, u16 control_base, enum AtaDrive drive){
    int error = 0;

    // Initialize device data
    device->port_base = port_base;
    device->control_base = control_base;
    device->drive = drive;

    device->addressing_mode = ATA_LBA28;
    device->default_mode = ATA_PIO;

    // Check for floating bus
    u16 float_bus = ata_read_reg(device, ATA_STATUS_REGISTER);
    if(float_bus == 0xFF)
        return 1;

    // Issue command
    ata_write_reg(device, ATA_DRIVE_REGISTER, device->drive);
    ata_write_reg(device, ATA_LBALO_REGISTER, 0);
    ata_write_reg(device, ATA_LBAMID_REGISTER, 0);
    ata_write_reg(device, ATA_LBAHI_REGISTER, 0);
    ata_write_reg(device, ATA_COMMAND_REGISTER, 0xEC); // TODO: Add enums for commands

    // Check status register
    u16 status = ata_read_reg(device, ATA_STATUS_REGISTER);
    if(!status)
        return 4;

    // Wait for busy flag to clear
    u16 polling_value = ata_read_reg(device, ATA_STATUS_REGISTER);
    while (polling_value & 0x80)
        polling_value = ata_read_reg(device, ATA_STATUS_REGISTER);

    // Check for different types of ATA
    u16 lba_mid = ata_read_reg(device, ATA_LBAMID_REGISTER);
    u16 lba_high = ata_read_reg(device, ATA_LBAHI_REGISTER);
    if(lba_mid || lba_high) // TODO: differentiante between different ATA devices
        return 2;

    // Check for errors
    while (!(polling_value & 0x08) && !(polling_value & 0x01))
        polling_value = ata_read_reg(device, ATA_STATUS_REGISTER);
    if(polling_value & 0x01)
        return 3;

    // Read disk description data
    u16 buffer[256];
    ata_read_words(device, buffer, 256);

    // TODO: support for 48-bit LBA
    u32 low = buffer[60];
    u32 high = buffer[61] << 16; // TODO: check if this is correct conversion
    device->addressable_sectors = low | high;

    return 0;
}

static void ata_write_bytes(ATA_DEVICE* device, u8* buffer, u32 size){
    for (u32 i = 0; i < size; i+=2){
        u16 tmp = ((u16)(buffer[i + 1]) << 8) | ((u16)buffer[i]);
        ata_write_data(device, tmp);
    }
}

static void ata_read_bytes(ATA_DEVICE* device, u8* buffer, u32 size){
    for (u32 i = 0; i < size; i+=2){
        u16 tmp = ata_read_data(device);
        buffer[i + 1] = ((tmp & 0xFF00) >> 8);
        buffer[i] = (u8)tmp;
    }
}

static void ata_read_words(ATA_DEVICE* device, u16* buffer, u32 size){
    for (u32 i = 0; i < size; i++)
        *(buffer++) = ata_read_data(device);
}

static u16 ata_read_reg(ATA_DEVICE* device, enum AtaRegister reg){
    u16 value;
    if(device->addressing_mode == ATA_LBA28 || reg > 5){
        u8 tmp = port_byte_in(device->port_base + reg);
        value = (u16)tmp;
    }
    else{
        value = port_word_in(device->port_base + reg);
    }
    return value;
}

// If send 8 bytes than send lower byte
static void ata_write_reg(ATA_DEVICE* device, enum AtaRegister reg, u16 val){
    if(device->addressing_mode == ATA_LBA28 || reg > 5){
        u8 tmp = (u8)val;
        port_byte_out(device->port_base + reg, tmp);
    }
    else{
        port_word_out(device->port_base + reg, val);
    }
}

static void ata_write_control_reg(ATA_DEVICE* device, enum AtaControlRegister reg, u8 val){
    port_byte_out(device->control_base + reg, val);
}

static u16 ata_read_control_reg(ATA_DEVICE* device, enum AtaControlRegister reg){
    return port_byte_in(device->control_base + reg);
}


static u16 ata_read_data(ATA_DEVICE* device){
    return port_word_in(device->port_base + ATA_DATA_REGISTER);
}

static void ata_write_data(ATA_DEVICE* device, u16 data){
    port_word_out(device->port_base + ATA_DATA_REGISTER, data);
}
