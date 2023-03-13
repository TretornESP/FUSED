override FUSE := fuse
override GDBCFG := debug.gdb

CC := /usr/bin/cc
LD := /usr/bin/ld
GDB := gdb

CFLAGS ?= -O2 -g -Wall -Wextra -pipe -std=c11
GDBFLAGS ?= --nx --command=$(GDBCFG)

DISAS := intel
ENTRY := main

ABSDIR := $(shell pwd)
SRCDIR := $(ABSDIR)/src
BUILDHOME := $(ABSDIR)/build
BUILDDIR := $(BUILDHOME)/bin
OBJDIR := $(BUILDHOME)/lib
IMGDIR := $(BUILDHOME)/img
COMPATDIR := stuff

MOUNTPOINT := /mnt/$(FUSE)
TESTDIR := ./test
INFILE := test.input
OUTFILE := test.output
TESTEMPTY := 800
TESTBLK := 1024
TESTCNT := 700
RAW := raw.img
DUMMY := dummy.img

FS := ext2#Warning, this must be mkfs compatible
BLKSIZE := 1024
SECTSIZE = 1024
SECTCOUNT = 10000
DIRS := $(wildcard $(SRCDIR)/*)
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# Check that given variables are set and all have non-empty values,
# die with an error otherwise.
#
# Params:
#   1. Variable name(s) to test.
#   2. (optional) Error message to print.
check_defined = \
    $(strip $(foreach 1,$1, \
        $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
      $(error Undefined $1$(if $2, ($2))))

override CFLAGS +=       \
    -I.                  \
    -std=c11             \
    -m64

override CFLAGS += $(CEXTRA)

override CFILES :=$(call rwildcard,$(SRCDIR),*.c)        
override OBJS := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(CFILES))

all:
	@echo "Cleaning..."
	@make clean
	@echo "Building fuse..."
	@make fuse
	@echo "Running..."
	@make run

pull:
	@echo "Updating..."
	@git pull

upload:
	@echo "Cleaning"
	@make cleansetup
	@make clean
	@echo "Adding files to git"
	@git add .
	@git commit
	@git push origin main

compatest:
	@echo "Running base test..."
	@make test
	@sudo losetup -f $(IMGDIR)/$(DUMMY)
	@make compatest2

compatest2:
	@echo "Running compatibility test..."
	$(eval LOOP_DEV_PATH := $(shell losetup -a | grep $(DUMMY) | cut -d: -f1))
	@echo Loop device path: $(LOOP_DEV_PATH)
	@sudo mount $(LOOP_DEV_PATH) $(MOUNTPOINT)
	@-sudo ls -lsart $(MOUNTPOINT)
	@-sudo ls -lsart $(MOUNTPOINT)/$(COMPATDIR)
	@sudo umount $(MOUNTPOINT)
	@sudo losetup -d $(LOOP_DEV_PATH)

.PHONY: test
test:
	@make reset
	@make
	@make test2
test2:
	$(eval m1 := $(shell md5sum $(TESTDIR)/$(INFILE) | cut -d' ' -f1))
	$(eval m2 := $(shell md5sum $(TESTDIR)/$(OUTFILE) | cut -d' ' -f1))
	@echo "md5sum of $(TESTDIR)/$(INFILE) is $(m1)"
	@echo "md5sum of $(TESTDIR)/$(OUTFILE) is $(m2)"
	@if [ "$(m1)" = "$(m2)" ]; then \
		echo "Test passed!"; \
	else \
		echo "Test failed!"; \
	fi

reset:
	@rm -rf $(TESTDIR)
	@mkdir -p $(TESTDIR)
	@dd if=/dev/random of=$(TESTDIR)/$(INFILE) bs=$(TESTBLK) count=$(TESTCNT)
	@dd if=/dev/random bs=$(TESTBLK) count=$(TESTEMPTY) >> $(TESTDIR)/$(INFILE)
	@rm -f $(IMGDIR)/$(DUMMY)
	@cp $(IMGDIR)/$(RAW) $(IMGDIR)/$(DUMMY)
	@mkfs.$(FS) $(IMGDIR)/$(DUMMY) -b $(BLKSIZE)
	@sudo losetup -f $(IMGDIR)/$(DUMMY)
	@make reset2

reset2:
	$(eval LOOP_DEV_PATH := $(shell losetup -a | grep $(DUMMY) | cut -d: -f1))
	@echo Loop device path: $(LOOP_DEV_PATH)
	@sudo mount $(LOOP_DEV_PATH) $(MOUNTPOINT)
	@sudo cp $(TESTDIR)/$(INFILE) $(MOUNTPOINT)/$(INFILE)
	@sudo umount $(MOUNTPOINT)
	@sudo losetup -d $(LOOP_DEV_PATH)

setup:
	@mkdir -p $(BUILDDIR)
	@mkdir -p $(OBJDIR)
	@mkdir -p $(IMGDIR)
	@echo file $(BUILDDIR)/$(FUSE) > debug.gdb
	@echo set disassembly-flavor $(DISAS) >> debug.gdb
	@echo b $(ENTRY) >> debug.gdb
	@echo run >> debug.gdb
	
testsetup:
	@dd if=/dev/zero of=$(IMGDIR)/$(RAW) bs=$(SECTSIZE) count=$(SECTCOUNT)
	@sudo mkdir -p $(MOUNTPOINT)
	@make reset

.PHONY: debug
debug:
	@$(GDB) $(GDBFLAGS)

demo:
	@echo "Building demo file..."
	@echo "#include \"fused/bfuse.h\"" > $(SRCDIR)/AUTOGEN_test.c
	@echo "#include \"demofs/ext2.h\"" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "#include <stdio.h>" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "int main(int argc, char *argv[]) {" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "    register_drive(\"$(IMGDIR)/$(RAW)\", \"mountpoint1\", 512);" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "    register_drive(\"$(IMGDIR)/$(DUMMY)\", \"mountpoint2\", 512);" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "    if ($(FS)_search(\"mountpoint1\", 0)) {" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "        printf(\"Found $(FS) filesystem on mountpoint1\\\n\");" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "    }" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "    if ($(FS)_search(\"mountpoint2\", 0)) {" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "        printf(\"Found $(FS) filesystem on mountpoint2\\\n\");" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "    }" >> $(SRCDIR)/AUTOGEN_test.c
	@echo "}" >> $(SRCDIR)/AUTOGEN_test.c

cleantest:
	@rm -f $(SRCDIR)/AUTOGEN_*.c
	@rm -f $(IMGDIR)/$(RAW)
	@rm -f $(IMGDIR)/$(DUMMY)
	@sudo rm -rf $(MOUNTPOINT)

cleansetup:
	@rm -rf $(BUILDHOME)
	@rm -f $(GDBCFG)

fuse: $(OBJS) link

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling $<..."
	@ mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@

link:
	@echo "Linking..."
	@$(CC) $(CFLAGS) $(OBJS) -o $(BUILDDIR)/$(FUSE)
	@echo "Link complete"

.PHONY: clean
clean:
	@echo "Cleaning..."
	@rm -rf $(BUILDDIR)/*
	@rm -rf $(OBJDIR)/*

.PHONY: run
run:
	@echo "Running..."
	@$(BUILDDIR)/$(FUSE)