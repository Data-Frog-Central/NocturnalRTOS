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
#include <dartos.h>

#include "sf2000_gfx.h"
#include "../phobos/phobos.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

typedef struct sf2000_gfx_data {
    int fbdev;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    uint32_t screen_size;
    uint8_t *fb_base;
    uint32_t line_width;
    uint32_t pixel_size;
} sf2000_gfx_data_t;
static sf2000_gfx_data_t sf2000_ctx = {0};
static hcge_context *hcge_ctx = NULL;

extern void frontend_log_cb(enum retro_log_level level, const char *tag, const char *fmt, ...);
extern bool enable_xrgb8888_support;

ScalingMode global_scaling_mode = SCALE_STRETCH;
RotationMode global_rotation_mode;

bool show_fps_counter = false;
char current_fps_str[16] = "";

bool gfx_custom_x_enabled = false;
bool gfx_custom_y_enabled = false;
int gfx_custom_x = 0;
int gfx_custom_y = 0;

static int init_fb_device(void) {
    int ret;
    if(hcge_open(&hcge_ctx) != 0) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"Init hcge error.\n");
        return -1;
    }
    sf2000_ctx.fbdev = open("/dev/fb0", O_RDWR);

    ioctl(sf2000_ctx.fbdev, FBIOGET_FSCREENINFO, &sf2000_ctx.fix);
    ioctl(sf2000_ctx.fbdev, FBIOGET_VSCREENINFO, &sf2000_ctx.var);

    sf2000_ctx.line_width  = sf2000_ctx.var.xres * sf2000_ctx.var.bits_per_pixel / 8;
    sf2000_ctx.pixel_size = sf2000_ctx.var.bits_per_pixel / 8;
    sf2000_ctx.screen_size = sf2000_ctx.var.xres * sf2000_ctx.var.yres * sf2000_ctx.var.bits_per_pixel / 8;

    // Make sure that the display is on.
    if (ioctl(sf2000_ctx.fbdev, FBIOBLANK, FB_BLANK_UNBLANK) != 0) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"FB_BLANK_UNBLANK failed\n");
    }

    frontend_log_cb(RETRO_LOG_DEBUG, "DISPLAY_DRIVER" ,"xres=%d, yres=%d, xres_virtual=%d, yres_virtual=%d\n", (int)sf2000_ctx.var.xres, (int)sf2000_ctx.var.yres, (int)sf2000_ctx.var.xres_virtual, (int)sf2000_ctx.var.yres_virtual);
    frontend_log_cb(RETRO_LOG_DEBUG, "DISPLAY_DRIVER" ,"bits_per_pixel=%d, red.length=%d, green.length=%d, blue.length=%d, transp.length=%d\n", (int)sf2000_ctx.var.bits_per_pixel, (int)sf2000_ctx.var.red.length, (int)sf2000_ctx.var.green.length, (int)sf2000_ctx.var.blue.length, (int)sf2000_ctx.var.transp.length);

    sf2000_ctx.var.yoffset = 0;
    sf2000_ctx.var.xoffset = 0;
	sf2000_ctx.var.xres_virtual = sf2000_ctx.var.xres;
	sf2000_ctx.var.yres_virtual = sf2000_ctx.var.yres;

	// this will sets the framebuffer internal format (and thus fb_base too) to HCFB_FMT_RGB565
	sf2000_ctx.var.bits_per_pixel = 16;
	sf2000_ctx.var.red.length = 5;
	sf2000_ctx.var.green.length = 6;
	sf2000_ctx.var.blue.length = 5;

    //set variable information
    if(ioctl(sf2000_ctx.fbdev, FBIOPUT_VSCREENINFO, &sf2000_ctx.var) == -1) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"FBIOPUT_VSCREENINFO failed\n");
        return -1;
    }

    sf2000_ctx.fb_base = (unsigned char *)mmap(NULL, sf2000_ctx.fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, sf2000_ctx.fbdev, 0);
    if (sf2000_ctx.fb_base == MAP_FAILED) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"mmap failed\n");
        return -1;
    }
	memset(sf2000_ctx.fb_base, 0x00, sf2000_ctx.fix.smem_len);

    ret = ioctl(sf2000_ctx.fbdev, FBIOPAN_DISPLAY, &sf2000_ctx.var);
    if(ret < 0)
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"FBIOPAN_DISPLAY failed. ret=%d\n", ret);

    ret = ioctl(sf2000_ctx.fbdev, FBIO_WAITFORVSYNC, &ret);
    if (ret < 0)
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"FBIO_WAITFORVSYNC failed. ret=%d\n", ret);

    return 0;
}

