#include "core/ai_diplo.h"

#include "core/ai_popup.h"
#include "core/colony.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * FUN_15b3_* bilateral Euro×Euro bytes + 6d8e timers + 5bfb war/ally.
 * Thin map: original_sources_annotated/ai/euro_diplo.md
 *
 * Peer flags: `nation[a].euro_relation[b]` (DS −0x77c4 / Col1 mapped).
 * Do NOT use unknown26[4..7] for flags — those save bytes are unrelated and
 * false-triggered WAR (Privateer spam on seed-100 TURN1→2).
 * Remaining unknown26 Linux stand-ins (timers / sticky / privateer mask):
 *   [0..3] treaty timers  [8] Indian sticky  [9] Privateer spawn mask
 * Indian×Euro full 15b3 matrix still PORT DEBT (thin feeler / war-hit / sticky).
 * Phase 1 deepen (T3 roadmap): unmet euro_relation==0 no longer stamped;
 * relation==0 is unmet not war; peaceful meet floor 96 (seed-100 TURN3+).
 */

#define AI_DIPLO_FLAG_BASE 4
#define AI_DIPLO_INDIAN_HOSTILE_STICKY 8
#define AI_DIPLO_PRIVATEER_SPAWN_SLOT 9
/* Off-map Europe tile (turn / ai_euro Europe gate x|y >= 200). */
#define AI_DIPLO_EUROPE_X 236
#define AI_DIPLO_EUROPE_Y 236
/* Same gate as ai_euro_in_europe — naval war hunt skips Europe until HS. */
#define AI_DIPLO_IN_EUROPE(x, y) ((x) >= 200 || (y) >= 200)

/* Thin FUN_5bfb_153e stand-in: treasury + tax friction on war declare;
 * unpark #5 deepens military score + colony-gap trade sting + Tools embargo.
 * FA 3f41 full body/UI PARKED - thin ally-aid + FA gift + break trust.
 * War trade embargo: OR all 16 @CARGO cargos (Food + Sugar + Tobacco + Cotton
 * + Furs + Lumber + Ore + Silver + Horses + Rum + Cigars + Cloth + Coats +
 * Trade Goods + Tools + Muskets) into nation.boycott_bitmap (idx 0..15;
 * colony.h / NAMES.TXT). Sugar uses the same bit1 as king refuse
 * (ai_king AI_KING_BOYCOTT_CARGO_BIT) for consistency — shared boycott_bitmap
 * path; lift on peace may clear a lingering king Sugar bit while unknown46[2]
 * still holds tax refuse (thin stand-in). Cotton = COLONIZE_CARGO_COTTON (R11
 * leftover). Tools always OR'd on first declare (R10); colony-gap ≥2 still
 * drains the extra rich-side trade sting. Full per-rival 153e PARKED. */

#define AI_DIPLO_WAR_GOLD_STING 100u
#define AI_DIPLO_WAR_TAX_BUMP 1u
#define AI_DIPLO_WAR_TAX_CAP 75u
#define AI_DIPLO_WAR_UPKEEP_GOLD 5u
/* PARKED accuracy debt: null-units treasury stand-in only; do not change rate. */
#define AI_DIPLO_PRIVATEER_PRIZE_GOLD 8u
#define AI_DIPLO_WAR_FOOD_EMBARGO_BIT (1u << COLONIZE_CARGO_FOOD)
#define AI_DIPLO_WAR_EMBARGO_CARGO_BIT (1u << COLONIZE_CARGO_FURS)
#define AI_DIPLO_WAR_TOBACCO_EMBARGO_BIT (1u << COLONIZE_CARGO_TOBACCO)
#define AI_DIPLO_WAR_SUGAR_EMBARGO_BIT (1u << COLONIZE_CARGO_SUGAR)
#define AI_DIPLO_WAR_COTTON_EMBARGO_BIT (1u << COLONIZE_CARGO_COTTON)
#define AI_DIPLO_WAR_LUMBER_EMBARGO_BIT (1u << COLONIZE_CARGO_LUMBER)
#define AI_DIPLO_WAR_HORSES_EMBARGO_BIT (1u << COLONIZE_CARGO_HORSES)
#define AI_DIPLO_WAR_RUM_EMBARGO_BIT (1u << COLONIZE_CARGO_RUM)
#define AI_DIPLO_WAR_CIGARS_EMBARGO_BIT (1u << COLONIZE_CARGO_CIGARS)
#define AI_DIPLO_WAR_CLOTH_EMBARGO_BIT (1u << COLONIZE_CARGO_CLOTH)
#define AI_DIPLO_WAR_COATS_EMBARGO_BIT (1u << COLONIZE_CARGO_COATS)
#define AI_DIPLO_WAR_ORE_EMBARGO_BIT (1u << COLONIZE_CARGO_ORE)
#define AI_DIPLO_WAR_SILVER_EMBARGO_BIT (1u << COLONIZE_CARGO_SILVER)
#define AI_DIPLO_WAR_TRADE_GOODS_EMBARGO_BIT (1u << COLONIZE_CARGO_TRADE_GOODS)
#define AI_DIPLO_WAR_TOOLS_EMBARGO_BIT (1u << COLONIZE_CARGO_TOOLS)
#define AI_DIPLO_WAR_MUSKETS_EMBARGO_BIT (1u << COLONIZE_CARGO_MUSKETS)
#define AI_DIPLO_WAR_TRADE_STING 25u
#define AI_DIPLO_WAR_COLONY_GAP 2
/* First declare: seed peer treaty timer so near-parity peace waits for
 * timer==0 (war aged / fatigue). Reuses unknown26[0..3]; live timers kept. */
#define AI_DIPLO_WAR_FATIGUE_TIMER 8u
#define AI_DIPLO_ALLY_GOLD_COST 25u
#define AI_DIPLO_ALLY_TREATY_MIN 8u
#define AI_DIPLO_ALLY_AID_GOLD 10u
#define AI_DIPLO_ALLY_AID_MIN_TREASURY 50u
#define AI_DIPLO_FA_GIFT_GOLD 15u
#define AI_DIPLO_FA_GIFT_MIN_TREASURY 100u
#define AI_DIPLO_FA_GIFT_TIMER_BUMP 2u
/* Ally longevity when FA gift gold gates fail: timer+1 both dirs (no gold). */
#define AI_DIPLO_ALLY_LONGEVITY_BUMP 1u
#define AI_DIPLO_BREAK_GOLD_PENALTY 20u
#define AI_DIPLO_INDIAN_DRIFT_CAP 160u
#define AI_DIPLO_WAR_INDIAN_HIT 5
/* At-war gate: relation < 50 (same band as contact alarm≥50 mission block). */
#define AI_DIPLO_INDIAN_AT_WAR_REL 50
/* Very-low deepen: relation < 40 (contact peaceful-gift friction < 40 inverted). */
#define AI_DIPLO_INDIAN_VERY_LOW_REL 40
#define AI_DIPLO_INDIAN_HOSTILE_EXTRA 10
#define AI_DIPLO_INDIAN_HARASS_GOLD 2u
/* Peace feeler / first-meet content floor. Seed-100 TURN3+ write 96 on meet
 * (not 100). Heal mid-band up to this ceiling; drift still climbs to 160.
 * Source: fandom Indians — peace → gifts / improve relations (no large gold). */
#define AI_DIPLO_INDIAN_PEACE_MEET 96u
#define AI_DIPLO_INDIAN_CONTENT_FLOOR AI_DIPLO_INDIAN_PEACE_MEET
#define AI_DIPLO_INDIAN_FEELER_HEAL 2
#define AI_DIPLO_STICKY_CLEAR 0u
#define AI_DIPLO_STICKY_AT_WAR 1u
#define AI_DIPLO_STICKY_DEEP 2u
/*
 * Franklin NW peace: either Euro in the pair owns Benjamin Franklin.
 * Source: docs/fandom_col1994.md — king's European wars no longer affect NW
 * relations; Europeans always offer peace in negotiations.
 */
static int ai_diplo_franklin_pair(const ColonizeCol1Save* col1, int nation_a, int nation_b) {
  return founding_fathers_franklin_keeps_nw_peace(col1, nation_a) ||
         founding_fathers_franklin_keeps_nw_peace(col1, nation_b);
}

static uint8_t* ai_diplo_timer_byte(ColonizeCol1Save* col1, int nation, int peer);
static void ai_diplo_popup_ok(
  ColonizeTurnContext* ctx,
  AiPopupTag tag,
  int nation_a,
  int nation_b,
  const char* title,
  const char* body
);

static void ai_diplo_war_treasury_sting(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    if (nat->gold > AI_DIPLO_WAR_GOLD_STING) {
      nat->gold -= AI_DIPLO_WAR_GOLD_STING;
    } else {
      nat->gold = 0;
    }
  }
}

/* Thin 153e side effect: +1 tax_rate both sides, cap 75 (king tax path). */
static void ai_diplo_war_tax_bump(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    if (nat->tax_rate >= AI_DIPLO_WAR_TAX_CAP) {
      continue;
    }
    uint8_t next = (uint8_t)(nat->tax_rate + AI_DIPLO_WAR_TAX_BUMP);
    if (next > AI_DIPLO_WAR_TAX_CAP) {
      next = (uint8_t)AI_DIPLO_WAR_TAX_CAP;
    }
    nat->tax_rate = next;
  }
}

/* Thin wartime trade embargo: OR all 16 @CARGO boycott bits both nations
 * (Food+Sugar+Tobacco+Cotton+Furs+Lumber+Ore+Silver+Horses+Rum+Cigars+Cloth+
 * Coats+Trade Goods+Tools+Muskets). Sugar = cargo idx 1 (@CARGO /
 * COLONIZE_CARGO_SUGAR) — same bit1 as king refuse (king_ref.md tax boycott).
 * Cotton = idx 3 (COLONIZE_CARGO_COTTON; R11 leftover). Food=0 / Lumber=5 /
 * Horses=8 / Tools=14 / Muskets=15 / Trade Goods=13 / Rum=9 / Cigars=10 /
 * Cloth=11 / Coats=12 / Ore=6 / Silver=7 (colony.h @CARGO / NAMES.TXT).
 * Source: 153e wartime freeze stand-in (Europe boycott_bitmap freezes named
 * cargos); fuller per-rival body PARKED. */
static void ai_diplo_war_embargo_set(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  const uint16_t set =
    (uint16_t)(AI_DIPLO_WAR_FOOD_EMBARGO_BIT | AI_DIPLO_WAR_EMBARGO_CARGO_BIT |
               AI_DIPLO_WAR_TOBACCO_EMBARGO_BIT | AI_DIPLO_WAR_SUGAR_EMBARGO_BIT |
               AI_DIPLO_WAR_COTTON_EMBARGO_BIT | AI_DIPLO_WAR_LUMBER_EMBARGO_BIT |
               AI_DIPLO_WAR_HORSES_EMBARGO_BIT | AI_DIPLO_WAR_RUM_EMBARGO_BIT |
               AI_DIPLO_WAR_CIGARS_EMBARGO_BIT | AI_DIPLO_WAR_CLOTH_EMBARGO_BIT |
               AI_DIPLO_WAR_COATS_EMBARGO_BIT | AI_DIPLO_WAR_ORE_EMBARGO_BIT |
               AI_DIPLO_WAR_SILVER_EMBARGO_BIT | AI_DIPLO_WAR_TRADE_GOODS_EMBARGO_BIT |
               AI_DIPLO_WAR_TOOLS_EMBARGO_BIT | AI_DIPLO_WAR_MUSKETS_EMBARGO_BIT);
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap | set);
  }
}

/* Count owned Col1 colonies for nation (153e trade-score stand-in). */
static int ai_diplo_col1_colony_count(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || !col1->colony || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  int n = 0;
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    if ((int)col1->colony[i].nation_id == nation_id) {
      n++;
    }
  }
  return n;
}

/*
 * Unpark #5 153e trade deepen: if |colony_count_a − colony_count_b| ≥ 2,
 * drain AI_DIPLO_WAR_TRADE_STING from the richer treasury (floor 0). Tools
 * embargo is OR'd on every first declare (war_embargo_set); gap deepen keeps
 * the rich-side gold sting only. Full per-rival trade dialog PARKED.
 */
static void ai_diplo_war_trade_score_sting(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1 || nation_a < 0 || nation_a >= 4 || nation_b < 0 || nation_b >= 4) {
    return;
  }
  const int ca = ai_diplo_col1_colony_count(col1, nation_a);
  const int cb = ai_diplo_col1_colony_count(col1, nation_b);
  int gap = ca - cb;
  if (gap < 0) {
    gap = -gap;
  }
  if (gap < AI_DIPLO_WAR_COLONY_GAP) {
    return;
  }
  ColonizeCol1Nation* a = &col1->nation[nation_a];
  ColonizeCol1Nation* b = &col1->nation[nation_b];
  ColonizeCol1Nation* rich = (a->gold >= b->gold) ? a : b;
  if (rich->gold > AI_DIPLO_WAR_TRADE_STING) {
    rich->gold -= AI_DIPLO_WAR_TRADE_STING;
  } else {
    rich->gold = 0;
  }
}

/*
 * Lift all 16 wartime @CARGO embargo bits (Food+…+Cotton+…+Tools+Muskets)
 * when a nation has no remaining Euro×Euro wars. Tools bit is OR'd on every
 * first declare (with the other wartime cargos); Cotton (R11 leftover) joins
 * the shared set/lift mask. Peace/alliance clear all sixteen via this mask.
 * Sugar shares king refuse bit1 (see war_embargo_set). Source: thin 153e trade
 * embargo stand-in; Fugger FF forgives boycotts — full 153e PARKED. Call sites:
 * make_peace, form_alliance (clears WAR). Raw PEACE writes do not.
 */
