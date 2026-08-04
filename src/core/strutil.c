#include "core/strutil.h"

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
