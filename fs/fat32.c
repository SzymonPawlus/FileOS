#include "fat32.h"
#include "../libc/memory.h"
#include "vfs.h"
#include "../kernel/util.h"
#include "../libc/strings.h"

typedef int (*CLUSTER_ACTION)(struct VFS_PARTITION* partition, u32 cluster, void* buffer, u32 size, u32 offset);

static FAT32_PARTITION* init_partition(FAT32_BOOT_RECORD* info);
static void* traverse_fat(struct VFS_PARTITION* partition, u32* clusters, u32 starting_cluster);
static u32 traverse_fat_coroutine(struct VFS_PARTITION* partition, void* buffer, u32 cluster,
                                  u32 size, u32 offset,
                                  CLUSTER_ACTION action);
static u32 get_cluster_from_node(FAT32_PARTITION* partition ,struct FAT32_NODE* node);
static int cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer);
static int write_cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer);
static void make_8point3_name(char* filename, u32 filename_length, char* buffer);
static struct FAT32_NODE* resolve_path(char* path, VFS_PARTITION* partition, VFS_NODE* node);
static int read_part_cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer, u32 size, u32 offset);
static int write_part_cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer, u32 size, u32 offset);
static int idle_cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer, u32 size, u32 offset);
static u32 allocate_cluster(VFS_PARTITION* partition, u32 old_cluster);

enum E_PARTITION fat32_find_partition(struct VFS_DEVICE *device){

    FAT32_BOOT_RECORD* info = k_malloc(512);

    if(vfs_device_read(device, &info->buffer[0], 1, 0) != E_DEVICE_OK)
        return E_PARTITION_DEVICE_FAILED;
    else if(info->ebr.signature != 0x28 && info->ebr.signature != 0x29)
        return E_PARTITION_NO_FILESYSTEM_DETECTED;
    else if(info->ebr.boot_signature != 0xAA55)
        return E_PARTITINO_INVALID_SIGNATURE;

    struct FAT32_FSINFO* fs_info = k_malloc(512);
    vfs_device_read(device, fs_info, 1, info->ebr.sector_of_FSInfo);
    FAT32_PARTITION* fat_partition = init_partition(info);
    VFS_PARTITION* _;
    vfs_register_partition(fat_partition, device, fat32_open_file, fat32_create_file,
                           fat32_remove_file, fat32_open_dir, fat32_make_dir, fat32_remove_dir, _);
    k_free(info);
    k_free(fs_info);
    return E_PARTITION_OK;
}

static FAT32_PARTITION* init_partition(FAT32_BOOT_RECORD* info){
    FAT32_PARTITION* partition = k_malloc(sizeof(FAT32_PARTITION));

    partition->sectors_per_cluster = info->bpb.sectors_per_cluster;
    partition->reserved_sectors = info->bpb.reserved_sectors;
    partition->FATs = info->bpb.fats;
    partition->root_entries = info->bpb.directory_entries;
    partition->sectors = info->bpb.sectors_in_volume < 0xFFFF ?
        info->bpb.sectors_in_volume : info->bpb.large_sectors_count;
    partition->sectors_per_fat = info->ebr.sectors_per_fat;
    partition->hidden_sectors = info->bpb.hidden_sectors;

    partition->backup_boot_offset = info->ebr.sector_of_backup;
    partition->fsinfo_offset = info->ebr.sector_of_FSInfo;
    partition->fat_offset = info->bpb.reserved_sectors;
    partition->root_size = ((partition->root_entries * 32) + 511) / 512;
    partition->data_offset = partition->reserved_sectors + partition->FATs * partition->sectors_per_fat;
    partition->root_sector = info->ebr.cluster_of_root;
    partition->buffer_loaded = 0;
    partition->buffer_changed = 0;

    k_memcpy(&info->ebr.volume_label, &partition->label, 11);

    return partition;
}


VFS_NODE*            fat32_open_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                     char* path, enum VFS_ACCESS_MODES flags){
    struct FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    struct FAT32_NODE* node = resolve_path(path, partition, dir);
    if(!node)
        return 0;
    VFS_NODE* file_descriptor;
    vfs_file_create_descriptor(node, partition, fat32_write_file, fat32_read_file, fat32_list_dir, &file_descriptor);

    return file_descriptor;
}

