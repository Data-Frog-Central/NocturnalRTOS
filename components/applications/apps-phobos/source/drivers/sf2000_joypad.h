#ifndef SF2000_JOYPAD_H__
#define SF2000_JOYPAD_H__

#define HOTKEY_EXIT_MASK  ((1 << RETRO_DEVICE_ID_JOYPAD_SELECT) | (1 << RETRO_DEVICE_ID_JOYPAD_START))

typedef struct {
   uint32_t data[8];
   uint16_t analogs[8];
   uint16_t analog_buttons[16];
} input_bits_t;

typedef enum {
    DEVICE_SF2000 = 0,
    DEVICE_GB300,
    DEVICE_DY19,
} joypad_device_t;

typedef void (*hotkey_action_t)(void);

typedef struct {
    uint16_t mask;
    hotkey_action_t action;
} hotkey_entry_t;

void joypad_init(const char *device_name);
void joypad_get_buttons(unsigned port, input_bits_t *state);
void frontend_check_hotkeys(void);
void frontend_input_poll_cb(void);

#endif // SF2000_JOYPAD_H__
