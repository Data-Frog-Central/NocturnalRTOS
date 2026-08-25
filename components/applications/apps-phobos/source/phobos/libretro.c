#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <kernel/drivers/lcd_printf.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <libretro.h>
#include <core_api.h>
#include <dartos.h>

#include "../drivers/sf2000_audio.h"
#include "../drivers/sf2000_gfx.h"
#include "../drivers/sf2000_joypad.h"
#include "../drivers/sf2000_core_loading.h"
#include "file_functions.h"
#include "phobos.h"

static retro_audio_buffer_status_callback_t audio_buff_status_cb = NULL;
void (*core_frameskip)(bool flag);

struct retro_system_info sysinfo;
struct retro_game_info gameinfo;
struct retro_system_av_info av_info;
struct retro_game_info_ext game_info_ext;

bool core_supports_rom_in_buffer = false;
bool enable_xrgb8888_support = false;
bool mono_audio_enabled = true;
int brightness_percentage = 100;

bool close_emulator_flag = false;
bool safe_shutdown_flag = false;

char rom_path[MAXPATH];
char core_path[MAXPATH];
char assets_dir[MAXPATH];
char *dir = NULL;
char *rom_filename = NULL;
char *extension = NULL;
void *rom_buffer = NULL;
size_t rom_size;

unsigned short loading_txt_color = RGB565(255,255,255);  // white
unsigned short loading_bg_color = RGB565(0,0,0);   // black

void frontend_log_cb(enum retro_log_level level, const char *tag, const char *fmt, ...) {
    char buffer[500];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Determine the log level string
	// TODO: Add a setting to block logs as a whole and specific log levels
	// Users will probably only want WARN and ERROR logs
    const char *level_str = "";
    switch (level) {
        case RETRO_LOG_DEBUG: level_str = "DEBUG"; break;
        case RETRO_LOG_INFO: level_str = "INFO"; break;
        case RETRO_LOG_WARN: level_str = "WARN"; break;
        case RETRO_LOG_ERROR: level_str = "ERROR"; break;
        default: break;
    }
	// TODO: Right now we just print logs and rely on fileuart
    // While fileuart is useful for debugging, logging ALL print functions might be a bad idea
    // So maybe a new xlog function would be desirable
    printf("[%s][%s] %s", tag, level_str, buffer);
}

void core_log_cb(enum retro_log_level level, const char *fmt, ...) {
    char buffer[500];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Determine the log level string
	// TODO: Add a setting to block logs as a whole and specific log levels
	// Users will probably only want WARN and ERROR logs
    const char *level_str = "";
    switch (level) {
        case RETRO_LOG_DEBUG: level_str = "DEBUG"; break;
        case RETRO_LOG_INFO: level_str = "INFO"; break;
        case RETRO_LOG_WARN: level_str = "WARN"; break;
        case RETRO_LOG_ERROR: level_str = "ERROR"; break;
        default: break;
    }
	// TODO: Right now we just print logs and rely on fileuart
    // While fileuart is useful for debugging, logging ALL print functions might be a bad idea
    // So maybe a new xlog function would be desirable
    printf("[CORE][%s] %s", level_str, buffer);
}

static void frameskip_cb(bool skipping) {
    if (!audio_buff_status_cb)
        return;

	int occupancy = get_audio_occupancy();
    bool underrun_likely = (occupancy < 25);
    audio_buff_status_cb(skipping == true, occupancy, underrun_likely);
}

static void get_emu_framerate(double core_fps, double *emu_frame_rate) {
	// TODO: Allow people to force fps?
    /*switch (g_video_mode) {
        case EMU_VIDEO_PAL:
            *emu_frame_rate = 50.0f;
            break;

        case EMU_VIDEO_NTSC:
            *emu_frame_rate = 60.0f;
            break;

        case EMU_VIDEO_AUTO:
        default:
            *emu_frame_rate = core_fps;
            break;
    }*/

    /* clamp safety */
	*emu_frame_rate = core_fps;
    if (*emu_frame_rate < 10.0f || *emu_frame_rate > 200.0f) {
		int region = core_api.retro_get_region();
		if (region == RETRO_REGION_PAL) *emu_frame_rate = 50;
		else *emu_frame_rate = 60;
	}
}

static inline bool should_frameskip(int frames_behind, int occupancy) {
    /* audio-driven priority */
    if (occupancy < 25)
        return true;

    /* time-based fallback */
    if (frames_behind > 0)
        return true;

    return false;
}

