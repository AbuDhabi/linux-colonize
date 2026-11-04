/*
 * Regression guards for FIXED port_bugs.md rows that had no test (gap audit
 * 2026-09-26). Slice: trade_raid.
 */
#include "core/ai_contact.h"
#include "core/ai_contact_internal.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include "../common/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_NAME "unit_regress_trade_raid"
#include "../common/test_fail.h"

/*
 * #830 — ai_contact_apply_raid_loot AI_RAID_STORES arm (FUN_5fef_0f14 raw
 * 99913-99926): `h = stock>>1; amt = rand(min(h,10), h)`, clamped to stock
 * and floored to 1 — a ROLL over the upper half of the pile, not the flat
 * `min(stock>>1,10)` the port used to take. With a 200-stock pile the roll
 * space is 10..100: drive it through two DOS-RNG seeds and require the
 * looted amount to differ (a flat formula would be constant across seeds)
 * and to always land inside [10,100].
 */
static int case_830_stores_amount_is_a_roll(void) {
  int amounts[6];
  /* Tiny consecutive seeds give degenerate first draws (docs/conventions.md
   * "Tiny-seed RNG"); spread them. */
  const uint32_t seeds[6] = {1u * 12345u, 2u * 12345u, 3u * 12345u,
                              4u * 12345u, 5u * 12345u, 6u * 12345u};
  for (int i = 0; i < 6; ++i) {
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ColonizeDosRng rng;
    dos_rng_seed(&rng, seeds[i]);
    ctx.rng = &rng;

    ColonizeColony c;
    memset(&c, 0, sizeof(c));
    c.active = true;
    c.stock[COLONIZE_CARGO_TOOLS] = 200;
    ai_contact_s_raid_cargo = COLONIZE_CARGO_TOOLS; /* pretend the picker chose this */
    /* ai_contact_s_raid_cargo is set by ai_contact_pick_raid_kind's STORES
     * arm normally; poke it directly here so this case isolates the amount
     * roll from the cargo-pick roll (#832 has its own case for that). */
    ai_contact_apply_raid_loot(&ctx, &c, 4, 0, AI_RAID_STORES);
    amounts[i] = 200 - c.stock[COLONIZE_CARGO_TOOLS];
    if (amounts[i] < 10 || amounts[i] > 100) {
      fprintf(
        stderr, "830: amount %d out of DOS roll range [10,100] (seed %u)\n", amounts[i],
        seeds[i]
      );
      return 1;
    }
  }
  int saw_variation = 0;
  for (int i = 1; i < 6; ++i) {
    if (amounts[i] != amounts[0]) {
      saw_variation = 1;
      break;
    }
  }
  if (!saw_variation) {
    return fail("830: amount identical across 6 seeds; flat min(stock>>1,10) reintroduced?");
  }

  /* Floor-1 / clamp: a tiny pile (stock 1) must still lose exactly 1, not 0
   * and not more than it has. */
  {
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 42u);
    ctx.rng = &rng;
    ColonizeColony c;
    memset(&c, 0, sizeof(c));
    c.active = true;
    c.stock[COLONIZE_CARGO_TOOLS] = 1;
    ai_contact_s_raid_cargo = COLONIZE_CARGO_TOOLS;
    ai_contact_apply_raid_loot(&ctx, &c, 4, 0, AI_RAID_STORES);
    if (c.stock[COLONIZE_CARGO_TOOLS] != 0) {
      return fail("830: 1-stock pile must lose exactly 1 (floor-1/clamp)");
    }
  }
  return 0;
}

/*
 * #832 — ai_contact_raid_roll_stores_cargo (FUN_5fef_0f14 raw 99819-99833),
 * reached through ai_contact_pick_raid_kind (the picker is static; this is
 * the public entry that exercises it). The DOS body is a retry loop, not a
 * value-sort: roll cargo 0..0xf, retry while stock[cargo] < 10, give up
 * after 100 tries -> kind 0. Guard the two literal oddities the value-sort
 * stand-in dropped:
 *   (a) horseless-tribe substitution: on try 1, if horse_herds==0 and
 *       stock[c] > 0x34 and a coin flip lands, c becomes HORSES even though
 *       the roll picked something else;
 *   (b) the 0xf "give up" cargo id: 0xf is COLONIZE_CARGO_COUNT-1 (16 goods,
 *       ids 0..15), a slot this colony never stocks, forcing the retry loop
 *       to keep spinning and burn the dummy rand(0,200) whenever the roll
 *       lands on MUSKETS (port_bugs.md note (c)) -- exercised implicitly by the
 *       loop terminating deterministically for a fixed seed.
 * Both are pinned by fixing seed+colony stock so the roll is forced onto the
 * horse-substitution branch, and by checking the picked cargo differs from
 * a plain value-sort's answer (which would return the highest-stock good
 * with no substitution and no retries).
 */
