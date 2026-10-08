#include "core/units.h"

/*
 * Tile occupancy, sight/vision, combat hook registration & context setters.
 *
 * Sections:
 *  - Tile occupancy/sight/vision, combat hook registration & context setters (units_is_on_map .. units_enter_reason_status)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/combat_analysis.h"
#include "core/combat_strength.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/sound.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/village_trade_intel.h"
#include "core/woodcut.h"
#include "platform/diagnostics.h"
#include "core/units_internal.h"

/* Occupancy map handle shared by the units*.c split (was a units.c static). */
ColonizeWorldMap* units_occupancy_map = NULL;

/* ===================== Tile occupancy/sight/vision, combat hook registration & context setters (units_is_on_map .. units_enter_reason_status) ===================== */


bool units_is_on_map(const ColonizeUnit* unit) {
  /* id < 0 = cleared/ghost slot (tests may flip active without respawn). */
  return unit && unit->active && unit->id >= 0 && unit->aboard_ship_id < 0;
}

/* Header owns the prose: the off-map park (x or y >= 200). */
bool units_coords_in_europe_park(int x, int y) {
  return x >= 200 || y >= 200;
}

/*
 * Tile-stack walk (theme M). DOS parks a boarded passenger off-map at (-2,-2)
 * (FUN_1427_10be), so an on-map test is the DOS-faithful stack filter and a
 * bare `active` test is not: the latter would let passengers answer for the
 * tile their carrier stands on.
 */
ColonizeUnit* units_next_on_tile(ColonizeUnitPool* pool, int x, int y, int* slot) {
  if (!pool || !slot) {
    return NULL;
  }
  for (int i = *slot < 0 ? 0 : *slot; i < units_slot_end(pool); ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    *slot = i + 1;
    return u;
  }
  *slot = COLONIZE_UNITS_MAX;
  return NULL;
}

const ColonizeUnit* units_next_on_tile_const(
  const ColonizeUnitPool* pool, int x, int y, int* slot
) {
  return units_next_on_tile((ColonizeUnitPool*)pool, x, y, slot);
}

int units_count_at(const ColonizeUnitPool* pool, int x, int y) {
  int slot = 0;
  int n = 0;
  while (units_next_on_tile_const(pool, x, y, &slot) != NULL) {
    n++;
  }
  return n;
}

/* ===== Slot index: id -> slot hash and slot_end high-water mark ===== */

#define UNITS_ID_MASK (COLONIZE_UNITS_ID_HASH - 1)

static bool units_slot_in_use(const ColonizeUnit* u) {
  return u->active || u->id > 0;
}

/* Hash position holding `id`, or -1. */
static int units_index_find(const ColonizeUnitPool* pool, int id) {
  for (int h = id & UNITS_ID_MASK, n = 0; n < COLONIZE_UNITS_ID_HASH;
       h = (h + 1) & UNITS_ID_MASK, ++n) {
    const int e = pool->id_slot[h];
    if (e == 0) {
      return -1;
    }
    if (pool->units[e - 1].id == id) {
      return h;
    }
  }
  return -1;
}

void units_index_add(ColonizeUnitPool* pool, ColonizeUnit* slot) {
  const int s = (int)(slot - pool->units);
  if (s + 1 > pool->slot_end) {
    pool->slot_end = s + 1;
  }
  if (slot->id < 0) {
    return;
  }
  int h = slot->id & UNITS_ID_MASK;
  while (pool->id_slot[h] != 0) {
    h = (h + 1) & UNITS_ID_MASK;
  }
  pool->id_slot[h] = (int16_t)(s + 1);
}

/* Linear-probing delete with backward shift, so no tombstones pile up. */
void units_index_remove(ColonizeUnitPool* pool, ColonizeUnit* slot) {
  if (slot->id < 0) {
    return;
  }
  int i = units_index_find(pool, slot->id);
  if (i < 0 || pool->id_slot[i] - 1 != (int)(slot - pool->units)) {
    return;
  }
  for (int j = (i + 1) & UNITS_ID_MASK;; j = (j + 1) & UNITS_ID_MASK) {
    const int e = pool->id_slot[j];
    if (e == 0) {
      break;
    }
    const int home = pool->units[e - 1].id & UNITS_ID_MASK;
    const bool stays = (i <= j) ? (i < home && home <= j) : (i < home || home <= j);
    if (!stays) {
      pool->id_slot[i] = (int16_t)e;
      i = j;
    }
  }
  pool->id_slot[i] = 0;
}

void units_pool_sync(ColonizeUnitPool* pool) {
  if (!pool) {
    return;
  }
  memset(pool->id_slot, 0, sizeof(pool->id_slot));
  pool->slot_end = 0;
  pool->unit_count = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!units_slot_in_use(u)) {
      continue;
    }
    units_index_add(pool, u);
    pool->unit_count++;
  }
}

/* COLONIZE_UNITS_STRICT=1 (set for every ctest run): verify the bookkeeping
 * and abort on a pool filled by hand without units_pool_sync. The allocator
 * runs the full check; units_slot_end and units_get an O(1) one (the slot
 * right at slot_end), and units_get also proves a miss by a scan up to
 * slot_end. */
int g_units_strict;

__attribute__((constructor)) static void units_strict_init(void) {
  const char* e = getenv("COLONIZE_UNITS_STRICT");
  g_units_strict = (e && e[0] == '1') ? 1 : 0;
}