int                  fat32_create_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                       char* path, enum VFS_ACCESS_MODES flags){
    return 1;
}

int                  fat32_remove_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                       char* path){
    return 1;
}

VFS_NODE*            fat32_open_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                    char* path){
    struct FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    struct FAT32_NODE* node = resolve_path(path, partition, dir);
    if(!node)
        return 0;
    VFS_NODE* file_descriptor;
    vfs_file_create_descriptor(node, partition, fat32_write_file, fat32_read_file, fat32_list_dir, &file_descriptor);

    return file_descriptor;
}

int                  fat32_make_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                    char* path){
    return 1;
}

int                  fat32_remove_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                      char* path){
    return 1;
}

static inline u32 max(u32 a, u32 b){
    return (a > b) ? a : b;
}

static inline u32 min(u32 a, u32 b){
    return (a > b) ? b : a;
}

int                  fat32_write_file (VFS_NODE* node, void* buffer, u32 size, u32 offset){
    VFS_PARTITION* partition = vfs_node_get_partition(node);
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    struct FAT32_NODE* fat_node = vfs_node_get_data(node);
    u32 current_offset = 0;
    u32 current_cluster = get_cluster_from_node(fat_partition, fat_node);
    while(size > 0){
        // Calculate parameters for righting to segment;
        u32 left = max(offset, current_offset);
        u32 right = min(offset + size, current_offset + 512);
        if(left > right)
            left = right;
        u32 part_size = right - left;

        // Write to the segment
        u32 new_cluster = traverse_fat_coroutine(partition, buffer, current_cluster, part_size, left, write_part_cluster);

        // Change info about current stuff
        size -= part_size;
        offset += part_size;
        current_offset += 512;

        // If we came to the end of allocated cluster we need a new one
        if(new_cluster == 0 && size > 0){
            new_cluster = allocate_cluster(partition, current_cluster);
        }
        current_cluster = new_cluster;
    }
    // I still have to return value but I'll think this through after implementing everything
}

int                  fat32_read_file  (VFS_NODE* node, void* buffer, u32 size, u32 offset){
    VFS_PARTITION* partition = vfs_node_get_partition(node);
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    struct FAT32_NODE* fat_node = vfs_node_get_data(node);
    u32 current_cluster = get_cluster_from_node(fat_partition, fat_node);
    u32 current_offset  = 0;

}

int                  fat32_list_dir   (VFS_NODE* node, DIR_ENTRY* buffer, u32* size){
    return 1;

}

///
/// Static helper functions
///

static inline FAT32_NODE_POSITION calculate_fat_position(FAT32_PARTITION *p, u32 cluster){
    FAT32_NODE_POSITION pos = {
         .sector = cluster / 128,
         .offset = (cluster % 128),
    };
    return pos;
}

static int save_current_fat(struct VFS_PARTITION* p){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(p);

    // If buffer didn't change or is not loaded no point in saving it
    if(!fat_partition->buffer_changed || !fat_partition->buffer_loaded) return 0;

    // Otherwise write changed fat to the disk
    VFS_DEVICE* device = vfs_partition_get_device(p);
    int result = vfs_device_write(device, fat_partition->buffer, 1, fat_partition->current_sector + fat_partition->fat_offset);

    // Unset dirty bit
    fat_partition->buffer_changed = 0;
    return result;
}

static int load_fat_sector(struct VFS_PARTITION* p, u32 sector){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(p);
    // No need to load if it's here
    if(fat_partition->current_sector == sector && fat_partition->buffer_loaded) return 0;

    // Save previous fat before doing anything stupid
    int result = save_current_fat(p);
    if(result != E_DEVICE_OK) return result;

    // Read new sector to the cache buffer
    VFS_DEVICE* device = vfs_partition_get_device(p);
    result = vfs_device_read(device, fat_partition->buffer, 1, sector + fat_partition->fat_offset);
    if(result == E_DEVICE_OK){
        fat_partition->buffer_loaded = 1;
        fat_partition->current_sector = sector;
    }
    return result;
}

static u32 get_next_cluster(struct VFS_PARTITION* p, u32 cluster){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(p);
    VFS_DEVICE* device = vfs_partition_get_device(p);

    // Calculate position
    FAT32_NODE_POSITION pos = calculate_fat_position(fat_partition, cluster);
    load_fat_sector(p, pos.sector);

    u32 next_cluster = fat_partition->buffer[pos.offset];
    return next_cluster;
}

