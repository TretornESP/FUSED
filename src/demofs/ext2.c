#include "ext2.h"
//Supress -Waddress-of-packed-member warning
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#include "../fused/primitives.h"
#include "../fused/auxiliary.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

//Fix by FunkyWaddle: Inverted values
#define DIVIDE_ROUNDED_UP(x, y) ((x % y) ? ((x) + (y) - 1) / (y) : (x) / (y))
#define SWAP_ENDIAN_16(x) (((x) >> 8) | ((x) << 8))

struct ext2_partition * ext2_partition_head = 0x0;
uint32_t ext2_unique_id = EXT2_UNIQUE_START;

struct ext2_read_requirements {
    uint8_t valid;

    char disk_name[MAX_DISK_NAME_LENGTH]; //Ok
    uint32_t sector_size; //Ok
    uint32_t block_size; //Ok
    uint32_t partition_lba; //Ok
    uint32_t sectors_per_block; //Ok
    uint32_t ext2_version; //Ok
    struct ext2_inode_descriptor * inode;
};

void ext2_disk_from_partition(char * destination, const char * partition) {
    uint32_t partition_name = strlen(partition);
    //iterate partition backwards to find the last 'p' character
    for (int i = partition_name - 1; i >= 0; i--) {
        if (partition[i] == 'p') {
            //copy the partition name to the destination
            memcpy(destination, partition, i);
            destination[i] = 0;
            return;
        }
    }
}

uint8_t ext2_check_status(const char* disk) {
    int status = get_disk_status(disk);
    printf("[EXT2] Checking disk %s status\n", disk);
    if (status != STATUS_READY) {
        if (init_disk(disk) != OP_SUCCESS) {
            printf("[EXT2] Failed to initialize disk\n");
            return 0;
        }
        if (get_disk_status(disk) != STATUS_READY) {
            printf("[EXT2] Disk not ready\n");
            return 0;
        }
    }
    return 1;
}

struct ext2_partition * get_partition(const char * partno) {
    struct ext2_partition * partition = ext2_partition_head;
    while (partition != 0) {
        if (strcmp(partition->name, partno) == 0) {
            return partition;
        }
        partition = partition->next;
    }
    return 0;
}

void epoch_to_date(char* date, uint32_t epoch) {
    time_t t = epoch;
    struct tm *tm = localtime(&t);
    strftime(date, 32, "%Y-%m-%d %H:%M:%S", tm);
}

uint8_t * ext2_buffer_for_size(struct ext2_partition * partition, uint64_t size) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t blocks_for_size = DIVIDE_ROUNDED_UP(size, block_size);
    uint32_t buffer_size = blocks_for_size * block_size;

    return malloc(buffer_size);
}

void ext2_list_directory(struct ext2_partition* partition, const char * path) {
    uint32_t inode_number = ext2_path_to_inode(partition, path);
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * root_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);

    if (root_inode->i_mode & INODE_TYPE_DIR) {
        uint8_t *block_buffer = malloc(root_inode->i_size + block_size);
        ext2_read_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size);

        uint32_t parsed_bytes = 0;
        
        printf("[EXT2] Directory listing for %s]\n", path);
        while (parsed_bytes < root_inode->i_size) {
            struct ext2_directory_entry *entry = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            printf("[EXT2] ino: %d rec_len: %d name_len: %d file_type: %d name: %s\n", entry->inode, entry->rec_len, entry->name_len, entry->file_type, entry->name);
            parsed_bytes += entry->rec_len;
        }
       
        free(block_buffer);
    }
}

uint32_t ext2_inode_from_path_and_parent(struct ext2_partition* partition, uint32_t parent_inode, const char* path) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * root_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, parent_inode);
    if (root_inode->i_mode & INODE_TYPE_DIR) {
        uint8_t *block_buffer = malloc(root_inode->i_size + block_size);
        ext2_read_inode_bytes(partition, parent_inode, block_buffer, root_inode->i_size);
        uint32_t parsed_bytes = 0;

        while (parsed_bytes < root_inode->i_size) {
            struct ext2_directory_entry *entry = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            if (strcmp(entry->name, path) == 0) {
                return entry->inode;
            }
            parsed_bytes += entry->rec_len;
        }
       
        free(block_buffer);
    }

    return 0;
}

uint32_t ext2_get_unique_id() {
    //Check for overflow
    if (ext2_unique_id == 0xFFFFFFFF) {
        ext2_unique_id = EXT2_UNIQUE_START;
    } else {
        ext2_unique_id++;
    }

    return ext2_unique_id;
}

uint32_t ext2_path_to_inode(struct ext2_partition* partition, const char * path) {
    char * path_copy = malloc(strlen(path) + 1);
    strcpy(path_copy, path);
    char * token = strtok(path_copy, "/");
    uint32_t inode_index = EXT2_ROOT_INO_INDEX;
    while (token != 0) {
        inode_index = ext2_inode_from_path_and_parent(partition, inode_index, token);

        if (inode_index == 0) {
            return 0;
        }

        token = strtok(0, "/");
    }

    return inode_index;
}

uint64_t ext2_get_file_size(struct ext2_partition* partition, const char* path) {
    uint32_t inode_number = ext2_path_to_inode(partition, path);
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);
    return inode->i_size;
}

struct ext2_partition * register_ext2_partition(const char* disk, uint32_t lba) {
    if (!ext2_check_status(disk)) {
        return 0;
    }

    uint32_t sector_size;
    ioctl_disk(disk, IOCTL_GET_SECTOR_SIZE, &sector_size);

    printf("[EXT2] Disk %s is ready, sector size is %d\n", disk, sector_size);

    uint8_t superblock_buffer[1024];
    if (read_disk(disk, superblock_buffer, lba+SB_OFFSET_LBA, 2)) {
        printf("[EXT2] Failed to read superblock\n");
        return 0;
    }

    struct ext2_superblock * superblock = (struct ext2_superblock*)superblock_buffer;
    if (superblock->s_magic != EXT2_SUPER_MAGIC) {
        printf("[EXT2] Invalid superblock magic\n");
        return 0;
    }

    uint32_t block_size = 1024 << superblock->s_log_block_size;
    uint32_t sectors_per_block = DIVIDE_ROUNDED_UP(block_size, sector_size);
    printf("[EXT2] First superblock found at LBA %d\n", lba+SB_OFFSET_LBA);
    printf("[EXT2] Superblock magic valid, ext2 version: %d\n", superblock->s_rev_level);
    printf("[EXT2] Block size is %d\n", block_size);
    printf("[EXT2] Sectors per block: %d\n", sectors_per_block);
    printf("[EXT2] Blocks count: %d\n", superblock->s_blocks_count);
    printf("[EXT2] Inodes count: %d\n", superblock->s_inodes_count);
    printf("[EXT2] Blocks per group: %d\n", superblock->s_blocks_per_group);
    printf("[EXT2] Inodes per group: %d\n", superblock->s_inodes_per_group);

