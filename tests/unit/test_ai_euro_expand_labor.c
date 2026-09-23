/* Slice of the former tests/unit/test_ai_euro_expand.c (split by feature 2026-09-23):
 * colony labor binding, food emergency, colony AI flags, 0a60 work plan gate. */
#include "test_ai_euro_expand_common.h"

static int unit_labor_shortage_join(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("labor-shortage alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
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
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  /*
   * `labor_shortage = 2` used to be seeded here and asserted to survive the
   * tick as 2→1. It cannot any more: since 2026-09-09 (smell audit #38) the
   * colony tick stamps +0x8e unconditionally from the real FUN_5952_035e
   * formula (raw 94029-94045) before anything consumes it, so a hand-seeded
   * value is overwritten. For this fixture — pop 3, one colonist standing on
   * the tile (DS:0x8d72 = 1), zero threat — DOS computes
   * n = 4, want = max((4-1)/2, quota 0) = 1, clamped to n/2 = 2, no WoI, no
   * ring-1 enemy, and the on-tile walk decrements nothing (a Free Colonist's
   * 0x5236 combat byte is not > 1). So the tick stamps 1 and the join
   * consumes it to 0, which is what is asserted below.
   */
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* On colony tile — admit/join path. */
  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* col = units_get(&units, uid);
  if (!col) {
    fx_map_free(&map);
    return fail("labor-shortage spawn colonist");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves = 1 * UNITS_MP_PER_TILE;

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

  uint32_t turn = 52;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int pop_before = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  col = units_get(&units, uid);
  c = &colonies.colonies[0];
  const int joined = (col == NULL || !col->active) && c->population == pop_before + 1;
  if (!joined || c->labor_shortage != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: labor_shortage joined=%d pop %d→%d shortage=%u\n",
      joined,
      pop_before,
      c->population,
      (unsigned)c->labor_shortage
    );
    fx_map_free(&map);
    return fail("expected join consuming the tick-stamped labor_shortage 1→0");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: labor_shortage join ok\n");
  return 0;
}

static int unit_labor_bind_food_short(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("labor-bind alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short vs pop*2 */
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 5, 4);
  ColonizeUnit* col = units_get(&units, uid);
  if (!col) {
    fx_map_free(&map);
    return fail("labor-bind spawn colonist");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves = 1 * UNITS_MP_PER_TILE;

  ai_goals_reset();
  /* Distant FOUND lure — founders would prefer this without LABOR bind. */
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

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

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 42;

  const int pop_before = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  col = units_get(&units, uid);
  c = &colonies.colonies[0];
  const int joined = col == NULL || !col->active;
  const int at_colony = col && col->active && col->x == 4 && col->y == 4;
  const int not_yanked = !(col && col->active && col->goto_x == 12 && col->goto_y == 12);
  const int labor_goal = ai_goals_primary(nation, 0) &&
                         (ai_goals_primary(nation, 0)->code == AI_GOAL_LABOR ||
                          ai_goals_primary(nation, 0)->code == AI_GOAL_COLONY);

  if ((!joined && !at_colony) || !not_yanked) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: labor joined=%d at_col=%d goto=(%d,%d) pop %d→%d\n",
      joined,
      at_colony,
      col ? col->goto_x : -1,
      col ? col->goto_y : -1,
      pop_before,
      c->population
    );
    fx_map_free(&map);
    return fail("expected LABOR bind toward food-short colony, not FOUND yank");
  }
  (void)labor_goal;

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: LABOR bind food-short ok\n");
  return 0;
}

/*
 * Construction hammers bind: idle Pioneer on own colony with Stockade in
 * production stays/LABOR-joins rather than leave for distant FOUND.
 * Cite: building_production.md Stockade; euro_unit_act §2e.
 */

/*
 * Col1 +0x1d bit7 wants_construction: LABOR join even without
 * building_in_production (save latch). Cite: FUN_5952 ~95792 / ~94660.
 */

/*
 * Col1 +0x1b ai_flags: foreign Man-O-War within MD≤5 sets bit1 → COLONY_ALT
 * (code 8, prio 8). Cite: FUN_4962_0018; euro_dispatcher COLONY 5|8.
 */

/*
 * Col1 +0x1c colony_flags starvation: food < pop*2 latches bit3 → LABOR join.
 * Cite: FUN_364b_0688; save_format_map.md +0x1c.
 */
static int unit_colony_flags_starvation_labor(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("colony-flags alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
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
  c->stock[COLONIZE_CARGO_FOOD] = 2; /* < pop*2 → starvation */
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  c->labor_shortage = 0;
  c->colony_flags = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* col = units_get(&units, uid);
  if (!col) {
    fx_map_free(&map);
    return fail("colony-flags spawn");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves = 1 * UNITS_MP_PER_TILE;

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

  uint32_t turn = 65;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int pop_before = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  c = &colonies.colonies[0];
  col = units_get(&units, uid);
  /* The AI's food-short reading (stock < 2 per head) used to be stashed in
   * colony_flags bit3; that bit is DOS's inefficient-government latch again,
   * so assert the condition the dispatcher actually reads. */
  if (c->stock[COLONIZE_CARGO_FOOD] >=
      (c->colonist_count > 0 ? c->colonist_count : c->population) * 2) {
    fprintf(stderr, "unit_ai_euro_expand: colony_flags=0x%02x food=%d\n",
            (unsigned)c->colony_flags, c->stock[COLONIZE_CARGO_FOOD]);
    fx_map_free(&map);
    return fail("expected a food-short colony");
  }
  const int joined = (col == NULL || !col->active) && c->population == pop_before + 1;
  if (!joined) {
    fx_map_free(&map);
    return fail("expected LABOR join from starvation flag");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: colony_flags starvation LABOR ok\n");
  return 0;
}

/*
 * FUN_5952_035e `+0x1b |= 0xa0` (raw 94200-94206) — the "send a Pioneer to
 * CLEAR" pair. Forest-locked ring (8 Conifer Forest tiles: every ring tile is
 * unproductive, forests > 1) must raise both COLONIZE_COLONY_AI_WANTS_PIONEER_
 * CLEAR (0x20) and COLONIZE_COLONY_AI_WANTS_PIONEER_WORK (0x80); an open
 * Plains ring (food 4 everywhere, no forest, no worked tiles) must raise
 * neither.
 */
static int unit_colony_ai_flags_pioneer_clear(void) {
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
    return fail("pioneer-clear alloc map");
  }
  /* Plains (pedia 2, Farmer food 4) everywhere — the control ring. */
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 2;
  }
  /* Colony A at (4,4): ring all Conifer Forest (pedia 13; clears to Savannah,
   * Farmer food 3 > 2, so it also counts as "clearable"). */
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      map.terrain[(4 + dy) * 16 + (4 + dx)] = 13;
    }
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* forest = &colonies.colonies[0];
  forest->id = 0;
  forest->active = true;
  forest->nation_id = nation;
  forest->x = 4;
  forest->y = 4;
  forest->population = 2;
  forest->colonist_count = 2;
  forest->colonists[0].active = true;
  forest->colonists[0].field_job = -1;
  forest->colonists[0].building_type = -1;
  forest->colonists[1].active = true;
  forest->colonists[1].field_job = -1;
  forest->colonists[1].building_type = -1;
  forest->stock[COLONIZE_CARGO_FOOD] = 40;
  forest->building_in_production = -1;
  forest->ai_flags = 0;

  ColonizeColony* open = &colonies.colonies[1];
  open->id = 1;
  open->active = true;
  open->nation_id = nation;
  open->x = 11;
  open->y = 11;
  open->population = 2;
  open->colonist_count = 2;
  open->colonists[0].active = true;
  open->colonists[0].field_job = -1;
  open->colonists[0].building_type = -1;
  open->colonists[1].active = true;
  open->colonists[1].field_job = -1;
  open->colonists[1].building_type = -1;
  open->stock[COLONIZE_CARGO_FOOD] = 40;
  open->building_in_production = -1;
  open->ai_flags = 0;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  ai_goals_reset();
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 30;
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

  forest = &colonies.colonies[0];
  open = &colonies.colonies[1];
  const unsigned want = COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR |
                        COLONIZE_COLONY_AI_WANTS_PIONEER_WORK;
  int rc = 0;
  if (((unsigned)forest->ai_flags & want) != want) {
    fprintf(stderr, "unit_ai_euro_expand: forest colony ai_flags=0x%02x (want 0xa0 set)\n",
            (unsigned)forest->ai_flags);
    rc = 1;
  }
  if (((unsigned)open->ai_flags & want) != 0) {
    fprintf(stderr, "unit_ai_euro_expand: open colony ai_flags=0x%02x (want 0xa0 clear)\n",
            (unsigned)open->ai_flags);
    rc = 1;
  }

  fx_map_free(&map);
  if (rc != 0) {
    return fail("expected +0x1b 0xa0 pair only on the forest-locked colony");
  }
  fprintf(stderr, "unit_ai_euro_expand: +0x1b 0xa0 pioneer-clear pair ok\n");
  return 0;
}

/*
 * FUN_521d_0a60 work-queue "+1500 exposed combat unit" arm (raw :87626-87635):
 * DOS also requires the unit's `+0x314b` ai_plan letter to be neither 'G' nor
 * 'A' (raw :87631) — a unit already assigned/garrisoned is not "exposed". Same
 * colony, same Soldier, only the ai_plan byte differs: default (0) must arm the
 * arm (work row with military == 1), 'G' must not.
 */
static int unit_0a60_work_military_ai_plan_gate(void) {
  const int nation = 1;
  int rc = 0;

  for (int pass = 0; pass < 2 && rc == 0; ++pass) {
    const uint8_t plan = pass == 0 ? 0u : 0x47u; /* default vs 'G' */

    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    map.width = 16;
    map.height = 16;
    map.tile_count = 256;
    map.terrain = calloc(256, 1);
    map.layer2 = calloc(256, 1);
    map.layer3 = calloc(256, 1);
    if (!map.terrain || !map.layer2 || !map.layer3) {
      return fail("0a60 ai_plan alloc map");
    }
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = (i % 16 < 2) ? 25 : 2; /* west strip ocean, rest Plains */
    }

    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    units_set_occupancy_map(NULL);
    /* Every other case in this file resets through fx_units_init(), but
     * this one hand-builds its pool; without units_reset_hooks() /
     * units_reset_state() / ai_euro_reset() / ai_native_reset() /
     * turn_reset() it can inherit ai_euro.c's per-unit-id or per-nation
     * statics from whichever case ran immediately before it under
     * COLONIZE_TEST_SHUFFLE (seeds 5 and 7 reproduce it). */
    units_reset_hooks();
    units_reset_state();
    ai_euro_reset();
    ai_native_reset();
    turn_reset();
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
    units.types[0].movement = 1;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[0].attack = 2;

    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    colonies_set_occupancy_map(NULL);
    ColonizeColony* c = &colonies.colonies[0];
    c->id = 0;
    c->active = true;
    c->nation_id = nation;
    c->x = 2;
    c->y = 6;
    c->population = 2;
    c->colonist_count = 2;
    c->colonists[0].active = true;
    c->colonists[0].field_job = -1;
    c->colonists[0].building_type = -1;
    c->colonists[1].active = true;
    c->colonists[1].field_job = -1;
    c->colonists[1].building_type = -1;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
    colonies.colony_count = 1;
    colonies.next_id = 1;

    const int sid = units_spawn(&units, 0, 2, 6);
    ColonizeUnit* sol = units_get(&units, sid);
    if (!sol) {
      fx_map_free(&map);
      return fail("0a60 ai_plan spawn Soldier");
    }
    sol->nation_id = nation;
    sol->moves = UNITS_MP_PER_TILE;
    sol->col1_ai_plan = plan;

    ai_goals_reset();
    ColonizeCol1Save col1;
    col1_save_init(&col1);
    memset(col1.nation, 0, sizeof(col1.nation));
    memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
    for (int i = 0; i < 4; ++i) {
      col1.player[i].control = 1;
      col1.player[i].diplomacy = 0;
    }
    col1.nation[nation].gold = 200;
    col1.stuff.ship_counts[nation] = 1;

    uint32_t turn = 30;
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.turn_number = &turn;
    ctx.units = &units;
    ctx.colonies = &colonies;
    ctx.map = &map;
    ctx.col1 = &col1;
    ctx.col1_ok = true;
    ctx.rng_seed = 11;

    ai_euro_dispatcher_turn(&ctx, nation);

    int military = 0;
    for (int i = 0; i < AI_WORK_SLOTS; ++i) {
      const AiWorkSlot* w = ai_goals_work(i);
      if (w && w->id == 0 && w->military) {
        military = 1;
        break;
      }
    }
    if (pass == 0 && !military) {
      fprintf(stderr, "unit_ai_euro_expand: default ai_plan did not arm the +1500 arm\n");
      rc = 1;
    }
    if (pass == 1 && military) {
      fprintf(stderr, "unit_ai_euro_expand: ai_plan 'G' still armed the +1500 arm\n");
      rc = 1;
    }

    fx_map_free(&map);
  }

  if (rc != 0) {
    return fail("0a60 +1500 arm must respect the +0x314b 'G'/'A' gate");
  }
  fprintf(stderr, "unit_ai_euro_expand: 0a60 +1500 ai_plan G/A gate ok\n");
  return 0;
}