static int case_832_stores_cargo_retry_loop(void) {
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.head.tribe_count = 0;
  col1.player[0].control = 0;
  ctx.col1 = &col1;
  ctx.col1_ok = true;

  ColonizeCol1Indian* ind = &col1.indian[0]; /* nation 4 */
  memset(ind, 0, sizeof(*ind));
  ind->horse_herds = 0; /* horseless tribe: substitution branch eligible */

  ColonizeColony c;
  memset(&c, 0, sizeof(c));
  c.active = true;
  /* Every cargo stocked > 0x34 (52) so (a) the retry loop accepts whatever
   * it rolls on try 1 regardless of substitution -- an empty colony would
   * converge on HORSES as the only stocked good even with the substitution
   * branch deleted -- and (b) the substitution test `stock[rolled_cargo] >
   * 0x34` (DOS reads the ROLLED cargo's stock, not HORSES') can fire no
   * matter which cargo the first roll lands on. */
  for (int ci = 0; ci < COLONIZE_CARGO_COUNT; ++ci) {
    c.stock[ci] = 60;
  }
  c.stock[COLONIZE_CARGO_HORSES] = 80;
  c.building_in_production = -1;

  /* Walk seeds until we land one where try 1's coin flip (rand(0,1)==0)
   * fires and the FIRST cargo roll is not already HORSES -- that is the
   * only seed shape that can distinguish "rolled X, substituted to HORSES"
   * from "rolled HORSES directly", i.e. proves the substitution branch ran
   * rather than being a coincidence. ai_contact_pick_raid_kind resets
   * ai_contact_s_raid_cargo on every call and drives the walls/kind rolls
   * ahead of the cargo pick, so try seeds until STORES with cargo HORSES
   * comes back for a fixture where a bare rand(0,0xf) landing on HORSES on
   * try 1 is only 1/16 likely -- i.e. seeing HORSES repeatedly across many
   * seeds is the substitution branch, not luck.
   */
  int horses_hits = 0;
  int total = 0;
  for (uint32_t seed = 1; seed < 400; ++seed) {
    ColonizeDosRng rng;
    dos_rng_seed(&rng, seed * 12345u); /* spread tiny seeds */
    c.stock[COLONIZE_CARGO_HORSES] = 80;
    AiRaidKind k = ai_contact_pick_raid_kind(&ctx, &c, 4, -1, &rng, 1 /* forced: skip walls veto */);
    if (k == AI_RAID_STORES) {
      total++;
      if (ai_contact_s_raid_cargo == COLONIZE_CARGO_HORSES) {
        horses_hits++;
      }
    }
  }
  if (total < 10) {
    return fail("832: too few STORES samples to judge cargo distribution");
  }
  /* A bare rand(0,0xf) with no substitution hits HORSES about 1/16 of
   * STORES draws (~6%); the substitution branch (fires on ~1/2 of tries
   * whose first roll isn't already HORSES, given horse_herds==0 and
   * stock[HORSES]>0x34) pushes that well past half. Confirms the retry-loop
   * body -- including the substitution arm -- actually ran. */
  const double frac = (double)horses_hits / (double)total;
  if (frac < 0.30) {
    fprintf(
      stderr, "832: HORSES picked %d/%d STORES draws (%.2f); want >=0.30 (substitution branch "
      "missing, value-sort stand-in reintroduced?)\n",
      horses_hits, total, frac
    );
    return 1;
  }
  return 0;
}

/*
 * #795 — ai_contact_2e92_candidates (ai_contact_trade.c ~1394/1683, DOS-
 * literal asm 4d56:3144-319b): the filtered buy-candidate walk returns the
 * bid-SORTED SLOT (`0xf - i` as it walks cand[] top-down) alongside the
 * cargo id, and every later price/haggle term must read `bid[slot]`, not
 * `bid[cargo]`. Build an AiContact2820 whose `cand[]` order and `bid[]`
 * values disagree (sorted slot != cargo id for the same candidate), and
 * assert (a) the returned slot for a given cargo is its position in cand[]
 * counted from the top (0xf - k), not the cargo id itself, and (b) reading
 * bid[slot] vs bid[cargo] for that candidate gives different numbers --
 * i.e. the two indices are not interchangeable in this fixture, so a
 * regression back to cargo-indexed pricing is distinguishable.
 */
