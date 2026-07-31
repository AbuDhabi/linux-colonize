#ifndef COLONIZE_POPUP_H
#define COLONIZE_POPUP_H

#include <stdint.h>

#include "core/ss.h"
#include "platform/platform.h"

/*
 * Reusable wood popup chrome (DOS dialog / confirm / prompt frame).
 *
 * Outside → in for any [x,y,w,h):
 *   1. Tile fill (caller sheet sprite 0; solid 4 if sheet missing)
 *   2. 1px black outer (all sides)
 *   3. 1px mid wood-brown (all sides) — @COLORS border0
 *   4. 1px bevel: light top+right (border1), dark bottom+left (border2)
 *
 * Content belongs in the inner rect (inset by POPUP_FRAME_INSET).
 * Title menu uses OPENTILE.SS; in-game popups typically use WOODTILE.SS.
 */
#define POPUP_FRAME_INSET 3
#define POPUP_FALLBACK_FILL 4

typedef struct ColonizePopupColors {
  uint8_t outer; /* black */
  uint8_t mid; /* wood brown, all sides */
  uint8_t light; /* top + right bevel */
  uint8_t dark; /* bottom + left bevel */
} ColonizePopupColors;

/* Fill from NAMES.TXT @COLORS border0/1/2 (+ outer 0) on WOODPANL / in-game indices. */
void popup_colors_from_ui(ColonizePopupColors* out);

/*
 * Remap mid/light/dark onto target_palette by nearest RGB, using source_palette
 * as the reference for the @COLORS indices. Outer stays 0.
 * If either palette is NULL, leaves colors unchanged (aside from ensuring outer=0).
 */
void popup_colors_remap(
  ColonizePopupColors* colors,
  const ColonizePalette* source_palette,
  const ColonizePalette* target_palette
);

/*
 * Draw fill + three outline layers. Optional out_inner_* receives the content
 * rect inset by POPUP_FRAME_INSET (clamped; may be empty if w/h too small).
 * tile may be NULL (solid POPUP_FALLBACK_FILL). colors may be NULL → from_ui.
 */
void popup_draw(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  const ColonizeSpriteSheet* tile,
  const ColonizePopupColors* colors,
  int* out_inner_x,
  int* out_inner_y,
  int* out_inner_w,
  int* out_inner_h
);

#endif
