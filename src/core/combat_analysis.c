#include "core/combat_analysis.h"

#include <stdio.h>
#include <string.h>

#include "core/colony.h"
#include "core/ui_colors.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"

static ColonizeCombatAnalysisPresenter g_combat_analysis_presenter = NULL;
static void* g_combat_analysis_presenter_user = NULL;

void combat_analysis_set_presenter(ColonizeCombatAnalysisPresenter fn, void* user) {
  g_combat_analysis_presenter = fn;
  g_combat_analysis_presenter_user = user;
}

ColonizeCombatAnalysisPresenter combat_analysis_presenter(void) {
  return g_combat_analysis_presenter;
}

void* combat_analysis_presenter_user(void) {
  return g_combat_analysis_presenter_user;
}

void combat_analysis_present_if_hooked(const ColonizeCombatEngagement* eng) {
  if (g_combat_analysis_presenter && eng) {
    g_combat_analysis_presenter(eng, g_combat_analysis_presenter_user);
  }
}

void combat_analysis_close(CombatAnalysisDialog* dlg) {
  if (!dlg) {
    return;
  }
  dlg->open = false;
  dlg->arm_input = 0;
  dlg->atk_line_count = 0;
  dlg->def_line_count = 0;
  memset(&dlg->atk_chrome, 0, sizeof(dlg->atk_chrome));
  memset(&dlg->def_chrome, 0, sizeof(dlg->def_chrome));
  dlg->atk_chrome.sprite = -1;
  dlg->def_chrome.sprite = -1;
}

bool combat_analysis_should_show(
  const ColonizeCol1Save* col1,
  int atk_nation,
  int def_nation,
  int human_nation
) {
  if (!col1 || !col1->head.game_options.combat_analysis) {
    return false;
  }
  /* Human side: player.control==0, or matches human_nation. */
  const int atk_human =
    (atk_nation >= 0 && atk_nation <= 3 && col1->player[atk_nation].control == 0) ||
    (human_nation >= 0 && atk_nation == human_nation);
  const int def_human =
    (def_nation >= 0 && def_nation <= 3 && col1->player[def_nation].control == 0) ||
    (human_nation >= 0 && def_nation == human_nation);
  return atk_human || def_human;
}

static void combat_analysis_push_row(
  CombatAnalysisRow* rows,
  int* count,
  const char* label,
  int signed_pct
) {
  if (!rows || !count || !label || *count >= COMBAT_ANALYSIS_LINES_MAX) {
    return;
  }
  CombatAnalysisRow* row = &rows[*count];
  snprintf(row->label, sizeof(row->label), "%s", label);
  snprintf(
    row->value, sizeof(row->value), "%+d%%", signed_pct
  );
  row->icon_kind = COMBAT_ROW_ICON_NONE;
  row->icon_sprite = -1;
  row->icon_nation = -1;
  row->label_indent = 0;
  (*count)++;
}

/*
 * Same row, plus the picture DOS blits at the column's left edge before the
 * label (FUN_636c_0000 draws the icon at the row's top-left and then bumps
 * local_76 — its label pen — by `indent`).
 */
static void combat_analysis_push_row_icon(
  CombatAnalysisRow* rows,
  int* count,
  const char* label,
  int signed_pct,
  int icon_kind,
  int icon_sprite,
  int icon_nation,
  int indent
) {
  const int at = count ? *count : 0;
  combat_analysis_push_row(rows, count, label, signed_pct);
  if (!count || *count <= at) {
    return; /* row list full */
  }
  CombatAnalysisRow* row = &rows[at];
  if (icon_sprite < 0) {
    return; /* row still prints, just without chrome */
  }
  row->icon_kind = icon_kind;
  row->icon_sprite = icon_sprite;
  row->icon_nation = icon_nation;
  row->label_indent = indent;
}

/*
 * Modifier rows only (FUN_636c_0000 flag walk, DOS check order). Labels match
 * the LABELS.TXT Combat Analysis block. land_attack_bonus: land engage ×3/2.
 */
