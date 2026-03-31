#define LOG_TAG "lcd_jd9168v2"
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
#include "../lcd_main.h"
#include <hcuapi/lvds.h>
/*
*	TIME: 2022 11 12
*	support: TPR W2 BOARD
*	PINPAD_T01 clk
*	PINPAD_T02 mosi
*	PINPAD_T00 cs
*	PINPAD_T03 STBYB
*	PINPAD_T04 backlight

	lcd-jd9168v2{
		spi-gpio-sck	= <PINPAD_T01>;
		spi-gpio-mosi	= <PINPAD_T02>;
		spi-gpio-cs		= <PINPAD_T00>;
		spi-gpio-stbyb	= <PINPAD_T03>;
		default-off;
		status = "okay";
	};
	lcd{
		lcd-map-name = "lcd-jd9168v2";
		default-off;
		status = "okay";
	};
*/

typedef struct jd9168v2_dev{
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
}jd9168v2_dev_t;


#define SET_DWORD(i, d)         (*(volatile unsigned long *)(i)) = (d)
#define GET_DWORD(i)            (*(volatile unsigned long *)(i))
#define LVDS_GPIO_SUPPORT 0

static void lcd_stbyb_start(void);
static void lcd_stbyb_normal(void);
static void jd9168v2_write_data(unsigned short cmds);
void LCD_SCS_CLEAR(void);

static jd9168v2_dev_t jd9168v2dev;

static void gpio_spi_set_mosi(unsigned char data)
{
    lcd_gpio_set_output(jd9168v2dev.spi_mosi_num,(bool)data);
}

