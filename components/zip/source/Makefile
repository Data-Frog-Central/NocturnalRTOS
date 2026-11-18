include $(BR2_MKENTRY)
ifeq ($(skip-makefile),)
ROOT_DIR = zip
SRC_DIR = $(ROOT_DIR)/src

target-y += libzip.a
cflags-y += -DZIP_HAVE_SYMLINK=0 -DMINIZ_NO_TIME=1 -DZIP_ENABLE_DEFLATE=1 -Dlstat=stat -include unistd.h -I$(srctree)/$(SRC_DIR)

ZIP_OBJS = \
	zip.o

obj-$(BR2_PACKAGE_ZIP) += $(ZIP_OBJS)

all: $(target-y)

%.o: $(srctree)/$(SRC_DIR)/%.c
	$(Q)$(CC) $(cflags-y) $(CFLAGS) -c $< -o $@
	
libzip.a: $(ZIP_OBJS)
	$(Q)$(AR) -rc $@ $^

install: FORCE
	install -d -m 0775 $(STAGING_DIR)/usr/include/helix
	cp -fr $(srctree)/$(SRC_DIR)/*.h $(STAGING_DIR)/usr/include
	install -d -m 0775 $(STAGING_DIR)/usr/lib
	install -m 0664 $(target-y) $(STAGING_DIR)/usr/lib/

clean: FORCE
	$(Q)$(MAKE) $(clean)=.
	-rm -f libzip.a

.PHONY: FORCE

FORCE:

endif
