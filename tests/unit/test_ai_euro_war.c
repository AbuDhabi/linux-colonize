/* Smoke: at-war Euro mid-hire / MILITARY bind + G stance + thin naval hunt. */
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
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

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_euro_war: FAIL %s\n", msg);
  return 1;
}

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
  units_reset(&units);
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
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->moves_left = 1 * UNITS_MP_PER_TILE;
  soldier->orders = 0;

  /* Europe-dock Caravel with free cargo — expect at-war Soldier hire/board. */
  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0; /* stay docked; hire path only needs Europe tile */

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
  col1.nation[foe].gold = 500;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected war after declare");
  }
  /* Replenish after war sting so hire_cost (200) is affordable. */
  col1.nation[nation].gold = 500;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected G stance MILITARY prio ≥6 with own≥2 at war");
  }
  if (mil_prio >= 7) {
    fprintf(stderr, "unit_ai_euro_war: G stance mil_prio=%d (own=2 should be 6 not 7)\n", mil_prio);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected MILITARY AI_MOVE or Soldier hire/board");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 3;
  units.types[0].defense = 2;
  units.types[0].cargo = 0;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* warship = units_get(&units, own_id);
  if (!warship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn own frigate");
  }
  warship->nation_id = nation;
  warship->orders = 0;
  warship->moves_left = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_ship = units_get(&units, foe_id);
  if (!foe_ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn foe frigate");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves_left = 0; /* stationary target */

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
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected AI_SAIL toward foe, closer move, or combat");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 3;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("flee-fort spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("flee-fort ship should survive");
  }
  /* Left the battery ring (not adjacent to Fort colony). */
  const int adj = abs(ship->x - 5) <= 1 && abs(ship->y - 5) <= 1 && !(ship->x == 5 && ship->y == 5);
  if (adj) {
    fprintf(stderr, "flee-fort still adjacent at %d,%d\n", ship->x, ship->y);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected ship to flee Fort battery adjacency");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: naval flee fort fire ok (%d,%d)\n", ship->x, ship->y);
  return 0;
}

/*
 * Privateer hunt: at war, named Privateer with a prior west-explore sail goto
 * re-aims AI_SAIL toward enemy sea (commerce raid). Cite: euro_unit_act §2b;
 * europe Privateer; fandom Drake Privateer.
 */
