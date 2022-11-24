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
    struct mount* mount = get_drive(drive);
    if (mount == 0) {
        return OP_FAILURE;
    }
    u64 offset = sector * mount->sector_size;
    u64 size = count * mount->sector_size;

#ifdef __EAGER
    __fuse_memcpy(mount->file_ptr + offset, buffer, size);
#else
    __fuse_lseek(mount->file_handle, offset, __fuse_SEEK_SET);
    if (__fuse_write(mount->file_handle, buffer, size) != size) {
        return OP_FAILURE;
    }
    __fuse_lseek(mount->file_handle, 0, __fuse_SEEK_SET);
#endif
    return OP_SUCCESS;
}

int ioctl_disk(const char * drive, int request, void *buffer) {
    struct mount* mount = get_drive(drive);
    if (mount == 0) {
        return OP_FAILURE;
    }

    u64 result = *(u64*) buffer;
    switch (request) {
        case IOCTL_SYNC:                {return OP_FAILURE;}
        case IOCTL_TRIM:                {return OP_FAILURE;}
        case IOCTL_GET_SECTOR_SIZE:     {result = mount->sector_size; break;}
        case IOCTL_GET_SECTOR_COUNT:    {result = mount->sector_count; break;}
        case IOCTL_IDLE:                {mount->power_state = DEV_PWR_IDLE; break;}
        case IOCTL_POWEROFF:            {mount->power_state = DEV_PWR_OFF; break;}
        case IOCTL_LOCK:                {mount->can_eject = 0; break;}
        case IOCTL_UNLOCK:              {mount->can_eject = 1; break;}
        case IOCTL_ATA_GET_REV:         {__fuse_memcpy(buffer, mount->ATA_REVISION, ATA_REV_LEN); break;}
        case IOCTL_ATA_GET_MODEL:       {__fuse_memcpy(buffer, mount->ATA_MODEL, ATA_MODEL_LEN); break;}
        case IOCTL_ATA_GET_SN:          {__fuse_memcpy(buffer, mount->ATA_SERIAL, ATA_SN_LEN); break;}
        case IOCTL_EJECT:               {
            if (mount->can_eject) {
                mount->power_state = DEV_PWR_EJECTED;
                break;
            } else {
                return OP_FAILURE;
            }
        }
        default:                        {return OP_FAILURE;}
    }
    memcpy(buffer, &result, sizeof(u32));
    return OP_SUCCESS;
}

int get_disk_status(const char * drive) {
    struct mount* mount = get_drive(drive);
    if (mount == 0) {
        return STATUS_NON_PRESENT;
    }

    if (mount->configured == 0) {
        return STATUS_NOT_READY;
    }

    //TODO: Add busy option after delay emulation is implemented
    return STATUS_READY;
}

int init_disk(const char * drive) {
    struct mount* mount = get_drive(drive);
    if (mount == 0) {
        return OP_FAILURE;
    }

    //Drive startup code goes here
    //--------------------------------

    //--------------------------------

    mount->configured = 1;
    return OP_SUCCESS;
}