/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libretro.h>
#include <retro_miscellaneous.h>
#include "sf2000_joypad.h"
#include "../phobos/phobos.h"

#undef ARRAY_SIZE

#include <hcuapi/gpio.h>
#include <hcuapi/pinpad.h>

extern void frontend_log_cb(enum retro_log_level level, const char *tag, const char *fmt, ...);

//#define LOG_BTN_PRESS = true;
#define IGNORE_BTN 255

static pinpad_e key_shifter_clk_pin = PINPAD_L24;
static pinpad_e key_shifter_pl1_pin = PINPAD_L23;
static pinpad_e key_shifter_pl2_pin = PINPAD_L26;
static pinpad_e key_shifter_pl3_pin = PINPAD_INVALID;

static joypad_device_t current_device = DEVICE_SF2000;
static const uint8_t *current_shift_map;
static size_t current_shift_map_size; // Needed?
static uint16_t btn_state; // TODO: static var?

static void set_device_map(const uint8_t *map, int size) {
    current_shift_map = map;
    current_shift_map_size = size;
}

static void set_key_shifter_pins(pinpad_e clk, pinpad_e p1, pinpad_e p2, pinpad_e p3) {
    key_shifter_clk_pin = clk;
    key_shifter_pl1_pin  = p1;
    key_shifter_pl2_pin  = p2;
	key_shifter_pl3_pin = p3;
}

// TODO: the X60 handheld have BTN_L and BTN_R swapped compared to SF2000
// need to conditionaly adjust the enum based on if we compile for X60 or SF2000
// also add custom mapping
static uint8_t sf2000_joypad_map[] = {
	IGNORE_BTN,
	RETRO_DEVICE_ID_JOYPAD_R,
	RETRO_DEVICE_ID_JOYPAD_Y,
	RETRO_DEVICE_ID_JOYPAD_X,
	RETRO_DEVICE_ID_JOYPAD_L,
	RETRO_DEVICE_ID_JOYPAD_A,
	RETRO_DEVICE_ID_JOYPAD_B,
	RETRO_DEVICE_ID_JOYPAD_SELECT,
	RETRO_DEVICE_ID_JOYPAD_START,
	RETRO_DEVICE_ID_JOYPAD_UP,
	RETRO_DEVICE_ID_JOYPAD_DOWN,
	RETRO_DEVICE_ID_JOYPAD_LEFT,
	RETRO_DEVICE_ID_JOYPAD_RIGHT
};

bool hotkey_exit(void) {
	if (sysinfo.library_name && strcmp(sysinfo.library_name, "FrogUI") == 0) safe_shutdown_flag = true;
	else {
		snprintf(core_path, sizeof(core_path), "FrogUI");
    	snprintf(rom_path, sizeof(rom_path), "/media/mmcblk0p2/ROMS/menu/p");
		close_emulator();
	}
	return false; // Needed because a bool is expected
}

static const hotkey_entry_t hotkeys[] = {
    { HOTKEY_EXIT_MASK, hotkey_exit },
};

void joypad_init(const char *device_name) {
	frontend_log_cb(RETRO_LOG_INFO, "JOYPAD_DRIVER" ,"Joypad Init\n");
	btn_state = 0;

	if (strcmp(device_name, "SF2000") == 0) {
		current_device = DEVICE_SF2000;
		set_key_shifter_pins(PINPAD_L24, PINPAD_L23, PINPAD_L26, PINPAD_INVALID);
		set_device_map(sf2000_joypad_map, 13);
	} else if (strcmp(device_name, "GB300") == 0) {
		current_device = DEVICE_GB300;
		set_key_shifter_pins(PINPAD_L26, PINPAD_L27, PINPAD_L25, PINPAD_INVALID);
		set_device_map(sf2000_joypad_map, 13);
	} else if (strcmp(device_name, "DY19") == 0) {
		current_device = DEVICE_DY19;
		set_key_shifter_pins(PINPAD_L24, PINPAD_L25, PINPAD_L26, PINPAD_L27);
		set_device_map(sf2000_joypad_map, 13);
	}
}

