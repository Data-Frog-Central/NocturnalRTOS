/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - Tobias Jakobi
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/mman.h>

#include <kernel/io.h>
#include <kernel/fb.h>
#include <hcuapi/fb.h>
#include <hcge/ge_api.h>
#include <cpu_func.h>

#include <libretro.h>

#include "sf2000_gfx.h"
#include "../menu/menu.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// TODO: move the statics to sf2000_gfx_data_t 
static hcge_context *ctx = NULL;
static int fbdev;
static struct fb_fix_screeninfo fix;    /* Current fix */
static struct fb_var_screeninfo var;    /* Current var */
static uint32_t screen_size;
static uint8_t *fb_base;
static uint32_t line_width;
static uint32_t pixel_size;
//static uint8_t *screen_buffer[2];
static int buffer_num  = 4;

extern void frontend_log_cb(enum retro_log_level level, const char *tag, const char *fmt, ...);
extern bool enable_xrgb8888_support;

bool preserve_aspect_ratio = false;
bool use_integer_scaling = false;
bool gfx_custom_x_enabled = false;
bool gfx_custom_y_enabled = false;
int gfx_custom_x = 0;
int gfx_custom_y = 0;

typedef struct sf2000_gfx_data
{
	bool vsync;
	bool rgb32;
} sf2000_gfx_data_t;


static int init_fb_device(void)
{
    int ret;
    if(hcge_open(&ctx) != 0) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"Init hcge error.\n");
        return -1;
    }
    fbdev = open("/dev/fb0", O_RDWR);

    ioctl(fbdev, FBIOGET_FSCREENINFO, &fix);
    ioctl(fbdev, FBIOGET_VSCREENINFO, &var);

    line_width  = var.xres * var.bits_per_pixel / 8;
    pixel_size = var.bits_per_pixel / 8;
    screen_size = var.xres * var.yres * var.bits_per_pixel / 8;

    buffer_num = fix.smem_len / screen_size;

    // Make sure that the display is on.
    if (ioctl(fbdev, FBIOBLANK, FB_BLANK_UNBLANK) != 0) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"FB_BLANK_UNBLANK failed\n");
    }

    frontend_log_cb(RETRO_LOG_DEBUG, "DISPLAY_DRIVER" ,"buffer_num=%d\n", buffer_num);
    frontend_log_cb(RETRO_LOG_DEBUG, "DISPLAY_DRIVER" ,"xres=%d, yres=%d, xres_virtual=%d, yres_virtual=%d\n", (int)var.xres, (int)var.yres, (int)var.xres_virtual, (int)var.yres_virtual);
    frontend_log_cb(RETRO_LOG_DEBUG, "DISPLAY_DRIVER" ,"bits_per_pixel=%d, red.length=%d, green.length=%d, blue.length=%d, transp.length=%d\n", (int)var.bits_per_pixel, (int)var.red.length, (int)var.green.length, (int)var.blue.length, (int)var.transp.length);

    /*var.activate = FB_ACTIVATE_VBL;*/
    //var.activate = FB_ACTIVATE_NOW;
    var.yoffset = 0;
    var.xoffset = 0;
	var.xres_virtual = var.xres;
	var.yres_virtual = var.yres;

	// this will sets the framebuffer internal format (and thus fb_base too) to HCFB_FMT_RGB565
	var.bits_per_pixel = 16;
	var.red.length = 5;
	var.green.length = 6;
	var.blue.length = 5;
    //var.transp.length = 8;
    //var.yres_virtual = buffer_num * var.yres;

    //set variable information
    if(ioctl(fbdev, FBIOPUT_VSCREENINFO, &var) == -1) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"FBIOPUT_VSCREENINFO failed\n");
        return -1;
    }

    fb_base = (unsigned char *)mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev, 0);
    if (fb_base == MAP_FAILED) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"mmap failed\n");
        return -1;
    }
	memset(fb_base, 0x00, fix.smem_len);

    ret = ioctl(fbdev, FBIOPAN_DISPLAY, &var);
    if(ret < 0)
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"FBIOPAN_DISPLAY failed. ret=%d\n", ret);

    ret = ioctl(fbdev, FBIO_WAITFORVSYNC, &ret);
    if (ret < 0)
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"FBIO_WAITFORVSYNC failed. ret=%d\n", ret);

    return 0;
}

