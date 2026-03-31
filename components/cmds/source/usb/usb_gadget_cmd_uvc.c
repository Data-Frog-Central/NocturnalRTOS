#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <kernel/lib/console.h>
#include <linux/bitops.h>
#include <linux/bits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <nuttx/fs/dirent.h>
#include <nuttx/fs/fs.h>
#include <nuttx/mtd/mtd.h>
#include <string.h>
#include <kernel/elog.h>
#include <kernel/drivers/hcusb.h>

static void usbd_uvc_help_info(void)
{
	printf("usb device mode : uvc\n");
}

/* ---------------------  ---------------------  ---------------------  --------------------- */
/* ---------------------  DVP camera (BR3A03)    ---------------------  --------------------- */
/* ---------------------  ---------------------  ---------------------  --------------------- */

#if 0

#include <hcuapi/common.h>
#include <hcuapi/kshm.h>
#include <hcuapi/vindvp.h>
#include <hcuapi/tvtype.h>
#include <hcuapi/vidmp.h>
#include <linux/delay.h>

static bool g_is_camera_start = false;
struct kshm_info g_video_read_hdl = { 0 };
static uint8_t dvp_data[100 * 1024];
static int g_vin_dvp_fd = -1;

static int dvp_camera_switch(bool is_turn_on)
{
	enum VINDVP_VIDEO_DATA_PATH vpath = VINDVP_VIDEO_TO_KSHM;
	unsigned int combine_mode = VINDVP_COMBINED_MODE_DISABLE;
	enum VINDVP_BG_COLOR bg_color = VINDVP_BG_COLOR_BLACK;
	int stop_mode = VINDVP_BLACK_SRCREEN_ANYWAY;
	int port = 0;
	enum TVTYPE tv_sys = TV_NTSC;

	if (is_turn_on == true) {
        if(g_vin_dvp_fd >= 0)
            return 0;

		g_vin_dvp_fd = open("/dev/vindvp", O_WRONLY);
		if (g_vin_dvp_fd < 0) {
			printf("[Error] Cannot open /dev/vindvp\n");
			return -1;
		}

		// printf("vpath %d\n", vpath);
		// printf("combine_mode %d\n", combine_mode);
		ioctl(g_vin_dvp_fd, VINDVP_SET_VIDEO_DATA_PATH, vpath);
		ioctl(g_vin_dvp_fd, VINDVP_SET_COMBINED_MODE, combine_mode);
		ioctl(g_vin_dvp_fd, VINDVP_SET_BG_COLOR, bg_color);
		ioctl(g_vin_dvp_fd, VINDVP_SET_VIDEO_STOP_MODE, stop_mode);
		ioctl(g_vin_dvp_fd, VINDVP_SET_EXT_DEV_INPUT_PORT_NUM,
		      port); //FOR BR3A03
		ioctl(g_vin_dvp_fd, VINDVP_VIDEO_KSHM_ACCESS,
		      &g_video_read_hdl);

		printf(" turn on dvp camera\n");
		ioctl(g_vin_dvp_fd, VINDVP_START, tv_sys);
	} else {

        if(g_vin_dvp_fd < 0)
            return 0;

		printf(" turn off dvp camera\n");

        msleep(333); // !note: 等待还没完成的video数据接收完成
 
		// g_vin_dvp_fd = open("/dev/vindvp", O_WRONLY);
		ioctl(g_vin_dvp_fd, VINDVP_STOP);
		close(g_vin_dvp_fd);
		g_vin_dvp_fd = -1;
		g_is_camera_start = false;
	}

	return 0;
}


static int dvp_camera_start(void)
{
   	enum TVTYPE tv_sys = TV_NTSC; 
	if (g_vin_dvp_fd < 0) {
		printf("[Error] /dev/vindvp is not open\n");
		return -1;
	}
	printf(" start dvp camera\n");
	ioctl(g_vin_dvp_fd, VINDVP_START, tv_sys);

	return 0;
}


