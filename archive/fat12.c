//
// Created by szymon on 04/08/2021.
//

#include <stddef.h>
#include "fat12.h"
#include "../libc/memory.h"
#include "../kernel/util.h"
#include "../libc/strings.h"
#include "../libc/math.h"
// Napisze sobie tutaj jakis komentarz
const struct FAT_NODE root_dir = { .attributes = FAT_DA_DIR  };

static struct FAT_NODE node_buffer[32];
static uint32_t node_buffer_lba = 0;
static char path_buffer[13];

struct {
    uint8_t buffer[0x600]; // 3 disk sectors sized buffer
    uint32_t current_sector; // Sector of FAT that was currently loaded
    int changed;
} FAT_buffer = { "" , -1, 0};

static FAT12_PARTITION init_partition(FAT12_BOOT_RECORD* bootsector);

static char * fat_12_stat_resolve_path(char* path);

// Path functions
static struct FAT_NODE resolve_directories(struct VFS_PARTITION *partition, char **path);
static struct FAT_NODE
find_in_directory(struct VFS_PARTITION *partition, struct FAT_NODE *node, char *path, FAT_NODE_POSITION *position);
static struct VFS_NODE
fat_to_vfs(struct VFS_PARTITION *partition, struct FAT_NODE *fat_node, FAT_NODE_POSITION *position);
static struct FAT_NODE empty_node();
static int fat12_remove_node(struct VFS_PARTITION *this, struct FAT_NODE *parent_node, char *name);
static int is_node_empty(struct FAT_NODE *node);

// Nodes functions
static struct FAT_NODE *resolve_root(struct VFS_PARTITION *part, FAT_NODE_POSITION *position);
static struct FAT_NODE *
resolve_dir(struct VFS_PARTITION *part, struct FAT_NODE *resolve_node, FAT_NODE_POSITION *position);
static struct FAT_NODE *list_root(uint32_t *entries, struct VFS_PARTITION *partition);
static struct FAT_NODE* list_dir(uint32_t* entries, struct VFS_NODE* resolve_node);

static int add_node_root(struct FAT_NODE *node, struct VFS_PARTITION *part);
static void add_node_dir(struct VFS_NODE* parent_node, struct FAT_NODE* node);
static void fill_node(struct FAT_NODE *node, char *name, uint8_t attributes, uint32_t start_cluster, uint32_t size);

// FAT buffer functions
static uint32_t get_entry_offset(struct VFS_PARTITION *part, uint32_t cluster);
static uint32_t read_fat_value(struct VFS_PARTITION *part, uint32_t cluster);
static void write_fat_value(struct VFS_PARTITION *part, uint32_t cluster, uint32_t value);
static void flush_fat_buffer(struct VFS_PARTITION *part);

// FAT functions
static uint8_t *read_chain(struct VFS_PARTITION *part, uint32_t cluster, uint32_t *length);
static uint32_t *reserve_chain(struct VFS_PARTITION *part, uint32_t clusters);
static void fill_chain(struct VFS_PARTITION *part, uint32_t *clusters_list, uint32_t clusters, uint8_t *buffer);
static uint32_t *resize_chain(struct VFS_PARTITION *part, uint32_t cluster, uint32_t clusters);
static void fill_chain_sector_from_begin_offset(struct VFS_PARTITION *part, uint32_t cluster, uint8_t *buffer, uint32_t offset);

// FAT utils
static uint32_t cluster_FAT_offset(uint32_t cluster);

struct VFS_PARTITION fat12_init(struct VFS_DEVICE *device) {
    FAT12_BOOT_RECORD* info = k_malloc(512);
    device->read(device, &info->buffer[0], 1, 0);

    struct VFS_PARTITION v_partition = {
            .device      = device,
            .open_file   = fat12_open_file,
            .create_file = fat12_create_file,
            .remove_file = fat12_remove_file,
            .open_dir    = fat12_open_dir,
            .make_dir    = fat12_make_dir,
            .remove_dir  = fat12_remove_dir,
            .error       = 0
    };


    // Check signatures!
    if(info->ebr.signature != 0x28 && info->ebr.signature != 0x29) v_partition.error = 1;
    if(info->ebr.boot_signature != 0xAA55) v_partition.error = 2;

    FAT12_PARTITION* f_partition = k_malloc(sizeof(FAT12_PARTITION));
    *f_partition = init_partition(info);
    v_partition.partition_data = f_partition;

    k_free(info);
    return v_partition;
}


// Public section

