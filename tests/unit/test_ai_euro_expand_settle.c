/* Slice of the former tests/unit/test_ai_euro_expand.c (split by feature 2026-09-23):
 * Indian-land founding, pioneer improve timer, order 9. */
#include "test_ai_euro_expand_common.h"

static int count_nation_colonies(const ColonizeColonyPool* colonies, int nation_id) {
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (colonies->colonies[i].active && colonies->colonies[i].nation_id == nation_id) {
      ++n;
    }
  }
  return n;
}

/*
 * Pioneer plow/road planner: idle Hardy Pioneer with tools on unplowed colony
 * surround → units_pioneer_plow (clear+plow API). Cite: Colonization.pdf
 * Clear/Plow/Road; Hardy Pioneer faster work.
 */

/*
 * Col1 +0x8c improve_timer: pioneer plow gated until timer ≥ 2; inventory INC;
 * successful plow clears timer. Cite: FUN_5952 ~93663 / ~94546.
 */
static int unit_improve_timer_pioneer_gate(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  map.improve = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3 || !map.improve) {
    return fail("improve-timer alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  c->improve_timer = 0; /* blocked (INC → 1 still < 2) */
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* On colony tile first — adjacent land is FOUND-eligible under 06ae and would
   * consume the pioneer before the improve_timer gate is observed. */
  int pid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    fx_map_free(&map);
    return fail("improve-timer spawn");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves = 1 * UNITS_MP_PER_TILE;
  pioneer->tools = 100;
  pioneer->profession = UNITS_JOB_PIONEER;

  ai_goals_reset();
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 23;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 7;

  ai_euro_dispatcher_turn(&ctx, nation);
  if (map_tile_is_plowed(&map, 4, 3)) {
    fx_map_free(&map);
    return fail("expected improve_timer gate to block plow");
  }
  if (colonies.colonies[0].improve_timer < 1) {
    fx_map_free(&map);
    return fail("expected improve_timer INC");
  }

  /* Unblock: park pioneer on plowable surround and meet gate. */
  colonies.colonies[0].improve_timer = 2;
  pioneer = units_get(&units, pid);
  if (!pioneer || !pioneer->active) {
    /*
     * FUN_5952_035e absorption arm (raw 94257-94263, ported 2026-09-18): a
     * Pioneer standing on the town tile of a colony that wants colonists is
     * taken into the workforce. This fixture parks it there for phase 1, so
     * phase 2 spawns a fresh one on the surround tile it is really about.
     */
    pid = units_spawn(&units, 0, 4, 3);
    pioneer = units_get(&units, pid);
    if (!pioneer) {
      fx_map_free(&map);
      return fail("improve-timer respawn");
    }
    pioneer->nation_id = nation;
    pioneer->profession = UNITS_JOB_PIONEER;
  }
  pioneer->x = 4;
  pioneer->y = 3;
  pioneer->moves = 1 * UNITS_MP_PER_TILE;
  pioneer->orders = 0;
  pioneer->tools = 100;
  /*
   * Real DS:0x2f78 Pioneer threshold byte (2026-08-20 live capture) means
   * plow no longer finishes in a single dispatcher turn even for a Hardy
   * Pioneer on plains (threshold 5, halved to 2) — drive several turns
   * rather than assuming one-shot completion, matching the real formula
   * this test's gate logic is otherwise exercising correctly.
   */
  int any_plow = 0;
  for (int t = 0; t < 8 && !any_plow; ++t) {
    pioneer = units_get(&units, pid);
    if (pioneer && pioneer->active) {
      pioneer->moves = 1 * UNITS_MP_PER_TILE;
    }
    turn++;
    ai_euro_dispatcher_turn(&ctx, nation);
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        if (map_tile_is_plowed(&map, 4 + dx, 4 + dy)) {
          any_plow = 1;
        }
      }
    }
  }
  if (!any_plow) {
    fx_map_free(&map);
    return fail("expected plow after improve_timer meets gate");
  }
  if (colonies.colonies[0].improve_timer != 0) {
    fprintf(stderr, "improve_timer after plow=%u\n",
            (unsigned)colonies.colonies[0].improve_timer);
    fx_map_free(&map);
    return fail("expected improve_timer clear on plow");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: improve_timer pioneer gate ok\n");
  return 0;
}

