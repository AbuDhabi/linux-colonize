#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "platform/platform.h"

/* This module is the DOS-era asset-name shim; it owns no clock. The former
 * tick/port stubs (set_tick_rate_hz / tick_count / in_port / out_port /
 * ptr_from_segment_offset / trace_unknown) were removed: nothing read them, and
 * their hardcoded 18 Hz contradicted the real DOS clocks documented in
 * game_loop.c (IRQ0 at PIT divisor 0x7a8 = 608.77 Hz, and its /10 = 60.877 Hz).
 * If a DOS tick is ever needed, take it from those, not from here. */

void dos_compat_init(void) {
}

void dos_compat_shutdown(void) {
}

static void uppercase_copy(char* out, size_t out_size, const char* in) {
  size_t i = 0;
  if (!out || !in || out_size == 0) {
    return;
  }
  for (; in[i] != '\0' && i + 1 < out_size; ++i) {
    char c = in[i];
    if (c >= 'a' && c <= 'z') {
      c = (char)(c - ('a' - 'A'));
    }
    out[i] = c;
  }
  out[i] = '\0';
}

bool dos_compat_normalize_asset_path(
  const char* data_dir,
  const char* legacy_name,
  char* out_path,
  size_t out_path_size
) {
  struct stat st;
  char upper[256];
  if (!data_dir || !legacy_name || !out_path || out_path_size == 0) {
    return false;
  }

  snprintf(out_path, out_path_size, "%s/%s", data_dir, legacy_name);
  if (stat(out_path, &st) == 0) {
    return true;
  }

  uppercase_copy(upper, sizeof(upper), legacy_name);
  snprintf(out_path, out_path_size, "%s/%s", data_dir, upper);
  if (stat(out_path, &st) == 0) {
    return true;
  }

  /* Missing file is a normal probe failure — callers check the bool, so do not
   * log or spam stderr here. */
  return false;
}