static void deinit_fb_device(void) {
    if(sf2000_ctx.fbdev > 0) {
        if(sf2000_ctx.fb_base) {
            munmap(sf2000_ctx.fb_base, sf2000_ctx.screen_size);
            sf2000_ctx.fb_base = NULL;
        }
        close(sf2000_ctx.fbdev);
        sf2000_ctx.fbdev = -1;
    }
}

void hcge_accel_blit(FrameInfo src_info, FrameInfo dst_info, RotationMode rotation) {
	if (!src_info.frame || !dst_info.frame)return;
	
    hcge_state *state = &hcge_ctx->state;
    hcge_ctx->blit_direct = false;
	HCGERectangle srect = {src_info.x, src_info.y, src_info.width, src_info.height};

    switch (rotation) {
        case ROTATE_90:
            state->blittingflags = HCGE_DSBLIT_ROTATE90;
            break;

        case ROTATE_180:
            state->blittingflags = HCGE_DSBLIT_ROTATE180;
            break;

        case ROTATE_270:
            state->blittingflags = HCGE_DSBLIT_ROTATE270;
            break;

        case ROTATE_0:
        default:
            state->blittingflags = HCGE_DSBLIT_NOFX;
            if ((dst_info.rgb32 == src_info.rgb32) && (dst_info.pitch == src_info.pitch)) hcge_ctx->blit_direct = true;
            break;
    }

    state->render_options = HCGE_DSRO_NONE;
    state->drawingflags = HCGE_DSDRAW_NOFX;

    state->src_blend = HCGE_DSBF_SRCALPHA;
    state->dst_blend = HCGE_DSBF_ZERO;

    state->destination.config.size.w = dst_info.full_width;
    state->destination.config.size.h = dst_info.full_height;
    state->destination.config.format = dst_info.rgb32 ? HCGE_DSPF_ARGB : HCGE_DSPF_RGB16;
    state->dst.phys = (uint32_t)PHY_ADDR(dst_info.frame);
    state->dst.pitch = dst_info.pitch;

    state->source.config.size.w = src_info.full_width;
    state->source.config.size.h = src_info.full_height;
    state->source.config.format = src_info.rgb32 ? HCGE_DSPF_ARGB : HCGE_DSPF_RGB16;
    state->src.phys = (uint32_t)PHY_ADDR(src_info.frame);
    state->src.pitch = src_info.pitch;

	cache_flush(src_info.frame, src_info.pitch * src_info.full_height);

    state->accel = HCGE_DFXL_BLIT;
    hcge_set_state(hcge_ctx, &hcge_ctx->state, state->accel);
	if (!hcge_blit(hcge_ctx, &srect, dst_info.x, dst_info.y)) {
		static int count = 0;
		if (count == 0)
            frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"hcge_blit failed\n");
		count = (count + 1) % 60;
	}
    hcge_engine_sync(hcge_ctx);

    cache_invalidate(dst_info.frame, dst_info.pitch * dst_info.full_height);
}

