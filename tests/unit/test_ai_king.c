/* Smoke: King/REF SoL, tax→REF, boycott audience + Fugger sync, tax SoL≥30 gate
 * (+ SoL-low hike assert), SoL chrome, declare+160a/1528/congress, MoW cargo×6
 * (units_ship_capacity / cargo_ids board + multi-unload ≤moves/capacity)
 * Regular+Dragoon mix + second MoW@diff≥2, 10f0 (dual + third@diff≥2 + ≤2@diff<2
 * Regular+Dragoon mix + nation pick), REF land hunt/capture+owner-change+status
 * +fortify one/two (Regular else Dragoon/Cont.Cav cap-2), REF stack extras hunt,
 * fortify extras hunt, after-capture next colony hunt, idle Regular/Dragoon/
 * Cont.Cav fortify on crown/captured capital, Artillery
 * after capture / idle on crown colony FORTIFY (Euro pattern; already-FORTIFIED
 * stay), Artillery siege
 * bias (+ competing Regular pool / unfortified Regular prefer; adjacent
 * unfortified must not override fortified), Dragoon/Cont. Cav
 * open-land bias (+ Cont. Army stays nearest negative), MoW+cargo AI_SAIL→coast
 * + unload-at-colony (Regular else Dragoon seize; multi ≤moves; fortify after
 * multi-unload; full unload + moves → next human coast; after next-coast sail
 * prefer unload if already adjacent), idle empty MoW coastal patrol, 0982 MoW
 * on water adjacent, 2244 merc hire (Soldier type) or cannot-afford once
 * (+ refuse→later gold still blocked via unknown46[3]), 1eca colony-SoL bias +
 * Cont. Army/Cont. Cav capital-rally (+ hold on capital + capital fortify cap-2
 * + capital MD slack) +
 * SoL=50 mid-band edge + Cont. Army abbrev skip, REF capital MD hunt
 * bias (+ Artillery siege capital when fortified MD slack), congress
 * unknown46[5] on declare, WoI unknown46[0] only when SoL≥50,
 * Refuse→dump CHOICE→@TEAPARTY OK (thin 3dc8 stock dump), 2244 Decline
 * follow-up OK, second MoW only @diff≥2. 160a letter cinematic Done
 * (declaration.c). PARK: dump-goods CHOICE prompt invent English
 * (picker + Europe bid>0 Done). */
#include "core/ai_king.h"
#include "core/ai_diplo.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_king: FAIL %s\n", msg);
  return 1;
}

static int count_active(const ColonizeUnitPool* units) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (units->units[i].active) {
      n++;
    }
  }
  return n;
}

static int count_nation(const ColonizeUnitPool* units, int nation_id) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (units->units[i].active && units->units[i].nation_id == nation_id) {
      n++;
    }
  }
  return n;
}

/* Crown sea vs land (MoW cargo unload stand-in). */
static int count_nation_sea(const ColonizeUnitPool* units, int nation_id) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      n++;
    }
  }
  return n;
}

static int count_nation_land(const ColonizeUnitPool* units, int nation_id) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (!units_is_sea(units, u->id)) {
      n++;
    }
  }
  return n;
}


/* FUN_38fd_5930 @KINGNEWWAR: Crown cancels peace with a random peer, grants gold + vets. */
static int test_king_new_war_event(void) {
  ColonizeCol1Save c;
  col1_save_init(&c);
  c.head.difficulty = 0;
  for (int i = 0; i < 4; ++i) {
    c.player[i].control = (i == 0) ? 0 : 1;
    memset(&c.nation[i], 0, sizeof(c.nation[i]));
  }
  memset(c.head.founding_father, -1, sizeof(c.head.founding_father));
  snprintf(c.player[2].country_name, sizeof(c.player[2].country_name), "Spain");
  /* Human 0 at peace with 2 only (1 is the Crown/REF slot for human 0); 3 unmet. */
  ai_diplo_or_both(&c, 0, 2, (uint8_t)(AI_DIPLO_MET | AI_DIPLO_PEACE));
  c.nation[0].gold = 50;
  uint32_t turn = 500; /* (0+2)*500 > 799 */
  AiPopupState pops;
  ai_popup_clear(&pops);
  ColonizeDosRng rng;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.col1 = &c;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.turn_number = &turn;
  ctx.human_nation = 0;
  ctx.ai_popups = &pops;
  int fired = 0;
  dos_rng_seed(&rng, 100u); /* one continuous stream (LCG warm-up bias on tiny seeds) */
  for (int i = 0; i < 4000 && !fired; ++i) {
    fired = ai_king_new_war_event(&ctx);
  }
  if (!fired) {
    return fail("KINGNEWWAR should fire for some seed (roll <= difficulty)");
  }
  if (c.nation[0].gold != 150) {
    fprintf(stderr, "unit_ai_king: newwar gold %u\n", (unsigned)c.nation[0].gold);
    return fail("KINGNEWWAR should grant (difficulty+1)*100 gold when combat totals are equal");
  }
  if ((c.nation[0].euro_relation[2] & AI_DIPLO_PEACE) || (c.nation[2].euro_relation[0] & AI_DIPLO_PEACE)) {
    return fail("KINGNEWWAR should clear PEACE both ways");
  }
  if (!(ai_diplo_read(&c, 0, 2) & AI_DIPLO_CROWN_ARMED) || !(ai_diplo_read(&c, 0, 2) & AI_DIPLO_MET)) {
    return fail("KINGNEWWAR should set CROWN_ARMED (0x10) and keep MET");
  }
  if (pops.queue_count != 1 || pops.queue[0].tag != AI_POPUP_TAG_KING_TAX ||
      pops.queue[0].payload != 100) {
    return fail("KINGNEWWAR should enqueue one OK popup carrying the gold grant");
  }
  /* Franklin suppresses it entirely. */
  ai_diplo_or_both(&c, 0, 2, AI_DIPLO_PEACE);
  c.nation[0].founding_fathers[FF_BENJAMIN_FRANKLIN / 8] |= (uint8_t)(1u << (FF_BENJAMIN_FRANKLIN % 8));
  c.nation[0].gold = 50;
  for (int i = 0; i < 2000; ++i) {
    if (ai_king_new_war_event(&ctx)) {
      return fail("Franklin must suppress KINGNEWWAR");
    }
  }
  /* A met-but-unpeaced peer blocks it. */
  c.nation[0].founding_fathers[FF_BENJAMIN_FRANKLIN / 8] = 0;
  c.nation[0].euro_relation[3] = AI_DIPLO_MET; /* raw: met, no peace (or_both would default PEACE) */
  for (int i = 0; i < 2000; ++i) {
    if (ai_king_new_war_event(&ctx)) {
      return fail("a met-but-unpeaced peer must block KINGNEWWAR");
    }
  }
  fprintf(stderr, "unit_ai_king: KINGNEWWAR ok\n");
  return 0;
}

/*
 * bugs.md: a crown LAND unit that cannot fight must not march or attack.
 * A Wagon Train changes hands when the REF takes a port
 * (ai_king_try_capture_at flips every civilian on the tile to the crown), and
 * the King's column had no combat-role gate — so a "Tory Wagon Train" walked
 * out and opened combat on the human's units. DOS gates every combat entry on
 * the @UNIT attack byte (DS:0x5236); ai_euro_try_attack already did.
 */
static int test_king_noncombat_never_attacks(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 0;
  ai_king_latch_clear(&col1);
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    memset(&col1.nation[i], 0, sizeof(col1.nation[i]));
  }
  col1.head.game_options.woi = 1;
  col1.head.colony_count = 1;
  col1.colony = calloc(1, sizeof(ColonizeCol1Colony));
  if (!col1.colony) {
    return fail("noncombat: alloc colony");
  }
  col1.colony[0].nation_id = 0;
  col1.colony[0].x = 5;
  col1.colony[0].y = 5;
  col1.colony[0].population = 4;
  col1.colony[0].rebel_dividend = 60;
  col1.colony[0].rebel_divisor = 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("noncombat: alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].cargo = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Continental Army");
  units.types[1].movement = 1;
  units.types[1].attack = 4;
  units.types[1].defense = 4;

  const int crown = ai_king_crown_nation(0);
  const int wagon = units_spawn_allow_stack(&units, 0, 7, 7);
  const int rebel = units_spawn_allow_stack(&units, 1, 8, 7);
  ColonizeUnit* wu = units_get(&units, wagon);
  ColonizeUnit* ru = units_get(&units, rebel);
  if (!wu || !ru) {
    return fail("noncombat: spawn");
  }
  units_set_nation(wu, crown);
  wu->orders = UNITS_ORDER_NONE;
  wu->goto_x = -1;
  wu->goto_y = -1;
  units_set_nation(ru, 0);
  ru->orders = UNITS_ORDER_FORTIFY;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 5;
  c->y = 5;
  c->population = 4;
  c->colonist_count = 4;
  snprintf(c->name, sizeof(c->name), "Jamestown");
  colonies.colony_count = 1;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7u);
  uint16_t year = 1776;
  uint16_t autumn = 0;
  uint32_t turn = 1;
  char status[128];
  status[0] = '\0';
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = 0;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.rng = &rng;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.turn_number = &turn;
  ctx.status = status;
  ctx.status_size = sizeof(status);

  for (int t = 0; t < 4; ++t) {
    ai_king_nation_turn(&ctx);
    ColonizeUnit* w = units_get(&units, wagon);
    ColonizeUnit* r = units_get(&units, rebel);
    if (!r || !r->active || r->nation_id != 0) {
      return fail("crown Wagon Train must not be able to attack a human unit");
    }
    if (!w || !w->active) {
      return fail("crown Wagon Train should survive its own idle turn");
    }
    if (w->x != 7 || w->y != 7) {
      fprintf(stderr, "unit_ai_king: Tory wagon moved to (%d,%d)\n", w->x, w->y);
      return fail("crown Wagon Train must not join the REF hunt");
    }
    w->moves_left = units_max_mp(&units, wagon);
  }

  colonies_init(&colonies);
  free(col1.colony);
  col1.colony = NULL;
  map_free(&map);
  fprintf(stderr, "unit_ai_king: crown non-combat gate ok\n");
  return 0;
}

