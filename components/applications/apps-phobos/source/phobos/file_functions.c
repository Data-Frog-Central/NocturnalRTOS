
#include <ctype.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <unistd.h>

#include <libretro.h>
#include <file/file_path.h>
#include <file/config_file.h>

#include <core_api.h>
#include <zip.h>

#include "../drivers/sf2000_gfx.h"
#include "../drivers/sf2000_core_loading.h"
#include "file_functions.h"
#include "phobos.h"

static config_file_t *frontend_config = NULL;
static config_file_t *core_config = NULL;
static config_file_t *rom_config = NULL;

const struct retro_system_content_info_override* content_info_overrides[MAX_CONTENT_INFO_OVERRIDES];
size_t content_info_override_count = 0;

char *temp_rom_path, *temp_core_path, *temp_audio_device, *temp_joypad_device;

void extract_extension(const char *filename, char **extension) {
    char *dot = strrchr(filename, '.');
	if (dot == NULL) {
        *extension = NULL;
    } else {
        *extension = strdup(dot + 1);
    }
}

void extract_path_components(const char *filepath, char **dir, char **filename, char **extension) {
    if (filepath == NULL || filepath[0] == '\0') {
        if (dir) *dir = NULL;
        if (filename) *filename = NULL;
        if (extension) *extension = NULL;
        return;
    }

    // Copy filepath to avoid modifying the original string
    char *path_copy = strdup(filepath);

    // Extract directory part by finding the last slash
    char *last_slash = strrchr(path_copy, '/');
    *last_slash = '\0';
    *dir = strdup(path_copy);
    *filename = strdup(last_slash + 1);
    extract_extension(*filename, extension);

	if (*extension != NULL) {
        char *dot = strrchr(*filename, '.');
        if (dot != NULL) {
            *dot = '\0';
        }
    }

    free(path_copy);
}

bool is_zip_wqw_file(const char* path, bool* is_wqw) {
	*is_wqw = false;
	FILE *file = fopen(path, "rb");
    if (file == NULL) {
        frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"Error opening rom file=%s\n", path);
        return 0;
    }

    uint32_t signature;
    size_t bytes_read = fread(&signature, 1, sizeof(signature), file);
    fclose(file);
    if (bytes_read < sizeof(signature)) return 0;

	// Check if the first 4 bytes match the WQW file header (0x57 0x51 0x57 0x03)
    if (signature == 0x03575157) {
		*is_wqw = true; // It's a valid WQW file
		return 1; // It's a valid ZIP file
	}

    // Check if the first 4 bytes match the ZIP file header (0x50 0x4B 0x03 0x04)
    if (signature == 0x04034b50) {
        return 1;  // It's a valid ZIP file
    }

    return 0;  // Not a ZIP file
}

static bool str_eq_ci(const char *a, const char *b, size_t len) {
    for (size_t i = 0; i < len; i++)
    {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return false;
    }
    return true;
}

static bool extension_in_list_ci(const char *ext, const char *list) {
    if (!ext || !list) return false;

    const char *start = list;
    const char *end;

    while (*start)
    {
        end = strchr(start, '|');
        if (!end) end = start + strlen(start);

        size_t len = end - start;

        if (strlen(ext) == len && str_eq_ci(ext, start, len))
            return true;

        if (*end == '\0')
            break;
        start = end + 1;
    }

    return false;
}

bool extension_supports_no_fullpath(const char *ext) {
    for (size_t i = 0; i < content_info_override_count; i++) {
        const struct retro_system_content_info_override *ovr = content_info_overrides[i];

        if (!ovr->need_fullpath && extension_in_list_ci(ext, ovr->extensions))
            return true;
    }
    return false;
}

bool extract_zip_file(const char *file_path, void **rom_buffer, size_t *rom_size, char *out_name, size_t out_name_size) {
    struct zip_t *zip = zip_open(file_path, 0, 'r');
    if (!zip) return false;

    int n = zip_entries_total(zip);

    for (int i = 0; i < n; i++) {

        if (zip_entry_openbyindex(zip, i) < 0)
            continue;

        if (zip_entry_isdir(zip)) {
            zip_entry_close(zip);
            continue;
        }

        void *buf = NULL;
        size_t sz = 0;

        ssize_t ret = zip_entry_read(zip, &buf, &sz);
        const char *name = zip_entry_name(zip);
        if (out_name && out_name_size > 0) snprintf(out_name, out_name_size, "%s", name);

        zip_entry_close(zip);

        if (ret > 0 && buf && sz > 0) {
            *rom_buffer = buf;
            *rom_size = sz;
            zip_close(zip);
            return true;
        }
    }

    zip_close(zip);
    return false;
}

