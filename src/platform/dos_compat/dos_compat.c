#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "platform/platform.h"

static uint32_t g_tick_rate_hz = 18;
static uint32_t g_emulated_ticks = 0;

void dos_compat_init(void) {
  g_tick_rate_hz = 18;
  g_emulated_ticks = 0;
}

void dos_compat_shutdown(void) {
}

void dos_compat_set_tick_rate_hz(uint32_t hz) {
  if (hz == 0) {
    return;
  }
  g_tick_rate_hz = hz;
}

uint32_t dos_compat_tick_count(void) {
  /* This preserves the monotonic tick semantics expected by DOS-era loops. */
  ++g_emulated_ticks;
  return g_emulated_ticks;
}

void dos_compat_trace_unknown(const char* callsite, uint32_t code) {
  const char* where = callsite ? callsite : "unknown";
  fprintf(stderr, "[dos_compat] unresolved callsite=%s code=0x%x\n", where, code);
}

uint8_t dos_compat_in_port(uint16_t port) {
  switch (port) {
    case 0x3da: /* VGA status register */
      return 0x08;
    default:
      dos_compat_trace_unknown("in_port", port);
      return 0;
  }
}

void dos_compat_out_port(uint16_t port, uint8_t value) {
  (void)value;
  switch (port) {
    case 0x3c8:
    case 0x3c9:
    case 0x43:
    case 0x40:
      return;
    default:
      dos_compat_trace_unknown("out_port", port);
      return;
  }
}

void* dos_compat_ptr_from_segment_offset(uint16_t segment, uint16_t offset) {
  uintptr_t linear = ((uintptr_t)segment << 4) + (uintptr_t)offset;
  return (void*)linear;
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

  /* Missing file is a normal probe failure — callers check the bool. Do not
   * spam stderr via dos_compat_trace_unknown (that path is for unresolved
   * DOS port I/O stubs, not optional asset lookups). */
  return false;
}
