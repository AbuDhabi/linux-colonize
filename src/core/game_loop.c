#include "core/game_loop.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

#include "core/assets.h"
#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_goals.h"
#include "core/ai_king.h"
#include "core/ai_popup.h"
#include "core/cheat_list_dialog.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_screen.h"
#include "core/combat_strength.h"
#include "core/debug_atlas.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/howmuch_dialog.h"
#include "core/map.h"
#include "core/map_gen.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/name_entry_dialog.h"
#include "core/new_game.h"
#include "core/options_dialog.h"
#include "core/pedia.h"
#include "core/pik.h"
#include "core/pick_music.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/save_load_dialog.h"
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
#include "core/combat_analysis.h"
#include "core/version.h"
#include "platform/diagnostics.h"

#define MENU_MAX_OPTIONS 12

typedef enum GameMapConfirm {
  GAME_MAP_CONFIRM_NONE = 0,
  GAME_MAP_CONFIRM_DISBAND,
  GAME_MAP_CONFIRM_OVERBOARD,
  GAME_MAP_CONFIRM_QUIT,
  GAME_MAP_CONFIRM_RETIRE,
  GAME_MAP_CONFIRM_TRADE_DELETE,
  GAME_MAP_CONFIRM_TITLE_EXIT,
  GAME_MAP_CONFIRM_BUY_CONSTRUCTION,
  GAME_MAP_CONFIRM_FOUND_INLAND
} GameMapConfirm;

/* Hall of Fame: ranked table of retired Colonization Scores, persisted to
 * HOF.TXT (one line per entry, highest score first). No decomp evidence for
 * the DOS table's exact size / on-disk layout; 10 entries is our own choice
 * (COLONIZE_HOF_ROW_MAX, shared with the reports.c screen renderer). */
#define COLONIZE_HOF_MAX COLONIZE_HOF_ROW_MAX

typedef struct ColonizeHofEntry {
  char leader[NEW_GAME_LEADER_NAME_MAX];
  char nation[24];
  int score;
  int year;
  int difficulty; /* 0 Discoverer .. 4 Viceroy */
} ColonizeHofEntry;

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
  int map_zoom; /* 0..3 — VIEW Zoom In/Out/Level N. FUN_2b5a_0f92 DS:0x184; 0 = 15×12 native. */
  /*
   * VIEW ~Hidden Terrain (H): 0 = off; 1..3 = DOS's three peel passes (units/
   * settlements; non-exempt land PHYS; hills+forest). Auto-advances on a
   * timer, then holds at 3 until any click/keypress cancels back to 0.
   */
  int hidden_terrain_phase;
  uint32_t hidden_terrain_phase_ms; /* elapsed_ms when the current phase started */
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
  CheatListDialog cheat_list;
  HowmuchDialog howmuch;
  NameEntryDialog name_entry;
  OptionsDialog options_dlg;
  CombatAnalysisDialog combat_analysis;
  GameMapConfirm map_confirm;
  int map_confirm_payload; /* unit id / trade route slot / … */
  int trade_select_mode; /* 0=idle, 1=begin route, 2=edit, 3=delete */
  /* Thin TRADE Edit cargo picker: route/stop being edited; phase unload→load. */
  int trade_edit_route; /* -1 = idle */
  int trade_edit_stop;
  bool trade_edit_need_load; /* after unload confirm, open load picker */
  AiPopupState ai_popups;
  SaveLoadDialog save_load;
  UnitStackPopup unit_stack;
  ColonizeMsgCatalog labels;
  bool labels_ok;
  ColonizeMsgCatalog debug_txt;
  bool debug_txt_ok;
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
  bool report_exits_to_menu; /* Retire: close score → title menu */
  bool congress_page2; /* Continental Congress is two pages; closing p1 shows p2 */
  int labor_detail_job; /* -1 = Labor report grid; >=0 = zoomed job id detail view */
  int economic_page; /* 0 = European Trade; >=1 = Cargo in Port page N */
  int colony_page; /* 0..k-1 = Military Garrisons; k..2k-1 = Sons of Liberty */
  ColonizeHofEntry hof_entries[COLONIZE_HOF_MAX]; /* ranked desc; session + HOF.TXT */
  int hof_count;
  bool in_hall_of_fame; /* title-menu "View Hall of Fame" screen (reports_render_hall_of_fame) */
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
  bool debug_show_strategy; /* CHEAT Show Strategy: per-nation top AI goal overlay */
  bool debug_show_colony_sites; /* CHEAT Show Colony Sites: ai_goals_best_found_tile overlay */
  uint16_t debug_flags_mask; /* CHEAT Debug Info Flags (@OPTIONS, 7 bits); bits 1/3 shadow
                               * col1.game_options.show_indian_moves/show_foreign_moves */
  int cheat_create_stage; /* CHEAT Create Unit: 0=main @CREATE, 1=@CSHIP, 2=@FOREIGN, 3=@FOREIGN2 */
  int cheat_create_pending_nation; /* Foreign Unit stage: nation picked at @FOREIGN */
  int cheat_unlock_step;   /* 0=expect W, 1=I, 2=N for Alt-WIN */
  /*
   * Cheat Reveal Map viewpoint: -2 = normal (human fog), -1 = complete map,
   * 0..3 = view that European nation's seen bits.
   */
  int fog_view;
  UiDragSession ui_drag;
  int map_goto_anchor_x; /* tile under pointer when map goto drag began */
  int map_goto_anchor_y;
  bool map_goto_left_tile; /* true once pointer leaves the anchor tile */
  int map_goto_down_px; /* logical 320×200 mouse at drag begin */
  int map_goto_down_py;
  bool map_goto_dragged_px; /* true once pointer moved ≥1 logical pixel */
  uint32_t goto_step_accum_ms; /* paces Go-To at 10 steps/sec */
  bool map_goto_place_mode; /* ORDERS Go to Place: next map click sets goto */
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
  /* Set from main each frame; used by Combat Analysis nested present loop. */
  ColonizePlatform* platform;
};

static void game_combat_analysis_present(const ColonizeCombatEngagement* eng, void* user) {
  ColonizeGameState* game = (ColonizeGameState*)user;
  if (!game || !eng || !game->units_ok) {
    return;
  }
  if (!combat_analysis_open(&game->combat_analysis, &game->units, eng)) {
    return;
  }
  /* Headless / no platform: auto-dismiss (tests never set presenter). */
  if (!game->platform) {
    combat_analysis_close(&game->combat_analysis);
    return;
  }
  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  ColonizePalette pal;
  memset(pixels, 0, sizeof(pixels));
  while (game->combat_analysis.open) {
    ColonizeInputState input = {0};
    if (!platform_poll_input(game->platform, &input) || input.quit_requested) {
      combat_analysis_close(&game->combat_analysis);
      break;
    }
    (void)combat_analysis_handle_input(&game->combat_analysis, &input);
    game_render(game, &fb, &pal);
    if (!platform_present(game->platform, &fb, &pal)) {
      combat_analysis_close(&game->combat_analysis);
      break;
    }
    platform_sleep_ms(16);
  }
}

static void game_set_view_center(ColonizeGameState* game, int x, int y);

static bool game_move_is_near_human(
  const ColonizeGameState* game,
  const ColonizeUnit* mover,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int x,
  int y
) {
  if (!game || !mover || !map || !colonies || !game->col1_ok) {
    return false;
  }
  if (game->col1.head.show_entire_map ||
      map_tile_seen_by(map, x, y, game->human_nation)) {
    return true;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* human = &game->units.units[i];
    if (!human->active || human->nation_id != game->human_nation ||
        !units_is_on_map(human)) {
      continue;
    }
    if (abs(human->x - x) <= 1 && abs(human->y - y) <= 1) {
      return true;
    }
  }
  for (int i = 0; i < colonies->colony_count; ++i) {
    const ColonizeColony* colony = &colonies->colonies[i];
    if (colony->active && colony->nation_id == game->human_nation &&
        abs(colony->x - x) <= 1 && abs(colony->y - y) <= 1) {
      return true;
    }
  }
  (void)mover;
  return false;
}

static void game_move_watch(
  void* user,
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int unit_id,
  int from_x,
  int from_y,
  int to_x,
  int to_y
) {
  ColonizeGameState* game = (ColonizeGameState*)user;
  const ColonizeUnit* unit = pool ? units_get_const(pool, unit_id) : NULL;
  if (!game || !unit || !map || !colonies || !game->platform || !game->col1_ok ||
      unit->nation_id == game->human_nation) {
    return;
  }
  const bool show =
    (unit->nation_id >= 0 && unit->nation_id < 4)
      ? game->col1.head.game_options.show_foreign_moves != 0
      : game->col1.head.game_options.show_indian_moves != 0;
  if (!show || !game_move_is_near_human(game, unit, map, colonies, to_x, to_y)) {
    return;
  }
  game_set_view_center(game, to_x, to_y);
  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  ColonizePalette pal;
  game_render(game, &fb, &pal);
  if (platform_present(game->platform, &fb, &pal)) {
    const uint32_t delay_ms =
      game->col1.head.game_options.fast_piece_slide ? 80u : 100u;
    platform_sleep_ms(delay_ms);
  }
  (void)from_x;
  (void)from_y;
}

static void game_bind_combat_analysis(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  combat_analysis_set_presenter(game_combat_analysis_present, game);
  units_set_combat_human_nation(game->human_nation);
  units_set_combat_music_hooks(sound_play, sound_active_song_id);
}

static void game_refresh_orders_menu(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  MapMenuOrdersContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = game->units_ok ? &game->units : NULL;
  ctx.map = game->world_map_ok ? &game->world_map : NULL;
  ctx.colonies = game->colonies_ok ? &game->colonies : NULL;
  ctx.selected_id = game->units_ok ? game->units.selected_id : -1;
  ctx.cursor_x = game->map_cursor_x;
  ctx.cursor_y = game->map_cursor_y;
  ctx.human_nation = game->human_nation;
  ctx.europe_ok = game->europe_ok;
  map_menu_refresh_orders(&game->map_menu, &ctx);
}

static char game_key_letter(ColonizeKey key) {
  switch (key) {
    case COLONIZE_KEY_A: return 'A';
    case COLONIZE_KEY_B: return 'B';
    case COLONIZE_KEY_C: return 'C';
    case COLONIZE_KEY_D: return 'D';
    case COLONIZE_KEY_E: return 'E';
    case COLONIZE_KEY_F: return 'F';
    case COLONIZE_KEY_G: return 'G';
    case COLONIZE_KEY_H: return 'H';
    case COLONIZE_KEY_I: return 'I';
    case COLONIZE_KEY_L: return 'L';
    case COLONIZE_KEY_N: return 'N';
    case COLONIZE_KEY_O: return 'O';
    case COLONIZE_KEY_P: return 'P';
    case COLONIZE_KEY_Q: return 'Q';
    case COLONIZE_KEY_R: return 'R';
    case COLONIZE_KEY_S: return 'S';
    case COLONIZE_KEY_T: return 'T';
    case COLONIZE_KEY_U: return 'U';
    case COLONIZE_KEY_V: return 'V';
    case COLONIZE_KEY_W: return 'W';
    case COLONIZE_KEY_X: return 'X';
    case COLONIZE_KEY_Z: return 'Z';
    default: return 0;
  }
}

static void set_status(ColonizeGameState* game, const char* prefix, const char* detail);
static bool game_do_found_colony_at_unit(ColonizeGameState* game, int uid);
static void game_request_noport_found_confirm(ColonizeGameState* game, int uid);
static bool game_try_found_colony_at_cursor(ColonizeGameState* game);
static void game_fill_turn_context(ColonizeGameState* game, ColonizeTurnContext* ctx);
static void game_apply_ai_popup_result(ColonizeGameState* game);
static void game_after_unit_action(ColonizeGameState* game);
static void activate_menu_selection(ColonizeGameState* game);
static void game_wait_next_unit(ColonizeGameState* game);
static void game_open_retire_score(ColonizeGameState* game);
static void game_open_trade_unload_picker(ColonizeGameState* game);
static int game_trade_route_aim_stop(ColonizeGameState* game, ColonizeUnit* u, int stop_i);
static void game_set_view_center(ColonizeGameState* game, int x, int y);
static void game_open_found_name_entry(ColonizeGameState* game, int colony_id);
static void game_open_landho_name_entry(ColonizeGameState* game);
static void game_landho_default_region(const ColonizeGameState* game, char* out, size_t out_size);
static void game_apply_name_entry_result(ColonizeGameState* game);
static void game_try_prompt_landho(ColonizeGameState* game);
static bool game_handle_modal_input(ColonizeGameState* game, const ColonizeInputState* input);
static void game_apply_howmuch_result(ColonizeGameState* game);
static void game_apply_save_load_result(ColonizeGameState* game);
static void game_apply_cheat_list_result(ColonizeGameState* game);
static void game_select_unit(ColonizeGameState* game, int unit_id);
static bool game_load_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size);
static bool game_save_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size);
static void game_do_buy_construction(ColonizeGameState* game, int colony_id);
static void game_request_buy_construction_confirm(ColonizeGameState* game);

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
    case COLONIZE_KEY_A: return "A";
    case COLONIZE_KEY_G: return "G";
    case COLONIZE_KEY_V: return "V";
    case COLONIZE_KEY_X: return "X";
    case COLONIZE_KEY_Z: return "Z";
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

/* Manual / fandom: Europe screen closes once independence is declared. */
static bool game_europe_blocked_by_woi(const ColonizeGameState* game) {
  return game && game->col1_ok && ai_king_independence_declared(&game->col1);
}

static bool game_try_enter_europe(ColonizeGameState* game) {
  if (!game || !game->europe_ok) {
    return false;
  }
  if (game_europe_blocked_by_woi(game)) {
    set_status(game, "Europe is closed during the War of Independence", NULL);
    return false;
  }
  game->in_europe = true;
  game->in_pedia = false;
  game->in_colony = false;
  game->in_report = false;
  snprintf(
    game->europe.status,
    sizeof(game->europe.status),
    "Home port ready. Recruit / Train / S Sail / Esc."
  );
  return true;
}

/* @WAREHOUSEFULL when ship→colony unload hits capacity. */
static void game_emit_warehouse_full(
  ColonizeGameState* game,
  int colony_id,
  int cargo_type
) {
  if (!game) {
    return;
  }
  const ColonizeColony* col = colonies_get(&game->colonies, colony_id);
  if (!col) {
    return;
  }
  const char* cargo_name = "cargo";
  if (cargo_type >= 0 && cargo_type < COLONIZE_CARGO_COUNT &&
      game->europe.cargo[cargo_type].name[0]) {
    cargo_name = game->europe.cargo[cargo_type].name;
  }
  colonies_emit_warehouse_full_chrome(
    &game->colonies, col, cargo_type, cargo_name, &game->ai_popups, &game->messages
  );
}

/*
 * Complete human FOUND at founder unit tile (after gates / @NOPORT confirm).
 * FUN_4cc6_07c2 Indian homeland purchase via colonies_found_with_indian_land —
 * same charge/Minuit-free gate as AI euro FOUND. Cite: Colonization.pdf Indian
 * Land / Peter Minuit (FF 2). smoke_game_flow has no tribe fixture; covered by
 * unit_founding_fathers + unit_ai_euro_expand indian-land cases.
 */
static bool game_do_found_colony_at_unit(ColonizeGameState* game, int uid) {
  if (!game || !game->world_map_ok || uid < 0) {
    return false;
  }
  const ColonizeUnit* founder = units_get_const(&game->units, uid);
  if (!founder || !founder->active || !units_is_on_map(founder)) {
    set_status(game, "No unit at cursor to found colony", NULL);
    return false;
  }
  if (units_is_sea(&game->units, uid)) {
    set_status(game, "Ships cannot found colonies", NULL);
    return false;
  }
  const int cx = founder->x;
  const int cy = founder->y;
  if (!colonies_can_found(&game->colonies, &game->world_map, cx, cy)) {
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }

  const int type_index = founder->type_index;
  const int profession = founder->profession;
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(&game->units, uid, &tools, &muskets, &horses);

  const int hn = game->human_nation;
  ColonizeCol1Save* col1 = game->col1_ok ? &game->col1 : NULL;
  uint32_t* gold = NULL;
  int land_cost = 0;
  if (col1 && hn >= 0 && hn < 4) {
    /* Live treasury is europe.gold; sync into col1 before land charge. */
    col1->nation[hn].gold = (uint32_t)(game->europe.gold < 0 ? 0 : game->europe.gold);
    gold = &col1->nation[hn].gold;
    land_cost = colonies_indian_land_purchase_gold(col1, &game->world_map, cx, cy, hn);
    if (land_cost > 0 && *gold < (uint32_t)land_cost) {
      char detail[48];
      snprintf(detail, sizeof(detail), "need %d gold", land_cost);
      set_status(game, "Indian land", detail);
      return false;
    }
  }

  const int cid = colonies_found_with_indian_land(
    &game->colonies,
    &game->world_map,
    col1,
    gold,
    cx,
    cy,
    hn,
    type_index,
    profession,
    tools,
    muskets,
    horses
  );
  if (cid < 0) {
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }

  units_despawn(&game->units, uid);
  if (gold) {
    game->europe.gold = (int)*gold;
  }
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  if (land_cost > 0) {
    snprintf(
      game->status,
      sizeof(game->status),
      "Founded %s (paid %d gold)",
      col ? col->name : "colony",
      land_cost
    );
  } else {
    snprintf(
      game->status,
      sizeof(game->status),
      "Founded %s (pop %d)",
      col ? col->name : "colony",
      col ? col->population : 0
    );
  }
  game_open_found_name_entry(game, cid);
  return true;
}

/* @NOPORT CHOICE when founding inland (no ocean access). */
static void game_request_noport_found_confirm(ColonizeGameState* game, int uid) {
  if (!game || uid < 0) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages,
    "NOPORT",
    &tok,
    "This square does not have access to the ocean.",
    body,
    sizeof(body)
  );
  char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const ColonizeMsgSection* sec = assets_msg_find(&game->messages, "NOPORT");
  int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
  const char* labels[2];
  const int ids[] = {0, 1}; /* forgot / proceed */
  if (nch >= 2) {
    labels[0] = choice_buf[0];
    labels[1] = choice_buf[1];
  } else {
    labels[0] = "Oh, I forgot about that.";
    labels[1] = "And that is exactly what I had in mind.";
  }
  game->map_confirm = GAME_MAP_CONFIRM_FOUND_INLAND;
  game->map_confirm_payload = uid;
  if (!ai_popup_enqueue_choice(
        &game->ai_popups, AI_POPUP_TAG_MAP_CONFIRM, NULL, body, labels, ids, 2
      )) {
    game->map_confirm = GAME_MAP_CONFIRM_NONE;
    game->map_confirm_payload = -1;
    set_status(game, "Dialog queue full", NULL);
  }
}

/*
 * Human FOUND (B key / Orders → Build Colony).
 * @SEACOLONY on water; @TOOMOUNTAIN on mountains; @NOPORT CHOICE when land is not coastal.
 */
static bool game_try_found_colony_at_cursor(ColonizeGameState* game) {
  if (!game || !game->world_map_ok) {
    return false;
  }
  const int cx = game->map_cursor_x;
  const int cy = game->map_cursor_y;
  if (!map_tile_is_land(&game->world_map, cx, cy)) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages,
      "SEACOLONY",
      NULL,
      "Colonies cannot be built at sea.",
      body,
      sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }
  for (int i = 0; i < game->colonies.colony_count; ++i) {
    const ColonizeColony* col = &game->colonies.colonies[i];
    if (col->active && abs(col->x - cx) <= 1 && abs(col->y - cy) <= 1) {
      char body[AI_POPUP_BODY_LEN];
      PopupMsgTokens tok = {0};
      tok.string0 = col->name[0] ? col->name : "nearby colony";
      popup_msg_fill(
        &game->messages,
        "TOONEAR",
        &tok,
        "This land is too near to another colony for a new colony, Your Excellency.",
        body,
        sizeof(body)
      );
      ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      set_status(game, "Cannot found colony here", NULL);
      return false;
    }
  }
  if (map_pedia_terrain_index_at(&game->world_map, cx, cy) == 27) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages,
      "TOOMOUNTAIN",
      NULL,
      "Colonies cannot be built in the mountains.",
      body,
      sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }
  if (!colonies_can_found(&game->colonies, &game->world_map, cx, cy)) {
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }
  const int uid = units_id_at(&game->units, cx, cy);
  if (uid < 0) {
    set_status(game, "No unit at cursor to found colony", NULL);
    return false;
  }
  if (units_is_sea(&game->units, uid)) {
    set_status(game, "Ships cannot found colonies", NULL);
    return false;
  }
  if (!map_tile_is_coastal(&game->world_map, cx, cy)) {
    game_request_noport_found_confirm(game, uid);
    return true;
  }
  return game_do_found_colony_at_unit(game, uid);
}

static void game_enqueue_yes_no(
  ColonizeGameState* game,
  GameMapConfirm confirm,
  int payload,
  const char* section,
  const char* fallback_body,
  const PopupMsgTokens* tok
) {
  if (!game) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, section, tok, fallback_body, body, sizeof(body));
  const char* labels[] = {"Yes", "No"};
  const int ids[] = {1, 0};
  game->map_confirm = confirm;
  game->map_confirm_payload = payload;
  if (!ai_popup_enqueue_choice(
        &game->ai_popups,
        AI_POPUP_TAG_MAP_CONFIRM,
        NULL,
        body,
        labels,
        ids,
        2
      )) {
    game->map_confirm = GAME_MAP_CONFIRM_NONE;
    set_status(game, "Dialog queue full", NULL);
  }
}

static void game_do_disband(ColonizeGameState* game, int uid) {
  if (!game) {
    return;
  }
  if (uid < 0 || !units_disband(&game->units, uid)) {
    set_status(game, "Cannot disband", NULL);
  } else {
    set_status(game, "Unit disbanded", NULL);
    game_wait_next_unit(game);
  }
}

static void game_do_overboard(ColonizeGameState* game, int uid) {
  if (!game) {
    return;
  }
  int ctype = 0;
  int amt = 0;
  if (uid < 0 || units_dump_cargo_overboard(&game->units, uid, &ctype, &amt) <= 0) {
    set_status(game, "No cargo to dump", NULL);
  } else {
    snprintf(game->status, sizeof(game->status), "Dumped %d overboard", amt);
  }
}

