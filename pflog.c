#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "platform/platform.h"

#define FILTER_ENV_VAR "PFLOG_FILTER"
#define FILTER_STDOUT_ENV_VAR "PFLOG_STDOUT_FILTER"
#define FILTER_STDERR_ENV_VAR "PFLOG_STDERR_FILTER"
#define MAX_FILTER_NAME_LEN 256
#define MAX_FILTERS 256

typedef int (*ExecveFn)(const char *, const char *[], const char *[]);

typedef struct {
    char filter[MAX_FILTER_NAME_LEN];
    size_t len;
} Filter;

typedef struct {
    Filter stdout_filters[MAX_FILTERS];
    Filter stderr_filters[MAX_FILTERS];
    size_t stdout_filters_count;
    size_t stderr_filters_count;
} Filters;

static ExecveFn real_execve;
static Filters filters;

typedef struct {
    const char *filter;
    size_t len;
    size_t value_cursor;
} FilterValueIter;

static FilterValueIter FilterValueIter_create(const char *filter) {
    FilterValueIter iter;
    iter.filter = filter;
    iter.value_cursor = 0;
    iter.len = strlen(filter);
    return iter;
}

typedef struct {
    const char *value;
    size_t len;
} String;

static String String_from_filter(const Filter *filter) {
    String res;
    res.value = filter->filter;
    res.len = filter->len;
    return res;
}

static String FilterValueIter_next(FilterValueIter *iter) {
    if (iter->value_cursor >= iter->len) {
        return (String){};
    }

    for (size_t i = iter->value_cursor; i != iter->len; ++i) {
        if (iter->filter[i] == ':') {
            String res;
            res.value = &iter->filter[iter->value_cursor];
            res.len = i - iter->value_cursor;
            iter->value_cursor = i + 1;
            return res;
        }
    }

    // If we exited the loop above, we have parsed the whole filter value,
    // and we need to return the last part
    String res;
    res.value = &iter->filter[iter->value_cursor];
    res.len = iter->len - iter->value_cursor;
    iter->value_cursor = iter->len;
    return res;
}

static bool String_equal(String lhs, String rhs) {
    if (lhs.len != rhs.len) {
        return false;
    }

    return strncmp(lhs.value, rhs.value, lhs.len) == 0;
}

static void parse_filter_all(const char *filter) {
    FilterValueIter iter = FilterValueIter_create(filter);
    if (iter.len == 0) {
        return;
    }

    size_t stdout_filter_index = filters.stdout_filters_count;
    size_t stderr_filter_index = filters.stderr_filters_count;

    while (true) {
        String value = FilterValueIter_next(&iter);
        if (value.len == 0) {
            break;
        }

        if (value.len > MAX_FILTER_NAME_LEN) {
            fprintf(
                stderr,
                "Filter name %.*s exceeds maximum size of %d bytes\n",
                (int) value.len, value.value, MAX_FILTER_NAME_LEN
            );
            return;
        }

        if (stdout_filter_index >= MAX_FILTERS || stderr_filter_index >= MAX_FILTERS) {
            fprintf(stderr, "Number of filters exceeds maximum which is %d\n", MAX_FILTERS);
            return;
        }

        memcpy(filters.stdout_filters[stdout_filter_index].filter, value.value, value.len);
        filters.stdout_filters[stdout_filter_index].len = value.len;

        memcpy(filters.stderr_filters[stderr_filter_index].filter, value.value, value.len);
        filters.stderr_filters[stderr_filter_index].len = value.len;

        ++stdout_filter_index;
        ++stderr_filter_index;
    }

    filters.stdout_filters_count = stdout_filter_index;
    filters.stderr_filters_count = stderr_filter_index;
}