static void ai_diplo_war_embargo_lift_if_peace(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  const uint16_t lift = (uint16_t)(AI_DIPLO_WAR_FOOD_EMBARGO_BIT |
                                   AI_DIPLO_WAR_EMBARGO_CARGO_BIT |
                                   AI_DIPLO_WAR_TOBACCO_EMBARGO_BIT |
                                   AI_DIPLO_WAR_SUGAR_EMBARGO_BIT |
                                   AI_DIPLO_WAR_COTTON_EMBARGO_BIT |
                                   AI_DIPLO_WAR_LUMBER_EMBARGO_BIT |
                                   AI_DIPLO_WAR_HORSES_EMBARGO_BIT |
                                   AI_DIPLO_WAR_RUM_EMBARGO_BIT |
                                   AI_DIPLO_WAR_CIGARS_EMBARGO_BIT |
                                   AI_DIPLO_WAR_CLOTH_EMBARGO_BIT |
                                   AI_DIPLO_WAR_COATS_EMBARGO_BIT |
                                   AI_DIPLO_WAR_ORE_EMBARGO_BIT |
                                   AI_DIPLO_WAR_SILVER_EMBARGO_BIT |
                                   AI_DIPLO_WAR_TRADE_GOODS_EMBARGO_BIT |
                                   AI_DIPLO_WAR_MUSKETS_EMBARGO_BIT |
                                   AI_DIPLO_WAR_TOOLS_EMBARGO_BIT);
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    if (ai_diplo_at_war_with_any(col1, n)) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap & (uint16_t)~lift);
  }
}

/*
 * War fatigue: on first declare, if peer treaty timer is 0, seed it to 8 so
 * euro_balance near-parity peace waits until timer==0 (war aged). Live timers
 * left alone. Source: reuse unknown26[0..3] 6d8e timer; no new unknown26 slot.
 */
static void ai_diplo_war_fatigue_timer_seed(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  uint8_t* ta = ai_diplo_timer_byte(col1, nation_a, nation_b);
  uint8_t* tb = ai_diplo_timer_byte(col1, nation_b, nation_a);
  if (ta && *ta == 0) {
    *ta = (uint8_t)AI_DIPLO_WAR_FATIGUE_TIMER;
  }
  if (tb && *tb == 0) {
    *tb = (uint8_t)AI_DIPLO_WAR_FATIGUE_TIMER;
  }
}

/* Per-turn light upkeep while at war (euro_balance); floor 0. */
static void ai_diplo_war_upkeep_drain(ColonizeCol1Nation* nat) {
  if (!nat || nat->gold == 0) {
    return;
  }
  if (nat->gold > AI_DIPLO_WAR_UPKEEP_GOLD) {
    nat->gold -= AI_DIPLO_WAR_UPKEEP_GOLD;
  } else {
    nat->gold = 0;
  }
}

/*
 * Hunt-ready spawn: New World water so ai_euro naval war hunt can aim
 * (!ai_euro_in_europe). Europe dock (x|y>=200) is intentional last resort -
 * hunt waits for Europe->HS teleport. Cite: euro_unit_act §2b Privateer hunt.
 */
static int ai_diplo_privateer_spawn_hunt_ready(
  const ColonizeTurnContext* ctx,
  int x,
  int y
) {
  if (AI_DIPLO_IN_EUROPE(x, y)) {
    return 0;
  }
  if (ctx && ctx->map && !map_tile_is_water(ctx->map, x, y)) {
    return 0;
  }
  return 1;
}

/*
 * Find water near own coastal colony, else stack on own New World sea unit,
 * else Europe dock. Skip Europe-dock / non-water stacks so euro hunt can use
 * the ship same tick when coast water exists.
 * Source: FF Jones coastal-water helper; Europe purchase Privateer; fandom
 * Drake / Privateer commerce raid (euro_unit_act §2b).
 */
static int ai_diplo_find_privateer_spawn(
  const ColonizeTurnContext* ctx,
  int nation_id,
  int* out_x,
  int* out_y
) {
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  if (!ctx || !out_x || !out_y || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  if (ctx->map && ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* col = &ctx->colonies->colonies[i];
      if (!col->active || col->nation_id != nation_id) {
        continue;
      }
      if (!map_tile_is_coastal(ctx->map, col->x, col->y)) {
        continue;
      }
      for (int d = 0; d < 8; ++d) {
        const int nx = col->x + dx[d];
        const int ny = col->y + dy[d];
        if (map_tile_is_water(ctx->map, nx, ny) &&
            ai_diplo_privateer_spawn_hunt_ready(ctx, nx, ny)) {
          *out_x = nx;
          *out_y = ny;
          return 1;
        }
      }
    }
  }
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id || !units_is_on_map(u)) {
        continue;
      }
      if (!units_is_sea(ctx->units, u->id)) {
        continue;
      }
      /* Skip Europe-dock stacks - not hunt-ready until HS teleport. */
      if (!ai_diplo_privateer_spawn_hunt_ready(ctx, u->x, u->y)) {
        continue;
      }
      *out_x = u->x;
      *out_y = u->y;
      return 1;
    }
  }
  /* No hunt-ready coast / ship: commission from Europe dock (off-map). */
  *out_x = AI_DIPLO_EUROPE_X;
  *out_y = AI_DIPLO_EUROPE_Y;
  return 1;
}

static int ai_diplo_privateer_spawn_armed(
  const ColonizeCol1Save* col1,
  int nation_id,
  int peer
) {
  if (!col1 || nation_id < 0 || nation_id >= 4 || peer < 0 || peer >= 4) {
    return 0;
  }
  return (col1->nation[nation_id].privateer_spawn_mask &
          (uint8_t)(1u << peer)) != 0;
}

static void ai_diplo_privateer_spawn_set(
  ColonizeCol1Save* col1,
  int nation_id,
  int peer
) {
  if (!col1 || nation_id < 0 || nation_id >= 4 || peer < 0 || peer >= 4) {
    return;
  }
  col1->nation[nation_id].privateer_spawn_mask =
    (uint8_t)(col1->nation[nation_id].privateer_spawn_mask |
              (uint8_t)(1u << peer));
}

static void ai_diplo_privateer_spawn_clear(
  ColonizeCol1Save* col1,
  int nation_id,
  int peer
) {
  if (!col1 || nation_id < 0 || nation_id >= 4 || peer < 0 || peer >= 4) {
    return;
  }
  col1->nation[nation_id].privateer_spawn_mask =
    (uint8_t)(col1->nation[nation_id].privateer_spawn_mask &
              (uint8_t)~(1u << peer));
}

/*
 * Wartime Privateer unit spawn (once per war peer via unknown26[9] bit).
 * units_find_type("Privateer") + units_spawn_allow_stack near coast / Europe.
 * No-op if type missing, units null, already commissioned, or spawn fails.
 * Returns 1 on successful spawn. Source: Europe Privateer purchase; fandom
 * Drake Privateer combat; euro_unit_act §2b commerce raid.
 * Accuracy: this is the real wartime Privateer path when ctx->units is set;
 * cargo-raid loot stays with ai_euro naval combat (FUN_5fef hold plunder not
 * wired here) — do not invent a diplo gold rate.
 */
static int ai_diplo_war_privateer_spawn(
  ColonizeTurnContext* ctx,
  int nation_id,
  int peer
) {
  if (!ctx || !ctx->col1 || !ctx->units || nation_id < 0 || nation_id >= 4 ||
      peer < 0 || peer >= 4 || nation_id == peer) {
    return 0;
  }
  if (ai_diplo_privateer_spawn_armed(ctx->col1, nation_id, peer)) {
    return 0;
  }
  const int ty = units_find_type(ctx->units, "Privateer");
  if (ty < 0) {
    return 0;
  }
  int sx = 0;
  int sy = 0;
  if (!ai_diplo_find_privateer_spawn(ctx, nation_id, &sx, &sy)) {
    return 0;
  }
  /*
   * Read-only hunt-ready check (ai_euro naval war hunt needs !in_europe +
   * water). If finder returned a bad New World tile, refuse rather than arm
   * unknown26[9] on a land/invalid spawn. Europe dock (not hunt-ready) is OK.
   */
  if (!AI_DIPLO_IN_EUROPE(sx, sy) &&
      !ai_diplo_privateer_spawn_hunt_ready(ctx, sx, sy)) {
    return 0;
  }
  const int sid = units_spawn_allow_stack(ctx->units, ty, sx, sy);
  if (sid < 0) {
    return 0;
  }
  ColonizeUnit* ship = units_get(ctx->units, sid);
  if (ship) {
    units_set_nation(ship, nation_id);
    ship->orders = UNITS_ORDER_AI_SAIL;
    /* Station-keep goto (self): Privateer hunt always re-aims in ai_euro. */
    ship->goto_x = sx;
    ship->goto_y = sy;
  }
  ai_diplo_privateer_spawn_set(ctx->col1, nation_id, peer);
  return 1;
}

/*
 * PARKED accuracy debt — thin wartime Privateer treasury stand-in (8g).
 * Intended effect: cargo-raid hold plunder (FUN_5fef_016c / naval combat loot)
 * when a Privateer captures goods; no diplo hold-plunder API is wired, so this
 * richer→poorer 8g transfer is only used when ctx->units is null (no spawn path).
 * Do NOT invent a different gold rate. With units present, euro_balance uses
 * spawn-only + ai_euro hunt/combat instead. Source: Europe Privateer; fandom
 * Drake; euro_unit_act §2b; FUNCTION_CATALOG FUN_5fef_016c.
 */
static int ai_diplo_war_privateer_prize(ColonizeCol1Save* col1, int nation_id, int peer) {
  if (!col1 || nation_id < 0 || nation_id >= 4 || peer < 0 || peer >= 4 || nation_id == peer) {
    return 0;
  }
  ColonizeCol1Nation* self = &col1->nation[nation_id];
  ColonizeCol1Nation* other = &col1->nation[peer];
  ColonizeCol1Nation* donor;
  ColonizeCol1Nation* prize;
  if (self->gold > other->gold) {
    donor = self;
    prize = other;
  } else if (other->gold > self->gold) {
    donor = other;
    prize = self;
  } else {
    return 0;
  }
  if (donor->gold < AI_DIPLO_PRIVATEER_PRIZE_GOLD) {
    return 0;
  }
  donor->gold -= AI_DIPLO_PRIVATEER_PRIZE_GOLD;
  prize->gold += AI_DIPLO_PRIVATEER_PRIZE_GOLD;
  return 1;
}

/* Thin alliance treasury cost: each side pays 25 if able (floor 0). */
static void ai_diplo_ally_treasury_cost(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    if (nat->gold > AI_DIPLO_ALLY_GOLD_COST) {
      nat->gold -= AI_DIPLO_ALLY_GOLD_COST;
    } else {
      nat->gold = 0;
    }
  }
}

/* Ensure treaty timer toward peer is at least 8 if currently 0 (both dirs). */
static void ai_diplo_ally_treaty_timer_bump(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  uint8_t* ta = ai_diplo_timer_byte(col1, nation_a, nation_b);
  uint8_t* tb = ai_diplo_timer_byte(col1, nation_b, nation_a);
  if (ta && *ta == 0) {
    *ta = (uint8_t)AI_DIPLO_ALLY_TREATY_MIN;
  }
  if (tb && *tb == 0) {
    *tb = (uint8_t)AI_DIPLO_ALLY_TREATY_MIN;
  }
}

/* Break-alliance trust loss: −20 gold each side (floor 0). Tax path unused. */
static void ai_diplo_break_trust_penalty(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    if (nat->gold > AI_DIPLO_BREAK_GOLD_PENALTY) {
      nat->gold -= AI_DIPLO_BREAK_GOLD_PENALTY;
    } else {
      nat->gold = 0;
    }
  }
}

/*
 * Break-alliance Indian sticky raise (thin): −5 on all 8 Indian relation slots
 * both sides, then sync sticky. Same scalar as Euro×Euro war Indian hit
 * (no very-low extra −10). When relations were already near the at-war floor
 * (< 50 after hit), sticky rises 0→1 (or deepens). Source: Indians wary of
 * Euro treachery (fandom / euro_diplo war-hit stand-in); full 15b3 PARKED.
 */
static void ai_diplo_break_indian_sticky_raise(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    for (int idx = 0; idx < 8; ++idx) {
      ai_diplo_indian_relation_delta(col1, 4 + idx, n, -AI_DIPLO_WAR_INDIAN_HIT);
    }
    ai_diplo_indian_hostility_sync(col1, n);
  }
}

/*
 * Thin FA / ally-aid stand-in (full 3f41 PARKED): once per euro_balance peer visit,
 * if allied and peer gold < self gold/2 and self gold >= 50, transfer 10 gold to ally.
 */
static void ai_diplo_ally_foreign_aid(ColonizeCol1Save* col1, int nation_id, int peer) {
  if (!col1 || nation_id < 0 || nation_id >= 4 || peer < 0 || peer >= 4 || nation_id == peer) {
    return;
  }
  ColonizeCol1Nation* self = &col1->nation[nation_id];
  ColonizeCol1Nation* other = &col1->nation[peer];
  if (self->gold < AI_DIPLO_ALLY_AID_MIN_TREASURY) {
    return;
  }
  if (other->gold >= self->gold / 2u) {
    return;
  }
  if (self->gold < AI_DIPLO_ALLY_AID_GOLD) {
    return;
  }
  self->gold -= AI_DIPLO_ALLY_AID_GOLD;
  other->gold += AI_DIPLO_ALLY_AID_GOLD;
}

/*
 * Thin FA goodwill gift (full 3f41 body/UI PARKED): separate from ally-aid.
 * When donor gold >= 100 and peer gold < donor*2, transfer 15g and bump both
 * treaty timers +2 (saturate 255). Caller gates on ALLY + timer==1; if this
 * no-ops, euro_balance applies longevity timer+1 (no second gold transfer).
 */
void ai_diplo_fa_gift(ColonizeCol1Save* col1, int from, int to) {
  if (!col1 || from < 0 || from >= 4 || to < 0 || to >= 4 || from == to) {
    return;
  }
  ColonizeCol1Nation* donor = &col1->nation[from];
  ColonizeCol1Nation* peer = &col1->nation[to];
  if (donor->gold < AI_DIPLO_FA_GIFT_MIN_TREASURY) {
    return;
  }
  if (peer->gold >= donor->gold * 2u) {
    return;
  }
  if (donor->gold < AI_DIPLO_FA_GIFT_GOLD) {
    return;
  }
  donor->gold -= AI_DIPLO_FA_GIFT_GOLD;
  peer->gold += AI_DIPLO_FA_GIFT_GOLD;
  uint8_t* ta = ai_diplo_timer_byte(col1, from, to);
  uint8_t* tb = ai_diplo_timer_byte(col1, to, from);
  if (ta) {
    unsigned next = (unsigned)*ta + AI_DIPLO_FA_GIFT_TIMER_BUMP;
    *ta = (uint8_t)(next > 255u ? 255u : next);
  }
  if (tb) {
    unsigned next = (unsigned)*tb + AI_DIPLO_FA_GIFT_TIMER_BUMP;
    *tb = (uint8_t)(next > 255u ? 255u : next);
  }
}

