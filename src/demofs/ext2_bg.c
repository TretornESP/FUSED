#include "ext2_bg.h"
#include "ext2_util.h"

#include "../fused/primitives.h"
#include "../fused/auxiliary.h"

#include <stdio.h>

int32_t ext2_operate_on_bg(struct ext2_partition * partition, uint8_t (*callback)(struct ext2_partition *, struct ext2_block_group_descriptor*, uint32_t)) {
    uint32_t i;
    for (i = 0; i < partition->group_number; i++) {
        if (callback(partition, &partition->gd[i], i)) 
            return (int32_t)i;
    }

    return -1;
}

uint8_t ext2_flush_bg(struct ext2_partition* partition, struct ext2_block_group_descriptor* bg, uint32_t bgid) {
    uint32_t block_size = 1024 << ((struct ext2_superblock*)(partition->sb))->s_log_block_size;

    uint32_t block_group_descriptors_size = DIVIDE_ROUNDED_UP(partition->group_number * sizeof(struct ext2_block_group_descriptor), partition->sector_size);
    uint32_t sectors_per_group = ((struct ext2_superblock*)(partition->sb))->s_blocks_per_group * (block_size / partition->sector_size);
    write_disk(partition->disk, (uint8_t*)bg, partition->lba+(sectors_per_group*bgid)+partition->bgdt_block, block_group_descriptors_size);
    return 0;
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