/*
 * AI FOUND on Indian homeland: charges FUN_4cc6_07c2 gold; short gold PARK;
 * Minuit elect bit → free. Cite: colonies_found_with_indian_land; FF 2.
 *
 * promote_secondary_to_primary wipes pre-set primaries each turn — so seed a
 * stocked colony (COLONY not LABOR), discover its expand FOUND tile, park a
 * tribe on that tile (homeland), and stand the founder there.
 */
static void unit_indian_land_seed_colony(ColonizeColonyPool* colonies, int nation) {
  colonies_init(colonies);
  ColonizeColony* c = &colonies->colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  /* Closes FUN_5952_035e's emergency lumber buy (raw 94680: stock < 2), which
   * would otherwise take 200 gold off this nation on turn 0. */
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  c->building_in_production = -1;
  colonies->colony_count = 1;
  colonies->next_id = 1;
}

static int unit_indian_land_found(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("indian-land alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* desert land */
  }

  ColonizeCol1Tribe tribe;
  memset(&tribe, 0, sizeof(tribe));
  tribe.x = 8;
  tribe.y = 8;
  tribe.nation_id = 4; /* Arawak */
  tribe.state.capital = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1; /* AI */
    col1.player[i].diplomacy = 0;
  }
  col1.tribe = &tribe;
  col1.head.tribe_count = 1;
  col1.head.difficulty = 0;
  memset(&col1.indian[0], 0, sizeof(col1.indian[0]));
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  ColonizeColonyPool colonies;
  unit_indian_land_seed_colony(&colonies, nation);

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  /*
   * 2026-09-08: the expand-FOUND-around-own-colony writer this phase used
   * to probe was a Linux invention and is deleted (DOS 0a60 writes FOUND
   * only at village-adjacent ocean beachheads on colony-free continents,
   * decomp 88049, and next to foreign colonies, decomp 87983 — both
   * ported). The FOUND goal is now seeded directly; this scenario's subject
   * is the Indian homeland purchase, not goal production.
   */
  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 11;

  const int fx = 10;
  const int fy = 10;

  /* Park tribe on expand FOUND → homeland; founder stands there. */
  tribe.x = (uint8_t)fx;
  tribe.y = (uint8_t)fy;
  const int cost = colonies_indian_land_purchase_gold(&col1, &map, fx, fy, nation);
  if (cost <= 0) {
    fx_map_free(&map);
    return fail("indian-land: expected homeland purchase gold > 0");
  }

  /* Phase 1: enough gold → found + debit (planning treasury bump then charge). */
  {
    unit_indian_land_seed_colony(&colonies, nation);
    units_reset(&units);
    units_set_occupancy_map(NULL);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
    units.types[0].movement = 3;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    col1.nation[nation].gold = 500;
    /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
     * ladder / recruit / Artillery buys naturally inert (blank census). */
    col1.stuff.ship_counts[nation] = 1;
    col1.indian[0].lands_bought = 0;
    col1.nation[nation].founding_fathers[0] = 0;
    col1.nation[nation].founding_father_count = 0;

    const int uid = units_spawn(&units, 0, fx, fy);
    ColonizeUnit* founder = units_get(&units, uid);
    if (!founder) {
      fx_map_free(&map);
      return fail("indian-land spawn pay");
    }
    founder->nation_id = nation;
    founder->orders = 0;
    founder->moves = 3 * UNITS_MP_PER_TILE;

    ai_goals_reset();
    ai_goals_upsert_secondary(nation, fx, fy, AI_GOAL_FOUND, 2);
    turn = 41;
    const uint32_t gold0 = col1.nation[nation].gold;
    ai_euro_dispatcher_turn(&ctx, nation);

    founder = units_get(&units, uid);
    const int n = count_nation_colonies(&colonies, nation);
    if (n != 2 || (founder && founder->active)) {
      fprintf(
        stderr,
        "unit_ai_euro_expand: indian-land pay n=%d active=%d gold %u→%u "
        "cost=%d FOUND=(%d,%d)\n",
        n,
        founder ? (int)founder->active : 0,
        gold0,
        col1.nation[nation].gold,
        cost,
        fx,
        fy
      );
      fx_map_free(&map);
      return fail("indian-land: expected found + despawn when gold enough");
    }
    /* gold_after == gold0 + bump - cost; bump small (≤80). */
    {
      const uint32_t spent_and_bump = col1.nation[nation].gold + (uint32_t)cost;
      if (spent_and_bump < gold0 || spent_and_bump - gold0 > 80u) {
        fprintf(
          stderr,
          "unit_ai_euro_expand: indian-land gold before=%u after=%u cost=%d\n",
          gold0,
          col1.nation[nation].gold,
          cost
        );
        fx_map_free(&map);
        return fail("indian-land: unexpected gold after homeland found");
      }
    }
  }

  /*
   * Phase 1b (bugs.md #709/#710, 2026-09-23): the AI affordability gate is
   * FUN_479b_00ca's `gold - cost >= cost / 2` (asm 121034-121094), not
   * `gold >= cost`, and FUN_4cc6_07c2's price carries the census term
   * `score -= max((10 - census_pop_proxy[nation]) >> 1, 0)` (raw 81213).
   */
  {
    /* Phase 1 stamped MAP_LAYER2_PURCHASED on (fx,fy); clear so this tile is
     * chargeable again. */
    {
      const size_t idx0 = (size_t)fy * (size_t)map.width + (size_t)fx;
      if (map.layer2 && idx0 < map.tile_count) {
        map.layer2[idx0] = (uint8_t)(map.layer2[idx0] & (uint8_t)~MAP_LAYER2_PURCHASED);
      }
    }
    col1.indian[0].lands_bought = 0;
    col1.nation[nation].founding_fathers[0] = 0;
    col1.nation[nation].founding_father_count = 0;
    /* Census term: a bigger nation pays strictly more for the same tile. */
    const uint8_t census0 = col1.stuff.census_pop_proxy[nation];
    col1.stuff.census_pop_proxy[nation] = 0;
    const int cost_small = colonies_indian_land_purchase_gold(&col1, &map, fx, fy, nation);
    col1.stuff.census_pop_proxy[nation] = 10;
    const int cost_big = colonies_indian_land_purchase_gold(&col1, &map, fx, fy, nation);
    col1.stuff.census_pop_proxy[nation] = census0;
    if (cost_big <= cost_small) {
      fprintf(
        stderr, "unit_ai_euro_expand: census term small=%d big=%d\n", cost_small, cost_big
      );
      fx_map_free(&map);
      return fail("indian-land: census_pop_proxy term must raise the price");
    }

    /* Half-margin gate: gold == cost is NOT affordable for an AI nation. */
    unit_indian_land_seed_colony(&colonies, nation);
    units_reset(&units);
    units_set_occupancy_map(NULL);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
    units.types[0].movement = 3;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    col1.stuff.ship_counts[nation] = 1;
    col1.indian[0].lands_bought = 0;
    col1.nation[nation].founding_fathers[0] = 0;
    col1.nation[nation].founding_father_count = 0;
    {
      const size_t idx = (size_t)fy * (size_t)map.width + (size_t)fx;
      if (map.layer2 && idx < map.tile_count) {
        map.layer2[idx] = (uint8_t)(map.layer2[idx] & (uint8_t)~MAP_LAYER2_PURCHASED);
      }
    }
    const int gate_cost = colonies_indian_land_purchase_gold(&col1, &map, fx, fy, nation);
    col1.nation[nation].gold = (uint32_t)gate_cost;

    const int uid = units_spawn(&units, 0, fx, fy);
    ColonizeUnit* founder = units_get(&units, uid);
    if (!founder) {
      fx_map_free(&map);
      return fail("indian-land spawn gate");
    }
    founder->nation_id = nation;
    founder->orders = 0;
    founder->moves = 3 * UNITS_MP_PER_TILE;

    ai_goals_reset();
    ai_goals_upsert_secondary(nation, fx, fy, AI_GOAL_FOUND, 2);
    turn = 41;
    const uint32_t gate_gold0 = col1.nation[nation].gold;
    ai_euro_dispatcher_turn(&ctx, nation);

    /* DOS's 00ca simply refuses to pay; the port still founds (caller of the
     * thunk 2a1f:01dd unresolved), so the colony appears with gold intact. */
    if (count_nation_colonies(&colonies, nation) != 2) {
      fx_map_free(&map);
      return fail("indian-land: half-margin gate must still found");
    }
    if (col1.nation[nation].gold + (uint32_t)gate_cost <= gate_gold0) {
      fprintf(
        stderr,
        "unit_ai_euro_expand: half-margin gate paid gold %u→%u cost=%d\n",
        gate_gold0,
        col1.nation[nation].gold,
        gate_cost
      );
      fx_map_free(&map);
      return fail("indian-land: gold == cost must not buy (gold-cost < cost/2)");
    }
  }

  /* Phase 2: short gold → PARK (seed colony remains; no second colony).
   * Phase 1 stamped MAP_LAYER2_PURCHASED on (fx,fy); clear so charge still
   * applies (founding must not treat prior buy as free forever for this smoke). */
  {
    unit_indian_land_seed_colony(&colonies, nation);
    units_reset(&units);
    units_set_occupancy_map(NULL);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
    units.types[0].movement = 3;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    col1.nation[nation].gold = 10;
    /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
     * ladder / recruit / Artillery buys naturally inert (blank census). */
    col1.stuff.ship_counts[nation] = 1;
    col1.indian[0].lands_bought = 0;
    col1.nation[nation].founding_fathers[0] = 0;
    col1.nation[nation].founding_father_count = 0;
    {
      const size_t idx = (size_t)fy * (size_t)map.width + (size_t)fx;
      if (map.layer2 && idx < map.tile_count) {
        map.layer2[idx] = (uint8_t)(map.layer2[idx] & (uint8_t)~MAP_LAYER2_PURCHASED);
      }
    }

    const int uid = units_spawn(&units, 0, fx, fy);
    ColonizeUnit* founder = units_get(&units, uid);
    if (!founder) {
      fx_map_free(&map);
      return fail("indian-land spawn poor");
    }
    founder->nation_id = nation;
    founder->orders = 0;
    founder->moves = 3 * UNITS_MP_PER_TILE;

    char status[128];
    memset(status, 0, sizeof(status));
    col1.player[nation].control = 0; /* human for thin status chrome */
    ctx.human_nation = nation;
    ctx.status = status;
    ctx.status_size = sizeof(status);

    ai_goals_reset();
    ai_goals_upsert_secondary(nation, fx, fy, AI_GOAL_FOUND, 2);
    turn = 42;
    ai_euro_dispatcher_turn(&ctx, nation);

    founder = units_get(&units, uid);
    if (count_nation_colonies(&colonies, nation) != 1 || !founder || !founder->active) {
      fx_map_free(&map);
      return fail("indian-land: short gold must PARK found");
    }
    if (col1.nation[nation].gold >= (uint32_t)cost) {
      fx_map_free(&map);
      return fail("indian-land: PARK case gold unexpectedly covers cost");
    }
    if (strstr(status, "Not enough gold") == NULL) {
      fprintf(stderr, "unit_ai_euro_expand: indian-land status=%s\n", status);
      fx_map_free(&map);
      return fail("indian-land: short gold should set human status");
    }
    ctx.status = NULL;
    ctx.status_size = 0;
    col1.player[nation].control = 1;
  }

  /* Phase 3: Minuit elect bit → free homeland found. */
  {
    unit_indian_land_seed_colony(&colonies, nation);
    units_reset(&units);
    units_set_occupancy_map(NULL);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
    units.types[0].movement = 3;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    col1.nation[nation].gold = 200;
    /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
     * ladder / recruit / Artillery buys naturally inert (blank census). */
    col1.stuff.ship_counts[nation] = 1;
    col1.indian[0].lands_bought = 0;
    col1.nation[nation].founding_fathers[FF_PETER_MINUIT / 8] |=
      (uint8_t)(1u << (FF_PETER_MINUIT % 8));
    if (!founding_fathers_nation_has(&col1, nation, FF_PETER_MINUIT)) {
      fx_map_free(&map);
      return fail("indian-land: Minuit elect bit helper");
    }
    if (colonies_indian_land_purchase_gold(&col1, &map, fx, fy, nation) != 0) {
      fx_map_free(&map);
      return fail("indian-land: Minuit must zero purchase gold");
    }

    const uint32_t gold0 = col1.nation[nation].gold;
    const int uid = units_spawn(&units, 0, fx, fy);
    ColonizeUnit* founder = units_get(&units, uid);
    if (!founder) {
      fx_map_free(&map);
      return fail("indian-land spawn Minuit");
    }
    founder->nation_id = nation;
    founder->orders = 0;
    founder->moves = 3 * UNITS_MP_PER_TILE;

    ai_goals_reset();
    ai_goals_upsert_secondary(nation, fx, fy, AI_GOAL_FOUND, 2);
    turn = 43;
    ai_euro_dispatcher_turn(&ctx, nation);

    founder = units_get(&units, uid);
    if (count_nation_colonies(&colonies, nation) != 2 || (founder && founder->active)) {
      fx_map_free(&map);
      return fail("indian-land: Minuit free found failed");
    }
    if (col1.nation[nation].gold < gold0 || col1.nation[nation].gold > gold0 + 80u) {
      fprintf(
        stderr,
        "unit_ai_euro_expand: Minuit gold %u→%u (want bump-only)\n",
        gold0,
        col1.nation[nation].gold
      );
      fx_map_free(&map);
      return fail("indian-land: Minuit free found must not spend land gold");
    }
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: indian-land FOUND charge/Minuit ok\n");
  return 0;
}

