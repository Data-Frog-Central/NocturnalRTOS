#ifndef SF2000_GFX_H__
#define SF2000_GFX_H__

#include <dartos.h>

#define RGB565(r,g,b) ( ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3) )
#define RGB32(r, g, b) ( (0xFF << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF) ) // Automatically opaque 
#define ARGB32(a, r, g, b) ( ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF) )

typedef enum {
    SCALE_STRETCH, // Stretch to full screen res
    SCALE_ASPECT_FLOAT, // Aspect ratio is provided by framebuffer size, float scaling
    SCALE_ASPECT_INT, // Aspect ratio is provided by framebuffer size, int scaling
    CORE_PROVIDED_FLOAT, // Aspect ratio is provided by core (might stretch), float scaling
    CORE_PROVIDED_INT, // Aspect ratio is provided by core (might stretch), int scaling
    CUSTOM // Custom resolution
} ScalingMode;

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPFileHeader;

typedef struct {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pixels_per_m;
    int32_t  y_pixels_per_m;
    uint32_t colors_used;
    uint32_t colors_important;
} BMPInfoHeader;
#pragma pack(pop)

extern ScalingMode global_scaling_mode;
extern RotationMode global_rotation_mode;

extern bool gfx_custom_x_enabled;
extern bool gfx_custom_y_enabled;
extern int gfx_custom_x;
extern int gfx_custom_y;

extern bool show_fps_counter;
extern char current_fps_str[16];

void frontend_video_init(void);
void frontend_video_deinit(void);
void frontend_video_cb(const void *data, unsigned width, unsigned height, size_t pitch);
void blit_to_screen(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, ScalingMode scale_mode);
void hcge_accel_blit(FrameInfo src_info, FrameInfo dst_info, RotationMode rotation);
void hcge_accel_stretch_blit(FrameInfo src_info, FrameInfo dst_info);
void hcge_fb_fill_rect(FrameInfo fill_frame, uint32_t color);
void draw_border(const char *core_path);
uint16_t* load_bmp_image(const char *file_path, unsigned *out_width, unsigned *out_height, unsigned *out_pitch);

#endif // SF2000_GFX_H__
