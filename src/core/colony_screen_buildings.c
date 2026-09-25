#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony_craft.h"
#include "core/colony_preview.h"
#include "core/colony_production.h"
#include "core/colony_screen_internal.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/fb.h"
#include "core/ff.h"
#include "core/founding_fathers.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/ui_colors.h"
#include "core/turn.h"
#include "core/ui_button.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * Sections:
 *  - Building slot layout, chains & building strip rendering (colony_screen_set_layout_seed .. colony_screen_blit_buildings) (~line 162)
 *  - Cargo/transport strip rendering (colony_screen_draw_cargo_strip .. colony_screen_draw_transports) (~line 954)
 */


/*
 * ---------------------------------------------------------------------
 * DOS settlement-view layout — a real port of FUN_2f2b_0434 (assignment)
 * and FUN_2f2b_171c (draw), replacing this port's earlier invented
 * candidate-slot pools. Every table below is initialised DS data read
 * straight out of VICEROY.EXE at file offset 121248 + addr (the same base
 * as the DS strings; verify with DS:0xfef == "."). See
 * docs/colony_screen.md for the write-up.
 *
 * DOS groups the 42 @BUILDING rows into 15 CATEGORIES (the static setup
 * table FUN_75c2_144c), each category having one SIZE CLASS (0..4, the
 * "size" column of NAMES.TXT @BUILDING) and each class owning a fixed run
 * of screen SLOTS. Slot coordinates are DS:0x266; the run layout is
 * DS:0x224 (count) / DS:0x22a (base). Counts are 7/4/2/1/1 = 15 slots for
 * 15 categories, so every category always gets exactly one slot — an
 * unbuilt one just draws its class's placeholder sprite there.
 * ---------------------------------------------------------------------
 */

typedef struct ColonyPoint {
  int x;
  int y;
} ColonyPoint;

/*
 * The 15 colony-screen building categories (audit CO-13). The tier name
 * lists themselves live once in colony.c behind colonies_building_chain() —
 * the same table col1_bridge.c's save encode/decode walks — so a renamed or
 * re-tiered building cannot draw one thing and save another.
 *
 * Two DOS screen categories are not a 1:1 match for a save chain:
 *  - Town Hall: DOS's table really does continue into Capitol / Capitol
 *    Expansion, but both are unbuildable (colonies_building_is_buildable)
 *    and BUILDING.SS has only stubs for them, so the screen slot stops at
 *    the Town Hall and the Capitol keeps its own save chain.
 *  - Warehouse: DOS folds the Stable into this one slot (its sprite rule is
 *    in colony_screen_category_sprite), while the save format gives the
 *    Stable its own bit group and level byte. Hence the render-only
 *    override below, which appends the Stable after the warehouse tiers.
 */
/* @BUILDING rows, -1 terminated: Warehouse, Warehouse Expansion, then the
 * Stable the screen folds into the same slot. */
static const int k_slot_warehouse_render_rows[] = {
  COLONY_BUILDING_WAREHOUSE, COLONY_BUILDING_WAREHOUSE_EXPANSION, COLONY_BUILDING_STABLE, -1
};

/* DOS category order (FUN_75c2_144c). The order matters: it is also the
 * order categories claim positions within their size class. */
const ColonyBuildingSlot colony_screen_building_slots[] = {
  {COLONIES_CHAIN_FORTIFICATION, NULL, 3}, /*  0 fortification (the fence corner) */
  {COLONIES_CHAIN_ARMORY, NULL, 1},        /*  1 */
  {COLONIES_CHAIN_DOCKS, NULL, 4},         /*  2 (the dock corner) */
  {COLONIES_CHAIN_TOWN_HALL, NULL, 2},     /*  3 */
  {COLONIES_CHAIN_SCHOOL, NULL, 1},        /*  4 */
  {COLONIES_CHAIN_WAREHOUSE, k_slot_warehouse_render_rows, 1}, /*  5 warehouse + stable */
  {COLONIES_CHAIN_CUSTOM_HOUSE, NULL, 0},  /*  6 */
  {COLONIES_CHAIN_PRESS, NULL, 0},         /*  7 */
  {COLONIES_CHAIN_WEAVER, NULL, 0},        /*  8 */
  {COLONIES_CHAIN_TOBACCONIST, NULL, 0},   /*  9 */
  {COLONIES_CHAIN_RUM, NULL, 0},           /* 10 */
  {COLONIES_CHAIN_FUR, NULL, 0},           /* 11 */
  {COLONIES_CHAIN_CARPENTER, NULL, 1},     /* 12 */
  {COLONIES_CHAIN_CHURCH, NULL, 2},        /* 13 */
  {COLONIES_CHAIN_BLACKSMITH, NULL, 0},    /* 14 */
};
const int colony_screen_building_slot_count =
  (int)(sizeof(colony_screen_building_slots) / sizeof(colony_screen_building_slots[0]));

/*
 * DS:0x266 — the 15 slot origins, (x, y) word pairs. DOS draws each slot's
 * sprite at (x, y + 8) with no further offset; this port's callers add
 * (COLONY_VIEWPORT_X, COLONY_VIEWPORT_Y) = (1, 8) instead, so the stored
 * values below are DOS's x minus 1 and DOS's raw y (the +8 and the viewport
 * origin cancel). Slots 13 and 14 are the fence and dock corners — the two
 * positions this port had already recovered by template-matching the
 * goldens, which is what pins the whole table.
 */
