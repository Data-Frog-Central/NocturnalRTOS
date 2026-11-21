#ifndef MENU_H__
#define MENU_H__

extern struct retro_system_info sysinfo;
extern struct retro_game_info gameinfo;
extern struct retro_system_av_info av_info;
extern struct retro_game_info_ext game_info_ext;

extern bool mono_audio_enabled;
extern int brightness_percentage;

extern bool close_emulator_flag;
extern bool safe_shutdown_flag;

extern bool dont_overwrite_saveram;
extern bool per_core_srm;
extern bool auto_savestate;

extern char rom_path[MAXPATH];
extern char core_path[MAXPATH];
extern char *dir, *rom_filename, *extension;
extern char *temp_audio_device, *temp_joypad_device;

extern unsigned short loading_txt_color;
extern unsigned short loading_bg_color;

bool run_emulator(const char *game_path, const char *core_path, int load_state);
void close_emulator(void);
int apply_backlight_brightness(int pwm_level, int pwm_frequency, int polarity);
void update_fps_counter(const void *data, unsigned width, unsigned height, size_t pitch, bool rgb32);

#endif // MENU_H__
