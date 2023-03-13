#include "dependencies.h"

void * __fuse_memcpy(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n);
}

u64 __fuse_lseek(int fd, u64 offset, int whence) {
    return lseek(fd, offset, whence);
}

u64 __fuse_read(int fd, void *buf, u64 count) {
    return read(fd, buf, count);
}

int __fuse_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}

int __fuse_fstat(int fd, struct stat *statbuf) {
    return fstat(fd, statbuf);
}

int __fuse_close(int fd) {
    return close(fd);
}

int __fuse_open(const char *pathname, int flags) {
    return open(pathname, flags);
}

//write to disk
u64 __fuse_write(int fd, const void *buf, u64 count) {
    return write(fd, buf, count);
}

void * __fuse_mmap(void *addr, u64 length, int prot, int flags, int fd, u64 offset) {
    return mmap(addr, length, prot, flags, fd, offset);
}

int __fuse_munmap(void *addr, u64 length) {
    return munmap(addr, length);
}

void * __fuse_malloc(u64 size) {
    return malloc(size);
}
void __fuse_free(void * ptr) {
    free(ptr);
}

int __fuse_strcmp(const char * str1, const char * str2) {
    return strcmp(str1, str2);
}

char *__fuse_strncpy(char *dest, const char *src, u64 n) {
    return strncpy(dest, src, n);
}

u64 __fuse_strlen(const char *s) {
    return strlen(s);
}