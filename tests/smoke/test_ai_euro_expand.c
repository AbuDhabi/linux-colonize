/* Smoke: Euro second-wave settle + CONTACT scout rings + tools delivery. */
#include "core/ai_diplo.h"
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
 * Second+ colony FOUND via 06ae: when colony_count>=1, prefer coastal foundable
 * over richer inland (river) tile. Cite: ai_euro_pick_founding_tile coastal +6;
 * fandom Docks coastal gate.
 */
static int smoke_second_colony_coastal_prefer(void) {
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
    return fail("coastal-found alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  /* Ocean south of (8,9) → (8,9) coastal; inland north (8,7) gets river (+3).
   * Dir0 scans north first — without coastal bias river would win; with bias
   * coastal south must beat it. */
  map.terrain[10 * 16 + 8] = 25; /* ocean at (8,10) */
  map.terrain[7 * 16 + 8] = (uint8_t)(1u | 0x40u); /* plains + minor river (8,7) */

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 8;
  c->y = 8;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int pid = units_spawn(&units, 0, 8, 8);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("coastal-found spawn pioneer");
  }
  pioneer->nation_id = nation;
  pioneer->moves_left = 0; /* plan only — inspect FOUND goal */
  pioneer->orders = 0;

  if (!map_tile_is_coastal(&map, 8, 9)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("coastal-found setup: (8,9) should be coastal");
  }
  if (map_tile_is_coastal(&map, 8, 7)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("coastal-found setup: (8,7) should be inland");
  }

  ai_goals_reset();

  uint32_t turn = 12;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1_ok = false;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  int found_x = -1;
  int found_y = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_FOUND) {
      found_x = (int)g->x;
      found_y = (int)g->y;
      break;
    }
  }

  if (found_x != 8 || found_y != 9) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: coastal FOUND got=(%d,%d) want=(8,9) "
      "coastal9=%d coastal7=%d river7=%d\n",
      found_x,
      found_y,
      map_tile_is_coastal(&map, 8, 9),
      map_tile_is_coastal(&map, 8, 7),
      map_tile_has_river(&map, 8, 7)
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected coastal FOUND (8,9) over inland river (8,7)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: second-colony coastal FOUND ok\n");
  return 0;
}

/*
 * CONTACT scout rings (unpark #4): peaceful nation with own≥1 colony + Scout +
 * tribe beyond adjacent → upsert CONTACT at Manhattan ring 2–4 around tribe;
 * Scout AI_MOVE toward that tile. Fog plane optional (prefer unseen when set).
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
 * Thin NEW WORLD tools-cargo hire stand-in (5d04 wagon matrix thin slice):
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

/*
 * tools_short == 40 (2 colonies tools=0): threshold lowered from >40 to >20 —
 * still prefer Pioneer + tools cargo / colony +15 (no Wagon type in pool).
 */
static int smoke_tools_mid_threshold_hire(void) {
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
    return fail("tools-mid alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
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
  units.types[1].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  static const int cx[2] = {4, 6};
  static const int cy[2] = {4, 6};
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = cx[i];
    c->y = cy[i];
    c->population = 3;
    c->colonist_count = 3;
    c->stock[COLONIZE_CARGO_TOOLS] = 0; /* short += 20 each → 40 */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int tools0_before = colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS];
  const int tools1_before = colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS];

  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("tools-mid spawn europe ship");
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
    colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS] >= tools1_before + 15;

  const int ok_side =
    pioneer_boarded &&
    (ship_tools >= 20 || colony_tools_rose || pioneer_tools >= UNITS_EQUIP_TOOLS_STEP);

  if (!ok_side) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: tools-mid boarded=%d pax_tools=%d ship_tools=%d "
      "colony_tools=%d/%d gold=%u\n",
      pioneer_boarded,
      pioneer_tools,
      ship_tools,
      colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS],
      colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS],
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Pioneer hire at tools_short==40 (>20) with tools cargo");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: tools-mid hire ok (boarded=%d ship_tools=%d "
    "colony=%d/%d pax_tools=%d)\n",
    pioneer_boarded,
    ship_tools,
    colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS],
    colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS],
    pioneer_tools
  );
  return 0;
}

/*
 * tools_short>30 + Wagon Train type → hire wagon once (TOOLS on wagon);
 * second planning pass with free cargo slot prefers Pioneer (not a 2nd wagon).
 */
static int smoke_wagon_hire_once(void) {
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
    return fail("wagon-hire alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
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
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Wagon Train");
  units.types[2].movement = 2;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].cargo = 2; /* goods holds for TOOLS load */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  static const int cx[2] = {4, 6};
  static const int cy[2] = {4, 6};
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = cx[i];
    c->y = cy[i];
    c->population = 3;
    c->colonist_count = 3;
    c->stock[COLONIZE_CARGO_TOOLS] = 0; /* short=40 >30 */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-hire spawn europe ship");
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
  col1.nation[nation].gold = 800;

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

  ai_euro_dispatcher_turn(&ctx, nation);

  int wagon_boarded = 0;
  int wagon_tools = 0;
  int wagon_uid = -1;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Wagon")) {
      wagon_boarded = 1;
      wagon_uid = pax->id;
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        if (pax->hold_goods_amount[h] > 0 && pax->hold_goods_amount[h] < 255 &&
            pax->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
          wagon_tools += pax->hold_goods_amount[h];
        }
      }
      break;
    }
  }

  if (!wagon_boarded || wagon_tools < 20) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: wagon first pass boarded=%d wagon_tools=%d "
      "cargo=%d gold=%u\n",
      wagon_boarded,
      wagon_tools,
      ship->cargo_count,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon Train hire once with TOOLS aboard wagon");
  }

  /* Second pass: wagon already owned → Pioneer, not a second wagon. */
  col1.nation[nation].gold = 800;
  turn = 27;
  /* Keep colonies tools=0 so tools_short stays high after inventory rebuild. */
  ai_euro_dispatcher_turn(&ctx, nation);

  int wagon_count = 0;
  int pioneer_boarded = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, u->type_index);
    if (!ty) {
      continue;
    }
    if (strstr(ty->name, "Wagon")) {
      wagon_count++;
    }
  }
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Pioneer")) {
      pioneer_boarded = 1;
      break;
    }
  }

  if (wagon_count != 1 || !pioneer_boarded) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: wagon once-guard wagons=%d pioneer=%d cargo=%d "
      "first_uid=%d gold=%u\n",
      wagon_count,
      pioneer_boarded,
      ship->cargo_count,
      wagon_uid,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected exactly one wagon then Pioneer on second hire");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: wagon-hire once ok (wagon_tools=%d then Pioneer)\n",
    wagon_tools
  );
  return 0;
}