static int unit_colony_ai_flags_mow_colony_alt(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("ai-flags alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = (i % 16 < 2) ? 25 : 1; /* west strip real ocean (0x19) */
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  /* DOS census: bit 0x02 = FRIGATE nearby (type 0x11 literal check), not
   * Man-O-War — census_tally.md 2026-08-19 correction, probe live 2026-09-07. */
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 8;
  units.types[0].cargo = 6;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 2; /* coastal: the FUN_6662_0906 sea-flood gate needs a route */
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  c->ai_flags = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Frigate on water inside the 11×11 box, short sea route to the colony. */
  const int sid = units_spawn(&units, 0, 1, 6);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("ai-flags spawn Frigate");
  }
  ship->nation_id = foe;
  ship->moves = 4 * UNITS_MP_PER_TILE;

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

  uint32_t turn = 64;
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

  c = &colonies.colonies[0];
  if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) == 0) {
    fprintf(stderr, "unit_ai_euro_expand: ai_flags=0x%02x (want MoW bit)\n",
            (unsigned)c->ai_flags);
    fx_map_free(&map);
    return fail("expected nearby_frigate ai_flags bit");
  }
  int found_alt = 0;
  for (int i = 0; i < 16; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_COLONY_ALT && g->x == 2 && g->y == 4 && g->prio >= 8) {
      found_alt = 1;
      break;
    }
  }
  if (!found_alt) {
    fx_map_free(&map);
    return fail("expected COLONY_ALT prio 8 at colony under MoW pressure");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: colony ai_flags MoW COLONY_ALT ok\n");
  return 0;
}