static const ColonyPoint k_dos_slot_xy[COLONY_DOS_SLOT_COUNT] = {
  {55, 5}, {144, 7}, {172, 10}, {7, 33}, {36, 37}, {66, 46}, {95, 45}, /* class 0 */
  {5, 6}, {127, 45}, {9, 68}, {14, 94},                                /* class 1 */
  {86, 3}, {65, 79},                                                   /* class 2 */
  {122, 98},                                                           /* class 3 */
  {122, 47}                                                            /* class 4 */
};
static const int k_dos_class_count[COLONY_DOS_CLASS_COUNT] = {7, 4, 2, 1, 1}; /* DS:0x224 */
static const int k_dos_class_base[COLONY_DOS_CLASS_COUNT] = {0, 7, 11, 13, 14}; /* DS:0x22a */
/*
 * DS:0x260, empty-slot filler per class, minus one: DOS's sprite ids are
 * 1-based (0 = none) where this port's SS loader is 0-based. Class 3 has no
 * filler because its category (fortification) is always placed.
 */
static const int k_dos_class_placeholder[COLONY_DOS_CLASS_COUNT] = {
  COLONY_TREE_SMALL, COLONY_TREE_MED, COLONY_TREE_LARGE, -1, COLONY_COAST_PLACEHOLDER
};

/*
 * The per-game half of DOS's layout seed (DS:0x8d80). DOS fills it from the
 * BIOS tick at 0040:006C when the game boots (FUN_1c0c_0012 via
 * FUN_75c2_2d46 raw 121990) and SAVES it in the post-map tail @608
 * (FUN_2a1f_0c9c raw 120106 / 0cb4 raw 120429) = post_map.boot_timer, so a
 * save does reproduce its layouts (bugs.md #564/#578). The live game hands
 * that field in through colony_screen_set_layout_seed; this fallback is only
 * for callers without a save (tests, tools) and is not arbitrary: 25281 =
 * 844481 mod 32768, the boot_timer of the screenshot saves
 * (original_saves/colony-prod-tests/COLONY00-dutch2-*), the unique base in
 * 0..0x7fff that reproduces BOTH golden screenshots (New Amsterdam at (50,43)
 * and Recife at (41,38), 18 independent category→slot constraints).
 */
#define COLONY_DOS_LAYOUT_SEED 25281u

/* ===================== Building slot layout, chains & building strip rendering (colony_screen_set_layout_seed .. colony_screen_blit_buildings) ===================== */

void colony_screen_set_layout_seed(ColonyScreenView* view, uint32_t boot_timer) {
  if (!view) {
    return;
  }
  view->layout_seed_set = true;
  view->layout_seed_base = boot_timer;
}

uint32_t colony_screen_layout_seed(const ColonyScreenView* view) {
  return (view && view->layout_seed_set) ? view->layout_seed_base : COLONY_DOS_LAYOUT_SEED;
}

static int colony_screen_slot_size_class(int slot) {
  for (int c = 0; c < COLONY_DOS_CLASS_COUNT; ++c) {
    if (slot >= k_dos_class_base[c] && slot < k_dos_class_base[c] + k_dos_class_count[c]) {
      return c;
    }
  }
  return 0;
}

/*
 * Port of FUN_2f2b_0434. Fills xs[]/ys[] (each sized colony_screen_building_slot_count,
 * indexed by DOS category) with this colony's building positions.
 *
 * DOS: reseed the C-library LCG from (colony.y << 8) + colony.x + DS:0x8d80
 * (FUN_15eb_1476, masked to 15 bits by srand), then walk slots 0..14 and give
 * each one a random still-free POSITION inside its own size class, retrying
 * on collision. Positions are then handed to categories in category order,
 * sequentially within each class — a fixed mapping, so all the randomness
 * lives in that one shuffle. Deterministic per colony, cheap enough to redo
 * on every call.
 */
void colony_screen_assign_slot_positions_ex(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int* xs,
  int* ys,
  int* out_slot, /* optional, indexed by category: the DS:0x266 SLOT it landed in */
  uint32_t seed_base /* DS:0x8d80, see colony_screen_layout_seed */
) {
  (void)pool;
  int perm[COLONY_DOS_SLOT_COUNT];
  for (int i = 0; i < COLONY_DOS_SLOT_COUNT; ++i) {
    perm[i] = -1;
  }

  ColonizeDosRng rng;
  const uint32_t xy = colony ? (((uint32_t)colony->y << 8) + (uint32_t)colony->x) : 0u;
  dos_rng_seed(&rng, (seed_base + xy) & 0x7fffu);

  for (int slot = 0; slot < COLONY_DOS_SLOT_COUNT; ++slot) {
    const int cls = colony_screen_slot_size_class(slot);
    const int base = k_dos_class_base[cls];
    const int count = k_dos_class_count[cls];
    int pos = base;
    /* Each class has exactly as many positions as slots, so this always
     * terminates; the guard is belt and braces against a bad table edit. */
    for (int guard = 0; guard < 1024; ++guard) {
      pos = base + dos_rng_range(&rng, 0, count - 1);
      if (perm[pos] < 0) {
        break;
      }
    }
    if (perm[pos] >= 0) {
      for (pos = base; pos < base + count && perm[pos] >= 0; ++pos) {
      }
      if (pos >= base + count) {
        pos = base;
      }
    }
    perm[pos] = slot;
  }

  int next_in_class[COLONY_DOS_CLASS_COUNT] = {0};
  for (int cat = 0; cat < colony_screen_building_slot_count; ++cat) {
    const int cls = colony_screen_building_slots[cat].size_class;
    const int pos = k_dos_class_base[cls] + next_in_class[cls]++;
    const int slot = (pos >= 0 && pos < COLONY_DOS_SLOT_COUNT && perm[pos] >= 0) ? perm[pos] : 0;
    xs[cat] = k_dos_slot_xy[slot].x;
    ys[cat] = k_dos_slot_xy[slot].y;
    if (out_slot) {
      out_slot[cat] = slot;
    }
  }
}

/* colonies_has_building_named's answer plus the building-type index, which
 * the slot renderer needs as a BUILDING.SS sprite id (audit CO-17: the bool
 * uses now call the colony.h helper; this spelling stays only for the id). */
