#include "core/ai_diplo.h"
#include "core/ai_contact.h"
#include "core/ai_euro.h"

#include "core/ai_popup.h"
#include "core/colony.h"
#include "core/combat_strength.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/units.h"
#include "core/woodcut.h"

#include <stddef.h>
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
 * FA 3f41 full body/UI PARKED. Linux-only alliance machinery (ally-aid,
 * FA gift, break trust) retired T2.4 2026-09-06.
 * Euro×Euro war does NOT boycott Europe cargos (DOS tea-party / king refuse
 * only). An invented wartime all-16-bit embargo was removed after
 * all_boycotted.SAV (1516, tax 0, boycott_bitmap 0xFFFF). Peace still
 * lifts leftover wartime bits on poisoned saves. The full per-rival 153e
 * audience is LIVE (ai_diplo_153e_encounter below, 2026-09-06 close-out);
 * this sting remains the declare-war side-effect layer. */

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
#define AI_DIPLO_INDIAN_DRIFT_CAP 160u
#define AI_DIPLO_WAR_INDIAN_HIT 5
/* At-war gate: relation < 50 (same band as contact alarm≥50 mission block). */
#define AI_DIPLO_INDIAN_AT_WAR_REL 26 /* alarm > 0x4a (FUN_5bfb_153e hostile tier) */
/* Very-low deepen: relation < 40 (contact peaceful-gift friction < 40 inverted). */
#define AI_DIPLO_INDIAN_VERY_LOW_REL 16 /* alarm >= 85: sticky deepen band (Linux) */
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
 * drain AI_DIPLO_WAR_TRADE_STING from the richer treasury (floor 0). Gap
 * deepen is gold-only — Euro war does not boycott Europe cargos.
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
 * Lift leftover wartime @CARGO embargo bits when a nation has no remaining
 * Euro×Euro wars. Declare no longer sets these (DOS tea-party only); this
 * still repairs poisoned 0xFFFF saves on peace. Raw PEACE writes do not lift.
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

/*
 * Peaceful Indian×Euro relation drift (not full 15b3 matrix).
 * Per tick: for each of 8 Indian slots already contacted (r>0), if < 160 and
 * Euro not at war → +1 (cap 160). Do not invent contact from r==0 (seed-100
 * early goldens keep relation_by_indian at 0 until meet). Source: 6d8e §4;
 * fandom alarm cools without encroachment.
 */
static void ai_diplo_indian_peaceful_drift(ColonizeCol1Save* col1, int nation_id) {
  /*
   * Retired 2026-08-27: DOS has no per-turn Indian alarm decay (alarm_by_player
   * is byte-stable across seed-100 TURN3..7 saves; the only ±1 moves are the
   * FUN_4d56_152e accumulator, ported in ai.c). Kept as a no-op so the tick
   * shape/callers stay put.
   */
  (void)col1;
  (void)nation_id;
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
  /* Retired 2026-08-27 with the drift above — no DOS counterpart (see there). */
  (void)col1;
  (void)nation_id;
  return 0;
}

/*
 * Retired 2026-09-03 (bugs: attacking a Spanish colony popped "Natives grow
 * hostile"): no DOS declare-war site (5fef_1b0e 0x53c8 clears, 153e, 684c_08c0,
 * 6cb2_24b8) touches Indian relations — DOS grows alarm only through the
 * FUN_4d56_152e accumulator (see alarm-fandom-drips retirement). The −5×8
 * "Indians dislike Euro×Euro war" hit was a fandom stand-in.
 */
static void ai_diplo_war_indian_relation_hit(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  (void)col1;
  (void)nation_a;
  (void)nation_b;
}

uint8_t ai_diplo_indian_read(const ColonizeCol1Save* col1, int euro_nation, int indian_idx) {
  if (!col1 || euro_nation < 0 || euro_nation >= 4 || indian_idx < 0 || indian_idx >= 8) {
    return 0;
  }
  if ((col1->indian[indian_idx].euro_diplo[euro_nation] & COL1_INDIAN_MET_BIT) == 0) {
    return 0; /* unmet */
  }
  const int r = 100 - ai_diplo_indian_alarm(col1, 4 + indian_idx, euro_nation);
  return (uint8_t)(r < 1 ? 1 : r);
}

int ai_diplo_indian_at_war(const ColonizeCol1Save* col1, int euro_nation, int indian_idx) {
  const uint8_t r = ai_diplo_indian_read(col1, euro_nation, indian_idx);
  if (r == 0) {
    return 0; /* unmet */
  }
  /* FUN_5bfb_153e: alarm > 0x4a, or the diplo WAR bit (FUN_1000_8c28 & 2). */
  if ((col1->indian[indian_idx].euro_diplo[euro_nation] & COL1_INDIAN_WAR_BIT) != 0) {
    return 1;
  }
  return r < AI_DIPLO_INDIAN_AT_WAR_REL;
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
 * Sync unknown26[8] from the Indian alarm matrix (via ai_diplo_indian_read).
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
  /* relation_by_indian is the DOS 0x60 MET|PEACE flag byte, not a scalar. */
  col1->nation[euro_nation].relation_by_indian[idx] = (uint8_t)AI_DIPLO_INDIAN_PEACE_MEET;
  if (ind->alarm_by_player[euro_nation] > 20u) {
    ind->alarm_by_player[euro_nation] = 20u; /* FUN_5bfb first-contact clamp (:96624) */
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
  /* head.nation_relation (DS:0x53c8) is NOT a relation summary: DOS uses it as
   * the per-nation Crown-war turn stamp (FUN_38fd_5930 writes turn; every
   * attack/declare site zeroes both nations' slots). The old WAR/ALLY mirror
   * was dropped 2026-08-27; see ai_diplo_declare_war / ai_king_new_war_event. */
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
  /* Raw byte. 2026-08-27: the old "unwritten = PEACE|MET" synthesis is gone —
   * DOS reads DS:-0x77c4 directly and an unmet pair is 0 there. */
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
  /* DOS attack/declare sites (5fef_1b0e, 684c_08c0, 6cb2_24b8): DS:0x53c8[a]=[b]=0. */
  if (nation_a >= 0 && nation_a < 4 && nation_b >= 0 && nation_b < 4) {
    col1->head.nation_relation[nation_a] = 0;
    col1->head.nation_relation[nation_b] = 0;
  }
  ai_diplo_clear_both(col1, nation_a, nation_b, AI_DIPLO_PEACE);
  ai_diplo_or_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_MET));
  /* Thin 153e-shaped sting: gold drain + tax bump both sides (relation via mirror). */
  if (!already) {
    ai_diplo_war_treasury_sting(col1, nation_a, nation_b);
    ai_diplo_war_tax_bump(col1, nation_a, nation_b);
    /* Indians dislike Euro×Euro war (scalar stand-in; full 15b3 PARKED). */
    ai_diplo_war_indian_relation_hit(col1, nation_a, nation_b);
    /* Unpark #5: colony-gap rich-side trade sting. */
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
  (void)title;
  (void)ai_popup_enqueue_ok_ctx(
    ctx->ai_popups, tag, nation_a, nation_b, 0, NULL, body
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
  popup_msg_strip_markup(ctx->status); /* status line: no {} coloring */
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
  popup_msg_strip_markup(ctx->status); /* status line: no {} coloring */
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

/* Full wartime 16-bit embargo mask (lift leftover bits; declare no longer sets). */
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

/* Sound hook (unit tests build ai_diplo.c without sound.c). */
static void (*g_ai_diplo_sound_play)(int id) = NULL;
void ai_diplo_set_sound_hook(void (*play_fn)(int id)) {
  g_ai_diplo_sound_play = play_fn;
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
    if (human >= 0 && human < 4 && (nation_a == human || nation_b == human)) {
      if (g_ai_diplo_sound_play) {
        g_ai_diplo_sound_play(0x8020); /* FUN_5bfb_153e 5bfb:2c7c: war-declaration chord sting */
      }
    }
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
 * the 13b0 treaty-sign path (both sides' border garrisons refreshed).
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
  {7, "3bee", "FUN_5bfb_13b0", "ai_diplo_13b0_treaty_tick sign/cancel (Done)"},
  /* idx4, offset 3bdf — raw line 485, the ONLY selector call inside the
   * worthiness-score phase itself: the per-colony border probe inside the
   * colony loop. See ai_diplo_153e_border_probe below (full port). */
  {4, "3bdf", "FUN_5bfb_0000", "ai_diplo_153e_border_probe (Done)"},
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
 * FUN_5bfb_0000 (selector idx4) — colony-border stack probe, full port
 * (2026-08-27). DOS is called once per colony (DS:0x8542 = the colony
 * selected by FUN_281f_09e6 in 153e's loop over DS:0x539e colonies) with
 * `param_4 = target`. Body: stack-query opcode 0xa (# military land units,
 * FUN_1427_0d38) on the colony tile's own stack -> *out_colony_military;
 * then for each of the 8 neighbours: the stack there, its owner nibble,
 * its opcode-0xa count; when the owner is `target`: ret += opcode 0xb
 * (Σ FUN_157e_004a(unit,1) over the stack) >> 3, *out_adjacent_military
 * += count, and *out_matched = target whenever count >= running max
 * (max starts at 0, so any adjacent target stack matches).
 * Earlier this was an inert stub; the "not independently ported" note is
 * retired — the body above is the decompile at viceroy_unpacked.c:96448.
 */
typedef struct Ai153eBorderProbe {
  int value;             /* raw local_a: Σ (adjacent target stack combat >> 3) */
  int matched_target;    /* raw local_10: target, or -1 */
  int colony_military;   /* raw local_4: opcode 0xa on the colony tile stack */
  int adjacent_military; /* raw local_16: Σ opcode 0xa over adjacent target stacks */
} Ai153eBorderProbe;

/* FUN_1427_0d38 opcode 0xa: land units with attack > 1 on (x,y). */
static int ai_diplo_stack_military_count(const ColonizeTurnContext* ctx, int x, int y) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active || u->aboard_ship_id >= 0 || u->x != x || u->y != y) {
      continue;
    }
    const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
    if (t && t->attack > 1 && t->domain != COLONIZE_UNIT_DOMAIN_SEA) {
      n++;
    }
  }
  return n;
}

/* FUN_1427_0d38 opcode 0xb: Σ 004a(unit, mode 1) for units whose domain matches the tile. */
static int ai_diplo_stack_attack_sum(const ColonizeTurnContext* ctx, int x, int y) {
  ColonizeCombatStrengthCtx sctx;
  sctx.units = ctx->units;
  sctx.map = ctx->map;
  sctx.colonies = ctx->colonies;
  sctx.col1 = ctx->col1;
  const int tile_land = ctx->map ? map_tile_is_land(ctx->map, x, y) : 1;
  int sum = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active || u->aboard_ship_id >= 0 || u->x != x || u->y != y) {
      continue;
    }
    const int is_sea = units_is_sea(ctx->units, i);
    if ((tile_land && is_sea) || (!tile_land && !is_sea)) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, i, 1, NULL);
  }
  return sum;
}

