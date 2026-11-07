#ifndef COLONIZE_CORE_MAP_PANEL_INTERNAL_H
#define COLONIZE_CORE_MAP_PANEL_INTERNAL_H

/*
 * Stage seam for map_panel_render_w (the FUN_49dd_0424 sidebar port), split
 * by panel region (port_bugs.md #1107). The stages stay file-local in production
 * (COLONIZE_INTERNAL, core/internal.h); tests may call one directly.
 */

#include "core/internal.h"
#include "core/map_panel.h"

/* Shared state of one map_panel_render_w call. Inputs are the entry point's
 * parameters; the derived block is filled by the dispatcher (text column,
 * selection / info tile) and by the minimap stage (section_bottom). text_y
 * is the running text cursor every text stage advances. */
typedef struct MapPanelRenderCtx {
  const ColonizeWorldMap* map;
  const ColonizeUnitPool* units;
  const ColonizeColonyPool* colonies;
  const ColonizeCol1Save* col1;
  const MapPanel* panel;
  const ColonizeSpriteSheet* icons;
  const ColonizeFont* font;
  const ColonizeMsgCatalog* names;
  const ColonizeMsgCatalog* labels;
  int view_x;
  int view_y;
  int view_cols;
  int view_rows;
  int cursor_x;
  int cursor_y;
  int fog_nation;
  uint16_t game_year;
  uint16_t game_autumn;
  int gold;
  int tax_percent;
  const char* nation_name;
  const ColonizePalette* active_palette;
  bool end_turn_active;
  bool end_turn_blink_white;
  ColonizeFramebuffer8* framebuffer;
  uint16_t debug_flags; /* DS:0x894 CHEAT Debug Info Flags (MapPanel.debug_flags) */

  int resolved_human; /* save's control==0 nation, see map_panel_render_w */
  int panel_y;
  int section_bottom;
  int text_x;
  int indent_x;
  int panel_right;
  int line_h;
  int y_limit;
  int text_y;
  const ColonizeUnit* selected; /* NULL in View Pieces / foreign selection */
  int info_x;
  int info_y;
  bool tile_seen;
} MapPanelRenderCtx;

#ifdef COLONIZE_TESTING
/* port_bugs.md #1177b pure formatters (FUN_49dd_0424 B1-B4), see map_panel.c. */
COLONIZE_INTERNAL void map_panel_format_locat(
  char* out,
  size_t size,
  const char* label,
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  bool need_cheat
);
/* O9 ship cargo strip order (FUN_49dd_01aa raw 78508-78562): hold slots,
 * ascending DS:0x84bc key. Returns the count. */
COLONIZE_INTERNAL int map_panel_ship_cargo_order(
  const ColonizeUnit* carrier, const ColonizeCol1Save* col1, int out_slot[COLONIZE_UNIT_CARGO_MAX]
);
/* #1467 colony "With:" goods strip order (FUN_49dd_0424 raw 79035-79069). */
COLONIZE_INTERNAL int map_panel_colony_goods_strip(
  const ColonizeColony* col, const ColonizeCol1Save* col1, int out_cargo[5]
);
/* #1468 seen-land owner line (FUN_49dd_0424 raw 78786-78815). */
COLONIZE_INTERNAL void map_panel_format_owner(
  char* out,
  size_t size,
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  const char* nation_name,
  int human_nation
);
/* FUN_49dd_0424 raw 79282-79297 (DS:0x894 & 1) per-tribe debug row:
 * "<@TRIBES plural> <alarm word>". False for an extinct tribe (no row). */
COLONIZE_INTERNAL bool map_panel_format_debug_tribe(
  char* out, size_t size, const ColonizeCol1Save* col1, int tribe, int view_nation
);
COLONIZE_INTERNAL int map_panel_cheat_index(
  const ColonizeCol1Save* col1, const ColonizeUnitPool* units, int unit_id
);
COLONIZE_INTERNAL void map_panel_render_backdrop(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_minimap(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_header(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_selected_block(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_cursor_block(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_tile_details(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_holds(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_colony_row(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_village_row(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_stack(MapPanelRenderCtx* c);
COLONIZE_INTERNAL void map_panel_render_end_of_turn(MapPanelRenderCtx* c);
#endif

#endif /* COLONIZE_CORE_MAP_PANEL_INTERNAL_H */
