//
// Created by szymon on 10/08/2021.
//

#ifndef FILEOS_VFS_H
#define FILEOS_VFS_H

#include "../cpu/types.h"

///
///  DEFINITIONS
///

typedef struct VFS_DEVICE VFS_DEVICE;
typedef struct VFS_PARTITION VFS_PARTITION;
typedef struct VFS_NODE VFS_NODE;
typedef struct DIR_ENTRY DIR_ENTRY;

///
/// Enums
///

enum PARTITION_FORMAT {
     PARTITION_FORMAT_FAT12 = 0x01,
     PARTITION_FORMAT_FAT16 = 0x02,
     PARTITION_FORMAT_FAT32 = 0x04,
     PARTITION_FORMAT_EXFAT = 0x08,
     PARTITION_FORMAT_EXT2  = 0x10,
};

enum ERROR_DEVICE {
    ED_FLOPPY     = 0x0100,
    ED_FAT        = 0x0200,
    ED_VFS        = 0x0300,
};

enum ERROR_SIZE {
    E_WARNING = 0x0000,
    E_ERROR   = 0x0080
};

enum E_PARTITION {
    E_PARTITION_OK = 0,
    E_PARTITION_DEVICE_FAILED = 1,
    E_PARTITION_NO_FILESYSTEM_DETECTED = 2,
    E_PARTITINO_INVALID_SIGNATURE = 3,
};

enum VFS_ACCESS_MODES {
    READ_ACCESS       = 0x01,
    WRITE_ACCESS      = 0x02,
    READ_WRITE_ACCESS = READ_ACCESS | WRITE_ACCESS,
};

enum NODE_TYPE{
    INVALID = 0,
    DIRECTORY = 1,
    FILE = 2
};

enum E_DEVICE {
    E_DEVICE_OK = 0,
    E_DEVICE_NOT_FOUND = 1,
    E_DEVICE_INCORRECT_DEVICE = 2,
    E_DEVICE_TOO_SMALL_BUFFER = 3,
    E_DEVICE_BAD_SECTOR = 4,
    E_DEVICE_WRITE_FAILED = 5,
    E_DEVICE_READ_FAILED = 6,
    E_DEVICE_NOT_READABLE = 7,
    E_DEVICE_NOT_WRITABLE = 8,
    E_DEVICE_BAD_READ = 9,
    E_DEVICE_BAD_WRITE = 10,
};

enum E_FILE {
    E_FILE_OK = 0,
    E_FILE_NOT_FOUND = 1,
    E_FILE_CORRUPTED = 2,
};


// VFS methods

// Helper functions
struct VFS_NODE vfs_empty_node();
int vfs_is_node_empty(struct VFS_NODE *node);


///
///  TYPES
///

// Device functions
typedef enum E_DEVICE (*READ_DEVICE  )(VFS_DEVICE*, void* buffer, u32 sectors, u32 lba);
typedef enum E_DEVICE (*WRITE_DEVICE )(VFS_DEVICE*, void* buffer, u32 sectors, u32 lba);

// File creation/deletion functions
typedef VFS_NODE* (*OPEN_FILE)  (VFS_PARTITION*,VFS_NODE* dir,
                                        char* path, enum VFS_ACCESS_MODES flags);
typedef int              (*CREATE_FILE)(VFS_PARTITION*, VFS_NODE* dir,
                                        char* path, enum VFS_ACCESS_MODES flags);
typedef int              (*RMV_FILE)   (VFS_PARTITION*, VFS_NODE* dir,
                                        char* path);

//
typedef VFS_NODE* (*OPEN_DIR)   (VFS_PARTITION*, VFS_NODE* dir,
                                        char* path);
typedef int              (*MAKE_DIR)   (VFS_PARTITION*, VFS_NODE* dir,
                                        char* path);
typedef int              (*RMV_DIR)    (VFS_PARTITION*, VFS_NODE* dir,
                                        char* path);

// Node functions (file-only)
typedef int              (*WRITE_FILE) (VFS_NODE* node, void* buffer, u32 size, u32 offset);
typedef int              (*READ_FILE)  (VFS_NODE* node, void* buffer, u32 size, u32 offset);

// Node functions (directory-only)
typedef int              (*LIST_DIR)   (VFS_NODE* node, DIR_ENTRY* buffer, u32* size);

struct VFS_NODE_INFO{
    char name[64];
    u32 size;
    enum VFS_ACCESS_MODES access;
    enum NODE_TYPE type;
};
// 3rd and 4th bytes of error reserved for device number

// Device methods
enum E_DEVICE vfs_device_register(void* device_data, READ_DEVICE, WRITE_DEVICE,
                                VFS_DEVICE** device_descriptor);
enum E_DEVICE vfs_device_read(VFS_DEVICE* device, void* buffer, u32 sectors, u32 lba);
enum E_DEVICE vfs_device_write(VFS_DEVICE* device, void* buffer, u32 sectors, u32 lba);
void*         vfs_device_get_data(VFS_DEVICE* device);

// Partition methods
enum E_PARTITION vfs_partitions_find_on_device(VFS_DEVICE* partition,
                                               enum PARTITION_FORMAT formats,
                                               VFS_PARTITION** partitions,
                                               u32* size);
enum E_PARTITION vfs_register_partition(void* data,
                                        VFS_DEVICE*,
                                        OPEN_FILE, CREATE_FILE,RMV_FILE,
                                        OPEN_DIR, MAKE_DIR, RMV_DIR,
                                        VFS_PARTITION* partition_descriptor);
void*            vfs_partition_get_data(VFS_PARTITION* partition);
char             vfs_partition_get_name(VFS_PARTITION* partition);
VFS_DEVICE*      vfs_partition_get_device(VFS_PARTITION* partition);

// File Methods
enum E_FILE vfs_file_create_descriptor(void* data, VFS_PARTITION* partition,
                            WRITE_FILE, READ_FILE, LIST_DIR, VFS_NODE** file_descriptor);
void*            vfs_node_get_data(VFS_NODE* node);
VFS_PARTITION* vfs_node_get_partition(VFS_NODE* node);
s32 vfs_node_get_offset(VFS_NODE* node);
s32 vfs_node_move_offset(VFS_NODE* node, s32 translation);

// Node interface functions
struct VFS_NODE_INFO info_file(VFS_NODE* file);

// File interface Methods
VFS_NODE* open_file(char* path, enum VFS_ACCESS_MODES flags);
VFS_NODE* open_file_relative(VFS_NODE* dir,
                                    char* relative_path, enum VFS_ACCESS_MODES flags);
int             make_file(char* path, enum VFS_ACCESS_MODES flags);
int             make_file_relative(VFS_NODE* dir,
                                   char* relative_path, enum VFS_ACCESS_MODES flags);
int             rmv_file (char* path);
int             rmv_file_relative (VFS_NODE* dir, char* path);
int             write_file (VFS_NODE* node, void* buffer, u32 size, u32 offset);
int             read_file  (VFS_NODE* node, void* buffer, u32 size, u32 offset);


// Folder intermethods methods
VFS_NODE* open_dir (char* path);
VFS_NODE* open_dir_relative (VFS_NODE* dir, char* path);
int             make_dir (char* path);
int             make_dir_relative (VFS_NODE* dir, char* path);
int             rmv_dir  (char* path);
int             rmv_dir_relative  (VFS_NODE* dir, char* path);
int             list_dir (VFS_NODE* node, DIR_ENTRY* buffer, u32* size);



struct DIR_ENTRY{
    char name[32];
    enum NODE_TYPE type;
};

#endif //FILEOS_VFS_H