static Ai153eBorderProbe ai_diplo_153e_border_probe(
  const ColonizeTurnContext* ctx, int colony_x, int colony_y, int target
) {
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  Ai153eBorderProbe r;
  r.value = 0;
  r.matched_target = -1;
  r.colony_military = 0;
  r.adjacent_military = 0;
  if (!ctx || !ctx->units) {
    return r;
  }
  if (units_id_at(ctx->units, colony_x, colony_y) >= 0) {
    r.colony_military = ai_diplo_stack_military_count(ctx, colony_x, colony_y);
  }
  int best = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = colony_x + dx[d];
    const int ny = colony_y + dy[d];
    const int uid = units_id_at(ctx->units, nx, ny);
    if (uid < 0) {
      continue;
    }
    const ColonizeUnit* u = units_get_const(ctx->units, uid);
    if (!u) {
      continue;
    }
    const int mil = ai_diplo_stack_military_count(ctx, nx, ny);
    if (u->nation_id == target) {
      r.value += ai_diplo_stack_attack_sum(ctx, nx, ny) >> 3;
      r.adjacent_military += mil;
      if (best <= mil) {
        best = mil;
        r.matched_target = target;
      }
    }
  }
  return r;
}

/*
 * FUN_5bfb_00f8 — nation rank table (DS:0xa150..0xa153). score[n] =
 * gold/100 + colony_counts*2 + census_pop_proxy + land_combat_strength;
 * the 4 indices are sorted ascending by score (FUN_291f_0ed0), so
 * DS:0xa153 is the top-ranked nation. Ties: stable ascending sort keeps
 * index order, so the highest index among equal scores lands on top.
 */
int ai_diplo_00f8_top_ranked_nation(const ColonizeCol1Save* col1) {
  if (!col1) {
    return -1;
  }
  int idx[4] = {0, 1, 2, 3};
  long score[4];
  for (int n = 0; n < 4; ++n) {
    const ColonizeCol1Nation* nat = &col1->nation[n];
    score[n] = (long)(nat->gold / 100u) + (long)col1->stuff.colony_counts[n] * 2 +
               (long)col1->stuff.census_pop_proxy[n] + (long)col1->stuff.land_combat_strength[n];
  }
  for (int i = 1; i < 4; ++i) { /* insertion sort, stable */
    const int v = idx[i];
    int j = i - 1;
    while (j >= 0 && score[idx[j]] > score[v]) {
      idx[j + 1] = idx[j];
      j--;
    }
    idx[j + 1] = v;
  }
  return idx[3];
}

/* -0x6a4e field_combat_strength_by_continent: Σ 004a(u,1) over land units on
 * cid that are not fortified and not inside a colony (save_format_map row 300).
 * Byte table in DOS — capped at 255. */
static int ai_diplo_153e_exposed_combat_at(const ColonizeTurnContext* ctx, int nation, int cid) {
  ColonizeCombatStrengthCtx sctx;
  sctx.units = ctx->units;
  sctx.map = ctx->map;
  sctx.colonies = ctx->colonies;
  sctx.col1 = ctx->col1;
  int sum = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active || u->nation_id != nation || u->aboard_ship_id >= 0 ||
        units_is_sea(ctx->units, i)) {
      continue;
    }
    if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    if (ctx->colonies && colonies_id_at(ctx->colonies, u->x, u->y) >= 0) {
      continue;
    }
    if (map_continent_id_at(ctx->map, u->x, u->y) != cid) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, i, 1, NULL);
    if (sum > 255) {
      return 255;
    }
  }
  return sum;
}

/* -0x6ada skilled_unit_counts_by_continent: +1 per land unit whose type has a
 * profession slot (FUN_281f_0b78 / DS:0x30e >= 0 — colonist-class types 0..9). */
static int ai_diplo_153e_skilled_units_at(const ColonizeTurnContext* ctx, int nation, int cid) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active || u->nation_id != nation || u->aboard_ship_id >= 0 ||
        u->type_index < 0 || u->type_index > 9) {
      continue;
    }
    if (map_continent_id_at(ctx->map, u->x, u->y) == cid) {
      n++;
    }
  }
  return n > 255 ? 255 : n;
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
 * lands) is the real next step before this is a Tier 3 candidate.
 *
 * 2026-08-20 (T1.11, write-trigger resolved): traced `FUN_0000_5b62`
 * (the raw byte-setter) via Ghidra's own XREF index (not a text grep) to
 * its only two callers — `FUN_0000_5b96` (OR-set a mask into the
 * relation byte, both directions) and `FUN_0000_5c00` (AND-clear a mask,
 * both directions) — then searched the canonical export for every
 * literal `,0x10)` call into their overlay-side wrappers
 * (`switchD_2000:da9f::caseD_10` / `FUN_281f_0a10`). Exactly one call
 * site sets `0x10`: `FUN_38fd_5930` (raw ~67030-67141), a periodic
 * per-nation event — gated on turn×wealth-rank threshold, not already
 * crown-controlled (the `0x543f` field) — that, once per qualifying
 * check, RNG-picks one already-met, not-at-war rival nation the acting
 * nation is *behind* in colonial development, grants it up to a handful
 * of free Veteran Soldiers plus a treasury bump scaled by the
 * development gap, then sets both `AI_DIPLO_MET` (redundantly) and this
 * bit on that specific (self, rival) pair. Reads as a scripted "the
 * crown arms a struggling colony against a specific stronger rival"
 * event — `peace_bit_0x10` is the marker that this just happened for
 * this pair, which coheres with its role here (a flat, unconditional
 * `worthy=1` + score bump): having just been armed against nation X is a
 * legitimate independent reason to consider declaring on X. Not
 * independently confirmed against any other source (no live capture, no
 * cross-reference to a second DOS function) — structural/mechanical
 * confidence, not full semantic certainty, same caveat this project's
 * method notes always flag. **Still not wired live** — this only answers
 * "what sets the bit," not "is `153e` safe to flip" (T3.2, needs user
 * confirm regardless). Full trace: `euro_diplo_153e_full.md`'s 2026-08-20
 * T1.11 update.
 *
 * LIVE since the 2026-08-27 deepen (ai_diplo_153e_encounter, fired from
 * FUN_5bfb_3180's both move sides — game_loop for the human, ai_euro
 * post-act for AI units since 2026-09-06). The old "not safe to flip"
 * caution is resolved: bit 0x10 is AI_DIPLO_CROWN_ARMED (FUN_38fd_5930),
 * read as real data below.
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

Ai153eWorthinessScore ai_diplo_153e_worthiness_score(
  ColonizeTurnContext* ctx, int self, int target, int encounter_unit, int forced_gate
) {
  Ai153eWorthinessScore out;
  out.handled = 0;
  out.worthy = 0;
  out.dominance_bonus = 0;
  out.score = 0;
  out.at_war = 0;
  out.old_stamp = 0;
  out.own_border = 0;
  out.border_value = 0;
  out.any_border = 0;
  out.forced = 0;
  (void)encounter_unit; /* DOS param_4: only read by the demand-goods phase (not ported) */
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->map || !ctx->colonies || !ctx->units ||
      self < 0 || self >= 4 || target < 0 || target >= 4 || self == target) {
    return out;
  }
  ColonizeCol1Save* col1 = ctx->col1;

  /* raw :97406 entry gate: self > 3 or 0x543f[self] != 0 (an AI self) routes to
   * FUN_5bfb_13b0 (ai_diplo_13b0_treaty_tick) and never scores. */
  if (self != ctx->human_nation) {
    out.handled = 1;
    return out;
  }
  if (col1->head.game_options.woi) {
    return out; /* DS:0x5382 bit0 */
  }

  const ColonizeCol1Nation* nat_self = &col1->nation[self];
  const ColonizeCol1Nation* nat_target = &col1->nation[target];
  const int difficulty = col1->head.difficulty;
  const int turn = col1->head.turn;

  /* raw 412-428: gate cascade. Unmet pair forces the talk (first contact);
   * a Crown-war stamp (DS:0x53c8[target]) older than 16 turns forces it too
   * and clears the crown-armed bit 0x10 both ways. */
  int gate = forced_gate;
  if ((ai_diplo_read(col1, self, target) & AI_DIPLO_MET) == 0) {
    gate = 1;
  }
  if ((int)col1->head.nation_relation[target] + 0x10 <= turn) {
    gate = 1;
    ai_diplo_clear_both(col1, self, target, AI_DIPLO_CROWN_ARMED);
  }
  const int crown_armed = (ai_diplo_read(col1, self, target) & AI_DIPLO_CROWN_ARMED) != 0;
  if (gate == 0) {
    return out; /* raw: goto LAB_OVL16_L0040__0034de, nothing computed */
  }
  out.handled = 1;

  /* raw 432-438: nation_flags bit 0x04 only feeds phase-4 flavor text; the
   * cooldown stamp is refreshed to the current turn (old value kept). */
  out.old_stamp = (int)col1->head.nation_relation[target];
  col1->head.nation_relation[target] = (int16_t)turn;

  /* raw 439-475: continent loop over the G-table tallies. */
  int dominance_bonus = 0;   /* raw iStack_ce (stack local, asm 5bfb:16c9) */
  int combat_delta_sum = 0;  /* raw local_68 */
  int worthy = 0;             /* raw local_a8 */
  const int target_threshold = (col1->stuff.colony_counts[target] > 1) ? 1 : 0;
  for (int cid = 1; cid < 0xf; ++cid) {
    const int target_colonies = ai_diplo_153e_colonies_at(ctx, target, cid);
    const int target_land_units = ai_diplo_153e_land_units_at(ctx, target, cid);
    const int self_colonies = ai_diplo_153e_colonies_at(ctx, self, cid);
    const int self_exposed = ai_diplo_153e_exposed_combat_at(ctx, self, cid);
    const int target_exposed = ai_diplo_153e_exposed_combat_at(ctx, target, cid);
    const int self_skilled = ai_diplo_153e_skilled_units_at(ctx, self, cid);
    if (target_threshold < target_colonies && target_land_units < self_exposed) {
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
      } else if (target_colonies < 2) {
        delta = target_exposed;
      } else {
        delta = target_exposed - self_exposed;
      }
      combat_delta_sum += delta;
    }
  }

  /* raw 476-508: colony loop (DS:0x539e / 0x8542 are the colony count and
   * the selected colony record, +0x1a its owner) — every colony owned by
   * self or target gets the FUN_5bfb_0000 border probe against `target`. */
  int border_value_sum = 0;  /* raw local_b2 */
  int own_border_sum = 0;    /* raw local_8 */
  int any_border = 0;        /* raw local_62 */
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || (c->nation_id != self && c->nation_id != target)) {
      continue;
    }
    Ai153eBorderProbe s = ai_diplo_153e_border_probe(ctx, c->x, c->y, target);
    int value = s.value;
    if (s.matched_target == target) {
      combat_delta_sum += value * 2;
      if (s.colony_military == 0 || s.adjacent_military > 1) {
        if (dominance_bonus != 0) {
          dominance_bonus -= 1;
        }
        worthy = 1;
      }
      const int cid = map_continent_id_at(ctx->map, c->x, c->y);
      if (ai_diplo_153e_colonies_at(ctx, target, cid) == 0) {
        value <<= 1;
      }
      border_value_sum += value;
      any_border = 1;
    }
    if (s.matched_target == self) {
      own_border_sum += value;
      combat_delta_sum += -(s.adjacent_military * 2);
    }
  }
  out.own_border = own_border_sum;
  out.border_value = border_value_sum;
  out.any_border = any_border;

  /* raw 509-521: forced-conflict override. DS:0xa153 = top-ranked nation
   * (FUN_5bfb_00f8 rank table). */
  const int top_ranked = ai_diplo_00f8_top_ranked_nation(col1);
  int forced_conflict = top_ranked == self && turn > 0x4f &&
                        col1->stuff.colony_counts[self] > 3 &&
                        col1->stuff.colony_counts[target] > 1;
  /* raw uStack_ae: direct -0x77c4 read, bit 0x02 = WAR (T1.19 bit map). */
  const int at_war = (nat_target->euro_relation[self] & AI_DIPLO_WAR) != 0;
  const uint8_t self_totals = col1->stuff.field_combat_totals[self];
  const uint8_t target_totals_x3 = (uint8_t)(col1->stuff.field_combat_totals[target] * 3);
  if (forced_conflict || (at_war && self_totals < target_totals_x3)) {
    worthy = 1;
    dominance_bonus = 0;
    const int lo = difficulty * 200 + 100;
    const int hi = 0x26ac;
    if (combat_delta_sum < lo) combat_delta_sum = lo;
    if (combat_delta_sum > hi) combat_delta_sum = hi;
  }

  /* raw 522-527: crown-armed bit (FUN_38fd_5930 @KINGNEWWAR marker). */
  if (crown_armed) {
    worthy = 1;
  }
  if (dominance_bonus != 0) {
    worthy = 0;
  }

  /* raw 528-535: FUN_1000_89a4(self, 0x13) = Franklin owned. */
  const int franklin = founding_fathers_nation_has(col1, self, FF_BENJAMIN_FRANKLIN) ? 1 : 0;
  int old_stamp = out.old_stamp;
  if (franklin) {
    forced_conflict = 0;
    worthy = 0;
    if (old_stamp < 0) {
      old_stamp = 0;
    }
  }

  /* raw 536-544: difficulty-threshold override, only when not at war. */
  if (!at_war) {
    const int thr = (difficulty - 10) * -10;
    if (thr != turn && turn <= thr) {
      forced_conflict = 0;
      worthy = 0;
      if (old_stamp < 0) {
        old_stamp = 0;
      }
    }
  }
  out.forced = forced_conflict; /* raw iStack_c survives to phase 2's tally gate */

  /* raw 545-563: final scaling. */
  int scaled = ((difficulty + 8) * combat_delta_sum * 10) / 100;
  if (!at_war) {
    combat_delta_sum = scaled >> 2;
    if (old_stamp >= 0) {
      if (turn < 0x32) {
        scaled >>= 1;
      } else if (turn < 100) {
        scaled -= combat_delta_sum;
      }
      combat_delta_sum = scaled;
      if (col1->stuff.colony_counts[self] < 3 && col1->stuff.census_pop_proxy[self] < 8) {
        combat_delta_sum >>= 1;
      }
    }
  } else {
    combat_delta_sum = scaled << 1;
  }

  /* raw 564-566 */
  {
    int v = (int)(((difficulty + 1) * combat_delta_sum) >> 3);
    if (v < 0) v = 0;
    if (v > 400) v = 400;
    combat_delta_sum = v * 50;
  }
  if (crown_armed) {
    combat_delta_sum += (difficulty + 1) * 500;
  }

  /* raw 570-584: affordability clamp against self's own treasury
   * (*0x84fc = the record FUN_281f_0582(self) selected, +0x2a/+0x2c = gold):
   * when the score exceeds the treasury but not twice it, and the treasury
   * holds at least 300, round the score down to the treasury in 50s. */
  {
    const long gold = (long)nat_self->gold;
    if (gold < (long)combat_delta_sum) {
      if ((long)combat_delta_sum <= gold * 2 && gold >= 300) {
        combat_delta_sum = (int)((gold / 50) * 50);
      }
    }
  }

  /* raw 585-588 */
  if (franklin) {
    combat_delta_sum >>= 1;
  }

  /* raw 589-593 */
  if (combat_delta_sum == 0 || target_totals_x3 < self_totals) {
    worthy = 0;
  }

  out.worthy = worthy;
  out.dominance_bonus = dominance_bonus;
  out.score = combat_delta_sum;
  out.at_war = at_war;
  return out;
}

