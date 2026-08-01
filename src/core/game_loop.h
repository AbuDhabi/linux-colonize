#ifndef COLONIZE_GAME_LOOP_H
#define COLONIZE_GAME_LOOP_H

#include <stdbool.h>
#include <stdint.h>

#include "platform/platform.h"

typedef struct ColonizeGameConfig {
  const char* data_dir;
  const char* save_dir;
} ColonizeGameConfig;

typedef struct ColonizeGameState ColonizeGameState;

ColonizeGameState* game_create(const ColonizeGameConfig* config);
void game_destroy(ColonizeGameState* game);
bool game_update(ColonizeGameState* game, const ColonizeInputState* input, uint32_t dt_ms);
void game_render(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette);
const char* game_status_text(const ColonizeGameState* game);
/* Build/toggle SDL mouse cursor from CURSOR.SS when the pointer is over the 320x200 frame. */
void game_apply_mouse_cursor(
  ColonizeGameState* game,
  ColonizePlatform* platform,
  int mouse_x,
  int mouse_y
);

#endif
