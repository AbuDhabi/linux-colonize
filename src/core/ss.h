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
  /*
   * Per-sprite anchor from the on-disk header (bytes 8 and 10 of the 16-byte
   * record), which DOS keeps at +4 / +6 of its in-memory sprite record. The
   * centering helper FUN_6f30_002e places a sprite at
   * (anchor_x - width/2, anchor_y - height + 1) — i.e. anchor_x is a
   * horizontal centre and anchor_y a bottom baseline, both in screen space.
   */
  int anchor_x;
  int anchor_y;
  uint8_t* pixels;
} ColonizeSprite;

typedef struct ColonizeSpriteSheet {
  ColonizeSprite* sprites;
  int sprite_count;
  ColonizePalette palette;
  bool has_palette;
  /*
   * Sheet-level popup-placement words from the 0x98-byte section-0 header
   * (bytes 0x0e/0x10/0x12; DOS keeps them at +0x10/+0x12/+0x14 of the loaded
   * picture object). Only the MSSn/MYRn popup decorations set them: the DOS
   * popup compositor (FUN_6f74_14c6) draws the sprite above the dialog with
   * its bottom overlapping the dialog top by place_offset_y px;
   * place_mode 0 = sprite at the dialog's left (horizontal overlap
   * place_offset_x), 1 = sprite centred over the dialog, 2 = at the right
   * (overlap place_offset_x). All zero for ordinary sheets.
   */
  int place_offset_y;
  int place_mode;
  int place_offset_x;
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
/* Like ss_blit_sprite, but every opaque pixel is written as replace_color (shadow underlay). */
void ss_blit_sprite_color(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y,
  uint8_t replace_color
);
/* Copy sprite pixels only onto framebuffer cells that currently equal match_color (MAPEDIT masked terrain). */
void ss_blit_sprite_where_dest(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y,
  uint8_t match_color
);

#endif