/* ======================================================================
 * FUN_5bfb_153e phases 2-4 — the human x AI-Euro encounter dialog
 * (raw viceroy_unpacked.c:97594-98430), ported 2026-08-27 as a popup
 * state machine: every DOS FUN_2a1f_0688 dialog becomes one
 * AI_POPUP_TAG_DIPLO_TALK popup (payload = stage), its answer resumes the
 * flow in ai_diplo_153e_talk_resume. GAME.TXT tags are the DOS strcpy/
 * strcat products (HELLO+FIRST/AHOY/MEEK/MANLY, PEACE+MEEK/MANLY, ...).
 * @WANTSTUFF goods demand ported 2026-09-06 (see the want_* fields and
 * AI_TALK_ST_WANTSTUFF; incl. the byte-verified DOS Furs stale-index bug).
 * Still not modeled: the USA (post-independence) text variants, the
 * LEADER/LEADER2/KINGS/DEEDS name-prep prefixes (folded into single-tag
 * bodies), unit "encounter direction" stamps (+0x314f).
 * ====================================================================== */
enum {
  AI_TALK_ST_THIRD = 1,
  AI_TALK_ST_PIRACY,
  AI_TALK_ST_SIEGES,
  AI_TALK_ST_TRIBUTE,
  AI_TALK_ST_WANTSTUFF,
  AI_TALK_ST_WORTHY,
  AI_TALK_ST_GIVECASH,
  AI_TALK_ST_PEACEMENU,
  AI_TALK_ST_WITHDRAW,
  AI_TALK_ST_ALLY_PICK,
  AI_TALK_ST_ALLY_PAY,
  AI_TALK_ST_DONE
};

typedef struct Ai153eTalk {
  int active;
  int self;
  int target;
  int unit_id;
  int stage;
  int worthy;      /* raw local_a8 (live) */
  int worthy_end;  /* raw local_94: worthy at the end of phase 1 */
  int manly;       /* raw iVar6 */
  int latch;       /* raw local_b0 */
  int score;       /* raw local_68 */
  int at_war;      /* raw local_ae */
  int crown_armed; /* raw local_9e */
  int dominance;   /* raw iVar16 */
  int own_border;
  int border_value;
  int any_border;
  int sieges_paid; /* raw local_be */
  int third;       /* raw iVar17: third party to gang up on (-1 none) */
  int forced;      /* raw local_c */
  int ally_pick;
  int ally_cost;
  int withdraw_cost;
  int pending_gold; /* GIVECASH / GIFTS amount */
  /* @WANTSTUFF demand-goods pick (raw :97562-97593): armed only when the
   * encounter's moving unit (DOS param_4) belongs to `target`. */
  int want_cargo;  /* raw iStack_b4: picked cargo index, -1 none */
  int want_amount; /* raw iStack_8a: demanded amount (deficit capped to source stock) */
  int want_src;    /* raw iStack_88: colony on the encounter tile (the human's) */
  int want_dst;    /* raw iStack_64: target's nearest colony (the receiver) */
  int last_talk_turn[4][4];
} Ai153eTalk;
static Ai153eTalk s_talk;

/* Weak fallback for link units built without ai_contact.c (unit_units). */
__attribute__((weak)) const char* ai_contact_tribe_name(int nation_id) {
  (void)nation_id;
  return "natives";
}

static ColonizeCol1Nation* ai_talk_nat(ColonizeTurnContext* ctx, int n) {
  return &ctx->col1->nation[n];
}
static const char* ai_talk_name(ColonizeTurnContext* ctx, int n) {
  if (n >= 4 && n <= 11) {
    return ai_contact_tribe_name(n);
  }
  return ai_diplo_rival_name(ctx->col1, n);
}
static void ai_talk_sync_gold(ColonizeTurnContext* ctx) {
  if (ctx->europe && ctx->human_nation >= 0 && ctx->human_nation < 4) {
    ctx->europe->gold = (int)ctx->col1->nation[ctx->human_nation].gold;
  }
}
static void ai_talk_gold(ColonizeTurnContext* ctx, int from, int to, int amount) {
  if (amount <= 0) {
    return;
  }
  ColonizeCol1Nation* f = ai_talk_nat(ctx, from);
  ColonizeCol1Nation* t = ai_talk_nat(ctx, to);
  const uint32_t a = (uint32_t)amount;
  f->gold = f->gold >= a ? f->gold - a : 0u;
  t->gold += a;
  ai_talk_sync_gold(ctx);
}
static int ai_talk_franklin(ColonizeTurnContext* ctx, int n) {
  return founding_fathers_nation_has(ctx->col1, n, FF_BENJAMIN_FRANKLIN) ? 1 : 0;
}
static int ai_talk_peace(ColonizeTurnContext* ctx, int a, int b) {
  return (ai_diplo_read(ctx->col1, a, b) & AI_DIPLO_PEACE) != 0;
}
static int ai_talk_met(ColonizeTurnContext* ctx, int a, int b) {
  if (b >= 4 && b <= 11) {
    return (ctx->col1->indian[b - 4].euro_diplo[a] & COL1_INDIAN_MET_BIT) != 0;
  }
  return (ai_diplo_read(ctx->col1, a, b) & AI_DIPLO_MET) != 0;
}
static int ai_talk_rng(ColonizeTurnContext* ctx, int lo, int hi) {
  return ctx->rng ? dos_rng_range(ctx->rng, lo, hi) : lo;
}

/* Send a unit to Europe (FUN_281f_0880/0920+0948 teleport to the nation's
 * Europe tile). Human ships/dock units go through the Europe screen. */
static void ai_talk_unit_to_europe(ColonizeTurnContext* ctx, int unit_id) {
  ColonizeUnit* u = units_get(ctx->units, unit_id);
  if (!u || !u->active) {
    return;
  }
  const int human = ctx->human_nation;
  if (u->nation_id == human && ctx->europe) {
    if (units_is_sea(ctx->units, unit_id)) {
      int types[COLONIZE_UNIT_CARGO_MAX];
      int hold_t[COLONIZE_UNIT_CARGO_MAX];
      int hold_a[COLONIZE_UNIT_CARGO_MAX];
      int n = 0;
      int type_index = -1;
      char name[48];
      if (units_despawn_ship_with_cargo(
            ctx->units, unit_id, &type_index, name, sizeof(name), types, &n,
            COLONIZE_UNIT_CARGO_MAX, hold_t, hold_a, COLONIZE_UNIT_CARGO_MAX
          )) {
        (void)europe_harbor_push(ctx->europe, type_index, name, types, n, hold_t, hold_a);
      }
      return;
    }
    (void)europe_dock_push_load(ctx->europe, units_display_name(ctx->units, u), u->profession);
    (void)units_despawn(ctx->units, unit_id);
    return;
  }
  u->x = 200;
  u->y = 100;
  u->orders = UNITS_ORDER_NONE;
  u->goto_x = UNITS_GOTO_NONE;
  u->goto_y = UNITS_GOTO_NONE;
  u->moves_left = 0;
}

/* raw :98001-98032 / :98328-98358: military land units of `who` adjacent to
 * a colony of `near` are sent to Europe. Returns the count moved. */
static int ai_talk_withdraw(ColonizeTurnContext* ctx, int who, int near_nation) {
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int moved = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active || u->nation_id != who || u->aboard_ship_id >= 0 || u->x >= 200 ||
        units_is_sea(ctx->units, i)) {
      continue;
    }
    const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
    if (!t || t->attack <= 1) {
      continue;
    }
    int adjacent = 0;
    for (int d = 0; d < 8 && !adjacent; ++d) {
      const int cid = colonies_id_at(ctx->colonies, u->x + dx[d], u->y + dy[d]);
      const ColonizeColony* c = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
      if (c && c->active && c->nation_id == near_nation) {
        adjacent = 1;
      }
    }
    if (adjacent) {
      ai_talk_unit_to_europe(ctx, u->id);
      moved++;
    }
  }
  return moved;
}

