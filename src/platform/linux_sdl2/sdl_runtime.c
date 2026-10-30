#include <SDL2/SDL.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <sys/timeb.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#include "platform/diagnostics.h"
#include "platform/platform.h"
#include "core/json_min.h" /* json_write_escaped_string — input log strings */
#include "core/sound.h"
#include "core/screen_geom.h" /* SCREEN_MAX_W / SCREEN_MAX_H — resize clamp */
#include "core/ss.h" /* COLONIZE_SS_TRANSPARENT — the cursor key, platform.h:138 */

struct ColonizePlatform {
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  uint32_t* rgba_buffer;
  int width;       /* logical screen columns; grows with the window */
  int height;      /* texture / framebuffer rows, screen + log strip */
  int game_height; /* the screen part; mouse never reports below it */
  int extra_height; /* log strip rows = height - game_height */
  int window_scale;
  bool audio_enabled;
  SDL_AudioDeviceID audio_device;
  int audio_freq;
  int audio_channels;
  int last_mouse_x;
  int last_mouse_y;
  bool mouse_left_down;
  bool mouse_right_down;
  SDL_Cursor* game_cursor;
  SDL_Cursor* default_cursor;
  bool game_cursor_active;
  /* settings.json debug.input_logger (input_log_open). */
  FILE* input_log;
  int64_t input_log_unix_ms0; /* wall clock at open ... */
  uint32_t input_log_ticks0;  /* ... and SDL_GetTicks at the same instant */
  uint32_t poll_count;        /* platform_poll_input calls = game frames */
  /* platform_set_outer_palette: palette for the pixels outside the rect. */
  bool outer_on;
  ColonizePalette outer;
  int outer_x, outer_y, outer_w, outer_h;
};

static void input_log_open(ColonizePlatform* platform, const char* dir);

static void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
  ColonizePlatform* platform = (ColonizePlatform*)userdata;
  if (!platform || !stream || len <= 0) {
    return;
  }
  const int channels = platform->audio_channels > 0 ? platform->audio_channels : 1;
  const int freq = platform->audio_freq > 0 ? platform->audio_freq : 44100;
  const int frames = len / ((int)sizeof(int16_t) * channels);
  sound_render_s16((int16_t*)stream, frames, channels, freq);
}

