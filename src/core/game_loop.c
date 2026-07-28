#include "core/game_loop.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/map.h"
#include "core/pik.h"
#include "core/savegame.h"
#include "core/ss.h"
#include "platform/diagnostics.h"

#define MENU_MAX_OPTIONS 12

struct ColonizeGameState {
  ColonizeGameConfig config;
  char resolved_data_dir[512];
  uint32_t turn_number;
  uint32_t elapsed_ms;
  uint8_t map_seed;
  int map_cursor_x;
  int map_cursor_y;
  bool in_menu;
  bool assets_ok;
  bool palette_ok;
  ColonizePalette palette;
  ColonizeMsgCatalog messages;
  ColonizePikImage menu_bg;
  bool menu_bg_ok;
  ColonizeSpriteSheet terrain;
  ColonizeSpriteSheet phys0;
  ColonizeSpriteSheet cursor;
  bool terrain_ok;
  bool phys0_ok;
  bool cursor_ok;
  ColonizeFont menu_font;
  bool menu_font_ok;
  ColonizeWorldMap world_map;
  bool world_map_ok;
  ColonizePalette map_palette;
  bool map_palette_ok;
  bool in_debug_atlas;
  int debug_atlas_sheet; /* 0=PHYS0, 1=TERRAIN */
  int debug_atlas_scroll;
  char menu_options[MENU_MAX_OPTIONS][COLONIZE_MSG_LINE_LEN];
  int menu_option_count;
  int menu_selection;
  char status[128];
};

static const char* key_name(ColonizeKey key) {
  switch (key) {
    case COLONIZE_KEY_ESCAPE: return "ESCAPE";
    case COLONIZE_KEY_ENTER: return "ENTER";
    case COLONIZE_KEY_SPACE: return "SPACE";
    case COLONIZE_KEY_UP: return "UP";
    case COLONIZE_KEY_DOWN: return "DOWN";
    case COLONIZE_KEY_LEFT: return "LEFT";
    case COLONIZE_KEY_RIGHT: return "RIGHT";
    case COLONIZE_KEY_S: return "S";
    case COLONIZE_KEY_L: return "L";
    case COLONIZE_KEY_Q: return "Q";
    case COLONIZE_KEY_TILDE: return "TILDE";
    default: return "NONE";
  }
}

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

static void strip_hotkey_markers(char* text) {
  char* dst = text;
  for (char* src = text; *src; ++src) {
    if (*src == '~') {
      continue;
    }
    *dst++ = *src;
  }
  *dst = '\0';
}

static void load_begin_menu(ColonizeGameState* game) {
  game->menu_option_count = 0;
  game->menu_selection = 0;

  const ColonizeMsgSection* section = assets_msg_find(&game->messages, "BEGINMENU");
  if (!section) {
    diag_warn("GAME.TXT missing @BEGINMENU; using fallback menu.");
    snprintf(game->menu_options[0], sizeof(game->menu_options[0]), "Start a Game in NEW WORLD");
    snprintf(game->menu_options[1], sizeof(game->menu_options[1]), "LOAD Game");
    snprintf(game->menu_options[2], sizeof(game->menu_options[2]), "Exit");
    game->menu_option_count = 3;
    return;
  }

  bool in_options = false;
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (strcmp(line, "@options") == 0) {
      in_options = true;
      continue;
    }
    if (!in_options || line[0] == '@' || line[0] == '{') {
      continue;
    }
    if (game->menu_option_count >= MENU_MAX_OPTIONS) {
      break;
    }
    snprintf(
      game->menu_options[game->menu_option_count],
      sizeof(game->menu_options[0]),
      "%s",
      line
    );
    strip_hotkey_markers(game->menu_options[game->menu_option_count]);
    game->menu_option_count++;
  }

  diag_info("BEGINMENU loaded with %d options", game->menu_option_count);
  for (int i = 0; i < game->menu_option_count; ++i) {
    diag_info("  menu[%d]=%s", i, game->menu_options[i]);
  }
}

static void blit_map_sprite(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[sprite_index];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int ox = screen_tile_x * tile_w + (tile_w - tile->width) / 2;
  const int oy = screen_tile_y * tile_h + (tile_h - tile->height) / 2;
  ss_blit_sprite(sheet, sprite_index, framebuffer, ox, oy);
}

