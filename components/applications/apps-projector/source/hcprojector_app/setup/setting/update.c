#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <stdint.h>
#include <hcuapi/persistentmem.h>
#include <semaphore.h>
#include "screen.h"
#include "factory_setting.h"
#include "setup.h"
#include "com_api.h"
#include "app_log.h"
#include "mul_lang_text.h"
#include <hcfota.h>
#include "app_config.h"
#include <sys/stat.h>
#include "osd_com.h"
#include <pthread.h>
#include <hcuapi/sysdata.h>
#ifdef LVGL_RESOLUTION_240P_SUPPORT
    #define UPGRADE_WIDGET_WIDTH_PCT 45
    #define UPGRADE_WIDGET_HEIGHT_PCT 33
#else
    #define UPGRADE_WIDGET_WIDTH_PCT 42
    #define UPGRADE_WIDGET_HEIGHT_PCT 25
#endif

enum{
    CHECK_OK = 0,
    CHECK_NO_FILE,
    CHECK_FILE_ERR,
    CHECK_PRODUCT_ERR,
    CHECK_VERSION_ERR,
};

extern lv_timer_t *timer_setting, *timer_setting_clone;
extern lv_font_t* select_font_normal[3];
extern lv_obj_t* slave_scr_obj;
static long progress = -1;
static char* updata_url;
lv_timer_t *timer_update;
lv_obj_t *prompt_label = NULL;
lv_obj_t *prompt_label1 = NULL;
lv_obj_t *progress_bar = NULL;
screen_ctrl prev_ctrl = NULL;
char urls[160]={0};

#ifdef USB_AUTO_UPGRADE
volatile int m_usb_upgrade = 0;
static sem_t sys_upg_usb_check_sem;
static sem_t find_software_sem;
static sem_t *find_software_sem_p;
static void upgrade_prompt_msg_box();
#endif

extern void set_remote_control_disable(bool b);
extern int mmp_get_usb_stat();
static int find_software(DIR *d,  char str[160]);
static int hcfota_report(hcfota_report_event_e event, unsigned long param, unsigned long usrdata);
static void* software_update_handler(void *target_);
static void timer_update_handle(lv_timer_t *t);
static void software_update_event_handle(lv_event_t *e);
static void reboot_timer_handle(lv_timer_t *t);
static void software_update_widget_(char* urls);
static void upgrade_entry_init(void);

static int version_check(char* file_name){
    int ret = CHECK_NO_FILE;
    struct hcfota_header fw_header = { 0 };
    FILE *fp = NULL;
    int header_len = sizeof(struct hcfota_header);
    int file_len;
    int len;

    fp = fopen(file_name, "rb+");
    if (fp == NULL) {
        log(DEMO, INFO, "%s fopen:%s error.\n", __func__, file_name);
        return CHECK_FILE_ERR;
    }

    fseek(fp, 0, SEEK_END);
    file_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_len > header_len) {
        int read_len = header_len;
        int red_cnt = 0;
        while (read_len) {
            len = fread(&fw_header + red_cnt, 1, read_len, fp);
            if (len <= 0) {
                break;
            }

            red_cnt += len;
            read_len -= len;
        }

        if (red_cnt != header_len) {
            log(DEMO, INFO, "%s fw header len not match.\n", __func__);
            fclose(fp);
            return CHECK_FILE_ERR;
        }

    } else {
        log(DEMO, INFO, "%s fw header len error.\n", __func__);
        fclose(fp);
        return CHECK_FILE_ERR;
    }

    fclose(fp);

    log(DEMO, INFO, "local product:%s, fw product:%s\n", (char *)projector_get_some_sys_param(P_DEV_PRODUCT_ID), (char *)fw_header.board);
    log(DEMO, INFO, "local version:%u, fw version:%u\n", projector_get_some_sys_param(P_DEV_VERSION), (unsigned int)fw_header.version);

    char product_id[16] = { 0 };
    snprintf(product_id, sizeof(product_id), "%s", (char *)projector_get_some_sys_param(P_DEV_PRODUCT_ID));
    if (memcmp(product_id, fw_header.board, sizeof(fw_header.board)) == 0) {
        if (
            fw_header.ignore_version_check || 
            (fw_header.version != projector_get_some_sys_param(P_DEV_VERSION))
            ) {
            log(DEMO, INFO, "%s(), line: %d. found upg file:%s!\n", __func__, __LINE__, file_name);
            ret = CHECK_OK;
        }else{
            ret = CHECK_VERSION_ERR;
        }

    }else{
        ret = CHECK_PRODUCT_ERR;
    }
    return ret;
}