static void game_do_trade_delete_slot(ColonizeGameState* game, int slot) {
  if (!game || !game->col1_ok || slot < 0 ||
      slot >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  char gone[32];
  snprintf(gone, sizeof(gone), "%s", game->col1.trade_route[slot].name);
  memset(&game->col1.trade_route[slot], 0, sizeof(game->col1.trade_route[slot]));
  uint16_t hi = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT; ++i) {
    if (game->col1.trade_route[i].name[0] != '\0' ||
        game->col1.trade_route[i].dest_count > 0) {
      hi = (uint16_t)(i + 1);
    }
  }
  game->col1.head.trade_route_count = hi;
  snprintf(game->status, sizeof(game->status), "Deleted %s", gone[0] ? gone : "route");
}

static void game_trade_begin_route(ColonizeGameState* game, int route) {
  if (!game || !game->units_ok) {
    return;
  }
  const int sid = game->units.selected_id;
  if (sid < 0 || !units_order_trade_route(&game->units, sid)) {
    set_status(game, "Select a ship or wagon", NULL);
    return;
  }
  ColonizeUnit* u = units_get(&game->units, sid);
  if (!u || route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    set_status(game, "Invalid trade route", NULL);
    return;
  }
  u->follow_unit_id = route;
  u->turns_worked = 0;
  if (game_trade_route_aim_stop(game, u, 0)) {
    snprintf(
      game->status,
      sizeof(game->status),
      "Trade route: %s (stop 1/%d)",
      game->col1.trade_route[route].name,
      (int)game->col1.trade_route[route].dest_count
    );
  } else {
    set_status(game, "Trade route begun (could not aim first stop)", NULL);
  }
  game_wait_next_unit(game);
}

static void game_trade_edit_append_stop(ColonizeGameState* game, int route) {
  if (!game || !game->col1_ok || route < 0 ||
      route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
  if (r->dest_count >= 4) {
    snprintf(game->status, sizeof(game->status), "%s full (4 stops)", r->name);
    return;
  }
  const int cid = colonies_id_at(&game->colonies, game->map_cursor_x, game->map_cursor_y);
  uint16_t stop_idx = 999;
  const char* stop_label = "Europe";
  if (cid >= 0) {
    const ColonizeColony* c = colonies_get(&game->colonies, cid);
    if (!c || !c->active || c->nation_id != game->human_nation) {
      set_status(game, "Edit: cursor must be own colony (or Europe for sea)", NULL);
      return;
    }
    stop_idx = (uint16_t)cid;
    stop_label = c->name[0] ? c->name : "colony";
  } else if (!r->sea) {
    set_status(game, "Edit: put cursor on own colony to add stop", NULL);
    return;
  }
  if (r->dest_count > 0 && r->stop[r->dest_count - 1].colony_index == stop_idx) {
    snprintf(
      game->status,
      sizeof(game->status),
      "%s already ends at %s (%d/4)",
      r->name,
      stop_label,
      (int)r->dest_count
    );
    return;
  }
  ColonizeCol1TradeStop* st = &r->stop[r->dest_count];
  memset(st, 0, sizeof(*st));
  st->colony_index = stop_idx;
  {
    const ColonizeColony* fill_c =
      (stop_idx == 999) ? NULL : colonies_get(&game->colonies, (int)stop_idx);
    colonies_trade_stop_autofill(st, fill_c, &game->units, game->units.selected_id);
  }
  r->dest_count++;
  game->trade_edit_route = route;
  game->trade_edit_stop = (int)r->dest_count - 1;
  game->trade_edit_need_load = (stop_idx != 999);
  snprintf(
    game->status,
    sizeof(game->status),
    "%s +%s (%d/4) — pick unload/load",
    r->name,
    stop_label,
    (int)r->dest_count
  );
  game_open_trade_unload_picker(game);
}

static void game_apply_map_confirm(ColonizeGameState* game) {
  if (!game || game->ai_popups.result_tag != AI_POPUP_TAG_MAP_CONFIRM) {
    return;
  }
  const GameMapConfirm conf = game->map_confirm;
  const int payload = game->map_confirm_payload;
  const bool yes = !game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1;
  game->map_confirm = GAME_MAP_CONFIRM_NONE;
  game->map_confirm_payload = -1;
  ai_popup_consume_result(&game->ai_popups);
  if (!yes) {
    set_status(game, "Cancelled", NULL);
    return;
  }
  switch (conf) {
    case GAME_MAP_CONFIRM_DISBAND:
      game_do_disband(game, payload);
      break;
    case GAME_MAP_CONFIRM_OVERBOARD:
      game_do_overboard(game, payload);
      break;
    case GAME_MAP_CONFIRM_QUIT:
      game->elapsed_ms = UINT32_MAX;
      break;
    case GAME_MAP_CONFIRM_TITLE_EXIT:
      game->elapsed_ms = UINT32_MAX;
      break;
    case GAME_MAP_CONFIRM_RETIRE: {
      ColonizeInputState empty;
      memset(&empty, 0, sizeof(empty));
      map_menu_handle_input(&game->map_menu, &empty, NULL, true);
      game_open_retire_score(game);
      break;
    }
    case GAME_MAP_CONFIRM_TRADE_DELETE:
      game_do_trade_delete_slot(game, payload);
      break;
    case GAME_MAP_CONFIRM_BUY_CONSTRUCTION:
      game_do_buy_construction(game, payload);
      break;
    case GAME_MAP_CONFIRM_FOUND_INLAND:
      (void)game_do_found_colony_at_unit(game, payload);
      break;
    default:
      break;
  }
}

static void game_request_disband_confirm(ColonizeGameState* game) {
  if (!game || !game->units_ok) {
    return;
  }
  const int uid = game->units.selected_id;
  if (uid < 0) {
    set_status(game, "Cannot disband", NULL);
    return;
  }
  const ColonizeUnit* u = units_get_const(&game->units, uid);
  const ColonizeUnitType* ut = u ? units_type(&game->units, u->type_index) : NULL;
  const char* uname = (ut && ut->name[0]) ? ut->name : "unit";

  /* @DISBANDSHIP is an error OK (no Yes/No) when a ship still carries units. */
  if (units_is_sea(&game->units, uid)) {
    int has_pax = 0;
    if (u && u->cargo_count > 0) {
      has_pax = 1;
    } else {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* p = &game->units.units[i];
        if (p->active && p->aboard_ship_id == uid) {
          has_pax = 1;
          break;
        }
      }
    }
    if (has_pax) {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        &game->messages,
        "DISBANDSHIP",
        NULL,
        "We cannot disband a ship at sea while it is carrying units.",
        body,
        sizeof(body)
      );
      ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      set_status(game, "Cannot disband ship with units aboard", NULL);
      return;
    }
  }

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = uname;
  game_enqueue_yes_no(
    game,
    GAME_MAP_CONFIRM_DISBAND,
    uid,
    "SUREDISBAND",
    "Really disband this unit?",
    &tok
  );
}

