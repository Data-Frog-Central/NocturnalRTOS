input-y := input.o input-mt.o sf2000_gamepad.o

CFLAGS_input-mt.o += -Wno-sign-compare

obj-y += input.o
obj-$(CONFIG_RC_CORE) += rc/
obj-$(CONFIG_TP) += tp/
obj-y += ../saradc/
obj-y += sf2000_gamepad.c
