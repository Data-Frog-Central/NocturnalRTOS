#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <ctype.h>
#include <inttypes.h>

#include <libfdt.h>
#include <libfdt_env.h>
#include <fdt.h>
#include "fdt_api.h"

#include "util.h"

#define max(a, b)                                                              \
	({                                                                     \
		typeof(a) _a = a;                                              \
		typeof(b) _b = b;                                              \
		_a > _b ? _a : _b;                                             \
	})

#define FDT_MAGIC_SIZE	4
#define MAX_VERSION 17

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)	((void *)(ALIGN((unsigned long)(p), (a))))
#define GET_CELL(p)	(p += 4, *((const fdt32_t *)(p-4)))
static void print_usage(const char *prog)
{
	printf("Usage: %s [-iosftenpb]\n", prog);
	puts("  -i --input    input file\n"
	     "  -o --output   output file\n"
	     "  -s --size     bootloader size\n"
	     "  -f --from     bootloader address in sflash\n"
	     "  -t --to       bootloader address in dram\n"
	     "  -e --entry    bootloader entry address\n"
	     "  -n --nand     spi nand boot\n"
	     "  -p --pagesize spi nand pagesize, default 0x800\n"
	     "  -b --erasesize spi nand erasesize, default 0x20000\n"
	     "  -I --irq      hcprogrammer usb irq detect timeout, default 300 (milliseconds)\n"
	     "  -S --sync     hcprogrammer usb sync detect timeout, default 1000 (milliseconds)\n"
	     "  -A --portA    hcprogrammer support usb port A (port 0), default 1(enable)\n"
	     "  -B --portB    hcprogrammer support usb port B (port 1), default 1(enable)\n"
	     "  -d --dtb      input dtb file\n");
}

static bool valid_header(char *p, off_t len)
{
	if (len < sizeof(struct fdt_header) ||
	    fdt_magic(p) != FDT_MAGIC ||
	    fdt_version(p) > MAX_VERSION ||
	    fdt_last_comp_version(p) > MAX_VERSION ||
	    fdt_totalsize(p) >= len ||
	    fdt_off_dt_struct(p) >= len ||
	    fdt_off_dt_strings(p) >= len)
		return 0;
	else
		return 1;
}

