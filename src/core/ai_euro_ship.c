/*
 * Euro AI — ship band: delivery/load matrices, berth arrival, trade haul + Europe export, unload/settle, first-colony landing
 *
 * Split out of ai_euro.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_euro_internal.h. See ai_euro.c for the dispatcher entry point.
 *
 * Citation units (bugs.md #816): `raw NNNNN` = a line in
 * original_sources_decompiled/viceroy_unpacked.c (docs/conventions.md).
 * The FUN_521d_20e6 cites in this file are `md:NNNN` = a line in
 * original_sources_annotated/ai/move_scoring_20e6_full.md, a different
 * numbering — never mix the two.
 *
 * Sections:
 *   20e6 hold-cargo delivery matrix (tallies / colony pick / sell tail)
 *   20e6 load pick, wagon village errand, board-mark cleanup, transport assemble
 *   20e6 ship berth arrival + trade haul + Europe export/entry
 *   Fort-fire flee, adjacent colony/village seizure, goal fold
 *   20e6 unload masks and colony sail pick
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
#include "core/ai_internal.h"
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
 * `ai_euro_ocean_colony_sail_score` — the thin second port of LAB_521d_3558
 * (~89614-89711) — lived here until smell audit sweep-3 area C #6 retired it:
 * every term was invented (`pop*8` where DOS scores `((0x11-pop)^2+2)*4`, i.e.
 * the opposite sign of preference; `-d*4` for DOS's `-((d>>1)+1)`; `idle*8` for
 * a signed `+idle`; a Stockade/Fort/Fortress ladder DOS does not have; a "docks
 * flag 0x1b&0x10" that is really NEEDS_COLONISTS), and it had no rng draw,
 * wanted-size term, human-presence term or commit threshold. The one DOS site
 * it answered is ported structurally by `ai_euro_20e6_colony_sail_pick` (raw
 * 89663 ladder), reached from `ai_euro_act_ship_dos` — DOS enters 3558 once
 * per act, from the 20e6 unload/settle flow, and now so does this port.
 */

int ai_euro_20e6_unit_col5(const ColonizeUnitPool* pool, int dos_type);

/*
 * FUN_521d_20e6 hold-cargo colony-delivery matrix (md:2047-2139;
 * viceroy_overlays.asm 137180-137466, labels 004026/004038/0040a1/0041e5).
 *
 * DOS unit byte +0x314a latches the colony a hauler last LOADED at (written in
 * the load matrix, md:2147) and the delivery loop skips it. Modelled by the
 * real persistent `col1_origin` byte (ai_euro_20e6_origin_get/_set).
 */

/*
 * FUN_521d_20e6 md:1691 gate + md:2996-3017 dump sweep: a SHIP standing at
 * an own colony empties every hold into that colony before anything else the
 * function does. See ai_euro_20e6_ship_berth_arrival for the full decode.
 * Kill switch / bisect: AI_20E6_SHIP_DUMP=0 restores the 06e empty-hull gate
 * substitution. Trace: AI_20E6_SHIP_DUMP_TRACE=1.
 */
#ifndef AI_20E6_SHIP_DUMP_DEFAULT
#define AI_20E6_SHIP_DUMP_DEFAULT 1
#endif

static int ai_euro_20e6_ship_dump_enabled(void) {
  return ai_euro_env_flag("AI_20E6_SHIP_DUMP", AI_20E6_SHIP_DUMP_DEFAULT);
}

/*
 * acStack_c8 per-cargo tallies (md:1702-1712). DOS walks the ship's occupied
 * holds, keeps only cargo types 0x0d..0x0f (Trade Goods / Tools / Muskets) and
 * 0x08 (Horses) — the goods a colony consumes — sums their amounts per type,
 * and records the highest such type in iStack_44 (−1 = none, which switches
 * the whole delivery block off). Returns that max type.
 *
 * Ghidra sizes acStack_c8 as char[8]; the memset is 0x10 wide and the asm
 * indexes it as [BP+SI-0xc6] for SI = 0..0xf, so the array is 16 wide and the
 * separately-declared `cStack_c0` (asm [BP-0xbe]) is element 8 — the Horses
 * tally used by the warehouse gate below. (Ghidra's Stack_XX names run two
 * bytes below the real BP displacement throughout this frame: uStack_50 is
 * [BP-0x4e], iStack_44 is [BP-0x42], uStack_e6 is [BP-0xe4].)
 */
static int ai_euro_20e6_delivery_tallies(
  const ColonizeTurnContext* ctx, const ColonizeUnit* ship, int tally[COLONIZE_CARGO_COUNT]
) {
  int max_type = -1;
  for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
    tally[g] = 0;
  }
  if (!ctx || !ctx->units || !ship) {
    return -1;
  }
  const int n = units_goods_hold_count(ctx->units, ship->id);
  for (int h = 0; h < n && h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    const int amt = units_hold_amount(ctx->units, ship->id, h);
    if (amt <= 0) {
      continue;
    }
    const int ct = ship->hold_goods_type[h];
    if (ct < 0 || ct >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    if (ct <= 0x0c && ct != COLONIZE_CARGO_HORSES) {
      continue; /* md:1706: 0xc < type || type == 8 */
    }
    if (ct > max_type) {
      max_type = ct;
    }
    tally[ct] += amt;
    if (tally[ct] > 127) {
      tally[ct] = 127; /* acStack_c8 is a signed char accumulator */
    }
  }
  return max_type;
}


/*
 * The matrix proper (md:2047-2139). Candidate = own colony, coastal
 * (+0x1c bit 0x40), not the ship's own nearest colony when it is standing on
 * it (uStack_62 / iStack_2e), not the colony the ship last loaded at
 * (+0x314a), and — when Horses are the only delivery cargo (iStack_44 == 8) —
 * one whose warehouse can still take them.
 *
 * Per carried cargo g with a nonzero tally (iStack_22 = warehouse capacity,
 * FUN_1000_8f2a = 100*(1+warehouse_level)):
 *   colony already PRODUCES g (+0x90 cargo_produced_mask) and stock[g] > 99
 *       → colony rejected outright (iStack_a = 0, break)
 *   cap <= tally[g] + stock[g]  (the drop would overflow)
 *       → score += (cap − stock[g] − tally[g]) * euro_price[nation][g] * 4
 *         (negative: the overflow is priced at the DS:0x84bc = −0x7b44 per
 *         nation × cargo byte table, col1->nation[n].trade.euro_price[])
 *   colony specialty (+0x8d) == g → score += (idle_turns(+0x8f) + 8) * 4
 *   score += (cap − stock[g]) − 1                      (room for g)
 * Muskets aboard (iStack_44 == 0xf) adds a threat term:
 *   +0x10 when the human holds colonies on this continent
 *        (−0x6b1a = colony_counts_by_continent[human][cid], DS:0x94e6)
 *   over the 20-tile ring (DS:0xc8/0xde = ai_euro_k_20e6_ring20_dx/dy): in-bounds
 *        tile with presence < 4 → +0x18; Indian-held tile →
 *        quartile(alarm_by_player[tribe][nation]) * 0x10
 * Shared tail: +0x1b bit 0x02 (NEARBY_FRIGATE) and cargo-ship type < 0x10 →
 *   (col5 − 10) * 8; else bit 0x01 (NEARBY_ARMED_SHIP) → (col5 − 10) * 2.
 * Then score /= ((FUN_1000_856a dist >> 2) + 1) and later-ties-win against a
 * best seeded to −1, so a colony must score ≥ −1 to be picked at all
 * (md:2128; asm CMP/JL at 0041e5+0x1a).
 *
 * Substitutions: the coastal bit is OR'd with a live map_tile_is_coastal probe
 * as belt-and-braces. Since 2026-09-10 the bit itself is DOS-shaped — stamped
 * once by colonies_found from map_tile_is_open_sea_adjacent, self-healed
 * set-only by ai_euro_refresh_colony_ai_flags — so the OR only matters for
 * fixtures that build colony records by hand; colonies_warehouse_capacity is now
 * cargo-independent like DOS 8f2a (the port's uncited FOOD-199 branch went
 * with smell audit #25), so the cargo argument below is cosmetic — FOOD is
 * never a delivery cargo here anyway. Nothing invented.
 */
static int ai_euro_20e6_delivery_colony_pick(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* ship,
  int nation,
  const int tally[COLONIZE_CARGO_COUNT],
  int max_type,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->colonies || !ctx->map || !ship || nation < 0 || nation > 3 ||
      max_type < 0) {
    return 0;
  }
  const int ship_type = ai_euro_20e6_dos_type(ctx->units, ship);
  /* uStack_62 / iStack_2e: nearest own colony and its distance. */
  int home_dist = 9999;
  const int home_colony = ai_euro_20e6_nearest_colony(ctx, ship->x, ship->y, nation, -1, &home_dist);
  const int loaded_at = ai_euro_20e6_origin_get(ship);
  int best = -1; /* iStack_e2 seed */
  int pick = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation) {
      continue;
    }
    if (c->id == home_colony && home_dist == 0) {
      continue; /* md:2053 */
    }
    if ((c->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) == 0 &&
        !map_tile_is_coastal(ctx->map, c->x, c->y)) {
      continue; /* md:2054: +0x1c bit 0x40 */
    }
    if (c->id == loaded_at) {
      continue; /* md:2055: unit +0x314a */
    }
    const int cap = colonies_warehouse_capacity(ctx->colonies, c, COLONIZE_CARGO_TOOLS);
    if (max_type == COLONIZE_CARGO_HORSES &&
        tally[COLONIZE_CARGO_HORSES] + (int)c->stock[COLONIZE_CARGO_HORSES] > cap) {
      continue; /* md:2056-2058 */
    }
    const int cid = map_continent_id_at(ctx->map, c->x, c->y);
    int score = 0;
    int ok = 1;
    for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
      if (tally[g] == 0) {
        continue;
      }
      const int stock = (int)c->stock[g];
      if ((c->cargo_produced_mask & (uint16_t)(1u << g)) != 0 && stock > 99) {
        ok = 0;
        break; /* md:2067-2070 */
      }
      if (cap <= tally[g] + stock) {
        /* DS:0x84bc (= −0x7b44) byte table, indexed nation*0x10 + cargo. */
        const int price = (ctx->col1_ok && ctx->col1)
                            ? (int)ctx->col1->nation[nation].trade.euro_price[g]
                            : 0;
        score += (cap - stock - tally[g]) * price * 4; /* md:2072-2077 */
      }
      if ((int)c->specialty_cargo == g) {
        score += ((int)(int8_t)c->cargo_idle_turns + 8) * 4; /* md:2080-2081 */
      }
      score += (cap - stock) - 1; /* md:2083-2084 */
    }
    if (!ok) {
      continue;
    }
    if (max_type == COLONIZE_CARGO_MUSKETS) {
      if (cid >= 0 && ai_euro_20e6_own_colonies_on(ctx, ctx->human_nation, cid) != 0) {
        score += 0x10; /* md:2089-2090 */
      }
      for (int r = 0; r < 20; ++r) {
        const int tx = c->x + (int)ai_euro_k_20e6_ring20_dx[r];
        const int ty = c->y + (int)ai_euro_k_20e6_ring20_dy[r];
        if (tx < 0 || ty < 0 || tx >= (int)ctx->map->width || ty >= (int)ctx->map->height) {
          continue; /* FUN_1000_84f2 map_tile_in_bounds */
        }
        const int pres = ai_euro_20e6_tribe_or_presence(ctx, tx, ty);
        if (pres < 4) {
          score += 0x18; /* md:2100-2101 */
          continue;
        }
        int alarm = 0;
        if (ctx->col1_ok && ctx->col1 && (pres - 4) < (int)COLONIZE_COL1_INDIAN_COUNT) {
          alarm = (int)ctx->col1->indian[pres - 4].alarm_by_player[nation];
        }
        score += ai_relation_quartile(alarm) * 0x10; /* md:2103-2106 */
      }
    }
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0) {
      if (ship_type < 0x10) {
        score += (ai_euro_20e6_unit_col5(ctx->units, ship_type) - 10) * 8; /* md:2119-2121 */
      }
    } else if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP) != 0) {
      if (ship_type < 0x10) {
        score += (ai_euro_20e6_unit_col5(ctx->units, ship_type) - 10) * 2; /* md:2113-2115 */
      }
    }
    const int dist = map_dos_dist(ship->x - c->x, ship->y - c->y);
    score = score / ((dist >> 2) + 1); /* md:2126-2127 */
    if (score >= best) {               /* DOS later-ties-win, best seeded −1 */
      best = score;
      pick = i;
    }
  }
  if (pick < 0) {
    return 0;
  }
  if (getenv("AI_20E6_DELIVER_TRACE")) {
    fprintf(
      stderr,
      "[deliver] ship %d n%d max=%d -> colony %d (%d,%d) score %d\n",
      ship->id,
      nation,
      max_type,
      ctx->colonies->colonies[pick].id,
      (int)ctx->colonies->colonies[pick].x,
      (int)ctx->colonies->colonies[pick].y,
      best
    );
  }
  *out_x = (int)ctx->colonies->colonies[pick].x;
  *out_y = (int)ctx->colonies->colonies[pick].y;
  return 1;
}

