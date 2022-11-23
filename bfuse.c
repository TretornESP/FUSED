#include "bfuse.h"
#include "dependencies.h"

struct mount * mount_header = 0x0;

void add_mount(const char * mount_point, const char * drive, u8 * buffer, u64 start_sector, u64 sector_count) {
    struct mount * new_mount = __malloc(sizeof(struct mount));
    new_mount->mount_point = mount_point;
    new_mount->drive = drive;
    new_mount->buffer = buffer;
    new_mount->start_sector = start_sector;
    new_mount->sector_count = sector_count;
    new_mount->next = mount_header;
    mount_header = new_mount;
}

void remove_mount(const char * mount_point) {
    struct mount * current = mount_header;
    struct mount * previous = 0x0;
    while (current != 0x0) {
        if (__strcmp(current->mount_point, mount_point) == 0) {
            if (previous == 0x0) {
                mount_header = current->next;
            } else {
                previous->next = current->next;
            }
            __free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

struct mount * get_mount(const char * mount_point) {
    struct mount * current = mount_header;
    while (current != 0x0) {
        if (__strcmp(current->mount_point, mount_point) == 0) {
            return current;
        }
        current = current->next;
    }
    return 0x0;
}

struct mount* get_drive(char *mount_point) {
    struct mount * mount = get_mount(mount_point);
    if (mount == 0x0) {
        return 0x0;
    }
    return mount;
}

void register_drive(const char * filename, const char* mount_point, u32 sector_size) {
    
}

void register_drive_subsection(const char* filename, const char* mount_point, u32 sector_size, u64 starting_sector, u64 sector_count);

void unregister_drive(const char *mount_point) {
    remove_mount(mount_point);
}