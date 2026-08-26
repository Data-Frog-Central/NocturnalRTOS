#include <errno.h>

int execve(const char *name, char * const *argv, char * const *env) {
	name = name;
	argv = argv;
	env = env;
	errno = ENOMEM;
	return -1;
}
