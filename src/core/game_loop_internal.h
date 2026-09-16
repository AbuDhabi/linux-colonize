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

#endif /* COLONIZE_CORE_GAME_LOOP_INTERNAL_H */
