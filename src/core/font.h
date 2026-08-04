#ifndef COLONIZE_FONT_H
#define COLONIZE_FONT_H

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

/* Pixel width of text as drawn (honors '~' / '#' markup; missing FF glyphs use builtin 6px). */
int font_text_width(const ColonizeFont* font, const char* text);

#endif