static void parse_filter_single(const char *filter, Filter *dest, size_t *dest_len) {
    FilterValueIter iter = FilterValueIter_create(filter);
    if (iter.len == 0) {
        return;
    }

    size_t filter_index = *dest_len;

    while (true) {
        String value = FilterValueIter_next(&iter);
        if (value.len == 0) {
            break;
        }

        if (value.len > MAX_FILTER_NAME_LEN) {
            fprintf(
                stderr,
                "Filter name %.*s exceeds maximum size of %d bytes\n",
                (int) value.len, value.value, MAX_FILTER_NAME_LEN
            );
            return;
        }

        if (filter_index >= MAX_FILTERS) {
            fprintf(stderr, "Number of filters exceeds maximum which is %d\n", MAX_FILTERS);
            return;
        }

        memcpy(dest[filter_index].filter, value.value, value.len);
        dest[filter_index].len = value.len;

        ++filter_index;
    }

    *dest_len = filter_index;
}

static String get_filename_from_path(const char *path) {
    size_t path_len = strlen(path);
    for (ssize_t i = (ssize_t)path_len - 1; i >= 0; --i) {
        if (path[i] == '/') {
            String res;
            res.value = &path[i + 1];
            res.len = path_len - i - 1;
            return res;
        }
    }

    String res;
    res.value = path;
    res.len = path_len;
    return res;
}

typedef enum {
    FilterType_None = 0,
    FilterType_Stdout = 1,
    FilterType_Stderr = 2,
    FilterType_All = 3,
} FilterType;

static FilterType get_filter_type_for_filename(String filename) {
    bool found_in_stdout = false;
    bool found_in_stderr = false;

    for (size_t i = 0U; i != filters.stdout_filters_count; ++i) {
        Filter *filter = &filters.stdout_filters[i];
        String filter_value = String_from_filter(filter);
        if (String_equal(filter_value, filename)) {
            found_in_stdout = true;
            break;
        }
    }

    for (size_t i = 0U; i != filters.stderr_filters_count; ++i) {
        Filter *filter = &filters.stderr_filters[i];
        String filter_value = String_from_filter(filter);
        if (String_equal(filter_value, filename)) {
            found_in_stderr = true;
            break;
        }
    }

    if (found_in_stdout && found_in_stderr) {
        return FilterType_All;
    } else if (found_in_stdout) {
        return FilterType_Stdout;
    } else if (found_in_stderr) {
        return FilterType_Stderr;
    } else {
        return FilterType_None;
    }
}

static void close_output_streams(FilterType filter_type) {
    switch (filter_type) {
        case FilterType_Stdout: {
            fclose(stdout);
        } break;

        case FilterType_Stderr: {
            fclose(stderr);
        } break;

        case FilterType_All: {
            fclose(stdout);
            fclose(stderr);
        } break;

        case FilterType_None: {}

        default: __builtin_unreachable();
    }
}

__attribute__((constructor))
void setup(void) {
    void* libc = load_libc();
    if (!libc) {
        fprintf(stderr, "Failed to load libc\n");
        return;
    }

    real_execve = (ExecveFn) load_libc_symbol(libc, "execve");
    if (!real_execve) {
        fprintf(stderr, "Failed to load execve from libc\n");
        close_libc(libc);
        return;
    }

    close_libc(libc);

    const char *filter = getenv(FILTER_ENV_VAR);
    if (filter) {
        parse_filter_all(filter);
    }

    const char *stdout_filter = getenv(FILTER_STDOUT_ENV_VAR);
    if (stdout_filter) {
        parse_filter_single(stdout_filter, filters.stdout_filters, &filters.stdout_filters_count);
    }

    const char *stderr_filter = getenv(FILTER_STDERR_ENV_VAR);
    if (stderr_filter) {
        parse_filter_single(stderr_filter, filters.stderr_filters, &filters.stderr_filters_count);
    }

    const char* exe_path = get_current_process_exe_path();
    String filename = get_filename_from_path(exe_path);

    FilterType filter_type = get_filter_type_for_filename(filename);
    if (filter_type != FilterType_None) {
        close_output_streams(filter_type);
    }
}

int execve(const char *filename, const char *argv[], const char *envp[]) {
    if (!real_execve) {
        // Nothing we can really do
        return -1;
    }

    String filename_string = get_filename_from_path(filename);
    FilterType filter_type = get_filter_type_for_filename(filename_string);

    if (filter_type != FilterType_None) {
        close_output_streams(filter_type);
    }

    return real_execve(filename, argv, envp);
}
