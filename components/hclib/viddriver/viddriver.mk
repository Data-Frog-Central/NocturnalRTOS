################################################################################
#
# video driver
#
################################################################################
VIDDRIVER_VERSION = 8a3c29228074e10086912c71841c9b56b94c8d2e
VIDDRIVER_SITE_METHOD = git
VIDDRIVER_SITE = ssh://git@hichiptech.gitlab.com:33888/hclib/libvid.git
VIDDRIVER_DEPENDENCIES = kernel

VIDDRIVER_MAKE_FLAGS += \
		CROSS_COMPILE=$(TARGET_CROSS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="${TARGET_LDFLAGS}"

VIDDRIVER_INSTALL_STAGING = YES

VIDDRIVER_EXTRACT_CMDS = \
	git -C $(@D) init && \
	git -C $(@D) remote add origin $(VIDDRIVER_SITE) && \
	git -C $(@D) fetch && \
	git -C $(@D) checkout master && \
	git -C $(@D) checkout $(VIDDRIVER_VERSION)


VIDDRIVER_CLEAN_CMDS = $(TARGET_MAKE_ENV) $(VIDDRIVER_MAKE_ENV) $(MAKE) $(VIDDRIVER_MAKE_FLAGS) -C $(@D) clean
VIDDRIVER_BUILD_CMDS = $(TARGET_MAKE_ENV) $(VIDDRIVER_MAKE_ENV) $(MAKE) $(VIDDRIVER_MAKE_FLAGS) -C $(@D) all
VIDDRIVER_INSTALL_STAGING_CMDS = $(TARGET_MAKE_ENV) $(VIDDRIVER_MAKE_ENV) $(MAKE) $(VIDDRIVER_MAKE_FLAGS) -C $(@D) install

$(eval $(generic-package))
