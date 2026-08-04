#ifndef COLONIZE_STRUTIL_H
#define COLONIZE_STRUTIL_H

#include <stddef.h>

/* Copy src into dst[dst_sz], always NUL-terminated; truncates if needed. */
void str_copy_trunc(char* dst, size_t dst_sz, const char* src);

/*
 * Join dir + "/" + name into dst[dst_sz] (NUL-terminated, truncating).
 * If dir is empty/NULL, copies name only.
 */
void str_path_join(char* dst, size_t dst_sz, const char* dir, const char* name);

#endif
