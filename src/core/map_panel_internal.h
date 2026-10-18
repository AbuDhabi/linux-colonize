#ifndef COLONIZE_CORE_MAP_PANEL_INTERNAL_H
#define COLONIZE_CORE_MAP_PANEL_INTERNAL_H

/*
 * Stage seam for map_panel_render_w (the FUN_49dd_0424 sidebar port), split
 * by panel region (bugs.md #1107). The stages stay file-local in production
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
