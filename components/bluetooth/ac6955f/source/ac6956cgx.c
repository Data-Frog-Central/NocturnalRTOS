#define LOG_TAG "BLUETOOTH"
#define ELOG_OUTPUT_LVL  ELOG_LVL_DEBUG

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <kernel/io.h>
#include <kernel/types.h>
#include <kernel/vfs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include <kernel/lib/console.h>
#include <kernel/elog.h>
#include <kernel/lib/fdt_api.h>
#include <kernel/module.h>
#include <kernel/drivers/hc_clk_gate.h>
#include <hcuapi/sci.h>
#include <time.h>
#include <signal.h>
#include <kernel/completion.h>
#include "bluetooth.h"
#include <kernel/delay.h>

#include <hcuapi/input-event-codes.h>
// #include "ac6956cgx.h"
#include <semaphore.h>
#include "bluetooth_io.h"


// bt io cmd 
typedef enum _E_HC_BT_CMD
{
	CMD_SET_BT_CMD_ACK=0x00,
	CMD_SET_BT_POWER_ON=0x02,
	CMD_SET_BT_POWER_OFF=0x03,
	CMD_REPORT_CONNECT_RESULTS=0x04,
	CMD_REPORT_DISCONNECT_RESULTS=0x05,
	CMD_SET_INQUIRY_START=0x06,
	CMD_SET_INQUIRY_STOP=0x07,
	CMD_SET_CONNECT=0x08,
	CMD_SET_DISCONNECT=0x09,
	CMD_REPORT_CONNECT_STATUS=0x0a,
	CMD_REPORT_INQUIRY_COMPLETE=0x0b,
	CMD_REPORT_INQUIRY_RESULTS=0x0c,
	CMD_SET_BT_MUSIC_VOL_VALUE=0x0e,
	CMD_SET_AUDIO_CHANNEL_INPUT_SELECT=0x11,
	CMD_GET_AUDIO_CHANNEL_INPUT_SELECT=0x12,
	CMD_REPORT_AUDIO_CHANNEL_INPUT_SELECT=0x13,
	CMD_SET_DELETE_LAST_DEVICE=0x20,
	CMD_SET_DELETE_ALL_DEVICE=0x21,
	CMD_SET_MEMORY_CONNECTION=0x23, //not support
	CMD_GET_BT_CONNECT_STATE=0x44,
	CMD_SET_BT_CTRL_CMD=0x4a,
	CMD_GET_BT_MUSIC_VOL_RESULTS=0x4b,
	CMD_REPORT_BT_MUSIC_VOL_RESULTS=0x4c,
	// HC DEFINE CMD
	CMD_SET_BT_PIN_FUNCTION = 0xe0,
	CMD_SET_BT_GPIO_POL= 0xe1,
	CMD_REPORT_BT_GPI_STAT= 0xe2,
	CMD_SET_BT_IR_PROTOCAL= 0xe3, //unspport
	CMD_REPORT_BT_IR_KEY = 0xe4,
	CMD_REPORT_BT_ADC_KEY=0xe5,
	// 0xe6??
	CMD_SET_BT_IR_POWER_KEY=0xe7,
	CMD_SET_BT_FM=0xe8,
	CMD_SET_IR_USERCODE=0xe9,
	CMD_SET_BT_POWER_PINPAD_OFF=0xea,
	CMD_SET_BT_PWM_PARAM=0xeb,
}bt_cmds_e;

typedef struct {
	uint32_t report_code;
	uint16_t keycode;
}bt_keymap_table_t;

// bt adckey report keycode map
static bt_keymap_table_t adc_keymap_table[]={
	{0x00,KEY_POWER},
	{0X01,KEY_LEFT},
	{0x02,KEY_RIGHT},
	{0X03,KEY_EXIT},
	{0x04,KEY_DOWN},
	{0X05,KEY_UP},
	{0X06,KEY_OK},
};
// bt irkey report keycode map
static bt_keymap_table_t ir_keymap_table[]={
	
	/*  0x1c   0x3   0x42  0x8  *
	 * POWER  AUDIO  SAT   MUTE *
	 *                          */
	{ 0x1c, KEY_POWER },
	{ 0x03, KEY_AUDIO },
	{ 0x42, KEY_SAT   },
	{ 0x08, KEY_MUTE  },

	/*  0x55    0x51       0x5e *
	 * ZOOM    TIMESHIFT   SUB  *
	 *                          */
	{ 0x55, KEY_ZOOM     },
	{ 0x51, KEY_TIME     },
	{ 0x5e, KEY_SUBTITLE },

	/*  0x5a    0x52     0x5d *
	 * TV/RADIO TTX   FILELIST  *
	 *                          */
	{ 0x5a, KEY_TV   },
	{ 0x52, KEY_TEXT },
	{ 0x5d, KEY_LIST },

	/*  0x18           0x17  *
	 * MENU           EXIT   *
	 *                       */
	{ 0x18, KEY_MENU },
	{ 0x17, KEY_EXIT },

	/*          0x1a          *
	 *           Up           *
	 *                        *
	 *  0x47    0x06    0x07  *
	 *  Left     Ok     Right *
	 *                        *
	 *         0x48           *
	 *         Down           *
	 *                        */
	{ 0x47, KEY_LEFT  },
	{ 0x1a, KEY_UP    },
	{ 0x07, KEY_RIGHT },
	{ 0x48, KEY_DOWN  },
	{ 0x06, KEY_OK },

	/*  0x49           0xa  *
	 * EPG           INFO   *
	 *                      */
	{ 0x49, KEY_EPG  },
	{ 0x0a, KEY_INFO },

	/*  0x54    0x16    0x15  *
	 *   1       2       3    *
	 *                        *
	 *  0x50    0x12    0x11  *
	 *   4       5       6    *
	 *                        *
	 *  0x4c    0xe    0xd    *
	 *   7       8       9    *
	 *                        */
	{ 0x54, KEY_NUMERIC_1 },
	{ 0x16, KEY_NUMERIC_2 },
	{ 0x15, KEY_NUMERIC_3 },
	{ 0x50, KEY_NUMERIC_4 },
	{ 0x12, KEY_NUMERIC_5 },
	{ 0x11, KEY_NUMERIC_6 },
	{ 0x4c, KEY_NUMERIC_7 },
	{ 0x0e, KEY_NUMERIC_8 },
	{ 0x0d, KEY_NUMERIC_9 },

	/*  0x10    0x41    0xc  *
	 * RECALL    FAV      0  *
	 *                       */
	{ 0x10, KEY_AGAIN     },
	{ 0x41, KEY_FAVORITES },
	{ 0x0c, KEY_NUMERIC_0 },


	/*  0x09       0x05        0x4b   0x4f *
	 *  LEFTSHIFT RIGHTSHIFT PREVIOUS NEXT *
	 *                                     */
	{ 0x09, KEY_LEFTSHIFT  },
	{ 0x05, KEY_RIGHTSHIFT },
	{ 0x4b, KEY_PREVIOUS   },
	{ 0x4f, KEY_NEXT       },

	/*  0x01  0x5f  0x19  0x58 *
	 *  PLAY PAUSE STOP RECORD *
	 *                         */
	{ 0x01, KEY_PLAY   },
	{ 0x5f, KEY_PAUSE  },
	{ 0x19, KEY_STOP   },
	{ 0x58, KEY_RECORD },

	/*  0x56  0x57  0x1f  0x5b *
	 *  RED  GREEN YELLO BLUE  *
	 *                         */
	{ 0x56, KEY_RED    },
	{ 0x57, KEY_GREEN  },
	{ 0x1f, KEY_YELLOW },
	{ 0x5b, KEY_BLUE   },

	/*  0x14              0x13      *
	 *  KEY_VOLUMEUP KEY_VOLUMEDOWN *
	 *                              */
	{ 0x14, KEY_VOLUMEUP    },
	{ 0x13, KEY_VOLUMEDOWN  },
};

