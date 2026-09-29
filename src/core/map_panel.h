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
#include "core/screen_geom.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/platform.h"
#include "core/world.h"

/*
 * Main-map right sidebar + scrolling 1:1-tile minimap (DOS layout, port-resized).
 *
 * DOS is a fixed 320x200; the port lets the main map view grow with the window
 * (core/screen_geom.h). Everything below is derived from the live logical size,
 * and evaluates to the DOS literals at 320x200:
 *
 *   Menu bar:     y = 0 .. MAP_MENU_BAR_H-1    (8px; black rule on last row)
 *   Map viewport: x = 0 .. MAP_PANEL_X-1       (240px = 15x16 tiles; 12 rows)
 *   Right panel:  x = MAP_PANEL_X .. w-1       (80px, flush at the right edge)
 *
 * The panel keeps its 80px width and grows downwards; the viewport takes the
 * rest. Minimap is a scrolling window (MAP_PANEL_MINIMAP_W x MAP_PANEL_MINIMAP_H)
 * and does not resize.
 *
 * NOTE: MAP_PANEL_X / MAP_VIEW_* are function calls, not integer constant
 * expressions — use the _MAX forms for array dimensions and static initialisers.
 */
#define MAP_PANEL_W 80
#define MAP_PANEL_X (screen_geom_w() - MAP_PANEL_W)
#define MAP_VIEW_W MAP_PANEL_X
#define MAP_VIEW_TILE_W 16
#define MAP_VIEW_TILE_H 16
#define MAP_VIEW_ORIGIN_Y MAP_MENU_BAR_H
#define MAP_VIEW_H (screen_geom_h() - MAP_MENU_BAR_H)
/* Round up: a partial tile at the right/bottom edge is clipped, not dropped. */
#define MAP_VIEW_TILE_COLS ((MAP_VIEW_W + MAP_VIEW_TILE_W - 1) / MAP_VIEW_TILE_W)
#define MAP_VIEW_TILE_ROWS ((MAP_VIEW_H + MAP_VIEW_TILE_H - 1) / MAP_VIEW_TILE_H)
/* Compile-time maxima, for buffer sizing only. */
#define MAP_VIEW_W_MAX (SCREEN_MAX_W - MAP_PANEL_W)
#define MAP_VIEW_H_MAX (SCREEN_MAX_H - MAP_MENU_BAR_H)

#define MAP_PANEL_MINIMAP_W 56
#define MAP_PANEL_MINIMAP_H 39
/* Terrain sits 1px below the menu black rule so the brown top border touches it. */
#define MAP_PANEL_MINIMAP_ORIGIN_Y (MAP_MENU_BAR_H + 1)
#define MAP_PANEL_TEXT_MARGIN 2

typedef struct MapPanel {
  ColonizeSpriteSheet wood_tile;
  bool wood_ok;
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

/*
 * The stack-row type-detail string (Pioneers' tools / Treasure gold) exactly as
 * FUN_49dd_0424's stack list spells it: no parentheses, and the LABELS @MISC
 * row 4 "Expert" word in front when the profession byte is 0x14 (bugs.md #630).
 * Exposed for tests; the selected-unit block uses the parenthesised spelling.
 */
bool map_panel_stack_detail_text(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u,
  const ColonizeMsgCatalog* names,
  char* out,
  size_t out_size
);

void map_panel_render_w(
  const ColonizeWorld* w,
  const MapPanel* panel,
  const ColonizeSpriteSheet* icons,
  const ColonizeFont* font,
  const ColonizeMsgCatalog* names,
  const ColonizeMsgCatalog* labels,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int cursor_x,
  int cursor_y,
  int selected_unit_id,
  int fog_nation,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  int tax_percent,
  const char* nation_name,
  const ColonizePalette* active_palette,
  bool end_turn_active,
  bool end_turn_blink_white,
  ColonizeFramebuffer8* framebuffer
);
/* Indian village markers on the main map viewport (ICONS.SS #10–13 by tech). */
void map_panel_render_tribes_on_map_w(
  const ColonizeWorld* w,
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
  int fog_nation,
  /* Active output palette (TERRAIN.SS for the map): the mission cross and
   * the other-European alarm mark are drawn in nation shades whose raw DOS
   * indices are ICONS.SS-native — NULL keeps the raw indices (bugs.md #364). */
  const ColonizePalette* active_palette
);

/*
 * Units the sidebar lists for a tile, in DOS stack order (FUN_1427_04d6 with
 * param_2 == 0: transports first, then Treasure, then descending @UNIT size
 * class). Includes units aboard a transport on the tile — COL1 keeps them in
 * the same per-tile chain, so each passenger gets its own sidebar row.
 * Returns the number of ids written.
 */
int map_panel_collect_stack(
  const ColonizeUnitPool* units,
  int x,
  int y,
  int* out_ids,
  int max
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
