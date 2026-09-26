/*
 * Regression guards for FIXED bugs.md rows that had no test (gap audit
 * 2026-09-26). Slice: production.
 */
#include "core/ai_contact.h"
#include "core/ai_contact_internal.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/turn.h"
#include "core/units.h"

#include "../common/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_regress_production: FAIL %s\n", msg);
  return 1;
}

/*
 * #935 — colony_prod_tick_rebel_accumulators (FUN_364b_0688 Phase C, raw
 * 57371-57376): DOS floors rebel_divisor to 1 between the `>>6` decay and
 * the `pop*2` add. A pop-0 colony whose divisor has already decayed to 0
 * must land at divisor 1, not 0, after the tick. Reverting the floor
 * leaves divisor at 0 (pop*2 == 0 adds nothing).
 */
static int case_935_rebel_divisor_floor(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.player[0].control = 0;
  col1.head.colony_count = 1;
  col1.colony = calloc(1, sizeof(ColonizeCol1Colony));
  if (!col1.colony) {
    return fail("935: alloc col1 colony");
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* c = &colonies.colonies[0];
  memset(c, 0, sizeof(*c));
  c->active = true;
  c->id = 0;
  c->x = 5;
  c->y = 5;
  c->nation_id = 0;
  c->population = 0;
  c->colonist_count = 0;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  col1.colony[0].x = 5;
  col1.colony[0].y = 5;
  col1.colony[0].nation_id = 0;
  col1.colony[0].population = 0;
  col1.colony[0].rebel_dividend = 0;
  col1.colony[0].rebel_divisor = 0; /* already decayed to 0 */

  colony_prod_tick_rebel_accumulators(&colonies, c, &col1);

  const int got = (int)col1.colony[0].rebel_divisor;
  free(col1.colony);
  if (got != 1) {
    fprintf(stderr, "935: rebel_divisor got %d want 1 (floor lost)\n", got);
    return 1;
  }
  return 0;
}

/*
 * #936 — colony_prod_sol_bonus (FUN_15eb_18ec raw 11868-11882): DOS clamps
 * neither the difficulty byte (0..4) nor the tory threshold (>=2); the port
 * used to invent both clamps. Use a difficulty value outside the old 0..4
 * clamp (20) with a human-controlled colony so thresh = 10-20 = -10
 * unclamped. tories=6 (pop 6, sol 0%) gives 6/-10 == 0 (C truncation) so
 * mod == 0. The old clamped code forces diff->4 (thresh 6, floor 2 n/a)
 * giving 6/6 == 1, mod == -1 — distinguishable.
 */
static int case_936_sol_bonus_no_clamp(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.head.difficulty = 20; /* outside any 0..4 clamp */
  col1.player[0].control = 0; /* human */

  ColonizeColony colony;
  memset(&colony, 0, sizeof(colony));
  colony.active = true;
  colony.nation_id = 0;
  colony.population = 6;
  colony.colonist_count = 6;
  colony.colony_flags = 0; /* no SoL latch bits */

  const int mod = colony_prod_sol_bonus(&col1, &colony);
  if (mod != 0) {
    fprintf(stderr, "936: sol_bonus got %d want 0 (clamp reintroduced)\n", mod);
    return 1;
  }
  return 0;
}

/*
 * #886 — turn_production.c ~876, school student whitelist (FUN_364b_0688
 * raw 57510): DOS's list is exactly {0x1a,0x19,0x1c,0x13}; the port used to
 * add a `prof < 0` tolerance arm. A colonist with profession -1 (no
 * profession assigned) must NOT be picked as a student even when a
 * teacher is ready to graduate someone.
 */
static int case_886_school_whitelist_exact(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "College");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Boston");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;

  /* Teacher: profession 21 (Veteran Soldier, school level 2 -> 6 turns),
   * seated in the College. */
  col->colonists[0].active = true;
  col->colonists[0].profession = 21;
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 5; /* one tick -> 6 == need */

  /* "Student": profession -1, not in the DOS whitelist. */
  col->colonists[1].active = true;
  col->colonists[1].profession = -1;
  col->colonists[1].building_type = -1;
  col->colonists[1].field_job = COLONIZE_JOB_FARMER;
  col->colonists[1].turns_in_job = 0;

  col->colonist_count = 2;
  col->population = 2;
  pool.colony_count = 1;

  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_colony_free_production(&pool, col, NULL, &prod, NULL);

  if (col->colonists[1].profession != -1) {
    fprintf(
      stderr, "886: profession -1 colonist got educated to %d (whitelist widened)\n",
      col->colonists[1].profession
    );
    return 1;
  }
  return 0;
}