void config_free() {
	config_file_free(frontend_config);
	config_file_free(core_config);
	config_file_free(rom_config);
    frontend_config = NULL;
    core_config = NULL;
    rom_config = NULL;

}

static bool config_get_var_helper(config_file_t *config_file, struct retro_variable *var) {
	if (!config_file)
		return false;

	const struct config_entry_list *entry = config_get_entry(config_file, var->key);
	if (!entry)
		return false;

	var->value = entry->value;
	return true;
}

bool config_get_var(struct retro_variable *var) {
	bool ret = false;
	if (rom_config) ret = config_get_var_helper(rom_config, var);
	if (!ret && core_config) ret = config_get_var_helper(core_config, var);
	return ret;
}

void frontend_load_settings(config_file_t *config_file, bool first_run) {
    if (config_file == NULL) return;

    if (first_run) { // There is no reason to check these again
        // Initialize these in case no config
        snprintf(core_path, sizeof(core_path), "FrogUI");
    	snprintf(rom_path, sizeof(rom_path), "/media/mmcblk0p2/ROMS/menu/p");

        config_get_string(config_file, "hcrtos_rom_path", &temp_rom_path);
        if (temp_rom_path) {
            strncpy(rom_path, temp_rom_path, MAXPATH - 1);
	        rom_path[MAXPATH - 1] = '\0';
        }

        config_get_string(config_file, "hcrtos_core_path", &temp_core_path);
        if (temp_core_path) {
            strncpy(core_path, temp_core_path, MAXPATH - 1);
	        core_path[MAXPATH - 1] = '\0';
        }

        config_get_string(config_file, "hcrtos_audio_device", &temp_audio_device);
        config_get_string(config_file, "hcrtos_joypad_device", &temp_joypad_device);
    }

	config_get_bool(config_file, "hcrtos_mono_audio_enabled", &mono_audio_enabled);
    config_get_uint(config_file, "hcrtos_brightness_percentage", &brightness_percentage);
    config_get_bool(config_file, "hcrtos_gfx_custom_x_enabled", &gfx_custom_x_enabled);
    config_get_bool(config_file, "hcrtos_gfx_custom_y_enabled", &gfx_custom_y_enabled);
    config_get_int(config_file, "hcrtos_gfx_custom_x", &gfx_custom_x);
    config_get_int(config_file, "hcrtos_gfx_custom_y", &gfx_custom_y);
    config_get_bool(config_file, "hcrtos_show_fps_counter", &show_fps_counter);
        
    const struct config_entry_list *e;
    e = config_get_entry(config_file, "hcrtos_scaling_mode");
	if (e) {
	    if (strcasecmp(e->value, "stretch") == 0)
			global_scaling_mode = SCALE_STRETCH;
		else if (strcasecmp(e->value, "aspect float") == 0)
			global_scaling_mode = SCALE_ASPECT_FLOAT;
		else if (strcasecmp(e->value, "aspect int") == 0)
			global_scaling_mode = SCALE_ASPECT_INT;
		else if (strcasecmp(e->value, "core float") == 0)
			global_scaling_mode = CORE_PROVIDED_FLOAT;
		else if (strcasecmp(e->value, "core int") == 0)
			global_scaling_mode = CORE_PROVIDED_INT;
        else if (strcasecmp(e->value, "custom") == 0)
			global_scaling_mode = CUSTOM;
	}
}

void frontend_config_load(void) {
    bool ret = false;
    char config_frontend_filepath[MAXPATH];
	snprintf(config_frontend_filepath, sizeof(config_frontend_filepath), "%s/%s.opt", CONFIG_DIRECTORY, "Phobos");

    // load global hcrtos options
    if (access(config_frontend_filepath, F_OK) == 0) {
        frontend_config = config_file_new_alloc();
        ret = config_append_file(frontend_config, config_frontend_filepath);
        frontend_load_settings(frontend_config, true);
    }
    frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"config_load: %s %s\n", config_frontend_filepath, ret ? "loaded" : "not found");
}

void core_config_load(void) {
    bool ret = false;
    char config_core_filepath[MAXPATH];
	snprintf(config_core_filepath, sizeof(config_core_filepath), "%s/%s/%s.opt", CONFIG_DIRECTORY, sysinfo.library_name, sysinfo.library_name);

	// load per core options
    if (access(config_core_filepath, F_OK) == 0) {
        core_config = config_file_new_alloc();
        ret = config_append_file(core_config, config_core_filepath);
        frontend_load_settings(core_config, false);
    }
    frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"config_load: %s %s\n", config_core_filepath, ret ? "loaded" : "not found");
}