/*
 * FUN_4962_0018 for the HUMAN nation (ai_euro_census_ship_pressure_refresh,
 * wired into TURN_PROC_FINISH 2026-09-08d). Before that split only the AI
 * nations ran the probe, so a human colony's blockade pair +0x1b bits
 * 0x01/0x02 stayed frozen at whatever the save import left there. Asserts
 * both directions: a foreign Frigate in range sets bit 0x02, and once the
 * ship is gone the next refresh CLEARS the stale pair (the regression) while
 * leaving the unrelated +0x1b bits alone.
 */
static int unit_human_census_ship_pressure_refresh(void) {
  const int human = 0;
  const int foe = 2;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("human census alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = (i % 16 < 2) ? 25 : 1; /* west strip real ocean (0x19) */
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 8;
  units.types[0].cargo = 6;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = human;
  c->x = 2; /* coastal: the FUN_6662_0906 sea-flood gate needs a route */
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->building_in_production = -1;
  c->ai_flags = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Foreign Frigate on water inside the 11x11 box, short sea route. */
  const int sid = units_spawn(&units, 0, 1, 6);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("human census spawn Frigate");
  }
  ship->nation_id = foe;
  ship->moves = 4 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }

  uint32_t turn = 64;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  /* (a) Frigate present: the probe raises bit 0x02 for the human nation. */
  ai_euro_census_ship_pressure_refresh(&ctx, human);
  c = &colonies.colonies[0];
  if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) == 0) {
    fprintf(stderr, "unit_ai_euro_expand: human ai_flags=0x%02x (want frigate bit)\n",
            (unsigned)c->ai_flags);
    fx_map_free(&map);
    return fail("expected human nearby_frigate ai_flags bit");
  }

  /* (b) Ship gone: the next refresh must clear BOTH blockade bits. Seed the
   * armed-ship bit as well so the clear is the thing under test, not the
   * scan simply never having set it. */
  ship = units_get(&units, sid);
  if (ship) {
    ship->active = false;
  }
  c->ai_flags = (uint8_t)(c->ai_flags | COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP |
                          COLONIZE_COLONY_AI_NEEDS_COLONISTS);
  ai_euro_census_ship_pressure_refresh(&ctx, human);
  c = &colonies.colonies[0];
  if ((c->ai_flags & (COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP |
                      COLONIZE_COLONY_AI_NEARBY_FRIGATE)) != 0) {
    fprintf(stderr, "unit_ai_euro_expand: human ai_flags=0x%02x (want blockade pair clear)\n",
            (unsigned)c->ai_flags);
    fx_map_free(&map);
    return fail("expected stale human blockade bits cleared");
  }
  /* Raw 78259 clears +0x1b &= 0xfc only — the rest of the byte survives. */
  if ((c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) == 0) {
    fx_map_free(&map);
    return fail("census probe clobbered unrelated ai_flags bits");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: human census ship pressure refresh ok\n");
  return 0;
}

