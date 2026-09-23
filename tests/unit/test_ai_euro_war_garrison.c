/* Slice of the former tests/unit/test_ai_euro_war.c (split by feature 2026-09-23):
 * fortify/garrison quota arms, per-type peace fortify. */
#include "test_ai_euro_war_common.h"

/*
 * Peace fortify Soldier on colony wakes when foreign Euro land unit enters MD≤2.
 * Cite: euro_unit_act §2d3 peace colony-defense wake; units_wake.
 */
/*
 * (Deleted 2026-09-23, bugs.md #760.) Five "peace border wake" tests lived
 * here (Soldier / Dragoon / Regular / Continental Army / Continental Cavalry).
 * They pinned the invented peace colony-defence wake arm that has been removed
 * from ai_euro.c: FUN_521d_20e6 (raw 88266-90445) has no MD<=2 foreign-threat
 * wake; the only own-colony garrison handling is the LAB_5899 arm
 * (raw 88584-88612).
 */

/*
 * Peace fortified Dragoon on colony wakes when foreign Euro land unit enters
 * MD≤2 (same as Soldier). Cite: Colonization.pdf Defending a Colony (fortify
 * soldiers, dragoons…); euro_unit_act §2d3; units_wake.
 */


/*
 * Peace fortified Regular on colony wakes when foreign Euro land unit enters
 * MD≤2. Cite: Colonization.pdf Defending a Colony; euro_unit_act §2d3.
 */

/*
 * 5d04 treasury: at war, prefer Artillery but gold < Europe purchase 500$ →
 * fall back to Soldier hire (hire_cost), not unpaid Artillery fiction.
 */

/*
 * Peace fortified Continental Cavalry on colony wakes when foreign Euro land unit enters
 * MD≤2. Cite: Colonization.pdf Defending a Colony; euro_unit_act §2d3.
 */

/*
 * Peace fortified Continental Army on colony wakes when foreign Euro land unit enters
 * MD≤2. Cite: Colonization.pdf Defending a Colony; euro_unit_act §2d3.
 */

/*
 * 5d04 treasury: at war, prefer Artillery but gold < Europe purchase 500$ →
 * fall back to Soldier hire (hire_cost), not unpaid Artillery fiction.
 */

/*
 * Peace fortified Continental Cavalry on colony wakes when foreign Euro land unit enters
 * MD≤2. Cite: Colonization.pdf Defending a Colony; euro_unit_act §2d3.
 */

/*
 * Idle fortified Soldier at war → wake (clear fortify) and hunt toward foe.
 * Cite: units_wake; euro_unit_act §2c sentry/fortify wake.
 */
