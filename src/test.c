#include "fused/bfuse.h"
#include "demofs/ext2.h"
#include <stdio.h>
#include <stdlib.h>

#define TEST_SKIP 0
//#define TEST_SIZE 0

int main(int argc, char *argv[]) {

    (void)argc;
    (void)argv;
    
    const char drive[]= "/mnt/hda";
    
    register_drive("/mnt/c/Users/xabier.iglesias/fuse/build/img/dummy.img", drive, 512);
    if (ext2_search(drive, 0)) {
        if (register_ext2_partition(drive, 0)) {
            printf("Registered ext2 partition\n");
        } else {
            printf("Failed to register ext2 partition\n");
            return 1;
        }
    } else {
        printf("Failed to find ext2 partition\n");
        return 1;
    }

    uint32_t partitions = ext2_count_partitions();
    printf("Found %d partitions\n", partitions);
    for (uint32_t i = 0; i < partitions; i++) {

        struct ext2_partition * partition = ext2_get_partition_by_index(i);
        printf("Partition %d: %s\n", i, partition->name);

        #ifndef TEST_SIZE
        uint64_t file_size = ext2_get_file_size(partition, "/test.input");
        #else
        uint64_t file_size = TEST_SIZE;
        #endif

        printf("File size: %lu\n", file_size);

        uint8_t * buffer = malloc(file_size);
        if (buffer == 0) {
            printf("Failed to allocate buffer\n");
            return 1;
        }

        printf("Listing files:\n");
        ext2_list_directory(partition, "/");
        printf("Listing files done\n");

        if (partition) {
            if (ext2_read_file(partition, "/test.input", buffer, file_size, TEST_SKIP)) {
                if (ext2_write_file(partition, "/test.output", buffer, file_size, TEST_SKIP)) {
                    free(buffer);
                    buffer = malloc(file_size);
                    ext2_read_file(partition, "/test.output", buffer, file_size, TEST_SKIP);
                    FILE * file = fopen("./test/test.output", "wb");
                    fwrite(buffer, 1, file_size, file);
                    fclose(file);
                    printf("File copied\n");
                } else {
                    printf("Failed to copy file\n");
                }
            } else {
                printf("Failed to read file\n");
            }
        }

        free(buffer);
    }
}
