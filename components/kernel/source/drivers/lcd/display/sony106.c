#define LOG_TAG "lcd_sony106"
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
#include "../lcd_main.h"

/*
*	TIME: 2023 08 15
*	xb 480*800
	lcd-sony106{
		spi-gpio-sck	= <PINPAD_L21>;
		spi-gpio-mosi	= <PINPAD_L28>;
		spi-gpio-cs		= <PINPAD_L29>;
		reset = <PINPAD_L20>;
		power-gpio-rtos = <PINPAD_B02>;
		default-off;
		status = "okay";
	};
	lcd{
		lcd-map-name = "lcd-sony106";
		default-off;
		status = "okay";
	};
*/

static void lcd_reset(void);

typedef struct sony106_dev{
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
	u32 power_on;
}sony106_dev_t;
static sony106_dev_t sony106dev;

static void gpio_spi_set_mosi(unsigned char data)
{
    lcd_gpio_set_output(sony106dev.spi_mosi_num,(bool)data);
}

static void gpio_spi_generate_clk(void)
{   
    if(sony106dev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(sony106dev.spi_clk_num,1);
	else
		lcd_gpio_set_output(sony106dev.spi_clk_num,0);
	usleep(2);
	if(sony106dev.spi_clk_vaild_edge == 1)//1 
		lcd_gpio_set_output(sony106dev.spi_clk_num,0);
	else
		lcd_gpio_set_output(sony106dev.spi_clk_num,1);
}
static void gpio_spi_enable_cs(void)//==0
{
	if(sony106dev.spi_cs_polar == 0)	
		lcd_gpio_set_output(sony106dev.spi_cs_num,0);
	else
		lcd_gpio_set_output(sony106dev.spi_cs_num,1);
}

static void gpio_spi_disable_cs(void)
{
	if(sony106dev.spi_cs_polar == 0)
		lcd_gpio_set_output(sony106dev.spi_cs_num,1);//cs
	else
		lcd_gpio_set_output(sony106dev.spi_cs_num,0);//cs
}

static void gpio_spi_init_clk(void)
{
	if(sony106dev.spi_clk_vaild_edge == 1)//1 //==1
		lcd_gpio_set_output(sony106dev.spi_clk_num,0);//sck
	else
		lcd_gpio_set_output(sony106dev.spi_clk_num,1);//sck
		
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
	if(sony106dev.spi_is_9bit == 1)
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

static void sony106_write_data(unsigned char data)
{
	lcd_gpio_spi_config_write(1,data);
}

static void sony106_spi_sends_data(unsigned char *data,unsigned char len)
{
	int i=len;
	do{
		sony106_write_data(*data++);
	}
	while(i--);
}
static void sony106_write_command(unsigned short cmds)
{
	lcd_gpio_spi_config_write(0,(unsigned char)cmds);
}

static int sony106_rorate(lcd_rotate_type_e dir)
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
	sony106_write_command(0x36);	  sony106_write_data(data);//(data); //v
	return 0;
}


static void lcd_reset(void)
{
	if(sony106dev.lcd_reset_num!=PINPAD_INVALID)
	{
		lcd_gpio_set_output(sony106dev.lcd_reset_num,!sony106dev.lcd_reset_polar);
		usleep(500*1000);
		lcd_gpio_set_output(sony106dev.lcd_reset_num,sony106dev.lcd_reset_polar);
		usleep(500*1000);
		lcd_gpio_set_output(sony106dev.lcd_reset_num,!sony106dev.lcd_reset_polar);
		usleep(500*1000);
	}
}

static int sony106_gpio_power_on(u32 val)
{
	if(val == 0)
	{
		gpio_configure(sony106dev.power_on,GPIO_DIR_OUTPUT);//miso
		lcd_gpio_set_output(sony106dev.power_on,0);
	}
	else
	{
		gpio_configure(sony106dev.power_on,GPIO_DIR_OUTPUT);//miso
		lcd_gpio_set_output(sony106dev.power_on,1);
	}
}

static int sony106_display_init(void)
{
	gpio_configure(sony106dev.spi_clk_num,GPIO_DIR_OUTPUT);//clk
	gpio_configure(sony106dev.spi_cs_num,GPIO_DIR_OUTPUT);//mosi
	gpio_configure(sony106dev.spi_mosi_num,GPIO_DIR_OUTPUT);//cs
	gpio_configure(sony106dev.lcd_reset_num,GPIO_DIR_OUTPUT);//reset
	sony106_gpio_power_on(1);
	// gpio_configure(PINPAD_R05,GPIO_DIR_OUTPUT);//
	// gpio_set_output(PINPAD_R05,true);

	printf("%s %d\n", __FUNCTION__,__LINE__);

	lcd_reset();

	sony106_write_command(0xB9);
	sony106_write_data(0xFF);
	sony106_write_data(0x83);
	sony106_write_data(0x69);

	sony106_write_command(0xB1);
	sony106_write_data(0x0C);
	sony106_write_data(0x83);
	sony106_write_data(0x77);	
	sony106_write_data(0x00);
	sony106_write_data(0x0F);
	sony106_write_data(0x0F);
	sony106_write_data(0x18);
	sony106_write_data(0x18);
	sony106_write_data(0x0C);
	sony106_write_data(0x0A);

	sony106_write_command(0xB2);
	sony106_write_data(0x00);
	sony106_write_data(0x2a);	//0x20

	sony106_write_command(0xB3);
	sony106_write_data(0x03);	//0x83	///3:DPL 2:HSPL 1:VSPL 0:EPL
	sony106_write_data(0x00);
	sony106_write_data(0x31);	
	sony106_write_data(0x03);

	sony106_write_command(0xB4);	///Set display waveform cycles
	sony106_write_data(0x05);	///00

	sony106_write_command(0xB6 );	///vcom
	sony106_write_data(0x5a );	//0xA0
	sony106_write_data(0x5a);	//0xA0

	sony106_write_command(0xCB);
	sony106_write_data(0x6D);

	sony106_write_command(0xC6);
	sony106_write_data(0x41);
	sony106_write_data(0xFF);
	sony106_write_data(0x7A);	

	sony106_write_command(0xEA);
	sony106_write_data(0x72);

	sony106_write_command(0xE3);
	sony106_write_data(0x07);
	sony106_write_data(0x0F);
	sony106_write_data(0x07);	
	sony106_write_data(0x0F);

	sony106_write_command(0xC0);
	sony106_write_data(0x73);
	sony106_write_data(0x50);
	sony106_write_data(0x00);	
	sony106_write_data(0x34);
	sony106_write_data(0xC4);
	sony106_write_data(0x09);

	sony106_write_command(0xC1 );
	sony106_write_data(0x00 );

	sony106_write_command(0xD5);
	sony106_write_data(0x00);
	sony106_write_data(0x00);
	sony106_write_data(0x08);	
	sony106_write_data(0x00);
	sony106_write_data(0x0A);
	sony106_write_data(0x00);
	sony106_write_data(0x00);	  
	sony106_write_data(0x10);	  
	sony106_write_data(0x01);	  
	sony106_write_data(0x00);	
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x01);	  
	sony106_write_data(0x49);	  
	sony106_write_data(0x37);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	
	sony106_write_data(0x0A);	  
	sony106_write_data(0x0A);	  
	sony106_write_data(0x0B);	  
	sony106_write_data(0x47);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	
	sony106_write_data(0x60);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	
	sony106_write_data(0x00);	  
	sony106_write_data(0x03);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x26);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	
	sony106_write_data(0x91);	  
	sony106_write_data(0x13);	  
	sony106_write_data(0x35);	  
	sony106_write_data(0x57);	  
	sony106_write_data(0x75);	  
	sony106_write_data(0x18);	  
	sony106_write_data(0x00);	
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x86);	  
	sony106_write_data(0x64);	  
	sony106_write_data(0x42);	  
	sony106_write_data(0x20);	
	sony106_write_data(0x00);	  
	sony106_write_data(0x49);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x90);	  
	sony106_write_data(0x02);	
	sony106_write_data(0x24);	  
	sony106_write_data(0x46);	  
	sony106_write_data(0x64);	  
	sony106_write_data(0x08);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	
	sony106_write_data(0x00);	  
	sony106_write_data(0x87);	  
	sony106_write_data(0x75);	  
	sony106_write_data(0x53);	  
	sony106_write_data(0x31);	  
	sony106_write_data(0x11);	  
	sony106_write_data(0x59);	
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x01);	  
	sony106_write_data(0x00);	
	sony106_write_data(0x00);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x0F);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x0F);	  
	sony106_write_data(0xFF);	  
	sony106_write_data(0xFF);	
	sony106_write_data(0x0F);	  
	sony106_write_data(0x00);	  
	sony106_write_data(0x0F);	  
	sony106_write_data(0xFF);	  
	sony106_write_data(0xFF);	  
	sony106_write_data(0x00);	
	sony106_write_data(0x80);	  
	sony106_write_data(0x5A);	  

	sony106_write_command(0xE0); 
	sony106_write_data(0x00); 
	sony106_write_data(0x07); 
	sony106_write_data(0x0C);	
	sony106_write_data(0x30); 
	sony106_write_data(0x32); 
	sony106_write_data(0x3F); 
	sony106_write_data(0x1C); 
	sony106_write_data(0x3A); 
	sony106_write_data(0x08); 
	sony106_write_data(0x0D);	
	sony106_write_data(0x10); 
	sony106_write_data(0x14); 
	sony106_write_data(0x16); 
	sony106_write_data(0x14); 
	sony106_write_data(0x15); 
	sony106_write_data(0x0E); 
	sony106_write_data(0x12);	
	sony106_write_data(0x00); 
	sony106_write_data(0x07); 
	sony106_write_data(0x0C); 
	sony106_write_data(0x30); 
	sony106_write_data(0x32); 
	sony106_write_data(0x3F); 
	sony106_write_data(0x1C);	
	sony106_write_data(0x3A); 
	sony106_write_data(0x08); 
	sony106_write_data(0x0D); 
	sony106_write_data(0x10); 
	sony106_write_data(0x14); 
	sony106_write_data(0x16); 
	sony106_write_data(0x14);	
	sony106_write_data(0x15); 
	sony106_write_data(0x0E); 
	sony106_write_data(0x12); 
	sony106_write_data(0x01); 

	sony106_write_command(0x36); 
	sony106_write_data(0x00); 	//0x20=mv bit3 rgb/rgb 

	sony106_write_command(0x3A); 
	sony106_write_data(0x66); 

