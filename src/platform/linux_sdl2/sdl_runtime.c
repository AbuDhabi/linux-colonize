#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"
#include "platform/platform.h"
#include "core/sound.h"

struct ColonizePlatform {
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  uint32_t* rgba_buffer;
  int width;
  int height;
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
};

static ColonizeKey map_key(SDL_Keycode key) {
  switch (key) {
    case SDLK_ESCAPE: return COLONIZE_KEY_ESCAPE;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      return COLONIZE_KEY_ENTER;
    case SDLK_SPACE: return COLONIZE_KEY_SPACE;
    case SDLK_UP: return COLONIZE_KEY_UP;
    case SDLK_DOWN: return COLONIZE_KEY_DOWN;
    case SDLK_LEFT: return COLONIZE_KEY_LEFT;
    case SDLK_RIGHT: return COLONIZE_KEY_RIGHT;
    case SDLK_KP_1: return COLONIZE_KEY_KP1;
    case SDLK_KP_2: return COLONIZE_KEY_KP2;
    case SDLK_KP_3: return COLONIZE_KEY_KP3;
    case SDLK_KP_4: return COLONIZE_KEY_KP4;
    case SDLK_KP_6: return COLONIZE_KEY_KP6;
    case SDLK_KP_7: return COLONIZE_KEY_KP7;
    case SDLK_KP_8: return COLONIZE_KEY_KP8;
    case SDLK_KP_9: return COLONIZE_KEY_KP9;
    case SDLK_s: return COLONIZE_KEY_S;
    case SDLK_f: return COLONIZE_KEY_F;
    case SDLK_l: return COLONIZE_KEY_L;
    case SDLK_q: return COLONIZE_KEY_Q;
    case SDLK_p: return COLONIZE_KEY_P;
    case SDLK_e: return COLONIZE_KEY_E;
    case SDLK_r: return COLONIZE_KEY_R;
    case SDLK_t: return COLONIZE_KEY_T;
    case SDLK_d: return COLONIZE_KEY_D;
    case SDLK_b: return COLONIZE_KEY_B;
    case SDLK_c: return COLONIZE_KEY_C;
    case SDLK_h: return COLONIZE_KEY_H;
    case SDLK_o: return COLONIZE_KEY_O;
    case SDLK_u: return COLONIZE_KEY_U;
    case SDLK_w: return COLONIZE_KEY_W;
    case SDLK_i: return COLONIZE_KEY_I;
    case SDLK_n: return COLONIZE_KEY_N;
    case SDLK_LEFTBRACKET: return COLONIZE_KEY_LEFTBRACKET;
    case SDLK_RIGHTBRACKET: return COLONIZE_KEY_RIGHTBRACKET;
    case SDLK_BACKQUOTE: return COLONIZE_KEY_TILDE;
    case SDLK_F1: return COLONIZE_KEY_F1;
    case SDLK_F2: return COLONIZE_KEY_F2;
    case SDLK_F3: return COLONIZE_KEY_F3;
    case SDLK_F4: return COLONIZE_KEY_F4;
    case SDLK_F5: return COLONIZE_KEY_F5;
    case SDLK_F6: return COLONIZE_KEY_F6;
    case SDLK_F7: return COLONIZE_KEY_F7;
    case SDLK_F8: return COLONIZE_KEY_F8;
    case SDLK_F9: return COLONIZE_KEY_F9;
    case SDLK_F10: return COLONIZE_KEY_F10;
    case SDLK_BACKSPACE: return COLONIZE_KEY_BACKSPACE;
    default: return COLONIZE_KEY_NONE;
  }
}

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
  const int width = 320;
  const int height = 200;
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
  }

  ColonizePlatform* platform = calloc(1, sizeof(*platform));
  if (!platform) {
    SDL_Quit();
    return NULL;
  }

  platform->width = width;
  platform->height = height;
  platform->window_scale = scale;
  platform->default_cursor = SDL_GetDefaultCursor();
  platform->window = SDL_CreateWindow(
    "Colonization Linux Port",
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

static void mouse_to_logical(
  const ColonizePlatform* platform,
  int window_x,
  int window_y,
  int* out_x,
  int* out_y
) {
  int ww = platform->width;
  int wh = platform->height;
  if (platform->window) {
    SDL_GetWindowSize(platform->window, &ww, &wh);
  }
  if (ww < 1) {
    ww = 1;
  }
  if (wh < 1) {
    wh = 1;
  }
  int lx = window_x * platform->width / ww;
  int ly = window_y * platform->height / wh;
  if (lx < 0) {
    lx = 0;
  }
  if (ly < 0) {
    ly = 0;
  }
  if (lx >= platform->width) {
    lx = platform->width - 1;
  }
  if (ly >= platform->height) {
    ly = platform->height - 1;
  }
  if (out_x) {
    *out_x = lx;
  }
  if (out_y) {
    *out_y = ly;
  }
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
      case SDL_KEYDOWN:
        out_input->last_key = map_key(event.key.keysym.sym);
        break;
      case SDL_TEXTINPUT:
        if (event.text.text[0] && out_input->text_input_len + 1 < COLONIZE_TEXT_INPUT_MAX) {
          /* Take first byte of each text event (ASCII names). */
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
  return true;
}

void platform_set_mouse_cursor_default(ColonizePlatform* platform) {
  if (!platform) {
    return;
  }
  if (platform->default_cursor) {
    SDL_SetCursor(platform->default_cursor);
  } else {
    SDL_Cursor* arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    if (arrow) {
      SDL_SetCursor(arrow);
      /* Leak one system cursor if default was null — rare. */
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
      if (index != 0xFDu) {
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

  const size_t pixel_count = (size_t)framebuffer->width * (size_t)framebuffer->height;
  for (size_t i = 0; i < pixel_count; ++i) {
    uint8_t index = framebuffer->pixels[i];
    uint8_t r = palette->rgb[index][0];
    uint8_t g = palette->rgb[index][1];
    uint8_t b = palette->rgb[index][2];
    platform->rgba_buffer[i] = 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
  }

  SDL_UpdateTexture(platform->texture, NULL, platform->rgba_buffer, framebuffer->width * (int)sizeof(uint32_t));
  SDL_RenderClear(platform->renderer);
  SDL_RenderCopy(platform->renderer, platform->texture, NULL, NULL);
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
      framebuffer->width * (int)sizeof(uint32_t)
    );
  }
  return true;
}

uint32_t platform_ticks_ms(void) {
  return SDL_GetTicks();
}

void platform_sleep_ms(uint32_t ms) {
  SDL_Delay(ms);
}

void platform_set_window_title(ColonizePlatform* platform, const char* title) {
  if (!platform || !platform->window || !title) {
    return;
  }
  SDL_SetWindowTitle(platform->window, title);
}