/*
 * Fog-aware CONTACT rings: when map.seen exists, prefer an unseen ring tile
 * over a closer seen tile (FoW explore — map_tile_seen_by).
 */
static int smoke_scout_fog_prefer_unseen(void) {
  const int nation = 1;
  const int tribe_x = 10;
  const int tribe_y = 10;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  map.seen = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3 || !map.seen) {
    return fail("fog-scout alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  /* Mark toward-scout ring tiles (closer to scout at 5,5) as seen; leave
   * far-side ring tiles unseen so fog preference should pick those. */
  for (int dy = -4; dy <= 4; ++dy) {
    for (int dx = -4; dx <= 4; ++dx) {
      const int md = abs(dx) + abs(dy);
      if (md < 2 || md > 4) {
        continue;
      }
      const int nx = tribe_x + dx;
      const int ny = tribe_y + dy;
      if (nx < 0 || ny < 0 || nx >= 16 || ny >= 16) {
        continue;
      }
      const int to_scout = abs(nx - 5) + abs(ny - 5);
      if (to_scout <= 8) {
        map_reveal_tile(&map, nx, ny, nation);
      }
    }
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
    free(map.seen);
    return fail("fog-scout spawn");
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
    free(map.seen);
    return fail("fog-scout tribe");
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
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

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
  const int ok_ring = contact_x >= 0 && ring_md >= 2 && ring_md <= 4;
  const int ok_unseen =
    ok_ring && !map_tile_seen_by(&map, contact_x, contact_y, nation);

  if (!ok_ring || !ok_unseen) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: fog CONTACT=(%d,%d) ring_md=%d seen=%d\n",
      contact_x,
      contact_y,
      ring_md,
      ok_ring ? (int)map_tile_seen_by(&map, contact_x, contact_y, nation) : -1
    );
    free(col1.tribe);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("expected CONTACT ring on unseen FoW tile");
  }

  free(col1.tribe);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.seen);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: fog CONTACT prefer-unseen ok (goto=(%d,%d))\n",
    contact_x,
    contact_y
  );
  return 0;
}

/*
 * Thin multi-step land 20e6: Soldier with moves_left>=2 on MILITARY goto advances
 * two tiles in one dispatcher act when path is clear.
 */
static int smoke_multistep_military(void) {
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
    return fail("multistep alloc map");
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
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("multistep spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->moves_left = 3;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("multistep soldier inactive");
  }
  const int advanced = soldier->x - x0;
  if (advanced < 2) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: multistep x %d→%d (want ≥2) orders=%d goto=(%d,%d)\n",
      x0,
      soldier->x,
      soldier->orders,
      soldier->goto_x,
      soldier->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected MILITARY multi-step advance of 2 tiles");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: MILITARY multi-step ok (x %d→%d)\n",
    x0,
    soldier->x
  );
  return 0;
}

/*
 * Case-7 dock expert once: peace + tools_short high + Europe dock has
 * Hardy Pioneers → board that type (consume dock); do not invent if absent.
 */
static int smoke_dock_expert_hire(void) {
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
    return fail("dock-hire alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Hardy Pioneer");
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-hire spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Hardy Pioneers");
  europe.dock[0].profession = 20;
  europe.dock[0].present = true;
  europe.dock[0].sentry = true;

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
  ctx.europe = &europe;
  ctx.rng_seed = 42;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int hardy_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Hardy Pioneer")) {
        hardy_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!hardy_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: dock hardy=%d dock_count=%d gold %u→%u cargo=%d\n",
      hardy_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Hardy Pioneer dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: dock Hardy Pioneer hire ok\n");
  return 0;
}

/*
 * 5d04 treasury gate: gold below colonist hire_cost → no Europe hire / tools-cargo.
 */
static int smoke_treasury_skip_hire(void) {
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
    return fail("treasury alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
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
  units.types[1].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasury spawn ship");
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
  col1.head.difficulty = 0; /* hire_cost=200; bump≈30 → still <200 if gold=0 */
  col1.nation[nation].gold = 0;

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
      "smoke_ai_euro_expand: treasury cargo=%d gold=%u\n",
      ship->cargo_count,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected no Europe hire when gold < hire_cost");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: treasury skip-hire ok\n");
  return 0;
}

/*
 * LABOR bind: idle Free Colonist adjacent to own colony with food_short
 * (and a distant FOUND lure) → LABOR goto / join, not yank to FOUND.
 * Cite: 5b66 unload/labor + 5cf6 food_short; no invented production.
 */
static int smoke_labor_bind_food_short(void) {
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
    return fail("labor-bind alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
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
  c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short vs pop*2 */
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 5, 4);
  ColonizeUnit* col = units_get(&units, uid);
  if (!col) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("labor-bind spawn colonist");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves_left = 1;

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
      "smoke_ai_euro_expand: labor joined=%d at_col=%d goto=(%d,%d) pop %d→%d\n",
      joined,
      at_colony,
      col ? col->goto_x : -1,
      col ? col->goto_y : -1,
      pop_before,
      c->population
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected LABOR bind toward food-short colony, not FOUND yank");
  }
  (void)labor_goal;

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: LABOR bind food-short ok\n");
  return 0;
}

/*
 * Wagon hire-once deepen: Wagon Train on tools-short colony with TOOLS hold
 * → colonies_transfer_from_unit into stock (no invented +10).
 */
