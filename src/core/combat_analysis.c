#include "core/combat_analysis.h"

#include <stdio.h>
#include <string.h>

#include "core/ai_contact.h"
#include "core/ui_colors.h"
#include "core/unit_chrome.h"

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
  (*count)++;
}

/*
 * Modifier rows only (FUN_636c_0000 flag walk, DOS check order). Labels match
 * the LABELS.TXT Combat Analysis block. land_attack_bonus: land engage ×3/2.
 * tribe_name: village defender's tribe (DOS labels the village row with the
 * tribe name from the dwelling record; there is no "Village" label).
 */
static void combat_analysis_fill_mods(
  CombatAnalysisRow* rows,
  int* count,
  const ColonizeCombatSideFlags* flags,
  int land_attack_bonus,
  bool is_attacker,
  const char* tribe_name
) {
  *count = 0;
  if (!flags) {
    return;
  }

  if (flags->flags & COMBAT_FLAG_VETERAN) {
    combat_analysis_push_row(rows, count, "Veteran", 50);
  }
  if (flags->flags & COMBAT_FLAG_HOLDS) {
    const int pct = flags->holds_occupied > 0 ? (flags->holds_occupied * 100) >> 3 : 0;
    combat_analysis_push_row(rows, count, "Cargo", -pct);
  }
  /* LABELS "Attack Bonus" — land ×3/2 (FUN_5fef_1b0e / FUN_636c bit0 walk). */
  if (land_attack_bonus) {
    combat_analysis_push_row(rows, count, "Attack Bonus", 50);
  }
  if (flags->flags & COMBAT_FLAG_REF) {
    combat_analysis_push_row(rows, count, "Expeditionary Force", 50);
  }
  if (flags->flags2 & COMBAT_FLAG_TORIES) {
    combat_analysis_push_row(rows, count, "Tories", flags->sol_percent);
  } else if (flags->flags2 & COMBAT_FLAG_REBELS) {
    combat_analysis_push_row(rows, count, "Rebels", flags->sol_percent);
  }
  /* DOS 0x2e56/0x2e58: attacker terrain line reads "Ambush", defender "Terrain". */
  if (flags->flags & COMBAT_FLAG_TERRAIN) {
    combat_analysis_push_row(
      rows, count, is_attacker ? "Ambush" : "Terrain", flags->terrain_byte * 25
    );
  }
  if (flags->flags & COMBAT_FLAG_COLONY) {
    if (flags->flags & COMBAT_FLAG_FORTRESS) {
      combat_analysis_push_row(rows, count, "Fortress", 200);
    } else if (flags->flags & COMBAT_FLAG_STOCKADE) {
      combat_analysis_push_row(rows, count, "Stockade", 100);
    } else {
      combat_analysis_push_row(rows, count, "Colony", 50);
    }
  }
  if (flags->flags & COMBAT_FLAG_VILLAGE) {
    combat_analysis_push_row(
      rows,
      count,
      (tribe_name && tribe_name[0]) ? tribe_name : "Village",
      (flags->village_n + 1) * 50
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
  }
  if (def_u && def_u->active) {
    snprintf(dlg->def_name, sizeof(dlg->def_name), "%s", units_display_name(pool, def_u));
  }

  const char* atk_tribe =
    (atk_u && atk_u->active && atk_u->nation_id >= 4) ? ai_contact_tribe_name(atk_u->nation_id)
                                                      : NULL;
  const char* def_tribe =
    (def_u && def_u->active && def_u->nation_id >= 4) ? ai_contact_tribe_name(def_u->nation_id)
                                                      : NULL;

  /* Land attacker always gets ×3/2 standing attack factor — list as Attack Bonus. */
  const int atk_bonus = !eng->is_naval && (eng->atk_flags.flags & COMBAT_FLAG_MODE_ATK);
  combat_analysis_fill_mods(
    dlg->atk_rows, &dlg->atk_line_count, &eng->atk_flags, atk_bonus, true, atk_tribe
  );
  combat_analysis_fill_mods(
    dlg->def_rows, &dlg->def_line_count, &eng->def_flags, 0, false, def_tribe
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
        need = font_text_width(font, rows_arr[i].label) + 3 +
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
      const int ry = y0 + i * row_pitch + text_dy;
      popup_draw_text_shadowed(font, framebuffer, col_x, ry, row->label, text_color);
      const int lw = font_text_width(font, row->label);
      const int vw = font_text_width(font, row->value);
      /* Right-align value at the column edge; a long label pushes it right
       * instead of being overdrawn. */
      int vx = col_right - vw;
      if (vx < col_x + lw + 3) {
        vx = col_x + lw + 3;
      }
      popup_draw_text_shadowed(font, framebuffer, vx, ry, row->value, text_color);
    }
  }
}
