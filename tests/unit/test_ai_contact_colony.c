/* Slice of the former tests/unit/test_ai_contact.c (split by feature 2026-09-23):
 * 5952 colony tick at war, prelude alarm band, missionary village arm. */
#include "test_ai_contact_common.h"

/*
 * FUN_5952_035e war-declare block (ai_contact_colony_tick_war_5952) — the AI
 * colony tick's `or_both(nation, tribe + 4, 2)` (raw viceroy_unpacked.c:94170-
 * 94190, far-call args recovered from viceroy_unpacked.asm LAB_5952_0ac6).
 * Own fixtures, fully memset: the shared main() ones are mutated in place by
 * the raid/menu blocks and cannot be reused for a strength comparison.
 */
static int test_colony_tick_war_5952(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 2;
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    return fail("5952 war: alloc tribe");
  }
  col1.tribe[0].x = 5;
  col1.tribe[0].y = 5;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].population = 4;
  memset(&col1.indian[0], 0, sizeof(col1.indian[0]));
  col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;
  col1.indian[0].alarm_by_player[0] = 40; /* > 0x19 alarm floor */

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("5952 war: alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* one all-land continent */
  }

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Brave");
  units.types[0].movement = 1;
  units.types[0].attack = 2; /* combat_unit_base_x8(mode 1) = 16 */
  units.types[0].defense = 1;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 1;
  units.types[1].attack = 2; /* 16 each */
  units.types[1].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 10;
  c->y = 10;
  c->population = 3;
  c->colonist_count = 3;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int brave_id = units_spawn_allow_stack(&units, 0, 5, 5);
  const int sol_a = units_spawn_allow_stack(&units, 1, 12, 12);
  const int sol_b = units_spawn_allow_stack(&units, 1, 13, 12);
  ColonizeUnit* brave = units_get(&units, brave_id);
  ColonizeUnit* ua = units_get(&units, sol_a);
  ColonizeUnit* ub = units_get(&units, sol_b);
  if (!brave || !ua || !ub) {
    return fail("5952 war: spawn");
  }
  brave->nation_id = 4;
  ua->nation_id = 0;
  ub->nation_id = 0;

  uint32_t turn = 1;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  /* exposed = 32 (two soldiers outside the colony), total = 32 →
   * lim_a = 64, lim_b = 128; the tribe's single Brave is 16 on both counts,
   * alarm 40 > 0x19 → DOS declares. */
  ai_contact_colony_tick_war_5952(&ctx, 0, c->x, c->y);
  if ((col1.indian[0].euro_diplo[0] & COL1_INDIAN_WAR_BIT) == 0) {
    return fail("5952 war: or_both must set indian[].euro_diplo WAR bit");
  }
  if ((col1.nation[0].relation_by_indian[0] & COL1_INDIAN_WAR_BIT) == 0) {
    return fail("5952 war: or_both must set the Euro-side relation_by_indian bit too");
  }
  if (!ai_diplo_indian_at_war(&col1, 0, 0)) {
    return fail("5952 war: FUN_5bfb_153e at-war reader should see the bit");
  }

  /* Alarm floor: <= 0x19 and not Spain → no declaration. */
  col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;
  col1.nation[0].relation_by_indian[0] = COL1_INDIAN_MET_BIT;
  col1.indian[0].alarm_by_player[0] = 0x19;
  ai_contact_colony_tick_war_5952(&ctx, 0, c->x, c->y);
  if ((col1.indian[0].euro_diplo[0] & COL1_INDIAN_WAR_BIT) != 0) {
    return fail("5952 war: alarm <= 0x19 must not declare (non-Spain)");
  }
  /* Spain (nation 2) skips the alarm floor. Its own colony/units, so the
   * foreign-presence gate is skipped for it too by DOS. */
  {
    c->nation_id = 2;
    ua->nation_id = 2;
    ub->nation_id = 2;
    col1.indian[0].euro_diplo[2] = COL1_INDIAN_MET_BIT;
    col1.indian[0].alarm_by_player[2] = 0;
    ai_contact_colony_tick_war_5952(&ctx, 2, c->x, c->y);
    if ((col1.indian[0].euro_diplo[2] & COL1_INDIAN_WAR_BIT) == 0) {
      return fail("5952 war: Spain declares with no alarm floor");
    }
    c->nation_id = 0;
    ua->nation_id = 0;
    ub->nation_id = 0;
  }

  /* Stronger tribe: 4 more Braves → brave total 80 > lim_a 64 → no war. */
  col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;
  col1.nation[0].relation_by_indian[0] = COL1_INDIAN_MET_BIT;
  col1.indian[0].alarm_by_player[0] = 40;
  int extra[4];
  for (int i = 0; i < 4; ++i) {
    extra[i] = units_spawn_allow_stack(&units, 0, 5 + i, 6);
    ColonizeUnit* b = units_get(&units, extra[i]);
    if (!b) {
      return fail("5952 war: spawn extra brave");
    }
    b->nation_id = 4;
  }
  ai_contact_colony_tick_war_5952(&ctx, 0, c->x, c->y);
  if ((col1.indian[0].euro_diplo[0] & COL1_INDIAN_WAR_BIT) != 0) {
    return fail("5952 war: brave total > land_combat_strength<<1 must not declare");
  }
  for (int i = 0; i < 4; ++i) {
    units_despawn(&units, extra[i]);
  }

  /* Foreign-presence gate (DS:0x95f2 bit 2): a rival's land unit on this
   * continent sends DOS down the else-arm — appetite cap only, no war. */
  {
    const int rival = units_spawn_allow_stack(&units, 1, 2, 2);
    ColonizeUnit* r = units_get(&units, rival);
    if (!r) {
      return fail("5952 war: spawn rival");
    }
    r->nation_id = 1;
    ai_contact_colony_tick_war_5952(&ctx, 0, c->x, c->y);
    if ((col1.indian[0].euro_diplo[0] & COL1_INDIAN_WAR_BIT) != 0) {
      return fail("5952 war: foreign Euro unit on the continent must block the declare");
    }
    units_despawn(&units, rival);
  }

  /* Sanity: with the rival gone the same call declares again. */
  ai_contact_colony_tick_war_5952(&ctx, 0, c->x, c->y);
  if ((col1.indian[0].euro_diplo[0] & COL1_INDIAN_WAR_BIT) == 0) {
    return fail("5952 war: declare should resume once the continent is clear");
  }

  /*
   * Audit follow-up B — the -0x6a4e exposed row (DS:0x95b2) uses DOS's own
   * gate (FUN_4962_0018, 4962:022f-026e): a unit only drops out when it
   * stands on a settlement AND (its nation is human-controlled OR its
   * ai_plan is 'A'/'G'). The port used to test the ORDERS byte for
   * FORTIFY/FORTIFIED and to exclude every in-colony unit outright; +0x314b
   * is ai_plan, +0x314c is orders.
   */
  {
    col1.player[0].control = 1; /* AI-controlled nation 0 */

    /* (a) Fortified in the open still counts. Under the old orders gate both
     * soldiers dropped out, exposed fell to 0 and the `exposed <= 1` guard
     * killed the declare outright. */
    col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;
    col1.nation[0].relation_by_indian[0] = COL1_INDIAN_MET_BIT;
    col1.indian[0].alarm_by_player[0] = 40;
    ua->orders = UNITS_ORDER_FORTIFIED;
    ub->orders = UNITS_ORDER_FORTIFY;
    ua->col1_ai_plan = 0;
    ub->col1_ai_plan = 0;
    ai_contact_colony_tick_war_5952(&ctx, 0, c->x, c->y);
    if ((col1.indian[0].euro_diplo[0] & COL1_INDIAN_WAR_BIT) == 0) {
      return fail("5952 war: fortify ORDERS must not empty the exposed row");
    }
    ua->orders = UNITS_ORDER_NONE;
    ub->orders = UNITS_ORDER_NONE;

    /* (b) AI field units standing in their own colony still count. */
    col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;
    col1.nation[0].relation_by_indian[0] = COL1_INDIAN_MET_BIT;
    col1.indian[0].alarm_by_player[0] = 40;
    ua->x = c->x;
    ua->y = c->y;
    ub->x = c->x;
    ub->y = c->y;
    ai_contact_colony_tick_war_5952(&ctx, 0, c->x, c->y);
    if ((col1.indian[0].euro_diplo[0] & COL1_INDIAN_WAR_BIT) == 0) {
      return fail("5952 war: an AI unit inside a colony is still exposed in DOS");
    }

    /* (c) ai_plan 'A' / 'G' on a settlement tile is the real exclusion. */
    col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;
    col1.nation[0].relation_by_indian[0] = COL1_INDIAN_MET_BIT;
    col1.indian[0].alarm_by_player[0] = 40;
    ua->col1_ai_plan = 0x41u; /* 'A' */
    ub->col1_ai_plan = 0x47u; /* 'G' */
    ai_contact_colony_tick_war_5952(&ctx, 0, c->x, c->y);
    if ((col1.indian[0].euro_diplo[0] & COL1_INDIAN_WAR_BIT) != 0) {
      return fail("5952 war: garrison ai_plan 'A'/'G' must leave the exposed row");
    }

    /* (d) Human-controlled nation: never counted on a settlement tile. */
    ua->col1_ai_plan = 0;
    ub->col1_ai_plan = 0;
    col1.player[0].control = 0;
    col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;
    col1.nation[0].relation_by_indian[0] = COL1_INDIAN_MET_BIT;
    col1.indian[0].alarm_by_player[0] = 40;
    ai_contact_colony_tick_war_5952(&ctx, 0, c->x, c->y);
    if ((col1.indian[0].euro_diplo[0] & COL1_INDIAN_WAR_BIT) != 0) {
      return fail("5952 war: a human nation's in-colony units never count as exposed");
    }

    col1.player[0].control = 1;
    ua->x = 12;
    ua->y = 12;
    ub->x = 13;
    ub->y = 12;
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(col1.tribe);
  col1.tribe = NULL;
  col1_save_free(&col1);
  return 0;
}

