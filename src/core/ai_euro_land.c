/*
 * Euro AI — FUN_521d_20e6 land band: type cache, tile improvement/roads, explorer/patrol/village/labor arms, wander + move-scoring gate, attack
 *
 * Split out of ai_euro.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_euro_internal.h. See ai_euro.c for the dispatcher entry point.
 *
 * Sections:
 *   FUN_521d_20e6 type tables + live catalog cache
 *   FUN_5952_035e colony-tick tile improvement (raw 94402-94551) + road connect
 *   20e6 prologue, continent queries, explorer flag, patrol / surplus recall arms
 *   20e6 village + labor arms, land explore scan, attack term
 *   20e6 wander step, border park, ship far-roam, ring hop
 *   20e6 treasure cash-in, 47b9 dead end, wagon origin walk, 457e HS cadence
 *   ai_euro_move_scoring_gate, @VIOLATE notify, ai_euro_try_attack
 */

#include "core/internal.h"
#include "core/ai_euro.h"

#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_king.h"
#include "core/ai_goals.h"
#include "core/assets.h"
#include "core/ai_euro_internal.h"
#include "core/colony.h"
#include "core/colony_craft.h"
#include "core/colony_yield.h"
#include "core/colony_production.h"
#include "core/col1_save.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/units.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * bugs.md #655: DS:0x5236 (@UNIT column 4, ATTACK) and DS:0x523d (@UNIT
 * column 12, the capability bit-string) are now catalog fields
 * (ColonizeUnitType.attack / .cap_bits, units_load_types). These two
 * NAMES.TXT-default tables are kept ONLY as the fallback for a pool whose
 * types did not come from a real NAMES.TXT load (hand-built test pools —
 * see ai_euro_20e6_dos_type's own -1 fallback comment); the live tables
 * below (s_20e6_type_combat_live / s_20e6_type_flags_live), refreshed from
 * the loaded catalog once per dispatcher turn, are read first.
 */
static const uint8_t k_20e6_type_combat[AI_20E6_TYPE_COUNT] = {
  /* NAMES.TXT @UNIT column 4 = ATTACK (DS:0x5236), same column the live
   * cache reads (t->attack); bugs.md #668 (was the DEFENSE column). */
  0, 2, 0, 0, 3, 1, 5, 5, 6, 4, 0, 7, 0, 0, 0, 0, 8, 16, 24, 1, 2, 2, 3
};
/*
 * DS:0x523d unit-type capability flags = NAMES.TXT @UNIT trailing bit-string
 * read MSB-first (Brave "00111000" → 0x38, the value quiet_brave_scoring.c
 * independently cites for type 19 — that match is the confirmation).
 */
static const uint8_t k_20e6_type_flags[AI_20E6_TYPE_COUNT] = {
  0x40, 0x1c, 0x40, 0x20, 0x3c, 0x64, 0x1c, 0x1c, 0x1c, 0x1c, 0x00, 0x18,
  0x00, 0xa2, 0x82, 0x82, 0x01, 0x81, 0x81, 0x38, 0x38, 0x38, 0x38
};

/*
 * bugs.md #655: live per-DOS-row cache of ColonizeUnitType.attack / .cap_bits
 * from the loaded catalog, refreshed once per dispatcher turn
 * (ai_euro_20e6_refresh_type_cache, called from ai_euro_dispatcher_turn — the
 * same per-nation-turn boundary that zeroes the other 20e6 file-local
 * latches). Indexed by DOS row (units_type_dos_code == ColonizeUnitKind),
 * which is what every ai_euro_20e6_dos_type() caller already produces.
 * `ai_euro_s_20e6_type_cache_valid` bit *i* means row i had a matching catalog entry
 * this refresh; unset rows fall back to the NAMES.TXT-default tables above
 * (hand-built test pools with no real @UNIT load never set any bit).
 */
static uint8_t s_20e6_type_combat_live[AI_20E6_TYPE_COUNT];
static uint8_t s_20e6_type_flags_live[AI_20E6_TYPE_COUNT];
uint32_t ai_euro_s_20e6_type_cache_valid;

void ai_euro_20e6_refresh_type_cache(const ColonizeUnitPool* units) {
  ai_euro_s_20e6_type_cache_valid = 0;
  if (!units) {
    return;
  }
  const int n = units->type_count < COLONIZE_UNIT_TYPES_MAX ? units->type_count : COLONIZE_UNIT_TYPES_MAX;
  for (int i = 0; i < n; ++i) {
    const ColonizeUnitType* t = &units->types[i];
    /* Only a units_load_types-stamped entry (kind_plus1 > 0) actually
     * parsed a real @UNIT column 12 bit-string / column 4 attack value;
     * a hand-built test pool (ai_fixture-style, kind_plus1 left 0, cap_bits
     * left at its zero-init) resolves a DOS row through the name fallback
     * (units_type_dos_code -> units_name_kind) but carries no real
     * cap_bits/attack data, so it must not overwrite the NAMES.TXT-default
     * fallback table below. */
    if (t->kind_plus1 <= 0) {
      continue;
    }
    const int row = units_type_dos_code(t);
    if (row < 0 || row >= AI_20E6_TYPE_COUNT) {
      continue;
    }
    s_20e6_type_combat_live[row] = (uint8_t)t->attack;
    s_20e6_type_flags_live[row] = t->cap_bits;
    ai_euro_s_20e6_type_cache_valid |= (1u << (unsigned)row);
  }
}
/*
 * DS:0x2f79 terrain record +3 (terrain_yields.md "DS:0x2f76 terrain-class
 * record", decoded 2026-08-21 from 20 dump instances) — colony-site
 * desirability per neighbour tile, added by the explorer far-probe ring.
 */
static const uint8_t k_20e6_terr_site_byte[32] = {
  2, 2, 4, 4, 4, 4, 2, 2, 3, 1, 3, 3, 3, 3, 1, 1, 3, 1, 3, 3, 3, 3, 1, 1, 0, 3, 0, 2, 2, 0, 0, 0
};
/*
 * DS:0xc8 / 0xde — 20-entry radius-2 ring (FUN_15eb_04c0 walks it as the
 * fort-scaled colony work radius: 5×5 minus centre minus 4 corners). First
 * 8 entries are the dir8 table (DS:0xb4/0xbe); the outer 12 are in
 * clockwise-from-north order. Only ever consumed via a uniform RNG slot or
 * a full-ring sum here, so intra-ring order does not affect results.
 */
const int8_t ai_euro_k_20e6_ring20_dx[20] = {0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 1, 0, -1, -2, -2, -2, -1};
const int8_t ai_euro_k_20e6_ring20_dy[20] = {-1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -1, 0, 1, 2, 2, 2, 1, 0, -1, -2};

/* ===== FUN_5952_035e colony-tick tile improvement (raw 94402-94551) ===== */

/*
 * DS:0x2f76 terrain record +0x3 ("colony-site desirability per tile",
 * docs/terrain_yields.md) is `k_20e6_terr_site_byte` above — FUN_5952_035e
 * raw 94417 reads the same column (`*(byte *)(class * 0x10 + 0x2f79)`) as the
 * base improvement value of a candidate work plot.
 */
/*
 * DS:0x97b2 (`-0x684e`) — the 14-byte special-resource desirability column.
 * Raw 120909 loads it at start-up from the same NAMES.TXT section the
 * @RESOURCE names come from (`FUN_2a1f_088a` per row, 0xe rows), so it is a
 * catalog column, not a compiled constant: read it back out of @RESOURCE
 * field 1 (data_vs_hardcoded.md Part D — catalog miss = 0, never a typed
 * fallback).
 */
static int ai_euro_5952_resource_site_byte(const ColonizeMsgCatalog* names, int resource) {
  char buf[32];
  if (!names || resource < 0 || resource > 13) {
    return 0;
  }
  if (!assets_msg_row_field(names, "RESOURCE", resource, 1, buf, sizeof(buf))) {
    return 0;
  }
  return atoi(buf);
}

/* `layer2 & 0x0a` (FUN_281f_0754): road OR settlement on the tile. */
static int ai_euro_5952_tile_road_or_settlement(const ColonizeWorldMap* map, int x, int y) {
  return (map_tile_has_road(map, x, y) || map_tile_has_city(map, x, y)) ? 0x0a : 0;
}

/* DOS `colony+0x70[plot]` -> `FUN_281f_0c0e` (FUN_15eb_0e18): the field job of
 * the colonist working this plot, or -1 when the plot is unworked. */
static int ai_euro_5952_plot_job(const ColonizeColony* col, int plot) {
  for (int i = 0; i < col->colonist_count && i < COLONIZE_COLONY_POP_MAX; ++i) {
    if (!col->colonists[i].active) {
      continue;
    }
    if (colonies_colonist_tile(col, i) == plot) {
      return col->colonists[i].field_job;
    }
  }
  return -1;
}

/*
 * DOS-LITERAL FUN_5952_035e raw 94402-94551 (clean body
 * original_sources_annotated/ai/colony_tick_5952_035e.md:919-1027) — the AI's
 * ONLY tile-improvement arm. It runs inside the colony tick, not on a unit:
 * DOS picks the best work plot, spawns a *phantom* colonist on it
 * (`FUN_291f_0a20` = `FUN_478c_002c`, type DS:0x524e), stamps `+0x315a = 99`
 * so the very next work tick completes, calls the real Pioneer body
 * (`FUN_291f_01c2` = `FUN_479b_01a6` clear/plow, or `FUN_291f_0216` =
 * `FUN_479b_0526` road), then undoes the phantom (`FUN_291f_0a06` =
 * `FUN_478c_00d0`). On success the colony pays 20 tools and `+0x8c` resets.
 *
 * Entry gate, raw 94402: `(colony+0x1b & 0x80) && local_16 && turn % 7 != 0`.
 * `local_16` is seeded at raw 93978 as `colony+0xb6 > 0x13`, i.e. 20+ tools in
 * stock. The `turn % 7 == 0` alternative (raw 94389-94400, the inter-colony
 * road-connect helper FUN_5952_0000 at raw 93589+) and the 20-tool purchase
 * block at raw 94370-94388 are the two sibling arms, ported as
 * ai_euro_5952_tools_supply_and_connect (bugs.md #640/#641), which runs
 * immediately before this one.
 *
 * Scoring, per ring plot (raw 94406-94469):
 *   filters: FUN_281f_0302 interior, FUN_281f_0768 not ocean/sea-lane,
 *            FUN_15eb_23f2 blocked mask == 0 (colonies_plot_blocked_mask);
 *   base   : terrain +0x3 byte, replaced by the @RESOURCE byte on a special;
 *   if NOT (wants-clear && class 8..0x17):
 *       worked plot whose job's improvement is missing doubles the value
 *       (job < 4 -> looks for plowed 0x40, job >= 4 -> looks for road 0x0a);
 *       a plot that has BOTH road and plow is skipped outright;
 *     else (forest on a wants-clear colony): value doubles;
 *   tribal claim (the DS:0x8d9e 5x5 table, FUN_15eb_26e4): penalty
 *       `-(alarm - 4)`, doubled on a special resource, doubled again when the
 *       continent carries Indian settlements (DS:0x95f2 bit 0) and no wagon
 *       train is homed here — and the plot is skipped entirely unless the
 *       colony wants a clear; finally doubled when the purse is under 2000
 *       gold, halved otherwise. `value -= penalty`.
 *   best wins, `>=` (DOS seeds `local_16e = 0xffff` = -1 as a signed 16-bit).
 */
void ai_euro_5952_improve_best_plot(ColonizeTurnContext* ctx, ColonizeColony* col) {
  if (!ctx || !ctx->map || !ctx->colonies || !col || !col->active) {
    return;
  }
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return;
  }
  const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  if ((col->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) == 0) {
    return; /* raw 94402: colony+0x1b & 0x80 */
  }
  if (col->stock[COLONIZE_CARGO_TOOLS] <= 0x13) {
    return; /* raw 93978 local_16 */
  }
  if (turn % 7 == 0) {
    return; /* raw 94403: that turn belongs to the road-connect helper */
  }
  const ColonizeWorld w = (ColonizeWorld){
    .units = ctx->units,
    .colonies = ctx->colonies,
    .map = ctx->map,
    .col1 = ctx->col1_ok ? ctx->col1 : NULL,
    .col1_ok = (ctx->col1_ok && ctx->col1) != 0
  };
  const int wants_clear = (col->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR) != 0;
  const int colony_cid = map_continent_id_at(ctx->map, col->x, col->y);
  const int presence = ai_contact_continent_presence_4962(ctx, nation, colony_cid);
  const long purse = (long)(int32_t)europe_nation_gold(ctx->europe, ctx->col1, nation);
  const int ring = colonies_work_plot_count(ctx->colonies, col);
  int best = -1;      /* local_16e, signed */
  int best_plot = -1; /* local_34 */
  for (int ti = 0; ti < ring; ++ti) {
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = col->x + dx;
    const int ty = col->y + dy;
    if (!map_coords_inset(ctx->map, tx, ty)) {
      continue; /* FUN_281f_0302 */
    }
    const int cls = map_dos_terr_class_at(ctx->map, tx, ty);
    if (cls == 0x19 || cls == 0x1a) {
      continue; /* FUN_281f_0768 */
    }
    if (colonies_plot_blocked_mask(&w, col, ti) != 0) {
      continue;
    }
    const int res = map_resource_type_at(ctx->map, tx, ty);
    int value = (res != -1) ? ai_euro_5952_resource_site_byte(ctx->names, res)
                            : (int)k_20e6_terr_site_byte[cls & 31];
    const int forest = (cls >= 8 && cls <= 0x17);
    if (!wants_clear || !forest) {
      const int job = ai_euro_5952_plot_job(col, ti);
      if (job >= 0) {
        const int have = (job < 4) ? (map_tile_is_plowed(ctx->map, tx, ty) ? 0x40 : 0)
                                   : ai_euro_5952_tile_road_or_settlement(ctx->map, tx, ty);
        if (have == 0) {
          value <<= 1;
        }
      }
      if (ai_euro_5952_tile_road_or_settlement(ctx->map, tx, ty) != 0 &&
          map_tile_is_plowed(ctx->map, tx, ty)) {
        continue; /* raw 94438-94440: nothing left to do here */
      }
    } else {
      value <<= 1;
    }
    const int claim = colonies_indian_claim_tribe_from_w(&w, nation, col->x, col->y, tx, ty);
    if (claim >= 0 && ctx->col1_ok && ctx->col1 && ctx->col1->tribe &&
        claim < (int)ctx->col1->head.tribe_count) {
      const int ind = (int)ctx->col1->tribe[claim].nation_id - 4;
      const int alarm = (ind >= 0 && ind < (int)COLONIZE_COL1_INDIAN_COUNT)
                          ? (int)ctx->col1->indian[ind].alarm_by_player[nation]
                          : 0;
      int pen = -(alarm - 4);
      if (res != -1) {
        pen = (alarm - 4) * -2;
      }
      if ((presence & 1) != 0 && (col->colony_flags & COLONIZE_COLONY_FLAG_WAGON_TRAIN) == 0) {
        if (!wants_clear) {
          continue; /* raw 94454: goto LAB_5952_122c */
        }
        pen <<= 1;
      }
      if (purse < 2000) {
        pen <<= 1;
      } else {
        pen >>= 1;
      }
      value -= pen;
    }
    if (best <= value) { /* raw 94466: `if ((int)local_16e < (int)local_132)` */
      best = value;
      best_plot = ti;
    }
  }
  if (best_plot < 0) {
    return;
  }
  int dx = 0;
  int dy = 0;
  if (!colonies_field_tile_delta(best_plot, &dx, &dy)) {
    return;
  }
  const int tx = col->x + dx;
  const int ty = col->y + dy;
  /*
   * raw 94474-94489: the 8-neighbour veto. Any neighbour whose layer3 owner
   * is a EUROPEAN nation (< 4) that is not AI-controlled (DS:0x543f == 0, i.e.
   * the human) kills the improvement — but the plot's own owner being this
   * colony's nation un-kills it (raw 94488-94491, run AFTER the loop).
   */
  int allow = 1;
  for (int d = 0; d < 8; ++d) {
    /* DS:0xb4/0xbe dir8 = the first 8 slots of the ring20 table.
     * FUN_281f_06d2 = settlement owner else unit owner (ai_goals note). */
    const int owner = map_tile_tribe_or_presence(
      ctx->map, tx + ai_euro_k_20e6_ring20_dx[d], ty + ai_euro_k_20e6_ring20_dy[d]
    );
    if (owner >= 0 && owner < 4 && owner == ctx->human_nation) {
      allow = 0; /* DS:0x543f[owner] == 0, the human-controlled seat */
    }
  }
  if (map_tile_tribe_or_presence(ctx->map, tx, ty) == nation) {
    allow = 1; /* raw 94488-94491, after the loop */
  }
  if (!allow) {
    return;
  }
  /*
   * raw 94492-94504: the work-turns threshold. `DS:0x2f78[class] + 2`, or
   * `+ 4` when the plot is forest AND the colony carries the wants-clear bit
   * (that is also what makes this a CLEAR rather than a plow/road decision).
   */
  const int cls = map_dos_terr_class_at(ctx->map, tx, ty);
  const int clear_mode = ((cls >= 8 && cls <= 0x17) && wants_clear) ? 1 : 0;
  const int base = map_dos_terr_pioneer_threshold_byte(cls);
  const int threshold = base + (clear_mode ? 4 : 2);
  if ((int8_t)threshold > (int8_t)col->improve_timer) {
    return; /* raw 94505 */
  }
  /*
   * The phantom-colonist trick, raw 94506-94540. DOS spawns a throwaway unit
   * of type DS:0x524e on the plot, stamps `+0x315a = 99` (any threshold is
   * met, so the body finishes on its first tick), runs the real Pioneer body
   * and despawns it. The port needs one extra thing DOS does not: its work
   * bodies gate on `units_is_pioneer`, which wants tools > 0, so the phantom
   * is handed tools. Nothing survives the despawn either way.
   */
  const int ptype = ai_euro_5d04_linux_type_for(ctx->units, 2);
  if (ptype < 0) {
    return;
  }
  const int pid = units_spawn_allow_stack(ctx->units, ptype, tx, ty);
  ColonizeUnit* ph = units_get(ctx->units, pid);
  if (!ph) {
    return;
  }
  ph->nation_id = nation;
  ph->tools = UNITS_EQUIP_TOOLS_STEP;
  ph->col1_counter16 = 99; /* +0x315a = 99 */
  const ColonizeWorld pw = (ColonizeWorld){
    .units = ctx->units, .colonies = ctx->colonies, .map = ctx->map
  };
  char err[64];
  int worked = 0;
  const int road_bits = ai_euro_5952_tile_road_or_settlement(ctx->map, tx, ty);
  const int plowed = map_tile_is_plowed(ctx->map, tx, ty) ? 0x40 : 0;
  int do_plow = 0;
  int do_road = 0;
  if (!clear_mode) {
    const int job = ai_euro_5952_plot_job(col, best_plot);
    if (job >= 0 && job < 4 && plowed == 0) {
      do_plow = 1; /* LAB_5952_1508 */
    } else if (job > 3 && road_bits == 0) {
      do_road = 1; /* LAB_5952_15b0 */
    } else if (cls < 2 || cls > 7) {
      do_road = (road_bits == 0);
    } else {
      do_plow = (plowed == 0);
    }
  } else {
    do_plow = 1; /* forest CLEAR shares FUN_479b_01a6 */
  }
  if (do_plow) {
    worked = units_pioneer_plow_w(&pw, pid, err, sizeof(err), NULL, NULL) ? 1 : 0;
  } else if (do_road) {
    worked = units_pioneer_road_w(&pw, pid, err, sizeof(err), NULL, NULL) ? 1 : 0;
  }
  units_despawn(ctx->units, pid); /* FUN_291f_0a06 */
  if (worked) {
    /* raw 94541-94549: colony+0xb6 -= min(stock, 20); colony+0x8c = 0. */
    int pay = col->stock[COLONIZE_CARGO_TOOLS];
    if (pay > 0x14) {
      pay = 0x14;
    }
    col->stock[COLONIZE_CARGO_TOOLS] -= pay;
    col->improve_timer = 0;
  }
}

int ai_euro_20e6_own_colonies_on(const ColonizeTurnContext* ctx, int nation, int cid);

/* FUN_281f_06be = FUN_137f_03e4 (raw 6840-6858): the owner of the SETTLEMENT
 * on a tile (layer2 bit 0x02 -> layer3 high nibble), -1 when there is none.
 * Unlike map_tile_tribe_or_presence (FUN_281f_06d2 = FUN_137f_0428) it does
 * NOT fall back to an occupying unit. */
static int ai_euro_5952_settlement_owner_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map_in_bounds(map, x, y) || !map_tile_has_city(map, x, y)) {
    return -1;
  }
  const int hi = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
  return hi == 0x0f ? -1 : hi;
}

/* FUN_281f_06e6 = FUN_137f_044a (raw 6877-6900): the owner of an IMPROVED
 * tile (`layer2 & 0x48` = plowed 0x40 or road 0x08) when that owner is a
 * DIFFERENT Euro nation (0..3) this nation is at PEACE with
 * (`nation[self].euro_relation[owner] & 0x40`, AI_DIPLO_PEACE); -1 otherwise. */
static int ai_euro_5952_improved_peer_owner_at(
  const ColonizeCol1Save* col1, const ColonizeWorldMap* map, int x, int y, int nation
) {
  if (!map_in_bounds(map, x, y)) {
    return -1;
  }
  if (!map_tile_is_plowed(map, x, y) && !map_tile_has_road(map, x, y)) {
    return -1;
  }
  const int hi = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
  if (hi < 0 || hi >= 4 || hi == nation || !col1) {
    return -1;
  }
  return (col1->nation[nation].euro_relation[hi] & AI_DIPLO_PEACE) != 0 ? hi : -1;
}