/* Returns info of file pointed by given path */
struct VFS_NODE
fat12_open_file (struct VFS_PARTITION *this, char *path) {
    struct FAT_NODE parent_node = resolve_directories(this, &path);
    if(is_node_empty(&parent_node)) return vfs_empty_node(); // Couldn't follow path!

    FAT_NODE_POSITION* pos = k_malloc(sizeof(FAT_NODE_POSITION));

    struct FAT_NODE node = find_in_directory(this, &parent_node, path, pos);
    if(node.attributes & FAT_DA_DIR) return vfs_empty_node(); // Not a file!

    return fat_to_vfs(this, &node, pos); // Return file
}

/* Creates file in the given path */
int
fat12_create_file (struct VFS_PARTITION *this, char* path) {
    struct FAT_NODE parent_node = resolve_directories(this, &path);
    if(is_node_empty(&parent_node)) return ED_FAT | E_ERROR; // Couldn't follow path!

    struct FAT_NODE node = find_in_directory(this, &parent_node, path, NULL);
    if(is_node_empty(&node)){
        // Do the stuff
        uint32_t clusters = 1;
        uint32_t* clusters_list = reserve_chain(this, clusters);
        uint8_t buffer[512] = {};
        fill_chain(this, clusters_list, clusters, &buffer[0]);
        struct FAT_NODE new_node = { };
        fill_node(&new_node, &path_buffer[0], 0, *clusters_list, 0);
        k_free(clusters_list);
        if(!parent_node.start_low) { // Root directory
            add_node_root(&new_node, this);
        }else{ // Normal directory
            struct VFS_NODE parent = fat_to_vfs(this, &parent_node, NULL);
            add_node_dir(&parent, &new_node);
        }
        return 0;
    }
    else
        return ED_FAT | E_ERROR; // Return suitable error
}

int
fat12_remove_file(struct VFS_PARTITION *this, char* path){
    struct FAT_NODE parent_node = resolve_directories(this, &path);
    if(is_node_empty(&parent_node)) return ED_FAT | E_ERROR; // Couldn't follow path!
    struct FAT_NODE node = find_in_directory(this, &parent_node, path, NULL);
    if(node.attributes & FAT_DA_DIR) return ED_FAT | E_ERROR; // Not a file!

    return fat12_remove_node(this, &parent_node, path);
}

/* Returns info of directory pointed by path */
struct VFS_NODE
fat12_open_dir (struct VFS_PARTITION *this, char* path){
    struct FAT_NODE parent_node = resolve_directories(this, &path);
    if(is_node_empty(&parent_node)) return vfs_empty_node(); // Couldn't follow path!
    struct FAT_NODE node = find_in_directory(this, &parent_node, path, NULL);
    if(!(node.attributes & FAT_DA_DIR)) return vfs_empty_node(); // Not a directory!

    return fat_to_vfs(this, &node, NULL); // Return directory
}

/* Creates directory in place pointed by path */
int
fat12_make_dir (struct VFS_PARTITION *this, char* path){
    struct FAT_NODE parent_node = resolve_directories(this, &path);
    if(is_node_empty(&parent_node)) return ED_FAT | E_ERROR; // Couldn't follow path!

    struct FAT_NODE node = find_in_directory(this, &parent_node, path, NULL);
    if(is_node_empty(&node)){
        // Create basic folders
        struct FAT_NODE* here_dir = k_malloc(512);
        struct FAT_NODE* back_dir = here_dir + 1;

        k_memset(here_dir, 512, 0x00);

        uint32_t* dir_cluster = reserve_chain(this, 1);

        char* resolved_name = fat_12_stat_resolve_path(path);

        fill_node(here_dir, ".",  FAT_DA_DIR, *dir_cluster, 0);
        fill_node(back_dir, "..", FAT_DA_DIR, parent_node.start_low, 0);
        fill_node(&node, resolved_name, FAT_DA_DIR, *dir_cluster, 0);


        fill_chain(this, dir_cluster, 1, (uint8_t *) here_dir);

        if(!parent_node.start_low) { // Root directory
            add_node_root(&node, this);
        }else{ // Normal directory
            struct VFS_NODE parent = fat_to_vfs(this, &parent_node, NULL);
            add_node_dir(&parent, &node);
        }

        k_free(resolved_name);
        k_free(here_dir);

        return 0;
    }
    else
        return ED_FAT | E_ERROR; // Return suitable error
}

int
fat12_remove_dir (struct VFS_PARTITION *this, char* path){
    return E_ERROR | ED_FAT;
}

