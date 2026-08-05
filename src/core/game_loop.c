#include "core/game_loop.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/ai.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_screen.h"
#include "core/debug_atlas.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/map.h"
#include "core/map_gen.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/new_game.h"
#include "core/pedia.h"
#include "core/pik.h"
#include "core/pick_music.h"
#include "core/popup.h"
#include "core/reports.h"
#include "core/savegame.h"
#include "core/sound.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/turn.h"
#include "core/ui_button.h"
#include "core/ui_colors.h"
#include "core/ui_drag.h"
#include "core/unit_chrome.h"
#include "core/unit_stack.h"
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
  NewGameWizard new_game;
  int difficulty;
  char leader_name[NEW_GAME_LEADER_NAME_MAX];
  bool assets_ok;
  bool palette_ok;
  ColonizePalette palette;
  ColonizeMsgCatalog messages;
  ColonizeMsgCatalog map_menu_txt;
  MapMenuBar map_menu;
  PickMusicDialog pick_music;
  UnitStackPopup unit_stack;
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
  int debug_mouse_x;       /* last pointer in 320×200 framebuffer space */
  int debug_mouse_y;
  bool debug_show_mouse_coords; /* DEBUG menu toggle; default on */
  int cheat_unlock_step;   /* 0=expect W, 1=I, 2=N for Alt-WIN */
  UiDragSession ui_drag;
  int map_goto_anchor_x; /* tile under pointer when map goto drag began */
  int map_goto_anchor_y;
  bool map_goto_left_tile; /* true once pointer leaves the anchor tile */
  int map_goto_down_px; /* logical 320×200 mouse at drag begin */
  int map_goto_down_py;
  bool map_goto_dragged_px; /* true once pointer moved ≥1 logical pixel */
  uint32_t goto_step_accum_ms; /* paces Go-To at 10 steps/sec */
  ColonizeDosRng move_rng; /* FUN_465b partial-overspend rolls */
  uint32_t ai_rng_seed; /* FUN_281f_04ca timer word; VR_SEED = 100 */
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

static void set_status(ColonizeGameState* game, const char* prefix, const char* detail);
static void activate_menu_selection(ColonizeGameState* game);

typedef struct BeginMenuLayout {
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int inner_x;
  int inner_y;
  int inner_w;
  int inner_h;
  int list_y0;
  int line_h;
  int title_h;
  int title_pad_top;
  int title_pad_x;
  int option_pad_x;
  int gap_after_title;
  int option_count;
} BeginMenuLayout;

static bool begin_menu_compute_layout(
  const ColonizeGameState* game,
  int fb_w,
  int fb_h,
  BeginMenuLayout* out
);
static int begin_menu_option_at_xy(const BeginMenuLayout* layout, int mx, int my);

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
    case COLONIZE_KEY_F: return "F";
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
    case COLONIZE_KEY_W: return "W";
    case COLONIZE_KEY_I: return "I";
    case COLONIZE_KEY_N: return "N";
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
    case COLONIZE_KEY_BACKSPACE: return "Backspace";
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
  if (new_game_active(&game->new_game)) {
    return "new-game";
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
  /* Continue LCG for FUN_465b / AI nation turns. VR_SEED fixtures use seed 100;
   * prefer that when the save looks like a seed-100 NEW WORLD start (turn<=6,
   * 34 tribes), else fall back to turn/year. */
  {
    uint32_t seed = game->turn_number ? game->turn_number
                                      : (game->game_year ? game->game_year : 1u);
    if (game->col1_ok && game->col1.head.tribe_count == 34 && game->col1.head.turn <= 6) {
      seed = 100u;
    }
    dos_rng_seed(&game->move_rng, seed);
    game->ai_rng_seed = seed;
  }
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
    /* Apply new-game wizard identity onto the template. */
    for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT; ++i) {
      game->col1.player[i].control = 1;
    }
    const int hn = game->human_nation;
    if (hn >= 0 && hn < (int)COLONIZE_COL1_NATION_COUNT) {
      game->col1.player[hn].control = 0;
      str_copy_trunc(
        game->col1.player[hn].name,
        sizeof(game->col1.player[hn].name),
        game->leader_name[0] ? game->leader_name : "Governor"
      );
      str_copy_trunc(
        game->col1.player[hn].country_name,
        sizeof(game->col1.player[hn].country_name),
        new_game_nation_name(hn)
      );
    }
    game->col1.head.difficulty = (uint8_t)game->difficulty;
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

static void game_open_debug_atlas(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game->in_debug_atlas = true;
  game->in_pedia = false;
  game->in_europe = false;
  game->in_colony = false;
  game->in_report = false;
  if (game->debug_atlas.count <= 0) {
    debug_atlas_scan(&game->debug_atlas, game->resolved_data_dir);
  }
  debug_atlas_load(&game->debug_atlas, game->resolved_data_dir, 0);
  diag_info("Entered graphic atlas debug (%d files).", game->debug_atlas.count);
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
    if (page.category == PEDIA_CAT_UNIT) {
      const ColonizeFont* chrome_font = game->colony_font_ok ? &game->colony_font
        : (game->menu_font_ok ? &game->menu_font : NULL);
      unit_chrome_blit_unit(
        framebuffer,
        chrome_font,
        &game->unit_icons,
        page.icon_sprite,
        preview_x,
        preview_y,
        page.index,
        game->human_nation,
        UNITS_ORDER_NONE,
        false,
        false
      );
    } else {
      ss_blit_sprite(&game->unit_icons, page.icon_sprite, framebuffer, preview_x, preview_y);
    }
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

static void europe_fill_rect(
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
  for (int y = y0; y < y1; ++y) {
    if (y < 0 || y >= fb->height) {
      continue;
    }
    for (int x = x0; x < x1; ++x) {
      if (x < 0 || x >= fb->width) {
        continue;
      }
      fb->pixels[y * fb->width + x] = color;
    }
  }
}

static void europe_draw_box_border(
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int w,
  int h,
  uint8_t color
) {
  europe_fill_rect(fb, x, y, x + w, y + 1, color);
  europe_fill_rect(fb, x, y + h - 1, x + w, y + h, color);
  europe_fill_rect(fb, x, y, x + 1, y + h, color);
  europe_fill_rect(fb, x + w - 1, y, x + w, y + h, color);
}

/* Two-line header + ship icons inside an Expected/Bound/Loading water box. */
static int europe_ship_icon_sprite(const ColonizeUnitPool* units, const EuropeHarborShip* ship) {
  if (!units || !ship) {
    return -1;
  }
  int ti = ship->type_index;
  if (ti < 0) {
    ti = units_find_type(units, ship->name);
  }
  const ColonizeUnitType* ut = units_type(units, ti);
  return ut ? ut->icon_sprite : -1;
}

static void europe_render_transit_box(
  const ColonizeGameState* game,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int box_x,
  int box_y,
  int box_w,
  int box_h,
  const char* header,
  const EuropeHarborShip* ships,
  int count,
  int selected_index
) {
  if (!framebuffer) {
    return;
  }
  const int line_h = font ? (font->max_height > 0 ? (int)font->max_height + 2 : 8) : 8;
  const int header_h = EUROPE_TRANSIT_HEADER_LINES * line_h;
  font_draw_text(font, framebuffer, box_x + 2, box_y + 2, header, EUROPE_TEXT_GREEN);

  if (!game || !game->unit_icons_ok || !game->units_ok || count <= 0 || !ships) {
    return;
  }

  const int ship_y0 = box_y + 2 + header_h + 10;
  const int ship_area_h = box_y + box_h - ship_y0 - 1;
  if (ship_area_h < 8) {
    return;
  }

  int x = box_x + 3;
  int y = ship_y0;
  int row_h = 0;
  for (int i = 0; i < count; ++i) {
    const int sprite = europe_ship_icon_sprite(&game->units, &ships[i]);
    if (sprite < 0 || sprite >= game->unit_icons.sprite_count) {
      continue;
    }
    const ColonizeSprite* sp = &game->unit_icons.sprites[sprite];
    const int sw = sp->width > 0 ? sp->width : 14;
    const int sh = sp->height > 0 ? sp->height : 16;
    if (x + sw > box_x + box_w - 2) {
      x = box_x + 3;
      y += row_h + 1;
      row_h = 0;
      if (y + sh > box_y + box_h - 1) {
        break;
      }
    }
    if (sh > row_h) {
      row_h = sh;
    }
    unit_chrome_blit_unit(
      framebuffer,
      font,
      &game->unit_icons,
      sprite,
      x,
      y,
      ships[i].type_index,
      game->human_nation,
      UNITS_ORDER_NONE,
      ships[i].cargo_count > 0,
      false
    );
    if (i == selected_index) {
      europe_draw_box_border(framebuffer, x - 1, y - 1, sw + 2, sh + 2, 14);
    }
    x += sw + 2;
  }
}

/* RECRUIT/TRAIN/PURCHASE/DOCK wood popup — chrome via popup_draw. */
static void europe_render_menu_popup(
  const ColonizeGameState* game,
  ColonizeFramebuffer8* framebuffer
) {
  const EuropeScreen* eu = &game->europe;
  if (eu->menu == EUROPE_MENU_NONE) {
    return;
  }
  const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
  const int line_h = font ? (font->max_height + 2) : 9;
  const int pad = 4;

  int rows = 0;
  char title[64];
  switch (eu->menu) {
    case EUROPE_MENU_RECRUIT:
      rows = 1 + EUROPE_POOL_SIZE;
      snprintf(title, sizeof(title), "Recruit (passage %d$)", eu->recruit_passage);
      break;
    case EUROPE_MENU_TRAIN:
      rows = 1 + eu->train_count;
      snprintf(title, sizeof(title), "%s", "The Royal University");
      break;
    case EUROPE_MENU_PURCHASE:
      rows = 1 + eu->purchase_count;
      snprintf(title, sizeof(title), "%s", "Purchase");
      break;
    case EUROPE_MENU_DOCK:
      rows = 4;
      snprintf(title, sizeof(title), "%s", "Dock orders");
      break;
    default:
      return;
  }

  int dialog_w = 220;
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h + rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  const int dialog_x = (framebuffer->width - dialog_w) / 2;
  const int dialog_y = 16;

  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
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
    (eu->wood_tile_ok) ? &eu->wood_tile : NULL,
    &colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  if (inner_w <= 0 || inner_h <= 0) {
    return;
  }

  font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, title, 15);
  const int list_y0 = inner_y + pad + line_h;
  static const char* k_dock_opts[4] = {"None", "Don't board", "Board next", "Move to front"};

  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    if (row_y + line_h > framebuffer->height) {
      break;
    }
    if (i == eu->menu_selection) {
      europe_fill_rect(
        framebuffer, inner_x + 1, row_y - 1, inner_x + inner_w - 1, row_y + line_h - 1, 138
      );
    }
    char label[64];
    uint8_t color = 15;
    if (i == 0) {
      snprintf(label, sizeof(label), "%s", "None");
    } else if (eu->menu == EUROPE_MENU_RECRUIT) {
      const EuropePoolSlot* p = &eu->pool[i - 1];
      snprintf(label, sizeof(label), "%s", p->filled ? p->name : "(empty)");
    } else if (eu->menu == EUROPE_MENU_TRAIN) {
      const EuropeTrainOption* t = &eu->train[i - 1];
      snprintf(label, sizeof(label), "%s (Cost: %d)", t->expert_name, t->cost);
      color = (eu->gold >= t->cost) ? 14 : 8;
    } else if (eu->menu == EUROPE_MENU_PURCHASE) {
      const EuropePurchaseOption* p = &eu->purchase[i - 1];
      snprintf(label, sizeof(label), "%s (Cost: %d)", p->name, p->gold);
    } else if (eu->menu == EUROPE_MENU_DOCK && i >= 0 && i < 4) {
      snprintf(label, sizeof(label), "%s", k_dock_opts[i]);
    }
    font_draw_text(font, framebuffer, inner_x + pad, row_y, label, color);
  }
}

static void europe_tile_wood(
  const ColonizeSpriteSheet* sheet,
  int origin_x,
  int origin_y,
  int rect_w,
  int rect_h,
  ColonizeFramebuffer8* framebuffer
) {
  if (!sheet || sheet->sprite_count < 1 || !framebuffer || rect_w <= 0 || rect_h <= 0) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[0];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int x1 = origin_x + rect_w;
  const int y1 = origin_y + rect_h;
  for (int y = origin_y; y < y1; y += tile->height) {
    for (int x = origin_x; x < x1; x += tile->width) {
      for (int sy = 0; sy < tile->height; ++sy) {
        const int fy = y + sy;
        if (fy < origin_y || fy >= y1 || fy < 0 || fy >= framebuffer->height) {
          continue;
        }
        for (int sx = 0; sx < tile->width; ++sx) {
          const int fx = x + sx;
          if (fx < origin_x || fx >= x1 || fx < 0 || fx >= framebuffer->width) {
            continue;
          }
          const uint8_t color = tile->pixels[sy * tile->width + sx];
          if (color == COLONIZE_SS_TRANSPARENT) {
            continue;
          }
          framebuffer->pixels[fy * framebuffer->width + fx] = color;
        }
      }
    }
  }
}

static int europe_dock_sprite(const ColonizeUnitPool* units, const EuropeDockImmigrant* d) {
  if (!units || !d || !d->name[0]) {
    return -1;
  }
  if (strcmp(d->name, "Artillery") == 0) {
    const int ti = units_find_type(units, "Artillery");
    const ColonizeUnitType* ut = units_type(units, ti);
    return ut ? ut->icon_sprite : -1;
  }
  int ti = units_find_type(units, d->name);
  if (ti < 0) {
    ti = units_find_type(units, "Colonists");
  }
  if (ti < 0) {
    return -1;
  }
  const ColonizeUnitType* ut = units_type(units, ti);
  if (ut && strstr(ut->name, "Colonist") != NULL) {
    return units_working_colonist_sprite(units, ti, d->profession);
  }
  return ut ? ut->icon_sprite : -1;
}

static int europe_harbor_open_holds(const ColonizeUnitPool* units, const EuropeHarborShip* ship) {
  if (!units || !ship) {
    return 0;
  }
  int ti = ship->type_index;
  if (ti < 0) {
    ti = units_find_type(units, ship->name);
  }
  const ColonizeUnitType* ut = units_type(units, ti);
  if (!ut || ut->cargo <= 0) {
    return 0;
  }
  return ut->cargo > EUROPE_HOLD_MAX ? EUROPE_HOLD_MAX : ut->cargo;
}

