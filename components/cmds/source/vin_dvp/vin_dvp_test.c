#include <fcntl.h>
#include <unistd.h>
#include <kernel/vfs.h>
#include <stdio.h>
#include <kernel/io.h>
#include <getopt.h>
#include <malloc.h>
#include <sys/mman.h>
#include <kernel/lib/console.h>
#include <kernel/completion.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <hcuapi/common.h>
#include <hcuapi/kshm.h>
#include <hcuapi/vindvp.h>
#include <hcuapi/tvtype.h>
#include <hcuapi/vidmp.h>
#include <stdlib.h>

static int vin_dvp_fd = -1;
static struct completion video_read_task_completion;
static bool video_stop_read = 0;
static struct kshm_info video_read_hdl = { 0 };
static enum VINDVP_VIDEO_DATA_PATH vpath = VINDVP_VIDEO_TO_DE;
static unsigned int rotate_mode = ROTATE_TYPE_0;
static unsigned int mirror_mode = MIRROR_TYPE_NONE;
static enum TVTYPE tv_sys = TV_NTSC;
static bool vin_dvp_started = false;
static unsigned int combine_mode = VINDVP_COMBINED_MODE_DISABLE;
static bool creat_read_task = false;
static  enum VINDVP_BG_COLOR bg_color = VINDVP_BG_COLOR_BLACK;
static int stop_mode = 0;
static int port = 0;
static struct vindvp_to_kshm_setting to_kshm_setting = { 0 };

static void vin_dvp_video_read_thread(void *args)
{
    struct kshm_info *hdl = (struct kshm_info *)args;
    AvPktHd hdr = { 0 };
    uint8_t *data = malloc(1024 * 1024);

    printf("video_read_thread run```\n");
    while(!video_stop_read)
    {
        while(kshm_read(hdl , &hdr , sizeof(AvPktHd)) != sizeof(AvPktHd) && !video_stop_read)
        {
            //printf("read audio hdr from kshm err\n");
            usleep(20 * 1000);
        }
        printf("pkt size 0x%x, flag %d\n" , (int)hdr.size , (int)hdr.flag);

        data = realloc(data , hdr.size);
        while(kshm_read(hdl , data , hdr.size) != hdr.size && !video_stop_read)
        {
            //printf("read audio data from kshm err\n");
            usleep(20 * 1000);
        }
        printf("data: 0x%x\n" , (unsigned int)data);
    }
    usleep(1000);

    if(data)
        free(data);

    complete(&video_read_task_completion);
    vTaskSuspend(NULL);
}

static int vin_dvp_set_to_kshm_setting(int argc , char *argv[])
{
    int opt;
    opterr = 0;
    optind = 0;
    bool rotate_enable = false;
    bool scale_enable = false;
    enum ROTATE_TYPE rotate_mode = 0;
    int scaled_width = 0;
    int scaled_height = 0;
    enum VINDVP_TO_KSHM_SCALE_MODE scale_mode = 0;
    enum VIDDEC_SCALE_FACTOR scale_factor = 0;
    bool snapshot_enable = false;
    while ((opt = getopt(argc , argv , "r:s:f:w:h:v:")) != EOF)
    {
        switch (opt)
        {
            case 'r':
                if (atoi(optarg) != 0)
                {
                    rotate_enable = 1;
                    rotate_mode = atoi(optarg);
                }
                break;
            case 's':
                if (atoi(optarg) == 1)
                {
                    scale_enable = true;
                    scale_mode = VIN_DVP_TO_KSHM_SCALE_BY_FACTOR;
                }
                else if (atoi(optarg) == 2)
                {
                    scale_enable = true;
                    scale_mode = VIN_DVP_TO_KSHM_SCALE_BY_SIZE;
                }
                break;
            case 'f':
                scale_factor = atoi(optarg);
                break;
            case 'w':
                scaled_width = atoi(optarg);
                break;
            case 'h':
                scaled_height = atoi(optarg);
                break;
            case 'v':
                snapshot_enable = atoi(optarg);
                break;
            default:
                break;
        }
    }
    to_kshm_setting.rotate_enable = rotate_enable;
    to_kshm_setting.scale_enable = scale_enable;
    to_kshm_setting.rotate_mode = rotate_mode;
    to_kshm_setting.scale_mode = scale_mode;
    if (scale_mode == VIN_DVP_TO_KSHM_SCALE_BY_FACTOR)
    {
        to_kshm_setting.scale_factor = scale_factor;
    }
    else
    {
        to_kshm_setting.scaled_width = scaled_width;
        to_kshm_setting.scaled_height = scaled_height;
    }
    to_kshm_setting.snapshot_enable = snapshot_enable;
    printf("to_kshm_setting rotate_enable:%d scale_enable:%d rotate_mode=%d scale_mode=%d snapshot_enable = %d\n" ,
           rotate_enable , scale_enable , rotate_mode , scale_mode , snapshot_enable);
    return 0;
}