int
fat12_write_file (struct VFS_NODE* this, uint8_t* buffer, uint32_t size, uint32_t offset){
    uint32_t all_clusters    = ((offset + size) / 512) + 1; // TODO - make 512 a constant
    uint32_t current_cluster = this->FS_reference; // TODO - check if given cluster is correct
    uint32_t end = offset + size;
    FAT12_PARTITION* fat_partition = this->partition->partition_data;
    uint32_t bytes_written = 0;

    int writing = 0;
    uint32_t last_cluster;

    for (int i = 0; i < all_clusters; ++i) {
        if(!writing){
            if(!(0 < current_cluster && current_cluster < 0xff8)) {
                writing = 1;
                current_cluster = 2; // Start finding clusters from beginning
            }
        }
        if(writing){ // If we are writing than add new sectors
            // Iterate until you find free sector
            while(read_fat_value(this->partition, current_cluster)){
                current_cluster++;
                if(current_cluster >= 0xff8) return ED_FAT | E_ERROR; // TODO - ensure that changed file size
            }
            // Change last chain value
            write_fat_value(this->partition, last_cluster, current_cluster);

            // Maybe not most optimal but ensures that chain is always finished
            write_fat_value(this->partition, current_cluster, 0xff8);

            last_cluster = current_cluster;
        }
        uint32_t current_offset_low =   i      * 512; // TODO - make 512 a constant
        uint32_t current_offset_high =   (i + 1)      * 512; // TODO - make 512 a constant

        uint32_t low  = clamp(offset, current_offset_low, current_offset_high);
        uint32_t high = clamp(end   , current_offset_low, current_offset_high);

        // Then we can write something
        if(low < high){
            if((low % 512) == 0 && (high % 512) == 0) { // Write full sector
                this->partition->device->write(this->partition->device, &buffer[bytes_written], 1,
                                              current_cluster + fat_partition->data_offset - 2);
                bytes_written += 512; // TODO - extract to constant
            }
            else{
                uint8_t temp_buffer[512];
                this->partition->device->read(this->partition->device, &temp_buffer[0], 1, current_cluster + fat_partition->data_offset - 2);
                k_memcpy(&buffer[bytes_written], &temp_buffer[low % 512], high - low);
                this->partition->device->write(this->partition->device, &temp_buffer[0], 1, current_cluster + fat_partition->data_offset - 2);
                bytes_written += high - low;
            }
        }
        if(!writing) {
            last_cluster = current_cluster;
            current_cluster = read_fat_value(this->partition, current_cluster);
        }
    }

    flush_fat_buffer(this->partition);

    FAT_NODE_POSITION* position = this->node_data;
    struct FAT_NODE temp_buffer[16];
    this->partition->device->read(this->partition->device, &temp_buffer[0], 1, position->sector);
    temp_buffer[position->offset].size = max(this->size, end);
    this->partition->device->write(this->partition->device, &temp_buffer[0], 1, position->sector);

    return 0; // Means no error

}

int
fat12_read_file  (struct VFS_NODE* this, uint8_t* buffer, uint32_t size, uint32_t offset){
    uint32_t all_clusters    = ((offset + size) / 512) + 1; // TODO - make 512 a constant
    uint32_t current_cluster = this->FS_reference;
    uint32_t end = offset + size;
    FAT12_PARTITION* fat_partition = this->partition->partition_data;
    uint32_t bytes_read = 0;


    for (int i = 0; i < all_clusters; ++i) {
        if(!(0 < current_cluster && current_cluster < 0xff8)) return ED_FAT | E_ERROR;
        uint32_t current_offset_low =   i      * 512; // TODO - make 512 a constant
        uint32_t current_offset_high =   (i + 1)      * 512; // TODO - make 512 a constant

        uint32_t low  = clamp(offset, current_offset_low, current_offset_high);
        uint32_t high = clamp(end   , current_offset_low, current_offset_high);

        // Then we can write something
        if(low < high){
            if((low % 512) == 0 && (high % 512) == 0) { // Write full sector
                this->partition->device->read(this->partition->device, &buffer[bytes_read], 1,
                                              current_cluster + fat_partition->data_offset - 2);
                bytes_read += 512; // TODO - extract to constant
                }
            else{
                uint8_t temp_buffer[512];
                this->partition->device->read(this->partition->device, &temp_buffer[0], 1, current_cluster + fat_partition->data_offset - 2);
                k_memcpy(&temp_buffer[low % 512], &buffer[bytes_read], high - low);
                bytes_read += high - low;
            }
        }
        current_cluster = read_fat_value(this->partition, current_cluster);
    }
    return 0; // Means no error
}