bool frontend_environment_cb(unsigned cmd, void *data) {
	switch (cmd)
	{
		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		{
			struct retro_log_callback *cb = (struct retro_log_callback*)data;
			cb->log = core_log_cb;
			return true;
		}

		case RETRO_ENVIRONMENT_SET_MESSAGE:
		case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
		{
			const struct retro_message *msg = (const struct retro_message*)data;
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"SET_MESSAGE: %s\n", msg->msg);
			return true;
		}

		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		{
			const char *dir = SYSTEM_DIRECTORY;
			*(const char**)data = dir;
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"SYSTEM_DIRECTORY: \"%s\"\n", dir);
			return true;
		}

		case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
		{
			if (!assets_dir) return false;
			*(const char**)data = assets_dir;
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"ASSETS_DIRECTORY: \"%s\"\n", assets_dir);
			return true;
		}

		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		{
			const char *dir = SAVE_DIRECTORY;
			*(const char**)data = dir;
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"SAVE_DIRECTORY: \"%s\"\n", dir);
			return true;
		}

		case RETRO_ENVIRONMENT_GET_CAN_DUPE:
			*(bool*)data = true;
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"GET_CAN_DUPE: true\n");
			return true;

		case RETRO_ENVIRONMENT_GET_VARIABLE:
		{
			struct retro_variable *var = (struct retro_variable*)data;
			bool ret = config_get_var(var);
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"GET_VARIABLE: %s=%s\n", var->key, ret ? var->value : "");
			return true;
		}

		case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
		{
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"SET_MEMORY_MAPS\n");
			break;
		}

		case RETRO_ENVIRONMENT_SET_GEOMETRY:
		{
			const struct retro_game_geometry *geom = (const struct retro_game_geometry*)data;
            frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"SET_GEOMETRY: %ux%u, Aspect: %.3f.\n",
				geom->base_width, geom->base_height, geom->aspect_ratio);
			break;
		}

		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		{
    		enum retro_pixel_format fmt = *(enum retro_pixel_format*)data;

    		switch (fmt)
    		{
        		case RETRO_PIXEL_FORMAT_XRGB8888:
					enable_xrgb8888_support = true;
					frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"PIXEL_FORMAT: XRGB8888\n");
					return true;
        		case RETRO_PIXEL_FORMAT_RGB565:
					enable_xrgb8888_support = false;
					frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"PIXEL_FORMAT: RGB565\n");
            		return true;

        		default:
            		return false; // unsupported
    		}
		}

		case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
		{
    		struct retro_audio_buffer_status_callback *buff_status_cb = (struct retro_audio_buffer_status_callback*)data;

    		if (buff_status_cb && buff_status_cb->callback) {
        		audio_buff_status_cb = buff_status_cb->callback;
        		core_frameskip = frameskip_cb;
				frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"support for auto frameskipping enabled\n");
    		} else {
        		audio_buff_status_cb = NULL;
        		core_frameskip = NULL;
				frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"support for auto frameskipping disabled\n");
    		}

    		return true;
		}

		case RETRO_ENVIRONMENT_SHUTDOWN:
        {
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"RETRO_ENVIRONMENT_SHUTDOWN\n");
			close_emulator();
            return true;
        }

		case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
		{
    		const struct retro_system_content_info_override *overrides = (const struct retro_system_content_info_override *)data;
    		content_info_override_count = 0;

    		for (size_t i = 0; overrides[i].extensions; i++) {
        		if (content_info_override_count >= MAX_CONTENT_INFO_OVERRIDES) {
					frontend_log_cb(RETRO_LOG_WARN, "ENVIRON" ,"SET_CONTENT_INFO_OVERRIDE: too many overrides, truncating at %zu\n", (size_t)MAX_CONTENT_INFO_OVERRIDES);
            		break;
        		}
        		content_info_overrides[content_info_override_count++] = &overrides[i];
    		}

    		return true;
		}

