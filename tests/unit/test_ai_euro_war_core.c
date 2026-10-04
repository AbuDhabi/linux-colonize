/* Slice of the former tests/unit/test_ai_euro_war.c (split by feature 2026-09-23):
 * mercenary hire, 5952 labor/AI flags, peace and goal tails, g-stance priorities. */
#include "test_ai_euro_war_common.h"

/*
 * FUN_5952_035e labor_shortage (+0x8e) and the +0x1b flag byte, ported
 * 2026-09-09 (smell audit #38/#41; raw 94029-94071 and 94141-94199).
 *
 * Colony nation 1 at (5,5), population 3, nothing hostile anywhere:
 *   threat = 0 → garrison_quota = 0
 *   n = 3 + 0 outside → want = max((3-1)/2, 0) = 1, clamped to n/2 = 1
 *   labor_shortage = 1; no military on the tile → want stays 1
 *   → +0x1b bit 0x40 (NEEDS_GARRISON) SET at zero threat, which the retired
 *     `garrison_quota > 0` substitution could never do.
 * Then the same fixture with a Soldier standing in the town: the on-tile
 * walk decrements want to 0 → the bit is clear, while +0x8e still records
 * the pre-decrement 1 (DOS stamps it before the walk).
 * Finally: an imported +0x1b with every bit set is cleared to `& 7` by the
 * tick, so 0x08/0x10/0x20/0x80 cannot survive frozen from a DOS save while
 * 0x01/0x02/0x04 do.
 */
static int unit_labor_shortage_and_ai_flags_5952(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("labor-shortage alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldiers");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 5, 5, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.tribe_count = 0;
  col1.tribe = NULL;

  uint32_t turn = 20;
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

  /* 1. Empty town, zero threat → labor_shortage 1, NEEDS_GARRISON set. */
  c->labor_shortage = 0;
  c->ai_flags = 0;
  ai_goals_reset();
  ai_euro_dispatcher_turn(&ctx, nation);
  if (colonies.colonies[0].labor_shortage != 1 ||
      (colonies.colonies[0].ai_flags & COLONIZE_COLONY_AI_NEEDS_GARRISON) == 0) {
    fprintf(
      stderr,
      "unit_ai_euro_war: labor_shortage=%u ai_flags=0x%02x (want 1 / bit 0x40)\n",
      (unsigned)colonies.colonies[0].labor_shortage,
      (unsigned)colonies.colonies[0].ai_flags
    );
    rc = fail("FUN_5952_035e labor_shortage seed: expected 1 + NEEDS_GARRISON");
  }

  /* 2. A Soldier standing in the town consumes the shortage → bit clear. */
  if (rc == 0) {
    const int sid = units_spawn_allow_stack(&units, 0, 5, 5);
    ColonizeUnit* s = units_get(&units, sid);
    if (!s) {
      rc = fail("labor-shortage soldier spawn");
    } else {
      s->nation_id = nation;
      s->moves = 0;
      colonies.colonies[0].labor_shortage = 0;
      colonies.colonies[0].ai_flags = 0;
      ai_goals_reset();
      ai_euro_dispatcher_turn(&ctx, nation);
      if ((colonies.colonies[0].ai_flags & COLONIZE_COLONY_AI_NEEDS_GARRISON) != 0) {
        fprintf(
          stderr,
          "unit_ai_euro_war: garrisoned ai_flags=0x%02x (want bit 0x40 clear)\n",
          (unsigned)colonies.colonies[0].ai_flags
        );
        rc = fail("on-tile military must consume the labor_shortage garrison bit");
      }
      units_despawn(&units, sid);
    }
  }

  /* 3. raw 94142 `+0x1b &= 7`: an imported byte keeps 0x01/0x02/0x04 only. */
  if (rc == 0) {
    colonies.colonies[0].ai_flags = 0xffu;
    ai_goals_reset();
    ai_euro_dispatcher_turn(&ctx, nation);
    const unsigned af = (unsigned)colonies.colonies[0].ai_flags;
    /* 0x20 has no writer in the port, so `& 7` must leave it clear; 0x04 is
     * inside the preserved mask and survives (DOS spends it at its consumers,
     * not here); 0x08/0x40 are whatever this tick recomputed. */
    if ((af & 0x20u) != 0 || (af & 0x04u) == 0) {
      fprintf(stderr, "unit_ai_euro_war: post-clear ai_flags=0x%02x\n", af);
      rc = fail("+0x1b clear must be `& 7`: 0x20 dropped, 0x04 preserved");
    }
  }

  fx_map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_ai_euro_war: FUN_5952_035e labor_shortage + flag byte ok\n");
  }
  return rc;
}