/*
 * DOS-LITERAL FUN_5952_0000 (raw 93589-93685) — the AI's inter-colony
 * ROAD-CONNECT helper, reached only from FUN_5952_035e's `turn % 7 == 0`
 * arm (raw 94389-94400) via thunk_FUN_2a1f_05d8. bugs.md #641.
 *
 *   iVar4 = FUN_281f_0722(x, y)                      // this colony's continent
 *   if (DS:0x94e6[nation*0x10 + iVar4] <= 1) return 0 // need 2+ own colonies here
 *   for (each other colony of the same nation, index != DS:0x8dc6):
 *     if (|ox-x| > 6 && |oy-y| > 6) continue          // raw 93612-93621, AND
 *     if (continent(ox,oy) != iVar4) continue
 *     if (FUN_281f_04d4(0, count-2) != 0) continue    // 1-in-(count-1) roll
 *     DS:0x1dd6 = -1; DS:0xa14e/0xa14c = (ox,oy); DS:0x1dd4 = 1; DS:0x1dd2 = 1
 *     walk FUN_2a1f_05f0 (= FUN_6662_00f2) one direction at a time from the
 *       colony toward the other colony, stopping on dir < 0 / dir == 8 / on
 *       arrival, and stopping with `found` when the stepped-onto tile has
 *       NO settlement (FUN_281f_06be < 0) AND no road/settlement bits
 *       (FUN_281f_0754 & 0x0a == 0) -- that tile is the gap in the link.
 *     if (!found) continue
 *     if (FUN_281f_06d2(gap) >= 0 && != nation) continue     // occupied
 *     if (FUN_281f_06e6(gap, nation) >= 0 && != nation) continue
 *     if (colony+0x8c < DS:0x2f78[class*0x10] + 2) return 0   // NOT continue
 *     spawn DS:0x524e phantom, +0x315a = 99, FUN_291f_0216 (= FUN_479b_0526
 *       road), FUN_291f_0a06 despawn, return 1
 *
 * Port notes: the walk needs a mover for units_next_goto_step_w (the port of
 * the same FUN_6662 pathfinder), so a walker phantom carries the virtual
 * position DOS keeps in local_18/local_1e; it is despawned BEFORE the
 * occupancy tests so it cannot be mistaken for the occupant DOS's
 * unit-less walk never sees. The road phantom is then spawned on the gap
 * tile exactly as the #612 improve arm does. The step loop carries a
 * width+height cap DOS does not need (its pathfinder always terminates).
 */
static int ai_euro_5952_road_connect_0000(ColonizeTurnContext* ctx, ColonizeColony* col) {
  if (!ctx || !ctx->map || !ctx->colonies || !ctx->units || !col || !col->active) {
    return 0;
  }
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return 0;
  }
  const int cid = map_continent_id_at(ctx->map, col->x, col->y); /* FUN_281f_0722 */
  /* raw 93601: `1 < DS:0x94e6[nation*0x10 + continent]` */
  const int own_here = ai_euro_20e6_own_colonies_on(ctx, nation, cid);
  if (own_here <= 1) {
    return 0;
  }
  const ColonizeWorld w = (ColonizeWorld){
    .units = ctx->units,
    .colonies = ctx->colonies,
    .map = ctx->map,
    .rng = ctx->rng,
    .col1 = ctx->col1_ok ? ctx->col1 : NULL,
    .col1_ok = (ctx->col1_ok && ctx->col1) != 0
  };
  const int step_cap = (int)ctx->map->width + (int)ctx->map->height;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    ColonizeColony* other = &ctx->colonies->colonies[ci];
    if (!other->active || other->nation_id != nation || other == col) {
      continue; /* raw 93606: same nation, DS:0x8dc6 != index */
    }
    const int ox = other->x;
    const int oy = other->y;
    /* raw 93610-93621 — DOS only checks y when |dx| > 6, so the skip needs
     * BOTH axes beyond 6. Transcribed as written. */
    if (abs(ox - col->x) > 6 && abs(oy - col->y) > 6) {
      continue;
    }
    if (map_continent_id_at(ctx->map, ox, oy) != cid) {
      continue; /* raw 93623 */
    }
    if (!ctx->rng || dos_rng_range(ctx->rng, 0, own_here - 2) != 0) {
      continue; /* raw 93625 FUN_281f_04d4(0, count - 2) */
    }
    /* The virtual walk, raw 93632-93652. */
    const int wtype = ai_euro_5d04_linux_type_for(ctx->units, 2);
    if (wtype < 0) {
      return 0;
    }
    const int wid = units_spawn_allow_stack(ctx->units, wtype, col->x, col->y);
    ColonizeUnit* walker = units_get(ctx->units, wid);
    if (!walker) {
      return 0;
    }
    walker->nation_id = nation;
    int cx = col->x;
    int cy = col->y;
    int found = 0;
    if (units_set_goto_w(&w, wid, ox, oy)) {
      for (int steps = 0; steps < step_cap; ++steps) {
        int nx = 0;
        int ny = 0;
        if (!units_next_goto_step_w(&w, wid, &nx, &ny)) {
          break; /* dir < 0 or dir == 8 */
        }
        const int px = cx;
        const int py = cy;
        cx = nx;
        cy = ny;
        walker->x = nx;
        walker->y = ny;
        units_occupancy_notify_moved(ctx->units, px, py, nx, ny);
        units_note_goto_step(wid, nx - px, ny - py);
        if (cx == ox && cy == oy) {
          break; /* raw 93639 */
        }
        if (ai_euro_5952_settlement_owner_at(ctx->map, cx, cy) < 0 &&
            (ai_euro_5952_tile_road_or_settlement(ctx->map, cx, cy) & 0x0a) == 0) {
          found = 1; /* raw 93641-93645: bVar3 = false */
          break;
        }
      }
    }
    units_despawn(ctx->units, wid);
    if (!found) {
      continue;
    }
    /* raw 93655-93661: the gap tile must carry no occupant and no at-peace
     * rival's improvement. */
    const int occ = map_tile_tribe_or_presence(ctx->map, cx, cy); /* FUN_281f_06d2 */
    if (occ >= 0 && occ != nation) {
      continue;
    }
    const int peer = ai_euro_5952_improved_peer_owner_at(
      ctx->col1_ok ? ctx->col1 : NULL, ctx->map, cx, cy, nation
    ); /* FUN_281f_06e6 */
    if (peer >= 0 && peer != nation) {
      continue;
    }
    /* raw 93663-93666: DOS *returns 0* here, it does not try another colony. */
    const int cls = map_dos_terr_class_at(ctx->map, cx, cy);
    const int threshold = map_dos_terr_pioneer_threshold_byte(cls) + 2;
    if ((int8_t)col->improve_timer < (int8_t)threshold) {
      return 0;
    }
    /* raw 93667-93671: the road phantom. */
    const int pid = units_spawn_allow_stack(ctx->units, wtype, cx, cy);
    ColonizeUnit* ph = units_get(ctx->units, pid);
    if (!ph) {
      return 0;
    }
    ph->nation_id = nation;
    ph->tools = UNITS_EQUIP_TOOLS_STEP; /* units_is_pioneer gate, see #612 */
    ph->col1_counter16 = 99;            /* +0x315a = 99 */
    const ColonizeWorld pw = (ColonizeWorld){
      .units = ctx->units, .colonies = ctx->colonies, .map = ctx->map
    };
    char err[64];
    const int built = units_pioneer_road_w(&pw, pid, err, sizeof(err), NULL, NULL) ? 1 : 0;
    units_despawn(ctx->units, pid); /* FUN_291f_0a06 */
    if (built) {
      return 1;
    }
    return 0;
  }
  return 0;
}

/*
 * DOS-LITERAL FUN_5952_035e raw 94370-94400 — the two arms that sit between
 * the tick's civic pass and the #612 tile-improvement arm. bugs.md #640/#641.
 *
 * raw 94370-94388, the 20-tool purchase (#640):
 *   if (((colony+0x1b & 0x80) || turn % 10 == 0) && local_16 == 0) {
 *     uVar12 = DS:0x84ca[nation*0x10];        // = DS:0x84bc[nation*0x10 + 0x0e]
 *     cost   = uVar12 * 0x14;                 // 20 tools
 *     if (32-bit gold at *0x84fc+0x2a >= cost) {
 *       gold -= cost; FUN_291f_0c14(); colony+0xb6 += 0x14; local_16 = 1;
 *     }
 *   }
 * DS:0x84ca is NOT a separate table: 0x84ca == 0x84bc + 0x0e and the read
 * uses the same `nation*0x10 + cargo` index as the SELL price row already
 * ported as AiEuro5952Want.sell_price (see the DS:0x84b4 note above and
 * docs/port_plan.md:976) — cargo 0x0e is Tools. Every writer of that row
 * stores `euro_price - 1` clamped at 0, so the AI pays the sell price, one
 * below the Europe ask. No byte dump was needed and no constant is invented.
 *
 * raw 94389-94400, the road-connect arm (#641):
 *   if (local_16 && turn % 7 == 0 && FUN_5952_0000()) {
 *     colony+0xb6 -= min(colony+0xb6, 0x14); colony+0x8c = 0;
 *   }
 * Note it is NOT gated on `colony+0x1b & 0x80` the way the improve arm is.
 */
void ai_euro_5952_tools_supply_and_connect(
  ColonizeTurnContext* ctx, ColonizeColony* col
) {
  if (!ctx || !col || !col->active) {
    return;
  }
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return;
  }
  const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  int have_tools = col->stock[COLONIZE_CARGO_TOOLS] > 0x13; /* local_16, raw 93978 */
  const int wants_work = (col->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) != 0;
  if ((wants_work || turn % 10 == 0) && !have_tools && ctx->col1_ok && ctx->col1) {
    /*
     * DS:0x84bc[nation*0x10 + 0x0e] — every writer of that row stores
     * `euro_price - 1` clamped at 0 (FUN_38fd_0040, europe.c's dump-sell
     * note). A game that never came from a DOS save leaves the AI nations'
     * byte at 0, exactly as europe.c's Custom House substitution documents;
     * with no live Europe screen either there is no DS:0x84bc row to read at
     * all, so the arm does not run (it cannot price the purchase — it does
     * not price it at zero).
     */
    const uint8_t row = ctx->col1->nation[nation].trade.euro_price[COLONIZE_CARGO_TOOLS];
    const int price = row != 0 ? (int)row - 1
                               : europe_sell_price(ctx->europe, COLONIZE_CARGO_TOOLS);
    const int have_market = (row != 0) || (ctx->europe != NULL);
    const long cost = (long)price * 0x14;
    const long gold = (long)(int32_t)europe_nation_gold(ctx->europe, ctx->col1, nation);
    if (have_market && gold >= 0 && gold >= cost) {
      europe_nation_gold_add(ctx->europe, ctx->col1, nation, -cost);
      col->stock[COLONIZE_CARGO_TOOLS] += 0x14;
      have_tools = 1;
    }
  }
  if (have_tools && turn % 7 == 0 && ai_euro_5952_road_connect_0000(ctx, col)) {
    int pay = col->stock[COLONIZE_CARGO_TOOLS];
    if (pay > 0x14) {
      pay = 0x14;
    }
    col->stock[COLONIZE_CARGO_TOOLS] -= pay;
    col->improve_timer = 0;
  }
}

uint8_t ai_euro_s_20e6_explorers[16];
/*
 * DOS unit+0x3154, the land-explorer branch (raw ~1600-1607): a per-unit
 * explore-fatigue counter, ++ (cap 0x7f) each explore-ring pass; the ring-hop
 * arm decrements it by 8. In DOS the same byte doubles as cargo_hold[0]
 * storage; land explorers never carry cargo, so a session-local array is the
 * honest home (not save-persisted — documented divergence).
 */
uint8_t ai_euro_s_20e6_explore_fatigue[COLONIZE_UNITS_MAX];
/*
 * DOS unit+0x3155 / +0x3156 — the explorer's 4-tile ring-hop wander latch
 * (raw 1600-1611 countdown / 2416-2458 hop pick; ported 2026-09-06).
 * +0x3156 latches a random ring20 slot (0xff = unset, re-rolled rng(1,0x14)−1
 * when needed); +0x3155 counts down the committed hop (max(dx,dy)*4, signed
 * char semantics kept). Like +0x3154 these bytes are cargo_hold storage in
 * DOS and land explorers never carry cargo, so session-local arrays are the
 * honest home (not save-persisted — documented divergence).
 * ai_euro_s_20e6_hop_slot stores slot+1 so the zero-initialised state reads "unset".
 */
int8_t ai_euro_s_20e6_hop_steps[COLONIZE_UNITS_MAX];
int16_t ai_euro_s_20e6_hop_slot[COLONIZE_UNITS_MAX];

/* DOS unit+0x3146 type index (NAMES.TXT @UNIT order) from a Linux unit. */
/*
 * DS:0x5239 = the @UNIT "cost" column (ColonizeUnitType.cost). The @UNIT
 * loader (raw 121115-121135) stores the NAMES.TXT numeric columns in file
 * order at 0x5232 icon, 0x5234 moves×3, 0x5236 attack, 0x5235 defense,
 * 0x5237 cargo, 0x5238 size, 0x5239 cost, 0x523a tools, 0x523b guns,
 * 0x523c hull, 0x523d bits. Cost is non-zero for every land type (Colonists
 * 1, Soldiers 2, Dragoons 3, Regulars 3, Cavalry 4, Artillery 6; ships
 * Caravel 4 .. Man-O-War 32). An earlier table here returned the guns
 * column (0x523b) instead, which is 0 for every land type and made the
 * LAB_52aa odds core score 0 against any 2+-defender land stack
 * (bugs.md #521).
 */
static int ai_euro_20e6_unit_col9(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const ColonizeUnitType* t = (units && u) ? units_type(units, u->type_index) : NULL;
  return t ? t->cost : 0;
}

int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const ColonizeUnitType* t = units ? units_type(units, u->type_index) : NULL;
  if (!t) {
    return -1;
  }
  /* units_type_dos_code IS this table (audit AE-4) — the kind ids are the
   * NAMES.TXT @UNIT row numbers, most-specific spelling first. */
  const int code = units_type_dos_code(t);
  if (code >= 0) {
    return code;
  }
  if (units->type_count >= 19 && u->type_index >= 0 && u->type_index < AI_20E6_TYPE_COUNT) {
    return u->type_index; /* NAMES-loaded pool: index == DOS index */
  }
  return -1;
}

int ai_euro_20e6_type_combat(int dos_type) {
  if (dos_type < 0 || dos_type >= AI_20E6_TYPE_COUNT) {
    return 0;
  }
  if ((ai_euro_s_20e6_type_cache_valid & (1u << (unsigned)dos_type)) != 0) {
    return (int)s_20e6_type_combat_live[dos_type];
  }
  return (int)k_20e6_type_combat[dos_type];
}

int ai_euro_20e6_type_flags(int dos_type) {
  if (dos_type < 0 || dos_type >= AI_20E6_TYPE_COUNT) {
    return 0;
  }
  if ((ai_euro_s_20e6_type_cache_valid & (1u << (unsigned)dos_type)) != 0) {
    return (int)s_20e6_type_flags_live[dos_type];
  }
  return (int)k_20e6_type_flags[dos_type];
}

/* FUN_1000_88cc / FUN_137f_0200 owner_nibble: layer3 high nibble, 0xf → −1. */
static int ai_euro_20e6_owner_nibble(const ColonizeWorldMap* map, int x, int y) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return -1;
  }
  const int n = (int)(map_get_layer3(map, x, y) >> 4) & 0xf;
  return n == 0xf ? -1 : n;
}

/*
 * FUN_1000_8c28 / FUN_15b3_0004 diplomacy byte, Euro or Indian counterpart.
 * 2026-09-08: the Indian-side synthesizer (MET assumed + WAR from
 * ai_diplo_indian_at_war) is gone — ai_diplo_read is dual-mode now and
 * nation[].relation_by_indian is live-maintained (contact 0x60, 4cc6_00f2
 * clear-both cooling, 153e smite or_both WAR, @WHACKINDIANS or_both bit 4),
 * so the raw byte IS the DOS read. Fixes the neighbour penalty consumer:
 * DOS `(rel & 0x60) != 0x20` skips peaceful-met tribes (byte 0x60); the
 * synthesizer's bare MET (0x20) wrongly penalised them.
 */
static int ai_euro_20e6_diplo(const ColonizeCol1Save* col1, int nation, int other) {
  if (!col1 || nation < 0 || nation > 3 || other < 0 || other > 11) {
    return 0;
  }
  return (int)ai_diplo_read(col1, nation, other);
}

/*
 * FUN_15eb_0142 nearest colony: shared with ai_goals (audit AE-39). Two port
 * divergences were fixed there against viceroy_unpacked.c:9380-9424: the
 * tie-break is `<=` (LAST tie wins, raw 9411) and DOS has no nation 0..3
 * filter (raw 9402 `param_3 < 0 || record_nation == param_3`).
 */
int ai_euro_20e6_nearest_colony(
  const ColonizeTurnContext* ctx, int x, int y, int nation, int cid, int* out_dist
) {
  return ai_goals_nearest_colony_15eb_0142(ctx->map, ctx->colonies, x, y, nation, cid, out_dist);
}

/* FUN_1000_8f74 / FUN_4cc6_0356 nearest village to (x,y). */
int ai_euro_20e6_nearest_village(const ColonizeTurnContext* ctx, int x, int y, int* out_dist) {
  int best = -1;
  int bd = 9999;
  if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if (t->nation_id < 4 || t->nation_id > 11 || t->x >= 200 || t->y >= 200) {
        continue;
      }
      const int d = map_dos_dist(x - (int)t->x, y - (int)t->y);
      if (d < bd) {
        bd = d;
        best = (int)ti;
      }
    }
  }
  if (out_dist) {
    *out_dist = bd;
  }
  return best;
}

/* FUN_1000_8886 / FUN_137f_0358 Euro settlement owner at tile, else −1. */
int ai_euro_20e6_colony_owner_at(const ColonizeTurnContext* ctx, int x, int y) {
  if (!ctx->colonies) {
    return -1;
  }
  const int cid = colonies_id_at(ctx->colonies, x, y);
  if (cid < 0) {
    return -1;
  }
  const ColonizeColony* c = colonies_get(ctx->colonies, cid);
  return (c && c->active) ? c->nation_id : -1;
}

/*
 * FUN_1000_88c2 / FUN_137f_0428 tile_tribe_or_presence: settlement owner
 * (Euro or Indian) if one sits here, else the occupying unit's owner, else −1.
 */
int ai_euro_20e6_tribe_or_presence(const ColonizeTurnContext* ctx, int x, int y) {
  int o = ai_euro_20e6_colony_owner_at(ctx, x, y);
  if (o >= 0) {
    return o;
  }
  o = ai_euro_village_nation_at(ctx->col1_ok ? ctx->col1 : NULL, x, y);
  if (o >= 0) {
    return o;
  }
  const int uid = units_id_at(ctx->units, x, y);
  if (uid >= 0) {
    const ColonizeUnit* ou = units_get_const(ctx->units, uid);
    return ou ? ou->nation_id : -1;
  }
  return -1;
}



/* Raw lines ~1006-1090: the locals every later arm reads. */
void ai_euro_20e6_prologue(ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation, Ai20e6Unit* s) {
  memset(s, 0, sizeof(*s));
  s->nation = nation;
  s->x = u->x;
  s->y = u->y;
  s->dos_type = ai_euro_20e6_dos_type(ctx->units, u);
  s->flags = ai_euro_20e6_type_flags(s->dos_type);
  s->combat = ai_euro_20e6_type_combat(s->dos_type);
  s->is_ship = (s->dos_type >= 0xd && s->dos_type <= 0x12) || units_is_sea(ctx->units, u->id);
  s->cid = map_tile_is_land(ctx->map, u->x, u->y) ? map_continent_id_at(ctx->map, u->x, u->y) : -1;
  s->home_colony = ai_euro_20e6_nearest_colony(ctx, u->x, u->y, nation, -1, &s->home_dist);
  if (s->home_colony < 0) {
    s->home_cid = -2;
    s->home_dist = 0; /* DS:0x8db8 untouched by a miss; DOS leaves the prior value — 0 is the
                         common case (unit freshly landed, nothing bound yet) */
  } else {
    const ColonizeColony* hc = colonies_get(ctx->colonies, s->home_colony);
    s->home_cid = hc ? map_continent_id_at(ctx->map, hc->x, hc->y) : -2;
  }
  (void)ai_euro_20e6_nearest_colony(ctx, u->x, u->y, -1, -1, &s->any_colony_dist);
  if (s->any_colony_dist == 9999) {
    s->any_colony_dist = 0;
  }
  s->stance = (s->cid < 0) ? 5 : ai_euro_continent_stance_at(nation, s->cid);
  s->village_idx = ai_euro_20e6_nearest_village(ctx, u->x, u->y, &s->village_dist);
  s->unit_river = map_tile_has_river(ctx->map, u->x, u->y) ? 1 : 0;
  s->unit_road = map_tile_has_road(ctx->map, u->x, u->y) ? 1 : 0;
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
    s->act_state = u->orders;        /* +0x314c */
    s->order_code = u->col1_ai_plan; /* +0x314b */
  }
  s->turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  s->year = (ctx->game_year && *ctx->game_year) ? (int)*ctx->game_year : 1492;
  s->woi = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.game_options.woi : 0;
}


/* Per-nation, per-continent DOS −0x6b1a / −0x6a8e / −0x6a0e reads. */
int ai_euro_20e6_own_colonies_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  int n = 0;
  if (ctx->colonies && cid >= 0) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == nation && map_continent_id_at(ctx->map, c->x, c->y) == cid) {
        n++;
      }
    }
  }
  return n;
}

static int ai_euro_20e6_combat_value_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  int sum = 0;
  if (cid < 0) {
    return 0;
  }
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->nation_id != nation || units_is_sea(ctx->units, o->id)) {
      continue;
    }
    if (map_continent_id_at(ctx->map, o->x, o->y) != cid) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, o->id, 1, NULL);
    if (sum > 255) {
      return 255; /* DOS byte table */
    }
  }
  return sum;
}

/* DS:0x95f2 continent_presence_flags bit 0x04: a foreign colony sits on cid. */
int ai_euro_10ec_land_units_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  int n = 0;
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->nation_id != nation || units_is_sea(ctx->units, o->id)) {
      continue;
    }
    if (map_continent_id_at(ctx->map, o->x, o->y) == cid) {
      n++;
    }
  }
  return n > 255 ? 255 : n; /* −0x6b5a byte table */
}

