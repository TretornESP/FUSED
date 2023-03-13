#pragma GCC diagnostic ignored "-Wvariadic-macros"

#include "ext2_dentry.h"
#include "ext2_util.h"
#include "ext2_inode.h"
#include "ext2_block.h"
#include "ext2_integrity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_MAX 16 //TODO: Changeme

uint8_t ext2_delete_dentry(struct ext2_partition* partition, const char * path) {
    char * name;
    char * parent_path;
    if (!ext2_path_to_parent_and_name(path, &parent_path, &name)) return 1;
    
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);

    uint32_t parent_inode_number = ext2_path_to_inode(partition, parent_path);
    struct ext2_inode_descriptor_generic * parent_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, parent_inode_number);

    uint32_t target_inode_number = ext2_path_to_inode(partition, path);
    struct ext2_inode_descriptor * target_inode_full = ext2_read_inode(partition, target_inode_number);
    struct ext2_inode_descriptor_generic * target_inode = (struct ext2_inode_descriptor_generic *)&(target_inode_full->id);
    uint8_t deleted = 0;
    if (parent_inode->i_mode & INODE_TYPE_DIR) {
        uint8_t *block_buffer = malloc(parent_inode->i_size + block_size);
        ext2_read_inode_bytes(partition, parent_inode_number, block_buffer, parent_inode->i_size);

        uint32_t parsed_bytes = 0;
        
        EXT2_DEBUG("Deleting file %s", path);
        struct ext2_directory_entry* previous_entry = 0;
        while (parsed_bytes < parent_inode->i_size) {
            struct ext2_directory_entry *entry = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            if (entry->inode == target_inode_number) {
                entry->inode = 0;
                entry->name_len = 0;
                entry->file_type = 0;
                entry->name[0] = 0;
                if (previous_entry) {
                    previous_entry->rec_len += entry->rec_len;
                }
                entry->rec_len = 0;

                EXT2_DEBUG("Deleted dentry");
                deleted = 1;

                break;
            }
            previous_entry = entry;
            parsed_bytes += entry->rec_len;
        }
        if (deleted)
            ext2_write_inode_bytes(partition, parent_inode_number, block_buffer, parent_inode->i_size);
        free(block_buffer);
    }



    free(parent_path);
    free(name);
    
    if (deleted) {
        target_inode->i_links_count--;
        if (target_inode->i_links_count == 0) {
            target_inode->i_dtime = ext2_get_current_epoch();
        }
        ext2_write_inode(partition, target_inode_number, target_inode_full);
        return 0;
    }
    
    return 1;
}

void ext2_list_dentry(struct ext2_partition* partition, const char * path) {
    uint32_t inode_number = ext2_path_to_inode(partition, path);
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * root_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);

    if (root_inode->i_mode & INODE_TYPE_DIR) {
        uint8_t *block_buffer = malloc(root_inode->i_size + block_size);
        ext2_read_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size);

        uint32_t parsed_bytes = 0;
        uint32_t list_count = 0;
        printf("[EXT2] Directory listing for %s\n", path);
        while (parsed_bytes < root_inode->i_size && list_count < LIST_MAX) {
            struct ext2_directory_entry *entry = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            printf("[EXT2] ino: %d rec_len: %d name_len: %d file_type: %d name: %s\n", entry->inode, entry->rec_len, entry->name_len, entry->file_type, entry->name);
            parsed_bytes += entry->rec_len;
            list_count++;
        }
       
        free(block_buffer);
    }
}

uint32_t ext2_get_all_dirs(struct ext2_partition* partition, const char* parent_path, struct ext2_directory_entry** entries) {
    uint32_t inode_number = ext2_path_to_inode(partition, parent_path);
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * root_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);

    if (root_inode->i_mode & INODE_TYPE_DIR) {
        uint8_t *block_buffer = malloc(root_inode->i_size + block_size);
        ext2_read_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size);

        uint32_t parsed_bytes = 0;
        uint32_t entry_count = 0;
        while (parsed_bytes < root_inode->i_size) {
            struct ext2_directory_entry *entry = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            if (entry->inode != 0) {
                entry_count++;
            }
            parsed_bytes += entry->rec_len;
        }
        *entries = malloc(sizeof(struct ext2_directory_entry) * entry_count);
        parsed_bytes = 0;
        entry_count = 0;
        while (parsed_bytes < root_inode->i_size) {
            struct ext2_directory_entry *entry = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            if (entry->inode != 0) {
                (*entries)[entry_count] = *entry;
                entry_count++;
            }
            parsed_bytes += entry->rec_len;
        }
        free(block_buffer);
        return entry_count;
    }
    return 0;

}