void units_pool_check_slot_end(const ColonizeUnitPool* pool) {
  if (pool->slot_end < COLONIZE_UNITS_MAX && units_slot_in_use(&pool->units[pool->slot_end])) {
    fprintf(stderr,
            "units: slot %d in use at slot_end; call units_pool_sync after filling a "
            "pool by hand\n",
            pool->slot_end);
    abort();
  }
}

void units_pool_check(const ColonizeUnitPool* pool) {
  if (!g_units_strict) {
    return;
  }
  int in_use = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!units_slot_in_use(u)) {
      continue;
    }
    in_use++;
    const int h = u->id >= 0 ? units_index_find(pool, u->id) : -1;
    if (i >= pool->slot_end || h < 0 || pool->id_slot[h] - 1 != i) {
      fprintf(stderr,
              "units: slot %d (id %d) outside slot_end %d or unindexed; "
              "call units_pool_sync after filling a pool by hand\n",
              i, u->id, pool->slot_end);
      abort();
    }
  }
  if (in_use != pool->unit_count) {
    fprintf(stderr, "units: unit_count %d but %d slots in use; call units_pool_sync\n",
            pool->unit_count, in_use);
    abort();
  }
}

static void units_clear_slot(ColonizeUnitPool* pool, ColonizeUnit* unit) {
  const int n = pool->unit_count; /* still counts this unit */
  if (n > 0 && n <= COLONIZE_UNITS_DOS_MAX) {
    for (int i = units_slot_end(pool) - 1; i >= 0; --i) {
      const ColonizeUnit* last = &pool->units[i];
      if (last->active) {
        pool->dos_tail[n - 1].valid = 1;
        pool->dos_tail[n - 1].goto_x = (uint8_t)last->goto_x;
        pool->dos_tail[n - 1].goto_y = (uint8_t)last->goto_y;
        pool->dos_tail[n - 1].facing = (int8_t)last->last_dir;
        pool->dos_tail[n - 1].raw_valid = last->col1_hold_raw_valid ? 1 : 0;
        memcpy(pool->dos_tail[n - 1].raw, last->col1_hold_raw, sizeof(pool->dos_tail[n - 1].raw));
        break;
      }
    }
  }
  units_index_remove(pool, unit);
  unit->active = false;
  unit->id = -1;
  unit->type_index = -1;
  unit->x = 0;
  unit->y = 0;
  unit->moves = 0;
  unit->nation_id = 0;
  unit->aboard_ship_id = -1;
  unit->cargo_count = 0;
  memset(unit->cargo_ids, 0, sizeof(unit->cargo_ids));
  memset(unit->hold_goods_type, 0, sizeof(unit->hold_goods_type));
  memset(unit->hold_goods_amount, 0, sizeof(unit->hold_goods_amount));
  unit->orders = UNITS_ORDER_NONE;
  unit->goto_x = 0xFF;
  unit->goto_y = 0xFF;
  unit->profession = UNITS_JOB_NONE;
  unit->tools = 0;
  unit->muskets = 0;
  unit->horses = 0;
  unit->home_tribe_id = -1;
  unit->col1_counter16 = 0;
  unit->mp_spent_turn = 0;
  unit->aboard_moves = -1;
  unit->last_dir = 0;
  unit->col1_origin = 0xff;
  unit->col1_flags15 = 0;
  unit->col1_ai_plan = 0;
  unit->col1_vis_mask = 0;
  while (pool->slot_end > 0 && !units_slot_in_use(&pool->units[pool->slot_end - 1])) {
    pool->slot_end--;
  }
}

/* DOS-LITERAL FUN_1427_0824 raw 7794-7795: destroying a Euro unit takes it
 * off the FUN_4962_0018 all_unit_counts snapshot (DS:0x8cfc) at once; the
 * next census of that nation recounts. */
static void units_census_count_destroy(const ColonizeUnit* unit) {
  const int n = unit->nation_id & 0xf;
  if (g_units_fallout_col1 && n < 4 && g_units_fallout_col1->stuff.all_unit_counts[n] != 0) {
    g_units_fallout_col1->stuff.all_unit_counts[n]--;
  }
}

