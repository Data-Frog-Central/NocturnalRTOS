#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>

#include <hcuapi/audsink.h>
#include <time.h>
#include "com_api.h"
#include "lvgl/lvgl.h"
#include "../lvgl/src/misc/lv_types.h"
#include "key.h"
#include "hcuapi/gpio.h"
#include "hcuapi/pinpad.h"
#include <ffplayer.h>

#include <kernel/delay.h>

#define KEY_SHIFTER_CLK_PIN PINPAD_L24 // the clock pin is shared
//#define KEY_SHIFTER_PL1_PIN PINPAD_L25 // X60
#define KEY_SHIFTER_PL1_PIN PINPAD_L23 // SF2000
#define KEY_SHIFTER_PL2_PIN PINPAD_L26 // currently unimplemented

static const char * const key_names[] = {
	"L", "Y", "X", "R", "A", "B", "Select", "Start", "Up", "Down", "Left", "Right"
};

static void keypad_test_loop(void)
{
	int i, prev[12], state[12];

	for (i = 0; i < 12; i++) prev[i] = 1;

	gpio_configure(KEY_SHIFTER_CLK_PIN, GPIO_DIR_OUTPUT);
	gpio_set_output(KEY_SHIFTER_CLK_PIN, 1); // shifts on 1->0 transition
	while (1) {
		// probably latches the state while the pin is actively driven
		gpio_configure(KEY_SHIFTER_PL1_PIN, GPIO_DIR_OUTPUT);
		gpio_set_output(KEY_SHIFTER_PL1_PIN, 0);
		usleep(4);
		gpio_configure(KEY_SHIFTER_PL1_PIN, GPIO_DIR_INPUT);
		for (i = 0; i < 12; i++) {
			state[i] = gpio_get_input(KEY_SHIFTER_PL1_PIN);
			gpio_set_output(KEY_SHIFTER_CLK_PIN, 0);
			usleep(2);
			gpio_set_output(KEY_SHIFTER_CLK_PIN, 1);
		}

		for (i = 0; i < 12; i++) if (prev[i] != state[i]) {
			printf("%s %s\n", key_names[i], state[i] ? "released" : "pressed");
			prev[i] = state[i];
		}

		// original software has 4 but scans every 4th iteration
		// a software key debounce, or the MCU can't handle faster rate?
		msleep(16);
	}
}

static void exit_console(int signo)
{
    printf("%s(), signo: %d, error: %s\n", __FUNCTION__, signo, strerror(errno));

    api_watchdog_stop();

    exit(0);
}

static void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
//    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t vkey = VKEY_NULL;
    lv_group_t *group = lv_group_get_default();

    if (code == LV_EVENT_KEY)
    {
        uint32_t value = lv_indev_get_key(lv_indev_get_act());
        vkey = key_convert_vkey(value);

        if (V_KEY_UP == vkey)
        {
            lv_group_focus_prev(group);
        }
        else if (V_KEY_DOWN == vkey)
        {
            lv_group_focus_next(group);
        }
        else
        {
            api_control_send_key(vkey);
        }
    }
}

static void *watchdog_task(void *arg)
{
    while (1)
    {
        usleep(3 * 1000 * 1000);
        api_watchdog_feed();
    }
}

static void start_watchdog_task()
{
    pthread_t pid;

#ifdef WATCHDOG_KERNEL_FEED
    return ;
#endif

    pthread_create(&pid, NULL, watchdog_task, NULL);
}

void list_dir(char* dir_name)
{
    DIR *dirp = NULL;
    struct dirent *entry = NULL;
    int check_count = 10;
    char sub_path[1024];
    while(check_count --) {
        if ((dirp = opendir(dir_name)) == NULL) {
            api_sleep_ms(100);
            continue;
        }

        while (1) {
            entry = readdir(dirp);
            if (!entry)
                break;

            if(!strcmp(entry->d_name, ".") ||
               !strcmp(entry->d_name, "..")){
                //skip the upper dir
                continue;
            }

            if (strlen(entry->d_name)){
                printf("(%d) %s / %s", entry->d_type, dir_name, entry->d_name);
                printf("\n");
                memset(&sub_path[0], 0, sizeof(sub_path));
                if (entry->d_type == 4) { //dir
                    if (!strcmp(dir_name, "/"))
                        sprintf(sub_path, "/%s", entry->d_name);
                    else
                        sprintf(sub_path, "%s/%s", dir_name, entry->d_name);
                    list_dir(sub_path);
                }
            }

        }
        break;
    }
}

void setUpPins()
{
    gpio_configure(PINPAD_R07, GPIO_DIR_OUTPUT); //Speaker Disable
    gpio_set_output(PINPAD_R07, false); // high = disable, low = enable;

    gpio_configure(PINPAD_R05, GPIO_DIR_OUTPUT); //AV / LCD switch
    gpio_set_output(PINPAD_R05, false); // high = LCD, low = AV;

    gpio_configure(PINPAD_L00, GPIO_DIR_OUTPUT); //Charging LED
    gpio_set_output(PINPAD_L00, false); // high = off, low = on;

    gpio_configure(PINPAD_T07, GPIO_DIR_OUTPUT); //Speaker fix?
    gpio_set_output(PINPAD_T07, true); // high = off, low = on;
}

static void *m_video_player = NULL;

void playVideo()
{
    char *file_path = "media/mmcblk0p2/video_test.mp4";

    HCPlayerInitArgs player_args;
    memset(&player_args, 0, sizeof(player_args));
    player_args.uri = file_path;
    player_args.msg_id = 0;
    player_args.sync_type = HCPLAYER_VIDEO_MASTER;

    m_video_player = hcplayer_create(&player_args);
    if (!m_video_player)
    {
        printf("hcplayer_create() fail!\n");
        return;
    }

    hcplayer_play(m_video_player);
}

void * main_sf2000(void *arg)
{
    printf("%s %d\n", __FUNCTION__,__LINE__);
    api_sleep_ms(2000); //Wait 2 seconds for mmc to init
    printf("%s %d\n", __FUNCTION__,__LINE__);
    setUpPins();
    printf("%s %d\n", __FUNCTION__,__LINE__);
    printf("Welcom to SF2000!\n");
    //list_dir("/");
    keypad_test_loop();
    printf("%s %d\n", __FUNCTION__,__LINE__);
    app_ffplay_init();

    start_watchdog_task();

    api_system_init();
    api_video_init();
    api_audio_init();
    printf("%s %d\n", __FUNCTION__,__LINE__);
    playVideo();
    printf("After Logo!\n");

    //api_lvgl_init(OSD_MAX_WIDTH, OSD_MAX_HEIGHT);

    /*Handle LitlevGL tasks (tickless mode)*/
    while (1)
    {
        //lv_task_handler();
        usleep(1000);//frank, the sleep time will result in the OSD UI flush
    }

    printf("sf2000_demo exit!\n");

    return 0;
}

