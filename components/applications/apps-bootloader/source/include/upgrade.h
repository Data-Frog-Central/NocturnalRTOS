#ifndef _BOOT_UPGRADE_H
#define _BOOT_UPGRADE_H

#include <generated/br2_autoconf.h>

#ifdef CONFIG_BOOT_UPGRADE
int upgrade_detect(void);
int upgrade_force(void);
int upgrade_detect_key(void);
int do_hcfota_upgrade(unsigned int ota_mode);
#else
static int upgrade_detect(void)
{
	return -1;
}
static int upgrade_force(void)
{
	return -1;
}
static int upgrade_detect_key(void)
{
	return -1;
}
int do_hcfota_upgrade(unsigned int ota_mode)
{
	return -1;
}
#endif

#endif
