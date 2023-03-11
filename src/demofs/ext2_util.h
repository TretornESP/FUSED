#ifndef _EXT2_UTIL_H
#define _EXT2_UTIL_H

#include "ext2.h"

#include <stdint.h>

#define EXT2_UNIQUE_START       0x00CAFE00

void epoch_to_date(char* date, uint32_t epoch);
uint8_t * ext2_buffer_for_size(struct ext2_partition * partition, uint64_t size);
uint32_t ext2_get_unique_id();
uint8_t ext2_path_to_parent_and_name(const char* source, char** path, char** name);

#endif /* _EXT2_UTIL_H */