static void gpio_spi_generate_clk(void)
{   
    if(jd9168v2dev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(jd9168v2dev.spi_clk_num,1);
	else
		lcd_gpio_set_output(jd9168v2dev.spi_clk_num,0);
	usleep(2);
	if(jd9168v2dev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(jd9168v2dev.spi_clk_num,0);
	else
		lcd_gpio_set_output(jd9168v2dev.spi_clk_num,1);
}
static void gpio_spi_enable_cs(void)//==0
{
	if(jd9168v2dev.spi_cs_polar == 0)	
		lcd_gpio_set_output(jd9168v2dev.spi_cs_num,0);
	else
		lcd_gpio_set_output(jd9168v2dev.spi_cs_num,1);
}

static void gpio_spi_disable_cs(void)
{
	if(jd9168v2dev.spi_cs_polar == 0)
		lcd_gpio_set_output(jd9168v2dev.spi_cs_num,1);//cs
	else
		lcd_gpio_set_output(jd9168v2dev.spi_cs_num,0);//cs
}

static void gpio_spi_init_clk(void)
{
	if(jd9168v2dev.spi_clk_vaild_edge == 1)//1 //==1
		lcd_gpio_set_output(jd9168v2dev.spi_clk_num,0);//sck
	else
		lcd_gpio_set_output(jd9168v2dev.spi_clk_num,1);//sck
		
}
#if 0
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
#endif
static void lcd_gpio_spi_config_write_16bit(unsigned short cmd)
{
	unsigned short i = 0;
	gpio_spi_enable_cs();
	usleep(2);
	for(i = 0; i< 16; i++)
	{
		if(cmd&0x8000)
			gpio_spi_set_mosi(1);
		else
			gpio_spi_set_mosi(0);
		cmd <<= 1;
		usleep(22);
		lcd_gpio_set_output(jd9168v2dev.spi_clk_num,0);
		usleep(22);
		lcd_gpio_set_output(jd9168v2dev.spi_clk_num,1);
	}
	usleep(2);
	gpio_spi_disable_cs();
	usleep(2);
}

static void jd9168v2_write_data(unsigned short cmds)
{
	lcd_gpio_spi_config_write_16bit(cmds);
}

void LCD_SCS_CLEAR(void)
{
	usleep(2);
	gpio_spi_disable_cs();
	gpio_spi_set_mosi(0);
	usleep(10);

	gpio_spi_disable_cs();//cs
	usleep(10);
	gpio_spi_init_clk();//sck
	gpio_spi_enable_cs();
	
}

static void lcd_stbyb_start(void)
{
	lcd_gpio_set_output(jd9168v2dev.lcd_stbyb_num,!jd9168v2dev.lcd_stbyb_polar);
	usleep(200*500);
	lcd_gpio_set_output(jd9168v2dev.lcd_stbyb_num,jd9168v2dev.lcd_stbyb_polar);
	usleep(200*500);
}

static void lcd_stbyb_normal(void)
{
	usleep(10*1000);
	lcd_gpio_set_output(jd9168v2dev.lcd_stbyb_num,!jd9168v2dev.lcd_stbyb_polar);
}

static void lcd_reset(void)
{
	if(jd9168v2dev.lcd_reset_num!=PINPAD_INVALID)
	{
		lcd_gpio_set_output(jd9168v2dev.lcd_reset_num,!jd9168v2dev.lcd_reset_polar);
		usleep(500*1000);
		lcd_gpio_set_output(jd9168v2dev.lcd_reset_num,jd9168v2dev.lcd_reset_polar);
		usleep(500*1000);
		lcd_gpio_set_output(jd9168v2dev.lcd_reset_num,!jd9168v2dev.lcd_reset_polar);
		usleep(500*1000);
	}
}

static int jd9168v2_display_init(void)
{
	// gpio_configure(PINPAD_T04,GPIO_DIR_OUTPUT);//backlight
	// lcd_gpio_set_output(PINPAD_T04,1);
	printf("%s %d \n",__func__,__LINE__);
	gpio_configure(jd9168v2dev.spi_clk_num,GPIO_DIR_OUTPUT);//clk
	gpio_configure(jd9168v2dev.spi_mosi_num,GPIO_DIR_OUTPUT);//mosi
	gpio_configure(jd9168v2dev.spi_cs_num,GPIO_DIR_OUTPUT);//cs
	gpio_configure(jd9168v2dev.lcd_stbyb_num,GPIO_DIR_OUTPUT);//STBYB
	gpio_configure(jd9168v2dev.spi_miso_num,GPIO_DIR_OUTPUT);//miso
	lcd_gpio_set_output(jd9168v2dev.spi_cs_num,1);//cs
	lcd_gpio_set_output(jd9168v2dev.spi_clk_num,1);//clk
	lcd_gpio_set_output(jd9168v2dev.spi_mosi_num,1);//mosi

	lcd_reset();

	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x03DF);
	jd9168v2_write_data(0x9168);
	jd9168v2_write_data(0xF9F1);
	jd9168v2_write_data(0x0101);
	
	jd9168v2_write_data(0xDE00);
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x02B2);
	jd9168v2_write_data(0x0091);	
	jd9168v2_write_data(0xF101);
	
	jd9168v2_write_data(0x02B3);
	jd9168v2_write_data(0x0091);	
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x02C1);
	jd9168v2_write_data(0x0014); 
	
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x07BB);
	jd9168v2_write_data(0x0015);
	jd9168v2_write_data(0x0019);
	jd9168v2_write_data(0x2944);
	
	jd9168v2_write_data(0x44F1);
	jd9168v2_write_data(0x0102);
	jd9168v2_write_data(0xBE1A);
	jd9168v2_write_data(0xF1F1);
	jd9168v2_write_data(0x0114);
	
	jd9168v2_write_data(0xC310);
	jd9168v2_write_data(0x2378);
	jd9168v2_write_data(0x2378);
	jd9168v2_write_data(0x0505);
	jd9168v2_write_data(0x0505);
	
	jd9168v2_write_data(0x1515);
	jd9168v2_write_data(0x3185);
	jd9168v2_write_data(0x1C05);
	jd9168v2_write_data(0x6C0A);
	jd9168v2_write_data(0x100A);
	
	jd9168v2_write_data(0x10F1);
	jd9168v2_write_data(0x0107);
	jd9168v2_write_data(0xC413);
	jd9168v2_write_data(0x2C00);
	jd9168v2_write_data(0x1B0A);
	
	jd9168v2_write_data(0x0714);	
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x17CE);
	jd9168v2_write_data(0x0003);
	jd9168v2_write_data(0x0303);
	
	jd9168v2_write_data(0x0303);
	jd9168v2_write_data(0x0303);
	jd9168v2_write_data(0x0303);
	jd9168v2_write_data(0x0303);
	jd9168v2_write_data(0x0F0F);
	
	jd9168v2_write_data(0x0303);
	jd9168v2_write_data(0x0303);
	jd9168v2_write_data(0x0303);
	jd9168v2_write_data(0x030F);
	jd9168v2_write_data(0x0FF1);
	
	jd9168v2_write_data(0x010D);
	jd9168v2_write_data(0xCF00);
	jd9168v2_write_data(0x0047);
	jd9168v2_write_data(0x017C);
	jd9168v2_write_data(0x017C);
	
	jd9168v2_write_data(0x2FFF);
	jd9168v2_write_data(0x067C);
	jd9168v2_write_data(0x0000); 
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x17D0);
	
	jd9168v2_write_data(0x001F);
	jd9168v2_write_data(0x0200);
	jd9168v2_write_data(0x0808);
	jd9168v2_write_data(0x0A0A);
	jd9168v2_write_data(0x0404);
	
	jd9168v2_write_data(0x0606);
	jd9168v2_write_data(0x1717);
	jd9168v2_write_data(0x1717);
	jd9168v2_write_data(0x1F1F);
	jd9168v2_write_data(0x151E);
	
	jd9168v2_write_data(0x1E1F);
	jd9168v2_write_data(0x1FF1);
	jd9168v2_write_data(0x0117);
	jd9168v2_write_data(0xD100);
	jd9168v2_write_data(0x1F03);
	
	jd9168v2_write_data(0x0109);
	jd9168v2_write_data(0x090B);
	jd9168v2_write_data(0x0B05);
	jd9168v2_write_data(0x0507);
	jd9168v2_write_data(0x0717);
	
	jd9168v2_write_data(0x1717);
	jd9168v2_write_data(0x171F);
	jd9168v2_write_data(0x1F15);
	jd9168v2_write_data(0x1E1E);
	jd9168v2_write_data(0x1F1F); 
	
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x17D2);
	jd9168v2_write_data(0x001F);
	jd9168v2_write_data(0x0103);
	jd9168v2_write_data(0x0707);
	
	jd9168v2_write_data(0x0505);
	jd9168v2_write_data(0x0B0B);
	jd9168v2_write_data(0x0909);
	jd9168v2_write_data(0x1717);
	jd9168v2_write_data(0x1717);
	
	jd9168v2_write_data(0x1F1F);
	jd9168v2_write_data(0x151F);
	jd9168v2_write_data(0x1F1E);
	jd9168v2_write_data(0x1EF1);
	jd9168v2_write_data(0x0117);
	
	jd9168v2_write_data(0xD300);
	jd9168v2_write_data(0x1F00);
	jd9168v2_write_data(0x0206);
	jd9168v2_write_data(0x0604);
	jd9168v2_write_data(0x040A);
	
	jd9168v2_write_data(0x0A08);
	jd9168v2_write_data(0x0817);
	jd9168v2_write_data(0x1717);
	jd9168v2_write_data(0x171F);
	jd9168v2_write_data(0x1F15);
	
	jd9168v2_write_data(0x1F1F);
	jd9168v2_write_data(0x1E1E); 
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x2BD4);
	jd9168v2_write_data(0x3000);
	
	jd9168v2_write_data(0x0006);
	jd9168v2_write_data(0x0008);
	jd9168v2_write_data(0x0000);
	jd9168v2_write_data(0x0000);
	jd9168v2_write_data(0x0003);
	
	jd9168v2_write_data(0x0300);
	jd9168v2_write_data(0x0080);
	jd9168v2_write_data(0x06C0);
	jd9168v2_write_data(0x0803);
	jd9168v2_write_data(0x0311);
	
	jd9168v2_write_data(0x0000);
	jd9168v2_write_data(0x0000);
	jd9168v2_write_data(0x0000);
	jd9168v2_write_data(0x0001);
	jd9168v2_write_data(0x0305);
	
	jd9168v2_write_data(0x0000);
	jd9168v2_write_data(0x0304);
	jd9168v2_write_data(0xC704);
	jd9168v2_write_data(0xC300);
	jd9168v2_write_data(0x0000);
	
	jd9168v2_write_data(0x03F1);
	jd9168v2_write_data(0x0113);
	jd9168v2_write_data(0xD568);
	jd9168v2_write_data(0x7300);
	jd9168v2_write_data(0x0A08);
	
	jd9168v2_write_data(0x0003);
	jd9168v2_write_data(0x0004);
	jd9168v2_write_data(0x1003);
	jd9168v2_write_data(0x0402);
	jd9168v2_write_data(0xD31C);
	
	jd9168v2_write_data(0xB300);
	jd9168v2_write_data(0x0000);
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x06B7);
	jd9168v2_write_data(0x101F);
	
	jd9168v2_write_data(0x0110);
	jd9168v2_write_data(0x1F01);	
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x26C8);
	jd9168v2_write_data(0x7C61);
	
	jd9168v2_write_data(0x4E3F);
	jd9168v2_write_data(0x3828);
	jd9168v2_write_data(0x2A13);
	jd9168v2_write_data(0x2B29);
	jd9168v2_write_data(0x2845);
	
	jd9168v2_write_data(0x333B);
	jd9168v2_write_data(0x302A);
	jd9168v2_write_data(0x1A0E);
	jd9168v2_write_data(0x067C);
	jd9168v2_write_data(0x614E);
	
	jd9168v2_write_data(0x3F38);
	jd9168v2_write_data(0x282A);
	jd9168v2_write_data(0x132B);
	jd9168v2_write_data(0x2928);
	jd9168v2_write_data(0x4533);
	
	jd9168v2_write_data(0x3B30);
	jd9168v2_write_data(0x2A1A);
	jd9168v2_write_data(0x0E06); 
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x01DE);
	
	jd9168v2_write_data(0x02F1);
	jd9168v2_write_data(0x0101);
	jd9168v2_write_data(0xC115);	
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x01E7);
	
	jd9168v2_write_data(0x01F1);
	jd9168v2_write_data(0x0101);
	jd9168v2_write_data(0xDE00);	  
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x0011);
	
	jd9168v2_write_data(0xF101);
	jd9168v2_write_data(0x0029);

	usleep(200*1000);
	return 0;
}

