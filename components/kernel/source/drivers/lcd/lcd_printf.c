#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <kernel/ld.h>
#include <kernel/drivers/lcd_printf.h>

#include "lcd_font.h"

static char *lcd_buf = NULL;
static unsigned lcd_y, lcd_x;

#define COLS 	(320/FONT_WIDTH_STRIDE)
#define ROWS	(240/FONT_HEIGHT)

void lcd_init(void) {
	if (!lcd_buf)
		lcd_buf = (char*)malloc(COLS * ROWS * sizeof(char));

	if (lcd_buf)
		memset(lcd_buf, ' ', COLS * ROWS * sizeof(char));

	lcd_y = lcd_x = 0;
}

int lcd_vprintf(const char* fmt, va_list ap) {
	int ret;
	char buf[999];

	ret = vsnprintf(buf, sizeof buf, fmt, ap);

	for (char *pc = buf; *pc; pc++) {
		if (lcd_x >= COLS) {
			lcd_x = 0;
			lcd_y++;
		}
		if (lcd_y >= ROWS)
			continue;
		if (*pc == '\n') {
			lcd_x = 0;
			lcd_y++;
		}
		else if (*pc == '\r') {
    		lcd_x = 0;
		}
		else if (*pc >= ' ') {
			lcd_buf[lcd_y*COLS + lcd_x] = *pc;
			lcd_x++;
		}
	}
	return ret;
}

int lcd_printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = lcd_vprintf(fmt, ap);
	va_end(ap);
    return ret;
}

static void lcd_pinmux_gpio(void) {
	*((volatile unsigned *)&PINMUXL + 0) &= 0xffff; // L02-03 (D0-1)
	*((volatile unsigned *)&PINMUXL + 1) = 0; // L04-07 (D2-4, WR)
	*((volatile unsigned *)&PINMUXL + 2) &= 0xff00ffff; // L10 (CS)

	*((volatile unsigned *)&PINMUXT + 0) &= 0xff; // T01-03 (RS, D11-12)
	*((volatile unsigned *)&PINMUXT + 1) &= 0xff000000; // T04-06 (D13-15)
	*((volatile unsigned *)&PINMUXT + 2) &= 0xff; // T09-11 (D5-D7)
	*((volatile unsigned *)&PINMUXT + 3) &= 0xff000000; // T12-14 (D8-10)
}

static void lcd_send(unsigned short data) {
	*((volatile unsigned *)&GPIOLCTRL + 4) = // clear L10 (CS), L07 (WR); L02-06 <- D0-4
		*((volatile unsigned *)&GPIOLCTRL + 4) & 0xfffffb03 | data << 2 & 0x7c;
	*((volatile unsigned *)&GPIOTCTRL + 4) = // T09-14 <- D5-10, D11-15 -> T02-06, tDST
		*((volatile unsigned *)&GPIOTCTRL + 4) & 0xffff8183 | data << 4 & 0x7e00 | data >> 9 & 0x7c;
	*((volatile unsigned *)&GPIOLCTRL + 4) |= 1 << 7; // set L07 (WR), tCSH
	*((volatile unsigned *)&GPIOLCTRL + 4) |= 1 << 10; // set L10 (CS)
}

static void lcd_send_cmd(unsigned char cmd) {
	*((volatile unsigned *)&GPIOTCTRL + 4) &= ~(1 << 1); // clear T01 (RS)
	lcd_send(cmd);
}

static void lcd_send_data(unsigned short data) {
	*((volatile unsigned *)&GPIOTCTRL + 4) |= 1 << 1; // set T01 (RS)
	lcd_send(data);
}

void lcd_flush(unsigned short text_color, unsigned short background_color) {
	lcd_pinmux_gpio();

	lcd_send_cmd(0x2a); // CASET
	lcd_send_data(0);
	lcd_send_data(0);
	lcd_send_data((320 - 1) >> 8);
	lcd_send_data((320 - 1) & 255);

	lcd_send_cmd(0x2b); // RASET
	lcd_send_data(0);
	lcd_send_data(0);
	lcd_send_data((240 - 1) >> 8);
	lcd_send_data((240 - 1) & 255);

	lcd_send_cmd(0x2c); // RAMWR
	for (unsigned y = 0; y < 240; y++) {
		for (unsigned x = 0; x < 320; x++) {
			unsigned row = y / FONT_HEIGHT;
			unsigned col = x / FONT_WIDTH_STRIDE;

			unsigned char symbol_index = ' '; // default to blank

			if (row < ROWS && col < COLS)
				symbol_index = lcd_buf[row * COLS + col];

			//unsigned symbol_index = lcd_buf[(y / FONT_HEIGHT) * COLS + (x / FONT_WIDTH_STRIDE)];
			unsigned i = (x % FONT_WIDTH_STRIDE), j = y % FONT_HEIGHT;
			unsigned char rem = 1 << ((i + j * FONT_WIDTH) & 7);
			unsigned offset = (i + j * FONT_WIDTH) >> 3;

			if (i < FONT_WIDTH && (lcd_font[FONT_OFFSET(symbol_index) + offset] & rem) > 0)
				lcd_send_data(text_color);
			else
				lcd_send_data(background_color);
		}
	}
}

void dbg_show_noblock(unsigned short text_color, unsigned short background_color, const char *fmt, ...) {
    taskENTER_CRITICAL();
	lcd_init();
	lcd_printf("\n DartOS - HC-RTOS\n\n");

	va_list ap;
	va_start(ap, fmt);
	lcd_vprintf(fmt, ap);
	va_end(ap);

	lcd_printf("                                         ]\r");
	lcd_printf("          [");

	for (int i=0; i<30; i++) {
		lcd_printf(".");
		lcd_flush(text_color, background_color);
	}

    taskEXIT_CRITICAL();
}

void lcd_bsod(const char *fmt, ...) {
    taskENTER_CRITICAL();
	lcd_init();
	lcd_printf("\n DartOS - HC-RTOS\n\n");

	va_list ap;
	va_start(ap, fmt);
	lcd_vprintf(fmt, ap);
	va_end(ap);

	lcd_flush(BSOD_TXT, BSOD_BG); // White text, Blue background 
	do {
	} while (1);
}