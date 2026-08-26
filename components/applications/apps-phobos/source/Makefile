#
# Makefile
#
CC ?= gcc
LVGL_DIR_NAME ?= lvgl
ORIGINAL_MAKEFILE_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
LVGL_DIR ?= $(ORIGINAL_MAKEFILE_DIR)

SYS_DIR = -I$(LVGL_DIR) \
		-I$(STAGING_DIR)/usr/include \
		-I$(STAGING_DIR)/usr/include/hcuapi

LVGL_HEADERS = -I$(STAGING_DIR)/usr/include/lvgl/ \
		-I$(STAGING_DIR)/usr/include/lvgl/lvgl \
		-I$(LVGL_DIR)/libs/libretro-common/include \
		-I$(LVGL_DIR)/libs/zip/src \
		-I$(LVGL_DIR)/libs/cores/include 

MY_DEF =

SCREEN_APP_SUPPORT := $(shell echo $(PHOBOSAPP_DEF) | grep -o SCREEN_APP_SUPPORT)

CFLAGS ?= -O3 $(SYS_DIR) -Wall -Wshadow -Wundef -Wmissing-prototypes -Wno-discarded-qualifiers -Wall -Wextra -Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith -fno-strict-aliasing -Wno-error=cpp -Wuninitialized -Wmaybe-uninitialized -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated -Wempty-body -Wtype-limits -Wshift-negative-value -Wstack-usage=2048 -Wno-unused-value -Wno-unused-parameter -Wno-missing-field-initializers -Wuninitialized -Wmaybe-uninitialized -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wpointer-arith -Wno-cast-qual -Wmissing-prototypes -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wno-discarded-qualifiers -Wformat-security -Wno-ignored-qualifiers -Wno-sign-compare
CFLAGS += $(LVGL_HEADERS)
LDFLAGS ?= -lm -llvgl

DEMO_LIBS = -lffplayer -lpthread -lge -Wl,--start-group -lstdc++ $(CAST_LIBS) -Wl,--end-group

include $(LVGL_DIR)/drivers/sf2000_drivers.mk
include $(LVGL_DIR)/phobos/phobos.mk
include $(LVGL_DIR)/libs/libretro-common/Makefile.hcrtos
PHOBOS_SRC += $(LVGL_DIR)/libs/zip/src/zip.c
DEMO_NAME = phobos_demo

OBJEXT ?= .o

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))
PHOBOS_OBJS = $(PHOBOS_SRC:.c=$(OBJEXT))
PHOBOS_OBJS += $(PHOBOS_CPP_SRC:.cpp=$(OBJEXT))

all_depends := $(patsubst %.c,%.d,$(CSRCS) $(PHOBOS_SRC))
all_depends +=$(patsubst %.cpp,%.d,$(PHOBOS_CPP_SRC))
all_depends += $(patsubst %.S,%.d,$(ASRCS))

all: phobos_app

%.o: %.c
	$(CC) $(CFLAGS) $(MY_DEF) -c $< -o $@
	echo "CC $<"
	
.PHONY: FORCE

%.o: %.cpp
	$(CC) $(CFLAGS) $(MY_DEF) -c $< -o $@
	echo "CC $<"

%.d: %.c
	set -e;rm -rf $@;$(CC) $(CFLAGS) $(MY_DEF) -MM $^ > $@.$$$$;cp $@.$$$$ $@;rm -rf $@.$$$$

%.d: %.cpp
	set -e;rm -rf $@;$(CC) $(CFLAGS) $(MY_DEF) -MM $^ > $@.$$$$;cp $@.$$$$ $@;rm -rf $@.$$$$

-include $(all_depends)

phobos_app:	$(COBJS) $(PHOBOS_OBJS)
	@$(CC) -o $(DEMO_NAME) $(PHOBOS_OBJS) $(COBJS) $(LDFLAGS) $(DEMO_LIBS)
	
clean: 
	rm -f $(AOBJS) $(COBJS) $(PHOBOS_OBJS)
	@find ./ -type f -name "*.o" |xargs rm -rf
	rm -rf $(DEMO_NAME)

install:
	@echo "install ......"