/*
 * FUN_521d_20e6 delivery SELL TAIL (md:2140-2163 = doc 2147-2170, the
 * `while (*(char *)(param_2 * 0x1c + 0x3150) != '\0')` loop right after the
 * matrix's `if (-1 < uStack_24)` commit). When the matrix picked no colony
 * DOS dumps the whole hold for gold rather than leaving the hull loaded:
 *
 *   while (unit->holds_occupied != 0) {
 *     g   = FUN_1000_8cdc(unit, 0);          // remove hold slot 0, compact
 *     qty = DS:0x8dc4;                        //   ... and stash its amount
 *     func_0x00019c1e(g, qty);                // = FUN_291f_0a2e -> 38fd_1dfa
 *     v = euro_price[nation][g] * qty;        // DS:0x84bc byte table
 *     nation->gold(+0x2a)          += v;      // 32-bit, with carry
 *     nation->trade.gold(+0x7c)[g] += v;      // 32-bit, with carry
 *     nation->trade.tons(+0xbc)[g] += qty;    // 32-bit, with carry
 *   }
 *   unit->holds_occupied = 0;
 *
 * Resolved symbols (nothing invented):
 *   FUN_1000_8cdc   -> FUN_281f_0aec -> FUN_15eb_317c "remove unit cargo /
 *                      passenger slot; compact; stash qty in 0x8dc4"
 *                      (address_mapping.csv:991; FUNCTION_CATALOG.md:440/1455)
 *   func_0x00019c1e -> FUN_291f_0a2e -> FUN_38fd_1dfa "sell volume: ledgers +
 *                      tax gold" (address_mapping.csv:1274;
 *                      FUNCTION_CATALOG.md:1743/2353) — already ported exactly
 *                      as europe_apply_trade_volume(..., is_buy=0,
 *                      immediate_threshold=0), the same call shape
 *                      europe_ai_colony_dump_sell uses.
 *   *(int *)0x84fc  -> the BOUND NATION RECORD pointer (not FUN_1000_84fc):
 *                      +0x2a gold, +0x7c trade.gold[16], +0xbc trade.tons[16],
 *                      +0xfc trade.tons2[16] — byte-checked against
 *                      viceroy_unpacked.c:60247 (FUN_38fd_1dfa) and against
 *                      offsetof() on ColonizeCol1Nation (42/124/188/252).
 *
 * DOS quirk kept verbatim: 1dfa has ALREADY credited trade.gold[g] with the
 * tax-adjusted proceeds and trade.tons[g]/tons2[g] with qty; this tail then
 * adds the UNtaxed euro_price*qty to trade.gold[g] and qty to trade.tons[g] a
 * second time. Only the +0x2a treasury credit is single, and it is untaxed —
 * the AI dump-sell pays no Crown cut on this path. Ported as written; the
 * double ledger entry is DOS behaviour, not a port bug.
 *
 * Substitution: DOS's `holds_occupied` walk with slot-0 compaction is a plain
 * sweep over the port's occupied goods holds (same set, same order).
 */
static int ai_euro_20e6_delivery_sell_tail(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  int nation
) {
  if (!ctx || !ctx->units || !ship || nation < 0 || nation > 3 || !ctx->col1_ok ||
      !ctx->col1) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation];
  const int n = units_goods_hold_count(ctx->units, ship->id);
  int total = 0;
  for (int h = 0; h < n && h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    const int qty = units_hold_amount(ctx->units, ship->id, h);
    if (qty <= 0) {
      continue;
    }
    const int g = ship->hold_goods_type[h];
    if (g < 0 || g >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    /* FUN_1000_8cdc: the slot is emptied before the ledgers run. */
    ship->hold_goods_amount[h] = 0;
    ship->hold_goods_type[h] = 0;
    /* func_0x00019c1e = 38fd_1dfa. Needs the shared EuropeScreen; when the AI
     * runs headless without one the price/volume ledger is simply skipped —
     * the treasury half below is col1-only and always applies. */
    if (ctx->europe) {
      europe_apply_trade_volume(
        ctx->europe, ctx->col1, nation, ctx->human_nation, g, qty, 0, 0
      );
    }
    const int32_t v = (int32_t)nat->trade.euro_price[g] * (int32_t)qty;
    nat->gold += (uint32_t)v;
    nat->trade.gold[g] += v;
    nat->trade.tons[g] += (int32_t)qty;
    total += (int)v;
  }
  if (total != 0 && getenv("AI_20E6_DELIVER_TRACE")) {
    fprintf(
      stderr, "[deliver] ship %d n%d no colony -> sold hold for %d\n", ship->id, nation, total
    );
  }
  return total;
}

/*
 * FUN_521d_20e6 LOAD-at-colony matrix (md:3059-3134 of the recovered C:
 * `while ((iStack_d2 != 0 && (bVar17)))` … `LAB_OVL14_L0000__003356`).
 * Picks ONE cargo per free hold; the caller re-runs it until the hull is full
 * or the matrix declines.
 *
 * TRAP (this pass): `iStack_34` is NOT a war/peace flag. Raw 1174-1179 sets
 * it from the unit TYPE alone — `iStack_34 = (0xd <= type && type <= 0x12)`,
 * i.e. 1 = SHIP, 0 = land hauler (Wagon Train). Every `iStack_34` arm below is
 * therefore a ship-vs-wagon split, and the two latches at the bottom of the
 * loop are `+0x314a = colony` for ships (which is exactly what the delivery
 * matrix skips) and `+0x3158 = 1` for wagons (the village-errand byte 47b9
 * reads).
 *
 * The stock term (md:3068-3077), then the per-cargo gates:
 *   term = stock[g]
 *   if (term < cap || g == 0) { if (g == 8) term = max(term - cap + 0x17, 0); }
 *   else                      { term <<= 1; }
 *   g == 5 (LUMBER)                        -> skipped outright
 *   g == 0xe/0xf (TOOLS/MUSKETS)           -> ships only, and only when the
 *        colony PRODUCES it (+0x90 cargo_produced_mask); then term -= 100
 *   ship && (g == 0xd || g == 0)           -> skipped (no Trade Goods / Food)
 *   term == 0                              -> skipped
 *   score(ship)  = euro_price[nation][g] * term
 *   score(wagon) = walk p = euro_price[nation][g] down while p > 1 and
 *                  86c4(0,3) == 0; thr = (g == 0xd) ? 8 : 4;
 *                  p < thr ? term*(thr-p) + (1-p)*5 : -1;  term < 0x32 -> -1
 *   best is seeded -1 with a STRICT `<`, so the first cargo wins ties and a
 *   score of -1 can never be picked.
 * Returns the cargo id, or −1 when the matrix declines (DOS `iStack_d2 = 0`).
 *
 * `FUN_1000_86c4` = FUN_281f_04d4 = dos_rng_range (address_mapping.csv:844);
 * inclusive-range mapping per this file's own 86c4(1,0x14) port above.
 */
int ai_euro_20e6_load_pick(
  ColonizeTurnContext* ctx,
  const ColonizeColony* c,
  int nation,
  int is_ship
) {
  if (!ctx || !ctx->colonies || !c || nation < 0 || nation > 3) {
    return -1;
  }
  /* iStack_a4 = FUN_1000_8f2a() warehouse capacity — cargo-independent, as
   * in DOS: the port's FOOD-199 branch was removed by smell audit #25, so the
   * cargo argument here no longer changes the shared per-cargo term. */
  const int cap = colonies_warehouse_capacity(ctx->colonies, c, COLONIZE_CARGO_TOOLS);
  int best = -1;
  int pick = -1;
  for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
    int term = (int)c->stock[g];
    if (term < cap || g == COLONIZE_CARGO_FOOD) {
      if (g == COLONIZE_CARGO_HORSES) {
        term = (term - cap) + 0x17;
        if (term < 0) {
          term = 0;
        }
      }
    } else {
      term = term << 1;
    }
    if (g == COLONIZE_CARGO_LUMBER) {
      continue; /* md:3081: `if (uStack_b6 != 5)` guards the whole body */
    }
    if (g == COLONIZE_CARGO_TOOLS || g == COLONIZE_CARGO_MUSKETS) {
      if (!is_ship || (c->cargo_produced_mask & (uint16_t)(1u << g)) == 0) {
        continue; /* md:3082-3085 */
      }
      term += -100; /* md:3086 */
    }
    if (is_ship && (g == COLONIZE_CARGO_TRADE_GOODS || g == COLONIZE_CARGO_FOOD)) {
      continue; /* md:3087 */
    }
    if (term == 0) {
      continue; /* md:3087 tail */
    }
    const int price = (ctx->col1_ok && ctx->col1)
                        ? (int)ctx->col1->nation[nation].trade.euro_price[g]
                        : 0;
    int score;
    if (!is_ship) {
      int p = price;
      while (p > 1 && ctx->rng && dos_rng_range(ctx->rng, 0, 3) == 0) {
        p -= 1; /* md:3091-3093 throttle walk-down */
      }
      const int thr = (g == COLONIZE_CARGO_TRADE_GOODS) ? 8 : 4; /* md:3094-3099 */
      if (p < thr) {
        score = term * (thr - p) + (1 - p) * 5; /* md:3101-3102 */
      } else {
        score = -1; /* md:3104 */
      }
      if (term < 0x32) {
        score = -1; /* md:3107 */
      }
    } else {
      score = price * term; /* md:3111 */
    }
    if (best < score) { /* md:3113: strict, first-wins ties */
      best = score;
      pick = g;
    }
  }
  return pick;
}

/*
 * FUN_521d_20e6 wagon village-errand arm (md:2284-2307) + the 4528 AI-arm
 * case-1 consumer. A wagon whose errand latch (+0x3158) is set:
 *   - scan every settlement, keep the nearest on the wagon's own landmass
 *     (FUN_1000_8912 == iStack_38); a capital (record +3 & 4) halves its
 *     distance, min 1; strict `<` — the earlier record wins ties;
 *   - none on this landmass → LAB_47b9 destroy;
 *   - adjacent (or on the tile) → FUN_4d56_4528 AI arm, type 0xc → case 1
 *     Trade → the 2820 shell (ai_contact_ai_wagon_village_trade). 2820
 *     clears the DOS errand byte at entry for land types
 *     (viceroy_unpacked.c:82122) — mirrored by clearing the latch on the
 *     attempt, traded or refused; the 4528 return forfeits the MP;
 *   - else goto the village tile.
 * The port's AI movement never steps ONTO tribe tiles, so "arrival" is
 * adjacency here — the same substitution every other village arm uses.
 * Returns 1 when it owned the wagon this beat (goto set, traded, or
 * destroyed — caller must re-check active).
 */
int ai_euro_20e6_wagon_village_errand(
  ColonizeTurnContext* ctx, int nation_id, ColonizeUnit* wagon
) {
  if (!ctx || !ctx->units || !ctx->map || !wagon || !wagon->active || wagon->id < 0 ||
      wagon->id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  if (!ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || ctx->col1->head.tribe_count == 0) {
    ai_euro_s_20e6_wagon_errand[wagon->id] = 0;
    return 0;
  }
  const int cid = map_continent_id_at(ctx->map, wagon->x, wagon->y);
  int best_i = -1;
  int best_d = 9999; /* DOS iStack_e2 seed */
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if (map_continent_id_at(ctx->map, t->x, t->y) != cid) {
      continue;
    }
    int d = map_dos_dist((int)t->x - wagon->x, (int)t->y - wagon->y);
    if (t->state.capital) {
      d >>= 1;
      if (d < 1) {
        d = 1;
      }
    }
    if (d < best_d) {
      best_d = d;
      best_i = (int)i;
    }
  }
  if (best_i < 0) {
    /* goto LAB_47b9 — dead end, destroy. */
    if (getenv("AI_20E6_DEADEND_TRACE")) {
      fprintf(stderr, "[47b9] errand wagon %d n%d (%d,%d) no village on cid %d -> destroy\n",
              wagon->id, nation_id, wagon->x, wagon->y, cid);
    }
    ai_euro_s_20e6_wagon_errand[wagon->id] = 0;
    (void)units_despawn(ctx->units, wagon->id);
    return 1;
  }
  const ColonizeCol1Tribe* t = &ctx->col1->tribe[best_i];
  const int adj = abs((int)t->x - wagon->x) <= 1 && abs((int)t->y - wagon->y) <= 1;
  if (adj) {
    const int ind = (int)t->nation_id;
    if (ind >= 4 && ind <= 11) {
      /* bugs.md #813: DOS clears +0x3158 inside FUN_4d56_2820 itself (raw
       * 82121-82123: `if (type < 0xd || 0x12 < type) +0x3158 = 0;`), i.e. at
       * trade entry and only when a trade is actually entered — not on bare
       * adjacency. `ai_contact_ai_wagon_village_trade` lives in
       * ai_contact_trade.c, so the clear sits at the call site instead, but
       * under the same gate. */
      ai_euro_s_20e6_wagon_errand[wagon->id] = 0;
      (void)ai_contact_ai_wagon_village_trade(ctx, ind, nation_id, wagon->id);
    }
    wagon->moves = 0; /* 4528 return-code 1 MP forfeit */
    return 1;
  }
  if (units_orders_follow_goto(wagon->orders) && wagon->goto_x == t->x &&
      wagon->goto_y == t->y) {
    return 1;
  }
  ai_euro_set_goto(wagon, UNITS_ORDER_AI_MOVE, (int)t->x, (int)t->y);
  return 1;
}

