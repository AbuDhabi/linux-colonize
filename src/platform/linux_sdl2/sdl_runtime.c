#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"
#include "platform/platform.h"

struct ColonizePlatform {
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  uint32_t* rgba_buffer;
  int width;
  int height;
  bool audio_enabled;
  SDL_AudioDeviceID audio_device;
};

static ColonizeKey map_key(SDL_Keycode key) {
  switch (key) {
    case SDLK_ESCAPE: return COLONIZE_KEY_ESCAPE;
    case SDLK_RETURN: return COLONIZE_KEY_ENTER;
    case SDLK_SPACE: return COLONIZE_KEY_SPACE;
    case SDLK_UP: return COLONIZE_KEY_UP;
    case SDLK_DOWN: return COLONIZE_KEY_DOWN;
    case SDLK_LEFT: return COLONIZE_KEY_LEFT;
    case SDLK_RIGHT: return COLONIZE_KEY_RIGHT;
    case SDLK_s: return COLONIZE_KEY_S;
    case SDLK_l: return COLONIZE_KEY_L;
    case SDLK_q: return COLONIZE_KEY_Q;
    case SDLK_BACKQUOTE: return COLONIZE_KEY_TILDE;
    default: return COLONIZE_KEY_NONE;
  }
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
  if (want_audio) {
    SDL_AudioSpec want;
    SDL_AudioSpec have;
    SDL_zero(want);
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    platform->audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (platform->audio_device == 0) {
      diag_warn("SDL audio device unavailable (%s); continuing silent.", SDL_GetError());
    } else {
      platform->audio_enabled = true;
      diag_info("SDL audio opened: freq=%d channels=%d format=0x%x (mixer playback not wired yet)",
        have.freq, have.channels, have.format);
      SDL_PauseAudioDevice(platform->audio_device, 1);
    }
  } else {
    diag_info("Audio disabled (--nosound or build without COLONIZE_ENABLE_AUDIO).");
  }

  return platform;
}

void platform_destroy(ColonizePlatform* platform) {
  if (!platform) {
    return;
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

bool platform_poll_input(ColonizePlatform* platform, ColonizeInputState* out_input) {
  (void)platform;
  if (!out_input) {
    return false;
  }

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        out_input->quit_requested = true;
        break;
      case SDL_MOUSEMOTION:
        out_input->mouse_x = event.motion.x;
        out_input->mouse_y = event.motion.y;
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT) {
          out_input->mouse_left_down = (event.type == SDL_MOUSEBUTTONDOWN);
        }
        break;
      case SDL_KEYDOWN:
        out_input->last_key = map_key(event.key.keysym.sym);
        break;
      default:
        break;
    }
  }
  return true;
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