static int32_t sf2000_joypad_button(unsigned port, uint16_t joykey)
{
	if (port != 0)
		return 0;

	if ((joykey == RETRO_DEVICE_ID_JOYPAD_MASK) || (joykey == 65535)) {
		//frontend_log_cb(RETRO_LOG_DEBUG, "JOYPAD_DRIVER" ,"btn_state=%u joykey=%u\n", btn_state, joykey);
		return btn_state;
	}

	//frontend_log_cb(RETRO_LOG_DEBUG, "JOYPAD_DRIVER" ,"btn_state=%u joykey=%u ret=%u\n", btn_state, joykey, (btn_state & (1 << joykey)));

	return (btn_state & (1 << joykey));
}

void joypad_get_buttons(unsigned port, input_bits_t *state)
{
	if (port == 0)
	{
		BITS_COPY16_PTR(state, btn_state);
	}
	else
	{
		BIT256_CLEAR_ALL_PTR(state);
	}

	//frontend_log_cb(RETRO_LOG_DEBUG, "JOYPAD_DRIVER" ,"btn_state=%u port=%u state->data[0]=%lu\n", btn_state, port, state->data[0]);
}

void frontend_check_hotkeys(void) {
	uint16_t state = btn_state;

    for (size_t i = 0; i < sizeof(hotkeys)/sizeof(hotkeys[0]); ++i) {
        if (state == hotkeys[i].mask) {
			btn_state = 0;
            bool ret = hotkeys[i].action();
            //if (ret) show_osd_message(osd_message); //TODO: ADD OSD
        }
    }
}

void frontend_input_poll_cb(void) {
    uint16_t new_state = 0;

    // Configure pins
    gpio_configure(key_shifter_clk_pin, GPIO_DIR_OUTPUT);
    gpio_set_output(key_shifter_clk_pin, 1);

    gpio_configure(key_shifter_pl1_pin, GPIO_DIR_OUTPUT);
    gpio_configure(key_shifter_pl2_pin, GPIO_DIR_OUTPUT);
	if (current_device == DEVICE_DY19) gpio_configure(key_shifter_pl3_pin, GPIO_DIR_OUTPUT);

    gpio_set_output(key_shifter_clk_pin, 0);
    usleep(4); // KEY_SHIFTER_LOAD_US

    // Set D0/D1 as inputs for serial reading
    gpio_configure(key_shifter_pl1_pin, GPIO_DIR_INPUT);
    gpio_configure(key_shifter_pl2_pin, GPIO_DIR_INPUT);
	if (current_device == DEVICE_DY19) gpio_configure(key_shifter_pl3_pin, GPIO_DIR_INPUT);

    // Read 16-bit shift register
    for (int i = 0; i < current_shift_map_size; i++) {
        int raw0 = 1 ^ gpio_get_input(key_shifter_pl1_pin); // 0=release, 1=press
        int raw1 = 1 ^ gpio_get_input(key_shifter_pl2_pin);
		int raw2 = 0;
		if (current_device == DEVICE_DY19) raw2 = 1 ^ gpio_get_input(key_shifter_pl3_pin);

        // Combine into mask if either P1 or P2 is active
        uint8_t button = current_shift_map[i];
		if (current_device == DEVICE_DY19) new_state |= (raw0 || raw1 || raw2) << button;
		else new_state |= (raw0 || raw1) << button;

        // Pulse clock
        gpio_set_output(key_shifter_clk_pin, 0);
        usleep(2); // KEY_SHIFTER_CLOCK_LOW_US
        gpio_set_output(key_shifter_clk_pin, 1);
        usleep(2); // KEY_SHIFTER_CLOCK_HIGH_US
    }

#ifdef LOG_BTN_PRESS
	static const char * const key_names[] = {
		"B", "Y", "Select", "Start", "Up", "Down", "Left", "Right", "A", "X", "R", "L", 
	};
	for (int i = 0; i < current_shift_map_size; i++) if (BIT16_GET(new_state,i) != BIT16_GET(btn_state,i)) {
		frontend_log_cb(RETRO_LOG_DEBUG, "JOYPAD_DRIVER" ,"poll = %s(%d) %s\n", key_names[i], i, BIT16_GET(new_state,i) ? "pressed" : "released");
	}
#endif

    btn_state = new_state;
	frontend_check_hotkeys();
}