static bluetooth_ir_control_t bt_control;
int bluetooth_ir_key_init(bluetooth_ir_control_t control)
{
	if(control == NULL)
		return BT_RET_ERROR;

	bt_control = control;
	log_d("%s %d\n",__func__,__LINE__);
	return BT_RET_SUCCESS;
}

/*
function: Bluetooth sending infrared button
examples: bluetooth_ir_key_send(KEY_DOWN);
*/
int bluetooth_ir_key_send(unsigned short code)
{
	struct input_event_bt event_key = {0};
	if(bt_control == NULL)
		return BT_RET_ERROR;
	event_key.type = EV_KEY;
	event_key.value = 1;
	event_key.code = code;
	bt_control(event_key);
	log_d("%s %d\n",__func__,__LINE__);
	return BT_RET_SUCCESS;
}

int bluetooth_input_event_send(struct input_event_bt event_key)
{
	if(bt_control == NULL)
		return BT_RET_ERROR;
	event_key.type = EV_KEY;
	bt_control(event_key);
	log_d("%s %d\n",__func__,__LINE__);
	return BT_RET_SUCCESS;
}



int bluetooth_set_gpio_backlight(unsigned char value)
{
	bt_pwm_param_t lcd_backlight={0};	
	lcd_backlight.pinpad=PINPAD_BT_PB2;
	lcd_backlight.type=0;//freq
	lcd_backlight.value=10;//10Khz
	bluetooth_ioctl(BLUETOOTH_SET_PWM_PARAM,&lcd_backlight);
	if(value){
		// set duty pwm polarity is false.set output level high
		lcd_backlight.pinpad=PINPAD_BT_PB2;
		lcd_backlight.type=1;//duty
		lcd_backlight.value=0;
		bluetooth_ioctl(BLUETOOTH_SET_PWM_PARAM,&lcd_backlight);		
	}else{
		// set duty pwm polarity is false.set output level low
		lcd_backlight.pinpad=PINPAD_BT_PB2;
		lcd_backlight.type=1;//duty
		lcd_backlight.value=100;
		bluetooth_ioctl(BLUETOOTH_SET_PWM_PARAM,&lcd_backlight);
	}
	return BT_RET_SUCCESS;
}

int bluetooth_set_cvbs_aux_mode(void)
{
	return 0;
}
int bluetooth_set_cvbs_fiber_mode(void)
{
	return 0;
}

int bluetooth_set_music_vol(unsigned char val)
{
	return 0;
}

int bluetooth_set_connection_cvbs_aux_mode(void)
{
	return 0;
}

int bluetooth_set_connection_cvbs_fiber_mode(void)
{
	return 0;
}

int bluetooth_power_on_to_rx(void)
{
	return 0;
}
int bluetooth_factory_reset(void)
{
	return 0;
}

#ifdef BR2_PACKAGE_BLUETOOTH_FAKE
int bluetooth_init(const char *uart_path, bluetooth_callback_t callback)
{
	return 0;
}
int bluetooth_deinit(void)
{
	return 0;
}
int bluetooth_poweron(void)
{
	return 0;
}
int bluetooth_poweroff(void)
{
	return 0;
}
int bluetooth_scan(void)
{
	return 0;
}
int bluetooth_stop_scan(void)
{
	return 0;
}
int bluetooth_connect(unsigned char *mac)
{
	return 0;
}
int bluetooth_is_connected(void)
{
	return 0;
}
int bluetooth_disconnect(void)
{
	return 0;
}
int bluetooth_del_all_device(void)
{
	return 0;
}

int bluetooth_del_list_device(void)
{
	return 0;
}

int bluetooth_memory_connection(unsigned char value)
{
	return 0;
}
int bluetooth_set_gpio_mutu(unsigned char value)
{
	return 0;
}

int bluetooth_ioctl(int cmd, void* arg)
{
	return 0;
}
void bt_module_config(void)
{
	return ;
}

#else
#define BT_RET_EXIT     1
#define BT_DATA_AGAIN	2
#define BT_ERROR_DATA		0x100
#define BT_ERROR_DATA_OFFSET_1	0x101
#define BT_ERROR_DATA_OFFSET_2	0x102
#define BT_ERROR_DATA_MAX		0x103
#define UART_RX_RECEIVE_MAX_BUFF 1024
#define UART_TX_WRITE_MAX_BUFF 256

#define AC6956C_MAX_WRITE_SIZE          128
#define BLUETOOTH_DATE_TIMEOUT 1000
#define BLUETOOTH_MINIMUM_CHECKSUM 7
#define BLUETOOTH_MAX_VOLUME_RANGE 31



typedef enum _E_BT_DEVICE_STATUS_
{
	EBT_DEVICE_STATUS_NOWORKING_DEFAULT=0,
	EBT_DEVICE_STATUS_NOWORKING_STATUS_ERROR,
	EBT_DEVICE_STATUS_NOWORKING_BT_POWER_OFF,
	EBT_DEVICE_STATUS_NOWORKING_NOT_EXISTENT,
	EBT_DEVICE_STATUS_WORKING_EXISTENT,
	EBT_DEVICE_STATUS_WORKING_INQUIRY_DATA_SEARCHED,
	EBT_DEVICE_STATUS_WORKING_INQUIRY_STOP_SEARCHING,
	EBT_DEVICE_STATUS_WORKING_DISCONNECTED,
	EBT_DEVICE_STATUS_WORKING_CONNECTED,
	EBT_DEVICE_STATUS_WORKING_GET_CONNECTED_INFO,
}bt_device_status_e;

struct AC6956C_MessageBody{
	unsigned short  frame_head;
	unsigned short  frame_len;
	unsigned char   frame_id;
	unsigned char   cmd_id;
	unsigned char   cmd_len;
	unsigned char   cmd_value[AC6956C_MAX_WRITE_SIZE];
	unsigned short  Frame_checksum;
};
struct bt_ac6956c_priv {
	int uartfd;
	int cref;
	bluetooth_callback_t callback;
	struct bluetooth_slave_dev inquiry_info;
	struct bluetooth_slave_dev connet_info;
	bt_device_status_e get_dev_status;
	bt_cmds_e bt_cmds;
	bt_gpio_set_t bt_pinpad_level;
	uint8_t volume;
};