static void combat_analysis_fill_mods(
  CombatAnalysisRow* rows,
  int* count,
  const ColonizeCombatSideFlags* flags,
  int land_attack_bonus,
  bool is_attacker
) {
  *count = 0;
  if (!flags) {
    return;
  }

  /*
   * DOS 636c bit 0x400 (asm 636c:01d4-0288) is the FIRST modifier row, ahead
   * of Veteran: FUN_281f_0254 blits ICONS.SS sprite 0x26 (the literal AX the
   * asm loads — DOS's 1-based icon space, so 37 here, the @CARGO 15 Muskets
   * icon, ICONS.SS #22-37 = @CARGO0-15), bumps the label pen by 8, then
   * labels the row from the pointer at DS:0x97de — the same global the Indian
   * Adviser (FUN_3f41_010a, asm 3f41:...ff36de97) prints its Muskets tally
   * with, i.e. NAMES.TXT @CARGO 15 "Muskets".
   *
   * The value is 0146 (sign) + 0182 (number 1) and, uniquely on this row, NO
   * 010a percent suffix — so DOS prints a bare "+1", an additive point of
   * base combat, not a percentage.
   *
   * Trigger (DOS FUN_5fef_1b0e, asm 5fef:1d2f-1d5f): the defender of an
   * undefended colony is auto-spawned; if the colony's nation has Founding
   * Father 12 (Paul Revere, FUN_281f_07b4 → FUN_15eb_3960(nation, 0xc)) and
   * the colony's Muskets stock (colony record +0xb8 = stock[15]) is >= 0x32
   * (50), DOS swaps the defender graphic to 0x4b, does INC on the base combat
   * byte and sets 0x8d03 bit 2 = flags bit 0x400. The port models Revere by
   * ejecting a real Soldier instead (founding_fathers_revere_auto_arm), so
   * nothing sets this bit yet — see docs/combat.md, trigger left open.
   */
  if (flags->flags & COMBAT_FLAG_MUSKETS) {
    combat_analysis_push_row_icon(
      rows, count, "Muskets", 1, COMBAT_ROW_ICON_UNIT, COMBAT_ANALYSIS_MUSKETS_ICON, -1, 8
    );
    if (*count > 0) {
      /* Bare "+1": this row has no FUN_281f_010a, so no trailing '%'. */
      snprintf(rows[*count - 1].value, sizeof(rows[*count - 1].value), "%+d", 1);
    }
  }
  if (flags->flags & COMBAT_FLAG_VETERAN) {
    combat_analysis_push_row(rows, count, "Veteran", 50);
  }
  if (flags->flags & COMBAT_FLAG_HOLDS) {
    const int pct = flags->holds_occupied > 0 ? (flags->holds_occupied * 100) >> 3 : 0;
    combat_analysis_push_row(rows, count, "Cargo", -pct);
  }
  /*
   * DOS 636c fatigue rows — bit 0x100 (asm 636c:02ff, value 0x21 = 33) and
   * a156 bit 3 (asm 636c:03da, value 0x42 = 66), both labelled DS:0x2e52
   * ("Fatigue", LABELS.TXT line 91). Both use FUN_281f_015a (the minus
   * formatter, same one Cargo and Artillery In Open take) rather than 0146.
   * They sit between Cargo and Attack Bonus in DOS's bit walk, not at the end
   * of the column, and DOS tests them as two independent ifs. Live since the
   * strength calc models the @HALF penalty: 2 thirds left = x2/3, 1 = x1/3.
   * Attacker side only — a defender is never charged for being tired.
   */
  if (flags->flags & COMBAT_FLAG_FATIGUE_33) {
    combat_analysis_push_row(rows, count, "Fatigue", -33);
  }
  if (flags->flags2 & COMBAT_FLAG_FATIGUE_66) {
    combat_analysis_push_row(rows, count, "Fatigue", -66);
  }
  /* LABELS "Attack Bonus" — land ×3/2 (FUN_5fef_1b0e / FUN_636c bit0 walk). */
  if (land_attack_bonus) {
    combat_analysis_push_row(rows, count, "Attack Bonus", 50);
  }
  /* bugs.md 248: DOS 636c bit 0x8000 label ptr DS:0x2e8a = @MISC 104
   * "Bombard" (the WoI colony-attack support bonus), not "Expeditionary
   * Force" (@MISC 91, the Congress force row). */
  if (flags->flags & COMBAT_FLAG_REF) {
    combat_analysis_push_row_icon(
      rows, count, "Bombard", 50, COMBAT_ROW_ICON_UNIT, flags->bombard_icon, -1, 0x10
    );
  }
  /*
   * WoI popular-support rows. DOS 636c reads DS:0x2ec2 (a156 bit 1, asm
   * 636c:0f56) and DS:0x2ec4 (a156 bit 2, asm 636c:0ff0); by the LABELS
   * pointer-table base 0x2d9c (addr = 0x2d9c + 2*line, anchored on 0x2e52 =
   * line 91 "Fatigue" and 0x2e8a = line 119 "Bombard") those are LABELS.TXT
   * lines 147 / 148 = "Tory Unrest" / "Rebel Unrest". The port used to print
   * "Tories" / "Rebels" (lines 101 / 102), which 636c never reads.
   */
  if (flags->flags2 & COMBAT_FLAG_TORIES) {
    combat_analysis_push_row(rows, count, "Tory Unrest", flags->sol_percent);
  } else if (flags->flags2 & COMBAT_FLAG_REBELS) {
    combat_analysis_push_row(rows, count, "Rebel Unrest", flags->sol_percent);
  }
  /*
   * DOS 0x2e56/0x2e58: attacker terrain line reads "Ambush", defender
   * "Terrain". 636c blits the engagement tile itself in front of the label
   * (FUN_281f_033a → FUN_1baa_0006, asm 636c:08c4-0921) and indents by 0x11.
   */
  if (flags->flags & COMBAT_FLAG_TERRAIN) {
    combat_analysis_push_row_icon(
      rows,
      count,
      is_attacker ? "Ambush" : "Terrain",
      flags->terrain_byte * 25,
      COMBAT_ROW_ICON_TERRAIN,
      flags->terrain_sprite,
      -1,
      0x11
    );
  }
  /*
   * DOS 636c colony row (bit 0x40, asm 636c:09db-0a05): the settlement marker
   * is blitted first (FUN_281f_02a8 → 112b_0c64 at scale 100, indent 0x14),
   * then the label is the topmost built fortification's own name
   * (FUN_281f_0bdc = FUN_15eb_0434(0) walking the Stockade→Fort→Fortress
   * chain through DS:0x8f82+4), or LABELS "Colony" (DS:0x2e5a) when the colony
   * has none; the value is (FUN_157e_0008 + 1) * 50 — so Fort finally prints
   * its own +150% tier instead of collapsing into Stockade's +100%.
   */
  if (flags->flags & COMBAT_FLAG_COLONY) {
    static const char* k_fort_tier_names[4] = {"Colony", "Stockade", "Fort", "Fortress"};
    int tier = flags->fort_tier;
    if (tier < 0) {
      tier = 0;
    }
    if (tier > 3) {
      tier = 3;
    }
    combat_analysis_push_row_icon(
      rows,
      count,
      k_fort_tier_names[tier],
      (tier + 1) * 50,
      COMBAT_ROW_ICON_SETTLEMENT,
      flags->colony_icon,
      flags->colony_nation,
      0x14
    );
  }
  /*
   * DOS 636c village row (bit 8): label = NAMES @LEVELS noun by tribe tech
   * (Camp / Village / City), overridden to "Capital" (@LEVELS row 5) when the
   * dwelling is a capital; pct = 50 ×2 for tech>1 (bit 0x10) ×2 for capital
   * (bit 0x20). It never prints the tribe name (bugs: "Aztec +150%").
   */
  if (flags->flags & COMBAT_FLAG_VILLAGE) {
    const bool capital = (flags->flags2 & COMBAT_FLAG_VILLAGE_CAPITAL) != 0;
    const char* label = capital ? "Capital"
      : (flags->village_n > 1 ? "City" : (flags->village_n == 1 ? "Village" : "Camp"));
    int pct = flags->village_n > 1 ? 100 : 50;
    if (capital) {
      pct *= 2;
    }
    /*
     * 636c blits the dwelling first (FUN_281f_02b2 → 112b_0790 at scale 100,
     * asm 636c:0add-0b03, indent 0x14). 112b_0790 reads the marker id out of
     * the DS:0x84c per-tech table, which is the same ICONS.SS #10-13 run the
     * map draws (map_panel.c MAP_PANEL_TRIBE_ICON_BASE).
     */
    const int tech = flags->village_n < 0 ? 0 : (flags->village_n > 3 ? 3 : flags->village_n);
    combat_analysis_push_row_icon(
      rows, count, label, pct, COMBAT_ROW_ICON_VILLAGE, 10 + tech, -1, 0x14
    );
  }
  if (flags->flags & COMBAT_FLAG_ARTILLERY) {
    combat_analysis_push_row(rows, count, "Artillery In Open", -75);
  }
  if (flags->flags2 & COMBAT_FLAG_ARTY_COLONY) {
    combat_analysis_push_row(rows, count, "Artillery Vs. Raid", 100);
  }
  if (flags->flags & COMBAT_FLAG_FORTIFY) {
    combat_analysis_push_row(rows, count, "Fortified", 50);
  }
  if (flags->flags & COMBAT_FLAG_AMBUSH) {
    combat_analysis_push_row(rows, count, "Spain Bonus", 50);
  }
  if (flags->flags_hi & COMBAT_FLAG_DRAKE) {
    combat_analysis_push_row(rows, count, "Drake", 50);
  }
}