static int unit_fortify_wake_hunt(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("wake-hunt alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 2;
  own->y = 2;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  ColonizeColony* enemy_col = &colonies.colonies[1];
  enemy_col->id = 1;
  enemy_col->active = true;
  enemy_col->nation_id = foe;
  enemy_col->x = 10;
  enemy_col->y = 5;
  enemy_col->population = 2;
  enemy_col->colonist_count = 2;
  colonies.colony_count = 2;

  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("wake-hunt spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = UNITS_ORDER_FORTIFIED;
  soldier->moves = 3 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset();

  uint32_t turn = 22;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int x0 = soldier->x;
  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  if (!soldier || !soldier->active) {
    fx_map_free(&map);
    fprintf(stderr, "unit_ai_euro_war: fortify-wake ok (combat despawn)\n");
    return 0;
  }

  const int woken = soldier->orders != UNITS_ORDER_FORTIFIED &&
                    soldier->orders != UNITS_ORDER_FORTIFY;
  const int hunting =
    units_orders_follow_goto(soldier->orders) || soldier->x != x0;
  if (!woken || !hunting) {
    fprintf(
      stderr,
      "unit_ai_euro_war: wake orders=%d goto=(%d,%d) pos=(%d,%d) x0=%d\n",
      soldier->orders,
      soldier->goto_x,
      soldier->goto_y,
      soldier->x,
      soldier->y,
      x0
    );
    fx_map_free(&map);
    return fail("expected fortified Soldier to wake and hunt at war");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: fortify-wake hunt ok\n");
  return 0;
}

/*
 * Peace: idle Soldier on own colony → FORTIFY. Cite: euro_unit_act §2d3.
 */

/*
 * Col1 +0x1e garrison_quota: the DOS-LITERAL 'F' arm (FUN_521d_20e6 raw
 * 89011-89031) has no quota concept, so with quota=1 BOTH idle Soldiers on
 * the colony tile fortify; the quota is merely decremented once (1 -> 0) so
 * the other +0x1e readers see a garrison. The old expectation — one fortify,
 * the second Soldier admitted into the colony because quota had reached 0 —
 * encoded the invented quota-0 admit arm deleted 2026-09-18 (sibling of
 * bugs.md #512); DOS's 20e6 has no join-colony outcome at all (its whole
 * +0x314b vocabulary is 0x39/0x3d/0x40/0x42/0x46/0x47/0x4c/0x56/0x65).
 *
 * 2026-09-08: the quota is no longer preset by the fixture — presetting it is
 * futile now that the colony tick recomputes +0x1e from the real
 * FUN_5952_035e threat accumulator every pass. It is produced instead by a
 * lone foreign (nation 2, AI-controlled so no ×1.5 human bump) Soldier
 * standing 4 tiles east: v = attack(2) × 8 = 16, dist = dos_dist(4,0) = 4,
 * contribution = (8 − 4) × 16 >> 3 = 8, no walls → quota = 8 >> 3 = 1.
 */
static int unit_garrison_quota_one_fortify(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("garrison-quota alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 5;
  c->y = 5;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  c->garrison_quota = 0; /* seeded by the FUN_5952_035e threat accumulator */
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid0 = units_spawn_allow_stack(&units, 0, 5, 5);
  const int uid1 = units_spawn_allow_stack(&units, 0, 5, 5);
  ColonizeUnit* s0 = units_get(&units, uid0);
  ColonizeUnit* s1 = units_get(&units, uid1);
  if (!s0 || !s1) {
    fx_map_free(&map);
    return fail("garrison-quota spawn");
  }
  s0->nation_id = nation;
  s0->moves = 1 * UNITS_MP_PER_TILE;
  s0->orders = 0;
  s1->nation_id = nation;
  s1->moves = 1 * UNITS_MP_PER_TILE;
  s1->orders = 0;

  /* Threat source for the FUN_5952_035e seed: foreign Soldier 4 tiles east. */
  const int uid2 = units_spawn_allow_stack(&units, 0, 9, 5);
  ColonizeUnit* foe = units_get(&units, uid2);
  if (!foe) {
    fx_map_free(&map);
    return fail("garrison-quota foe spawn");
  }
  foe->nation_id = 2;
  foe->moves = 0;
  foe->orders = 0;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 55;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.human_nation = 0; /* nation 2 is AI → no DS:0x543f ×1.5 human bump */
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  s0 = units_get(&units, uid0);
  s1 = units_get(&units, uid1);
  c = &colonies.colonies[0];
  int fortified = 0;
  int idle = 0;
  int joined = 0;
  for (int i = 0; i < 2; ++i) {
    const ColonizeUnit* s = (i == 0) ? s0 : s1;
    if (!s || !s->active) {
      joined++; /* must not happen: no DOS admit arm */
      continue;
    }
    if (s->orders == UNITS_ORDER_FORTIFY || s->orders == UNITS_ORDER_FORTIFIED) {
      fortified++;
    } else {
      idle++;
    }
  }
  /*
   * Both fortify (quota does not gate the 'F' arm). Since bugs.md #554 the
   * fortify comes from FUN_521d_20e6's own stay tail LAB_521d_589e
   * (raw 90378-90386, +0x314c = 5), which runs before the port's peace
   * fortify fallback; that fallback is the only consumer of garrison_quota
   * (+0x1e) and it skips an already-fortified unit, so the quota seed stays 1.
   */
  if (fortified != 2 || c->garrison_quota != 1 || idle != 0 || joined != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_war: garrison_quota fortified=%d idle=%d joined=%d quota=%u\n",
      fortified,
      idle,
      joined,
      (unsigned)c->garrison_quota
    );
    fx_map_free(&map);
    return fail("expected both Soldiers fortified and garrison_quota untouched");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: garrison_quota fortify ok\n");
  return 0;
}

/*
 * bugs.md #554 — FUN_521d_20e6's own-colony garrison stay branch
 * (raw 88584-88612) must exit through the shared tail LAB_521d_589e
 * (raw 90378-90386): with `local_76 == 8` DOS writes +0x314f = 8 and forces
 * +0x314c into the fortify family (5, or 6 when +0x3148 & 2 is set). The port
 * only stamped +0x314b = 'G' (0x47), so a garrison unit that still carried a
 * stale AI_MOVE course kept being dispatched by the goal drain, which keys on
 * orders alone. DOS does NOT clear +0x314d/e here, so the goto tile survives;
 * the act_state is what unbinds the unit.
 *
 * Lone Soldier standing on its own colony with a stale goto across the map,
 * full MP (so the gate is entered at all): it must stay on the colony tile in
 * the fortify family.
 */
static int unit_garrison_stay_clears_stale_goto(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("stay-tail alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 5;
  c->y = 5;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  c->labor_shortage = 2; /* +0x8e >= 1: the garrison arm is entered */
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn_allow_stack(&units, 0, 5, 5);
  ColonizeUnit* s0 = units_get(&units, uid);
  if (!s0) {
    fx_map_free(&map);
    return fail("stay-tail spawn");
  }
  s0->nation_id = nation;
  s0->moves = 1 * UNITS_MP_PER_TILE; /* fresh allotment -> gate is entered */
  s0->orders = UNITS_ORDER_GOTO;     /* stale course */
  s0->col1_ai_plan = 'A';            /* +0x314b == 'A': DOS goto LAB_521d_5899 */
  s0->goto_x = 12;
  s0->goto_y = 12;

  /*
   * A distant Brave puts the nation in the at-war land band, so the port's
   * peace fortify fallback (the other arm that could fortify this unit) is
   * out of the way and the assertion sees LAB_589e's write alone.
   */
  const int bid = units_spawn_allow_stack(&units, 0, 12, 12);
  ColonizeUnit* brave = units_get(&units, bid);
  if (!brave) {
    fx_map_free(&map);
    return fail("stay-tail brave spawn");
  }
  brave->nation_id = 4;
  brave->moves = 0;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 55;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.human_nation = 0;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  s0 = units_get(&units, uid);
  if (!s0 || !s0->active) {
    fx_map_free(&map);
    return fail("stay-tail unit vanished");
  }
  if (s0->col1_ai_plan != 0x47) {
    fprintf(stderr, "unit_ai_euro_war: stay-tail plan=%d\n", (int)s0->col1_ai_plan);
    fx_map_free(&map);
    return fail("expected the LAB_5888 garrison stay branch (+0x314b = 0x47)");
  }
  if (s0->orders != UNITS_ORDER_FORTIFY && s0->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(stderr, "unit_ai_euro_war: stay-tail orders=%d pos=(%d,%d)\n",
            (int)s0->orders, s0->x, s0->y);
    fx_map_free(&map);
    return fail("LAB_589e stay tail must leave +0x314c in the fortify family");
  }
  if (s0->x != 5 || s0->y != 5) {
    fprintf(stderr, "unit_ai_euro_war: stay-tail walked to (%d,%d)\n", s0->x, s0->y);
    fx_map_free(&map);
    return fail("garrison stay must not follow the stale goto");
  }

  /*
   * The tail itself, straight from raw 90378-90386: a stale AI_MOVE course
   * becomes 5, an already-fortified unit is left alone, the +0x3148 & 2
   * (roam re-evaluate) unit becomes 6, and the goto tile is never touched —
   * DOS writes +0x314d/e only on the `local_76 != 8` arm.
   */
  s0->orders = UNITS_ORDER_AI_MOVE;
  s0->goto_x = 12;
  s0->goto_y = 12;
  s0->col1_flags15 = 0;
  ai_euro_20e6_stay_tail_589e(s0);
  if (s0->orders != UNITS_ORDER_FORTIFY || s0->last_dir != 8) {
    fprintf(stderr, "unit_ai_euro_war: tail orders=%d dir=%d\n", (int)s0->orders,
            (int)s0->last_dir);
    fx_map_free(&map);
    return fail("LAB_589e must write +0x314c = 5 and +0x314f = 8");
  }
  if (s0->goto_x != 12 || s0->goto_y != 12) {
    fx_map_free(&map);
    return fail("LAB_589e stay arm must not touch +0x314d/e");
  }
  s0->orders = UNITS_ORDER_FORTIFIED;
  ai_euro_20e6_stay_tail_589e(s0);
  if (s0->orders != UNITS_ORDER_FORTIFIED) {
    fx_map_free(&map);
    return fail("LAB_589e must keep an existing 6");
  }
  s0->orders = UNITS_ORDER_AI_MOVE;
  s0->col1_flags15 = 0x02; /* +0x3148 & 2 */
  ai_euro_20e6_stay_tail_589e(s0);
  if (s0->orders != UNITS_ORDER_FORTIFIED) {
    fx_map_free(&map);
    return fail("LAB_589e must write +0x314c = 6 when +0x3148 & 2");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: LAB_589e garrison stay tail ok\n");
  return 0;
}

/*
 * FUN_5952_035e colony threat accumulator → garrison_quota (+0x1e), ported
 * 2026-09-08 (ai_euro_colony_threat_seed_5952; clean recovery
 * original_sources_annotated/ai/colony_tick_5952_035e.md raw 254-324).
 *
 * Colony (nation 1) at (5,5); two hostile Braves at (8,5) and (8,6), both
 * homed to tribe 0. Per Brave: v = attack(2) × 8 = 16; dos_dist(3,0) and
 * dos_dist(3,1) are both 3, so each contributes (8 − 3) × 16 >> 3 = 10.
 * threat = 20, floor = min(20, 0x10) = 0x10, no walls → 20 / 1 = 20 ≥ floor
 * → garrison_quota = 20 >> 3 = 2 (a value the retired "idle Soldier on the
 * tile → 1" latch could never produce).
 *
 * Then the two DOS zeroing gates, each re-run from the same fixture:
 *   - DS:0x54f6 attitude (tribe[home_tribe_id].alarm[euro] word) < 0x80 → 0
 *   - FUN_281f_030c alarm (alarm_by_player) < 0x19 → 0
 */
static int unit_garrison_quota_threat_seed(void) {
  const int nation = 1;
  const int indian = 4;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("threat-seed alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Brave");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 5;
  c->y = 5;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  c->garrison_quota = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int bid0 = units_spawn_allow_stack(&units, 0, 8, 5);
  const int bid1 = units_spawn_allow_stack(&units, 0, 8, 6);
  ColonizeUnit* b0 = units_get(&units, bid0);
  ColonizeUnit* b1 = units_get(&units, bid1);
  if (!b0 || !b1) {
    fx_map_free(&map);
    return fail("threat-seed brave spawn");
  }
  b0->nation_id = indian;
  b0->home_tribe_id = 0;
  b0->moves = 0;
  b1->nation_id = indian;
  b1->home_tribe_id = 0;
  b1->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  col1.stuff.ship_counts[nation] = 1;

  ColonizeCol1Tribe tribes[1];
  memset(tribes, 0, sizeof(tribes));
  tribes[0].x = 12;
  tribes[0].y = 12;
  tribes[0].nation_id = (uint8_t)indian;
  col1.tribe = tribes;
  col1.head.tribe_count = 1;

  uint32_t turn = 55;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.human_nation = 0;
  ctx.rng_seed = 42;

  int rc = 0;

  /* 1. Both gates satisfied → threat 20 → quota 2. */
  col1.indian[indian - 4].alarm_by_player[nation] = 40; /* >= 0x19 */
  /* DS:0x54f6 = the record's attitude[euro] word (friction | attacks<<8).
   * One recorded trespass (attacks = 1) is word 0x100, comfortably over the
   * 0x80 gate without loading the friction byte other gates read. */
  col1_tribe_attitude_set(&tribes[0], nation, 0x100);
  colonies.colonies[0].garrison_quota = 0;
  ai_goals_reset();
  ai_euro_dispatcher_turn(&ctx, nation);
  if (colonies.colonies[0].garrison_quota != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_war: threat seed quota=%u (want 2)\n",
      (unsigned)colonies.colonies[0].garrison_quota
    );
    rc = fail("FUN_5952_035e threat>>3 seed: expected quota 2");
  }

  /* 2. DS:0x54f6 tension below 0x80 zeroes each contribution → quota 0. */
  if (rc == 0) {
    col1.indian[indian - 4].alarm_by_player[nation] = 40;
    col1_tribe_attitude_set(&tribes[0], nation, 0x7f);
    colonies.colonies[0].garrison_quota = 0;
    ai_goals_reset();
    ai_euro_dispatcher_turn(&ctx, nation);
    if (colonies.colonies[0].garrison_quota != 0) {
      fprintf(
        stderr,
        "unit_ai_euro_war: tension 0x7f quota=%u (want 0)\n",
        (unsigned)colonies.colonies[0].garrison_quota
      );
      rc = fail("tension < 0x80 must zero the Indian threat contribution");
    }
  }

  /* 3. FUN_281f_030c alarm below 0x19 zeroes it too → quota 0. */
  if (rc == 0) {
    col1.indian[indian - 4].alarm_by_player[nation] = 0x18;
    col1_tribe_attitude_set(&tribes[0], nation, 0x100); /* one recorded attack */
    colonies.colonies[0].garrison_quota = 0;
    ai_goals_reset();
    ai_euro_dispatcher_turn(&ctx, nation);
    if (colonies.colonies[0].garrison_quota != 0) {
      fprintf(
        stderr,
        "unit_ai_euro_war: alarm 0x18 quota=%u (want 0)\n",
        (unsigned)colonies.colonies[0].garrison_quota
      );
      rc = fail("alarm < 0x19 must zero the Indian threat contribution");
    }
  }

  col1.tribe = NULL;
  fx_map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_ai_euro_war: FUN_5952_035e threat seed ok\n");
  }
  return rc;
}

static int unit_peace_soldier_fortify_colony(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 5, 5, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* sol = units_get(&units, uid);
  if (!sol) {
    fx_map_free(&map);
    return fail("peace-fortify spawn");
  }
  sol->nation_id = nation;
  sol->moves = 1 * UNITS_MP_PER_TILE;
  sol->orders = 0;

  /*
   * Threat source for the live FUN_5952_035e seed: foreign Soldier 4 tiles
   * east keeps garrison_quota at 1 so the fortify path (not the quota-0
   * admit-as-colonist path) is what this test exercises — the retired thin
   * latch used to grant quota 1 to any quiet colony with an idle soldier.
   * MD 4 > 2 so the fortify-wake probe stays out of the picture.
   */
  const int foe_id = units_spawn_allow_stack(&units, 0, 9, 5);
  ColonizeUnit* foe = units_get(&units, foe_id);
  if (!foe) {
    fx_map_free(&map);
    return fail("peace-fortify foe spawn");
  }
  foe->nation_id = 2;
  foe->moves = 0;
  foe->orders = 0;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  /* Peace — no Euro war bits. */
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  sol = units_get(&units, uid);
  if (!sol || !sol->active) {
    fx_map_free(&map);
    return fail("peace-fortify soldier should remain");
  }
  if (sol->orders != UNITS_ORDER_FORTIFY && sol->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(
      stderr,
      "unit_ai_euro_war: peace-fortify orders=%d pos=(%d,%d)\n",
      sol->orders,
      sol->x,
      sol->y
    );
    fx_map_free(&map);
    return fail("expected idle Soldier on colony to FORTIFY at peace");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace soldier fortify colony ok\n");
  return 0;
}

/*
 * Peace Dragoon fortify: idle Dragoon on own colony → FORTIFY (same arm as
 * Soldier). Cite: euro_unit_act §2d3; Colonization.pdf Defending a Colony.
 */
static int unit_peace_dragoon_fortify_colony(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-dragoon-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Dragoon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 5, 5, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* drag = units_get(&units, uid);
  if (!drag) {
    fx_map_free(&map);
    return fail("peace-dragoon-fortify spawn");
  }
  drag->nation_id = nation;
  drag->moves = 4 * UNITS_MP_PER_TILE;
  drag->orders = 0;

  /* Threat source keeping garrison_quota at 1 under the live FUN_5952_035e
   * seed (the retired thin latch granted quota 1 to any quiet colony);
   * MD 4 > 2 keeps the fortify-wake probe out of the picture. */
  const int foe_id = units_spawn_allow_stack(&units, 0, 9, 5);
  ColonizeUnit* foe = units_get(&units, foe_id);
  if (!foe) {
    fx_map_free(&map);
    return fail("peace-dragoon-fortify foe spawn");
  }
  foe->nation_id = 2;
  foe->moves = 0;
  foe->orders = 0;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  drag = units_get(&units, uid);
  if (!drag || !drag->active) {
    fx_map_free(&map);
    return fail("peace-fortify dragoon should remain");
  }
  if (drag->orders != UNITS_ORDER_FORTIFY && drag->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(
      stderr,
      "unit_ai_euro_war: peace-dragoon-fortify orders=%d pos=(%d,%d)\n",
      drag->orders,
      drag->x,
      drag->y
    );
    fx_map_free(&map);
    return fail("expected idle Dragoon on colony to FORTIFY at peace");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace dragoon fortify colony ok\n");
  return 0;
}

/*
 * Peace Regular fortify: idle Regular on own colony → FORTIFY. Cite:
 * euro_unit_act §2d3; Colonization.pdf Defending a Colony.
 */
static int unit_peace_regular_fortify_colony(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-regular-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regular");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 5, 5, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* reg = units_get(&units, uid);
  if (!reg) {
    fx_map_free(&map);
    return fail("peace-regular-fortify spawn");
  }
  reg->nation_id = nation;
  reg->moves = 3 * UNITS_MP_PER_TILE;
  reg->orders = 0;

  /* Threat source: quota 1 under the live FUN_5952_035e seed (see the
   * soldier variant's note). */
  {
    const int foe_id = units_spawn_allow_stack(&units, 0, 9, 5);
    ColonizeUnit* foe = units_get(&units, foe_id);
    if (!foe) {
      fx_map_free(&map);
      return fail("peace-regular-fortify foe spawn");
    }
    foe->nation_id = 2;
    foe->moves = 0;
    foe->orders = 0;
  }

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  reg = units_get(&units, uid);
  if (!reg || !reg->active) {
    fx_map_free(&map);
    return fail("peace-fortify regular should remain");
  }
  if (reg->orders != UNITS_ORDER_FORTIFY && reg->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(
      stderr,
      "unit_ai_euro_war: peace-regular-fortify orders=%d pos=(%d,%d)\n",
      reg->orders,
      reg->x,
      reg->y
    );
    fx_map_free(&map);
    return fail("expected Regular FORTIFY on own colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Regular fortify colony ok\n");
  return 0;
}

/*
 * Peace Continental Army fortify on own colony. Cite: Defending a Colony
 * ("…army, cavalry…"); euro_unit_act §2d3.
 */
static int unit_peace_continental_fortify_colony(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-cont-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Army");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 5, 5, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* army = units_get(&units, uid);
  if (!army) {
    fx_map_free(&map);
    return fail("peace-cont-fortify spawn");
  }
  army->nation_id = nation;
  army->moves = 3 * UNITS_MP_PER_TILE;
  army->orders = 0;

  /* Threat source: quota 1 under the live FUN_5952_035e seed (see the
   * soldier variant's note). */
  {
    const int foe_id = units_spawn_allow_stack(&units, 0, 9, 5);
    ColonizeUnit* foe = units_get(&units, foe_id);
    if (!foe) {
      fx_map_free(&map);
      return fail("peace-cont-fortify foe spawn");
    }
    foe->nation_id = 2;
    foe->moves = 0;
    foe->orders = 0;
  }

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  army = units_get(&units, uid);
  if (!army || !army->active) {
    fx_map_free(&map);
    return fail("peace-fortify continental should remain");
  }
  if (army->orders != UNITS_ORDER_FORTIFY && army->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(
      stderr,
      "unit_ai_euro_war: peace-cont-fortify orders=%d pos=(%d,%d)\n",
      army->orders,
      army->x,
      army->y
    );
    fx_map_free(&map);
    return fail("expected Continental Army FORTIFY on own colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Continental Army fortify colony ok\n");
  return 0;
}

/*
 * Peace Continental Cavalry fortify on own colony. Cite: Defending a Colony
 * ("…cavalry…"); euro_unit_act §2d3.
 */
static int unit_peace_continental_cavalry_fortify_colony(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-cont-cav-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Cavalry");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 5, 5, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* cav = units_get(&units, uid);
  if (!cav) {
    fx_map_free(&map);
    return fail("peace-cont-cav-fortify spawn");
  }
  cav->nation_id = nation;
  cav->moves = 4 * UNITS_MP_PER_TILE;
  cav->orders = 0;

  /* Threat source: quota 1 under the live FUN_5952_035e seed (see the
   * soldier variant's note). */
  {
    const int foe_id = units_spawn_allow_stack(&units, 0, 9, 5);
    ColonizeUnit* foe = units_get(&units, foe_id);
    if (!foe) {
      fx_map_free(&map);
      return fail("peace-cont-cav-fortify foe spawn");
    }
    foe->nation_id = 2;
    foe->moves = 0;
    foe->orders = 0;
  }

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  cav = units_get(&units, uid);
  if (!cav || !cav->active) {
    fx_map_free(&map);
    return fail("peace-fortify continental cavalry should remain");
  }
  if (cav->orders != UNITS_ORDER_FORTIFY && cav->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(
      stderr,
      "unit_ai_euro_war: peace-cont-cav-fortify orders=%d pos=(%d,%d)\n",
      cav->orders,
      cav->x,
      cav->y
    );
    fx_map_free(&map);
    return fail("expected Continental Cavalry FORTIFY on own colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Continental Cavalry fortify colony ok\n");
  return 0;
}

/*
 * Peace Cannon fortify on own colony (Artillery name alias). Cite: Defending a
 * Colony ("…or artillery"); ai_euro_is_artillery_name Cannon; euro_unit_act §2d3.
 */
static int unit_peace_cannon_fortify_colony(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-cannon-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Cannon");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 5, 5, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* art = units_get(&units, uid);
  if (!art) {
    fx_map_free(&map);
    return fail("peace-cannon-fortify spawn");
  }
  art->nation_id = nation;
  art->moves = 1 * UNITS_MP_PER_TILE;
  art->orders = 0;

  /* Threat source: quota 1 under the live FUN_5952_035e seed (see the
   * soldier variant's note). */
  {
    const int foe_id = units_spawn_allow_stack(&units, 0, 9, 5);
    ColonizeUnit* foe = units_get(&units, foe_id);
    if (!foe) {
      fx_map_free(&map);
      return fail("peace-cannon-fortify foe spawn");
    }
    foe->nation_id = 2;
    foe->moves = 0;
    foe->orders = 0;
  }

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  art = units_get(&units, uid);
  if (!art || !art->active) {
    fx_map_free(&map);
    return fail("peace-fortify cannon should remain");
  }
  if (art->orders != UNITS_ORDER_FORTIFY && art->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(
      stderr,
      "unit_ai_euro_war: peace-cannon-fortify orders=%d pos=(%d,%d)\n",
      art->orders,
      art->x,
      art->y
    );
    fx_map_free(&map);
    return fail("expected Cannon FORTIFY on own colony in peace");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Cannon fortify colony ok\n");
  return 0;
}

/*
 * bugs.md #512 — FUN_521d_20e6 raw 89011-89013: the peace 'F' arm gates on the
 * @UNIT ATTACK column (DS:0x5236[type] > 1) and a non-ship type, and spends no
 * garrison quota. A Soldier standing on its own colony whose garrison_quota is
 * already 0 must therefore still FORTIFY — before the fix the quota gate made
 * it fall through and be admitted into the colony as a colonist.
 */
static int unit_peace_fortify_ignores_garrison_quota(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("quota-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 4;
  own->y = 4;
  /* Population 3+ keeps the separate pre-stage "quota 0 + tiny colony" admit
   * arm (ai_euro_act_land_pre) out of this test — the arm under test is the
   * 20e6 'F' arm alone. */
  own->population = 4;
  own->colonist_count = 4;
  own->garrison_quota = 0; /* no slots left under the old quota gate */
  colonies.colony_count = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* soldier = units_get(&units, uid);
  if (!soldier) {
    fx_map_free(&map);
    return fail("quota-fortify spawn");
  }
  soldier->nation_id = nation;
  soldier->orders = UNITS_ORDER_NONE;
  soldier->moves = 3 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  col1.stuff.ship_counts[nation] = 1;

  ai_goals_reset();

  uint32_t turn = 25;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int pop0 = own->population;
  struct ai_euro_act_ctx a;
  memset(&a, 0, sizeof(a));
  a.ctx = &ctx;
  a.u = soldier;
  a.nation_id = nation;
  a.uname = units_display_name(&units, soldier);
  a.goal_code = -1;
  (void)ai_euro_act_land_fortify(&a);

  soldier = units_get(&units, uid);
  if (!soldier || !soldier->active) {
    fx_map_free(&map);
    return fail("Soldier was admitted into the colony instead of fortifying");
  }
  if (own->population != pop0) {
    fx_map_free(&map);
    return fail("colony population changed — the admit arm fired");
  }
  if (soldier->orders != UNITS_ORDER_FORTIFY && soldier->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(stderr, "unit_ai_euro_war: quota-fortify orders=%d\n", soldier->orders);
    fx_map_free(&map);
    return fail("expected FORTIFY with garrison_quota == 0");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace fortify ignores garrison quota ok\n");
  return 0;
}

/*
 * The other half of the same DOS test: attack <= 1 types are NOT fortified by
 * this arm. A Pioneer (DS:0x5236[2] == 1) on its own colony tile must not come
 * out of the turn fortified — the old name-keyed gate happened to agree here,
 * so this pins the new literal ATTACK-column reading.
 */
static int unit_peace_fortify_skips_attack_one_type(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("attack1-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 1;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 4;
  own->y = 4;
  own->population = 2;
  own->colonist_count = 2;
  own->garrison_quota = 4;
  colonies.colony_count = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* pioneer = units_get(&units, uid);
  if (!pioneer) {
    fx_map_free(&map);
    return fail("attack1-fortify spawn");
  }
  pioneer->nation_id = nation;
  pioneer->orders = UNITS_ORDER_NONE;
  pioneer->moves = 3 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  col1.stuff.ship_counts[nation] = 1;

  ai_goals_reset();

  uint32_t turn = 25;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  struct ai_euro_act_ctx a;
  memset(&a, 0, sizeof(a));
  a.ctx = &ctx;
  a.u = pioneer;
  a.nation_id = nation;
  a.uname = units_display_name(&units, pioneer);
  a.goal_code = -1;
  (void)ai_euro_act_land_fortify(&a);

  pioneer = units_get(&units, uid);
  if (pioneer && pioneer->active &&
      (pioneer->orders == UNITS_ORDER_FORTIFY || pioneer->orders == UNITS_ORDER_FORTIFIED)) {
    fx_map_free(&map);
    return fail("attack-1 Pioneer must not take the 20e6 'F' arm");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: attack-1 type skips 'F' arm ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_fortify_wake_hunt", unit_fortify_wake_hunt},
    {"unit_garrison_quota_threat_seed", unit_garrison_quota_threat_seed},
    {"unit_garrison_quota_one_fortify", unit_garrison_quota_one_fortify},
    {"unit_garrison_stay_clears_stale_goto", unit_garrison_stay_clears_stale_goto},
    {"unit_peace_soldier_fortify_colony", unit_peace_soldier_fortify_colony},
    {"unit_peace_dragoon_fortify_colony", unit_peace_dragoon_fortify_colony},
    {"unit_peace_regular_fortify_colony", unit_peace_regular_fortify_colony},
    {"unit_peace_continental_fortify_colony", unit_peace_continental_fortify_colony},
    {"unit_peace_continental_cavalry_fortify_colony", unit_peace_continental_cavalry_fortify_colony},
    {"unit_peace_cannon_fortify_colony", unit_peace_cannon_fortify_colony},
    {"unit_peace_fortify_ignores_garrison_quota", unit_peace_fortify_ignores_garrison_quota},
    {"unit_peace_fortify_skips_attack_one_type", unit_peace_fortify_skips_attack_one_type},
};
TEST_MAIN(k_cases)