static void ai_talk_body(
  ColonizeTurnContext* ctx, const char* tag, const PopupMsgTokens* tok, const char* fallback,
  char* out, size_t out_size
) {
  popup_msg_fill(ctx->messages, tag, tok, fallback, out, out_size);
}

static void ai_talk_ok(
  ColonizeTurnContext* ctx, const char* tag, const PopupMsgTokens* tok, const char* fallback
) {
  char body[AI_POPUP_BODY_LEN];
  ai_talk_body(ctx, tag, tok, fallback, body, sizeof(body));
  (void)ai_popup_enqueue_ok_ctx(
    ctx->ai_popups, AI_POPUP_TAG_DIPLO_TALK, s_talk.self, s_talk.target, 0,
    NULL, body
  );
  /* DOS FUN_2a1f_0688: every 153e audience popup wears the ruler MYR{nation}
   * — except sections DOS shows via FUN_281f_0652 with their own MSS figure
   * (e.g. @DECLAREWAR), which keep it. */
  if (popup_msg_mss_index_for_section(tag) < 0) {
    ai_popup_set_last_graphic_myr(ctx->ai_popups, s_talk.target);
  }
}

/* Enqueue a CHOICE for `stage`; labels from GAME.TXT when present. */
static void ai_talk_choice(
  ColonizeTurnContext* ctx, const char* tag, const PopupMsgTokens* tok, const char* fallback,
  const char* const* fallback_labels, int count, int stage
) {
  char body[AI_POPUP_BODY_LEN];
  ai_talk_body(ctx, tag, tok, fallback, body, sizeof(body));
  char buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const ColonizeMsgSection* sec = ctx->messages ? assets_msg_find(ctx->messages, tag) : NULL;
  const int nch = sec ? popup_msg_choices(sec, buf, AI_POPUP_CHOICE_MAX) : 0;
  const char* labels[AI_POPUP_CHOICE_MAX];
  int ids[AI_POPUP_CHOICE_MAX];
  char filled[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  for (int i = 0; i < count && i < AI_POPUP_CHOICE_MAX; ++i) {
    const char* src = (nch >= count) ? buf[i] : fallback_labels[i];
    popup_msg_apply_tokens(filled[i], sizeof(filled[i]), src, tok);
    labels[i] = filled[i];
    ids[i] = i + 1;
  }
  (void)ai_popup_enqueue_choice_ctx(
    ctx->ai_popups, AI_POPUP_TAG_DIPLO_TALK, s_talk.self, s_talk.target, stage,
    NULL, body, labels, ids, count
  );
  if (popup_msg_mss_index_for_section(tag) < 0) {
    ai_popup_set_last_graphic_myr(ctx->ai_popups, s_talk.target);
  }
}

static void ai_talk_advance(ColonizeTurnContext* ctx);

static void ai_talk_finish(ColonizeTurnContext* ctx) {
  Ai153eTalk* k = &s_talk;
  ColonizeCol1Save* col1 = ctx->col1;
  /* raw LAB_5bfb_30ca/30de: amicable latch + treaty cooldown. */
  if (k->latch) {
    uint8_t* f = ai_diplo_flag_byte(col1, k->target, k->self);
    if (f) {
      *f = (uint8_t)(*f | 0x08);
    }
  }
  if (ai_talk_peace(ctx, k->self, k->target)) {
    int cool = (6 - (int)col1->head.difficulty) * 2;
    if (ai_talk_franklin(ctx, k->self)) {
      cool >>= 1;
    }
    col1->nation[k->self].treaty_timer[k->target] = (uint8_t)cool;
  }
  k->active = 0;
}

/* The "not worthy" peace negotiation (raw :98060-98170) after the demands. */
static void ai_talk_peace_offer(ColonizeTurnContext* ctx) {
  Ai153eTalk* k = &s_talk;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_talk_name(ctx, k->target);
  tok.string1 = ai_talk_name(ctx, k->self);
  tok.string2 = ai_talk_name(ctx, k->target);
  if (ai_talk_peace(ctx, k->self, k->target)) {
    k->stage = AI_TALK_ST_PEACEMENU;
    return;
  }
  static const char* const yn[2] = {"Yes", "No"};
  k->stage = AI_TALK_ST_WORTHY;
  ai_talk_choice(
    ctx, "WORTHY", &tok,
    "\"We propose a demarcation treaty dividing the land into %STRING1 and %STRING2 "
    "spheres of influence. Will you agree to such a partition?\"",
    yn, 2, AI_TALK_ST_WORTHY
  );
}

static void ai_talk_advance(ColonizeTurnContext* ctx) {
  Ai153eTalk* k = &s_talk;
  ColonizeCol1Save* col1 = ctx->col1;
  const int h = k->self;
  const int t = k->target;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_talk_name(ctx, t);
  tok.string1 = ai_talk_name(ctx, h);
  tok.string2 = ai_talk_name(ctx, t);
  tok.string3 = k->manly ? "demand" : "request"; /* @MEEKNESS */
  for (int guard = 0; guard < 16; ++guard) {
    switch (k->stage) {
      case AI_TALK_ST_THIRD: {
        k->stage = AI_TALK_ST_PIRACY;
        /* raw :97662: `(-1 < iStack_ca) && (uStack_9e == 0)` — the crown-armed
         * marker suppresses the third-party demand too. */
        if (k->third >= 0 && !k->crown_armed && ai_talk_met(ctx, h, k->third)) {
          PopupMsgTokens t3 = tok;
          t3.string0 = ai_talk_name(ctx, k->third);
          t3.string1 = k->third >= 4 ? ai_talk_name(ctx, k->third) : (k->manly ? "demand" : "request");
          static const char* const lab_a[2] = {"Never! They are our friends!", "Yes! We shall crush them together!"};
          if (k->third < 4) {
            ai_talk_choice(
              ctx, "APOSTATES", &t3,
              "\"We note that you have signed a treaty with the %STRING0. We %STRING1 that you cancel it at once.\"",
              lab_a, 2, AI_TALK_ST_THIRD
            );
          } else {
            ai_talk_choice(
              ctx, "HEATHEN", &t3,
              "\"We are subduing the heathen %STRING1 tribe. Will you join us in this holy task?\"",
              lab_a, 2, AI_TALK_ST_THIRD
            );
          }
          return;
        }
        break;
      }
      case AI_TALK_ST_PIRACY: {
        k->stage = AI_TALK_ST_SIEGES;
        const int alert = (ai_diplo_read(col1, t, h) & AI_DIPLO_TREASURE_ALERT) != 0;
        if (alert && !k->crown_armed && col1->stuff.unit_type_counts[h][16] != 0) {
          static const char* const lab[2] = {"What pirates? We have NEVER condoned piracy!", "Very well, we shall withdraw our privateers to Europe."};
          ai_talk_choice(
            ctx, "PIRACY", &tok,
            "\"%STRING0 is most displeased with the %STRING1 pirates lying in wait off our coast. We %STRING3 that you withdraw all privateers immediately.\"",
            lab, 2, AI_TALK_ST_PIRACY
          );
          return;
        }
        break;
      }
      case AI_TALK_ST_SIEGES: {
        k->stage = AI_TALK_ST_TRIBUTE;
        if (!k->crown_armed) {
          const int quarter = (int)col1->stuff.colony_pop_totals[t] >> 2;
          int ask = k->own_border >= quarter;
          if (!ask && k->own_border > 12 && ai_talk_rng(ctx, 0, 4) == 0) {
            ask = 1;
          }
          if (ask) {
            static const char* const lab[2] = {"Our forces protect valid interests and shall stay.", "Very well, we shall withdraw our forces to Europe."};
            ai_talk_choice(
              ctx, "SIEGES", &tok,
              "\"%STRING0 is disturbed by the large %STRING1 forces lurking outside our colonies. We %STRING3 that you withdraw all military units adjacent to our colonies immediately.\"",
              lab, 2, AI_TALK_ST_SIEGES
            );
            return;
          }
        }
        break;
      }
      case AI_TALK_ST_TRIBUTE: {
        k->stage = AI_TALK_ST_WANTSTUFF;
        if (k->score != 0 && k->manly && col1->nation[h].gold >= (uint32_t)k->score) {
          PopupMsgTokens tt = tok;
          tt.number0 = k->score;
          tt.has_number0 = true;
          static const char* const lab[2] = {"Not a penny for those heretic swine!", "We will gladly donate %NUMBER0$ to such a worthy cause."};
          ai_talk_choice(
            ctx, "TRIBUTE", &tt,
            "\"%STRING0 has told us to drive all %STRING1 from these shores. We might overlook this in exchange for a donation of %NUMBER0$.\"",
            lab, 2, AI_TALK_ST_TRIBUTE
          );
          return;
        }
        break;
      }
      case AI_TALK_ST_WANTSTUFF: {
        /*
         * @WANTSTUFF (raw :97896-97930, msg ids 0x191e LEADER2 + 0x1926):
         * gate = manly && score != 999 (tribute dialog not shown — raw sets
         * uStack_68 = 999 unconditionally after TRIBUTE) && a cargo was
         * picked in the encounter's demand phase (only armed when the
         * encounter's moving unit belongs to `target`). Tokens: NUMBER0 =
         * amount, STRING1 = the picked @CARGO name, STRING2 = target.
         */
        k->stage = AI_TALK_ST_WORTHY;
        if (k->manly && k->score != 999 && k->want_cargo >= 0 && ctx->colonies) {
          PopupMsgTokens tw = tok;
          tw.number0 = k->want_amount;
          tw.has_number0 = true;
          tw.string1 = ai_diplo_cargo_name(k->want_cargo);
          tw.string2 = ai_talk_name(ctx, t);
          static const char* const lab[2] = {
            "We laugh at your puny threats.",
            "We gladly share %NUMBER0 %STRING1 with our %STRING2 brothers."
          };
          ai_talk_choice(
            ctx, "WANTSTUFF", &tw,
            "\"We are displeased that you continue to befoul lands that are "
            "rightfully ours by order of %STRING0.  We demand %NUMBER0 %STRING1 "
            "as reparations.\"",
            lab, 2, AI_TALK_ST_WANTSTUFF
          );
          return;
        }
        break;
      }
      case AI_TALK_ST_WORTHY: {
        /* raw :98047-98062 provoke; then the peace negotiation for !worthy. */
        /*
         * Benjamin Franklin — "Europeans in the New World always offer peace
         * in negotiations" (docs/fandom_col1994.md). This is where that rule
         * belongs: with Franklin on either side the provoke arm is skipped
         * and the peace offer is made even to a "worthy" (threatening)
         * counterpart. It used to be an unsolicited Accept/Refuse popup in
         * the euro-balance tick, which was invented whole (bugs.md).
         */
        if (k->worthy && ai_diplo_franklin_pair(col1, h, t) && k->at_war) {
          if (ai_talk_peace(ctx, h, t)) {
            k->stage = AI_TALK_ST_PEACEMENU;
            break;
          }
          ai_talk_peace_offer(ctx);
          if (k->stage == AI_TALK_ST_WORTHY) {
            return; /* CHOICE queued */
          }
          break;
        }
        if (k->worthy) {
          if (ai_talk_peace(ctx, h, t) && k->score >= 0x65) {
            ai_talk_ok(ctx, "PROVOKE", &tok, "\"We can no longer tolerate your foul provocations. Prepare for WAR!\"");
            ai_diplo_clear_both(col1, h, t, AI_DIPLO_PEACE);
          }
          k->stage = AI_TALK_ST_PEACEMENU;
          break;
        }
        if (ai_talk_peace(ctx, h, t)) {
          k->stage = AI_TALK_ST_PEACEMENU;
          break;
        }
        ai_talk_peace_offer(ctx);
        if (k->stage == AI_TALK_ST_WORTHY) {
          return; /* CHOICE queued */
        }
        break;
      }
      case AI_TALK_ST_GIVECASH: {
        /* reached only via a "No" to WORTHY: offer cash when not at war. */
        k->stage = AI_TALK_ST_PEACEMENU;
        if (!k->at_war) {
          int offer = (k->dominance - 2) * 2;
          const int cap = (int)(col1->nation[t].gold / 100u);
          if (offer < 0) offer = 0;
          if (offer > cap) offer = cap;
          offer *= 100;
          if (offer > 0) {
            PopupMsgTokens tt = tok;
            tt.number0 = offer;
            tt.has_number0 = true;
            k->pending_gold = offer;
            static const char* const lab[2] = {"Very well, you shall be spared.", "Alas, it is God's will."};
            ai_talk_choice(
              ctx, "GIVECASH", &tt,
              "\"Please spare our settlement from destruction. We will give you %NUMBER0$ if you agree not to attack us.\"",
              lab, 2, AI_TALK_ST_GIVECASH
            );
            return;
          }
        }
        if (!ai_talk_peace(ctx, h, t)) {
          ai_talk_ok(ctx, k->manly ? "WARMANLY" : "WARMEEK", &tok, "\"Then prepare for WAR!\"");
        }
        break;
      }
      case AI_TALK_ST_PEACEMENU: {
        k->stage = AI_TALK_ST_DONE;
        if (ai_talk_peace(ctx, h, t)) {
          ai_diplo_wake_border_garrisons(ctx, h, t);
          ai_diplo_wake_border_garrisons(ctx, t, h);
          if (!k->worthy) {
            const char* tag = k->at_war ? (k->manly ? "PEACEMANLY" : "PEACEMEEK")
                                       : (k->manly ? "OLDPEACEMANLY" : "OLDPEACEMEEK");
            static const char* const lab[4] = {
              "Go in peace, %STRING1 brothers.",
              "First you must withdraw your forces from our colonies!",
              "How much do you value your worthless lives, heathen swine?",
              "We suggest an alliance."
            };
            ai_talk_choice(
              ctx, tag, &tok,
              "\"We welcome the friendship of our brothers the %STRING0.\"", lab, 4,
              AI_TALK_ST_PEACEMENU
            );
            return;
          }
        }
        break;
      }
      case AI_TALK_ST_WITHDRAW: {
        k->stage = AI_TALK_ST_DONE;
        break;
      }
      case AI_TALK_ST_ALLY_PICK: {
        k->stage = AI_TALK_ST_DONE;
        /* FUN_291f_0182 list: nations/tribes the human has met. */
        const char* labels[AI_POPUP_CHOICE_MAX];
        int picks[AI_POPUP_CHOICE_MAX];
        int n = 0;
        for (int p = 0; p < 12 && n < AI_POPUP_CHOICE_MAX; ++p) {
          if (p == h || p == t) {
            continue;
          }
          if (p >= 4 && p <= 11 &&
              (col1->tribe == NULL || (col1->indian[p - 4].euro_diplo[h] & COL1_INDIAN_MET_BIT) == 0)) {
            continue;
          }
          if (p < 4 && !ai_talk_met(ctx, h, p)) {
            continue;
          }
          labels[n] = ai_talk_name(ctx, p);
          picks[n] = p + 1;
          n++;
        }
        if (n == 0) {
          break;
        }
        (void)ai_popup_enqueue_choice_ctx(
          ctx->ai_popups, AI_POPUP_TAG_DIPLO_TALK, h, t, AI_TALK_ST_ALLY_PICK,
          NULL, "\"Against whom shall we ally?\"", labels, picks, n
        );
        ai_popup_set_last_graphic_myr(ctx->ai_popups, t);
        return;
      }
      case AI_TALK_ST_ALLY_PAY: {
        k->stage = AI_TALK_ST_DONE;
        const int p = k->ally_pick;
        if (p < 0) {
          break;
        }
        PopupMsgTokens tp = tok;
        tp.string0 = ai_talk_name(ctx, p);
        if (!ai_talk_met(ctx, t, p)) {
          ai_talk_ok(ctx, "NOCONTACT", &tp, "\"We have no contact with the %STRING0.\"");
          break;
        }
        const int t_peace_p = (p < 4) ? ai_talk_peace(ctx, t, p)
                                      : (col1->indian[p - 4].euro_diplo[t] & COL1_INDIAN_PEACE_BIT) != 0;
        if (!t_peace_p) {
          ai_talk_ok(ctx, "ALREADYSMITE", &tp, "\"We are already at war with the %STRING0.\"");
          break;
        }
        long base;
        const long gold50 = (long)(col1->nation[h].gold / 50u);
        if (p < 4) {
          base = ((long)col1->stuff.field_combat_totals[p] + (long)col1->stuff.land_combat_strength[p]) * gold50 / 50;
        } else {
          base = (long)col1->stuff.tribe_data_9184[p - 4] * gold50 * 3 / 50; /* -0x6e7c */
        }
        if (base < 10) base = 10;
        if (base > 200) base = 200;
        int cost = (int)base * 50;
        if (ai_talk_franklin(ctx, h)) {
          cost >>= 1;
        }
        k->ally_cost = cost;
        tp.number0 = cost;
        tp.has_number0 = true;
        static const char* const lab[2] = {"We shall gladly pay %NUMBER0$.", "Never mind."};
        ai_talk_choice(
          ctx, p < 4 ? "SMITEEUROPE" : "SMITEINDIANS", &tp,
          "\"We would gladly smite the %STRING0 for a consideration of %NUMBER0$.\"", lab, 2,
          AI_TALK_ST_ALLY_PAY
        );
        return;
      }
      case AI_TALK_ST_DONE:
      default:
        ai_talk_finish(ctx);
        return;
    }
  }
  ai_talk_finish(ctx);
}

/* Popup answer for stage `stage` (choice ids are 1-based). */
static void ai_talk_resume(ColonizeTurnContext* ctx, int stage, int choice) {
  Ai153eTalk* k = &s_talk;
  if (!k->active || !ctx || !ctx->col1) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  const int h = k->self;
  const int t = k->target;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_talk_name(ctx, t);
  tok.string1 = ai_talk_name(ctx, h);
  switch (stage) {
    case AI_TALK_ST_THIRD:
      if (choice == 2) {
        k->worthy = 0;
        if (ai_talk_rng(ctx, 0, 1) != 0) {
          k->score = 0;
        }
        if (k->third < 4) {
          ai_diplo_clear_both(col1, h, k->third, AI_DIPLO_PEACE);
          uint8_t* f = ai_diplo_flag_byte(col1, k->third, h);
          if (f) {
            *f = (uint8_t)(*f | AI_DIPLO_WAR);
          }
        } else {
          /* FUN_281f_0d6c(tribe, h, 100, 0): the real alarm-delta writer
           * (clamps 0..100) — joining the crusade enrages the tribe at US. */
          ai_diplo_indian_alarm_delta(col1, k->third, h, 100);
        }
      }
      break;
    case AI_TALK_ST_PIRACY:
      if (choice == 2) {
        int on_map = 0;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = units_get_const(ctx->units, i);
          if (!u || !u->active || u->nation_id != h) {
            continue;
          }
          const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
          if (!ty || !strstr(ty->name, "Privateer")) {
            continue;
          }
          if (u->x < 200) {
            on_map++;
          }
          ai_talk_unit_to_europe(ctx, u->id);
        }
        uint8_t* f = ai_diplo_flag_byte(col1, t, h);
        if (f) {
          *f = (uint8_t)(*f & 0x7f);
        }
        if (on_map != 0 && ai_talk_rng(ctx, 0, on_map) != 0) {
          k->worthy = 0;
        }
        k->score = k->score / (on_map + 1);
      }
      break;
    case AI_TALK_ST_SIEGES:
      if (choice == 2) {
        k->score -= k->own_border * 100;
        if (k->score < 0) {
          k->score = 0;
        }
        k->sieges_paid = 1;
        k->worthy = 0;
        (void)ai_talk_withdraw(ctx, h, t);
      }
      break;
    case AI_TALK_ST_TRIBUTE:
      if (choice == 2) {
        ai_talk_gold(ctx, h, t, k->score);
        k->worthy = 0;
      }
      /* raw :97894: uStack_68 = 999 unconditionally after the TRIBUTE dialog
       * (decline included) — marks "tribute asked" so @WANTSTUFF is skipped
       * and the worthy/PROVOKE arm reads score >= 0x65. */
      k->score = 999;
      break;
    case AI_TALK_ST_WANTSTUFF:
      if (choice == 2) {
        /*
         * Byte-verified DOS bug (OVL16 asm 0x2995-0x29B2): the transfer
         * indexes the colony stock rows with [bp-0xa2] — the loop counter
         * left at 4 by the euro rival-tally do-while — NOT the picked cargo
         * [bp-0xb2]. So DOS always moves FURS (cargo 4) by the demanded
         * amount, whatever the dialog named. Ported faithfully. Linux-only
         * bound: source stock floors at 0 (DOS int16 may underflow when the
         * picked cargo != Furs); receiver is uncapped like DOS.
         */
        ColonizeColony* cs = colonies_get_mut(ctx->colonies, k->want_src);
        ColonizeColony* cd = colonies_get_mut(ctx->colonies, k->want_dst);
        if (cs && cd && cs->active && cd->active) {
          cs->stock[COLONIZE_CARGO_FURS] -= k->want_amount;
          if (cs->stock[COLONIZE_CARGO_FURS] < 0) {
            cs->stock[COLONIZE_CARGO_FURS] = 0;
          }
          cd->stock[COLONIZE_CARGO_FURS] += k->want_amount;
        }
        k->worthy = 0; /* raw :97928 iStack_a8 = 0 */
      }
      k->stage = AI_TALK_ST_WORTHY;
      ai_talk_advance(ctx);
      return;
    case AI_TALK_ST_WORTHY:
      if (choice == 1) {
        ai_diplo_or_both(col1, h, t, (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_MET));
        col1->head.nation_relation[t] = (int16_t)(col1->head.turn + 0x10);
        k->stage = AI_TALK_ST_PEACEMENU;
      } else {
        k->stage = AI_TALK_ST_GIVECASH;
      }
      ai_talk_advance(ctx);
      return;
    case AI_TALK_ST_GIVECASH:
      if (choice == 1) {
        ai_diplo_or_both(col1, h, t, (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_MET));
        ai_talk_gold(ctx, t, h, k->pending_gold);
      } else if (!ai_talk_peace(ctx, h, t)) {
        ai_talk_ok(ctx, k->manly ? "WARMANLY" : "WARMEEK", &tok, "\"Then prepare for WAR!\"");
      }
      k->stage = AI_TALK_ST_PEACEMENU;
      ai_talk_advance(ctx);
      return;
    case AI_TALK_ST_PEACEMENU: {
      const int diff = (int)col1->head.difficulty;
      if (choice == 2) {
        /* withdraw demand by the human (raw :98252-98330) */
        if (!k->any_border) {
          ai_talk_ok(ctx, "NOTHINGWITHDRAW", &tok, "\"We have no forces adjacent to your colonies.\"");
        } else if (!k->manly || k->latch) {
          int cost = (diff + 2) * k->border_value * (k->at_war ? 0x32 : 0x19);
          if (k->worthy_end) {
            cost += cost >> 1;
          }
          if (k->sieges_paid) {
            cost -= k->own_border * 0x32;
          }
          if (ai_talk_franklin(ctx, h)) {
            cost >>= 1;
          }
          if (cost < 100) {
            cost = 100;
          }
          k->withdraw_cost = cost;
          if (col1->nation[h].gold < (uint32_t)cost || k->latch) {
            ai_talk_ok(ctx, "NOTWITHDRAW", &tok, "\"Our forces protect valid %STRING0 interests and shall not be moved.\"");
          } else {
            PopupMsgTokens tc = tok;
            tc.number0 = cost;
            tc.has_number0 = true;
            static const char* const lab[3] = {"We shall gladly pay %NUMBER0$.", "Withdraw or perish, heathen pigs!", "Oh. Never mind then."};
            k->stage = AI_TALK_ST_WITHDRAW;
            ai_talk_choice(
              ctx, "MAYBEWITHDRAW", &tc,
              "\"We are willing to move our forces in exchange for %NUMBER0$ to cover the cost of demobilization.\"",
              lab, 3, AI_TALK_ST_WITHDRAW
            );
            return;
          }
        } else {
          ai_talk_ok(ctx, "WITHDRAW", &tok, "\"In the interest of peace, we shall withdraw our forces.\"");
          (void)ai_talk_withdraw(ctx, t, h);
        }
      } else if (choice == 3) {
        /* tribute demand by the human (raw :98360-98395) */
        int g = k->dominance;
        if (ai_talk_franklin(ctx, h) && ai_talk_rng(ctx, 0, 2) == 0) {
          g++;
        }
        const int cap = (int)(col1->nation[t].gold / 100u);
        if (g < 0) g = 0;
        if (g > cap) g = cap;
        g *= 100;
        if (g <= 0) {
          if (!k->worthy_end) {
            ai_talk_ok(ctx, "THREATS", &tok, "\"We laugh at your feeble threats.\"");
          } else {
            ai_talk_ok(ctx, "PROVOKE", &tok, "\"We can no longer tolerate your foul provocations. Prepare for WAR!\"");
            ai_diplo_clear_both(col1, h, t, AI_DIPLO_PEACE);
          }
        } else {
          PopupMsgTokens tg = tok;
          tg.number0 = g;
          tg.has_number0 = true;
          ai_talk_ok(ctx, "GIFTS", &tg, "\"We present you with a gift of %NUMBER0$ in exchange for your continued forbearance.\"");
          ai_talk_gold(ctx, t, h, g);
        }
      } else if (choice == 4) {
        k->stage = AI_TALK_ST_ALLY_PICK;
        ai_talk_advance(ctx);
        return;
      }
      k->stage = AI_TALK_ST_DONE;
      ai_talk_advance(ctx);
      return;
    }
    case AI_TALK_ST_WITHDRAW:
      if (choice == 1) {
        ai_talk_gold(ctx, h, t, k->withdraw_cost);
        (void)ai_talk_withdraw(ctx, t, h);
      } else if (choice == 2) {
        int span = (int)col1->stuff.field_combat_totals[t] + (int)col1->stuff.field_combat_totals[h];
        if (k->at_war) {
          span *= 2;
        }
        /* raw asm OVL16 0x2EF2-0x2EFA: `cmp cx(=totals[self]), ax(=roll); jl
         * refuse` — roll <= the HUMAN's field-combat total → they back down.
         * (Was inverted here before 2026-09-06.) */
        if (ai_talk_rng(ctx, 0, span) <= (int)col1->stuff.field_combat_totals[h]) {
          ai_talk_ok(ctx, "WITHDRAW", &tok, "\"In the interest of peace, we shall withdraw our forces.\"");
          (void)ai_talk_withdraw(ctx, t, h);
        } else {
          ai_talk_ok(ctx, "WARMANLY", &tok, "\"Then prepare for WAR!\"");
          ai_diplo_clear_both(col1, h, t, AI_DIPLO_PEACE);
        }
      }
      k->stage = AI_TALK_ST_DONE;
      ai_talk_advance(ctx);
      return;
    case AI_TALK_ST_ALLY_PICK:
      k->ally_pick = choice - 1;
      k->stage = AI_TALK_ST_ALLY_PAY;
      ai_talk_advance(ctx);
      return;
    case AI_TALK_ST_ALLY_PAY:
      if (choice == 1) {
        const int p = k->ally_pick;
        if (col1->nation[h].gold < (uint32_t)k->ally_cost) {
          ai_talk_ok(ctx, "UNFORTUNATE", &tok, "\"Unfortunately you cannot afford that.\"");
        } else {
          if (p < 4) {
            ai_diplo_clear_both(col1, t, p, AI_DIPLO_PEACE);
            /* raw :98393: nation[pick].euro_relation[target] |= WAR — the
             * PICKED nation's byte toward the hired smiter, same direction
             * as the APOSTATES arm (was written (t,p) before 2026-09-06). */
            uint8_t* f = ai_diplo_flag_byte(col1, p, t);
            if (f) {
              *f = (uint8_t)(*f | AI_DIPLO_WAR);
            }
          } else {
            col1->indian[p - 4].euro_diplo[t] =
              (uint8_t)((col1->indian[p - 4].euro_diplo[t] & (uint8_t)~COL1_INDIAN_PEACE_BIT) | COL1_INDIAN_WAR_BIT);
          }
          ai_talk_ok(ctx, "MERCENARY", &tok, "\"It is done. We march against them.\"");
          ai_talk_gold(ctx, h, t, k->ally_cost);
        }
      }
      k->stage = AI_TALK_ST_DONE;
      ai_talk_advance(ctx);
      return;
    default:
      break;
  }
  ai_talk_advance(ctx);
}