/*
 * Smell #53 — ai_contact_clamp_alarms (reached from
 * ai_contact_indian_prelude) bands the alarm mirror. It used to allow
 * 0..200 while FUN_4cc6_00f2 clamps 0..100 on every write and
 * ai_diplo_indian_alarm clamps 0..100 on every read, so a DOS-authored or
 * poisoned value in 101..200 was visible to this file's raw readers
 * (pair_friction, the raid gate) but not to the accessors.
 */
static int test_prelude_alarm_band(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.tribe_count = 0;
  col1.tribe = NULL;
  memset(&col1.indian[0], 0, sizeof(col1.indian[0]));
  col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;
  col1.indian[0].alarm_by_player[0] = 150; /* out of band on load */
  col1.indian[0].alarm_by_player[1] = 90;  /* in band, must not move */

  uint32_t turn = 3;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.turn_number = &turn;
  ctx.col1 = &col1;
  ctx.col1_ok = true;

  ai_contact_indian_prelude(&ctx, 4);

  if (col1.indian[0].alarm_by_player[0] != 100u) {
    fprintf(
      stderr,
      "unit_ai_contact: prelude alarm band %u (want 100)\n",
      (unsigned)col1.indian[0].alarm_by_player[0]
    );
    col1_save_free(&col1);
    return fail("clamp_alarms must band the mirror at the DOS 0..100 range");
  }
  if (col1.indian[0].alarm_by_player[1] != 90u) {
    col1_save_free(&col1);
    return fail("clamp_alarms must leave in-band values alone");
  }
  col1_save_free(&col1);
  return 0;
}