void hcge_accel_stretch_blit(FrameInfo src_info, FrameInfo dst_info) {
	if (!src_info.frame || !dst_info.frame)return;
	
	hcge_state *state = &hcge_ctx->state;
    hcge_ctx->blit_direct = false;
	HCGERectangle srect = {src_info.x, src_info.y, src_info.width, src_info.height};
    HCGERectangle drect = {dst_info.x, dst_info.y, dst_info.width, dst_info.height};

    state->render_options = HCGE_DSRO_NONE;
    state->drawingflags = HCGE_DSDRAW_NOFX;
    state->blittingflags = HCGE_DSBLIT_NOFX;

    state->src_blend = HCGE_DSBF_SRCALPHA;
    state->dst_blend = HCGE_DSBF_ZERO;

    state->destination.config.size.w = dst_info.full_width;
    state->destination.config.size.h = dst_info.full_height;
    state->destination.config.format = dst_info.rgb32 ? HCGE_DSPF_ARGB : HCGE_DSPF_RGB16;
    state->dst.phys = (uint32_t)PHY_ADDR(dst_info.frame);
    state->dst.pitch = dst_info.pitch;

    state->source.config.size.w = src_info.full_width;
    state->source.config.size.h = src_info.full_height;
    state->source.config.format = src_info.rgb32 ? HCGE_DSPF_ARGB : HCGE_DSPF_RGB16;
    state->src.phys = (uint32_t)PHY_ADDR(src_info.frame);
    state->src.pitch = src_info.pitch;

	cache_flush(src_info.frame, src_info.pitch * src_info.full_height);

    state->accel = HCGE_DFXL_STRETCHBLIT;
    hcge_set_state(hcge_ctx, &hcge_ctx->state, state->accel);
	if (!hcge_stretch_blit(hcge_ctx, &srect, &drect)) {
		static int count = 0;
		if (count == 0)
            frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"hcge_stretch_blit failed\n");
		count = (count + 1) % 60;
	}
    hcge_engine_sync(hcge_ctx);

    cache_invalidate(dst_info.frame, dst_info.pitch * dst_info.full_height);
}

void hcge_fb_fill_rect(FrameInfo fill_frame, uint32_t color) {
    if (!hcge_ctx || !fill_frame.frame || fill_frame.width == 0 || fill_frame.height == 0 || fill_frame.full_width == 0 || fill_frame.full_height == 0) return;

    HCGERectangle rect = {
        .x = fill_frame.x,
        .y = fill_frame.y,
        .w = fill_frame.width,
        .h = fill_frame.height
    };

    hcge_state *state = &hcge_ctx->state;

    state->dst.phys  = (uint32_t)PHY_ADDR(fill_frame.frame);
    state->dst.pitch = fill_frame.pitch;
    state->destination.config.format = fill_frame.rgb32 ? HCGE_DSPF_ARGB : HCGE_DSPF_RGB16;
    state->destination.config.size.w = fill_frame.full_width;
    state->destination.config.size.h = fill_frame.full_height;

    state->accel        = HCGE_DFXL_FILLRECTANGLE;
    state->drawingflags = HCGE_DSDRAW_NOFX;
    hcge_ctx->clip_en        = false;

    HCGEColor hcge_color;
    if (fill_frame.rgb32) {
        hcge_color.a = (color >> 24) & 0xFF;
        hcge_color.r = (color >> 16) & 0xFF;
        hcge_color.g = (color >> 8)  & 0xFF;
        hcge_color.b = color         & 0xFF;
    } else {
        hcge_color.a = 0xFF;
        hcge_color.r = ((color >> 11) & 0x1F) << 3;
        hcge_color.g = ((color >> 5)  & 0x3F) << 2;
        hcge_color.b = (color         & 0x1F) << 3;
    }
    state->color = hcge_color;

    cache_flush(fill_frame.frame, fill_frame.pitch * fill_frame.full_height);

    hcge_set_state(hcge_ctx, &hcge_ctx->state, state->accel);
    hcge_fill_rect(hcge_ctx, &rect);
    hcge_engine_sync(hcge_ctx);

    cache_invalidate(fill_frame.frame, fill_frame.pitch * fill_frame.full_height);
}