int ai_diplo_153e_encounter(ColonizeTurnContext* ctx, int human, int target, int unit_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->ai_popups || !ctx->units || !ctx->map ||
      !ctx->colonies || human < 0 || human > 3 || target < 0 || target > 3 || human == target ||
      human != ctx->human_nation) {
    return 0;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  if (s_talk.active) {
    return 0;
  }
  if (s_talk.last_talk_turn[human][target] == (int)col1->head.turn + 1) {
    return 0; /* one talk per pair per turn (3180 fires per move) */
  }
  Ai153eWorthinessScore w = ai_diplo_153e_worthiness_score(ctx, human, target, unit_id, 0);
  if (!w.handled) {
    return 0;
  }
  s_talk.last_talk_turn[human][target] = (int)col1->head.turn + 1;
  memset(&s_talk.self, 0, sizeof(s_talk) - offsetof(Ai153eTalk, self));
  Ai153eTalk* k = &s_talk;
  k->active = 1;
  k->self = human;
  k->target = target;
  k->unit_id = unit_id;
  k->worthy = w.worthy;
  k->worthy_end = w.worthy;
  k->score = w.score;
  k->at_war = w.at_war;
  k->crown_armed = (ai_diplo_read(col1, human, target) & AI_DIPLO_CROWN_ARMED) != 0;
  k->dominance = w.dominance_bonus;
  k->own_border = w.own_border;
  k->border_value = w.border_value;
  k->any_border = w.any_border;
  k->third = -1;
  k->ally_pick = -1;
  k->want_cargo = -1;
  k->forced = w.forced;
  k->manly = w.worthy ? 1 : 0;

  /*
   * @WANTSTUFF demand pick (raw :97562-97593): only when the encounter's
   * moving unit (DOS param_4, from FUN_5bfb_3180) belongs to `target` — an
   * AI unit walked up to the human side. Receiver = target's nearest colony
   * (FUN_1000_8804, nearest-by-distance approximated Chebyshev); source =
   * the colony on the encounter tile (FUN_1000_89ae at unit+dir; the Linux
   * encounter has no direction, so the first adjacent colony stands in —
   * identical whenever one adjacent colony exists, which is the DOS case).
   * value = min(warehouse-capacity deficit at receiver, source stock) ×
   * euro_price[target][cargo] (-0x7b44), kept when >= (score/10)*(diff-2)
   * + score/2 and >= the running best.
   */
  {
    const ColonizeUnit* eu = units_get_const(ctx->units, unit_id);
    if (eu && eu->active && eu->nation_id == target && ctx->colonies) {
      static const int ddx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int ddy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      int dst = -1;
      int best_d = 0x7fff;
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active || c->nation_id != target) {
          continue;
        }
        int dx0 = c->x - eu->x;
        int dy0 = c->y - eu->y;
        if (dx0 < 0) dx0 = -dx0;
        if (dy0 < 0) dy0 = -dy0;
        const int d = dx0 > dy0 ? dx0 : dy0;
        if (d < best_d) {
          best_d = d;
          dst = i;
        }
      }
      int src = -1;
      for (int d = 0; d < 8 && src < 0; ++d) {
        src = colonies_id_at(ctx->colonies, eu->x + ddx[d], eu->y + ddy[d]);
      }
      const ColonizeColony* cd = dst >= 0 ? colonies_get(ctx->colonies, dst) : NULL;
      const ColonizeColony* cs = src >= 0 ? colonies_get(ctx->colonies, src) : NULL;
      if (cd && cs && cd->active && cs->active) {
        const int thr = (w.score / 10) * ((int)col1->head.difficulty - 2) + (w.score >> 1);
        int best_v = 0; /* raw iStack_c2 */
        for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
          /* FUN_1000_8f2a = warehouse capacity (FUN_15eb_0a50). */
          int amount = colonies_warehouse_capacity(ctx->colonies, cd, c) - cd->stock[c];
          if (cs->stock[c] < amount) {
            amount = cs->stock[c];
          }
          const int v = amount * (int)col1->nation[target].trade.euro_price[c];
          if (v >= thr && v >= best_v) {
            best_v = v;
            k->want_cargo = c;
            k->want_amount = amount;
            k->want_src = src;
            k->want_dst = dst;
          }
        }
      }
    }
  }

  /* raw :97594-97650: rival tally (local_ba) + the third-party candidate. */
  int ba = k->at_war ? -2 : 0;
  for (int tr = 0; tr < 8; ++tr) {
    const ColonizeCol1Indian* ind = &col1->indian[tr];
    if (ind->extinct) {
      continue; /* asm 0x202B: test [0x8d4e+3], 0x80 */
    }
    /* asm 0x2035-0x205E: alarm(tribe→target) > 0x4a OR the SELF↔tribe WAR
     * bit (FUN_1000_8c28(self, tribe+4) & 2 — the earlier `target` read here
     * was a decompile-arg misread, fixed 2026-09-06 against the raw pushes). */
    const int hostile_to_t = ind->alarm_by_player[target] > 0x4a ||
                             (ind->euro_diplo[human] & COL1_INDIAN_WAR_BIT) != 0;
    if (!hostile_to_t) {
      continue;
    }
    if (col1->stuff.land_combat_strength[target] < col1->stuff.tribe_data_9184[tr]) {
      ba++; /* -0x6e7c per-tribe Brave combat sum */
    }
    ba++;
    if (ind->alarm_by_player[human] < 0x4b && (ind->euro_diplo[human] & COL1_INDIAN_WAR_BIT) == 0) {
      k->third = tr + 4;
    }
  }
  for (int n = 0; n < 4; ++n) {
    if (n == human || n == target || n == (int)col1->head.crown_nation_id) {
      continue;
    }
    if ((ai_diplo_read(col1, target, n) & (AI_DIPLO_PEACE | AI_DIPLO_MET)) != AI_DIPLO_MET) {
      continue;
    }
    if (col1->stuff.land_combat_strength[target] < col1->stuff.land_combat_strength[n] * 4) {
      ba++;
    }
    if (col1->stuff.land_combat_strength[target] < col1->stuff.land_combat_strength[n]) {
      ba++;
    }
    if (ai_diplo_read(col1, human, n) & AI_DIPLO_PEACE) {
      k->third = n;
    }
  }
  /* raw :97648: ba -= (char)(-0x6a9a[target*3]) — the ×3-stride per-nation
   * byte table is still unresolved project-wide (Linux unknown34_pad; same
   * stub as ai_euro_10ec_war_worthy). Stubbed 0, documented, not invented. */
  if (col1->stuff.land_combat_strength[human] < col1->stuff.land_combat_strength[target]) {
    ba--;
  }
  const int peace_ht = ai_talk_peace(ctx, human, target);
  if (!k->forced && (peace_ht ? 0 : 1) < ba) {
    k->worthy = 0;
    if (ai_talk_rng(ctx, 0, 1) != 0) {
      k->score = 0;
    }
  }
  if (!k->worthy && k->dominance != 0 && peace_ht) {
    k->active = 0;
    return 0; /* raw :97655: nothing to say */
  }
  if (k->worthy && peace_ht &&
      (k->at_war || col1->stuff.land_combat_strength[target] < col1->stuff.land_combat_strength[human])) {
    k->latch = 1;
    k->worthy = 0;
    k->manly = 0;
  }

  /* Greeting (HELLO + FIRST/AHOY/MEEK/MANLY). */
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_talk_name(ctx, human);
  tok.string1 = "the New World";
  tok.string2 = ai_talk_name(ctx, target);
  tok.string3 = k->manly ? "subdue the heathen" : "spread the faith";
  const char* hello = k->manly ? "HELLOMANLY" : "HELLOMEEK";
  if ((ai_diplo_read(col1, human, target) & AI_DIPLO_MET) == 0) {
    const ColonizeUnit* u = units_get_const(ctx->units, unit_id);
    hello = (u && units_is_sea(ctx->units, unit_id)) ? "HELLOAHOY" : "HELLOFIRST";
    /* 5bfb:15cf: the same "MET (0x20) still clear" test gates woodcut 10. */
    (void)woodcut_fire(col1, WOODCUT_MEETING_FELLOW_EUROPEANS);
  }
  ai_talk_ok(ctx, hello, &tok, "\"Greetings, %STRING0, and welcome to %STRING1.\"");
  k->stage = AI_TALK_ST_THIRD;
  ai_talk_advance(ctx);
  /* FUN_5bfb_3180 :98530: a nonzero 153e return stamps MET (0x20) both
   * directions (switchD_2000:da9f::caseD_10(a, b, 0x20)) — the greeting
   * above read the pre-stamp value for FIRST/AHOY. */
  ai_diplo_or_both(col1, human, target, AI_DIPLO_MET);
  return 1;
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
    /* Expiry: thin peace/met tweak (the Linux-only break-alliance arm was
     * retired with the Euro×Euro alliance machinery, T2.4 2026-09-06).
     * Use stored flags (not ai_diplo_read virtual PEACE|MET for unmet 0) so
     * early turns do not stamp euro_relation in seed-100 goldens. */
    uint8_t* f = ai_diplo_flag_byte(ctx->col1, nation_id, other);
    if (!f) {
      continue;
    }
    const uint8_t stored = *f;
    /*
     * bugs.md 388: this used to assign `*f = PEACE|MET` — a raw overwrite of
     * ONE direction's byte. That both wiped every other flag in it (0x80
     * treasure alert, 0x10 crown-armed, 0x08) and left the pair in a state
     * DOS's own writers never produce here: the peer at peace with us while
     * we are still at war with them. The Foreign Affairs report reads the
     * viewer's own byte and the attack gate read the OR of both, so the two
     * screens disagreed. Route it through the normal peace path instead,
     * which clears WAR and ORs PEACE|MET on both bytes.
     */
    if (ctx->rng && (stored & AI_DIPLO_MET) && dos_rng_range(ctx->rng, 1, 8) == 1) {
      ai_diplo_make_peace(ctx->col1, nation_id, other);
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


/*
 * FUN_5bfb_13b0 — AI-initiated treaty sign/cancel (static port 2026-08-27,
 * T1.20). Replaces the invented 1-in-40 "alliance offer". DOS: skip if WoI;
 * cadence (a+turn+b)%3==0 unless unmet; skip if WAR either way. If neither
 * side is war-worthy (FUN_5bfb_10ec) and no PEACE → @SIGNTREATY, PEACE both
 * ways, wake border garrisons both ways, cooldown=1. Else if PEACE or unmet
 * → @DECLAREWAR (no PEACE) / @CANCELTREATY, cooldown=0, clear PEACE (war
 * proper starts when someone attacks). Notices only — no CHOICE.
 */
/* Weak fallback for link units built without ai_euro.c (unit_units): never war-worthy. */
__attribute__((weak)) int ai_euro_10ec_war_worthy(const ColonizeTurnContext* ctx, int a, int b) {
  (void)ctx;
  (void)a;
  (void)b;
  return 0;
}

static void ai_diplo_13b0_treaty_tick(ColonizeTurnContext* ctx, int a, int b) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->turn_number || a < 0 || a > 3 || b < 0 ||
      b > 3 || a == b) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  if (col1->head.game_options.woi) {
    return;
  }
  /*
   * 2026-08-27: 13b0 is only reached through FUN_5bfb_153e, whose sole DOS
   * caller is FUN_5bfb_3180 — the adjacent-unit encounter resolver. The first
   * pass ran it every balance, which signed PEACE on unmet pairs on turn 1
   * (golden TURN1->2 expects 0x00). Require an actual encounter: a unit of
   * `a` within Chebyshev 1 of a unit or colony of `b`.
   */
  {
    int encounter = 0;
    if (ctx->units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX && !encounter; ++i) {
        const ColonizeUnit* u = units_get_const(ctx->units, i);
        if (!u || !u->active || u->nation_id != a || u->aboard_ship_id >= 0 || u->x >= 200 ||
            u->y >= 200) {
          continue;
        }
        for (int j = 0; j < COLONIZE_UNITS_MAX && !encounter; ++j) {
          const ColonizeUnit* o = units_get_const(ctx->units, j);
          if (!o || !o->active || o->nation_id != b || o->aboard_ship_id >= 0) {
            continue;
          }
          if (abs(o->x - u->x) <= 1 && abs(o->y - u->y) <= 1) {
            encounter = 1;
          }
        }
        if (!encounter && ctx->colonies) {
          for (int dy = -1; dy <= 1 && !encounter; ++dy) {
            for (int dx = -1; dx <= 1 && !encounter; ++dx) {
              const int cid = colonies_id_at(ctx->colonies, u->x + dx, u->y + dy);
              if (cid >= 0) {
                const ColonizeColony* c = colonies_get(ctx->colonies, cid);
                if (c && c->active && c->nation_id == b) {
                  encounter = 1;
                }
              }
            }
          }
        }
      }
    }
    if (!encounter) {
      return;
    }
  }
  const uint8_t rel_ab = col1->nation[a].euro_relation[b];
  const uint8_t rel_ba = col1->nation[b].euro_relation[a];
  if ((a + (int)*ctx->turn_number + b) % 3 != 0 && (rel_ab & AI_DIPLO_MET)) {
    return;
  }
  if ((rel_ab & AI_DIPLO_WAR) || (rel_ba & AI_DIPLO_WAR)) {
    return;
  }
  /* Linux choice: an unmet pair still gets the bit effects (DOS does sign
   * PEACE on unmet pairs — real saves carry 0xa0), but no notice/status, so
   * a never-contacted nation can't narrate over the human's own status line. */
  const int notify = ai_diplo_involves_human(ctx, a, b) && (rel_ab & AI_DIPLO_MET) != 0;
  const char* na = ai_diplo_rival_name(col1, a);
  const char* nb = ai_diplo_rival_name(col1, b);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = na;
  tok.string1 = nb;
  char body[AI_POPUP_BODY_LEN];
  const int worthy = ai_euro_10ec_war_worthy(ctx, a, b) || ai_euro_10ec_war_worthy(ctx, b, a);
  if (!worthy) {
    if ((rel_ab & AI_DIPLO_PEACE) == 0) {
      char fb[AI_POPUP_BODY_LEN];
      snprintf(fb, sizeof(fb), "The %s and %s have signed a peace treaty.", na, nb);
      popup_msg_fill(ctx->messages, "SIGNTREATY", &tok, fb, body, sizeof(body));
      ai_diplo_or_both(col1, a, b, AI_DIPLO_PEACE);
      ai_diplo_wake_border_garrisons(ctx, a, b);
      ai_diplo_wake_border_garrisons(ctx, b, a);
      col1->nation[a].treaty_timer[b] = 1;
      col1->nation[b].treaty_timer[a] = 1;
      if (ctx->ai_popups && notify) {
        (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_DIPLO_PEACE, a, b, 0, NULL, body);
      }
      if (ctx->status && ctx->status_size > 0 && notify) {
        snprintf(ctx->status, ctx->status_size, "%s", body);
      }
    }
    return;
  }
  if ((rel_ab & AI_DIPLO_PEACE) || (rel_ab & AI_DIPLO_MET) == 0) {
    const int is_cancel = (rel_ab & AI_DIPLO_PEACE) != 0;
    /*
     * DOS 13b0 cancel branch shows tag 0x1898 = "CANCELTREATY" — but
     * GAME.TXT ships NO @CANCELTREATY section, so the dialog open fails and
     * real DOS cancels the treaty silently (bits/timers only, no popup).
     * The port's invented fallback text ("The X cancel their treaty with
     * the Y.") was player-reported as a popup DOS never shows. Keep the
     * effects; pop the notice only if some GAME.TXT actually has the
     * section.
     */
    body[0] = '\0';
    if (!is_cancel) {
      char fb[AI_POPUP_BODY_LEN];
      snprintf(fb, sizeof(fb), "The %s and %s are now at war.", na, nb);
      popup_msg_fill(ctx->messages, "DECLAREWAR", &tok, fb, body, sizeof(body));
    } else if (ctx->messages && assets_msg_find(ctx->messages, "CANCELTREATY")) {
      popup_msg_fill(ctx->messages, "CANCELTREATY", &tok, NULL, body, sizeof(body));
    }
    col1->nation[a].treaty_timer[b] = 0;
    col1->nation[b].treaty_timer[a] = 0;
    ai_diplo_clear_both(col1, a, b, AI_DIPLO_PEACE);
    if (ctx->ai_popups && notify && body[0]) {
      (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_DIPLO_BREAK, a, b, 0, NULL, body);
    }
    if (ctx->status && ctx->status_size > 0 && notify && body[0]) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
  }
}

