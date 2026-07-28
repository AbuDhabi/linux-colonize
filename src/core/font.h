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

#endif
