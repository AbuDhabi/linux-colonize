/* Smoke: at-war Euro mid-hire / MILITARY bind + G stance + thin naval hunt. */
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_euro_war: FAIL %s\n", msg);
  return 1;
}

static int smoke_mid_hire_mil(void) {
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
  soldier->moves_left = 1;
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
    fprintf(stderr, "smoke_ai_euro_war: G stance mil_prio=%d (want ≥6)\n", mil_prio);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected G stance MILITARY prio ≥6 with own≥2 at war");
  }

  if (!mil_goto && !(soldier_boarded && gold_spent)) {
    fprintf(
      stderr,
      "smoke_ai_euro_war: mil_goto=%d boarded=%d gold %u→%u orders=%d goto=(%d,%d)\n",
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
    "smoke_ai_euro_war: mid-hire ok (mil_goto=%d boarded=%d gold_spent=%d mil_prio=%d)\n",
    mil_goto,
    soldier_boarded,
    gold_spent,
    mil_prio
  );
  return 0;
}

/* Two nations at war, idle ocean ships — expect AI_SAIL toward foe / closer / combat. */
static int smoke_naval_war_hunt(void) {
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
  warship->moves_left = 4;

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
      "smoke_ai_euro_war: naval orders=%d goto=(%d,%d) pos=(%d,%d) foe_active=%d\n",
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
    "smoke_ai_euro_war: naval ok (sail=%d closer=%d combat=%d)\n",
    sail_toward,
    moved_closer,
    combat_done
  );
  return 0;
}

/* Two nations at war, idle land soldiers — expect AI_MOVE toward foe / closer / combat. */
static int smoke_land_war_hunt(void) {
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
  soldier->moves_left = 1;

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
      "smoke_ai_euro_war: land orders=%d goto=(%d,%d) pos=(%d,%d) foe_active=%d\n",
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
    "smoke_ai_euro_war: land ok (move=%d closer=%d combat=%d)\n",
    move_toward,
    moved_closer,
    combat_done
  );
  return 0;
}

/*
 * Sticky CONTACT re-hunt: fortified Soldier (hunter adjacent-attack skipped)
 * with moves left next to a war foe — sticky still try_attacks.
 */
static int smoke_sticky_contact_rehunt(void) {
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
  soldier->moves_left = 2;

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
      "smoke_ai_euro_war: sticky soldier_active=%d foe_active=%d moves=%d orders=%d\n",
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
  fprintf(stderr, "smoke_ai_euro_war: sticky CONTACT re-hunt ok\n");
  return 0;
}

/*
 * Thin mid-hire Artillery: at war, colonies>=2, gold, Europe ship with Soldier
 * already aboard → prefer Artillery (Cannon name fallback). If Artillery/Cannon
 * type missing from pool, hire falls back to Soldier/Dragoon path (documented).
 */
static int smoke_mid_hire_artillery(void) {
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
      "smoke_ai_euro_war: artillery type missing — skip (Soldier path fallback)\n"
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
      "smoke_ai_euro_war: art_boarded=%d gold %u→%u cargo %d→%d\n",
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
    "smoke_ai_euro_war: artillery mid-hire ok (boarded=%d gold_spent=%d)\n",
    art_boarded,
    gold_spent
  );
  return 0;
}

int main(void) {
  if (smoke_mid_hire_mil() != 0) {
    return 1;
  }
  if (smoke_mid_hire_artillery() != 0) {
    return 1;
  }
  if (smoke_naval_war_hunt() != 0) {
    return 1;
  }
  if (smoke_land_war_hunt() != 0) {
    return 1;
  }
  if (smoke_sticky_contact_rehunt() != 0) {
    return 1;
  }
  fprintf(stderr, "smoke_ai_euro_war: ok\n");
  return 0;
}