int ai_euro_10ec_war_worthy(const ColonizeTurnContext* ctx, int a, int b) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->map || !ctx->turn_number ||
      a < 0 || a > 3 || b < 0 || b > 3 || a == b) {
    return 0;
  }
  const ColonizeCol1Save* col1 = ctx->col1;
  const int focus = ctx->human_nation; /* DS:0x5398 */
  /* asm 5bfb:10f4: `CMP [0xa153],AL(=0x5398); JNZ -> return 0` — the AI only
   * weighs war/treaty-cancel while the human is the top-ranked nation
   * (FUN_5bfb_00f8 rank table). Skipped by the first pass, wired 2026-08-27. */
  if (ai_diplo_00f8_top_ranked_nation(col1) != focus) {
    return 0;
  }
  if ((int)*ctx->turn_number <= 0x27) {
    return 0;
  }
  if (col1->stuff.colony_pop_totals[a] <= 7 && col1->stuff.colony_pop_totals[b] <= 7) {
    return 0; /* −0x6bf4 */
  }
  if ((col1->nation[a].nation_flags & 0x04) || (col1->nation[b].nation_flags & 0x04)) {
    return 0; /* independent */
  }
  if (focus >= 0 && focus < 4) {
    /* For each of a/b: met-but-unpeaced with the focus nation only passes when the
     * focus nation is no stronger (−0x6be4 word, −0x6bf0 byte). DOS compares
     * b's word but a's −0x6bf0 byte in the second clause (verbatim). */
    const int fa = col1->nation[a].euro_relation[focus] & (AI_DIPLO_PEACE | AI_DIPLO_MET);
    if (fa == AI_DIPLO_MET &&
        !(col1->stuff.land_combat_strength[focus] <= col1->stuff.land_combat_strength[a] &&
          col1->stuff.census_pop_proxy[focus] <= col1->stuff.census_pop_proxy[a])) {
      return 0;
    }
    const int fb = col1->nation[b].euro_relation[focus] & (AI_DIPLO_PEACE | AI_DIPLO_MET);
    if (fb == AI_DIPLO_MET &&
        !(col1->stuff.land_combat_strength[focus] <= col1->stuff.land_combat_strength[b] &&
          col1->stuff.census_pop_proxy[focus] <= col1->stuff.census_pop_proxy[a])) {
      return 0;
    }
  }
  int rivals = 0;
  for (int n = 0; n < 4; ++n) {
    if (n != a && n != b &&
        (col1->nation[a].euro_relation[n] & (AI_DIPLO_PEACE | AI_DIPLO_MET)) == AI_DIPLO_MET) {
      rivals++;
    }
  }
  if ((col1->nation[a].euro_relation[b] & (AI_DIPLO_PEACE | AI_DIPLO_MET)) != AI_DIPLO_MET) {
    rivals++;
  }
  int local_4 = 0; /* any shared continent */
  int local_8 = 0; /* a's defense value where both present */
  int local_6 = 1; /* b's (units + defense)/2 + 1 */
  for (int cid = 1; cid < 0xf; ++cid) {
    const int a_col = ai_euro_20e6_own_colonies_on(ctx, a, cid);
    const int a_units = ai_euro_10ec_land_units_on(ctx, a, cid);
    const int a_def = ai_euro_20e6_combat_value_on(ctx, a, cid);
    const int b_def = ai_euro_20e6_combat_value_on(ctx, b, cid);
    if (a_col != 0 && (uint8_t)((a_units >> 1) + (a_def >> 1)) < b_def) {
      return 0; /* a is dwarfed where it has colonies */
    }
    const int b_units = ai_euro_10ec_land_units_on(ctx, b, cid);
    if (a_def != 0 && b_units != 0) {
      local_8 += a_def;
      local_6 += (b_units + b_def) >> 1;
      local_4 = 1;
    }
  }
  /* −0x6a9a[a*3] — the leader's belligerence column, resolved 2026-09-06d
   * (NAMES.TXT @LEADERNAME field 1; see ai_diplo_leader_trait). */
  const int belligerence = ai_diplo_leader_trait(ctx, a, 0);
  if ((rivals - belligerence) + 4 <= (local_8 << 2) / local_6 || local_4 == 0) {
    return 1;
  }
  return 0;
}

int ai_euro_20e6_foreign_colony_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  if (!ctx->colonies || cid < 0) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (c->active && c->nation_id != nation && map_continent_id_at(ctx->map, c->x, c->y) == cid) {
      return 1;
    }
  }
  return 0;
}

/*
 * DOS local_12 = −0x6168[cid]*8 + unit+0x3154 (explore fatigue). The
 * −0x6168 table is the 0a60 per-nation-turn max-tracker (largest foreign
 * colony pop vs capped ≤4 rival land units — written for real in
 * ai_euro_refresh_continent_stance, read back here at DOS position).
 */
static int ai_euro_20e6_local12(const ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation, int cid) {
  (void)ctx;
  const int fat = (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) ? (int)ai_euro_s_20e6_explore_fatigue[u->id] : 0;
  return ai_euro_rival_strength_at(nation, cid) * 8 + fat;
}

/*
 * DS:0x9650 — per-nation stats pass (decomp ~78316): number of continents
 * with continent_tally_a > 7 the nation has no colonies on ("open frontier
 * count"). Recomputed per call.
 */
int ai_euro_20e6_open_continents(const ColonizeTurnContext* ctx, int nation) {
  if (!ctx->col1_ok || !ctx->col1) {
    return 0;
  }
  int n = 0;
  for (int cid = 0; cid < 16; ++cid) {
    if ((int)ctx->col1->post_map.continent_tally_a[cid] > 7 &&
        ai_euro_20e6_own_colonies_on(ctx, nation, cid) == 0) {
      n++;
    }
  }
  return n;
}

/*
 * FUN_281f_074a & 0xf — the seen-plane low nibble = map-gen colony-site AI
 * score (save_format_map.md "seen" row; nawagers). DOS-imported maps carry it;
 * Linux map_gen writes no nibble, so when the whole plane carries none the
 * old per-nation unseen→4 stand-in stays (an all-zero nibble field would
 * otherwise disable the explore ring on generated maps).
 */
static int ai_euro_20e6_site_nibble(const ColonizeTurnContext* ctx, int x, int y, int nation) {
  /*
   * Was a byte-for-byte copy of ai_goals.c's probe, including its own
   * file-static "does this plane carry nibbles" memo — which nothing ever
   * invalidated, so a new game or a Load that reused the same `seen`
   * allocation kept the previous world's verdict and flipped the whole extras
   * term between the real nibble and the unseen→4 stand-in. Single cache now,
   * cleared by ai_goals_reset. Smell audit 2026-09-10 D8.
   */
  return ai_goals_site_nibble(ctx ? ctx->map : NULL, x, y, nation);
}

/*
 * FUN_1000_8aac = FUN_281f_08bc → FUN_1427_0d38 stack-query dispatcher.
 * Full case table re-decoded 2026-09-06 byte-exact from the CS:0xd78 jump
 * table (segment 1427 file base 0xF670 in viceroy_unpacked_2, entries
 * 0d96/0ef8/0db9/0db0/0dbe/0de0/0de6/0ef8×3/0e16/0e46/0e8c/0e94/0ebc —
 * see move_scoring_20e6_full.md "2026-09-06b"). All cases walk the unit's
 * tile stack (FUN_1427_0002 head / 004a next) accumulating DI:
 *   0    Σ @UNIT col9 (0x5239) over the stack
 *   2    TOTAL stack unit count (bare `inc di` per member — the earlier
 *        "case 2 = # military types" reading was case 4's body)
 *   3    # Pioneers (type 2) — the "# Pioneers" note was right all along
 *   4    # military land types {1,4,6,7,8,9}
 *   5    # Scouts (type 5)
 *   6    # {Soldier, Dragoon} + # veteran-professioned (0x315b == 0x15)
 *        others + # types {6..9} (a veteran-professioned type 6..9 counts
 *        twice — DOS's mobilizable-military count)
 *   a    # non-ship units with @UNIT combat col (0x5236) > 1
 *   b    Σ FUN_157e_004a(u,1) where ship-ness matches the tile
 *   c    # Artillery (type 0xb)
 *   d    Σ ship hold capacity (0x5237) over ships in stack
 *   e    max hold capacity over ships with +0x3148 bit7 clear
 *   1/7/8/9  return 0
 * Case 2 helper — used by the LAB_52aa odds divisor and the labor loop
 * (both call sites ndisasm-confirmed `PUSH 0x2`, viceroy_overlays.asm
 * 135593 / 139050+). Aboard units are modelled as ship cargo in Linux, so
 * they are excluded here and counted by the cargo-side queries instead.
 */
static int ai_euro_20e6_stack_count(const ColonizeTurnContext* ctx, int x, int y) {
  return units_count_at(ctx->units, x, y);
}

/*
 * Case 0xb: Σ combat value (FUN_157e_004a mode 1 — combat_unit_base_x8 mode 1
 * here, the same read −0x6a8e sums) over stack units whose ship-ness matches
 * the tile (land tile → land units).
 */
static int ai_euro_20e6_stack_combat_0b(ColonizeTurnContext* ctx, int x, int y) {
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  const int water = map_tile_is_water(ctx->map, x, y);
  int sum = 0;
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->aboard_ship_id >= 0 || o->x != x || o->y != y) {
      continue;
    }
    if ((units_is_sea(ctx->units, o->id) ? 1 : 0) != (water ? 1 : 0)) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, o->id, 1, NULL);
  }
  return sum;
}

/* FUN_OVL14_L0000__0072d6 → FUN_521d_0906 probe, ≥0 = adjacent foreign claim. */
static int ai_euro_20e6_probe_adjacent(const ColonizeTurnContext* ctx, int x, int y, int nation) {
  int side = -1;
  return ai_goals_probe_adjacent_contact_claim_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, x, y, nation, 0, &side);
}

/*
 * iStack_6a — "this unit explores this call" (raw lines ~1095-1180). Order of
 * the clauses is DOS's own; later clauses override earlier ones.
 */
static void ai_euro_20e6_explorer_flag(ColonizeTurnContext* ctx, const ColonizeUnit* u, Ai20e6Unit* s) {
  const int t = s->dos_type;
  int ex = (t == 2 || t == 0) ? 1 : 0; /* Pioneers / Colonists */
  if (u->profession == UNITS_JOB_CONVERT) {           /* Indian Convert */
    ex = 0;
  }
  if (t == 1 || t == 4) { /* Soldiers / Dragoons */
    if (s->act_state == 0) {
      ex = 1;
    }
    if (s->act_state == AI_EURO_ACT_GOAL &&
        map_dos_dist(u->x - u->goto_x, u->y - u->goto_y) > 12) {
      ex = 1; /* goal (+0x314d/e) more than 12 tiles away */
    }
    if (ai_goals_colony_balance_flags_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, s->nation, s->cid) > 2) {
      ex = 1;
    }
    if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, s->nation) >= 0) {
      ex = 0;
    }
    if (u->profession == UNITS_JOB_SOLDIER) { /* Veteran Soldier */
      ex = 0;
    }
    if (ai_euro_20e6_own_colonies_on(ctx, s->nation, s->cid) == 0 &&
        ai_euro_20e6_combat_value_on(ctx, s->nation, s->cid) < 8) {
      ex = 1;
    }
    if (t == 4 && ai_euro_20e6_foreign_colony_on(ctx, s->nation, s->cid)) {
      ex = 0;
    }
  }
  if (t == 5) { /* Scouts */
    if (s->order_code == '2') {
      ex = 1;
    }
    if (s->stance == 0) {
      ex = 0;
    }
    if (ai_euro_20e6_own_colonies_on(ctx, s->nation, s->cid) == 0 && (s->turn % 15) == 0) {
      ex = 1;
    }
    if (s->home_dist > 12 && s->any_colony_dist > 2) {
      ex = 1;
    }
    if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, s->nation) >= 0 || s->year > 1650) {
      ex = 0;
    }
  }
  /* FUN_521d_0600 composite priority must be non-zero (iStack_14). */
  if (ex) {
    const int prio = ai_goals_composite_unit_priority_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, s->nation, u->x, u->y, t, u->profession, s->turn, colonies_count_for_nation(ctx->colonies, s->nation));
    if (prio == 0) {
      ex = 0;
    }
  }
  /* Colonist standing on an own colony that still wants colonists: stay. */
  if (t == 0 && s->home_dist == 0 && s->home_colony >= 0) {
    const ColonizeColony* hc = colonies_get(ctx->colonies, s->home_colony);
    if (hc && (hc->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS)) {
      ex = 0;
    }
  }
  /* −0x5ec4[continent] explorer cap: 2 colonists / 3 others per continent. */
  if (ex && s->cid >= 0 && s->cid < 16) {
    if (ai_euro_s_20e6_explorers[s->cid] < 0xff) {
      ai_euro_s_20e6_explorers[s->cid]++;
    }
    if ((3 - (t == 0 ? 1 : 0)) < (int)ai_euro_s_20e6_explorers[s->cid]) {
      ex = 0;
    }
  }
  if (s->woi) {
    ex = 0;
  }
  s->explorer = ex;
}

/*
 * LAB_521d_277a SCOUT/PATROL arm: stance 0 (no planner pressure on this
 * continent), not already tasked ('t'/'i'), land, Pioneer-or-combat unit
 * whose bound colony shares its continent → sit on the colony (orders 0x56)
 * or walk back to it (goto commit LAB_27f5). Returns 1 when it handled the
 * unit. Cite: move_scoring_land.md "0x8db8 identified".
 */
static int ai_euro_20e6_patrol_arm(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s) {
  const int tasked = (s->order_code == 't' || s->order_code == 'i');
  if (s->stance != 0 || tasked || s->is_ship ||
      !(s->dos_type == UNITS_KIND_PIONEER || s->combat > 1) ||
      s->home_cid != s->cid || s->home_colony < 0) {
    return 0;
  }
  if (s->home_dist == 0) {
    return 1; /* orders 0x56: stay put, re-evaluate next call */
  }
  const ColonizeColony* hc = colonies_get(ctx->colonies, s->home_colony);
  if (!hc) {
    return 0;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, hc->x, hc->y);
  return 1;
}

/*
 * FUN_521d_20e6 surplus-garrison recall arm (raw 85313-85337; the decompiler
 * emits the same body a second time at raw 90149-90177 — both end in
 * `goto LAB_521d_27f5`, the "commit the walk to the bound colony" tail).
 *
 * DOS: a non-ship unit (type outside 0x0d..0x12) whose DS:0x5236 combat byte
 * is > 1 and whose `+0x314a` origin is bound loads that home colony
 * (FUN_281f_09e6), and if
 *   colony +0x1b bit 0x04 is set (surplus defenders — see colony.h)
 *   && (colony +0x1e garrison_quota != 0 || DOS unit type != 4, a Dragoon)
 *   && the colony sits on this unit's own continent (FUN_281f_0722 ==
 *      uStack_38)
 * it CLEARS 0x04, decrements garrison_quota and walks the unit home.
 *
 * The single-shot clear is the whole mechanism: 0x04 survives
 * FUN_5952_035e's per-tick `+0x1b &= 7`, so nothing but its consumers takes
 * it down, and this arm therefore recalls exactly ONE surplus unit per colony
 * per tick before the flag goes quiet until the next 5952 recompute raises it
 * again. Ported 2026-09-09 with the bit-0x04 consumer audit (the third
 * consumer, FUN_5952_035e's join-colonist loop at raw 94247, belongs to an
 * unported pass — documented in colony.h).
 *
 * Returns 1 when it handled the unit.
 */
static int ai_euro_20e6_surplus_recall_arm(
  ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s
) {
  if (s->is_ship || s->combat <= 1 || !ctx->colonies || !ctx->map) {
    return 0;
  }
  const int home = ai_euro_20e6_origin_get(u); /* +0x314a, −1 unbound */
  if (home < 0 || home >= COLONIZE_COLONIES_MAX) {
    return 0;
  }
  ColonizeColony* hc = &ctx->colonies->colonies[home];
  if (!hc->active || hc->nation_id != s->nation) {
    return 0;
  }
  if ((hc->ai_flags & COLONIZE_COLONY_AI_MILITARY_SURPLUS) == 0) {
    return 0;
  }
  if (hc->garrison_quota == 0 && ai_euro_20e6_dos_type(ctx->units, u) == 4) {
    return 0; /* a Dragoon is only recalled while there is quota to spend */
  }
  if (map_continent_id_at(ctx->map, hc->x, hc->y) != s->cid) {
    return 0;
  }
  hc->ai_flags = (uint8_t)(hc->ai_flags & (uint8_t)~COLONIZE_COLONY_AI_MILITARY_SURPLUS);
  if (hc->garrison_quota != 0) {
    hc->garrison_quota--;
  }
  if (u->x == hc->x && u->y == hc->y) {
    return 1; /* already home — 20c6 has nothing to walk */
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, hc->x, hc->y);
  return 1;
}

/*
 * DOS 8d4a village attitude[nation] — UNPARKED 2026-09-06. The record's
 * +10+nation*2 int16 maps exactly onto ColonizeCol1Tribe.alarm[nation]
 * (= {uint8_t friction; uint8_t attacks;} at byte offset +10, low byte
 * friction — settlement_record_8d4a.md "Relationship to Linux", exact
 * offset match), save-backed and live. The 0x4c gates now read it for
 * real: word == 0 for the scout arm, word < 0x40 for the colonist arm.
 * What stays session-local is only the VISIT-INCREMENT writer
 * (FUN_465b_0000 attitude++ on each visit, unported): without it the
 * ==0/<0x40 gates would re-fire (and re-draw RNG) every turn when the
 * visit outcome does not set the scouted/learned latch, so the per-village
 * nation bits below stand in for that one writer.
 */
static int ai_euro_20e6_village_attitude(const ColonizeCol1Tribe* t, int nation) {
  return col1_tribe_attitude(t, nation);
}
uint8_t ai_euro_s_20e6_village_visited[AI_20E6_VILLAGE_MAX];

/*
 * FUN_521d_20e6 orders-0x4c village arms (raw ~1366-1371 scout, ~1682-1691
 * colonist). Both need the unit adjacent (dist 1) to the nearest village.
 * Gates:
 *   Scout (type 5): record +3 bit8 (tribe.state.scouted) clear, alarm
 *     quartile < 0x19 (quartile domain 0..3 — vacuously true, omitted),
 *     attitude[nation] == 0 (stand-in latch above).
 *   Colonist: profession-bearing non-combat non-Scout/Missionary type whose
 *     profession byte is 0x1c (Free/none) or 0x19 (Indentured Servant),
 *     record +3 bit2 (tribe.state.learned) clear, attitude < 0x40 (stand-in).
 * Outcome: DOS writes orders 0x4c + a 2a1f_059c dir (enter the village);
 * Linux resolves the entry through the same @ACTIONS outcome functions
 * (Speak With Chief / Live Among The Natives) — an AI unit stepping onto a
 * village tile is an attack in this port, so the peaceful entry runs in
 * place. Returns 1 when the act was consumed.
 */
static int ai_euro_20e6_village_arm(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s) {
  if (!ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || s->village_idx < 0 ||
      s->village_dist != 1 || s->is_ship || s->nation < 0 || s->nation > 3) {
    return 0;
  }
  if (s->village_idx >= AI_20E6_VILLAGE_MAX ||
      (ai_euro_s_20e6_village_visited[s->village_idx] & (1u << s->nation))) {
    return 0;
  }
  ColonizeCol1Tribe* t = &ctx->col1->tribe[s->village_idx];
  if (t->nation_id < 4 || t->nation_id > 11) {
    return 0;
  }
  /* Real 8d4a attitude[nation] gates (raw 1366 scout ==0, raw 1686 colonist
   * <0x40) — the record word is tribe.alarm[nation] {friction, attacks}. */
  const int att = ai_euro_20e6_village_attitude(t, s->nation);
  if (s->dos_type == UNITS_KIND_SCOUT && !t->state.scouted && att == 0) {
    if (ai_contact_ai_scout_visit_village(ctx, s->nation, s->village_idx, u->id)) {
      ai_euro_s_20e6_village_visited[s->village_idx] |= (uint8_t)(1u << s->nation);
      u->moves = 0;
      return 1;
    }
  }
  /*
   * FUN_4d56_4528 non-human switch caseD_3 (Missionary): incite the village
   * against the human, else establish a mission, else denounce a foreign
   * mission. Resolved from the adjacent tile like the two arms around it
   * (entering a village tile is an attack in this port). bugs.md #557.
   */
  if (s->dos_type == UNITS_KIND_MISSIONARY) {
    if (ai_contact_ai_missionary_village(ctx, s->nation, s->village_idx, u->id)) {
      ai_euro_s_20e6_village_visited[s->village_idx] |= (uint8_t)(1u << s->nation);
      u->moves = 0;
      return 1;
    }
  }
  if (!(s->combat > 1 || s->dos_type == UNITS_KIND_SCOUT ||
        s->dos_type == UNITS_KIND_MISSIONARY) &&
      (u->profession == UNITS_JOB_NONE || u->profession == UNITS_JOB_SERVANT) && att < 0x40 &&
      !t->state.learned && !units_is_sea(ctx->units, u->id)) {
    if (ai_contact_ai_live_among_village(ctx, s->nation, s->village_idx, u->id)) {
      ai_euro_s_20e6_village_visited[s->village_idx] |= (uint8_t)(1u << s->nation);
      u->moves = 0;
      return 1;
    }
  }
  return 0;
}

