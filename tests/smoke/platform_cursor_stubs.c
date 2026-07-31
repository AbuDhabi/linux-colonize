#include <stdbool.h>
#include <stdint.h>

#include "platform/platform.h"

/* Smoke tests link game_loop without the SDL runtime. */
bool platform_set_mouse_cursor_indexed(
  ColonizePlatform* platform,
  const uint8_t* indexed_pixels,
  int width,
  int height,
  int hotspot_x,
  int hotspot_y,
  const ColonizePalette* palette
) {
  (void)platform;
  (void)indexed_pixels;
  (void)width;
  (void)height;
  (void)hotspot_x;
  (void)hotspot_y;
  (void)palette;
  return false;
}

void platform_set_mouse_cursor_default(ColonizePlatform* platform) {
  (void)platform;
}

void platform_show_game_mouse_cursor(ColonizePlatform* platform, bool show_game_cursor) {
  (void)platform;
  (void)show_game_cursor;
}