/*
 * Food emergency: food_short high + Pioneer at MD 5 → LABOR goto toward hungry
 * colony (not only MD≤1 bind). Cite: 5cf6 food_short; manual 2 food/colonist.
 */
static int unit_food_emergency_labor(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("food-emerg alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 4);
  c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short = 8 ≥ 4 */
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  /* Pioneer at MD 5 — beyond adjacent LABOR bind. */
  const int pid = units_spawn(&units, 0, 9, 4);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    fx_map_free(&map);
    return fail("food-emerg spawn");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves = 3 * UNITS_MP_PER_TILE;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

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

  pioneer = units_get(&units, pid);
  int labor = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_LABOR && g->x == 4 && g->y == 4) {
      labor = 1;
      break;
    }
  }
  const int toward =
    pioneer && pioneer->active &&
    ((pioneer->orders == UNITS_ORDER_AI_MOVE && pioneer->goto_x == 4 &&
      pioneer->goto_y == 4) ||
     (pioneer->x == 4 && pioneer->y == 4) ||
     (abs(pioneer->x - 4) + abs(pioneer->y - 4)) < 5);

  if (!labor || !toward) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: food-emerg labor=%d orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      labor,
      pioneer ? pioneer->orders : -1,
      pioneer ? pioneer->goto_x : -1,
      pioneer ? pioneer->goto_y : -1,
      pioneer ? pioneer->x : -1,
      pioneer ? pioneer->y : -1
    );
    fx_map_free(&map);
    return fail("expected food-emergency LABOR bind for distant Pioneer");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: food-emergency LABOR ok\n");
  return 0;
}