void ai_diplo_euro_balance(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  /* Keep the 153e selector-table documentation compiled (the worthiness
   * score itself is now a real exported function — see ai_diplo.h). */
  (void)ai_diplo_153e_selector_table;
  (void)AI_DIPLO_153E_SELECTOR_COUNT;
  /*
   * FUN_5bfb_10ec / 13b0 checklist:
   *  1 skip human; at-war → upkeep + privateer prize; war-fatigue + near-parity
   *    → make_peace_ctx (timer==0 while WAR)
   *  2 military score (0000/00f8/312e stand-in)
   *  3 10ec eligibility: war if self ≫ other
   *  4 13b0 treaty sign/cancel (ai_diplo_13b0_treaty_tick; the Linux-only
   *    alliance form/break + ally aid / FA gift / longevity arms were retired
   *    T2.4 2026-09-06 — DOS has no Euro×Euro alliances)
   *  5 declare_war_ctx → thin 153e gold+tax + human status (102a/1092 chrome)
   *  + Indian matrix: feeler (skip sticky2 / any Euro war) + sticky sync/pressure
   *    + harassment; sticky2 also refuses new treaties
   *  + Franklin (fandom): NW pair with FF → always offer/conclude peace; skip
   *    10ec declare pressure (king Euro wars must not poison NW peers)
   */
  ai_diplo_indian_matrix_tick(ctx, nation_id);
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
        /*
         * bugs.md: the "<nation> offers peace." Accept/Refuse toast that used
         * to sit here was invented — body, labels and all. DOS never puts an
         * unsolicited peace dialog in front of the player; peace with a
         * European peer is settled in the FUN_5bfb_153e encounter
         * negotiation, which this file already ports in full (@WORTHY →
         * @PEACEMANLY / @PEACEMEEK). Franklin's "Europeans in the New World
         * always offer peace" is a rule about NEGOTIATIONS, so it now lives
         * there (see AI_TALK_ST_WORTHY) as well. The treaty itself still
         * concludes here and is announced with DOS's own @SIGNTREATY line,
         * which ai_diplo_make_peace_ctx already emits — that is a real
         * GAME.TXT string, not a hand-written dialog.
         */
        ai_diplo_make_peace_ctx(ctx, nation_id, peer);
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
          /* Same as the Franklin arm above (bugs.md): no invented peace
           * toast. The treaty is concluded and announced with DOS's own
           * @SIGNTREATY line, which ai_diplo_make_peace_ctx already emits. */
          ai_diplo_make_peace_ctx(ctx, nation_id, peer);
        }
      }
      continue;
    }

    /* The Linux-only standing-alliance arms (FA gift / ally-aid / longevity /
     * imbalance break CHOICE) were retired with the Euro×Euro alliance
     * machinery (T2.4 2026-09-06) — DOS has no Euro×Euro alliances; 13b0 is
     * treaty sign/cancel only. */

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
              NULL,
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

    /* FUN_5bfb_13b0: AI-initiated treaty sign/cancel (replaces the invented
     * near-parity alliance offer, 2026-08-27). Sticky deep native unrest still
     * refuses new treaties this balance (Linux layer, kept; its old
     * "precludes new alliances" status chrome retired with T2.4). */
    if (ai_diplo_indian_hostility_sticky(ctx->col1, nation_id) != AI_DIPLO_STICKY_DEEP) {
      ai_diplo_13b0_treaty_tick(ctx, nation_id, peer);
    }
  }
}