static int find_software(DIR *d,  char str[160]){
    struct stat sd;
    int ret = CHECK_NO_FILE;
    char file_name[160] = { 0 };

    memset(file_name, 0, sizeof(file_name));
    snprintf(file_name, sizeof(file_name), "%s/%s", mmp_get_usb_dev_name(), BR2_EXTERNAL_HCFOTA_FILENAME);
    if (!stat(file_name, &sd)) {
        ret = version_check(file_name);
        if (CHECK_OK == ret){
            memcpy(str, file_name, 160);
            log(DEMO, INFO, "update file: %s\n", str);
        }
    }else{
        ret = CHECK_NO_FILE;
    }
    return ret;
}

static int hcfota_report(hcfota_report_event_e event, unsigned long param, unsigned long usrdata){
    if(event == HCFOTA_REPORT_EVENT_UPGRADE_PROGRESS){
        progress = param;
        control_msg_t msg = {0};
        msg.msg_type = MSG_TYPE_UPG_BURN_PROGRESS;
        api_control_send_msg(&msg);

    }else if(event == HCFOTA_REPORT_EVENT_DOWNLOAD_PROGRESS){
        progress = param;
        control_msg_t msg = {0};
        msg.msg_type = MSG_TYPE_UPG_DOWNLOAD_PROGRESS;
        api_control_send_msg(&msg);
    }
    return 0;
}

#define REBOOT_LATER 4

static void update_failed_timer_handle(lv_timer_t *t){
    if(prompt_label && lv_obj_is_valid(prompt_label->parent)){
        lv_obj_del(prompt_label->parent);
        turn_to_setup_root();
    }
}

static int set_ota_detect_mode(unsigned long mode){
	int fd;
	struct persistentmem_node node;
	struct sysdata sysdata = { 0 };

	fd = open("/dev/persistentmem", O_SYNC | O_RDWR);
	if (fd < 0) {
		log(DEMO, INFO, "open /dev/persistentmem failed\n");
		return -1;
	}

	sysdata.ota_detect_modes = mode;
	node.id = PERSISTENTMEM_NODE_ID_SYSDATA;
	node.offset = offsetof(struct sysdata, ota_detect_modes);
	node.size = sizeof(sysdata.ota_detect_modes);
	node.buf = &sysdata.ota_detect_modes;
	if (ioctl(fd, PERSISTENTMEM_IOCTL_NODE_PUT, &node) < 0) {
		log(DEMO, INFO, "put sysdata failed\n");
		close(fd);
		return -1;
	}

	return 0;
}

