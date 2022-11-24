#include "ext2.h"
#include "../fused/primitives.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define DIVIDE_ROUNDED_UP(x, y) (((x) + (y) - 1) / (y))
struct ext2_partition * ext2_partition_head = 0x0;

void epoch_to_date(char* date, uint32_t epoch) {
    time_t t = epoch;
    struct tm *tm = localtime(&t);
    strftime(date, 32, "%Y-%m-%d %H:%M:%S", tm);
}

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

char register_ext2_partition(const char* disk, uint32_t lba) {
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
    
    if (read_disk(disk, (uint8_t*)block_group_descriptor_buffer, lba+(sectors_per_block*BGDT_BLOCK), block_group_descriptors_size)) return 0;
    struct ext2_block_group_descriptor * block_group_descriptor = (struct ext2_block_group_descriptor*)block_group_descriptor_buffer;

    printf("[EXT2] Groups %d loaded\n", block_groups_first);
    for (uint32_t i = 0; i < block_groups_first; i++) {
        printf("[EXT2] Group %d: block bitmap at %d, inode bitmap at %d, inode table at %d\n", i, block_group_descriptor[i].bg_block_bitmap, block_group_descriptor[i].bg_inode_bitmap, block_group_descriptor[i].bg_inode_table);
        printf("[EXT2] Group %d: free blocks count: %d, free inodes count: %d, used dirs count: %d\n", i, block_group_descriptor[i].bg_free_blocks_count, block_group_descriptor[i].bg_free_inodes_count, block_group_descriptor[i].bg_used_dirs_count); 
    }

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
    partition->group_number = block_groups_first;
    partition->lba = lba;
    partition->sb = malloc(1024);
    memcpy(partition->sb, superblock, 1024);
    partition->gd = malloc(block_group_descriptors_size * sector_size);
    memcpy(partition->gd, block_group_descriptor, block_group_descriptors_size * sector_size);

    printf("[EXT2] Partition %s has: %d groups\n", partition->name, block_groups_first);

    return 1;
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

int ext2_get_partition_name_by_index(char * partno, uint32_t index) {
    if (partno == 0) {
        return 0;
    }
    struct ext2_partition * partition = ext2_partition_head;
    uint32_t partition_id = 0;

    while (partition != 0) {
        if (partition_id == index) {
            strcpy(partno, partition->name);
            return 1;
        }
        partition = partition->next;
        partition_id++;
    }
    return 0;
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

uint8_t ext2_read_root_inode(const char* partno) {
    char disk_name[32];
    ext2_disk_from_partition(disk_name, partno);
    if (!ext2_check_status(disk_name)) {
        return 0;
    }

    uint32_t sector_size;
    ioctl_disk(disk_name, IOCTL_GET_SECTOR_SIZE, &sector_size);

    printf("[EXT2] Reading root inode for partition %s\n", partno);
    struct ext2_partition * partition = get_partition(partno);
    if (partition == 0) {
        printf("[EXT2] Partition %s not found\n", partno);
        return 0;
    }

    struct ext2_superblock * superblock = (struct ext2_superblock*)partition->sb;
    struct ext2_superblock_extended * superblock_extended = (struct ext2_superblock_extended*)partition->sb;
    uint32_t ext2_version = superblock->s_rev_level;


    uint32_t inodes_per_group = superblock->s_inodes_per_group;
    uint32_t block_size = 1024 << superblock->s_log_block_size;
    uint32_t inode_size = (ext2_version < 1) ? 128 : superblock_extended->s_inode_size;
    uint32_t inode_sectors = DIVIDE_ROUNDED_UP(inode_size, sector_size);

    uint32_t inode_group = (EXT2_ROOT_INO_INDEX - 1 ) / inodes_per_group;
    uint32_t inode_index = (EXT2_ROOT_INO_INDEX - 1 ) % inodes_per_group;
    uint32_t inode_block = (inode_index * inode_size) / block_size;

    struct ext2_block_group_descriptor * block_group_descriptor = (struct ext2_block_group_descriptor*)partition->gd;
    uint32_t inode_table_block = block_group_descriptor[inode_group].bg_inode_table;

    uint32_t inode_table_lba = (inode_table_block * block_size) / 512;

    void * root_inode_buffer = malloc(inode_sectors * sector_size);
    if (read_disk(disk_name, (uint8_t*)root_inode_buffer, partition->lba + inode_table_lba + inode_block, inode_sectors)) {
        printf("[EXT2] Root inode read failed\n");
        return 0;
    }

    struct ext2_inode_descriptor * root_inode_ext = (struct ext2_inode_descriptor*)root_inode_buffer;
    struct ext2_inode_descriptor_generic * root_inode = (struct ext2_inode_descriptor_generic*)&(root_inode_ext->id);

    char atime[32];
    char ctime[32];
    char mtime[32];
    char dtime[32];

    epoch_to_date(atime, root_inode->i_atime);
    epoch_to_date(ctime, root_inode->i_ctime);
    epoch_to_date(mtime, root_inode->i_mtime);
    epoch_to_date(dtime, root_inode->i_dtime);

    printf("[EXT2] Root inode: %d, size: %d, blocks: %d, block[0]: %d\n", EXT2_ROOT_INO_INDEX, root_inode->i_size, root_inode->i_blocks, root_inode->i_block[0]);
    printf("[EXT2] Root inode: mode: %d, links: %d, uid: %d, gid: %d\n", root_inode->i_mode, root_inode->i_links_count, root_inode->i_uid, root_inode->i_gid);
    printf("[EXT2] Root inode: atime: %s, ctime: %s, mtime: %s, dtime: %s\n", atime, ctime, mtime, dtime);
    return 1;
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