static int case_795_buy_candidate_uses_sorted_slot(void) {
  AiContact2820 s;
  memset(&s, 0, sizeof(s));
  /* cand[] top-down (k=15..0) is the bid-sorted cargo order; skip ids
   * 15/0/14/13 per the candidate walk. Put cargo 5 at k=15 (top -> slot 0),
   * cargo 6 at k=14 (slot 1), cargo 7 at k=12 (slot 3, k=13 skipped). */
  for (int k = 0; k < 16; ++k) {
    s.cand[k] = k; /* placeholder; overwritten below for the ones we probe */
  }
  s.cand[15] = 5;
  s.cand[14] = 6;
  s.cand[12] = 7;
  /* bid[] keyed by CARGO id (DS:0x9e78 is a per-cargo table). Make bid[cargo]
   * and bid[slot] disagree for every probed cargo. */
  for (int i = 0; i < 16; ++i) {
    s.bid[i] = (int16_t)(100 + i); /* bid[cargo id] */
  }

  int goods[3];
  int slots[3];
  const int n = ai_contact_2e92_candidates(&s, goods, slots);
  if (n != 3) {
    fprintf(stderr, "795: candidates n=%d want 3\n", n);
    return 1;
  }
  const int want_goods[3] = {5, 6, 7};
  const int want_slots[3] = {15, 14, 12}; /* the raw cand[] index (top-down) */
  for (int i = 0; i < 3; ++i) {
    if (goods[i] != want_goods[i] || slots[i] != want_slots[i]) {
      fprintf(
        stderr, "795: [%d] goods=%d slots=%d want goods=%d slots=%d\n", i, goods[i], slots[i],
        want_goods[i], want_slots[i]
      );
      return 1;
    }
    /* The DOS quirk this guards: bid[slot] must differ from bid[cargo] in
     * this fixture, so a caller that reads bid[cargo] instead of bid[slot]
     * (the pre-#795 bug) is observably wrong. */
    if (s.bid[slots[i]] == s.bid[goods[i]]) {
      return fail("795: bid[slot] == bid[cargo] in fixture; cannot distinguish indices");
    }
  }
  return 0;
}

/*
 * #805 — ai_contact_apply_buy0's haggle re-ask (2820 doc 419-425/437-441):
 * the sticky refusal, @BADHAGGLE2 popup, price walk-down and @BUY1 re-ask
 * are all gated on ai_contact_2820_met_bit (relation_by_indian & 0x40), the
 * MET bit of FUN_1000_8c28's peer byte. A tribe that has never met the
 * nation (bit clear) must fall through with NO re-ask popup enqueued and no
 * sticky_trade_good write, whatever the haggle roll says.
 */
/* Run apply_buy0's choice-2 haggle re-ask with a fresh AiContact2820 slot
 * seeded deterministically; returns 1 if a @BUY0/@BUY1 re-ask popup was
 * enqueued this call. */
static int run_buy0_haggle(
  ColonizeTurnContext* ctx, AiPopupState* pop, ColonizeCol1Indian* ind, int e, int nation_id,
  int uid, int cargo, uint32_t seed
) {
  AiContact2820* s = &ai_contact_s_2820[e];
  memset(s, 0, sizeof(*s));
  s->active = 1;
  s->nation_id = nation_id;
  s->unit_id = uid;
  s->buy_cargo = cargo;
  s->buy_slot = 3;
  s->bid[3] = 1000; /* bid/25+8 == 48: wide haggle-roll range */
  s->buy_qty = 25;
  s->round = 0;
  s->price = 100;
  dos_rng_seed(&s->rng, seed);

  ai_popup_clear(pop);
  const int payload = (uid & 0xffff) | (cargo << 16) | (100 << 20);
  ai_contact_apply_buy0(ctx, ind, nation_id, e, payload, 2 /* choice: haggle */);

  for (int qi = 0; qi < pop->queue_count; ++qi) {
    if (pop->queue[qi].tag == AI_POPUP_TAG_CONTACT_BUY0) {
      return 1;
    }
  }
  return 0;
}

