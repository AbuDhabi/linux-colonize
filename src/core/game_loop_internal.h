#ifndef COLONIZE_CORE_GAME_LOOP_INTERNAL_H
#define COLONIZE_CORE_GAME_LOOP_INTERNAL_H

/*
 * Stage seams for game_loop.c's three dispatchers:
 *   game_update  -> game_update_* stages (GameUpdateStep)
 *   game_try_unit_move -> game_move_* stages (GameMoveStep)
 *   game_render  -> game_render_* stages (mostly void, some bool "claimed?")
 * ColonizeGameState is fully defined in core/game_dialogs.h (included by
 * game_loop.c), so tests can build a real instance. See core/internal.h for
 * the COLONIZE_INTERNAL / COLONIZE_TESTING pattern this follows.
 */

#include "core/game_dialogs.h"
#include "core/internal.h"
#include "core/world.h"

typedef enum GameMoveStep {
  GAME_MOVE_CONTINUE = 0,  /* fall through to the next stage of game_try_unit_move */
  GAME_MOVE_RETURN_TRUE,   /* game_try_unit_move returns true immediately */
  GAME_MOVE_RETURN_FALSE   /* game_try_unit_move returns false immediately */
} GameMoveStep;

typedef enum GameUpdateStep {
  GAME_UPDATE_CONTINUE = 0, /* fall through to the next step of game_update */
  GAME_UPDATE_RETURN_TRUE, /* game_update returns true immediately */
  GAME_UPDATE_RETURN_FALSE /* game_update returns false immediately (quit) */
} GameUpdateStep;

#ifdef COLONIZE_TESTING
bool game_move_is_near_human(
  const ColonizeGameState* game, const ColonizeUnit* mover, const ColonizeWorldMap* map,
  int x, int y
);
COLONIZE_INTERNAL void game_move_watch_w(
  const ColonizeWorld* w,
  void* user,
  int unit_id,
  int from_x,
  int from_y,
  int to_x,
  int to_y
);
/* Compat shim: kept — registered by name as the ColonizeUnitsMoveWatchFn
 * callback (units_set_move_watch), so its signature is fixed by that
 * typedef and cannot move to ColonizeWorld. */
COLONIZE_INTERNAL void game_move_watch(
  void* user,
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int unit_id,
  int from_x,
  int from_y,
  int to_x,
  int to_y
);
void game_render_modal_overlays(
  const ColonizeGameState* game, const ColonizeFont* last_resort,
  ColonizeFramebuffer8* framebuffer
);
GameMoveStep game_move_passenger_unload(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, const ColonizeColonyPool* colonies,
  int dest_x, int dest_y
);
GameMoveStep game_move_sea_unit(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, const ColonizeColonyPool* colonies,
  int dest_x, int dest_y
);
GameMoveStep game_move_native_prompts(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, int dest_x, int dest_y
);
GameMoveStep game_move_colony_prompts(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, int dest_x, int dest_y
);
GameMoveStep game_move_commit(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, const ColonizeColonyPool* colonies,
  int dest_x, int dest_y
);
GameUpdateStep game_update_services(
  ColonizeGameState* game, const ColonizeInputState* input, uint32_t dt_ms
);
GameUpdateStep game_update_unit_pacer(
  ColonizeGameState* game, const ColonizeInputState* input, uint32_t dt_ms
);
GameUpdateStep game_update_report_screen(ColonizeGameState* game, const ColonizeInputState* input);
GameUpdateStep game_update_colony_screen(ColonizeGameState* game, const ColonizeInputState* input);
GameUpdateStep game_update_europe_screen(ColonizeGameState* game, const ColonizeInputState* input);
GameUpdateStep game_update_pedia_screen(ColonizeGameState* game, const ColonizeInputState* input);
GameUpdateStep game_update_debug_atlas_screen(ColonizeGameState* game, const ColonizeInputState* input);
GameUpdateStep game_update_title_menu(ColonizeGameState* game, const ColonizeInputState* input);
GameUpdateStep game_update_map_menu_bar(ColonizeGameState* game, const ColonizeInputState* input);
GameUpdateStep game_update_map_keys(ColonizeGameState* game, const ColonizeInputState* input);
void game_render_begin_menu(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer);
bool game_render_fullscreen_takeover(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette
);
void game_render_select_palette(
  const ColonizeGameState* game, const ColonizeFramebuffer8* framebuffer, ColonizePalette* palette,
  uint32_t render_log_counter
);
bool game_render_screen(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette
);
void game_render_map_composite(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, int map_zoom, int view_x,
  int view_y, int view_cols, int view_rows
);
void game_render_map_overlays(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, int view_x, int view_y,
  int view_cols, int view_rows, int screen_tile_px
);
void game_render_map_panel(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, int view_x, int view_y,
  int view_cols, int view_rows
);
void game_render_map_dialogs(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette
);
void game_render_map(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette
);
#endif /* COLONIZE_TESTING */