bool units_despawn(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* unit = units_get(pool, unit_id);
  if (!unit) {
    return false;
  }
  const int ox = unit->x;
  const int oy = unit->y;
  const int was_on_map = units_is_on_map(unit) ? 1 : 0;
  /* Passengers ride with the ship — despawn them if this is a carrier. */
  if (unit->cargo_count > 0) {
    for (int i = 0; i < unit->cargo_count; ++i) {
      ColonizeUnit* pax = units_get(pool, unit->cargo_ids[i]);
      if (pax) {
        units_census_count_destroy(pax);
        units_clear_slot(pool, pax);
        if (pool->unit_count > 0) {
          pool->unit_count--;
        }
        if (pool->selected_id == unit->cargo_ids[i]) {
          pool->selected_id = -1;
        }
      }
    }
  }
  /* If this unit is aboard a ship, remove it from that ship's hold. */
  if (unit->aboard_ship_id >= 0) {
    ColonizeUnit* ship = units_get(pool, unit->aboard_ship_id);
    if (ship) {
      for (int i = 0; i < ship->cargo_count; ++i) {
        if (ship->cargo_ids[i] == unit_id) {
          for (int j = i + 1; j < ship->cargo_count; ++j) {
            ship->cargo_ids[j - 1] = ship->cargo_ids[j];
          }
          ship->cargo_count--;
          break;
        }
      }
    }
  }
  /*
   * DOS-LITERAL FUN_1427_0824 raw 7796-7799 (bugs.md #837): unit DESTROY is
   * the producer of the settlement "needs first colonist" bit —
   *   if ((3 < (unit+0x3147 & 0xf)) && (-1 < (char)unit+0x314a))
   *     *(byte*)(unit+0x314a * 0x12 + 0x54ef) |= 1;
   * i.e. any Indian-owned unit with a valid home village flags that village,
   * and FUN_4d56_152e raw 81410-81437 consumes it as `local_16 = 1` and
   * issues one replacement Brave armed out of tribe stock. DOS sets it with
   * no check on the village's own nation, and the village-destroy path
   * (FUN_4d56_00e0 raw 81313, FUN_281f_0808) routes through 0824 too — the
   * bit then lands on a record the very next compaction overwrites, so it is
   * harmless there. units_despawn is the single chokepoint every port
   * despawn goes through, matching 0824.
   */
  if (g_units_fallout_col1 && g_units_fallout_col1->tribe && unit->nation_id >= 4 &&
      unit->home_tribe_id >= 0 &&
      unit->home_tribe_id < (int)g_units_fallout_col1->head.tribe_count) {
    g_units_fallout_col1->tribe[unit->home_tribe_id].state.needs_colonist = 1;
  }
  units_census_count_destroy(unit);
  units_clear_slot(pool, unit);
  if (pool->unit_count > 0) {
    pool->unit_count--;
  }
  if (pool->selected_id == unit_id) {
    pool->selected_id = -1;
  }
  if (was_on_map) {
    units_occupancy_refresh_tile(pool, ox, oy, unit_id);
  }
  return true;
}

bool units_is_sea(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  if (!unit) {
    return false;
  }
  const ColonizeUnitType* type = units_type(pool, unit->type_index);
  return type && type->domain == COLONIZE_UNIT_DOMAIN_SEA;
}

bool units_unit_is_sea(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  const ColonizeUnitType* type = u ? units_type(pool, u->type_index) : NULL;
  return type && type->domain == COLONIZE_UNIT_DOMAIN_SEA;
}

int units_sight_radius(
  const ColonizeUnitPool* pool, const ColonizeUnit* u, const ColonizeCol1Save* col1
) {
  if (!pool || !u) {
    return 1;
  }
  int radius = 1;
  const bool ship = units_unit_is_sea(pool, u);
  /*
   * DOS-LITERAL FUN_13f1_02f8 (viceroy_unpacked.c raw 7226-7238; asm
   * CODE_17:13f1:02f8-0356). The radius is NOT a @UNIT column — 02f8 builds
   * it in DI from three hardcoded type compares and hands it to
   * FUN_13f1_02b4 in DX (02b4 itself only turns type 0xd..0x12 into the
   * ship/land flag for the reveal loop, which is why it looks radius-free):
   *   MOV DI,1                                  ; default
   *   CMP [BX+0x3146],0xf / 0x10 / 0x11 -> DI=2 ; Galleon/Privateer/Frigate
   *   FUN_15eb_3960(nation, 7) && !ship -> DI=2 ; de Soto
   *   CMP [BX+0x3146],0x5 -> INC DI             ; Scouts
   * Man-O-War is type 0x12 and is deliberately NOT in the list: radius 1.
   */
  if (u->type_index == units_kind_type_index(pool, UNITS_KIND_GALLEON) ||
      u->type_index == units_kind_type_index(pool, UNITS_KIND_PRIVATEER) ||
      u->type_index == units_kind_type_index(pool, UNITS_KIND_FRIGATE)) {
    radius = 2;
  }
  /* 13f1:0321 — FF 7 (de Soto) and not a ship (type outside 0xd..0x12). */
  if (!ship && col1 && u->nation_id >= 0 && u->nation_id < 4 &&
      founding_fathers_nation_has(col1, u->nation_id, FF_HERNANDO_DE_SOTO)) {
    radius = 2;
  }
  /* 13f1:034b — Scouts (row 5) +1. */
  if (u->type_index == units_kind_type_index(pool, UNITS_KIND_SCOUT)) {
    radius += 1;
  }
  return radius;
}

typedef struct UnitsRevealCtx {
  ColonizeWorldMap* map;
  ColonizeUnitPool* pool;
  ColonizeColonyPool* colonies;
  int nation;
  bool pacific;
} UnitsRevealCtx;

/* FUN_13f1_000a tail: owner stamp, unit vis bits, colony snapshot. */
static void units_reveal_tile_effects(void* vctx, int x, int y, bool outer) {
  UnitsRevealCtx* ctx = (UnitsRevealCtx*)vctx;
  ColonizeWorldMap* map = ctx->map;
  if (!outer && !ctx->pacific && map->layer2 &&
      (map_tile_is_water(map, x, y) || map_tile_is_high_seas(map, x, y)) &&
      (map->layer2[y * map->width + x] & MAP_LAYER2_PACIFIC) != 0) {
    ctx->pacific = true;
  }
  if (map->layer3) {
    const int owner = (map_get_layer3(map, x, y) >> 4) & 0x0f;
    /* FUN_137f_0200 < 0 (nibble 0xf) and not a rumour spot (FUN_137f_0598). */
    if (owner == 0x0f && !map_tile_has_rumour(map, x, y)) {
      /*
       * Never claim a settlement tile: village nibble must stay the tribe's
       * nation (DOS invariant; euro nibble there triggers @COLONYFLAG).
       */
      const bool settlement =
        map->layer2 != NULL &&
        (map->layer2[(size_t)y * (size_t)map->width + (size_t)x] & MAP_OCCUPANCY_HAS_CITY) != 0;
      if (!settlement) {
        map_set_owner_nibble(map, x, y, ctx->nation);
      }
    }
  }
  const uint8_t bit = (uint8_t)(1u << (ctx->nation & 3));
  for (int i = 0; i < units_slot_end(ctx->pool); ++i) {
    ColonizeUnit* o = &ctx->pool->units[i];
    if (!units_is_on_map(o) || o->x != x || o->y != y) {
      continue;
    }
    /* Outer ring only marks European units (FUN_13f1_000a `param_1 == 0 || owner < 4`). */
    if (outer && (o->nation_id < 0 || o->nation_id > 3)) {
      continue;
    }
    o->col1_vis_mask = (uint8_t)(o->col1_vis_mask | bit);
  }
  if (ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, x, y);
    if (cid >= 0) {
      colonies_fog_snapshot(ctx->colonies, cid, ctx->nation);
    }
  }
}

