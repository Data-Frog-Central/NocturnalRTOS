/*
    The purpose of this file is to wrap functions passed via frontend_functions as regular functions needed by the cores.
    Wrapping seems preferable here compared to pointers to cause the least amount of conflicts.
    More or less a replacement for lib.c from Multicore.
*/
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libretro.h>
#include "core_api.h"

char* ram_buffer = NULL;
size_t ram_buffer_size = 64 * 1024 * 1024;  // 64 MB

struct frontend_functions_t frontend_functions;

int (*xlog)(const char *format, ...) = printf;
void (*frontend_log_cb)(enum retro_log_level level, const char *tag, const char *fmt, ...) = NULL;

//      System Calls       //
__attribute__((noreturn))
void _exit(int status) {
    frontend_functions._exit(status);
    while (1) {}  // Should never return
}

__attribute__((noreturn))
void abort(void) {
    frontend_functions.abort();
    while (1) {}  // Should never return
}

// Create ram buffer for custom sbrk
bool create_ram_buffer(size_t buffer_size) {
    if (ram_buffer_size != buffer_size) ram_buffer_size == buffer_size;
    ram_buffer = (char*)frontend_functions.malloc(ram_buffer_size);
    if (ram_buffer) return true;
}

void full_cache_flush() {
	unsigned idx;

	// Index_Writeback_Inv_D
	for (idx = 0x80000000; idx <= 0x80004000; idx += 16) // all of D-cache
		asm volatile("cache 1, 0(%0); cache 1, 0(%0)" : : "r"(idx));

	asm volatile("sync 0; nop; nop");

	// Index_Invalidate_I
	for (idx = 0x80000000; idx <= 0x80004000; idx += 16) // all of I-cache
		asm volatile("cache 0, 0(%0); cache 0, 0(%0)" : : "r"(idx));

	asm volatile("nop; nop; nop; nop; nop"); // ehb may be nop on this core
}

void _flush_cache(void* start, void* end) {
    // note: params are ignored and *all* the cache is cleared instead.
	// this seems to produce the most stable behavior for running dynarec code.
    full_cache_flush();
}

// Custom sbrk from multicore
void *sbrk(ptrdiff_t incr) {
	static void *s_heap_end;
	static void *s_heap_ptr = NULL;

	if (!s_heap_ptr) {
		// TODO: better managment?
        if (!ram_buffer) {
            if (!create_ram_buffer(ram_buffer_size)) {
                // TODO: Add BSOD
                frontend_log_cb(RETRO_LOG_ERROR, "CORE_API" ,"sbrk: can't create buffer!\n");
		        //lcd_bsod("sbrk: can't create buffer!");
		        abort();
            }
            frontend_log_cb(RETRO_LOG_INFO, "CORE_API" ,"sbrk: created %u bytes buffer\n", ram_buffer_size);
        }
		s_heap_ptr = ram_buffer;
		s_heap_end = ram_buffer + ram_buffer_size;
	}

	void *curr_ptr = s_heap_ptr;
	void *new_ptr = s_heap_ptr + incr;

	if (new_ptr >= s_heap_end) {
		// TODO: Add BSOD
        frontend_log_cb(RETRO_LOG_ERROR, "CORE_API" ,"sbrk: out of memory!\n");
		//lcd_bsod("sbrk: out of memory!");
		abort();
	}

	s_heap_ptr = new_ptr;
	
	return curr_ptr;
}

int	stat(const char *path, struct stat *sbuf) {
    xlog("stat called\n");
    return frontend_functions.stat(path, sbuf);
}

int fstat(int fd, struct stat *sbuf) {
    return frontend_functions.fstat(fd, sbuf);
}

int kill(pid_t pid, int sig) {
    return frontend_functions.kill(pid, sig);
}

pid_t getpid(void) {
    return frontend_functions.getpid();
}

int gettimeofday(struct timeval *tv, void *tz) {
    return frontend_functions.gettimeofday(tv, tz);
}

//      I/O Operations    //
int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;

    // If O_CREAT flag is set, we need to handle the mode argument
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);  // Extract the mode argument
        va_end(args);
    }

    // Log before opening (optional)
    xlog("Opening file: %s with flags: %d and mode: %o\n", pathname, flags, mode);

    // Call the system 'open' function with the pathname, flags, and mode
    return frontend_functions.open(pathname, flags, mode);
}

int close(int fd) {
    return frontend_functions.close(fd);
}

int write(int fd, const void *buf, size_t count) {
    return frontend_functions.write(fd, buf, count);
}

int read(int fd, void *buf, size_t count) {
    return frontend_functions.read(fd, buf, count);
}

int isatty(int fd) {
    return frontend_functions.isatty(fd);
}

off_t lseek(int fd, off_t offset, int whence) {
    return frontend_functions.lseek(fd, offset, whence);
}