static struct lcd_map_list jd9168v2_map = {
	.map = {
		.lcd_init = jd9168v2_display_init,
		.name = "lcd-jd9168v2",
	}
};

static int jd9168v2_probe(const char *node)
{
	int np = fdt_node_probe_by_path(node);

	if(np < 0){
		goto error;
	}

	printf("%s %d \n",__func__,__LINE__);
	memset(&jd9168v2dev,0,sizeof(struct jd9168v2_dev));

	jd9168v2dev.spi_clk_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.sck;
	jd9168v2dev.spi_clk_vaild_edge=1;
	jd9168v2dev.spi_cs_polar=0;
	jd9168v2dev.spi_is_9bit=1;
	jd9168v2dev.spi_cs_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.cs;
	jd9168v2dev.spi_mosi_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.mosi;
	jd9168v2dev.spi_miso_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.miso;
	jd9168v2dev.lcd_stbyb_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.stbyb;
	jd9168v2dev.lcd_stbyb_polar = 1;

	#if 0
	jd9168v2dev.spi_clk_num=PINPAD_LVDS_DN5;
	jd9168v2dev.spi_clk_vaild_edge=1;
	jd9168v2dev.spi_cs_polar=0;
	jd9168v2dev.spi_is_9bit=1;
	jd9168v2dev.spi_cs_num=PINPAD_LVDS_DP4;
	jd9168v2dev.spi_mosi_num=PINPAD_LVDS_DP5;
	jd9168v2dev.spi_miso_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.miso;
	jd9168v2dev.lcd_stbyb_num=PINPAD_INVALID;

	#endif
#if 0
	jd9168v2dev.spi_clk_num=PINPAD_T01;
	jd9168v2dev.spi_clk_vaild_edge=1;
	jd9168v2dev.spi_cs_polar=0;
	jd9168v2dev.spi_is_9bit=1;
	jd9168v2dev.spi_cs_num=PINPAD_T00;
	jd9168v2dev.spi_mosi_num=PINPAD_T02;
	jd9168v2dev.lcd_stbyb_num=PINPAD_T03;
	jd9168v2dev.lcd_stbyb_polar=0;
#endif

	fdt_get_property_u_32_index(np, "reset", 			0, &jd9168v2dev.lcd_reset_num);
	fdt_get_property_u_32_index(np, "spi-gpio-sck", 	0, &jd9168v2dev.spi_clk_num);
	fdt_get_property_u_32_index(np, "spi-gpio-mosi", 	0, &jd9168v2dev.spi_mosi_num);
	fdt_get_property_u_32_index(np, "spi-gpio-miso", 	0, &jd9168v2dev.spi_miso_num);
	fdt_get_property_u_32_index(np, "spi-gpio-cs", 		0, &jd9168v2dev.spi_cs_num);
	fdt_get_property_u_32_index(np, "spi-gpio-stbyb", 	0, &jd9168v2dev.lcd_stbyb_num);
	log_d("jd9168v2dev.lcd_stbyb_num = %d %d %d %d\n",jd9168v2dev.spi_clk_num,jd9168v2dev.spi_mosi_num,jd9168v2dev.spi_cs_num,jd9168v2dev.lcd_stbyb_num);

	int default_off = 0;
	fdt_get_property_u_32_index(np, "default-off", 	0, &default_off);
	if(default_off ==0)
		jd9168v2_display_init();

	jd9168v2_map.map.default_off_val = default_off;
	lcd_map_register(&jd9168v2_map);
error:
	return 0;
}

static int jd9168v2_init(void)
{
	jd9168v2_probe("/hcrtos/lcd-jd9168v2");
	return 0;
}

module_driver(jd9168v2, jd9168v2_init, NULL, 2)