static int case_805_haggle_reask_needs_met_bit(void) {
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 1;
  memset(&units.types[0], 0, sizeof(units.types[0]));
  const int uid = units_spawn(&units, 0, 5, 5);
  if (uid < 0) {
    return fail("805: could not spawn probe unit");
  }

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.head.difficulty = 0;
  col1.head.tribe_count = 0;

  const int e = 0;
  const int nation_id = 4;
  const int cargo = 2;
  ColonizeCol1Indian* ind = &col1.indian[nation_id - 4];
  memset(ind, 0, sizeof(*ind));

  AiPopupState pop;
  ai_popup_init(&pop);
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.ai_popups = &pop;

  /* Find a seed where a MET tribe re-asks (again==1 in the haggle roll);
   * that same seed drives the LCG identically for a not-met tribe (met_bit
   * is read after the roll, not before), so it isolates the gate. */
  uint32_t found_seed = 0;
  int found = 0;
  for (uint32_t seed = 1; seed < 200 && !found; ++seed) {
    col1.nation[e].relation_by_indian[nation_id - 4] = 0x40; /* met */
    const uint32_t spread_seed = seed * 12345u; /* spread tiny seeds */
    if (run_buy0_haggle(&ctx, &pop, ind, e, nation_id, uid, cargo, spread_seed)) {
      found = 1;
      found_seed = spread_seed;
    }
  }
  if (!found) {
    return fail("805: met tribe never re-asked across 200 seeds (met-bit gate too strict?)");
  }

  col1.nation[e].relation_by_indian[nation_id - 4] = 0x00; /* not met */
  if (run_buy0_haggle(&ctx, &pop, ind, e, nation_id, uid, cargo, found_seed)) {
    return fail("805: not-yet-met tribe raised a haggle re-ask popup (met-bit gate missing)");
  }
  return 0;
}

/*
 * The heresy weights read FUN_4cc6_03f8's leaning nation. DOS (raw
 * 81111-81113) gates colonies on FUN_281f_037a = max + min/2 < 7, and (raw
 * 81146-81155) counts only the +0x84 building bits, which Warehouse Expansion
 * does not have. One village of tribe 4 at (10,10), no mission; one Dutch (2)
 * colony; foreign = 2, so its score lands on pro_me. human_diff >= 0 makes the
 * Dutch human at that difficulty (raw 81120-81140 scaling).
 */
static int heresy_weights_ex(
  int cx, int cy, int pop, int plain_buildings, bool whe, int human_diff,
  const ColonizeUnitPool* units, int e, int foreign, uint8_t mission, int* out_mine
) {
  static ColonizeCol1Save col1;
  static ColonizeCol1Tribe tribe;
  static ColonizeColonyPool pool;
  ColonizeWorldMap map;
  char err[128];
  memset(&map, 0, sizeof(map));
  if (!map_alloc(&map, 20, 20, err, sizeof(err))) {
    return -1000;
  }
  for (int i = 0; i < 20 * 20; ++i) {
    map.terrain[i] = 1;
    map.layer3[i] = 0xf1;
  }
  col1_save_init(&col1);
  memset(&tribe, 0, sizeof(tribe));
  tribe.nation_id = 4;
  tribe.x = 10;
  tribe.y = 10;
  tribe.mission = mission;
  tribe.population = 4;
  col1.tribe = &tribe;
  col1.head.tribe_count = 1;
  col1.indian[0].tech = 0;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  pool.building_types[plain_buildings].row_plus1 = COLONY_BUILDING_WAREHOUSE_EXPANSION + 1;
  pool.building_type_count = plain_buildings + 1;
  ColonizeColony* c = &pool.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 2;
  c->x = (uint8_t)cx;
  c->y = (uint8_t)cy;
  c->population = (uint8_t)pop;
  c->building_in_production = -1;
  for (int b = 0; b < plain_buildings; ++b) {
    c->has_building[b] = true;
  }
  c->has_building[plain_buildings] = whe;
  pool.colony_count = 1;
  pool.next_id = 1;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  ctx.colonies = &pool;
  ctx.units = (ColonizeUnitPool*)units;
  ctx.human_nation = human_diff >= 0 ? 2 : 0;
  col1.head.difficulty = (uint8_t)(human_diff >= 0 ? human_diff : 0);
  int mine = 0;
  int pro_me = 0;
  ai_contact_heresy_weights(&ctx, e, 4, foreign, &mine, &pro_me);
  map_free(&map);
  if (out_mine) {
    *out_mine = mine;
  }
  return pro_me;
}

static int heresy_presence_ex(
  int cx, int cy, int pop, int plain_buildings, bool whe, int human_diff,
  const ColonizeUnitPool* units
) {
  return heresy_weights_ex(cx, cy, pop, plain_buildings, whe, human_diff, units, 0, 2,
                           COL1_TRIBE_MISSION_NONE, NULL);
}

static int heresy_presence(int cx, int cy, int pop, int plain_buildings, bool whe) {
  return heresy_presence_ex(cx, cy, pop, plain_buildings, whe, -1, NULL);
}

