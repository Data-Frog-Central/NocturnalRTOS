#ifndef __LCD_PRINTF_H
#define __LCD_PRINTF_H

#define BSOD_BG 0x1f // Blue
#define BSOD_TXT 0xffff // White

extern void lcd_init(void);
extern int lcd_vprintf(const char* fmt, va_list ap);
extern int lcd_printf(const char *fmt, ...);
extern void lcd_flush(unsigned short text_color, unsigned short background_color);

extern void dbg_show_noblock(unsigned short text_color, unsigned short background_color, const char *fmt, ...);
extern void lcd_bsod(const char *fmt, ...);

#endif //__LCD_PRINTF_H