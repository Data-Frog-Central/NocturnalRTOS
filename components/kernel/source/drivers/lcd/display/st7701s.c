#define LOG_TAG "lcd_st7701s"
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
#include <kernel/delay.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <hcuapi/spidev.h>
#include "../lcd_main.h"

/*
*	TIME: 2023 06 26
*	support: yt200 180*320 config

	sfspi {
		pinmux-active = <PINPAD_T15 1 PINPAD_T16 1 PINPAD_T17 1 PINPAD_T18 1>;
		sclk = <50000000>;
		dma-mode = <1>;
		status = "okay";

		spi_nor_flash {
			spi-tx-bus-width = <1>;
			spi-rx-bus-width = <1>;
			reg = <0>;
			status = "okay";
			partitions {
				part-num = <5>;

				part1-label = "boot";
				part1-reg = <0x0 0x5f000>;
				part1-filename = "bootloader.bin";

				part2-label = "individual";
				part2-reg = <0x5f000 0x1000>;
				part2-filename = "hrxkey.bin";

				part3-label = "eromfs";
				part3-reg = <0x60000 0xa0000>;
				part3-filename = "romfs.img";

				part4-label = "firmware";
				part4-reg = <0x100000 0x5e0000>;
				part4-filename = "hcdemo.uImage";

				part5-label = "persistentmem";
				part5-reg = <0x7e0000 0x20000>;
				part5-filename = "persistentmem.bin";
			};
		};

		spidev@0 {
			devpath = "/dev/spidev0";
			reg = <0>;
			spi-max-frequency = <50000000>;
			status = "okay";
		};
	};

	lcd-st7701s{
		spi-gpio-cs		= <PINPAD_L04>;
		devpath = "/dev/spidev0";
		default-off;
		status = "okay";
	};
	lcd{
		lcd-map-name = "lcd-st7701s";
		default-off;
		status = "okay";
	};
*/

typedef struct st7701s_dev{
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
	const char *spi_devpath;
}st7701s_dev_t;
static st7701s_dev_t st7701sdev;


static void gpio_spi_set_mosi(unsigned char data)
{
    lcd_gpio_set_output(st7701sdev.spi_mosi_num,(bool)data);
}

