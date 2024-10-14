//
// Created by szymon on 10/08/2021.
//

#include "vfs.h"
#include "../libc/strings.h"
#include "../libc/memory.h"
#include "fat32.h"

///
/// Type declarations
///

struct VFS_DEVICE {
    // Implementation dependent
    void* device_data;

    // Methods
    READ_DEVICE  read;
    WRITE_DEVICE write;
};

struct VFS_PARTITION {
    // High level data
    char letter;

    // Implementation dependent
    void* partition_data;

    // References
    struct VFS_DEVICE* device;

    // Methods
    OPEN_FILE   open_file;
    CREATE_FILE create_file;
    RMV_FILE    remove_file;

    OPEN_DIR    open_dir;
    MAKE_DIR    make_dir;
    RMV_DIR     remove_dir;
};

struct VFS_NODE {
    // High level info
    struct VFS_NODE_INFO info;

    // Implementation dependent info
    void* node_data;

    // References
    struct VFS_PARTITION* partition;

    // File methods
    WRITE_FILE  write_file;
    READ_FILE   read_file;

    // Folder methods
    LIST_DIR    list_dir;

    // File descriptor info
    s32 offset;
};
///
/// Variables
///

static struct VFS_PARTITION vfs_part;
static struct VFS_DEVICE virtual_devices[16];
static int virtual_devices_count = 0;

static struct VFS_PARTITION partitions[16];
static int partitions_count = 0;

///
/// Static declarations
///

static struct VFS_PARTITION* get_partition_from_path(const char* path);
static char*                 get_path_relative_to_root(char* path);

///
/// Virtual Device I/O methods
///

enum E_DEVICE vfs_device_register(void *device_data, READ_DEVICE read, WRITE_DEVICE write, VFS_DEVICE** dd){
    if(!device_data)
        return E_DEVICE_NOT_FOUND;
    if(!read && !write)
        return E_DEVICE_INCORRECT_DEVICE;
    struct VFS_DEVICE new_device = {
        .device_data = device_data,
        .read = read,
        .write = write,
    };
    virtual_devices[virtual_devices_count] = new_device;
    *dd = virtual_devices + virtual_devices_count;
    virtual_devices_count++;
    return E_DEVICE_OK;
}

enum E_DEVICE vfs_device_read(struct VFS_DEVICE* device, void* buffer, u32 sectors, u32 lba){
    if(!device)
        return E_DEVICE_NOT_FOUND;
    if(!device->read)
        return E_DEVICE_NOT_READABLE;
    if(!buffer)
        return E_DEVICE_TOO_SMALL_BUFFER;
    if(sectors == 0)
        return E_DEVICE_BAD_READ;
    return device->read(device, buffer, sectors, lba);
}

enum E_DEVICE vfs_device_write(struct VFS_DEVICE* device, void* buffer, u32 sectors, u32 lba){
    if(!device)
        return E_DEVICE_NOT_FOUND;
    if(!device->write)
        return E_DEVICE_NOT_WRITABLE;
    if(!buffer)
        return E_DEVICE_TOO_SMALL_BUFFER;
    if(sectors == 0)
        return E_DEVICE_BAD_WRITE;
    return device->write(device, buffer, sectors, lba);
}

void*         vfs_device_get_data(VFS_DEVICE* device){
    if(!device)
        return 0;
    return device->device_data;
}

///
/// Partitions
///

enum E_PARTITION vfs_partitions_find_on_device(struct VFS_DEVICE* dev, enum PARTITION_FORMAT format,
                                               struct VFS_PARTITION** parts, u32* size){
    int current_partitions_count = partitions_count;
    if(format & PARTITION_FORMAT_FAT12)
        ;
    if(format & PARTITION_FORMAT_FAT16)
        ;
    if(format & PARTITION_FORMAT_FAT32)
        fat32_find_partition(dev);
    if(format & PARTITION_FORMAT_EXFAT)
        ;
    if(format & PARTITION_FORMAT_EXT2)
        ;
    *parts = &partitions[partitions_count];
    *size = partitions_count - current_partitions_count;
    return *size ? E_PARTITION_OK : E_PARTITION_NO_FILESYSTEM_DETECTED;
}

