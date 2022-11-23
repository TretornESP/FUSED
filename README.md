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

For this you will have to:

1. Set the config variables in `config.h` to your liking (comment them to disable)
**Note:** `LAZY MODE (EAGER COMMENTED)` is unfinished
2. (Optional) modify the stubs in `dependencies.h` and `dependencies.c` to suit your environment (they start by __fuse_)
3. Start implementing your fs by including `primitives.h`
4. In the code used to test your fs implementation include `bfuse.h` and register the device to the driver by running:

    ```register_drive("./path/to/image.img", "mount point string", 512); // 512 is the sector size``` 

5. After that, you are golden, now you can run call the driver in your fs, remember to identify the device throgh the mount string