    uint8_t bgdt_block = (block_size == 1024) ? 2 : 1;

    uint32_t block_groups_first  = DIVIDE_ROUNDED_UP(superblock->s_blocks_count, superblock->s_blocks_per_group);
    uint32_t block_groups_second = DIVIDE_ROUNDED_UP(superblock->s_inodes_count, superblock->s_inodes_per_group);

    if (block_groups_first != block_groups_second) {
        printf("ext2: lock_groups_first != block_groups_second\n");
        printf("ext2: block_groups_first = %d, block_groups_second = %d\n", block_groups_first, block_groups_second);
        return 0;
    }
    printf("[EXT2] Block groups: %d\n", block_groups_first);
    printf("[EXT2] Checking if sb is valid in all block groups...\n");
    uint32_t blocks_per_group = superblock->s_blocks_per_group;
    uint32_t sectors_per_group = blocks_per_group * sectors_per_block;
    uint8_t dummy_sb_buffer[1024];
    for (uint32_t i = 0; i < block_groups_first; i++) {
        if (read_disk(disk, dummy_sb_buffer, lba+(i*sectors_per_group)+SB_OFFSET_LBA, 2)) {
            printf("[EXT2] Failed to read dummy superblock\n");
            return 0;
        }
        struct ext2_superblock * dummy_sb = (struct ext2_superblock*)dummy_sb_buffer;
        if (dummy_sb->s_magic != EXT2_SUPER_MAGIC) {
            printf("[EXT2] Invalid dummy superblock magic\n");
            return 0;
        }
    }
    printf("[EXT2] All %d superblocks are valid\n", block_groups_first);
    //TODO: Delete this sanity check
    uint32_t block_group_descriptors_size = DIVIDE_ROUNDED_UP(block_groups_first * sizeof(struct ext2_block_group_descriptor), sector_size);
    printf("[EXT2] Block group descriptors size: %d\n", block_group_descriptors_size);
    //TODO: End of sanity check

    void * block_group_descriptor_buffer = malloc(block_group_descriptors_size * sector_size);
    
    if (read_disk(disk, (uint8_t*)block_group_descriptor_buffer, lba+(sectors_per_block*bgdt_block), block_group_descriptors_size)) return 0;
    struct ext2_block_group_descriptor * block_group_descriptor = (struct ext2_block_group_descriptor*)block_group_descriptor_buffer;

    printf("[EXT2] Registering partition %s:%d\n", disk, lba);

    struct ext2_partition * partition = ext2_partition_head;
    uint32_t partition_id = 0;

    if (partition == 0) {
        ext2_partition_head = malloc(sizeof(struct ext2_partition));
        partition = ext2_partition_head;
    } else {
        while (partition->next != 0) {
            partition = partition->next;
            partition_id++;
        }
        partition->next = malloc(sizeof(struct ext2_partition));
        partition = partition->next;
    }

    snprintf(partition->name, 32, "%sp%d", disk, partition_id);
    snprintf(partition->disk, 32, "%s", disk);
    partition->group_number = block_groups_first;
    partition->lba = lba;
    partition->sector_size = sector_size;
    partition->sb = malloc(1024);
    partition->sb_block = SB_OFFSET_LBA;
    partition->bgdt_block = bgdt_block; 
    memcpy(partition->sb, superblock, 1024);
    partition->gd = malloc(block_group_descriptors_size * sector_size);
    memcpy(partition->gd, block_group_descriptor, block_group_descriptors_size * sector_size);

    printf("[EXT2] Partition %s has: %d groups\n", partition->name, block_groups_first);

    return partition;
}

uint8_t ext2_dump_bg(struct ext2_partition* partition, struct ext2_block_group_descriptor * bg, uint32_t id) {
    (void)partition;
    printf("Block group %d:\n", id);
    printf("  Block bitmap: %d\n", bg->bg_block_bitmap);
    printf("  Inode bitmap: %d\n", bg->bg_inode_bitmap);
    printf("  Inode table: %d\n", bg->bg_inode_table);
    printf("  Free blocks: %d\n", bg->bg_free_blocks_count);
    printf("  Free inodes: %d\n", bg->bg_free_inodes_count);
    printf("  Directories: %d\n", bg->bg_used_dirs_count);
    return 0;
}

uint8_t ext2_bg_has_free_inodes(struct ext2_partition * partition, struct ext2_block_group_descriptor * bg, uint32_t id) {
    (void)id;
    (void)partition;
    return bg->bg_free_inodes_count > 0;
}

uint8_t ext2_flush_bg(struct ext2_partition* partition, struct ext2_block_group_descriptor* bg, uint32_t bgid) {
    uint32_t block_size = 1024 << ((struct ext2_superblock*)(partition->sb))->s_log_block_size;

    uint32_t block_group_descriptors_size = DIVIDE_ROUNDED_UP(partition->group_number * sizeof(struct ext2_block_group_descriptor), partition->sector_size);
    uint32_t sectors_per_group = ((struct ext2_superblock*)(partition->sb))->s_blocks_per_group * (block_size / partition->sector_size);
    write_disk(partition->disk, (uint8_t*)bg, partition->lba+(sectors_per_group*bgid)+partition->bgdt_block, block_group_descriptors_size);
    return 0;
}

uint8_t ext2_flush_sb(struct ext2_partition* partition, struct ext2_block_group_descriptor* bg, uint32_t bgid) {
    (void)bg;
    uint32_t block_size = 1024 << ((struct ext2_superblock*)(partition->sb))->s_log_block_size;
    uint32_t sectors_per_group = ((struct ext2_superblock*)(partition->sb))->s_blocks_per_group * (block_size / partition->sector_size);

    write_disk(partition->disk, (uint8_t*)partition->sb, partition->lba+(sectors_per_group*bgid)+partition->sb_block, 2);
    return 0;
}

int32_t ext2_operate_on_bg(struct ext2_partition * partition, uint8_t (*callback)(struct ext2_partition *, struct ext2_block_group_descriptor*, uint32_t)) {
    uint32_t i;
    for (i = 0; i < partition->group_number; i++) {
        if (callback(partition, &partition->gd[i], i)) 
            return (int32_t)i;
    }

    return -1;
}

uint8_t ext2_flush_structures(struct ext2_partition * partition) {
    printf("[EXT2] Flushing structures for partition %s\n", partition->name);
    ext2_operate_on_bg(partition, ext2_flush_bg);
    ext2_operate_on_bg(partition, ext2_flush_sb);
    printf("[EXT2] Flushed structures for partition %s\n", partition->name);
    return 0;
}

uint32_t ext2_count_partitions() {
    uint32_t count = 0;
    struct ext2_partition * partition = ext2_partition_head;

    while (partition != 0) {
        count++;
        partition = partition->next;
    }
    return count;
}

struct ext2_partition * ext2_get_partition_by_index(uint32_t index) {
    struct ext2_partition * partition = ext2_partition_head;
    uint32_t partition_id = 0;

