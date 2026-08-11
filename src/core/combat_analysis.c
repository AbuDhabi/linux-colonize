#include "core/combat_analysis.h"

#include <stdio.h>
#include <string.h>

#include "core/ui_colors.h"

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
  dlg->atk_line_count = 0;
  dlg->def_line_count = 0;
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

static void combat_analysis_push_line(
  char lines[][COMBAT_ANALYSIS_LINE_LEN],
  int* count,
  const char* text
) {
  if (!lines || !count || !text || *count >= COMBAT_ANALYSIS_LINES_MAX) {
    return;
  }
  snprintf(lines[*count], COMBAT_ANALYSIS_LINE_LEN, "%s", text);
  (*count)++;
}

static void combat_analysis_fill_side(
  char lines[][COMBAT_ANALYSIS_LINE_LEN],
  int* count,
  const char* unit_name,
  int strength,
  const ColonizeCombatSideFlags* flags,
  int is_attacker
) {
  char buf[COMBAT_ANALYSIS_LINE_LEN];
  *count = 0;
  snprintf(buf, sizeof(buf), "%s", unit_name && unit_name[0] ? unit_name : "Unit");
  combat_analysis_push_line(lines, count, buf);

  if (flags) {
    snprintf(buf, sizeof(buf), "Combat %d", flags->base_combat);
    combat_analysis_push_line(lines, count, buf);

    if (flags->flags & COMBAT_FLAG_VETERAN) {
      combat_analysis_push_line(lines, count, "Veteran +50%");
    }
    if (flags->flags_hi & COMBAT_FLAG_DRAKE) {
      combat_analysis_push_line(lines, count, "Drake +50%");
    }
    if (flags->flags & COMBAT_FLAG_HOLDS) {
      const int pct = flags->holds_occupied > 0 ? (flags->holds_occupied * 100) >> 3 : 0;
      snprintf(buf, sizeof(buf), "Cargo -%d%%", pct);
      combat_analysis_push_line(lines, count, buf);
    }
    if (flags->flags & COMBAT_FLAG_TERRAIN) {
      snprintf(buf, sizeof(buf), "Terrain +%d%%", flags->terrain_byte * 25);
      combat_analysis_push_line(lines, count, buf);
    }
    if (flags->flags & COMBAT_FLAG_VILLAGE) {
      snprintf(buf, sizeof(buf), "Village +%d%%", (flags->village_n + 1) * 50);
      combat_analysis_push_line(lines, count, buf);
    }
    if (flags->flags & COMBAT_FLAG_COLONY) {
      int pct = 50; /* bare colony ×1.5 */
      if (flags->flags & COMBAT_FLAG_FORTRESS) {
        pct = 200;
      } else if (flags->flags & COMBAT_FLAG_STOCKADE) {
        pct = 100;
      }
      if (flags->flags & COMBAT_FLAG_FORTRESS) {
        combat_analysis_push_line(lines, count, "Fortress +200%");
      } else if (flags->flags & COMBAT_FLAG_STOCKADE) {
        snprintf(buf, sizeof(buf), "Stockade +%d%%", pct);
        combat_analysis_push_line(lines, count, buf);
      } else {
        combat_analysis_push_line(lines, count, "Colony +50%");
      }
    }
    if (flags->flags & COMBAT_FLAG_FORTIFY) {
      combat_analysis_push_line(lines, count, "Fortified +50%");
    }
    if (flags->flags & COMBAT_FLAG_ARTILLERY) {
      combat_analysis_push_line(lines, count, "Artillery -75%");
    }
    if (flags->flags2 & COMBAT_FLAG_ARTY_COLONY) {
      combat_analysis_push_line(lines, count, "Artillery vs natives +100%");
    }
    if (flags->flags & COMBAT_FLAG_AMBUSH) {
      combat_analysis_push_line(lines, count, "Ambush +50%");
    }
    if (flags->flags & COMBAT_FLAG_REF) {
      combat_analysis_push_line(lines, count, "REF +50%");
    }
    if (flags->flags2 & COMBAT_FLAG_SOL) {
      snprintf(buf, sizeof(buf), "SoL +%d%%", flags->sol_percent);
      combat_analysis_push_line(lines, count, buf);
    }
    if ((flags->flags & COMBAT_FLAG_MODE_ATK) && is_attacker) {
      /* Mode bit shown only as identity; no extra % line beyond base. */
    }
  }

  snprintf(buf, sizeof(buf), "Strength %d", strength);
  combat_analysis_push_line(lines, count, buf);
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

  const ColonizeUnit* atk = units_get_const(pool, eng->attacker_id);
  const ColonizeUnit* def = units_get_const(pool, eng->defender_id);
  const ColonizeUnitType* at = atk ? units_type(pool, atk->type_index) : NULL;
  const ColonizeUnitType* dt = def ? units_type(pool, def->type_index) : NULL;
  snprintf(dlg->atk_name, sizeof(dlg->atk_name), "%s", at && at->name[0] ? at->name : "Attacker");
  snprintf(dlg->def_name, sizeof(dlg->def_name), "%s", dt && dt->name[0] ? dt->name : "Defender");

  combat_analysis_fill_side(
    dlg->atk_lines, &dlg->atk_line_count, dlg->atk_name, eng->atk_strength, &eng->atk_flags, 1
  );
  combat_analysis_fill_side(
    dlg->def_lines, &dlg->def_line_count, dlg->def_name, eng->def_strength, &eng->def_flags, 0
  );

  {
    char buf[COMBAT_ANALYSIS_LINE_LEN];
    snprintf(buf, sizeof(buf), "Roll %d", eng->roll);
    combat_analysis_push_line(dlg->atk_lines, &dlg->atk_line_count, buf);
    snprintf(buf, sizeof(buf), "%s", eng->atk_wins ? "Victory" : "Defeat");
    combat_analysis_push_line(dlg->atk_lines, &dlg->atk_line_count, buf);
  }
  return true;
}

