NPROC=$(shell nproc)
SHELL:=/bin/bash

CORE=snes9x2005
CONSOLE=snes9x2005

MIPS=/opt/mips32-mti-elf/2019.09-03-2/bin/mips-mti-elf-

CC = $(MIPS)gcc
CXX = $(MIPS)g++
LD = $(MIPS)ld
OBJCOPY = $(MIPS)objcopy

SF2000_CFLAGS := -EL -march=mips32 -mtune=mips32 -msoft-float
SF2000_CFLAGS += -O3 -G0 -fno-pic
SF2000_CFLAGS += -ffunction-sections -fdata-sections
SF2000_CFLAGS += -I ../libs/libretro-common/include

LDFLAGS := -EL -nostdlib -z max-page-size=32
LDFLAGS += --gc-sections
# needed for .text LMA = VMA
LDFLAGS += --build-id

SF2000_CXX_LDFLAGS := -EL -march=mips32 -mtune=mips32 -msoft-float
SF2000_CXX_LDFLAGS += -Wl,--gc-sections --static
SF2000_CXX_LDFLAGS += -z max-page-size=32

CORE_DIR := $(CORE)
include $(shell pwd)/$(CORE)/Makefile.common
CORE_OBJS := $(SOURCES_CXX:.cpp=.o)

# Default target
ifneq ($(CORE),)
all: $(CONSOLE).hcrtos install
else
all: install
endif

%.o: %.cpp
	$(CXX) $(SF2000_CFLAGS) -o $@ -c $<

%.o: %.c
	$(CC) $(SF2000_CFLAGS) -o $@ -c $<

%.o: %.s
	$(CC) $(SF2000_CFLAGS) -o $@ -c $<

libretro_core:
	@$(call echo_i,"compiling $(CORE)")
	$(MAKE) -j$(NPROC) -C "$(CORE)" $(MAKEFILE) platform=sf2000

libretro_core.a: libretro_core
	cp "$(CORE)"/*.a libretro_core.a

libretro-common:
	@$(call echo_i,"compiling $@")
	$(MAKE) -j$(NPROC) -C ../libs/libretro-common

libretro-common.a: libretro-common
	cp -u "../libs/libretro-common/$@" "$@"

core.elf: libretro_core.a libretro-common.a core_api.o frontend_functions.o
	@$(call echo_i,"compiling $@")
	$(CXX) -Wl,-Map=$@.map $(SF2000_CXX_LDFLAGS) -e __core_entry__ -Tcore.ld -o $@ \
		-Wl,--start-group core_api.o frontend_functions.o libretro_core.a libretro-common.a -lc -Wl,--end-group

$(CONSOLE).hcrtos: core.elf
	$(OBJCOPY) -O binary -R .MIPS.abiflags -R .note.gnu.build-id -R ".rel*" core.elf $(CONSOLE).hcrtos

install:
	@$(call echo_i,"install to sdcard")
	@$(call echo_d,"$(CORE)")

# Clean intermediate files and the final executable
clean:
	-rm -f core_api.o frontend_functions.o
	-rm -f core.elf core.elf.map $(CONSOLE).hcrtos
	-rm -f libretro_core.a libretro-common.a
	$(MAKE) -j$(NPROC) -C ../libs/libretro-common clean
	$(MAKE) -j$(NPROC) -C $(CORE) $(MAKEFILE) clean platform=sf2000

.PHONY: all clean

define echo_i
    echo -e "\033[1;33m$(1)\033[0m"
endef

define echo_e
    echo -e "\033[1;31m$(1)\033[0m"
endef

define echo_d
    echo -e "\033[1;37m$(1)\033[0m"
endef

define copy_if_updated
	diff -q $(1) $(2) || (rm -rf $$(dirname $(2)) && mkdir -p $$(dirname $(2)) && cp $(1) $(2) && echo "$(1) updated")
endef

