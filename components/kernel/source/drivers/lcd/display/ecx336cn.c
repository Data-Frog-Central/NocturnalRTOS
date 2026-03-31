#define LOG_TAG "lcd_ecx336cn"
#define ELOG_OUTPUT_LVL ELOG_LVL_ERROR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <kernel/io.h>
#include <kernel/types.h>
#include <kernel/vfs.h>
#include <kernel/lib/console.h>
#include <kernel/elog.h>
#include <kernel/lib/fdt_api.h>
#include <kernel/module.h>
#include <hcuapi/gpio.h>
#include <hcuapi/pinpad.h>
#include <hcuapi/pinmux.h>
#include <kernel/io.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <hcuapi/spidev.h>
#include "../lcd_main.h"
#include <kernel/delay.h>
/*
*	TIME: 2022 11 12
*	support: HQ 640 * 400 rgb
*	PINPAD_R08 clk
*	PINPAD_T01 mosi
*	PINPAD_R00 cs

	lcd-ecx336cn{
		spi-gpio-sck	= <PINPAD_R08>;
		spi-gpio-mosi	= <PINPAD_T01>;
		spi-gpio-cs		= <PINPAD_R00>;
		reset			= <PINPAD_T07>;
		default-off;
		status = "okay";
	};

	lcd{
		lcd-map-name = "lcd-ecx336cn";
		default-off;
		status = "okay";
	};

	or

	spi-gpio {
		gpio-sck = <PINPAD_R08>;
		gpio-mosi = <PINPAD_T01>;
		// gpio-miso = <PINPAD_MAX>;
		num-chipselects = <1>;
		cs-gpios = <PINPAD_R00>;
		status = "okay";

		spidev@2 {
			devpath = "/dev/spidev2";
			reg = <0>;
			spi-max-frequency = <300000>;
			status = "okay";
		};
	};

	lcd-ecx336cn{
		devpath = "/dev/spidev2";
		reset			= <PINPAD_T07>;
		default-off;
		status = "okay";
	};

	lcd{
		lcd-map-name = "lcd-ecx336cn";
		default-off;
		status = "okay";
	};

*/

typedef struct ecx336cn_dev{
	u32 spi_clk_num;
	u32 spi_clk_vaild_edge;
	u32 spi_cs_num;
	u32 spi_cs_polar;  
	u32 spi_mosi_num;
	u32 spi_miso_num;
	u32 spi_is_9bit;
	u32 spi_mode;
	u32 lcd_reset_num;
	u32 lcd_reset_polar;
	u32 lcd_stbyb_num;
	u32 lcd_stbyb_polar;
	u32 cur_type;
	int spi_fd;
}ecx336cn_dev_t;

static ecx336cn_dev_t ecx336cndev;

static void gpio_spi_set_mosi(unsigned char data)
{
    lcd_gpio_set_output(ecx336cndev.spi_mosi_num,(bool)data);
}

