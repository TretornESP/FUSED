override FUSE := fuse

CC := /usr/bin/cc
LD := /usr/bin/ld
GDB := gdb

CFLAGS ?= -O2 -g -Wall -Wextra -Wpedantic -pipe -std=c11

ABSDIR := $(shell pwd)
SRCDIR := $(ABSDIR)/src
BUILDHOME := $(ABSDIR)/build
BUILDDIR := $(BUILDHOME)/bin
OBJDIR := $(BUILDHOME)/lib
IMGDIR := $(BUILDHOME)/img

RAW := raw.img
DUMMY := dummy.img

FS := ext2#Warning, this must be mkfs compatible

SECTSIZE = 512
SECTCOUNT = 204800

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
	@echo "Building fuse..."
	@make fuse
	@echo "Running..."
	@make run

setup:
	@make cleansetup
	@mkdir -p $(BUILDDIR)
	@mkdir -p $(OBJDIR)
	@mkdir -p $(IMGDIR)

	@dd if=/dev/zero of=$(IMGDIR)/$(RAW) bs=$(SECTSIZE) count=$(SECTCOUNT)
	@cp $(IMGDIR)/$(RAW) $(IMGDIR)/$(DUMMY)
	@mkfs.$(FS) $(IMGDIR)/$(DUMMY)
	@make autogen

autogen:
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

cleanautogen:
	@rm -f $(SRCDIR)/AUTOGEN_*.c

cleansetup:
	@rm -rf $(BUILDHOME)

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
	@rm -rf $(BUILDHOME)
	@rm -f $(SRCDIR)/AUTOGEN_*

.PHONY: run
run:
	@echo "Running..."
	@$(BUILDDIR)/$(FUSE)