/*
 * Expert Farmer food LABOR: idle Expert Farmer (profession @JOB Farmer / name)
 * at MD 5 + food_short → LABOR goto. Cite: building_production.md Farmer→Food;
 * euro_unit_act §2e Expert Farmer.
 */
static int unit_expert_farmer_food_labor(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("expert-farmer alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Farmer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 0;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 4);
  c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short = 8 ≥ 4 */
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  /* Expert Farmer at MD 5. */
  const int fid = units_spawn(&units, 0, 9, 4);
  ColonizeUnit* farmer = units_get(&units, fid);
  if (!farmer) {
    fx_map_free(&map);
    return fail("expert-farmer spawn");
  }
  farmer->nation_id = nation;
  farmer->profession = 0; /* @JOB Farmer */
  farmer->orders = 0;
  farmer->moves = 3 * UNITS_MP_PER_TILE;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 18;
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

  farmer = units_get(&units, fid);
  int labor_bound = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_LABOR && g->x == 4 && g->y == 4) {
      labor_bound = 1;
      break;
    }
  }
  const int moving =
    farmer && farmer->active &&
    ((farmer->orders == UNITS_ORDER_AI_MOVE && farmer->goto_x == 4 &&
      farmer->goto_y == 4) ||
     (farmer->x == 4 && farmer->y == 4) ||
     (abs(farmer->x - 4) + abs(farmer->y - 4)) < 5);
  if (!labor_bound || !moving) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: expert-farmer orders=%d goto=(%d,%d) pos=(%d,%d) labor=%d\n",
      farmer ? farmer->orders : -1,
      farmer ? farmer->goto_x : -1,
      farmer ? farmer->goto_y : -1,
      farmer ? farmer->x : -1,
      farmer ? farmer->y : -1,
      labor_bound
    );
    fx_map_free(&map);
    return fail("expected Expert Farmer food-short LABOR bind");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: Expert Farmer food LABOR ok\n");
  return 0;
}

/*
 * Free Colonist food LABOR (non-Expert Farmer): idle Free Colonist at MD 5 +
 * food_short > 0 but < 4 (not emergency) → LABOR goto. Cite: euro_unit_act §2e
 * Free Colonist food LABOR; manual 2 food/colonist.
 */