// RETRO_ENVIRONMENT_PRIVATE
		case RETRO_ENVIRONMENT_GET_ROMS_DIRECTORY:
        {
			const char *dir = ROMS_DIRECTORY;
			*(const char**)data = dir;
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"ROMS_DIRECTORY: \"%s\"\n", dir);
			return true;
        }

        case RETRO_ENVIRONMENT_GET_CONFIG_DIRECTORY:
        {
			const char *dir = CONFIG_DIRECTORY;
			*(const char**)data = dir;
			frontend_log_cb(RETRO_LOG_INFO, "ENVIRON" ,"CONFIG_DIRECTORY: \"%s\"\n", dir);
			return true;
        }

        case RETRO_ENVIRONMENT_RUN_EMULATOR:
        {	
			const struct retro_private_emulator_paths *paths = (const struct retro_private_emulator_paths*)data;
            snprintf(core_path, sizeof(core_path), "%s", paths->core_path);
            snprintf(rom_path, sizeof(rom_path), "%s", paths->rom_path);
			close_emulator();
            return true;
        }

		case RETRO_ENVIRONMENT_GET_HCGE_ACCEL_FUNCTIONS:
		{
			struct retro_private_accel_functions *accel_functions = (struct retro_private_accel_functions*)data;
			accel_functions->hcge_fb_fill_rect = hcge_fb_fill_rect;
			accel_functions->hcge_accel_blit = hcge_accel_blit;
			accel_functions->hcge_accel_stretch_blit = hcge_accel_stretch_blit;
			return true;
		}

        default:
            // Unknown/unsupported command
            return false;
	}
}

int16_t frontend_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0 || device != RETRO_DEVICE_JOYPAD)
        return 0;

    input_bits_t state;
    joypad_get_buttons(port, &state);

    // Return 1 if the bit corresponding to 'id' is set, 0 otherwise
    return !!(state.data[0] & (1 << id));
}

bool load_game(const char *file_path) {
	bool is_wqw = false;
	bool is_zip = false;
	char filename_in_archive[MAXPATH];

	is_zip = is_zip_wqw_file(file_path, &is_wqw);
	if (!sysinfo.block_extract && (!sysinfo.need_fullpath || extension_supports_no_fullpath(extension))) core_supports_rom_in_buffer = true;

    if (core_supports_rom_in_buffer || (is_zip && !sysinfo.need_fullpath && !sysinfo.block_extract)) {
		if (is_zip) {
			if (!extract_zip_file(file_path, &rom_buffer, &rom_size, filename_in_archive, sizeof(filename_in_archive))) return false;
			extract_extension(filename_in_archive, &extension);
			core_supports_rom_in_buffer = false;
			if (!sysinfo.block_extract && (!sysinfo.need_fullpath || extension_supports_no_fullpath(extension))) core_supports_rom_in_buffer = true;
			if (!core_supports_rom_in_buffer) {
				frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"Core has no zip support!\n");
				return false;
			}
			frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"Game loaded into temp buffer. size=%ld\n", rom_size);
		} else {
			FILE *hfile = fopen(file_path, "rb");
			if (!hfile) {
				frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"Error opening rom file=%s\n", file_path);
				return false;
			}

	    	fseeko(hfile, 0, SEEK_END);
	    	rom_size = ftell(hfile);
	    	fseeko(hfile, 0, SEEK_SET);

	    	rom_buffer = malloc(rom_size);

	    	fread(rom_buffer, 1, rom_size, hfile);
			fclose(hfile);
				
			frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"Game loaded into temp buffer. size=%ld\n", rom_size);
		}
    }

	// Game Info
	gameinfo.path = rom_path;
	gameinfo.data = core_supports_rom_in_buffer ? rom_buffer : NULL;
	gameinfo.size = core_supports_rom_in_buffer ? rom_size : 0;
	gameinfo.meta = NULL; // TODO: What's this for?

	// Extended Game Info
	game_info_ext.full_path = rom_path;
	game_info_ext.archive_path = is_zip ? rom_path : NULL;
	game_info_ext.archive_file = is_zip ? filename_in_archive : NULL;
	game_info_ext.dir = dir;
	game_info_ext.name = rom_filename; // Going with method 1 unless we decide to support multiple files in a zip
	game_info_ext.ext = extension;
	game_info_ext.meta = NULL; // TODO: What's this for?
	game_info_ext.data = core_supports_rom_in_buffer ? rom_buffer : NULL;
	game_info_ext.size = core_supports_rom_in_buffer ? rom_size : 0;
	game_info_ext.file_in_archive = is_zip;
	game_info_ext.persistent_data = false; // TODO: Does anything need this?

	bool ret = core_api.retro_load_game(&gameinfo);
	if (!ret) { 
		frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"retro_load_game failed\n");
        return false;
    }
	frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"retro_load_game ok\n");
	return true;
}

