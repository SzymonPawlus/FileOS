#include "fat32.h"

#include <stdint.h>

#include "../libc/memory.h"
#include "vfs.h"
#include "../kernel/util.h"
#include "../libc/strings.h"

typedef enum E_DEVICE (*CLUSTER_ACTION)(VFS_PARTITION* partition, uint32_t cluster, void* buffer, uint32_t size, uint32_t offset);

// Partition intialization
static FAT32_PARTITION* init_partition(FAT32_BOOT_RECORD* info);

// Partition state saving routines
static enum E_DEVICE save_descriptor(VFS_PARTITION* partition, FAT32_NODE_INFO* node_info);
static enum E_DEVICE save_current_fat(VFS_PARTITION *p);

// File access routines
static int32_t write_bytes_in_file(VFS_NODE *node, void *buffer, uint32_t size);
static int32_t read_bytes_from_file(VFS_NODE* node, void* buffer, uint32_t size);

// Directory access routines
static FAT32_NODE_INFO find_node_in_directory(VFS_PARTITION* partition, char* filename, struct FAT32_NODE* directory, int is_creating);
static int is_created(FAT32_NODE_INFO* node_info);
static int initialize_node(VFS_PARTITION *partition, FAT32_NODE_INFO *node_info, char *filename, int flags);
static int initialize_directory_inside(VFS_PARTITION *partition, FAT32_NODE_INFO *node_info);

// Going through FAT
static uint32_t traverse_fat_coroutine(VFS_PARTITION* partition, void* buffer, uint32_t cluster,
                                       uint32_t size, uint32_t offset,
                                       CLUSTER_ACTION action, int is_allocating);
static uint32_t get_cluster_from_node(FAT32_PARTITION* partition ,struct FAT32_NODE* node);
static uint32_t allocate_cluster(VFS_PARTITION* partition, uint32_t end_of_cluster);
static uint32_t find_empty_cluster(VFS_PARTITION* partition);
static uint32_t get_next_cluster(VFS_PARTITION* p, uint32_t cluster);
static FAT32_NODE_POSITION calculate_fat_position(FAT32_PARTITION* partition, uint32_t cluster);
static enum E_DEVICE load_fat_sector(VFS_PARTITION* partition, uint32_t sector);

// Partial cluster access routines
static enum E_DEVICE read_part_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer, uint32_t size, uint32_t offset);
static enum E_DEVICE write_part_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer, uint32_t size,
                                        uint32_t offset);
static enum E_DEVICE idle_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer, uint32_t size,
                                  uint32_t offset);

// Cluster access routines
static enum E_DEVICE read_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer);
static enum E_DEVICE write_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer);

// Path resolving
static void make_8point3_name(char* filename, uint32_t filename_length, char* buffer);
static FAT32_NODE_INFO *resolve_path(const char *path, VFS_PARTITION *partition, FAT32_NODE_INFO *node, int is_creating);
static char* get_filename_from_path(const char* path);

// Listing directory
static DIR_ENTRY fat32_entry_to_dir_entry(struct FAT32_NODE* node);

// Deleting node
static int delete_fat_chain(VFS_PARTITION *partition, uint32_t start_cluster);

enum E_PARTITION fat32_find_partition(VFS_DEVICE *device){

    FAT32_BOOT_RECORD info = {};

    if(vfs_device_read(device, &info.buffer[0], 1, 0) != E_DEVICE_OK)
        return E_PARTITION_DEVICE_FAILED;
    if(info.ebr.signature != 0x28 && info.ebr.signature != 0x29)
        return E_PARTITION_NO_FILESYSTEM_DETECTED;
    if(info.ebr.boot_signature != 0xAA55)
        return E_PARTITINO_INVALID_SIGNATURE;

    struct FAT32_FSINFO fs_info;
    vfs_device_read(device, &fs_info, 1, info.ebr.sector_of_FSInfo);
    FAT32_PARTITION* fat_partition = init_partition(&info);
    VFS_PARTITION* _;
    vfs_register_partition(fat_partition, device, fat32_open_file, fat32_create_file,
                           fat32_remove_file, fat32_open_dir, fat32_make_dir, fat32_remove_dir, _);
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
                                     char* path, int flags){
    // TODO: Implement flags checking
    FAT32_NODE_INFO* node_info = resolve_path(path, partition, vfs_node_get_data(dir), flags);
    if(!node_info)
        return 0;

    if(flags & O_CREAT && is_created(node_info)) {
        initialize_node(partition, node_info, get_filename_from_path(path), 0);
    }

    // Cannot open directory with this call
    if(node_info->node.attributes & FAT32_DA_DIR)
        return 0;

    VFS_NODE* file_descriptor;
    vfs_file_create_descriptor(node_info, partition, fat32_write_file, fat32_read_file, fat32_lseek, 0, &file_descriptor);

    return file_descriptor;
}

