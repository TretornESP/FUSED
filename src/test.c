#include "fused/bfuse.h"
#include "demofs/ext2.h"
#include <stdio.h>
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

    char name[32];
    uint32_t partitions = ext2_count_partitions();
    for (uint32_t i = 0; i < partitions; i++) {
        ext2_get_partition_name_by_index(name, i);
        printf("Searching root ino for partition: %s\n", name);
        ext2_read_root_inode(name);
    }
}
