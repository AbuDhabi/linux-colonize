#ifndef COLONIZE_CORE_COLONY_SCREEN_INTERNAL_H
#define COLONIZE_CORE_COLONY_SCREEN_INTERNAL_H

/*
 * Cross-file seams for the colony screen, split along its section banners:
 *   colony_screen.c           view state, subpanel open/close, PIK load/free
 *   colony_screen_draw.c      chrome primitives, icon strips, overlays, minimap
 *   colony_screen_buildings.c DOS settlement layout, building + cargo strips
 *   colony_screen_panels.c    population and multifunction production panels
 *   colony_screen_popups.c    popup rendering, hit testing, render entry point
 *
 * Everything declared here was `static` in the single-file colony_screen.c and
 * is file-scope by convention still: nothing outside those five .c files may
 * use it — the screen's public surface stays in core/colony_screen.h. Same
 * pattern as core/game_loop_internal.h.
 */

#include "core/colony_screen.h"

/* ===================== Shared sprite ids, box sizes & category ids ===================== */

/*
 * Approximate collage positions inside the PARCH buildings section.
 * Exact DOS placement is not recovered yet; this is a readable bring-up layout.
 *
 * BUILDING.SS notes:
 *   #16 — full pre-stockade fence (bottom-right of buildings section)
 *   #45 — empty coastal placeholder (trees + shore); docks/drydock/shipyard replace it
 *   #42–44,46–47 — empty-slot tree clumps (large/med/small)
 *
 * Classic bottom-right stack: coast/docks (75×48) above fence/stockade (73×18).
 */
enum {
  COLONY_FENCE_SPRITE = 16,
  COLONY_TREE_LARGE = 42,
  COLONY_TREE_MED = 43,
  COLONY_TREE_SMALL = 44,
  COLONY_COAST_PLACEHOLDER = 45,
  /* DOS 0x2f / 0x30 (minus one): the warehouse slot's stable-only and
   * warehouse-plus-stable variants — see colony_screen_category_sprite. */
  COLONY_STABLE_ONLY_SPRITE = 46,
  COLONY_WAREHOUSE_STABLE_SPRITE = 47,
  COLONY_FENCE_W = 73,
  COLONY_FENCE_H = 18,
  COLONY_COAST_W = 75,
  COLONY_COAST_H = 48,
  COLONY_BUILDING_WORKERS_MAX = 3
};

typedef struct ColonyBuildingSlot {
  int chain;                 /* COLONIES_CHAIN_* (colony.h owns the name list) */
  const int* render; /* NULL, or a render-only override of that chain, as rows */
  int size_class;            /* NAMES.TXT @BUILDING column 4 */
} ColonyBuildingSlot;

enum {
  COLONY_CAT_FORTIFICATION = 0,
  COLONY_CAT_DOCKS = 2,
  COLONY_CAT_WAREHOUSE = 5,
  COLONY_DOS_SLOT_COUNT = 15,
  COLONY_DOS_CLASS_COUNT = 5
};

/* The 15 colony-screen building categories and their sizes — defined in
 * colony_screen_buildings.c, walked by colony_screen_popups.c's hit test. */
extern const ColonyBuildingSlot colony_screen_building_slots[];
extern const int colony_screen_building_slot_count;

/* DOS size-class boxes (DS:0x230 / DS:0x236) — defined in colony_screen_draw.c. */
extern const int colony_screen_class_box[5][2];

/* ===================== People band layout (colony_screen_panels.c <-> _popups.c) ===================== */

typedef struct PeopleEntry {
  int sprite;
  int sel_colonist; /* colonist index, or -1 */
  int sel_unit;     /* outside unit id, or -1 */
  int px;
  int iw;
} PeopleEntry;

/* ===================== Shared drawing / layout helpers ===================== */

void colony_screen_assign_slot_positions_ex(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int* xs,
  int* ys,
  int* out_slot, /* optional, indexed by category: the DS:0x266 SLOT it landed in */
  uint32_t seed_base /* DS:0x8d80, see colony_screen_layout_seed */
);
void colony_screen_blit_buildings(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeFont* font,
  bool debug_rects,
  ColonizeFramebuffer8* framebuffer
);
void colony_screen_blit_cargo(
  const ColonyScreenView* view,
  int cargo,
  bool grey,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y
);
void colony_screen_blit_icon(
  const ColonyScreenView* view,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y
);
void colony_screen_blit_mini_unit(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  int sprite,
  int x,
  int y
);
int colony_screen_building_worker_strip(
  const ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  int built,
  int* out_ci,
  int* out_icons,
  int* out_strip_h
);
int colony_screen_category_built(
  const ColonizeColonyPool* pool, const ColonizeColony* colony, int cat
);
void colony_screen_debug_building_rect(
  const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer, int sprite, int x, int y
);
void colony_screen_draw_area_overlays(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);
void colony_screen_draw_cargo_strip(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);
void colony_screen_draw_chrome_selection(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  int sprite,
  int x,
  int y
);
void colony_screen_draw_hline(ColonizeFramebuffer8* framebuffer, int y, int color);
void colony_screen_draw_icon_selection(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  int sprite,
  int x,
  int y
);
void colony_screen_draw_icon_strip(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  const int* icons,
  int count,
  int selected_index,
  uint8_t number_color,
  bool always_show_number
);
void colony_screen_draw_multifunction(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  const ColonizeMsgCatalog* labels,
  ColonizeFramebuffer8* framebuffer
);
void colony_screen_draw_outlined_number(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t fg_color
);
void colony_screen_draw_people(
  ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);
void colony_screen_draw_resource_count(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  int icon_sprite,
  int amount,
  uint8_t number_color,
  bool always_show_number
);
void colony_screen_draw_resource_count_pair(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  int icon0,
  int amount0,
  int icon1,
  int amount1,
  uint8_t number_color,
  bool always_show_number
);
void colony_screen_draw_selection_box(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  uint8_t color
);
void colony_screen_draw_top_bar(
  const ColonizeColony* colony,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);
void colony_screen_draw_transports(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);
void colony_screen_draw_vline(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y0,
  int y1,
  int color
);
void colony_screen_fence_rect(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const int* xs,
  const int* ys,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h
);
void colony_screen_fill_parch(const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer);
void colony_screen_fill_top_bar_wood(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer
);
void colony_screen_fill_wood_tile(const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer);
const char* colony_screen_hammers_word(void);
int colony_screen_icon_strip_layout(int x, int w, int count, int ref_iw, int* out_x);
uint32_t colony_screen_layout_seed(const ColonyScreenView* view);
int colony_screen_outside_display_sprite(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
);
void colony_screen_outside_icon_metrics(
  const ColonyScreenView* view,
  const ColonizeUnitPool* units,
  int unit_id,
  int* out_w,
  int* out_h
);
int colony_screen_people_layout(
  const ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  PeopleEntry* ent,
  int max_ent
);
const ColonizeFont* colony_screen_popup_font(
  const ColonyScreenView* view,
  const ColonizeFont* screen_font,
  bool smallfont
);
void colony_screen_render_minimap(
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  int colony_x,
  int colony_y,
  ColonizeFramebuffer8* framebuffer
);
bool colony_screen_unit_on_fence(const ColonizeUnitPool* units, const ColonizeUnit* u);
#endif /* COLONIZE_CORE_COLONY_SCREEN_INTERNAL_H */
