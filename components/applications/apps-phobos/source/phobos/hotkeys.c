#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <libretro.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <helix/nocturnal_paths.h>
#include <helix/helix_logging.h>
#include <nocturnal_joypad.h>

#include "hotkeys.h"
#include "phobos.h"

static uint16_t hotkey_exit_mask = ((1U << RETRO_DEVICE_ID_JOYPAD_SELECT) | (1U << RETRO_DEVICE_ID_JOYPAD_START));

static bool hotkey_exit(void) {
	if (sysinfo.library_name && strcmp(sysinfo.library_name, "FrogUI") == 0) safe_shutdown_flag = true;
	else {
		snprintf(core_path, sizeof(core_path), "FrogUI");
    	snprintf(rom_path, sizeof(rom_path), "/media/mmcblk0p2/ROMS/menu/p");
		close_emulator();
	}
	return false; // Needed because a bool is expected
}

static const hotkey_entry_t hotkeys[] = {
    { &hotkey_exit_mask, hotkey_exit },
};

void frontend_check_hotkeys(void) {
	uint16_t state = joypad_btn_state;

    for (size_t i = 0; i < sizeof(hotkeys)/sizeof(hotkeys[0]); ++i) {
		const uint16_t *mask_ptr = hotkeys[i].mask;
		if (mask_ptr != NULL && *mask_ptr != IGNORE_BTN && hotkeys[i].action != NULL && state == *mask_ptr) {
			joypad_btn_state = 0;
            bool ret = hotkeys[i].action();
            //if (ret) show_osd_message(osd_message); //TODO: ADD OSD
			break;
        }
    }
}

static const hotkey_button_map_t HOTKEY_BUTTONS[] = {
    { "B",      (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_B) },
    { "Y",      (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_Y) },
    { "SELECT", (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_SELECT) },
    { "START",  (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_START) },
    { "UP",     (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_UP) },
    { "DOWN",   (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_DOWN) },
    { "LEFT",   (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_LEFT) },
    { "RIGHT",  (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_RIGHT) },
    { "A",      (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_A) },
    { "X",      (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_X) },
    { "L",      (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_L) },
    { "R",      (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_R) },
};

static uint16_t lookup_button_bit(const char *token) {
    for (size_t i = 0; i < sizeof(HOTKEY_BUTTONS) / sizeof(HOTKEY_BUTTONS[0]); i++) {
        if (strcasecmp(token, HOTKEY_BUTTONS[i].name) == 0) {
            return (uint16_t)HOTKEY_BUTTONS[i].bit;
        }
    }
    return 0;
}

static bool parse_hotkey_mask(const char *value, uint16_t *out_mask) {
    if (!value || !out_mask || *value == '\0') return false;
    if (strcasecmp(value, "none") == 0 || strcasecmp(value, "disabled") == 0) {
        *out_mask = IGNORE_BTN;
        return true;
    }

    uint16_t mask = 0;
    const char *start = value;

    while (true) {
        const char *plus = strchr(start, '+');
        size_t len = plus ? (size_t)(plus - start) : strlen(start);
        if (len == 0) return false;

        char name[32];
        if (len >= sizeof(name)) return false;
        memcpy(name, start, len);
        name[len] = '\0';

        uint16_t bit = lookup_button_bit(name);
        if (bit == 0) return false;
        mask |= bit;

        if (!plus) break;
        start = plus + 1;
    }

    *out_mask = mask;
    return true;
}

static void set_hotkeys_helper(config_file_t *config_file, const char* name, uint16_t *out_mask) {
	const struct config_entry_list *e = config_get_entry(config_file, name);
	if (!e || !e->value) return;
	bool ret = parse_hotkey_mask(e->value, out_mask);
	frontend_log_cb(ret ? RETRO_LOG_INFO : RETRO_LOG_WARN, "HOTKEYS", "%s set to %s %s\n", name, e->value ? e->value : "NULL", ret ? "Successfully" : "Failed");
}

void set_hotkeys(config_file_t *config_file) {
	set_hotkeys_helper(config_file, "phobos_hotkey_exit", &hotkey_exit_mask);
}