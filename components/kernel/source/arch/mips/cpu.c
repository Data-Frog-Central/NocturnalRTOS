#include <string.h>
#include <kernel/io.h>
#include <kernel/irqflags.h>
#include <kernel/lib/console.h>
#include <kernel/ld.h>

int reset(void)
{
	void *wdt_addr = (void *)&WDT0;

	arch_local_irq_disable();
	REG32_WRITE(wdt_addr, 0xfffffffa);
	REG32_WRITE(wdt_addr + 4, 0x26);
	asm volatile(".word 0x1000ffff; nop; nop;"); /* Wait for reboot */
}

static int do_reset(int argc, char **argv)
{
	if ((argc == 2) && !strcmp(argv[1], "-u")) {
		REG16_WRITE(0xb8818a00, 0x5991);
	}

	reset();
	return 0;
}

CONSOLE_CMD(reset, NULL, do_reset, CONSOLE_CMD_MODE_SELF, "reset system")
