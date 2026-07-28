#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/game_loop.h"
#include "core/savegame.h"

struct ColonizeGameState {
  ColonizeGameConfig config;
  uint32_t turn_number;
  uint32_t elapsed_ms;
  uint8_t map_seed;
  int map_cursor_x;
  int map_cursor_y;
  bool in_menu;
  bool assets_ok;
  char status[128];
};

static void set_status(ColonizeGameState* game, const char* prefix, const char* detail) {
  if (!game || !prefix) {
    return;
  }
  if (!detail) {
    snprintf(game->status, sizeof(game->status), "%s", prefix);
    return;
  }
  snprintf(game->status, sizeof(game->status), "%.64s: %.60s", prefix, detail);
}

static void make_palette(ColonizePalette* palette) {
  for (int i = 0; i < 256; ++i) {
    palette->rgb[i][0] = (uint8_t)i;
    palette->rgb[i][1] = (uint8_t)((i * 3) & 0xff);
    palette->rgb[i][2] = (uint8_t)(255 - i);
  }
  palette->rgb[0][0] = 0;
  palette->rgb[0][1] = 0;
  palette->rgb[0][2] = 0;
  palette->rgb[255][0] = 255;
  palette->rgb[255][1] = 255;
  palette->rgb[255][2] = 255;
}

ColonizeGameState* game_create(const ColonizeGameConfig* config) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game || !config) {
    free(game);
    return NULL;
  }
  game->config = *config;
  game->turn_number = 1;
  game->map_seed = 73;
  game->map_cursor_x = 8;
  game->map_cursor_y = 8;
  game->in_menu = true;

  char err[256];
  game->assets_ok = assets_validate_required_files(config->data_dir, err, sizeof(err));
  if (!game->assets_ok) {
    set_status(game, "Asset error", err);
  } else {
    snprintf(game->status, sizeof(game->status), "Colonization Linux Port - Turn %u", game->turn_number);
  }

  dos_compat_init();
  dos_compat_set_tick_rate_hz(18);
  return game;
}

void game_destroy(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  dos_compat_shutdown();
  free(game);
}

bool game_update(ColonizeGameState* game, const ColonizeInputState* input, uint32_t dt_ms) {
  if (!game || !input) {
    return false;
  }

  game->elapsed_ms += dt_ms;
  (void)dos_compat_tick_count();

  if (input->last_key == COLONIZE_KEY_Q || input->last_key == COLONIZE_KEY_ESCAPE) {
    return false;
  }

  if (input->last_key == COLONIZE_KEY_SPACE || input->last_key == COLONIZE_KEY_ENTER) {
    if (game->in_menu) {
      game->in_menu = false;
      snprintf(game->status, sizeof(game->status), "Map view active - arrows move, S save, L load, Q quit");
      return true;
    }
    game->turn_number++;
  }

  if (input->last_key == COLONIZE_KEY_UP && game->map_cursor_y > 0) {
    game->map_cursor_y--;
  } else if (input->last_key == COLONIZE_KEY_DOWN && game->map_cursor_y < 15) {
    game->map_cursor_y++;
  } else if (input->last_key == COLONIZE_KEY_LEFT && game->map_cursor_x > 0) {
    game->map_cursor_x--;
  } else if (input->last_key == COLONIZE_KEY_RIGHT && game->map_cursor_x < 15) {
    game->map_cursor_x++;
  }

  if (input->last_key == COLONIZE_KEY_S) {
    ColonizeSavePayload payload = {
      .turn_number = game->turn_number,
      .random_seed = game->elapsed_ms,
      .map_seed = game->map_seed
    };
    char err[256];
    if (!savegame_write(game->config.save_dir, "slot1", &payload, err, sizeof(err))) {
      set_status(game, "Save failed", err);
      return true;
    }
    snprintf(game->status, sizeof(game->status), "Saved slot1 (turn %u)", game->turn_number);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_L) {
    ColonizeSavePayload payload;
    char err[256];
    if (!savegame_read(game->config.save_dir, "slot1", &payload, err, sizeof(err))) {
      set_status(game, "Load failed", err);
      return true;
    }
    game->turn_number = payload.turn_number;
    game->map_seed = payload.map_seed;
    snprintf(game->status, sizeof(game->status), "Loaded slot1 (turn %u)", game->turn_number);
    return true;
  }

  if (game->assets_ok) {
    if (game->in_menu) {
      snprintf(game->status, sizeof(game->status), "Main menu: Enter starts map | Q quits");
    } else {
      snprintf(
        game->status,
        sizeof(game->status),
        "Turn %u Cursor %d,%d",
        game->turn_number,
        game->map_cursor_x,
        game->map_cursor_y
      );
    }
  }
  return true;
}

void game_render(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette) {
  if (!game || !framebuffer || !palette || !framebuffer->pixels) {
    return;
  }
  make_palette(palette);

  if (game->in_menu) {
    memset(framebuffer->pixels, 12, (size_t)framebuffer->width * (size_t)framebuffer->height);
    for (int y = 48; y < 152; ++y) {
      for (int x = 48; x < 272; ++x) {
        framebuffer->pixels[y * framebuffer->width + x] = (uint8_t)(((x + y) & 31) + 80);
      }
    }
    return;
  }

  for (int y = 0; y < framebuffer->height; ++y) {
    for (int x = 0; x < framebuffer->width; ++x) {
      const int idx = y * framebuffer->width + x;
      uint8_t base = (uint8_t)((x + y + game->turn_number) & 0xff);
      framebuffer->pixels[idx] = (uint8_t)(base ^ game->map_seed);
    }
  }

  /* Draw a simple horizon and "map grid" style visualization. */
  for (int y = 100; y < framebuffer->height; ++y) {
    for (int x = 0; x < framebuffer->width; ++x) {
      const int idx = y * framebuffer->width + x;
      framebuffer->pixels[idx] = (uint8_t)((framebuffer->pixels[idx] / 2) + 32);
    }
  }

  /* Draw a 16x16 "tile map" grid with a highlighted cursor tile. */
  const int tile_w = framebuffer->width / 16;
  const int tile_h = framebuffer->height / 16;
  for (int gy = 0; gy < 16; ++gy) {
    for (int gx = 0; gx < 16; ++gx) {
      const bool is_cursor = (gx == game->map_cursor_x && gy == game->map_cursor_y);
      int start_x = gx * tile_w;
      int start_y = gy * tile_h;
      for (int y = start_y; y < start_y + tile_h; ++y) {
        for (int x = start_x; x < start_x + tile_w; ++x) {
          int idx = y * framebuffer->width + x;
          if (is_cursor) {
            framebuffer->pixels[idx] = (uint8_t)((framebuffer->pixels[idx] + 100) & 0xff);
          } else if (x == start_x || y == start_y) {
            framebuffer->pixels[idx] = (uint8_t)((framebuffer->pixels[idx] + 16) & 0xff);
          }
        }
      }
    }
  }
}

const char* game_status_text(const ColonizeGameState* game) {
  if (!game) {
    return "Colonization Linux Port";
  }
  return game->status;
}
