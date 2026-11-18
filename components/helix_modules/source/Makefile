include $(BR2_MKENTRY)
ifeq ($(skip-makefile),)
ROOT_DIR = helix_modules
SRC_DIR = $(ROOT_DIR)/src
INC_DIR = $(ROOT_DIR)/include

target-y += libhelix_modules.a
cflags-y += -DNOCTURNALRTOS -I$(srctree)/$(INC_DIR)

HELIX_MODULES_OBJS = \
	helix_fake_rtc.o helix_images.o helix_logging.o helix_saves.o

obj-$(BR2_PACKAGE_HELIX_MODULES) += $(HELIX_MODULES_OBJS)

all: $(target-y)

%.o: $(srctree)/$(SRC_DIR)/%.c
	$(Q)$(CC) $(cflags-y) $(CFLAGS) -c $< -o $@
	
libhelix_modules.a: $(HELIX_MODULES_OBJS)
	$(Q)$(AR) -rc $@ $^

install: FORCE
	install -d -m 0775 $(STAGING_DIR)/usr/include/helix
	cp -fr $(srctree)/$(INC_DIR)/helix/*.h $(STAGING_DIR)/usr/include/helix
	install -d -m 0775 $(STAGING_DIR)/usr/lib
	install -m 0664 $(target-y) $(STAGING_DIR)/usr/lib/

clean: FORCE
	$(Q)$(MAKE) $(clean)=.
	-rm -f libhelix_modules.a

.PHONY: FORCE

FORCE:

endif
