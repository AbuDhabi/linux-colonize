#ifndef COLONIZE_UI_DRAG_H
#define COLONIZE_UI_DRAG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/ss.h"
#include "platform/platform.h"

#define UI_DRAG_CURSOR_MAX 48 /* max edge for cached drag cursor sprite */

typedef enum UiDragKind {
  UI_DRAG_NONE = 0,
  UI_DRAG_COLONY_CARGO,
  UI_DRAG_COLONY_HOLD,
  UI_DRAG_COLONY_COLONIST,
  UI_DRAG_COLONY_OUTSIDE,
  UI_DRAG_EUROPE_MARKET,
  UI_DRAG_EUROPE_HOLD,
  UI_DRAG_EUROPE_HARBOR_SHIP,
  UI_DRAG_EUROPE_EXPECTED_SHIP,
  UI_DRAG_EUROPE_BOUND_SHIP
} UiDragKind;

typedef struct UiDragSession {
  UiDragKind kind;
  int index;   /* cargo type, hold slot, colonist idx, ship idx, … */
  int unit_id; /* outside unit / transport context when needed */
  int amount;  /* e.g. 100 for warehouse / market loads */
  bool cursor_ok;
  int cursor_w;
  int cursor_h;
  uint8_t cursor_pixels[UI_DRAG_CURSOR_MAX * UI_DRAG_CURSOR_MAX];
  bool cursor_dirty; /* true after begin/clear until applied */
} UiDragSession;

void ui_drag_clear(UiDragSession* drag);

bool ui_drag_active(const UiDragSession* drag);

void ui_drag_begin(
  UiDragSession* drag,
  UiDragKind kind,
  int index,
  int unit_id,
  int amount
);

/* Copy a sprite into the drag cursor buffer (clamped to UI_DRAG_CURSOR_MAX). */
bool ui_drag_set_cursor_sprite(UiDragSession* drag, const ColonizeSprite* sp);

bool ui_drag_set_cursor_from_sheet(
  UiDragSession* drag,
  const ColonizeSpriteSheet* sheet,
  int sprite_index
);

/*
 * Apply drag icon (center hotspot) or restore arrow (tip hotspot).
 * Returns true if the OS cursor was (re)built this call.
 */
bool ui_drag_apply_cursor(
  UiDragSession* drag,
  ColonizePlatform* platform,
  const ColonizePalette* palette,
  const ColonizeSpriteSheet* arrow_sheet,
  bool* arrow_built_inout
);

#endif