    while (partition != 0) {
        if (partition_id == index) {
            return partition;
        }
        partition = partition->next;
        partition_id++;
    }
    return 0;
}

void ext2_print_inode(struct ext2_inode_descriptor_generic* inode) {
    char atime[32];
    char ctime[32];
    char mtime[32];
    char dtime[32];

    epoch_to_date(atime, inode->i_atime);
    epoch_to_date(ctime, inode->i_ctime);
    epoch_to_date(mtime, inode->i_mtime);
    epoch_to_date(dtime, inode->i_dtime);

    printf("[EXT2] Inode dump start ----\n");
    printf("i_mode: %d\n", inode->i_mode);
    printf("i_uid: %d\n", inode->i_uid);
    printf("i_size: %d\n", inode->i_size);
    printf("i_atime: %s\n", atime);
    printf("i_ctime: %s\n", ctime);
    printf("i_mtime: %s\n", mtime);
    printf("i_dtime: %s\n", dtime);
    printf("i_gid: %d\n", inode->i_gid);
    printf("i_links_count: %d\n", inode->i_links_count);
    printf("i_sectors: %d\n", inode->i_sectors);
    printf("i_flags: %d\n", inode->i_flags);
    printf("i_osd1: %d\n", inode->i_osd1);
    printf("i_generation: %d\n", inode->i_generation);
    printf("i_file_acl: %d\n", inode->i_file_acl);
    printf("i_dir_acl: %d\n", inode->i_dir_acl);
    printf("i_faddr: %d\n", inode->i_faddr);
    printf("[EXT2] Inode dump end ----\n");

}

uint8_t ext2_search(const char* name, uint32_t lba) {
    uint8_t bpb[1024];
    if (read_disk(name, bpb, lba+2, 2)) return 0;

    struct ext2_superblock *sb = (struct ext2_superblock *)bpb;
    return (sb->s_magic == EXT2_SUPER_MAGIC);
}

uint8_t unregister_ext2_partition(char letter) {
    (void)letter;
    printf("[EXT2] unregstr_ext2_partition Not implemented\n");
    return 0;
}

void ext2_debug_print_file_inode(struct ext2_partition* partition, uint32_t inode_number) {
    if (inode_number == 0) {
        printf("[EXT2] Failed to get inode number for %d\n", inode_number);
        return;
    }

    struct ext2_inode_descriptor * inode = ext2_read_inode(partition, inode_number);
    if (inode == 0) {
        printf("[EXT2] Failed to read inode %d\n", inode_number);
    }

    ext2_print_inode((struct ext2_inode_descriptor_generic*)inode);
}

struct ext2_inode_descriptor * ext2_initialize_inode(struct ext2_partition* partition, uint32_t inode_number, uint32_t type) {
    struct ext2_inode_descriptor * inode_descriptor = ext2_read_inode(partition, inode_number);
    struct ext2_inode_descriptor_generic * inode = &(inode_descriptor->id);
    if (inode == 0) {
        printf("[EXT2] Failed to allocate memory for inode\n");
        return 0;
    }

    uint32_t unique_value = ext2_get_unique_id();
    if (unique_value == 0) {
        printf("[EXT2] Failed to get unique id\n");
        return 0;
    }

    inode->i_mode = type;
    inode->i_uid = get_uid();   
    inode->i_size = 0;
    inode->i_atime = get_current_epoch(); 
    inode->i_ctime = inode->i_atime;
    inode->i_mtime = inode->i_atime;
    inode->i_dtime = 0; 
    inode->i_gid = get_gid();
    inode->i_links_count = 1;
    inode->i_sectors = 0;
    inode->i_flags = 0;
    inode->i_osd1 = 0;    
    
    memset(inode->i_block, 0, sizeof(uint32_t) * 15);

    inode->i_generation = unique_value;
    inode->i_file_acl = 0;
    inode->i_dir_acl = 0; 
    inode->i_faddr = 0;  

    return inode_descriptor;
}

uint8_t ext2_write_inode(struct ext2_partition* partition, uint32_t inode_number, struct ext2_inode_descriptor* inode) {
    struct ext2_superblock * superblock = (struct ext2_superblock*)partition->sb;
    struct ext2_superblock_extended * superblock_extended = (struct ext2_superblock_extended*)partition->sb;

    uint32_t block_size = 1024 << superblock->s_log_block_size;
    uint32_t sectors_per_block = DIVIDE_ROUNDED_UP(block_size, partition->sector_size);
    uint32_t inode_size = (superblock->s_rev_level < 1) ? 128 : superblock_extended->s_inode_size;

    uint32_t inode_group = (inode_number - 1 ) / superblock->s_inodes_per_group;
    uint32_t inode_index = (inode_number - 1 ) % superblock->s_inodes_per_group;
    uint32_t inode_block = (inode_index * inode_size) / (block_size);
    uint32_t inode_table_block = partition->gd[inode_group].bg_inode_table;
    uint32_t inode_table_lba = (inode_table_block * block_size) / partition->sector_size;
    uint8_t * root_inode_buffer = malloc(block_size);

    if (read_disk(partition->disk, root_inode_buffer, partition->lba + inode_table_lba + inode_block*sectors_per_block, sectors_per_block)) {
        printf("[EXT2] Root inode read failed\n");
        free(root_inode_buffer);
        return 1;
    }

    uint32_t inode_offset = (inode_index * inode_size) % block_size;
    memcpy(root_inode_buffer + inode_offset, inode, inode_size);

    if (write_disk(partition->disk, root_inode_buffer, partition->lba + inode_table_lba + inode_block*sectors_per_block, sectors_per_block)) {
        printf("[EXT2] Root inode write failed\n");
        free(root_inode_buffer);
        return 1;
    }

    free(root_inode_buffer);

    return 0;
}

struct ext2_inode_descriptor * ext2_read_inode(struct ext2_partition* partition, uint32_t inode_number) {
    
    struct ext2_superblock * superblock = (struct ext2_superblock*)partition->sb;
    struct ext2_superblock_extended * superblock_extended = (struct ext2_superblock_extended*)partition->sb;

    uint32_t block_size = 1024 << superblock->s_log_block_size;
    uint32_t sectors_per_block = DIVIDE_ROUNDED_UP(block_size, partition->sector_size);
    uint32_t inode_size = (superblock->s_rev_level < 1) ? 128 : superblock_extended->s_inode_size;

    uint32_t inode_group = (inode_number - 1 ) / superblock->s_inodes_per_group;
    uint32_t inode_index = (inode_number - 1 ) % superblock->s_inodes_per_group;
    uint32_t inode_block = (inode_index * inode_size) / (block_size);
    uint32_t inode_table_block = partition->gd[inode_group].bg_inode_table;
    uint32_t inode_table_lba = (inode_table_block * block_size) / partition->sector_size;
    uint8_t * root_inode_buffer = malloc(block_size);
    if (read_disk(partition->disk, root_inode_buffer, partition->lba + inode_table_lba + inode_block*sectors_per_block, sectors_per_block)) {
        printf("[EXT2] Root inode read failed\n");
        free(root_inode_buffer);
        return 0;
    }

