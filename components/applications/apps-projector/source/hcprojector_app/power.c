#include "app_config.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <hcuapi/standby.h>
#include <hcuapi/lvds.h>
#include <hcuapi/mipi.h>
#include "screen.h"
#include "factory_setting.h"
#include "com_api.h"

#ifdef BLUETOOTH_SUPPORT
#include <bluetooth.h>
#endif

#ifdef __HCRTOS__
//#include <kernel/elog.h>
//#include <kernel/delay.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <kernel/io.h>
#endif
#include "channel/local_mp/media_player.h"
#include "channel/local_mp/mp_ctrlbarpage.h"

static void standby_pre_process(void);
// pre process before enter standby mode
static void standby_pre_process(void)
{
    int fd;
    int temp = 0;

    //step 1: close display    
    api_set_backlight_brightness(0);
    api_osd_show_onoff(false);
    api_logo_off();
    api_dis_show_onoff(false);

    //step 2: stop device & save system data	
    hdmirx_pause();
    cvbs_rx_stop();
    media_player_close();

    projector_sys_param_save();

#if PROJECTER_C2_D3000_VERSION
    api_set_i2so_gpio_mute(true);
#endif

#ifdef  BLUETOOTH_SUPPORT   
    bluetooth_set_gpio_backlight(0);
    bluetooth_set_gpio_mutu(1);
    printf("bluetooth disconnect test\n");
    bluetooth_poweroff();//stop bluetooth
    api_sleep_ms(200);
    bluetooth_deinit();
#endif    

    //step 3:  lowpower disaplay: lcd/backlight/light-machine etc.
    printf("close lcd/backlight/ etc.\n");
	fd = open("/dev/lvds", O_RDWR);
	if (fd) {
		ioctl(fd, LVDS_SET_GPIO_POWER, 0); //lvds gpio power close
		close(fd);
	}
	fd = open("/dev/mipi", O_RDWR);
	if (fd) {
		ioctl(fd, MIPI_DSI_GPIO_ENABLE, 0); //mipi close gpio enable
		close(fd);
	}
	fd = open("/dev/lcddev", O_RDWR); //lcddev close gpio enable
	if (fd) {
		temp = 0;
		write(fd, &temp, 4);
		close(fd);
	}

    api_dis_suspend();
    api_sleep_ms(100);


}

// the Keys set in DTS, not here
void enter_standby(void)
{
    int fd_standby;    
    
    fd_standby = open("/dev/standby", O_RDWR);
    if(fd_standby<0){
        printf("Open /dev/standby failed!\n");
        return;
    }
    standby_pre_process();

    printf("enter standby!\n");

    api_watchdog_stop();

#ifdef __HCRTOS__    
    taskDISABLE_INTERRUPTS();
#endif    
    ioctl(fd_standby, STANDBY_ENTER, 0);
    close(fd_standby);
}