void rom_config_load(void) {
    if (!rom_filename || rom_filename[0] == '\0' || !sysinfo.library_name) return;
    
    bool ret = false;
    char config_game_filepath[MAXPATH];
	snprintf(config_game_filepath, sizeof(config_game_filepath), "%s/%s/options/%s.opt", CONFIG_DIRECTORY, sysinfo.library_name, rom_filename);

	// load per game options
    if (access(config_game_filepath, F_OK) == 0) {
        rom_config = config_file_new_alloc();
        ret = config_append_file(rom_config, config_game_filepath);
        frontend_load_settings(rom_config, false);
    }
    frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"config_load: %s %s\n", config_game_filepath, ret ? "loaded" : "not found");
}

int create_dir(const char *path) {
    if (access(path, F_OK) != 0) {
        frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"filepath: creating %s\n", path);
        if (mkdir(path, 0755) != 0) {
            frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"mkdir failed\n");
            return -1;
        }
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            if (fsync(fd) != 0)
                frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"fsync failed\n");
            close(fd);
        }
    }
    return 0;
}

void build_srm_filepath(char *filepath, size_t size, const char *basename, const char *extension, int slot, const char * type) {
    char ext[5];
    if (slot) snprintf(ext, 5, "%s%d", extension, slot);
    else snprintf(ext, 5, "%s", extension);
    
	char directory[MAXPATH] = SAVE_DIRECTORY;
	create_dir(directory); // Make sure SAVE_DIRECTORY exists 

	/*if(g_per_core_srm){
		snprintf(directory, size, "%s/%s", SAVE_DIRECTORY, sysinfo.library_name);
		create_dir(directory);	// Make sure SAVE_DIRECTORY/sysinfo.library_name exists 
	}*/

	snprintf(filepath, size, "%s/%s.%s", directory, basename, extension);
    frontend_log_cb(RETRO_LOG_INFO, "FRONTEND" ,"%s_%s file: %s\n", type, extension, filepath);
}

void save_srm(int slot){
    char ram_filepath[MAXPATH];

    // Save SRM
    build_srm_filepath(ram_filepath, sizeof(ram_filepath), rom_filename, "srm", slot, "Save");
    size_t save_size = core_api.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);	
	if(save_size == 0) return;
	FILE *ram_file = fopen(ram_filepath, "wb");
	if (!ram_file) return;
	fwrite(core_api.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM), save_size, 1, ram_file);
    fsync(fileno(ram_file));
	fclose(ram_file);

	// Save RTC
    build_srm_filepath(ram_filepath, sizeof(ram_filepath), rom_filename, "rtc", slot, "Save");
    size_t rtc_size = core_api.retro_get_memory_size(RETRO_MEMORY_RTC);
    if (rtc_size == 0) return;
    FILE *rtc_file = fopen(ram_filepath, "wb");
    if (!rtc_file) return;
    fwrite(core_api.retro_get_memory_data(RETRO_MEMORY_RTC), rtc_size, 1, rtc_file);
    fsync(fileno(rtc_file));
    fclose(rtc_file);
}

void load_srm(int slot){
    char ram_filepath[MAXPATH];

    // Load SRM
    build_srm_filepath(ram_filepath, sizeof(ram_filepath), rom_filename, "srm", slot, "Load");
	size_t save_size = core_api.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    FILE *ram_file = fopen(ram_filepath, "rb");
	if (!ram_file) return;
	fseeko(ram_file, 0, SEEK_END);
	size_t ram_file_size = ftell(ram_file);
	fseeko(ram_file, 0, SEEK_SET);

	if(ram_file_size < save_size){
		save_size = ram_file_size;
	}

	if(save_size == 0){
		fclose(ram_file);
		return;
	}

	fread(core_api.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM), 1, save_size, ram_file);
	fclose(ram_file);

	// Load RTC
    build_srm_filepath(ram_filepath, sizeof(ram_filepath), rom_filename, "rtc", slot, "Load");
    size_t rtc_size = core_api.retro_get_memory_size(RETRO_MEMORY_RTC);
    FILE *rtc_file = fopen(ram_filepath, "rb");
    if (!rtc_file) return;
    fseeko(rtc_file, 0, SEEK_END);
    size_t rtc_file_size = ftell(rtc_file);
    fseeko(rtc_file, 0, SEEK_SET);

    if (rtc_file_size < rtc_size) {
        rtc_size = rtc_file_size;
    }

    if (rtc_size == 0) {
        fclose(rtc_file);
        return;
    }

    fread(core_api.retro_get_memory_data(RETRO_MEMORY_RTC), 1, rtc_size, rtc_file);
    fclose(rtc_file);
}