/*
 * #841 — ai_contact_trade.c's @INDIANCITY arming tail (DOS 022e raw
 * ~96906-96928): plain signed-byte `+= 1` on muskets/horse_herds, no 0xff
 * saturation guard, so a village already sitting at 0xff wraps to 0.
 */
static int case_841_musket_no_saturation(void) {
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  ctx.col1 = &col1;
  ctx.col1_ok = true;

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  ctx.units = &units;

  ColonizeCol1Indian ind;
  memset(&ind, 0, sizeof(ind));
  ind.muskets = 0xff;

  const int nation_id = 4;
  const int e = 0;
  memset(&ai_contact_s_reparations[e], 0, sizeof(ai_contact_s_reparations[e]));
  ai_contact_s_reparations[e].active = 1;
  ai_contact_s_reparations[e].nation_id = nation_id;
  ai_contact_s_reparations[e].flavor = AI_CONTACT_REPARATIONS_CITY;
  ai_contact_s_reparations[e].tribe_index = -1;
  ai_contact_s_reparations[e].brave_id = -1; /* no brave -> ladder rank < 0 */
  ai_contact_s_reparations[e].colony_id = -1;
  ai_contact_s_reparations[e].unit_id = -1;
  ai_contact_s_reparations[e].hold = 0;
  ai_contact_s_reparations[e].cargo = COLONIZE_CARGO_MUSKETS;
  ai_contact_s_reparations[e].qty = 1;
  ai_contact_s_reparations[e].score = 0;

  ai_contact_apply_reparations(&ctx, &ind, nation_id, e, AI_CONTACT_REPARATIONS_CITY, 1);

  if (ind.muskets != 0) {
    fprintf(stderr, "841: muskets got %d want 0 (0xff+1 wrap, saturation guard reintroduced)\n",
      ind.muskets);
    return 1;
  }
  return 0;
}

/*
 * #806 — ai_contact_2820_sell_price: `ind->muskets` / `horse_herds` must be
 * read as SIGNED int8_t in the trade table prep. A count past 0x7f (e.g.
 * 0x90 = -112 signed) must make the term negative, raising `base` instead
 * of lowering it. Comparing against a low count (12, term 0) isolates the
 * sign: an unsigned read of 0x90 would subtract (144-12)=132, an unsigned
 * read stays far below; the signed read subtracts (-112-12) = -124, i.e.
 * ADDS 124 to base.
 */
static int case_806_signed_musket_horse_read(void) {
  ColonizeCol1Indian ind;
  memset(&ind, 0, sizeof(ind));
  ind.muskets = 12; /* baseline: term (12-12) == 0 */

  AiContact2820 s_lo;
  memset(&s_lo, 0, sizeof(s_lo));
  dos_rng_seed(&s_lo.rng, 12345u);
  const int price_lo =
    ai_contact_2820_sell_price(&ind, COLONIZE_CARGO_MUSKETS, 10, 100, 2, 30, &s_lo);

  ind.muskets = (uint8_t)0x90; /* signed: -112 */
  AiContact2820 s_hi;
  memset(&s_hi, 0, sizeof(s_hi));
  dos_rng_seed(&s_hi.rng, 12345u);
  const int price_hi =
    ai_contact_2820_sell_price(&ind, COLONIZE_CARGO_MUSKETS, 10, 100, 2, 30, &s_hi);

  /* Signed read: base rises sharply (muskets-12 very negative), so price_hi
   * must be strictly greater than price_lo. An unsigned read of 0x90 (144)
   * would instead push base down further (price_hi <= price_lo). */
  if (!(price_hi > price_lo)) {
    fprintf(
      stderr, "806: price_hi(%d) not > price_lo(%d); muskets read as unsigned?\n",
      price_hi, price_lo
    );
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"case_935_rebel_divisor_floor", case_935_rebel_divisor_floor},
  {"case_936_sol_bonus_no_clamp", case_936_sol_bonus_no_clamp},
  {"case_886_school_whitelist_exact", case_886_school_whitelist_exact},
  {"case_841_musket_no_saturation", case_841_musket_no_saturation},
  {"case_806_signed_musket_horse_read", case_806_signed_musket_horse_read},
};
TEST_MAIN(k_cases)