// Private section
struct VFS_NODE*
fat12_list_dir   (struct VFS_NODE* this, uint32_t* size){
    struct FAT_NODE* node;
    uint32_t max_entries = 0;
    if(!this->FS_reference){
        node = list_root(&max_entries, this->partition);
    }else{
        node = list_dir(&max_entries, this);
    }

    *size = max_entries;

    struct FAT_NODE* temp_node = node;

    struct VFS_NODE* entries_safe = k_malloc(max_entries * sizeof(struct VFS_NODE));
    k_memset(entries_safe, max_entries * sizeof(struct VFS_NODE), 0x00);
    struct VFS_NODE* entries = entries_safe;

    for (int i = 0; i < max_entries; ++i) {
        if(temp_node->filename[0] == '\0') break;
        else if((uint8_t)temp_node->filename[0] == 0xe5);
        else {
            if((temp_node->attributes & FAT_DA_HIDDEN) == FAT_DA_HIDDEN) {
                temp_node++; continue;
            }
            *entries = fat_to_vfs(this->partition, temp_node, NULL);
            entries++;
        }
        temp_node++;
    }

    k_free(node);
    return entries_safe;
}

static struct FAT_NODE
resolve_directories(struct VFS_PARTITION *partition, char **path) {
    if(*path[0] == '/') (*path)++;
    struct FAT_NODE* node = &root_dir;
    int slash_pos;
    while((slash_pos = str_until(*path, '/'))){

        char temp_buffer[12] = "";
        k_memcpy(*path, &temp_buffer[0], MIN(12, slash_pos));
        fat_12_stat_resolve_path(&temp_buffer[0]);

        if(!node->start_low)
            node = resolve_root(partition, NULL);
        else
            node = resolve_dir(partition, node, NULL);

        if(node && (node->attributes & FAT_DA_DIR) == FAT_DA_DIR){
            *path += slash_pos + 1;
        }else{
            return empty_node();
        }
    }
    return *node;
}

static struct FAT_NODE
find_in_directory(struct VFS_PARTITION *partition, struct FAT_NODE *node, char *path, FAT_NODE_POSITION *position) {
    fat_12_stat_resolve_path(path);
    struct FAT_NODE* fs_node;
    if(!node->start_low)
        fs_node = resolve_root(partition, position);
    else
        fs_node = resolve_dir(partition, node, position);
    if(fs_node) return *fs_node;
    else return empty_node();
}

static struct FAT_NODE
empty_node(){
    struct FAT_NODE node = {};
    return node;
}

static struct VFS_NODE
fat_to_vfs(struct VFS_PARTITION *partition, struct FAT_NODE *fat_node, FAT_NODE_POSITION *position) {
    uint8_t buffer[sizeof(struct FAT_NODE)] = {};
    if(!k_memcmp((uint8_t *) &fat_node, &buffer[0], sizeof(struct FAT_NODE))) return vfs_empty_node();

    struct VFS_NODE v_node = {
            .partition = partition,
            .size = fat_node->size,
            .FS_reference = fat_node->start_low,
            .type = fat_node->attributes & FAT_DA_DIR ? DIRECTORY : FILE,
            .write_file = fat12_write_file,
            .read_file = fat12_read_file,
            .list_dir = fat12_list_dir,
            .node_data = position
    };
    str_cpy_size(&fat_node->filename[0], &v_node.name[0], 11);

    return v_node;
}

static int
is_node_empty(struct FAT_NODE *node) {
    uint8_t buffer[sizeof(struct FAT_NODE)] = {};
    return !k_memcmp((uint8_t *) node, &buffer[0], sizeof(struct FAT_NODE));
}

int fat12_remove_node(struct VFS_PARTITION *this, struct FAT_NODE *parent_node, char *name) {
    FAT12_PARTITION* partition = this->partition_data;
    char* resolved_name = fat_12_stat_resolve_path(name);
    if(parent_node->start_low) {
        uint32_t current_cluster = parent_node->start_low;
        while (0 < current_cluster && current_cluster < 0xff8) {
            struct FAT_NODE nodes[16];
            this->device->read(this->device, &nodes[0], 1, partition->data_offset - 2 + current_cluster);

            // Condition
            for (int i = 0; i < 16; ++i)
                if (!k_memcmp(&nodes[i], resolved_name, 11)) {
                    k_memset(&nodes[i], sizeof(struct FAT_NODE), 0x00);
                    k_memset(&nodes[i], 1, 0xE5);
                    int error = this->device->write(this->device, &nodes[0], 1, partition->data_offset - 2 + current_cluster);
                    k_free(resolved_name);
                    return error;
                }

            current_cluster = read_fat_value(this, current_cluster);
        }
    }
    else{
        for (int i = 0; i < partition->root_size; ++i) {
            struct FAT_NODE nodes[16];
            this->device->read(this->device, &nodes[0], 1, partition->root_offset + i);

            // Condition
            for (int j = 0; j < 16; ++j)
                if (!k_memcmp(&nodes[j], resolved_name, 11)) {
                    k_memset(&nodes[j], sizeof(struct FAT_NODE), 0x00);
                    k_memset(&nodes[j], 1, 0xE5);
                    int error = this->device->write(this->device, &nodes[0], 1, partition->root_offset + i);
                    k_free(resolved_name);
                    return error;
                }
        }
    }

    k_free(resolved_name);
    return E_ERROR | ED_FAT;
}

