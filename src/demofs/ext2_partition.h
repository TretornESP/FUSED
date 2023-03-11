#ifndef _EXT2_PARTITION_H
#define _EXT2_PARTITION_H

#include "ext2.h"

#include <stdint.h>

#define SB_OFFSET_LBA           2
#define BGDT_BLOCK              1
#define BLOCK_NUMBER            4
#define MAX_DISK_NAME_LENGTH    32
#define EXT2_SUPER_MAGIC        0xEF53

#define EXT2_ROOT_INO_INDEX 2
#define EXT2_EOF 0xFFFFFFFF

void ext2_disk_from_partition(char * destination, const char * partition);
struct ext2_partition * get_partition(const char * partno);
uint32_t ext2_count_partitions();
struct ext2_partition * register_ext2_partition(const char* disk, uint32_t lba);
uint8_t unregister_ext2_partition(char letter);
struct ext2_partition * ext2_get_partition_by_index(uint32_t index);
uint8_t ext2_check_status(const char* disk);

#endif /* _EXT2_PARTITION_H */