static int colony_screen_find_built(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* name
) {
  if (!pool || !colony || !name) {
    return -1;
  }
  const int idx = colonies_find_building(pool, name);
  if (idx < 0 || !colony->has_building[idx]) {
    return -1;
  }
  return idx;
}

/* Row-identified sibling of colony_screen_find_built, for callers that
 * already know the @BUILDING row (no-DOS-text-in-binary: identity is the
 * row, not a catalog name). */
static int colony_screen_find_built_row(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  ColonizeBuildingRow row
) {
  const int idx = colonies_building_row(pool, row);
  if (idx < 0 || !pool || !colony || idx >= COLONIZE_BUILDING_TYPES_MAX ||
      !colony->has_building[idx]) {
    return -1;
  }
  return idx;
}

/* Highest present building in an upgrade chain (names ordered low → high). */
static int colony_screen_best_built(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* const* names,
  size_t name_count
) {
  int best = -1;
  for (size_t i = 0; i < name_count; ++i) {
    const int idx = colony_screen_find_built(pool, colony, names[i]);
    if (idx >= 0) {
      best = idx;
    }
  }
  return best;
}

/* A screen category's tier list: the shared colony.h chain, unless the
 * category carries a render-only override (audit CO-13). */
static const char* const* colony_screen_slot_chain(int cat) {
  if (cat < 0 || cat >= (int)(sizeof(colony_screen_building_slots) / sizeof(colony_screen_building_slots[0]))) {
    return NULL;
  }
  if (colony_screen_building_slots[cat].render) {
    /* Name view of the row override, spelled by the catalog. */
    static const char* names[8];
    int n = 0;
    for (const int* r = colony_screen_building_slots[cat].render; *r >= 0 && n < 7; ++r) {
      names[n++] = colonies_building_row_name(*r);
    }
    names[n] = NULL;
    return names;
  }
  return colonies_building_chain(colony_screen_building_slots[cat].chain);
}

/* Chain length helper — the tables are NULL-terminated. */
static size_t colony_screen_chain_len(const char* const* chain) {
  size_t n = 0;
  while (chain && chain[n]) {
    ++n;
  }
  return n;
}

/*
 * The building a category's slot stands for, or -1 if the colony owns none of
 * it. Highest built tier, with DOS's one normalisation (FUN_2f2b_14d4's
 * 0x0f/0x11 swaps): the shared warehouse/stable slot reports the Warehouse
 * whenever it is owned, never the Stable and never the Expansion — DOS never
 * sets the Expansion's own building bit at all, it only bumps the level
 * counter this port keeps in warehouse_level.
 */
int colony_screen_category_built(
  const ColonizeColonyPool* pool, const ColonizeColony* colony, int cat
) {
  if (cat < 0 || cat >= colony_screen_building_slot_count) {
    return -1;
  }
  if (cat == COLONY_CAT_WAREHOUSE) {
    return colony_screen_find_built_row(pool, colony, COLONY_BUILDING_WAREHOUSE);
  }
  const char* const* chain = colony_screen_slot_chain(cat);
  return colony_screen_best_built(pool, colony, chain, colony_screen_chain_len(chain));
}

/*
 * BUILDING.SS sprite for a category's slot, or -1 to draw nothing.
 * Port of FUN_2f2b_14d4's `local_5a` (DOS ids are 1-based, this port's are
 * 0-based, hence every DOS constant here appears minus one):
 *
 *  - fortification: always drawn, even unbuilt — an unowned Stockade shows
 *    the post-and-rail fence, DOS id 0x11.
 *  - warehouse/stable share one slot and have their own art: no Warehouse
 *    shows the stable-only sprite (DOS 0x2f), Warehouse + Stable a combined
 *    one (DOS 0x30), Warehouse alone the plain warehouse. Warehouse
 *    Expansion is never drawn as itself — BUILDING.SS's slot for it holds
 *    the fence art, and DOS reports the expansion with the white "2" badge
 *    instead (see the badge block in colony_screen_draw_area_overlays).
 *  - anything else: its highest built tier, else the size class's filler.
 */
static int colony_screen_category_sprite(
  const ColonizeColonyPool* pool, const ColonizeColony* colony, int cat
) {
  if (cat < 0 || cat >= colony_screen_building_slot_count) {
    return -1;
  }
  if (cat == COLONY_CAT_FORTIFICATION) {
    const int built = colony_screen_category_built(pool, colony, cat);
    return built >= 0 ? built : COLONY_FENCE_SPRITE;
  }
  if (cat == COLONY_CAT_WAREHOUSE) {
    const int warehouse = colony_screen_find_built_row(pool, colony, COLONY_BUILDING_WAREHOUSE);
    const bool stable = colonies_has_building_row(pool, colony, COLONY_BUILDING_STABLE);
    if (warehouse < 0) {
      return stable ? COLONY_STABLE_ONLY_SPRITE : k_dos_class_placeholder[colony_screen_building_slots[cat].size_class];
    }
    return stable ? COLONY_WAREHOUSE_STABLE_SPRITE : warehouse;
  }
  const int built = colony_screen_category_built(pool, colony, cat);
  if (built >= 0) {
    return built;
  }
  return k_dos_class_placeholder[colony_screen_building_slots[cat].size_class];
}

/*
 * bugs.md #536: DOS-LITERAL anchor for the fortification's outside-unit
 * strip. FUN_2f2b_14d4 (raw 48255) calls the strip drawer
 * thunk_FUN_291f_0564 -> FUN_2f2b_11b2 (raw 47883) with
 * x = *(char*)(param_4+0x23c) + slot_x, y = *(char*)(param_4+0x242) +
 * slot_y, where param_4 is the slot's size CLASS (not the built sprite).
 * DS:0x23c/0x242 are two more 5-entry (class 0..4) byte tables sitting
 * right after the already-recovered DS:0x230 width / 0x236 height class
 * boxes (file offset 121248+addr, per docs/conventions.md's DS->EXE rule);
 * dumped from VICEROY.EXE: {3,20,25,5,0} / {12,8,22,5,0}. The fortification
 * slot is always size class 3 (colony_screen_building_slots[COLONY_CAT_FORTIFICATION]),
 * so the raw DOS anchor offset is (+5,+5) from the slot's top-left corner —
 * fixed regardless of which tier (fence/stockade/fort/fortress) occupies
 * it, because the class (hence the offset) never changes across tiers.
 * The port previously anchored at (+0,+0) *and* resized the whole strip
 * box to the actual built sprite's pixel dimensions, so the row visibly
 * shifted as the garrison was upgraded — neither behaviour exists in DOS,
 * which always uses the fixed class box (73x18) and the fixed class
 * offset.
 */