/*
 * Ally longevity (13b0/3f41 treaty sustain stand-in): +1 both treaty timers
 * when FA gift gold gates fail. No treasury transfer — avoids double-gift.
 * Source: alliance treaty timer refresh; FA dialog UI PARKED.
 */
static void ai_diplo_ally_longevity_timer(ColonizeCol1Save* col1, int from, int to) {
  if (!col1) {
    return;
  }
  uint8_t* ta = ai_diplo_timer_byte(col1, from, to);
  uint8_t* tb = ai_diplo_timer_byte(col1, to, from);
  if (ta) {
    unsigned next = (unsigned)*ta + AI_DIPLO_ALLY_LONGEVITY_BUMP;
    *ta = (uint8_t)(next > 255u ? 255u : next);
  }
  if (tb) {
    unsigned next = (unsigned)*tb + AI_DIPLO_ALLY_LONGEVITY_BUMP;
    *tb = (uint8_t)(next > 255u ? 255u : next);
  }
}

/*
 * Peaceful Indian×Euro relation drift (not full 15b3 matrix).
 * Per tick: for each of 8 Indian slots already contacted (r>0), if < 160 and
 * Euro not at war → +1 (cap 160). Do not invent contact from r==0 (seed-100
 * early goldens keep relation_by_indian at 0 until meet). Source: 6d8e §4;
 * fandom alarm cools without encroachment.
 */
static void ai_diplo_indian_peaceful_drift(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ai_diplo_at_war_with_any(col1, nation_id)) {
    return;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  for (int i = 0; i < 8; ++i) {
    uint8_t r = nat->relation_by_indian[i];
    if (r == 0) {
      continue; /* unmet — not a drift candidate */
    }
    /*
     * Seed-100 early goldens hold peaceful meet at 96 through TURN7 — do not
     * auto-climb past the meet floor here (feeler/trade own further gains).
     */
    if (r >= AI_DIPLO_INDIAN_PEACE_MEET) {
      continue;
    }
    if (r < AI_DIPLO_INDIAN_DRIFT_CAP) {
      nat->relation_by_indian[i] = (uint8_t)(r + 1u);
    }
  }
}

/*
 * Peace feeler toward Indians (unpark #5 matrix deepen): once per euro_balance,
 * if Euro is at peace with all Euro peers (!ai_diplo_at_war_with_any), each
 * mid/high Indian slot (relation ≥ at-war floor 50 and < content floor 100)
 * heals +2 toward 100. Skip while any Euro×Euro war (same gate as drift).
 * Returns 1 if any slot healed (caller may write human feeler status).
 * Source: fandom Indians — peace → gifts / improve relations; contact trade
 * already uses +2 relation. No gold cost (prefer flags over treasury fiction).
 * Full gift dialog / 15b3 bilateral write PARKED.
 */
static int ai_diplo_indian_peace_feeler(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  if (ai_diplo_at_war_with_any(col1, nation_id)) {
    return 0;
  }
  /*
   * Sticky==2 (very-low deepen) refuses the improve-relations feeler at every
   * call site (matrix tick + make_peace restore). Source: fandom Indians —
   * alarmed/hostile may refuse trade/gifts; contact friction <40 inverted.
   */
  if (ai_diplo_indian_hostility_sticky(col1, nation_id) == AI_DIPLO_STICKY_DEEP) {
    return 0;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  int healed = 0;
  for (int i = 0; i < 8; ++i) {
    uint8_t r = nat->relation_by_indian[i];
    if (r < AI_DIPLO_INDIAN_AT_WAR_REL || r >= AI_DIPLO_INDIAN_CONTENT_FLOOR) {
      continue;
    }
    unsigned next = (unsigned)r + (unsigned)AI_DIPLO_INDIAN_FEELER_HEAL;
    if (next > AI_DIPLO_INDIAN_CONTENT_FLOOR) {
      next = AI_DIPLO_INDIAN_CONTENT_FLOOR;
    }
    nat->relation_by_indian[i] = (uint8_t)next;
    healed = 1;
  }
  return healed;
}

/* Indians dislike Euro×Euro war: −5 on all 8 Indian relation slots (both sides). */
static void ai_diplo_war_indian_relation_hit(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    for (int idx = 0; idx < 8; ++idx) {
      ai_diplo_indian_relation_delta(col1, 4 + idx, n, -AI_DIPLO_WAR_INDIAN_HIT);
      /* Extra hit once when already hostile after the −5 (thin matrix deepen). */
      if (ai_diplo_indian_read(col1, n, idx) < AI_DIPLO_INDIAN_VERY_LOW_REL) {
        ai_diplo_indian_relation_delta(col1, 4 + idx, n, -AI_DIPLO_INDIAN_HOSTILE_EXTRA);
      }
    }
    /* Keep sticky consistent with matrix after Euro×Euro war Indian hit. */
    ai_diplo_indian_hostility_sync(col1, n);
  }
}

uint8_t ai_diplo_indian_read(const ColonizeCol1Save* col1, int euro_nation, int indian_idx) {
  if (!col1 || euro_nation < 0 || euro_nation >= 4 || indian_idx < 0 || indian_idx >= 8) {
    return 0;
  }
  return col1->nation[euro_nation].relation_by_indian[indian_idx];
}

int ai_diplo_indian_at_war(const ColonizeCol1Save* col1, int euro_nation, int indian_idx) {
  const uint8_t r = ai_diplo_indian_read(col1, euro_nation, indian_idx);
  /* Unmet (0) is not at war — only contacted low relations. */
  return r > 0 && r < AI_DIPLO_INDIAN_AT_WAR_REL;
}

int ai_diplo_indian_any_at_war(const ColonizeCol1Save* col1, int euro_nation) {
  if (!col1 || euro_nation < 0 || euro_nation >= 4) {
    return 0;
  }
  for (int idx = 0; idx < 8; ++idx) {
    if (ai_diplo_indian_at_war(col1, euro_nation, idx)) {
      return 1;
    }
  }
  return 0;
}

uint8_t ai_diplo_indian_hostility_sticky(const ColonizeCol1Save* col1, int euro_nation) {
  if (!col1 || euro_nation < 0 || euro_nation >= 4) {
    return AI_DIPLO_STICKY_CLEAR;
  }
  return col1->nation[euro_nation].indian_hostility_sticky;
}

/*
 * Sync unknown26[8] from relation_by_indian matrix (unpark #5 sticky deepen).
 *  0 — no Indian at-war slots (all unmet r==0 or relation ≥ 50)
 *  1 — any contacted indian_at_war (0 < relation < 50)
 *  2 — deepen when already hostile and any slot very low (0 < relation < 40)
 * Unmet r==0 is not war (seed-100 early TURN goldens). Source: 15b3 Indian
 * hostility stand-in; contact alarm/friction <40 / ≥50 gates.
 */
void ai_diplo_indian_hostility_sync(ColonizeCol1Save* col1, int euro_nation) {
  if (!col1 || euro_nation < 0 || euro_nation >= 4) {
    return;
  }
  int any_war = 0;
  int any_very_low = 0;
  for (int idx = 0; idx < 8; ++idx) {
    const uint8_t r = ai_diplo_indian_read(col1, euro_nation, idx);
    if (r == 0) {
      continue; /* unmet */
    }
    if (r < AI_DIPLO_INDIAN_AT_WAR_REL) {
      any_war = 1;
    }
    if (r < AI_DIPLO_INDIAN_VERY_LOW_REL) {
      any_very_low = 1;
    }
  }
  uint8_t next = AI_DIPLO_STICKY_CLEAR;
  if (any_war) {
    next = any_very_low ? AI_DIPLO_STICKY_DEEP : AI_DIPLO_STICKY_AT_WAR;
  }
  col1->nation[euro_nation].indian_hostility_sticky = next;
}

void ai_diplo_indian_capital_surrender(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation
) {
  if (!col1 || euro_nation < 0 || euro_nation > 3) {
    return;
  }
  const int idx = indian_nation - 4;
  if (idx < 0 || idx >= 8) {
    return;
  }
  ColonizeCol1Indian* ind = &col1->indian[idx];
  ind->alarm_by_player[euro_nation] = 0;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &col1->tribe[ti];
      if ((int)t->nation_id != indian_nation) {
        continue;
      }
      t->alarm[euro_nation].friction = 0;
      t->alarm[euro_nation].attacks = 0;
      /* Fandom: no new capital after capital falls. */
      t->state.capital = 0;
    }
  }
  ind->euro_diplo[euro_nation] =
    (uint8_t)(ind->euro_diplo[euro_nation] | COL1_INDIAN_PEACE_BIT);
  {
    const uint8_t cur = ai_diplo_indian_relation(col1, indian_nation, euro_nation);
    if (cur < AI_DIPLO_INDIAN_PEACE_MEET) {
      ai_diplo_indian_relation_delta(
        col1, indian_nation, euro_nation, (int)(AI_DIPLO_INDIAN_PEACE_MEET - cur)
      );
    }
  }
  ai_diplo_indian_hostility_sync(col1, euro_nation);
}

/*
 * euro_balance Indian matrix arm: peace feeler → sticky sync → harassment.
 * Sticky→pressure: sticky==2 skips feeler + human "Natives remain hostile."
 * Human status chrome on rise/clear/deep (102a/1092 widgets PARKED);
 * feeler heal while sticky stays clear → "Native relations improve."
 */
static void ai_diplo_indian_matrix_tick(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  const uint8_t prev_sticky = ai_diplo_indian_hostility_sticky(col1, nation_id);
  int feeler_healed = 0;

  /*
   * Sticky→pressure (unpark #5): when sticky==2 (very-low deepen), block the
   * peace feeler this tick — deep hostility refuses the improve-relations path.
   * Source: fandom Indians — alarmed/hostile may refuse trade/gifts; contact
   * friction <40 band inverted. No invented gold drain (harassment owns −2g).
   */
  if (prev_sticky != AI_DIPLO_STICKY_DEEP) {
    /* Peace feeler before sync so content-floor heals can clear sticky. */
    feeler_healed = ai_diplo_indian_peace_feeler(col1, nation_id);
  }
  ai_diplo_indian_hostility_sync(col1, nation_id);

  const uint8_t sticky = ai_diplo_indian_hostility_sticky(col1, nation_id);
  int native_chrome = 0;
  if (ctx->human_nation == nation_id && ctx->status && ctx->status_size > 0) {
    if (prev_sticky == AI_DIPLO_STICKY_CLEAR && sticky != AI_DIPLO_STICKY_CLEAR) {
      /* Thin Contact/King status stand-in; full native-hostility dialog PARKED. */
      snprintf(ctx->status, ctx->status_size, "Natives grow hostile.");
      native_chrome = 1;
    } else if (prev_sticky != AI_DIPLO_STICKY_CLEAR && sticky == AI_DIPLO_STICKY_CLEAR) {
      /* Source: fandom / manual improve-relations feel after sticky clears
       * (peace feeler / drift). Thin 102a/1092; FA UI PARKED. */
      snprintf(ctx->status, ctx->status_size, "Native tensions ease.");
      native_chrome = 1;
    } else if (sticky == AI_DIPLO_STICKY_DEEP) {
      /* Structural pressure chrome while deep sticky persists. */
      snprintf(ctx->status, ctx->status_size, "Natives remain hostile.");
      native_chrome = 1;
    } else if (feeler_healed) {
      /* Mid-band feeler nudge while sticky stays clear (no rise/clear/deep).
       * Source: fandom Indians — peace → gifts / improve relations; FA UI PARKED. */
      snprintf(ctx->status, ctx->status_size, "Native relations improve.");
      native_chrome = 1;
    }
  }
  /* FUN_15b3 Indian hostility chrome → OK popup (INFO); status kept. */
  if (native_chrome) {
    ai_diplo_popup_ok(
      ctx, AI_POPUP_TAG_INFO, nation_id, -1, "Natives", ctx->status
    );
  }

  if (ai_diplo_indian_any_at_war(col1, nation_id)) {
    ColonizeCol1Nation* nat = &col1->nation[nation_id];
    /*
     * Harassment −2g once per balance tick; floor at 0. Skip when already 0
     * so a second invent-below-zero does not fire in the same path.
     * Source: thin Indian hostility drain; no multi-slot gold fiction.
     */
    if (nat->gold == 0) {
      /* already floored this tick path */
    } else if (nat->gold > AI_DIPLO_INDIAN_HARASS_GOLD) {
      nat->gold -= AI_DIPLO_INDIAN_HARASS_GOLD;
    } else {
      nat->gold = 0;
    }
  }
}

static uint8_t* ai_diplo_timer_byte(ColonizeCol1Save* col1, int nation, int peer) {
  if (!col1 || nation < 0 || nation >= 4 || peer < 0 || peer >= 4 || nation == peer) {
    return NULL;
  }
  return &col1->nation[nation].treaty_timer[peer];
}

static uint8_t* ai_diplo_flag_byte(ColonizeCol1Save* col1, int nation, int peer) {
  if (!col1 || nation < 0 || nation >= 4 || peer < 0 || peer >= 4 || nation == peer) {
    return NULL;
  }
  /* FUN_15b3 / DS −0x77c4 — mapped Col1 euro_relation[peer]. */
  return &col1->nation[nation].euro_relation[peer];
}

static const uint8_t* ai_diplo_flag_byte_const(const ColonizeCol1Save* col1, int nation, int peer) {
  if (!col1 || nation < 0 || nation >= 4 || peer < 0 || peer >= 4 || nation == peer) {
    return NULL;
  }
  return &col1->nation[nation].euro_relation[peer];
}

/* Mirror WAR/ALLY into nation_relation for legacy readers (derived only). */
static void ai_diplo_mirror_relation_summary(ColonizeCol1Save* col1, int nation) {
  if (!col1 || nation < 0 || nation >= 4) {
    return;
  }
  int war = 0;
  int ally = 0;
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation) {
      continue;
    }
    const uint8_t* f = ai_diplo_flag_byte_const(col1, nation, peer);
    if (!f) {
      continue;
    }
    if (*f & AI_DIPLO_WAR) {
      war = 1;
    }
    if (*f & AI_DIPLO_ALLY) {
      ally = 1;
    }
  }
  if (war) {
    col1->head.nation_relation[nation] = -50;
  } else if (ally) {
    col1->head.nation_relation[nation] = 40;
  } else {
    col1->head.nation_relation[nation] = 0;
  }
  /* Keep player.diplomacy as a coarse OR of peer flags (UI crumb). */
  uint8_t agg = 0;
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation) {
      continue;
    }
    const uint8_t* f = ai_diplo_flag_byte_const(col1, nation, peer);
    if (f) {
      agg = (uint8_t)(agg | *f);
    }
  }
  col1->player[nation].diplomacy = agg;
}

