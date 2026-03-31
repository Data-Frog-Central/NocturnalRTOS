#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <kernel/io.h>
#include <kernel/ld.h>
#include <kernel/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <kernel/lib/fdt_api.h>
#include <image.h>
#include <mtdload.h>

static void spi_nor_read(void *buf, unsigned int offset, unsigned int size)
{
#define SF_BASE 0xb882e000
#define DMA_ADDR_ALIGN (32)
	int use_bounce_buf = 0;
	u32 val;
	void *tmp = buf;
	offset += 0xafc00000;

	if (!IS_ALIGNED((u32)buf, DMA_ADDR_ALIGN)) {
		tmp = memalign(32, size + 32);
		use_bounce_buf = 1;
	}

	cache_flush_invalidate(tmp, size);

	val = REG32_READ(SF_BASE + 0x98);
	val |= BIT21 | BIT14 | BIT31 | BIT30;
	REG32_WRITE(SF_BASE + 0x98, val);

	REG32_WRITE_SYNC(SF_BASE + 0x58, (unsigned int)tmp & 0x0fffffff);
	REG32_WRITE_SYNC(SF_BASE + 0x5c, (unsigned int)offset & (~0xafc00000));
	REG32_WRITE_SYNC(SF_BASE + 0x60, size);
	REG32_WRITE(SF_BASE + 0x64, 0x001e0020);

	REG8_WRITE_SYNC(SF_BASE + 0x99, 0x4d);

	REG32_SET_BIT(SF_BASE + 0xa0, BIT0);
	REG32_SET_BIT(SF_BASE + 0x64, BIT5);

	while (!REG32_GET_BIT(SF_BASE + 0xa0, BIT0));

	REG32_SET_BIT(SF_BASE + 0xa0, BIT0);

	val = REG32_READ(SF_BASE + 0x98);
	val &= ~(BIT21 | BIT14);
	REG32_WRITE(SF_BASE + 0x98, val);

	REG8_WRITE_SYNC(SF_BASE + 0x99, 0x0d);

	REG32_WRITE_SYNC(SF_BASE + 0x58, 0x00);
	REG32_WRITE_SYNC(SF_BASE + 0x5c, 0x00);
	REG32_WRITE_SYNC(SF_BASE + 0x60, 0x00);
	REG32_WRITE_SYNC(SF_BASE + 0x64, 0x80000000);

	REG32_WRITE(0xafc00000, 0x00);

	cache_invalidate(tmp, size);

	if (use_bounce_buf) {
		memcpy(buf, tmp, size);
		free(tmp);
	}
}

int mtdloadraw(unsigned char dev_type, void *dtb, u32 dtb_size, u32 start, u32 size)
{
	u32 length = (dtb_size < size) ? dtb_size : size;

	if (dev_type == IH_DEVT_SPINOR) {
		spi_nor_read(dtb, start, length);
	} else if (dev_type == IH_DEVT_SPINAND) {
		return -1;
	}

	return -1;
}

int mtdloaduImage(unsigned char dev_type, u32 start, u32 size)
{
	image_header_t hdr = { 0 };
	ssize_t length = 0;
	unsigned int bootsize;

	if (dev_type == IH_DEVT_SPINOR) {
		length = image_get_header_size();
		spi_nor_read(&hdr, start, length);

		length = image_get_image_size(&hdr);
		if ((void *)image_load_addr == NULL)
			image_load_addr = (unsigned long)malloc(length);
		else
			image_load_addr = (unsigned long)realloc((void *)image_load_addr, length);
		if (!image_load_addr) {
			printf("ERROR: malloc for image with size %ld\n", length);
			return -1;
		}
		printf("default image load address = 0x%08lx\n", image_load_addr);
		spi_nor_read((void *)image_load_addr, start, length);
		return 0;
	} else if (dev_type == IH_DEVT_SPINAND) {
		return -1;
	}

	return -1;
}