/*
 * FUN_1427_10be — the DOS transport-chain ASSEMBLY step (resident flat
 * `0000:532e`; 20e6 reaches it through the `FUN_281f_0920` thunk, overlay
 * `FUN_1000_8b10`). Ported 2026-09-07e; this retires the "boarding is
 * collapsed into one beat" substitution note.
 *
 * Raw body (decomp 8606-8686), the part 20e6 exercises:
 *
 *   free = hold_capacity(0x5237[type]) − holds_occupied(+0x3150);
 *   member = stack_head(0002); if (member == ship) member = next(004a);
 *   0362(ship, -2, -2);                     // ship joins the carrier list
 *   while (member >= 0) {
 *     next = 004a();
 *     take = (member+0x314c == 1);          // the act_state-1 board mark
 *     … two force-board arms (see "not ported" below) …
 *     size = 0x5238[member.type];
 *     if (size <= free && take) {
 *       free -= size;
 *       member+0x314c = 1;
 *       0362(member, -2, -2);               // parked at the (−2,−2) sentinel
 *     }
 *     member = next;
 *   }
 *
 * A DOS passenger is a unit parked at sentinel coords (−2,−2) on the shared
 * tile-stack lists, not an entry in a ship-side array. OpenCol keeps passengers
 * in `cargo_ids` (the same substitution `ai_euro_20e6_ship_cargo_counts` and
 * `ai_euro_20e6_stack_settler` make), so the sentinel park is `units_board`
 * and no (−2,−2) coordinate ever reaches the unit pool or the save. Nothing
 * to round-trip: see the note on the mark's lifetime below.
 *
 * TIMING — the mark and the assembly are ONE act, not two (control flow
 * verified in `viceroy_overlays.asm`, not from the decompiler's line order):
 *
 *   LAB_OVL14_L0000__00304c  :135683  arrival gate; three `JMP 0x3558` exits
 *                                     (:135706 / :135710 / :135721)
 *   …0x30a7                  :135722  arrival block = the gate's FALL-THROUGH
 *                                     (stale-mark clear at 0x30b6 :135728,
 *                                     then dump, cower, the md:3024-3051
 *                                     board-MARK scan, load matrix)
 *   …0x32f5                  :135980  after the scan: `[0x1734+n*2] = 0`,
 *                                     `JMP 0x354e`
 *   …0x354e                  :136215  load-matrix loop test → `JZ 0x3558`
 *   LAB_OVL14_L0000__003558  :136221  the band; XREF list names 0x3083,
 *                                     0x308c, 0x30a4, 0x3313, 0x3553
 *   …0x3609                  :136290  `PUSH [BP+6]` / `CALLF FUN_1000_8b10`
 *                                     immediately before the 0d38 batch
 *                                     (`PUSH 2` / `8aac`, `PUSH 3`, …)
 *
 * So the berth scan marks and, later in the SAME 20e6 call, this sweep
 * assembles — Ghidra renders the arrival block after `LAB_3558` in text order
 * only because every path inside the `if` leaves by goto. The budget hand-off
 * is exact because of that order: the scan reserves `iStack_d2` for the
 * passengers, the load matrix fills the rest of the holds with goods, and
 * `free = capacity − holds_occupied` here re-derives precisely the reserved
 * remainder. The cower arm is the one berth path that never reaches this
 * sweep (it stamps orders 0x43 and jumps to LAB_5899, past 0x3558).
 *
 * The mark's lifetime is one nation turn: `ai_euro_0a60_unit_housekeeping`
 * resets act_state 1/2/3 → 0 for every on-map inset unit at the top of the
 * nation turn, exactly as DOS does at :87560-87563. A mark therefore cannot
 * survive a save/load in an observable way and needs no save round-trip.
 *
 * The two force-board arms are ported 2026-09-07f — see the board-predicate
 * comment in the loop below.
 *
 * The recursive `FUN_1427_101c` pre-pass — called from `FUN_1427_10be`
 * (viceroy_unpacked.c raw 8628-8645, re-anchored 2026-09-24; the earlier
 * cite pointed at FUN_1427_101c itself, but that recursive call site is
 * in its caller FUN_1427_10be) — is CLOSED as unreachable from here,
 * 2026-09-08:
 *
 *   - The ONLY writer of `+0x314c = 2` in the whole decomp is
 *     `FUN_2b5a_1e66` (:42863), the human "assign trade route" order:
 *     it bails on `DS:0x53a0 == 0` (trade_route_count) with popup 0xa2d,
 *     picks a route with `FUN_291f_02dc` (= `FUN_647e_0796`, the route
 *     picker) and finishes in `FUN_291f_02b2` (= `FUN_479b_0bd0`, the
 *     goto-colony/route body). `FUN_1427_12c6` (stamp act_state on a whole
 *     stack) is the only indirect writer and its one caller (:75718, through
 *     thunk `FUN_281f_08f8`) passes 1.
 *   - No AI path assigns trade routes, and `ai_euro_0a60_unit_housekeeping`
 *     clears act_state 1/2/3 → 0 for every on-map unit at the nation-turn top
 *     (DOS :87558-87562) — so even a captured or loaded unit reaches 20e6
 *     with act_state 0.
 *   - What it does, for the record: 101c reorders the tile stack (`04d6`),
 *     then repeatedly 10be's each ship still on the tile (each call parks that
 *     ship plus its members at (−2,−2)) until none is left, and finally
 *     `FUN_1427_040c` flushes the whole (−2,−2) chain back to the tile. It is
 *     a tile-stack chain rebuild; its return value is the leftover non-ship
 *     head, and 10be uses only that: a trade-route ship at sea (`03e4`
 *     tile_tribe_owner < 0, not in its own Europe slot) gets `free = 0` unless
 *     loose units remain after every other transport on the tile has claimed
 *     its own. Nothing here has a port counterpart — the port has no tile-stack
 *     chain, passengers live in `cargo_ids`.
 *
 * So +0x314c only ever carries 0/1 in this sweep.
 *
 * OpenCol tile substitution: DOS ships berth ON the colony tile, so 10be's own
 * tile stack already holds the marked land units. This port berths ships on
 * adjacent water (`ai_euro_tiles_near`, same substitution the arrival block
 * and the 06e load block make), so the sweep covers the ship's tile plus any
 * adjacent own-colony tile.
 */
/*
 * Raw 2991-2997 (asm 0x30a7-0x30d5) stale board-mark clear, shared by the
 * arrival block and the LAB_3558 anchor. In DOS every act that reaches the
 * 0x3609 10be call at a colony berth has already fallen through the arrival
 * block, so 10be never sees a mark that this act's scan did not set. The
 * 0a60 housekeeping stamps act_state=1 on every aboard passenger at the
 * nation-turn top; without this clear on the unload path a passenger dropped
 * ashore this turn still carries that stamp and 10be re-boards it (the
 * load/unload oscillation the -O3 unit_ai_euro_war failure exposed). Sweep =
 * ship tile + adjacent own colony tile, the same berth substitution 10be
 * makes.
 */
static void ai_euro_20e6_clear_stale_board_marks(
  ColonizeTurnContext* ctx,
  int nation_id,
  const ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ship) {
    return;
  }
  /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index. */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    ColonizeUnit* lu = &ctx->units->units[ui];
    if (!lu->active || lu->aboard_ship_id >= 0 || lu->id < 0 ||
        lu->id >= COLONIZE_UNITS_MAX) {
      continue;
    }
    int on_stack = (lu->x == ship->x && lu->y == ship->y);
    if (!on_stack && ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, lu->x, lu->y);
      const ColonizeColony* lc = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
      if (lc && lc->active && lc->nation_id == nation_id &&
          ai_euro_tiles_near(ship->x, ship->y, lc->x, lc->y)) {
        on_stack = 1;
      }
    }
    if (on_stack && lu->orders == UNITS_ORDER_SENTRY) {
      lu->orders = UNITS_ORDER_NONE; /* +0x314c = 0 */
    }
  }
}

/*
 * 10be force-board arm 1's coordinate test — DOS `member+0x3144 < 0` (asm
 * 1427:11f0 `CMP byte [BX+0x3144],0x0` / `JGE`), i.e. the member sits in an
 * OFF-MAP bucket instead of on a real tile. Every DOS park is a negative x:
 * (−2,−2) = riding a transport, (−3,−3)/(−4,−4) = the transient parks
 * `FUN_1427_04d6` and 10be's own 101c pre-pass use, and x = nation − 0x14 =
 * that nation's Europe slot. The port has one off-map park and spells it with
 * a positive sentinel (Europe at (200,100), `ai_euro_in_europe`), so the
 * faithful predicate is "not addressable as a map tile".
 */
static int ai_euro_20e6_member_off_map(const ColonizeTurnContext* ctx, const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  if (u->x < 0 || u->y < 0 || ai_euro_in_europe(u->x, u->y)) {
    return 1;
  }
  if (ctx && ctx->map && (u->x >= (int)ctx->map->width || u->y >= (int)ctx->map->height)) {
    return 1;
  }
  return 0;
}

static int ai_euro_20e6_transport_assemble(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ship || !ship->active || ship->id < 0 ||
      ship->id >= COLONIZE_UNITS_MAX || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  if (!units_is_sea(ctx->units, ship->id) || ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  /* free = 0x5237[type] − +0x3150 (capacity less goods holds AND passengers). */
  int free_holds = units_ship_free_passenger_slots(ctx->units, ship->id);
  if (free_holds <= 0) {
    return 0;
  }
  const int trace = getenv("AI_20E6_BOARD_TRACE") != NULL;
  int boarded = 0;
  /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index — `units_board`,
   * `units_is_sea` and the id-keyed shadow all take `lu->id`. */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX && free_holds > 0; ++ui) {
    ColonizeUnit* lu = &ctx->units->units[ui];
    if (!lu->active || lu->id == ship->id || lu->nation_id != nation_id ||
        lu->id < 0 || lu->id >= COLONIZE_UNITS_MAX) {
      continue;
    }
    if (lu->aboard_ship_id >= 0 || units_is_sea(ctx->units, lu->id)) {
      continue; /* already in a transport chain / not a stack land member */
    }
    /* Same tile as the ship, or the adjacent own colony it is berthed at. */
    int on_stack = (lu->x == ship->x && lu->y == ship->y);
    if (!on_stack && ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, lu->x, lu->y);
      const ColonizeColony* lc = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
      if (lc && lc->active && lc->nation_id == nation_id &&
          ai_euro_tiles_near(ship->x, ship->y, lc->x, lc->y)) {
        on_stack = 1;
      }
    }
    if (!on_stack) {
      continue;
    }
    /*
     * The board predicate — `FUN_1427_10be`, viceroy_unpacked.c raw
     * 8658-8672 (re-anchored 2026-09-24; verified against the .c, not just
     * the asm), asm 1427:11d4-1231. Three arms,
     * and the decompile's `local_4`/`bVar4` shuffle hides that the two
     * force-board arms are mutually exclusive on the SIGN OF x, not chained:
     *
     *   1427:11d4  CMP byte [BX+0x314c],0x1 / JNZ  →  DI = 1     ; the MARK
     *   1427:11e1  CMP DI,1 / SBB AX,AX / NEG AX / MOV local_4,AX ; = !DI
     *   1427:11ed  OR DI,DI / JNZ 1211                            ; marked → done
     *   1427:11f0  CMP byte [BX+0x3144],0x0 / JGE 1211            ; x >= 0 → arm 2
     *   1427:11f6  MOV local_4,DI            ; DI is 0 here, so local_4 = 0:
     *                                        ; entering arm 1 DISARMS arm 2
     *   1427:11f9  AL = ([BX+0x3147] & 0xf) − [BX+0x3144]
     *              CMP AL,0x14 / JNZ 120e    ; not this nation's Europe slot
     *   1427:1208  CMP byte [BX+0x314c],0x1 / JNZ 1211  ; dead (DI==0 ⇒ !=1)
     *   1427:120e  DI = 1                                         ; ARM 1 boards
     *   1427:1211  CMP local_4,0 / JZ 1232
     *   1427:1219  CALLF FUN_13e4_0074(x, y) / OR AX,AX / JZ 1232
     *   1427:1230  DI = 1                                         ; ARM 2 boards
     *
     * `FUN_13e4_0074` (viceroy_unpacked.c:7033) is `ocean_or_high_seas`:
     * `uVar1 = FUN_137f_010e(x,y); return (uVar1 & 0x1f) == 0x19 ||
     * (uVar1 & 0x1f) == 0x1a;` — terrain class 25 (Ocean) / 26 (High Seas),
     * the same predicate `map_tile_is_water` implements (`map_is_ocean_index`,
     * MAP_OCEAN_INDEX / MAP_HIGH_SEAS_INDEX) and the same one the Indian
     * claim table (`colonies_indian_claim_tribe_from`) and the 4cc6 threat
     * ring already use. It was never undecoded — the 20e6 notes simply had
     * not connected it to `ai_is_ocean_hs` / `FUN_281f_0768`.
     *
     * What the two arms MEAN, given the bucket the loop walks: DOS units are
     * linked into per-coordinate buckets (+0x315c prev / +0x315e next,
     * `FUN_1427_0002`/`004a`), and 10be iterates the bucket the ship was in,
     * so every member shares the ship's coordinates. Therefore:
     *   arm 1 fires only when the SHIP is in an off-map bucket, and boards
     *     every member of it that is not resting in its own nation's Europe
     *     slot — i.e. re-attaches units already parked at (−2,−2)/(−3,−3)/
     *     (−4,−4) to this hull when the transport chain is rebuilt;
     *   arm 2 fires only when the ship's TILE is ocean/high seas, and boards
     *     every land member of that tile unconditionally — a land unit can
     *     only be standing on open water because it is riding this ship.
     * Both are therefore invariant repair, not new recruitment: DOS's way of
     * keeping existing passengers attached. At a colony berth DOS's own tile
     * is the (land) colony tile, so neither arm fires there and the mark arm
     * is the only one that recruits — which is why the 2026-09-07e port with
     * the mark arm alone matched the goldens.
     *
     * OpenCol mapping: passengers live in `cargo_ids`/`aboard_ship_id` rather
     * than in a shared sentinel bucket, so the units the two arms re-attach
     * are exactly the ones the `aboard_ship_id >= 0` skip above already keeps
     * attached — the arms are ported for the cases that skip does NOT cover:
     * an unattached unit sitting in a non-Europe off-map park (arm 1), or
     * standing on the ship's open-water tile (arm 2).
     */
    int take = (lu->orders == UNITS_ORDER_SENTRY); /* +0x314c == 1 */
    int arm = 0;
    if (!take) {
      if (ai_euro_20e6_member_off_map(ctx, lu)) {
        /* arm 1: off-map, but not this unit's own Europe slot. */
        if (!ai_euro_in_europe(lu->x, lu->y)) {
          take = 1;
          arm = 1;
        }
      } else if (ctx->map && map_tile_is_water(ctx->map, lu->x, lu->y)) {
        take = 1; /* arm 2: FUN_13e4_0074 — ocean / high seas */
        arm = 2;
      }
    }
    if (!take) {
      continue;
    }
    const ColonizeUnitType* lty = units_type(ctx->units, lu->type_index);
    const int size = lty ? lty->space : 1; /* 0x5238[type] */
    if (size > free_holds || size >= 99) {
      continue;
    }
    /* FUN_1427_10be walks the ship's coordinate bucket. DOS berths on
     * the colony tile; units_board only admits adjacent coordinates. */
    const int same_tile = lu->x == ship->x && lu->y == ship->y;
    const int attached = same_tile ? units_board_stacked(ctx->units, lu->id, ship->id)
                                   : units_board(ctx->units, lu->id, ship->id);
    if (!attached) {
      continue;
    }
    lu->orders = UNITS_ORDER_SENTRY; /* asm 1427:1264 `[BX+0x314c] = 1` */
    free_holds -= size;
    boarded++;
    if (trace) {
      static const char* const arm_name[3] = {"mark", "force:offmap", "force:water"};
      fprintf(
        stderr, "[10be] ship %d n%d assembles unit %d via %s (size %d, free %d left)\n", ship->id,
        nation_id, lu->id, arm_name[arm], size, free_holds
      );
    }
  }
  return boarded;
}

