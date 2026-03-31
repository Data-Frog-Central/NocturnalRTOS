#define LOG_TAG "lcd_dsi0"
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
#include <hcuapi/mipi.h>
#include "../lcd_main.h"
/*
*	TIME: 2023 06 17
	dsi0 {
		default-off;
		status = "okay";
	};
*/
#define MIPIDEV_PATH  "/dev/mipi"

static int dsi0_display_init(void)
{
	int fd = 0;
	fd = open(MIPIDEV_PATH, O_RDWR);
	printf("%s %d \n",__func__,__LINE__);
	if(fd)
	{
		struct mipi_display_timing timing={0};
		ioctl(fd,MIPI_DSI_GET_TIMING,&timing);
		timing.h_sync_level = 1;
		timing.v_sync_level = 0;
		ioctl(fd,MIPI_DSI_SET_TIMING,&timing);
		ioctl(fd, MIPI_DSI_INIT, NULL);
		close(fd);
	}
	else
		log_e("dsi0 init error");
	return 0;
}

// static struct lcd_map_list dsi0_map = { .map = {
// 						.lcd_init = dsi0_display_init,
// 						.name = "lcd-dsi0",
// 					} };

static int dsi0_probe(const char *node)
{
	int np = fdt_node_probe_by_path(node);

	if (np < 0) {
		goto error;
	}

	int default_off = fdt_property_read_bool(np, "default-off");

	if (default_off == 1) 
		dsi0_display_init();
	// dsi0_map.map.default_off_val = 0;
	// lcd_map_register(&dsi0_map);
error:
	return 0;
}

static int dsi0_init(void)
{
	dsi0_probe("/hcrtos/dsi0");
	return 0;
}

module_driver(dsi0, dsi0_init, NULL, 1)
