/*
 * FUN_5952_035e indoor-workplace want-weight scorer (raw 94784-94860, asm
 * 5952:1ef7-5952:2193). These cases drive ai_euro_5952_want_weight /
 * ai_euro_5952_job_score directly through ai_euro_internal.h, so every
 * expected number below is recomputed by hand from the asm, not from the
 * port's own output.
 */
#include "core/ai_euro_internal.h"
#include "core/colony.h"
#include "core/colony_production.h"

#include <string.h>

#define TEST_NAME "unit_ai_euro_5952_indoor"
#include "../common/test_fail.h"
#include "../common/test_runner.h"

/* A want block with every knob at its neutral value. */
static void want_init(AiEuro5952Want* w) {
  memset(w, 0, sizeof *w);
  w->owner_nation = 1;
  w->human_nation = 0;
  w->year = 0x650;  /* 1616: past 1540 and 1600, short of 1700 */
  w->turn = 60;
  w->population = 8; /* neither pop halving applies */
  w->capitol_level = 0;
  w->press_chain_count = 0;
  w->tories = 0;
  /* wealth ranks equal => neither the halve nor the double fires, and the
   * Tools/Muskets `rank[human] <= rank[owner]` doubling DOES fire. */
  w->wealth_rank[0] = 2;
  w->wealth_rank[1] = 2;
  w->owner_wealth_rank = 2; /* local_1b0 = the owner */
}

/* ---- price arm (out < 0x10), asm 5952:1e72 --------------------------- */

static int case_price_plain_cargo(void) {
  AiEuro5952Want w;
  want_init(&w);
  /* nation 1, Rum (9): index 0x19; the -0x7b4c read is index 0x11 = nation 1
   * Cigars. */
  w.sell_price[0x19] = 20;
  w.sell_price[0x11] = 7;
  const int v = ai_euro_5952_want_weight(&w, COLONIZE_PROF_DISTILLER, COLONIZE_CARGO_RUM);
  if (v != 13) {
    return fail("price arm: 20 - 7 != 13");
  }
  /* score tail: (qty*8 + 5) * weight */
  if (ai_euro_5952_job_score(&w, COLONIZE_PROF_DISTILLER, COLONIZE_CARGO_RUM, 6) != 53 * 13) {
    return fail("score tail (qty*8+5)*w");
  }
  return 0;
}

static int case_price_tools_plus4_and_double(void) {
  AiEuro5952Want w;
  want_init(&w);
  w.sell_price[0x10 + COLONIZE_CARGO_TOOLS] = 5;
  /* out == 0xe: no -0x7b4c subtraction, +4, then x2 (turn 60 >= 0x32 and
   * rank[human] <= rank[owner]). */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_BLACKSMITH, COLONIZE_CARGO_TOOLS) != 18) {
    return fail("tools: (5+4)*2 != 18");
  }
  w.turn = 0x31; /* asm CMP [0x538e],0x32 / JL — 49 is below the gate */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_BLACKSMITH, COLONIZE_CARGO_TOOLS) != 9) {
    return fail("tools below turn 0x32: no doubling");
  }
  w.turn = 60;
  w.wealth_rank[0] = 3; /* human poorer than owner => rank[human] > rank[owner] */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_BLACKSMITH, COLONIZE_CARGO_TOOLS) != 9) {
    return fail("tools with richer owner: no doubling");
  }
  return 0;
}

static int case_price_minus8_row_first_eight(void) {
  AiEuro5952Want w;
  want_init(&w);
  w.owner_nation = 0;
  /* nation 0, Sugar (1): index 1, so the -0x7b4c read falls in the eight
   * bytes ahead of DS:0x84bc, which the port reads as 0. */
  w.sell_price[1] = 11;
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_DISTILLER, COLONIZE_CARGO_SUGAR) != 11) {
    return fail("index < 8: no subtraction");
  }
  return 0;
}

/* ---- Statesman / bells arm, asm 5952:1fb2 ---------------------------- */