/*
 * bugs.md #557 — the AI missionary acts only at a village, through
 * FUN_4d56_4528's non-human unit-type switch caseD_3: no incite gate met and
 * no mission on the village → case 3 Establish Mission (thunk_FUN_1000_a5dc),
 * which sets tribe.mission to the acting nation and consumes the unit. A
 * human-owned missionary must take the @ACTIONS menu instead, so the AI arm
 * refuses it.
 */
static int test_ai_missionary_village_arm(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 2;
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    return fail("missionary village: alloc tribe");
  }
  col1.tribe[0].x = 5;
  col1.tribe[0].y = 5;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
  col1.tribe[0].population = 4;
  col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 4;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Missionaries");
  units.types[3].kind_plus1 = (uint8_t)(UNITS_KIND_MISSIONARY + 1);
  units.types[3].movement = 1;
  const int mid = units_spawn_allow_stack(&units, 3, 6, 5);
  ColonizeUnit* miss = units_get(&units, mid);
  if (!miss) {
    return fail("missionary village: spawn");
  }
  miss->nation_id = 0;

  uint32_t turn = 5;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.turn_number = &turn;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.units = &units;
  ctx.human_nation = 2; /* nation 0 is an AI here */

  if (!ai_contact_ai_missionary_village(&ctx, 0, 0, mid)) {
    col1_save_free(&col1);
    return fail("4528 case 3 should establish a mission for the AI missionary");
  }
  if ((col1.tribe[0].mission & COL1_TRIBE_MISSION_NATION_MASK) != 0) {
    col1_save_free(&col1);
    return fail("establish should set tribe.mission to the acting nation");
  }
  if (units_get(&units, mid) && units_get(&units, mid)->active) {
    col1_save_free(&col1);
    return fail("establish should consume the missionary");
  }

  /* Human-owned missionary: the AI arm must not fire (menu path only). */
  col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
  const int hid = units_spawn_allow_stack(&units, 3, 6, 5);
  ColonizeUnit* hm = units_get(&units, hid);
  if (!hm) {
    col1_save_free(&col1);
    return fail("missionary village: spawn human");
  }
  hm->nation_id = 2;
  if (ai_contact_ai_missionary_village(&ctx, 2, 0, hid) ||
      col1.tribe[0].mission != COL1_TRIBE_MISSION_NONE) {
    col1_save_free(&col1);
    return fail("the human missionary must use the @ACTIONS menu, not the AI arm");
  }
  col1_save_free(&col1);
  return 0;
}

static const TestCase k_cases[] = {
    {"test_colony_tick_war_5952", test_colony_tick_war_5952},
    {"test_prelude_alarm_band", test_prelude_alarm_band},
    {"test_ai_missionary_village_arm", test_ai_missionary_village_arm},
};
TEST_MAIN(k_cases)