enum {
  COLONY_FENCE_UNIT_OFFSET_X = 5, /* DS:0x23c[class 3] */
  /* The port represents the strip as a top-left 18px screen rect. Applying
   * DOS's raw +5 y anchor would place its final row on the bottom separator,
   * so keep the visible strip two pixels higher. */
  COLONY_FENCE_UNIT_OFFSET_Y = 3
};

/*
 * Screen rectangle of the fortification slot (DOS category 0, always drawn),
 * which the outside-unit strip sits on. xs/ys come from
 * colony_screen_assign_slot_positions_ex.
 */
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
) {
  (void)view;
  (void)pool;
  (void)colony;
  *out_x = COLONY_VIEWPORT_X + xs[COLONY_CAT_FORTIFICATION] + COLONY_FENCE_UNIT_OFFSET_X;
  *out_y = COLONY_VIEWPORT_Y + ys[COLONY_CAT_FORTIFICATION] + COLONY_FENCE_UNIT_OFFSET_Y;
  *out_w = COLONY_FENCE_W;
  *out_h = COLONY_FENCE_H;
}

static void colony_screen_blit_slot(
  const ColonyScreenView* view,
  int sprite_index,
  int x,
  int y,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !framebuffer || sprite_index < 0 || sprite_index >= view->buildings.sprite_count) {
    return;
  }
  const ColonizeSprite* spr = &view->buildings.sprites[sprite_index];
  if (!spr || !spr->pixels || spr->width <= 2 || spr->height <= 2) {
    return;
  }
  ss_blit_sprite(&view->buildings, sprite_index, framebuffer, x, y);
}

int colony_screen_outside_display_sprite(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
);

/* Icon metrics for an outside (on-tile) unit; defaults if sheet/sprite missing. */
void colony_screen_outside_icon_metrics(
  const ColonyScreenView* view,
  const ColonizeUnitPool* units,
  int unit_id,
  int* out_w,
  int* out_h
) {
  int w = 12;
  int h = 16;
  if (units && view && view->icons_ok) {
    const ColonizeUnit* u = units_get_const(units, unit_id);
    const int sprite = colony_screen_outside_display_sprite(units, u);
    if (sprite >= 0 && sprite < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[sprite];
      if (sp && sp->width > 0 && sp->height > 0) {
        w = sp->width;
        h = sp->height;
      }
    }
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
}

int colony_screen_outside_display_sprite(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
) {
  if (!units || !u) {
    return -1;
  }
  const ColonizeUnitType* type = units_type(units, u->type_index);
  int sprite = units_map_sprite(units, u->id);
  if (units_type_is_colonist(type) &&
      u->muskets <= 0 && u->horses <= 0 && u->tools <= 0) {
    sprite = units_working_colonist_sprite(units, u->type_index, u->profession);
  }
  return sprite;
}

/* DOS-LITERAL FUN_2f2b_11b2 raw 47899-47906: FUN_281f_0b28 ->
 * FUN_15eb_08e6 admits only DS:0x30e[type] >= 0 to the fence strip.
 * Treasure and Artillery remain in the on-tile Units Present roster. */
bool colony_screen_unit_on_fence(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return false;
  }
  return units_type_has_profession_slot(u->type_index);
}

int colony_screen_multi_units_layout(
  const ColonyScreenView* view,
  const ColonizeUnitPool* units,
  int px,
  int py,
  int pane_w,
  int pane_h,
  ColonyMultiUnitSlot* out,
  int max
) {
  if (!view || !units || !out || max <= 0 || pane_w <= 0 || pane_h <= 0) {
    return 0;
  }
  /* Land units only (colonist-class + Artillery); ships/wagons stay on the
   * Transport strip. outside_unit_ids already excludes units_is_transport. */
  int ids[COLONY_MULTI_UNITS_SLOT_MAX];
  int n = 0;
  for (int i = 0; i < view->outside_unit_count && n < COLONY_MULTI_UNITS_SLOT_MAX; ++i) {
    ids[n++] = view->outside_unit_ids[i];
  }

  /*
   * DOS FUN_2f2b_1e46 (2f2b:1ecc..1ffd), click hit-test FUN_2f2b_59a0: the
   * first 5 units fill the bottom row of full 16x16 icons at x=0xd5 step
   * 0x12, y=0x9e; every unit past that goes on an overflow row above —
   * y=0x98, then y=0x90 — as a 3x5 miniature at step 5, 17 per row (the
   * scaled-blit path at 2f2b:1f14). Absolute screen coordinates, like DOS;
   * px/py/pane_* only bound the caller's clip.
   */
  (void)px;
  (void)py;
  (void)pane_w;
  (void)pane_h;
  int count = 0;
  for (int i = 0; i < n && count < max; ++i) {
    const ColonizeUnit* u = units_get_const(units, ids[i]);
    const int sprite = u ? colony_screen_outside_display_sprite(units, u) : -1;
    if (sprite < 0) {
      continue;
    }
    if (count < COLONY_MULTI_UNITS_ROW0) {
      out[count].unit_id = ids[i];
      out[count].x = COLONY_MULTI_UNITS_X + count * COLONY_MULTI_UNITS_ROW0_STEP;
      out[count].y = COLONY_MULTI_UNITS_ROW0_Y;
      out[count].w = COLONY_MULTI_UNITS_ROW0_STEP;
      out[count].h = 16;
      out[count].mini = false;
    } else {
      const int over = count - COLONY_MULTI_UNITS_ROW0;
      const int row = over / COLONY_MULTI_UNITS_OVERFLOW_PER_ROW;
      const int slot = over % COLONY_MULTI_UNITS_OVERFLOW_PER_ROW;
      if (row > 1) {
        break; /* DOS stops after two overflow rows (local_6a > 2). */
      }
      out[count].unit_id = ids[i];
      out[count].x = COLONY_MULTI_UNITS_X + slot * COLONY_MULTI_UNITS_OVERFLOW_STEP;
      out[count].y = (row == 0) ? COLONY_MULTI_UNITS_ROW1_Y : COLONY_MULTI_UNITS_ROW2_Y;
      out[count].w = COLONY_MULTI_UNITS_OVERFLOW_STEP;
      out[count].h = COLONY_MULTI_UNITS_MINI_H;
      out[count].mini = true;
    }
    count++;
  }
  return count;
}

