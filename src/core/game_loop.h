#ifndef COLONIZE_GAME_LOOP_H
#define COLONIZE_GAME_LOOP_H

#include <stdbool.h>
#include <stdint.h>

#include "platform/platform.h"

typedef struct ColonizeGameConfig {
  const char* data_dir;
  const char* save_dir;
  /* Fixed campaign RNG seed (DOS timer word; VR_SEED = 100). Used only when
   * rng_seed_set; 0 is a valid seed. Unset → elapsed_ms / 1. */
  uint32_t rng_seed;
  bool rng_seed_set;
  /* DEBUG navbar + pointer HUD. Ignored unless the matching *_set flag is set
   * (tests leave them unset and keep the hardcoded off-by-default). */
  bool debug_menu;
  bool debug_menu_set;
  bool show_mouse_coords;
  bool show_mouse_coords_set;
  bool show_building_rects;
  bool show_building_rects_set;
  bool debug_logs;
  bool debug_logs_set;
} ColonizeGameConfig;

typedef struct ColonizeGameState ColonizeGameState;

ColonizeGameState* game_create(const ColonizeGameConfig* config);
void game_destroy(ColonizeGameState* game);
/* Platform for nested Combat Analysis present loop (set each frame from main). */
void game_set_platform(ColonizeGameState* game, ColonizePlatform* platform);
bool game_update(ColonizeGameState* game, const ColonizeInputState* input, uint32_t dt_ms);
void game_render(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette);
const char* game_status_text(const ColonizeGameState* game);
/* Build/toggle SDL mouse cursor from CURSOR.SS when the pointer is over the 320x200 frame. */
void game_apply_mouse_cursor(
  ColonizeGameState* game,
  ColonizePlatform* platform,
  int mouse_x,
  int mouse_y
);

/* New-game wizard / campaign identity (for smoke tests). */
bool game_in_menu(const ColonizeGameState* game);
bool game_in_new_game(const ColonizeGameState* game);
int game_human_nation(const ColonizeGameState* game);
int game_difficulty(const ColonizeGameState* game);
const char* game_leader_name(const ColonizeGameState* game);

/*
 * Hall of Fame: read-only view of the ranked HOF.TXT table (desc by score).
 * game_hof_entry returns false when index is out of [0, game_hof_count).
 */
typedef struct ColonizeHofEntryView {
  char leader[32];
  char nation[24];
  int score;
  int year;
  int difficulty; /* 0 Discoverer .. 4 Viceroy */
  int rating; /* Colonization Rating percent (DOS HoF sort key) */
  bool declared;
  bool achieved;
} ColonizeHofEntryView;

int game_hof_count(const ColonizeGameState* game);
bool game_hof_entry(const ColonizeGameState* game, int index, ColonizeHofEntryView* out);

/* True while the title-menu "View Hall of Fame" screen is open (Esc/Enter closes it). */
bool game_in_hall_of_fame(const ColonizeGameState* game);

/*
 * Headless play-smoke probes (tests/smoke/test_play_smoke.c). Read-only;
 * enough to drive new-game → landfall → found colony → colony screen →
 * Europe → save from a key script without reaching into the state struct.
 */
bool game_in_colony_screen(const ColonizeGameState* game);
bool game_in_europe_screen(const ColonizeGameState* game);
/* Start OPENING.EXE if settings allow. True while the intro owns the screen. */
bool game_try_start_intro(ColonizeGameState* game);
/* Any modal that swallows input (AI popup, name entry, save/load, ...). */
bool game_modal_open(const ColonizeGameState* game);
/* AI popup queued but not presented yet: feed one idle frame before keys. */
bool game_ai_popup_pending(const ColonizeGameState* game);
/* AiPopupTag of the open AI popup, or -1 when none is open. */
int game_ai_popup_tag(const ColonizeGameState* game);
bool game_save_dialog_open(const ColonizeGameState* game);
bool game_turn_busy(const ColonizeGameState* game);
uint32_t game_turn_number(const ColonizeGameState* game);
int game_colony_count(const ColonizeGameState* game);
bool game_colony_pos(const ColonizeGameState* game, int index, int* x, int* y);
int game_selected_unit(const ColonizeGameState* game);
/* False when id is not an active unit. is_sea/moves_left/x/y may be NULL. */
bool game_unit_info(
  const ColonizeGameState* game, int unit_id, int* x, int* y, bool* is_sea, int* moves_left
);

#endif
