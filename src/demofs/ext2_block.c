#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

#include "ext2_block.h"
#include "ext2_util.h"

#include "../fused/primitives.h"
#include "../fused/auxiliary.h"

#include <stdio.h>
#include <stdlib.h>


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

int64_t ext2_read_inode_bytes(struct ext2_partition* partition, uint32_t inode_number, uint8_t * destination_buffer, uint64_t count) {
    uint64_t blocks = DIVIDE_ROUNDED_UP(count, 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
    int64_t result = ext2_read_inode_blocks(partition, inode_number, destination_buffer, blocks);
    if (result == EXT2_READ_FAILED) return EXT2_READ_FAILED;
    return result * (1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
}

int64_t ext2_write_inode_bytes(struct ext2_partition* partition, uint32_t inode_number, uint8_t * source_buffer, uint64_t count) {
    uint64_t blocks = DIVIDE_ROUNDED_UP(count, 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
    printf("[EXT2] Writing %ld bytes to inode %d (%ld blocks)\n", count, inode_number, blocks);    
    int64_t result = ext2_write_inode_blocks(partition, inode_number, source_buffer, blocks);
    if (result == EXT2_WRITE_FAILED) return EXT2_WRITE_FAILED;
    return result * (1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
}