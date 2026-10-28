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
  UNIT_CHROME_CORNER_TOP_CENTER_DAMAGED = 4 /* Damaged Artillery: top-center, y+2 */
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
/* port_bugs.md WoI flags: rebel (human) nation — its colonies fly the American
 * flag; crown-held colonies fly this nation's original color. Cleared
 * automatically when the crown nation is set to -1. */
void unit_chrome_set_rebel_nation(int nation_id);
int unit_chrome_rebel_nation(void);
/* US flag palette picks (navy hoist / red / white stripes) for the rebel
 * colony marker. Any output may come back -1 without a palette. */
void unit_chrome_rebel_flag_colors_for_palette(
  const ColonizePalette* active_palette, int* out_navy, int* out_red, int* out_white
);

/*
 * The two nation shades DOS's @COUNTRY table (DS:0x848) encodes as index
 * and index-8 — the bright fill and its darker companion — nearest-matched
 * into `active_palette`. Same reasoning as
 * unit_chrome_blit_unit_for_palette: the raw indices are ICONS.SS-native,
 * and every other screen's palette (TERRAIN.SS included) repurposes slots
 * 5/13 back to EGA magenta, which is what made the Dutch map chrome pink
 * (port_bugs.md #364). Outputs are -1 when active_palette is NULL or nation_id
 * isn't a 0..3 European; the crown/rebel overrides do NOT apply here.
 */
void unit_chrome_nation_shades_for_palette(
  int nation_id, const ColonizePalette* active_palette, int* out_bright, int* out_dark
);
/* -1 outside the WoI; else the crown's borrowed nation slot. */
int unit_chrome_crown_nation(void);

/*
 * DOS @UNIT type id → badge corner. `damaged` is the unit's DOS +0x3148
 * bit7 (OpenCol col1_flags15 & 0x80); it only matters for Artillery
 * (@UNIT 0x0b), whose damaged form takes the y+2 box (DOS FUN_112b_01ba
 * raw 2109-2111 / 2253-2254). It is NOT "aboard a ship" — that was the
 * port's own reading until the 2026-09-10 seventh fix wave.
 * The type argument is an @UNIT id, not a pool index; callers pass a pool
 * index and get away with it only through the NAMES.TXT ordering invariant
 * — see the definition's comment (smell audit 2026-09-10 #7).
 */
UnitChromeCorner unit_chrome_corner_for_type(int dos_unit_type_id, bool damaged);

/*
 * port_bugs.md #705. FUN_112b_01ba's LAST badge arm (viceroy_unpacked_2.c raw
 * 2148-2172) replaces the @ORDERS letter outright for a DAMAGED unit
 * (+0x3148 bit7) whose type is not 0x0b (Artillery keeps its own y+2 box
 * instead, raw 2109-2111):
 *
 *   iVar5 = @UNIT[type].0x5235 - unit[+0x315a];          // repair countdown
 *   if (FUN_137f_000a(unit.x, unit.y)) iVar5 = (iVar5 + 1) >> 1;
 *   badge = iVar5 < 10 ? '0' + iVar5 : '+';
 *
 * DS:0x5235 is the @UNIT combat/DEFENSE column the repair tick already uses
 * as its threshold (units.c), +0x315a is col1_counter16 (turns repaired so
 * far), and FUN_137f_000a(x,y) (raw 5215-5226) is the plain "interior map
 * tile" test `x >= 1 && y >= 1 && x < width-1 && y < height-1` — false for
 * a hull parked on DOS's off-map Europe diagonal, which is why only a ship
 * still on the map shows the halved figure.
 *
 * Returns an orders_index that unit_chrome_order_letter renders as that
 * digit, or -1 when the arm does not apply and the caller should pass the
 * unit's real orders byte.
 */
#define UNIT_CHROME_ORDERS_REPAIR_BASE 0x100
/* FUN_112b_01ba raw 2122-2129: a foreign Privateer's badge is 'X'. */
#define UNIT_CHROME_ORDERS_PRIVATEER_X 0x200
/* Same routine: a foreign hull's goods-hold count digit (ordinary ink). */
#define UNIT_CHROME_ORDERS_HOLDS_BASE 0x300
/*
 * port_bugs.md #1177a. FUN_112b_01ba (viceroy_unpacked_2.c ~2134, asm
 * 0x3959-0x3986): for an AI / withdrawn Euro nation (`nation < 4 &&
 * player[nation].control != 0`) with the cheat bit (head.game_options.
 * cheats_enabled) and Debug Info Flags DS:0x894 & 8 ("Foreign AI planning
 * modes"), the badge glyph is the unit's AI plan byte (+0x314b, `ai_plan`;
 * >= 0x80 reads 'E'; plan 0 = an empty string, a 3px box). It lands after the
 * hold digit / Privateer 'X' and before the damaged repair digit (last write
 * wins). The ink still follows the real orders byte, so the index carries both:
 * PLAN_BASE + (glyph << 4) + (orders & 15). unit_chrome_order_letter returns
 * the glyph (possibly NUL); unit_chrome_letter_color decodes the orders.
 */
#define UNIT_CHROME_ORDERS_PLAN_BASE 0x1000
/* Nations (bit n = nation n, 0..3) whose units wear the plan glyph; the game
 * loop recomputes it every frame (cheat bit && flag 8 && control != 0). */
void unit_chrome_set_plan_nations(unsigned mask);
/* `cur` (the order / hold / X index the caller already settled on) when the
 * nation is not in the mask, else the plan index above. `orders` is the unit's
 * real @ORDERS byte, `plan` its +0x314b. Apply before the repair badge. */
int unit_chrome_plan_badge_index(int nation_id, unsigned plan, int orders, int cur);
/* The same for one unit, plus the port-only Automate badge 'A' (divergences.pioneer_plan),
 * carried as a plan glyph so the ink still follows the real orders byte. */
typedef struct ColonizeUnit ColonizeUnit;
int unit_chrome_unit_badge_index(const ColonizeUnit* u, int cur);
/* Real @ORDERS byte behind an index (for ink); plan indices carry it, others are it. */
int unit_chrome_real_orders(int orders_index);
/* Repair digit ink (raw 2290-2296): 12 for nation 2, else 15. */
bool unit_chrome_is_repair_badge(int orders_index);
int unit_chrome_repair_badge_index(
  int dos_unit_type_id, bool damaged, int repair_threshold, int repair_counter, bool halve
);

/* @ORDERS index → single letter (natives always '-'), or the #705 repair
 * digit when orders_index came from unit_chrome_repair_badge_index. */
char unit_chrome_order_letter(int orders_index, int nation_id);

/* Letter ink: black, or names_color-8 / 8 for Sentry & Fortified. */
uint8_t unit_chrome_letter_color(int nation_id, int orders_index);




/*
 * Preferred entry point for any screen that knows its own active output
 * palette (the palette the framebuffer is actually converted to RGB
 * through — e.g. the map's `game->map_palette`, the colony screen's
 * `view->frame.palette`, a report's plate palette). Looks up the nearest
 * available match to each of the 4 European nations' VICEROY.PAL
 * fill/letter color (see k_nation_fill_rgb_native in unit_chrome.c) within
 * `active_palette` and draws with that instead of
 * unit_chrome_nation_color()'s raw index; under a VICEROY-based palette the
 * two agree. `active_palette` NULL, or
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
  bool damaged,
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
  bool damaged,
  int fill_override,
  int letter_override
);

#endif