static void render_europe_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer) {
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);
  /* Main Europe chrome uses FONTTINY; popups keep menu_font (see europe_render_menu_popup). */
  const ColonizeFont* font = game->colony_font_ok ? &game->colony_font
    : (game->menu_font_ok ? &game->menu_font : NULL);
  EuropeScreen* eu_mut = game->europe_ok ? (EuropeScreen*)&game->europe : NULL;
  const EuropeScreen* eu = &game->europe;
  if (eu_mut) {
    europe_refresh_harbor_selection(eu_mut);
  }

  if (game->europe_ok && eu->background_ok) {
    pik_blit(&eu->background, framebuffer, 0, 0);
  }

  /* Colony-style wood top bar + black separator; info centered. */
  if (eu->wood_tile_ok) {
    europe_tile_wood(&eu->wood_tile, 0, 0, framebuffer->width, EUROPE_TOP_BAR_H, framebuffer);
  } else {
    europe_fill_rect(framebuffer, 0, 0, framebuffer->width, EUROPE_TOP_BAR_H, 6);
  }
  europe_fill_rect(
    framebuffer, 0, EUROPE_TOP_SEPARATOR_Y, framebuffer->width, EUROPE_TOP_SEPARATOR_Y + 1, 0
  );

  char line[192];
  if (game->game_year > 0) {
    char date[32];
    turn_format_date(game->game_year, game->game_autumn, date, sizeof(date));
    snprintf(
      line,
      sizeof(line),
      "%s, %s.  %s.  Tax: %d%%  Gold: %d$",
      eu->port_city,
      eu->nation_name,
      date,
      eu->tax_percent,
      eu->gold
    );
  } else {
    snprintf(
      line,
      sizeof(line),
      "%s, %s.  Tax: %d%%  Gold: %d$",
      eu->port_city,
      eu->nation_name,
      eu->tax_percent,
      eu->gold
    );
  }
  {
    const int tw = font_text_width(font, line);
    const int th = font ? (font->max_height > 0 ? (int)font->max_height : 6) : 7;
    const int tx = (framebuffer->width - tw) / 2;
    const int ty = (EUROPE_TOP_BAR_H - th) / 2;
    font_draw_text(font, framebuffer, tx > 0 ? tx : 0, ty > 0 ? ty : 1, line, EUROPE_TEXT_GREEN);
  }

  /* Transit boxes: two-line headers + ship icons (not text lists). */
  europe_render_transit_box(
    game,
    font,
    framebuffer,
    EUROPE_EXPECTED_X,
    EUROPE_EXPECTED_Y,
    EUROPE_EXPECTED_W,
    EUROPE_EXPECTED_H,
    "Expected\nSoon",
    eu->expected,
    eu->expected_ships,
    -1
  );

  snprintf(
    line,
    sizeof(line),
    "Bound For\n%s",
    eu->colony_region[0] ? eu->colony_region : "New World"
  );
  europe_render_transit_box(
    game,
    font,
    framebuffer,
    EUROPE_BOUND_X,
    EUROPE_BOUND_Y,
    EUROPE_BOUND_W,
    EUROPE_BOUND_H,
    line,
    eu->bound,
    eu->bound_ships,
    -1
  );

  if (eu->selected_harbor >= 0 && eu->selected_harbor < eu->harbor_ships) {
    snprintf(line, sizeof(line), "Loading:\n%s", eu->harbor[eu->selected_harbor].name);
  } else if (eu->harbor_ships > 0) {
    snprintf(line, sizeof(line), "%s", "Loading:\n");
  } else {
    snprintf(line, sizeof(line), "%s", "Loading:\n");
  }
  europe_render_transit_box(
    game,
    font,
    framebuffer,
    EUROPE_LOADING_X,
    EUROPE_LOADING_Y,
    EUROPE_LOADING_W,
    EUROPE_LOADING_H,
    line,
    eu->harbor,
    eu->harbor_ships,
    eu->selected_harbor
  );

  /* Commodity holds — same closed/open cover behavior as colony transport pane. */
  {
    const EuropeHarborShip* ship = NULL;
    int open_holds = 0;
    if (eu->selected_harbor >= 0 && eu->selected_harbor < eu->harbor_ships) {
      ship = &eu->harbor[eu->selected_harbor];
      open_holds = game->units_ok ? europe_harbor_open_holds(&game->units, ship) : 0;
    }
    if (ship && open_holds > 0) {
      for (int i = 0; i < open_holds; ++i) {
        const int x = EUROPE_HOLD_X + i * EUROPE_HOLD_PITCH;
        const int hy = EUROPE_HOLD_Y;
        const int amt = ship->hold_goods_amount[i];
        const int gtype = ship->hold_goods_type[i];
        if (amt > 0 && amt < 255 && gtype >= 0 && gtype < COLONIZE_CARGO_COUNT &&
            game->unit_icons_ok) {
          const int sprite = EUROPE_CARGO_ICON_BASE + gtype;
          if (sprite < game->unit_icons.sprite_count) {
            const ColonizeSprite* sp = &game->unit_icons.sprites[sprite];
            const int ix = x + (EUROPE_HOLD_W - sp->width) / 2;
            ss_blit_sprite(&game->unit_icons, sprite, framebuffer, ix, hy);
          }
        }
      }
      for (int i = 0; i < ship->cargo_count; ++i) {
        const int slot = open_holds + i;
        if (slot >= EUROPE_HOLD_MAX) {
          break;
        }
        const int x = EUROPE_HOLD_X + slot * EUROPE_HOLD_PITCH;
        const int hy = EUROPE_HOLD_Y;
        const int pax_type = ship->cargo_types[i];
        if (game->unit_icons_ok && pax_type >= 0) {
          const ColonizeUnitType* ut = units_type(&game->units, pax_type);
          if (ut && ut->icon_sprite >= 0 && ut->icon_sprite < game->unit_icons.sprite_count) {
            ss_blit_sprite(&game->unit_icons, ut->icon_sprite, framebuffer, x, hy);
          }
        }
      }
    }
    if (game->unit_icons_ok && EUROPE_ICON_EMPTY_HOLD < game->unit_icons.sprite_count) {
      const ColonizeSprite* cov = &game->unit_icons.sprites[EUROPE_ICON_EMPTY_HOLD];
      const int cover_w = (cov && cov->width > 0) ? cov->width : EUROPE_HOLD_W;
      const int cover_h = (cov && cov->height > 0) ? cov->height : 12;
      for (int i = open_holds; i < EUROPE_HOLD_MAX; ++i) {
        const int x =
          EUROPE_HOLD_X + i * EUROPE_HOLD_PITCH + (EUROPE_HOLD_W - cover_w) / 2;
        const int y = EUROPE_HOLD_Y + (EUROPE_HOLD_H - cover_h) / 2;
        ss_blit_sprite(&game->unit_icons, EUROPE_ICON_EMPTY_HOLD, framebuffer, x, y);
      }
    }
  }

  /* Dock colonists as unit sprites from (235,140). */
  if (game->units_ok && game->unit_icons_ok) {
    for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
      const int dx = EUROPE_DOCK_X + i * EUROPE_DOCK_PITCH;
      const int dy = EUROPE_DOCK_Y;
      if (dx + EUROPE_DOCK_PITCH > framebuffer->width) {
        break;
      }
      const int sprite = europe_dock_sprite(&game->units, &eu->dock[i]);
      if (sprite >= 0 && sprite < game->unit_icons.sprite_count) {
        const ColonizeSprite* sp = &game->unit_icons.sprites[sprite];
        const int iw = sp->width > 0 ? sp->width : 16;
        const int ih = sp->height > 0 ? sp->height : 16;
        const int orders =
          eu->dock[i].sentry ? UNITS_ORDER_SENTRY : UNITS_ORDER_NONE;
        /* Any dock immigrant shows the multi-unit tab when the queue has >1. */
        const bool stacked = eu->dock_count > 1;
        int dtype = units_find_type(&game->units, eu->dock[i].name);
        if (dtype < 0) {
          dtype = units_find_type(&game->units, "Colonists");
        }
        if (dtype < 0) {
          dtype = 0;
        }
        unit_chrome_blit_unit(
          framebuffer,
          font,
          &game->unit_icons,
          sprite,
          dx,
          dy,
          dtype,
          game->human_nation,
          orders,
          stacked,
          false
        );
        if (eu->menu == EUROPE_MENU_DOCK && eu->menu_dock_index == i) {
          int fx = 0;
          int fy = 0;
          int fw = 0;
          int fh = 0;
          unit_chrome_selection_frame(dx, dy, iw, ih, &fx, &fy, &fw, &fh);
          europe_draw_box_border(framebuffer, fx, fy, fw, fh, 14);
        }
      }
    }
  }

  /* Market: 20x20 cells sharing borders; selection lights the cell. */
  for (int i = 0; i < eu->cargo_count && i < EUROPE_CARGO_MAX; ++i) {
    const int mx = EUROPE_MARKET_X + i * EUROPE_MARKET_PITCH;
    if (mx + EUROPE_MARKET_CELL > framebuffer->width) {
      break;
    }
    const bool sel = (i == eu->selected_market);
    const int sprite = EUROPE_CARGO_ICON_BASE + i;
    if (game->unit_icons_ok && sprite < game->unit_icons.sprite_count) {
      const ColonizeSprite* sp = &game->unit_icons.sprites[sprite];
      const int ix = mx + (EUROPE_MARKET_CELL - sp->width) / 2;
      const int iy = EUROPE_MARKET_Y + 1;
      ss_blit_sprite(&game->unit_icons, sprite, framebuffer, ix, iy);
    }
    snprintf(line, sizeof(line), "%d/%d", eu->cargo[i].bid, eu->cargo[i].ask);
    {
      const int tw = font_text_width(font, line);
      const int th = font ? (font->max_height > 0 ? (int)font->max_height : 6) : 7;
      const int tx = mx + (EUROPE_MARKET_CELL - tw) / 2;
      const int ty = EUROPE_MARKET_Y + EUROPE_MARKET_CELL - th - 1;
      font_draw_text(font, framebuffer, tx, ty, line, 0);
    }
    if (sel) {
      europe_draw_box_border(
        framebuffer, mx, EUROPE_MARKET_Y, EUROPE_MARKET_CELL, EUROPE_MARKET_CELL + 1, 14
      );
    }
  }

  /* RECRUIT / PURCHASE / TRAIN — transparent beveled ALL-CAPS buttons. */
  {
    static const char* k_btn_labels[3] = {"~RECRUIT", "~PURCHASE", "~TRAIN"};
    UiButtonColors bc;
    bc.dark = EUROPE_BTN_DARK;
    bc.light = EUROPE_BTN_LIGHT;
    bc.text = 15;
    bc.hotkey = 14;
    for (int i = 0; i < 3; ++i) {
      int bw = EUROPE_BTN_W;
      int bh = EUROPE_BTN_H;
      ui_button_measure(font, k_btn_labels[i], &bw, &bh);
      if (bw < EUROPE_BTN_W) {
        bw = EUROPE_BTN_W;
      }
      ui_button_draw(
        font,
        framebuffer,
        EUROPE_BTN_X,
        EUROPE_BTN_Y + i * EUROPE_BTN_PITCH,
        bw,
        bh,
        k_btn_labels[i],
        &bc
      );
    }
  }

  europe_render_menu_popup(game, framebuffer);

  if (!game->europe_ok) {
    font_draw_text(font, framebuffer, 4, 100, "EUROPE.PIK / NAMES.TXT failed to load", 12);
  }
}

static void render_colony_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer) {
  const ColonizeColony* colony = colonies_get(&game->colonies, game->colony_view_id);
  const ColonizeFont* font = game->colony_font_ok
    ? &game->colony_font
    : (game->menu_font_ok ? &game->menu_font : NULL);
  /* View is mutated for UI scratch (deltas / highlight); game pointer stays const. */
  colony_screen_render(
    game->colony_screen_ok ? (ColonyScreenView*)&game->colony_screen : NULL,
    &game->colonies,
    colony,
    game->units_ok ? &game->units : NULL,
    game->world_map_ok ? &game->world_map : NULL,
    game->terrain_ok ? &game->terrain : NULL,
    game->phys0_ok ? &game->phys0 : NULL,
    game->col1_ok ? &game->col1 : NULL,
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
  game->difficulty = 0;
  snprintf(game->leader_name, sizeof(game->leader_name), "Walter Raleigh");
  game->game_year = 1492;
  game->game_autumn = 0;
  game->human_nation = 0;
  game->active_turn_nation = 0;
  dos_rng_seed(&game->move_rng, 1u);
  game->ai_rng_seed = 1u;
  game->pedia_category = PEDIA_CAT_TERRAIN;
  game->pedia_index = 0;
  game->pedia_hover_entry = -1;
  game->pedia_view = PEDIA_VIEW_LIST;
  game->pedia_return_to_list = false;
  game->pedia_father_loaded = -1;
  game->debug_show_mouse_coords = true;
  game->cheat_unlock_step = 0;
  col1_save_init(&game->col1);
  game->col1_ok = false;

  assets_msg_init(&game->messages);
  assets_msg_init(&game->map_menu_txt);
  assets_msg_init(&game->labels);
  assets_msg_init(&game->pedia);
  assets_msg_init(&game->names);
  map_menu_init(&game->map_menu);
  pick_music_init(&game->pick_music);
  new_game_init(&game->new_game);
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
      unit_chrome_load_orders(&game->names);
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
  pick_music_close(&game->pick_music);
  new_game_free(&game->new_game);
  assets_msg_free(&game->pedia);
  assets_msg_free(&game->names);
  debug_atlas_free(&game->debug_atlas);
  dos_compat_shutdown();
  free(game);
}

static void game_commit_new_campaign(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  NewGameWizard* ng = &game->new_game;

  game->difficulty = ng->difficulty;
  game->human_nation = ng->nation;
  game->active_turn_nation = ng->nation;
  snprintf(game->leader_name, sizeof(game->leader_name), "%s", ng->leader_name);

  char err[256];
  map_free(&game->world_map);
  game->world_map_ok = false;

  char map_label[NEW_GAME_MAP_NAME_MAX];
  map_label[0] = '\0';

  ColonizeDosRng campaign_rng;
  memset(&campaign_rng, 0, sizeof(campaign_rng));
  bool share_campaign_rng = false;

  if (ng->generate_map || ng->path == NEW_GAME_PATH_NEW_WORLD ||
      ng->path == NEW_GAME_PATH_CUSTOMIZE) {
    uint32_t seed = ng->gen_params.seed;
    if (seed == 0) {
      seed = game->elapsed_ms ? game->elapsed_ms : 1u;
    }
    ng->gen_params.seed = seed;
    /*
     * DOS NEW WORLD: seed LCG → draw customize axes → reseed with same tick
     * before FUN_684c_08c0 (plan hypothesis 1). CUSTOMIZE keeps user axes;
     * only seed the LCG once for generate + tribe stream.
     */
    dos_rng_seed(&campaign_rng, seed);
    ng->gen_params.rng = &campaign_rng;
    share_campaign_rng = true;
    if (ng->path == NEW_GAME_PATH_NEW_WORLD) {
      /* Hypothesis 1: draw customize axes, then reseed before FUN_684c_08c0. */
      map_gen_params_random(&ng->gen_params, seed);
      dos_rng_seed(&campaign_rng, seed);
      ng->gen_params.rng = &campaign_rng;
    }
    game->world_map_ok = map_generate(&game->world_map, &ng->gen_params, err, sizeof(err));
    if (!game->world_map_ok) {
      diag_error("new world map_generate failed: %s", err);
    }
    snprintf(
      map_label,
      sizeof(map_label),
      "%s",
      ng->path == NEW_GAME_PATH_CUSTOMIZE ? "CUSTOMIZE" : "NEW WORLD"
    );
  } else {
    char map_path[512];
    if (!dos_compat_normalize_asset_path(game->resolved_data_dir, ng->map_file, map_path, sizeof(map_path))) {
      str_path_join(map_path, sizeof(map_path), game->resolved_data_dir, ng->map_file);
    }
    game->world_map_ok = map_load_mp(map_path, &game->world_map, err, sizeof(err));
    if (!game->world_map_ok) {
      diag_error("new game map load failed (%s): %s", ng->map_file, err);
      if (dos_compat_normalize_asset_path(game->resolved_data_dir, "AMER2.MP", map_path, sizeof(map_path))) {
        game->world_map_ok = map_load_mp(map_path, &game->world_map, err, sizeof(err));
      }
    }
    snprintf(map_label, sizeof(map_label), "%s", ng->map_file[0] ? ng->map_file : "AMER2.MP");
  }

  col1_save_free(&game->col1);
  game->col1_ok = false;
  game->game_year = 1492;
  game->game_autumn = 0;
  game->turn_number = 0;
  europe_reset_campaign_nation(&game->europe, game->human_nation);
  /* Wipe live colonies but restore @BUILDING / COLONY.TXT so founding can grant starters. */
  colonies_init(&game->colonies);
  game->colonies_ok = false;
  if (game->names_ok) {
    if (!colonies_load_buildings(&game->colonies, &game->names)) {
      diag_warn("Failed to reload @BUILDING after new campaign init");
    }
  }
  {
    char colony_txt[512];
    if (dos_compat_normalize_asset_path(
          game->resolved_data_dir, "COLONY.TXT", colony_txt, sizeof(colony_txt)
        )) {
      game->colonies_ok = colonies_load_names(&game->colonies, colony_txt);
    }
  }
  if (!game->colonies_ok) {
    game->colonies_ok = game->colonies.building_type_count > 0;
  }

  int sx = 39, sy = 10;
  if (ng->generate_map || ng->path == NEW_GAME_PATH_NEW_WORLD ||
      ng->path == NEW_GAME_PATH_CUSTOMIZE) {
    if (!map_gen_pick_start(
          &game->world_map, game->human_nation, -1, -1, 0, &sx, &sy
        )) {
      sx = game->world_map.width / 2;
      sy = game->world_map.height / 2;
    }
  } else {
    char stem[64];
    snprintf(stem, sizeof(stem), "%s", ng->map_file);
    char* dot = strrchr(stem, '.');
    if (dot) {
      *dot = '\0';
    }
    new_game_scenario_start(
      game->names_ok ? &game->names : NULL, stem, game->human_nation, &sx, &sy
    );
  }

  if (game->world_map_ok && game->units_ok) {
    units_new_world_start(
      &game->units, &game->world_map, sx, sy, game->human_nation, game->difficulty
    );
    if (game->units.selected_id >= 0) {
      const ColonizeUnit* u = units_get_const(&game->units, game->units.selected_id);
      if (u) {
        game->map_cursor_x = u->x;
        game->map_cursor_y = u->y;
        game->map_view_x = u->x;
        game->map_view_y = u->y;
      }
    } else {
      game->map_cursor_x = sx;
      game->map_cursor_y = sy;
      game->map_view_x = sx;
      game->map_view_y = sy;
    }

    char stem[64];
    stem[0] = '\0';
    if (ng->map_file[0] && ng->path == NEW_GAME_PATH_AMERICA) {
      snprintf(stem, sizeof(stem), "%s", ng->map_file);
      char* dot = strrchr(stem, '.');
      if (dot) {
        *dot = '\0';
      }
    }
    AiNewGameParams ai;
    memset(&ai, 0, sizeof(ai));
    ai.col1 = &game->col1;
    ai.col1_ok = &game->col1_ok;
    ai.map = &game->world_map;
    ai.units = &game->units;
    ai.europe = &game->europe;
    ai.names = game->names_ok ? &game->names : NULL;
    ai.data_dir = game->resolved_data_dir;
    ai.human_nation = game->human_nation;
    ai.difficulty = game->difficulty;
    ai.leader_name = game->leader_name;
    ai.use_tribe_txt = (ng->path == NEW_GAME_PATH_AMERICA);
    ai.map_stem = stem[0] ? stem : NULL;
    ai.human_start_x = sx;
    ai.human_start_y = sy;
    ai.rng_seed = game->elapsed_ms ? game->elapsed_ms : 1u;
    if (ng->path == NEW_GAME_PATH_NEW_WORLD || ng->path == NEW_GAME_PATH_CUSTOMIZE) {
      ai.rng_seed = ng->gen_params.seed ? ng->gen_params.seed : ai.rng_seed;
    }
    if (share_campaign_rng) {
      ai.rng = &campaign_rng;
    }
    char ai_err[256];
    if (!ai_init_new_game(&ai, ai_err, sizeof(ai_err))) {
      diag_warn("ai_init_new_game failed: %s", ai_err[0] ? ai_err : "unknown");
    }
    if (share_campaign_rng) {
      game->move_rng = campaign_rng;
    } else {
      dos_rng_seed(&game->move_rng, ai.rng_seed ? ai.rng_seed : 1u);
    }
    game->ai_rng_seed = ai.rng_seed ? ai.rng_seed : 1u;
    /* NEW WORLD fog: reveal around owned units and colonies (scenario .MP is all-seen). */
    if (game->world_map_ok) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &game->units.units[i];
        if (!u->active || u->nation_id != game->human_nation || !units_is_on_map(u)) {
          continue;
        }
        map_reveal_radius(&game->world_map, u->x, u->y, game->human_nation, 1);
      }
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &game->colonies.colonies[i];
        if (!c->active || c->nation_id != game->human_nation) {
          continue;
        }
        map_reveal_radius(&game->world_map, c->x, c->y, game->human_nation, 2);
      }
    }
    if (game->units.selected_id >= 0) {
      const ColonizeUnit* u = units_get_const(&game->units, game->units.selected_id);
      if (u) {
        game->map_cursor_x = u->x;
        game->map_cursor_y = u->y;
        game->map_view_x = u->x;
        game->map_view_y = u->y;
      }
    }
  }

  sound_set_bgm(1);
  new_game_cancel(ng);
  game->in_menu = false;
  snprintf(
    game->status,
    sizeof(game->status),
    "%s of %s (%s)",
    game->leader_name,
    new_game_nation_name(game->human_nation),
    map_label
  );
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

  if (strstr(choice, "Hall of Fame") != NULL || strstr(choice, "HALL") != NULL) {
    set_status(game, "Hall of Fame", "not implemented yet");
    return;
  }

  if (strstr(choice, "CUSTOMIZE") != NULL || strstr(choice, "Customize") != NULL) {
    game->in_menu = false;
    new_game_begin(
      &game->new_game,
      NEW_GAME_PATH_CUSTOMIZE,
      game->resolved_data_dir,
      &game->messages,
      game->names_ok ? &game->names : NULL
    );
    set_status(game, "New game", "Customize New World");
    return;
  }

  if (strstr(choice, "AMERICA") != NULL || strstr(choice, "America") != NULL) {
    game->in_menu = false;
    new_game_begin(
      &game->new_game,
      NEW_GAME_PATH_AMERICA,
      game->resolved_data_dir,
      &game->messages,
      game->names_ok ? &game->names : NULL
    );
    set_status(game, "New game", "America");
    return;
  }

  if (strstr(choice, "NEW WORLD") != NULL || strstr(choice, "New World") != NULL ||
      strstr(choice, "Start") != NULL) {
    game->in_menu = false;
    new_game_begin(
      &game->new_game,
      NEW_GAME_PATH_NEW_WORLD,
      game->resolved_data_dir,
      &game->messages,
      game->names_ok ? &game->names : NULL
    );
    set_status(game, "New game", "New World");
    return;
  }

  set_status(game, "Menu", "unknown option");
}

