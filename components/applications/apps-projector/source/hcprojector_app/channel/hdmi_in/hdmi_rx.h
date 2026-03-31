
#ifndef _HDMI_RX_H_
#define _HDMI_RX_H_

#include <hcuapi/kshm.h>
#include <hcuapi/hdmi_rx.h>
#include <hcuapi/hdmi_tx.h>
#include <hcuapi/dis.h>
#include <hcuapi/viddec.h>
#include <hcuapi/fb.h>

#ifdef __HCRTOS__
#include <kernel/lib/console.h>
#include <nuttx/wqueue.h>
#endif


#define HDMI_RX_STATUS_PLUGOUT		0 //  plugout, disconnect
#define HDMI_RX_STATUS_PLUGIN		1 // plugin , connect, playing
#define HDMI_RX_STATUS_ERR_INPUT    2  // this hrx device not support

#define HDMI_RX_STATUS_STOP   		3
#define HDMI_RX_STATUS_START   		4
#define HDMI_RX_STATUS_PAUSE 		5
#define HDMI_RX_STATUS_PLAYING   	6

#define HDMI_SWITCH_BOOTLOGO_LAYER			DIS_LAYER_MAIN
#define HDMI_SWITCH_HDMI_RX_LAYER           DIS_LAYER_AUXP

#define HDMI_SWITCH_DEFAULT_DISP_RESOLUTION TVSYS_1080P_60
#define HDMI_SWITCH_GET_EDID_TRY_TIME		500




struct hdmi_info{
    int fd;

#ifdef __HCRTOS__
    struct work_notifier_s notify_plugin;
    struct work_notifier_s notify_plugout;
    struct work_notifier_s notify_err_input;
    struct work_notifier_s notify_edid;
	int in_ntkey; //plugin ntkey
	int out_ntkey; //plugin ntkey
	int err_ntkey; //err ntkey
#else
    pthread_t pid;
    int thread_run;
    int epoll_fd;
    int kumsg_id;
#endif
    char plug_status;
    char status;
    char enable;
};

struct hdmi_switch{
    struct hdmi_info in;
    struct hdmi_info out;

    int dis_fd;
    struct dis_display_info bootlogo;
    struct dis_display_info hdmi_rx;
    struct dis_tvsys hdmi_out_tvsys;
    enum TVSYS last_tv_sys;
};


int hdmirx_get_plug_status(void);
#endif