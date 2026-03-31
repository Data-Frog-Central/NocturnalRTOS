#define LOG_TAG "hid_dev"
#define ELOG_OUTPUT_LVL ELOG_LVL_INFO

#include <kernel/elog.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <kernel/drivers/input.h>
#include <hcuapi/usbhid.h>

#include <kernel/notify.h>
#include <hcuapi/sys-blocking-notify.h>
#include <kernel/drivers/hcusb.h>
#include <kernel/delay.h>


struct hc_hid_demo_t {
    
    struct work_s work;

    bool is_config_gadget;
    
    /* Host HID */
    /* report descriptor from HOST HID mouse/keyboard */
    struct hidg_func_descriptor *mouse_report;
    struct hidg_func_descriptor *kbd_report;
    bool is_mouse_plugin;
    bool is_kbd_plugin;
    
    /* Gadget HID */
    int fd_g_mouse;
    int fd_g_kbd;

} g_hc_hid_demo;


static void dump_report_desc(struct hidg_func_descriptor *report_desc)
{
    int n;
    if(!report_desc)
        return;

    printf("============== report desc ============\n");
	printf("subclass:%d\n", report_desc->subclass);
	printf("protocol:%d\n", report_desc->protocol);
	printf("report_length:%d\n", report_desc->report_length);
	printf("report_desc_length:%d\n", report_desc->report_desc_length);
	printf("report desc:\n");
	for(n = 0; n < report_desc->report_desc_length; n++){
		if(n && n % 8 == 0)
			printf("\n");        
		printf("0x%2.2x ", report_desc->report_desc[n]);
	}
	printf("\n=======================================\n\n");
}

static int usbmouse_hook(char *data, int len)
{
    int ret = 0;
    if(len){
        // int index;
        // char str[128] = {0};
        // char *pstr = &str[0];
        // pstr += sprintf(pstr, "mouse hook(len:%d): ", len);
        // for(index = 0; index < len; index++)
        //     pstr += sprintf(pstr, " %2.2x", (unsigned char)data[index]);
        // pstr += sprintf(pstr, "\n");
        // log_i("%s", str);

        if(g_hc_hid_demo.fd_g_mouse < 0)
            return 0;

        ret = write(g_hc_hid_demo.fd_g_mouse, data, len);
    }
    return ret;
}


static int usbkbd_hook(char *data, int len)
{
    int ret = 0;
    if(len){
        // int index;
        // char str[128] = {0};
        // char *pstr = &str[0];
        // pstr += sprintf(pstr, "kbd hook(len:%d): ", len);
        // for(index = 0; index < len; index++)
        //     pstr += sprintf(pstr, " %2.2x", (unsigned char)data[index]);
        // pstr += sprintf(pstr, "\n");
        // log_i("%s", str);

        if(g_hc_hid_demo.fd_g_kbd < 0)
            return 0;

        ret = write(g_hc_hid_demo.fd_g_kbd, data, len);
    }
    return ret;
}