uint8_t ai_diplo_read(const ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1 || nation_a < 0 || nation_a >= 4 || nation_b < 0 || nation_b >= 4) {
    return 0;
  }
  if (nation_a == nation_b) {
    return AI_DIPLO_PEACE | AI_DIPLO_ALLY;
  }
  const uint8_t* f = ai_diplo_flag_byte_const(col1, nation_a, nation_b);
  if (!f) {
    return 0;
  }
  /* Unmet / never written: treat as peaceful known (PEACE|MET). */
  if (*f == 0) {
    return (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_MET);
  }
  return *f;
}

void ai_diplo_write(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t value) {
  if (!col1 || nation_a < 0 || nation_a >= 4 || nation_b < 0 || nation_b >= 4) {
    return;
  }
  if (nation_a == nation_b) {
    return;
  }
  uint8_t* f = ai_diplo_flag_byte(col1, nation_a, nation_b);
  if (!f) {
    return;
  }
  *f = value;
  ai_diplo_mirror_relation_summary(col1, nation_a);
}

void ai_diplo_or_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits) {
  if (!col1 || nation_a == nation_b) {
    return;
  }
  uint8_t a = (uint8_t)(ai_diplo_read(col1, nation_a, nation_b) | bits);
  uint8_t b = (uint8_t)(ai_diplo_read(col1, nation_b, nation_a) | bits);
  ai_diplo_write(col1, nation_a, nation_b, a);
  ai_diplo_write(col1, nation_b, nation_a, b);
}

void ai_diplo_clear_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits) {
  if (!col1 || nation_a == nation_b) {
    return;
  }
  uint8_t a = (uint8_t)(ai_diplo_read(col1, nation_a, nation_b) & (uint8_t)~bits);
  uint8_t b = (uint8_t)(ai_diplo_read(col1, nation_b, nation_a) & (uint8_t)~bits);
  /* Store raw result (0 = unread default on next read). */
  uint8_t* fa = ai_diplo_flag_byte(col1, nation_a, nation_b);
  uint8_t* fb = ai_diplo_flag_byte(col1, nation_b, nation_a);
  if (fa) {
    *fa = a;
  }
  if (fb) {
    *fb = b;
  }
  ai_diplo_mirror_relation_summary(col1, nation_a);
  ai_diplo_mirror_relation_summary(col1, nation_b);
}

int ai_diplo_at_war(const ColonizeCol1Save* col1, int nation_a, int nation_b) {
  return (ai_diplo_read(col1, nation_a, nation_b) & AI_DIPLO_WAR) != 0;
}

int ai_diplo_at_war_with(const ColonizeCol1Save* col1, int nation_a, int nation_b) {
  return ai_diplo_at_war(col1, nation_a, nation_b);
}

int ai_diplo_at_war_with_any(const ColonizeCol1Save* col1, int nation) {
  if (!col1 || nation < 0 || nation >= 4) {
    return 0;
  }
  for (int other = 0; other < 4; ++other) {
    if (other == nation) {
      continue;
    }
    if (ai_diplo_at_war(col1, nation, other)) {
      return 1;
    }
  }
  return 0;
}

void ai_diplo_declare_war(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  /*
   * Franklin: refuse NW Euro×Euro declare so king/Euro war spillover and
   * opportunistic pressure cannot poison peer relations (war-hit / embargo /
   * sting stay gated with this no-op). Source: docs/fandom_col1994.md
   * Benjamin Franklin. Combat/player callers share this gate for thin port.
   */
  if (ai_diplo_franklin_pair(col1, nation_a, nation_b)) {
    return;
  }
  const int already = ai_diplo_at_war(col1, nation_a, nation_b);
  ai_diplo_clear_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_ALLY));
  ai_diplo_or_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_MET));
  /* Thin 153e-shaped sting: gold drain + tax bump both sides (relation via mirror). */
  if (!already) {
    ai_diplo_war_treasury_sting(col1, nation_a, nation_b);
    ai_diplo_war_tax_bump(col1, nation_a, nation_b);
    /* Indians dislike Euro×Euro war (scalar stand-in; full 15b3 PARKED). */
    ai_diplo_war_indian_relation_hit(col1, nation_a, nation_b);
    /* Wartime trade embargo: all 16 @CARGO bits (incl. Cotton R11 leftover). */
    ai_diplo_war_embargo_set(col1, nation_a, nation_b);
    /* Unpark #5: colony-gap rich-side trade sting (Tools already in embargo set). */
    ai_diplo_war_trade_score_sting(col1, nation_a, nation_b);
    /* War fatigue: seed treaty timer if 0 so near-parity peace waits for age. */
    ai_diplo_war_fatigue_timer_seed(col1, nation_a, nation_b);
  }
}

/* Rival label for thin status: player.country_name or "rival". */
const char* ai_diplo_rival_name(const ColonizeCol1Save* col1, int nation) {
  if (!col1 || nation < 0 || nation >= 4) {
    return "rival";
  }
  if (col1->player[nation].country_name[0] != '\0') {
    return col1->player[nation].country_name;
  }
  return "rival";
}

/* True when human_nation is a or b (human-facing chrome / popup gate). */
static int ai_diplo_involves_human(const ColonizeTurnContext* ctx, int nation_a, int nation_b) {
  if (!ctx) {
    return 0;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return 0;
  }
  return nation_a == human || nation_b == human;
}

/* True if queue already has OK/CHOICE with same tag + pair (either order). */
static int ai_diplo_popup_pair_queued(
  const AiPopupState* st,
  AiPopupTag tag,
  int nation_a,
  int nation_b
) {
  if (!st) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    const AiPopupRequest* r = &st->queue[i];
    if (r->tag != tag) {
      continue;
    }
    if ((r->nation_a == nation_a && r->nation_b == nation_b) ||
        (r->nation_a == nation_b && r->nation_b == nation_a)) {
      return 1;
    }
  }
  return 0;
}

/*
 * Also enqueue map AI OK popup (FUN_15b3 / 5bfb 102a/1092 stand-in) when
 * ctx->ai_popups is set. Status line stays; full VGA dialog PARKED.
 * War re-declare uses !already (no spam); do not gate OK on tag+pair —
 * war boycott OK and peace Tools-lift OK share DIPLO_BOYCOTT.
 */
static void ai_diplo_popup_ok(
  ColonizeTurnContext* ctx,
  AiPopupTag tag,
  int nation_a,
  int nation_b,
  const char* title,
  const char* body
) {
  if (!ctx || !ctx->ai_popups || !body || body[0] == '\0') {
    return;
  }
  if (!ai_diplo_involves_human(ctx, nation_a, nation_b)) {
    return;
  }
  (void)ai_popup_enqueue_ok_ctx(
    ctx->ai_popups, tag, nation_a, nation_b, 0, title, body
  );
}

/* Tag from final human status after war/peace preference chain. */
static AiPopupTag ai_diplo_tag_from_status(const char* status, AiPopupTag fallback) {
  if (!status || status[0] == '\0') {
    return fallback;
  }
  if (strstr(status, "boycott") != NULL || strstr(status, "embargo") != NULL) {
    return AI_POPUP_TAG_DIPLO_BOYCOTT;
  }
  if (strstr(status, "Natives") != NULL || strstr(status, "Native ") != NULL) {
    return AI_POPUP_TAG_INFO;
  }
  return fallback;
}

/*
 * Thin 102a/1092 status when human is a party (Contact/King ctx->status pattern).
 * Full multi-line dialog widgets PARKED.
 */
static void ai_diplo_status_human_pair(
  ColonizeTurnContext* ctx,
  int nation_a,
  int nation_b,
  const char* fmt
) {
  if (!ctx || !ctx->col1 || !ctx->status || ctx->status_size == 0 || !fmt) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  int rival = -1;
  if (nation_a == human) {
    rival = nation_b;
  } else if (nation_b == human) {
    rival = nation_a;
  } else {
    return;
  }
  snprintf(ctx->status, ctx->status_size, fmt, ai_diplo_rival_name(ctx->col1, rival));
}

/*
 * @DECLAREWAR base line ("The {%STRING0} and {%STRING1} are now at war.") —
 * authentic GAME.TXT text for the war-declared OK popup body. Boycott/
 * hostility chrome in ai_diplo_declare_war_ctx may override this default
 * with a more specific status line when applicable (thin 102a/1092 stand-in;
 * full FA 3f41 dialog remains PARKED).
 */
static void ai_diplo_status_declare_war(
  ColonizeTurnContext* ctx,
  int nation_a,
  int nation_b
) {
  if (!ctx || !ctx->col1 || !ctx->status || ctx->status_size == 0) {
    return;
  }
  if (!ai_diplo_involves_human(ctx, nation_a, nation_b)) {
    return;
  }
  PopupMsgTokens tok = {0};
  tok.string0 = ai_diplo_rival_name(ctx->col1, nation_a);
  tok.string1 = ai_diplo_rival_name(ctx->col1, nation_b);
  popup_msg_fill(
    ctx->messages, "DECLAREWAR", &tok,
    "The %STRING0 and %STRING1 are now at war.",
    ctx->status, ctx->status_size
  );
}

/*
 * @SIGNTREATY base line ("The {%STRING0} and {%STRING1} have signed a peace
 * treaty.") — authentic GAME.TXT text for the peace-concluded OK popup body.
 * Tools-embargo-lift chrome in ai_diplo_make_peace_ctx may override this
 * default with the more specific line when applicable (thin 102a/1092
 * stand-in; full FA 3f41 dialog remains PARKED).
 */
static void ai_diplo_status_sign_treaty(
  ColonizeTurnContext* ctx,
  int nation_a,
  int nation_b
) {
  if (!ctx || !ctx->col1 || !ctx->status || ctx->status_size == 0) {
    return;
  }
  if (!ai_diplo_involves_human(ctx, nation_a, nation_b)) {
    return;
  }
  PopupMsgTokens tok = {0};
  tok.string0 = ai_diplo_rival_name(ctx->col1, nation_a);
  tok.string1 = ai_diplo_rival_name(ctx->col1, nation_b);
  popup_msg_fill(
    ctx->messages, "SIGNTREATY", &tok,
    "The %STRING0 and %STRING1 have signed a peace treaty.",
    ctx->status, ctx->status_size
  );
}

/* @CARGO display names (colony.h / NAMES.TXT / reports.c) for boycott chrome. */
static const char* ai_diplo_cargo_name(int cargo_idx) {
  static const char* const names[COLONIZE_CARGO_COUNT] = {
    "Food",        "Sugar",  "Tobacco", "Cotton", "Furs",  "Lumber",
    "Ore",         "Silver", "Horses",  "Rum",    "Cigars", "Cloth",
    "Coats",       "Trade Goods", "Tools", "Muskets"
  };
  if (cargo_idx < 0 || cargo_idx >= COLONIZE_CARGO_COUNT) {
    return "cargo";
  }
  return names[cargo_idx];
}

/* Full wartime 16-bit embargo mask (same set as war_embargo_set/lift). */
static uint16_t ai_diplo_wartime_boycott_mask(void) {
  return (uint16_t)(AI_DIPLO_WAR_FOOD_EMBARGO_BIT | AI_DIPLO_WAR_EMBARGO_CARGO_BIT |
                    AI_DIPLO_WAR_TOBACCO_EMBARGO_BIT | AI_DIPLO_WAR_SUGAR_EMBARGO_BIT |
                    AI_DIPLO_WAR_COTTON_EMBARGO_BIT | AI_DIPLO_WAR_LUMBER_EMBARGO_BIT |
                    AI_DIPLO_WAR_HORSES_EMBARGO_BIT | AI_DIPLO_WAR_RUM_EMBARGO_BIT |
                    AI_DIPLO_WAR_CIGARS_EMBARGO_BIT | AI_DIPLO_WAR_CLOTH_EMBARGO_BIT |
                    AI_DIPLO_WAR_COATS_EMBARGO_BIT | AI_DIPLO_WAR_ORE_EMBARGO_BIT |
                    AI_DIPLO_WAR_SILVER_EMBARGO_BIT | AI_DIPLO_WAR_TRADE_GOODS_EMBARGO_BIT |
                    AI_DIPLO_WAR_TOOLS_EMBARGO_BIT | AI_DIPLO_WAR_MUSKETS_EMBARGO_BIT);
}

void ai_diplo_declare_war_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b) {
  if (!ctx || !ctx->col1) {
    return;
  }
  const int already = ai_diplo_at_war(ctx->col1, nation_a, nation_b);
  const int human = ctx->human_nation;
  /* Boycott chrome tracks full wartime @CARGO mask (all 16). */
  const uint16_t boycott_mask = ai_diplo_wartime_boycott_mask();
  uint16_t boycott_before = 0;
  uint8_t sticky_before = AI_DIPLO_STICKY_CLEAR;
  if (human >= 0 && human < 4) {
    boycott_before =
      (uint16_t)(ctx->col1->nation[human].boycott_bitmap & boycott_mask);
    sticky_before = ai_diplo_indian_hostility_sticky(ctx->col1, human);
  }
  ai_diplo_declare_war(ctx->col1, nation_a, nation_b);
  /* Franklin may no-op declare — only chrome when WAR actually stuck. */
  const int now_war = ai_diplo_at_war(ctx->col1, nation_a, nation_b);
  if (!already && now_war) {
    ai_diplo_status_declare_war(ctx, nation_a, nation_b);
    /*
     * Wartime boycott human chrome (102a/1092 stand-in): prefer Sugar/Tobacco/
     * Tools combined lines when those bits are newly OR'd; else name the first
     * newly boycotted @CARGO (colony.h / NAMES.TXT) over the war line. Else if
     * Indian sticky newly rose from the −5 war-hit, prefer "Natives grow
     * hostile." Widgets PARKED. Source: thin 153e trade deepen + Contact/King
     * status; Indians dislike Euro×Euro war (fandom / euro_diplo.md).
     * Also enqueue AI OK popup (FUN_15b3 / 5bfb); FA 3f41 full UI PARKED.
     */
    if (human >= 0 && human < 4 && (nation_a == human || nation_b == human) &&
        ctx->status && ctx->status_size > 0) {
      const uint16_t boycott_after =
        (uint16_t)(ctx->col1->nation[human].boycott_bitmap & boycott_mask);
      const uint16_t newly = (uint16_t)(boycott_after & (uint16_t)~boycott_before);
      if (newly & AI_DIPLO_WAR_TOOLS_EMBARGO_BIT) {
        snprintf(ctx->status, ctx->status_size, "Sugar/Tobacco/Tools boycott imposed.");
      } else if (newly &
                 (uint16_t)(AI_DIPLO_WAR_SUGAR_EMBARGO_BIT | AI_DIPLO_WAR_TOBACCO_EMBARGO_BIT)) {
        snprintf(ctx->status, ctx->status_size, "Sugar/Tobacco boycott imposed.");
      } else if (newly != 0) {
        /* First newly OR'd wartime cargo by @CARGO index (Food..Muskets). */
        for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
          if (newly & (uint16_t)(1u << c)) {
            snprintf(ctx->status, ctx->status_size, "%s boycott imposed.",
                     ai_diplo_cargo_name(c));
            break;
          }
        }
      } else {
        const uint8_t sticky_after = ai_diplo_indian_hostility_sticky(ctx->col1, human);
        if (sticky_before == AI_DIPLO_STICKY_CLEAR &&
            sticky_after != AI_DIPLO_STICKY_CLEAR) {
          snprintf(ctx->status, ctx->status_size, "Natives grow hostile.");
        }
      }
      ai_diplo_popup_ok(
        ctx,
        ai_diplo_tag_from_status(ctx->status, AI_POPUP_TAG_DIPLO_WAR),
        nation_a,
        nation_b,
        "Diplomacy",
        ctx->status
      );
    }
  }
}