static void game_do_buy_construction(ColonizeGameState* game, int colony_id) {
  if (!game || !game->in_colony) {
    return;
  }
  ColonizeColony* colony = colonies_get_mut(&game->colonies, colony_id);
  ColonyScreenView* csv = &game->colony_screen;
  if (!colony || colony->building_in_production < 0) {
    set_status(game, "No project", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const int gold_before = game->europe.gold;
  const ColonizeBuildingType* bt =
    colonies_building_type(&game->colonies, colony->building_in_production);
  const int tools = bt ? bt->tools_cost : 0;
  if (colonies_construction_tools_needed(&game->colonies, colony) > 0) {
    set_status(game, "Need tools", NULL);
  } else if (game->europe.gold < colonies_construction_gold_cost(&game->colonies, colony)) {
    set_status(game, "Need gold", NULL);
  } else if (colonies_buy_construction(&game->colonies, colony_id, &game->europe.gold)) {
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
}

static void game_request_buy_construction_confirm(ColonizeGameState* game) {
  if (!game || !game->in_colony) {
    return;
  }
  ColonizeColony* colony = colonies_get_mut(&game->colonies, game->colony_view_id);
  ColonyScreenView* csv = &game->colony_screen;
  if (!colony || colony->building_in_production < 0) {
    set_status(game, "No project", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  if (colonies_construction_tools_needed(&game->colonies, colony) > 0) {
    set_status(game, "Need tools", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const int gold_cost = colonies_construction_gold_cost(&game->colonies, colony);
  if (game->europe.gold < gold_cost) {
    set_status(game, "Need gold", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const ColonizeBuildingType* bt =
    colonies_building_type(&game->colonies, colony->building_in_production);
  const char* bname = (bt && bt->name[0]) ? bt->name : "building";
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = bname;
  tok.number0 = gold_cost;
  tok.has_number0 = true;
  tok.number1 = game->europe.gold;
  tok.has_number1 = true;
  char body[AI_POPUP_BODY_LEN];
  char fallback[160];
  snprintf(
    fallback,
    sizeof(fallback),
    "Cost to complete %s: %d$. Treasury: %d$.",
    bname,
    gold_cost,
    game->europe.gold
  );
  popup_msg_fill(&game->messages, "BUYME1", &tok, fallback, body, sizeof(body));
  char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const ColonizeMsgSection* sec = assets_msg_find(&game->messages, "BUYME1");
  int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
  const char* labels[2];
  const int ids[] = {0, 1}; /* Never mind / Complete it */
  if (nch >= 2) {
    labels[0] = choice_buf[0];
    labels[1] = choice_buf[1];
  } else {
    labels[0] = "Never mind";
    labels[1] = "Complete it";
  }
  game->map_confirm = GAME_MAP_CONFIRM_BUY_CONSTRUCTION;
  game->map_confirm_payload = game->colony_view_id;
  if (!ai_popup_enqueue_choice(
        &game->ai_popups, AI_POPUP_TAG_MAP_CONFIRM, NULL, body, labels, ids, 2
      )) {
    game->map_confirm = GAME_MAP_CONFIRM_NONE;
    set_status(game, "Dialog queue full", NULL);
    colony_screen_set_status(csv, game->status);
  }
}

static void game_request_overboard_confirm(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  const int sid = game->units.selected_id;
  if (sid < 0 || !units_is_transport(&game->units, sid)) {
    set_status(game, "No cargo to dump", NULL);
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  game_enqueue_yes_no(
    game,
    GAME_MAP_CONFIRM_OVERBOARD,
    sid,
    "OVERBOARD",
    "Throw cargo overboard?",
    &tok
  );
}

static void game_open_find_colony_picker(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  const char* labels[CHEAT_LIST_MAX_OPTIONS];
  int ids[CHEAT_LIST_MAX_OPTIONS];
  char name_bufs[CHEAT_LIST_MAX_OPTIONS][CHEAT_LIST_LABEL_LEN];
  int count = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX && count < CHEAT_LIST_MAX_OPTIONS; ++i) {
    const ColonizeColony* c = &game->colonies.colonies[i];
    if (!c->active || c->nation_id != game->human_nation) {
      continue;
    }
    str_copy_trunc(
      name_bufs[count],
      sizeof(name_bufs[count]),
      c->name[0] ? c->name : "Colony"
    );
    labels[count] = name_bufs[count];
    ids[count] = c->id;
    count++;
  }
  if (count <= 0) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages, "NOCITY", NULL, "No colonies founded yet.", body, sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    return;
  }
  char prompt[COLONIZE_MSG_LINE_LEN];
  popup_msg_fill(
    &game->messages, "FINDCITY", NULL, "Where the heck is . . .", prompt, sizeof(prompt)
  );
  if (!cheat_list_open_find_colony(&game->cheat_list, prompt, labels, ids, count)) {
    set_status(game, "Find Colony unavailable", NULL);
  }
}

static void game_open_trade_route_picker(ColonizeGameState* game, int mode) {
  if (!game || !game->col1_ok) {
    set_status(game, "No trade routes", NULL);
    return;
  }
  const char* labels[CHEAT_LIST_MAX_OPTIONS];
  int ids[CHEAT_LIST_MAX_OPTIONS];
  char name_bufs[CHEAT_LIST_MAX_OPTIONS][CHEAT_LIST_LABEL_LEN];
  int count = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT && count < CHEAT_LIST_MAX_OPTIONS;
       ++i) {
    const ColonizeCol1TradeRoute* r = &game->col1.trade_route[i];
    if (r->name[0] == '\0' && r->dest_count == 0) {
      continue;
    }
    str_copy_trunc(
      name_bufs[count],
      sizeof(name_bufs[count]),
      r->name[0] ? r->name : "Route"
    );
    labels[count] = name_bufs[count];
    ids[count] = i;
    count++;
  }
  if (count <= 0) {
    set_status(game, "No trade routes", NULL);
    return;
  }
  game->trade_select_mode = mode;
  const char* sec = (mode == 3) ? "TRADEDELETE" : "TRADESELECT";
  char prompt[COLONIZE_MSG_LINE_LEN];
  popup_msg_fill(
    &game->messages,
    sec,
    NULL,
    mode == 3 ? "Which trade route should we delete:" : "Select a trade route:",
    prompt,
    sizeof(prompt)
  );
  if (!cheat_list_open_trade_select(&game->cheat_list, prompt, labels, ids, count)) {
    game->trade_select_mode = 0;
    set_status(game, "Trade select unavailable", NULL);
  }
}

static void game_open_found_name_entry(ColonizeGameState* game, int colony_id) {
  if (!game) {
    return;
  }
  const ColonizeColony* col = colonies_get(&game->colonies, colony_id);
  char prompt[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages,
    "COLONY",
    NULL,
    "What shall we name this colony?",
    prompt,
    sizeof(prompt)
  );
  const char* seed = col && col->name[0] ? col->name : "Colony";
  if (!name_entry_open(
        &game->name_entry, NAME_ENTRY_KIND_FOUND, prompt, seed, colony_id
      )) {
    set_status(game, "Name entry failed", NULL);
  }
}

static void game_landho_default_region(const ColonizeGameState* game, char* out, size_t out_size) {
  static const char* k_regions[4] = {
    "New England", "New France", "New Spain", "New Netherlands"
  };
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!game) {
    str_copy_trunc(out, out_size, "New England");
    return;
  }
  /* Europe screen already loaded NAMES.TXT @COLONYNAME for the human nation. */
  if (game->europe.colony_region[0]) {
    str_copy_trunc(out, out_size, game->europe.colony_region);
    return;
  }
  const int nation = game->human_nation;
  if (game->names_ok && nation >= 0 && nation <= 3) {
    const ColonizeMsgSection* reg = assets_msg_find(&game->names, "COLONYNAME");
    if (reg && nation < reg->line_count) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", reg->lines[nation]);
      /* Trim trailing CR/spaces lightly. */
      size_t n = strlen(line);
      while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n' || line[n - 1] == ' ')) {
        line[--n] = '\0';
      }
      if (line[0] && line[0] != ';') {
        str_copy_trunc(out, out_size, line);
        return;
      }
    }
  }
  if (nation >= 0 && nation <= 3) {
    str_copy_trunc(out, out_size, k_regions[nation]);
  } else {
    str_copy_trunc(out, out_size, k_regions[0]);
  }
}

static void game_open_landho_name_entry(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  char prompt[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages,
    "LANDHO",
    NULL,
    "Land Ho! What shall we call this new land, Your Excellency?",
    prompt,
    sizeof(prompt)
  );
  char seed[48];
  game_landho_default_region(game, seed, sizeof(seed));
  if (!name_entry_open(
        &game->name_entry, NAME_ENTRY_KIND_LANDHO, prompt, seed, -1
      )) {
    /* Fallback: mark discovery so DOS woodcut one-shot does not re-fire. */
    if (game->col1_ok) {
      col1_bridge_mark_new_world_discovered(&game->col1, game->human_nation);
    }
    str_copy_trunc(game->europe.colony_region, sizeof(game->europe.colony_region), seed);
    set_status(game, "Name entry failed", NULL);
  }
}

static void game_apply_name_entry_result(ColonizeGameState* game) {
  if (!game || !game->name_entry.has_result) {
    return;
  }
  const NameEntryKind kind = game->name_entry.result_kind;
  if (kind == NAME_ENTRY_KIND_LANDHO) {
    char fallback[48];
    game_landho_default_region(game, fallback, sizeof(fallback));
    const char* name =
      game->name_entry.result_name[0] ? game->name_entry.result_name : fallback;
    str_copy_trunc(game->europe.colony_region, sizeof(game->europe.colony_region), name);
    if (game->col1_ok) {
      col1_bridge_mark_new_world_discovered(&game->col1, game->human_nation);
    }
    snprintf(game->status, sizeof(game->status), "New land: %s", name);
  } else if (!game->name_entry.result_cancelled) {
    ColonizeColony* col =
      colonies_get_mut(&game->colonies, game->name_entry.result_colony_id);
    if (col) {
      str_copy_trunc(col->name, sizeof(col->name), game->name_entry.result_name);
      snprintf(game->status, sizeof(game->status), "Colony: %s", col->name);
      if (game->in_colony) {
        colony_screen_set_status(&game->colony_screen, game->status);
      }
    }
  }
  game->name_entry.has_result = false;
}

static void game_try_prompt_landho(ColonizeGameState* game) {
  if (!game || !game->col1_ok || !game->world_map_ok) {
    return;
  }
  if (game->name_entry.open || game->in_menu) {
    return;
  }
  if (game->human_nation < 0 || game->human_nation > 3) {
    return;
  }
  if (game->col1.player[game->human_nation].named_new_world) {
    return;
  }
  if (!col1_bridge_human_has_seen_land(&game->world_map, game->human_nation)) {
    return;
  }
  game_open_landho_name_entry(game);
}

static void game_apply_options_result(ColonizeGameState* game) {
  if (!game || !game->options_dlg.has_result) {
    return;
  }
  if (!game->options_dlg.result_cancelled) {
    if (game->options_dlg.result_kind == OPTIONS_KIND_GAME && game->col1_ok) {
      options_dialog_apply_game(&game->options_dlg, &game->col1.head.game_options);
      set_status(game, "Game options updated", NULL);
    } else if (game->options_dlg.result_kind == OPTIONS_KIND_COLONY && game->col1_ok) {
      options_dialog_apply_colony(
        &game->options_dlg, &game->col1.head.colony_report_options
      );
      set_status(game, "Colony report options updated", NULL);
    } else if (game->options_dlg.result_kind == OPTIONS_KIND_SOUND) {
      bool bg = true, ev = true, sfx = true;
      if (options_dialog_apply_sound(&game->options_dlg, &bg, &ev, &sfx)) {
        ColonizeSoundOptions so = sound_get_options();
        so.background_music = bg;
        so.event_music = ev;
        so.sound_effects = sfx;
        sound_set_options(so);
        if (game->col1_ok) {
          game->col1.head.tut2.background_music = bg ? 1 : 0;
          game->col1.head.tut2.event_music = ev ? 1 : 0;
          game->col1.head.tut2.sound_effects = sfx ? 1 : 0;
        }
        set_status(game, "Sound options updated", NULL);
      }
    }
  }
  game->options_dlg.has_result = false;
}

/*
 * Parent-view hotkeys must not fire while any wood modal is open (name entry,
 * howmuch, options, ai_popup, lists, stack picker).
 */
static bool game_handle_modal_input(ColonizeGameState* game, const ColonizeInputState* input) {
  if (!game || !input) {
    return false;
  }
  if (game->pick_music.open) {
    const ColonizeFont* pm_font = game->colony_font_ok ? &game->colony_font :
                                  (game->menu_font_ok ? &game->menu_font : NULL);
    pick_music_handle_input(
      &game->pick_music, &game->messages, input, pm_font, game->status, sizeof(game->status)
    );
    return true;
  }
  if (game->save_load.open) {
    save_load_handle_input(&game->save_load, input);
    game_apply_save_load_result(game);
    return true;
  }
  if (game->options_dlg.open) {
    options_dialog_handle_input(&game->options_dlg, input);
    game_apply_options_result(game);
    return true;
  }
  if (game->combat_analysis.open) {
    combat_analysis_handle_input(&game->combat_analysis, input);
    return true;
  }
  if (game->name_entry.open) {
    name_entry_handle_input(&game->name_entry, input);
    game_apply_name_entry_result(game);
    return true;
  }
  if (game->howmuch.open) {
    howmuch_handle_input(&game->howmuch, input);
    if (game->howmuch.has_result) {
      if (!game->howmuch.result_cancelled && game->howmuch.result_amount > 0) {
        game_apply_howmuch_result(game);
      }
      game->howmuch.has_result = false;
    }
    return true;
  }
  if (game->cheat_list.open) {
    cheat_list_handle_input(&game->cheat_list, input);
    game_apply_cheat_list_result(game);
    return true;
  }
  if (game->ai_popups.open) {
    ai_popup_handle_input(&game->ai_popups, input);
    game_apply_ai_popup_result(game);
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
  return false;
}

static void game_apply_howmuch_result(ColonizeGameState* game) {
  if (!game || game->howmuch.result_cancelled || game->howmuch.result_amount <= 0) {
    return;
  }
  const int amt = game->howmuch.result_amount;
  const int cargo = game->howmuch.result_cargo;
  const HowmuchKind kind = game->howmuch.result_kind;
  if (kind == HOWMUCH_KIND_LOAD || kind == HOWMUCH_KIND_MOVE) {
    ColonyScreenView* csv = &game->colony_screen;
    if (!game->in_colony || csv->transport_unit_id < 0) {
      return;
    }
    const int moved = colonies_transfer_to_unit(
      &game->colonies,
      game->colony_view_id,
      &game->units,
      csv->transport_unit_id,
      cargo,
      amt
    );
    if (moved > 0) {
      snprintf(game->status, sizeof(game->status), "Loaded %d", moved);
    } else {
      set_status(game, "No empty hold", NULL);
    }
    colony_screen_set_status(csv, game->status);
  } else if (kind == HOWMUCH_KIND_UNLOAD) {
    /* Port transfer_from_unit unloads a whole hold; amount prompt clamps via prior pick. */
    ColonyScreenView* csv = &game->colony_screen;
    if (!game->in_colony || csv->transport_unit_id < 0) {
      return;
    }
    bool full = false;
    const int hold = game->howmuch.result_payload;
    int peek_type = -1;
    {
      const ColonizeUnit* tu = units_get_const(&game->units, csv->transport_unit_id);
      if (tu && hold >= 0 && hold < COLONIZE_UNIT_CARGO_MAX) {
        peek_type = tu->hold_goods_type[hold];
      }
    }
    const int moved = colonies_transfer_from_unit(
      &game->colonies,
      game->colony_view_id,
      &game->units,
      csv->transport_unit_id,
      hold,
      &full
    );
    (void)amt;
    if (moved > 0 && full) {
      snprintf(game->status, sizeof(game->status), "Unloaded %d (Warehouse full)", moved);
      game_emit_warehouse_full(game, game->colony_view_id, peek_type);
    } else if (moved > 0) {
      snprintf(game->status, sizeof(game->status), "Unloaded %d", moved);
    } else if (full) {
      set_status(game, "Warehouse full", NULL);
      game_emit_warehouse_full(game, game->colony_view_id, peek_type);
    } else {
      set_status(game, "Cannot unload", NULL);
    }
    colony_screen_set_status(csv, game->status);
  } else if (kind == HOWMUCH_KIND_BUY) {
    EuropeScreen* eu = &game->europe;
    if (!game->in_europe || eu->selected_harbor < 0) {
      return;
    }
    europe_buy_cargo(eu, eu->selected_harbor, cargo, amt);
  } else if (kind == HOWMUCH_KIND_SELL) {
    EuropeScreen* eu = &game->europe;
    if (!game->in_europe || eu->selected_harbor < 0) {
      return;
    }
    const int hold = game->howmuch.result_payload;
    if (hold < 0) {
      return;
    }
    EuropeHarborShip* ship = &eu->harbor[eu->selected_harbor];
    int left = amt;
    while (left > 0 && ship->hold_goods_amount[hold] > 0) {
      const int ctype = ship->hold_goods_type[hold];
      const int take = ship->hold_goods_amount[hold] < left ? ship->hold_goods_amount[hold] : left;
      const int gained = europe_sell_proceeds(eu, ctype, take);
      eu->gold += gained;
      ship->hold_goods_amount[hold] = (uint8_t)(ship->hold_goods_amount[hold] - take);
      if (ship->hold_goods_amount[hold] == 0) {
        ship->hold_goods_type[hold] = 255;
      }
      left -= take;
      snprintf(eu->status, sizeof(eu->status), "Sold %d for %d$.", amt - left, gained);
    }
  } else if (kind == HOWMUCH_KIND_SOUND_TEST) {
    sound_play(amt);
    char line[32];
    snprintf(line, sizeof(line), "Playing sound #%d", amt);
    set_status(game, line, NULL);
  }
}

static void game_apply_ai_popup_result(ColonizeGameState* game) {
  if (!game || !game->ai_popups.has_result) {
    return;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_MAP_CONFIRM) {
    game_apply_map_confirm(game);
    return;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_LANDFALL) {
    if (!game->ai_popups.result_cancelled) {
      const int ship_id = game->ai_popups.result_nation_a;
      const int dest_x = game->ai_popups.result_nation_b;
      const int dest_y = game->ai_popups.result_payload;
      const int choice = game->ai_popups.result_choice_id;
      ColonizeUnit* ship = units_get(&game->units, ship_id);
      /* choice 0 = Stay With Ships; 1 = Make Landfall (one passenger ashore). */
      if (ship && units_is_sea(&game->units, ship_id) && choice == 1) {
        /* DOS 4720: prefer cargo with moves; sentry cargo still eligible. */
        const int pax_id = units_first_landfall_cargo(&game->units, ship_id);
        if (pax_id >= 0 &&
            units_unload_passenger(
              &game->units, ship_id, pax_id, &game->world_map, dest_x, dest_y, &game->colonies
            )) {
          /* Ship spends the coastal order (1 MP); passenger charged in unload. */
          if (ship->moves_left > 0) {
            ship->moves_left--;
          }
          game->units.selected_id = pax_id;
          snprintf(game->status, sizeof(game->status), "Landfall at (%d,%d)", dest_x, dest_y);
          game_after_unit_action(game);
        } else {
          set_status(game, "Landfall failed", NULL);
        }
      } else if (choice == 0) {
        set_status(game, "Staying with ships", NULL);
      }
    }
    ai_popup_consume_result(&game->ai_popups);
    return;
  }
  /*
   * FUN_4d56_4528 village raid warn: Leave aborts; Attack opens hostilities then
   * commits the deferred move (combat if Brave on tile; empty → fallout).
   */
  if (game->ai_popups.result_tag == AI_POPUP_TAG_CONTACT_VILLAGE_WARN) {
    const int unit_id = game->ai_popups.result_nation_a;
    const int indian_nation = game->ai_popups.result_nation_b;
    const int dest_x = game->ai_popups.result_payload & 0xff;
    const int dest_y = (game->ai_popups.result_payload >> 8) & 0xff;
    const int choice = game->ai_popups.result_choice_id;
    if (!game->ai_popups.result_cancelled && choice == 1 /* Attack */) {
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      ColonizeUnit* u = units_get(&game->units, unit_id);
      const int euro = u ? u->nation_id : game->human_nation;
      ai_contact_village_open_hostilities(&ctx, indian_nation, euro);
      units_set_ff_col1(game->col1_ok ? &game->col1 : NULL);
      units_set_combat_human_nation(game->human_nation);
      units_set_combat_popups(&game->ai_popups, &game->messages);
      units_set_occupancy_map(&game->world_map);
      units_set_combat_colonies(&game->colonies);
      units_set_native_fallout_context(
        game->col1_ok ? &game->col1 : NULL, &game->world_map, -1
      );
      /*
       * Empty village: FUN_5fef_1b0e temp Brave; try_move fights then stays
       * adjacent (no enter). Population-- / destroy via finish helper.
       */
      game->units.selected_id = unit_id;
      if (units_try_move(
            &game->units, unit_id, &game->world_map, dest_x, dest_y, &game->colonies, &game->move_rng
          )) {
        if (units_last_combat_outcome() > 0) {
          snprintf(game->status, sizeof(game->status), "Village attacked (%d,%d)", dest_x, dest_y);
        } else {
          snprintf(game->status, sizeof(game->status), "Village contact (%d,%d)", dest_x, dest_y);
        }
        game_after_unit_action(game);
      } else if (units_last_combat_outcome() < 0) {
        set_status(game, "Combat lost", NULL);
        game_after_unit_action(game);
      } else {
        set_status(game, "Attack failed", NULL);
      }
    } else {
      set_status(game, "Left the village alone", NULL);
    }
    ai_popup_consume_result(&game->ai_popups);
    return;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_COMBAT_RANSOM) {
    if (game->col1_ok) {
      (void)units_combat_apply_ransom_popup(&game->col1, &game->ai_popups);
    }
    ai_popup_consume_result(&game->ai_popups);
    return;
  }
  ColonizeTurnContext ctx;
  game_fill_turn_context(game, &ctx);
  ai_king_apply_popup_result(&ctx, &game->ai_popups);
  if (game->ai_popups.result_tag == AI_POPUP_TAG_KING_SCORED &&
      !game->ai_popups.result_cancelled &&
      game->ai_popups.result_choice_id == 0) {
    /* @SCORED "That's all." → open retire score (same path as Retire menu). */
    game_open_retire_score(game);
  }
  ai_contact_apply_popup_result(&ctx, &game->ai_popups);
  ai_diplo_apply_popup_result(&ctx, &game->ai_popups);
  founding_fathers_apply_popup_result(&ctx, &game->ai_popups);
  ai_popup_consume_result(&game->ai_popups);
}

/* Fog nation for map paint: special view override, else human. */
static int game_fog_nation(const ColonizeGameState* game) {
  if (!game) {
    return 0;
  }
  if (game->fog_view == -1) {
    return -1; /* Complete Map */
  }
  if (game->fog_view >= 0 && game->fog_view <= 3) {
    return game->fog_view;
  }
  return game->human_nation; /* NORMAL (-2) or invalid */
}

static void game_apply_setview(ColonizeGameState* game, int view_id, const char* label) {
  if (!game) {
    return;
  }
  game->fog_view = view_id;
  if (game->col1_ok) {
    game->col1.head.show_entire_map = (uint16_t)(view_id == -2 ? 0 : 1);
  }
  if (view_id == -2) {
    set_status(game, "No special view", NULL);
  } else if (label && label[0]) {
    set_status(game, "View", label);
  } else {
    set_status(game, "Viewpoint changed", NULL);
  }
}

static void game_apply_kill_indians(ColonizeGameState* game, int nation_id, const char* label) {
  if (!game || !game->col1_ok) {
    set_status(game, "No Indians", NULL);
    return;
  }
  /* Count units before wipe so empty-tribe status is accurate. */
  int unit_n = 0;
  if (game->units_ok) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &game->units.units[i];
      if (u->active && u->nation_id == nation_id) {
        unit_n++;
      }
    }
  }
  const int removed = col1_kill_indian_nation(
    &game->col1,
    game->units_ok ? &game->units : NULL,
    game->world_map_ok ? &game->world_map : NULL,
    nation_id
  );
  if (removed <= 0 && unit_n <= 0) {
    set_status(game, "No Indians of that tribe", label);
  } else if (label && label[0]) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Killed %s", label);
    set_status(game, buf, NULL);
  } else {
    set_status(game, "Indians killed", NULL);
  }
}

static void game_open_cheat_setview(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  if (!cheat_list_open_setview(
        &game->cheat_list, game->debug_txt_ok ? &game->debug_txt : NULL
      )) {
    set_status(game, "Reveal Map unavailable", NULL);
  }
}

static void game_open_cheat_kill_indians(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  if (!cheat_list_open_kill_indians(
        &game->cheat_list, game->names_ok ? &game->names : NULL
      )) {
    set_status(game, "Kill Indians unavailable", NULL);
  }
}

/* NAMES.TXT @UNIT type names for the CHEAT Create Unit stages. */
static const char* const k_cheat_create_main_labels[] = {
  "Colonists",   "Pioneers", "Militia",     "Missionary", "Scout",
  "Artillery",   "Wagon Train", "Treasure", "Ship",       "Indian Braves",
  "Indian Armed Braves", "Indian Horse", "Indian Armed Horse", "Foreign Unit"
};
/* units.h ColonizeUnitType.name for each main-stage entry (Ship/Foreign Unit
 * resolved by a follow-up stage instead). */
static const char* const k_cheat_create_main_types[] = {
  "Colonists", "Pioneers", "Soldiers", "Missionaries", "Scouts",
  "Artillery", "Wagon Train", NULL /* Treasure */, NULL /* Ship */,
  "Braves", "Armed Braves", "Mtd. Braves", "Mtd. Warriors", NULL /* Foreign */
};
static const char* const k_cheat_create_ship_labels[] = {
  "Caravel", "Merchantman", "Galleon", "Privateer", "Frigate", "Man-O-War"
};
static const char* const k_cheat_create_nation_labels[] = {
  "English", "French", "Spanish", "Dutch"
};
static const char* const k_cheat_create_foreign_labels[] = {"Rebel", "Loyal"};

static void game_open_cheat_create_unit(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  static int ids[14];
  for (int i = 0; i < 14; ++i) {
    ids[i] = i;
  }
  game->cheat_create_stage = 0;
  if (!cheat_list_open_create_unit(
        &game->cheat_list, "Select Unit To Create", k_cheat_create_main_labels, ids, 14
      )) {
    set_status(game, "Create Unit unavailable", NULL);
  }
}

static void game_open_cheat_set_human(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  if (!cheat_list_open_set_human(&game->cheat_list, game->debug_txt_ok ? &game->debug_txt : NULL)) {
    set_status(game, "Set Human Player unavailable", NULL);
  }
}

static void game_open_cheat_debug_flags(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  /* Bits 1 (Indian AI movement) / 3 (Foreign AI planning modes) are backed by
   * real Col1 game-options bits; the rest are Linux-only, not round-tripped. */
  uint16_t mask = game->debug_flags_mask;
  if (game->col1_ok) {
    mask = (uint16_t)(mask & (uint16_t) ~((1u << 1) | (1u << 3)));
    if (game->col1.head.game_options.show_indian_moves) {
      mask |= (uint16_t)(1u << 1);
    }
    if (game->col1.head.game_options.show_foreign_moves) {
      mask |= (uint16_t)(1u << 3);
    }
  }
  if (!cheat_list_open_debug_flags(
        &game->cheat_list, game->debug_txt_ok ? &game->debug_txt : NULL, mask
      )) {
    set_status(game, "Debug Info Flags unavailable", NULL);
  }
}

/* Applies a confirmed CHEAT_LIST_KIND_CREATE_UNIT id for the current stage;
 * may re-open the dialog for a follow-up stage (Ship / Foreign Unit). */
static void game_apply_cheat_create_unit(ColonizeGameState* game, int id) {
  if (!game || !game->units_ok) {
    return;
  }
  const int x = game->map_cursor_x;
  const int y = game->map_cursor_y;

  if (game->cheat_create_stage == 1) {
    /* @CSHIP result: id = index into k_cheat_create_ship_labels. */
    game->cheat_create_stage = 0;
    if (id < 0 || id >= 6) {
      return;
    }
    const int type_idx = units_find_type(&game->units, k_cheat_create_ship_labels[id]);
    const int uid = type_idx >= 0
      ? units_spawn_allow_stack(&game->units, type_idx, x, y)
      : -1;
    if (uid < 0) {
      set_status(game, "Cannot create unit here", NULL);
      return;
    }
    units_set_nation(units_get(&game->units, uid), game->human_nation);
    set_status(game, "Created", k_cheat_create_ship_labels[id]);
    return;
  }
  if (game->cheat_create_stage == 2) {
    /* @FOREIGN result: id = nation 0..3. Ask Rebel/Loyal next. */
    game->cheat_create_pending_nation = id;
    game->cheat_create_stage = 3;
    static int ids2[2] = {0, 1};
    if (!cheat_list_open_create_unit(
          &game->cheat_list, "Select Type to Create", k_cheat_create_foreign_labels, ids2, 2
        )) {
      game->cheat_create_stage = 0;
      set_status(game, "Create Unit unavailable", NULL);
    }
    return;
  }
  if (game->cheat_create_stage == 3) {
    /* @FOREIGN2 result: 0 = Rebel (Cont. Army), 1 = Loyal (Regulars). */
    game->cheat_create_stage = 0;
    const int nation = game->cheat_create_pending_nation;
    game->cheat_create_pending_nation = -1;
    if (nation < 0 || nation > 3) {
      return;
    }
    const char* type_name = id == 0 ? "Cont. Army" : "Regulars";
    const int type_idx = units_find_type(&game->units, type_name);
    const int uid = type_idx >= 0
      ? units_spawn_allow_stack(&game->units, type_idx, x, y)
      : -1;
    if (uid < 0) {
      set_status(game, "Cannot create unit here", NULL);
      return;
    }
    units_set_nation(units_get(&game->units, uid), nation);
    set_status(game, "Created", type_name);
    return;
  }

  /* Stage 0: main @CREATE list. */
  if (id < 0 || id >= 14) {
    return;
  }
  if (id == 8) {
    /* Ship: follow up with @CSHIP. */
    game->cheat_create_stage = 1;
    static int ids3[6] = {0, 1, 2, 3, 4, 5};
    if (!cheat_list_open_create_unit(
          &game->cheat_list, "Select Ship To Create", k_cheat_create_ship_labels, ids3, 6
        )) {
      game->cheat_create_stage = 0;
      set_status(game, "Create Unit unavailable", NULL);
    }
    return;
  }
  if (id == 13) {
    /* Foreign Unit: follow up with @FOREIGN. */
    game->cheat_create_stage = 2;
    static int ids4[4] = {0, 1, 2, 3};
    if (!cheat_list_open_create_unit(
          &game->cheat_list, "Select Nationality To Create", k_cheat_create_nation_labels, ids4, 4
        )) {
      game->cheat_create_stage = 0;
      set_status(game, "Create Unit unavailable", NULL);
    }
    return;
  }
  if (id == 7) {
    /* Treasure: no @HOWMUCH gold prompt in @CREATE — debug default amount. */
    const int uid =
      units_spawn_treasure_train(&game->units, x, y, game->human_nation, 1000);
    if (uid < 0) {
      set_status(game, "Cannot create unit here", NULL);
      return;
    }
    set_status(game, "Created", "Treasure (1000)");
    return;
  }
  const char* type_name = k_cheat_create_main_types[id];
  if (!type_name) {
    return;
  }
  /* Indian unit types spawn for the first native tribe (id 4); @CREATE has
   * no tribe picker in DEBUG.TXT. Cite: docs/save_format_map.md nation ids. */
  const int nation = id >= 9 ? 4 : game->human_nation;
  const int type_idx = units_find_type(&game->units, type_name);
  const int uid =
    type_idx >= 0 ? units_spawn_allow_stack(&game->units, type_idx, x, y) : -1;
  if (uid < 0) {
    set_status(game, "Cannot create unit here", NULL);
    return;
  }
  units_set_nation(units_get(&game->units, uid), nation);
  set_status(game, "Created", k_cheat_create_main_labels[id]);
}

static void game_apply_set_human(ColonizeGameState* game, int nation_id) {
  if (!game) {
    return;
  }
  if (nation_id < 0 || nation_id > 3) {
    /* "None" (spectator, no human nation) touches every game->human_nation
     * indexing site unguarded elsewhere — PARK rather than risk corruption. */
    set_status(game, "Spectator mode not supported", NULL);
    return;
  }
  game->human_nation = nation_id;
  game->active_turn_nation = nation_id;
  units_set_combat_human_nation(game->human_nation);
  static const char* const k_names[4] = {"English", "French", "Spanish", "Dutch"};
  set_status(game, "Now playing", k_names[nation_id]);
}

static void game_apply_debug_flags(ColonizeGameState* game, uint16_t mask) {
  if (!game) {
    return;
  }
  game->debug_flags_mask = mask;
  if (game->col1_ok) {
    game->col1.head.game_options.show_indian_moves = (mask & (1u << 1)) ? 1 : 0;
    game->col1.head.game_options.show_foreign_moves = (mask & (1u << 3)) ? 1 : 0;
  }
  set_status(game, "Debug options set", NULL);
}

/*
 * CHEAT Advance Revolution Status: DEBUG.TXT @FORCED has no options, just a
 * notice. Wires col1.game_options.independence_force (0x20 — "bypass
 * REF/event gates"), the real Col1 latch matching the DOS text exactly.
 */
static void game_cheat_advance_revolution(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  if (!game->col1_ok) {
    set_status(game, "Advance Revolution Status unavailable", NULL);
    return;
  }
  game->col1.head.game_options.independence_force = 1;
  set_status(game, "Independence forced at end of next turn.", NULL);
}

static void game_cheat_toggle_strategy(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game->debug_show_strategy = !game->debug_show_strategy;
  set_status(game, "Show Strategy", game->debug_show_strategy ? "on" : "off");
}

static void game_cheat_toggle_colony_sites(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game->debug_show_colony_sites = !game->debug_show_colony_sites;
  set_status(game, "Show Colony Sites", game->debug_show_colony_sites ? "on" : "off");
}

/* DEBUG.TXT @TEST: "Number of Units = %NUMBER0; Number of Colonies = %NUMBER1". */
static void game_cheat_test_routine(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  int units_n = 0;
  if (game->units_ok) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      if (game->units.units[i].active) {
        units_n++;
      }
    }
  }
  int colonies_n = 0;
  if (game->colonies_ok) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      if (game->colonies.colonies[i].active) {
        colonies_n++;
      }
    }
  }
  char line[80];
  snprintf(
    line, sizeof(line), "Number of Units = %d; Number of Colonies = %d", units_n, colonies_n
  );
  set_status(game, line, NULL);
}

/*
 * DEBUG.TXT @MEMORY reports DOS heap pools (Memory/Menu/Near/Stack Available,
 * PSP segment) that have no Linux equivalent under malloc. Closest real
 * analogue: actual process RSS (getrusage) plus the port's own fixed-size
 * unit/colony pool headroom, which is where a Linux "out of memory" cheat
 * check would actually matter.
 */
static void game_cheat_memory_check(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  struct rusage ru;
  long rss_kb = 0;
  if (getrusage(RUSAGE_SELF, &ru) == 0) {
    rss_kb = ru.ru_maxrss; /* Linux: KB already */
  }
  const int units_n = game->units_ok ? game->units.unit_count : 0;
  const int colonies_n = game->colonies_ok ? game->colonies.colony_count : 0;
  char line[96];
  snprintf(
    line,
    sizeof(line),
    "RSS = %ld KB; Units = %d/%d; Colonies = %d/%d",
    rss_kb,
    units_n,
    COLONIZE_UNITS_MAX,
    colonies_n,
    COLONIZE_COLONIES_MAX
  );
  set_status(game, line, NULL);
}

static void game_open_cheat_sound_test(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  howmuch_open(&game->howmuch, HOWMUCH_KIND_SOUND_TEST, "Play what sound #?", 255, 0, 0, 0);
}

static uint16_t game_trade_stop_mask_from_nibbles(const uint8_t nibbles[3], int count) {
  uint16_t m = 0;
  for (int i = 0; i < count && i < 6; ++i) {
    const int ct = col1_trade_nibble_cargo(nibbles, i);
    if (ct >= 0 && ct < 16) {
      m = (uint16_t)(m | (uint16_t)(1u << ct));
    }
  }
  return m;
}

static void game_trade_mask_to_types(uint16_t mask, int* out, int* out_n) {
  int n = 0;
  if (!out || !out_n) {
    return;
  }
  for (int c = 0; c < 16 && n < 6; ++c) {
    if ((mask & (uint16_t)(1u << c)) != 0) {
      out[n++] = c;
    }
  }
  *out_n = n;
}