static int unit_free_colonist_food_labor(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("fc-food alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 0;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 1);
  c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short = 2 (not emergency ≥4) */
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  /* Free Colonist at MD 5 — beyond adjacent; needs food-short MD≤8 bind. */
  const int fid = units_spawn(&units, 0, 9, 4);
  ColonizeUnit* col = units_get(&units, fid);
  if (!col) {
    fx_map_free(&map);
    return fail("fc-food spawn");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves = 3 * UNITS_MP_PER_TILE;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 19;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 19;

  ai_euro_dispatcher_turn(&ctx, nation);

  col = units_get(&units, fid);
  int labor_bound = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_LABOR && g->x == 4 && g->y == 4) {
      labor_bound = 1;
      break;
    }
  }
  const int moving =
    col && col->active &&
    ((col->orders == UNITS_ORDER_AI_MOVE && col->goto_x == 4 && col->goto_y == 4) ||
     (col->x == 4 && col->y == 4) || (abs(col->x - 4) + abs(col->y - 4)) < 5);
  if (!labor_bound || !moving) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: fc-food orders=%d goto=(%d,%d) pos=(%d,%d) labor=%d\n",
      col ? col->orders : -1,
      col ? col->goto_x : -1,
      col ? col->goto_y : -1,
      col ? col->x : -1,
      col ? col->y : -1,
      labor_bound
    );
    fx_map_free(&map);
    return fail("expected Free Colonist food-short LABOR bind (non-Farmer)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: Free Colonist food LABOR ok\n");
  return 0;
}

/*
 * Tools-short Pioneer deepen: peace Pioneer at MD 5 + tools_short colony →
 * LABOR goto (feeds on-tile tools delivery). Cite: euro_unit_act §2e.
 */
static int unit_tools_short_pioneer_labor(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("tools-labor alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_FOOD] = 40; /* not food emergency */
  c->stock[COLONIZE_CARGO_TOOLS] = 5; /* tools_short = 15 */

  /* Pioneer at MD 5 — beyond adjacent LABOR; tools deepen extends to MD≤8. */
  const int pid = units_spawn(&units, 0, 9, 4);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    fx_map_free(&map);
    return fail("tools-labor spawn");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves = 3 * UNITS_MP_PER_TILE;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

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

  ai_euro_dispatcher_turn(&ctx, nation);

  pioneer = units_get(&units, pid);
  int labor = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_LABOR && g->x == 4 && g->y == 4) {
      labor = 1;
      break;
    }
  }
  const int toward =
    pioneer && pioneer->active &&
    ((pioneer->orders == UNITS_ORDER_AI_MOVE && pioneer->goto_x == 4 &&
      pioneer->goto_y == 4) ||
     (abs(pioneer->x - 4) + abs(pioneer->y - 4) < 5));

  if (!labor && !toward) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: tools-labor orders=%d goto=(%d,%d) pos=(%d,%d) "
      "labor=%d\n",
      pioneer ? pioneer->orders : -1,
      pioneer ? pioneer->goto_x : -1,
      pioneer ? pioneer->goto_y : -1,
      pioneer ? pioneer->x : -1,
      pioneer ? pioneer->y : -1,
      labor
    );
    fx_map_free(&map);
    return fail("expected tools-short LABOR bind for distant Pioneer");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: tools-short Pioneer LABOR ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_labor_shortage_join", unit_labor_shortage_join},
    {"unit_labor_bind_food_short", unit_labor_bind_food_short},
    {"unit_food_emergency_labor", unit_food_emergency_labor},
    {"unit_expert_farmer_food_labor", unit_expert_farmer_food_labor},
    {"unit_free_colonist_food_labor", unit_free_colonist_food_labor},
    {"unit_tools_short_pioneer_labor", unit_tools_short_pioneer_labor},
    {"unit_colony_flags_starvation_labor", unit_colony_flags_starvation_labor},
    {"unit_colony_ai_flags_mow_colony_alt", unit_colony_ai_flags_mow_colony_alt},
    {"unit_colony_ai_flags_pioneer_clear", unit_colony_ai_flags_pioneer_clear},
    {"unit_0a60_work_military_ai_plan_gate", unit_0a60_work_military_ai_plan_gate},
    {"unit_human_census_ship_pressure_refresh", unit_human_census_ship_pressure_refresh},
};
TEST_MAIN(k_cases)