/* "Veteran +50% Fortified +50%" for one side's rows. */
static void combat_analysis_join_rows(
  char* out,
  size_t out_size,
  const CombatAnalysisRow* rows,
  int count
) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  size_t at = 0;
  for (int i = 0; i < count && i < COMBAT_ANALYSIS_LINES_MAX; ++i) {
    const int n = snprintf(
      out + at, out_size - at, "%s%s %s", i ? " " : "", rows[i].label, rows[i].value
    );
    if (n <= 0 || (size_t)n >= out_size - at) {
      break;
    }
    at += (size_t)n;
  }
  if (count <= 0) {
    snprintf(out, out_size, "none");
  }
}

void combat_analysis_log_engagement(
  const ColonizeUnitPool* pool,
  const ColonizeCombatEngagement* eng,
  bool resolved
) {
  if (!diag_info_enabled() || !eng) {
    return;
  }
  const ColonizeUnit* atk_u = pool ? units_get_const(pool, eng->attacker_id) : NULL;
  const ColonizeUnit* def_u = pool ? units_get_const(pool, eng->defender_id) : NULL;
  char atk_name[COMBAT_ANALYSIS_LINE_LEN];
  char def_name[COMBAT_ANALYSIS_LINE_LEN];
  snprintf(
    atk_name, sizeof(atk_name), "%s",
    (atk_u && atk_u->active) ? units_display_name(pool, atk_u)
                             : (eng->atk_label[0] ? eng->atk_label : "?")
  );
  snprintf(
    def_name, sizeof(def_name), "%s",
    (def_u && def_u->active) ? units_display_name(pool, def_u) : "?"
  );

  CombatAnalysisRow atk_rows[COMBAT_ANALYSIS_LINES_MAX];
  CombatAnalysisRow def_rows[COMBAT_ANALYSIS_LINES_MAX];
  int atk_count = 0;
  int def_count = 0;
  const int atk_bonus = !eng->is_naval && (eng->atk_flags.flags & COMBAT_FLAG_MODE_ATK);
  combat_analysis_fill_mods(atk_rows, &atk_count, &eng->atk_flags, atk_bonus, true);
  combat_analysis_fill_mods(def_rows, &def_count, &eng->def_flags, 0, false);
  char atk_mods[256];
  char def_mods[256];
  combat_analysis_join_rows(atk_mods, sizeof(atk_mods), atk_rows, atk_count);
  combat_analysis_join_rows(def_mods, sizeof(def_mods), def_rows, def_count);

  const int total = eng->atk_strength + eng->def_strength;
  diag_info(
    "COMBAT %s attacker=%s (nation %d, id %d) defender=%s (nation %d, id %d) at (%d,%d)",
    eng->is_naval ? "naval" : "land",
    atk_name,
    atk_u ? atk_u->nation_id : -1,
    eng->attacker_id,
    def_name,
    def_u ? def_u->nation_id : -1,
    eng->defender_id,
    def_u ? def_u->x : -1,
    def_u ? def_u->y : -1
  );
  diag_info(
    "COMBAT   attacker base=%d strength=%d mods: %s",
    eng->atk_flags.base_combat, eng->atk_strength, atk_mods
  );
  diag_info(
    "COMBAT   defender base=%d strength=%d mods: %s",
    eng->def_flags.base_combat, eng->def_strength, def_mods
  );
  if (resolved) {
    diag_info(
      "COMBAT   odds %d/%d (%d%%) roll=%d -> %s wins",
      eng->atk_strength,
      total,
      total > 0 ? (eng->atk_strength * 100) / total : 100,
      eng->roll,
      eng->atk_wins ? "attacker" : "defender"
    );
  }
}

