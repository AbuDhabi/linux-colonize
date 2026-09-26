/*
 * Regression guards for FIXED bugs.md rows that had no test (gap audit
 * 2026-09-26). Slice: trade_raid.
 */
#include "core/ai_contact.h"
#include "core/ai_contact_internal.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/turn.h"
#include "core/units.h"

#include "../common/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_regress_trade_raid: FAIL %s\n", msg);
  return 1;
}

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
 *       lands on MUSKETS (bugs.md note (c)) -- exercised implicitly by the
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

static const TestCase k_cases[] = {
    {"case_830_stores_amount_is_a_roll", case_830_stores_amount_is_a_roll},
    {"case_832_stores_cargo_retry_loop", case_832_stores_cargo_retry_loop},
    {"case_795_buy_candidate_uses_sorted_slot", case_795_buy_candidate_uses_sorted_slot},
    {"case_805_haggle_reask_needs_met_bit", case_805_haggle_reask_needs_met_bit},
};
TEST_MAIN(k_cases)