/*
 * Overflow-row miniature: the unit sprite resampled into a 3x5 cell (DOS
 * routes these through the scaled/dithered blitter FUN_1c56_0004; nearest
 * sample keeps the unit's dominant colours at this size).
 */
void colony_screen_blit_mini_unit(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  int sprite,
  int x,
  int y
) {
  if (!view || !view->icons_ok || !framebuffer || sprite < 0 ||
      sprite >= view->icons.sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &view->icons.sprites[sprite];
  if (!sp || !sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  for (int oy = 0; oy < COLONY_MULTI_UNITS_MINI_H; ++oy) {
    const int sy = (oy * sp->height + sp->height / 2) / COLONY_MULTI_UNITS_MINI_H;
    for (int ox = 0; ox < COLONY_MULTI_UNITS_MINI_W; ++ox) {
      const int sx = (ox * sp->width + sp->width / 2) / COLONY_MULTI_UNITS_MINI_W;
      const uint8_t c = sp->pixels[sy * sp->width + sx];
      if (c == COLONIZE_SS_TRANSPARENT) {
        continue;
      }
      const int fx = x + ox;
      const int fy = y + oy;
      if (fx >= 0 && fx < framebuffer->width && fy >= 0 && fy < framebuffer->height) {
        framebuffer->pixels[fy * framebuffer->width + fx] = c;
      }
    }
  }
}

static int colony_screen_building_production_badge(
  const ColonizeColonyPool* pool,
  int built
) {
  if (!pool || built < 0 || built >= pool->building_type_count) {
    return -1;
  }
  const int row = colonies_building_type_row(pool, built);
  if (row < 0) {
    return -1;
  }
  /* Printing Press / Newspaper are colony-wide bell *multipliers* — a
   * separate building slot (k_slot_press) nobody can ever be assigned to
   * work (no @JOB for them). Player-caught: matching them here drew a bell
   * badge on the Press/Newspaper sprite itself, duplicating Town Hall's own
   * badge — only Town Hall has a worker slot and a bell count to show. */
  const int chain = colonies_building_row_chain(row);
  if (chain == COLONIES_CHAIN_TOWN_HALL) {
    return COLONY_ICON_BELL;
  }
  if (chain == COLONIES_CHAIN_CHURCH) {
    return COLONY_ICON_CROSS;
  }
  if (chain == COLONIES_CHAIN_CARPENTER) {
    return COLONY_ICON_HAMMER;
  }
  /* Manufacturing: the badge icon is the recipe's out_cargo, read from the
   * one shared table in colony_craft.c (audit CO-12), matched by chain — not
   * the private ladder this replaced, whose looser needles could miss a
   * renamed factory tier (a staffed Cigar Factory drew no produce badge
   * while its Tobacconist's Shop predecessor did). Every @BUILDING row in a
   * chain resolves to the same icon. */
  const ColonizeCraftRecipe* rec = colony_craft_recipe_for_building_row(row);
  if (rec) {
    return COLONY_CARGO_ICON_BASE + rec->out_cargo;
  }
  return -1;
}

/* DEBUG menu "Building Rects": violet (EGA bright magenta, WOODPANL.PIK
 * idx 13 — not used anywhere else on this screen) outline around a
 * building sprite's actual bounds, so placement can be tweaked by eye. */
#define COLONY_DEBUG_RECT_COLOR 13

void colony_screen_debug_building_rect(
  const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer, int sprite, int x, int y
) {
  if (!view || sprite < 0 || sprite >= view->buildings.sprite_count) {
    return;
  }
  const ColonizeSprite* spr = &view->buildings.sprites[sprite];
  if (!spr || spr->width <= 0 || spr->height <= 0) {
    return;
  }
  colony_screen_draw_selection_box(framebuffer, x, y, spr->width, spr->height, COLONY_DEBUG_RECT_COLOR);
}

/*
 * Workers assigned to one building (DOS shows up to 3) plus the strip height
 * the drawer and the hit-tester must agree on (audit CO-18: drift between
 * the two silently desynced click regions from what was drawn). Returns the
 * worker count; out_ci gets colonist indices, out_icons their ICONS.SS
 * sprites, out_strip_h the tallest icon (never below the 16px default).
 */
int colony_screen_building_worker_strip(
  const ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  int built,
  int* out_ci,
  int* out_icons,
  int* out_strip_h
) {
  int workers = 0;
  int strip_h = 16;
  if (view && colony && units && built >= 0) {
    for (int ci = 0; ci < colony->colonist_count && workers < COLONY_BUILDING_WORKERS_MAX; ++ci) {
      const ColonizeColonist* c = &colony->colonists[ci];
      if (!c->active || c->building_type != built) {
        continue;
      }
      const int sprite =
        units_working_colonist_sprite(units, c->unit_type_index, c->profession);
      if (sprite < 0) {
        continue;
      }
      out_ci[workers] = ci;
      out_icons[workers] = sprite;
      workers++;
    }
    for (int wi = 0; wi < workers; ++wi) {
      if (out_icons[wi] >= 0 && out_icons[wi] < view->icons.sprite_count) {
        const int ih = view->icons.sprites[out_icons[wi]].height;
        if (ih > strip_h) {
          strip_h = ih;
        }
      }
    }
  }
  if (out_strip_h) {
    *out_strip_h = strip_h;
  }
  return workers;
}

void colony_screen_blit_buildings(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeFont* font,
  bool debug_rects,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->buildings_ok || !pool || !colony || !framebuffer) {
    return;
  }

  const int slot_ox = COLONY_VIEWPORT_X;
  const int slot_oy = COLONY_VIEWPORT_Y;
  int slot_x[32];
  int slot_y[32];
  colony_screen_assign_slot_positions_ex(
    pool, colony, slot_x, slot_y, NULL, colony_screen_layout_seed(view)
  );
  /* Pass 1: every building sprite first. Badges/worker strips/production
   * counters go in a second pass so an overlapping neighbour's sprite can
   * never blit over another slot's counters (player-reported: resource
   * counters rendered under buildings). */
  for (int i = 0; i < colony_screen_building_slot_count; ++i) {
    const int drawn_sprite = colony_screen_category_sprite(pool, colony, i);
    if (drawn_sprite < 0) {
      continue;
    }
    colony_screen_blit_slot(view, drawn_sprite, slot_ox + slot_x[i], slot_oy + slot_y[i], framebuffer);
    if (debug_rects) {
      colony_screen_debug_building_rect(
        view, framebuffer, drawn_sprite, slot_ox + slot_x[i], slot_oy + slot_y[i]
      );
    }
  }
  for (int i = 0; i < colony_screen_building_slot_count; ++i) {
    const ColonyBuildingSlot* slot = &colony_screen_building_slots[i];
    const int built = colony_screen_category_built(pool, colony, i);
    const int drawn_sprite = colony_screen_category_sprite(pool, colony, i);
    if (drawn_sprite < 0) {
      continue;
    }

    /*
     * Expansion badge. DOS FUN_2f2b_14d4's tail reads the colony's warehouse
     * level (+0x95) for building 0x0f and its capitol level (+0x96) for 0x1e,
     * and when either is above 1 prints the number in plain white (ink 0x0f)
     * centred in the building's size-class box: x + w/2 - 1, y + h/2 - 3 off
     * the slot origin. So a Warehouse Expansion shows as a white "2" on the
     * warehouse. (The Capitol half never fires — DOS's construction gate
     * refuses that building outright; see colonies_building_is_buildable.)
     */
    if (built >= 0 && font && i == COLONY_CAT_WAREHOUSE) {
      const int level = (int)colony->warehouse_level;
      if (level > 1) {
        const int cls = slot->size_class;
        char badge_text[8];
        snprintf(badge_text, sizeof(badge_text), "%d", level);
        font_draw_text(
          font,
          framebuffer,
          slot_ox + slot_x[i] + colony_screen_class_box[cls][0] / 2 - 1,
          slot_oy + slot_y[i] + colony_screen_class_box[cls][1] / 2 - 3,
          badge_text,
          15
        );
      }
    }

    /* Workers in this building (up to 3): Note 1 strip, bottom-center of sprite. */
    if (built < 0 || !units) {
      continue;
    }
    int worker_ci[COLONY_BUILDING_WORKERS_MAX];
    int worker_icons[COLONY_BUILDING_WORKERS_MAX];
    int strip_h = 16;
    const int workers = colony_screen_building_worker_strip(
      view, colony, units, built, worker_ci, worker_icons, &strip_h
    );
    const ColonizeSprite* bspr =
      (built >= 0 && built < view->buildings.sprite_count) ? &view->buildings.sprites[built] : NULL;
    const int bw = (bspr && bspr->width > 2) ? bspr->width : COLONY_BUILDING_SLOT_W;
    const int bh = (bspr && bspr->height > 2) ? bspr->height : COLONY_BUILDING_SLOT_H;
    const int bx = slot_ox + slot_x[i];
    const int by = slot_oy + slot_y[i];
    const int strip_y = by + bh - strip_h;
    if (workers > 0) {
      int selected = -1;
      for (int wi = 0; wi < workers; ++wi) {
        if (view->selected_colonist == worker_ci[wi]) {
          selected = wi;
          break;
        }
      }
      colony_screen_draw_icon_strip(
        view,
        NULL,
        framebuffer,
        bx,
        strip_y,
        bw,
        strip_h,
        worker_icons,
        workers,
        selected,
        15,
        false
      );
    }
    /*
     * Production strip: worker output + Town Hall / Church / Cathedral free
     * bells/crosses (still shown when the building is empty).
     */
    if (view->icons_ok) {
      const int badge = colony_screen_building_production_badge(pool, built);
      if (badge >= 0) {
        /* golden-confirmed (New Amsterdam): every building badge here shows
         * the colony's real per-tick total for that resource, not this one
         * function's own local (sol_bonus=0, and — for bells/crosses — pre-
         * FF/AI-subsidy) estimate: Blacksmith's "24" is the Production
         * tab's craft_capacity[TOOLS], and Church's "19"/Town Hall's "82" are
         * exactly the People band's crosses/bells (colony_prod_building_
         * display_output's own calc gave 7/13, undercounting both — see
         * colony_prod_colony_crosses_ff/_bells_ff's FF+AI-subsidy folding,
         * building_production.md). Reuse view->preview throughout rather
         * than a second, drifting local calc.
         *
         * craft_capacity (not craft_gross): a manufacturing badge shows the
         * staffed worker's maximum *potential* output, not this tick's
         * stock-clamped actual — player-caught (Weaver's House, dutch-
         * reports.SAV): a shortfall of cotton makes craft_gross[CLOTH] read
         * 5 (what actually got made) where DOS shows 10 (what the worker
         * can make, cotton permitting) — the shortfall itself already shows
         * separately on the Production tab. */
        /* bugs.md (carpentry.SAV): the badge is the crew's POTENTIAL with
         * the SoL bonus folded in — never this tick's input-clamped actual.
         * The hammers preview override is gone for the same reason: with
         * lumber at 0 it read 0 (and silently dropped the SoL bonus). */
        int amount = colony_prod_building_display_output_sol(
          pool, colony, col1, built, colony_prod_sol_bonus(col1, colony)
        );
        if (badge >= COLONY_CARGO_ICON_BASE && badge < COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_COUNT &&
            view->preview_valid) {
          const int cargo = badge - COLONY_CARGO_ICON_BASE;
          if (view->preview.craft_capacity[cargo] > 0) {
            amount = view->preview.craft_capacity[cargo];
          }
        } else if (view->preview_valid) {
          if (badge == COLONY_ICON_BELL && view->preview.bells > 0) {
            amount = view->preview.bells;
          } else if (badge == COLONY_ICON_CROSS && view->preview.crosses > 0) {
            amount = view->preview.crosses;
          }
        }
        if (amount > 0) {
          colony_screen_draw_resource_count(
            view,
            font,
            framebuffer,
            bx,
            strip_y - 12,
            bw,
            10,
            badge,
            amount,
            15,
            false
          );
        }
      }
    }
  }

  /*
   * The fortification and the docks are ordinary DOS categories now (0 and
   * 2), drawn by the loop above at slots 13 and 14 — no separate corner
   * blit. The outside-unit strip still needs that fence rectangle.
   */
  int fence_x = 0, fence_y = 0, fence_w = COLONY_FENCE_W, fence_h = COLONY_FENCE_H;
  colony_screen_fence_rect(
    view, pool, colony, slot_x, slot_y, &fence_x, &fence_y, &fence_w, &fence_h
  );

  /* Outside units: Note 1 strip centered on the fortification. */
  if (units && view->outside_unit_count > 0 && view->icons_ok) {
    int icons[COLONY_OUTSIDE_MAX];
    int n = 0;
    int selected = -1;
    for (int i = 0; i < view->outside_unit_count && n < COLONY_OUTSIDE_MAX; ++i) {
      const ColonizeUnit* u = units_get_const(units, view->outside_unit_ids[i]);
      if (!colony_screen_unit_on_fence(units, u)) {
        continue;
      }
      const int sprite = colony_screen_outside_display_sprite(units, u);
      if (sprite < 0) {
        continue;
      }
      if (view->selected_outside_unit == u->id) {
        selected = n;
      }
      icons[n++] = sprite;
    }
    if (n > 0) {
      colony_screen_draw_icon_strip(
        view,
        NULL,
        framebuffer,
        fence_x,
        fence_y,
        fence_w,
        fence_h,
        icons,
        n,
        selected,
        15,
        false
      );
    }
  }
}


/* Warehouse strip: icon centered in each COLONY.PIK slot, amount below. */
/* ===================== Cargo/transport strip rendering (colony_screen_draw_cargo_strip .. colony_screen_draw_transports) ===================== */

void colony_screen_draw_cargo_strip(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!colony || !framebuffer) {
    return;
  }

  /*
   * Digit colors, golden-measured (New Amsterdam, exact RGB match against
   * WOODPANL.PIK's palette, not a nearest-color guess): a 3-digit stock's
   * hundreds digit is always gold, independent of the cargo; the rest of
   * the digits are green when this cargo is currently toggled on in the
   * colony's Custom House per-cargo mask (will be auto-sold this EOT) and
   * navy otherwise — the "per-cargo UI chrome" europe.h's
   * europe_custom_house_autosell comment had PARKed. Player-reported: the
   * port was instead drawing every digit in one flat color (white/green/
   * red keyed off an unrelated this-turn production delta). */
  const uint8_t kHundredsColor = 148;
  for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
    const int slot_x = COLONY_CARGO_SLOT_X0 + i * COLONY_CARGO_PITCH;
    const int sprite = COLONY_CARGO_ICON_BASE + i;
    if (view && view->icons_ok && sprite < view->icons.sprite_count) {
      const ColonizeSprite* spr = &view->icons.sprites[sprite];
      const int icon_x = slot_x + (COLONY_CARGO_SLOT_W - spr->width) / 2;
      /* Player-reported: 1px lower than before. */
      ss_blit_sprite(&view->icons, sprite, framebuffer, icon_x, COLONY_CARGO_STRIP_Y + 1);
    }

    if (font) {
      char amount[16];
      int delta = 0;
      if (view && view->last_delta_valid) {
        delta = view->last_delta.goods[i];
      }
      /* Player-reported: 2px lower than before. */
      const int num_y = COLONY_CARGO_NUM_Y + 2;
      if (delta != 0) {
        /* This-turn production delta suffix — a separate, not golden-
         * verified display mode; left as a single flat color (unaffected
         * by this fix) rather than guessing how it'd interact with the
         * hundreds/Custom-House split above. */
        snprintf(amount, sizeof(amount), "%d%+d", colony->stock[i], delta);
        const int tw = font_text_width_skip(font, amount, FONT_SKIP_NONE);
        const int tx = slot_x + (COLONY_CARGO_SLOT_W - tw) / 2;
        const uint8_t col = delta > 0 ? 10 : 12;
        font_draw_text(font, framebuffer, tx, num_y, amount, col);
        continue;
      }
      snprintf(amount, sizeof(amount), "%d", colony->stock[i]);
      const int tw = font_text_width_skip(font, amount, FONT_SKIP_NONE);
      const int tx = slot_x + (COLONY_CARGO_SLOT_W - tw) / 2;
      /* bugs.md: stock past warehouse capacity draws in the alert colour —
       * the excess spoils next turn (over-capacity unloads are allowed).
       * FUN_2f2b_28d6 (viceroy_unpacked.c 49118-49123) wraps that test in
       * `if (local_80 != 0)`: Food is never drawn as over-capacity, because
       * food over capacity is the new-colonist bank, not spoilage. */
      const int wcap = colonies_warehouse_capacity(pool, colony, i);
      const bool over =
        i != COLONIZE_CARGO_FOOD && wcap > 0 && colony->stock[i] > wcap;
      const uint8_t base_col = over
        ? 12
        : (europe_custom_house_cargo_enabled(colony->custom_house_bits, i) ? 10 : 61);
      const size_t len = strlen(amount);
      if (len > 2) {
        char hundreds[16];
        const size_t hlen = len - 2;
        memcpy(hundreds, amount, hlen);
        hundreds[hlen] = '\0';
        font_draw_text(font, framebuffer, tx, num_y, hundreds, kHundredsColor);
        const int hw = font_text_width_skip(font, hundreds, FONT_SKIP_NONE);
        font_draw_text(font, framebuffer, tx + hw, num_y, amount + hlen, base_col);
      } else {
        font_draw_text(font, framebuffer, tx, num_y, amount, base_col);
      }
    }
  }
}