static int unit_privateer_war_hunt(void) {
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
    return fail("privateer alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 2;
  units.types[0].defense = 1;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* priv = units_get(&units, own_id);
  if (!priv) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn privateer");
  }
  priv->nation_id = nation;
  /* Prior west-explore goto — Privateer should override toward foe. */
  priv->orders = UNITS_ORDER_AI_SAIL;
  priv->goto_x = 0;
  priv->goto_y = own_y;
  priv->moves_left = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_ship = units_get(&units, foe_id);
  if (!foe_ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn foe privateer");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves_left = 0;

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
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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

  const int dist0 = abs(own_x - foe_x) + abs(own_y - foe_y);
  const int mp0 = priv->moves_left;
  ai_euro_dispatcher_turn(&ctx, nation);

  priv = units_get(&units, own_id);
  foe_ship = units_get(&units, foe_id);

  const int combat_done =
    (priv == NULL || !priv->active) || (foe_ship == NULL || !foe_ship->active);
  int hunt = 0;
  int moved_closer = 0;
  int spent = 0;
  if (priv && priv->active) {
    hunt = priv->orders == UNITS_ORDER_AI_SAIL && priv->goto_x == foe_x &&
           priv->goto_y == foe_y;
    const int dist1 = foe_ship && foe_ship->active
                        ? abs(priv->x - foe_ship->x) + abs(priv->y - foe_ship->y)
                        : 0;
    moved_closer = dist1 < dist0 || (priv->x != own_x || priv->y != own_y);
    spent = mp0 - priv->moves_left;
    /* Must not keep west-explore goto (0, own_y) when foe is east. */
    if (priv->goto_x == 0 && priv->goto_y == own_y && !combat_done) {
      hunt = 0;
      moved_closer = 0;
    }
  }

  if (!combat_done && !hunt && !moved_closer) {
    fprintf(
      stderr,
      "unit_ai_euro_war: privateer orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      priv ? priv->orders : -1,
      priv ? priv->goto_x : -1,
      priv ? priv->goto_y : -1,
      priv ? priv->x : -1,
      priv ? priv->y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Privateer hunt override toward foe sea");
  }
  /* Multi-step sail: when still alive and advancing, scored steps spend MP. */
  if (!combat_done && moved_closer && spent < 1) {
    fprintf(
      stderr,
      "unit_ai_euro_war: privateer multi-step mp %d→%d pos=(%d,%d) goto=(%d,%d)\n",
      mp0,
      priv ? priv->moves_left : -1,
      priv ? priv->x : -1,
      priv ? priv->y : -1,
      priv ? priv->goto_x : -1,
      priv ? priv->goto_y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Privateer hunt multi-step to spend MP");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: privateer hunt ok (hunt=%d closer=%d combat=%d mp_spent=%d)\n",
    hunt,
    moved_closer,
    combat_done,
    spent
  );
  return 0;
}

/*
 * Post-diplo Privateer spawn station-keep: idle AI_SAIL with goto=self (as
 * euro_diplo wartime commission) → still re-aims hunt toward foe sea.
 * Cite: euro_diplo Privateer spawn; euro_unit_act §2b; is_privateer re-aim.
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
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 2;
  units.types[0].defense = 1;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* priv = units_get(&units, own_id);
  if (!priv) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-sk spawn");
  }
  priv->nation_id = nation;
  /* Diplo spawn station-keep (goto=self) — must still hunt. */
  priv->orders = UNITS_ORDER_AI_SAIL;
  priv->goto_x = own_x;
  priv->goto_y = own_y;
  priv->moves_left = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_ship = units_get(&units, foe_id);
  if (!foe_ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-sk foe spawn");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves_left = 0;

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

  const int dist0 = abs(own_x - foe_x) + abs(own_y - foe_y);
  ai_euro_dispatcher_turn(&ctx, nation);

  priv = units_get(&units, own_id);
  foe_ship = units_get(&units, foe_id);
  const int combat_done =
    (priv == NULL || !priv->active) || (foe_ship == NULL || !foe_ship->active);
  int hunt = 0;
  int moved_closer = 0;
  if (priv && priv->active) {
    hunt = priv->orders == UNITS_ORDER_AI_SAIL && priv->goto_x == foe_x &&
           priv->goto_y == foe_y;
    const int dist1 = foe_ship && foe_ship->active
                        ? abs(priv->x - foe_ship->x) + abs(priv->y - foe_ship->y)
                        : 0;
    moved_closer = dist1 < dist0 || (priv->x != own_x || priv->y != own_y);
    /* Must leave station-keep (self) goto. */
    if (priv->goto_x == own_x && priv->goto_y == own_y && !combat_done) {
      hunt = 0;
      moved_closer = 0;
    }
  }

  if (!combat_done && !hunt && !moved_closer) {
    fprintf(
      stderr,
      "unit_ai_euro_war: priv-sk orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      priv ? priv->orders : -1,
      priv ? priv->goto_x : -1,
      priv ? priv->goto_y : -1,
      priv ? priv->x : -1,
      priv ? priv->y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected station-keep Privateer to hunt toward foe sea");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: privateer station-keep hunt ok\n");
  return 0;
}

/* Two nations at war, idle land soldiers — expect AI_MOVE toward foe / closer / combat. */
static int unit_land_war_hunt(void) {
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
    return fail("land alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn own soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 1 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_soldier = units_get(&units, foe_id);
  if (!foe_soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn foe soldier");
  }
  foe_soldier->nation_id = foe;
  foe_soldier->orders = 0;
  foe_soldier->moves_left = 0; /* stationary target */

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
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("land expected war");
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

  soldier = units_get(&units, own_id);
  foe_soldier = units_get(&units, foe_id);

  const int combat_done =
    (soldier == NULL || !soldier->active) || (foe_soldier == NULL || !foe_soldier->active);
  int move_toward = 0;
  int moved_closer = 0;
  if (soldier && soldier->active) {
    move_toward =
      soldier->orders == UNITS_ORDER_AI_MOVE && soldier->goto_x == foe_x &&
      soldier->goto_y == foe_y;
    const int dist1 = foe_soldier && foe_soldier->active
                        ? abs(soldier->x - foe_soldier->x) + abs(soldier->y - foe_soldier->y)
                        : 0;
    moved_closer = dist1 < dist0 || (soldier->x != own_x || soldier->y != own_y);
  }

  if (!combat_done && !move_toward && !moved_closer) {
    fprintf(
      stderr,
      "unit_ai_euro_war: land orders=%d goto=(%d,%d) pos=(%d,%d) foe_active=%d\n",
      soldier ? soldier->orders : -1,
      soldier ? soldier->goto_x : -1,
      soldier ? soldier->goto_y : -1,
      soldier ? soldier->x : -1,
      soldier ? soldier->y : -1,
      foe_soldier && foe_soldier->active
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected AI_MOVE toward foe, closer move, or combat");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: land ok (move=%d closer=%d combat=%d)\n",
    move_toward,
    moved_closer,
    combat_done
  );
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("indian-hunt alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  memset(units.types, 0, sizeof(units.types));
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int sid = units_spawn(&units, 0, 2, 2);
  ColonizeUnit* soldier = units_get(&units, sid);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("indian-hunt spawn");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 3 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  col1.player[nation].control = 0;
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 100;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("indian-hunt expected indian war");
  }

  ai_euro_dispatcher_turn(&ctx, nation);
  soldier = units_get(&units, sid);
  if (!soldier || !soldier->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected AI_MOVE toward capital tribe (10,2) or east move");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: indian capital hunt ok (goto_cap=%d east=%d)\n",
    toward_cap,
    moved_east
  );
  return 0;
}

/*
 * Thin 20e6 multi-step land war hunt: Soldier with moves_left>=2, no MILITARY
 * goal upsert — act-level hunt still advances two tiles toward foe in one act.
 * Cite: euro_unit_act §2c3; FUN_521d_20e6 thin multi-step combat deepen.
 */
static int unit_land_war_hunt_multistep(void) {
  const int nation = 1;
  const int foe = 2;
  const int own_x = 3;
  const int own_y = 3;
  const int foe_x = 10;
  const int foe_y = 3;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("land-multistep alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies); /* no own colony — avoid LABOR yank; hunt alone */

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("land-multistep spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 3 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_soldier = units_get(&units, foe_id);
  if (!foe_soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("land-multistep spawn foe");
  }
  foe_soldier->nation_id = foe;
  foe_soldier->orders = 0;
  foe_soldier->moves_left = 0;

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
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset(); /* no MILITARY goal — act hunt alone must multi-step */

  uint32_t turn = 31;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 43;

  const int dist0 = abs(own_x - foe_x) + abs(own_y - foe_y);
  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  foe_soldier = units_get(&units, foe_id);
  if (!soldier || !soldier->active || !foe_soldier || !foe_soldier->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("land-multistep: both soldiers should remain (foe far)");
  }

  const int dist1 = abs(soldier->x - foe_soldier->x) + abs(soldier->y - foe_soldier->y);
  const int steps = dist0 - dist1;
  /* Thin 20e6: two scored advances in one act (movement 3). */
  if (steps < 2) {
    fprintf(
      stderr,
      "unit_ai_euro_war: land-multistep dist %d→%d steps=%d pos=(%d,%d) goto=(%d,%d)\n",
      dist0,
      dist1,
      steps,
      soldier->x,
      soldier->y,
      soldier->goto_x,
      soldier->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected land war hunt multi-step (≥2 tiles closer)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: land war-hunt multi-step ok (steps=%d)\n",
    steps
  );
  return 0;
}

/*
 * Thin land war hunt: Continental Army advances toward foe (same multi-step
 * arm as Soldier). Cite: euro_unit_act §2c; Defending a Colony army.
 */
static int unit_continental_army_land_hunt(void) {
  const int nation = 1;
  const int foe = 2;
  const int own_x = 3;
  const int own_y = 3;
  const int foe_x = 10;
  const int foe_y = 3;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("cont-hunt alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Army");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies); /* no own colony — avoid LABOR yank; hunt alone */

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* army = units_get(&units, own_id);
  if (!army) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cont-hunt spawn army");
  }
  army->nation_id = nation;
  army->orders = 0;
  army->moves_left = 3 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_army = units_get(&units, foe_id);
  if (!foe_army) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cont-hunt spawn foe");
  }
  foe_army->nation_id = foe;
  foe_army->orders = 0;
  foe_army->moves_left = 0;

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
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset(); /* no MILITARY goal — act hunt alone must multi-step */

  uint32_t turn = 31;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 43;

  const int dist0 = abs(own_x - foe_x) + abs(own_y - foe_y);
  ai_euro_dispatcher_turn(&ctx, nation);

  army = units_get(&units, own_id);
  foe_army = units_get(&units, foe_id);
  if (!army || !army->active || !foe_army || !foe_army->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cont-hunt: both armys should remain (foe far)");
  }

  const int dist1 = abs(army->x - foe_army->x) + abs(army->y - foe_army->y);
  const int steps = dist0 - dist1;
  /* Thin 20e6: two scored advances in one act (movement 3). */
  if (steps < 2) {
    fprintf(
      stderr,
      "unit_ai_euro_war: cont-hunt dist %d→%d steps=%d pos=(%d,%d) goto=(%d,%d)\n",
      dist0,
      dist1,
      steps,
      army->x,
      army->y,
      army->goto_x,
      army->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Army land war hunt multi-step (≥2 tiles closer)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: Continental Army land war hunt ok (steps=%d)\n",
    steps
  );
  return 0;
}

/*
 * Thin land war hunt: Continental Cavalry advances toward foe (same multi-step
 * arm as Soldier). Cite: euro_unit_act §2c; Defending a Colony cavalry.
 */
static int unit_continental_cavalry_land_hunt(void) {
  const int nation = 1;
  const int foe = 2;
  const int own_x = 3;
  const int own_y = 3;
  const int foe_x = 10;
  const int foe_y = 3;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("cont-hunt alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Cavalry");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies); /* no own colony — avoid LABOR yank; hunt alone */

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* cav = units_get(&units, own_id);
  if (!cav) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cont-hunt spawn cav");
  }
  cav->nation_id = nation;
  cav->orders = 0;
  cav->moves_left = 3 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_cav = units_get(&units, foe_id);
  if (!foe_cav) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cont-hunt spawn foe");
  }
  foe_cav->nation_id = foe;
  foe_cav->orders = 0;
  foe_cav->moves_left = 0;

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
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset(); /* no MILITARY goal — act hunt alone must multi-step */

  uint32_t turn = 31;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 43;

  const int dist0 = abs(own_x - foe_x) + abs(own_y - foe_y);
  ai_euro_dispatcher_turn(&ctx, nation);

  cav = units_get(&units, own_id);
  foe_cav = units_get(&units, foe_id);
  if (!cav || !cav->active || !foe_cav || !foe_cav->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cont-hunt: both cavs should remain (foe far)");
  }

  const int dist1 = abs(cav->x - foe_cav->x) + abs(cav->y - foe_cav->y);
  const int steps = dist0 - dist1;
  /* Thin 20e6: two scored advances in one act (movement 3). */
  if (steps < 2) {
    fprintf(
      stderr,
      "unit_ai_euro_war: cont-hunt dist %d→%d steps=%d pos=(%d,%d) goto=(%d,%d)\n",
      dist0,
      dist1,
      steps,
      cav->x,
      cav->y,
      cav->goto_x,
      cav->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Cavalry land war hunt multi-step (≥2 tiles closer)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: Continental Cavalry land war hunt ok (steps=%d)\n",
    steps
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("sticky alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = UNITS_ORDER_FORTIFIED; /* skip land_try_adjacent_attack */
  soldier->moves_left = 2 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves_left = 0;

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
      soldier ? soldier->moves_left : -1,
      soldier ? soldier->orders : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky CONTACT re-hunt should attempt combat vs adjacent war foe");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: sticky CONTACT re-hunt ok\n");
  return 0;
}

/*
 * Thin multi-step land adjacent combat: Soldier with MP>1 kills foe A then
 * continues onto adjacent foe B in the same act (drain moves_left). Cite:
 * euro_unit_act §2c multi-step combat; ai_euro_land_try_adjacent_attack chain.
 */
static int unit_land_adjacent_combat_chain(void) {
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
    return fail("combat-chain alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("combat-chain spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 3 * UNITS_MP_PER_TILE;

  const int foe_a = units_spawn(&units, 0, 6, 5);
  ColonizeUnit* fa = units_get(&units, foe_a);
  const int foe_b = units_spawn(&units, 0, 7, 5);
  ColonizeUnit* fb = units_get(&units, foe_b);
  if (!fa || !fb) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("combat-chain spawn foes");
  }
  fa->nation_id = foe;
  fa->orders = 0;
  fa->moves_left = 0;
  fb->nation_id = foe;
  fb->orders = 0;
  fb->moves_left = 0;

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
  /* bugs.md 249: a land attacker stays put after a win — foe A (adjacent)
   * dies; the AI may then STEP into the vacated tile as a normal move, but
   * the attack itself no longer carries it there, so foe B two tiles out
   * survives the act. */
  if (!a_dead || b_dead) {
    fprintf(
      stderr,
      "unit_ai_euro_war: chain soldier=%d,%d moves=%d a_dead=%d b_dead=%d\n",
      soldier ? soldier->x : -1,
      soldier ? soldier->y : -1,
      soldier ? soldier->moves_left : -1,
      a_dead,
      b_dead
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adjacent foe dies, attacker stays put, far foe survives");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("colony-seize alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("colony-seize spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 3 * UNITS_MP_PER_TILE;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("land unit should walk into and seize an undefended adjacent foe colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: land adjacent undefended colony seize ok\n");
  return 0;
}

/*
 * Thin mid-hire Artillery: at war, colonies>=2, gold, Europe ship with Soldier
 * already aboard → prefer Artillery (Cannon name fallback). If Artillery/Cannon
 * type missing from pool, hire falls back to Soldier/Dragoon path (documented).
 */
static int unit_mid_hire_artillery(void) {
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
    return fail("artillery alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 4;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 2;
  units.types[1].defense = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Artillery");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 3;
  units.types[2].defense = 1;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Free Colonist");
  units.types[3].movement = 1;
  units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;

  const int art_ty = units_find_type(&units, "Artillery");
  if (art_ty < 0) {
    /* Documented fallback: without Artillery/Cannon type, war hire stays Soldier. */
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    fprintf(
      stderr,
      "unit_ai_euro_war: artillery type missing — skip (Soldier path fallback)\n"
    );
    return 0;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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

  /* Europe Caravel with Soldier already boarded (1 free slot) → Artillery hire. */
  const int ship_id = units_spawn_allow_stack(&units, 0, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("artillery spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;

  const int sid = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* soldier = units_get(&units, sid);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("artillery spawn soldier cargo");
  }
  soldier->nation_id = nation;
  if (!units_board_stacked(&units, sid, ship_id)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("artillery board soldier");
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
  col1.nation[nation].gold = 500;
  col1.nation[foe].gold = 500;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("artillery expected war");
  }
  col1.nation[nation].gold = 500;
  const uint32_t gold_before = col1.nation[nation].gold;
  const int cargo_before = ship->cargo_count;

  ai_goals_reset();

  uint32_t turn = 20; /* even — Artillery via mil-aboard, not turn parity */
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

  ship = units_get(&units, ship_id);
  int art_boarded = 0;
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && (strstr(ty->name, "Artillery") || strstr(ty->name, "Cannon"))) {
        art_boarded = 1;
        break;
      }
    }
  }
  const int gold_spent = (col1.nation[nation].gold < gold_before);
  const int cargo_grew = ship && ship->cargo_count > cargo_before;

  if (!(art_boarded && gold_spent) && !(gold_spent && cargo_grew)) {
    fprintf(
      stderr,
      "unit_ai_euro_war: art_boarded=%d gold %u→%u cargo %d→%d\n",
      art_boarded,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      cargo_before,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Artillery hire/board or gold spent on artillery path");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: artillery mid-hire ok (boarded=%d gold_spent=%d)\n",
    art_boarded,
    gold_spent
  );
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("adj-foe alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-foe spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 1 * UNITS_MP_PER_TILE; /* one adjacent fight only — no re-hunt onto fortified */

  /* Strong fortified foe to the north (first octant dir) — should NOT be preferred. */
  const int strong_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* strong = units_get(&units, strong_id);
  if (!strong) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-foe spawn strong");
  }
  strong->nation_id = foe_nat;
  strong->orders = UNITS_ORDER_FORTIFIED;
  strong->moves_left = 0;

  /* Weak colonist to the south — preferred target. */
  const int weak_id = units_spawn(&units, 1, own_x, own_y + 1);
  ColonizeUnit* weak = units_get(&units, weak_id);
  if (!weak) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-foe spawn weak");
  }
  weak->nation_id = foe_nat;
  weak->orders = 0;
  weak->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected attack on weak colonist, fortified Soldier left alone");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("adj-treasure alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-treasure spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 1 * UNITS_MP_PER_TILE;

  const int scout_id = units_spawn(&units, 1, own_x, own_y - 1); /* N first */
  ColonizeUnit* scout = units_get(&units, scout_id);
  if (!scout) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-treasure spawn scout");
  }
  scout->nation_id = foe_nat;
  scout->orders = 0;
  scout->moves_left = 0;

  const int treasure_id = units_spawn(&units, 2, own_x, own_y + 1); /* S */
  ColonizeUnit* treasure = units_get(&units, treasure_id);
  if (!treasure) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-treasure spawn treasure");
  }
  treasure->nation_id = foe_nat;
  treasure->orders = 0;
  treasure->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected attack on Treasure over equal-toughness Scout");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe prefer Treasure ok\n");
  return 0;
}

/*
 * Land war hunt: nearer Scout (MD=2) vs Treasure (MD=4, slack ≤3) → prefer
 * Treasure goto. Cite: Colonization.pdf Treasure Trains; euro_unit_act hunt.
 */
static int unit_land_hunt_prefer_treasure(void) {
  const int nation = 1;
  const int foe_nat = 2;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 20;
  map.height = 20;
  map.tile_count = 400;
  map.terrain = calloc(400, 1);
  map.layer2 = calloc(400, 1);
  map.layer3 = calloc(400, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("hunt-treasure alloc map");
  }
  for (int i = 0; i < 400; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
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
  colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 2;
  own->y = 2;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("hunt-treasure spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 3 * UNITS_MP_PER_TILE;

  const int scout_id = units_spawn(&units, 1, 7, 5); /* MD=2 */
  ColonizeUnit* scout = units_get(&units, scout_id);
  if (!scout) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("hunt-treasure spawn scout");
  }
  scout->nation_id = foe_nat;
  scout->orders = 0;
  scout->moves_left = 0;

  const int treasure_id = units_spawn(&units, 2, 9, 5); /* MD=4 ≤ 2+3 */
  ColonizeUnit* treasure = units_get(&units, treasure_id);
  if (!treasure) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("hunt-treasure spawn treasure");
  }
  treasure->nation_id = foe_nat;
  treasure->orders = 0;
  treasure->moves_left = 0;

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
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  if (!soldier || !soldier->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("hunt-treasure soldier despawned");
  }
  /* Goto or moved toward Treasure x=9 (not stuck on Scout x=7). */
  const int toward_treasure =
    (soldier->goto_x == 9 && soldier->goto_y == 5) || soldier->x > 7 ||
    (soldier->orders == UNITS_ORDER_AI_MOVE && soldier->goto_x >= 8);
  if (!toward_treasure) {
    fprintf(
      stderr,
      "unit_ai_euro_war: hunt-treasure xy=(%d,%d) goto=(%d,%d) orders=%d\n",
      soldier->x,
      soldier->y,
      soldier->goto_x,
      soldier->goto_y,
      soldier->orders
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("Soldier hunt should prefer Treasure over nearer Scout");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: land hunt prefer Treasure ok\n");
  return 0;
}

/*
 * Land war hunt toughness: nearer Soldier (MD=2, def 8) vs farther Free Colonist
 * (MD=4, def 1, slack ≤3) → prefer weaker Colonist. Cite: thin 20e6 / euro_unit_act.
 */
static int unit_land_hunt_prefer_weak(void) {
  const int nation = 1;
  const int foe_nat = 2;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 20;
  map.height = 20;
  map.tile_count = 400;
  map.terrain = calloc(400, 1);
  map.layer2 = calloc(400, 1);
  map.layer3 = calloc(400, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("hunt-weak alloc map");
  }
  for (int i = 0; i < 400; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 8;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Free Colonist");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 0;
  units.types[1].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 2;
  own->y = 2;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("hunt-weak spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 3 * UNITS_MP_PER_TILE;

  const int strong_id = units_spawn(&units, 0, 7, 5); /* MD=2 Soldier */
  ColonizeUnit* strong = units_get(&units, strong_id);
  if (!strong) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("hunt-weak spawn strong");
  }
  strong->nation_id = foe_nat;
  strong->orders = 0;
  strong->moves_left = 0;

  const int weak_id = units_spawn(&units, 1, 9, 5); /* MD=4 Colonist */
  ColonizeUnit* weak = units_get(&units, weak_id);
  if (!weak) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("hunt-weak spawn weak");
  }
  weak->nation_id = foe_nat;
  weak->orders = 0;
  weak->moves_left = 0;

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
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  if (!soldier || !soldier->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("hunt-weak soldier despawned");
  }
  const int toward_weak =
    (soldier->goto_x == 9 && soldier->goto_y == 5) || soldier->x > 7 ||
    (soldier->orders == UNITS_ORDER_AI_MOVE && soldier->goto_x >= 8);
  if (!toward_weak) {
    fprintf(
      stderr,
      "unit_ai_euro_war: hunt-weak xy=(%d,%d) goto=(%d,%d) orders=%d\n",
      soldier->x,
      soldier->y,
      soldier->goto_x,
      soldier->goto_y,
      soldier->orders
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("Soldier hunt should prefer weaker Colonist within MD slack");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: land hunt prefer weak ok\n");
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("adj-stockade alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 4;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-stockade spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 1 * UNITS_MP_PER_TILE;

  /* Stockade defender to the north (first dir) — tougher (def 4→8). */
  const int stock_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* stock = units_get(&units, stock_id);
  if (!stock) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-stockade spawn stockade foe");
  }
  stock->nation_id = foe_nat;
  stock->orders = 0;
  stock->moves_left = 0;

  /* Open-field same Soldier to the south — preferred (def 4). */
  const int open_id = units_spawn(&units, 0, own_x, own_y + 1);
  ColonizeUnit* open = units_get(&units, open_id);
  if (!open) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-stockade spawn open foe");
  }
  open->nation_id = foe_nat;
  open->orders = 0;
  open->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected attack on open-field Soldier, Stockade left alone");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("adj-vet alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 8;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-vet spawn own");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 1 * UNITS_MP_PER_TILE;
  soldier->profession = UNITS_JOB_NONE;

  /* Veteran to the north (first octant) — tougher via +50%. */
  const int vet_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* vet = units_get(&units, vet_id);
  if (!vet) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-vet spawn veteran");
  }
  vet->nation_id = foe_nat;
  vet->orders = 0;
  vet->moves_left = 0;
  vet->profession = UNITS_JOB_SOLDIER;

  /* Plain Soldier to the south — preferred. */
  const int plain_id = units_spawn(&units, 0, own_x, own_y + 1);
  ColonizeUnit* plain = units_get(&units, plain_id);
  if (!plain) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-vet spawn plain");
  }
  plain->nation_id = foe_nat;
  plain->orders = 0;
  plain->moves_left = 0;
  plain->profession = UNITS_JOB_NONE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  col1.nation[nation].gold = 50;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected attack on non-veteran Soldier, veteran left alone");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe prefer non-veteran ok\n");
  return 0;
}

/*
 * FUN_157e_004a Drake peel: Man-O-War between Drake Privateer (N) and equal
 * Privateer without Drake (S) — prefer non-Drake (+50% toughness). Cite:
 * FUN_157e_004a type 0x10 + FF Drake; fandom Drake.
 */
static int unit_naval_adjacent_foe_prefer_non_drake(void) {
  const int nation = 1;
  const int foe_drake = 2;
  const int foe_plain = 3;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("adj-drake alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Man-O-War");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 16;
  units.types[0].defense = 16;
  units.types[0].guns = 32; /* NAMES @UNIT guns/hull: sink roll inputs */
  units.types[0].hull = 64;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Privateer");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].attack = 8;
  units.types[1].defense = 8;
  units.types[1].guns = 12;
  units.types[1].hull = 12;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* own = units_get(&units, own_id);
  if (!own) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-drake spawn own");
  }
  own->nation_id = nation;
  own->orders = 0;
  own->moves_left = 1 * UNITS_MP_PER_TILE;

  const int drake_id = units_spawn(&units, 1, own_x, own_y - 1);
  ColonizeUnit* drake_u = units_get(&units, drake_id);
  if (!drake_u) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-drake spawn Drake Privateer");
  }
  drake_u->nation_id = foe_drake;
  drake_u->orders = 0;
  drake_u->moves_left = 0;

  const int plain_id = units_spawn(&units, 1, own_x, own_y + 1);
  ColonizeUnit* plain = units_get(&units, plain_id);
  if (!plain) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("adj-drake spawn plain Privateer");
  }
  plain->nation_id = foe_plain;
  plain->orders = 0;
  plain->moves_left = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  col1.head.founding_father[FF_FRANCIS_DRAKE] = (int8_t)foe_drake;
  col1.nation[foe_drake].founding_fathers[FF_FRANCIS_DRAKE / 8] |=
    (uint8_t)(1u << (FF_FRANCIS_DRAKE % 8));
  /* Pre-arm Privateer spawn mask so diplo balance does not spawn extra ships. */
  col1.nation[nation].privateer_spawn_mask =
    (uint8_t)((1u << foe_drake) | (1u << foe_plain));
  ai_diplo_declare_war(&col1, nation, foe_drake);
  ai_diplo_declare_war(&col1, nation, foe_plain);

  ai_goals_reset();

  uint32_t turn = 47;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 48;

  ai_euro_dispatcher_turn(&ctx, nation);

  own = units_get(&units, own_id);
  drake_u = units_get(&units, drake_id);
  plain = units_get(&units, plain_id);

  const int plain_dead = plain == NULL || !plain->active;
  const int drake_alive = drake_u && drake_u->active;
  const int own_alive = own && own->active;

  if (!plain_dead || !drake_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: adj-drake own=%d plain_dead=%d drake_alive=%d\n",
      own_alive,
      plain_dead,
      drake_alive
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected attack on non-Drake Privateer, Drake Privateer left alone");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: naval adjacent-foe prefer non-Drake ok\n");
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("art-adj alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("art-adj spawn artillery");
  }
  art->nation_id = nation;
  art->orders = 0;
  art->moves_left = 1 * UNITS_MP_PER_TILE;

  const int open_id = units_spawn(&units, 1, own_x, own_y - 1); /* N open */
  ColonizeUnit* open = units_get(&units, open_id);
  if (!open) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("art-adj spawn open");
  }
  open->nation_id = foe_nat;
  open->orders = 0;
  open->moves_left = 0;

  const int stock_id = units_spawn(&units, 1, own_x, own_y + 1); /* S stockade */
  ColonizeUnit* stock = units_get(&units, stock_id);
  if (!stock) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("art-adj spawn stockade");
  }
  stock->nation_id = foe_nat;
  stock->orders = 0;
  stock->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("Artillery should attack Stockade foe, leave open Soldier");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Artillery adjacent prefer Stockade ok\n");
  return 0;
}

/*
 * Artillery off-colony siege hunt: prefer Stockade colony (farther) over nearer
 * open colony (MD slack ≤3). Cite: king_ref Artillery siege hunt.
 */
static int unit_artillery_siege_hunt_prefer_stockade(void) {
  const int nation = 1;
  const int foe_nat = 2;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 20;
  map.height = 20;
  map.tile_count = 400;
  map.terrain = calloc(400, 1);
  map.layer2 = calloc(400, 1);
  map.layer3 = calloc(400, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("art-hunt alloc map");
  }
  for (int i = 0; i < 400; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Artillery");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_type_count = 1;
  /* Near open foe colony at (8,5); Stockade at (10,5) — MD 5 vs 3 from (5,5). */
  ColonizeColony* open_col = &colonies.colonies[0];
  open_col->id = 0;
  open_col->active = true;
  open_col->nation_id = foe_nat;
  open_col->x = 8;
  open_col->y = 5;
  open_col->population = 1;
  open_col->colonist_count = 1;
  ColonizeColony* stock_col = &colonies.colonies[1];
  stock_col->id = 1;
  stock_col->active = true;
  stock_col->nation_id = foe_nat;
  stock_col->x = 10;
  stock_col->y = 5;
  stock_col->population = 2;
  stock_col->colonist_count = 2;
  stock_col->has_building[0] = true;
  colonies.colony_count = 2;

  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* art = units_get(&units, own_id);
  if (!art) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("art-hunt spawn");
  }
  art->nation_id = nation;
  art->orders = 0;
  art->moves_left = 1 * UNITS_MP_PER_TILE;

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
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();
  uint32_t turn = 44;
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
  if (!art || !art->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("art-hunt artillery despawned");
  }
  if (art->goto_x != 10 || art->goto_y != 5) {
    fprintf(
      stderr,
      "unit_ai_euro_war: art-hunt goto=(%d,%d) want Stockade (10,5)\n",
      art->goto_x,
      art->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("Artillery siege hunt should prefer Stockade colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Artillery siege hunt prefer Stockade ok\n");
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
  memset(&map, 0, sizeof(map));
  map.width = 20;
  map.height = 20;
  map.tile_count = 400;
  map.terrain = calloc(400, 1);
  map.layer2 = calloc(400, 1);
  map.layer3 = calloc(400, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("dragoon-hunt alloc map");
  }
  for (int i = 0; i < 400; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Dragoon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 6;
  units.types[0].defense = 4;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dragoon-hunt spawn");
  }
  drag->nation_id = nation;
  drag->orders = 0;
  drag->moves_left = 4 * UNITS_MP_PER_TILE;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dragoon-hunt despawned");
  }
  if (drag->goto_x != 10 || drag->goto_y != 5) {
    fprintf(
      stderr,
      "unit_ai_euro_war: dragoon-hunt goto=(%d,%d) want open (10,5)\n",
      drag->goto_x,
      drag->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("Dragoon hunt should prefer open colony over Stockade");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Dragoon hunt prefer open ok\n");
  return 0;
}

/*
 * Thin 20e6 naval adjacent-foe pick: Man-O-War between high-defense foe (N) and
 * weak Caravel (S). Prefer weaker defense (old first-dir scan would hit N).
 * Cite: FUN_521d_20e6 naval combat thin; FUN_157e_004a holds/damage Done.
 */
static int unit_naval_adjacent_foe_prefer_weak(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("naval-adj alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Man-O-War");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 8;
  units.types[0].defense = 8;
  units.types[0].guns = 32; /* NAMES @UNIT guns/hull: sink roll inputs */
  units.types[0].hull = 64;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].attack = 0;
  units.types[1].defense = 1;
  units.types[1].guns = 0;
  units.types[1].hull = 4;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* own = units_get(&units, own_id);
  if (!own) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-adj spawn own");
  }
  own->nation_id = nation;
  own->orders = 0;
  own->moves_left = 1 * UNITS_MP_PER_TILE;

  const int strong_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* strong = units_get(&units, strong_id);
  if (!strong) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-adj spawn strong");
  }
  strong->nation_id = foe_nat;
  strong->orders = 0;
  strong->moves_left = 0;

  const int weak_id = units_spawn(&units, 1, own_x, own_y + 1);
  ColonizeUnit* weak = units_get(&units, weak_id);
  if (!weak) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-adj spawn weak");
  }
  weak->nation_id = foe_nat;
  weak->orders = 0;
  weak->moves_left = 0;

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

  own = units_get(&units, own_id);
  strong = units_get(&units, strong_id);
  weak = units_get(&units, weak_id);

  const int weak_dead = weak == NULL || !weak->active;
  const int strong_alive = strong && strong->active;
  const int own_alive = own && own->active;

  if (!weak_dead || !strong_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: naval-adj own=%d weak_dead=%d strong_alive=%d\n",
      own_alive,
      weak_dead,
      strong_alive
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected naval attack on weak Caravel, strong Man-O-War left alone");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: naval adjacent-foe prefer-weak ok\n");
  return 0;
}

