#include "ext2.h"
#include "ext2_inode.h"

#include "../fused/primitives.h"
#include "../fused/auxiliary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t ext2_get_file_size(struct ext2_partition* partition, const char* path) {
    uint32_t inode_number = ext2_path_to_inode(partition, path);
    struct ext2_inode_descriptor_generic * inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);
    return inode->i_size;
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