    struct ext2_inode_descriptor * inode = malloc(inode_size);

    memcpy(inode, root_inode_buffer + (((inode_index * inode_size) % block_size)), inode_size);

    free(root_inode_buffer);

    return inode;
}

int64_t ext2_read_block(struct ext2_partition* partition, uint32_t block, uint8_t * destination_buffer) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t block_sectors = DIVIDE_ROUNDED_UP(block_size, partition->sector_size);
    uint32_t block_lba = (block * block_size) / partition->sector_size;

    if (block) {
        if (read_disk(partition->disk, destination_buffer, partition->lba + block_lba, block_sectors)) {
            printf("[EXT2] Failed to read block %d\n", block);
            return EXT2_READ_FAILED;
        }
    } else {
        printf("[EXT2] Attempted to read block 0\n");
        return 0;
    }

    return 1;
}

int64_t ext2_read_direct_blocks(struct ext2_partition* partition, uint32_t * blocks, uint32_t max, uint8_t * destination_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint64_t blocks_read = 0;

    for (uint32_t i = 0; i < max; i++) {
        int64_t read_result = ext2_read_block(partition, blocks[i], destination_buffer);

        if (read_result == EXT2_READ_FAILED) {
            return EXT2_READ_FAILED;
        } else if (read_result == 0) {
            return blocks_read;
        }

        destination_buffer += block_size;
        blocks_read += read_result;
        
        if (blocks_read == count) {
            break;
        }

    }

    return blocks_read;
}

int64_t ext2_read_indirect_blocks(struct ext2_partition* partition, uint32_t * blocks, uint32_t max, uint8_t *destination_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t entries_per_block = block_size / 4; //TODO: Abstract for n case
    uint64_t blocks_read = 0;

    for (uint32_t i = 0; i < max; i++) {
        uint32_t * indirect_block = (uint32_t*)ext2_buffer_for_size(partition, block_size);
        if (blocks[i] == 0) printf("[WARNING!!!!] Single indirect block is 0\n");

        if (ext2_read_block(partition, blocks[i], (uint8_t*)indirect_block) <= 0) {
            printf("[EXT2] Failed to read indirect block\n");
            free(indirect_block);
            return EXT2_READ_FAILED;
        }

        int64_t read_result = ext2_read_direct_blocks(partition, indirect_block, entries_per_block, destination_buffer, count - blocks_read);
        free(indirect_block);
        if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;

        destination_buffer += (read_result * block_size);
        blocks_read += read_result;

        if (blocks_read == count) {
            break;
        }
    }

    return blocks_read;
}

int64_t ext2_read_double_indirect_blocks(struct ext2_partition* partition, uint32_t * blocks, uint32_t max, uint8_t *destination_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t entries_per_block = block_size / 4;
    uint32_t blocks_read = 0;

    for (uint32_t i = 0; i < max; i++) {
        uint32_t * double_indirect_block = (uint32_t*)ext2_buffer_for_size(partition, block_size);
        if (blocks[i] == 0) printf("[WARNING!!!!] Double indirect block is 0\n");
        if (ext2_read_block(partition, blocks[i], (uint8_t*)double_indirect_block) <= 0) {
            printf("[EXT2] Failed to read double indirect block\n");
            free(double_indirect_block);
            return EXT2_READ_FAILED;
        }
        int64_t read_result = ext2_read_indirect_blocks(partition, double_indirect_block, entries_per_block, destination_buffer, count - blocks_read);
        free(double_indirect_block);

        if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;


        destination_buffer += (read_result * block_size);
        blocks_read += read_result;

        if (blocks_read == count) {
            break;
        }
    }

    return blocks_read;
}

int64_t ext2_read_triple_indirect_blocks(struct ext2_partition* partition, uint32_t * blocks, uint32_t max, uint8_t *destination_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t entries_per_block = block_size / 4;
    uint32_t blocks_read = 0;

    for (uint32_t i = 0; i < max; i++) {
        uint32_t * triple_indirect_block = (uint32_t*)ext2_buffer_for_size(partition, block_size);
        if (blocks[i] == 0) printf("[WARNING!!!!] Triple indirect block is 0\n");
        if (ext2_read_block(partition, blocks[i], (uint8_t*)triple_indirect_block) <= 0) {
            printf("[EXT2] Failed to read triple indirect block\n");
            free(triple_indirect_block);
            return EXT2_READ_FAILED;
        }

        int64_t read_result = ext2_read_double_indirect_blocks(partition, triple_indirect_block, entries_per_block, destination_buffer, count - blocks_read);
        free(triple_indirect_block);

        if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;

        destination_buffer += (read_result * block_size);
        blocks_read += read_result;

        if (blocks_read == count) {
            break;
        }
    }

    return blocks_read;
}

int64_t ext2_read_inode_blocks(struct ext2_partition* partition, uint32_t inode_number, uint8_t * destination_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);
    if (inode == 0) return EXT2_READ_FAILED;

    uint32_t blocks_read = 0;
    int64_t read_result = 0;
    printf("[EXT2] First file block: %d\n", inode->i_block[0]);
    read_result = ext2_read_direct_blocks(partition, inode->i_block, 12, destination_buffer, count);
    if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;
    blocks_read += read_result;
    destination_buffer += read_result * block_size;
    printf("[EXT2] Basic read %d blocks, max: %ld\n", blocks_read, count);
    if (blocks_read >= count) return blocks_read;

    printf("[EXT2] Reading indirect block\n");
    read_result = ext2_read_indirect_blocks(partition, &(inode->i_block[12]), 1, destination_buffer, count - blocks_read);
    if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;
    blocks_read += read_result;
    destination_buffer += read_result * block_size;
    printf("[EXT2] Indirect read %d blocks, max: %ld\n", blocks_read, count);
    if (blocks_read >= count) return blocks_read;

    printf("[EXT2] Reading double indirect block\n");
    read_result = ext2_read_double_indirect_blocks(partition, &(inode->i_block[13]), 1, destination_buffer, count - blocks_read);
    if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;
    blocks_read += read_result;
    destination_buffer += read_result * block_size;
    printf("[EXT2] Double indirect read %d blocks, max: %ld\n", blocks_read, count);
    if (blocks_read >= count) return blocks_read;

    printf("[EXT2] Reading triple indirect block\n");
    read_result = ext2_read_triple_indirect_blocks(partition, &(inode->i_block[14]), 1, destination_buffer, count - blocks_read);
    if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;
    blocks_read += read_result;
    destination_buffer += read_result * block_size;
    printf("[EXT2] Triple indirect read %d blocks, max: %ld\n", blocks_read, count);
    if (blocks_read >= count) return blocks_read;

    printf("[EXT2] File is too big for block size: %d \n", block_size);
    return EXT2_READ_FAILED;
}