static void combat_analysis_snap_chrome(
  CombatAnalysisSideChrome* chrome,
  const ColonizeUnitPool* pool,
  int unit_id
) {
  memset(chrome, 0, sizeof(*chrome));
  chrome->sprite = -1;
  chrome->display_type = -1;
  chrome->nation_id = -1;
  if (!pool || unit_id < 0) {
    return;
  }
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active) {
    return;
  }
  chrome->sprite = units_map_sprite(pool, unit_id);
  chrome->display_type = units_display_type_index(pool, unit_id);
  chrome->nation_id = u->nation_id;
  chrome->orders = u->orders;
  chrome->aboard = u->aboard_ship_id >= 0;
}

bool combat_analysis_open(
  CombatAnalysisDialog* dlg,
  const ColonizeUnitPool* pool,
  const ColonizeCombatEngagement* eng
) {
  if (!dlg || !pool || !eng) {
    return false;
  }
  dlg->open = true;
  dlg->eng = *eng;
  /* Pre-roll: ignore any roll/victor the caller may have left set. */
  dlg->eng.roll = 0;
  dlg->eng.atk_wins = false;
  /* Village Attack CHOICE click must not dismiss analysis on the same press. */
  dlg->arm_input = 0;

  combat_analysis_snap_chrome(&dlg->atk_chrome, pool, eng->attacker_id);
  combat_analysis_snap_chrome(&dlg->def_chrome, pool, eng->defender_id);

  /* Header names (DOS NAMES type string via 0x5230 table). */
  dlg->atk_name[0] = '\0';
  dlg->def_name[0] = '\0';
  const ColonizeUnit* atk_u = units_get_const(pool, eng->attacker_id);
  const ColonizeUnit* def_u = units_get_const(pool, eng->defender_id);
  if (atk_u && atk_u->active) {
    snprintf(dlg->atk_name, sizeof(dlg->atk_name), "%s", units_display_name(pool, atk_u));
  } else if (eng->atk_label[0]) {
    /* bugs.md 267: unit-less attacker (coastal Fort/Fortress battery). */
    snprintf(dlg->atk_name, sizeof(dlg->atk_name), "%s", eng->atk_label);
  }
  if (def_u && def_u->active) {
    snprintf(dlg->def_name, sizeof(dlg->def_name), "%s", units_display_name(pool, def_u));
  }

  /* Land attacker always gets ×3/2 standing attack factor — list as Attack Bonus. */
  const int atk_bonus = !eng->is_naval && (eng->atk_flags.flags & COMBAT_FLAG_MODE_ATK);
  combat_analysis_fill_mods(
    dlg->atk_rows, &dlg->atk_line_count, &eng->atk_flags, atk_bonus, true
  );
  combat_analysis_fill_mods(
    dlg->def_rows, &dlg->def_line_count, &eng->def_flags, 0, false
  );
  return true;
}