ColonizePlatform* platform_create(const ColonizePlatformConfig* config) {
  /* Restored logical size (settings.json display.window_width/height); the DOS
   * 320x200 when unset. Fullscreen ignores it — the desktop size decides. */
  int width = SCREEN_BASE_W;
  int game_height = SCREEN_BASE_H;
  if (config && config->windowed) {
    if (config->window_width > 0) {
      width = config->window_width;
    }
    if (config->window_height > 0) {
      game_height = config->window_height;
    }
  }
  if (width < SCREEN_BASE_W) {
    width = SCREEN_BASE_W;
  }
  if (width > SCREEN_MAX_W) {
    width = SCREEN_MAX_W;
  }
  if (game_height < SCREEN_BASE_H) {
    game_height = SCREEN_BASE_H;
  }
  if (game_height > SCREEN_MAX_H) {
    game_height = SCREEN_MAX_H;
  }
  int extra = (config && config->extra_height > 0) ? config->extra_height : 0;
  /* Log strip is a windowed-mode affordance; fullscreen keeps 320x200. */
  if (!(config && config->windowed)) {
    extra = 0;
  }
  const int height = game_height + extra;
  int scale = 2;
  if (config && config->window_scale > 0) {
    scale = config->window_scale;
  }

  uint32_t sdl_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
  bool want_audio = !(config && config->no_sound);
#if defined(COLONIZE_ENABLE_AUDIO)
  if (want_audio) {
    sdl_flags |= SDL_INIT_AUDIO;
  }
#else
  want_audio = false;
#endif

  if (SDL_Init(sdl_flags) != 0) {
    diag_error("SDL_Init failed: %s", SDL_GetError());
    return NULL;
  }
  SDL_StartTextInput();
  diag_info("SDL version=%d.%d.%d audio_requested=%s",
    SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
    want_audio ? "yes" : "no");

  uint32_t flags = 0;
  if (!(config && config->windowed)) {
    flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
  } else {
    /* Resizable also gives the window manager's maximize button. */
    flags = SDL_WINDOW_RESIZABLE;
  }

  ColonizePlatform* platform = calloc(1, sizeof(*platform));
  if (!platform) {
    SDL_Quit();
    return NULL;
  }

  platform->width = width;
  platform->height = height;
  platform->game_height = game_height;
  platform->extra_height = extra;
  platform->window_scale = scale;
  platform->default_cursor = SDL_GetDefaultCursor();
  platform->window = SDL_CreateWindow(
    "OpenCol",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    width * scale,
    height * scale,
    flags
  );
  if (!platform->window) {
    diag_error("SDL_CreateWindow failed: %s", SDL_GetError());
    platform_destroy(platform);
    return NULL;
  }
  SDL_SetWindowMinimumSize(
    platform->window, SCREEN_BASE_W * scale, (SCREEN_BASE_H + extra) * scale
  );

  platform->renderer = SDL_CreateRenderer(platform->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!platform->renderer) {
    diag_warn("Accelerated renderer failed (%s); trying software.", SDL_GetError());
    platform->renderer = SDL_CreateRenderer(platform->window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!platform->renderer) {
    diag_error("SDL_CreateRenderer failed: %s", SDL_GetError());
    platform_destroy(platform);
    return NULL;
  }

  SDL_RendererInfo renderer_info;
  if (SDL_GetRendererInfo(platform->renderer, &renderer_info) == 0) {
    diag_info("SDL renderer=%s flags=0x%x max_texture=%dx%d",
      renderer_info.name ? renderer_info.name : "(unknown)",
      renderer_info.flags,
      renderer_info.max_texture_width,
      renderer_info.max_texture_height);
  }

  platform->texture = SDL_CreateTexture(
    platform->renderer,
    SDL_PIXELFORMAT_ARGB8888,
    SDL_TEXTUREACCESS_STREAMING,
    width,
    height
  );
  if (!platform->texture) {
    diag_error("SDL_CreateTexture failed: %s", SDL_GetError());
    platform_destroy(platform);
    return NULL;
  }

  Uint32 tex_format = 0;
  int tex_w = 0;
  int tex_h = 0;
  if (SDL_QueryTexture(platform->texture, &tex_format, NULL, &tex_w, &tex_h) == 0) {
    diag_info("SDL texture format=0x%x size=%dx%d logical_framebuffer=%dx%d scale=%d windowed=%s",
      tex_format, tex_w, tex_h, width, height, scale,
      (config && config->windowed) ? "yes" : "no");
  }

  platform->rgba_buffer = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
  if (!platform->rgba_buffer) {
    diag_error("Failed to allocate RGBA buffer (%dx%d)", width, height);
    platform_destroy(platform);
    return NULL;
  }

  if (config && config->data_dir) {
    diag_info("Platform data_dir=%s", config->data_dir);
  }

  platform->audio_enabled = false;
  platform->audio_device = 0;
  platform->audio_freq = 44100;
  platform->audio_channels = 2;
  if (want_audio) {
    SDL_AudioSpec want;
    SDL_AudioSpec have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = sdl_audio_callback;
    want.userdata = platform;
    platform->audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (platform->audio_device == 0) {
      diag_warn("SDL audio device unavailable (%s); continuing silent.", SDL_GetError());
    } else {
      platform->audio_enabled = true;
      platform->audio_freq = have.freq;
      platform->audio_channels = have.channels;
      diag_info(
        "SDL audio opened: freq=%d channels=%d format=0x%x (callback wired)",
        have.freq,
        have.channels,
        have.format
      );
      SDL_PauseAudioDevice(platform->audio_device, 1);
    }
  } else {
    diag_info("Audio disabled (--nosound or build without COLONIZE_ENABLE_AUDIO).");
  }

  if (config) {
    input_log_open(platform, config->input_log_dir);
  }
  return platform;
}

void platform_audio_resume(ColonizePlatform* platform) {
  if (!platform || platform->audio_device == 0 || !platform->audio_enabled) {
    return;
  }
  SDL_PauseAudioDevice(platform->audio_device, 0);
  diag_info("SDL audio resumed");
}

bool platform_audio_enabled(const ColonizePlatform* platform) {
  return platform && platform->audio_enabled;
}

void platform_destroy(ColonizePlatform* platform) {
  if (!platform) {
    return;
  }
  if (platform->game_cursor) {
    SDL_FreeCursor(platform->game_cursor);
    platform->game_cursor = NULL;
  }
  if (platform->default_cursor) {
    SDL_SetCursor(platform->default_cursor);
  }
  if (platform->audio_device != 0) {
    SDL_CloseAudioDevice(platform->audio_device);
    platform->audio_device = 0;
  }
  if (platform->input_log) {
    fclose(platform->input_log);
  }
  free(platform->rgba_buffer);
  if (platform->texture) {
    SDL_DestroyTexture(platform->texture);
  }
  if (platform->renderer) {
    SDL_DestroyRenderer(platform->renderer);
  }
  if (platform->window) {
    SDL_DestroyWindow(platform->window);
  }
  SDL_Quit();
  free(platform);
}

/*
 * The logical framebuffer is blitted 1:1 into the texture and drawn at integer
 * `window_scale`, centred in the window; the up-to-(scale-1) leftover pixels on
 * each axis stay black (letterbox) instead of stretching the pixels.
 */
static void present_dest_rect(const ColonizePlatform* platform, int out_w, int out_h, SDL_Rect* rect) {
  const int scale = platform->window_scale > 0 ? platform->window_scale : 1;
  rect->w = platform->width * scale;
  rect->h = platform->height * scale;
  rect->x = (out_w - rect->w) / 2;
  rect->y = (out_h - rect->h) / 2;
  if (rect->x < 0) {
    rect->x = 0;
  }
  if (rect->y < 0) {
    rect->y = 0;
  }
}

/* Window resized: grow/shrink the logical screen to fit, at the same integer
 * scale. Texture + RGBA buffer are rebuilt; on any failure the old size is
 * kept intact (never a half-updated platform). */
static void platform_resize(ColonizePlatform* platform, int win_w, int win_h) {
  const int scale = platform->window_scale > 0 ? platform->window_scale : 1;
  const int min_h = SCREEN_BASE_H + platform->extra_height;
  int new_w = win_w / scale;
  int new_h = win_h / scale;
  if (new_w < SCREEN_BASE_W) {
    new_w = SCREEN_BASE_W;
  }
  if (new_w > SCREEN_MAX_W) {
    new_w = SCREEN_MAX_W;
  }
  if (new_h < min_h) {
    new_h = min_h;
  }
  if (new_h > SCREEN_MAX_H + platform->extra_height) {
    new_h = SCREEN_MAX_H + platform->extra_height;
  }
  if (new_w == platform->width && new_h == platform->height) {
    return;
  }

  SDL_Texture* texture = SDL_CreateTexture(
    platform->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, new_w, new_h
  );
  if (!texture) {
    diag_warn("Resize to %dx%d: SDL_CreateTexture failed (%s); keeping %dx%d",
      new_w, new_h, SDL_GetError(), platform->width, platform->height);
    return;
  }
  uint32_t* buffer = calloc((size_t)new_w * (size_t)new_h, sizeof(uint32_t));
  if (!buffer) {
    SDL_DestroyTexture(texture);
    diag_warn("Resize to %dx%d: RGBA buffer allocation failed; keeping %dx%d",
      new_w, new_h, platform->width, platform->height);
    return;
  }

  if (platform->texture) {
    SDL_DestroyTexture(platform->texture);
  }
  free(platform->rgba_buffer);
  platform->texture = texture;
  platform->rgba_buffer = buffer;
  platform->width = new_w;
  platform->height = new_h;
  platform->game_height = new_h - platform->extra_height;
  diag_info("Window resized: logical framebuffer=%dx%d game_height=%d scale=%d",
    platform->width, platform->height, platform->game_height, scale);
}

void platform_set_outer_palette(
  ColonizePlatform* platform, const ColonizePalette* outer, int x, int y, int w, int h
) {
  if (!platform) {
    return;
  }
  platform->outer_on = outer != NULL;
  if (outer) {
    platform->outer = *outer;
    platform->outer_x = x;
    platform->outer_y = y;
    platform->outer_w = w;
    platform->outer_h = h;
  }
}

void platform_framebuffer_size(const ColonizePlatform* platform, int* out_w, int* out_h) {
  if (out_w) {
    *out_w = platform ? platform->width : SCREEN_BASE_W;
  }
  if (out_h) {
    *out_h = platform ? platform->height : SCREEN_BASE_H;
  }
}

int platform_log_strip_height(const ColonizePlatform* platform) {
  return platform ? platform->extra_height : 0;
}

static void mouse_to_logical(
  const ColonizePlatform* platform,
  int window_x,
  int window_y,
  int* out_x,
  int* out_y
) {
  const int scale = platform->window_scale > 0 ? platform->window_scale : 1;
  int ww = platform->width * scale;
  int wh = platform->height * scale;
  if (platform->window) {
    SDL_GetWindowSize(platform->window, &ww, &wh);
  }
  /* SDL reports mouse events in window points; the dest rect platform_present
   * centres lives in DRAWABLE pixels, which differ under HiDPI scaling. */
  int out_w = ww;
  int out_h = wh;
  if (platform->renderer) {
    SDL_GetRendererOutputSize(platform->renderer, &out_w, &out_h);
  }
  if (ww < 1) {
    ww = 1;
  }
  if (wh < 1) {
    wh = 1;
  }
  SDL_Rect dest;
  present_dest_rect(platform, out_w, out_h, &dest);
  int lx = (window_x * out_w / ww - dest.x) / scale;
  int ly = (window_y * out_h / wh - dest.y) / scale;
  if (lx < 0) {
    lx = 0;
  }
  if (ly < 0) {
    ly = 0;
  }
  if (lx >= platform->width) {
    lx = platform->width - 1;
  }
  /* The log strip is not part of the game screen: a pointer over it reads as
   * the bottom screen row, so map / panel hit tests never see y >= 200. */
  if (ly >= platform->game_height) {
    ly = platform->game_height - 1;
  }
  if (out_x) {
    *out_x = lx;
  }
  if (out_y) {
    *out_y = ly;
  }
}

/* ===== Input log (settings.json debug.input_logger) =====
 *
 * Port-only debug aid: every raw SDL keyboard / mouse event, one JSON object
 * per line (JSON Lines), as the event arrives — before platform_key_from_sdl or the edge
 * flags fold it. Record shape: docs/settings.md "Input log". */

static int64_t input_log_wall_ms(void) {
#ifdef _WIN32
  /* mingw msvcrt has no timespec_get. */
  struct _timeb tb;
  _ftime(&tb);
  return (int64_t)tb.time * 1000 + tb.millitm;
#else
  struct timespec ts;
  if (timespec_get(&ts, TIME_UTC) != TIME_UTC) {
    return (int64_t)time(NULL) * 1000;
  }
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

static void input_log_open(ColonizePlatform* platform, const char* dir) {
  if (!dir || !dir[0]) {
    return;
  }
  if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
    diag_warn("Input log: cannot create %s: %s", dir, strerror(errno));
    return;
  }
  platform->input_log_unix_ms0 = input_log_wall_ms();
  platform->input_log_ticks0 = SDL_GetTicks();
  const time_t now = (time_t)(platform->input_log_unix_ms0 / 1000);
  const struct tm* tm_now = localtime(&now);
  char stamp[32] = "unknown";
  if (tm_now) {
    strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", tm_now);
  }
  char path[700];
  snprintf(
    path, sizeof(path), "%s/input_%s_%03d.jsonl", dir, stamp,
    (int)(platform->input_log_unix_ms0 % 1000)
  );
  platform->input_log = fopen(path, "wbx");
  if (!platform->input_log) {
    diag_warn("Input log: cannot open %s: %s", path, strerror(errno));
    return;
  }
  fprintf(
    platform->input_log,
    "{\"type\":\"session\",\"version\":1,\"t_ms\":%u,\"unix_ms\":%lld,"
    "\"window_scale\":%d,\"screen_w\":%d,\"screen_h\":%d,\"audio\":%s}\n",
    (unsigned)platform->input_log_ticks0, (long long)platform->input_log_unix_ms0,
    platform->window_scale, platform->width, platform->game_height,
    platform->audio_device ? "true" : "false"
  );
  fflush(platform->input_log);
  diag_info("Input log: %s", path);
}

/* Opens one record; the caller appends its fields and the closing "}\n". */
static FILE* input_log_begin(ColonizePlatform* platform, const char* type, uint32_t t_ms) {
  FILE* f = platform->input_log;
  /* SDL stamps events in ms since SDL_Init; unix_ms rebases that on the wall
   * clock read at open, so both share the event's own time, not the poll's. */
  const int64_t unix_ms =
    platform->input_log_unix_ms0 + (int64_t)(int32_t)(t_ms - platform->input_log_ticks0);
  fprintf(
    f, "{\"type\":\"%s\",\"t_ms\":%u,\"unix_ms\":%lld,\"frame\":%u", type, (unsigned)t_ms,
    (long long)unix_ms, (unsigned)platform->poll_count
  );
  return f;
}

static const char* input_log_button_name(Uint8 button) {
  switch (button) {
    case SDL_BUTTON_LEFT: return "left";
    case SDL_BUTTON_MIDDLE: return "middle";
    case SDL_BUTTON_RIGHT: return "right";
    case SDL_BUTTON_X1: return "x1";
    case SDL_BUTTON_X2: return "x2";
    default: return "other";
  }
}

/* x/y = window points as SDL reports them; lx/ly = the logical screen pixel
 * the game sees (mouse_to_logical, clamped to the screen). */
static void input_log_mouse_pos(ColonizePlatform* platform, FILE* f, int x, int y) {
  int lx = 0;
  int ly = 0;
  mouse_to_logical(platform, x, y, &lx, &ly);
  fprintf(f, ",\"x\":%d,\"y\":%d,\"lx\":%d,\"ly\":%d", x, y, lx, ly);
}

/* Held buttons as SDL_BUTTON_*MASK bits: 1 left, 2 middle, 4 right, 8 x1,
 * 16 x2. A motion record with buttons != 0 is a drag step. */
static void input_log_event(ColonizePlatform* platform, const SDL_Event* event) {
  FILE* f = NULL;
  switch (event->type) {
    case SDL_MOUSEMOTION:
      if (event->motion.which == SDL_TOUCH_MOUSEID) {
        return;
      }
      f = input_log_begin(platform, "mouse_move", event->motion.timestamp);
      input_log_mouse_pos(platform, f, event->motion.x, event->motion.y);
      fprintf(
        f, ",\"dx\":%d,\"dy\":%d,\"buttons\":%u", event->motion.xrel, event->motion.yrel,
        (unsigned)event->motion.state
      );
      break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      if (event->button.which == SDL_TOUCH_MOUSEID) {
        return;
      }
      f = input_log_begin(
        platform, event->type == SDL_MOUSEBUTTONDOWN ? "mouse_down" : "mouse_up",
        event->button.timestamp
      );
      input_log_mouse_pos(platform, f, event->button.x, event->button.y);
      fprintf(
        f, ",\"button\":\"%s\",\"clicks\":%u", input_log_button_name(event->button.button),
        (unsigned)event->button.clicks
      );
      break;
    case SDL_MOUSEWHEEL:
      if (event->wheel.which == SDL_TOUCH_MOUSEID) {
        return;
      }
      f = input_log_begin(platform, "mouse_wheel", event->wheel.timestamp);
      fprintf(f, ",\"dx\":%d,\"dy\":%d", event->wheel.x, event->wheel.y);
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      const SDL_Keysym* k = &event->key.keysym;
      const char* name = SDL_GetKeyName(k->sym);
      f = input_log_begin(
        platform, event->type == SDL_KEYDOWN ? "key_down" : "key_up", event->key.timestamp
      );
      fprintf(f, ",\"key\":");
      json_write_escaped_string(f, name ? name : "", 64);
      fprintf(
        f, ",\"keycode\":%d,\"scancode\":%d,\"mod\":%u,\"repeat\":%s", (int)k->sym,
        (int)k->scancode, (unsigned)k->mod, event->key.repeat ? "true" : "false"
      );
      break;
    }
    case SDL_TEXTINPUT:
      f = input_log_begin(platform, "text", event->text.timestamp);
      fprintf(f, ",\"text\":");
      json_write_escaped_string(f, event->text.text, sizeof(event->text.text));
      break;
    case SDL_WINDOWEVENT:
      /* Focus loss synthesizes the button releases (platform_poll_input). */
      if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        f = input_log_begin(platform, "focus_lost", event->window.timestamp);
      } else if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        f = input_log_begin(platform, "focus_gained", event->window.timestamp);
      } else if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        f = input_log_begin(platform, "resize", event->window.timestamp);
        fprintf(f, ",\"w\":%d,\"h\":%d", event->window.data1, event->window.data2);
      } else {
        return;
      }
      break;
    case SDL_QUIT:
      f = input_log_begin(platform, "quit", event->quit.timestamp);
      break;
    default:
      return;
  }
  fputs("}\n", f);
}