/*struct VFS_NODE fat12_write_file_o(struct VFS_NODE *parent_node, uint32_t size, uint8_t *buffer, char *name) {
    fat_12_stat_resolve_path(name);
    struct FAT_NODE* node;
    if(!parent_node->FS_reference) { // Root directory
        node = resolve_root(parent_node->partition);
    }else{ // Normal directory
        node = Vresolve_dir(parent_node);
    }

    // Edit file
    if(node) {
        if((node->attributes & FAT_DA_DIR) == FAT_DA_DIR) kprint("Not a file! \n");
        else if((node->attributes & FAT_DA_HIDDEN) == FAT_DA_HIDDEN) kprint("Couldn't access file!");
        else{
            uint32_t clusters = MAX((size >> 9) + (size % 512 == 511), 1);
            uint32_t* clusters_list = resize_chain(parent_node->partition, node->start_low, clusters);
            fill_chain(parent_node->partition, clusters_list, clusters, buffer);
            k_free(clusters_list);
        }

    }
        // Create file
    else {
        uint32_t clusters = MAX((size >> 9) + (size % 512 == 511), 1);
        uint32_t* clusters_list = reserve_chain(parent_node->partition, clusters);
        fill_chain(parent_node->partition, clusters_list, clusters, buffer);
        struct FAT_NODE new_node = { };
        fill_node(&new_node, &path_buffer[0], 0, *clusters_list, size);
        k_free(clusters_list);
        if(!parent_node->FS_reference) { // Root directory
            add_node_root(&new_node, parent_node->partition);
        }else{ // Normal directory
            add_node_dir(parent_node, &new_node);
        }

    }
}*/

/*void fat12_read_file(char *path, uint8_t* buffer){
    fat_12_stat_resolve_path(path);
    struct FAT_NODE* node;
    if(!current_node.start_low) { // Root directory
        node = resolve_root(NULL);
    }else{ // Normal directory
        node = resolve_dir(NULL, &current_node);
    }

    if(node){
        if((node->attributes & FAT_DA_DIR) == FAT_DA_DIR) kprint("Not a file!\n");
        else if((node->attributes & FAT_DA_HIDDEN) == FAT_DA_HIDDEN) kprint("Cannot find file!");
        else{
            uint32_t length;
            uint8_t* some_buffer = read_chain(0, node->start_low, &length);
            kprint((int8_t*)some_buffer);
            k_free(some_buffer);
        }
    }else kprint("File doesn't exist\n");
}*/

FAT12_PARTITION init_partition(FAT12_BOOT_RECORD* bootsector){
    FAT12_PARTITION partition = {
            bootsector->bpb.sectors_per_cluster,
            bootsector->bpb.reserved_sectors,
            bootsector->bpb.fats,
            bootsector->bpb.directory_entries,
            bootsector->bpb.sectors_in_volume < 0xffff ? bootsector->bpb.sectors_in_volume : bootsector->bpb.large_sectors_count,
            bootsector->bpb.sectors_per_fat,
            bootsector->bpb.hidden_sectors,
            .buffer = {}
    };

    partition.fat_offset  = bootsector->bpb.reserved_sectors;

    partition.root_offset = partition.reserved_sectors + partition.FATs * partition.sectors_per_fat;
    partition.root_size   = ((partition.root_entries * 32) + 511) / 512;

    partition.data_offset = partition.root_offset + partition.root_size;

    k_memcpy((int8_t*)&bootsector->ebr.volume_label_string, (int8_t*)&partition.label, 11);

    return partition;
}

