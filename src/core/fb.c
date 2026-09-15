#include "core/fb.h"

#include <stddef.h>

void fb_put(ColonizeFramebuffer8* fb, int x, int y, uint8_t color) {
  if (!fb || !fb->pixels || x < 0 || y < 0 || x >= fb->width || y >= fb->height) {
    return;
  }
  fb->pixels[y * fb->width + x] = color;
}

void fb_hline(ColonizeFramebuffer8* fb, int y, int x0, int x1, uint8_t color) {
  if (!fb || !fb->pixels || y < 0 || y >= fb->height) {
    return;
  }
  if (x0 > x1) {
    const int t = x0;
    x0 = x1;
    x1 = t;
  }
  if (x0 < 0) {
    x0 = 0;
  }
  if (x1 >= fb->width) {
    x1 = fb->width - 1;
  }
  uint8_t* row = fb->pixels + (size_t)y * (size_t)fb->width;
  for (int x = x0; x <= x1; ++x) {
    row[x] = color;
  }
}

void fb_vline(ColonizeFramebuffer8* fb, int x, int y0, int y1, uint8_t color) {
  if (!fb || !fb->pixels || x < 0 || x >= fb->width) {
    return;
  }
  if (y0 > y1) {
    const int t = y0;
    y0 = y1;
    y1 = t;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (y1 >= fb->height) {
    y1 = fb->height - 1;
  }
  for (int y = y0; y <= y1; ++y) {
    fb->pixels[(size_t)y * (size_t)fb->width + (size_t)x] = color;
  }
}

void fb_fill_rect(ColonizeFramebuffer8* fb, int x, int y, int w, int h, uint8_t color) {
  if (!fb || !fb->pixels || w <= 0 || h <= 0) {
    return;
  }
  int x0 = x;
  int y0 = y;
  int x1 = x + w - 1;
  int y1 = y + h - 1;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 >= fb->width) {
    x1 = fb->width - 1;
  }
  if (y1 >= fb->height) {
    y1 = fb->height - 1;
  }
  if (x0 > x1 || y0 > y1) {
    return;
  }
  for (int yy = y0; yy <= y1; ++yy) {
    uint8_t* row = fb->pixels + (size_t)yy * (size_t)fb->width;
    for (int xx = x0; xx <= x1; ++xx) {
      row[xx] = color;
    }
  }
}

void fb_rect_outline(
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int w,
  int h,
  uint8_t top_left_col,
  uint8_t bottom_right_col
) {
  if (!fb || !fb->pixels || w <= 0 || h <= 0) {
    return;
  }
  /* Edge order matters at the corners: ui_button_draw_frame paints both
   * horizontal edges first and then both vertical ones, so the left and right
   * columns own all four corner pixels. Keep that order. */
  fb_fill_rect(fb, x, y, w, 1, top_left_col);
  fb_fill_rect(fb, x, y + h - 1, w, 1, bottom_right_col);
  fb_fill_rect(fb, x, y, 1, h, top_left_col);
  fb_fill_rect(fb, x + w - 1, y, 1, h, bottom_right_col);
}