bool combat_analysis_handle_input(CombatAnalysisDialog* dlg, const ColonizeInputState* input) {
  if (!dlg || !dlg->open || !input) {
    return false;
  }
  /*
   * Arm after mouse buttons are up and no edge click/key this frame — so the
   * Attack CHOICE click that opened village combat cannot dismiss analysis.
   */
  if (!dlg->arm_input) {
    if (!input->mouse_left_down && !input->mouse_right_down && !input->mouse_left_clicked &&
        !input->mouse_right_clicked && input->last_key == 0) {
      dlg->arm_input = 1;
    }
    return true;
  }
  if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_ENTER ||
      input->last_key == COLONIZE_KEY_SPACE) {
    combat_analysis_close(dlg);
    return true;
  }
  if (input->mouse_left_clicked || input->mouse_right_clicked) {
    combat_analysis_close(dlg);
    return true;
  }
  return true; /* consume while open */
}

static void combat_analysis_blit_side(
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* icons,
  const CombatAnalysisSideChrome* chrome,
  int x,
  int y,
  const ColonizePalette* active_palette
) {
  if (!fb || !chrome || !icons || chrome->sprite < 0 || chrome->sprite >= icons->sprite_count) {
    return;
  }
  unit_chrome_blit_unit_for_palette(
    fb,
    font,
    icons,
    chrome->sprite,
    x,
    y,
    chrome->display_type,
    chrome->nation_id,
    chrome->orders,
    false,
    chrome->aboard,
    active_palette
  );
}

