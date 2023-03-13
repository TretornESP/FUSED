//Supress -Waddress-of-packed-member warning
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#pragma GCC diagnostic ignored "-Wvariadic-macros"

#include "ext2.h"
#include "ext2_block.h"
#include "ext2_inode.h"
#include "ext2_sb.h"
#include "ext2_bg.h"
#include "ext2_util.h"
#include "ext2_integrity.h"

#include "../fused/primitives.h"
#include "../fused/auxiliary.h"

#include <stdlib.h>

int64_t ext2_read_block(struct ext2_partition* partition, uint32_t block, uint8_t * destination_buffer) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t block_sectors = DIVIDE_ROUNDED_UP(block_size, partition->sector_size);
    uint32_t block_lba = (block * block_size) / partition->sector_size;

    if (block) {
        if (read_disk(partition->disk, destination_buffer, partition->lba + block_lba, block_sectors)) {
            EXT2_ERROR("Failed to read block %d", block);
            return EXT2_READ_FAILED;
        }
    } else {
        EXT2_ERROR("Attempted to read block 0");
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
        uint32_t * indirect_block = (uint32_t*)ext2_buffer_for_size(block_size, block_size);
        if (blocks[i] == 0) EXT2_WARN("Single indirect block is 0");

        if (ext2_read_block(partition, blocks[i], (uint8_t*)indirect_block) <= 0) {
            EXT2_ERROR("Failed to read indirect block");
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
        uint32_t * double_indirect_block = (uint32_t*)ext2_buffer_for_size(block_size, block_size);
        if (blocks[i] == 0) EXT2_WARN("Double indirect block is 0");
        if (ext2_read_block(partition, blocks[i], (uint8_t*)double_indirect_block) <= 0) {
            EXT2_ERROR("Failed to read double indirect block");
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
        uint32_t * triple_indirect_block = (uint32_t*)ext2_buffer_for_size(block_size, block_size);
        if (blocks[i] == 0) EXT2_WARN("Triple indirect block is 0");
        if (ext2_read_block(partition, blocks[i], (uint8_t*)triple_indirect_block) <= 0) {
            EXT2_ERROR("Failed to read triple indirect block");
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


int64_t ext2_write_block(struct ext2_partition* partition, uint32_t block, uint8_t * source_buffer) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t block_sectors = DIVIDE_ROUNDED_UP(block_size, partition->sector_size);
    uint32_t block_lba = (block * block_size) / partition->sector_size;

    if (block) {
        if (write_disk(partition->disk, source_buffer, partition->lba + block_lba, block_sectors)) {
            EXT2_ERROR("Failed to write block %d", block);
            return EXT2_WRITE_FAILED;
        }
    } else {
        EXT2_ERROR("Attempted to write block 0");
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
        uint32_t * indirect_block = (uint32_t*)ext2_buffer_for_size(block_size, block_size);
        if (blocks[i] == 0) EXT2_WARN("Single indirect block is 0");

        if (ext2_read_block(partition, blocks[i], (uint8_t*)indirect_block) <= 0) {
            EXT2_ERROR("Failed to read indirect block");
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
        uint32_t * double_indirect_block = (uint32_t*)ext2_buffer_for_size(block_size, block_size);
        if (blocks[i] == 0) EXT2_WARN("Double indirect block is 0");
        if (ext2_read_block(partition, blocks[i], (uint8_t*)double_indirect_block) <= 0) {
            EXT2_ERROR("Failed to read double indirect block");
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
        uint32_t * triple_indirect_block = (uint32_t*)ext2_buffer_for_size(block_size, block_size);
        if (blocks[i] == 0) EXT2_WARN("Triple indirect block is 0");
        if (ext2_read_block(partition, blocks[i], (uint8_t*)triple_indirect_block) <= 0) {
            EXT2_ERROR("Failed to read triple indirect block (for write)");
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



//TODO: Optimize this so you start searching from the last block allocated
uint32_t ext2_allocate_block(struct ext2_partition* partition) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    for (uint32_t i = 0; i < partition->group_number; i++) {
        struct ext2_block_group_descriptor * block_group = &partition->gd[i];
        if (!block_group) {
            EXT2_ERROR("Failed to read block group descriptor");
            return 0;
        }

        if (block_group->bg_free_blocks_count == 0) {
            EXT2_ERROR("Block group %d is full", i);
            continue;
        }

        uint32_t * block_bitmap = (uint32_t *)malloc(block_size);
        if (!ext2_read_block(partition, block_group->bg_block_bitmap, (uint8_t*)block_bitmap)) {
            EXT2_ERROR("Failed to read block bitmap");
            free(block_bitmap);
            return 0;
        }

        for (uint32_t j = 0; j < block_size / sizeof(uint32_t); j++) {
            if (block_bitmap[j] == 0xFFFFFFFF) continue;
            for (uint32_t k = 0; k < 32; k++) {
                if (!(block_bitmap[j] & (1 << k))) {
                    block_bitmap[j] |= (1 << k);
                    block_group->bg_free_blocks_count--;
                    ((struct ext2_superblock*)partition->sb)->s_free_blocks_count--;
                    if (ext2_write_block(partition, block_group->bg_block_bitmap, (uint8_t*)block_bitmap)  <= 0) {
                        EXT2_ERROR("Failed to write block bitmap");
                        free(block_bitmap);
                        //Undo the changes
                        block_group->bg_free_blocks_count++;
                        ((struct ext2_superblock*)partition->sb)->s_free_blocks_count++;
                        return 0;
                    }

                    ext2_flush_required(partition);
                    uint32_t block = j * 32 + k + 1;
                    block += i * ((struct ext2_superblock*)partition->sb)->s_blocks_per_group;
                    free(block_bitmap);
                    return block;
                }
            }
        }

        free(block_bitmap);
    }

    EXT2_ERROR("No free blocks");
    return 0;
}

uint32_t ext2_deallocate_blocks(struct ext2_partition* partition, uint32_t *blocks, uint32_t block_number) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t blocks_deallocated = 0;
    uint8_t * block_bitmap = malloc(block_size);

    for (uint32_t i = 0; i < block_number; i++) {
        uint32_t block_group = (blocks[i] - 1) / ((struct ext2_superblock*)partition->sb)->s_blocks_per_group;
        uint32_t block_index = (blocks[i] - 1) % ((struct ext2_superblock*)partition->sb)->s_blocks_per_group;
        struct ext2_block_group_descriptor* block_group_descriptor = &((struct ext2_block_group_descriptor*)partition->gd)[block_group];
        uint32_t block_bitmap_block = block_group_descriptor->bg_block_bitmap;
        ext2_read_block(partition, block_bitmap_block, block_bitmap);
        block_bitmap[block_index / 32] &= ~(1 << (block_index % 32));
        ext2_write_block(partition, block_bitmap_block, block_bitmap);
        blocks_deallocated++;
        block_group_descriptor->bg_free_blocks_count++;
    }

    free(block_bitmap);

    ((struct ext2_superblock*)partition->sb)->s_free_blocks_count += block_number;
    
    ext2_flush_required(partition);
    return blocks_deallocated;
}

uint32_t ext2_deallocate_block(struct ext2_partition* partition, uint32_t block) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint32_t block_group = (block - 1) / ((struct ext2_superblock*)partition->sb)->s_blocks_per_group;
    uint32_t block_index = (block - 1) % ((struct ext2_superblock*)partition->sb)->s_blocks_per_group;
    uint32_t block_bitmap_index = block_index / 32;
    uint32_t block_bitmap_offset = block_index % 32;

    struct ext2_block_group_descriptor * block_group_descriptor = &partition->gd[block_group];
    if (!block_group_descriptor) {
        EXT2_ERROR("Failed to read block group descriptor");
        return 0;
    }

    uint32_t * block_bitmap = (uint32_t *)malloc(block_size);
    if (!ext2_read_block(partition, block_group_descriptor->bg_block_bitmap, (uint8_t*)block_bitmap)) {
        EXT2_ERROR("Failed to read block bitmap");
        free(block_bitmap);
        return 0;
    }

    if (block_bitmap[block_bitmap_index] & (1 << block_bitmap_offset)) {
        block_bitmap[block_bitmap_index] &= ~(1 << block_bitmap_offset);
        block_group_descriptor->bg_free_blocks_count++;
        ((struct ext2_superblock*)partition->sb)->s_free_blocks_count++;
        if (ext2_write_block(partition, block_group_descriptor->bg_block_bitmap, (uint8_t*)block_bitmap)  <= 0) {
            EXT2_ERROR("Failed to write block bitmap");
            free(block_bitmap);
            //Undo the changes
            block_group_descriptor->bg_free_blocks_count--;
            ((struct ext2_superblock*)partition->sb)->s_free_blocks_count--;
            return 0;
        }

        ext2_flush_required(partition);
        free(block_bitmap);
        return 1;
    }

    free(block_bitmap);
    return 0;
}

uint32_t ext2_allocate_indirect_block(struct ext2_partition* partition, uint32_t * target_block, uint32_t blocks_to_allocate) {
        EXT2_DEBUG("Allocating block group");
        uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
        if (!target_block) {
            EXT2_ERROR("Invalid target block");
            return 0;
        }
        *target_block = ext2_allocate_block(partition);
        if (!*target_block) {
            EXT2_ERROR("Failed to allocate block");
            return 0;
        }

        uint32_t * block_buffer = (uint32_t *)malloc(block_size);
        if (!block_buffer) {
            EXT2_ERROR("Failed to allocate memory for block buffer");
            return 0;
        }

        uint32_t blocks_written = 0;
        for (uint32_t j = 0; j < block_size / sizeof(uint32_t); j++) {
            if (blocks_written == blocks_to_allocate) break;
            block_buffer[j] = ext2_allocate_block(partition);
            if (!block_buffer[j]) {
                EXT2_ERROR("Failed to allocate block");
                free(block_buffer);
                return 0;
            }
            blocks_written++;
        }

        if (ext2_write_block(partition, *target_block, (uint8_t*)block_buffer) <= 0) {
            EXT2_ERROR("Failed to write block");
            free(block_buffer);
            return 0;
        }

        free(block_buffer);
        return blocks_written;
}

uint32_t ext2_allocate_double_indirect_block(struct ext2_partition* partition, uint32_t * target_block, uint32_t blocks_to_allocate) {
    EXT2_DEBUG("Allocating double indirect block");
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    if (!target_block) {
        EXT2_ERROR("Invalid target block");
        return 0;
    }
    *target_block = ext2_allocate_block(partition);
    if (!*target_block) {
        EXT2_ERROR("Failed to allocate block");
        return 0;
    }

    uint32_t * block_buffer = (uint32_t *)malloc(block_size);
    if (!block_buffer) {
        EXT2_ERROR("Failed to allocate memory for block buffer");
        return 0;
    }

    uint32_t blocks_written = 0;
    for (uint32_t j = 0; j < block_size / sizeof(uint32_t); j++) {
        if (blocks_written == blocks_to_allocate) break;
        blocks_written += ext2_allocate_indirect_block(partition, &block_buffer[j], blocks_to_allocate - blocks_written);
    }

    if (ext2_write_block(partition, *target_block, (uint8_t*)block_buffer) <= 0) {
        EXT2_ERROR("Failed to write block");
        free(block_buffer);
        return 0;
    }

    free(block_buffer);
    return blocks_written;
}

uint32_t ext2_allocate_triple_indirect_block(struct ext2_partition* partition, uint32_t * target_block, uint32_t blocks_to_allocate) {
    EXT2_DEBUG("Allocating triple indirect block");
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    if (!target_block) {
        EXT2_ERROR("Invalid target block");
        return 0;
    }
    *target_block = ext2_allocate_block(partition);
    if (!*target_block) {
        EXT2_ERROR("Failed to allocate block");
        return 0;
    }

    uint32_t * block_buffer = (uint32_t *)malloc(block_size);
    if (!block_buffer) {
        EXT2_ERROR("Failed to allocate memory for block buffer");
        return 0;
    }

    uint32_t blocks_written = 0;
    for (uint32_t j = 0; j < block_size / sizeof(uint32_t); j++) {
        if (blocks_written == blocks_to_allocate) break;
        blocks_written += ext2_allocate_double_indirect_block(partition, &block_buffer[j], blocks_to_allocate - blocks_written);
    }

    if (ext2_write_block(partition, *target_block, (uint8_t*)block_buffer) <= 0) {
        EXT2_ERROR("Failed to write block");
        free(block_buffer);
        return 0;
    }

    free(block_buffer);
    return blocks_written;
}


uint8_t ext2_allocate_blocks(struct ext2_partition* partition, struct ext2_inode_descriptor_generic * inode, uint32_t blocks_to_allocate) {
    uint32_t blocks_allocated = 0;
    for (uint32_t i = 0; i < 12; i++) {
        if (blocks_allocated == blocks_to_allocate) break;
        if (!inode->i_block[i]) {
            inode->i_block[i] = ext2_allocate_block(partition);
            if (!inode->i_block[i]) {
                EXT2_ERROR("Failed to allocate block");
                return 1;
            }
            blocks_allocated++;
        }
    }

    if (blocks_allocated == blocks_to_allocate) {
        EXT2_DEBUG("Allocated all blocks");
        return 0;
    }

    blocks_allocated += ext2_allocate_indirect_block(partition, &inode->i_block[12], blocks_to_allocate - blocks_allocated);

    if (blocks_allocated == blocks_to_allocate) {
        EXT2_DEBUG("Allocated all blocks");
        return 0;
    }

    blocks_allocated += ext2_allocate_double_indirect_block(partition, &inode->i_block[13], blocks_to_allocate - blocks_allocated);

    if (blocks_allocated == blocks_to_allocate) {
        EXT2_DEBUG("Allocated all blocks");
        return 0;
    }

    blocks_allocated += ext2_allocate_triple_indirect_block(partition, &inode->i_block[14], blocks_to_allocate - blocks_allocated);

    if (blocks_allocated == blocks_to_allocate) {
        EXT2_DEBUG("Allocated all blocks");
        return 0;
    }

    EXT2_ERROR("Failed to allocate all blocks");
    return 1;
}

int64_t ext2_read_inode_blocks(struct ext2_partition* partition, uint32_t inode_number, uint8_t * destination_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);
    if (inode == 0) return EXT2_READ_FAILED;

    uint32_t blocks_read = 0;
    int64_t read_result = 0;
    EXT2_DEBUG("First file block: %d", inode->i_block[0]);
    read_result = ext2_read_direct_blocks(partition, inode->i_block, 12, destination_buffer, count);
    if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;
    blocks_read += read_result;
    destination_buffer += read_result * block_size;
    EXT2_DEBUG("Basic read %d blocks, max: %ld", blocks_read, count);
    if (blocks_read >= count) return blocks_read;

    EXT2_DEBUG("Reading indirect block");
    read_result = ext2_read_indirect_blocks(partition, &(inode->i_block[12]), 1, destination_buffer, count - blocks_read);
    if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;
    blocks_read += read_result;
    destination_buffer += read_result * block_size;
    EXT2_DEBUG("Indirect read %d blocks, max: %ld", blocks_read, count);
    if (blocks_read >= count) return blocks_read;

    EXT2_DEBUG("Reading double indirect block");
    read_result = ext2_read_double_indirect_blocks(partition, &(inode->i_block[13]), 1, destination_buffer, count - blocks_read);
    if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;
    blocks_read += read_result;
    destination_buffer += read_result * block_size;
    EXT2_DEBUG("Double indirect read %d blocks, max: %ld", blocks_read, count);
    if (blocks_read >= count) return blocks_read;

    EXT2_DEBUG("Reading triple indirect block");
    read_result = ext2_read_triple_indirect_blocks(partition, &(inode->i_block[14]), 1, destination_buffer, count - blocks_read);
    if (read_result == EXT2_READ_FAILED || read_result == 0) return read_result;
    blocks_read += read_result;
    destination_buffer += read_result * block_size;
    EXT2_DEBUG("Triple indirect read %d blocks, max: %ld", blocks_read, count);
    if (blocks_read >= count) return blocks_read;

    EXT2_ERROR("File is too big for block size: %d", block_size);
    return EXT2_READ_FAILED;
}

int64_t ext2_read_inode_bytes(struct ext2_partition* partition, uint32_t inode_number, uint8_t * destination_buffer, uint64_t count) {
    uint64_t blocks = DIVIDE_ROUNDED_UP(count, 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
    int64_t result = ext2_read_inode_blocks(partition, inode_number, destination_buffer, blocks);
    if (result == EXT2_READ_FAILED) return EXT2_READ_FAILED;
    return result * (1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
}

int64_t ext2_write_inode_blocks(struct ext2_partition* partition, uint32_t inode_number, uint8_t * source_buffer, uint64_t count) {
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);
    if (inode == 0) return EXT2_WRITE_FAILED;

    uint32_t blocks_written = 0;
    int64_t write_result = 0;

    EXT2_DEBUG("Writing basic blocks %d", inode->i_block[0]);
    write_result = ext2_write_direct_blocks(partition, inode->i_block, 12, source_buffer, count);
    if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;
    blocks_written += write_result;
    source_buffer += write_result * block_size;
    EXT2_DEBUG("Basic write %d blocks, max: %ld", blocks_written, count);
    if (blocks_written >= count) return blocks_written;

    EXT2_DEBUG("Writing indirect block");
    write_result = ext2_write_indirect_blocks(partition, &(inode->i_block[12]), 1, source_buffer, count - blocks_written);
    if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;
    blocks_written += write_result;
    source_buffer += write_result * block_size;
    EXT2_DEBUG("Indirect write %d blocks, max: %ld", blocks_written, count);
    if (blocks_written >= count) return blocks_written;

    EXT2_DEBUG("Writing double indirect block");
    write_result = ext2_write_double_indirect_blocks(partition, &(inode->i_block[13]), 1, source_buffer, count - blocks_written);
    if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;
    blocks_written += write_result;
    source_buffer += write_result * block_size;
    EXT2_DEBUG("Double indirect write %d blocks, max: %ld", blocks_written, count);
    if (blocks_written >= count) return blocks_written;

    EXT2_DEBUG("Writing triple indirect block");
    write_result = ext2_write_triple_indirect_blocks(partition, &(inode->i_block[14]), 1, source_buffer, count - blocks_written);
    if (write_result == EXT2_WRITE_FAILED || write_result == 0) return write_result;
    blocks_written += write_result;
    source_buffer += write_result * block_size;
    EXT2_DEBUG("Triple indirect write  %d blocks, max: %ld", blocks_written, count);
    if (blocks_written >= count) return blocks_written;

    EXT2_ERROR("File is too big for block size: %d", block_size);
    return EXT2_WRITE_FAILED;
}

int64_t ext2_write_inode_bytes(struct ext2_partition* partition, uint32_t inode_number, uint8_t * source_buffer, uint64_t count) {
    uint64_t blocks = DIVIDE_ROUNDED_UP(count, 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
    EXT2_DEBUG("Writing %ld bytes to inode %d (%ld blocks)", count, inode_number, blocks);    
    int64_t result = ext2_write_inode_blocks(partition, inode_number, source_buffer, blocks);
    if (result == EXT2_WRITE_FAILED) return EXT2_WRITE_FAILED;
    return result * (1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size));
}