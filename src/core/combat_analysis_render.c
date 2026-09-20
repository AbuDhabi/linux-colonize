/*
 * Combat Analysis rendering half, split out of combat_analysis.c 2026-09-16
 * so the simulation side keeps only should_show / log_engagement / the
 * presenter hook. Pure move: bodies are byte-identical.
 *
 * This file is UI.
 */
#include "core/combat_analysis.h"

#include <stdio.h>
#include <string.h>

#include "core/colony.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/reports.h"
#include "core/ss.h"
#include "core/ui_colors.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"

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
    chrome->damaged,
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

  /* LABELS.TXT @MISC row 75 "COMBAT ANALYSIS". */
  const char* title = reports_misc_display_word(75, "COMBAT ANALYSIS");
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
