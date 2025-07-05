#include "../platform.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <gnu/lib-names.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

int close(int);
ssize_t read(int, void*, size_t);

static char exe_path[PATH_MAX + 1];

const char* get_current_process_exe_path(void) {
    // The command line arguments are separated by the null byte.
    // That means we will correctly be able to read up to the first token
    // of the command line which is the path of the executable.
    // Just read up to PATH_MAX bytes
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd == -1) {
        return NULL;
    }

    size_t bytes_to_read = sizeof(exe_path) - 1;
    ssize_t remaining_bytes = bytes_to_read;
    while (remaining_bytes != 0) {
        size_t buffer_offset = bytes_to_read - remaining_bytes;
        ssize_t bytes_read = read(fd, &exe_path[buffer_offset], remaining_bytes);
        if (bytes_read > 0) {
            remaining_bytes -= bytes_read;
        } else if (bytes_read == -1) {
            return NULL;
        } else if (bytes_read == 0) {
            break;
        }
    }

    close(fd);

    return exe_path;
}

void* load_libc(void) {
    return dlopen(LIBC_SO, RTLD_LAZY);
}

void* load_libc_symbol(void* libc_handle, const char* symbol) {
    return dlsym(libc_handle, symbol);
}

void close_libc(void* libc_handle) {
    dlclose(libc_handle);
}
