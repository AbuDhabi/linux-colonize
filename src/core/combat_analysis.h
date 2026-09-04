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
 *
 * Layout (DOS / LABELS.TXT, FUN_636c_0000 draw pass):
 *   Frame w=0xd6 at x=0x35, height rows*0x14+pad, vertically centered.
 *   1. Centered title "COMBAT ANALYSIS"
 *   2. Two columns (atk left, def right). Row 0 per column: unit chrome +
 *      type name, baseline strength right-aligned (NAMES byte — not the
 *      post-×8 roll weight).
 *   3. Modifier rows, 0x14 pitch: label left, ±N% right-aligned.
 *      Attacker terrain line reads "Ambush", defender "Terrain";
 *      village line uses the NAMES @LEVELS noun (Camp/Village/City, or
 *      "Capital") — DOS 636c bit-8 row, never the tribe name.
 *
 * Shown after strengths are known, before the combat roll / outcome UI.
 */

#define COMBAT_ANALYSIS_LINES_MAX 12
#define COMBAT_ANALYSIS_LINE_LEN 40
#define COMBAT_ANALYSIS_VALUE_LEN 12

/* One modifier row: label left, value right-aligned in the column (DOS 013c/0150). */
typedef struct CombatAnalysisRow {
  char label[COMBAT_ANALYSIS_LINE_LEN];
  char value[COMBAT_ANALYSIS_VALUE_LEN];
} CombatAnalysisRow;

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
  /* bugs.md 267: unit-less attacker (coastal Fort/Fortress battery) — the
   * header name when attacker_id < 0. Empty for normal unit engagements. */
  char atk_label[COMBAT_ANALYSIS_LINE_LEN];
} ColonizeCombatEngagement;

typedef struct CombatAnalysisSideChrome {
  int sprite;
  int display_type;
  int nation_id;
  int orders;
  bool aboard;
} CombatAnalysisSideChrome;

typedef struct CombatAnalysisDialog {
  bool open;
  ColonizeCombatEngagement eng;
  CombatAnalysisSideChrome atk_chrome;
  CombatAnalysisSideChrome def_chrome;
  /* Header row (DOS: unit chrome + NAMES type name + baseline strength). */
  char atk_name[COMBAT_ANALYSIS_LINE_LEN];
  char def_name[COMBAT_ANALYSIS_LINE_LEN];
  CombatAnalysisRow atk_rows[COMBAT_ANALYSIS_LINES_MAX];
  CombatAnalysisRow def_rows[COMBAT_ANALYSIS_LINES_MAX];
  int atk_line_count;
  int def_line_count;
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  /* Ignore click/key dismiss until mouse released (village Attack same-click). */
  int arm_input;
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
 * Open dialog from pre-roll engagement (strengths + flags only).
 * Snapshots unit chrome so render stays valid if units later despawn.
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
  const ColonizeSpriteSheet* unit_icons,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  const ColonizePalette* active_palette,
  ColonizeFramebuffer8* framebuffer
);

/*
 * Optional presenter called from units_resolve_* after strengths, before roll.
 * When NULL, analysis is skipped (tests / AI-only).
 */
typedef void (*ColonizeCombatAnalysisPresenter)(const ColonizeCombatEngagement* eng, void* user);

void combat_analysis_set_presenter(ColonizeCombatAnalysisPresenter fn, void* user);
ColonizeCombatAnalysisPresenter combat_analysis_presenter(void);
void* combat_analysis_presenter_user(void);

/* Invoke presenter if set; no-op otherwise. */
void combat_analysis_present_if_hooked(const ColonizeCombatEngagement* eng);

#endif
