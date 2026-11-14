#include "fb_drawing.h"

void draw_filled_circle(unsigned short *fb, int width, int height, int cx, int cy, int r, unsigned short color) {
    for (int y = -r; y <= r; y++) {
        int py = cy + y;
        if (py < 0 || py >= height) continue;
        
        for (int x = -r; x <= r; x++) {
            int px = cx + x;
            if (px < 0 || px >= width) continue;
            
            // Integer circle equation: x^2 + y^2 <= r^2
            if ((x * x) + (y * y) <= (r * r)) {
                fb[py * width + px] = color;
            }
        }
    }
}