/*
 * No conjured TOOLS (smell audit #39). An AI Pioneer standing in its own
 * tools-short colony used to hand it +10 stock[TOOLS] out of nothing on every
 * dispatcher pass, cited to "5b66 case 7" — which is Found Colony and touches
 * no stock at all. DOS's real answers (Pioneer absorption banking the unit's
 * own tools byte; the colony tick's gold-funded +20 Europe purchase) are
 * conserved or paid for, and neither is this. With no wagon on the tile the
 * warehouse must not move.
 */
static int unit_pioneer_conjures_no_tools(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("no-conjure alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 5, 5, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 0; /* tools_short, and no carrier in sight */
  c->stock[COLONIZE_CARGO_MUSKETS] = 0;

  const int pid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* p = units_get(&units, pid);
  if (!p) {
    fx_map_free(&map);
    return fail("no-conjure spawn");
  }
  p->nation_id = nation;
  p->moves = UNITS_MP_PER_TILE;
  p->orders = 0;
  p->tools = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.tribe_count = 0;
  col1.tribe = NULL;

  uint32_t turn = 20;
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

  ai_goals_reset();
  ai_euro_dispatcher_turn(&ctx, nation);

  int rc = 0;
  if (colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] != 0) {
    fprintf(
      stderr, "unit_ai_euro_war: conjured tools=%d\n",
      colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS]
    );
    rc = fail("Pioneer on own colony must not create TOOLS out of nothing");
  }

  fx_map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_ai_euro_war: no conjured tools ok\n");
  }
  return rc;
}

/*
 * Sticky CONTACT re-hunt gates (smell audit #37). Nation 1's Soldier stands
 * next to a nation 0 Free Colonist, both at PEACE and with no signed treaty
 * (relation 0) — exactly the state smell #106 made the foe picker admit, so
 * `ai_euro_land_best_adjacent_foe` returns the neighbour. The act-tail
 * re-hunt used to run for any land unit with moves left, which turned that
 * into a @SNEAK war opening; it now carries the same
 * `at_war_land && is_land_hunter && !fortified` gates as the sibling block
 * above it, so at peace nothing happens.
 */
static int unit_peace_tail_does_not_open_war(void) {
  const int nation = 1;
  const int foe_nation = 0;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-tail alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldiers");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 5, 5, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int mine = units_spawn(&units, 0, 8, 8);
  const int theirs = units_spawn(&units, 0, 9, 8);
  ColonizeUnit* m = units_get(&units, mine);
  ColonizeUnit* t = units_get(&units, theirs);
  if (!m || !t) {
    fx_map_free(&map);
    return fail("peace-tail spawn");
  }
  m->nation_id = nation;
  m->moves = UNITS_MP_PER_TILE;
  m->orders = 0;
  t->nation_id = foe_nation;
  t->moves = 0;
  t->orders = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  /* relation 0 on both sides: met, no signed treaty, NOT at war. */
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.tribe_count = 0;
  col1.tribe = NULL;

  uint32_t turn = 20;
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

  ai_goals_reset();
  ai_euro_dispatcher_turn(&ctx, nation);

  int rc = 0;
  if (ai_diplo_at_war(&col1, nation, foe_nation)) {
    rc = fail("peace act tail must not open a war on an adjacent Euro");
  } else {
    const ColonizeUnit* survivor = units_get_const(&units, theirs);
    if (!survivor || !survivor->active) {
      rc = fail("peace act tail must not attack an adjacent Euro");
    }
  }

  fx_map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_ai_euro_war: peace act-tail re-hunt gated ok\n");
  }
  return rc;
}

