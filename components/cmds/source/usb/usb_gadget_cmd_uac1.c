#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <kernel/lib/console.h>
#include <linux/bitops.h>
#include <linux/bits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <nuttx/fs/dirent.h>
#include <nuttx/fs/fs.h>
#include <nuttx/mtd/mtd.h>
#include <string.h>

#include <kernel/elog.h>

#include <kernel/drivers/hcusb.h>

static void usbd_uac_help_info(void)
{
    printf("usb device mode : uac\n");
}


int setup_usbd_uac1(int argc, char **argv)
{
    char ch;
    int i;
    const char *udc_name = NULL;
    int usb_port = 0;

    opterr = 0;
    optind = 0;

    elog_set_filter_tag_lvl("uac", ELOG_LVL_ALL);
    elog_set_filter_tag_lvl("usbd", ELOG_LVL_ALL);

    while ((ch = getopt(argc, argv, "hHsSp:P:")) != EOF) {
        switch(ch) {
            case 'h' :
            case 'H' :
                usbd_uac_help_info();
                return 0;
            case 'p' :
            case 'P' :
                usb_port = atoi(optarg);
                udc_name = get_udc_name(usb_port);
                if(udc_name == NULL){
                    printf("[error] parameter(-p {usb_port}) error,"
                        "please check for help information(cmd: g_mass_storage -h)\n");
                    return -1;
                }
                printf("==> set usb#%u as uac1 demo gadget\n", usb_port);
                break;
            case 's' :
            case 'S' :
                hcusb_gadget_uac_deinit();
                return 0;
            default:
                break;
        }
    }

    hcusb_set_mode(usb_port, MUSB_PERIPHERAL);
    if(!udc_name)
        hcusb_gadget_uac_init();
    else 
        hcusb_gadget_uac_specified_init(udc_name);
    return 0;
}
