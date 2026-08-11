#ifndef COLONIZE_COMBAT_ANALYSIS_H
#define COLONIZE_COMBAT_ANALYSIS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/col1_save.h"
#include "core/combat_strength.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * Dual-column Combat Analysis dialog (FUN_636c_0000-shaped).
 * Gated by game_options.combat_analysis when a human side is involved.
 * Cite: FUN_5fef_1b0e gate 0x5383&2; FUN_2a1f_0704 → FUN_636c_0000.
 */

#define COMBAT_ANALYSIS_LINES_MAX 12
#define COMBAT_ANALYSIS_LINE_LEN 40

typedef struct ColonizeCombatEngagement {
  int attacker_id;
  int defender_id;
  int atk_strength;
  int def_strength;
  int roll;
  bool atk_wins;
  bool is_naval;
  ColonizeCombatSideFlags atk_flags;
  ColonizeCombatSideFlags def_flags;
} ColonizeCombatEngagement;

typedef struct CombatAnalysisDialog {
  bool open;
  ColonizeCombatEngagement eng;
  char atk_name[32];
  char def_name[32];
  char atk_lines[COMBAT_ANALYSIS_LINES_MAX][COMBAT_ANALYSIS_LINE_LEN];
  char def_lines[COMBAT_ANALYSIS_LINES_MAX][COMBAT_ANALYSIS_LINE_LEN];
  int atk_line_count;
  int def_line_count;
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
} CombatAnalysisDialog;

void combat_analysis_close(CombatAnalysisDialog* dlg);

/* True when option bit set and atk or def nation is human (player.control==0). */
bool combat_analysis_should_show(
  const ColonizeCol1Save* col1,
  int atk_nation,
  int def_nation,
  int human_nation
);

/*
 * Open dual-column dialog from a rolled engagement. Names from unit types.
 * Returns false if dlg/pool missing.
 */
bool combat_analysis_open(
  CombatAnalysisDialog* dlg,
  const ColonizeUnitPool* pool,
  const ColonizeCombatEngagement* eng
);

bool combat_analysis_handle_input(CombatAnalysisDialog* dlg, const ColonizeInputState* input);

void combat_analysis_render(
  CombatAnalysisDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
);

/*
 * Optional presenter called from units_resolve_* after roll, before apply.
 * When NULL, analysis is skipped (tests / AI-only).
 */
typedef void (*ColonizeCombatAnalysisPresenter)(const ColonizeCombatEngagement* eng, void* user);

void combat_analysis_set_presenter(ColonizeCombatAnalysisPresenter fn, void* user);
ColonizeCombatAnalysisPresenter combat_analysis_presenter(void);
void* combat_analysis_presenter_user(void);

/* Invoke presenter if set; no-op otherwise. */
void combat_analysis_present_if_hooked(const ColonizeCombatEngagement* eng);

#endif
