#include "primitives.h"
#include "dependencies.h"
#include "bfuse.h"
#ifdef __DEBUG_ENABLED
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"
#endif

int read_disk(const char* drive, void *buffer, int sector, int count) {
    
    struct mount* mount = get_drive(drive);
    if (mount == 0) {
        return OP_FAILURE;
    }
    u64 offset = sector * mount->sector_size;
    u64 size = count * mount->sector_size;

#ifdef __EAGER
    __fuse_memcpy(buffer, mount->file_ptr + offset, size);
#else
    __fuse_lseek(mount->file_handle, offset, __fuse_SEEK_SET);
    if (__fuse_read(mount->file_handle, buffer, size) != size) {
        return OP_FAILURE;
    }
    __fuse_lseek(mount->file_handle, 0, __fuse_SEEK_SET);
#endif
    return OP_SUCCESS;
}

int write_disk(const char * drive, void *buffer, int sector, int count) {

}

int ioctl_disk(const char * drive, int request, void *buffer) {

}

int get_disk_status(const char * drive) {

}

int init_disk(const char * drive) {

}