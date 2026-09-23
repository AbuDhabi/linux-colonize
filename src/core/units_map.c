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
  for (int i = *slot < 0 ? 0 : *slot; i < COLONIZE_UNITS_MAX; ++i) {
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

static void units_clear_slot(ColonizeUnit* unit) {
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
  unit->park_nights = 0;
  unit->mp_spent_turn = 0;
  unit->aboard_moves = -1;
  unit->last_dir = 0;
  unit->col1_origin = 0xff;
  unit->col1_flags15 = 0;
  unit->col1_ai_plan = 0;
  unit->col1_vis_mask = 0;
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
        units_clear_slot(pax);
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
  units_clear_slot(unit);
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
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
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

void units_vis_mask_after_move(
  ColonizeUnitPool* pool, const ColonizeWorldMap* map, int unit_id, int x, int y
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return;
  }
  uint8_t mask = units_vis_mask_for_tile(map, x, y, u->nation_id);
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
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
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

ColonizeUnit* units_get(ColonizeUnitPool* pool, int unit_id) {
  if (!pool || unit_id < 0) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (u->active && u->id == unit_id) {
      return u;
    }
  }
  return NULL;
}

const ColonizeUnit* units_get_const(const ColonizeUnitPool* pool, int unit_id) {
  if (!pool || unit_id < 0) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (u->active && u->id == unit_id) {
      return u;
    }
  }
  return NULL;
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
  if (nation > 3 && map->layer2) {
    const uint8_t l2 = map->layer2[y * map->width + x];
    if ((l2 & MAP_OCCUPANCY_HAS_CITY) != 0) {
      return; /* village tile keeps tribe owner */
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
    units_occupancy_refresh_tile(pool, new_x, new_y, -1);
    /* AI teleport/step movers: same vis reset as units_try_move, for the whole arriving stack. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool->units[i];
      if (units_is_on_map(u) && u->aboard_ship_id < 0 && u->x == new_x && u->y == new_y) {
        units_vis_mask_after_move(pool, units_occupancy_map, u->id, new_x, new_y);
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
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
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
    return "Combat";
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
    return "Village";
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