static void bt_ac6956c_cp_connet_info(struct bluetooth_slave_dev *connet_cmds,struct AC6956C_MessageBody *obj);
static int bt_ac6956c_messagebody_switch_str(struct AC6956C_MessageBody *body,unsigned char *buf);
static void bt_ac6956c_read_thread(void *args);
static void bt_ac6956c_set_poll_timeout(int cmds,int *poll_t);
static void bt_ac6956c_serial_data_judg(bt_cmds_e cmd,char *buf,unsigned int count);
static int bt_ac6956c_set_action(bt_cmds_e cmd,unsigned char *send_buf);
static int str_switch_bt_ac6956c_messagebody(char *buf,unsigned int *count,struct AC6956C_MessageBody *body);
static void printf_bt_ac6956c_dev_status(bt_device_status_e status);
static bt_device_status_e bt_get_device_sys_status(void);
static struct completion bt_ac6956c_task_completion;
static int bt_ac6956c_task_start_flag = 0;
static struct bt_ac6956c_priv *gbt = NULL;
static int bt_get_device_sys_connet_info(struct bluetooth_slave_dev *con_data);
static int str_with_ac6956c_messagebody_compare(char *buf,unsigned char count);
static uint16_t bt_reportkey_mapping(bt_keytype_e keytype,uint32_t btcode);
static sem_t bluetooth_cmds_sem;


int bluetooth_init(const char *uart_path, bluetooth_callback_t callback)
{
	int fd;
	struct sci_setting bt_ad6956f_sci;
	int ret = BT_RET_SUCCESS;
	if(gbt==NULL)
	{
		gbt = (struct bt_ac6956c_priv *)malloc(sizeof(struct bt_ac6956c_priv));
		if(gbt==NULL)goto error;
		memset(gbt,0,sizeof(struct bt_ac6956c_priv));
	}
	if (gbt->cref == 0) {
		/* init bt */
		if(uart_path==NULL)
		{
			goto null_error;  
		}

		fd = open(uart_path,O_RDWR);
		if(fd<0)
		{
			log_e("open %s error %d\n",uart_path,fd);
			goto uart_error;
		}
		bt_ad6956f_sci.parity_mode=PARITY_NONE;
		bt_ad6956f_sci.bits_mode=bits_mode_default;
		ioctl(fd, SCIIOC_SET_BAUD_RATE_115200, NULL);
		
		ioctl(fd, SCIIOC_SET_SETTING, &bt_ad6956f_sci);
		
		/* create task */
		log_d("gbt->uartfd = %d\n",fd);
		gbt->uartfd = fd;
		bt_ac6956c_task_start_flag = 0;
		init_completion(&bt_ac6956c_task_completion);
		sem_init(&bluetooth_cmds_sem, 0, 0);
		ret = xTaskCreate(bt_ac6956c_read_thread , "bt_ac6956c_read_thread" ,
						  0x1000 , &gbt->bt_cmds , portPRI_TASK_NORMAL , NULL);

		if(ret != pdTRUE)
		{
			log_e("kshm recv thread create failed\n");
			goto taskcreate_error;
		}
		/* init success */
		gbt->callback = callback;
		gbt->cref++;
	} else {
		gbt->cref++;
	}

	/* SUCCESS */
	return BT_RET_SUCCESS;

taskcreate_error:
	bt_ac6956c_task_start_flag=1;
	wait_for_completion_timeout(&bt_ac6956c_task_completion , 3000);
	close(fd);
uart_error:
	gbt->uartfd=-1;
null_error:
	free(gbt);
	gbt = NULL;
error:
	return BT_RET_ERROR;
}

int bluetooth_deinit(void)
{
	if(gbt==NULL) 
		return BT_RET_ERROR;

	if (gbt->cref == 0)
		return BT_RET_ERROR;

	gbt->cref--;
	if (gbt->cref == 0) {
		// bluetooth_poweroff();
		/* delete task */
		bt_ac6956c_task_start_flag=1;
		wait_for_completion_timeout(&bt_ac6956c_task_completion , 3000);
		/* deinit bt */
		close(gbt->uartfd);
		free(gbt);
		gbt = NULL;
	}

	return BT_RET_SUCCESS;
}

int bluetooth_poweron(void)
{
	bt_device_status_e read_state=0;
	int count=0;
	if(bt_ac6956c_set_action(CMD_SET_BT_POWER_ON,0)==0)
	{
		#if 0 // for test do not let it return error
		while(1)
		{
			read_state = bt_get_device_sys_status();
			if(read_state!=EBT_DEVICE_STATUS_NOWORKING_DEFAULT)
			{
				if(read_state<EBT_DEVICE_STATUS_WORKING_EXISTENT)
					return BT_RET_ERROR;
				break;
			}
			if(count++>200)return BT_RET_ERROR;
			usleep(20 * 1000);
		}
		#endif
	}
	else
		return BT_RET_ERROR;
	return BT_RET_SUCCESS;
}