void ai_diplo_euro_timers(ColonizeTurnContext* ctx, int nation_id) {
  ai_diplo_treaty_timers(ctx, nation_id);
}

/* FUN_281f_0a60 -> FUN_15dc_00a2 quartile bucketer, same formula as
 * ai.c's ai_indian_152e_quartile (duplicated here, not shared, to avoid
 * cross-module coupling under parallel edits — 5-line pure function). */
static int ai_diplo_indian_relation_quartile(int relation) {
  if (relation < 25) {
    return 0;
  }
  if (relation < 50) {
    return 1;
  }
  if (relation < 75) {
    return 2;
  }
  return 3;
}

/*
 * DS:0x54f6 grudge/tension tier-crossing update — FUN_4cc6_00f2's second
 * half (viceroy_unpacked.c:80864-80900), never wired before this pass
 * (docs/archive/mysteries_catalog.md: "still no Linux accessor or struct field").
 *
 * Raw DOS body (only the reachable arm, see below):
 *   if (iVar5 < 100 || !(peace bit set)) {          // most calls take this
 *     if (iVar5 > 99) iVar5 = 99;
 *     if (iVar5/-5 != iVar2/-5) {                    // 5-pt tier crossing
 *       if (delta < 0) {
 *         for each tribe of this Indian nation:
 *           if ((iVar3>>1)-(iVar6>>1) < 2)            // always true, see below
 *             *(0x54f6 slot) = min(*(0x54f6 slot), iVar6>>1==0 ? 0x20 : 0x60);
 *           else
 *             *(0x54f6 slot) = 0;                     // DEAD, see below
 *       }
 *     }
 *   }
 * where iVar2/iVar5 are the OLD/NEW relation values (0..100 DOS scale,
 * same storage as ai_diplo_indian_relation_delta's own clamp) and
 * iVar3/iVar6 = ai_diplo_indian_relation_quartile(iVar2)/(iVar5) — the
 * *same* FUN_281f_0a60 quartile bucketer ai.c already ported for 152e,
 * not a separate "combat strength" stat (archive/mysteries_catalog.md's framing
 * of this branch was a misreading of what FUN_281f_0a60 was bucketing —
 * flagging for doc correction, not changing that shared file here).
 *
 * The `else` branch (hard reset to 0 on a >=2-tier gap) is DEAD CODE:
 * quartile() only returns 0..3, so `>>1` is always 0 or 1, so the gap
 * `(iVar3>>1)-(iVar6>>1)` is always in {-1,0,1} — never >=2. Verified by
 * direct read of both call sites' value ranges, not assumed. Only the
 * clamp-down arm is implemented below.
 *
 * DOS reads iVar2/iVar5 already clamped to [0,100] from the same storage
 * this function's caller writes (0x5b1c == relation_by_indian). Linux's
 * relation_by_indian is [0,255] (separate PORT DEBT, not rescoped here —
 * see port_plan.md) so old/new are locally capped to 99 for this tier
 * check only, matching DOS's own `if (iVar5>99) iVar5=99` clamp; storage
 * itself is untouched.
 */
