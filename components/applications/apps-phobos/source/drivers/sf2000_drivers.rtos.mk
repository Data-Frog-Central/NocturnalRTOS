CSRCS += $(shell find -L $(PROJECTOR_DIR)/drivers/ -name "*.c")
CFLAGS += -I$(PROJECTOR_DIR)/drivers