int                  fat32_create_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                       char* path, int flags){
    return 0;
}

int                  fat32_remove_file(VFS_PARTITION *partition, VFS_NODE* dir,
                                       char* path){
    FAT32_PARTITION *fat_partition = vfs_partition_get_data(partition);
    FAT32_NODE_INFO* fat_info = vfs_node_get_data(dir);
    FAT32_NODE_INFO* node_info = resolve_path(path, partition, fat_info, 0);
    if(!node_info)
        return 1;
    if(node_info->node.attributes & FAT32_DA_DIR) {
        k_free(node_info);
        return 1;
    }
    node_info->node.filename[0] = (int8_t)0xE5;
    save_descriptor(partition, node_info);
    delete_fat_chain(partition, get_cluster_from_node(fat_partition, &node_info->node));
    k_free(node_info);

    return 0;
}

VFS_NODE*            fat32_open_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                    char* path){
    FAT32_NODE_INFO* fat_dir = vfs_node_get_data(dir);
    FAT32_NODE_INFO* node_info = resolve_path(path, partition, fat_dir, 0);
    if(!node_info)
        return 0;

    VFS_NODE* file_descriptor;
    vfs_file_create_descriptor(node_info, partition, 0, 0, 0, fat32_list_dir, &file_descriptor);

    return file_descriptor;
}

int                  fat32_make_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                    char* path){
    FAT32_NODE_INFO* fat_dir = vfs_node_get_data(dir);
    FAT32_NODE_INFO* node_info = resolve_path(path, partition, fat_dir, O_CREAT);
    if(!node_info)
        return 1;
    if(is_created(node_info)) {
        initialize_node(partition, node_info, get_filename_from_path(path), O_DIR);
        initialize_directory_inside(partition, node_info);
    }
    k_free(node_info);

    return 0;
}

int                  fat32_remove_dir(VFS_PARTITION *partition, VFS_NODE* dir,
                                      char* path){
    return 1;
}

static uint32_t max(uint32_t a, uint32_t b){
    return (a > b) ? a : b;
}

static uint32_t min(uint32_t a, uint32_t b){
    return (a > b) ? b : a;
}

int                  fat32_write_file (VFS_NODE* node, void* buffer, uint32_t size){
    int32_t bytes_written = write_bytes_in_file(node, buffer, size);
    vfs_node_move_offset(node, bytes_written);

    return bytes_written;

}

int                  fat32_read_file  (VFS_NODE* node, void* buffer, uint32_t size){
    int32_t bytes_read = read_bytes_from_file(node, buffer, size);
    vfs_node_move_offset(node, bytes_read);

    return bytes_read;


}

int                  fat32_lseek(VFS_NODE* node, int32_t offset, enum SEEK whence) {
    if(!node)
        return -E_LSEEK_BADF;
    if(whence == SEEK_SET){
        if(offset > vfs_node_get_offset(node))
            vfs_node_move_offset(node, offset - vfs_node_get_offset(node));
        else
            vfs_node_move_offset(node, -vfs_node_get_offset(node));
    } else if(whence == SEEK_CUR){
        vfs_node_move_offset(node, offset);
    } else if(whence == SEEK_END){
        FAT32_NODE_INFO* info = vfs_node_get_data(node);
        vfs_node_move_offset(node, (int32_t)info->node.size + offset);
    }else {
        return -E_LSEEK_INVAL;
    }
    return vfs_node_get_offset(node);
}

