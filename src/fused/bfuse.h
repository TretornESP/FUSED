#ifndef _BFUSE_H
#define _BFUSE_H
#include "config.h"
#define MAX_FILE_NAME_LENGTH  256
#define MAX_DRIVE_NAME_LENGTH 256

#define DEV_PWR_IDLE        0
#define DEV_PWR_ON          1
#define DEV_PWR_OFF         2
#define DEV_PWR_EJECTED     3


#define ATA_REV_STRING "ATA Revision 3.0"
#define ATA_SERIAL_STRING "69420694206942069420"
#define ATA_MODEL_STRING "BFUSE emulated disk"

#define ATA_REV_LEN        16
#define ATA_MODEL_LEN      40
#define ATA_SN_LEN         20

struct mount {
    const char ATA_REVISION[ATA_REV_LEN];
    const char ATA_MODEL[ATA_MODEL_LEN];
    const char ATA_SERIAL[ATA_SN_LEN];

    char mount_point[MAX_DRIVE_NAME_LENGTH];
    char file_name[MAX_FILE_NAME_LENGTH];

#ifdef __EAGER
    u8   *file_ptr;
#else
    int  file_handle;
#endif
    u8   configured;
    u8   can_eject;
    u8   power_state;
    u32  sector_size;
    u64  starting_sector;
    u64  sector_count;

    struct mount * next;
};

#ifdef __DEBUG_ENABLED
//Debug only, deleteme
void debug();
#endif

//Get the drive id from the mount point
struct mount* get_drive(const char *mount_point);

//Register an entire file as a drive, size must be multiple of sector size
void register_drive(const char * filename, const char* mount_point, u32 sector_size);

//Register a part of a file as a drive.
void register_drive_subsection(const char* filename, const char* mount_point, u32 sector_size, u64 starting_sector, u64 sector_count);

//Unregister a drive
void unregister_drive(const char *mount_point);

#endif