static void update_control(void* arg1, void* arg2){
    (void)arg2;
    if (prev_ctrl) {
        prev_ctrl(arg1, arg2);
    }
     control_msg_t *ctl_msg = (control_msg_t*)arg1;
     switch (ctl_msg->msg_type)
     {
     case MSG_TYPE_UPG_STATUS:
        switch (ctl_msg->msg_code)
        {
        case UPG_STATUS_BURN_FAIL:
            lv_label_set_text(prompt_label, api_rsc_string_get(STR_UPGRADE_ERR));
            break;
        case UPG_STATUS_VERSION_IS_OLD:
            lv_label_set_text(prompt_label, api_rsc_string_get(STR_UPGRADE_VERSION_ERR));
            break;
        case UPG_STATUS_USB_READ_ERR:
            lv_label_set_text(prompt_label, api_rsc_string_get(STR_UPGRADE_LOAD_ERR));
            break;
        case UPG_STATUS_FILE_CRC_ERROR:
            lv_label_set_text(prompt_label, api_rsc_string_get(STR_UPGRADE_ERR));
            break;
        case UPG_STATUS_FILE_UNZIP_ERROR:                
            lv_label_set_text(prompt_label, api_rsc_string_get(STR_UPGRADE_DECO_ERR));
            break;
        case UPG_STATUS_BURN_OK:
            lv_label_set_text_fmt(prompt_label,"%s %d%s", api_rsc_string_get(STR_UPGRADE_SUCCESS_MSG1), REBOOT_LATER,api_rsc_string_get(STR_UPGRADE_SUCCESS_MSG2));
            lv_timer_t* reboot_timer = lv_timer_create(reboot_timer_handle, 1000, NULL);
            lv_timer_reset(reboot_timer);
            lv_timer_set_repeat_count(reboot_timer, REBOOT_LATER);
            break;
        default:
            break;
        }
        if(ctl_msg->msg_code != UPG_STATUS_BURN_OK){
            if(timer_setting_clone){
                lv_timer_resume(timer_setting_clone);
                lv_timer_reset(timer_setting_clone);
                timer_setting = timer_setting_clone;
                timer_setting_clone = NULL;
            }
            lv_obj_add_flag(prompt_label1, LV_OBJ_FLAG_HIDDEN);
            lv_timer_t *timer = lv_timer_create(update_failed_timer_handle, 3000, NULL);
            lv_timer_set_repeat_count(timer, 1);
            lv_timer_reset(timer);            
        }
        if(timer_update){
            lv_timer_pause(timer_update);
            lv_timer_del(timer_update);
            timer_update = NULL;
        }
        break;
    case MSG_TYPE_UPG_DOWNLOAD_PROGRESS:
    case  MSG_TYPE_UPG_BURN_PROGRESS:
        if(ctl_msg->msg_type ==  MSG_TYPE_UPG_DOWNLOAD_PROGRESS){
            lv_label_set_text(prompt_label, api_rsc_string_get(STR_UPGRADE_DOWNLOADING));
        }else{
            lv_label_set_text(prompt_label, api_rsc_string_get(STR_UPGRADEING));            
        }

        static unsigned long prev_progress = 0;
        if(prev_progress != progress){
            lv_bar_set_value(progress_bar, progress, LV_ANIM_OFF);
            char ss[5];
            memset(ss, 0, 4);
            sprintf(ss, "%ld%%", progress);
            lv_label_set_text(lv_obj_get_child(progress_bar, 0), ss);  
            prev_progress = progress;     
        }
        break;
    case MSG_TYPE_USB_UPGRADE:
        software_update_widget_(urls);
        break;
     default:
        break;
     }
}

static void reboot_timer_handle(lv_timer_t *t){
    static int count = REBOOT_LATER-1;
    if(count--){
        lv_label_set_text_fmt(prompt_label,"%s %d%s", api_rsc_string_get(STR_UPGRADE_SUCCESS_MSG1), count,api_rsc_string_get(STR_UPGRADE_SUCCESS_MSG2));
    }else{
       if(set_ota_detect_mode(HCFOTA_REBOOT_OTA_DETECT_NONE)<0){
            log(DEMO, INFO, "set ota detect mode failed\n");
       }
        api_system_reboot();
    }
}

static void* software_update_handler(void* param){//
    (void)param;
    set_remote_control_disable(true);
    if(set_ota_detect_mode(HCFOTA_REBOOT_OTA_DETECT_USB_DEVICE)<0){
        log(DEMO, INFO, "set ota detect mode failed\n");
    }
    int rc =hcfota_url(updata_url, hcfota_report, 0);
    control_msg_t msg = {0};
    msg.msg_type = MSG_TYPE_UPG_STATUS;     
    if(rc==0){
        msg.msg_code = UPG_STATUS_BURN_OK;        
    }else{
        lv_mem_free(updata_url);
        set_remote_control_disable(false);      
        switch (rc){
            case HCFOTA_ERR_LOADFOTA:
                log(DEMO, INFO, "load file err!");
                msg.msg_code = UPG_STATUS_USB_READ_ERR;
                break;
            case HCFOTA_ERR_HEADER_CRC:
                log(DEMO, INFO, "header crc err!");
                msg.msg_code = UPG_STATUS_FILE_CRC_ERROR;
                break;
            case HCFOTA_ERR_VERSION:
                log(DEMO, INFO, "version err!");
                msg.msg_code = UPG_STATUS_VERSION_IS_OLD;
                break;
            case HCFOTA_ERR_DECOMPRESSS:
                log(DEMO, INFO, "decompress err!");
                msg.msg_code = UPG_STATUS_FILE_UNZIP_ERROR;
                break;
            case HCFOTA_ERR_UPGRADE:
                log(DEMO, INFO, "upgrade err");
                msg.msg_code = UPG_STATUS_BURN_FAIL;
            default:
                break;
        }
        #ifdef USB_AUTO_UPGRADE
            m_usb_upgrade = 0;
        #endif
    }
    api_control_send_msg(&msg);
    return NULL;
}