/*
 * One flag row's picture (FUN_636c_0000). DOS reaches four different blitters
 * from the same row loop; the port routes them by CombatAnalysisRowIcon:
 *   UNIT       FUN_281f_0254 → FUN_1c36_000a, a plain ICONS.SS blit (sprite
 *              index in AX, x in DX, y pushed) — the Bombard row.
 *   SETTLEMENT FUN_281f_02a8 → FUN_112b_0c64 at scale 100 — ICONS.SS #0-3
 *              plus the owner's flag pixels, which is colonies_blit_settlement_icon.
 *   VILLAGE    FUN_281f_02b2 → FUN_112b_0790 at scale 100 — ICONS.SS #10-13.
 *   TERRAIN    FUN_281f_033a → FUN_1baa_0006 — the engagement tile from
 *              TERRAIN.SS. Silently skipped when the sheet is not loaded.
 */
static void combat_analysis_blit_row_icon(
  ColonizeFramebuffer8* fb,
  const ColonizeSpriteSheet* icons,
  const ColonizeSpriteSheet* terrain,
  const CombatAnalysisRow* row,
  int x,
  int y,
  const ColonizePalette* active_palette
) {
  if (!fb || !row || row->icon_sprite < 0) {
    return;
  }
  const ColonizeSpriteSheet* sheet =
    row->icon_kind == COMBAT_ROW_ICON_TERRAIN ? terrain : icons;
  if (!sheet || row->icon_sprite >= sheet->sprite_count) {
    return;
  }
  if (row->icon_kind == COMBAT_ROW_ICON_SETTLEMENT) {
    colonies_blit_settlement_icon(
      sheet, row->icon_sprite, fb, x, y, row->icon_nation, active_palette
    );
    return;
  }
  ss_blit_sprite(sheet, row->icon_sprite, fb, x, y);
}