/*
 * Thin make-peace (not full 153e peace dialog / 102a/1092):
 * clear WAR both ways, set PEACE|MET, lift all 16 wartime @CARGO boycotts
 * (Food+…+Cotton+…+Tools+Muskets) if no Euro wars remain (shared lift helper).
 * No gold cost (war sting/upkeep already drained treasury). When sticky was
 * at-war (==1) on either side, nudge Indian peace feeler once after WAR clear
 * (existing feeler path; restores improve-relations that Euro war blocked).
 * sticky==2 still refuses feeler (self-gated). Full 153e PARKED. Privateer
 * prize + once-per-war spawn bit are WAR-gated — peace stops prize and clears
 * unknown26[9] peer bits so the next war may commission again.
 */
void ai_diplo_make_peace(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1 || nation_a < 0 || nation_a >= 4 || nation_b < 0 || nation_b >= 4 ||
      nation_a == nation_b) {
    return;
  }
  const int was_war = ai_diplo_at_war(col1, nation_a, nation_b);
  const uint8_t sticky_a = ai_diplo_indian_hostility_sticky(col1, nation_a);
  const uint8_t sticky_b = ai_diplo_indian_hostility_sticky(col1, nation_b);
  ai_diplo_clear_both(col1, nation_a, nation_b, AI_DIPLO_WAR);
  ai_diplo_or_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_MET));
  ai_diplo_war_embargo_lift_if_peace(col1, nation_a, nation_b);
  /* Clear once-per-war Privateer commission bits both dirs (next war may respawn). */
  if (was_war) {
    ai_diplo_privateer_spawn_clear(col1, nation_a, nation_b);
    ai_diplo_privateer_spawn_clear(col1, nation_b, nation_a);
  }
  /*
   * Peace restores Indian feeler (unpark #5): Euro×Euro war gates feeler off;
   * when sticky was at-war (==1), nudge once via existing peace-feeler path
   * after WAR clear so !at_war_with_any can pass. sticky==2 self-gates inside
   * feeler (deep hostility refuses). Sync sticky after heal.
   * Source: fandom Indians — peace → gifts / improve relations; FA UI PARKED.
   */
  if (was_war) {
    if (sticky_a == AI_DIPLO_STICKY_AT_WAR) {
      ai_diplo_indian_peace_feeler(col1, nation_a);
      ai_diplo_indian_hostility_sync(col1, nation_a);
    }
    if (sticky_b == AI_DIPLO_STICKY_AT_WAR) {
      ai_diplo_indian_peace_feeler(col1, nation_b);
      ai_diplo_indian_hostility_sync(col1, nation_b);
    }
  }
}

void ai_diplo_make_peace_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b) {
  if (!ctx || !ctx->col1) {
    return;
  }
  const int was_war = ai_diplo_at_war(ctx->col1, nation_a, nation_b);
  const int human = ctx->human_nation;
  uint16_t tools_before = 0;
  if (human >= 0 && human < 4) {
    tools_before =
      (uint16_t)(ctx->col1->nation[human].boycott_bitmap & AI_DIPLO_WAR_TOOLS_EMBARGO_BIT);
  }
  ai_diplo_make_peace(ctx->col1, nation_a, nation_b);
  if (was_war) {
    ai_diplo_status_sign_treaty(ctx, nation_a, nation_b);
    /*
     * Tools embargo lift chrome when human had Tools bit and peace cleared it
     * (no remaining Euro wars). Prefer Tools line over peace when applicable.
     * Also enqueue AI OK popup (FUN_15b3 / 5bfb 102a/1092); status kept.
     */
    if (human >= 0 && human < 4 && (nation_a == human || nation_b == human)) {
      const uint16_t tools_after =
        (uint16_t)(ctx->col1->nation[human].boycott_bitmap & AI_DIPLO_WAR_TOOLS_EMBARGO_BIT);
      if (tools_before != 0 && tools_after == 0 && ctx->status && ctx->status_size > 0) {
        snprintf(ctx->status, ctx->status_size, "Tools embargo lifted.");
      }
      if (ctx->status && ctx->status[0] != '\0') {
        ai_diplo_popup_ok(
          ctx,
          ai_diplo_tag_from_status(ctx->status, AI_POPUP_TAG_DIPLO_PEACE),
          nation_a,
          nation_b,
          "Diplomacy",
          ctx->status
        );
      }
    }
  }
}

/*
 * FUN_5bfb_12d0, full port (euro_diplo_153e_full.md, resolved 2026-08-19):
 * for every combat-capable land unit belonging to `nation_b` sitting
 * adjacent to a settlement owned by `nation_a`, cancel a pending
 * Fortify/Fortified order (DOS order-state 5/6 -> 0) so the unit re-plans
 * next act instead of sitting garrisoned against a neighbor whose
 * diplomatic status just changed. Decomp calls this (A,B) then (B,A) from
 * the alliance-form path (both sides' border garrisons refreshed).
 */
static void ai_diplo_wake_border_garrisons(
  ColonizeTurnContext* ctx, int nation_a, int nation_b
) {
  if (!ctx || !ctx->units || !ctx->colonies) {
    return;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = units_get(ctx->units, i);
    if (!u || !u->active || u->nation_id != nation_b || units_is_sea(ctx->units, i)) {
      continue;
    }
    if (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED) {
      continue;
    }
    const ColonizeUnitType* ut = units_type(ctx->units, u->type_index);
    if (!ut || ut->attack <= 1) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int cid = colonies_id_at(ctx->colonies, u->x + dx[d], u->y + dy[d]);
      if (cid < 0) {
        continue;
      }
      const ColonizeColony* c = colonies_get(ctx->colonies, cid);
      if (c && c->active && c->nation_id == nation_a) {
        units_clear_orders(ctx->units, i);
        break;
      }
    }
  }
}

/*
 * FUN_5bfb_153e — outcome-table selector, resolved 2026-08-19 (was
 * "genuinely open," euro_diplo_153e_full.md). There is NO runtime
 * index-selector variable. The perceived "outcome jump table"
 * (OVL16_L0040:3bcb-3bf8, 10 entries, 5-byte JMPF stride each) is DOS's
 * own inter-overlay call linkage: every `FUN_OVL16_L0040__003bXX` symbol
 * Ghidra printed inside 153e's body is a COMPILE-TIME-FIXED call site
 * bound to exactly one table slot. Confirmed directly, not inferred —
 * cross-referenced `tools/address_mapping.csv`'s `OVL16_L0040` offset
 * column (rows 2360-2369) against the 10 trivial 2-line resident thunks
 * at `viceroy_unpacked.c:38150-38246`
 * (`FUN_210d_0dab(0x2a1f); FUN_5bfb_XXXX(); return;`). 153e's body only
 * ever reaches 5 of the 10 slots (never 312e, 0182, or 022e — those are
 * reached by other callers):
 */
typedef struct Ai153eSelectorSite {
  int table_index;          /* 0-9 slot in the OVL16_L0040:3bcb-3bf8 table */
  const char* ovl_offset;   /* 153e's own call-site symbol */
  const char* target;       /* canonical FUN_5bfb_XXXX bound to that slot */
  const char* linux_status; /* where the target already lives in Linux */
} Ai153eSelectorSite;

static const Ai153eSelectorSite ai_diplo_153e_selector_table[] = {
  /* idx7, offset 3bee — raw line 406, 153e's OWN entry gate: fires with
   * an unrecoverable zero-arg register call when `param_2` is invalid or
   * IS the human nation (DOS `param_2*0x34+0x543f != 0`, the same
   * control-status byte `ai_king.c`'s FUN_43f7_2244 header already cites
   * as identical to `turn_run_european_ai_stubs`'s human-skip gate). */
  {7, "3bee", "FUN_5bfb_13b0", "ai_diplo_form_alliance/break_alliance (Done)"},
  /* idx4, offset 3bdf — raw line 485, the ONLY selector call inside the
   * worthiness-score phase itself: a per-unit "score" callee inside the
   * unit-ownership loop. See ai_diplo_153e_unit_score_stub below. */
  {4, "3bdf", "FUN_5bfb_0000", "score stand-in, not independently ported (stub)"},
  /* idx2, offset 3bd5 — commit/flavor-text phase (raw ~704+, past the
   * worthiness-score phase), fired ~9x with different message-id
   * literals (0x18bb..0x197c). Thin ctx->status dialog already covers
   * the generic shape project-wide. */
  {2, "3bd5", "FUN_5bfb_102a", "thin ctx->status dialog (Done, generic)"},
  /* idx5, offset 3be4 — commit-phase sibling status/bool setter (not a
   * message id), fired 3x (raw 734/785/842). */
  {5, "3be4", "FUN_5bfb_1092", "thin ctx->status dialog (Done, generic)"},
  /* idx1, offset 3bd0 — commit phase (raw 1065-1067): border-garrison
   * wake, fired (A,B) then (B,A) once the "at war" bit reads set on the
   * just-updated relation. Already the full port target above. */
  {1, "3bd0", "FUN_5bfb_12d0", "ai_diplo_wake_border_garrisons (Done)"},
};
#define AI_DIPLO_153E_SELECTOR_COUNT \
  (int)(sizeof(ai_diplo_153e_selector_table) / sizeof(ai_diplo_153e_selector_table[0]))

/*
 * FUN_5bfb_0000 (selector idx4) — "census/rank/combat factor, score
 * stand-in" per euro_diplo.md; not independently resolved/ported as its
 * own DOS body anywhere in this project (the project-wide generic
 * stand-in for this role is ai_diplo_military_score, a different, already
 * -live approximation — not a byte-exact port of this specific callee).
 * Honest stub: contributes nothing and never claims a per-unit border
 * match, so the worthiness-score unit loop's own control flow (which
 * units it visits, which nation each belongs to) stays real while the one
 * genuinely unresolved leaf value stays inert instead of invented.
 */
typedef struct Ai153eUnitScoreStub {
  int value;          /* raw iStack_b8 */
  int matched_target;  /* raw iStack_92 — which nation this call judged */
  int flag;             /* raw iStack_6 */
  int count;            /* raw iStack_c4 */
} Ai153eUnitScoreStub;

static Ai153eUnitScoreStub ai_diplo_153e_unit_score_stub(int self, int target, int unit_owner) {
  (void)self;
  (void)target;
  (void)unit_owner;
  Ai153eUnitScoreStub r;
  r.value = 0;
  r.matched_target = -1;
  r.flag = 0;
  r.count = 0;
  return r;
}

/*
 * FUN_5bfb_153e — worthiness-score phase, structural reference port
 * (2026-08-19; euro_diplo_153e_full.md's "phase 1", raw lines ~405-594).
 * Mirrors the DOS control flow and arithmetic shape end to end. Every DS
 * global this project has NOT independently resolved elsewhere is an
 * honest stub (neutral/inert value, never an invented formula/constant)
 * — see each stub call's own comment for the raw offset it stands in
 * for.
 *
 * 2026-08-20 (T2.1 precedent, T2.2 delta catalog): **unlike the `5d04`
 * hire-ladder tail, this function is NOT inert-by-construction.** Traced
 * the full control flow with every current stub at its neutral value:
 * `dominance_bonus` and the continent/unit-ownership loops genuinely
 * cancel to zero end to end (confirmed, not assumed — `target_land_units
 * < self_exposed(stub=0)` can never be true since counts are ≥0, so
 * `dominance_bonus` never leaves 0; the unit-score stub never matches).
 * `combat_delta_sum` likewise nets to exactly 0 through every stubbed
 * term — **except** `peace_bit_0x10` (`ai_diplo_read(self,target)&0x10`,
 * raw `uStack_9e`), the one non-stub, live-data read in this whole
 * function. If that bit is ever set for a real nation pair, it directly
 * sets `worthy=1` (raw line ~527) *and* adds `(difficulty+1)*500` to
 * `combat_delta_sum` (raw ~566), which is otherwise the sole thing
 * standing between "always inert" (line ~589's `combat_delta_sum==0 →
 * worthy=0` catches every other path) and a real war-worthiness
 * assertion. So: today, wiring this live is safe only because *nothing
 * calls it* — the function itself would already react to live DOS
 * diplomatic state via this one bit, not just to stubs, unlike `5d04`'s
 * fully-gated tail. Bit `0x10` on this accessor (same family as the
 * already-resolved `AI_DIPLO_MET`==`0x40`) has **not** been independently
 * named anywhere in this project — genuinely open, not just unattempted;
 * resolving it (or explicitly stubbing `peace_bit_0x10` to 0 to make this
 * function fully inert like `5d04`, if wiring is wanted before that RE
 * lands) is the real next step before this is a Tier 3 candidate. NOT
 * wired into any live caller: matches the
 * ai_euro_5d04_nation_planning_structural precedent — finishing a
 * structural port doesn't by itself make it safe to flip live on a
 * function this stub-dense.
 *
 * Real inputs used: euro_relation[] peace bit (resolved, DS -0x77c4),
 * turn/difficulty (col1->head, resolved), the human-nation entry gate
 * (resolved, see the selector table above), and the G-table's already-
 * resolved per-continent colony/land-unit counts (-0x6b1a/-0x6b5a),
 * recomputed locally the same cheap way ai_euro_refresh_continent_stance
 * does rather than reaching into that file's static tables.
 *
 * Genuinely unresolved DS globals stubbed here (documented, not
 * guessed — see euro_diplo_153e_full.md for the full raw citations):
 *   -0x77f8  per-nation flags byte, bit2 (raw 432) - NOT -0x77c4/euro_
 *            relation, a distinct field this project hasn't named
 *   -0x6d68  per-nation "tension" byte (raw 441, 556, 590)
 *   -0x6a4e  per-continent EXPOSED combat value (raw 442+) - distinct
 *            from the already-resolved -0x6e74/-0x6a8e sums this file's
 *            G-table exposes; the "exposed" filter (not fortified,
 *            orders != A/G, plus the 0x543f class gate) isn't wired
 *   -0x6ada  per-continent skilled-unit count (raw 454)
 *   0x53c8[] per-nation declare-war cooldown timer (raw 421-424, 436-437)
 *   0xa153   single unresolved byte (raw 509) - no match anywhere else
 *            in this project's docs or tools/address_mapping.csv
 *   0x84fc   indirect royal/crown treasury record, +0x2a/+0x2c halves
 *            (raw 570-584) - the affordability clamp this gates never
 *            fires here (stub keeps the score unclamped by this gate)
 *   FUN_1000_89a4 (table -0x77f1, idx 0x13) - per-nation FF/feature-bit
 *            test (FUNCTION_CATALOG.md: "nation feature/FF bit test");
 *            which specific FF isn't identified, stub reads "absent"
 *   FUN_1000_8714/86b0/8772 - raw 419/430/435, dialog-name-prep side
 *            effects that only feed phase 4's flavor text (out of this
 *            phase's scope); no-ops here
 */
