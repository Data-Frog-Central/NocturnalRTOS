#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <kernel/drivers/lcd_font.h>
#include "../drivers/sf2000_gfx.h"

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