static void hc_hid_workqueue(void *param)
{
    int ret;
    // unsigned long action = (unsigned long)param;
    // int fd_h_mouse = -1;
    // int fd_h_kbd = -1;

    // switch (action)
    // {
    //     case USB_HID_MOUSE_NOTIFY_CONNECT:
    //         fd_h_mouse = open("/dev/usbmouse", O_RDWR);
    //         if(fd_h_mouse < 0){
    //             printf("Error: cannot open /dev/usbmouse\n");
    //             return;
    //         }
    //         ioctl(fd_h_mouse, USBHID_SET_HOOK, usbmouse_hook);
    //         close(fd_h_mouse);
    //         g_hc_hid_demo.is_mouse_plugin = true;
    //         break;
        
    //     case USB_HID_MOUSE_NOTIFY_DISCONNECT:
    //         g_hc_hid_demo.is_mouse_plugin = false;
    //         break;

    //     case USB_HID_KBD_NOTIFY_CONNECT:
    //         fd_h_kbd = open("/dev/usbkbd", O_RDWR);
    //         if(fd_h_kbd < 0){
    //             printf("Error: cannot open /dev/usbmouse\n");
    //             return;
    //         }
    //         ioctl(fd_h_kbd, USBHID_SET_HOOK, usbkbd_hook);
    //         close(fd_h_kbd);
    //         g_hc_hid_demo.is_kbd_plugin = true;
    //         break;
        
    //     case USB_HID_KBD_NOTIFY_DISCONNECT:
    //         g_hc_hid_demo.is_kbd_plugin = false;
    //         break;

    //     default:
    //         return;
    // }


    if(g_hc_hid_demo.is_mouse_plugin && g_hc_hid_demo.is_kbd_plugin){

        /* 配置连接PC的USB 口为 Gadget 模式 */
        hcusb_set_mode(USB_PORT_0, MUSB_PERIPHERAL);

        /* 配置usb gadget HID mode */
        ret = hcusb_gadget_hidg_mouse_kbd_init2(
                get_udc_name(USB_PORT_0),
                g_hc_hid_demo.mouse_report,
                g_hc_hid_demo.kbd_report,
                g_hc_hid_demo.mouse_report,
                g_hc_hid_demo.kbd_report);
        if(ret){
            printf("Cannot config usb as HID Gadget mode !!\n");
            return;
        }

        g_hc_hid_demo.fd_g_mouse = open("/dev/hidg1", O_RDWR);
        if(g_hc_hid_demo.fd_g_mouse < 0){
            printf("Cannot open /dev/hidg1\n");
            return;
        }

        g_hc_hid_demo.fd_g_kbd = open("/dev/hidg0", O_RDWR);
        if(g_hc_hid_demo.fd_g_kbd < 0){
            close(g_hc_hid_demo.fd_g_mouse);
            printf("Cannot open /dev/hidg0\n");
            return;
        }

        printf("==> usb#0 was successfully configured to HID Gadget mode \n");
        g_hc_hid_demo.is_config_gadget = true;
    } 
    else if (g_hc_hid_demo.is_config_gadget == true ) {
        close(g_hc_hid_demo.fd_g_mouse);
        close(g_hc_hid_demo.fd_g_kbd);
        g_hc_hid_demo.fd_g_mouse = -1;
        g_hc_hid_demo.fd_g_kbd = -1;
        g_hc_hid_demo.is_config_gadget = false;

        hcusb_gadget_hidg_deinit();

        hcusb_set_mode(USB_PORT_0, MUSB_HOST);

        printf("==> usb#0 was successfully configured to HOST mode\n"); 
    }
}


static int hc_hid_device_notify(struct notifier_block *self, unsigned long action, void* dev)
{
    unsigned char found = 0;
    char path[64];
    struct usb_hid_dev_info *info = (struct usb_hid_dev_info *)dev;
    struct hidg_func_descriptor *report_desc = info->report;
    int fd_h_mouse = -1;
    int fd_h_kbd = -1;
    int ret;

    switch (action)
    {
        case USB_HID_MOUSE_NOTIFY_CONNECT:
            sprintf(&path[0], "/dev/usbmouse%s", info->devname);
            fd_h_mouse = open(path, O_RDWR);
            if(fd_h_mouse < 0){
                printf("[Error] cannot open mouse (%s)\n", path);
                return -1;
            }
            ioctl(fd_h_mouse, USBHID_SET_HOOK, usbmouse_hook);
            close(fd_h_mouse);

            log_i("usb mouse connet -- %s\n", path);
            dump_report_desc(report_desc);

            g_hc_hid_demo.is_mouse_plugin = true;
            g_hc_hid_demo.mouse_report = report_desc;
        break;
        
        case USB_HID_MOUSE_NOTIFY_DISCONNECT:
            sprintf(&path[0], "/dev/usbmouse%s", info->devname);
            log_i("usb mouse disconnet -- %s\n", path);

            g_hc_hid_demo.is_mouse_plugin = false;
            g_hc_hid_demo.mouse_report = NULL;
        break;

        case USB_HID_KBD_NOTIFY_CONNECT:
            sprintf(&path[0], "/dev/usbkbd%s", info->devname);
            fd_h_kbd = open(path, O_RDWR);
            if(fd_h_kbd < 0){
                printf("[Error] cannot open keyboard (%s)\n", path);
                return -1;
            }
            ioctl(fd_h_kbd, USBHID_SET_HOOK, usbkbd_hook);
            close(fd_h_kbd);

            log_i("usb keyboard connet -- %s\n", path);
            dump_report_desc(report_desc);

            g_hc_hid_demo.is_kbd_plugin = true;
            g_hc_hid_demo.kbd_report = report_desc;
        break;
        
        case USB_HID_KBD_NOTIFY_DISCONNECT:
            sprintf(&path[0], "/dev/usbkbd%s", info->devname);
            log_i("usb keyboard disconnet\n");

            g_hc_hid_demo.is_kbd_plugin = false;
            g_hc_hid_demo.kbd_report = NULL;
        break;

        default:
            return 0;
    }

    work_queue(HPWORK, &g_hc_hid_demo.work, hc_hid_workqueue, (void *)action, 50);
    return NOTIFY_OK;
}