void colony_screen_draw_transports(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !units || !framebuffer) {
    return;
  }
  /* bugs.md: >4 transports overflowed the box — shrink the pitch so the
   * last icon still ends inside (same squeeze the worker strips use). */
  int tr_pitch = COLONY_TRANSPORT_PITCH;
  if (view->docked_transport_count > 1) {
    const int avail = COLONY_TRANSPORT_W - 8 - 16;
    if ((view->docked_transport_count - 1) * tr_pitch > avail) {
      tr_pitch = avail / (view->docked_transport_count - 1);
      if (tr_pitch < 2) {
        tr_pitch = 2;
      }
    }
  }
  for (int i = 0; i < view->docked_transport_count; ++i) {
    const ColonizeUnit* u = units_get_const(units, view->docked_transport_ids[i]);
    if (!u) {
      continue;
    }
    const ColonizeUnitType* type = units_type(units, u->type_index);
    const int x = COLONY_TRANSPORT_X + 4 + i * tr_pitch;
    const int y = COLONY_TRANSPORT_ICON_Y;
    if (type && type->icon_sprite >= 0 && view->icons_ok) {
      unit_chrome_blit_unit_for_palette(
        framebuffer,
        font,
        &view->icons,
        type->icon_sprite,
        x,
        y,
        units_display_type_index(units, u->id),
        u->nation_id,
        u->orders,
        view->docked_transport_count > 1,
        false,
        (view->frame_ok && view->frame.has_palette) ? &view->frame.palette : NULL
      );
      if (view->transport_unit_id == u->id) {
        colony_screen_draw_chrome_selection(view, framebuffer, type->icon_sprite, x, y);
      }
    }
  }

  if (view->transport_unit_id >= 0 && font) {
    const ColonizeUnit* ship = units_get_const(units, view->transport_unit_id);
    const ColonizeUnitType* type = ship ? units_type(units, ship->type_index) : NULL;
    if (type && type->name[0]) {
      font_draw_text(
        font, framebuffer, COLONY_TRANSPORT_X + 4, COLONY_PANEL_CONTENT_Y, type->name, 15
      );
    }
  }

  const int max_holds = 6;
  int open_holds = 0;
  if (view->transport_unit_id >= 0) {
    const ColonizeUnit* ship = units_get_const(units, view->transport_unit_id);
    if (ship) {
      const int goods_holds = units_goods_hold_count(units, view->transport_unit_id);
      open_holds = goods_holds < 0 ? 0 : (goods_holds > max_holds ? max_holds : goods_holds);
      for (int i = 0; i < open_holds; ++i) {
        /* Centre the cargo icon inside the painted 9x12 box interior
         * (COLONY.PIK-measured; see colony_screen.h). */
        int icon_w = COLONY_HOLD_W;
        const int amt = ship->hold_goods_amount[i];
        const int gtype = ship->hold_goods_type[i];
        if (amt > 0 && amt < 255 && gtype >= 0 && gtype < COLONIZE_CARGO_COUNT) {
          const bool partial = amt < 100;
          const int sprite =
            (partial ? COLONY_CARGO_GREY_BASE : COLONY_CARGO_ICON_BASE) + gtype;
          if (view->icons_ok && sprite >= 0 && sprite < view->icons.sprite_count) {
            icon_w = view->icons.sprites[sprite].width;
          }
          const int x = COLONY_HOLD_X + i * COLONY_HOLD_PITCH + (COLONY_HOLD_W - icon_w) / 2;
          const int y = COLONY_HOLD_Y;
          colony_screen_blit_cargo(view, gtype, partial, framebuffer, x, y);
        }
      }
      /*
       * bugs.md: only goods ever appear in a docked transport's holds. A unit
       * in a colony is *in the colony*, standing on the dock — whether it will
       * sail with a ship is not settled until the ship actually leaves, at
       * which point the sentried units on the tile board it
       * (units_ship_departure_pickup). Arriving passengers are put ashore
       * the moment the ship docks (units_try_move), so a docked ship normally
       * has no passengers to draw here at all.
       */
    }
  }
  /* Cover unused holds; with no ship selected, all six are covered. The
   * 10x12 cover (#122) sits over the 9x12 box interior plus its left
   * border, one per painted box on the same 12px pitch. */
  for (int i = open_holds; i < max_holds; ++i) {
    const int x = COLONY_HOLD_X - 1 + i * COLONY_HOLD_PITCH;
    const int y = COLONY_HOLD_Y;
    colony_screen_blit_icon(view, COLONY_ICON_EMPTY_HOLD, framebuffer, x, y);
  }
}
