include $(BR2_MKENTRY)
ifeq ($(skip-makefile),)
ROOT_DIR = nocturnal_drivers
SRC_DIR = $(ROOT_DIR)/src
INC_DIR = $(ROOT_DIR)/include

target-y += libnocturnal_drivers.a
cflags-y += -I$(srctree)/$(INC_DIR) -D__HCRTOS__=1 -I$(STAGING_DIR)/usr/include_fix

NOCTURNAL_DRIVERS_OBJS = \
	nocturnal_joypad.o

obj-$(BR2_PACKAGE_NOCTURNAL_DRIVERS) += $(NOCTURNAL_DRIVERS_OBJS)

all: $(target-y)

%.o: $(srctree)/$(SRC_DIR)/%.c
	$(Q)$(CC) $(cflags-y) $(CFLAGS) -c $< -o $@
	
libnocturnal_drivers.a: $(NOCTURNAL_DRIVERS_OBJS)
	$(Q)$(AR) -rc $@ $^

install: FORCE
	install -d -m 0775 $(STAGING_DIR)/usr/include
	cp -fr $(srctree)/$(INC_DIR)/*.h $(STAGING_DIR)/usr/include
	install -d -m 0775 $(STAGING_DIR)/usr/lib
	install -m 0664 $(target-y) $(STAGING_DIR)/usr/lib/

clean: FORCE
	$(Q)$(MAKE) $(clean)=.
	-rm -f libnocturnal_drivers.a

.PHONY: FORCE

FORCE:

endif
