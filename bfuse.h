#ifndef _BFUSE_H
#define _BFUSE_H
#include "config.h"
#define MAX_FILE_NAME_LENGTH 256
#define MAX_DRIVE_NAME_LENGTH 256

struct mount {
    char drive_name[MAX_DRIVE_NAME_LENGTH];
    char file_name[MAX_FILE_NAME_LENGTH];
    u8  *file_ptr;
    u32  sector_size;
    u64  starting_sector;
    u64  sector_count;
    struct mount * next;
};

//Get the drive id from the mount point
struct mount* get_drive(char *mount_point)

//Register an entire file as a drive, size must be multiple of sector size
void register_drive(const char * filename, const char* mount_point, u32 sector_size);

//Register a part of a file as a drive.
void register_drive_subsection(const char* filename, const char* mount_point, u32 sector_size, u64 starting_sector, u64 sector_count);

//Unregister a drive
void unregister_drive(const char *mount_point);

#endif