static void hid_dev_prepare(void)
{
    int fd, ret;
    unsigned long action;

    /* Check if the mouse is plugged in */
    fd = open("/dev/usbmouse", O_RDWR);
    if(fd >= 0){        
        ret = ioctl(fd, USBHID_GET_REPORT_DESC, &g_hc_hid_demo.mouse_report);
        if(ret){
            printf("Error : cannot get usb mouse HID report\n");
            return;
        }
        ioctl(fd, USBHID_SET_HOOK, usbmouse_hook);
        g_hc_hid_demo.is_mouse_plugin = true;
        printf("usb mouse connect\n");
        dump_report_desc(g_hc_hid_demo.mouse_report);
        close(fd);
    }

    /* Check if the keyboard is plugged in */    
    fd = open("/dev/usbkbd", O_RDWR);
    if(fd >= 0){
        ret = ioctl(fd, USBHID_GET_REPORT_DESC, &g_hc_hid_demo.kbd_report);
        if(ret){
            printf("Error : cannot get usb mouse HID report\n");
            return;
        }
        ioctl(fd, USBHID_SET_HOOK, usbkbd_hook);
        g_hc_hid_demo.is_kbd_plugin = true;
        printf("usb keyboard connect\n");
        dump_report_desc(g_hc_hid_demo.kbd_report);
        close(fd);
    }

    /* if usb keyboard and mouse is plugged in, setup usb#0 as HID gadget mode */
    if(g_hc_hid_demo.is_mouse_plugin && g_hc_hid_demo.is_kbd_plugin){
        /* 配置连接PC的USB 口为 Gadget 模式 */
        hcusb_set_mode(USB_PORT_0, MUSB_PERIPHERAL);

        /* 配置usb gadget HID mode */
        ret = hcusb_gadget_hidg_mouse_kbd_init2(
                get_udc_name(USB_PORT_0),
                g_hc_hid_demo.mouse_report,
                g_hc_hid_demo.kbd_report,
                g_hc_hid_demo.mouse_report,
                g_hc_hid_demo.kbd_report);
        if(ret){
            printf("Cannot config usb as HID Gadget mode !!\n");
            return;
        }

        g_hc_hid_demo.fd_g_mouse = open("/dev/hidg1", O_RDWR);
        if(g_hc_hid_demo.fd_g_mouse < 0){
            printf("Cannot open /dev/hidg1\n");
            return;
        }

        g_hc_hid_demo.fd_g_kbd = open("/dev/hidg0", O_RDWR);
        if(g_hc_hid_demo.fd_g_kbd < 0){
            close(g_hc_hid_demo.fd_g_mouse);
            printf("Cannot open /dev/hidg0\n");
            return;
        }

        printf("==> usb#0 was successfully configured to HID Gadget mode \n");
        g_hc_hid_demo.is_config_gadget = true;
    }
}


static struct notifier_block hc_hid_hotplug_nb =
{
    .notifier_call = hc_hid_device_notify,
};


