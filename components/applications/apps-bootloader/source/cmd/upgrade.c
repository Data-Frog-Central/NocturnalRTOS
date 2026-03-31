#include <generated/br2_autoconf.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <kernel/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <kernel/module.h>
#include <kernel/lib/console.h>
#include <hcuapi/sys-blocking-notify.h>
#include <hcfota.h>
#include <kernel/completion.h>
#include <kernel/notify.h>
#include <linux/notifier.h>
#include <hcuapi/persistentmem.h>
#include <hcuapi/input.h>
#include <poll.h>
#include <kernel/lib/fdt_api.h>
#include <kernel/lib/libfdt/libfdt.h>
#include <sys/types.h>
#include <sys/time.h>
#include "usbd_upgrade.h"
#include "show_upgrade_way.h"

#define max(a, b) ({\
		typeof(a) _a = a;\
		typeof(b) _b = b;\
		_a > _b ? _a : _b; })

#define min(a, b) ({\
		typeof(a) _a = a;\
		typeof(b) _b = b;\
		_a < _b ? _a : _b; })

struct hcfota_upgrade_key{
    int enable;

    int adc_key_fd;
    char adc_key_path[32];
    int adc_key_value;
};

#define HCFOTA_BIN "hcfota.bin"
#define HCFOTA_SYSTEM_DATA_BIN "system_data.bin"
static uint32_t HCFOTA_TIMEOUT = CONFIG_BOOT_HCFOTA_TIMEOUT;

static struct completion upgrade_done;
static struct completion storage_ready;
static struct hcfota_upgrade_key upgrade_key = {0};
#if defined(CONFIG_BOOT_AUTO_UPGRADE_SUPPORT_USBHOST)
static uint32_t WAIT_MSC_CONNECT_TIMEOUT = CONFIG_BOOT_WAIT_MSC_CONNECT_READY;
#endif

unsigned int mode;
int do_hcfota_upgrade(unsigned int ota_mode);

static int get_upgrade_key(void)
{
	static int np = -1;
	const char *status;
	int ret = 0;

    np = fdt_get_node_offset_by_path("/hcrtos/hcfota-upgrade");
    if(np < 0){
        printf("%s:%d: fdt_get_node_offset_by_path failed\n", __func__, __LINE__);
        return -1;
    }

    ret = fdt_get_property_string_index(np, "status", 0, &status);
    if(ret != 0){
        printf("%s:%d: fdt_get_property_u_32_index ir_key failed\n", __func__, __LINE__);
    }

    if(strcmp(status, "okay")){
        upgrade_key.enable = 0;
        printf("%s:%d: hcfota-upgrade is disable\n", __func__, __LINE__);
        return -1;
    }else{
        upgrade_key.enable = 1;
    }

    ret = fdt_get_property_u_32_index(np, "adc_key", 0, &upgrade_key.adc_key_value);
    if(ret != 0){
        printf("%s:%d: fdt_get_property_u_32_index ir_key failed\n", __func__, __LINE__);
    }

    printf("%s:%d: adc_key=%d\n", __func__, __LINE__, upgrade_key.adc_key_value);

    return 0;
}

#if 0
int upgrade_detect_key(void)
{
	int fd = 0;
	struct input_event t = {0};
	int detect = 0;
	int time = 150;
	int ret = 0;

    upgrade_key.adc_key_fd = -1;

    ret = get_upgrade_key();
    if(ret != 0){
        printf("%s:%d: get_upgrade_key failed\n", __func__, __LINE__);
        goto end;
    }

    if(upgrade_key.enable == 0){
        printf("%s:%d: hcfota-upgrade key is disable\n", __func__, __LINE__);
        goto end;
    }

#if defined(CONFIG_RC_HC) && defined(CONFIG_KEY_ADC)
    strcpy(upgrade_key.adc_key_path, "/dev/input/event1");
#elif defined(CONFIG_KEY_ADC)
    strcpy(upgrade_key.adc_key_path, "/dev/input/event0");
#endif

    printf("%s:%d: adc_key_path=%s\n", __func__, __LINE__, upgrade_key.adc_key_path);

    if(strlen(upgrade_key.adc_key_path)){
        upgrade_key.adc_key_fd = open(upgrade_key.adc_key_path, O_RDONLY);
        if(upgrade_key.adc_key_fd < 0){
            printf("%s:%d: open %s failed\n", __func__, __LINE__, upgrade_key.adc_key_path);
            goto end;
        }
    }

    while(time--){
        if(upgrade_key.adc_key_fd >= 0){
            if (read(upgrade_key.adc_key_fd, &t, sizeof(t)) == sizeof(t)) {
                printf("adc_key: type:%d, code:%d, value:%ld\n", t.type, t.code, t.value);
                if (t.type == EV_KEY) {
                    if (t.code == upgrade_key.adc_key_value && t.value) {
                        printf("%s:%d: adc upgrade key is Pressed\n", __func__, __LINE__);
                        detect = 1;
                        break;
                    }
                }
            }
        }

        usleep(1000);
    }

    if(time <= 0){
        printf("%s:%d: detect upgrade key timeout\n", __func__, __LINE__);
    }

end:
    if(upgrade_key.adc_key_fd < 0){
        close(upgrade_key.adc_key_fd);
    }

	return detect;
}
#else
#define UPGRADE_KEY_MAX_NUMBER 10
#define UPGRADE_KEY_MAX_INPUT_EVENTS 5
#define UPGRADE_KEY_REPEAT_LONG_KEY_TIME 3

