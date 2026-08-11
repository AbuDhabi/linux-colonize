#include "core/combat_analysis.h"

#include <stdio.h>
#include <string.h>

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

/*
 * Modifier lines only (FUN_636c_0000 flag walk). Labels match LABELS.TXT @MISC
 * Combat Analysis block (Veteran / Ambush→Spain Bonus / Artillery In Open / …).
 */
static void combat_analysis_fill_mods(
  char lines[][COMBAT_ANALYSIS_LINE_LEN],
  int* count,
  const ColonizeCombatSideFlags* flags
) {
  char buf[COMBAT_ANALYSIS_LINE_LEN];
  *count = 0;
  if (!flags) {
    return;
  }

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
    if (flags->flags & COMBAT_FLAG_FORTRESS) {
      combat_analysis_push_line(lines, count, "Fortress +200%");
    } else if (flags->flags & COMBAT_FLAG_STOCKADE) {
      combat_analysis_push_line(lines, count, "Stockade +100%");
    } else {
      combat_analysis_push_line(lines, count, "Colony +50%");
    }
  }
  if (flags->flags & COMBAT_FLAG_FORTIFY) {
    combat_analysis_push_line(lines, count, "Fortified +50%");
  }
  if (flags->flags & COMBAT_FLAG_ARTILLERY) {
    combat_analysis_push_line(lines, count, "Artillery In Open -75%");
  }
  if (flags->flags2 & COMBAT_FLAG_ARTY_COLONY) {
    combat_analysis_push_line(lines, count, "Artillery vs natives +100%");
  }
  if (flags->flags & COMBAT_FLAG_AMBUSH) {
    combat_analysis_push_line(lines, count, "Spain Bonus +50%");
  }
  if (flags->flags & COMBAT_FLAG_REF) {
    combat_analysis_push_line(lines, count, "Expeditionary Force +50%");
  }
  if (flags->flags2 & COMBAT_FLAG_TORIES) {
    snprintf(buf, sizeof(buf), "Tories +%d%%", flags->sol_percent);
    combat_analysis_push_line(lines, count, buf);
  } else if (flags->flags2 & COMBAT_FLAG_REBELS) {
    snprintf(buf, sizeof(buf), "Rebels +%d%%", flags->sol_percent);
    combat_analysis_push_line(lines, count, buf);
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

  combat_analysis_snap_chrome(&dlg->atk_chrome, pool, eng->attacker_id);
  combat_analysis_snap_chrome(&dlg->def_chrome, pool, eng->defender_id);
  combat_analysis_fill_mods(dlg->atk_lines, &dlg->atk_line_count, &eng->atk_flags);
  combat_analysis_fill_mods(dlg->def_lines, &dlg->def_line_count, &eng->def_flags);
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

static void combat_analysis_blit_side(
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* icons,
  const CombatAnalysisSideChrome* chrome,
  int x,
  int y
) {
  if (!fb || !chrome || !icons || chrome->sprite < 0 || chrome->sprite >= icons->sprite_count) {
    return;
  }
  unit_chrome_blit_unit(
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
    chrome->aboard
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
  ColonizeFramebuffer8* framebuffer
) {
  (void)select_color;
  if (!dlg || !dlg->open || !framebuffer) {
    return;
  }

  const int line_h = font ? (font->max_height > 0 ? font->max_height + 2 : 8) : 8;
  const int icon_h = 16;
  const int icon_w = 16;
  const int header_h = icon_h + 4;
  const int mod_rows =
    dlg->atk_line_count > dlg->def_line_count ? dlg->atk_line_count : dlg->def_line_count;
  const int title_h = line_h + 4;
  const int body_h = header_h + mod_rows * line_h + 8;
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

  if (!font) {
    return;
  }

  const char* title = "COMBAT ANALYSIS";
  const int tw = font_text_width(font, title);
  popup_draw_text_shadowed(
    font, framebuffer, ix + (iw - tw) / 2, iy + 2, title, text_color
  );

  const int mid = ix + iw / 2;
  const int y_hdr = iy + title_h;
  const int atk_icon_x = ix + 4;
  const int def_icon_x = ix + iw - 4 - icon_w - UNIT_CHROME_SPRITE_DX;
  char str_buf[16];

  combat_analysis_blit_side(framebuffer, font, unit_icons, &dlg->atk_chrome, atk_icon_x, y_hdr);
  combat_analysis_blit_side(framebuffer, font, unit_icons, &dlg->def_chrome, def_icon_x, y_hdr);

  snprintf(str_buf, sizeof(str_buf), "%d", dlg->eng.atk_strength);
  {
    const int sx = atk_icon_x + icon_w + UNIT_CHROME_SPRITE_DX + 4;
    const int sy = y_hdr + (icon_h - line_h) / 2 + 2;
    popup_draw_text_shadowed(font, framebuffer, sx, sy, str_buf, text_color);
  }
  snprintf(str_buf, sizeof(str_buf), "%d", dlg->eng.def_strength);
  {
    const int sw = font_text_width(font, str_buf);
    const int sx = def_icon_x - 4 - sw;
    const int sy = y_hdr + (icon_h - line_h) / 2 + 2;
    popup_draw_text_shadowed(font, framebuffer, sx, sy, str_buf, text_color);
  }

  const int y0 = y_hdr + header_h;
  for (int i = 0; i < dlg->atk_line_count; ++i) {
    popup_draw_text_shadowed(
      font, framebuffer, ix + 4, y0 + i * line_h, dlg->atk_lines[i], text_color
    );
  }
  for (int i = 0; i < dlg->def_line_count; ++i) {
    popup_draw_text_shadowed(
      font, framebuffer, mid + 2, y0 + i * line_h, dlg->def_lines[i], text_color
    );
  }
}