int hid_dev_demo_main(int argc, char **argv)
{
    elog_set_filter_tag_lvl("hid_dev", ELOG_LVL_INFO);
    
    memset(&g_hc_hid_demo, 0, sizeof(struct hc_hid_demo_t));
    g_hc_hid_demo.fd_g_kbd = -1;
    g_hc_hid_demo.fd_g_mouse = -1;

    hid_dev_prepare();

    sys_register_notify(&hc_hid_hotplug_nb);
    return 0;
}







/* ********************************************************************************* */

int fd_hidg_mouse0 = -1; 
int fd_hidg_mouse1 = -1; 
int fd_hidg_kbd0 = -1; 
int fd_hidg_kbd1 = -1; 

static int usbmouse0_hook(char *data, int len)
{
    int ret = 0;
    if(len){
        int index;
        char str[128] = {0};
        char *pstr = &str[0];
        pstr += sprintf(pstr, "mouse0 hook(len:%d): ", len);
        for(index = 0; index < len; index++)
            pstr += sprintf(pstr, " %2.2x", (unsigned char)data[index]);
        pstr += sprintf(pstr, "\n");
        log_i("%s", str);

        if(fd_hidg_mouse0 < 0)
            return 0;

        ret = write(fd_hidg_mouse0, data, len);
    }
    return ret;
}

static int usbmouse1_hook(char *data, int len)
{
    int ret = 0;
    if(len){
        int index;
        char str[128] = {0};
        char *pstr = &str[0];
        pstr += sprintf(pstr, "mouse1 hook(len:%d): ", len);
        for(index = 0; index < len; index++)
            pstr += sprintf(pstr, " %2.2x", (unsigned char)data[index]);
        pstr += sprintf(pstr, "\n");
        log_i("%s", str);

        if(fd_hidg_mouse1 < 0)
            return 0;

        ret = write(fd_hidg_mouse1, data, len);
    }
    return ret;
}

static int usbkbd0_hook(char *data, int len)
{
    int ret = 0;
    if(len){
        int index;
        char str[128] = {0};
        char *pstr = &str[0];
        pstr += sprintf(pstr, "keyboard0 hook(len:%d): ", len);
        for(index = 0; index < len; index++)
            pstr += sprintf(pstr, " %2.2x", (unsigned char)data[index]);
        pstr += sprintf(pstr, "\n");
        log_i("%s", str);

        if(fd_hidg_kbd0 < 0)
            return 0;

        ret = write(fd_hidg_kbd0, data, len);
    }
    return ret;
}


static int usbkbd1_hook(char *data, int len)
{
    int ret = 0;
    if(len){
        int index;
        char str[128] = {0};
        char *pstr = &str[0];
        pstr += sprintf(pstr, "keyboard1 hook(len:%d): ", len);
        for(index = 0; index < len; index++)
            pstr += sprintf(pstr, " %2.2x", (unsigned char)data[index]);
        pstr += sprintf(pstr, "\n");
        log_i("%s", str);

        if(fd_hidg_kbd1 < 0)
            return 0;

        ret = write(fd_hidg_kbd1, data, len);
    }
    return ret;
}



