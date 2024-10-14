#ifndef FAT32_H_
#define FAT32_H_

#include "vfs.h"

enum E_PARTITION     fat32_find_partition(struct VFS_DEVICE *device);

VFS_NODE*            fat32_open_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                     char* path, enum VFS_ACCESS_MODES flags);
int                  fat32_create_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                       char* path, enum VFS_ACCESS_MODES flags);
int                  fat32_remove_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                       char* path);

VFS_NODE*            fat32_open_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                    char* path);
int                  fat32_make_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                    char* path);
int                  fat32_remove_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                      char* path);

int                  fat32_write_file (VFS_NODE* node, void* buffer, u32 size, u32 offset);
int                  fat32_read_file  (VFS_NODE* node, void* buffer, u32 size, u32 offset);

int                  fat32_list_dir   (VFS_NODE* node, DIR_ENTRY* buffer, u32* size);

struct FAT32_BPB {
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

struct FAT32_EBR {
    u32 sectors_per_fat;
    u16 flags;
    u16 version;
    u32 cluster_of_root;
    u16 sector_of_FSInfo;
    u16 sector_of_backup;
    u8  reserved[12];
    u8  drive_number;
    u8  windows_flags;
    u8  signature;
    u32 volume_ID;
    u8  volume_label[11];
    u8  filesystem_ID[8]; // Should be "FAT32   "
    u8  boot_code[420];
    u16 boot_signature;
} __attribute__((packed));

struct FAT32_FSINFO {
    u32 lead_signature;
    u8  reserved[480];
    u32 middle_signature;
    u32 free_clusters;
    u32 search_start;
    u8  reserved2[12];
    u32 trail_signature;
};

typedef union {
    u8 buffer[512];
    struct {
        struct FAT32_BPB bpb;
        struct FAT32_EBR ebr;
    };
} FAT32_BOOT_RECORD;

enum FAT32_DIR_ATTR {
    FAT32_DA_READ_ONLY = 0x01,
    FAT32_DA_HIDDEN    = 0x02,
    FAT32_DA_SYSTEM    = 0x04,
    FAT32_DA_VOLUME_ID = 0x08,
    FAT32_DA_DIR       = 0x10,
    FAT32_DA_ARCHIVE   = 0x20,
    FAT32_DA_LFN       = FAT32_DA_READ_ONLY | FAT32_DA_HIDDEN | FAT32_DA_SYSTEM | FAT32_DA_VOLUME_ID
};

struct FAT32_NODE {
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

struct FAT32_NODE_INFO {
    u32 descriptor_cluster;
    u32 descriptor_offset;
    u32 first_cluster;
};

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
    u32 data_offset;
    u32 backup_boot_offset;
    u32 fsinfo_offset;
    u32 root_size;

    u32 root_sector;

    u32 buffer[0x80]; // 1 disk sectors sized buffer
    u32 current_sector; // Sector of FAT that was currently loaded
    int buffer_loaded;
    int buffer_changed;

} FAT32_PARTITION;


typedef struct {
    u32 sector;
    u8 offset;
} __attribute__((packed)) FAT32_NODE_POSITION;
#endif // FAT32_H_
