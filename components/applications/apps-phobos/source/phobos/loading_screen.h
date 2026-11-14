#ifndef LOADING_SCREEN__
#define LOADING_SCREEN__

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

typedef struct {
    unsigned short *loading_fb;
    char *text_fb;
    unsigned int width;
    unsigned int height;
    unsigned int rows;
    unsigned int cols;
    unsigned int fb_y;
    unsigned int fb_x;
} loading_context_t;

typedef struct {
    char message[512];
    bool progress_bar;
    unsigned short text_color;
    unsigned short background_color;
    TaskHandle_t notify_task;
} loading_msg_t;

extern QueueHandle_t xLoadingQueue;
extern SemaphoreHandle_t xLoadingInitSemaphore;

void loading_flush(loading_context_t *ctx, loading_msg_t msg);
int loading_vprintf(loading_context_t *ctx, const char* fmt, va_list ap);
int loading_printf(loading_context_t *ctx, const char *fmt, ...);
void render_loading_frame(loading_context_t *ctx, loading_msg_t msg);

void loading_screen_task(void *pvParameters);
void show_loading_screen(bool block_loading, bool show_anim, unsigned short text_color, unsigned short background_color, const char *fmt, ...);
void finish_loading(void);

#endif // LOADING_SCREEN__