static const ColonizeSpriteSheet* debug_atlas_sheet(const ColonizeGameState* game) {
  if (game->debug_atlas_sheet == 1) {
    return game->terrain_ok ? &game->terrain : NULL;
  }
  return game->phys0_ok ? &game->phys0 : NULL;
}

static const char* debug_atlas_sheet_name(const ColonizeGameState* game) {
  return game->debug_atlas_sheet == 1 ? "TERRAIN" : "PHYS0";
}

static void debug_atlas_layout(int* out_cols, int* out_rows, int* out_cell_w, int* out_cell_h) {
  *out_cell_w = 20;
  *out_cell_h = 26;
  *out_cols = 320 / *out_cell_w;
  *out_rows = (200 - 12) / *out_cell_h;
}

static int debug_atlas_max_scroll(const ColonizeGameState* game) {
  const ColonizeSpriteSheet* sheet = debug_atlas_sheet(game);
  if (!sheet || sheet->sprite_count <= 0) {
    return 0;
  }
  int cols = 0;
  int rows = 0;
  int cell_w = 0;
  int cell_h = 0;
  debug_atlas_layout(&cols, &rows, &cell_w, &cell_h);
  const int visible = cols * rows;
  if (sheet->sprite_count <= visible) {
    return 0;
  }
  const int last_start = sheet->sprite_count - visible;
  const int max_row = (last_start + cols - 1) / cols;
  return max_row;
}

