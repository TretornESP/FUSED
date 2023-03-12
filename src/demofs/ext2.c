#include "ext2.h"
#include "ext2_block.h"
#include "ext2_util.h"
#include "ext2_inode.h"
#include "ext2_bg.h"
#include "ext2_sb.h"
#include "ext2_dentry.h"

//Supress -Waddress-of-packed-member warning
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#include "../fused/primitives.h"
#include "../fused/auxiliary.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct ext2_partition * ext2_partition_head = 0x0;

struct ext2_partition * ext2_register_partition(const char* disk, uint32_t lba) {
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

uint8_t ext2_search(const char* name, uint32_t lba) {
    uint8_t bpb[1024];
    if (read_disk(name, bpb, lba+2, 2)) return 0;

    struct ext2_superblock *sb = (struct ext2_superblock *)bpb;
    return (sb->s_magic == EXT2_SUPER_MAGIC);
}

uint8_t ext2_unregister_partition(char letter) {
    (void)letter;
    printf("[EXT2] unregstr_ext2_partition Not implemented\n");
    return 0;
}

//Operations over files

uint64_t ext2_get_file_size(struct ext2_partition* partition, const char* path) {
    uint32_t inode_number = ext2_path_to_inode(partition, path);
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);
    return inode->i_size;
}

const char * ext2_get_partition_name(struct ext2_partition * partition) {
    return partition->name;
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
    if (ext2_allocate_blocks(partition, inode, blocks_to_allocate)) {
        printf("[EXT2] Failed to allocate blocks\n");
        return 0;
    }

    inode->i_size = new_size;
    if (!ext2_write_inode(partition, inode_index, (struct ext2_inode_descriptor*)inode)) {
        printf("[EXT2] Failed to write inode\n");
        return 0;
    }

    ext2_flush_structures(partition);
    return 1;
}

uint8_t ext2_read_file(struct ext2_partition * partition, const char * path, uint8_t * destination_buffer, uint64_t size, uint64_t skip) {
    //TODO: This way of skipping bytes is an aberration!
    uint32_t inode_index = ext2_path_to_inode(partition, path);
    uint32_t block_size = 1024 << ((struct ext2_superblock*)partition->sb)->s_log_block_size;
    if (!inode_index) {printf("err ino index\n");return 0;}

    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_index);
    if (inode->i_mode & INODE_TYPE_DIR) {printf("err ino type\n");return 0;}

    uint64_t file_size = inode->i_size;
    uint8_t * full_buffer = ext2_buffer_for_size(block_size, file_size);

    int64_t read_bytes = ext2_read_inode_bytes(partition, inode_index, full_buffer, file_size);
    if (read_bytes == EXT2_READ_FAILED) {printf("err file read bytes\n");return 0;}
    if (read_bytes <= 0) {printf("err read bytes\n");return 0;}

    memcpy(destination_buffer, full_buffer + skip, size);
    free(full_buffer);
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