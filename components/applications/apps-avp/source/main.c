
#include <generated/br2_autoconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <kernel/elog.h>
#include <sys/poll.h>
#include <kernel/module.h>
#include <kernel/io.h>
#include <kernel/lib/console.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hcuapi/input.h>


static void app_main(void *pvParameters);

#define INPUT_DEVICE "/dev/input/event0"

static int input_test(void)
{
	int fd;
	struct input_event t;
	struct pollfd pfd;

	long tmp;
	int x = 0, y = 0;
	int event_num = 0;
	char ch;
	opterr = 0;
	optind = 0;

	fd = open(INPUT_DEVICE, O_RDONLY);
	pfd.fd = fd;
	pfd.events = POLLIN | POLLRDNORM;

	if(fd < 0){
		printf("can't open %s\n", INPUT_DEVICE);
		return -1;
	}

	while (1) {
		if (poll(&pfd, 1, -1) <= 0)
			continue;

		if (read(fd, &t, sizeof(t)) != sizeof(t))
			continue;

		printf("type:%d, code:%d, value:%ld\n", t.type, t.code, t.value);

		if (t.type == EV_KEY) {
			printf("key 0x%x %s\n", t.code, (t.value) ? "Pressed" : "Released");
		}
		else{
			if (t.type == EV_ABS)
			{
				if (t.type == EV_ABS&& t.code == ABS_X) {
					x = t.value;
				}
				if (t.type == EV_ABS && t.code == ABS_Y) {
					y = t.value;
				}
			}
			if (t.type == EV_SYN) {
				printf("(%4d %4d)\n",x,y);
			}
		}
	}

	close(fd);

	return 0;
}

int main(void)
{
	xTaskCreate(app_main, (const char *)"app_main", configTASK_STACK_DEPTH,
		    NULL, portPRI_TASK_NORMAL, NULL);

	vTaskStartScheduler();

	abort();
	return 0;
}

static void app_main(void *pvParameters)
{
	assert(module_init("all") == 0);

	/* Set default time zone is GMT+8 */
	setenv("TZ", CONFIG_APP_TIMEZONE, 1);
	tzset();
	// For some reason, printf does not work before this point. 

	//console_init();
	/* Console loop */
	//console_start();

	printf("Pre Main Loop\n");
	fflush(stdout);
	printf("ribbit\n");
	fflush(stdout);
	printf("Hello Froggy!\n");
	fflush(stdout);

	input_test();

	/* Program should not run to here. */
	for (;;)
	{
		vTaskDelay(100);
	}

	/* Delete current thread. */
	vTaskDelete(NULL);
}