static void gpio_spi_generate_clk(void)
{   
    if(st7701sdev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(st7701sdev.spi_clk_num,1);
	else
		lcd_gpio_set_output(st7701sdev.spi_clk_num,0);
	usleep(2);
	if(st7701sdev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(st7701sdev.spi_clk_num,0);
	else
		lcd_gpio_set_output(st7701sdev.spi_clk_num,1);
}

static void gpio_spi_enable_cs(void)//==0
{
	if(st7701sdev.spi_cs_polar == 0)	
		lcd_gpio_set_output(st7701sdev.spi_cs_num,0);
	else
		lcd_gpio_set_output(st7701sdev.spi_cs_num,1);
}

static void gpio_spi_disable_cs(void)
{
	if(st7701sdev.spi_cs_polar == 0)
		lcd_gpio_set_output(st7701sdev.spi_cs_num,1);//cs
	else
		lcd_gpio_set_output(st7701sdev.spi_cs_num,0);//cs
}

static void gpio_spi_init_clk(void)
{
	if(st7701sdev.spi_clk_vaild_edge == 1)//1 //==1
		lcd_gpio_set_output(st7701sdev.spi_clk_num,0);//sck
	else
		lcd_gpio_set_output(st7701sdev.spi_clk_num,1);//sck
		
}

static void lcd_gpio_spi_write(unsigned char bit_9,unsigned char cmd)
{
	int i=0;
	unsigned char cmd_val = 0;
	gpio_spi_disable_cs();//cs
	usleep(10);
	gpio_spi_init_clk();//sck
	gpio_spi_enable_cs();
	usleep(2);
	if(st7701sdev.spi_is_9bit == 1)
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

static int lcd_spi_dev_write(unsigned char bit_9,unsigned char cmd)
{
	int ret = 0;
	uint16_t wdata[2] = { 0 };
	uint16_t tmp = 0;
	int spi_fd = 0;

	spi_fd = open(st7701sdev.spi_devpath, O_RDWR);
	if (spi_fd <= 0) {
		printf("on found spidev\n");
		return -1;
	}

	tmp = (bit_9 << 8) | cmd;
	wdata[0] = tmp;

	struct spi_ioc_transfer xfer[1] = { {
		.tx_buf = (unsigned long)wdata,
		.rx_buf = (unsigned long)NULL,
		.len = 1,
		.delay_usecs = 0,
		.speed_hz = 30000,
		.bits_per_word = 9,
	} };

	ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer);

	if (ret < 1) {
		printf("can't send  spi message\n");
		return -1;
	}

	close(spi_fd);

	return ret;
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

static void lcd_gpio_spi_config_write(unsigned char bit_9,unsigned char cmd)
{
	if(st7701sdev.spi_fd > 0)
		lcd_spi_dev_write(bit_9, cmd);
	else
		lcd_gpio_spi_write(bit_9, cmd);
		// lcd_gpio_spi_config_write_16bit(bit_9<<8 | cmd);
}

static void st7701s_write_data(unsigned char data)
{
	lcd_gpio_spi_config_write(1,data);
}

static void st7701s_spi_sends_data(unsigned char *data,unsigned char len)
{
	int i=len;
	do{
		st7701s_write_data(*data++);
	}
	while(i--);
}
static void st7701s_write_command(unsigned short cmds)
{
	lcd_gpio_spi_config_write(0,(unsigned char)cmds);
}

static void lcd_reset(void)
{
	if(st7701sdev.lcd_reset_num!=PINPAD_INVALID)
	{
		lcd_gpio_set_output(st7701sdev.lcd_reset_num,!st7701sdev.lcd_reset_polar);
		usleep(500*1000);
		lcd_gpio_set_output(st7701sdev.lcd_reset_num,st7701sdev.lcd_reset_polar);
		usleep(500*1000);
		lcd_gpio_set_output(st7701sdev.lcd_reset_num,!st7701sdev.lcd_reset_polar);
		usleep(500*1000);
	}
}

static int st7701s_display_init(void)
{
	gpio_spi_disable_cs();
	gpio_configure(st7701sdev.spi_clk_num,GPIO_DIR_OUTPUT);//clk
	gpio_configure(st7701sdev.spi_cs_num,GPIO_DIR_OUTPUT);//mosi
	gpio_configure(st7701sdev.spi_mosi_num,GPIO_DIR_OUTPUT);//cs
	gpio_configure(st7701sdev.lcd_reset_num,GPIO_DIR_OUTPUT);//reset
	printf("%s %d\n", __FUNCTION__,__LINE__);
	REG32_SET_BIT(0xb8800094, BIT17);
	REG32_SET_BIT(0xb8800094, BIT19);
	REG32_SET_BIT(0xb8800094, BIT20);
	REG32_SET_BIT(0xb8800094, BIT24);

	lcd_reset();
	msleep(120);

#if 1
	st7701s_write_command(0xFF);
	st7701s_write_data(0x77);
	st7701s_write_data(0x01);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x13);

	st7701s_write_command(0xEF);
	st7701s_write_data(0x08);

	st7701s_write_command(0xFF);
	st7701s_write_data(0x77);
	st7701s_write_data(0x01);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x10);

	st7701s_write_command(0xC0);
	st7701s_write_data(0x63);
	st7701s_write_data(0x00);

	st7701s_write_command(0xC1);
	st7701s_write_data(0x09);
	st7701s_write_data(0x0C);

	st7701s_write_command(0xC2);
	st7701s_write_data(0x07);
	st7701s_write_data(0x08);

	st7701s_write_command(0xCC);
	st7701s_write_data(0x30);

	st7701s_write_command(0xB0);
	st7701s_write_data(0x00);
	st7701s_write_data(0x0D);
	st7701s_write_data(0x14);
	st7701s_write_data(0x0D);
	st7701s_write_data(0x11);
	st7701s_write_data(0x07);
	st7701s_write_data(0x04);
	st7701s_write_data(0x08);
	st7701s_write_data(0x08);
	st7701s_write_data(0x20);
	st7701s_write_data(0x05);
	st7701s_write_data(0x14);
	st7701s_write_data(0x12);
	st7701s_write_data(0x25);
	st7701s_write_data(0x2D);
	st7701s_write_data(0x1C);

	st7701s_write_command(0xB1);
	st7701s_write_data(0x00);
	st7701s_write_data(0x0C);
	st7701s_write_data(0x14);
	st7701s_write_data(0x0D);
	st7701s_write_data(0x11);
	st7701s_write_data(0x06);
	st7701s_write_data(0x03);
	st7701s_write_data(0x08);
	st7701s_write_data(0x08);
	st7701s_write_data(0x1F);
	st7701s_write_data(0x05);
	st7701s_write_data(0x14);
	st7701s_write_data(0x12);
	st7701s_write_data(0x25);
	st7701s_write_data(0x2E);
	st7701s_write_data(0x1C);

	st7701s_write_command(0xFF);
	st7701s_write_data(0x77);
	st7701s_write_data(0x01);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x11);

	st7701s_write_command(0xB0);
	st7701s_write_data(0x58);

	st7701s_write_command(0xB1);
	st7701s_write_data(0x4A);

	st7701s_write_command(0xB2);
	st7701s_write_data(0x87);

	st7701s_write_command(0xB3);
	st7701s_write_data(0x80);

	st7701s_write_command(0xB5);
	st7701s_write_data(0x4C);

	st7701s_write_command(0xB7);
	st7701s_write_data(0x8A);

	st7701s_write_command(0xB8);
	st7701s_write_data(0x21);

	st7701s_write_command(0xC0);
	st7701s_write_data(0x03);

	st7701s_write_command(0xC1);
	st7701s_write_data(0x78);

	st7701s_write_command(0xC2);
	st7701s_write_data(0x78);

	st7701s_write_command(0xD0);
	st7701s_write_data(0x88);

	st7701s_write_command(0xE0);
	st7701s_write_data(0x00);

	st7701s_write_data(0x00);
	st7701s_write_data(0x02);

	st7701s_write_command(0xE1);
	st7701s_write_data(0x01);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x03);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x02);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x04);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x00);
	st7701s_write_data(0x44);
	st7701s_write_data(0x44);

	st7701s_write_command(0xE2);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);

	st7701s_write_command(0xE3);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x33);
	st7701s_write_data(0x33);

	st7701s_write_command(0xE4);
	st7701s_write_data(0x44);
	st7701s_write_data(0x44);

	st7701s_write_command(0xE5);
	st7701s_write_data(0x01);
	st7701s_write_data(0x26);
	st7701s_write_data(0xA0);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x03);
	st7701s_write_data(0x28);
	st7701s_write_data(0xA0);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x05);
	st7701s_write_data(0x2A);
	st7701s_write_data(0xA0);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x07);
	st7701s_write_data(0x2C);
	st7701s_write_data(0xA0);
	st7701s_write_data(0xA0);

	st7701s_write_command(0xE6);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x33);
	st7701s_write_data(0x33);

	st7701s_write_command(0xE7);
	st7701s_write_data(0x44);
	st7701s_write_data(0x44);

	st7701s_write_command(0xE8);
	st7701s_write_data(0x02);
	st7701s_write_data(0x26);
	st7701s_write_data(0xA0);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x04);
	st7701s_write_data(0x28);
	st7701s_write_data(0xA0);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x06);
	st7701s_write_data(0x2A);
	st7701s_write_data(0xA0);
	st7701s_write_data(0xA0);
	st7701s_write_data(0x08);
	st7701s_write_data(0x2C);
	st7701s_write_data(0xA0);
	st7701s_write_data(0xA0);

	st7701s_write_command(0xEB);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0xE4);
	st7701s_write_data(0xE4);
	st7701s_write_data(0x44);
	st7701s_write_data(0x00);
	st7701s_write_data(0x40);

	st7701s_write_command(0xED);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xF7);
	st7701s_write_data(0x65);
	st7701s_write_data(0x4F);
	st7701s_write_data(0x0B);
	st7701s_write_data(0xA1);
	st7701s_write_data(0xCF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFC);
	st7701s_write_data(0x1A);
	st7701s_write_data(0xB0);
	st7701s_write_data(0xF4);
	st7701s_write_data(0x56);
	st7701s_write_data(0x7F);
	st7701s_write_data(0xFF);

	st7701s_write_command(0xEF);
	st7701s_write_data(0x08);
	st7701s_write_data(0x08);
	st7701s_write_data(0x08);
	st7701s_write_data(0x45);
	st7701s_write_data(0x3F);
	st7701s_write_data(0x54);

	st7701s_write_command(0xFF);
	st7701s_write_data(0x77);
	st7701s_write_data(0x01);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x13);

	st7701s_write_command(0xE8);
	st7701s_write_data(0x00);
	st7701s_write_data(0x0e);

	st7701s_write_command (0x3A);//565RGB
	st7701s_write_data (0x77);

	st7701s_write_command (0x36);//565RGB
	st7701s_write_data (0x08);

	st7701s_write_command(0x11);
	msleep(120);
	st7701s_write_command(0xE8);
	st7701s_write_data(0x00);
	st7701s_write_data(0x0C);
	msleep(20);
	st7701s_write_command(0xE8);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_command(0xE6);
	st7701s_write_data(0x16);
	st7701s_write_data(0x7C);
	st7701s_write_command(0xFF);
	st7701s_write_data(0x77);
	st7701s_write_data(0x01);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_command(0x29);