void combat_analysis_render(
  CombatAnalysisDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizeSpriteSheet* unit_icons,
  const ColonizeSpriteSheet* terrain,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  const ColonizePalette* active_palette,
  ColonizeFramebuffer8* framebuffer
) {
  (void)select_color;
  if (!dlg || !dlg->open || !framebuffer) {
    return;
  }

  const int line_h = font ? (font->max_height > 0 ? font->max_height + 2 : 8) : 8;
  /* DOS FUN_636c_0000 draw pass: w=0xd6 at x=0x35, row pitch 0x14, height by
   * tallest column (header row + mods), vertically centered. */
  const int row_pitch = 20;
  const int mod_rows =
    dlg->atk_line_count > dlg->def_line_count ? dlg->atk_line_count : dlg->def_line_count;
  const int rows = 1 + mod_rows; /* header (name + baseline) counts as a row */
  const int title_h = line_h + 6;
  /* DOS width 0xd6; grow only when a label+value row cannot fit its half
   * column in this font (DOS overdraws instead — we widen). */
  int col_w = (214 - POPUP_FRAME_INSET * 2) / 2 - 7;
  if (font) {
    for (int side = 0; side < 2; ++side) {
      const CombatAnalysisRow* rows_arr = side == 0 ? dlg->atk_rows : dlg->def_rows;
      const int count = side == 0 ? dlg->atk_line_count : dlg->def_line_count;
      const char* name = side == 0 ? dlg->atk_name : dlg->def_name;
      const int base =
        side == 0 ? dlg->eng.atk_flags.base_combat : dlg->eng.def_flags.base_combat;
      char num[16];
      snprintf(num, sizeof(num), "%d", base);
      int need = 16 + UNIT_CHROME_SPRITE_DX + 3 + font_text_width(font, name) + 6 +
        font_text_width(font, num);
      if (need > col_w) {
        col_w = need;
      }
      for (int i = 0; i < count; ++i) {
        need = rows_arr[i].label_indent + font_text_width(font, rows_arr[i].label) + 3 +
          font_text_width(font, rows_arr[i].value);
        if (need > col_w) {
          col_w = need;
        }
      }
    }
  }
  int w = 2 * (col_w + 7) + POPUP_FRAME_INSET * 2;
  if (w > 312) {
    w = 312;
  }
  const int h = title_h + rows * row_pitch + 12;
  const int x = (320 - w) / 2;
  const int y = (200 - h) / 2;
  dlg->dialog_x = x;
  dlg->dialog_y = y;
  dlg->dialog_w = w;
  dlg->dialog_h = h;

  int ix = 0, iy = 0, iw = 0, ih = 0;
  popup_draw(framebuffer, x, y, w, h, wood_tile, colors, &ix, &iy, &iw, &ih);

  if (!font) {
    return;
  }

  const char* title = "COMBAT ANALYSIS";
  const int tw = font_text_width(font, title);
  popup_draw_text_shadowed(
    font, framebuffer, ix + (iw - tw) / 2, iy + 3, title, text_color
  );

  /* Columns split the interior in half; values right-align at column edge. */
  const int atk_x = ix + 2;
  const int atk_right = ix + iw / 2 - 5;
  const int def_x = ix + iw / 2 + 3;
  const int def_right = ix + iw - 4;
  const int y_hdr = iy + title_h;
  const int icon_h = 16;
  const int icon_w = 16;
  const int text_dy = (row_pitch - line_h) / 2 > 0 ? (row_pitch - line_h) / 2 : 0;
  char str_buf[16];

  /* Header row: unit chrome, type name at +17, baseline strength at right
   * (NAMES byte via 0x8d06 / -0x72fa — not the post-×8 roll weight). */
  combat_analysis_blit_side(
    framebuffer, font, unit_icons, &dlg->atk_chrome, atk_x, y_hdr, active_palette
  );
  combat_analysis_blit_side(
    framebuffer, font, unit_icons, &dlg->def_chrome, def_x, y_hdr, active_palette
  );
  {
    const int name_dy = (icon_h - line_h) / 2 + 1;
    const int atk_name_x = atk_x + icon_w + UNIT_CHROME_SPRITE_DX + 3;
    const int def_name_x = def_x + icon_w + UNIT_CHROME_SPRITE_DX + 3;
    snprintf(str_buf, sizeof(str_buf), "%d", dlg->eng.atk_flags.base_combat);
    {
      const int sw = font_text_width(font, str_buf);
      const int name_w = font_text_width(font, dlg->atk_name);
      const int room = atk_right - sw - 3 - atk_name_x;
      if (dlg->atk_name[0] && name_w <= room) {
        popup_draw_text_shadowed(
          font, framebuffer, atk_name_x, y_hdr + name_dy, dlg->atk_name, text_color
        );
      }
      popup_draw_text_shadowed(
        font, framebuffer, atk_right - sw, y_hdr + name_dy, str_buf, text_color
      );
    }
    snprintf(str_buf, sizeof(str_buf), "%d", dlg->eng.def_flags.base_combat);
    {
      const int sw = font_text_width(font, str_buf);
      const int name_w = font_text_width(font, dlg->def_name);
      const int room = def_right - sw - 3 - def_name_x;
      if (dlg->def_name[0] && name_w <= room) {
        popup_draw_text_shadowed(
          font, framebuffer, def_name_x, y_hdr + name_dy, dlg->def_name, text_color
        );
      }
      popup_draw_text_shadowed(
        font, framebuffer, def_right - sw, y_hdr + name_dy, str_buf, text_color
      );
    }
  }

  const int y0 = y_hdr + row_pitch;
  for (int side = 0; side < 2; ++side) {
    const CombatAnalysisRow* rows_arr = side == 0 ? dlg->atk_rows : dlg->def_rows;
    const int count = side == 0 ? dlg->atk_line_count : dlg->def_line_count;
    const int col_x = side == 0 ? atk_x : def_x;
    const int col_right = side == 0 ? atk_right : def_right;
    for (int i = 0; i < count; ++i) {
      const CombatAnalysisRow* row = &rows_arr[i];
      const int row_top = y0 + i * row_pitch;
      const int ry = row_top + text_dy;
      /* DOS blits the row's picture at the column's left edge on the row top
       * (local_76 / local_10), then draws the label local_76 + indent along. */
      combat_analysis_blit_row_icon(
        framebuffer, unit_icons, terrain, row, col_x, row_top, active_palette
      );
      const int label_x = col_x + row->label_indent;
      popup_draw_text_shadowed(font, framebuffer, label_x, ry, row->label, text_color);
      const int lw = font_text_width(font, row->label);
      const int vw = font_text_width(font, row->value);
      /* Right-align value at the column edge; a long label pushes it right
       * instead of being overdrawn. */
      int vx = col_right - vw;
      if (vx < label_x + lw + 3) {
        vx = label_x + lw + 3;
      }
      popup_draw_text_shadowed(font, framebuffer, vx, ry, row->value, text_color);
    }
  }
}