/*
 * bugs.md #521 — defended-colony assault through the goal-consumption tail.
 * A land unit carrying a MILITARY goal on a defended foreign colony must
 * resolve its goto step as an attack: FUN_465b_0000 fights the tile's BEST
 * DEFENDER (FUN_5fef_0000), so neither a berthed foreign hull on the colony
 * tile nor the garrison behind it may read as "nothing to attack" and leave
 * the unit oscillating beside its own target (the REF stall in
 * golden_woi_ref01). Drive ai_euro_act_land_goal_dispatch directly: only
 * the drain loop's own step can produce the attack.
 */
static int unit_goal_tail_assaults_defended_colony(void) {
  const int nation = 1;
  const int foe = 0;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("defended-assault alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regulars");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 7;
  units.types[0].defense = 5;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Merchantman");
  units.types[1].movement = 5;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].attack = 0;
  units.types[1].defense = 6;
  units.types[1].cargo = 4;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* enemy = fx_colony_add(&colonies, foe, 9, 8, 3);
  enemy->stock[COLONIZE_CARGO_FOOD] = 20;

  /* Garrison first, hull second: the hull is the head of the tile stack. */
  const int def_id = units_spawn(&units, 0, 9, 8);
  const int hull_id = units_spawn_allow_stack(&units, 1, 9, 8);
  const int atk_id = units_spawn(&units, 0, 8, 8);
  ColonizeUnit* d = units_get(&units, def_id);
  ColonizeUnit* h = units_get(&units, hull_id);
  ColonizeUnit* a = units_get(&units, atk_id);
  if (!d || !h || !a) {
    fx_map_free(&map);
    return fail("defended-assault spawn");
  }
  d->nation_id = foe;
  d->moves = 0;
  h->nation_id = foe;
  h->moves = 0;
  a->nation_id = nation;
  a->moves = 1 * UNITS_MP_PER_TILE;
  a->orders = UNITS_ORDER_AI_MOVE;
  a->goto_x = 9;
  a->goto_y = 8;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.tribe_count = 0;
  col1.tribe = NULL;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    fx_map_free(&map);
    return fail("defended-assault expected war after declare");
  }

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.human_nation = foe;
  ctx.rng_seed = 4242;

  struct ai_euro_act_ctx act;
  memset(&act, 0, sizeof(act));
  act.ctx = &ctx;
  act.u = a;
  act.nation_id = nation;
  act.is_ship = 0;
  act.uname = units_display_name(&units, a);
  act.goal_code = AI_GOAL_MILITARY;
  act.goal_x = 9;
  act.goal_y = 8;

  ai_goals_reset();
  ai_euro_reset();
  (void)ai_euro_act_land_goal_dispatch(&act);

  int rc = 0;
  const ColonizeUnit* atk_after = units_get_const(&units, atk_id);
  const ColonizeUnit* def_after = units_get_const(&units, def_id);
  /* The step must have resolved as combat on (9,8): either side may fall,
   * but the attacker must not have wandered off to some other neighbour. */
  const int engaged =
    !atk_after || !atk_after->active || !def_after || !def_after->active ||
    (atk_after->x == 9 && atk_after->y == 8);
  if (!engaged) {
    rc = fail("MILITARY goto step onto a defended colony must resolve as an attack");
  }

  fx_map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_ai_euro_war: goal tail assaults defended colony ok\n");
  }
  return rc;
}

static const TestCase k_cases[] = {
    {"unit_labor_shortage_and_ai_flags_5952", unit_labor_shortage_and_ai_flags_5952},
    {"unit_pioneer_conjures_no_tools", unit_pioneer_conjures_no_tools},
    {"unit_peace_tail_does_not_open_war", unit_peace_tail_does_not_open_war},
    {"unit_goal_tail_assaults_defended_colony", unit_goal_tail_assaults_defended_colony},
};
TEST_MAIN(k_cases)
