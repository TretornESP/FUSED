#include "bfuse.h"
#include "primitives.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    register_drive("./test/img.raw", "mountpoint1", 512);

    uint8_t *buffer = malloc(512);

    int result = read_disk("mountpoint1", buffer, 0, 1);
    
    if (result == OP_SUCCESS) {
        for (int i = 0; i < 512; i++) {
            if (i % 16 == 0) {
                printf("\n");
            }
            printf("%x ", buffer[i]);
        }
    } else {
        printf("Error reading disk\n");
    }

    free(buffer);
}