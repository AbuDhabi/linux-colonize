#ifndef COLONIZE_MAP_PANEL_H
#define COLONIZE_MAP_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/font.h"
#include "core/map.h"
#include "core/map_menu.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * Main-map right sidebar + scrolling 1:1-tile minimap (DOS layout).
 *
 *   Menu bar:     y = 0 .. MAP_MENU_BAR_H-1  (8px; black rule on last row)
 *   Map viewport: x = 0 .. MAP_PANEL_X-1     (240px = 15×16 tiles; 12 rows)
 *   Right panel:  x = MAP_PANEL_X .. 319     (80px)
 *
 * Minimap is a scrolling window (MAP_PANEL_MINIMAP_W × MAP_PANEL_MINIMAP_H).
 */
#define MAP_PANEL_X 240
#define MAP_PANEL_W (320 - MAP_PANEL_X)
#define MAP_VIEW_W MAP_PANEL_X
#define MAP_VIEW_TILE_W 16
#define MAP_VIEW_TILE_H 16
#define MAP_VIEW_TILE_COLS (MAP_VIEW_W / MAP_VIEW_TILE_W)
#define MAP_VIEW_ORIGIN_Y MAP_MENU_BAR_H
#define MAP_VIEW_TILE_ROWS ((200 - MAP_MENU_BAR_H) / MAP_VIEW_TILE_H)
#define MAP_VIEW_H (MAP_VIEW_TILE_ROWS * MAP_VIEW_TILE_H)

#define MAP_PANEL_MINIMAP_W 56
#define MAP_PANEL_MINIMAP_H 39
/* Terrain sits 1px below the menu black rule so the brown top border touches it. */
#define MAP_PANEL_MINIMAP_ORIGIN_Y (MAP_MENU_BAR_H + 1)
#define MAP_PANEL_TEXT_MARGIN 2

typedef struct MapPanel {
  ColonizeSpriteSheet wood_tile;
  ColonizeSpriteSheet nameplat;
  bool wood_ok;
  bool nameplat_ok;
  char label_moves[32];
  char label_locat[32];
  char label_with[32];
} MapPanel;

bool map_panel_load(MapPanel* panel, const char* data_dir, const ColonizeMsgCatalog* labels);
void map_panel_free(MapPanel* panel);

bool map_panel_contains_xy(int mouse_x, int mouse_y);

/*
 * FUN_6ba1_000c (zoom 0): top-left tile of the main viewport centered on
 * (center_x, center_y), clamped so the 1-tile map rim is never drawn.
 * Origin ∈ [1 .. map_size − view_size − 1] when the interior fits the view.
 */
void map_panel_clamp_view_origin(
  int map_w,
  int map_h,
  int center_x,
  int center_y,
  int view_cols,
  int view_rows,
  int* out_view_x,
  int* out_view_y
);

void map_panel_minimap_rect(
  const ColonizeWorldMap* map,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h,
  int* out_origin_x,
  int* out_origin_y
);

bool map_panel_minimap_click(
  const ColonizeWorldMap* map,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int mouse_x,
  int mouse_y,
  int* out_tile_x,
  int* out_tile_y
);

void map_panel_render(
  const MapPanel* panel,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeSpriteSheet* icons,
  const ColonizeFont* font,
  const ColonizeMsgCatalog* names,
  const ColonizeMsgCatalog* labels,
  const ColonizeCol1Save* col1,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int cursor_x,
  int cursor_y,
  int selected_unit_id,
  int human_nation,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  int tax_percent,
  const char* nation_name,
  const ColonizePalette* active_palette,
  /* Player-requested: flashing "End Turn" prompt in the sidebar once no
   * more units need orders this turn (View Pieces mode). Caller passes
   * false whenever the prompt shouldn't show at all (units still pending,
   * a dialog is up, ...) and toggles true/false on its own timer for the
   * blink while it should. */
  bool end_turn_flash_on,
  ColonizeFramebuffer8* framebuffer
);

/* Indian village markers on the main map viewport (ICONS.SS #10–13 by tech). */
void map_panel_render_tribes_on_map(
  const ColonizeCol1Save* col1,
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* framebuffer,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  const ColonizeWorldMap* fog_map,
  int fog_nation
);

void map_panel_tile_rect(
  const ColonizeSpriteSheet* sheet,
  int origin_x,
  int origin_y,
  int rect_w,
  int rect_h,
  ColonizeFramebuffer8* framebuffer
);

#endif
