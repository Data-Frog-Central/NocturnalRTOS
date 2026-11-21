#ifndef NOCTURNAL_JOYPAD_H__
#define NOCTURNAL_JOYPAD_H__

#define IGNORE_BTN 255

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

extern uint16_t joypad_btn_state;
extern bool log_btn_press;

void joypad_init(const char *device_name);
void joypad_get_buttons(unsigned port, input_bits_t *state);
void frontend_input_poll_cb(void);

#endif // NOCTURNAL_JOYPAD_H__
