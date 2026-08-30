include $(BR2_MKENTRY)
ifeq ($(skip-makefile),)

ROOT_DIR = libretro-common

cflags-y += -isystem $(STAGING_DIR)/usr/include -I$(STAGING_DIR)/usr/include_fix -I$(STAGING_DIR)/../include -I$(STAGING_DIR)/usr/include/hcuapi
cflags-y += -I$(srctree) -I$(srctree)/$(ROOT_DIR)
cflags-y += -EL -march=mips32 -mtune=mips32 -msoft-float
cflags-y += -ffast-math -fomit-frame-pointer
cflags-y += -G0 -mno-abicalls -fno-pic
cflags-y += -ffunction-sections -fdata-sections
cflags-y += -O2 -DNDEBUG -std=gnu99
cflags-y += -I$(srctree)/$(ROOT_DIR)/include
cflags-y += -I$(srctree)/$(ROOT_DIR)/../../include
cflags-y += -D_7ZIP_ST -DNOCTURNAL

target-y += libretro-common.a

SOURCES_C += \
    $(ROOT_DIR)/audio/conversion/float_to_s16.c \
    $(ROOT_DIR)/audio/conversion/s16_to_float.c \
    $(ROOT_DIR)/audio/resampler/audio_resampler.c \
    $(ROOT_DIR)/audio/resampler/drivers/sinc_resampler.c \
    $(ROOT_DIR)/compat/fopen_utf8.c \
    $(ROOT_DIR)/compat/compat_posix_string.c \
    $(ROOT_DIR)/compat/compat_strl.c \
    $(ROOT_DIR)/compat/compat_snprintf.c \
    $(ROOT_DIR)/compat/compat_strcasestr.c \
    $(ROOT_DIR)/encodings/encoding_crc32.c \
    $(ROOT_DIR)/encodings/encoding_utf.c \
    $(ROOT_DIR)/features/features_cpu.c  \
    $(ROOT_DIR)/file/config_file.c \
	$(ROOT_DIR)/file/config_file_io.c \
    $(ROOT_DIR)/file/config_file_userdata.c \
    $(ROOT_DIR)/file/file_path.c \
    $(ROOT_DIR)/file/file_path_io.c \
    $(ROOT_DIR)/file/retro_dirent.c \
    $(ROOT_DIR)/formats/png/rpng.o \
    $(ROOT_DIR)/libco/libco.c \
    $(ROOT_DIR)/lists/dir_list.c \
    $(ROOT_DIR)/lists/string_list.c \
    $(ROOT_DIR)/memmap/memalign.c \
    $(ROOT_DIR)/streams/chd_stream.c \
    $(ROOT_DIR)/streams/file_stream.c \
    $(ROOT_DIR)/streams/file_stream_transforms.c \
    $(ROOT_DIR)/streams/interface_stream.c \
    $(ROOT_DIR)/streams/memory_stream.c \
    $(ROOT_DIR)/streams/trans_stream.o \
    $(ROOT_DIR)/streams/trans_stream_pipe.o \
    $(ROOT_DIR)/streams/trans_stream_zlib.o \
    $(ROOT_DIR)/string/stdstring.c \
    $(ROOT_DIR)/time/rtime.c \
    $(ROOT_DIR)/utils/md5.c \
    $(ROOT_DIR)/vfs/vfs_implementation.c \

OBJECTS := $(patsubst %.c,%.o,$(filter %.c,$(SOURCES_C))) $(filter %.o,$(SOURCES_C))
obj-$(BR2_PACKAGE_LIBRETRO_COMMON) += $(OBJECTS)

all: $(target-y)

$(ROOT_DIR)/%.o: $(srctree)/$(ROOT_DIR)/%.c
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(cflags-y) -c $< -o $@

$(target-y): $(OBJECTS)
	$(Q)$(AR) -rc $@ $^

install: FORCE
	install -d -m 0775 $(STAGING_DIR)/usr/lib
	install -m 0664 $(target-y) $(STAGING_DIR)/usr/lib/
	install -d -m 0775 $(STAGING_DIR)/usr/include
	cp -r $(srctree)/$(ROOT_DIR)/include/* $(STAGING_DIR)/usr/include/

clean: FORCE
	$(Q)$(MAKE) $(clean)=.
	-rm -f $(target-y) $(OBJECTS)

.PHONY: FORCE

FORCE:

endif