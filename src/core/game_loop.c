#include "core/game_loop.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_screen.h"
#include "core/debug_atlas.h"
#include "core/europe.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/map.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/pedia.h"
#include "core/pik.h"
#include "core/pick_music.h"
#include "core/popup.h"
#include "core/reports.h"
#include "core/savegame.h"
#include "core/ss.h"
#include "core/sound.h"
#include "core/turn.h"
#include "core/ui_colors.h"
#include "core/units.h"
#include "core/version.h"
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
  int map_view_x; /* viewport center tile (may diverge from cursor while a unit is selected) */
  int map_view_y;
  bool in_menu;
  bool assets_ok;
  bool palette_ok;
  ColonizePalette palette;
  ColonizeMsgCatalog messages;
  ColonizeMsgCatalog map_menu_txt;
  MapMenuBar map_menu;
  PickMusicDialog pick_music;
  ColonizeMsgCatalog labels;
  bool labels_ok;
  MapPanel map_panel;
  bool map_panel_ok;
  ColonizeMsgCatalog pedia;
  bool pedia_ok;
  bool in_pedia;
  PediaViewMode pedia_view;
  bool pedia_return_to_list; /* article Esc → list (menu/P); F1 opens article-only */
  PediaCategory pedia_category;
  int pedia_index;
  int pedia_hover_entry;
  ColonizePikImage pedia_wood;
  bool pedia_wood_ok;
  ColonizeSpriteSheet pedia_buildings;
  bool pedia_buildings_ok;
  ColonizeSpriteSheet pedia_father;
  bool pedia_father_ok;
  int pedia_father_loaded; /* -1 or last CC index loaded */
  ColonizeReportsView reports;
  bool reports_ok;
  bool in_report;
  ColonizeReportId report_id;
  EuropeScreen europe;
  bool europe_ok;
  bool in_europe;
  ColonyScreenView colony_screen;
  bool colony_screen_ok;
  bool in_colony;
  int colony_view_id;
  ColonizePikImage menu_bg;
  bool menu_bg_ok;
  ColonizeSpriteSheet menu_opentile; /* OPENTILE.SS — title-dialog wood fill */
  bool menu_opentile_ok;
  ColonizeSpriteSheet terrain;
  ColonizeSpriteSheet phys0;
  ColonizeSpriteSheet cursor;
  ColonizeSpriteSheet unit_icons;
  bool terrain_ok;
  bool phys0_ok;
  bool cursor_ok;
  bool mouse_cursor_built; /* SDL color cursor created from CURSOR.SS #0 */
  bool unit_icons_ok;
  ColonizeFont menu_font;
  bool menu_font_ok;
  ColonizeFont intro_font; /* FONTINTR.FF — VICEROY title/dialog default */
  bool intro_font_ok;
  ColonizeFont colony_font;
  bool colony_font_ok;
  ColonizeMsgCatalog names;
  bool names_ok;
  ColonizeUnitPool units;
  bool units_ok;
  ColonizeColonyPool colonies;
  bool colonies_ok;
  ColonizeWorldMap world_map;
  bool world_map_ok;
  ColonizeCol1Save col1;
  bool col1_ok;
  uint16_t game_year;
  uint16_t game_autumn;
  int human_nation;
  int active_turn_nation; /* whose turn box color (FUN_1984_00aa) */
  ColonizeTurnProcessor turn_proc;
  ColonizePalette map_palette;
  bool map_palette_ok;
  bool in_debug_atlas;
  DebugAtlas debug_atlas;
  char menu_options[MENU_MAX_OPTIONS][COLONIZE_MSG_LINE_LEN];
  int menu_option_count;
  int menu_selection;
  int menu_dialog_width; /* @BEGINMENU @width (default 160) */
  int menu_dialog_y; /* @BEGINMENU @y (default 91) */
  bool menu_smallfont; /* @BEGINMENU @smallfont → FONTTINY */
  char menu_version_line[COLONIZE_MSG_LINE_LEN];
  /* @COLORS indices remapped into OPENMENU.PIK palette (match WOODPANL RGB). */
  uint8_t menu_col_basic;
  uint8_t menu_col_hilite;
  uint8_t menu_col_select;
  ColonizePopupColors menu_popup_colors;
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
    case COLONIZE_KEY_P: return "P";
    case COLONIZE_KEY_E: return "E";
    case COLONIZE_KEY_R: return "R";
    case COLONIZE_KEY_T: return "T";
    case COLONIZE_KEY_D: return "D";
    case COLONIZE_KEY_B: return "B";
    case COLONIZE_KEY_C: return "C";
    case COLONIZE_KEY_H: return "H";
    case COLONIZE_KEY_O: return "O";
    case COLONIZE_KEY_U: return "U";
    case COLONIZE_KEY_LEFTBRACKET: return "[";
    case COLONIZE_KEY_RIGHTBRACKET: return "]";
    case COLONIZE_KEY_TILDE: return "TILDE";
    case COLONIZE_KEY_F1: return "F1";
    case COLONIZE_KEY_F2: return "F2";
    case COLONIZE_KEY_F3: return "F3";
    case COLONIZE_KEY_F4: return "F4";
    case COLONIZE_KEY_F5: return "F5";
    case COLONIZE_KEY_F6: return "F6";
    case COLONIZE_KEY_F7: return "F7";
    case COLONIZE_KEY_F8: return "F8";
    case COLONIZE_KEY_F9: return "F9";
    case COLONIZE_KEY_F10: return "F10";
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

/* Nearest palette index to an 8-bit RGB triple (for cross-PIK @COLORS remap). */
static uint8_t palette_nearest_rgb(const ColonizePalette* pal, int r, int g, int b) {
  if (!pal) {
    return 0;
  }
  int best = 0;
  int best_d = 1 << 30;
  for (int i = 0; i < 256; ++i) {
    const int dr = r - (int)pal->rgb[i][0];
    const int dg = g - (int)pal->rgb[i][1];
    const int db = b - (int)pal->rgb[i][2];
    const int d = dr * dr + dg * dg + db * db;
    if (d < best_d) {
      best_d = d;
      best = i;
      if (d == 0) {
        break;
      }
    }
  }
  return (uint8_t)best;
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
  game->menu_dialog_width = 160;
  game->menu_dialog_y = 91;
  game->menu_smallfont = false;
  /* Title dialog version line: COLONIZATION in emphasis, then Linux port tag. */
  snprintf(
    game->menu_version_line,
    sizeof(game->menu_version_line),
    "{COLONIZATION} Linux Port %s",
    COLONIZE_VERSION_STRING
  );

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
    if (!in_options) {
      if (strncmp(line, "@width=", 7) == 0) {
        game->menu_dialog_width = atoi(line + 7);
        if (game->menu_dialog_width < 40) {
          game->menu_dialog_width = 40;
        }
        if (game->menu_dialog_width > 320) {
          game->menu_dialog_width = 320;
        }
        continue;
      }
      if (strncmp(line, "@y=", 3) == 0) {
        game->menu_dialog_y = atoi(line + 3);
        continue;
      }
      if (strcmp(line, "@smallfont") == 0) {
        game->menu_smallfont = true;
        continue;
      }
      /* Skip GAME.TXT version/prompt lines; we supply our own above. */
      continue;
    }
    if (line[0] == '@' || line[0] == '\0') {
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

  diag_info(
    "BEGINMENU loaded with %d options (width=%d y=%d smallfont=%d)",
    game->menu_option_count,
    game->menu_dialog_width,
    game->menu_dialog_y,
    game->menu_smallfont ? 1 : 0
  );
  diag_info("  version=%s", game->menu_version_line);
  for (int i = 0; i < game->menu_option_count; ++i) {
    diag_info("  menu[%d]=%s", i, game->menu_options[i]);
  }
}

static void blit_map_sprite_offset(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  int pixel_ox,
  int pixel_oy
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[sprite_index];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int ox = origin_x + screen_tile_x * tile_w + pixel_ox;
  const int oy = origin_y + screen_tile_y * tile_h + pixel_oy;
  ss_blit_sprite(sheet, sprite_index, framebuffer, ox, oy);
}

static void blit_map_sprite_where_dest(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  uint8_t match_color
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[sprite_index];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int cox = (tile_w - tile->width) / 2;
  const int coy = (tile_h - tile->height) / 2;
  const int ox = origin_x + screen_tile_x * tile_w + cox;
  const int oy = origin_y + screen_tile_y * tile_h + coy;
  ss_blit_sprite_where_dest(sheet, sprite_index, framebuffer, ox, oy, match_color);
}

static void blit_map_sprite(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[sprite_index];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int cox = (tile_w - tile->width) / 2;
  const int coy = (tile_h - tile->height) / 2;
  blit_map_sprite_offset(
    sheet,
    sprite_index,
    framebuffer,
    screen_tile_x,
    screen_tile_y,
    tile_w,
    tile_h,
    origin_x,
    origin_y,
    cox,
    coy
  );
}

static const char* render_mode_name(const ColonizeGameState* game) {
  if (game->in_report) {
    return "report";
  }
  if (game->in_pedia) {
    return "pedia";
  }
  if (game->in_europe) {
    return "europe";
  }
  if (game->in_colony) {
    return "colony";
  }
  if (game->in_debug_atlas) {
    return "debug-atlas";
  }
  if (game->in_menu) {
    return "menu";
  }
  return "map";
}

static bool game_apply_col1_save(ColonizeGameState* game, ColonizeCol1Save* loaded, char* err, size_t err_size) {
  ColonizeCol1BridgeResult result;
  europe_reset_campaign(&game->europe);
  game->europe.harbor_ships = 0;
  game->europe.dock_count = 0;
  if (!col1_bridge_apply(
        loaded,
        &game->world_map,
        &game->units,
        &game->colonies,
        &game->europe,
        &result,
        err,
        err_size
      )) {
    return false;
  }
  game->world_map_ok = true;
  game->turn_number = result.turn_number;
  game->game_year = result.year;
  game->game_autumn = result.autumn;
  game->human_nation = result.human_nation;
  game->active_turn_nation = result.human_nation;
  game->map_cursor_x = result.cursor_x;
  game->map_cursor_y = result.cursor_y;
  game->map_view_x = result.cursor_x;
  game->map_view_y = result.cursor_y;
  game->in_menu = false;
  game->in_europe = false;
  game->in_colony = false;
  game->in_pedia = false;
  game->in_report = false;
  game->in_debug_atlas = false;
  game->colony_view_id = -1;

  col1_save_free(&game->col1);
  game->col1 = *loaded;
  memset(loaded, 0, sizeof(*loaded));
  game->col1_ok = true;
  {
    ColonizeSoundOptions opts = sound_get_options();
    opts.background_music = game->col1.head.tut2.background_music != 0;
    opts.event_music = game->col1.head.tut2.event_music != 0;
    opts.sound_effects = game->col1.head.tut2.sound_effects != 0;
    sound_set_options(opts);
  }
  sound_set_bgm(1);
  return true;
}

static bool game_load_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size) {
  ColonizeCol1Save loaded;
  col1_save_init(&loaded);
  if (!savegame_read_col1(game->config.save_dir, slot, &loaded, err, err_size)) {
    /* Developer convenience: fall back to repo original_saves/COLONY00.SAV. */
    char fallback[640];
    snprintf(fallback, sizeof(fallback), "original_saves/COLONY%02d.SAV", slot);
    if (!col1_save_read_file(fallback, &loaded, err, err_size)) {
      col1_save_free(&loaded);
      return false;
    }
    diag_info("Loaded fallback save %s", fallback);
  }
  if (!game_apply_col1_save(game, &loaded, err, err_size)) {
    col1_save_free(&loaded);
    return false;
  }
  return true;
}

static bool game_save_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size) {
  if (!game->world_map_ok) {
    snprintf(err, err_size, "no map loaded");
    return false;
  }
  if (!game->col1_ok) {
    if (!col1_bridge_init_template(
          &game->col1,
          game->world_map.width,
          game->world_map.height,
          err,
          err_size
        )) {
      return false;
    }
    game->col1_ok = true;
    if (game->game_year == 0) {
      game->game_year = 1492;
    }
  }
  if (!col1_bridge_capture(
        &game->col1,
        &game->world_map,
        &game->units,
        &game->colonies,
        &game->europe,
        game->game_year,
        game->game_autumn,
        game->turn_number,
        game->human_nation,
        game->map_cursor_x,
        game->map_cursor_y,
        game->units.selected_id,
        err,
        err_size
      )) {
    return false;
  }
  return savegame_write_col1(game->config.save_dir, slot, &game->col1, err, err_size);
}