static int case_heresy_presence_dos_distance(void) {
  /* (16,12): Chebyshev 6 but DOS 6 + 2/2 = 7, outside the < 7 gate. */
  const int far = heresy_presence(16, 12, 20, 0, false);  if (far != 0) {
    fprintf(stderr, "heresy: far colony pro_me=%d\n", far);
    return fail("a colony at DOS distance 7 must not lean the village");
  }
  /* Control: the same colony at DOS 6 does lean it. */
  if (heresy_presence(16, 10, 20, 0, false) <= 0) {
    return fail("a colony at DOS distance 6 must lean the village");
  }
  /* (12,10), pop 10, 7 plain buildings: base 13 -> 3; one more counted
   * building would make base 14 -> 4. Warehouse Expansion has no bit. */
  const int without = heresy_presence(12, 10, 10, 7, false);
  const int with = heresy_presence(12, 10, 10, 7, true);
  if (without != 3 || with != 3) {
    fprintf(stderr, "heresy: whe off=%d on=%d\n", without, with);
    return fail("Warehouse Expansion must not count as a FUN_4cc6_03f8 building");
  }
  return 0;
}

/*
 * FUN_4cc6_03f8 human scaling (raw 81120-81140): a human colony's building
 * bits count {1/2, 3/4, 1/1, 3/2, 2/1} by difficulty, and the difficulty
 * itself joins the base. Colony at (11,10) (d = 1), pop 10, 16 buildings: AI
 * base 6+8+((16-8)>>2) = 16 -> 30/5 = 6; human at difficulty 0: 8 -> 14 ->
 * 26/5 = 5; at difficulty 4: 32 -> 6+8+4+6 = 24 -> 46/5 = 9.
 */
static int case_heresy_human_difficulty(void) {
  const int ai = heresy_presence_ex(11, 10, 10, 16, false, -1, NULL);
  const int d0 = heresy_presence_ex(11, 10, 10, 16, false, 0, NULL);
  const int d4 = heresy_presence_ex(11, 10, 10, 16, false, 4, NULL);
  if (ai != 6 || d0 != 5 || d4 != 9) {
    fprintf(stderr, "heresy human: ai=%d d0=%d d4=%d\n", ai, d0, d4);
    return fail("a human colony's threat must scale with difficulty (FUN_4cc6_03f8)");
  }
  return 0;
}

/*
 * thunk_FUN_1000_a594: a village leaning toward a third nation seeds its
 * mission weight with that lean. The denouncer is 0 and the rival 1; the
 * (12,10) Dutch colony scores 3, and 4 with a mission that is not Dutch
 * (FUN_4cc6_03f8 adds half). The mission is the denouncer's (byte 0,
 * population 4): mine = 4 + 4 = 8, pro_me 0.
 */
static int case_heresy_third_nation_seeds_mission(void) {
  int mine = 0;
  const int pro_me = heresy_weights_ex(12, 10, 10, 7, false, -1, NULL, 0, 1, 0, &mine);
  if (mine != 8 || pro_me != 0) {
    fprintf(stderr, "heresy third nation: mine=%d pro_me=%d (want 8 0)\n", mine, pro_me);
    return fail("a third nation's lean must seed the village's mission weight");
  }
  return 0;
}

/*
 * FUN_4cc6_03f8 ring pass (raw 81045-81057): the tile belongs to its chain
 * head's nation (FUN_281f_07e0, newest arrival) and the sum then takes every
 * armed unit in the chain, whatever its nation. Base: the (12,10) Dutch
 * colony, pop 10, 7 buildings, scores 3. Two attack-2 units on ring tile
 * (11,11); the second to arrive is the head.
 */
static ColonizeUnitPool s_threat_units;

static int heresy_with_stack(int first_nation, int second_nation) {
  memset(&s_threat_units, 0, sizeof(s_threat_units));
  units_reset(&s_threat_units);
  units_set_occupancy_map(NULL);
  s_threat_units.type_count = 1;
  memset(s_threat_units.types, 0, sizeof(s_threat_units.types[0]));
  s_threat_units.types[0].attack = 2;
  const int nations[2] = {first_nation, second_nation};
  for (int k = 0; k < 2; ++k) {
    const int id = units_spawn_allow_stack(&s_threat_units, 0, 11, 11);
    ColonizeUnit* u = units_get(&s_threat_units, id);
    if (!u) {
      return -1000;
    }
    u->nation_id = nations[k];
    units_tile_stack_arrive(&s_threat_units, id);
  }
  return heresy_presence_ex(12, 10, 10, 7, false, -1, &s_threat_units);
}

static int case_threat_ring_mixed_stack(void) {
  /* Brave below a Dutch head: Dutch tile, both units sum -> 3 + 4. */
  const int dutch_head = heresy_with_stack(4, 2);
  /* Dutch unit in the lowest slot under a Brave head: tile abandoned -> 3. */
  const int brave_head = heresy_with_stack(2, 4);
  if (dutch_head != 7 || brave_head != 3) {
    fprintf(stderr, "threat ring: dutch_head=%d brave_head=%d\n", dutch_head, brave_head);
    return fail("ring owner is the chain head; the sum takes every armed unit (#768)");
  }
  return 0;
}

