#ifndef COLONIZE_UI_BUTTON_H
#define COLONIZE_UI_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "core/font.h"
#include "platform/platform.h"

/*
 * Transparent text button: no fill, 1px bevel only.
 * Dark top+left, light bottom+right (inset look — Europe RECRUIT etc.).
 * Label may use '~' hotkey markup (see font_draw_text_hotkey).
 */
typedef struct UiButtonColors {
  uint8_t dark;  /* top + left */
  uint8_t light; /* bottom + right */
  uint8_t text;
  uint8_t hotkey;
} UiButtonColors;

void ui_button_draw_frame(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  uint8_t dark,
  uint8_t light
);

/* Frame + centered label. w/h may be 0 to size to text + padding. */
void ui_button_draw(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  const char* label,
  const UiButtonColors* colors
);

bool ui_button_hit(int x, int y, int w, int h, int mx, int my);

/* Preferred size for label (includes 1px frame + 2px padding each side). */
void ui_button_measure(
  const ColonizeFont* font,
  const char* label,
  int* out_w,
  int* out_h
);

#endif
