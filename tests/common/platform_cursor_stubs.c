#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/platform.h"

/* Smoke tests link game_loop without the SDL runtime. The input-replay
 * harness (tests/regression) brings its own poll / present. */
#ifndef COLONIZE_REPLAY_PLATFORM
bool platform_poll_input(ColonizePlatform* platform, ColonizeInputState* out_input) {
  (void)platform;
  if (out_input) {
    memset(out_input, 0, sizeof(*out_input));
  }
  return false;
}

bool platform_present(
  ColonizePlatform* platform,
  const ColonizeFramebuffer8* framebuffer,
  const ColonizePalette* palette
) {
  (void)platform;
  (void)framebuffer;
  (void)palette;
  return false;
}

void platform_set_outer_palette(
  ColonizePlatform* platform, const ColonizePalette* outer, int x, int y, int w, int h
) {
  (void)platform;
  (void)outer;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
}
#endif

/* A virtual clock: sleeping advances it, so time-paced loops (the piece
 * slide, the combat lunge) finish instantly and deterministically. */
static uint32_t s_stub_ticks_ms;

uint32_t platform_ticks_ms(void) {
  return s_stub_ticks_ms;
}

void platform_sleep_ms(uint32_t ms) {
  s_stub_ticks_ms += ms;
}

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

void platform_set_window_icon(ColonizePlatform* platform, const uint32_t* argb, int width, int height) {
  (void)platform;
  (void)argb;
  (void)width;
  (void)height;
}

void platform_show_game_mouse_cursor(ColonizePlatform* platform, bool show_game_cursor) {
  (void)platform;
  (void)show_game_cursor;
}
