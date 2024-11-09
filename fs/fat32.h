#ifndef FAT32_H_
#define FAT32_H_

#include "vfs.h"

enum E_PARTITION     fat32_find_partition(VFS_DEVICE *device);

VFS_NODE*            fat32_open_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                     char* path, int flags);
int                  fat32_create_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                       char* path, int flags);
int                  fat32_remove_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                       char* path);

VFS_NODE*            fat32_open_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                    char* path);
int                  fat32_make_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                    char* path);
int                  fat32_remove_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                      char* path);

int                  fat32_write_file (VFS_NODE* node, void* buffer, uint32_t size);
int                  fat32_read_file  (VFS_NODE* node, void* buffer, uint32_t size);
int                  fat32_lseek(VFS_NODE* node, int32_t offset, enum SEEK whence);

int                  fat32_list_dir   (VFS_NODE* node, DIR_ENTRY* buffer, uint32_t size);

struct FAT32_BPB {
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

struct FAT32_EBR {
    uint32_t sectors_per_fat;
    uint16_t flags;
    uint16_t version;
    uint32_t cluster_of_root;
    uint16_t sector_of_FSInfo;
    uint16_t sector_of_backup;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  windows_flags;
    uint8_t  signature;
    uint32_t volume_ID;
    uint8_t  volume_label[11];
    uint8_t  filesystem_ID[8]; // Should be "FAT32   "
    uint8_t  boot_code[420];
    uint16_t boot_signature;
} __attribute__((packed));

struct FAT32_FSINFO {
    uint32_t lead_signature;
    uint8_t  reserved[480];
    uint32_t middle_signature;
    uint32_t free_clusters;
    uint32_t search_start;
    uint8_t  reserved2[12];
    uint32_t trail_signature;
};

typedef union {
    uint8_t buffer[512];
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
    int8_t  filename[11];
    uint8_t  attributes;
    uint8_t  reserved_NT;
    uint8_t  creation_length;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_accessed_data;
    uint16_t start_high;
    uint16_t last_modification_time;
    uint16_t last_modification_date;
    uint16_t start_low;
    uint32_t size;
} __attribute__((packed));

typedef struct  {
    uint32_t descriptor_cluster;
    uint32_t descriptor_offset;
    struct FAT32_NODE node;
    uint32_t parent_cluster;
} FAT32_NODE_INFO;

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
    uint32_t data_offset;
    uint32_t backup_boot_offset;
    uint32_t fsinfo_offset;
    uint32_t root_size;

    uint32_t root_sector;

    uint32_t buffer[0x80]; // 1 disk sectors sized buffer
    uint32_t current_sector; // Sector of FAT that was currently loaded
    int buffer_loaded;
    int buffer_changed;

} FAT32_PARTITION;


typedef struct {
    uint32_t sector;
    uint8_t offset;
} __attribute__((packed)) FAT32_NODE_POSITION;
#endif // FAT32_H_
