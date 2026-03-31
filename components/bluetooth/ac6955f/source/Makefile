include $(BR2_MKENTRY)
ifeq ($(skip-makefile),)

TARGETS := libac6955f.a

all : $(TARGETS) FORCE

ac6955f-$(BR2_PACKAGE_AC6955F_RT) := ac6955f.o
ac6955f-$(BR2_PACKAGE_AC6955F_GX) := ac6955fgx.o
ac6955f-$(BR2_PACKAGE_AC6956C_GX) := ac6956cgx.o

obj-y = ac6955f.o

$(TARGETS): built-in.o
	rm -f $@
	$(Q)$(AR) -rc $@ $^

built-in.o: FORCE
	$(Q)$(MAKE) $(build)=.

install: FORCE
	install -D -m 0664 $(TARGETS) $(STAGING_DIR)/usr/lib/$(TARGETS)
	cp -fr $(srctree)/bluetooth.h $(srctree)/bluetooth_io.h $(STAGING_DIR)/usr/include/

clean: FORCE
	$(Q)$(MAKE) $(clean)=.
	rm -f $(TARGETS)

.PHONY: FORCE

FORCE:

endif	# skip-makefile