/*
 * bugs.md #611 — DOS-LITERAL FUN_521d_20e6 raw 90183-90206. A Pioneer that is
 * not the explorer pick takes order 9 (Build Road) with plan byte 0x52 at the
 * land commit point, unless the nearest village claims the tile (within the
 * tribe's tech-tier radius and alarm quartile < 3) or a foreign colony stands
 * within DOS distance 3. With no villages and no colonies at all on the map,
 * neither veto can fire.
 */
static int unit_pioneer_takes_order9(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("pioneer-order9 alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneers");
  units.types[0].kind_plus1 = (uint8_t)(UNITS_KIND_PIONEER + 1);
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* One own colony, far enough that the raw 90193 foreign-colony veto (which
   * only fires for another nation's colony inside DOS distance 3) is inert. */
  ColonizeColony* home = fx_colony_add(&colonies, nation, 2, 2, 4);
  home->stock[COLONIZE_CARGO_FOOD] = 100;

  /*
   * The −0x5ec4 explorer cap is 3 per continent for a non-Colonist, so with
   * four Pioneers at least one fails the iStack_6a explorer test and reaches
   * the raw 90183 commit point.
   */
  int pids[4];
  for (int i = 0; i < 4; ++i) {
    pids[i] = units_spawn(&units, 0, 4 + i * 2, 8);
    ColonizeUnit* p = units_get(&units, pids[i]);
    if (!p) {
      fx_map_free(&map);
      return fail("pioneer-order9 spawn");
    }
    p->nation_id = nation;
    p->moves = UNITS_MP_PER_TILE;
    p->orders = 0;
    p->tools = 100;
    p->profession = UNITS_JOB_PIONEER;
  }

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.tribe_count = 0;

  uint32_t turn = 32;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 7;

  ai_euro_dispatcher_turn(&ctx, nation);

  int road_orders = 0;
  for (int i = 0; i < 4; ++i) {
    const ColonizeUnit* p = units_get(&units, pids[i]);
    if (p && p->orders == UNITS_ORDER_BUILD_ROAD && p->col1_ai_plan == 0x52) {
      ++road_orders;
    }
  }
  fx_map_free(&map);

  if (road_orders == 0) {
    return fail("#611: no AI Pioneer took +0x314c = 9 / +0x314b = 0x52");
  }
  fprintf(stderr, "unit_ai_euro_expand: pioneer order-9 arm ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_pioneer_takes_order9", unit_pioneer_takes_order9},
    {"unit_indian_land_found", unit_indian_land_found},
    {"unit_improve_timer_pioneer_gate", unit_improve_timer_pioneer_gate},
};
TEST_MAIN(k_cases)
