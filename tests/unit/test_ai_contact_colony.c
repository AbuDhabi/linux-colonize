/* Slice of the former tests/unit/test_ai_contact.c (split by feature 2026-09-23):
 * 5952 colony tick at war, prelude alarm band, missionary village arm. */
#include "test_ai_contact_common.h"

#include "core/ai_contact_internal.h"

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

/*
 * bugs.md #863 — FUN_5bfb_022e raw 87687-87698: a food beg the colony
 * CONCEDES sets bVar7 and forces bVar6 true, so the visit falls through into
 * the gift half (LAB_5bfb_096c) in the same encounter; the gift fork at raw
 * 87903 then reads bVar7 as a third term
 * (`bid[0] <= ask[0] || bVar7 || 25 < colony food` -> @INDIANGIVESTUFF), so
 * the village cannot hand back the food it just took. The port used to make
 * beg and gift mutually exclusive arms.
 */
static int test_beg_conceded_falls_into_gift_863(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 0; /* raw 87692 rand(0, difficulty) is then always 0 */
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    return fail("#863: alloc tribe");
  }
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].x = 5;
  col1.tribe[0].y = 7;
  col1.tribe[0].population = 1;
  col1.tribe[0].mission = 0xff; /* no mission: never the convert arm */
  col1.tribe[0].alarm[0].friction = 10;
  col1.tribe[0].alarm[0].attacks = 0;
  col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT | 1;
  col1.indian[0].alarm_by_player[0] = 10; /* <= 0x31, so the gift half is live */
  col1.indian[0].tech = 1;
  col1.indian[0].contact_state[0] = 0;
  for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
    col1.nation[0].trade.euro_price[cg] = 2;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    col1_save_free(&col1);
    return fail("#863: alloc map");
  }
  /* Terrain class 2 all round: the 2154 scorer's food bucket (local_6e) then
   * gives the village a food SURPLUS (bid[0] > ask[0]) at population 1, which
   * with a colony under 26 food is exactly the @INDIANGIVEFOOD branch bVar7
   * has to veto. */
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 2;
  }

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Brave");
  units.types[0].movement = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* c = &colonies.colonies[0];
  c->active = true;
  c->nation_id = 0;
  c->x = 5;
  c->y = 5;
  snprintf(c->name, sizeof(c->name), "Jamestown");
  c->stock[COLONIZE_CARGO_FOOD] = 40; /* half goes, leaving 20 (<= 0x19) */

  const int bid = units_spawn_allow_stack(&units, 0, 6, 5);
  ColonizeUnit* bu = units_get(&units, bid);
  if (!bu) {
    col1_save_free(&col1);
    return fail("#863: spawn brave");
  }
  bu->nation_id = 4;
  bu->home_tribe_id = 0;
  /* The DOS move-tail selector both halves share. */
  ai_native_note_brave_turn_origin(bid, 9, 9);

  ColonizeDosRng rng;
  dos_rng_seed(&rng, 12345u);
  uint32_t turn = 5;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.turn_number = &turn;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.rng = &rng;
  ctx.human_nation = 3; /* nation 0 is an AI Euro here */

  /* The Brave's own 022e head published a NON-generous verdict (bVar6 false):
   * without the beg concession this encounter would take LAB_5bfb_0def only. */
  ai_contact_visit_mood_publish(&ctx, 4, 0, bid, 0);

  ai_contact_apply_beg_food(&ctx, &col1.indian[0], 4, 0, 0, 0, 1);

  const AiContactVisitMood* m = &ai_contact_s_visit_mood[0][0];
  if (!m->valid || !m->bvar7 || !m->bvar6) {
    col1_save_free(&col1);
    return fail("#863: a conceded beg must publish bVar7 and force bVar6");
  }
  if (col1.indian[0].contact_state[0] != 2) {
    col1_save_free(&col1);
    return fail("#863: raw 87698 stamps contact_state 2 on the conceded beg");
  }
  /*
   * The gift half ran in the same visit (it stamps state 2 and discharges the
   * village word) but bVar7 vetoed @INDIANGIVEFOOD, so the 20 food the colony
   * just handed over was NOT topped back up to 75.
   */
  if (c->stock[COLONIZE_CARGO_FOOD] != 20) {
    fprintf(
      stderr, "unit_ai_contact: #863 colony food %d (want 20)\n",
      c->stock[COLONIZE_CARGO_FOOD]
    );
    col1_save_free(&col1);
    return fail("#863: bVar7 must veto the @INDIANGIVEFOOD branch");
  }

  /*
   * Control: the SAME encounter with bVar7 clear (a plain generous visit)
   * does take @INDIANGIVEFOOD and tops the colony to 0x4b — so the food
   * staying at 20 above is the bVar7 veto, not a gift arm that never ran.
   */
  col1.indian[0].contact_state[0] = 0;
  ai_native_note_brave_turn_origin(bid, 9, 9);
  ai_contact_visit_mood_publish(&ctx, 4, 0, bid, 1);
  (void)ai_contact_try_village_gifts(&ctx, 4);
  if (c->stock[COLONIZE_CARGO_FOOD] != 0x4b) {
    fprintf(
      stderr, "unit_ai_contact: #863 control food %d (want 75)\n",
      c->stock[COLONIZE_CARGO_FOOD]
    );
    col1_save_free(&col1);
    return fail("#863 control: without bVar7 the gift arm must give food");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  col1_save_free(&col1);
  return 0;
}

