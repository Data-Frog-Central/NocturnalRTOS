
#include <generated/br2_autoconf.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <_syslist.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/unistd.h>
#include <kernel/delay.h>
#include <kernel/elog.h>
#include <kernel/io.h>
#include <kernel/ld.h>
#include <kernel/module.h>
#include <kernel/lib/console.h>
#include <kernel/lib/fdt_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <hcuapi/gpio.h>
#include <hcuapi/pinpad.h>
#include <hcuapi/pwm.h>

#include <libretro.h>
#include <helix/helix_logging.h>
#include <helix/nocturnal_paths.h>
#include <helix/helix_fake_rtc.h>

#include "drivers/sf2000_gfx.h"
#include "phobos/phobos.h"
#include "phobos/loading_screen.h"

#define SYSIO_BASE       ((volatile unsigned char *)&SYSIO0)
#define NB_REG(offset)   (*(volatile uint32_t *)(SYSIO_BASE + (offset)))
#define NB_LDCR          0x60
#define NB_LDRCR         0x80

#define VAL_LDCR    0x00000f88
#define VAL_LDCR1   0x0bc04040
#define VAL_LDRCR   0x000023c0
#define VAL_LDRCR1  0xa00b4000

extern void frontend_config_load(void);

int apply_backlight_brightness(int pwm_level, int pwm_frequency, int polarity) {
    if (pwm_level < 0) pwm_level = 0;
    if (pwm_level > 100) pwm_level = 100;

    int fd = open("/dev/pwm2", O_RDWR);
    if (fd < 0) {
        frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"Failed to open PWM device /dev/pwm2\n");
        return -1;
    }

    // Set PWM characteristics and start the PWM signal
    struct pwm_info_s info = { 0 };
    info.period_ns = 1000000000u / pwm_frequency; // Set the PWM period in nanoseconds based on the given frequency
    info.duty_ns = (info.period_ns * (uint32_t)pwm_level) / 100u;  // Calculate duty cycle based on the level
    info.polarity = polarity;

    int ret_set = ioctl(fd, PWMIOC_SETCHARACTERISTICS, &info);
    int ret_start = ioctl(fd, PWMIOC_START, 0);
    close(fd);

    if (ret_set == 0 && ret_start == 0) return 0;        
    else return -1;
}

uint32_t get_time_ms(void) {
    return (uint32_t)xTaskGetTickCount();
}

// TODO: clean this up and turn off other stuff
static void check_nb_registers(bool first_run) {
    uint32_t ldcr_before  = NB_REG(NB_LDCR);
    uint32_t ldcr1_before = NB_REG(NB_LDCR + 4);
    uint32_t ldrcr_before = NB_REG(NB_LDRCR);
    uint32_t ldrcr1_before= NB_REG(NB_LDRCR + 4);

    if (ldcr_before != VAL_LDCR || ldcr1_before != VAL_LDCR1 || 
        ldrcr_before != VAL_LDRCR || ldrcr1_before != VAL_LDRCR1) {
        
        if (!first_run) printf("Overheating fix overridden, reapplying overheating fix...\n");
        printf("Overheating Fix:\n");
        printf("BEFORE -> LDCR: 0x%08x | LDCR1: 0x%08x | LDRCR: 0x%08x | LDRCR1: 0x%08x\n",
               (unsigned int)ldcr_before, (unsigned int)ldcr1_before, (unsigned int)ldrcr_before, (unsigned int)ldrcr1_before);

        NB_REG(NB_LDCR)     = VAL_LDCR;
        NB_REG(NB_LDCR + 4) = VAL_LDCR1;
        NB_REG(NB_LDRCR)    = VAL_LDRCR;
        NB_REG(NB_LDRCR + 4)= VAL_LDRCR1;

        uint32_t ldcr_after  = NB_REG(NB_LDCR);
        uint32_t ldcr1_after = NB_REG(NB_LDCR + 4);
        uint32_t ldrcr_after = NB_REG(NB_LDRCR);
        uint32_t ldrcr1_after= NB_REG(NB_LDRCR + 4);

        printf("AFTER  -> LDCR: 0x%08x | LDCR1: 0x%08x | LDRCR: 0x%08x | LDRCR1: 0x%08x\n",
               (unsigned int)ldcr_after, (unsigned int)ldcr1_after, (unsigned int)ldrcr_after, (unsigned int)ldrcr1_after);
    }
}

