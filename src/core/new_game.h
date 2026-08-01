#ifndef COLONIZE_NEW_GAME_H
#define COLONIZE_NEW_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/map_gen.h"
#include "core/pik.h"
#include "core/popup.h"
#include "core/ss.h"
#include "platform/platform.h"

#define NEW_GAME_LEADER_NAME_MAX 32
#define NEW_GAME_MAP_NAME_MAX 64
#define NEW_GAME_OPTION_MAX 16
#define NEW_GAME_OPTION_LABEL_LEN 48
#define NEW_GAME_SAIL_FRAMES 10

typedef enum NewGamePath {
  NEW_GAME_PATH_NEW_WORLD = 0,
  NEW_GAME_PATH_AMERICA
} NewGamePath;

typedef enum NewGamePhase {
  NEW_GAME_PHASE_IDLE = 0,
  NEW_GAME_PHASE_AMERICA_CHOICE,
  NEW_GAME_PHASE_MAP_PICK,
  NEW_GAME_PHASE_DIFFICULTY,
  NEW_GAME_PHASE_NATION,
  NEW_GAME_PHASE_LEADER_NAME,
  NEW_GAME_PHASE_NATION_LORE_A,
  NEW_GAME_PHASE_NATION_LORE_B,
  NEW_GAME_PHASE_KING,
  NEW_GAME_PHASE_SAIL,
  NEW_GAME_PHASE_COMMIT
} NewGamePhase;

typedef struct NewGameWizard {
  NewGamePhase phase;
  NewGamePath path;
  int difficulty; /* 0..4 */
  int nation; /* 0..3 England..Netherlands */
  char leader_name[NEW_GAME_LEADER_NAME_MAX];
  char map_file[NEW_GAME_MAP_NAME_MAX]; /* basename, e.g. AMER2.MP; empty if generate */
  bool generate_map; /* NEW WORLD procedural map */
  MapGenParams gen_params;
  char data_dir[512];

  int selection;
  int option_count;
  char options[NEW_GAME_OPTION_MAX][NEW_GAME_OPTION_LABEL_LEN];
  char prompt_lines[8][COLONIZE_MSG_LINE_LEN];
  int prompt_line_count;
  int dialog_width;
  int pref_dialog_y; /* -1 = center; from @y= */

  /* Last list-dialog layout for hit-testing. */
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int list_y0;
  int line_h;

  int sail_frame; /* 0..9 */
  uint32_t sail_accum_ms;

  /* Lazy-loaded assets (freed on cancel/commit/destroy). */
  ColonizePikImage difficul_pik;
  bool difficul_ok;
  ColonizePikImage nations_pik;
  bool nations_ok;
  ColonizePikImage kinglss_pik;
  bool kinglss_ok;
  ColonizeSpriteSheet king1;
  bool king1_ok;
  ColonizeSpriteSheet nation_art; /* ENGLND1 etc. */
  bool nation_art_ok;
  ColonizeFont fontking;
  bool fontking_ok;
  ColonizePikImage levn[NEW_GAME_SAIL_FRAMES];
  bool levn_ok[NEW_GAME_SAIL_FRAMES];

  const ColonizeMsgCatalog* game_txt;
  const ColonizeMsgCatalog* names_txt;
  const ColonizeMsgCatalog* labels_txt; /* LABELS.TXT for "Click Here When Finished" */
  const ColonizeFont* ui_font; /* FONTINTR — prompts / name */
  const ColonizeFont* lore_font; /* FONTSMAL — nation lore body (larger) */
  const ColonizeSpriteSheet* wood_tile; /* OPENTILE for remaining popups (America) */
  const ColonizePikImage* woodpanl; /* WOODPANL.PIK full-screen for name/lore */

  /* Image-region pick UI (difficulty / nation): finished-button hit box. */
  int finished_x;
  int finished_y;
  int finished_w;
  int finished_h;
} NewGameWizard;

void new_game_init(NewGameWizard* ng);
void new_game_free(NewGameWizard* ng);

bool new_game_active(const NewGameWizard* ng);
bool new_game_wants_commit(const NewGameWizard* ng);

/*
 * Start wizard. path NEW_WORLD → difficulty; AMERICA → @AMERICA choice.
 * names_txt supplies @LEADERNAME defaults and @SCENARIO (optional).
 */
bool new_game_begin(
  NewGameWizard* ng,
  NewGamePath path,
  const char* data_dir,
  const ColonizeMsgCatalog* game_txt,
  const ColonizeMsgCatalog* names_txt
);

void new_game_cancel(NewGameWizard* ng);

/* Returns true if input was consumed. */
bool new_game_handle_input(NewGameWizard* ng, const ColonizeInputState* input);

void new_game_update(NewGameWizard* ng, uint32_t dt_ms);

void new_game_render(
  NewGameWizard* ng,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* out_palette,
  const ColonizePopupColors* popup_colors,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color
);

/* AMER2 @SCENARIO start tile for nation 0..3 (Dutch falls back if missing). */
bool new_game_scenario_start(
  const ColonizeMsgCatalog* names_txt,
  const char* map_stem,
  int nation,
  int* out_x,
  int* out_y
);

const char* new_game_nation_name(int nation);
const char* new_game_nation_port(int nation);
const char* new_game_nation_ruler_title(int nation); /* "King" or "Stadtholder" */

#endif