static int smoke_wagon_tools_delivery(void) {
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
    return fail("wagon-tools alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

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
  c->stock[COLONIZE_CARGO_TOOLS] = 5;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-tools spawn");
  }
  wagon->nation_id = nation;
  wagon->orders = 0;
  wagon->moves_left = 2;
  wagon->hold_goods_type[0] = COLONIZE_CARGO_TOOLS;
  wagon->hold_goods_amount[0] = 20;

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

  uint32_t turn = 21;
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

  const int tools_before = c->stock[COLONIZE_CARGO_TOOLS];
  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  c = &colonies.colonies[0];
  const int tools_after = c->stock[COLONIZE_CARGO_TOOLS];
  int hold_left = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < 2; ++h) {
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
        hold_left += wagon->hold_goods_amount[h];
      }
    }
  }

  if (tools_after < tools_before + 20 || hold_left != 0) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: wagon-tools %d→%d hold_left=%d (want +20, hold 0)\n",
      tools_before,
      tools_after,
      hold_left
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected wagon TOOLS transfer into short colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: wagon-tools delivery ok (tools %d→%d)\n",
    tools_before,
    tools_after
  );
  return 0;
}

/*
 * Construction hammers bind: idle Pioneer on own colony with Stockade in
 * production stays/LABOR-joins rather than leave for distant FOUND.
 * Cite: building_production.md Stockade; euro_unit_act §2e.
 */
static int smoke_construction_labor_stockade(void) {
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
    return fail("construction alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40; /* no tools-delivery lure */
  c->building_in_production = 0; /* Stockade */
  c->hammers = 10;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* pioneer = units_get(&units, uid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("construction spawn pioneer");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves_left = 3;

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
  col1.nation[nation].gold = 200;

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

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  pioneer = units_get(&units, uid);
  const int joined = (pioneer == NULL || !pioneer->active) && c->population > pop0;
  const int labor_stay =
    pioneer && pioneer->active && pioneer->x == 4 && pioneer->y == 4 &&
    (pioneer->orders == UNITS_ORDER_AI_MOVE || pioneer->orders == 0) &&
    ((pioneer->goto_x == 4 && pioneer->goto_y == 4) ||
     ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR) >= 4);
  const int left_for_found =
    pioneer && pioneer->active && (pioneer->x != 4 || pioneer->y != 4 ||
                                   (pioneer->goto_x == 12 && pioneer->goto_y == 12));

  if (left_for_found || (!joined && !labor_stay)) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: construction joined=%d labor_stay=%d left=%d "
      "pop %d→%d active=%d pos=(%d,%d) goto=(%d,%d) orders=%d labor_prio=%d\n",
      joined,
      labor_stay,
      left_for_found,
      pop0,
      c->population,
      pioneer && pioneer->active,
      pioneer ? pioneer->x : -1,
      pioneer ? pioneer->y : -1,
      pioneer ? pioneer->goto_x : -1,
      pioneer ? pioneer->goto_y : -1,
      pioneer ? pioneer->orders : -1,
      ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR)
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Pioneer stay/LABOR for Stockade construction");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: construction Stockade LABOR ok (joined=%d)\n",
    joined
  );
  return 0;
}

/*
 * Master Carpenter construction LABOR: idle Master Carpenter on colony with
 * incomplete Stockade → stay/join LABOR (Stockade pattern). Cite: euro_unit_act
 * §2e; docs/building_production.md Carpenter→Hammers; Skills Chart.
 */
static int smoke_master_carpenter_construction_labor(void) {
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
    return fail("carpenter-labor alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Carpenter");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = 0; /* Stockade incomplete */
  c->hammers = 10;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* carpenter = units_get(&units, uid);
  if (!carpenter) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("carpenter-labor spawn");
  }
  carpenter->nation_id = nation;
  carpenter->orders = 0;
  carpenter->moves_left = 3;
  carpenter->profession = 13; /* @JOB Carpenter */

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
  col1.nation[nation].gold = 200;

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

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  carpenter = units_get(&units, uid);
  const int joined = (carpenter == NULL || !carpenter->active) && c->population > pop0;
  const int labor_stay =
    carpenter && carpenter->active && carpenter->x == 4 && carpenter->y == 4 &&
    (carpenter->orders == UNITS_ORDER_AI_MOVE || carpenter->orders == 0) &&
    ((carpenter->goto_x == 4 && carpenter->goto_y == 4) ||
     ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR) >= 4);
  const int left_for_found =
    carpenter && carpenter->active &&
    (carpenter->x != 4 || carpenter->y != 4 ||
     (carpenter->goto_x == 12 && carpenter->goto_y == 12));

  if (left_for_found || (!joined && !labor_stay)) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: carpenter joined=%d labor_stay=%d left=%d "
      "pop %d→%d active=%d pos=(%d,%d) goto=(%d,%d) orders=%d labor_prio=%d\n",
      joined,
      labor_stay,
      left_for_found,
      pop0,
      c->population,
      carpenter && carpenter->active,
      carpenter ? carpenter->x : -1,
      carpenter ? carpenter->y : -1,
      carpenter ? carpenter->goto_x : -1,
      carpenter ? carpenter->goto_y : -1,
      carpenter ? carpenter->orders : -1,
      ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR)
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Carpenter stay/LABOR for Stockade construction");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: Master Carpenter construction LABOR ok (joined=%d)\n",
    joined
  );
  return 0;
}

/*
 * Expert Lumberjack LABOR: idle Expert Lumberjack on colony with incomplete
 * Warehouse (building type exists) → stay/join LABOR. Field-assign PARKED.
 * Cite: euro_unit_act §2e; docs/building_production.md Lumberjack→Lumber;
 * Colonization.pdf Skills Chart.
 */
