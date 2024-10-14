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
int              fat12_write_file (struct VFS_NODE* this, u8* buffer, u32 size, u32 offset);
int              fat12_read_file  (struct VFS_NODE* this, u8* buffer, u32 size, u32 offset);

// Directory own functions
struct VFS_NODE* fat12_list_dir   (struct VFS_NODE* this, u32* size);

struct FAT12_BPB {
    u8  jmp_signature[3];
    u8  oem_identifier[8];
    u16 bytes_per_sector;
    u8  sectors_per_cluster;
    u16 reserved_sectors;
    u8  fats;
    u16 directory_entries;
    u16 sectors_in_volume;
    u8  media_descriptor_type;
    u16 sectors_per_fat;
    u16 sectors_per_track;
    u16 heads_on_media;
    u32 hidden_sectors;
    u32 large_sectors_count;
} __attribute__((packed)) ;

struct FAT12_EBR {
    u8  drive_number;
    u8  windows_reserved;
    u8  signature;
    u8  volume_id[4];
    s8  volume_label_string[11];
    u8  system_identifier_string[8];
    u8  boot_code[448];
    u16 boot_signature;
} __attribute__((packed));

typedef union {
    u8 buffer[512];
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
    s8  filename[11];
    u8  attributes;
    u8  reserved_NT;
    u8  creation_length;
    u16 creation_time;
    u16 creation_date;
    u16 last_accessed_data;
    u16 high_16_first_cluster;
    u16 last_modification_time;
    u16 last_modification_date;
    u16 start_low;
    u32 size;
} __attribute__((packed));

typedef struct {
    u8  sectors_per_cluster;
    u16 reserved_sectors; // FAT beginning sector
    u8  FATs;
    u16 root_entries;
    u32 sectors;
    u16 sectors_per_fat;
    u32 hidden_sectors;

    s8 label[11];

    u32 fat_offset;
    u32 root_offset;
    u32 data_offset;

    u32 root_size;

    struct {
        u8 buffer[0x600]; // 3 disk sectors sized buffer
        u32 current_sector; // Sector of FAT that was currently loaded
        int changed;
    } buffer;
} FAT12_PARTITION;

typedef struct {
    u32 sector;
    u8 offset;
} __attribute__((packed)) FAT_NODE_POSITION;

// Initialization



#endif //FILEOS_FAT12_H