bool platform_poll_input(ColonizePlatform* platform, ColonizeInputState* out_input) {
  if (!out_input) {
    return false;
  }

  /* Preserve edged flags that callers may have zeroed; we OR into a fresh poll. */
  out_input->text_input_len = 0;
  out_input->text_input[0] = '\0';

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (platform->input_log) {
      input_log_event(platform, &event);
    }
    switch (event.type) {
      case SDL_QUIT:
        out_input->quit_requested = true;
        break;
      case SDL_MOUSEMOTION:
        mouse_to_logical(
          platform, event.motion.x, event.motion.y, &platform->last_mouse_x, &platform->last_mouse_y
        );
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        mouse_to_logical(
          platform,
          event.button.x,
          event.button.y,
          &platform->last_mouse_x,
          &platform->last_mouse_y
        );
        if (event.button.button == SDL_BUTTON_LEFT) {
          platform->mouse_left_down = (event.type == SDL_MOUSEBUTTONDOWN);
          if (event.type == SDL_MOUSEBUTTONDOWN) {
            out_input->mouse_left_clicked = true;
          } else {
            out_input->mouse_left_released = true;
          }
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
          platform->mouse_right_down = (event.type == SDL_MOUSEBUTTONDOWN);
          if (event.type == SDL_MOUSEBUTTONDOWN) {
            out_input->mouse_right_clicked = true;
          } else {
            out_input->mouse_right_released = true;
          }
        }
        break;
      case SDL_MOUSEWHEEL:
        if (event.wheel.which != SDL_TOUCH_MOUSEID) {
          out_input->mouse_wheel_y += event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
            ? -event.wheel.y : event.wheel.y;
        }
        break;
      case SDL_KEYDOWN: {
        /* An unmapped key (Shift etc.) must not erase a mapped one from the
         * same poll. */
        const ColonizeKey k = platform_key_from_sdl(event.key.keysym.sym);
        if (k != COLONIZE_KEY_NONE) {
          out_input->last_key = k;
        }
        break;
      }
      case SDL_WINDOWEVENT:
        /*
         * port_bugs.md: after alt-tabbing away mid-press (or releasing the button
         * outside the window) SDL never delivers the BUTTONUP, so the held
         * flags stayed latched and screens driven by press/release pairs
         * (the European Status drags in particular) ate several clicks
         * before resyncing. Losing focus drops the held state and
         * synthesizes the release so drag sessions resolve.
         */
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
          if (platform->mouse_left_down) {
            out_input->mouse_left_released = true;
          }
          if (platform->mouse_right_down) {
            out_input->mouse_right_released = true;
          }
          platform->mouse_left_down = false;
          platform->mouse_right_down = false;
        } else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          platform_resize(platform, event.window.data1, event.window.data2);
        }
        break;
      case SDL_TEXTINPUT:
        /* ASCII only: every consumer (text_edit, hotkeys) is 7-bit, and the
         * lead byte of a multibyte UTF-8 char would insert garbage. */
        if (event.text.text[0] && (unsigned char)event.text.text[0] < 0x80 &&
            out_input->text_input_len + 1 < COLONIZE_TEXT_INPUT_MAX) {
          out_input->text_input[out_input->text_input_len++] = event.text.text[0];
          out_input->text_input[out_input->text_input_len] = '\0';
        }
        break;
      default:
        break;
    }
  }

  out_input->mouse_x = platform->last_mouse_x;
  out_input->mouse_y = platform->last_mouse_y;
  out_input->mouse_left_down = platform->mouse_left_down;
  out_input->mouse_right_down = platform->mouse_right_down;
  out_input->alt_held = (SDL_GetModState() & KMOD_ALT) != 0;
  out_input->shift_held = (SDL_GetModState() & KMOD_SHIFT) != 0;
  out_input->ctrl_held = (SDL_GetModState() & KMOD_CTRL) != 0;
  if (platform->input_log) {
    /* Once per frame, so a crash loses at most the current frame's events. */
    fflush(platform->input_log);
  }
  ++platform->poll_count;
  return true;
}

