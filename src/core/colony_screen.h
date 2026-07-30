#ifndef COLONIZE_COLONY_SCREEN_H
#define COLONIZE_COLONY_SCREEN_H

#include <stdbool.h>
#include <stddef.h>

#include "core/colony.h"
#include "core/font.h"
#include "core/map.h"
#include "core/pik.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * DOS colony screen layout (320×200):
 *   WOODPANL.PIK  — full-screen wood chrome (authoritative palette)
 *   PARCH.SS      — beige scrollwork tiled across the upper-left buildings section
 *   BUILDING.SS   — building sprites (indices match NAMES.TXT @BUILDING);
 *                   #16 is the pre-stockade fence; sprites 42–47 are tree clumps
 *   WOODTILE.SS   — wood grain tiled in the square top-right minimap section
 *   TERRAIN+PHYS0 — 3×3 catchment tiles centered in the minimap section
 *   COLONY.PIK    — bottom panel (outside colony / dock / cargo)
 */
#define COLONY_SCREEN_WIDTH 320
#define COLONY_SCREEN_HEIGHT 200
#define COLONY_BOTTOM_PANEL_HEIGHT 72
#define COLONY_BOTTOM_PANEL_Y (COLONY_SCREEN_HEIGHT - COLONY_BOTTOM_PANEL_HEIGHT)

/* Top-right minimap panel: square so L/R WOODTILE margins match T/B. */
#define COLONY_MINIMAP_SECTION_H 128
#define COLONY_MINIMAP_SECTION_W COLONY_MINIMAP_SECTION_H
#define COLONY_MINIMAP_SECTION_X (COLONY_SCREEN_WIDTH - COLONY_MINIMAP_SECTION_W)
#define COLONY_MINIMAP_SECTION_Y 0
#define COLONY_MINIMAP_GRID 3
#define COLONY_MINIMAP_TILE 16

/* Upper-left buildings section — PARCH fills the entire area left of the minimap. */
#define COLONY_VIEWPORT_X 0
#define COLONY_VIEWPORT_Y 0
#define COLONY_VIEWPORT_W COLONY_MINIMAP_SECTION_X
#define COLONY_VIEWPORT_H COLONY_MINIMAP_SECTION_H

typedef struct ColonyScreenView {
  ColonizePikImage frame;          /* WOODPANL.PIK — also supplies screen palette */
  ColonizeSpriteSheet parch;       /* PARCH.SS — buildings ground fill */
  ColonizeSpriteSheet wood_tile;   /* WOODTILE.SS — minimap section fill */
  ColonizeSpriteSheet buildings;   /* BUILDING.SS remapped into WOODPANL indices */
  ColonizePikImage bottom_panel;   /* COLONY.PIK */
  bool frame_ok;
  bool parch_ok;
  bool wood_tile_ok;
  bool buildings_ok;
  bool bottom_panel_ok;
  char status[96];
} ColonyScreenView;

bool colony_screen_load(ColonyScreenView* view, const char* data_dir, char* err, size_t err_size);
void colony_screen_free(ColonyScreenView* view);
void colony_screen_set_status(ColonyScreenView* view, const char* text);

void colony_screen_render(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

#endif