/*
 * FUN_521d_20e6 md:3120-3133: fill the ship's free holds from colony `c`,
 * one 20e6 load pick per hold (a hold is burnt whether or not goods moved).
 * Returns 1 when anything was loaded. Shared by the berth-arrival and the
 * hauler-pickup arms, which had the loop twice.
 */
static int ai_euro_20e6_load_holds(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int nation_id,
  ColonizeUnit* ship,
  int free_holds,
  int trace
) {
  int loaded = 0;
  /* DOS-LITERAL FUN_521d_20e6 raw 90295: `while (local_d2 != 0 && bVar20)` —
   * bVar20 is the prologue seed (raw 88556-88573): a Man-O-War never loads
   * goods, a Frigate / Privateer only under the seed's narrowings. bugs.md #818. */
  if (!ai_euro_20e6_bvar20_seed(ctx, ship, ai_euro_20e6_dos_type(ctx->units, ship))) {
    return 0;
  }
  while (free_holds > 0) {
    const int g = ai_euro_20e6_load_pick(ctx, c, nation_id, 1);
    if (g < 0) {
      break; /* md:3122-3123: iStack_d2 = 0 */
    }
    int qty = (int)c->stock[g];
    if (qty > 100) {
      qty = 100; /* md:3126-3129 */
    }
    const int moved =
      (qty > 0) ? colonies_transfer_to_unit(ctx->colonies, c->id, ctx->units, ship->id, g, qty)
                : 0;
    if (moved > 0) {
      loaded = 1;
    }
    if (trace || getenv("AI_20E6_LOAD_TRACE")) {
      fprintf(
        stderr, "[load] ship %d n%d colony %d cargo %d qty %d moved %d free %d\n", ship->id,
        nation_id, c->id, g, qty, moved, free_holds
      );
    }
    free_holds -= 1; /* md:3133: DOS burns the hold either way */
  }
  return loaded;
}

/*
 * FUN_521d_20e6 own-colony ARRIVAL block for ships (md:2996-3138), the ship
 * twin of the wagon sequence shipped 2026-09-06f.
 *
 * Structure, from the raw. The gate at md:1691 (doc 1698-1700) is the fork
 * that decides whether 20e6 enters its normal band (LAB_003558: the delivery
 * tallies + delivery matrix + 4393 work-queue peel + the 8-dir wander) or the
 * arrival block first:
 *
 *   bVar10 = unit type;
 *   if ( capacity_table[type*0xe + 0x5237] == 0        // no cargo holds
 *        || iStack_2e != 0                            // NOT at own colony
 *        || ( (type < 0xd || 0x12 < type)             // land hauler …
 *             && unit+0x314a != uStack_62 ) )         // … not bound here
 *     goto LAB_003558;                                // normal band
 *   … arrival block (md:2996-3138) …
 *   goto LAB_003558;                                  // then the normal band
 *
 * So for a SHIP (type 0x0d..0x12) the third clause can never hold: a ship
 * with holds standing at an own colony ALWAYS runs the arrival block, and the
 * delivery matrix then scores the hull the arrival block just rebuilt. That
 * settles the precedence question the 06d note left open: at an own-colony
 * berth the dump+load runs FIRST and the delivery matrix routes the NEW
 * cargo; "delivery matrix first" only ever described a ship away from berth.
 *
 * The block itself:
 *   md:2991-2997  clear act_state 1 on the berth tile's units (LIVE since
 *                  2026-09-07e — the +0x314c channel is the
 *                  the real +0x314c byte)
 *   md:2999-3001  bind colony uStack_62 / nation uStack_e6
 *   md:3002-3007  while (holds_occupied) { g = pull hold 0 (8cdc compacts
 *                  and stashes qty in 0x8dc4); colony stock[g] += qty; }
 *                  — UNCONDITIONAL: every hold, every cargo type, no ship /
 *                  cargo exception, and no "colony is short of it" test.
 *   md:3008-3011  ships only: unit+0x314a = 0xff (drop the last-loaded-at
 *                  latch the delivery matrix reads) and colony+0x8f = 0
 *                  (cargo_idle_turns).
 *   md:3012-3016  iStack_d2 = capacity − holds_occupied (0 after the dump,
 *                  so = capacity); wagons clamp to 1.
 *   md:3052-3131  the LOAD matrix (ai_euro_20e6_load_pick, ported 06e), one
 *                  cargo per free hold, qty = min(stock, 100).
 *   md:3127-3131  a load latches unit+0x314a = colony for ships.
 *
 * Both formerly-unmodelled arms of this block are live:
 *   - md:3018-3023 "cower in port" (2026-09-07): +0x315a is the COL1
 *     `col1_counter16` byte, save-round-tripped since 2026-09-07c.
 *   - md:3024-3051: the passenger-board MARK scan. It only stamps
 *     act_state = 1 and debits the hold budget; the physical boarding is
 *     FUN_1427_10be at LAB_3558, ported as
 *     ai_euro_20e6_transport_assemble and called from the tail of this
 *     function (DOS's fall-through into 0x3558). See that function's header
 *     for the asm control-flow proof that mark and assembly are one act.
 *
 * Returns the colony id the ship berthed at (dump attempted), else −1.
 */
static int ai_euro_20e6_ship_berth_arrival(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship,
  int* out_cowered
) {
  if (!ctx || !ctx->units || !ctx->colonies || !ship || !ship->active || ship->id < 0 ||
      ship->id >= COLONIZE_UNITS_MAX) {
    return -1;
  }
  /* md:1691 first clause: capacity_table[type] == 0 → no arrival block. */
  const int holds = units_goods_hold_count(ctx->units, ship->id);
  if (holds <= 0) {
    return -1;
  }
  const int trace = getenv("AI_20E6_SHIP_DUMP_TRACE") != NULL;
  if (trace) {
    fprintf(
      stderr, "[shipdump] enter ship %d n%d at (%d,%d) holds %d\n", ship->id, nation_id,
      ship->x, ship->y, holds
    );
  }
  ColonizeColony* c = NULL;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* cand = &ctx->colonies->colonies[i];
    if (!cand->active || cand->nation_id != nation_id) {
      continue;
    }
    /* iStack_2e == 0: standing at own colony uStack_62. Ships berth on
     * adjacent water in this port, so near, not colonies_id_at — the same
     * substitution the 06e load block already used. */
    if (!ai_euro_tiles_near(ship->x, ship->y, cand->x, cand->y)) {
      continue;
    }
    c = cand;
    break;
  }
  if (!c) {
    return -1;
  }
  /*
   * Raw 2991-2997 (asm 0x30a7-0x30d5): before anything else, walk the berth
   * tile's stack and clear every stale act_state == 1 board mark, so this
   * act's scan re-decides from scratch. DOS iterates the ship's own tile
   * (ships berth ON the colony tile); this port berths on adjacent water, so
   * the colony tile the ship is berthed at is the equivalent stack.
   */
  ai_euro_20e6_clear_stale_board_marks(ctx, nation_id, ship);

  /* md:3002-3007: dump every hold into the colony, unconditionally. */
  int dumped = 0;
  for (;;) {
    int hold = -1;
    const int n = units_goods_hold_count(ctx->units, ship->id);
    for (int h = 0; h < n; ++h) {
      if (units_hold_amount(ctx->units, ship->id, h) > 0) {
        hold = h;
        break;
      }
    }
    if (hold < 0) {
      break;
    }
    const int g = ship->hold_goods_type[hold];
    const int moved =
      colonies_transfer_from_unit(ctx->colonies, c->id, ctx->units, ship->id, hold, NULL);
    if (moved <= 0) {
      break; /* warehouse refused (OpenCol-only clamp) — do not spin */
    }
    dumped += moved;
    if (trace) {
      fprintf(
        stderr, "[shipdump] ship %d n%d colony %d dump cargo %d qty %d\n", ship->id,
        nation_id, c->id, g, moved
      );
    }
  }

  /* md:3008-3011: ships drop the +0x314a bind and zero colony +0x8f. */
  ai_euro_20e6_origin_set(ship, -1);
  c->cargo_idle_turns = 0;

  /*
   * Raw 3018-3023 "cower in port" (2026-09-07): a cargo ship no bigger than a
   * Merchantman (type <= 0xe) at a colony flagged NEARBY_FRIGATE
   * (+0x1b bit 0x02) bumps its +0x315a counter and parks (orders 0x43,
   * LAB_5899) until the counter reaches 10 − hold capacity (Caravel 8 beats,
   * Merchantman 6). DOS never resets the byte — the wait happens once per
   * hull. The park skips boarding and the load matrix. +0x315a is the COL1
   * `col1_counter16` byte (unit record offset 0x16), save-round-tripped since
   * 2026-09-07 — the counter survives save/load as in DOS.
   */
  const int arrival_dos_type = ai_euro_20e6_dos_type(ctx->units, ship);
  if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0 && arrival_dos_type <= 0x0e) {
    if (ship->col1_counter16 < 255) {
      ship->col1_counter16++;
    }
    if (ship->col1_counter16 < 10 - holds) {
      if (out_cowered) {
        *out_cowered = 1;
      }
      if (trace) {
        fprintf(
          stderr, "[shipdump] ship %d n%d cowers at colony %d (count %d < %d)\n", ship->id,
          nation_id, c->id, ship->col1_counter16, 10 - holds
        );
      }
      return c->id;
    }
  }

  /* md:3012-3016 + 3052-3131: free = capacity − holds_occupied, then the
   * DOS LOAD matrix one cargo per free hold. Passengers already aboard are
   * charged here (ai_euro_20e6_ship_hold_budget) because the port's cargo_ids
   * substitution stops the scan below from re-reserving them the way DOS's
   * clear+re-mark round trip does. */
  int loaded = 0;
  int free_holds = ai_euro_20e6_ship_hold_budget(ctx->units, ship);

  /*
   * Raw 3024-3051 passenger-board MARK scan (2026-09-07; reworked to the
   * literal mark-then-assemble 2026-09-07e). Gate: type gate bVar7 and
   * unit+0x3148 bit 0x20 clear. DOS walks the colony-tile stack while
   * DS:0x1734[nation] < 0x19 and marks members act_state = 1, debiting the
   * ship's hold budget by the member's 0x5238 size. It does NOT board here:
   * the budget it leaves is what the load matrix below may fill with goods,
   * and FUN_1427_10be (ai_euro_20e6_transport_assemble, called at the tail of
   * this function = DOS's 0x3558 fall-through) then boards exactly the
   * reserved remainder. Eligible: an armed land unit (0x5236 combat > 1,
   * orders byte not 'G'/'A') on a stance-0 continent; a Pioneer (type 2)
   * unless the ship's composite priority (iStack_14, FUN_521d_0600) is 0 on a
   * non-0 stance continent. After the scan DOS zeroes 0x1734[nation]
   * (:81295) — the only reset the counter has.
   */
  if (ai_euro_20e6_457e_type_gate(ctx, ship, arrival_dos_type) &&
      (ship->col1_flags15 & AI_EURO_F3148_SPARE) == 0) {
    const int cid = ctx->map ? map_continent_id_at(ctx->map, c->x, c->y) : -1;
    const int stance = ai_euro_continent_stance_at(nation_id, cid);
    const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
    const int ship_prio = ai_goals_composite_unit_priority_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, nation_id, ship->x, ship->y, arrival_dos_type, ship->profession, turn, colonies_count_for_nation(ctx->colonies, nation_id));
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      if (ai_euro_s_0a60_work_registered[nation_id] >= 0x19) {
        break;
      }
      /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index; the id-keyed
       * shadow below keeps its `lu->id` key and its 256-wide guard. */
      ColonizeUnit* lu = &ctx->units->units[ui];
      if (!lu->active || lu->nation_id != nation_id || lu->aboard_ship_id >= 0 ||
          lu->id == ship->id || lu->id < 0 || lu->id >= COLONIZE_UNITS_MAX) {
        continue;
      }
      if (lu->x != c->x || lu->y != c->y) {
        continue;
      }
      const int lt = ai_euro_20e6_dos_type(ctx->units, lu);
      if (lt < 0 || (lt >= 0x0d && lt <= 0x12)) {
        continue;
      }
      const ColonizeUnitType* lty = units_type(ctx->units, lu->type_index);
      const int space = lty ? lty->space : 1;
      if (space > free_holds || space >= 99) {
        continue;
      }
      int mark = 0;
      const int oc = lu->col1_ai_plan; /* +0x314b */
      if (ai_euro_20e6_type_combat(lt) > 1 && oc != 'G' && oc != 'A' && stance == 0) {
        mark = 1;
      }
      if (lt == 0x02) {
        if (ship_prio == 0 && stance != 0) {
          continue; /* raw goto 32e3 */
        }
        mark = 1;
      }
      if (mark) {
        /* md:3046-3050: act_state = 1, iStack_d2 -= 0x5238[type]. */
        lu->orders = UNITS_ORDER_SENTRY; /* +0x314c = 1 */
        free_holds -= space;
        if (trace) {
          fprintf(
            stderr, "[shipdump] ship %d n%d MARKS unit %d (type 0x%02x space %d)\n",
            ship->id, nation_id, lu->id, lt, space
          );
        }
      }
    }
    ai_euro_s_0a60_work_registered[nation_id] = 0; /* :81295 */
  }

  if (ai_euro_20e6_load_holds(ctx, c, nation_id, ship, free_holds, trace)) {
    loaded = 1;
  }
  /* md:3127-3131: the ship arm latches the source colony in +0x314a, which
   * the delivery matrix below then skips (md:2055). */
  if (loaded) {
    ai_euro_20e6_origin_set(ship, c->id);
  }
  /*
   * asm 0x354e → 0x3558 → 0x3609: the arrival block falls out of the load
   * loop straight into LAB_3558, whose first call is FUN_1000_8b10
   * (= FUN_1427_10be) on this ship. Everything the scan above marked boards
   * here, in the same act, into the space the load matrix left free.
   */
  (void)ai_euro_20e6_transport_assemble(ctx, nation_id, ship);
  if (trace) {
    fprintf(
      stderr, "[shipdump] ship %d n%d colony %d dumped %d loaded %d\n", ship->id, nation_id,
      c->id, dumped, loaded
    );
  }
  return c->id;
}

