#ifndef COLONIZE_UNIT_CHROME_H
#define COLONIZE_UNIT_CHROME_H

#include <stdbool.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Unit graphic = black silhouette (2px left) + orders/allegiance box + sprite
 * (DOS FUN_112b_01ba). Prefer unit_chrome_blit_unit at call sites.
 *
 * F6 Colony / F7 Naval adviser icon rows (when added) should call
 * unit_chrome_blit_unit the same way as map / sidebar / Europe / colony.
 */

typedef enum UnitChromeCorner {
  UNIT_CHROME_CORNER_BOTTOM_RIGHT = 0,
  UNIT_CHROME_CORNER_TOP_RIGHT = 1,
  UNIT_CHROME_CORNER_TOP_CENTER = 2,
  UNIT_CHROME_CORNER_TOP_LEFT = 3,
  UNIT_CHROME_CORNER_TOP_CENTER_ABOARD = 4 /* Artillery aboard: top-center, y+2 */
} UnitChromeCorner;

#define UNIT_CHROME_ORDERS_MAX 16
/* Color sprite / silhouette offset from the orders-box origin (box stays put). */
#define UNIT_CHROME_SPRITE_DX 2
/* Black silhouette underlay relative to the color sprite. */
#define UNIT_CHROME_SHADOW_DX (-2)
/* Extra pad so stack under-rect (±2) stays inside selection frames. */
#define UNIT_CHROME_STACK_PAD 2

/* Inclusive outer frame around chrome unit art (shadow + orders + sprite), 1px margin. */
void unit_chrome_selection_frame(
  int x,
  int y,
  int sprite_w,
  int sprite_h,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h
);

/* Load @ORDERS letter column from NAMES.TXT (fallback table if missing). */
void unit_chrome_load_orders(const ColonizeMsgCatalog* names);

/*
 * Nation fill color for badges / turn-owner box.
 * England uses saturated red 112 (NAMES lists 12; VGA entry 12 is pink 255,85,85 —
 * original Europe screenshots show pure red ≈ palette 112).
 */
uint8_t unit_chrome_nation_color(int nation_id);

/* Col1 @UNIT type index → badge corner (aboard: unit is cargo of a ship). */
UnitChromeCorner unit_chrome_corner_for_type(int display_type_index, bool aboard);

/* @ORDERS index → single letter (natives always '-'). */
char unit_chrome_order_letter(int orders_index, int nation_id);

/* Letter ink: black, or names_color-8 / 8 for Sentry & Fortified. */
uint8_t unit_chrome_letter_color(int nation_id, int orders_index);

/* Orders/allegiance box only (no sprite). */
void unit_chrome_draw(
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  int icon_x,
  int icon_y,
  int icon_w,
  int icon_h,
  int display_type_index,
  int nation_id,
  int orders_index,
  bool show_stack,
  bool aboard
);

/*
 * Full unit graphic: orders box at (x,y); color sprite at (x+SPRITE_DX,y) with
 * black silhouette SPRITE_DX+SHADOW_DX to its left.
 */
void unit_chrome_blit_unit(
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  int x,
  int y,
  int display_type_index,
  int nation_id,
  int orders_index,
  bool show_stack,
  bool aboard
);

/*
 * Same as unit_chrome_blit_unit, but fill_override/letter_override (each
 * -1 for "use the normal nation-color computation", matching
 * unit_chrome_blit_unit exactly) let the caller substitute raw palette
 * indices for the orders-box fill and its letter ink.
 *
 * Needed because unit_chrome_nation_color()/unit_chrome_letter_color()'s
 * indices are tuned against ICONS.SS's own *native* palette (its index 13
 * really is a saturated Dutch orange, index 5 a matching darker shade for
 * the letter) — correct for screens whose active output palette is close
 * to that native one, but not for report screens (REPORT*.PIK), whose own
 * embedded palettes repurpose those exact slots back to plain EGA magenta.
 * See reports.c's Colony report (F6) garrison row for the call site that
 * looks up the closest-available match in the active report palette and
 * passes it here instead.
 */
void unit_chrome_blit_unit_colored(
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  int x,
  int y,
  int display_type_index,
  int nation_id,
  int orders_index,
  bool show_stack,
  bool aboard,
  int fill_override,
  int letter_override
);

#endif
