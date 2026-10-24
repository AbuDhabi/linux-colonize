#ifndef COLONIZE_FONT_H
#define COLONIZE_FONT_H

#include <stdbool.h>
#include <stdint.h>

#include "core/font_file.h"
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

/*
 * font_draw_text for port-only surfaces (debug overlays, the window log strip): a char the
 * face lacks draws as the builtin 5x7 glyph with a 6px advance. Every DOS-drawn text path
 * uses the others, which draw such a char as nothing with no advance (FUN_1c11_000c,
 * port_bugs.md #1229). Measure with font_text_width_port.
 */
void font_draw_text_port(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
);

/*
 * One glyph through the shade map with no marker meaning ('~', '#', '\n' are glyphs
 * too): the escaped character of FUN_6f74_0538's '~' (6f74:05be). Returns the
 * advance; framebuffer NULL = measure only.
 */
int font_draw_glyph_shaded(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  unsigned char ch,
  const uint8_t shade_colors[4]
);

/*
 * Marker sets for font_text_width_skip. The six private width loops retired on
 * 2026-09-14 each skipped a different set: font_text_width and
 * map_menu_text_width skip the hotkey markers, game_loop's
 * begin_menu_text_width and popup's markup measure skip the emphasis braces,
 * new_game's prose measure also skips the GAME.TXT layout directives, while
 * the old pedia/colony_screen width loops skipped nothing (so a '~' in a
 * label widened those strings). Pick the set the string actually carries.
 */
#define FONT_SKIP_NONE ""
#define FONT_SKIP_HOTKEY "~#"
#define FONT_SKIP_BRACES "{}"
#define FONT_SKIP_MARKUP "~#{}"
#define FONT_SKIP_LAYOUT "{}^_"

/* Pixel width of text as drawn, stopping at the first '\n'. Characters in
 * `skipset` (a NUL-terminated set, may be NULL/empty) cost nothing, so do chars
 * the face lacks (DOS); with no face loaded every char is the builtin 6px cell. */
int font_text_width_skip(const ColonizeFont* font, const char* text, const char* skipset);

/* font_text_width_skip with FONT_SKIP_HOTKEY. */
int font_text_width(const ColonizeFont* font, const char* text);

/* font_text_width as font_draw_text_port draws: a lacking char costs 6px. */
int font_text_width_port(const ColonizeFont* font, const char* text);

/*
 * Tight ink bounds of a single glyph within its cell (0-based).
 * Returns false if the glyph is missing from a loaded face or empty; with no face it
 * reports the builtin 5x7 glyph. On success writes inclusive min/max.
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