char * fat_12_stat_resolve_path(char* path) {
    k_memset(&path_buffer[0], 13, 0x00);
    char* temp_buffer = k_malloc(13 * sizeof(char));
    k_memset(temp_buffer, 13, 0x00);

    // Special cases
    if(!str_cmp(path, "..")) {
        str_cpy("..", temp_buffer);
        k_memreplace(temp_buffer, 0, ' ', 11);
        str_cpy(temp_buffer, &path_buffer[0]);
        return temp_buffer;
    }

    if(!str_cmp(path, ".")) {
        str_cpy(".", temp_buffer);
        k_memreplace(temp_buffer, 0, ' ', 11);
        str_cpy(temp_buffer, &path_buffer[0]);
        return temp_buffer;
    }

    // Copy to buffer and make upper case
    str_cpy_size(path, temp_buffer, 12);
    str_upper_case(temp_buffer);

    // Work with dot
    int dot_position = str_until(temp_buffer, '.');
    if(dot_position) {
        str_trail_char(&temp_buffer[dot_position], '.');
        if(dot_position < 8)
            str_insert(&temp_buffer[dot_position], ' ', 8 - dot_position);
        else
            k_memset(&temp_buffer[8], dot_position - 8, ' ');
    }
    k_memreplace((uint8_t*)&temp_buffer[0], 0, ' ', 11);
    str_cpy(temp_buffer, &path_buffer[0]);
    return temp_buffer;
}

/*void get_directories_from(uint32_t lba){
    floppy_read_sector((uint8_t *) &node_buffer, lba);
}*/

struct FAT_NODE *resolve_node_buffer(FAT_NODE_POSITION *position) {
    for (int i = 0; i < 16; ++i) {
        struct FAT_NODE* node = &node_buffer[i];
        if(node->filename[0] == 0) return 0;
        if((uint8_t)node->filename[0] == 0xe5) continue;
        if(!k_memcmp((uint8_t*)&node->filename[0],(uint8_t*)&path_buffer[0], 11)) {
            if(position) position->offset = i;
            return node;
        }
    }
    return 0;
}

struct FAT_NODE *resolve_root(struct VFS_PARTITION *part, FAT_NODE_POSITION *position) {
    struct FAT_NODE* node;

    if(path_buffer[0] == 0x20) return &root_dir;
    FAT12_PARTITION* partition = part->partition_data;

    for (uint32_t i = 0; i < partition->root_size; ++i) {
        node_buffer_lba = partition->root_offset + i;
        part->device->read(part->device, (uint8_t *) &node_buffer, 1, node_buffer_lba);
        if((node = resolve_node_buffer(position))) {
            if(position) position->sector = partition->root_offset + i;
            return node;
        }
    }

    return 0;
}

struct FAT_NODE *resolve_dir(struct VFS_PARTITION *part, struct FAT_NODE *resolve_node, FAT_NODE_POSITION *position) {
    struct FAT_NODE* node;
    uint32_t current_cluster = resolve_node->start_low;
    FAT12_PARTITION* partition = part->partition_data;

    while(0 < current_cluster && current_cluster < 0xff8) {
        node_buffer_lba = partition->data_offset - 2 + current_cluster;
        part->device->read(part->device, (uint8_t *) &node_buffer, 1, node_buffer_lba);
        if((node = resolve_node_buffer(position))) {
            if(position) position->sector = node_buffer_lba;
            return node;
        }
        current_cluster = read_fat_value(part, current_cluster);
    }

    return 0;
}

struct FAT_NODE *list_root(uint32_t *entries, struct VFS_PARTITION *partition) {
    FAT12_PARTITION* fat_partition = partition->partition_data;
    struct FAT_NODE* node = k_malloc(fat_partition->root_size * 512);
    k_memset(node, fat_partition->root_size * 512, 0x00);
    partition->device->read(partition->device, (uint8_t *) node, fat_partition->root_size, fat_partition->root_offset);
    *entries = fat_partition->root_entries;
    return node;
}

struct FAT_NODE* list_dir(uint32_t* entries, struct VFS_NODE* resolve_node){
    struct FAT_NODE* node = (struct FAT_NODE *) read_chain(resolve_node->partition, resolve_node->FS_reference,
                                                           entries);
    *entries *= 16;

    return node;
}

int add_node_root(struct FAT_NODE *node, struct VFS_PARTITION *part) {
    // Prepare variables
    uint32_t entries;
    FAT12_PARTITION* partition = part->partition_data;

    struct FAT_NODE* root = list_root(&entries, part); // Needed for free
    struct FAT_NODE* root_node = root;
    struct FAT_NODE* place_to_write = 0;
    int i = 0;

    // Iterate until 0x00 - it means the end of directory
    for (; root_node->filename[0] != 0 ; root_node++, i++) {
        // If exists node with the same name - return
        if(!k_memcmp((uint8_t *) root_node, (uint8_t *) node, 11)) { k_free(root); return 0; }

        // If first byte 0xe5 than sign place to write
        if((uint8_t)root_node->filename[0] == 0xe5 && !place_to_write) place_to_write = root_node;
    }

    // Write a node if enough place in root folder
    if(!place_to_write && i < partition->root_entries) place_to_write = root_node;
    if(place_to_write) {
        k_memcpy(node, place_to_write, 32);
        part->device->write(part->device, (uint8_t *) root, partition->root_size, partition->root_offset);
    }
    k_free(root);

    return 1;
}