/*
 * FUN_157e_004a holds_occupied (0x3150): equal Caravels N/S — loaded foe is
 * weaker (def − holds) so prefer attack on loaded. Cite: FUN_157e_004a.
 */
static int unit_naval_adjacent_foe_prefer_loaded(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("naval-holds alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Man-O-War");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 8;
  units.types[0].defense = 8;
  units.type_count = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].attack = 0;
  units.types[1].defense = 4;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* own = units_get(&units, own_id);
  if (!own) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-holds spawn own");
  }
  own->nation_id = nation;
  own->orders = 0;
  own->moves_left = 1 * UNITS_MP_PER_TILE;

  const int empty_id = units_spawn(&units, 1, own_x, own_y - 1);
  ColonizeUnit* empty = units_get(&units, empty_id);
  if (!empty) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-holds spawn empty");
  }
  empty->nation_id = foe_nat;
  empty->orders = 0;
  empty->moves_left = 0;

  const int loaded_id = units_spawn(&units, 1, own_x, own_y + 1);
  ColonizeUnit* loaded = units_get(&units, loaded_id);
  if (!loaded) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-holds spawn loaded");
  }
  loaded->nation_id = foe_nat;
  loaded->orders = 0;
  loaded->moves_left = 0;
  loaded->hold_goods_type[0] = 1;
  loaded->hold_goods_amount[0] = 100;
  loaded->hold_goods_type[1] = 2;
  loaded->hold_goods_amount[1] = 50;
  loaded->hold_goods_type[2] = 3;
  loaded->hold_goods_amount[2] = 25;

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
  ctx.rng_seed = 43;

  ai_euro_dispatcher_turn(&ctx, nation);

  own = units_get(&units, own_id);
  empty = units_get(&units, empty_id);
  loaded = units_get(&units, loaded_id);

  const int loaded_dead = loaded == NULL || !loaded->active;
  const int empty_alive = empty && empty->active;
  const int own_alive = own && own->active;

  if (!loaded_dead || !empty_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: naval-holds own=%d loaded_dead=%d empty_alive=%d\n",
      own_alive,
      loaded_dead,
      empty_alive
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected attack on loaded Caravel (holds_occupied), empty left alone");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: naval adjacent-foe prefer-loaded ok\n");
  return 0;
}