int main(int argc, char *argv[])
{
	uint32_t magic;
	uint32_t chip;
	uint32_t size = 0, from = 0, to = 0, entry = 0;
	uint32_t is_nand = 0, nand_pagesize = 0x800, nand_erasesize = (0x800 * 64);
	char *input = NULL;
	char *output = NULL;
	char *buf = NULL;
	char *ptr = NULL;
	struct stat sb;
	FILE *fpin, *fpout;
	size_t ret = -1;
	uint32_t hcprogrammer_irq_timeout = 300;
	uint32_t hcprogrammer_sync_timeout = 1000;
	uint32_t hcprogrammer_usb0_en = 1;
	uint32_t hcprogrammer_usb1_en = 1;
	uint32_t spi_wire = 1;
	uint32_t spi_sclk = 27000000;
	unsigned short scpu_clk = 0; /* scpu clock selection, default (594 MHz) */
	unsigned short scpu_dpll_clk = 800; /* scpu dpll clock (MHz) */
	unsigned short mcpu_clk = 0; /* mcpu clock selection, default (594 MHz) */
	unsigned short mcpu_dpll_clk = 900; /* mcpu dpll clock (MHz) */
	const char *dtb_path = NULL;
	char *dtb;
	size_t len;
	int np;

	opterr = 0;
	optind = 0;

	while (1) {
		static const struct option lopts[] = {
			{ "input",  1, 0, 'i' },
			{ "output", 1, 0, 'o' },
			{ "size",   1, 0, 's' },
			{ "from",   1, 0, 'f' },
			{ "to",     1, 0, 't' },
			{ "entry",  1, 0, 'e' },
			{ "nand",  0, 0, 'n' },
			{ "pagesize",  1, 0, 'p' },
			{ "erasesize",  1, 0, 'b' },
			{ "irq",  1, 0, 'I' },
			{ "sync",  1, 0, 'S' },
			{ "portA",  1, 0, 'A' },
			{ "portB",  1, 0, 'B' },
			{ "dtb",  1, 0, 'd' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "i:o:s:f:t:e:np:b:I:S:A:B:d:", lopts, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'i':
			input = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case 's':
			size = strtoul(optarg, NULL, 0);
			size = ((size + 31) / 32) * 32;
			break;
		case 'f':
			from = strtoul(optarg, NULL, 0);
			break;
		case 't':
			to = strtoul(optarg, NULL, 0);
			to |= 0xa0000000;
			break;
		case 'e':
			entry = strtoul(optarg, NULL, 0);
			entry |= 0xa0000000;
			break;
		case 'n':
			is_nand = 1;
			break;
		case 'p':
			nand_pagesize = strtoul(optarg, NULL, 0);
			break;
		case 'b':
			nand_erasesize = strtoul(optarg, NULL, 0);
			break;
		case 'I':
			hcprogrammer_irq_timeout = strtoul(optarg, NULL, 0);
			break;
		case 'S':
			hcprogrammer_sync_timeout = strtoul(optarg, NULL, 0);
			break;
		case 'A':
			hcprogrammer_usb0_en = !!strtoul(optarg, NULL, 0);
			break;
		case 'B':
			hcprogrammer_usb1_en = !!strtoul(optarg, NULL, 0);
			break;
		case 'd':
			dtb_path = optarg;
			break;
		default:
			print_usage(argv[0]);
			return -1;
		}
	}

	if (!input) {
		printf("No input file!\n");
		print_usage(argv[0]);
		return -1;
	}

	if (stat(input, &sb) == -1) {
		printf("stat %s fail", input);
		return -1;
	}

	if (!output) {
		fpin = fpout = fopen(input, "wb+");
	} else {
		fpin = fopen(input, "rb");
		fpout = fopen(output, "wb");
	}

	if (dtb_path) {
		dtb = utilfdt_read(dtb_path, &len);
		if (!dtb)
			die("could not read: %s\n", dtb_path);

		if (!valid_header(dtb, len))
			die("%s: header is not valid\n", dtb_path);

		fdt_setup(dtb);
		np = fdt_get_node_offset_by_path("/hcrtos/sfspi");
		if (np >= 0) {
			uint32_t sclk = 0;
			fdt_get_property_u_32_index(np, "sclk", 0, &sclk);
			if (sclk != 0)
				spi_sclk = sclk;
		}

		if (is_nand) {
			np = fdt_get_node_offset_by_path("/hcrtos/sfspi/spi_nand_flash");
		} else {
			np = fdt_get_node_offset_by_path("/hcrtos/sfspi/spi_nor_flash");
		}
		if (np >= 0) {
			uint32_t wire = 0;
			fdt_get_property_u_32_index(np, "spi-rx-bus-width", 0, &wire);
			if (wire == 1 || wire == 2) {
				spi_wire = wire;
			} else if (wire == 4) {
				spi_wire = 2;
			}
		}
		np = fdt_get_node_offset_by_path("/hcrtos/scpu");
		if (np >= 0) {
			uint32_t val = 0;
			fdt_get_property_u_32_index(np, "clock", 0, &val);
			if (val != 0)
				scpu_clk = (unsigned short)val;
			val = 0;
			fdt_get_property_u_32_index(np, "scpu-dig-pll-clk", 0, &val);
			if (val != 0)
				scpu_dpll_clk = (unsigned short)val;
		}
		np = fdt_get_node_offset_by_path("/hcrtos/mcpu");
		if (np >= 0) {
			uint32_t val = 0;
			fdt_get_property_u_32_index(np, "clock", 0, &val);
			if (val != 0)
				mcpu_clk = (unsigned short)val;
			val = 0;
			fdt_get_property_u_32_index(np, "mcpu-dig-pll-clk", 0, &val);
			if (val != 0)
				mcpu_dpll_clk = (unsigned short)val;
		}
		free(dtb);
	}

	if (is_nand) {
		buf = malloc((unsigned int)sb.st_size + nand_pagesize);
		memset(buf, 0, (unsigned int)sb.st_size + nand_pagesize);
		*(uint32_t *)(buf + 0x0) = 0xaeeaaeea;
		*(uint32_t *)(buf + 0x4) = 0xaeeaaeea;
		ptr = buf + nand_pagesize;
		from += nand_pagesize;
	} else {
		buf = malloc((unsigned int)sb.st_size);
		memset(buf, 0, (unsigned int)sb.st_size);
		ptr = buf;
	}

	fseek(fpin, 0, SEEK_SET);
	if ((ret = fread(ptr, 1, (unsigned int)sb.st_size, fpin)) < 0) {
		printf("read input %s failed\n", input);
		free(buf);
		return -1;
	}

	printf("Pre-boot fixup hcprogrammer for USB0     [%s]\n", hcprogrammer_usb0_en ? "Enabled" : "Disabled");
	printf("Pre-boot fixup hcprogrammer for USB1     [%s]\n", hcprogrammer_usb1_en ? "Enabled" : "Disabled");
	printf("Pre-boot fixup hcprogrammer sync timeout [%d]\n", hcprogrammer_sync_timeout);
	printf("Pre-boot fixup hcprogrammer irq timeout  [%d]\n", hcprogrammer_irq_timeout);
	printf("Pre-boot fixup boot mode                 [%s]\n", is_nand ? "SPI Nand" : "SPI Nor");
	printf("Pre-boot fixup bootloader size           [%d]\n", size);
	printf("Pre-boot fixup bootloader flash address  [0x%08x]\n", from);
	printf("Pre-boot fixup bootloader dram address   [0x%08x]\n", to);
	printf("Pre-boot fixup bootloader entry address  [0x%08x]\n", entry);
	printf("Pre-boot fixup spi rx bus width          [%d]\n", spi_wire);
	printf("Pre-boot fixup spi clock                 [%d]\n", spi_sclk);
	printf("Pre-boot fixup scpu clock selection      [%d]\n", scpu_clk);
	printf("Pre-boot fixup scpu dpll clock           [%d]\n", scpu_dpll_clk);
	printf("Pre-boot fixup mcpu clock selection      [%d]\n", mcpu_clk);
	printf("Pre-boot fixup mcpu dpll clock           [%d]\n", mcpu_dpll_clk);

	magic = *(uint32_t *)(ptr + 0x10);
	if (magic == 0x5a5aa5a5) {
		if (is_nand) {
			*(uint32_t *)(ptr + 0x10 + 0x4) = 0xeaaeeaae;
			*(uint32_t *)(ptr + 0x10 + 0x8) = nand_pagesize;
			*(uint32_t *)(ptr + 0x10 + 0xc) = nand_erasesize;
		}
		*(uint32_t *)(ptr + 0x20 + 0x0) = size;
		*(uint32_t *)(ptr + 0x20 + 0x4) = to;
		*(uint32_t *)(ptr + 0x20 + 0x8) = from;
		*(uint32_t *)(ptr + 0x20 + 0xc) = entry;
		*(unsigned char *)(ptr + 0x30 + 0x0) = hcprogrammer_usb0_en;
		*(unsigned char *)(ptr + 0x30 + 0x1) = hcprogrammer_usb1_en;
		*(unsigned int *)(ptr + 0x30 + 0x4) = hcprogrammer_irq_timeout;
		*(unsigned int *)(ptr + 0x30 + 0x8) = hcprogrammer_sync_timeout;
		*(unsigned int *)(ptr + 0x40 + 0x0) = spi_wire;
		*(unsigned int *)(ptr + 0x40 + 0x4) = spi_sclk;
		*(unsigned short *)(ptr + 0x40 + 0x8) = scpu_clk; /* scpu clock selection */
		*(unsigned short *)(ptr + 0x40 + 0xa) = scpu_dpll_clk; /* scpu dpll clock (MHz) */
		*(unsigned short *)(ptr + 0x40 + 0xc) = mcpu_clk; /* mcpu clock selection */
		*(unsigned short *)(ptr + 0x40 + 0xe) = mcpu_dpll_clk; /* mcpu dpll clock (MHz) */
	} else {
		chip = *(uint32_t *)(ptr + 0x14);
		if (chip == 0x1512) {
			*(uint8_t *)(ptr + 0x814) = (size & 0x00ff0000) >> 16;
			*(uint8_t *)(ptr + 0x819) = (size & 0x0000ff00) >> 8;
			*(uint8_t *)(ptr + 0x818) = (size & 0x000000ff);

			*(uint8_t *)(ptr + 0x810) = (entry & 0x00ff0000) >> 16;
			*(uint8_t *)(ptr + 0x811) = (entry & 0xff000000) >> 24;
			*(uint8_t *)(ptr + 0x84c) = (entry & 0x00ff0000) >> 16;
			*(uint8_t *)(ptr + 0x84d) = (entry & 0xff000000) >> 24;
		} else if (chip == 0x1600) {
			*(uint8_t *)(ptr + 0x814) = (size & 0x00ff0000) >> 16;
			*(uint8_t *)(ptr + 0x819) = (size & 0x0000ff00) >> 8;
			*(uint8_t *)(ptr + 0x818) = (size & 0x000000ff);

			*(uint8_t *)(ptr + 0x810) = (entry & 0x00ff0000) >> 16;
			*(uint8_t *)(ptr + 0x811) = (entry & 0xff000000) >> 24;
			*(uint8_t *)(ptr + 0x83c) = (entry & 0x00ff0000) >> 16;
			*(uint8_t *)(ptr + 0x83d) = (entry & 0xff000000) >> 24;
		}
	}

	fseek(fpout, 0, SEEK_SET);
	if (is_nand)
		fwrite(buf, 1, (unsigned int)sb.st_size + nand_pagesize, fpout);
	else
		fwrite(buf, 1, (unsigned int)sb.st_size, fpout);

	if (!output) {
		fclose(fpout);
	} else {
		fclose(fpin);
		fclose(fpout);
	}

	free(buf);
	return 0;
}

