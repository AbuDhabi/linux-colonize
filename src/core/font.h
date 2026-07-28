#ifndef COLONIZE_FONT_H
#define COLONIZE_FONT_H

#include <stdint.h>

#include "platform/platform.h"

void font_draw_text(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
);

#endif
