#define LOG_TAG "pq_start"

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <kernel/elog.h>
#include <hcuapi/pq.h>
#include <kernel/lib/console.h>
#include <kernel/delay.h>
#include <kernel/io.h>

static int mipi_set_power(int val)
{
	/*When switching to PQ for the first time, mipi may need to be closed*/
	int fd = 0;
	#define MIPIDEV_PATH  "/dev/mipi"
	fd = open(MIPIDEV_PATH, O_RDWR);
	if(fd > 0)
	{
		msleep(1);
		printf("%s %d\n",__func__, __LINE__);
		REG32_WRITE(0xb884a004, val);
		close(fd);
	}
}

int open_pq_start(int argc, char *argv[])
{
	int pq_fd = -1;

	pq_fd = open("/dev/pq", O_WRONLY);
	if (pq_fd < 0) {
		log_e("pq_start open error\n");
		return -1;
	}

	mipi_set_power(0);
	ioctl(pq_fd, PQ_START);
	mipi_set_power(1);
	close(pq_fd);
	return 0;
}

CONSOLE_CMD(pq_start, NULL, open_pq_start, CONSOLE_CMD_MODE_SELF,
	    "boot enter standby")
