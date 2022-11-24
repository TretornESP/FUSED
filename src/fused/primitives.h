
#ifndef _PRIMITIVES_H
#define _PRIMITIVES_H
#include "config.h"

#define OP_SUCCESS                  0    
#define OP_FAILURE                  1

#define STATUS_NOT_READY            0
#define STATUS_READY                1
#define STATUS_BUSY                 2
#define STATUS_NON_PRESENT          3
#define STATUS_UNCONTROLLED_ERROR   4

#define IOCTL_SYNC                  0
#define IOCTL_TRIM                  1
#define IOCTL_GET_SECTOR_SIZE       2
#define IOCTL_GET_SECTOR_COUNT      3
#define IOCTL_GET_BLOCK_SIZE        4
#define IOCTL_FORMAT                5
#define IOCTL_IDLE                  6
#define IOCTL_POWEROFF              7
#define IOCTL_LOCK                  8
#define IOCTL_UNLOCK                9
#define IOCTL_EJECT                10
#define IOCTL_SMART                11
#define IOCTL_MMC_TYPE             12
#define IOCTL_MMC_GET_CSD          13
#define IOCTL_MMC_GET_CID          14
#define IOCTL_MMC_GET_OCR          15
#define IOCTL_MMC_GET_SDSTAT       16
#define IOCTL_ATA_GET_REV          17
#define IOCTL_ATA_GET_MODEL        18
#define IOCTL_ATA_GET_SN           19
#define IOCTL_ISDIO_READ           20
#define IOCTL_ISDIO_WRITE          21
#define IOCTL_ISDIO_MRITE          22

//Reads n sectors with offset into buffer
//Returns 0 on success, 1 on failure
int read_disk(const char* drive, void *buffer, int sector, int count);
//Writes n sectors with offset from buffer
//Returns 0 on success, 1 on failure
int write_disk(const char * drive, void *buffer, int sector, int count);
//Sends a command to the disk, may send or receive data through buffer
//Returns 0 on success, 1 on failure
//Valid operations (recommended):
// 0 - Syncronize, make sure the disk is done writing
// 1 - Trim, remove data from the disk.
// 2 - Get sector size, returns the sector size in buffer (4 bytes)
// 3 - Get sector count, returns the sector count in buffer (8 bytes)
// 4 - Get block size, returns the block size in buffer (4 bytes)
//Valid operations (optional):
// 5 - Format, format the disk
// 6 - Idle, put the disk in idle mode
// 7 - Poweroff, poweroff the disk
// 8 - Lock, lock the eject mechanism
// 9 - Unlock, unlock the eject mechanism
// 10 - Eject, eject the disk
// 11 - Smart, get smart capabilities
// 12 - mmc type, get the mmc type
// 13 - mmc get csd, get the mmc csd
// 14 - mmc get cid, get the mmc cid
// 15 - mmc get ocr, get the mmc ocr
// 16 - mmc get sdstat, get the mmc sdstat
// 17 - ata get rev, get the ata revision
// 18 - ata get model, get the ata model
// 19 - ata get sn, get the ata serial number
// 20 - isdio read, read from the sdio
// 21 - isdio write, write to the sdio
// 22 - isdio mrite, write to the sdio multiple

int ioctl_disk(const char * drive, int request, void *buffer);
//Get status of the drive
//Possible status values:
//0 - drive is not ready
//1 - drive is ready
//2 - drive is busy
//3 - drive is not present
//4 - uncontrolled error
int get_disk_status(const char * drive);
//Initialize the disk
//Returns 0 on success, 1 on failure
int init_disk(const char * drive);
#endif