/*
 * FUN_521d_20e6 ship band entry: own-colony arrival (dump + load matrix),
 * the hold-cargo delivery matrix and its sell tail, then the 4393 work-queue
 * peel. Hull test is DOS's own (md:1691: nonzero hold capacity), not a name
 * list. Peace only — the war arms own idle ships at war.
 * Returns 1 if a course was set or the beat was claimed at a berth.
 */
int ai_euro_try_ship_trade_haul(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !ship || !ship->active) {
    return 0;
  }
  if (ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  /*
   * DOS md:1691 gate, first clause: `capacity_table[type*0xe + 0x5237] == 0`.
   * That is the ONLY hull test FUN_521d_20e6 applies before the arrival block
   * and the delivery band — any unit type 0x0d..0x12 (Caravel..Man-O-War) with
   * a nonzero hold capacity runs both. The port used to gate this whole
   * function on `ai_euro_is_cargo_ship_name` (Caravel/Merchantman/Galleon),
   * which left a Privateer's capture loot (FUN_5fef_0352 hands the winner the
   * loser's holds, viceroy_overlays.c:85035-85050) with no DOS consumer at
   * all. Widened to the DOS shape 2026-09-18; the 4393 work-queue peel below
   * keeps the narrower hull test, see its call site.
   */
  if (!units_is_sea(ctx->units, ship->id)) {
    return 0;
  }
  if (units_goods_hold_count(ctx->units, ship->id) <= 0) {
    return 0;
  }
  /*
   * DOS own-colony ARRIVAL block (md:1691 gate + md:2996-3138) runs BEFORE
   * everything below — see ai_euro_20e6_ship_berth_arrival for the fork decode.
   * It sits above the has_tools/has_cap early-out on purpose: DOS's gate asks
   * only "has holds && at own colony", so a hull with no haul cargo and no
   * free slot still dumps. The cargo flags below are therefore computed on the
   * post-arrival hull.
   */
  int cowered = 0;
  const int berthed_at = ai_euro_20e6_ship_dump_enabled()
                           ? ai_euro_20e6_ship_berth_arrival(ctx, nation_id, ship, &cowered)
                           : -1;
  if (cowered) {
    return 1; /* md:3023 → LAB_5899: parked in port this beat */
  }
  const int has_tools = ai_euro_unit_hold_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_TOOLS);
  const int has_lumber =
    ai_euro_unit_hold_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_LUMBER);
  const int has_ore = ai_euro_unit_hold_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_ORE);
  const int has_muskets =
    ai_euro_unit_hold_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_MUSKETS);
  const int has_horses =
    ai_euro_unit_hold_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_HORSES);
  const int has_food = ai_euro_unit_hold_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_FOOD);
  const int has_cap = ai_euro_unit_hold_has_capacity(ctx->units, ship);
  if (!has_tools && !has_lumber && !has_ore && !has_muskets && !has_horses && !has_food &&
      !has_cap) {
    return 0;
  }

  /*
   * (The port-only "adjacent short coastal colony + haul cargo -> unload" arm
   * that stood here was deleted 2026-09-18. DOS has no per-cargo "colony is
   * short of this" unload in FUN_521d_20e6 — the arrival block dumps the whole
   * hull unconditionally (md:3002-3007) and the delivery matrix (raw
   * 2047-2139) picks the destination. The arm was already unreachable with
   * AI_20E6_SHIP_DUMP on, because the berth arrival above fires for exactly
   * the hull/colony pairs it tested. `ai_euro_colony_haul_cargo_short` went
   * with it as its only caller.)
   */

  /*
   * On an own coastal colony with a free hold → the DOS load matrix
   * (ai_euro_20e6_load_pick, md:3059-3134): one cargo per free hold, DOS
   * weights, until the hull is full or the matrix declines. Replaces the
   * port's own TOOLS/LUMBER/ORE/MUSKETS/HORSES/FOOD ladder (and its
   * food_short-first reorder), which had no DOS reading.
   *
   * FALLBACK ONLY. With AI_20E6_SHIP_DUMP on (the default) the arrival block
   * at the top of this function has already run the DOS pair — the raw
   * 3002-3007 dump sweep and then this same matrix on the emptied hull — so
   * berthed_at >= 0 and this block is dead. It survives verbatim as the
   * AI_20E6_SHIP_DUMP=0 path: the 06e empty-hull gate substitution, which
   * stood in for the dump precondition by simply refusing to score a
   * part-loaded hull (DOS state the matrix never sees, because DOS empties it
   * first).
   * Ships berth on adjacent water, so ai_euro_tiles_near, not colonies_id_at.
   */
  if (berthed_at < 0 && has_cap &&
      ai_euro_hauler_free_holds(ctx->units, ship) ==
        units_goods_hold_count(ctx->units, ship->id)) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (!ai_euro_tiles_near(ship->x, ship->y, c->x, c->y)) {
        continue;
      }
      int loaded = 0;
      /* iStack_d2 = hold capacity − holds_occupied (md:3020-3023), less the
       * hull the port's persistent passengers hold (see
       * ai_euro_20e6_ship_hold_budget). */
      int free_holds = ai_euro_20e6_ship_hold_budget(ctx->units, ship);
      if (ai_euro_20e6_load_holds(ctx, c, nation_id, ship, free_holds, 0)) {
        loaded = 1;
      }
      /*
       * Raw 3134-3138: the ship arm (iStack_34 != 0) latches the source colony
       * in unit byte +0x314a — exactly the colony the delivery matrix then
       * skips. (The land arm sets +0x3158 = 1 instead, the village-errand byte
       * 47b9 reads; wagons do not come through here.)
       */
      if (loaded) {
        ai_euro_20e6_origin_set(ship, c->id);
      }
      break;
    }
  }

  int cx = 0;
  int cy = 0;
  /*
   * FUN_521d_20e6 md:2044-2139. Delivery cargo aboard (Horses / Trade Goods /
   * Tools / Muskets) → the DOS matrix owns the destination outright: 20e6 is
   * the per-unit mover; a matrix that rejects every colony runs the sell
   * tail (md:2140-2163) and then falls through to the 4393 peel with the
   * emptied hull, exactly as DOS does. Without such cargo DOS never enters
   * this block at
   * all (`-1 < iStack_44`), so the 4393 work-queue peel and the short-colony
   * haul keep owning FOOD/LUMBER/ORE runs.
   */
  int tally[COLONIZE_CARGO_COUNT];
  const int max_type = ai_euro_20e6_delivery_tallies(ctx, ship, tally);
  int have_dest = 0;
  if (max_type >= 0) {
    have_dest =
      ai_euro_20e6_delivery_colony_pick(ctx, ship, nation_id, tally, max_type, &cx, &cy);
    if (!have_dest) {
      /*
       * Raw 2140-2163 sell tail: no colony will take the delivery cargo, so
       * the hold is dumped for gold. DOS then falls straight through to
       * LAB_004393 with an empty hull (the `bVar10 = holds_occupied` gate
       * right after the loop cannot fire once it has been zeroed), so the
       * work-queue peel below owns the now-empty ship this same beat.
       */
      (void)ai_euro_20e6_delivery_sell_tail(ctx, ship, nation_id);
    }
  }
  /*
   * Raw 2166-2168 (doc 2173-2175) — the arrival block's own consumer. Once the
   * delivery matrix has had its say DOS re-reads holds_occupied:
   *
   *   bVar10 = unit+0x3150;
   *   if (capacity_table[type] == bVar10 || 1 < bVar10) goto LAB_003fa6;
   *
   * and LAB_003fa6 is `FUN_1000_94da(param_2)` = FUN_291f_02ea →
   * FUN_48d3_015e, the expanding-ring hunt for a High Seas tile that stamps
   * orders 0x45 (resolved in this file's 47b9/457e symbol table). So a ship
   * that leaves the berth laden sails for Europe and never reaches
   * LAB_004393's work-queue peel — declining here hands it to the
   * dispatcher's Europe-export arm, which is the port's stand-in for
   * 48d3_015e.
   *
   * Without this the dump+load could park the ship at its own berth forever
   * (the 06e/06f oscillation class); the gate is DOS's own answer to it.
   *
   * Unscoped 2026-09-07 to the DOS shape: the gate keys off holds_occupied
   * for EVERY untasked ship reaching this point (DOS guards the whole band
   * with `uStack_b6 != 0` at LAB_003558 entry — an empty hull skips to 4393
   * directly, which `occupied > 1 || occupied == capacity` preserves). A
   * ship carrying 2+ holds of non-delivery cargo now sails for Europe
   * instead of taking a queue tip, exactly as DOS routes it.
   */
  if (!have_dest) {
    const int capacity = units_goods_hold_count(ctx->units, ship->id);
    const int occupied = capacity - ai_euro_hauler_free_holds(ctx->units, ship);
    if (occupied > 1 || (capacity > 0 && occupied == capacity)) {
      return 0;
    }
  }
  if (!have_dest) {
    /*
     * Queue tip only (2026-09-07): the OpenCol-only nearest-short-coastal-colony
     * fallback is retired — DOS has no such scan. An empty queue drops the
     * ship to LAB_457e's arms (HS cadence / Europe export / explore band),
     * exactly where the dispatcher chain sends a declined ship here.
     *
     * Hull test (bVar17, raw ~1284-1306): DOS's base term is `type != 0x12`
     * (Man-O-War), narrowed further for Privateer (0x10) and Frigate (0x11) by
     * globals this project has not resolved — see
     * ai_euro_4393_work_queue_haul_pick's header, "DEAD END". Those narrowings
     * can only REMOVE warships from the queue, so when the band gate above was
     * widened to DOS's raw-1691 hull test (2026-09-18) the peel kept the
     * pre-existing cargo-hull list rather than guess at the unresolved half.
     */
    if (!ai_euro_is_cargo_ship_name(ai_euro_unit_kind(ctx->units, ship)) ||
        !ai_euro_4393_work_queue_haul_pick(
          ctx, nation_id, ship->x, ship->y, ship, &cx, &cy
        )) {
      return 0;
    }
  }
  /* FUN_521d_20e6 LAB_4567 -> 27f5, raw 89927-89929: the
   * destination is the colony itself, not a neighbouring water tile. */
  ai_euro_set_goto(ship, AI_EURO_ACT_GOAL, cx, cy);
  return 1;
}

/*
 * Peace Europe export sail — the port's stand-in for LAB_003fa6
 * (`FUN_1000_94da` = FUN_291f_02ea -> FUN_48d3_015e), the expanding-ring High
 * Seas hunt FUN_521d_20e6 jumps to at md:2166-2168 when the band leaves the
 * hull laden. The Europe end is DOS-real and already ported: FUN_521d_5d04's
 * dock loop (viceroy_overlays.c:83168-83192, `ai_euro_5d04_cb_sell_hold0` /
 * `_cb_reward_case`) empties the holds of EVERY Europe ship of type
 * 0x0d..0x12, warships included — so any hull that gets here has a seller.
 * The colony-surplus LOAD below stays a cargo-hull errand (FUN_364b_0688 /
 * 0636, stock>99 → leave 50); the sail itself is open to any hull already
 * carrying export-eligible goods, which is where a Privateer's capture loot
 * (FUN_5fef_0352, viceroy_overlays.c:85035-85050) goes.
 */
static int ai_euro_ship_holds_export_goods(const ColonizeUnitPool* units, const ColonizeUnit* ship) {
  if (!units || !ship) {
    return 0;
  }
  const int n = units_goods_hold_count(units, ship->id);
  for (int h = 0; h < n; ++h) {
    if (units_hold_amount(units, ship->id, h) <= 0) {
      continue;
    }
    if (europe_cargo_export_eligible(ship->hold_goods_type[h])) {
      return 1;
    }
  }
  return 0;
}

/*
 * Europe-bound sail tail shared by the export and Privateer-loot arms (audit
 * AE-16): step into the Europe park if the ship already stands on a High Seas
 * tile, else aim AI_SAIL at the nearest Europe lane entry. Returns 0 only when
 * there is no lane to aim at, or the ship is already standing on it.
 */
static int ai_euro_ship_sail_to_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship) {
  if (ai_euro_ship_enter_europe(ctx, ship)) {
    return 1;
  }
  int ex = 0;
  int ey = 0;
  if (!ai_euro_europe_sail_target(ctx, ship->x, ship->y, &ex, &ey)) {
    return 0;
  }
  if (ship->x == ex && ship->y == ey) {
    return 0;
  }
  if (units_orders_follow_goto(ship->orders) && ship->goto_x == ex && ship->goto_y == ey) {
    return 1;
  }
  ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, ex, ey);
  return 1;
}

/*
 * FUN_48d3_0002 (raw 77563-77588) for an AI hull: reseed from the timer word
 * (FUN_281f_04ca(DS:0x83a6) = ai_nation_reseed), then the shared 1-2 turn
 * roll over the nation's hull tally DS:0x9418 and Magellan. The second
 * argument is the New World x of the crossing (bugs.md #1019).
 */
