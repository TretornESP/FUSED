#include "fused/bfuse.h"
#include "demofs/ext2.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {

    (void)argc;
    (void)argv;

    const char drive[]= "/dev/hda";
    register_drive("/mnt/c/Users/85562/FUSED/build/img/dummy.img", drive, 512);
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
    for (uint32_t i = 0; i < partitions; i++) {
        struct ext2_partition * partition = ext2_get_partition_by_index(i);
        uint64_t file_size = ext2_get_file_size(partition, "/test.input");
        uint8_t * buffer = ext2_buffer_for_size(partition, file_size);
        if (partition) {
            if (ext2_read_file(partition, "/test.input", buffer, file_size)) {
                FILE * file = fopen("./test/test.output", "wb");
                fwrite(buffer, 1, file_size, file);
                fclose(file);
            } else {
                printf("Failed to read file\n");
            }
        }

        free(buffer);
    }
}
