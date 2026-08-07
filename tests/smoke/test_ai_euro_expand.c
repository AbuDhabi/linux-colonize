/* Smoke: Euro second-wave settle + CONTACT scout rings + tools delivery. */
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
 * CONTACT scout rings (unpark #4): peaceful nation with own≥1 colony + Scout +
 * tribe beyond adjacent → upsert CONTACT at Manhattan ring 2–4 around tribe;
 * Scout AI_MOVE toward that tile. Deep fog rings PARKED.
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

  scout = units_get(&units, sid);
  int contact_x = -1;
  int contact_y = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (!g || g->code != AI_GOAL_CONTACT) {
      continue;
    }
    contact_x = (int)g->x;
    contact_y = (int)g->y;
    break;
  }

  const int ring_md =
    (contact_x >= 0) ? (abs(contact_x - tribe_x) + abs(contact_y - tribe_y)) : -1;
  const int ok_contact = contact_x >= 0 && ring_md >= 2 && ring_md <= 4;
  const int ok_move =
    scout && scout->active && scout->orders == UNITS_ORDER_AI_MOVE &&
    scout->goto_x == contact_x && scout->goto_y == contact_y;
  const int ok_closer =
    ok_move &&
    (abs(scout->x - contact_x) + abs(scout->y - contact_y)) <
      (abs(5 - contact_x) + abs(5 - contact_y));

  if (!ok_contact || (!ok_move && !ok_closer)) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: scout orders=%d goto=(%d,%d) pos=(%d,%d) "
      "CONTACT=(%d,%d) ring_md=%d\n",
      scout ? scout->orders : -1,
      scout ? scout->goto_x : -1,
      scout ? scout->goto_y : -1,
      scout ? scout->x : -1,
      scout ? scout->y : -1,
      contact_x,
      contact_y,
      ring_md
    );
    free(col1.tribe);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Scout AI_MOVE toward CONTACT ring (MD 2–4) around tribe");
  }

  free(col1.tribe);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: CONTACT scout-ring ok (goto=(%d,%d) pos=(%d,%d) ring_md=%d)\n",
    scout->goto_x,
    scout->goto_y,
    scout->x,
    scout->y,
    ring_md
  );
  return 0;
}

/*
 * Thin Pioneer tools delivery (case 7 economy stand-in): colony tools low,
 * Pioneer on colony tile; dispatcher/act should bump stock[TOOLS].
 */
static int smoke_pioneer_tools_delivery(void) {
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
    return fail("tools alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
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
  c->stock[COLONIZE_CARGO_TOOLS] = 5; /* < 20 → tools_short */
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int pid = units_spawn(&units, 0, 4, 4); /* on colony tile */
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn pioneer on colony");
  }
  pioneer->nation_id = nation;
  pioneer->moves_left = 3;
  pioneer->orders = 0;

  ai_goals_reset();

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1_ok = false;
  ctx.rng_seed = 42; /* not seed-100 fixture */

  const int tools_before = c->stock[COLONIZE_CARGO_TOOLS];
  ai_euro_dispatcher_turn(&ctx, nation);
  const int tools_after = c->stock[COLONIZE_CARGO_TOOLS];

  if (tools_after < tools_before + 10) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: tools before=%d after=%d (want +10)\n",
      tools_before,
      tools_after
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Pioneer tools delivery +10");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: tools-delivery ok (tools %d→%d)\n",
    tools_before,
    tools_after
  );
  return 0;
}

/*
 * Thin NEW WORLD tools-cargo hire stand-in (5d04 wagon matrix PARKED):
 * tools_short > 40 (3 colonies at tools=0 → short=60), peaceful Europe dock
 * with gold + free passenger slot → Pioneer hire; side effect is ship hold
 * +20 TOOLS and/or nearest-colony +15 tools stock.
 */
static int smoke_tools_cargo_hire(void) {
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
    return fail("tools-cargo alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2; /* passenger + goods hold slots */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  /* Three own colonies with tools=0 → inventory tools_short = 60 (>40). */
  static const int cx[3] = {4, 6, 8};
  static const int cy[3] = {4, 6, 8};
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = cx[i];
    c->y = cy[i];
    c->population = 3;
    c->colonist_count = 3;
    c->stock[COLONIZE_CARGO_TOOLS] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int tools0_before = colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS];
  const int tools1_before = colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS];
  const int tools2_before = colonies.colonies[2].stock[COLONIZE_CARGO_TOOLS];

  /* Europe-dock Caravel — hire path boards Pioneer + optional tools cargo. */
  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("tools-cargo spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0; /* stay docked; planning hire only */

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0; /* hire_cost = 200 */
  col1.nation[nation].gold = 500;

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
  ctx.rng_seed = 42; /* not seed-100 fixture */

  ai_euro_dispatcher_turn(&ctx, nation);

  int pioneer_boarded = 0;
  int pioneer_tools = 0;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Pioneer")) {
      pioneer_boarded = 1;
      pioneer_tools = pax->tools;
      break;
    }
  }

  int ship_tools = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] > 0 && ship->hold_goods_amount[h] < 255 &&
        ship->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
      ship_tools += ship->hold_goods_amount[h];
    }
  }

  const int colony_tools_rose =
    colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] >= tools0_before + 15 ||
    colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS] >= tools1_before + 15 ||
    colonies.colonies[2].stock[COLONIZE_CARGO_TOOLS] >= tools2_before + 15;

  /* Side effect: ship TOOLS hold and/or nearest-colony +15; Pioneer carries tools. */
  const int ok_side =
    pioneer_boarded &&
    (ship_tools >= 20 || colony_tools_rose || pioneer_tools >= UNITS_EQUIP_TOOLS_STEP);

  if (!ok_side) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: tools-cargo boarded=%d pax_tools=%d ship_tools=%d "
      "colony_tools=%d/%d/%d gold=%u\n",
      pioneer_boarded,
      pioneer_tools,
      ship_tools,
      colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS],
      colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS],
      colonies.colonies[2].stock[COLONIZE_CARGO_TOOLS],
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Pioneer hire with tools cargo / colony +15 stand-in");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: tools-cargo hire ok (boarded=%d ship_tools=%d "
    "colony=%d/%d/%d pax_tools=%d)\n",
    pioneer_boarded,
    ship_tools,
    colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS],
    colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS],
    colonies.colonies[2].stock[COLONIZE_CARGO_TOOLS],
    pioneer_tools
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
  if (smoke_pioneer_tools_delivery() != 0) {
    return 1;
  }
  if (smoke_tools_cargo_hire() != 0) {
    return 1;
  }
  fprintf(stderr, "smoke_ai_euro_expand: ok\n");
  return 0;
}
