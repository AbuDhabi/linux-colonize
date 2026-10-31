#include "core/ui_button.h"

#include <string.h>

#include "core/fb.h"

void ui_button_draw_frame(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  uint8_t dark,
  uint8_t light
) {
  if (!framebuffer || !framebuffer->pixels || w < 2 || h < 2) {
    return;
  }
  fb_rect_outline(framebuffer, x, y, w, h, dark, light);
}

void ui_button_measure(
  const ColonizeFont* font,
  const char* label,
  int* out_w,
  int* out_h
) {
  const int tw = font_text_width(font, label);
  const int th = font ? (font->max_height > 0 ? (int)font->max_height : 6) : 7;
  if (out_w) {
    *out_w = tw + 6; /* 1px frame + 2px pad each side */
  }
  if (out_h) {
    *out_h = th + 4;
  }
}

void ui_button_draw(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  const char* label,
  const UiButtonColors* colors
) {
  if (!framebuffer || !label || !colors) {
    return;
  }
  int bw = w;
  int bh = h;
  if (bw <= 0 || bh <= 0) {
    ui_button_measure(font, label, &bw, &bh);
  }
  ui_button_draw_frame(framebuffer, x, y, bw, bh, colors->dark, colors->light);

  const int tw = font_text_width(font, label);
  const int th = font ? (font->max_height > 0 ? (int)font->max_height : 6) : 7;
  const int tx = x + (bw - tw) / 2;
  const int ty = y + (bh - th) / 2;
  font_draw_text_hotkey(
    font, framebuffer, tx, ty, label, colors->text, colors->hotkey
  );
}