static void game_open_report(ColonizeGameState* game, ColonizeReportId id) {
  if (!game) {
    return;
  }
  game->in_report = true;
  game->report_id = id;
  game->in_pedia = false;
  game->in_europe = false;
  game->in_colony = false;
  game->in_debug_atlas = false;
  snprintf(game->status, sizeof(game->status), "%s", reports_title(id));
  diag_info("Opened report %s (%s)", reports_title(id), reports_background_name(id));
}

/* Open Colonizopedia list / article. */
static void game_pedia_ensure_father_sheet(ColonizeGameState* game, int father_index);

static const ColonizeFont* game_pedia_font(const ColonizeGameState* game) {
  if (game->colony_font_ok) {
    return &game->colony_font;
  }
  if (game->menu_font_ok) {
    return &game->menu_font;
  }
  return NULL;
}

static void game_pedia_enter_shell(ColonizeGameState* game) {
  game->in_pedia = true;
  game->in_report = false;
  game->in_europe = false;
  game->in_colony = false;
  game->in_debug_atlas = false;
}

static void game_open_pedia_list(ColonizeGameState* game, PediaCategory category) {
  if (!game) {
    return;
  }
  game_pedia_enter_shell(game);
  game->pedia_view = PEDIA_VIEW_LIST;
  game->pedia_return_to_list = true;
  game->pedia_category = category;
  game->pedia_index = 0;
  game->pedia_hover_entry = -1;
  snprintf(game->status, sizeof(game->status), "%s", pedia_category_label(category));
  diag_info("Opened Colonizopedia list (%s)", pedia_category_label(category));
}

static void game_open_pedia_article(
  ColonizeGameState* game,
  PediaCategory category,
  int index,
  bool return_to_list
) {
  if (!game) {
    return;
  }
  game_pedia_enter_shell(game);
  game->pedia_view = PEDIA_VIEW_ARTICLE;
  game->pedia_return_to_list = return_to_list;
  game->pedia_category = category;
  const int count = pedia_category_count(category);
  if (index < 0) {
    index = 0;
  }
  if (count > 0 && index >= count) {
    index = count - 1;
  }
  game->pedia_index = index;
  game->pedia_hover_entry = -1;
  snprintf(game->status, sizeof(game->status), "%s", pedia_category_label(category));
  {
    PediaPage page;
    pedia_page(
      game->pedia_ok ? &game->pedia : NULL,
      game->names_ok ? &game->names : NULL,
      game->pedia_category,
      game->pedia_index,
      &page
    );
    if (page.preview_kind == PEDIA_PREVIEW_FATHER) {
      game_pedia_ensure_father_sheet(game, page.father_index);
    }
  }
}

/* F1 / REPORTS → Terrain Information: article for the tile under the cursor. */
static void game_open_terrain_pedia_at_cursor(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  int index = 0;
  if (game->world_map_ok) {
    index = map_pedia_terrain_index_at(&game->world_map, game->map_cursor_x, game->map_cursor_y);
  }
  game_open_pedia_article(game, PEDIA_CAT_TERRAIN, index, false);
  snprintf(game->status, sizeof(game->status), "Terrain Information");
  diag_info(
    "Opened Terrain Information pedia index=%d at cursor (%d,%d)",
    index,
    game->map_cursor_x,
    game->map_cursor_y
  );
}

static void game_pedia_ensure_father_sheet(ColonizeGameState* game, int father_index) {
  if (!game || father_index < 0 || father_index >= PEDIA_FATHER_COUNT) {
    return;
  }
  if (game->pedia_father_ok && game->pedia_father_loaded == father_index) {
    return;
  }
  if (game->pedia_father_ok) {
    ss_free(&game->pedia_father);
    game->pedia_father_ok = false;
  }
  game->pedia_father_loaded = -1;
  char name[32];
  char path[512];
  char err[128];
  snprintf(name, sizeof(name), "CC-%02d.SS", father_index);
  if (!dos_compat_normalize_asset_path(game->resolved_data_dir, name, path, sizeof(path))) {
    return;
  }
  if (ss_load(path, &game->pedia_father, err, sizeof(err))) {
    game->pedia_father_ok = true;
    game->pedia_father_loaded = father_index;
  }
}

static void game_handle_report_fkey(ColonizeGameState* game, ColonizeKey key) {
  if (!game || key < COLONIZE_KEY_F1 || key > COLONIZE_KEY_F10) {
    return;
  }
  const int fnum = (int)(key - COLONIZE_KEY_F1) + 1;
  if (fnum == 1) {
    game_open_terrain_pedia_at_cursor(game);
    return;
  }
  ColonizeReportId id;
  if (reports_id_from_fkey(fnum, &id)) {
    game_open_report(game, id);
  }
}

static void blit_pedia_preview_tile(
  const ColonizeGameState* game,
  const PediaTerrainPreview* preview,
  ColonizeFramebuffer8* framebuffer,
  int pixel_x,
  int pixel_y
) {
  if (!preview) {
    return;
  }
  if (game->terrain_ok && preview->terrain_sprite >= 0) {
    ss_blit_sprite(&game->terrain, preview->terrain_sprite, framebuffer, pixel_x, pixel_y);
  }
  if (game->phys0_ok) {
    for (int i = 0; i < preview->phys0_count; ++i) {
      ss_blit_sprite(&game->phys0, preview->phys0_sprites[i], framebuffer, pixel_x, pixel_y);
    }
  }
}

static void render_pedia_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer) {
  const ColonizeFont* font = game_pedia_font(game);

  if (game->pedia_view == PEDIA_VIEW_LIST) {
    pedia_list_render(
      game->pedia_ok ? &game->pedia : NULL,
      game->names_ok ? &game->names : NULL,
      game->pedia_category,
      game->pedia_wood_ok ? &game->pedia_wood : NULL,
      font,
      game->pedia_hover_entry,
      framebuffer
    );
    return;
  }

  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);

  PediaPage page;
  pedia_page(
    game->pedia_ok ? &game->pedia : NULL,
    game->names_ok ? &game->names : NULL,
    game->pedia_category,
    game->pedia_index,
    &page
  );

  char hud[128];
  snprintf(
    hud,
    sizeof(hud),
    "%s  %d/%d  L/R change  Esc %s",
    page.category_label,
    page.flat_index,
    page.flat_count > 0 ? page.flat_count - 1 : 0,
    game->pedia_return_to_list ? "list" : "exit"
  );
  font_draw_text(font, framebuffer, 2, 2, hud, 15);
  font_draw_text(font, framebuffer, 2, 12, page.title, 14);

  const int preview_x = 8;
  const int preview_y = 28;
  int text_x = 120;

  if (page.preview_kind == PEDIA_PREVIEW_TERRAIN) {
    const int tile = 16;
    const int grid = 3;
    text_x = preview_x + grid * tile + 12;
    for (int gy = 0; gy < grid; ++gy) {
      for (int gx = 0; gx < grid; ++gx) {
        const int px = preview_x + gx * tile;
        const int py = preview_y + gy * tile;
        for (int y = 0; y < tile; ++y) {
          for (int x = 0; x < tile; ++x) {
            const int dx = px + x;
            const int dy = py + y;
            if (dx < 0 || dy < 0 || dx >= framebuffer->width || dy >= framebuffer->height) {
              continue;
            }
            framebuffer->pixels[dy * framebuffer->width + dx] =
              (uint8_t)(((x / 4) ^ (y / 4)) & 1 ? 8 : 0);
          }
        }
        blit_pedia_preview_tile(game, &page.terrain, framebuffer, px, py);
      }
    }
  } else if (page.preview_kind == PEDIA_PREVIEW_ICON && page.icon_sprite >= 0 && game->unit_icons_ok) {
    ss_blit_sprite(&game->unit_icons, page.icon_sprite, framebuffer, preview_x, preview_y);
    text_x = preview_x + 48;
  } else if (
    page.preview_kind == PEDIA_PREVIEW_BUILDING && page.building_sprite >= 0 && game->pedia_buildings_ok
  ) {
    ss_blit_sprite(
      &game->pedia_buildings, page.building_sprite, framebuffer, preview_x, preview_y
    );
    text_x = preview_x + 72;
  } else if (page.preview_kind == PEDIA_PREVIEW_FATHER && page.father_index >= 0) {
    /* Father sheet may be loaded lazily by the update path; try current sheet. */
    if (game->pedia_father_ok && game->pedia_father_loaded == page.father_index &&
        game->pedia_father.sprite_count > 0) {
      ss_blit_sprite(&game->pedia_father, 0, framebuffer, preview_x, preview_y);
      text_x = preview_x + 80;
    }
  }

  const int line_step = font ? (font->max_height + 2) : 10;
  int text_y = preview_y;
  for (int i = 0; i < page.body_line_count; ++i) {
    font_draw_text(font, framebuffer, text_x, text_y, page.body[i], 15);
    text_y += line_step;
    if (text_y > framebuffer->height - 12) {
      break;
    }
  }

  if (!game->pedia_ok) {
    font_draw_text(font, framebuffer, text_x, text_y + 4, "(PEDIA.TXT not loaded)", 12);
  }
}

static void render_europe_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer) {
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);
  const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
  const EuropeScreen* eu = &game->europe;

  if (game->europe_ok && eu->background_ok) {
    pik_blit(&eu->background, framebuffer, 0, 0);
  }

  char line[96];
  snprintf(
    line,
    sizeof(line),
    "Europe — %s, %s",
    eu->port_city,
    eu->nation_name
  );
  font_draw_text(font, framebuffer, 4, 2, line, 15);

  snprintf(line, sizeof(line), "Gold %d$   Tax %d%%   Ships %d", eu->gold, eu->tax_percent, eu->harbor_ships);
  font_draw_text(font, framebuffer, 4, 12, line, 14);

  font_draw_text(font, framebuffer, 4, 28, "Docks", 14);
  int y = 38;
  if (eu->dock_count == 0) {
    font_draw_text(font, framebuffer, 8, y, "(empty)", 12);
    y += 10;
  } else {
    for (int i = 0; i < eu->dock_count; ++i) {
      snprintf(line, sizeof(line), "%d. %s", i + 1, eu->dock[i].name);
      font_draw_text(font, framebuffer, 8, y, line, 15);
      y += 9;
      if (y > 88) {
        break;
      }
    }
  }

  font_draw_text(font, framebuffer, 4, 100, "Harbor", 14);
  y = 110;
  if (eu->harbor_ships == 0) {
    font_draw_text(font, framebuffer, 8, y, "(empty)", 12);
  } else {
    for (int i = 0; i < eu->harbor_ships; ++i) {
      if (eu->harbor[i].cargo_count > 0) {
        snprintf(
          line,
          sizeof(line),
          "%d. %s (+%d)",
          i + 1,
          eu->harbor[i].name,
          eu->harbor[i].cargo_count
        );
      } else {
        snprintf(line, sizeof(line), "%d. %s", i + 1, eu->harbor[i].name);
      }
      font_draw_text(font, framebuffer, 8, y, line, 15);
      y += 9;
      if (y > 155) {
        break;
      }
    }
  }

  font_draw_text(font, framebuffer, 170, 28, "Market (bid/ask)", 14);
  y = 38;
  const int market_rows = eu->cargo_count > 12 ? 12 : eu->cargo_count;
  for (int i = 0; i < market_rows; ++i) {
    snprintf(
      line,
      sizeof(line),
      "%-11s %3d/%3d",
      eu->cargo[i].name,
      eu->cargo[i].bid,
      eu->cargo[i].ask
    );
    font_draw_text(font, framebuffer, 170, y, line, 15);
    y += 9;
  }

  font_draw_text(font, framebuffer, 4, 168, eu->status, 14);
  font_draw_text(font, framebuffer, 4, 180, "R Recruit  T Train  S Sail  ] +1000$  [ tax-  Esc", 12);
  if (!game->europe_ok) {
    font_draw_text(font, framebuffer, 4, 100, "EUROPE.PIK / NAMES.TXT failed to load", 12);
  }
}