/*
 * bugs.md #803 — FUN_4d56_2820 tests the acting unit's ARRAY INDEX into the
 * 300-slot unit pool (`param_2 == 0xf || param_2 == 8`, 2820 doc line 551),
 * not the traded cargo. Ported via the same pool-slot-index proxy as
 * bugs.md #878(a) (ai_euro_land.c): `unit - ctx->units->units`. Drive one
 * human sale through the accept path (LAB_002bbc) with the trading unit at
 * pool slot 8, and again at slot 9, and confirm only slot 8 blanks
 * tribe.last_bought to 0xff instead of the sold cargo.
 */
static int run_803_trade_case(int filler_units, uint8_t* out_last_bought) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 2;
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    return fail("#803: alloc tribe");
  }
  col1.tribe[0].x = 5;
  col1.tribe[0].y = 5;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].population = 4;
  col1.tribe[0].last_bought = 0xffu;
  col1.tribe[0].last_sold = 0xffu;
  col1.tribe[0].sticky_trade_good = 0xffu;
  ColonizeCol1Indian* ind = &col1.indian[0];
  memset(ind, 0, sizeof(*ind));
  ind->euro_diplo[0] = COL1_INDIAN_MET_BIT;
  ind->alarm_by_player[0] = 10; /* high relation: avoid the refuse gate */
  for (int c = 1; c < 8; ++c) {
    ind->tons[c] = 300; /* every good in stock so 2820's ask[] table is live */
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    col1_save_free(&col1);
    return fail("#803: alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Brave");
  units.types[0].movement = 1;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Free Colonist");
  units.types[1].movement = 1;
  units.types[1].cargo = 2;

  const int brave_id = units_spawn_allow_stack(&units, 0, 5, 5);
  ColonizeUnit* brave = units_get(&units, brave_id);
  if (!brave) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    col1_save_free(&col1);
    return fail("#803: brave spawn");
  }
  brave->nation_id = 4;

  /* Burn pool slots with filler units so the trading unit lands at a chosen
   * index (`filler_units` inactive/active fillers ahead of it). */
  for (int i = 0; i < filler_units; ++i) {
    const int fid = units_spawn_allow_stack(&units, 1, 20, 20);
    ColonizeUnit* fu = units_get(&units, fid);
    if (!fu) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      col1_save_free(&col1);
      return fail("#803: filler spawn");
    }
    fu->nation_id = 0;
  }

  const int trader_id = units_spawn_allow_stack(&units, 1, 6, 5);
  ColonizeUnit* trader = units_get(&units, trader_id);
  if (!trader) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    col1_save_free(&col1);
    return fail("#803: trader spawn");
  }
  trader->nation_id = 0;
  trader->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
  trader->hold_goods_amount[0] = 50;
  const int slot = (int)(trader - units.units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);

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

  AiPopupState pop;
  ai_popup_clear(&pop);
  ctx.ai_popups = &pop;

  ai_contact_reset();
  village_trade_intel_reset();

  pop.has_result = true;
  pop.result_cancelled = false;
  pop.result_choice_id = 1; /* AI_CONTACT_CHOICE_TRADE */
  pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
  pop.result_nation_a = 0;
  pop.result_nation_b = 4;
  pop.result_payload = 0;
  ai_contact_apply_popup_result(&ctx, &pop);

  if (pop.queue_count < 1 || pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_CHOICE ||
      pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_TRADE_OFFER) {
    fprintf(stderr, "unit_ai_contact: #803 slot %d no TRADE_OFFER CHOICE queued\n", slot);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    col1_save_free(&col1);
    return fail("#803: Meet+Trade should queue the price CHOICE");
  }
  const int price = pop.queue[pop.queue_count - 1].payload;
  ai_popup_clear(&pop);
  pop.has_result = true;
  pop.result_cancelled = false;
  pop.result_choice_id = 1; /* AI_CONTACT_TRADE_OFFER_ACCEPT */
  pop.result_tag = AI_POPUP_TAG_CONTACT_TRADE_OFFER;
  pop.result_nation_a = 0;
  pop.result_nation_b = 4;
  pop.result_payload = price;
  ai_contact_apply_popup_result(&ctx, &pop);

  if (trader->hold_goods_amount[0] != 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    col1_save_free(&col1);
    return fail("#803: accept should remove the sold hold");
  }

  *out_last_bought = col1.tribe[0].last_bought;

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  col1_save_free(&col1);
  return 0;
}