static int smoke_lumberjack_warehouse_labor(void) {
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
    return fail("lumberjack-labor alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Lumberjack");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Warehouse");
  colonies.building_types[0].hammers = 80;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = 0; /* Warehouse incomplete */
  c->hammers = 10;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* lumber = units_get(&units, uid);
  if (!lumber) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("lumberjack-labor spawn");
  }
  lumber->nation_id = nation;
  lumber->orders = 0;
  lumber->moves_left = 3;
  lumber->profession = 5; /* @JOB Lumberjack */

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
  col1.nation[nation].gold = 200;

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 11;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  lumber = units_get(&units, uid);
  const int joined = (lumber == NULL || !lumber->active) && c->population > pop0;
  const int labor_stay =
    lumber && lumber->active && lumber->x == 4 && lumber->y == 4 &&
    (lumber->orders == UNITS_ORDER_AI_MOVE || lumber->orders == 0) &&
    ((lumber->goto_x == 4 && lumber->goto_y == 4) ||
     ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR) >= 4);
  const int left_for_found =
    lumber && lumber->active &&
    (lumber->x != 4 || lumber->y != 4 ||
     (lumber->goto_x == 12 && lumber->goto_y == 12));

  if (left_for_found || (!joined && !labor_stay)) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: lumberjack joined=%d labor_stay=%d left=%d "
      "active=%d pop %d→%d orders=%d goto=(%d,%d) labor_prio=%d\n",
      joined,
      labor_stay,
      left_for_found,
      lumber ? (int)lumber->active : 0,
      pop0,
      c->population,
      lumber ? lumber->orders : -1,
      lumber ? lumber->goto_x : -1,
      lumber ? lumber->goto_y : -1,
      ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR)
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Lumberjack stay/LABOR for Warehouse");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: Lumberjack Warehouse LABOR ok (joined=%d)\n",
    joined
  );
  return 0;
}

/*
 * Threatened Stockade: idle Free Colonist within MD≤3 of own colony with
 * incomplete Stockade + war-peer threat prefers LABOR over distant FOUND.
 * Cite: building_production.md Stockade; Colonization.pdf fortify defense.
 */
static int smoke_stockade_threat_labor(void) {
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
    return fail("threat-stockade alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldier");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 2;
  units.types[1].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = 0; /* Stockade incomplete */
  c->hammers = 10;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Free Colonist at MD 2 — beyond adjacent LABOR, within threat Stockade MD≤3. */
  const int uid = units_spawn(&units, 0, 6, 4);
  ColonizeUnit* colonist = units_get(&units, uid);
  if (!colonist) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("threat-stockade spawn colonist");
  }
  colonist->nation_id = nation;
  colonist->orders = 0;
  colonist->moves_left = 3;

  /* Foe soldier MD≤3 from colony (threat). */
  const int fid = units_spawn(&units, 1, 4, 6);
  ColonizeUnit* foe_u = units_get(&units, fid);
  if (!foe_u) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("threat-stockade spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves_left = 0;
  foe_u->muskets = 50;

  ai_goals_reset();
  /* Distant FOUND lure — founder scan prefers FOUND unless threat LABOR overrides. */
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
  col1.head.difficulty = 0;
  ai_diplo_declare_war(&col1, nation, foe);

  uint32_t turn = 5;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 3;

  const int pop_before = c->colonist_count;
  ai_euro_dispatcher_turn(&ctx, nation);

  int labor = 0;
  int labor_prio = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_LABOR && g->x == 4 && g->y == 4) {
      labor = 1;
      labor_prio = (int)g->prio;
      break;
    }
  }

  colonist = units_get(&units, uid);
  int toward = 0;
  int yanked_found = 0;
  if (colonist && colonist->active) {
    toward =
      (colonist->orders == UNITS_ORDER_AI_MOVE && colonist->goto_x == 4 &&
       colonist->goto_y == 4) ||
      (colonist->x == 4 && colonist->y == 4) ||
      (abs(colonist->x - 4) + abs(colonist->y - 4) < 2);
    yanked_found =
      colonist->orders == UNITS_ORDER_AI_MOVE && colonist->goto_x == 12 &&
      colonist->goto_y == 12;
  }
  const int joined = colonies.colonies[0].colonist_count > pop_before;

  /* Threat bump (≥6) + (goto/closer/join) and not FOUND-yank. */
  if (!labor || labor_prio < 6 || yanked_found || (!toward && !joined)) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: threat-stockade labor=%d prio=%d toward=%d joined=%d "
      "found_yank=%d pos=(%d,%d) orders=%d goto=(%d,%d)\n",
      labor,
      labor_prio,
      toward,
      joined,
      yanked_found,
      colonist ? colonist->x : -1,
      colonist ? colonist->y : -1,
      colonist ? colonist->orders : -1,
      colonist ? colonist->goto_x : -1,
      colonist ? colonist->goto_y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Free Colonist LABOR toward threatened Stockade, not FOUND");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: threatened Stockade LABOR ok (prio=%d toward=%d joined=%d)\n",
    labor_prio,
    toward,
    joined
  );
  return 0;
}

/*
 * Sticky≥2 CONTACT rings: prefer closer MD (weight) around tribe.
 * Scout placed so md=4 is nearer scout than md=2 without sticky weight.
 * Cite: ai_diplo_indian_hostility_sticky / euro_diplo.md unknown26[8].
 */
static int smoke_scout_sticky_closer_ring(void) {
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
    return fail("sticky-ring alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
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
  c->x = 2;
  c->y = 2;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* On the tribe column north — md=4 tile (12,8) is nearer than md=2 (12,10). */
  const int sid = units_spawn(&units, 0, 12, 8);
  ColonizeUnit* scout = units_get(&units, sid);
  if (!scout) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky-ring spawn scout");
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
  col1.nation[nation].unknown26[8] = 2; /* sticky very-low deepen */
  if (ai_diplo_indian_hostility_sticky(&col1, nation) < 2) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky-ring expected sticky≥2");
  }
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky-ring alloc tribe");
  }
  col1.tribe[0].x = (uint8_t)tribe_x;
  col1.tribe[0].y = (uint8_t)tribe_y;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].population = 4;
  col1.tribe[0].mission = 0xff;

  ai_goals_reset();

  uint32_t turn = 16;
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

  int contact_x = -1;
  int contact_y = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (!g || g->code != AI_GOAL_CONTACT) {
      continue;
    }
    contact_x = g->x;
    contact_y = g->y;
    break;
  }
  free(col1.tribe);
  if (contact_x < 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sticky-ring expected CONTACT goal");
  }
  const int ring_md = abs(contact_x - tribe_x) + abs(contact_y - tribe_y);
  if (ring_md != 2) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: sticky CONTACT=(%d,%d) ring_md=%d (want 2)\n",
      contact_x,
      contact_y,
      ring_md
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected sticky≥2 to prefer CONTACT ring md=2");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: sticky CONTACT closer-ring ok (goto=(%d,%d) md=%d)\n",
    contact_x,
    contact_y,
    ring_md
  );
  return 0;
}