static void game_open_trade_unload_picker(ColonizeGameState* game) {
  if (!game || game->trade_edit_route < 0 || !game->col1_ok) {
    return;
  }
  ColonizeCol1TradeRoute* r = &game->col1.trade_route[game->trade_edit_route];
  if (game->trade_edit_stop < 0 || game->trade_edit_stop >= (int)r->dest_count) {
    return;
  }
  const ColonizeCol1TradeStop* st = &r->stop[game->trade_edit_stop];
  const uint16_t mask =
    game_trade_stop_mask_from_nibbles(st->unload_cargo_nibbles, (int)st->unload_count);
  if (!cheat_list_open_trade_cargos(&game->cheat_list, CHEAT_LIST_KIND_TRADE_UNLOAD, mask)) {
    set_status(game, "Trade cargo picker unavailable", NULL);
    game->trade_edit_route = -1;
  }
}

static void game_open_trade_load_picker(ColonizeGameState* game) {
  if (!game || game->trade_edit_route < 0 || !game->col1_ok) {
    return;
  }
  ColonizeCol1TradeRoute* r = &game->col1.trade_route[game->trade_edit_route];
  if (game->trade_edit_stop < 0 || game->trade_edit_stop >= (int)r->dest_count) {
    return;
  }
  const ColonizeCol1TradeStop* st = &r->stop[game->trade_edit_stop];
  const uint16_t mask =
    game_trade_stop_mask_from_nibbles(st->load_cargo_nibbles, (int)st->load_count);
  if (!cheat_list_open_trade_cargos(&game->cheat_list, CHEAT_LIST_KIND_TRADE_LOAD, mask)) {
    set_status(game, "Trade cargo picker unavailable", NULL);
    game->trade_edit_route = -1;
  }
}

static void game_apply_cheat_list_result(ColonizeGameState* game) {
  if (!game || !game->cheat_list.has_result) {
    return;
  }
  const CheatListKind kind = game->cheat_list.result_kind;
  const int id = game->cheat_list.result_id;
  const char* label = game->cheat_list.result_label;
  const uint16_t mask = game->cheat_list.result_mask;
  game->cheat_list.has_result = false;
  if (kind == CHEAT_LIST_KIND_SETVIEW) {
    game_apply_setview(game, id, label);
  } else if (kind == CHEAT_LIST_KIND_KILL_INDIANS) {
    game_apply_kill_indians(game, id, label);
  } else if (kind == CHEAT_LIST_KIND_CREATE_UNIT) {
    game_apply_cheat_create_unit(game, id);
  } else if (kind == CHEAT_LIST_KIND_SET_HUMAN) {
    game_apply_set_human(game, id);
  } else if (kind == CHEAT_LIST_KIND_DEBUG_FLAGS) {
    game_apply_debug_flags(game, mask);
  } else if (kind == CHEAT_LIST_KIND_TRADE_UNLOAD || kind == CHEAT_LIST_KIND_TRADE_LOAD) {
    if (game->trade_edit_route < 0 || !game->col1_ok) {
      return;
    }
    ColonizeCol1TradeRoute* r = &game->col1.trade_route[game->trade_edit_route];
    if (game->trade_edit_stop < 0 || game->trade_edit_stop >= (int)r->dest_count) {
      game->trade_edit_route = -1;
      return;
    }
    ColonizeCol1TradeStop* st = &r->stop[game->trade_edit_stop];
    int types[6];
    int n = 0;
    game_trade_mask_to_types(mask, types, &n);
    if (kind == CHEAT_LIST_KIND_TRADE_UNLOAD) {
      int load_types[6];
      int ln = 0;
      for (int i = 0; i < (int)st->load_count && i < 6; ++i) {
        load_types[ln++] = col1_trade_nibble_cargo(st->load_cargo_nibbles, i);
      }
      colonies_trade_stop_set_cargos(st, types, n, load_types, ln);
      if (game->trade_edit_need_load && st->colony_index != 999) {
        game_open_trade_load_picker(game);
      } else {
        snprintf(
          game->status,
          sizeof(game->status),
          "%s stop unload=%u load=%u",
          r->name,
          (unsigned)st->unload_count,
          (unsigned)st->load_count
        );
        game->trade_edit_route = -1;
      }
    } else {
      int unload_types[6];
      int un = 0;
      for (int i = 0; i < (int)st->unload_count && i < 6; ++i) {
        unload_types[un++] = col1_trade_nibble_cargo(st->unload_cargo_nibbles, i);
      }
      colonies_trade_stop_set_cargos(st, unload_types, un, types, n);
      snprintf(
        game->status,
        sizeof(game->status),
        "%s stop unload=%u load=%u",
        r->name,
        (unsigned)st->unload_count,
        (unsigned)st->load_count
      );
      game->trade_edit_route = -1;
    }
  } else if (kind == CHEAT_LIST_KIND_FIND_COLONY) {
    const ColonizeColony* target = colonies_get(&game->colonies, id);
    if (!target) {
      set_status(game, "Colony not found", NULL);
      return;
    }
    game->map_cursor_x = target->x;
    game->map_cursor_y = target->y;
    game_set_view_center(game, target->x, target->y);
    snprintf(game->status, sizeof(game->status), "Find Colony: %s", target->name);
    /* Enter colony screen when possible. */
    if (game->colony_screen_ok) {
      game->colony_view_id = target->id;
      game->in_colony = true;
      game->in_europe = false;
      game->in_pedia = false;
    }
  } else if (kind == CHEAT_LIST_KIND_TRADE_SELECT) {
    const int mode = game->trade_select_mode;
    game->trade_select_mode = 0;
    if (mode == 1) {
      game_trade_begin_route(game, id);
    } else if (mode == 2) {
      game_trade_edit_append_stop(game, id);
    } else if (mode == 3) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = label && label[0] ? label : "route";
      game_enqueue_yes_no(
        game,
        GAME_MAP_CONFIRM_TRADE_DELETE,
        id,
        "SUREDELETE",
        "Are you sure you want to delete this route?",
        &tok
      );
    }
  }
}

static void game_open_save_load(ColonizeGameState* game, SaveLoadMode mode) {
  if (!game) {
    return;
  }
  const char* dir = game->config.save_dir ? game->config.save_dir : savegame_default_dir();
  if (!save_load_open(&game->save_load, mode, dir)) {
    set_status(game, mode == SAVE_LOAD_MODE_SAVE ? "Save unavailable" : "Load unavailable", NULL);
  }
}