void platform_set_mouse_cursor_default(ColonizePlatform* platform) {
  if (!platform) {
    return;
  }
  if (platform->default_cursor) {
    SDL_SetCursor(platform->default_cursor);
  } else {
    /* Create once and keep it as the default (SDL_Quit frees cursors). */
    platform->default_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    if (platform->default_cursor) {
      SDL_SetCursor(platform->default_cursor);
    }
  }
  platform->game_cursor_active = false;
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
  if (!platform || !indexed_pixels || !palette || width <= 0 || height <= 0) {
    return false;
  }

  int scale = platform->window_scale > 0 ? platform->window_scale : 1;
  const int sw = width * scale;
  const int sh = height * scale;
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, sw, sh, 32, SDL_PIXELFORMAT_ARGB8888);
  if (!surface) {
    diag_warn("SDL_CreateRGBSurfaceWithFormat failed: %s", SDL_GetError());
    return false;
  }

  if (SDL_LockSurface(surface) != 0) {
    diag_warn("SDL_LockSurface failed: %s", SDL_GetError());
    SDL_FreeSurface(surface);
    return false;
  }

  uint32_t* dst = (uint32_t*)surface->pixels;
  const int pitch = surface->pitch / (int)sizeof(uint32_t);
  for (int y = 0; y < sh; ++y) {
    const int sy = y / scale;
    for (int x = 0; x < sw; ++x) {
      const int sx = x / scale;
      const uint8_t index = indexed_pixels[sy * width + sx];
      uint32_t pixel = 0;
      if (index != COLONIZE_SS_TRANSPARENT) {
        const uint8_t r = palette->rgb[index][0];
        const uint8_t g = palette->rgb[index][1];
        const uint8_t b = palette->rgb[index][2];
        pixel = 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
      }
      dst[y * pitch + x] = pixel;
    }
  }
  SDL_UnlockSurface(surface);

  const int hot_x = hotspot_x * scale;
  const int hot_y = hotspot_y * scale;
  SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hot_x, hot_y);
  SDL_FreeSurface(surface);
  if (!cursor) {
    diag_warn("SDL_CreateColorCursor failed: %s", SDL_GetError());
    return false;
  }

  if (platform->game_cursor) {
    SDL_FreeCursor(platform->game_cursor);
  }
  platform->game_cursor = cursor;
  SDL_SetCursor(cursor);
  platform->game_cursor_active = true;
  return true;
}