static void render_colony_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer) {
  const ColonizeColony* colony = colonies_get(&game->colonies, game->colony_view_id);
  const ColonizeFont* font = game->colony_font_ok
    ? &game->colony_font
    : (game->menu_font_ok ? &game->menu_font : NULL);
  colony_screen_render(
    game->colony_screen_ok ? &game->colony_screen : NULL,
    &game->colonies,
    colony,
    game->units_ok ? &game->units : NULL,
    game->world_map_ok ? &game->world_map : NULL,
    game->terrain_ok ? &game->terrain : NULL,
    game->phys0_ok ? &game->phys0 : NULL,
    game->game_year,
    game->game_autumn,
    game->europe.gold,
    font,
    framebuffer
  );
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
  game->colony_view_id = -1;
  game->map_cursor_x = 29;
  game->map_cursor_y = 36;
  game->map_view_x = 29;
  game->map_view_y = 36;
  game->in_menu = true;
  game->game_year = 1492;
  game->game_autumn = 0;
  game->human_nation = 0;
  game->active_turn_nation = 0;
  game->pedia_category = PEDIA_CAT_TERRAIN;
  game->pedia_index = 0;
  game->pedia_hover_entry = -1;
  game->pedia_view = PEDIA_VIEW_LIST;
  game->pedia_return_to_list = false;
  game->pedia_father_loaded = -1;
  col1_save_init(&game->col1);
  game->col1_ok = false;

  assets_msg_init(&game->messages);
  assets_msg_init(&game->map_menu_txt);
  assets_msg_init(&game->labels);
  assets_msg_init(&game->pedia);
  assets_msg_init(&game->names);
  map_menu_init(&game->map_menu);
  pick_music_init(&game->pick_music);
  units_reset(&game->units);
  colonies_init(&game->colonies);
  debug_atlas_init(&game->debug_atlas);

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

  char menu_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "MENU.TXT", menu_txt, sizeof(menu_txt))) {
    if (assets_msg_load_file(&game->map_menu_txt, menu_txt)) {
      map_menu_load(&game->map_menu, &game->map_menu_txt);
    } else {
      diag_warn("Failed to parse MENU.TXT");
    }
  }

  char labels_txt[512];
  game->labels_ok = false;
  if (dos_compat_normalize_asset_path(
        game->resolved_data_dir, "LABELS.TXT", labels_txt, sizeof(labels_txt)
      )) {
    if (assets_msg_load_file(&game->labels, labels_txt)) {
      game->labels_ok = true;
      diag_info("Loaded LABELS.TXT");
    } else {
      diag_warn("Failed to parse LABELS.TXT");
    }
  }

  game->map_panel_ok = map_panel_load(
    &game->map_panel,
    game->resolved_data_dir,
    game->labels_ok ? &game->labels : NULL
  );
  if (game->map_panel_ok) {
    diag_info("Loaded map right panel (WOODTILE/NAMEPLAT)");
  } else {
    diag_warn("Map right panel assets incomplete");
  }

  char names_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "NAMES.TXT", names_txt, sizeof(names_txt))) {
    if (assets_msg_load_file(&game->names, names_txt)) {
      game->names_ok = true;
      game->units_ok = units_load_types(&game->units, &game->names);
      if (!colonies_load_buildings(&game->colonies, &game->names)) {
        diag_warn("Failed to load @BUILDING from NAMES.TXT");
      }
    } else {
      diag_warn("Failed to parse NAMES.TXT");
    }
  }

  char colony_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "COLONY.TXT", colony_txt, sizeof(colony_txt))) {
    game->colonies_ok = colonies_load_names(&game->colonies, colony_txt);
  }

  char pedia_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "PEDIA.TXT", pedia_txt, sizeof(pedia_txt))) {
    if (assets_msg_load_file(&game->pedia, pedia_txt)) {
      game->pedia_ok = true;
      diag_info("Loaded Colonizopedia text from PEDIA.TXT");
    } else {
      diag_warn("Failed to parse PEDIA.TXT");
    }
  }

  game->menu_bg_ok = false;
  game->pedia_wood_ok = false;
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

  game->menu_opentile_ok = false;
  {
    char ss_path_ot[512];
    char ss_err_ot[256];
    if (dos_compat_normalize_asset_path(
          game->resolved_data_dir, "OPENTILE.SS", ss_path_ot, sizeof(ss_path_ot)
        )) {
      if (ss_load(ss_path_ot, &game->menu_opentile, ss_err_ot, sizeof(ss_err_ot))) {
        game->menu_opentile_ok = true;
        diag_info(
          "Loaded title dialog wood OPENTILE.SS (%d sprites)",
          game->menu_opentile.sprite_count
        );
      } else {
        diag_warn("Failed to load OPENTILE.SS: %s", ss_err_ot);
      }
    }
  }
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "WOODPANL.PIK", pik_path, sizeof(pik_path))) {
    if (pik_load(pik_path, &game->pedia_wood, pik_err, sizeof(pik_err))) {
      game->pedia_wood_ok = true;
      diag_info(
        "Loaded Colonizopedia wood panel WOODPANL.PIK (%dx%d)",
        game->pedia_wood.width,
        game->pedia_wood.height
      );
    } else {
      diag_warn("Failed to load WOODPANL.PIK for Colonizopedia: %s", pik_err);
    }
  }

  /* OPENMENU.PIK embeds a different palette than WOODPANL; map @COLORS by RGB so
   * title-menu text matches Colonizopedia link green (basic=68 → often openmenu 254). */
  game->menu_col_basic = COLONIZE_COL_BASIC;
  game->menu_col_hilite = COLONIZE_COL_HILITE;
  game->menu_col_select = COLONIZE_COL_SELECT;
  popup_colors_from_ui(&game->menu_popup_colors);
  if (game->menu_bg_ok && game->menu_bg.has_palette && game->pedia_wood_ok &&
      game->pedia_wood.has_palette) {
    game->menu_col_basic = palette_nearest_rgb(
      &game->menu_bg.palette,
      game->pedia_wood.palette.rgb[COLONIZE_COL_BASIC][0],
      game->pedia_wood.palette.rgb[COLONIZE_COL_BASIC][1],
      game->pedia_wood.palette.rgb[COLONIZE_COL_BASIC][2]
    );
    game->menu_col_hilite = palette_nearest_rgb(
      &game->menu_bg.palette,
      game->pedia_wood.palette.rgb[COLONIZE_COL_HILITE][0],
      game->pedia_wood.palette.rgb[COLONIZE_COL_HILITE][1],
      game->pedia_wood.palette.rgb[COLONIZE_COL_HILITE][2]
    );
    game->menu_col_select = palette_nearest_rgb(
      &game->menu_bg.palette,
      game->pedia_wood.palette.rgb[COLONIZE_COL_SELECT][0],
      game->pedia_wood.palette.rgb[COLONIZE_COL_SELECT][1],
      game->pedia_wood.palette.rgb[COLONIZE_COL_SELECT][2]
    );
    popup_colors_remap(
      &game->menu_popup_colors, &game->pedia_wood.palette, &game->menu_bg.palette
    );
    diag_info(
      "Title menu @COLORS remap: basic %u→%u hilite %u→%u select %u→%u "
      "popup mid/light/dark %u/%u/%u",
      (unsigned)COLONIZE_COL_BASIC,
      (unsigned)game->menu_col_basic,
      (unsigned)COLONIZE_COL_HILITE,
      (unsigned)game->menu_col_hilite,
      (unsigned)COLONIZE_COL_SELECT,
      (unsigned)game->menu_col_select,
      (unsigned)game->menu_popup_colors.mid,
      (unsigned)game->menu_popup_colors.light,
      (unsigned)game->menu_popup_colors.dark
    );
  }

  /* Log MADSPACK samples for bring-up. */
  static const char* packed_samples[] = {"WOODPANL.PIK", "COLONY.PIK", "BUILDING.SS"};
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
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "ICONS.SS", ss_path, sizeof(ss_path))) {
    if (ss_load(ss_path, &game->unit_icons, ss_err, sizeof(ss_err))) {
      game->unit_icons_ok = true;
      diag_info("Loaded unit icon sheet with %d sprites", game->unit_icons.sprite_count);
    } else {
      diag_warn("Failed to load ICONS.SS: %s", ss_err);
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
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "FONTINTR.FF", ff_path, sizeof(ff_path))) {
    if (ff_load(ff_path, &game->intro_font, ff_err, sizeof(ff_err))) {
      game->intro_font_ok = true;
      diag_info(
        "Loaded intro font FONTINTR.FF (%ux%u)",
        game->intro_font.max_width,
        game->intro_font.max_height
      );
    } else {
      diag_warn("Failed to load FONTINTR.FF: %s", ff_err);
    }
  }
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "FONTTINY.FF", ff_path, sizeof(ff_path))) {
    if (ff_load(ff_path, &game->colony_font, ff_err, sizeof(ff_err))) {
      game->colony_font_ok = true;
      diag_info(
        "Loaded colony font FONTTINY.FF (%ux%u)",
        game->colony_font.max_width,
        game->colony_font.max_height
      );
    } else {
      diag_warn("Failed to load FONTTINY.FF: %s", ff_err);
    }
  }

  char mp_path[512];
  char mp_err[256];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "AMER2.MP", mp_path, sizeof(mp_path))) {
    if (map_load_mp(mp_path, &game->world_map, mp_err, sizeof(mp_err))) {
      game->world_map_ok = true;
      game->map_cursor_x = game->world_map.width / 2;
      game->map_cursor_y = game->world_map.height / 2;
      game->map_view_x = game->map_cursor_x;
      game->map_view_y = game->map_cursor_y;
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

  char europe_err[256];
  if (europe_load(&game->europe, game->resolved_data_dir, europe_err, sizeof(europe_err))) {
    game->europe_ok = true;
  } else {
    game->europe_ok = false;
    diag_warn("Failed to load Europe screen: %s", europe_err);
  }

  char colony_screen_err[256];
  if (colony_screen_load(&game->colony_screen, game->resolved_data_dir, colony_screen_err, sizeof(colony_screen_err))) {
    game->colony_screen_ok = true;
  } else {
    game->colony_screen_ok = false;
    diag_warn("Failed to load colony screen: %s", colony_screen_err);
  }

  char reports_err[256];
  reports_init(&game->reports);
  if (reports_load(&game->reports, game->resolved_data_dir, reports_err, sizeof(reports_err))) {
    game->reports_ok = true;
  } else {
    game->reports_ok = false;
    diag_warn("Failed to load report screens: %s", reports_err);
  }

  {
    char ss_path[512];
    char ss_err[128];
    if (dos_compat_normalize_asset_path(
          game->resolved_data_dir, "BUILDING.SS", ss_path, sizeof(ss_path)
        ) &&
        ss_load(ss_path, &game->pedia_buildings, ss_err, sizeof(ss_err))) {
      game->pedia_buildings_ok = true;
      diag_info(
        "Loaded BUILDING.SS for Colonizopedia (%d sprites)",
        game->pedia_buildings.sprite_count
      );
    } else {
      game->pedia_buildings_ok = false;
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
  pik_free(&game->pedia_wood);
  europe_free(&game->europe);
  colony_screen_free(&game->colony_screen);
  reports_free(&game->reports);
  ss_free(&game->menu_opentile);
  ss_free(&game->terrain);
  ss_free(&game->phys0);
  ss_free(&game->cursor);
  ss_free(&game->unit_icons);
  ss_free(&game->pedia_buildings);
  ss_free(&game->pedia_father);
  ff_free(&game->menu_font);
  ff_free(&game->intro_font);
  ff_free(&game->colony_font);
  map_free(&game->world_map);
  col1_save_free(&game->col1);
  assets_msg_free(&game->messages);
  assets_msg_free(&game->map_menu_txt);
  assets_msg_free(&game->labels);
  map_menu_free(&game->map_menu);
  map_panel_free(&game->map_panel);
  assets_msg_free(&game->pedia);
  assets_msg_free(&game->names);
  debug_atlas_free(&game->debug_atlas);
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
    char err[256];
    if (!game_load_col1_slot(game, 0, err, sizeof(err))) {
      set_status(game, "Load failed", err);
      diag_error("Menu load failed: %s", err);
      return;
    }
    snprintf(
      game->status,
      sizeof(game->status),
      "Loaded COLONY00 (turn %u, year %u)",
      game->turn_number,
      game->game_year
    );
    return;
  }

  /* Start / customize / America / New World -> enter map view. */
  game->in_menu = false;
  col1_save_free(&game->col1);
  game->col1_ok = false;
  game->game_year = 1492;
  game->game_autumn = 0;
  game->human_nation = 0;
  game->active_turn_nation = 0;
  game->turn_number = 0;
  europe_reset_campaign(&game->europe);
  if (game->world_map_ok) {
    units_new_world_start(&game->units, &game->world_map);
  }
  sound_set_bgm(1);
  snprintf(
    game->status,
    sizeof(game->status),
    "Map: menus (mouse), arrows, Enter move, B found, C colony, E Europe"
  );
}

static void game_set_view_center(ColonizeGameState* game, int x, int y) {
  if (!game) {
    return;
  }
  game->map_view_x = x;
  game->map_view_y = y;
}

/* Tile-select mode: clear unit selection, place blinking cursor, center view. */
static void game_select_tile(ColonizeGameState* game, int x, int y) {
  if (!game) {
    return;
  }
  if (game->units_ok) {
    game->units.selected_id = -1;
  }
  game->map_cursor_x = x;
  game->map_cursor_y = y;
  game_set_view_center(game, x, y);
}

static void game_select_unit(ColonizeGameState* game, int unit_id) {
  if (!game || !game->units_ok) {
    return;
  }
  const ColonizeUnit* u = units_get_const(&game->units, unit_id);
  if (!u) {
    return;
  }
  game->units.selected_id = unit_id;
  game->map_cursor_x = u->x;
  game->map_cursor_y = u->y;
  game_set_view_center(game, u->x, u->y);
  const ColonizeUnitType* ut = units_type(&game->units, u->type_index);
  snprintf(game->status, sizeof(game->status), "Selected %s", ut ? ut->name : "unit");
}

/* Human-owned map unit on tile; prefers one with moves remaining. */
static int game_owned_unit_at(const ColonizeGameState* game, int x, int y) {
  if (!game || !game->units_ok) {
    return -1;
  }
  int with_moves = -1;
  int without_moves = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &game->units.units[i];
    if (!units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    if (u->nation_id != game->human_nation) {
      continue;
    }
    if (u->moves_left > 0) {
      with_moves = u->id;
    } else if (without_moves < 0) {
      without_moves = u->id;
    }
  }
  return with_moves >= 0 ? with_moves : without_moves;
}

static void game_find_next_colony(ColonizeGameState* game) {
  if (!game || game->colonies.colony_count <= 0) {
    set_status(game, "No colonies founded yet", NULL);
    return;
  }
  int best_id = -1;
  int best_x = 9999;
  int best_y = 9999;
  int next_id = -1;
  int next_x = 9999;
  int next_y = 9999;
  const int cx = game->map_cursor_x;
  const int cy = game->map_cursor_y;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &game->colonies.colonies[i];
    if (!c->active) {
      continue;
    }
    if (best_id < 0 || c->y < best_y || (c->y == best_y && c->x < best_x)) {
      best_id = c->id;
      best_x = c->x;
      best_y = c->y;
    }
    const bool after =
      (c->y > cy) || (c->y == cy && c->x > cx);
    if (after &&
        (next_id < 0 || c->y < next_y || (c->y == next_y && c->x < next_x))) {
      next_id = c->id;
      next_x = c->x;
      next_y = c->y;
    }
  }
  const ColonizeColony* target =
    colonies_get(&game->colonies, next_id >= 0 ? next_id : best_id);
  if (!target) {
    set_status(game, "No colonies founded yet", NULL);
    return;
  }
  game->map_cursor_x = target->x;
  game->map_cursor_y = target->y;
  game_set_view_center(game, target->x, target->y);
  snprintf(game->status, sizeof(game->status), "Find Colony: %s", target->name);
}

static void game_enter_colony_at_cursor(ColonizeGameState* game) {
  const int cid = colonies_id_at(&game->colonies, game->map_cursor_x, game->map_cursor_y);
  if (cid < 0) {
    set_status(game, "No colony at cursor", NULL);
    return;
  }
  game->in_colony = true;
  game->in_europe = false;
  game->in_pedia = false;
  game->in_report = false;
  game->colony_view_id = cid;
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  snprintf(game->status, sizeof(game->status), "Entered %s", col ? col->name : "colony");
  colony_screen_set_status(&game->colony_screen, col ? col->name : "Colony");
}

static void game_center_on_selected_unit(ColonizeGameState* game) {
  const ColonizeUnit* selected = units_get_const(&game->units, game->units.selected_id);
  if (!selected || !selected->active) {
    set_status(game, "No active unit to center on", NULL);
    return;
  }
  game->map_cursor_x = selected->x;
  game->map_cursor_y = selected->y;
  game_set_view_center(game, selected->x, selected->y);
  set_status(game, "Centered on active unit", NULL);
}

static void game_apply_turn_autosave(ColonizeGameState* game, const ColonizeTurnResult* result) {
  if (!game || !result) {
    return;
  }
  char err[256];
  if (result->request_autosave_decade) {
    if (!game_save_col1_slot(game, 8, err, sizeof(err))) {
      diag_warn("Decade autosave failed: %s", err);
    } else {
      diag_info("Decade autosave → COLONY08.SAV");
    }
  }
  if (result->request_autosave_turn) {
    if (!game_save_col1_slot(game, 9, err, sizeof(err))) {
      diag_warn("Turn autosave failed: %s", err);
    } else {
      diag_info("Turn autosave → COLONY09.SAV");
    }
  }
}

static void game_fill_turn_context(ColonizeGameState* game, ColonizeTurnContext* ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->turn_number = &game->turn_number;
  ctx->game_year = &game->game_year;
  ctx->game_autumn = &game->game_autumn;
  ctx->human_nation = game->human_nation;
  ctx->active_turn_nation = &game->active_turn_nation;
  ctx->units = game->units_ok ? &game->units : NULL;
  ctx->colonies = &game->colonies;
  ctx->europe = game->europe_ok ? &game->europe : &game->europe;
  ctx->col1 = game->col1_ok ? &game->col1 : NULL;
  ctx->col1_ok = game->col1_ok;
  ctx->status = game->status;
  ctx->status_size = sizeof(game->status);
}

static void game_finish_end_turn(ColonizeGameState* game, const ColonizeTurnResult* result) {
  game_apply_turn_autosave(game, result);
  const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
  if (sel && sel->active) {
    game->map_cursor_x = sel->x;
    game->map_cursor_y = sel->y;
    game_set_view_center(game, sel->x, sel->y);
  }
}

static void game_do_end_turn(ColonizeGameState* game) {
  if (!game || turn_processor_active(&game->turn_proc)) {
    return;
  }
  turn_processor_start(&game->turn_proc);
  /* Run setup immediately so calendar advances on the same input that ends the turn. */
  ColonizeTurnContext ctx;
  game_fill_turn_context(game, &ctx);
  if (!turn_processor_advance(&game->turn_proc, &ctx)) {
    game_finish_end_turn(game, &game->turn_proc.result);
  }
}

static void game_wait_next_unit(ColonizeGameState* game) {
  if (!game || !game->units_ok) {
    return;
  }
  if (!turn_select_next_unit(&game->units, game->human_nation)) {
    game->units.selected_id = -1;
    if (turn_option_end_of_turn(game->col1_ok ? &game->col1 : NULL, game->col1_ok)) {
      snprintf(game->status, sizeof(game->status), "%s", "End of Turn");
    } else {
      game_do_end_turn(game);
    }
    return;
  }
  game_center_on_selected_unit(game);
  snprintf(game->status, sizeof(game->status), "%s", "Continue turn.");
}

/* Returns false if the game should quit. */
static bool game_apply_map_menu_action(ColonizeGameState* game, MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_NONE:
      return true;
    case MAP_MENU_ACTION_SEPARATOR:
      return true;
    case MAP_MENU_ACTION_UNIMPLEMENTED:
      set_status(game, "Not implemented yet", NULL);
      return true;
    case MAP_MENU_ACTION_SAVE: {
      char err[256];
      if (!game_save_col1_slot(game, 0, err, sizeof(err))) {
        set_status(game, "Save failed", err);
        return true;
      }
      snprintf(
        game->status,
        sizeof(game->status),
        "Saved COLONY00 (turn %u, year %u)",
        game->turn_number,
        game->game_year
      );
      return true;
    }
    case MAP_MENU_ACTION_LOAD: {
      char err[256];
      if (!game_load_col1_slot(game, 0, err, sizeof(err))) {
        set_status(game, "Load failed", err);
        return true;
      }
      snprintf(
        game->status,
        sizeof(game->status),
        "Loaded COLONY00 (turn %u, year %u)",
        game->turn_number,
        game->game_year
      );
      return true;
    }
    case MAP_MENU_ACTION_RETIRE: {
      ColonizeInputState empty;
      memset(&empty, 0, sizeof(empty));
      map_menu_handle_input(&game->map_menu, &empty, NULL, true);
      game->in_menu = true;
      sound_stop_bgm();
      sound_play(SOUND_TITLE_ID);
      set_status(game, "Retired to main menu", NULL);
      return true;
    }
    case MAP_MENU_ACTION_EXIT:
      return false;
    case MAP_MENU_ACTION_PICK_MUSIC:
      if (!pick_music_open(&game->pick_music, &game->messages)) {
        set_status(game, "Pick Music unavailable", "GAME.TXT @PICKMUSIC missing");
      } else {
        set_status(game, "Pick Music", "Esc closes");
      }
      return true;
    case MAP_MENU_ACTION_EUROPE:
      game->in_europe = true;
      game->in_pedia = false;
      game->in_colony = false;
      snprintf(
        game->europe.status,
        sizeof(game->europe.status),
        "Home port ready. Recruit / Train / S Sail / Esc."
      );
      return true;
    case MAP_MENU_ACTION_FIND_COLONY:
      game_find_next_colony(game);
      return true;
    case MAP_MENU_ACTION_CENTER_VIEW:
      game_center_on_selected_unit(game);
      return true;
    case MAP_MENU_ACTION_ACTIVATE_UNIT: {
      const int at = units_id_at(&game->units, game->map_cursor_x, game->map_cursor_y);
      if (at < 0) {
        set_status(game, "No unit at cursor", NULL);
      } else {
        game->units.selected_id = at;
        const ColonizeUnit* u = units_get_const(&game->units, at);
        const ColonizeUnitType* ut = u ? units_type(&game->units, u->type_index) : NULL;
        snprintf(game->status, sizeof(game->status), "Selected %s", ut ? ut->name : "unit");
      }
      return true;
    }
    case MAP_MENU_ACTION_WAIT_UNIT:
      game_wait_next_unit(game);
      return true;
    case MAP_MENU_ACTION_BUILD_COLONY: {
      const int cx = game->map_cursor_x;
      const int cy = game->map_cursor_y;
      if (!game->world_map_ok || !colonies_can_found(&game->colonies, &game->world_map, cx, cy)) {
        set_status(game, "Cannot found colony here", NULL);
      } else {
        const int uid = units_id_at(&game->units, cx, cy);
        if (uid < 0) {
          set_status(game, "No unit at cursor to found colony", NULL);
        } else if (units_is_sea(&game->units, uid)) {
          set_status(game, "Ships cannot found colonies", NULL);
        } else {
          ColonizeUnit* founder = units_get(&game->units, uid);
          const int type_index = founder ? founder->type_index : -1;
          int tools = 0, muskets = 0, horses = 0;
          units_founder_loot(&game->units, uid, &tools, &muskets, &horses);
          const int cid = colonies_found(
            &game->colonies, &game->world_map, cx, cy, type_index, tools, muskets, horses
          );
          if (cid >= 0) {
            ColonizeColony* neu = colonies_get_mut(&game->colonies, cid);
            if (neu) {
              neu->nation_id = game->human_nation;
            }
            units_despawn(&game->units, uid);
            const ColonizeColony* col = colonies_get(&game->colonies, cid);
            snprintf(
              game->status,
              sizeof(game->status),
              "Founded %s (pop %d)",
              col ? col->name : "colony",
              col ? col->population : 0
            );
          }
        }
      }
      return true;
    }
    case MAP_MENU_ACTION_JOIN_COLONY:
      game_enter_colony_at_cursor(game);
      return true;
    case MAP_MENU_ACTION_LOAD_CARGO: {
      if (!game->world_map_ok || !game->units_ok) {
        set_status(game, "Cannot load cargo", NULL);
        return true;
      }
      const int sid = game->units.selected_id;
      const int at_cursor = units_id_at(&game->units, game->map_cursor_x, game->map_cursor_y);
      int land_id = -1;
      int ship_id = -1;
      if (sid >= 0 && at_cursor >= 0) {
        if (!units_is_sea(&game->units, sid) && units_is_sea(&game->units, at_cursor)) {
          land_id = sid;
          ship_id = at_cursor;
        } else if (units_is_sea(&game->units, sid) && !units_is_sea(&game->units, at_cursor)) {
          land_id = at_cursor;
          ship_id = sid;
        }
      }
      if (land_id < 0 || ship_id < 0) {
        set_status(game, "Select land unit and cursor on adjacent ship (or reverse)", NULL);
      } else if (!units_board(&game->units, land_id, ship_id)) {
        set_status(game, "Cannot board (need adjacent ship with free hold)", NULL);
      } else {
        const ColonizeUnit* ship = units_get_const(&game->units, ship_id);
        snprintf(
          game->status,
          sizeof(game->status),
          "Boarded ship (hold %d)",
          ship ? ship->cargo_count : 0
        );
      }
      return true;
    }
    case MAP_MENU_ACTION_UNLOAD_CARGO: {
      const int sid = game->units.selected_id;
      if (!game->world_map_ok || !game->units_ok || sid < 0 || !units_is_sea(&game->units, sid)) {
        set_status(game, "Select a ship to unload", NULL);
      } else if (!units_unload(
                   &game->units, sid, &game->world_map, game->map_cursor_x, game->map_cursor_y
                 )) {
        set_status(game, "Cannot unload (need adjacent free land)", NULL);
      } else {
        set_status(game, "Unit unloaded", NULL);
      }
      return true;
    }
    case MAP_MENU_ACTION_RETURN_EUROPE: {
      /* Same rules as H: selected ship on high seas sails to Europe harbor. */
      if (!game->world_map_ok || !game->units_ok || !game->europe_ok) {
        set_status(game, "Cannot return to Europe", NULL);
        return true;
      }
      const int sid = game->units.selected_id;
      const ColonizeUnit* ship = units_get_const(&game->units, sid);
      if (sid < 0 || !ship || !units_is_sea(&game->units, sid)) {
        set_status(game, "Select a ship to sail to Europe", NULL);
      } else if (!units_on_high_seas(&game->world_map, ship->x, ship->y)) {
        set_status(game, "Ship must be on high seas", NULL);
      } else {
        const int berth_x = ship->x;
        const int berth_y = ship->y;
        int type_index = -1;
        char ship_name[32];
        int cargo_types[EUROPE_SHIP_CARGO_MAX];
        int cargo_count = 0;
        if (!units_despawn_ship_with_cargo(
              &game->units,
              sid,
              &type_index,
              ship_name,
              sizeof(ship_name),
              cargo_types,
              &cargo_count,
              EUROPE_SHIP_CARGO_MAX
            )) {
          set_status(game, "Failed to sail ship", NULL);
        } else if (!europe_harbor_push(
                     &game->europe, type_index, ship_name, cargo_types, cargo_count
                   )) {
          const int restored = units_spawn_ship_with_cargo(
            &game->units, type_index, berth_x, berth_y, cargo_types, cargo_count
          );
          if (restored >= 0) {
            game->units.selected_id = restored;
          }
          set_status(game, "Europe harbor is full", NULL);
        } else {
          snprintf(game->status, sizeof(game->status), "%s sailed to Europe", ship_name);
          game->in_europe = true;
        }
      }
      return true;
    }
    case MAP_MENU_ACTION_NO_ORDERS:
      game_do_end_turn(game);
      return true;
    case MAP_MENU_ACTION_PEDIA_CARGO:
      game_open_pedia_list(game, PEDIA_CAT_CARGO);
      return true;
    case MAP_MENU_ACTION_PEDIA_UNIT:
      game_open_pedia_list(game, PEDIA_CAT_UNIT);
      return true;
    case MAP_MENU_ACTION_PEDIA_TERRAIN:
      game_open_pedia_list(game, PEDIA_CAT_TERRAIN);
      return true;
    case MAP_MENU_ACTION_PEDIA_JOB:
      game_open_pedia_list(game, PEDIA_CAT_JOB);
      return true;
    case MAP_MENU_ACTION_PEDIA_BUILDING:
      game_open_pedia_list(game, PEDIA_CAT_BUILDING);
      return true;
    case MAP_MENU_ACTION_PEDIA_FATHER:
      game_open_pedia_list(game, PEDIA_CAT_FATHER);
      return true;
    case MAP_MENU_ACTION_PEDIA_MISC:
      game_open_pedia_list(game, PEDIA_CAT_MISC);
      return true;
    case MAP_MENU_ACTION_REPORT_TERRAIN:
      game_open_terrain_pedia_at_cursor(game);
      return true;
    case MAP_MENU_ACTION_REPORT_RELIGIOUS:
      game_open_report(game, COLONIZE_REPORT_RELIGIOUS);
      return true;
    case MAP_MENU_ACTION_REPORT_CONGRESS:
      game_open_report(game, COLONIZE_REPORT_CONGRESS);
      return true;
    case MAP_MENU_ACTION_REPORT_LABOR:
      game_open_report(game, COLONIZE_REPORT_LABOR);
      return true;
    case MAP_MENU_ACTION_REPORT_ECONOMIC:
      game_open_report(game, COLONIZE_REPORT_ECONOMIC);
      return true;
    case MAP_MENU_ACTION_REPORT_COLONY:
      game_open_report(game, COLONIZE_REPORT_COLONY);
      return true;
    case MAP_MENU_ACTION_REPORT_NAVAL:
      game_open_report(game, COLONIZE_REPORT_NAVAL);
      return true;
    case MAP_MENU_ACTION_REPORT_FOREIGN:
      game_open_report(game, COLONIZE_REPORT_FOREIGN);
      return true;
    case MAP_MENU_ACTION_REPORT_INDIAN:
      game_open_report(game, COLONIZE_REPORT_INDIAN);
      return true;
    case MAP_MENU_ACTION_REPORT_SCORE:
      game_open_report(game, COLONIZE_REPORT_SCORE);
      return true;
    default:
      set_status(game, "Not implemented yet", NULL);
      return true;
  }
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
  sound_service();

  /* End-of-turn nation phases: advance one slice per frame; block other input. */
  if (turn_processor_active(&game->turn_proc)) {
    ColonizeTurnContext ctx;
    game_fill_turn_context(game, &ctx);
    if (!turn_processor_advance(&game->turn_proc, &ctx)) {
      game_finish_end_turn(game, &game->turn_proc.result);
    }
    return true;
  }

  if (input->last_key != COLONIZE_KEY_NONE) {
    diag_info(
      "Key pressed: %s (menu=%s debug=%s pedia=%s europe=%s colony=%s report=%s turn=%u cursor=%d,%d)",
      key_name(input->last_key),
      game->in_menu ? "yes" : "no",
      game->in_debug_atlas ? "yes" : "no",
      game->in_pedia ? "yes" : "no",
      game->in_europe ? "yes" : "no",
      game->in_colony ? "yes" : "no",
      game->in_report ? "yes" : "no",
      game->turn_number,
      game->map_cursor_x,
      game->map_cursor_y
    );
  }

  if (input->last_key == COLONIZE_KEY_Q) {
    return false;
  }

  if (game->in_report) {
    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_ENTER) {
      game->in_report = false;
      diag_info("Left report screen.");
      return true;
    }
    /* F-keys switch reports (F2–F10) or open terrain pedia (F1). */
    if (input->last_key >= COLONIZE_KEY_F1 && input->last_key <= COLONIZE_KEY_F10) {
      game_handle_report_fkey(game, input->last_key);
    }
    return true;
  }

  if (game->in_colony) {
    if (input->last_key == COLONIZE_KEY_ESCAPE ||
        input->last_key == COLONIZE_KEY_C ||
        input->last_key == COLONIZE_KEY_ENTER) {
      game->in_colony = false;
      game->colony_view_id = -1;
      diag_info("Left colony screen.");
      return true;
    }
    /* Cheat: Space = free production cycle without advancing world time. */
    if (input->last_key == COLONIZE_KEY_SPACE) {
      ColonizeColony* colony = colonies_get_mut(&game->colonies, game->colony_view_id);
      if (colony) {
        ColonizeTurnResult prod;
        memset(&prod, 0, sizeof(prod));
        turn_colony_free_production(&game->colonies, colony, &prod);
        snprintf(
          game->status,
          sizeof(game->status),
          "Colony free turn (food %d, hammers %d)",
          colony->stock[COLONIZE_CARGO_FOOD],
          colony->hammers
        );
        colony_screen_set_status(&game->colony_screen, game->status);
        diag_info("Colony free production id=%d", colony->id);
      }
      return true;
    }
    return true;
  }

  if (game->in_europe) {
    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_E) {
      game->in_europe = false;
      diag_info("Left Europe screen.");
      return true;
    }
    if (input->last_key == COLONIZE_KEY_R) {
      europe_recruit(&game->europe);
    } else if (input->last_key == COLONIZE_KEY_T) {
      europe_train_stub(&game->europe);
    } else if (input->last_key == COLONIZE_KEY_S) {
      if (game->europe.harbor_ships <= 0) {
        snprintf(game->europe.status, sizeof(game->europe.status), "%s", "No ships in harbor.");
      } else if (!game->world_map_ok || !game->units_ok) {
        snprintf(game->europe.status, sizeof(game->europe.status), "%s", "Cannot sail: map unavailable.");
      } else {
        int type_index = -1;
        char ship_name[32];
        int cargo_types[EUROPE_SHIP_CARGO_MAX];
        int cargo_count = 0;
        if (!europe_harbor_pop(
              &game->europe,
              &type_index,
              ship_name,
              sizeof(ship_name),
              cargo_types,
              &cargo_count,
              EUROPE_SHIP_CARGO_MAX
            )) {
          snprintf(game->europe.status, sizeof(game->europe.status), "%s", "Harbor pop failed.");
        } else {
          int sx = 39;
          int sy = 10;
          if (!units_find_high_seas_tile(&game->units, &game->world_map, sx, sy, &sx, &sy)) {
            europe_harbor_push(
              &game->europe, type_index, ship_name, cargo_types, cargo_count
            );
            snprintf(game->europe.status, sizeof(game->europe.status), "%s", "No free high-seas berth.");
          } else {
            const int ship_id = units_spawn_ship_with_cargo(
              &game->units, type_index, sx, sy, cargo_types, cargo_count
            );
            if (ship_id < 0) {
              europe_harbor_push(
                &game->europe, type_index, ship_name, cargo_types, cargo_count
              );
              snprintf(game->europe.status, sizeof(game->europe.status), "%s", "Could not place ship.");
            } else {
              game->units.selected_id = ship_id;
              game->map_cursor_x = sx;
              game->map_cursor_y = sy;
              game_set_view_center(game, sx, sy);
              game->in_europe = false;
              snprintf(
                game->status,
                sizeof(game->status),
                "%s arrived at (%d,%d)",
                ship_name,
                sx,
                sy
              );
              diag_info("Sailed %s from Europe to (%d,%d)", ship_name, sx, sy);
            }
          }
        }
      }
    } else if (input->last_key == COLONIZE_KEY_RIGHTBRACKET) {
      europe_cheat_add_gold(&game->europe, 1000);
    } else if (input->last_key == COLONIZE_KEY_LEFTBRACKET) {
      europe_cheat_adjust_tax(&game->europe, -1);
    }
    return true;
  }

  if (game->in_pedia) {
    const ColonizeFont* font = game_pedia_font(game);
    if (game->pedia_view == PEDIA_VIEW_LIST) {
      const PediaListHit hover = pedia_list_hit(
        game->pedia_ok ? &game->pedia : NULL,
        game->names_ok ? &game->names : NULL,
        game->pedia_category,
        font,
        input->mouse_x,
        input->mouse_y
      );
      game->pedia_hover_entry =
        (hover.kind == PEDIA_LIST_HIT_ENTRY) ? hover.entry_index : -1;

      if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_P) {
        game->in_pedia = false;
        diag_info("Left Colonizopedia.");
        return true;
      }
      if (input->mouse_left_clicked) {
        const PediaListHit hit = pedia_list_hit(
          game->pedia_ok ? &game->pedia : NULL,
          game->names_ok ? &game->names : NULL,
          game->pedia_category,
          font,
          input->mouse_x,
          input->mouse_y
        );
        if (hit.kind == PEDIA_LIST_HIT_EXIT) {
          game->in_pedia = false;
          diag_info("Left Colonizopedia (Exit).");
          return true;
        }
        if (hit.kind == PEDIA_LIST_HIT_ENTRY) {
          game_open_pedia_article(game, game->pedia_category, hit.entry_index, true);
          return true;
        }
      }
      return true;
    }

    /* Article view. */
    if (input->last_key == COLONIZE_KEY_P) {
      game->in_pedia = false;
      diag_info("Left Colonizopedia.");
      return true;
    }
    if (input->last_key == COLONIZE_KEY_ESCAPE) {
      if (game->pedia_return_to_list) {
        game_open_pedia_list(game, game->pedia_category);
      } else {
        game->in_pedia = false;
        diag_info("Left Colonizopedia.");
      }
      return true;
    }
    const int count = pedia_category_count(game->pedia_category);
    if (count > 0) {
      if (input->last_key == COLONIZE_KEY_LEFT || input->last_key == COLONIZE_KEY_UP) {
        if (game->pedia_index > 0) {
          game->pedia_index--;
        } else {
          game->pedia_index = count - 1;
        }
      } else if (input->last_key == COLONIZE_KEY_RIGHT || input->last_key == COLONIZE_KEY_DOWN) {
        game->pedia_index++;
        if (game->pedia_index >= count) {
          game->pedia_index = 0;
        }
      }
    }
    /* Lazy-load founding-father portrait for the current article. */
    {
      PediaPage page;
      pedia_page(
        game->pedia_ok ? &game->pedia : NULL,
        game->names_ok ? &game->names : NULL,
        game->pedia_category,
        game->pedia_index,
        &page
      );
      if (page.preview_kind == PEDIA_PREVIEW_FATHER) {
        game_pedia_ensure_father_sheet(game, page.father_index);
      }
    }
    return true;
  }

  if (game->in_debug_atlas) {
    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_TILDE) {
      game->in_debug_atlas = false;
      debug_atlas_free(&game->debug_atlas);
      debug_atlas_init(&game->debug_atlas);
      diag_info("Left sprite atlas debug screen.");
      return true;
    }
    if (input->last_key == COLONIZE_KEY_RIGHT) {
      debug_atlas_next_file(&game->debug_atlas, game->resolved_data_dir, 1);
    } else if (input->last_key == COLONIZE_KEY_LEFT) {
      debug_atlas_prev_file(&game->debug_atlas, game->resolved_data_dir, 1);
    } else if (input->last_key == COLONIZE_KEY_RIGHTBRACKET) {
      debug_atlas_next_file(&game->debug_atlas, game->resolved_data_dir, 10);
    } else if (input->last_key == COLONIZE_KEY_LEFTBRACKET) {
      debug_atlas_prev_file(&game->debug_atlas, game->resolved_data_dir, 10);
    } else if (input->last_key == COLONIZE_KEY_UP) {
      debug_atlas_scroll_by(&game->debug_atlas, -1);
    } else if (input->last_key == COLONIZE_KEY_DOWN) {
      debug_atlas_scroll_by(&game->debug_atlas, 1);
    } else if (input->last_key == COLONIZE_KEY_SPACE || input->last_key == COLONIZE_KEY_ENTER) {
      debug_atlas_page_down(&game->debug_atlas);
    }
    return true;
  }

  if (input->last_key == COLONIZE_KEY_TILDE) {
    game->in_debug_atlas = true;
    game->in_pedia = false;
    game->in_europe = false;
    game->in_colony = false;
    game->in_report = false;
    if (game->debug_atlas.count <= 0) {
      debug_atlas_scan(&game->debug_atlas, game->resolved_data_dir);
    }
    debug_atlas_load(&game->debug_atlas, game->resolved_data_dir, 0);
    diag_info(
      "Entered graphic atlas debug (%d files).",
      game->debug_atlas.count
    );
    return true;
  }

  if (input->last_key == COLONIZE_KEY_P) {
    game_open_pedia_list(game, PEDIA_CAT_CARGO);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_C && !game->in_menu && game->world_map_ok) {
    const int cid = colonies_id_at(&game->colonies, game->map_cursor_x, game->map_cursor_y);
    if (cid < 0) {
      set_status(game, "No colony at cursor", NULL);
    } else {
      game->in_colony = true;
      game->in_europe = false;
      game->in_pedia = false;
      game->in_report = false;
      game->colony_view_id = cid;
      const ColonizeColony* col = colonies_get(&game->colonies, cid);
      snprintf(
        game->status,
        sizeof(game->status),
        "Entered %s",
        col ? col->name : "colony"
      );
      colony_screen_set_status(
        &game->colony_screen,
        col ? col->name : "Colony"
      );
      diag_info("Entered colony screen (id=%d).", cid);
    }
    return true;
  }

  if (input->last_key == COLONIZE_KEY_E && !game->in_menu) {
    game->in_europe = true;
    game->in_pedia = false;
    game->in_colony = false;
    game->in_report = false;
    snprintf(
      game->europe.status,
      sizeof(game->europe.status),
      "Home port ready. Recruit / Train / S Sail / Esc."
    );
    diag_info("Entered Europe screen.");
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

  /* Map-screen menu bar (MENU.TXT pull-downs) + mouse map click. */
  if (!game->in_colony && !game->in_europe && !game->in_pedia && !game->in_debug_atlas &&
      !game->in_report) {
    if (game->pick_music.open) {
      const ColonizeFont* pm_font = game->colony_font_ok ? &game->colony_font :
                                    (game->menu_font_ok ? &game->menu_font : NULL);
      pick_music_handle_input(
        &game->pick_music, &game->messages, input, pm_font, game->status, sizeof(game->status)
      );
      return true;
    }

    /* F1 terrain pedia at cursor; F2–F10 adviser / report screens. */
    if (input->last_key >= COLONIZE_KEY_F1 && input->last_key <= COLONIZE_KEY_F10) {
      game_handle_report_fkey(game, input->last_key);
      return true;
    }

    const ColonizeFont* menu_font = game->colony_font_ok ? &game->colony_font :
                                    (game->menu_font_ok ? &game->menu_font : NULL);
    const bool menu_was_open = game->map_menu.open_index >= 0;
    if (input->last_key == COLONIZE_KEY_ESCAPE && menu_was_open) {
      ColonizeInputState empty;
      memset(&empty, 0, sizeof(empty));
      map_menu_handle_input(&game->map_menu, &empty, menu_font, true);
      return true;
    }

    const bool click_on_menu_ui =
      input->mouse_left_clicked &&
      map_menu_hit_ui(&game->map_menu, input->mouse_x, input->mouse_y);
    const MapMenuAction menu_action =
      map_menu_handle_input(&game->map_menu, input, menu_font, false);
    if (menu_action != MAP_MENU_ACTION_NONE) {
      if (menu_action == MAP_MENU_ACTION_UNIMPLEMENTED) {
        set_status(game, "Not implemented yet", NULL);
        return true;
      }
      if (!game_apply_map_menu_action(game, menu_action)) {
        return false;
      }
      return true;
    }

    if (input->mouse_left_clicked && (click_on_menu_ui || menu_was_open)) {
      /* Menu open/close consumed the click. */
      return true;
    }

    if ((input->mouse_left_clicked || input->mouse_right_clicked) && game->world_map_ok) {
      const int tile_w = MAP_VIEW_TILE_W;
      const int tile_h = MAP_VIEW_TILE_H;
      const int map_origin_x = 0;
      const int map_origin_y = MAP_VIEW_ORIGIN_Y;
      const int view_cols = MAP_VIEW_TILE_COLS;
      const int view_rows = MAP_VIEW_TILE_ROWS;
      if (input->mouse_y < map_origin_y) {
        return true;
      }

      int view_x = game->map_view_x - view_cols / 2;
      int view_y = game->map_view_y - view_rows / 2;
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

      /* Right panel / minimap: left-click centers the view on the clicked tile. */
      if (map_panel_contains_xy(input->mouse_x, input->mouse_y)) {
        if (input->mouse_left_clicked) {
          int tx = 0;
          int ty = 0;
          if (map_panel_minimap_click(
                &game->world_map,
                view_x,
                view_y,
                view_cols,
                view_rows,
                input->mouse_x,
                input->mouse_y,
                &tx,
                &ty
              )) {
            game_set_view_center(game, tx, ty);
          }
        }
        return true;
      }

      if (input->mouse_x >= MAP_PANEL_X) {
        return true;
      }

      const int mx = view_x + (input->mouse_x - map_origin_x) / tile_w;
      const int my = view_y + (input->mouse_y - map_origin_y) / tile_h;
      if (mx < 0 || my < 0 || mx >= (int)game->world_map.width || my >= (int)game->world_map.height) {
        return true;
      }

      if (input->mouse_right_clicked) {
        /* Right-click: unselect unit (if any) and select the tile. */
        game_select_tile(game, mx, my);
        return true;
      }

      /* Left-click */
      if (game->units.selected_id >= 0) {
        /* Unit selected: pan view only — keep unit, do not select tile. */
        game_set_view_center(game, mx, my);
        return true;
      }

      const int owned = game_owned_unit_at(game, mx, my);
      if (owned >= 0) {
        const ColonizeUnit* u = units_get_const(&game->units, owned);
        if (u && u->moves_left > 0) {
          game_select_unit(game, owned);
          return true;
        }
        /* Owned unit with no moves: select the tile. */
        game_select_tile(game, mx, my);
        return true;
      }

      const int cid = colonies_id_at(&game->colonies, mx, my);
      if (cid >= 0) {
        const ColonizeColony* col = colonies_get(&game->colonies, cid);
        if (col && col->nation_id == game->human_nation) {
          game_select_tile(game, mx, my);
          game_enter_colony_at_cursor(game);
          return true;
        }
      }

      game_select_tile(game, mx, my);
      return true;
    }
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    game->in_menu = true;
    sound_stop_bgm();
    sound_play(SOUND_TITLE_ID);
    diag_info("Returned to main menu.");
    return true;
  }

  if (input->last_key == COLONIZE_KEY_SPACE) {
    game_do_end_turn(game);
    return true;
  }

  const int map_max_x = game->world_map_ok ? (int)game->world_map.width - 1 : 15;
  const int map_max_y = game->world_map_ok ? (int)game->world_map.height - 1 : 15;

  if (input->last_key == COLONIZE_KEY_ENTER && game->world_map_ok && game->units_ok) {
    const int at_cursor = units_id_at(&game->units, game->map_cursor_x, game->map_cursor_y);
    if (at_cursor >= 0 && at_cursor != game->units.selected_id) {
      game_select_unit(game, at_cursor);
    } else if (game->units.selected_id >= 0) {
      ColonizeUnit* selected = units_get(&game->units, game->units.selected_id);
      if (selected &&
          (selected->x != game->map_cursor_x || selected->y != game->map_cursor_y)) {
        if (units_try_move(
              &game->units,
              game->units.selected_id,
              &game->world_map,
              game->map_cursor_x,
              game->map_cursor_y
            )) {
          game_set_view_center(game, game->map_cursor_x, game->map_cursor_y);
          snprintf(
            game->status,
            sizeof(game->status),
            "Moved unit to (%d,%d)",
            game->map_cursor_x,
            game->map_cursor_y
          );
          /* Auto end-turn when option is clear and no human moves remain. */
          if (!turn_option_end_of_turn(game->col1_ok ? &game->col1 : NULL, game->col1_ok) &&
              turn_human_units_exhausted(&game->units, game->human_nation)) {
            game_do_end_turn(game);
            return true;
          }
        } else {
          set_status(game, "Move blocked", NULL);
        }
      } else if (at_cursor >= 0) {
        game_select_unit(game, at_cursor);
      }
    } else if (at_cursor >= 0) {
      game_select_unit(game, at_cursor);
    }
  }

  /* H: sail selected ship to Europe from high seas (passengers ride along). */
  if (input->last_key == COLONIZE_KEY_H && game->world_map_ok && game->units_ok && game->europe_ok) {
    const int sid = game->units.selected_id;
    ColonizeUnit* ship = units_get(&game->units, sid);
    if (!ship || !units_is_sea(&game->units, sid)) {
      set_status(game, "Select a ship to sail to Europe", NULL);
    } else if (!units_on_high_seas(&game->world_map, ship->x, ship->y)) {
      set_status(game, "Ship must be on high seas", NULL);
    } else {
      const int berth_x = ship->x;
      const int berth_y = ship->y;
      int type_index = -1;
      char ship_name[32];
      int cargo_types[EUROPE_SHIP_CARGO_MAX];
      int cargo_count = 0;
      if (!units_despawn_ship_with_cargo(
            &game->units,
            sid,
            &type_index,
            ship_name,
            sizeof(ship_name),
            cargo_types,
            &cargo_count,
            EUROPE_SHIP_CARGO_MAX
          )) {
        set_status(game, "Failed to sail ship", NULL);
      } else if (!europe_harbor_push(
                   &game->europe, type_index, ship_name, cargo_types, cargo_count
                 )) {
        /* Harbor full — put the ship back on the map with passengers. */
        const int restored = units_spawn_ship_with_cargo(
          &game->units, type_index, berth_x, berth_y, cargo_types, cargo_count
        );
        if (restored >= 0) {
          game->units.selected_id = restored;
        }
        set_status(game, "Europe harbor is full", NULL);
      } else {
        if (cargo_count > 0) {
          snprintf(
            game->status,
            sizeof(game->status),
            "%s sailed to Europe (+%d aboard)",
            ship_name,
            cargo_count
          );
        } else {
          snprintf(game->status, sizeof(game->status), "%s sailed to Europe", ship_name);
        }
        diag_info("Sailed %s to Europe harbor (cargo=%d)", ship_name, cargo_count);
      }
    }
  }

  /* O: board land unit onto adjacent ship (selected land↔cursor ship, or vice versa). */
  if (input->last_key == COLONIZE_KEY_O && game->world_map_ok && game->units_ok) {
    const int sid = game->units.selected_id;
    const int at_cursor = units_id_at(&game->units, game->map_cursor_x, game->map_cursor_y);
    int land_id = -1;
    int ship_id = -1;
    if (sid >= 0 && at_cursor >= 0) {
      if (!units_is_sea(&game->units, sid) && units_is_sea(&game->units, at_cursor)) {
        land_id = sid;
        ship_id = at_cursor;
      } else if (units_is_sea(&game->units, sid) && !units_is_sea(&game->units, at_cursor)) {
        land_id = at_cursor;
        ship_id = sid;
      }
    }
    if (land_id < 0 || ship_id < 0) {
      set_status(game, "Select land unit and cursor on adjacent ship (or reverse)", NULL);
    } else if (!units_board(&game->units, land_id, ship_id)) {
      set_status(game, "Cannot board (need adjacent ship with free hold)", NULL);
    } else {
      const ColonizeUnit* ship = units_get_const(&game->units, ship_id);
      snprintf(
        game->status,
        sizeof(game->status),
        "Boarded ship (hold %d)",
        ship ? ship->cargo_count : 0
      );
    }
  }

  /* U: unload oldest passenger from selected ship onto cursor land tile. */
  if (input->last_key == COLONIZE_KEY_U && game->world_map_ok && game->units_ok) {
    const int sid = game->units.selected_id;
    if (sid < 0 || !units_is_sea(&game->units, sid)) {
      set_status(game, "Select a ship to unload", NULL);
    } else if (!units_unload(
                 &game->units, sid, &game->world_map, game->map_cursor_x, game->map_cursor_y
               )) {
      set_status(game, "Cannot unload (need adjacent free land)", NULL);
    } else {
      set_status(game, "Unit unloaded", NULL);
    }
  }

  if (input->last_key == COLONIZE_KEY_D && game->world_map_ok && game->europe_ok) {
    if (game->europe.dock_count <= 0) {
      set_status(game, "No immigrants on dock", NULL);
    } else {
      const char* immigrant = game->europe.dock[0].name;
      if (!units_deploy_colonist(
            &game->units,
            &game->world_map,
            game->map_cursor_x,
            game->map_cursor_y,
            immigrant
          )) {
        set_status(game, "Cannot deploy here", immigrant);
      } else {
        char name[40];
        europe_pop_dock_immigrant(&game->europe, name, sizeof(name));
        snprintf(
          game->status,
          sizeof(game->status),
          "Deployed %s at (%d,%d)",
          name,
          game->map_cursor_x,
          game->map_cursor_y
        );
      }
    }
  }

  /* B: found a colony — disband land unit into colonist + stockpile loot. */
  if (input->last_key == COLONIZE_KEY_B && game->world_map_ok) {
    const int cx = game->map_cursor_x;
    const int cy = game->map_cursor_y;
    if (!colonies_can_found(&game->colonies, &game->world_map, cx, cy)) {
      set_status(game, "Cannot found colony here", NULL);
    } else {
      const int uid = units_id_at(&game->units, cx, cy);
      if (uid < 0) {
        set_status(game, "No unit at cursor to found colony", NULL);
      } else if (units_is_sea(&game->units, uid)) {
        set_status(game, "Ships cannot found colonies", NULL);
      } else {
        ColonizeUnit* founder = units_get(&game->units, uid);
        const int type_index = founder ? founder->type_index : -1;
        int tools = 0;
        int muskets = 0;
        int horses = 0;
        units_founder_loot(&game->units, uid, &tools, &muskets, &horses);
        const int cid = colonies_found(
          &game->colonies, &game->world_map, cx, cy, type_index, tools, muskets, horses
        );
        if (cid >= 0) {
          ColonizeColony* neu = colonies_get_mut(&game->colonies, cid);
          if (neu) {
            neu->nation_id = game->human_nation;
          }
          units_despawn(&game->units, uid);
          const ColonizeColony* col = colonies_get(&game->colonies, cid);
          snprintf(
            game->status,
            sizeof(game->status),
            "Founded %s (pop %d)",
            col ? col->name : "colony",
            col ? col->population : 0
          );
        }
      }
    }
  }

  if (input->last_key == COLONIZE_KEY_UP || input->last_key == COLONIZE_KEY_DOWN ||
      input->last_key == COLONIZE_KEY_LEFT || input->last_key == COLONIZE_KEY_RIGHT) {
    if (game->units.selected_id < 0) {
      if (input->last_key == COLONIZE_KEY_UP && game->map_cursor_y > 0) {
        game->map_cursor_y--;
      } else if (input->last_key == COLONIZE_KEY_DOWN && game->map_cursor_y < map_max_y) {
        game->map_cursor_y++;
      } else if (input->last_key == COLONIZE_KEY_LEFT && game->map_cursor_x > 0) {
        game->map_cursor_x--;
      } else if (input->last_key == COLONIZE_KEY_RIGHT && game->map_cursor_x < map_max_x) {
        game->map_cursor_x++;
      }
      game_set_view_center(game, game->map_cursor_x, game->map_cursor_y);
    } else {
      /* Unit selected: arrows pan the view without changing selection. */
      if (input->last_key == COLONIZE_KEY_UP && game->map_view_y > 0) {
        game->map_view_y--;
      } else if (input->last_key == COLONIZE_KEY_DOWN && game->map_view_y < map_max_y) {
        game->map_view_y++;
      } else if (input->last_key == COLONIZE_KEY_LEFT && game->map_view_x > 0) {
        game->map_view_x--;
      } else if (input->last_key == COLONIZE_KEY_RIGHT && game->map_view_x < map_max_x) {
        game->map_view_x++;
      }
    }
  }

  if (input->last_key == COLONIZE_KEY_S) {
    char err[256];
    diag_info(
      "Save requested: slot=COLONY00 save_dir=%s turn=%u",
      game->config.save_dir ? game->config.save_dir : "(null)",
      game->turn_number
    );
    if (!game_save_col1_slot(game, 0, err, sizeof(err))) {
      set_status(game, "Save failed", err);
      diag_error("Save failed: %s", err);
      return true;
    }
    snprintf(
      game->status,
      sizeof(game->status),
      "Saved COLONY00 (turn %u, year %u)",
      game->turn_number,
      game->game_year
    );
    diag_info("Save succeeded for COLONY00 (turn %u)", game->turn_number);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_L) {
    char err[256];
    diag_info(
      "Load requested: slot=COLONY00 save_dir=%s",
      game->config.save_dir ? game->config.save_dir : "(null)"
    );
    if (!game_load_col1_slot(game, 0, err, sizeof(err))) {
      set_status(game, "Load failed", err);
      diag_error("Load failed: %s", err);
      return true;
    }
    snprintf(
      game->status,
      sizeof(game->status),
      "Loaded COLONY00 (turn %u, year %u)",
      game->turn_number,
      game->game_year
    );
    diag_info(
      "Load succeeded: turn=%u year=%u units=%d colonies=%d",
      game->turn_number,
      game->game_year,
      game->units.unit_count,
      game->colonies.colony_count
    );
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

/* Title-menu helpers: brace markup + selection fill (chrome via popup_draw). */

static int begin_menu_text_width(const ColonizeFont* font, const char* text) {
  if (!text) {
    return 0;
  }
  int w = 0;
  for (const char* p = text; *p; ++p) {
    if (*p == '{' || *p == '}') {
      continue;
    }
    const unsigned char ch = (unsigned char)*p;
    if (font && font->section_data && ch < 128 && font->char_widths[ch] > 0) {
      w += font->char_widths[ch];
    } else {
      w += 6;
    }
  }
  return w;
}

static void begin_menu_fill_rect(
  ColonizeFramebuffer8* fb,
  int x0,
  int y0,
  int x1,
  int y1,
  uint8_t color
) {
  if (!fb || !fb->pixels) {
    return;
  }
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 >= fb->width) {
    x1 = fb->width - 1;
  }
  if (y1 >= fb->height) {
    y1 = fb->height - 1;
  }
  if (x0 > x1 || y0 > y1) {
    return;
  }
  for (int y = y0; y <= y1; ++y) {
    uint8_t* row = fb->pixels + y * fb->width;
    for (int x = x0; x <= x1; ++x) {
      row[x] = color;
    }
  }
}

/* Draw GAME.TXT brace markup: {text} uses emphasis color. */
static void begin_menu_draw_markup(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t normal_color,
  uint8_t emphasis_color
) {
  if (!fb || !text) {
    return;
  }
  uint8_t color = normal_color;
  int cx = x;
  char chbuf[2] = {0, 0};
  for (const char* p = text; *p; ++p) {
    if (*p == '{') {
      color = emphasis_color;
      continue;
    }
    if (*p == '}') {
      color = normal_color;
      continue;
    }
    chbuf[0] = *p;
    font_draw_text(font, fb, cx, y, chbuf, color);
    const unsigned char ch = (unsigned char)*p;
    if (font && font->section_data && ch < 128 && font->char_widths[ch] > 0) {
      cx += font->char_widths[ch];
    } else {
      cx += 6;
    }
  }
}

static const ColonizeFont* begin_menu_font(const ColonizeGameState* game) {
  if (!game) {
    return NULL;
  }
  /* @smallfont → FONTTINY (colony_font). Default dialog slot is FONTINTR. */
  if (game->menu_smallfont && game->colony_font_ok) {
    return &game->colony_font;
  }
  if (game->intro_font_ok) {
    return &game->intro_font;
  }
  if (game->colony_font_ok) {
    return &game->colony_font;
  }
  if (game->menu_font_ok) {
    return &game->menu_font;
  }
  return NULL;
}

static void game_render_begin_menu(
  const ColonizeGameState* game,
  ColonizeFramebuffer8* framebuffer
) {
  if (!game || !framebuffer || !framebuffer->pixels) {
    return;
  }

  const ColonizeFont* font = begin_menu_font(game);
  const int line_h = font ? (font->max_height + 2) : 8;
  const int title_h = font ? font->max_height : 6;
  const int title_pad_top = 3; /* from innermost top bevel */
  const int title_pad_x = 2; /* from innermost left/right bevel */
  const int option_pad_x = 8;
  const int gap_after_title = 4; /* title glyph bottom → first option */
  const int options_h = game->menu_option_count * line_h;
  /*
   * Outer height from the previous pad_y=6 / line_h layout, then -12 from the
   * bottom (dialog_y unchanged). Inner title spacing is tighter (3 / title_h / 4).
   */
  int dialog_h = POPUP_FRAME_INSET * 2 + 6 +
                 (game->menu_version_line[0] ? line_h + 4 : 0) + options_h + 6 - 12;
  if (dialog_h < 24) {
    dialog_h = 24;
  }

  int dialog_w = game->menu_dialog_width > 0 ? game->menu_dialog_width : 160;
  if (dialog_w > framebuffer->width) {
    dialog_w = framebuffer->width;
  }
  int dialog_x = (framebuffer->width - dialog_w) / 2;
  int dialog_y = game->menu_dialog_y;
  if (dialog_y < 0) {
    dialog_y = 0;
  }
  if (dialog_y + dialog_h > framebuffer->height) {
    dialog_y = framebuffer->height - dialog_h;
    if (dialog_y < 0) {
      dialog_y = 0;
      dialog_h = framebuffer->height;
    }
  }

  int inner_x = 0;
  int inner_y = 0;
  int inner_w = 0;
  int inner_h = 0;
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    game->menu_opentile_ok ? &game->menu_opentile : NULL,
    &game->menu_popup_colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  if (inner_w <= 0 || inner_h <= 0) {
    return;
  }

  int text_y = inner_y + title_pad_top;
  if (game->menu_version_line[0]) {
    const int tw = begin_menu_text_width(font, game->menu_version_line);
    int tx = inner_x + (inner_w - tw) / 2;
    if (tx < inner_x + title_pad_x) {
      tx = inner_x + title_pad_x;
    }
    begin_menu_draw_markup(
      font,
      framebuffer,
      tx,
      text_y,
      game->menu_version_line,
      game->menu_col_basic,
      game->menu_col_hilite
    );
    text_y += title_h + gap_after_title;
  }

  for (int i = 0; i < game->menu_option_count; ++i) {
    const int row_y = text_y + i * line_h;
    const bool selected = (i == game->menu_selection);
    if (selected) {
      /* 1px inset from each side of the inner content edge. */
      begin_menu_fill_rect(
        framebuffer,
        inner_x + 1,
        row_y - 1,
        inner_x + inner_w - 2,
        row_y + line_h - 2,
        game->menu_col_select
      );
    }
    font_draw_text(
      font, framebuffer, inner_x + option_pad_x, row_y, game->menu_options[i], game->menu_col_basic
    );
  }
}

void game_render(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette) {
  static uint32_t render_log_counter = 0;
  if (!game || !framebuffer || !palette || !framebuffer->pixels) {
    return;
  }

  *palette = (game->in_menu && !game->in_debug_atlas && !game->in_pedia && !game->in_europe &&
              !game->in_colony && !game->in_report)
    ? game->palette
    : (game->in_debug_atlas && debug_atlas_palette(&game->debug_atlas))
      ? *debug_atlas_palette(&game->debug_atlas)
      : (game->in_pedia && game->pedia_view == PEDIA_VIEW_LIST && game->pedia_wood_ok &&
         game->pedia_wood.has_palette)
        ? game->pedia_wood.palette
        : (game->in_report && game->reports_ok && game->reports.background_ok[game->report_id] &&
           game->reports.backgrounds[game->report_id].has_palette)
          ? game->reports.backgrounds[game->report_id].palette
          : (game->in_europe && game->europe_ok && game->europe.background.has_palette)
            ? game->europe.background.palette
            : (game->in_colony && game->colony_screen_ok && game->colony_screen.frame.has_palette)
              ? game->colony_screen.frame.palette
              : (game->map_palette_ok ? game->map_palette : game->palette);

  if (render_log_counter == 0) {
    diag_info(
      "Render mode=%s framebuffer=%dx%d palette=%s",
      render_mode_name(game),
      framebuffer->width,
      framebuffer->height,
      game->palette_ok ? "VICEROY.PAL" : "fallback"
    );
  }

  if (game->in_europe) {
    render_europe_screen(game, framebuffer);
    goto render_log_sample;
  }

  if (game->in_report) {
    const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
    reports_render(
      game->reports_ok ? &game->reports : NULL,
      game->report_id,
      &game->colonies,
      game->units_ok ? &game->units : NULL,
      game->world_map_ok ? &game->world_map : NULL,
      game->europe_ok ? &game->europe : NULL,
      game->col1_ok ? &game->col1 : NULL,
      game->human_nation,
      game->map_cursor_x,
      game->map_cursor_y,
      game->turn_number,
      font,
      framebuffer
    );
    goto render_log_sample;
  }

  if (game->in_colony) {
    render_colony_screen(game, framebuffer);
    goto render_log_sample;
  }

  if (game->in_pedia) {
    render_pedia_screen(game, framebuffer);
    goto render_log_sample;
  }

  if (game->in_debug_atlas) {
    const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
    debug_atlas_render(&game->debug_atlas, font, framebuffer);
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
    game_render_begin_menu(game, framebuffer);
    goto render_log_sample;
  }

  /* Map view: scrollable world map (13×11 tiles) left of the right info panel. */
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);

  const int tile_w = 16;
  const int tile_h = 16;
  const int map_origin_x = 0;
  const int map_origin_y = MAP_MENU_BAR_H;
  const int view_cols = MAP_VIEW_TILE_COLS;
  const int view_rows = MAP_VIEW_TILE_ROWS;

  int view_x = game->map_view_x - view_cols / 2;
  int view_y = game->map_view_y - view_rows / 2;
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
        int underlayer = -1;
        int coast_layers = 0;
        if (game->world_map_ok) {
          const int mx = view_x + sx;
          const int my = view_y + sy;
          if (mx < 0 || my < 0 || mx >= game->world_map.width || my >= game->world_map.height) {
            continue;
          }
          underlayer = map_coast_underlayer_sprite_at(&game->world_map, mx, my);
          coast_layers = map_phys0_coast_layer_count(&game->world_map, mx, my);
          base_sprite = (underlayer >= 0) ? underlayer : map_terrain_sprite_at(&game->world_map, mx, my);
        } else {
          base_sprite = (view_x + sx + view_y + sy + (int)game->map_seed) % game->terrain.sprite_count;
        }
        if (base_sprite < 0 || base_sprite >= game->terrain.sprite_count) {
          base_sprite = 0;
        }
        blit_map_sprite(
          &game->terrain, base_sprite, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
        );

        if (game->phys0_ok && game->world_map_ok) {
          const int mx = view_x + sx;
          const int my = view_y + sy;
          /* MAPEDIT: land transitions before forest (FUN_1a47_06da). */
          if (underlayer < 0) {
            const int transitions = map_land_transition_count(&game->world_map, mx, my);
            for (int ti = 0; ti < transitions; ++ti) {
              const int mask = map_land_transition_mask_sprite_at(&game->world_map, mx, my, ti);
              const int fill = map_land_transition_fill_terrain_at(&game->world_map, mx, my, ti);
              if (mask >= 0) {
                blit_map_sprite(
                  &game->phys0, mask, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
                );
              }
              if (fill >= 0 && fill < game->terrain.sprite_count) {
                blit_map_sprite_where_dest(
                  &game->terrain,
                  fill,
                  framebuffer,
                  sx,
                  sy,
                  tile_w,
                  tile_h,
                  map_origin_x,
                  map_origin_y,
                  0
                );
              }
            }
          }
          const int forest_sprite = map_phys0_forest_sprite_at(&game->world_map, mx, my);
          if (forest_sprite >= 0) {
            blit_map_sprite(
              &game->phys0, forest_sprite, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
            );
          }
          const int overlay_layers = map_phys0_overlay_count(&game->world_map, mx, my);
          /* MAPEDIT: coast PHYS0, then masked ocean into colour-0 holes, then estuary. */
          const int coast_end = (underlayer >= 0) ? coast_layers : overlay_layers;
          for (int layer = 0; layer < coast_end; ++layer) {
            const int overlay_sprite = map_phys0_overlay_sprite_at(&game->world_map, mx, my, layer);
            if (overlay_sprite >= 0) {
              int ox = 0;
              int oy = 0;
              map_phys0_overlay_offset_at(&game->world_map, mx, my, layer, &ox, &oy);
              blit_map_sprite_offset(
                &game->phys0,
                overlay_sprite,
                framebuffer,
                sx,
                sy,
                tile_w,
                tile_h,
                map_origin_x,
                map_origin_y,
                ox,
                oy
              );
            }
          }
          if (underlayer >= 0) {
            const int ocean_sprite = map_terrain_sprite_at(&game->world_map, mx, my);
            if (ocean_sprite >= 0 && ocean_sprite < game->terrain.sprite_count) {
              blit_map_sprite_where_dest(
                &game->terrain,
                ocean_sprite,
                framebuffer,
                sx,
                sy,
                tile_w,
                tile_h,
                map_origin_x,
                map_origin_y,
                0
              );
            }
            for (int layer = coast_layers; layer < overlay_layers; ++layer) {
              const int overlay_sprite = map_phys0_overlay_sprite_at(&game->world_map, mx, my, layer);
              if (overlay_sprite >= 0) {
                int ox = 0;
                int oy = 0;
                map_phys0_overlay_offset_at(&game->world_map, mx, my, layer, &ox, &oy);
                blit_map_sprite_offset(
                  &game->phys0,
                  overlay_sprite,
                  framebuffer,
                  sx,
                  sy,
                  tile_w,
                  tile_h,
                  map_origin_x,
                  map_origin_y,
                  ox,
                  oy
                );
              }
            }
          }
        }
      }
    }
  } else {
    for (int y = map_origin_y; y < framebuffer->height; ++y) {
      for (int x = 0; x < framebuffer->width; ++x) {
        const int idx = y * framebuffer->width + x;
        uint8_t base = (uint8_t)(((x / 8) ^ (y / 8) ^ (int)game->turn_number) & 0x0f);
        framebuffer->pixels[idx] = (uint8_t)(16 + ((base + game->map_seed) & 0x0f));
      }
    }
  }

  if (game->units_ok && game->unit_icons_ok) {
    units_render_on_map(
      &game->units,
      &game->unit_icons,
      framebuffer,
      view_x,
      view_y,
      view_cols,
      view_rows,
      tile_w,
      tile_h,
      map_origin_x,
      map_origin_y
    );
  }

  if (game->colonies_ok || game->colonies.colony_count > 0) {
    colonies_render_on_map(
      &game->colonies,
      game->unit_icons_ok ? &game->unit_icons : NULL,
      framebuffer,
      game->menu_font_ok ? &game->menu_font : NULL,
      view_x,
      view_y,
      view_cols,
      view_rows,
      tile_w,
      tile_h,
      map_origin_x,
      map_origin_y
    );
  }

  /*
   * Map tile cursor: blinking white outline only in tile-select mode (no unit selected).
   * CURSOR.SS is the OS mouse pointer, not a tile overlay.
   */
  if (game->units.selected_id < 0) {
    const int sx = game->map_cursor_x - view_x;
    const int sy = game->map_cursor_y - view_y;
    if (sx >= 0 && sy >= 0 && sx < view_cols && sy < view_rows) {
      const bool blink_on = ((game->elapsed_ms / 250u) % 2u) == 0u;
      if (blink_on) {
        const int cx0 = map_origin_x + sx * tile_w;
        const int cy0 = map_origin_y + sy * tile_h;
        for (int y = cy0; y < cy0 + tile_h; ++y) {
          for (int x = cx0; x < cx0 + tile_w; ++x) {
            if (x < 0 || y < 0 || x >= framebuffer->width || y >= framebuffer->height) {
              continue;
            }
            if (x == cx0 || x == cx0 + tile_w - 1 || y == cy0 || y == cy0 + tile_h - 1) {
              framebuffer->pixels[y * framebuffer->width + x] = 15;
            }
          }
        }
      }
    }
  }

  if (game->map_panel_ok) {
    const ColonizeFont* panel_font = game->colony_font_ok ? &game->colony_font :
                                     (game->menu_font_ok ? &game->menu_font : NULL);
    map_panel_render(
      &game->map_panel,
      game->world_map_ok ? &game->world_map : NULL,
      game->units_ok ? &game->units : NULL,
      game->colonies_ok || game->colonies.colony_count > 0 ? &game->colonies : NULL,
      game->unit_icons_ok ? &game->unit_icons : NULL,
      panel_font,
      game->names_ok ? &game->names : NULL,
      game->labels_ok ? &game->labels : NULL,
      game->col1_ok ? &game->col1 : NULL,
      view_x,
      view_y,
      view_cols,
      view_rows,
      game->map_cursor_x,
      game->map_cursor_y,
      game->units.selected_id,
      game->game_year,
      game->game_autumn,
      game->europe.gold,
      game->europe.tax_percent,
      game->europe.nation_name,
      framebuffer
    );
  }

  const ColonizeFont* hud_font = game->colony_font_ok ? &game->colony_font :
                                 (game->menu_font_ok ? &game->menu_font : NULL);
  if (!game->in_menu && !game->in_colony && !game->in_europe && !game->in_pedia &&
      !game->in_debug_atlas && !game->in_report) {
    const ColonizeSpriteSheet* wood =
      (game->map_panel_ok && game->map_panel.wood_ok) ? &game->map_panel.wood_tile : NULL;
    map_menu_render((MapMenuBar*)&game->map_menu, hud_font, wood, framebuffer);
    if (game->pick_music.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      pick_music_render(
        (PickMusicDialog*)&game->pick_music,
        hud_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
  }

render_log_sample:
  if (!game->in_menu && turn_processor_show_indicator(&game->turn_proc)) {
    turn_draw_owner_indicator(framebuffer, game->active_turn_nation);
  }
  render_log_counter++;
  if (render_log_counter == 1 || render_log_counter == 60 || render_log_counter == 300) {
    const int cx = framebuffer->width / 2;
    const int cy = framebuffer->height / 2;
    const int idx_center = cy * framebuffer->width + cx;
    diag_info(
      "Framebuffer sample frame=%u mode=%s idx0=%u idx_center=%u turn=%u cursor=%d,%d",
      render_log_counter,
      render_mode_name(game),
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

void game_apply_mouse_cursor(
  ColonizeGameState* game,
  ColonizePlatform* platform,
  int mouse_x,
  int mouse_y
) {
  if (!game || !platform) {
    return;
  }

  /* Build the game pointer once from CURSOR.SS #0 (arrow tip near 1,0). */
  if (game->cursor_ok && game->cursor.sprite_count > 0 && !game->mouse_cursor_built) {
    const ColonizeSprite* sp = &game->cursor.sprites[0];
    const ColonizePalette* pal =
      game->map_palette_ok ? &game->map_palette : (game->palette_ok ? &game->palette : NULL);
    if (sp->pixels && pal &&
        platform_set_mouse_cursor_indexed(
          platform, sp->pixels, sp->width, sp->height, 1, 0, pal
        )) {
      game->mouse_cursor_built = true;
    }
  }

  /* Game cursor over the whole 320x200 frame (map + top menu bar). */
  const bool on_game_frame = !game->in_menu && !game->in_debug_atlas && !game->in_pedia &&
    !game->in_europe && !game->in_colony && !game->in_report &&
    mouse_x >= 0 && mouse_x < 320 && mouse_y >= 0 && mouse_y < 200;

  if (on_game_frame && game->mouse_cursor_built) {
    platform_show_game_mouse_cursor(platform, true);
  } else {
    platform_show_game_mouse_cursor(platform, false);
  }
}