static int case_bells_base_and_jefferson(void) {
  AiEuro5952Want w;
  want_init(&w);
  w.press_chain_count = 2; /* Printing Press + Newspaper */
  w.capitol_level = 1;
  w.tories = 4;
  /* 2*4 + 4 + 7 + 1*4 = 23; tories < 10; no Jefferson; year in (1540,1700]
   * => one doubling at year > 0x640 => 46; pop 8 => no halving; ranks equal;
   * clamp(46 - gross[bells]=0, 1, 100) = 46. */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 46) {
    return fail("bells base");
  }
  w.jefferson = 1; /* asm 5952:1fe4 — a <<1, NOT the colony screen's x1.5 */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 92) {
    return fail("bells Jefferson <<1");
  }
  return 0;
}

static int case_bells_tory_and_year_gates(void) {
  AiEuro5952Want w;
  want_init(&w);
  w.tories = 10; /* >= 10 => *2 */
  /* 0 + 10 + 7 = 17, *2 = 34, year > 0x640 => 68, clamp => 68 */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 68) {
    return fail("bells tories >= 10 doubling");
  }
  w.year = 0x603; /* 1539 — below the 1540 gate zeroes it, then clamp -> 1 */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 1) {
    return fail("bells year < 1540 => 0 => clamp 1");
  }
  w.year = 0x6a5; /* 1701 — both the > 0x640 and > 0x6a4 doublings */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 100) {
    return fail("bells year > 1700 clamps at 100");
  }
  return 0;
}

static int case_bells_pop_rank_indep_and_clamp(void) {
  AiEuro5952Want w;
  want_init(&w);
  w.tories = 10;
  w.year = 0x610; /* 1552: above 1540, below 1600 — no year doubling */
  /* base 17, tories*2 => 34 */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 34) {
    return fail("bells no-year-bonus base");
  }
  w.population = 3; /* both `pop <= 3` and `pop < 6` halve */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 8) {
    return fail("bells pop 3 halved twice");
  }
  w.population = 8;
  w.wealth_rank[0] = 1; /* human richer than owner => halve */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 17) {
    return fail("bells richer human halves");
  }
  w.wealth_rank[0] = 3; /* owner richer => double */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 68) {
    return fail("bells richer owner doubles");
  }
  w.wealth_rank[0] = 2;
  w.nation_flag_bit4 = 1; /* *(byte*)DS:0x84fc & 4 */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 17) {
    return fail("bells nation flag bit 4 halves");
  }
  w.nation_flag_bit4 = 0;
  w.gross[AI_EURO_5952_BELLS] = 30; /* clamp(34 - 30, 1, 100) */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 4) {
    return fail("bells subtracts gross before the clamp");
  }
  w.gross[AI_EURO_5952_BELLS] = 300;
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 1) {
    return fail("bells clamp low bound 1");
  }
  w.gross[AI_EURO_5952_BELLS] = 0;
  w.independence = 1; /* DS:0x5382 & 1 */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS) != 1) {
    return fail("bells zeroed after independence");
  }
  return 0;
}

/* ---- Carpenter / hammers arm, asm 5952:20a1 -------------------------- */

static int case_hammers_arm(void) {
  AiEuro5952Want w;
  want_init(&w);
  /* v = -(gross/3 - 5) */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_CARPENTER, AI_EURO_5952_HAMMERS) != 5) {
    return fail("hammers at gross 0");
  }
  w.gross[AI_EURO_5952_HAMMERS] = 9;
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_CARPENTER, AI_EURO_5952_HAMMERS) != 2) {
    return fail("hammers gross 9 => 5 - 3");
  }
  w.wants_construction = 1; /* colony +0x1d & 0x80 halves */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_CARPENTER, AI_EURO_5952_HAMMERS) != 1) {
    return fail("hammers wants-construction halves");
  }
  w.wants_construction = 0;
  w.gross[AI_EURO_5952_HAMMERS] = 60;
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_CARPENTER, AI_EURO_5952_HAMMERS) != 1) {
    return fail("hammers floor of 1");
  }
  return 0;
}

