#include "fused/bfuse.h"
#include "demofs/ext2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEST_SKIP 0
//#define TEST_SIZE 0
//If the function returns 1 call ext2_stacktrace() to get the error stacktrace and then return 1
#define CATCH_ERROR(x) if (x == EXT2_RESULT_ERROR) { ext2_stacktrace(); return 1; }

int main(int argc, char *argv[]) {

    (void)argc;
    (void)argv;
    
    const char drive[]= "/mnt/hda";
    ext2_set_debug_base("/mnt/c/Users/85562/FUSED/src/demofs/");
    register_drive("/mnt/c/Users/85562/FUSED/build/img/dummy.img", drive, 512);
    if (ext2_search(drive, 0)) {
        if (ext2_register_partition(drive, 0)) {
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
        printf("Partition %d: %s\n", i, ext2_get_partition_name(partition));

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

        char dummy_buffer[] = "Hello world!";

        if (partition) {
            printf("Performing fs operations\n");
            
            printf("Reading file /test.input\n");
            CATCH_ERROR(ext2_read_file(partition, "/test.input", buffer, file_size, TEST_SKIP));
            
            printf("Creating file /patata.output\n");
            CATCH_ERROR(ext2_create_file(partition, "/patata.output", EXT2_FILE_TYPE_REGULAR));
            
            printf("Writing file /patata.output\n");
            CATCH_ERROR(ext2_write_file(partition, "/patata.output", (uint8_t*)dummy_buffer, strlen(dummy_buffer), TEST_SKIP));
            
            printf("Creating file /test.output\n");
            CATCH_ERROR(ext2_create_file(partition, "/test.output", EXT2_FILE_TYPE_REGULAR));
            
            printf("Creating directory /stuff\n");
            CATCH_ERROR(ext2_create_file(partition, "/stuff", EXT2_FILE_TYPE_DIRECTORY));

            printf("Creating file /stuff/test.output\n");
            CATCH_ERROR(ext2_create_file(partition, "/stuff/test.output", EXT2_FILE_TYPE_REGULAR));

            printf("Deleting file /patata.output\n");
            CATCH_ERROR(ext2_delete_file(partition, "/patata.output"));

            printf("Writing file /stuff/test.output\n");
            CATCH_ERROR(ext2_write_file(partition, "/stuff/test.output", buffer, file_size, TEST_SKIP));
            
            printf("Performing test\n");
            free(buffer);
            buffer = malloc(file_size);
            
            CATCH_ERROR(ext2_read_file(partition, "/stuff/test.output", buffer, file_size, TEST_SKIP));
            FILE * file = fopen("./test/test.output", "wb");
            fwrite(buffer, 1, file_size, file);
            fclose(file);

            printf("Final status of fs:\n");
            ext2_list_directory(partition, "/");

            printf("Deleting directory /stuff\n");
            CATCH_ERROR(ext2_delete_file(partition, "/stuff"));

        }

        free(buffer);
    }
}
