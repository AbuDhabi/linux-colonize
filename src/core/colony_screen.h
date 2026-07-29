#ifndef COLONIZE_COLONY_SCREEN_H
#define COLONIZE_COLONY_SCREEN_H

#include <stdbool.h>
#include <stddef.h>

#include "core/colony.h"
#include "core/font.h"
#include "core/map.h"
#include "core/pik.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * DOS colony screen layout (320×200):
 *   WOODPANL.PIK  — full-screen wood chrome (authoritative palette)
 *   WOODFRAM.SS   — recessed frame around the building viewport
 *   COLONY.PIK    — bottom panel (outside colony / dock / cargo), no embedded palette
 *   TERRAIN.SS    — 3×3 catchment minimap in the right strip
 */
#define COLONY_SCREEN_WIDTH 320
#define COLONY_SCREEN_HEIGHT 200
#define COLONY_BOTTOM_PANEL_HEIGHT 72
#define COLONY_BOTTOM_PANEL_Y (COLONY_SCREEN_HEIGHT - COLONY_BOTTOM_PANEL_HEIGHT)
#define COLONY_MINIMAP_GRID 3
#define COLONY_MINIMAP_TILE 15
#define COLONY_MINIMAP_X 274
#define COLONY_MINIMAP_Y 16

typedef struct ColonyScreenView {
  ColonizePikImage frame;          /* WOODPANL.PIK — also supplies screen palette */
  ColonizeSpriteSheet wood_frame;  /* WOODFRAM.SS remapped into WOODPANL indices */
  ColonizePikImage bottom_panel;   /* COLONY.PIK */
  bool frame_ok;
  bool wood_frame_ok;
  bool bottom_panel_ok;
  char status[96];
} ColonyScreenView;

bool colony_screen_load(ColonyScreenView* view, const char* data_dir, char* err, size_t err_size);
void colony_screen_free(ColonyScreenView* view);
void colony_screen_set_status(ColonyScreenView* view, const char* text);

void colony_screen_render(
  const ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

#endif