bool units_reveal_sight_w(
  const ColonizeWorld* w,
  const ColonizeUnit* u
) {
  ColonizeWorldMap* map = w->map;
  ColonizeUnitPool* pool = w->units;
  ColonizeColonyPool* colonies = w->colonies;
  const ColonizeCol1Save* col1 = w->col1;

  if (!map || !pool || !u || !units_is_on_map(u) || u->nation_id < 0 || u->nation_id > 3) {
    return false;
  }
  UnitsRevealCtx ctx = {map, pool, colonies, u->nation_id, false};
  map_reveal_sight_each(
    map,
    u->x,
    u->y,
    u->nation_id,
    units_sight_radius(pool, u, col1),
    units_unit_is_sea(pool, u),
    units_reveal_tile_effects,
    &ctx
  );
  return ctx.pacific;
}


uint8_t units_vis_mask_for_tile(const ColonizeWorldMap* map, int x, int y, int mover_nation) {
  uint8_t mask = 0;
  if (!map) {
    return 0;
  }
  if (mover_nation >= 0 && mover_nation < 4 && map->layer3 && x >= 0 && y >= 0 &&
      x < map->width && y < map->height) {
    const int owner = (map_get_layer3(map, x, y) >> 4) & 0x0f;
    if (owner < 4) { /* 0x10 << nibble, byte-truncated: nibbles ≥ 4 contribute nothing */
      mask = (uint8_t)(mask | (1u << owner));
    }
  }
  for (int n = 0; n < 4; ++n) {
    if (map_nation_watches_tile(map, x, y, n)) {
      mask = (uint8_t)(mask | (1u << n));
    }
  }
  return mask;
}

int units_tile_owner_nibble(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->layer3 || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return -1;
  }
  const int owner = (map_get_layer3(map, x, y) >> 4) & 0x0f;
  return owner == 0x0f ? -1 : owner;
}

void units_vis_mask_after_move(
  ColonizeUnitPool* pool, const ColonizeWorldMap* map, int unit_id, int x, int y, int dest_owner
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return;
  }
  /* -1 as mover nation: 0c9a's own owner term is replaced by 07d6 below. */
  uint8_t mask = units_vis_mask_for_tile(map, x, y, -1);
  if (dest_owner >= 0 && dest_owner < 4) { /* 0x10 << nibble, byte-truncated */
    mask = (uint8_t)(mask | (1u << dest_owner));
  }
  if (u->nation_id >= 0 && u->nation_id < 4) {
    mask = (uint8_t)(mask | (1u << (u->nation_id & 3)));
  }
  u->col1_vis_mask = mask;
  for (int i = 0; i < u->cargo_count; ++i) {
    ColonizeUnit* pax = units_get(pool, u->cargo_ids[i]);
    if (pax && pax->active) {
      pax->col1_vis_mask = mask;
    }
  }
}

int units_count_sea_for_nation(const ColonizeUnitPool* pool, int nation_id) {
  if (!pool) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < units_slot_end(pool); ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (u->active && u->nation_id == nation_id && units_unit_is_sea(pool, u)) {
      n++;
    }
  }
  return n;
}

bool units_on_high_seas(const ColonizeWorldMap* map, int x, int y) {
  return map_tile_is_high_seas(map, x, y);
}

void units_founder_loot(
  const ColonizeUnitPool* pool,
  int unit_id,
  int* out_tools,
  int* out_muskets,
  int* out_horses
) {
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  if (unit) {
    /* Trust carried gear fields. Do not invent 100 tools from the type name —
     * that hid FUN_479b_0158 wear (depleted pioneers looked fully stocked). */
    tools = unit->tools > 0 ? unit->tools : 0;
    muskets = unit->muskets > 0 ? unit->muskets : 0;
    horses = unit->horses > 0 ? unit->horses : 0;
  }
  if (out_tools) {
    *out_tools = tools;
  }
  if (out_muskets) {
    *out_muskets = muskets;
  }
  if (out_horses) {
    *out_horses = horses;
  }
}

int units_id_at(const ColonizeUnitPool* pool, int x, int y) {
  int slot = 0;
  const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot);
  return u ? u->id : -1;
}

void units_tile_stack_arrive(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* unit = units_get(pool, unit_id);
  if (!pool || !unit) {
    return;
  }
  /* Zero is the empty/reset sentinel. A 64-bit order keeps long-lived games
   * from confusing later arrivals with the first tile occupant. */
  if (pool->next_tile_stack_order == 0) {
    pool->next_tile_stack_order = 1;
  }
  unit->tile_stack_order = pool->next_tile_stack_order++;
}

