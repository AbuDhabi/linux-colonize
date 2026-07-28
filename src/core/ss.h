#ifndef COLONIZE_SS_H
#define COLONIZE_SS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/platform.h"

#define COLONIZE_SS_TRANSPARENT 0xFD

typedef struct ColonizeSprite {
  int width;
  int height;
  uint8_t* pixels;
} ColonizeSprite;

typedef struct ColonizeSpriteSheet {
  ColonizeSprite* sprites;
  int sprite_count;
  ColonizePalette palette;
  bool has_palette;
} ColonizeSpriteSheet;

bool ss_load(const char* path, ColonizeSpriteSheet* out_sheet, char* err, size_t err_size);
void ss_free(ColonizeSpriteSheet* sheet);
void ss_blit_sprite(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y
);

#endif