bool combat_analysis_handle_input(CombatAnalysisDialog* dlg, const ColonizeInputState* input) {
  if (!dlg || !dlg->open || !input) {
    return false;
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

void combat_analysis_render(
  CombatAnalysisDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
) {
  (void)select_color;
  if (!dlg || !dlg->open || !framebuffer) {
    return;
  }

  const int line_h = font ? (font->max_height > 0 ? font->max_height + 2 : 8) : 8;
  const int rows =
    dlg->atk_line_count > dlg->def_line_count ? dlg->atk_line_count : dlg->def_line_count;
  const int title_h = line_h + 4;
  const int body_h = rows * line_h + 8;
  const int w = 220;
  const int h = title_h + body_h + 12;
  const int x = (320 - w) / 2;
  const int y = (200 - h) / 2;
  dlg->dialog_x = x;
  dlg->dialog_y = y;
  dlg->dialog_w = w;
  dlg->dialog_h = h;

  int ix = 0, iy = 0, iw = 0, ih = 0;
  popup_draw(framebuffer, x, y, w, h, wood_tile, colors, &ix, &iy, &iw, &ih);

  if (font) {
    popup_draw_text_shadowed(font, framebuffer, ix + 4, iy + 2, "Combat Analysis", text_color);
    const int col_w = iw / 2;
    const int y0 = iy + title_h;
    for (int i = 0; i < dlg->atk_line_count; ++i) {
      popup_draw_text_shadowed(
        font, framebuffer, ix + 4, y0 + i * line_h, dlg->atk_lines[i], text_color
      );
    }
    for (int i = 0; i < dlg->def_line_count; ++i) {
      popup_draw_text_shadowed(
        font, framebuffer, ix + col_w + 2, y0 + i * line_h, dlg->def_lines[i], text_color
      );
    }
  }
}
