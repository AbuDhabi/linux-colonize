#ifndef COLONIZE_UNIT_STACK_H
#define COLONIZE_UNIT_STACK_H

#include <stdbool.h>
#include <stdint.h>

#include "core/popup.h"
#include "core/ss.h"
#include "core/units_cargo.h"
#include "platform/platform.h"

typedef struct ColonizeFont ColonizeFont;

/*
 * Tile stack picker: on-map units plus ship cargo at one tile.
 * Click sentry cargo once to wake (orders=0); click again (or any non-sentry) to select.
 * Port-only: Shift+click marks rows with moves left (green frame); letting go of
 * Shift closes the picker with ids[0..group_count) = the marked units, for the
 * caller to take as a unit group (game_unit_group_set) and zero.
 */
typedef struct UnitStackPopup {
  bool open;
  int tile_x;
  int tile_y;
  int ids[UNITS_TILE_STACK_MAX];
  int total; /* units collected at the tile (ids[0..total)) */
  int count; /* rows shown: ids[0..count), column-major */
  int rows;  /* rows per column (DOS: 10) */
  int cols;  /* columns shown (DOS: 1); render grows it */
  int selection;
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int list_y0; /* top of row 0's icon row */
  int line_h; /* icon row pitch (16) */
  int list_x0; /* left edge of column 0 */
  int col_w;   /* column pitch */
  bool picked[UNITS_TILE_STACK_MAX]; /* Shift-marked rows, by ids[] slot */
  int group_count;
} UnitStackPopup;

/* Collect human units at (x,y); opens only when count > 1. Shows one column until
 * unit_stack_render lays out more (port-only, screens over 320x200). */
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
  const ColonizeFont* font,       /* row text: FONTINTR (DS:0x268a) */
  const ColonizeFont* chrome_font, /* orders letter: FONTTINY (DS:0x89e), FUN_112b_01ba */
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  const ColonizePalette* active_palette, /* frame palette for nation-fill remap */
  const ColonizeCol1Save* col1,          /* WoI Rebel/Tory adjectives; may be NULL */
  ColonizeFramebuffer8* framebuffer
);

#endif