static int dvp_camera_get_frame(uint8_t **frame, int *frame_sz)
{
	int index = 0;
	AvPktHd hdr = { 0 };
	uint8_t *data = &dvp_data[0];

	if (g_is_camera_start == false) {
		dvp_camera_start();
		g_is_camera_start = true;
	}

	while (kshm_read(&g_video_read_hdl, &hdr, sizeof(AvPktHd)) !=
	       sizeof(AvPktHd)) {
		mdelay(5);
	}

    // printf("\n==> frame size %d\n", hdr.size + 2);

	while (kshm_read(&g_video_read_hdl, data, hdr.size) != hdr.size) {
		mdelay(5);
	}
    // printf("==> frame %p\n", data);

    /* add jpg ending content */
	data[hdr.size + 1] = 0xFF;
	data[hdr.size + 2] = 0xD9;
	hdr.size += 2;

	*frame = data;
	*frame_sz = hdr.size;
	return 0;
}

#endif

/* ---------------------  ---------------------  ---------------------  --------------------- */
/* ---------------------  demo, read from udisk  ---------------------  --------------------- */
/* ---------------------  ---------------------  ---------------------  --------------------- */

__attribute__((weak)) int dvp_camera_switch(bool is_turn_on)
{
	printf(" ==> camera %s\n", is_turn_on ? "on" : "off");
	return 0;
}


__attribute__((weak)) int dvp_camera_get_frame(uint8_t **frame, int *frame_sz)
{
#define FB_BUF_SIZE (300 * 1024)
	int fd;
	int file_size;
	int i;
	int ret;
	static uint8_t *buf = NULL;

	if (!buf) {
		buf = malloc(FB_BUF_SIZE);
		if (!buf) {
			log_e("Cannot malloc enough memory\n");
			return -1;
		}
	}

	fd = open("/media/sda1/tt.jpg", O_RDONLY);
	if (fd < 0) {
		log_e("Cannot open /media/sda1/tt.jpg\n");
		return -1;
	}

	lseek(fd, 0, SEEK_SET);
	file_size = lseek(fd, 0, SEEK_END);
	if (file_size > FB_BUF_SIZE) {
		log_e("Buffer isnot enough to save one frame data\n");
		free(buf);
		close(fd);
		buf = NULL;
		return -1;
	}

	lseek(fd, 0, SEEK_SET);
	ret = read(fd, buf, file_size);
	if (ret != file_size) {
		log_e("Read error. return value is %d, but file size is %d\n",
		      ret, file_size);
		return -1;
	}

	close(fd);

	*frame = buf;
	*frame_sz = file_size;

	return 0;
}

int setup_usbd_uvc(int argc, char **argv)
{
	char ch;
	int i;
	const char *udc_name = NULL;
	int usb_port = 0;

	opterr = 0;
	optind = 0;

	// elog_set_filter_tag_lvl("uvc", ELOG_LVL_INFO);
	// elog_set_filter_tag_lvl("usbd", ELOG_LVL_ALL);
	// elog_set_filter_tag_lvl("hcusb", ELOG_LVL_ALL);
	// elog_set_filter_tag_lvl("musbg", ELOG_LVL_WARN);
	// elog_set_filter_tag_lvl("composite", ELOG_LVL_INFO);

	// elog_set_filter_tag_lvl("usbd-ep0", ELOG_LVL_INFO);
	// elog_set_filter_tag_lvl("hcusb", ELOG_LVL_INFO);

	while ((ch = getopt(argc, argv, "hHsSp:P:")) != EOF) {
		switch (ch) {
		case 'h':
		case 'H':
			usbd_uvc_help_info();
			return 0;
		case 'p':
		case 'P':
			usb_port = atoi(optarg);
			udc_name = get_udc_name(usb_port);
			if (udc_name == NULL) {
				printf("[error] parameter(-p {usb_port}) error,"
				       "please check for help information(cmd: g_uvc -h)\n");
				return -1;
			}
			printf("==> set usb#%u as uvc demo gadget\n", usb_port);
			break;
		case 's':
		case 'S':
			hcusb_set_mode(usb_port, MUSB_HOST);
			hcusb_gadget_uvc_deinit();
			return 0;
		default:
			break;
		}
	}

	if (!udc_name)
		hcusb_gadget_uvc_init(dvp_camera_switch, dvp_camera_get_frame);
	else
		hcusb_gadget_uvc_specified_init(udc_name, dvp_camera_switch,
						dvp_camera_get_frame);

	hcusb_set_mode(usb_port, MUSB_PERIPHERAL);
	return 0;
}