int                  fat32_list_dir   (VFS_NODE* node, DIR_ENTRY* buffer, uint32_t size){
    // Function goes through directory using couroutine and fills buffer with entries until buffer full
    // If buffer is full return number of entries read
    // If encountered end of directory return 0
    // If encountered error return -1

    // Get FAT32 node info and partition
    FAT32_NODE_INFO* info = vfs_node_get_data(node);
    VFS_PARTITION* partition = vfs_node_get_partition(node);
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);

    // Initialize variables
    uint32_t cluster = get_cluster_from_node(fat_partition, &info->node);
    uint32_t entries_read = 0;
    struct FAT32_NODE fat32_entries[512 / sizeof(struct FAT32_NODE)];

    // Traverse the directory clusters
    while (cluster && entries_read < size) {
        cluster = traverse_fat_coroutine(partition, fat32_entries, cluster, 512, 0, read_part_cluster, 0);
        for (int i = 0; i < 16 && entries_read < size; i++) {
            if (fat32_entries[i].filename[0] == '\0') {
                // End of directory
                return 0;
            }
            if ((uint8_t)fat32_entries[i].filename[0] == 0xE5) {
                // Deleted entry
                continue;
            }
            // Convert FAT32 entry to DIR_ENTRY and add to buffer
            buffer[entries_read++] = fat32_entry_to_dir_entry(&fat32_entries[i]);
        }
    }

    // Update size with the number of entries read
    return (int)entries_read;
}

///
/// Static helper functions
///
static int delete_fat_chain(VFS_PARTITION *partition, uint32_t start_cluster) {
    FAT32_PARTITION *fat_partition = vfs_partition_get_data(partition);
    uint32_t current_cluster = start_cluster;
    uint32_t next_cluster;

    while (current_cluster >= 2 && current_cluster < 0xFFFFFF8) {
        next_cluster = get_next_cluster(partition, current_cluster);

        // Mark the current cluster as free
        FAT32_NODE_POSITION pos = calculate_fat_position(fat_partition, current_cluster);
        load_fat_sector(partition, pos.sector);
        fat_partition->buffer[pos.offset] = 0x00000000;
        fat_partition->buffer_changed = 1;

        current_cluster = next_cluster;
    }

    // Save the changes to the FAT
    return save_current_fat(partition);
}

static DIR_ENTRY fat32_entry_to_dir_entry(struct FAT32_NODE* node) {
    DIR_ENTRY entry;
    k_memcpy(node->filename, entry.name, 11);
    entry.name[11] = '\0';
    entry.type = (node->attributes & FAT32_DA_DIR) ? DIRECTORY : FILE;
    entry.size = node->size;
    return entry;
}

static int32_t read_bytes_from_file(VFS_NODE* node, void* buffer, uint32_t size) {
    // Get Information
    FAT32_NODE_INFO* info = vfs_node_get_data(node);
    VFS_PARTITION* partition = vfs_node_get_partition(node);
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    int32_t offset = vfs_node_get_offset(node);

    uint32_t max_size = info->node.size - offset;
    size = min(size, max_size);

    // Traverse to the current offset of a file descriptor
    uint32_t current_cluster = get_cluster_from_node(fat_partition, &info->node);
    while(offset > 512){
        current_cluster = traverse_fat_coroutine(partition, buffer, current_cluster, 512,
            0, idle_cluster, 1);
        offset -= 512;
    }

    // Read buffer not allocating anything along the way till end of file OR size
    int32_t bytes_read = 0;
    while(size > 0){
        uint32_t bytes_to_read = min(size, 512 - offset);
        current_cluster = traverse_fat_coroutine(partition, &buffer[bytes_read], current_cluster, bytes_to_read,
            offset, read_part_cluster, 0);
        bytes_read += (int32_t)bytes_to_read;
        offset = 0;
        if(current_cluster == 0)
            return bytes_read;
        size -= bytes_to_read;
    }

    return bytes_read;
}

static int32_t write_bytes_in_file(VFS_NODE *node, void *buffer, uint32_t size) {
    // Get Information
    FAT32_NODE_INFO* info = vfs_node_get_data(node);
    VFS_PARTITION* partition = vfs_node_get_partition(node);
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    int32_t offset = vfs_node_get_offset(node);
    int32_t starting_offset = offset;

    // Traverse to the current offset of a file descriptor
    uint32_t current_cluster = get_cluster_from_node(fat_partition, &info->node);
    while(offset > 512){
        current_cluster = traverse_fat_coroutine(partition, buffer, current_cluster, 512,
            0, idle_cluster, 1);
        offset -= 512;
    }

    // Write buffer maybe allocating new FATs along the way
    int32_t bytes_written = 0;
    while(size > 0){
        uint32_t bytes_to_write = min(size, 512 - offset);
        current_cluster = traverse_fat_coroutine(partition, &buffer[bytes_written], current_cluster, bytes_to_write,
            offset, write_part_cluster, bytes_to_write < size);
        bytes_written += (int32_t)bytes_to_write;
        offset = 0;
        if(current_cluster == 0)
            break;
        size -= bytes_to_write;
    }

    // Return the number of bytes written
    info->node.size = max(info->node.size, starting_offset + bytes_written);
    save_current_fat(partition);
    save_descriptor(partition, info);

    return bytes_written;

}