// TODO: what kind magic goes here?
// without that the audio output is silent. can it be done in dts instead?
// the i2so driver in "i2so_platform_init" function reads "pinmux-data" and
// "pinmux-mute" settings from the dts. might be related?
void setUpPins(void) {
    gpio_configure(PINPAD_L00, GPIO_DIR_OUTPUT); //Charging LED
    gpio_set_output(PINPAD_L00, false); // high = off, low = on;
    check_nb_registers(true);
}

static bool is_fileuart_enabled(void) {
    int np = 0;
    const char *status = NULL;
    int ret;

    np = fdt_node_probe_by_path("/hcrtos/fileuart");
    if (np < 0) return false;

    ret = fdt_get_property_string_index(np, "status", 0, &status);
    if (ret == 0 && status != NULL) {
        if (strcmp(status, "okay") == 0 || strcmp(status, "ok") == 0) return true;
        else return false;
    }
    return false;
}

static void main_sf2000(void *pvParameters) {
    assert(module_init("all") == 0);

	// Waits for fileuart to be ready
    bool fileuart = is_fileuart_enabled();
    if (fileuart) {
	    int fd = open("/dev/fileuart", O_WRONLY);
	    struct pollfd pfd = {
    	    .fd = fd,
    	    .events = POLLOUT
	    };
	    poll(&pfd, 1, -1);
        close(fd);
    }

    setUpPins();

    // Every boot clear the logs if fileuart isn't enabled
    // otherwise assume it's a debug build and keep them
    init_xlog(LOG_PATH, !fileuart, fileuart);
    xlog("[FRONTEND][INFO] Init Frontend\n"); // xlog these so they log no matter what
    xlog("[FRONTEND][INFO] Fileuart status = %s\n", fileuart ? "enabled" : "disabled");

    fake_rtc_init();
    frontend_config_load();
    set_min_log_level(0); // TODO: let the user set this
    frontend_video_init();

    xSemaphoreTake(xLoadingInitSemaphore, portMAX_DELAY);

    while (1) { // TODO: Guard this to FrogUI only, lua frontend will handle this itself
        check_nb_registers(false);
        if (!run_emulator(rom_path, core_path, 0)) safe_shutdown_flag = true;
        sync_xlog();
        if (safe_shutdown_flag) break;
    }

    fake_rtc_deinit();
    close_xlog();
    if (fileuart) module_exit("fileuart");
    // Unmount the sd card to stop corruption
    if (umount(SDCARD_DIRECTORY) < 0) show_loading_screen(true, false, loading_txt_color, loading_bg_color, "Failed to unmount SD card cleanly.\nTurn off the console now.");
    show_loading_screen(true, false, loading_txt_color, loading_bg_color, "Safe Shutdown Complete.\nIt is now safe to power off your console.");
}

int main(void) {
    size_t stackSize = 0x20000 / sizeof(StackType_t);  // 64 KB stack size in words (FreeRTOS uses words, not bytes)

    xLoadingQueue = xQueueCreate(5, sizeof(loading_msg_t));
    configASSERT(xLoadingQueue);

    xLoadingInitSemaphore = xSemaphoreCreateBinary();
    configASSERT(xLoadingInitSemaphore);

    xTaskCreate(loading_screen_task, (const char *)"loading_task", 2048, NULL, portPRI_TASK_NORMAL + 1, NULL);
    xTaskCreate(main_sf2000, (const char *)"main_sf2000", stackSize, NULL, portPRI_TASK_NORMAL, NULL);
    vTaskStartScheduler();

    abort(); // Should never reach here
    return 0;
}
