#include "../platform.h"
#include <dlfcn.h>
#include <libproc.h>
#include <stdio.h>

#define LIBC_PATH "/usr/lib/libSystem.B.dylib"

int getpid(void);

static char exe_path[PROC_PIDPATHINFO_MAXSIZE];

const char* get_current_process_exe_path(void) {
    pid_t pid = getpid();
    if (proc_pidpath(pid, exe_path, sizeof(exe_path)) == -1) {
        return NULL;
    }
    return exe_path;
}

void* load_libc(void) {
    return dlopen(LIBC_PATH, RTLD_LAZY);
}

void* load_libc_symbol(void* libc_handle, const char* symbol) {
    return dlsym(libc_handle, symbol);
}

void close_libc(void* libc_handle) {
    dlclose(libc_handle);
}
