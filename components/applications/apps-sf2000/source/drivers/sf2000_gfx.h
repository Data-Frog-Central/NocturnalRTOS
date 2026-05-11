#ifndef SF2000_GFX_H__
#define SF2000_GFX_H__

extern bool preserve_aspect_ratio;
extern bool use_integer_scaling;

void init_fb(void);
void frontend_video_cb(const void *data, unsigned width, unsigned height, size_t pitch);

#endif // SF2000_GFX_H__