#ifndef __LCDFONT_H
#define __LCDFONT_H

/* borrowed from RetroArch/gfx/drivers_font_renderer, usual license restrictions apply */
#define FONT_WIDTH 5
#define FONT_HEIGHT 10

#define FONT_WIDTH_STRIDE (FONT_WIDTH + 1)

#define FONT_OFFSET(x) ((x - 0x20) * ((FONT_HEIGHT * FONT_WIDTH + 7) / 8))

extern const unsigned char lcd_font[];

#endif
