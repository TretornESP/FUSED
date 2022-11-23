#include "bfuse.h"
#include "ext2.h"

#include <stdio.h>

int main(int argc, char* argv[]) {
    register_drive("./test/raw.img", "mountpoint1", 512);
    register_drive("./test/ext2.img", "mountpoint2", 512);
    if (ext2_search("mountpoint1", 0)) {
        printf("Found ext2 filesystem on mountpoint1\n");
    }
    if (ext2_search("mountpoint2", 0)) {
        printf("Found ext2 filesystem on mountpoint2\n");
    }
}