/* Slice of the former tests/unit/test_ai_euro_expand.c (split by feature 2026-09-23):
 * 5d04 Europe purchases (caravel/merchantman/galleon/frigate), treasury skip, multistep military. */
#include "test_ai_euro_expand_common.h"

/*
 * 2026-09-08 — 2 more scenarios RETIRED with the Linux-shaped 06ae extras
 * they were written against (`unit_second_wave`, ring-2..4 rescan founding
 * around an existing colony; `unit_second_colony_coastal_prefer`, the +10/+40
 * coastal bias). DOS's 06ae scores only DS:0x2f77[class] + 0492*0x10 + the
 * 074a nibble over the 3x3 ring, and every tile within Chebyshev 1 of a
 * colony is unfoundable, so a colony-origin ring scan can never yield a
 * second-colony site; DOS seeds second colonies from the 3180 map scan
 * instead. DOS-side founding coverage lives in the goldens
 * (golden_ai_turns / golden_ai_joint), byte-green across the swap.
 */

/*
 * tools_short>30 + Wagon Train type → hire wagon once (TOOLS on wagon);
 * second planning pass with free cargo slot prefers Pioneer (not a 2nd wagon).
 */

/*
 * Thin multi-step land 20e6: Soldier with moves>=3 on MILITARY goto drains
 * scored steps in one dispatcher act when path is clear (MP full-drain).
 */
static int unit_multistep_military(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("multistep alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 2;
  c->y = 2;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  ColonizeColony* enemy = &colonies.colonies[1];
  enemy->id = 1;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 12;
  enemy->y = 2;
  enemy->population = 2;
  enemy->colonist_count = 2;
  enemy->stock[COLONIZE_CARGO_FOOD] = 20;
  enemy->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int sid = units_spawn(&units, 0, 4, 2);
  ColonizeUnit* soldier = units_get(&units, sid);
  if (!soldier) {
    fx_map_free(&map);
    return fail("multistep spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->moves = 4 * UNITS_MP_PER_TILE;
  soldier->orders = 0;

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
  ai_goals_upsert_primary(nation, 12, 2, AI_GOAL_MILITARY, 6);

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

  const int x0 = soldier->x;
  ai_euro_dispatcher_turn(&ctx, nation);
  soldier = units_get(&units, sid);
  if (!soldier || !soldier->active) {
    fx_map_free(&map);
    return fail("multistep soldier inactive");
  }
  const int advanced = soldier->x - x0;
  if (advanced < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: multistep x %d→%d (want ≥3) orders=%d goto=(%d,%d)\n",
      x0,
      soldier->x,
      soldier->orders,
      soldier->goto_x,
      soldier->goto_y
    );
    fx_map_free(&map);
    return fail("expected MILITARY MP-drain advance of ≥3 tiles");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_expand: MILITARY MP-drain ok (x %d→%d)\n",
    x0,
    soldier->x
  );
  return 0;
}

/*
 * Thin 5d04 / 5c3c: no Europe ship + gold ≥ Caravel 1000$ → buy Caravel at
 * Europe dock stand-in. Gold tuned so after treasury bump + purchase, hire_cost
 * is not met (ship only). Cite: europe purchase.png Caravel; FUN_521d_5c3c.
 */

/*
 * Thin 5d04 mid-game: colonies ≥ 6 still runs Europe ship-buy ladder (peace
 * early-settle hire matrix stays <6). Cite: euro_goals 03d0 <0x30; unpark #4.
 */
static int unit_5d04_buy_caravel_colonies_ge6(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("buy-caravel-ge6 alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  for (int i = 0; i < 6; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + (i % 3) * 2;
    c->y = 2 + (i / 3) * 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->stock[COLONIZE_CARGO_TOOLS] = 30;
    c->building_in_production = -1;
  }
  colonies.colony_count = 6;
  colonies.next_id = 6;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  /* Caravel purchase (1000$) + leftover under hire_cost (200$ at diff=0),
   * so the hire ladder is blocked by treasury, not skipped by having zero
   * gold outright (a fixture that leaves exactly 0 can't tell "correctly
   * gated" apart from "spent nothing" and made the test brittle). */
  col1.nation[nation].gold = 1150;
  /* Truthful census: no ships, no hold capacity — the real FUN_521d_5c3c
   * ladder's Caravel arm (ship_cargo_totals < 3) fires; the no-ships gold
   * floor is a no-op at 1150 gold. */
  col1.stuff.colony_counts[nation] = 6;

  ai_goals_reset();

  uint32_t turn = 40;
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

  int caravel_n = 0;
  int caravel_europe = 0;
  int any_pax = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, u->type_index);
    if (!ty || !strstr(ty->name, "Caravel")) {
      continue;
    }
    caravel_n++;
    if (u->x >= 200 || u->y >= 200) {
      caravel_europe = 1;
    }
    if (u->cargo_count > 0) {
      any_pax = 1;
    }
  }

  const unsigned gold = col1.nation[nation].gold;
  if (caravel_n != 1 || !caravel_europe || any_pax || gold >= 200 || gold == 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: buy-caravel-ge6 n=%d europe=%d pax=%d gold=%u\n",
      caravel_n,
      caravel_europe,
      any_pax,
      gold
    );
    fx_map_free(&map);
    return fail("expected Europe Caravel buy at colonies>=6 (no settle hire)");
  }

  fx_map_free(&map);
  fprintf(
    stderr, "unit_ai_euro_expand: 5d04 buy-caravel-colonies-ge6 ok (gold=%u)\n", gold
  );
  return 0;
}