/* FUN_1427_04d6 mode 0 bucket: transports, Treasure, then @UNIT size 6..1; 0 is never moved. */
static int units_stack_rank_04d6(const ColonizeUnitPool* pool, int id) {
  const ColonizeUnit* u = units_get_const(pool, id);
  const ColonizeUnitType* t = u ? units_type(pool, u->type_index) : NULL;
  if (t && t->cargo > 0) {
    return 8;
  }
  if (u && u->type_index == UNITS_KIND_TREASURE) {
    return 7;
  }
  return t && t->space >= 1 && t->space <= 6 ? t->space : 0;
}

/*
 * DOS-LITERAL FUN_1427_04d6(unit, 0) raw 7612-7677 on a bottom→top id list:
 * each bucket is peeled top-down onto the -3 chain, which is relinked
 * top-first, so the chain ends rank-ascending, stable in its old order.
 */
static void units_stack_sort_04d6(const ColonizeUnitPool* pool, int* ids, int n) {
  for (int i = 1; i < n; ++i) {
    const int id = ids[i];
    const int r = units_stack_rank_04d6(pool, id);
    int at = i;
    while (at > 0 && units_stack_rank_04d6(pool, ids[at - 1]) > r) {
      ids[at] = ids[at - 1];
      --at;
    }
    ids[at] = id;
  }
}

/* Bottom→top insertion by tile_stack_order. */
static int units_stack_push_ordered(const ColonizeUnitPool* pool, int* ids, int n, int from, int id) {
  const uint64_t o = units_get_const(pool, id)->tile_stack_order;
  int at = n;
  while (at > from && units_get_const(pool, ids[at - 1])->tile_stack_order > o) {
    ids[at] = ids[at - 1];
    --at;
  }
  ids[at] = id;
  return n + 1;
}

void units_tile_stack_ship_relink(ColonizeUnitPool* pool, int ship_id, bool sort_group) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship) {
    return;
  }
  /* FUN_1427_10be raw 8606-8680: 04d6 the ship's tile first. */
  static int tile[COLONIZE_UNITS_MAX];
  int tn = 0;
  for (int i = 0; i < units_slot_end(pool); ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (u->active && u->x == ship->x && u->y == ship->y) {
      tn = units_stack_push_ordered(pool, tile, tn, 0, u->id);
    }
  }
  units_stack_sort_04d6(pool, tile, tn);
  for (int i = 0; i < tn; ++i) {
    units_tile_stack_arrive(pool, tile[i]);
  }
  /* The hull goes to the -2 chain, then its passengers walked top-down. */
  int chain[COLONIZE_UNIT_CARGO_MAX + 1];
  int n = 1;
  chain[0] = ship->id;
  for (int i = 0; i < ship->cargo_count && n <= COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (units_get_const(pool, ship->cargo_ids[i])) {
      n = units_stack_push_ordered(pool, chain, n, 1, ship->cargo_ids[i]);
    }
  }
  for (int i = 1, j = n - 1; i < j; ++i, --j) {
    const int t = chain[i];
    chain[i] = chain[j];
    chain[j] = t;
  }
  /* The mover's FUN_281f_08e4 = FUN_1427_0644 runs 04d6 on the -2 chain
   * (raw 75704; DOSBox trace 2026-10-06, docs/idle_campaign.md). */
  if (sort_group) {
    units_stack_sort_04d6(pool, chain, n);
  }
  /* FUN_1427_040c relinks the -2 chain top-first: it lands reversed. */
  for (int i = n - 1; i >= 0; --i) {
    units_tile_stack_arrive(pool, chain[i]);
  }
}

int units_tile_head_id_at(const ColonizeUnitPool* pool, int x, int y) {
  if (!pool) {
    return -1;
  }
  int head_id = -1;
  uint64_t head_order = 0;
  for (int i = 0; i < units_slot_end(pool); ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    if (head_id < 0 || u->tile_stack_order > head_order) {
      head_id = u->id;
      head_order = u->tile_stack_order;
    }
  }
  return head_id;
}

ColonizeUnit* units_get(ColonizeUnitPool* pool, int unit_id) {
  return (ColonizeUnit*)units_get_const(pool, unit_id);
}

const ColonizeUnit* units_get_const(const ColonizeUnitPool* pool, int unit_id) {
  if (!pool || unit_id < 0) {
    return NULL;
  }
  if (g_units_strict) {
    units_pool_check_slot_end(pool);
  }
  const int h = units_index_find(pool, unit_id);
  if (h < 0) {
    if (g_units_strict) {
      /* Slots past slot_end are covered by the allocator's full check. */
      for (int i = 0; i < pool->slot_end; ++i) {
        if (pool->units[i].active && pool->units[i].id == unit_id) {
          fprintf(stderr, "units: id %d active in slot %d but unindexed; call units_pool_sync\n",
                  unit_id, i);
          abort();
        }
      }
    }
    return NULL;
  }
  const ColonizeUnit* u = &pool->units[pool->id_slot[h] - 1];
  return u->active ? u : NULL;
}

const ColonizeUnitType* units_type(const ColonizeUnitPool* pool, int type_index) {
  if (!pool || type_index < 0 || type_index >= pool->type_count) {
    return NULL;
  }
  return &pool->types[type_index];
}

