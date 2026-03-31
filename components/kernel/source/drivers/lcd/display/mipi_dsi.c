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
*	TIME: 2022 11 12
	dsi0 {
		default-off;
		status = "okay";
	};
*/
#define MIPIDEV_PATH  "/dev/mipi"

static int dsi0_display_init(void)
{
	int fb = 0;
	printf("%s %d \n",__func__,__LINE__);
	fb = open(MIPIDEV_PATH, O_RDWR);
	if (fb < 0) {
		log_e("open %s failed, ret=%d\n", MIPIDEV_PATH, fb);
		return -1;
	}
	ioctl(fb, MIPI_DSI_INIT, NULL);
	close(fb);
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
	// }
error:
	return 0;
}

static int dsi0_init(void)
{
	dsi0_probe("/hcrtos/dsi0");
	return 0;
}

module_driver(dsi0, dsi0_init, NULL, 1)