static void ai_diplo_indian_tension_tier_update(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int old_relation,
  int new_relation,
  int delta
) {
  /* Operands are DOS alarm values (0..100), not the Linux relation view. */
  if (!col1 || !col1->indian_tension || !col1->tribe || delta >= 0) {
    return;
  }
  int old99 = old_relation > 99 ? 99 : (old_relation < 0 ? 0 : old_relation);
  int new99 = new_relation > 99 ? 99 : (new_relation < 0 ? 0 : new_relation);
  if (old99 / -5 == new99 / -5) {
    return; /* no tier boundary crossed */
  }
  const int cap = (ai_diplo_indian_relation_quartile(new99) >> 1) == 0 ? 0x20 : 0x60;
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    if ((int)col1->tribe[ti].nation_id != indian_nation) {
      continue;
    }
    int16_t* slot = &col1->indian_tension[(size_t)ti * 4 + (size_t)euro_nation];
    if (*slot > cap) {
      *slot = (int16_t)cap;
    }
  }
}

static int ai_diplo_indian_slot(const ColonizeCol1Save* col1, int indian_nation, int euro_nation) {
  if (!col1 || euro_nation < 0 || euro_nation >= 4) {
    return -1;
  }
  const int idx = indian_nation - 4;
  return (idx < 0 || idx >= 8) ? -1 : idx;
}

int ai_diplo_indian_alarm(const ColonizeCol1Save* col1, int indian_nation, int euro_nation) {
  const int idx = ai_diplo_indian_slot(col1, indian_nation, euro_nation);
  if (idx < 0) {
    return 0;
  }
  const int a = (int)col1->indian[idx].alarm_by_player[euro_nation];
  return a > 100 ? 100 : a;
}

void ai_diplo_indian_alarm_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
) {
  /*
   * FUN_4cc6_00f2: table[0x5b1c + (tribe*0x27+euro)*2] = clamp(old + delta, 0, 100);
   * on a 5-point tier crossing after a negative delta, clamp the tribes'
   * DS:0x54f6 tension slots (ai_diplo_indian_tension_tier_update). DOS also
   * clears diplo bit 4 / war bit 2 (281f_0a10) on a negative delta once
   * below 75 — not mirrored here (Linux war state lives in euro_diplo).
   */
  const int idx = ai_diplo_indian_slot(col1, indian_nation, euro_nation);
  if (idx < 0) {
    return;
  }
  const int old_v = (int)col1->indian[idx].alarm_by_player[euro_nation];
  int v = old_v + delta;
  if (v < 0) {
    v = 0;
  }
  if (v > 100) {
    v = 100;
  }
  col1->indian[idx].alarm_by_player[euro_nation] = (uint16_t)v;
  if (delta < 0) {
    /* FUN_4cc6_00f2: 281f_0a10(tribe+4, euro, 4) on any cooling; (…, 2) below 75. */
    uint8_t* d = &col1->indian[idx].euro_diplo[euro_nation];
    *d = (uint8_t)(*d & (uint8_t)~COL1_INDIAN_ATTACK_CONFIRMED_BIT);
    if (v < 0x4b) {
      *d = (uint8_t)(*d & (uint8_t)~COL1_INDIAN_WAR_BIT);
    }
  }
  ai_diplo_indian_tension_tier_update(col1, indian_nation, euro_nation, old_v, v, delta);
}

void ai_diplo_indian_relation_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
) {
  ai_diplo_indian_alarm_delta(col1, indian_nation, euro_nation, -delta);
}

uint8_t ai_diplo_indian_relation(
  const ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation
) {
  if (ai_diplo_indian_slot(col1, indian_nation, euro_nation) < 0) {
    return 0;
  }
  return (uint8_t)(100 - ai_diplo_indian_alarm(col1, indian_nation, euro_nation));
}

void ai_diplo_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup) {
  if (!ctx || !popup || !popup->has_result || popup->result_cancelled) {
    return;
  }
  /*
   * War-fatigue peace offer CHOICE (FUN_5bfb / 15b3): Accept (1) →
   * make_peace_ctx; Refuse (2) → status + follow-up OK (chrome polish;
   * human-facing status also enqueues OK). Full 153e peace UI PARKED.
   *
   * 10ec war declare CHOICE AI→human: Accept (1) → declare_war_ctx;
   * Refuse (2) → status + follow-up OK (chrome polish; mirrors peace
   * Refuse). OK popups share DIPLO_WAR + choice_id 0.
   *
   * (The DIPLO_ALLIANCE / DIPLO_BREAK alliance CHOICE arms were retired
   * with the Linux-only Euro×Euro alliance machinery, T2.4 2026-09-06;
   * DIPLO_BREAK remains as the 13b0 treaty-cancel OK tag.)
   */
  if (popup->result_tag == AI_POPUP_TAG_DIPLO_TALK) {
    if (popup->result_choice_id > 0) {
      ai_talk_resume(ctx, popup->result_payload, popup->result_choice_id);
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
  /* DIPLO_BREAK arrives only as the 13b0 treaty-cancel OK (choice_id 0) —
   * no action on dismiss. */
}