/*
 * FUN_521d_20e6 colonist labor loop (raw ~1617-1679): a type-0 Colonist that
 * is not exploring walks to (or joins) the min-score same-continent own
 * colony that wants colonists (+0x1b bit 0x10, i.e.
 * COLONIZE_COLONY_AI_NEEDS_COLONISTS; Indian Converts join regardless).
 * Wanted size = FUN_15eb_0484 clamped ≤16 (a constant 8 in real play — see
 * ai_euro_colony_ring_tier; the old "fortification capacity 8/12/32" reading
 * of this table was an invention, corrected 2026-09-09);
 * candidate only while stack-military + population < wanted + 2.
 * Score = dist>>1, ×need when under-filled, ×2 when full (min-pick,
 * verbatim DOS arithmetic). No candidate:
 *   standing on an own colony → become a Pioneer (tools = 20, DOS orders
 *   0x3d / type 2 / +0x3159 = 0x14);
 *   else (outside WoI) force the explore ring (DOS re-loops with
 *   iStack_6a = 1) — signalled via s->explorer.
 * Returns 0 = not handled, 1 = goto set (act continues), 2 = unit consumed
 * (join / convert — abort the act, the unit may be gone).
 */
static int ai_euro_20e6_labor_arm(ColonizeTurnContext* ctx, ColonizeUnit* u, Ai20e6Unit* s) {
  if (s->dos_type != UNITS_KIND_COLONIST || s->explorer || s->is_ship || !ctx->colonies) {
    return 0;
  }
  int best = -1;
  int best_score = 9999;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != s->nation) {
      continue;
    }
    if (map_continent_id_at(ctx->map, c->x, c->y) != s->cid) {
      continue;
    }
    if (!(c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) &&
        u->profession != UNITS_JOB_CONVERT) {
      continue; /* 27 = Indian Convert (DOS profession 0x1b) */
    }
    int wanted = ai_euro_colony_wanted_size(ctx->colonies, c); /* FUN_15eb_0484 */
    if (wanted > 0x10) {
      wanted = 0x10;
    }
    const int dist = map_dos_dist(u->x - c->x, u->y - c->y);
    int score = dist >> 1;
    const int need = wanted - (int)c->population;
    if (need > 0) {
      score = need * score;
    }
    if ((int)c->population >= wanted) {
      score <<= 1;
    }
    /* 8aac case 2 = TOTAL units standing on the colony tile (PUSH 0x2 at
     * viceroy_overlays.asm 135593; real 0d38 case-2 body counts every stack
     * member, not just military — corrected 2026-09-06). */
    const int stack = ai_euro_20e6_stack_count(ctx, c->x, c->y);
    if (stack + (int)c->population < wanted + 2 && score < best_score) {
      best_score = score;
      best = i;
    }
  }
  if (best >= 0) {
    const ColonizeColony* c = colonies_get(ctx->colonies, best);
    if (!c) {
      return 0;
    }
    if (u->x == c->x && u->y == c->y) {
      (void)colonies_admit_unit_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, best, u->id);
      return 2; /* FUN_1000_9b94 join; unit consumed either way (8b24) */
    }
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, c->x, c->y);
    return 1;
  }
  if (s->home_dist == 0 && s->home_colony >= 0) {
    /*
     * DOS-LITERAL FUN_521d_20e6 raw 89350-89357 — the labor-band fall-through
     * for a unit standing on its own colony with nothing left to staff:
     *   unit[+0x314b] = 0x3d; unit[+0x3146] = 2; unit[+0x3159] = 0x14;
     *   FUN_281f_0934(unit); goto LAB_521d_5a78;
     * i.e. the surplus colonist BECOMES a Pioneer (type 2) carrying exactly 20
     * tools — the tools write is unconditional, not a `< 20` floor. The port
     * wrote only the floor, so no AI colonist ever changed type and the 5952
     * equip arm / 5d04 Europe arm (both gated on `unit_type_counts[n][2] == 0`)
     * kept re-arming (bugs.md #614).
     */
    u->col1_ai_plan = 0x3d; /* +0x314b */
    {
      const int pioneer_type = ai_euro_5d04_linux_type_for(ctx->units, 2);
      if (pioneer_type >= 0) {
        u->type_index = pioneer_type; /* +0x3146 = 2 */
      }
    }
    u->tools = UNITS_EQUIP_TOOLS_STEP; /* +0x3159 = 0x14, unconditional */
    u->moves = 0;                      /* FUN_281f_0934 */
    return 2;
  }
  if (!s->woi) {
    s->explorer = 1; /* DOS: iStack_6a = 1, re-run the ring as an explorer */
  }
  return 0;
}

/*
 * FUN_521d_20e6 explore ring (LAB_521d_2912 → 2a59), structural port of the
 * DOS windowed best-tile scan. Was a thin radius-5 scan (2026-08-15); now the
 * raw scoring: explore-nibble ×4 base, colony pull −(d−9)² own / −(a2−d)²
 * foreign (a2 = 7 with own colonies on the continent, else 5; skip d<2, skip
 * d==2 own, −20 d==2 foreign), village penalty (pop + relation + 3)×2 scaled
 * by distance bands / capital / nation quirks / −0x6a8e discount, explorer
 * bonus (+50% when no own colony here, ×2 colonists, +local_12>>tier, +16
 * when an explore goal exists). Radius 3 (DOS local_12 rival-strength term
 * read as 0 — see block header). Coastal gate = FUN_15eb_00a2 any-neighbour-
 * ocean via map_tile_is_coastal. Skips LCR class 0x1b and tiles another own
 * unit is already committed to (unit+0x314c==7) within the radius-2 ring.
 */