int main(void) {
  if (test_king_new_war_event() != 0) {
    return 1;
  }
  if (test_king_noncombat_never_attacks() != 0) {
    return 1;
  }
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 0;
  ai_king_latch_clear(&col1);
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    memset(&col1.nation[i], 0, sizeof(col1.nation[i]));
  }

  /* SoL from rebel_dividend/divisor. */
  col1.head.colony_count = 1;
  col1.colony = calloc(1, sizeof(ColonizeCol1Colony));
  if (!col1.colony) {
    return fail("alloc colony");
  }
  col1.colony[0].nation_id = 0;
  col1.colony[0].x = 5;
  col1.colony[0].y = 5;
  col1.colony[0].population = 4;
  col1.colony[0].rebel_dividend = 60;
  col1.colony[0].rebel_divisor = 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* land */
    map.layer3[i] = 1; /* region 1 = open ocean nibble — water here is NOT a lake */
  }
  map.terrain[5 * 16 + 4] = 25; /* ocean west of colony for MoW */

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 9;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regular");
  units.types[0].movement = 1;
  units.types[0].attack = 3;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Man-O-War");
  units.types[1].movement = 4;
  units.types[1].domain = 1;
  units.types[1].cargo = 6;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Dragoon");
  units.types[2].movement = 2;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Artillery");
  units.types[3].movement = 1;
  /* NAMES.TXT @UNIT "Artillery, 10, 1, 7, 5, ..." — the fixture used to leave
   * both combat bytes at 0, which reads as a non-combat unit to every DOS
   * attack/role gate. */
  units.types[3].attack = 7;
  units.types[3].defense = 5;
  snprintf(units.types[4].name, sizeof(units.types[4].name), "Soldier");
  units.types[4].movement = 1;
  units.types[4].attack = 2;
  units.types[4].defense = 2;
  snprintf(units.types[5].name, sizeof(units.types[5].name), "Continental Army");
  units.types[5].movement = 1;
  units.types[5].attack = 4;
  units.types[5].defense = 4;
  snprintf(units.types[6].name, sizeof(units.types[6].name), "Continental Cavalry");
  units.types[6].movement = 4;
  units.types[6].attack = 5;
  units.types[6].defense = 5;
  snprintf(units.types[7].name, sizeof(units.types[7].name), "Veteran Soldier");
  units.types[7].movement = 1;
  units.types[7].attack = 3;
  units.types[7].defense = 3;
  /* NAMES.TXT @UNIT "Cavalry, 127, 4, 6, 6, ..." — the crown's own mounted arm.
   * The fixture used to stop at Veteran Soldier, which is how the REF Cavalry
   * could fall through every King name check unnoticed (bugs.md "the REF seems
   * to skip a turn"). */
  snprintf(units.types[8].name, sizeof(units.types[8].name), "Cavalry");
  units.types[8].movement = 4;
  units.types[8].attack = 6;
  units.types[8].defense = 6;
  const int ty_regular = 0;
  const int ty_mow = 1;
  const int ty_soldier = 4;
  const int ty_dragoon = 2;
  const int ty_artillery = 3;
  const int ty_cont_army = 5;
  const int ty_cont_cav = 6;
  const int ty_vet_soldier = 7;
  const int ty_cavalry = 8;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 5;
  c->y = 5;
  c->population = 4;
  c->colonist_count = 4;
  snprintf(c->name, sizeof(c->name), "Jamestown");
  colonies.colony_count = 1;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.tax_percent = 0;

  uint16_t year = 1536;
  uint16_t autumn = 0;
  uint32_t turn = 1;
  char status[128];
  status[0] = '\0';
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = 0;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.turn_number = &turn;
  ctx.status = status;
  ctx.status_size = sizeof(status);

  const int sol = ai_king_sol_percent(&ctx, 0);
  if (sol != 60) {
    fprintf(stderr, "unit_ai_king: unexpected SoL %d (want 60)\n", sol);
    return fail("SoL from rebel fields");
  }

  /*
   * King-audience tax event (FUN_38fd_5be8/3dc8) — 2026-08-20 rewrite
   * against the real formula (ai_king_audience_roll / apply_delta),
   * replacing the retired deterministic/year-gated design this block
   * used to test (spring-tax-year-always-hikes, persistent SoL-gated
   * refuse state, Fugger-clears-refuse — none of that is real DOS, see
   * ai_king_tax_event's own header comment). Real shape: a turn-
   * interval-gated RNG favor-score roll, applied unconditionally; only
   * a real positive applied delta can lead to a tea-party revert.
   * See port_plan.md T1.12 for the full redesign writeup.
   *
   * All seeds below are seed=1 (dos_rng_seed) — deltas/scores computed
   * by hand from the real formula (score = RNG(1,1000) +
   * (rebel_sentiment_report*2 - tax_rate)*5 + gold/100 + SoL% + turn/30,
   * ladder: <100 cut / <650 +1 / >949 +3..+8 / else +2), same "pick a
   * real seed, run, assert actual output" method already used elsewhere
   * in this file (the merc-hire block). `turn` is set explicitly per
   * scenario and reset to 1 (its file-wide default) at the very end of
   * this section, since nothing outside the audience formula reads it
   * (confirmed 2026-08-20 scoping pass) — every other scenario in this
   * file still runs with the audience gate closed, unaffected.
   */
  /* Audience block: keep SoL below declare gate and WoI clear — try_declare
   * runs after every ai_king_nation_turn peacetime slice. */
  col1.colony[0].rebel_dividend = 40;
  col1.colony[0].rebel_divisor = 100;
  col1.head.game_options.woi = 0;
  ai_king_latch_set(&col1, 0, 0);
  ai_king_latch_set(&col1, 5, 0);
  {
    /* Off-interval turn: no audience at all (interval=22 @ diff0,
     * year<=1600 baked into ai_king_audience_roll; 43 % 22 != 0). */
    turn = 43;
    col1.nation[0].tax_rate = 20;
    europe.tax_percent = 20;
    ColonizeDosRng off_rng;
    dos_rng_seed(&off_rng, 1u);
    ctx.rng = &off_rng;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 20) {
      return fail("off-interval turn must not roll an audience");
    }
  }
  {
    /* tax_rate>85 gate: no event even on an interval turn. */
    turn = 44;
    col1.nation[0].tax_rate = 90;
    europe.tax_percent = 90;
    ColonizeDosRng gate_rng;
    dos_rng_seed(&gate_rng, 1u);
    ctx.rng = &gate_rng;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 90) {
      return fail("tax_rate>85 should skip the audience roll entirely");
    }
    col1.head.game_options.woi = 0;
    ai_king_latch_set(&col1, 0, 0);
    ai_king_latch_set(&col1, 5, 0);
  }
  {
    /* Cut branch (score<100): rebel_sentiment=0, tax=50, SoL=0, turn=44,
     * seed=1 -> score -247, cut roll(2,5)=4 (capped at tax_rate) -> -4. */
    turn = 44;
    col1.head.rebel_sentiment_report = 0;
    col1.nation[0].tax_rate = 50;
    europe.tax_percent = 50;
    col1.nation[0].gold = 0;
    col1.colony[0].rebel_dividend = 0;
    col1.colony[0].rebel_divisor = 100;
    col1.head.expeditionary_force[0] = 0; /* isolate REF growth this call */
    ColonizeDosRng cut_rng;
    dos_rng_seed(&cut_rng, 1u);
    ctx.rng = &cut_rng;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 46) {
      fprintf(stderr, "unit_ai_king: cut branch tax_rate=%u (want 46)\n",
              col1.nation[0].tax_rate);
      return fail("audience cut branch should lower tax_rate by 4 (seed 1)");
    }
    if (!strstr(status, "lowers taxes") || !strstr(status, "46")) {
      fprintf(stderr, "unit_ai_king: cut branch status: '%s'\n", status);
      return fail("audience cut branch should mention the lowered rate");
    }
    if (col1.head.expeditionary_force[0] == 0) {
      return fail("an audience event (any delta sign) should still grow REF");
    }
  }
  {
    /* +1 branch (100<=score<650, streak<30): rebel=30, tax=40, SoL=20,
     * turn=44, seed=1 -> score 123 -> delta +1, streak increments. */
    turn = 44;
    col1.head.rebel_sentiment_report = 30;
    col1.nation[0].tax_rate = 40;
    europe.tax_percent = 40;
    col1.nation[0].gold = 0;
    col1.colony[0].rebel_dividend = 20;
    col1.colony[0].rebel_divisor = 100;
    col1.head.king_audience_streak = 0;
    ColonizeDosRng p1_rng;
    dos_rng_seed(&p1_rng, 1u);
    ctx.rng = &p1_rng;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 41) {
      fprintf(stderr, "unit_ai_king: +1 branch tax_rate=%u (want 41)\n",
              col1.nation[0].tax_rate);
      return fail("audience +1 branch should hike tax_rate by exactly 1 (seed 1)");
    }
    if (col1.head.king_audience_streak != 1) {
      return fail("audience +1 branch should increment king_audience_streak");
    }
  }
  {
    /* +2 branch (650<=score<=949): rebel=80, tax=20, SoL=20, turn=44,
     * seed=1 -> score 723 -> delta +2. */
    turn = 44;
    col1.head.rebel_sentiment_report = 80;
    col1.nation[0].tax_rate = 20;
    europe.tax_percent = 20;
    col1.nation[0].gold = 0;
    col1.colony[0].rebel_dividend = 20;
    col1.colony[0].rebel_divisor = 100;
    ColonizeDosRng p2_rng;
    dos_rng_seed(&p2_rng, 1u);
    ctx.rng = &p2_rng;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 22) {
      fprintf(stderr, "unit_ai_king: +2 branch tax_rate=%u (want 22)\n",
              col1.nation[0].tax_rate);
      return fail("audience +2 branch should hike tax_rate by exactly 2 (seed 1)");
    }
  }
  {
    /*
     * Big-raise branch (score>949, <1100 sub-band): rebel=100, tax=0,
     * SoL=0, turn=44, seed=1 -> score 1003 -> delta +4. Also checks the
     * unconditional-apply shape (no accept/refuse gate before the hike
     * itself), REF growth, and europe.tax_percent sync.
     */
    turn = 44;
    col1.head.rebel_sentiment_report = 100;
    col1.nation[0].tax_rate = 0;
    europe.tax_percent = 0;
    col1.nation[0].gold = 0;
    col1.colony[0].rebel_dividend = 0;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].liberty_bells_total = 0;
    col1.nation[0].boycott_bitmap = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    const uint16_t pool_before = col1.head.expeditionary_force[0];
    ColonizeDosRng big_rng;
    dos_rng_seed(&big_rng, 1u);
    ctx.rng = &big_rng;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 4) {
      fprintf(stderr, "unit_ai_king: big-raise branch tax_rate=%u (want 4)\n",
              col1.nation[0].tax_rate);
      return fail("audience big-raise branch should hike tax_rate to exactly 4 (seed 1)");
    }
    if (europe.tax_percent != col1.nation[0].tax_rate) {
      return fail("big-raise branch should sync europe.tax_percent");
    }
    if (col1.head.expeditionary_force[0] <= pool_before) {
      return fail("big-raise branch should grow REF (1d42)");
    }
    if (!strstr(status, "raises taxes") || !strstr(status, "4")) {
      fprintf(stderr, "unit_ai_king: big-raise status: '%s'\n", status);
      return fail("big-raise branch should mention the new rate in status");
    }
  }
  {
    /*
     * No-popups auto path (FUN_38fd_3dc8's tea-party choice has no
     * documented AI/auto answer — this port's own invented stand-in,
     * see ai_king_tax_event's header): a real hike that crosses
     * AI_KING_BOYCOTT_TAX_MIN with SoL/bells over threshold reverts
     * itself and boycotts the single roulette-picked cargo, same turn,
     * no ai_popups needed. rebel=101, tax=20, SoL=45 (below declare gate),
     * turn=44, seed=1 -> hike delta +4 (tax 20->24), then one bid-weighted
     * dump-goods roll over the same rng stream picks Tobacco (heavy weight).
     */
    turn = 44;
    col1.head.game_options.woi = 0;
    ai_king_latch_set(&col1, 0, 0);
    ai_king_latch_set(&col1, 5, 0);
    col1.head.rebel_sentiment_report = 101;
    col1.nation[0].tax_rate = 20;
    europe.tax_percent = 20;
    col1.nation[0].gold = 0;
    col1.colony[0].rebel_dividend = 45;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].liberty_bells_total = 0;
    col1.nation[0].boycott_bitmap = 0;
    europe.cargo_count = COLONIZE_CARGO_COUNT;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      europe.cargo[c].bid = 1;
    }
    europe.cargo[COLONIZE_CARGO_TOBACCO].bid = 500;
    ctx.europe = &europe;
    /*
     * bugs.md: the roulette only sees cargos a colony actually holds — you
     * cannot dump 0 tons in protest — so stock the two the pick may name.
     */
    memset(c->stock, 0, sizeof(c->stock));
    c->stock[COLONIZE_CARGO_TOBACCO] = 120;
    c->stock[COLONIZE_CARGO_SUGAR] = 30;
    ColonizeDosRng tea_rng;
    dos_rng_seed(&tea_rng, 1u);
    ctx.rng = &tea_rng;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 20) {
      fprintf(stderr, "unit_ai_king: auto-teaparty tax_rate=%u (want reverted to 20)\n",
              col1.nation[0].tax_rate);
      return fail("no-popups auto path should revert the hike (tea party)");
    }
    if ((col1.nation[0].boycott_bitmap & (1u << COLONIZE_CARGO_TOBACCO)) == 0) {
      return fail("no-popups auto path should boycott the picked cargo (Tobacco)");
    }
    /* Exactly one cargo bit — the single roulette pick, not a fixed
     * Sugar-first two-cargo boycott (that shape is retired). */
    int bits = 0;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      if (col1.nation[0].boycott_bitmap & (1u << c)) {
        bits++;
      }
    }
    if (bits != 1) {
      fprintf(stderr, "unit_ai_king: auto-teaparty boycott_bitmap=0x%x bits=%d\n",
              (unsigned)col1.nation[0].boycott_bitmap, bits);
      return fail("no-popups auto path should boycott exactly one cargo");
    }
    if (!strstr(status, "tea party") || !strstr(status, "20")) {
      fprintf(stderr, "unit_ai_king: auto-teaparty status: '%s'\n", status);
      return fail("no-popups auto path tea-party status should mention the reverted rate");
    }

    /* Empty warehouses → nothing to throw in the sea, so the hike just stands. */
    turn = 44;
    col1.nation[0].tax_rate = 20;
    europe.tax_percent = 20;
    col1.nation[0].boycott_bitmap = 0;
    memset(c->stock, 0, sizeof(c->stock));
    dos_rng_seed(&tea_rng, 1u);
    ctx.rng = &tea_rng;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].boycott_bitmap != 0) {
      fprintf(stderr, "unit_ai_king: empty-store boycott_bitmap=0x%x\n",
              (unsigned)col1.nation[0].boycott_bitmap);
      return fail("a tea party must not boycott a cargo no colony stores");
    }
    if (col1.nation[0].tax_rate == 20) {
      return fail("with nothing to dump the hike should stand, not revert");
    }
  }
  {
    /*
     * Same shape, but below AI_KING_BOYCOTT_TAX_MIN so the auto path
     * never evaluates SoL/bells at all: the hike simply stands.
     * rebel=100, tax=0, SoL=0, turn=44, seed=1 -> delta +4, tax 0->4.
     */
    turn = 44;
    col1.head.rebel_sentiment_report = 100;
    col1.nation[0].tax_rate = 0;
    europe.tax_percent = 0;
    col1.nation[0].gold = 0;
    col1.colony[0].rebel_dividend = 0;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].liberty_bells_total = 0;
    col1.nation[0].boycott_bitmap = 0;
    ColonizeDosRng stand_rng;
    dos_rng_seed(&stand_rng, 1u);
    ctx.rng = &stand_rng;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 4) {
      return fail("a hike below the auto-teaparty tax floor should just stand");
    }
    if (col1.nation[0].boycott_bitmap != 0) {
      return fail("a hike below the auto-teaparty tax floor must not boycott anything");
    }
  }

  /*
   * Dump-goods pick API (FUN_38fd_3dc8 thin): among candidate bits clear in
   * boycott_bitmap, dos_rng picks one — not a fixed Tobacco second refuse.
   * Cite: docs/fandom_col1994.md Boycott “named goods”; viceroy FUN_38fd_3dc8.
   * (Direct-function-call scenarios, unaffected by the turn/interval
   * redesign above — unchanged from before this pass.)
   */
  {
    ColonizeDosRng dump_rng;
    dos_rng_seed(&dump_rng, 42u);
    const uint16_t sugar_only = (uint16_t)(1u << COLONIZE_CARGO_SUGAR);
    const uint16_t all16 = 0xffffu;
    if (ai_king_pick_dump_goods_cargo(all16, all16, &dump_rng, NULL) != -1) {
      return fail("dump-goods pick must return -1 when all candidates boycotted");
    }
    if (ai_king_pick_dump_goods_cargo(0, 0, &dump_rng, NULL) != -1) {
      return fail("dump-goods pick must return -1 when candidate_mask empty");
    }
    if (ai_king_pick_dump_goods_cargo(sugar_only, all16, NULL, NULL) != -1) {
      return fail("dump-goods pick must return -1 when rng NULL");
    }
    const int picked =
      ai_king_pick_dump_goods_cargo(sugar_only, all16, &dump_rng, NULL);
    if (picked < 0 || picked >= COLONIZE_CARGO_COUNT) {
      return fail("dump-goods pick should return a cargo index");
    }
    if (picked == COLONIZE_CARGO_SUGAR) {
      return fail("dump-goods pick must skip already-boycotted Sugar bit");
    }
    if (((1u << picked) & sugar_only) != 0) {
      return fail("dump-goods pick returned boycotted bit");
    }
    /* Single eligible: must be that cargo (not invent Tobacco). */
    const uint16_t furs_only = (uint16_t)(1u << COLONIZE_CARGO_FURS);
    const int only =
      ai_king_pick_dump_goods_cargo(sugar_only, furs_only, &dump_rng, NULL);
    if (only != COLONIZE_CARGO_FURS) {
      return fail("dump-goods pick single-candidate must return Furs");
    }
    /* Weighted pick: high Europe bid cargo preferred over many turns. */
    {
      ColonizeDosRng w_rng;
      dos_rng_seed(&w_rng, 777u);
      int bids[COLONIZE_CARGO_COUNT];
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        bids[c] = 1;
      }
      bids[COLONIZE_CARGO_TOBACCO] = 500;
      int tobacco_hits = 0;
      const int trials = 40;
      for (int t = 0; t < trials; ++t) {
        const int p =
          ai_king_pick_dump_goods_cargo(sugar_only, all16, &w_rng, bids);
        if (p == COLONIZE_CARGO_TOBACCO) {
          tobacco_hits++;
        }
      }
      if (tobacco_hits < trials / 2) {
        fprintf(stderr,
                "unit_ai_king: weighted dump-goods Tobacco hits=%d/%d\n",
                tobacco_hits, trials);
        return fail("weighted dump-goods pick should favor high-bid Tobacco");
      }
    }
    /*
     * Eligibility: when cargo_bid non-NULL, bid<=0 cargos are ineligible
     * (FUN_38fd_3dc8 / Europe local_7a — refuse must not dump zero-price).
     */
    {
      ColonizeDosRng z_rng;
      dos_rng_seed(&z_rng, 1234u);
      int bids[COLONIZE_CARGO_COUNT];
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        bids[c] = 0;
      }
      bids[COLONIZE_CARGO_FURS] = 10;
      bids[COLONIZE_CARGO_COTTON] = 0;
      for (int t = 0; t < 20; ++t) {
        const int p =
          ai_king_pick_dump_goods_cargo(sugar_only, all16, &z_rng, bids);
        if (p != COLONIZE_CARGO_FURS) {
          fprintf(stderr, "unit_ai_king: bid>0 eligibility pick=%d (want Furs)\n",
                  p);
          return fail("dump-goods with bids must only pick bid>0 cargos");
        }
      }
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        bids[c] = 0;
      }
      if (ai_king_pick_dump_goods_cargo(sugar_only, all16, &z_rng, bids) != -1) {
        return fail("dump-goods with all bids==0 must return -1");
      }
    }
  }

  /*
   * Reset for the rest of the file: turn back below the interval gate
   * so nothing downstream accidentally rolls an audience (nothing past
   * this point cares about tax/audience — confirmed 2026-08-20 scoping
   * pass), and tax/SoL/bells/boycott back to the baseline the declare-
   * path scenarios below already expect.
   */
  turn = 1;
  col1.nation[0].tax_rate = 20;
  europe.tax_percent = 20;
  col1.colony[0].rebel_dividend = 60;
  col1.colony[0].rebel_divisor = 100;
  col1.nation[0].boycott_bitmap = 0;
  ai_king_latch_set(&col1, 2, 0);
  col1.nation[0].liberty_bells_total = 0;
  col1.head.king_audience_streak = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));

  /*
   * Thin pre-declare SoL chrome: SoL 40..49 → restless status; no congress yet.
   * Autumn skips tax so status is not overwritten by a tax hike line.
   */
  year = 1590;
  autumn = 1;
  col1.colony[0].rebel_dividend = 45;
  col1.colony[0].rebel_divisor = 100;
  col1.nation[0].liberty_bells_total = 50;
  status[0] = '\0';
  {
    const int sol45 = ai_king_sol_percent(&ctx, 0);
    if (sol45 != 45) {
      fprintf(stderr, "unit_ai_king: unexpected SoL %d (want 45)\n", sol45);
      return fail("SoL chrome setup");
    }
  }
  ai_king_nation_turn(&ctx);
  if (col1.head.rebel_sentiment_report != 45) {
    fprintf(
      stderr,
      "unit_ai_king: rebel_sentiment_report want 45 got %u\n",
      (unsigned)col1.head.rebel_sentiment_report
    );
    return fail("2424 tail must cache nation SoL in rebel_sentiment_report");
  }
  if (ai_king_latch_get(&col1, 0) != 0) {
    return fail("SoL 45 should not declare WoI");
  }
  if (ai_king_latch_get(&col1, 5) != 0) {
    return fail("SoL 45 should not set congress confirm unknown46[5]");
  }
  if (!strstr(status, "Sons of Liberty") || !strstr(status, "45")) {
    fprintf(stderr, "unit_ai_king: SoL chrome status: '%s'\n", status);
    return fail("SoL 40-49 should set restless status line");
  }
  /* unknown46 consistency: restless chrome must not set WoI or congress. */
  if (ai_king_latch_get(&col1, 0) != 0 || ai_king_latch_get(&col1, 5) != 0) {
    return fail("restless SoL chrome must leave WoI/congress unknown46 clear");
  }
  /* Optional tax mention when tax_rate already in refuse band (≥20). */
  if (col1.nation[0].tax_rate >= 20 &&
      !strstr(status, "Tax is at") && !strstr(status, "tax")) {
    fprintf(stderr, "unit_ai_king: restless+high-tax status: '%s'\n", status);
    return fail("SoL restless with high tax_rate should mention tax");
  }

  /*
   * 2424 tail decile SoL notify (DS:0x53d8 dedup) must survive the restless
   * chrome below it: both fire in the SoL 40..49 band, and the restless
   * block used to unconditionally overwrite ctx->status.
   */
  {
    year = 1590;
    autumn = 1;
    col1.colony[0].rebel_dividend = 45;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].liberty_bells_total = 50;
    col1.nation[0].founding_father_count = 4;
    col1.head.sol_pct_last_notified = 3; /* previous decile 30-39% */
    ai_king_latch_set(&col1, 0, 0);
    ai_king_latch_set(&col1, 5, 0);
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    if (!strstr(status, "Congress notes")) {
      fprintf(stderr, "unit_ai_king: decile+restless status: '%s'\n", status);
      return fail("decile SoL notify must not be clobbered by restless chrome");
    }
    col1.nation[0].founding_father_count = 0;
  }

  /*
   * WoI unknown46[0] SoL gate (FUN_43f7_2564 / fandom total SoL ≥ 50%):
   * SoL 49 must NOT declare regardless of liberty bells.
   */
  {
    year = 1591;
    autumn = 1;
    col1.colony[0].rebel_dividend = 49;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].liberty_bells_total = 200;
    status[0] = '\0';
    {
      const int sol49 = ai_king_sol_percent(&ctx, 0);
      if (sol49 != 49) {
        fprintf(stderr, "unit_ai_king: unexpected SoL %d (want 49)\n", sol49);
        return fail("SoL49 declare-gate setup");
      }
    }
    ai_king_nation_turn(&ctx);
    if (ai_king_latch_get(&col1, 0) != 0) {
      return fail("SoL 49 must not set WoI unknown46[0]");
    }
    if (ai_king_latch_get(&col1, 5) != 0) {
      return fail("SoL 49 must not set congress unknown46[5]");
    }
  }

  /* Declare path: autumn skips tax; SoL≥50 only (no bells gate). Wave runs same turn. */
  year = 1600;
  autumn = 1;
  col1.colony[0].rebel_dividend = 60;
  col1.colony[0].rebel_divisor = 100;
  col1.nation[0].liberty_bells_total = 200;
  col1.nation[0].gold = 0; /* 2244 cannot-afford once; hire cleared later */
  europe.gold = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  col1.player[1].control = 0;
  col1.player[2].control = 0;
  col1.player[3].control = 0;
  status[0] = '\0';
  const int units_before = count_active(&units);

  /*
   * Pre-declare WAR vs the soon-to-be-eliminated nation 2, so the 0108
   * diplo-clear/set (below) has something real to clear. Nation 1 is the
   * crown fold for human=0 (ai_king_crown_nation) and is excluded from
   * elimination/0108 entirely, matching DOS (0108 never targets 0x53d2).
   */
  ai_diplo_or_both(&col1, 0, 2, (uint8_t)AI_DIPLO_WAR);

  /* Seed a pre-declare country so 160a rename is observable. */
  snprintf(col1.player[0].country_name, sizeof(col1.player[0].country_name), "England");
  snprintf(europe.nation_name, sizeof(europe.nation_name), "England");

  ai_king_nation_turn(&ctx);
  if (ai_king_latch_get(&col1, 0) == 0) {
    return fail("declare should set WoI flag unknown46[0]");
  }
  if (ai_king_latch_get(&col1, 5) == 0) {
    return fail("declare should set congress confirm unknown46[5]");
  }
  /* bugs.md 245: NO rename — DOS 160a is only the signing cinematic;
   * "United Colonies" was a port invention. The faction reads Rebels/Tory
   * via units_combat_nation_label instead. */
  if (strcmp(col1.player[0].country_name, "England") != 0) {
    fprintf(stderr, "unit_ai_king: country_name after declare: '%s'\n",
            col1.player[0].country_name);
    return fail("declare must NOT rename player.country_name (bugs.md 245)");
  }
  if (strcmp(europe.nation_name, "England") != 0) {
    return fail("declare must NOT rename europe.nation_name (bugs.md 245)");
  }
  /* bugs.md 234: the crown's borrowed slot (human 0 → 1) stays a live AI
   * combatant (DOS 1a26: *(0x53d2*0x34+0x543f)=1); only the other two Euro
   * powers withdraw. crown_nation_id must be stamped for DOS interop. */
  if (col1.player[1].control != 1 || col1.player[2].control != 2 ||
      col1.player[3].control != 2) {
    return fail("declare: crown slot AI (1), others withdrawn (2)");
  }
  if (col1.head.crown_nation_id != 1) {
    return fail("declare should stamp head.crown_nation_id");
  }
  /*
   * FUN_43f7_0108 diplo-clear/set: eliminated nation 2 loses WAR/PEACE and
   * gains MET vs both the declaring human (0) and the crown fold (1).
   */
  if ((ai_diplo_read(&col1, 2, 0) & (AI_DIPLO_WAR | AI_DIPLO_PEACE)) != 0) {
    return fail("0108 should clear WAR/PEACE between eliminated nation and human");
  }
  if ((ai_diplo_read(&col1, 0, 2) & (AI_DIPLO_WAR | AI_DIPLO_PEACE)) != 0) {
    return fail("0108 should clear WAR/PEACE symmetrically (human side)");
  }
  if ((ai_diplo_read(&col1, 2, 0) & AI_DIPLO_MET) == 0) {
    return fail("0108 should mark eliminated nation as MET vs human");
  }
  if ((ai_diplo_read(&col1, 2, 1) & AI_DIPLO_MET) == 0) {
    return fail("0108 should mark eliminated nation as MET vs crown fold");
  }
  /* bugs.md: the first wave WAITS one turn after the declaration — the
   * declare turn itself must land nothing. */
  if (count_nation(&units, 1) >= 1) {
    return fail("declare turn must not land the wave (one-turn wait)");
  }
  ai_king_nation_turn(&ctx);
  /* Seed then drain: residual +1 regular may leave pools non-zero; require spawn. */
  if (count_nation(&units, 1) < 1) {
    return fail("post-declare wave should spawn crown (nation 1) unit");
  }
  if (count_active(&units) <= units_before) {
    return fail("wave should increase unit count");
  }
  if (count_nation(&units, 0) != 0) {
    return fail("REF/irregular must not spawn as human nation");
  }
  /*
   * FUN_43f7_0982: declare seeds force[2]>0 → same-beat Man-O-War on water
   * adjacent to the weakest coastal colony and max(3, min(garrison, 31))
   * land units on the adjacent land tiles (no cargo boarding — DOS lands
   * straight onto the tiles beside the ship).
   */
  {
    const int crown_sea = count_nation_sea(&units, 1);
    const int crown_land = count_nation_land(&units, 1);
    if (crown_sea < 1 || crown_land < 3) {
      fprintf(stderr, "unit_ai_king: post-declare 0982 sea=%d land=%d (want ≥1 ship + ≥3 land)\n",
              crown_sea, crown_land);
      return fail("0982 should spawn a MoW and at least 3 land units");
    }
    int mow_on_adj_water = 0;
    int land_adjacent = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 1) {
        continue;
      }
      if (u->type_index == ty_mow) {
        if (!map_tile_is_water(&map, u->x, u->y)) {
          fprintf(stderr, "unit_ai_king: MoW at (%d,%d) not on water\n", u->x, u->y);
          return fail("0982 MoW must spawn on water tile");
        }
        if (abs(u->x - 5) <= 1 && abs(u->y - 5) <= 1) {
          mow_on_adj_water = 1;
        }
      } else if (!units_is_sea(&units, u->id) && abs(u->x - 5) <= 1 && abs(u->y - 5) <= 1) {
        if (u->aboard_ship_id >= 0) {
          return fail("0982 lands units on tiles, never as cargo");
        }
        land_adjacent++;
      }
    }
    if (!mow_on_adj_water) {
      return fail("0982 MoW must spawn on water adjacent to target colony");
    }
    if (land_adjacent < 3) {
      fprintf(stderr, "unit_ai_king: 0982 land adjacent=%d\n", land_adjacent);
      return fail("0982 must land ≥3 units beside the colony");
    }
  }
  /*
   * Thin 1528: successful 0982 spawn writes @INVASION status (VGA PARKED).
   * Same-turn war_act may overwrite with capture. The rebel troop-gift
   * purchase (FUN_43f7_2022 rebel branch, ai_king_merc_offer) never fires
   * in this test — ctx.rng is NULL here, and that mechanic is a no-op
   * without a real RNG (see the dedicated seeded coverage further below,
   * "2022 rebel troop-gift purchase"); no once-per-war flag exists for it
   * to set (DOS has none — see king_ref.md "2244/2022 — corrected").
   */
  if (!strstr(status, "Expeditionary Force") && !strstr(status, "lands near") &&
      !strstr(status, "captured")) {
    fprintf(stderr, "unit_ai_king: status after wave: '%s'\n", status);
    return fail("0982 wave should set thin 1528 @INVASION (or same-turn capture) status");
  }
  /* Pools seeded on declare then drained; still expect REF-present stand-in. */
  if (ai_king_latch_get(&col1, 1) == 0) {
    return fail("wave should set REF-present unknown46[1]");
  }
  /* Declare should seed thin backup_force (10f0 stand-in). */
  if (col1.head.backup_force[0] == 0 && col1.head.backup_force[1] == 0 &&
      col1.head.backup_force[2] == 0 && col1.head.backup_force[3] == 0) {
    return fail("declare should seed backup_force for 10f0");
  }
  if (col1.head.rival_nation_slot_1 != 2 || col1.head.rival_nation_slot_2 != 3) {
    fprintf(
      stderr,
      "unit_ai_king: rival slots want 2,3 got %d,%d\n",
      (int)col1.head.rival_nation_slot_1,
      (int)col1.head.rival_nation_slot_2
    );
    return fail("declare should cache rival_nation_slot_1/2 at 1a26");
  }

  /*
   * FUN_43f7_10f0 (re-read 2026-08-28, port_plan P5.5): REF empty + backup
   * pools → the intervention force lands for the HUMAN's own nation
   * (DS:0x5398), i.e. player-controlled: one Man-O-War on the water tile
   * next to the colony (pool[2] −1), then Cont. Cav. ≤ 2 (pool[1]),
   * Artillery ≤ 2 (pool[3]) and Cont. Army = 6 − those (pool[0]), each
   * capped by its pool, all at the colony. Pools 4/3/1/2 → MoW + 2 Army +
   * 2 Cav. + 2 Art.; pools left 2/1/0/0. No ally-tagged units.
   */
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  /* Crown wave may have captured the port; restore human ownership for landing pick. */
  colonies.colonies[0].nation_id = 0;
  /* DOS 2022 (75007): the free backup drain runs only AFTER the bells-bought
   * @INTERVENTION announce set 0x5382 bit2 — model latch here. */
  ai_king_latch_set(&col1, AI_KING_INTERVENE_ANNOUNCED_BYTE, 1);
  col1.head.backup_force[0] = 4;
  col1.head.backup_force[1] = 3;
  col1.head.backup_force[2] = 1;
  col1.head.backup_force[3] = 2;
  /* A free ocean tile east of the colony: DOS skips a water tile holding a
   * foreign unit (281f_0682), and the crown wave above parked units on (4,5). */
  map.terrain[5 * 16 + 6] = 25;
  {
    const int human_before = count_nation(&units, 0);
    const int n2_before = count_nation(&units, 2);
    const int n3_before = count_nation(&units, 3);
    const int units_mid = count_active(&units);
    bool was_active[COLONIZE_UNITS_MAX];
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      was_active[i] = units.units[i].active;
    }
    ai_king_nation_turn(&ctx);
    const int human_spawned = count_nation(&units, 0) - human_before;
    if (human_spawned != 7) {
      fprintf(stderr, "unit_ai_king: 10f0 human units +%d (want 7)\n", human_spawned);
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (u->active && u->x >= 3 && u->x <= 7 && u->y >= 3 && u->y <= 7) {
          fprintf(stderr, "  unit %d nation %d type %d at %d,%d aboard %d\n", i, u->nation_id,
                  u->type_index, u->x, u->y, u->aboard_ship_id);
        }
      }
      return fail("10f0 should land MoW + 6 troops for the human nation");
    }
    if (count_nation(&units, 2) != n2_before || count_nation(&units, 3) != n3_before) {
      return fail("10f0 must not spawn ally-tagged units (force is player-controlled)");
    }
    if (count_active(&units) <= units_mid) {
      return fail("intervention turn should increase unit count");
    }
    if (col1.head.backup_force[0] != 2 || col1.head.backup_force[1] != 1 ||
        col1.head.backup_force[2] != 0 || col1.head.backup_force[3] != 0) {
      fprintf(stderr, "unit_ai_king: 10f0 pools %u/%u/%u/%u (want 2/1/0/0)\n",
              (unsigned)col1.head.backup_force[0], (unsigned)col1.head.backup_force[1],
              (unsigned)col1.head.backup_force[2], (unsigned)col1.head.backup_force[3]);
      return fail("10f0 pool caps: Cav ≤2, Art ≤2, Army = 6 − those, MoW −1");
    }
    int mow_at_sea = 0;
    int army = 0;
    int cav = 0;
    int art = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 0 || was_active[i]) {
        continue;
      }
      if (u->type_index == ty_mow) {
        if (map_tile_is_water(&map, u->x, u->y) && u->x >= 4 && u->x <= 6 && u->y >= 4 &&
            u->y <= 6) {
          mow_at_sea++;
        }
      } else if (u->x == 5 && u->y == 5) {
        if (u->type_index == ty_cont_army) {
          army++;
        } else if (u->type_index == ty_cont_cav) {
          cav++;
        } else if (u->type_index == ty_artillery) {
          art++;
        }
      }
      /* Orders: spawned with none; the same turn's colony garrison pass may
       * fortify Cont. Army in place, so no assertion here. */
    }
    if (mow_at_sea != 1 || army != 2 || cav != 2 || art != 2) {
      fprintf(stderr, "unit_ai_king: 10f0 mow@sea=%d army=%d cav=%d art=%d\n",
              mow_at_sea, army, cav, art);
      return fail("10f0 landing composition/placement");
    }
  }

  /* Small pools: 1/0/1/0 → MoW (pool[2] 1→0) + 1 Army. (DOS 2022 75007: the
   * free drain needs the MoW pool nonzero — an empty 0x53e6 routes to the
   * merc-roll branch instead, so pool[2]=0 would land nothing here.) */
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  colonies.colonies[0].nation_id = 0;
  col1.head.backup_force[0] = 1;
  col1.head.backup_force[1] = 0;
  col1.head.backup_force[2] = 1;
  col1.head.backup_force[3] = 0;
  {
    const int human_before = count_nation(&units, 0);
    ai_king_nation_turn(&ctx);
    const int human_spawned = count_nation(&units, 0) - human_before;
    if (human_spawned != 2 || col1.head.backup_force[0] != 0) {
      fprintf(stderr, "unit_ai_king: 10f0 small pools +%d pool0=%u\n", human_spawned,
              (unsigned)col1.head.backup_force[0]);
      return fail("10f0 small pools should land MoW + 1 Army");
    }
  }
  col1.head.difficulty = 0; /* restore for later checks */
  /* Retire the landed force so the REF hunt / capture probes below see the
   * same undefended colony they were written against. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units.units[i];
    if (u->active && u->nation_id == 0 &&
        (u->type_index == ty_mow || u->type_index == ty_cont_army ||
         u->type_index == ty_cont_cav || u->type_index == ty_artillery) &&
        u->x >= 4 && u->x <= 6 && u->y >= 4 && u->y <= 6) {
      u->active = false;
    }
  }

  /*
   * REF land hunt + colony capture + fortify Regular (fandom REF AI / conquest):
   * Idle crown Regular with moves → AI_MOVE toward nearest human land unit;
   * REF on human colony tile → colonies_capture (even with 0 moves) then
   * UNITS_ORDER_FORTIFY on the capturing Regular.
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    /* A second, inland human colony keeps the war alive after Jamestown
     * falls (otherwise the same turn ends in @LOSING2 and overwrites the
     * capture status this block asserts). Deactivated again below. */
    ColonizeColony* inland = &colonies.colonies[1];
    inland->id = 1;
    inland->active = true;
    inland->nation_id = 0;
    inland->x = 14;
    inland->y = 14;
    for (int y = 14; y >= 1; --y) {
      bool free_tile = false;
      for (int x = 14; x >= 8 && !free_tile; --x) {
        if (!map_tile_is_land(&map, x, y)) {
          continue;
        }
        free_tile = true;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &units.units[i];
          if (u->active && u->x == x && u->y == y) {
            free_tile = false;
            break;
          }
        }
        if (free_tile) {
          inland->x = x;
          inland->y = y;
        }
      }
      if (free_tile) {
        break;
      }
    }
    inland->population = 10; /* > Jamestown: 06a6 irregulars pick the weakest colony */
    inland->colonist_count = 10;
    snprintf(inland->name, sizeof(inland->name), "Inland");
    if (colonies.colony_count < 2) {
      colonies.colony_count = 2;
    }
    /* …coastal (a port must survive Jamestown's fall or @LOSING1 fires)… */
    map.terrain[inland->y * 16 + inland->x - 1] = 25;
    /* …with a fortified human Soldier, so the crown wave that lands on the
     * weakest colony each turn can't take it undefended. */
    const int inland_guard = units_spawn_allow_stack(&units, ty_soldier, inland->x, inland->y);
    if (inland_guard < 0) {
      return fail("capture setup should spawn inland guard");
    }
    {
      ColonizeUnit* g = units_get(&units, inland_guard);
      g->nation_id = 0;
      g->moves_left = 0;
      g->orders = UNITS_ORDER_FORTIFY;
    }
    /* Park existing crown movers so hunt/capture probes are stable. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
      }
    }
    /* Capture: REF Regular already on human colony, no moves. */
    const int cap_id = units_spawn_allow_stack(&units, ty_regular, 5, 5);
    if (cap_id < 0) {
      return fail("capture setup should spawn crown Regular on colony");
    }
    {
      ColonizeUnit* cu = units_get(&units, cap_id);
      if (!cu) {
        return fail("capture setup unit lookup");
      }
      cu->nation_id = 1;
      cu->moves_left = 0;
      cu->orders = UNITS_ORDER_NONE;
      cu->goto_x = -1;
      cu->goto_y = -1;
    }
    /* Hunt: human Soldier at (12,5); idle crown Regular at (10,5) with moves. */
    const int prey_id = units_spawn_allow_stack(&units, ty_soldier, 12, 5);
    const int hunter_id = units_spawn_allow_stack(&units, ty_regular, 10, 5);
    if (prey_id < 0 || hunter_id < 0) {
      return fail("land-hunt setup should spawn human Soldier + crown Regular");
    }
    {
      ColonizeUnit* prey = units_get(&units, prey_id);
      ColonizeUnit* hunter = units_get(&units, hunter_id);
      if (!prey || !hunter) {
        return fail("land-hunt setup unit lookup");
      }
      prey->nation_id = 0;
      prey->moves_left = 0;
      hunter->nation_id = 1;
      hunter->moves_left = 1 * UNITS_MP_PER_TILE;
      hunter->orders = UNITS_ORDER_NONE;
      hunter->goto_x = -1;
      hunter->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    /* REF capture clears human ownership (conquest — colonies_capture). */
    if (colonies.colonies[0].nation_id != 1) {
      return fail("REF on human colony should colonies_capture (owner → crown)");
    }
    if (colonies.colonies[0].nation_id == 0) {
      return fail("REF capture must clear human colony ownership");
    }
    inland->active = false; /* don't perturb later weakest-port / hunt picks */
    units.units[inland_guard].active = false;
    /* Thin conquest status (full chrome PARKED): exact phrase + colony name. */
    if (!strstr(status, "The King's forces have captured") || !strstr(status, "Jamestown")) {
      fprintf(stderr, "unit_ai_king: capture status: '%s'\n", status);
      return fail("REF capture should set 'The King's forces have captured %s!' status");
    }
    {
      /* Any crown Regular on the colony may capture first (wave leftovers); one
       * must end FORTIFY after capture. */
      int fortified = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != 1 || u->x != 5 || u->y != 5) {
          continue;
        }
        if (u->type_index == ty_regular && u->orders == UNITS_ORDER_FORTIFY) {
          fortified = 1;
          break;
        }
      }
      if (!fortified) {
        const ColonizeUnit* cu = units_get_const(&units, cap_id);
        fprintf(stderr, "unit_ai_king: post-capture cap orders=%d (want a FORTIFY Regular)\n",
                cu ? cu->orders : -1);
        return fail("capture should fortify one Regular on colony tile");
      }
    }
    /*
     * Capture cap-2: two Regulars on human colony tile → both FORTIFY when
     * second has moves (Colonization.pdf Defending a Colony; king_ref thin).
     */
    {
      colonies.colonies[0].nation_id = 0;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
            u->orders = UNITS_ORDER_NONE;
          }
        }
      }
      const int cap_a = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      const int cap_b = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      if (cap_a < 0 || cap_b < 0) {
        return fail("capture cap-2 setup should spawn two crown Regulars");
      }
      {
        ColonizeUnit* a = units_get(&units, cap_a);
        ColonizeUnit* b = units_get(&units, cap_b);
        if (!a || !b) {
          return fail("capture cap-2 unit lookup");
        }
        a->nation_id = 1;
        a->moves_left = 0;
        a->orders = UNITS_ORDER_NONE;
        b->nation_id = 1;
        b->moves_left = 1 * UNITS_MP_PER_TILE;
        b->orders = UNITS_ORDER_NONE;
      }
      ai_king_nation_turn(&ctx);
      if (colonies.colonies[0].nation_id != 1) {
        return fail("capture cap-2 should colonies_capture");
      }
      int fortified = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != 1 || u->x != 5 || u->y != 5) {
          continue;
        }
        if (u->type_index == ty_regular &&
            (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED)) {
          fortified++;
        }
      }
      if (fortified != 2) {
        fprintf(stderr, "unit_ai_king: capture cap-2 fortified=%d (want 2)\n", fortified);
        return fail("capture with two Regulars should fortify both when second has moves");
      }
    }
    {
      const ColonizeUnit* hunter = units_get_const(&units, hunter_id);
      if (!hunter || !hunter->active) {
        return fail("land-hunt Regular should remain active");
      }
      if (hunter->orders != UNITS_ORDER_AI_MOVE || hunter->goto_x != 12 ||
          hunter->goto_y != 5) {
        fprintf(stderr, "unit_ai_king: hunt goto=(%d,%d) orders=%d (want AI_MOVE→12,5)\n",
                hunter->goto_x, hunter->goto_y, hunter->orders);
        return fail("REF Regular should AI_MOVE toward nearest human land unit");
      }
      /* One step east toward prey (10,5) → (11,5). */
      if (hunter->x != 11 || hunter->y != 5) {
        fprintf(stderr, "unit_ai_king: hunt pos=(%d,%d) (want 11,5)\n", hunter->x,
                hunter->y);
        return fail("REF land hunt should step toward human land unit");
      }
    }
    /*
     * REF stack cap-2 (Colonization.pdf Defending a Colony; king_ref thin
     * multi-garrison): second Regular with moves on captured colony fortifies
     * when only one garrison slot is taken; third hunts.
     */
    {
      /* Clear the colony tile: only one garrison Regular may remain. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != 1) {
          continue;
        }
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
          u->orders = UNITS_ORDER_NONE;
        }
      }
      /* Sole fortified garrison on crown colony. */
      {
        ColonizeUnit* cu = units_get(&units, cap_id);
        if (!cu || !cu->active) {
          return fail("REF stack needs capture Regular as garrison");
        }
        cu->x = 5;
        cu->y = 5;
        cu->nation_id = 1;
        cu->orders = UNITS_ORDER_FORTIFY;
        cu->moves_left = 0;
      }
      colonies.colonies[0].nation_id = 1;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      col1.head.backup_force[0] = 0;
      col1.head.backup_force[1] = 0;
      col1.head.backup_force[2] = 0;
      col1.head.backup_force[3] = 0;
      const int extra_id = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      const int stack_prey = units_spawn_allow_stack(&units, ty_soldier, 12, 8);
      if (extra_id < 0 || stack_prey < 0) {
        return fail("REF stack setup should spawn extra Regular + human prey");
      }
      {
        ColonizeUnit* ex = units_get(&units, extra_id);
        ColonizeUnit* prey = units_get(&units, stack_prey);
        if (!ex || !prey) {
          return fail("REF stack unit lookup");
        }
        ex->nation_id = 1;
        ex->moves_left = 1 * UNITS_MP_PER_TILE;
        ex->orders = UNITS_ORDER_NONE;
        ex->goto_x = -1;
        ex->goto_y = -1;
        prey->nation_id = 0;
        prey->moves_left = 0;
      }
      ai_king_nation_turn(&ctx);
      int fortified = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != 1 || u->type_index != ty_regular) {
          continue;
        }
        if ((u->x == 5 && u->y == 5) &&
            (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED)) {
          fortified++;
        }
      }
      {
        const ColonizeUnit* ex = units_get_const(&units, extra_id);
        if (!ex || !ex->active) {
          return fail("REF stack extra Regular should remain active");
        }
        if (ex->orders != UNITS_ORDER_FORTIFY && ex->orders != UNITS_ORDER_FORTIFIED) {
          fprintf(stderr, "unit_ai_king: stack extra orders=%d (want FORTIFY cap-2)\n",
                  ex->orders);
          return fail("REF stack: second Regular with moves should fortify (cap 2)");
        }
      }
      if (fortified != 2) {
        fprintf(stderr, "unit_ai_king: fortified Regulars on colony=%d (want exactly 2)\n",
                fortified);
        return fail("REF stack should fortify two Regulars on colony when second has moves");
      }
      /* Third extra with moves must hunt when cap-2 stack is full. */
      const int third_id = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      const int third_prey = units_spawn_allow_stack(&units, ty_soldier, 12, 9);
      if (third_id < 0 || third_prey < 0) {
        return fail("REF stack third setup should spawn Regular + prey");
      }
      {
        ColonizeUnit* th = units_get(&units, third_id);
        ColonizeUnit* prey = units_get(&units, third_prey);
        if (!th || !prey) {
          return fail("REF stack third unit lookup");
        }
        th->nation_id = 1;
        th->moves_left = 1 * UNITS_MP_PER_TILE;
        th->orders = UNITS_ORDER_NONE;
        th->goto_x = -1;
        th->goto_y = -1;
        prey->nation_id = 0;
        prey->moves_left = 0;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* th = units_get_const(&units, third_id);
        if (!th || !th->active) {
          return fail("REF stack third Regular should remain active");
        }
        if (th->orders == UNITS_ORDER_FORTIFY || th->orders == UNITS_ORDER_FORTIFIED) {
          return fail("REF stack: third Regular must hunt when two already fortified");
        }
        if (th->orders != UNITS_ORDER_AI_MOVE) {
          fprintf(stderr, "unit_ai_king: stack third orders=%d (want AI_MOVE hunt)\n",
                  th->orders);
          return fail("REF stack third Regular should hunt, not fortify");
        }
      }
    }
    /*
     * After-capture next colony (fandom REF uncaptured-port pressure):
     * founding capital captured + two fortify slots taken → idle third Regular
     * must prefer next nearest remaining human colony over a closer human land
     * unit. Capital MD slack must not apply (founding capital is no longer human).
     */
    {
      ColonizeColony* next_col = &colonies.colonies[2];
      next_col->id = 2;
      next_col->active = true;
      next_col->nation_id = 0;
      next_col->x = 12;
      next_col->y = 5;
      next_col->population = 2;
      next_col->colonist_count = 2;
      next_col->has_building[0] = false;
      snprintf(next_col->name, sizeof(next_col->name), "Plymouth");
      if (colonies.colony_count < 3) {
        colonies.colony_count = 3;
      }
      colonies.colonies[0].nation_id = 1; /* captured capital */
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active) {
          continue;
        }
        if (u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
            u->orders = UNITS_ORDER_NONE;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 1;
          u->y = 14;
          u->moves_left = 0;
        }
      }
      /* Two fortified garrisons on captured capital (cap-2 stack full). */
      {
        ColonizeUnit* cu = units_get(&units, cap_id);
        if (!cu || !cu->active) {
          return fail("after-capture next-colony needs capture Regular as garrison");
        }
        cu->x = 5;
        cu->y = 5;
        cu->nation_id = 1;
        cu->orders = UNITS_ORDER_FORTIFY;
        cu->moves_left = 0;
      }
      const int garrison2 = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      if (garrison2 < 0) {
        return fail("after-capture next-colony needs second fortified Regular");
      }
      {
        ColonizeUnit* g2 = units_get(&units, garrison2);
        if (!g2) {
          return fail("after-capture next-colony garrison2 lookup");
        }
        g2->nation_id = 1;
        g2->orders = UNITS_ORDER_FORTIFY;
        g2->moves_left = 0;
      }
      /* Closer human Soldier decoy (MD=2) vs next colony at (12,5) (MD=7). */
      const int decoy_id = units_spawn_allow_stack(&units, ty_soldier, 7, 5);
      const int next_hunter = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      if (decoy_id < 0 || next_hunter < 0) {
        return fail("after-capture next-colony setup spawn");
      }
      {
        ColonizeUnit* decoy = units_get(&units, decoy_id);
        ColonizeUnit* h = units_get(&units, next_hunter);
        if (!decoy || !h) {
          return fail("after-capture next-colony unit lookup");
        }
        decoy->nation_id = 0;
        decoy->moves_left = 0;
        h->nation_id = 1;
        h->moves_left = 1 * UNITS_MP_PER_TILE;
        h->orders = UNITS_ORDER_NONE;
        h->goto_x = -1;
        h->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* h = units_get_const(&units, next_hunter);
        if (!h || !h->active) {
          return fail("after-capture next-colony hunter should remain active");
        }
        if (h->orders == UNITS_ORDER_FORTIFY || h->orders == UNITS_ORDER_FORTIFIED) {
          return fail("after-capture extra must not join capital fortify stack");
        }
        if (h->orders != UNITS_ORDER_AI_MOVE || h->goto_x != 12 || h->goto_y != 5) {
          fprintf(stderr,
                  "unit_ai_king: after-capture next goto=(%d,%d) orders=%d "
                  "(want next colony 12,5 not decoy 7,5)\n",
                  h->goto_x, h->goto_y, h->orders);
          return fail("after-capture idle extra should hunt next nearest human colony");
        }
      }
      next_col->active = false;
    }
    /* Restore human port for later promote / merc checks. */
    colonies.colonies[0].nation_id = 0;
  }

  /*
   * REF idle fortify (heal stand-in): Regular on own (crown) colony with moves
   * and no adjacent human foe/colony → UNITS_ORDER_FORTIFY (not hunt away).
   * Also: idle Regular on captured human capital prefers FORTIFY when stack
   * allows; already-FORTIFIED capital garrison stays put.
   */
  {
    colonies.colonies[0].nation_id = 0;
    /* Isolated crown colony at (8,8) — far from human (5,5). */
    ColonizeColony* crown_col = &colonies.colonies[3];
    crown_col->id = 3;
    crown_col->active = true;
    crown_col->nation_id = 1;
    crown_col->x = 8;
    crown_col->y = 8;
    crown_col->population = 2;
    crown_col->colonist_count = 1;
    if (colonies.colony_count < 4) {
      colonies.colony_count = 4;
    }
    map.terrain[8 * 16 + 8] = 1; /* land */
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
      }
    }
    /* Park human land units so none are adjacent to (8,8). */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        if (abs(u->x - 8) <= 1 && abs(u->y - 8) <= 1) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    const int idle_id = units_spawn_allow_stack(&units, ty_regular, 8, 8);
    if (idle_id < 0) {
      return fail("idle fortify setup should spawn crown Regular on crown colony");
    }
    {
      ColonizeUnit* idle = units_get(&units, idle_id);
      if (!idle) {
        return fail("idle fortify unit lookup");
      }
      idle->nation_id = 1;
      idle->moves_left = 2 * UNITS_MP_PER_TILE;
      idle->orders = UNITS_ORDER_NONE;
      idle->goto_x = -1;
      idle->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* idle = units_get_const(&units, idle_id);
      if (!idle || !idle->active) {
        return fail("idle fortify Regular should remain active");
      }
      if (idle->orders != UNITS_ORDER_FORTIFY) {
        fprintf(stderr, "unit_ai_king: idle fortify orders=%d (want FORTIFY)\n",
                idle->orders);
        return fail("Regular on crown colony with no adjacent foe should fortify");
      }
      if (idle->x != 8 || idle->y != 8) {
        return fail("idle fortify Regular should stay on crown colony");
      }
    }
    /*
     * Idle fortify cap-2: second Regular with moves fortifies; third hunts
     * (fandom REF garrison; same stack rule as post-capture).
     */
    {
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && u->id != idle_id) {
          u->moves_left = 0;
        }
      }
      /* Keep the fortified garrison; spawn extra with moves + distant prey. */
      {
        ColonizeUnit* idle = units_get(&units, idle_id);
        if (!idle || !idle->active) {
          return fail("idle fortify extras needs fortified Regular");
        }
        idle->orders = UNITS_ORDER_FORTIFY;
        idle->moves_left = 0;
      }
      const int extra_id = units_spawn_allow_stack(&units, ty_regular, 8, 8);
      const int prey_id = units_spawn_allow_stack(&units, ty_soldier, 14, 8);
      if (extra_id < 0 || prey_id < 0) {
        return fail("idle fortify extras setup should spawn Regular + prey");
      }
      {
        ColonizeUnit* ex = units_get(&units, extra_id);
        ColonizeUnit* prey = units_get(&units, prey_id);
        if (!ex || !prey) {
          return fail("idle fortify extras unit lookup");
        }
        ex->nation_id = 1;
        ex->moves_left = 1 * UNITS_MP_PER_TILE;
        ex->orders = UNITS_ORDER_NONE;
        ex->goto_x = -1;
        ex->goto_y = -1;
        prey->nation_id = 0;
        prey->moves_left = 0;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* idle = units_get_const(&units, idle_id);
        const ColonizeUnit* ex = units_get_const(&units, extra_id);
        if (!idle || !idle->active ||
            (idle->orders != UNITS_ORDER_FORTIFY && idle->orders != UNITS_ORDER_FORTIFIED)) {
          return fail("idle fortify extras: garrison Regular must stay FORTIFY");
        }
        if (!ex || !ex->active) {
          return fail("idle fortify extras: extra Regular should remain active");
        }
        if (ex->orders != UNITS_ORDER_FORTIFY && ex->orders != UNITS_ORDER_FORTIFIED) {
          fprintf(stderr, "unit_ai_king: idle extras orders=%d (want FORTIFY cap-2)\n",
                  ex->orders);
          return fail("idle fortify extras: second Regular with moves should fortify");
        }
      }
      {
        int fortified = 0;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &units.units[i];
          if (!u->active || u->nation_id != 1 || u->type_index != ty_regular) {
            continue;
          }
          if ((u->x == 8 && u->y == 8) &&
              (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED)) {
            fortified++;
          }
        }
        if (fortified != 2) {
          fprintf(stderr, "unit_ai_king: idle extras fortified=%d (want 2)\n", fortified);
          return fail("idle fortify extras should fortify two Regulars when second has moves");
        }
      }
      /* Third with moves hunts when cap-2 full. */
      const int third_id = units_spawn_allow_stack(&units, ty_regular, 8, 8);
      const int third_prey = units_spawn_allow_stack(&units, ty_soldier, 14, 9);
      if (third_id < 0 || third_prey < 0) {
        return fail("idle fortify third setup should spawn Regular + prey");
      }
      {
        ColonizeUnit* th = units_get(&units, third_id);
        ColonizeUnit* prey = units_get(&units, third_prey);
        if (!th || !prey) {
          return fail("idle fortify third unit lookup");
        }
        th->nation_id = 1;
        th->moves_left = 1 * UNITS_MP_PER_TILE;
        th->orders = UNITS_ORDER_NONE;
        th->goto_x = -1;
        th->goto_y = -1;
        prey->nation_id = 0;
        prey->moves_left = 0;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* th = units_get_const(&units, third_id);
        if (!th || !th->active) {
          return fail("idle fortify third Regular should remain active");
        }
        if (th->orders == UNITS_ORDER_FORTIFY || th->orders == UNITS_ORDER_FORTIFIED) {
          return fail("idle fortify third Regular must hunt when two already fortified");
        }
        if (th->orders != UNITS_ORDER_AI_MOVE) {
          fprintf(stderr, "unit_ai_king: idle third orders=%d (want AI_MOVE hunt)\n",
                  th->orders);
          return fail("idle fortify third Regular should hunt");
        }
      }
    }
    crown_col->active = false;

    /*
     * Captured human capital garrison: idle Regular on Jamestown (5,5) after
     * crown capture → FORTIFY when stack empty; already-FORTIFIED stays.
     */
    {
      colonies.colonies[0].nation_id = 1; /* captured capital */
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      col1.head.backup_force[0] = 0;
      col1.head.backup_force[1] = 0;
      col1.head.backup_force[2] = 0;
      col1.head.backup_force[3] = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active) {
          continue;
        }
        if (u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
            u->orders = UNITS_ORDER_NONE;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          /* Keep human land clear of capital adjacency. */
          if (abs(u->x - 5) <= 1 && abs(u->y - 5) <= 1) {
            u->x = 1;
            u->y = 14;
          }
        }
      }
      const int cap_garrison = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      if (cap_garrison < 0) {
        return fail("capital garrison setup should spawn Regular on Jamestown");
      }
      {
        ColonizeUnit* g = units_get(&units, cap_garrison);
        if (!g) {
          return fail("capital garrison unit lookup");
        }
        g->nation_id = 1;
        g->moves_left = 2 * UNITS_MP_PER_TILE;
        g->orders = UNITS_ORDER_NONE;
        g->goto_x = -1;
        g->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* g = units_get_const(&units, cap_garrison);
        if (!g || !g->active) {
          return fail("capital garrison Regular should remain active");
        }
        if (g->orders != UNITS_ORDER_FORTIFY) {
          fprintf(stderr, "unit_ai_king: capital garrison orders=%d (want FORTIFY)\n",
                  g->orders);
          return fail("idle Regular on captured human capital should fortify");
        }
        if (g->x != 5 || g->y != 5) {
          return fail("capital garrison Regular should stay on Jamestown");
        }
      }
      /* Already fortified with moves restored → stay, do not hunt away. */
      {
        ColonizeUnit* g = units_get(&units, cap_garrison);
        if (!g) {
          return fail("capital stay setup");
        }
        g->orders = UNITS_ORDER_FORTIFIED;
        g->moves_left = 2 * UNITS_MP_PER_TILE;
        g->goto_x = -1;
        g->goto_y = -1;
        /* Distant human prey that would otherwise attract a hunter. */
        const int bait = units_spawn_allow_stack(&units, ty_soldier, 12, 5);
        if (bait < 0) {
          return fail("capital stay bait spawn");
        }
        {
          ColonizeUnit* b = units_get(&units, bait);
          if (b) {
            b->nation_id = 0;
            b->moves_left = 0;
          }
        }
        ai_king_nation_turn(&ctx);
        g = units_get(&units, cap_garrison);
        if (!g || !g->active) {
          return fail("fortified capital garrison should remain active");
        }
        if (g->orders != UNITS_ORDER_FORTIFIED) {
          fprintf(stderr, "unit_ai_king: capital stay orders=%d (want FORTIFIED)\n",
                  g->orders);
          return fail("already-FORTIFIED Regular on capital must stay garrisoned");
        }
        if (g->x != 5 || g->y != 5) {
          return fail("already-FORTIFIED capital Regular must not leave tile");
        }
      }
      colonies.colonies[0].nation_id = 0;
    }
  }

  /*
   * Dragoon / Cont. Cav garrison fallback (Colonization.pdf Defending a Colony;
   * king_ref thin multi-garrison cap 2): when no Regular is available, fortify
   * one Dragoon or Cont. Cav after capture / idle on crown colony; second cavalry
   * with moves may join. Cont. Army is not a cavalry fallback.
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
          u->orders = UNITS_ORDER_NONE;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        if (abs(u->x - 5) <= 1 && abs(u->y - 5) <= 1) {
          u->x = 1;
          u->y = 14;
        }
      }
    }
    /* Capture: crown Dragoon alone on human colony (no Regular). */
    const int drag_cap = units_spawn_allow_stack(&units, ty_dragoon, 5, 5);
    if (drag_cap < 0) {
      return fail("Dragoon capture garrison setup should spawn Dragoon");
    }
    {
      ColonizeUnit* d = units_get(&units, drag_cap);
      if (!d) {
        return fail("Dragoon capture garrison unit lookup");
      }
      d->nation_id = 1;
      d->moves_left = 0;
      d->orders = UNITS_ORDER_NONE;
      d->goto_x = -1;
      d->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    if (colonies.colonies[0].nation_id != 1) {
      return fail("Dragoon on human colony should colonies_capture");
    }
    {
      const ColonizeUnit* d = units_get_const(&units, drag_cap);
      if (!d || !d->active) {
        return fail("Dragoon capturer should remain active");
      }
      if (d->orders != UNITS_ORDER_FORTIFY) {
        fprintf(stderr, "unit_ai_king: Dragoon capture orders=%d (want FORTIFY)\n",
                d->orders);
        return fail("capture with no Regular should fortify one Dragoon");
      }
      if (d->x != 5 || d->y != 5) {
        return fail("fortified Dragoon should stay on captured colony");
      }
    }
    /* Idle Cont. Cav on crown colony (no Regular) → fortify one. */
    {
      ColonizeColony* crown_col = &colonies.colonies[3];
      crown_col->id = 3;
      crown_col->active = true;
      crown_col->nation_id = 1;
      crown_col->x = 8;
      crown_col->y = 8;
      crown_col->population = 2;
      crown_col->colonist_count = 1;
      if (colonies.colony_count < 4) {
        colonies.colony_count = 4;
      }
      map.terrain[8 * 16 + 8] = 1;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 8 && u->y == 8) {
            u->x = 1;
            u->y = 1;
            u->orders = UNITS_ORDER_NONE;
          }
        }
        if (u->active && u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          if (abs(u->x - 8) <= 1 && abs(u->y - 8) <= 1) {
            u->x = 1;
            u->y = 1;
          }
        }
      }
      const int cav_id = units_spawn_allow_stack(&units, ty_cont_cav, 8, 8);
      if (cav_id < 0) {
        return fail("Cont. Cav idle garrison setup should spawn Cont. Cav");
      }
      {
        ColonizeUnit* cav = units_get(&units, cav_id);
        if (!cav) {
          return fail("Cont. Cav idle garrison unit lookup");
        }
        cav->nation_id = 1;
        cav->moves_left = 2 * UNITS_MP_PER_TILE;
        cav->orders = UNITS_ORDER_NONE;
        cav->goto_x = -1;
        cav->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* cav = units_get_const(&units, cav_id);
        if (!cav || !cav->active) {
          return fail("idle Cont. Cav garrison should remain active");
        }
        if (cav->orders != UNITS_ORDER_FORTIFY) {
          fprintf(stderr, "unit_ai_king: Cont. Cav idle orders=%d (want FORTIFY)\n",
                  cav->orders);
          return fail("idle Cont. Cav on crown colony with no Regular should fortify");
        }
        if (cav->x != 8 || cav->y != 8) {
          return fail("idle Cont. Cav garrison should stay on crown colony");
        }
      }
      /* Cap-2: extra Dragoon with moves fortifies when Cont. Cav holds slot 1. */
      {
        ColonizeUnit* cav = units_get(&units, cav_id);
        if (!cav) {
          return fail("cav stack setup");
        }
        cav->orders = UNITS_ORDER_FORTIFY;
        cav->moves_left = 0;
        const int extra_d = units_spawn_allow_stack(&units, ty_dragoon, 8, 8);
        const int prey = units_spawn_allow_stack(&units, ty_soldier, 14, 8);
        if (extra_d < 0 || prey < 0) {
          return fail("cav stack extras setup spawn");
        }
        {
          ColonizeUnit* ex = units_get(&units, extra_d);
          ColonizeUnit* p = units_get(&units, prey);
          if (!ex || !p) {
            return fail("cav stack extras unit lookup");
          }
          ex->nation_id = 1;
          ex->moves_left = 1 * UNITS_MP_PER_TILE;
          ex->orders = UNITS_ORDER_NONE;
          ex->goto_x = -1;
          ex->goto_y = -1;
          p->nation_id = 0;
          p->moves_left = 0;
        }
        memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
        memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
        ai_king_nation_turn(&ctx);
        {
          const ColonizeUnit* cav2 = units_get_const(&units, cav_id);
          const ColonizeUnit* ex = units_get_const(&units, extra_d);
          if (!cav2 ||
              (cav2->orders != UNITS_ORDER_FORTIFY &&
               cav2->orders != UNITS_ORDER_FORTIFIED)) {
            return fail("cav stack: Cont. Cav garrison must stay FORTIFY");
          }
          if (!ex || !ex->active) {
            return fail("cav stack: extra Dragoon should remain active");
          }
          if (ex->orders != UNITS_ORDER_FORTIFY && ex->orders != UNITS_ORDER_FORTIFIED) {
            fprintf(stderr, "unit_ai_king: cav stack extra orders=%d (want FORTIFY cap-2)\n",
                    ex->orders);
            return fail("cav stack: second Dragoon with moves should fortify (cap 2)");
          }
        }
        int fortified = 0;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &units.units[i];
          if (!u->active || u->nation_id != 1 || u->x != 8 || u->y != 8) {
            continue;
          }
          if ((u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) &&
              (u->type_index == ty_dragoon || u->type_index == ty_cont_cav)) {
            fortified++;
          }
        }
        if (fortified != 2) {
          fprintf(stderr, "unit_ai_king: cav stack fortified=%d (want 2)\n", fortified);
          return fail("cav stack should fortify two cavalry when second has moves");
        }
      }
      crown_col->active = false;
    }
    /* Prefer Regular over Dragoon for first slot; cap-2 allows second with moves. */
    {
      ColonizeColony* crown_col = &colonies.colonies[3];
      crown_col->active = true;
      crown_col->nation_id = 1;
      crown_col->x = 8;
      crown_col->y = 8;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active) {
          continue;
        }
        if (u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 8 && u->y == 8) {
            u->x = 1;
            u->y = 1;
            u->orders = UNITS_ORDER_NONE;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          if (abs(u->x - 8) <= 1 && abs(u->y - 8) <= 1) {
            u->x = 1;
            u->y = 14;
          }
        }
      }
      const int reg_id = units_spawn_allow_stack(&units, ty_regular, 8, 8);
      const int drag_id = units_spawn_allow_stack(&units, ty_dragoon, 8, 8);
      if (reg_id < 0 || drag_id < 0) {
        return fail("Regular-prefer garrison setup spawn");
      }
      {
        ColonizeUnit* r = units_get(&units, reg_id);
        ColonizeUnit* d = units_get(&units, drag_id);
        if (!r || !d) {
          return fail("Regular-prefer garrison unit lookup");
        }
        r->nation_id = 1;
        r->moves_left = 2 * UNITS_MP_PER_TILE;
        r->orders = UNITS_ORDER_NONE;
        r->goto_x = -1;
        r->goto_y = -1;
        d->nation_id = 1;
        d->moves_left = 2 * UNITS_MP_PER_TILE;
        d->orders = UNITS_ORDER_NONE;
        d->goto_x = -1;
        d->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* r = units_get_const(&units, reg_id);
        const ColonizeUnit* d = units_get_const(&units, drag_id);
        if (!r || r->orders != UNITS_ORDER_FORTIFY) {
          return fail("when Regular present, prefer Regular for garrison fortify");
        }
        if (!d || (d->orders != UNITS_ORDER_FORTIFY && d->orders != UNITS_ORDER_FORTIFIED)) {
          return fail("cap-2: Dragoon with moves should fortify as second garrison");
        }
      }
      crown_col->active = false;
    }
    colonies.colonies[0].nation_id = 0;
  }

  /*
   * Artillery after capture (Euro pattern): crown Artillery on human colony →
   * colonies_capture then UNITS_ORDER_FORTIFY on that Artillery (Colonization.pdf
   * fortify defense / euro_unit_act Artillery fortify after siege). Idle
   * Artillery on crown colony with no adjacent foe also FORTIFY.
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
          u->orders = UNITS_ORDER_NONE;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        if (abs(u->x - 5) <= 1 && abs(u->y - 5) <= 1) {
          u->x = 1;
          u->y = 14;
        }
      }
    }
    const int art_cap = units_spawn_allow_stack(&units, ty_artillery, 5, 5);
    if (art_cap < 0) {
      return fail("Artillery after-capture setup should spawn Artillery on colony");
    }
    {
      ColonizeUnit* art = units_get(&units, art_cap);
      if (!art) {
        return fail("Artillery after-capture unit lookup");
      }
      art->nation_id = 1;
      art->moves_left = 0;
      art->orders = UNITS_ORDER_NONE;
      art->goto_x = -1;
      art->goto_y = -1;
    }
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    if (colonies.colonies[0].nation_id != 1) {
      return fail("Artillery on human colony should colonies_capture");
    }
    {
      const ColonizeUnit* art = units_get_const(&units, art_cap);
      if (!art || !art->active) {
        return fail("Artillery capturer should remain active");
      }
      if (art->orders != UNITS_ORDER_FORTIFY) {
        fprintf(stderr, "unit_ai_king: Artillery after-capture orders=%d (want FORTIFY)\n",
                art->orders);
        return fail("Artillery on newly captured colony should FORTIFY (Euro pattern)");
      }
      if (art->x != 5 || art->y != 5) {
        return fail("Artillery after-capture should stay on colony tile");
      }
    }
    /* Idle Artillery on crown colony (moves>0, no adjacent foe) → FORTIFY.
     * Also: already-FORTIFIED Artillery on own colony stays put (Euro pattern;
     * same stay gate as Regular garrison). */
    {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && u->id != art_cap) {
          u->moves_left = 0;
        }
      }
      ColonizeUnit* art = units_get(&units, art_cap);
      if (!art) {
        return fail("Artillery idle fortify setup");
      }
      art->orders = UNITS_ORDER_NONE;
      art->moves_left = 2 * UNITS_MP_PER_TILE;
      art->goto_x = -1;
      art->goto_y = -1;
      colonies.colonies[0].nation_id = 1;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      ai_king_nation_turn(&ctx);
      art = units_get(&units, art_cap);
      if (!art || !art->active) {
        return fail("idle Artillery on crown colony should remain active");
      }
      if (art->orders != UNITS_ORDER_FORTIFY) {
        fprintf(stderr, "unit_ai_king: idle Artillery orders=%d (want FORTIFY)\n",
                art->orders);
        return fail("idle Artillery on crown colony should FORTIFY (Euro pattern)");
      }
      if (art->x != 5 || art->y != 5) {
        return fail("idle Artillery should stay on crown colony");
      }
      /* Already FORTIFIED: stay on crown colony (do not wake to hunt). */
      art = units_get(&units, art_cap);
      if (!art) {
        return fail("Artillery fortified-stay setup");
      }
      art->orders = UNITS_ORDER_FORTIFIED;
      art->moves_left = 2 * UNITS_MP_PER_TILE;
      art->goto_x = -1;
      art->goto_y = -1;
      /* Bait: distant fortified human colony so hunt would otherwise leave. */
      colonies.building_type_count = 1;
      snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
               "Stockade");
      ColonizeColony* bait = &colonies.colonies[2];
      bait->id = 2;
      bait->active = true;
      bait->nation_id = 0;
      bait->x = 14;
      bait->y = 5;
      bait->population = 2;
      bait->colonist_count = 2;
      bait->has_building[0] = true;
      snprintf(bait->name, sizeof(bait->name), "FortBait");
      if (colonies.colony_count < 3) {
        colonies.colony_count = 3;
      }
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && u->id != art_cap) {
          u->moves_left = 0;
        }
      }
      ai_king_nation_turn(&ctx);
      art = units_get(&units, art_cap);
      if (!art || !art->active) {
        return fail("fortified Artillery stay should remain active");
      }
      if (art->orders != UNITS_ORDER_FORTIFIED) {
        fprintf(stderr, "unit_ai_king: Artillery stay orders=%d (want FORTIFIED)\n",
                art->orders);
        return fail("already-FORTIFIED Artillery on crown colony must stay");
      }
      if (art->x != 5 || art->y != 5) {
        return fail("already-FORTIFIED Artillery must not leave crown colony");
      }
      bait->active = false;
      bait->has_building[0] = false;
    }
    colonies.colonies[0].nation_id = 0;
  }

  /*
   * Thin Artillery siege: fortified target + Artillery type in pool + force[3]>0
   * (no MoW/Regular/Dragoon pools) → wave prefers Artillery spawn.
   * Deep multi-step siege scoring PARKED.
   */
  {
    /*
     * FUN_43f7_0982 MoW-pool gate: force[2]==0 → +1 only while the crown has
     * no Man-O-War alive, and nothing lands this beat (73990-73994).
     */
    colonies.building_type_count = 1;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Stockade");
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].has_building[0] = true;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.expeditionary_force[3] = 2; /* Artillery only, no MoW */
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1 && units_is_sea(&units, u->id)) {
        u->active = false;
      } else if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    {
      const int art_before = count_nation_land(&units, 1);
      ai_king_nation_turn(&ctx);
      if (col1.head.expeditionary_force[2] != 1 || col1.head.expeditionary_force[3] != 2) {
        fprintf(stderr, "unit_ai_king: pools after MoW-empty beat %u/%u/%u/%u\n",
                (unsigned)col1.head.expeditionary_force[0], (unsigned)col1.head.expeditionary_force[1],
                (unsigned)col1.head.expeditionary_force[2], (unsigned)col1.head.expeditionary_force[3]);
        return fail("0982: empty MoW pool must regrow +1 and land nothing");
      }
      if (count_nation_land(&units, 1) != art_before || count_nation_sea(&units, 1) != 0) {
        return fail("0982: no landing while the MoW pool is empty");
      }
    }
    /*
     * Next beat: MoW pool 1 → ship + landing. Dragoon/Artillery share is
     * capped at max(1, garrison>>3) (=1 here), Regulars fill the rest — none
     * left, so exactly one Artillery lands (74192-74199).
     */
    {
      int art_units_before = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && u->type_index == ty_artillery) {
          art_units_before++;
        }
        if (u->active && u->nation_id == 1) {
          units.units[i].moves_left = 0;
        }
      }
      ai_king_nation_turn(&ctx);
      int art_units_after = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && u->type_index == ty_artillery) {
          art_units_after++;
        }
      }
      if (art_units_after != art_units_before + 1) {
        fprintf(stderr, "unit_ai_king: Artillery %d→%d (want +1)\n", art_units_before,
                art_units_after);
        return fail("0982: one Artillery lands under the cap");
      }
      if (col1.head.expeditionary_force[3] != 1 || col1.head.expeditionary_force[2] != 0) {
        return fail("0982: landing drains force[3] and the MoW pool");
      }
      if (count_nation_sea(&units, 1) != 1) {
        return fail("0982: exactly one Man-O-War per beat");
      }
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1 && units_is_sea(&units, u->id)) {
        u->active = false;
      }
    }
    /* Artillery hunt prefer fortified: closer unfortified unit vs fortified colony. */
    colonies.colonies[0].nation_id = 0;
    colonies.building_type_count = 1;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Stockade");
    colonies.colonies[0].has_building[0] = true;
    if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
      return fail("Artillery hunt setup requires fortified human colony");
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
        /* Move prior crown stacks off the fortified colony so hunt is clean. */
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    const int decoy_id = units_spawn_allow_stack(&units, ty_soldier, 7, 5);
    const int art_id = units_spawn_allow_stack(&units, ty_artillery, 9, 5);
    if (decoy_id < 0 || art_id < 0) {
      return fail("Artillery hunt setup should spawn decoy + Artillery");
    }
    {
      ColonizeUnit* decoy = units_get(&units, decoy_id);
      ColonizeUnit* art = units_get(&units, art_id);
      if (!decoy || !art) {
        return fail("Artillery hunt unit lookup");
      }
      decoy->nation_id = 0;
      decoy->moves_left = 0;
      art->nation_id = 1;
      art->moves_left = 1 * UNITS_MP_PER_TILE;
      art->orders = UNITS_ORDER_NONE;
      art->goto_x = -1;
      art->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* art = units_get_const(&units, art_id);
      if (!art || !art->active) {
        return fail("Artillery hunter should remain active");
      }
      if (art->orders != UNITS_ORDER_AI_MOVE || art->goto_x != 5 || art->goto_y != 5) {
        fprintf(stderr, "unit_ai_king: Artillery goto=(%d,%d) orders=%d (want fortified 5,5)\n",
                art->goto_x, art->goto_y, art->orders);
        return fail("Artillery should prefer fortified human colony over nearer unit");
      }
    }
    /*
     * Artillery siege tighten: adjacent unfortified human colony must not
     * override a farther fortified hunt target.
     */
    {
      ColonizeColony* soft = &colonies.colonies[3];
      soft->id = 3;
      soft->active = true;
      soft->nation_id = 0;
      soft->x = 10;
      soft->y = 5; /* adjacent east of Artillery at (9,5) */
      soft->population = 2;
      soft->colonist_count = 2;
      soft->has_building[0] = false;
      if (colonies.colony_count < 4) {
        colonies.colony_count = 4;
      }
      colonies.colonies[0].nation_id = 0;
      colonies.colonies[0].has_building[0] = true;
      if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
        return fail("Artillery adj-fort tighten requires fortified Jamestown");
      }
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1) {
          u->moves_left = 0;
          if ((u->x == 5 && u->y == 5) || (u->x == 10 && u->y == 5)) {
            u->x = 1;
            u->y = 1;
          }
        }
      }
      {
        ColonizeUnit* art = units_get(&units, art_id);
        if (!art || !art->active) {
          return fail("Artillery adj-fort setup needs live Artillery");
        }
        art->x = 9;
        art->y = 5;
        art->moves_left = 1 * UNITS_MP_PER_TILE;
        art->orders = UNITS_ORDER_NONE;
        art->goto_x = -1;
        art->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* art = units_get_const(&units, art_id);
        if (!art || !art->active) {
          return fail("Artillery adj-fort hunter should remain active");
        }
        if (art->orders != UNITS_ORDER_AI_MOVE || art->goto_x != 5 || art->goto_y != 5) {
          fprintf(stderr,
                  "unit_ai_king: Artillery adj-fort goto=(%d,%d) orders=%d "
                  "(want fortified 5,5 not soft 10,5)\n",
                  art->goto_x, art->goto_y, art->orders);
          return fail("Artillery must not let adjacent unfortified override fortified hunt");
        }
      }
      soft->active = false;
    }
    colonies.colonies[0].has_building[0] = false; /* clear fort for later */
  }

  /*
   * Dragoon open-land bias: when Artillery type exists, prefer farther open
   * human land unit over nearer fortified colony (Artillery owns siege).
   * Deep role-split scoring PARKED.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.building_type_count = 1;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Stockade");
    colonies.colonies[0].has_building[0] = true;
    if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
      return fail("Dragoon open-land setup requires fortified human colony");
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    /* Park crown; sweep human land units off the probe corridor. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        /* Keep map clear so Dragoon does not bump into leftovers. */
        u->x = 1;
        u->y = 14;
        u->moves_left = 0;
      }
    }
    /* Nearer fortified colony (5,5); farther open Soldier (14,5); Dragoon at (8,5). */
    const int open_id = units_spawn_allow_stack(&units, ty_soldier, 14, 5);
    const int drg_id = units_spawn_allow_stack(&units, ty_dragoon, 8, 5);
    if (open_id < 0 || drg_id < 0) {
      return fail("Dragoon open-land setup should spawn open Soldier + Dragoon");
    }
    {
      ColonizeUnit* openu = units_get(&units, open_id);
      ColonizeUnit* drg = units_get(&units, drg_id);
      if (!openu || !drg) {
        return fail("Dragoon open-land unit lookup");
      }
      openu->nation_id = 0;
      openu->moves_left = 0;
      drg->nation_id = 1;
      drg->moves_left = 1 * UNITS_MP_PER_TILE;
      drg->orders = UNITS_ORDER_NONE;
      drg->goto_x = -1;
      drg->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* drg = units_get_const(&units, drg_id);
      if (!drg || !drg->active) {
        return fail("Dragoon hunter should remain active");
      }
      if (drg->orders != UNITS_ORDER_AI_MOVE || drg->goto_x != 14 || drg->goto_y != 5) {
        fprintf(stderr,
                "unit_ai_king: Dragoon goto=(%d,%d) orders=%d (want open unit 14,5 not fort 5,5)\n",
                drg->goto_x, drg->goto_y, drg->orders);
        return fail("Dragoon should prefer open land unit over nearer fortified colony");
      }
    }
    colonies.colonies[0].has_building[0] = false;
  }

  /*
   * Cont. Cav open-land bias (same Dragoon role when Artillery exists):
   * prefer farther open human land unit over nearer fortified colony.
   * Cont. Army stays nearest — Cont. Cav only. Deep role-split PARKED.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.building_type_count = 1;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Stockade");
    colonies.colonies[0].has_building[0] = true;
    if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
      return fail("Cont. Cav open-land setup requires fortified human colony");
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        u->x = 1;
        u->y = 14;
        u->moves_left = 0;
      }
    }
    const int open_id = units_spawn_allow_stack(&units, ty_soldier, 14, 5);
    const int cav_id = units_spawn_allow_stack(&units, ty_cont_cav, 8, 5);
    if (open_id < 0 || cav_id < 0) {
      return fail("Cont. Cav open-land setup should spawn open Soldier + Cont. Cav");
    }
    {
      ColonizeUnit* openu = units_get(&units, open_id);
      ColonizeUnit* cav = units_get(&units, cav_id);
      if (!openu || !cav) {
        return fail("Cont. Cav open-land unit lookup");
      }
      openu->nation_id = 0;
      openu->moves_left = 0;
      cav->nation_id = 1; /* crown Cont. Cav hunter */
      cav->moves_left = 1 * UNITS_MP_PER_TILE;
      cav->orders = UNITS_ORDER_NONE;
      cav->goto_x = -1;
      cav->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* cav = units_get_const(&units, cav_id);
      if (!cav || !cav->active) {
        return fail("Cont. Cav hunter should remain active");
      }
      if (cav->orders != UNITS_ORDER_AI_MOVE || cav->goto_x != 14 || cav->goto_y != 5) {
        fprintf(stderr,
                "unit_ai_king: Cont. Cav goto=(%d,%d) orders=%d (want open 14,5 not fort 5,5)\n",
                cav->goto_x, cav->goto_y, cav->orders);
        return fail("Cont. Cav should prefer open land unit over nearer fortified colony");
      }
    }
    /*
     * Cont. Army stays nearest (no open-land bias): same geometry → fortified
     * colony (5,5), not open Soldier (14,5). Cont. Cav only above.
     * Restore human ownership — prior Cont. Cav beat may have captured via a
     * leftover crown stack on the tile (capture runs at 0 moves).
     */
    {
      colonies.colonies[0].nation_id = 0;
      colonies.colonies[0].has_building[0] = true;
      if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
        return fail("Cont. Army open-land negative requires fortified human colony");
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active) {
          continue;
        }
        if (u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 1;
          u->y = 14;
          u->moves_left = 0;
        }
      }
      const int open_id2 = units_spawn_allow_stack(&units, ty_soldier, 14, 5);
      const int army_id = units_spawn_allow_stack(&units, ty_cont_army, 8, 5);
      if (open_id2 < 0 || army_id < 0) {
        return fail("Cont. Army open-land negative setup should spawn open Soldier + Cont. Army");
      }
      {
        ColonizeUnit* openu = units_get(&units, open_id2);
        ColonizeUnit* army = units_get(&units, army_id);
        if (!openu || !army) {
          return fail("Cont. Army open-land negative unit lookup");
        }
        openu->nation_id = 0;
        openu->moves_left = 0;
        army->nation_id = 1;
        army->moves_left = 1 * UNITS_MP_PER_TILE;
        army->orders = UNITS_ORDER_NONE;
        army->goto_x = -1;
        army->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* army = units_get_const(&units, army_id);
        if (!army || !army->active) {
          return fail("Cont. Army hunter should remain active");
        }
        if (army->orders != UNITS_ORDER_AI_MOVE || army->goto_x != 5 || army->goto_y != 5) {
          fprintf(stderr,
                  "unit_ai_king: Cont. Army goto=(%d,%d) orders=%d (want fort 5,5 not open 14,5)\n",
                  army->goto_x, army->goto_y, army->orders);
          return fail("Cont. Army should stay nearest (fort colony), not open-land bias");
        }
      }
    }
    colonies.colonies[0].has_building[0] = false;
  }

  /*
   * Wartime MoW with cargo → AI_SAIL toward water adjacent to human colony.
   * Ship at (2,5); coast water west of colony is (4,5); step east toward coast.
   * Decoy weak port draws 06a6 irregulars so the coastal colony stays human.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].population = 8; /* stronger — not 06a6 pick */
    ColonizeColony* decoy_port = &colonies.colonies[2];
    decoy_port->id = 2;
    decoy_port->active = true;
    decoy_port->nation_id = 0;
    decoy_port->x = 14;
    decoy_port->y = 14;
    decoy_port->population = 1;
    decoy_port->colonist_count = 1;
    if (colonies.colony_count < 3) {
      colonies.colony_count = 3;
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    /* Ocean corridor (2,5)-(4,5) for MoW sail. */
    map.terrain[5 * 16 + 2] = 25;
    map.terrain[5 * 16 + 3] = 25;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
        /* Off human colonies — 0-move capture would clear MoW coast targets. */
        if ((u->x == 5 && u->y == 5) || (u->x == 14 && u->y == 14)) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    const int mow_id = units_spawn_allow_stack(&units, ty_mow, 2, 5);
    const int pax_id = units_spawn_allow_stack(&units, ty_regular, 2, 5);
    if (mow_id < 0 || pax_id < 0) {
      return fail("MoW sail setup should spawn Man-O-War + Regular");
    }
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      ColonizeUnit* pax = units_get(&units, pax_id);
      if (!mow || !pax) {
        return fail("MoW sail unit lookup");
      }
      mow->nation_id = 1;
      mow->moves_left = 4 * UNITS_MP_PER_TILE;
      mow->orders = UNITS_ORDER_NONE;
      mow->goto_x = -1;
      mow->goto_y = -1;
      pax->nation_id = 1;
      if (!units_board_stacked(&units, pax_id, mow_id)) {
        return fail("MoW sail should board Regular as cargo");
      }
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* mow = units_get_const(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("MoW with cargo should remain active");
      }
      if (mow->orders != UNITS_ORDER_AI_SAIL || mow->goto_x != 4 || mow->goto_y != 5) {
        fprintf(stderr, "unit_ai_king: MoW goto=(%d,%d) orders=%d (want AI_SAIL→4,5)\n",
                mow->goto_x, mow->goto_y, mow->orders);
        return fail("MoW with cargo should AI_SAIL toward water adjacent to human colony");
      }
      if (mow->x < 3) {
        fprintf(stderr, "unit_ai_king: MoW pos=(%d,%d) (want step toward coast)\n", mow->x,
                mow->y);
        return fail("MoW with cargo should step toward human coast water");
      }
    }
    /* Place MoW on coast water adjacent to human colony → unload onto adjacent
     * foundable/coastal land (colony tile is last-resort fallback only —
     * king_ref 2026-08-24 adjacent-first). Single passenger + moves≥1 → unload
     * that one (multi-unload capped by cargo). */
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      if (!mow || !mow->active || mow->cargo_count <= 0) {
        return fail("MoW unload setup needs MoW still carrying cargo");
      }
      mow->x = 4;
      mow->y = 5;
      mow->moves_left = 2 * UNITS_MP_PER_TILE;
      mow->orders = UNITS_ORDER_NONE;
      /* Ensure colony tile is free of blocking foreign units for unload. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->x == 5 && u->y == 5 && u->id != mow_id &&
            u->nation_id != 1) {
          u->x = 1;
          u->y = 1;
        }
      }
      /* Soft coastal land (4,4)/(4,6) must win over colony tile (5,5). */
      map.terrain[4 * 16 + 4] = 1;
      map.terrain[6 * 16 + 4] = 1;
      const int cargo_before = mow->cargo_count;
      const int expect_unload =
          cargo_before < mow->moves_left ? cargo_before : mow->moves_left;
      int ashore_before = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && !units_is_sea(&units, u->id) &&
            u->aboard_ship_id < 0) {
          ashore_before++;
        }
      }
      colonies.colonies[0].nation_id = 0;
      ai_king_nation_turn(&ctx);
      mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("MoW should remain after coastal unload");
      }
      if (mow->cargo_count != cargo_before - expect_unload) {
        fprintf(stderr, "unit_ai_king: MoW cargo after unload %d (want %d)\n",
                mow->cargo_count, cargo_before - expect_unload);
        return fail("MoW adjacent to human colony coast should unload ≤moves cargo");
      }
      {
        int ashore_after = 0;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &units.units[i];
          if (u->active && u->nation_id == 1 && !units_is_sea(&units, u->id) &&
              u->aboard_ship_id < 0) {
            ashore_after++;
          }
        }
        if (ashore_after < ashore_before + expect_unload) {
          return fail("MoW coastal unload should place crown land ashore");
        }
      }
      /* Adjacent-first: passenger lands on soft coast, not the colony tile. */
      {
        int on_adj = 0;
        int on_colony = 0;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &units.units[i];
          if (!u->active || u->nation_id != 1 || units_is_sea(&units, u->id)) {
            continue;
          }
          if (u->type_index != ty_regular || u->aboard_ship_id >= 0) {
            continue;
          }
          if (u->x == 5 && u->y == 5) {
            on_colony = 1;
          } else if ((u->x == 4 && u->y == 4) || (u->x == 4 && u->y == 6) ||
                     (u->x == 5 && u->y == 4) || (u->x == 5 && u->y == 6)) {
            on_adj = 1;
          }
        }
        if (!on_adj) {
          return fail("MoW unload should prefer adjacent coastal land over colony tile");
        }
        (void)on_colony; /* 06a6 hunt may still walk onto colony same beat */
      }
    }
    /*
     * Multi-unload deepen (MoW×6 seize): board 3 Regulars, moves_left=2 * UNITS_MP_PER_TILE →
     * unload exactly 2 this beat (cap by moves; leftover 1 stays aboard).
     * Cite: fandom man-o-war×6 / units_unload_passenger; no invent.
     */
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("multi-unload setup needs live Man-O-War");
      }
      while (mow->cargo_count > 0) {
        const int cid = mow->cargo_ids[0];
        ColonizeUnit* p = units_get(&units, cid);
        if (p) {
          p->aboard_ship_id = -1;
          p->active = false;
        }
        mow->cargo_count = 0;
      }
      for (int k = 0; k < 3; ++k) {
        const int pid = units_spawn_allow_stack(&units, ty_regular, 4, 5);
        if (pid < 0) {
          return fail("multi-unload setup should spawn Regular cargo");
        }
        ColonizeUnit* p = units_get(&units, pid);
        if (!p) {
          return fail("multi-unload Regular lookup");
        }
        p->nation_id = 1;
        if (!units_board_stacked(&units, pid, mow_id)) {
          return fail("multi-unload should board Regular");
        }
      }
      mow = units_get(&units, mow_id);
      if (!mow || mow->cargo_count != 3) {
        return fail("multi-unload setup wants cargo_count 3");
      }
      mow->x = 4;
      mow->y = 5;
      mow->moves_left = 2 * UNITS_MP_PER_TILE;
      mow->orders = UNITS_ORDER_NONE;
      map.terrain[4 * 16 + 4] = 1;
      map.terrain[6 * 16 + 4] = 1;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active || u->id == mow_id) {
          continue;
        }
        if (u->nation_id == 1 && u->aboard_ship_id < 0) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 1;
          u->y = 14;
          u->moves_left = 0;
        }
      }
      colonies.colonies[0].nation_id = 0;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      int ashore_before = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && !units_is_sea(&units, u->id) &&
            u->aboard_ship_id < 0) {
          ashore_before++;
        }
      }
      ai_king_nation_turn(&ctx);
      mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("MoW should remain after multi-unload");
      }
      if (mow->cargo_count != 1) {
        fprintf(stderr, "unit_ai_king: multi-unload cargo=%d (want 1 leftover)\n",
                mow->cargo_count);
        return fail("MoW multi-unload should dump 2 of 3 when moves_left=2 * UNITS_MP_PER_TILE");
      }
      int ashore_after = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && !units_is_sea(&units, u->id) &&
            u->aboard_ship_id < 0) {
          ashore_after++;
        }
      }
      if (ashore_after < ashore_before + 2) {
        fprintf(stderr, "unit_ai_king: multi-unload ashore %d→%d (want +2)\n",
                ashore_before, ashore_after);
        return fail("MoW multi-unload should place 2 crown land ashore");
      }
      /* bugs.md 406: NO same-beat seize — DOS lands adjacent and walks in
       * only on a later activation with moves restored. */
      if (colonies.colonies[0].nation_id != 0) {
        return fail("MoW multi-unload must NOT seize the colony on the landing beat");
      }
      /* Next crown activation: refresh the landed Regulars' moves and run the
       * king again — the war-act walk now enters the undefended colony and
       * captures it (the DOS later-activation capture). */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && !units_is_sea(&units, u->id) &&
            u->aboard_ship_id < 0) {
          u->moves_left = UNITS_MP_PER_TILE;
        }
      }
      ai_king_nation_turn(&ctx);
      if (colonies.colonies[0].nation_id != 1) {
        return fail("MoW landing should capture the colony on the NEXT activation");
      }
      {
        /* bugs.md 406: the capture now lands on the next activation via the
         * war-act walk, so the garrison on the tile may be whichever crown
         * land unit walked in first — assert a fortified crown garrison,
         * not specifically the freshly landed Regulars. */
        int fortified = 0;
        int crown_on = 0;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &units.units[i];
          if (!u->active || u->nation_id != 1 || units_is_sea(&units, u->id)) {
            continue;
          }
          if (u->x != 5 || u->y != 5 || u->aboard_ship_id >= 0) {
            continue;
          }
          crown_on++;
          if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
            fortified++;
          }
        }
        if (crown_on < 1) {
          return fail("capture should leave a crown garrison on the colony tile");
        }
        if (fortified < 1) {
          return fail("capture should fortify at least one garrison unit");
        }
      }
    }
    /*
     * Full unload + moves left → AI_SAIL toward *next* human coast (skip the
     * port just served). Ship at (4,5) next to Jamestown; second human port
     * with water at (13,14); cargo=1 moves=3 → unload then sail toward 13,14.
     */
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("full-unload sail setup needs live Man-O-War");
      }
      while (mow->cargo_count > 0) {
        const int cid = mow->cargo_ids[0];
        ColonizeUnit* p = units_get(&units, cid);
        if (p) {
          p->aboard_ship_id = -1;
          p->active = false;
        }
        mow->cargo_count = 0;
      }
      /* Water west of decoy port (14,14) — next human coast target. */
      map.terrain[14 * 16 + 13] = 25;
      decoy_port->active = true;
      decoy_port->nation_id = 0;
      decoy_port->population = 1;
      colonies.colonies[0].nation_id = 0;
      colonies.colonies[0].population = 8;
      const int pax2 = units_spawn_allow_stack(&units, ty_regular, 4, 5);
      if (pax2 < 0) {
        return fail("full-unload sail setup should spawn Regular cargo");
      }
      {
        ColonizeUnit* p = units_get(&units, pax2);
        if (!p) {
          return fail("full-unload sail Regular lookup");
        }
        p->nation_id = 1;
        if (!units_board_stacked(&units, pax2, mow_id)) {
          return fail("full-unload sail should board Regular");
        }
      }
      mow = units_get(&units, mow_id);
      mow->x = 4;
      mow->y = 5;
      mow->moves_left = 3 * UNITS_MP_PER_TILE;
      mow->orders = UNITS_ORDER_NONE;
      mow->goto_x = -1;
      mow->goto_y = -1;
      map.terrain[4 * 16 + 4] = 1;
      map.terrain[6 * 16 + 4] = 1;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active || u->id == mow_id || u->id == pax2) {
          continue;
        }
        if (u->nation_id == 1 && u->aboard_ship_id < 0) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 1;
          u->y = 14;
          u->moves_left = 0;
        }
      }
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      ai_king_nation_turn(&ctx);
      mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("MoW should remain after full unload + sail");
      }
      if (mow->cargo_count != 0) {
        return fail("full unload should empty MoW hold before sail");
      }
      if (mow->orders != UNITS_ORDER_AI_SAIL || mow->goto_x != 13 || mow->goto_y != 14) {
        fprintf(stderr,
                "unit_ai_king: post-full-unload MoW goto=(%d,%d) orders=%d "
                "(want AI_SAIL→13,14 next coast)\n",
                mow->goto_x, mow->goto_y, mow->orders);
        return fail("after full unload with moves left MoW should AI_SAIL to next human coast");
      }
    }
    /*
     * Coast-adjacent unload prefers soft coastal/foundable land over the human
     * colony tile (king_ref 2026-08-24 adjacent-first; colony is fallback only).
     * Ship already on coast water (4,5) next to colony (5,5); soft land at
     * (4,4)/(4,6) must win. Sail-toward-coast from further out is covered by
     * the MoW AI_SAIL case above.
     */
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("post-sail unload setup needs live Man-O-War");
      }
      while (mow->cargo_count > 0) {
        const int cid = mow->cargo_ids[0];
        ColonizeUnit* p = units_get(&units, cid);
        if (p) {
          p->aboard_ship_id = -1;
          p->active = false;
        }
        mow->cargo_count = 0;
      }
      decoy_port->active = false;
      colonies.colonies[0].nation_id = 0;
      colonies.colonies[0].population = 8;
      map.terrain[5 * 16 + 3] = 25;
      map.terrain[5 * 16 + 4] = 25;
      map.terrain[4 * 16 + 4] = 1;
      map.terrain[6 * 16 + 4] = 1;
      mow = units_get(&units, mow_id);
      mow->x = 4;
      mow->y = 5;
      mow->moves_left = 3 * UNITS_MP_PER_TILE;
      mow->orders = UNITS_ORDER_NONE;
      mow->goto_x = UNITS_GOTO_NONE;
      mow->goto_y = UNITS_GOTO_NONE;
      const int pax_sail = units_spawn_allow_stack(&units, ty_regular, 4, 5);
      if (pax_sail < 0) {
        return fail("post-sail unload setup should spawn Regular cargo");
      }
      {
        ColonizeUnit* p = units_get(&units, pax_sail);
        if (!p) {
          return fail("post-sail unload Regular lookup");
        }
        p->nation_id = 1;
        if (!units_board_stacked(&units, pax_sail, mow_id)) {
          return fail("post-sail unload should board Regular");
        }
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active || u->id == mow_id || u->id == pax_sail) {
          continue;
        }
        if (u->nation_id == 1 && u->aboard_ship_id < 0) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 1;
          u->y = 14;
          u->moves_left = 0;
        }
      }
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      ai_king_nation_turn(&ctx);
      mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("MoW should remain after post-sail unload");
      }
      if (mow->cargo_count != 0) {
        fprintf(stderr, "unit_ai_king: post-sail MoW cargo=%d (want 0 after adjacent unload)\n",
                mow->cargo_count);
        return fail("after coast sail step MoW should unload when already adjacent");
      }
      {
        const ColonizeUnit* pax = units_get_const(&units, pax_sail);
        if (!pax || !pax->active || pax->aboard_ship_id >= 0) {
          return fail("post-sail unload should put Regular ashore");
        }
        if (pax->x == 5 && pax->y == 5) {
          fprintf(stderr, "unit_ai_king: post-sail pax at colony (%d,%d) "
                          "(want adjacent coastal land)\n",
                  pax->x, pax->y);
          return fail("post-sail unload should prefer adjacent coastal land over colony");
        }
        {
          const int adx = pax->x - 4;
          const int ady = pax->y - 5;
          const int adj =
              adx >= -1 && adx <= 1 && ady >= -1 && ady <= 1 && (adx != 0 || ady != 0);
          if (!adj || !map_tile_is_land(&map, pax->x, pax->y)) {
            fprintf(stderr,
                    "unit_ai_king: post-sail pax at (%d,%d) (want land adj to ship 4,5)\n",
                    pax->x, pax->y);
            return fail("post-sail unload should land on adjacent coastal land");
          }
        }
      }
      /* Same-beat seize is via 06a6 hunt (or later beat), not unload dest. */
    }
    /*
     * Dragoon coastal unload when cargo allows (no Regular in hold):
     * prefer Regular otherwise; here only Dragoon → unload Dragoon.
     * Prefer colony tile: make (5,5) the only adjacent land so soft coast
     * cannot win. Same-beat hunt may move the Dragoon after seize — assert
     * cargo drop + not-aboard + human colony captured (owner → crown).
     */
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("Dragoon unload setup needs live Man-O-War");
      }
      /* Clear remaining cargo so we can board a Dragoon alone. */
      while (mow->cargo_count > 0) {
        const int cid = mow->cargo_ids[0];
        ColonizeUnit* p = units_get(&units, cid);
        if (p) {
          p->aboard_ship_id = -1;
          p->active = false;
        }
        mow->cargo_count = 0;
      }
      const int drg_pax = units_spawn_allow_stack(&units, ty_dragoon, 4, 5);
      if (drg_pax < 0) {
        return fail("Dragoon unload setup should spawn Dragoon");
      }
      {
        ColonizeUnit* drg = units_get(&units, drg_pax);
        if (!drg) {
          return fail("Dragoon unload unit lookup");
        }
        drg->nation_id = 1;
        if (!units_board_stacked(&units, drg_pax, mow_id)) {
          return fail("Dragoon unload should board Dragoon as cargo");
        }
      }
      mow->x = 4;
      mow->y = 5;
      mow->moves_left = 2 * UNITS_MP_PER_TILE;
      mow->orders = UNITS_ORDER_NONE;
      /* Only colony tile (5,5) enterable from ship — soft coast → water. */
      map.terrain[4 * 16 + 4] = 25;
      map.terrain[6 * 16 + 4] = 25;
      map.terrain[4 * 16 + 5] = 25; /* (5,4) */
      map.terrain[6 * 16 + 5] = 25; /* (5,6) */
      map.terrain[4 * 16 + 3] = 25;
      map.terrain[5 * 16 + 3] = 25;
      map.terrain[6 * 16 + 3] = 25;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active || u->id == mow_id || u->id == drg_pax) {
          continue;
        }
        if (u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
            u->orders = UNITS_ORDER_NONE;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 1;
          u->y = 14;
          u->moves_left = 0;
        }
      }
      const int cargo_before = mow->cargo_count;
      colonies.colonies[0].nation_id = 0;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      ai_king_nation_turn(&ctx);
      mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("MoW should remain after Dragoon coastal unload");
      }
      if (mow->cargo_count != cargo_before - 1) {
        fprintf(stderr, "unit_ai_king: MoW cargo after Dragoon unload %d (want %d)\n",
                mow->cargo_count, cargo_before - 1);
        return fail("MoW with Dragoon-only cargo should unload one Dragoon");
      }
      {
        const ColonizeUnit* drg = units_get_const(&units, drg_pax);
        if (!drg || !drg->active || drg->aboard_ship_id >= 0) {
          return fail("Dragoon should be ashore after MoW unload");
        }
      }
      if (colonies.colonies[0].nation_id != 1) {
        return fail("Dragoon unload onto colony tile should seize (owner → crown)");
      }
      /* Restore corridor land for later empty-MoW patrol. */
      map.terrain[4 * 16 + 4] = 1;
      map.terrain[6 * 16 + 4] = 1;
      map.terrain[4 * 16 + 5] = 1;
      map.terrain[6 * 16 + 5] = 1;
    }
    /*
     * Idle empty MoW coastal patrol (fandom REF man-o-war → ports):
     * cargo_count==0 → AI_SAIL toward water adjacent to human colony; step
     * toward coast. Redirects existing ship only — no invent spawn.
     * Hold fill = ship capacity (cargo_ids); 160a letter cinematic Done.
     */
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("empty MoW patrol setup needs live Man-O-War");
      }
      /* Drain any remaining cargo so ship is idle-empty. */
      while (mow->cargo_count > 0) {
        const int cid = mow->cargo_ids[0];
        ColonizeUnit* p = units_get(&units, cid);
        if (p) {
          p->aboard_ship_id = -1;
          p->active = false;
        }
        mow->cargo_count = 0;
        break;
      }
      mow->x = 2;
      mow->y = 5;
      mow->moves_left = 4 * UNITS_MP_PER_TILE;
      mow->orders = UNITS_ORDER_NONE;
      mow->goto_x = -1;
      mow->goto_y = -1;
      colonies.colonies[0].nation_id = 0;
      colonies.colonies[0].population = 8;
      decoy_port->active = true;
      decoy_port->nation_id = 0;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && u->id != mow_id) {
          u->moves_left = 0;
          if ((u->x == 5 && u->y == 5) || (u->x == 14 && u->y == 14)) {
            u->x = 1;
            u->y = 1;
          }
        }
      }
      ai_king_nation_turn(&ctx);
      mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("idle empty MoW should remain active");
      }
      if (mow->orders != UNITS_ORDER_AI_SAIL || mow->goto_x != 4 || mow->goto_y != 5) {
        fprintf(stderr,
                "unit_ai_king: empty MoW goto=(%d,%d) orders=%d (want AI_SAIL→4,5)\n",
                mow->goto_x, mow->goto_y, mow->orders);
        return fail("idle empty MoW should AI_SAIL toward human coast water");
      }
      if (mow->x < 3) {
        fprintf(stderr, "unit_ai_king: empty MoW pos=(%d,%d) (want step toward coast)\n",
                mow->x, mow->y);
        return fail("idle empty MoW should step toward human coast water");
      }
    }
    decoy_port->active = false;
    colonies.colonies[0].population = 4;
  }

  /*
   * ai_king_merc_offer (FUN_43f7_2022 rebel branch) is a no-op through this
   * whole test — ctx.rng stays NULL here, and that mechanic requires a real
   * RNG (dos_rng_range(NULL,...) returns lo, but the function guards on
   * !ctx->rng and returns before rolling at all). See the dedicated seeded
   * coverage further below ("2022 rebel troop-gift purchase") for its real
   * gate/price/spawn behavior.
   */
  colonies.colonies[0].nation_id = 0;
  col1.nation[0].gold = 450;
  europe.gold = 450;
  status[0] = '\0';

  /*
   * FUN_43f7_1eca full port: units on the colony's own tile are eligible
   * (decomp walks the colony-tile unit stack, not every unit the nation
   * owns) — re-verified 2026-08-24 by reading the complete raw decomp body
   * (viceroy_unpacked.c:74910-74972) end to end: it tests only unit+0x3146
   * (raw type 1/4) and unit+0x315b (profession); it never reads unit+0x08
   * (ViceroyUnit.orders), so fortified/sentry/active state does NOT gate
   * this promote (prior test/impl both wrongly required FORTIFIED — fixed
   * this pass). Soldier -> Continental Army, Dragoon -> Continental
   * Cavalry; Regular is never touched (decomp tests raw type 1/4 only).
   * Also requires Veteran profession (UNITS_JOB_SOLDIER, DOS
   * unit+0x315b==0x15) — an ordinary armed colonist does not promote.
   * colony0 (5,5) pop=4 SoL=60 by default caps at 1 promote
   * (population*(sol-50)/50 == 0, floored to 1), too tight to prove Soldier
   * + Dragoon together, so widen pop/SoL here. Also proves the own-tile
   * gate and the Veteran-profession gate each independently block a
   * promote, and that a non-fortified Veteran on-tile unit still promotes.
   */
  col1.colony[0].population = 20;
  col1.colony[0].rebel_dividend = 70;
  col1.colony[0].rebel_divisor = 100;
  colonies.colonies[0].nation_id = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units.units[i];
    if (u->active && u->nation_id == 1) {
      u->moves_left = 0;
    }
  }
  const int sid = units_spawn_allow_stack(&units, ty_soldier, 5, 5);
  const int did = units_spawn_allow_stack(&units, ty_dragoon, 5, 5);
  const int rid = units_spawn_allow_stack(&units, ty_regular, 5, 5);
  const int unfort_id = units_spawn_allow_stack(&units, ty_soldier, 5, 5);
  const int offtile_id = units_spawn_allow_stack(&units, ty_soldier, 6, 5);
  const int plain_id = units_spawn_allow_stack(&units, ty_soldier, 5, 5);
  if (sid < 0 || did < 0 || rid < 0 || unfort_id < 0 || offtile_id < 0 || plain_id < 0) {
    return fail("1eca setup should spawn human Soldier + Dragoon + Regular probes");
  }
  {
    ColonizeUnit* su = units_get(&units, sid);
    ColonizeUnit* du = units_get(&units, did);
    ColonizeUnit* ru = units_get(&units, rid);
    ColonizeUnit* unfort = units_get(&units, unfort_id);
    ColonizeUnit* offtile = units_get(&units, offtile_id);
    ColonizeUnit* plain = units_get(&units, plain_id);
    if (!su || !du || !ru || !unfort || !offtile || !plain) {
      return fail("1eca setup unit lookup");
    }
    su->nation_id = 0;
    du->nation_id = 0;
    ru->nation_id = 0;
    unfort->nation_id = 0;
    offtile->nation_id = 0;
    plain->nation_id = 0;
    su->orders = UNITS_ORDER_FORTIFIED;
    du->orders = UNITS_ORDER_FORTIFIED;
    ru->orders = UNITS_ORDER_FORTIFIED;
    unfort->orders = UNITS_ORDER_NONE; /* on-tile, NOT fortified: must still promote */
    offtile->orders = UNITS_ORDER_FORTIFIED; /* fortified, off-tile: must stay Soldier */
    plain->orders = UNITS_ORDER_FORTIFIED;
    /*
     * FUN_43f7_1eca gates on unit+0x315b == 0x15 (UNITS_JOB_SOLDIER,
     * "Veteran Soldiers") alongside the raw type check — only Veteran-
     * status Soldier/Dragoon promote. su/du/unfort/offtile earn that so
     * the fortify-state and own-tile gates are each isolated (unfort is
     * off the fortified path entirely; offtile is fortified but on the
     * wrong tile); plain stays UNITS_JOB_NONE (an ordinary armed
     * colonist) to prove the profession gate on its own.
     */
    su->profession = UNITS_JOB_SOLDIER;
    du->profession = UNITS_JOB_SOLDIER;
    unfort->profession = UNITS_JOB_SOLDIER;
    offtile->profession = UNITS_JOB_SOLDIER;
  }
  /* bugs.md 256: 1eca is a ONCE-only mobilization gated on nation_flags bit
   * 0x08 (already consumed by the first war turn above) — re-arm it so this
   * subtest exercises the mobilization body itself. */
  col1.nation[0].nation_flags = (uint8_t)(col1.nation[0].nation_flags & ~0x08u);
  ai_king_nation_turn(&ctx);
  {
    const ColonizeUnit* su = units_get_const(&units, sid);
    const ColonizeUnit* du = units_get_const(&units, did);
    const ColonizeUnit* ru = units_get_const(&units, rid);
    const ColonizeUnit* unfort = units_get_const(&units, unfort_id);
    const ColonizeUnit* offtile = units_get_const(&units, offtile_id);
    const ColonizeUnit* plain = units_get_const(&units, plain_id);
    if (!su || !su->active || su->type_index != ty_cont_army) {
      fprintf(stderr, "unit_ai_king: Soldier type after 1eca: %d (want %d)\n",
              su ? su->type_index : -1, ty_cont_army);
      return fail("1eca should promote fortified colony-tile Soldier → Continental Army");
    }
    if (!du || !du->active || du->type_index != ty_cont_cav) {
      fprintf(stderr, "unit_ai_king: Dragoon type after 1eca: %d (want %d)\n",
              du ? du->type_index : -1, ty_cont_cav);
      return fail("1eca should promote fortified colony-tile Dragoon → Continental Cavalry");
    }
    if (!ru || !ru->active || ru->type_index != ty_regular) {
      fprintf(stderr, "unit_ai_king: Regular type after 1eca: %d (want %d)\n",
              ru ? ru->type_index : -1, ty_regular);
      return fail("1eca must never touch Regular (decomp tests only type 1/4)");
    }
    if (!unfort || !unfort->active || unfort->type_index != ty_cont_army) {
      fprintf(stderr, "unit_ai_king: unfortified Veteran Soldier type after 1eca: %d (want %d)\n",
              unfort ? unfort->type_index : -1, ty_cont_army);
      return fail("1eca must promote a non-fortified on-tile Veteran Soldier "
                   "(decomp never reads unit+0x08/orders)");
    }
    if (!offtile || !offtile->active || offtile->type_index != ty_soldier) {
      fprintf(stderr, "unit_ai_king: off-tile Soldier type after 1eca: %d (want %d)\n",
              offtile ? offtile->type_index : -1, ty_soldier);
      return fail("1eca must skip a Veteran Soldier off the colony's own tile "
                   "regardless of fortify state");
    }
    if (!plain || !plain->active || plain->type_index != ty_soldier) {
      fprintf(stderr, "unit_ai_king: non-Veteran Soldier type after 1eca: %d (want %d)\n",
              plain ? plain->type_index : -1, ty_soldier);
      return fail("1eca must skip a fortified colony-tile Soldier without Veteran profession");
    }
  }

  /*
   * FUN_43f7_1eca gate: colony SoL<=49 (decomp `0x31 < iVar1`, i.e. sol>49)
   * must not promote at all, even fortified on the colony's own tile.
   */
  col1.colony[0].rebel_dividend = 45;
  col1.colony[0].rebel_divisor = 100;
  {
    const int sol45w = ai_king_sol_percent(&ctx, 0);
    if (sol45w != 45) {
      fprintf(stderr, "unit_ai_king: unexpected SoL %d (want 45) for sub-threshold 1eca\n",
              sol45w);
      return fail("1eca sub-threshold SoL setup");
    }
  }
  colonies.colonies[0].nation_id = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units.units[i];
    if (u->active && u->nation_id == 1) {
      u->moves_left = 0;
    }
  }
  const int sid2 = units_spawn_allow_stack(&units, ty_soldier, 5, 5);
  if (sid2 < 0) {
    return fail("1eca sub-threshold setup should spawn Soldier");
  }
  {
    ColonizeUnit* su = units_get(&units, sid2);
    if (!su) {
      return fail("1eca sub-threshold unit lookup");
    }
    su->nation_id = 0;
    su->orders = UNITS_ORDER_FORTIFIED;
  }
  ai_king_nation_turn(&ctx);
  {
    const ColonizeUnit* su = units_get_const(&units, sid2);
    if (!su || !su->active || su->type_index != ty_soldier) {
      fprintf(stderr, "unit_ai_king: Soldier type SoL45: %d (want %d)\n",
              su ? su->type_index : -1, ty_soldier);
      return fail("1eca SoL<=49 should leave a fortified colony-tile Soldier unpromoted");
    }
  }
  /*
   * Clear every probe left fortified on (5,5) from the blocks above, and
   * unfortify every other human unit anywhere (accumulated cruft from
   * earlier sub-tests) — the next block needs cap==1 exactly, so any
   * stray fortified Soldier/Dragoon elsewhere would steal that one slot
   * ahead of its own probes (lower pool index scans first).
   */
  units_despawn(&units, sid);
  units_despawn(&units, did);
  units_despawn(&units, rid);
  units_despawn(&units, unfort_id);
  units_despawn(&units, offtile_id);
  units_despawn(&units, sid2);
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units.units[i];
    if (u->active && u->nation_id == 0 && u->orders == UNITS_ORDER_FORTIFIED) {
      u->orders = UNITS_ORDER_NONE;
    }
  }

  /*
   * SoL promote band: exactly SoL=50 satisfies decomp `0x31 < iVar1`
   * (49 < 50) so it IS eligible, same as any SoL>49. At the exact threshold
   * the cap formula `min(pop>>1, pop*(sol-50)/50)` always floors to 1
   * regardless of population (the second term is always 0 at sol==50), so
   * only the *first* eligible unit found on the tile promotes this turn —
   * the Soldier here, scanned before the Dragoon; the Dragoon is left for a
   * later turn. Cont. Army abbrev already-promoted stays (raw type check
   * only matches base Soldier/Dragoon type ids).
   */
  col1.colony[0].population = 20;
  col1.colony[0].rebel_dividend = 50;
  col1.colony[0].rebel_divisor = 100;
  {
    const int sol50 = ai_king_sol_percent(&ctx, 0);
    if (sol50 != 50) {
      fprintf(stderr, "unit_ai_king: unexpected SoL %d (want 50) for band edge\n", sol50);
      return fail("1eca SoL=50 band-edge setup");
    }
  }
  colonies.colonies[0].nation_id = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units.units[i];
    if (u->active && u->nation_id == 1) {
      u->moves_left = 0;
    }
  }
  {
    char army_name_save[32];
    snprintf(army_name_save, sizeof(army_name_save), "%s", units.types[ty_cont_army].name);
    snprintf(units.types[ty_cont_army].name, sizeof(units.types[ty_cont_army].name),
             "Cont. Army");
    const int sid50 = units_spawn_allow_stack(&units, ty_soldier, 5, 5);
    const int did50 = units_spawn_allow_stack(&units, ty_dragoon, 5, 5);
    const int ca50 = units_spawn_allow_stack(&units, ty_cont_army, 5, 5);
    if (sid50 < 0 || did50 < 0 || ca50 < 0) {
      snprintf(units.types[ty_cont_army].name, sizeof(units.types[ty_cont_army].name), "%s",
               army_name_save);
      return fail("1eca SoL=50 band setup should spawn Soldier+Dragoon+Cont.Army");
    }
    {
      ColonizeUnit* su = units_get(&units, sid50);
      ColonizeUnit* du = units_get(&units, did50);
      ColonizeUnit* ca = units_get(&units, ca50);
      if (!su || !du || !ca) {
        snprintf(units.types[ty_cont_army].name, sizeof(units.types[ty_cont_army].name), "%s",
                 army_name_save);
        return fail("1eca SoL=50 band unit lookup");
      }
      su->nation_id = 0;
      du->nation_id = 0;
      ca->nation_id = 0;
      su->orders = UNITS_ORDER_FORTIFIED;
      du->orders = UNITS_ORDER_FORTIFIED;
      ca->orders = UNITS_ORDER_FORTIFIED;
      ca->moves_left = 0; /* hold — only assert type skip, not rally */
      su->profession = UNITS_JOB_SOLDIER; /* Veteran gate, see 1eca note above */
      du->profession = UNITS_JOB_SOLDIER; /* eligible by type/profession; cap==1 still skips it */
    }
    /* bugs.md 256: re-arm the once-only mobilization for this subtest. */
    col1.nation[0].nation_flags = (uint8_t)(col1.nation[0].nation_flags & ~0x08u);
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* su = units_get_const(&units, sid50);
      const ColonizeUnit* du = units_get_const(&units, did50);
      const ColonizeUnit* ca = units_get_const(&units, ca50);
      if (!su || !su->active || su->type_index != ty_cont_army) {
        fprintf(stderr, "unit_ai_king: Soldier type SoL50: %d (want %d)\n",
                su ? su->type_index : -1, ty_cont_army);
        snprintf(units.types[ty_cont_army].name, sizeof(units.types[ty_cont_army].name), "%s",
                 army_name_save);
        return fail("1eca SoL=50 should promote Soldier → Continental Army (49 < 50)");
      }
      if (!du || !du->active || du->type_index != ty_dragoon) {
        fprintf(stderr, "unit_ai_king: Dragoon type SoL50: %d (want %d)\n",
                du ? du->type_index : -1, ty_dragoon);
        snprintf(units.types[ty_cont_army].name, sizeof(units.types[ty_cont_army].name), "%s",
                 army_name_save);
        return fail("1eca SoL=50 cap==1 should leave Dragoon for a later turn (Soldier spent it)");
      }
      if (!ca || !ca->active || ca->type_index != ty_cont_army) {
        fprintf(stderr, "unit_ai_king: Cont. Army type SoL50: %d (want %d)\n",
                ca ? ca->type_index : -1, ty_cont_army);
        snprintf(units.types[ty_cont_army].name, sizeof(units.types[ty_cont_army].name), "%s",
                 army_name_save);
        return fail("1eca Cont. Army abbrev must stay skipped (not re-typed)");
      }
    }
    /* Clear this block's probes — a leftover fortified Cont. Army on (5,5)
     * would otherwise fill the capital's 2-slot garrison cap ahead of the
     * later Cont. capital-rally / fortify-on-capital sub-tests. */
    units_despawn(&units, sid50);
    units_despawn(&units, did50);
    units_despawn(&units, ca50);
    snprintf(units.types[ty_cont_army].name, sizeof(units.types[ty_cont_army].name), "%s",
             army_name_save);
  }

  /*
   * 1eca colony-SoL bias (FUN_43f7_1eca): nation aggregate mid/low, but a
   * fortified unit on a high-SoL Col1 colony's own tile promotes to
   * Continental; the same on a low-SoL colony tile does not. King promote
   * path only — not FF Washington mass-promote. No treasury bumps.
   */
  {
    ColonizeCol1Colony* grown = calloc(2, sizeof(ColonizeCol1Colony));
    if (!grown) {
      return fail("alloc 1eca colony-SoL colonies");
    }
    grown[0] = col1.colony[0];
    free(col1.colony);
    col1.colony = grown;
    col1.head.colony_count = 2;
    /* Low SoL off the weakest-port tile so crown combat cannot delete the probe. */
    col1.colony[0].x = 3;
    col1.colony[0].y = 3;
    col1.colony[0].nation_id = 0;
    col1.colony[0].population = 4;
    col1.colony[0].rebel_dividend = 30;
    col1.colony[0].rebel_divisor = 100;
    col1.colony[1].x = 11;
    col1.colony[1].y = 5;
    col1.colony[1].nation_id = 0;
    col1.colony[1].population = 1;
    col1.colony[1].rebel_dividend = 70;
    col1.colony[1].rebel_divisor = 100;
  }
  {
    const int sol_nat = ai_king_sol_percent(&ctx, 0);
    /* (30*4 + 70*1)/5 = 38 — nation alone would not Continental-promote. */
    if (sol_nat != 38) {
      fprintf(stderr, "unit_ai_king: unexpected nation SoL %d (want 38) for colony-SoL\n",
              sol_nat);
      return fail("1eca colony-SoL nation aggregate setup");
    }
  }
  colonies.colonies[0].nation_id = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  /* Park crown movers so war_act combat cannot delete the SoL probe units. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units.units[i];
    if (u->active && u->nation_id == 1) {
      u->moves_left = 0;
    }
  }
  const int sid_hi = units_spawn_allow_stack(&units, ty_soldier, 11, 5);
  const int sid_lo = units_spawn_allow_stack(&units, ty_soldier, 3, 3);
  if (sid_hi < 0 || sid_lo < 0) {
    return fail("1eca colony-SoL setup should spawn Soldiers on both colonies");
  }
  {
    ColonizeUnit* hi = units_get(&units, sid_hi);
    ColonizeUnit* lo = units_get(&units, sid_lo);
    if (!hi || !lo) {
      return fail("1eca colony-SoL unit lookup");
    }
    hi->nation_id = 0;
    lo->nation_id = 0;
    hi->orders = UNITS_ORDER_FORTIFIED;
    lo->orders = UNITS_ORDER_FORTIFIED;
    hi->profession = UNITS_JOB_SOLDIER; /* Veteran gate, see 1eca note above */
    lo->profession = UNITS_JOB_SOLDIER;
  }
  /* bugs.md 256: re-arm the once-only mobilization for this subtest. */
  col1.nation[0].nation_flags = (uint8_t)(col1.nation[0].nation_flags & ~0x08u);
  ai_king_nation_turn(&ctx);
  {
    const ColonizeUnit* hi = units_get_const(&units, sid_hi);
    const ColonizeUnit* lo = units_get_const(&units, sid_lo);
    if (!hi || !hi->active || hi->type_index != ty_cont_army) {
      fprintf(stderr, "unit_ai_king: high-SoL colony Soldier type: %d (want %d)\n",
              hi ? hi->type_index : -1, ty_cont_army);
      return fail("1eca colony-SoL>50 at tile should promote Soldier → Continental Army");
    }
    if (!lo || !lo->active || lo->type_index != ty_soldier) {
      fprintf(stderr, "unit_ai_king: low-SoL colony Soldier type: %d (want %d)\n",
              lo ? lo->type_index : -1, ty_soldier);
      return fail("1eca low colony-SoL should leave Soldier unpromoted");
    }
  }

  /*
   * bugs.md: the King's war beat must NOT drive the human's own Continentals.
   * This block used to assert the invented "capital rally" (idle human Cont.
   * Army / Cont. Cav AI_MOVE to the founding capital, then fortify two on it).
   * FUN_43f7_1eca promotes only — it never reads unit+0x08 (orders) or the
   * goto pair — so the rally is gone and the assertions are inverted: after a
   * full King turn every human Continental keeps the orders, the goto and the
   * tile the player left it on.
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    col1.head.expeditionary_force[0] = 1; /* non-empty pools: no 06a6 irregulars */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->active = false; /* no crown unit may reach and fight the probes */
      }
    }
    const int ca_id = units_spawn_allow_stack(&units, ty_cont_army, 10, 5);
    const int cav_id = units_spawn_allow_stack(&units, ty_cont_cav, 12, 5);
    const int fort_id = units_spawn_allow_stack(&units, ty_cont_army, 5, 5);
    if (ca_id < 0 || cav_id < 0 || fort_id < 0) {
      return fail("Cont. no-auto-play setup spawn");
    }
    {
      ColonizeUnit* ca = units_get(&units, ca_id);
      ColonizeUnit* cav = units_get(&units, cav_id);
      ColonizeUnit* fort = units_get(&units, fort_id);
      if (!ca || !cav || !fort) {
        return fail("Cont. no-auto-play unit lookup");
      }
      ca->nation_id = 0;
      ca->moves_left = 2 * UNITS_MP_PER_TILE;
      ca->orders = UNITS_ORDER_NONE;
      ca->goto_x = -1;
      ca->goto_y = -1;
      cav->nation_id = 0;
      cav->moves_left = 2 * UNITS_MP_PER_TILE;
      cav->orders = UNITS_ORDER_NONE;
      cav->goto_x = -1;
      cav->goto_y = -1;
      /* On the founding capital and dug in — the old code re-fortified this
       * one; nothing may touch it either way. */
      fort->nation_id = 0;
      fort->moves_left = 2 * UNITS_MP_PER_TILE;
      fort->orders = UNITS_ORDER_FORTIFIED;
      fort->goto_x = -1;
      fort->goto_y = -1;
    }
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* ca = units_get_const(&units, ca_id);
      const ColonizeUnit* cav = units_get_const(&units, cav_id);
      const ColonizeUnit* fort = units_get_const(&units, fort_id);
      if (!ca || !ca->active || !cav || !cav->active || !fort || !fort->active) {
        return fail("Cont. no-auto-play probes should remain active");
      }
      if (ca->orders != UNITS_ORDER_NONE || ca->x != 10 || ca->y != 5) {
        fprintf(stderr, "unit_ai_king: idle Cont. Army orders=%d pos=(%d,%d)\n", ca->orders,
                ca->x, ca->y);
        return fail("King turn must not order the human's Cont. Army");
      }
      if (cav->orders != UNITS_ORDER_NONE || cav->x != 12 || cav->y != 5) {
        fprintf(stderr, "unit_ai_king: idle Cont. Cav orders=%d pos=(%d,%d)\n", cav->orders,
                cav->x, cav->y);
        return fail("King turn must not order the human's Cont. Cav");
      }
      if (fort->orders != UNITS_ORDER_FORTIFIED || fort->x != 5 || fort->y != 5) {
        fprintf(stderr, "unit_ai_king: capital Cont. Army orders=%d pos=(%d,%d)\n",
                fort->orders, fort->x, fort->y);
        return fail("King turn must not wake a fortified human Continental");
      }
    }
    /*
     * A Continental the player parked with a Go To of his own keeps it: the
     * King must not retarget the human's goto pair either.
     */
    {
      ColonizeUnit* ca = units_get(&units, ca_id);
      if (!ca) {
        return fail("Cont. no-auto-play goto probe lookup");
      }
      ca->orders = UNITS_ORDER_GOTO;
      ca->goto_x = 14;
      ca->goto_y = 14;
      ca->moves_left = 2 * UNITS_MP_PER_TILE;
      ai_king_nation_turn(&ctx);
      ca = units_get(&units, ca_id);
      if (!ca || !ca->active) {
        return fail("Cont. goto probe should remain active");
      }
      if (ca->orders != UNITS_ORDER_GOTO || ca->goto_x != 14 || ca->goto_y != 14) {
        fprintf(stderr, "unit_ai_king: human goto retargeted to (%d,%d) orders=%d\n",
                ca->goto_x, ca->goto_y, ca->orders);
        return fail("King turn must not retarget a human unit's Go To");
      }
      ca->active = false;
    }
    {
      ColonizeUnit* cav = units_get(&units, cav_id);
      ColonizeUnit* fort = units_get(&units, fort_id);
      if (cav) {
        cav->active = false;
      }
      if (fort) {
        fort->active = false;
      }
    }
  }

  /*
   * bugs.md: the crown's mounted arm is spelled `Cavalry` in NAMES.TXT @UNIT,
   * not `Dragoons` (that is the colonial type). ai_king_is_ref_land_hunter used
   * to test the literal "Dragoon" only, so every REF Cavalry landed, kept the
   * beachhead goto the wave gave it (its own tile), and then sat there with a
   * full move allowance for the rest of the war. A hunter must get a target
   * that is not the tile it is standing on, and must spend moves reaching it.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].has_building[0] = false;
    colonies.colonies[0].x = 5;
    colonies.colonies[0].y = 5;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    col1.head.expeditionary_force[0] = 1; /* non-empty pools: no 06a6 irregulars */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->active = false; /* only the probe Cavalry acts this beat */
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        u->x = 1;
        u->y = 14;
        u->moves_left = 0;
      }
    }
    const int cav_id = units_spawn_allow_stack(&units, ty_cavalry, 10, 5);
    if (cav_id < 0) {
      return fail("REF Cavalry hunt setup spawn");
    }
    {
      ColonizeUnit* cav = units_get(&units, cav_id);
      if (!cav) {
        return fail("REF Cavalry hunt unit lookup");
      }
      cav->nation_id = 1;
      cav->moves_left = 2 * UNITS_MP_PER_TILE;
      /* Exactly what ai_king_ref_wave hands a fresh landing: AI_MOVE with the
       * goto pointing at the tile it stands on. */
      cav->orders = UNITS_ORDER_AI_MOVE;
      cav->goto_x = 10;
      cav->goto_y = 5;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* cav = units_get_const(&units, cav_id);
      if (!cav || !cav->active) {
        return fail("REF Cavalry should remain active");
      }
      if (cav->orders != UNITS_ORDER_AI_MOVE) {
        fprintf(stderr, "unit_ai_king: REF Cavalry orders=%d (want AI_MOVE)\n", cav->orders);
        return fail("REF Cavalry should be a land hunter");
      }
      if (cav->goto_x == 10 && cav->goto_y == 5) {
        return fail("REF Cavalry kept its own tile as goto (no hunt target)");
      }
      if (cav->x == 10 && cav->y == 5) {
        fprintf(stderr, "unit_ai_king: REF Cavalry parked at (%d,%d) goto=(%d,%d) mv=%d\n",
                cav->x, cav->y, cav->goto_x, cav->goto_y, cav->moves_left);
        return fail("REF Cavalry should march toward its hunt target");
      }
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->active = false;
      }
    }
    colonies.colonies[0].nation_id = 0;
  }

  /*
   * REF capital MD hunt bias (fandom REF main-port pressure):
   * founding capital id0 at (5,5); nearer distant colony at (11,5); Regular at
   * (9,5) → MD capital=4, MD distant=2; slack=2 → prefer capital over distant.
   * Clear human land units so colony bias is observable. 160a letter
   * cinematic Done. PARK: extra boycott cargos beyond Sugar.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].x = 5;
    colonies.colonies[0].y = 5;
    colonies.colonies[0].has_building[0] = false;
    ColonizeColony* distant = &colonies.colonies[2];
    distant->id = 2;
    distant->active = true;
    distant->nation_id = 0;
    distant->x = 11;
    distant->y = 5;
    distant->population = 2;
    distant->colonist_count = 2;
    distant->has_building[0] = false;
    snprintf(distant->name, sizeof(distant->name), "Outpost");
    if (colonies.colony_count < 3) {
      colonies.colony_count = 3;
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if ((u->x == 5 && u->y == 5) || (u->x == 11 && u->y == 5) ||
            (u->x == 9 && u->y == 5)) {
          u->x = 1;
          u->y = 1;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        /* Sweep human land so hunt compares colonies only. */
        u->x = 1;
        u->y = 14;
        u->moves_left = 0;
      }
    }
    const int cap_hunter = units_spawn_allow_stack(&units, ty_regular, 9, 5);
    if (cap_hunter < 0) {
      return fail("capital MD bias setup should spawn crown Regular");
    }
    {
      ColonizeUnit* h = units_get(&units, cap_hunter);
      if (!h) {
        return fail("capital MD bias unit lookup");
      }
      h->nation_id = 1;
      h->moves_left = 1 * UNITS_MP_PER_TILE;
      h->orders = UNITS_ORDER_NONE;
      h->goto_x = -1;
      h->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* h = units_get_const(&units, cap_hunter);
      if (!h || !h->active) {
        return fail("capital MD bias Regular should remain active");
      }
      if (h->orders != UNITS_ORDER_AI_MOVE || h->goto_x != 5 || h->goto_y != 5) {
        fprintf(stderr,
                "unit_ai_king: capital MD bias goto=(%d,%d) orders=%d "
                "(want capital 5,5 not distant 11,5)\n",
                h->goto_x, h->goto_y, h->orders);
        return fail("REF idle hunter should prefer capital when MD comparable");
      }
    }
    distant->active = false;
  }

  /*
   * Artillery siege capital MD slack (like idle hunters): both capital and
   * distant fortified; Artillery at (9,5) → MD capital=4, MD distant=2;
   * slack=2 → prefer fortified founding capital over nearer fortified outpost.
   * Source: fandom REF main-port pressure; deep multi-step siege PARKED.
   */
  {
    colonies.building_type_count = 1;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Stockade");
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].x = 5;
    colonies.colonies[0].y = 5;
    colonies.colonies[0].has_building[0] = true;
    ColonizeColony* distant = &colonies.colonies[2];
    distant->id = 2;
    distant->active = true;
    distant->nation_id = 0;
    distant->x = 11;
    distant->y = 5;
    distant->population = 2;
    distant->colonist_count = 2;
    distant->has_building[0] = true;
    snprintf(distant->name, sizeof(distant->name), "Outpost");
    if (colonies.colony_count < 3) {
      colonies.colony_count = 3;
    }
    if (!colonies_has_fortification(&colonies, &colonies.colonies[0]) ||
        !colonies_has_fortification(&colonies, distant)) {
      return fail("Artillery capital MD setup needs both colonies fortified");
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if ((u->x == 5 && u->y == 5) || (u->x == 11 && u->y == 5) ||
            (u->x == 9 && u->y == 5)) {
          u->x = 1;
          u->y = 1;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        u->x = 1;
        u->y = 14;
        u->moves_left = 0;
      }
    }
    const int art_cap = units_spawn_allow_stack(&units, ty_artillery, 9, 5);
    if (art_cap < 0) {
      return fail("Artillery capital MD setup should spawn Artillery");
    }
    {
      ColonizeUnit* art = units_get(&units, art_cap);
      if (!art) {
        return fail("Artillery capital MD unit lookup");
      }
      art->nation_id = 1;
      art->moves_left = 1 * UNITS_MP_PER_TILE;
      art->orders = UNITS_ORDER_NONE;
      art->goto_x = -1;
      art->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* art = units_get_const(&units, art_cap);
      if (!art || !art->active) {
        return fail("Artillery capital MD hunter should remain active");
      }
      if (art->orders != UNITS_ORDER_AI_MOVE || art->goto_x != 5 || art->goto_y != 5) {
        fprintf(stderr,
                "unit_ai_king: Artillery capital MD goto=(%d,%d) orders=%d "
                "(want fortified capital 5,5 not distant 11,5)\n",
                art->goto_x, art->goto_y, art->orders);
        return fail("Artillery siege should prefer fortified capital when MD slack");
      }
    }
    distant->active = false;
    colonies.colonies[0].has_building[0] = false;
  }

  /*
   * FUN_43f7_0982 lands exactly one Man-O-War per beat whatever the
   * difficulty, and max(3, min(garrison, 31)) land units beside the colony.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].active = true;
    col1.head.difficulty = 2;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    col1.head.expeditionary_force[0] = 3;
    col1.head.expeditionary_force[2] = 2;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1 && units_is_sea(&units, u->id)) {
        u->active = false;
      } else if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    {
      const int sea_before = count_nation_sea(&units, 1);
      const int land_before = count_nation_land(&units, 1);
      status[0] = '\0';
      ai_king_nation_turn(&ctx);
      const int sea_spawned = count_nation_sea(&units, 1) - sea_before;
      const int landed = count_nation_land(&units, 1) - land_before;
      if (sea_spawned != 1 || col1.head.expeditionary_force[2] != 1) {
        fprintf(stderr, "unit_ai_king: 0982 sea_spawned=%d force2=%u\n", sea_spawned,
                (unsigned)col1.head.expeditionary_force[2]);
        return fail("0982 spawns one Man-O-War per beat");
      }
      if (landed != 3 || col1.head.expeditionary_force[0] != 0) {
        fprintf(stderr, "unit_ai_king: 0982 landed=%d force0=%u\n", landed,
                (unsigned)col1.head.expeditionary_force[0]);
        return fail("0982 lands max(3, garrison) Regulars from the pool");
      }
    }
    col1.head.difficulty = 0;
  }

  /*
   * 0982 composition: the mounted/Artillery pools are each capped at
   * max(1, garrison>>3) (=1 here) when Regulars < mounted+Artillery, Regulars
   * fill the rest. force[0]=1, force[1]=2, force[2]=1 → 1 Cavalry + 1 Regular,
   * then the Regular pool is empty and the beat stops (pool-limited, never
   * invented). pool[1] fields the crown's own `Cavalry` type; the fixture only
   * fell back to the colonial `Dragoon` while it carried no Cavalry type.
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    col1.head.expeditionary_force[0] = 1; /* Regular */
    col1.head.expeditionary_force[1] = 2; /* Dragoon */
    col1.head.expeditionary_force[2] = 1; /* MoW */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1 && units_is_sea(&units, u->id)) {
        u->active = false;
      } else if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    int reg_before = 0;
    int drg_before = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 1 || units_is_sea(&units, u->id)) {
        continue;
      }
      if (u->type_index == ty_regular) {
        reg_before++;
      } else if (u->type_index == ty_cavalry) {
        drg_before++; /* pool[1] fields the crown's Cavalry (Dragoons is the fallback) */
      }
    }
    ai_king_nation_turn(&ctx);
    if (count_nation_sea(&units, 1) != 1) {
      return fail("0982 mix beat should spawn the Man-O-War");
    }
    int reg_after = 0;
    int drg_after = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 1 || units_is_sea(&units, u->id)) {
        continue;
      }
      if (u->aboard_ship_id >= 0) {
        return fail("0982 never boards land units as cargo");
      }
      if (u->type_index == ty_regular) {
        reg_after++;
      } else if (u->type_index == ty_cavalry) {
        drg_after++;
      }
    }
    if (drg_after != drg_before + 1 || reg_after != reg_before + 1) {
      fprintf(stderr, "unit_ai_king: 0982 mix Regular %d→%d Cavalry %d→%d (want +1/+1)\n",
              reg_before, reg_after, drg_before, drg_after);
      return fail("0982 mix: one Cavalry under the cap, then the last Regular");
    }
    if (col1.head.expeditionary_force[0] != 0 || col1.head.expeditionary_force[1] != 1 ||
        col1.head.expeditionary_force[2] != 0) {
      fprintf(stderr, "unit_ai_king: 0982 mix pools %u/%u/%u\n",
              (unsigned)col1.head.expeditionary_force[0], (unsigned)col1.head.expeditionary_force[1],
              (unsigned)col1.head.expeditionary_force[2]);
      return fail("0982 mix should leave one Dragoon in the pool");
    }
  }

  /*
   * 0982 Regular-heavy beat: force[0]=4, force[1]=2, force[2]=1 → Regulars ≥
   * Dragoons+Artillery forces the cap to 1, so the landing is 1 Dragoon +
   * Regulars up to max(3, garrison).
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    col1.head.expeditionary_force[0] = 4; /* Regular */
    col1.head.expeditionary_force[1] = 2; /* Dragoon */
    col1.head.expeditionary_force[2] = 1; /* MoW */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1 && units_is_sea(&units, u->id)) {
        u->active = false;
      } else if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    const int land_before = count_nation_land(&units, 1);
    ai_king_nation_turn(&ctx);
    const int landed = count_nation_land(&units, 1) - land_before;
    const int drg_used = 2 - (int)col1.head.expeditionary_force[1];
    const int reg_used = 4 - (int)col1.head.expeditionary_force[0];
    if (landed < 3 || drg_used != 1 || reg_used != landed - 1) {
      fprintf(stderr, "unit_ai_king: 0982 heavy landed=%d drg_used=%d reg_used=%d\n", landed,
              drg_used, reg_used);
      return fail("0982 Regular-heavy beat: 1 Dragoon + Regulars");
    }
    if (col1.head.expeditionary_force[2] != 0) {
      return fail("0982 Regular-heavy beat drains the MoW pool");
    }
  }

  /*
   * ai_popup wire (human queue on turn ctx): tax audience CHOICE defers hike;
   * apply Accept finishes 1d42 effect. Without ai_popups, auto path unchanged.
   * Refuse → dump-goods CHOICE → @TEAPARTY OK (thin 3dc8 stock dump).
   */
  {
    AiPopupState pop;
    ai_popup_init(&pop);
    ctx.ai_popups = &pop;

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      return fail("tax audience: GAME.TXT load failed");
    }
    ctx.messages = &game_txt;

    /* Restore multi-cargo Europe bids so dump CHOICE has Furs/etc. */
    europe.cargo_count = COLONIZE_CARGO_COUNT;
    for (int ci = 0; ci < COLONIZE_CARGO_COUNT; ++ci) {
      europe.cargo[ci].bid = 1;
    }
    ctx.europe = &europe;

    /*
     * King-audience CHOICE flow. The KING_AUDIENCE CHOICE decides whether
     * the rolled hike happens at all: the rate stays put while the dialog
     * is on screen and only Accept ("kiss the ring") commits it; Refuse
     * ("hold a tea party") leaves it and boycotts instead (bugs.md — DOS's
     * apply-then-revert put a raise the player had not answered into the
     * save, and reaches the same two end states). Payload carries the exact
     * (applied delta, picked cargo) pair via ai_king_teaparty_payload, both
     * fixed at roll time, not a player-picked cargo from a second CHOICE
     * (that two-step "dump-goods CHOICE after Refuse" shape is retired —
     * see ai_king.c's R6 "stale-claim correction").
     *
     * Deterministic via seed=1: rebel_sentiment=100, tax=10, SoL=100,
     * turn=44 -> score 1053 -> delta +4 (tax 10->14), then a uniform
     * (all Europe bids=1) dump-goods roll over the same rng stream
     * picks cargo index 3 (Cotton).
     */
    turn = 44;
    ai_king_latch_set(&col1, 0, 0);
    col1.head.game_options.woi = 0;
    ai_king_latch_set(&col1, 2, 0);
    ai_king_latch_set(&col1, 5, 0);
    col1.head.rebel_sentiment_report = 100;
    col1.nation[0].tax_rate = 10;
    europe.tax_percent = 10;
    col1.nation[0].boycott_bitmap = 0;
    col1.nation[0].liberty_bells_total = 0;
    col1.nation[0].gold = 0;
    col1.colony[0].rebel_dividend = 100;
    col1.colony[0].rebel_divisor = 100;
    year = 1536;
    autumn = 0;
    status[0] = '\0';
    ai_popup_clear(&pop);
    /* Every cargo stocked: the roulette then sees the same candidate set the
     * flat bids describe (bugs.md — only stocked goods are eligible). */
    for (int ci = 0; ci < COLONIZE_CARGO_COUNT; ++ci) {
      colonies.colonies[0].stock[ci] = 50;
    }
    const int expected_cargo = COLONIZE_CARGO_COTTON;
    const int expected_delta = 4;
    /* ai_king_teaparty_payload's own formula (applied*100+cargo) — not
     * exported via ai_king.h, so mirrored here rather than exposed. */
    const int expected_payload = expected_delta * 100 + expected_cargo;
    ColonizeDosRng accept_rng;
    dos_rng_seed(&accept_rng, 1u);
    ctx.rng = &accept_rng;
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 10) {
      fprintf(stderr, "unit_ai_king: audience hike tax_rate=%u (want %d)\n",
              col1.nation[0].tax_rate, 10);
      assets_msg_free(&game_txt);
      return fail("audience hike must stay unapplied while the CHOICE is pending");
    }
    int choice_qi = -1;
    for (int i = 0; i < pop.queue_count; ++i) {
      if (pop.queue[i].tag == AI_POPUP_TAG_KING_AUDIENCE &&
          pop.queue[i].kind == AI_POPUP_KIND_CHOICE) {
        choice_qi = i;
        break;
      }
    }
    if (choice_qi < 0) {
      assets_msg_free(&game_txt);
      return fail("ai_popups should enqueue KING_AUDIENCE choice after the hike");
    }
    if (pop.queue[choice_qi].payload != expected_payload) {
      fprintf(stderr, "unit_ai_king: KING_AUDIENCE payload=%d (want %d)\n",
              pop.queue[choice_qi].payload, expected_payload);
      assets_msg_free(&game_txt);
      return fail("KING_AUDIENCE choice payload should carry (applied, picked cargo)");
    }
    /* Accept ("kiss the ring"): the proposed hike is committed now. */
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* AI_KING_CHOICE_ACCEPT */
    pop.result_tag = AI_POPUP_TAG_KING_AUDIENCE;
    pop.result_nation_a = 0;
    pop.result_nation_b = 1;
    pop.result_payload = pop.queue[choice_qi].payload;
    ai_king_apply_popup_result(&ctx, &pop);
    ai_popup_consume_result(&pop);
    if (col1.nation[0].tax_rate != 10 + expected_delta) {
      assets_msg_free(&game_txt);
      return fail("Accept should commit the proposed hike");
    }
    if (europe.tax_percent != 10 + expected_delta) {
      assets_msg_free(&game_txt);
      return fail("Accept should mirror the committed rate into Europe");
    }
    if (col1.nation[0].boycott_bitmap != 0) {
      assets_msg_free(&game_txt);
      return fail("Accept must not boycott anything");
    }
    if (!strstr(status, "raised") || !strstr(status, "14")) {
      fprintf(stderr, "unit_ai_king: Accept status: '%s'\n", status);
      assets_msg_free(&game_txt);
      return fail("Accept apply should status the committed hike rate");
    }

    /* Refuse ("hold a tea party"): fresh identical roll, hike never lands. */
    col1.nation[0].tax_rate = 10;
    europe.tax_percent = 10;
    col1.nation[0].boycott_bitmap = 0;
    col1.nation[0].liberty_bells_total = 0;
    col1.colony[0].rebel_dividend = 100;
    col1.colony[0].rebel_divisor = 100;
    status[0] = '\0';
    ai_popup_clear(&pop);
    ColonizeDosRng refuse_rng2;
    dos_rng_seed(&refuse_rng2, 1u);
    ctx.rng = &refuse_rng2;
    ai_king_nation_turn(&ctx);
    ctx.rng = NULL;
    if (col1.nation[0].tax_rate != 10) {
      assets_msg_free(&game_txt);
      return fail("refuse setup: hike must stay unapplied (same as Accept path)");
    }
    choice_qi = -1;
    for (int i = 0; i < pop.queue_count; ++i) {
      if (pop.queue[i].tag == AI_POPUP_TAG_KING_AUDIENCE &&
          pop.queue[i].kind == AI_POPUP_KIND_CHOICE) {
        choice_qi = i;
        break;
      }
    }
    if (choice_qi < 0) {
      assets_msg_free(&game_txt);
      return fail("ai_popups should enqueue KING_AUDIENCE choice (refuse path)");
    }
    /* Seed richest-colony stock for the @TEAPARTY dump. */
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].active = true;
    colonies.colonies[0].stock[expected_cargo] = 75;
    const int stock_before = colonies.colonies[0].stock[expected_cargo];
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 2; /* AI_KING_CHOICE_REFUSE */
    pop.result_tag = AI_POPUP_TAG_KING_AUDIENCE;
    pop.result_nation_a = 0;
    pop.result_nation_b = 1;
    pop.result_payload = pop.queue[choice_qi].payload;
    ai_king_apply_popup_result(&ctx, &pop);
    ai_popup_consume_result(&pop);
    if (col1.nation[0].tax_rate != 10) {
      fprintf(stderr, "unit_ai_king: Refuse tax_rate=%u (want unchanged 10)\n",
              col1.nation[0].tax_rate);
      assets_msg_free(&game_txt);
      return fail("Refuse should leave the tax rate where it was");
    }
    if ((col1.nation[0].boycott_bitmap & (1u << expected_cargo)) == 0) {
      assets_msg_free(&game_txt);
      return fail("Refuse should boycott the cargo picked at hike time (Cotton)");
    }
    if (colonies.colonies[0].stock[expected_cargo] != stock_before - 75) {
      fprintf(stderr, "unit_ai_king: TEAPARTY stock got %d want %d\n",
              colonies.colonies[0].stock[expected_cargo], stock_before - 75);
      assets_msg_free(&game_txt);
      return fail("Refuse @TEAPARTY should dump min(100,stock) from the richest colony");
    }
    int found_teaparty = 0;
    for (int i = 0; i < pop.queue_count; ++i) {
      if (pop.queue[i].tag == AI_POPUP_TAG_KING_TAX && pop.queue[i].kind == AI_POPUP_KIND_OK &&
          strstr(pop.queue[i].body, "Sons of Liberty") &&
          strstr(pop.queue[i].body, "throw") && strstr(pop.queue[i].body, "75")) {
        found_teaparty = 1;
        break;
      }
    }
    if (!found_teaparty) {
      assets_msg_free(&game_txt);
      return fail("Refuse apply should enqueue @TEAPARTY KING_TAX OK");
    }

    /* Keep GAME.TXT loaded for @DECLARE congress CHOICE (+ merc uses messages). */

    /* Congress CHOICE: gate met → enqueue @DECLARE, no WoI until Confirm. */
    ai_king_latch_set(&col1, 0, 0);
    col1.head.game_options.woi = 0;
    ai_king_latch_set(&col1, 5, 0);
    col1.colony[0].rebel_dividend = 60;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].liberty_bells_total = 200;
    snprintf(col1.player[0].country_name, sizeof(col1.player[0].country_name), "England");
    snprintf(europe.nation_name, sizeof(europe.nation_name), "England");
    year = 1600;
    autumn = 0;
    status[0] = '\0';
    ai_popup_clear(&pop);
    /* Avoid another tax audience this beat: off tax interval. */
    year = 1537;
    ai_king_nation_turn(&ctx);
    if (ai_king_latch_get(&col1, 0) != 0) {
      assets_msg_free(&game_txt);
      return fail("ai_popups congress must defer WoI until Confirm");
    }
    /*
     * bugs.md: the per-turn tick must never raise the @DECLARE confirm on its
     * own, however high SoL runs — DOS reaches FUN_43f7_2564 only from the
     * MENU.TXT @GAME "DECLARE INDEPENDENCE" command.
     */
    for (int i = 0; i < pop.queue_count; ++i) {
      if (pop.queue[i].tag == AI_POPUP_TAG_KING_CONGRESS) {
        assets_msg_free(&game_txt);
        return fail("nation turn must not spawn the @DECLARE choice by itself");
      }
    }
    ai_king_menu_declare_independence(&ctx);
    {
      int found_congress = 0;
      int congress_qi = -1;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_CONGRESS &&
            pop.queue[i].kind == AI_POPUP_KIND_CHOICE) {
          found_congress = 1;
          congress_qi = i;
          break;
        }
      }
      if (!found_congress) {
        assets_msg_free(&game_txt);
        return fail("ai_popups should enqueue KING_CONGRESS choice");
      }
      if (!strstr(pop.queue[congress_qi].body, "independence from England") &&
          !strstr(pop.queue[congress_qi].body, "declare our independence")) {
        fprintf(stderr, "unit_ai_king: @DECLARE body: '%s'\n",
                pop.queue[congress_qi].body);
        assets_msg_free(&game_txt);
        return fail("KING_CONGRESS body should use @DECLARE prose");
      }
      int found_never = 0;
      int found_liberty = 0;
      for (int ci = 0; ci < pop.queue[congress_qi].choice_count; ++ci) {
        if (strstr(pop.queue[congress_qi].choices[ci], "Never")) {
          found_never = 1;
        }
        if (strstr(pop.queue[congress_qi].choices[ci], "liberty")) {
          found_liberty = 1;
        }
      }
      if (!found_never || !found_liberty) {
        assets_msg_free(&game_txt);
        return fail("KING_CONGRESS choices should use @DECLARE Never/Yes labels");
      }
    }
    /* Earlier subtests left a partial force (no man-o-wars); zero it so the
     * declare-time fallback seed (DOS 75c2:360b values) provides the fleet
     * the same-turn wave below needs. */
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* Confirm (AI_KING_CHOICE_CONFIRM) */
    pop.result_tag = AI_POPUP_TAG_KING_CONGRESS;
    pop.result_nation_a = 0;
    pop.result_nation_b = 1;
    pop.result_payload = 60;
    ai_king_apply_popup_result(&ctx, &pop);
    ai_popup_consume_result(&pop);
    if (ai_king_latch_get(&col1, 0) == 0 || ai_king_latch_get(&col1, 5) == 0) {
      assets_msg_free(&game_txt);
      return fail("apply Confirm should declare WoI + congress unknown46[5]");
    }
    /* R2: Confirm chain → @INDEPENDENCE letter OK. bugs.md 242: NO @HOWTOWIN
     * at declare — it fires at the first rebel recapture (units.c). */
    {
      int found_letter = 0;
      int found_how = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind != AI_POPUP_KIND_OK) {
          continue;
        }
        if (pop.queue[i].title[0] != '\0') {
          fprintf(stderr, "unit_ai_king: invented title '%s'\n", pop.queue[i].title);
          assets_msg_free(&game_txt);
          return fail("GAME.TXT wood OKs must not invent a title line");
        }
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_LETTER &&
            (strstr(pop.queue[i].body, "Declaration of Independence") ||
             strstr(pop.queue[i].body, "Continental Congress signs"))) {
          found_letter = 1;
        }
        if (strstr(pop.queue[i].body, "road to freedom") ||
            (strstr(pop.queue[i].body, "recapture") &&
             strstr(pop.queue[i].body, "ground forces"))) {
          found_how = 1;
        }
      }
      if (!found_letter) {
        assets_msg_free(&game_txt);
        return fail("apply Confirm should enqueue @INDEPENDENCE KING_LETTER OK");
      }
      if (found_how) {
        assets_msg_free(&game_txt);
        return fail("apply Confirm must NOT enqueue @HOWTOWIN (bugs.md 242)");
      }
    }
    /* bugs.md 245: no rename on the choice-apply path either. */
    if (strcmp(col1.player[0].country_name, "United Colonies") == 0) {
      assets_msg_free(&game_txt);
      return fail("apply Confirm must NOT rename country_name (bugs.md 245)");
    }

    /*
     * Same-turn after Confirm: wartime wave with seeded REF → @INVASION
     * KING_ARRIVAL (colony still human; force not yet cleared for merc).
     */
    {
      colonies.colonies[0].nation_id = 0;
      snprintf(colonies.colonies[0].name, sizeof(colonies.colonies[0].name), "Jamestown");
      status[0] = '\0';
      ai_popup_clear(&pop);
      ai_king_nation_turn(&ctx);
      int found_invasion = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_ARRIVAL &&
            pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "lands near") &&
            strstr(pop.queue[i].body, "Jamestown")) {
          found_invasion = 1;
          break;
        }
      }
      if (!found_invasion) {
        fprintf(stderr, "unit_ai_king: post-Confirm ARRIVAL queue_count=%d status='%s'\n",
                pop.queue_count, status);
        assets_msg_free(&game_txt);
        return fail("post-Confirm wave should enqueue @INVASION KING_ARRIVAL");
      }
    }

    /*
     * Merc CHOICE (FUN_43f7_2022 rebel branch, real port): recurring
     * per-turn 1-in-3 roll while REF absent/Artillery pool empty — needs a
     * real ctx.rng to fire at all (guards on !ctx->rng). Seed 1 hits the
     * roll on its first call (dos_rng's LCG warm-up bias — a tiny seed's
     * first output is small regardless of range, same property the
     * WoI-defection test coverage already documented); at difficulty 0
     * that seed also rolls qty_a=3/extra=Dragoon/price=5500 (probed
     * offline, not hand-derived) — read the real values back from the
     * popup payload rather than hardcode them, in case the roll/price
     * formula shifts.
     */
    ColonizeDosRng merc_rng;
    /* Seed re-probed for the DOS 06a6 Tory-uprising roll now consumed first
     * (bugs.md 261): first draw skips the uprising, second hits the 1-in-3
     * merc roll. */
    dos_rng_seed(&merc_rng, 3u);
    ctx.rng = &merc_rng;
    col1.nation[0].gold = 6000;
    europe.gold = 6000;
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].active = true; /* wave test above may have captured/destroyed it */
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    /* MoW pool (0x53e6, backup_force[2]) empty → 2022 gate allows the roll;
     * zero Artillery pool [3] too so ai_king_foreign_intervene's land-troop
     * drain can't spawn an unrelated unit in this merc-offer probe. */
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    /* Park crown far from the port so same-beat war_act combat/capture
     * cannot re-take it before the popup apply reads it back
     * (weakest_port needs it human). moves_left=0 alone wasn't enough —
     * a crown unit already standing on/adjacent to the tile can still
     * capture on presence; move them away entirely. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
        u->x = 0;
        u->y = 0;
      }
    }
    status[0] = '\0';
    ai_popup_clear(&pop);
    /* bugs.md 256: intervene/merc skip the mobilization turn — make sure the
     * once-only mobilization is already consumed before this probe. */
    col1.nation[0].nation_flags |= 0x08u;
    const int merc_units_before = count_nation(&units, 0);
    const uint32_t merc_gold_before = col1.nation[0].gold;
    ai_king_nation_turn(&ctx);
    if (col1.nation[0].gold != merc_gold_before) {
      return fail("ai_popups merc must defer spend until Hire apply");
    }
    int merc_payload = 0;
    {
      int found_merc = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_MERC &&
            pop.queue[i].kind == AI_POPUP_KIND_CHOICE) {
          found_merc = 1;
          merc_payload = pop.queue[i].payload;
          break;
        }
      }
      if (!found_merc) {
        return fail("ai_popups should enqueue KING_MERC Hire/Decline");
      }
    }
    const int merc_price = merc_payload & 0x7fff; /* price = bits 0-14; bit 15 = extra_flag */
    if (merc_price <= 0) {
      return fail("KING_MERC payload should carry a positive rolled price");
    }
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* Hire */
    pop.result_tag = AI_POPUP_TAG_KING_MERC;
    pop.result_nation_a = 0;
    pop.result_nation_b = 1;
    pop.result_payload = merc_payload;
    ai_king_apply_popup_result(&ctx, &pop);
    ai_popup_consume_result(&pop);
    if (col1.nation[0].gold != merc_gold_before - (uint32_t)merc_price) {
      fprintf(stderr, "unit_ai_king: gold after Hire=%u want=%u (price=%d)\n",
              (unsigned)col1.nation[0].gold, (unsigned)(merc_gold_before - (uint32_t)merc_price),
              merc_price);
      return fail("apply Hire should spend the rolled price");
    }
    if (count_nation(&units, 0) <= merc_units_before) {
      return fail("apply Hire should spawn rebel troop-gift units");
    }
    {
      int found_hire_ok = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_MERC &&
            pop.queue[i].kind == AI_POPUP_KIND_OK &&
            (strstr(pop.queue[i].body, "mercenaries arrive") != NULL ||
             strstr(pop.queue[i].body, "Mercenaries arrive") != NULL)) {
          found_hire_ok = 1;
          break;
        }
      }
      if (!found_hire_ok) {
        return fail("apply Hire should enqueue merc success follow-up OK");
      }
    }

    /* R6: Decline apply → follow-up OK, no spend/spawn, no once-per-war gate
     * (DOS has none — a fresh roll can offer again on a later turn). */
    {
      dos_rng_seed(&merc_rng, 6u); /* fresh roll (uprising-draw-aware, see above) */
      col1.nation[0].gold = 6000;
      europe.gold = 6000;
      colonies.colonies[0].nation_id = 0;
      colonies.colonies[0].active = true;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      /* MoW pool [2] empty → 2022 gate allows the roll; zero Artillery [3]
       * too so foreign_intervene's land-troop drain stays out of this probe. */
      col1.head.backup_force[2] = 0;
      col1.head.backup_force[3] = 0;
      /* Park crown away from the port — see Hire block above. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1) {
          u->moves_left = 0;
          u->x = 0;
          u->y = 0;
        }
      }
      status[0] = '\0';
      ai_popup_clear(&pop);
      const uint32_t decline_gold_before = col1.nation[0].gold;
      ai_king_nation_turn(&ctx);
      /* Counted after the turn: 10f0 may land the human's own intervention
       * troops during it; only the Decline apply must add nothing. */
      const int decline_units_before = count_nation(&units, 0);
      int decline_payload = 0;
      {
        int found_merc = 0;
        for (int i = 0; i < pop.queue_count; ++i) {
          if (pop.queue[i].tag == AI_POPUP_TAG_KING_MERC &&
              pop.queue[i].kind == AI_POPUP_KIND_CHOICE) {
            found_merc = 1;
            decline_payload = pop.queue[i].payload;
            break;
          }
        }
        if (!found_merc) {
          return fail("R6 Decline probe should enqueue KING_MERC Hire/Decline");
        }
      }
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 2; /* Decline */
      pop.result_tag = AI_POPUP_TAG_KING_MERC;
      pop.result_nation_a = 0;
      pop.result_nation_b = 1;
      pop.result_payload = decline_payload;
      ai_king_apply_popup_result(&ctx, &pop);
      ai_popup_consume_result(&pop);
      if (col1.nation[0].gold != decline_gold_before) {
        return fail("apply Decline must not spend gold");
      }
      if (count_nation(&units, 0) != decline_units_before) {
        return fail("apply Decline must not spawn rebel troop-gift units");
      }
      if (!strstr(status, "Mercenaries declined")) {
        fprintf(stderr, "unit_ai_king: Decline status: '%s'\n", status);
        return fail("apply Decline should write Mercenaries declined. status");
      }
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_MERC &&
            pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "declined")) {
          return fail("apply Decline must not enqueue invented declined OK");
        }
      }
    }
    ctx.rng = NULL; /* restore — later blocks in this test assume no RNG */

    /*
     * R3: 10f0 intervene landing enqueues @INTERVENTION + @INTERVENE ARRIVAL.
     * WoI + REF empty + backup; merc flag already set so no Hire CHOICE spam.
     */
    {
      ai_king_latch_set(&col1, 0, 1);
      col1.head.game_options.woi = 1;
      /* bugs.md 258: @INTERVENTION fires once per game — earlier probes in
       * this file already landed an intervention; reset the latch so this
       * subtest sees the announcement again. */
      ai_king_latch_set(&col1, AI_KING_INTERVENE_ANNOUNCED_BYTE, 0);
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      colonies.colonies[0].nation_id = 0;
      col1.head.backup_force[0] = 2;
      col1.head.backup_force[1] = 2;
      col1.head.backup_force[2] = 0;
      col1.head.backup_force[3] = 0;
      status[0] = '\0';
      ai_popup_clear(&pop);
      /* The @INTERVENTION announce is the bells-threshold spend (DOS 0a22 →
       * 74462), not the per-turn drain — drive it directly. */
      if (!ai_king_spend_woi_bell_pool(&ctx, 0)) {
        return fail("bells spend should fire the intervention announce");
      }
      {
        int arrival_ok = 0;
        int found_intervention = 0;
        int found_intervene = 0;
        for (int i = 0; i < pop.queue_count; ++i) {
          if (pop.queue[i].tag == AI_POPUP_TAG_KING_ARRIVAL &&
              pop.queue[i].kind == AI_POPUP_KIND_OK) {
            arrival_ok++;
            if (strstr(pop.queue[i].body, "declares war") ||
                strstr(pop.queue[i].body, "War of Independence")) {
              found_intervention = 1;
            }
            if (strstr(pop.queue[i].body, "Intervention Force") ||
                strstr(pop.queue[i].body, "regales")) {
              found_intervene = 1;
            }
          }
        }
        if (arrival_ok != 2 || !found_intervention || !found_intervene) {
          fprintf(stderr,
                  "unit_ai_king: intervene ARRIVAL count=%d interv=%d arrive=%d\n",
                  arrival_ok, found_intervention, found_intervene);
          return fail("10f0 intervene should enqueue @INTERVENTION + @INTERVENE once each");
        }
      }
      /* Same-turn capture may overwrite status (1528 pattern); popup is canonical. */
    }

    /*
     * Restless chrome: SoL 45 -> status only (invented wood OK demoted).
     * Autumn + SoL 45 + taxes below declare; peacetime (clear WoI).
     * Force single-colony SoL (earlier 1eca block may leave colony_count=2).
     */
    {
      ai_king_latch_set(&col1, 0, 0);
      col1.head.game_options.woi = 0;
      ai_king_latch_set(&col1, 5, 0);
      col1.head.colony_count = 1;
      col1.colony[0].nation_id = 0;
      col1.colony[0].population = 4;
      col1.colony[0].rebel_dividend = 45;
      col1.colony[0].rebel_divisor = 100;
      col1.nation[0].liberty_bells_total = 50;
      year = 1590;
      autumn = 1;
      status[0] = '\0';
      ai_popup_clear(&pop);
      if (ai_king_sol_percent(&ctx, 0) != 45) {
        return fail("restless+ai_popups SoL setup want 45");
      }
      ai_king_nation_turn(&ctx);
      if (ai_king_latch_get(&col1, 0) != 0 || ai_king_latch_get(&col1, 5) != 0) {
        return fail("restless+ai_popups must leave WoI/congress clear");
      }
      if (!strstr(status, "Sons of Liberty") || !strstr(status, "45")) {
        fprintf(stderr, "unit_ai_king: restless+popups status: '%s'\n", status);
        return fail("restless+ai_popups should still set restless status");
      }
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            (pop.queue[i].tag == AI_POPUP_TAG_INFO ||
             pop.queue[i].tag == AI_POPUP_TAG_KING_TAX) &&
            strstr(pop.queue[i].body, "restless")) {
          return fail("restless chrome must not enqueue invented INFO OK");
        }
      }
    }

    assets_msg_free(&game_txt);
    ctx.messages = NULL;
    ctx.ai_popups = NULL;
  }

  /* Revolution lose @LOSING2: WoI + REF + zero colonies → unknown46[4]=2. */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1); /* REF already invading */
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1785;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("rev-lose2 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 1;
    }

    ColonizeUnitPool eu;
    units_reset(&eu);

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-lose2: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 2) {
      fprintf(stderr, "unit_ai_king: rev-lose2 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("WoI + REF + no colonies should latch revolution lost");
    }
    if (!strstr(estatus, "control all colonies")) {
      fprintf(stderr, "unit_ai_king: rev-lose2 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-lose2 should set @LOSING2 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "control all colonies") &&
            strstr(pop.queue[i].body, "United Colonies") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("rev-lose2 should enqueue @LOSING2 INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution lose2 (no colonies) ok\n");
  }

  /* Revolution lose @LOSING1: WoI + REF + inland colony only (ports==0) → lost. */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1); /* REF already invading */
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1785;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("rev-lose1 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 1; /* all land — inland colony is non-coastal */
    }
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    units_reset(&eu);

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-lose1: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 2) {
      fprintf(stderr, "unit_ai_king: rev-lose1 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("WoI + REF + inland-only should latch revolution lost via ports");
    }
    if (!strstr(estatus, "control all ports") && !strstr(estatus, "ports in")) {
      fprintf(stderr, "unit_ai_king: rev-lose1 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-lose1 should set @LOSING1 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "control all ports") &&
            strstr(pop.queue[i].body, "United Colonies") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("rev-lose1 should enqueue @LOSING1 INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution lose1 (no ports, inland left) ok\n");
  }

  /* Mid-war @WARN1/@WARN2: WoI + REF + exactly one coastal colony → INFO OKs;
   * unknown46[6] port episode + unknown46[7] colony episode; clear when >1. */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    ai_king_latch_set(&end, 6, 0);
    ai_king_latch_set(&end, 7, 0);
    end.head.year = 1600;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("warn1 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25; /* ocean */
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25; /* coast neighbor for (5,5) */
    emap.terrain[8 + 5 * 16] = 1;
    emap.terrain[9 + 5 * 16] = 25; /* coast neighbor for (8,5) */
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    units_reset(&eu);
    /* Keep a crown unit so year<<1850 win path cannot fire. */
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1;
      u->x = 0;
      u->y = 0;
      eu.unit_count = 1;
    }
    /* DOS 3844_0442 win gate: a bare typeless crown unit doesn't count as
     * land force — keep the REF Regulars pool stocked so the King has not
     * "run out of forces" in this warn-only scenario (bugs.md 261 rework). */
    end.head.expeditionary_force[0] = 5;

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn1: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 0) {
      fprintf(stderr, "unit_ai_king: warn1 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn1 must not latch endgame");
    }
    if (ai_king_latch_get(&end, 6) != 1) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn1 should set unknown46[6] episode latch");
    }
    if (ai_king_latch_get(&end, 7) != 1) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn2 should set unknown46[7] episode latch");
    }
    if (!strstr(estatus, "all but 1") || !strstr(estatus, "surrender")) {
      fprintf(stderr, "unit_ai_king: warn1 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn1 should set @WARN1 status");
    }
    {
      int found_port = 0;
      int found_col = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind != AI_POPUP_KIND_OK) {
          continue;
        }
        if (strstr(pop.queue[i].body, "all but 1") &&
            strstr(pop.queue[i].body, "United Colonies") &&
            strstr(pop.queue[i].body, "surrender")) {
          found_port = 1;
        }
        if (strstr(pop.queue[i].body, "all but 1") &&
            strstr(pop.queue[i].body, "colonies") &&
            strstr(pop.queue[i].body, "lose the war")) {
          found_col = 1;
        }
      }
      if (!found_port) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn1 should enqueue @WARN1 INFO OK");
      }
      if (!found_col) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn2 should enqueue @WARN2 INFO OK");
      }
    }

    /* Second turn with latches set: no second WARN1/WARN2 enqueue. */
    {
      const int q0 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      if (ai_king_latch_get(&end, 6) != 1 || ai_king_latch_get(&end, 7) != 1) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn latches should remain while one colony/port left");
      }
      int rewarn = 0;
      for (int i = q0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "all but 1")) {
          rewarn = 1;
          break;
        }
      }
      if (rewarn) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn1/warn2 must not re-enqueue while latched");
      }
    }

    /* bugs.md batch (DOS 3844_0442): warn band is <3 ports/colonies, so the
     * episode clears only at >=3 — reclaim TWO coastal colonies to clear,
     * then drop back below to re-fire. */
    {
      ColonizeColony* c2 = &cp.colonies[1];
      memset(c2, 0, sizeof(*c2));
      c2->active = true;
      c2->nation_id = 0;
      c2->x = 8;
      c2->y = 5;
      ColonizeColony* c3 = &cp.colonies[2];
      memset(c3, 0, sizeof(*c3));
      c3->active = true;
      c3->nation_id = 0;
      c3->x = 10;
      c3->y = 5;
      emap.terrain[10 + 5 * 16] = 1; /* land tile for the third port */
      cp.colony_count = 3;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      if (ai_king_latch_get(&end, 6) != 0 || ai_king_latch_get(&end, 7) != 0) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn latches should clear when ports/colonies>1");
      }
      c2->active = false;
      c3->active = false;
      cp.colony_count = 1;
      const int q1 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      if (ai_king_latch_get(&end, 6) != 1 || ai_king_latch_get(&end, 7) != 1) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn should re-latch after reclaim then drop to one");
      }
      int found_port = 0;
      int found_col = 0;
      for (int i = q1; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind != AI_POPUP_KIND_OK) {
          continue;
        }
        if (strstr(pop.queue[i].body, "surrender") &&
            strstr(pop.queue[i].body, "all but 1")) {
          found_port = 1;
        }
        if (strstr(pop.queue[i].body, "lose the war") &&
            strstr(pop.queue[i].body, "colonies")) {
          found_col = 1;
        }
      }
      if (!found_port || !found_col) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn1/warn2 should re-enqueue after reclaim episode");
      }
    }

    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution warn1/warn2 (one colony) ok\n");
  }

  /* Mid-war @WARN3: crown pop share 50–89%; unknown46[10] episode. */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    ai_king_latch_set(&end, 10, 0);
    end.head.year = 1600;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("warn3 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25;
    }
    /* Two human coastal colonies (avoid WARN1/WARN2) + one crown. */
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25;
    emap.terrain[8 + 5 * 16] = 1;
    emap.terrain[9 + 5 * 16] = 25;
    emap.terrain[5 + 8 * 16] = 1;
    emap.terrain[6 + 8 * 16] = 25;
    {
      ColonizeColony* c0 = &cp.colonies[0];
      memset(c0, 0, sizeof(*c0));
      c0->active = true;
      c0->nation_id = 0;
      c0->x = 5;
      c0->y = 5;
      c0->population = 20;
      ColonizeColony* c1 = &cp.colonies[1];
      memset(c1, 0, sizeof(*c1));
      c1->active = true;
      c1->nation_id = 0;
      c1->x = 8;
      c1->y = 5;
      c1->population = 20;
      ColonizeColony* ck = &cp.colonies[2];
      memset(ck, 0, sizeof(*ck));
      ck->active = true;
      ck->nation_id = 1; /* crown */
      ck->x = 5;
      ck->y = 8;
      ck->population = 227; /* 85% share — DOS warn band is 80-89% */
      cp.colony_count = 3;
    }

    ColonizeUnitPool eu;
    units_reset(&eu);
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1;
      eu.unit_count = 1;
    }

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 0) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3 must not latch endgame");
    }
    if (ai_king_latch_get(&end, 10) != 1) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3 should set unknown46[10] episode latch");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "85%") &&
            strstr(pop.queue[i].body, "population") &&
            strstr(pop.queue[i].body, "United Colonies")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn3 should enqueue @WARN3 INFO OK");
      }
    }
    {
      const int q0 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      int re = 0;
      for (int i = q0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "85%") &&
            strstr(pop.queue[i].body, "population")) {
          re = 1;
          break;
        }
      }
      if (re) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn3 must not re-enqueue while latched");
      }
    }
    /* Drop crown share below 80% → clear latch; raise again → re-fire. */
    cp.colonies[2].population = 10; /* 10/50 = 20% */
    estatus[0] = '\0';
    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 10) != 0) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3 latch should clear when pop share <80%");
    }
    cp.colonies[2].population = 227;
    const int q1 = pop.queue_count;
    estatus[0] = '\0';
    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 10) != 1) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3 should re-latch after share returns to 80–89%");
    }
    {
      int found = 0;
      for (int i = q1; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "population") &&
            strstr(pop.queue[i].body, "85%")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn3 should re-enqueue after reclaim episode");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution warn3 (pop share) ok\n");
  }

  /* Revolution lose @LOSING3: crown pop share ≥90%. */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1785;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("lose3 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25;
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25;
    emap.terrain[8 + 5 * 16] = 1;
    emap.terrain[9 + 5 * 16] = 25;
    {
      ColonizeColony* c0 = &cp.colonies[0];
      memset(c0, 0, sizeof(*c0));
      c0->active = true;
      c0->nation_id = 0;
      c0->x = 5;
      c0->y = 5;
      c0->population = 10;
      ColonizeColony* ck = &cp.colonies[1];
      memset(ck, 0, sizeof(*ck));
      ck->active = true;
      ck->nation_id = 1;
      ck->x = 8;
      ck->y = 5;
      ck->population = 90; /* 90% */
      cp.colony_count = 2;
    }

    ColonizeUnitPool eu;
    units_reset(&eu);
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1;
      eu.unit_count = 1;
    }

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("lose3: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 2) {
      fprintf(stderr, "unit_ai_king: lose3 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("crown pop ≥90% should latch revolution lost via @LOSING3");
    }
    if (!strstr(estatus, "over 90%") && !strstr(estatus, "90% of")) {
      fprintf(stderr, "unit_ai_king: lose3 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("lose3 should set @LOSING3 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "population") &&
            strstr(pop.queue[i].body, "United Colonies") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("lose3 should enqueue @LOSING3 INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution lose3 (pop share) ok\n");
  }

  /* Revolution win: WoI + year≥1850 + no crown units → unknown46[4]=1.
   * GAME.TXT @WINNING when messages + ai_popups attached. */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1850;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");

    ColonizeColonyPool cp;
    colonies_init(&cp);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain) {
      return fail("rev-win alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25; /* ocean */
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25; /* coast neighbor */
    /* Found a coastal colony for human so lose path doesn't fire. */
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    units_reset(&eu);
    /* No crown units. */

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-win: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 1) {
      fprintf(stderr, "unit_ai_king: rev-win endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("WoI + 1850 + no crown units should latch revolution won");
    }
    if (!strstr(estatus, "Expeditionary Force annihilated") &&
        !strstr(estatus, "accepts surrender") &&
        !strstr(estatus, "annihilated")) {
      fprintf(stderr, "unit_ai_king: rev-win status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-win should set @WINNING status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            (strstr(pop.queue[i].body, "Expeditionary Force annihilated") ||
             strstr(pop.queue[i].body, "accepts surrender")) &&
            strstr(pop.queue[i].body, "Washington") &&
            strstr(pop.queue[i].body, "United Colonies")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("rev-win should enqueue @WINNING INFO OK");
      }
    }
    /* bugs.md 264: @WINNING (KING_WAR_END payload 1) FIRST, then the
     * @KINGLOSE throne audience (KING_THRONE payload 1) — the audience
     * dismissal is what opens the retire score. */
    {
      int win_at = -1;
      int throne_at = -1;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_WAR_END && pop.queue[i].payload == 1) {
          win_at = i;
        }
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_THRONE) {
          throne_at = i;
          if (pop.queue[i].payload != 1 ||
              !strstr(pop.queue[i].body, "go your own way")) {
            assets_msg_free(&game_txt);
            free(emap.terrain);
            free(emap.layer2);
            free(emap.layer3);
            return fail("rev-win throne audience should carry @KINGLOSE, payload 1");
          }
        }
      }
      if (win_at < 0 || throne_at < 0 || throne_at < win_at) {
        fprintf(stderr, "unit_ai_king: rev-win order win_at=%d throne_at=%d\n", win_at,
                throne_at);
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("rev-win: @WINNING must precede the @KINGLOSE throne audience");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution win (1850) ok\n");
  }

  /* Wartime 1850 stalemate: crown still alive → @RETIRING2 + unknown46[4]=2. */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1850;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("retiring2 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25;
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25;
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      c->population = 8;
      snprintf(c->name, sizeof(c->name), "Jamestown");
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    units_reset(&eu);
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1; /* crown still in the field */
      eu.unit_count = 1;
    }
    /* DOS win gate (bugs.md 261): keep the REF pool stocked so the
     * exhaustion win cannot preempt the 1850 war-weariness loss. */
    end.head.expeditionary_force[0] = 5;

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("retiring2: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 2) {
      fprintf(stderr, "unit_ai_king: retiring2 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("1850 + crown alive should latch revolution lost via @RETIRING2");
    }
    if (ai_king_latch_get(&end, 1) != 0) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("@RETIRING2 should clear REF-present");
    }
    if (!strstr(estatus, "sues for peace") && !strstr(estatus, "War-weary")) {
      fprintf(stderr, "unit_ai_king: retiring2 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("1850 stalemate should set @RETIRING2 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            (strstr(pop.queue[i].body, "sues for peace") ||
             strstr(pop.queue[i].body, "War-weary")) &&
            strstr(pop.queue[i].body, "Washington") &&
            strstr(pop.queue[i].body, "Jamestown")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("1850 stalemate should enqueue @RETIRING2 INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution retiring2 (1850 stalemate) ok\n");
  }

  /* Peacetime year≥1800: latch PEACE_1800 + @SCORED CHOICE; That's all → @RETIRING. */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 0);
    end.head.game_options.woi = 0;
    ai_king_latch_set(&end, 1, 0);
    end.head.game_options.ref_present = 0;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1800;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    end.nation[0].liberty_bells_total = 0; /* no declare */
    end.head.colony_count = 0;

    ColonizeColonyPool cp;
    colonies_init(&cp);
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      c->population = 6;
      snprintf(c->name, sizeof(c->name), "Jamestown");
      cp.colony_count = 1;
    }
    ColonizeUnitPool eu;
    units_reset(&eu);

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      return fail("scored: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 3) {
      fprintf(stderr, "unit_ai_king: scored endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      return fail("year≥1800 peacetime should latch PEACE_1800");
    }
    if (!strstr(estatus, "Scoring for this game")) {
      fprintf(stderr, "unit_ai_king: scored status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      return fail("1800 end should set @SCORED status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind != AI_POPUP_KIND_CHOICE ||
            pop.queue[i].tag != AI_POPUP_TAG_KING_SCORED) {
          continue;
        }
        if (!strstr(pop.queue[i].body, "Scoring for this game")) {
          continue;
        }
        if (pop.queue[i].choice_count < 2) {
          continue;
        }
        if (!strstr(pop.queue[i].choices[0], "That's all") ||
            !strstr(pop.queue[i].choices[1], "Keep playing")) {
          continue;
        }
        found = 1;
        break;
      }
      if (!found) {
        assets_msg_free(&game_txt);
        return fail("1800 end should enqueue @SCORED CHOICE");
      }
    }
    const int q_before_apply = pop.queue_count;
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_tag = AI_POPUP_TAG_KING_SCORED;
    pop.result_choice_id = 0; /* That's all */
    pop.result_nation_a = 0;
    ai_king_apply_popup_result(&ectx, &pop);
    if (!strstr(estatus, "steps down") || !strstr(estatus, "loyal service") ||
        !strstr(estatus, "Washington") || !strstr(estatus, "Jamestown")) {
      fprintf(stderr, "unit_ai_king: scored apply status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      return fail("@SCORED That's all should set @RETIRING status");
    }
    {
      int found = 0;
      for (int i = q_before_apply; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "steps down") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        return fail("@SCORED That's all should enqueue @RETIRING INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "unit_ai_king: peacetime @SCORED/@RETIRING (1800) ok\n");
  }

  /* Peacetime Spring 1790: @SOONRETIRING0 once (unknown46[8]). */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 0);
    end.head.game_options.woi = 0;
    ai_king_latch_set(&end, 4, 0);
    ai_king_latch_set(&end, 8, 0);
    end.head.year = 1790;
    end.head.difficulty = 0;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    end.nation[0].liberty_bells_total = 0;
    end.head.colony_count = 0;

    ColonizeColonyPool cp;
    colonies_init(&cp);
    ColonizeUnitPool eu;
    units_reset(&eu);

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      return fail("soon0: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    uint16_t autumn = 0; /* spring */
    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.game_autumn = &autumn;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 8) != 1) {
      assets_msg_free(&game_txt);
      return fail("1790 spring should set unknown46[8] @SOONRETIRING0 latch");
    }
    if (!strstr(estatus, "retire in 1800")) {
      fprintf(stderr, "unit_ai_king: soon0 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      return fail("1790 should set @SOONRETIRING0 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "retire in 1800") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        return fail("1790 should enqueue @SOONRETIRING0 INFO OK");
      }
    }
    {
      const int q0 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      int re = 0;
      for (int i = q0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "retire in 1800")) {
          re = 1;
          break;
        }
      }
      if (re) {
        assets_msg_free(&game_txt);
        return fail("@SOONRETIRING0 must not re-enqueue while latched");
      }
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "unit_ai_king: peacetime @SOONRETIRING0 (1790) ok\n");
  }

  /* Wartime 1840: @SOONRETIRING1 once (unknown46[9]). */
  {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    ai_king_latch_set(&end, 9, 0);
    end.head.year = 1840;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("soon1 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25;
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25;
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    units_reset(&eu);
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1;
      eu.unit_count = 1;
    }
    /* DOS win gate (bugs.md 261): stocked pool keeps the war live at 1840. */
    end.head.expeditionary_force[0] = 5;

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("soon1: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 9) != 1) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("1840 WoI should set unknown46[9] @SOONRETIRING1 latch");
    }
    if (!strstr(estatus, "weary of this long war") && !strstr(estatus, "1850")) {
      fprintf(stderr, "unit_ai_king: soon1 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("1840 should set @SOONRETIRING1 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "weary of this long war") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("1840 should enqueue @SOONRETIRING1 INFO OK");
      }
    }
    {
      const int q0 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      int re = 0;
      for (int i = q0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "weary of this long war")) {
          re = 1;
          break;
        }
      }
      if (re) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("@SOONRETIRING1 must not re-enqueue while latched");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: wartime @SOONRETIRING1 (1840) ok\n");
  }

  /*
   * FUN_43f7_2244: peacetime AI-nation self/ally-funded troop gift
   * (ai_king_ai_peacetime_gift) — implemented 2026-08-14, see king_ref.md
   * "2244/2022 — corrected". Deterministic seed=13 hits both the 1-in-21
   * gate and rolls beneficiary==nation_id (self-gift) on its first two
   * calls (probed empirically, same small-seed-first-roll convention used
   * elsewhere in this file). AI-only: never fires post-WoI or for the
   * human nation (not exercised here since the caller itself, not this
   * function, is what skips the human — see ai.c's ai_euro_nation_turn).
   */
  {
    ai_king_latch_set(&col1, 0, 0); /* peacetime */
    col1.head.game_options.woi = 0;
    ColonizeDosRng gift_rng;
    dos_rng_seed(&gift_rng, 13u);
    ctx.rng = &gift_rng;
    col1.nation[1].gold = 1000000;
    colonies.colonies[0].active = true;
    colonies.colonies[0].nation_id = 1;
    colonies.colonies[0].population = 3;
    const int gift_units_before = count_nation(&units, 1);
    const uint32_t gift_gold_before = col1.nation[1].gold;
    ai_king_ai_peacetime_gift(&ctx, 1);
    if (col1.nation[1].gold >= gift_gold_before) {
      return fail("2244 self-gift (seed=13) should spend gold from the acting nation");
    }
    if (count_nation(&units, 1) <= gift_units_before) {
      return fail("2244 self-gift (seed=13) should land at least one unit");
    }
    fprintf(stderr, "unit_ai_king: 2244 peacetime AI self-gift ok\n");

    /* Post-WoI: must no-op even on the same hit-shaped seed. */
    ai_king_latch_set(&col1, 0, 1);
    col1.head.game_options.woi = 1;
    dos_rng_seed(&gift_rng, 13u);
    col1.nation[1].gold = 1000000;
    const uint32_t gift_gold_before2 = col1.nation[1].gold;
    ai_king_ai_peacetime_gift(&ctx, 1);
    if (col1.nation[1].gold != gift_gold_before2) {
      return fail("2244 must no-op once WoI is declared");
    }
    ai_king_latch_set(&col1, 0, 0);
    col1.head.game_options.woi = 0;
    ctx.rng = NULL; /* restore — later code in this test assumes no RNG */
  }

  /* FUN_4345_0a22 wartime spend: bell pool → intervention when REF absent. */
  {
    founding_fathers_reset();
    col1.head.game_options.woi = 1;
    col1.head.game_options.ref_present = 0;
    /* Fresh scenario: DOS 0a22 spends once per game (0x5382 bit2) — earlier
     * subtests already announced; reset the latch. */
    ai_king_latch_set(&col1, AI_KING_INTERVENE_ANNOUNCED_BYTE, 0);
    col1.head.difficulty = 2;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 3;
    col1.head.backup_force[1] = 2;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    colonies.colonies[0].nation_id = 0;
    founding_fathers_accrue_bells(0, 2u * 0x5dcu + 2000u);

    const int intervene_before = count_nation(&units, 0);
    const unsigned pool_before = founding_fathers_bells_since_last_elect(0);
    if (pool_before < founding_fathers_bells_needed(&col1, 0)) {
      return fail("WoI bell spend setup pool below threshold");
    }
    if (!ai_king_spend_woi_bell_pool(&ctx, 0)) {
      return fail("ai_king_spend_woi_bell_pool should succeed when REF absent");
    }
    founding_fathers_consume_woi_bell_pool(0);
    if (founding_fathers_bells_since_last_elect(0) != 0u) {
      return fail("consume_woi_bell_pool must zero side-table pool");
    }
    if (founding_fathers_intervention_bells(0) != 1u) {
      return fail("consume_woi_bell_pool must increment intervention_bells");
    }
    if (count_nation(&units, 0) <= intervene_before) {
      return fail("WoI bell spend should spawn foreign intervention");
    }
    col1.head.game_options.woi = 0;
    col1.head.game_options.ref_present = 0;
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    founding_fathers_reset();
    fprintf(stderr, "unit_ai_king: WoI bell pool intervention spend ok\n");
  }

  const uint8_t tax_final = col1.nation[0].tax_rate;
  const int crown_final = count_nation(&units, 1);
  const int intervene_final = count_nation(&units, 2);
  const int boycott_final = ai_king_latch_get(&col1, 2);
  const int merc_final = ai_king_latch_get(&col1, 3);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  col1_save_free(&col1);
  fprintf(stderr,
          "unit_ai_king: ok (sol=%d tax=%u crown=%d intervene=%d boycott=%d merc=%d "
          "1eca=colony-SoL popups)\n",
          sol, tax_final, crown_final, intervene_final, boycott_final, merc_final);
  return 0;
}
