#include "ext2.h"
#include "../fused/primitives.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

//Fix by FunkyWaddle: Inverted values
#define DIVIDE_ROUNDED_UP(x, y) ((x % y) ? ((x) + (y) - 1) / (y) : (x) / (y))

struct ext2_partition * ext2_partition_head = 0x0;

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

/*
uint8_t ext2_prepare_read(struct ext2_read_requirements * target, struct ext2_partition * partition, uint32_t inode_number) {
    target->valid = 0;

    ext2_disk_from_partition(target->disk_name, partno);
    if (!ext2_check_status(target->disk_name)) {
        return 0;
    }

    ioctl_disk(target->disk_name, IOCTL_GET_SECTOR_SIZE, &(target->sector_size));

    printf("[EXT2] Reading inode %d for partition %s\n", inode_number, partno);
    struct ext2_partition *partition = get_partition(partno);
    if (partition == 0) {
        printf("[EXT2] Partition %s not found\n", partno);
        return 0;
    }

    struct ext2_superblock * superblock = (struct ext2_superblock*)partition->sb;
    struct ext2_superblock_extended * superblock_extended = (struct ext2_superblock_extended*)partition->sb;

    target->ext2_version = superblock->s_rev_level;
    target->block_size = 1024 << superblock->s_log_block_size;
    target->partition_lba = partition->lba;
    target->sectors_per_block = DIVIDE_ROUNDED_UP(target->block_size, target->sector_size);

    uint32_t inodes_per_group = superblock->s_inodes_per_group;
    uint32_t inode_size = (target->ext2_version < 1) ? 128 : superblock_extended->s_inode_size;
    uint32_t inode_sectors = DIVIDE_ROUNDED_UP(inode_size, target->sector_size);

    uint32_t inode_group = (inode_number - 1 ) / inodes_per_group;
    uint32_t inode_index = (inode_number - 1 ) % inodes_per_group;
    uint32_t inode_block = (inode_index * inode_size) / target->block_size;

    struct ext2_block_group_descriptor * block_group_descriptor = (struct ext2_block_group_descriptor*)partition->gd;
    uint32_t inode_table_block = block_group_descriptor[inode_group].bg_inode_table;

    uint32_t inode_table_lba = (inode_table_block * target->block_size) / 512;

    void * root_inode_buffer = malloc(inode_sectors * target->sector_size);
    if (read_disk(target->disk_name, (uint8_t*)root_inode_buffer, target->partition_lba + inode_table_lba + inode_block, inode_sectors)) {
        printf("[EXT2] Root inode read failed\n");
        free(root_inode_buffer);
        return 0;
    }

    target->inode = (struct ext2_inode_descriptor*)root_inode_buffer;
    target->valid = 1;
    return 1;
}*/

uint8_t * ext2_buffer_for_size(struct ext2_partition * partition, uint64_t size) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t blocks_for_size = DIVIDE_ROUNDED_UP(size, block_size);
    uint32_t buffer_size = blocks_for_size * block_size;

    return malloc(buffer_size);
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
        return 0;
    }

    return block_size;
}

int64_t ext2_read_blocks(struct ext2_partition* partition, struct ext2_inode_descriptor_generic * inode, uint32_t max, uint8_t * destination_buffer, uint64_t count) {
    uint64_t blocks_read = 0;

    for (uint32_t i = 0; i < max; i++) {
        int64_t read_result = ext2_read_block(partition, inode->i_block[i], destination_buffer);

        if (read_result == EXT2_READ_FAILED) {
            return EXT2_READ_FAILED;
        } else if (read_result == 0) {
            return blocks_read;
        }

        destination_buffer += read_result;
        if (blocks_read++ == count) {
            break;
        }
    }

    return blocks_read;
}

int64_t ext2_read_indirect_block(struct ext2_partition* partition, struct ext2_inode_descriptor_generic* inode, uint8_t *destination_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t blocks_per_block = block_size / 4;

    uint32_t * indirect_block = (uint32_t*)ext2_buffer_for_size(partition, block_size);

    if (ext2_read_block(partition, inode->i_block[12], (uint8_t*)indirect_block) <= 0) {
        printf("[EXT2] Failed to read indirect block\n");
        free(indirect_block);
        return EXT2_READ_FAILED;
    }

    uint32_t blocks_read = 0;
    for (uint32_t i = 0; i < blocks_per_block; i++) {
        int64_t read_result = ext2_read_block(partition, indirect_block[i], destination_buffer);

        if (read_result == EXT2_READ_FAILED) {
            free(indirect_block);
            return EXT2_READ_FAILED;
        } else if (read_result == 0) {
            free(indirect_block);
            return blocks_read;
        }

        destination_buffer += read_result;
        if (blocks_read++ == count) {
            break;
        }
    }

    free(indirect_block);
    return blocks_read;
}

