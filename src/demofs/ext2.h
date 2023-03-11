//https://wiki.osdev.org/Ext2
#ifndef _EXT2_H
#define _EXT2_H

#include <stdint.h>

//Fix by FunkyWaddle: Inverted values
#define DIVIDE_ROUNDED_UP(x, y) ((x % y) ? ((x) + (y) - 1) / (y) : (x) / (y))
#define SWAP_ENDIAN_16(x) (((x) >> 8) | ((x) << 8))
#define EXT2_NAME_LEN           255

struct ext2_superblock {
    uint32_t s_inodes_count;        /* Inodes count */
    uint32_t s_blocks_count;        /* Blocks count */
    uint32_t s_r_blocks_count;      /* Reserved blocks count */
    uint32_t s_free_blocks_count;   /* Free blocks count */
    uint32_t s_free_inodes_count;   /* Free inodes count */
    uint32_t s_first_sb_block;      /* First superblock Block */
    uint32_t s_log_block_size;      /* Block size */
    uint32_t s_log_frag_size;       /* Fragment size */
    uint32_t s_blocks_per_group;    /* # Blocks per group */
    uint32_t s_frags_per_group;     /* # Fragments per group */
    uint32_t s_inodes_per_group;    /* # Inodes per group */
    uint32_t s_mtime;               /* Mount time */
    uint32_t s_wtime;               /* Write time */
    uint16_t s_mnt_count;           /* Mount count */
    uint16_t s_max_mnt_count;       /* Maximal mount count */
    uint16_t s_magic;               /* Magic signature */
    uint16_t s_state;               /* File system state */
    uint16_t s_errors;              /* Behaviour when detecting errors */
    uint16_t s_minor_rev_level;     /* minor revision level */
    uint32_t s_lastcheck;           /* time of last check */
    uint32_t s_checkinterval;       /* max. time between checks */
    uint32_t s_creator_os;          /* OS */
    uint32_t s_rev_level;           /* Revision level */
    uint16_t s_def_resuid;          /* Default uid for reserved blocks */
    uint16_t s_def_resgid;          /* Default gid for reserved blocks */
} __attribute__((packed));

struct ext2_superblock_extended {
    struct ext2_superblock sb;
    uint32_t s_first_ino;           /* First non-reserved inode */
    uint16_t s_inode_size;          /* size of inode structure */
    uint16_t s_block_group_nr;      /* block group # of this superblock */
    uint32_t s_feature_compat;      /* compatible feature set */
    uint32_t s_feature_incompat;    /* incompatible feature set */
    uint32_t s_feature_ro_compat;   /* readonly-compatible feature set */
    uint8_t  s_uuid[16];            /* 128-bit uuid for volume */
    char     s_volume_name[16];     /* volume name */
    char     s_last_mounted[64];    /* directory where last mounted */
    uint32_t s_algo_bitmap;         /* For compression */
    uint8_t  s_prealloc_blocks;     /* Nr of blocks to try to preallocate*/
    uint8_t  s_prealloc_dir_blocks; /* Nr to preallocate for dirs */
    uint16_t s_padding1;
    uint8_t  s_journal_uuid[16];    /* uuid of journal superblock */
    uint32_t s_journal_inum;        /* inode number of journal file */
    uint32_t s_journal_dev;         /* device number of journal file */
    uint32_t s_last_orphan;         /* start of list of inodes to delete */
    uint8_t  s_unused[788];         /* Padding to the end of the block */
} __attribute__((packed));

struct ext2_block_group_descriptor {
    uint32_t bg_block_bitmap;        /* Blocks bitmap block */
    uint32_t bg_inode_bitmap;       /* Inodes bitmap block */
    uint32_t bg_inode_table;        /* Inodes table block */
    uint16_t bg_free_blocks_count;  /* Free blocks count */
    uint16_t bg_free_inodes_count;  /* Free inodes count */
    uint16_t bg_used_dirs_count;    /* Directories count */
    uint8_t  bg_pad[14];            /* Padding to the end of the block */
} __attribute__((packed));

struct ext2_partition {
    char name[32];
    char disk[32];
    uint32_t lba;
    uint32_t group_number;
    uint32_t sector_size;
    uint32_t bgdt_block;
    uint32_t sb_block;
    struct ext2_superblock_extended *sb;
    struct ext2_block_group_descriptor *gd;
    struct ext2_partition *next;
};

struct ext2_directory_entry {
    uint32_t inode;                 /* Inode number */
    uint16_t rec_len;               /* Directory entry length */
    uint8_t  name_len;              /* Name length */
    uint8_t  file_type;             /* File type */
    char     name[EXT2_NAME_LEN];   /* File name */ //IM SO FUCKING DUMB
} __attribute__((packed));

//Directory entry types
#define EXT2_DIR_TYPE_UNKNOWN   0
#define EXT2_DIR_TYPE_REGULAR   1
#define EXT2_DIR_TYPE_DIRECTORY 2
#define EXT2_DIR_TYPE_CHARDEV   3
#define EXT2_DIR_TYPE_BLOCKDEV  4
#define EXT2_DIR_TYPE_FIFO      5
#define EXT2_DIR_TYPE_SOCKET    6
#define EXT2_DIR_TYPE_SYMLINK   7

void ext2_list_directory(struct ext2_partition* partition, const char * path);
uint8_t ext2_flush_structures(struct ext2_partition * partition);
uint8_t ext2_search(const char* name, uint32_t lba);
uint8_t ext2_read_file(struct ext2_partition * partition, const char * path, uint8_t * destination_buffer, uint64_t size, uint64_t skip);
void ext2_dump_inode_bitmap(struct ext2_partition * partition);
uint8_t ext2_create_directory_entry(struct ext2_partition* partition, uint32_t inode_number, uint32_t child_inode, const char* name, uint32_t type);
uint8_t ext2_create_file(struct ext2_partition * partition, const char* path);
uint8_t ext2_resize_file(struct ext2_partition* partition, uint32_t inode_index, uint32_t new_size);
uint8_t ext2_write_file(struct ext2_partition * partition, const char * path, uint8_t * source_buffer, uint64_t size, uint64_t skip);

#endif