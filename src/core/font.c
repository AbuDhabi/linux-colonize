#include "core/font.h"

#include <stdbool.h>
#include <string.h>

#include "core/fb.h"

/* Tiny 5x7 glyphs for printable ASCII 32..126 (packed as 5 columns, LSB top). */
static const uint8_t FONT5X7[95][5] = {
  {0x00,0x00,0x00,0x00,0x00}, /* space */
  {0x00,0x00,0x5F,0x00,0x00}, /* ! */
  {0x00,0x07,0x00,0x07,0x00},
  {0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12},
  {0x23,0x13,0x08,0x64,0x62},
  {0x36,0x49,0x55,0x22,0x50},
  {0x00,0x05,0x03,0x00,0x00},
  {0x00,0x1C,0x22,0x41,0x00},
  {0x00,0x41,0x22,0x1C,0x00},
  {0x14,0x08,0x3E,0x08,0x14},
  {0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00},
  {0x08,0x08,0x08,0x08,0x08},
  {0x00,0x60,0x60,0x00,0x00},
  {0x20,0x10,0x08,0x04,0x02},
  {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
  {0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46},
  {0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},
  {0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30},
  {0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},
  {0x06,0x49,0x49,0x29,0x1E},
  {0x00,0x36,0x36,0x00,0x00},
  {0x00,0x56,0x36,0x00,0x00},
  {0x08,0x14,0x22,0x41,0x00},
  {0x14,0x14,0x14,0x14,0x14},
  {0x00,0x41,0x22,0x14,0x08},
  {0x02,0x01,0x51,0x09,0x06},
  {0x32,0x49,0x79,0x41,0x3E},
  {0x7E,0x11,0x11,0x11,0x7E}, /* A */
  {0x7F,0x49,0x49,0x49,0x36},
  {0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},
  {0x7F,0x49,0x49,0x49,0x41},
  {0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A},
  {0x7F,0x08,0x08,0x08,0x7F},
  {0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01},
  {0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F},
  {0x7F,0x04,0x08,0x10,0x7F},
  {0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},
  {0x3E,0x41,0x51,0x21,0x5E},
  {0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},
  {0x01,0x01,0x7F,0x01,0x01},
  {0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F},
  {0x3F,0x40,0x38,0x40,0x3F},
  {0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07},
  {0x61,0x51,0x49,0x45,0x43},
  {0x00,0x7F,0x41,0x41,0x00},
  {0x02,0x04,0x08,0x10,0x20},
  {0x00,0x41,0x41,0x7F,0x00},
  {0x04,0x02,0x01,0x02,0x04},
  {0x40,0x40,0x40,0x40,0x40},
  {0x00,0x01,0x02,0x04,0x00},
  {0x20,0x54,0x54,0x54,0x78}, /* a */
  {0x7F,0x48,0x44,0x44,0x38},
  {0x38,0x44,0x44,0x44,0x20},
  {0x38,0x44,0x44,0x48,0x7F},
  {0x38,0x54,0x54,0x54,0x18},
  {0x08,0x7E,0x09,0x01,0x02},
  {0x0C,0x52,0x52,0x52,0x3E},
  {0x7F,0x08,0x04,0x04,0x78},
  {0x00,0x44,0x7D,0x40,0x00},
  {0x20,0x40,0x44,0x3D,0x00},
  {0x7F,0x10,0x28,0x44,0x00},
  {0x00,0x41,0x7F,0x40,0x00},
  {0x7C,0x04,0x18,0x04,0x78},
  {0x7C,0x08,0x04,0x04,0x78},
  {0x38,0x44,0x44,0x44,0x38},
  {0x7C,0x14,0x14,0x14,0x08},
  {0x08,0x14,0x14,0x18,0x7C},
  {0x7C,0x08,0x04,0x04,0x08},
  {0x48,0x54,0x54,0x54,0x24},
  {0x04,0x3F,0x44,0x40,0x20},
  {0x3C,0x40,0x40,0x20,0x7C},
  {0x1C,0x20,0x40,0x20,0x1C},
  {0x3C,0x40,0x30,0x40,0x3C},
  {0x44,0x28,0x10,0x28,0x44},
  {0x0C,0x50,0x50,0x50,0x3C},
  {0x44,0x64,0x54,0x4C,0x44},
  {0x00,0x08,0x36,0x41,0x00},
  {0x00,0x00,0x7F,0x00,0x00},
  {0x00,0x41,0x36,0x08,0x00},
  {0x10,0x08,0x08,0x10,0x08}
};

/*
 * White/gray AA map for ink 15 or 7 (unbold white). Other colors paint every
 * non-zero shade as solid ink (FONTKING stores body as shade 3).
 */
static const uint8_t FF_COLOR_MAP[4] = {0, 0x0F, 0x07, 0x08};

static void draw_ff_glyph(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  unsigned char ch,
  uint8_t color,
  bool unbold_colored,
  const uint8_t* shade_colors
) {
  if (ch >= 128 || font->char_widths[ch] == 0) {
    return;
  }

  const int width = font->char_widths[ch];
  const int height = font->max_height;
  size_t pos = font->char_offsets[ch];
  if (pos >= font->section_size) {
    return;
  }

  for (int row = 0; row < height; ++row) {
    int col = 0;
    while (col < width) {
      if (pos >= font->section_size) {
        return;
      }
      const uint8_t byte = font->section_data[pos++];
      for (int shift = 6; shift >= 0; shift -= 2) {
        const uint8_t shade = (uint8_t)((byte >> shift) & 0x03u);
        if (shade != 0) {
          uint8_t pixel;
          if (shade_colors) {
            /* Caller-supplied per-shade palette — bypasses the color==15/7
             * hardcoded FF_COLOR_MAP AA blend entirely (font_draw_text_shaded). */
            pixel = shade_colors[shade];
          } else if (color == 15 || color == 7) {
            pixel = FF_COLOR_MAP[shade];
          } else if (unbold_colored && shade != 1) {
            /* Skip soft AA shades — thin colored captions (FONTINTR). */
            col += 1;
            if (col >= width) {
              break;
            }
            continue;
          } else {
            pixel = color;
          }
          fb_put(framebuffer, x + col, y + row, pixel);
        }
        col += 1;
        if (col >= width) {
          break;
        }
      }
    }
  }
}

static void draw_builtin_glyph(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  unsigned char ch,
  uint8_t color
) {
  if (ch < 32 || ch > 126) {
    ch = '?';
  }
  const uint8_t* glyph = FONT5X7[ch - 32];
  for (int col = 0; col < 5; ++col) {
    uint8_t bits = glyph[col];
    for (int row = 0; row < 7; ++row) {
      if (bits & (1u << row)) {
        fb_put(framebuffer, x + col, y + row, color);
      }
    }
  }
}

/* DOS-LITERAL FUN_1c11_000c raw 17468-17471 (the glyph loop of every text writer, 6f74
 * compositor and FUN_104b_* alike): a char whose face width byte is 0 draws nothing and
 * advances 0. FONTTINY has no *<=>@\^_`{|}~, FONTSMAL no punctuation but . and #, FONTINTR
 * no \^`{|}~ (port_bugs.md #1225, #1229). Chars >= 128 (DOS: >= 0x81 end the string) are
 * treated the same. The builtin 5x7 cell stands in only when no face is loaded, or with
 * port_fallback for the port-only surfaces (font_draw_text_port). */
static int font_face_advance(const ColonizeFont* font, unsigned char ch, bool port_fallback) {
  if (font && font->section_data) {
    const int w = ch < 128 ? font->char_widths[ch] : 0;
    if (w != 0 || !port_fallback) {
      return w;
    }
  }
  return 6;
}

static int font_text_width_run(
  const ColonizeFont* font, const char* text, const char* skipset, bool port_fallback
) {
  if (!text) {
    return 0;
  }
  int width = 0;
  for (const char* p = text; *p; ++p) {
    unsigned char ch = (unsigned char)*p;
    if (ch == '\n') {
      break;
    }
    if (skipset && strchr(skipset, (char)ch)) {
      continue;
    }
    width += font_face_advance(font, ch, port_fallback);
  }
  return width;
}

int font_text_width_skip(const ColonizeFont* font, const char* text, const char* skipset) {
  return font_text_width_run(font, text, skipset, false);
}

int font_text_width(const ColonizeFont* font, const char* text) {
  return font_text_width_skip(font, text, FONT_SKIP_HOTKEY);
}

int font_text_width_port(const ColonizeFont* font, const char* text) {
  return font_text_width_run(font, text, FONT_SKIP_HOTKEY, true);
}

bool font_glyph_ink_bounds(
  const ColonizeFont* font,
  unsigned char ch,
  int* out_min_x,
  int* out_min_y,
  int* out_max_x,
  int* out_max_y
) {
  if (!out_min_x || !out_min_y || !out_max_x || !out_max_y) {
    return false;
  }
  int min_x = 0;
  int min_y = 0;
  int max_x = -1;
  int max_y = -1;
  bool any = false;

  if (font && font->section_data && ch < 128 && font->char_widths[ch] != 0) {
    const int width = font->char_widths[ch];
    const int height = font->max_height;
    size_t pos = font->char_offsets[ch];
    min_x = width;
    min_y = height;
    for (int row = 0; row < height; ++row) {
      int col = 0;
      while (col < width) {
        if (pos >= font->section_size) {
          break;
        }
        const uint8_t byte = font->section_data[pos++];
        for (int shift = 6; shift >= 0; shift -= 2) {
          const uint8_t shade = (uint8_t)((byte >> shift) & 0x03u);
          if (shade != 0) {
            if (col < min_x) {
              min_x = col;
            }
            if (col > max_x) {
              max_x = col;
            }
            if (row < min_y) {
              min_y = row;
            }
            if (row > max_y) {
              max_y = row;
            }
            any = true;
          }
          col += 1;
          if (col >= width) {
            break;
          }
        }
      }
    }
  } else if (!(font && font->section_data)) {
    if (ch < 32 || ch > 126) {
      ch = '?';
    }
    const uint8_t* glyph = FONT5X7[ch - 32];
    min_x = 5;
    min_y = 7;
    for (int col = 0; col < 5; ++col) {
      uint8_t bits = glyph[col];
      for (int row = 0; row < 7; ++row) {
        if (bits & (1u << row)) {
          if (col < min_x) {
            min_x = col;
          }
          if (col > max_x) {
            max_x = col;
          }
          if (row < min_y) {
            min_y = row;
          }
          if (row > max_y) {
            max_y = row;
          }
          any = true;
        }
      }
    }
  }

  if (!any) {
    return false;
  }
  *out_min_x = min_x;
  *out_min_y = min_y;
  *out_max_x = max_x;
  *out_max_y = max_y;
  return true;
}

void font_draw_text(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  font_draw_text_hotkey(font, framebuffer, x, y, text, color, color);
}

/* One glyph, no marker handling; returns the advance. framebuffer NULL = measure only. */
static int font_draw_glyph(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  unsigned char ch,
  uint8_t color,
  bool unbold_colored,
  const uint8_t* shade_colors,
  bool port_fallback
) {
  const int advance = font_face_advance(font, ch, port_fallback);
  if (!framebuffer || advance == 0) {
    return advance;
  }
  if (font && font->section_data && ch < 128 && font->char_widths[ch] != 0) {
    draw_ff_glyph(font, framebuffer, x, y, ch, color, unbold_colored, shade_colors);
  } else {
    draw_builtin_glyph(framebuffer, x, y, ch, color);
  }
  return advance;
}

int font_draw_glyph_shaded(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  unsigned char ch,
  const uint8_t shade_colors[4]
) {
  return font_draw_glyph(
    font, shade_colors ? framebuffer : NULL, x, y, ch, shade_colors ? shade_colors[1] : 0, false,
    shade_colors, false
  );
}

/*
 * IN-15: font_draw_text, _unbold, _hotkey and _shaded were four copies of one
 * glyph loop (newline step, marker skip, FF-vs-builtin dispatch, advance),
 * differing only in the draw_ff_glyph arguments. One run here; the four public
 * names stay as wrappers. hotkey_color < 0 means '~' is a plain skip rather
 * than a hotkey marker, which is what _unbold and _shaded did.
 */
static void font_draw_run(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color,
  int hotkey_color,
  bool unbold_colored,
  const uint8_t* shade_colors,
  bool port_fallback
) {
  if (!framebuffer || !text) {
    return;
  }
  const int line_step = font ? (font->max_height + 2) : 8;
  int cx = x;
  bool next_hotkey = false;
  for (const char* p = text; *p; ++p) {
    unsigned char ch = (unsigned char)*p;
    if (ch == '\n') {
      y += line_step;
      cx = x;
      next_hotkey = false;
      continue;
    }
    if (ch == '~') {
      if (hotkey_color >= 0) {
        next_hotkey = true;
      }
      continue;
    }
    if (ch == '#') {
      continue;
    }

    const uint8_t use = next_hotkey ? (uint8_t)hotkey_color : color;
    next_hotkey = false;
    cx += font_draw_glyph(
      font, framebuffer, cx, y, ch, use, unbold_colored, shade_colors, port_fallback
    );
  }
}

void font_draw_text_unbold(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  font_draw_run(font, framebuffer, x, y, text, color, -1, true, NULL, false);
}

void font_draw_text_hotkey(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color,
  uint8_t hotkey_color
) {
  font_draw_run(font, framebuffer, x, y, text, color, (int)hotkey_color, false, NULL, false);
}

void font_draw_text_shaded(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  const uint8_t shade_colors[4]
) {
  if (!shade_colors) {
    return;
  }
  font_draw_run(font, framebuffer, x, y, text, shade_colors[1], -1, false, shade_colors, false);
}

void font_draw_text_port(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  font_draw_run(font, framebuffer, x, y, text, color, color, false, NULL, true);
}