static int test_803_last_bought_keyed_on_unit_slot(void) {
  uint8_t lb_slot8 = 0xaa;
  uint8_t lb_slot9 = 0xaa;

  /* One filler (index 0) + the brave (index 1) leaves the trader at slot 2
   * for a baseline probe of the actual slot index units_spawn_allow_stack
   * hands out; walk `filler_units` up until the trader lands exactly on 8
   * and 9, since despawn/id-reuse policy is an implementation detail we
   * should not hardcode a filler count against. */
  int slot8_filler = -1;
  int slot9_filler = -1;
  for (int f = 0; f <= 16 && (slot8_filler < 0 || slot9_filler < 0); ++f) {
    ColonizeUnitPool probe;
    memset(&probe, 0, sizeof(probe));
    units_reset(&probe);
    units_set_occupancy_map(NULL);
    probe.type_count = 2;
    const int b = units_spawn_allow_stack(&probe, 0, 5, 5);
    (void)b;
    for (int i = 0; i < f; ++i) {
      units_spawn_allow_stack(&probe, 1, 20, 20);
    }
    const int t = units_spawn_allow_stack(&probe, 1, 6, 5);
    ColonizeUnit* tu = units_get(&probe, t);
    if (tu) {
      const int slot = (int)(tu - probe.units);
      if (slot == 8 && slot8_filler < 0) {
        slot8_filler = f;
      }
      if (slot == 9 && slot9_filler < 0) {
        slot9_filler = f;
      }
    }
    units_set_occupancy_map(NULL);
  }
  if (slot8_filler < 0 || slot9_filler < 0) {
    return fail("#803: could not locate filler counts landing the trader on slots 8/9");
  }

  if (run_803_trade_case(slot8_filler, &lb_slot8)) {
    return 1;
  }
  if (run_803_trade_case(slot9_filler, &lb_slot9)) {
    return 1;
  }

  if (lb_slot8 != 0xffu) {
    fprintf(stderr, "unit_ai_contact: #803 slot8 last_bought=0x%02x (want 0xff)\n", lb_slot8);
    return fail("#803: trading unit at pool slot 8 must blank last_bought to 0xff");
  }
  if (lb_slot9 != (uint8_t)COLONIZE_CARGO_TRADE_GOODS) {
    fprintf(
      stderr, "unit_ai_contact: #803 slot9 last_bought=0x%02x (want cargo 0x%02x)\n", lb_slot9,
      (uint8_t)COLONIZE_CARGO_TRADE_GOODS
    );
    return fail("#803: trading unit off slots 8/15 must set last_bought = sold cargo");
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"test_colony_tick_war_5952", test_colony_tick_war_5952},
    {"test_prelude_alarm_band", test_prelude_alarm_band},
    {"test_ai_missionary_village_arm", test_ai_missionary_village_arm},
    {"test_beg_conceded_falls_into_gift_863", test_beg_conceded_falls_into_gift_863},
    {"test_803_last_bought_keyed_on_unit_slot", test_803_last_bought_keyed_on_unit_slot},
};
TEST_MAIN(k_cases)
