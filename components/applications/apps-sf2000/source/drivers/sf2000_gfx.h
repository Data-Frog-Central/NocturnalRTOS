#ifndef SF2000_GFX_H__
#define SF2000_GFX_H__

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

extern bool preserve_aspect_ratio;
extern bool use_integer_scaling;
extern bool gfx_custom_x_enabled;
extern bool gfx_custom_y_enabled;
extern int gfx_custom_x;
extern int gfx_custom_y;

void init_fb(void);
void draw_border(const char *core_path);
void frontend_video_cb(const void *data, unsigned width, unsigned height, size_t pitch);

#endif // SF2000_GFX_H__
