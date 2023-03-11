#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include "auxiliary.h"

uint32_t get_current_epoch() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

uint32_t get_uid() {
    return (uint32_t)getuid();
}

uint32_t get_gid() {
    return (uint32_t)getgid();
}