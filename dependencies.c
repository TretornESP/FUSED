#include "dependencies.h"
#include <stdlib.h>
#include <string.h>

void * __malloc(u64 size) {
    return malloc(size);
}
void __free(void * ptr) {
    free(ptr);
}

int __strcmp(const char * str1, const char * str2) {
    return strcmp(str1, str2);
}