enum E_PARTITION vfs_register_partition(void* data, struct VFS_DEVICE* dev, OPEN_FILE open, CREATE_FILE crat,
                                        RMV_FILE rm, OPEN_DIR open_dir, MAKE_DIR mkdir, RMV_DIR rmdir,
                                        struct VFS_PARTITION* p){
    if(!data || !dev || !open || !crat || !rm || !open_dir || !mkdir || !rmdir)
        return E_PARTITION_NO_FILESYSTEM_DETECTED;
    struct VFS_PARTITION tmp_partition = {
         .partition_data = data,
         .device = dev,
         .open_file = open,
         .create_file = crat,
         .remove_file = rm,
         .open_dir = open_dir,
         .make_dir = mkdir,
         .remove_dir = rmdir,
         .letter = 'A' + partitions_count,
    };
    partitions[partitions_count] = tmp_partition;
    p = &partitions[partitions_count];
    partitions_count++;
    return E_PARTITION_OK;

}

void*            vfs_partition_get_data(struct VFS_PARTITION* partition){
    if(!partition)
        return 0;
    return partition->partition_data;
}

char             vfs_partition_get_name(struct VFS_PARTITION* partition){
    if(!partition)
        return '\0';
    return partition->letter;
}

VFS_DEVICE*      vfs_partition_get_device(VFS_PARTITION* partition){
    if(!partition)
        return 0;
    return partition->device;
}

enum E_FILE vfs_file_create_descriptor(void* data, struct VFS_PARTITION* partition,
                            WRITE_FILE write, READ_FILE read, LIST_DIR list,
                            struct VFS_NODE** file_descriptor){
    if(!data || (!write && !read && !list))
        return E_FILE_NOT_FOUND;
    *file_descriptor = k_malloc(sizeof(struct VFS_NODE));
    (*file_descriptor)->node_data = data;
    (*file_descriptor)->read_file = read;
    (*file_descriptor)->partition = partition;
    (*file_descriptor)->write_file = write;
    (*file_descriptor)->list_dir = list;
    return E_FILE_OK;
}
void* vfs_node_get_data(struct VFS_NODE* node){
    if(!node)
        return 0;
    return node->node_data;
}
struct VFS_PARTITION* vfs_node_get_partition(struct VFS_NODE* node){
    if(!node)
        return 0;
    return node->partition;
}

s32 vfs_node_get_offset(VFS_NODE* node){
    if(!node)
        return -1;
    return node->offset;
}
s32 vfs_node_move_offset(VFS_NODE* node, s32 translation){
    if(!node)
        return -1;
    node->offset += translation;
    return node->offset;
}

// Node functions
struct VFS_NODE_INFO info_file(struct VFS_NODE* file){
    struct VFS_NODE_INFO info = {};
    if(!file)
        return info;
    info = file->info;
    return info;
}

// File Methods
struct VFS_NODE* open_file(char* path, enum VFS_ACCESS_MODES flags){
    struct VFS_PARTITION* partition = get_partition_from_path(path);
    if(!partition)
        return 0;
    char* path_from_root = get_path_relative_to_root(path);
    if(!path_from_root)
        return 0;
    struct VFS_NODE* file_descriptor = partition->open_file(partition, 0, path_from_root, flags);
    return file_descriptor;
}

struct VFS_NODE* open_file_relative(struct VFS_NODE* dir,
                                    char* relative_path, enum VFS_ACCESS_MODES flags)
{
    if(!dir)
        return 0;
    struct VFS_PARTITION* partition = vfs_node_get_partition(dir);
    return partition->open_file(partition, dir, relative_path, flags);
}

