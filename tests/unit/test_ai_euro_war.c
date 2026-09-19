/* Smoke: at-war Euro mid-hire / MILITARY bind + G stance + thin naval hunt. */
/*
 * 2026-09-07e — 6 scenarios RETIRED with the Linux-shaped 5d04 hire matrix
 * they were written against: `unit_mid_hire_mil_colonies_ge6`,
 * `unit_mid_hire_dragoon_prefer`, `unit_mid_hire_veteran_prefer`,
 * `unit_at_war_tools_prefer_soldier`, `unit_mid_hire_artillery`,
 * `unit_artillery_treasury_fallback`. FUN_521d_5d04 has no war/peace hire
 * fork, no Dragoon-over-Soldier or Veteran-over-Soldier preference and no
 * `units_find_type("Artillery")` gold gate — those were Linux inventions,
 * deleted from ai_euro.c when the DOS hire matrix (raw 92568-93070,
 * `ai_euro_5d04_hire_ladder_tail`) became the only Europe hire economy.
 * What DOS actually does: Colonist + 50 Muskets -> Soldier, then + 50
 * Horses -> Dragoon, gated on the DS:0xa0db colony muskets-need tally and
 * per-unit RNG, with Artillery bought from the 5c3c purchase table only
 * when Europe holds none. `unit_mid_hire_mil` is KEPT — it accepts the
 * MILITARY bind path, which is unaffected.
 */
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_euro_internal.h"
#include "core/ai_goals.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_NAME "unit_ai_euro_war"
#include "../common/ai_fixture.h"
#include "../common/test_fail.h"
#include "../common/test_runner.h"


static int unit_mid_hire_mil(void) {
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
    return fail("alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
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
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Free Colonist");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Soldier");
  units.types[3].movement = 1;
  units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[3].attack = 2;
  units.types[3].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 4;
  own->y = 4;
  own->population = 3;
  own->colonist_count = 3;
  own->stock[COLONIZE_CARGO_FOOD] = 40;
  own->building_in_production = -1;

  /* Second own colony — unlocks G continent stance (own ≥ 2). */
  ColonizeColony* own2 = &colonies.colonies[1];
  own2->id = 1;
  own2->active = true;
  own2->nation_id = nation;
  own2->x = 6;
  own2->y = 4;
  own2->population = 2;
  own2->colonist_count = 2;
  own2->stock[COLONIZE_CARGO_FOOD] = 20;
  own2->building_in_production = -1;

  ColonizeColony* enemy = &colonies.colonies[2];
  enemy->id = 2;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 10;
  enemy->y = 10;
  enemy->population = 2;
  enemy->colonist_count = 2;
  enemy->stock[COLONIZE_CARGO_FOOD] = 20;
  enemy->building_in_production = -1;
  colonies.colony_count = 3;
  colonies.next_id = 3;

  /* Idle Soldier near own colony — expect MILITARY goto toward enemy. */
  const int sid = units_spawn(&units, 3, 5, 5);
  ColonizeUnit* soldier = units_get(&units, sid);
  if (!soldier) {
    fx_map_free(&map);
    return fail("spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->moves = 1 * UNITS_MP_PER_TILE;
  soldier->orders = 0;

  /* Europe-dock Caravel with free cargo — expect at-war Soldier hire/board. */
  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    fx_map_free(&map);
    return fail("spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves = 0; /* stay docked; hire path only needs Europe tile */

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 500;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 500;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    fx_map_free(&map);
    return fail("expected war after declare");
  }
  /* Replenish after war sting so hire_cost (200) is affordable. */
  col1.nation[nation].gold = 500;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  const uint32_t gold_before = col1.nation[nation].gold;

  ai_goals_reset();

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42; /* not seed-100 fixture */

  ai_euro_dispatcher_turn(&ctx, nation);

  const int mil_goto =
    soldier->active && soldier->orders == UNITS_ORDER_AI_MOVE &&
    soldier->goto_x == enemy->x && soldier->goto_y == enemy->y;

  int soldier_boarded = 0;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Soldier")) {
      soldier_boarded = 1;
      break;
    }
  }
  const int gold_spent = (col1.nation[nation].gold < gold_before);

  /* G stance: own≥2 + at war → MILITARY primary prio 6 (above E's 5). */
  const int mil_prio =
    ai_goals_max_primary_prio(nation, enemy->x, enemy->y, AI_GOAL_MILITARY);
  if (mil_prio < 6) {
    fprintf(stderr, "unit_ai_euro_war: G stance mil_prio=%d (want ≥6)\n", mil_prio);
    fx_map_free(&map);
    return fail("expected G stance MILITARY prio ≥6 with own≥2 at war");
  }
  if (mil_prio >= 7) {
    fprintf(stderr, "unit_ai_euro_war: G stance mil_prio=%d (own=2 should be 6 not 7)\n", mil_prio);
    fx_map_free(&map);
    return fail("own=2 must not take own≥3 deepen prio 7");
  }

  if (!mil_goto && !(soldier_boarded && gold_spent)) {
    fprintf(
      stderr,
      "unit_ai_euro_war: mil_goto=%d boarded=%d gold %u→%u orders=%d goto=(%d,%d)\n",
      mil_goto,
      soldier_boarded,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      soldier->orders,
      soldier->goto_x,
      soldier->goto_y
    );
    fx_map_free(&map);
    return fail("expected MILITARY AI_MOVE or Soldier hire/board");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_war: mid-hire ok (mil_goto=%d boarded=%d gold_spent=%d mil_prio=%d)\n",
    mil_goto,
    soldier_boarded,
    gold_spent,
    mil_prio
  );
  return 0;
}

