#ifndef COLONIZE_PLATFORM_H
#define COLONIZE_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum ColonizeKey {
  COLONIZE_KEY_NONE = 0,
  COLONIZE_KEY_ESCAPE,
  COLONIZE_KEY_ENTER,
  COLONIZE_KEY_SPACE,
  COLONIZE_KEY_UP,
  COLONIZE_KEY_DOWN,
  COLONIZE_KEY_LEFT,
  COLONIZE_KEY_RIGHT,
  COLONIZE_KEY_S,
  COLONIZE_KEY_L,
  COLONIZE_KEY_Q,
  COLONIZE_KEY_P,
  COLONIZE_KEY_E,
  COLONIZE_KEY_R,
  COLONIZE_KEY_T,
  COLONIZE_KEY_D,
  COLONIZE_KEY_B,
  COLONIZE_KEY_C,
  COLONIZE_KEY_H,
  COLONIZE_KEY_O,
  COLONIZE_KEY_U,
  COLONIZE_KEY_LEFTBRACKET,
  COLONIZE_KEY_RIGHTBRACKET,
  COLONIZE_KEY_TILDE,
  COLONIZE_KEY_F1,
  COLONIZE_KEY_F2,
  COLONIZE_KEY_F3,
  COLONIZE_KEY_F4,
  COLONIZE_KEY_F5,
  COLONIZE_KEY_F6,
  COLONIZE_KEY_F7,
  COLONIZE_KEY_F8,
  COLONIZE_KEY_F9,
  COLONIZE_KEY_F10,
  COLONIZE_KEY_BACKSPACE
} ColonizeKey;

#define COLONIZE_TEXT_INPUT_MAX 16

typedef struct ColonizeInputState {
  bool quit_requested;
  bool mouse_left_down;
  bool mouse_left_clicked; /* edged: true on left button down this frame */
  bool mouse_right_down;
  bool mouse_right_clicked; /* edged: true on right button down this frame */
  int mouse_x;             /* framebuffer/logical coords (320×200 space) */
  int mouse_y;
  ColonizeKey last_key;
  char text_input[COLONIZE_TEXT_INPUT_MAX]; /* printable chars this frame */
  int text_input_len;
} ColonizeInputState;

typedef struct ColonizePalette {
  uint8_t rgb[256][3];
} ColonizePalette;

typedef struct ColonizeFramebuffer8 {
  int width;
  int height;
  uint8_t* pixels;
} ColonizeFramebuffer8;

typedef struct ColonizePlatformConfig {
  const char* data_dir;
  bool windowed;
  bool no_sound;
  int window_scale;
} ColonizePlatformConfig;

typedef struct ColonizePlatform ColonizePlatform;

ColonizePlatform* platform_create(const ColonizePlatformConfig* config);
void platform_destroy(ColonizePlatform* platform);
bool platform_poll_input(ColonizePlatform* platform, ColonizeInputState* out_input);
bool platform_present(
  ColonizePlatform* platform,
  const ColonizeFramebuffer8* framebuffer,
  const ColonizePalette* palette
);
uint32_t platform_ticks_ms(void);
void platform_sleep_ms(uint32_t ms);
void platform_set_window_title(ColonizePlatform* platform, const char* title);

/*
 * Replace the OS mouse pointer with an indexed-color sprite (0xFD = transparent),
 * scaled to the window scale. Pass NULL pixels via platform_set_mouse_cursor_default
 * to restore the system arrow. platform_show_game_mouse_cursor toggles between the
 * last built game cursor and the default without rebuilding.
 */
bool platform_set_mouse_cursor_indexed(
  ColonizePlatform* platform,
  const uint8_t* indexed_pixels,
  int width,
  int height,
  int hotspot_x,
  int hotspot_y,
  const ColonizePalette* palette
);
void platform_set_mouse_cursor_default(ColonizePlatform* platform);
void platform_show_game_mouse_cursor(ColonizePlatform* platform, bool show_game_cursor);

/* Audio: SDL device is opened paused; resume after sound_init. */
void platform_audio_resume(ColonizePlatform* platform);
bool platform_audio_enabled(const ColonizePlatform* platform);

/* DOS-compat hooks called by core logic. */
void dos_compat_init(void);
void dos_compat_shutdown(void);
void dos_compat_set_tick_rate_hz(uint32_t hz);
uint32_t dos_compat_tick_count(void);
void dos_compat_trace_unknown(const char* callsite, uint32_t code);
uint8_t dos_compat_in_port(uint16_t port);
void dos_compat_out_port(uint16_t port, uint8_t value);
void* dos_compat_ptr_from_segment_offset(uint16_t segment, uint16_t offset);
bool dos_compat_normalize_asset_path(
  const char* data_dir,
  const char* legacy_name,
  char* out_path,
  size_t out_path_size
);

#endif