int64_t ext2_read_inode_bytes(struct ext2_partition* partition, uint32_t inode_number, uint8_t * destination_buffer, uint64_t count) {
    uint64_t blocks = DIVIDE_ROUNDED_UP(count, 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
    int64_t result = ext2_read_inode_blocks(partition, inode_number, destination_buffer, blocks);
    if (result == EXT2_READ_FAILED) return EXT2_READ_FAILED;
    return result * (1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
}

//TODO: This way of skipping bytes is an aberration!
uint8_t ext2_read_file(struct ext2_partition * partition, const char * path, uint8_t * destination_buffer, uint64_t size, uint64_t skip) {
    uint32_t inode_index = ext2_path_to_inode(partition, path);
    if (!inode_index) {printf("err ino index\n");return 0;}

    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_index);
    if (inode->i_mode & INODE_TYPE_DIR) {printf("err ino type\n");return 0;}

    uint64_t file_size = inode->i_size;
    uint8_t * full_buffer = ext2_buffer_for_size(partition, file_size);

    int64_t read_bytes = ext2_read_inode_bytes(partition, inode_index, full_buffer, file_size);
    if (read_bytes == EXT2_READ_FAILED) {printf("err file read bytes\n");return 0;}
    if (read_bytes <= 0) {printf("err read bytes\n");return 0;}

    memcpy(destination_buffer, full_buffer + skip, size);
    free(full_buffer);
    return 1;
}

//WRITE LOGIC

int64_t ext2_write_block(struct ext2_partition* partition, uint32_t block, uint8_t * source_buffer) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t block_sectors = DIVIDE_ROUNDED_UP(block_size, partition->sector_size);
    uint32_t block_lba = (block * block_size) / partition->sector_size;

    if (block) {
        if (write_disk(partition->disk, source_buffer, partition->lba + block_lba, block_sectors)) {
            printf("[EXT2] Failed to write block %d\n", block);
            return EXT2_WRITE_FAILED;
        }
    } else {
        printf("[EXT2] Attempted to write block 0\n");
        return 0;
    }

    return 1;
}

int64_t ext2_write_direct_blocks(struct ext2_partition* partition, uint32_t * blocks, uint32_t max, uint8_t * source_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint64_t blocks_written = 0;

    for (uint32_t i = 0; i < max; i++) {
        int64_t write_result = ext2_write_block(partition, blocks[i], source_buffer);

        if (write_result == EXT2_WRITE_FAILED) {
            return EXT2_WRITE_FAILED;
        } else if (write_result == 0) {
            return blocks_written;
        }

        source_buffer += block_size;
        blocks_written += write_result;
        
        if (blocks_written == count) {
            break;
        }

    }

    return blocks_written;
}

int64_t ext2_write_indirect_blocks(struct ext2_partition* partition, uint32_t * blocks, uint32_t max, uint8_t *source_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t entries_per_block = block_size / 4; //TODO: Abstract for n case
    uint64_t blocks_written = 0;

    for (uint32_t i = 0; i < max; i++) {
        uint32_t * indirect_block = (uint32_t*)ext2_buffer_for_size(partition, block_size);
        if (blocks[i] == 0) printf("[WARNING!!!!] Single indirect block is 0\n");

        if (ext2_read_block(partition, blocks[i], (uint8_t*)indirect_block) <= 0) {
            printf("[EXT2] Failed to read indirect block\n");
            free(indirect_block);
            return EXT2_WRITE_FAILED;
        }

        int64_t write_result = ext2_write_direct_blocks(partition, indirect_block, entries_per_block, source_buffer, count - blocks_written);
        free(indirect_block);
        if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;

        source_buffer += (write_result * block_size);
        blocks_written += write_result;

        if (blocks_written == count) {
            break;
        }
    }

    return blocks_written;
}

int64_t ext2_write_double_indirect_blocks(struct ext2_partition* partition, uint32_t * blocks, uint32_t max, uint8_t *source_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t entries_per_block = block_size / 4;
    uint32_t blocks_written = 0;

    for (uint32_t i = 0; i < max; i++) {
        uint32_t * double_indirect_block = (uint32_t*)ext2_buffer_for_size(partition, block_size);
        if (blocks[i] == 0) printf("[WARNING!!!!] Double indirect block is 0\n");
        if (ext2_read_block(partition, blocks[i], (uint8_t*)double_indirect_block) <= 0) {
            printf("[EXT2] Failed to read double indirect block\n");
            free(double_indirect_block);
            return EXT2_WRITE_FAILED;
        }
        int64_t write_result = ext2_write_indirect_blocks(partition, double_indirect_block, entries_per_block, source_buffer, count - blocks_written);
        free(double_indirect_block);

        if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;


        source_buffer += (write_result * block_size);
        blocks_written += write_result;

        if (blocks_written == count) {
            break;
        }
    }

    return blocks_written;
}

int64_t ext2_write_triple_indirect_blocks(struct ext2_partition* partition, uint32_t * blocks, uint32_t max, uint8_t *source_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t entries_per_block = block_size / 4;
    uint32_t blocks_written = 0;

    for (uint32_t i = 0; i < max; i++) {
        uint32_t * triple_indirect_block = (uint32_t*)ext2_buffer_for_size(partition, block_size);
        if (blocks[i] == 0) printf("[WARNING!!!!] Triple indirect block is 0\n");
        if (ext2_read_block(partition, blocks[i], (uint8_t*)triple_indirect_block) <= 0) {
            printf("[EXT2] Failed to read triple indirect block (for write)\n");
            free(triple_indirect_block);
            return EXT2_WRITE_FAILED;
        }

        int64_t write_result = ext2_write_double_indirect_blocks(partition, triple_indirect_block, entries_per_block, source_buffer, count - blocks_written);
        free(triple_indirect_block);

        if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;

        source_buffer += (write_result * block_size);
        blocks_written += write_result;

        if (blocks_written == count) {
            break;
        }
    }

    return blocks_written;
}

