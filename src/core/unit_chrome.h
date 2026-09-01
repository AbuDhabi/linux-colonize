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
/* WoI: mark the Crown's borrowed nation slot so REF units render white
 * (-1 = none; set each frame by the game loop while independence is
 * declared). */
void unit_chrome_set_crown_nation(int nation_id);

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

/*
 * Preferred entry point for any screen that knows its own active output
 * palette (the palette the framebuffer is actually converted to RGB
 * through — e.g. the map's `game->map_palette`/TERRAIN.SS, the colony
 * screen's `view->frame.palette`/WOODPANL.PIK, a report's own background
 * palette). Looks up the nearest available match to each of the 4
 * European nations' *true* fill/letter color (see k_nation_fill_rgb_native
 * in unit_chrome.c) within `active_palette` and draws with that instead of
 * unit_chrome_nation_color()'s raw ICONS.SS-native index, which is only
 * correct when the active palette happens to still be native-compatible —
 * true for nothing except ICONS.SS itself. `active_palette` NULL, or
 * nation_id outside 0..3 (natives — no reported color issue there),
 * falls back to unit_chrome_blit_unit's plain default behavior.
 */
void unit_chrome_blit_unit_for_palette(
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
  const ColonizePalette* active_palette
);

/*
 * Nation light/dark palette indices (nearest match within active_palette,
 * -1/-1 if active_palette is NULL or nation_id isn't a 0..3 European) for
 * recoloring ICONS.SS #0-3's stored blue colony-flag pixels to the owning
 * nation. See unit_chrome.c for the full explanation and
 * colony_map_icon_flag_pixels (colony.h) for the fixed pixel mask.
 */
void unit_chrome_nation_flag_shades_for_palette(
  int nation_id, const ColonizePalette* active_palette, int* out_light, int* out_dark
);

/*
 * Every screen that blits a unit/colonist sprite standalone (not part of a
 * tile/building draw) does it in one of exactly three ways. Name the mode
 * at the call site instead of open-coding "which blit call(s) do I need
 * here" per screen — that's how the settlement-view colonist shadow ended
 * up 1px (an ad-hoc colony_screen-local helper) instead of the map's 2px:
 * nothing forced it to match the existing convention.
 *
 *   UNIT_CHROME_PLAIN_SPRITE       — just the sprite, no shadow, no box.
 *   UNIT_CHROME_SPRITE_WITH_SHADOW — sprite + the same 2px-left black-
 *                                    silhouette underlay the map/orders
 *                                    mode uses (UNIT_CHROME_SHADOW_DX),
 *                                    tinted `shadow_color`, no orders box.
 *   UNIT_CHROME_SPRITE_ORDERS      — shadow + nation-color orders/
 *                                    allegiance box + sprite (DOS
 *                                    FUN_112b_01ba); everything
 *                                    unit_chrome_blit_unit_colored draws.
 */
typedef enum UnitChromeDrawMode {
  UNIT_CHROME_PLAIN_SPRITE = 0,
  UNIT_CHROME_SPRITE_WITH_SHADOW = 1,
  UNIT_CHROME_SPRITE_ORDERS = 2
} UnitChromeDrawMode;

/*
 * Single entry point for all three modes above. Params outside a given
 * mode's own list are ignored (pass 0/false/-1/NULL) — `font` only matters
 * for ORDERS (its letter glyph); `shadow_color` only for SHADOW/ORDERS
 * (pass 0 for plain black, matching every existing caller); everything
 * from `display_type_index` on is ORDERS-only and matches
 * unit_chrome_blit_unit_colored's own params exactly (fill_override/
 * letter_override: -1 = auto nation color).
 */
void unit_chrome_blit(
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  int x,
  int y,
  UnitChromeDrawMode mode,
  int shadow_color,
  int display_type_index,
  int nation_id,
  int orders_index,
  bool show_stack,
  bool aboard,
  int fill_override,
  int letter_override
);

#endif