/*
 * if no key press is detected, then exit
 * if upgrade key is detected, press and hold the key for 5 seconds
 */
int upgrade_detect_key(void)
{
    int fds[UPGRADE_KEY_MAX_INPUT_EVENTS] = { 0 };
    int key_value[UPGRADE_KEY_MAX_NUMBER] = { 0 };
    int key_code = 0;
    struct input_event t = {0};
    u32 nkeys = 0;
    int i, np, nfd = 0, detect = 0;
    int retry_ms = 300;
    struct timeval time_now = {0};
    double start_time = 0;
    double cur_time = 0;
    int res = 0;

    /* key init */
    nkeys = 0;
    for (i = 0; i < UPGRADE_KEY_MAX_NUMBER; i++)
        key_value[i] = -1;

    do {
        np = fdt_get_node_offset_by_path("/hcrtos/hcfota-upgrade");
        if (np < 0)
            break;
        const char *status = NULL;
        if (!fdt_get_property_string_index(np, "status", 0, &status) && !strcmp(status, "disabled"))
            break;
        if (fdt_get_property_data_by_name(np, "adc_key", &nkeys) == NULL)
            nkeys = 0;
        nkeys >>= 2;
        if (nkeys == 0)
            break;
        nkeys = min((int)nkeys, UPGRADE_KEY_MAX_NUMBER);
        for (i = 0; i < (int)nkeys; i++) {
            fdt_get_property_u_32_index(np, "adc_key", i, &key_value[i]);
        }
    } while (0);

    if (nkeys == 0)
        return 0;

    for (i = 0; i < UPGRADE_KEY_MAX_INPUT_EVENTS; i++) {
        char path[64];
        int fd;
        memset(path, 0, sizeof(path));
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        fd = open(path, O_RDONLY);
        if (fd < 0)
            continue;
        fds[nfd++] = fd;
    }

    if (nfd == 0)
        return 0;

    /* try to read upgrade key */
    for (i = 0; i < nfd; i++) {
        if (read(fds[i], &t, sizeof(t)) != sizeof(t))
            continue;

        if (t.type == EV_SYN || !t.value){
            continue;
        }

        /* get key_code, long key's code in t.value */
        if(t.type == EV_KEY){
            key_code = t.code;
        }else if(t.type == EV_MSC){
            key_code = t.value;
        }else{
            key_code = -2;  /* key_value defaut is -1 */
        }

        for (int j = 0; j < (int)nkeys; j++) {
            if (key_code == key_value[j]) {
                detect = 1;

                gettimeofday(&time_now, NULL);
                start_time = time_now.tv_sec;

                printf("t.code=%u, t.type=%u, t.value=%ld, start_time(%llu, %lu)\n",
                        t.code, t.type, t.value, time_now.tv_sec, time_now.tv_usec);

                break;
            }
        }
	}

    /* if no key press is detected, then return */
    if (detect == 0){
        return 0;
    }else{
    /* check is long key? */
        detect = 0;
    }

    /* if upgrade key is detected, press and hold the key for 5 seconds */
    while (1) {
        for (i = 0; i < nfd; i++) {
            if (read(fds[i], &t, sizeof(t)) != sizeof(t)){
                gettimeofday(&time_now, NULL);
                cur_time = time_now.tv_sec;
                if((cur_time - start_time) >= UPGRADE_KEY_REPEAT_LONG_KEY_TIME){
                    printf("wait upgrade key timeout\n");
                    detect = 0;
                    goto end;
                }

                /* the minimum time interval between button sending is 33ms */
                usleep(10000);
                continue;
            }

            if (t.type == EV_SYN){
                continue;
            }

            /* get key_code */
            if(t.type == EV_KEY){
                key_code = t.code;
            }else if(t.type == EV_MSC){
                key_code = t.value;
            }else{
                key_code = -2;  /* key_value defaut is -1 */
            }

            for (int j = 0; j < (int)nkeys; j++) {
                if (key_code == key_value[j]) {
                    //printf("t.code=%u, t.type=%u, t.value=%ld, time(%llu, %lu)\n",
                    //        t.code, t.type, t.value, time_now.tv_sec, time_now.tv_usec);

                    gettimeofday(&time_now, NULL);
                    cur_time = time_now.tv_sec;
                    if((cur_time - start_time) >= (UPGRADE_KEY_REPEAT_LONG_KEY_TIME - 1)){
                        detect = 1;
                        printf("t.code=%u, t.type=%u, t.value=%ld, end_time(%llu, %lu)\n",
                                t.code, t.type, t.value, time_now.tv_sec, time_now.tv_usec);
                        printf("Upgrade button detected Press %ds, enter the upgrade mode\n", UPGRADE_KEY_REPEAT_LONG_KEY_TIME);
                        goto end;
                    }else{
                        /* detected key up, then exit */
                        if(t.value == 0){
                            detect = 0;
                            printf("t.code=%u, t.type=%u, t.value=%ld, end_time(%llu, %lu)\n",
                                    t.code, t.type, t.value, time_now.tv_sec, time_now.tv_usec);
                            printf("detected key up, exit detection, time is %fs\n", (cur_time - start_time));
                            goto end;
                        }
                    }

                    break;
                }
            }
        }

        /* the minimum time interval between button sending is 33ms */
        usleep(10000);
    }

end:
    for (i = 0; i < nfd; i++)
        close(fds[i]);

    return detect;
}