uint64_t ext2_read_inode_blocks(struct ext2_partition* partition, uint32_t inode_number, uint8_t * destination_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);
    int64_t read_result = ext2_read_blocks(partition, inode, 12, destination_buffer, count);

    uint64_t blocks_read = 0;

    if (read_result < 0) {
        printf("[EXT2] File read failed\n");
        return 0;
    }
    blocks_read += read_result;
    destination_buffer += read_result * block_size;

    printf("[EXT2] Read %ld blocks, max: %ld\n", blocks_read, count);
    if (blocks_read < count) {
        printf("[EXT2] Reading indirect block\n");
        read_result = ext2_read_indirect_block(partition, inode, destination_buffer, count - blocks_read);
        if (read_result < 0) {
            printf("[EXT2] File read failed\n");
            return 0;
        }

        blocks_read += read_result;
        destination_buffer += read_result * block_size;
        printf("[EXT2] Indirect read %ld blocks, max: %ld\n", blocks_read, count);
        if (blocks_read < count) {
            printf("Double indirect not implemented yet!\n");
        }
    }

    return blocks_read;
}

uint64_t ext2_read_inode_bytes(struct ext2_partition* partition, uint32_t inode_number, uint8_t * destination_buffer, uint64_t count) {
    uint64_t blocks = DIVIDE_ROUNDED_UP(count, 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
    return ext2_read_inode_blocks(partition, inode_number, destination_buffer, blocks) * (1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
}

    //uint64_t entries = requirements.block_size / BLOCK_NUMBER;
/*
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic*)requirements.inode;
    char atime[32];
    epoch_to_date(atime, inode->i_atime);
    printf("[EXT2] [debug] Reading inode %d, atime: %s\n", inode_index, atime);

    uint64_t blocks_to_read = DIVIDE_ROUNDED_UP(size, requirements.block_size);
    printf("With a size of: %ld and a block size of: %d, i have to read : %ld blocks\n", size, requirements.block_size, blocks_to_read);
    uint64_t read_blocks = 0;

    for (uint32_t i = 0; i < 12; i++) {
        if (read_blocks >= blocks_to_read) {
            goto end;
        }
        if (inode->i_block[i]) {
            read_disk(
                requirements.disk_name,
                destination_buffer + (read_blocks * requirements.block_size),
                requirements.partition_lba + inode->i_block[i] * requirements.sectors_per_block,
                requirements.sectors_per_block
            );
            read_blocks++;
        } else {
            goto end;
        }
    }
    
    if (inode->i_block[12]) {
        uint32_t * indirect_block = (uint32_t*)malloc(block_size);
        read_disk(disk_name, indirect_block, partition->lba + inode->i_block[12], block_size);
        for (uint32_t i = 0; i < entries; i++) {
            if (indirect_block[i]) valid_blocks++;
        }
        free(indirect_block);
    }

    if (inode->i_block[13]) {
        uint32_t * double_indirect_block = (uint32_t*)malloc(block_size);
        read_disk(disk_name, double_indirect_block, partition->lba + inode->i_block[13], block_size);
        for (uint32_t i = 0; i < entries; i++) {
            if (double_indirect_block[i]) {
                uint32_t * indirect_block = (uint32_t*)malloc(block_size);
                read_disk(disk_name, indirect_block, partition->lba + double_indirect_block[i], block_size);
                for (uint32_t j = 0; j < entries; j++) {
                    if (indirect_block[j]) valid_blocks++;
                }
                free(indirect_block);
            }
        }
        free(double_indirect_block);
    }
end:
    return read_blocks * requirements.block_size;
}*/

void ext2_list_directory(struct ext2_partition* partition, const char * path) {
    uint32_t inode_number = ext2_path_to_inode(partition, path);
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * root_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);

    if (root_inode->i_mode & INODE_TYPE_DIR) {
        uint8_t *block_buffer = malloc(root_inode->i_size + block_size);
        ext2_read_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size);

        uint32_t parsed_bytes = 0;

        while (parsed_bytes < root_inode->i_size) {
            struct ext2_directory_entry *entry = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            printf("[INO: %d] %s\n", entry->inode, entry->name);
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

//Hey this may corrupt memory
uint8_t ext2_read_file(struct ext2_partition * partition, const char * path, uint8_t * destination_buffer, uint64_t size) {
    uint32_t inode_index = ext2_path_to_inode(partition, path);
    if (!inode_index) {printf("err ino index\n");return 0;}
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_index);
    if (inode->i_mode & INODE_TYPE_DIR) {printf("err ino type\n");return 0;}
    if (inode->i_size < size) size = inode->i_size;
    uint64_t read_bytes = ext2_read_inode_bytes(partition, inode_index, destination_buffer, size);
    if (read_bytes <= 0) {printf("err read bytes\n");return 0;}
    return 1;
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
    memcpy(partition->sb, superblock, 1024);
    partition->gd = malloc(block_group_descriptors_size * sector_size);
    memcpy(partition->gd, block_group_descriptor, block_group_descriptors_size * sector_size);

    printf("[EXT2] Partition %s has: %d groups\n", partition->name, block_groups_first);

    return partition;
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

    printf("[EXT2] size: %d, blocks: %d, block[0]: %d\n", inode->i_size, inode->i_sectors, inode->i_block[0]);
    printf("[EXT2] inode: mode: %x, links: %d, uid: %d, gid: %d\n", inode->i_mode, inode->i_links_count, inode->i_uid, inode->i_gid);
    printf("[EXT2] inode: atime: %s, ctime: %s, mtime: %s, dtime: %s\n", atime, ctime, mtime, dtime);
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

    struct ext2_inode_descriptor * inode = malloc(sizeof(struct ext2_inode_descriptor));
    memcpy(inode, root_inode_buffer + (((inode_index * inode_size) % block_size)), inode_size);
    free(root_inode_buffer);

    return inode;
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