static void software_update_event_handle(lv_event_t *e){
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    if(code == LV_EVENT_KEY){
        uint16_t key = lv_indev_get_key(lv_indev_get_act());
        if(key == LV_KEY_ESC){
            lv_obj_del(target);
            turn_to_setup_root();
        }else if(key == LV_KEY_HOME){
            lv_obj_del(target);
            turn_to_setup_root();
        }
    }
}


void software_update_widget(lv_obj_t* btn){
    int check_ret;
    uint8_t *err_str = NULL;
    lv_obj_t *obj = lv_obj_create(setup_slave_root);
    slave_scr_obj = obj;
    lv_obj_set_style_radius(obj, 20, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_group_add_obj(setup_g, obj);
    lv_group_focus_obj(obj);
    lv_obj_add_event_cb(obj, software_update_event_handle, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(obj, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_size(obj,LV_PCT(25),LV_PCT(25));
    lv_obj_center(obj);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *label;
    DIR *d = opendir(MOUNT_ROOT_DIR"/");
    int usb_status = mmp_get_usb_stat();
   if(usb_status == USB_STAT_UNMOUNT || usb_status == USB_STAT_INVALID){
       label = lv_label_create(obj);
       lv_obj_center(label);
       lv_label_set_recolor(label, true);
       lv_label_set_text(label, api_rsc_string_get(STR_UPGRADE_NO_DEV));
       lv_obj_set_style_text_font(label, osd_font_get(FONT_MID), 0);
       lv_obj_set_style_text_color(label, lv_color_white(), 0);
        if(timer_setting){
            lv_timer_reset(timer_setting);
            lv_timer_resume(timer_setting);
        }
       
       return;
   }  
    check_ret = find_software(d, urls);
	if (CHECK_OK != check_ret) {
		label = lv_label_create(obj);
        if (CHECK_FILE_ERR == check_ret)
            err_str = api_rsc_string_get(STR_UPGRADE_LOAD_ERR);        
        else if (CHECK_PRODUCT_ERR == check_ret)
            err_str = api_rsc_string_get(STR_UPGRADE_PRODUCT_ERR);        
        else if (CHECK_VERSION_ERR == check_ret)
            err_str = api_rsc_string_get(STR_UPGRADE_VERSION_ERR);        
        else
            err_str = api_rsc_string_get(STR_UPGRADE_NO_SOFTWATE);        

		lv_obj_center(label);
        lv_obj_set_size(label, LV_PCT(100), LV_PCT(45));
		lv_label_set_recolor(label, true);
		lv_label_set_text(label, err_str);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER,0);
        lv_obj_set_style_text_color(label, lv_color_white(),0);
		lv_obj_set_style_text_font(label, osd_font_get(FONT_MID), 0);
        if(timer_setting){
            lv_timer_reset(timer_setting);
            lv_timer_resume(timer_setting);
        }
       return;
   }
    lv_obj_del(obj);
    upgrade_entry_init();
    control_msg_t msg = {0};
    msg.msg_type = MSG_TYPE_USB_UPGRADE;
    api_control_send_msg(&msg);
}

static void software_update_widget_(char* urls){
    updata_url = lv_mem_alloc(strlen(urls)+1);
    memcpy(updata_url, urls, strlen(urls)+1);
    lv_obj_t *obj1 = NULL;
    if(lv_scr_act() == setup_scr){
        obj1 = lv_obj_create(setup_slave_root);
    }else{
        obj1 = lv_obj_create(lv_layer_top());
    }
    
    lv_obj_set_style_text_color(obj1, lv_color_white(), 0);
    lv_obj_set_style_outline_width(obj1, 0, 0);
    lv_obj_set_style_border_width(obj1, 0, 0);
    lv_obj_set_style_bg_color(obj1, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_scrollbar_mode(obj1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(obj1, 20, 0);
    lv_obj_set_size(obj1,LV_PCT(UPGRADE_WIDGET_WIDTH_PCT),LV_PCT(UPGRADE_WIDGET_HEIGHT_PCT));
    lv_obj_center(obj1);

    lv_obj_t *bar = lv_bar_create(obj1);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_size(bar, LV_PCT(100), LV_PCT(20));
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_center(bar);
    progress_bar = bar;

    lv_obj_t* label = lv_label_create(bar);
    lv_label_set_text(label, "0%");
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
   
    prompt_label = lv_label_create(obj1);
    lv_label_set_text(prompt_label, api_rsc_string_get(STR_UPGRADE_DOWNLOADING));
    lv_obj_set_width(prompt_label, LV_PCT(100));
    lv_obj_set_style_text_font(prompt_label, osd_font_get(FONT_MID), 0);
    lv_obj_align_to(prompt_label, bar, LV_ALIGN_OUT_TOP_MID, 0, -3);
    lv_obj_set_style_text_align(prompt_label, LV_TEXT_ALIGN_CENTER, 0);
    
    label = lv_label_create(obj1);
    lv_label_set_text(label, api_rsc_string_get(STR_UPGRADE_NOT_POWER_OFF));
    lv_obj_set_style_text_font(label, osd_font_get(FONT_MID), 0);
    lv_obj_align_to(label, bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    prompt_label1 = label;

    if(timer_setting){
        lv_timer_pause(timer_setting);
        timer_setting_clone = timer_setting;
        timer_setting = NULL;
    }

	pthread_t thread_id = 0;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 0x1000);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread_id, &attr, software_update_handler, NULL);
	pthread_attr_destroy(&attr);
}

static void upgrade_entry_init(void){
	screen_entry_t *update_entry = api_screen_get_ctrl_entry(lv_scr_act());
	bool new_entry = false;
	screen_entry_t entry_temp;
	if (!update_entry) {
		new_entry = true;
		update_entry = &entry_temp;
		update_entry->screen = lv_scr_act();
	} else {
		prev_ctrl = update_entry->control;
	}

	update_entry->control = update_control;
	if (new_entry) {
		api_screen_regist_ctrl_handle(update_entry);
	}
}

#ifdef USB_AUTO_UPGRADE

static int find_soft = 0;
static bool sys_upg_usb_task_running = 0;
static void *sys_upg_usb_task(void *arg){
	sys_upg_usb_task_running = 1;
	while (1) {
		sem_wait(&sys_upg_usb_check_sem);

		DIR *d = opendir(MOUNT_ROOT_DIR);
		if (CHECK_OK == find_software(d, urls)) {
			find_soft = 1;
			upgrade_entry_init();
			control_msg_t msg = { 0 };
			msg.msg_type = MSG_TYPE_USB_UPGRADE;
			api_control_send_msg(&msg);
            sem_post(&find_software_sem);
            break;
		}
		sem_post(&find_software_sem);
	}
}

int sys_upg_usb_check_notify(void){
	while (!sys_upg_usb_task_running) {
		usleep(1000);
	}
	sem_post(&sys_upg_usb_check_sem);
    if(find_software_sem_p){
        sem_wait(find_software_sem_p);
        if(find_soft && find_software_sem_p){
            sem_destroy(find_software_sem_p);
            find_software_sem_p = NULL;
        }        
    }
	return find_soft;
}

int sys_upg_usb_check_init(void){
	pthread_t thread_id = 0;
	pthread_attr_t attr;

	if (m_usb_upgrade) {
		log(DEMO, INFO, "%s(), line: %d. usb uprade task is running!\n", __func__, __LINE__);
		return API_SUCCESS;
	}
	sem_init(&sys_upg_usb_check_sem, 0, 0);
	sem_init(&find_software_sem, 0, 0);
    find_software_sem_p = &find_software_sem;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 0x2000);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&thread_id, &attr, sys_upg_usb_task, NULL)) {
		return API_FAILURE;
	}
	m_usb_upgrade = 1;
	return API_SUCCESS;
}
#endif