static int ai_euro_land_explore_scan_target(
  ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation_id, int force_explorer, int* out_x, int* out_y
) {
  if (!ctx || !ctx->map || !u || !out_x || !out_y) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  ai_euro_20e6_explorer_flag(ctx, u, &s);
  if (force_explorer) {
    s.explorer = 1; /* labor-loop fall-through: DOS re-loops with iStack_6a=1 */
  }
  if (s.cid < 0) {
    return 0; /* not on a mapped landmass (e.g. still in Europe) */
  }
  int tier = 3;
  const int tally_a = (ctx->col1_ok && ctx->col1 && s.cid < 16)
                        ? (int)ctx->col1->post_map.continent_tally_a[s.cid]
                        : 0;
  if (tally_a < 9) {
    tier = 0;
  } else if (tally_a < 0x19) {
    tier = 1;
  } else if (tally_a < 0x31) {
    tier = 2;
  }
  /*
   * Raw ~1600-1607: explorer pass bumps unit+0x3154 (cap 0x7f), then
   * local_12 = −0x6168[cid]*8 + that counter. Both terms live now
   * (s_euro_rival_strength writer / ai_euro_s_20e6_explore_fatigue).
   */
  if (s.explorer && u->id >= 0 && u->id < COLONIZE_UNITS_MAX &&
      ai_euro_s_20e6_explore_fatigue[u->id] < 0x7f) {
    ai_euro_s_20e6_explore_fatigue[u->id]++;
  }
  const int local_12 = ai_euro_20e6_local12(ctx, u, nation_id, s.cid);
  int radius = 3;
  if (local_12 > 0x1f) {
    radius = 2;
  }
  if (local_12 > 0x3f) {
    radius = 1;
  }
  const int own_here = ai_euro_20e6_own_colonies_on(ctx, nation_id, s.cid);
  /* Raw 89078 `0116(nation, x, y, 6)` — the primary-table explore probe.
   * DOS hoists it above the do-loop; hoisting is unobservable because the only
   * other code-6 op in the loop (`001c`, wired at the LAB_2912 entry in
   * ai_euro_20e6_land_act) touches the *secondary* table.
   *
   * AK-51: this read is a constant 0 and every term it feeds below is inert.
   * Nothing in src/ ever upserts AI_GOAL_EXPLORE into the primary table, and
   * DOS's own code-6 writer (`0214`, raw 89278) is dead too — see the note at
   * ai_goals.h:24-31. Faithful dead arithmetic; keep it as DOS spells it. */
  const int explore_goal = ai_goals_max_primary_prio(nation_id, u->x, u->y, AI_GOAL_EXPLORE);
  int best = -999;
  int best_nib = 0;
  int bx = u->x;
  int by = u->y;
  for (int ty = u->y - radius; ty <= u->y + radius; ++ty) {
    for (int tx = u->x - radius; tx <= u->x + radius; ++tx) {
      if (tx < 0 || ty < 0 || tx >= ctx->map->width || ty >= ctx->map->height) {
        continue;
      }
      const int pres = ai_euro_20e6_tribe_or_presence(ctx, tx, ty);
      if (!(pres < 0 || pres == nation_id)) {
        continue;
      }
      if (!map_tile_is_land(ctx->map, tx, ty) || map_continent_id_at(ctx->map, tx, ty) != s.cid) {
        continue;
      }
      /* FUN_1000_893a & 0xf — real seen-plane site-score nibble (fallback
       * inside the helper for nibble-less generated maps). */
      int nib = ai_euro_20e6_site_nibble(ctx, tx, ty, nation_id);
      int score = nib * 4;
      if (map_dos_terr_class_at(ctx->map, tx, ty) == 0x1b) {
        continue; /* LCR */
      }
      const int coastal = map_tile_is_coastal(ctx->map, tx, ty) ? 1 : 0;
      if (!coastal) {
        nib = 0;
      } else {
        int cd = 9999;
        const int ncol = ai_euro_20e6_nearest_colony(ctx, tx, ty, -1, s.cid, &cd);
        if (ncol >= 0) {
          if (cd < 2) {
            continue;
          }
          const ColonizeColony* nc = colonies_get(ctx->colonies, ncol);
          if (nc && nc->nation_id == nation_id) {
            if (cd == 2) {
              continue;
            }
            if (cd < 9) {
              score += -(cd - 9) * (cd - 9);
            }
          } else {
            if (cd == 2) {
              score -= 0x14;
            }
            const int a2 = own_here ? 7 : 5;
            if (cd < a2) {
              score += -(a2 - cd) * (a2 - cd);
            }
          }
        }
        /* Another own unit already committed (act_state 7) to this ring? */
        int free_site = 1;
        for (int r = 0; r < 9 && free_site; ++r) {
          const int rx = (r < 8) ? tx + ai_euro_k_20e6_ring20_dx[r] : tx;
          const int ry = (r < 8) ? ty + ai_euro_k_20e6_ring20_dy[r] : ty;
          const int oid = units_id_at(ctx->units, rx, ry);
          const ColonizeUnit* ou = oid >= 0 ? units_get_const(ctx->units, oid) : NULL;
          if (ou && oid != u->id && ou->orders == UNITS_ORDER_BUILD_COLONY) {
            free_site = 0;
          }
        }
        if (!free_site) {
          continue;
        }
        /* Village proximity penalty (raw ~1395-1440). */
        int vd = 9999;
        const int vi = ai_euro_20e6_nearest_village(ctx, tx, ty, &vd);
        if (vi >= 0 && vd < 9999) {
          const ColonizeCol1Tribe* v = &ctx->col1->tribe[vi];
          const int vcid = map_continent_id_at(ctx->map, (int)v->x, (int)v->y);
          int d = vd;
          if (ai_euro_20e6_own_colonies_on(ctx, nation_id, vcid) == 0) {
            d += 1;
          }
          if (d < 6) {
            const int rel = ai_diplo_indian_alarm(ctx->col1, (int)v->nation_id, nation_id);
            const int quart = rel < 25 ? 0 : rel < 50 ? 1 : rel < 75 ? 2 : 3;
            int base = ((int)v->population + quart + 3) * 2;
            if (vcid != s.cid) {
              base >>= 1;
            }
            int pen = base >> 1;
            if (d < 5) {
              pen += base;
            }
            if (d < 4) {
              pen += base * 2;
            }
            if (d < 3) {
              pen += base * 4;
            }
            if (d < 2) {
              pen += base * 8;
            }
            if (v->state.capital) {
              pen <<= 1;
            }
            if (nation_id == 1) {
              pen >>= 1;
            }
            if (founding_fathers_nation_has(ctx->col1, nation_id, FF_POCAHONTAS)) {
              pen >>= 1;
            }
            if (nation_id == 2) {
              pen >>= 2;
            }
            if (local_12 > 0x28) {
              pen >>= 1;
            }
            pen -= ai_euro_20e6_combat_value_on(ctx, nation_id, vcid);
            if (pen < 0) {
              pen = 0;
            }
            score -= pen;
          }
        }
        if (s.explorer && nib > 3) {
          if (own_here == 0) {
            score += score >> 1;
          }
          if (s.dos_type == UNITS_KIND_COLONIST) {
            score <<= 1;
          }
          score += local_12 >> tier;
          if (explore_goal != 0) {
            score += 0x10;
          }
        }
      }
      if (score >= best) {
        best = score;
        best_nib = nib;
        bx = tx;
        by = ty;
      }
    }
  }
  if (best_nib <= 0) {
    return 0;
  }
  /*
   * DOS raw 89270-89280 / asm 002e10-002e72:
   *     if (best_nib > 0) {
   *       if (iStack_6a) { ...move to (bx,by), or act_state = 7 when here... }
   *       else 0214(nation, x + dx[dir], y + dy[dir], 6, 2);   // raw 89278
   *     }
   * **The `else` is unreachable.** `iStack_6a` is the explorer flag at
   * [BP-0x68]; the whole scan is entered only through
   * `CMP word [BP-0x68],0 / JNZ 0029b6` (asm 135058-135060) and nothing
   * between 0029b6 and 002e1a writes that slot — verified by scanning every
   * `[BP + -0x68]` reference across the 20e6 body (writes only in the
   * prologue flag block and at raw 89361, both outside the range). So the
   * second `CMP word [BP-0x68],0` at 002e1a (asm 135468, same four bytes
   * `83 7e 98 00`) can never take the JZ, and `0072f4`/`0214` at 002e4a is
   * compiler-preserved dead code — its only call site anywhere in the game
   * (grep of both decompiles + both asm listings: one hit each for
   * `thunk_FUN_2a1f_04c4` / `FUN_OVL14_L0000__0072f4`).
   * Consequence: goal code 6 is never written to either table in DOS, so
   * `explore_goal` above is a constant 0 in DOS too and the `+0x10` bonus at
   * the explorer arm never fires. NOT ported deliberately — wiring it would
   * manufacture goals DOS does not have and, via 0342's promote, feed the
   * primary table on the next nation turn.
   */
  /*
   * DOS-LITERAL FUN_521d_20e6 raw 89272-89275 (bugs.md #683): the best site is
   * the tile the unit already stands on, so 20e6 commits it instead of walking
   *     *(param_1 * 0x1c + 0x314c) = 7;  goto LAB_521d_5a78;
   * i.e. act_state 7 = UNITS_ORDER_BUILD_COLONY, the same byte the human Build
   * handler writes at raw 45657 right before FUN_291f_01fa. This is the ONLY
   * writer of act_state 7 in the AI, and it is what makes the two ring scans
   * that read it live: this function's own raw-89182 "another founder already
   * committed inside the ring" reject, and the human @TOONEARBUILD scan at raw
   * 45670. Returning 2 here rather than writing through the const unit keeps
   * the write at the caller, which owns `u`.
   */
  if (bx == u->x && by == u->y) {
    return 2;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * LAB_521d_52aa attack term — odds core byte-shaped from the asm
 * (viceroy_overlays.asm:139036+, 2026-08-27; the C there is register-garbage):
 *   base = FUN_1000_9c04(unit, x, y, 0, 0)          = FUN_5fef_1b0e probe mode:
 *          (atk_strength << 3) / (def_strength + 1)  (combat_land/naval_engage)
 *   a    = FUN_1000_8aac(foe, 0) + 1                 = Σ @UNIT col9 over the foe stack + 1
 *   d    = max(FUN_1000_8aac(foe, 2), 1)             = TOTAL foe stack size
 * (case 2 re-decoded byte-exact 2026-09-06: it counts every stack member;
 * the old "# military types" reading was case 4's body)
 *   odds = ((a / d) * base) / max(@UNIT col9[own type], 1)
 * (8aac = FUN_281f_08bc → FUN_1427_0d38, the stack query dispatcher, cases
 * decoded from its jump table; col9 = DS:0x5239 = the @UNIT cost column,
 * see ai_euro_20e6_unit_col9 — non-zero for every land type.) Then the transcribed
 * modifiers: ×3 own-colony tile, ×2 village, Artillery in the open → 0,
 * ×3 when flags&0x10 and stance==4, clamp 0..1000, <12 → −999 else +odds×4.
 * Still substituted: the Soldier/Dragoon
 * vs colony adjacent-Spanish-strength skip (8aac case 0xb) — not wired.
 */
static int ai_euro_20e6_attack_term(
  ColonizeTurnContext* ctx, const ColonizeUnit* u, const Ai20e6Unit* s, int nx, int ny, int foe_id, int* score
) {
  if (s->combat == 0) {
    return 0;
  }
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  /* base = 1b0e probe: (atk << 3) / (def + 1). No foe on the tile → a bare
   * settlement/empty tile: defence strength 0 (DOS spawns a temp defender there;
   * kept as the previous stand-in). */
  int base;
  if (foe_id >= 0) {
    ColonizeCombatEngageResult er;
    memset(&er, 0, sizeof(er));
    if (s->is_ship) {
      combat_naval_engage(&sctx, u->id, foe_id, &er);
    } else {
      combat_land_engage(&sctx, u->id, foe_id, &er);
    }
    base = (er.atk_strength << 3) / (er.def_strength + 1);
  } else {
    base = (combat_unit_base_x8(&sctx, u->id, 1, NULL) << 3) / 1;
  }
  /* 8aac(foe, 0) + 1 and max(8aac(foe, 2), 1) over the whole foe stack at
   * (nx, ny); case 2 = total stack size (byte-exact 0d38 decode). */
  int col9_sum = 0;
  int stack = 0;
  if (foe_id >= 0) {
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* o = &ctx->units->units[i];
      if (!o->active || o->aboard_ship_id >= 0 || o->x != nx || o->y != ny) {
        continue;
      }
      col9_sum += ai_euro_20e6_unit_col9(ctx->units, o);
      stack++;
    }
  }
  const int a = col9_sum + 1;
  const int d = stack < 1 ? 1 : stack;
  int odds = (a / d) * base;
  {
    const int own_c9 = ai_euro_20e6_unit_col9(ctx->units, u);
    odds /= own_c9 < 1 ? 1 : own_c9;
  }
  int settlement = 0;
  if (ai_euro_20e6_colony_owner_at(ctx, nx, ny) >= 0) {
    odds *= 3;
    settlement = 1;
  }
  if (ai_euro_village_nation_at(ctx->col1_ok ? ctx->col1 : NULL, nx, ny) >= 0) {
    odds <<= 1;
    settlement = 1;
  }
  if (s->dos_type == UNITS_KIND_ARTILLERY && !settlement) {
    odds = 0;
  }
  /* FUN_521d_20e6 raw 88913-88915: the ACTING unit's own nation nibble
   * (uVar11, raw 88403 = `*(byte *)(unit + 0x3147) & 0xf`) equals DS:0x53d2
   * (head.crown_nation_id) ∧ open tile (local_4 == 0) ∧ local_2e == 0
   * → halve. (The old comment's "raw 2855" was Ghidra switch garbage and the
   * crown id was compared against the literal 2.) */
  if (ctx->col1_ok && ctx->col1 &&
      (int)u->nation_id == (int)ctx->col1->head.crown_nation_id && !settlement &&
      s->home_dist == 0) {
    odds >>= 1;
  }
  if ((s->flags & 0x10) && s->stance == 4) {
    odds *= 3;
  }
  /*
   * Raw 2862-2882 (LAB_52aa tail): Soldier/Dragoon assaulting a Euro colony
   * tile — mass gate. def = 8aac(tile stack, 0xb) combat sum; own = Σ over
   * the 8 neighbours of the target of 8aac(neighbour stack, 0xb) for stacks
   * of the attacker's own nation (the decompile compares the owner nibble to
   * a clobbered constant 2; own-nation is the only coherent reading — noted
   * in move_scoring_20e6_full.md as "adjacent Spanish-owned units?").
   * own ≤ def → `goto LAB_521d_5183`.
   *
   * LAB_5183 is the direction loop's own increment, NOT an exit from the scan
   * (bugs.md #523, refuted 2026-09-18e): viceroy_overlays.asm
   * `LAB_OVL14_L0000__0054b0  CMP [BP+0xff28],AX / JL 0054b5 / JMP 005183`,
   * and `LAB_OVL14_L0000__005183` is `INC word ptr [BP-0x4e]` followed by
   * `LAB_..__005186  CMP [BP-0x4e],0x8 / JL 00518f` — the loop counter bump
   * and test, with 14 XREFs, i.e. every "this direction is unscoreable" arm
   * in the scorer jumps there. Returning 0 so the caller `continue`s to the
   * next direction is exactly DOS.
   */
  if ((s->dos_type == UNITS_KIND_SOLDIER || s->dos_type == UNITS_KIND_DRAGOON) &&
      ai_euro_20e6_colony_owner_at(ctx, nx, ny) >= 0) {
    const int def = ai_euro_20e6_stack_combat_0b(ctx, nx, ny);
    if (def != 0) {
      static const int mdx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int mdy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      int own_sum = 0;
      for (int n = 0; n < 8; ++n) {
        const int ax = nx + mdx[n];
        const int ay = ny + mdy[n];
        const int oid = units_id_at(ctx->units, ax, ay);
        const ColonizeUnit* ou = oid >= 0 ? units_get_const(ctx->units, oid) : NULL;
        if (ou && ou->nation_id == s->nation) {
          own_sum += ai_euro_20e6_stack_combat_0b(ctx, ax, ay);
        }
      }
      if (own_sum <= def) {
        return 0; /* caller treats the tile as unscoreable */
      }
    }
  }
  if (odds > 999 || odds < 0) {
    odds = 1000;
  }
  /*
   * LAB_OVL14_L0000__0054b5 (viceroy_overlays.asm 0x0054b5, decomp
   * viceroy_unpacked_2.c:85550-85563): the −999 penalty is gated on the mover
   * being a NON-ship (`CMP [BX+0x3146],0xd / JC` … `CMP [BX+0x3146],0x12 /
   * JBE`, i.e. type ∉ [0xd,0x12]); a ship with poor odds falls into the else
   * arm instead, where DOS clamps `odds < 1` up to 1 before the ×4 bonus.
   * Both were dropped by the first pass; restored verbatim.
   */
  const int dos_ship = (s->dos_type >= 0xd && s->dos_type <= 0x12);
  if (odds < 0xc && !dos_ship) {
    *score -= 999;
  } else {
    if (odds < 1) {
      odds = 1;
    }
    *score += odds * 4;
  }
  return 1;
}

/*
 * FUN_281f_06b4(x, y) == 1 for water: the open-sea body (water region 1).
 * Same region-0 relaxation as map_tile_is_open_sea_adjacent — synthetic /
 * test maps leave layer3 zero.
 */
static int ai_euro_20e6_open_sea(const ColonizeWorldMap* map, int x, int y) {
  if (!map_tile_is_water(map, x, y)) {
    return 0;
  }
  const int region = (int)(map_get_layer3(map, x, y) & 0x0fu);
  return region <= 1;
}

/*
 * LAB_521d_4d2e → 5183: the 8-direction wander scorer. Picks one adjacent
 * tile (or stay) exactly as DOS does when no arm above has committed a
 * destination. Ships reach it through the raw 90219 `goto LAB_4d2e` after the
 * 3558 / 4393 / 457e bands fall through; their arms are the iStack_34 branches
 * inside the same loop (water-only step, no settlement term, fort/artillery
 * term scaled by holds, west lean, unseen-water credit). Returns dir 0..7,
 * or 8 = stay.
 *
 * `out_attack` = DOS `local_ce` (raw 88874): the winning candidate was scored
 * through LAB_521d_52aa, i.e. the pick is an attack. `out_saw_foe` = DOS
 * `local_ea` (raw 88887): *any* candidate reached LAB_52aa with a non-zero
 * DS:0x5236 row — an attackable foreigner stood next to this unit, whatever
 * the odds came out as. Both are read by the LAB_4d2e tail (raw 88983-89040);
 * either may be NULL.
 */
int ai_euro_20e6_wander_step(
  ColonizeTurnContext* ctx, ColonizeUnit* u, Ai20e6Unit* s, int* out_attack, int* out_saw_foe
) {
  const int nation = s->nation;
  /* uVar14 — far-probe/fog enable: no adjacent claim (probe mode 1) or a
   * non-combat land unit. */
  int fog_enable = 0;
  if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, nation) < 0 || (!s->is_ship && s->combat == 0)) {
    fog_enable = 1;
  }
  /* raw 88622-88624: a ship in the War of Independence never idles (bVar20). */
  if (s->is_ship && s->woi) {
    fog_enable = 0;
  }
  /* iStack_90: the unit's own tile is ocean / high seas. */
  const int unit_on_water = map_tile_is_water(ctx->map, u->x, u->y) ? 1 : 0;
  /* +0x3150 occupied goods holds — the multiplier of the ship fort term. */
  int goods_holds = 0;
  if (s->is_ship) {
    const int nh = units_goods_hold_count(ctx->units, u->id);
    for (int h = 0; h < nh; ++h) {
      if (units_hold_amount(ctx->units, u->id, h) > 0) {
        goods_holds++;
      }
    }
  }
  /* unit+0x3148 bit4 wander_dest_chosen: peacetime distant-tile roll (raw
   * ~1960-1995) — kept as the latch only; the >7-tile random goto it
   * produces is a ship-only arm (type 0xd..0x12), not reached on land. */
  int best = -999;
  int best_dir = 8;
  int best_attack = 0;
  int saw_foe = 0; /* local_ea */
  const int last_dir = (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) ? ai_euro_s_euro_last_dir[u->id] : -1;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    if (!map_coords_inset(ctx->map, nx, ny)) {
      continue; /* FUN_1000_84f2 inset bounds */
    }
    const int terr = map_dos_terr_class_at(ctx->map, nx, ny);
    const int dest_water = (terr == 0x19 || terr == 0x1a);
    if (!s->is_ship) {
      if (dest_water) {
        continue; /* land unit: ocean / high seas */
      }
    } else if (!dest_water || !ai_euro_20e6_open_sea(ctx->map, nx, ny)) {
      /* raw 88679-88687: a ship steps only onto ocean / high seas in the open
       * sea body (FUN_281f_06b4 == 1); a land tile falls to the bare
       * FUN_281f_0696 probe with no score. */
      continue;
    }
    int owner = ai_euro_20e6_owner_nibble(ctx->map, nx, ny);
    const int here = units_id_at(ctx->units, nx, ny);
    const ColonizeUnit* hu = here >= 0 ? units_get_const(ctx->units, here) : NULL;
    if (s->is_ship && hu && owner < 0) {
      /* DOS stamps the owner nibble from the stack on entry; the port's water
       * tiles carry no claim, so the hull on the tile is the owner. */
      owner = hu->nation_id;
    }
    int score = 0;
    const int dest_river = map_tile_has_river(ctx->map, nx, ny) ? 1 : 0;
    const int dest_road = map_tile_has_road(ctx->map, nx, ny) ? 1 : 0;
    const int cardinal = (d & 1) == 0;
    if (s->dos_type == UNITS_KIND_SCOUT) { /* Scouts */
      score = dos_rng_range(ctx->rng, 1, 8);
      if (s->unit_river && dest_river && cardinal) {
        score += 2;
      } else if (s->unit_road && dest_road) {
        score += 1;
      } else {
        score -= map_dos_terr_cost_byte(terr) * 3;
      }
    } else if (!s->explorer) {
      if (u->col1_vis_mask == 0 && !unit_on_water) { /* unseen; raw 88709 also iStack_90 == 0 */
        if ((s->flags & 0x20) == 0 && (s->flags & 0x10) == 0) {
          score = dos_rng_range(ctx->rng, 1, 3);
          if (!s->woi) {
            score += map_dos_terr_found_score_byte(terr);
          } else {
            score -= map_dos_terr_found_score_byte(terr);
          }
        } else {
          score = dos_rng_range(ctx->rng, 1, 3);
          if ((s->unit_river && dest_river && cardinal) || (s->unit_road && dest_road)) {
            score += 1;
          } else {
            score -= map_dos_terr_cost_byte(terr);
          }
        }
      } else {
        score = dos_rng_range(ctx->rng, 1, 5);
        if (here < 0 || owner != nation) {
          score += map_dos_terr_found_score_byte(terr) << 2;
        }
      }
    } else {
      /* Raw ~2670: (rng(1,4) + (893a & 0xf)) >> 1 — real site nibble now. */
      const int nib = ai_euro_20e6_site_nibble(ctx, nx, ny, nation);
      score = (dos_rng_range(ctx->rng, 1, 4) + nib) >> 1;
    }
    if (terr == 0x1a) {
      score -= 0x10;
    }
    /*
     * Combat land unit stepping onto a settlement tile —
     * LAB_OVL14_L0000__00507a..0050fc (viceroy_overlays.asm 0x00507a, decomp
     * viceroy_unpacked_2.c:85338-85376):
     *   if (type ∉ [0xd,0x12] && DS:0x5236[type] > 1)
     *     if (FUN_1000_8886(dest) >= 0)              // Euro colony on the tile
     *       if ([BP-0xe] == [BP+0xff1c])             // TILE owner nibble == mover nation
     *         FUN_1000_8bd6([BP-0x60]);              // bind the MOVER's nearest own
     *                                                // colony (FUN_1000_8804(ux,uy,nation,-1),
     *                                                // stored in the prologue at 0x0021f2)
     *         flags = *(DS:0x8542)+0x1b: 0x40 → +10, 0x04 → +6, 0x10 → +3
     *       else score += 0x10;
     * The first pass read the destination tile's colony record and widened the
     * single owner gate to `owner == nation || col_owner == nation`; both are
     * back to the DOS form (s->home_colony = the same 8804 search).
     */
    if (!s->is_ship && s->combat > 1) {
      const int col_owner = ai_euro_20e6_colony_owner_at(ctx, nx, ny);
      if (col_owner >= 0) {
        if (owner == nation) {
          const ColonizeColony* c =
            s->home_colony >= 0 ? colonies_get(ctx->colonies, s->home_colony) : NULL;
          const int af = c ? (int)c->ai_flags : 0;
          if (af & COLONIZE_COLONY_AI_NEEDS_GARRISON) {
            score += 10;
          } else if (af & COLONIZE_COLONY_AI_MILITARY_SURPLUS) {
            score += 6;
          } else if (af & COLONIZE_COLONY_AI_NEEDS_COLONISTS) {
            score += 3;
          }
        } else {
          score += 0x10;
        }
      }
    }
    int attack = 0;
    const int pres = ai_euro_20e6_tribe_or_presence(ctx, nx, ny);
    if ((here < 0 && pres < 0) || owner == nation) {
      /* LAB_54f5: empty or own tile — fall through to facing/fog terms. */
    } else if (owner < 4) {
      /*
       * raw 88877-88880: `local_10 = FUN_281f_06dc(x, y)` is the layer3 owner
       * nibble with 0xf → −1, and the Euro arm is entered on `local_10 < 4`,
       * so a foe standing on UNCLAIMED land (owner −1) is scored here too.
       * DOS then reads `FUN_15b3_0004(nation, −1)` = the byte before the
       * nation's relation row (nation[n−1] +0x13b, a trade-table tail byte;
       * DS:0x883b for nation 0) — a stray read whose value is not knowable
       * statically. The port stands in with the relation to the occupant's
       * nation, which is the only reading under which the arm is coherent
       * (bugs.md #521, 2026-09-22: without it a foe on unclaimed land was
       * never scored and land units went passive).
       */
      const int partner = owner >= 0 ? owner : (hu ? hu->nation_id : -1);
      const int rel = ai_euro_20e6_diplo(ctx->col1, nation, partner);
      const int hu_type = hu ? ai_euro_20e6_dos_type(ctx->units, hu) : -1;
      /*
       * raw 88883-88885 (FUN_521d_20e6, LAB_521d_52aa entry gate):
       *   (*(byte*)0x5382 & 1) == 0 ||
       *   ((local_10 < 4 && *(char*)(local_10*0x34 + 0x543f) == '\0') || 3 < local_10)
       * During the War of Independence (DS:0x5382 bit0) an attack is scored
       * only against a human-controlled player slot (control byte 0) or a
       * tribe. The port previously spelled this `!s->woi || owner < 0`, which
       * rejected every claimed tile and left the REF unable to score an
       * assault on a defended rebel colony (bugs.md #521).
       */
      const int woi_ok =
        !s->woi || partner > 3 ||
        (partner >= 0 && partner < 4 && ctx->col1_ok && ctx->col1 &&
         ctx->col1->player[partner].control == 0);
      /* DOS scores the tile as an attack when the owner is not yet MET (a
       * forced first contact) or a Privateer is involved; Linux contact is
       * driven by ai_contact_*, so this port only takes the arm at war —
       * a deliberate narrowing, not a transcription slip. */
      const int at_war = partner >= 0 && ctx->col1 && ai_diplo_at_war(ctx->col1, nation, partner);
      if ((at_war ||
           ((rel & AI_DIPLO_MET) == 0 && s->dos_type == UNITS_KIND_PRIVATEER) ||
           hu_type == UNITS_KIND_PRIVATEER) &&
          woi_ok) {
        /* raw 88885-88887, LAB_521d_52aa entry: `if (DS:0x5236[type*0xe] !=
         * 0) { local_7e = 1; local_ea = 1; ... }`. local_ea is sticky for the
         * whole 8-direction scan and is set here, before the odds are known —
         * a −999 candidate still marks it. */
        if (s->combat != 0) {
          saw_foe = 1;
        }
        if (!ai_euro_20e6_attack_term(ctx, u, s, nx, ny, here, &score)) {
          continue;
        }
        attack = 1;
      } else {
        continue;
      }
    } else {
      const int rel_score = ai_diplo_indian_alarm(ctx->col1, owner, nation); /* 84fc > 0x4a */
      const int rel = ai_euro_20e6_diplo(ctx->col1, nation, owner);
      if (rel_score > 0x4a || (rel & AI_DIPLO_WAR)) {
        if (rel & AI_DIPLO_WAR) {
          score <<= 1;
        }
        if (ai_euro_20e6_own_colonies_on(ctx, nation, s->cid) == 0) {
          continue;
        }
        /* raw 88885-88887 again: the tribe branch enters the same LAB_52aa. */
        if (s->combat != 0) {
          saw_foe = 1;
        }
        if (!ai_euro_20e6_attack_term(ctx, u, s, nx, ny, here, &score)) {
          continue;
        }
        attack = 1;
      } else {
        continue;
      }
    }
    /* LAB_54f5 facing: −2·diff² against unit+0x314f. */
    if (last_dir >= 0 && last_dir < 8) {
      int diff = last_dir - d;
      if (diff < 1) {
        diff = -diff;
      }
      if (diff > 4) {
        diff = 8 - diff;
      }
      score += diff * diff * -2;
    }
    /* Neighbours of the destination: −10 per hostile combat unit when the
     * mover has combat byte 0 (Treasure). */
    for (int n = 0; n < 8; ++n) {
      const int ax = nx + MAP_DIR8_DX[n];
      const int ay = ny + MAP_DIR8_DY[n];
      if (ax < 0 || ay < 0 || ax >= ctx->map->width || ay >= ctx->map->height) {
        continue;
      }
      const int po = map_tile_tribe_or_presence(ctx->map, ax, ay);
      if (po < 0 || po == nation || s->combat != 0) {
        continue;
      }
      if ((ai_euro_20e6_diplo(ctx->col1, nation, po) & 0x60) != 0x20) {
        continue;
      }
      const int oid = units_id_at(ctx->units, ax, ay);
      const ColonizeUnit* ou = oid >= 0 ? units_get_const(ctx->units, oid) : NULL;
      if (ou && ai_euro_20e6_type_combat(ai_euro_20e6_dos_type(ctx->units, ou)) != 0) {
        score -= 10;
      }
    }
    /*
     * Ship-only (raw 88808-88830): a hostile Euro colony (diplo & 0x60 ==
     * 0x20) next to the destination costs 0x14 with a Stockade, 0x28 with a
     * Fort (FUN_281f_0322 feature bits 1 / 2), plus 0x1e per Artillery in
     * its stack, all multiplied by the ship's occupied goods holds — an
     * empty hull ignores the batteries entirely.
     */
    if (s->is_ship && goods_holds > 0) {
      for (int n = 0; n < 8; ++n) {
        const int ax = nx + MAP_DIR8_DX[n];
        const int ay = ny + MAP_DIR8_DY[n];
        const ColonizeColony* fc = colonies_find_at_xy(ctx->colonies, ax, ay);
        if (!fc || !fc->active || fc->nation_id < 0 || fc->nation_id > 3 ||
            fc->nation_id == nation) {
          continue;
        }
        if ((ai_euro_20e6_diplo(ctx->col1, nation, fc->nation_id) & 0x60) != 0x20) {
          continue;
        }
        const int bonus = colonies_fortification_defense_bonus_percent(ctx->colonies, fc);
        int pen = 0;
        if (bonus >= 100) {
          pen = 0x14;
        }
        if (bonus >= 150) {
          pen = 0x28;
        }
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* o = &ctx->units->units[i];
          if (o->active && o->aboard_ship_id < 0 && o->x == ax && o->y == ay &&
              ai_euro_20e6_dos_type(ctx->units, o) == 0xb) {
            pen += 0x1e;
          }
        }
        score -= pen * goods_holds;
      }
    }
    /* Far probe (unit + 4·dir) explore terms. */
    if (fog_enable) {
      const int fx = u->x + MAP_DIR8_DX[d] * 4;
      const int fy = u->y + MAP_DIR8_DY[d] * 4;
      if (s->is_ship && ai_euro_ship_dos_enabled()) {
        /* raw 88837-88840: the real DS:0x9faa coarse plane, restamped with
         * this nation's units and colonies at its 0a60 entry. */
        if (map_coords_inset(ctx->map, fx, fy) && !map_tile_is_water(ctx->map, fx, fy) &&
            ai_coarse_fog_explore_unseen(fx, fy)) {
          score += 8;
        }
      } else if (map_coords_inset(ctx->map, fx, fy) && !map_tile_is_water(ctx->map, fx, fy) &&
          ctx->map->seen && !map_tile_seen_by(ctx->map, fx, fy, nation)) {
        score += 8; /* DS:0x9faa coarse cell unseen — per-nation seen[] stand-in */
      }
      /* raw 88842-88844: a ship in the eastern half leans west (dirs SW/W/NW). */
      if (s->is_ship && d >= 5 && d <= 7 && u->x > (int)ctx->map->width / 2) {
        score += 4;
      }
      for (int n = 0; n < 8; ++n) {
        const int ax = fx + MAP_DIR8_DX[n];
        const int ay = fy + MAP_DIR8_DY[n];
        if (!map_coords_inset(ctx->map, ax, ay)) {
          continue;
        }
        if (nation < 4 && ctx->map->seen && !map_tile_seen_by(ctx->map, ax, ay, nation) &&
            (s->is_ship || !map_tile_is_water(ctx->map, ax, ay))) {
          score += 2; /* raw 88853-88856: ships count unseen water too */
        }
        /* DOS 521d:57c2 calls FUN_281f_0682 (unit-presence bit only) here,
         * not 06d2 — a settlement tile without a unit does not −2. */
        if (map_tile_owner_or_presence(ctx->map, ax, ay) >= 0) {
          score -= 2;
        }
        if (s->explorer) {
          score += (int)k_20e6_terr_site_byte[map_dos_terr_class_at(ctx->map, ax, ay) & 31];
        }
      }
    }
    if ((s->is_ship && getenv("AI_SHIP_TRACE")) || getenv("AI_4D2E_TRACE")) {
      fprintf(stderr, "[4d2e] u%d dir %d (%d,%d) score %d attack %d\n", u->id, d, nx, ny, score, attack);
    }
    if (score > best) {
      best = score;
      best_dir = d;
      best_attack = attack;
    }
  }
  /* LAB_5183 tail for an attack pick: DOS stays when fewer than 3 thirds
   * (one full move) remain — Linux moves is whole moves and the act
   * loop already requires >0, so nothing extra to gate here. */
  if (out_attack) {
    *out_attack = best_attack;
  }
  if (out_saw_foe) {
    *out_saw_foe = saw_foe;
  }
  return best_dir;
}

/*
 * DOS-LITERAL FUN_521d_20e6 raw 89011-89029 — the 0x46 arm of the LAB_4d2e
 * tail (asm OVL14_L0000:0x005992-0x005a31, viceroy_overlays.asm; the same body
 * decompiles a second time at raw 85856-85874, where the nation compare reads
 * `uStack_e6` and confirms it is the acting unit's own nation nibble).
 *
 * Position: the tail runs `local_ce == 0` (the winning wander pick is not an
 * attack, asm 0x5840) → `local_8e == 0` (not the own-colony ≥2-garrison entry,
 * asm 0x5876) → the 0x42 arm (DS:0x523d bit0 pioneer work, raw 88989-89000) →
 * the 0x65 arm (bit2, raw 89002-89009) → **this arm** → the 0x39 fallthrough
 * (asm 0x586a). Every failed sub-test jumps straight to 0x586a/0x39, so this
 * arm never changes the wander pick unless all four sub-tests pass.
 *
 *   bVar10 = unit+0x3146;                              // raw 89011
 *   if (DS:0x5236[bVar10 * 0xe] > 1 &&                 // @UNIT attack col > 1
 *       (bVar10 < 0xd || 0x12 < bVar10) &&             // not a ship type
 *       local_ea != 0) {                               // saw an attackable foe
 *     if (FUN_281f_098e(param_1) == FUN_281f_02ee(param_1)) {   // raw 89016-89019
 *       for (i = 0; i < 8; i++) {                      // raw 89020-89029
 *         y = DS:0xbe[i] + local_94; x = DS:0xb4[i] + local_88; // the UNIT's tile
 *         local_10 = FUN_281f_0696(x, y);              // Euro settlement owner
 *         if (-1 < local_10 && local_10 != uVar11) {   // owned by another nation
 *           unit+0x314b = 0x46; goto LAB_521d_5899;    // local_76 = 8 -> stay
 *         }
 *       }
 *     }
 *   }
 *
 * The args of the two chain walks are dropped by Ghidra (far thunks); the asm
 * at 0x59c9 passes the unit index in AX with no stack cleanup:
 *   MOV AX,[BP+6]; CALLF FUN_1000_8b7e  -> FUN_281f_098e -> FUN_1427_0026
 *   MOV AX,[BP+6]; CALLF FUN_1000_84de  -> FUN_281f_02ee -> FUN_1427_0002
 * FUN_1427_0026 walks unit+0x315e to the chain tail, FUN_1427_0002 walks
 * unit+0x315c to the chain head (raw 7241-7277); both return the unit itself
 * when its link is −1, so `tail == head` is exactly "this unit is alone in its
 * per-tile chain" — the same chain units_map_stack_chrome models (units.h).
 *
 * What the arm does NOT do: it issues no FORTIFY order and no goto. +0x314b is
 * the AI's own annotation byte and no DOS reader compares it against 0x46; the
 * whole behavioural effect is LAB_5899's `local_76 = 8`, i.e. the unit does not
 * take the wander step it just scored and its act state falls to 5/6 at the
 * epilogue (raw 90374-90386). It also spends no garrison quota and does not
 * require the unit to stand on (or near) a colony of its own. bugs.md #512.
 */
static int ai_euro_20e6_border_park_arm(
  ColonizeTurnContext* ctx, const ColonizeUnit* u, const Ai20e6Unit* s, int saw_foe
) {
  /* raw 89012-89013 / asm 0x5992-0x59c6, in DOS's own order. */
  if (s->combat <= 1) {
    return 0;
  }
  if (s->dos_type >= 0xd && s->dos_type <= 0x12) {
    return 0;
  }
  if (!saw_foe) {
    return 0;
  }
  /* raw 89016-89019 / asm 0x59c9-0x59df: alone in the tile chain. */
  if (units_map_stack_chrome(ctx->units, u->id)) {
    return 0;
  }
  /* raw 89020-89029 / asm 0x59e2-0x5a31: a Euro settlement of another nation
   * on one of the unit's own eight neighbours. */
  for (int d = 0; d < 8; ++d) {
    const int owner =
      ai_euro_20e6_colony_owner_at(ctx, u->x + MAP_DIR8_DX[d], u->y + MAP_DIR8_DY[d]);
    if (owner >= 0 && owner != s->nation) {
      return 1;
    }
  }
  return 0;
}