static int unit_5d04_buy_caravel_no_ship(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("buy-caravel alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 2);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 30; /* no tools-cargo pressure */

  /* No ships — planning must purchase Caravel before hire matrix can run. */

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0; /* hire_cost=200; treasury bump is 0 at diff 0
                              * (raw local_2e = difficulty * local_12, so
                              * difficulty 0 always bumps by 0 — do not
                              * rely on it here). */
  /* Buy Caravel (1000) leaving a remainder under hire_cost (200) but not
   * exactly 0, so the assertion below can tell "correctly gated by
   * treasury" apart from "spent nothing at all". */
  col1.nation[nation].gold = 1120;
  /* Truthful census: no ships, no hold capacity — the real FUN_521d_5c3c
   * ladder's Caravel arm (ship_cargo_totals < 3) fires; the no-ships gold
   * floor is a no-op at 1120 gold. */
  col1.stuff.colony_counts[nation] = 1;

  ai_goals_reset();

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

  int caravel_n = 0;
  int caravel_europe = 0;
  int any_pax = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, u->type_index);
    if (!ty || !strstr(ty->name, "Caravel")) {
      continue;
    }
    caravel_n++;
    if (u->x >= 200 || u->y >= 200) {
      caravel_europe = 1;
    }
    if (u->cargo_count > 0) {
      any_pax = 1;
    }
  }

  const unsigned gold = col1.nation[nation].gold;
  /* bump≈30, purchase 1000 → remaining well under hire_cost (200). */
  if (caravel_n != 1 || !caravel_europe || any_pax || gold >= 200 || gold == 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: buy-caravel n=%d europe=%d pax=%d gold=%u\n",
      caravel_n,
      caravel_europe,
      any_pax,
      gold
    );
    fx_map_free(&map);
    return fail("expected one Europe Caravel purchase, no hire pax");
  }

  fx_map_free(&map);
  fprintf(
    stderr, "unit_ai_euro_expand: 5d04 buy-caravel-no-ship ok (gold=%u)\n", gold
  );
  return 0;
}

/*
 * Thin 5d04 / 5c3c: no Europe ship + at war + gold ≥ 5000$ → prefer Frigate
 * over Galleon/Merchantman/Caravel. Cite: purchase.png Frigate 5000$; war hunt.
 */
static int unit_5d04_buy_frigate_at_war(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("buy-frigate alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 5;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Merchantman");
  units.types[2].movement = 4;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[2].cargo = 4;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Galleon");
  units.types[3].movement = 4;
  units.types[3].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[3].cargo = 6;
  snprintf(units.types[4].name, sizeof(units.types[4].name), "Frigate");
  units.types[4].movement = 5;
  units.types[4].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[4].cargo = 4;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 5000;
  col1.nation[foe].gold = 500;
  ai_diplo_declare_war(&col1, nation, foe);
  /* +100 headroom for pre-ladder trickle spends. */
  col1.nation[nation].gold = 5100;
  /* Census: small fleet, 3 holds hauled — blocks the 5c3c Caravel arm (<3);
   * the Frigate arm (armed<8, RNG coin, no clear strongest navy) is the
   * first affordable buy at 5100 gold. */
  col1.stuff.ship_counts[nation] = 1;
  col1.stuff.ship_cargo_totals[nation] = 3;
  col1.stuff.colony_counts[nation] = 3;

  ai_goals_reset();

  uint32_t turn = 27;
  ColonizeDosRng ladder_rng;
  dos_rng_seed(&ladder_rng, 27182); /* 5c3c Frigate arm is an RNG coin flip */
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = &ladder_rng;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  int frig_n = 0;
  int other_ship = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, u->type_index);
    if (!ty) {
      continue;
    }
    if (strstr(ty->name, "Frigate")) {
      frig_n++;
    } else if (strstr(ty->name, "Caravel") || strstr(ty->name, "Merchantman") ||
               strstr(ty->name, "Galleon")) {
      other_ship++;
    }
  }

  const unsigned gold = col1.nation[nation].gold;
  if (frig_n != 1 || other_ship != 0 || gold >= 200) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: buy-frigate f=%d other=%d gold=%u\n",
      frig_n,
      other_ship,
      gold
    );
    fx_map_free(&map);
    return fail("expected Frigate purchase when at war with gold>=5000");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: 5d04 buy-frigate-at-war ok (gold=%u)\n", gold);
  return 0;
}

