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

/* 636c:00c6 row pitch (add [bp-0xe],0x14). */
#define COMBAT_ROW_PITCH 0x14

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
 *   TERRAIN    FUN_281f_033a → FUN_1baa_0006 — the engagement tile. 1baa_0006
 *              is a plain surface-to-surface rect copy off the *rendered map*,
 *              so the row shows the tile exactly as the map draws it, overlays
 *              included (a mountain over Savannah shows the mountain). The port
 *              redraws it from map_tile_layer_cmds, the same list the map
 *              viewport and the colony minimap blit; it falls back to the bare
 *              base tile when the map or PHYS0.SS is not available, and is
 *              silently skipped when TERRAIN.SS is not loaded.
 */
static void combat_analysis_blit_row_icon(
  ColonizeFramebuffer8* fb,
  const ColonizeSpriteSheet* icons,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  const ColonizeWorldMap* map,
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
  if (row->icon_kind == COMBAT_ROW_ICON_TERRAIN && map && phys0 && row->icon_tile_x >= 0) {
    ColonizeMapLayerCmd cmds[MAP_LAYER_CMDS_MAX];
    const int ncmd =
      map_tile_layer_cmds(map, row->icon_tile_x, row->icon_tile_y, 0, cmds, MAP_LAYER_CMDS_MAX);
    for (int ci = 0; ci < ncmd; ++ci) {
      const ColonizeMapLayerCmd* cmd = &cmds[ci];
      const ColonizeSpriteSheet* layer =
        (cmd->sheet == MAP_LAYER_SHEET_TERRAIN) ? terrain : phys0;
      if (cmd->sprite < 0 || cmd->sprite >= layer->sprite_count) {
        continue;
      }
      if (cmd->into_holes) {
        ss_blit_sprite_where_dest(layer, cmd->sprite, fb, x + cmd->ox, y + cmd->oy, 0);
      } else {
        ss_blit_sprite(layer, cmd->sprite, fb, x + cmd->ox, y + cmd->oy);
      }
    }
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

/* FUN_281f_013c / 0150 / 0100 -> FUN_104b_0288 / 02c2 / 0318 (raw 977 / 991 / 1007): one
 * FONTTINY blit through the map {skip, c, c, c} (104b:0293-0298), no shadow pass. */
static void combat_analysis_text(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, int x, int y, const char* s, uint8_t c
) {
  font_draw_text_shaded(font, fb, x, y, s, (const uint8_t[4]){0, c, c, c});
}

void combat_analysis_render(
  CombatAnalysisDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizeSpriteSheet* unit_icons,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  const ColonizeWorldMap* map,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t hilite_color,
  const ColonizePalette* active_palette,
  ColonizeFramebuffer8* framebuffer
) {
  if (!dlg || !dlg->open || !framebuffer) {
    return;
  }

  /* FUN_636c_0000 geometry, read from the OVL18 asm (636c:003c-0097, 10b3-10bc):
   * frame x=0x35 w=0xd6 (fixed: DOS overdraws a too-long label, it never
   * widens the frame), h = N*0x14+6 with N = 2 (title row + header row) + the
   * tallest column's modifier rows, y = 100 - (h >> 1) (636c:004a-0059). */
  const int mod_rows =
    dlg->atk_line_count > dlg->def_line_count ? dlg->atk_line_count : dlg->def_line_count;
  const int h = (2 + mod_rows) * COMBAT_ROW_PITCH + 6;
  const int w = 0xd6;
  const int x = 0x35;
  const int y = 100 - (h >> 1);
  dlg->dialog_x = x;
  dlg->dialog_y = y;
  dlg->dialog_w = w;
  dlg->dialog_h = h;

  int ix = 0, iy = 0, iw = 0, ih = 0;
  popup_draw(framebuffer, x, y, w, h, wood_tile, colors, &ix, &iy, &iw, &ih);

  if (!font) {
    return;
  }

  /* LABELS.TXT @MISC row 75 "COMBAT ANALYSIS". FUN_636c_0000 draws labels and the title
   * in @COLORS basic (DS:0x830, 636c:0080 / 0184...), values and the base strength in
   * @COLORS hilite (DS:0x831, 636c:01a3...). Title: FUN_281f_0100 = FUN_104b_0318
   * centred in [0x38, 0x38+0xd0) at frame_y+9 (636c:0087-00aa); the centring width is
   * FUN_104b_0216 = pixel width - 1, hence (tw - 1). */
  const char* title = reports_misc_display_word(75, "");
  const int tw = font_text_width(font, title);
  combat_analysis_text(font, framebuffer, 0x38 + (0xd0 >> 1) - ((tw - 1) >> 1), y + 9, title, text_color);

  /* Column pen x (636c:10b1-10b9): attacker 0x38+2, defender 0x38+0x68; values
   * right-align at pen+0x50 (FUN_281f_0150, 636c:01aa); text y = row top + 6
   * (636c:018a); rows start at frame_y+0x17 (636c:00b2), 0x14 pitch. */
  const int col_x_of[2] = {0x38 + 2, 0x38 + 0x68};
  const int y_hdr = y + 0x17;
  char str_buf[16];

  /* Header row: unit chrome at (pen, row top) (FUN_281f_02bc, 636c:00f9), type name at
   * pen+0x11, baseline strength right-aligned (NAMES byte via 0x8d06 / -0x72fa). */
  for (int side = 0; side < 2; ++side) {
    const CombatAnalysisSideChrome* chrome = side == 0 ? &dlg->atk_chrome : &dlg->def_chrome;
    const char* name = side == 0 ? dlg->atk_name : dlg->def_name;
    const int base =
      side == 0 ? dlg->eng.atk_flags.base_combat : dlg->eng.def_flags.base_combat;
    const int col_x = col_x_of[side];
    combat_analysis_blit_side(framebuffer, font, unit_icons, chrome, col_x, y_hdr, active_palette);
    combat_analysis_text(font, framebuffer, col_x + 0x11, y_hdr + 6, name, text_color);
    snprintf(str_buf, sizeof(str_buf), "%d", base);
    combat_analysis_text(
      font, framebuffer, col_x + 0x50 - font_text_width(font, str_buf), y_hdr + 6, str_buf,
      hilite_color
    );
  }

  const int y0 = y_hdr + COMBAT_ROW_PITCH;
  for (int side = 0; side < 2; ++side) {
    const CombatAnalysisRow* rows_arr = side == 0 ? dlg->atk_rows : dlg->def_rows;
    const int count = side == 0 ? dlg->atk_line_count : dlg->def_line_count;
    const int col_x = col_x_of[side];
    for (int i = 0; i < count; ++i) {
      const CombatAnalysisRow* row = &rows_arr[i];
      const int row_top = y0 + i * COMBAT_ROW_PITCH;
      /* DOS blits the row's picture at the pen (colx, row top), then draws the
       * label at pen + indent (local_76) and the value right-aligned at colx+0x50. */
      combat_analysis_blit_row_icon(
        framebuffer, unit_icons, terrain, phys0, map, row, col_x, row_top, active_palette
      );
      combat_analysis_text(
        font, framebuffer, col_x + row->label_indent, row_top + 6, row->label, text_color
      );
      combat_analysis_text(
        font, framebuffer, col_x + 0x50 - font_text_width(font, row->value), row_top + 6,
        row->value, hilite_color
      );
    }
  }
}