/* Debug-log helpers (defined with the order commands below). */
const char* units_order_name(int orders);
void units_log_ident(
  const ColonizeUnitPool* pool,
  int unit_id,
  char* out,
  size_t out_size
);

int g_units_last_combat = 0;
ColonizeEnterReason g_units_last_enter_reason = COLONIZE_ENTER_OK;
const ColonizeCol1Save* g_units_ff_col1 = NULL;
ColonizeCol1Save* g_units_fallout_col1 = NULL;
ColonizeWorldMap* g_units_fallout_map = NULL;
static int g_units_conquest_gold = -1;
const ColonizeColonyPool* g_units_combat_colonies = NULL;
int g_units_combat_human_nation = -1;
AiPopupState* g_units_combat_popups = NULL;
const ColonizeMsgCatalog* g_units_combat_game_txt = NULL;
EuropeScreen* g_units_combat_europe = NULL;
ColonizeUnitsMoveWatchFn g_units_move_watch = NULL;
ColonizeUnitsCombatWatchFn g_units_combat_watch = NULL;
void* g_units_combat_watch_user = NULL;

void units_set_combat_watch(ColonizeUnitsCombatWatchFn fn, void* user) {
  g_units_combat_watch = fn;
  g_units_combat_watch_user = user;
}

void units_combat_watch_notify(const ColonizeUnitPool* pool, int unit_id, int x, int y) {
  if (g_units_combat_watch && pool) {
    g_units_combat_watch(g_units_combat_watch_user, pool, unit_id, x, y);
  }
}

/*
 * DOS fizzle present (FUN_12d6_0000 / FUN_281f_03ea) — see units.h. Phase 0
 * snapshots the pre-outcome frame, phase 1 dissolves to the post-outcome
 * frame. No-op headless.
 */
ColonizeUnitsDissolveFn g_units_dissolve = NULL;
void* g_units_dissolve_user = NULL;

void units_set_combat_dissolve(ColonizeUnitsDissolveFn fn, void* user) {
  g_units_dissolve = fn;
  g_units_dissolve_user = user;
}

/* 1b0e raid handoff — registered by ai_contact.c's constructor (units.h). */
ColonizeUnitsRaidRepelledFn g_units_raid_repelled = NULL;

void units_set_colony_raid_repelled(ColonizeUnitsRaidRepelledFn fn) {
  g_units_raid_repelled = fn;
}

void units_dissolve_notify(int phase) {
  if (g_units_dissolve) {
    g_units_dissolve(g_units_dissolve_user, phase);
  }
}

/*
 * bugs.md: every combat concludes in isolation — the popups it queued are
 * presented (and answered) before the NEXT combat runs, instead of being
 * hoarded until the whole AI slice ends. The hook is a nested modal loop in
 * game_loop (headless callers leave it unset).
 */
ColonizeUnitsPopupPumpFn g_units_popup_pump = NULL;
void* g_units_popup_pump_user = NULL;

void units_set_combat_popup_pump(ColonizeUnitsPopupPumpFn fn, void* user) {
  g_units_popup_pump = fn;
  g_units_popup_pump_user = user;
}

void units_combat_pump_popups(void) {
  if (g_units_popup_pump) {
    g_units_popup_pump(g_units_popup_pump_user);
  }
}

/* Public pump for non-combat callers that must block on queued popups before
 * animating (bugs.md #237: REF landing popup precedes the disembark slides). */
void units_pump_combat_popups(void) {
  units_combat_pump_popups();
}
void* g_units_move_watch_user = NULL;

void units_set_ff_col1(const ColonizeCol1Save* col1) {
  g_units_ff_col1 = col1;
}

void units_set_move_watch(ColonizeUnitsMoveWatchFn fn, void* user) {
  g_units_move_watch = fn;
  g_units_move_watch_user = user;
}

void units_move_watch_notify(
  const ColonizeUnitPool* pool, const ColonizeWorldMap* map, const ColonizeColonyPool* colonies,
  int unit_id, int from_x, int from_y
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (g_units_move_watch && u && units_is_on_map(u)) {
    g_units_move_watch(g_units_move_watch_user, pool, map, colonies, unit_id, from_x, from_y, u->x, u->y);
  }
}

void units_set_combat_human_nation(int human_nation) {
  g_units_combat_human_nation = human_nation;
}

void units_set_combat_popups(AiPopupState* popups, const ColonizeMsgCatalog* game_txt) {
  g_units_combat_popups = popups;
  g_units_combat_game_txt = game_txt;
}

/*
 * DOS DS:0x8d03 bit 2, set by FUN_5fef_1b0e's Revere arm (asm 5fef:1d2f-1d5f)
 * on the auto-spawned colony defender and read back by the 636c Combat
 * Analysis panel as the "Muskets" row. DOS writes a global there because the
 * arm runs while the defender is being built, before the analysis is drawn;
 * the port mirrors that with a one-shot latch consumed by the next
 * engagement (units_revere_defend_colony_tile → units_resolve_land_combat_ff).
 */
int g_units_revere_muskets_latch = 0;

