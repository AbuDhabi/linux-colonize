#ifndef COLONIZE_COLONY_SCREEN_H
#define COLONIZE_COLONY_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
 *                   #16 fence (bottom-right); #45 empty coast above fence;
 *                   sprites 42–47 are tree clumps
 *   WOODTILE.SS   — wood grain tiled in the square top-right minimap section
 *   TERRAIN+PHYS0 — 3×3 catchment tiles centered in the minimap section
 *   COLONY.PIK    — bottom panel (outside colony / dock / cargo)
 *   ICONS.SS      — cargo icons 22..37 for the bottom warehouse strip
 */
#define COLONY_SCREEN_WIDTH 320
#define COLONY_SCREEN_HEIGHT 200
#define COLONY_BOTTOM_PANEL_HEIGHT 72
#define COLONY_BOTTOM_PANEL_Y (COLONY_SCREEN_HEIGHT - COLONY_BOTTOM_PANEL_HEIGHT)

/*
 * DOS-style colony screen sections:
 *   top bar (name/date/gold), 1px black separator, middle (buildings+minimap),
 *   1px black separator, then COLONY.PIK bottom panel.
 */
#define COLONY_TOP_BAR_H 11
#define COLONY_TOP_SEPARATOR_Y COLONY_TOP_BAR_H
#define COLONY_MIDDLE_Y (COLONY_TOP_SEPARATOR_Y + 1)
#define COLONY_BOTTOM_SEPARATOR_Y (COLONY_BOTTOM_PANEL_Y - 1)

/* Top-right minimap panel (square in the middle band, right of a 1px separator). */
#define COLONY_MINIMAP_SECTION_H (COLONY_BOTTOM_SEPARATOR_Y - COLONY_MIDDLE_Y)
#define COLONY_MINIMAP_SECTION_W COLONY_MINIMAP_SECTION_H
#define COLONY_MINIMAP_SECTION_X (COLONY_SCREEN_WIDTH - COLONY_MINIMAP_SECTION_W)
#define COLONY_MINIMAP_SECTION_Y COLONY_MIDDLE_Y
#define COLONY_MINIMAP_GRID 3
#define COLONY_MINIMAP_TILE 16

/* Upper-left buildings section — PARCH fills the middle band left of minimap. */
#define COLONY_VIEWPORT_X 0
#define COLONY_VIEWPORT_Y COLONY_MIDDLE_Y
#define COLONY_VIEWPORT_W (COLONY_MINIMAP_SECTION_X - 1) /* leave 1px black separator */
#define COLONY_VIEWPORT_H (COLONY_BOTTOM_SEPARATOR_Y - COLONY_MIDDLE_Y)

/*
 * COLONY.PIK cargo strip (measured from empty slots in the asset):
 *   16 slots of 18px with 1px dividers (pitch 19), starting at x=1;
 *   fill rows y=52..70; Exit button occupies x=305..319.
 *   ICONS.SS #22..#37 (12px tall) sit at the top of each slot; amounts below.
 */
#define COLONY_CARGO_SLOT_X0 1
#define COLONY_CARGO_SLOT_W 18
#define COLONY_CARGO_PITCH 19
#define COLONY_CARGO_ICON_BASE 22
#define COLONY_CARGO_STRIP_Y (COLONY_BOTTOM_PANEL_Y + 52)
#define COLONY_CARGO_NUM_Y (COLONY_BOTTOM_PANEL_Y + 64)

typedef struct ColonyScreenView {
  ColonizePikImage frame;          /* WOODPANL.PIK — also supplies screen palette */
  ColonizeSpriteSheet parch;       /* PARCH.SS — buildings ground fill */
  ColonizeSpriteSheet wood_tile;   /* WOODTILE.SS — minimap section fill */
  ColonizeSpriteSheet buildings;   /* BUILDING.SS remapped into WOODPANL indices */
  ColonizeSpriteSheet icons;       /* ICONS.SS remapped — cargo strip icons */
  ColonizePikImage bottom_panel;   /* COLONY.PIK */
  bool frame_ok;
  bool parch_ok;
  bool wood_tile_ok;
  bool buildings_ok;
  bool icons_ok;
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
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

#endif
