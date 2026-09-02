#ifndef COLONIZE_UNIT_STACK_H
#define COLONIZE_UNIT_STACK_H

#include <stdbool.h>
#include <stdint.h>

#include "core/popup.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/platform.h"

typedef struct ColonizeFont ColonizeFont;

/*
 * Tile stack picker: on-map units plus ship cargo at one tile.
 * Click sentry cargo once to wake (orders=0); click again (or any non-sentry) to select.
 */
typedef struct UnitStackPopup {
  bool open;
  int tile_x;
  int tile_y;
  int ids[UNITS_TILE_STACK_MAX];
  int count;
  int selection;
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int list_y0;
  int list_x0;
  int line_h;
  int col_w;   /* grid column width in px (bugs.md 252) */
  int cols;    /* columns in use, 1..3 */
  int rows;    /* rows per column */
} UnitStackPopup;

void unit_stack_close(UnitStackPopup* dlg);

/* Collect human units at (x,y); opens only when count > 1. */
bool unit_stack_try_open(
  UnitStackPopup* dlg,
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id
);

/*
 * Returns true if input was consumed (dialog open).
 * *out_select_id >= 0 means caller should select that unit and close was already done.
 */
bool unit_stack_handle_input(
  UnitStackPopup* dlg,
  ColonizeUnitPool* pool,
  const ColonizeInputState* input,
  int* out_select_id
);

void unit_stack_render(
  UnitStackPopup* dlg,
  const ColonizeUnitPool* pool,
  const ColonizeSpriteSheet* icons,
  const ColonizeMsgCatalog* names,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  const ColonizePalette* active_palette, /* frame palette for nation-fill remap */
  ColonizeFramebuffer8* framebuffer
);

#endif