int64_t ext2_write_inode_blocks(struct ext2_partition* partition, uint32_t inode_number, uint8_t * source_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);
    if (inode == 0) return EXT2_WRITE_FAILED;

    uint32_t blocks_written = 0;
    int64_t write_result = 0;

    printf("[EXT2] Writing basic blocks %d\n", inode->i_block[0]);
    write_result = ext2_write_direct_blocks(partition, inode->i_block, 12, source_buffer, count);
    if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;
    blocks_written += write_result;
    source_buffer += write_result * block_size;
    printf("[EXT2] Basic write %d blocks, max: %ld\n", blocks_written, count);
    if (blocks_written >= count) return blocks_written;

    printf("[EXT2] Writing indirect block\n");
    write_result = ext2_write_indirect_blocks(partition, &(inode->i_block[12]), 1, source_buffer, count - blocks_written);
    if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;
    blocks_written += write_result;
    source_buffer += write_result * block_size;
    printf("[EXT2] Indirect write %d blocks, max: %ld\n", blocks_written, count);
    if (blocks_written >= count) return blocks_written;

    printf("[EXT2] Writing double indirect block\n");
    write_result = ext2_write_double_indirect_blocks(partition, &(inode->i_block[13]), 1, source_buffer, count - blocks_written);
    if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;
    blocks_written += write_result;
    source_buffer += write_result * block_size;
    printf("[EXT2] Double indirect write %d blocks, max: %ld\n", blocks_written, count);
    if (blocks_written >= count) return blocks_written;

    printf("[EXT2] Writing triple indirect block\n");
    write_result = ext2_write_triple_indirect_blocks(partition, &(inode->i_block[14]), 1, source_buffer, count - blocks_written);
    if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;
    blocks_written += write_result;
    source_buffer += write_result * block_size;
    printf("[EXT2] Triple indirect write  %d blocks, max: %ld\n", blocks_written, count);
    if (blocks_written >= count) return blocks_written;

    printf("[EXT2] File is too big for block size: %d \n", block_size);
    return EXT2_WRITE_FAILED;
}

int64_t ext2_write_inode_bytes(struct ext2_partition* partition, uint32_t inode_number, uint8_t * source_buffer, uint64_t count) {
    uint64_t blocks = DIVIDE_ROUNDED_UP(count, 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
    printf("[EXT2] Writing %ld bytes to inode %d (%ld blocks)\n", count, inode_number, blocks);    
    int64_t result = ext2_write_inode_blocks(partition, inode_number, source_buffer, blocks);
    if (result == EXT2_WRITE_FAILED) return EXT2_WRITE_FAILED;
    return result * (1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
}

void ext2_dump_inode_bitmap(struct ext2_partition * partition) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t block_group_count = partition->group_number;
    
    for (uint32_t i = 0; i < block_group_count; i++) {
        struct ext2_block_group_descriptor * bgd = (struct ext2_block_group_descriptor *)&partition->gd[i];
        uint8_t * bitmap = (uint8_t*)ext2_buffer_for_size(partition, block_size);
        if (ext2_read_block(partition, bgd->bg_inode_bitmap, bitmap) <= 0) {
            printf("[EXT2] Failed to read inode bitmap\n");
            free(bitmap);
            return;
        }

        printf("[EXT2] Inode bitmap for block group %d\n", i);
        for (uint32_t j = 0; j < block_size; j++) {
            printf("%02x ", bitmap[j]);
        }
        printf("\n");

        free(bitmap);
    }
}

uint32_t ext2_allocate_inode(struct ext2_partition * partition) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);

    //ext2_operate_on_bg(partition, ext2_dump_bg);
    //ext2_dump_inode_bitmap(partition);

    int32_t ext2_block_group_id = ext2_operate_on_bg(partition, ext2_bg_has_free_inodes);
    if (ext2_block_group_id == -1) {
        printf("[EXT2] No block group with free inodes\n");
        return 0;
    }

    struct ext2_block_group_descriptor * bgd = (struct ext2_block_group_descriptor *)&partition->gd[ext2_block_group_id];

    if (bgd == 0) {
        printf("[EXT2] No bg with free inodes\n");
        return 0;
    }

    printf("[EXT2] Found bg with free inodes\n");

    uint8_t * inode_bitmap = (uint8_t*)ext2_buffer_for_size(partition, block_size);
    if (ext2_read_block(partition, bgd->bg_inode_bitmap, inode_bitmap) <= 0) {
        printf("[EXT2] Failed to read inode bitmap\n");
        free(inode_bitmap);
        return 0;
    }

    uint32_t inode_number = 0;
    for (uint32_t i = 0; i < block_size; i++) {
        if (inode_bitmap[i] != 0xFF) {
            for (uint32_t j = 0; j < 8; j++) {
                if ((inode_bitmap[i] & (1 << j)) == 0) {
                    inode_number = i * 8 + j + 1;
                    break;
                }
            }
            break;
        }
    }

    //0f 00001111
    //2f 00101111
    if (inode_number == 0) {
        printf("[EXT2] No free inodes\n");
        free(inode_bitmap);
        return 0;
    }

    printf("[EXT2] Found free inode: %d\n", inode_number);

    inode_bitmap[(inode_number-1) / 8] |= 1 << (inode_number-1) % 8;

    if (ext2_write_block(partition, bgd->bg_inode_bitmap, inode_bitmap) <= 0) {
        printf("[EXT2] Failed to write inode bitmap\n");
        free(inode_bitmap);
        return 0;
    }

    bgd->bg_free_inodes_count--;

    //Update superblock
    struct ext2_superblock_extended * sb = (struct ext2_superblock_extended*)partition->sb;
    ((struct ext2_superblock*)sb)->s_free_inodes_count--;

    //Update partition
    partition->sb = sb;
    partition->gd[ext2_block_group_id] = *bgd;

    ext2_flush_structures(partition);
    free(inode_bitmap);

    return inode_number;
}

uint8_t ext2_create_directory_entry(struct ext2_partition* partition, uint32_t inode_number, uint32_t child_inode, const char* name, uint32_t type) {
    printf("[EXT2] Creating directory entry for inode %d\n", child_inode);
    printf("[EXT2] Parent inode: %d\n", inode_number);
    printf("[EXT2] Name: %s\n", name);
    printf("[EXT2] Type: %d\n", type);

    struct ext2_inode_descriptor_generic * root_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);
    
    struct ext2_directory_entry child_entry = {
        .inode = child_inode,
        .rec_len = 0,
        .name_len = strlen(name),
        .file_type = type
    };

    uint32_t entry_size = sizeof(struct ext2_directory_entry) + child_entry.name_len - EXT2_NAME_LEN;
    child_entry.rec_len = (entry_size + 3) & ~3;

    if (root_inode->i_mode & INODE_TYPE_DIR) {
        uint8_t *block_buffer = malloc(root_inode->i_size);
        ext2_read_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size);

        uint32_t parsed_bytes = 0;
        
        struct ext2_directory_entry *entry = 0;
        while (parsed_bytes < root_inode->i_size) {
            entry = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            parsed_bytes += entry->rec_len;
        }

        if (entry == 0) {
            printf("[EXT2] No directory entries\n");
            free(block_buffer);
            return 1;
        }

        uint32_t entry_size = sizeof(struct ext2_directory_entry) + entry->name_len - EXT2_NAME_LEN;
        uint32_t entry_size_aligned = (entry_size + 3) & ~3;

        if (entry_size_aligned + child_entry.rec_len > entry->rec_len) {
            printf("[EXT2] No space for directory entry\n");
            free(block_buffer);
            return 1;
        }

        uint32_t original_rec_len = entry->rec_len;
        entry->rec_len = entry_size_aligned;
        child_entry.rec_len = original_rec_len - entry_size_aligned;

        memcpy(block_buffer + parsed_bytes - original_rec_len, entry, entry_size);
        memcpy(block_buffer + parsed_bytes - original_rec_len + entry_size_aligned, &child_entry, sizeof(struct ext2_directory_entry) - EXT2_NAME_LEN);
        memcpy(block_buffer + parsed_bytes - original_rec_len + entry_size_aligned + sizeof(struct ext2_directory_entry) - EXT2_NAME_LEN, name, child_entry.name_len);

        //Start debug
        parsed_bytes = 0;        
        printf("[EXT2] DEBUG Directory listing after creation]\n");
        while (parsed_bytes < root_inode->i_size) {
            struct ext2_directory_entry *dent = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            printf("[EXT2] ino: %d rec_len: %d name_len: %d file_type: %d name: %s\n", dent->inode, dent->rec_len, dent->name_len, dent->file_type, dent->name);
            parsed_bytes += dent->rec_len;
        }
        printf("[EXT2] DEBUG [END]\n");

        //End debug
        if (ext2_write_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size) <= 0) {
            printf("[EXT2] Failed to write directory entry\n");
            free(block_buffer);
            return 1;
        }

        free(block_buffer);
        printf("[EXT2] Directory entry created\n");
        return 0;
    } else {
        printf("[EXT2] Inode is not a directory\n");
        return 1;
    }
}