static void fill_fallback_palette(ColonizePalette* palette) {
  for (int i = 0; i < 256; ++i) {
    palette->rgb[i][0] = (uint8_t)i;
    palette->rgb[i][1] = (uint8_t)((i * 3) & 0xff);
    palette->rgb[i][2] = (uint8_t)(255 - i);
  }
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
  game->map_cursor_x = 29;
  game->map_cursor_y = 36;
  game->in_menu = true;

  assets_msg_init(&game->messages);

  if (!assets_resolve_data_dir(config->data_dir, game->resolved_data_dir, sizeof(game->resolved_data_dir))) {
    /* Keep resolved path even if missing so errors remain actionable. */
  }
  game->config.data_dir = game->resolved_data_dir;

  assets_log_inventory(game->resolved_data_dir);

  char err[256];
  game->assets_ok = assets_validate_required_files(game->resolved_data_dir, err, sizeof(err));
  if (!game->assets_ok) {
    set_status(game, "Asset error", err);
    diag_error("Asset validation failed: %s", err);
  } else {
    snprintf(game->status, sizeof(game->status), "Colonization Linux Port");
    diag_info("Asset validation succeeded for data_dir=%s", game->resolved_data_dir);
  }

  game->palette_ok = assets_load_palette(game->resolved_data_dir, &game->palette);
  if (!game->palette_ok) {
    fill_fallback_palette(&game->palette);
    diag_warn("Using fallback generated palette.");
  }

  char game_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "GAME.TXT", game_txt, sizeof(game_txt))) {
    if (!assets_msg_load_file(&game->messages, game_txt)) {
      diag_warn("Failed to parse GAME.TXT");
    }
  }
  load_begin_menu(game);

  game->menu_bg_ok = false;
  char pik_path[512];
  char pik_err[256];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "OPENMENU.PIK", pik_path, sizeof(pik_path))) {
    if (pik_load(pik_path, &game->menu_bg, pik_err, sizeof(pik_err))) {
      game->menu_bg_ok = true;
      if (game->menu_bg.has_palette) {
        game->palette = game->menu_bg.palette;
        game->palette_ok = true;
        diag_info("Using palette embedded in OPENMENU.PIK for menu.");
      }
    } else {
      diag_warn("Failed to load menu background OPENMENU.PIK: %s", pik_err);
    }
  }

  /* Log MADSPACK samples for bring-up. */
  static const char* packed_samples[] = {"COLONY.PIK", "CCBKGD.PIK", "BUILDING.SS"};
  for (size_t i = 0; i < sizeof(packed_samples) / sizeof(packed_samples[0]); ++i) {
    char path[512];
    char info[128];
    if (dos_compat_normalize_asset_path(game->resolved_data_dir, packed_samples[i], path, sizeof(path)) &&
        assets_detect_madspack(path, info, sizeof(info))) {
      diag_info("Packed asset %s: %s", packed_samples[i], info);
    }
  }

  char ss_path[512];
  char ss_err[256];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "TERRAIN.SS", ss_path, sizeof(ss_path))) {
    if (ss_load(ss_path, &game->terrain, ss_err, sizeof(ss_err))) {
      game->terrain_ok = true;
      if (game->terrain.has_palette) {
        game->map_palette = game->terrain.palette;
        game->map_palette_ok = true;
      }
      diag_info("Loaded terrain sheet with %d sprites", game->terrain.sprite_count);
    } else {
      diag_warn("Failed to load TERRAIN.SS: %s", ss_err);
    }
  }
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "PHYS0.SS", ss_path, sizeof(ss_path))) {
    if (ss_load(ss_path, &game->phys0, ss_err, sizeof(ss_err))) {
      game->phys0_ok = true;
      diag_info("Loaded PHYS0 overlay sheet with %d sprites", game->phys0.sprite_count);
    } else {
      diag_warn("Failed to load PHYS0.SS: %s", ss_err);
    }
  }
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "CURSOR.SS", ss_path, sizeof(ss_path))) {
    if (ss_load(ss_path, &game->cursor, ss_err, sizeof(ss_err))) {
      game->cursor_ok = true;
      diag_info("Loaded cursor sheet with %d sprites", game->cursor.sprite_count);
    } else {
      diag_warn("Failed to load CURSOR.SS: %s", ss_err);
    }
  }

  char ff_path[512];
  char ff_err[256];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "FONTSMAL.FF", ff_path, sizeof(ff_path))) {
    if (ff_load(ff_path, &game->menu_font, ff_err, sizeof(ff_err))) {
      game->menu_font_ok = true;
      diag_info(
        "Loaded menu font FONTSMAL.FF (%ux%u)",
        game->menu_font.max_width,
        game->menu_font.max_height
      );
    } else {
      diag_warn("Failed to load FONTSMAL.FF: %s", ff_err);
    }
  }

  char mp_path[512];
  char mp_err[256];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "AMER2.MP", mp_path, sizeof(mp_path))) {
    if (map_load_mp(mp_path, &game->world_map, mp_err, sizeof(mp_err))) {
      game->world_map_ok = true;
      game->map_cursor_x = game->world_map.width / 2;
      game->map_cursor_y = game->world_map.height / 2;
      diag_info(
        "Loaded world map AMER2.MP (%ux%u), cursor at %d,%d",
        game->world_map.width,
        game->world_map.height,
        game->map_cursor_x,
        game->map_cursor_y
      );
    } else {
      diag_warn("Failed to load AMER2.MP: %s", mp_err);
    }
  }

  diag_info("Game config save_dir=%s", config->save_dir ? config->save_dir : "(null)");
  dos_compat_init();
  dos_compat_set_tick_rate_hz(18);
  return game;
}

void game_destroy(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  pik_free(&game->menu_bg);
  ss_free(&game->terrain);
  ss_free(&game->phys0);
  ss_free(&game->cursor);
  ff_free(&game->menu_font);
  map_free(&game->world_map);
  assets_msg_free(&game->messages);
  dos_compat_shutdown();
  free(game);
}

static void activate_menu_selection(ColonizeGameState* game) {
  if (game->menu_selection < 0 || game->menu_selection >= game->menu_option_count) {
    return;
  }
  const char* choice = game->menu_options[game->menu_selection];
  diag_info("Menu selected: %s", choice);

  if (strstr(choice, "Exit") != NULL || strstr(choice, "DOS") != NULL) {
    game->status[0] = '\0';
    /* Signal quit via emptying and special handling in update return? Use turn_number sentinel. */
    game->elapsed_ms = UINT32_MAX;
    return;
  }

  if (strstr(choice, "LOAD") != NULL || strstr(choice, "Load") != NULL) {
    ColonizeSavePayload payload;
    char err[256];
    if (!savegame_read(game->config.save_dir, "slot1", &payload, err, sizeof(err))) {
      set_status(game, "Load failed", err);
      diag_error("Menu load failed: %s", err);
      return;
    }
    game->turn_number = payload.turn_number;
    game->map_seed = payload.map_seed;
    game->in_menu = false;
    snprintf(game->status, sizeof(game->status), "Loaded slot1 (turn %u)", game->turn_number);
    return;
  }

  /* Start / customize / America / New World -> enter map view. */
  game->in_menu = false;
  snprintf(game->status, sizeof(game->status), "Map view - arrows move, Space end turn, S/L save/load");
}