/* ---- Preacher / crosses arm, asm 5952:20e5 --------------------------- */

static int case_crosses_arm(void) {
  AiEuro5952Want w;
  want_init(&w);
  w.turn = 100;
  /* v starts at 3 (asm 5952:1fa3): 3 - ((0>>1) + 100/100 - 6) = 3 + 5 = 8 */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_PREACHER, AI_EURO_5952_CROSSES) != 8) {
    return fail("crosses base at turn 100");
  }
  w.gross[AI_EURO_5952_CROSSES] = 10;
  /* 3 - (5 + 1 - 6) = 3 */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_PREACHER, AI_EURO_5952_CROSSES) != 3) {
    return fail("crosses subtracts gross>>1");
  }
  w.gross[AI_EURO_5952_CROSSES] = 40;
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_PREACHER, AI_EURO_5952_CROSSES) != 1) {
    return fail("crosses floor of 1");
  }
  return 0;
}

/* ---- generic civic default and the tie rule -------------------------- */

static int case_default_weight_three(void) {
  AiEuro5952Want w;
  want_init(&w);
  /* An out slot past 0x0f on a job with no special arm keeps the literal 3
   * the asm loads at 5952:1fa3. */
  if (ai_euro_5952_want_weight(&w, COLONIZE_PROF_WEAVER, AI_EURO_5952_BELLS) != 3) {
    return fail("default civic weight 3");
  }
  /* Input clamp shape: DOS clamps the producible quantity, so a clamped qty
   * lowers the score linearly through the (qty*8 + 5) tail. */
  const int full = ai_euro_5952_job_score(&w, COLONIZE_PROF_WEAVER, AI_EURO_5952_BELLS, 9);
  const int clamped = ai_euro_5952_job_score(&w, COLONIZE_PROF_WEAVER, AI_EURO_5952_BELLS, 2);
  if (full != 77 * 3 || clamped != 21 * 3 || clamped >= full) {
    return fail("clamped qty scores below the unclamped one");
  }
  /* A zero-quantity candidate still scores 5*w, which is why DOS's tie rule
   * `score > best` (asm 5952:1ee7 JLE) matters: the first job to reach a
   * given score keeps it. */
  if (ai_euro_5952_job_score(&w, COLONIZE_PROF_WEAVER, AI_EURO_5952_BELLS, 0) != 15) {
    return fail("zero qty scores 5*w");
  }
  return 0;
}

/* ---- fallback, asm 5952:2139-5952:2174 ------------------------------ */

static int case_fallback_job(void) {
  if (ai_euro_5952_fallback_job(0, 1, 0) != COLONIZE_PROF_CARPENTER) {
    return fail("fallback without a Church is Carpenter");
  }
  if (ai_euro_5952_fallback_job(1, 0, 0) != COLONIZE_PROF_CARPENTER) {
    return fail("fallback without a lumber surplus is Carpenter");
  }
  if (ai_euro_5952_fallback_job(1, 1, 3) != COLONIZE_PROF_CARPENTER) {
    return fail("fallback with three preachers is Carpenter");
  }
  if (ai_euro_5952_fallback_job(1, 1, 2) != COLONIZE_PROF_PREACHER) {
    return fail("Church + lumber surplus + room => Preacher");
  }
  return 0;
}

/* OVL15 0x1cb2-0x1e65: an unplaced Master Carpenter with lumber on hand is
 * seated as Carpenter before the scored indoor pass (DOSBox 1552). */