static void gpio_spi_generate_clk(void)
{   
    if(ecx336cndev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(ecx336cndev.spi_clk_num,1);
	else
		lcd_gpio_set_output(ecx336cndev.spi_clk_num,0);
	usleep(2);
	if(ecx336cndev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(ecx336cndev.spi_clk_num,0);
	else
		lcd_gpio_set_output(ecx336cndev.spi_clk_num,1);
}
static void gpio_spi_enable_cs(void)//==0
{
	if(ecx336cndev.spi_cs_polar == 0)	
		lcd_gpio_set_output(ecx336cndev.spi_cs_num,0);
	else
		lcd_gpio_set_output(ecx336cndev.spi_cs_num,1);
	// printf("%s %d val = %08lx\n",__FUNCTION__,__LINE__,GET_DWORD(0xb8800054));
}

static void gpio_spi_disable_cs(void)
{
	if(ecx336cndev.spi_cs_polar == 0)
		lcd_gpio_set_output(ecx336cndev.spi_cs_num,1);//cs
	else
		lcd_gpio_set_output(ecx336cndev.spi_cs_num,0);//cs
	// printf("%s %d val = %08lx\n",__FUNCTION__,__LINE__,GET_DWORD(0xb8800054));
}

static void gpio_spi_init_clk(void)
{
	if(ecx336cndev.spi_clk_vaild_edge == 1)//1 //==1
		lcd_gpio_set_output(ecx336cndev.spi_clk_num,0);//sck
	else
		lcd_gpio_set_output(ecx336cndev.spi_clk_num,1);//sck
}

static void lcd_gpio_spi_config_write(unsigned char bit_9,unsigned char cmd)
{
	int i=0;
	unsigned char cmd_val = 0;
	gpio_spi_disable_cs();//cs
	usleep(10);
	gpio_spi_init_clk();//sck
	gpio_spi_enable_cs();
	usleep(2);
	if(ecx336cndev.spi_is_9bit == 1)
	{
		gpio_spi_set_mosi(bit_9);//sda dat=0
		usleep(3);
		gpio_spi_generate_clk();
	}
	for(i=8;i>0;i--){
		cmd_val = (cmd>>(i-1))&0x1;
		gpio_spi_set_mosi(cmd_val);
		usleep(2);
		gpio_spi_generate_clk();
	}
	usleep(2);
	gpio_spi_disable_cs();
	gpio_spi_set_mosi(0);
	usleep(10);
}

static unsigned char lcd_gpio_spi_config_read(unsigned char bit_9,unsigned char cmd,unsigned char len)
{
	int i=0;
	unsigned char cmd_val = 0;
	unsigned char get_reg_val = 0;
	unsigned char len_t = len;
	gpio_spi_disable_cs();//cs
	usleep(10);
	gpio_spi_init_clk();//sck
	gpio_spi_enable_cs();
	usleep(2);
	if(ecx336cndev.spi_is_9bit == 1)
	{
		gpio_spi_set_mosi(bit_9);//sda dat=0
		usleep(3);
		gpio_spi_generate_clk();
	}
	gpio_configure(ecx336cndev.spi_mosi_num, GPIO_DIR_OUTPUT);//sda
	for(i=8;i>0;i--){
		cmd_val = (cmd>>(i-1))&0x1;
		gpio_spi_set_mosi(cmd_val);
		usleep(2);
		gpio_spi_generate_clk();
	}
	usleep(2);
	gpio_spi_set_mosi(0);
	while(len_t--)
	{
		if(ecx336cndev.spi_mode == 0)
			gpio_configure(ecx336cndev.spi_mosi_num, GPIO_DIR_INPUT);//sda
		for(i=8;i>0;i--)
		{
			if(ecx336cndev.spi_mode == 0)
				cmd_val = gpio_get_input(ecx336cndev.spi_mosi_num);
			get_reg_val |= cmd_val<<(i-1);
			gpio_spi_generate_clk();
			usleep(2);
		}
		if(ecx336cndev.spi_mode == 0)
			gpio_configure(ecx336cndev.spi_mosi_num, GPIO_DIR_OUTPUT);//sda
		gpio_spi_disable_cs();
	}
	gpio_spi_set_mosi(0);
	usleep(10);
	return get_reg_val;
}

static void lcd_gpio_spi_config_write_16bit(unsigned short cmd)
{
	int i=0;
	unsigned char cmd_val = 0;
	gpio_spi_disable_cs();//cs
	usleep(10);
	gpio_spi_init_clk();//sck
	gpio_spi_enable_cs();
	usleep(2);

	for(i=16;i>0;i--){
		cmd_val = (cmd>>(i-1))&0x1;
		gpio_spi_set_mosi(cmd_val);
		usleep(2);
		gpio_spi_generate_clk();
	}
	usleep(2);
	gpio_spi_disable_cs();
	gpio_spi_set_mosi(0);
	usleep(10);
}

static void lcd_gpio_spi_config_send_cmds(unsigned char cmds)
{
	lcd_gpio_spi_config_write(0,cmds);
}

static void lcd_gpio_spi_config_send_data(unsigned char data)
{
	lcd_gpio_spi_config_write(1,data);
}

static void lcd_spi_send_cmds(unsigned char cmds)
{
	lcd_gpio_spi_config_write(0,cmds);
}

static void lcd_reset(void)
{
	if(ecx336cndev.lcd_reset_num!=0xff)
	{
		lcd_gpio_set_output(ecx336cndev.lcd_reset_num,1);
		usleep(500*1000);
		lcd_gpio_set_output(ecx336cndev.lcd_reset_num,0);
		usleep(500*1000);
		lcd_gpio_set_output(ecx336cndev.lcd_reset_num,1);
		usleep(500*1000);
	}
}
static unsigned char lcd_gpio_spi_config_read_16bit(unsigned char cmd)
{
	
	int i=0;
	unsigned char cmd_val = 0;
	unsigned char get_reg_val = 0;
	gpio_spi_disable_cs();//cs
	usleep(10);
	gpio_spi_init_clk();//sck
	gpio_spi_enable_cs();
	usleep(2);
	gpio_configure(ecx336cndev.spi_mosi_num, GPIO_DIR_OUTPUT);//sda
	for(i=8;i>0;i--){
		cmd_val = (cmd>>(i-1))&0x1;
		gpio_spi_set_mosi(cmd_val);
		usleep(2);
		gpio_spi_generate_clk();
	}
	usleep(2);
	gpio_spi_set_mosi(0);
	if(ecx336cndev.spi_mode == 0)
		gpio_configure(ecx336cndev.spi_mosi_num, GPIO_DIR_INPUT);//sda
	for(i=8;i>0;i--)
	{
		if(ecx336cndev.spi_mode == 0)
			cmd_val = gpio_get_input(ecx336cndev.spi_mosi_num);
		else
			cmd_val = gpio_get_input(ecx336cndev.spi_miso_num);
		get_reg_val |= cmd_val<<(i-1);
		gpio_spi_generate_clk();
		usleep(2);
	}
	gpio_spi_disable_cs();
	gpio_spi_set_mosi(0);
	usleep(10);
	return get_reg_val;
}

int lcd_gpio_spi_write_16bit(unsigned short u16_value)
{
	int ret = 0;
	uint8_t wdata[4] = { 0 };
	if (ecx336cndev.spi_fd <= 0)
	{
		printf("on found spidev\n");
		return -1;
	}

	wdata[1] = (u8)(u16_value >> 8);
	wdata[0] = (u8)(u16_value);

	struct spi_ioc_transfer xfer[1] = { {
		.tx_buf = (unsigned long)wdata,
		.rx_buf = (unsigned long)NULL,
		.len = 2,
		.delay_usecs = 0,
		.speed_hz = 30000,
		.bits_per_word = 16,
	} };

	ret = ioctl(ecx336cndev.spi_fd, SPI_IOC_MESSAGE(1), &xfer);

	if (ret < 1) {
		printf("can't send  spi message\n");
		return -1;
	}

	return ret;
}

static void ecx336cn_write_data(unsigned short data)
{
	if(ecx336cndev.spi_fd < 0)
    	lcd_gpio_spi_config_write_16bit(data);
	else
		lcd_gpio_spi_write_16bit(data);
}

static int ecx336cn_display_init(void)
{
	gpio_configure(ecx336cndev.spi_clk_num,GPIO_DIR_OUTPUT);//clk
	gpio_configure(ecx336cndev.spi_mosi_num,GPIO_DIR_OUTPUT);//mosi
	gpio_configure(ecx336cndev.spi_cs_num,GPIO_DIR_OUTPUT);//cs
	gpio_configure(ecx336cndev.lcd_stbyb_num,GPIO_DIR_OUTPUT);//STBYB
	gpio_configure(ecx336cndev.spi_miso_num,GPIO_DIR_OUTPUT);//miso
	gpio_configure(ecx336cndev.lcd_reset_num,GPIO_DIR_OUTPUT);//miso
	lcd_reset();
    printf("lcd init %s %d\n", __func__, __LINE__);
	#if 0
	ecx336cn_write_data(0x0050);
	ecx336cn_write_data(0x8000);
	ecx336cn_write_data(0x4010);
	ecx336cn_write_data(0xC004);
	ecx336cn_write_data(0x20FC);
	ecx336cn_write_data(0xA0D3);
	ecx336cn_write_data(0x6000);
	ecx336cn_write_data(0xE002);
	ecx336cn_write_data(0x10A3);
	ecx336cn_write_data(0x9000);
	ecx336cn_write_data(0x5008);
	ecx336cn_write_data(0xD000);
	ecx336cn_write_data(0x3000);
	ecx336cn_write_data(0xB000);
	ecx336cn_write_data(0x7000);
	ecx336cn_write_data(0xF06A);
	ecx336cn_write_data(0x0800);
	ecx336cn_write_data(0x8800);
	ecx336cn_write_data(0x4800);
	ecx336cn_write_data(0xC800);
	ecx336cn_write_data(0x2800);
	ecx336cn_write_data(0xA800);
	ecx336cn_write_data(0x6800);
	ecx336cn_write_data(0xE800);
	ecx336cn_write_data(0x1800);
	ecx336cn_write_data(0x9800);
	ecx336cn_write_data(0x5800);
	ecx336cn_write_data(0xD800);
	ecx336cn_write_data(0x3C00);
	ecx336cn_write_data(0xB800);
	ecx336cn_write_data(0x7800);
	ecx336cn_write_data(0xF800);
	ecx336cn_write_data(0x0480);
	ecx336cn_write_data(0x8400);
	ecx336cn_write_data(0x4402);
	ecx336cn_write_data(0xC402);
	ecx336cn_write_data(0x2402);
	ecx336cn_write_data(0xA401);
	ecx336cn_write_data(0x640C);
	ecx336cn_write_data(0xE4A2);
	ecx336cn_write_data(0x14AA);
	ecx336cn_write_data(0x94D0);
	ecx336cn_write_data(0x547D);
	ecx336cn_write_data(0xD41E);
	ecx336cn_write_data(0x1A10);
	ecx336cn_write_data(0xB45E);
	ecx336cn_write_data(0x7440);
	ecx336cn_write_data(0xF45F);
	ecx336cn_write_data(0x0C64);
	ecx336cn_write_data(0x8C80);
	ecx336cn_write_data(0x4C6D);
	ecx336cn_write_data(0xCC00);
	ecx336cn_write_data(0x2CC0);
	ecx336cn_write_data(0xAC5A);
	ecx336cn_write_data(0x6C00);
	ecx336cn_write_data(0xEC6E);
	ecx336cn_write_data(0x1C40);
	ecx336cn_write_data(0x9C7F);
	ecx336cn_write_data(0x5C40);
	ecx336cn_write_data(0xDCB0);
	ecx336cn_write_data(0x3C00);
	ecx336cn_write_data(0xBCD8);
	ecx336cn_write_data(0x7C00);
	ecx336cn_write_data(0xFE1E);
	ecx336cn_write_data(0x0280);
	ecx336cn_write_data(0x82BB);
	ecx336cn_write_data(0x4280);
	ecx336cn_write_data(0xC27B);
	ecx336cn_write_data(0x2201);
	ecx336cn_write_data(0xA200);
	ecx336cn_write_data(0x6200);
	ecx336cn_write_data(0xE2B4);
	ecx336cn_write_data(0x1218);
	ecx336cn_write_data(0x9280);
	ecx336cn_write_data(0x527E);
	ecx336cn_write_data(0xD210);
	ecx336cn_write_data(0x3250);
	ecx336cn_write_data(0xB220);
	ecx336cn_write_data(0x7200);
	ecx336cn_write_data(0xF25C);
	ecx336cn_write_data(0x0540);
	ecx336cn_write_data(0x8A1A);
	ecx336cn_write_data(0x4A80);
	ecx336cn_write_data(0xCAB4);
	ecx336cn_write_data(0x2A80);
	ecx336cn_write_data(0xAAA8);
	ecx336cn_write_data(0x6A00);
	ecx336cn_write_data(0xEA69);
	ecx336cn_write_data(0x1A88);
	ecx336cn_write_data(0x8D20);
	ecx336cn_write_data(0x5A88);
	ecx336cn_write_data(0xDA40);
	ecx336cn_write_data(0x3AA4);
	ecx336cn_write_data(0xBA20);
	ecx336cn_write_data(0x7AD0);
	ecx336cn_write_data(0xFA00);
	ecx336cn_write_data(0x06C4);
	ecx336cn_write_data(0x8640);
	ecx336cn_write_data(0x4658);
	ecx336cn_write_data(0xC600);
	ecx336cn_write_data(0x2658);
	ecx336cn_write_data(0xA680);
	ecx336cn_write_data(0x6631);
	ecx336cn_write_data(0xE60C);
	ecx336cn_write_data(0x1600);
	ecx336cn_write_data(0x9600);
	ecx336cn_write_data(0x5600);
	ecx336cn_write_data(0xD600);
	ecx336cn_write_data(0x3600);
	ecx336cn_write_data(0xB600);
	ecx336cn_write_data(0x7600);
	ecx336cn_write_data(0xF606);
	ecx336cn_write_data(0x0700);
	ecx336cn_write_data(0x8E00);
	ecx336cn_write_data(0x4E00);
	ecx336cn_write_data(0xCE00);
	ecx336cn_write_data(0x2E00);
	ecx336cn_write_data(0xAE00);
	ecx336cn_write_data(0x6E00);
	ecx336cn_write_data(0xEE00);
	ecx336cn_write_data(0x1E00);
	ecx336cn_write_data(0x9E0B);
	ecx336cn_write_data(0x5E00);
	ecx336cn_write_data(0xDE00);
	ecx336cn_write_data(0x3E00);
	ecx336cn_write_data(0xBE00);
	ecx336cn_write_data(0x7E00);
	ecx336cn_write_data(0xFE00);
	msleep(3000);
	ecx336cn_write_data(0x547D);
	ecx336cn_write_data(0x00D0);
	#endif
	ecx336cn_write_data(0x0070);
	ecx336cn_write_data(0x8000);
	ecx336cn_write_data(0x4000);
	ecx336cn_write_data(0xC004);
	ecx336cn_write_data(0x20FC);
	ecx336cn_write_data(0xA013);
	ecx336cn_write_data(0x6000);
	ecx336cn_write_data(0xE002);
	ecx336cn_write_data(0x1001);
	ecx336cn_write_data(0x9000);
	ecx336cn_write_data(0x5008);
	ecx336cn_write_data(0xD000);
	ecx336cn_write_data(0x3000);
	ecx336cn_write_data(0xB000);
	ecx336cn_write_data(0x7000);
	ecx336cn_write_data(0xF06A);
	ecx336cn_write_data(0x0800);
	ecx336cn_write_data(0x8800);
	ecx336cn_write_data(0x4800);
	ecx336cn_write_data(0xC800);
	ecx336cn_write_data(0x2800);
	ecx336cn_write_data(0xA800);
	ecx336cn_write_data(0x6800);
	ecx336cn_write_data(0xE800);
	ecx336cn_write_data(0x1800);
	ecx336cn_write_data(0x9800);
	ecx336cn_write_data(0x5800);
	ecx336cn_write_data(0xD800);
	ecx336cn_write_data(0x3800);
	ecx336cn_write_data(0xB800);
	ecx336cn_write_data(0x7800);
	ecx336cn_write_data(0xF800);
	ecx336cn_write_data(0x0480);
	ecx336cn_write_data(0x8400);
	ecx336cn_write_data(0x4402);
	ecx336cn_write_data(0xC402);
	ecx336cn_write_data(0x2402);
	ecx336cn_write_data(0xA401);
	ecx336cn_write_data(0x6402);
	ecx336cn_write_data(0xE402);
	ecx336cn_write_data(0x1402);
	ecx336cn_write_data(0x94D0);
	ecx336cn_write_data(0x547D);
	ecx336cn_write_data(0xD43C);
	ecx336cn_write_data(0x3440);
	ecx336cn_write_data(0xB45E);
	ecx336cn_write_data(0x7440);
	ecx336cn_write_data(0xF45F);
	ecx336cn_write_data(0x0C7C);
	ecx336cn_write_data(0x8C80);
	ecx336cn_write_data(0x4C73);
	ecx336cn_write_data(0xCC00);
	ecx336cn_write_data(0x2CC0);
	ecx336cn_write_data(0xAC5A);
	ecx336cn_write_data(0x6C00);
	ecx336cn_write_data(0xEC6E);
	ecx336cn_write_data(0x1C40);
	ecx336cn_write_data(0x9C7F);
	ecx336cn_write_data(0x5C40);
	ecx336cn_write_data(0xDCB0);
	ecx336cn_write_data(0x3C00);
	ecx336cn_write_data(0xBCCC);
	ecx336cn_write_data(0x7C00);
	ecx336cn_write_data(0xFC2C);
	ecx336cn_write_data(0x0240);
	ecx336cn_write_data(0x82D0);
	ecx336cn_write_data(0x4240);
	ecx336cn_write_data(0xC230);
	ecx336cn_write_data(0x2201);
	ecx336cn_write_data(0xA200);
	ecx336cn_write_data(0x6200);
	ecx336cn_write_data(0xE2B4);
	ecx336cn_write_data(0x1210);
	ecx336cn_write_data(0x9280);
	ecx336cn_write_data(0x527E);
	ecx336cn_write_data(0xD210);
	ecx336cn_write_data(0x3250);
	ecx336cn_write_data(0xB220);
	ecx336cn_write_data(0x7200);
	ecx336cn_write_data(0xF25C);
	ecx336cn_write_data(0x0A80);
	ecx336cn_write_data(0x8A1A);
	ecx336cn_write_data(0x4A80);
	ecx336cn_write_data(0xCAB4);
	ecx336cn_write_data(0x2A80);
	ecx336cn_write_data(0xAAA8);
	ecx336cn_write_data(0x6A00);
	ecx336cn_write_data(0xEAD4);
	ecx336cn_write_data(0x1A88);
	ecx336cn_write_data(0x9A40);
	ecx336cn_write_data(0x5A88);
	ecx336cn_write_data(0xDA40);
	ecx336cn_write_data(0x3AA4);
	ecx336cn_write_data(0xBA20);
	ecx336cn_write_data(0x7AD0);
	ecx336cn_write_data(0xFA00);
	ecx336cn_write_data(0x06C4);
	ecx336cn_write_data(0x8640);
	ecx336cn_write_data(0x4658);
	ecx336cn_write_data(0xC600);
	ecx336cn_write_data(0x2650);
	ecx336cn_write_data(0xA680);
	ecx336cn_write_data(0x6631);
	ecx336cn_write_data(0xE60C);
	ecx336cn_write_data(0x1600);
	ecx336cn_write_data(0x9600);
	ecx336cn_write_data(0x5600);
	ecx336cn_write_data(0xD600);
	ecx336cn_write_data(0x3600);
	ecx336cn_write_data(0xB600);
	ecx336cn_write_data(0x7600);
	ecx336cn_write_data(0xF606);
	ecx336cn_write_data(0x0E00);
	ecx336cn_write_data(0x8E00);
	ecx336cn_write_data(0x4E00);
	ecx336cn_write_data(0xCE00);
	ecx336cn_write_data(0x2E00);
	ecx336cn_write_data(0xAE00);
	ecx336cn_write_data(0x6E00);
	ecx336cn_write_data(0xEE00);
	ecx336cn_write_data(0x1E00);
	ecx336cn_write_data(0x9E16);
	ecx336cn_write_data(0x5E00);
	ecx336cn_write_data(0xDE00);
	ecx336cn_write_data(0x3E00);
	ecx336cn_write_data(0xBE00);
	ecx336cn_write_data(0x7E00);
	ecx336cn_write_data(0xFE00);
	usleep(120);
	ecx336cn_write_data(0x00F0);
	ecx336cn_write_data(0x1000);
	ecx336cn_write_data(0xA001);
}

static struct lcd_map_list ecx336cn_map = {
	.map = {
		.lcd_init = ecx336cn_display_init,
		.name = "lcd-ecx336cn",
	}
};

static int ecx336cn_probe(const char *node)
{
	int np = fdt_node_probe_by_path(node);
	const char *spi_devpath = NULL;
	if(np < 0){
		goto error;
	}

	memset(&ecx336cndev,0,sizeof(struct ecx336cn_dev));

	ecx336cndev.spi_clk_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.sck;
	ecx336cndev.spi_clk_vaild_edge=1;
	ecx336cndev.spi_cs_polar=0;
	ecx336cndev.spi_is_9bit=1;
	ecx336cndev.spi_cs_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.cs;
	ecx336cndev.spi_mosi_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.mosi;
	ecx336cndev.spi_miso_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.miso;
	ecx336cndev.lcd_stbyb_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.stbyb;
	ecx336cndev.lcd_stbyb_polar = 0;
	ecx336cndev.lcd_reset_num = PINPAD_INVALID;
#if 0
//  gpio_configure(PINPAD_T23,GPIO_DIR_OUTPUT);//reset
	ecx336cndev.spi_clk_num=PINPAD_L03;
	ecx336cndev.spi_clk_vaild_edge=1;
	ecx336cndev.spi_cs_polar=0;
	ecx336cndev.spi_is_9bit=1;
	ecx336cndev.spi_cs_num=PINPAD_L01;
	ecx336cndev.spi_mosi_num=PINPAD_L02;
	ecx336cndev.lcd_reset_num=PINPAD_INVALID;
	ecx336cndev.lcd_reset_polar=0;
#endif
	ecx336cndev.spi_fd = -1;
	fdt_get_property_u_32_index(np, "reset", 			0, &ecx336cndev.lcd_reset_num);
	fdt_get_property_u_32_index(np, "spi-gpio-sck", 	0, &ecx336cndev.spi_clk_num);
	fdt_get_property_u_32_index(np, "spi-gpio-mosi", 	0, &ecx336cndev.spi_mosi_num);
	fdt_get_property_u_32_index(np, "spi-gpio-miso", 	0, &ecx336cndev.spi_miso_num);
	fdt_get_property_u_32_index(np, "spi-gpio-cs", 		0, &ecx336cndev.spi_cs_num);
	fdt_get_property_u_32_index(np, "spi-gpio-stbyb", 	0, &ecx336cndev.lcd_stbyb_num);
	printf("ecx336cndev.lcd_stbyb_num = %d %d %d %d %d\n",
	       ecx336cndev.spi_clk_num, ecx336cndev.spi_mosi_num,
	       ecx336cndev.spi_cs_num, ecx336cndev.lcd_stbyb_num,
	       ecx336cndev.lcd_reset_num);
	fdt_get_property_string_index(np, "devpath", 0, &spi_devpath);
	if(spi_devpath != NULL)
	{
		ecx336cndev.spi_fd = open(spi_devpath, O_RDWR);
		if(ecx336cndev.spi_fd < 0)
		{
			printf("ecx336cndev.spi_fd open error devpath = %s\n", spi_devpath);
			return 0;
		}
		u32 mode = SPI_MODE_0;
		if (ioctl(ecx336cndev.spi_fd, SPI_IOC_WR_MODE32, &mode) == -1) {
			printf("can't set spi mode");
			return 0;
		}
	}

	int default_off = fdt_property_read_bool(np, "default-off");
	if(default_off ==0)
		ecx336cn_display_init();

	ecx336cn_map.map.default_off_val = default_off;
	lcd_map_register(&ecx336cn_map);
error:
	return 0;
}

static int ecx336cn_init(void)
{
	ecx336cn_probe("/hcrtos/lcd-ecx336cn");
	return 0;
}

module_driver(ecx336cn, ecx336cn_init, NULL, 2)