/* ===== Cross-file seams of the game_loop.c split (2026-09-23) =====
 * game_loop.c was 15k lines; it is now split into game_loop{,_menus,_saveload,
 * _render,_orders,_colony,_eot,_update}.c. The declarations below are the
 * symbols used across those files; everything else stayed `static` in its own
 * file. Code moved verbatim — these are the only de-static'd names.
 * ===================================================================== */

/* Map-zoom geometry + hidden-terrain pacing, shared by the split files. */
#define MAP_ZOOM_MAX 3
#define MAP_ZOOM_NATIVE_TILE 16
#define MAP_ZOOM_MAX_VIEW_COLS (15 << MAP_ZOOM_MAX)
#define MAP_ZOOM_MAX_VIEW_ROWS (12 << MAP_ZOOM_MAX)
#define HIDDEN_TERRAIN_STEP_MS 700u

void blit_map_sprite(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y
);
void blit_map_sprite_fill_mask_holes(
  const ColonizeSpriteSheet* fill_sheet,
  int fill_index,
  const ColonizeSpriteSheet* mask_sheet,
  int mask_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y
);
void blit_map_sprite_offset(
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
);
void blit_map_sprite_where_dest(
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
);
const ColonizeSpriteSheet* europe_menu_decoration(const ColonizeGameState* game);
int europe_menu_row_at(const ColonizeGameState* game, int mx, int my);
int europe_ship_icon_sprite(const ColonizeUnitPool* units, const EuropeHarborShip* ship);
bool game_apply_map_menu_action(ColonizeGameState* game, MapMenuAction action);
void game_bind_combat_analysis(ColonizeGameState* game);
void game_center_on_selected_unit(ColonizeGameState* game);
void game_cheat_advance_revolution(ColonizeGameState* game);
void game_cheat_memory_check(ColonizeGameState* game);
void game_cheat_test_routine(ColonizeGameState* game);
void game_cheat_toggle_colony_sites(ColonizeGameState* game);
void game_cheat_toggle_strategy(ColonizeGameState* game);
void game_colony_apply_dock_order(
  ColonizeGameState* game,
  ColonyScreenView* csv,
  ColonyDockOrderAction action
);
bool game_colony_apply_outside_role(
  const ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeUnitPool* units,
  int unit_id,
  int role
);
void game_colony_area_tile_drop(
  ColonizeGameState* game,
  ColonizeColony* colony,
  const ColonizeWorldMap* cmap,
  int tile_index
);
void game_colony_assign_building_drop(ColonizeGameState* game, int building_index);
ColoniesBuildableOpts game_colony_buildable_opts(const ColonizeGameState* game);
void game_colony_commit_construction(ColonizeGameState* game, int bid);
void game_colony_commit_job(ColonizeGameState* game, ColonizeColony* colony, int job);
void game_colony_drag_begin_cargo(ColonizeGameState* game, int cargo_type);
void game_colony_drag_begin_colonist(ColonizeGameState* game, int colonist_index);
void game_colony_drag_begin_hold(ColonizeGameState* game, int hold_index);
void game_colony_drag_begin_outside(ColonizeGameState* game, int unit_id);
bool game_colony_drag_drop(
  ColonizeGameState* game,
  ColonizeColony* colony,
  const ColonizeWorldMap* cmap,
  int mx,
  int my,
  bool shift
);
void game_colony_fence_drop(ColonizeGameState* game, ColonizeColony* colony);
bool game_colony_request_eject(
  ColonizeGameState* game,
  int colonist_index,
  int role
);
void game_colony_select_colonist(ColonizeGameState* game, int colonist_index);
void game_colony_select_outside(ColonizeGameState* game, int unit_id);
void game_commit_new_campaign(ColonizeGameState* game);
void game_do_end_turn(ColonizeGameState* game);
bool game_end_turn_prompt_active(const ColonizeGameState* game);
void game_enqueue_war_scored_choice(ColonizeGameState* game);
void game_enter_colony_at_cursor(ColonizeGameState* game);
void game_europe_deliver_bound_ships(ColonizeGameState* game);
bool game_europe_drag_drop(ColonizeGameState* game, int mx, int my, bool shift);
EuropeHitResult game_europe_hit(const ColonizeGameState* game, int mx, int my);
bool game_europe_menu_confirm(ColonizeGameState* game);
void game_europe_request_sail(ColonizeGameState* game, int hidx);
void game_finish_end_turn(ColonizeGameState* game, const ColonizeTurnResult* result);
int game_fog_nation(const ColonizeGameState* game);
bool game_fortify_treaty_confirm(ColonizeGameState* game, int uid);
void game_handle_report_fkey(ColonizeGameState* game, ColonizeKey key);
const ColonizeSpriteSheet* game_icons(const ColonizeGameState* game);
int game_issue_goto(ColonizeGameState* game, int uid, int dest_x, int dest_y);
void game_join_colony_order(ColonizeGameState* game);
char game_key_letter(ColonizeKey key);
bool game_key_move_delta(ColonizeKey key, int* out_dx, int* out_dy);
const char* game_labels_misc_or(int row, const char* fallback);
void game_map_click_dispatch(ColonizeGameState* game, int mx, int my);
int game_map_zoom_clamp(int zoom);
void game_map_zoom_set(ColonizeGameState* game, int zoom);
int game_map_zoom_tile_px(int zoom);
void game_map_zoom_view_size(int zoom, int* out_cols, int* out_rows);
bool game_names_row(
  const ColonizeGameState* game,
  const char* section,
  int row,
  char* out,
  size_t out_size
);
void game_open_cheat_create_unit(ColonizeGameState* game);
void game_open_cheat_debug_flags(ColonizeGameState* game);
void game_open_cheat_kill_indians(ColonizeGameState* game);
void game_open_cheat_set_human(ColonizeGameState* game);
void game_open_cheat_setview(ColonizeGameState* game);
void game_open_cheat_sound_test(ColonizeGameState* game);
void game_open_debug_atlas(ColonizeGameState* game);
void game_open_pedia_list(ColonizeGameState* game, PediaCategory category);
void game_open_save_load(ColonizeGameState* game, SaveLoadMode mode);
void game_open_terrain_pedia_at_cursor(ColonizeGameState* game);
void game_order_board(ColonizeGameState* game);
bool game_order_fortify(ColonizeGameState* game, int uid);
void game_order_return_europe(ColonizeGameState* game);
void game_order_unload(ColonizeGameState* game);
int game_owned_unit_at(const ColonizeGameState* game, int x, int y);
const ColonizeFont* game_pedia_font(const ColonizeGameState* game);
uint32_t game_pick_rng_seed(const ColonizeGameState* game, uint32_t fallback);
void game_refresh_orders_menu(ColonizeGameState* game);
void game_retire_after_score(ColonizeGameState* game);
bool game_screen_owns_display(const ColonizeGameState* game);
void game_select_tile(ColonizeGameState* game, int x, int y);
void game_status_from_menu_row(
  ColonizeGameState* game,
  const char* section,
  int row,
  char* out,
  size_t out_size
);
void game_trade_route_retarget(ColonizeGameState* game, ColonizeUnit* u);
void game_ui_drag_clear(ColonizeGameState* game);
void game_ui_drag_set_icon(ColonizeGameState* game, int sprite);
bool game_unit_selectable(const ColonizeGameState* game, const ColonizeUnit* u);
bool game_units_pending_orders(const ColonizeGameState* game);
int game_voyage_ship_count(const ColonizeGameState* game);
int game_voyage_turns(ColonizeGameState* game);
int game_voyage_turns_for(ColonizeGameState* game, int ships);
void game_water_cycle_tick(ColonizeGameState* game);
void load_begin_menu(ColonizeGameState* game);
void render_colony_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer);
void render_europe_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer);
void render_pedia_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer);

void game_combat_watch(
  void* user, const ColonizeUnitPool* pool, int attacker_id, int def_x, int def_y
);
void game_combat_dissolve(void* user, int phase);
void game_combat_popup_pump(void* user);

void game_hof_load(ColonizeGameState* game);
void game_hof_save(const ColonizeGameState* game);
void game_hof_insert(ColonizeGameState* game, const ColonizeHofEntry* entry);

/* FUN_479b raw 76477-76484: crown Man-O-War exemption from the WoI Europe
 * gate (bugs.md #870). */
bool game_ship_woi_europe_exempt(const ColonizeGameState* game, const ColonizeUnit* ship);

#endif /* COLONIZE_CORE_GAME_LOOP_INTERNAL_H */