static void game_apply_save_load_result(ColonizeGameState* game) {
  if (!game || !game->save_load.has_result) {
    return;
  }
  const SaveLoadMode mode = game->save_load.result_mode;
  const int slot = game->save_load.result_slot;
  game->save_load.has_result = false;
  char err[256];
  if (mode == SAVE_LOAD_MODE_SAVE) {
    diag_info(
      "Save confirmed: slot=COLONY%02d save_dir=%s turn=%u",
      slot,
      game->config.save_dir ? game->config.save_dir : "(null)",
      game->turn_number
    );
    if (!game_save_col1_slot(game, slot, err, sizeof(err))) {
      set_status(game, "Save failed", err);
      diag_error("Save failed: %s", err);
      return;
    }
    snprintf(
      game->status,
      sizeof(game->status),
      "Saved COLONY%02d (turn %u, year %u)",
      slot,
      game->turn_number,
      game->game_year
    );
    diag_info("Save succeeded for COLONY%02d (turn %u)", slot, game->turn_number);
    return;
  }

  diag_info(
    "Load confirmed: slot=COLONY%02d save_dir=%s",
    slot,
    game->config.save_dir ? game->config.save_dir : "(null)"
  );
  if (!game_load_col1_slot(game, slot, err, sizeof(err))) {
    set_status(game, "Load failed", err);
    diag_error("Load failed: %s", err);
    return;
  }
  snprintf(
    game->status,
    sizeof(game->status),
    "Loaded COLONY%02d (turn %u, year %u)",
    slot,
    game->turn_number,
    game->game_year
  );
  diag_info(
    "Load succeeded: slot=COLONY%02d turn=%u year=%u units=%d colonies=%d",
    slot,
    game->turn_number,
    game->game_year,
    game->units.unit_count,
    game->colonies.colony_count
  );
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

/*
 * VIEW Zoom In/Out/Level N (FUN_2b5a_0f92: DS:0x184 clamped 0..3). DOS redraws
 * the viewport at 16>>zoom px/tile so 15<<zoom × 12<<zoom tiles fit the same
 * 240×192 area (FUN_6ba1_000c: view_w=0xf<<zoom, view_h=0xc<<zoom, tile_px=
 * 0x10>>zoom). The port composites the wider tile grid at native 16px/tile
 * into an offscreen buffer, then nearest-neighbor-decimates it into the fixed
 * on-screen viewport — same visual result, different graphics pipeline.
 */
#define MAP_ZOOM_MAX 3
#define MAP_ZOOM_NATIVE_TILE 16
#define MAP_ZOOM_MAX_VIEW_COLS (15 << MAP_ZOOM_MAX)
#define MAP_ZOOM_MAX_VIEW_ROWS (12 << MAP_ZOOM_MAX)

/*
 * VIEW ~Hidden Terrain (H): brief pause between each of DOS's three peel
 * passes before auto-advancing to the next. Equivalent-information UI
 * convenience, not a timed DOS value — see docs.
 */
#define HIDDEN_TERRAIN_STEP_MS 700u

static int game_map_zoom_clamp(int zoom) {
  if (zoom < 0) {
    return 0;
  }
  if (zoom > MAP_ZOOM_MAX) {
    return MAP_ZOOM_MAX;
  }
  return zoom;
}

/* Nominal viewport size in tiles at this zoom tier; DOS 0xf<<zoom / 0xc<<zoom. */
static void game_map_zoom_view_size(int zoom, int* out_cols, int* out_rows) {
  zoom = game_map_zoom_clamp(zoom);
  if (out_cols) {
    *out_cols = MAP_VIEW_TILE_COLS << zoom;
  }
  if (out_rows) {
    *out_rows = MAP_VIEW_TILE_ROWS << zoom;
  }
}

/* On-screen pixels per tile at this zoom tier; DOS 0x10>>zoom. */
static int game_map_zoom_tile_px(int zoom) {
  return MAP_ZOOM_NATIVE_TILE >> game_map_zoom_clamp(zoom);
}

static void game_map_zoom_set(ColonizeGameState* game, int zoom) {
  if (!game) {
    return;
  }
  game->map_zoom = game_map_zoom_clamp(zoom);
}

static const char* render_mode_name(const ColonizeGameState* game) {
  if (game->in_report) {
    return "report";
  }
  if (game->in_hall_of_fame) {
    return "hall-of-fame";
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
  units_set_occupancy_map(&game->world_map);
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
  /* Restore Complete Map cheat (DOS show_entire_map @ DS:0x53a2); nation view not saved. */
  game->fog_view = (game->col1.head.show_entire_map != 0) ? -1 : -2;
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
  /* FUN_75c2_0840 LOADSIZE: reject when a live map differs (mid-game Load). */
  if (game->world_map_ok) {
    if (!col1_save_validate_head(
          &loaded.head, game->world_map.width, game->world_map.height, err, err_size
        )) {
      col1_save_free(&loaded);
      return false;
    }
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
  game->report_exits_to_menu = false;
  game->congress_page2 = false;
  game->labor_detail_job = -1;
  game->economic_page = 0;
  game->colony_page = 0;
  game->in_pedia = false;
  game->in_europe = false;
  game->in_colony = false;
  game->in_debug_atlas = false;
  snprintf(game->status, sizeof(game->status), "%s", reports_title(id));
  diag_info("Opened report %s (%s)", reports_title(id), reports_background_name(id));
}

static void game_open_retire_score(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game_open_report(game, COLONIZE_REPORT_SCORE);
  game->report_exits_to_menu = true;
  set_status(game, "Retired — Colonization Score", "Enter/Esc returns to menu");
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
      unit_chrome_blit_unit_for_palette(
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
        false,
        (game->pedia_wood_ok && game->pedia_wood.has_palette) ? &game->pedia_wood.palette : NULL
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
    unit_chrome_blit_unit_for_palette(
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
      false,
      (game->europe_ok && game->europe.background.has_palette) ? &game->europe.background.palette : NULL
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
    /* Live boycott mirror: ai_king.c tea-party / ai_diplo.c embargo write
     * game->col1.nation[human].boycott_bitmap directly; europe.c has no
     * col1 pointer, so refresh the UI-side copy every render (screen is
     * always rendered at least once before the player can act on it). */
    if (game->col1_ok && game->human_nation >= 0 &&
        game->human_nation < (int)COLONIZE_COL1_NATION_COUNT) {
      eu_mut->boycott_bitmap = game->col1.nation[game->human_nation].boycott_bitmap;
    }
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
        unit_chrome_blit_unit_for_palette(
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
          false,
          (game->europe_ok && game->europe.background.has_palette) ? &game->europe.background.palette : NULL
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
    /* Boycotted cargo (ai_king.c tea-party / ai_diplo.c embargo): price text
     * in red — trading is blocked until the boycott lifts (europe_buy_cargo /
     * europe_sell_hold / europe_sell_unit_hold refuse it; see
     * europe_cargo_boycotted). Source: fandom Boycott (Col). */
    const bool boycotted = europe_cargo_boycotted(eu, i);
    snprintf(line, sizeof(line), "%d/%d", eu->cargo[i].bid, eu->cargo[i].ask);
    {
      const int tw = font_text_width(font, line);
      const int th = font ? (font->max_height > 0 ? (int)font->max_height : 6) : 7;
      const int tx = mx + (EUROPE_MARKET_CELL - tw) / 2;
      const int ty = EUROPE_MARKET_Y + EUROPE_MARKET_CELL - th - 1;
      font_draw_text(font, framebuffer, tx, ty, line, boycotted ? 12 : 0);
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

  if (game->howmuch.open || game->name_entry.open) {
    const ColonizeSpriteSheet* wood =
      (game->map_panel_ok && game->map_panel.wood_ok) ? &game->map_panel.wood_tile : NULL;
    ColonizePopupColors popup_cols;
    popup_colors_from_ui(&popup_cols);
    const ColonizeFont* popup_font =
      game->intro_font_ok ? &game->intro_font : font;
    if (game->howmuch.open) {
      howmuch_render(
        (HowmuchDialog*)&game->howmuch,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->name_entry.open) {
      name_entry_render(
        (NameEntryDialog*)&game->name_entry,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
  }

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
  if (game->howmuch.open || game->name_entry.open) {
    const ColonizeSpriteSheet* wood =
      (game->map_panel_ok && game->map_panel.wood_ok) ? &game->map_panel.wood_tile : NULL;
    ColonizePopupColors popup_cols;
    popup_colors_from_ui(&popup_cols);
    const ColonizeFont* popup_font =
      game->intro_font_ok ? &game->intro_font : font;
    if (game->howmuch.open) {
      howmuch_render(
        (HowmuchDialog*)&game->howmuch,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->name_entry.open) {
      name_entry_render(
        (NameEntryDialog*)&game->name_entry,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
  }
}

static void fill_fallback_palette(ColonizePalette* palette) {
  for (int i = 0; i < 256; ++i) {
    palette->rgb[i][0] = (uint8_t)i;
    palette->rgb[i][1] = (uint8_t)((i * 3) & 0xff);
    palette->rgb[i][2] = (uint8_t)(255 - i);
  }
}

static void game_hof_load(ColonizeGameState* game);
static void game_hof_save(const ColonizeGameState* game);

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
  game->cheat_create_pending_nation = -1;
  game->cheat_unlock_step = 0;
  game->fog_view = -2;
  col1_save_init(&game->col1);
  game->col1_ok = false;

  assets_msg_init(&game->messages);
  assets_msg_init(&game->map_menu_txt);
  assets_msg_init(&game->labels);
  assets_msg_init(&game->debug_txt);
  assets_msg_init(&game->pedia);
  assets_msg_init(&game->names);
  map_menu_init(&game->map_menu);
  pick_music_init(&game->pick_music);
  cheat_list_init(&game->cheat_list);
  howmuch_init(&game->howmuch);
  name_entry_init(&game->name_entry);
  options_dialog_init(&game->options_dlg);
  combat_analysis_close(&game->combat_analysis);
  game->platform = NULL;
  game_bind_combat_analysis(game);
  game->map_confirm = GAME_MAP_CONFIRM_NONE;
  game->map_confirm_payload = -1;
  game->trade_select_mode = 0;
  game->trade_edit_route = -1;
  game->trade_edit_stop = -1;
  game->trade_edit_need_load = false;
  ai_popup_init(&game->ai_popups);
  save_load_init(&game->save_load);
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
  game_hof_load(game);

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

  /* DEBUG.TXT: cheat dialog strings (@SETVIEW, etc.). Optional — fallbacks exist. */
  game->debug_txt_ok = false;
  char debug_txt_path[512];
  if (dos_compat_normalize_asset_path(
        game->resolved_data_dir, "DEBUG.TXT", debug_txt_path, sizeof(debug_txt_path)
      )) {
    if (assets_msg_load_file(&game->debug_txt, debug_txt_path)) {
      game->debug_txt_ok = true;
      diag_info("Loaded DEBUG.TXT");
    } else {
      diag_warn("Failed to parse DEBUG.TXT");
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

void game_set_platform(ColonizeGameState* game, ColonizePlatform* platform) {
  if (!game) {
    return;
  }
  game->platform = platform;
  units_set_combat_human_nation(game->human_nation);
}

void game_destroy(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  units_set_move_watch(NULL, NULL);
  combat_analysis_set_presenter(NULL, NULL);
  combat_analysis_close(&game->combat_analysis);
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
  assets_msg_free(&game->debug_txt);
  map_menu_free(&game->map_menu);
  map_panel_free(&game->map_panel);
  pick_music_close(&game->pick_music);
  cheat_list_close(&game->cheat_list);
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
  game->fog_view = -2;
  snprintf(game->leader_name, sizeof(game->leader_name), "%s", ng->leader_name);

  char err[256];
  map_free(&game->world_map);
  game->world_map_ok = false;
  units_set_occupancy_map(NULL);

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
    ng->gen_params.focus_nation = game->human_nation;
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
    /* FUN_684c LAB_684c_1b4c HS-rim landfall for human nation. */
    if (!map_gen_euro_landfall(&game->world_map, game->human_nation, &sx, &sy)) {
      if (!map_gen_pick_start(
            &game->world_map, game->human_nation, -1, -1, 0, &sx, &sy
          )) {
        sx = game->world_map.width / 2;
        sy = game->world_map.height / 2;
      }
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
    units_set_occupancy_map(&game->world_map);
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
      if (game->col1_ok) {
        /* Live play prompts @LANDHO; do not silent-mark on campaign start. */
        game_try_prompt_landho(game);
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
    game_enqueue_yes_no(
      game, GAME_MAP_CONFIRM_TITLE_EXIT, -1, "DOS", "Exit to DOS?", NULL
    );
    return;
  }

  if (strstr(choice, "LOAD") != NULL || strstr(choice, "Load") != NULL) {
    game_open_save_load(game, SAVE_LOAD_MODE_LOAD);
    return;
  }

  if (strstr(choice, "Hall of Fame") != NULL || strstr(choice, "HALL") != NULL) {
    game->in_menu = false;
    game->in_hall_of_fame = true;
    set_status(game, "Hall of Fame", NULL);
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
  if (game->world_map_ok) {
    map_clamp_coords_inset(&game->world_map, &x, &y);
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
  /* FF + native settlement fallout for human combat (same as turn_refresh). */
  units_set_ff_col1(game->col1_ok ? &game->col1 : NULL);
  units_set_combat_human_nation(game->human_nation);
  units_set_combat_popups(&game->ai_popups, &game->messages);
  units_set_occupancy_map(&game->world_map);
  units_set_native_fallout_context(
    game->col1_ok ? &game->col1 : NULL, &game->world_map, -1
  );
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
        set_status(game, units_enter_reason_status(units_last_enter_reason()), NULL);
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
      const ColonizeEnterReason landfall = units_enter_probe(
        &game->units, selected->type_index, &game->world_map, dest_x, dest_y, sid, colonies
      );
      /* Native village: FUN_4d56_4528 ship abort — never @LANDFALL. */
      if (landfall == COLONIZE_ENTER_VILLAGE_SHIP) {
        if (game->col1_ok) {
          ColonizeTurnContext ctx;
          game_fill_turn_context(game, &ctx);
          if (ai_contact_try_ship_village(&ctx, selected->nation_id, dest_x, dest_y)) {
            return true;
          }
        }
        set_status(game, units_enter_reason_status(landfall), NULL);
        return false;
      }
      if (landfall != COLONIZE_ENTER_LANDFALL) {
        set_status(game, units_enter_reason_status(landfall), NULL);
        return false;
      }
      const int pax_ready = units_first_landfall_cargo(&game->units, sid);
      if (pax_ready < 0) {
        set_status(game, "No unit ready to disembark", NULL);
        return false;
      }
      {
        /* GAME.TXT @LANDFALL: Stay With Ships / Make Landfall. */
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages,
          "LANDFALL",
          NULL,
          "Shall we make landfall, Your Excellency, and leave the ships behind?",
          body,
          sizeof(body)
        );
        char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
        const ColonizeMsgSection* sec = assets_msg_find(&game->messages, "LANDFALL");
        int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
        const char* labels[2];
        const int ids[] = {0, 1};
        if (nch >= 2) {
          labels[0] = choice_buf[0];
          labels[1] = choice_buf[1];
        } else {
          labels[0] = "Stay With Ships";
          labels[1] = "Make Landfall";
        }
        if (!ai_popup_enqueue_choice_ctx(
              &game->ai_popups,
              AI_POPUP_TAG_LANDFALL,
              sid,
              dest_x,
              dest_y,
              NULL,
              body,
              labels,
              ids,
              2
            )) {
          if (!units_unload_passenger(
                &game->units, sid, pax_ready, &game->world_map, dest_x, dest_y, colonies
              )) {
            set_status(game, "Move blocked", NULL);
            return false;
          }
          /* Ship spends the coastal order; passenger charged in unload. */
          if (selected->moves_left > 0) {
            selected->moves_left--;
          }
          game->units.selected_id = pax_ready;
          snprintf(game->status, sizeof(game->status), "Landfall at (%d,%d)", dest_x, dest_y);
          game_after_unit_action(game);
          return true;
        }
      }
      set_status(game, "Landfall…", NULL);
      return true;
    }
  }

  /*
   * FUN_4d56_4528: combatish land unit → village tile gets Attack/Leave warn
   * before enter (defers move). Non-combat → Meet from adjacent (no enter).
   * Cite: indian_settlement_4528.md; ai_contact_try_village_raid_warn.
   */
  if (game->col1_ok && !units_is_sea(&game->units, sid) && game->col1.tribe) {
    for (uint16_t ti = 0; ti < game->col1.head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &game->col1.tribe[ti];
      if ((int)t->x != dest_x || (int)t->y != dest_y) {
        continue;
      }
      if (t->nation_id < 4 || t->nation_id > 11) {
        continue;
      }
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      if (combat_unit_is_combat_role(&game->units, sid)) {
        if (ai_contact_try_village_raid_warn(
              &ctx, selected->nation_id, (int)t->nation_id, sid, dest_x, dest_y
            )) {
          set_status(game, "Village…", NULL);
          return true;
        }
      } else if (ai_contact_try_village_meet(
                   &ctx,
                   selected->nation_id,
                   (int)t->nation_id,
                   selected->profession == UNITS_JOB_MISSIONARY,
                   t->state.capital
                 )) {
        /* Peaceful Meet/Trade from adjacent — spend a step, stay put. */
        const int cost = units_move_cost(&game->units, sid, &game->world_map, dest_x, dest_y);
        selected->moves_left -= cost > 0 ? cost : 1;
        if (selected->moves_left < 0) {
          selected->moves_left = 0;
        }
        set_status(game, "Village…", NULL);
        game_after_unit_action(game);
        return true;
      }
      break;
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
        set_status(game, units_enter_reason_status(units_last_enter_reason()), NULL);
      }
      return false;
    }
    if (units_last_enter_reason() == COLONIZE_ENTER_BOARD) {
      set_status(game, "Boarded ship", NULL);
      game_after_unit_action(game);
      return true;
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
    if (game->col1_ok && u->nation_id == game->human_nation) {
      game_try_prompt_landho(game);
    }
  }
  /* LCR: Scout on rumour clears + rolls a manual outcome (Fountain of Youth,
   * Cibola, treasure, burial mounds, …); de Soto keeps outcomes positive. */
  if (game->world_map_ok && units_is_on_map(u) && map_tile_has_rumour(&game->world_map, u->x, u->y)) {
    (void)units_resolve_lcr_rumour(
      &game->units,
      u->id,
      &game->world_map,
      game->col1_ok ? &game->col1 : NULL,
      &game->move_rng,
      game->europe_ok ? &game->europe : NULL,
      game->human_nation
    );
  }
  /*
   * First contact / village friction: land units only. Natives do not hail
   * ships (DOS meet gates ocean tiles; FUN_5bfb_022e after landfall).
   */
  if (game->col1_ok && u->nation_id >= 0 && u->nation_id <= 3 && units_is_on_map(u) &&
      !units_is_sea(&game->units, u->id)) {
    char contact[80];
    contact[0] = '\0';
    int first_indian = -1;
    if (col1_contact_adjacent_tribe(
          &game->col1, u->x, u->y, u->nation_id, contact, sizeof(contact), &first_indian
        )) {
      if (first_indian >= 4) {
        ColonizeTurnContext ctx;
        game_fill_turn_context(game, &ctx);
        (void)ai_contact_try_first_welcome(&ctx, u->nation_id, first_indian);
      } else if (contact[0]) {
        snprintf(game->status, sizeof(game->status), "%s", contact);
      }
    }
    /*
     * Already-met village Meet is enqueued from adjacent step (no enter).
     * Exact-tile Meet kept only if a unit somehow stands on the dwelling.
     */
    if (game->col1.tribe) {
      for (uint16_t ti = 0; ti < game->col1.head.tribe_count; ++ti) {
        const ColonizeCol1Tribe* t = &game->col1.tribe[ti];
        if ((int)t->x != u->x || (int)t->y != u->y) {
          continue;
        }
        if (t->nation_id < 4 || t->nation_id > 11) {
          continue;
        }
        ColonizeTurnContext ctx;
        game_fill_turn_context(game, &ctx);
        if (ai_contact_try_village_meet(
              &ctx,
              u->nation_id,
              (int)t->nation_id,
              u->profession == UNITS_JOB_MISSIONARY,
              t->state.capital
            )) {
          break;
        }
      }
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

/*
 * ORDERS Join Colony: admit selected land unit on an owned colony tile into
 * the population; otherwise open the colony screen at the cursor (legacy).
 */
static void game_join_colony_order(ColonizeGameState* game) {
  if (!game || !game->units_ok || !game->colonies_ok) {
    set_status(game, "Cannot join colony", NULL);
    return;
  }
  const int sid = game->units.selected_id;
  const ColonizeUnit* u = units_get_const(&game->units, sid);
  if (u && u->active && units_is_on_map(u) && !units_is_sea(&game->units, sid)) {
    const int cid = colonies_id_at(&game->colonies, u->x, u->y);
    const ColonizeColony* col = colonies_get(&game->colonies, cid);
    if (col && col->nation_id == game->human_nation) {
      if (col->colonist_count >= COLONIZE_COLONY_POP_MAX) {
        set_status(game, "Colony full", NULL);
        colonies_emit_full_chrome(col, &game->ai_popups, &game->messages);
        return;
      }
      const int ci = colonies_admit_unit(&game->colonies, cid, &game->units, sid);
      if (ci >= 0) {
        game->units.selected_id = -1;
        snprintf(
          game->status,
          sizeof(game->status),
          "Joined %s",
          col->name[0] ? col->name : "colony"
        );
        game_wait_next_unit(game);
        return;
      }
      set_status(game, "Cannot join colony", NULL);
      return;
    }
  }
  game_enter_colony_at_cursor(game);
}

/* Next owned colony after cursor (wrap); used by Find Colony and Go to Port. */
static const ColonizeColony* game_next_owned_colony(ColonizeGameState* game) {
  if (!game || game->colonies.colony_count <= 0) {
    return NULL;
  }
  int best_id = -1;
  int best_x = 9999;
  int best_y = 9999;
  int next_id = -1;
  int next_x = 9999;
  int next_y = 9999;
  const int cx = game->map_cursor_x;
  const int cy = game->map_cursor_y;
  const int nation = game->human_nation;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &game->colonies.colonies[i];
    if (!c->active || c->nation_id != nation) {
      continue;
    }
    if (best_id < 0 || c->y < best_y || (c->y == best_y && c->x < best_x)) {
      best_id = c->id;
      best_x = c->x;
      best_y = c->y;
    }
    const bool after = (c->y > cy) || (c->y == cy && c->x > cx);
    if (after &&
        (next_id < 0 || c->y < next_y || (c->y == next_y && c->x < next_x))) {
      next_id = c->id;
      next_x = c->x;
      next_y = c->y;
    }
  }
  return colonies_get(&game->colonies, next_id >= 0 ? next_id : best_id);
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
  /* fandom Stockade/Colony: with Stockade/Fort/Fortress cannot voluntarily
   * drop below 3 (port previously kept ≥2 — docs/fandom_col1994.md Conflicts). */
  if (colonies_has_fortification(&game->colonies, colony) && colony->colonist_count <= 3) {
    colony_screen_close_eject(csv);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages,
      "KEEPSTOCKADE",
      NULL,
      "We cannot voluntarily reduce below three the population of a colony that has a stockade, fort, or fortress.",
      body,
      sizeof(body)
    );
    colony_screen_open_message_ok(csv, body);
    return true;
  }
  if (colony->colonist_count <= 1) {
    char body[AI_POPUP_BODY_LEN];
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = colony->name[0] ? colony->name : "this";
    const char* section = "ABANDON";
    if (game->col1_ok && game->col1.head.year >= 1600) {
      /* After 1600, last colony warn — @ABANDON2. */
      int human_cols = 0;
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &game->colonies.colonies[i];
        if (c->active && c->nation_id == game->human_nation) {
          human_cols++;
        }
      }
      if (human_cols <= 1) {
        section = "ABANDON2";
      }
    }
    popup_msg_fill(
      &game->messages,
      section,
      &tok,
      "Shall we indeed abandon our colony, Your Excellency?",
      body,
      sizeof(body)
    );
    char choices[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    const ColonizeMsgSection* sec = assets_msg_find(&game->messages, section);
    int nch = popup_msg_choices(sec, choices, AI_POPUP_CHOICE_MAX);
    colony_screen_open_abandon_confirm(
      csv,
      colonist_index,
      role,
      body,
      nch >= 1 ? choices[0] : "Yes",
      nch >= 2 ? choices[1] : "No"
    );
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
      const ColonizeColony* col = colonies_get(&game->colonies, game->colony_view_id);
      const ColonizeColonist* c =
        (col && ci >= 0 && ci < col->colonist_count) ? &col->colonists[ci] : NULL;
      const int school_tier =
        colonies_school_building_tier(&game->colonies, building_index);
      if (c && c->building_type != building_index &&
          colonies_building_worker_count(col, building_index) >= COLONIZE_BUILDING_MAX_WORKERS) {
        set_status(game, "Building is full", NULL);
        colonies_emit_more_than_three_chrome(col, &game->ai_popups, &game->messages);
      } else if (c && school_tier > 0 && !colonies_profession_may_teach(c->profession)) {
        set_status(game, "Need a skilled teacher", NULL);
        colonies_emit_noteacher_chrome(&game->ai_popups, &game->messages);
      } else if (
        c && school_tier > 0 &&
        colonies_school_tier_shortfall(c->profession, school_tier) != 0
      ) {
        const int need = colonies_school_tier_shortfall(c->profession, school_tier);
        set_status(
          game, need >= 3 ? "Need a university" : "Need a college", NULL
        );
        colonies_emit_need_school_chrome(
          c->profession, school_tier, &game->ai_popups, &game->messages
        );
      } else {
        set_status(game, "Cannot assign here", NULL);
      }
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
        int peek_type = -1;
        {
          const ColonizeUnit* tu = units_get_const(&game->units, csv->transport_unit_id);
          if (tu && drag->index >= 0 && drag->index < COLONIZE_UNIT_CARGO_MAX) {
            peek_type = tu->hold_goods_type[drag->index];
          }
        }
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
          game_emit_warehouse_full(game, game->colony_view_id, peek_type);
        } else if (moved > 0) {
          snprintf(game->status, sizeof(game->status), "Unloaded %d", moved);
        } else if (full) {
          set_status(game, "Warehouse full", NULL);
          game_emit_warehouse_full(game, game->colony_view_id, peek_type);
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

/* GAME.TXT @SHIPOPTIONS "Unload all cargo": drain every goods hold into the
 * viewed colony's warehouse (same path as the single-hold drag/drop). */
static void game_colony_unload_all_cargo(ColonizeGameState* game, int unit_id) {
  if (!game || !game->units_ok || game->colony_view_id < 0) {
    return;
  }
  const int holds = units_goods_hold_count(&game->units, unit_id);
  int total = 0;
  bool any_full = false;
  int last_full_type = -1;
  for (int i = 0; i < holds; ++i) {
    int peek_type = -1;
    const ColonizeUnit* tu = units_get_const(&game->units, unit_id);
    if (tu && i < COLONIZE_UNIT_CARGO_MAX) {
      peek_type = tu->hold_goods_type[i];
    }
    bool full = false;
    const int moved = colonies_transfer_from_unit(
      &game->colonies, game->colony_view_id, &game->units, unit_id, i, &full
    );
    total += moved;
    if (full) {
      any_full = true;
      last_full_type = peek_type;
    }
  }
  if (total > 0 && any_full) {
    snprintf(game->status, sizeof(game->status), "Unloaded %d (Warehouse full)", total);
    game_emit_warehouse_full(game, game->colony_view_id, last_full_type);
  } else if (total > 0) {
    snprintf(game->status, sizeof(game->status), "Unloaded %d", total);
  } else if (any_full) {
    set_status(game, "Warehouse full", NULL);
    game_emit_warehouse_full(game, game->colony_view_id, last_full_type);
  } else {
    set_status(game, "No cargo to unload", NULL);
  }
}

/*
 * Colony docked-unit orders popup apply (DOS FUN_2f2b_5746). "Move to front"
 * ports as (re)select the colony's active docked transport — the port has no
 * persistent dock queue for a literal front-of-line reorder.
 */
static void game_colony_apply_dock_order(
  ColonizeGameState* game,
  ColonyScreenView* csv,
  ColonyDockOrderAction action
) {
  if (!game || !csv || !game->units_ok) {
    return;
  }
  const int uid = csv->dock_orders_unit_id;
  if (!units_get(&game->units, uid)) {
    colony_screen_close_dock_orders(csv);
    return;
  }
  switch (action) {
  case COLONY_DOCK_ORDER_ACTIVATE:
    csv->transport_unit_id = uid;
    set_status(game, "Transport selected", NULL);
    break;
  case COLONY_DOCK_ORDER_CLEAR:
    units_wake(&game->units, uid);
    set_status(game, "Orders cleared", NULL);
    break;
  case COLONY_DOCK_ORDER_SENTRY:
    units_order_sentry(&game->units, uid);
    set_status(game, "Sentry", NULL);
    break;
  case COLONY_DOCK_ORDER_FORTIFY:
    if (units_is_sea(&game->units, uid)) {
      units_order_anchor(&game->units, uid, &game->colonies);
    } else {
      units_order_fortify(&game->units, uid);
    }
    set_status(game, "Fortify", NULL);
    break;
  case COLONY_DOCK_ORDER_UNLOAD_ALL:
    game_colony_unload_all_cargo(game, uid);
    break;
  case COLONY_DOCK_ORDER_CANCEL:
  default:
    break;
  }
  colony_screen_close_dock_orders(csv);
  colony_screen_set_status(csv, game->status);
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
  ctx->ai_popups = &game->ai_popups;
  ctx->messages = &game->messages;
  ctx->names = game->names_ok ? &game->names : NULL;
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
 * COL1 Treasure gold: cargo_hold[0..1] LE16. ColonizeUnit mirrors those bytes in
 * hold_goods_amount[0] (lo) + [1] (hi) — same field AI euro cash uses.
 * Cite: Colonization.pdf Treasure Trains; europe.h cargo_treasure_gold;
 * GAME.TXT @LOOTCASH. Non-Treasure passengers keep 0 (goods holds are not gold).
 *
 * Manual verify (smoke_game_flow stays title-only; no CMake smoke hook here):
 * board Treasure with LE16 in hold_goods_amount[0..1], H / Return to Europe on
 * high seas → Expected.cargo_treasure_gold set → tick to Harbor → cash-in.
 * unit_europe covers cash when cargo_treasure_gold is already set.
 */
static int game_treasure_gold_from_unit(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  if (!ut || !ut->name[0] || strstr(ut->name, "Treasure") == NULL) {
    return 0;
  }
  const unsigned lo = (unsigned)(u->hold_goods_amount[0] & 0xff);
  const unsigned hi = (unsigned)(u->hold_goods_amount[1] & 0xff);
  return (int)(lo | (hi << 8));
}

static void game_europe_capture_pax_treasure_gold(
  const ColonizeUnitPool* units,
  int ship_id,
  int* out_gold,
  int max
) {
  if (!out_gold || max <= 0) {
    return;
  }
  for (int i = 0; i < max; ++i) {
    out_gold[i] = 0;
  }
  const ColonizeUnit* ship = units_get_const(units, ship_id);
  if (!ship) {
    return;
  }
  for (int i = 0; i < ship->cargo_count && i < max; ++i) {
    const ColonizeUnit* pax = units_get_const(units, ship->cargo_ids[i]);
    out_gold[i] = game_treasure_gold_from_unit(units, pax);
  }
}

/* After europe_enqueue_expected: fill PARKED cargo_treasure_gold on newest slot. */
static void game_europe_fill_expected_treasure_gold(
  EuropeScreen* eu,
  const int* treasure_gold,
  int cargo_count
) {
  if (!eu || !treasure_gold || eu->expected_ships <= 0 || cargo_count <= 0) {
    return;
  }
  EuropeHarborShip* ship = &eu->expected[eu->expected_ships - 1];
  const int n = cargo_count > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : cargo_count;
  for (int i = 0; i < n; ++i) {
    ship->cargo_treasure_gold[i] = treasure_gold[i];
  }
}

/* Lane-full restore: put LE16 back onto respawned Treasure passengers. */
static void game_europe_restore_pax_treasure_gold(
  ColonizeUnitPool* units,
  int ship_id,
  const int* treasure_gold,
  int cargo_count
) {
  if (!units || !treasure_gold || cargo_count <= 0) {
    return;
  }
  ColonizeUnit* ship = units_get(units, ship_id);
  if (!ship) {
    return;
  }
  for (int i = 0; i < ship->cargo_count && i < cargo_count; ++i) {
    if (treasure_gold[i] <= 0) {
      continue;
    }
    ColonizeUnit* pax = units_get(units, ship->cargo_ids[i]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ut = units_type(units, pax->type_index);
    if (!ut || !ut->name[0] || strstr(ut->name, "Treasure") == NULL) {
      continue;
    }
    pax->hold_goods_amount[0] = treasure_gold[i] & 0xff;
    pax->hold_goods_amount[1] = (treasure_gold[i] >> 8) & 0xff;
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
  units_set_move_watch(NULL, NULL);
  game_apply_turn_autosave(game, result);
  game_europe_deliver_bound_ships(game);
  if (result && result->request_europe_open && game->europe_ok) {
    game->europe.open_on_dock = true;
  }
  if (game->europe_ok && game->europe.open_on_dock && !game_europe_blocked_by_woi(game)) {
    game->in_europe = true;
  }
  if (result && result->year_end_defeat) {
    snprintf(game->status, sizeof(game->status), "Defeat: no colonies remain.");
  } else if (result && result->year_end_victory) {
    snprintf(game->status, sizeof(game->status), "Victory: independence won.");
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
  units_set_move_watch(game_move_watch, game);
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

/* Persistent Hall of Fame: COLONIZE/HOF.TXT holds a ranked table of retired
 * Colonization Scores, one "score|leader|nation|year|difficulty" line per
 * entry, highest score first. Older single-integer-per-line files (the prior
 * thin stub) still load as a 1-entry table. */
static void game_hof_path(const ColonizeGameState* game, char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!game) {
    return;
  }
  if (!dos_compat_normalize_asset_path(game->resolved_data_dir, "HOF.TXT", out, out_size)) {
    snprintf(out, out_size, "%s/HOF.TXT", game->resolved_data_dir);
  }
}

/* Insert into game->hof_entries, keeping desc-by-score order, capped at
 * COLONIZE_HOF_MAX. */
static void game_hof_insert(ColonizeGameState* game, const ColonizeHofEntry* entry) {
  if (!game || !entry) {
    return;
  }
  int count = game->hof_count;
  if (count > COLONIZE_HOF_MAX) {
    count = COLONIZE_HOF_MAX;
  }
  int pos = count;
  if (count < COLONIZE_HOF_MAX) {
    ++count;
  } else if (count == 0 || entry->score <= game->hof_entries[count - 1].score) {
    return; /* table full and this score does not make the cut */
  } else {
    pos = count - 1;
  }
  for (int i = pos; i > 0 && game->hof_entries[i - 1].score < entry->score; --i) {
    game->hof_entries[i] = game->hof_entries[i - 1];
    pos = i - 1;
  }
  game->hof_entries[pos] = *entry;
  game->hof_count = count;
}

static void game_hof_load(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game->hof_count = 0;
  char path[640];
  game_hof_path(game, path, sizeof(path));
  FILE* f = fopen(path, "r");
  if (!f) {
    return;
  }
  char line[256];
  int lines_read = 0;
  /* Read (and re-rank via game_hof_insert) more lines than fit in the table
   * so a hand-edited or unsorted file still yields the true top scores. */
  while (lines_read < COLONIZE_HOF_MAX * 4 && fgets(line, sizeof(line), f)) {
    ++lines_read;
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0') {
      continue;
    }
    ColonizeHofEntry entry;
    memset(&entry, 0, sizeof(entry));
    char leader[NEW_GAME_LEADER_NAME_MAX];
    char nation[sizeof(entry.nation)];
    leader[0] = '\0';
    nation[0] = '\0';
    int score = 0;
    int year = 0;
    int difficulty = 0;
    int parsed = sscanf(
      line, "%d|%31[^|]|%23[^|]|%d|%d", &score, leader, nation, &year, &difficulty
    );
    if (parsed < 1 && sscanf(line, "%d", &score) != 1) {
      continue; /* malformed line */
    }
    if (score == 0) {
      continue;
    }
    entry.score = score;
    snprintf(entry.leader, sizeof(entry.leader), "%s", parsed >= 2 ? leader : "Governor");
    snprintf(entry.nation, sizeof(entry.nation), "%s", parsed >= 3 ? nation : "");
    entry.year = parsed >= 4 ? year : 0;
    entry.difficulty = parsed >= 5 ? difficulty : 0;
    game_hof_insert(game, &entry);
  }
  fclose(f);
}

static void game_hof_save(const ColonizeGameState* game) {
  if (!game || game->hof_count <= 0) {
    return;
  }
  char path[640];
  game_hof_path(game, path, sizeof(path));
  FILE* f = fopen(path, "w");
  if (!f) {
    return;
  }
  int count = game->hof_count > COLONIZE_HOF_MAX ? COLONIZE_HOF_MAX : game->hof_count;
  for (int i = 0; i < count; ++i) {
    const ColonizeHofEntry* e = &game->hof_entries[i];
    fprintf(
      f, "%d|%s|%s|%d|%d\n", e->score, e->leader, e->nation, e->year, e->difficulty
    );
  }
  fclose(f);
}

/*
 * Resolve Col1 trade-stop to map coords. colony_index 999 = Europe (eastern HS
 * for ships; coastal land for wagons). Returns 1 if *ox,*oy set.
 * Cite: ColonizeCol1TradeStop; Colonization.pdf Trade Routes.
 */
static int game_trade_stop_coords(
  ColonizeGameState* game,
  const ColonizeUnit* u,
  uint16_t colony_index,
  int* ox,
  int* oy
) {
  if (!game || !u || !ox || !oy) {
    return 0;
  }
  if (colony_index == 999) {
    if (units_is_sea(&game->units, u->id)) {
      int sx = 0;
      int sy = 0;
      if (units_find_eastern_high_seas_tile(&game->units, &game->world_map, u->y, &sx, &sy)) {
        *ox = sx;
        *oy = sy;
        return 1;
      }
    }
    /* Land: nearest own coastal colony as Europe sail stand-in. */
    int best = -1;
    int bx = 0;
    int by = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &game->colonies.colonies[i];
      if (!c->active || c->nation_id != game->human_nation) {
        continue;
      }
      if (!map_tile_is_coastal(&game->world_map, c->x, c->y)) {
        continue;
      }
      const int d = abs(c->x - u->x) + abs(c->y - u->y);
      if (best < 0 || d < best) {
        best = d;
        bx = c->x;
        by = c->y;
      }
    }
    if (best < 0) {
      return 0;
    }
    *ox = bx;
    *oy = by;
    return 1;
  }
  const ColonizeColony* c = colonies_get(&game->colonies, (int)colony_index);
  if (!c || !c->active) {
    return 0;
  }
  if (units_is_sea(&game->units, u->id)) {
    /* Ships aim coastal water next to colony. */
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    int best = -1;
    int bx = c->x;
    int by = c->y;
    for (int d = 0; d < 8; ++d) {
      const int nx = c->x + dx[d];
      const int ny = c->y + dy[d];
      if (!map_tile_is_water(&game->world_map, nx, ny)) {
        continue;
      }
      const int dist = abs(nx - u->x) + abs(ny - u->y);
      if (best < 0 || dist < best) {
        best = dist;
        bx = nx;
        by = ny;
      }
    }
    *ox = bx;
    *oy = by;
    return 1;
  }
  *ox = c->x;
  *oy = c->y;
  return 1;
}

/*
 * Aim TRADE_ROUTE unit at stop index. Linux stand-in: follow_unit_id = route
 * slot (0..11); turns_worked = stop index. Load/unload nibbles still thin.
 */
static int game_trade_route_aim_stop(ColonizeGameState* game, ColonizeUnit* u, int stop_i) {
  if (!game || !game->col1_ok || !u || u->orders != UNITS_ORDER_TRADE_ROUTE) {
    return 0;
  }
  const int route = u->follow_unit_id;
  if (route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return 0;
  }
  const ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
  if (r->dest_count <= 0) {
    return 0;
  }
  int si = stop_i % (int)r->dest_count;
  if (si < 0) {
    si = 0;
  }
  int tx = 0;
  int ty = 0;
  if (!game_trade_stop_coords(game, u, r->stop[si].colony_index, &tx, &ty)) {
    return 0;
  }
  u->goto_x = tx;
  u->goto_y = ty;
  u->turns_worked = si;
  return 1;
}

/*
 * Stop service (before advancing): Europe=999 ship sells holds; own colony
 * uses colonies_trade_route_service_stop (Col1 load/unload nibbles when set).
 * TRADE Edit nibble UI still thin. Cite: ColonizeCol1TradeStop;
 * Colonization.pdf Trade Routes.
 */
static void game_trade_route_service_stop(ColonizeGameState* game, ColonizeUnit* u) {
  if (!game || !u || !game->col1_ok) {
    return;
  }
  const int route = u->follow_unit_id;
  if (route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  const ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
  const int si = u->turns_worked;
  if (si < 0 || si >= (int)r->dest_count) {
    return;
  }
  const ColonizeCol1TradeStop* st = &r->stop[si];
  const uint16_t cidx = st->colony_index;
  if (cidx == 999) {
    if (units_is_sea(&game->units, u->id) && (u->x >= 200 || u->y >= 200 ||
                                                map_tile_is_high_seas(&game->world_map, u->x, u->y))) {
      /* Sell from front — europe_sell_unit_hold may compact holds. */
      for (int guard = 0; guard < 8; ++guard) {
        const int n = units_goods_hold_count(&game->units, u->id);
        int sold = 0;
        for (int h = 0; h < n; ++h) {
          if (u->hold_goods_amount[h] > 0 && u->hold_goods_amount[h] < 255) {
            if (europe_sell_unit_hold(&game->europe, &game->units, u->id, h) > 0) {
              sold = 1;
            }
            break;
          }
        }
        if (!sold) {
          break;
        }
      }
    }
    return;
  }
  int cid = (int)cidx;
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  if (!col || !col->active || col->nation_id != game->human_nation) {
    return;
  }
  /* Ship: allow adjacent coastal berth. */
  if (units_is_sea(&game->units, u->id)) {
    if (abs(u->x - col->x) > 1 || abs(u->y - col->y) > 1) {
      return;
    }
  } else if (u->x != col->x || u->y != col->y) {
    return;
  }
  (void)colonies_trade_route_service_stop(&game->colonies, cid, &game->units, u->id, st);
}

/* If TRADE_ROUTE unit is at current stop (or has no goto), service then advance. */
static void game_trade_route_retarget(ColonizeGameState* game, ColonizeUnit* u) {
  if (!game || !u || u->orders != UNITS_ORDER_TRADE_ROUTE) {
    return;
  }
  const int route = u->follow_unit_id;
  if (route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  const ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
  if (r->dest_count <= 0) {
    return;
  }
  const int at_dest =
    (u->goto_x >= UNITS_GOTO_NONE || u->goto_y >= UNITS_GOTO_NONE) ||
    (u->x == u->goto_x && u->y == u->goto_y);
  if (!at_dest) {
    return;
  }
  game_trade_route_service_stop(game, u);
  const int next = (u->turns_worked + 1) % (int)r->dest_count;
  (void)game_trade_route_aim_stop(game, u, next);
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
      game_open_save_load(game, SAVE_LOAD_MODE_SAVE);
      return true;
    }
    case MAP_MENU_ACTION_LOAD: {
      game_open_save_load(game, SAVE_LOAD_MODE_LOAD);
      return true;
    }
    case MAP_MENU_ACTION_DECLARE_INDEPENDENCE: {
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      ai_king_menu_declare_independence(&ctx);
      return true;
    }
    case MAP_MENU_ACTION_RETIRE: {
      ColonizeInputState empty;
      memset(&empty, 0, sizeof(empty));
      map_menu_handle_input(&game->map_menu, &empty, NULL, true);
      game_enqueue_yes_no(
        game,
        GAME_MAP_CONFIRM_RETIRE,
        -1,
        "RETIRE",
        "Do you really want to quit?",
        NULL
      );
      return true;
    }
    case MAP_MENU_ACTION_EXIT: {
      game_enqueue_yes_no(
        game, GAME_MAP_CONFIRM_QUIT, -1, "DOS", "Exit to DOS?", NULL
      );
      return true;
    }
    case MAP_MENU_ACTION_PICK_MUSIC:
      if (!pick_music_open(&game->pick_music, &game->messages)) {
        set_status(game, "Pick Music unavailable", "GAME.TXT @PICKMUSIC missing");
      } else {
        set_status(game, "Pick Music", "Esc closes");
      }
      return true;
    case MAP_MENU_ACTION_OPTIONS:
      if (!game->col1_ok) {
        set_status(game, "Options need a loaded game", NULL);
      } else if (!options_dialog_open_game(
                   &game->options_dlg, &game->messages, &game->col1.head.game_options
                 )) {
        set_status(game, "Options unavailable", NULL);
      }
      return true;
    case MAP_MENU_ACTION_COLONY_OPTIONS:
      if (!game->col1_ok) {
        set_status(game, "Options need a loaded game", NULL);
      } else if (!options_dialog_open_colony(
                   &game->options_dlg,
                   &game->messages,
                   &game->col1.head.colony_report_options
                 )) {
        set_status(game, "Colony options unavailable", NULL);
      }
      return true;
    case MAP_MENU_ACTION_SOUND_OPTIONS: {
      ColonizeSoundOptions so = sound_get_options();
      if (!options_dialog_open_sound(
            &game->options_dlg,
            &game->messages,
            so.background_music,
            so.event_music,
            so.sound_effects
          )) {
        set_status(game, "Sound options unavailable", NULL);
      }
      return true;
    }
    case MAP_MENU_ACTION_EUROPE:
      (void)game_try_enter_europe(game);
      return true;
    case MAP_MENU_ACTION_FIND_COLONY:
      game_open_find_colony_picker(game);
      return true;
    case MAP_MENU_ACTION_ZOOM_IN:
      /* In = toward 15×12 (level 0, most detail); Out = toward 120×96. */
      game_map_zoom_set(game, game->map_zoom - 1);
      return true;
    case MAP_MENU_ACTION_ZOOM_OUT:
      game_map_zoom_set(game, game->map_zoom + 1);
      return true;
    case MAP_MENU_ACTION_ZOOM_LEVEL_120X96:
      game_map_zoom_set(game, 3);
      return true;
    case MAP_MENU_ACTION_ZOOM_LEVEL_60X48:
      game_map_zoom_set(game, 2);
      return true;
    case MAP_MENU_ACTION_ZOOM_LEVEL_30X24:
      game_map_zoom_set(game, 1);
      return true;
    case MAP_MENU_ACTION_ZOOM_LEVEL_15X12:
      game_map_zoom_set(game, 0);
      return true;
    case MAP_MENU_ACTION_CENTER_VIEW:
      game_center_on_selected_unit(game);
      return true;
    case MAP_MENU_ACTION_VIEW_HIDDEN_TERRAIN:
      game->hidden_terrain_phase = 1;
      game->hidden_terrain_phase_ms = game->elapsed_ms;
      set_status(game, "Hidden Terrain: units and settlements hidden", NULL);
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
    case MAP_MENU_ACTION_ANCHOR: {
      const int uid = game->units.selected_id;
      if (uid < 0 || !units_order_anchor(&game->units, uid, &game->colonies)) {
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
      game_request_disband_confirm(game);
      return true;
    }
    case MAP_MENU_ACTION_BUILD_COLONY:
      (void)game_try_found_colony_at_cursor(game);
      return true;
    case MAP_MENU_ACTION_JOIN_COLONY:
      game_join_colony_order(game);
      return true;
    case MAP_MENU_ACTION_CLEAR_FOREST:
    case MAP_MENU_ACTION_PLOW_FIELDS: {
      const int sid = game->units.selected_id;
      char msg[96];
      msg[0] = '\0';
      if (!game->world_map_ok || !game->units_ok ||
          !units_pioneer_plow(
            &game->units,
            sid,
            &game->world_map,
            msg,
            sizeof(msg),
            &game->colonies,
            &game->ai_popups,
            &game->messages
          )) {
        set_status(game, msg[0] ? msg : "Cannot plow", NULL);
      } else {
        set_status(game, msg, NULL);
        game_wait_next_unit(game);
      }
      return true;
    }
    case MAP_MENU_ACTION_BUILD_ROAD: {
      const int sid = game->units.selected_id;
      char msg[96];
      msg[0] = '\0';
      if (!game->world_map_ok || !game->units_ok ||
          !units_pioneer_road(
            &game->units,
            sid,
            &game->world_map,
            msg,
            sizeof(msg),
            &game->colonies,
            &game->ai_popups,
            &game->messages
          )) {
        set_status(game, msg[0] ? msg : "Cannot build road", NULL);
      } else {
        set_status(game, msg, NULL);
        game_wait_next_unit(game);
      }
      return true;
    }
    case MAP_MENU_ACTION_PILLAGE: {
      const int sid = game->units.selected_id;
      char msg[96];
      msg[0] = '\0';
      if (!game->world_map_ok || !game->units_ok ||
          !units_pillage(
            &game->units, sid, &game->world_map, &game->colonies, msg, sizeof(msg)
          )) {
        set_status(game, msg[0] ? msg : "Cannot pillage", NULL);
      } else {
        set_status(game, msg, NULL);
        game_wait_next_unit(game);
      }
      return true;
    }
    case MAP_MENU_ACTION_GOTO_PORT: {
      const int sid = game->units.selected_id;
      const ColonizeUnit* u = units_get_const(&game->units, sid);
      const ColonizeColony* port = game_next_owned_colony(game);
      if (sid < 0 || !u || !u->active || !units_is_on_map(u)) {
        set_status(game, "Select a unit", NULL);
      } else if (!port) {
        set_status(game, "No colonies founded yet", NULL);
      } else if (!units_set_goto(
                   &game->units, sid, &game->world_map, port->x, port->y, &game->colonies
                 )) {
        set_status(game, "Cannot go to port", NULL);
      } else {
        game->map_cursor_x = port->x;
        game->map_cursor_y = port->y;
        snprintf(game->status, sizeof(game->status), "Go to Port: %s", port->name);
      }
      return true;
    }
    case MAP_MENU_ACTION_GOTO_PLACE: {
      const int sid = game->units.selected_id;
      const ColonizeUnit* u = units_get_const(&game->units, sid);
      if (sid < 0 || !u || !u->active || !units_is_on_map(u)) {
        set_status(game, "Select a unit", NULL);
      } else {
        game->map_goto_place_mode = true;
        set_status(game, "Go to Place: click destination (Esc cancels)", NULL);
      }
      return true;
    }
    case MAP_MENU_ACTION_TRADE_ROUTE: {
      const int sid = game->units.selected_id;
      if (sid < 0 || !units_is_transport(&game->units, sid)) {
        set_status(game, "Select a ship or wagon", NULL);
        return true;
      }
      game_open_trade_route_picker(game, 1); /* begin */
      return true;
    }
    case MAP_MENU_ACTION_TRADE_CREATE: {
      if (!game->col1_ok) {
        set_status(game, "No save data for trade routes", NULL);
        return true;
      }
      ColonizeCol1Save* col1 = &game->col1;
      int slot = -1;
      for (int i = 0; i < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT; ++i) {
        if (col1->trade_route[i].name[0] == '\0' && col1->trade_route[i].dest_count == 0) {
          slot = i;
          break;
        }
      }
      if (slot < 0) {
        set_status(game, "Trade route list full (12)", NULL);
        return true;
      }
      ColonizeCol1TradeRoute* r = &col1->trade_route[slot];
      memset(r, 0, sizeof(*r));
      snprintf(r->name, sizeof(r->name), "Route %d", slot + 1);
      /* Sea route if a ship is selected; else land/wagon. Cite: Col1 sea byte. */
      const int sid = game->units.selected_id;
      const ColonizeUnit* u = (sid >= 0) ? units_get_const(&game->units, sid) : NULL;
      r->sea = (u && units_is_sea(&game->units, sid)) ? 1u : 0u;
      r->dest_count = 0;
      if (col1->head.trade_route_count < (uint16_t)(slot + 1)) {
        col1->head.trade_route_count = (uint16_t)(slot + 1);
      }
      snprintf(
        game->status,
        sizeof(game->status),
        "Created %s (%s) — add stops via Edit (thin)",
        r->name,
        r->sea ? "sea" : "land"
      );
      return true;
    }
    case MAP_MENU_ACTION_TRADE_EDIT: {
      if (!game->col1_ok) {
        set_status(game, "No save data for trade routes", NULL);
        return true;
      }
      game_open_trade_route_picker(game, 2); /* edit */
      return true;
    }
    case MAP_MENU_ACTION_TRADE_DELETE: {
      if (!game->col1_ok) {
        set_status(game, "No save data for trade routes", NULL);
        return true;
      }
      game_open_trade_route_picker(game, 3); /* delete */
      return true;
    }
    case MAP_MENU_ACTION_DUMP_OVERBOARD: {
      game_request_overboard_confirm(game);
      return true;
    }
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
      if (game_europe_blocked_by_woi(game)) {
        set_status(game, "Europe is closed during the War of Independence", NULL);
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
        int cargo_treasure_gold[EUROPE_SHIP_CARGO_MAX];
        memset(hold_types, 0, sizeof(hold_types));
        memset(hold_amts, 0, sizeof(hold_amts));
        memset(cargo_treasure_gold, 0, sizeof(cargo_treasure_gold));
        game_europe_capture_pax_professions(
          &game->units, sid, cargo_profs, EUROPE_SHIP_CARGO_MAX
        );
        game_europe_capture_pax_treasure_gold(
          &game->units, sid, cargo_treasure_gold, EUROPE_SHIP_CARGO_MAX
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
              game_europe_restore_pax_treasure_gold(
                &game->units, restored, cargo_treasure_gold, cargo_count
              );
              game->units.selected_id = restored;
            }
            set_status(game, "Europe lane is full", NULL);
          } else {
            game_europe_fill_expected_treasure_gold(
              &game->europe, cargo_treasure_gold, cargo_count
            );
            snprintf(game->status, sizeof(game->status), "%s sailed to Europe", ship_name);
            if (!game_try_enter_europe(game)) {
              /* Ship already despawned — stay on map with status from helper. */
            }
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
      game_open_cheat_setview(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_KILL_INDIANS:
      game_open_cheat_kill_indians(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_CREATE_UNIT:
      game_open_cheat_create_unit(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS:
      game_open_cheat_debug_flags(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_SET_HUMAN:
      game_open_cheat_set_human(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION:
      game_cheat_advance_revolution(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_SOUND_TEST:
      game_open_cheat_sound_test(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_MEMORY_CHECK:
      game_cheat_memory_check(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_SHOW_STRATEGY:
      game_cheat_toggle_strategy(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_SHOW_COLONY_SITES:
      game_cheat_toggle_colony_sites(game);
      return true;
    case MAP_MENU_ACTION_CHEAT_TEST_ROUTINE:
      game_cheat_test_routine(game);
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
    units_set_move_watch(game_move_watch, game);
    ColonizeTurnContext ctx;
    game_fill_turn_context(game, &ctx);
    if (!turn_processor_advance(&game->turn_proc, &ctx)) {
      game_finish_end_turn(game, &game->turn_proc.result);
    }
    return true;
  }

  /* Present next queued AI popup once the turn processor is idle. */
  if (!game->ai_popups.open && !game->ai_popups.has_result) {
    ai_popup_try_present_next(&game->ai_popups);
  }

  /* Pace Go-To at 10 tile-steps/sec, or 12.5 with Fast Piece Slide. */
  if (game->units_ok && game->world_map_ok) {
    game->goto_step_accum_ms += dt_ms;
    const uint32_t goto_step_ms =
      (game->col1_ok && game->col1.head.game_options.fast_piece_slide) ? 80u : 100u;
    if (game->goto_step_accum_ms >= goto_step_ms) {
      game->goto_step_accum_ms -= goto_step_ms;
      if (game->goto_step_accum_ms > 200u) {
        game->goto_step_accum_ms = 0; /* drop backlog after hitch */
      }
      int stepped = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &game->units.units[i];
        if (!u->active || !units_orders_follow_goto(u->orders) || !units_is_on_map(u)) {
          continue;
        }
        /* This pacer only ever runs outside turn_processor_active() (see
         * the early return above) — i.e. only during the human's own
         * turn. AI/native units resolve their own goto orders exclusively
         * inside turn_processor_advance(), during their own turn; a unit
         * that's mid-multi-turn goto (e.g. a ship sailing a multi-turn
         * ocean crossing) legitimately still carries that order in a
         * loaded save, but shouldn't visibly keep sliding across the map
         * while it's the human's turn. Player-reported: loading a save
         * captured mid-human-turn showed other Europeans' units moving on
         * their own before the human ended their turn. */
        if (u->nation_id != game->human_nation) {
          continue;
        }
        if (u->orders == UNITS_ORDER_TRADE_ROUTE) {
          game_trade_route_retarget(game, u);
        }
        if (!units_advance_goto_one_step(
              &game->units, u->id, &game->world_map, &game->colonies, &game->move_rng
            )) {
          continue;
        }
        stepped++;
        u = units_get(&game->units, u->id);
        if (u && u->orders == UNITS_ORDER_TRADE_ROUTE) {
          game_trade_route_retarget(game, u);
        }
        if (u && u->nation_id >= 0 && u->nation_id <= 3) {
          map_reveal_radius(&game->world_map, u->x, u->y, u->nation_id, 1);
          if (game->col1_ok && u->nation_id == game->human_nation) {
            game_try_prompt_landho(game);
          }
        }
      }
      if (stepped > 0 && game->units.selected_id >= 0) {
        const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
        if (sel && sel->active && units_is_on_map(sel)) {
          game->map_cursor_x = sel->x;
          game->map_cursor_y = sel->y;
          if (!game->in_menu && !game->in_colony && !game->in_europe && !game->in_report &&
              !game->in_pedia && !game->in_debug_atlas && !game->in_hall_of_fame) {
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

  /* First-land @LANDHO before parent hotkeys; modal gate blocks E/Q/etc. */
  game_try_prompt_landho(game);
  if (game_handle_modal_input(game, input)) {
    if (game->elapsed_ms == UINT32_MAX) {
      return false;
    }
    return true;
  }

  if (input->last_key == COLONIZE_KEY_Q) {
    game_enqueue_yes_no(
      game, GAME_MAP_CONFIRM_QUIT, -1, "DOS", "Exit to DOS?", NULL
    );
    return true;
  }

  if (game->in_report) {
    /* Labor report grid: a click on a profession cell zooms to its detail
     * view (golden: labor_detail.png) instead of anything OK/Esc-related. */
    if (game->report_id == COLONIZE_REPORT_LABOR && game->labor_detail_job < 0 &&
        input->mouse_left_clicked) {
      const int hit = reports_labor_cell_hit(input->mouse_x, input->mouse_y);
      if (hit >= 0) {
        game->labor_detail_job = hit;
        return true;
      }
    }
    /* Congress page 2 has no OK button (golden: full-bleed photo, no chrome)
     * — any click closes it, not just a hit on a drawn box. */
    const bool page2_click_anywhere =
      game->report_id == COLONIZE_REPORT_CONGRESS && game->congress_page2 &&
      input->mouse_left_clicked;
    const bool ok_clicked = page2_click_anywhere ||
      (input->mouse_left_clicked &&
       reports_ok_button_hit(game->report_id, game->congress_page2, input->mouse_x, input->mouse_y));
    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_ENTER ||
        ok_clicked) {
      /* Continental Congress is two pages: closing page 1 (any means — Esc,
       * click, Enter) shows page 2 instead of leaving the report. */
      if (game->report_id == COLONIZE_REPORT_CONGRESS && !game->congress_page2) {
        game->congress_page2 = true;
        return true;
      }
      /* Labor detail view: closing it (Esc/Enter/OK) returns to the grid,
       * same as other reports' Esc — it doesn't leave the report yet. */
      if (game->report_id == COLONIZE_REPORT_LABOR && game->labor_detail_job >= 0) {
        game->labor_detail_job = -1;
        return true;
      }
      /* Economic report is European Trade + however many Cargo in Port
       * pages this nation's colony count needs — OK/Esc/Enter advances to
       * the next page, or leaves the report from the last one. */
      if (game->report_id == COLONIZE_REPORT_ECONOMIC) {
        const int page_count = reports_economic_page_count(
          game->col1_ok ? &game->col1 : NULL, game->human_nation
        );
        if (game->economic_page + 1 < page_count) {
          game->economic_page++;
          return true;
        }
        game->economic_page = 0;
      }
      /* Colony report is Military Garrisons + Sons of Liberty, each
       * paginated over this nation's colonies — same OK/Esc/Enter
       * page-advance shape as Economic above. */
      if (game->report_id == COLONIZE_REPORT_COLONY) {
        const int page_count = reports_colony_page_count(
          game->col1_ok ? &game->col1 : NULL, game->human_nation
        );
        if (game->colony_page + 1 < page_count) {
          game->colony_page++;
          return true;
        }
        game->colony_page = 0;
      }
      game->in_report = false;
      diag_info("Left report screen.");
      if (game->report_exits_to_menu) {
        game->report_exits_to_menu = false;
        /* Record F10 total into the ranked Hall of Fame on Retire
         * (FUN_41f2_0f56 thin). */
        if (game->col1_ok) {
          ColonizeScoreBreakdown sc;
          reports_compute_score(
            &sc, &game->col1, game->human_nation, &game->colonies, &game->europe
          );
          ColonizeHofEntry entry;
          memset(&entry, 0, sizeof(entry));
          snprintf(
            entry.leader,
            sizeof(entry.leader),
            "%s",
            game->leader_name[0] ? game->leader_name : "Governor"
          );
          snprintf(
            entry.nation, sizeof(entry.nation), "%s", new_game_nation_name(game->human_nation)
          );
          entry.score = sc.total;
          entry.year = sc.year;
          entry.difficulty = sc.difficulty;
          game_hof_insert(game, &entry);
          game_hof_save(game);
        }
        game->in_menu = true;
        sound_stop_bgm();
        sound_play(SOUND_TITLE_ID);
        set_status(game, "Retired to main menu", NULL);
      }
      return true;
    }
    /* F-keys switch reports (F2–F10) or open terrain pedia (F1). */
    if (input->last_key >= COLONIZE_KEY_F1 && input->last_key <= COLONIZE_KEY_F10) {
      game_handle_report_fkey(game, input->last_key);
    }
    return true;
  }

  /* Title-menu Hall of Fame screen (reports_render_hall_of_fame). */
  if (game->in_hall_of_fame) {
    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_ENTER) {
      game->in_hall_of_fame = false;
      game->in_menu = true;
      set_status(game, "Colonization Linux Port", NULL);
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
      if (csv->dock_orders_open) {
        colony_screen_close_dock_orders(csv);
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
      if (csv->dock_orders_open) {
        if (csv->dock_orders_selection >= 0 && csv->dock_orders_selection < csv->dock_orders_count) {
          game_colony_apply_dock_order(
            game, csv, csv->dock_orders_actions[csv->dock_orders_selection]
          );
        } else {
          colony_screen_close_dock_orders(csv);
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
            } else {
              const ColonizeColony* col_ref =
                colonies_get(&game->colonies, game->colony_view_id);
              if (col_ref && bid >= 0 && bid < COLONIZE_BUILDING_TYPES_MAX &&
                  col_ref->has_building[bid]) {
                const ColonizeBuildingType* bt =
                  colonies_building_type(&game->colonies, bid);
                set_status(game, "Already built", NULL);
                colonies_emit_already_have_chrome(
                  col_ref, bt ? bt->name : NULL, &game->ai_popups, &game->messages
                );
              }
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
        } else if (ch == '+') {
          const int cargo = csv->selected_cargo;
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
        } else {
          const int cargo = csv->selected_cargo;
          const int max_amt = colony->stock[cargo] < 100 ? colony->stock[cargo] : 100;
          char prompt[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = (game->europe_ok && cargo < game->europe.cargo_count)
                          ? game->europe.cargo[cargo].name
                          : "cargo";
          tok.string1 = "ship";
          tok.number0 = max_amt;
          tok.has_number0 = true;
          popup_msg_fill(
            &game->messages,
            "HOWMUCH1",
            &tok,
            "How much should be loaded?",
            prompt,
            sizeof(prompt)
          );
          howmuch_open(
            &game->howmuch, HOWMUCH_KIND_LOAD, prompt, max_amt, max_amt, cargo, 0
          );
        }
        colony_screen_set_status(csv, game->status);
        return true;
      }
      if ((ch == 'r' || ch == 'R') && colony && !csv->jobs_open && !csv->construction_open &&
          csv->message_kind == COLONY_MSG_NONE) {
        char prompt[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages,
          "RENAMECOLONY",
          NULL,
          "What shall we rename this colony?",
          prompt,
          sizeof(prompt)
        );
        name_entry_open(
          &game->name_entry,
          NAME_ENTRY_KIND_RENAME,
          prompt,
          colony->name,
          game->colony_view_id
        );
        return true;
      }
    }

    if (csv->jobs_open) {
      if (colonize_key_up(input->last_key) && csv->jobs_selection > 0) {
        csv->jobs_selection--;
        return true;
      }
      if (colonize_key_down(input->last_key) && csv->jobs_selection < csv->job_count) {
        csv->jobs_selection++;
        return true;
      }
    } else if (csv->message_kind != COLONY_MSG_NONE) {
      const int max_sel = (csv->message_kind == COLONY_MSG_CONFIRM) ? 1 : 0;
      if (colonize_key_up(input->last_key) && csv->message_selection > 0) {
        csv->message_selection--;
        return true;
      }
      if (colonize_key_down(input->last_key) && csv->message_selection < max_sel) {
        csv->message_selection++;
        return true;
      }
    } else if (csv->eject_open) {
      if (colonize_key_up(input->last_key) && csv->eject_selection > 0) {
        csv->eject_selection--;
        return true;
      }
      if (colonize_key_down(input->last_key) &&
          csv->eject_selection + 1 < csv->eject_role_count) {
        csv->eject_selection++;
        return true;
      }
    } else if (csv->dock_orders_open) {
      if (colonize_key_up(input->last_key) && csv->dock_orders_selection > 0) {
        csv->dock_orders_selection--;
        return true;
      }
      if (colonize_key_down(input->last_key) &&
          csv->dock_orders_selection + 1 < csv->dock_orders_count) {
        csv->dock_orders_selection++;
        return true;
      }
    } else if (csv->construction_open) {
      const int max_sel = csv->buildable_count;
      if (colonize_key_up(input->last_key) && csv->construction_selection > 0) {
        csv->construction_selection--;
        return true;
      }
      if (colonize_key_down(input->last_key) && csv->construction_selection < max_sel) {
        csv->construction_selection++;
        return true;
      }
    } else {
      if (colonize_key_up(input->last_key) && colony && csv->selected_colonist > 0) {
        game_colony_select_colonist(game, csv->selected_colonist - 1);
        return true;
      }
      if (colonize_key_down(input->last_key) && colony &&
          csv->selected_colonist + 1 < colony->colonist_count) {
        game_colony_select_colonist(game, csv->selected_colonist + 1);
        return true;
      }
    }

    /* B = buy remaining construction with gold + warehouse tools. */
    if (input->last_key == COLONIZE_KEY_B && colony &&
        colony->building_in_production >= 0) {
      game_request_buy_construction_confirm(game);
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
            int peek_type = -1;
            {
              const ColonizeUnit* tu = units_get_const(&game->units, csv->transport_unit_id);
              if (tu && hold >= 0 && hold < COLONIZE_UNIT_CARGO_MAX) {
                peek_type = tu->hold_goods_type[hold];
              }
            }
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
              game_emit_warehouse_full(game, game->colony_view_id, peek_type);
            } else if (moved > 0) {
              snprintf(game->status, sizeof(game->status), "Unloaded %d", moved);
            } else if (full) {
              set_status(game, "Warehouse full", NULL);
              game_emit_warehouse_full(game, game->colony_view_id, peek_type);
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
          const int uid = csv->docked_transport_ids[hit.index];
          if (uid == csv->transport_unit_id && game->units_ok) {
            /* Second click on the already-selected transport: docked-unit
             * orders (DOS FUN_2f2b_5746 / @COLONYUNIT), same select-then-
             * click-assigns convention used elsewhere in the colony screen. */
            colony_screen_open_dock_orders(csv, &game->units, &game->messages, uid);
          } else {
            csv->transport_unit_id = uid;
            const ColonizeUnit* tu = units_get_const(&game->units, uid);
            const ColonizeUnitType* tt = tu ? units_type(&game->units, tu->type_index) : NULL;
            snprintf(
              game->status,
              sizeof(game->status),
              "%s",
              tt && tt->name[0] ? tt->name : "Transport selected"
            );
            colony_screen_set_status(csv, game->status);
          }
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
      case COLONY_HIT_DOCK_ORDERS_ROW:
        if (hit.index >= 0 && hit.index < csv->dock_orders_count) {
          game_colony_apply_dock_order(game, csv, csv->dock_orders_actions[hit.index]);
        }
        break;
      case COLONY_HIT_DOCK_ORDERS_OUTSIDE:
        colony_screen_close_dock_orders(csv);
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
      case COLONY_HIT_MULTI_UNIT_ICON:
        /* hit.index is the unit id (see colony_screen_hit_test). Same
         * select-then-click convention as the Transport strip: second click
         * on the already-selected unit opens its docked/stationed-unit
         * orders (DOS FUN_2f2b_59a0 double-click → FUN_2f2b_5746). */
        if (hit.index >= 0 && game->units_ok) {
          if (hit.index == csv->multi_unit_selected_id) {
            colony_screen_open_dock_orders(csv, &game->units, &game->messages, hit.index);
          } else {
            csv->multi_unit_selected_id = hit.index;
            const ColonizeUnit* tu = units_get_const(&game->units, hit.index);
            const ColonizeUnitType* tt = tu ? units_type(&game->units, tu->type_index) : NULL;
            snprintf(
              game->status, sizeof(game->status), "%s", tt && tt->name[0] ? tt->name : "Unit selected"
            );
            colony_screen_set_status(csv, game->status);
          }
        }
        break;
      case COLONY_HIT_MULTI_PANE:
        if (csv->multi_mode == COLONY_MULTI_PRODUCTION) {
          csv->show_production_numbers = !csv->show_production_numbers;
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
        game_request_buy_construction_confirm(game);
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
          } else {
            const ColonizeColony* col_ref =
              colonies_get(&game->colonies, game->colony_view_id);
            if (col_ref && bid >= 0 && bid < COLONIZE_BUILDING_TYPES_MAX &&
                col_ref->has_building[bid]) {
              const ColonizeBuildingType* bt =
                colonies_building_type(&game->colonies, bid);
              set_status(game, "Already built", NULL);
              colonies_emit_already_have_chrome(
                col_ref, bt ? bt->name : NULL, &game->ai_popups, &game->messages
              );
            }
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
      if (colonize_key_up(input->last_key) && eu->menu_selection > 0) {
        eu->menu_selection--;
      } else if (colonize_key_down(input->last_key) && eu->menu_selection < max_sel) {
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

    /* L / '=' : howmuch buy. U : sell howmuch / best hold. */
    if (input->last_key == COLONIZE_KEY_L) {
      if (eu->selected_harbor < 0) {
        snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
      } else {
        const int cargo = eu->selected_market;
        const int max_amt = 100;
        char prompt[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 =
          (cargo >= 0 && cargo < eu->cargo_count) ? eu->cargo[cargo].name : "cargo";
        tok.string1 = "ship";
        tok.number0 = max_amt;
        tok.has_number0 = true;
        tok.number1 = (cargo >= 0 && cargo < eu->cargo_count) ? eu->cargo[cargo].bid : 0;
        tok.has_number1 = true;
        popup_msg_fill(
          &game->messages, "HOWMUCH4", &tok, "How much to purchase?", prompt, sizeof(prompt)
        );
        howmuch_open(
          &game->howmuch, HOWMUCH_KIND_BUY, prompt, max_amt, max_amt, cargo, 0
        );
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
          EuropeHarborShip* ship = &eu->harbor[eu->selected_harbor];
          const int cargo = ship->hold_goods_type[hold];
          const int max_amt = ship->hold_goods_amount[hold];
          char prompt[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 =
            (cargo >= 0 && cargo < eu->cargo_count) ? eu->cargo[cargo].name : "cargo";
          tok.string2 = eu->port_city[0] ? eu->port_city : "Europe";
          tok.number0 = max_amt;
          tok.has_number0 = true;
          tok.number1 = (cargo >= 0 && cargo < eu->cargo_count) ? eu->cargo[cargo].bid : 0;
          tok.has_number1 = true;
          popup_msg_fill(
            &game->messages, "HOWMUCH5", &tok, "How much to sell?", prompt, sizeof(prompt)
          );
          howmuch_open(
            &game->howmuch, HOWMUCH_KIND_SELL, prompt, max_amt, max_amt, cargo, hold
          );
        }
      }
      return true;
    }
    for (int ti = 0; ti < input->text_input_len; ++ti) {
      const char ch = input->text_input[ti];
      if (ch == '=' && eu->selected_harbor >= 0) {
        /* Same as L — amount dialog. */
        const int cargo = eu->selected_market;
        const int max_amt = 100;
        char prompt[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 =
          (cargo >= 0 && cargo < eu->cargo_count) ? eu->cargo[cargo].name : "cargo";
        tok.string1 = "ship";
        tok.number0 = max_amt;
        tok.has_number0 = true;
        tok.number1 = (cargo >= 0 && cargo < eu->cargo_count) ? eu->cargo[cargo].bid : 0;
        tok.has_number1 = true;
        popup_msg_fill(
          &game->messages, "HOWMUCH4", &tok, "How much to purchase?", prompt, sizeof(prompt)
        );
        howmuch_open(
          &game->howmuch, HOWMUCH_KIND_BUY, prompt, max_amt, max_amt, cargo, 0
        );
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
        if (europe_cargo_boycotted(eu, hit.index)) {
          /* GAME.TXT @SOMEBOYCOTT: "...click on the cargo type in question"
           * to ask that the boycott be lifted -- pay-back-taxes buyback,
           * not the normal buy/sell flow. See europe_buyback_boycott. */
          if (game->col1_ok && game->human_nation >= 0) {
            europe_buyback_boycott(eu, &game->col1, game->human_nation, hit.index);
          }
        } else if (eu->selected_harbor < 0) {
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
        !game->in_hall_of_fame && game->world_map_ok && game->units_ok) {
      const int sid = game->units.selected_id;
      const ColonizeUnit* su = units_get_const(&game->units, sid);
      if (su && units_is_pioneer(&game->units, sid) && su->moves_left > 0) {
        char msg[96];
        units_pioneer_plow(
          &game->units,
          sid,
          &game->world_map,
          msg,
          sizeof(msg),
          &game->colonies,
          &game->ai_popups,
          &game->messages
        );
        set_status(game, msg, NULL);
        return true;
      }
    }
    game_open_pedia_list(game, PEDIA_CAT_CARGO);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_R) {
    if (!game->in_menu && !game->in_europe && !game->in_colony && !game->in_report &&
        !game->in_hall_of_fame && game->world_map_ok && game->units_ok) {
      const int sid = game->units.selected_id;
      const ColonizeUnit* su = units_get_const(&game->units, sid);
      if (su && units_is_pioneer(&game->units, sid) && su->moves_left > 0) {
        char msg[96];
        units_pioneer_road(
          &game->units,
          sid,
          &game->world_map,
          msg,
          sizeof(msg),
          &game->colonies,
          &game->ai_popups,
          &game->messages
        );
        set_status(game, msg, NULL);
        return true;
      }
    }
  }

  if (input->last_key == COLONIZE_KEY_E && !game->in_menu) {
    if (game_try_enter_europe(game)) {
      diag_info("Entered Europe screen.");
    }
    return true;
  }

  if (game->in_menu) {
    if (!game->ai_popups.open && !game->ai_popups.has_result) {
      ai_popup_try_present_next(&game->ai_popups);
    }
    if (game->ai_popups.open || game->save_load.open) {
      /* Early modal gate should have consumed; keep status refresh only. */
      return true;
    }
    if (input->last_key == COLONIZE_KEY_ESCAPE) {
      game_enqueue_yes_no(
        game, GAME_MAP_CONFIRM_TITLE_EXIT, -1, "DOS", "Exit to DOS?", NULL
      );
      return true;
    }
    if (colonize_key_up(input->last_key) && game->menu_selection > 0) {
      game->menu_selection--;
    } else if (colonize_key_down(input->last_key) &&
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

    /* Skip when activation above left the title menu (e.g. Hall of Fame,
     * Customize/America/New World, LOAD dialog): that screen owns its own
     * status now, and game_update runs every frame so this would otherwise
     * clobber it one frame later even though in_menu is already false. */
    if (game->in_menu && game->menu_option_count > 0 && !game->ai_popups.open) {
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
    /*
     * VIEW ~Hidden Terrain (H): auto-advance phases 1→2→3 on a timer, then
     * hold at 3 until any click/keypress cancels back to the normal view.
     * Simplification vs. DOS: any input during the brief auto-peel (not just
     * once resting at 3) also cancels — equivalent information, less state.
     */
    if (game->hidden_terrain_phase != 0) {
      const bool any_input = input->mouse_left_clicked || input->mouse_right_clicked ||
        input->last_key != COLONIZE_KEY_NONE;
      if (any_input) {
        game->hidden_terrain_phase = 0;
        set_status(game, "Hidden Terrain view off", NULL);
        return true;
      }
      if (game->hidden_terrain_phase < 3 &&
          game->elapsed_ms - game->hidden_terrain_phase_ms >= HIDDEN_TERRAIN_STEP_MS) {
        game->hidden_terrain_phase++;
        game->hidden_terrain_phase_ms = game->elapsed_ms;
        if (game->hidden_terrain_phase == 2) {
          set_status(game, "Hidden Terrain: roads, resources hidden", NULL);
        } else {
          set_status(game, "Hidden Terrain: hills and forest hidden", NULL);
        }
      }
    }

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

    /* Drop selection if the active unit no longer has moves (e.g. after load). */
    if (game->units_ok && game->units.selected_id >= 0) {
      const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
      if (!game_unit_selectable(game, sel)) {
        const int tx = sel ? sel->x : game->map_cursor_x;
        const int ty = sel ? sel->y : game->map_cursor_y;
        game_select_tile(game, tx, ty);
      }
    }

    /*
     * F1 terrain pedia at cursor; F2–F10 adviser / report screens. Shift-Fn =
     * CHEAT menu hotkeys when unlocked, per MENU.TXT @CUP: ~F~0~1 Create
     * Unit, ~F~0~2 Debug Info Flags, ~F~0~4 Reveal Map, ~F~0~5 Set Human
     * Player, ~F~0~6 Kill Indians, ~F~0~7 Advance Revolution Status,
     * ~F~0~8 Show Strategy, ~F~0~9 Show Colony Sites, ~F~1~0 Test Routine.
     * Sound Test / Memory Check have no @CUP hotkey (menu-only).
     */
    if (input->last_key >= COLONIZE_KEY_F1 && input->last_key <= COLONIZE_KEY_F10) {
      if (input->shift_held && game->map_menu.cheat_visible) {
        switch (input->last_key) {
          case COLONIZE_KEY_F1:
            game_open_cheat_create_unit(game);
            return true;
          case COLONIZE_KEY_F2:
            game_open_cheat_debug_flags(game);
            return true;
          case COLONIZE_KEY_F4:
            game_open_cheat_setview(game);
            return true;
          case COLONIZE_KEY_F5:
            game_open_cheat_set_human(game);
            return true;
          case COLONIZE_KEY_F6:
            game_open_cheat_kill_indians(game);
            return true;
          case COLONIZE_KEY_F7:
            game_cheat_advance_revolution(game);
            return true;
          case COLONIZE_KEY_F8:
            game_cheat_toggle_strategy(game);
            return true;
          case COLONIZE_KEY_F9:
            game_cheat_toggle_colony_sites(game);
            return true;
          case COLONIZE_KEY_F10:
            game_cheat_test_routine(game);
            return true;
          default:
            break;
        }
      }
      game_handle_report_fkey(game, input->last_key);
      return true;
    }

    /* Alt-W/I/N unlocks CHEAT; Alt-W alone turns it off (COLONIZE README).
     * Other Alt+letter opens map menu titles (~GAME, ~VIEW, ~ORDERS, …). */
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
      const char alt_letter = game_key_letter(k);
      if (alt_letter && map_menu_open_alt_hotkey(&game->map_menu, alt_letter)) {
        return true;
      }
    }

    game_refresh_orders_menu(game);

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

    /* Plain ORDERS hotkeys (same as choosing the enabled ORDERS item). */
    if (!input->alt_held && input->last_key != COLONIZE_KEY_NONE) {
      const bool space = (input->last_key == COLONIZE_KEY_SPACE);
      const char letter = space ? 0 : game_key_letter(input->last_key);
      const MapMenuAction order_hk = map_menu_orders_hotkey(
        &game->map_menu, letter, input->shift_held, space
      );
      if (order_hk != MAP_MENU_ACTION_NONE) {
        if (!game_apply_map_menu_action(game, order_hk)) {
          return false;
        }
        return true;
      }
      /*
       * Plain VIEW hotkeys (Zoom In ~Z / Zoom Out ~X, Show ~Hidden Terrain —
       * MENU.TXT). H is contextually overloaded: with a ship selected it
       * sails to Europe (below, unconditional on world/units/europe_ok);
       * otherwise it opens the Hidden Terrain reveal.
       */
      const bool ship_selected = game->units_ok && game->units.selected_id >= 0 &&
        units_is_sea(&game->units, game->units.selected_id);
      if (!space && letter && !(letter == 'H' && ship_selected)) {
        const MapMenuAction view_hk = map_menu_view_hotkey(&game->map_menu, letter);
        if (view_hk != MAP_MENU_ACTION_NONE) {
          if (!game_apply_map_menu_action(game, view_hk)) {
            return false;
          }
          return true;
        }
      }
    }

    if ((input->mouse_left_clicked || input->mouse_right_clicked || input->mouse_left_released ||
         (ui_drag_active(&game->ui_drag) && game->ui_drag.kind == UI_DRAG_MAP_GOTO &&
          input->mouse_left_down)) &&
        game->world_map_ok) {
      const int tile_w = game_map_zoom_tile_px(game->map_zoom);
      const int tile_h = tile_w;
      const int map_origin_x = 0;
      const int map_origin_y = MAP_VIEW_ORIGIN_Y;
      int view_cols = 0;
      int view_rows = 0;
      game_map_zoom_view_size(game->map_zoom, &view_cols, &view_rows);
      if (input->mouse_y < map_origin_y) {
        if (input->mouse_left_released && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
          game_ui_drag_clear(game);
        }
        return true;
      }

      int view_x = 0;
      int view_y = 0;
      map_panel_clamp_view_origin(
        (int)game->world_map.width,
        (int)game->world_map.height,
        game->map_view_x,
        game->map_view_y,
        view_cols,
        view_rows,
        &view_x,
        &view_y
      );

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
      if (!map_coords_inset(&game->world_map, mx, my)) {
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
        if (game->map_goto_place_mode) {
          game->map_goto_place_mode = false;
          set_status(game, "Go to Place cancelled", NULL);
          return true;
        }
        game_select_tile(game, mx, my);
        return true;
      }

      if (!input->mouse_left_clicked) {
        return true;
      }

      /* ORDERS Go to Place: click sets goto destination. */
      if (game->map_goto_place_mode && game->units_ok) {
        const int uid = game->units.selected_id;
        const ColonizeUnit* u = units_get_const(&game->units, uid);
        game->map_goto_place_mode = false;
        if (!u || !game_unit_selectable(game, u)) {
          set_status(game, "Go to Place cancelled", NULL);
          return true;
        }
        if (mx == u->x && my == u->y) {
          set_status(game, "Already here", NULL);
          return true;
        }
        if (units_set_goto(
              &game->units, uid, &game->world_map, mx, my, &game->colonies
            )) {
          snprintf(game->status, sizeof(game->status), "Go to (%d,%d)", mx, my);
          game_set_view_center(game, u->x, u->y);
        } else {
          set_status(game, "Cannot go there", NULL);
        }
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
    if (game->map_goto_place_mode) {
      game->map_goto_place_mode = false;
      set_status(game, "Go to Place cancelled", NULL);
      return true;
    }
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

  const int map_min_x = 1;
  const int map_min_y = 1;
  const int map_max_x = game->world_map_ok ? (int)game->world_map.width - 2 : 15;
  const int map_max_y = game->world_map_ok ? (int)game->world_map.height - 2 : 15;

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
      int cargo_treasure_gold[EUROPE_SHIP_CARGO_MAX];
      memset(hold_types, 0, sizeof(hold_types));
      memset(hold_amts, 0, sizeof(hold_amts));
      memset(cargo_treasure_gold, 0, sizeof(cargo_treasure_gold));
      game_europe_capture_pax_professions(
        &game->units, sid, cargo_profs, EUROPE_SHIP_CARGO_MAX
      );
      game_europe_capture_pax_treasure_gold(
        &game->units, sid, cargo_treasure_gold, EUROPE_SHIP_CARGO_MAX
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
            game_europe_restore_pax_treasure_gold(
              &game->units, restored, cargo_treasure_gold, cargo_count
            );
            game->units.selected_id = restored;
          }
          set_status(game, "Europe lane is full", NULL);
        } else {
          game_europe_fill_expected_treasure_gold(
            &game->europe, cargo_treasure_gold, cargo_count
          );
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
    game_request_disband_confirm(game);
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

  /* B: found a colony — Indian land gold/Minuit via colonies_found_with_indian_land. */
  if (input->last_key == COLONIZE_KEY_B && game->world_map_ok) {
    (void)game_try_found_colony_at_cursor(game);
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
        if (nx < map_min_x) {
          nx = map_min_x;
        }
        if (ny < map_min_y) {
          ny = map_min_y;
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
    game_open_save_load(game, SAVE_LOAD_MODE_SAVE);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_L) {
    game_open_save_load(game, SAVE_LOAD_MODE_LOAD);
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
              !game->in_colony && !game->in_report && !game->in_hall_of_fame)
    ? game->palette
    : (game->in_debug_atlas && debug_atlas_palette(&game->debug_atlas))
      ? *debug_atlas_palette(&game->debug_atlas)
      : (game->in_pedia && game->pedia_view == PEDIA_VIEW_LIST && game->pedia_wood_ok &&
         game->pedia_wood.has_palette)
        ? game->pedia_wood.palette
        : (game->in_report && game->reports_ok &&
           game->report_id == COLONIZE_REPORT_CONGRESS && !game->congress_page2 &&
           game->reports.congress_page1_bg_ok && game->reports.congress_page1_bg.has_palette)
          ? game->reports.congress_page1_bg.palette
        : (game->in_report && game->reports_ok && game->reports.background_ok[game->report_id] &&
           game->reports.backgrounds[game->report_id].has_palette)
          ? game->reports.backgrounds[game->report_id].palette
          : (game->in_hall_of_fame && game->reports_ok &&
             game->reports.background_ok[COLONIZE_REPORT_SCORE] &&
             game->reports.backgrounds[COLONIZE_REPORT_SCORE].has_palette)
            ? game->reports.backgrounds[COLONIZE_REPORT_SCORE].palette
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
      game->congress_page2,
      game->labor_detail_job,
      game->economic_page,
      game->colony_page,
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

  if (game->in_hall_of_fame) {
    const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
    ColonizeHofRow rows[COLONIZE_HOF_ROW_MAX];
    const int count = game->hof_count > COLONIZE_HOF_ROW_MAX ? COLONIZE_HOF_ROW_MAX : game->hof_count;
    for (int i = 0; i < count; ++i) {
      const ColonizeHofEntry* e = &game->hof_entries[i];
      snprintf(rows[i].leader, sizeof(rows[i].leader), "%s", e->leader);
      snprintf(rows[i].nation, sizeof(rows[i].nation), "%s", e->nation);
      rows[i].score = e->score;
      rows[i].year = e->year;
    }
    reports_render_hall_of_fame(
      game->reports_ok ? &game->reports : NULL, rows, count, font, framebuffer
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
    if (game->save_load.open || game->ai_popups.open) {
      const ColonizeFont* font = game->intro_font_ok ? &game->intro_font :
                                 (game->colony_font_ok ? &game->colony_font :
                                  (game->menu_font_ok ? &game->menu_font : NULL));
      const ColonizeSpriteSheet* wood =
        game->menu_opentile_ok ? &game->menu_opentile :
        ((game->map_panel_ok && game->map_panel.wood_ok) ? &game->map_panel.wood_tile : NULL);
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      if (game->save_load.open) {
        save_load_render(
          (SaveLoadDialog*)&game->save_load,
          font,
          wood,
          &popup_cols,
          COLONIZE_COL_BASIC,
          COLONIZE_COL_SELECT,
          framebuffer
        );
      }
      if (game->ai_popups.open) {
        ai_popup_render(
          (AiPopupState*)&game->ai_popups,
          font,
          wood,
          &popup_cols,
          COLONIZE_COL_BASIC,
          COLONIZE_COL_SELECT,
          framebuffer
        );
      }
    }
    goto render_log_sample;
  }

  /* Map view: scrollable world map (15<<zoom × 12<<zoom tiles) left of the right info panel. */
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);

  const int map_zoom = game_map_zoom_clamp(game->map_zoom);
  const int tile_w = MAP_ZOOM_NATIVE_TILE; /* offscreen compositing stays native 16px/tile */
  const int tile_h = MAP_ZOOM_NATIVE_TILE;
  const int map_origin_x = 0;
  const int map_origin_y = 0;
  int view_cols = 0;
  int view_rows = 0;
  game_map_zoom_view_size(map_zoom, &view_cols, &view_rows);
  const int screen_tile_px = game_map_zoom_tile_px(map_zoom);

  int view_x = 0;
  int view_y = 0;
  if (game->world_map_ok) {
    map_panel_clamp_view_origin(
      (int)game->world_map.width,
      (int)game->world_map.height,
      game->map_view_x,
      game->map_view_y,
      view_cols,
      view_rows,
      &view_x,
      &view_y
    );
  }

  /*
   * DOS redraws the viewport at 16>>zoom px/tile per FUN_6ba1_000c. This port
   * instead reuses the zoom-0 tile compositor unchanged, drawing the wider
   * (15<<zoom × 12<<zoom) tile grid at native 16px/tile into an offscreen
   * buffer, then nearest-neighbor-decimates it into the fixed 240×192
   * on-screen viewport below — same visual result (more tiles, smaller),
   * different graphics pipeline, per the architectural gap noted in
   * docs/roadmap.md.
   */
  static uint8_t s_map_zoom_buf[MAP_ZOOM_MAX_VIEW_COLS * MAP_ZOOM_NATIVE_TILE *
                                 MAP_ZOOM_MAX_VIEW_ROWS * MAP_ZOOM_NATIVE_TILE];
  ColonizeFramebuffer8 zoom_fb = {
    .width = view_cols * tile_w, .height = view_rows * tile_h, .pixels = s_map_zoom_buf
  };
  memset(zoom_fb.pixels, 0, (size_t)zoom_fb.width * (size_t)zoom_fb.height);
  ColonizeFramebuffer8* const framebuffer_screen = framebuffer;
  framebuffer = &zoom_fb;

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
          if (!map_tile_seen_by(&game->world_map, mx, my, game_fog_nation(game))) {
            /* Unexplored: leave black (framebuffer cleared above). */
            continue;
          }
          underlayer = map_coast_underlayer_sprite_at(&game->world_map, mx, my);
          coast_layers = map_phys0_coast_layer_count(&game->world_map, mx, my);
          base_sprite = (underlayer >= 0) ? underlayer : map_terrain_sprite_at(&game->world_map, mx, my);
          /* Hidden Terrain phase 3: scrub forest reveals as Desert (its cleared
           * base type), not the scrub-ground quirk sprite under its canopy. */
          if (game->hidden_terrain_phase >= 3 && underlayer < 0 &&
              map_tile_is_scrub_forest(&game->world_map, mx, my)) {
            base_sprite = 1;
          }
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
          /* Hidden Terrain phase 3 removes forest canopy. */
          if (forest_sprite >= 0 && game->hidden_terrain_phase < 3) {
            blit_map_sprite(
              &game->phys0, forest_sprite, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
            );
          }
          const int overlay_layers = map_phys0_overlay_count(&game->world_map, mx, my);
          /* MAPEDIT: coast PHYS0, then masked ocean into colour-0 holes, then estuary. */
          const int coast_end = (underlayer >= 0) ? coast_layers : overlay_layers;
          for (int layer = 0; layer < coast_end; ++layer) {
            /*
             * Hidden Terrain phases 2/3: peel non-exempt land PHYS. Resource/
             * rumour markers drop at phase 2; hills drop at phase 3 too (river
             * / mountain stay exempt through phase 3; coast/estuary are water,
             * never peeled).
             */
            if (game->hidden_terrain_phase >= 2) {
              const ColonizeMapOverlayKind kind =
                map_phys0_overlay_kind_at(&game->world_map, mx, my, layer);
              if (kind == MAP_OVERLAY_KIND_RESOURCE || kind == MAP_OVERLAY_KIND_RUMOUR) {
                continue;
              }
              if (game->hidden_terrain_phase >= 3 && kind == MAP_OVERLAY_KIND_HILL) {
                continue;
              }
            }
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
              if (game->hidden_terrain_phase >= 2) {
                const ColonizeMapOverlayKind kind =
                  map_phys0_overlay_kind_at(&game->world_map, mx, my, layer);
                if (kind == MAP_OVERLAY_KIND_RESOURCE || kind == MAP_OVERLAY_KIND_RUMOUR) {
                  continue;
                }
              }
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
          /* Runtime plow / road: PHYS0 149 / 80–88 after static overlays, before fog. */
          {
            const int plow = map_phys0_plow_sprite_at(&game->world_map, mx, my);
            if (plow >= 0 && plow < game->phys0.sprite_count) {
              blit_map_sprite(
                &game->phys0, plow, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
              );
            }
            /* Hidden Terrain phase 2+: roads aren't in the exempt set. */
            const int road_n =
              (game->hidden_terrain_phase >= 2) ? 0 : map_phys0_road_layer_count(&game->world_map, mx, my);
            for (int ri = 0; ri < road_n; ++ri) {
              const int road =
                map_phys0_road_layer_sprite_at(&game->world_map, mx, my, ri);
              if (road >= 0 && road < game->phys0.sprite_count) {
                blit_map_sprite(
                  &game->phys0, road, framebuffer, sx, sy, tile_w, tile_h, map_origin_x,
                  map_origin_y
                );
              }
            }
          }
          /* Fog transitional edges: PHYS0 104–107 black fringe toward unseen. */
          {
            const int fog_n = game_fog_nation(game);
            const int edges = map_fog_edge_count(&game->world_map, mx, my, fog_n);
            for (int ei = 0; ei < edges; ++ei) {
              const int mask = map_fog_edge_mask_sprite_at(&game->world_map, mx, my, fog_n, ei);
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

  /* Hidden Terrain phase 1+ (VIEW ~Hidden Terrain): units/settlements peeled first, stay off. */
  if (game->hidden_terrain_phase == 0 && (game->colonies_ok || game->colonies.colony_count > 0)) {
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
      game_fog_nation(game),
      game->map_palette_ok ? &game->map_palette : NULL
    );
  }

  if (game->hidden_terrain_phase == 0 && game->col1_ok && game->unit_icons_ok) {
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
      game_fog_nation(game)
    );
  }

  if (game->hidden_terrain_phase == 0 && game->units_ok && game->unit_icons_ok) {
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
      game_fog_nation(game),
      game->map_palette_ok ? &game->map_palette : NULL
    );
  }

  /* Decimate the native-res composite down to the fixed 240×192 on-screen
   * viewport (step = 1<<zoom is exact — nominal view width in screen px is
   * (15<<zoom)*(16>>zoom) = 240 at every tier, so this always lands in-bounds). */
  framebuffer = framebuffer_screen;
  {
    const int step = 1 << map_zoom;
    for (int sy = 0; sy < MAP_VIEW_H; ++sy) {
      const int src_y = sy * step;
      if (src_y >= zoom_fb.height) {
        continue;
      }
      const uint8_t* src_row = &zoom_fb.pixels[(size_t)src_y * (size_t)zoom_fb.width];
      uint8_t* dst_row = &framebuffer->pixels[(size_t)(MAP_MENU_BAR_H + sy) * (size_t)framebuffer->width];
      for (int sx = 0; sx < MAP_VIEW_W; ++sx) {
        const int src_x = sx * step;
        if (src_x < zoom_fb.width) {
          dst_row[sx] = src_row[src_x];
        }
      }
    }
  }

  /*
   * Map tile cursor: blinking white outline only in tile-select mode (no unit selected).
   * CURSOR.SS is the OS mouse pointer, not a tile overlay. Drawn post-decimation at
   * on-screen tile size (16>>zoom) so the outline stays crisp at every zoom tier.
   */
  if (game->units.selected_id < 0) {
    const int sx = game->map_cursor_x - view_x;
    const int sy = game->map_cursor_y - view_y;
    if (sx >= 0 && sy >= 0 && sx < view_cols && sy < view_rows) {
      const bool blink_on = ((game->elapsed_ms / 250u) % 2u) == 0u;
      if (blink_on) {
        const int cx0 = sx * screen_tile_px;
        const int cy0 = MAP_MENU_BAR_H + sy * screen_tile_px;
        for (int y = cy0; y < cy0 + screen_tile_px; ++y) {
          for (int x = cx0; x < cx0 + screen_tile_px; ++x) {
            if (x < 0 || y < 0 || x >= framebuffer->width || y >= framebuffer->height) {
              continue;
            }
            if (x == cx0 || x == cx0 + screen_tile_px - 1 || y == cy0 ||
                y == cy0 + screen_tile_px - 1) {
              framebuffer->pixels[y * framebuffer->width + x] = 15;
            }
          }
        }
      }
    }
  }

  /*
   * CHEAT Show Strategy: each AI nation's top-priority goal slot
   * (ai_goals_primary — AI_GOAL_* code + target tile), labelled at that
   * tile. Surfaces the live AI planner state the DOS debug tool exposed.
   */
  if (game->debug_show_strategy) {
    const ColonizeFont* dbg_font =
      game->colony_font_ok ? &game->colony_font : (game->menu_font_ok ? &game->menu_font : NULL);
    if (dbg_font) {
      static const char k_nation_letter[4] = {'E', 'F', 'S', 'D'};
      for (int n = 0; n < 4; ++n) {
        if (n == game->human_nation) {
          continue;
        }
        const AiGoalSlot* g = ai_goals_primary(n, 0);
        if (!g || g->code == AI_GOAL_EMPTY) {
          continue;
        }
        const int tx = g->x - view_x;
        const int ty = g->y - view_y;
        if (tx < 0 || ty < 0 || tx >= view_cols || ty >= view_rows) {
          continue;
        }
        const char* code_name = "?";
        switch (g->code) {
          case AI_GOAL_CONTACT: code_name = "CONTACT"; break;
          case AI_GOAL_FOUND: code_name = "FOUND"; break;
          case AI_GOAL_LABOR: code_name = "LABOR"; break;
          case AI_GOAL_MILITARY: code_name = "MILIT"; break;
          case AI_GOAL_COLONY: code_name = "COLONY"; break;
          case AI_GOAL_MIL_EXPAND: code_name = "MILEXP"; break;
          case AI_GOAL_COLONY_ALT: code_name = "COLONY2"; break;
          default: break;
        }
        char label[16];
        snprintf(label, sizeof(label), "%c:%s", k_nation_letter[n], code_name);
        font_draw_text(
          dbg_font, framebuffer, tx * screen_tile_px, MAP_MENU_BAR_H + ty * screen_tile_px, label, 15
        );
      }
    }
  }

  /*
   * CHEAT Show Colony Sites: each AI nation's cached best founding tile
   * (ai_goals_best_found_tile), marked at that tile.
   */
  if (game->debug_show_colony_sites) {
    const ColonizeFont* dbg_font =
      game->colony_font_ok ? &game->colony_font : (game->menu_font_ok ? &game->menu_font : NULL);
    if (dbg_font) {
      static const char k_nation_letter[4] = {'E', 'F', 'S', 'D'};
      for (int n = 0; n < 4; ++n) {
        int bx = 0;
        int by = 0;
        if (!ai_goals_best_found_tile(n, &bx, &by)) {
          continue;
        }
        const int tx = bx - view_x;
        const int ty = by - view_y;
        if (tx < 0 || ty < 0 || tx >= view_cols || ty >= view_rows) {
          continue;
        }
        char label[4];
        snprintf(label, sizeof(label), "%c*", k_nation_letter[n]);
        font_draw_text(
          dbg_font, framebuffer, tx * screen_tile_px, MAP_MENU_BAR_H + ty * screen_tile_px, label, 14
        );
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
      game_fog_nation(game),
      game->game_year,
      game->game_autumn,
      game->europe.gold,
      game->europe.tax_percent,
      game->europe.nation_name,
      game->map_palette_ok ? &game->map_palette : NULL,
      framebuffer
    );
  }

  const ColonizeFont* hud_font = game->colony_font_ok ? &game->colony_font :
                                 (game->menu_font_ok ? &game->menu_font : NULL);
  if (!game->in_menu && !game->in_colony && !game->in_europe && !game->in_pedia &&
      !game->in_debug_atlas && !game->in_report && !game->in_hall_of_fame) {
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
    if (game->save_load.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      save_load_render(
        (SaveLoadDialog*)&game->save_load,
        hud_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->cheat_list.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      cheat_list_render(
        (CheatListDialog*)&game->cheat_list,
        hud_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->options_dlg.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      const ColonizeFont* popup_font =
        game->intro_font_ok ? &game->intro_font
        : (game->menu_font_ok ? &game->menu_font : hud_font);
      options_dialog_render(
        (OptionsDialog*)&game->options_dlg,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->combat_analysis.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      const ColonizeFont* popup_font =
        game->intro_font_ok ? &game->intro_font
        : (game->menu_font_ok ? &game->menu_font : hud_font);
      combat_analysis_render(
        (CombatAnalysisDialog*)&game->combat_analysis,
        popup_font,
        wood,
        game->unit_icons_ok ? &game->unit_icons : NULL,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        game->map_palette_ok ? &game->map_palette : NULL,
        framebuffer
      );
    }
    if (game->name_entry.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      const ColonizeFont* popup_font =
        game->intro_font_ok ? &game->intro_font
        : (game->menu_font_ok ? &game->menu_font : hud_font);
      name_entry_render(
        (NameEntryDialog*)&game->name_entry,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->howmuch.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      const ColonizeFont* popup_font =
        game->intro_font_ok ? &game->intro_font
        : (game->menu_font_ok ? &game->menu_font : hud_font);
      howmuch_render(
        (HowmuchDialog*)&game->howmuch,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->ai_popups.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      /* Map dialogs use FONTINTR (nation-explanation size), not FONTTINY HUD. */
      const ColonizeFont* popup_font =
        game->intro_font_ok ? &game->intro_font
        : (game->menu_font_ok ? &game->menu_font : hud_font);
      ai_popup_render(
        (AiPopupState*)&game->ai_popups,
        popup_font,
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

  game->platform = platform;
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

bool game_in_hall_of_fame(const ColonizeGameState* game) {
  return game && game->in_hall_of_fame;
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

int game_hof_count(const ColonizeGameState* game) {
  if (!game) {
    return 0;
  }
  return game->hof_count > COLONIZE_HOF_MAX ? COLONIZE_HOF_MAX : game->hof_count;
}

bool game_hof_entry(const ColonizeGameState* game, int index, ColonizeHofEntryView* out) {
  if (!game || !out || index < 0 || index >= game_hof_count(game)) {
    return false;
  }
  const ColonizeHofEntry* e = &game->hof_entries[index];
  snprintf(out->leader, sizeof(out->leader), "%s", e->leader);
  snprintf(out->nation, sizeof(out->nation), "%s", e->nation);
  out->score = e->score;
  out->year = e->year;
  out->difficulty = e->difficulty;
  return true;
}
