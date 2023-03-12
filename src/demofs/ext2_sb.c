#include "ext2_sb.h"

#include "../fused/primitives.h"
#include "../fused/auxiliary.h"

uint8_t ext2_flush_sb(struct ext2_partition* partition, struct ext2_block_group_descriptor* bg, uint32_t bgid) {
    (void)bg;
    uint32_t block_size = 1024 << ((struct ext2_superblock*)(partition->sb))->s_log_block_size;
    uint32_t sectors_per_group = ((struct ext2_superblock*)(partition->sb))->s_blocks_per_group * (block_size / partition->sector_size);

    write_disk(partition->disk, (uint8_t*)partition->sb, partition->lba+(sectors_per_group*bgid)+partition->sb_block, 2);
    return 0;
}