static enum E_DEVICE save_descriptor(VFS_PARTITION* partition, FAT32_NODE_INFO* node_info){
    VFS_DEVICE* device = vfs_partition_get_device(partition);
    return write_part_cluster(partition, node_info->descriptor_cluster, &node_info->node,
        sizeof(struct FAT32_NODE),
        node_info->descriptor_offset * sizeof(struct FAT32_NODE));
}

static FAT32_NODE_POSITION calculate_fat_position(FAT32_PARTITION *p, uint32_t cluster){
    FAT32_NODE_POSITION pos = {
         .sector = cluster / 128,
         .offset = (cluster % 128),
    };
    return pos;
}

static enum E_DEVICE save_current_fat(VFS_PARTITION *p){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(p);

    // If buffer didn't change or is not loaded no point in saving it
    if(!fat_partition->buffer_changed || !fat_partition->buffer_loaded) return 0;

    // Otherwise write changed fat to the disk
    VFS_DEVICE* device = vfs_partition_get_device(p);

    // Write first FAT
    enum E_DEVICE result = vfs_device_write(device, fat_partition->buffer, 1, fat_partition->current_sector + fat_partition->fat_offset);
    if(result != E_DEVICE_OK) return result;

    // Write second FAT
    result = vfs_device_write(device, fat_partition->buffer, 1,
        fat_partition->current_sector + fat_partition->fat_offset + fat_partition->sectors_per_fat);

    // Unset dirty bit
    fat_partition->buffer_changed = 0;
    return result;
}

static enum E_DEVICE load_fat_sector(VFS_PARTITION* p, uint32_t sector){
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

static uint32_t get_next_cluster(VFS_PARTITION* p, uint32_t cluster){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(p);

    // Calculate position
    FAT32_NODE_POSITION pos = calculate_fat_position(fat_partition, cluster);
    load_fat_sector(p, pos.sector);

    uint32_t next_cluster = fat_partition->buffer[pos.offset];
    return next_cluster;
}

static enum E_DEVICE change_fat(VFS_PARTITION *partition, uint32_t end_of_chain_cluster, uint32_t new_cluster){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);

    // Calculate positions
    const FAT32_NODE_POSITION old_pos = calculate_fat_position(fat_partition, end_of_chain_cluster);
    const FAT32_NODE_POSITION new_pos = calculate_fat_position(fat_partition, new_cluster);

    // Write new values
    enum E_DEVICE result = load_fat_sector(partition, old_pos.sector);
    if(result != E_DEVICE_OK)
        return result;
    fat_partition->buffer[old_pos.offset] = new_cluster;
    fat_partition->buffer_changed = 1;

    result = load_fat_sector(partition, new_pos.sector);
    if(result != E_DEVICE_OK)
        return result;
    fat_partition->buffer[new_pos.offset] = 0xFFFFFFF;
    fat_partition->buffer_changed = 1;
    save_current_fat(partition);
}

static uint32_t find_empty_cluster(VFS_PARTITION* partition){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    for (int i = 2; i < fat_partition->sectors_per_fat * 128; i++) {
        int potential_cluster = get_next_cluster(partition, i);
        if(potential_cluster == 0){
            return i;
        }
    }
    return 0;
}

static uint32_t allocate_cluster(VFS_PARTITION* partition, uint32_t end_of_cluster){
    // Find a new cluster
    uint32_t new_cluster = find_empty_cluster(partition);
    if(new_cluster == 0) return 0;

    // Write zeroes to newly allocated cluster
    uint8_t zero_buffer[512];
    k_memset(zero_buffer, 512, 0);
    enum E_DEVICE result = write_cluster(partition, new_cluster, zero_buffer);
    if(result != E_DEVICE_OK)
        return 0;

    // Change fat
    if(end_of_cluster == 0)
        end_of_cluster = new_cluster;
    change_fat(partition, end_of_cluster, new_cluster);

    // Tidy up
    return new_cluster;
}

static enum E_DEVICE read_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    VFS_DEVICE* device = vfs_partition_get_device(partition);
    return vfs_device_read(device, buffer, 1, fat_partition->data_offset + cluster - 2);
}