/*
 * DOS scratch @UNIT row 0x17 — the unit id of the phantom defender
 * FUN_5fef_1b0e auto-spawns (DOS `bVar28`) for an undefended colony
 * (units_spawn_colony_temp_defender) or an empty dwelling
 * (units_spawn_village_temp_defender). BOTH arms build it through
 * FUN_291f_0a20 (= FUN_478c_002c, raw 76545-76564), which stamps unit type
 * byte 0x17, and 1b0e deletes it with FUN_291f_0a06 (raw 100636-100639)
 * *before* the win/lose branch:
 *
 *     uVar25 = 0x281f;
 *     if (bVar28) { uVar25 = 0x291f; FUN_291f_0a06(0x281f); }
 *     if (bVar8) { ... if (bVar28) { colony/village consequences } else { 0352 } }
 *
 * Two consequences the port has to honour:
 *   • FUN_5fef_0352 (units_apply_land_loss_outcome) is reached only on the
 *     `else` limb, i.e. never for the phantom — no @COLONISTCAPTURE, no
 *     @DEMOTE, no nation flip. The town's real consequence is the walk-in
 *     that follows (units_try_capture_foreign_colony / the village drain).
 *   • FUN_5fef_172c (units_promote_on_win) opens with
 *     `if (type byte == 1 || type byte == 4)`; a 0x17 row fails it and
 *     returns before FUN_281f_04d4 — so a phantom that WINS draws no
 *     promotion RNG. Keeping the draw here would also desync the stream.
 *
 * `bVar28` itself is already carried by combat_set_auto_defender /
 * combat_auto_defender (combat_strength.c reads it for the beginner shield),
 * raised by exactly the two auto-spawn arms and lowered the moment the
 * engagement returns — so that is the predicate both gates below use.
 */
bool units_defender_is_dos_scratch_row(void) {
  return combat_auto_defender();
}

/*
 * FUN_5fef_1b0e `bVar13` / `bVar14` (raw 100733 / 100741, read at raw
 * 101088-101095 to append '1' / '2' to the @INDIANWIN tag): the native gear
 * step that just fired, for the chrome owner. bugs.md #645.
 */
int g_units_native_gear_armed = 0;
int g_units_native_gear_mounted = 0;
int g_units_loss_winner_attacked = 1;

void units_last_native_gear_step(int* out_armed, int* out_mounted) {
  if (out_armed) {
    *out_armed = g_units_native_gear_armed;
  }
  if (out_mounted) {
    *out_mounted = g_units_native_gear_mounted;
  }
}

void units_clear_native_gear_step(void) {
  g_units_native_gear_armed = 0;
  g_units_native_gear_mounted = 0;
}

/* See units.h: ai_contact owns the richer ambush chrome for its own calls. */
int g_units_native_chrome_owned = 0;
void units_set_native_combat_chrome_owned(int owned) {
  g_units_native_chrome_owned = owned;
}

ColonizeCombatStrengthCtx units_combat_strength_ctx(const ColonizeCol1Save* col1) {
  ColonizeCombatStrengthCtx ctx;
  ctx.units = NULL; /* filled by caller */
  ctx.map = units_occupancy_map ? units_occupancy_map : g_units_fallout_map;
  ctx.colonies = g_units_combat_colonies;
  ctx.col1 = col1 ? col1 : g_units_ff_col1;
  return ctx;
}

/* FUN_281f_0768 — ocean or high seas (terrain 0x19 / 0x1a). */
int units_tile_is_ocean_or_hs(const ColonizeCol1Save* col1, int x, int y) {
  const ColonizeCombatStrengthCtx sctx = units_combat_strength_ctx(col1);
  if (!sctx.map) {
    return 0;
  }
  return (map_tile_is_water(sctx.map, x, y) || map_tile_is_high_seas(sctx.map, x, y)) ? 1 : 0;
}

void units_combat_maybe_present_analysis(
  const ColonizeCol1Save* col1,
  const ColonizeCombatEngagement* eng,
  int atk_nation,
  int def_nation
) {
  if (!eng || !combat_analysis_should_show(col1, atk_nation, def_nation, g_units_combat_human_nation)) {
    return;
  }
  combat_analysis_present_if_hooked(eng);
}

void units_set_occupancy_map(ColonizeWorldMap* map) {
  units_occupancy_map = map;
}

static int units_tile_has_on_map_unit(const ColonizeUnitPool* pool, int x, int y, int except_id) {
  if (!pool || x < 0 || y < 0 || x >= 200 || y >= 200) {
    return 0;
  }
  int slot = 0;
  for (const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot); u != NULL;
       u = units_next_on_tile_const(pool, x, y, &slot)) {
    if (u->id != except_id) {
      return 1;
    }
  }
  return 0;
}

/*
 * FUN_1427_02ca: when a unit is alone on a tile, stamp layer3 owner (nation).
 * Indians skip when the tribe bit is set (FUN_137f_0598). Leaving a tile
 * (FUN_1427_023a) clears presence only — owner nibble stays claimed.
 */
void units_claim_tile_owner_from_stack(
  ColonizeUnitPool* pool,
  ColonizeWorldMap* map,
  int x,
  int y,
  int except_id
) {
  if (!pool || !map || !map->layer3) {
    return;
  }
  if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return;
  }
  int nation = -1;
  int slot = 0;
  for (const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot); u != NULL;
       u = units_next_on_tile_const(pool, x, y, &slot)) {
    if (u->id != except_id) {
      nation = u->nation_id;
      break;
    }
  }
  if (nation < 0) {
    return;
  }
  if (map->layer2) {
    const int i = y * map->width + x;
    /* Village tile keeps its tribe owner. A Euro nibble is never valid there:
     * DOS creates a unit with its nation (FUN_1427_06b4), while a port spawn
     * runs this with the nation-0 placeholder before the caller sets it (a
     * temp defender for an empty village stamped England on the village). */
    if ((map->layer2[i] & MAP_OCCUPANCY_HAS_CITY) != 0 &&
        (nation > 3 || (map->layer3[i] >> 4) >= 4)) {
      return;
    }
  }
  map_set_owner_nibble(map, x, y, nation);
}