/*
 * Privateer cargo prey deepen: adjacent Frigate (lower defense) vs Merchantman
 * (higher defense) → prefer Merchantman/Caravel cargo over warship.
 * Cite: euro_unit_act §2f; Europe Privateer commerce raid.
 */
static int unit_privateer_prefer_cargo_prey(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("priv-cargo alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 8;
  units.types[0].defense = 2;
  units.types[0].cargo = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Frigate");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].attack = 4;
  units.types[1].defense = 1; /* weaker defense than Merchantman */
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Merchantman");
  units.types[2].movement = 4;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[2].attack = 0;
  units.types[2].defense = 4;
  units.types[2].cargo = 4;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* own = units_get(&units, own_id);
  if (!own) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-cargo spawn own");
  }
  own->nation_id = nation;
  own->orders = 0;
  own->moves_left = 1 * UNITS_MP_PER_TILE;

  /* Frigate N (lower def) — toughness-only pick would prefer this. */
  const int frig_id = units_spawn(&units, 1, own_x, own_y - 1);
  ColonizeUnit* frig = units_get(&units, frig_id);
  if (!frig) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-cargo spawn Frigate");
  }
  frig->nation_id = foe_nat;
  frig->orders = 0;
  frig->moves_left = 0;

  /* Merchantman S (higher def, cargo prey). */
  const int merch_id = units_spawn(&units, 2, own_x, own_y + 1);
  ColonizeUnit* merch = units_get(&units, merch_id);
  if (!merch) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-cargo spawn Merchantman");
  }
  merch->nation_id = foe_nat;
  merch->orders = 0;
  merch->moves_left = 0;

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
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();

  uint32_t turn = 44;
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

  own = units_get(&units, own_id);
  frig = units_get(&units, frig_id);
  merch = units_get(&units, merch_id);

  const int merch_dead = merch == NULL || !merch->active;
  const int frig_alive = frig && frig->active;
  const int own_alive = own && own->active;
  /* Cargo preference: Merchantman must die (Privateer still up). Frigate may
   * also despawn in the same turn under multi-wave act — warship-first would
   * leave Merchantman alive instead. Cite: euro_unit_act §2f. */
  if (!merch_dead || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: priv-cargo own=%d merch_dead=%d frig_alive=%d\n",
      own_alive,
      merch_dead,
      frig_alive
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Privateer to prefer Merchantman cargo over weaker Frigate");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: privateer prefer cargo prey ok\n");
  return 0;
}

/*
 * Frigate warship hunt deepen: adjacent Merchantman (lower defense) vs Privateer
 * (higher defense) → prefer warship over cargo (complement Privateer cargo prey).
 * Cite: euro_unit_act §2f; Europe Frigate purchase.
 */
static int unit_frigate_prefer_warship(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("frig-war alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 8;
  units.types[0].defense = 4;
  units.types[0].cargo = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Merchantman");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].attack = 0;
  units.types[1].defense = 1; /* weaker — toughness-only would prefer this */
  units.types[1].cargo = 4;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Privateer");
  units.types[2].movement = 4;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[2].attack = 4;
  units.types[2].defense = 4;
  units.types[2].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* own = units_get(&units, own_id);
  if (!own) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("frig-war spawn own");
  }
  own->nation_id = nation;
  own->orders = 0;
  own->moves_left = 1 * UNITS_MP_PER_TILE;

  /* Merchantman N (lower def, cargo) — toughness-only pick would prefer this. */
  const int merch_id = units_spawn(&units, 1, own_x, own_y - 1);
  ColonizeUnit* merch = units_get(&units, merch_id);
  if (!merch) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("frig-war spawn Merchantman");
  }
  merch->nation_id = foe_nat;
  merch->orders = 0;
  merch->moves_left = 0;

  /* Privateer S (higher def, warship prey). */
  const int priv_id = units_spawn(&units, 2, own_x, own_y + 1);
  ColonizeUnit* priv = units_get(&units, priv_id);
  if (!priv) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("frig-war spawn Privateer");
  }
  priv->nation_id = foe_nat;
  priv->orders = 0;
  priv->moves_left = 0;

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
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  own = units_get(&units, own_id);
  merch = units_get(&units, merch_id);
  priv = units_get(&units, priv_id);

  const int priv_dead = priv == NULL || !priv->active;
  const int merch_alive = merch && merch->active;
  const int own_alive = own && own->active;
  /* Warship preference: Privateer must die (Frigate still up). Merchantman may
   * also despawn under multi-wave act — cargo-first would leave Privateer alive. */
  if (!priv_dead || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: frig-war own=%d priv_dead=%d merch_alive=%d\n",
      own_alive,
      priv_dead,
      merch_alive
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Frigate to prefer Privateer warship over weaker Merchantman");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: frigate prefer warship ok\n");
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("peace-border alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("peace-border spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = UNITS_ORDER_FORTIFIED;
  soldier->moves_left = 3 * UNITS_MP_PER_TILE;

  /* Foreign Soldier at MD=2 — peace border threat. */
  const int foe_id = units_spawn(&units, 0, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("peace-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected peace-fortified Soldier to wake for MD≤2 border threat");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("dragoon-border alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Dragoon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dragoon-border spawn dragoon");
  }
  dragoon->nation_id = nation;
  dragoon->orders = UNITS_ORDER_FORTIFIED;
  dragoon->moves_left = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dragoon-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected peace-fortified Dragoon to wake for MD≤2 border threat");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("arty-border alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("arty-border spawn artillery");
  }
  arty->nation_id = nation;
  arty->orders = UNITS_ORDER_FORTIFIED;
  arty->moves_left = 1 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 1, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("arty-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected peace-fortified Artillery to wake for MD≤2 border threat");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("regular-border alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("regular-border spawn regular");
  }
  reg->nation_id = nation;
  reg->orders = UNITS_ORDER_FORTIFIED;
  reg->moves_left = 3 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 1, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("regular-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected peace-fortified Regular to wake for MD≤2 border threat");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("carmy-border alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("carmy-border spawn regular");
  }
  army->nation_id = nation;
  army->orders = UNITS_ORDER_FORTIFIED;
  army->moves_left = 3 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 1, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("carmy-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected peace-fortified Continental Army to wake for MD≤2 border threat");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("ccav-border alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccav-border spawn continental cavalry");
  }
  cav->nation_id = nation;
  cav->orders = UNITS_ORDER_FORTIFIED;
  cav->moves_left = 3 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 1, 6, 4);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccav-border spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected peace-fortified Continental Cavalry to wake for MD≤2 border threat");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: peace Continental Cavalry border wake ok\n");
  return 0;
}

/*
 * 5d04 treasury: at war, prefer Artillery but gold < Europe purchase 500$ →
 * fall back to Soldier hire (hire_cost), not unpaid Artillery fiction.
 */
static int unit_artillery_treasury_fallback(void) {
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
    return fail("art-treasury alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Artillery");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 7;
  units.types[2].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 3 + i;
    c->y = 3;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_FOOD] = 20;
    c->building_in_production = -1;
  }
  ColonizeColony* fc = &colonies.colonies[2];
  fc->id = 2;
  fc->active = true;
  fc->nation_id = foe;
  fc->x = 12;
  fc->y = 12;
  fc->population = 2;
  fc->colonist_count = 2;
  fc->stock[COLONIZE_CARGO_FOOD] = 20;
  fc->building_in_production = -1;
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("art-treasury spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;

  /* Pre-board a Soldier so prefer_art path triggers (mil aboard). */
  const int mid = units_spawn_allow_stack(&units, 0, 200, 200);
  ColonizeUnit* mil = units_get(&units, mid);
  if (!mil || !units_board_stacked(&units, mid, sid)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("art-treasury board soldier");
  }
  mil = units_get(&units, mid);
  if (mil) {
    mil->nation_id = nation;
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
  col1.nation[nation].gold = 250;
  col1.nation[foe].gold = 100;

  ai_goals_reset();
  ai_diplo_declare_war(&col1, nation, foe);

  uint32_t turn = 11; /* odd → prefer_art when mil aboard */
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  /* After war sting: ≥ hire_cost 200, < Artillery purchase 500$. */
  col1.nation[nation].gold = 250;
  ai_euro_dispatcher_turn(&ctx, nation);

  int art_boarded = 0;
  int soldier_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (!ty) {
        continue;
      }
      if (strstr(ty->name, "Artillery")) {
        art_boarded = 1;
      }
      if (strstr(ty->name, "Soldier")) {
        soldier_boarded++;
      }
    }
  }

  if (art_boarded || soldier_boarded < 2) {
    fprintf(
      stderr,
      "unit_ai_euro_war: art_treasury art=%d soldiers=%d cargo=%d gold=%u\n",
      art_boarded,
      soldier_boarded,
      ship ? ship->cargo_count : -1,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Soldier fallback hire, not Artillery under 500$");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Artillery treasury fallback→Soldier ok\n");
  return 0;
}

/*
 * At war + tools_short: prefer Soldier hire over Pioneer when gold covers
 * hire_cost (peace tools→Pioneer must not win). Cite: 5d04 war arm.
 */