/* Two nations at war, idle ocean ships — expect AI_SAIL toward foe / closer / combat. */
static int unit_naval_war_hunt(void) {
  const int nation = 1;
  const int foe = 2;
  const int own_x = 4;
  const int own_y = 4;
  const int foe_x = 10;
  const int foe_y = 10;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("naval alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 3;
  units.types[0].defense = 2;
  units.types[0].cargo = 0;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* warship = units_get(&units, own_id);
  if (!warship) {
    fx_map_free(&map);
    return fail("spawn own frigate");
  }
  warship->nation_id = nation;
  warship->orders = 0;
  warship->moves = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_ship = units_get(&units, foe_id);
  if (!foe_ship) {
    fx_map_free(&map);
    return fail("spawn foe frigate");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves = 0; /* stationary target */

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    fx_map_free(&map);
    return fail("naval expected war");
  }

  ai_goals_reset();

  uint32_t turn = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int dist0 = abs(own_x - foe_x) + abs(own_y - foe_y);
  ai_euro_dispatcher_turn(&ctx, nation);

  warship = units_get(&units, own_id);
  foe_ship = units_get(&units, foe_id);

  const int combat_done = (warship == NULL || !warship->active) || (foe_ship == NULL || !foe_ship->active);
  int sail_toward = 0;
  int moved_closer = 0;
  if (warship && warship->active) {
    sail_toward =
      warship->orders == UNITS_ORDER_AI_SAIL && warship->goto_x == foe_x &&
      warship->goto_y == foe_y;
    const int dist1 = foe_ship && foe_ship->active
                        ? abs(warship->x - foe_ship->x) + abs(warship->y - foe_ship->y)
                        : 0;
    moved_closer = dist1 < dist0 || (warship->x != own_x || warship->y != own_y);
  }

  if (!combat_done && !sail_toward && !moved_closer) {
    fprintf(
      stderr,
      "unit_ai_euro_war: naval orders=%d goto=(%d,%d) pos=(%d,%d) foe_active=%d\n",
      warship ? warship->orders : -1,
      warship ? warship->goto_x : -1,
      warship ? warship->goto_y : -1,
      warship ? warship->x : -1,
      warship ? warship->y : -1,
      foe_ship && foe_ship->active
    );
    fx_map_free(&map);
    return fail("expected AI_SAIL toward foe, closer move, or combat");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_war: naval ok (sail=%d closer=%d combat=%d)\n",
    sail_toward,
    moved_closer,
    combat_done
  );
  return 0;
}

/*
 * Ship under enemy Fort battery flees to safe water (Marathon8 AI wire).
 * Cite: FUN_364b_03f6; ai_euro_naval_try_flee_fort_fire.
 */
static int unit_naval_flee_fort_fire(void) {
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
    return fail("flee-fort alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  /* Land colony tile at (5,5); ship starts at (5,4) under battery. */
  map.terrain[5 + 5 * 16] = 1;

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 3;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Fort");
  colonies.building_type_count = 1;
  ColonizeColony* col = &colonies.colonies[0];
  col->id = 0;
  col->active = true;
  col->nation_id = foe;
  col->x = 5;
  col->y = 5;
  col->population = 3;
  col->has_building[0] = true;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, 5, 4);
  ColonizeUnit* ship = units_get(&units, own_id);
  if (!ship) {
    fx_map_free(&map);
    return fail("flee-fort spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves = 4 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    fx_map_free(&map);
    return fail("flee-fort expected war");
  }

  ai_goals_reset();
  uint32_t turn = 30;
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

  ship = units_get(&units, own_id);
  if (!ship || !ship->active) {
    fx_map_free(&map);
    return fail("flee-fort ship should survive");
  }
  /* Left the battery ring (not adjacent to Fort colony). */
  const int adj = abs(ship->x - 5) <= 1 && abs(ship->y - 5) <= 1 && !(ship->x == 5 && ship->y == 5);
  if (adj) {
    fprintf(stderr, "flee-fort still adjacent at %d,%d\n", ship->x, ship->y);
    fx_map_free(&map);
    return fail("expected ship to flee Fort battery adjacency");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: naval flee fort fire ok (%d,%d)\n", ship->x, ship->y);
  return 0;
}

/*
 * Privateer at war with a foe ship adjacent and a prior sail goto: the 20e6
 * wander scorer (raw 90210-90219 busy-unit entry, LAB_52aa odds term) picks
 * the foe tile and the act resolves the naval fight. DOS has no distant hunt.
 */
static int unit_privateer_war_hunt(void) {
  const int nation = 1;
  const int foe = 2;
  const int own_x = 4;
  const int own_y = 4;
  const int foe_x = 5;
  const int foe_y = 5;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("privateer alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 2;
  units.types[0].defense = 1;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* priv = units_get(&units, own_id);
  if (!priv) {
    fx_map_free(&map);
    return fail("spawn privateer");
  }
  priv->nation_id = nation;
  /* Prior west sail goto: a BUSY hull. DOS (raw 90210-90219) still sends it
   * through LAB_4d2e when FUN_281f_0984 finds a foreign unit adjacent, and
   * the LAB_52aa odds term makes the foe tile the pick. */
  priv->orders = UNITS_ORDER_AI_SAIL;
  priv->goto_x = 0;
  priv->goto_y = own_y;
  priv->moves = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_ship = units_get(&units, foe_id);
  if (!foe_ship) {
    fx_map_free(&map);
    return fail("spawn foe privateer");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 100;
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    fx_map_free(&map);
    return fail("privateer expected war");
  }

  ai_goals_reset();

  uint32_t turn = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int foe_hp0 = (int)(foe_ship->col1_flags15 & 0x80u);
  ai_euro_dispatcher_turn(&ctx, nation);

  priv = units_get(&units, own_id);
  foe_ship = units_get(&units, foe_id);

  const int combat_done =
    (priv == NULL || !priv->active) || (foe_ship == NULL || !foe_ship->active) ||
    (foe_ship && (int)(foe_ship->col1_flags15 & 0x80u) != foe_hp0) || (priv && (priv->col1_flags15 & 0x80u) != 0);
  /* A naval resolve spends the whole allotment (ai_euro_try_attack). */
  const int fought = combat_done || (priv && priv->moves == 0 && priv->x == own_x && priv->y == own_y);
  const int sailed_west = priv && priv->active && priv->x < own_x;
  if (!fought || sailed_west) {
    fprintf(
      stderr,
      "unit_ai_euro_war: privateer orders=%d goto=(%d,%d) pos=(%d,%d) mp=%d foe_active=%d\n",
      priv ? priv->orders : -1,
      priv ? priv->goto_x : -1,
      priv ? priv->goto_y : -1,
      priv ? priv->x : -1,
      priv ? priv->y : -1,
      priv ? priv->moves : -1,
      foe_ship && foe_ship->active
    );
    fx_map_free(&map);
    return fail("expected busy Privateer to fight the adjacent foe (LAB_52aa pick)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: privateer adjacent-foe attack ok (combat=%d)\n", combat_done);
  return 0;
}

/*
 * Post-diplo Privateer spawn station-keep: idle AI_SAIL with goto=self (as
 * euro_diplo wartime commission). goto=self is not a useful goto, so the
 * 20e6 ship wander (raw 90210 → LAB_4d2e) moves it off station.
 */
static int unit_privateer_station_keep_hunt(void) {
  const int nation = 1;
  const int foe = 2;
  const int own_x = 4;
  const int own_y = 4;
  const int foe_x = 10;
  const int foe_y = 10;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("priv-sk alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
    map.layer3[i] = 0xf0; /* owner nibble 0xf = unclaimed (calloc 0 = English) */
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 2;
  units.types[0].defense = 1;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* priv = units_get(&units, own_id);
  if (!priv) {
    fx_map_free(&map);
    return fail("priv-sk spawn");
  }
  priv->nation_id = nation;
  /* Diplo spawn station-keep (goto=self) — must still hunt. */
  priv->orders = UNITS_ORDER_AI_SAIL;
  priv->goto_x = own_x;
  priv->goto_y = own_y;
  priv->moves = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_ship = units_get(&units, foe_id);
  if (!foe_ship) {
    fx_map_free(&map);
    return fail("priv-sk foe spawn");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset();

  uint32_t turn = 31;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 43;

  ai_euro_dispatcher_turn(&ctx, nation);

  priv = units_get(&units, own_id);
  foe_ship = units_get(&units, foe_id);
  const int combat_done =
    (priv == NULL || !priv->active) || (foe_ship == NULL || !foe_ship->active);
  /* DOS has no distant hunt: the idle hull takes a LAB_4d2e wander step and
   * leaves its station; it must not sit on goto=self all turn. */
  const int moved = priv && priv->active && (priv->x != own_x || priv->y != own_y);
  if (!combat_done && !moved) {
    fprintf(
      stderr,
      "unit_ai_euro_war: priv-sk orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      priv ? priv->orders : -1,
      priv ? priv->goto_x : -1,
      priv ? priv->goto_y : -1,
      priv ? priv->x : -1,
      priv ? priv->y : -1
    );
    fx_map_free(&map);
    return fail("expected station-keep Privateer to take a wander step");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: priv-sk wander ok (moved=%d combat=%d)\n", moved, combat_done);
  return 0;
}


/*
 * Indian×Euro war: Soldier hunts toward capital tribe over nearer non-capital.
 * Cite: ai_diplo_indian_at_war; tribe.state.capital; Cortes rich_capital path.
 */
static int unit_indian_war_capital_hunt(void) {
  const int nation = 1;
  const int indian = 4; /* Arawak */

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("indian-hunt alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  memset(units.types, 0, sizeof(units.types));
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);

  const int sid = units_spawn(&units, 0, 2, 2);
  ColonizeUnit* soldier = units_get(&units, sid);
  if (!soldier) {
    fx_map_free(&map);
    return fail("indian-hunt spawn");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 3 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  col1.player[nation].control = 0;
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  /* At war with Indian slot 0 (nation 4). */
  col1.indian[0].alarm_by_player[nation] = 80; /* relation 20 */
  col1.indian[0].euro_diplo[nation] |= COL1_INDIAN_MET_BIT;
  ai_diplo_indian_hostility_sync(&col1, nation);

  ColonizeCol1Tribe tribes[2];
  memset(tribes, 0, sizeof(tribes));
  tribes[0].x = 4;
  tribes[0].y = 2;
  tribes[0].nation_id = (uint8_t)indian;
  tribes[0].state.capital = 0; /* nearer non-capital */
  tribes[1].x = 10;
  tribes[1].y = 2;
  tribes[1].nation_id = (uint8_t)indian;
  tribes[1].state.capital = 1; /* farther capital — prefer */
  col1.tribe = tribes;
  col1.head.tribe_count = 2;

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

  if (!ai_diplo_indian_any_at_war(&col1, nation)) {
    fx_map_free(&map);
    return fail("indian-hunt expected indian war");
  }

  ai_euro_dispatcher_turn(&ctx, nation);
  soldier = units_get(&units, sid);
  if (!soldier || !soldier->active) {
    fx_map_free(&map);
    return fail("indian-hunt soldier gone");
  }
  const int toward_cap =
    soldier->orders == UNITS_ORDER_AI_MOVE && soldier->goto_x == 10 && soldier->goto_y == 2;
  const int moved_east = soldier->x > 2;
  if (!toward_cap && !moved_east) {
    fprintf(
      stderr,
      "unit_ai_euro_war: indian-hunt orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      soldier->orders,
      soldier->goto_x,
      soldier->goto_y,
      soldier->x,
      soldier->y
    );
    fx_map_free(&map);
    return fail("expected AI_MOVE toward capital tribe (10,2) or east move");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_war: indian capital hunt ok (goto_cap=%d east=%d)\n",
    toward_cap,
    moved_east
  );
  return 0;
}




/*
 * Sticky CONTACT re-hunt: fortified Soldier (hunter adjacent-attack skipped)
 * with moves left next to a war foe — sticky still try_attacks.
 */
static int unit_sticky_contact_rehunt(void) {
  const int nation = 1;
  const int foe = 2;
  const int own_x = 5;
  const int own_y = 5;
  const int foe_x = 6;
  const int foe_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("sticky alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 3;
  own->y = 3;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("sticky spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = UNITS_ORDER_FORTIFIED; /* skip land_try_adjacent_attack */
  soldier->moves = 2 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    fx_map_free(&map);
    return fail("sticky spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves = 0;

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

  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7);

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
  ctx.rng = &rng;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  foe_u = units_get(&units, foe_id);
  const int combat_done =
    (soldier == NULL || !soldier->active) || (foe_u == NULL || !foe_u->active);

  if (!combat_done) {
    fprintf(
      stderr,
      "unit_ai_euro_war: sticky soldier_active=%d foe_active=%d moves=%d orders=%d\n",
      soldier && soldier->active,
      foe_u && foe_u->active,
      soldier ? soldier->moves : -1,
      soldier ? soldier->orders : -1
    );
    fx_map_free(&map);
    return fail("sticky CONTACT re-hunt should attempt combat vs adjacent war foe");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: sticky CONTACT re-hunt ok\n");
  return 0;
}

/*
 * Thin multi-step land adjacent combat: Soldier with MP>1 kills foe A then
 * continues onto adjacent foe B in the same act (drain moves). Cite:
 * euro_unit_act §2c multi-step combat; ai_euro_land_try_adjacent_attack chain.
 */
static int unit_land_adjacent_combat_chain(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("combat-chain alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 1;

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

  /* Soldier — foeA — foeB in a line (east). */
  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("combat-chain spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 3 * UNITS_MP_PER_TILE;

  const int foe_a = units_spawn(&units, 0, 6, 5);
  ColonizeUnit* fa = units_get(&units, foe_a);
  const int foe_b = units_spawn(&units, 0, 7, 5);
  ColonizeUnit* fb = units_get(&units, foe_b);
  if (!fa || !fb) {
    fx_map_free(&map);
    return fail("combat-chain spawn foes");
  }
  fa->nation_id = foe;
  fa->orders = 0;
  fa->moves = 0;
  fb->nation_id = foe;
  fb->orders = 0;
  fb->moves = 0;

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

  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL; /* deterministic attack>=defense wins */
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  fa = units_get(&units, foe_a);
  fb = units_get(&units, foe_b);
  const int a_dead = !fa || !fa->active;
  const int b_dead = !fb || !fb->active;
  /* bugs.md #243: a land attacker stays put after a win — foe A (adjacent)
   * dies; the AI may then STEP into the vacated tile as a normal move, but
   * the attack itself no longer carries it there, so foe B two tiles out
   * survives the act. */
  if (!a_dead || b_dead) {
    fprintf(
      stderr,
      "unit_ai_euro_war: chain soldier=%d,%d moves=%d a_dead=%d b_dead=%d\n",
      soldier ? soldier->x : -1,
      soldier ? soldier->y : -1,
      soldier ? soldier->moves : -1,
      a_dead,
      b_dead
    );
    fx_map_free(&map);
    return fail("adjacent foe dies, attacker stays put, far foe survives");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: land adjacent combat (stay-put) ok\n");
  return 0;
}

/*
 * FUN_521d_20e6 `0x46` gate: combat-capable land unit adjacent to an
 * *undefended* foreign Euro colony (no unit on the tile) walks in and
 * seizes it outright — no combat needed. Distinct from
 * unit_land_adjacent_combat_chain (defended foe) and from the goal-driven
 * MILITARY-goto capture path (this fires opportunistically regardless of
 * the unit's assigned goal).
 */
static int unit_land_adjacent_colony_seize(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("colony-seize alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 1;

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
  ColonizeColony* target = &colonies.colonies[1];
  target->id = 1;
  target->active = true;
  target->nation_id = foe;
  target->x = 6;
  target->y = 5;
  target->population = 1;
  target->colonist_count = 1;
  target->stock[0] = 30; /* plunder should be reported, not required to move it */
  colonies.colony_count = 2;

  /* Soldier adjacent to the foe colony tile — no defender there. */
  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("colony-seize spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
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

  uint32_t turn = 40;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  const ColonizeColony* seized = colonies_get(&colonies, 1);
  const int on_tile = soldier && soldier->active && soldier->x == 6 && soldier->y == 5;
  const int captured = seized && seized->active && seized->nation_id == nation;
  if (!on_tile || !captured) {
    fprintf(
      stderr,
      "unit_ai_euro_war: seize soldier=(%d,%d) active=%d colony_nation=%d\n",
      soldier ? soldier->x : -1,
      soldier ? soldier->y : -1,
      soldier ? soldier->active : -1,
      seized ? seized->nation_id : -1
    );
    fx_map_free(&map);
    return fail("land unit should walk into and seize an undefended adjacent foe colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: land adjacent undefended colony seize ok\n");
  return 0;
}

/*
 * Thin 20e6 land adjacent-foe pick: Soldier between fortified high-defense foe
 * (N) and weak Free Colonist (S). Prefer the weaker/non-fortified target.
 * Old first-dir scan would hit N first.
 */
static int unit_land_adjacent_foe_prefer_weak(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("adj-foe alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 8;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Free Colonist");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 0;
  units.types[1].defense = 1;

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

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("adj-foe spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 1 * UNITS_MP_PER_TILE; /* one adjacent fight only — no re-hunt onto fortified */

  /* Strong fortified foe to the north (first octant dir) — should NOT be preferred. */
  const int strong_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* strong = units_get(&units, strong_id);
  if (!strong) {
    fx_map_free(&map);
    return fail("adj-foe spawn strong");
  }
  strong->nation_id = foe_nat;
  strong->orders = UNITS_ORDER_FORTIFIED;
  strong->moves = 0;

  /* Weak colonist to the south — preferred target. */
  const int weak_id = units_spawn(&units, 1, own_x, own_y + 1);
  ColonizeUnit* weak = units_get(&units, weak_id);
  if (!weak) {
    fx_map_free(&map);
    return fail("adj-foe spawn weak");
  }
  weak->nation_id = foe_nat;
  weak->orders = 0;
  weak->moves = 0;

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
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  /* No RNG → deterministic: attack 8 >= defense 1 → attacker wins vs weak. */
  ai_goals_reset();

  uint32_t turn = 41;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  strong = units_get(&units, strong_id);
  weak = units_get(&units, weak_id);

  const int weak_dead =
    weak == NULL || !weak->active || (weak->nation_id == nation);
  const int strong_alive = strong && strong->active && strong->nation_id == foe_nat;
  const int own_alive = soldier && soldier->active;

  if (!weak_dead || !strong_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: adj-foe own=%d weak_dead=%d strong_alive=%d\n",
      own_alive,
      weak_dead,
      strong_alive
    );
    fx_map_free(&map);
    return fail("expected attack on weak colonist, fortified Soldier left alone");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe prefer-weak ok\n");
  return 0;
}

/*
 * Thin 20e6 land adjacent-foe: equal toughness Scout (N, first dir) vs Treasure
 * (S) → prefer Treasure loot. Cite: Colonization.pdf Treasure Trains / @LOOTCASH.
 */
static int unit_land_adjacent_foe_prefer_treasure(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("adj-treasure alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 8;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Scout");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 0;
  units.types[1].defense = 0;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Treasure");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 0;
  units.types[2].defense = 0;

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

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("adj-treasure spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 1 * UNITS_MP_PER_TILE;

  const int scout_id = units_spawn(&units, 1, own_x, own_y - 1); /* N first */
  ColonizeUnit* scout = units_get(&units, scout_id);
  if (!scout) {
    fx_map_free(&map);
    return fail("adj-treasure spawn scout");
  }
  scout->nation_id = foe_nat;
  scout->orders = 0;
  scout->moves = 0;

  const int treasure_id = units_spawn(&units, 2, own_x, own_y + 1); /* S */
  ColonizeUnit* treasure = units_get(&units, treasure_id);
  if (!treasure) {
    fx_map_free(&map);
    return fail("adj-treasure spawn treasure");
  }
  treasure->nation_id = foe_nat;
  treasure->orders = 0;
  treasure->moves = 0;

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
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();

  uint32_t turn = 41;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  scout = units_get(&units, scout_id);
  treasure = units_get(&units, treasure_id);

  const int treasure_dead = treasure == NULL || !treasure->active;
  const int scout_alive = scout && scout->active;
  const int own_alive = soldier && soldier->active;

  if (!treasure_dead || !scout_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: adj-treasure own=%d treasure_dead=%d scout_alive=%d\n",
      own_alive,
      treasure_dead,
      scout_alive
    );
    fx_map_free(&map);
    return fail("expected attack on Treasure over equal-toughness Scout");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe prefer Treasure ok\n");
  return 0;
}



/*
 * Thin 20e6 land adjacent-foe: same-type Soldiers — prefer open-field over
 * Stockade colony tile (+100% defense). Cite: colonies_fortification_defense_bonus_percent;
 * units_resolve_land_combat_ff Stockade replace fortified ×2.
 */
static int unit_land_adjacent_foe_prefer_open_over_stockade(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("adj-stockade alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 4;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_type_count = 1;
  ColonizeColony* foe_col = &colonies.colonies[0];
  foe_col->id = 0;
  foe_col->active = true;
  foe_col->nation_id = foe_nat;
  foe_col->x = own_x;
  foe_col->y = own_y - 1; /* N: Stockade colony */
  foe_col->population = 2;
  foe_col->colonist_count = 2;
  foe_col->has_building[0] = true;
  ColonizeColony* own = &colonies.colonies[1];
  own->id = 1;
  own->active = true;
  own->nation_id = nation;
  own->x = 1;
  own->y = 1;
  own->population = 1;
  own->colonist_count = 1;
  colonies.colony_count = 2;

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("adj-stockade spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 1 * UNITS_MP_PER_TILE;

  /* Stockade defender to the north (first dir) — tougher (def 4→8). */
  const int stock_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* stock = units_get(&units, stock_id);
  if (!stock) {
    fx_map_free(&map);
    return fail("adj-stockade spawn stockade foe");
  }
  stock->nation_id = foe_nat;
  stock->orders = 0;
  stock->moves = 0;

  /* Open-field same Soldier to the south — preferred (def 4). */
  const int open_id = units_spawn(&units, 0, own_x, own_y + 1);
  ColonizeUnit* open = units_get(&units, open_id);
  if (!open) {
    fx_map_free(&map);
    return fail("adj-stockade spawn open foe");
  }
  open->nation_id = foe_nat;
  open->orders = 0;
  open->moves = 0;

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
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();

  uint32_t turn = 42;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  stock = units_get(&units, stock_id);
  open = units_get(&units, open_id);

  const int open_dead = open == NULL || !open->active;
  const int stock_alive = stock && stock->active;
  const int own_alive = soldier && soldier->active;

  if (!open_dead || !stock_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: adj-stockade own=%d open_dead=%d stock_alive=%d\n",
      own_alive,
      open_dead,
      stock_alive
    );
    fx_map_free(&map);
    return fail("expected attack on open-field Soldier, Stockade left alone");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe prefer open over Stockade ok\n");
  return 0;
}

/*
 * FUN_157e_004a vet peel: same-type Soldiers — prefer non-veteran (profession
 * none) over Veteran (UNITS_JOB_SOLDIER → +50% toughness). Cite: FUN_157e_004a
 * type Soldier/Dragoon + profession 0x15.
 */
static int unit_land_adjacent_foe_prefer_non_veteran(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("adj-vet alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 8;

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

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("adj-vet spawn own");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 1 * UNITS_MP_PER_TILE;
  soldier->profession = UNITS_JOB_NONE;

  /* Veteran to the north (first octant) — tougher via +50%. */
  const int vet_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* vet = units_get(&units, vet_id);
  if (!vet) {
    fx_map_free(&map);
    return fail("adj-vet spawn veteran");
  }
  vet->nation_id = foe_nat;
  vet->orders = 0;
  vet->moves = 0;
  vet->profession = UNITS_JOB_SOLDIER;

  /* Plain Soldier to the south — preferred. */
  const int plain_id = units_spawn(&units, 0, own_x, own_y + 1);
  ColonizeUnit* plain = units_get(&units, plain_id);
  if (!plain) {
    fx_map_free(&map);
    return fail("adj-vet spawn plain");
  }
  plain->nation_id = foe_nat;
  plain->orders = 0;
  plain->moves = 0;
  plain->profession = UNITS_JOB_NONE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();

  uint32_t turn = 46;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 47;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  vet = units_get(&units, vet_id);
  plain = units_get(&units, plain_id);

  const int plain_dead = plain == NULL || !plain->active;
  const int vet_alive = vet && vet->active;
  const int own_alive = soldier && soldier->active;

  if (!plain_dead || !vet_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: adj-vet own=%d plain_dead=%d vet_alive=%d\n",
      own_alive,
      plain_dead,
      vet_alive
    );
    fx_map_free(&map);
    return fail("expected attack on non-veteran Soldier, veteran left alone");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe prefer non-veteran ok\n");
  return 0;
}

/*
 * Artillery adjacent-foe: prefer Stockade colony Soldier over open-field
 * (siege — opposite of non-Artillery prefer-open). Cite: king_ref Artillery
 * adjacent-fort; Colonization.pdf Artillery.
 */
static int unit_artillery_adjacent_prefer_stockade(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("art-adj alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Artillery");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 12;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 4;
  units.types[1].defense = 4;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_type_count = 1;
  ColonizeColony* foe_col = &colonies.colonies[0];
  foe_col->id = 0;
  foe_col->active = true;
  foe_col->nation_id = foe_nat;
  foe_col->x = own_x;
  foe_col->y = own_y + 1; /* S: Stockade — Artillery should prefer */
  foe_col->population = 2;
  foe_col->colonist_count = 2;
  foe_col->has_building[0] = true;
  ColonizeColony* own = &colonies.colonies[1];
  own->id = 1;
  own->active = true;
  own->nation_id = nation;
  own->x = 1;
  own->y = 1;
  own->population = 1;
  own->colonist_count = 1;
  colonies.colony_count = 2;

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* art = units_get(&units, own_id);
  if (!art) {
    fx_map_free(&map);
    return fail("art-adj spawn artillery");
  }
  art->nation_id = nation;
  art->orders = 0;
  art->moves = 1 * UNITS_MP_PER_TILE;

  const int open_id = units_spawn(&units, 1, own_x, own_y - 1); /* N open */
  ColonizeUnit* open = units_get(&units, open_id);
  if (!open) {
    fx_map_free(&map);
    return fail("art-adj spawn open");
  }
  open->nation_id = foe_nat;
  open->orders = 0;
  open->moves = 0;

  const int stock_id = units_spawn(&units, 1, own_x, own_y + 1); /* S stockade */
  ColonizeUnit* stock = units_get(&units, stock_id);
  if (!stock) {
    fx_map_free(&map);
    return fail("art-adj spawn stockade");
  }
  stock->nation_id = foe_nat;
  stock->orders = 0;
  stock->moves = 0;

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
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();
  uint32_t turn = 43;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;

  ai_euro_dispatcher_turn(&ctx, nation);

  art = units_get(&units, own_id);
  open = units_get(&units, open_id);
  stock = units_get(&units, stock_id);
  const int stock_dead = stock == NULL || !stock->active;
  const int open_alive = open && open->active;
  const int art_alive = art && art->active;

  if (!stock_dead || !open_alive || !art_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: art-adj art=%d stock_dead=%d open_alive=%d\n",
      art_alive,
      stock_dead,
      open_alive
    );
    fx_map_free(&map);
    return fail("Artillery should attack Stockade foe, leave open Soldier");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: Artillery adjacent prefer Stockade ok\n");
  return 0;
}

/*
 * Dragoon land hunt: prefer open colony over farther Stockade (MD slack ≤3).
 * Cite: king_ref Dragoon open bias; leave fortified ports to Artillery.
 */
static int unit_dragoon_hunt_prefer_open(void) {
  const int nation = 1;
  const int foe_nat = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 20, 20, 1, false)) {
    return fail("dragoon-hunt alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Dragoon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 6;
  units.types[0].defense = 4;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_type_count = 1;
  /* Stockade off the eastbound path (8,8); open at (10,5) — prefer open within slack. */
  ColonizeColony* stock_col = &colonies.colonies[0];
  stock_col->id = 0;
  stock_col->active = true;
  stock_col->nation_id = foe_nat;
  stock_col->x = 8;
  stock_col->y = 8;
  stock_col->population = 2;
  stock_col->colonist_count = 2;
  stock_col->has_building[0] = true;
  ColonizeColony* open_col = &colonies.colonies[1];
  open_col->id = 1;
  open_col->active = true;
  open_col->nation_id = foe_nat;
  open_col->x = 10;
  open_col->y = 5;
  open_col->population = 1;
  open_col->colonist_count = 1;
  colonies.colony_count = 2;

  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* drag = units_get(&units, own_id);
  if (!drag) {
    fx_map_free(&map);
    return fail("dragoon-hunt spawn");
  }
  drag->nation_id = nation;
  drag->orders = 0;
  drag->moves = 4 * UNITS_MP_PER_TILE;

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
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();
  uint32_t turn = 45;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;

  ai_euro_dispatcher_turn(&ctx, nation);

  drag = units_get(&units, own_id);
  if (!drag || !drag->active) {
    fx_map_free(&map);
    return fail("dragoon-hunt despawned");
  }
  if (drag->goto_x != 10 || drag->goto_y != 5) {
    fprintf(
      stderr,
      "unit_ai_euro_war: dragoon-hunt goto=(%d,%d) want open (10,5)\n",
      drag->goto_x,
      drag->goto_y
    );
    fx_map_free(&map);
    return fail("Dragoon hunt should prefer open colony over Stockade");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: Dragoon hunt prefer open ok\n");
  return 0;
}

/*
 * Peace fortify Soldier on colony wakes when foreign Euro land unit enters MD≤2.
 * Cite: euro_unit_act §2d3 peace colony-defense wake; units_wake.
 */
static int unit_peace_fortify_border_wake(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-border alloc map");
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
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("peace-border spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = UNITS_ORDER_FORTIFIED;
  soldier->moves = 3 * UNITS_MP_PER_TILE;

  /* Foreign Soldier at MD=2 — peace border threat. */
  const int foe_id = units_spawn(&units, 0, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    fx_map_free(&map);
    return fail("peace-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves = 0;

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
  /* Peace — no declare_war. */

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

  const int x0 = soldier->x;
  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  foe_u = units_get(&units, foe_id);
  if (!soldier || !soldier->active) {
    fx_map_free(&map);
    fprintf(stderr, "unit_ai_euro_war: peace-border wake ok (combat despawn)\n");
    return 0;
  }

  const int woken = soldier->orders != UNITS_ORDER_FORTIFIED &&
                    soldier->orders != UNITS_ORDER_FORTIFY;
  const int hunting =
    units_orders_follow_goto(soldier->orders) || soldier->x != x0 ||
    (foe_u && !foe_u->active);
  const int toward =
    (soldier->goto_x == 6 && soldier->goto_y == 4) || soldier->x > x0 ||
    (foe_u && !foe_u->active);
  if (!woken || !hunting || !toward) {
    fprintf(
      stderr,
      "unit_ai_euro_war: peace-border orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      soldier->orders,
      soldier->goto_x,
      soldier->goto_y,
      soldier->x,
      soldier->y
    );
    fx_map_free(&map);
    return fail("expected peace-fortified Soldier to wake for MD≤2 border threat");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace fortify border wake ok\n");
  return 0;
}

/*
 * Peace fortified Dragoon on colony wakes when foreign Euro land unit enters
 * MD≤2 (same as Soldier). Cite: Colonization.pdf Defending a Colony (fortify
 * soldiers, dragoons…); euro_unit_act §2d3; units_wake.
 */
static int unit_peace_dragoon_border_wake(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("dragoon-border alloc map");
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
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 4;
  own->y = 4;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* dragoon = units_get(&units, own_id);
  if (!dragoon) {
    fx_map_free(&map);
    return fail("dragoon-border spawn dragoon");
  }
  dragoon->nation_id = nation;
  dragoon->orders = UNITS_ORDER_FORTIFIED;
  dragoon->moves = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    fx_map_free(&map);
    return fail("dragoon-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves = 0;

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

  ai_goals_reset();

  uint32_t turn = 26;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int x0 = dragoon->x;
  ai_euro_dispatcher_turn(&ctx, nation);

  dragoon = units_get(&units, own_id);
  foe_u = units_get(&units, foe_id);
  if (!dragoon || !dragoon->active) {
    fx_map_free(&map);
    fprintf(stderr, "unit_ai_euro_war: dragoon-border wake ok (combat despawn)\n");
    return 0;
  }

  const int woken = dragoon->orders != UNITS_ORDER_FORTIFIED &&
                    dragoon->orders != UNITS_ORDER_FORTIFY;
  const int hunting =
    units_orders_follow_goto(dragoon->orders) || dragoon->x != x0 ||
    (foe_u && !foe_u->active);
  const int toward =
    (dragoon->goto_x == 6 && dragoon->goto_y == 4) || dragoon->x > x0 ||
    (foe_u && !foe_u->active);
  if (!woken || !hunting || !toward) {
    fprintf(
      stderr,
      "unit_ai_euro_war: dragoon-border orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      dragoon->orders,
      dragoon->goto_x,
      dragoon->goto_y,
      dragoon->x,
      dragoon->y
    );
    fx_map_free(&map);
    return fail("expected peace-fortified Dragoon to wake for MD≤2 border threat");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Dragoon border wake ok\n");
  return 0;
}

/*
 * Peace fortified Artillery on colony wakes when foreign Euro land unit enters
 * MD≤2 (same Soldier/Dragoon arm). Cite: Colonization.pdf Defending a Colony
 * (…or artillery); euro_unit_act §2d3; units_wake.
 */
static int unit_peace_artillery_border_wake(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("arty-border alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Artillery");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 3;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 4;
  units.types[1].defense = 2;

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
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* arty = units_get(&units, own_id);
  if (!arty) {
    fx_map_free(&map);
    return fail("arty-border spawn artillery");
  }
  arty->nation_id = nation;
  arty->orders = UNITS_ORDER_FORTIFIED;
  arty->moves = 1 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 1, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    fx_map_free(&map);
    return fail("arty-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves = 0;

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

  const int x0 = arty->x;
  ai_euro_dispatcher_turn(&ctx, nation);

  arty = units_get(&units, own_id);
  foe_u = units_get(&units, foe_id);
  if (!arty || !arty->active) {
    fx_map_free(&map);
    fprintf(stderr, "unit_ai_euro_war: peace Artillery border wake ok (combat despawn)\n");
    return 0;
  }

  const int woken = arty->orders != UNITS_ORDER_FORTIFIED && arty->orders != UNITS_ORDER_FORTIFY;
  const int hunting =
    units_orders_follow_goto(arty->orders) || arty->x != x0 || (foe_u && !foe_u->active);
  const int toward =
    (arty->goto_x == 6 && arty->goto_y == 4) || arty->x > x0 || (foe_u && !foe_u->active);
  if (!woken || !hunting || !toward) {
    fprintf(
      stderr,
      "unit_ai_euro_war: arty-border orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      arty->orders,
      arty->goto_x,
      arty->goto_y,
      arty->x,
      arty->y
    );
    fx_map_free(&map);
    return fail("expected peace-fortified Artillery to wake for MD≤2 border threat");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Artillery border wake ok\n");
  return 0;
}

/*
 * Peace fortified Regular on colony wakes when foreign Euro land unit enters
 * MD≤2. Cite: Colonization.pdf Defending a Colony; euro_unit_act §2d3.
 */
static int unit_peace_regular_border_wake(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("regular-border alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regular");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 3;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 4;
  units.types[1].defense = 2;

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
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* reg = units_get(&units, own_id);
  if (!reg) {
    fx_map_free(&map);
    return fail("regular-border spawn regular");
  }
  reg->nation_id = nation;
  reg->orders = UNITS_ORDER_FORTIFIED;
  reg->moves = 3 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 1, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    fx_map_free(&map);
    return fail("regular-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves = 0;

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

  const int x0 = reg->x;
  ai_euro_dispatcher_turn(&ctx, nation);

  reg = units_get(&units, own_id);
  foe_u = units_get(&units, foe_id);
  if (!reg || !reg->active) {
    fx_map_free(&map);
    fprintf(stderr, "unit_ai_euro_war: peace Regular border wake ok (combat despawn)\n");
    return 0;
  }

  const int woken = reg->orders != UNITS_ORDER_FORTIFIED && reg->orders != UNITS_ORDER_FORTIFY;
  const int hunting =
    units_orders_follow_goto(reg->orders) || reg->x != x0 || (foe_u && !foe_u->active);
  const int toward =
    (reg->goto_x == 6 && reg->goto_y == 4) || reg->x > x0 || (foe_u && !foe_u->active);
  if (!woken || !hunting || !toward) {
    fprintf(
      stderr,
      "unit_ai_euro_war: regular-border orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      reg->orders,
      reg->goto_x,
      reg->goto_y,
      reg->x,
      reg->y
    );
    fx_map_free(&map);
    return fail("expected peace-fortified Regular to wake for MD≤2 border threat");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Regular border wake ok\n");
  return 0;
}

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
static int unit_peace_continental_army_border_wake(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("carmy-border alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Army");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 3;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 4;
  units.types[1].defense = 2;

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
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* army = units_get(&units, own_id);
  if (!army) {
    fx_map_free(&map);
    return fail("carmy-border spawn regular");
  }
  army->nation_id = nation;
  army->orders = UNITS_ORDER_FORTIFIED;
  army->moves = 3 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 1, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    fx_map_free(&map);
    return fail("carmy-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves = 0;

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

  const int x0 = army->x;
  ai_euro_dispatcher_turn(&ctx, nation);

  army = units_get(&units, own_id);
  foe_u = units_get(&units, foe_id);
  if (!army || !army->active) {
    fx_map_free(&map);
    fprintf(stderr, "unit_ai_euro_war: peace Continental Army border wake ok (combat despawn)\n");
    return 0;
  }

  const int woken = army->orders != UNITS_ORDER_FORTIFIED && army->orders != UNITS_ORDER_FORTIFY;
  const int hunting =
    units_orders_follow_goto(army->orders) || army->x != x0 || (foe_u && !foe_u->active);
  const int toward =
    (army->goto_x == 6 && army->goto_y == 4) || army->x > x0 || (foe_u && !foe_u->active);
  if (!woken || !hunting || !toward) {
    fprintf(
      stderr,
      "unit_ai_euro_war: carmy-border orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      army->orders,
      army->goto_x,
      army->goto_y,
      army->x,
      army->y
    );
    fx_map_free(&map);
    return fail("expected peace-fortified Continental Army to wake for MD≤2 border threat");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Continental Army border wake ok\n");
  return 0;
}

/*
 * 5d04 treasury: at war, prefer Artillery but gold < Europe purchase 500$ →
 * fall back to Soldier hire (hire_cost), not unpaid Artillery fiction.
 */

/*
 * Peace fortified Continental Cavalry on colony wakes when foreign Euro land unit enters
 * MD≤2. Cite: Colonization.pdf Defending a Colony; euro_unit_act §2d3.
 */
static int unit_peace_continental_cavalry_border_wake(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("ccav-border alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Cavalry");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 3;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 4;
  units.types[1].defense = 2;

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
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* cav = units_get(&units, own_id);
  if (!cav) {
    fx_map_free(&map);
    return fail("ccav-border spawn continental cavalry");
  }
  cav->nation_id = nation;
  cav->orders = UNITS_ORDER_FORTIFIED;
  cav->moves = 3 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 1, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    fx_map_free(&map);
    return fail("ccav-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves = 0;

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

  const int x0 = cav->x;
  ai_euro_dispatcher_turn(&ctx, nation);

  cav = units_get(&units, own_id);
  foe_u = units_get(&units, foe_id);
  if (!cav || !cav->active) {
    fx_map_free(&map);
    fprintf(stderr, "unit_ai_euro_war: peace Continental Cavalry border wake ok (combat despawn)\n");
    return 0;
  }

  const int woken = cav->orders != UNITS_ORDER_FORTIFIED && cav->orders != UNITS_ORDER_FORTIFY;
  const int hunting =
    units_orders_follow_goto(cav->orders) || cav->x != x0 || (foe_u && !foe_u->active);
  const int toward =
    (cav->goto_x == 6 && cav->goto_y == 4) || cav->x > x0 || (foe_u && !foe_u->active);
  if (!woken || !hunting || !toward) {
    fprintf(
      stderr,
      "unit_ai_euro_war: ccav-border orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      cav->orders,
      cav->goto_x,
      cav->goto_y,
      cav->x,
      cav->y
    );
    fx_map_free(&map);
    return fail("expected peace-fortified Continental Cavalry to wake for MD≤2 border threat");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Continental Cavalry border wake ok\n");
  return 0;
}

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
 * G stance deepen: own≥3 colonies + at war → MILITARY primary prio 7
 * (stand-in for −0x6790; no invented gold). Cite: euro_dispatcher G / decomp.
 */
static int unit_g_stance_own3_prio7(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("g3 alloc map");
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
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* own = &colonies.colonies[i];
    own->id = i;
    own->active = true;
    own->nation_id = nation;
    own->x = 3 + i * 2;
    own->y = 4;
    own->population = 2;
    own->colonist_count = 2;
    own->stock[COLONIZE_CARGO_FOOD] = 20;
    own->building_in_production = -1;
  }
  ColonizeColony* enemy = &colonies.colonies[3];
  enemy->id = 3;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 12;
  enemy->y = 10;
  enemy->population = 2;
  enemy->colonist_count = 2;
  enemy->stock[COLONIZE_CARGO_FOOD] = 20;
  enemy->building_in_production = -1;
  colonies.colony_count = 4;
  colonies.next_id = 4;

  const int sid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* soldier = units_get(&units, sid);
  if (!soldier) {
    fx_map_free(&map);
    return fail("g3 spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->moves = 1 * UNITS_MP_PER_TILE;
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
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 100;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  const int mil_prio =
    ai_goals_max_primary_prio(nation, enemy->x, enemy->y, AI_GOAL_MILITARY);
  if (mil_prio < 7) {
    fprintf(stderr, "unit_ai_euro_war: G own≥3 mil_prio=%d (want ≥7)\n", mil_prio);
    fx_map_free(&map);
    return fail("expected G stance MILITARY prio ≥7 with own≥3 at war");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: G own≥3 prio7 ok (mil_prio=%d)\n", mil_prio);
  return 0;
}

/*
 * G stance deepen: own≥4 colonies + at war → MILITARY primary prio 8
 * (stand-in for −0x6790; no invented gold). Cite: euro_dispatcher G / decomp.
 */
static int unit_g_stance_own4_prio8(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("g4 alloc map");
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
  for (int i = 0; i < 4; ++i) {
    ColonizeColony* own = &colonies.colonies[i];
    own->id = i;
    own->active = true;
    own->nation_id = nation;
    own->x = 3 + i * 2;
    own->y = 4;
    own->population = 2;
    own->colonist_count = 2;
    own->stock[COLONIZE_CARGO_FOOD] = 20;
    own->building_in_production = -1;
  }
  ColonizeColony* enemy = &colonies.colonies[4];
  enemy->id = 4;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 12;
  enemy->y = 10;
  enemy->population = 2;
  enemy->colonist_count = 2;
  enemy->stock[COLONIZE_CARGO_FOOD] = 20;
  enemy->building_in_production = -1;
  colonies.colony_count = 5;
  colonies.next_id = 5;

  const int sid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* soldier = units_get(&units, sid);
  if (!soldier) {
    fx_map_free(&map);
    return fail("g4 spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->moves = 1 * UNITS_MP_PER_TILE;
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
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset();

  uint32_t turn = 24;
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

  const int mil_prio =
    ai_goals_max_primary_prio(nation, enemy->x, enemy->y, AI_GOAL_MILITARY);
  if (mil_prio < 8) {
    fprintf(stderr, "unit_ai_euro_war: G own≥4 mil_prio=%d (want ≥8)\n", mil_prio);
    fx_map_free(&map);
    return fail("expected G stance MILITARY prio ≥8 with own≥4 at war");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: G own≥4 prio8 ok (mil_prio=%d)\n", mil_prio);
  return 0;
}

/*
 * Naval multi-step: Frigate AI_SAIL war hunt with moves≥2 advances two
 * scored ocean steps in one act (mirror land 2-step) and spends remaining MP.
 * Cite: euro_unit_act §2c4 / §2b Frigate war hunt.
 */
static int unit_naval_multistep_sail(void) {
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
    return fail("naval-ms alloc map");
  }
  /*
   * All-ocean geometry left every one of the 8 wander directions equally
   * legal at spawn, so the very first internal step (before any goto is
   * committed) was a dead tie the AI could only break via the momentum
   * bias in s_euro_last_dir[] (unit+0x314f) — a file-local latch this
   * binary never resets between tests, so the assertion only held because
   * an *earlier* test happened to leave that slot biased eastward. DOS
   * itself has no distant naval hunt (ai_euro_act_ship_war_trade only
   * fights an adjacent foe; the far pursuit ran through the plain 20e6
   * wander scorer), so a truly fresh ship's very first move is a genuine
   * coin flip in DOS too — asserting it always heads toward a foe 12
   * tiles away was never a load-bearing DOS fact, just an artifact of
   * cross-test global-state bleed in this binary.
   * Fix: wall the row so east is the *only* legal wander step (land on
   * every other one of the 8 neighbor tiles from spawn) — the test is now
   * deterministic and self-contained, independent of last_dir/RNG state
   * carried over from whichever test happened to run before it.
   */
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }
  for (int x = 0; x < 16; ++x) {
    map.terrain[x + 8 * 16] = 25; /* ocean corridor along y=8 */
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 3;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, 2, 8);
  ColonizeUnit* warship = units_get(&units, own_id);
  if (!warship) {
    fx_map_free(&map);
    return fail("naval-ms spawn");
  }
  warship->nation_id = nation;
  warship->orders = 0;
  warship->moves = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, 14, 8);
  ColonizeUnit* foe_ship = units_get(&units, foe_id);
  if (!foe_ship) {
    fx_map_free(&map);
    return fail("naval-ms foe");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset();

  uint32_t turn = 31;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int x0 = warship->x;
  const int mp0 = warship->moves;
  ai_euro_dispatcher_turn(&ctx, nation);
  warship = units_get(&units, own_id);
  if (!warship || !warship->active) {
    fx_map_free(&map);
    /* Combat ate the ship — still proves hunt ran; accept. */
    fprintf(stderr, "unit_ai_euro_war: naval multi-step ok (combat)\n");
    return 0;
  }
  const int advanced = warship->x - x0;
  const int spent = mp0 - warship->moves;
  const int hunting =
    units_orders_follow_goto(warship->orders) && warship->goto_x >= 0 &&
    (warship->goto_x > x0 || warship->goto_x == foe_ship->x);
  if (advanced < 2 || spent < 2 || !hunting) {
    fprintf(
      stderr,
      "unit_ai_euro_war: naval multi-step x %d→%d mp %d→%d orders=%d goto=(%d,%d)\n",
      x0,
      warship->x,
      mp0,
      warship->moves,
      warship->orders,
      warship->goto_x,
      warship->goto_y
    );
    fx_map_free(&map);
    return fail("expected Frigate war-hunt AI_SAIL multi-step (≥2 tiles, spend MP)");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_war: Frigate war-hunt multi-step ok (x %d→%d mp spent %d)\n",
    x0,
    warship->x,
    spent
  );
  return 0;
}

/*
 * At war + own colonies ≥ 3 + Dragoon type present → prefer Dragoon hire
 * over Soldier. Cite: euro_dispatcher mid-hire deepen; fandom Dragoon.
 */

/*
 * Series L negative: preset sticky=2 with unmet Indian relations → euro_balance
 * hostility_sync clears sticky (stance mil nibble / unload gate closes). Live
 * very-low relation (sticky positive smoke) is required for peacetime unload.
 * Cite: ai_diplo_indian_hostility_sync; Series L.
 */
static int unit_unload_stance0_no_sticky(void) {
  const int nation = 1;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].indian_hostility_sticky = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, 1, false)) {
    return fail("zunload alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);

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
  ctx.rng_seed = 9;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (ai_diplo_indian_hostility_sticky(&col1, nation) != 0) {
    fx_map_free(&map);
    return fail("unmet relations should clear sticky (stance0 / mil path closed)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: stance0 sticky-clear skip mil path ok\n");
  return 0;
}

/*
 * Threatened-port unload: Regular-only cargo → unload Regular. Cite: king_ref
 * MoW Regular-prefer; euro_unit_act §2b2.
 */
static int unit_unload_regular_threatened(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("runload alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("runload colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regular");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;
  units.types[1].attack = 2;
  units.types[1].defense = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Free Colonist");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 0;
  units.types[2].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* Full colony so unload stays as field unit (no admit after mil unload). */
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 8);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* reg = units_get(&units, uid);
  if (!reg) {
    fx_map_free(&map);
    return fail("runload spawn regular");
  }
  reg->nation_id = nation;
  reg->orders = 0;
  reg->moves = 0;
  reg->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("runload spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves = 4 * UNITS_MP_PER_TILE;
  ship->cargo_count = 0;
  if (!units_board(&units, uid, sid)) {
    fx_map_free(&map);
    return fail("runload board regular");
  }

  const int tid = units_spawn(&units, 2, 5, 4);
  ColonizeUnit* threat = units_get(&units, tid);
  if (!threat) {
    fx_map_free(&map);
    return fail("runload spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves = 0;

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

  uint32_t turn = 28;
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
  ship = units_get(&units, sid);
  if (!reg || !reg->active || reg->aboard_ship_id >= 0) {
    fprintf(
      stderr,
      "unit_ai_euro_war: runload aboard=%d active=%d cargo=%d pos=(%d,%d)\n",
      reg ? reg->aboard_ship_id : -99,
      reg ? reg->active : 0,
      ship ? ship->cargo_count : -1,
      reg ? reg->x : -1,
      reg ? reg->y : -1
    );
    fx_map_free(&map);
    return fail("expected Regular unloaded at threatened coastal colony");
  }
  if (abs(reg->x - 4) > 1 || abs(reg->y - 4) > 1) {
    fx_map_free(&map);
    return fail("expected Regular unloaded near threatened colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: Regular unload threatened colony ok\n");
  return 0;
}

/*
 * Peace: idle Soldier on own colony → FORTIFY. Cite: euro_unit_act §2d3.
 */

/*
 * Threatened-port unload: Continental Cavalry-only cargo → unload Continental Cavalry. Cite: king_ref
 * MoW Continental Cavalry-prefer; euro_unit_act §2b2.
 */

/*
 * Threatened-port unload: Continental Army-only cargo → unload Continental Army. Cite: king_ref
 * MoW Continental Army-prefer; euro_unit_act §2b2.
 */
static int unit_unload_continental_army_threatened(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("carmyunload alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("carmyunload colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Army");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;
  units.types[1].attack = 2;
  units.types[1].defense = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Free Colonist");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 0;
  units.types[2].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* Full colony so unload stays as field unit (no admit after mil unload). */
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 8);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* army = units_get(&units, uid);
  if (!army) {
    fx_map_free(&map);
    return fail("carmyunload spawn regular");
  }
  army->nation_id = nation;
  army->orders = 0;
  army->moves = 0;
  army->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("carmyunload spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves = 4 * UNITS_MP_PER_TILE;
  ship->cargo_count = 0;
  if (!units_board(&units, uid, sid)) {
    fx_map_free(&map);
    return fail("carmyunload board regular");
  }

  const int tid = units_spawn(&units, 2, 5, 4);
  ColonizeUnit* threat = units_get(&units, tid);
  if (!threat) {
    fx_map_free(&map);
    return fail("carmyunload spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves = 0;

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

  uint32_t turn = 28;
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
  ship = units_get(&units, sid);
  if (!army || !army->active || army->aboard_ship_id >= 0) {
    fprintf(
      stderr,
      "unit_ai_euro_war: carmyunload aboard=%d active=%d cargo=%d pos=(%d,%d)\n",
      army ? army->aboard_ship_id : -99,
      army ? army->active : 0,
      ship ? ship->cargo_count : -1,
      army ? army->x : -1,
      army ? army->y : -1
    );
    fx_map_free(&map);
    return fail("expected Continental Army unloaded at threatened coastal colony");
  }
  if (abs(army->x - 4) > 1 || abs(army->y - 4) > 1) {
    fx_map_free(&map);
    return fail("expected Continental Army unloaded near threatened colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: Continental Army unload threatened colony ok\n");
  return 0;
}

/*
 * Peace: idle Soldier on own colony → FORTIFY. Cite: euro_unit_act §2d3.
 */

/*
 * Threatened-port unload: Continental Cavalry-only cargo → unload Continental Cavalry. Cite: king_ref
 * MoW Continental Cavalry-prefer; euro_unit_act §2b2.
 */
static int unit_unload_continental_cavalry_threatened(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("ccavunload alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("ccavunload colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Cavalry");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;
  units.types[1].attack = 2;
  units.types[1].defense = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Free Colonist");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 0;
  units.types[2].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* Full colony so unload stays as field unit (no admit after mil unload). */
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 8);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* cav = units_get(&units, uid);
  if (!cav) {
    fx_map_free(&map);
    return fail("ccavunload spawn regular");
  }
  cav->nation_id = nation;
  cav->orders = 0;
  cav->moves = 0;
  cav->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("ccavunload spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves = 4 * UNITS_MP_PER_TILE;
  ship->cargo_count = 0;
  if (!units_board(&units, uid, sid)) {
    fx_map_free(&map);
    return fail("ccavunload board regular");
  }

  const int tid = units_spawn(&units, 2, 5, 4);
  ColonizeUnit* threat = units_get(&units, tid);
  if (!threat) {
    fx_map_free(&map);
    return fail("ccavunload spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves = 0;

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

  uint32_t turn = 28;
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
  ship = units_get(&units, sid);
  if (!cav || !cav->active || cav->aboard_ship_id >= 0) {
    fprintf(
      stderr,
      "unit_ai_euro_war: ccavunload aboard=%d active=%d cargo=%d pos=(%d,%d)\n",
      cav ? cav->aboard_ship_id : -99,
      cav ? cav->active : 0,
      ship ? ship->cargo_count : -1,
      cav ? cav->x : -1,
      cav ? cav->y : -1
    );
    fx_map_free(&map);
    return fail("expected Continental Cavalry unloaded at threatened coastal colony");
  }
  if (abs(cav->x - 4) > 1 || abs(cav->y - 4) > 1) {
    fx_map_free(&map);
    return fail("expected Continental Cavalry unloaded near threatened colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: Continental Cavalry unload threatened colony ok\n");
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
  /* Both fortify (quota does not gate the 'F' arm); quota spent once 1 -> 0. */
  if (fortified != 2 || c->garrison_quota != 0 || idle != 0 || joined != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_war: garrison_quota fortified=%d idle=%d joined=%d quota=%u\n",
      fortified,
      idle,
      joined,
      (unsigned)c->garrison_quota
    );
    fx_map_free(&map);
    return fail("expected both Soldiers fortified and garrison_quota 1→0");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: garrison_quota fortify ok\n");
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
 * golden_woi_ref01). Drive ai_euro_act_land_goal_dispatch directly with
 * is_land_hunter = 0 so the golden-backed adjacent-attack stand-in in the
 * same stage cannot run: only the drain loop's own step can produce the
 * attack.
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
  act.at_war_land = 1;
  act.is_land_hunter = 0; /* keep the stand-in out of this stage */
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
 * Peace Artillery fortify on own colony. Cite: Defending a Colony ("…or
 * artillery"); euro_unit_act §2d3.
 */
static int unit_peace_artillery_fortify_colony(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-art-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Artillery");
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
    return fail("peace-art-fortify spawn");
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
      return fail("peace-art-fortify foe spawn");
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
    return fail("peace-fortify artillery should remain");
  }
  if (art->orders != UNITS_ORDER_FORTIFY && art->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(
      stderr,
      "unit_ai_euro_war: peace-art-fortify orders=%d pos=(%d,%d)\n",
      art->orders,
      art->x,
      art->y
    );
    fx_map_free(&map);
    return fail("expected Artillery FORTIFY on own colony in peace");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: peace Artillery fortify colony ok\n");
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
 * Artillery fortify after siege: idle Artillery on own colony at war → FORTIFY.
 * Cite: euro_unit_act §2d3; Colonization.pdf fortify defense.
 */
static int unit_artillery_fortify_colony(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("art-fortify alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Artillery");
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
    return fail("art-fortify spawn");
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
      return fail("art-fortify foe spawn");
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
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    fx_map_free(&map);
    return fail("art-fortify expected war");
  }

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

  art = units_get(&units, uid);
  if (!art || !art->active) {
    fx_map_free(&map);
    return fail("art-fortify artillery should remain");
  }
  if (art->orders != UNITS_ORDER_FORTIFY && art->orders != UNITS_ORDER_FORTIFIED) {
    fprintf(
      stderr,
      "unit_ai_euro_war: art-fortify orders=%d pos=(%d,%d)\n",
      art->orders,
      art->x,
      art->y
    );
    fx_map_free(&map);
    return fail("expected idle Artillery on colony to FORTIFY at war");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: artillery fortify colony ok\n");
  return 0;
}

/*
 * War transport: idle Galleon with passenger space prefers threatened own
 * coastal colony water over distant foe sea. Cite: euro_unit_act §2b2.
 */
static int unit_war_transport_threatened_colony(void) {
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
    return fail("wtrans alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  /* Land colony at (4,4); coastal via water neighbours. */
  map.terrain[4 * 16 + 4] = 1;
  map.terrain[4 * 16 + 5] = 1;
  map.terrain[5 * 16 + 4] = 1;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("wtrans colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Galleon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 2;
  units.types[0].defense = 2;
  units.types[0].cargo = 6;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 2;
  units.types[1].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 2);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  /* Own Galleon far south — closer to threatened colony than to distant foe. */
  const int own_id = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* galleon = units_get(&units, own_id);
  if (!galleon) {
    fx_map_free(&map);
    return fail("wtrans spawn galleon");
  }
  galleon->nation_id = nation;
  galleon->orders = 0;
  galleon->moves = 4 * UNITS_MP_PER_TILE;
  galleon->cargo_count = 0;

  /* Foe soldier adjacent to own colony (threat MD≤3). */
  const int threat_id = units_spawn(&units, 1, 5, 4);
  ColonizeUnit* threat = units_get(&units, threat_id);
  if (!threat) {
    fx_map_free(&map);
    return fail("wtrans spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves = 0;

  /* Distant foe ship — must not win over threatened port. */
  const int foe_ship_id = units_spawn(&units, 0, 14, 14);
  ColonizeUnit* foe_ship = units_get(&units, foe_ship_id);
  if (!foe_ship) {
    fx_map_free(&map);
    return fail("wtrans spawn foe ship");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves = 0;

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
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    fx_map_free(&map);
    return fail("wtrans expected war");
  }

  ai_goals_reset();

  uint32_t turn = 32;
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

  galleon = units_get(&units, own_id);
  if (!galleon || !galleon->active) {
    fx_map_free(&map);
    return fail("wtrans galleon should remain");
  }

  /* Expect AI_SAIL toward coastal water near (4,4), not distant foe (14,14). */
  int near_colony = 0;
  if (galleon->orders == UNITS_ORDER_AI_SAIL) {
    const int gd = abs(galleon->goto_x - 4) + abs(galleon->goto_y - 4);
    const int fd = abs(galleon->goto_x - 14) + abs(galleon->goto_y - 14);
    near_colony = gd <= 2 && gd < fd;
  }
  const int moved_closer =
    abs(galleon->x - 4) + abs(galleon->y - 4) < abs(3 - 4) + abs(10 - 4);

  if (!near_colony && !moved_closer) {
    fprintf(
      stderr,
      "unit_ai_euro_war: wtrans orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      galleon->orders,
      galleon->goto_x,
      galleon->goto_y,
      galleon->x,
      galleon->y
    );
    fx_map_free(&map);
    return fail("expected Galleon sail toward threatened own coastal colony");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_war: war transport threatened colony ok (near=%d closer=%d)\n",
    near_colony,
    moved_closer
  );
  return 0;
}

/*
 * Retired with smell audit sweep-3 area C #6: `unit_war_cargo_fortress_prefer`
 * asserted the thin LAB_521d_3558 scorer's invented Stockade/Fort/Fortress
 * ladder (ai_euro_ocean_colony_sail_score, deleted with its war-cargo arm).
 * DOS has no building-tier ladder there — its 3558 ranks own colonies by the
 * raw-89663 pop/wanted/flag ladder, reached once per act from the 20e6
 * unload/settle flow (ai_euro_20e6_colony_sail_pick), and a goods-only hull
 * like this fixture's is the delivery matrix's business (raw 2047-2139).
 */

/*
 * Idle Man-O-War at war with a spotted foe hull: FUN_521d_0a60's
 * foreign-ship producer (raw 87577-87586) upserts CONTACT prio 3 on the foe
 * ship's tile, and the goal tail (raw 88164-88230) binds the MoW to it
 * (DS:0x523d bit0 = CONTACT is a warship capability). Nothing in DOS sends
 * it to a threatened own colony instead — the old premise of this test
 * ("prefer threatened coastal colony", cited to a non-decomp note) went with
 * bugs.md #527, when 0a60 started binding ships.
 */
static int unit_mow_war_transport_threatened(void) {
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
    return fail("mowtrans alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  /* Land colony at (4,4); coastal via water neighbours. */
  map.terrain[4 * 16 + 4] = 1;
  map.terrain[4 * 16 + 5] = 1;
  map.terrain[5 * 16 + 4] = 1;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("mowtrans colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Man-O-War");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 2;
  units.types[0].defense = 2;
  units.types[0].cargo = 6;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 2;
  units.types[1].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 2);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  /* Own Man-O-War far south — closer to threatened colony than to distant foe. */
  const int own_id = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* mow = units_get(&units, own_id);
  if (!mow) {
    fx_map_free(&map);
    return fail("mowtrans spawn mow");
  }
  mow->nation_id = nation;
  mow->orders = 0;
  mow->moves = 4 * UNITS_MP_PER_TILE;
  mow->cargo_count = 0;

  /* Foe soldier adjacent to own colony (threat MD≤3). */
  const int threat_id = units_spawn(&units, 1, 5, 4);
  ColonizeUnit* threat = units_get(&units, threat_id);
  if (!threat) {
    fx_map_free(&map);
    return fail("mowtrans spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves = 0;

  /* Distant foe ship — must not win over threatened port. */
  const int foe_ship_id = units_spawn(&units, 0, 14, 14);
  ColonizeUnit* foe_ship = units_get(&units, foe_ship_id);
  if (!foe_ship) {
    fx_map_free(&map);
    return fail("mowtrans spawn foe ship");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves = 0;

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
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    fx_map_free(&map);
    return fail("mowtrans expected war");
  }

  ai_goals_reset();

  uint32_t turn = 32;
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

  mow = units_get(&units, own_id);
  if (!mow || !mow->active) {
    fx_map_free(&map);
    return fail("mowtrans mow should remain");
  }

  /* Bound to the foe ship's CONTACT goal (act 0x0b, +0x314d/e = its tile)
   * and walking it: strictly closer to (14,14) than the spawn (3,10). */
  const int bound = mow->orders == UNITS_ORDER_AI_SAIL && mow->goto_x == 14 && mow->goto_y == 14;
  const int walked = abs(mow->x - 14) + abs(mow->y - 14) < abs(3 - 14) + abs(10 - 14);
  if (!bound || !walked) {
    fprintf(
      stderr,
      "unit_ai_euro_war: mowtrans orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      mow->orders,
      mow->goto_x,
      mow->goto_y,
      mow->x,
      mow->y
    );
    fx_map_free(&map);
    return fail("expected Man-O-War bound to the spotted foe ship's CONTACT goal");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_war: Man-O-War bound to foe-ship CONTACT goal ok (pos=%d,%d)\n",
    mow->x,
    mow->y
  );
  return 0;
}

/*
 * War transport: idle Frigate with passenger space prefers threatened own
 * coastal colony water over distant foe sea. Cite: euro_unit_act §2b2.
 */
static int unit_frigate_war_transport_threatened(void) {
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
    return fail("frigtrans alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  /* Land colony at (4,4); coastal via water neighbours. */
  map.terrain[4 * 16 + 4] = 1;
  map.terrain[4 * 16 + 5] = 1;
  map.terrain[5 * 16 + 4] = 1;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("frigtrans colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 2;
  units.types[0].defense = 2;
  units.types[0].cargo = 6;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 2;
  units.types[1].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 2);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;

  /* Own Frigate far south — closer to threatened colony than to distant foe. */
  const int own_id = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* frig = units_get(&units, own_id);
  if (!frig) {
    fx_map_free(&map);
    return fail("frigtrans spawn frig");
  }
  frig->nation_id = nation;
  frig->orders = 0;
  frig->moves = 4 * UNITS_MP_PER_TILE;
  frig->cargo_count = 0;

  /* Foe soldier adjacent to own colony (threat MD≤3). */
  const int threat_id = units_spawn(&units, 1, 5, 4);
  ColonizeUnit* threat = units_get(&units, threat_id);
  if (!threat) {
    fx_map_free(&map);
    return fail("frigtrans spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves = 0;

  /* Distant foe ship — must not win over threatened port. */
  const int foe_ship_id = units_spawn(&units, 0, 14, 14);
  ColonizeUnit* foe_ship = units_get(&units, foe_ship_id);
  if (!foe_ship) {
    fx_map_free(&map);
    return fail("frigtrans spawn foe ship");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves = 0;

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
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    fx_map_free(&map);
    return fail("frigtrans expected war");
  }

  ai_goals_reset();

  uint32_t turn = 32;
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

  frig = units_get(&units, own_id);
  if (!frig || !frig->active) {
    fx_map_free(&map);
    return fail("frigtrans frig should remain");
  }

  /* Expect AI_SAIL toward coastal water near (4,4), not distant foe (14,14). */
  int near_colony = 0;
  if (frig->orders == UNITS_ORDER_AI_SAIL) {
    const int gd = abs(frig->goto_x - 4) + abs(frig->goto_y - 4);
    const int fd = abs(frig->goto_x - 14) + abs(frig->goto_y - 14);
    near_colony = gd <= 2 && gd < fd;
  }
  const int moved_closer =
    abs(frig->x - 4) + abs(frig->y - 4) < abs(3 - 4) + abs(10 - 4);

  if (!near_colony && !moved_closer) {
    fprintf(
      stderr,
      "unit_ai_euro_war: frigtrans orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      frig->orders,
      frig->goto_x,
      frig->goto_y,
      frig->x,
      frig->y
    );
    fx_map_free(&map);
    return fail("expected Frigate sail toward threatened own coastal colony");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_war: Frigate war transport threatened ok (near=%d closer=%d)\n",
    near_colony,
    moved_closer
  );
  return 0;
}

/*
 * FUN_5bfb_3180 ship-slow (units_ship_slow_scan, run from units_try_move's
 * commit tail). Frigate (nation 1) next to a foreign Man-O-War: over a
 * sweep of seeds the drain is 0 / 4 (tie, half of 8) / 8 (the NEIGHBOUR's
 * type constant), and at least one seed slows. PEACE bit set → never
 * slowed; a Privateer mover ignores PEACE. Adjacent foreign Fort → −2 with
 * no roll; Fortress → dead stop; Stockade → nothing.
 */
static int unit_naval_ambush(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;
  const int full = 5 * UNITS_MP_PER_TILE;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("ship-slow alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 5;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Man-O-War");
  units.types[1].movement = 5;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Privateer");
  units.types[2].movement = 8;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fort");
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Fortress");
  colonies.building_type_count = 3;

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* own = units_get(&units, own_id);
  const int foe_id = units_spawn(&units, 1, own_x, own_y - 1);
  ColonizeUnit* foe = units_get(&units, foe_id);
  if (!own || !foe) {
    fx_map_free(&map);
    return fail("ship-slow spawn");
  }
  own->nation_id = nation;
  foe->nation_id = foe_nat;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1; /* nobody human: no popups needed */
  }
  units_set_native_fallout_context(&col1, &map, -1);
  units_set_combat_popups(NULL, NULL);

  ColonizeDosRng rng;
  int slowed = 0;
  int ran = 0;
  for (uint32_t seed = 1000; seed < 60000; seed += 1777) {
    dos_rng_seed(&rng, seed);
    own->moves = full;
    {
      ColonizeWorld w_ = fx_world(&units, &colonies, &map, NULL, &rng, NULL);
      units_ship_slow_scan_w(&w_, own_id);
    }
    const int d = full - own->moves;
    if (d != 0 && d != 4 && d != 8) {
      fx_map_free(&map);
      fprintf(stderr, "ship-slow: seed %u drain %d\n", seed, d);
      return fail("ship-slow drain must be 0 / 4 / 8 (Man-O-War neighbour)");
    }
    slowed += d != 0;
    ran += d == 0;
  }
  if (!slowed || !ran) {
    fx_map_free(&map);
    return fail("ship-slow: expected both slowed and slipped-past outcomes across seeds");
  }

  /* PEACE: never slowed. */
  ai_diplo_or_both(&col1, nation, foe_nat, AI_DIPLO_PEACE);
  for (uint32_t seed = 1000; seed < 60000; seed += 1777) {
    dos_rng_seed(&rng, seed);
    own->moves = full;
    {
      ColonizeWorld w_ = fx_world(&units, &colonies, &map, NULL, &rng, NULL);
      units_ship_slow_scan_w(&w_, own_id);
    }
    if (own->moves != full) {
      fx_map_free(&map);
      return fail("ship-slow: PEACE pair must not be slowed");
    }
  }
  /* Privateer mover ignores PEACE. */
  own->type_index = 2;
  slowed = 0;
  for (uint32_t seed = 1000; seed < 60000; seed += 1777) {
    dos_rng_seed(&rng, seed);
    own->moves = full;
    {
      ColonizeWorld w_ = fx_world(&units, &colonies, &map, NULL, &rng, NULL);
      units_ship_slow_scan_w(&w_, own_id);
    }
    slowed += own->moves != full;
  }
  if (!slowed) {
    fx_map_free(&map);
    return fail("ship-slow: Privateer mover must be slowed despite PEACE");
  }
  own->type_index = 0;

  /* Fort / Fortress branch: foe ship gone, foreign colony on land west. */
  units_despawn(&units, foe_id);
  map.terrain[own_y * map.width + (own_x - 1)] = 1; /* land */
  ColonizeColony* col = fx_colony_add(&colonies, foe_nat, own_x - 1, own_y, 1);
  dos_rng_seed(&rng, 1);
  own->moves = full;
  {
    ColonizeWorld w_ = fx_world(&units, &colonies, &map, NULL, &rng, NULL);
    units_ship_slow_scan_w(&w_, own_id);
  }
  if (own->moves != full) {
    fx_map_free(&map);
    return fail("ship-slow: PEACE colony must not slow");
  }
  ai_diplo_clear_both(&col1, nation, foe_nat, AI_DIPLO_PEACE);
  own->moves = full;
  {
    ColonizeWorld w_ = fx_world(&units, &colonies, &map, NULL, &rng, NULL);
    units_ship_slow_scan_w(&w_, own_id);
  }
  if (own->moves != full) {
    fx_map_free(&map);
    return fail("ship-slow: bare colony must not slow");
  }
  col->has_building[0] = true; /* Stockade: nothing */
  own->moves = full;
  {
    ColonizeWorld w_ = fx_world(&units, &colonies, &map, NULL, &rng, NULL);
    units_ship_slow_scan_w(&w_, own_id);
  }
  if (own->moves != full) {
    fx_map_free(&map);
    return fail("ship-slow: Stockade must not slow");
  }
  col->has_building[1] = true; /* Fort: +2 spent */
  own->moves = full;
  {
    ColonizeWorld w_ = fx_world(&units, &colonies, &map, NULL, &rng, NULL);
    units_ship_slow_scan_w(&w_, own_id);
  }
  if (own->moves != full - 2) {
    fx_map_free(&map);
    fprintf(stderr, "ship-slow: fort left %d\n", own->moves);
    return fail("ship-slow: Fort must cost exactly 2 thirds");
  }
  col->has_building[2] = true; /* Fortress: dead stop */
  own->moves = full;
  {
    ColonizeWorld w_ = fx_world(&units, &colonies, &map, NULL, &rng, NULL);
    units_ship_slow_scan_w(&w_, own_id);
  }
  if (own->moves != 0) {
    fx_map_free(&map);
    return fail("ship-slow: Fortress must exhaust the ship");
  }

  units_set_native_fallout_context(NULL, NULL, -1);
  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: ship-slow (3180 naval half) ok\n");
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
  {"unit_mid_hire_mil", unit_mid_hire_mil},
  {"unit_unload_stance0_no_sticky", unit_unload_stance0_no_sticky},
  {"unit_unload_regular_threatened", unit_unload_regular_threatened},
  {"unit_unload_continental_army_threatened", unit_unload_continental_army_threatened},
  {"unit_unload_continental_cavalry_threatened", unit_unload_continental_cavalry_threatened},
  {"unit_fortify_wake_hunt", unit_fortify_wake_hunt},
  {"unit_garrison_quota_threat_seed", unit_garrison_quota_threat_seed},
  {"unit_garrison_quota_one_fortify", unit_garrison_quota_one_fortify},
  {"unit_labor_shortage_and_ai_flags_5952", unit_labor_shortage_and_ai_flags_5952},
  {"unit_pioneer_conjures_no_tools", unit_pioneer_conjures_no_tools},
  {"unit_peace_tail_does_not_open_war", unit_peace_tail_does_not_open_war},
  {"unit_goal_tail_assaults_defended_colony", unit_goal_tail_assaults_defended_colony},
  {"unit_peace_soldier_fortify_colony", unit_peace_soldier_fortify_colony},
  {"unit_peace_dragoon_fortify_colony", unit_peace_dragoon_fortify_colony},
  {"unit_peace_regular_fortify_colony", unit_peace_regular_fortify_colony},
  {"unit_peace_continental_fortify_colony", unit_peace_continental_fortify_colony},
  {"unit_peace_continental_cavalry_fortify_colony", unit_peace_continental_cavalry_fortify_colony},
  {"unit_peace_artillery_fortify_colony", unit_peace_artillery_fortify_colony},
  {"unit_peace_cannon_fortify_colony", unit_peace_cannon_fortify_colony},
  {"unit_artillery_fortify_colony", unit_artillery_fortify_colony},
  {"unit_war_transport_threatened_colony", unit_war_transport_threatened_colony},
  {"unit_mow_war_transport_threatened", unit_mow_war_transport_threatened},
  {"unit_frigate_war_transport_threatened", unit_frigate_war_transport_threatened},
  {"unit_g_stance_own3_prio7", unit_g_stance_own3_prio7},
  {"unit_g_stance_own4_prio8", unit_g_stance_own4_prio8},
  {"unit_naval_war_hunt", unit_naval_war_hunt},
  {"unit_naval_flee_fort_fire", unit_naval_flee_fort_fire},
  {"unit_privateer_war_hunt", unit_privateer_war_hunt},
  {"unit_privateer_station_keep_hunt", unit_privateer_station_keep_hunt},
  {"unit_naval_multistep_sail", unit_naval_multistep_sail},
  {"unit_indian_war_capital_hunt", unit_indian_war_capital_hunt},
  {"unit_sticky_contact_rehunt", unit_sticky_contact_rehunt},
  {"unit_land_adjacent_combat_chain", unit_land_adjacent_combat_chain},
  {"unit_land_adjacent_colony_seize", unit_land_adjacent_colony_seize},
  {"unit_land_adjacent_foe_prefer_weak", unit_land_adjacent_foe_prefer_weak},
  {"unit_land_adjacent_foe_prefer_treasure", unit_land_adjacent_foe_prefer_treasure},
  {"unit_land_adjacent_foe_prefer_open_over_stockade", unit_land_adjacent_foe_prefer_open_over_stockade},
  {"unit_land_adjacent_foe_prefer_non_veteran", unit_land_adjacent_foe_prefer_non_veteran},
  {"unit_artillery_adjacent_prefer_stockade", unit_artillery_adjacent_prefer_stockade},
  {"unit_dragoon_hunt_prefer_open", unit_dragoon_hunt_prefer_open},
  {"unit_naval_ambush", unit_naval_ambush},
  {"unit_peace_fortify_border_wake", unit_peace_fortify_border_wake},
  {"unit_peace_dragoon_border_wake", unit_peace_dragoon_border_wake},
  {"unit_peace_artillery_border_wake", unit_peace_artillery_border_wake},
  {"unit_peace_regular_border_wake", unit_peace_regular_border_wake},
  {"unit_peace_continental_army_border_wake", unit_peace_continental_army_border_wake},
  {"unit_peace_continental_cavalry_border_wake", unit_peace_continental_cavalry_border_wake},
  {"unit_peace_fortify_ignores_garrison_quota", unit_peace_fortify_ignores_garrison_quota},
  {"unit_peace_fortify_skips_attack_one_type", unit_peace_fortify_skips_attack_one_type},
};
TEST_MAIN(k_cases)