/*
 * Thin 5d04 / 5c3c: no Europe ship + at war + gold ≥ 3000$ → prefer Galleon
 * over Merchantman/Caravel. Cite: europe purchase.png Galleon 3000$; war transport.
 */
static int unit_5d04_buy_galleon_at_war(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("buy-galleon alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 4;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Merchantman");
  units.types[2].movement = 4;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[2].cargo = 4;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Galleon");
  units.types[3].movement = 4;
  units.types[3].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[3].cargo = 6;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 0; /* would prefer Merchantman if peace */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 3000;
  col1.nation[foe].gold = 500;
  ai_diplo_declare_war(&col1, nation, foe);
  /* Replenish after war sting; +100 headroom for pre-ladder trickle spends. */
  col1.nation[nation].gold = 3100;
  /* Census: small fleet, 3 holds hauled — blocks the 5c3c Caravel arm (<3),
   * leaves the Galleon arm (RNG 3-in-4) and Merchantman arm (<0xc) open;
   * with 3100 gold the Galleon arm is the first affordable buy. */
  col1.stuff.ship_counts[nation] = 1;
  col1.stuff.ship_cargo_totals[nation] = 3;
  col1.stuff.colony_counts[nation] = 3;

  ai_goals_reset();

  uint32_t turn = 23;
  ColonizeDosRng ladder_rng;
  dos_rng_seed(&ladder_rng, 1); /* 5c3c Galleon arm is a 3-in-4 RNG roll */
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = &ladder_rng;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  int galleon_n = 0;
  int other_ship = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, u->type_index);
    if (!ty) {
      continue;
    }
    if (strstr(ty->name, "Galleon")) {
      galleon_n++;
    } else if (strstr(ty->name, "Caravel") || strstr(ty->name, "Merchantman")) {
      other_ship++;
    }
  }

  const unsigned gold = col1.nation[nation].gold;
  if (galleon_n != 1 || other_ship != 0 || gold >= 200) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: buy-galleon g=%d other=%d gold=%u\n",
      galleon_n,
      other_ship,
      gold
    );
    fx_map_free(&map);
    return fail("expected Galleon purchase when at war");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: 5d04 buy-galleon-at-war ok (gold=%u)\n", gold);
  return 0;
}

/*
 * Thin 5d04 / 5c3c: no Europe ship + cargo pressure (tools_short) + gold ≥ 2000$
 * → prefer Merchantman over Caravel. Cite: europe purchase.png Merchantman 2000$.
 */
static int unit_5d04_buy_merchantman_cargo_pressure(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("buy-merchantman alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Merchantman");
  units.types[2].movement = 4;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[2].cargo = 4;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 0; /* tools_short high */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  /* After bump ≈2030 → Merchantman 2000 → ~30 < hire_cost → ship only. */
  col1.nation[nation].gold = 2000;
  /* Census: a small fleet already hauling (3 holds) — blocks the 5c3c
   * Caravel arm (< 3) while the Merchantman arm (< 0xc) stays open. */
  col1.stuff.ship_counts[nation] = 1;
  col1.stuff.ship_cargo_totals[nation] = 3;
  col1.stuff.colony_counts[nation] = 3;

  ai_goals_reset();

  uint32_t turn = 21;
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

  int merchant_n = 0;
  int caravel_n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, u->type_index);
    if (!ty) {
      continue;
    }
    if (strstr(ty->name, "Merchantman")) {
      merchant_n++;
    } else if (strstr(ty->name, "Caravel")) {
      caravel_n++;
    }
  }

  const unsigned gold = col1.nation[nation].gold;
  if (merchant_n != 1 || caravel_n != 0 || gold >= 200) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: buy-merchantman m=%d c=%d gold=%u\n",
      merchant_n,
      caravel_n,
      gold
    );
    fx_map_free(&map);
    return fail("expected Merchantman purchase under cargo pressure");
  }

  fx_map_free(&map);
  fprintf(
    stderr, "unit_ai_euro_expand: 5d04 buy-merchantman-cargo ok (gold=%u)\n", gold
  );
  return 0;
}