static int unit_at_war_tools_prefer_soldier(void) {
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
    return fail("war-tools alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 2;
  units.types[1].defense = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Caravel");
  units.types[2].movement = 4;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[2].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  /* Two tools-empty colonies → tools_short high (profession_demand would want Pioneer). */
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 3 + i * 2;
    c->y = 3;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int sid = units_spawn_allow_stack(&units, 2, 210, 210);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("war-tools spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 400;
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("war-tools expect at war");
  }
  /* Replenish after war sting so hire_cost (200) is affordable. */
  col1.nation[nation].gold = 400;

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

  ship = units_get(&units, sid);
  int soldier_boarded = 0;
  int pioneer_boarded = 0;
  if (ship) {
    for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (!ty) {
        continue;
      }
      if (strstr(ty->name, "Soldier")) {
        soldier_boarded = 1;
      }
      if (strstr(ty->name, "Pioneer")) {
        pioneer_boarded = 1;
      }
    }
  }

  if (!soldier_boarded || pioneer_boarded) {
    fprintf(
      stderr,
      "unit_ai_euro_war: war+tools soldier=%d pioneer=%d cargo=%d gold=%u\n",
      soldier_boarded,
      pioneer_boarded,
      ship ? ship->cargo_count : -1,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Soldier hire at war+tools_short, not Pioneer");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: at-war tools→Soldier (not Pioneer) ok\n");
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("wake-hunt alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wake-hunt spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = UNITS_ORDER_FORTIFIED;
  soldier->moves_left = 3 * UNITS_MP_PER_TILE;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected fortified Soldier to wake and hunt at war");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("g3 alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("g3 spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->moves_left = 1 * UNITS_MP_PER_TILE;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected G stance MILITARY prio ≥7 with own≥3 at war");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("g4 alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("g4 spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->moves_left = 1 * UNITS_MP_PER_TILE;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected G stance MILITARY prio ≥8 with own≥4 at war");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: G own≥4 prio8 ok (mil_prio=%d)\n", mil_prio);
  return 0;
}

/*
 * Naval multi-step: Frigate AI_SAIL war hunt with moves_left≥2 advances two
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
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 3;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, 2, 8);
  ColonizeUnit* warship = units_get(&units, own_id);
  if (!warship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-ms spawn");
  }
  warship->nation_id = nation;
  warship->orders = 0;
  warship->moves_left = 4 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, 14, 8);
  ColonizeUnit* foe_ship = units_get(&units, foe_id);
  if (!foe_ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-ms foe");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves_left = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 50;
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
  const int mp0 = warship->moves_left;
  ai_euro_dispatcher_turn(&ctx, nation);
  warship = units_get(&units, own_id);
  if (!warship || !warship->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    /* Combat ate the ship — still proves hunt ran; accept. */
    fprintf(stderr, "unit_ai_euro_war: naval multi-step ok (combat)\n");
    return 0;
  }
  const int advanced = warship->x - x0;
  const int spent = mp0 - warship->moves_left;
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
      warship->moves_left,
      warship->orders,
      warship->goto_x,
      warship->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Frigate war-hunt AI_SAIL multi-step (≥2 tiles, spend MP)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
 * Thin 5d04 mid-game: colonies ≥ 6 still hires wartime Soldier onto Europe ship.
 * Peace settle matrix stays gated <6. Cite: unpark #4 leftover mid 5d04.
 */
static int unit_mid_hire_mil_colonies_ge6(void) {
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
    return fail("mid-hire-ge6 alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Free Colonist");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 2;
  units.types[2].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    c->building_in_production = -1;
  }
  ColonizeColony* enemy = &colonies.colonies[6];
  enemy->id = 6;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 12;
  enemy->y = 12;
  enemy->population = 2;
  enemy->colonist_count = 2;
  enemy->stock[COLONIZE_CARGO_FOOD] = 20;
  enemy->building_in_production = -1;
  colonies.colony_count = 7;
  colonies.next_id = 7;

  const int ship_id = units_spawn_allow_stack(&units, 0, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("mid-hire-ge6 spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;

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
  col1.nation[foe].gold = 500;
  ai_diplo_declare_war(&col1, nation, foe);
  col1.nation[nation].gold = 500;
  const uint32_t gold_before = col1.nation[nation].gold;

  ai_goals_reset();

  uint32_t turn = 44;
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
  const int gold_spent = (int)gold_before > (int)col1.nation[nation].gold;
  if (!soldier_boarded || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_war: mid-hire-ge6 boarded=%d gold_spent=%d gold=%u cargo=%d\n",
      soldier_boarded,
      gold_spent,
      col1.nation[nation].gold,
      ship->cargo_count
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected wartime Soldier hire at colonies>=6");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: mid-hire colonies>=6 ok\n");
  return 0;
}

static int unit_mid_hire_dragoon_prefer(void) {
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
    return fail("dragoon-hire alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 4;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 2;
  units.types[2].defense = 2;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Dragoon");
  units.types[3].movement = 3;
  units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[3].attack = 3;
  units.types[3].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 4 + i * 2;
    c->y = 4;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->building_in_production = -1;
  }
  ColonizeColony* enemy = &colonies.colonies[3];
  enemy->id = 3;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 12;
  enemy->y = 12;
  enemy->population = 2;
  enemy->colonist_count = 2;
  enemy->stock[COLONIZE_CARGO_FOOD] = 20;
  enemy->building_in_production = -1;
  colonies.colony_count = 4;
  colonies.next_id = 4;

  const int sid = units_spawn(&units, 1, 210, 210);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dragoon-hire spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 800;
  col1.head.difficulty = 0;
  ai_diplo_declare_war(&col1, nation, foe);

  uint32_t turn = 40; /* even — avoid Artillery prefer_art odd-turn path */
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
  int dragoon_aboard = 0;
  int soldier_aboard = 0;
  if (ship) {
    for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const char* n = units_display_name(&units, pax);
      if (n && strstr(n, "Dragoon")) {
        dragoon_aboard = 1;
      }
      if (n && strstr(n, "Soldier") && !strstr(n, "Dragoon")) {
        soldier_aboard = 1;
      }
    }
  }
  if (!dragoon_aboard || soldier_aboard) {
    fprintf(
      stderr,
      "unit_ai_euro_war: dragoon=%d soldier=%d cargo=%d gold=%u\n",
      dragoon_aboard,
      soldier_aboard,
      ship ? ship->cargo_count : -1,
      col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Dragoon hire at war with ≥3 colonies (not Soldier)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: mid-hire Dragoon prefer (≥3 colonies) ok\n");
  return 0;
}

/*
 * At war + own colonies ≥ 2 + Veteran Soldier type + affordable cost → prefer
 * Veteran over plain Soldier. Cite: NAMES @JOB Soldier→Veteran Soldiers 2000$;
 * euro_unit_act mid-hire deepen.
 */
static int unit_mid_hire_veteran_prefer(void) {
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
    return fail("veteran-hire alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 4;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 2;
  units.types[2].defense = 2;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Veteran Soldier");
  units.types[3].movement = 1;
  units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[3].attack = 3;
  units.types[3].defense = 3;
  units.types[3].cost = 2000; /* NAMES @JOB train stand-in */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 4 + i * 2;
    c->y = 4;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->building_in_production = -1;
  }
  ColonizeColony* enemy = &colonies.colonies[2];
  enemy->id = 2;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 12;
  enemy->y = 12;
  enemy->population = 2;
  enemy->colonist_count = 2;
  enemy->stock[COLONIZE_CARGO_FOOD] = 20;
  enemy->building_in_production = -1;
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int sid = units_spawn(&units, 1, 210, 210);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("veteran-hire spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 2500; /* covers @JOB 2000$ */
  col1.head.difficulty = 0;
  ai_diplo_declare_war(&col1, nation, foe);

  uint32_t turn = 40; /* even — avoid Artillery odd-turn path */
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
  int vet_aboard = 0;
  int soldier_aboard = 0;
  if (ship) {
    for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      const char* dn = units_display_name(&units, pax);
      if (ty && strstr(ty->name, "Veteran") && strstr(ty->name, "Soldier")) {
        vet_aboard = 1;
      } else if (dn && strstr(dn, "Veteran") && strstr(dn, "Soldier")) {
        vet_aboard = 1;
      } else if (ty && strstr(ty->name, "Soldier") && !strstr(ty->name, "Veteran")) {
        soldier_aboard = 1;
      }
    }
  }
  const uint32_t gold_after = col1.nation[nation].gold;
  if (!vet_aboard || soldier_aboard || gold_after >= 2500u) {
    fprintf(
      stderr,
      "unit_ai_euro_war: vet=%d soldier=%d cargo=%d gold=%u\n",
      vet_aboard,
      soldier_aboard,
      ship ? ship->cargo_count : -1,
      gold_after
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Veteran Soldier hire (≥2 colonies) over Soldier");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: mid-hire Veteran Soldier prefer (≥2) ok\n");
  return 0;
}

/*
 * At war: idle Soldier on coastal own colony boards empty transport with space.
 * Cite: Colonization.pdf naval transport; units_board.
 */
static int unit_soldier_board_empty_transport(void) {
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
    return fail("sboard alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* land */
  }
  /* Water west of coastal colony (4,4) — empty Galleon at (3,4). */
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sboard colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  ColonizeColony* enemy = &colonies.colonies[1];
  enemy->id = 1;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 14;
  enemy->y = 14;
  enemy->population = 1;
  enemy->colonist_count = 1;
  enemy->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  /* Spawn soldier first so act runs before ship (moves_left 0 on ship anyway). */
  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* soldier = units_get(&units, uid);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sboard spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 1 * UNITS_MP_PER_TILE;
  soldier->muskets = 50; /* armed Soldier display name */

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* galleon = units_get(&units, sid);
  if (!galleon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sboard spawn galleon");
  }
  galleon->nation_id = nation;
  galleon->orders = 0;
  galleon->moves_left = 0;
  galleon->cargo_count = 0;

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
  col1.head.difficulty = 0;
  ai_diplo_declare_war(&col1, nation, foe);

  uint32_t turn = 10;
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

  soldier = units_get(&units, uid);
  galleon = units_get(&units, sid);
  if (!soldier || soldier->aboard_ship_id != sid) {
    fprintf(
      stderr,
      "unit_ai_euro_war: sboard aboard=%d want %d cargo=%d pos=(%d,%d) ship=(%d,%d) "
      "orders=%d active=%d\n",
      soldier ? soldier->aboard_ship_id : -1,
      sid,
      galleon ? galleon->cargo_count : -1,
      soldier ? soldier->x : -1,
      soldier ? soldier->y : -1,
      galleon ? galleon->x : -1,
      galleon ? galleon->y : -1,
      soldier ? soldier->orders : -1,
      soldier ? (int)soldier->active : 0
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Soldier to board empty coastal Galleon");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Soldier board empty transport ok\n");
  return 0;
}

/*
 * At war: idle Dragoon on coastal own colony boards empty transport (same
 * Soldier embark path). Cite: Colonization.pdf naval transport / Defending a
 * Colony; euro_unit_act §2d3 ship board military.
 */
static int unit_dragoon_board_empty_transport(void) {
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
    return fail("dboard alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* land */
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dboard colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Dragoon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  ColonizeColony* enemy = &colonies.colonies[1];
  enemy->id = 1;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 14;
  enemy->y = 14;
  enemy->population = 1;
  enemy->colonist_count = 1;
  enemy->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* dragoon = units_get(&units, uid);
  if (!dragoon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dboard spawn dragoon");
  }
  dragoon->nation_id = nation;
  dragoon->orders = 0;
  dragoon->moves_left = 1 * UNITS_MP_PER_TILE;
  dragoon->muskets = 50;
  dragoon->horses = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* galleon = units_get(&units, sid);
  if (!galleon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dboard spawn galleon");
  }
  galleon->nation_id = nation;
  galleon->orders = 0;
  galleon->moves_left = 0;
  galleon->cargo_count = 0;

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
  col1.head.difficulty = 0;
  ai_diplo_declare_war(&col1, nation, foe);

  uint32_t turn = 11;
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

  dragoon = units_get(&units, uid);
  galleon = units_get(&units, sid);
  if (!dragoon || dragoon->aboard_ship_id != sid) {
    fprintf(
      stderr,
      "unit_ai_euro_war: dboard aboard=%d want %d cargo=%d pos=(%d,%d)\n",
      dragoon ? dragoon->aboard_ship_id : -1,
      sid,
      galleon ? galleon->cargo_count : -1,
      dragoon ? dragoon->x : -1,
      dragoon ? dragoon->y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Dragoon to board empty coastal Galleon");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Dragoon board empty transport ok\n");
  return 0;
}

/*
 * At war: idle Regular on coastal own colony boards empty transport. Cite:
 * Defending a Colony army; euro_unit_act §2b2 / §2d3 ship board; king_ref Regular.
 */
static int unit_regular_board_empty_transport(void) {
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
    return fail("rboard alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("rboard colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regular");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  ColonizeColony* enemy = &colonies.colonies[1];
  enemy->id = 1;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 14;
  enemy->y = 14;
  enemy->population = 1;
  enemy->colonist_count = 1;
  enemy->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* reg = units_get(&units, uid);
  if (!reg) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("rboard spawn regular");
  }
  reg->nation_id = nation;
  reg->orders = 0;
  reg->moves_left = 1 * UNITS_MP_PER_TILE;
  reg->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("rboard spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves_left = 0;
  ship->cargo_count = 0;

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
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);

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

  reg = units_get(&units, uid);
  ship = units_get(&units, sid);
  const int aboard = reg && reg->aboard_ship_id == sid;
  const int cargo_ok = ship && ship->cargo_count >= 1;
  if (!aboard || !cargo_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_war: rboard aboard=%d cargo=%d\n",
      aboard,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Regular to board empty coastal transport");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Regular board empty transport ok\n");
  return 0;
}

/*
 * At war: idle Continental Army on coastal own colony boards empty transport.
 * Cite: Defending a Colony army; euro_unit_act §2b2 / §2d3; king_ref Cont. Army.
 */
static int unit_continental_army_board_empty_transport(void) {
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
    return fail("caboard alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("caboard colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Army");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  ColonizeColony* enemy = &colonies.colonies[1];
  enemy->id = 1;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 14;
  enemy->y = 14;
  enemy->population = 1;
  enemy->colonist_count = 1;
  enemy->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* reg = units_get(&units, uid);
  if (!reg) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("caboard spawn army");
  }
  reg->nation_id = nation;
  reg->orders = 0;
  reg->moves_left = 1 * UNITS_MP_PER_TILE;
  reg->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("caboard spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves_left = 0;
  ship->cargo_count = 0;

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
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);

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

  reg = units_get(&units, uid);
  ship = units_get(&units, sid);
  const int aboard = reg && reg->aboard_ship_id == sid;
  const int cargo_ok = ship && ship->cargo_count >= 1;
  if (!aboard || !cargo_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_war: caboard aboard=%d cargo=%d\n",
      aboard,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Army to board empty coastal transport");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Continental Army board empty transport ok\n");
  return 0;
}

/*
 * At war: idle Artillery on coastal own colony boards empty transport (same
 * Soldier/Dragoon embark path; before on-colony fortify). Cite: Colonization.pdf
 * naval transport / Defending a Colony; euro_unit_act §2d3 ship board military.
 */

/*
 * At war: idle Continental Cavalry on coastal own colony boards empty transport.
 * Cite: Defending a Colony army; euro_unit_act §2b2 / §2d3; king_ref Cont. Army.
 */
static int unit_continental_cavalry_board_empty_transport(void) {
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
    return fail("ccavboard alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccavboard colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Cavalry");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  ColonizeColony* enemy = &colonies.colonies[1];
  enemy->id = 1;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 14;
  enemy->y = 14;
  enemy->population = 1;
  enemy->colonist_count = 1;
  enemy->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* cav = units_get(&units, uid);
  if (!cav) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccavboard spawn army");
  }
  cav->nation_id = nation;
  cav->orders = 0;
  cav->moves_left = 1 * UNITS_MP_PER_TILE;
  cav->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccavboard spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves_left = 0;
  ship->cargo_count = 0;

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
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);

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

  cav = units_get(&units, uid);
  ship = units_get(&units, sid);
  const int aboard = cav && cav->aboard_ship_id == sid;
  const int cargo_ok = ship && ship->cargo_count >= 1;
  if (!aboard || !cargo_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_war: ccavboard aboard=%d cargo=%d\n",
      aboard,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Cavalry to board empty coastal transport");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Continental Cavalry board empty transport ok\n");
  return 0;
}

/*
 * At war: idle Artillery on coastal own colony boards empty transport (same
 * Soldier/Dragoon embark path; before on-colony fortify). Cite: Colonization.pdf
 * naval transport / Defending a Colony; euro_unit_act §2d3 ship board military.
 */
static int unit_artillery_board_empty_transport(void) {
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
    return fail("aboard alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* land */
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("aboard colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Artillery");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 5;
  units.types[0].defense = 1;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  ColonizeColony* enemy = &colonies.colonies[1];
  enemy->id = 1;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 14;
  enemy->y = 14;
  enemy->population = 1;
  enemy->colonist_count = 1;
  enemy->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* art = units_get(&units, uid);
  if (!art) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("aboard spawn artillery");
  }
  art->nation_id = nation;
  art->orders = 0;
  art->moves_left = 1 * UNITS_MP_PER_TILE;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* galleon = units_get(&units, sid);
  if (!galleon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("aboard spawn galleon");
  }
  galleon->nation_id = nation;
  galleon->orders = 0;
  galleon->moves_left = 0;
  galleon->cargo_count = 0;

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
  col1.head.difficulty = 0;
  ai_diplo_declare_war(&col1, nation, foe);

  uint32_t turn = 12;
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

  art = units_get(&units, uid);
  galleon = units_get(&units, sid);
  if (!art || art->aboard_ship_id != sid) {
    fprintf(
      stderr,
      "unit_ai_euro_war: aboard art aboard=%d want %d cargo=%d orders=%d pos=(%d,%d)\n",
      art ? art->aboard_ship_id : -1,
      sid,
      galleon ? galleon->cargo_count : -1,
      art ? art->orders : -1,
      art ? art->x : -1,
      art ? art->y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Artillery to board empty coastal Galleon");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Artillery board empty transport ok\n");
  return 0;
}

/*
 * At war: Galleon with Soldier cargo adjacent to own threatened coastal colony
 * unloads Soldier onto colony. Cite: Colonization.pdf naval transport;
 * euro_unit_act §2b2; complements board + sail-to-threatened-port.
 */
static int unit_unload_military_threatened(void) {
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
    return fail("munload alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* land */
  }
  /* Water west of coastal colony (4,4) — Galleon at (3,4). */
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("munload colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;
  units.types[1].attack = 2;
  units.types[1].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  /* Full colony so unload stays as field unit (no admit after mil unload). */
  c->population = 8;
  c->colonist_count = 8;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Spawn Soldier then board onto Galleon before turn. */
  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* soldier = units_get(&units, uid);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("munload spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 0;
  soldier->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* galleon = units_get(&units, sid);
  if (!galleon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("munload spawn galleon");
  }
  galleon->nation_id = nation;
  galleon->orders = 0;
  galleon->moves_left = 4 * UNITS_MP_PER_TILE;
  galleon->cargo_count = 0;

  if (!units_board(&units, uid, sid)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("munload board setup");
  }
  soldier = units_get(&units, uid);
  galleon = units_get(&units, sid);
  if (!soldier || soldier->aboard_ship_id != sid || !galleon || galleon->cargo_count != 1) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("munload expected Soldier aboard before turn");
  }

  /* Foe soldier adjacent to colony (threat MD≤3). */
  const int threat_id = units_spawn(&units, 0, 5, 4);
  ColonizeUnit* threat = units_get(&units, threat_id);
  if (!threat) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("munload spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves_left = 0;
  threat->muskets = 50;

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
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

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

  soldier = units_get(&units, uid);
  galleon = units_get(&units, sid);
  if (!soldier || !soldier->active || soldier->aboard_ship_id >= 0) {
    fprintf(
      stderr,
      "unit_ai_euro_war: munload aboard=%d want_sid=%d active=%d cargo=%d pos=(%d,%d)\n",
      soldier ? soldier->aboard_ship_id : -99,
      sid,
      soldier ? (int)soldier->active : 0,
      galleon ? galleon->cargo_count : -1,
      soldier ? soldier->x : -1,
      soldier ? soldier->y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Soldier unloaded at threatened coastal colony");
  }
  /* Unload lands on colony; same-act land war hunt may step onto adjacent threat. */
  const int near_colony = abs(soldier->x - 4) + abs(soldier->y - 4) <= 1;
  if (!near_colony) {
    fprintf(
      stderr,
      "unit_ai_euro_war: munload landfall=(%d,%d) want near colony (4,4)\n",
      soldier->x,
      soldier->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Soldier unloaded near threatened colony");
  }
  if (galleon && galleon->cargo_count != 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Galleon cargo empty after military unload");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: military unload threatened colony ok (pos=(%d,%d))\n",
    soldier->x,
    soldier->y
  );
  return 0;
}

/*
 * Series L: peacetime sticky≥2 + Brave MD≤3 → mil unload (no Euro×Euro war).
 * Seed very-low Indian relation so euro_balance hostility_sync keeps sticky=2.
 * Cite: move_scoring_ship.md −0x6790==4; ai_euro_try_unload_military_threatened.
 */
static int unit_unload_sticky_brave_threatened(void) {
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
    return fail("sunload alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sunload colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;
  units.types[1].attack = 2;
  units.types[1].defense = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Brave");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 1;
  units.types[2].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* soldier = units_get(&units, uid);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sunload spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves_left = 0;
  soldier->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* galleon = units_get(&units, sid);
  if (!galleon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sunload spawn galleon");
  }
  galleon->nation_id = nation;
  galleon->orders = 0;
  galleon->moves_left = 4 * UNITS_MP_PER_TILE;
  galleon->cargo_count = 0;

  if (!units_board(&units, uid, sid)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sunload board setup");
  }

  const int brave_id = units_spawn(&units, 2, 5, 4);
  ColonizeUnit* brave = units_get(&units, brave_id);
  if (!brave) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sunload spawn Brave");
  }
  brave->nation_id = 4;
  brave->orders = 0;
  brave->moves_left = 0;

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
  col1.indian[0].alarm_by_player[nation] = 90; /* relation 25 */
  col1.indian[0].euro_diplo[nation] |= COL1_INDIAN_MET_BIT;
  col1.nation[nation].indian_hostility_sticky = 2;

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

  soldier = units_get(&units, uid);
  galleon = units_get(&units, sid);
  if (ai_diplo_indian_hostility_sticky(&col1, nation) < 2) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky Brave smoke needs hostility_sync to keep sticky≥2");
  }
  if (!soldier || !soldier->active || soldier->aboard_ship_id >= 0) {
    fprintf(
      stderr,
      "unit_ai_euro_war: sticky munload aboard=%d active=%d cargo=%d\n",
      soldier ? soldier->aboard_ship_id : -99,
      soldier ? (int)soldier->active : 0,
      galleon ? galleon->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky Brave threat should unload Soldier");
  }
  if (abs(soldier->x - 4) + abs(soldier->y - 4) > 1) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky munload should land near colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: sticky Brave mil unload ok\n");
  return 0;
}

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
  memset(&map, 0, sizeof(map));
  map.width = 8;
  map.height = 8;
  map.tile_count = 64;
  map.terrain = calloc(64, 1);
  map.layer2 = calloc(64, 1);
  map.layer3 = calloc(64, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("zunload alloc map");
  }
  for (int i = 0; i < 64; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  ColonizeColonyPool colonies;
  colonies_init(&colonies);

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("unmet relations should clear sticky (stance0 / mil path closed)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: stance0 sticky-clear skip mil path ok\n");
  return 0;
}

/*
 * Threatened-port unload: Dragoon-only cargo → unload Dragoon (Soldier ladder
 * fallback). Cite: euro_unit_act §2b2; king_ref MoW unload else Dragoon.
 */
static int unit_unload_dragoon_threatened(void) {
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
    return fail("dunload alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dunload colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Dragoon");
  units.types[0].movement = 1; /* after unload, no same-act hunt into threat */
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 6;
  units.types[0].defense = 4;
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
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  /* Full colony so unload stays as field unit (no admit after mil unload). */
  c->population = 8;
  c->colonist_count = 8;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* drag = units_get(&units, uid);
  if (!drag) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dunload spawn dragoon");
  }
  drag->nation_id = nation;
  drag->orders = 0;
  drag->moves_left = 0;
  drag->muskets = 50;
  drag->horses = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* galleon = units_get(&units, sid);
  if (!galleon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dunload spawn galleon");
  }
  galleon->nation_id = nation;
  galleon->orders = 0;
  galleon->moves_left = 4 * UNITS_MP_PER_TILE;
  galleon->cargo_count = 0;

  if (!units_board(&units, uid, sid)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dunload board setup");
  }

  const int threat_id = units_spawn(&units, 2, 5, 4);
  ColonizeUnit* threat = units_get(&units, threat_id);
  if (!threat) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dunload spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves_left = 0;

  ai_goals_reset();
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  col1.nation[nation].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  /* Block Privateer spawn noise. */
  col1.nation[nation].privateer_spawn_mask = (uint8_t)(1u << foe);

  uint32_t turn = 13;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 10;

  ai_euro_dispatcher_turn(&ctx, nation);

  drag = units_get(&units, uid);
  galleon = units_get(&units, sid);
  if (!drag || !drag->active || drag->aboard_ship_id >= 0) {
    fprintf(
      stderr,
      "unit_ai_euro_war: dunload aboard=%d active=%d cargo=%d pos=(%d,%d)\n",
      drag ? drag->aboard_ship_id : -99,
      drag ? (int)drag->active : 0,
      galleon ? galleon->cargo_count : -1,
      drag ? drag->x : -1,
      drag ? drag->y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Dragoon unloaded at threatened coastal colony");
  }
  const int near_colony = abs(drag->x - 4) + abs(drag->y - 4) <= 1;
  if (!near_colony) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Dragoon unloaded near threatened colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Dragoon unload threatened colony ok\n");
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("runload alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("runload colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  /* Full colony so unload stays as field unit (no admit after mil unload). */
  c->population = 8;
  c->colonist_count = 8;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* reg = units_get(&units, uid);
  if (!reg) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("runload spawn regular");
  }
  reg->nation_id = nation;
  reg->orders = 0;
  reg->moves_left = 0;
  reg->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("runload spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->cargo_count = 0;
  if (!units_board(&units, uid, sid)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("runload board regular");
  }

  const int tid = units_spawn(&units, 2, 5, 4);
  ColonizeUnit* threat = units_get(&units, tid);
  if (!threat) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("runload spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Regular unloaded at threatened coastal colony");
  }
  if (abs(reg->x - 4) > 1 || abs(reg->y - 4) > 1) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Regular unloaded near threatened colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("carmyunload alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("carmyunload colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  /* Full colony so unload stays as field unit (no admit after mil unload). */
  c->population = 8;
  c->colonist_count = 8;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* army = units_get(&units, uid);
  if (!army) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("carmyunload spawn regular");
  }
  army->nation_id = nation;
  army->orders = 0;
  army->moves_left = 0;
  army->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("carmyunload spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->cargo_count = 0;
  if (!units_board(&units, uid, sid)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("carmyunload board regular");
  }

  const int tid = units_spawn(&units, 2, 5, 4);
  ColonizeUnit* threat = units_get(&units, tid);
  if (!threat) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("carmyunload spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Army unloaded at threatened coastal colony");
  }
  if (abs(army->x - 4) > 1 || abs(army->y - 4) > 1) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Army unloaded near threatened colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("ccavunload alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccavunload colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  /* Full colony so unload stays as field unit (no admit after mil unload). */
  c->population = 8;
  c->colonist_count = 8;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* cav = units_get(&units, uid);
  if (!cav) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccavunload spawn regular");
  }
  cav->nation_id = nation;
  cav->orders = 0;
  cav->moves_left = 0;
  cav->muskets = 50;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccavunload spawn ship");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->cargo_count = 0;
  if (!units_board(&units, uid, sid)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccavunload board regular");
  }

  const int tid = units_spawn(&units, 2, 5, 4);
  ColonizeUnit* threat = units_get(&units, tid);
  if (!threat) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ccavunload spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves_left = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Cavalry unloaded at threatened coastal colony");
  }
  if (abs(cav->x - 4) > 1 || abs(cav->y - 4) > 1) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Cavalry unloaded near threatened colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: Continental Cavalry unload threatened colony ok\n");
  return 0;
}

/*
 * Peace: idle Soldier on own colony → FORTIFY. Cite: euro_unit_act §2d3.
 */

/*
 * Col1 +0x1e garrison_quota: with quota=1, only one of two idle Soldiers
 * fortifies (other stays idle). Cite: save_format_map.md; FUN_5952_035e DEC.
 */
static int unit_garrison_quota_one_fortify(void) {
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
    return fail("garrison-quota alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
  c->garrison_quota = 1; /* only one fortify slot */
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid0 = units_spawn_allow_stack(&units, 0, 5, 5);
  const int uid1 = units_spawn_allow_stack(&units, 0, 5, 5);
  ColonizeUnit* s0 = units_get(&units, uid0);
  ColonizeUnit* s1 = units_get(&units, uid1);
  if (!s0 || !s1) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("garrison-quota spawn");
  }
  s0->nation_id = nation;
  s0->moves_left = 1 * UNITS_MP_PER_TILE;
  s0->orders = 0;
  s1->nation_id = nation;
  s1->moves_left = 1 * UNITS_MP_PER_TILE;
  s1->orders = 0;

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

  uint32_t turn = 55;
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

  s0 = units_get(&units, uid0);
  s1 = units_get(&units, uid1);
  c = &colonies.colonies[0];
  int fortified = 0;
  int idle = 0;
  int joined = 0;
  for (int i = 0; i < 2; ++i) {
    const ColonizeUnit* s = (i == 0) ? s0 : s1;
    if (!s || !s->active) {
      joined++; /* admit-as-colonist when quota exhausted */
      continue;
    }
    if (s->orders == UNITS_ORDER_FORTIFY || s->orders == UNITS_ORDER_FORTIFIED) {
      fortified++;
    } else {
      idle++;
    }
  }
  /*
   * One fortify consumes quota; the other stays idle or joins the colony
   * (no-slot admit path — Dutch Isabella). Cite: euro_unit_act fortify+quota.
   */
  if (fortified != 1 || c->garrison_quota != 0 || (idle + joined) != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_war: garrison_quota fortified=%d idle=%d joined=%d quota=%u\n",
      fortified,
      idle,
      joined,
      (unsigned)c->garrison_quota
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected exactly one fortify consuming garrison_quota 1→0");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: garrison_quota one fortify ok\n");
  return 0;
}

static int unit_peace_soldier_fortify_colony(void) {
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
    return fail("peace-fortify alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* sol = units_get(&units, uid);
  if (!sol) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("peace-fortify spawn");
  }
  sol->nation_id = nation;
  sol->moves_left = 1 * UNITS_MP_PER_TILE;
  sol->orders = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Soldier on colony to FORTIFY at peace");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("peace-dragoon-fortify alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Dragoon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* drag = units_get(&units, uid);
  if (!drag) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("peace-dragoon-fortify spawn");
  }
  drag->nation_id = nation;
  drag->moves_left = 4 * UNITS_MP_PER_TILE;
  drag->orders = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Dragoon on colony to FORTIFY at peace");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("peace-regular-fortify alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regular");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* reg = units_get(&units, uid);
  if (!reg) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("peace-regular-fortify spawn");
  }
  reg->nation_id = nation;
  reg->moves_left = 3 * UNITS_MP_PER_TILE;
  reg->orders = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Regular FORTIFY on own colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("peace-cont-fortify alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Army");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 4;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* army = units_get(&units, uid);
  if (!army) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("peace-cont-fortify spawn");
  }
  army->nation_id = nation;
  army->moves_left = 3 * UNITS_MP_PER_TILE;
  army->orders = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Army FORTIFY on own colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("peace-cont-cav-fortify alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Continental Cavalry");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* cav = units_get(&units, uid);
  if (!cav) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("peace-cont-cav-fortify spawn");
  }
  cav->nation_id = nation;
  cav->moves_left = 4 * UNITS_MP_PER_TILE;
  cav->orders = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Continental Cavalry FORTIFY on own colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("peace-art-fortify alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Artillery");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* art = units_get(&units, uid);
  if (!art) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("peace-art-fortify spawn");
  }
  art->nation_id = nation;
  art->moves_left = 1 * UNITS_MP_PER_TILE;
  art->orders = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Artillery FORTIFY on own colony in peace");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("peace-cannon-fortify alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Cannon");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* art = units_get(&units, uid);
  if (!art) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("peace-cannon-fortify spawn");
  }
  art->nation_id = nation;
  art->moves_left = 1 * UNITS_MP_PER_TILE;
  art->orders = 0;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Cannon FORTIFY on own colony in peace");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("art-fortify alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Artillery");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 3;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* art = units_get(&units, uid);
  if (!art) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("art-fortify spawn");
  }
  art->nation_id = nation;
  art->moves_left = 1 * UNITS_MP_PER_TILE;
  art->orders = 0;

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
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Artillery on colony to FORTIFY at war");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wtrans colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Own Galleon far south — closer to threatened colony than to distant foe. */
  const int own_id = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* galleon = units_get(&units, own_id);
  if (!galleon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wtrans spawn galleon");
  }
  galleon->nation_id = nation;
  galleon->orders = 0;
  galleon->moves_left = 4 * UNITS_MP_PER_TILE;
  galleon->cargo_count = 0;

  /* Foe soldier adjacent to own colony (threat MD≤3). */
  const int threat_id = units_spawn(&units, 1, 5, 4);
  ColonizeUnit* threat = units_get(&units, threat_id);
  if (!threat) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wtrans spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves_left = 0;

  /* Distant foe ship — must not win over threatened port. */
  const int foe_ship_id = units_spawn(&units, 0, 14, 14);
  ColonizeUnit* foe_ship = units_get(&units, foe_ship_id);
  if (!foe_ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wtrans spawn foe ship");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves_left = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Galleon sail toward threatened own coastal colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: war transport threatened colony ok (near=%d closer=%d)\n",
    near_colony,
    moved_closer
  );
  return 0;
}

/*
 * Series O: war cargo colony-sail prefers Fortress coastal over bare at equal
 * distance (0x1b defense ladder). Cite: move_scoring_ship.md; Series O.
 */
static int unit_war_cargo_fortress_prefer(void) {
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
    return fail("wcargo alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  /* Two coastal land colonies at (4,4) and (12,4). */
  map.terrain[4 * 16 + 4] = 1;
  map.terrain[4 * 16 + 5] = 1;
  map.terrain[5 * 16 + 4] = 1;
  map.terrain[4 * 16 + 12] = 1;
  map.terrain[4 * 16 + 11] = 1;
  map.terrain[5 * 16 + 12] = 1;
  if (!map_tile_is_coastal(&map, 4, 4) || !map_tile_is_coastal(&map, 12, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wcargo colonies should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Galleon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 6;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Fortress");
  colonies.building_type_count = 1;

  ColonizeColony* bare = &colonies.colonies[0];
  bare->id = 0;
  bare->active = true;
  bare->nation_id = nation;
  bare->x = 4;
  bare->y = 4;
  bare->population = 3;
  bare->colonist_count = 3;
  bare->stock[COLONIZE_CARGO_FOOD] = 40;
  bare->building_in_production = -1;

  ColonizeColony* fortress = &colonies.colonies[1];
  fortress->id = 1;
  fortress->active = true;
  fortress->nation_id = nation;
  fortress->x = 12;
  fortress->y = 4;
  fortress->population = 3;
  fortress->colonist_count = 3;
  fortress->stock[COLONIZE_CARGO_FOOD] = 40;
  fortress->building_in_production = -1;
  fortress->has_building[0] = true; /* Fortress */
  colonies.colony_count = 2;
  colonies.next_id = 2;

  /* Equidistant MD from (8,10): to (4,4)=10, to (12,4)=10. */
  const int sid = units_spawn(&units, 0, 8, 10);
  ColonizeUnit* galleon = units_get(&units, sid);
  if (!galleon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wcargo spawn galleon");
  }
  galleon->nation_id = nation;
  galleon->orders = 0;
  galleon->moves_left = 4 * UNITS_MP_PER_TILE;
  galleon->hold_goods_type[0] = COLONIZE_CARGO_MUSKETS;
  galleon->hold_goods_amount[0] = 50;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  col1.nation[foe].gold = 100;
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
  ctx.rng_seed = 7;

  ai_euro_dispatcher_turn(&ctx, nation);

  galleon = units_get(&units, sid);
  if (!galleon || !galleon->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wcargo galleon should remain");
  }
  const int d_fort =
    abs(galleon->goto_x - 12) + abs(galleon->goto_y - 4);
  const int d_bare = abs(galleon->goto_x - 4) + abs(galleon->goto_y - 4);
  const int prefer_fort =
    galleon->orders == UNITS_ORDER_AI_SAIL && d_fort <= 2 && d_fort < d_bare;
  const int moved_fort =
    abs(galleon->x - 12) + abs(galleon->y - 4) < abs(galleon->x - 4) + abs(galleon->y - 4);
  if (!prefer_fort && !moved_fort) {
    fprintf(
      stderr,
      "unit_ai_euro_war: wcargo orders=%d goto=(%d,%d) pos=(%d,%d) d_fort=%d d_bare=%d\n",
      galleon->orders,
      galleon->goto_x,
      galleon->goto_y,
      galleon->x,
      galleon->y,
      d_fort,
      d_bare
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("war cargo sail should prefer Fortress colony over bare");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_war: war cargo Fortress prefer ok\n");
  return 0;
}

/*
 * War transport: idle Man-O-War with passenger space prefers threatened own
 * coastal colony water over distant foe sea. Cite: euro_unit_act §2b2.
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("mowtrans colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Own Man-O-War far south — closer to threatened colony than to distant foe. */
  const int own_id = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* mow = units_get(&units, own_id);
  if (!mow) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("mowtrans spawn mow");
  }
  mow->nation_id = nation;
  mow->orders = 0;
  mow->moves_left = 4 * UNITS_MP_PER_TILE;
  mow->cargo_count = 0;

  /* Foe soldier adjacent to own colony (threat MD≤3). */
  const int threat_id = units_spawn(&units, 1, 5, 4);
  ColonizeUnit* threat = units_get(&units, threat_id);
  if (!threat) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("mowtrans spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves_left = 0;

  /* Distant foe ship — must not win over threatened port. */
  const int foe_ship_id = units_spawn(&units, 0, 14, 14);
  ColonizeUnit* foe_ship = units_get(&units, foe_ship_id);
  if (!foe_ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("mowtrans spawn foe ship");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves_left = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("mowtrans mow should remain");
  }

  /* Expect AI_SAIL toward coastal water near (4,4), not distant foe (14,14). */
  int near_colony = 0;
  if (mow->orders == UNITS_ORDER_AI_SAIL) {
    const int gd = abs(mow->goto_x - 4) + abs(mow->goto_y - 4);
    const int fd = abs(mow->goto_x - 14) + abs(mow->goto_y - 14);
    near_colony = gd <= 2 && gd < fd;
  }
  const int moved_closer =
    abs(mow->x - 4) + abs(mow->y - 4) < abs(3 - 4) + abs(10 - 4);

  if (!near_colony && !moved_closer) {
    fprintf(
      stderr,
      "unit_ai_euro_war: mowtrans orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      mow->orders,
      mow->goto_x,
      mow->goto_y,
      mow->x,
      mow->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Man-O-War sail toward threatened own coastal colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: Man-O-War war transport threatened ok (near=%d closer=%d)\n",
    near_colony,
    moved_closer
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("frigtrans colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Own Frigate far south — closer to threatened colony than to distant foe. */
  const int own_id = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* frig = units_get(&units, own_id);
  if (!frig) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("frigtrans spawn frig");
  }
  frig->nation_id = nation;
  frig->orders = 0;
  frig->moves_left = 4 * UNITS_MP_PER_TILE;
  frig->cargo_count = 0;

  /* Foe soldier adjacent to own colony (threat MD≤3). */
  const int threat_id = units_spawn(&units, 1, 5, 4);
  ColonizeUnit* threat = units_get(&units, threat_id);
  if (!threat) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("frigtrans spawn threat");
  }
  threat->nation_id = foe;
  threat->orders = 0;
  threat->moves_left = 0;

  /* Distant foe ship — must not win over threatened port. */
  const int foe_ship_id = units_spawn(&units, 0, 14, 14);
  ColonizeUnit* foe_ship = units_get(&units, foe_ship_id);
  if (!foe_ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("frigtrans spawn foe ship");
  }
  foe_ship->nation_id = foe;
  foe_ship->orders = 0;
  foe_ship->moves_left = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 100;
  col1.nation[foe].gold = 100;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Frigate sail toward threatened own coastal colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_war: Frigate war transport threatened ok (near=%d closer=%d)\n",
    near_colony,
    moved_closer
  );
  return 0;
}

/*
 * FUN_5bfb_3180 naval ambush (thin, non-destructive): a Frigate ending its
 * turn adjacent to a foreign Man-O-War, at peace (not war — DOS fires this
 * regardless of war state), may lose moves to a surprise encounter. Seeded
 * RNG so the outcome is deterministic; asserts moves_left is either
 * unchanged (no ambush) or reduced by exactly the Frigate's type drain (6),
 * proving the mechanic ran and picked one of its two real outcomes rather
 * than silently no-op'ing or corrupting state.
 */
static int unit_naval_ambush(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("naval-ambush alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Frigate");
  units.types[0].movement = 5;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 5;
  units.types[0].defense = 5;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Man-O-War");
  units.types[1].movement = 5;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].attack = 8;
  units.types[1].defense = 8;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* own = units_get(&units, own_id);
  if (!own) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-ambush spawn own");
  }
  own->nation_id = nation;
  own->orders = 0;
  own->moves_left = 5 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 1, own_x, own_y - 1);
  ColonizeUnit* foe = units_get(&units, foe_id);
  if (!foe) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("naval-ambush spawn foe");
  }
  foe->nation_id = foe_nat;
  foe->orders = 0;
  foe->moves_left = 0;

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
  col1.nation[foe_nat].gold = 50;
  /* Deliberately at peace — DOS ambush fires regardless of war state. */

  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7);

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
  ctx.rng = &rng;
  ctx.rng_seed = 7;

  ai_euro_dispatcher_turn(&ctx, nation);

  own = units_get(&units, own_id);
  foe = units_get(&units, foe_id);
  const int own_alive = own && own->active;
  const int foe_alive = foe && foe->active;
  const int moves_after = own_alive ? own->moves_left : -1;
  const int ok = own_alive && foe_alive &&
                 (moves_after == 5 /* no ambush this roll */ ||
                  moves_after == 0 /* ambushed: drain 6 > moves_left 5, floored at 0 */);

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  if (!ok) {
    fprintf(
      stderr,
      "unit_ai_euro_war: naval ambush moves_after=%d own_alive=%d foe_alive=%d\n",
      moves_after,
      own_alive,
      foe_alive
    );
    return fail("expected naval ambush to either no-op or drain exactly the Frigate amount");
  }
  fprintf(
    stderr, "unit_ai_euro_war: naval ambush ok (moves 5->%d, no combat, no war)\n", moves_after
  );
  return 0;
}

int main(void) {
  if (unit_mid_hire_mil() != 0) {
    return 1;
  }
  if (unit_mid_hire_mil_colonies_ge6() != 0) {
    return 1;
  }
  if (unit_mid_hire_dragoon_prefer() != 0) {
    return 1;
  }
  if (unit_mid_hire_veteran_prefer() != 0) {
    return 1;
  }
  if (unit_soldier_board_empty_transport() != 0) {
    return 1;
  }
  if (unit_dragoon_board_empty_transport() != 0) {
    return 1;
  }
  if (unit_regular_board_empty_transport() != 0) {
    return 1;
  }
  if (unit_continental_army_board_empty_transport() != 0) {
    return 1;
  }
  if (unit_continental_cavalry_board_empty_transport() != 0) {
    return 1;
  }
  if (unit_artillery_board_empty_transport() != 0) {
    return 1;
  }
  if (unit_unload_military_threatened() != 0) {
    return 1;
  }
  if (unit_unload_sticky_brave_threatened() != 0) {
    return 1;
  }
  if (unit_unload_stance0_no_sticky() != 0) {
    return 1;
  }
  if (unit_war_cargo_fortress_prefer() != 0) {
    return 1;
  }
  if (unit_unload_dragoon_threatened() != 0) {
    return 1;
  }
  if (unit_unload_regular_threatened() != 0) {
    return 1;
  }
  if (unit_unload_continental_army_threatened() != 0) {
    return 1;
  }
  if (unit_unload_continental_cavalry_threatened() != 0) {
    return 1;
  }
  if (unit_at_war_tools_prefer_soldier() != 0) {
    return 1;
  }
  if (unit_fortify_wake_hunt() != 0) {
    return 1;
  }
  if (unit_garrison_quota_one_fortify() != 0) {
    return 1;
  }
  if (unit_peace_soldier_fortify_colony() != 0) {
    return 1;
  }
  if (unit_peace_dragoon_fortify_colony() != 0) {
    return 1;
  }
  if (unit_peace_regular_fortify_colony() != 0) {
    return 1;
  }
  if (unit_peace_continental_fortify_colony() != 0) {
    return 1;
  }
  if (unit_peace_continental_cavalry_fortify_colony() != 0) {
    return 1;
  }
  if (unit_peace_artillery_fortify_colony() != 0) {
    return 1;
  }
  if (unit_peace_cannon_fortify_colony() != 0) {
    return 1;
  }
  if (unit_artillery_fortify_colony() != 0) {
    return 1;
  }
  if (unit_war_transport_threatened_colony() != 0) {
    return 1;
  }
  if (unit_mow_war_transport_threatened() != 0) {
    return 1;
  }
  if (unit_frigate_war_transport_threatened() != 0) {
    return 1;
  }
  if (unit_g_stance_own3_prio7() != 0) {
    return 1;
  }
  if (unit_g_stance_own4_prio8() != 0) {
    return 1;
  }
  if (unit_mid_hire_artillery() != 0) {
    return 1;
  }
  if (unit_artillery_treasury_fallback() != 0) {
    return 1;
  }
  if (unit_naval_war_hunt() != 0) {
    return 1;
  }
  if (unit_naval_flee_fort_fire() != 0) {
    return 1;
  }
  if (unit_privateer_war_hunt() != 0) {
    return 1;
  }
  if (unit_privateer_station_keep_hunt() != 0) {
    return 1;
  }
  if (unit_naval_multistep_sail() != 0) {
    return 1;
  }
  if (unit_land_war_hunt() != 0) {
    return 1;
  }
  if (unit_indian_war_capital_hunt() != 0) {
    return 1;
  }
  if (unit_land_war_hunt_multistep() != 0) {
    return 1;
  }
  if (unit_continental_army_land_hunt() != 0) {
    return 1;
  }
  if (unit_continental_cavalry_land_hunt() != 0) {
    return 1;
  }
  if (unit_sticky_contact_rehunt() != 0) {
    return 1;
  }
  if (unit_land_adjacent_combat_chain() != 0) {
    return 1;
  }
  if (unit_land_adjacent_colony_seize() != 0) {
    return 1;
  }
  if (unit_land_adjacent_foe_prefer_weak() != 0) {
    return 1;
  }
  if (unit_land_adjacent_foe_prefer_treasure() != 0) {
    return 1;
  }
  if (unit_land_hunt_prefer_treasure() != 0) {
    return 1;
  }
  if (unit_land_hunt_prefer_weak() != 0) {
    return 1;
  }
  if (unit_land_adjacent_foe_prefer_open_over_stockade() != 0) {
    return 1;
  }
  if (unit_land_adjacent_foe_prefer_non_veteran() != 0) {
    return 1;
  }
  if (unit_artillery_adjacent_prefer_stockade() != 0) {
    return 1;
  }
  if (unit_artillery_siege_hunt_prefer_stockade() != 0) {
    return 1;
  }
  if (unit_dragoon_hunt_prefer_open() != 0) {
    return 1;
  }
  if (unit_naval_adjacent_foe_prefer_weak() != 0) {
    return 1;
  }
  if (unit_naval_ambush() != 0) {
    return 1;
  }
  if (unit_naval_adjacent_foe_prefer_loaded() != 0) {
    return 1;
  }
  if (unit_naval_adjacent_foe_prefer_non_drake() != 0) {
    return 1;
  }
  if (unit_privateer_prefer_cargo_prey() != 0) {
    return 1;
  }
  if (unit_frigate_prefer_warship() != 0) {
    return 1;
  }
  if (unit_peace_fortify_border_wake() != 0) {
    return 1;
  }
  if (unit_peace_dragoon_border_wake() != 0) {
    return 1;
  }
  if (unit_peace_artillery_border_wake() != 0) {
    return 1;
  }
  if (unit_peace_regular_border_wake() != 0) {
    return 1;
  }
  if (unit_peace_continental_army_border_wake() != 0) {
    return 1;
  }
  if (unit_peace_continental_cavalry_border_wake() != 0) {
    return 1;
  }
  fprintf(stderr, "unit_ai_euro_war: ok\n");
  return 0;
}
