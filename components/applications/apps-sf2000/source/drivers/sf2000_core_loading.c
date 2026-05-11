#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/unistd.h>

#include <libretro.h>
#include "sf2000_core_loading.h"
#include "../menu/menu.h"
#include "../cores/core_api.h"

void *core_buffer = (void*)CORE_LOAD_ADDR;
struct retro_core_t core_api;

void load_core(const char *core) {
    memset(core_buffer, 0, CORE_LOAD_SIZE);  // Clear the 16 MB core section
	char file_path[MAXPATH];
	snprintf(file_path, sizeof(file_path), "%s/HCRTOS/cores/%s.hcrtos", SDCARD_DIRECTORY, core);
	FILE *hfile = fopen(file_path, "rb");
	if (!hfile) {
		frontend_log_cb(RETRO_LOG_ERROR, "FRONTEND" ,"Error opening core file=%s\n", file_path);
		abort();
	}

	fseeko(hfile, 0, SEEK_END);
	long core_size = ftell(hfile);
	fseeko(hfile, 0, SEEK_SET);

	fread(core_buffer, 1, core_size, hfile);
	fclose(hfile);

	struct frontend_functions_t frontend_funcs = {
		.printf = printf,
		.frontend_log_cb = frontend_log_cb,
		._exit = _exit,
		.abort = abort,
		.malloc = malloc,
		.memset = memset,
		.stat = stat,
		.fstat = fstat,
		.kill = kill,
		.getpid = getpid,
		.gettimeofday = gettimeofday,
		.open = open,
		.close = close,
		.write = write,
		.read = read,
		.isatty = isatty,
		.lseek = lseek
	};

	core_entry_t core_entry = core_buffer;
	core_api = *core_entry(&frontend_funcs);
}