#include "fused/bfuse.h"
#include "demofs/ext2.h"
#include <stdio.h>
int main(int argc, char *argv[]) {
    const char drive[]= "/dev/hda";
    register_drive("/mnt/c/Users/xabier.iglesias/fuse/build/img/dummy.img", drive, 512);
    if (ext2_search(drive, 0)) {
        if (register_ext2_partition(drive, 0)) {
            printf("Registered ext2 partition\n");
        } else {
            printf("Failed to register ext2 partition\n");
        }
    } else {
        printf("Failed to find ext2 partition\n");
    }
}
