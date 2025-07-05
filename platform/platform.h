#ifndef PLATFORM_H
#define PLATFORM_H

const char* get_current_process_exe_path(void);

void* load_libc(void);
void* load_libc_symbol(void* libc_handle, const char* symbol);
void close_libc(void* libc_handle);

#endif //PLATFORM_H