void units_occupancy_notify_moved(ColonizeUnitPool* pool, int old_x, int old_y, int new_x, int new_y) {
  if (!pool || !units_occupancy_map) {
    return;
  }
  if (old_x >= 0 && old_y >= 0 && old_x < 200 && old_y < 200) {
    units_occupancy_refresh_tile(pool, old_x, old_y, -1);
  }
  if (new_x >= 0 && new_y >= 0 && new_x < 200 && new_y < 200) {
    const int dest_owner = units_tile_owner_nibble(units_occupancy_map, new_x, new_y);
    units_occupancy_refresh_tile(pool, new_x, new_y, -1);
    /* AI teleport/step movers: same vis reset as units_try_move, for the whole arriving stack. */
    for (int i = 0; i < units_slot_end(pool); ++i) {
      const ColonizeUnit* u = &pool->units[i];
      if (units_is_on_map(u) && u->aboard_ship_id < 0 && u->x == new_x && u->y == new_y) {
        units_vis_mask_after_move(pool, units_occupancy_map, u->id, new_x, new_y, dest_owner);
      }
    }
  }
}

void units_occupancy_rebuild(ColonizeUnitPool* pool) {
  ColonizeWorldMap* map = units_occupancy_map;
  if (!pool || !map || !map->layer2) {
    return;
  }
  /*
   * Presence bit (DOS UNITFLAG, layer2 0x01) recomputed from the pool as a
   * safety net for movers that bypass units_occupancy_notify_moved. Only the
   * bit: the layer3 owner nibble is stamped at placement (DOS FUN_1427_02ca)
   * and otherwise left alone — DOS keeps old stamps (seed-100 TURN4: the
   * Dutch ship's whole route still reads 3) and a save-loaded unit standing
   * on a foreign-stamped tile keeps that stamp until it moves.
   */
  for (size_t i = 0; i < map->tile_count; ++i) {
    map->layer2[i] = (uint8_t)(map->layer2[i] & (uint8_t)~MAP_OCCUPANCY_HAS_UNIT);
  }
  for (int i = 0; i < units_slot_end(pool); ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->aboard_ship_id >= 0 || !units_is_on_map(u) || u->x >= 200 ||
        u->y >= 200) {
      continue;
    }
    map_occupancy_set_layer2(map, u->x, u->y, MAP_OCCUPANCY_HAS_UNIT, true);
  }
}

void units_occupancy_refresh_tile(ColonizeUnitPool* pool, int x, int y, int except_id) {
  if (!units_occupancy_map || !pool) {
    return;
  }
  const int present = units_tile_has_on_map_unit(pool, x, y, except_id);
  map_occupancy_set_layer2(
    units_occupancy_map, x, y, MAP_OCCUPANCY_HAS_UNIT, present != 0
  );
  if (present) {
    units_claim_tile_owner_from_stack(pool, units_occupancy_map, x, y, except_id);
  }
}

void units_set_native_fallout_context(
  ColonizeCol1Save* col1,
  ColonizeWorldMap* map,
  int conquest_gold
) {
  g_units_fallout_col1 = col1;
  g_units_fallout_map = map;
  g_units_conquest_gold = conquest_gold;
}

void units_set_combat_colonies(const ColonizeColonyPool* colonies) {
  g_units_combat_colonies = colonies;
}

void units_set_combat_europe(struct EuropeScreen* europe) {
  g_units_combat_europe = (EuropeScreen*)europe;
}

int units_last_combat_outcome(void) {
  return g_units_last_combat;
}

ColonizeEnterReason units_last_enter_reason(void) {
  return g_units_last_enter_reason;
}

const char* units_enter_reason_status(ColonizeEnterReason reason) {
  switch (reason) {
  case COLONIZE_ENTER_OK:
  case COLONIZE_ENTER_DOCK:
    return "Moved";
  case COLONIZE_ENTER_LANDFALL:
    return "Landfall";
  case COLONIZE_ENTER_COMBAT_LAND:
  case COLONIZE_ENTER_COMBAT_NAVAL:
    return "Battle started";
  case COLONIZE_ENTER_BOUNCE_FOREIGN:
    return "Cannot attack (non-combat unit)";
  case COLONIZE_ENTER_BOUNCE_PEACE:
    return "At peace — cannot attack";
  case COLONIZE_ENTER_BLOCKED_DOMAIN:
    return "Wrong terrain";
  case COLONIZE_ENTER_BLOCKED_EDGE:
  case COLONIZE_ENTER_EDGE_SAIL: /* bugs.md #717 */
    return "Map edge";
  case COLONIZE_ENTER_BLOCKED_HS_SAIL:
    return "Need sail order for high seas";
  case COLONIZE_ENTER_VILLAGE_ILLEGAL:
    return "Illegal entry into village";
  case COLONIZE_ENTER_BOARD:
    return "Boarded ship";
  case COLONIZE_ENTER_VILLAGE_SHIP:
    return "Native settlement entered";
  case COLONIZE_ENTER_LAKE_BLOCKED:
    return "Ship cannot enter lake";
  case COLONIZE_ENTER_LANDFIRST:
    return "Must unload troops first";
  case COLONIZE_ENTER_NO_MP:
    return "No moves left";
  case COLONIZE_ENTER_BLOCKED:
  default:
    return "Move blocked";
  }
}
