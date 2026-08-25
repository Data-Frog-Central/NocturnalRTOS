#ifndef SF2000_CORE_LOADING_H__
#define SF2000_CORE_LOADING_H__

#define CORE_LOAD_ADDR 0x87000000
#define CORE_LOAD_SIZE 0x01000000

extern struct retro_core_t core_api;
bool load_core(const char *file_path);

#endif // SF2000_CORE_LOADING_H__