void add_node_dir(struct VFS_NODE* parent_node, struct FAT_NODE* node){
    // Setup data
    uint32_t length;
    uint32_t node_to_write = 0;
    FAT12_PARTITION* partition = parent_node->partition->partition_data;
    struct FAT_NODE* directories = (struct FAT_NODE *) read_chain(parent_node->partition, parent_node->FS_reference, &length);
    length <<= 4;
    int i;

    // Iterate over directory
    for (i = 0; i < length; ++i) {
        // If exists node with same name - return (and free memory)
        if(!k_memcmp((uint8_t*)&node->filename[0], (uint8_t*)&directories[i].filename[0], 11)){
            k_free(directories);
            return;
        }

        // If exists node with 0xE5 byte at the beginning - mark it as possible to write and no need for extension
        if((uint8_t)directories[i].filename[0] == 0xe5 && !node_to_write)
            node_to_write = i;

        // If exists node with 0x00 byte at the beginning - mark no need for extension
        if(directories[i].filename[0] == 0x00){
            if(!node_to_write) node_to_write = i;
            break;
        }
    }
    // If extension needed - add new cluster to the chain
    if(i >= length && !node_to_write){
        uint32_t* clusters_list = resize_chain(parent_node->partition, parent_node->FS_reference, (length >> 4) + 1);
        uint8_t buffer[512] = "";
        k_memcpy(node, &buffer[0], 32);
        uint32_t try = clusters_list[length >> 4];
        parent_node->partition->device->write(parent_node->partition->device, &buffer[0], 1, clusters_list[length >> 4] + partition->data_offset - 2);
        k_free(clusters_list);
    }
    // Copy node to parent node
    else{
        k_memcpy(node, directories + node_to_write, 32);
        fill_chain_sector_from_begin_offset(parent_node->partition, parent_node->FS_reference, directories + (node_to_write >> 4),
                                            node_to_write >> 4);
    }
    k_free(directories);
}

static uint8_t *read_chain(struct VFS_PARTITION *part, uint32_t cluster, uint32_t *length) {

    FAT12_PARTITION* partition = part->partition_data;
    // Array of clusters to read
    uint32_t* clusters_list = k_malloc(256 * sizeof(uint32_t));
    uint32_t current_cluster = clusters_list[0] = cluster;
    int i;
    for (i = 1; i < 256; ++i) {
        // Obtain new cluster value from current cluster
        current_cluster = clusters_list[i] = read_fat_value(part, current_cluster);
        if(0xff8 < current_cluster || current_cluster < 1) break;
    }

    // Read data from clusters
    *length = i;
    uint8_t* buffer = k_malloc(512 * i);
    k_memset(buffer, 512 * i, 0x00);
    for (int j = 0; j < i; ++j) {
        part->device->read(part->device, (buffer + 512 * j), 1, clusters_list[j] + partition->data_offset - 2);
    }

    k_free(clusters_list);

    return buffer;
}

static uint32_t *reserve_chain(struct VFS_PARTITION *part, uint32_t clusters) {
    uint32_t clusters_marked = 0;
    uint32_t current_cluster = 2;

    FAT12_PARTITION* partition = part->partition_data;

    uint32_t* clusters_list = k_malloc((clusters + 1) * sizeof(uint32_t));

    // Find empty clusters
    while(clusters_marked < clusters){
        if(current_cluster > partition->sectors) return 0;

        // Add cluster to list if empty
        if(!read_fat_value(part, current_cluster))
            clusters_list[clusters_marked++] = current_cluster;

        current_cluster++;
    }

    clusters_list[clusters_marked] = 0xfff; // To omit the special case

    // Update FAT
    for (int i = 0; i < clusters; ++i)
        write_fat_value(part, clusters_list[i], clusters_list[i + 1]);

    // Flash changes
    flush_fat_buffer(part);

    return clusters_list;
}

void fill_chain(struct VFS_PARTITION *part, uint32_t *clusters_list, uint32_t clusters, uint8_t *buffer) {
    FAT12_PARTITION* partition = part->partition_data;

    for (int i = 0; i < clusters; ++i)
        part->device->write(part->device, buffer, 1, partition->data_offset - 2 + clusters_list[i]);
}

void fill_chain_sector_from_begin_offset(struct VFS_PARTITION *part, uint32_t cluster, uint8_t *buffer, uint32_t offset) {
    FAT12_PARTITION* partition = part->partition_data;
    uint32_t current_cluster = cluster;
    int i;
    for (i = 0; i < offset; ++i) {
        // Obtain new cluster value from current cluster
        current_cluster = read_fat_value(part, current_cluster);
        if(0xff8 < current_cluster || current_cluster < 1) break;
    }

    part->device->write(part->device, buffer, 1, partition->data_offset - 2 + current_cluster);

}

