#include "core/ui_drag.h"

#include <stdlib.h>
#include <string.h>

void ui_drag_clear(UiDragSession* drag) {
  if (!drag) {
    return;
  }
  const bool had = drag->kind != UI_DRAG_NONE || drag->cursor_ok;
  memset(drag, 0, sizeof(*drag));
  if (had) {
    drag->cursor_dirty = true;
  }
}

bool ui_drag_active(const UiDragSession* drag) {
  return drag && drag->kind != UI_DRAG_NONE;
}

void ui_drag_begin(
  UiDragSession* drag,
  UiDragKind kind,
  int index,
  int unit_id,
  int amount
) {
  if (!drag || kind == UI_DRAG_NONE) {
    return;
  }
  drag->kind = kind;
  drag->index = index;
  drag->unit_id = unit_id;
  drag->amount = amount;
  drag->cursor_ok = false;
  drag->cursor_w = 0;
  drag->cursor_h = 0;
  drag->hotspot_x = 0;
  drag->hotspot_y = 0;
  drag->cursor_dirty = true;
}

bool ui_drag_set_cursor_sprite(UiDragSession* drag, const ColonizeSprite* sp) {
  if (!drag || !sp || !sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return false;
  }
  const int w = sp->width > UI_DRAG_CURSOR_MAX ? UI_DRAG_CURSOR_MAX : sp->width;
  const int h = sp->height > UI_DRAG_CURSOR_MAX ? UI_DRAG_CURSOR_MAX : sp->height;
  for (int y = 0; y < h; ++y) {
    memcpy(
      &drag->cursor_pixels[(size_t)y * (size_t)w],
      &sp->pixels[(size_t)y * (size_t)sp->width],
      (size_t)w
    );
  }
  drag->cursor_w = w;
  drag->cursor_h = h;
  drag->hotspot_x = w / 2;
  drag->hotspot_y = h / 2;
  drag->cursor_ok = true;
  drag->cursor_dirty = true;
  return true;
}

bool ui_drag_set_cursor_from_sheet(
  UiDragSession* drag,
  const ColonizeSpriteSheet* sheet,
  int sprite_index
) {
  if (!drag || !sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return false;
  }
  return ui_drag_set_cursor_sprite(drag, &sheet->sprites[sprite_index]);
}

static bool ui_drag_install_arrow(
  ColonizePlatform* platform,
  const ColonizePalette* palette,
  const ColonizeSpriteSheet* arrow_sheet
) {
  if (!arrow_sheet || arrow_sheet->sprite_count <= 0) {
    return false;
  }
  const ColonizeSprite* sp = &arrow_sheet->sprites[0];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return false;
  }
  const size_t n = (size_t)sp->width * (size_t)sp->height;
  uint8_t* masked = (uint8_t*)malloc(n);
  if (!masked) {
    return false;
  }
  memcpy(masked, sp->pixels, n);
  for (size_t i = 0; i < n; ++i) {
    if (masked[i] == 0x09u) {
      masked[i] = COLONIZE_SS_TRANSPARENT;
    }
  }
  const bool ok = platform_set_mouse_cursor_indexed(
    platform, masked, sp->width, sp->height, 1, 0, palette
  );
  free(masked);
  return ok;
}

bool ui_drag_apply_cursor(
  UiDragSession* drag,
  ColonizePlatform* platform,
  const ColonizePalette* palette,
  const ColonizeSpriteSheet* arrow_sheet,
  bool* arrow_built_inout
) {
  if (!platform || !palette || !arrow_built_inout) {
    return false;
  }

  if (drag && drag->kind != UI_DRAG_NONE && drag->cursor_ok && drag->cursor_w > 0 &&
      drag->cursor_h > 0) {
    if (!drag->cursor_dirty && !*arrow_built_inout) {
      return false; /* drag icon already showing */
    }
    const int hx = drag->hotspot_x;
    const int hy = drag->hotspot_y;
    if (!platform_set_mouse_cursor_indexed(
          platform,
          drag->cursor_pixels,
          drag->cursor_w,
          drag->cursor_h,
          hx,
          hy,
          palette
        )) {
      return false;
    }
    drag->cursor_dirty = false;
    *arrow_built_inout = false;
    return true;
  }

  /* Inactive drag: ensure arrow cursor is installed. */
  if (*arrow_built_inout && !(drag && drag->cursor_dirty)) {
    return false;
  }
  if (!ui_drag_install_arrow(platform, palette, arrow_sheet)) {
    return false;
  }
  if (drag) {
    drag->cursor_dirty = false;
  }
  *arrow_built_inout = true;
  return true;
}
