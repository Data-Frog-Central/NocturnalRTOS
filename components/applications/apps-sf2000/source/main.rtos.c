
#include <generated/br2_autoconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <_syslist.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <kernel/elog.h>
#include <sys/poll.h>
#include <kernel/module.h>
#include <kernel/io.h>
#include <kernel/lib/console.h>
#include <kernel/drivers/lcd_printf.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <kernel/delay.h>

#include <sys/ioctl.h>
#include <hcuapi/pwm.h>

#include <hcuapi/gpio.h>
#include <hcuapi/pinpad.h>

#include "libs/libretro-common/include/libretro.h"
#include "menu/file_functions.h"
#include "menu/menu.h"

char *temp_rom_path, *temp_core_path, *temp_audio_device, *temp_joypad_device;

int apply_backlight_brightness(int pwm_level, int pwm_frequency, int polarity) {
    int fd;
    uint32_t period_ns;
    struct pwm_info_s info = { 0 };
    int ret_set, ret_start;

    // Ensure backlight_pwm_level is within valid range (0-100)
    if (pwm_level < 0)
        pwm_level = 0;
    if (pwm_level > 100)
        pwm_level = 100;

    // Set the PWM period in nanoseconds based on the given frequency
    period_ns = 1000000000u / pwm_frequency;

    // Set PWM characteristics
    info.period_ns = period_ns;
    info.duty_ns = (period_ns * (uint32_t)pwm_level) / 100u;  // Calculate duty cycle based on the level
    info.polarity = polarity;

    // Open the PWM device (/dev/pwm2)
    fd = open("/dev/pwm2", O_RDWR);
    if (fd < 0) {
        frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"Failed to open PWM device /dev/pwm2\n");
        return -1;
    }

    // Set PWM characteristics and start the PWM signal
    ret_set = ioctl(fd, PWMIOC_SETCHARACTERISTICS, &info);
    ret_start = ioctl(fd, PWMIOC_START, 0);
    close(fd);

    // Check if the PWM setup was successful
    if (ret_set == 0 && ret_start == 0) {
        frontend_log_cb(RETRO_LOG_DEBUG, "FRONTEND" ,"PWM: pwm_level=%d freq=%d period_ns=%u duty_ns=%u polarity=%d\n", pwm_level, pwm_frequency, (unsigned)info.period_ns, (unsigned)info.duty_ns, info.polarity);
        return 0;
    } else {
        frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"PWM setup failed: ret_set=%d ret_start=%d\n", ret_set, ret_start);
        return -1;
    }
}

// TODO: what kind magic goes here?
// without that the audio output is silent. can it be done in dts instead?
// the i2so driver in "i2so_platform_init" function reads "pinmux-data" and
// "pinmux-mute" settings from the dts. might be related?
void setUpPins(void) {
    gpio_configure(PINPAD_L00, GPIO_DIR_OUTPUT); //Charging LED
    gpio_set_output(PINPAD_L00, false); // high = off, low = on;

    // Overheating fix
    *(volatile unsigned *)0xb8800060 = 0x00000f88; // NB_LDCR Local Device Clock Gating Control Register            
    *(volatile unsigned *)0xb8800064 = 0x0bc04040; // Local Device Clock Gating Control Register1
    *(volatile unsigned *)0xb8800080 = 0x000023c0; // NB_LDRCR NorthBridge Local Device Reset Control Register
    *(volatile unsigned *)0xb8800084 = 0xa00b4000;
}

static void main_sf2000(void *pvParameters) {
    assert(module_init("all") == 0);
	
	// Waits for fileuart to be ready
	int fd = open("/dev/fileuart", O_WRONLY);
	struct pollfd pfd = {
    	.fd = fd,
    	.events = POLLOUT
	};
	poll(&pfd, 1, -1);
    close(fd);

    setUpPins();

	frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"Init Frontend\n");
    frontend_config_load();
	bool ret = run_emulator(temp_rom_path, temp_core_path, 0);
    // TODO: When i implement a menu this should be a dbg_show_noblock not a bsod
    if (!ret) lcd_bsod(" Loading ROM Failed\n\n Check logs for more info\n\n");
    lcd_bsod(" Safe Shutdown\n\n Turn off the console now.\n\n");
    vTaskDelete(NULL);
}

int main(void) {
    size_t stackSize = 0x20000 / sizeof(StackType_t);  // 64 KB stack size in words (FreeRTOS uses words, not bytes)
    xTaskCreate(main_sf2000, (const char *)"main_sf2000", stackSize, NULL, portPRI_TASK_NORMAL, NULL);
    vTaskStartScheduler();
    abort(); // Should never reach here
    return 0;
}
