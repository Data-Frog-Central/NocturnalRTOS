// Based on lcd_printf stuff but writes to a framebuffer instead
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <kernel/drivers/lcd_font.h>
#include "../drivers/sf2000_gfx.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

static unsigned fb_y, fb_x;
static unsigned cols, rows;
static char *text_fb = NULL;
static unsigned short *loading_fb = NULL;

static void loading_flush(unsigned int width, unsigned int height, unsigned short text_color, unsigned short background_color) {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            
            unsigned row = y / FONT_HEIGHT;
            unsigned col = x / FONT_WIDTH_STRIDE;

            unsigned char symbol_index = ' ';

            if (row < rows && col < cols) {
                symbol_index = text_fb[row * cols + col];
            }

            unsigned i = (x % FONT_WIDTH_STRIDE);
            unsigned j = y % FONT_HEIGHT;
            unsigned char rem = 1 << ((i + j * FONT_WIDTH) & 7);
            unsigned offset = (i + j * FONT_WIDTH) >> 3;

            unsigned int fb_index = y * width + x;

            if (i < FONT_WIDTH && (lcd_font[FONT_OFFSET(symbol_index) + offset] & rem) > 0) {
                loading_fb[fb_index] = text_color;
            } else {
                loading_fb[fb_index] = background_color;
            }
        }
    }
}

static int loading_vprintf(const char* fmt, va_list ap) {
	int ret;
	char buf[999];

	ret = vsnprintf(buf, sizeof buf, fmt, ap);

	for (char *pc = buf; *pc; pc++) {
		if (fb_x >= cols) {
			fb_x = 0;
			fb_y++;
		}
		if (fb_y >= rows)
			continue;
		if (*pc == '\n') {
			fb_x = 0;
			fb_y++;
		}
		else if (*pc == '\r') {
    		fb_x = 0;
		}
		else if (*pc >= ' ') {
			text_fb[fb_y*cols + fb_x] = *pc;
			fb_x++;
		}
	}
	return ret;
}

static int loading_printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = loading_vprintf(fmt, ap);
	va_end(ap);
    return ret;
}

void show_loading_screen(bool block_loading, bool loading_dots, unsigned short text_color, unsigned short background_color, const char *fmt, ...) {
    rows = SCREEN_HEIGHT / FONT_HEIGHT;
    cols = SCREEN_WIDTH / FONT_WIDTH_STRIDE;
    fb_y = fb_x = 0;
    text_fb = malloc(rows * cols);
    loading_fb = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(unsigned short));

    memset(text_fb, ' ', rows * cols);

	loading_printf("\n DartOS - HC-RTOS\n\n");

	va_list ap;
	va_start(ap, fmt);
	loading_vprintf(fmt, ap);
	va_end(ap);

    unsigned int pitch = SCREEN_WIDTH * sizeof(unsigned short);

    if (loading_dots) {
        // This basically pauses loading but can be useful for causing a loading screen to stay for a certain amount of time
        loading_printf("                                         ]\r");
	    loading_printf("          [");

	    for (int i=0; i<30; i++) {
		    loading_printf(".");
		    loading_flush(SCREEN_WIDTH, SCREEN_HEIGHT, text_color, background_color);
            frontend_video_cb(loading_fb, SCREEN_WIDTH, SCREEN_HEIGHT, pitch);
            vTaskDelay(75);
	    }
    } else {
        loading_flush(SCREEN_WIDTH, SCREEN_HEIGHT, text_color, background_color);
        frontend_video_cb(loading_fb, SCREEN_WIDTH, SCREEN_HEIGHT, pitch);
    }

    free(text_fb);
    text_fb = NULL;
    free(loading_fb);
    loading_fb = NULL;

    if(block_loading) {
        // Stops anything else from happening for safe shutdown
        // Basically 100% the cpu so maybe not the best idea...
        taskENTER_CRITICAL();
        do {
	    } while (1);
    }

}