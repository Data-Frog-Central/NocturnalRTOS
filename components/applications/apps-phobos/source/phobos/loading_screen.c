// Based on lcd_printf stuff but writes to a framebuffer instead
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <kernel/drivers/lcd_font.h>
#include "../drivers/sf2000_gfx.h"

#include "fb_drawing.h"
#include "loading_screen.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

QueueHandle_t xLoadingQueue = NULL;
SemaphoreHandle_t xLoadingInitSemaphore = NULL;
static bool loading_done = false;

void loading_flush(loading_context_t *ctx, loading_msg_t msg) {
    for (unsigned y = 0; y < ctx->height; y++) {
        for (unsigned x = 0; x < ctx->width; x++) {
            
            unsigned row = y / FONT_HEIGHT;
            unsigned col = x / FONT_WIDTH_STRIDE;

            unsigned char symbol_index = ' ';

            if (row < ctx->rows && col < ctx->cols) {
                symbol_index = ctx->text_fb[row * ctx->cols + col];
            }

            unsigned i = (x % FONT_WIDTH_STRIDE);
            unsigned j = y % FONT_HEIGHT;
            unsigned char rem = 1 << ((i + j * FONT_WIDTH) & 7);
            unsigned offset = (i + j * FONT_WIDTH) >> 3;

            unsigned int fb_index = y * ctx->width + x;

            if (i < FONT_WIDTH && (lcd_font[FONT_OFFSET(symbol_index) + offset] & rem) > 0) {
                ctx->loading_fb[fb_index] = msg.text_color;
            } else {
                ctx->loading_fb[fb_index] = msg.background_color;
            }
        }
    }
}

int loading_vprintf(loading_context_t *ctx, const char* fmt, va_list ap) {
	int ret;
	static char buf[512];

	ret = vsnprintf(buf, sizeof buf, fmt, ap);

	for (char *pc = buf; *pc; pc++) {
		if (ctx->fb_x >= ctx->cols) {
			ctx->fb_x = 1;
			ctx->fb_y += 2;
		}

		if (ctx->fb_y >= ctx->rows) continue;

		if (*pc == '\n') {
			ctx->fb_x = 1;
			ctx->fb_y += 2;
		} else if (*pc == '\r') {
    		ctx->fb_x = 1;
		} else if (*pc >= ' ') {
			ctx->text_fb[ctx->fb_y*ctx->cols + ctx->fb_x] = *pc;
			ctx->fb_x++;
		}
	}
	return ret;
}

int loading_printf(loading_context_t *ctx, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = loading_vprintf(ctx, fmt, ap);
	va_end(ap);
    return ret;
}

static void render_loading_anim(loading_context_t *ctx, loading_msg_t msg, unsigned int pitch, int anim_frame) {
    int step = anim_frame % 30;
    int radius = 6 + (step < 15 ? step : (30 - step));

	loading_flush(ctx, msg);
    draw_filled_circle(ctx->loading_fb, ctx->width, ctx->height, 160, 170, radius, msg.text_color);
    blit_to_screen(ctx->loading_fb, ctx->width, ctx->height, pitch, false, SCALE_STRETCH);
    vTaskDelay(75);
}

void render_loading_frame(loading_context_t *ctx, loading_msg_t msg) {
    memset(ctx->text_fb, ' ', ctx->rows * ctx->cols);
    unsigned int pitch = SCREEN_WIDTH * sizeof(unsigned short);
    ctx->fb_x = 1;
    ctx->fb_y = 1;

    int anim_frame = 0;

    loading_printf(ctx, "Phobos - NocturnalRTOS\n");
    if (msg.message[0] != '\0') loading_printf(ctx, "%s", msg.message);

    if (msg.progress_bar) {
	    for (int i=0; i<30; i++) {
            render_loading_anim(ctx, msg, pitch, anim_frame);
            anim_frame++;
	    }
        if (!loading_done) while (1) {
            if (loading_done) break;
            render_loading_anim(ctx, msg, pitch, anim_frame);
            anim_frame++;
        }
    } else {
        loading_flush(ctx, msg);
        blit_to_screen(ctx->loading_fb, ctx->width, ctx->height, pitch, false, SCALE_STRETCH);
        vTaskDelay(75);
    }
}

void finish_loading(void) {
    loading_done = true;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

void show_loading_screen(bool block_loading, bool show_anim, unsigned short text_color, unsigned short background_color, const char *fmt, ...) {
    loading_msg_t msg;
    msg.progress_bar = show_anim;
    msg.text_color = text_color;
    msg.background_color = background_color;
    msg.notify_task = xTaskGetCurrentTaskHandle();
    loading_done = false;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg.message, sizeof(msg.message), fmt, ap);
    va_end(ap);

    xQueueSend(xLoadingQueue, &msg, portMAX_DELAY);

    if(block_loading) {
        finish_loading();
        vTaskSuspend(NULL);
    }
}

void loading_screen_task(void *pvParameters) {
    loading_context_t ctx;

    ctx.width = SCREEN_WIDTH;
    ctx.height = SCREEN_HEIGHT;
    ctx.rows = SCREEN_HEIGHT / FONT_HEIGHT;
    ctx.cols = SCREEN_WIDTH / FONT_WIDTH_STRIDE;
    ctx.fb_y = 1;
    ctx.fb_x = 1;

    ctx.text_fb = pvPortMalloc(ctx.rows * ctx.cols);
    ctx.loading_fb = pvPortMalloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(unsigned short));

    xSemaphoreGive(xLoadingInitSemaphore);
    loading_msg_t msg;

    while (1) { // Wait for a loading update message from any worker thread
        if (xQueueReceive(xLoadingQueue, &msg, portMAX_DELAY) == pdPASS) {
            render_loading_frame(&ctx, msg);
            if (msg.notify_task != NULL) xTaskNotifyGive(msg.notify_task);
        }
    }
}