/*
 * Fog explore without CONTACT: peaceful Scout, own≥1, no beyond-adjacent tribe
 * → AI_MOVE to nearest unseen land within MD 8 (map_tile_seen_by). No CONTACT
 * upsert. Cite: Col1 FoW / euro_unit_act fog-explore.
 */
static int smoke_scout_fog_explore_no_contact(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  map.seen = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3 || !map.seen) {
    return fail("fog-explore alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  /* Reveal around scout at (5,5); leave (5,9) unseen at MD 4. */
  for (int dy = -2; dy <= 2; ++dy) {
    for (int dx = -2; dx <= 2; ++dx) {
      map_reveal_tile(&map, 5 + dx, 5 + dy, nation);
    }
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
    free(map.seen);
    return fail("fog-explore spawn");
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
  col1.head.tribe_count = 0;
  col1.tribe = NULL; /* no CONTACT ring */

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
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  scout = units_get(&units, sid);
  int has_contact = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_CONTACT) {
      has_contact = 1;
      break;
    }
  }

  const int toward_unseen =
    scout && scout->active && scout->orders == UNITS_ORDER_AI_MOVE &&
    scout->goto_x >= 0 && scout->goto_y >= 0 &&
    !map_tile_seen_by(&map, scout->goto_x, scout->goto_y, nation) &&
    (abs(scout->goto_x - 5) + abs(scout->goto_y - 5)) <= 8 &&
    (abs(scout->goto_x - 5) + abs(scout->goto_y - 5)) >= 1;

  if (has_contact || !toward_unseen) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: fog-explore orders=%d goto=(%d,%d) contact=%d\n",
      scout ? scout->orders : -1,
      scout ? scout->goto_x : -1,
      scout ? scout->goto_y : -1,
      has_contact
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("expected Scout AI_MOVE to unseen MD≤8 without CONTACT");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.seen);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: fog-explore no-CONTACT ok (goto=(%d,%d))\n",
    scout->goto_x,
    scout->goto_y
  );
  return 0;
}

/*
 * Seasoned Scout fog deepen: with near (MD=3) and far (MD=7) unseen tiles,
 * Seasoned prefers the deeper fog target; plain Scout keeps nearest.
 * Cite: Colonization.pdf Seasoned Scout "Better at exploring"; no invented sight.
 */
static int smoke_seasoned_scout_deeper_fog(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  map.seen = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3 || !map.seen) {
    return fail("seasoned-fog alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  /* Reveal all MD≤8 around (5,5) except (5,8) MD=3 and (5,12) MD=7. */
  for (int dy = -8; dy <= 8; ++dy) {
    for (int dx = -8; dx <= 8; ++dx) {
      const int md = abs(dx) + abs(dy);
      if (md < 1 || md > 8) {
        continue;
      }
      const int nx = 5 + dx;
      const int ny = 5 + dy;
      if ((nx == 5 && ny == 8) || (nx == 5 && ny == 12)) {
        continue;
      }
      map_reveal_tile(&map, nx, ny, nation);
    }
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
    free(map.seen);
    return fail("seasoned-fog spawn");
  }
  scout->nation_id = nation;
  scout->moves_left = 4;
  scout->orders = 0;
  scout->horses = 50;
  scout->profession = UNITS_JOB_SCOUT; /* → display "Seasoned Scout" */

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.head.tribe_count = 0;
  col1.tribe = NULL;

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

  scout = units_get(&units, sid);
  const int deep_md =
    scout && scout->active && scout->orders == UNITS_ORDER_AI_MOVE &&
    scout->goto_x >= 0 && scout->goto_y >= 0
      ? (abs(scout->goto_x - 5) + abs(scout->goto_y - 5))
      : -1;
  const int deep =
    scout && scout->active && scout->orders == UNITS_ORDER_AI_MOVE &&
    scout->goto_x == 5 && scout->goto_y == 12 && deep_md == 7 &&
    !map_tile_seen_by(&map, scout->goto_x, scout->goto_y, nation);
  if (!deep) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: seasoned-fog orders=%d goto=(%d,%d) md=%d name=%s\n",
      scout ? scout->orders : -1,
      scout ? scout->goto_x : -1,
      scout ? scout->goto_y : -1,
      deep_md,
      scout ? units_display_name(&units, scout) : "?"
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("expected Seasoned Scout AI_MOVE to deeper unseen MD=7 not MD=3");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.seen);
  fprintf(stderr, "smoke_ai_euro_expand: Seasoned Scout deeper fog ok\n");
  return 0;
}

/*
 * Treasure train: idle Treasure inland → AI_MOVE toward own coastal colony.
 * Cite: Colonization.pdf Treasure Trains — park in coastal colony.
 */
static int smoke_treasure_coast(void) {
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
    return fail("treasure alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  /* Ocean adjacent west of coastal colony at (4,4). */
  map.terrain[4 * 16 + 3] = 25; /* water at (3,4) */
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Treasure");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

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
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Inland Treasure — not on coast. */
  const int tid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* treasure = units_get(&units, tid);
  if (!treasure) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure spawn");
  }
  treasure->nation_id = nation;
  treasure->moves_left = 1;
  treasure->orders = 0;

  ai_goals_reset();
  /* Distant FOUND should not yank Treasure off coast route. */
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

  treasure = units_get(&units, tid);
  if (!treasure || !treasure->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure should remain active");
  }
  if (treasure->orders != UNITS_ORDER_AI_MOVE || treasure->goto_x != 4 ||
      treasure->goto_y != 4) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: treasure orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      treasure->orders,
      treasure->goto_x,
      treasure->goto_y,
      treasure->x,
      treasure->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Treasure AI_MOVE toward coastal colony (4,4)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: treasure coast ok (goto=(%d,%d))\n",
    treasure->goto_x,
    treasure->goto_y
  );
  return 0;
}

/*
 * Missionary CONTACT: peace Jesuit/Missionary → CONTACT + AI_MOVE toward
 * nearest tribe without mission. Fleeing (Alarm≥55 adjacent) skips.
 */