typedef struct Ai153eWorthinessScore {
  int handled;         /* raw uStack_8e - did the phase run to completion */
  int worthy;           /* raw bVar12/iStack_a8 at phase end */
  int dominance_bonus;  /* raw iStack_ce */
  int score;             /* raw uStack_68 - feeds the (unported) commit phase */
  int at_peace;          /* raw uStack_ae: euro_relation[target][self] bit2,
                             direct (non-accessor) read - doc: "consistent
                             with plain AI_DIPLO_PEACE" */
} Ai153eWorthinessScore;

static int ai_diplo_153e_colonies_at(
  const ColonizeTurnContext* ctx, int nation, int continent_id
) {
  int n = 0;
  if (!ctx->colonies) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (c->active && c->nation_id == nation &&
        map_continent_id_at(ctx->map, c->x, c->y) == continent_id) {
      ++n;
    }
  }
  return n;
}

static int ai_diplo_153e_land_units_at(
  const ColonizeTurnContext* ctx, int nation, int continent_id
) {
  int n = 0;
  if (!ctx->units) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (u && u->active && u->nation_id == nation && !units_is_sea(ctx->units, i) &&
        map_continent_id_at(ctx->map, u->x, u->y) == continent_id) {
      ++n;
    }
  }
  return n;
}

static Ai153eWorthinessScore ai_diplo_153e_worthiness_score_structural(
  ColonizeTurnContext* ctx, int self, int target, int forced_gate
) {
  Ai153eWorthinessScore out;
  out.handled = 0;
  out.worthy = 0;
  out.dominance_bonus = 0;
  out.score = 0;
  out.at_peace = 0;
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->map || !ctx->colonies || !ctx->units ||
      self < 0 || self >= 4 || target < 0 || target >= 4) {
    return out;
  }

  /* raw 405-410: entry gate — invalid self, or self IS the human nation.
   * DOS fires the idx7 (FUN_5bfb_13b0) call here with unrecoverable args
   * (register-passed thunk call, Ghidra shows empty parens) — not
   * invented; we just mark the phase "handled" and bail, matching the
   * DOS `goto LAB_OVL16_L0040__0034de` early exit. */
  if (self == ctx->human_nation) {
    out.handled = 1;
    return out;
  }

  const ColonizeCol1Nation* nat_target = &ctx->col1->nation[target];
  const int difficulty = ctx->col1->head.difficulty;
  const int turn = ctx->col1->head.turn;

  /* raw 412-428: gate cascade. `forced_gate` is DOS param_6 (caller-
   * supplied). The two internal forcers (relation bit 0x20, and the
   * 0x53c8[] cooldown timer) both stub to "doesn't force" — the timer
   * table isn't tracked in Linux (see header), and the raw bit-0x20
   * semantics on this specific accessor call (itself called with
   * unrecoverable args, raw 415) aren't confirmed against Linux's own
   * AI_DIPLO_* numbering (see euro_diplo.md's "do not conflate" note on
   * DOS bit 0x40 vs Linux MET — the same caution applies to other raw
   * bit literals in this function). */
  int gate = forced_gate;
  if (gate == 0) {
    out.handled = 0;
    return out; /* raw: goto LAB_OVL16_L0040__0034de, nothing computed */
  }
  out.handled = 1;

  /* raw 432-434: -0x77f8 per-nation flags bit2 — stub, unresolved field,
   * flavor-text-only downstream (phase 4, out of this phase's scope). */

  /* raw 436-438: 0x53c8[target] cooldown reset to current turn — Linux
   * has no persistent equivalent wired here (reference port, not live). */

  /* raw 439-475: continent loop — the real G-table dominance/delta
   * accumulators. Colonies (-0x6b1a) and land units (-0x6b5a) are real,
   * recomputed locally; exposed combat value (-0x6a4e) and skilled-unit
   * count (-0x6ada) are honest stubs (0) per the header — this makes the
   * "self is dominant" branch never trigger and the delta-sum branch's
   * exposed-value terms always cancel to a land-unit-count-only shape,
   * which is real but incomplete without those two tables. */
  int dominance_bonus = 0;   /* raw iStack_ce */
  int combat_delta_sum = 0;  /* raw uStack_68 */
  int worthy = 0;             /* raw iStack_a8 */
  for (int cid = 1; cid <= 14; ++cid) {
    const int target_colonies = ai_diplo_153e_colonies_at(ctx, target, cid);
    const int target_land_units = ai_diplo_153e_land_units_at(ctx, target, cid);
    const int self_colonies = ai_diplo_153e_colonies_at(ctx, self, cid);
    const int self_exposed = 0;   /* -0x6a4e stub */
    const int target_exposed = 0; /* -0x6a4e stub */
    const int self_skilled = 0;   /* -0x6ada stub */
    const int target_tension = 0; /* -0x6d68 stub */
    const int dominance_threshold = (target_tension > 1) ? 1 : 0;
    if (dominance_threshold < target_colonies && target_land_units < self_exposed) {
      dominance_bonus += (self_exposed / (target_land_units + 1)) << (difficulty == 0 ? 1 : 2);
    } else {
      if (self_exposed != 0 && target_exposed != 0) {
        worthy = 1;
      }
      if (target_exposed != 0 && self_skilled > 4) {
        worthy = 1;
      }
      int delta;
      if (self_colonies == 0) {
        delta = (target_exposed - self_exposed) >> (target_colonies == 0 ? 1 : 2);
      } else if (target_land_units < 2) {
        delta = target_exposed;
      } else {
        delta = target_exposed - self_exposed;
      }
      combat_delta_sum += delta;
    }
  }

  /* raw 476-508: unit-ownership loop, selector idx4 (FUN_5bfb_0000). Real
   * control flow (visits every active unit, checks owner against
   * self/target); the per-unit score contribution is the honest stub
   * above (always 0 / never matches), so this loop is currently inert on
   * the returned score but structurally faithful. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active) {
      continue;
    }
    if (u->nation_id != self && u->nation_id != target) {
      continue;
    }
    const Ai153eUnitScoreStub s = ai_diplo_153e_unit_score_stub(self, target, u->nation_id);
    if (s.matched_target == target) {
      combat_delta_sum += s.value * 2;
      if (s.flag == 0 || s.count > 1) {
        if (dominance_bonus != 0) {
          dominance_bonus -= 1;
        }
        worthy = 1;
      }
    }
    if (s.matched_target == self) {
      combat_delta_sum += -(s.count * 2);
    }
  }

  /* raw 509-521: forced-conflict override. 0xa153 (unresolved single
   * byte) stubs to "never matches self" — the tension half of the OR
   * still evaluates, but both tension bytes are themselves -0x6d68
   * stubs (0), so this branch is currently always false in this port. */
  const int forced_conflict_byte_match = 0; /* 0xa153 stub */
  const int self_tension = 0;               /* -0x6d68 stub, reused */
  const int target_tension2 = 0;            /* -0x6d68 stub, reused */
  int forced_conflict =
    forced_conflict_byte_match && turn > 0x4f && self_tension > 3 && target_tension2 > 1;
  const int at_peace = (nat_target->euro_relation[self] & AI_DIPLO_PEACE) != 0; /* raw uStack_ae */
  if (forced_conflict || (at_peace && self_tension < target_tension2 * 3)) {
    worthy = 1;
    dominance_bonus = 0;
    /* raw 520: func_0x0001854c(seg, combat_delta_sum, difficulty*200+100,
     * 0x26ac) — argument shape (value, min, max) read as a clamp; DOS
     * name/body not independently confirmed, but no constant is invented
     * (min/max are the raw literals). */
    const int lo = difficulty * 200 + 100;
    const int hi = 0x26ac;
    if (combat_delta_sum < lo) combat_delta_sum = lo;
    if (combat_delta_sum > hi) combat_delta_sum = hi;
  }

  /*
   * raw 522-527. 2026-08-20 (T1.11): mechanical role now pinned — a third,
   * independent worthy=1 trigger (alongside crown-pressure and
   * peace+wealth-disparity below), and it also adds a flat
   * `(difficulty+1)*500` straight into combat_delta_sum unconditionally
   * (see the second `peace_bit_0x10` use below). What DOS condition ever
   * *sets* this bit is still genuinely unresolved — the writer
   * (`FUN_0000_5b62`, confirmed via address_mapping.csv) takes a raw byte,
   * not a mask, so its callers read-modify-write; no literal `|0x10` near
   * the euro_relation address pattern exists anywhere in the canonical
   * export (checked). Needs an XREF search on the writer or a live
   * write-breakpoint, not another grep. Full trace:
   * euro_diplo_153e_full.md's 2026-08-20 T1.11 update. Still not safe to
   * wire this function live without resolving that or explicitly zeroing
   * this bit first (see T2.2 note above).
   */
  const int peace_bit_0x10 = (ai_diplo_read(ctx->col1, self, target) & 0x10) != 0; /* raw uStack_9e */
  if (peace_bit_0x10) {
    worthy = 1;
  }
  if (dominance_bonus != 0) {
    worthy = 0;
  }

  /* raw 528-535: FUN_1000_89a4(seg, self, 0x13) — per-nation FF/feature
   * bit test (table -0x77f1), stub reads "absent" (0). */
  const int self_has_feature_0x13 = 0;
  if (self_has_feature_0x13) {
    forced_conflict = 0;
    worthy = 0;
  }

  /* raw 536-544: difficulty-threshold override, only when not at peace. */
  if (!at_peace) {
    const int thr = (difficulty - 10) * -10;
    if (thr != turn && turn <= thr) {
      forced_conflict = 0;
      worthy = 0;
    }
  }
  (void)forced_conflict; /* not read again within this phase's own scope */

  /* raw 545-563: final scaling. */
  int scaled = ((difficulty + 8) * combat_delta_sum * 10) / 100;
  if (!at_peace) {
    combat_delta_sum = scaled >> 2;
    /* raw 548 `-1 < iStack_8c` (old cooldown value) always true here —
     * the 0x53c8[] cooldown table is stubbed, see header. */
    if (turn < 0x32) {
      scaled >>= 1;
    } else if (turn < 100) {
      scaled -= combat_delta_sum;
    }
    combat_delta_sum = scaled;
    if (self_tension < 3 && 0 /* -0x6bf0 stub, raw 556 second operand */) {
      combat_delta_sum >>= 1;
    }
  } else {
    combat_delta_sum = scaled << 1;
  }

  /* raw 564-566 */
  {
    const int lo = 0;
    const int hi = 400;
    int v = (int)(((difficulty + 1) * combat_delta_sum) >> 3);
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    combat_delta_sum = v * 50;
  }
  if (peace_bit_0x10) {
    combat_delta_sum += (difficulty + 1) * 500;
  }

  /* raw 570-584: royal/crown treasury affordability gate (DS 0x84fc,
   * +0x2a/+0x2c halves) — stub, never triggers (treated as "always
   * affordable"), so combat_delta_sum is not clamped by this gate here. */

  /* raw 585-588 */
  if (self_has_feature_0x13) {
    combat_delta_sum >>= 1;
  }

  /* raw 589-593 */
  if (combat_delta_sum == 0 || self_tension > target_tension2 * 3) {
    worthy = 0;
  }

  out.worthy = worthy;
  out.dominance_bonus = dominance_bonus;
  out.score = combat_delta_sum;
  out.at_peace = at_peace;
  return out;
}

void ai_diplo_form_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  const int was_war = ai_diplo_at_war(col1, nation_a, nation_b);
  ai_diplo_clear_both(col1, nation_a, nation_b, AI_DIPLO_WAR);
  ai_diplo_or_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_ALLY | AI_DIPLO_PEACE | AI_DIPLO_MET));
  /* Lift all 16 wartime @CARGO bits if neither side remains at Euro war. */
  ai_diplo_war_embargo_lift_if_peace(col1, nation_a, nation_b);
  if (was_war) {
    ai_diplo_privateer_spawn_clear(col1, nation_a, nation_b);
    ai_diplo_privateer_spawn_clear(col1, nation_b, nation_a);
  }
  /* Thin alliance treasury cost: 25 gold each side (floor 0). */
  ai_diplo_ally_treasury_cost(col1, nation_a, nation_b);
  /* Treaty timer: if peer slot is 0, set to 8 so alliance persists a few ticks. */
  ai_diplo_ally_treaty_timer_bump(col1, nation_a, nation_b);
}

