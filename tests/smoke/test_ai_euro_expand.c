/* Smoke: Euro second-wave settle + thin E peaceful Scout explore. */
#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_euro_expand: FAIL %s\n", msg);
  return 1;
}

static int count_nation_colonies(const ColonizeColonyPool* colonies, int nation_id) {
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (colonies->colonies[i].active && colonies->colonies[i].nation_id == nation_id) {
      ++n;
    }
  }
  return n;
}

/* Second-wave settle via ai_euro_dispatcher_turn (colony_count 1→2). */
static int smoke_second_wave(void) {
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
    return fail("alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
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

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  /* Existing first colony — second-wave path (colony_count == 1). */
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
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Idle pioneer near colony expand FOUND (pick_founding_tile → north (4,3)). */
  const int pid = units_spawn(&units, 0, 4, 5);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn pioneer");
  }
  pioneer->nation_id = nation;
  pioneer->moves_left = 3;
  pioneer->orders = 0;

  ai_goals_reset();

  uint32_t turn = 10;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1_ok = false;
  ctx.rng_seed = 42; /* not seed-100 fixture */

  const int max_turns = 24;
  for (int t = 0; t < max_turns; ++t) {
    units_end_turn(&units);
    ai_euro_dispatcher_turn(&ctx, nation);
    if (count_nation_colonies(&colonies, nation) >= 2) {
      break;
    }
  }

  const int final_n = count_nation_colonies(&colonies, nation);
  /* Accept second colony, or (fallback) pioneer parked on a foundable expand tile. */
  int ok = (final_n >= 2);
  if (!ok && pioneer->active &&
      colonies_can_found(&colonies, &map, pioneer->x, pioneer->y) &&
      (pioneer->x != 4 || pioneer->y != 5) && /* moved off spawn */
      (pioneer->x != c->x || pioneer->y != c->y)) {
    ok = 1;
  }
  if (!ok) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: colonies=%d pioneer=(%d,%d) active=%d\n",
      final_n,
      pioneer->active ? pioneer->x : -1,
      pioneer->active ? pioneer->y : -1,
      pioneer->active
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected colony_count>=2 or found-at-tile after move");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: second-wave ok (colonies=%d)\n", final_n);
  return 0;
}

/*
 * Thin E scout explore: peaceful nation with own≥1 colony + Scout + tribe;
 * dispatcher should give Scout AI_MOVE toward tribe / FOUND tile.
 */
static int smoke_scout_explore(void) {
  const int nation = 1;
  const int tribe_x = 12;
  const int tribe_y = 12;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("scout alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Scout");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* scout = units_get(&units, sid);
  if (!scout) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn scout");
  }
  scout->nation_id = nation;
  scout->moves_left = 4;
  scout->orders = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("alloc tribe");
  }
  col1.tribe[0].x = (uint8_t)tribe_x;
  col1.tribe[0].y = (uint8_t)tribe_y;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].population = 4;
  col1.tribe[0].mission = 0xff;

  ai_goals_reset();

  uint32_t turn = 15;
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

  /* Expected explore tile: founding-adjacent to tribe, else tribe xy. */
  int expect_x = tribe_x;
  int expect_y = tribe_y;
  int fx = 0;
  int fy = 0;
  if (ai_goals_pick_founding_tile(&map, &colonies, nation, tribe_x, tribe_y, &fx, &fy)) {
    expect_x = fx;
    expect_y = fy;
  }

  const int ok_move =
    scout->active && scout->orders == UNITS_ORDER_AI_MOVE &&
    scout->goto_x == expect_x && scout->goto_y == expect_y;
  /* Also accept if Scout already stepped closer while keeping that goto. */
  const int ok_closer =
    scout->active && scout->orders == UNITS_ORDER_AI_MOVE &&
    scout->goto_x == expect_x && scout->goto_y == expect_y &&
    (abs(scout->x - expect_x) + abs(scout->y - expect_y)) <
      (abs(5 - expect_x) + abs(5 - expect_y));

  if (!ok_move && !ok_closer) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: scout orders=%d goto=(%d,%d) pos=(%d,%d) expect=(%d,%d)\n",
      scout->orders,
      scout->goto_x,
      scout->goto_y,
      scout->x,
      scout->y,
      expect_x,
      expect_y
    );
    free(col1.tribe);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Scout AI_MOVE toward tribe/FOUND");
  }

  free(col1.tribe);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: scout-explore ok (goto=(%d,%d) pos=(%d,%d))\n",
    scout->goto_x,
    scout->goto_y,
    scout->x,
    scout->y
  );
  return 0;
}

int main(void) {
  if (smoke_second_wave() != 0) {
    return 1;
  }
  if (smoke_scout_explore() != 0) {
    return 1;
  }
  fprintf(stderr, "smoke_ai_euro_expand: ok\n");
  return 0;
}