/*
 * Raw 88632-88664, the ship-only arm just ahead of LAB_4d2e: an idle hull
 * (bVar20) carrying military or Pioneers (8aac modes 4 + 3) with no unload
 * mask, outside the War of Independence, rolls FUN_281f_04d4(0, 0x10) once
 * per act; on 0 it picks a random inset tile (rng(2, w-3), rng(2, h-3)) and,
 * if that is open-sea water more than 7 Manhattan tiles away, latches unit
 * +0x3148 bit 0x10 and commits it as the goto (LAB_27f5). With the bit
 * already set, rng(0, 0x30) == 0 clears it. The unload mask is taken as 0
 * here: the port calls this after the ship band's own unload arm has had its
 * go, which is the only way DOS reaches this line with cargo still aboard.
 * Returns 1 when a far goto was set.
 */
int ai_euro_20e6_ship_far_roam(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s) {
  if (!s->is_ship || s->woi || u->id < 0 || u->id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, s->nation) >= 0) {
    return 0; /* bVar20 false */
  }
  int pioneers = 0;
  int mil = 0;
  int scouts = 0;
  int milvet = 0;
  int civ = 0;
  ai_euro_20e6_ship_cargo_counts(ctx, u, &pioneers, &mil, &scouts, &milvet, &civ);
  if (mil + pioneers == 0) {
    return 0;
  }
  uint8_t* flags = &u->col1_flags15; /* unit+0x3148 */
  if ((*flags & 0x10) == 0) {
    if (dos_rng_range(ctx->rng, 0, 0x10) != 0) {
      return 0;
    }
    const int tx = dos_rng_range(ctx->rng, 2, (int)ctx->map->width - 3);
    const int ty = dos_rng_range(ctx->rng, 2, (int)ctx->map->height - 3);
    if (!map_tile_is_water(ctx->map, tx, ty) || !ai_euro_20e6_open_sea(ctx->map, tx, ty)) {
      return 0;
    }
    if (abs(tx - u->x) + abs(ty - u->y) <= 7) {
      return 0;
    }
    *flags |= 0x10;
    ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, tx, ty);
    if (getenv("AI_SHIP_TRACE")) {
      fprintf(stderr, "[ship] unit %d far roam -> (%d,%d)\n", u->id, tx, ty);
    }
    return 1;
  }
  if (dos_rng_range(ctx->rng, 0, 0x30) == 0) {
    *flags &= (uint8_t)~0x10u;
  }
  return 0;
}

/*
 * Ship entry into the 20e6 wander scorer (raw 90210-90219 → LAB_4d2e). An
 * idle hull (act state 0 / 5 / 6 / 0xa, or a step goto onto its own tile)
 * always scores; a busy one only when FUN_281f_0984 finds a foreign unit
 * adjacent, and then the port keeps its course unless the pick is an attack
 * (a wander step would otherwise overwrite a delivery goto the port has no
 * goal record to rebuild from). An attack pick resolves at once through
 * ai_euro_try_attack (LAB_589e's step into a foe tile); a water pick becomes
 * a one-tile AI_SAIL goto for the sail loop below. Returns 1 when the act
 * committed something.
 */
int ai_euro_20e6_ship_wander_act(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id, int busy) {
  if (!ctx || !ctx->map || !u || !u->active || u->id < 0 || u->id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (!s.is_ship) {
    return 0;
  }
  if (!busy && ai_euro_20e6_ship_far_roam(ctx, u, &s)) {
    return 1;
  }
  const int dir = ai_euro_20e6_wander_step(ctx, u, &s, NULL, NULL);
  if (getenv("AI_SHIP_TRACE")) {
    fprintf(stderr, "[ship] unit %d wander dir %d busy %d at (%d,%d)\n", u->id, dir, busy, u->x, u->y);
  }
  if (dir < 0 || dir > 7) {
    if (!busy) {
      ai_euro_s_euro_last_dir[u->id] = 8; /* unit+0x314f, 8 = stay */
    }
    return 0;
  }
  const int nx = u->x + MAP_DIR8_DX[dir];
  const int ny = u->y + MAP_DIR8_DY[dir];
  const int foe = units_id_at(ctx->units, nx, ny);
  if (foe >= 0) {
    const ColonizeUnit* f = units_get_const(ctx->units, foe);
    if (!f || f->nation_id == nation_id) {
      return 0;
    }
    ai_euro_s_euro_last_dir[u->id] = (int8_t)dir;
    if (getenv("AI_SHIP_TRACE")) {
      fprintf(stderr, "[ship] unit %d wander attack (%d,%d)\n", u->id, nx, ny);
    }
    ai_euro_try_attack(ctx, u, nx, ny);
    return 1;
  }
  if (busy) {
    return 0;
  }
  ai_euro_s_euro_last_dir[u->id] = (int8_t)dir;
  if (getenv("AI_SHIP_TRACE")) {
    fprintf(stderr, "[ship] unit %d wander step (%d,%d)\n", u->id, nx, ny);
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, nx, ny);
  ai_euro_s_euro_roam_wander[u->id] = 1; /* unit+0x314c==5 idle-roam */
  return 1;
}

/* Returns non-zero to abort act (DOS 20e6 non-zero return). */
/*
 * FUN_521d_20e6 ring-hop pick (raw 2416-2458, the LAB_4b2c-region explorer
 * arm): when the windowed ring scan produced no target, an explorer hops
 * 4 tiles out along a latched random ring20 direction. Slot unset → roll
 * FUN_1000_86c4(1,0x14) − 1; target = (ring_dx*4, ring_dy*4); validate
 * walkable land (84f2), fresh coarse region (−0x6056 nibble &6 == 0 —
 * Linux substitution per the block header: target tile unseen by the
 * nation, the same seen-plane stand-in used for DS:0x9faa), same continent
 * (8912) and no presence at all (88c2 < 0). On success the hop length
 * max(dx*4, dy*4) (signed char, byte-faithful) latches into +0x3155, the
 * explore fatigue drops by 8 when above 8 (raw 2455-2456), and the target
 * commits as a goto (27f5). Returns 1 when a hop goto was set.
 */
static int ai_euro_20e6_ring_hop(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s) {
  if (!ctx || !ctx->map || !u || u->id < 0 || u->id >= COLONIZE_UNITS_MAX || s->cid < 0) {
    return 0;
  }
  int16_t* slotp = &ai_euro_s_20e6_hop_slot[u->id];
  if (*slotp == 0) {
    if (!ctx->rng) {
      return 0;
    }
    *slotp = (int16_t)dos_rng_range(ctx->rng, 1, 0x14); /* raw 2421: 86c4(1,0x14), stored +1 */
  }
  const int slot = (int)*slotp - 1;
  if (slot < 0 || slot >= 20) {
    *slotp = 0;
    return 0;
  }
  const int hx = (int)ai_euro_k_20e6_ring20_dx[slot] * 4;
  const int hy = (int)ai_euro_k_20e6_ring20_dy[slot] * 4;
  const int tx = u->x + hx;
  const int ty = u->y + hy;
  if (!map_coords_inset(ctx->map, tx, ty) || !map_tile_is_land(ctx->map, tx, ty)) {
    return 0; /* 84f2 */
  }
  if (map_tile_seen_by(ctx->map, tx, ty, s->nation)) {
    return 0; /* −0x6056 coarse region already stamped (substituted) */
  }
  if (map_continent_id_at(ctx->map, tx, ty) != s->cid) {
    return 0; /* 8912 */
  }
  if (ai_euro_20e6_tribe_or_presence(ctx, tx, ty) >= 0) {
    return 0; /* 88c2: any owner/presence blocks the hop */
  }
  if (getenv("AI_20E6_HOP_TRACE")) {
    fprintf(stderr, "[hop] unit %d n%d slot %d -> (%d,%d)\n", u->id, s->nation, slot, tx, ty);
  }
  ai_euro_s_20e6_hop_steps[u->id] = (int8_t)(hx < hy ? hy : hx); /* raw 2447-2453 */
  if (ai_euro_s_20e6_explore_fatigue[u->id] > 8) {
    ai_euro_s_20e6_explore_fatigue[u->id] -= 8;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
  return 1;
}

/* FUN_1000_8a98 → FUN_281f_08a8 → FUN_1427_14f4: nearest unit of `nation`
 * (excluding `except_id`) by dos_dist; ties take the LAST match (DOS `<=`). */
static int ai_euro_20e6_nearest_own_unit(
  const ColonizeTurnContext* ctx, int nation, int except_id, int x, int y
) {
  int best = -1;
  int bd = 9999;
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id —
   * so `except_id` (the caller passes `u->id`) has to be matched against
   * `o->id`, and the returned handle is `o->id`, which is what the caller
   * feeds back to `units_get_const`. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->nation_id != nation || o->id == except_id) {
      continue;
    }
    const int d = map_dos_dist(x - o->x, y - o->y);
    if (d <= bd) {
      bd = d;
      best = o->id;
    }
  }
  return best;
}

/*
 * FUN_521d_20e6 treasure act band, FIRST arm — the in-colony cash-in
 * (move_scoring_20e6_full.md raw ~2313-2331, inside
 * `if (*(char *)(param_2 * 0x1c + 0x3146) == '\n')`):
 *
 *   if (iStack_2e == 0) {
 *     uVar13 = (uint)*(byte *)(param_2 * 0x1c + 0x315b) * 100;
 *     *(uint *)(*(int *)0x84fc + 0x2a) += uVar13;   (32-bit nation gold)
 *     if ((*(byte *)0x5382 & 1) == 0) { ... FUN_1000_8842(0x181f,0x1786,2); }
 *     goto LAB_OVL14_L0000__0047b9;                 destroy_unit
 *   }
 *
 * It sits BEFORE the bind+goto (iStack_2c == iStack_38), the 8a98 rendezvous
 * and the 0072d6 stranded-destroy, so it is checked first here too — ahead of
 * the port's Cortes / board / coast chain, which has no counterpart anywhere
 * in this band. DOS charges no Crown share and needs no Galleon, no Cortes and
 * no coastal colony: an AI Treasure that reaches ANY own colony is money.
 *
 * Traps carried over from the 47b9 pass (see its header): iStack_2e == 0 must
 * be read as `home_colony >= 0 && home_dist == 0` because the Linux prologue
 * zeroes home_dist on a missed colony search, and a unit sitting in the Europe
 * pool has lane-sentinel coordinates and must be excluded outright.
 *
 * Trace env: AI_20E6_TREASURE_TRACE=1. Kill switch: AI_20E6_TREASURE_CASH=0.
 * Returns 1 when the treasure was consumed (caller must stop touching it).
 */
#ifndef AI_20E6_TREASURE_CASH_DEFAULT
#define AI_20E6_TREASURE_CASH_DEFAULT 1
#endif

static int ai_euro_20e6_treasure_cash_enabled(void) {
  return ai_euro_env_flag("AI_20E6_TREASURE_CASH", AI_20E6_TREASURE_CASH_DEFAULT);
}

int ai_euro_20e6_treasure_cash_in(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int nation_id
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !u || !u->active) {
    return 0;
  }
  if (!ctx->col1_ok || !ctx->col1) {
    return 0; /* no nation record to credit */
  }
  if (u->id < 0 || u->id >= COLONIZE_UNITS_MAX || u->aboard_ship_id >= 0) {
    return 0; /* a passenger is not standing in the colony */
  }
  if (!ai_euro_20e6_treasure_cash_enabled()) {
    return 0;
  }
  /* Europe-pool lane sentinels are not map coordinates (47b9 trap 3). */
  if (ai_euro_in_europe(u->x, u->y)) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (s.dos_type != UNITS_KIND_TREASURE) {
    return 0;
  }
  if (s.cid < 0) {
    return 0; /* iStack_38 < 0: off-land, DOS never runs the band */
  }
  /* iStack_6 (raw 2250): a goal-tasked unit skips the whole band. */
  if (s.order_code == 't' || s.order_code == 'i') {
    return 0;
  }
  /* iStack_2e == 0, read the Linux way (47b9 trap 1). */
  if (!(s.home_colony >= 0 && s.home_dist == 0)) {
    return 0;
  }
  const int uid = u->id; /* the call despawns u — snapshot before it runs */
  const int ux = u->x;
  const int uy = u->y;
  const int gold = units_ai_treasure_cash_in_colony(
    ctx->units, ctx->col1, nation_id, uid, ctx->ai_popups, ctx->messages
  );
  if (getenv("AI_20E6_TREASURE_TRACE")) {
    fprintf(
      stderr,
      "[20e6-treasure] unit %d n%d (%d,%d) colony %d -> cash %d\n",
      uid, nation_id, ux, uy, s.home_colony, gold
    );
  }
  return 1; /* DOS destroys the unit whatever the value byte held */
}

/*
 * LAB_521d_47b9 — wagon / treasure dead-end destroy (raw 2249-2360 of
 * move_scoring_20e6_full.md; `47b9` itself is just
 * `FUN_1000_89f8(unit)` = FUN_281f_0808 → FUN_1427_0824 destroy_unit, then
 * return). Reached from the LAB_521d_457e band once the ship arms above it
 * have declined, so it is the AI's cleanup for a hauler with nowhere to go.
 *
 * Wagon Train (type 0x0c), village-delivery slot unit+0x3158 == 0:
 *   - standing on an own colony (iStack_2e == 0): bind +0x314a to it and park
 *     (orders 0x55) — not a dead end.
 *   - unbound (+0x314a < 0) and the nearest own colony is NOT on this
 *     landmass (iStack_2c != iStack_38, and iStack_2c is −2 when the nation
 *     owns no colony at all) → DESTROY.
 *   - otherwise bind the nearest own colony and goto it (LAB_4701/4567).
 * Treasure Train (type 0x0a), not standing in a colony:
 *   - nearest own colony on this landmass → bind + goto (LAB_4701).
 *   - else nearest own unit (FUN_1000_8a98) on this landmass → walk to it
 *     (LAB_27f5; the escort/galleon rendezvous).
 *   - else, when the adjacent-claim probe (FUN_OVL14_0072d6 → FUN_521d_0906,
 *     ai_euro_20e6_probe_adjacent) names the human player (DS:0x5398)
 *     → DESTROY.
 *
 * NOT ported here, deliberately:
 *   - the +0x3158 != 0 village-errand arm lives in
 *     ai_euro_20e6_wagon_village_errand (ported 2026-09-06f with the wagon
 *     load matrix that writes the latch); this function only handles the
 *     +0x3158 == 0 arms and skips errand wagons below.
 *
 * The treasure in-colony cash-in (iStack_2e == 0) is the band's FIRST treasure
 * arm and lives in ai_euro_20e6_treasure_cash_in below, called ahead of the
 * rest of the treasure chain so DOS precedence holds; this function is only
 * reached once that arm has declined.
 *
 * Returns 0 when the band declined, 1 when it bound a course (LAB_4701 /
 * LAB_27f5) and 2 when the unit was destroyed (caller must stop touching it).
 */
int ai_euro_20e6_47b9_dead_end(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !u || !u->active) {
    return 0;
  }
  if (u->id < 0 || u->id >= COLONIZE_UNITS_MAX || u->aboard_ship_id >= 0) {
    return 0;
  }
  /* DOS's unit loop never reaches 20e6 for a unit sitting in the Europe pool
   * (its x/y are the lane sentinels, not map coords) — without this guard a
   * wagon waiting in Europe reads iStack_38 = −1 and looks stranded. */
  if (ai_euro_in_europe(u->x, u->y)) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (s.dos_type != UNITS_KIND_WAGON && s.dos_type != UNITS_KIND_TREASURE) {
    return 0;
  }
  if (s.cid < 0) {
    return 0; /* iStack_38 < 0: off-land (aboard / sentinel) — no dead-end call */
  }
  /* iStack_6 (raw 2250): a unit already tasked by the goal engine skips the
   * whole band and falls straight to LAB_5a78. */
  if (s.order_code == 't' || s.order_code == 'i') {
    return 0;
  }
  /* iStack_2e == 0 only means "on the colony" when one was actually found —
   * the prologue zeroes home_dist on a miss (DOS leaves DS:0x8db8 stale). */
  const int in_own_colony = (s.home_colony >= 0 && s.home_dist == 0);
  const int destroy_trace = getenv("AI_20E6_DEADEND_TRACE") != NULL;

  if (s.dos_type == UNITS_KIND_WAGON) {
    /* The wagon 457e arm now lives whole in ai_euro_20e6_wagon_origin_walk
     * (bind / park / walk / destroy), called from ai_euro_try_wagon_haul in
     * DOS position. Nothing left for the act-level dead-end pass to do. */
    return 0;
  }

  /* Treasure Train. */
  if (in_own_colony) {
    return 0; /* cash-in arm — ai_euro_20e6_treasure_cash_in already had it */
  }
  /*
   * arm 2 (raw 90016-90017): `uVar14 = local_62; if (local_2c == local_38)
   * goto LAB_4701` — FUN_281f_09e6(nearest own colony) selects it, LAB_4567
   * loads its coords and LAB_27f5 walks there. local_62 is the prologue's
   * nearest own colony by FUN_281f_037a distance with NO coastline term, and
   * the guard is a continent-id compare. DOS does not write the unit's
   * +0x314a origin byte on this arm (only the wagon 457e arm does).
   */
  if (s.home_cid == s.cid && s.home_colony >= 0) {
    const ColonizeColony* home = colonies_get(ctx->colonies, s.home_colony);
    if (home && home->active && home->nation_id == nation_id) {
      if (units_orders_follow_goto(u->orders) && u->goto_x == home->x &&
          u->goto_y == home->y) {
        return 1; /* already walking there */
      }
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, home->x, home->y);
      return 1;
    }
  }
  /*
   * arm 3 (raw 90018-90030): nearest own unit (FUN_281f_08a8) whose tile's
   * continent id equals the treasure's → LAB_27f5 walk to it.
   */
  {
    const int mate = ai_euro_20e6_nearest_own_unit(ctx, nation_id, u->id, u->x, u->y);
    if (mate >= 0) {
      const ColonizeUnit* mu = units_get_const(ctx->units, mate);
      if (mu && map_continent_id_at(ctx->map, mu->x, mu->y) == s.cid) {
        if (units_orders_follow_goto(u->orders) && u->goto_x == mu->x &&
            u->goto_y == mu->y) {
          return 1;
        }
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, mu->x, mu->y);
        return 1;
      }
    }
  }
  {
    const int human = (ctx->col1_ok && ctx->col1) ? col1_save_human_nation(ctx->col1) : -1;
    if (human >= 0 && ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, nation_id) == human) {
      if (destroy_trace) {
        fprintf(stderr, "[47b9] treasure %d n%d (%d,%d) stranded -> destroy\n",
                u->id, nation_id, u->x, u->y);
      }
      (void)units_despawn(ctx->units, u->id);
      return 2;
    }
  }
  return 0;
}

/*
 * LAB_521d_457e wagon arm, WHOLE (raw 2256-2289 — doc 2263-2296), the
 * `type == 0x0c && +0x3158 == 0` band. Replaces the port's old substitute of
 * letting wagons peel the ships-only 4393 work queue (divergence retired
 * 2026-09-07):
 *
 *   if (iStack_2e == 0) {                     // standing at an own colony
 *     if (+0x314a < 0) +0x314a = uStack_62;   // bind origin to it
 *     if (+0x314a == uStack_62) {             // at MY colony:
 *       +0x314b = 0x55; goto LAB_5899;        //   park (sentry cache byte)
 *     }
 *   }
 *   if (+0x314a < 0) {
 *     if (iStack_2c != iStack_38) goto 47b9;  // unbound, no own colony on
 *     +0x314a = uStack_62;                    //   this landmass → DESTROY
 *   }
 *   bind colony +0x314a; goto LAB_4567;       // walk home (27f5)
 *
 * DOS's 0x55 is 20e6's own per-unit decision cache, not a real order — the
 * band re-runs next turn — so the park is modelled as claiming the beat with
 * no goto. The lifecycle this closes: dump at bound colony (arrival block) →
 * load one hold + errand → village sell (2820 clears +0x3158) → walk home →
 * park/dump again. Returns 1 when the beat is claimed (incl. destroy).
 */
int ai_euro_20e6_wagon_origin_walk(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !u || !u->active) {
    return 0;
  }
  if (u->id < 0 || u->id >= COLONIZE_UNITS_MAX || u->aboard_ship_id >= 0) {
    return 0;
  }
  if (ai_euro_in_europe(u->x, u->y)) {
    return 0; /* DOS's unit loop never reaches 20e6 in the Europe pool */
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (s.dos_type != UNITS_KIND_WAGON || s.cid < 0) {
    return 0;
  }
  if (s.order_code == 't' || s.order_code == 'i') {
    return 0; /* iStack_6 (raw 2250): tasked unit skips the band */
  }
  if (ai_euro_s_20e6_wagon_errand[u->id]) {
    return 0; /* +0x3158 != 0 — the errand walker owns this wagon */
  }
  const int destroy_trace = getenv("AI_20E6_DEADEND_TRACE") != NULL;
  /* iStack_2e == 0 only means "on the colony" when one was actually found —
   * the prologue zeroes home_dist on a miss (DOS leaves DS:0x8db8 stale). */
  const int in_own_colony = (s.home_colony >= 0 && s.home_dist == 0);
  int ori = ai_euro_20e6_origin_get(u);
  if (in_own_colony) {
    if (ori < 0) {
      ai_euro_20e6_origin_set(u, s.home_colony);
      ori = s.home_colony;
    }
    if (ori == s.home_colony) {
      return 1; /* orders 0x55, LAB_5899 park — beat claimed, no goto */
    }
  }
  if (ori < 0) {
    if (s.home_cid != s.cid) {
      if (destroy_trace) {
        fprintf(stderr, "[47b9] wagon %d n%d (%d,%d) no colony on cid %d -> destroy\n",
                u->id, nation_id, u->x, u->y, s.cid);
      }
      (void)units_despawn(ctx->units, u->id);
      return 1;
    }
    ai_euro_20e6_origin_set(u, s.home_colony);
    ori = s.home_colony;
  }
  const ColonizeColony* home = colonies_get(ctx->colonies, ori);
  if (!home || !home->active || home->nation_id != nation_id) {
    /* bugs.md #814: DOS keeps the dangling +0x314a and walks to the stale
     * colony record (raw 89960-89964: `uVar14 = +0x314a` → LAB_521d_4701 →
     * LAB_521d_4567) — it never unbinds. The port cannot dereference a dead
     * slot, so it claims the beat and leaves the binding alone; the colony
     * tick rebinds. Do NOT reintroduce an `origin_set(u, -1)` here. */
    return 1;
  }
  /* bugs.md #814: no `x == home->x && y == home->y` arm in DOS — the
   * in-colony case is the `iStack_2e == 0` / park-0x55 arm above
   * (raw 89945-89953). */
  if (units_orders_follow_goto(u->orders) && u->goto_x == home->x && u->goto_y == home->y) {
    return 1;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, home->x, home->y);
  return 1;
}

