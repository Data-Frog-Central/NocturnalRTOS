
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
#include <kernel/delay.h>

#include <hcuapi/gpio.h>
#include <hcuapi/pinpad.h>

//TODO: Check if there is a correct way to include retroarch headers. Current inclusion causes compile issues.
int rarch_main(int argc, char *argv[], void *data);
void verbosity_enable(void);
void verbosity_set_log_level(unsigned level);

// TODO: what kind magic goes here?
// without that the audio output is silent. can it be done in dts instead?
// the i2so driver in "i2so_platform_init" function reads "pinmux-data" and
// "pinmux-mute" settings from the dts. might be related?
void setUpPins(void) {
    gpio_configure(PINPAD_R07, GPIO_DIR_OUTPUT); //Speaker Disable
    gpio_set_output(PINPAD_R07, false); // high = disable, low = enable;

    gpio_configure(PINPAD_R05, GPIO_DIR_OUTPUT); //AV / LCD switch
    gpio_set_output(PINPAD_R05, false); // high = LCD, low = AV;

    gpio_configure(PINPAD_L00, GPIO_DIR_OUTPUT); //Charging LED
    gpio_set_output(PINPAD_L00, false); // high = off, low = on;

    gpio_configure(PINPAD_T07, GPIO_DIR_OUTPUT); //Speaker fix?
    gpio_set_output(PINPAD_T07, true); // high = off, low = on;
}

static void main_sf2000(void *pvParameters) {
    //TODO: Remove need for sleep, by adding a buffer to fileuart
    msleep(2000); //Initial delay to allow fileuart to catch up. Tests has shown that 600 is at least needed, but might be more.

	setUpPins();

    printf("Init Retroarch!\n");

    /*
    char *argv[] = {
        "retroarch",
        "--menu",
        "-v"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    rarch_main(argc, argv, NULL);
    */

    // TODO: learn how to properly pass startup params to retroarch
    // or maybe better to pass via retroarch.cfg file instead
    // for now just force logging verbosity and dont pass anything

    verbosity_enable();
	verbosity_set_log_level(0);	// 0-DGB, 1-INFO, 2-WARN, 3-ERR

    rarch_main(0, NULL, NULL);
    vTaskDelete(NULL);
}

// Looks like this needs to be its own thread
static void app_main(void *pvParameters) {
    assert(module_init("all") == 0); // Needed to init drivers

    // TODO: Does this affect performance?
    console_init();
    console_start(); // Thread should not run past this point.
    vTaskDelete(NULL);
}

int main(void) {
    xTaskCreate(app_main, (const char *)"app_main", configTASK_STACK_DEPTH, NULL, portPRI_TASK_NORMAL, NULL);
    size_t stackSize = 0x10000 / sizeof(StackType_t);  // 64 KB stack size in words (FreeRTOS uses words, not bytes)
    xTaskCreate(main_sf2000, (const char *)"main_sf2000", stackSize, NULL, portPRI_TASK_NORMAL, NULL);
    vTaskStartScheduler();
    abort(); // Should never reach here
    return 0;
}