#endif

int upgrade_force(void)
{
	unsigned long ota_mode = 0;

	ota_mode |= hcfota_reboot_ota_detect_mode_priority(HCFOTA_REBOOT_OTA_DETECT_USB_HOST, 0);
	ota_mode |= hcfota_reboot_ota_detect_mode_priority(HCFOTA_REBOOT_OTA_DETECT_SD, 1);
	ota_mode |= hcfota_reboot_ota_detect_mode_priority(HCFOTA_REBOOT_OTA_DETECT_NETWORK, 2);
	ota_mode |= hcfota_reboot_ota_detect_mode_priority(HCFOTA_REBOOT_OTA_DETECT_USB_DEVICE, 3);

	return do_hcfota_upgrade(ota_mode);
}

static int get_externel_flash_type(void)
{
    hcfota_external_flash_type_e flash_type = HCFOTA_EXTERNAL_FLASH_NULL;

#ifdef CONFIG_BOOT_UPGRADE_EXTERNAL_FLASH_EMMC
    flash_type = HCFOTA_EXTERNAL_FLASH_EMMC;
#endif

#ifdef CONFIG_BOOT_UPGRADE_EXTERNAL_FLASH_NAND
    flash_type = HCFOTA_EXTERNAL_FLASH_NAND;
#endif

#ifdef CONFIG_BOOT_UPGRADE_EXTERNAL_FLASH_SDCARD
    flash_type = HCFOTA_EXTERNAL_FLASH_SDCARD;
#endif

    return flash_type;
}

static unsigned long get_ota_detect_mode(void)
{
	int fd;
	struct persistentmem_node_create new_node;
	struct persistentmem_node node;
	struct sysdata sysdata = { 0 };

	fd = open("/dev/persistentmem", O_SYNC | O_RDWR);
	if (fd < 0) {
		printf("open /dev/persistentmem failed\n");
		return HCFOTA_REBOOT_OTA_DETECT_NONE;
	}

	node.id = PERSISTENTMEM_NODE_ID_SYSDATA;
	node.offset = 0;
	node.size = sizeof(struct sysdata);
	node.buf = &sysdata;
	if (ioctl(fd, PERSISTENTMEM_IOCTL_NODE_GET, &node) < 0) {
		new_node.id = PERSISTENTMEM_NODE_ID_SYSDATA;
		new_node.size = sizeof(struct sysdata);
		if (ioctl(fd, PERSISTENTMEM_IOCTL_NODE_CREATE, &new_node) < 0) {
			printf("get sysdata failed\n");
			close(fd);
			return HCFOTA_REBOOT_OTA_DETECT_NONE;
		}
	}

	close(fd);

	return sysdata.ota_detect_modes;
}