static uint32_t *resize_chain(struct VFS_PARTITION *part, uint32_t cluster, uint32_t clusters) {
    // Read chain
    FAT12_PARTITION * partition = part->partition_data;
    uint32_t* clusters_list = k_malloc(256 * sizeof(uint32_t));
    uint32_t current_cluster = clusters_list[0] = cluster;
    int length;
    for (length = 1; length < 256; ++length) {
        // Obtain new cluster value from current cluster
        current_cluster = clusters_list[length] = read_fat_value(part, current_cluster);
        if(0xff8 < current_cluster || current_cluster < 1) break;
    }

    // If length < clusters than add new to list
    if(length < clusters){
        current_cluster = 2;
        while(length < clusters){
            if(current_cluster > partition->sectors) return 0;

            // Add cluster to list if empty
            if(!read_fat_value(part, current_cluster))
                clusters_list[length++] = current_cluster;

            current_cluster++;
        }

        clusters_list[length] = 0xfff;

        for (int i = 0; i < clusters; ++i)
            write_fat_value(part, clusters_list[i], clusters_list[i + 1]);

        // Flash changes
        flush_fat_buffer(part);

    }

    // If length > clusters than remove clusters from list
    while(length > clusters)
        write_fat_value(part, clusters_list[--length], 0x00);
    flush_fat_buffer(part);

    // Return full list of clusters
    return clusters_list;
}

static uint32_t cluster_FAT_offset(uint32_t cluster){
    return (cluster + (cluster / 2)) / 512;
}

static uint32_t get_entry_offset(struct VFS_PARTITION *part, uint32_t cluster) {
    FAT12_PARTITION * partition = part->partition_data;
    uint32_t sector_to_load = cluster_FAT_offset(cluster);
    if( (sector_to_load / 3) != (FAT_buffer.current_sector / 3) ){ // Check whether current FAT sectors are enough
        // Cache FAT changes if needed
        flush_fat_buffer(part);

        // Load new FAT
        FAT_buffer.current_sector = ((sector_to_load / 3) * 3);
        part->device->read(part->device, &FAT_buffer.buffer[0], 3, partition->fat_offset + FAT_buffer.current_sector);
    }

    // Calculate entry offset
    return (cluster + (cluster / 2) % (512 * 3));
}

static uint32_t read_fat_value(struct VFS_PARTITION *part, uint32_t cluster) {
    uint32_t offset = get_entry_offset(part, cluster);
    if(cluster & 1)
        return ((FAT_buffer.buffer[offset] & 0xf0) >> 4) | (FAT_buffer.buffer[offset + 1] << 4);
    else
        return FAT_buffer.buffer[offset] | ((FAT_buffer.buffer[offset + 1] & 0x0f) << 8);
}

static void write_fat_value(struct VFS_PARTITION *part, uint32_t cluster, uint32_t value) {
    uint32_t offset = get_entry_offset(part, cluster);
    if(cluster & 1){ // Odd
        FAT_buffer.buffer[offset] = ((value & 0x0f) << 4) | (FAT_buffer.buffer[offset] & 0x0f);
        FAT_buffer.buffer[offset + 1] = (value & 0xff0) >> 4;
    }else{           // Even
        FAT_buffer.buffer[offset] = value & 0xff;
        FAT_buffer.buffer[offset + 1] = (FAT_buffer.buffer[offset + 1] & 0xf0) | ((value & 0xf00) >> 8);
    }
    FAT_buffer.changed = 1;
}

static void flush_fat_buffer(struct VFS_PARTITION *part) {
    FAT12_PARTITION* partition = part->partition_data;
    if(FAT_buffer.changed){
        part->device->write(part->device, &FAT_buffer.buffer[0], 3, partition->fat_offset + FAT_buffer.current_sector);
        part->device->write(part->device, &FAT_buffer.buffer[0], 3,
                     partition->fat_offset + partition->sectors_per_fat + FAT_buffer.current_sector);
        FAT_buffer.changed = 0;
    }
}

static void fill_node(struct FAT_NODE *node, char *name, uint8_t attributes, uint32_t start_cluster, uint32_t size) {
    if(name) {
        str_cpy(name, &node->filename[0]);
        k_memreplace((uint8_t *) &node->filename[0], 0, ' ', 11);
    }
    node->attributes = attributes;
    node->start_low = start_cluster;
    node->size = size;
}