static int change_fat(VFS_PARTITION* partition, u32 end_of_chain_cluster, u32 new_cluster){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);

    // Calculate positions
    FAT32_NODE_POSITION old_pos = calculate_fat_position(fat_partition, end_of_chain_cluster);
    FAT32_NODE_POSITION new_pos = calculate_fat_position(fat_partition, new_cluster);

    // Write new values
    load_fat_sector(partition, old_pos.sector);
    fat_partition->buffer[old_pos.offset] = new_cluster;

    load_fat_sector(partition, new_pos.sector);
    fat_partition->buffer[new_pos.offset] = 0xFFFFFFF;
}

// TODO: room for improvement here to not do linear search, though pesmistically it will be O(n)
static u32 find_empty_cluster(VFS_PARTITION* partition){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    for (int i = 2; i < fat_partition->sectors_per_fat * 128; i++) {
        int potential_cluster = get_next_cluster(partition, i);
        if(potential_cluster == 0){
            return i;
        }
    }
    return 0;
}

static u32 allocate_cluster(VFS_PARTITION* partition, u32 end_of_cluster){
    // Find a new cluster
    int new_cluster = find_empty_cluster(partition);
    if(new_cluster == 0) return 0;

    // Write zeroes to newly allocated cluster
    void* zero_buffer = k_malloc(512);
    k_memset(zero_buffer, 512, 0);
    write_cluster(partition, new_cluster, zero_buffer);

    // Change fat
    change_fat(partition, end_of_cluster, new_cluster);

    // Tidy up
    k_free(zero_buffer);
    return new_cluster;
}

static int cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    VFS_DEVICE* device = vfs_partition_get_device(partition);
    return vfs_device_read(device, buffer, 1, fat_partition->data_offset + cluster - 2);
}

static int write_cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    VFS_DEVICE* device = vfs_partition_get_device(partition);
    return vfs_device_write(device, buffer, 1, fat_partition->data_offset + cluster - 2);
}

static int read_part_cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer, u32 size, u32 offset){
    void* cluster_buffer = k_malloc(512);
    int result = cluster(partition, cluster, cluster_buffer);
    if(result == E_DEVICE_OK){
        k_memcpy((u8*)cluster_buffer + offset, buffer, size);
    }
    k_free(cluster_buffer);
    return result;
}

static int idle_cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer, u32 size, u32 offset){
    return 0;
}

static int write_part_cluster(struct VFS_PARTITION* partition, u32 cluster, void* buffer, u32 size, u32 offset){
    void* cluster_buffer = k_malloc(512);
    int result = cluster(partition, cluster, cluster_buffer);
    int result2 = 0;
    if(result == E_DEVICE_OK){
        k_memcpy(buffer, (u8*) cluster_buffer + offset, size);
        result2 = write_cluster(partition, cluster, cluster_buffer);
    }
    k_free(cluster_buffer);
    return result | result2;
}

static u32 traverse_fat_coroutine(struct VFS_PARTITION* partition, void* buffer, u32 cluster,
                                  u32 size, u32 offset,
                                  CLUSTER_ACTION action){
    if(cluster != 0x0 && cluster < 0xFFFFFF8){
        action(partition, cluster, buffer, size, offset);
        cluster = get_next_cluster(partition, cluster);
        return cluster;
    }
    return 0;
}

static u32 get_cluster_from_node(FAT32_PARTITION* partition ,struct FAT32_NODE* node){
    if(!node)
        return partition->root_sector;
    return ((u32)node->high_16_first_cluster << 16) | (u32)node->start_low;
}

static char to_upper_case(char c){
    if('a' <= c && c <= 'z')
        return 'A' + (c - 'a');
    return c;
}

