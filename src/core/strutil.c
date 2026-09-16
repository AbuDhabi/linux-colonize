#include "core/strutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void str_copy_trunc(char* dst, size_t dst_sz, const char* src) {
  if (!dst || dst_sz == 0) {
    return;
  }
  if (!src) {
    dst[0] = '\0';
    return;
  }
  size_t i = 0;
  while (i + 1 < dst_sz && src[i] != '\0') {
    dst[i] = src[i];
    i++;
  }
  dst[i] = '\0';
}

void str_trim(char* s) {
  if (!s) {
    return;
  }
  char* start = s;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != s) {
    memmove(s, start, strlen(start) + 1);
  }
  size_t n = strlen(s);
  while (n > 0 &&
         (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
    s[--n] = '\0';
  }
}

bool str_next_int_field(const char** cursor, int* out) {
  if (!cursor || !*cursor || !out) {
    return false;
  }
  while (**cursor == ' ' || **cursor == '\t' || **cursor == ',') {
    ++(*cursor);
  }
  if (**cursor == '\0') {
    return false;
  }
  char* end = NULL;
  const long v = strtol(*cursor, &end, 10);
  if (end == *cursor) {
    return false;
  }
  *out = (int)v;
  *cursor = end;
  return true;
}

void str_strip_chars(char* s, const char* set) {
  if (!s || !set) {
    return;
  }
  char* dst = s;
  for (const char* src = s; *src; ++src) {
    if (strchr(set, *src)) {
      continue;
    }
    *dst++ = *src;
  }
  *dst = '\0';
}

void str_strip_comment(char* line) {
  if (!line) {
    return;
  }
  char* semi = strchr(line, ';');
  if (semi) {
    *semi = '\0';
  }
}

bool file_slurp(const char* path, uint8_t** out_buf, size_t* out_size, char* err, size_t err_size) {
  if (out_buf) {
    *out_buf = NULL;
  }
  if (out_size) {
    *out_size = 0;
  }
  if (!path || !out_buf || !out_size) {
    if (err && err_size) {
      snprintf(err, err_size, "file_slurp bad args");
    }
    return false;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    if (err && err_size) {
      snprintf(err, err_size, "cannot open %s", path);
    }
    return false;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    if (err && err_size) {
      snprintf(err, err_size, "seek failed for %s", path);
    }
    return false;
  }
  const long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    if (err && err_size) {
      snprintf(err, err_size, "tell failed for %s", path);
    }
    return false;
  }
  rewind(f);
  uint8_t* buf = (uint8_t*)malloc(sz > 0 ? (size_t)sz : 1u);
  if (!buf) {
    fclose(f);
    if (err && err_size) {
      snprintf(err, err_size, "oom reading %s", path);
    }
    return false;
  }
  if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
    free(buf);
    fclose(f);
    if (err && err_size) {
      snprintf(err, err_size, "short read: %s", path);
    }
    return false;
  }
  fclose(f);
  *out_buf = buf;
  *out_size = (size_t)sz;
  if (err && err_size) {
    err[0] = '\0';
  }
  return true;
}

void str_path_join(char* dst, size_t dst_sz, const char* dir, const char* name) {
  if (!dst || dst_sz == 0) {
    return;
  }
  if (!name) {
    name = "";
  }
  if (!dir || dir[0] == '\0') {
    str_copy_trunc(dst, dst_sz, name);
    return;
  }

  size_t i = 0;
  while (i + 1 < dst_sz && dir[i] != '\0') {
    dst[i] = dir[i];
    i++;
  }
  if (i + 1 < dst_sz && (i == 0 || dst[i - 1] != '/')) {
    dst[i++] = '/';
  }
  size_t j = 0;
  while (i + 1 < dst_sz && name[j] != '\0') {
    dst[i++] = name[j++];
  }
  dst[i] = '\0';
}

void str_strip_quotes(char* s) {
  if (!s || s[0] != '"') {
    return;
  }
  const size_t n = strlen(s);
  if (n >= 2 && s[n - 1] == '"') {
    memmove(s, s + 1, n - 2);
    s[n - 2] = '\0';
  }
}

char* str_split_name_row(char* line) {
  if (!line) {
    return NULL;
  }
  str_strip_comment(line);
  char* comma = strchr(line, ',');
  if (!comma) {
    return NULL;
  }
  *comma = '\0';
  str_trim(line);
  char* rest = comma + 1;
  str_trim(rest);
  return rest;
}