static void game_set_view_center(ColonizeGameState* game, int x, int y) {
  if (!game) {
    return;
  }
  game->map_view_x = x;
  game->map_view_y = y;
}

static void game_do_end_turn(ColonizeGameState* game);
static void game_center_on_selected_unit(ColonizeGameState* game);
static void game_after_unit_action(ColonizeGameState* game);

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

/* On-map, or awake passenger (orders cleared) with moves remaining. */
static bool game_unit_selectable(const ColonizeGameState* game, const ColonizeUnit* u) {
  if (!game || !u || !u->active || u->nation_id != game->human_nation || u->moves_left <= 0) {
    return false;
  }
  if (units_is_on_map(u)) {
    return true;
  }
  return u->aboard_ship_id >= 0 && u->orders != 1;
}

/* Select a human unit that still has moves; otherwise select the tile under it. */
static void game_select_unit(ColonizeGameState* game, int unit_id) {
  if (!game || !game->units_ok) {
    return;
  }
  const ColonizeUnit* u = units_get_const(&game->units, unit_id);
  if (!u || !u->active) {
    return;
  }
  if (!game_unit_selectable(game, u)) {
    game_select_tile(game, u->x, u->y);
    return;
  }
  game->units.selected_id = unit_id;
  game->map_cursor_x = u->x;
  game->map_cursor_y = u->y;
  game_set_view_center(game, u->x, u->y);
  const ColonizeUnitType* ut = units_type(&game->units, u->type_index);
  snprintf(game->status, sizeof(game->status), "Selected %s", ut ? ut->name : "unit");
}

static bool game_friendly_colony_at(const ColonizeGameState* game, int x, int y) {
  if (!game) {
    return false;
  }
  const int cid = colonies_id_at(&game->colonies, x, y);
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  return col && col->active && col->nation_id == game->human_nation;
}

/*
 * Move selected unit to dest: ship landfall unload, colony dock disembark,
 * awake passenger walking ashore, or normal try_move.
 */
static bool game_try_unit_move(ColonizeGameState* game, int dest_x, int dest_y) {
  if (!game || !game->units_ok || !game->world_map_ok) {
    return false;
  }
  const int sid = game->units.selected_id;
  ColonizeUnit* selected = units_get(&game->units, sid);
  if (!selected || selected->moves_left <= 0) {
    return false;
  }
  const ColonizeColonyPool* colonies = &game->colonies;

  /* Awake passenger: walk onto adjacent land → unload. */
  if (selected->aboard_ship_id >= 0) {
    if (selected->orders == 1) {
      set_status(game, "Wake unit from stack first", NULL);
      return false;
    }
    const int ship_id = selected->aboard_ship_id;
    if (!units_unload_passenger(
          &game->units, ship_id, sid, &game->world_map, dest_x, dest_y, colonies
        )) {
      set_status(game, "Cannot disembark here", NULL);
      return false;
    }
    game->units.selected_id = sid;
    snprintf(game->status, sizeof(game->status), "Disembarked to (%d,%d)", dest_x, dest_y);
    game_after_unit_action(game);
    return true;
  }

  if (units_is_sea(&game->units, sid)) {
    const bool dest_water = map_tile_is_water(&game->world_map, dest_x, dest_y);
    const bool dest_land = map_tile_is_land(&game->world_map, dest_x, dest_y);
    if (dest_land && game_friendly_colony_at(game, dest_x, dest_y)) {
      if (!units_try_move(
            &game->units, sid, &game->world_map, dest_x, dest_y, colonies, &game->move_rng
          )) {
        set_status(game, "Move blocked", NULL);
        return false;
      }
      const int n = units_disembark_all(&game->units, sid, dest_x, dest_y);
      game->units.selected_id = sid;
      if (n > 0) {
        snprintf(game->status, sizeof(game->status), "Docked; %d disembarked", n);
      } else {
        snprintf(game->status, sizeof(game->status), "Moved to colony (%d,%d)", dest_x, dest_y);
      }
      game_after_unit_action(game);
      return true;
    }
    if (dest_land && !dest_water) {
      const int pax_id = units_first_cargo_with_moves(&game->units, sid);
      if (pax_id < 0) {
        set_status(game, "No unit ready to disembark", NULL);
        return false;
      }
      if (!units_unload_passenger(
            &game->units, sid, pax_id, &game->world_map, dest_x, dest_y, colonies
          )) {
        set_status(game, "Move blocked", NULL);
        return false;
      }
      game->units.selected_id = pax_id;
      snprintf(game->status, sizeof(game->status), "Landfall at (%d,%d)", dest_x, dest_y);
      game_after_unit_action(game);
      return true;
    }
  }

  {
    const int mp_before = selected->moves_left;
    if (!units_try_move(
          &game->units, sid, &game->world_map, dest_x, dest_y, colonies, &game->move_rng
        )) {
      if (units_last_combat_outcome() < 0) {
        set_status(game, "Combat lost", NULL);
        game_after_unit_action(game);
      } else if (selected->moves_left < mp_before) {
        /* DOS charges MP on failed partial-overspend rolls even when the unit stays. */
        set_status(game, "Move failed", NULL);
        game_after_unit_action(game);
      } else {
        set_status(game, "Move blocked", NULL);
      }
      return false;
    }
    if (units_last_combat_outcome() > 0) {
      snprintf(game->status, sizeof(game->status), "Combat won at (%d,%d)", dest_x, dest_y);
      game_after_unit_action(game);
      return true;
    }
  }
  snprintf(game->status, sizeof(game->status), "Moved unit to (%d,%d)", dest_x, dest_y);
  game_after_unit_action(game);
  return true;
}

/* After spending moves: keep unit, advance to next with moves, or tile-select. */
static void game_after_unit_action(ColonizeGameState* game) {
  if (!game || !game->units_ok) {
    return;
  }
  ColonizeUnit* u = units_get(&game->units, game->units.selected_id);
  if (!u || !u->active) {
    game_select_tile(game, game->map_cursor_x, game->map_cursor_y);
    return;
  }
  if (game->world_map_ok && u->nation_id >= 0 && u->nation_id <= 3 && units_is_on_map(u)) {
    map_reveal_radius(&game->world_map, u->x, u->y, u->nation_id, 1);
  }
  if (game->col1_ok && u->nation_id >= 0 && u->nation_id <= 3 && units_is_on_map(u)) {
    char contact[80];
    contact[0] = '\0';
    if (col1_contact_adjacent_tribe(
          &game->col1, u->x, u->y, u->nation_id, contact, sizeof(contact)
        ) &&
        contact[0]) {
      snprintf(game->status, sizeof(game->status), "%s", contact);
    }
  }
  game->map_cursor_x = u->x;
  game->map_cursor_y = u->y;
  game_set_view_center(game, u->x, u->y);
  if (u->moves_left > 0) {
    return;
  }
  const int exhausted_x = u->x;
  const int exhausted_y = u->y;
  if (turn_select_next_unit(&game->units, game->human_nation)) {
    game_center_on_selected_unit(game);
    const ColonizeUnit* next = units_get_const(&game->units, game->units.selected_id);
    const ColonizeUnitType* ut = next ? units_type(&game->units, next->type_index) : NULL;
    snprintf(game->status, sizeof(game->status), "Selected %s", ut ? ut->name : "unit");
    return;
  }
  game_select_tile(game, exhausted_x, exhausted_y);
  if (!turn_option_end_of_turn(game->col1_ok ? &game->col1 : NULL, game->col1_ok) &&
      turn_human_units_exhausted(&game->units, game->human_nation)) {
    game_do_end_turn(game);
  } else {
    snprintf(game->status, sizeof(game->status), "%s", "End of Turn");
  }
}

static bool game_key_move_delta(ColonizeKey key, int* out_dx, int* out_dy) {
  int dx = 0;
  int dy = 0;
  switch (key) {
    case COLONIZE_KEY_UP:
    case COLONIZE_KEY_KP8:
      dy = -1;
      break;
    case COLONIZE_KEY_DOWN:
    case COLONIZE_KEY_KP2:
      dy = 1;
      break;
    case COLONIZE_KEY_LEFT:
    case COLONIZE_KEY_KP4:
      dx = -1;
      break;
    case COLONIZE_KEY_RIGHT:
    case COLONIZE_KEY_KP6:
      dx = 1;
      break;
    case COLONIZE_KEY_KP7:
      dx = -1;
      dy = -1;
      break;
    case COLONIZE_KEY_KP9:
      dx = 1;
      dy = -1;
      break;
    case COLONIZE_KEY_KP1:
      dx = -1;
      dy = 1;
      break;
    case COLONIZE_KEY_KP3:
      dx = 1;
      dy = 1;
      break;
    default:
      return false;
  }
  if (out_dx) {
    *out_dx = dx;
  }
  if (out_dy) {
    *out_dy = dy;
  }
  return true;
}

