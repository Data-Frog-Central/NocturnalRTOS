LIBDLNA_VERSION = b0f4f65abb416353c4e53faa94eff7c9216602fb
LIBDLNA_SITE_METHOD = git
LIBDLNA_SITE = ssh://git@hichiptech.gitlab.com:33888/hclib/libdlna.git
LIBDLNA_DEPENDENCIES = kernel newlib pthread

LIBDLNA_CONF_OPTS += -DBUILD_LIBRARY_TYPE="STATIC" -DBUILD_OS_TARGET="HCRTOS"

LIBDLNA_EXTRACT_CMDS = \
	git -C $(@D) init && \
	git -C $(@D) remote add origin $(LIBDLNA_SITE) && \
	git -C $(@D) fetch && \
	git -C $(@D) checkout master && \
	git -C $(@D) checkout $(LIBDLNA_VERSION) && \
	cd $(@D) && sh ./gen-version.sh

define LIBDLNA_POST_BUILD
	$(TARGET_CROSS)strip -g -S -d $(@D)/libdlna.a
endef

LIBDLNA_POST_BUILD_HOOKS += LIBDLNA_POST_BUILD

define LIBDLNA_INSTALL_PREBUILT
	$(INSTALL) -D -m 0664 $(@D)/include/hccast/dlna_api.h $(PREBUILT_DIR)/usr/include/dlna/dlna_api.h
	$(INSTALL) -D -m 0664 $(@D)/libdlna.a $(PREBUILT_DIR)/usr/lib/libdlna.a
endef

LIBDLNA_POST_INSTALL_TARGET_HOOKS += LIBDLNA_INSTALL_PREBUILT

LIBDLNA_INSTALL_STAGING = YES

$(eval $(cmake-package))
