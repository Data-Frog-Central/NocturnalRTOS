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

static void loading_flush(unsigned short *loading_fb, unsigned int width, unsigned int height, unsigned short text_color, unsigned short background_color) {
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

static void draw_char_direct(uint8_t *fb, int x_pos, int y_pos, char c, uint32_t text_color, int pitch, bool rgb32) {
    unsigned bpp = rgb32 ? 4 : 2;
    int runtime_width = pitch / bpp;

    if (x_pos < 0 || y_pos < 0 || (x_pos + FONT_WIDTH) > runtime_width) return;

    uint8_t b0 = text_color & 0xFF;
    uint8_t b1 = (text_color >> 8) & 0xFF;
    uint8_t b2 = (text_color >> 16) & 0xFF;
    uint8_t b3 = (text_color >> 24) & 0xFF;

    for (int j = 0; j < FONT_HEIGHT; j++) {
        for (int i = 0; i < FONT_WIDTH; i++) {
            unsigned char rem = 1 << ((i + j * FONT_WIDTH) & 7);
            unsigned offset = (i + j * FONT_WIDTH) >> 3;

            if ((lcd_font[FONT_OFFSET((unsigned char)c) + offset] & rem) > 0) {                
                int target_x = x_pos + i;
                int target_y = y_pos + j;

                int byte_index = (target_y * pitch) + (target_x * bpp);

                if (rgb32) {
                    fb[byte_index]     = b0; // Blue
                    fb[byte_index + 1] = b1; // Green
                    fb[byte_index + 2] = b2; // Red
                    fb[byte_index + 3] = b3; // Alpha
                } else {
                    fb[byte_index]     = b0;
                    fb[byte_index + 1] = b1;    
                }
            }
        }
    }
}

// TODO: I should update this to be a full overlay fb that gets blit on top of data instead of modifying data
void update_fps_counter(const void *data, unsigned width, unsigned height, size_t pitch, bool rgb32) {
    if (!data) return;

    uint8_t *fb_mutable = (uint8_t *)data;
    int str_len = strlen(current_fps_str);
    if (str_len == 0) return;
    
    int start_x = width - (str_len * FONT_WIDTH_STRIDE) - 4;
    int start_y = 4;

    unsigned bar_padding_x = 4;
    unsigned bar_padding_y = 4;
    unsigned bar_x = start_x - bar_padding_x;
    unsigned bar_y = start_y - bar_padding_y;
    unsigned bar_w = (str_len * FONT_WIDTH_STRIDE) + (bar_padding_x * 2);
    unsigned bar_h = FONT_HEIGHT + (bar_padding_y * 2);

    uint32_t txt_color = rgb32 ? RGB32(255, 255, 255) : RGB565(255, 255, 255); // White
    uint32_t bg_color = rgb32 ? RGB32(0, 0, 0) : RGB565(0, 0, 0); // Black

    FrameInfo fill_frame = { data, width, height, bar_w, bar_h, pitch, bar_x, bar_y, rgb32 };
    hcge_fb_fill_rect(fill_frame, bg_color);

    for (int i = 0; i < str_len; i++) {
        draw_char_direct(
            fb_mutable, 
            start_x + (i * FONT_WIDTH_STRIDE), 
            start_y, 
            current_fps_str[i], 
            txt_color, 
            pitch,
            rgb32
        );
    }
}

void show_loading_screen(bool block_loading, bool loading_dots, unsigned short text_color, unsigned short background_color, const char *fmt, ...) {
    rows = SCREEN_HEIGHT / FONT_HEIGHT;
    cols = SCREEN_WIDTH / FONT_WIDTH_STRIDE;
    fb_y = fb_x = 0;
    text_fb = pvPortMalloc(rows * cols);
    unsigned short *loading_fb = pvPortMalloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(unsigned short));

    memset(text_fb, ' ', rows * cols);

	loading_printf("\n Phobos - NocturnalRTOS\n\n");

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
		    loading_flush(loading_fb, SCREEN_WIDTH, SCREEN_HEIGHT, text_color, background_color);
            blit_to_screen(loading_fb, SCREEN_WIDTH, SCREEN_HEIGHT, pitch, false, SCALE_STRETCH);
            vTaskDelay(75);
	    }
    } else {
        loading_flush(loading_fb, SCREEN_WIDTH, SCREEN_HEIGHT, text_color, background_color);
        blit_to_screen(loading_fb, SCREEN_WIDTH, SCREEN_HEIGHT, pitch, false, SCALE_STRETCH);
        vTaskDelay(75);
    }

    vPortFree(text_fb);
    text_fb = NULL;
    vPortFree(loading_fb);
    loading_fb = NULL;

    if(block_loading) vTaskSuspend(NULL);
}