static enum E_DEVICE write_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer){
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    VFS_DEVICE* device = vfs_partition_get_device(partition);
    return vfs_device_write(device, buffer, 1, fat_partition->data_offset + cluster - 2);
}

static enum E_DEVICE read_part_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer, uint32_t size, uint32_t offset){
    uint8_t temp_buffer[512];
    enum E_DEVICE result = read_cluster(partition, cluster, temp_buffer);
    if(result != E_DEVICE_OK)
        return result;
    k_memcpy(&temp_buffer[offset], buffer, size);
    return E_DEVICE_OK;
}

static enum E_DEVICE idle_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer, uint32_t size,
                                  uint32_t offset){
    return E_DEVICE_OK;
}

static enum E_DEVICE write_part_cluster(VFS_PARTITION *partition, uint32_t cluster, void *buffer, uint32_t size, uint32_t offset){
    uint8_t temp_buffer[512];
    enum E_DEVICE result = read_cluster(partition, cluster, temp_buffer);
    if(result != E_DEVICE_OK)
        return result;
    k_memcpy(buffer, temp_buffer + offset, size);
    return write_cluster(partition, cluster, temp_buffer);
}

static uint32_t traverse_fat_coroutine(VFS_PARTITION* partition, void* buffer, uint32_t cluster,
                                       uint32_t size, uint32_t offset,
                                       CLUSTER_ACTION action, int is_allocating){
    if(cluster == 0x0 || cluster >= 0xFFFFFF8)
        return 0;

    action(partition, cluster, buffer, size, offset);
    uint32_t new_cluster = get_next_cluster(partition, cluster);
    if(new_cluster >= 0xFFFFFF8 && is_allocating){
        cluster = allocate_cluster(partition, cluster);
    }else {
        cluster = new_cluster;
    }
    return cluster;
}

static uint32_t get_cluster_from_node(FAT32_PARTITION* partition ,struct FAT32_NODE* node){
    if(!node)
        return partition->root_sector;
    return ((uint32_t)node->start_high << 16) | (uint32_t)node->start_low;
}


///
/// Resolving Path
///
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
static void make_8point3_name(char* filename, uint32_t filename_length, char* buffer){
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
    FAT32_MATCH_DELETED = 0x8,
};

static enum FAT32_NODE_MATCH compare_directory(struct FAT32_NODE* dir_entry, char* name83){
    if(dir_entry->filename[0] == '\0')
        return FAT32_MATCH_END;
    if((uint8_t)dir_entry->filename[0] == 0xE5)
        return FAT32_MATCH_DELETED;
    if(k_memcmp(name83, dir_entry->filename, 11))
        return FAT32_MATCH_NO_MATCH;
    if(dir_entry->attributes & FAT32_DA_DIR)
        return FAT32_MATCH_DIR;
    return FAT32_MATCH_NOT_DIR;
}

static FAT32_NODE_INFO find_node_in_directory(VFS_PARTITION* partition, char* filename, struct FAT32_NODE* directory, int is_creating) {
    FAT32_PARTITION* fat_partition = vfs_partition_get_data(partition);
    struct FAT32_NODE buffer[512 / sizeof(struct FAT32_NODE)];

    // Prepare 8.3 name for comparing
    char name83[11];
    make_8point3_name(filename, str_len(filename), name83);

    uint32_t cluster = get_cluster_from_node(fat_partition, directory);
    uint32_t starting_cluster = cluster;
    uint32_t new_cluster = cluster;
    int i = 0;
    FAT32_NODE_INFO for_creation = {0, 0, {}};
    while(new_cluster){
        cluster = new_cluster;
        new_cluster = traverse_fat_coroutine(partition, buffer, cluster, 512, 0, read_part_cluster, 0);
        for (i = 0; i < 16; i++) {
            switch(compare_directory(&buffer[i], name83)){
                case FAT32_MATCH_END:
                    goto after_search;
                case FAT32_MATCH_NO_MATCH:
                    continue;
                case FAT32_MATCH_DIR:
                case FAT32_MATCH_NOT_DIR:
                    return (FAT32_NODE_INFO) {cluster, i, buffer[i], starting_cluster};
                case FAT32_MATCH_DELETED:
                    for_creation = (FAT32_NODE_INFO) {cluster, i, buffer[i], starting_cluster};
            }
        }
    }
    after_search:
    if(!is_creating)
        return (FAT32_NODE_INFO) {0, 0, {}};

    // If we are creating
    // If we found a deleted entry we can use it
    if(for_creation.descriptor_cluster != 0)
        return for_creation;

    // If we were by the end of the cluster we need to allocated another cluster
    if(i == 16) {
        cluster = allocate_cluster(partition, cluster);
        i = 0;
        if(cluster == 0)
            return (FAT32_NODE_INFO) {0, 0, {}};
    }
    buffer[i].filename[0] = (int8_t)0xE5; // Set it as deleted entry to indicated later that this is newly created field
    return (FAT32_NODE_INFO) {cluster, i, buffer[i], starting_cluster};
}