#endif
#if 0
	st7701s_write_command (0x01);
	msleep(120);
	st7701s_write_command (0xFF);
	st7701s_write_data (0x77);
	st7701s_write_data (0x01);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x13);
	st7701s_write_command (0xEF);
	st7701s_write_data (0x08);
	st7701s_write_command (0xFF);
	st7701s_write_data (0x77);
	st7701s_write_data (0x01);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x06);
	st7701s_write_data (0x07);
	st7701s_write_data (0x0A);
	st7701s_write_data (0x09);
	st7701s_write_data (0x22);
	st7701s_write_data (0x04);
	st7701s_write_data (0x10);
	st7701s_write_data (0x0E);
	st7701s_write_data (0x28);
	st7701s_write_data (0x30);
	st7701s_write_data (0x1C);
	st7701s_write_command (0xB1);
	st7701s_write_data (0x00);
	st7701s_write_data (0x12);
	st7701s_write_data (0x19);
	st7701s_write_data (0x0D);
	st7701s_write_data (0x10);
	st7701s_write_data (0x04);
	st7701s_write_data (0x06);
	st7701s_write_data (0x07);
	st7701s_write_data (0x08);
	st7701s_write_data (0x23);
	st7701s_write_data (0x04);
	st7701s_write_data (0x12);
	st7701s_write_data (0x11);
	st7701s_write_data (0x28);
	st7701s_write_data (0x30);
	st7701s_write_data (0x1C);
	st7701s_write_command (0xFF);
	st7701s_write_data (0x77);
	st7701s_write_data (0x01);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x11);
	st7701s_write_command (0xB0);
	st7701s_write_data (0x4D);
	st7701s_write_command (0xB1);
	st7701s_write_data (0x47);
	st7701s_write_command (0xB2);
	st7701s_write_data (0x80);
	st7701s_write_command (0xB3);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_command (0xE3);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x33);
	st7701s_write_data (0x33);
	st7701s_write_command (0xE4);
	st7701s_write_data (0x44);
	st7701s_write_data (0x44);
	st7701s_write_command (0xE5);
	st7701s_write_data (0x01);
	st7701s_write_data (0x26);
	st7701s_write_data (0xA0);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x03);
	st7701s_write_data (0x28);
	st7701s_write_data (0xA0);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x05);
	st7701s_write_data (0x2A);
	st7701s_write_data (0xA0);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x07);
	st7701s_write_data (0x2C);
	st7701s_write_data (0xA0);
	st7701s_write_data (0xA0);
	st7701s_write_command (0xE6);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x33);
	st7701s_write_data (0x33);
	st7701s_write_command (0xE7);
	st7701s_write_data (0x44);
	st7701s_write_data (0x44);
	st7701s_write_command (0xE8);
	st7701s_write_data (0x02);
	st7701s_write_data (0x26);
	st7701s_write_data (0xA0);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x08);
	st7701s_write_data (0x45);
	st7701s_write_data (0x3F);
	st7701s_write_data (0x54);
	st7701s_write_command (0x35);
	st7701s_write_data (0x00);
	st7701s_write_command (0x11);
	msleep(120);
	st7701s_write_command (0xFF);
	st7701s_write_data (0x77);
	st7701s_write_data (0x01);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x12);
	st7701s_write_command (0xD1);
	st7701s_write_data (0x81);
	st7701s_write_command (0xD2);
	st7701s_write_data (0x0C);
	st7701s_write_data (0x10);
	st7701s_write_command (0xC0);
	st7701s_write_data (0x63);
	st7701s_write_data (0x00);
	st7701s_write_command (0xC1);
	st7701s_write_data (0x09);
	st7701s_write_data (0x02);
	st7701s_write_command (0xC2);
	st7701s_write_data (0x07);
	st7701s_write_data (0x08);
	st7701s_write_command (0xB0);
	st7701s_write_data (0x00);
	st7701s_write_data (0x11);
	st7701s_write_data (0x19);
	st7701s_write_data (0x0C);
	st7701s_write_data (0x10);
	st7701s_write_data (0x80);
	st7701s_write_command (0xB5);
	st7701s_write_data (0x40);
	st7701s_write_command (0xB7);
	st7701s_write_data (0x8A);
	st7701s_write_command (0xB8);
	st7701s_write_data (0x21);
	st7701s_write_command (0xC0);
	st7701s_write_data (0x09);
	st7701s_write_command (0xC1);
	st7701s_write_data (0x78);
	st7701s_write_command (0xC2);
	st7701s_write_data (0x78);
	st7701s_write_command (0xD0);
	st7701s_write_data (0x88);
	st7701s_write_command (0xE0);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x02);
	st7701s_write_command (0xE1);
	st7701s_write_data (0x01);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x03);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x02);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x04);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x00);
	st7701s_write_data (0x44);
	st7701s_write_data (0x44);
	st7701s_write_command (0xE2);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x04);
	st7701s_write_data (0x28);
	st7701s_write_data (0xA0);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x06);
	st7701s_write_data (0x2A);
	st7701s_write_data (0xA0);
	st7701s_write_data (0xA0);
	st7701s_write_data (0x08);
	st7701s_write_data (0x2C);
	st7701s_write_data (0xA0);
	st7701s_write_data (0xA0);
	st7701s_write_command (0xEB);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0xE4);
	st7701s_write_data (0xE4);
	st7701s_write_data (0x44);
	st7701s_write_data (0x00);
	st7701s_write_data (0x40);
	st7701s_write_command (0xED);
	st7701s_write_data (0xFF);
	st7701s_write_data (0xF7);
	st7701s_write_data (0x65);
	st7701s_write_data (0x4F);
	st7701s_write_data (0x0B);
	st7701s_write_data (0xA1);
	st7701s_write_data (0xCF);
	st7701s_write_data (0xFF);
	st7701s_write_data (0xFF);
	st7701s_write_data (0xFC);
	st7701s_write_data (0x1A);
	st7701s_write_data (0xB0);
	st7701s_write_data (0xF4);
	st7701s_write_data (0x56);
	st7701s_write_data (0x7F);
	st7701s_write_data (0xFF);
	st7701s_write_command (0xEF);
	st7701s_write_data (0x08);
	st7701s_write_data (0x08);
	st7701s_write_data (0x00);
	st7701s_write_command (0x29);
	st7701s_write_command (0xFF);
	st7701s_write_data (0x77);
	st7701s_write_data (0x01);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x12);
	st7701s_write_command (0xD1);
	st7701s_write_data (0x00);
	st7701s_write_command (0xFF);
	st7701s_write_data (0x77);
	st7701s_write_data (0x01);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	st7701s_write_data (0x00);
	msleep(120);
	gpio_spi_disable_cs();
