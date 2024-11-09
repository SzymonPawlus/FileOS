//
// Created by szymon on 04/08/2021.
//

#ifndef FILEOS_FAT12_H
#define FILEOS_FAT12_H

#include "../cpu/types.h"
#include "vfs.h"

struct VFS_PARTITION fat12_init(struct VFS_DEVICE *device);

// Partition-own functions
struct VFS_NODE fat12_open_file  (struct VFS_PARTITION *this, char *path);
int             fat12_create_file(struct VFS_PARTITION *this, char *path);
int             fat12_remove_file(struct VFS_PARTITION *this, char *path);

struct VFS_NODE fat12_open_dir   (struct VFS_PARTITION *this, char* path);
int             fat12_make_dir   (struct VFS_PARTITION *this, char* path);
int             fat12_remove_dir (struct VFS_PARTITION *this, char* path);

// Partition files/directories functions
// Files own functions
int              fat12_write_file (struct VFS_NODE* this, uint8_t* buffer, uint32_t size, uint32_t offset);
int              fat12_read_file  (struct VFS_NODE* this, uint8_t* buffer, uint32_t size, uint32_t offset);

// Directory own functions
struct VFS_NODE* fat12_list_dir   (struct VFS_NODE* this, uint32_t* size);

struct FAT12_BPB {
    uint8_t  jmp_signature[3];
    uint8_t  oem_identifier[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fats;
    uint16_t directory_entries;
    uint16_t sectors_in_volume;
    uint8_t  media_descriptor_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads_on_media;
    uint32_t hidden_sectors;
    uint32_t large_sectors_count;
} __attribute__((packed)) ;

struct FAT12_EBR {
    uint8_t  drive_number;
    uint8_t  windows_reserved;
    uint8_t  signature;
    uint8_t  volume_id[4];
    int8_t  volume_label_string[11];
    uint8_t  system_identifier_string[8];
    uint8_t  boot_code[448];
    uint16_t boot_signature;
} __attribute__((packed));

typedef union {
    uint8_t buffer[512];
    struct {
        struct FAT12_BPB bpb;
        struct FAT12_EBR ebr;
    };
} FAT12_BOOT_RECORD;

enum FAT_DIR_ATTR {
    FAT_DA_READ_ONLY = 0x01,
    FAT_DA_HIDDEN    = 0x02,
    FAT_DA_SYSTEM    = 0x04,
    FAT_DA_VOLUME_ID = 0x08,
    FAT_DA_DIR       = 0x10,
    FAT_DA_ARCHIVE   = 0x20,
    FAT_DA_LFN       = FAT_DA_READ_ONLY | FAT_DA_HIDDEN | FAT_DA_SYSTEM | FAT_DA_VOLUME_ID
};

struct FAT_NODE {
    int8_t  filename[11];
    uint8_t  attributes;
    uint8_t  reserved_NT;
    uint8_t  creation_length;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_accessed_data;
    uint16_t high_16_first_cluster;
    uint16_t last_modification_time;
    uint16_t last_modification_date;
    uint16_t start_low;
    uint32_t size;
} __attribute__((packed));

typedef struct {
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors; // FAT beginning sector
    uint8_t  FATs;
    uint16_t root_entries;
    uint32_t sectors;
    uint16_t sectors_per_fat;
    uint32_t hidden_sectors;

    int8_t label[11];

    uint32_t fat_offset;
    uint32_t root_offset;
    uint32_t data_offset;

    uint32_t root_size;

    struct {
        uint8_t buffer[0x600]; // 3 disk sectors sized buffer
        uint32_t current_sector; // Sector of FAT that was currently loaded
        int changed;
    } buffer;
} FAT12_PARTITION;

typedef struct {
    uint32_t sector;
    uint8_t offset;
} __attribute__((packed)) FAT_NODE_POSITION;

// Initialization



#endif //FILEOS_FAT12_H