uint8_t ext2_operate_on_dentry(struct ext2_partition* partition, const char* path, uint8_t (*callback)(struct ext2_partition* partition, uint32_t inode_entry)) {
    uint32_t inode_number = ext2_path_to_inode(partition, path);
    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    struct ext2_inode_descriptor_generic * root_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);

    if (root_inode->i_mode & INODE_TYPE_DIR) {
        uint8_t *block_buffer = malloc(root_inode->i_size + block_size);
        ext2_read_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size);

        uint32_t parsed_bytes = 0;
        
        while (parsed_bytes < root_inode->i_size) {
            struct ext2_directory_entry *entry = (struct ext2_directory_entry *) (block_buffer + parsed_bytes);
            if (callback(partition, entry->inode)) {
                free(block_buffer);
                return 1;
            }
            parsed_bytes += entry->rec_len;
        }
       
        free(block_buffer);
    }
    return 0;
}

uint8_t ext2_initialize_directory(struct ext2_partition* partition, uint32_t inode_number, uint32_t parent_inode_number) {
    struct ext2_inode_descriptor_generic * root_inode = (struct ext2_inode_descriptor_generic *)ext2_read_inode(partition, inode_number);

    //Create . and .. entries
    struct ext2_directory_entry entries[2] = {
        {
            .inode = inode_number,
            .rec_len = 0,
            .name_len = 1,
            .file_type = EXT2_FILE_TYPE_DIRECTORY,
            .name = "."
        },
        {
            .inode = parent_inode_number,
            .rec_len = 0,
            .name_len = 2,
            .file_type = EXT2_FILE_TYPE_DIRECTORY,
            .name = ".."
        }
    };

    uint32_t entry_size_first = sizeof(struct ext2_directory_entry) + entries[0].name_len - EXT2_NAME_LEN;
    entries[0].rec_len = (entry_size_first + 3) & ~3;

    uint32_t entry_size_second = sizeof(struct ext2_directory_entry) + entries[1].name_len - EXT2_NAME_LEN;
    entries[1].rec_len = 1024 - entries[0].rec_len;

    uint32_t block_size = 1024 << (((struct ext2_superblock*)partition->sb)->s_log_block_size);
    uint8_t *block_buffer = malloc(block_size);
    memset(block_buffer, 0, block_size);

    ext2_read_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size);

    memcpy(block_buffer, &entries[0], entry_size_first);
    memcpy(block_buffer + entries[0].rec_len, &entries[1], entry_size_second);


    if (ext2_write_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size)) {
        EXT2_ERROR("Failed to write directory entry");
        free(block_buffer);
        return 1;
    }

    free(block_buffer);
    return 1;
}

uint8_t ext2_create_directory_entry(struct ext2_partition* partition, uint32_t inode_number, uint32_t child_inode, const char* name, uint32_t type) {
    EXT2_DEBUG("File name: %s, Parent inode: %d, Type: %d", name, inode_number, type);

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
            EXT2_ERROR("No directory entries");
            free(block_buffer);
            return 1;
        }

        uint32_t entry_size = sizeof(struct ext2_directory_entry) + entry->name_len - EXT2_NAME_LEN;
        uint32_t entry_size_aligned = (entry_size + 3) & ~3;

        if (entry_size_aligned + child_entry.rec_len > entry->rec_len) {
            EXT2_ERROR("No space for directory entry");
            free(block_buffer);
            return 1;
        }

        uint32_t original_rec_len = entry->rec_len;
        entry->rec_len = entry_size_aligned;
        child_entry.rec_len = original_rec_len - entry_size_aligned;

        memcpy(block_buffer + parsed_bytes - original_rec_len, entry, entry_size);
        memcpy(block_buffer + parsed_bytes - original_rec_len + entry_size_aligned, &child_entry, sizeof(struct ext2_directory_entry) - EXT2_NAME_LEN);
        memcpy(block_buffer + parsed_bytes - original_rec_len + entry_size_aligned + sizeof(struct ext2_directory_entry) - EXT2_NAME_LEN, name, child_entry.name_len);

        if (ext2_write_inode_bytes(partition, inode_number, block_buffer, root_inode->i_size) <= 0) {
            EXT2_ERROR("Failed to write directory entry");
            free(block_buffer);
            return 1;
        }

        free(block_buffer);
        EXT2_DEBUG("Directory entry created");
        return 0;
    } else {
        printf("Inode is: %d\n", root_inode->i_mode);
        return 1;
    }
}