/*
 * FUN_4d56_2154 phase 2 (raw 81956-81959) gates every tribe +-2 tile on
 * FUN_281f_0302 (FUN_137f_000a, 1 <= x < width-1, 1 <= y < height-1).
 * A village at (1,1) has 7 tiles of its +-2 window on row 0 / column 0.
 * DOS never reads them, so turning them from mountains (no bucket at
 * tech 0) into plains must not move any bid or ask.
 */
static int meet_econ_rim(int rim_terrain, AiContactMeetEcon2154* out) {
  const int vx = 1;
  static ColonizeCol1Save col1;
  static ColonizeCol1Tribe tribe;
  ColonizeWorldMap map;
  char err[128];
  memset(&map, 0, sizeof(map));
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    return 0;
  }
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const int inner = x >= vx && x <= vx + 2 && y >= vx && y <= vx + 2;
      const int rim = x == 0 || y == 0;
      map.terrain[y * 8 + x] = rim ? rim_terrain : inner ? 3 : 0xa0; /* 0xa0 = mountains */
      map.layer3[y * 8 + x] = 0xf1;
    }
  }
  col1_save_init(&col1);
  memset(&tribe, 0, sizeof(tribe));
  tribe.nation_id = 4;
  tribe.x = (uint8_t)vx;
  tribe.y = (uint8_t)vx;
  tribe.population = 4;
  col1.indian[0].tech = 0;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  const int ok = ai_contact_meet_economics_2154(&ctx, 4, &tribe, out);
  map_free(&map);
  return ok;
}

static int case_meet_econ_skips_map_rim(void) {
  AiContactMeetEcon2154 a;
  AiContactMeetEcon2154 b;
  if (!meet_econ_rim(0xa0, &a) || !meet_econ_rim(3, &b)) {
    return fail("2154 rim fixture did not run");
  }
  if (a.bid[0] <= 0 || memcmp(a.bid, b.bid, sizeof(a.bid)) != 0 ||
      memcmp(a.ask, b.ask, sizeof(a.ask)) != 0) {
    fprintf(stderr, "2154 rim: mountain rim bid0=%d bid3=%d, plains rim bid0=%d bid3=%d\n",
            a.bid[0], a.bid[3], b.bid[0], b.bid[3]);
    return fail("2154 phase 2 must skip the map rim (FUN_281f_0302)");
  }
  return 0;
}

/*
 * FUN_5fef_0f14 kind 2. The picker reseeds from DS:0x83a6 when that word is
 * set (raw 99773), so these cases park it at -1 and let each seed drive the
 * stream.
 */
static ColonizeColonyPool s_burn_pool;

static void burn_pool_types(ColonizeBuildingRow lo, ColonizeBuildingRow hi) {
  colonies_init(&s_burn_pool);
  colonies_set_occupancy_map(NULL);
  s_burn_pool.building_types[0].row_plus1 = (int)lo + 1;
  s_burn_pool.building_types[1].row_plus1 = (int)hi + 1;
  s_burn_pool.building_type_count = 2;
}

/* Pool colony 0 owning type 0 and (when hi_owned) type 1, levels at 2, one
 * colonist working in type 1. */
static ColonizeColony* burn_colony(ColonizeBuildingRow lo, ColonizeBuildingRow hi, bool hi_owned) {
  burn_pool_types(lo, hi);
  ColonizeColony* c = &s_burn_pool.colonies[0];
  memset(c, 0, sizeof(*c));
  c->active = true;
  c->building_in_production = -1;
  c->population = 1;
  c->colonist_count = 1;
  c->colonists[0].active = true;
  c->colonists[0].building_type = 1;
  c->warehouse_level = 2;
  c->capitol_level = 2;
  c->has_building[0] = true;
  c->has_building[1] = hi_owned;
  s_burn_pool.colony_count = 1;
  return c;
}

static void burn_ctx(ColonizeTurnContext* ctx, ColonizeCol1Save* col1, ColonizeDosRng* rng) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->rng = rng;
  ctx->colonies = &s_burn_pool;
  ctx->col1 = col1;
  ctx->col1_ok = true;
}

static AiRaidKind burn_pick(ColonizeColony* c, ColonizeCol1Save* col1, ColonizeDosRng* rng) {
  ColonizeTurnContext ctx;
  burn_ctx(&ctx, col1, rng);
  return ai_contact_pick_raid_kind(&ctx, c, 4, 0, rng, 1);
}

