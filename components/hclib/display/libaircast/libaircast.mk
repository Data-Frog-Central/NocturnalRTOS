LIBAIRCAST_VERSION = 112fa16358fee01ca2ab4cc0e19143628d586f31
LIBAIRCAST_SITE_METHOD = git
LIBAIRCAST_SITE = ssh://git@hichiptech.gitlab.com:33888/hclib/libaircast.git
LIBAIRCAST_DEPENDENCIES = kernel newlib pthread

LIBAIRCAST_CONF_OPTS += -DHCRTOS=ON

LIBAIRCAST_EXTRACT_CMDS = \
	git -C $(@D) init && \
	git -C $(@D) remote add origin $(LIBAIRCAST_SITE) && \
	git -C $(@D) fetch && \
	git -C $(@D) checkout master && \
	git -C $(@D) checkout $(LIBAIRCAST_VERSION)

define LIBAIRCAST_POST_BUILD
	$(TARGET_CROSS)strip -g -S -d $(@D)/libaircast/libaircast.a
endef

LIBAIRCAST_POST_BUILD_HOOKS += LIBAIRCAST_POST_BUILD

LIBAIRCAST_INSTALL_STAGING = YES

$(eval $(cmake-package))