static int smoke_missionary_contact(void) {
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
    return fail("missionary alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Jesuit Missionary");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

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
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int mid = units_spawn(&units, 0, 6, 6);
  ColonizeUnit* miss = units_get(&units, mid);
  if (!miss) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("missionary spawn");
  }
  miss->nation_id = nation;
  miss->moves_left = 2;
  miss->orders = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.tribe_count = 2;
  col1.tribe = calloc(2, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("missionary alloc tribe");
  }
  /* Farther tribe already has a mission — prefer unmissioned nearer. */
  col1.tribe[0].x = (uint8_t)tribe_x;
  col1.tribe[0].y = (uint8_t)tribe_y;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].population = 4;
  col1.tribe[0].mission = 0xff;
  col1.tribe[1].x = 8;
  col1.tribe[1].y = 8;
  col1.tribe[1].nation_id = 4;
  col1.tribe[1].population = 3;
  col1.tribe[1].mission = (uint8_t)nation; /* own mission — skip */

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
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  miss = units_get(&units, mid);
  int contact_x = -1;
  int contact_y = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_CONTACT) {
      contact_x = g->x;
      contact_y = g->y;
      break;
    }
  }
  if (contact_x != tribe_x || contact_y != tribe_y) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: missionary CONTACT=(%d,%d) want (%d,%d)\n",
      contact_x,
      contact_y,
      tribe_x,
      tribe_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(col1.tribe);
    return fail("expected CONTACT at unmissioned tribe");
  }
  if (!miss || miss->orders != UNITS_ORDER_AI_MOVE ||
      (miss->goto_x != tribe_x || miss->goto_y != tribe_y)) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: missionary orders=%d goto=(%d,%d)\n",
      miss ? miss->orders : -1,
      miss ? miss->goto_x : -1,
      miss ? miss->goto_y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(col1.tribe);
    return fail("expected Missionary AI_MOVE toward unmissioned tribe");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(col1.tribe);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: missionary CONTACT ok (goto=(%d,%d))\n",
    miss->goto_x,
    miss->goto_y
  );
  return 0;
}

/*
 * Missionary flee gate: adjacent alarmed tribe (friction≥55) → no CONTACT.
 */
static int smoke_missionary_flee_skip(void) {
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
    return fail("miss-flee alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Missionary");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 2;
  c->y = 2;
  c->population = 2;
  c->colonist_count = 2;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Adjacent to alarmed tribe at (8,8). */
  const int mid = units_spawn(&units, 0, 8, 9);
  ColonizeUnit* miss = units_get(&units, mid);
  if (!miss) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("miss-flee spawn");
  }
  miss->nation_id = nation;
  miss->moves_left = 2;
  miss->orders = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.tribe_count = 2;
  col1.tribe = calloc(2, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("miss-flee alloc tribe");
  }
  col1.tribe[0].x = 8;
  col1.tribe[0].y = 8;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].population = 4;
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].alarm[nation].friction = 60; /* ≥55 flee */
  /* Distant unmissioned — would be CONTACT target if not fleeing. */
  col1.tribe[1].x = 14;
  col1.tribe[1].y = 14;
  col1.tribe[1].nation_id = 4;
  col1.tribe[1].population = 3;
  col1.tribe[1].mission = 0xff;

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
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_CONTACT) {
      fprintf(
        stderr,
        "smoke_ai_euro_expand: flee skip got CONTACT=(%d,%d)\n",
        g->x,
        g->y
      );
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      free(col1.tribe);
      return fail("fleeing Missionary should not upsert CONTACT");
    }
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(col1.tribe);
  fprintf(stderr, "smoke_ai_euro_expand: missionary flee-skip CONTACT ok\n");
  return 0;
}

/*
 * Food emergency: food_short high + Pioneer at MD 5 → LABOR goto toward hungry
 * colony (not only MD≤1 bind). Cite: 5cf6 food_short; manual 2 food/colonist.
 */
static int smoke_food_emergency_labor(void) {
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
    return fail("food-emerg alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
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
  c->population = 4;
  c->colonist_count = 4;
  c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short = 8 ≥ 4 */
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Pioneer at MD 5 — beyond adjacent LABOR bind. */
  const int pid = units_spawn(&units, 0, 9, 4);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("food-emerg spawn");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves_left = 3;

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
      "smoke_ai_euro_expand: food-emerg labor=%d orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      labor,
      pioneer ? pioneer->orders : -1,
      pioneer ? pioneer->goto_x : -1,
      pioneer ? pioneer->goto_y : -1,
      pioneer ? pioneer->x : -1,
      pioneer ? pioneer->y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected food-emergency LABOR bind for distant Pioneer");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: food-emergency LABOR ok\n");
  return 0;
}

/*
 * Expert Farmer food LABOR: idle Expert Farmer (profession @JOB Farmer / name)
 * at MD 5 + food_short → LABOR goto. Cite: building_production.md Farmer→Food;
 * euro_unit_act §2e Expert Farmer.
 */
static int smoke_expert_farmer_food_labor(void) {
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
    return fail("expert-farmer alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Farmer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 0;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short = 8 ≥ 4 */
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Expert Farmer at MD 5. */
  const int fid = units_spawn(&units, 0, 9, 4);
  ColonizeUnit* farmer = units_get(&units, fid);
  if (!farmer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expert-farmer spawn");
  }
  farmer->nation_id = nation;
  farmer->profession = 0; /* @JOB Farmer */
  farmer->orders = 0;
  farmer->moves_left = 3;

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
      "smoke_ai_euro_expand: expert-farmer orders=%d goto=(%d,%d) pos=(%d,%d) labor=%d\n",
      farmer ? farmer->orders : -1,
      farmer ? farmer->goto_x : -1,
      farmer ? farmer->goto_y : -1,
      farmer ? farmer->x : -1,
      farmer ? farmer->y : -1,
      labor_bound
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Farmer food-short LABOR bind");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: Expert Farmer food LABOR ok\n");
  return 0;
}

/*
 * Free Colonist food LABOR (non-Expert Farmer): idle Free Colonist at MD 5 +
 * food_short > 0 but < 4 (not emergency) → LABOR goto. Cite: euro_unit_act §2e
 * Free Colonist food LABOR; manual 2 food/colonist.
 */