static ColonizeCol1Save* burn_col1(void) {
  static ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.tribe_count = 0;
  return &col1;
}

/* Warehouse Expansion has no DOS bit (FUN_364b_0114 raw 56911-56923), so the
 * port's has_building mirror must not change the pick: a colony with the
 * mirror and one without draw the same stream. */
static int case_burn_ignores_expansion_mirror(void) {
  ColonizeCol1Save* col1 = burn_col1();
  const int32_t saved_83a6 = dos_rng_get_83a6();
  dos_rng_set_83a6(-1);
  int burns = 0;
  int bad_seed = 0;
  int bad_with = 0;
  int bad_without = 0;
  for (uint32_t seed = 1; seed < 300 && !bad_seed; ++seed) {
    ColonizeDosRng ra;
    ColonizeDosRng rb;
    dos_rng_seed(&ra, seed * 12345u);
    dos_rng_seed(&rb, seed * 12345u);
    ColonizeColony* c =
      burn_colony(COLONY_BUILDING_WAREHOUSE, COLONY_BUILDING_WAREHOUSE_EXPANSION, true);
    const AiRaidKind ka = burn_pick(c, col1, &ra);
    c = burn_colony(COLONY_BUILDING_WAREHOUSE, COLONY_BUILDING_WAREHOUSE_EXPANSION, false);
    const AiRaidKind kb = burn_pick(c, col1, &rb);
    burns += ka == AI_RAID_BURN;
    if (ka != kb || dos_rng_range(&ra, 0, 0x7fff) != dos_rng_range(&rb, 0, 0x7fff)) {
      bad_seed = (int)seed;
      bad_with = (int)ka;
      bad_without = (int)kb;
    }
  }
  dos_rng_set_83a6(saved_83a6);
  if (bad_seed) {
    fprintf(stderr, "burn mirror: seed %d kind %d vs %d\n", bad_seed, bad_with, bad_without);
    return fail("Warehouse Expansion mirror changed the burn pick (DOS has no 0x10 bit)");
  }
  return burns > 0 ? 0 : fail("burn mirror: no BURN sampled");
}

/* Seeds until a BURN, then applies it. Returns 0 when no BURN was sampled. */
static int burn_apply(ColonizeBuildingRow lo, ColonizeBuildingRow hi, ColonizeColony** out) {
  ColonizeCol1Save* col1 = burn_col1();
  const int32_t saved_83a6 = dos_rng_get_83a6();
  dos_rng_set_83a6(-1);
  int found = 0;
  for (uint32_t seed = 1; seed < 300 && !found; ++seed) {
    ColonizeColony* c = burn_colony(lo, hi, true);
    ColonizeDosRng rng;
    dos_rng_seed(&rng, seed * 12345u);
    if (burn_pick(c, col1, &rng) != AI_RAID_BURN) {
      continue;
    }
    ColonizeTurnContext ctx;
    burn_ctx(&ctx, col1, &rng);
    ai_contact_apply_raid_loot(&ctx, c, 4, 0, AI_RAID_BURN);
    *out = c;
    found = 1;
  }
  dos_rng_set_83a6(saved_83a6);
  return found;
}

/* raw 99951-99973: a Warehouse / Capitol burn at level 2 leaves level 1; the
 * port's Expansion mirror (no DOS bit) must go with it. */
static int case_burn_clears_expansion_mirror(void) {
  ColonizeColony* c = NULL;
  if (!burn_apply(COLONY_BUILDING_WAREHOUSE, COLONY_BUILDING_WAREHOUSE_EXPANSION, &c)) {
    return fail("burn mirror: no Warehouse BURN sampled");
  }
  const int wh_level = c->warehouse_level;
  const int wh = c->has_building[0];
  const int whe = c->has_building[1];
  if (!burn_apply(COLONY_BUILDING_CAPITOL, COLONY_BUILDING_CAPITOL_EXPANSION, &c)) {
    return fail("burn mirror: no Capitol BURN sampled");
  }
  if (wh_level != 1 || !wh || whe || c->capitol_level != 1 || !c->has_building[0] ||
      c->has_building[1]) {
    fprintf(stderr, "burn mirror: wh level %d wh %d whe %d, cap level %d cap %d cpe %d\n",
            wh_level, wh, whe, c->capitol_level, c->has_building[0], c->has_building[1]);
    return fail("a 2->1 burn must drop the Expansion mirror and keep the base building");
  }
  return 0;
}

/* raw 99857-99864 walks up to the Cathedral; raw 99975-99983 evicts the crew
 * only for a chain root, so the Cathedral's preacher keeps working, now in
 * the Church. */