uint16_t* load_bmp_image(const char *file_path, unsigned *out_width, unsigned *out_height, unsigned *out_pitch) {
    if (out_width)  *out_width = 0;
    if (out_height) *out_height = 0;
    if (out_pitch)  *out_pitch = 0;

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        frontend_log_cb(RETRO_LOG_INFO, "DISPLAY_DRIVER" ,"Failed to open BMP: %s\n", file_path);
        return NULL;
    }

    BMPFileHeader file_header;
    BMPInfoHeader info_header;

    if (fread(&file_header, sizeof(BMPFileHeader), 1, file) != 1 ||
        fread(&info_header, sizeof(BMPInfoHeader), 1, file) != 1) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"Failed to read BMP headers.\n");
        fclose(file);
        return NULL;
    }

    if (file_header.type != 0x4D42 || info_header.bits_per_pixel != 24 || info_header.compression != 0) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"Unsupported BMP format. Must be uncompressed 24-bit.\n");
        fclose(file);
        return NULL;
    }

    unsigned width = info_header.width;
    unsigned height = abs(info_header.height);
    bool flip_vertical = (info_header.height > 0);
    unsigned pitch = width * sizeof(uint16_t);

    uint16_t *rgb565_buffer = (uint16_t *)malloc(width * height * sizeof(uint16_t));
    if (!rgb565_buffer) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"Memory allocation failed for RGB565 buffer.\n");
        fclose(file);
        return NULL;
    }

    if (fseek(file, file_header.offset, SEEK_SET) != 0) {
        frontend_log_cb(RETRO_LOG_ERROR, "DISPLAY_DRIVER" ,"Failed to seek to pixel data.\n");
        free(rgb565_buffer);
        fclose(file);
        return NULL;
    }

    int row_padding = (4 - (width * 3) % 4) % 4;
    uint8_t bgr_pixel[3];

    for (int y = 0; y < height; y++) {
        int target_y = flip_vertical ? (height - 1 - y) : y;
        uint16_t *row_ptr = rgb565_buffer + (target_y * width);

        for (int x = 0; x < width; x++) {
            if (fread(bgr_pixel, 3, 1, file) != 1) {
                break;
            }

            uint16_t r = (bgr_pixel[2] & 0xF8) << 8;
            uint16_t g = (bgr_pixel[1] & 0xFC) << 3;
            uint16_t b = (bgr_pixel[0] & 0xF8) >> 3;

            row_ptr[x] = r | g | b;
        }
        fseek(file, row_padding, SEEK_CUR);
    }

    fclose(file);

    if (out_width)  *out_width = width;
    if (out_height) *out_height = height;
    if (out_pitch)  *out_pitch = pitch;

    return rgb565_buffer;
}

void draw_black_screen(void) {
    if (sf2000_ctx.fb_base == NULL) return;
    FrameInfo fill_frame = { sf2000_ctx.fb_base, sf2000_ctx.var.xres, sf2000_ctx.var.yres, sf2000_ctx.var.xres, sf2000_ctx.var.yres, sf2000_ctx.line_width, 0, 0, false };
    hcge_fb_fill_rect(fill_frame, 0x0000);
}

void draw_border(const char *core_path) {
    unsigned width = 0;
    unsigned height = 0;
    unsigned pitch = 0;

    char file_path[MAXPATH];
    snprintf(file_path, sizeof(file_path), "%s/%s/%s.bmp", BORDERS_DIRECTORY, core_path, core_path);

    uint16_t *rgb565_buffer = load_bmp_image(file_path, &width, &height, &pitch);

    if (!rgb565_buffer) {
        draw_black_screen();
        return;
    }

    blit_to_screen((const void*)rgb565_buffer, width, height, pitch, false, SCALE_STRETCH);
    free(rgb565_buffer);
}

