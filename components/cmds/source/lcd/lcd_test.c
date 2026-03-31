#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <kernel/lib/console.h>
#include <string.h>
#include <hcuapi/lvds.h>
#include <hcuapi/gpio.h>
#include <kernel/delay.h>
#include <getopt.h>
#include <dt-bindings/gpio/gpio.h>
#include "lcd.h"

#define LCDDEV_PATH  "/dev/lcddev"

static int lcd_test_enter(int argc, char *argv[])
{
	return 0;
}

static int lcd_init_all_cb(int argc, char *argv[])
{
	int fd = 0;
	fd = open(LCDDEV_PATH, O_RDWR);
	if (fd < 0) {
		printf("open %s failed, ret=%d\n", LCDDEV_PATH, fd);
		return -1;
	}
	ioctl(fd, LCD_INIT_ALL, NULL);
	close(fd);
	return 0;
}

static int lcd_init_driver_cb(int argc, char *argv[])
{
	int fd = 0;
	fd = open(LCDDEV_PATH, O_RDWR);
	if (fd < 0) {
		printf("open %s failed, ret=%d\n", LCDDEV_PATH, fd);
		return -1;
	}
	ioctl(fd, LCD_DRIVER_INIT, NULL);
	close(fd);
	return 0;
}

static int lcd_rotate_cb(int argc, char *argv[])
{
	int fd = 0;
	int val = 0;
	fd = open(LCDDEV_PATH, O_RDWR);
	if (fd < 0) {
		printf("open %s failed, ret=%d\n", LCDDEV_PATH, fd);
		return -1;
	}

	if (val > 1) {
		val = atoi(argv[1]);
		ioctl(fd, LCD_SET_ROTATE, val);
		printf("rotate val = %d\n", val);
		close(fd);
	}
	return 0;
}

static int lcd_cmds_data_cb(int argc, char *argv[])
{
	int fd = 0;
	int val = 0, i = 1;
	unsigned char lcd_data_buf[5] = { 0 };
	char *ptr;
	unsigned long arg = 0;
	struct hc_lcd_master_dev cmds;
	fd = open(LCDDEV_PATH, O_RDWR);
	int count = 0;
	if (argc == 2) {
		/*
			lcddev send cmds
		*/
		cmds.count = 1;
		memcpy(lcd_data_buf, argv[i], 4);
		cmds.packet[i - 1] = (unsigned short)strtol(lcd_data_buf, &ptr, 16);
		printf("packet = %d cmds.count = %d \n", cmds.packet[0], cmds.count);
		arg = (unsigned long)&cmds;
		ioctl(fd, LCD_SEND_CMDS, arg);
		close(fd);
	} else if (argc > 2) {
		/*
			lcddev send cmds
		*/
		cmds.count = 1;
		memcpy(lcd_data_buf, argv[i], 4);
		cmds.packet[0] = (unsigned short)strtol(lcd_data_buf, &ptr, 16);
		printf("packet = %d cmds.count = %d \n", cmds.packet[0],cmds.count);
		arg = (unsigned long)&cmds;
		ioctl(fd, LCD_SEND_CMDS, arg);

		/*
			lcddev send data
		*/
		count = cmds.count = argc - 2;
		memset(cmds.packet, 0, count);
		printf("lcd cmds:");
		while (count-- > 0) {
			memcpy(lcd_data_buf, argv[i + 1], 4);
			cmds.packet[i] = (unsigned short)strtol(lcd_data_buf, &ptr, 16);
			printf("0x%04x ", cmds.packet[i]);
			i++;
		}
		printf("cmds.count=%d \n", cmds.count);
		arg = (unsigned long)&cmds;
		ioctl(fd, LCD_SEND_DATE, arg);
		close(fd);
	}
}

static int lcd_send_data_cb(int argc, char *argv[])
{
	int fd = 0;
	int val = 0, i = 1;
	unsigned char lcd_data_buf[5] = { 0 };
	char *ptr;
	unsigned long arg = 0;
	struct hc_lcd_master_dev cmds;
	fd = open(LCDDEV_PATH, O_RDWR);
	int count = 0;
	if (argc > 1) {
		count = cmds.count = argc - 1;
		memset(cmds.packet, 0, sizeof(cmds.packet));
		printf("lcd cmds:");
		while (count-- > 0) {
			memcpy(lcd_data_buf, argv[i], 4);
			cmds.packet[i - 1] =
				(unsigned short)strtol(lcd_data_buf, &ptr, 16);
			printf("0x%04x ", cmds.packet[i - 1]);
			i++;
		}
		printf("cmds.count=%d \n", cmds.count);
		arg = (unsigned long)&cmds;
		ioctl(fd, LCD_SEND_DATE, arg);
		close(fd);
	}
}