uint8_t ext2_path_to_parent_and_name(const char* source, char** path, char** name) {
    uint64_t path_length = strlen(source);
    if (path_length == 0) return 0;

    uint64_t last_slash = 0;
    for (uint64_t i = 0; i < path_length; i++) {
        if (source[i] == '/') last_slash = i;
    }

    if (last_slash == 0) {
        *path = (char*)malloc(2);
        (*path)[0] = '/';
        (*path)[1] = 0;
        *name = (char*)malloc(path_length + 1);
        memcpy(*name, source+1, path_length + 1);
        return 1;
    }

    *path = (char*)malloc(last_slash + 1);
    memcpy(*path, source, last_slash);
    (*path)[last_slash] = 0;

    *name = (char*)malloc(path_length - last_slash);
    memcpy(*name, source + last_slash + 1, path_length - last_slash);
    return 1;
}

uint8_t ext2_create_file(struct ext2_partition * partition, const char* path) {
    char * name;
    char * parent_path;
    if (!ext2_path_to_parent_and_name(path, &parent_path, &name)) return 0;

    printf("[EXT2] Creating file %s in %s\n", name, parent_path);

    uint32_t parent_inode_index = ext2_path_to_inode(partition, parent_path);
    if (!parent_inode_index) {
        printf("[EXT2] Parent directory doesn't exist\n");
        return 1;
    }

    struct ext2_inode_descriptor_generic * parent_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, parent_inode_index);
    if (!(parent_inode->i_mode & INODE_TYPE_DIR)) {
        printf("[EXT2] Parent inode is not a directory\n");
        return 1;
    }

    printf("[EXT2] Parent inode is a directory, ready to allocate\n");

    uint32_t new_inode_index = ext2_allocate_inode(partition);
    if (!new_inode_index) {
        printf("[EXT2] Failed to allocate inode\n");
        return 1;
    }

    printf("[EXT2] Allocated inode %d\n", new_inode_index);
    struct ext2_inode_descriptor * inode = ext2_initialize_inode(partition, new_inode_index, INODE_TYPE_FILE);
    printf("[EXT2] Initialized inode %d\n", new_inode_index);
    printf("[EXT2] inode created at %d\n", (inode->id).i_ctime);
    if (ext2_write_inode(partition, new_inode_index, inode)) {
        printf("[EXT2] Failed to write inode\n");
        return 1;
    }

    printf("[EXT2] Creating directory entry for inode %d\n", new_inode_index);
    if (ext2_create_directory_entry(partition, parent_inode_index, new_inode_index, name, EXT2_DIR_TYPE_REGULAR)) {
        printf("[EXT2] Failed to create directory entry\n");
        return 1;
    }

    return 0;
}

/*
* ext2_write_file(
    ext2_create_file(
        ext2_allocate_inode
        ext2_initialize_inode
    )
)a
*/

//TODO: Optimize this so you start searching from the last block allocated
uint32_t ext2_allocate_block(struct ext2_partition* partition) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    for (uint32_t i = 0; i < partition->group_number; i++) {
        struct ext2_block_group_descriptor * block_group = &partition->gd[i];
        if (!block_group) {
            printf("[EXT2] Failed to read block group descriptor\n");
            return 0;
        }

        if (block_group->bg_free_blocks_count == 0) {
            printf("[EXT2] Block group %d is full\n", i);
            continue;
        }

        uint32_t * block_bitmap = (uint32_t *)malloc(block_size);
        if (!ext2_read_block(partition, block_group->bg_block_bitmap, (uint8_t*)block_bitmap)) {
            printf("[EXT2] Failed to read block bitmap\n");
            return 0;
        }

        for (uint32_t j = 0; j < block_size / sizeof(uint32_t); j++) {
            if (block_bitmap[j] == 0xFFFFFFFF) continue;
            for (uint32_t k = 0; k < 32; k++) {
                if (!(block_bitmap[j] & (1 << k))) {
                    //printf("[EXT2] Block %d is free\n", j * 32 + k + 1);
                    block_bitmap[j] |= (1 << k);
                    block_group->bg_free_blocks_count--;
                    ((struct ext2_superblock*)partition->sb)->s_free_blocks_count--;
                    if (ext2_write_block(partition, block_group->bg_block_bitmap, (uint8_t*)block_bitmap)  <= 0) {
                        printf("[EXT2] Failed to write block bitmap\n");
                        return 0;
                    }

                    uint32_t block = j * 32 + k + 1;
                    block += i * ((struct ext2_superblock*)partition->sb)->s_blocks_per_group;
                    //printf("[EXT2] Allocated block %d\n", block);
                    return block;
                }
            }
        }
    }

    printf("[EXT2] No free blocks\n");
    return 0;
}

uint32_t ext2_allocate_indirect_block(struct ext2_partition* partition, uint32_t * target_block, uint32_t blocks_to_allocate) {
        printf("[EXT2] Allocating block group\n");
        uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
        if (!target_block) {
            printf("[EXT2] Invalid target block\n");
            return 0;
        }
        *target_block = ext2_allocate_block(partition);
        if (!*target_block) {
            printf("[EXT2] Failed to allocate block\n");
            return 0;
        }


        uint32_t * block_buffer = (uint32_t *)malloc(block_size);
        if (!block_buffer) {
            printf("[EXT2] Failed to allocate memory for block buffer\n");
            return 0;
        }

        uint32_t blocks_written = 0;
        for (uint32_t j = 0; j < block_size / sizeof(uint32_t); j++) {
            if (blocks_written == blocks_to_allocate) break;
            block_buffer[j] = ext2_allocate_block(partition);
            if (!block_buffer[j]) {
                printf("[EXT2] Failed to allocate block\n");
                free(block_buffer);
                return 0;
            }
            blocks_written++;
        }

        if (ext2_write_block(partition, *target_block, (uint8_t*)block_buffer) <= 0) {
            printf("[EXT2] Failed to write block\n");
            free(block_buffer);
            return 0;
        }

        free(block_buffer);
        return blocks_written;
}