bool game_update(ColonizeGameState* game, const ColonizeInputState* input, uint32_t dt_ms) {
  if (!game || !input) {
    return false;
  }

  if (game->elapsed_ms == UINT32_MAX) {
    return false;
  }

  game->elapsed_ms += dt_ms;
  (void)dos_compat_tick_count();

  if (input->last_key != COLONIZE_KEY_NONE) {
    diag_info("Key pressed: %s (menu=%s debug=%s turn=%u cursor=%d,%d)",
      key_name(input->last_key),
      game->in_menu ? "yes" : "no",
      game->in_debug_atlas ? "yes" : "no",
      game->turn_number,
      game->map_cursor_x,
      game->map_cursor_y);
  }

  if (input->last_key == COLONIZE_KEY_Q) {
    return false;
  }

  if (game->in_debug_atlas) {
    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_TILDE) {
      game->in_debug_atlas = false;
      diag_info("Left sprite atlas debug screen.");
      return true;
    }
    if (input->last_key == COLONIZE_KEY_LEFT || input->last_key == COLONIZE_KEY_RIGHT) {
      game->debug_atlas_sheet = game->debug_atlas_sheet == 0 ? 1 : 0;
      game->debug_atlas_scroll = 0;
      return true;
    }
    if (input->last_key == COLONIZE_KEY_UP && game->debug_atlas_scroll > 0) {
      game->debug_atlas_scroll--;
    } else if (input->last_key == COLONIZE_KEY_DOWN) {
      const int max_scroll = debug_atlas_max_scroll(game);
      if (game->debug_atlas_scroll < max_scroll) {
        game->debug_atlas_scroll++;
      }
    } else if (input->last_key == COLONIZE_KEY_SPACE || input->last_key == COLONIZE_KEY_ENTER) {
      int cols = 0;
      int rows = 0;
      int cell_w = 0;
      int cell_h = 0;
      debug_atlas_layout(&cols, &rows, &cell_w, &cell_h);
      const int max_scroll = debug_atlas_max_scroll(game);
      game->debug_atlas_scroll += rows;
      if (game->debug_atlas_scroll > max_scroll) {
        game->debug_atlas_scroll = max_scroll;
      }
    }
    return true;
  }

  if (input->last_key == COLONIZE_KEY_TILDE) {
    game->in_debug_atlas = true;
    game->debug_atlas_sheet = 0;
    game->debug_atlas_scroll = 0;
    diag_info("Entered sprite atlas debug screen.");
    return true;
  }

  if (game->in_menu) {
    if (input->last_key == COLONIZE_KEY_ESCAPE) {
      return false;
    }
    if (input->last_key == COLONIZE_KEY_UP && game->menu_selection > 0) {
      game->menu_selection--;
    } else if (input->last_key == COLONIZE_KEY_DOWN &&
               game->menu_selection + 1 < game->menu_option_count) {
      game->menu_selection++;
    } else if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
      activate_menu_selection(game);
      if (game->elapsed_ms == UINT32_MAX) {
        return false;
      }
    }
    if (game->menu_option_count > 0) {
      snprintf(
        game->status,
        sizeof(game->status),
        "Menu: %.100s",
        game->menu_options[game->menu_selection]
      );
    }
    return true;
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    game->in_menu = true;
    diag_info("Returned to main menu.");
    return true;
  }

  if (input->last_key == COLONIZE_KEY_SPACE || input->last_key == COLONIZE_KEY_ENTER) {
    game->turn_number++;
    diag_info("Advanced turn to %u", game->turn_number);
  }

  const int map_max_x = game->world_map_ok ? (int)game->world_map.width - 1 : 15;
  const int map_max_y = game->world_map_ok ? (int)game->world_map.height - 1 : 15;

  if (input->last_key == COLONIZE_KEY_UP && game->map_cursor_y > 0) {
    game->map_cursor_y--;
  } else if (input->last_key == COLONIZE_KEY_DOWN && game->map_cursor_y < map_max_y) {
    game->map_cursor_y++;
  } else if (input->last_key == COLONIZE_KEY_LEFT && game->map_cursor_x > 0) {
    game->map_cursor_x--;
  } else if (input->last_key == COLONIZE_KEY_RIGHT && game->map_cursor_x < map_max_x) {
    game->map_cursor_x++;
  }

  if (input->last_key == COLONIZE_KEY_S) {
    ColonizeSavePayload payload = {
      .turn_number = game->turn_number,
      .random_seed = game->elapsed_ms,
      .map_seed = game->map_seed
    };
    char err[256];
    diag_info("Save requested: slot=slot1 save_dir=%s turn=%u",
      game->config.save_dir ? game->config.save_dir : "(null)",
      game->turn_number);
    if (!savegame_write(game->config.save_dir, "slot1", &payload, err, sizeof(err))) {
      set_status(game, "Save failed", err);
      diag_error("Save failed: %s", err);
      return true;
    }
    snprintf(game->status, sizeof(game->status), "Saved slot1 (turn %u)", game->turn_number);
    diag_info("Save succeeded for slot1 (turn %u)", game->turn_number);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_L) {
    ColonizeSavePayload payload;
    char err[256];
    diag_info("Load requested: slot=slot1 save_dir=%s",
      game->config.save_dir ? game->config.save_dir : "(null)");
    if (!savegame_read(game->config.save_dir, "slot1", &payload, err, sizeof(err))) {
      set_status(game, "Load failed", err);
      diag_error("Load failed: %s", err);
      return true;
    }
    game->turn_number = payload.turn_number;
    game->map_seed = payload.map_seed;
    snprintf(game->status, sizeof(game->status), "Loaded slot1 (turn %u)", game->turn_number);
    diag_info("Load succeeded: turn=%u map_seed=%u random_seed=%u",
      payload.turn_number, payload.map_seed, payload.random_seed);
    return true;
  }

  if (game->assets_ok) {
    snprintf(
      game->status,
      sizeof(game->status),
      "Turn %u Cursor %d,%d",
      game->turn_number,
      game->map_cursor_x,
      game->map_cursor_y
    );
  }
  return true;
}

