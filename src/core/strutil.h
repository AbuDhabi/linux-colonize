#ifndef COLONIZE_STRUTIL_H
#define COLONIZE_STRUTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Copy src into dst[dst_sz], always NUL-terminated; truncates if needed. */
void str_copy_trunc(char* dst, size_t dst_sz, const char* src);

/*
 * Join dir + "/" + name into dst[dst_sz] (NUL-terminated, truncating).
 * If dir is empty/NULL, copies name only.
 */
void str_path_join(char* dst, size_t dst_sz, const char* dir, const char* name);

/*
 * Trim leading and trailing whitespace in place. Leading set is space/tab;
 * trailing set is space/tab/CR/LF. This is the union of the five private
 * copies retired on 2026-09-14 (units_trim, unit_chrome_trim, europe_trim,
 * map_menu_trim strip space/tab/CR; colony_trim also stripped LF) — every
 * caller feeds it catalog or fgets lines, where a trailing newline is noise.
 */
void str_trim(char* s);

/*
 * Read the next integer out of a comma/space-separated row, advancing
 * *cursor past it. Leading spaces, tabs and commas are skipped; returns false
 * at end of string or when no digits follow. From units.c's
 * units_parse_int_field (europe.c's europe_parse_int_field was byte-identical).
 */
bool str_next_int_field(const char** cursor, int* out);

/* Remove every character of `set` from `s` in place (markup strippers:
 * "~#" hotkey markers, "{}" emphasis braces, "{}^_" layout directives). */
void str_strip_chars(char* s, const char* set);

/* Truncate `line` at the first ';' (NAMES/LABELS/GAME.TXT comment marker). */
void str_strip_comment(char* line);

static inline int clamp_int(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

/*
 * Read a whole file into a freshly malloc'd buffer (NOT NUL-terminated).
 * On success *out_buf / *out_size are set and the caller frees *out_buf; on
 * failure a message is written to err (when err_size > 0) and false returned.
 * Replaces the "fopen, fseek END, ftell, rewind, malloc, fread, fclose"
 * block that was written out eight times across src/, tests/ and tools/.
 */
bool file_slurp(const char* path, uint8_t** out_buf, size_t* out_size, char* err, size_t err_size);

#endif