static int smoke_free_colonist_food_labor(void) {
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
    return fail("fc-food alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 0;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 1;
  c->colonist_count = 1;
  c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short = 2 (not emergency ≥4) */
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Free Colonist at MD 5 — beyond adjacent; needs food-short MD≤8 bind. */
  const int fid = units_spawn(&units, 0, 9, 4);
  ColonizeUnit* col = units_get(&units, fid);
  if (!col) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("fc-food spawn");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves_left = 3;

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
      "smoke_ai_euro_expand: fc-food orders=%d goto=(%d,%d) pos=(%d,%d) labor=%d\n",
      col ? col->orders : -1,
      col ? col->goto_x : -1,
      col ? col->goto_y : -1,
      col ? col->x : -1,
      col ? col->y : -1,
      labor_bound
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Free Colonist food-short LABOR bind (non-Farmer)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: Free Colonist food LABOR ok\n");
  return 0;
}

/*
 * Tools-short Pioneer deepen: peace Pioneer at MD 5 + tools_short colony →
 * LABOR goto (feeds on-tile tools delivery). Cite: euro_unit_act §2e.
 */
static int smoke_tools_short_pioneer_labor(void) {
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
    return fail("tools-labor alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
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
  c->stock[COLONIZE_CARGO_FOOD] = 40; /* not food emergency */
  c->stock[COLONIZE_CARGO_TOOLS] = 5; /* tools_short = 15 */
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Pioneer at MD 5 — beyond adjacent LABOR; tools deepen extends to MD≤8. */
  const int pid = units_spawn(&units, 0, 9, 4);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("tools-labor spawn");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves_left = 3;

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
      "smoke_ai_euro_expand: tools-labor orders=%d goto=(%d,%d) pos=(%d,%d) "
      "labor=%d\n",
      pioneer ? pioneer->orders : -1,
      pioneer ? pioneer->goto_x : -1,
      pioneer ? pioneer->goto_y : -1,
      pioneer ? pioneer->x : -1,
      pioneer ? pioneer->y : -1,
      labor
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected tools-short LABOR bind for distant Pioneer");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: tools-short Pioneer LABOR ok\n");
  return 0;
}

/*
 * Treasure at coastal colony + adjacent ship with space → board + AI_SAIL
 * Europe (eastward). Cite: Colonization.pdf Treasure Trains. Gold unload PARKED.
 */
static int smoke_treasure_board_sail(void) {
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
    return fail("treasure-sail alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  /* Water west of coastal colony (4,4) — ship sits at (3,4). */
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure-sail colony should be coastal");
  }
  /* More eastern water for Europe-sail target. */
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Treasure");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
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
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int tid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* treasure = units_get(&units, tid);
  if (!treasure) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure-sail spawn treasure");
  }
  treasure->nation_id = nation;
  treasure->moves_left = 1;
  treasure->orders = 0;

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure-sail spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
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
  col1.nation[nation].gold = 200;
  const uint32_t gold_before = col1.nation[nation].gold;

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

  treasure = units_get(&units, tid);
  ship = units_get(&units, sid);
  if (!treasure || !ship || !ship->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure-sail units missing after turn");
  }
  if (treasure->aboard_ship_id != sid) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: treasure aboard=%d want ship %d pos=(%d,%d)\n",
      treasure->aboard_ship_id,
      sid,
      treasure->x,
      treasure->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Treasure boarded onto Galleon");
  }
  if (ship->orders != UNITS_ORDER_AI_SAIL || ship->goto_x <= ship->x) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: ship orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      ship->orders,
      ship->goto_x,
      ship->goto_y,
      ship->x,
      ship->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Galleon AI_SAIL eastward (Europe stand-in)");
  }
  /* PARK: Treasure→gold unload — AI must not invent a Europe unload credit.
   * Planning still applies the existing 5d04 treasury bump (small); a Treasure
   * haul would be hundreds. Cite: Colonization.pdf Treasure Trains; EuropeScreen. */
  const uint32_t gold_after = col1.nation[nation].gold;
  const unsigned gold_delta =
    gold_after >= gold_before ? (unsigned)(gold_after - gold_before) : 0u;
  if (gold_delta > 80u) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: treasure gold %u→%u delta=%u (PARK: no AI gold unload)\n",
      (unsigned)gold_before,
      (unsigned)gold_after,
      gold_delta
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("Treasure board/sail must not invent Europe gold unload (PARK)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: treasure board+sail ok (ship AI_SAIL goto=(%d,%d) "
    "gold_delta=%u PARK unload)\n",
    ship->goto_x,
    ship->goto_y,
    gold_delta
  );
  return 0;
}

/*
 * Idle Wagon with hold capacity → AI_MOVE toward tools-short colony.
 * Cite: euro_unit_act §2d wagon haul / tools delivery.
 */