#endif



	#if 0
	st7701s_write_command(0x11);

	msleep(120);
	
	st7701s_write_command(0xFF);
	st7701s_write_data(0x77);
	st7701s_write_data(0x01);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x10);
	
	st7701s_write_command(0xC0);
	st7701s_write_data(0x63);
	st7701s_write_data(0x00);

	
	st7701s_write_command(0xC1);
	st7701s_write_data(0x0A);
	st7701s_write_data(0x02);

	
	st7701s_write_command(0xC2);
	st7701s_write_data(0x31);
	st7701s_write_data(0x08);

	st7701s_write_command(0xCC);
	st7701s_write_data(0x10);

	/*///////////////////////////gamma//////////////////////////*/
	
	st7701s_write_command(0xB0);
	st7701s_write_data(0x00);
	st7701s_write_data(0x11);
	st7701s_write_data(0x19);
	st7701s_write_data(0x0C);
	st7701s_write_data(0x10);
	st7701s_write_data(0x06);
	st7701s_write_data(0x07);
	st7701s_write_data(0x0A);
	st7701s_write_data(0x09);
	st7701s_write_data(0x22);
	st7701s_write_data(0x04);
	st7701s_write_data(0x10);
	st7701s_write_data(0x0E);
	st7701s_write_data(0x28);
	st7701s_write_data(0x30);
	st7701s_write_data(0x1C);

	
	st7701s_write_command(0xB1);
	st7701s_write_data(0x00);
	st7701s_write_data(0x12);
	st7701s_write_data(0x19);
	st7701s_write_data(0x0D);
	st7701s_write_data(0x10);
	st7701s_write_data(0x04);
	st7701s_write_data(0x06);
	st7701s_write_data(0x07);
	st7701s_write_data(0x08);
	st7701s_write_data(0x23);
	st7701s_write_data(0x04);
	st7701s_write_data(0x12);
	st7701s_write_data(0x11);
	st7701s_write_data(0x28);
	st7701s_write_data(0x30);
	st7701s_write_data(0x1C);
	/*/////////////////////////////////////////////////////////////*/

	
	st7701s_write_command(0xFF);
	st7701s_write_data(0x77);
	st7701s_write_data(0x01);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x11);

	
	st7701s_write_command(0xB0);
	st7701s_write_data(0x4D);

	
	st7701s_write_command(0xB1);
	st7701s_write_data(0x3E);

	
	st7701s_write_command(0xB2);
	st7701s_write_data(0x07);

	
	st7701s_write_command(0xB3);
	st7701s_write_data(0x80);

	
	st7701s_write_command(0xB5);
	st7701s_write_data(0x47);

	
	st7701s_write_command(0xB7);
	st7701s_write_data(0x85);

	
	st7701s_write_command(0xB8);
	st7701s_write_data(0x21);

	
	st7701s_write_command(0xB9);
	st7701s_write_data(0x10);

	
	st7701s_write_command(0xC1);
	st7701s_write_data(0x78);

	
	st7701s_write_command(0xC2);
	st7701s_write_data(0x78);

	
	st7701s_write_command(0xD0);
	st7701s_write_data(0x88);

	msleep(100);
	st7701s_write_command(0xE0);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x02);

	
	st7701s_write_command(0xE1);
	st7701s_write_data(0x04);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x05);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x20);
	st7701s_write_data(0x20);

	
	st7701s_write_command(0xE2);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);

	
	st7701s_write_command(0xE3);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x33);
	st7701s_write_data(0x00);

	st7701s_write_command(0xE4);
	st7701s_write_data(0x22);
	st7701s_write_data(0x00);

	
	st7701s_write_command(0xE5);
	st7701s_write_data(0x04);
	st7701s_write_data(0x34);
	st7701s_write_data(0xAA);
	st7701s_write_data(0xAA);
	st7701s_write_data(0x06);
	st7701s_write_data(0x34);
	st7701s_write_data(0xAA);
	st7701s_write_data(0xAA);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);

	st7701s_write_command(0xE6);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x33);
	st7701s_write_data(0x00);

	
	st7701s_write_command(0xE7);
	st7701s_write_data(0x22);
	st7701s_write_data(0x00);

	
	st7701s_write_command(0xE8);
	st7701s_write_data(0x05);
	st7701s_write_data(0x34);
	st7701s_write_data(0xAA);
	st7701s_write_data(0xAA);
	st7701s_write_data(0x07);
	st7701s_write_data(0x34);
	st7701s_write_data(0xAA);
	st7701s_write_data(0xAA);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);

	st7701s_write_command(0xEB);
	st7701s_write_data(0x02);
	st7701s_write_data(0x00);
	st7701s_write_data(0x40);
	st7701s_write_data(0x40);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);

	
	st7701s_write_command(0xEC);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);

	
	st7701s_write_command(0xED);
	st7701s_write_data(0xFA);
	st7701s_write_data(0x45);
	st7701s_write_data(0x0B);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xFF);
	st7701s_write_data(0xB0);
	st7701s_write_data(0x54);
	st7701s_write_data(0xAF);

	
	st7701s_write_command(0xFF);
	st7701s_write_data(0x77);
	st7701s_write_data(0x01);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);

	st7701s_write_command(0x2A);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x01);
	st7701s_write_data(0xdf);

	st7701s_write_command(0x2B);
	st7701s_write_data(0x00);
	st7701s_write_data(0x00);
	st7701s_write_data(0x03);
	st7701s_write_data(0x1f);

	st7701s_write_command(0x2C);

	st7701s_write_command(0x29);
	#endif


	//pinmux_configure(st7701sdev.spi_clk_num,1);//clk
	//pinmux_configure(st7701sdev.spi_cs_num,1);//mosi
	//pinmux_configure(st7701sdev.spi_mosi_num,1);//cs

	return 0;
}