/*
 * Thin 102a/1092 status when human is a party (Contact/King pattern).
 * First form → "Alliance formed with %s"; prefer gold-drain chrome when 25g
 * cost fires. AI callers keep using form_alliance without status.
 * FA dialog UI PARKED. Source: 102a/1092 stand-in; euro_diplo.md.
 */
void ai_diplo_form_alliance_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b) {
  if (!ctx || !ctx->col1) {
    return;
  }
  const int was_ally =
    (ai_diplo_read(ctx->col1, nation_a, nation_b) & AI_DIPLO_ALLY) != 0;
  const int human = ctx->human_nation;
  uint16_t gold_before = 0;
  const int human_party =
    (human >= 0 && human < 4 && (nation_a == human || nation_b == human));
  if (human_party) {
    gold_before = ctx->col1->nation[human].gold;
  }
  ai_diplo_form_alliance(ctx->col1, nation_a, nation_b);
  ai_diplo_wake_border_garrisons(ctx, nation_a, nation_b);
  ai_diplo_wake_border_garrisons(ctx, nation_b, nation_a);
  int alliance_chrome = 0;
  if (!was_ally) {
    ai_diplo_status_human_pair(ctx, nation_a, nation_b, "Alliance formed with %s");
    alliance_chrome = human_party ? 1 : 0;
  }
  /* Prefer gold-drain chrome over formed when human treasury paid. */
  if (human_party && ctx->col1->nation[human].gold < gold_before) {
    ai_diplo_status_human_pair(ctx, nation_a, nation_b, "Alliance with %s costs gold.");
    alliance_chrome = 1;
  }
  /* FUN_5bfb_13b0 / 15b3 alliance chrome → OK popup; FA 3f41 full UI PARKED. */
  if (alliance_chrome && ctx->status && ctx->status[0] != '\0') {
    ai_diplo_popup_ok(
      ctx, AI_POPUP_TAG_DIPLO_ALLIANCE, nation_a, nation_b, "Alliance", ctx->status
    );
  }
}

void ai_diplo_break_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  const int was_ally = (ai_diplo_read(col1, nation_a, nation_b) & AI_DIPLO_ALLY) != 0;
  ai_diplo_clear_both(col1, nation_a, nation_b, AI_DIPLO_ALLY);
  ai_diplo_or_both(col1, nation_a, nation_b, AI_DIPLO_PEACE);
  /* Trust loss stand-in (FA 3f41 PARKED): −20 gold each side if they were allied. */
  if (was_ally) {
    ai_diplo_break_trust_penalty(col1, nation_a, nation_b);
    /* Indians wary of Euro treachery: −5 relation + sticky sync both sides. */
    ai_diplo_break_indian_sticky_raise(col1, nation_a, nation_b);
  }
}

/* Thin 102a/1092 status when human is a party (Contact/King pattern).
 * AI callers keep using break_alliance without status. FA dialog UI PARKED.
 * Sticky-rise "Natives grow hostile." also enqueues OK (FUN_15b3 / 5bfb). */
void ai_diplo_break_alliance_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b) {
  if (!ctx || !ctx->col1) {
    return;
  }
  const int was_ally =
    (ai_diplo_read(ctx->col1, nation_a, nation_b) & AI_DIPLO_ALLY) != 0;
  const int human = ctx->human_nation;
  uint8_t sticky_before = AI_DIPLO_STICKY_CLEAR;
  if (human >= 0 && human < 4) {
    sticky_before = ai_diplo_indian_hostility_sticky(ctx->col1, human);
  }
  ai_diplo_break_alliance(ctx->col1, nation_a, nation_b);
  if (was_ally) {
    ai_diplo_status_human_pair(ctx, nation_a, nation_b, "Alliance broken with %s");
    /*
     * Prefer sticky-rise chrome when break −5 Indian hit newly raises human
     * hostility sticky (same Contact/King pattern as declare_war_ctx).
     * Source: Indians wary of Euro treachery; FA 3f41 UI PARKED.
     * Also enqueue AI OK popup (FUN_15b3 / 5bfb); status kept — sticky
     * native line uses INFO tag via tag_from_status.
     */
    if (human >= 0 && human < 4 && (nation_a == human || nation_b == human) &&
        ctx->status && ctx->status_size > 0) {
      const uint8_t sticky_after = ai_diplo_indian_hostility_sticky(ctx->col1, human);
      if (sticky_before == AI_DIPLO_STICKY_CLEAR &&
          sticky_after != AI_DIPLO_STICKY_CLEAR) {
        snprintf(ctx->status, ctx->status_size, "Natives grow hostile.");
      }
      ai_diplo_popup_ok(
        ctx,
        ai_diplo_tag_from_status(ctx->status, AI_POPUP_TAG_DIPLO_BREAK),
        nation_a,
        nation_b,
        "Diplomacy",
        ctx->status
      );
    }
  }
}

void ai_diplo_treaty_timers(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  /* 6d8e step 4: decrement per-rival treaty timer bytes before planning. */
  for (int other = 0; other < 4; ++other) {
    if (other == nation_id) {
      continue;
    }
    uint8_t* t = ai_diplo_timer_byte(ctx->col1, nation_id, other);
    if (!t) {
      continue;
    }
    if (*t > 0) {
      (*t)--;
    }
    if (*t != 0) {
      continue;
    }
    /* Expiry: break alliance if allied; else thin peace/met tweak.
     * Use stored flags (not ai_diplo_read virtual PEACE|MET for unmet 0) so
     * early turns do not stamp euro_relation in seed-100 goldens. */
    uint8_t* f = ai_diplo_flag_byte(ctx->col1, nation_id, other);
    if (!f) {
      continue;
    }
    const uint8_t stored = *f;
    if (stored & AI_DIPLO_ALLY) {
      ai_diplo_break_alliance_ctx(ctx, nation_id, other);
      continue;
    }
    if (ctx->rng && (stored & AI_DIPLO_MET) && dos_rng_range(ctx->rng, 1, 8) == 1) {
      *f = (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_MET);
      ai_diplo_mirror_relation_summary(ctx->col1, nation_id);
    }
  }
  /* Peaceful Indian relation drift (thin; full Indian×Euro 15b3 PORT DEBT). */
  ai_diplo_indian_peaceful_drift(ctx->col1, nation_id);
}

int ai_diplo_military_score(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  int score = 0;
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
      if (t) {
        score += t->attack + t->defense;
        if (t->domain == COLONIZE_UNIT_DOMAIN_SEA) {
          score += 3; /* thin naval weight (5bfb_00f8-ish) */
        }
      }
    }
  }
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      score += c->population * 2;
      if (colonies_has_fortification(ctx->colonies, c)) {
        score += 5;
      }
    }
  }
  /* Treasury / trade capacity stand-in (153e military+trade blend). */
  if (ctx->col1_ok && ctx->col1) {
    score += (int)(ctx->col1->nation[nation_id].gold / 50u);
  }
  /* FUN_5bfb_00f8 place: stronger (rank 0) → +6, weakest → 0. */
  if (ctx->euro_power_rank_ok) {
    const int place = (int)ctx->euro_power_rank[nation_id];
    if (place >= 0 && place < 4) {
      score += (3 - place) * 2;
    }
  }
  return score;
}