/* Human-owned map unit on tile with moves remaining (else -1). */
static int game_owned_unit_at(const ColonizeGameState* game, int x, int y) {
  if (!game || !game->units_ok) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &game->units.units[i];
    if (!units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    if (u->nation_id != game->human_nation || u->moves_left <= 0) {
      continue;
    }
    return u->id;
  }
  return -1;
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

static bool game_nation_has_ff(const ColonizeGameState* game, int nation, int ff_index) {
  if (!game || !game->col1_ok || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT ||
      ff_index < 0 || ff_index >= (int)COLONIZE_COL1_FF_COUNT) {
    return false;
  }
  const int8_t owner = game->col1.head.founding_father[ff_index];
  /* DOS: -1 = not in congress; 0..3 = European nation that elected them. */
  if (owner >= 0 && owner == (int8_t)nation) {
    return true;
  }
  const uint8_t byte = game->col1.nation[nation].founding_fathers[ff_index / 8];
  return (byte & (uint8_t)(1u << (ff_index % 8))) != 0;
}

static ColoniesBuildableOpts game_colony_buildable_opts(const ColonizeGameState* game) {
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  if (!game) {
    return opts;
  }
  opts.map = game->world_map_ok ? &game->world_map : NULL;
  const ColonizeColony* col = colonies_get(&game->colonies, game->colony_view_id);
  const int nation = col ? col->nation_id : game->human_nation;
  /* NAMES.TXT @FATHERS: Adam Smith=0, Peter Stuyvesant=3. */
  opts.has_adam_smith = game_nation_has_ff(game, nation, 0);
  opts.has_peter_stuyvesant = game_nation_has_ff(game, nation, 3);
  return opts;
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
  colony_screen_reset_ui(&game->colony_screen);
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  if (col && col->colonist_count > 0) {
    game->colony_screen.selected_colonist = 0;
  }
  snprintf(game->status, sizeof(game->status), "Entered %s", col ? col->name : "colony");
  colony_screen_set_status(&game->colony_screen, col ? col->name : "Colony");
}

/* One selection: colony colonist index, or admit selected outside unit first. */
static int game_colony_selected_colonist(ColonizeGameState* game) {
  if (!game) {
    return -1;
  }
  ColonyScreenView* csv = &game->colony_screen;
  if (csv->selected_colonist >= 0) {
    return csv->selected_colonist;
  }
  if (csv->selected_outside_unit < 0 || !game->units_ok) {
    return -1;
  }
  const int ci = colonies_admit_unit(
    &game->colonies, game->colony_view_id, &game->units, csv->selected_outside_unit
  );
  if (ci < 0) {
    return -1;
  }
  csv->selected_outside_unit = -1;
  csv->selected_colonist = ci;
  return ci;
}

static void game_colony_select_colonist(ColonizeGameState* game, int colonist_index);
static void game_colony_select_outside(ColonizeGameState* game, int unit_id);

static void game_colony_finish_eject(
  ColonizeGameState* game,
  int colonist_index,
  int role
) {
  ColonyScreenView* csv = &game->colony_screen;
  const int uid = colonies_eject_colonist(
    &game->colonies, game->colony_view_id, colonist_index, &game->units, role
  );
  if (uid < 0) {
    set_status(game, "Cannot leave colony", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  game_colony_select_outside(game, uid);
  const ColonizeColony* col = colonies_get(&game->colonies, game->colony_view_id);
  if (col && col->colonist_count <= 0) {
    colonies_abandon(&game->colonies, game->colony_view_id);
    game->in_colony = false;
    game->colony_view_id = -1;
    snprintf(game->status, sizeof(game->status), "Colony abandoned");
    set_status(game, game->status, NULL);
    return;
  }
  snprintf(
    game->status, sizeof(game->status), "Left as %s", colonies_eject_role_name(role)
  );
  colony_screen_set_status(csv, game->status);
}

/* Returns true if eject was handled (done, blocked, or confirm opened). */
static bool game_colony_request_eject(
  ColonizeGameState* game,
  int colonist_index,
  int role
) {
  ColonyScreenView* csv = &game->colony_screen;
  const ColonizeColony* colony = colonies_get(&game->colonies, game->colony_view_id);
  if (!colony || colonist_index < 0) {
    set_status(game, "Cannot leave colony", NULL);
    colony_screen_set_status(csv, game->status);
    return true;
  }
  if (colonies_has_fortification(&game->colonies, colony) && colony->colonist_count <= 2) {
    colony_screen_close_eject(csv);
    colony_screen_open_message_ok(
      csv, "A colony with a Stockade must keep at least 2 colonists."
    );
    return true;
  }
  if (colony->colonist_count <= 1) {
    colony_screen_open_abandon_confirm(csv, colonist_index, role);
    return true;
  }
  colony_screen_close_eject(csv);
  game_colony_finish_eject(game, colonist_index, role);
  return true;
}

static void game_colony_select_colonist(ColonizeGameState* game, int colonist_index) {
  if (!game) {
    return;
  }
  game->colony_screen.selected_colonist = colonist_index;
  game->colony_screen.selected_outside_unit = -1;
}

static void game_colony_select_outside(ColonizeGameState* game, int unit_id) {
  if (!game) {
    return;
  }
  game->colony_screen.selected_outside_unit = unit_id;
  game->colony_screen.selected_colonist = -1;
}

static int game_colony_list_outside_roles(
  const ColonizeColony* colony,
  const ColonizeUnit* unit,
  int* out_roles,
  int out_max
);

static void game_ui_drag_clear(ColonizeGameState* game) {
  if (game) {
    ui_drag_clear(&game->ui_drag);
  }
}

static const ColonizeSpriteSheet* game_icons(const ColonizeGameState* game) {
  return (game && game->unit_icons_ok) ? &game->unit_icons : NULL;
}

static int game_europe_transit_line_h(const ColonizeGameState* game) {
  const ColonizeFont* font = (game && game->menu_font_ok) ? &game->menu_font : NULL;
  return font && font->max_height > 0 ? (int)font->max_height + 2 : 8;
}

static EuropeHitResult game_europe_hit(const ColonizeGameState* game, int mx, int my) {
  return europe_hit_test_ex(
    &game->europe,
    mx,
    my,
    game->units_ok ? &game->units : NULL,
    game_icons(game),
    game_europe_transit_line_h(game)
  );
}

static void game_ui_drag_set_icon(ColonizeGameState* game, int sprite) {
  const ColonizeSpriteSheet* icons = game_icons(game);
  if (icons) {
    ui_drag_set_cursor_from_sheet(&game->ui_drag, icons, sprite);
  }
}

static void game_colony_drag_begin_cargo(ColonizeGameState* game, int cargo_type) {
  ColonyScreenView* csv = &game->colony_screen;
  ColonizeColony* colony = colonies_get_mut(&game->colonies, game->colony_view_id);
  if (!colony || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return;
  }
  csv->selected_cargo = cargo_type;
  if (csv->transport_unit_id < 0) {
    set_status(game, "Select a ship first", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  if (colony->stock[cargo_type] <= 0) {
    set_status(game, "No cargo in slot", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const int want = colony->stock[cargo_type] < 100 ? colony->stock[cargo_type] : 100;
  ui_drag_begin(
    &game->ui_drag, UI_DRAG_COLONY_CARGO, cargo_type, csv->transport_unit_id, want
  );
  game_ui_drag_set_icon(game, COLONY_CARGO_ICON_BASE + cargo_type);
}

static void game_colony_drag_begin_hold(ColonizeGameState* game, int hold_index) {
  ColonyScreenView* csv = &game->colony_screen;
  if (csv->transport_unit_id < 0 || !game->units_ok) {
    set_status(game, "Select a ship first", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const ColonizeUnit* tu = units_get_const(&game->units, csv->transport_unit_id);
  if (!tu || hold_index < 0 || hold_index >= COLONIZE_UNIT_CARGO_MAX) {
    return;
  }
  if (tu->hold_goods_amount[hold_index] <= 0 || tu->hold_goods_amount[hold_index] >= 255) {
    set_status(game, "Hold empty", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const int ctype = tu->hold_goods_type[hold_index];
  ui_drag_begin(
    &game->ui_drag, UI_DRAG_COLONY_HOLD, hold_index, csv->transport_unit_id, 0
  );
  if (ctype >= 0 && ctype < COLONIZE_CARGO_COUNT) {
    game_ui_drag_set_icon(game, COLONY_CARGO_ICON_BASE + ctype);
  }
}

static void game_colony_drag_begin_colonist(ColonizeGameState* game, int colonist_index) {
  ColonizeColony* colony = colonies_get_mut(&game->colonies, game->colony_view_id);
  game_colony_select_colonist(game, colonist_index);
  if (!colony || colonist_index < 0 || colonist_index >= colony->colonist_count) {
    return;
  }
  const ColonizeColonist* c = &colony->colonists[colonist_index];
  ui_drag_begin(&game->ui_drag, UI_DRAG_COLONY_COLONIST, colonist_index, -1, 0);
  if (game->units_ok) {
    const int sprite =
      units_working_colonist_sprite(&game->units, c->unit_type_index, c->profession);
    game_ui_drag_set_icon(game, sprite);
  }
}

static void game_colony_drag_begin_outside(ColonizeGameState* game, int unit_id) {
  game_colony_select_outside(game, unit_id);
  ui_drag_begin(&game->ui_drag, UI_DRAG_COLONY_OUTSIDE, -1, unit_id, 0);
  if (!game->units_ok) {
    return;
  }
  const ColonizeUnit* u = units_get_const(&game->units, unit_id);
  if (!u) {
    return;
  }
  const ColonizeUnitType* ut = units_type(&game->units, u->type_index);
  int sprite = ut ? ut->icon_sprite : -1;
  if (ut && strstr(ut->name, "Colonist") != NULL) {
    sprite = units_working_colonist_sprite(&game->units, u->type_index, u->profession);
  }
  game_ui_drag_set_icon(game, sprite);
}

static void game_colony_assign_building_drop(ColonizeGameState* game, int building_index) {
  ColonyScreenView* csv = &game->colony_screen;
  if (building_index < 0) {
    set_status(game, "Build it first", NULL);
  } else {
    const int ci = game_colony_selected_colonist(game);
    if (ci < 0) {
      set_status(game, "Select a colonist first", NULL);
    } else if (colonies_assign_workplace(
                 &game->colonies, game->colony_view_id, ci, building_index
               )) {
      const ColonizeBuildingType* bt = colonies_building_type(&game->colonies, building_index);
      snprintf(
        game->status, sizeof(game->status), "Assigned to %s", bt ? bt->name : "building"
      );
    } else {
      set_status(game, "Cannot assign here", NULL);
    }
  }
  colony_screen_set_status(csv, game->status);
}

static void game_colony_area_tile_drop(
  ColonizeGameState* game,
  ColonizeColony* colony,
  const ColonizeWorldMap* cmap,
  int tile_index
) {
  ColonyScreenView* csv = &game->colony_screen;
  const int who = (int)colony->tiles[tile_index];
  if (who >= 0 && who < colony->colonist_count) {
    game_colony_select_colonist(game, who);
    colony_screen_open_jobs(csv, cmap, colony, tile_index);
  } else {
    const int ci = game_colony_selected_colonist(game);
    if (ci < 0) {
      set_status(game, "Select a colonist first", NULL);
      colony_screen_set_status(csv, game->status);
    } else {
      colony_screen_open_jobs(csv, cmap, colony, tile_index);
    }
  }
}

static void game_colony_fence_drop(ColonizeGameState* game, ColonizeColony* colony) {
  ColonyScreenView* csv = &game->colony_screen;
  if (csv->selected_colonist < 0) {
    if (csv->selected_outside_unit >= 0) {
      const ColonizeUnit* u = game->units_ok
        ? units_get_const(&game->units, csv->selected_outside_unit)
        : NULL;
      if (u && colony) {
        csv->eject_role_count = game_colony_list_outside_roles(
          colony, u, csv->eject_roles, COLONIZE_EJECT_ROLE_COUNT
        );
        if (csv->eject_role_count <= 0) {
          csv->eject_roles[0] = COLONIZE_EJECT_COLONIST;
          csv->eject_role_count = 1;
        }
        csv->eject_colonist_index = -1;
        csv->eject_unit_id = u->id;
        csv->eject_selection = 0;
        csv->eject_open = true;
      } else {
        set_status(game, "Select a colonist first", NULL);
        colony_screen_set_status(csv, game->status);
      }
    } else {
      set_status(game, "Select a colonist first", NULL);
      colony_screen_set_status(csv, game->status);
    }
  } else {
    colony_screen_open_eject(csv, &game->colonies, game->colony_view_id, csv->selected_colonist);
  }
}

static bool game_colony_drag_drop(
  ColonizeGameState* game,
  ColonizeColony* colony,
  const ColonizeWorldMap* cmap,
  int mx,
  int my
) {
  ColonyScreenView* csv = &game->colony_screen;
  UiDragSession* drag = &game->ui_drag;
  if (!ui_drag_active(drag) || !colony) {
    game_ui_drag_clear(game);
    return false;
  }
  const ColonyScreenHitResult hit = colony_screen_hit_test(
    csv, &game->colonies, colony, game->units_ok ? &game->units : NULL, mx, my
  );
  const UiDragKind kind = drag->kind;

  if (kind == UI_DRAG_COLONY_CARGO) {
    if (hit.kind == COLONY_HIT_HOLD || hit.kind == COLONY_HIT_TRANSPORT) {
      int tid = csv->transport_unit_id;
      if (hit.kind == COLONY_HIT_TRANSPORT && hit.index >= 0 &&
          hit.index < csv->docked_transport_count) {
        tid = csv->docked_transport_ids[hit.index];
        csv->transport_unit_id = tid;
      }
      if (tid >= 0 && game->units_ok) {
        const int moved = colonies_transfer_to_unit(
          &game->colonies,
          game->colony_view_id,
          &game->units,
          tid,
          drag->index,
          drag->amount > 0 ? drag->amount : 100
        );
        if (moved > 0) {
          snprintf(game->status, sizeof(game->status), "Loaded %d", moved);
        } else {
          set_status(game, "No empty hold", NULL);
        }
        colony_screen_set_status(csv, game->status);
      }
    }
  } else if (kind == UI_DRAG_COLONY_HOLD) {
    if (hit.kind == COLONY_HIT_CARGO_SLOT ||
        (hit.kind == COLONY_HIT_NONE && my >= COLONY_CARGO_STRIP_Y)) {
      if (csv->transport_unit_id >= 0 && game->units_ok) {
        bool full = false;
        const int moved = colonies_transfer_from_unit(
          &game->colonies,
          game->colony_view_id,
          &game->units,
          csv->transport_unit_id,
          drag->index,
          &full
        );
        if (moved > 0 && full) {
          snprintf(game->status, sizeof(game->status), "Unloaded %d (Warehouse full)", moved);
        } else if (moved > 0) {
          snprintf(game->status, sizeof(game->status), "Unloaded %d", moved);
        } else if (full) {
          set_status(game, "Warehouse full", NULL);
        } else {
          set_status(game, "Hold empty", NULL);
        }
        colony_screen_set_status(csv, game->status);
      }
    }
  } else if (kind == UI_DRAG_COLONY_COLONIST || kind == UI_DRAG_COLONY_OUTSIDE) {
    if (hit.kind == COLONY_HIT_BUILDING) {
      game_colony_assign_building_drop(game, hit.index);
    } else if (hit.kind == COLONY_HIT_AREA_TILE) {
      game_colony_area_tile_drop(game, colony, cmap, hit.index);
    } else if (hit.kind == COLONY_HIT_FENCE) {
      game_colony_fence_drop(game, colony);
    }
  }

  game_ui_drag_clear(game);
  return true;
}

static void game_europe_sail_harbor(ColonizeGameState* game, int hidx) {
  EuropeScreen* eu = &game->europe;
  if (eu->harbor_ships <= 0 || hidx < 0 || hidx >= eu->harbor_ships) {
    snprintf(eu->status, sizeof(eu->status), "%s", "No ships in harbor.");
    return;
  }
  if (!game->units_ok) {
    snprintf(eu->status, sizeof(eu->status), "%s", "Cannot sail: units unavailable.");
    return;
  }
  EuropeHarborShip* hs = &eu->harbor[hidx];
  if (hs->type_index < 0) {
    const int resolved = units_find_type(&game->units, hs->name);
    if (resolved >= 0) {
      hs->type_index = resolved;
    }
  }
  int movement = 5;
  const ColonizeUnitType* ut = units_type(&game->units, hs->type_index);
  if (ut) {
    movement = ut->movement;
  }
  europe_set_sail_from_harbor(eu, hidx, movement, &game->units);
}

static bool game_europe_drag_drop(ColonizeGameState* game, int mx, int my) {
  EuropeScreen* eu = &game->europe;
  UiDragSession* drag = &game->ui_drag;
  if (!ui_drag_active(drag)) {
    game_ui_drag_clear(game);
    return false;
  }
  const EuropeHitResult hit = game_europe_hit(game, mx, my);
  const UiDragKind kind = drag->kind;

  if (kind == UI_DRAG_EUROPE_MARKET) {
    if (hit.kind == EUROPE_HIT_HOLD || hit.kind == EUROPE_HIT_HARBOR_SHIP) {
      int hidx = eu->selected_harbor;
      if (hit.kind == EUROPE_HIT_HARBOR_SHIP && hit.index >= 0) {
        hidx = hit.index;
        eu->selected_harbor = hidx;
      }
      if (hidx >= 0) {
        europe_buy_cargo(eu, hidx, drag->index, drag->amount > 0 ? drag->amount : 100);
      } else {
        snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
      }
    }
  } else if (kind == UI_DRAG_EUROPE_HOLD) {
    if (hit.kind == EUROPE_HIT_MARKET) {
      if (eu->selected_harbor >= 0) {
        europe_sell_hold(eu, eu->selected_harbor, drag->index);
      }
    }
  } else if (kind == UI_DRAG_EUROPE_HARBOR_SHIP) {
    if (hit.kind == EUROPE_HIT_BOUND) {
      game_europe_sail_harbor(game, drag->index);
    }
  } else if (kind == UI_DRAG_EUROPE_EXPECTED_SHIP) {
    if (hit.kind == EUROPE_HIT_BOUND) {
      europe_reverse_transit(eu, true, drag->index);
    }
  } else if (kind == UI_DRAG_EUROPE_BOUND_SHIP) {
    if (hit.kind == EUROPE_HIT_EXPECTED) {
      europe_reverse_transit(eu, false, drag->index);
    }
  }

  game_ui_drag_clear(game);
  return true;
}

static int game_colony_list_outside_roles(
  const ColonizeColony* colony,
  const ColonizeUnit* unit,
  int* out_roles,
  int out_max
) {
  if (!colony || !unit || !out_roles || out_max <= 0) {
    return 0;
  }
  int stock_tools = colony->stock[COLONIZE_CARGO_TOOLS] + (unit->tools > 0 ? unit->tools : 0);
  int stock_muskets = colony->stock[COLONIZE_CARGO_MUSKETS] + (unit->muskets > 0 ? unit->muskets : 0);
  int stock_horses = colony->stock[COLONIZE_CARGO_HORSES] + (unit->horses > 0 ? unit->horses : 0);
  int n = 0;
  out_roles[n++] = COLONIZE_EJECT_COLONIST;
  if (n < out_max && stock_tools >= UNITS_EQUIP_TOOLS_STEP) {
    out_roles[n++] = COLONIZE_EJECT_PIONEER;
  }
  if (n < out_max && stock_muskets >= UNITS_EQUIP_MUSKETS) {
    out_roles[n++] = COLONIZE_EJECT_SOLDIER;
  }
  if (n < out_max && stock_horses >= UNITS_EQUIP_HORSES) {
    out_roles[n++] = COLONIZE_EJECT_SCOUT;
  }
  if (n < out_max && stock_muskets >= UNITS_EQUIP_MUSKETS &&
      stock_horses >= UNITS_EQUIP_HORSES) {
    out_roles[n++] = COLONIZE_EJECT_DRAGOON;
  }
  return n;
}

static bool game_colony_apply_outside_role(
  ColonizeColony* colony,
  ColonizeUnitPool* units,
  int unit_id,
  int role
) {
  if (!colony || !units) {
    return false;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u) {
    return false;
  }
  int tools_take = 0;
  int muskets_take = 0;
  int horses_take = 0;
  const char* type_name = "Colonists";
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    tools_take = UNITS_EQUIP_TOOLS_MAX;
    type_name = "Pioneers";
    break;
  case COLONIZE_EJECT_SOLDIER:
    muskets_take = UNITS_EQUIP_MUSKETS;
    type_name = "Soldiers";
    break;
  case COLONIZE_EJECT_SCOUT:
    horses_take = UNITS_EQUIP_HORSES;
    type_name = "Scouts";
    break;
  case COLONIZE_EJECT_DRAGOON:
    muskets_take = UNITS_EQUIP_MUSKETS;
    horses_take = UNITS_EQUIP_HORSES;
    type_name = "Dragoons";
    break;
  case COLONIZE_EJECT_COLONIST:
  default:
    type_name = "Colonists";
    break;
  }

  int stock_tools = colony->stock[COLONIZE_CARGO_TOOLS] + (u->tools > 0 ? u->tools : 0);
  int stock_muskets = colony->stock[COLONIZE_CARGO_MUSKETS] + (u->muskets > 0 ? u->muskets : 0);
  int stock_horses = colony->stock[COLONIZE_CARGO_HORSES] + (u->horses > 0 ? u->horses : 0);
  if (stock_tools < tools_take || stock_muskets < muskets_take || stock_horses < horses_take) {
    return false;
  }

  colony->stock[COLONIZE_CARGO_TOOLS] = stock_tools - tools_take;
  colony->stock[COLONIZE_CARGO_MUSKETS] = stock_muskets - muskets_take;
  colony->stock[COLONIZE_CARGO_HORSES] = stock_horses - horses_take;

  int type_index = units_find_type(units, type_name);
  if (type_index < 0) {
    type_index = u->type_index;
  }
  u->type_index = type_index;
  u->tools = tools_take;
  u->muskets = muskets_take;
  u->horses = horses_take;
  return true;
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
  ctx->map = game->world_map_ok ? &game->world_map : NULL;
  ctx->col1 = game->col1_ok ? &game->col1 : NULL;
  ctx->col1_ok = game->col1_ok;
  ctx->rng = &game->move_rng;
  ctx->rng_seed = game->ai_rng_seed ? game->ai_rng_seed : 100u;
  ctx->status = game->status;
  ctx->status_size = sizeof(game->status);
}

/* Purchased-ship cargo tags (see europe_board_sentry_dockers): 0 = Colonists,
 * -2 = Artillery. Real passenger type indices (map→Europe round trips) pass through. */
static int game_europe_resolve_pax_type(const ColonizeUnitPool* units, int tag) {
  if (tag == -2) {
    const int t = units_find_type(units, "Artillery");
    return t >= 0 ? t : 0;
  }
  if (tag < 0 || tag >= units->type_count) {
    const int t = units_find_type(units, "Colonists");
    return t >= 0 ? t : 0;
  }
  return tag;
}

static void game_europe_capture_pax_professions(
  const ColonizeUnitPool* units,
  int ship_id,
  int* out_profs,
  int max
) {
  if (!out_profs || max <= 0) {
    return;
  }
  for (int i = 0; i < max; ++i) {
    out_profs[i] = -1;
  }
  const ColonizeUnit* ship = units_get_const(units, ship_id);
  if (!ship) {
    return;
  }
  for (int i = 0; i < ship->cargo_count && i < max; ++i) {
    const ColonizeUnit* pax = units_get_const(units, ship->cargo_ids[i]);
    out_profs[i] = pax ? pax->profession : -1;
  }
}

/*
 * Spawn every Bound-for-New-World ship whose voyage has finished. Called after
 * turn end, when leaving the Europe screen, and at Europe-input time so ships
 * that just ticked to 0 appear promptly.
 */
static void game_europe_deliver_bound_ships(ColonizeGameState* game) {
  if (!game || !game->europe_ok || !game->units_ok || !game->world_map_ok) {
    return;
  }
  EuropeScreen* eu = &game->europe;
  for (;;) {
    int idx = -1;
    for (int i = 0; i < eu->bound_ships; ++i) {
      if (eu->bound[i].turns_left <= 0) {
        idx = i;
        break;
      }
    }
    if (idx < 0) {
      break;
    }

    int cargo_professions[EUROPE_SHIP_CARGO_MAX];
    memcpy(cargo_professions, eu->bound[idx].cargo_professions, sizeof(cargo_professions));

    int type_index = -1;
    char name[32];
    int cargo_types[EUROPE_SHIP_CARGO_MAX];
    int cargo_count = 0;
    int hold_types[EUROPE_SHIP_CARGO_MAX];
    int hold_amts[EUROPE_SHIP_CARGO_MAX];
    int exit_x = 0;
    int exit_y = 0;
    bool exit_east = true;
    memset(hold_types, 0, sizeof(hold_types));
    memset(hold_amts, 0, sizeof(hold_amts));
    if (!europe_bound_pop_arrived(
          eu,
          &type_index,
          name,
          sizeof(name),
          cargo_types,
          &cargo_count,
          EUROPE_SHIP_CARGO_MAX,
          hold_types,
          hold_amts,
          EUROPE_SHIP_CARGO_MAX,
          &exit_x,
          &exit_y,
          &exit_east
        )) {
      break; /* shouldn't happen — idx was just found */
    }

    if (type_index < 0) {
      type_index = units_find_type(&game->units, name); /* purchased ship, never resolved */
    }
    if (type_index < 0) {
      diag_warn("Europe arrival: could not resolve ship type for '%s'", name);
      continue;
    }

    int sx = exit_x;
    int sy = exit_y;
    if (sx <= 0 && sy <= 0 && eu->last_exit_valid) {
      sx = eu->last_exit_x;
      sy = eu->last_exit_y;
    }
    if (sx <= 0 && sy <= 0) {
      sx = (int)game->world_map.width / 2;
      sy = (int)game->world_map.height / 2;
    }
    int fx = sx;
    int fy = sy;
    if (!units_find_high_seas_tile(&game->units, &game->world_map, sx, sy, &fx, &fy)) {
      diag_warn("Europe arrival: no free high-seas tile for '%s'", name);
      continue;
    }

    int resolved_cargo[EUROPE_SHIP_CARGO_MAX];
    for (int i = 0; i < cargo_count; ++i) {
      resolved_cargo[i] = game_europe_resolve_pax_type(&game->units, cargo_types[i]);
    }

    const int ship_id = units_spawn_ship_with_cargo(
      &game->units, type_index, fx, fy, resolved_cargo, cargo_count, hold_types, hold_amts
    );
    if (ship_id < 0) {
      diag_warn("Europe arrival: failed to spawn '%s' at (%d,%d)", name, fx, fy);
      continue;
    }
    ColonizeUnit* ship = units_get(&game->units, ship_id);
    if (ship) {
      for (int i = 0; i < ship->cargo_count && i < cargo_count; ++i) {
        ColonizeUnit* pax = units_get(&game->units, ship->cargo_ids[i]);
        if (pax && cargo_professions[i] >= 0) {
          pax->profession = cargo_professions[i];
        }
      }
    }
    snprintf(
      game->status, sizeof(game->status), "%s arrived from Europe at (%d,%d)", name, fx, fy
    );
    diag_info("Europe arrival: spawned '%s' id=%d at (%d,%d)", name, ship_id, fx, fy);
  }
}

static void game_finish_end_turn(ColonizeGameState* game, const ColonizeTurnResult* result) {
  game_apply_turn_autosave(game, result);
  game_europe_deliver_bound_ships(game);
  if (game->europe_ok && game->europe.open_on_dock) {
    game->in_europe = true;
  }
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
    game_select_tile(game, game->map_cursor_x, game->map_cursor_y);
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
        units_wake(&game->units, at);
        game->units.selected_id = at;
        const ColonizeUnit* u = units_get_const(&game->units, at);
        const ColonizeUnitType* ut = u ? units_type(&game->units, u->type_index) : NULL;
        snprintf(game->status, sizeof(game->status), "Activated %s", ut ? ut->name : "unit");
      }
      return true;
    }
    case MAP_MENU_ACTION_WAIT_UNIT:
      game_wait_next_unit(game);
      return true;
    case MAP_MENU_ACTION_FORTIFY: {
      const int uid = game->units.selected_id;
      if (uid < 0 || !units_order_fortify(&game->units, uid)) {
        set_status(game, "Cannot fortify", NULL);
      } else {
        set_status(game, "Fortifying", NULL);
        game_wait_next_unit(game);
      }
      return true;
    }
    case MAP_MENU_ACTION_SENTRY: {
      const int uid = game->units.selected_id;
      if (uid < 0 || !units_order_sentry(&game->units, uid)) {
        set_status(game, "Cannot sentry", NULL);
      } else {
        set_status(game, "Sentry", NULL);
        game_wait_next_unit(game);
      }
      return true;
    }
    case MAP_MENU_ACTION_DISBAND: {
      const int uid = game->units.selected_id;
      if (uid < 0 || !units_disband(&game->units, uid)) {
        set_status(game, "Cannot disband", NULL);
      } else {
        set_status(game, "Unit disbanded", NULL);
        game_wait_next_unit(game);
      }
      return true;
    }
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
          const int profession = founder ? founder->profession : UNITS_JOB_NONE;
          int tools = 0, muskets = 0, horses = 0;
          units_founder_loot(&game->units, uid, &tools, &muskets, &horses);
          const int cid = colonies_found(
            &game->colonies,
            &game->world_map,
            cx,
            cy,
            game->human_nation,
            type_index,
            profession,
            tools,
            muskets,
            horses
          );
          if (cid >= 0) {
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
      } else {
        const ColonizeUnit* ship = units_get_const(&game->units, sid);
        const int pax_id = (ship && ship->cargo_count > 0) ? ship->cargo_ids[0] : -1;
        if (!units_unload(
              &game->units,
              sid,
              &game->world_map,
              game->map_cursor_x,
              game->map_cursor_y,
              &game->colonies
            )) {
          set_status(game, "Cannot unload (need adjacent free land)", NULL);
        } else {
          if (pax_id >= 0) {
            game->units.selected_id = pax_id;
          }
          set_status(game, "Unit unloaded", NULL);
        }
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
        const int exit_x = ship->x;
        const int exit_y = ship->y;
        const bool exit_east = exit_x >= (int)game->world_map.width / 2;
        int type_index = -1;
        char ship_name[32];
        int cargo_types[EUROPE_SHIP_CARGO_MAX];
        int cargo_profs[EUROPE_SHIP_CARGO_MAX];
        int cargo_count = 0;
        int hold_types[EUROPE_SHIP_CARGO_MAX];
        int hold_amts[EUROPE_SHIP_CARGO_MAX];
        memset(hold_types, 0, sizeof(hold_types));
        memset(hold_amts, 0, sizeof(hold_amts));
        game_europe_capture_pax_professions(
          &game->units, sid, cargo_profs, EUROPE_SHIP_CARGO_MAX
        );
        if (!units_despawn_ship_with_cargo(
              &game->units,
              sid,
              &type_index,
              ship_name,
              sizeof(ship_name),
              cargo_types,
              &cargo_count,
              EUROPE_SHIP_CARGO_MAX,
              hold_types,
              hold_amts,
              EUROPE_SHIP_CARGO_MAX
            )) {
          set_status(game, "Failed to sail ship", NULL);
        } else {
          const ColonizeUnitType* ut = units_type(&game->units, type_index);
          const int movement = ut ? ut->movement : 5;
          if (!europe_enqueue_expected(
                &game->europe,
                type_index,
                ship_name,
                cargo_types,
                cargo_profs,
                cargo_count,
                hold_types,
                hold_amts,
                exit_x,
                exit_y,
                exit_east,
                movement
              )) {
            const int restored = units_spawn_ship_with_cargo(
              &game->units,
              type_index,
              exit_x,
              exit_y,
              cargo_types,
              cargo_count,
              hold_types,
              hold_amts
            );
            if (restored >= 0) {
              game->units.selected_id = restored;
            }
            set_status(game, "Europe lane is full", NULL);
          } else {
            snprintf(game->status, sizeof(game->status), "%s sailed to Europe", ship_name);
            game->in_europe = true;
          }
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
    case MAP_MENU_ACTION_DEBUG_SPRITE_VIEWER:
      game_open_debug_atlas(game);
      return true;
    case MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS:
      game->debug_show_mouse_coords = !game->debug_show_mouse_coords;
      snprintf(
        game->status,
        sizeof(game->status),
        "Mouse coords: %s",
        game->debug_show_mouse_coords ? "on" : "off"
      );
      return true;
    case MAP_MENU_ACTION_CHEAT_REVEAL_MAP:
      if (game->world_map_ok) {
        map_reveal_all(&game->world_map, -1);
        set_status(game, "Map revealed", NULL);
      } else {
        set_status(game, "No map", NULL);
      }
      return true;
    case MAP_MENU_ACTION_CHEAT_CREATE_UNIT:
    case MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS:
    case MAP_MENU_ACTION_CHEAT_SET_HUMAN:
    case MAP_MENU_ACTION_CHEAT_KILL_INDIANS:
    case MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION:
    case MAP_MENU_ACTION_CHEAT_SOUND_TEST:
    case MAP_MENU_ACTION_CHEAT_MEMORY_CHECK:
    case MAP_MENU_ACTION_CHEAT_SHOW_STRATEGY:
    case MAP_MENU_ACTION_CHEAT_SHOW_COLONY_SITES:
    case MAP_MENU_ACTION_CHEAT_TEST_ROUTINE:
      set_status(game, "Cheat not implemented yet", map_menu_action_name(action));
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

  /* Pace Go-To at 10 tile-steps/sec so pathing is visible. */
  if (game->units_ok && game->world_map_ok) {
    game->goto_step_accum_ms += dt_ms;
    if (game->goto_step_accum_ms >= 100u) {
      game->goto_step_accum_ms -= 100u;
      if (game->goto_step_accum_ms > 200u) {
        game->goto_step_accum_ms = 0; /* drop backlog after hitch */
      }
      int stepped = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &game->units.units[i];
        if (!u->active || !units_orders_follow_goto(u->orders) || !units_is_on_map(u)) {
          continue;
        }
        if (!units_advance_goto_one_step(
              &game->units, u->id, &game->world_map, &game->colonies, &game->move_rng
            )) {
          continue;
        }
        stepped++;
        u = units_get(&game->units, u->id);
        if (u && u->nation_id >= 0 && u->nation_id <= 3) {
          map_reveal_radius(&game->world_map, u->x, u->y, u->nation_id, 1);
        }
      }
      if (stepped > 0 && game->units.selected_id >= 0) {
        const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
        if (sel && sel->active && units_is_on_map(sel)) {
          game->map_cursor_x = sel->x;
          game->map_cursor_y = sel->y;
          if (!game->in_menu && !game->in_colony && !game->in_europe && !game->in_report &&
              !game->in_pedia && !game->in_debug_atlas) {
            game_set_view_center(game, sel->x, sel->y);
          }
          if (sel->moves_left <= 0) {
            game_after_unit_action(game);
          }
        }
      }
    }
  }

  /* New-game wizard (difficulty → nation → … → sail). */
  if (new_game_active(&game->new_game)) {
    game->new_game.ui_font = game->intro_font_ok ? &game->intro_font
      : (game->colony_font_ok ? &game->colony_font
                              : (game->menu_font_ok ? &game->menu_font : NULL));
    game->new_game.tiny_font = game->colony_font_ok ? &game->colony_font
      : (game->menu_font_ok ? &game->menu_font : game->new_game.ui_font);
    game->new_game.lore_font = game->new_game.ui_font;
    game->new_game.wood_tile = game->menu_opentile_ok ? &game->menu_opentile : NULL;
    game->new_game.woodpanl = (game->pedia_wood_ok) ? &game->pedia_wood : NULL;
    game->new_game.labels_txt = game->labels_ok ? &game->labels : NULL;
    new_game_update(&game->new_game, dt_ms);
    if (new_game_wants_commit(&game->new_game)) {
      game_commit_new_campaign(game);
      return true;
    }
    new_game_handle_input(&game->new_game, input);
    if (new_game_wants_commit(&game->new_game)) {
      game_commit_new_campaign(game);
      return true;
    }
    if (!new_game_active(&game->new_game)) {
      /* Cancelled back to title. */
      game->in_menu = true;
      set_status(game, "Colonization Linux Port", NULL);
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
    ColonizeColony* colony = colonies_get_mut(&game->colonies, game->colony_view_id);
    ColonyScreenView* csv = &game->colony_screen;
    const ColonizeWorldMap* cmap = game->world_map_ok ? &game->world_map : NULL;
    if (colony && game->units_ok) {
      colony_screen_refresh_transports(csv, &game->units, colony);
    }

    if (input->last_key == COLONIZE_KEY_ESCAPE) {
      if (ui_drag_active(&game->ui_drag)) {
        game_ui_drag_clear(game);
        return true;
      }
      if (csv->message_kind != COLONY_MSG_NONE) {
        colony_screen_close_message(csv);
        return true;
      }
      if (csv->jobs_open) {
        colony_screen_close_jobs(csv);
        return true;
      }
      if (csv->eject_open) {
        colony_screen_close_eject(csv);
        return true;
      }
      if (csv->construction_open) {
        colony_screen_close_construction(csv);
        return true;
      }
      game->in_colony = false;
      game->colony_view_id = -1;
      diag_info("Left colony screen.");
      return true;
    }
    if (input->last_key == COLONIZE_KEY_ENTER) {
      if (csv->message_kind == COLONY_MSG_OK) {
        colony_screen_close_message(csv);
        return true;
      }
      if (csv->message_kind == COLONY_MSG_CONFIRM) {
        if (csv->message_selection == 0) {
          const int who = csv->pending_eject_colonist;
          const int role = csv->pending_eject_role;
          colony_screen_close_message(csv);
          game_colony_finish_eject(game, who, role);
        } else {
          colony_screen_close_message(csv);
        }
        return true;
      }
      if (csv->eject_open) {
        if (csv->eject_selection >= 0 && csv->eject_selection < csv->eject_role_count &&
            game->units_ok) {
          const int role = csv->eject_roles[csv->eject_selection];
          if (csv->eject_unit_id >= 0 && colony) {
            const bool ok =
              game_colony_apply_outside_role(colony, &game->units, csv->eject_unit_id, role);
            colony_screen_close_eject(csv);
            if (ok) {
              game_colony_select_outside(game, csv->selected_outside_unit);
              snprintf(game->status, sizeof(game->status), "Equipped as %s", colonies_eject_role_name(role));
            } else {
              set_status(game, "Cannot equip unit", NULL);
            }
            colony_screen_set_status(csv, game->status);
          } else {
            game_colony_request_eject(game, csv->eject_colonist_index, role);
          }
        }
        return true;
      }
      if (csv->jobs_open) {
        if (csv->jobs_selection >= 0 && csv->jobs_selection < csv->job_count) {
          const int job = csv->job_ids[csv->jobs_selection];
          const int ci = game_colony_selected_colonist(game);
          if (ci < 0) {
            set_status(game, "Select a colonist first", NULL);
          } else if (colonies_assign_field(
                       &game->colonies,
                       game->colony_view_id,
                       ci,
                       csv->jobs_tile_index,
                       job
                     )) {
            snprintf(
              game->status,
              sizeof(game->status),
              "Working as %s",
              colony_yield_job_name(job)
            );
          } else {
            set_status(game, "Cannot assign field", NULL);
          }
        }
        colony_screen_close_jobs(csv);
        colony_screen_set_status(csv, game->status);
        return true;
      }
      if (csv->construction_open) {
        if (csv->construction_selection == 0) {
          colonies_clear_construction(&game->colonies, game->colony_view_id);
          set_status(game, "Construction cleared", NULL);
        } else {
          const int bi = csv->construction_selection - 1;
          if (bi >= 0 && bi < csv->buildable_count) {
            const int bid = csv->buildable_ids[bi];
            if (colonies_set_construction(&game->colonies, game->colony_view_id, bid)) {
              const ColonizeBuildingType* bt = colonies_building_type(&game->colonies, bid);
              snprintf(
                game->status,
                sizeof(game->status),
                "Building %s",
                bt ? bt->name : "project"
              );
            }
          }
        }
        colony_screen_close_construction(csv);
        colony_screen_set_status(csv, game->status);
        return true;
      }
      /* Enter on selected colonist opens field jobs; otherwise leave. */
      if (colony && csv->selected_colonist >= 0) {
        int tile = colonies_colonist_tile(colony, csv->selected_colonist);
        if (tile < 0) {
          tile = 0;
        }
        colony_screen_open_jobs(csv, cmap, colony, tile);
        return true;
      }
      game->in_colony = false;
      game->colony_view_id = -1;
      diag_info("Left colony screen.");
      return true;
    }

    /* C = Construction Change (tech-supp). */
    if (input->last_key == COLONIZE_KEY_C) {
      if (csv->jobs_open) {
        colony_screen_close_jobs(csv);
      }
      if (csv->construction_open) {
        colony_screen_close_construction(csv);
      } else {
        {
          ColoniesBuildableOpts bopts = game_colony_buildable_opts(game);
          colony_screen_open_construction(
            csv, &game->colonies, game->colony_view_id, &bopts
          );
        }
        csv->multi_mode = COLONY_MULTI_CONSTRUCTION;
      }
      return true;
    }

    /* 1/2/3 = multifunction tabs; M cycles; N toggles production numbers; =/+ load. */
    for (int ti = 0; ti < input->text_input_len; ++ti) {
      const char ch = input->text_input[ti];
      if (ch == '1') {
        csv->multi_mode = COLONY_MULTI_PRODUCTION;
        return true;
      }
      if (ch == '2') {
        csv->multi_mode = COLONY_MULTI_UNITS;
        return true;
      }
      if (ch == '3') {
        csv->multi_mode = COLONY_MULTI_CONSTRUCTION;
        return true;
      }
      if (ch == 'm' || ch == 'M') {
        csv->multi_mode = (ColonyMultiMode)(((int)csv->multi_mode + 1) % 3);
        return true;
      }
      if (ch == 'n' || ch == 'N') {
        csv->show_production_numbers = !csv->show_production_numbers;
        return true;
      }
      if ((ch == '=' || ch == '+') && colony && game->units_ok && !csv->jobs_open &&
          !csv->construction_open) {
        if (csv->transport_unit_id < 0) {
          set_status(game, "Select a ship first", NULL);
        } else if (csv->selected_cargo < 0 || csv->selected_cargo >= COLONIZE_CARGO_COUNT ||
                   colony->stock[csv->selected_cargo] <= 0) {
          set_status(game, "Select warehouse cargo", NULL);
        } else {
          const int cargo = csv->selected_cargo;
          const int want = (ch == '=') ? 100
                                       : (colony->stock[cargo] < 100 ? colony->stock[cargo] : 100);
          const int moved = colonies_transfer_to_unit(
            &game->colonies,
            game->colony_view_id,
            &game->units,
            csv->transport_unit_id,
            cargo,
            want
          );
          if (moved > 0) {
            snprintf(game->status, sizeof(game->status), "Loaded %d", moved);
          } else {
            set_status(game, "No empty hold", NULL);
          }
        }
        colony_screen_set_status(csv, game->status);
        return true;
      }
    }

    if (csv->jobs_open) {
      if (input->last_key == COLONIZE_KEY_UP && csv->jobs_selection > 0) {
        csv->jobs_selection--;
        return true;
      }
      if (input->last_key == COLONIZE_KEY_DOWN && csv->jobs_selection < csv->job_count) {
        csv->jobs_selection++;
        return true;
      }
    } else if (csv->message_kind != COLONY_MSG_NONE) {
      const int max_sel = (csv->message_kind == COLONY_MSG_CONFIRM) ? 1 : 0;
      if (input->last_key == COLONIZE_KEY_UP && csv->message_selection > 0) {
        csv->message_selection--;
        return true;
      }
      if (input->last_key == COLONIZE_KEY_DOWN && csv->message_selection < max_sel) {
        csv->message_selection++;
        return true;
      }
    } else if (csv->eject_open) {
      if (input->last_key == COLONIZE_KEY_UP && csv->eject_selection > 0) {
        csv->eject_selection--;
        return true;
      }
      if (input->last_key == COLONIZE_KEY_DOWN &&
          csv->eject_selection + 1 < csv->eject_role_count) {
        csv->eject_selection++;
        return true;
      }
    } else if (csv->construction_open) {
      const int max_sel = csv->buildable_count;
      if (input->last_key == COLONIZE_KEY_UP && csv->construction_selection > 0) {
        csv->construction_selection--;
        return true;
      }
      if (input->last_key == COLONIZE_KEY_DOWN && csv->construction_selection < max_sel) {
        csv->construction_selection++;
        return true;
      }
    } else {
      if (input->last_key == COLONIZE_KEY_UP && colony && csv->selected_colonist > 0) {
        game_colony_select_colonist(game, csv->selected_colonist - 1);
        return true;
      }
      if (input->last_key == COLONIZE_KEY_DOWN && colony &&
          csv->selected_colonist + 1 < colony->colonist_count) {
        game_colony_select_colonist(game, csv->selected_colonist + 1);
        return true;
      }
    }

    /* B = buy remaining construction with gold + warehouse tools. */
    if (input->last_key == COLONIZE_KEY_B && colony &&
        colony->building_in_production >= 0) {
      const int gold_before = game->europe.gold;
      const ColonizeBuildingType* bt =
        colonies_building_type(&game->colonies, colony->building_in_production);
      const int tools = bt ? bt->tools_cost : 0;
      if (colonies_construction_tools_needed(&game->colonies, colony) > 0) {
        set_status(game, "Need tools", NULL);
      } else if (game->europe.gold < colonies_construction_gold_cost(&game->colonies, colony)) {
        set_status(game, "Need gold", NULL);
      } else if (colonies_buy_construction(
                   &game->colonies, game->colony_view_id, &game->europe.gold
                 )) {
        snprintf(
          game->status,
          sizeof(game->status),
          "Bought %s (-%d$, -%d tools)",
          bt ? bt->name : "building",
          gold_before - game->europe.gold,
          tools
        );
        colony_screen_close_construction(csv);
      } else {
        set_status(game, "Cannot buy", NULL);
      }
      colony_screen_set_status(csv, game->status);
      return true;
    }

    /* L = load highest-value cargo; U = unload first non-empty hold. */
    if (!csv->jobs_open && !csv->construction_open && colony && game->units_ok) {
      if (input->last_key == COLONIZE_KEY_L) {
        if (csv->transport_unit_id < 0) {
          set_status(game, "Select a ship first", NULL);
        } else {
          const int cargo = colonies_best_load_cargo(colony);
          if (cargo < 0) {
            set_status(game, "Nothing to load", NULL);
          } else {
            const int want = colony->stock[cargo] < 100 ? colony->stock[cargo] : 100;
            const int moved = colonies_transfer_to_unit(
              &game->colonies,
              game->colony_view_id,
              &game->units,
              csv->transport_unit_id,
              cargo,
              want
            );
            if (moved > 0) {
              snprintf(game->status, sizeof(game->status), "Loaded %d", moved);
            } else {
              set_status(game, "No empty hold", NULL);
            }
          }
        }
        colony_screen_set_status(csv, game->status);
        return true;
      }
      if (input->last_key == COLONIZE_KEY_U) {
        if (csv->transport_unit_id < 0) {
          set_status(game, "Select a ship first", NULL);
        } else {
          const int hold = units_first_goods_hold(&game->units, csv->transport_unit_id);
          if (hold < 0) {
            set_status(game, "Hold empty", NULL);
          } else {
            bool full = false;
            const int moved = colonies_transfer_from_unit(
              &game->colonies,
              game->colony_view_id,
              &game->units,
              csv->transport_unit_id,
              hold,
              &full
            );
            if (moved > 0 && full) {
              snprintf(game->status, sizeof(game->status), "Unloaded %d (Warehouse full)", moved);
            } else if (moved > 0) {
              snprintf(game->status, sizeof(game->status), "Unloaded %d", moved);
            } else if (full) {
              set_status(game, "Warehouse full", NULL);
            } else {
              set_status(game, "Cannot unload", NULL);
            }
          }
        }
        colony_screen_set_status(csv, game->status);
        return true;
      }
    }

    if (input->last_key == COLONIZE_KEY_SPACE) {
      if (colony) {
        ColonizeTurnResult prod;
        ColonizeColonyProdDelta delta;
        memset(&prod, 0, sizeof(prod));
        turn_colony_free_production(&game->colonies, colony, cmap, &prod, &delta);
        colony_screen_set_delta(csv, &delta);
        {
          static const struct {
            int cargo;
            const char* tag;
          } k_craft[] = {
            {COLONIZE_CARGO_RUM, "Rum"},
            {COLONIZE_CARGO_CIGARS, "Cigar"},
            {COLONIZE_CARGO_CLOTH, "Cloth"},
            {COLONIZE_CARGO_COATS, "Coat"},
            {COLONIZE_CARGO_TOOLS, "Tool"},
            {COLONIZE_CARGO_MUSKETS, "Gun"},
          };
          char craft[48];
          craft[0] = '\0';
          size_t cn = 0;
          for (size_t ci = 0; ci < sizeof(k_craft) / sizeof(k_craft[0]); ++ci) {
            const int g = delta.goods[k_craft[ci].cargo];
            if (g <= 0) {
              continue;
            }
            const int wrote = snprintf(
              craft + cn,
              sizeof(craft) - cn,
              "%s%s%+d",
              cn > 0 ? " " : "",
              k_craft[ci].tag,
              g
            );
            if (wrote > 0) {
              cn += (size_t)wrote;
            }
            if (cn >= sizeof(craft)) {
              break;
            }
          }
          if (craft[0]) {
            snprintf(
              game->status,
              sizeof(game->status),
              "Food%+d L%+d H%+d %s",
              delta.food_net,
              delta.lumber,
              delta.hammers_added,
              craft
            );
          } else {
            snprintf(
              game->status,
              sizeof(game->status),
              "Food%+d Lumber%+d Ore%+d H%+d",
              delta.food_net,
              delta.lumber,
              delta.ore,
              delta.hammers_added
            );
          }
        }
        if (delta.building_completed) {
          snprintf(game->status, sizeof(game->status), "Building completed!");
        }
        colony_screen_set_status(csv, game->status);
        diag_info("Colony free production id=%d", colony->id);
      }
      return true;
    }

    if (input->mouse_right_clicked && ui_drag_active(&game->ui_drag)) {
      game_ui_drag_clear(game);
      return true;
    }

    if (ui_drag_active(&game->ui_drag) && input->mouse_left_released && colony) {
      game_colony_drag_drop(game, colony, cmap, input->mouse_x, input->mouse_y);
      return true;
    }

    if (input->mouse_left_clicked && colony) {
      if (ui_drag_active(&game->ui_drag)) {
        return true; /* ignore click while dragging */
      }
      const ColonyScreenHitResult hit = colony_screen_hit_test(
        csv, &game->colonies, colony, game->units_ok ? &game->units : NULL, input->mouse_x, input->mouse_y
      );
      switch (hit.kind) {
      case COLONY_HIT_EXIT:
        game_ui_drag_clear(game);
        game->in_colony = false;
        game->colony_view_id = -1;
        return true;
      case COLONY_HIT_TRANSPORT:
        if (hit.index >= 0 && hit.index < csv->docked_transport_count) {
          csv->transport_unit_id = csv->docked_transport_ids[hit.index];
          {
            const ColonizeUnit* tu = units_get_const(&game->units, csv->transport_unit_id);
            const ColonizeUnitType* tt = tu ? units_type(&game->units, tu->type_index) : NULL;
            snprintf(
              game->status,
              sizeof(game->status),
              "%s",
              tt && tt->name[0] ? tt->name : "Transport selected"
            );
          }
          colony_screen_set_status(csv, game->status);
        }
        break;
      case COLONY_HIT_CARGO_SLOT:
        game_colony_drag_begin_cargo(game, hit.index);
        break;
      case COLONY_HIT_HOLD:
        game_colony_drag_begin_hold(game, hit.index);
        break;
      case COLONY_HIT_COLONIST:
      case COLONY_HIT_PEOPLE_COLONIST:
        game_colony_drag_begin_colonist(game, hit.index);
        break;
      case COLONY_HIT_OUTSIDE_UNIT:
        if (hit.index >= 0 && hit.index < csv->outside_unit_count) {
          game_colony_drag_begin_outside(game, csv->outside_unit_ids[hit.index]);
        }
        break;
      case COLONY_HIT_FENCE: {
        game_colony_fence_drop(game, colony);
        break;
      }
      case COLONY_HIT_EJECT_ROW: {
        if (hit.index >= 0 && hit.index < csv->eject_role_count && game->units_ok) {
          const int role = csv->eject_roles[hit.index];
          if (csv->eject_unit_id >= 0 && colony) {
            const int uid = csv->eject_unit_id;
            const bool ok =
              game_colony_apply_outside_role(colony, &game->units, uid, role);
            colony_screen_close_eject(csv);
            if (ok) {
              game_colony_select_outside(game, uid);
              snprintf(
                game->status,
                sizeof(game->status),
                "Equipped as %s",
                colonies_eject_role_name(role)
              );
            } else {
              set_status(game, "Cannot equip unit", NULL);
            }
          } else {
            game_colony_request_eject(game, csv->eject_colonist_index, role);
          }
          colony_screen_set_status(csv, game->status);
        }
        break;
      }
      case COLONY_HIT_EJECT_OUTSIDE:
        colony_screen_close_eject(csv);
        break;
      case COLONY_HIT_MESSAGE_OK:
      case COLONY_HIT_MESSAGE_NO:
        colony_screen_close_message(csv);
        break;
      case COLONY_HIT_MESSAGE_YES: {
        const int who = csv->pending_eject_colonist;
        const int role = csv->pending_eject_role;
        colony_screen_close_message(csv);
        game_colony_finish_eject(game, who, role);
        break;
      }
      case COLONY_HIT_MESSAGE_OUTSIDE:
        /* Keep modal open until Yes/No/OK. */
        break;
      case COLONY_HIT_MULTI_BTN:
        if (hit.index >= 0 && hit.index < 3) {
          csv->multi_mode = (ColonyMultiMode)hit.index;
        }
        break;
      case COLONY_HIT_MULTI_PANE:
        if (csv->multi_mode == COLONY_MULTI_PRODUCTION) {
          csv->show_production_numbers = !csv->show_production_numbers;
        } else if (csv->multi_mode == COLONY_MULTI_UNITS) {
          if (csv->selected_outside_unit >= 0) {
            set_status(game, "Orders: Clear / Sentry / Fortify (stub)", NULL);
          } else {
            set_status(game, "Select an outside unit", NULL);
          }
          colony_screen_set_status(csv, game->status);
        } else if (csv->multi_mode == COLONY_MULTI_CONSTRUCTION) {
          {
            ColoniesBuildableOpts bopts = game_colony_buildable_opts(game);
            colony_screen_open_construction(
              csv, &game->colonies, game->colony_view_id, &bopts
            );
          }
        }
        break;
      case COLONY_HIT_MULTI_BUY: {
        if (!colony || colony->building_in_production < 0) {
          set_status(game, "No project", NULL);
        } else {
          const int gold_before = game->europe.gold;
          const ColonizeBuildingType* bt = colonies_building_type(
            &game->colonies, colony->building_in_production
          );
          const int tools = bt ? bt->tools_cost : 0;
          if (colonies_construction_tools_needed(&game->colonies, colony) > 0) {
            set_status(game, "Need tools", NULL);
          } else if (game->europe.gold < colonies_construction_gold_cost(&game->colonies, colony)) {
            set_status(game, "Need gold", NULL);
          } else if (colonies_buy_construction(
                       &game->colonies, game->colony_view_id, &game->europe.gold
                     )) {
            snprintf(
              game->status,
              sizeof(game->status),
              "Bought %s (-%d$, -%d tools)",
              bt ? bt->name : "building",
              gold_before - game->europe.gold,
              tools
            );
          } else {
            set_status(game, "Cannot buy", NULL);
          }
        }
        colony_screen_set_status(csv, game->status);
        break;
      }
      case COLONY_HIT_MULTI_CHANGE:
        {
          ColoniesBuildableOpts bopts = game_colony_buildable_opts(game);
          colony_screen_open_construction(
            csv, &game->colonies, game->colony_view_id, &bopts
          );
        }
        break;
      case COLONY_HIT_AREA_TILE: {
        game_colony_area_tile_drop(game, colony, cmap, hit.index);
        break;
      }
      case COLONY_HIT_JOBS_ROW:
        if (hit.index >= 0 && hit.index < csv->job_count) {
          const int job = csv->job_ids[hit.index];
          const int ci = game_colony_selected_colonist(game);
          if (ci < 0) {
            set_status(game, "Select a colonist first", NULL);
          } else if (colonies_assign_field(
                       &game->colonies, game->colony_view_id, ci, csv->jobs_tile_index, job
                     )) {
            snprintf(
              game->status, sizeof(game->status), "Working as %s", colony_yield_job_name(job)
            );
          } else {
            set_status(game, "Cannot assign field", NULL);
          }
        }
        colony_screen_close_jobs(csv);
        colony_screen_set_status(csv, game->status);
        break;
      case COLONY_HIT_JOBS_OUTSIDE:
        colony_screen_close_jobs(csv);
        break;
      case COLONY_HIT_BUILDING: {
        game_colony_assign_building_drop(game, hit.index);
        break;
      }
      case COLONY_HIT_CONSTRUCTION_BANNER:
        {
          ColoniesBuildableOpts bopts = game_colony_buildable_opts(game);
          colony_screen_open_construction(
            csv, &game->colonies, game->colony_view_id, &bopts
          );
        }
        break;
      case COLONY_HIT_CONSTRUCTION_CLEAR:
        colonies_clear_construction(&game->colonies, game->colony_view_id);
        colony_screen_close_construction(csv);
        set_status(game, "Construction cleared", NULL);
        colony_screen_set_status(csv, game->status);
        break;
      case COLONY_HIT_CONSTRUCTION_ROW:
        if (hit.index >= 0 && hit.index < csv->buildable_count) {
          const int bid = csv->buildable_ids[hit.index];
          if (colonies_set_construction(&game->colonies, game->colony_view_id, bid)) {
            const ColonizeBuildingType* bt = colonies_building_type(&game->colonies, bid);
            snprintf(
              game->status, sizeof(game->status), "Building %s", bt ? bt->name : "project"
            );
          }
        }
        colony_screen_close_construction(csv);
        colony_screen_set_status(csv, game->status);
        break;
      case COLONY_HIT_CONSTRUCTION_OUTSIDE:
        colony_screen_close_construction(csv);
        break;
      default:
        break;
      }
      return true;
    }

    return true;
  }

  if (game->in_europe) {
    EuropeScreen* eu = &game->europe;
    europe_refresh_harbor_selection(eu);

    /* Bound ships that ticked to 0 turns since we last checked. */
    for (int i = 0; i < eu->bound_ships; ++i) {
      if (eu->bound[i].turns_left <= 0) {
        game_europe_deliver_bound_ships(game);
        break;
      }
    }

    if (eu->menu != EUROPE_MENU_NONE) {
      if (ui_drag_active(&game->ui_drag)) {
        game_ui_drag_clear(game);
      }
      if (input->last_key == COLONIZE_KEY_ESCAPE) {
        europe_menu_close(eu);
        return true;
      }
      int max_sel = 0;
      switch (eu->menu) {
        case EUROPE_MENU_RECRUIT:
          max_sel = EUROPE_POOL_SIZE;
          break;
        case EUROPE_MENU_TRAIN:
          max_sel = eu->train_count;
          break;
        case EUROPE_MENU_PURCHASE:
          max_sel = eu->purchase_count;
          break;
        case EUROPE_MENU_DOCK:
          max_sel = 3;
          break;
        default:
          break;
      }
      if (input->last_key == COLONIZE_KEY_UP && eu->menu_selection > 0) {
        eu->menu_selection--;
      } else if (input->last_key == COLONIZE_KEY_DOWN && eu->menu_selection < max_sel) {
        eu->menu_selection++;
      } else if (input->last_key == COLONIZE_KEY_ENTER) {
        europe_menu_confirm(eu);
      }
      return true;
    }

    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_E) {
      if (ui_drag_active(&game->ui_drag)) {
        game_ui_drag_clear(game);
        if (input->last_key == COLONIZE_KEY_ESCAPE) {
          return true;
        }
      }
      game_ui_drag_clear(game);
      game->in_europe = false;
      game_europe_deliver_bound_ships(game);
      diag_info("Left Europe screen.");
      return true;
    }

    if (input->last_key == COLONIZE_KEY_R) {
      europe_menu_open(eu, EUROPE_MENU_RECRUIT);
      return true;
    }
    if (input->last_key == COLONIZE_KEY_P) {
      europe_menu_open(eu, EUROPE_MENU_PURCHASE);
      return true;
    }
    if (input->last_key == COLONIZE_KEY_T) {
      europe_menu_open(eu, EUROPE_MENU_TRAIN);
      return true;
    }
    for (int ti = 0; ti < input->text_input_len; ++ti) {
      const char ch = input->text_input[ti];
      if (ch == '1') {
        europe_menu_open(eu, EUROPE_MENU_RECRUIT);
        return true;
      }
      if (ch == '2') {
        europe_menu_open(eu, EUROPE_MENU_PURCHASE);
        return true;
      }
      if (ch == '3') {
        europe_menu_open(eu, EUROPE_MENU_TRAIN);
        return true;
      }
    }

    /* L / '=' : buy 100 of selected_market. U : sell best hold. */
    if (input->last_key == COLONIZE_KEY_L) {
      if (eu->selected_harbor < 0) {
        snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
      } else {
        europe_buy_cargo(eu, eu->selected_harbor, eu->selected_market, 100);
      }
      return true;
    }
    if (input->last_key == COLONIZE_KEY_U) {
      if (eu->selected_harbor < 0) {
        snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
      } else {
        const int hold = europe_best_sell_hold(eu, eu->selected_harbor);
        if (hold < 0) {
          snprintf(eu->status, sizeof(eu->status), "%s", "Nothing to sell.");
        } else {
          europe_sell_hold(eu, eu->selected_harbor, hold);
        }
      }
      return true;
    }
    for (int ti = 0; ti < input->text_input_len; ++ti) {
      const char ch = input->text_input[ti];
      if (ch == '=' && eu->selected_harbor >= 0) {
        europe_buy_cargo(eu, eu->selected_harbor, eu->selected_market, 100);
        return true;
      }
      if (ch == '+' && eu->selected_harbor >= 0) {
        europe_buy_cargo(eu, eu->selected_harbor, eu->selected_market, 1);
        return true;
      }
      if ((ch == '-' || ch == '_') && eu->selected_harbor >= 0) {
        EuropeHarborShip* ship = &eu->harbor[eu->selected_harbor];
        int hold = -1;
        for (int hi = 0; hi < EUROPE_SHIP_CARGO_MAX; ++hi) {
          if (ship->hold_goods_amount[hi] > 0 && ship->hold_goods_amount[hi] < 255 &&
              ship->hold_goods_type[hi] == eu->selected_market) {
            hold = hi;
            break;
          }
        }
        if (hold < 0) {
          hold = europe_best_sell_hold(eu, eu->selected_harbor);
        }
        if (hold < 0) {
          snprintf(eu->status, sizeof(eu->status), "%s", "Nothing to sell.");
        } else if (ship->hold_goods_amount[hold] > 1) {
          const int ctype = ship->hold_goods_type[hold];
          const int gained = europe_sell_proceeds(eu, ctype, 1);
          eu->gold += gained;
          ship->hold_goods_amount[hold]--;
          snprintf(eu->status, sizeof(eu->status), "Sold 1 for %d$.", gained);
        } else {
          europe_sell_hold(eu, eu->selected_harbor, hold);
        }
        return true;
      }
    }

    if (input->mouse_right_clicked && ui_drag_active(&game->ui_drag)) {
      game_ui_drag_clear(game);
      return true;
    }

    if (ui_drag_active(&game->ui_drag) && input->mouse_left_released) {
      game_europe_drag_drop(game, input->mouse_x, input->mouse_y);
      return true;
    }

    if (input->mouse_left_clicked) {
      if (ui_drag_active(&game->ui_drag)) {
        return true;
      }
      const EuropeHitResult hit = game_europe_hit(game, input->mouse_x, input->mouse_y);
      switch (hit.kind) {
      case EUROPE_HIT_EXIT:
        game_ui_drag_clear(game);
        game->in_europe = false;
        game_europe_deliver_bound_ships(game);
        diag_info("Left Europe screen (Exit).");
        break;
      case EUROPE_HIT_HARBOR_SHIP:
        eu->selected_harbor = hit.index;
        snprintf(eu->status, sizeof(eu->status), "Selected %s.", eu->harbor[hit.index].name);
        ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_HARBOR_SHIP, hit.index, -1, 0);
        if (game->units_ok && game_icons(game)) {
          const int sprite = europe_ship_icon_sprite(&game->units, &eu->harbor[hit.index]);
          game_ui_drag_set_icon(game, sprite);
        }
        break;
      case EUROPE_HIT_HOLD:
        if (eu->selected_harbor >= 0) {
          EuropeHarborShip* ship = &eu->harbor[eu->selected_harbor];
          if (hit.index >= 0 && hit.index < EUROPE_SHIP_CARGO_MAX &&
              ship->hold_goods_amount[hit.index] > 0 &&
              ship->hold_goods_amount[hit.index] < 255) {
            const int ctype = ship->hold_goods_type[hit.index];
            ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_HOLD, hit.index, -1, 0);
            if (ctype >= 0) {
              game_ui_drag_set_icon(game, EUROPE_CARGO_ICON_BASE + ctype);
            }
          }
        }
        break;
      case EUROPE_HIT_MARKET:
        eu->selected_market = hit.index;
        if (eu->selected_harbor < 0) {
          snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
        } else {
          ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_MARKET, hit.index, -1, 100);
          game_ui_drag_set_icon(game, EUROPE_CARGO_ICON_BASE + hit.index);
        }
        break;
      case EUROPE_HIT_BTN_RECRUIT:
        europe_menu_open(eu, EUROPE_MENU_RECRUIT);
        break;
      case EUROPE_HIT_BTN_PURCHASE:
        europe_menu_open(eu, EUROPE_MENU_PURCHASE);
        break;
      case EUROPE_HIT_BTN_TRAIN:
        europe_menu_open(eu, EUROPE_MENU_TRAIN);
        break;
      case EUROPE_HIT_DOCK:
        eu->menu_dock_index = hit.index;
        europe_menu_open(eu, EUROPE_MENU_DOCK);
        break;
      case EUROPE_HIT_EXPECTED:
        if (hit.index >= 0 && hit.index < eu->expected_ships) {
          ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_EXPECTED_SHIP, hit.index, -1, 0);
          if (game->units_ok && game_icons(game)) {
            const int sprite =
              europe_ship_icon_sprite(&game->units, &eu->expected[hit.index]);
            game_ui_drag_set_icon(game, sprite);
          }
        }
        break;
      case EUROPE_HIT_BOUND:
        if (hit.index >= 0 && hit.index < eu->bound_ships) {
          ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_BOUND_SHIP, hit.index, -1, 0);
          if (game->units_ok && game_icons(game)) {
            const int sprite = europe_ship_icon_sprite(&game->units, &eu->bound[hit.index]);
            game_ui_drag_set_icon(game, sprite);
          }
        }
        break;
      default:
        break;
      }
      return true;
    }

    if (input->last_key == COLONIZE_KEY_S) {
      const int hidx = eu->selected_harbor >= 0 ? eu->selected_harbor : 0;
      game_europe_sail_harbor(game, hidx);
      return true;
    } else if (input->last_key == COLONIZE_KEY_RIGHTBRACKET) {
      europe_cheat_add_gold(eu, 1000);
    } else if (input->last_key == COLONIZE_KEY_LEFTBRACKET) {
      europe_cheat_adjust_tax(eu, -1);
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
    game_open_debug_atlas(game);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_P) {
    if (!game->in_menu && !game->in_europe && !game->in_colony && !game->in_report &&
        game->world_map_ok && game->units_ok) {
      const int sid = game->units.selected_id;
      const ColonizeUnit* su = units_get_const(&game->units, sid);
      if (su && units_is_pioneer(&game->units, sid) && su->moves_left > 0) {
        char msg[96];
        units_pioneer_plow(&game->units, sid, &game->world_map, msg, sizeof(msg));
        set_status(game, msg, NULL);
        return true;
      }
    }
    game_open_pedia_list(game, PEDIA_CAT_CARGO);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_R) {
    if (!game->in_menu && !game->in_europe && !game->in_colony && !game->in_report &&
        game->world_map_ok && game->units_ok) {
      const int sid = game->units.selected_id;
      const ColonizeUnit* su = units_get_const(&game->units, sid);
      if (su && units_is_pioneer(&game->units, sid) && su->moves_left > 0) {
        char msg[96];
        units_pioneer_road(&game->units, sid, &game->world_map, msg, sizeof(msg));
        set_status(game, msg, NULL);
        return true;
      }
    }
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

    /* Mouse: hover selects; click activates (same pattern as Pick Music). */
    BeginMenuLayout menu_layout;
    if (begin_menu_compute_layout(game, 320, 200, &menu_layout)) {
      const int hit =
        begin_menu_option_at_xy(&menu_layout, input->mouse_x, input->mouse_y);
      if (hit >= 0) {
        game->menu_selection = hit;
        if (input->mouse_left_clicked) {
          activate_menu_selection(game);
          if (game->elapsed_ms == UINT32_MAX) {
            return false;
          }
        }
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
    /* Go-To cursor only after ≥1 logical pixel (even if pointer leaves the map). */
    if (game->ui_drag.kind == UI_DRAG_MAP_GOTO && !game->map_goto_dragged_px) {
      const int pdx = input->mouse_x - game->map_goto_down_px;
      const int pdy = input->mouse_y - game->map_goto_down_py;
      if (pdx != 0 || pdy != 0) {
        game->map_goto_dragged_px = true;
        if (game->cursor_ok && game->cursor.sprite_count > 1) {
          ui_drag_set_cursor_from_sheet(&game->ui_drag, &game->cursor, 1);
          game->ui_drag.hotspot_x = 1;
          game->ui_drag.hotspot_y = 0;
        }
      }
    }

    if (game->pick_music.open) {
      const ColonizeFont* pm_font = game->colony_font_ok ? &game->colony_font :
                                    (game->menu_font_ok ? &game->menu_font : NULL);
      pick_music_handle_input(
        &game->pick_music, &game->messages, input, pm_font, game->status, sizeof(game->status)
      );
      return true;
    }

    if (game->unit_stack.open) {
      int select_id = -1;
      unit_stack_handle_input(&game->unit_stack, &game->units, input, &select_id);
      if (select_id >= 0) {
        game_select_unit(game, select_id);
      }
      return true;
    }

    /* Drop selection if the active unit no longer has moves (e.g. after load). */
    if (game->units_ok && game->units.selected_id >= 0) {
      const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
      if (!game_unit_selectable(game, sel)) {
        const int tx = sel ? sel->x : game->map_cursor_x;
        const int ty = sel ? sel->y : game->map_cursor_y;
        game_select_tile(game, tx, ty);
      }
    }

    /* F1 terrain pedia at cursor; F2–F10 adviser / report screens. */
    if (input->last_key >= COLONIZE_KEY_F1 && input->last_key <= COLONIZE_KEY_F10) {
      game_handle_report_fkey(game, input->last_key);
      return true;
    }

    /* Alt-W/I/N unlocks CHEAT; Alt-W alone turns it off (COLONIZE README). */
    if (input->alt_held && input->last_key != COLONIZE_KEY_NONE) {
      const ColonizeKey k = input->last_key;
      if (k == COLONIZE_KEY_W || k == COLONIZE_KEY_I || k == COLONIZE_KEY_N) {
        if (game->map_menu.cheat_visible) {
          if (k == COLONIZE_KEY_W) {
            map_menu_set_cheat_visible(&game->map_menu, false);
            game->cheat_unlock_step = 0;
            set_status(game, "Cheat mode off", NULL);
          }
          return true;
        }
        if (k == COLONIZE_KEY_W) {
          game->cheat_unlock_step = 1;
        } else if (k == COLONIZE_KEY_I && game->cheat_unlock_step == 1) {
          game->cheat_unlock_step = 2;
        } else if (k == COLONIZE_KEY_N && game->cheat_unlock_step == 2) {
          map_menu_set_cheat_visible(&game->map_menu, true);
          game->cheat_unlock_step = 0;
          set_status(game, "Cheat mode on", NULL);
        } else {
          game->cheat_unlock_step = (k == COLONIZE_KEY_W) ? 1 : 0;
        }
        return true;
      }
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

    if ((input->mouse_left_clicked || input->mouse_right_clicked || input->mouse_left_released ||
         (ui_drag_active(&game->ui_drag) && game->ui_drag.kind == UI_DRAG_MAP_GOTO &&
          input->mouse_left_down)) &&
        game->world_map_ok) {
      const int tile_w = MAP_VIEW_TILE_W;
      const int tile_h = MAP_VIEW_TILE_H;
      const int map_origin_x = 0;
      const int map_origin_y = MAP_VIEW_ORIGIN_Y;
      const int view_cols = MAP_VIEW_TILE_COLS;
      const int view_rows = MAP_VIEW_TILE_ROWS;
      if (input->mouse_y < map_origin_y) {
        if (input->mouse_left_released && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
          game_ui_drag_clear(game);
        }
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
        if (game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
          if (input->mouse_left_released || input->mouse_right_clicked) {
            game_ui_drag_clear(game);
          }
          return true;
        }
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
        if (input->mouse_left_released && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
          game_ui_drag_clear(game);
        }
        return true;
      }

      const int mx = view_x + (input->mouse_x - map_origin_x) / tile_w;
      const int my = view_y + (input->mouse_y - map_origin_y) / tile_h;
      if (mx < 0 || my < 0 || mx >= (int)game->world_map.width || my >= (int)game->world_map.height) {
        if (input->mouse_left_released && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
          game_ui_drag_clear(game);
        }
        return true;
      }

      /* Active map go-to drag: track / cancel / commit. */
      if (game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
        if (mx != game->map_goto_anchor_x || my != game->map_goto_anchor_y) {
          game->map_goto_left_tile = true;
        }
        if (input->mouse_right_clicked) {
          game_ui_drag_clear(game);
          return true;
        }
        if (input->mouse_left_released) {
          const int uid = game->ui_drag.unit_id;
          const ColonizeUnit* u = game->units_ok ? units_get_const(&game->units, uid) : NULL;
          game_ui_drag_clear(game);
          if (!u || !game_unit_selectable(game, u)) {
            return true;
          }
          if (!game->map_goto_left_tile) {
            /* Short click: colony enter or pan (legacy). */
            const int cid = colonies_id_at(&game->colonies, mx, my);
            if (cid >= 0) {
              const ColonizeColony* col = colonies_get(&game->colonies, cid);
              if (col && col->nation_id == game->human_nation) {
                game_select_tile(game, mx, my);
                game_enter_colony_at_cursor(game);
                return true;
              }
            }
            game_set_view_center(game, mx, my);
            return true;
          }
          if (mx == u->x && my == u->y) {
            return true;
          }
          if (units_set_goto(
                &game->units, uid, &game->world_map, mx, my, &game->colonies
              )) {
            /* Movement is paced in game_update (10 steps/sec). */
            snprintf(game->status, sizeof(game->status), "Go to (%d,%d)", mx, my);
            game_set_view_center(game, u->x, u->y);
          } else {
            set_status(game, "Cannot go there", NULL);
          }
          return true;
        }
        return true;
      }

      if (input->mouse_right_clicked) {
        /* Right-click: unselect unit (if any) and select the tile. */
        game_select_tile(game, mx, my);
        return true;
      }

      if (!input->mouse_left_clicked) {
        return true;
      }

      /* Left-click — with a selected unit, begin go-to drag. */
      if (game->units.selected_id >= 0 && game->units_ok) {
        const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
        if (game_unit_selectable(game, sel)) {
          ui_drag_begin(
            &game->ui_drag, UI_DRAG_MAP_GOTO, mx, game->units.selected_id, my
          );
          game->map_goto_anchor_x = mx;
          game->map_goto_anchor_y = my;
          game->map_goto_left_tile = false;
          game->map_goto_down_px = input->mouse_x;
          game->map_goto_down_py = input->mouse_y;
          game->map_goto_dragged_px = false;
          return true;
        }
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

      if (game->units_ok &&
          unit_stack_try_open(
            &game->unit_stack, &game->units, mx, my, game->human_nation
          )) {
        set_status(game, "Choose unit from stack", NULL);
        return true;
      }

      {
        int stack_ids[UNITS_TILE_STACK_MAX];
        const int n = game->units_ok ? units_collect_tile_stack(
                                         &game->units, mx, my, game->human_nation, stack_ids, UNITS_TILE_STACK_MAX
                                       )
                                     : 0;
        if (n == 1) {
          game_select_unit(game, stack_ids[0]);
          return true;
        }
      }

      const int owned = game_owned_unit_at(game, mx, my);
      if (owned >= 0) {
        game_select_unit(game, owned);
        return true;
      }

      /* Human unit with no moves: select the tile under it. */
      {
        const int any_id = units_id_at(&game->units, mx, my);
        const ColonizeUnit* any = units_get_const(&game->units, any_id);
        if (any && any->nation_id == game->human_nation) {
          game_select_tile(game, mx, my);
          return true;
        }
      }

      game_select_tile(game, mx, my);
      return true;
    }
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    if (ui_drag_active(&game->ui_drag) && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
      game_ui_drag_clear(game);
      return true;
    }
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

  if (input->last_key == COLONIZE_KEY_ENTER && game->world_map_ok) {
    const int cid = colonies_id_at(&game->colonies, game->map_cursor_x, game->map_cursor_y);
    if (cid >= 0) {
      const ColonizeColony* col = colonies_get(&game->colonies, cid);
      if (col && col->nation_id == game->human_nation) {
        game_enter_colony_at_cursor(game);
        return true;
      }
    }
    if (!game->units_ok) {
      return true;
    }
    const int at_cursor = game_owned_unit_at(game, game->map_cursor_x, game->map_cursor_y);
    if (at_cursor >= 0 && at_cursor != game->units.selected_id) {
      game_select_unit(game, at_cursor);
    } else if (game->units.selected_id >= 0) {
      ColonizeUnit* selected = units_get(&game->units, game->units.selected_id);
      if (selected &&
          (selected->x != game->map_cursor_x || selected->y != game->map_cursor_y)) {
        if (game_try_unit_move(game, game->map_cursor_x, game->map_cursor_y)) {
          return true;
        }
      } else if (at_cursor >= 0) {
        game_select_unit(game, at_cursor);
      }
    } else if (at_cursor >= 0) {
      game_select_unit(game, at_cursor);
    } else {
      /* Enter on exhausted human unit / empty tile → tile select. */
      const int any_id = units_id_at(&game->units, game->map_cursor_x, game->map_cursor_y);
      const ColonizeUnit* any = units_get_const(&game->units, any_id);
      if (any && any->nation_id == game->human_nation) {
        game_select_tile(game, any->x, any->y);
      }
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
      const int exit_x = ship->x;
      const int exit_y = ship->y;
      const bool exit_east = exit_x >= (int)game->world_map.width / 2;
      int type_index = -1;
      char ship_name[32];
      int cargo_types[EUROPE_SHIP_CARGO_MAX];
      int cargo_profs[EUROPE_SHIP_CARGO_MAX];
      int cargo_count = 0;
      int hold_types[EUROPE_SHIP_CARGO_MAX];
      int hold_amts[EUROPE_SHIP_CARGO_MAX];
      memset(hold_types, 0, sizeof(hold_types));
      memset(hold_amts, 0, sizeof(hold_amts));
      game_europe_capture_pax_professions(
        &game->units, sid, cargo_profs, EUROPE_SHIP_CARGO_MAX
      );
      if (!units_despawn_ship_with_cargo(
            &game->units,
            sid,
            &type_index,
            ship_name,
            sizeof(ship_name),
            cargo_types,
            &cargo_count,
            EUROPE_SHIP_CARGO_MAX,
            hold_types,
            hold_amts,
            EUROPE_SHIP_CARGO_MAX
          )) {
        set_status(game, "Failed to sail ship", NULL);
      } else {
        const ColonizeUnitType* ut = units_type(&game->units, type_index);
        const int movement = ut ? ut->movement : 5;
        if (!europe_enqueue_expected(
              &game->europe,
              type_index,
              ship_name,
              cargo_types,
              cargo_profs,
              cargo_count,
              hold_types,
              hold_amts,
              exit_x,
              exit_y,
              exit_east,
              movement
            )) {
          /* Lane full — put the ship back on the map with passengers. */
          const int restored = units_spawn_ship_with_cargo(
            &game->units,
            type_index,
            exit_x,
            exit_y,
            cargo_types,
            cargo_count,
            hold_types,
            hold_amts
          );
          if (restored >= 0) {
            game->units.selected_id = restored;
          }
          set_status(game, "Europe lane is full", NULL);
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
          diag_info(
            "Sailed %s to Europe (exit %d,%d cargo=%d)", ship_name, exit_x, exit_y, cargo_count
          );
        }
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
    } else {
      const ColonizeUnit* ship = units_get_const(&game->units, sid);
      const int pax_id = (ship && ship->cargo_count > 0) ? ship->cargo_ids[0] : -1;
      if (!units_unload(
            &game->units,
            sid,
            &game->world_map,
            game->map_cursor_x,
            game->map_cursor_y,
            &game->colonies
          )) {
        set_status(game, "Cannot unload (need adjacent free land)", NULL);
      } else {
        if (pax_id >= 0) {
          game->units.selected_id = pax_id;
        }
        set_status(game, "Unit unloaded", NULL);
      }
    }
  }

  /* F: fortify selected land unit. */
  if (input->last_key == COLONIZE_KEY_F && game->world_map_ok && game->units_ok) {
    const int uid = game->units.selected_id;
    if (uid >= 0 && units_order_fortify(&game->units, uid)) {
      set_status(game, "Fortifying", NULL);
      game_wait_next_unit(game);
      return true;
    }
  }

  /* Shift+D: disband selected unit. Plain D remains dock deploy. */
  if (input->last_key == COLONIZE_KEY_D && input->shift_held && game->units_ok) {
    const int uid = game->units.selected_id;
    if (uid < 0 || !units_disband(&game->units, uid)) {
      set_status(game, "Cannot disband", NULL);
    } else {
      set_status(game, "Unit disbanded", NULL);
      game_wait_next_unit(game);
    }
    return true;
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
        const int profession = founder ? founder->profession : UNITS_JOB_NONE;
        int tools = 0;
        int muskets = 0;
        int horses = 0;
        units_founder_loot(&game->units, uid, &tools, &muskets, &horses);
        const int cid = colonies_found(
          &game->colonies,
          &game->world_map,
          cx,
          cy,
          game->human_nation,
          type_index,
          profession,
          tools,
          muskets,
          horses
        );
        if (cid >= 0) {
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

  {
    int dx = 0;
    int dy = 0;
    if (game_key_move_delta(input->last_key, &dx, &dy)) {
      if (game->units.selected_id >= 0 && game->world_map_ok && game->units_ok) {
        ColonizeUnit* selected = units_get(&game->units, game->units.selected_id);
        if (selected && selected->moves_left > 0) {
          const int dest_x = selected->x + dx;
          const int dest_y = selected->y + dy;
          game_try_unit_move(game, dest_x, dest_y);
        } else if (selected) {
          /* Safety: exhausted selection → tile mode. */
          game_select_tile(game, selected->x, selected->y);
        }
      } else {
        int nx = game->map_cursor_x + dx;
        int ny = game->map_cursor_y + dy;
        if (nx < 0) {
          nx = 0;
        }
        if (ny < 0) {
          ny = 0;
        }
        if (nx > map_max_x) {
          nx = map_max_x;
        }
        if (ny > map_max_y) {
          ny = map_max_y;
        }
        game->map_cursor_x = nx;
        game->map_cursor_y = ny;
        game_set_view_center(game, game->map_cursor_x, game->map_cursor_y);
      }
    }
  }

  if (input->last_key == COLONIZE_KEY_S) {
    /* Selected map unit → sentry; otherwise save (menu Save still works). */
    if (game->units_ok && game->world_map_ok) {
      const int uid = game->units.selected_id;
      const ColonizeUnit* su = units_get_const(&game->units, uid);
      if (su && su->active && units_is_on_map(su) && !units_is_sea(&game->units, uid)) {
        if (units_order_sentry(&game->units, uid)) {
          set_status(game, "Sentry", NULL);
          game_wait_next_unit(game);
          return true;
        }
      }
    }
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

/* Same geometry as render — used for mouse hit-testing. */
static bool begin_menu_compute_layout(
  const ColonizeGameState* game,
  int fb_w,
  int fb_h,
  BeginMenuLayout* out
) {
  if (!game || !out || fb_w <= 0 || fb_h <= 0) {
    return false;
  }
  memset(out, 0, sizeof(*out));
  const ColonizeFont* font = begin_menu_font(game);
  out->line_h = font ? (font->max_height + 2) : 8;
  out->title_h = font ? font->max_height : 6;
  out->title_pad_top = 3;
  out->title_pad_x = 2;
  out->option_pad_x = 8;
  out->gap_after_title = 4;
  out->option_count = game->menu_option_count;
  const int options_h = out->option_count * out->line_h;
  /*
   * Outer height from the previous pad_y=6 / line_h layout, then -12 from the
   * bottom (dialog_y unchanged). Inner title spacing is tighter (3 / title_h / 4).
   */
  int dialog_h = POPUP_FRAME_INSET * 2 + 6 +
                 (game->menu_version_line[0] ? out->line_h + 4 : 0) + options_h + 6 - 12;
  if (dialog_h < 24) {
    dialog_h = 24;
  }

  int dialog_w = game->menu_dialog_width > 0 ? game->menu_dialog_width : 160;
  if (dialog_w > fb_w) {
    dialog_w = fb_w;
  }
  int dialog_x = (fb_w - dialog_w) / 2;
  int dialog_y = game->menu_dialog_y;
  if (dialog_y < 0) {
    dialog_y = 0;
  }
  if (dialog_y + dialog_h > fb_h) {
    dialog_y = fb_h - dialog_h;
    if (dialog_y < 0) {
      dialog_y = 0;
      dialog_h = fb_h;
    }
  }

  out->dialog_x = dialog_x;
  out->dialog_y = dialog_y;
  out->dialog_w = dialog_w;
  out->dialog_h = dialog_h;
  out->inner_x = dialog_x + POPUP_FRAME_INSET;
  out->inner_y = dialog_y + POPUP_FRAME_INSET;
  out->inner_w = dialog_w - POPUP_FRAME_INSET * 2;
  out->inner_h = dialog_h - POPUP_FRAME_INSET * 2;
  if (out->inner_w <= 0 || out->inner_h <= 0) {
    return false;
  }

  int text_y = out->inner_y + out->title_pad_top;
  if (game->menu_version_line[0]) {
    text_y += out->title_h + out->gap_after_title;
  }
  out->list_y0 = text_y;
  return true;
}

static int begin_menu_option_at_xy(const BeginMenuLayout* layout, int mx, int my) {
  if (!layout || layout->option_count <= 0 || layout->line_h <= 0) {
    return -1;
  }
  if (mx < layout->inner_x || mx >= layout->inner_x + layout->inner_w) {
    return -1;
  }
  if (my < layout->list_y0) {
    return -1;
  }
  const int rel = my - layout->list_y0;
  const int idx = rel / layout->line_h;
  if (idx < 0 || idx >= layout->option_count) {
    return -1;
  }
  return idx;
}

static void game_render_begin_menu(
  const ColonizeGameState* game,
  ColonizeFramebuffer8* framebuffer
) {
  if (!game || !framebuffer || !framebuffer->pixels) {
    return;
  }

  BeginMenuLayout L;
  if (!begin_menu_compute_layout(game, framebuffer->width, framebuffer->height, &L)) {
    return;
  }

  const ColonizeFont* font = begin_menu_font(game);
  int inner_x = 0;
  int inner_y = 0;
  int inner_w = 0;
  int inner_h = 0;
  popup_draw(
    framebuffer,
    L.dialog_x,
    L.dialog_y,
    L.dialog_w,
    L.dialog_h,
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

  int text_y = inner_y + L.title_pad_top;
  if (game->menu_version_line[0]) {
    const int tw = begin_menu_text_width(font, game->menu_version_line);
    int tx = inner_x + (inner_w - tw) / 2;
    if (tx < inner_x + L.title_pad_x) {
      tx = inner_x + L.title_pad_x;
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
    text_y += L.title_h + L.gap_after_title;
  }

  for (int i = 0; i < game->menu_option_count; ++i) {
    const int row_y = text_y + i * L.line_h;
    const bool selected = (i == game->menu_selection);
    if (selected) {
      /* 1px inset from each side of the inner content edge. */
      begin_menu_fill_rect(
        framebuffer,
        inner_x + 1,
        row_y - 1,
        inner_x + inner_w - 2,
        row_y + L.line_h - 2,
        game->menu_col_select
      );
    }
    font_draw_text(
      font, framebuffer, inner_x + L.option_pad_x, row_y, game->menu_options[i], game->menu_col_basic
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

  if (new_game_active(&game->new_game)) {
    ColonizePopupColors popup_cols;
    popup_colors_from_ui(&popup_cols);
    /* Hit-test layout is updated during render. */
    NewGameWizard* ng = (NewGameWizard*)&game->new_game;
    ng->ui_font = game->intro_font_ok ? &game->intro_font
      : (game->colony_font_ok ? &game->colony_font
                              : (game->menu_font_ok ? &game->menu_font : NULL));
    ng->tiny_font = game->colony_font_ok ? &game->colony_font
      : (game->menu_font_ok ? &game->menu_font : ng->ui_font);
    ng->lore_font = ng->ui_font;
    ng->wood_tile = game->menu_opentile_ok ? &game->menu_opentile : NULL;
    ng->woodpanl = game->pedia_wood_ok ? &game->pedia_wood : NULL;
    ng->labels_txt = game->labels_ok ? &game->labels : NULL;
    uint8_t basic = COLONIZE_COL_BASIC;
    uint8_t hilite = COLONIZE_COL_HILITE;
    uint8_t select = COLONIZE_COL_SELECT;
    /* Bright selection border on DIFFICUL/NATIONS/CUSTOMIZ own palettes. */
    if (ng->phase == NEW_GAME_PHASE_AMERICA_CHOICE || ng->phase == NEW_GAME_PHASE_MAP_PICK) {
      /*
       * Black screen + OPENTILE popup: same palette/colors as @BEGINMENU
       * (OPENMENU / remapped WOODPANL @COLORS). Do not use terrain palette.
       */
      if (game->menu_bg_ok && game->menu_bg.has_palette) {
        *palette = game->menu_bg.palette;
      } else if (game->menu_opentile_ok && game->menu_opentile.has_palette) {
        *palette = game->menu_opentile.palette;
      }
      popup_cols = game->menu_popup_colors;
      basic = game->menu_col_basic;
      hilite = game->menu_col_hilite;
      select = game->menu_col_select;
    } else if (ng->phase == NEW_GAME_PHASE_DIFFICULTY && ng->difficul_ok) {
      hilite = palette_nearest_rgb(&ng->difficul_pik.palette, 0, 255, 0);
      basic = palette_nearest_rgb(&ng->difficul_pik.palette, 0, 180, 0);
    } else if (ng->phase == NEW_GAME_PHASE_NATION && ng->nations_ok) {
      hilite = palette_nearest_rgb(&ng->nations_pik.palette, 0, 255, 0);
      basic = palette_nearest_rgb(&ng->nations_pik.palette, 0, 180, 0);
    } else if (ng->phase == NEW_GAME_PHASE_CUSTOMIZE && ng->customiz_ok) {
      /* Focused column yellow; other selection rects green (DOS 0xe / 0xa). */
      hilite = palette_nearest_rgb(&ng->customiz_pik.palette, 255, 255, 0);
      basic = palette_nearest_rgb(&ng->customiz_pik.palette, 0, 220, 0);
    } else if (
      (ng->phase == NEW_GAME_PHASE_LEADER_NAME || ng->phase == NEW_GAME_PHASE_NATION_LORE_A ||
       ng->phase == NEW_GAME_PHASE_NATION_LORE_B) &&
      game->pedia_wood_ok && game->pedia_wood.has_palette
    ) {
      basic = palette_nearest_rgb(&game->pedia_wood.palette, 40, 140, 40);
      hilite = palette_nearest_rgb(&game->pedia_wood.palette, 220, 220, 40);
    }
    new_game_render(
      ng,
      framebuffer,
      palette,
      &popup_cols,
      basic,
      hilite,
      select
    );
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
          if (!map_tile_seen_by(&game->world_map, mx, my, game->human_nation)) {
            /* Unexplored: leave black (framebuffer cleared above). */
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
          /* Fog transitional edges: PHYS0 104–107 black fringe toward unseen. */
          {
            const int edges =
              map_fog_edge_count(&game->world_map, mx, my, game->human_nation);
            for (int ei = 0; ei < edges; ++ei) {
              const int mask = map_fog_edge_mask_sprite_at(
                &game->world_map, mx, my, game->human_nation, ei
              );
              if (mask >= 0) {
                blit_map_sprite(
                  &game->phys0, mask, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
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
      map_origin_y,
      game->world_map_ok ? &game->world_map : NULL,
      game->human_nation
    );
  }

  if (game->col1_ok && game->unit_icons_ok) {
    map_panel_render_tribes_on_map(
      &game->col1,
      &game->unit_icons,
      framebuffer,
      view_x,
      view_y,
      view_cols,
      view_rows,
      tile_w,
      tile_h,
      map_origin_x,
      map_origin_y,
      game->world_map_ok ? &game->world_map : NULL,
      game->human_nation
    );
  }

  if (game->units_ok && game->unit_icons_ok) {
    /* Half-period 500ms → full blink cycle 1s (was 250ms / 500ms cycle). */
    const bool blink_on = ((game->elapsed_ms / 500u) % 2u) == 0u;
    const ColonizeFont* chrome_font = game->colony_font_ok ? &game->colony_font
      : (game->menu_font_ok ? &game->menu_font : NULL);
    units_render_on_map(
      &game->units,
      &game->unit_icons,
      chrome_font,
      framebuffer,
      view_x,
      view_y,
      view_cols,
      view_rows,
      tile_w,
      tile_h,
      map_origin_x,
      map_origin_y,
      blink_on,
      game->world_map_ok ? &game->world_map : NULL,
      game->human_nation
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
      game->human_nation,
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
    if (game->unit_stack.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      unit_stack_render(
        (UnitStackPopup*)&game->unit_stack,
        &game->units,
        game->unit_icons_ok ? &game->unit_icons : NULL,
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
  /* Debug measure: original-resolution pixel under the pointer (for layout). */
  if (game->debug_show_mouse_coords) {
    const ColonizeFont* font = game->colony_font_ok ? &game->colony_font
      : (game->menu_font_ok ? &game->menu_font : NULL);
    const int mx = game->debug_mouse_x;
    const int my = game->debug_mouse_y;
    if (font && mx >= 0 && my >= 0 && mx < framebuffer->width && my < framebuffer->height) {
      char label[24];
      snprintf(label, sizeof(label), "%d,%d", mx, my);
      int tx = mx + 12;
      int ty = my + 2;
      if (tx > framebuffer->width - 40) {
        tx = mx - 40;
      }
      if (ty > framebuffer->height - 8) {
        ty = framebuffer->height - 8;
      }
      if (tx < 0) {
        tx = 0;
      }
      if (ty < 0) {
        ty = 0;
      }
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          font_draw_text(font, framebuffer, tx + dx, ty + dy, label, 0);
        }
      }
      font_draw_text(font, framebuffer, tx, ty, label, 15);
    }
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

  game->debug_mouse_x = mouse_x;
  game->debug_mouse_y = mouse_y;

  const ColonizePalette* pal = NULL;
  /* Map go-to uses CURSOR.SS #1; cargo/unit icons use ICONS.SS palette. */
  if (ui_drag_active(&game->ui_drag) && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
    if (game->cursor_ok && game->cursor.has_palette) {
      pal = &game->cursor.palette;
    }
  } else if (ui_drag_active(&game->ui_drag) && game->ui_drag.cursor_ok) {
    if (game->unit_icons_ok && game->unit_icons.has_palette) {
      pal = &game->unit_icons.palette;
    } else if (game->in_europe && game->europe_ok && game->europe.background.has_palette) {
      pal = &game->europe.background.palette;
    } else if (game->in_colony && game->colony_screen_ok && game->colony_screen.frame.has_palette) {
      pal = &game->colony_screen.frame.palette;
    }
  }
  if (!pal) {
    if (game->cursor_ok && game->cursor.has_palette) {
      pal = &game->cursor.palette;
    } else if (game->map_palette_ok) {
      pal = &game->map_palette;
    } else if (game->palette_ok) {
      pal = &game->palette;
    }
  }

  if (pal && game->cursor_ok) {
    ui_drag_apply_cursor(
      &game->ui_drag, platform, pal, &game->cursor, &game->mouse_cursor_built
    );
  }

  /* Game cursor over the full 320x200 frame on every screen (menu, map, reports…). */
  const bool on_game_frame =
    mouse_x >= 0 && mouse_x < 320 && mouse_y >= 0 && mouse_y < 200;

  if (on_game_frame && game->mouse_cursor_built) {
    platform_show_game_mouse_cursor(platform, true);
  } else if (on_game_frame && ui_drag_active(&game->ui_drag) && game->ui_drag.cursor_ok) {
    platform_show_game_mouse_cursor(platform, true);
  } else {
    platform_show_game_mouse_cursor(platform, false);
  }
}

bool game_in_menu(const ColonizeGameState* game) {
  return game && game->in_menu;
}

bool game_in_new_game(const ColonizeGameState* game) {
  return game && new_game_active(&game->new_game);
}

int game_human_nation(const ColonizeGameState* game) {
  return game ? game->human_nation : 0;
}

int game_difficulty(const ColonizeGameState* game) {
  return game ? game->difficulty : 0;
}

const char* game_leader_name(const ColonizeGameState* game) {
  return game ? game->leader_name : "";
}
