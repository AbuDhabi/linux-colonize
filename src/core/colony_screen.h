#ifndef COLONIZE_COLONY_SCREEN_H
#define COLONIZE_COLONY_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/font.h"
#include "core/map.h"
#include "core/pik.h"
#include "core/popup.h"
#include "core/ss.h"
#include "core/turn.h"
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

#define COLONY_CONSTRUCTION_BANNER_Y (COLONY_MIDDLE_Y + 1)
#define COLONY_CONSTRUCTION_BANNER_H 10
#define COLONY_EXIT_X 305
#define COLONY_BUILDABLE_MAX 32
#define COLONY_BUILDING_SLOT_W 48
#define COLONY_BUILDING_SLOT_H 32
#define COLONY_COLONIST_LIST_X 8
#define COLONY_COLONIST_LIST_Y0 (COLONY_BOTTOM_PANEL_Y + 14)
#define COLONY_COLONIST_ROW_H 9
#define COLONY_JOB_LIST_MAX COLONIZE_FIELD_JOB_COUNT

typedef enum ColonyScreenHit {
  COLONY_HIT_NONE = 0,
  COLONY_HIT_COLONIST,
  COLONY_HIT_BUILDING,
  COLONY_HIT_CONSTRUCTION_BANNER,
  COLONY_HIT_CONSTRUCTION_ROW,
  COLONY_HIT_CONSTRUCTION_CLEAR,
  COLONY_HIT_EXIT,
  COLONY_HIT_CONSTRUCTION_OUTSIDE,
  COLONY_HIT_AREA_TILE,
  COLONY_HIT_JOBS_ROW,
  COLONY_HIT_JOBS_CLEAR,
  COLONY_HIT_JOBS_OUTSIDE
} ColonyScreenHit;

typedef struct ColonyScreenHitResult {
  ColonyScreenHit kind;
  int index; /* colonist, building slot, or construction list row */
} ColonyScreenHitResult;

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

  int selected_colonist; /* -1 none */
  bool construction_open;
  int construction_selection;
  int buildable_ids[COLONY_BUILDABLE_MAX];
  int buildable_count;
  /* Cached layout for construction popup hit-testing. */
  int construction_dialog_x;
  int construction_dialog_y;
  int construction_dialog_w;
  int construction_dialog_h;
  int construction_list_y0;
  int construction_line_h;

  bool jobs_open;
  int jobs_tile_index; /* field tile being assigned */
  int jobs_selection;
  int job_ids[COLONY_JOB_LIST_MAX];
  int job_count;
  int jobs_dialog_x;
  int jobs_dialog_y;
  int jobs_dialog_w;
  int jobs_dialog_h;
  int jobs_list_y0;
  int jobs_line_h;

  ColonizeColonyProdDelta last_delta;
  bool last_delta_valid;
} ColonyScreenView;

bool colony_screen_load(ColonyScreenView* view, const char* data_dir, char* err, size_t err_size);
void colony_screen_free(ColonyScreenView* view);
void colony_screen_set_status(ColonyScreenView* view, const char* text);
void colony_screen_reset_ui(ColonyScreenView* view);
void colony_screen_set_delta(ColonyScreenView* view, const ColonizeColonyProdDelta* delta);

void colony_screen_open_construction(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  int colony_id
);
void colony_screen_close_construction(ColonyScreenView* view);

void colony_screen_open_jobs(
  ColonyScreenView* view,
  const ColonizeWorldMap* map,
  const ColonizeColony* colony,
  int tile_index
);
void colony_screen_close_jobs(ColonyScreenView* view);

/* Pixel origin of the 3×3 area grid (for hit-tests / overlays). */
void colony_screen_minimap_origin(int* out_x, int* out_y);

/* Hit-test at framebuffer coords. Jobs/construction popups capture input when open. */
ColonyScreenHitResult colony_screen_hit_test(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int mx,
  int my
);

void colony_screen_render(
  ColonyScreenView* view,
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