static int alphanumeric(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

// filename - char[] with length filename_length
// buffer -   size of 11 bytes
static void make_8point3_name(char* filename, u32 filename_length, char* buffer){
    int counter = 0;
    for (int i = 0; i < 11; i++)
        buffer[i] = ' ';
    int last_dot = filename_length;
    for (int i = 0; i < filename_length - 1; i++)
        if(filename[i] == '.' && alphanumeric(filename[i + 1]))
            last_dot = i;

    while(counter < last_dot && counter < filename_length){
        if(counter < 8)
            buffer[counter] = to_upper_case(*filename);
        counter++; filename++;
    }
    filename++; counter++;
    int after_dot_counter = 0;
    while(counter < filename_length && after_dot_counter < 3){
        buffer[8 + after_dot_counter] = to_upper_case(*filename);
        after_dot_counter++; counter++; filename++;
    }
}

enum FAT32_NODE_MATCH {

    FAT32_MATCH_DIR = 0x0,
    FAT32_MATCH_END = 0x1,
    FAT32_MATCH_NO_MATCH = 0x02,
    FAT32_MATCH_NOT_DIR = 0x4,
};

static enum FAT32_NODE_MATCH compare_directory(struct FAT32_NODE* dir_entry, char* name83){
    if(dir_entry->filename[0] == '\0')
        return FAT32_MATCH_END;
    if((u8)dir_entry->filename[0] == 0xE5)
        return FAT32_MATCH_NO_MATCH;
    if(!k_memcmp(name83, dir_entry->filename, 11))
        return FAT32_MATCH_NO_MATCH;
    if(dir_entry->attributes & FAT32_DA_DIR)
        return FAT32_MATCH_DIR;
    return FAT32_MATCH_NOT_DIR;
}

static u32 find_in_directory(VFS_PARTITION* partition, u32 cluster,
                                            char* filename, u32 length){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    struct FAT32_NODE* buffer = k_malloc(512);

    // Prepare 8.3 name for comparing
    char name83[11];
    make_8point3_name(filename, length, name83);

    while(cluster){
        cluster = traverse_fat_coroutine(partition, buffer, cluster, 512, 0, read_part_cluster);
        for (int i = 0; i < 16; i++) {
            switch(compare_directory(&buffer[i], name83)){
                case FAT32_MATCH_END:
                    k_free(buffer);
                    return 0;
                case FAT32_MATCH_NO_MATCH:
                    continue;
                case FAT32_MATCH_DIR:
                case FAT32_MATCH_NOT_DIR:
                    k_free(buffer);
                    return get_cluster_from_node(fat_partition, &buffer[i]);
            }
        }
    }
    return 0;
}

static struct FAT32_NODE* resolve_path(char* path, VFS_PARTITION* partition, VFS_NODE* dir){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    struct FAT32_NODE* fat_node = dir ? vfs_node_get_data(dir) : 0;
    u32 cluster = get_cluster_from_node(fat_partition, fat_node);
    if(*path == '/')
        path++;
    struct FAT32_NODE* buffer = k_malloc(512);
    int length;
    int result;
    while((length = str_until(path, '/'))){
        cluster = find_in_directory(partition, cluster, path, length);

        path += length + 1;
    }
    int i;
    u32 previous_cluster;
    while(cluster != 0 && cluster < 0xFFFFFF8){
        previous_cluster = cluster;
        cluster = traverse_fat_coroutine(partition, buffer, cluster, 512, 0, read_part_cluster);
        length = str_len(path);
        char name83[11];
        make_8point3_name(path, length, name83);
        for (i = 0; i < 16; i++) {
            if(buffer[i].filename[0] == '\0'){
                result = -1;
                k_free(buffer);
                return 0;
            }
            if(((u8)buffer[i].filename[0]) == 0xE5)
                continue;
            result = k_memcmp(name83, &buffer[i].filename, 11);
            if(!result)
                break;
        }
    }
    struct FAT32_NODE_INFO* return_value = k_malloc(sizeof(struct FAT32_NODE_INFO));
    return_value->descriptor_cluster = previous_cluster;
    return_value->descriptor_offset = i;
    return_value->first_cluster = ((u32)buffer[i].high_16_first_cluster << 16) | buffer[i].start_low;

    // FAT32_NODE array
    // TODO: finish this
    cluster(partition, return_value->descriptor_cluster, buffer);

    struct FAT32_NODE* descriptor = k_malloc(sizeof(struct FAT32_NODE));
    k_memcpy(descriptor + return_value->descriptor_offset, descriptor, sizeof(struct FAT32_NODE));

    k_free(return_value);
    k_free(buffer);
    return descriptor;
}