//		sony106_write_command(0xCC); //Display direction 
//		sony106_write_data(0x0e);	//0x02 ??????? ??0x0E ???3???

	sony106_write_command(0x11);
	usleep(120*1000);

	//DISP ON
	sony106_write_command(0x29);

	usleep(20*1000);
}


static struct lcd_map_list sony106_map = {
	.map = {
		.lcd_init = sony106_display_init,
		.lcd_power_onoff = sony106_gpio_power_on,
		.name = "lcd-sony106",
	}
};

static int sony106_probe(const char *node)
{
	int np = fdt_node_probe_by_path(node);

	if(np < 0){
		goto error;
	}

	memset(&sony106dev,0,sizeof(struct sony106_dev));

	sony106dev.spi_clk_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.sck;
	sony106dev.spi_clk_vaild_edge=1;
	sony106dev.spi_cs_polar=0;
	sony106dev.spi_is_9bit=1;
	sony106dev.spi_cs_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.cs;
	sony106dev.spi_mosi_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.mosi;
	sony106dev.spi_miso_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.miso;
	sony106dev.lcd_stbyb_num = PINPAD_INVALID;//lcddrv->gpio_spi_config.stbyb;
	sony106dev.lcd_stbyb_polar = 0;
	sony106dev.lcd_reset_num = PINPAD_INVALID;
	sony106dev.power_on = PINPAD_INVALID;
#if 0
	sony106dev.spi_clk_num=PINPAD_L21;
	sony106dev.spi_clk_vaild_edge=1;
	sony106dev.spi_cs_polar=0;
	sony106dev.spi_is_9bit=1;
	sony106dev.spi_cs_num=PINPAD_L29;
	sony106dev.spi_mosi_num=PINPAD_L28;
	sony106dev.lcd_reset_num=PINPAD_L20;
	sony106dev.lcd_reset_polar=0;
#endif

	fdt_get_property_u_32_index(np, "reset", 			0, &sony106dev.lcd_reset_num);
	fdt_get_property_u_32_index(np, "spi-gpio-sck", 	0, &sony106dev.spi_clk_num);
	fdt_get_property_u_32_index(np, "spi-gpio-mosi", 	0, &sony106dev.spi_mosi_num);
	fdt_get_property_u_32_index(np, "spi-gpio-miso", 	0, &sony106dev.spi_miso_num);
	fdt_get_property_u_32_index(np, "spi-gpio-cs", 		0, &sony106dev.spi_cs_num);
	fdt_get_property_u_32_index(np, "spi-gpio-stbyb", 	0, &sony106dev.lcd_stbyb_num);
	fdt_get_property_u_32_index(np, "power-gpio-rtos", 	0, &sony106dev.power_on);
	log_d("sony106dev.lcd_stbyb_num = %d %d %d %d\n",sony106dev.spi_clk_num,sony106dev.spi_mosi_num,sony106dev.spi_cs_num,sony106dev.lcd_stbyb_num);

	int default_off = fdt_property_read_bool(np, "default-off");
	if(default_off ==0)
		sony106_display_init();

	sony106_map.map.default_off_val = default_off;

	lcd_map_register(&sony106_map);
error:
	return 0;
}

static int sony106_init(void)
{
	sony106_probe("/hcrtos/lcd-sony106");
	return 0;
}

module_driver(sony106, sony106_init, NULL, 2)