static void deinit_fb_device(void)
{
    if(fbdev > 0) {
        if(fb_base) {
            munmap(fb_base, screen_size);
            fb_base = NULL;
        }
        close(fbdev);
        fbdev = -1;
    }
}

static void blit(const void *frame, unsigned width, unsigned height, unsigned pitch)
{
	if (!frame)
		return;

	// TODO: do i need a special case for when frame is already full screen and use possibly a bit faster 
	// regular hcge_blit, or just let hcge_stretch_blit handle it internaly?

	// TODO: implement other stretching options to preserve the original image ratio
	
	hcge_state *state = &ctx->state;
	HCGERectangle srect = {0, 0, width, height};
    HCGERectangle drect = {0};

    bool use_integer_scaling = false;

    if (preserve_aspect_ratio) {
        int src_w = width;
        int src_h = height;
        int screen_w = var.xres;
        int screen_h = var.yres;

        if (use_integer_scaling) { // ---------- INTEGER SCALING ----------
            int scale_x = screen_w / src_w;
            int scale_y = screen_h / src_h;
            int scale = (scale_x < scale_y) ? scale_x : scale_y;

            if (scale < 1) scale = 1; // no downscaling below 1x

            int dst_w = src_w * scale;
            int dst_h = src_h * scale;

            // Center the image
            drect.w = dst_w;
            drect.h = dst_h;
            if (!gfx_custom_x_enabled) drect.x = (screen_w - dst_w) / 2;
            else drect.x = gfx_custom_x;
            if (!gfx_custom_y_enabled) drect.y = (screen_h - dst_h) / 2;
            else drect.y = gfx_custom_y;
        } else { // ---------- FLOAT SCALING ----------
            float src_w_f = (float)width;
            float src_h_f = (float)height;
            float screen_w_f = (float)var.xres;
            float screen_h_f = (float)var.yres;

            float src_aspect = src_w_f / src_h_f;
            float screen_aspect = screen_w_f / screen_h_f;

            float dst_w_f, dst_h_f;
            if (screen_aspect > src_aspect) {
                dst_h_f = screen_h_f;
                dst_w_f = src_aspect * dst_h_f;
            } else {
                dst_w_f = screen_w_f;
                dst_h_f = dst_w_f / src_aspect;
            }

            // Center the image
            drect.w = (int)(dst_w_f + 0.5f);
            drect.h = (int)(dst_h_f + 0.5f);
            if (!gfx_custom_x_enabled) drect.x = (int)((screen_w_f - dst_w_f) / 2.0f + 0.5f);
            else drect.x = gfx_custom_x;
            if (!gfx_custom_y_enabled) drect.y = (int)((screen_h_f - dst_h_f) / 2.0f + 0.5f);
            else drect.y = gfx_custom_y;
        }
    } else { // Fullscreen, ignore aspect ratio
        drect.x = 0;
        drect.y = 0;
        drect.w = var.xres;
        drect.h = var.yres;
    }

    state->render_options = HCGE_DSRO_NONE;
    state->drawingflags = HCGE_DSDRAW_NOFX;
    state->blittingflags = HCGE_DSBLIT_NOFX;

    state->src_blend = HCGE_DSBF_SRCALPHA;
    state->dst_blend = HCGE_DSBF_ZERO;

    state->destination.config.size.w = var.xres;
    state->destination.config.size.h = var.yres;
    state->destination.config.format = HCGE_DSPF_RGB16;
    state->dst.phys = (uint32_t)PHY_ADDR(fb_base);
    state->dst.pitch = line_width;

    state->source.config.size.w = width;
    state->source.config.size.h = height;
    state->source.config.format = enable_xrgb8888_support ? HCGE_DSPF_ARGB : HCGE_DSPF_RGB16;
    state->src.phys = (uint32_t)PHY_ADDR(frame);
    state->src.pitch = pitch;

	// NOTE: this fixes the artifacts on the left side of the screen
	cache_flush(frame, pitch * height);

    state->accel = HCGE_DFXL_STRETCHBLIT;
    hcge_set_state(ctx, &ctx->state, state->accel);
	if (!hcge_stretch_blit(ctx, &srect, &drect)) {
		// TODO:
		static int count = 0;
		if (count == 0)
            frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"hcge_stretch_blit failed\n");
		count = (count + 1) % 60;
	}
    hcge_engine_sync(ctx);
}

