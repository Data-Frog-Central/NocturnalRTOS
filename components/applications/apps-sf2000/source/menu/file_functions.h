#ifndef FILE_FUNCTIONS__
#define FILE_FUNCTIONS__

#define MAX_CONTENT_INFO_OVERRIDES 3

extern void frontend_load_settings(void);
extern void frontend_config_load(void);
extern void core_config_load(void);
extern void rom_config_load(void);
extern void extract_extension(const char *filename, char **extension);
extern void extract_path_components(const char *filepath, char **dir, char **filename, char **extension);

extern bool config_get_var(struct retro_variable *var);
extern bool is_zip_wqw_file(const char* path, bool* is_wqw);
//extern bool extract_zip_file(const char *file_path, char *filename, size_t size, void **rom_buffer);
extern bool extension_supports_no_fullpath(const char *ext);

extern void save_srm(int slot);
extern void load_srm(int slot);

#endif // FILE_FUNCTIONS__