void ai_diplo_euro_balance(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  /* Reference-only: keep the 153e worthiness-score structural port and
   * its selector-table documentation reachable/compiled without a live
   * caller (matches the ai_euro_5d04_nation_planning_structural
   * precedent — see euro_diplo_153e_full.md). */
  (void)ai_diplo_153e_worthiness_score_structural;
  (void)ai_diplo_153e_selector_table;
  (void)AI_DIPLO_153E_SELECTOR_COUNT;
  /*
   * FUN_5bfb_10ec / 13b0 checklist:
   *  1 skip human; at-war → upkeep + privateer prize; war-fatigue + near-parity
   *    → make_peace_ctx (timer==0 while WAR)
   *  2 military score (0000/00f8/312e stand-in)
   *  3 10ec eligibility: war if self ≫ other; ally if near-parity
   *  4 13b0 form/break + thin ally aid / FA gift / longevity (FA 3f41 PARKED)
   *  5 declare_war_ctx → thin 153e gold+tax + human status (102a/1092 chrome)
   *  + Indian matrix: feeler (skip sticky2 / any Euro war) + sticky sync/pressure
   *    + harassment; sticky2 also refuses new alliances + skips FA gift (no gold)
   *  + Franklin (fandom): NW pair with FF → always offer/conclude peace; skip
   *    10ec declare pressure (king Euro wars must not poison NW peers)
   */
  ai_diplo_indian_matrix_tick(ctx, nation_id);
  const uint8_t sticky_now = ai_diplo_indian_hostility_sticky(ctx->col1, nation_id);
  const int self = ai_diplo_military_score(ctx, nation_id);
  int war_upkeep_status_done = 0;
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation_id || ctx->col1->player[peer].control == 2) {
      continue;
    }
    const int other = ai_diplo_military_score(ctx, peer);
    const uint8_t bits = ai_diplo_read(ctx->col1, nation_id, peer);
    const int franklin = ai_diplo_franklin_pair(ctx->col1, nation_id, peer);

    if (bits & AI_DIPLO_WAR) {
      /*
       * Franklin: Europeans in the New World always offer peace (fandom).
       * Conclude peace for AI↔AI / human-as-actor; AI→human enqueues CHOICE.
       * Skips upkeep/privateer for this peer — negotiations stay peaceful.
       * Source: docs/fandom_col1994.md Benjamin Franklin; FA 3f41 UI PARKED.
       */
      if (franklin) {
        if (ctx->ai_popups && peer == ctx->human_nation) {
          if (!ai_diplo_popup_pair_queued(
                ctx->ai_popups, AI_POPUP_TAG_DIPLO_PEACE, nation_id, peer
              )) {
            char body[AI_POPUP_BODY_LEN];
            snprintf(
              body,
              sizeof(body),
              "%s offers peace.",
              ai_diplo_rival_name(ctx->col1, nation_id)
            );
            if (ctx->status && ctx->status_size > 0) {
              snprintf(ctx->status, ctx->status_size, "%s", body);
            }
            const char* labels[] = {"Accept", "Refuse"};
            const int ids[] = {1, 2};
            (void)ai_popup_enqueue_choice_ctx(
              ctx->ai_popups,
              AI_POPUP_TAG_DIPLO_PEACE,
              nation_id,
              peer,
              0,
              "Peace",
              body,
              labels,
              ids,
              2
            );
          }
        } else {
          ai_diplo_make_peace_ctx(ctx, nation_id, peer);
        }
        continue;
      }
      /* Thin ongoing 153e friction: 5 gold/turn while gold>0 (per war peer). */
      const uint16_t gold_before_upkeep = ctx->col1->nation[nation_id].gold;
      ai_diplo_war_upkeep_drain(&ctx->col1->nation[nation_id]);
      /*
       * Thin war-upkeep human chrome once per euro_balance tick (not per peer).
       * Prefer later privateer / peace status if those fire. FA UI PARKED.
       * Source: Contact/King 102a/1092 status stand-in; 153e upkeep drain.
       */
      if (!war_upkeep_status_done && gold_before_upkeep > 0 &&
          ctx->human_nation == nation_id && ctx->status && ctx->status_size > 0) {
        /* Status only — no GAME.TXT war-upkeep dialog. */
        snprintf(ctx->status, ctx->status_size, "War upkeep costs gold.");
        war_upkeep_status_done = 1;
      }
      /*
       * Wartime Privateer: spawn-only when ctx->units is set (unknown26[9] gate;
       * hunt-ready coast / New World sea stack / Europe dock). ai_euro naval
       * hunt + units_resolve_naval_combat hold plunder (units_plunder_ship_holds)
       * owns cargo-raid outcomes — no diplo gold fiction.
       * PARKED null-units only: thin 8g richer→poorer treasury stand-in
       * (AI_DIPLO_PRIVATEER_PRIZE_GOLD) when no units pool for spawn/combat.
       * Human chrome: commission / "Privateer prize from %s" (null-units only).
       * Source: Europe Privateer; fandom Drake; euro_unit_act §2b; FUN_5fef_016c.
       */
      if (ctx->units) {
        if (ai_diplo_war_privateer_spawn(ctx, nation_id, peer)) {
          /* Status only — no DOS GAME.TXT dialog for wartime Privateer spawn. */
          ai_diplo_status_human_pair(
            ctx, nation_id, peer, "Privateer commissioned against %s"
          );
        }
      } else if (ai_diplo_war_privateer_prize(ctx->col1, nation_id, peer)) {
        /* Status only — no GAME.TXT Privateer prize dialog. */
        ai_diplo_status_human_pair(ctx, nation_id, peer, "Privateer prize from %s");
      }
      /*
       * War-fatigue peace: near-parity (ally-eligible band) while at war AND
       * peer treaty timer==0 (war aged; seeded to 8 on first declare) → rare
       * make_peace. Human chrome when either party is human (102a/1092
       * stand-in via status_human_pair + Tools lift on human bitmap).
       * AI→human (FUN_5bfb / 15b3): enqueue CHOICE Accept/Refuse; apply calls
       * make_peace_ctx. AI↔AI / human-as-actor still auto make_peace_ctx.
       * Full 153e / FA 3f41 peace dialog UI PARKED; no gold cost.
       * Source: extend existing near-parity path; timer==0 + WAR = fatigue.
       */
      if (self > 10 && other > 10 && abs(self - other) < 15) {
        uint8_t* t = ai_diplo_timer_byte(ctx->col1, nation_id, peer);
        if (t && *t == 0 && ctx->rng && dos_rng_range(ctx->rng, 1, 30) == 1) {
          if (ctx->ai_popups && peer == ctx->human_nation) {
            /* Skip spam if a peace offer for this pair is already queued. */
            if (!ai_diplo_popup_pair_queued(
                  ctx->ai_popups, AI_POPUP_TAG_DIPLO_PEACE, nation_id, peer
                )) {
              char body[AI_POPUP_BODY_LEN];
              snprintf(
                body,
                sizeof(body),
                "%s offers peace.",
                ai_diplo_rival_name(ctx->col1, nation_id)
              );
              if (ctx->status && ctx->status_size > 0) {
                snprintf(ctx->status, ctx->status_size, "%s", body);
              }
              const char* labels[] = {"Accept", "Refuse"};
              const int ids[] = {1, 2};
              (void)ai_popup_enqueue_choice_ctx(
                ctx->ai_popups,
                AI_POPUP_TAG_DIPLO_PEACE,
                nation_id,
                peer,
                0,
                "Peace",
                body,
                labels,
                ids,
                2
              );
            }
          } else {
            ai_diplo_make_peace_ctx(ctx, nation_id, peer);
          }
        }
      }
      continue;
    }

    /* Thin FA: ally-aid (10g) + expiring-timer gift / longevity (3f41 PARKED). */
    if (bits & AI_DIPLO_ALLY) {
      ai_diplo_ally_foreign_aid(ctx->col1, nation_id, peer);
      uint8_t* t = ai_diplo_timer_byte(ctx->col1, nation_id, peer);
      if (t && *t == 1) {
        /*
         * Sticky→pressure deepen: sticky==2 skips FA gift to peers (no gold
         * transfer) — deep native hostility refuses gift diplomacy. Longevity
         * timer+1 still applies (no gold). Source: fandom alarmed refuse gifts;
         * contact friction <40 inverted. Ally-aid (10g) unchanged.
         */
        if (sticky_now != AI_DIPLO_STICKY_DEEP) {
          ai_diplo_fa_gift(ctx->col1, nation_id, peer);
        }
        /*
         * Alliance longevity: if FA gift gold gates failed (or sticky skipped),
         * timer still 1 → bump +1 both dirs without a second gold transfer.
         * Source: 13b0/3f41 treaty sustain; FA dialog UI PARKED.
         */
        if (*t == 1) {
          ai_diplo_ally_longevity_timer(ctx->col1, nation_id, peer);
        }
        /*
         * Human chrome when FA gift / longevity fires (102a/1092 stand-in).
         * Gift success: timer 1→3 (+2). Longevity-only: timer 1→2 (+1).
         * Skip when sticky==2 so "Natives remain hostile." stays preferred.
         * Source: thin 3f41 treaty sustain; FA dialog UI PARKED.
         */
        if (sticky_now != AI_DIPLO_STICKY_DEEP && ctx->status && ctx->status_size > 0 &&
            (ctx->human_nation == nation_id || ctx->human_nation == peer)) {
          int fa_chrome = 0;
          if (*t == 3) {
            ai_diplo_status_human_pair(ctx, nation_id, peer,
                                      "Alliance with %s strengthened.");
            fa_chrome = 1;
          } else if (*t == 2) {
            ai_diplo_status_human_pair(ctx, nation_id, peer, "Alliance with %s holds.");
            fa_chrome = 1;
          }
          /*
           * Thin FA report status (gift/longevity). Full 3f41 F2–F9 UI PARKED;
           * no invented INFO OK modal.
           */
          (void)fa_chrome;
        }
      }
    }

    /* 13b0 break: imbalance while allied (human status via _ctx). */
    if ((bits & AI_DIPLO_ALLY) && self > other * 2 + 25 && self > 40) {
      if (ctx->rng && dos_rng_range(ctx->rng, 1, 25) == 1) {
        /*
         * AI→human (FUN_5bfb_13b0 / 15b3): enqueue CHOICE Accept/Refuse;
         * apply calls break_alliance_ctx. AI↔AI / human-as-actor still auto
         * break_alliance_ctx. Treaty-timer expiry stays automatic (not CHOICE).
         * FA 3f41 full UI PARKED.
         */
        if (ctx->ai_popups && peer == ctx->human_nation) {
          if (!ai_diplo_popup_pair_queued(
                ctx->ai_popups, AI_POPUP_TAG_DIPLO_BREAK, nation_id, peer
              )) {
            char body[AI_POPUP_BODY_LEN];
            snprintf(
              body,
              sizeof(body),
              "%s breaks the alliance.",
              ai_diplo_rival_name(ctx->col1, nation_id)
            );
            if (ctx->status && ctx->status_size > 0) {
              snprintf(ctx->status, ctx->status_size, "%s", body);
            }
            const char* labels[] = {"Accept", "Refuse"};
            const int ids[] = {1, 2};
            (void)ai_popup_enqueue_choice_ctx(
              ctx->ai_popups,
              AI_POPUP_TAG_DIPLO_BREAK,
              nation_id,
              peer,
              0,
              "Alliance",
              body,
              labels,
              ids,
              2
            );
          }
        } else {
          ai_diplo_break_alliance_ctx(ctx, nation_id, peer);
        }
      }
      continue;
    }

    /* 10ec war eligibility. */
    if (self > other * 2 + 20 && self > 30) {
      /*
       * Franklin: skip declare-war pressure against NW Euro peers (fandom —
       * king's European wars / opportunistic war must not poison NW relations).
       * Source: docs/fandom_col1994.md Benjamin Franklin.
       */
      if (franklin) {
        continue;
      }
      if (ctx->rng && dos_rng_range(ctx->rng, 1, 20) == 1) {
        /*
         * AI→human (FUN_5bfb / 15b3): enqueue CHOICE Accept/Refuse; apply calls
         * declare_war_ctx. AI↔AI / human-as-actor still auto declare_war_ctx.
         * Thin 153e sting inside declare_war; full body / 12d0 / FA UI PARKED.
         */
        if (ctx->ai_popups && peer == ctx->human_nation) {
          if (!ai_diplo_popup_pair_queued(
                ctx->ai_popups, AI_POPUP_TAG_DIPLO_WAR, nation_id, peer
              )) {
            /*
             * @CANCELPEACE ("{%STRING0} cancel peace treaty with {%STRING1}.")
             * — authentic prompt for the AI-initiated war CHOICE; Accept lets
             * the declaration stand (declare_war_ctx), Refuse averts it.
             */
            char body[AI_POPUP_BODY_LEN];
            PopupMsgTokens tok;
            memset(&tok, 0, sizeof(tok));
            tok.string0 = ai_diplo_rival_name(ctx->col1, nation_id);
            tok.string1 = ai_diplo_rival_name(ctx->col1, peer);
            popup_msg_fill(
              ctx->messages, "CANCELPEACE", &tok,
              "%STRING0 cancel peace treaty with %STRING1.",
              body, sizeof(body)
            );
            if (ctx->status && ctx->status_size > 0) {
              snprintf(ctx->status, ctx->status_size, "%s", body);
            }
            const char* labels[] = {"Accept", "Refuse"};
            const int ids[] = {1, 2};
            (void)ai_popup_enqueue_choice_ctx(
              ctx->ai_popups,
              AI_POPUP_TAG_DIPLO_WAR,
              nation_id,
              peer,
              0,
              "War",
              body,
              labels,
              ids,
              2
            );
          }
        } else {
          ai_diplo_declare_war_ctx(ctx, nation_id, peer);
        }
      }
      continue;
    }

    /* 10ec/13b0 ally eligibility. */
    if (self > 10 && other > 10 && abs(self - other) < 15) {
      /* Stored flags only — ai_diplo_read invents PEACE|MET for unmet 0. */
      const uint8_t* raw_f = ai_diplo_flag_byte_const(ctx->col1, nation_id, peer);
      const uint8_t stored = raw_f ? *raw_f : 0;
      if ((stored & AI_DIPLO_ALLY) == 0) {
        /*
         * Sticky→pressure deepen (unpark #5): sticky==2 refuses new alliances
         * this balance — deep native hostility blocks the improve-relations /
         * treaty path. Source: fandom Indians — alarmed/hostile may refuse
         * trade/gifts; contact friction <40 inverted. Existing ALLY kept.
         * Raw form_alliance API still available for tests / scripted paths.
         */
        if (sticky_now == AI_DIPLO_STICKY_DEEP) {
          if (ctx->human_nation == nation_id && ctx->status && ctx->status_size > 0 &&
              ctx->rng && dos_rng_range(ctx->rng, 1, 40) == 1) {
            /* Status only — no GAME.TXT native-unrest dialog. */
            snprintf(ctx->status, ctx->status_size,
                     "Native unrest precludes new alliances.");
          }
        } else if (stored == 0) {
          /*
           * Unmet peer (euro_relation==0): do not invent PEACE|MET|ALLY.
           * Seed-100 early TURN goldens keep flags clear until real contact.
           * Cite: docs/ai_transcription.md joint diplo fields; euro_diplo.md.
           */
        } else if (ctx->rng && dos_rng_range(ctx->rng, 1, 40) == 1) {
          /*
           * Human-offer path (FUN_5bfb_13b0): AI nation offers alliance to the
           * human peer → CHOICE Accept/Refuse (apply via ai_diplo_apply_popup_result).
           * AI↔AI still auto-forms via form_alliance_ctx.
           */
          if (ctx->ai_popups && peer == ctx->human_nation) {
            char body[AI_POPUP_BODY_LEN];
            snprintf(
              body,
              sizeof(body),
              "%s offers an alliance.",
              ai_diplo_rival_name(ctx->col1, nation_id)
            );
            if (ctx->status && ctx->status_size > 0) {
              snprintf(ctx->status, ctx->status_size, "%s", body);
            }
            const char* labels[] = {"Accept", "Refuse"};
            const int ids[] = {1, 2};
            (void)ai_popup_enqueue_choice_ctx(
              ctx->ai_popups,
              AI_POPUP_TAG_DIPLO_ALLIANCE,
              nation_id,
              peer,
              0,
              "Alliance",
              body,
              labels,
              ids,
              2
            );
          } else {
            ai_diplo_form_alliance_ctx(ctx, nation_id, peer);
          }
        }
      }
    }
  }
}

void ai_diplo_euro_timers(ColonizeTurnContext* ctx, int nation_id) {
  ai_diplo_treaty_timers(ctx, nation_id);
}

void ai_diplo_indian_relation_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
) {
  /*
   * FUN_4cc6_00f2 / FUN_15dc_00e0-shaped scalar store on Euro nation record.
   * Full Indian×Euro 15b3 bilateral matrix is PORT DEBT (see euro_diplo.md).
   */
  if (!col1 || euro_nation < 0 || euro_nation >= 4) {
    return;
  }
  int idx = indian_nation - 4;
  if (idx < 0 || idx >= 8) {
    return;
  }
  /* Floor 0 / cap 255 — war −5 (+ optional −10 deepen) must not underflow.
   * Source: FUN_4cc6_00f2 / 15dc_00e0 scalar clamp; fandom relation band. */
  int v = (int)col1->nation[euro_nation].relation_by_indian[idx] + delta;
  if (v < 0) {
    v = 0;
  }
  if (v > 255) {
    v = 255;
  }
  col1->nation[euro_nation].relation_by_indian[idx] = (uint8_t)v;
}

uint8_t ai_diplo_indian_relation(
  const ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation
) {
  /* Read-only getter for contact/king; same indexing as relation_delta. */
  if (!col1 || euro_nation < 0 || euro_nation >= 4) {
    return 0;
  }
  int idx = indian_nation - 4;
  if (idx < 0 || idx >= 8) {
    return 0;
  }
  return col1->nation[euro_nation].relation_by_indian[idx];
}

void ai_diplo_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup) {
  if (!ctx || !popup || !popup->has_result || popup->result_cancelled) {
    return;
  }
  /*
   * FUN_5bfb_13b0 alliance offer CHOICE: Accept (1) → form_alliance_ctx
   * (status + follow-up OK "Alliance formed with %s" / gold-drain chrome);
   * Refuse (2) → status only. OK popups share DIPLO_ALLIANCE tag with
   * choice_id 0 — ignore those. Source: 15b3 / 5bfb; FA 3f41 full UI PARKED.
   *
   * War-fatigue peace offer CHOICE (FUN_5bfb / 15b3): Accept (1) →
   * make_peace_ctx; Refuse (2) → status + follow-up OK (chrome polish;
   * human-facing status also enqueues OK). Full 153e peace UI PARKED.
   *
   * 10ec war declare CHOICE AI→human: Accept (1) → declare_war_ctx;
   * Refuse (2) → status + follow-up OK (chrome polish; mirrors peace/
   * break Refuse). OK popups share DIPLO_WAR + choice_id 0.
   *
   * 13b0 break-alliance CHOICE AI→human: Accept (1) → break_alliance_ctx;
   * Refuse (2) → status + OK (ally kept). OK popups share DIPLO_BREAK +
   * choice_id 0. FA 3f41 full UI PARKED.
   */
  if (popup->result_tag == AI_POPUP_TAG_DIPLO_ALLIANCE) {
    if (popup->result_choice_id == 1) {
      /* Follow-up OK enqueued inside form_alliance_ctx when ai_popups set. */
      ai_diplo_form_alliance_ctx(ctx, popup->result_nation_a, popup->result_nation_b);
    } else if (popup->result_choice_id == 2) {
      if (ctx->status && ctx->status_size > 0) {
        ai_diplo_status_human_pair(
          ctx,
          popup->result_nation_a,
          popup->result_nation_b,
          "Alliance refused with %s"
        );
      }
    }
    return;
  }
  if (popup->result_tag == AI_POPUP_TAG_DIPLO_PEACE) {
    if (popup->result_choice_id == 1) {
      ai_diplo_make_peace_ctx(ctx, popup->result_nation_a, popup->result_nation_b);
    } else if (popup->result_choice_id == 2) {
      if (ctx->status && ctx->status_size > 0) {
        ai_diplo_status_human_pair(
          ctx,
          popup->result_nation_a,
          popup->result_nation_b,
          "Peace refused with %s"
        );
        /* Status only — refuse follow-up INFO OK was invented (FA UI PARKED). */
      }
    }
    return;
  }
  if (popup->result_tag == AI_POPUP_TAG_DIPLO_WAR) {
    if (popup->result_choice_id == 1) {
      ai_diplo_declare_war_ctx(ctx, popup->result_nation_a, popup->result_nation_b);
    } else if (popup->result_choice_id == 2) {
      if (ctx->status && ctx->status_size > 0) {
        ai_diplo_status_human_pair(
          ctx,
          popup->result_nation_a,
          popup->result_nation_b,
          "War refused with %s"
        );
        /* Status only — refuse follow-up INFO OK was invented (FA UI PARKED). */
      }
    }
    return;
  }
  if (popup->result_tag == AI_POPUP_TAG_DIPLO_BREAK) {
    if (popup->result_choice_id == 1) {
      ai_diplo_break_alliance_ctx(ctx, popup->result_nation_a, popup->result_nation_b);
    } else if (popup->result_choice_id == 2) {
      if (ctx->status && ctx->status_size > 0) {
        ai_diplo_status_human_pair(
          ctx,
          popup->result_nation_a,
          popup->result_nation_b,
          "Alliance break refused with %s"
        );
        /* Status only — refuse follow-up INFO OK was invented (FA UI PARKED). */
      }
    }
  }
}
