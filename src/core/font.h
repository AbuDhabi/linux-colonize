#ifndef COLONIZE_FONT_H
#define COLONIZE_FONT_H

#include <stdbool.h>
#include <stdint.h>

#include "core/ff.h"
#include "platform/platform.h"

void font_draw_text(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
);

/*
 * Like font_draw_text, but colored glyphs only ink 2bpp shade 1 (skip soft AA).
 * Use for thin green captions (FONTINTR). Do not use with FONTKING (body is shade 3).
 */
void font_draw_text_unbold(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
);

/* Like font_draw_text, but '~' marks the next character as a hotkey (hotkey_color). */
void font_draw_text_hotkey(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color,
  uint8_t hotkey_color
);

/*
 * Like font_draw_text, but with full control over each of the 4 baked-in AA
 * shade levels (shade_colors[0], background, is unused — glyph pixels are
 * only ever shade 1-3) instead of font_draw_text's hardcoded white/grey
 * blend for color 15/7 (FF_COLOR_MAP). Use to recolor a font's own soft-AA
 * "shadow" shades — e.g. FONTINTR's built-in grey/brown edge — to something
 * else (solid black, say) without layering on a second, separate shadow
 * pass.
 */
void font_draw_text_shaded(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  const uint8_t shade_colors[4]
);

/* Pixel width of text as drawn (honors '~' / '#' markup; missing FF glyphs use builtin 6px). */
int font_text_width(const ColonizeFont* font, const char* text);

/*
 * Tight ink bounds of a single glyph within its cell (0-based).
 * Returns false if the glyph is missing / empty; on success writes inclusive min/max.
 */
bool font_glyph_ink_bounds(
  const ColonizeFont* font,
  unsigned char ch,
  int* out_min_x,
  int* out_min_y,
  int* out_max_x,
  int* out_max_y
);

#endif