static int ai_euro_lane_voyage_turns(ColonizeTurnContext* ctx, int nation_id, int new_world_x) {
  ai_nation_reseed(ctx);
  const bool magellan = ctx->col1_ok && ctx->col1 &&
    founding_fathers_nation_has(ctx->col1, nation_id, FF_FERDINAND_MAGELLAN);
  return europe_voyage_turns_roll(
    ctx->rng, magellan, turn_voyage_ship_count(ctx, nation_id), new_world_x
  );
}

/* Hop a parked hull (and its passengers) to another off-map slot. */
static void ai_euro_lane_move(ColonizeTurnContext* ctx, ColonizeUnit* ship, int x, int y) {
  const int ox = ship->x;
  const int oy = ship->y;
  ship->x = x;
  ship->y = y;
  units_occupancy_notify_moved(ctx->units, ox, oy, x, y);
  ai_euro_sync_aboard_cargo_xy(ctx->units, ship);
}

/*
 * AI High Seas -> Europe crossing, DOS-LITERAL FUN_48d3_007a (raw
 * 77592-77632): a hull standing on a High Seas tile records it as its
 * landfall (+0x314d/e; DOS also writes the nation's -0x77c6/-0x77c5 copy,
 * which only the human's Europe screen reads), takes FUN_48d3_0002's 1-2
 * turn roll into +0x315a and is parked on the eastbound lane n - 0x0c
 * (244+n). ai_euro_europe_lane_tick walks it to the dock (bugs.md #1056;
 * it used to arrive instantly). Without this gate the sail target
 * re-resolves every act and the ship wiggles on the sealane forever.
 */
int ai_euro_ship_enter_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship) {
  if (!ctx || !ctx->map || !ctx->units || !ship ||
      !map_tile_is_high_seas(ctx->map, ship->x, ship->y)) {
    return 0;
  }
  const int ox = ship->x;
  const int oy = ship->y;
  const int n = ship->nation_id;
  ship->col1_counter16 = ai_euro_lane_voyage_turns(ctx, n, ox);
  ai_euro_lane_move(ctx, ship, 244 + n, 244 + n);
  ai_euro_set_goto(ship, UNITS_ORDER_NONE, ox, oy);
  /* The course is spent: DS:0x9456 now counts the hull through its lane
   * position (raw 78182-78185), so the `+0x314b = 0x45` stamp comes off or
   * the hull would be counted twice. */
  ship->col1_ai_plan = 0;
  ship->moves = 0;
  return 1;
}

/*
 * Europe -> New World departure, DOS-LITERAL FUN_48d3_0346 (raw
 * 77732-77757, reached through FUN_291f_0ec2 from the FUN_521d_5d04 dock
 * pass): roll the crossing against the landfall x and park the hull on the
 * westbound lane n - 0x18 (232+n). The port departs from the dispatcher's
 * Europe act rather than inside 5d04 (bugs.md #1056).
 */
void ai_euro_ship_leave_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship) {
  if (!ctx || !ctx->units || !ship) {
    return;
  }
  int lx = 0;
  int ly = 0;
  ai_euro_resolve_landfall_goto(ctx, ship, &lx, &ly);
  const int n = ship->nation_id;
  ship->col1_counter16 = ai_euro_lane_voyage_turns(ctx, n, lx);
  ai_euro_lane_move(ctx, ship, 232 + n, 232 + n);
  ship->moves = 0;
}

/*
 * FUN_48d3_06ba head (raw 77965-77980): four FUN_48d3_03d0 hops, in DOS
 * order, run at the end of the nation's own FUN_3844_00f2. Each hop walks
 * the hulls on one lane, ticks +0x315a down while it is non-zero and moves
 * the hull on when it reads zero. 224+n is left for the dispatcher's
 * Europe act to place (the FUN_48d3_064e / 048e tail); 236+n is the dock,
 * the port's (200,100).
 */
void ai_euro_europe_lane_tick(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || nation_id < 0 || nation_id > 3) {
    return;
  }
  static const int hops[4][2] = {{228, 224}, {232, 228}, {240, 236}, {244, 240}};
  for (int h = 0; h < 4; ++h) {
    const int from = hops[h][0] + nation_id;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0 ||
          u->x != from || u->y != from) {
        continue;
      }
      if (u->col1_counter16 > 0) {
        u->col1_counter16--;
      }
      if (u->col1_counter16 == 0) {
        if (hops[h][1] == 236) {
          ai_euro_lane_move(ctx, u, 200, 100);
        } else {
          ai_euro_lane_move(ctx, u, hops[h][1] + nation_id, hops[h][1] + nation_id);
        }
      }
    }
  }
}

int ai_euro_try_ship_europe_export(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !ship || !ship->active) {
    return 0;
  }
  if (ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  if (!units_is_sea(ctx->units, ship->id)) {
    return 0;
  }
  if (units_goods_hold_count(ctx->units, ship->id) <= 0) {
    return 0;
  }

  /*
   * Colony->ship load sweep retired 2026-09-24 (bugs.md #864): DOS's only AI
   * ship goods load is FUN_521d_20e6's LOAD matrix (raw 90295-90370,
   * `ai_euro_20e6_load_pick`); FUN_364b_0688 (raw 57238-57300) is the Custom
   * House in-place sale and never loads a hull.
   */

  if (!ai_euro_ship_holds_export_goods(ctx->units, ship)) {
    return 0;
  }
  /*
   * Only a real load is worth the crossing: the arm's own load rule above
   * yields >= 50 (stock > 99, leave 50), so hold that bar for cargo the
   * berth-arrival load matrix put aboard as well. A few furs from a young
   * colony's stock sent the Caravel to Europe and back every four turns
   * (the "circles"), the same trip again each time it came home.
   */
  {
    int total = 0;
    const int n = units_goods_hold_count(ctx->units, ship->id);
    for (int h = 0; h < n; ++h) {
      if (europe_cargo_export_eligible(ship->hold_goods_type[h])) {
        total += units_hold_amount(ctx->units, ship->id, h);
      }
    }
    if (total < 50) {
      return 0;
    }
  }
  return ai_euro_ship_sail_to_europe(ctx, ship);
}

/*
 * The war-cargo colony-sail arm that stood here (`ai_euro_try_ship_war_cargo_sail`)
 * was a second, earlier entry into LAB_521d_3558 with its own invented scorer;
 * smell audit sweep-3 area C #6 retired both. DOS reaches 3558 exactly once per
 * act, from the 20e6 unload/settle flow, which this port keeps in
 * `ai_euro_act_ship_dos` -> `ai_euro_20e6_colony_sail_pick` (raw 89614-89711,
 * ladder at 89663). Goods-only hulls are the delivery matrix's business (raw
 * 2047-2139), not 3558's; military passengers still reach the structural pick
 * through the settle gate.
 */

/*
 * (`ai_euro_try_privateer_europe_loot_sail` — "Privateer already carrying
 * export-eligible goods -> AI_SAIL Europe" — was deleted 2026-09-18. It was a
 * manual-cited duplicate of the Europe export arm above, differing only in
 * which hulls it accepted; with that arm's gate widened to DOS's raw-1691 hull
 * test (any type 0x0d..0x12 with holds) the Privateer loot case is the same
 * code path DOS uses, md:2166-2168 -> FUN_48d3_015e -> the FUN_521d_5d04 dock
 * sell loop.)
 */


/*
 * Effective defense for the thin 20e6 adjacent-foe picks (audit AE-10 — the
 * naval and land copies had identical bodies). Naval reads the shared
 * FUN_157e_004a base via combat_unit_base_x8 (damage / holds / Drake); land
 * reads FUN_157e_015e via combat_unit_toughness (colony / village / terrain /
 * fortify + vet). Cite: combat_strength.c; FUN_157e_004a / 015e.
 */
int ai_euro_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f,
  int is_naval
) {
  if (!units || !f) {
    return 9999;
  }
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  sctx.units = units;
  sctx.col1 = (ctx && ctx->col1_ok) ? ctx->col1 : NULL;
  const int tough = is_naval ? combat_unit_base_x8(&sctx, f->id, 0, NULL)
                             : combat_unit_toughness(&sctx, f->id, -1);
  return tough > 0 ? tough : 0;
}


/* True when a unit already has a non-stationary AI/sail/goto course (audit
 * AE-11: the ship and land copies were byte-identical). */
int ai_euro_has_useful_goto(const ColonizeUnit* u, const ColonizeWorldMap* map) {
  if (!u || !map || !units_orders_follow_goto(u->orders)) {
    return 0;
  }
  if (u->goto_x < 0 || u->goto_y < 0 || u->goto_x >= UNITS_GOTO_NONE ||
      u->goto_y >= UNITS_GOTO_NONE || u->goto_x >= map->width || u->goto_y >= map->height) {
    return 0;
  }
  return u->goto_x != u->x || u->goto_y != u->y;
}

/*
 * bugs.md #521 (closed 2026-09-22): the golden-backed adjacent-attack stand-in
 * pair (ai_euro_land_best_adjacent_foe / ai_euro_land_try_adjacent_attack)
 * that lived here was deleted. Its reason to exist was the LAB_521d_52aa odds
 * core scoring 0 against any 2+-defender land stack, which was a port bug:
 * ai_euro_20e6_unit_col9 returned the @UNIT guns column (0x523b) instead of
 * the cost column (0x5239). With the right column the DOS wander scorer
 * (ai_euro_20e6_attack_term) assaults stacked colonies on its own.
 */


/* (The ai_euro_foreign_land_threat_near helper was deleted with the invented
 * peace colony-defence wake arm, bugs.md #760.) */

/* FUN_521d_0a60 raw 87935/87938/88050 records the LAND continent
 * independently of its ocean goal tile; 20e6 reads those masks directly
 * (raw 89515/89552). Re-deriving them from goal coordinates loses that
 * distinction and suppresses a valid unload (#530). */
static int ai_euro_20e6_goal_on_continent(int nation, int cid, int want_mil) {
  if (nation < 0 || nation >= 4 || cid < 0 || cid >= 16) {
    return 0;
  }
  const uint16_t mask = want_mil ? ai_euro_s_0a60_mask_mil[nation]
                                 : ai_euro_s_0a60_mask_found[nation];
  return (mask & (1u << cid)) != 0;
}

/*
 * FUN_281f_08bc (= FUN_1427_0d38) stack-query counts over the ship's cargo
 * (DOS: tile stack — carried units share the ship's tile there; OpenCol keeps
 * them in cargo_ids, the equivalent set for a ship at sea). Modes ndisasm-
 * confirmed at the LAB_3558 call block (viceroy_overlays.asm 136292-136330,
 * literal PUSHes) against the byte-exact 0d38 case bodies (2026-09-06):
 *   iStack_4a = mode 3 = # Pioneers          — the "flag-count 0x40 founders"
 *     reading is retired; the attack-core "# Pioneers" note was correct.
 *   iStack_48 = mode 4 = # military land types {1,4,6,7,8,9};
 *     at war (DS:0x5382 bit0) += mode 0xc = # Artillery (type 0xb).
 *   iStack_16 = mode 5 = # Scouts            — not Missionary+Scout.
 *   iStack_46 = mode 6 = # {Soldier,Dragoon} + # veteran-professioned
 *     (profession 0x15) others + # types {6..9} (vet-typed counted twice).
 *   iStack_82 = (stack−1) − pioneers − mil(pre-war-add) − scouts = # plain
 *     civilians aboard (the "founder cargo" the colony-sail gate reads).
 */
void ai_euro_20e6_ship_cargo_counts(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship,
  int* pioneers, int* mil, int* scouts, int* milvet, int* civ
) {
  *pioneers = 0;
  *mil = 0;
  *scouts = 0;
  *milvet = 0;
  int total = 0;
  int artillery = 0;
  for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[s]);
    if (!p || !p->active) {
      continue;
    }
    total++;
    const int t = ai_euro_20e6_dos_type(ctx->units, p);
    if (t == 2) {
      (*pioneers)++;
    }
    if (t == 1 || t == 4 || t == 6 || t == 7 || t == 8 || t == 9) {
      (*mil)++;
    }
    if (t == 5) {
      (*scouts)++;
    }
    if (t == 0xb) {
      artillery++;
    }
    /* mode 6 body: {1,4} or vet profession, then {6..9} again. */
    if (t == 1 || t == 4) {
      (*milvet)++;
    } else if (p->profession == UNITS_JOB_SOLDIER) {
      (*milvet)++;
    }
    if (t >= 6 && t <= 9) {
      (*milvet)++;
    }
  }
  *civ = total - *pioneers - *mil - *scouts; /* iStack_82, pre-war formula */
  if (*civ < 0) {
    *civ = 0;
  }
  const int woi = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.game_options.woi : 0;
  if (woi) {
    *mil += artillery; /* md:1736-1738: wartime mode-0xc add */
  }
}

/*
 * FUN_521d_20e6 LAB_3558 land-adjacent unload mask (local_9c, raw ~1766-1884
 * of move_scoring_20e6_full.md; euro_ocean_scoring.c section map). Walks the
 * 8 tiles around the ship; each qualifying land tile recomputes the mask from
 * scratch (DOS re-zeroes local_9c per tile — the last qualifying tile wins,
 * replicated verbatim). Bits: 0x40 founder unload (Pioneers + goal-promoted
 * civilians), 0x20 Scout unload, 0x10 military, 0xffff "ship goto lands on
 * this continent — unload all". Cargo counts are the real 0d38 stack-query
 * modes since 2026-09-06 (see ai_euro_20e6_ship_cargo_counts).
 *
 * Substitutions (documented per term):
 *  - DS:0x1734 per-nation urgency accumulator: REAL since 2026-09-07
 *    (ai_euro_s_0a60_work_registered — bumped at 0a60 registration, zeroed by the
 *    berth boarding scan), no longer a colony-flag count.
 *  - a654 per-unit goal id for the goal fold: nation-level FOUND/MIL_EXPAND
 *    goal existence (see the fold comment in the body).
 *  - presence bit 0x08 of −0x6a0e: writer undecoded — read as 0.
 *  - DS:0x1740 recall latch (5bfb full-recall event): no OpenCol producer — 0.
 */
