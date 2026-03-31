#define LOG_TAG "lcd_st7265"
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
#include <hcuapi/lvds.h>
#include "../lcd_main.h"
/*
*	TIME: 2022 11 12
*	support: TPR W2 BOARD
*	PINPAD_LVDS_DN5 clk
*	PINPAD_LVDS_DP5 mosi
*	PINPAD_LVDS_DP4 cs

	lcd-st7265{
		spi-gpio-sck	= <PINPAD_LVDS_DN5>;
		spi-gpio-mosi	= <PINPAD_LVDS_DP5>;
		spi-gpio-cs		= <PINPAD_LVDS_DP4>;
		default-off;
		status = "okay";
	};
	lcd{
		lcd-map-name = "lcd-st7265";
		default-off;
		status = "okay";
	};
*/


typedef struct st7265_dev{
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
}st7265_dev_t;

static st7265_dev_t st7265dev;

static void gpio_spi_set_mosi(unsigned char data)
{
    lcd_gpio_set_output(st7265dev.spi_mosi_num,(bool)data);
}

static void gpio_spi_generate_clk(void)
{   
    if(st7265dev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(st7265dev.spi_clk_num,1);
	else
		lcd_gpio_set_output(st7265dev.spi_clk_num,0);
	usleep(2);
	if(st7265dev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(st7265dev.spi_clk_num,0);
	else
		lcd_gpio_set_output(st7265dev.spi_clk_num,1);
}
static void gpio_spi_enable_cs(void)//==0
{
	if(st7265dev.spi_cs_polar == 0)	
		lcd_gpio_set_output(st7265dev.spi_cs_num,0);
	else
		lcd_gpio_set_output(st7265dev.spi_cs_num,1);
	// printf("%s %d val = %08lx\n",__FUNCTION__,__LINE__,GET_DWORD(0xb8800054));
}

static void gpio_spi_disable_cs(void)
{
	if(st7265dev.spi_cs_polar == 0)
		lcd_gpio_set_output(st7265dev.spi_cs_num,1);//cs
	else
		lcd_gpio_set_output(st7265dev.spi_cs_num,0);//cs
	// printf("%s %d val = %08lx\n",__FUNCTION__,__LINE__,GET_DWORD(0xb8800054));
}

static void gpio_spi_init_clk(void)
{
	if(st7265dev.spi_clk_vaild_edge == 1)//1 //==1
		lcd_gpio_set_output(st7265dev.spi_clk_num,0);//sck
	else
		lcd_gpio_set_output(st7265dev.spi_clk_num,1);//sck
		
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

static void st7265_send_cmds_data(unsigned short val)
{
	lcd_gpio_spi_config_write_16bit(val);
}

static void lcd_reset(void)
{
	if (st7265dev.lcd_reset_num != PINPAD_INVALID) {
		lcd_gpio_set_output(st7265dev.lcd_reset_num, 1);
		usleep(500 * 1000);
		lcd_gpio_set_output(st7265dev.lcd_reset_num, 0);
		usleep(500 * 1000);
		lcd_gpio_set_output(st7265dev.lcd_reset_num, 1);
		usleep(500 * 1000);
	}
}

static int st7265_display_init(void)
{
	gpio_configure(st7265dev.spi_clk_num,GPIO_DIR_OUTPUT);//clk
	gpio_configure(st7265dev.spi_mosi_num,GPIO_DIR_OUTPUT);//mosi
	gpio_configure(st7265dev.spi_cs_num,GPIO_DIR_OUTPUT);//cs
	gpio_configure(st7265dev.lcd_stbyb_num,GPIO_DIR_OUTPUT);//STBYB
	gpio_configure(st7265dev.spi_miso_num,GPIO_DIR_OUTPUT);//miso
	lcd_reset();

    printf("%s %d lcd init\n",__func__,__LINE__);
	st7265_send_cmds_data(0x7F81);
	st7265_send_cmds_data(0x6531);
	st7265_send_cmds_data(0x663F);
	st7265_send_cmds_data(0x7F00);
	st7265_send_cmds_data(0x1C08);
	st7265_send_cmds_data(0x4013);
	st7265_send_cmds_data(0x4110);
	st7265_send_cmds_data(0x432C);
	st7265_send_cmds_data(0x45B5);
	st7265_send_cmds_data(0x464D);
	st7265_send_cmds_data(0x4722);
	st7265_send_cmds_data(0x2066);
	st7265_send_cmds_data(0x2129);
	st7265_send_cmds_data(0x22AB);
	st7265_send_cmds_data(0x23B6);
	st7265_send_cmds_data(0x24C8);
	st7265_send_cmds_data(0x25AA);
	st7265_send_cmds_data(0x26A3);
	st7265_send_cmds_data(0x2760);
	st7265_send_cmds_data(0x2888);
	st7265_send_cmds_data(0x2919);
	st7265_send_cmds_data(0x3006);
	st7265_send_cmds_data(0x3129);
	st7265_send_cmds_data(0x32AB);
	st7265_send_cmds_data(0x33B6);
	st7265_send_cmds_data(0x34C8);
	st7265_send_cmds_data(0x35AA);
	st7265_send_cmds_data(0x36A3);
	st7265_send_cmds_data(0x3760);
	st7265_send_cmds_data(0x3888);
	st7265_send_cmds_data(0x3919);
	st7265_send_cmds_data(0x7F11);
	st7265_send_cmds_data(0x9D50);
	st7265_send_cmds_data(0x9E8F);

}

static struct lcd_map_list st7265_map = {
	.map = {
		.lcd_init = st7265_display_init,
		.name = "lcd-st7265",
	}
};

static int st7265_probe(const char *node)
{
	int np = fdt_node_probe_by_path(node);

	if(np < 0){
		goto error;
	}

	memset(&st7265dev,0,sizeof(struct st7265_dev));

	st7265dev.spi_clk_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.sck;
	st7265dev.spi_clk_vaild_edge=1;
	st7265dev.spi_cs_polar=0;
	st7265dev.spi_is_9bit=1;
	st7265dev.spi_cs_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.cs;
	st7265dev.spi_mosi_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.mosi;
	st7265dev.spi_miso_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.miso;
	st7265dev.lcd_stbyb_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.stbyb;
	st7265dev.lcd_stbyb_polar = 0;
	st7265dev.lcd_reset_num = PINPAD_INVALID;

	fdt_get_property_u_32_index(np, "reset", 			0, &st7265dev.lcd_reset_num);
	fdt_get_property_u_32_index(np, "spi-gpio-sck", 	0, &st7265dev.spi_clk_num);
	fdt_get_property_u_32_index(np, "spi-gpio-mosi", 	0, &st7265dev.spi_mosi_num);
	fdt_get_property_u_32_index(np, "spi-gpio-miso", 	0, &st7265dev.spi_miso_num);
	fdt_get_property_u_32_index(np, "spi-gpio-cs", 		0, &st7265dev.spi_cs_num);
	fdt_get_property_u_32_index(np, "spi-gpio-stbyb", 	0, &st7265dev.lcd_stbyb_num);
	log_d("st7265dev.lcd_stbyb_num = %d %d %d %d\n",st7265dev.spi_clk_num,st7265dev.spi_mosi_num,st7265dev.spi_cs_num,st7265dev.lcd_stbyb_num);

#if 0
// gpio_configure(PINPAD_T23,GPIO_DIR_OUTPUT);//reset
	st7265dev.spi_clk_num = PINPAD_LVDS_DN5;
	st7265dev.spi_clk_vaild_edge = 1;
	st7265dev.spi_cs_polar = 0;
	st7265dev.spi_is_9bit = 1;
	st7265dev.spi_cs_num = PINPAD_LVDS_DP4;
	st7265dev.spi_mosi_num = PINPAD_LVDS_DP5;
	st7265dev.lcd_reset_num = PINPAD_INVALID;
	st7265dev.lcd_reset_polar = 0;
#endif

	int default_off = fdt_property_read_bool(np, "default-off");
	if(default_off ==0)
		st7265_display_init();

	st7265_map.map.default_off_val = default_off;
	lcd_map_register(&st7265_map);
error:
	return 0;
}

static int st7265_init(void)
{
	st7265_probe("/hcrtos/lcd-st7265");
	return 0;
}

module_driver(st7265, st7265_init, NULL, 2)

