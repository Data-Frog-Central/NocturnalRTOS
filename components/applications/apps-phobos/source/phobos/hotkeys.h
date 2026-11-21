#ifndef HOTKEYS_H__
#define HOTKEYS_H__

typedef bool (*hotkey_action_t)(void);

typedef struct {
    const char *name;
    uint16_t bit;
} hotkey_button_map_t;

typedef struct {
    uint16_t *mask;
    hotkey_action_t action;
} hotkey_entry_t;

void frontend_check_hotkeys(void);
void set_hotkeys(config_file_t *config_file);

#endif // HOTKEYS_H__
