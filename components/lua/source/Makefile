include $(BR2_MKENTRY)
ifeq ($(skip-makefile),)
ROOT_DIR = lua

target-y += liblua.a
cflags-y += -DLUA_32BITS

LUA_OBJS = \
	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o \
	lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o \
	ltm.o lundump.o lvm.o lzio.o \
	lauxlib.o \
	lbaselib.o ldblib.o liolib.o lmathlib.o loslib.o ltablib.o lstrlib.o \
	lutf8lib.o lcorolib.o loadlib.o linit.o

obj-$(BR2_PACKAGE_LUA) += $(LUA_OBJS)

all: $(target-y)

%.o: $(srctree)/$(ROOT_DIR)/%.c
	$(Q)$(CC) $(cflags-y) $(CFLAGS) -c $< -o $@
	
liblua.a: $(LUA_OBJS)
	$(Q)$(AR) -rc $@ $^

install: FORCE
	install -d -m 0775 $(STAGING_DIR)/usr/include
	cp -fr $(srctree)/$(ROOT_DIR)/lua.h $(STAGING_DIR)/usr/include/
	cp -fr $(srctree)/$(ROOT_DIR)/luaconf.h $(STAGING_DIR)/usr/include/
	cp -fr $(srctree)/$(ROOT_DIR)/lauxlib.h $(STAGING_DIR)/usr/include/
	cp -fr $(srctree)/$(ROOT_DIR)/lualib.h $(STAGING_DIR)/usr/include/
	install -d -m 0775 $(STAGING_DIR)/usr/lib
	install -m 0664 $(target-y) $(STAGING_DIR)/usr/lib/

clean: FORCE
	$(Q)$(MAKE) $(clean)=.
	-rm -f liblua.a

.PHONY: FORCE

FORCE:

endif
