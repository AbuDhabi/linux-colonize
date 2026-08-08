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
  if (mil_prio >= 7) {
    fprintf(stderr, "smoke_ai_euro_war: G stance mil_prio=%d (own=2 should be 6 not 7)\n", mil_prio);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("own=2 must not take own≥3 deepen prio 7");
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

/*
 * Privateer hunt: at war, named Privateer with a prior west-explore sail goto
 * re-aims AI_SAIL toward enemy sea (commerce raid). Cite: euro_unit_act §2b;
 * europe Privateer; fandom Drake Privateer.
 */
static int smoke_privateer_war_hunt(void) {
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
  priv->moves_left = 4;

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
      "smoke_ai_euro_war: privateer orders=%d goto=(%d,%d) pos=(%d,%d)\n",
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
      "smoke_ai_euro_war: privateer multi-step mp %d→%d pos=(%d,%d) goto=(%d,%d)\n",
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
    "smoke_ai_euro_war: privateer hunt ok (hunt=%d closer=%d combat=%d mp_spent=%d)\n",
    hunt,
    moved_closer,
    combat_done,
    spent
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

/*
 * Thin 20e6 land adjacent-foe pick: Soldier between fortified high-defense foe
 * (N) and weak Free Colonist (S). Prefer the weaker/non-fortified target.
 * Old first-dir scan would hit N first.
 */
static int smoke_land_adjacent_foe_prefer_weak(void) {
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
  soldier->moves_left = 1; /* one adjacent fight only — no re-hunt onto fortified */

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

  const int weak_dead = weak == NULL || !weak->active;
  const int strong_alive = strong && strong->active;
  const int own_alive = soldier && soldier->active;

  if (!weak_dead || !strong_alive || !own_alive) {
    fprintf(
      stderr,
      "smoke_ai_euro_war: adj-foe own=%d weak_dead=%d strong_alive=%d\n",
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
  fprintf(stderr, "smoke_ai_euro_war: adjacent-foe prefer-weak ok\n");
  return 0;
}

/*
 * Thin 20e6 naval adjacent-foe pick: Man-O-War between high-defense foe (N) and
 * weak Caravel (S). Prefer weaker defense (old first-dir scan would hit N).
 * Cite: FUN_521d_20e6 naval combat thin; damage mods PARKED (no damage byte).
 */
static int smoke_naval_adjacent_foe_prefer_weak(void) {
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
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].attack = 0;
  units.types[1].defense = 1;

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
  own->moves_left = 1;

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
      "smoke_ai_euro_war: naval-adj own=%d weak_dead=%d strong_alive=%d\n",
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
  fprintf(stderr, "smoke_ai_euro_war: naval adjacent-foe prefer-weak ok\n");
  return 0;
}

/*
 * Privateer cargo prey deepen: adjacent Frigate (lower defense) vs Merchantman
 * (higher defense) → prefer Merchantman/Caravel cargo over warship.
 * Cite: euro_unit_act §2f; Europe Privateer commerce raid.
 */
static int smoke_privateer_prefer_cargo_prey(void) {
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
  own->moves_left = 1;

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

  if (!merch_dead || !frig_alive || !own_alive) {
    fprintf(
      stderr,
      "smoke_ai_euro_war: priv-cargo own=%d merch_dead=%d frig_alive=%d\n",
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
  fprintf(stderr, "smoke_ai_euro_war: privateer prefer cargo prey ok\n");
  return 0;
}

/*
 * Frigate warship hunt deepen: adjacent Merchantman (lower defense) vs Privateer
 * (higher defense) → prefer warship over cargo (complement Privateer cargo prey).
 * Cite: euro_unit_act §2f; Europe Frigate purchase.
 */
static int smoke_frigate_prefer_warship(void) {
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
  own->moves_left = 1;

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

  if (!priv_dead || !merch_alive || !own_alive) {
    fprintf(
      stderr,
      "smoke_ai_euro_war: frig-war own=%d priv_dead=%d merch_alive=%d\n",
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
  fprintf(stderr, "smoke_ai_euro_war: frigate prefer warship ok\n");
  return 0;
}

/*
 * Peace fortify Soldier on colony wakes when foreign Euro land unit enters MD≤2.
 * Cite: euro_unit_act §2d3 peace colony-defense wake; units_wake.
 */
static int smoke_peace_fortify_border_wake(void) {
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
  soldier->moves_left = 3;

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
    fprintf(stderr, "smoke_ai_euro_war: peace-border wake ok (combat despawn)\n");
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
      "smoke_ai_euro_war: peace-border orders=%d goto=(%d,%d) pos=(%d,%d)\n",
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
  fprintf(stderr, "smoke_ai_euro_war: peace fortify border wake ok\n");
  return 0;
}

/*
 * Peace fortified Dragoon on colony wakes when foreign Euro land unit enters
 * MD≤2 (same as Soldier). Cite: Colonization.pdf Defending a Colony (fortify
 * soldiers, dragoons…); euro_unit_act §2d3; units_wake.
 */
static int smoke_peace_dragoon_border_wake(void) {
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
  dragoon->moves_left = 4;

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
    fprintf(stderr, "smoke_ai_euro_war: dragoon-border wake ok (combat despawn)\n");
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
      "smoke_ai_euro_war: dragoon-border orders=%d goto=(%d,%d) pos=(%d,%d)\n",
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
  fprintf(stderr, "smoke_ai_euro_war: peace Dragoon border wake ok\n");
  return 0;
}

/*
 * 5d04 treasury: at war, prefer Artillery but gold < Europe purchase 500$ →
 * fall back to Soldier hire (hire_cost), not unpaid Artillery fiction.
 */
static int smoke_artillery_treasury_fallback(void) {
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
      "smoke_ai_euro_war: art_treasury art=%d soldiers=%d cargo=%d gold=%u\n",
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
  fprintf(stderr, "smoke_ai_euro_war: Artillery treasury fallback→Soldier ok\n");
  return 0;
}

/*
 * At war + tools_short: prefer Soldier hire over Pioneer when gold covers
 * hire_cost (peace tools→Pioneer must not win). Cite: 5d04 war arm.
 */
static int smoke_at_war_tools_prefer_soldier(void) {
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
      "smoke_ai_euro_war: war+tools soldier=%d pioneer=%d cargo=%d gold=%u\n",
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
  fprintf(stderr, "smoke_ai_euro_war: at-war tools→Soldier (not Pioneer) ok\n");
  return 0;
}

/*
 * Idle fortified Soldier at war → wake (clear fortify) and hunt toward foe.
 * Cite: units_wake; euro_unit_act §2c sentry/fortify wake.
 */
static int smoke_fortify_wake_hunt(void) {
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
  soldier->moves_left = 3;

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
    fprintf(stderr, "smoke_ai_euro_war: fortify-wake ok (combat despawn)\n");
    return 0;
  }

  const int woken = soldier->orders != UNITS_ORDER_FORTIFIED &&
                    soldier->orders != UNITS_ORDER_FORTIFY;
  const int hunting =
    units_orders_follow_goto(soldier->orders) || soldier->x != x0;
  if (!woken || !hunting) {
    fprintf(
      stderr,
      "smoke_ai_euro_war: wake orders=%d goto=(%d,%d) pos=(%d,%d) x0=%d\n",
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
  fprintf(stderr, "smoke_ai_euro_war: fortify-wake hunt ok\n");
  return 0;
}

/*
 * G stance deepen: own≥3 colonies + at war → MILITARY primary prio 7
 * (stand-in for −0x6790; no invented gold). Cite: euro_dispatcher G / decomp.
 */
static int smoke_g_stance_own3_prio7(void) {
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
  soldier->moves_left = 1;
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
    fprintf(stderr, "smoke_ai_euro_war: G own≥3 mil_prio=%d (want ≥7)\n", mil_prio);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected G stance MILITARY prio ≥7 with own≥3 at war");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_war: G own≥3 prio7 ok (mil_prio=%d)\n", mil_prio);
  return 0;
}

/*
 * Naval multi-step: Frigate AI_SAIL war hunt with moves_left≥2 advances two
 * scored ocean steps in one act (mirror land 2-step) and spends remaining MP.
 * Cite: euro_unit_act §2c4 / §2b Frigate war hunt.
 */
static int smoke_naval_multistep_sail(void) {
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
  warship->moves_left = 4;

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
    fprintf(stderr, "smoke_ai_euro_war: naval multi-step ok (combat)\n");
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
      "smoke_ai_euro_war: naval multi-step x %d→%d mp %d→%d orders=%d goto=(%d,%d)\n",
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
    "smoke_ai_euro_war: Frigate war-hunt multi-step ok (x %d→%d mp spent %d)\n",
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
static int smoke_mid_hire_dragoon_prefer(void) {
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
      "smoke_ai_euro_war: dragoon=%d soldier=%d cargo=%d gold=%u\n",
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
  fprintf(stderr, "smoke_ai_euro_war: mid-hire Dragoon prefer (≥3 colonies) ok\n");
  return 0;
}

/*
 * At war + own colonies ≥ 2 + Veteran Soldier type + affordable cost → prefer
 * Veteran over plain Soldier. Cite: NAMES @JOB Soldier→Veteran Soldiers 2000$;
 * euro_unit_act mid-hire deepen.
 */
static int smoke_mid_hire_veteran_prefer(void) {
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
      "smoke_ai_euro_war: vet=%d soldier=%d cargo=%d gold=%u\n",
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
  fprintf(stderr, "smoke_ai_euro_war: mid-hire Veteran Soldier prefer (≥2) ok\n");
  return 0;
}

/*
 * At war: idle Soldier on coastal own colony boards empty transport with space.
 * Cite: Colonization.pdf naval transport; units_board.
 */
static int smoke_soldier_board_empty_transport(void) {
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
  soldier->moves_left = 1;
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
      "smoke_ai_euro_war: sboard aboard=%d want %d cargo=%d pos=(%d,%d) ship=(%d,%d) "
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
  fprintf(stderr, "smoke_ai_euro_war: Soldier board empty transport ok\n");
  return 0;
}

/*
 * At war: idle Dragoon on coastal own colony boards empty transport (same
 * Soldier embark path). Cite: Colonization.pdf naval transport / Defending a
 * Colony; euro_unit_act §2d3 ship board military.
 */
static int smoke_dragoon_board_empty_transport(void) {
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
  dragoon->moves_left = 1;
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
      "smoke_ai_euro_war: dboard aboard=%d want %d cargo=%d pos=(%d,%d)\n",
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
  fprintf(stderr, "smoke_ai_euro_war: Dragoon board empty transport ok\n");
  return 0;
}

/*
 * At war: idle Artillery on coastal own colony boards empty transport (same
 * Soldier/Dragoon embark path; before on-colony fortify). Cite: Colonization.pdf
 * naval transport / Defending a Colony; euro_unit_act §2d3 ship board military.
 */
static int smoke_artillery_board_empty_transport(void) {
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
  art->moves_left = 1;

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
      "smoke_ai_euro_war: aboard art aboard=%d want %d cargo=%d orders=%d pos=(%d,%d)\n",
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
  fprintf(stderr, "smoke_ai_euro_war: Artillery board empty transport ok\n");
  return 0;
}

/*
 * At war: Galleon with Soldier cargo adjacent to own threatened coastal colony
 * unloads Soldier onto colony. Cite: Colonization.pdf naval transport;
 * euro_unit_act §2b2; complements board + sail-to-threatened-port.
 */
static int smoke_unload_military_threatened(void) {
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
  c->population = 2;
  c->colonist_count = 2;
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
  galleon->moves_left = 4;
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
      "smoke_ai_euro_war: munload aboard=%d want_sid=%d active=%d cargo=%d pos=(%d,%d)\n",
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
      "smoke_ai_euro_war: munload landfall=(%d,%d) want near colony (4,4)\n",
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
    "smoke_ai_euro_war: military unload threatened colony ok (pos=(%d,%d))\n",
    soldier->x,
    soldier->y
  );
  return 0;
}

/*
 * Peace: idle Soldier on own colony → FORTIFY. Cite: euro_unit_act §2d3.
 */
static int smoke_peace_soldier_fortify_colony(void) {
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
  sol->moves_left = 1;
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
      "smoke_ai_euro_war: peace-fortify orders=%d pos=(%d,%d)\n",
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
  fprintf(stderr, "smoke_ai_euro_war: peace soldier fortify colony ok\n");
  return 0;
}

/*
 * Artillery fortify after siege: idle Artillery on own colony at war → FORTIFY.
 * Cite: euro_unit_act §2d3; Colonization.pdf fortify defense.
 */
static int smoke_artillery_fortify_colony(void) {
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
  art->moves_left = 1;
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
      "smoke_ai_euro_war: art-fortify orders=%d pos=(%d,%d)\n",
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
  fprintf(stderr, "smoke_ai_euro_war: artillery fortify colony ok\n");
  return 0;
}

/*
 * War transport: idle Galleon with passenger space prefers threatened own
 * coastal colony water over distant foe sea. Cite: euro_unit_act §2b2.
 */
static int smoke_war_transport_threatened_colony(void) {
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
  galleon->moves_left = 4;
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
      "smoke_ai_euro_war: wtrans orders=%d goto=(%d,%d) pos=(%d,%d)\n",
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
    "smoke_ai_euro_war: war transport threatened colony ok (near=%d closer=%d)\n",
    near_colony,
    moved_closer
  );
  return 0;
}

int main(void) {
  if (smoke_mid_hire_mil() != 0) {
    return 1;
  }
  if (smoke_mid_hire_dragoon_prefer() != 0) {
    return 1;
  }
  if (smoke_mid_hire_veteran_prefer() != 0) {
    return 1;
  }
  if (smoke_soldier_board_empty_transport() != 0) {
    return 1;
  }
  if (smoke_dragoon_board_empty_transport() != 0) {
    return 1;
  }
  if (smoke_artillery_board_empty_transport() != 0) {
    return 1;
  }
  if (smoke_unload_military_threatened() != 0) {
    return 1;
  }
  if (smoke_at_war_tools_prefer_soldier() != 0) {
    return 1;
  }
  if (smoke_fortify_wake_hunt() != 0) {
    return 1;
  }
  if (smoke_peace_soldier_fortify_colony() != 0) {
    return 1;
  }
  if (smoke_artillery_fortify_colony() != 0) {
    return 1;
  }
  if (smoke_war_transport_threatened_colony() != 0) {
    return 1;
  }
  if (smoke_g_stance_own3_prio7() != 0) {
    return 1;
  }
  if (smoke_mid_hire_artillery() != 0) {
    return 1;
  }
  if (smoke_artillery_treasury_fallback() != 0) {
    return 1;
  }
  if (smoke_naval_war_hunt() != 0) {
    return 1;
  }
  if (smoke_privateer_war_hunt() != 0) {
    return 1;
  }
  if (smoke_naval_multistep_sail() != 0) {
    return 1;
  }
  if (smoke_land_war_hunt() != 0) {
    return 1;
  }
  if (smoke_sticky_contact_rehunt() != 0) {
    return 1;
  }
  if (smoke_land_adjacent_foe_prefer_weak() != 0) {
    return 1;
  }
  if (smoke_naval_adjacent_foe_prefer_weak() != 0) {
    return 1;
  }
  if (smoke_privateer_prefer_cargo_prey() != 0) {
    return 1;
  }
  if (smoke_frigate_prefer_warship() != 0) {
    return 1;
  }
  if (smoke_peace_fortify_border_wake() != 0) {
    return 1;
  }
  if (smoke_peace_dragoon_border_wake() != 0) {
    return 1;
  }
  fprintf(stderr, "smoke_ai_euro_war: ok\n");
  return 0;
}