void platform_show_game_mouse_cursor(ColonizePlatform* platform, bool show_game_cursor) {
  if (!platform) {
    return;
  }
  if (show_game_cursor) {
    if (platform->game_cursor) {
      SDL_SetCursor(platform->game_cursor);
      platform->game_cursor_active = true;
    }
  } else {
    platform_set_mouse_cursor_default(platform);
  }
}

bool platform_present(
  ColonizePlatform* platform,
  const ColonizeFramebuffer8* framebuffer,
  const ColonizePalette* palette
) {
  static uint32_t present_counter = 0;
  if (!platform || !framebuffer || !palette) {
    diag_error("platform_present called with null argument(s)");
    return false;
  }

  if (!framebuffer->pixels || !platform->rgba_buffer) {
    diag_error("platform_present called without pixels");
    return false;
  }

  /*
   * `rgba_buffer` and the streaming texture are both platform->width x
   * platform->height (the current logical size), so the caller's dimensions
   * are never allowed to size this loop: sizing it from
   * framebuffer->width * framebuffer->height while the allocation came from
   * the hardcoded 320x200 was a latent heap overrun for any larger
   * framebuffer. Copy the overlap instead, keeping the destination stride at
   * the allocation's width, and say so once.
   */
  const int fb_w = framebuffer->width;
  const int fb_h = framebuffer->height;
  const int copy_w = fb_w < platform->width ? fb_w : platform->width;
  const int copy_h = fb_h < platform->height ? fb_h : platform->height;
  /* Only a width mismatch leaves stale columns; a shorter framebuffer is the
   * normal case for the nested modal pumps (their local fb is 320x200), and
   * the log strip rows below must keep the last full render's pixels. */
  if (fb_w != platform->width) {
    static bool size_mismatch_warned = false;
    if (!size_mismatch_warned) {
      size_mismatch_warned = true;
      diag_warn(
        "platform_present: framebuffer %dx%d != display %dx%d; presenting the %dx%d overlap",
        fb_w, fb_h, platform->width, platform->height, copy_w, copy_h
      );
    }
    memset(
      platform->rgba_buffer,
      0,
      (size_t)platform->width * (size_t)platform->height * sizeof(uint32_t)
    );
  }
  /* One ARGB lookup table per frame beats three palette byte loads and three
   * shifts per pixel (the conversion touches the whole window every frame). */
  uint32_t argb[256];
  for (int i = 0; i < 256; ++i) {
    argb[i] = 0xff000000u | ((uint32_t)palette->rgb[i][0] << 16) |
              ((uint32_t)palette->rgb[i][1] << 8) | (uint32_t)palette->rgb[i][2];
  }
  /* Outside the outer rect (a screen over the map) the map's palette rules. */
  uint32_t argb_outer[256];
  int in_x0 = 0, in_x1 = copy_w, in_y0 = 0, in_y1 = copy_h;
  if (platform->outer_on) {
    for (int i = 0; i < 256; ++i) {
      argb_outer[i] = 0xff000000u | ((uint32_t)platform->outer.rgb[i][0] << 16) |
                      ((uint32_t)platform->outer.rgb[i][1] << 8) | (uint32_t)platform->outer.rgb[i][2];
    }
    in_x0 = platform->outer_x;
    in_x1 = platform->outer_x + platform->outer_w;
    in_y0 = platform->outer_y;
    in_y1 = platform->outer_y + platform->outer_h;
  }
  for (int y = 0; y < copy_h; ++y) {
    const uint8_t* src_row = &framebuffer->pixels[(size_t)y * (size_t)fb_w];
    uint32_t* dst_row = &platform->rgba_buffer[(size_t)y * (size_t)platform->width];
    if (!platform->outer_on) {
      for (int x = 0; x < copy_w; ++x) {
        dst_row[x] = argb[src_row[x]];
      }
      continue;
    }
    const bool row_in = y >= in_y0 && y < in_y1;
    for (int x = 0; x < copy_w; ++x) {
      dst_row[x] = (row_in && x >= in_x0 && x < in_x1 ? argb : argb_outer)[src_row[x]];
    }
  }

  SDL_UpdateTexture(platform->texture, NULL, platform->rgba_buffer, platform->width * (int)sizeof(uint32_t));
  int out_w = platform->width * platform->window_scale;
  int out_h = platform->height * platform->window_scale;
  SDL_GetRendererOutputSize(platform->renderer, &out_w, &out_h);
  SDL_Rect dest;
  present_dest_rect(platform, out_w, out_h, &dest);
  SDL_SetRenderDrawColor(platform->renderer, 0, 0, 0, 255);
  SDL_RenderClear(platform->renderer);
  SDL_RenderCopy(platform->renderer, platform->texture, NULL, &dest);
  SDL_RenderPresent(platform->renderer);

  present_counter++;
  if (present_counter == 1 || present_counter == 120) {
    diag_info(
      "Present frame=%u fb=%dx%d sample_index=%u sample_rgba=0x%08x pitch=%d",
      present_counter,
      framebuffer->width,
      framebuffer->height,
      framebuffer->pixels[0],
      platform->rgba_buffer[0],
      platform->width * (int)sizeof(uint32_t)
    );
  }
  return true;
}

uint32_t platform_ticks_ms(void) {
  return SDL_GetTicks();
}

bool platform_clipboard_get(char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return false;
  }
  out[0] = '\0';
  if (!SDL_HasClipboardText()) {
    return false;
  }
  char* text = SDL_GetClipboardText();
  if (!text) {
    return false;
  }
  size_t n = strlen(text);
  if (n + 1 > out_size) {
    n = out_size - 1;
  }
  memcpy(out, text, n);
  out[n] = '\0';
  SDL_free(text);
  return n > 0;
}

void platform_clipboard_set(const char* text) {
  SDL_SetClipboardText(text ? text : "");
}

void platform_sleep_ms(uint32_t ms) {
  SDL_Delay(ms);
}

void platform_set_window_title(ColonizePlatform* platform, const char* title) {
  if (!platform || !platform->window || !title) {
    return;
  }
  /* Called every frame from the main loop; the title changes rarely and each
   * set is a window-manager round trip. */
  static char last[256];
  if (strncmp(last, title, sizeof(last) - 1) == 0) {
    return;
  }
  snprintf(last, sizeof(last), "%s", title);
  SDL_SetWindowTitle(platform->window, title);
}
