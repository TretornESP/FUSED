#include "bfuse.h"
#include "dependencies.h"

struct mount * mount_header = 0x0;

#ifdef __DEBUG_ENABLED
#include <stdio.h>
void debug() {
    struct mount * current = mount_header;
    if (current == 0)
        __fuse_printf("Empty list\n");
    while (current != 0x0) {
        __fuse_printf("Mount: %s\n", current->mount_point);
        current = current->next;
    }
}
#endif


#ifdef __EAGER
void add_mount(const char * mount_point, const char * file_name, u8 * buffer, u64 sector_size, u64 start_sector, u64 sector_count) {
#else
void add_mount(const char * mount_point, const char * file_name, int handle, u64 sector_size, u64 start_sector, u64 sector_count) {
#endif
    struct mount * new_mount = __fuse_malloc(sizeof(struct mount));
    __fuse_strncpy(new_mount->mount_point, mount_point, __fuse_strlen(mount_point));
    __fuse_strncpy(new_mount->file_name, file_name, __fuse_strlen(file_name));
#ifdef __EAGER
    new_mount->file_ptr = buffer;
#else
    new_mount->file_handle = handle;
#endif
    new_mount->sector_size = sector_size;
    new_mount->starting_sector = start_sector;
    new_mount->sector_count = sector_count;
    new_mount->next = mount_header;
    mount_header = new_mount;
}

void remove_mount(const char * mount_point) {
    struct mount * current = mount_header;
    struct mount * previous = 0x0;
    while (current != 0x0) {
        if (__fuse_strcmp(current->mount_point, mount_point) == 0) {
            if (previous == 0x0) {
                mount_header = current->next;
            } else {
                previous->next = current->next;
            }
            __fuse_free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

struct mount * get_mount(const char * mount_point) {
    struct mount * current = mount_header;
    while (current != 0x0) {
        if (__fuse_strcmp(current->mount_point, mount_point) == 0) {
            return current;
        }
        current = current->next;
    }
    return 0x0;
}

struct mount* get_drive(const char *mount_point) {
    struct mount * mount = get_mount(mount_point);
    if (mount == 0x0) {
        return 0x0;
    }
    return mount;
}

#ifdef __EAGER
uint8_t * load_file(const char* filename, u64 sector_size, u64 * sectors) {
#else
int load_file(const char* filename, u64 sector_size, u64 * sectors) {
#endif
    int file = __fuse_open(filename, __fuse_O_RDWR);
    if (file == -1) {
        __fuse_printf("Error opening file %s\n", filename);
        return (void*)0x0;
    }

    __fuse_struct_stat st;
    if (__fuse_fstat(file, &st) == -1) {
        __fuse_printf("Error getting file size\n");
        return (void*)0x0;
    }

    u64 file_size = st.st_size;
    *sectors = file_size / sector_size;

#ifdef __EAGER
    u8 * buffer = __fuse_mmap(0, file_size, __fuse_PROT_READ | __fuse_PROT_WRITE, __fuse_MAP_SHARED, file, 0);
    if (buffer == __fuse_MAP_FAILED) {
        __fuse_printf("Error mapping file\n");
        return (void*)0x0;
    }
    return buffer;
#else
    return file;
#endif
}


void register_drive(const char * filename, const char* mount_point, u32 sector_size) {
    u64 sector_count = 0;
    u8 * buffer = load_file(filename, sector_size, &sector_count);
    add_mount(mount_point, filename, buffer, sector_size, 0, sector_count);
}

void register_drive_subsection(const char* filename, const char* mount_point, u32 sector_size, u64 starting_sector, u64 sector_count) {
    add_mount(mount_point, filename, 0x0, sector_size, starting_sector, sector_count);
}

void unregister_drive(const char *mount_point) {
    remove_mount(mount_point);
}