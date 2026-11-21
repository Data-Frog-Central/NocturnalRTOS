#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libretro.h>
#include <retro_miscellaneous.h>
#include <nocturnal_joypad.h>
#include <helix/helix_logging.h>
#include <helix/nocturnal_paths.h>

#undef ARRAY_SIZE

#include <hcuapi/gpio.h>
#include <hcuapi/pinpad.h>

static pinpad_e key_shifter_clk_pin = PINPAD_L24;
static pinpad_e key_shifter_pl1_pin = PINPAD_L23;
static pinpad_e key_shifter_pl2_pin = PINPAD_L26;
static pinpad_e key_shifter_pl3_pin = PINPAD_INVALID;

static joypad_device_t current_device = DEVICE_SF2000;
static const uint8_t *current_shift_map;
static size_t current_shift_map_size; // Needed?
bool log_btn_press = false;
uint16_t joypad_btn_state;

static void set_device_map(const uint8_t *map, size_t size) {
    current_shift_map = map;
    current_shift_map_size = size;
}

static void set_key_shifter_pins(pinpad_e clk, pinpad_e p1, pinpad_e p2, pinpad_e p3) {
    key_shifter_clk_pin = clk;
    key_shifter_pl1_pin = p1;
    key_shifter_pl2_pin = p2;
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

void joypad_init(const char *device_name) {
	frontend_log_cb(RETRO_LOG_INFO, "JOYPAD_DRIVER" ,"Joypad Init.\n");
	joypad_btn_state = 0;

	if (device_name == NULL) goto fail;
	if (strcmp(device_name, "SF2000") == 0) {
		goto set_sf2000;
	} else if (strcmp(device_name, "GB300") == 0) {
		current_device = DEVICE_GB300;
		set_key_shifter_pins(PINPAD_L26, PINPAD_L27, PINPAD_L25, PINPAD_INVALID);
		set_device_map(sf2000_joypad_map, sizeof(sf2000_joypad_map) / sizeof(sf2000_joypad_map[0]));
	} else if (strcmp(device_name, "DY19") == 0) {
		current_device = DEVICE_DY19;
		set_key_shifter_pins(PINPAD_L24, PINPAD_L25, PINPAD_L26, PINPAD_L27);
		set_device_map(sf2000_joypad_map, sizeof(sf2000_joypad_map) / sizeof(sf2000_joypad_map[0]));
	} else goto fail;

	return;
	fail: // Upon fail just use SF2000 setup
		frontend_log_cb(RETRO_LOG_ERROR, "JOYPAD_DRIVER" ,"Unknown Joypad Device. Assuming SF2000.\n");
		goto set_sf2000; // Make it explicit
	set_sf2000:
		current_device = DEVICE_SF2000;
		set_key_shifter_pins(PINPAD_L24, PINPAD_L23, PINPAD_L26, PINPAD_INVALID);
		set_device_map(sf2000_joypad_map, sizeof(sf2000_joypad_map) / sizeof(sf2000_joypad_map[0]));
}

int32_t sf2000_joypad_button(unsigned port, uint16_t joykey) { // TODO: What to do with this?
	if (port != 0) return 0;

	if ((joykey == RETRO_DEVICE_ID_JOYPAD_MASK) || (joykey == 65535)) {
		//frontend_log_cb(RETRO_LOG_DEBUG, "JOYPAD_DRIVER" ,"joypad_btn_state=%u joykey=%u\n", joypad_btn_state, joykey);
		return joypad_btn_state;
	}

	//frontend_log_cb(RETRO_LOG_DEBUG, "JOYPAD_DRIVER" ,"joypad_btn_state=%u joykey=%u ret=%u\n", joypad_btn_state, joykey, (joypad_btn_state & (1 << joykey)));

	return (joypad_btn_state & (1U << joykey));
}

void joypad_get_buttons(unsigned port, input_bits_t *state) { // TODO: Add player 2
	if (port == 0) {
		BITS_COPY16_PTR(state, joypad_btn_state);
	} else {
		BIT256_CLEAR_ALL_PTR(state);
	}

	//frontend_log_cb(RETRO_LOG_DEBUG, "JOYPAD_DRIVER" ,"joypad_btn_state=%u port=%u state->data[0]=%lu\n", joypad_btn_state, port, state->data[0]);
}

void frontend_input_poll_cb(void) {
    uint16_t new_state = 0;

	if (current_device == DEVICE_GB300) { // TODO: Make this more universal
		pinmux_configure(key_shifter_clk_pin, PINMUX_L26_GPIO);
		pinmux_configure(key_shifter_pl1_pin, PINMUX_L27_GPIO);
		pinmux_configure(key_shifter_pl2_pin, PINMUX_L25_GPIO);
	}

    // Configure pins
    gpio_configure(key_shifter_clk_pin, GPIO_DIR_OUTPUT);
	gpio_set_output(key_shifter_clk_pin, (current_device == DEVICE_GB300) ? 0 : 1); // TODO: Make this more universal

    gpio_configure(key_shifter_pl1_pin, GPIO_DIR_OUTPUT);
    gpio_configure(key_shifter_pl2_pin, GPIO_DIR_OUTPUT);
	if (key_shifter_pl3_pin != PINPAD_INVALID) gpio_configure(key_shifter_pl3_pin, GPIO_DIR_OUTPUT);

	gpio_set_output(key_shifter_pl1_pin, 0);
	gpio_set_output(key_shifter_pl2_pin, 0);
	if (key_shifter_pl3_pin != PINPAD_INVALID) gpio_set_output(key_shifter_pl3_pin, 0);

    gpio_set_output(key_shifter_clk_pin, 0);
    usleep(4); // KEY_SHIFTER_LOAD

    // Set D0/D1 as inputs for serial reading
    gpio_configure(key_shifter_pl1_pin, GPIO_DIR_INPUT);
    gpio_configure(key_shifter_pl2_pin, GPIO_DIR_INPUT);
	if (key_shifter_pl3_pin != PINPAD_INVALID) gpio_configure(key_shifter_pl3_pin, GPIO_DIR_INPUT);
	usleep(4); // KEY_SHIFTER_SETTLE

    // Read 16-bit shift register
    for (size_t i = 0; i < current_shift_map_size; i++) {
        int raw0 = 1 ^ gpio_get_input(key_shifter_pl1_pin); // 0=release, 1=press
        int raw1 = 1 ^ gpio_get_input(key_shifter_pl2_pin);
		int raw2 = 0;
		if (key_shifter_pl3_pin != PINPAD_INVALID) raw2 = 1 ^ gpio_get_input(key_shifter_pl3_pin);

        // Combine into mask if either P1 or P2 is active
        uint8_t button = current_shift_map[i];
		if (button != IGNORE_BTN) {
			int pressed = (key_shifter_pl3_pin != PINPAD_INVALID) ? (raw0 || raw1 || raw2) : (raw0 || raw1);
			if (pressed) new_state |= (uint16_t)(1U << button);
		}

        // Pulse clock
        gpio_set_output(key_shifter_clk_pin, 0);
        usleep(3); // KEY_SHIFTER_CLOCK_LOW
        gpio_set_output(key_shifter_clk_pin, 1);
        usleep(3); // KEY_SHIFTER_CLOCK_HIGH
    }

	if (log_btn_press) {
		static const char * const key_names[] = {
			"BIT0", "BIT1", "BIT2", "BIT3", "BIT4", "BIT5", "BIT6", "BIT7", "BIT8", "BIT9", "BIT10", "BIT11", "BIT12", "BIT13", "BIT14", "BIT15", "BIT16",
		};
		for (size_t i = 0; i < current_shift_map_size; i++) if (BIT16_GET(new_state,i) != BIT16_GET(joypad_btn_state,i)) {
			frontend_log_cb(RETRO_LOG_DEBUG, "JOYPAD_DRIVER" ,"poll = %s(%d) %s\n", key_names[i], i, BIT16_GET(new_state,i) ? "pressed" : "released");
		}
	}

    joypad_btn_state = new_state;
}