static int lcd_onoff_cb(int argc, char *argv[])
{
	int fd = 0;
	int val = 0;
	fd = open(LCDDEV_PATH, O_RDWR);
	if (fd < 0) {
		printf("open %s failed, ret=%d\n", LCDDEV_PATH, fd);
		return -1;
	}

	if (val > 1) {
		val = atoi(argv[1]);
		ioctl(fd, LCD_SET_ONOFF, val);
		printf("%s %d val = %d\n", __func__, __LINE__, val);
		close(fd);
	}
	return 0;
}

static int lcd_pwm_vcom_cb(int argc, char *argv[])
{
	int fd = 0;
	int val = 0;
	fd = open(LCDDEV_PATH, O_RDWR);
	if (fd < 0) {
		printf("open %s failed, ret=%d\n", LCDDEV_PATH, fd);
		return -1;
	}

	if (val > 1) {
		val = atoi(argv[1]);
		ioctl(fd, LCD_SET_ONOFF, val);
		printf("%s %d val = %d\n", __func__, __LINE__, val);
		close(fd);
	}

	return 0;
}

static int lcd_gpio_power_cb(int argc, char *argv[])
{
	int fd = 0;
	int val = 0;
	fd = open(LCDDEV_PATH, O_RDWR);
	if (fd < 0) {
		printf("open %s failed, ret=%d\n", LCDDEV_PATH, fd);
		return -1;
	}

	if (val > 1) {
		val = atoi(argv[1]);
		ioctl(fd, LCD_SET_GPIO_POWER, val);
		printf("%s %d val = %d\n", __func__, __LINE__, val);
		close(fd);
	}

	return 0;
}

static int lcd_reset_cb(int argc, char *argv[])
{
	int fd = 0;
	int val = 0;
	fd = open(LCDDEV_PATH, O_RDWR);
	if (fd < 0) {
		printf("open %s failed, ret=%d\n", LCDDEV_PATH, fd);
		return -1;
	}

	if (val > 1) {
		val = atoi(argv[1]);
		ioctl(fd, LCD_SET_GPIO_RESET, val);
		printf("%s %d val = %d\n", __func__, __LINE__, val);
		close(fd);
	}

	return 0;
}

CONSOLE_CMD(lcd_test, NULL, lcd_test_enter, CONSOLE_CMD_MODE_SELF, "enter lcddev test")
CONSOLE_CMD(init, "lcd_test", lcd_init_all_cb, CONSOLE_CMD_MODE_SELF, "lcddev init all, eg: init")
CONSOLE_CMD(init_drv, "lcd_test", lcd_init_driver_cb, CONSOLE_CMD_MODE_SELF, "lcddev driver init, eg: init_drv")
CONSOLE_CMD(rotate, "lcd_test", lcd_rotate_cb, CONSOLE_CMD_MODE_SELF, "lcddev set rotate, eg: rotate 0")
CONSOLE_CMD(cmds_data, "lcd_test", lcd_cmds_data_cb, CONSOLE_CMD_MODE_SELF, "lcddev send cmds and data, eg: cmds_data 0x2000 0x0000")
CONSOLE_CMD(send_data, "lcd_test", lcd_send_data_cb, CONSOLE_CMD_MODE_SELF, "lcddev send data, eg: send_data 0x1000")
CONSOLE_CMD(onoff, "lcd_test", lcd_onoff_cb, CONSOLE_CMD_MODE_SELF, "lcd onoff, eg: onoff 0")
CONSOLE_CMD(vcom, "lcd_test", lcd_pwm_vcom_cb, CONSOLE_CMD_MODE_SELF, "lcd pwm vcom, eg: vcom 100")
CONSOLE_CMD(power, "lcd_test", lcd_gpio_power_cb, CONSOLE_CMD_MODE_SELF, "lcd set gpio power, eg: power 1")
CONSOLE_CMD(reset, "lcd_test", lcd_reset_cb, CONSOLE_CMD_MODE_SELF, "lcd set gpio power, eg: reset 1")