static int smoke_wagon_haul_tools_short(void) {
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
    return fail("wagon-haul alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* short_c = &colonies.colonies[0];
  short_c->id = 0;
  short_c->active = true;
  short_c->nation_id = nation;
  short_c->x = 4;
  short_c->y = 4;
  short_c->population = 3;
  short_c->colonist_count = 3;
  short_c->stock[COLONIZE_CARGO_TOOLS] = 5;
  short_c->stock[COLONIZE_CARGO_FOOD] = 40;
  short_c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Idle empty wagon inland — capacity only, no TOOLS yet. */
  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-haul spawn");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;

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

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-haul should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: wagon orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon AI_MOVE toward tools-short colony (4,4)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: wagon haul tools-short ok\n");
  return 0;
}

/*
 * Sticky + FoW: prefer deeper unseen ring (md=4) over closer seen (md=2).
 * Cite: euro_unit_act §2c2 sticky+fog deepen.
 */
static int smoke_scout_sticky_fog_deeper_unseen(void) {
  const int nation = 1;
  const int tribe_x = 10;
  const int tribe_y = 10;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  map.seen = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3 || !map.seen) {
    return fail("sticky-fog alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  /* Reveal all md=2 ring; leave md=4 unseen so sticky+fog picks deeper. */
  for (int dy = -4; dy <= 4; ++dy) {
    for (int dx = -4; dx <= 4; ++dx) {
      const int md = abs(dx) + abs(dy);
      if (md != 2) {
        continue;
      }
      const int nx = tribe_x + dx;
      const int ny = tribe_y + dy;
      if (nx >= 0 && ny >= 0 && nx < 16 && ny < 16) {
        map_reveal_tile(&map, nx, ny, nation);
      }
    }
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
  c->x = 2;
  c->y = 2;
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
    free(map.seen);
    return fail("sticky-fog spawn scout");
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
  col1.nation[nation].unknown26[8] = 2; /* sticky very-low */
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("sticky-fog alloc tribe");
  }
  col1.tribe[0].x = (uint8_t)tribe_x;
  col1.tribe[0].y = (uint8_t)tribe_y;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].population = 4;
  col1.tribe[0].mission = 0xff;

  ai_goals_reset();

  uint32_t turn = 17;
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

  int contact_x = -1;
  int contact_y = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (!g || g->code != AI_GOAL_CONTACT) {
      continue;
    }
    contact_x = g->x;
    contact_y = g->y;
    break;
  }
  free(col1.tribe);
  if (contact_x < 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("sticky-fog expected CONTACT goal");
  }
  const int ring_md = abs(contact_x - tribe_x) + abs(contact_y - tribe_y);
  const int ok =
    ring_md == 4 && !map_tile_seen_by(&map, contact_x, contact_y, nation);
  if (!ok) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: sticky-fog CONTACT=(%d,%d) md=%d seen=%d\n",
      contact_x,
      contact_y,
      ring_md,
      (int)map_tile_seen_by(&map, contact_x, contact_y, nation)
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("expected sticky+fog deeper unseen ring md=4");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.seen);
  fprintf(stderr, "smoke_ai_euro_expand: sticky+fog deeper unseen ring ok\n");
  return 0;
}

/*
 * Idle Caravel with goods-hold capacity → AI_SAIL toward tools-short coastal
 * colony water. Cite: euro_unit_act §2d2 cargo haul.
 */
static int smoke_ship_trade_haul_tools_short(void) {
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
    return fail("ship-haul alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  /* Water corridor: colony (4,4) coastal via (3,4); ship starts at (3,10). */
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 3] = 25;
  }
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-haul colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

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
  c->stock[COLONIZE_CARGO_TOOLS] = 5; /* tools-short */
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-haul spawn");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;

  ai_goals_reset();
  /* Distant FOUND must not steal idle cargo haul. */
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

  ship = units_get(&units, sid);
  if (!ship || !ship->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-haul should remain active");
  }
  /* Expect sail toward water near (4,4) — typically (3,4). */
  const int near_colony =
    abs(ship->goto_x - 4) <= 1 && abs(ship->goto_y - 4) <= 1 &&
    (ship->goto_x != 4 || ship->goto_y != 4);
  const int sailed = ship->orders == UNITS_ORDER_AI_SAIL && near_colony;
  /* Or already moved onto berth water. */
  const int at_berth = abs(ship->x - 4) <= 1 && abs(ship->y - 4) <= 1 &&
                       map_tile_is_water(&map, ship->x, ship->y);
  if (!sailed && !at_berth) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: ship orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      ship->orders,
      ship->goto_x,
      ship->goto_y,
      ship->x,
      ship->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Caravel AI_SAIL toward tools-short coastal colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: ship trade haul tools-short ok\n");
  return 0;
}

int main(void) {
  if (smoke_second_wave() != 0) {
    return 1;
  }
  if (smoke_second_colony_coastal_prefer() != 0) {
    return 1;
  }
  if (smoke_scout_explore() != 0) {
    return 1;
  }
  if (smoke_scout_fog_prefer_unseen() != 0) {
    return 1;
  }
  if (smoke_scout_sticky_closer_ring() != 0) {
    return 1;
  }
  if (smoke_scout_sticky_fog_deeper_unseen() != 0) {
    return 1;
  }
  if (smoke_scout_fog_explore_no_contact() != 0) {
    return 1;
  }
  if (smoke_seasoned_scout_deeper_fog() != 0) {
    return 1;
  }
  if (smoke_treasure_coast() != 0) {
    return 1;
  }
  if (smoke_treasure_board_sail() != 0) {
    return 1;
  }
  if (smoke_missionary_contact() != 0) {
    return 1;
  }
  if (smoke_missionary_flee_skip() != 0) {
    return 1;
  }
  if (smoke_pioneer_tools_delivery() != 0) {
    return 1;
  }
  if (smoke_tools_cargo_hire() != 0) {
    return 1;
  }
  if (smoke_tools_mid_threshold_hire() != 0) {
    return 1;
  }
  if (smoke_wagon_hire_once() != 0) {
    return 1;
  }
  if (smoke_wagon_tools_delivery() != 0) {
    return 1;
  }
  if (smoke_wagon_haul_tools_short() != 0) {
    return 1;
  }
  if (smoke_ship_trade_haul_tools_short() != 0) {
    return 1;
  }
  if (smoke_labor_bind_food_short() != 0) {
    return 1;
  }
  if (smoke_food_emergency_labor() != 0) {
    return 1;
  }
  if (smoke_expert_farmer_food_labor() != 0) {
    return 1;
  }
  if (smoke_free_colonist_food_labor() != 0) {
    return 1;
  }
  if (smoke_tools_short_pioneer_labor() != 0) {
    return 1;
  }
  if (smoke_construction_labor_stockade() != 0) {
    return 1;
  }
  if (smoke_master_carpenter_construction_labor() != 0) {
    return 1;
  }
  if (smoke_lumberjack_warehouse_labor() != 0) {
    return 1;
  }
  if (smoke_stockade_threat_labor() != 0) {
    return 1;
  }
  if (smoke_multistep_military() != 0) {
    return 1;
  }
  if (smoke_dock_expert_hire() != 0) {
    return 1;
  }
  if (smoke_treasury_skip_hire() != 0) {
    return 1;
  }
  fprintf(stderr, "smoke_ai_euro_expand: ok\n");
  return 0;
}
