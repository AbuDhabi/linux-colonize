#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"

struct ColonizePlatform {
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  uint32_t* rgba_buffer;
  int width;
  int height;
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

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    return NULL;
  }

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
    platform_destroy(platform);
    return NULL;
  }

  platform->renderer = SDL_CreateRenderer(platform->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!platform->renderer) {
    platform_destroy(platform);
    return NULL;
  }

  platform->texture = SDL_CreateTexture(
    platform->renderer,
    SDL_PIXELFORMAT_ARGB8888,
    SDL_TEXTUREACCESS_STREAMING,
    width,
    height
  );
  if (!platform->texture) {
    platform_destroy(platform);
    return NULL;
  }

  platform->rgba_buffer = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
  if (!platform->rgba_buffer) {
    platform_destroy(platform);
    return NULL;
  }

  return platform;
}

void platform_destroy(ColonizePlatform* platform) {
  if (!platform) {
    return;
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
  if (!platform || !framebuffer || !palette) {
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