int             make_file(char* path, enum VFS_ACCESS_MODES flags){
    struct VFS_PARTITION* partition = get_partition_from_path(path);
    if(!partition)
        return -1;
    char* path_from_root = get_path_relative_to_root(path);
    if(!path_from_root)
        return -2;
    return partition->create_file(partition, 0, path_from_root, flags);
}

int             make_file_relative(struct VFS_NODE* dir,
                                   char* relative_path, enum VFS_ACCESS_MODES flags){
    if(!dir)
        return -1;
    struct VFS_PARTITION* partition = vfs_node_get_partition(dir);
    return partition->create_file(partition, dir, relative_path, flags);
}

int             rmv_file (char* path){
    struct VFS_PARTITION* partition = get_partition_from_path(path);
    if(!partition)
        return -1;
    char* path_from_root = get_path_relative_to_root(path);
    if(!path_from_root)
        return -2;
    return partition->remove_file(partition, 0, path_from_root);
}

int             rmv_file_relative (struct VFS_NODE* dir, char* relative_path){
    if(!dir)
        return -1;
    struct VFS_PARTITION* partition = vfs_node_get_partition(dir);
    return partition->remove_file(partition, dir, relative_path);
}

int             write_file (struct VFS_NODE* node, void* buffer, u32 size, u32 offset){
    if(!node)
        return -1;
    if(!node->write_file)
        return -2;
    return node->write_file(node, buffer, size, offset);
}

int             read_file  (struct VFS_NODE* node, void* buffer, u32 size, u32 offset){
    if(!node)
        return -1;
    if(!node->read_file)
        return -2;
    return node->read_file(node, buffer, size, offset);
}


// Folder methods
struct VFS_NODE* open_dir (char* path){
    struct VFS_PARTITION* partition = get_partition_from_path(path);
    if(!partition)
        return 0;
    char* path_from_root = get_path_relative_to_root(path);
    if(!path_from_root)
        return 0;
    return partition->open_dir(partition, 0, path_from_root);
}

struct VFS_NODE* open_dir_relative (struct VFS_NODE* dir, char* relative_path){
    if(!dir)
        return 0;
    struct VFS_PARTITION* partition = vfs_node_get_partition(dir);
    return partition->open_dir(partition, dir, relative_path);
}

int             make_dir (char* path){
    struct VFS_PARTITION* partition = get_partition_from_path(path);
    if(!partition)
        return -1;
    char* path_from_root = get_path_relative_to_root(path);
    if(!path_from_root)
        return -2;
    return partition->make_dir(partition, 0, path_from_root);

}

int             make_dir_relative (struct VFS_NODE* dir, char* relative_path){
    if(!dir)
        return -1;
    struct VFS_PARTITION* partition = vfs_node_get_partition(dir);
    return partition->make_dir(partition, dir, relative_path);
}

int             rmv_dir  (char* path){
    struct VFS_PARTITION* partition = get_partition_from_path(path);
    if(!partition)
        return -1;
    char* path_from_root = get_path_relative_to_root(path);
    if(!path_from_root)
        return -2;
    return partition->remove_dir(partition, 0, path_from_root);
}

int             rmv_dir_relative  (struct VFS_NODE* dir, char* relative_path){
    if(!dir)
        return -1;
    struct VFS_PARTITION* partition = vfs_node_get_partition(dir);
    return partition->remove_dir(partition, dir, relative_path);

}

int             list_dir (struct VFS_NODE* node, struct DIR_ENTRY* buffer, u32* size){
    if(!node)
        return -1;
    return node->list_dir(node, buffer, size);
}

static struct VFS_PARTITION* get_partition_from_path(const char* path){
    if(*path < 'A' || *path > 'Z')
        return 0;
    if(partitions[*path - 'A'].letter == '\0')
        return 0;
    return &partitions[*path - 'A'];
}

static char* get_path_relative_to_root(char* path){
    int result = str_until(path, '/');
    if(!result)
        return 0;
    return path + result;
}