/*
 * `a654` — resolved 2026-09-06e, and it is NOT a per-unit goal binding.
 * The resident thunk targets FUN_521d_0656, re-disassembled byte-exact from
 * OVL14_L0000:0656 (viceroy_overlays.asm; the canonical decompile of 0656 is
 * register-mangled, which is what produced the earlier "walk the transport
 * chain to its end" reading):
 *
 *   best = -1;
 *   while (unit >= 0) {
 *     t = unit.type(+0x3146);
 *     if (DS:0x523d[t*0xe] & 0x40)                       // settler capability
 *       if (best < 0 || type[best] < t) best = unit;      // strictly-greater
 *     unit = next_stack_member(unit);                     // FUN_1000_84d4
 *   }
 *   return best;
 *
 * i.e. "the highest-typed settler-capable member of this unit's tile stack".
 * Bit 0x40 of the @UNIT capability byte is set for exactly types 0 Colonist,
 * 2 Pioneer and 5 Scout (k_20e6_type_flags); every ship type carries
 * 0x81/0x82/0xa2, so a carrier never selects itself and an empty ship yields
 * -1 — which is why the fold's `-1 < iStack_68` gate is meaningful and not
 * vacuous. OpenCol keeps carried units in cargo_ids rather than on the ship's
 * tile stack, the same substitution ai_euro_20e6_ship_cargo_counts makes.
 */
static const ColonizeUnit* ai_euro_20e6_stack_settler(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* ship
) {
  const ColonizeUnit* best = NULL;
  int best_t = -1;
  const ColonizeUnit* chain[1 + COLONIZE_UNIT_CARGO_MAX];
  int n = 0;
  chain[n++] = ship; /* DOS starts the walk at the unit itself */
  for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[s]);
    if (p && p->active) {
      chain[n++] = p;
    }
  }
  for (int i = 0; i < n; ++i) {
    const int t = ai_euro_20e6_dos_type(ctx->units, chain[i]);
    if (t < 0 || (ai_euro_20e6_type_flags(t) & 0x40) == 0) {
      continue;
    }
    if (t > best_t) {
      best_t = t;
      best = chain[i];
    }
  }
  return best;
}

/*
 * Raw 1746-1762 goal fold, BOTH branches (2026-09-06e; the promote half used
 * to stand in on "the nation has any FOUND/MIL_EXPAND primary goal" and the
 * demote half was unmodelled for want of a per-unit goal binding that, per
 * the a654 decode above, never existed):
 *
 *   rep = a654(ship);                                  // settler-capable rep
 *   if (rep >= 0) {
 *     urgency = 7326(nation, rep, cont) + 72e0(nation);
 *     if (urgency < 1) { if (found_probe == 0) { civ += pio; pio = 0; } }
 *     else             { carry80 = civ; pio += civ; civ = 0; }
 *   }
 *
 * `7326` is FUN_521d_052c (unit_desirability_score, ported) and `72e0` is
 * FUN_521d_03d0 (founding_expansion_urgency, ported) — both identified from
 * the OVL14 body of FUN_521d_0600 = 7326 + 72f9 + 72e0 at offset 0x600.
 * 052c is clamped to <= 0 and 03d0 returns 8 with the default (all-zero)
 * plan scratch, so the demote arm needs a strongly negative unit: it is the
 * lone criminal/servant-shaped transport, not a Pioneer one — exactly the
 * case the earlier "a wrong stand-in would strip founder status from a lone
 * Pioneer transport" warning was protecting.
 * `found_probe` is the caller's 7344(nation, x, y, AI_GOAL_FOUND) probe.
 */
void ai_euro_20e6_goal_fold(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* ship,
  int nation,
  int found_probe,
  int* pioneers,
  int* civ,
  int* carry80
) {
  if (carry80) {
    *carry80 = 0;
  }
  const ColonizeUnit* rep = ai_euro_20e6_stack_settler(ctx, ship);
  if (!rep) {
    return; /* iStack_68 < 0 — no settler-capable member, no fold */
  }
  const int total_colonies = ctx->colonies ? ctx->colonies->colony_count : 0;
  const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  /*
   * DOS 89432: `thunk_FUN_2a1f_053c(0x281f, uVar11, local_68, 0xffff)` — the
   * continent argument is the **0xffff = any** sentinel, not the ship's tile.
   * The port used to pass map_continent_id_at(ship->x, ship->y); the ship sits
   * on water, so that is a water-region id no land colony can match, 15eb_0142
   * always missed and the score was pinned to the +2 miss constant instead of
   * dist/5 - 1. ai_goals_nearest_colony_15eb_0142 spells "any" as continent < 0.
   */
  const int cont = -1;
  /* 052c runs its own FUN_15eb_0142 nearest-own-colony-on-continent search
   * (it owns the DS:0x8db8 write), so no caller-side distance is passed. */
  const int urgency =
    ai_goals_unit_desirability_score(
      ctx->map, ctx->colonies, nation, ship->x, ship->y,
      ai_euro_20e6_dos_type(ctx->units, rep), rep->profession, cont, turn, total_colonies
    ) +
    ai_goals_founding_expansion_urgency(nation, total_colonies);
  if (urgency < 1) {
    if (found_probe == 0) {
      *civ += *pioneers;
      *pioneers = 0;
    }
    return;
  }
  if (carry80) {
    *carry80 = *civ;
  }
  *pioneers += *civ;
  *civ = 0;
}

int ai_euro_20e6_unload_mask(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation) {
  if (!ctx->map || nation < 0 || nation > 3) {
    return 0;
  }
  /*
   * asm 0x3609: `CALLF FUN_1000_8b10` (FUN_1427_10be) sits immediately before
   * the 0d38 stack-count batch below, and LAB_3558 is reached on EVERY ship
   * act — the arrival block's fall-through and the three direct `JMP 0x3558`
   * exits of the md:1691 gate alike. This is that call for the acts that
   * skip the arrival block; the berth path runs its own at the arrival tail
   * (see ai_euro_20e6_transport_assemble). DOS reaches 0x3609 at a colony
   * berth only via the arrival block's md:2991-2997 stale-mark clear, so a
   * mark not set by THIS act's berth scan (e.g. the housekeeping aboard-stamp
   * on a passenger unloaded earlier this turn) never boards; a member the
   * budget could not fit is re-marked by the next berth act's scan, not by a
   * surviving mark.
   */
  ai_euro_20e6_clear_stale_board_marks(ctx, nation, ship);
  (void)ai_euro_20e6_transport_assemble(ctx, nation, ship);
  int pioneers = 0;
  int mil = 0;
  int scouts = 0;
  int milvet = 0;
  int civ = 0;
  ai_euro_20e6_ship_cargo_counts(ctx, ship, &pioneers, &mil, &scouts, &milvet, &civ);
  const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  const int woi = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.game_options.woi : 0;
  const int open_cont = ai_euro_20e6_open_continents(ctx, nation); /* DS:0x9650 */
  /* DS:0x1734[nation] — the real 0a60 registration counter (2026-09-07;
   * replaces the NEEDS_GARRISON/MILITARY colony-count substitute). */
  const int urgency = ai_euro_s_0a60_work_registered[(nation >= 0 && nation < 4) ? nation : 0];
  int home_dist = 0;
  const int home_colony = ai_euro_20e6_nearest_colony(ctx, ship->x, ship->y, nation, -1, &home_dist);
  int any_dist = 0;
  (void)ai_euro_20e6_nearest_colony(ctx, ship->x, ship->y, -1, -1, &any_dist);
  const int probe7 = ai_goals_max_primary_prio(nation, ship->x, ship->y, AI_GOAL_MIL_EXPAND);
  const int probe1 = ai_goals_max_primary_prio(nation, ship->x, ship->y, AI_GOAL_FOUND);
  /*
   * Raw 1746-1762 goal fold, both branches live since 2026-09-06e — see
   * ai_euro_20e6_goal_fold. Positive urgency promotes plain civilians into
   * the founder count (iStack_4a += iStack_82, iStack_80 keeps the old
   * civilian count); urgency < 1 with no FOUND probe demotes Pioneers into
   * civilians. This fold is the DOS mechanism that lets a colonist-only
   * second wave raise 0x40 — it resolves the "# Pioneers vs flag-count
   * founders" conflict noted in the 2026-09-06 pass.
   */
  int carry80 = 0; /* iStack_80 */
  ai_euro_20e6_goal_fold(ctx, ship, nation, probe1, &pioneers, &civ, &carry80);
  /*
   * Raw 1768 sits AFTER the fold, not before it (order corrected 2026-09-06e
   * together with the fold itself): a colonist-only wave arrives with all
   * three counts zero and only becomes a founder cargo once the promote arm
   * has run, so testing the gate first made the promote unreachable for
   * exactly the case the fold exists to serve.
   */
  if (pioneers == 0 && mil == 0 && scouts == 0) {
    return 0;
  }
  int goto_cid = -2;
  if (ship->orders == AI_EURO_ACT_GOAL && ship->goto_x >= 0 && ship->goto_y >= 0 &&
      ship->goto_x < (int)ctx->map->width && ship->goto_y < (int)ctx->map->height) {
    goto_cid = map_continent_id_at(ctx->map, ship->goto_x, ship->goto_y);
  }
  int mask = 0;
  for (int d = 0; d < 8; ++d) {
    const int ax = ship->x + MAP_DIR8_DX[d];
    const int ay = ship->y + MAP_DIR8_DY[d];
    if (!map_coords_inset(ctx->map, ax, ay)) {
      continue; /* 84f2 */
    }
    if (map_tile_is_water(ctx->map, ax, ay)) {
      continue; /* 8958 */
    }
    const int pres = ai_euro_20e6_tribe_or_presence(ctx, ax, ay);
    if (!(pres < 0 || pres == nation)) {
      continue; /* 88c2 own/empty */
    }
    /* FUN_521d_20e6 raw 89470: even a shore rejected by the stance or
     * latitude gate clears the previous neighbour's mask (bugs.md #530). */
    mask = 0;
    const int cid = map_continent_id_at(ctx->map, ax, ay);
    if (cid < 0 || ai_euro_continent_stance_at(nation, cid) == 0) {
      continue; /* DS:0x9870 G-table zero */
    }
    if (!(ay > 1 && ay <= ctx->map->height - 3)) {
      continue;
    }
    if (goto_cid >= 0 && goto_cid == cid) {
      mask = 0xffff; /* act_state 0x0b goto lands here: unload everything */
    }
    const int own_cols = ai_euro_20e6_own_colonies_on(ctx, nation, cid);
    const int own_units = ai_euro_10ec_land_units_on(ctx, nation, cid);
    const int tally_b = (ctx->col1_ok && ctx->col1 && cid < 16)
                          ? (int)ctx->col1->post_map.continent_tally_b[cid]
                          : 0; /* −0x7a38 land-tile count */
    /* Raw 1790: gated on iStack_16 = mode 5 = # Scouts aboard (byte-exact
     * 0d38 case-5 body counts type 5 only — Missionaries do not qualify). */
    if (scouts != 0 && ((own_cols == 0 && tally_b > 10) || own_units < (tally_b >> 3))) {
      mask |= 0x20;
    }
    /* Raw 1799: gated on iStack_4a = mode 3 = # Pioneers aboard (case-3 body
     * `cmp type,2` — DOS founds with Pioneers; the flag-count reading that
     * admitted plain Colonists here is retired). */
    if (pioneers != 0) {
      if (ai_goals_colony_balance_flags_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, nation, cid) > 0) {
        mask |= 0x40;
      }
      if (own_units == 0) {
        mask |= 0x40;
      }
      int ok98 = 1;
      if (home_colony >= 0 && cid >= 0) {
        const ColonizeColony* hc = colonies_get(ctx->colonies, home_colony);
        if (hc && map_continent_id_at(ctx->map, hc->x, hc->y) == cid) {
          const int iv = ai_euro_20e6_own_colonies_on(ctx, nation, cid) - 8;
          if (-home_dist != iv && home_dist <= -iv) {
            mask &= ~0x40; /* too close to an existing cluster */
          }
          if (home_dist > 0xb) {
            ok98 = 0;
          }
        }
      }
      if (ok98 && open_cont != 0 && (turn >> 4) < own_cols * 4 + own_units && urgency < 0x14) {
        mask &= ~0x40;
      }
      /* Raw 1825: iStack_80 (promoted-civilian carry from the goal fold) ∧
       * per-continent explorer count −0x5ec4[cid] > 1 → clear 0x40: enough
       * explorers already ashore here, keep the promoted civilians aboard. */
      if (carry80 != 0 && cid >= 0 && cid < 16 && ai_euro_s_20e6_explorers[cid] > 1) {
        mask &= ~0x40;
      }
      if (probe7 != 0) {
        mask |= 0x40;
      }
      if (probe1 != 0) {
        mask |= 0x40;
      }
      if (ai_euro_20e6_goal_on_continent(nation, cid, 0)) {
        mask |= 0x40; /* DS:0x173e */
      }
    }
    if (mil != 0 && woi && ctx->colonies) {
      if (ai_euro_20e6_own_colonies_on(ctx, ctx->human_nation, cid) != 0) {
        mask |= 0x10; /* WoI: human holds colonies here */
      }
    }
    if (mil != 0 && !woi) {
      if (ai_euro_continent_stance_at(nation, cid) == 4) {
        mask |= 0x10; /* war stance */
      }
      /* Raw 1846: `iStack_46 == 0 && …` clears 0x10. iStack_46 = mode 6
       * (mil-or-vet count) is a superset count of mode 4 over the same
       * stack, and the wartime Artillery add to iStack_48 only happens
       * inside the 0x5382-bit0 branch — so within this peace branch
       * mil != 0 ⇒ milvet != 0 and the clear NEVER fires in DOS. Kept
       * wired with the real count (2026-09-06 case-6 decode). */
      if (milvet == 0 && open_cont != 0 && (turn >> 4) < own_cols * 4 + own_units &&
          urgency < 0x14) {
        mask &= ~0x10;
      }
      if (own_cols == 0 && ai_euro_20e6_foreign_colony_on(ctx, nation, cid) && any_dist < 7) {
        mask |= 0x10; /* −0x6a0e bit4 + close colony */
      }
      /* −0x6a0e bit 0x08: writer undecoded — no term. */
    }
    if (mil != 0) {
      if (probe7 != 0 || probe1 != 0) {
        mask |= 0x10;
      }
      if (ai_euro_20e6_goal_on_continent(nation, cid, 1)) {
        mask |= 0x10; /* DS:0x173c */
      }
    }
  }
  /* DS:0x1740 recall latch: absent. */
  return mask;
}