static int is_created(FAT32_NODE_INFO* node_info) {
    if(!node_info)
        return 0;
    if(node_info->descriptor_cluster == 0)
        return 0;
    return (uint8_t)node_info->node.filename[0] == 0xE5;
}

static int initialize_node(VFS_PARTITION *partition, FAT32_NODE_INFO *node_info, char *filename, int flags) {
    if(!is_created(node_info))
        return 0;
    uint32_t first_cluster = allocate_cluster(partition, 0);
    if(first_cluster == 0)
        return 0;
    make_8point3_name(filename, str_len(filename), node_info->node.filename);
    node_info->node.size = 0;
    node_info->node.start_high = first_cluster >> 16;
    node_info->node.start_low = first_cluster;
    node_info->node.attributes = (flags & O_DIR) ? FAT32_DA_DIR : 0;
    enum E_DEVICE result = save_descriptor(partition, node_info);
    if(result != E_DEVICE_OK)
        return 0;
    result = save_current_fat(partition);
    return result == E_DEVICE_OK;

}

static int initialize_directory_inside(VFS_PARTITION *partition, FAT32_NODE_INFO *node_info) {
    node_info->node.attributes = FAT32_DA_DIR;
    node_info->node.size = 0;

    struct FAT32_NODE buffer[512 / sizeof(struct FAT32_NODE)];
    k_memset(buffer, 512, 0);
    for (int i = 0; i < 11; ++i) {
        buffer[0].filename[i] = ' ';
        buffer[1].filename[i] = ' ';

    }
    buffer[0].filename[0] = '.';
    buffer[1].filename[0] = '.';
    buffer[1].filename[1] = '.';

    buffer[0].start_low = node_info->node.start_low;
    buffer[0].start_high = node_info->node.start_high;
    buffer[1].start_low = node_info->parent_cluster & 0xFFFF;
    buffer[1].start_high = node_info->parent_cluster >> 16;

    buffer[0].attributes = FAT32_DA_DIR;
    buffer[1].attributes = FAT32_DA_DIR;
    return write_cluster(partition, node_info->node.start_low + (node_info->node.start_high << 16), buffer) == E_DEVICE_OK;
}


// Function takes a string and allocates a space for a string the same length and copy it
static char* copy_str(const char* str) {
    uint32_t len = str_len(str);
    char* new_str = k_malloc(len + 1);
    k_memcpy(str, new_str, len);
    new_str[len] = 0;
    return new_str;
}

static char* get_filename_from_path(const char* path) {
    const char* last_slash = path;
    while(*path){
        if(*path == '/')
            last_slash = path;
        path++;
    }
    return ++last_slash;
}

static FAT32_NODE_INFO *resolve_path(const char *path, VFS_PARTITION *partition, FAT32_NODE_INFO *fat32_node, int flags){
    // Get FAT data
    char* copied_path = copy_str(path);
    char* used_path = copied_path;

    // Choose local or global root
    if(*used_path == '/') {
        fat32_node = 0;
        used_path++;
    }

    // Go through directories and find till path empty using strtok
    char* token = strtok(used_path, '/');
    char* new_token = 0;
    FAT32_NODE_INFO info = fat32_node ? *fat32_node : (FAT32_NODE_INFO) {0, 0, {}};
    while(*token){
        new_token = strtok(0, '/');
        info = find_node_in_directory(partition, token, info.descriptor_cluster ? &info.node : 0, !*new_token && (flags & O_CREAT));
        token = new_token;
        if(info.descriptor_cluster == 0)
            break;
    }
    k_free(copied_path);
    if(info.descriptor_cluster == 0)
        return 0;

    FAT32_NODE_INFO* node_info = k_malloc(sizeof(FAT32_NODE_INFO));
    node_info->descriptor_cluster = info.descriptor_cluster;
    node_info->descriptor_offset = info.descriptor_offset;
    node_info->node = info.node;
    node_info->parent_cluster = info.parent_cluster;
    return node_info;
}