static int case_expert_pass_seats_master_carpenter(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Carpenter's Shop");
  pool.building_types[0].row_plus1 = COLONY_BUILDING_CARPENTERS_SHOP + 1;
  pool.building_type_count = 1;
  ColonizeColony* c = &pool.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 1;
  c->population = 1;
  c->colonist_count = 1;
  c->building_in_production = -1;
  pool.colony_count = 1;
  pool.next_id = 1;
  const int shop = colonies_building_row(&pool, COLONY_BUILDING_CARPENTERS_SHOP);
  if (shop < 0) {
    return fail("catalog has no Carpenter's Shop row");
  }
  c->has_building[shop] = true;
  c->stock[COLONIZE_CARGO_LUMBER] = 20;
  c->colonists[0].active = true;
  c->colonists[0].profession = COLONIZE_PROF_CARPENTER;
  c->colonists[0].building_type = -1;
  c->colonists[0].field_job = -1;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.colonies = &pool;
  bool placed[1] = {false};
  ai_euro_5952_indoor_expert_pass(&ctx, c, placed, 1);
  if (!placed[0] || c->colonists[0].building_type < 0 ||
      colonies_building_occupation(&pool, c->colonists[0].building_type) !=
        COLONIZE_PROF_CARPENTER) {
    return fail("Master Carpenter with lumber must be seated as Carpenter");
  }
  c->colonists[0].building_type = -1;
  c->stock[COLONIZE_CARGO_LUMBER] = 0; /* no lumber stock or production */
  placed[0] = false;
  ai_euro_5952_indoor_expert_pass(&ctx, c, placed, 1);
  if (placed[0]) {
    return fail("no lumber at all leaves the Carpenter to the scored pass");
  }
  return 0;
}

/* local_1b0 can be clobbered by the improvement scan (raw 94482); the election
 * then reads byte[local_1b0 + 0x917c] past the rank table (asm 5952:2047).
 * Fresh 1566 two-turn: -1 reads DS:0x917b = 0, so Isabella's statesman weight
 * doubles and she seats two Statesmen. */
static int case_rank_byte_unchecked_index(void) {
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof col1);
  col1.stuff.land_combat_totals[1] = 40;
  col1.stuff.tribe_data_9184[2] = 120;
  const unsigned char rank[4] = {2, 0, 3, 1};
  if (ai_euro_5952_rank_byte(&col1, rank, 2) != 3) {
    return fail("rank byte: nation 2 reads the rank table");
  }
  if (ai_euro_5952_rank_byte(&col1, rank, -1) != 0) {
    return fail("rank byte: -1 reads DS:0x917b = 0");
  }
  if (ai_euro_5952_rank_byte(&col1, rank, 5) != 40) {
    return fail("rank byte: tribe 5 reads DS:0x9181");
  }
  if (ai_euro_5952_rank_byte(&col1, rank, 10) != 120) {
    return fail("rank byte: tribe 10 reads DS:0x9186");
  }
  AiEuro5952Want w;
  want_init(&w);
  w.wealth_rank[0] = 2;
  w.owner_wealth_rank = 0; /* rank[human] > byte => statesman doubles */
  const int doubled = ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS);
  w.owner_wealth_rank = 2;
  const int plain = ai_euro_5952_want_weight(&w, COLONIZE_PROF_STATESMAN, AI_EURO_5952_BELLS);
  if (doubled != 2 * plain) {
    return fail("rank byte: a clobbered local_1b0 must drive the statesman doubling");
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"expert_pass_seats_master_carpenter", case_expert_pass_seats_master_carpenter},
  {"fallback_job", case_fallback_job},
  {"price_plain_cargo", case_price_plain_cargo},
  {"price_tools_plus4_and_double", case_price_tools_plus4_and_double},
  {"price_minus8_row_first_eight", case_price_minus8_row_first_eight},
  {"bells_base_and_jefferson", case_bells_base_and_jefferson},
  {"bells_tory_and_year_gates", case_bells_tory_and_year_gates},
  {"bells_pop_rank_indep_and_clamp", case_bells_pop_rank_indep_and_clamp},
  {"hammers_arm", case_hammers_arm},
  {"crosses_arm", case_crosses_arm},
  {"default_weight_three", case_default_weight_three},
  {"rank_byte_unchecked_index", case_rank_byte_unchecked_index},
};

TEST_MAIN(k_cases)
