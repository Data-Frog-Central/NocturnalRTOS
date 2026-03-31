#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <hcfota.h>

#include <libfdt.h>
#include <libfdt_env.h>
#include <fdt.h>
#include "fdt_api.h"

#include "util.h"

#include "iniparser/src/iniparser.h"

#define max(a, b)                                                              \
	({                                                                     \
		typeof(a) _a = a;                                              \
		typeof(b) _b = b;                                              \
		_a > _b ? _a : _b;                                             \
	})

enum {
	IH_COMP_NONE = 0, /*  No   Compression Used       */
	IH_COMP_GZIP, /* gzip  Compression Used       */
	IH_COMP_LZMA, /* lzma  Compression Used       */
};

enum {
	IH_DEVT_SPINOR = 0, /*  spi norflash       */
	IH_DEVT_SPINAND, /* spi nandflash       */
	IH_DEVT_NAND, /* nandflash       */
	IH_DEVT_EMMC, /* emmc or sd-card       */
};

enum {
	IH_ENTTRY_NORMAL = 0, /*  normal entry       */
	IH_ENTTRY_REMAP = 1, /*  remap entry       */
	IH_ENTTRY_DDRINFO = 2, /*  ddrinfo entry       */
	IH_ENTTRY_UPDATER = 3, /*  updater entry       */
	IH_ENTTRY_PARTINFO = 4, /*  partition info entry       */
};

extern uint32_t crc32(uint32_t crc, const uint8_t *p, uint32_t len);

static void print_usage(const char *prog)
{
	printf("Usage: %s [-iodeap]\n", prog);
	puts("  -i --ini       input ini file\n"
	     "  -o --output    output file\n"
	     "  -u --upgrade   hcfota for upgrade\n"
	     "  -d --dtb       DTB binary\n"
	     "  -e --erasenor  erase entire nor chip\n"
	     "  -a --erasenand erase entire nand chip\n"
	     "  -r --raminit   ram init file\n"
	     "  -p --updater   updater file\n"
	     "  -h --help      print this help\n");
}

static uint8_t dev2type(const char *device)
{
	if (!strncasecmp(device, "NOR", max(strlen(device), 3)))
		return (uint8_t)IH_DEVT_SPINOR;
	else if (!strncasecmp(device, "SPINAND", max(strlen(device), 7)))
		return (uint8_t)IH_DEVT_SPINAND;
	else if (!strncasecmp(device, "NAND", max(strlen(device), 4)))
		return (uint8_t)IH_DEVT_NAND;
	else if (!strncasecmp(device, "EMMC", max(strlen(device), 4)))
		return (uint8_t)IH_DEVT_EMMC;
	return (uint8_t)IH_DEVT_SPINOR;
}

static uint32_t get_filesize(const char *file)
{
	struct stat sb;

	if (stat(file, &sb) == -1)
		return (uint32_t)-1;
	return (uint32_t)sb.st_size;
}