uint32_t ext2_allocate_double_indirect_block(struct ext2_partition* partition, uint32_t * target_block, uint32_t blocks_to_allocate) {
    printf("[EXT2] Allocating double indirect block\n");
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    if (!target_block) {
        printf("[EXT2] Invalid target block\n");
        return 0;
    }
    *target_block = ext2_allocate_block(partition);
    if (!*target_block) {
        printf("[EXT2] Failed to allocate block\n");
        return 0;
    }

    uint32_t * block_buffer = (uint32_t *)malloc(block_size);
    if (!block_buffer) {
        printf("[EXT2] Failed to allocate memory for block buffer\n");
        return 0;
    }

    uint32_t blocks_written = 0;
    for (uint32_t j = 0; j < block_size / sizeof(uint32_t); j++) {
        if (blocks_written == blocks_to_allocate) break;
        blocks_written += ext2_allocate_indirect_block(partition, &block_buffer[j], blocks_to_allocate - blocks_written);
    }

    if (ext2_write_block(partition, *target_block, (uint8_t*)block_buffer) <= 0) {
        printf("[EXT2] Failed to write block\n");
        free(block_buffer);
        return 0;
    }

    free(block_buffer);
    return blocks_written;
}

uint32_t ext2_allocate_triple_indirect_block(struct ext2_partition* partition, uint32_t * target_block, uint32_t blocks_to_allocate) {
    printf("[EXT2] Allocating triple indirect block\n");
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    if (!target_block) {
        printf("[EXT2] Invalid target block\n");
        return 0;
    }
    *target_block = ext2_allocate_block(partition);
    if (!*target_block) {
        printf("[EXT2] Failed to allocate block\n");
        return 0;
    }

    uint32_t * block_buffer = (uint32_t *)malloc(block_size);
    if (!block_buffer) {
        printf("[EXT2] Failed to allocate memory for block buffer\n");
        return 0;
    }

    uint32_t blocks_written = 0;
    for (uint32_t j = 0; j < block_size / sizeof(uint32_t); j++) {
        if (blocks_written == blocks_to_allocate) break;
        blocks_written += ext2_allocate_double_indirect_block(partition, &block_buffer[j], blocks_to_allocate - blocks_written);
    }

    if (ext2_write_block(partition, *target_block, (uint8_t*)block_buffer) <= 0) {
        printf("[EXT2] Failed to write block\n");
        free(block_buffer);
        return 0;
    }

    free(block_buffer);
    return blocks_written;
}

uint8_t ext2_resize_file(struct ext2_partition* partition, uint32_t inode_index, uint32_t new_size) {
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_index);
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    if (!inode) {
        printf("[EXT2] Failed to read inode\n");
        return 0;
    }

    if (inode->i_size == new_size) {
        printf("[EXT2] File is already of the requested size\n");
        return 1;
    }

    if (inode->i_size > new_size) {
        printf("[EXT2] Shrinking files is not supported yet\n");
        return 0;
    }

    uint32_t blocks_to_allocate = (new_size - inode->i_size) / block_size;
    if ((new_size - inode->i_size) % block_size) blocks_to_allocate++;

    printf("[EXT2] Resizing file to %d bytes, allocating %d blocks\n", new_size, blocks_to_allocate);

    uint32_t blocks_allocated = 0;
    for (uint32_t i = 0; i < 12; i++) {
        if (blocks_allocated == blocks_to_allocate) break;
        if (!inode->i_block[i]) {
            inode->i_block[i] = ext2_allocate_block(partition);
            if (!inode->i_block[i]) {
                printf("[EXT2] Failed to allocate block\n");
                goto write_inode;
            }
            blocks_allocated++;
        }
    }

    if (blocks_allocated == blocks_to_allocate) {
        printf("[EXT2] Allocated all blocks\n");
        goto write_inode;
    }

    blocks_allocated += ext2_allocate_indirect_block(partition, &inode->i_block[12], blocks_to_allocate - blocks_allocated);

    if (blocks_allocated == blocks_to_allocate) {
        printf("[EXT2] Allocated all blocks\n");
        goto write_inode;
    }

    blocks_allocated += ext2_allocate_double_indirect_block(partition, &inode->i_block[13], blocks_to_allocate - blocks_allocated);

    if (blocks_allocated == blocks_to_allocate) {
        printf("[EXT2] Allocated all blocks\n");
        goto write_inode;
    }

    blocks_allocated += ext2_allocate_triple_indirect_block(partition, &inode->i_block[14], blocks_to_allocate - blocks_allocated);

    if (blocks_allocated == blocks_to_allocate) {
        printf("[EXT2] Allocated all blocks\n");
        goto write_inode;
    }

    printf("[EXT2] Failed to allocate all blocks\n");
    return 0;

write_inode:

    inode->i_size = new_size;
    if (!ext2_write_inode(partition, inode_index, (struct ext2_inode_descriptor*)inode)) {
        printf("[EXT2] Failed to write inode\n");
        return 0;
    }

    ext2_flush_structures(partition);
    return 1;
}

uint8_t ext2_write_file(struct ext2_partition * partition, const char * path, uint8_t * source_buffer, uint64_t size, uint64_t skip) {
    //TODO: Skip
    if (skip) {
        printf("[EXT2] Skipping is not supported yet\n");
        return 0;
    }

    uint32_t inode_index = ext2_path_to_inode(partition, path);
    if (!inode_index) {
        printf("[EXT2] File doesn't exist\n");
        return 0;
    }
    
    printf("[EXT2] File created, trying to get inode\n");
    inode_index = ext2_path_to_inode(partition, path);
    if (!inode_index) {
        printf("[EXT2] Failed to get file inode\n");
        return 0;
    }

    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_index);
    if (inode->i_mode & INODE_TYPE_DIR) {printf("err ino type\n");return 0;}

    if (size + skip > inode->i_size) {
        printf("[EXT2] File is too small, resizing\n");
        if (ext2_resize_file(partition, inode_index, size + skip)) {
            printf("[EXT2] Failed to resize file\n");
            return 0;
        }
    }

    struct ext2_inode_descriptor_generic * inode_after = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_index);
    if (inode_after->i_size != size + skip) {
        printf("[EXT2] File size is not correct\n");
        return 0;
    } else {
        printf("[EXT2] File size is correct\n");
    }

    int64_t write_bytes = ext2_write_inode_bytes(partition, inode_index, source_buffer, size);
    if (write_bytes == EXT2_WRITE_FAILED) {printf("err file write bytes\n");return 0;}
    if (write_bytes <= 0) {printf("err write bytes\n");return 0;}

    return 1;
}