/*
 * LAB_521d_457e empty-ship High Seas cadence (raw 2251-2257). An untasked
 * ship (type 0x0d..0x12) carrying no passengers (iStack_a8 == 0) and no
 * TradeGoods/Tools/Muskets/Horses (iStack_44 < 0, the LAB_3558 per-cargo
 * tally's max id over `> 0xc || == 8`), whose type gate bVar7 holds, jumps to
 * LAB_521d_3fa6 when EITHER unit+0x3148 bit 0x20 is set OR
 * `((char)unit_index + (char)DS:0x538e) & 0x1f == 0` (DS:0x538e = the turn
 * counter — europe_nation_eot.md). LAB_3fa6 = FUN_1000_94da → FUN_291f_02ea →
 * FUN_48d3_015e: spiral out for a High Seas tile (terrain 0x1a) that is empty
 * or own, latch it as the goto and stamp orders 0x45 — i.e. sail home.
 *
 * bVar7 (decomp 88556-88579), full formula live 2026-09-07 — the "budget
 * bytes with no decoded writer" turned out to be census fields the port
 * already persists (save I/O rows FUN_1d1d_060c(0x9414,4,..) /
 * (0x924c,0x4c,..) pinned them): 0x9414 = stuff.ship_cargo_totals,
 * 0x924c stride-0x13 = stuff.unit_type_counts[4][19], so −0x6da4/−0x6da2 =
 * counts of type 0x10 (Privateer) / 0x12 (Man-O-War).
 *   - base: type != 0x12;
 *   - Frigate (0x11): ship_cargo_totals[n] − 3*frigate_count[n] −
 *     privateer_count[n] < 4, AND FUN_281f_0984 (→ FUN_1427_09dc, the
 *     8-adjacent foreign-owner-on-same-body probe) returns 0;
 *   - Privateer (0x10): DS:0xa89b ≥ 2 || DS:0x9e52 ≥ 7 — those are the
 *     census frigate-pressure tallies (own colonies with a foreign Frigate
 *     in reach / their pop sum, ai_euro_s_ship_pressure), NOT difficulty (the old
 *     substitution misread 0xa89b);
 *   - Man-O-War (0x12): re-allowed on odd unit index or
 *     unit_type_counts[n][0x12] == 1.
 *
 * WIRED LIVE (2026-09-06d), in DOS order: the ship act calls this only after
 * the 4393 work-queue haul (ai_euro_try_ship_trade_haul) declines, which is
 * where LAB_457e sits in the raw flow, and behind the same
 * `!at_war && !ship_has_useful_goto` chain guard as its
 * neighbours. The golden-pinned SW coastal cruise
 * (ai_euro_try_post_found_coast_cruise) still runs first, so it keeps
 * ownership of the TURN4→7 beachhead transports; the cadence never fires on
 * any golden fixture (AI_20E6_HS_TRACE over golden_ai_turns / _mid01 /
 * _late01 / _joint / smoke_play: zero hits) and all 57 ctest cases stay
 * green. Kill switch / bisect: AI_20E6_HS_CADENCE=0.
 *
 * Thin: DOS also stamps orders 0x45 and act_state 3/0xb on the unit; the port
 * stamps UNITS_ORDER_AI_SAIL onto the spiral tile and leaves the actual
 * Europe crossing to the existing high-seas handling.
 */
#ifndef AI_20E6_HS_CADENCE_DEFAULT
#define AI_20E6_HS_CADENCE_DEFAULT 1
#endif

int ai_euro_20e6_hs_cadence_enabled(void) {
  return ai_euro_env_flag("AI_20E6_HS_CADENCE", AI_20E6_HS_CADENCE_DEFAULT);
}

/*
 * FUN_281f_0984 → FUN_1427_09dc (decomp 7927-7968): walk the 8 neighbours of
 * (x,y); a tile with a foreign UNIT owner always hits (the body compare is
 * trivially true on that arm — local_8 is overwritten with the neighbour's
 * body first); a foreign COLONY hits only when the neighbour tile's body id
 * equals the probe tile's current body (local_8 is NOT overwritten on that
 * arm), which for a ship at sea means never (land vs water body). Owner
 * lookups substituted with pool scans (the DOS layer2/3 presence bits mirror
 * them).
 */
int ai_euro_20e6_adjacent_foreign_09dc(
  const ColonizeTurnContext* ctx, int x, int y, int nation_id
) {
  if (!ctx || !ctx->map) {
    return 0;
  }
  const int own_unit = ctx->units ? units_id_at(ctx->units, x, y) : -1;
  int own_tile_owner = -1;
  if (own_unit >= 0) {
    const ColonizeUnit* ou = units_get_const((ColonizeUnitPool*)ctx->units, own_unit);
    own_tile_owner = ou ? ou->nation_id : -1;
  }
  int local_8 = (int)(map_get_layer3(ctx->map, x, y) & 0x0fu);
  for (int d = 0; d < 8; ++d) {
    const int nx = x + MAP_DIR8_DX[d];
    const int ny = y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    int b = local_8;
    if (own_tile_owner < 0) {
      b = (int)(map_get_layer3(ctx->map, nx, ny) & 0x0fu);
    }
    int owner = -1;
    const int uid = ctx->units ? units_id_at(ctx->units, nx, ny) : -1;
    if (uid >= 0) {
      const ColonizeUnit* nu = units_get_const((ColonizeUnitPool*)ctx->units, uid);
      owner = nu ? nu->nation_id : -1;
    }
    int keep = b;
    if (owner < 0) {
      const int cid = ctx->colonies ? colonies_id_at(ctx->colonies, nx, ny) : -1;
      if (cid >= 0) {
        owner = ctx->colonies->colonies[cid].nation_id;
      }
      keep = local_8;
    }
    local_8 = keep;
    if (owner >= 0 && owner != nation_id && b == local_8) {
      return 1;
    }
  }
  return 0;
}

/* bVar7 (decomp 88556-88579) — see the HS-cadence header above for the
 * operand decode. `u` is the acting ship. */
int ai_euro_20e6_457e_type_gate(
  const ColonizeTurnContext* ctx, const ColonizeUnit* u, int dos_type
) {
  const int n = u ? u->nation_id : -1;
  const ColonizeCol1Stuff* stuff =
    (ctx && ctx->col1_ok && ctx->col1) ? &ctx->col1->stuff : NULL;
  if (dos_type == UNITS_KIND_MAN_O_WAR) { /* Man-O-War (decomp 88576-88579) */
    if (u && (u->id & 1) != 0) {
      return 1;
    }
    return stuff && n >= 0 && n < 4 && stuff->unit_type_counts[n][0x12] == 1;
  }
  if (dos_type == UNITS_KIND_FRIGATE) { /* Frigate (decomp 88557-88566) */
    if (!stuff || !u || n < 0 || n > 3) {
      return 0;
    }
    const int budget = (int)stuff->ship_cargo_totals[n] -
                       3 * (int)stuff->unit_type_counts[n][0x11] -
                       (int)stuff->unit_type_counts[n][0x10];
    if (budget >= 4) {
      return 0;
    }
    return !ai_euro_20e6_adjacent_foreign_09dc(ctx, u->x, u->y, n);
  }
  if (dos_type == UNITS_KIND_PRIVATEER) { /* Privateer (decomp 88567-88574) */
    if (n < 0 || n > 3) {
      return 0;
    }
    const AiEuroShipPressure* sp = &ai_euro_s_ship_pressure[n];
    return sp->frigate_colonies >= 2 || sp->frigate_pop >= 7;
  }
  return 1;
}

int ai_euro_20e6_457e_hs_cadence(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  if (u->id < 0 || u->id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (s.dos_type < 0xd || s.dos_type > 0x12) {
    return 0;
  }
  if (s.order_code == 't' || s.order_code == 'i') {
    return 0; /* iStack_6 */
  }
  if (u->cargo_count != 0) {
    return 0; /* iStack_a8 = stack−1 */
  }
  /* iStack_44: max hold cargo id over `id > 0xc || id == 8`
   * (TradeGoods/Tools/Muskets, plus Horses) — < 0 means none aboard. */
  {
    const int holds = units_goods_hold_count(ctx->units, u->id);
    for (int h = 0; h < holds; ++h) {
      const int ct = u->hold_goods_type[h];
      if (units_hold_amount(ctx->units, u->id, h) <= 0 || ct < 0) {
        continue;
      }
      if (ct > 0xc || ct == 8) {
        return 0;
      }
    }
  }
  if (!ai_euro_20e6_457e_type_gate(ctx, u, s.dos_type)) {
    return 0;
  }
  const int spare = (u->col1_flags15 & AI_EURO_F3148_SPARE) != 0; /* unit+0x3148 */
  if (!spare && (((char)u->id + (char)s.turn) & 0x1f) != 0) {
    return 0;
  }
  /* Already on High Seas — orders 0x45 means cross, not re-spiral (the
   * re-spiral skips the own tile as occupied and wiggles the hull between
   * two rim tiles forever). */
  if (map_tile_is_high_seas(ctx->map, u->x, u->y)) {
    if (getenv("AI_20E6_HS_TRACE")) {
      fprintf(stderr, "[457e] ship %d n%d (%d,%d) turn %d on HS -> Europe\n", u->id, nation_id,
              u->x, u->y, s.turn);
    }
    return ai_euro_ship_enter_europe(ctx, u);
  }
  int hx = 0;
  int hy = 0;
  if (!units_spiral_place_hs_near(ctx->units, ctx->map, u->x, u->y, nation_id, &hx, &hy)) {
    return 0;
  }
  if (getenv("AI_20E6_HS_TRACE")) {
    fprintf(stderr, "[457e] ship %d n%d (%d,%d) turn %d spare %d -> HS (%d,%d)\n", u->id,
            nation_id, u->x, u->y, s.turn, spare, hx, hy);
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, hx, hy);
  return 1;
}

/*
 * LAB_521d_589e, the shared FUN_521d_20e6 exit tail, `local_76 == 8` arm
 * (DOS-LITERAL FUN_521d_20e6 raw 90378-90386):
 *   unit+0x314f = local_76;                       // facing, 8 = stay
 *   if (local_76 == 8) {
 *     if (+0x314c != 5 && +0x314c != 6) +0x314c = 5;
 *     if (+0x3148 & 2) +0x314c = 6;
 *   }
 * Every "the unit stays this beat" exit of 20e6 funnels through here (the
 * LAB_5899 garrison/park jumps included), so a stay always lands the unit in
 * the act_state 5/6 fortify family. DOS never touches +0x314d/e (the goto
 * tile) on this arm — a stale course is simply no longer dispatched, because
 * FUN_521d_5b66 only walks a goal at act_state 0x0b (raw 90552).
 * (bugs.md #554.)
 */
void ai_euro_20e6_stay_tail_589e(ColonizeUnit* u) {
  if (!u) {
    return;
  }
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
    ai_euro_s_euro_last_dir[u->id] = 8; /* +0x314f = local_76 = 8 */
  }
  u->last_dir = 8;
  if (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED) {
    u->orders = UNITS_ORDER_FORTIFY; /* +0x314c = 5 */
  }
  if (u->col1_flags15 & AI_EURO_F3148_ROAM) {
    u->orders = UNITS_ORDER_FORTIFIED; /* +0x314c = 6 */
  }
}