void init_fb(void) {
	sf2000_gfx_data_t *ctx = (sf2000_gfx_data_t*)calloc(1, sizeof(sf2000_gfx_data_t));
	if (!ctx)
		return;

	init_fb_device();
}

static void sf2000_gfx_free(void *data)
{
	sf2000_gfx_data_t* ctx = (sf2000_gfx_data_t*)data;
	if (!ctx)
		return;

	deinit_fb_device();

	free(ctx);
}

void draw_black_border(void) {
    size_t pitch = SCREEN_WIDTH * sizeof(uint16_t); 
    size_t buffer_size = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t);
    uint16_t *black_buffer = (uint16_t *)calloc(1, buffer_size);
    
    if (black_buffer != NULL) {
        blit(black_buffer, SCREEN_WIDTH, SCREEN_HEIGHT, pitch);
        free(black_buffer);
    }
}

void draw_border(const char *core_path) {
    char file_path[MAXPATH];
    snprintf(file_path, sizeof(file_path), "%s/HCRTOS/borders/%s/%s.bmp", SDCARD_DIRECTORY, core_path, core_path);
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER", "Failed to open BMP: %s\n", file_path);
        draw_black_border();
        return;
    }

    BMPFileHeader file_header;
    BMPInfoHeader info_header;

    fread(&file_header, sizeof(BMPFileHeader), 1, file);
    fread(&info_header, sizeof(BMPInfoHeader), 1, file);

    // Enforce uncompressed 24-bit format
    if (file_header.type != 0x4D42 || info_header.bits_per_pixel != 24 || info_header.compression != 0) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER", "BMP must be uncompressed 24-bit BGR.\n");
        fclose(file);
        draw_black_border();
        return;
    }

    int width = info_header.width;
    int height = abs(info_header.height); 
    bool flip_vertical = (info_header.height > 0); 

    size_t pitch = width * sizeof(uint16_t);
    uint16_t *rgb565_buffer = (uint16_t *)malloc(width * height * sizeof(uint16_t));
    if (!rgb565_buffer) {
        fclose(file);
        return;
    }

    fseek(file, file_header.offset, SEEK_SET);
    int row_padding = (4 - (width * 3) % 4) % 4;
    uint8_t bgr_pixel[3];

    for (int y = 0; y < height; y++) {
        int target_y = flip_vertical ? (height - 1 - y) : y;
        uint16_t *row_ptr = rgb565_buffer + (target_y * width);
        for (int x = 0; x < width; x++) {
            fread(bgr_pixel, 3, 1, file);

            uint16_t r = (bgr_pixel[2] & 0xF8) << 8;
            uint16_t g = (bgr_pixel[1] & 0xFC) << 3;
            uint16_t b = (bgr_pixel[0] & 0xF8) >> 3;

            row_ptr[x] = r | g | b;
        }
        fseek(file, row_padding, SEEK_CUR);
    }

    fclose(file);
    blit(rgb565_buffer, width, height, pitch);
    free(rgb565_buffer);
}

void frontend_video_cb(const void *data, unsigned width, unsigned height, size_t pitch) {
    blit(data, width, height, pitch);
}