static int set_ota_detect_mode(unsigned long mode)
{
	int fd;
	struct persistentmem_node node;
	struct sysdata sysdata = { 0 };

	fd = open("/dev/persistentmem", O_SYNC | O_RDWR);
	if (fd < 0) {
		printf("open /dev/persistentmem failed\n");
		return -1;
	}

	sysdata.ota_detect_modes = mode;
	node.id = PERSISTENTMEM_NODE_ID_SYSDATA;
	node.offset = offsetof(struct sysdata, ota_detect_modes);
	node.size = sizeof(sysdata.ota_detect_modes);
	node.buf = &sysdata.ota_detect_modes;
	if (ioctl(fd, PERSISTENTMEM_IOCTL_NODE_PUT, &node) < 0) {
		printf("put sysdata failed\n");
		close(fd);
		return -1;
	}

	return 0;
}


#if defined(CONFIG_BOOT_UPGRADE_SUPPORT_USBHOST) || defined(CONFIG_BOOT_UPGRADE_SUPPORT_SD)

static int usb_mmc_upgrade_main(void *dev)
{
	int ret = -1, i = 0;
	DIR *dirp;
	struct stat st;
	char entryp_path[512];
	char dir_path[512];
	char tmp_buf[4] = { 0 };

	int dir_name_len = 0;
	snprintf(dir_path, sizeof(dir_path), "/media/%s", (char *)dev);

	dirp = opendir(dir_path);
	if (dirp == NULL)
		printf("open dir %s failed\n", dir_path);

	/* Read each directory entry */
	FAR struct dirent *entryp;

	for (;;) {
		entryp = readdir(dirp);
		if (entryp == NULL) {
			/* Finished with this directory */
			break;
		}

		if (entryp->d_type == DT_DIR)
			continue;

		memset(entryp_path, 0, sizeof(entryp_path));
		strcat(entryp_path, dir_path);
		strcat(entryp_path, "/");
		strcat(entryp_path, entryp->d_name);

		if ((strncmp("HCFOTA", entryp->d_name, 6) == 0 ) || (strncmp("hcfota", entryp->d_name, 6) == 0)) {
			dir_name_len = strlen(entryp->d_name);
			for (i = 0; i < 3; i++) {
				tmp_buf[i] = entryp->d_name[dir_name_len - 3 + i];
			}
			if (((strncmp(tmp_buf, "bin", 3) == 0) || (strncmp(tmp_buf, "BIN", 3) == 0))) {
				complete(&storage_ready);
				printf("==> upgrade from = %s\n", entryp_path);
				ret = hcfota_from_path(entryp_path, hcfota_report, 0);
				closedir(dirp);
				return ret;
			}
		}
	}

	closedir(dirp);

	return ret;
}

static int fs_mount_notify(struct notifier_block *self, unsigned long action,
			   void *dev)
{
	char system_data_path[64] = { 0 };
	int upgrade = -1;

	switch (action) {
	case USB_MSC_NOTIFY_MOUNT: {
		printf("%s:%d ota mode = %d\n", __func__, __LINE__, mode);
		switch (mode) {
		case HCFOTA_REBOOT_OTA_DETECT_USB_HOST: {
			if (strncmp((void *)dev, "sd", 2) == 0) {
				upgrade = usb_mmc_upgrade_main(dev);
			}
			break;
		}
		case HCFOTA_REBOOT_OTA_DETECT_SD: {
			if (strncmp((void *)dev, "mmc", 3) == 0) {
				upgrade = usb_mmc_upgrade_main(dev);
			}
			break;
		}
		default:
			break;
		}

		if (upgrade == 0) {
			printf("%s:%d: upgrade success, just reboot to launch new firmware\n",
			       __func__, __LINE__);
			/* upgrade success, just reboot to launch new firmware */
			set_ota_detect_mode(HCFOTA_REBOOT_OTA_DETECT_NONE);

			printf("%s:%d: begin to reset system\n", __func__, __LINE__);

			complete(&upgrade_done);
			reset();
		} else if (upgrade == HCFOTA_ERR_VERSION) {
			printf("Version check failed, booting from old system!\n");
			complete(&upgrade_done);
		} else {
			/* No firmware to upgrade */
			printf("%s:%d:%s No firmware to upgrade\n", __func__, __LINE__, (char *)dev);
		}
	}
	}

	return NOTIFY_OK;
}

