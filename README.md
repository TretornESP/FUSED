# FUSED 

Userspace driver for developing filesystems

## What is FUSED?

FUSED enables you to map any raw image and expose a device driver interface through 4 functions:

* `read_disk`
* `write_disk`
* `ioctl_disk`
* `get_disk_status`
* `init_disk`

## How to test FUSED?

You can start by creating a test scenario by simply running:
    
    ```./maketest.sh```

Requires `mkfs.ext2` and `dd` to be installed.
Then just run `./go.sh` to run the test scenario.

## How to use FUSED in your own project?