void calculate_scale(FrameInfo *src_info, FrameInfo *dst_info, ScalingMode mode) {
    if (!src_info || !dst_info || src_info->full_width == 0 || src_info->full_height == 0 || dst_info->full_width == 0 || dst_info->full_height == 0) return;
    // TODO: implement other stretching options to preserve the original image ratio

    ScalingMode local_mode = mode;
    if (mode == SCALE_ASPECT_INT && ((dst_info->full_width < src_info->full_width) || (dst_info->full_height < src_info->full_height))) local_mode = SCALE_ASPECT_FLOAT;

    switch (local_mode) {
        case SCALE_ASPECT_INT: { // ---------- INTEGER SCALING ----------
            int scale_x = dst_info->full_width / src_info->width;
            int scale_y = dst_info->full_height / src_info->height;
            int scale = (scale_x < scale_y) ? scale_x : scale_y;

            if (scale < 1) scale = 1; // no downscaling below 1x

            dst_info->width = src_info->width * scale;
            dst_info->height = src_info->height * scale;
            break;
        }

        case SCALE_ASPECT_FLOAT: { // ---------- FLOAT SCALING ----------
            float src_aspect = (float)src_info->width / (float)src_info->height;
            float screen_aspect = (float)dst_info->full_width / (float)dst_info->full_height;

            float dst_w_f, dst_h_f;
            if (screen_aspect > src_aspect) {
                dst_h_f = (float)dst_info->full_height;
                dst_w_f = src_aspect * dst_h_f;
            } else {
                dst_w_f = (float)dst_info->full_width;
                dst_h_f = dst_w_f / src_aspect;
            }

            dst_info->width = (int)(dst_w_f + 0.5f);
            dst_info->height = (int)(dst_h_f + 0.5f);
            break;
        }

        case SCALE_STRETCH:
        default: // ---------- FULLSCREEN (IGNORE ASPECT) ----------
            dst_info->x = 0;
            dst_info->y = 0;
            dst_info->width = dst_info->full_width;
            dst_info->height = dst_info->full_height;
            return;
    }

    dst_info->x = (!gfx_custom_x_enabled) ? (int)((dst_info->full_width - dst_info->width) / 2) : gfx_custom_x;
    dst_info->y = (!gfx_custom_y_enabled) ? (int)((dst_info->full_height - dst_info->height) / 2) : gfx_custom_y;
}

void blit_to_screen(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, ScalingMode scale_mode) {
    if (!frame) return;
    
    static FrameInfo src_info;
    static FrameInfo dst_info;
    static ScalingMode last_mode = (ScalingMode)-1;

    if (width == sf2000_ctx.var.xres && height == sf2000_ctx.var.yres) {
        src_info = (FrameInfo){ frame, width, height, width, height, pitch, 0, 0, rgb32 };
        dst_info = (FrameInfo){ sf2000_ctx.fb_base, sf2000_ctx.var.xres, sf2000_ctx.var.yres, sf2000_ctx.var.xres, sf2000_ctx.var.yres, sf2000_ctx.line_width, 0, 0, false };
        hcge_accel_blit(src_info, dst_info, ROTATE_0);
        return;
    }

    if (width != src_info.full_width || height != src_info.full_height || pitch != src_info.pitch || rgb32 != src_info.rgb32|| scale_mode != last_mode) {
        src_info = (FrameInfo){ frame, width, height, width, height, pitch, 0, 0, rgb32 };
        dst_info = (FrameInfo){ sf2000_ctx.fb_base, sf2000_ctx.var.xres, sf2000_ctx.var.yres, sf2000_ctx.var.xres, sf2000_ctx.var.yres, sf2000_ctx.line_width, 0, 0, false };
        last_mode = scale_mode;
        calculate_scale(&src_info, &dst_info, scale_mode);
    }

    src_info.frame = frame;
    dst_info.frame = sf2000_ctx.fb_base;

    hcge_accel_stretch_blit(src_info, dst_info);
}

void frontend_video_cb(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (show_fps_counter) {
        static TickType_t prev_ticks = 0;
	    static int count_all = 0;
	    static int count_not_skipped = 0;

        TickType_t curr_ticks = xTaskGetTickCount();
        ++ count_all;
        if (data) ++count_not_skipped;

        if (curr_ticks - prev_ticks >= configTICK_RATE_HZ) {
            snprintf(current_fps_str, sizeof(current_fps_str), "%2d/%2d", count_not_skipped, count_all);

            prev_ticks = curr_ticks;
            count_all = 0;
            count_not_skipped = 0;
        }
    }
    if (!data) return;
    if (show_fps_counter) update_fps_counter(data, width, height, pitch, enable_xrgb8888_support);
    blit_to_screen(data, width, height, pitch, enable_xrgb8888_support, global_scaling_mode);
}

void frontend_video_init(void) {
	init_fb_device();
}

void frontend_video_deinit(void) {
	deinit_fb_device();
}