bool run_emulator(const char *game_path, const char *core_path, int load_state) {
	extract_path_components(game_path, &dir, &rom_filename, &extension);
	bool ret = false;

	audio_buff_status_cb = NULL;
	core_frameskip = NULL;

    frontend_log_cb(RETRO_LOG_DEBUG, "FRONTEND" ,"loading core\n", load_state);
	ret = load_core(core_path);
	if (!ret) return false;
	
	// Pass frontend functions to core
	core_api.retro_set_video_refresh(frontend_video_cb);
	core_api.retro_set_audio_sample_batch(frontend_audio_batch_cb);
	core_api.retro_set_audio_sample_batch(mono_audio_enabled ? frontend_mono_audio_batch_cb : frontend_audio_batch_cb);
	core_api.retro_set_input_poll(frontend_input_poll_cb);
	core_api.retro_set_input_state(frontend_input_state_cb);
	core_api.retro_set_environment(frontend_environment_cb);

	// Begin retro init
	frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"Retro Init\n");
	core_api.retro_get_system_info(&sysinfo);
	snprintf(assets_dir, sizeof(assets_dir), "%s/%s", ASSETS_DIRECTORY, sysinfo.library_name);
	apply_backlight_brightness(brightness_percentage, 10000, 1);
	core_config_load();
	rom_config_load();
	core_api.retro_init();

	frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"%s, (%s%s), %s, %s, %s%s%s, State:%d\n",
		sysinfo.library_name ? sysinfo.library_name : "(null)",
    	core_path ? core_path : "null",
		core_path ? ".hcrtos" : "",
		sysinfo.library_version ? sysinfo.library_version : "(null)",
    	dir ? dir : "(null)",
    	rom_filename ? rom_filename : "(null)",
		extension ? "." : "",
    	extension ? extension : "",
    	load_state
	);
	
	show_loading_screen(
		false,
		true,
    	loading_txt_color,
    	loading_bg_color,
    	" %s\n\n (%s%s)\n\n %s\n\n %s\n\n %s%s%s\n\n State:%d\n\n",
		sysinfo.library_name ? sysinfo.library_name : "(null)",
		core_path ? core_path : "null",
		core_path ? ".hcrtos" : "",
		sysinfo.library_version ? sysinfo.library_version : "(null)",
    	dir ? dir : "(null)",
    	rom_filename ? rom_filename : "(null)",
		extension ? "." : "",
    	extension ? extension : "",
    	load_state
	);

	ret = load_game(game_path);
	if (!ret) return false;
	load_srm(0);

	core_api.retro_get_system_av_info(&av_info);
    audio_init(temp_audio_device, av_info.timing.sample_rate);
	joypad_init(temp_joypad_device);
	
	if (core_supports_rom_in_buffer) {
		free(rom_buffer);
		rom_buffer = NULL;
	}

    double emu_frame_rate = 0;
	get_emu_framerate(av_info.timing.fps, &emu_frame_rate);
    double frame_time_us = 1000000.0 / emu_frame_rate;
	TickType_t frame_ticks = pdMS_TO_TICKS((uint32_t)(frame_time_us / 1000.0 + 0.5));
	if (frame_ticks < 1) frame_ticks = 1;
    TickType_t last_wake = xTaskGetTickCount();
	frontend_log_cb(RETRO_LOG_DEBUG, "FRONTEND", "frame_time_us=%f frame_ticks=%u emu_frame_rate=%f\n", frame_time_us, frame_ticks, emu_frame_rate);
	
	// TODO: Cache controller input and check for hotkeys
	// TODO: Pause menu and ability to exit
	draw_border(core_path); // Draw the border right before the main loop
	while (1) {
		frontend_input_poll_cb();
		frontend_check_hotkeys();
		if (close_emulator_flag || safe_shutdown_flag) {
			if (close_emulator_flag) close_emulator_flag = false;
			break;
		}
		if (core_frameskip) {
    		TickType_t now = xTaskGetTickCount();
    		TickType_t lag = now - last_wake;
    		int frames_behind = lag / frame_ticks;

    		if (frames_behind > 5) frames_behind = 5;  // clamp lag

    		int steps = 1 + frames_behind;

    		int occupancy = get_audio_occupancy();
    		core_frameskip(should_frameskip(frames_behind, occupancy));

    		for (int i = 0; i < steps; i++) core_api.retro_run();
		} else core_api.retro_run();

    	vTaskDelayUntil(&last_wake, frame_ticks);
	}

	frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"Retro Deinit\n");
	// TODO: Proper deinit
	save_srm(0);
	core_api.retro_unload_game();
	core_api.retro_deinit();
	audio_deinit();
	return true;
}

void close_emulator(void) {
	close_emulator_flag = true;
}