/*
 * Thin 5d04 / 5c3c second transport: Europe Caravel already full + gold ≥ 1000$
 * → buy another Caravel; hire boards the new empty ship (not the full one).
 */
static int unit_5d04_buy_caravel_ship_full(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("buy-caravel-full alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 1; /* one pax → full after boarding one */

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* tools_short high so hire wants a Pioneer after second Caravel buy. */
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int full_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* full = units_get(&units, full_id);
  if (!full) {
    fx_map_free(&map);
    return fail("buy-caravel-full spawn ship");
  }
  full->nation_id = nation;
  full->moves = 0;
  /* Fill the only passenger slot. */
  {
    const int pax_id = units_spawn_allow_stack(&units, 0, 200, 100);
    ColonizeUnit* pax = units_get(&units, pax_id);
    if (!pax || !units_board_stacked(&units, pax_id, full_id)) {
      fx_map_free(&map);
      return fail("buy-caravel-full board filler");
    }
    pax->nation_id = nation;
  }

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  /* bump≈30 + 1300 → buy 1000 → ~330 ≥ hire_cost 200 → Pioneer on new ship. */
  col1.nation[nation].gold = 1400; /* buy 1000 + tail cargo load 100 + hire 200 */
  /* Truthful census: one full Caravel (2 holds), colony pop 2 — the ladder
   * gate (cargo <= pop_proxy/2 + colonies) passes and the Caravel arm
   * (ship_cargo_totals < 3) buys the second hull. */
  col1.stuff.ship_counts[nation] = 1;
  col1.stuff.ship_cargo_totals[nation] = 2;
  col1.stuff.colony_counts[nation] = 1;
  col1.stuff.census_pop_proxy[nation] = 2;

  ai_goals_reset();

  uint32_t turn = 19;
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

  int caravel_n = 0;
  int new_in_europe = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, u->type_index);
    if (!ty || !strstr(ty->name, "Caravel")) {
      continue;
    }
    caravel_n++;
    if (u->id != full_id && (u->x >= 200 || u->y >= 200)) {
      new_in_europe = 1;
    }
  }

  /* DOS 5c3c buys the second hull; boarding/loading it happens on the hire
   * tail's own cadence (this turn it bought 100 tools cargo instead), so
   * the old "hire aboard the new ship same turn" thin-matrix expectation
   * is dropped — the purchase itself is the ported behavior. */
  if (caravel_n < 2 || !new_in_europe) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: buy-caravel-full n=%d new=%d gold=%u full_cargo=%d\n",
      caravel_n,
      new_in_europe,
      (unsigned)col1.nation[nation].gold,
      full->cargo_count
    );
    fx_map_free(&map);
    return fail("expected second Europe Caravel purchase (5c3c ladder)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: 5d04 buy-caravel-ship-full ok\n");
  return 0;
}

/*
 * 5d04 treasury gate: gold below colonist hire_cost → no Europe hire / tools-cargo.
 */
static int unit_treasury_skip_hire(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("treasury alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 20;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("treasury spawn ship");
  }
  ship->nation_id = nation;
  ship->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0; /* hire_cost=200; bump≈30 → still <200 if gold=0 */
  col1.nation[nation].gold = 0;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  ai_goals_reset();

  uint32_t turn = 12;
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
  ship = units_get(&units, sid);
  if (ship && ship->cargo_count > 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: treasury cargo=%d gold=%u\n",
      ship->cargo_count,
      (unsigned)col1.nation[nation].gold
    );
    fx_map_free(&map);
    return fail("expected no Europe hire when gold < hire_cost");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: treasury skip-hire ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_multistep_military", unit_multistep_military},
    {"unit_treasury_skip_hire", unit_treasury_skip_hire},
    {"unit_5d04_buy_caravel_colonies_ge6", unit_5d04_buy_caravel_colonies_ge6},
    {"unit_5d04_buy_caravel_no_ship", unit_5d04_buy_caravel_no_ship},
    {"unit_5d04_buy_merchantman_cargo_pressure", unit_5d04_buy_merchantman_cargo_pressure},
    {"unit_5d04_buy_galleon_at_war", unit_5d04_buy_galleon_at_war},
    {"unit_5d04_buy_frigate_at_war", unit_5d04_buy_frigate_at_war},
    {"unit_5d04_buy_caravel_ship_full", unit_5d04_buy_caravel_ship_full},
};
TEST_MAIN(k_cases)
