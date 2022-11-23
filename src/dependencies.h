#ifndef _DEPENDENCIES_H
#define _DEPENDENCIES_H

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>

typedef struct stat __fuse_struct_stat;

#define __fuse_PROT_WRITE    PROT_WRITE
#define __fuse_PROT_READ     PROT_READ
#define __fuse_SEEK_SET      SEEK_SET
#define __fuse_MAP_FAILED    MAP_FAILED
#define __fuse_MAP_SHARED    MAP_SHARED
#define __fuse_O_RDWR        O_RDWR


void * __fuse_memcpy(void *dest, const void *src, size_t n);
u64 __fuse_lseek(int fd, u64 offset, int whence);
u64 __fuse_read(int fd, void *buf, u64 count);
int __fuse_printf(const char *format, ...);
int __fuse_fstat(int fd, struct stat *statbuf);
int __fuse_close(int fd);
int __fuse_open(const char *pathname, int flags);
void * __fuse_mmap(void *addr, u64 length, int prot, int flags, int fd, u64 offset);
int __fuse_munmap(void *addr, u64 length);
void * __fuse_malloc(u64 size);
void __fuse_free(void * ptr);
int __fuse_strcmp(const char * str1, const char * str2);
char *__fuse_strncpy(char *dest, const char *src, u64 n);
u64 __fuse_strlen(const char *s);
#endif