static int vindvp_start(int argc , char *argv[])
{
    int opt;
    opterr = 0;
    optind = 0;
    int value = 0;

    if(vin_dvp_started == true)
    {
        return 0;
    }

    vpath = VINDVP_VIDEO_TO_DE;
    rotate_mode = ROTATE_TYPE_0;
    combine_mode = VINDVP_COMBINED_MODE_DISABLE;
    bg_color = VINDVP_BG_COLOR_BLACK;
    stop_mode = VINDVP_BLACK_SRCREEN_ANYWAY;

    vin_dvp_fd = open("/dev/vindvp" , O_RDWR);
    if(vin_dvp_fd < 0)
    {
        return -1;
    }

    while((opt = getopt(argc , argv , "v:r:t:c:b:s:p:m:")) != EOF)
    {
        switch(opt)
        {
            case 'v':
                vpath = atoi(optarg);
                break;
            case 'r':
                rotate_mode = atoi(optarg);
                break;
            case 'm':
                mirror_mode = atoi(optarg);
                break;
            case 't':
                value = atoi(optarg);
                break;
            case 'c':
                combine_mode = atoi(optarg);
                break;
            case 'b':
                bg_color = atoi(optarg);
                break;
            case 's':
                stop_mode = atoi(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                break;
        }
    }

    printf("vpath %d\n" , vpath);
    printf("combine_mode %d\n" , combine_mode);

    ioctl(vin_dvp_fd , VINDVP_SET_VIDEO_DATA_PATH , vpath);
    ioctl(vin_dvp_fd , VINDVP_SET_COMBINED_MODE , combine_mode);
    ioctl(vin_dvp_fd , VINDVP_SET_BG_COLOR , bg_color);
    ioctl(vin_dvp_fd , VINDVP_SET_VIDEO_STOP_MODE , stop_mode);
//    ioctl(vin_dvp_fd , VINDVP_SET_EXT_DEV_INPUT_PORT_NUM , port); //FOR BR3A03 

    if(vpath == VINDVP_VIDEO_TO_KSHM ||
       vpath == VINDVP_VIDEO_TO_DE_AND_KSHM ||
       vpath == VINDVP_VIDEO_TO_DE_ROTATE_AND_KSHM )
    {
        int ret;
        struct kshm_info kshm_hdl;

        ioctl(vin_dvp_fd , VINDVP_SET_TO_KSHM_SETTING , &to_kshm_setting);
        ioctl(vin_dvp_fd , VINDVP_VIDEO_KSHM_ACCESS , &video_read_hdl);

        init_completion(&video_read_task_completion);
        video_stop_read = 0;
        ret = xTaskCreate(vin_dvp_video_read_thread , "video_read_thread" ,
                          0x1000 , &video_read_hdl , portPRI_TASK_HIGH , NULL);
        if(ret != pdTRUE)
        {
            printf("kshm recv thread create failed\n");
        }
        creat_read_task = true;
    }

    if(vpath == VINDVP_VIDEO_TO_DE_ROTATE ||
       vpath == VINDVP_VIDEO_TO_DE_ROTATE_AND_KSHM)
    {
        printf("rotate_mode = 0x%x\n" , rotate_mode);
        ioctl(vin_dvp_fd , VINDVP_SET_VIDEO_ROTATE_MODE , rotate_mode);
        printf("mirror_mode = 0x%x\n" , mirror_mode);
        ioctl(vin_dvp_fd , VINDVP_SET_VIDEO_MIRROR_MODE , mirror_mode);
    }

    ioctl(vin_dvp_fd , VINDVP_START , tv_sys);
    printf("vin dvp start ok\n");
    vin_dvp_started = true;
    return 0;
}

static int vindvp_stop(int argc , char *argv[])
{
    int opt;

    if(vin_dvp_fd >= 0)
    {
        video_stop_read = 1;
        vin_dvp_started = false;

        if(vpath == VINDVP_VIDEO_TO_KSHM ||
           vpath == VINDVP_VIDEO_TO_DE_AND_KSHM ||
           vpath == VINDVP_VIDEO_TO_DE_ROTATE_AND_KSHM)
        {
            wait_for_completion_timeout(&video_read_task_completion , 3000);
        }

        ioctl(vin_dvp_fd , VINDVP_STOP);
        close(vin_dvp_fd);
        vin_dvp_fd = -1;
        return 0;
    }
    else
    {
        return -1;
    }
}

static int vindvp_enter(int argc , char *argv[])
{
    return 0;
}


static int vindvp_enable(int argc , char *argv[])
{
    if(vin_dvp_fd >= 0)
    {
        ioctl(vin_dvp_fd , VINDVP_ENABLE);
    }
    return 0;
}

static int vindvp_disable(int argc , char *argv[])
{
    if(vin_dvp_fd >= 0)
    {
        ioctl(vin_dvp_fd , VINDVP_DISABLE);
    }
    return 0;
}


static int vindvp_set_combine_regin_status(int argc , char *argv[])
{
    int opt;
    opterr = 0;
    optind = 0;
 
    combine_region_freeze_cfg_t region_cfg = {0};
 
    while((opt = getopt(argc , argv , "i:s:")) != EOF)
    {
        switch(opt)
        {
            case 'i':
                region_cfg.region = atoi(optarg);
                break;
            case 's':
                region_cfg.status = atoi(optarg);
                break;
            default:
                break;
        }
    }

    if(vin_dvp_fd >= 0)
    {
        ioctl(vin_dvp_fd , VINDVP_SET_COMBINED_REGION_FREEZE ,&region_cfg);
    }

    return 0;
}

static int vindvp_capture_pictrue(int argc , char *argv[])
{
    int opt;
    opterr = 0;
    optind = 0;

    vindvp_combined_capture_mode_e capture_mode = VINDVP_COMBINED_CAPTRUE_ORIGINAL;

    while((opt = getopt(argc , argv , "m:")) != EOF)
    {
        switch(opt)
        {
            case 'm':
                capture_mode = (vindvp_combined_capture_mode_e)atoi(optarg);
                break;
           
            default:
                break;
        }
    }

    if(vin_dvp_fd >= 0)
    {
        ioctl(vin_dvp_fd , VINDVP_CAPTURE_ONE_PICTURE , capture_mode);

        int ret;
        struct kshm_info kshm_hdl;

        if(false == creat_read_task)
        {
            ioctl(vin_dvp_fd , VINDVP_VIDEO_KSHM_ACCESS , &video_read_hdl);

            init_completion(&video_read_task_completion);
            video_stop_read = 0;
            ret = xTaskCreate(vin_dvp_video_read_thread , "video_read_thread" ,
                              0x1000 , &video_read_hdl , portPRI_TASK_HIGH , NULL);
            if(ret != pdTRUE)
            {
                printf("kshm recv thread create failed\n");
            }
            creat_read_task = true;
        }
        
    }
    return 0;
}

static int vindvp_get_video_info(int argc , char *argv[])
{
    int opt;
    struct vindvp_video_info video_info = { 0 };

    opterr = 0;
    optind = 0;

    if(vin_dvp_fd >= 0)
    {
        ioctl(vin_dvp_fd , VINDVP_GET_VIDEO_INFO , &video_info);
        printf("w:%d h:%d fps:%d\n", video_info.width, video_info.height, video_info.frame_rate);
    }

    return 0;

}

static int vindvp_get_video_buf(int argc , char *argv[])
{
    int buf_size = 0;
    void *buf = NULL;
    int opt;
    opterr = 0;
    optind = 0;
    bool capture_y_only = false;
    int rotate_mode = 0;

    while ((opt = getopt(argc , argv , "y:r:")) != EOF)
    {
        switch (opt)
        {
            case 'y':
                capture_y_only = atoi(optarg);
                break;
            case 'r':
                rotate_mode = atoi(optarg);
                break;
            default:
                break;
        }
    }
    if (vin_dvp_fd >= 0)
    {
        struct vindvp_video_info video_info = { 0 };
        ioctl(vin_dvp_fd , VINDVP_GET_VIDEO_INFO , &video_info);
        ioctl(vin_dvp_fd , VINDVP_SET_YUV_BUF_CAPTURE_FORMAT , capture_y_only);
        ioctl(vin_dvp_fd , VINDVP_SET_YUV_BUF_CAPTURE_ROTATE_MODE , rotate_mode);
        printf("w:%d h:%d fps:%d\n" , video_info.width , video_info.height , video_info.frame_rate);

        buf_size = video_info.width * video_info.height * 4;
        buf = mmap(0 , buf_size , PROT_READ | PROT_WRITE , MAP_SHARED , vin_dvp_fd , 0);
        printf("buf = 0x%x\n" , (int)buf);

        return 0;
    }
    else
    {
        return -1;
    }
}

static int vindvp_dump(int argc , char *argv[])
{
    int opt;
    opterr = 0;
    optind = 0;
    long buf_addr = 0;
    int len = 0;
    FILE *fd = 0;
    long long tmp = 0;
    char m_video_file_name[256] = { 0 };
    int index = 0;

    while ((opt = getopt(argc , argv , "b:s:i:")) != EOF)
    {
        switch (opt)
        {
            case 'b':
                tmp = strtoll(optarg , NULL , 16);
                buf_addr = (unsigned long)tmp;
                break;
            case 's':
                len = atoi(optarg);
                break;
            case 'i':
                index = atoi(optarg);
                break;
            default:
                break;
        }
    }
    sprintf(m_video_file_name , "/media/sda1/vindvp%d.bin" , index);
    fd = fopen(m_video_file_name , "w");
    //len = 8355840;
    printf("buf_addr = 0x%lx len = %d\n" , buf_addr , len);
    fwrite((char *)buf_addr , 1 , len , fd);
    fsync((int)fd);
    fclose(fd);

    return 0;
}

CONSOLE_CMD(vin_dvp , NULL , vindvp_enter , CONSOLE_CMD_MODE_SELF , "enter vin dvp or ahd")
CONSOLE_CMD(start , "vin_dvp" , vindvp_start , CONSOLE_CMD_MODE_SELF , "vin dvp_start")
CONSOLE_CMD(stop , "vin_dvp" , vindvp_stop , CONSOLE_CMD_MODE_SELF , "vin_dvp_stop")
CONSOLE_CMD(enable , "vin_dvp" , vindvp_enable , CONSOLE_CMD_MODE_SELF , "vin_dvp_enable")
CONSOLE_CMD(disable , "vin_dvp" , vindvp_disable , CONSOLE_CMD_MODE_SELF , "vin_dvp_disable")
CONSOLE_CMD(set_region , "vin_dvp" , vindvp_set_combine_regin_status , CONSOLE_CMD_MODE_SELF , "vin_set_set_region")
CONSOLE_CMD(capture , "vin_dvp" , vindvp_capture_pictrue , CONSOLE_CMD_MODE_SELF , "vin_dvp_capture_pictrue")
CONSOLE_CMD(get_video_info , "vin_dvp" , vindvp_get_video_info , CONSOLE_CMD_MODE_SELF , "get_video_info")
CONSOLE_CMD(get_video_buf , "vin_dvp" , vindvp_get_video_buf , CONSOLE_CMD_MODE_SELF , "get vin dvp video buf")
CONSOLE_CMD(dump , "vin_dvp" , vindvp_dump , CONSOLE_CMD_MODE_SELF , "dump data")
CONSOLE_CMD(set_kshm , "vin_dvp" , vin_dvp_set_to_kshm_setting , CONSOLE_CMD_MODE_SELF , "set_kshm(r:rotate mode s: scale enable w:scaled width h:scaled height v: 0:video mode 1:still pictrue mode)")