static int st7701s_rorate(lcd_rotate_type_e dir)
{
	unsigned char data = 0x0+0x10;
	printf("dir = %d\n",dir);
	switch(dir)
	{
		case LCD_ROTATE_0:
			data = 0x80+0x10;//0xc0+0x10;//0xc0
			break;
		case LCD_H_MIRROR:
			data = 0xc0+0x10;//0x80+0x10;//0x00
			break;
		case LCD_ROTATE_180:
			data = 0x00+0x10;//0x40+0x10;
			break;
		case LCD_V_MIRROR:
			data = 0x40+0x10;//0x40//0x00+0x10;//0x80
			break;
	}
	st7701s_write_command(0x36);	  st7701s_write_data(data);//(data); //v
	return 0;
}

static struct lcd_map_list st7701s_map = {
	.map = {
		.lcd_init = st7701s_display_init,
		.lcd_rorate = st7701s_rorate,
		.name = "lcd-st7701s",
	}
};

static int st7701s_probe(const char *node)
{
	int np = fdt_node_probe_by_path(node);

	if(np < 0){
		goto error;
	}

	memset(&st7701sdev,0,sizeof(struct st7701s_dev));

	st7701sdev.spi_clk_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.sck;
	st7701sdev.spi_clk_vaild_edge=1;
	st7701sdev.spi_cs_polar = 0;
	st7701sdev.spi_is_9bit = 1;
	st7701sdev.spi_cs_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.cs;
	st7701sdev.spi_mosi_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.mosi;
	st7701sdev.spi_miso_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.miso;
	st7701sdev.lcd_stbyb_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.stbyb;
	st7701sdev.lcd_stbyb_polar = 0;
	st7701sdev.lcd_reset_num = PINPAD_INVALID;


	fdt_get_property_u_32_index(np, "reset", 			0, &st7701sdev.lcd_reset_num);
	fdt_get_property_u_32_index(np, "spi-gpio-sck", 	0, &st7701sdev.spi_clk_num);
	fdt_get_property_u_32_index(np, "spi-gpio-mosi", 	0, &st7701sdev.spi_mosi_num);
	fdt_get_property_u_32_index(np, "spi-gpio-miso", 	0, &st7701sdev.spi_miso_num);
	fdt_get_property_u_32_index(np, "spi-gpio-cs", 		0, &st7701sdev.spi_cs_num);
	fdt_get_property_u_32_index(np, "spi-gpio-stbyb", 	0, &st7701sdev.lcd_stbyb_num);

#if 0
	st7701sdev.spi_clk_num=PINPAD_L20;
	st7701sdev.spi_clk_vaild_edge=1;
	st7701sdev.spi_cs_polar=0;
	st7701sdev.spi_is_9bit=1;
	st7701sdev.spi_cs_num=PINPAD_L19;
	st7701sdev.spi_mosi_num=PINPAD_L21;
	st7701sdev.lcd_reset_num=PINPAD_L00;
	st7701sdev.lcd_reset_polar=0;
#endif

	st7701sdev.spi_fd = -1;
	fdt_get_property_string_index(np, "devpath", 0, &st7701sdev.spi_devpath);
	if(st7701sdev.spi_devpath != NULL)
	{
		st7701sdev.spi_fd = open(st7701sdev.spi_devpath, O_RDWR);
		if(st7701sdev.spi_fd < 0)
		{
			printf("st7701sdev.spi_fd open error devpath = %s\n", st7701sdev.spi_devpath);
			return 0;
		}
		u32 mode = SPI_MODE_0;
		if (ioctl(st7701sdev.spi_fd, SPI_IOC_WR_MODE32, &mode) == -1) {
			printf("can't set spi mode");
			return 0;
		}
		close(st7701sdev.spi_fd);
	}

	int default_off = fdt_property_read_bool(np, "default-off");
	if(default_off ==0)
		st7701s_display_init();

	printf("st7701sdev.lcd_stbyb_num = %d %d %d %d %d default_off = %d\n",st7701sdev.spi_clk_num,st7701sdev.spi_mosi_num,st7701sdev.spi_cs_num,st7701sdev.lcd_stbyb_num,st7701sdev.lcd_reset_num,default_off);
	st7701s_map.map.default_off_val = default_off;

	lcd_map_register(&st7701s_map);
error:
	return 0;
}

static int st7701s_init(void)
{
	st7701s_probe("/hcrtos/lcd-st7701s");
	return 0;
}

module_driver(st7701s, st7701s_init, NULL, 2)