/*
 * LAB_3558 unload loop (raw ~1889-1928, decomp ~89566-89609): every carried
 * land unit whose DS:0x523d type-flag byte intersects the mask steps ashore;
 * the tile comes from thunk 2a1f_04ac → FUN_521d_06ae (the founding-tile
 * pick — dir arg mask&0x40 = founder mode, plus an is-Artillery flag).
 * Repeats until a
 * pass unloads nothing (DOS rescans the stack whenever a unit left the
 * tile). Returns the number of units put ashore.
 */
/*
 * DOS-LITERAL raw 89566-89609 unload loop:
 *   do {
 *     moved = 0; dir = -1;
 *     for (unit in tile chain, head first; dir != 8 && !moved) {
 *       if (unit is a hull) continue;
 *       dir = 04ac(nation, x, y, mask & 0x40, type == 0x0b);     // 06ae
 *       if ((DS:0x523d[type] & mask) && dir != 8) {
 *         +0x3149 = 0; 2a1f_0150(unit, dir); 0934(unit);         // step, exhaust
 *         if (unit left the tile) moved = 1;
 *       }
 *     }
 *   } while (moved);
 * The chain is newest-first (TURN2->3: the Dutch Soldier, boarded last,
 * takes (48,14) before the Pioneer; the French Soldier's landing blocks the
 * Pioneer's only tile), i.e. `cargo_ids` walked backwards. A passenger whose
 * 06ae finds no tile ends the pass even when its own flags do not match.
 */
int ai_euro_20e6_unload_by_mask(
  ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation, int mask
) {
  int total = 0;
  int moved;
  do {
    moved = 0;
    int dir8 = 0;
    for (int s = ship->cargo_count - 1; s >= 0 && !dir8 && !moved; --s) {
      if (s >= COLONIZE_UNIT_CARGO_MAX) {
        continue;
      }
      const int pid = ship->cargo_ids[s];
      ColonizeUnit* p = units_get(ctx->units, pid);
      if (!p || !p->active) {
        continue;
      }
      const int t = ai_euro_20e6_dos_type(ctx->units, p);
      if (t >= 0xd && t <= 0x12) {
        continue;
      }
      const ColonizeWorld w06 = {
        .units = ctx->units, .colonies = ctx->colonies, .map = ctx->map,
        .col1 = ctx->col1_ok ? ctx->col1 : NULL, .col1_ok = ctx->col1_ok && ctx->col1 != NULL
      };
      int lx = 0;
      int ly = 0;
      if (!ai_goals_pick_founding_tile_ex_w(
            &w06, nation, ship->x, ship->y, (mask & 0x40) != 0, t == 0x0b, &lx, &ly
          )) {
        dir8 = 1;
        continue;
      }
      if ((ai_euro_20e6_type_flags(t) & mask & 0xff) == 0) {
        continue;
      }
      if (ai_euro_unload_pax_at(ctx, ship, p, lx, ly, UNITS_ORDER_SENTRY, p->goto_x, p->goto_y)) {
        p = units_get(ctx->units, pid);
        if (p) {
          p->moves = 0; /* FUN_281f_0934 */
        }
        total++;
        moved = 1;
      }
    }
  } while (moved);
  return total;
}

/*
 * DS:0x5235[type] — the @UNIT DEFENSE column, not holds. The @UNIT loader
 * (FUN_5b66_0eee raw 121108-121133) reads the NAMES row strictly in file
 * order: name ptr -> 0x5230, icon -> 0x5232, moves*3 -> 0x5234, attack ->
 * 0x5236, defense -> 0x5235, holds -> 0x5237, size -> 0x5238. So the three
 * FUN_521d_20e6 reads of `type * 0xe + 0x5235` (raw 89685-89687 `(x-10)*2`
 * with gate type < 0x11 -- land units included; raw 89801-89803 `(x-10)*8`
 * with gate type < 0x10; and the same pair in the LAB_3558 colony-sail
 * matrix, raw 86375 / 86506 / 86512) all take DEFENSE. This used to return
 * the holds column, which flattened every land type to 0 (Dragoon scored
 * (0-10)*2 = -20 instead of (3-10)*2 = -14) and under-read every warship.
 * Read from the loaded @UNIT catalog rather than a hardcoded table: the
 * kind id IS the @UNIT row (see ai_euro_5d04_dos_type_of).
 */
int ai_euro_20e6_unit_col5(const ColonizeUnitPool* pool, int dos_type) {
  const int ti = pool ? units_kind_type_index(pool, (ColonizeUnitKind)dos_type) : -1;
  const ColonizeUnitType* t = ti >= 0 ? units_type(pool, ti) : NULL;
  return t ? t->defense : 0;
}

/*
 * LAB_3558 colony-sail matrix (md:1933-2031) — full structural port
 * 2026-09-06, replacing the thin Series-O score for this call path.
 * Loop: own colonies not on the ship's tile; carrying Pioneers (iStack_b4)
 * additionally requires nonzero G-stance on the colony's continent (raw
 * 1946). Peace score (no military cargo):
 *   rng(0,8) + ((17 − min(pop,16))² + 2)*4 − (pop − wanted)*2
 *   +0x14 when the human holds colonies on that continent
 *   ±0x19 on the colony-wants-colonists bit (+0x1b bit 0x10 =
 *     COLONIZE_COLONY_AI_NEEDS_COLONISTS): +0x19 set, −0x19 clear. (The
 *     "else −0x25" this header used to claim was a hex/decimal slip in the
 *     SBB idiom; corrected in code and here — prior smell audit #95.)
 * War-cargo score (military cargo aboard): G-stance 0 → skip colony;
 *   − difficulty × mil × 8 when mil > 1 (DS:0xa89c)
 *   +0x32 when the human holds colonies on the continent
 *   +0x1b bit 0x40 (NEEDS_GARRISON) → +0x3c; else when open continents < 2
 *   or urgency > 0x13: bit 0x08 → +0x2d else −0xf; else −0x2d
 *   ((−0x6a0e[cid] & 7) * 8 presence term omitted — writer undecoded)
 * Shared: + idle timer (+0x8f = cargo_idle_turns); +0x1b bit 0x02
 *   (NEARBY_FRIGATE) ∧ ship != Frigate → −0x32; else bit 0x01
 *   (NEARBY_ARMED_SHIP) ∧ ship type < 0x11 → +(col5 − 10)*2;
 *   − ((dist >> 1) + 1); later ties win (DOS <=).
 * Non-coastal colonies are skipped (the `FUN_1000_8804(...,0xfffe)`
 * reachability probe for the +0x1c bit-0x40-clear case — a ship can't
 * reach them; re-anchored 2026-09-24 to move_scoring_20e6_full.md:2013 —
 * FUN_1000_8804 is an out-of-segment callee with no body in
 * viceroy_unpacked.c/_2.c, so "raw 8804" is the call-site's decompiled
 * function offset, not a viceroy_unpacked.c line number).
 * Commit threshold (md:2031): peace best > −999, war-cargo best > 0.
 * +0x1b bit 0x08 (SHORT_DEFENDERS) is written by ai_euro_colony_threat_seed_5952;
 * bit 0x04 (MILITARY_SURPLUS) is the opposite side of that pair.
 */
int ai_euro_20e6_colony_sail_pick(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship, int nation, int mil, int pioneers_b4,
  int urgency, int* out_x, int* out_y
) {
  if (!ctx || !ctx->colonies || !ctx->map || nation < 0 || nation > 3) {
    return 0;
  }
  const int ship_type = ai_euro_20e6_dos_type(ctx->units, ship);
  /*
   * DS:0xa89c, raw 93110-93115: the COUNT of continents whose DS:0x95f2 byte
   * has bit 8 set (this nation has a dug-in field force there), recounted at
   * the head of every per-nation AI pass. Until 2026-09-10 this line read
   * `head.difficulty`, which is a different DS byte entirely — the writer was
   * simply unlocated. See ai_contact_continent_war_count_a89c.
   */
  const int war_cont_count = ai_contact_continent_war_count_a89c(ctx, nation);
  const int open_cont = ai_euro_20e6_open_continents(ctx, nation);
  int best = -9999;
  int have = 0;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation) {
      continue;
    }
    if (c->x == ship->x && c->y == ship->y) {
      continue;
    }
    const int cid = map_continent_id_at(ctx->map, c->x, c->y);
    if (pioneers_b4 != 0 && (cid < 0 || ai_euro_continent_stance_at(nation, cid) == 0)) {
      continue; /* md:1946: Pioneer cargo only sails at nonzero stance */
    }
    if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
      continue; /* 8804(...,0xfffe) reachability substitution */
    }
    int wanted = ai_euro_colony_wanted_size(ctx->colonies, c); /* FUN_1000_8e6c */
    if (wanted > 0xc) {
      wanted = 0x10; /* md:1949-1951 clamp */
    }
    int score;
    if (mil == 0) {
      int popc = (int)c->population;
      if (popc > 0x10) {
        popc = 0x10;
      }
      score = (ctx->rng ? dos_rng_range(ctx->rng, 0, 8) : 0) +
              ((0x11 - popc) * (0x11 - popc) + 2) * 4 - ((int)c->population - wanted) * 2;
      if (cid >= 0 && ai_euro_20e6_own_colonies_on(ctx, ctx->human_nation, cid) != 0) {
        score += 0x14;
      }
      score += (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) ? 0x19 : -0x19;
    } else {
      if (cid < 0 || ai_euro_continent_stance_at(nation, cid) == 0) {
        continue; /* md:1970: war cargo needs nonzero stance */
      }
      score = 0;
      /*
       * Raw 89654-89656: `local_58 = (*(byte *)(FUN_281f_0722(colony) +
       * -0x6a0e) & 7) * 8; local_28 += local_58;` — the DS:0x95f2 presence
       * byte for the CANDIDATE colony's continent, low three bits only, so
       * natives (1) + a foreign Euro unit (2) + a foreign colony (4) each add
       * 8, up to +56. All four bit writers are decoded (raw 78167/78235/
       * 78302/78312), so the term is live since 2026-09-10; it used to be a
       * "writer undecoded" comment with no term at all. Bit 8 is masked off
       * here — it reaches this scorer only through war_cont_count.
       */
      score += (ai_contact_continent_presence_4962(ctx, nation, cid) & 7) * 8;
      /* Raw 89660-89662, verbatim shape: `if (a89c != 0 && 1 < mil)`. */
      if (war_cont_count != 0 && mil > 1) {
        score += war_cont_count * mil * -8;
      }
      if (ai_euro_20e6_own_colonies_on(ctx, ctx->human_nation, cid) != 0) {
        score += 0x32;
      }
      if (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_GARRISON) {
        score += 0x3c;
      } else if (open_cont < 2 || urgency > 0x13) {
        /* Raw 89663: DOS tests +0x1b & 8 here (short-of-defenders), not & 4. */
        score += (c->ai_flags & COLONIZE_COLONY_AI_SHORT_DEFENDERS) ? 0x2d : -0xf;
      } else {
        score -= 0x2d;
      }
    }
    score += (int)(int8_t)c->cargo_idle_turns; /* +0x8f, DOS reads signed byte */
    if (c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) {
      if (ship_type != 0x11) {
        score -= 0x32;
      }
    } else if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP) && ship_type < 0x11) {
      score += (ai_euro_20e6_unit_col5(ctx->units, ship_type) - 10) * 2;
    }
    const int dist = map_dos_dist(ship->x - c->x, ship->y - c->y);
    score -= (dist >> 1) + 1; /* iStack_b2 == 1 */
    if (score >= best) { /* DOS later-ties-win */
      best = score;
      have = 1;
      bx = c->x;
      by = c->y;
    }
  }
  if (!have || best <= (mil == 0 ? -999 : 0)) {
    return 0; /* md:2031 commit threshold */
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/* --- 20e6 unload/settle: stage helpers --------------------------------- */

/*
 * First-colony found approach (TURN3→4): landfall-keyed found tile. Call before
 * the 20e6 scoring gate so settlers are not FOUND-yanked into Braves.
 * Skips beachhead tip acts (TURN2→3) — only after pioneer stays aboard (FR) or
 * cargo is empty (SP/DU). Cite: test-saves-ai/TURN3-4.
 */

/* --- first-colony landing: stage helpers ------------------------------- */

/* ========================================================================
 * FUN_521d_20e6 / FUN_521d_0a60 per-unit act — stage split (2026-09-16)
 *
 * `ai_euro_unit_act` below used to carry the whole act body (~2060 lines) in
 * one function. It is now a driver that runs the DOS stages in the same order,
 * as a chain of static per-stage helpers. The split is purely structural: no
 * statement was reordered, no gate changed, and every RNG-drawing call keeps
 * its position in the stream.
 *
 * Stage order (each helper's own header names its DOS citation):
 *   head (in ai_euro_unit_act)    guards, 049e notify, roam-abort, coastal
 *                                 embark / mil unload, MoW sail-home, early
 *                                 20e6 move-scoring gate, on-colony admit
 *   ai_euro_act_ship_dos          ship act (20e6 hull path + 479b walk)
 *   ai_euro_act_land              Treasure/Missionary/Wagon land band
 *
 * Locals that cross stage boundaries live in `struct ai_euro_act_ctx`; each
 * helper aliases the ones it needs at entry and writes the mutated ones back
 * at its fall-through exit. Control flow that used to be a bare `return;`
 * inside the body is carried out as `AI_EURO_ACT_RETURN` and honoured by the
 * caller, so an early bail still ends the act exactly where it did before.
 * ======================================================================== */

#include "core/ai_euro_internal.h" /* AiEuroActStatus, struct ai_euro_act_ctx */
