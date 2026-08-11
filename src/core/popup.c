#include "core/popup.h"

#include <stddef.h>

#include "core/font.h"
#include "core/ui_colors.h"

void popup_colors_from_ui(ColonizePopupColors* out) {
  if (!out) {
    return;
  }
  out->outer = 0;
  out->mid = COLONIZE_COL_BORDER0;
  out->light = COLONIZE_COL_BORDER1;
  out->dark = COLONIZE_COL_BORDER2;
}

static uint8_t popup_nearest_rgb(const ColonizePalette* pal, int r, int g, int b) {
  long best = 1L << 30;
  int best_i = 0;
  for (int i = 0; i < 256; ++i) {
    const long dr = (long)pal->rgb[i][0] - r;
    const long dg = (long)pal->rgb[i][1] - g;
    const long db = (long)pal->rgb[i][2] - b;
    const long d = dr * dr + dg * dg + db * db;
    if (d < best) {
      best = d;
      best_i = i;
    }
  }
  return (uint8_t)best_i;
}

void popup_colors_remap(
  ColonizePopupColors* colors,
  const ColonizePalette* source_palette,
  const ColonizePalette* target_palette
) {
  if (!colors) {
    return;
  }
  colors->outer = 0;
  if (!source_palette || !target_palette) {
    return;
  }
  const uint8_t mid = colors->mid;
  const uint8_t light = colors->light;
  const uint8_t dark = colors->dark;
  colors->mid = popup_nearest_rgb(
    target_palette,
    source_palette->rgb[mid][0],
    source_palette->rgb[mid][1],
    source_palette->rgb[mid][2]
  );
  colors->light = popup_nearest_rgb(
    target_palette,
    source_palette->rgb[light][0],
    source_palette->rgb[light][1],
    source_palette->rgb[light][2]
  );
  colors->dark = popup_nearest_rgb(
    target_palette,
    source_palette->rgb[dark][0],
    source_palette->rgb[dark][1],
    source_palette->rgb[dark][2]
  );
}

static void popup_put(ColonizeFramebuffer8* fb, int x, int y, uint8_t color) {
  if (!fb || !fb->pixels || x < 0 || y < 0 || x >= fb->width || y >= fb->height) {
    return;
  }
  fb->pixels[y * fb->width + x] = color;
}

static void popup_hline(ColonizeFramebuffer8* fb, int y, int x0, int x1, uint8_t color) {
  if (x0 > x1) {
    const int t = x0;
    x0 = x1;
    x1 = t;
  }
  for (int x = x0; x <= x1; ++x) {
    popup_put(fb, x, y, color);
  }
}

static void popup_vline(ColonizeFramebuffer8* fb, int x, int y0, int y1, uint8_t color) {
  if (y0 > y1) {
    const int t = y0;
    y0 = y1;
    y1 = t;
  }
  for (int y = y0; y <= y1; ++y) {
    popup_put(fb, x, y, color);
  }
}

static void popup_fill_rect(
  ColonizeFramebuffer8* fb, int x0, int y0, int x1, int y1, uint8_t color
) {
  if (!fb || !fb->pixels) {
    return;
  }
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
  for (int y = y0; y <= y1; ++y) {
    uint8_t* row = fb->pixels + y * fb->width;
    for (int x = x0; x <= x1; ++x) {
      row[x] = color;
    }
  }
}

static void popup_tile_rect(
  const ColonizeSpriteSheet* sheet,
  int ox,
  int oy,
  int rw,
  int rh,
  ColonizeFramebuffer8* fb
) {
  if (!sheet || sheet->sprite_count < 1 || !fb || !fb->pixels || rw <= 0 || rh <= 0) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[0];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int x1 = ox + rw;
  const int y1 = oy + rh;
  for (int ty = oy; ty < y1; ty += tile->height) {
    for (int tx = ox; tx < x1; tx += tile->width) {
      for (int sy = 0; sy < tile->height; ++sy) {
        const int fy = ty + sy;
        if (fy < oy || fy >= y1 || fy < 0 || fy >= fb->height) {
          continue;
        }
        for (int sx = 0; sx < tile->width; ++sx) {
          const int fx = tx + sx;
          if (fx < ox || fx >= x1 || fx < 0 || fx >= fb->width) {
            continue;
          }
          const uint8_t color = tile->pixels[sy * tile->width + sx];
          if (color == COLONIZE_SS_TRANSPARENT) {
            continue;
          }
          fb->pixels[fy * fb->width + fx] = color;
        }
      }
    }
  }
}

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
) {
  ColonizePopupColors local;
  if (!colors) {
    popup_colors_from_ui(&local);
    colors = &local;
  }

  if (out_inner_x) {
    *out_inner_x = 0;
  }
  if (out_inner_y) {
    *out_inner_y = 0;
  }
  if (out_inner_w) {
    *out_inner_w = 0;
  }
  if (out_inner_h) {
    *out_inner_h = 0;
  }

  if (!framebuffer || !framebuffer->pixels || w <= 0 || h <= 0) {
    return;
  }

  if (tile && tile->sprite_count > 0) {
    popup_tile_rect(tile, x, y, w, h, framebuffer);
  } else {
    popup_fill_rect(framebuffer, x, y, x + w - 1, y + h - 1, POPUP_FALLBACK_FILL);
  }

  const int x1 = x + w - 1;
  const int y1 = y + h - 1;

  /* Layer 1: outer black. */
  popup_hline(framebuffer, y, x, x1, colors->outer);
  popup_hline(framebuffer, y1, x, x1, colors->outer);
  popup_vline(framebuffer, x, y, y1, colors->outer);
  popup_vline(framebuffer, x1, y, y1, colors->outer);

  if (w >= 3 && h >= 3) {
    /* Layer 2: mid brown, inset 1. */
    const int mx0 = x + 1;
    const int my0 = y + 1;
    const int mx1 = x1 - 1;
    const int my1 = y1 - 1;
    popup_hline(framebuffer, my0, mx0, mx1, colors->mid);
    popup_hline(framebuffer, my1, mx0, mx1, colors->mid);
    popup_vline(framebuffer, mx0, my0, my1, colors->mid);
    popup_vline(framebuffer, mx1, my0, my1, colors->mid);
  }

  if (w >= 5 && h >= 5) {
    /* Layer 3: bevel inset 2 — light top/right, dark bottom/left. */
    const int bx0 = x + 2;
    const int by0 = y + 2;
    const int bx1 = x1 - 2;
    const int by1 = y1 - 2;
    popup_hline(framebuffer, by0, bx0, bx1, colors->light); /* top */
    popup_vline(framebuffer, bx1, by0, by1, colors->light); /* right */
    popup_hline(framebuffer, by1, bx0, bx1, colors->dark); /* bottom */
    popup_vline(framebuffer, bx0, by0, by1, colors->dark); /* left */
  }

  if (w > POPUP_FRAME_INSET * 2 && h > POPUP_FRAME_INSET * 2) {
    if (out_inner_x) {
      *out_inner_x = x + POPUP_FRAME_INSET;
    }
    if (out_inner_y) {
      *out_inner_y = y + POPUP_FRAME_INSET;
    }
    if (out_inner_w) {
      *out_inner_w = w - POPUP_FRAME_INSET * 2;
    }
    if (out_inner_h) {
      *out_inner_h = h - POPUP_FRAME_INSET * 2;
    }
  }
}

void popup_draw_text_shadowed(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  if (!font || !framebuffer || !text || !text[0]) {
    return;
  }
  font_draw_text_unbold(font, framebuffer, x + 1, y + 1, text, 0);
  font_draw_text_unbold(font, framebuffer, x, y, text, color);
}