int ai_euro_move_scoring_gate(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  /*
   * Ships: never retarget here — landfall/sail courses are owned by case 0x0b.
   * (Sticky clear or arrival wipe must not become a distant FOUND yank.)
   */
  if (units_is_sea(ctx->units, u->id)) {
    return 0;
  }
  /*
   * DOS-LITERAL FUN_521d_20e6 entry bail, raw 88404-88406 (bugs.md #734):
   *   if (act_state != 0 && act_state != 5 && act_state != 6 && act_state < 10)
   *     goto LAB_521d_5a78;
   * i.e. a unit holding any other order (sentry, goto, build colony, plow,
   * road ...) is left alone; only the LAB_5a78 tail runs, which the caller
   * applies after this returns. 20e6 returns 0 there (raw 90445); this
   * port's 0 means "nothing decided here", the same thing.
   */
  if (u->orders != 0 && u->orders != 5 && u->orders != 6 && u->orders < 10) {
    return 0;
  }
  /*
   * (bugs.md #521 lead b, 2026-09-18b) The at-war "defer course to act-level
   * hunt" early return that stood here — idle Soldier/Dragoon/Scout/Artillery
   * of a nation at war with any peer left the gate before any 20e6 arm ran —
   * had no DOS counterpart and was the reason the LAB_521d_4d2e attack term
   * could never fire on land. FUN_521d_20e6 has exactly two act-state gates,
   * both ported below: the entry bail (raw 88404-88406, `act_state ∉ {0,5,6}
   * && act_state < 10 → tail`) and the pre-4d2e gate (raw 90210-90219).
   * Neither looks at war state. Removed; the adjacent fight is now picked by
   * ai_euro_20e6_wander_step's attack term, as in DOS.
   */
  /*
   * FUN_521d_20e6 own-colony garrison arm, raw 88584-88612 (bugs.md #522).
   * This replaces three uncited early returns that used to stand here —
   * "passive colony Artillery", "military name on own colony" and "colony
   * wants construction labor" — siblings of the at-war bail deleted
   * 2026-09-18b. They parked every armed unit that stood on one of its own
   * colonies, so the whole land-arms branch below (and with it the LAB_4d2e
   * attack term) was unreachable for exactly the units that fight.
   *
   * DOS, verbatim, for a non-wagon/non-ship type (local_34 == 0):
   *   if (attack[type] < 2 || type == 4 || type == 8 || local_2e != 0 ||
   *       (colony(local_62).labor_shortage < 1 && order_code != 'A'))
   *      goto LAB_521d_277a;                       // normal arm chain
   *   armed = # units in this tile's chain with attack[type] > 1,
   *           type ∉ [0xd,0x12], type != 4, type != 8
   *   if (order_code == 'A')  goto LAB_521d_5899;  // stay
   *   if (armed < 2) { colony.labor_shortage--; order_code = 0x47;
   *                    goto LAB_521d_5899; }       // stay, garrison
   *   local_8e = 1; goto LAB_521d_4d2e;            // surplus: free-score
   * local_2e is the distance to the nearest own colony (DS:0x8db8 after the
   * uStack_62 search), so `local_2e == 0` is "standing on an own colony";
   * colony +0x8e is labor_shortage. Types 4 (Dragoons) and 8 (Cavalry) never
   * garrison and are not counted. LAB_5899 forces local_76 = 8, i.e. the unit
   * stays. The surplus branch is the DOS route by which a stack of two or
   * more armed units on an own colony marches back out and free-scores its
   * eight neighbours — the assault engine the port was missing.
   */
  int force_wander = 0; /* local_8e */
  {
    const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
    const int not_hauler = dtype < 0xd || dtype > 0x12; /* local_34 == 0 */
    const int cid = ctx->colonies ? colonies_id_at(ctx->colonies, u->x, u->y) : -1;
    ColonizeColony* oc = cid >= 0 ? colonies_get_mut(ctx->colonies, cid) : NULL;
    const int on_own_colony = oc && oc->active && oc->nation_id == nation_id; /* local_2e == 0 */
    const int admitted = u->col1_ai_plan == 'A'; /* +0x314b == 'A' */
    if (not_hauler && on_own_colony && ai_euro_20e6_type_combat(dtype) > 1 && dtype != 4 &&
        dtype != 8 && (oc->labor_shortage >= 1 || admitted)) {
      /* raw 88594-88600: the tile-chain armed count. */
      int armed = 0;
      for (int id = 1; id < COLONIZE_UNITS_MAX; ++id) {
        const ColonizeUnit* su = units_get_const(ctx->units, id);
        if (!su || !su->active || su->x != u->x || su->y != u->y) {
          continue;
        }
        const int st = ai_euro_20e6_dos_type(ctx->units, su);
        if (ai_euro_20e6_type_combat(st) > 1 && (st < 0xd || st > 0x12) && st != 4 && st != 8) {
          armed++;
        }
      }
      if (admitted) {
        ai_euro_20e6_stay_tail_589e(u); /* LAB_5899 local_76 = 8 -> LAB_589e */
        return 0;
      }
      if (armed < 2) {
        if (oc->labor_shortage > 0) {
          oc->labor_shortage--;
        }
        u->col1_ai_plan = 0x47;
        ai_euro_20e6_stay_tail_589e(u); /* LAB_5899 local_76 = 8 -> LAB_589e */
        return 0;
      }
      force_wander = 1; /* local_8e = 1, straight to LAB_521d_4d2e */
    }
  }
  int gx = u->x;
  int gy = u->y;
  int fx = 0;
  int fy = 0;
  int is_roam = 0;
  /*
   * No colony yet: settle where we landed, FUN_521d_06ae style (own tile plus
   * the eight neighbours), rather than walking at the nation-wide goal band.
   * Those goals sit next to villages all over the map, so a freshly landed
   * founder used to set off across the continent and either never arrive or
   * oscillate between two tiles forever.
   */
  int landed_settle = 0;
  if (ctx->colonies && colonies_count_for_nation(ctx->colonies, nation_id) == 0) {
    const ColonizeUnitKind fkind = ai_euro_unit_kind(ctx->units, u);
    if (ai_euro_name_is_pioneer(fkind) || fkind == UNITS_KIND_COLONIST) {
      int lx = 0;
      int ly = 0;
      if (ai_euro_pick_founding_tile(
            ctx->map, ctx->colonies, ctx->col1_ok ? ctx->col1 : NULL, ctx->units,
            nation_id, u->x, u->y, &lx, &ly
          )) {
        /*
         * bugs.md #708: the `ai_goals_upsert_primary(..., AI_GOAL_FOUND, 7)`
         * that stood here is gone (2026-09-23). FUN_521d_20e6 contains no goal
         * table writer at all — the goal tables are written only by
         * FUN_521d_0a60 and the 0906 producers — and the priority 7 was a bare
         * constant with no DOS origin. The local 06ae-shaped pick below is kept
         * as this act's walk target only (#530 opening scaffolding, untouched).
         */
        fx = lx;
        fy = ly;
        landed_settle = 1;
      }
    }
  }
  /*
   * FUN_521d_20e6 pre-LAB_4d2e gate, raw 90210-90219 — the last thing the arm
   * chain does before the 8-direction scorer:
   *   if (act_state != 0 && act_state != 10 && act_state != 5 &&
   *       act_state != 6 &&
   *       (act_state != 0x0b || +0x314d != x || +0x314e != y)) {
   *     if (FUN_281f_0984(x, y, continent) == 0) goto LAB_521d_5a78;
   *   }
   *   goto LAB_521d_4d2e;
   * i.e. a courseless unit (0/5/6), one flagged "a foreigner stands on one of
   * my eight neighbours" (10, FUN_521d_0a60 raw 87567) and a goal-bound unit
   * already standing on its goal tile ALWAYS free-score their neighbours; a
   * goal-bound unit whose goal lies elsewhere free-scores only when
   * FUN_281f_0984 (→ FUN_1427_09dc) finds a foreign unit or colony adjacent,
   * and otherwise leaves 20e6 for FUN_521d_5b66 to walk the goal.
   *
   * The port used to skip this test entirely and course every land unit at
   * the nearest FOUND goal, which is why the land arms below were never
   * entered in any golden (bugs.md #522: 284 of 460 gate calls).
   *
   * 2026-09-18 (bugs.md #525/#526): the test is now the DOS one, read off the
   * unit's real +0x314c/+0x314d/e (`u->orders` / `u->goto_x` / `u->goto_y`,
   * see the field map at the head of the 0a60 section) instead of the
   * `ai_euro_has_useful_goto` stand-in the retired 0a60 shadow forced. Every
   * course in this port already goes through `ai_euro_set_goto`, so the real
   * bytes are stamped everywhere — including by 0a60's goal-consumption tail,
   * which used to write only the mirror.
   */
  /*
   * `act_state ∈ {0, 10, 5, 6}`, and `0x0b standing on +0x314d/e`, all read
   * here as "no live course" — which is exactly `ai_euro_has_useful_goto`
   * now that every course, 0a60's goal commit included, stamps the real
   * +0x314c/+0x314d/e. Divergence, deliberate and one-way: DOS also sends
   * act_state 1/2/3 to the tail, but its 1 means "aboard ship / off-map"
   * (state this port keeps in `aboard_ship_id`) while the port's orders 1 is
   * SENTRY, a parked ON-MAP unit that must still free-score.
   */
  /*
   * DOS-LITERAL FUN_521d_20e6 raw 90183-90206 (dup raw 85345-85368, overlay
   * body viceroy_overlays.c:80720-80761) — the ONLY AI pioneer order DOS
   * writes. It sits immediately ahead of the pre-LAB_4d2e gate below:
   *
   *   if (unit[+0x3146] == 2 && local_6a == 0) {          // Pioneers, not an
   *     local_a = 1;                                      //   explorer pick
   *     if (-1 < village_idx) {                           // raw 90186
   *       FUN_281f_0a4c(village_idx);                     //   bind DS:0x8d52
   *       local_7c = FUN_281f_0a56(DS:0x8d52);            //   tech-tier radius
   *       if (village_dist <= local_7c && alarm_q < 3) local_a = 0;
   *     }
   *     if (-1 < colony_idx) {                            // raw 90193
   *       FUN_281f_09e6(colony_idx);
   *       if (colony[+0x1a] != nation && colony_dist < 3) local_a = 0;
   *     }
   *     if (local_a) { unit[+0x314c] = 9; unit[+0x314b] = 0x52;
   *                    goto LAB_521d_5a78; }
   *   }
   *
   * No tile-quality test, no colony ring, no tools test, no plow-before-road
   * rule — those were the invented planner (bugs.md #610).
   *
   * Who ticks an order-9 AI unit afterwards? **Nobody.** The only callers of
   * the real work bodies FUN_479b_01a6 / FUN_479b_0526 in the whole image are
   * the human ORDERS UI (raw 42512 / 42603, FUN_2b5a_123e / _1454) and the two
   * AI **colony-tick** phantom-unit sites (raw 93669 and raw 94520/94534). An
   * AI unit that takes order 9 also fails 20e6's own entry gate on every later
   * call (`act_state != 0/5/6 && act_state < 10 -> LAB_5a78`, raw 89327 in the
   * prologue), so the write is a *park*, not a work order. The tiles actually
   * get improved by FUN_5952_035e (ai_euro_5952_improve_best_plot, #612).
   * Ported literally anyway: it is what takes the unit out of the wander
   * scorer, which is visible behaviour.
   */
  /*
   * The 20e6 prologue and the iStack_6a explorer flag are DOS's, computed
   * ONCE per 20e6 call: ai_euro_20e6_explorer_flag has a side effect (the
   * −0x5ec4 per-continent explorer counter), so it must not be run twice.
   */
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  ai_euro_20e6_explorer_flag(ctx, u, &s);
  if (!landed_settle && !force_wander && s.dos_type == 2 && s.explorer == 0) {
    {
      {
      int allow = 1; /* local_a */
      if (s.village_idx >= 0 && ctx->col1_ok && ctx->col1 && ctx->col1->tribe &&
          s.village_idx < (int)ctx->col1->head.tribe_count) {
        const ColonizeCol1Tribe* vt = &ctx->col1->tribe[s.village_idx];
        const int ind = (int)vt->nation_id - 4;
        if (ind >= 0 && ind < (int)COLONIZE_COL1_INDIAN_COUNT) {
          /* FUN_281f_0a56 -> FUN_15dc_006a: tech 0/1 -> 1, 2 -> 2, else 3. */
          const unsigned tech = (unsigned)ctx->col1->indian[ind].tech;
          const int radius = (tech <= 1u) ? 1 : (tech == 2u ? 2 : 3);
          const int alarm = (nation_id >= 0 && nation_id < 4)
                              ? (int)ctx->col1->indian[ind].alarm_by_player[nation_id]
                              : 0;
          /* FUN_281f_0a60 -> FUN_15dc_00a2 quartile: <25 0, <50 1, <75 2, else 3. */
          if (s.village_dist <= radius && ai_relation_quartile(alarm) < 3) {
            allow = 0;
          }
        }
      }
      {
        int any_dist = 0;
        const int any_id = ai_euro_20e6_nearest_colony(ctx, u->x, u->y, -1, -1, &any_dist);
        if (any_id >= 0) {
          const ColonizeColony* ac = colonies_get(ctx->colonies, any_id);
          if (ac && ac->nation_id != nation_id && any_dist < 3) {
            allow = 0;
          }
        }
      }
      if (allow) {
        u->orders = UNITS_ORDER_BUILD_ROAD; /* +0x314c = 9 */
        u->col1_ai_plan = 0x52;             /* +0x314b = 'R' */
        /* `goto LAB_521d_5a78`: 20e6 is done with this unit — 1 makes
         * ai_euro_unit_act return instead of running the goal ladder, which
         * would immediately re-stamp an AI_MOVE over the order. */
        return 1;
      }
      }
    }
  }
  int to_4d2e = force_wander || !ai_euro_has_useful_goto(u, ctx->map) ||
                ai_euro_20e6_adjacent_foreign_09dc(ctx, u->x, u->y, nation_id);
  if (landed_settle) {
    to_4d2e = 0; /* FUN_521d_06ae settle-where-landed commits its own course */
    gx = fx;
    gy = fy;
  } else if (!to_4d2e) {
    /*
     * `goto LAB_521d_5a78` — 20e6 sets NO course here. DOS leaves the unit
     * bound to the goal already stored in +0x314d/e and FUN_521d_5b66's
     * switch (case 0x0b / 0x0c → FUN_479b_0972) walks one pathfinder step
     * toward it, keeping the binding until the unit stands on the tile.
     * `ai_euro_score_move` below is this port's stand-in for that walk, so
     * the only thing to do is aim it at the unit's OWN goal.
     *
     * A `ai_goals_best_found_tile_near` re-derive stood here until
     * 2026-09-18 (bugs.md #526) and re-aimed every already-bound land unit
     * at the nearest top-priority FOUND tile on every act, so no unit ever
     * finished a walk. It has no DOS counterpart at all: FOUND (code 1) is a
     * ship-only capability bit (bugs.md #511) and the only writer of a unit's
     * goal is FUN_521d_0a60's consumption tail.
     */
    gx = u->goto_x;
    gy = u->goto_y;
    if (gx < 0 || gy < 0 || gx >= UNITS_GOTO_NONE || gy >= UNITS_GOTO_NONE ||
        !map_coords_inset(ctx->map, gx, gy)) {
      return 0; /* stale/absent target: nothing to walk, LAB_5a78 tail only */
    }
  } else {
    /*
     * FUN_521d_20e6 land arms, in DOS order: SCOUT/PATROL (LAB_277a) →
     * explorer ring scan (LAB_2912, explorers only) → 8-direction wander
     * scorer (LAB_4d2e) committing one adjacent tile as a one-shot goto
     * (epilogue LAB_589e: unit+0x314c=0xc, +0x314d/e = next tile) or
     * staying (dir 8 → +0x314c=5, re-evaluate next call).
     */
    /*
     * raw 88612 `goto LAB_521d_4d2e`: the own-colony surplus branch enters the
     * scorer directly, skipping LAB_277a and every arm hanging off it.
     */
    if (!force_wander && ai_euro_20e6_patrol_arm(ctx, u, &s)) {
      return 0;
    }
    /*
     * LAB_521d_2912 entry, raw 89078-89082 (asm viceroy_overlays.asm:135003-
     * 135020, byte-checked): the explore-goal read `0116(nation, x, y, 6)`
     * then, for explorers only, `001c(nation, 6, x, y, radius = 0)` — the
     * unit clears the *secondary* explore goal standing on the tile it has
     * just reached. The read lives inside the ring scan below (it is only
     * consumed there); this is the write half.
     *
     * Faithful but inert, in DOS as here: nothing ever puts code 6 into the
     * secondary table (the sole `0214` code-6 writer is unreachable — see
     * ai_euro_land_explore_scan_target's tail), so the scan finds no match.
     * Kept because it is a real, reachable DOS call at this exact position.
     */
    if (s.explorer) {
      ai_goals_invalidate_nearby_secondary(nation_id, AI_GOAL_EXPLORE, u->x, u->y, 0);
    }
    /* DOS order: LAB_277a fall-through → 0x4c village arms → 2912 ring →
     * colonist labor loop (raw runs it inside the ring do-loop; same effect
     * here since the ring only fires for explorers). A consumed unit (village
     * entry, colony join, Pioneer convert) aborts the act — it may no longer
     * exist; a labor walk (goto set) lets the act loop move it this turn. */
    if (!force_wander && ai_euro_20e6_village_arm(ctx, u, &s)) {
      return 1;
    }
    {
      const int lr = force_wander ? 0 : ai_euro_20e6_labor_arm(ctx, u, &s);
      if (lr == 2) {
        return 1;
      }
      if (lr == 1) {
        return 0;
      }
    }
    /*
     * Raw 1600-1611 hop countdown, DOS shape: +0x3155 != 0 → decrement and
     * SKIP the ring scan (the flow drops straight to the LAB_4b2c hop arm,
     * which re-commits the latched slot's target); +0x3155 == 0 → reset the
     * slot latch (+0x3156 = 0xff), run the ring scan, and only a failed scan
     * reaches the hop pick with a fresh roll.
     */
    int hop_scan = 1;
    if (!force_wander && s.explorer && u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
      if (ai_euro_s_20e6_hop_steps[u->id] != 0) {
        ai_euro_s_20e6_hop_steps[u->id]--;
        hop_scan = 0;
        if (ai_euro_20e6_ring_hop(ctx, u, &s)) {
          return 0;
        }
      } else {
        ai_euro_s_20e6_hop_slot[u->id] = 0; /* raw 1602: +0x3156 = 0xff */
      }
    }
    /* Raw 85313-85337, DOS position: after the hop block, before the
     * settlement-step scorer below. */
    if (!force_wander && ai_euro_20e6_surplus_recall_arm(ctx, u, &s)) {
      return 0;
    }
    const int scan_r = (!force_wander && s.explorer && hop_scan)
                         ? ai_euro_land_explore_scan_target(ctx, u, nation_id, s.explorer, &fx, &fy)
                         : 0;
    if (scan_r == 2) {
      /*
       * DOS-LITERAL FUN_521d_20e6 raw 89274 (bugs.md #683): standing on the
       * best site → `+0x314c = 7; goto LAB_521d_5a78`. 20e6 returns 0 here
       * (raw 90445), so FUN_521d_5b66 falls straight through to its own switch
       * and dispatches case 7 (build colony) in the same call — that is the
       * consumer wired at the end of the move-scoring gate in ai_euro_unit_act.
       */
      u->orders = UNITS_ORDER_BUILD_COLONY;
      return 0;
    }
    if (scan_r) {
      gx = fx;
      gy = fy;
      is_roam = 1; /* unit+0x314c==5 idle-roam (explore ring) */
    } else if (!force_wander && s.explorer && hop_scan && ai_euro_20e6_ring_hop(ctx, u, &s)) {
      return 0; /* raw 2416-2458: scan failed, hop 4 tiles out instead */
    } else {
      int wander_attack = 0; /* local_ce */
      int wander_saw_foe = 0; /* local_ea */
      const int dir = ai_euro_20e6_wander_step(ctx, u, &s, &wander_attack, &wander_saw_foe);
      /*
       * LAB_4d2e tail, raw 88983-89031: with `local_ce == 0` (the pick is not
       * an attack) DOS runs the 0x42 / 0x65 / 0x46 park arms before the 0x39
       * fallthrough, and each of them reaches LAB_5899, which forces
       * `local_76 = 8` — the scored direction is dropped and the unit stays.
       * Only the 0x46 arm is ported here; the 0x42 / 0x65 arms (raw
       * 88989-89009) are the pioneer work codes, which this port runs at act
       * level in ai_euro_act_land_roles. `local_8e` (raw 88612) is 0 on every
       * path that reaches this point in the port: it is set only by the DOS
       * own-colony-with-2+-garrison entry at raw 88584-88612, whose own
       * outcome (0x47) never reaches the wander scorer's tail arms.
       * See ai_euro_20e6_border_park_arm.
       */
      /*
       * raw 88982-88984, the first test of the LAB_4d2e tail after
       * `local_ce == 0`: `if (local_8e != 0) { select(local_62);
       * goto LAB_521d_5888; }` — a surplus garrison unit (the raw 88612 entry)
       * that scored no attack this scan falls back into the garrison hold,
       * i.e. colony.labor_shortage--, order_code = 0x47, stay. It also
       * suppresses the 0x46 park arm below (raw 88985 reads local_8e == 0).
       */
      if (!wander_attack && force_wander) {
        const int hcid = ctx->colonies ? colonies_id_at(ctx->colonies, u->x, u->y) : -1;
        ColonizeColony* hc = hcid >= 0 ? colonies_get_mut(ctx->colonies, hcid) : NULL;
        if (hc && hc->labor_shortage > 0) {
          hc->labor_shortage--;
        }
        u->col1_ai_plan = 0x47; /* +0x314b */
        ai_euro_20e6_stay_tail_589e(u); /* LAB_5899 local_76 = 8 -> LAB_589e */
        return 0;
      }
      if (!wander_attack && !force_wander &&
          ai_euro_20e6_border_park_arm(ctx, u, &s, wander_saw_foe)) {
        u->col1_ai_plan = 0x46; /* +0x314b */
        ai_euro_20e6_stay_tail_589e(u); /* LAB_5899 local_76 = 8 -> LAB_589e */
        return 0; /* stay put next to the foreign border colony */
      }
      if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
        ai_euro_s_euro_last_dir[u->id] = (int8_t)dir; /* unit+0x314f, 8 = stay */
      }
      if (dir == 8) {
        ai_euro_20e6_stay_tail_589e(u); /* LAB_589e: +0x314c = 5 (6 when admitted) */
        return 0;
      }
      static const int wdx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int wdy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      const int nx = u->x + wdx[dir];
      const int ny = u->y + wdy[dir];
      if (!map_coords_inset(ctx->map, nx, ny)) {
        return 0;
      }
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, nx, ny);
      if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
        ai_euro_s_euro_roam_wander[u->id] = 1; /* unit+0x314c==5 idle-roam (wander step) */
      }
      return 0;
    }
  }
  int dx = 0;
  int dy = 0;
  if (!ai_euro_score_move(ctx, u, gx, gy, &dx, &dy)) {
    return 1;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, gx, gy);
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
    ai_euro_s_euro_roam_wander[u->id] = (uint8_t)is_roam;
  }
  return 0;
}

/*
 * FUN_4720_049e territorial notify (thin, approximate — 2026-08-15 find,
 * likely `@VIOLATE` trigger, "Zero code refs; trigger function not found"
 * per `popups.md` until now). DOS: land unit ends its move adjacent to a
 * foreign Euro unit whose nation it has met (both directions,
 * `FUN_1000_8c28 & 0x40` — corrected from an earlier "PEACE flag" misread,
 * this is `AI_DIPLO_MET`) and not at war, fires a dialog naming both
 * nations. GAME.TXT: "{%STRING0} violate {%STRING1} territory near
 * {%STRING2}! Colonists are outraged!"
 *
 * Approximated, not byte-exact — see euro_unit_act.md for the full trace:
 * - Exact catalog id (0x13cb vs 0x13d7) and violator/owner slot order were
 *   never confirmed; assumed here the acting (moving) unit's nation is the
 *   violator (%STRING0) and the encountered unit's nation is the owner
 *   (%STRING1) — the plain-English reading of the tag name.
 * - %STRING2 (place) has no explicit arg-set call in the DOS body found
 *   (likely auto-filled by the dialog engine from context); approximated
 *   here as the nearest colony's name.
 * - DOS's exact ship/land-terrain gate and re-fire suppression weren't
 *   fully traced; approximated with a land-unit-only trigger and a flat
 *   per-unit cooldown (own addition, not DOS-derived) to avoid repeat
 *   spam for units that just sit adjacent to each other.
 */
void ai_euro_try_violate_notify(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !u || !u->active ||
      units_is_sea(ctx->units, u->id) || u->nation_id < 0 || u->nation_id >= 4) {
    return;
  }
  if (!ctx->messages || !ctx->status || ctx->status_size <= 0) {
    return; /* structural notify only, matches @SNEAK precedent */
  }
  const uint32_t turn = (ctx->turn_number && *ctx->turn_number) ? *ctx->turn_number : 0;
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX && ai_euro_s_violate_last_turn[u->id] != 0 &&
      turn >= ai_euro_s_violate_last_turn[u->id] && turn - ai_euro_s_violate_last_turn[u->id] < 10) {
    return;
  }
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe < 0 || units_is_sea(ctx->units, foe)) {
      continue;
    }
    const ColonizeUnit* f = units_get_const(ctx->units, foe);
    if (!f || f->nation_id == u->nation_id || f->nation_id < 0 || f->nation_id >= 4) {
      continue;
    }
    if (!(ai_diplo_read(ctx->col1, u->nation_id, f->nation_id) & AI_DIPLO_MET) ||
        !(ai_diplo_read(ctx->col1, f->nation_id, u->nation_id) & AI_DIPLO_MET)) {
      continue;
    }
    if (ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
      continue;
    }
    if (ctx->human_nation < 0 || ctx->human_nation >= 4 ||
        (u->nation_id != ctx->human_nation && f->nation_id != ctx->human_nation)) {
      continue; /* structural notify only fires with a human party */
    }
    const char* place = NULL;
    int best_d = -1;
    if (ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active) {
          continue;
        }
        const int dd = abs(c->x - u->x) + abs(c->y - u->y);
        if (best_d < 0 || dd < best_d) {
          best_d = dd;
          place = c->name;
        }
      }
    }
    PopupMsgTokens tok = {0};
    tok.string0 = ai_diplo_rival_name(ctx->col1, u->nation_id);
    tok.string1 = ai_diplo_rival_name(ctx->col1, f->nation_id);
    tok.string2 = (place && place[0]) ? place : "the frontier";
    popup_msg_fill(
      ctx->messages, "VIOLATE", &tok,
      "",
      ctx->status, ctx->status_size
    );
    popup_msg_strip_markup(ctx->status); /* status line: no {} coloring */
    if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
      ai_euro_s_violate_last_turn[u->id] = turn ? turn : 1;
    }
    return;
  }
}

void ai_euro_try_attack(ColonizeTurnContext* ctx, ColonizeUnit* u, int tx, int ty) {
  if (!ctx || !ctx->units || !u) {
    return;
  }
  /*
   * @UNIT attack 0 means the unit cannot attack at all -- Pioneers, Colonists,
   * Wagon Trains, unarmed transports. Without this gate a settler walking a
   * FOUND goto straight at a village fought the Braves standing on it and died,
   * which is how AI nations kept losing their founder before ever founding.
   */
  {
    const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
    if (t && t->attack <= 0) {
      return;
    }
  }
  const int foe = units_best_defender_at(
    ctx->units, ctx->col1_ok ? ctx->col1 : NULL, tx, ty, u->id, u->id
  );
  if (foe < 0) {
    return;
  }
  const ColonizeUnit* f = units_get_const(ctx->units, foe);
  if (!f || f->nation_id == u->nation_id) {
    return;
  }
  if (ctx->col1_ok && ctx->col1 && f->nation_id >= 0 && f->nation_id < 4) {
    if (!ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
      /*
       * bugs.md #472: the signed-treaty bit 0x40 gates every Euro target
       * (same DOS rule as the LAB_4d2e attack term, smell #106). The
       * goal / goto / peace-border-garrison callers reach here without that
       * check and attacked treaty partners. Privateers fly no flag.
       */
      if ((ai_diplo_read(ctx->col1, u->nation_id, f->nation_id) & AI_DIPLO_PEACE) != 0 &&
          !units_type_is_privateer(units_type(ctx->units, u->type_index))) {
        return;
      }
      /*
       * @SNEAK ("Sneak attack by the treacherous {attacker}!") — confirmed
       * real, 2026-08-14, via live user testimony (euro_diplo.md "FA
       * negotiation screen"): AI Euro nations can attack outright, with
       * war declared as a SIDE EFFECT of the attack rather than a
       * prerequisite for it — exactly this code path (war-declare gated
       * on the attack itself, not the other way around). The mechanic was
       * already correctly implemented; only the player-facing
       * notification was missing (declare was via the bare, status-free
       * ai_diplo_declare_war). Switched to the _ctx variant (gains the
       * existing boycott/sticky chrome for free) and override its generic
       * @DECLAREWAR status with the real @SNEAK wording when the human is
       * a party, since this specific path is never a negotiated/expected
       * declaration.
       */
      ai_diplo_declare_war_ctx(ctx, u->nation_id, f->nation_id);
      /* Declare refused (Franklin no-op): no warless attack. */
      if (!ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id) &&
          !units_type_is_privateer(units_type(ctx->units, u->type_index))) {
        return;
      }
      if (ctx->human_nation >= 0 && ctx->human_nation < 4 &&
          (u->nation_id == ctx->human_nation || f->nation_id == ctx->human_nation) &&
          ctx->status && ctx->status_size > 0) {
        PopupMsgTokens tok = {0};
        tok.string0 = ai_diplo_rival_name(ctx->col1, u->nation_id);
        popup_msg_fill(
          ctx->messages, "SNEAK", &tok, "",
          ctx->status, ctx->status_size
        );
        popup_msg_strip_markup(ctx->status); /* status line: no {} coloring */
      }
    }
  }
  if (units_is_sea(ctx->units, u->id)) {
    units_resolve_naval_combat(ctx->units, u->id, foe, ctx->rng);
    /* Land combat spends MP via try_move into the tile; ships cannot enter
     * foe tiles — spend remaining MP after naval resolve (structural). */
    if (u->active) {
      u->moves = 0;
    }
  } else if (units_resolve_land_combat(ctx->units, u->id, foe, ctx->rng)) {
    {
      ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
      units_try_move_w(&w_, u->id, tx, ty);
    }
  }
  if (u->active && ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    if (cid >= 0) {
      ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
      if (c && c->nation_id != u->nation_id && c->nation_id >= 0 && c->nation_id < 4 &&
          units_foreign_unit_at(ctx->units, u->x, u->y, u->id, u->nation_id) < 0) {
        int plunder = 0;
        for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
          if (c->stock[i] > 0) {
            plunder += c->stock[i];
          }
        }
        ColonizeColony snap = *c;
        if (colonies_capture(ctx->colonies, cid, u->nation_id)) {
          units_combat_notify_colony_captured(
            ctx->col1_ok ? ctx->col1 : NULL, &snap, u->nation_id, plunder
          );
        }
      }
    }
  }
}

/* Water tile adjacent to a coastal colony (ships cannot enter foreign land). */
int ai_euro_coastal_water_near(
  const ColonizeWorldMap* map,
  int cx,
  int cy,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y || !map_tile_is_coastal(map, cx, cy)) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = cx + MAP_DIR8_DX[d];
    const int ny = cy + MAP_DIR8_DY[d];
    if (!map_tile_is_water(map, nx, ny)) {
      continue;
    }
    const int dist = abs(nx - from_x) + abs(ny - from_y);
    if (best < 0 || dist < best) {
      best = dist;
      bx = nx;
      by = ny;
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/* Caravel / Merchantman / Galleon — New-World cargo haul (manual trade ships). */
int ai_euro_is_cargo_ship_name(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_CARAVEL || kind == UNITS_KIND_MERCHANTMAN || kind == UNITS_KIND_GALLEON;
}