void game_render(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette) {
  static uint32_t render_log_counter = 0;
  if (!game || !framebuffer || !palette || !framebuffer->pixels) {
    return;
  }

  *palette = (game->in_menu && !game->in_debug_atlas)
    ? game->palette
    : (game->map_palette_ok ? game->map_palette : game->palette);

  if (render_log_counter == 0) {
    diag_info(
      "Render mode=%s framebuffer=%dx%d palette=%s",
      game->in_debug_atlas ? "debug-atlas" : (game->in_menu ? "menu" : "map"),
      framebuffer->width,
      framebuffer->height,
      game->palette_ok ? "VICEROY.PAL" : "fallback"
    );
  }

  if (game->in_debug_atlas) {
    memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);
    const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
    const ColonizeSpriteSheet* sheet = debug_atlas_sheet(game);
    int cols = 0;
    int rows = 0;
    int cell_w = 0;
    int cell_h = 0;
    debug_atlas_layout(&cols, &rows, &cell_w, &cell_h);
    const int first = game->debug_atlas_scroll * cols;
    const int count = sheet ? sheet->sprite_count : 0;
    const int last = count > 0 ? (first + cols * rows - 1) : -1;
    const int shown_last = last >= count ? count - 1 : last;

    char hud[96];
    snprintf(
      hud,
      sizeof(hud),
      "%s %d-%d/%d  L/R sheet Up/Dn scroll Esc",
      debug_atlas_sheet_name(game),
      count > 0 ? first : 0,
      shown_last >= 0 ? shown_last : 0,
      count
    );
    font_draw_text(font, framebuffer, 2, 1, hud, 15);

    if (sheet) {
      for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
          const int idx = first + row * cols + col;
          if (idx < 0 || idx >= sheet->sprite_count) {
            continue;
          }
          const int ox = col * cell_w + 2;
          const int oy = 12 + row * cell_h;
          /* Checkerboard behind transparent pixels so empty sprites are visible. */
          for (int py = 0; py < 16; ++py) {
            for (int px = 0; px < 16; ++px) {
              const int dx = ox + px;
              const int dy = oy + py;
              if (dx < 0 || dy < 0 || dx >= framebuffer->width || dy >= framebuffer->height) {
                continue;
              }
              framebuffer->pixels[dy * framebuffer->width + dx] =
                (uint8_t)(((px / 4) ^ (py / 4)) & 1 ? 8 : 0);
            }
          }
          ss_blit_sprite(sheet, idx, framebuffer, ox, oy);
          char label[12];
          snprintf(label, sizeof(label), "%d", idx);
          font_draw_text(font, framebuffer, ox, oy + 17, label, 14);
        }
      }
    } else {
      font_draw_text(font, framebuffer, 40, 90, "Sheet not loaded", 12);
    }
    goto render_log_sample;
  }

  if (game->in_menu) {
    if (game->menu_bg_ok) {
      memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);
      pik_blit(&game->menu_bg, framebuffer, 0, 0);
    } else {
      memset(framebuffer->pixels, 1, (size_t)framebuffer->width * (size_t)framebuffer->height);
      for (int y = 20; y < 180; ++y) {
        for (int x = 40; x < 280; ++x) {
          framebuffer->pixels[y * framebuffer->width + x] = 0;
        }
      }
    }
    const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
    const int line_step = font ? (font->max_height + 4) : 12;
    const int option_x = 48;
    int option_y = font ? 52 : 40;

    for (int i = 0; i < game->menu_option_count; ++i) {
      int y = option_y + i * line_step;
      uint8_t color = (i == game->menu_selection) ? 14 : 15;
      if (i == game->menu_selection) {
        const int bar_h = font ? font->max_height : 8;
        for (int x = 40; x < 280; ++x) {
          if (y >= 0 && y < framebuffer->height) {
            framebuffer->pixels[y * framebuffer->width + x] = 4;
          }
          if (y + bar_h >= 0 && y + bar_h < framebuffer->height) {
            framebuffer->pixels[(y + bar_h) * framebuffer->width + x] = 4;
          }
        }
      }
      font_draw_text(font, framebuffer, option_x, y, game->menu_options[i], color);
    }
    font_draw_text(font, framebuffer, 36, 188, "Up/Down select  Enter start  Q quit", 15);
    goto render_log_sample;
  }

  /* Map view: scrollable world map with terrain sprites and cursor overlay. */
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);

  const int tile_w = 16;
  const int tile_h = 16;
  const int view_cols = framebuffer->width / tile_w;
  const int view_rows = framebuffer->height / tile_h;

  int view_x = game->map_cursor_x - view_cols / 2;
  int view_y = game->map_cursor_y - view_rows / 2;
  if (game->world_map_ok) {
    const int max_view_x = (int)game->world_map.width - view_cols;
    const int max_view_y = (int)game->world_map.height - view_rows;
    if (view_x < 0) {
      view_x = 0;
    }
    if (view_y < 0) {
      view_y = 0;
    }
    if (max_view_x > 0 && view_x > max_view_x) {
      view_x = max_view_x;
    }
    if (max_view_y > 0 && view_y > max_view_y) {
      view_y = max_view_y;
    }
  } else {
    view_x = 0;
    view_y = 0;
  }

  if (game->terrain_ok && game->terrain.sprite_count > 0) {
    for (int sy = 0; sy < view_rows; ++sy) {
      for (int sx = 0; sx < view_cols; ++sx) {
        int base_sprite;
        if (game->world_map_ok) {
          const int mx = view_x + sx;
          const int my = view_y + sy;
          if (mx < 0 || my < 0 || mx >= game->world_map.width || my >= game->world_map.height) {
            continue;
          }
          base_sprite = map_terrain_sprite_at(&game->world_map, mx, my);
        } else {
          base_sprite = (view_x + sx + view_y + sy + (int)game->map_seed) % game->terrain.sprite_count;
        }
        if (base_sprite < 0 || base_sprite >= game->terrain.sprite_count) {
          base_sprite = 0;
        }
        blit_map_sprite(&game->terrain, base_sprite, framebuffer, sx, sy, tile_w, tile_h);

        if (game->phys0_ok && game->world_map_ok) {
          const int mx = view_x + sx;
          const int my = view_y + sy;
          const int forest_sprite = map_phys0_forest_sprite_at(&game->world_map, mx, my);
          if (forest_sprite >= 0) {
            blit_map_sprite(&game->phys0, forest_sprite, framebuffer, sx, sy, tile_w, tile_h);
          }
          const int overlay_layers = map_phys0_overlay_count(&game->world_map, mx, my);
          for (int layer = 0; layer < overlay_layers; ++layer) {
            const int overlay_sprite = map_phys0_overlay_sprite_at(&game->world_map, mx, my, layer);
            if (overlay_sprite >= 0) {
              blit_map_sprite(&game->phys0, overlay_sprite, framebuffer, sx, sy, tile_w, tile_h);
            }
          }
        }
      }
    }
  } else {
    for (int y = 0; y < framebuffer->height; ++y) {
      for (int x = 0; x < framebuffer->width; ++x) {
        const int idx = y * framebuffer->width + x;
        uint8_t base = (uint8_t)(((x / 8) ^ (y / 8) ^ (int)game->turn_number) & 0x0f);
        framebuffer->pixels[idx] = (uint8_t)(16 + ((base + game->map_seed) & 0x0f));
      }
    }
  }

  if (game->cursor_ok && game->cursor.sprite_count > 0) {
    const int cx = (game->map_cursor_x - view_x) * tile_w;
    const int cy = (game->map_cursor_y - view_y) * tile_h;
    ss_blit_sprite(&game->cursor, 0, framebuffer, cx, cy);
  } else {
    const int cx0 = (game->map_cursor_x - view_x) * tile_w;
    const int cy0 = (game->map_cursor_y - view_y) * tile_h;
    for (int y = cy0; y < cy0 + tile_h; ++y) {
      for (int x = cx0; x < cx0 + tile_w; ++x) {
        if (x >= 0 && x < framebuffer->width && y >= 0 && y < framebuffer->height) {
          framebuffer->pixels[y * framebuffer->width + x] = 14;
        }
      }
    }
  }

  char hud[96];
  if (game->world_map_ok) {
    const uint8_t terrain = map_get_terrain(&game->world_map, game->map_cursor_x, game->map_cursor_y);
    snprintf(
      hud,
      sizeof(hud),
      "Turn %u  (%d,%d) t=0x%02x o=%u  Esc=menu",
      game->turn_number,
      game->map_cursor_x,
      game->map_cursor_y,
      terrain,
      map_terrain_overlay(terrain)
    );
  } else {
    snprintf(hud, sizeof(hud), "Turn %u  Esc=menu", game->turn_number);
  }
  const ColonizeFont* hud_font = game->menu_font_ok ? &game->menu_font : NULL;
  font_draw_text(hud_font, framebuffer, 4, 4, hud, 15);

render_log_sample:
  render_log_counter++;
  if (render_log_counter == 1 || render_log_counter == 60 || render_log_counter == 300) {
    const int cx = framebuffer->width / 2;
    const int cy = framebuffer->height / 2;
    const int idx_center = cy * framebuffer->width + cx;
    diag_info(
      "Framebuffer sample frame=%u mode=%s idx0=%u idx_center=%u turn=%u cursor=%d,%d",
      render_log_counter,
      game->in_debug_atlas ? "debug-atlas" : (game->in_menu ? "menu" : "map"),
      framebuffer->pixels[0],
      framebuffer->pixels[idx_center],
      game->turn_number,
      game->map_cursor_x,
      game->map_cursor_y
    );
  }
}

const char* game_status_text(const ColonizeGameState* game) {
  if (!game) {
    return "Colonization Linux Port";
  }
  return game->status;
}