static int case_upper_tier_burn_keeps_crew(void) {
  ColonizeColony* c = NULL;
  if (!burn_apply(COLONY_BUILDING_CHURCH, COLONY_BUILDING_CATHEDRAL, &c)) {
    return fail("burn crew: no BURN sampled");
  }
  if (c->has_building[1] || !c->has_building[0] || c->colonists[0].building_type != 0) {
    fprintf(stderr, "burn crew: church %d cathedral %d crew %d\n", c->has_building[0],
            c->has_building[1], c->colonists[0].building_type);
    return fail("Cathedral burn must keep its crew working in the Church");
  }
  return 0;
}

/* raw 99975-99985 never touches the project: a burn of the building the
 * colony is producing leaves building_in_production as it was. */
static int case_burn_keeps_project(void) {
  ColonizeColony* c = burn_colony(COLONY_BUILDING_CHURCH, COLONY_BUILDING_CATHEDRAL, true);
  c->building_in_production = 1;
  const bool burned = colonies_destroy_building(&s_burn_pool, c->id, 1);
  if (!burned || c->has_building[1] || c->building_in_production != 1) {
    fprintf(stderr, "burn project: burned %d cathedral %d project %d (want 1 0 1)\n", burned,
            c->has_building[1], c->building_in_production);
    return fail("a burn must leave the colony's project alone");
  }
  return 0;
}

/* FUN_5fef_0f14 kind 3 (raw 99868-99874) takes the first hull in the
 * tile's chain (07e0 head, 02e4 next), not the lowest unit slot. */
static ColonizeUnitPool s_ship_units;

static void ship_setup(void) {
  memset(&s_ship_units, 0, sizeof(s_ship_units));
  units_reset(&s_ship_units);
  units_set_occupancy_map(NULL);
  s_ship_units.type_count = 2;
  memset(s_ship_units.types, 0, 2 * sizeof(s_ship_units.types[0]));
  /* @UNIT Privateer / Frigate combat, guns, hull (NAMES.TXT). */
  s_ship_units.types[0].kind_plus1 = UNITS_KIND_PRIVATEER + 1;
  s_ship_units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  s_ship_units.types[0].defense = 8;
  s_ship_units.types[0].guns = 4;
  s_ship_units.types[0].hull = 4;
  s_ship_units.types[1].kind_plus1 = UNITS_KIND_FRIGATE + 1;
  s_ship_units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  s_ship_units.types[1].defense = 16;
  s_ship_units.types[1].guns = 6;
  s_ship_units.types[1].hull = 6;
}

static int case_raid_ship_is_chain_head(void) {
  ship_setup();
  const int older = units_spawn_allow_stack(&s_ship_units, 1, 5, 5);
  const int newer = units_spawn_allow_stack(&s_ship_units, 1, 5, 5);
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &s_ship_units;
  ColonizeColony c;
  memset(&c, 0, sizeof(c));
  c.x = 5;
  c.y = 5;
  if (older < 0 || newer < 0 || units_tile_head_id_at(&s_ship_units, 5, 5) != newer) {
    return fail("raid ship: fixture chain head is not the newer ship");
  }
  const int got = ai_contact_raid_port_ship(&ctx, &c);
  if (got != newer) {
    fprintf(stderr, "raid ship: older %d newer %d picked %d\n", older, newer, got);
    return fail("raid ship must be the first hull in the tile chain, not the lowest slot");
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"case_raid_ship_is_chain_head", case_raid_ship_is_chain_head},
    {"case_burn_ignores_expansion_mirror", case_burn_ignores_expansion_mirror},
    {"case_burn_clears_expansion_mirror", case_burn_clears_expansion_mirror},
    {"case_upper_tier_burn_keeps_crew", case_upper_tier_burn_keeps_crew},
    {"case_burn_keeps_project", case_burn_keeps_project},
    {"case_meet_econ_skips_map_rim", case_meet_econ_skips_map_rim},
    {"case_threat_ring_mixed_stack", case_threat_ring_mixed_stack},
    {"case_heresy_presence_dos_distance", case_heresy_presence_dos_distance},
    {"case_heresy_human_difficulty", case_heresy_human_difficulty},
    {"case_heresy_third_nation_seeds_mission", case_heresy_third_nation_seeds_mission},
    {"case_830_stores_amount_is_a_roll", case_830_stores_amount_is_a_roll},
    {"case_832_stores_cargo_retry_loop", case_832_stores_cargo_retry_loop},
    {"case_795_buy_candidate_uses_sorted_slot", case_795_buy_candidate_uses_sorted_slot},
    {"case_805_haggle_reask_needs_met_bit", case_805_haggle_reask_needs_met_bit},
};
TEST_MAIN(k_cases)
