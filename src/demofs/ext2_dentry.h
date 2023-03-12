#ifndef _EXT2_DENTRY_H
#define _EXT2_DENTRY_H

#include "ext2_sb.h"
#include <stdint.h>

//Directory entry types
#define EXT2_DIR_TYPE_UNKNOWN   0
#define EXT2_DIR_TYPE_REGULAR   1
#define EXT2_DIR_TYPE_DIRECTORY 2
#define EXT2_DIR_TYPE_CHARDEV   3
#define EXT2_DIR_TYPE_BLOCKDEV  4
#define EXT2_DIR_TYPE_FIFO      5
#define EXT2_DIR_TYPE_SOCKET    6
#define EXT2_DIR_TYPE_SYMLINK   7

struct ext2_directory_entry {
    uint32_t inode;                 /* Inode number */
    uint16_t rec_len;               /* Directory entry length */
    uint8_t  name_len;              /* Name length */
    uint8_t  file_type;             /* File type */
    char     name[EXT2_NAME_LEN];   /* File name */ //IM SO FUCKING DUMB
} __attribute__((packed));

#endif /* _EXT2_DENTRY_H */