int main(int argc, char *argv[])
{
	const char *ini_name = NULL;
	const char *dtb_path = NULL;
	const char *output = NULL;
	char *dtb;
	size_t len;
	dictionary *ini;
	uint32_t version;
	const char *product;
	int erase_nor_chip = 0;
	int erase_nand_chip = 0;
	const char *key;
	int spinor_en_cs0 = 0;
	int spinand_en_cs0 = 0;
	int spinand_en_cs1 = 0;
	int nand_en = 0;
	int emmc_v20_left_en = 0;
	int emmc_v20_top_en = 0;
	int emmc_v30_en = 0;
	char str[4096];
	char str2[4096];
	char *pstr2 = NULL;
	char *tmp;
	FILE *fp;
	size_t ret;

	int for_upgrade = 0;
	int version_check = 0;

	const char *device;
	const char *file;
	int offset;
	int size;

	const char *old_device;
	const char *name;
	const char *new_device;
	int old_dev_index;
	int new_dev_index;
	int old_offset;
	int new_offset;
	int old_size;
	int new_size;

	uint32_t offset_in_payload = 0;
	void *payload = NULL;
	void *fota;
	int fota_size;

	uint32_t crc;

	struct hcfota_header head = { 0 };
	struct hcfota_payload_header ph = { 0 };
	int skip_persistentmem = 1;

	int np;
	const char *persistentmem_mtdname = NULL;
	const char *raminit = NULL;
	const char *updater = NULL;

	struct hcfota_entry_info entry_info = { 0 };

	while (1) {
		static const struct option lopts[] = {
			{ "ini",       1, 0, 'i' },
			{ "output",    1, 0, 'o' },
			{ "upgrade",   0, 0, 'u' },
			{ "dtb",       1, 0, 'd' },
			{ "erasenor",  0, 0, 'e' },
			{ "erasenand", 0, 0, 'a' },
			{ "versioncheck", 1, 0, 'c' },
			{ "ram",       1, 0, 'r' },
			{ "updater",   1, 0, 'p' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "i:o:d:eauhr:p:", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'd':
			dtb_path = optarg;
			break;
		case 'i':
			ini_name = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case 'u':
			for_upgrade = 1;
			break;
		case 'e':
			erase_nor_chip = 1;
			break;
		case 'a':
			erase_nand_chip = 1;
			break;
		case 'c':
			version_check = !!atoi(optarg);
			break;
		case 'r':
			raminit = optarg;
			break;
		case 'p':
			updater = optarg;
			break;
		case 'h':
		default:
			print_usage(argv[0]);
			return -1;
		}
	}

	if (!ini_name || !dtb_path) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	dtb = utilfdt_read(dtb_path, &len);
	if (!dtb)
		die("could not read: %s\n", dtb_path);

	fdt_setup(dtb);
	np = fdt_get_node_offset_by_path("/hcrtos/persistentmem");
	if (np < 0) {
		fprintf(stderr, "cannot find persistentmem in DTS\n");
		return -1;
	}

	if (fdt_get_property_string_index(np, "mtdname", 0, &persistentmem_mtdname)) {
		fprintf(stderr, "cannot find mtdname property in /hcrtos/persistentmem in DTS\n");
		return -1;
	}

	ini = iniparser_load(ini_name);
	if (ini == NULL) {
		fprintf(stderr, "cannot parse file: %s\n", ini_name);
		return -1;
	}

	key = "SYSINFO:VERSION";
	version = iniparser_getint(ini, key, -1);
	if (version == -1) {
		printf("%s not found!\n", key);
		return EXIT_FAILURE;
	}

	key = "SYSINFO:PRODUCT";
	product = iniparser_getstring(ini, key, NULL);
	if (!product) {
		printf("%s not found!\n", key);
		return EXIT_FAILURE;
	}

	key = "SYSINFO:SPINOR_EN_CS0";
	spinor_en_cs0 = iniparser_getboolean(ini, key, -1);
	if (spinor_en_cs0 == -1) {
		spinor_en_cs0 = 0;
	}

	key = "SYSINFO:SPINAND_EN_CS0";
	spinand_en_cs0 = iniparser_getboolean(ini, key, -1);
	if (spinand_en_cs0 == -1) {
		spinand_en_cs0 = 0;
	}

	key = "SYSINFO:SPINAND_EN_CS1";
	spinand_en_cs1 = iniparser_getboolean(ini, key, -1);
	if (spinand_en_cs1 == -1) {
		spinand_en_cs1 = 0;
	}

	key = "SYSINFO:NAND_EN";
	nand_en = iniparser_getboolean(ini, key, -1);
	if (nand_en == -1) {
		nand_en = 0;
	}

	key = "SYSINFO:EMMC_V20_LEFT_EN";
	emmc_v20_left_en = iniparser_getboolean(ini, key, -1);
	if (emmc_v20_left_en == -1) {
		emmc_v20_left_en = 0;
	}

	key = "SYSINFO:EMMC_V20_TOP_EN";
	emmc_v20_top_en = iniparser_getboolean(ini, key, -1);
	if (emmc_v20_top_en == -1) {
		emmc_v20_top_en = 0;
	}

	key = "SYSINFO:EMMC_V30_EN";
	emmc_v30_en = iniparser_getboolean(ini, key, -1);
	if (emmc_v30_en == -1) {
		emmc_v30_en = 0;
	}

	if (raminit != NULL && get_filesize(raminit) != -1) {
		ph.entry[ph.entry_number].upgrade.length = get_filesize(raminit);
		ph.entry[ph.entry_number].upgrade.index = ph.entry_number;
		ph.entry[ph.entry_number].upgrade.dev_index = -1;
		ph.entry[ph.entry_number].upgrade.dev_type = 0;
		ph.entry[ph.entry_number].upgrade.upgrade_enable = 0;
		ph.entry[ph.entry_number].upgrade.offset_in_payload = offset_in_payload;
		ph.entry[ph.entry_number].upgrade.erase_length = 0;
		ph.entry[ph.entry_number].upgrade.entry_type = IH_ENTTRY_DDRINFO;
		ph.entry[ph.entry_number].upgrade.offset_in_dev = -1;
		ph.entry[ph.entry_number].upgrade.offset_in_blkdev = -1;
		if (ph.entry[ph.entry_number].upgrade.length > 0) {
			payload = realloc(payload, offset_in_payload + ph.entry[ph.entry_number].upgrade.length);
			fp = fopen(raminit, "rb");
			fseek(fp, 0, SEEK_SET);
			ret = fread(payload + offset_in_payload, 1, ph.entry[ph.entry_number].upgrade.length, fp);
			if (ret != ph.entry[ph.entry_number].upgrade.length)
				printf("Error read %s!\n", str);
			fclose(fp);
		}

		offset_in_payload += ph.entry[ph.entry_number].upgrade.length;
		snprintf(entry_info.names[ph.entry_number], sizeof(entry_info.names[ph.entry_number]), "ddrinfo");
		ph.entry_number++;
	}

	if (updater != NULL && get_filesize(updater) != -1) {
		ph.entry[ph.entry_number].upgrade.length = get_filesize(updater);
		ph.entry[ph.entry_number].upgrade.index = ph.entry_number;
		ph.entry[ph.entry_number].upgrade.dev_index = -1;
		ph.entry[ph.entry_number].upgrade.dev_type = 0;
		ph.entry[ph.entry_number].upgrade.upgrade_enable = 0;
		ph.entry[ph.entry_number].upgrade.offset_in_payload = offset_in_payload;
		ph.entry[ph.entry_number].upgrade.erase_length = 0;
		ph.entry[ph.entry_number].upgrade.entry_type = IH_ENTTRY_UPDATER;
		ph.entry[ph.entry_number].upgrade.offset_in_dev = -1;
		ph.entry[ph.entry_number].upgrade.offset_in_blkdev = -1;
		if (ph.entry[ph.entry_number].upgrade.length > 0) {
			payload = realloc(payload, offset_in_payload + ph.entry[ph.entry_number].upgrade.length);
			fp = fopen(updater, "rb");
			fseek(fp, 0, SEEK_SET);
			ret = fread(payload + offset_in_payload, 1, ph.entry[ph.entry_number].upgrade.length, fp);
			if (ret != ph.entry[ph.entry_number].upgrade.length)
				printf("Error read %s!\n", str);
			fclose(fp);
		}

		offset_in_payload += ph.entry[ph.entry_number].upgrade.length;
		snprintf(entry_info.names[ph.entry_number], sizeof(entry_info.names[ph.entry_number]), "updater");
		ph.entry_number++;
	}

	for (int i = 0; i < sizeof(ph.entry) / sizeof(ph.entry[0]); i++) {
		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:OLDDEVICE", i);
		old_device = iniparser_getstring(ini, str, NULL); 
		if (!old_device)
			continue;

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:NEWDEVICE", i);
		new_device = iniparser_getstring(ini, str, NULL); 
		if (!new_device) {
			printf("Warning: %s is missing!\n", str);
			continue;
		}

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:OLDINDEX", i);
		old_dev_index = iniparser_getint(ini, str, -1);

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:NEWINDEX", i);
		new_dev_index = iniparser_getint(ini, str, -1);

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:OLDOFFSET", i);
		old_offset = iniparser_getint(ini, str, -1);
		if (old_offset == -1) {
			printf("Warning: %s is missing\n", str);
			continue;
		}

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:NEWOFFSET", i);
		new_offset = iniparser_getint(ini, str, -1);
		if (new_offset == -1) {
			printf("Warning: %s is missing\n", str);
			continue;
		}

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:OLDSIZE", i);
		old_size = iniparser_getint(ini, str, -1);
		if (old_size == -1) {
			printf("Warning: %s is missing\n", str);
			continue;
		}

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:NEWSIZE", i);
		new_size = iniparser_getint(ini, str, -1);
		if (new_size == -1) {
			printf("Warning: %s is missing\n", str);
			continue;
		}
		ph.entry[ph.entry_number].backup.index = ph.entry_number;
		ph.entry[ph.entry_number].backup.old_dev_index = old_dev_index;
		ph.entry[ph.entry_number].backup.old_dev_type = dev2type(old_device);
		ph.entry[ph.entry_number].backup.upgrade_enable = 1;
		ph.entry[ph.entry_number].backup.old_offset_in_dev = old_offset;
		ph.entry[ph.entry_number].backup.old_length = old_size;
		ph.entry[ph.entry_number].backup.entry_type = IH_ENTTRY_REMAP;
		ph.entry[ph.entry_number].backup.new_dev_index = new_dev_index;
		ph.entry[ph.entry_number].backup.new_dev_type = dev2type(new_device);
		ph.entry[ph.entry_number].backup.new_offset_in_dev = new_offset;
		ph.entry[ph.entry_number].backup.new_length = new_size;
		ph.entry_number++;
	}

rescan:
	for (int i = 0; i < sizeof(ph.entry) / sizeof(ph.entry[0]); i++) {
		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:OLDDEVICE", i);
		old_device = iniparser_getstring(ini, str, NULL); 
		if (old_device)
			continue;

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:NAME", i);
		name = iniparser_getstring(ini, str, NULL);
		if (skip_persistentmem) {
			if (name != NULL) {
				if (!strncmp(name, persistentmem_mtdname, max(strlen(name), strlen(persistentmem_mtdname))))
					continue;
			}
		} else {
			if (name == NULL)
				continue;
			if (strncmp(name, persistentmem_mtdname, max(strlen(name), strlen(persistentmem_mtdname))))
				continue;
		}

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:DEVICE", i);
		device = iniparser_getstring(ini, str, NULL); 
		if (!device) {
			continue;
		}

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:OFFSET", i);
		offset = iniparser_getint(ini, str, -1);
		if (offset == -1) {
			printf("Warning: %s is missing\n", str);
			continue;
		}

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:SIZE", i);
		size = iniparser_getint(ini, str, -1);
		if (size == -1) {
			printf("Warning: %s is missing\n", str);
			continue;
		}

		if (for_upgrade && !strncmp(name, "boot", max(strlen(name), strlen("boot")))) {
			/* Normally do not upgrade boot */
			/* User can manually generate HCFOTA if really need to upgrade the boot partition */
			continue;
		}

		memset(str, 0, sizeof(str));
		snprintf(str, sizeof(str), "PARTITION%d:FILE", i);
		file = iniparser_getstring(ini, str, NULL); 
		if (!strncasecmp(file, "null", max(strlen(file), 4))) {
			file = NULL;
		} else {
			memset(str, 0, sizeof(str));
			sprintf(str, "%s", ini_name);
			tmp = strrchr(str, '/');
			if (tmp == NULL) {
				memset(str, 0, sizeof(str));
				sprintf(str, "%s", file);
			} else {
				sprintf(tmp, "/%s", file);
			}
		}

		if (file == NULL)
			ph.entry[ph.entry_number].upgrade.length = 0;
		else
			ph.entry[ph.entry_number].upgrade.length = get_filesize(str);

		if (ph.entry[ph.entry_number].upgrade.length == (uint32_t)-1) {
			printf("Error %s not found!\n", str);
			return EXIT_FAILURE;
		}

		if (ph.entry[ph.entry_number].upgrade.length > size) {
			printf("Error: The size of %s is %d(0x%x), which is bigger than partition size %d(0x%x)!\n",
			       file, ph.entry[ph.entry_number].upgrade.length, ph.entry[ph.entry_number].upgrade.length,
			       size, size);
			printf("Error: Please increase the partitions size or make the file %s smaller!\n", file);
			exit(1);
		}

		ph.entry[ph.entry_number].upgrade.index = ph.entry_number;
		ph.entry[ph.entry_number].upgrade.dev_index = -1;
		ph.entry[ph.entry_number].upgrade.dev_type = dev2type(device);
		ph.entry[ph.entry_number].upgrade.upgrade_enable = 1;
		ph.entry[ph.entry_number].upgrade.offset_in_payload = offset_in_payload;
		ph.entry[ph.entry_number].upgrade.erase_length = size;
		ph.entry[ph.entry_number].upgrade.entry_type = IH_ENTTRY_NORMAL;
		ph.entry[ph.entry_number].upgrade.offset_in_dev = offset;
		ph.entry[ph.entry_number].upgrade.offset_in_blkdev = -1;
		if (ph.entry[ph.entry_number].upgrade.length > 0) {
			payload = realloc(payload, offset_in_payload + ph.entry[ph.entry_number].upgrade.length);
			fp = fopen(str, "rb");
			fseek(fp, 0, SEEK_SET);
			ret = fread(payload + offset_in_payload, 1, ph.entry[ph.entry_number].upgrade.length, fp);
			if (ret != ph.entry[ph.entry_number].upgrade.length)
				printf("Error read %s!\n", str);
			fclose(fp);
		}

		offset_in_payload += ph.entry[ph.entry_number].upgrade.length;
		snprintf(entry_info.names[ph.entry_number], sizeof(entry_info.names[ph.entry_number]), "%s", name);
		ph.entry_number++;
	}

	if (skip_persistentmem == 1) {
		skip_persistentmem = 0;
		goto rescan;
	}

	ph.entry[ph.entry_number].upgrade.length = sizeof(entry_info);
	ph.entry[ph.entry_number].upgrade.index = ph.entry_number;
	ph.entry[ph.entry_number].upgrade.dev_index = -1;
	ph.entry[ph.entry_number].upgrade.dev_type = 0;
	ph.entry[ph.entry_number].upgrade.upgrade_enable = 0;
	ph.entry[ph.entry_number].upgrade.offset_in_payload = offset_in_payload;
	ph.entry[ph.entry_number].upgrade.erase_length = 0;
	ph.entry[ph.entry_number].upgrade.entry_type = IH_ENTTRY_PARTINFO;
	ph.entry[ph.entry_number].upgrade.offset_in_dev = -1;
	ph.entry[ph.entry_number].upgrade.offset_in_blkdev = -1;
	if (ph.entry[ph.entry_number].upgrade.length > 0) {
		payload = realloc(payload, offset_in_payload + ph.entry[ph.entry_number].upgrade.length);
		memcpy(payload + offset_in_payload, &entry_info, sizeof(entry_info));
	}

	offset_in_payload += ph.entry[ph.entry_number].upgrade.length;
	snprintf(entry_info.names[ph.entry_number], sizeof(entry_info.names[ph.entry_number]), "updater");
	ph.entry_number++;

	head.compress_type = IH_COMP_NONE;
	if (for_upgrade) {
		head.ignore_version_check = 0;
	} else {
		head.ignore_version_check = !version_check;
	}

	head.ignore_version_update = 1;

	head.version = version;
	head.uncompressed_length = sizeof(ph) + offset_in_payload;
	snprintf((char *)&head.board[0], sizeof(head.board), "%s", product);
	head.payload_size = sizeof(ph) + offset_in_payload;
	head.spinor_en_cs0 = spinor_en_cs0;
	head.spinand_en_cs0 = spinand_en_cs0;
	head.spinand_en_cs1 = spinand_en_cs1;
	head.nand_en = nand_en;
	head.emmc_v20_left_en = emmc_v20_left_en;
	head.emmc_v20_top_en = emmc_v20_top_en;
	head.emmc_v30_en = emmc_v30_en;
	head.erase_nor_chip = erase_nor_chip;
	head.erase_nand_chip = erase_nand_chip;

	crc = crc32(0, (const uint8_t *)&ph, sizeof(ph));
	crc = crc32(crc, (const uint8_t *)payload, offset_in_payload);
	ph.crc = crc;

	crc = crc32(0, (const uint8_t *)&head, sizeof(head));
	crc = crc32(crc, (const uint8_t *)&ph, sizeof(ph));
	crc = crc32(crc, (const uint8_t *)payload, offset_in_payload);
	head.crc = crc;

	fota_size = sizeof(head) + sizeof(ph) + offset_in_payload;
	fota = malloc(fota_size);
	memcpy(fota + 0, &head, sizeof(head));
	memcpy(fota + sizeof(head), &ph, sizeof(ph));
	memcpy(fota + sizeof(head) + sizeof(ph), payload, offset_in_payload);

	memset(str, 0, sizeof(str));
	memset(str2, 0, sizeof(str2));
	if (output) {
		sprintf(str, "%s", output);
		sprintf(str2, "%s", output);
		pstr2 = &str2[0];
		tmp = strrchr(str2, '.');
		sprintf(tmp, "_%s_%u.bin", product, version);
	} else {
		sprintf(str, "%s", ini_name);
		tmp = strrchr(str, '/');
		if (tmp == NULL) {
			memset(str, 0, sizeof(str));
			sprintf(str, "HCFOTA_%s_%u.bin", product, version);
		} else {
			sprintf(tmp, "/HCFOTA_%s_%u.bin", product, version);
		}
	}

	fp = fopen(str, "wb");
	if (!fp) {
		printf("Error create file %s!\n", str);
	}

	fwrite(fota, fota_size, 1, fp);
	fclose(fp);

	if (pstr2) {
		fp = fopen(pstr2, "wb");
		if (!fp) {
			printf("Error create file %s!\n", pstr2);
		}

		fwrite(fota, fota_size, 1, fp);
		fclose(fp);
	}

	return 0;
}