static struct notifier_block fs_mount = {
       .notifier_call = fs_mount_notify,
};
#endif

int do_hcfota_upgrade(unsigned int ota_mode)
{
	int i = 0;
	char *excludes_mmc[] = {
		/* usb support */
		"usb",
		"musb_driver",
		"hc16xx_driver",
		"hcdisk_driver",
		"usb_storage_driver",
		/* usb gadget support */
		"mass_storage",
		/* usb host support */
		"usb_core",
		};

	char *excludes_usb[] = {
		/* mmc/sd support */
		"hc15_mmc_device",
		"hcmmc_device",
		};

	if (ota_mode == HCFOTA_REBOOT_OTA_DETECT_NONE)
		return 0;

	/* do upgrade */
 	for (i = 0; i < 8; i++) {
		mode = hcfota_reboot_get_ota_detect_mode_priority(ota_mode, i);

		switch (mode) {
#if defined(CONFIG_BOOT_UPGRADE_SUPPORT_USBDEVICE)
		case HCFOTA_REBOOT_OTA_DETECT_USB_DEVICE: {
			if (module_init2("all", 2, excludes_usb) != 0) {
				break;
			}
			create_usbd_upgarde_task();
			break;
		}
#endif
#if defined(CONFIG_BOOT_UPGRADE_SUPPORT_USBHOST)
		case HCFOTA_REBOOT_OTA_DETECT_USB_HOST: {
			init_completion(&upgrade_done);
			init_completion(&storage_ready);
			sys_register_notify(&fs_mount);
			if (module_init2("all", 2, excludes_usb) != 0) {
				break;
			}
#if defined(CONFIG_BOOT_AUTO_UPGRADE_SUPPORT_USBHOST)
			if (wait_for_completion_timeout(&storage_ready, WAIT_MSC_CONNECT_TIMEOUT) == 0) {
				printf("usbhost auto upgrade timeout!\n");
				break;
			}
#else
			if (wait_for_completion_timeout(&storage_ready, HCFOTA_TIMEOUT) == 0) {
				printf("usbhost upgrade timeout!\n");
				break;
			}
#endif
			wait_for_completion(&upgrade_done);
			break;
		}
#endif
#if defined(CONFIG_BOOT_STARTUP_FROM_EMMC_SD)
		case HCFOTA_REBOOT_OTA_DETECT_SD: {
			printf("flash + sd/mmc case no support sd ota upgrade\n");
			break;
		}
#else
#if defined (CONFIG_BOOT_UPGRADE_SUPPORT_SD)
		case HCFOTA_REBOOT_OTA_DETECT_SD: {
			init_completion(&upgrade_done);
			init_completion(&storage_ready);
			sys_register_notify(&fs_mount);
			if (module_init2("all", 7, excludes_mmc) != 0) {
				break;
			}
			if (wait_for_completion_timeout(&storage_ready, HCFOTA_TIMEOUT) == 0) {
				printf("sd/emmc upgrade timeout\n");
				break;
			}
			wait_for_completion(&upgrade_done);
			break;
		}
#endif
#endif
#ifdef CONFIG_BOOT_UPGRADE_SUPPORT_NETWORK
		case HCFOTA_REBOOT_OTA_DETECT_NETWORK: {
			printf("Not support upgrade from network!\n");
			break;
		}
#endif
		default:
			break;
		}
	}

	return 0;
}

int upgrade_detect(void)
{
	int fd;
	struct persistentmem_node_create new_node;
	struct persistentmem_node node;
	struct sysdata sysdata = { 0 };

	if (upgrade_detect_key()) {
		return upgrade_force();
	}

	return do_hcfota_upgrade(get_ota_detect_mode());
}

static int do_upgrade(int argc, char **argv)
{

	if (argc == 1)
		return upgrade_detect();

	if (argc == 2) {
		module_init("usb_core");
		module_init("hcmmc_device");
		module_init("hc15_mmc_device");
		return hcfota_url(argv[1], hcfota_report, 0);
	}

	return 0;
}

CONSOLE_CMD(upgrade, NULL, do_upgrade, CONSOLE_CMD_MODE_SELF, "upgrade firmware")
