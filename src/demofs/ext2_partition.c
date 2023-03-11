#include "ext2_partition.h"

#include "../fused/primitives.h"
#include "../fused/auxiliary.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct ext2_partition * ext2_partition_head = 0x0;

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

uint32_t ext2_count_partitions() {
    uint32_t count = 0;
    struct ext2_partition * partition = ext2_partition_head;

    while (partition != 0) {
        count++;
        partition = partition->next;
    }
    return count;
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


uint8_t unregister_ext2_partition(char letter) {
    (void)letter;
    printf("[EXT2] unregstr_ext2_partition Not implemented\n");
    return 0;
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
