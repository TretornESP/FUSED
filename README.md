![Logo](https://github.com/TretornESP/fused/raw/main/logo.png)

# FUSED 

Userspace driver for developing filesystems

## Roadmap
- [x] Basic FUSE driver
- [x] Read functions
- [x] Write functions
- [x] IOCTL functions
- [x] Startup functions
- [x] Status functions
- [x] ext2 demo
- [ ] Lazy loading
- [ ] Buffer cache emulation
- [ ] SSD Wear emulation

## What is FUSED?

FUSED enables you to map any raw image and expose a device driver interface through 4 functions:

* `read_disk`
* `write_disk`
* `ioctl_disk`
* `get_disk_status`
* `init_disk`

## How to test FUSED?

First create the build directory structure with `make setup`
You can start by creating a test scenario by simply running `make testsetup`
Then you may build the test program by runnin `make fuse` and running it by `make run`
Or simply do the last two steps with `make`

## How to use FUSED in your own project?

For this you will have to:

1. Set the config variables in `config.h` to your liking (comment them to disable)
**Note:** `LAZY MODE (EAGER COMMENTED)` is unfinished
2. (Optional) modify the stubs in `dependencies.h` and `dependencies.c` to suit your environment (they start by \_\_fuse\_)
3. Start implementing your fs by including `primitives.h`
4. In the code used to test your fs implementation include `bfuse.h` and register the device to the driver by running:

    ```register_drive("./path/to/image.img", "mount point string", 512); // 512 is the sector size``` 

5. After that, you are golden, now you can run call the driver in your fs, remember to identify the device throgh the mount string


## Make targets
### Setup and testing targets
* `make setup` - Creates the build directory structure
* `make testsetup` - Creates a test scenario with two files (empty and ext2 formatted)
* `make demo` - Creates a demo file that can be used to test the driver
* `make reset` - Resets the test scenario images
* `make test` - Runs the full test scenario
* `make cleantest` - Undo testsetup
* `make cleansetup` - Undo setup

### Dev targets
* `make pull` - Refreshes the repository
* `make upload` - Uploads the repository to the remote


### Build targets
* `make` - Cleans, builds and runs the test program
* `make all` - Same as above
* `make fuse` - Builds the program
* `make clean` - Deletes all compiled files

### Run targets
* `make run` - Runs the program
* `make debug` - Launches GDB