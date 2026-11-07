#include <sys/times.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

clock_t times(struct tms *buf) {
	TickType_t freertos_ticks;

	if (!xPortIsInISR()) freertos_ticks = xTaskGetTickCount();
	else freertos_ticks = xTaskGetTickCountFromISR();

	clock_t scaled_ticks = (clock_t)(((uint64_t)freertos_ticks * CLOCKS_PER_SEC) / configTICK_RATE_HZ);

	if (buf != NULL) {
		buf->tms_stime = scaled_ticks;
		buf->tms_utime = 0;
		buf->tms_cutime = 0;
		buf->tms_cstime = 0;
	}

	return scaled_ticks;
}
