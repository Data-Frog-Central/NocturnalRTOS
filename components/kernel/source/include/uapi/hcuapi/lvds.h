#ifndef __UAPI_LVDS_H_H
#define __UAPI_LVDS_H_H

#include <hcuapi/iocbase.h>
#include <hcuapi/pinpad.h>

#define LVDS_SET_CHANNEL_MODE		_IO (LVDS_IOCBASE, 1)
#define LVDS_SET_MAP_MODE		_IO (LVDS_IOCBASE, 2)
#define LVDS_SET_CH0_SRC_SEL		_IO (LVDS_IOCBASE, 3)
#define LVDS_SET_CH1_SRC_SEL		_IO (LVDS_IOCBASE, 4)
#define LVDS_SET_CH0_INVERT_CLK_SEL	_IO (LVDS_IOCBASE, 5)
#define LVDS_SET_CH1_INVERT_CLK_SEL	_IO (LVDS_IOCBASE, 6)
#define LVDS_SET_CH0_CLK_GATE		_IO (LVDS_IOCBASE, 7)
#define LVDS_SET_CH1_CLK_GATE		_IO (LVDS_IOCBASE, 8)
#define LVDS_SET_HSYNC_POLARITY		_IO (LVDS_IOCBASE, 9)
#define LVDS_SET_VSYNC_POLARITY		_IO (LVDS_IOCBASE, 10)
#define LVDS_SET_EVEN_ODD_ADJUST_MODE	_IO (LVDS_IOCBASE, 11)
#define LVDS_SET_EVEN_ODD_INIT_VALUE	_IO (LVDS_IOCBASE, 12)
#define LVDS_SET_SRC_SEL		_IO (LVDS_IOCBASE, 13)
#define LVDS_SET_RESET			_IO (LVDS_IOCBASE, 14)
#define LVDS_SET_POWER_TRIGGER		_IO (LVDS_IOCBASE, 15)
#define LVDS_SET_GPIO_OUT		_IOW (LVDS_IOCBASE, 16, struct lvds_set_gpio)
#define LVDS_SET_GPIO_BACKLIGHT		_IO (LVDS_IOCBASE, 17)		//!< param: value: 0 1
#define LVDS_SET_PWM_BACKLIGHT		_IO (LVDS_IOCBASE, 18)		//!< param: Duty cycle: 0~100
#define LVDS_SET_PWM_VCOM		_IO (LVDS_IOCBASE, 19)		//!< param: Duty cycle: 0~100
#define LVDS_SET_GPIO_POWER		_IO (LVDS_IOCBASE, 20)		//!< param: value: 0 1
#define LVDS_SET_TRIGGER_EN		_IO (LVDS_IOCBASE, 21)
#define LVDS_GET_CHANNEL_MODE		_IOR (LVDS_IOCBASE, 22, enum LVDS_CHANNEL_MODE)	//get lvds channel mode

struct lvds_set_gpio {
	unsigned int padctl;
	unsigned char value;
};

typedef enum LVDS_CHANNEL_MODE {
	LVDS_CHANNEL_MODE_SINGLE_IN_SINGLE_OUT,
	LVDS_CHANNEL_MODE_SINGLE_IN_DUAL_OUT,
} lvds_channel_mode_e;

#endif