int bluetooth_poweroff(void)
{
	if(bt_ac6956c_set_action(CMD_SET_BT_POWER_OFF,0)==0)
	{
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int bluetooth_scan(void)
{
	if(bt_ac6956c_set_action(CMD_SET_INQUIRY_START,0)==0)
	{
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int bluetooth_stop_scan(void)
{
	if(bt_ac6956c_set_action(CMD_SET_INQUIRY_STOP,0)==0)
	{
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int bluetooth_connect(unsigned char *mac)
{
	if(bt_ac6956c_set_action(CMD_SET_CONNECT,mac)==0)
	{
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int bluetooth_get_connect_info(unsigned char *mac)
{
	int count=0;
	if(bt_ac6956c_set_action(CMD_SET_CONNECT,mac)==0)
	{
		// while(1)
		// {
		//     if(bt_get_device_sys_connet_info(info)==0)return BT_RET_SUCCESS;
		//     if(count>200)return BT_RET_ERROR;
		//     usleep(20 * 1000);
		// }
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int bluetooth_is_connected(void)
{
	bt_device_status_e read_state=0;
	int count=0;
	if(bt_ac6956c_set_action(CMD_GET_BT_CONNECT_STATE,0)==0)
	{
		while(1)
		{
			count++;
			read_state = bt_get_device_sys_status();
			if(read_state>EBT_DEVICE_STATUS_NOWORKING_DEFAULT)
			{
				if(read_state==EBT_DEVICE_STATUS_WORKING_CONNECTED)
				{
					log_d("Device exists\n");
				}
				else if(read_state==EBT_DEVICE_STATUS_WORKING_GET_CONNECTED_INFO)
				{
					log_d("Device exist\n");
				}
				else if(read_state==EBT_DEVICE_STATUS_WORKING_DISCONNECTED)
				{
					log_d("Device does not exist\n");
					return BT_RET_ERROR;
				}
				break;
			}
			if(count>35)return BT_RET_ERROR;
				usleep(20 * 1000);
		}
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int bluetooth_disconnect(void)
{
	if(bt_ac6956c_set_action(CMD_SET_DISCONNECT,0)==0)
	{
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int bluetooth_del_all_device(void)
{
	if(bt_ac6956c_set_action(CMD_SET_DELETE_ALL_DEVICE,0)==0)
	{
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int bluetooth_del_list_device(void)
{
	if(bt_ac6956c_set_action(CMD_SET_DELETE_LAST_DEVICE,0)==0)
	{
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int bluetooth_memory_connection(unsigned char value)
{
	if(bt_ac6956c_set_action(CMD_SET_MEMORY_CONNECTION,&value)==0)
	{
		return BT_RET_SUCCESS;
	}
	else
		return BT_RET_ERROR;
}

int _bluetooth_ioctl_(int cmd,unsigned long arg)
{
	int ret=0;
	switch(cmd){
		case BLUETOOTH_SET_PINMUX:
		{
			bt_pinmux_set_t* bt_pinmux=(bt_pinmux_set_t*)arg;
			char send_buf[2]={0};
			//pinpad setting cmd_value size is 2Byte 
			send_buf[0]=(char)bt_pinmux->pinpad;
			send_buf[1]=(char)bt_pinmux->pinset;
			if(bt_ac6956c_set_action(CMD_SET_BT_PIN_FUNCTION,send_buf)==0){
				ret=BT_RET_SUCCESS;
			}else
				ret=BT_RET_ERROR;
			break;
		}
		case BLUETOOTH_SET_GPIO_OUT:
		{
			bt_gpio_set_t* bt_gpioset=(bt_gpio_set_t*)arg;
			char send_buf[2]={0};
			//pinpad setting cmd_value size is 2Byte 
			send_buf[0]=(char)bt_gpioset->pinpad;
			send_buf[1]=(char)bt_gpioset->value; 
			if(bt_ac6956c_set_action(CMD_SET_BT_GPIO_POL,send_buf)==0){
				ret=BT_RET_SUCCESS;
			}else
				ret=BT_RET_ERROR;
			break;
		}
		case BLUETOOTH_SET_IR_POWERKEY:
		{
			uint16_t keycode=(uint16_t)arg;
			char send_buf[2]={0};
			send_buf[1]=keycode&0xff;
			send_buf[0]=(keycode>>8)&0xff;
			if(bt_ac6956c_set_action(CMD_SET_BT_IR_POWER_KEY,send_buf)==0){
				ret=BT_RET_SUCCESS;
			}else
				ret=BT_RET_ERROR;
			break;
		}
		case BLUETOOTH_SET_CMD_ACK:
		{			
			if(bt_ac6956c_set_action(CMD_SET_BT_CMD_ACK,NULL)==0){
				ret=BT_RET_SUCCESS;
			}else
				ret=BT_RET_ERROR;
			break;
		}

		case BLUETOOTH_SET_IR_USERCODE:
		{
			uint16_t value=(uint16_t)arg;
			char send_buf[2]={0};
			send_buf[0]=(value>>8)&0xff;
			send_buf[1]=value&0xff;
			if(bt_ac6956c_set_action(CMD_SET_IR_USERCODE,send_buf)==0){
				ret=BT_RET_SUCCESS;
			}else
				ret=BT_RET_ERROR;
			break;
		}
		case BLUETOOTH_SET_POWER_PINPAD_OFF:
		{
			if(bt_ac6956c_set_action(CMD_SET_BT_POWER_PINPAD_OFF,NULL)==0){
				ret=BT_RET_SUCCESS;
			}else
				ret=BT_RET_ERROR;
			break;
		}
		case BLUETOOTH_SET_AUDIO_CHANNEL_INPUT:
		{
			uint8_t channel_val=(uint8_t)arg;
			if(channel_val>0x01)
				ret=BT_RET_ERROR;
			char send_buf[1]={0};
			send_buf[0]=channel_val;
			if(bt_ac6956c_set_action(CMD_SET_AUDIO_CHANNEL_INPUT_SELECT,send_buf)==0){
				ret=BT_RET_SUCCESS;
			}else
				ret=BT_RET_ERROR;
			break;
		}
		case BLUETOOTH_GET_AUDIO_CHANNEL_INPUT:
		{
			if(bt_ac6956c_set_action(CMD_GET_AUDIO_CHANNEL_INPUT_SELECT,NULL)==0){
				ret=BT_RET_SUCCESS;
			}else
				ret=BT_RET_ERROR;
			break;
		}
		case BLUETOOTH_SET_PWM_PARAM:
		{
			char send_buf[3]={0};
			bt_pwm_param_t* pwm_param=(bt_pwm_param_t*)arg;
			send_buf[0]=(char)pwm_param->pinpad;
			send_buf[1]=(char)pwm_param->type;
			send_buf[2]=(char)pwm_param->value;
			if(bt_ac6956c_set_action(CMD_SET_BT_PWM_PARAM,send_buf)==0){
				ret=BT_RET_SUCCESS;
			}else
				ret=BT_RET_ERROR;
			break;
		}
		case BLUETOOTH_SET_CTRL_CMD:
		{
			char send_buf[1]={0};
			uint8_t val=(uint8_t)arg;
			send_buf[0]=(char )val;
			bt_ac6956c_set_action(CMD_SET_BT_CTRL_CMD,send_buf);
			break;
		}
		case BLUETOOTH_GET_MUSIC_VOL_RESULTS:
		{
			bt_ac6956c_set_action(CMD_GET_BT_MUSIC_VOL_RESULTS,NULL);
			sem_wait(&bluetooth_cmds_sem);
			uint8_t* vol =(uint8_t *)arg;
			*vol=gbt->volume;
			break;
		}
		case BLUETOOTH_SET_MUSIC_VOL_VALUE: 
		{
			uint8_t val=(uint8_t)arg;
			char send_buf[1]={0};
			val=val/3>BLUETOOTH_MAX_VOLUME_RANGE?BLUETOOTH_MAX_VOLUME_RANGE:val/3;
			send_buf[0]=val;
			bt_ac6956c_set_action(CMD_SET_BT_MUSIC_VOL_VALUE,send_buf);
			break;
		}
		case BLUETOOTH_SET_DEFAULT_CONFIG:
		{
			uint16_t value=IRDEF_USERCODE;
			char send_buf[2]={0};
			send_buf[0]=(value>>8)&0xff;
			send_buf[1]=value&0xff;
			bt_ac6956c_set_action(CMD_SET_IR_USERCODE,send_buf);

			uint16_t keycode=IRDEF_PWR_KEYCODE;
			memset(send_buf,0,2);
			send_buf[1]=keycode&0xff;
			send_buf[0]=(keycode>>8)&0xff;
			bt_ac6956c_set_action(CMD_SET_BT_IR_POWER_KEY,send_buf);

			bt_pinmux_set_t lineout_det={0};
			lineout_det.pinpad=LINEOUTDET_PINPAD;
			lineout_det.pinset=PINMUX_BT_GPIO_INPUT;
			//pinpad setting cmd_value size is 2Byte 
			memset(send_buf,0,2);
			send_buf[0]=(char)lineout_det.pinpad;
			send_buf[1]=(char)lineout_det.pinset;
			bt_ac6956c_set_action(CMD_SET_BT_PIN_FUNCTION,send_buf);

			break;
		}
		default:
			break;
	}
	return ret;
}

int bluetooth_ioctl(int cmd, ...)
{
	int ret=0;
	va_list ap;
	va_start(ap, cmd);
	unsigned long arg=va_arg(ap, unsigned long);
	ret=_bluetooth_ioctl_(cmd,arg);
	va_end(ap);
	return ret;
}

int bluetooth_set_gpio_mutu(unsigned char value)
{
	// set gpiox output value
	bt_gpio_set_t mute_level={0};
	mute_level.pinpad=PINPAD_BT_PC3;
	mute_level.value=value;
	bluetooth_ioctl(BLUETOOTH_SET_GPIO_OUT,&mute_level);
	// bluetooth_pinpad_levelset(&mute_level);
	log_d(">>! %s,%d\n",__func__,value);
	return 0;
}

static void bt_ac6956c_read_thread(void *args)
{
	bt_cmds_e *cmds=(bt_cmds_e *)args;
	bt_cmds_e cmds_t=*cmds;
	char *rx_buf = (char *)malloc(1 * UART_RX_RECEIVE_MAX_BUFF);
	char byte = 0;
	struct pollfd fds[1];
	nfds_t nfds = 1;
	static unsigned char date_off_set = 0;
	static int count=0;
	int ret = BT_RET_SUCCESS;
	int poll_time = 100;
	TickType_t tTickNow = 0;
	if(gbt==NULL) return;

	fds[0].fd = gbt->uartfd;
	fds[0].events  = POLLIN|POLLRDNORM;;
	fds[0].revents = 0;
	log_d("fds[0].fd =%d \n",fds[0].fd);
	log_d("init \n");
	while(!bt_ac6956c_task_start_flag)
	{
		if(gbt->uartfd<0)
			break;

		ret = poll(fds, nfds, 0);   //poll bt uart 
		if (ret > 0)
		{
			if (fds[0].revents & (POLLRDNORM | POLLIN))
			{
				if(read(gbt->uartfd, &byte, 1))
				{
					rx_buf[count++]=byte;
					tTickNow = xTaskGetTickCount(); //to set tick cnt
					if(count==UART_RX_RECEIVE_MAX_BUFF) //resc 1k data 
					{
						date_off_set =0;
						count = 0;
					}
				}
			}
		}
		/*Serial port returns data for judgment*/
		if(count >= BLUETOOTH_MINIMUM_CHECKSUM) //rx buf data must > 7B
		{
			ret = str_with_ac6956c_messagebody_compare(&rx_buf[date_off_set],count - date_off_set);
			if(ret == BT_RET_SUCCESS)
			{
				if(cmds_t!=*cmds)
					cmds_t=*cmds;
				log_d("count =%d\n",count);
				bt_ac6956c_serial_data_judg(cmds_t,&rx_buf[date_off_set],count - date_off_set);
				memset(rx_buf,0,count);
				count =0;
				date_off_set = 0;
			}
			else if(ret == BT_ERROR_DATA_OFFSET_1 || ret == BT_ERROR_DATA_OFFSET_2)
			{
				date_off_set += ret - BT_ERROR_DATA;
				if(date_off_set > 10)
				{
					log_e("date_off_set too large\n");
					elog_hexdump2(ELOG_LVL_ERROR, "bt rxbuf data", 16, rx_buf, count);
					memset(rx_buf, 0, count);
					date_off_set = 0;
					count = 0;
				}
			}
			else if(ret == BT_RET_ERROR)
			{
				log_e("rx buff error\n");
				elog_hexdump2(ELOG_LVL_ERROR, "bt rxbuf data", 16, rx_buf, count);
				memset(rx_buf,0,count);
				date_off_set = 0;
				count = 0;
			}
		}

	}
	free(rx_buf);
	usleep(1000);
	complete(&bt_ac6956c_task_completion);
	vTaskDelete(NULL);
}


static bt_device_status_e bt_get_device_sys_status(void)
{
	if(gbt==NULL)
		return EBT_DEVICE_STATUS_NOWORKING_DEFAULT;
	return gbt->get_dev_status;
}

static int bt_get_device_sys_connet_info(struct bluetooth_slave_dev *con_data)
{
	bt_device_status_e sys_status=bt_get_device_sys_status();
	if(gbt==NULL||con_data==NULL) return BT_RET_ERROR;
	if(sys_status==EBT_DEVICE_STATUS_WORKING_GET_CONNECTED_INFO)
	{
		memcpy(con_data,&gbt->connet_info,sizeof(struct bluetooth_slave_dev));
		return BT_RET_SUCCESS;
	}
	else if(sys_status<EBT_DEVICE_STATUS_WORKING_CONNECTED)
		return 1;

	return BT_RET_ERROR;
}

static int bt_ac6956c_set_action(bt_cmds_e cmd,unsigned char *send_buf)
{
	struct AC6956C_MessageBody body_m={0};
	char buf[UART_TX_WRITE_MAX_BUFF]={0};
	int ret = BT_RET_SUCCESS;
	struct bluetooth_slave_dev *acq_dev_info=NULL;

	bt_cmds_e *dev_status=NULL;
	if(gbt==NULL)
	{
		log_e("gbt = NULL \n");
		return BT_RET_ERROR;
	}
		

	if(gbt->uartfd<0)
	{
		log_e("gbt->uartfd  =%d\n",gbt->uartfd);
		return BT_RET_ERROR;
	}

	if(cmd!=CMD_SET_BT_POWER_ON)
	{
		gbt->get_dev_status=EBT_DEVICE_STATUS_NOWORKING_DEFAULT;
	}
	body_m.cmd_id=cmd;
	body_m.cmd_len=0x00;
	switch(cmd)
	{
		case CMD_SET_BT_POWER_ON:
			if(gbt->get_dev_status<EBT_DEVICE_STATUS_WORKING_CONNECTED)
			{
				gbt->get_dev_status=EBT_DEVICE_STATUS_NOWORKING_DEFAULT;
			}
			break;
		case CMD_SET_INQUIRY_START:
			memset(&gbt->inquiry_info,0,sizeof(struct bluetooth_slave_dev));
			break;
		case CMD_SET_CONNECT:
			if(send_buf==NULL)goto error;
			body_m.cmd_len=0x06;
			break;
		case CMD_SET_BT_POWER_OFF:
			gbt->get_dev_status=EBT_DEVICE_STATUS_NOWORKING_NOT_EXISTENT;
			break;
		case CMD_SET_MEMORY_CONNECTION:
			body_m.cmd_len=0x01;
			break;
		case CMD_SET_BT_PIN_FUNCTION:
			body_m.cmd_len=0x02;
			break;
		case CMD_SET_BT_GPIO_POL:
			body_m.cmd_len=0x02;
			break;
		case CMD_SET_BT_IR_POWER_KEY:
			body_m.cmd_len=0x02;
			break;
		case CMD_SET_IR_USERCODE:
			body_m.cmd_id=CMD_SET_IR_USERCODE;
			body_m.cmd_len=0x02;
			break;
		case CMD_SET_AUDIO_CHANNEL_INPUT_SELECT:
			body_m.cmd_len=0x01; 
			break;
		case CMD_SET_BT_PWM_PARAM:
			body_m.cmd_len=0x03;
			break;
		case CMD_SET_BT_CTRL_CMD:
			body_m.cmd_len=0x01;
			break;
		case CMD_SET_BT_MUSIC_VOL_VALUE:
			body_m.cmd_len=0x01;
			break;
		default:
			break;
	}
	if(ret!=0)goto error;
	gbt->bt_cmds=cmd;
	memcpy(body_m.cmd_value,send_buf,body_m.cmd_len);
/*uart send data*/
	body_m.frame_id=0x00;
	body_m.frame_head=0xAC69;
	body_m.frame_len=9+body_m.cmd_len;
	memset(buf,0,UART_TX_WRITE_MAX_BUFF);
	if(bt_ac6956c_messagebody_switch_str(&body_m,buf)==BT_RET_ERROR)goto error;
	ret=write(gbt->uartfd,buf,body_m.frame_len);

	if(ret<0)goto error;
	ret = BT_RET_SUCCESS;
exit:
	return ret;
error:
	log_e("data error\n");
	ret = BT_RET_ERROR;
	return ret;
}
/**
 * @description: updata bt_priv_t,do action by rx_data 
 * @author: Yanisin
 */
static void bt_ac6956c_serial_data_judg(bt_cmds_e cmd,char *buf,unsigned int count)
{
	unsigned int i=0;
	int ret = BT_RET_SUCCESS;
	struct AC6956C_MessageBody body_m={0};
	struct bluetooth_slave_dev *acq_dev_info=&gbt->inquiry_info;
	struct bluetooth_slave_dev *connet_data=&gbt->connet_info;
	bt_gpio_set_t* pinpad_level=&(gbt->bt_pinpad_level); 
	struct input_event_bt event_key={0};
	unsigned char name_cnt=0;
	unsigned char disconnet_repeat_cnt = 0;
	unsigned char connet_repeat_cnt = 0;
	uint16_t tmp_code=0;
	i=count;
	while(i >= BLUETOOTH_MINIMUM_CHECKSUM)
	{
		if(ret !=0 ) {
			ret = BT_RET_SUCCESS;
			break;
		}
		if(str_switch_bt_ac6956c_messagebody(&buf[count-i],&i,&body_m)==BT_RET_SUCCESS)
		{
			if(body_m.frame_head != 0xac69) 
			{
				gbt->get_dev_status = EBT_DEVICE_STATUS_NOWORKING_STATUS_ERROR;
				break;
			}
			switch(cmd){
			case CMD_SET_BT_POWER_ON:
					if(gbt->get_dev_status < EBT_DEVICE_STATUS_WORKING_CONNECTED)
					{
						gbt->get_dev_status = EBT_DEVICE_STATUS_WORKING_EXISTENT;
					}
					// ret = BT_RET_EXIT;
				break;
			case CMD_SET_INQUIRY_START:
				break;
			case CMD_SET_CONNECT:
				break;
			case CMD_GET_BT_CONNECT_STATE:
				if(body_m.cmd_id==0X45)
				{
					if(body_m.cmd_value[0]==0)
					{
						gbt->get_dev_status = EBT_DEVICE_STATUS_WORKING_CONNECTED;
						gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_CONNECTED,0);
					}
					else
					{
						gbt->get_dev_status = EBT_DEVICE_STATUS_WORKING_DISCONNECTED;
						gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_DISCONNECTED,0);
						ret = BT_RET_EXIT;
					}
				}
				break;
			case CMD_SET_INQUIRY_STOP:
				if(body_m.cmd_id==0x0B)
				{
					gbt->get_dev_status = EBT_DEVICE_STATUS_WORKING_INQUIRY_STOP_SEARCHING;
					gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_SCAN_FINISHED,0);
				}
				break;
			case CMD_SET_DISCONNECT:
				// gbt->get_dev_status=EBT_DEVICE_STATUS_WORKING_DISCONNECTED;
				// gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_DISCONNECTED,0);
				// ret = BT_RET_EXIT;
				break;
			case CMD_SET_BT_POWER_OFF:
				gbt->get_dev_status=EBT_DEVICE_STATUS_NOWORKING_BT_POWER_OFF;
				// ret = BT_RET_EXIT;
				// for test do not let it disable bt output cmd
				break; 
			default:
				// due to after poweroff bt_module has to do something
				// ret = BT_RET_EXIT;
				break;
			}
			if(ret!=BT_RET_EXIT)
			{
				switch (body_m.cmd_id)
				{
					// handler rx_buffer data ,do action by cmd_id 
					case CMD_REPORT_CONNECT_STATUS:
						if(body_m.cmd_value[0]==0x01)
						{
							connet_repeat_cnt++;
							gbt->get_dev_status = EBT_DEVICE_STATUS_WORKING_CONNECTED;
							gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_CONNECTED,0);
							// ret = BT_RET_EXIT;
						}
						else
						{
							if(disconnet_repeat_cnt == 0)
							{
								memset(connet_data,0,sizeof(struct bluetooth_slave_dev));
								gbt->get_dev_status = EBT_DEVICE_STATUS_WORKING_DISCONNECTED;
								gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_DISCONNECTED,0);
							}
							disconnet_repeat_cnt++;
							// ret = BT_RET_EXIT;
						}
						break;
					case CMD_REPORT_CONNECT_RESULTS:
						memset(connet_data,0,sizeof(struct bluetooth_slave_dev));
						memcpy(connet_data->mac,body_m.cmd_value,BLUETOOTH_MAC_LEN);
						if(body_m.cmd_len-6<BLUETOOTH_NAME_LEN)
						{
							memcpy(connet_data->name,&body_m.cmd_value[BLUETOOTH_MAC_LEN],body_m.cmd_len-6);
						}
						else
						{
							memcpy(connet_data->name,&body_m.cmd_value[BLUETOOTH_MAC_LEN], BLUETOOTH_NAME_LEN);
							connet_data->name[BLUETOOTH_NAME_LEN-1] = 0;
						}
						log_d("connet_repeat_cnt = %d gbt->get_dev_status =%d\n",connet_repeat_cnt,gbt->get_dev_status);
						if(connet_repeat_cnt==0&&gbt->get_dev_status<BLUETOOTH_EVENT_SLAVE_DEV_CONNECTED)
						{
							gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_CONNECTED,0);
						}
						gbt->get_dev_status = EBT_DEVICE_STATUS_WORKING_GET_CONNECTED_INFO;
						gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_GET_CONNECTED_INFO,(unsigned long)connet_data);
						break;
					case CMD_REPORT_INQUIRY_RESULTS:
						memset(acq_dev_info,0,sizeof(struct bluetooth_slave_dev));
						name_cnt=body_m.cmd_len-BLUETOOTH_MAC_LEN-1;
						memcpy(acq_dev_info->mac,body_m.cmd_value,BLUETOOTH_MAC_LEN);
						if(name_cnt<BLUETOOTH_NAME_LEN)
						{
							memcpy(acq_dev_info->name,&body_m.cmd_value[BLUETOOTH_MAC_LEN+1], name_cnt);
						}
						else
						{
							memcpy(acq_dev_info->name,&body_m.cmd_value[BLUETOOTH_MAC_LEN+1], BLUETOOTH_NAME_LEN);
							acq_dev_info->name[BLUETOOTH_NAME_LEN-1] = 0;
						}
						gbt->get_dev_status = EBT_DEVICE_STATUS_WORKING_INQUIRY_DATA_SEARCHED;
						gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_SCANNED,(unsigned long)acq_dev_info);
						break;
					case CMD_REPORT_INQUIRY_COMPLETE:
						gbt->get_dev_status = EBT_DEVICE_STATUS_WORKING_INQUIRY_STOP_SEARCHING;
						gbt->callback(BLUETOOTH_EVENT_SLAVE_DEV_SCAN_FINISHED,(unsigned long)acq_dev_info);
						break;
					case CMD_REPORT_BT_GPI_STAT:
						log_d(">>! BT_REPORT_GPI_STAT:%d\n",body_m.cmd_value[1]);
						memcpy(&pinpad_level->pinpad,body_m.cmd_value,1);
						memcpy(&pinpad_level->value,body_m.cmd_value+1,1);
						gbt->callback(BLUETOOTH_EVENT_REPORT_GPI_STAT,(unsigned long)pinpad_level);
						break;
					case CMD_REPORT_BT_IR_KEY:
						// cmd_value =type(1B IR/ADC)+ir keycode size(4B?)+ status(repeat/press/release)(1B)?
						/*if ir report code is 2B ? */ 
						tmp_code=body_m.cmd_value[1];
						event_key.code=bt_reportkey_mapping(BT_IR_KEY,tmp_code); //map
						if(body_m.cmd_value[5]==0x00){
							event_key.value=1;
						}else if(body_m.cmd_value[5]==0x02){
							event_key.value=0; 	// report real value or def by user ?
						}else{
							event_key.value=body_m.cmd_value[5];
						}
						bluetooth_input_event_send(event_key); 
						//it will call a register func to write key_event_queue
						break;
					case CMD_REPORT_BT_ADC_KEY:
						// cmd_value =type(1B IR/ADC)+ir keycode size(1B)+ status(press/repeat/release)(1B)
						event_key.code=bt_reportkey_mapping(BT_ADC_KEY,body_m.cmd_value[1]); //map
						if(body_m.cmd_value[2]==0x00){
							event_key.value=1;
						}else if(body_m.cmd_value[2]==0x02){
							event_key.value=0; 	// report real value or def by user ?
						}else{
							event_key.value=body_m.cmd_value[2];
						}
						bluetooth_input_event_send(event_key); 
						break;
					case CMD_SET_BT_CMD_ACK:
						log_d(">>! bt_cmd_get_ack\n");
						break;
					case CMD_REPORT_AUDIO_CHANNEL_INPUT_SELECT:
						break;
					case CMD_REPORT_BT_MUSIC_VOL_RESULTS:
						body_m.cmd_value[0]=body_m.cmd_value[0]*3;
						memcpy(&gbt->volume,body_m.cmd_value,1);
						sem_post(&bluetooth_cmds_sem);
						break;
					default:
						break;
				}
			}
		}
		else
		{
			gbt->get_dev_status = EBT_DEVICE_STATUS_NOWORKING_STATUS_ERROR;
			ret = BT_RET_ERROR;
		}
	}
	printf_bt_ac6956c_dev_status(gbt->get_dev_status);
}

static void bt_ac6956c_set_poll_timeout(int cmds,int *poll_t)
{
	switch(cmds)
	{
		case CMD_SET_BT_POWER_ON:
			*poll_t=500;
			break;
		case CMD_SET_CONNECT:
			*poll_t=1000;
			break;
		case CMD_SET_INQUIRY_START:
		case CMD_SET_INQUIRY_STOP:
		case CMD_SET_DISCONNECT:
		case CMD_SET_BT_POWER_OFF:
		case CMD_GET_BT_CONNECT_STATE:
			*poll_t=200;
			break;
		default:
			*poll_t=100;
			break;
	}
}

static void printf_bt_ac6956c_dev_status(bt_device_status_e status)
{
	switch(status)
	{
		case EBT_DEVICE_STATUS_NOWORKING_DEFAULT:log_d("EBT_DEVICE_STATUS_NOWORKING_DEFAULT\n");break;
		case EBT_DEVICE_STATUS_NOWORKING_STATUS_ERROR:log_d("EBT_DEVICE_STATUS_NOWORKING_STATUS_ERROR\n");break;
		case EBT_DEVICE_STATUS_NOWORKING_NOT_EXISTENT:log_d("EBT_DEVICE_STATUS_NOWORKING_NOT_EXISTENT\n");break;
		case EBT_DEVICE_STATUS_NOWORKING_BT_POWER_OFF:log_d("EBT_DEVICE_STATUS_NOWORKING_BT_POWER_OFF\n");break;
		case EBT_DEVICE_STATUS_WORKING_EXISTENT:log_d("EBT_DEVICE_STATUS_WORKING_EXISTENT\n");break;
		case EBT_DEVICE_STATUS_WORKING_INQUIRY_DATA_SEARCHED:log_d("EBT_DEVICE_STATUS_WORKING_INQUIRY_DATA_SEARCHED\n");break;
		case EBT_DEVICE_STATUS_WORKING_INQUIRY_STOP_SEARCHING:log_d("EBT_DEVICE_STATUS_WORKING_INQUIRY_STOP_SEARCHING\n");break;
		case EBT_DEVICE_STATUS_WORKING_DISCONNECTED:log_d("EBT_DEVICE_STATUS_WORKING_DISCONNECTED\n");break;
		case EBT_DEVICE_STATUS_WORKING_CONNECTED:log_d("EBT_DEVICE_STATUS_WORKING_CONNECTED\n");break;
		case EBT_DEVICE_STATUS_WORKING_GET_CONNECTED_INFO:log_d("EBT_DEVICE_STATUS_WORKING_GET_CONNECTED_INFO\n");break;
		default:
			log_d("other\n");break;
			break;
	}
}
/**
 * @description: checksum calculate,add checksum end of MessageBody
 * @param {AC6956C_MessageBody} *body
 * @return {*}
 */
static void checksum_calculate(struct AC6956C_MessageBody *body)
{
	unsigned short sum=0;
	if(body==NULL)return;
	sum += (body->frame_head&0x00ff);
	sum += (body->frame_head >>8);
	sum += (body->frame_len&0x00ff);
	sum +=  (body->frame_len >>8);
	sum +=  body->frame_id;
	sum +=  body->cmd_id;
	sum +=  body->cmd_len;
	for(int i=0;i<body->cmd_len;i++)
	{
		sum += body->cmd_value[i];
	}
	body->Frame_checksum =sum;
}
// log_d send msg 
static void bt_ad6956f_mes_printf(struct AC6956C_MessageBody *body)
{
	if(body==NULL)return;
	log_d("ad6956 a cmds : %02x %02x %02x %02x %02x %02x %02x\n",(body->frame_head>>8),(body->frame_head&0x00ff),(body->frame_len&0x00ff),(body->frame_len>>8),body->frame_id,body->cmd_id,body->cmd_len);
	elog_hexdump("data",16,body->cmd_value,body->cmd_len);
	log_d("%02x %02x\n",(body->Frame_checksum&0x00ff),(body->Frame_checksum>>8));
}

// convert msg_t to rx_buf data 
static int bt_ac6956c_messagebody_switch_str(struct AC6956C_MessageBody *body,unsigned char *buf)
{
	unsigned char *offset=buf;
	if(body==NULL)return BT_RET_ERROR;
	if(body->cmd_len+9>=UART_RX_RECEIVE_MAX_BUFF)
	{
		log_e("Buf is too long\n");
		return BT_RET_ERROR;
	}
	checksum_calculate(body);
	*offset++=body->frame_head >>8;
	*offset++=body->frame_head&0x00ff;
	*offset++=body->frame_len&0x00ff;
	*offset++=body->frame_len >>8;
	*offset++=body->frame_id;
	*offset++=body->cmd_id;
	*offset++=body->cmd_len;
	memcpy(offset,body->cmd_value,body->cmd_len);
	offset+=body->cmd_len;
	*offset++=body->Frame_checksum&0x00ff;
	*offset=body->Frame_checksum >>8;
	bt_ad6956f_mes_printf(body);
	return BT_RET_SUCCESS;
}
/*uart rx_buf transform to messagebody_t */ 
static int str_switch_bt_ac6956c_messagebody(char *buf,unsigned int *count,struct AC6956C_MessageBody *body)
{
	unsigned char *offset=(unsigned char*)buf;
	unsigned int temp=0,temp1=0;
	if(body==NULL||buf==NULL||*count<=0)return BT_RET_ERROR;
	
	body->frame_head=*offset++;
	body->frame_head<<=8;
	body->frame_head+=(*offset++);
	
	if(body->frame_head!=0xac69)
	{
		*count -= 2;
		return BT_RET_ERROR;
	}
	body->frame_len=*offset++;
	temp=*offset++;
	body->frame_len|=(temp>>8);
	body->frame_id=*offset++;
	body->cmd_id=*offset++;
	body->cmd_len=*offset++;
	if(body->cmd_len+9>=UART_RX_RECEIVE_MAX_BUFF||body->cmd_len>=AC6956C_MAX_WRITE_SIZE)
	{
		log_e("Buf is too long\n");
		return BT_RET_ERROR;
	}
	memcpy(body->cmd_value,offset,body->cmd_len);
	offset+=body->cmd_len;
	checksum_calculate(body);
	temp=0;
	temp1 = (*offset++)&0x00ff;
	temp = *offset;
	temp <<= 8;
	temp += temp1;
	bt_ad6956f_mes_printf(body);
	*count -= (body->cmd_len+9);
	log_d("body->Frame_checksum =%04x temp= %04x\n",body->Frame_checksum,temp);
	if(body->Frame_checksum==temp)
		return BT_RET_SUCCESS;
	else
		return BT_RET_ERROR;
}

/**
 * @description: judge rx_data whether it is bt_cmd format
 * @param {char} *buf
 * @param {unsigned char} count
 * @return {*} rx_data buffer is bt cmd ->BT_RET_SUCCESS ,else BT_RET_ERROR
 */
static int str_with_ac6956c_messagebody_compare(char *buf,unsigned char count)
{
	struct AC6956C_MessageBody body = {0};
	unsigned char *offset=(unsigned char*)buf;
	unsigned int temp=0,temp1=0;

	body.frame_head=*offset++;
	if(body.frame_head!=0xac)
	{
		log_e("ad6956f frame head !=0xac error body.frame_head = %d\n",body.frame_head);
		return BT_ERROR_DATA_OFFSET_1;
	}

	body.frame_head<<=8;
	body.frame_head|=(*offset++);

	if(body.frame_head!=0xac69)
	{
		log_e("ad6956f frame head !=0xac69 error body.frame_head = %d\n",body.frame_head);
		return BT_ERROR_DATA_OFFSET_2;
	}
	body.frame_len=*offset++;
	temp=*offset++;
	body.frame_len|=(temp>>8);
	if(body.frame_len != count)
	{
		return BT_DATA_AGAIN;
	}
	body.frame_id=*offset++;
	body.cmd_id=*offset++;
	body.cmd_len=*offset++;
	if(body.cmd_len >= AC6956C_MAX_WRITE_SIZE)
	{
		log_e("Cmd_len buf is too long\n");
		return BT_RET_ERROR;
	}
	memcpy(body.cmd_value,offset,body.cmd_len);
	offset+=body.cmd_len;
	checksum_calculate(&body);
	temp=0;
	temp1 = (*offset++)&0x00ff;
	temp = *offset;
	temp <<= 8;
	temp += temp1;
	log_d("body.Frame_checksum =%04x temp= %04x\n",body.Frame_checksum,temp);
	if(body.Frame_checksum==temp)
		return BT_RET_SUCCESS;
	else
	{
		return BT_RET_ERROR;
	}
}


static void bt_ac6956c_cp_connet_info(struct bluetooth_slave_dev *connet_cmds,struct AC6956C_MessageBody *obj)
{
	if(connet_cmds==NULL||obj==NULL)return;
	memcpy(obj->cmd_value,connet_cmds->mac,BLUETOOTH_MAC_LEN);
	obj->cmd_len=BLUETOOTH_MAC_LEN;
}


/**
 * @description: mapping bt_report key to keymap_table keycode 
 * @param {bt_keytype_e} keytype to mapping diff keymap_table 
 * @param {uint32_t} btcode bt_report keycode 
 * @return {*} keymap_table's keycode 
 * @author: Yanisin
 */
static uint16_t bt_reportkey_mapping(bt_keytype_e keytype,uint32_t btcode)
{
	if(keytype==BT_IR_KEY){
		int key_cnt=sizeof(ir_keymap_table)/sizeof(ir_keymap_table[0]);
		for(int i=0;i<key_cnt;i++){
			if(btcode==ir_keymap_table[i].report_code){
				return ir_keymap_table[i].keycode;
			}
		}
	}else if(keytype==BT_ADC_KEY){
		int key_cnt=sizeof(adc_keymap_table)/sizeof(adc_keymap_table[0]);
		for(int i=0;i<key_cnt;i++){
			if(btcode==adc_keymap_table[i].report_code){
				return adc_keymap_table[i].keycode;
			}
		}
	}else if(keytype==BT_GPIO_KEY){
		/*reserve*/ 
	}
}


#endif