int hid_dev_demo2_main(int argc, char **argv)
{
    int fd;
    struct hidg_func_descriptor *mouse0_report;
    struct hidg_func_descriptor *mouse1_report;
    struct hidg_func_descriptor *kbd0_report;
    struct hidg_func_descriptor *kbd1_report;
    int index;
    bool is_mouse0 = false;
    bool is_mouse1 = false;
    bool is_kbd0 = false;
    bool is_kbd1 = false;

    if(argc != 5){
        printf("Error cmd format\n");
        printf("example: \n");
        printf("hid_dev_demo2 /dev/usbmouse0-v1C4Fp0034 /dev/usbkbd0-v1C4Fp0015 /dev/usbmouse1-v0000p3825 /dev/usbkbd1-v1C4Fp0015 \n");
        return -1;
    }

    // elog_set_filter_tag_lvl("hid_dev", ELOG_LVL_INFO);
    // elog_set_filter_tag_lvl("f_hidg", ELOG_LVL_DEBUG);
    // elog_set_filter_tag_lvl("hidg", ELOG_LVL_DEBUG);
 

    if(strstr(argv[1], "usbmouse")){
        fd = open(argv[1], O_RDWR);
        if(fd > 0){
            ioctl(fd, USBHID_SET_HOOK, usbmouse0_hook);
            ioctl(fd, USBHID_GET_REPORT_DESC, &mouse0_report);
            close(fd);
            printf("\n\t mouse 0 report: \n");
            dump_report_desc(mouse0_report);
            is_mouse0 = true;
        }else
            printf("Cannot open mouse(%s)\n", argv[1]);
    }else
        printf("usbmouse 0 is not exit...\n");

    if(strstr(argv[3], "usbmouse")){
        fd = open(argv[3], O_RDWR);
        if(fd > 0){
            ioctl(fd, USBHID_SET_HOOK, usbmouse1_hook);
            ioctl(fd, USBHID_GET_REPORT_DESC, &mouse1_report);
            close(fd);
            printf("\n\t mouse 1 report: \n");
            dump_report_desc(mouse1_report);
            is_mouse1 = true;
        }else
            printf("Cannot open mouse(%s)\n", argv[3]);
    }else
        printf("usbmouse 1 is not exit...\n");

    if(strstr(argv[2], "usbkbd")){
        fd = open(argv[2], O_RDWR);
        if(fd > 0){
            ioctl(fd, USBHID_SET_HOOK, usbkbd0_hook);
            ioctl(fd, USBHID_GET_REPORT_DESC, &kbd0_report);
            close(fd);
            printf("\n\t keyboard 0 report: \n");
            dump_report_desc(kbd0_report);
            is_kbd0 = true;       
        }else
            printf("Cannot open mouse(%s)\n", argv[2]);
    }else
        printf("usbkeyboard 0 is not exit...\n");

    if(strstr(argv[4], "usbkbd")){
        fd = open(argv[4], O_RDWR);
        if(fd > 0){
            ioctl(fd, USBHID_SET_HOOK, usbkbd1_hook);
            ioctl(fd, USBHID_GET_REPORT_DESC, &kbd1_report);
            close(fd);
            printf("\n\t keyboard 1 report: \n");
            dump_report_desc(kbd1_report);
            is_kbd1 = true;             
        }else
            printf("Cannot open mouse(%s)\n", argv[4]);
    }else
        printf("usbkeyboard 2 is not exit...\n");


    /* 配置连接PC的USB 口为 Gadget 模式 */
    hcusb_set_mode(USB_PORT_0, MUSB_PERIPHERAL);

    hcusb_gadget_hidg_mouse_kbd_init2(
            get_udc_name(USB_PORT_0),
            is_mouse0 ? (struct hidg_func_descriptor *)&mouse0_report[0] : NULL,
            is_kbd0 ? (struct hidg_func_descriptor *)&kbd0_report[0] : NULL,
            is_mouse1 ? (struct hidg_func_descriptor *)&mouse1_report[0] : NULL,
            is_kbd1 ? (struct hidg_func_descriptor *)&kbd1_report[0] : NULL);
    

    fd_hidg_kbd0 = open("/dev/hidg0", O_RDWR);
    if(fd_hidg_kbd0 < 0)
        printf("[Error] cannot open /dev/hidg0 (%d)\n", fd_hidg_kbd0);
    
    fd_hidg_kbd1 = open("/dev/hidg2", O_RDWR);
    if(fd_hidg_kbd1 < 0)
        printf("[Error] cannot open /dev/hidg2 (%d)\n", fd_hidg_kbd1);
    
    fd_hidg_mouse0 = open("/dev/hidg1", O_RDWR);
    if(fd_hidg_mouse0 < 0)
        printf("[Error] cannot open /dev/hidg1 (%d)\n", fd_hidg_mouse0);
    
    fd_hidg_mouse1 = open("/dev/hidg3", O_RDWR);
    if(fd_hidg_mouse1 < 0)
        printf("[Error] cannot open /dev/hidg3 (%d)\n", fd_hidg_mouse1);

    printf("===> hid_dev_demo2 finish\n");
    return 0;
}



