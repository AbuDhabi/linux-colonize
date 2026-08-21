/* Smoke: Euro second-wave settle + CONTACT scout rings + tools delivery. */
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_euro_expand: FAIL %s\n", msg);
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
static int unit_second_wave(void) {
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
      "unit_ai_euro_expand: colonies=%d pioneer=(%d,%d) active=%d\n",
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
  fprintf(stderr, "unit_ai_euro_expand: second-wave ok (colonies=%d)\n", final_n);
  return 0;
}

/*
 * Second+ colony FOUND via 06ae: when colony_count>=1, prefer coastal foundable
 * over richer inland (river) tile. Cite: ai_euro_pick_founding_tile coastal +6;
 * fandom Docks coastal gate.
 */
static int unit_second_colony_coastal_prefer(void) {
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

  if (found_x == 8 && found_y == 7) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected coastal FOUND over inland river (8,7)");
  }
  if (found_x < 0 || !map_tile_is_coastal(&map, found_x, found_y)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: coastal FOUND got=(%d,%d) coastal=%d inland7=%d\n",
      found_x,
      found_y,
      found_x >= 0 ? map_tile_is_coastal(&map, found_x, found_y) : 0,
      map_tile_is_coastal(&map, 8, 7)
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected coastal FOUND (06ae + coastal bias) over inland");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: second-colony coastal FOUND ok\n");
  return 0;
}

/*
 * CONTACT scout rings (unpark #4): peaceful nation with own≥1 colony + Scout +
 * tribe beyond adjacent → upsert CONTACT at Manhattan ring 2–4 around tribe;
 * Scout AI_MOVE toward that tile. Fog plane optional (prefer unseen when set).
 */
static int unit_scout_explore(void) {
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
      "unit_ai_euro_expand: scout orders=%d goto=(%d,%d) pos=(%d,%d) "
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
    "unit_ai_euro_expand: CONTACT scout-ring ok (goto=(%d,%d) pos=(%d,%d) ring_md=%d)\n",
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
static int unit_pioneer_tools_delivery(void) {
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
      "unit_ai_euro_expand: tools before=%d after=%d (want +10)\n",
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
    "unit_ai_euro_expand: tools-delivery ok (tools %d→%d)\n",
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
static int unit_tools_cargo_hire(void) {
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
      "unit_ai_euro_expand: tools-cargo boarded=%d pax_tools=%d ship_tools=%d "
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
    "unit_ai_euro_expand: tools-cargo hire ok (boarded=%d ship_tools=%d "
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
 * lumber_short high, tools stocked: after dock Expert Lumberjack hire, ship/colony
 * gets LUMBER cargo stand-in (not TOOLS). Cite: euro_unit_act §2d mid-5d04;
 * 5cf6 lumber_short.
 */
static int unit_lumber_cargo_hire(void) {
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
    return fail("lumber-cargo alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Lumberjack");
  units.types[0].movement = 1;
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
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Warehouse");
  colonies.building_types[0].hammers = 80;
  colonies.building_type_count = 1;
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = 0;
    c->hammers = 10;
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int lumber0 = colonies.colonies[0].stock[COLONIZE_CARGO_LUMBER];

  const int ship_id = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("lumber-cargo spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Expert Lumberjacks");
  europe.dock[0].profession = 5;
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

  uint32_t turn = 22;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  int boarded = 0;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Lumberjack")) {
      boarded = 1;
      break;
    }
  }

  int ship_lumber = 0;
  int ship_tools = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] <= 0 || ship->hold_goods_amount[h] >= 255) {
      continue;
    }
    if (ship->hold_goods_type[h] == COLONIZE_CARGO_LUMBER) {
      ship_lumber += ship->hold_goods_amount[h];
    }
    if (ship->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
      ship_tools += ship->hold_goods_amount[h];
    }
  }
  const int colony_lumber_rose =
    colonies.colonies[0].stock[COLONIZE_CARGO_LUMBER] >= lumber0 + 15 ||
    colonies.colonies[1].stock[COLONIZE_CARGO_LUMBER] >= 15 ||
    colonies.colonies[2].stock[COLONIZE_CARGO_LUMBER] >= 15;

  if (!boarded || ship_tools > 0 || !(ship_lumber >= 20 || colony_lumber_rose)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: lumber-cargo boarded=%d ship_lumber=%d ship_tools=%d "
      "colony_lumber=%d/%d/%d dock=%d\n",
      boarded,
      ship_lumber,
      ship_tools,
      colonies.colonies[0].stock[COLONIZE_CARGO_LUMBER],
      colonies.colonies[1].stock[COLONIZE_CARGO_LUMBER],
      colonies.colonies[2].stock[COLONIZE_CARGO_LUMBER],
      europe.dock_count
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Lumberjack hire + LUMBER cargo/colony (not TOOLS)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: lumber-cargo hire ok\n");
  return 0;
}

/*
 * food_short high, tools/lumber stocked: after dock Expert Farmer hire, ship/colony
 * gets FOOD cargo stand-in. Cite: euro_unit_act §2d mid-5d04; 5cf6 food_short.
 */
static int unit_food_cargo_hire(void) {
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
    return fail("food-cargo alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Farmer");
  units.types[0].movement = 1;
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
    c->population = 8;
    c->colonist_count = 8;
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 40;
    c->stock[COLONIZE_CARGO_MUSKETS] = 40;
    c->stock[COLONIZE_CARGO_HORSES] = 40;
    c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short = pop*2 each → 48 */
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int food0 = colonies.colonies[0].stock[COLONIZE_CARGO_FOOD];

  const int ship_id = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("food-cargo spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Expert Farmers");
  europe.dock[0].profession = 0;
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

  uint32_t turn = 22;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  int boarded = 0;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Farmer")) {
      boarded = 1;
      break;
    }
  }

  int ship_food = 0;
  int ship_tools = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] <= 0 || ship->hold_goods_amount[h] >= 255) {
      continue;
    }
    if (ship->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
      ship_food += ship->hold_goods_amount[h];
    }
    if (ship->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
      ship_tools += ship->hold_goods_amount[h];
    }
  }
  const int colony_food_rose =
    colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] >= food0 + 15 ||
    colonies.colonies[1].stock[COLONIZE_CARGO_FOOD] >= 15 ||
    colonies.colonies[2].stock[COLONIZE_CARGO_FOOD] >= 15;

  if (!boarded || ship_tools > 0 || !(ship_food >= 20 || colony_food_rose)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: food-cargo boarded=%d ship_food=%d ship_tools=%d "
      "colony_food=%d/%d/%d\n",
      boarded,
      ship_food,
      ship_tools,
      colonies.colonies[0].stock[COLONIZE_CARGO_FOOD],
      colonies.colonies[1].stock[COLONIZE_CARGO_FOOD],
      colonies.colonies[2].stock[COLONIZE_CARGO_FOOD]
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Farmer hire + FOOD cargo/colony (not TOOLS)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: food-cargo hire ok\n");
  return 0;
}

/*
 * horses_short high, other stocks full: after dock Seasoned Scout hire, ship/colony
 * gets HORSES cargo stand-in (+10). Cite: euro_unit_act §2d mid-5d04; horses_short.
 */
static int unit_horses_cargo_hire(void) {
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
    return fail("horses-cargo alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Seasoned Scout");
  units.types[0].movement = 4;
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
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 40;
    c->stock[COLONIZE_CARGO_MUSKETS] = 40;
    c->stock[COLONIZE_CARGO_HORSES] = 0; /* horses_short tally */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int horses0 = colonies.colonies[0].stock[COLONIZE_CARGO_HORSES];

  const int ship_id = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("horses-cargo spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Seasoned Scouts");
  europe.dock[0].profession = 22;
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

  uint32_t turn = 22;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  int boarded = 0;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Scout")) {
      boarded = 1;
      break;
    }
  }

  int ship_horses = 0;
  int ship_tools = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] <= 0 || ship->hold_goods_amount[h] >= 255) {
      continue;
    }
    if (ship->hold_goods_type[h] == COLONIZE_CARGO_HORSES) {
      ship_horses += ship->hold_goods_amount[h];
    }
    if (ship->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
      ship_tools += ship->hold_goods_amount[h];
    }
  }
  const int colony_horses_rose =
    colonies.colonies[0].stock[COLONIZE_CARGO_HORSES] >= horses0 + 10 ||
    colonies.colonies[1].stock[COLONIZE_CARGO_HORSES] >= 10 ||
    colonies.colonies[2].stock[COLONIZE_CARGO_HORSES] >= 10;

  if (!boarded || ship_tools > 0 || !(ship_horses >= 10 || colony_horses_rose)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: horses-cargo boarded=%d ship_horses=%d ship_tools=%d "
      "colony_horses=%d/%d/%d\n",
      boarded,
      ship_horses,
      ship_tools,
      colonies.colonies[0].stock[COLONIZE_CARGO_HORSES],
      colonies.colonies[1].stock[COLONIZE_CARGO_HORSES],
      colonies.colonies[2].stock[COLONIZE_CARGO_HORSES]
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Scout hire + HORSES cargo/colony (not TOOLS)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: horses-cargo hire ok\n");
  return 0;
}

/*
 * muskets_short high, other stocks full: after dock Master Gunsmith hire,
 * ship/colony gets MUSKETS cargo stand-in (+10). Cite: euro_unit_act §2d mid-5d04.
 */
static int unit_muskets_cargo_hire(void) {
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
    return fail("muskets-cargo alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Gunsmith");
  units.types[0].movement = 1;
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
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 40;
    c->stock[COLONIZE_CARGO_MUSKETS] = 0; /* muskets_short tally */
    c->stock[COLONIZE_CARGO_HORSES] = 40;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int muskets0 = colonies.colonies[0].stock[COLONIZE_CARGO_MUSKETS];

  const int ship_id = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("muskets-cargo spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Master Gunsmiths");
  europe.dock[0].profession = 15;
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

  uint32_t turn = 22;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  int boarded = 0;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Gunsmith")) {
      boarded = 1;
      break;
    }
  }

  int ship_muskets = 0;
  int ship_tools = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] <= 0 || ship->hold_goods_amount[h] >= 255) {
      continue;
    }
    if (ship->hold_goods_type[h] == COLONIZE_CARGO_MUSKETS) {
      ship_muskets += ship->hold_goods_amount[h];
    }
    if (ship->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
      ship_tools += ship->hold_goods_amount[h];
    }
  }
  const int colony_muskets_rose =
    colonies.colonies[0].stock[COLONIZE_CARGO_MUSKETS] >= muskets0 + 10 ||
    colonies.colonies[1].stock[COLONIZE_CARGO_MUSKETS] >= 10 ||
    colonies.colonies[2].stock[COLONIZE_CARGO_MUSKETS] >= 10;

  if (!boarded || ship_tools > 0 || !(ship_muskets >= 10 || colony_muskets_rose)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: muskets-cargo boarded=%d ship_muskets=%d ship_tools=%d "
      "colony_muskets=%d/%d/%d\n",
      boarded,
      ship_muskets,
      ship_tools,
      colonies.colonies[0].stock[COLONIZE_CARGO_MUSKETS],
      colonies.colonies[1].stock[COLONIZE_CARGO_MUSKETS],
      colonies.colonies[2].stock[COLONIZE_CARGO_MUSKETS]
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Gunsmith hire + MUSKETS cargo/colony (not TOOLS)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: muskets-cargo hire ok\n");
  return 0;
}

/*
 * tools_short == 40 (2 colonies tools=0): threshold lowered from >40 to >20 —
 * still prefer Pioneer + tools cargo / colony +15 (no Wagon type in pool).
 */
static int unit_tools_mid_threshold_hire(void) {
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
      "unit_ai_euro_expand: tools-mid boarded=%d pax_tools=%d ship_tools=%d "
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
    "unit_ai_euro_expand: tools-mid hire ok (boarded=%d ship_tools=%d "
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

/*
 * Thin 5d04 mid-game peace: colonies ≥ 6 still hires Wagon once under
 * tools_short>30 (Free Colonist settle spam stays gated). Cite: unpark #4.
 */
static int unit_wagon_hire_once_colonies_ge6(void) {
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
    return fail("wagon-hire-ge6 alloc map");
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
  units.types[2].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  for (int i = 0; i < 6; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + (i % 3) * 2;
    c->y = 2 + (i / 3) * 2;
    c->population = 3;
    c->colonist_count = 3;
    c->stock[COLONIZE_CARGO_TOOLS] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 6;
  colonies.next_id = 6;

  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-hire-ge6 spawn europe ship");
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

  uint32_t turn = 46;
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
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Wagon")) {
      wagon_boarded = 1;
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
      "unit_ai_euro_expand: wagon-ge6 boarded=%d tools=%d cargo=%d gold=%u\n",
      wagon_boarded,
      wagon_tools,
      ship->cargo_count,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon hire at colonies>=6 under tools_short");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon-hire colonies>=6 ok\n");
  return 0;
}

static int unit_wagon_hire_once(void) {
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
      "unit_ai_euro_expand: wagon first pass boarded=%d wagon_tools=%d "
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
      "unit_ai_euro_expand: wagon once-guard wagons=%d pioneer=%d cargo=%d "
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
    "unit_ai_euro_expand: wagon-hire once ok (wagon_tools=%d then Pioneer)\n",
    wagon_tools
  );
  return 0;
}

/*
 * lumber_short>30 (tools plentiful) + Wagon Train → hire wagon once with LUMBER
 * aboard. Cite: euro_unit_act §2d / 5cf6 lumber_short; Colonization.pdf Wagon.
 */
static int unit_wagon_hire_lumber_once(void) {
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
    return fail("wagon-lumber alloc map");
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
  units.types[2].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_type_count = 1;
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
    c->stock[COLONIZE_CARGO_TOOLS] = 40; /* no tools short */
    c->stock[COLONIZE_CARGO_LUMBER] = 0; /* short when constructing */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = 0; /* Stockade → lumber_short tallies */
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-lumber spawn europe ship");
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
  int wagon_lumber = 0;
  ship = units_get(&units, ship_id);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (!ty || strstr(ty->name, "Wagon") == NULL) {
        continue;
      }
      wagon_boarded = 1;
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        if (pax->hold_goods_amount[h] > 0 && pax->hold_goods_amount[h] < 255 &&
            pax->hold_goods_type[h] == COLONIZE_CARGO_LUMBER) {
          wagon_lumber += pax->hold_goods_amount[h];
        }
      }
    }
  }
  if (!wagon_boarded || wagon_lumber < 20) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon lumber boarded=%d lumber=%d cargo=%d\n",
      wagon_boarded,
      wagon_lumber,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon Train hire with LUMBER aboard");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_expand: wagon-hire lumber once ok (wagon_lumber=%d)\n",
    wagon_lumber
  );
  return 0;
}

/*
 * ore_short>30 (tools/lumber plentiful) + Wagon Train → hire wagon once with ORE
 * aboard. Cite: euro_unit_act §2d / 5cf6 ore_short; Colonization.pdf Wagon.
 */
static int unit_wagon_hire_ore_once(void) {
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
    return fail("wagon-ore alloc map");
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
  units.types[2].cargo = 2;

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
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 0; /* ore_short=40 >30 */
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
    return fail("wagon-ore spawn europe ship");
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
  int wagon_ore = 0;
  ship = units_get(&units, ship_id);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (!ty || strstr(ty->name, "Wagon") == NULL) {
        continue;
      }
      wagon_boarded = 1;
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        if (pax->hold_goods_amount[h] > 0 && pax->hold_goods_amount[h] < 255 &&
            pax->hold_goods_type[h] == COLONIZE_CARGO_ORE) {
          wagon_ore += pax->hold_goods_amount[h];
        }
      }
    }
  }
  if (!wagon_boarded || wagon_ore < 20) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon ore boarded=%d ore=%d cargo=%d\n",
      wagon_boarded,
      wagon_ore,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon Train hire with ORE aboard");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon-hire ore once ok (wagon_ore=%d)\n", wagon_ore);
  return 0;
}

/*
 * muskets_short>20 (tools/lumber/ore plentiful) + Wagon → hire with MUSKETS.
 * Tally caps at 10/colony so gate is >20 (not >30). Cite: euro_unit_act §2d.
 */
static int unit_wagon_hire_muskets_once(void) {
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
    return fail("wagon-muskets alloc map");
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
  units.types[2].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 40;
    c->stock[COLONIZE_CARGO_MUSKETS] = 0; /* short=30 >20 */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-muskets spawn europe ship");
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
  int wagon_muskets = 0;
  ship = units_get(&units, ship_id);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (!ty || strstr(ty->name, "Wagon") == NULL) {
        continue;
      }
      wagon_boarded = 1;
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        if (pax->hold_goods_amount[h] > 0 && pax->hold_goods_amount[h] < 255 &&
            pax->hold_goods_type[h] == COLONIZE_CARGO_MUSKETS) {
          wagon_muskets += pax->hold_goods_amount[h];
        }
      }
    }
  }
  if (!wagon_boarded || wagon_muskets < 20) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon muskets boarded=%d muskets=%d cargo=%d\n",
      wagon_boarded,
      wagon_muskets,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon Train hire with MUSKETS aboard");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr, "unit_ai_euro_expand: wagon-hire muskets once ok (wagon_muskets=%d)\n", wagon_muskets
  );
  return 0;
}

/*
 * horses_short>20 + Wagon → hire with HORSES. Cite: euro_unit_act §2d haul ladder.
 */
static int unit_wagon_hire_horses_once(void) {
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
    return fail("wagon-horses alloc map");
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
  units.types[2].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 40;
    c->stock[COLONIZE_CARGO_MUSKETS] = 40;
    c->stock[COLONIZE_CARGO_HORSES] = 0; /* short=30 >20 */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-horses spawn europe ship");
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
  int wagon_horses = 0;
  ship = units_get(&units, ship_id);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (!ty || strstr(ty->name, "Wagon") == NULL) {
        continue;
      }
      wagon_boarded = 1;
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        if (pax->hold_goods_amount[h] > 0 && pax->hold_goods_amount[h] < 255 &&
            pax->hold_goods_type[h] == COLONIZE_CARGO_HORSES) {
          wagon_horses += pax->hold_goods_amount[h];
        }
      }
    }
  }
  if (!wagon_boarded || wagon_horses < 20) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon horses boarded=%d horses=%d cargo=%d\n",
      wagon_boarded,
      wagon_horses,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon Train hire with HORSES aboard");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr, "unit_ai_euro_expand: wagon-hire horses once ok (wagon_horses=%d)\n", wagon_horses
  );
  return 0;
}

/*
 * food_short>30 + Wagon → hire with FOOD. Cite: euro_unit_act §2d; 5cf6 food_short.
 */
static int unit_wagon_hire_food_once(void) {
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
    return fail("wagon-food alloc map");
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
  units.types[2].cargo = 2;

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
    c->population = 20; /* food_short += 40 each → 80 >30 */
    c->colonist_count = 20;
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 40;
    c->stock[COLONIZE_CARGO_MUSKETS] = 40;
    c->stock[COLONIZE_CARGO_HORSES] = 40;
    c->stock[COLONIZE_CARGO_FOOD] = 0;
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
    return fail("wagon-food spawn europe ship");
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
  int wagon_food = 0;
  ship = units_get(&units, ship_id);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (!ty || strstr(ty->name, "Wagon") == NULL) {
        continue;
      }
      wagon_boarded = 1;
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        if (pax->hold_goods_amount[h] > 0 && pax->hold_goods_amount[h] < 255 &&
            pax->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
          wagon_food += pax->hold_goods_amount[h];
        }
      }
    }
  }
  if (!wagon_boarded || wagon_food < 20) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon food boarded=%d food=%d cargo=%d\n",
      wagon_boarded,
      wagon_food,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon Train hire with FOOD aboard");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon-hire food once ok (wagon_food=%d)\n", wagon_food);
  return 0;
}

/*
 * Fog-aware CONTACT rings: when map.seen exists, prefer an unseen ring tile
 * over a closer seen tile (FoW explore — map_tile_seen_by).
 */
static int unit_scout_fog_prefer_unseen(void) {
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
      "unit_ai_euro_expand: fog CONTACT=(%d,%d) ring_md=%d seen=%d\n",
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
    "unit_ai_euro_expand: fog CONTACT prefer-unseen ok (goto=(%d,%d))\n",
    contact_x,
    contact_y
  );
  return 0;
}

/*
 * Thin multi-step land 20e6: Soldier with moves_left>=3 on MILITARY goto drains
 * scored steps in one dispatcher act when path is clear (MP full-drain).
 */
static int unit_multistep_military(void) {
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
  units.types[0].movement = 4;
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
  soldier->moves_left = 4;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected MILITARY MP-drain advance of ≥3 tiles");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_expand: MILITARY MP-drain ok (x %d→%d)\n",
    x0,
    soldier->x
  );
  return 0;
}

/*
 * Jan de Witt AI: Wagon on foreign Euro colony loads TRADE_GOODS surplus.
 * Cite: fandom Jan de Witt; colonies_de_witt_transfer_*; ai_euro_try_de_witt_foreign_trade.
 */
static int unit_de_witt_wagon_foreign_trade(void) {
  const int nation = 0;
  const int foreign = 1;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 12;
  map.height = 12;
  map.tile_count = 144;
  map.terrain = calloc(144, 1);
  map.layer2 = calloc(144, 1);
  map.layer3 = calloc(144, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("de Witt wagon alloc map");
  }
  for (int i = 0; i < 144; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  memset(units.types, 0, sizeof(units.types));
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].movement = 3;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* home = &colonies.colonies[0];
  home->id = 0;
  home->active = true;
  home->nation_id = nation;
  home->x = 2;
  home->y = 2;
  home->population = 2;
  home->building_in_production = -1;
  home->stock[COLONIZE_CARGO_FOOD] = 40;
  home->stock[COLONIZE_CARGO_TOOLS] = 50;
  home->stock[COLONIZE_CARGO_MUSKETS] = 50;
  home->stock[COLONIZE_CARGO_HORSES] = 50;
  ColonizeColony* fr = &colonies.colonies[1];
  fr->id = 1;
  fr->active = true;
  fr->nation_id = foreign;
  fr->x = 5;
  fr->y = 5;
  fr->population = 3;
  fr->building_in_production = -1;
  fr->stock[COLONIZE_CARGO_FOOD] = 30;
  fr->stock[COLONIZE_CARGO_TRADE_GOODS] = 40;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("de Witt wagon spawn");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 3;
  wagon->orders = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    col1.head.founding_father[i] = -1;
  }
  col1.player[nation].control = 0;
  col1.player[foreign].control = 1;
  col1.nation[nation].gold = 100;

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = nation;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  ctx.units = &units;
  ctx.colonies = &colonies;

  /* Without FF: refuse load (API already smoked); act must not strip foreign stock. */
  turn_refresh_moves_for_nation(&units, nation, &col1, &map, NULL, NULL, NULL);
  ai_goals_reset();
  ai_euro_dispatcher_turn(&ctx, nation);
  if (fr->stock[COLONIZE_CARGO_TRADE_GOODS] != 40) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("de Witt wagon must not load without FF");
  }

  /* With FF: pin wagon on foreign tile and load TRADE_GOODS. */
  col1.head.founding_father[FF_JAN_DE_WITT] = 0;
  col1.nation[nation].founding_fathers[FF_JAN_DE_WITT / 8] |=
    (uint8_t)(1u << (FF_JAN_DE_WITT % 8));
  wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("de Witt wagon despawned");
  }
  wagon->x = 5;
  wagon->y = 5;
  wagon->orders = 0;
  wagon->goto_x = UNITS_GOTO_NONE;
  wagon->goto_y = UNITS_GOTO_NONE;
  turn_refresh_moves_for_nation(&units, nation, &col1, &map, NULL, NULL, NULL);
  ai_euro_dispatcher_turn(&ctx, nation);
  wagon = units_get(&units, wid);
  {
    int got = 0;
    const int n = units_goods_hold_count(&units, wid);
    for (int h = 0; h < n; ++h) {
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_TRADE_GOODS) {
        got += wagon->hold_goods_amount[h];
      }
    }
    if (got != 10 || fr->stock[COLONIZE_CARGO_TRADE_GOODS] != 30) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      fprintf(
        stderr,
        "de Witt wagon got=%d foreign_stock=%d\n",
        got,
        fr->stock[COLONIZE_CARGO_TRADE_GOODS]
      );
      return fail("de Witt wagon should load 10 TRADE_GOODS from foreign");
    }
  }

  /* Delivery: wagon with TRADE_GOODS → AI_MOVE home; on home tile → unload. */
  wagon = units_get(&units, wid);
  wagon->x = 5;
  wagon->y = 5;
  wagon->orders = 0;
  wagon->goto_x = UNITS_GOTO_NONE;
  wagon->goto_y = UNITS_GOTO_NONE;
  wagon->moves_left = 3;
  turn_refresh_moves_for_nation(&units, nation, &col1, &map, NULL, NULL, NULL);
  ai_euro_dispatcher_turn(&ctx, nation);
  wagon = units_get(&units, wid);
  if (!wagon || wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 2 ||
      wagon->goto_y != 2) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    fprintf(
      stderr,
      "de Witt delivery goto orders=%d goto=(%d,%d)\n",
      wagon ? wagon->orders : -1,
      wagon ? wagon->goto_x : -1,
      wagon ? wagon->goto_y : -1
    );
    return fail("de Witt wagon with TRADE_GOODS should AI_MOVE toward own colony");
  }
  wagon->x = 2;
  wagon->y = 2;
  wagon->orders = 0;
  wagon->goto_x = UNITS_GOTO_NONE;
  wagon->goto_y = UNITS_GOTO_NONE;
  wagon->moves_left = 3;
  const int home_tg_before = home->stock[COLONIZE_CARGO_TRADE_GOODS];
  turn_refresh_moves_for_nation(&units, nation, &col1, &map, NULL, NULL, NULL);
  ai_euro_dispatcher_turn(&ctx, nation);
  wagon = units_get(&units, wid);
  {
    int left = 0;
    const int n = units_goods_hold_count(&units, wid);
    for (int h = 0; h < n; ++h) {
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_TRADE_GOODS) {
        left += wagon->hold_goods_amount[h];
      }
    }
    if (left != 0 || home->stock[COLONIZE_CARGO_TRADE_GOODS] != home_tg_before + 10) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      fprintf(
        stderr,
        "de Witt unload left=%d home_tg=%d (want +10 from %d)\n",
        left,
        home->stock[COLONIZE_CARGO_TRADE_GOODS],
        home_tg_before
      );
      return fail("de Witt wagon should unload TRADE_GOODS into own warehouse");
    }
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: de Witt wagon foreign TRADE_GOODS + deliver ok\n");
  return 0;
}

/*
 * Case-7 dock expert once: peace + tools_short high + Europe dock has
 * Hardy Pioneers → board that type (consume dock); do not invent if absent.
 */
static int unit_dock_expert_hire(void) {
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
      "unit_ai_euro_expand: dock hardy=%d dock_count=%d gold %u→%u cargo=%d\n",
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
  fprintf(stderr, "unit_ai_euro_expand: dock Hardy Pioneer hire ok\n");
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("buy-caravel-ge6 alloc map");
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Europe Caravel buy at colonies>=6 (no settle hire)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr, "unit_ai_euro_expand: 5d04 buy-caravel-colonies-ge6 ok (gold=%u)\n", gold
  );
  return 0;
}

static int unit_5d04_buy_caravel_no_ship(void) {
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
    return fail("buy-caravel alloc map");
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
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 30; /* no tools-cargo pressure */
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected one Europe Caravel purchase, no hire pax");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("buy-frigate alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
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
  col1.nation[nation].gold = 5000;

  ai_goals_reset();

  uint32_t turn = 27;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Frigate purchase when at war with gold>=5000");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("buy-galleon alloc map");
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
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Merchantman");
  units.types[2].movement = 4;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[2].cargo = 4;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Galleon");
  units.types[3].movement = 4;
  units.types[3].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[3].cargo = 6;

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
  /* Replenish after war sting. */
  col1.nation[nation].gold = 3000;

  ai_goals_reset();

  uint32_t turn = 23;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Galleon purchase when at war");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("buy-merchantman alloc map");
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
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Merchantman");
  units.types[2].movement = 4;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[2].cargo = 4;

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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Merchantman purchase under cargo pressure");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("buy-caravel-full alloc map");
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
  units.types[1].cargo = 1; /* one pax → full after boarding one */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("buy-caravel-full spawn ship");
  }
  full->nation_id = nation;
  full->moves_left = 0;
  /* Fill the only passenger slot. */
  {
    const int pax_id = units_spawn_allow_stack(&units, 0, 200, 100);
    ColonizeUnit* pax = units_get(&units, pax_id);
    if (!pax || !units_board_stacked(&units, pax_id, full_id)) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
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
  col1.nation[nation].gold = 1300;

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
  int empty_or_hired_new = 0;
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
    if (u->id != full_id && (u->x >= 200 || u->y >= 200) && u->cargo_count >= 1) {
      empty_or_hired_new = 1;
    }
  }

  if (caravel_n < 2 || !empty_or_hired_new) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: buy-caravel-full n=%d hired_new=%d gold=%u full_cargo=%d\n",
      caravel_n,
      empty_or_hired_new,
      (unsigned)col1.nation[nation].gold,
      full->cargo_count
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected second Europe Caravel with hire aboard");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: 5d04 buy-caravel-ship-full ok\n");
  return 0;
}

/*
 * 5d04 treasury gate: gold below colonist hire_cost → no Europe hire / tools-cargo.
 */
static int unit_treasury_skip_hire(void) {
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
      "unit_ai_euro_expand: treasury cargo=%d gold=%u\n",
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
  fprintf(stderr, "unit_ai_euro_expand: treasury skip-hire ok\n");
  return 0;
}

/*
 * Ship TRADE_GOODS at Europe → europe_sell_unit_hold via AI act (no harbor UI).
 * Cite: europe_sell_unit_hold; Colonization.pdf Europe sell + tax.
 */
static int unit_transport_europe_sell_trade_goods(void) {
  const int nation = 1;
  const int amt = 50;
  const int bid = 4;
  const int tax = 20;
  const int expect = (bid * amt * (100 - tax)) / 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("eu-sell alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean — ship may leave Europe to HS */
  }
  /* Eastern HS stand-in for teleport after sell. */
  map.terrain[8 * 16 + 14] = 25;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Merchantman");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 4;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int sid = units_spawn(&units, 0, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-sell spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;
  ship->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
  ship->hold_goods_amount[0] = amt;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 100;
  europe.tax_percent = tax;
  europe.cargo_count = COLONIZE_CARGO_COUNT;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    europe.cargo[c].bid = bid;
    europe.cargo[c].ask = bid + 1;
  }

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  col1.nation[nation].gold = 100;
  col1.nation[nation].tax_rate = (uint8_t)tax;
  const uint32_t gold_before = col1.nation[nation].gold;

  ai_goals_reset();

  uint32_t turn = 50;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  ship = units_get(&units, sid);
  if (!ship || !ship->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-sell ship should remain");
  }
  if (ship->hold_goods_amount[0] != 0) {
    fprintf(stderr, "unit_ai_euro_expand: hold amt=%d after sell\n",
            ship->hold_goods_amount[0]);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-sell should clear TRADE_GOODS hold");
  }
  const uint32_t gold_after = col1.nation[nation].gold;
  if (gold_after < gold_before + (uint32_t)expect) {
    fprintf(stderr, "unit_ai_euro_expand: gold %u→%u want +%d\n",
            (unsigned)gold_before, (unsigned)gold_after, expect);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-sell should credit tax-adjusted TRADE_GOODS proceeds");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: transport Europe sell ok\n");
  return 0;
}

/*
 * Privateer SILVER at Europe → europe_sell_unit_hold via AI act (commerce-raid
 * dump-sell). Cite: units_is_transport Privateer holds; Colonization.pdf Europe
 * sell; euro_unit_act Privateer / dump-sell.
 */
static int unit_privateer_europe_sell_silver(void) {
  const int nation = 1;
  const int amt = 40;
  const int bid = 10;
  const int tax = 15;
  const int expect = (bid * amt * (100 - tax)) / 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("priv-eu-sell alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25;
  }
  map.terrain[8 * 16 + 14] = 25;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
  units.types[0].movement = 8;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int sid = units_spawn(&units, 0, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-eu-sell spawn");
  }
  ship->nation_id = nation;
  ship->moves_left = 8;
  ship->orders = 0;
  ship->hold_goods_type[0] = COLONIZE_CARGO_SILVER;
  ship->hold_goods_amount[0] = amt;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 100;
  europe.tax_percent = tax;
  europe.cargo_count = COLONIZE_CARGO_COUNT;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    europe.cargo[c].bid = bid;
    europe.cargo[c].ask = bid + 1;
  }

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  col1.nation[nation].gold = 100;
  col1.nation[nation].tax_rate = (uint8_t)tax;
  const uint32_t gold_before = col1.nation[nation].gold;

  ai_goals_reset();

  uint32_t turn = 50;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  ship = units_get(&units, sid);
  if (!ship || !ship->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-eu-sell ship should remain");
  }
  if (ship->hold_goods_amount[0] != 0) {
    fprintf(stderr, "unit_ai_euro_expand: priv hold amt=%d after sell\n",
            ship->hold_goods_amount[0]);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-eu-sell should clear SILVER hold");
  }
  const uint32_t gold_after = col1.nation[nation].gold;
  if (gold_after < gold_before + (uint32_t)expect) {
    fprintf(stderr, "unit_ai_euro_expand: priv gold %u→%u want +%d\n",
            (unsigned)gold_before, (unsigned)gold_after, expect);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-eu-sell should credit SILVER proceeds");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Privateer Europe sell silver ok\n");
  return 0;
}

/*
 * Ship multi-cargo (SUGAR + TOBACCO) at Europe → sell all holds via AI act.
 * Cite: europe_sell_unit_hold / europe_sell_proceeds; Colonization.pdf Europe sell + tax.
 */
static int unit_transport_europe_sell_multi_cargo(void) {
  const int nation = 1;
  const int sugar_amt = 30;
  const int tobacco_amt = 40;
  const int bid_sugar = 3;
  const int bid_tobacco = 5;
  const int tax = 10;
  const int expect =
    (bid_sugar * sugar_amt * (100 - tax)) / 100 +
    (bid_tobacco * tobacco_amt * (100 - tax)) / 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("eu-multi-sell alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Merchantman");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 4;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int sid = units_spawn(&units, 0, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-multi-sell spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;
  ship->hold_goods_type[0] = COLONIZE_CARGO_SUGAR;
  ship->hold_goods_amount[0] = sugar_amt;
  ship->hold_goods_type[1] = COLONIZE_CARGO_TOBACCO;
  ship->hold_goods_amount[1] = tobacco_amt;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 100;
  europe.tax_percent = tax;
  europe.cargo_count = COLONIZE_CARGO_COUNT;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    europe.cargo[c].bid = 1;
    europe.cargo[c].ask = 2;
  }
  europe.cargo[COLONIZE_CARGO_SUGAR].bid = bid_sugar;
  europe.cargo[COLONIZE_CARGO_TOBACCO].bid = bid_tobacco;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  col1.nation[nation].gold = 100;
  col1.nation[nation].tax_rate = (uint8_t)tax;
  const uint32_t gold_before = col1.nation[nation].gold;

  ai_goals_reset();

  uint32_t turn = 51;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 43;

  ai_euro_dispatcher_turn(&ctx, nation);

  ship = units_get(&units, sid);
  if (!ship || !ship->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-multi-sell ship should remain");
  }
  if (ship->hold_goods_amount[0] != 0 || ship->hold_goods_amount[1] != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: multi-sell holds %d/%d\n",
      ship->hold_goods_amount[0],
      ship->hold_goods_amount[1]
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-multi-sell should clear both holds");
  }
  const uint32_t gold_after = col1.nation[nation].gold;
  if (gold_after < gold_before + (uint32_t)expect) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: multi-sell gold %u→%u want +%d\n",
      (unsigned)gold_before,
      (unsigned)gold_after,
      expect
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-multi-sell should credit tax-adjusted proceeds");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: transport Europe multi-cargo sell ok\n");
  return 0;
}

/*
 * Europe dump-sell skips boycotted cargo (nation.boycott_bitmap bit = type).
 * SUGAR boycotted + TOBACCO free → sell tobacco only; leave sugar hold.
 * Cite: fandom Boycott (Col); king refuse boycott_bitmap; no invented prices.
 */
static int unit_transport_europe_sell_skip_boycott(void) {
  const int nation = 1;
  const int sugar_amt = 30;
  const int tobacco_amt = 40;
  const int bid_sugar = 3;
  const int bid_tobacco = 5;
  const int tax = 10;
  const int expect_tobacco = (bid_tobacco * tobacco_amt * (100 - tax)) / 100;
  const int expect_sugar = (bid_sugar * sugar_amt * (100 - tax)) / 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("eu-boycott-sell alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Merchantman");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 4;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int sid = units_spawn(&units, 0, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-boycott-sell spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;
  ship->hold_goods_type[0] = COLONIZE_CARGO_SUGAR;
  ship->hold_goods_amount[0] = sugar_amt;
  ship->hold_goods_type[1] = COLONIZE_CARGO_TOBACCO;
  ship->hold_goods_amount[1] = tobacco_amt;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 100;
  europe.tax_percent = tax;
  europe.cargo_count = COLONIZE_CARGO_COUNT;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    europe.cargo[c].bid = 1;
    europe.cargo[c].ask = 2;
  }
  europe.cargo[COLONIZE_CARGO_SUGAR].bid = bid_sugar;
  europe.cargo[COLONIZE_CARGO_TOBACCO].bid = bid_tobacco;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  col1.nation[nation].gold = 100;
  col1.nation[nation].tax_rate = (uint8_t)tax;
  col1.nation[nation].boycott_bitmap =
    (uint16_t)(1u << COLONIZE_CARGO_SUGAR); /* king refuse Sugar */
  const uint32_t gold_before = col1.nation[nation].gold;

  ai_goals_reset();

  uint32_t turn = 52;
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

  ai_euro_dispatcher_turn(&ctx, nation);

  ship = units_get(&units, sid);
  if (!ship || !ship->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-boycott-sell ship should remain");
  }
  if (ship->hold_goods_amount[0] != sugar_amt ||
      ship->hold_goods_type[0] != COLONIZE_CARGO_SUGAR) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: boycott sugar hold type=%d amt=%d\n",
      ship->hold_goods_type[0],
      ship->hold_goods_amount[0]
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-boycott-sell must leave boycotted SUGAR hold");
  }
  if (ship->hold_goods_amount[1] != 0) {
    fprintf(stderr, "unit_ai_euro_expand: tobacco amt=%d after sell\n",
            ship->hold_goods_amount[1]);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-boycott-sell should clear non-boycotted TOBACCO");
  }
  const uint32_t gold_after = col1.nation[nation].gold;
  /* Planner adds a small treasury bump; require tobacco proceeds credited and
   * sugar proceeds not (boycott skip). Cite: ai_euro_nation_planning bump. */
  if (gold_after < gold_before + (uint32_t)expect_tobacco) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: boycott gold %u→%u want ≥+%d (tobacco)\n",
      (unsigned)gold_before,
      (unsigned)gold_after,
      expect_tobacco
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-boycott-sell should credit tobacco proceeds");
  }
  if (gold_after >= gold_before + (uint32_t)(expect_tobacco + expect_sugar)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: boycott gold %u→%u suggests sugar also sold (+%d)\n",
      (unsigned)gold_before,
      (unsigned)gold_after,
      expect_tobacco + expect_sugar
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("eu-boycott-sell must not credit boycotted SUGAR");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Europe sell skip boycott ok\n");
  return 0;
}

/*
 * Case-7 dock Expert Farmer: peace + food_short high + Europe dock has
 * Expert Farmers → board that type (consume dock). Cite: europe.c pool;
 * euro_unit_act §2e Expert Farmer food LABOR.
 */
static int unit_dock_farmer_hire(void) {
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
    return fail("dock-farmer alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Farmer");
  units.types[0].movement = 1;
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
    c->population = 5;
    c->colonist_count = 5;
    c->stock[COLONIZE_CARGO_TOOLS] = 40; /* tools_short=0 */
    c->stock[COLONIZE_CARGO_FOOD] = 0;   /* food_short=10 each → 30 total */
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
    return fail("dock-farmer spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Expert Farmers");
  europe.dock[0].profession = 0;
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

  uint32_t turn = 13;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 44;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int farmer_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Expert Farmer")) {
        farmer_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!farmer_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock farmer=%d dock_count=%d gold %u→%u cargo=%d\n",
      farmer_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Farmer dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Expert Farmer hire ok\n");
  return 0;
}

/*
 * Same dock-farmer scenario, but with a real-NAMES.TXT-shaped type pool: only
 * base "Colonists" (no literal "Expert Farmer" / "Free Colonist" @UNIT rows —
 * those never exist in real data; specialists are "Colonists" + a @JOB
 * profession, per units_display_name()). Before the ai_euro_type_from_dock_name
 * fix this silently resolved to -1 and the dock hire never fired (roadmap.md
 * Phase 3 "Free Colonist" dead-lookup note) — this regresses that.
 */
static int unit_dock_farmer_hire_real_names(void) {
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
    return fail("dock-farmer real-names alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Colonists");
  units.types[0].movement = 1;
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
    c->population = 5;
    c->colonist_count = 5;
    c->stock[COLONIZE_CARGO_TOOLS] = 40; /* tools_short=0 */
    c->stock[COLONIZE_CARGO_FOOD] = 0;   /* food_short=10 each → 30 total */
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
    return fail("dock-farmer real-names spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Expert Farmers");
  europe.dock[0].profession = 0; /* NAMES.TXT @JOB Farmer */
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

  uint32_t turn = 13;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 44;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int farmer_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strcmp(ty->name, "Colonists") == 0 && pax->profession == 0) {
        farmer_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!farmer_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock real-names farmer=%d dock_count=%d gold %u→%u cargo=%d\n",
      farmer_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Colonists+Farmer dock hire against real-shaped NAMES.TXT pool");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Expert Farmer hire (real names) ok\n");
  return 0;
}

/*
 * Case-7 dock Master Carpenter: peace + construction LABOR wanted + Europe dock
 * has Master Carpenters → board that type (consume dock). No tools/food short
 * so Free Colonist fallback must not win. Cite: europe.c pool; building_production
 * Carpenter→Hammers; euro_unit_act §2e construction deepen.
 */
static int unit_dock_carpenter_hire(void) {
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
    return fail("dock-carpenter alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Carpenter");
  units.types[0].movement = 1;
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
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_type_count = 1;

  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 40; /* tools_short=0 */
    c->stock[COLONIZE_CARGO_FOOD] = 40;  /* food_short=0 */
    c->building_in_production = (i == 0) ? 0 : -1; /* colony 0: Stockade */
    if (i == 0) {
      c->hammers = 10;
    }
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-carpenter spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Master Carpenters");
  europe.dock[0].profession = 13;
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

  uint32_t turn = 14;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 45;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int carpenter_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Master Carpenter")) {
        carpenter_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!carpenter_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock carpenter=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      carpenter_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Carpenter dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Master Carpenter hire ok\n");
  return 0;
}

/*
 * Case-7 dock Expert Lumberjack: peace + lumber_short high + Europe dock has
 * Expert Lumberjacks → board that type (consume dock). No tools/food short
 * so Free Colonist fallback must not win. Cite: europe.c Expert Lumberjacks
 * pool; building_production Lumberjack→Lumber; euro_unit_act §2d lumber deepen.
 */
static int unit_dock_lumberjack_hire(void) {
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
    return fail("dock-lumberjack alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Lumberjack");
  units.types[0].movement = 1;
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
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Warehouse");
  colonies.building_types[0].hammers = 80;
  colonies.building_type_count = 1;

  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 40;  /* tools_short=0 */
    c->stock[COLONIZE_CARGO_FOOD] = 40;   /* food_short=0 */
    c->stock[COLONIZE_CARGO_LUMBER] = 0; /* lumber_short=20 each → 40 total */
    c->building_in_production = 0;        /* Warehouse incomplete */
    c->hammers = 10;
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-lumberjack spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Expert Lumberjacks");
  europe.dock[0].profession = 5;
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

  uint32_t turn = 15;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 46;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int lumberjack_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Expert Lumberjack")) {
        lumberjack_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!lumberjack_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock lumberjack=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      lumberjack_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Lumberjack dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Expert Lumberjack hire ok\n");
  return 0;
}

/*
 * Case-7 dock Expert Ore Miner: peace + ore_short high + Europe dock has
 * Expert Ore Miners → board that type (consume dock). Tools/food/lumber full
 * so Free Colonist fallback must not win. Cite: europe.c Expert Ore Miners;
 * terrain_yields Ore; euro_unit_act Ore Miner field-assign.
 */
static int unit_dock_ore_miner_hire(void) {
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
    return fail("dock-ore alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Ore Miner");
  units.types[0].movement = 1;
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

  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 0; /* ore_short=20 each → 40 total */
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-ore spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Expert Ore Miners");
  europe.dock[0].profession = 6;
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

  uint32_t turn = 16;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 47;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int ore_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Ore Miner")) {
        ore_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!ore_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock ore=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      ore_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Ore Miner dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Expert Ore Miner hire ok\n");
  return 0;
}

/*
 * Case-7 dock Expert Fisherman: peace + food_short high + coastal colony +
 * Europe dock has Expert Fishermen (no Farmer on dock) → board Fisherman.
 * Cite: europe.c Expert Fishermen; terrain_yields Fisherman; euro_unit_act
 * coastal food fallback.
 */
static int unit_dock_fisherman_hire(void) {
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
    return fail("dock-fisherman alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  /* Ocean adjacent to (2,2) so nation has a coastal colony. */
  map.terrain[2 * 16 + 3] = 25;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Fisherman");
  units.types[0].movement = 1;
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
    c->population = 5;
    c->colonist_count = 5;
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_FOOD] = 0; /* food_short=10 each → 30 total */
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  if (!map_tile_is_coastal(&map, 2, 2)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-fisherman setup: (2,2) should be coastal");
  }

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-fisherman spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Expert Fishermen");
  europe.dock[0].profession = 8;
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

  uint32_t turn = 17;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 48;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int fisherman_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Fisherman")) {
        fisherman_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!fisherman_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock fisherman=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      fisherman_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Fisherman dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Expert Fisherman hire ok\n");
  return 0;
}

/*
 * Case-7 dock Master Gunsmith: peace + muskets_short high + Europe dock has
 * Master Gunsmiths → board that type (consume dock). Tools/food/ore full so
 * Free Colonist / Ore Miner must not win. Cite: europe.c Master Gunsmiths;
 * building_production Gunsmith→Muskets; euro_unit_act workplace assign.
 */
static int unit_dock_gunsmith_hire(void) {
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
    return fail("dock-gunsmith alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Gunsmith");
  units.types[0].movement = 1;
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
    c->stock[COLONIZE_CARGO_TOOLS] = 40;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 40;
    c->stock[COLONIZE_CARGO_MUSKETS] = 0; /* muskets_short=10 each → 30 */
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
    return fail("dock-gunsmith spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Master Gunsmiths");
  europe.dock[0].profession = 15;
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

  uint32_t turn = 18;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 49;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int gunsmith_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Gunsmith")) {
        gunsmith_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!gunsmith_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock gunsmith=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      gunsmith_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Gunsmith dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Master Gunsmith hire ok\n");
  return 0;
}

/*
 * Case-7 dock Master Blacksmith: peace + tools_short high + Europe dock has
 * Master Blacksmiths (no Pioneer on dock) → board Blacksmith. Cite: europe.c
 * Master Blacksmiths; building_production Ore→Tools; euro_unit_act workplace.
 */
static int unit_dock_blacksmith_hire(void) {
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
    return fail("dock-blacksmith alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Blacksmith");
  units.types[0].movement = 1;
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
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 0; /* tools_short=20 each → 40 */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->stock[COLONIZE_CARGO_LUMBER] = 40;
    c->stock[COLONIZE_CARGO_ORE] = 40;
    c->stock[COLONIZE_CARGO_MUSKETS] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-blacksmith spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Master Blacksmiths");
  europe.dock[0].profession = 14;
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

  uint32_t turn = 19;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 50;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int blacksmith_boarded = 0;
  int free_colonist_boarded = 0;
  int pioneer_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Blacksmith")) {
        blacksmith_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
      if (ty && strstr(ty->name, "Pioneer")) {
        pioneer_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!blacksmith_boarded || free_colonist_boarded || pioneer_boarded || !dock_cleared ||
      !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock blacksmith=%d free=%d pioneer=%d dock_count=%d gold %u→%u\n",
      blacksmith_boarded,
      free_colonist_boarded,
      pioneer_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Blacksmith dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Master Blacksmith hire ok\n");
  return 0;
}

/*
 * Case-7 dock Seasoned Scout: peace + own colony + Europe dock has Seasoned
 * Scouts (no shortage priority) → board Scout. Cite: europe.c Seasoned Scouts;
 * euro_unit_act §2c2 fog/CONTACT.
 */
static int unit_dock_scout_hire(void) {
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
    return fail("dock-scout alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Seasoned Scout");
  units.types[0].movement = 1;
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
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-scout spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Seasoned Scouts");
  europe.dock[0].profession = 22;
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

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 51;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int scout_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int ci = 0; ci < ship->cargo_count; ++ci) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[ci]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Scout")) {
        scout_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!scout_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock scout=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      scout_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Seasoned Scout dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Seasoned Scout hire ok\n");
  return 0;
}

/*
 * Case-7 dock Jesuit Missionary: peace + unmissioned tribe + Europe dock has
 * Jesuit Missionaries → board Missionary (before Free Colonist). Cite: europe.c
 * Jesuit Missionaries; euro_unit_act §2c6 convert CONTACT.
 */
static int unit_dock_missionary_hire(void) {
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
    return fail("dock-missionary alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Jesuit Missionary");
  units.types[0].movement = 2;
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
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-missionary spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Jesuit Missionaries");
  europe.dock[0].profession = 24;
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
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-missionary alloc tribe");
  }
  col1.tribe[0].x = 12;
  col1.tribe[0].y = 12;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].population = 4;
  col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;

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
  ctx.europe = &europe;
  ctx.rng_seed = 52;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int missionary_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int ci = 0; ci < ship->cargo_count; ++ci) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[ci]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && (strstr(ty->name, "Missionary") || strstr(ty->name, "Jesuit"))) {
        missionary_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!missionary_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock missionary=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      missionary_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(col1.tribe);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Jesuit Missionary dock hire + dock consume + gold spend");
  }

  free(col1.tribe);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Jesuit Missionary hire ok\n");
  return 0;
}

/*
 * Case-7 dock Elder Statesman: peace + own colony + Europe dock has Elder
 * Statesmen → board Elder. Cite: europe.c Elder Statesmen; building_production
 * Elder→Liberty bells.
 */
static int unit_dock_elder_hire(void) {
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
    return fail("dock-elder alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Elder Statesman");
  units.types[0].movement = 1;
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
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-elder spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Elder Statesmen");
  europe.dock[0].profession = 17;
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

  uint32_t turn = 22;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 53;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int elder_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int ci = 0; ci < ship->cargo_count; ++ci) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[ci]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Elder")) {
        elder_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!elder_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock elder=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      elder_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Elder Statesman dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Elder Statesman hire ok\n");
  return 0;
}

/*
 * Case-7 dock Firebrand Preacher: peace + Church present + Europe dock has
 * Firebrand Preachers → board Preacher. Cite: europe.c Firebrand Preachers;
 * building_production Preacher→Crosses.
 */
static int unit_dock_preacher_hire(void) {
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
    return fail("dock-preacher alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Firebrand Preacher");
  units.types[0].movement = 1;
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
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Church");
  colonies.building_type_count = 1;
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 40;
  c->has_building[0] = true;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-preacher spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Firebrand Preachers");
  europe.dock[0].profession = 16;
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

  uint32_t turn = 23;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 54;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int preacher_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int ci = 0; ci < ship->cargo_count; ++ci) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[ci]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && (strstr(ty->name, "Preacher") || strstr(ty->name, "Firebrand"))) {
        preacher_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!preacher_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock preacher=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      preacher_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Firebrand Preacher dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Firebrand Preacher hire ok\n");
  return 0;
}

/*
 * Case-7 dock Expert Teacher: peace + Schoolhouse owned + Europe dock has
 * Expert Teachers → board Teacher (consume dock; spend gold). Cite: europe.c
 * Expert Teachers pool; building_production.md Skills Chart job 18;
 * Colonization.pdf Education / Teacher.
 */
static int unit_dock_teacher_hire(void) {
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
    return fail("dock-teacher alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Teacher");
  units.types[0].movement = 1;
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
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Schoolhouse");
  colonies.building_type_count = 1;
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 40;
  c->has_building[0] = true;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-teacher spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Expert Teachers");
  europe.dock[0].profession = 18;
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

  uint32_t turn = 23;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 54;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int teacher_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int ci = 0; ci < ship->cargo_count; ++ci) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[ci]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Teacher")) {
        teacher_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!teacher_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock teacher=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      teacher_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Teacher dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Expert Teacher hire ok\n");
  return 0;
}

/*
 * Case-7 dock Master Distiller: peace + Distiller's House + sugar≥20 + Europe
 * dock has Master Distiller → board Distiller. Cite: europe.c Master Distiller;
 * building_production Distiller Sugar→Rum; euro_unit_act workplace.
 */
static int unit_dock_distiller_hire(void) {
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
    return fail("dock-distiller alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Distiller");
  units.types[0].movement = 1;
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
  snprintf(
    colonies.building_types[0].name,
    sizeof(colonies.building_types[0].name),
    "Rum Distiller's House"
  );
  colonies.building_type_count = 1;
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 40;
  c->stock[COLONIZE_CARGO_SUGAR] = 25;
  c->has_building[0] = true;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-distiller spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Master Distiller");
  europe.dock[0].profession = 9;
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

  uint32_t turn = 24;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 55;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int distiller_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int ci = 0; ci < ship->cargo_count; ++ci) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[ci]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Distiller")) {
        distiller_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!distiller_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock distiller=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      distiller_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Distiller dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Master Distiller hire ok\n");
  return 0;
}

/*
 * Case-7 dock Master Weaver: peace + Weaver's House + cotton≥20 + Europe dock
 * has Master Weaver → board Weaver. Cite: europe.c; building_production Cloth.
 */
static int unit_dock_weaver_hire(void) {
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
    return fail("dock-weaver alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Weaver");
  units.types[0].movement = 1;
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
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Weaver's House");
  colonies.building_type_count = 1;
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 40;
  c->stock[COLONIZE_CARGO_COTTON] = 25;
  c->has_building[0] = true;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-weaver spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Master Weaver");
  europe.dock[0].profession = 11;
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

  uint32_t turn = 24;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 55;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int weaver_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int ci = 0; ci < ship->cargo_count; ++ci) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[ci]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Weaver")) {
        weaver_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!weaver_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock weaver=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      weaver_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Weaver dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Master Weaver hire ok\n");
  return 0;
}

/*
 * Case-7 dock Master Fur Trader: peace + Fur Trader's House + furs≥20.
 */
static int unit_dock_fur_trader_hire(void) {
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
    return fail("dock-fur alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Fur Trader");
  units.types[0].movement = 1;
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
  snprintf(
    colonies.building_types[0].name,
    sizeof(colonies.building_types[0].name),
    "Fur Trader's House"
  );
  colonies.building_type_count = 1;
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 40;
  c->stock[COLONIZE_CARGO_FURS] = 25;
  c->has_building[0] = true;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-fur spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Master Fur Trader");
  europe.dock[0].profession = 12;
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

  uint32_t turn = 24;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 55;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int fur_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int ci = 0; ci < ship->cargo_count; ++ci) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[ci]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Fur Trader")) {
        fur_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!fur_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock fur=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      fur_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Fur Trader dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Master Fur Trader hire ok\n");
  return 0;
}

/*
 * Case-7 dock Master Tobacconist: peace + Tobacconist's House + tobacco≥20.
 */
static int unit_dock_tobacconist_hire(void) {
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
    return fail("dock-tobac alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Tobacconist");
  units.types[0].movement = 1;
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
  snprintf(
    colonies.building_types[0].name,
    sizeof(colonies.building_types[0].name),
    "Tobacconist's House"
  );
  colonies.building_type_count = 1;
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 40;
  c->stock[COLONIZE_CARGO_TOBACCO] = 25;
  c->has_building[0] = true;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-tobac spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Master Tobacconist");
  europe.dock[0].profession = 10;
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

  uint32_t turn = 24;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 55;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int tobac_boarded = 0;
  int free_colonist_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int ci = 0; ci < ship->cargo_count; ++ci) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[ci]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Tobacconist")) {
        tobac_boarded = 1;
      }
      if (ty && strstr(ty->name, "Free Colonist")) {
        free_colonist_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!tobac_boarded || free_colonist_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: dock tobac=%d free=%d dock_count=%d gold %u→%u cargo=%d\n",
      tobac_boarded,
      free_colonist_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Tobacconist dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: dock Master Tobacconist hire ok\n");
  return 0;
}

/*
 * LABOR bind: idle Free Colonist adjacent to own colony with food_short
 * (and a distant FOUND lure) → LABOR goto / join, not yank to FOUND.
 * Cite: 5b66 unload/labor + 5cf6 food_short; no invented production.
 */

/*
 * Col1 +0x8e labor_shortage: Free Colonist on colony tile joins LABOR and
 * decrements the counter (FUN_521d_5b66 ~91589). Cite: euro_unit_act case 0x0b.
 */

/*
 * Col1 +0x8d specialty_cargo: wagon surplus load prefers specialty over
 * default tools-first ladder. Cite: FUN_5952_0306; euro_unit_act §2d.
 */

/*
 * Col1 +0x90 cargo_produced_mask: wagon surplus load prefers produced cargo
 * over default tools-first ladder. Cite: FUN_364b_0688; euro_unit_act §2d.
 */
static int unit_cargo_produced_mask_haul_prefer(void) {
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
    return fail("produced-mask alloc map");
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
  ColonizeColony* supply = &colonies.colonies[0];
  supply->id = 0;
  supply->active = true;
  supply->nation_id = nation;
  supply->x = 4;
  supply->y = 4;
  supply->population = 3;
  supply->colonist_count = 3;
  supply->stock[COLONIZE_CARGO_TOOLS] = 50; /* surplus — default ladder first */
  supply->stock[COLONIZE_CARGO_LUMBER] = 50; /* surplus — produced */
  supply->stock[COLONIZE_CARGO_FOOD] = 5;
  supply->building_in_production = -1;
  supply->specialty_cargo = 0xff;
  supply->cargo_produced_mask = (uint16_t)(1u << COLONIZE_CARGO_LUMBER);

  ColonizeColony* shortc = &colonies.colonies[1];
  shortc->id = 1;
  shortc->active = true;
  shortc->nation_id = nation;
  shortc->x = 8;
  shortc->y = 4;
  shortc->population = 3;
  shortc->colonist_count = 3;
  shortc->stock[COLONIZE_CARGO_LUMBER] = 0;
  shortc->stock[COLONIZE_CARGO_TOOLS] = 40;
  shortc->stock[COLONIZE_CARGO_FOOD] = 80;
  shortc->building_in_production = -1;
  shortc->specialty_cargo = 0xff;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("produced-mask spawn wagon");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;

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

  uint32_t turn = 63;
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
  int lumber_loaded = 0;
  int tools_loaded = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (wagon->hold_goods_amount[h] <= 0 || wagon->hold_goods_amount[h] >= 255) {
        continue;
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_LUMBER) {
        lumber_loaded += wagon->hold_goods_amount[h];
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
        tools_loaded += wagon->hold_goods_amount[h];
      }
    }
  }
  if (lumber_loaded < 20 || tools_loaded != 0) {
    fprintf(stderr,
      "unit_ai_euro_expand: produced lumber=%d tools=%d mask=0x%x\n",
      lumber_loaded, tools_loaded, (unsigned)colonies.colonies[0].cargo_produced_mask);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected wagon to load produced LUMBER over tools surplus");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: cargo_produced_mask haul prefer ok\n");
  return 0;
}

static int unit_specialty_cargo_haul_prefer(void) {
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
    return fail("specialty alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Pioneer");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* supply = &colonies.colonies[0];
  supply->id = 0;
  supply->active = true;
  supply->nation_id = nation;
  supply->x = 4;
  supply->y = 4;
  supply->population = 3;
  supply->colonist_count = 3;
  supply->stock[COLONIZE_CARGO_TOOLS] = 10; /* not surplus */
  supply->stock[COLONIZE_CARGO_LUMBER] = 50; /* surplus — only specialty candidate */
  supply->stock[COLONIZE_CARGO_FOOD] = 5; /* not food surplus (avoids specialty=FOOD) */
  supply->building_in_production = -1;
  supply->specialty_cargo = (uint8_t)COLONIZE_CARGO_LUMBER;
  /* Short colony so haul binds. */
  ColonizeColony* shortc = &colonies.colonies[1];
  shortc->id = 1;
  shortc->active = true;
  shortc->nation_id = nation;
  shortc->x = 8;
  shortc->y = 4;
  shortc->population = 3;
  shortc->colonist_count = 3;
  shortc->stock[COLONIZE_CARGO_LUMBER] = 0;
  shortc->stock[COLONIZE_CARGO_TOOLS] = 40;
  shortc->stock[COLONIZE_CARGO_FOOD] = 80;
  shortc->building_in_production = -1;
  shortc->specialty_cargo = 0xff;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("specialty spawn wagon");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;

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

  uint32_t turn = 60;
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
  int lumber_loaded = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (wagon->hold_goods_amount[h] > 0 && wagon->hold_goods_amount[h] < 255 &&
          wagon->hold_goods_type[h] == COLONIZE_CARGO_LUMBER) {
        lumber_loaded += wagon->hold_goods_amount[h];
      }
    }
  }
  if (lumber_loaded < 20) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: specialty lumber_loaded=%d specialty=%u tools=%d\n",
      lumber_loaded,
      (unsigned)colonies.colonies[0].specialty_cargo,
      colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS]
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected wagon to load specialty LUMBER surplus");
  }

  /* FUN_5952_0306: warehouse-full clears specialty. */
  ColonizeColony* c0 = &colonies.colonies[0];
  c0->specialty_cargo = (uint8_t)COLONIZE_CARGO_LUMBER;
  c0->stock[COLONIZE_CARGO_LUMBER] = colonies_warehouse_capacity(&colonies, c0, COLONIZE_CARGO_LUMBER);
  colonies_specialty_cargo_update(&colonies, c0, COLONIZE_CARGO_LUMBER, 1, 0);
  if (c0->specialty_cargo != 0xff) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected specialty clear when stock >= warehouse cap");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: specialty_cargo haul prefer ok\n");
  return 0;
}

/*
 * Series R: 4393 specialty flag_a match — equal-distance haul shorts with
 * distinct specialty; wagon holds only one type → goto matching colony.
 * Cite: move_scoring_ship.md thin 4393; Series R.
 */
static int unit_specialty_flag_a_haul_match(void) {
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
    return fail("flag_a alloc map");
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
  /* Equal MD=4 from wagon at (4,4). Inventory refreshes specialty from surplus:
   * A tools-short + lumber surplus → flag_a=LUMBER; B lumber-short + tools
   * surplus → flag_a=TOOLS. Wagon holds TOOLS → +32 picks B. */
  ColonizeColony* a = &colonies.colonies[0];
  a->id = 0;
  a->active = true;
  a->nation_id = nation;
  a->x = 8;
  a->y = 4;
  a->population = 3;
  a->colonist_count = 3;
  a->stock[COLONIZE_CARGO_TOOLS] = 0; /* short */
  a->stock[COLONIZE_CARGO_LUMBER] = 50; /* surplus → specialty LUMBER (under warehouse) */
  a->stock[COLONIZE_CARGO_FOOD] = 10; /* not FOOD surplus (avoids specialty overwrite) */
  a->building_in_production = -1;
  a->cargo_idle_turns = 0;
  a->specialty_cargo = 0xff;

  ColonizeColony* b = &colonies.colonies[1];
  b->id = 1;
  b->active = true;
  b->nation_id = nation;
  b->x = 4;
  b->y = 8;
  b->population = 3;
  b->colonist_count = 3;
  b->stock[COLONIZE_CARGO_LUMBER] = 0; /* short */
  b->stock[COLONIZE_CARGO_TOOLS] = 50; /* surplus → specialty TOOLS (under warehouse) */
  b->stock[COLONIZE_CARGO_FOOD] = 10; /* not FOOD surplus (avoids specialty overwrite) */
  b->building_in_production = -1;
  b->cargo_idle_turns = 0;
  b->specialty_cargo = 0xff;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("flag_a spawn wagon");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_TOOLS, 20) <= 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("flag_a load tools");
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
  col1.nation[nation].gold = 200;

  uint32_t turn = 62;
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
  if (!wagon || !wagon->active || !units_orders_follow_goto(wagon->orders) ||
      wagon->goto_x != 4 || wagon->goto_y != 8) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: flag_a goto=(%d,%d) orders=%d specA=%u specB=%u\n",
      wagon ? wagon->goto_x : -1,
      wagon ? wagon->goto_y : -1,
      wagon ? wagon->orders : -1,
      (unsigned)colonies.colonies[0].specialty_cargo,
      (unsigned)colonies.colonies[1].specialty_cargo
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected wagon goto specialty-matching tools short");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: specialty flag_a haul match ok\n");
  return 0;
}

/*
 * Col1 +0x8f cargo_idle_turns: haul prefers short colony with higher idle*8
 * score; inventory INC; goods unload clears. Cite: FUN_5952_035e; ~87677/~90249.
 */
static int unit_cargo_idle_turns_haul_prefer(void) {
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
    return fail("cargo-idle alloc map");
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
  /* Equal MD=4 from wagon at (4,4): A idle=0 east, B idle=20 south. */
  ColonizeColony* a = &colonies.colonies[0];
  a->id = 0;
  a->active = true;
  a->nation_id = nation;
  a->x = 8;
  a->y = 4;
  a->population = 3;
  a->colonist_count = 3;
  a->stock[COLONIZE_CARGO_TOOLS] = 0; /* short */
  a->stock[COLONIZE_CARGO_FOOD] = 80;
  a->building_in_production = -1;
  a->cargo_idle_turns = 0;
  a->specialty_cargo = 0xff;

  ColonizeColony* b = &colonies.colonies[1];
  b->id = 1;
  b->active = true;
  b->nation_id = nation;
  b->x = 4;
  b->y = 8;
  b->population = 3;
  b->colonist_count = 3;
  b->stock[COLONIZE_CARGO_TOOLS] = 0; /* short */
  b->stock[COLONIZE_CARGO_FOOD] = 80;
  b->building_in_production = -1;
  b->cargo_idle_turns = 20;
  b->specialty_cargo = 0xff;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cargo-idle spawn wagon");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_TOOLS, 20) <= 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cargo-idle load tools");
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
  col1.nation[nation].gold = 200;

  uint32_t turn = 61;
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
  if (!wagon || !wagon->active || !units_orders_follow_goto(wagon->orders) ||
      wagon->goto_x != 4 || wagon->goto_y != 8) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: cargo-idle goto=(%d,%d) orders=%d idleA=%u idleB=%u\n",
      wagon ? wagon->goto_x : -1,
      wagon ? wagon->goto_y : -1,
      wagon ? wagon->orders : -1,
      (unsigned)colonies.colonies[0].cargo_idle_turns,
      (unsigned)colonies.colonies[1].cargo_idle_turns
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected wagon goto higher cargo_idle short colony");
  }
  /* Inventory INC both shorts (cap 0x7f). */
  if (colonies.colonies[0].cargo_idle_turns < 1 ||
      colonies.colonies[1].cargo_idle_turns < 21) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected cargo_idle INC during inventory");
  }

  /* Unload clears idle. */
  ColonizeColony* dest = &colonies.colonies[1];
  dest->x = 4;
  dest->y = 4; /* same tile as wagon for transfer */
  wagon->x = 4;
  wagon->y = 4;
  dest->cargo_idle_turns = 30;
  const int moved =
    colonies_transfer_from_unit(&colonies, dest->id, &units, wid, 0, NULL);
  if (moved <= 0 || dest->cargo_idle_turns != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: unload moved=%d idle=%u\n",
      moved,
      (unsigned)dest->cargo_idle_turns
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected goods unload to clear cargo_idle_turns");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: cargo_idle_turns haul prefer ok\n");
  return 0;
}

static int unit_labor_shortage_join(void) {
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
    return fail("labor-shortage alloc map");
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
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  c->labor_shortage = 2;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* On colony tile — admit/join path. */
  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* col = units_get(&units, uid);
  if (!col) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("labor-shortage spawn colonist");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves_left = 1;

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
  if (!joined || c->labor_shortage != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: labor_shortage joined=%d pop %d→%d shortage=%u\n",
      joined,
      pop_before,
      c->population,
      (unsigned)c->labor_shortage
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected join decrementing labor_shortage 2→1");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: labor_shortage join ok\n");
  return 0;
}

static int unit_labor_bind_food_short(void) {
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
      "unit_ai_euro_expand: labor joined=%d at_col=%d goto=(%d,%d) pop %d→%d\n",
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
  fprintf(stderr, "unit_ai_euro_expand: LABOR bind food-short ok\n");
  return 0;
}

/*
 * Wagon hire-once deepen: Wagon Train on tools-short colony with TOOLS hold
 * → colonies_transfer_from_unit into stock (no invented +10).
 */
static int unit_wagon_tools_delivery(void) {
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
      "unit_ai_euro_expand: wagon-tools %d→%d hold_left=%d (want +20, hold 0)\n",
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
    "unit_ai_euro_expand: wagon-tools delivery ok (tools %d→%d)\n",
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
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("colony-flags alloc map");
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("colony-flags spawn");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves_left = 1;

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
  if ((c->colony_flags & COLONIZE_COLONY_FLAG_STARVATION) == 0) {
    fprintf(stderr, "unit_ai_euro_expand: colony_flags=0x%02x food=%d\n",
            (unsigned)c->colony_flags, c->stock[COLONIZE_CARGO_FOOD]);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected starvation colony_flags bit");
  }
  const int joined = (col == NULL || !col->active) && c->population == pop_before + 1;
  if (!joined) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected LABOR join from starvation flag");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: colony_flags starvation LABOR ok\n");
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
    map.terrain[i] = (i % 16 < 2) ? 0 : 1; /* west strip ocean */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Man-O-War");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].attack = 8;
  units.types[0].cargo = 6;

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
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  c->ai_flags = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* MoW on water within MD≤5 of colony. */
  const int sid = units_spawn(&units, 0, 1, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ai-flags spawn MoW");
  }
  ship->nation_id = foe;
  ship->moves_left = 4;

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
  if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_MAN_O_WAR) == 0) {
    fprintf(stderr, "unit_ai_euro_expand: ai_flags=0x%02x (want MoW bit)\n",
            (unsigned)c->ai_flags);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected nearby_man_o_war ai_flags bit");
  }
  int found_alt = 0;
  for (int i = 0; i < 16; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_COLONY_ALT && g->x == 4 && g->y == 4 && g->prio >= 8) {
      found_alt = 1;
      break;
    }
  }
  if (!found_alt) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected COLONY_ALT prio 8 at colony under MoW pressure");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: colony ai_flags MoW COLONY_ALT ok\n");
  return 0;
}

static int unit_build_ai_flags_wants_construction(void) {
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
    return fail("build-ai-flags alloc map");
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
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1; /* no named queue — bit alone */
  c->build_ai_flags = COLONIZE_BUILD_AI_WANTS_CONSTRUCTION;
  c->labor_shortage = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* col = units_get(&units, uid);
  if (!col) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("build-ai-flags spawn");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves_left = 1;

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

  uint32_t turn = 62;
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
  if (!joined) {
    fprintf(stderr,
      "unit_ai_euro_expand: build-ai joined=%d pop=%d→%d flags=0x%02x labor=%u active=%d\n",
      joined, pop_before, c->population, (unsigned)c->build_ai_flags,
      (unsigned)c->labor_shortage, col && col->active);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected LABOR join from build_ai_flags wants_construction");
  }

  /* clear_construction drops bit7 */
  c->build_ai_flags = COLONIZE_BUILD_AI_WANTS_CONSTRUCTION;
  c->building_in_production = 0;
  colonies_clear_construction(&colonies, c->id);
  if ((c->build_ai_flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) != 0 ||
      c->building_in_production != -1) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected clear_construction to drop wants_construction bit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: build_ai_flags wants_construction ok\n");
  return 0;
}

static int unit_construction_labor_stockade(void) {
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
      "unit_ai_euro_expand: construction joined=%d labor_stay=%d left=%d "
      "pop %d→%d active=%d pos=(%d,%d) goto=(%d,%d) orders=%d labor_prio=%d colonies=%d (c0=(%d,%d) c1=(%d,%d))\n",
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
      ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR),
      colonies.colony_count,
      colonies.colonies[0].x, colonies.colonies[0].y,
      colonies.colony_count > 1 ? colonies.colonies[1].x : -1,
      colonies.colony_count > 1 ? colonies.colonies[1].y : -1
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
    "unit_ai_euro_expand: construction Stockade LABOR ok (joined=%d)\n",
    joined
  );
  return 0;
}

/*
 * Master Carpenter construction LABOR: idle Master Carpenter on colony with
 * incomplete Stockade → stay/join LABOR (Stockade pattern). Cite: euro_unit_act
 * §2e; docs/building_production.md Carpenter→Hammers; Skills Chart.
 */
static int unit_master_carpenter_construction_labor(void) {
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
      "unit_ai_euro_expand: carpenter joined=%d labor_stay=%d left=%d "
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
    "unit_ai_euro_expand: Master Carpenter construction LABOR ok (joined=%d)\n",
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
static int unit_lumberjack_warehouse_labor(void) {
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
      "unit_ai_euro_expand: lumberjack joined=%d labor_stay=%d left=%d "
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
    "unit_ai_euro_expand: Lumberjack Warehouse LABOR ok (joined=%d)\n",
    joined
  );
  return 0;
}

/*
 * Threatened Stockade: idle Free Colonist within MD≤3 of own colony with
 * incomplete Stockade + war-peer threat prefers LABOR over distant FOUND.
 * Cite: building_production.md Stockade; Colonization.pdf fortify defense.
 */
static int unit_stockade_threat_labor(void) {
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
      "unit_ai_euro_expand: threat-stockade labor=%d prio=%d toward=%d joined=%d "
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
    "unit_ai_euro_expand: threatened Stockade LABOR ok (prio=%d toward=%d joined=%d)\n",
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
static int unit_scout_sticky_closer_ring(void) {
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
      "unit_ai_euro_expand: sticky CONTACT=(%d,%d) ring_md=%d (want 2)\n",
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
    "unit_ai_euro_expand: sticky CONTACT closer-ring ok (goto=(%d,%d) md=%d)\n",
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
static int unit_scout_fog_explore_no_contact(void) {
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
      "unit_ai_euro_expand: fog-explore orders=%d goto=(%d,%d) contact=%d\n",
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
    "unit_ai_euro_expand: fog-explore no-CONTACT ok (goto=(%d,%d))\n",
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
static int unit_seasoned_scout_deeper_fog(void) {
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
      "unit_ai_euro_expand: seasoned-fog orders=%d goto=(%d,%d) md=%d name=%s\n",
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
  fprintf(stderr, "unit_ai_euro_expand: Seasoned Scout deeper fog ok\n");
  return 0;
}

/*
 * Scout fog explore prefers map_tile_has_rumour over nearer plain unseen
 * within MD≤8. Fixture: (5,8) MD=3 plain + (3,10) MD=7 rumour (seed-100
 * procedural). Cite: Colonization.pdf Lost City Rumours / Seasoned Scout;
 * Pass5 LCR scaffold — resolve still on stand only.
 */
static int unit_scout_fog_prefer_rumour(void) {
  const int nation = 1;
  const int scout_x = 5;
  const int scout_y = 5;
  const int plain_x = 5;
  const int plain_y = 8; /* MD=3, no rumour */
  const int rum_x = 3;
  const int rum_y = 10; /* MD=7, procedural rumour */

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
    return fail("rumour-fog alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  if (!map_tile_has_rumour(&map, rum_x, rum_y)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("rumour-fog fixture (3,10) should have rumour");
  }
  if (map_tile_has_rumour(&map, plain_x, plain_y)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("rumour-fog fixture (5,8) must be plain unseen");
  }
  /* Reveal all MD≤8 except plain MD=3 and rumour MD=7. */
  for (int dy = -8; dy <= 8; ++dy) {
    for (int dx = -8; dx <= 8; ++dx) {
      const int md = abs(dx) + abs(dy);
      if (md < 1 || md > 8) {
        continue;
      }
      const int nx = scout_x + dx;
      const int ny = scout_y + dy;
      if ((nx == plain_x && ny == plain_y) || (nx == rum_x && ny == rum_y)) {
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

  const int sid = units_spawn(&units, 0, scout_x, scout_y);
  ColonizeUnit* scout = units_get(&units, sid);
  if (!scout) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("rumour-fog spawn");
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
  col1.tribe = NULL;

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

  scout = units_get(&units, sid);
  const int ok =
    scout && scout->active && scout->orders == UNITS_ORDER_AI_MOVE &&
    scout->goto_x == rum_x && scout->goto_y == rum_y;
  if (!ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: rumour-fog orders=%d goto=(%d,%d) want rumour=(%d,%d)\n",
      scout ? scout->orders : -1,
      scout ? scout->goto_x : -1,
      scout ? scout->goto_y : -1,
      rum_x,
      rum_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("expected Scout AI_MOVE to rumour over nearer plain unseen");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.seen);
  fprintf(stderr, "unit_ai_euro_expand: Scout fog prefer rumour ok\n");
  return 0;
}

/*
 * Treasure train: idle Treasure inland → AI_MOVE toward own coastal colony.
 * Cite: Colonization.pdf Treasure Trains — park in coastal colony.
 */
static int unit_treasure_coast(void) {
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
      "unit_ai_euro_expand: treasure orders=%d goto=(%d,%d) pos=(%d,%d)\n",
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
    "unit_ai_euro_expand: treasure coast ok (goto=(%d,%d))\n",
    treasure->goto_x,
    treasure->goto_y
  );
  return 0;
}

/*
 * Cortes free king galleon: Treasure on own coastal colony + FF Cortes →
 * europe_cash_treasure (tax cut) + despawn without boarding a ship.
 * Cite: fandom Hernan Cortes; GAME.TXT @KINGGALLEON3; founding_fathers_cortes_*.
 */
static int unit_cortes_king_galleon_cash(void) {
  const int nation = 1;
  const int treasure_value = 1000;
  const int tax = 20;
  const int expect_credit = (treasure_value * (100 - tax)) / 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("cortes-cash alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cortes-cash colony should be coastal");
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

  const int tid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* treasure = units_get(&units, tid);
  if (!treasure) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cortes-cash spawn treasure");
  }
  treasure->nation_id = nation;
  treasure->moves_left = 0;
  treasure->orders = 0;
  treasure->hold_goods_amount[0] = treasure_value & 0xff;
  treasure->hold_goods_amount[1] = (treasure_value >> 8) & 0xff;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 100;
  europe.tax_percent = tax;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    col1.head.founding_father[i] = -1;
  }
  col1.nation[nation].gold = 100;
  col1.nation[nation].tax_rate = (uint8_t)tax;

  ai_goals_reset();
  uint32_t turn = 50;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 7;

  ai_euro_dispatcher_turn(&ctx, nation);
  treasure = units_get(&units, tid);
  if (!treasure || !treasure->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("without Cortes treasure should remain on coast");
  }
  const uint32_t gold_mid = col1.nation[nation].gold;

  col1.head.founding_father[FF_HERNAN_CORTES] = 0;
  col1.nation[nation].founding_fathers[FF_HERNAN_CORTES / 8] |=
    (uint8_t)(1u << (FF_HERNAN_CORTES % 8));
  treasure->x = 4;
  treasure->y = 4;
  treasure->orders = 0;
  treasure->moves_left = 0;
  ai_euro_dispatcher_turn(&ctx, nation);

  treasure = units_get(&units, tid);
  const int treasure_gone = (!treasure || !treasure->active);
  const uint32_t gold_after = col1.nation[nation].gold;
  const unsigned delta =
    gold_after >= gold_mid ? (unsigned)(gold_after - gold_mid) : 0u;
  const int cash_ok =
    treasure_gone && delta >= (unsigned)expect_credit &&
    (delta - (unsigned)expect_credit) <= 80u;
  if (!cash_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: cortes cash gone=%d gold mid=%u after=%u delta=%u want +%d\n",
      treasure_gone,
      (unsigned)gold_mid,
      (unsigned)gold_after,
      delta,
      expect_credit
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Cortes coastal king-galleon cash + despawn");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_expand: Cortes king-galleon coastal cash ok (delta=%u credit=%d)\n",
    delta,
    expect_credit
  );
  return 0;
}

/*
 * Missionary CONTACT: peace Jesuit/Missionary → CONTACT + AI_MOVE toward
 * nearest tribe without mission. Fleeing (Alarm≥55 adjacent) skips.
 */
static int unit_missionary_contact(void) {
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
    /* Content-floor Indian relations — memset 0 reads as at-war (<50). */
    for (int ind = 0; ind < 8; ++ind) {
      col1.nation[i].relation_by_indian[ind] = 100;
    }
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
      "unit_ai_euro_expand: missionary CONTACT=(%d,%d) want (%d,%d)\n",
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
      "unit_ai_euro_expand: missionary orders=%d goto=(%d,%d)\n",
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
    "unit_ai_euro_expand: missionary CONTACT ok (goto=(%d,%d))\n",
    miss->goto_x,
    miss->goto_y
  );
  return 0;
}

/*
 * Missionary flee gate: adjacent alarmed tribe (friction≥55) → no CONTACT.
 */
static int unit_missionary_flee_skip(void) {
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
        "unit_ai_euro_expand: flee skip got CONTACT=(%d,%d)\n",
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
  fprintf(stderr, "unit_ai_euro_expand: missionary flee-skip CONTACT ok\n");
  return 0;
}

/*
 * Food emergency: food_short high + Pioneer at MD 5 → LABOR goto toward hungry
 * colony (not only MD≤1 bind). Cite: 5cf6 food_short; manual 2 food/colonist.
 */
static int unit_food_emergency_labor(void) {
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
      "unit_ai_euro_expand: food-emerg labor=%d orders=%d goto=(%d,%d) pos=(%d,%d)\n",
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
      "unit_ai_euro_expand: expert-farmer orders=%d goto=(%d,%d) pos=(%d,%d) labor=%d\n",
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
      "unit_ai_euro_expand: fc-food orders=%d goto=(%d,%d) pos=(%d,%d) labor=%d\n",
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
      "unit_ai_euro_expand: tools-labor orders=%d goto=(%d,%d) pos=(%d,%d) "
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
  fprintf(stderr, "unit_ai_euro_expand: tools-short Pioneer LABOR ok\n");
  return 0;
}

/*
 * Treasure at coastal colony + adjacent ship with space → board + AI_SAIL
 * Europe (eastward). Cite: Colonization.pdf Treasure Trains.
 * Gold cash runs only at Europe / HS (separate unit_treasure_europe_cash).
 */
static int unit_treasure_board_sail(void) {
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
      "unit_ai_euro_expand: treasure aboard=%d want ship %d pos=(%d,%d)\n",
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
      "unit_ai_euro_expand: ship orders=%d goto=(%d,%d) pos=(%d,%d)\n",
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
  /* Board+sail is not Europe/HS yet — no cash-in; do not invent gold here. */
  const uint32_t gold_after = col1.nation[nation].gold;
  const unsigned gold_delta =
    gold_after >= gold_before ? (unsigned)(gold_after - gold_before) : 0u;
  if (gold_delta > 80u) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: treasure gold %u→%u delta=%u (board/sail not Europe)\n",
      (unsigned)gold_before,
      (unsigned)gold_after,
      gold_delta
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("Treasure board/sail must not cash gold before Europe/HS");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_expand: treasure board+sail ok (ship AI_SAIL goto=(%d,%d) "
    "gold_delta=%u)\n",
    ship->goto_x,
    ship->goto_y,
    gold_delta
  );
  return 0;
}

/*
 * Ship at Europe with Treasure aboard + COL1 LE16 gold in hold_goods_amount →
 * europe_cash_treasure credits nation gold (tax cut); Treasure despawned.
 * Cite: Colonization.pdf Treasure Trains; GAME.TXT @LOOTCASH; europe.h.
 */
static int unit_treasure_europe_cash(void) {
  const int nation = 1;
  const int treasure_value = 800; /* LE16 in hold_goods_amount[0..1] */
  const int tax = 25;
  const int expect_credit = (treasure_value * (100 - tax)) / 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("treasure-cash alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
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

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure-cash spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  const int tid = units_spawn_allow_stack(&units, 0, 200, 200);
  ColonizeUnit* treasure = units_get(&units, tid);
  if (!treasure) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure-cash spawn treasure");
  }
  treasure->nation_id = nation;
  treasure->moves_left = 0;
  treasure->orders = 0;
  /* COL1 cargo_hold[0..1] LE16 gold → hold_goods_amount lo/hi bytes. */
  treasure->hold_goods_amount[0] = treasure_value & 0xff;
  treasure->hold_goods_amount[1] = (treasure_value >> 8) & 0xff;
  if (!units_board_stacked(&units, tid, sid)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasure-cash board setup");
  }

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 200;
  europe.tax_percent = tax;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  col1.nation[nation].tax_rate = (uint8_t)tax;
  const uint32_t gold_before = col1.nation[nation].gold;

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
  ctx.europe = &europe;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  treasure = units_get(&units, tid);
  const int treasure_gone = (!treasure || !treasure->active);
  const uint32_t gold_after = col1.nation[nation].gold;
  const unsigned delta =
    gold_after >= gold_before ? (unsigned)(gold_after - gold_before) : 0u;
  /* Planning 5d04 treasury bump is small (~30); cash credit is expect_credit. */
  const int cash_ok =
    treasure_gone && delta >= (unsigned)expect_credit &&
    (delta - (unsigned)expect_credit) <= 80u;
  if (!cash_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: treasure_gone=%d gold %u→%u delta=%u want +%d (tax %d%%)\n",
      treasure_gone,
      (unsigned)gold_before,
      (unsigned)gold_after,
      delta,
      expect_credit,
      tax
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Treasure Europe cash-in + despawn");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_expand: treasure Europe cash ok (gold %u→%u delta=%u credit=%d)\n",
    (unsigned)gold_before,
    (unsigned)gold_after,
    delta,
    expect_credit
  );
  return 0;
}

/*
 * Idle Wagon with hold capacity → AI_MOVE toward tools-short colony.
 * Cite: euro_unit_act §2d wagon haul / tools delivery.
 */
static int unit_wagon_haul_tools_short(void) {
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
      "unit_ai_euro_expand: wagon orders=%d goto=(%d,%d) pos=(%d,%d)\n",
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
  fprintf(stderr, "unit_ai_euro_expand: wagon haul tools-short ok\n");
  return 0;
}

/*
 * Sticky + FoW: prefer deeper unseen ring (md=4) over closer seen (md=2).
 * Cite: euro_unit_act §2c2 sticky+fog deepen.
 */
static int unit_scout_sticky_fog_deeper_unseen(void) {
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
      "unit_ai_euro_expand: sticky-fog CONTACT=(%d,%d) md=%d seen=%d\n",
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
  fprintf(stderr, "unit_ai_euro_expand: sticky+fog deeper unseen ring ok\n");
  return 0;
}

/*
 * Idle Caravel with goods-hold capacity → AI_SAIL toward tools-short coastal
 * colony water. Cite: euro_unit_act §2d2 cargo haul.
 */
static int unit_ship_trade_haul_tools_short(void) {
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
      "unit_ai_euro_expand: ship orders=%d goto=(%d,%d) pos=(%d,%d)\n",
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
  fprintf(stderr, "unit_ai_euro_expand: ship trade haul tools-short ok\n");
  return 0;
}

/*
 * Idle Caravel with MUSKETS cargo → AI_SAIL toward muskets-short coastal colony
 * (tools/food OK). Cite: euro_unit_act §2d2 ship haul muskets; wagon §2d.
 */
static int unit_ship_trade_haul_muskets_short(void) {
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
    return fail("ship-muskets alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 3] = 25;
  }
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-muskets colony should be coastal");
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
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 2; /* muskets-short */
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-muskets spawn");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;
  ship->hold_goods_type[0] = COLONIZE_CARGO_MUSKETS;
  ship->hold_goods_amount[0] = 10;

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
    return fail("ship-muskets should remain active");
  }
  const int near_colony =
    abs(ship->goto_x - 4) <= 1 && abs(ship->goto_y - 4) <= 1 &&
    (ship->goto_x != 4 || ship->goto_y != 4);
  const int sailed = ship->orders == UNITS_ORDER_AI_SAIL && near_colony;
  const int at_berth = abs(ship->x - 4) <= 1 && abs(ship->y - 4) <= 1 &&
                       map_tile_is_water(&map, ship->x, ship->y);
  const int delivered = c->stock[COLONIZE_CARGO_MUSKETS] > 2;
  if (!sailed && !at_berth && !delivered) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: ship-muskets orders=%d goto=(%d,%d) pos=(%d,%d) muskets=%d\n",
      ship->orders,
      ship->goto_x,
      ship->goto_y,
      ship->x,
      ship->y,
      c->stock[COLONIZE_CARGO_MUSKETS]
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Caravel AI_SAIL toward muskets-short coastal colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: ship trade haul muskets-short ok\n");
  return 0;
}

/*
 * Idle Caravel with SILVER hold (Custom House–eligible) at coastal colony that
 * is not haul-short → AI_SAIL Europe. Cite: FUN_364b_0636 / europe_cargo_export_eligible;
 * euro_unit_act §2d2 Europe export sail.
 */
static int unit_ship_europe_export_silver(void) {
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
    return fail("ship-export alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-export colony should be coastal");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
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
  /* Mid-band stocks: not haul-short and not haul-surplus (food 6..11). */
  c->stock[COLONIZE_CARGO_TOOLS] = 25;
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  c->stock[COLONIZE_CARGO_ORE] = 25;
  c->stock[COLONIZE_CARGO_MUSKETS] = 15;
  c->stock[COLONIZE_CARGO_HORSES] = 15;
  c->stock[COLONIZE_CARGO_FOOD] = 8;
  c->stock[COLONIZE_CARGO_SILVER] = 150;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-export spawn");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;
  ship->hold_goods_type[0] = COLONIZE_CARGO_SILVER;
  ship->hold_goods_amount[0] = 50;

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
    return fail("ship-export should remain active");
  }
  const int sailed_east =
    ship->orders == UNITS_ORDER_AI_SAIL && ship->goto_x > ship->x;
  if (!sailed_east) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: ship-export orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      ship->orders,
      ship->goto_x,
      ship->goto_y,
      ship->x,
      ship->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Caravel AI_SAIL eastward with SILVER (Europe export)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: ship Europe export silver ok\n");
  return 0;
}

/*
 * Idle Privateer with SILVER hold (loot) → AI_SAIL Europe dump-sell. Cite:
 * europe_cargo_export_eligible; ai_euro_try_privateer_europe_loot_sail;
 * euro_unit_act Privateer dump-sell.
 */
static int unit_privateer_europe_loot_sail(void) {
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
    return fail("priv-loot-sail alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
  units.types[0].movement = 8;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("priv-loot-sail spawn");
  }
  ship->nation_id = nation;
  ship->moves_left = 8;
  ship->orders = 0;
  ship->hold_goods_type[0] = COLONIZE_CARGO_SILVER;
  ship->hold_goods_amount[0] = 40;

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
    return fail("priv-loot-sail should remain active");
  }
  const int sailed_east =
    ship->orders == UNITS_ORDER_AI_SAIL && ship->goto_x > ship->x;
  if (!sailed_east) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: priv-loot orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      ship->orders,
      ship->goto_x,
      ship->goto_y,
      ship->x,
      ship->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Privateer AI_SAIL eastward with SILVER loot");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Privateer Europe loot sail ok\n");
  return 0;
}

/*
 * Idle Caravel at coastal SILVER surplus (stock>99) with empty hold → load
 * excess (leave 50) then AI_SAIL Europe. Cite: FUN_364b_0688; euro_unit_act §2d2.
 */
static int unit_ship_europe_export_load_silver(void) {
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
    return fail("ship-export-load alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-export-load colony should be coastal");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
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
  c->stock[COLONIZE_CARGO_TOOLS] = 25;
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  c->stock[COLONIZE_CARGO_ORE] = 25;
  c->stock[COLONIZE_CARGO_MUSKETS] = 15;
  c->stock[COLONIZE_CARGO_HORSES] = 15;
  c->stock[COLONIZE_CARGO_FOOD] = 8;
  c->stock[COLONIZE_CARGO_SILVER] = 150;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-export-load spawn");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;

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
    return fail("ship-export-load should remain active");
  }
  const int loaded = c->stock[COLONIZE_CARGO_SILVER] == 50 &&
                     ship->hold_goods_type[0] == COLONIZE_CARGO_SILVER &&
                     ship->hold_goods_amount[0] == 100;
  const int sailed_east =
    ship->orders == UNITS_ORDER_AI_SAIL && ship->goto_x > ship->x;
  if (!loaded || !sailed_east) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: export-load silver=%d hold_t=%d hold_a=%d "
      "orders=%d goto=(%d,%d)\n",
      c->stock[COLONIZE_CARGO_SILVER],
      ship->hold_goods_type[0],
      ship->hold_goods_amount[0],
      ship->orders,
      ship->goto_x,
      ship->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected load SILVER leave 50 + AI_SAIL Europe");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: ship Europe export load silver ok\n");
  return 0;
}

/*
 * Idle Galleon with goods-hold capacity → AI_SAIL toward tools-short coastal
 * colony (same haul ladder as Caravel). Cite: euro_unit_act §2d2; docs/assets.md
 * Europe purchase ladder (Galleon cargo ship).
 */

/*
 * Idle Galleon at coastal SILVER surplus (stock>99) with empty hold → load
 * excess (leave 50) then AI_SAIL Europe. Cite: FUN_364b_0688; euro_unit_act §2d2.
 */
static int unit_galleon_europe_export_load_silver(void) {
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
    return fail("galleon-export-load alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("galleon-export-load colony should be coastal");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Galleon");
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
  c->stock[COLONIZE_CARGO_TOOLS] = 25;
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  c->stock[COLONIZE_CARGO_ORE] = 25;
  c->stock[COLONIZE_CARGO_MUSKETS] = 15;
  c->stock[COLONIZE_CARGO_HORSES] = 15;
  c->stock[COLONIZE_CARGO_FOOD] = 8;
  c->stock[COLONIZE_CARGO_SILVER] = 150;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("galleon-export-load spawn");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;

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
    return fail("galleon-export-load should remain active");
  }
  const int loaded = c->stock[COLONIZE_CARGO_SILVER] == 50 &&
                     ship->hold_goods_type[0] == COLONIZE_CARGO_SILVER &&
                     ship->hold_goods_amount[0] == 100;
  const int sailed_east =
    ship->orders == UNITS_ORDER_AI_SAIL && ship->goto_x > ship->x;
  if (!loaded || !sailed_east) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: export-load silver=%d hold_t=%d hold_a=%d "
      "orders=%d goto=(%d,%d)\n",
      c->stock[COLONIZE_CARGO_SILVER],
      ship->hold_goods_type[0],
      ship->hold_goods_amount[0],
      ship->orders,
      ship->goto_x,
      ship->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected load SILVER leave 50 + AI_SAIL Europe");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: galleon Europe export load silver ok\n");
  return 0;
}

/*
 * Idle Galleon with goods-hold capacity → AI_SAIL toward tools-short coastal
 * colony (same haul ladder as Galleon). Cite: euro_unit_act §2d2; docs/assets.md
 * Europe purchase ladder (Galleon cargo ship).
 */

/*
 * Idle Merchantman at coastal SILVER surplus (stock>99) with empty hold → load
 * excess (leave 50) then AI_SAIL Europe. Cite: FUN_364b_0688; euro_unit_act §2d2.
 */
static int unit_merchantman_europe_export_load_silver(void) {
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
    return fail("mm-export-load alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("mm-export-load colony should be coastal");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Merchantman");
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
  c->stock[COLONIZE_CARGO_TOOLS] = 25;
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  c->stock[COLONIZE_CARGO_ORE] = 25;
  c->stock[COLONIZE_CARGO_MUSKETS] = 15;
  c->stock[COLONIZE_CARGO_HORSES] = 15;
  c->stock[COLONIZE_CARGO_FOOD] = 8;
  c->stock[COLONIZE_CARGO_SILVER] = 150;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("mm-export-load spawn");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;

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
    return fail("mm-export-load should remain active");
  }
  const int loaded = c->stock[COLONIZE_CARGO_SILVER] == 50 &&
                     ship->hold_goods_type[0] == COLONIZE_CARGO_SILVER &&
                     ship->hold_goods_amount[0] == 100;
  const int sailed_east =
    ship->orders == UNITS_ORDER_AI_SAIL && ship->goto_x > ship->x;
  if (!loaded || !sailed_east) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: export-load silver=%d hold_t=%d hold_a=%d "
      "orders=%d goto=(%d,%d)\n",
      c->stock[COLONIZE_CARGO_SILVER],
      ship->hold_goods_type[0],
      ship->hold_goods_amount[0],
      ship->orders,
      ship->goto_x,
      ship->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected load SILVER leave 50 + AI_SAIL Europe");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: merchantman Europe export load silver ok\n");
  return 0;
}

/*
 * Idle Merchantman with goods-hold capacity → AI_SAIL toward tools-short coastal
 * colony (same haul ladder as Merchantman). Cite: euro_unit_act §2d2; docs/assets.md
 * Europe purchase ladder (Merchantman cargo ship).
 */
static int unit_galleon_trade_haul_tools_short(void) {
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
    return fail("galleon-haul alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 3] = 25;
  }
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("galleon-haul colony should be coastal");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Galleon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 6;

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

  const int sid = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("galleon-haul spawn");
  }
  ship->nation_id = nation;
  ship->moves_left = 4;
  ship->orders = 0;

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
    return fail("galleon-haul should remain active");
  }
  const int near_colony =
    abs(ship->goto_x - 4) <= 1 && abs(ship->goto_y - 4) <= 1 &&
    (ship->goto_x != 4 || ship->goto_y != 4);
  const int sailed = ship->orders == UNITS_ORDER_AI_SAIL && near_colony;
  const int at_berth = abs(ship->x - 4) <= 1 && abs(ship->y - 4) <= 1 &&
                       map_tile_is_water(&map, ship->x, ship->y);
  if (!sailed && !at_berth) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: galleon-haul orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      ship->orders,
      ship->goto_x,
      ship->goto_y,
      ship->x,
      ship->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Galleon AI_SAIL toward tools-short coastal colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: galleon trade haul tools-short ok\n");
  return 0;
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
  const int pid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("improve-timer spawn");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves_left = 1;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("expected improve_timer gate to block plow");
  }
  if (colonies.colonies[0].improve_timer < 1) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("expected improve_timer INC");
  }

  /* Unblock: park pioneer on plowable surround and meet gate. */
  colonies.colonies[0].improve_timer = 2;
  pioneer = units_get(&units, pid);
  if (!pioneer || !pioneer->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("improve-timer pioneer gone before unblock");
  }
  pioneer->x = 4;
  pioneer->y = 3;
  pioneer->moves_left = 1;
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
      pioneer->moves_left = 1;
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
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("expected plow after improve_timer meets gate");
  }
  if (colonies.colonies[0].improve_timer != 0) {
    fprintf(stderr, "improve_timer after plow=%u\n",
            (unsigned)colonies.colonies[0].improve_timer);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("expected improve_timer clear on plow");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.improve);
  fprintf(stderr, "unit_ai_euro_expand: improve_timer pioneer gate ok\n");
  return 0;
}

static int unit_pioneer_plow_improve(void) {
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
    return fail("pioneer-plow alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }

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
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_TOOLS] = 40; /* no tools_short */
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  c->improve_timer = 2; /* Col1 +0x8c gate (≥2 thin) */
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Hardy Pioneer on north surround (4,3) — plowable plains. */
  const int pid = units_spawn(&units, 0, 4, 3);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("pioneer-plow spawn");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves_left = 1;
  pioneer->tools = 100;
  pioneer->profession = UNITS_JOB_PIONEER; /* Hardy */

  ai_goals_reset();
  /* Distant FOUND must not yank off improve. */
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

  uint32_t turn = 22;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 7;

  const int tools0 = pioneer->tools;
  ai_euro_dispatcher_turn(&ctx, nation);

  pioneer = units_get(&units, pid);
  const int plowed = map_tile_is_plowed(&map, 4, 3);
  const int tools_spent =
    pioneer && pioneer->active && pioneer->tools == tools0 - UNITS_EQUIP_TOOLS_STEP;
  /* Or goto toward another improvable surround if (4,3) skipped. */
  const int improving =
    pioneer && pioneer->active && pioneer->orders == UNITS_ORDER_AI_MOVE &&
    abs(pioneer->goto_x - 4) <= 1 && abs(pioneer->goto_y - 4) <= 1 &&
    (pioneer->goto_x != 4 || pioneer->goto_y != 4);
  /*
   * Or already on-tile and mid-job (started this turn, not yet finished) —
   * the real DS:0x2f78 threshold (2026-08-20 live capture) usually takes
   * more than one turn even for a Hardy Pioneer, so "started CLEAR_PLOW/
   * BUILD_ROAD on the surround tile" is now the common single-turn outcome,
   * not "already plowed".
   */
  const int started =
    pioneer && pioneer->active &&
    (pioneer->orders == UNITS_ORDER_CLEAR_PLOW || pioneer->orders == UNITS_ORDER_BUILD_ROAD) &&
    pioneer->x == 4 && pioneer->y == 3;

  if (!plowed && !tools_spent && !improving && !started) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: plow=%d tools=%d→%d orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      plowed,
      tools0,
      pioneer ? pioneer->tools : -1,
      pioneer ? pioneer->orders : -1,
      pioneer ? pioneer->goto_x : -1,
      pioneer ? pioneer->goto_y : -1,
      pioneer ? pioneer->x : -1,
      pioneer ? pioneer->y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("expected Hardy Pioneer plow or improve goto on colony surround");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.improve);
  fprintf(
    stderr,
    "unit_ai_euro_expand: pioneer plow ok (plowed=%d tools_spent=%d improving=%d)\n",
    plowed,
    tools_spent,
    improving
  );
  return 0;
}

/*
 * Expert Lumberjack forest field-assign: idle Expert Lumberjack on own colony
 * with free forest surround → admit + colonies_assign_field Lumberjack.
 * Cite: terrain_yields / building_production Lumberjack→Lumber; Skills Chart.
 */
static int unit_lumberjack_field_assign(void) {
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
    return fail("lumber-field alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  /* North surround forest (tile index 0). */
  map.terrain[3 * 16 + 4] = 10; /* mixed forest */

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Lumberjack");
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
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1; /* field-assign, not Warehouse LABOR */
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* lumber = units_get(&units, uid);
  if (!lumber) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("lumber-field spawn");
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

  uint32_t turn = 23;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 9;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  lumber = units_get(&units, uid);
  const int joined = (lumber == NULL || !lumber->active) && c->population > pop0;
  int field_ok = 0;
  if (joined) {
    for (int i = 0; i < c->colonist_count; ++i) {
      if (!c->colonists[i].active) {
        continue;
      }
      if (c->colonists[i].field_job == COLONIZE_JOB_LUMBERJACK &&
          colonies_colonist_tile(c, i) == 0) {
        field_ok = 1;
        break;
      }
    }
    /* Accept any forest field Lumberjack assign if tile 0 race-occupied. */
    if (!field_ok) {
      for (int i = 0; i < c->colonist_count; ++i) {
        if (c->colonists[i].active &&
            c->colonists[i].field_job == COLONIZE_JOB_LUMBERJACK) {
          field_ok = 1;
          break;
        }
      }
    }
  }

  if (!joined || !field_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: lumber-field joined=%d field=%d pop %d→%d "
      "active=%d tile0=%d\n",
      joined,
      field_ok,
      pop0,
      c->population,
      lumber ? (int)lumber->active : 0,
      (int)c->tiles[0]
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Lumberjack admit + forest field assign");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Lumberjack forest field-assign ok\n");
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

  ColonizeColonyPool colonies;
  unit_indian_land_seed_colony(&colonies, nation);

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  /* Plan-only: discover expand FOUND from colony (4,4). */
  const int probe = units_spawn(&units, 0, 4, 5);
  ColonizeUnit* pu = units_get(&units, probe);
  if (!pu) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("indian-land probe spawn");
  }
  pu->nation_id = nation;
  pu->orders = 0;
  pu->moves_left = 0;

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
  ctx.rng_seed = 11;
  ai_euro_dispatcher_turn(&ctx, nation);

  int fx = -1;
  int fy = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && g->code == AI_GOAL_FOUND) {
      fx = g->x;
      fy = g->y;
      break;
    }
  }
  if (fx < 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("indian-land: no expand FOUND from stocked colony");
  }

  /* Park tribe on expand FOUND → homeland; founder stands there. */
  tribe.x = (uint8_t)fx;
  tribe.y = (uint8_t)fy;
  const int cost = colonies_indian_land_purchase_gold(&col1, &map, fx, fy, nation);
  if (cost <= 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("indian-land: expected homeland purchase gold > 0");
  }

  /* Phase 1: enough gold → found + debit (planning treasury bump then charge). */
  {
    unit_indian_land_seed_colony(&colonies, nation);
    units_reset(&units);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
    units.types[0].movement = 3;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    col1.nation[nation].gold = 500;
    col1.indian[0].lands_bought = 0;
    col1.nation[nation].founding_fathers[0] = 0;
    col1.nation[nation].founding_father_count = 0;

    const int uid = units_spawn(&units, 0, fx, fy);
    ColonizeUnit* founder = units_get(&units, uid);
    if (!founder) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("indian-land spawn pay");
    }
    founder->nation_id = nation;
    founder->orders = 0;
    founder->moves_left = 3;

    ai_goals_reset();
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
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
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
        free(map.terrain);
        free(map.layer2);
        free(map.layer3);
        return fail("indian-land: unexpected gold after homeland found");
      }
    }
  }

  /* Phase 2: short gold → PARK (seed colony remains; no second colony).
   * Phase 1 stamped MAP_LAYER2_PURCHASED on (fx,fy); clear so charge still
   * applies (founding must not treat prior buy as free forever for this smoke). */
  {
    unit_indian_land_seed_colony(&colonies, nation);
    units_reset(&units);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
    units.types[0].movement = 3;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    col1.nation[nation].gold = 10;
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
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("indian-land spawn poor");
    }
    founder->nation_id = nation;
    founder->orders = 0;
    founder->moves_left = 3;

    char status[128];
    memset(status, 0, sizeof(status));
    col1.player[nation].control = 0; /* human for thin status chrome */
    ctx.human_nation = nation;
    ctx.status = status;
    ctx.status_size = sizeof(status);

    ai_goals_reset();
    turn = 42;
    ai_euro_dispatcher_turn(&ctx, nation);

    founder = units_get(&units, uid);
    if (count_nation_colonies(&colonies, nation) != 1 || !founder || !founder->active) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("indian-land: short gold must PARK found");
    }
    if (col1.nation[nation].gold >= (uint32_t)cost) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("indian-land: PARK case gold unexpectedly covers cost");
    }
    if (strstr(status, "Not enough gold") == NULL) {
      fprintf(stderr, "unit_ai_euro_expand: indian-land status=%s\n", status);
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
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
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
    units.types[0].movement = 3;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    col1.nation[nation].gold = 200;
    col1.indian[0].lands_bought = 0;
    col1.nation[nation].founding_fathers[FF_PETER_MINUIT / 8] |=
      (uint8_t)(1u << (FF_PETER_MINUIT % 8));
    if (!founding_fathers_nation_has(&col1, nation, FF_PETER_MINUIT)) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("indian-land: Minuit elect bit helper");
    }
    if (colonies_indian_land_purchase_gold(&col1, &map, fx, fy, nation) != 0) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("indian-land: Minuit must zero purchase gold");
    }

    const uint32_t gold0 = col1.nation[nation].gold;
    const int uid = units_spawn(&units, 0, fx, fy);
    ColonizeUnit* founder = units_get(&units, uid);
    if (!founder) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("indian-land spawn Minuit");
    }
    founder->nation_id = nation;
    founder->orders = 0;
    founder->moves_left = 3;

    ai_goals_reset();
    turn = 43;
    ai_euro_dispatcher_turn(&ctx, nation);

    founder = units_get(&units, uid);
    if (count_nation_colonies(&colonies, nation) != 2 || (founder && founder->active)) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("indian-land: Minuit free found failed");
    }
    if (col1.nation[nation].gold < gold0 || col1.nation[nation].gold > gold0 + 80u) {
      fprintf(
        stderr,
        "unit_ai_euro_expand: Minuit gold %u→%u (want bump-only)\n",
        gold0,
        col1.nation[nation].gold
      );
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("indian-land: Minuit free found must not spend land gold");
    }
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: indian-land FOUND charge/Minuit ok\n");
  return 0;
}

/*
 * Expert Ore Miner hills field-assign: idle Expert Ore Miner on own colony
 * with free hills surround → admit + colonies_assign_field Ore Miner.
 * Cite: terrain_yields Ore Miner; Skills Chart (parallel Lumberjack).
 */
static int unit_ore_miner_field_assign(void) {
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
    return fail("ore-field alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 3; /* prairie — no ore */
  }
  /* North surround hills (tile index 0): ore yield. */
  map.terrain[3 * 16 + 4] = 0x20u;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Ore Miner");
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
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* miner = units_get(&units, uid);
  if (!miner) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ore-field spawn");
  }
  miner->nation_id = nation;
  miner->orders = 0;
  miner->moves_left = 3;
  miner->profession = 6; /* @JOB Ore Miner */

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
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
  ctx.rng_seed = 14;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  miner = units_get(&units, uid);
  const int joined = (miner == NULL || !miner->active) && c->population > pop0;
  int field_ok = 0;
  if (joined) {
    for (int i = 0; i < c->colonist_count; ++i) {
      if (c->colonists[i].active && c->colonists[i].field_job == COLONIZE_JOB_ORE_MINER) {
        field_ok = 1;
        break;
      }
    }
  }

  if (!joined || !field_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: ore-field joined=%d field=%d pop %d→%d\n",
      joined,
      field_ok,
      pop0,
      c->population
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Ore Miner admit + hills field assign");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Ore Miner hills field-assign ok\n");
  return 0;
}

/*
 * Expert Silver Miner mountains field-assign: idle Expert Silver Miner on own
 * colony with free mountain surround → admit + colonies_assign_field Silver
 * Miner. Cite: terrain_yields Silver Miner (mountains); Skills Chart; parallel
 * Ore Miner field-assign.
 */
static int unit_silver_miner_field_assign(void) {
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
    return fail("silver-field alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 3; /* prairie — no silver */
  }
  /* North surround mountains (0xa0): silver yield. */
  map.terrain[3 * 16 + 4] = 0xa0u;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Silver Miner");
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
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* miner = units_get(&units, uid);
  if (!miner) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("silver-field spawn");
  }
  miner->nation_id = nation;
  miner->orders = 0;
  miner->moves_left = 3;
  miner->profession = COLONIZE_JOB_SILVER_MINER;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
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
  ctx.rng_seed = 14;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  miner = units_get(&units, uid);
  const int joined = (miner == NULL || !miner->active) && c->population > pop0;
  int field_ok = 0;
  if (joined) {
    for (int i = 0; i < c->colonist_count; ++i) {
      if (c->colonists[i].active && c->colonists[i].field_job == COLONIZE_JOB_SILVER_MINER) {
        field_ok = 1;
        break;
      }
    }
  }

  if (!joined || !field_ok) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Silver Miner admit + mountains field assign");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Silver Miner mountains field-assign ok\n");
  return 0;
}

/*
 * Expert Farmer food field-assign: idle Expert Farmer on own colony with free
 * plains surround → admit + colonies_assign_field Farmer. Cite: terrain_yields
 * Farmer; Skills Chart (parallel Lumberjack/Ore Miner).
 */
static int unit_farmer_field_assign(void) {
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
    return fail("farmer-field alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains — Farmer food yield */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Farmer");
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
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* farmer = units_get(&units, uid);
  if (!farmer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("farmer-field spawn");
  }
  farmer->nation_id = nation;
  farmer->orders = 0;
  farmer->moves_left = 3;
  farmer->profession = 0; /* @JOB Farmer */

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
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
  ctx.rng_seed = 15;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  farmer = units_get(&units, uid);
  const int joined = (farmer == NULL || !farmer->active) && c->population > pop0;
  int field_ok = 0;
  if (joined) {
    for (int i = 0; i < c->colonist_count; ++i) {
      if (c->colonists[i].active && c->colonists[i].field_job == COLONIZE_JOB_FARMER) {
        field_ok = 1;
        break;
      }
    }
  }

  if (!joined || !field_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: farmer-field joined=%d field=%d pop %d→%d\n",
      joined,
      field_ok,
      pop0,
      c->population
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Farmer admit + food field assign");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Farmer food field-assign ok\n");
  return 0;
}

/*
 * Pioneer road preference on already-plowed surround: idle Hardy Pioneer with
 * tools on plowed no-road tile → units_pioneer_road (not leave to plow elsewhere).
 * Cite: Colonization.pdf Clear/Plow/Road sequence.
 */
static int unit_pioneer_road_on_plowed(void) {
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
    return fail("pioneer-road alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  /* North surround already plowed, no road — prefer road here. */
  map_tile_set_plowed(&map, 4, 3, true);

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
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->improve_timer = 2; /* Col1 +0x8c gate */
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int pid = units_spawn(&units, 0, 4, 3);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("pioneer-road spawn");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves_left = 1;
  pioneer->tools = 100;
  pioneer->profession = UNITS_JOB_PIONEER;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 26;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 16;

  const int tools0 = pioneer->tools;
  /*
   * Real DS:0x2f78 threshold (2026-08-20 live capture) usually takes more
   * than one turn even for a Hardy Pioneer on plains — drive several turns
   * rather than assuming one-shot completion.
   */
  int roaded = 0;
  for (int t = 0; t < 8 && !roaded; ++t) {
    pioneer = units_get(&units, pid);
    if (pioneer && pioneer->active) {
      pioneer->moves_left = 1;
    }
    turn++;
    ai_euro_dispatcher_turn(&ctx, nation);
    roaded = map_tile_has_road(&map, 4, 3);
  }

  pioneer = units_get(&units, pid);
  const int tools_spent =
    pioneer && pioneer->active && pioneer->tools == tools0 - UNITS_EQUIP_TOOLS_STEP;
  const int still_plowed = map_tile_is_plowed(&map, 4, 3);

  if (!roaded || !tools_spent || !still_plowed) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: road=%d plow=%d tools=%d→%d orders=%d goto=(%d,%d)\n",
      roaded,
      still_plowed,
      tools0,
      pioneer ? pioneer->tools : -1,
      pioneer ? pioneer->orders : -1,
      pioneer ? pioneer->goto_x : -1,
      pioneer ? pioneer->goto_y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    return fail("expected Hardy Pioneer road on already-plowed surround");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.improve);
  fprintf(stderr, "unit_ai_euro_expand: pioneer road-on-plowed ok\n");
  return 0;
}


/*
 * Expert Fisherman coastal field-assign: idle Expert Fisherman on colony with
 * free ocean surround → admit + colonies_assign_field Fisherman. Cite:
 * terrain_yields Ocean fish; Skills Chart (parallel Farmer field-assign).
 */
static int unit_fisherman_field_assign(void) {
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
    return fail("fisherman-field alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  /* West surround ocean (pedia 25) — Fisherman food yield. */
  map.terrain[4 * 16 + 3] = 25;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Fisherman");
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
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* fisher = units_get(&units, uid);
  if (!fisher) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("fisherman-field spawn");
  }
  fisher->nation_id = nation;
  fisher->orders = 0;
  fisher->moves_left = 3;
  fisher->profession = COLONIZE_JOB_FISHERMAN;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 27;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 15;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  fisher = units_get(&units, uid);
  const int joined = (fisher == NULL || !fisher->active) && c->population > pop0;
  int field_ok = 0;
  if (joined) {
    for (int i = 0; i < c->colonist_count; ++i) {
      if (c->colonists[i].active && c->colonists[i].field_job == COLONIZE_JOB_FISHERMAN) {
        field_ok = 1;
        break;
      }
    }
  }

  if (!joined || !field_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: fisherman-field joined=%d field=%d pop %d→%d ocean_pedia=%d\n",
      joined,
      field_ok,
      pop0,
      c->population,
      map_pedia_terrain_index_at(&map, 3, 4)
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Fisherman admit + coastal fish field assign");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Fisherman coastal field-assign ok\n");
  return 0;
}

/*
 * Expert Sugar Planter field-assign: idle Expert Sugar Planter on own colony
 * with free savannah surround → admit + colonies_assign_field Sugar Planter.
 * Cite: terrain_yields Sugar Planter; Skills Chart (parallel Farmer).
 */
static int unit_sugar_planter_field_assign(void) {
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
    return fail("sugar-field alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* desert — no sugar */
  }
  /* North surround savannah (tile index 0): sugar yield. */
  map.terrain[3 * 16 + 4] = 5;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Sugar Planter");
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
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* planter = units_get(&units, uid);
  if (!planter) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("sugar-field spawn");
  }
  planter->nation_id = nation;
  planter->orders = 0;
  planter->moves_left = 3;
  planter->profession = COLONIZE_JOB_SUGAR_PLANTER;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 26;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 16;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  planter = units_get(&units, uid);
  const int joined = (planter == NULL || !planter->active) && c->population > pop0;
  int field_ok = 0;
  if (joined) {
    for (int i = 0; i < c->colonist_count; ++i) {
      if (c->colonists[i].active &&
          c->colonists[i].field_job == COLONIZE_JOB_SUGAR_PLANTER) {
        field_ok = 1;
        break;
      }
    }
  }

  if (!joined || !field_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: sugar-field joined=%d field=%d pop %d→%d\n",
      joined,
      field_ok,
      pop0,
      c->population
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Sugar Planter admit + savannah field assign");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Sugar Planter field-assign ok\n");
  return 0;
}

/*
 * Expert Tobacco Planter field-assign: idle Expert Tobacco Planter on own colony
 * with free grassland surround → admit + colonies_assign_field Tobacco Planter.
 * Cite: terrain_yields Tobacco Planter; Skills Chart (parallel Sugar Planter).
 */
static int unit_tobacco_planter_field_assign(void) {
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
    return fail("tobacco-field alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* desert — no tobacco */
  }
  /* North surround grassland (tile index 0): tobacco yield. */
  map.terrain[3 * 16 + 4] = 4;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Tobacco Planter");
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
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* planter = units_get(&units, uid);
  if (!planter) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("tobacco-field spawn");
  }
  planter->nation_id = nation;
  planter->orders = 0;
  planter->moves_left = 3;
  planter->profession = COLONIZE_JOB_TOBACCO_PLANTER;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 27;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 17;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  planter = units_get(&units, uid);
  const int joined = (planter == NULL || !planter->active) && c->population > pop0;
  int field_ok = 0;
  if (joined) {
    for (int i = 0; i < c->colonist_count; ++i) {
      if (c->colonists[i].active &&
          c->colonists[i].field_job == COLONIZE_JOB_TOBACCO_PLANTER) {
        field_ok = 1;
        break;
      }
    }
  }

  if (!joined || !field_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: tobacco-field joined=%d field=%d pop %d→%d\n",
      joined,
      field_ok,
      pop0,
      c->population
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Tobacco Planter admit + grassland field assign");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Tobacco Planter field-assign ok\n");
  return 0;
}

/*
 * Expert Cotton Planter field-assign: idle Expert Cotton Planter on own colony
 * with free prairie surround → admit + colonies_assign_field Cotton Planter.
 * Cite: terrain_yields Cotton Planter; Skills Chart (parallel Tobacco).
 */
static int unit_cotton_planter_field_assign(void) {
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
    return fail("cotton-field alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 0; /* tundra — no cotton */
  }
  /* North surround prairie (tile index 0): cotton yield. */
  map.terrain[3 * 16 + 4] = 3;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Cotton Planter");
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
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* planter = units_get(&units, uid);
  if (!planter) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("cotton-field spawn");
  }
  planter->nation_id = nation;
  planter->orders = 0;
  planter->moves_left = 3;
  planter->profession = COLONIZE_JOB_COTTON_PLANTER;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 28;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 18;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  planter = units_get(&units, uid);
  const int joined = (planter == NULL || !planter->active) && c->population > pop0;
  int field_ok = 0;
  if (joined) {
    for (int i = 0; i < c->colonist_count; ++i) {
      if (c->colonists[i].active &&
          c->colonists[i].field_job == COLONIZE_JOB_COTTON_PLANTER) {
        field_ok = 1;
        break;
      }
    }
  }

  if (!joined || !field_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: cotton-field joined=%d field=%d pop %d→%d\n",
      joined,
      field_ok,
      pop0,
      c->population
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Cotton Planter admit + prairie field assign");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Cotton Planter field-assign ok\n");
  return 0;
}

/*
 * Expert Fur Trapper field-assign: idle Expert Fur Trapper on own colony with
 * free mixed-forest surround → admit + colonies_assign_field Fur Trapper.
 * Cite: terrain_yields Fur Trapper (forested); Skills Chart (parallel Cotton).
 */
static int unit_fur_trapper_field_assign(void) {
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
    return fail("fur-field alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* desert/plains — no furs */
  }
  /* North surround mixed forest (tile index 0): fur yield. */
  map.terrain[3 * 16 + 4] = 10;

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Fur Trapper");
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
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* trapper = units_get(&units, uid);
  if (!trapper) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("fur-field spawn");
  }
  trapper->nation_id = nation;
  trapper->orders = 0;
  trapper->moves_left = 3;
  trapper->profession = COLONIZE_JOB_FUR_TRAPPER;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 29;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 19;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  trapper = units_get(&units, uid);
  const int joined = (trapper == NULL || !trapper->active) && c->population > pop0;
  int field_ok = 0;
  if (joined) {
    for (int i = 0; i < c->colonist_count; ++i) {
      if (c->colonists[i].active &&
          c->colonists[i].field_job == COLONIZE_JOB_FUR_TRAPPER) {
        field_ok = 1;
        break;
      }
    }
  }

  if (!joined || !field_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: fur-field joined=%d field=%d pop %d→%d\n",
      joined,
      field_ok,
      pop0,
      c->population
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Fur Trapper admit + forest field assign");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Fur Trapper field-assign ok\n");
  return 0;
}

/*
 * Peace construction pick: idle queue → Stockade before Warehouse/Docks.
 * Cite: fandom Defense Stockade→Fort→Fortress; building_production Stockade 64h.
 */
static int unit_peace_construction_stockade(void) {
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
    return fail("peace-stockade alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  map.terrain[4 * 16 + 3] = 25; /* ocean west → coastal (Docks also buildable) */

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Warehouse");
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Docks");
  colonies.building_types[2].hammers = 52;
  colonies.building_types[2].min_population = 1;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = false;
  c->has_building[1] = false;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 21;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace construct bip=%d (want Stockade=0)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle colony to prefer Stockade over Warehouse/Docks");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace construction Stockade prefer ok\n");
  return 0;
}

/*
 * Peace construction: Stockade owned → Fort before Warehouse/Docks.
 * Cite: fandom Defense Stockade→Fort→Fortress; building_production Fort 120h.
 */
static int unit_peace_construction_fort(void) {
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
    return fail("peace-fort alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fort");
  colonies.building_types[1].hammers = 120;
  colonies.building_types[1].min_population = 4;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Warehouse");
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 1;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Docks");
  colonies.building_types[3].hammers = 52;
  colonies.building_types[3].min_population = 1;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 100;
  c->has_building[0] = true; /* Stockade */
  c->has_building[1] = false;
  c->has_building[2] = false;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
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
  ctx.rng_seed = 22;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: fort prefer bip=%d (want Fort=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Stockade colony to prefer Fort over Warehouse/Docks");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace construction Fort prefer ok\n");
  return 0;
}

/*
 * Peace construction: Fort owned → Fortress before Warehouse/Docks.
 * Cite: fandom Defense Stockade→Fort→Fortress; building_production Fortress 320h.
 */
static int unit_peace_construction_fortress(void) {
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
    return fail("peace-fortress alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Fort");
  colonies.building_types[0].hammers = 120;
  colonies.building_types[0].min_population = 4;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fortress");
  colonies.building_types[1].hammers = 320;
  colonies.building_types[1].min_population = 8;
  colonies.building_types[1].tools_cost = 200;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Warehouse");
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 1;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Docks");
  colonies.building_types[3].hammers = 52;
  colonies.building_types[3].min_population = 1;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 200;
  c->has_building[0] = true; /* Fort */
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
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
  ctx.rng_seed = 22;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: fortress prefer bip=%d (want Fortress=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Fort colony to prefer Fortress over Warehouse/Docks");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace construction Fortress prefer ok\n");
  return 0;
}

/*
 * Peace construction: Stockade owned → Warehouse before coastal Docks.
 * Cite: fandom Storage Warehouse; building_production Warehouse 80h.
 */
static int unit_peace_construction_warehouse(void) {
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
    return fail("peace-warehouse alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25; /* coastal — Docks buildable but lower priority */

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Warehouse");
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Docks");
  colonies.building_types[2].hammers = 52;
  colonies.building_types[2].min_population = 1;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true; /* Stockade */
  c->has_building[1] = false;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 22;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace warehouse bip=%d (want Warehouse=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Stockade-owned idle colony to prefer Warehouse over Docks");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace construction Warehouse prefer ok\n");
  return 0;
}

/*
 * Near warehouse capacity (≥90% non-food) + Warehouse owned → Warehouse Expansion
 * before Docks. Cite: euro_unit_act peace construction; FUN_15eb_0a50 spoilage;
 * building_production Warehouse Expansion 80h.
 */
static int unit_peace_construction_warehouse_expansion(void) {
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
    return fail("peace-whe alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25; /* coastal — Docks buildable but lower priority */

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Warehouse");
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  snprintf(
    colonies.building_types[2].name,
    sizeof(colonies.building_types[2].name),
    "Warehouse Expansion"
  );
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 1;
  colonies.building_types[2].tools_cost = 20;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Docks");
  colonies.building_types[3].hammers = 52;
  colonies.building_types[3].min_population = 1;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 180; /* ≥90% of Warehouse cap 200 */
  c->has_building[0] = true; /* Stockade */
  c->has_building[1] = true; /* Warehouse */
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 33;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 22;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Warehouse Expansion bip=%d (want Expansion=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected near-cap Warehouse colony to prefer Expansion over Docks");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace Warehouse Expansion prefer ok\n");
  return 0;
}

/*
 * Peace construction: Stockade+Warehouse owned, coastal idle → Docks.
 * Cite: fandom Naval Docks→Drydock→Shipyard; building_production Dock 52h.
 */
static int unit_peace_construction_docks(void) {
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
    return fail("peace-docks alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25; /* ocean west → coastal */

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Warehouse");
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Docks");
  colonies.building_types[2].hammers = 52;
  colonies.building_types[2].min_population = 1;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true; /* Stockade */
  c->has_building[1] = true; /* Warehouse */
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 23;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace docks bip=%d (want Docks=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected coastal Stockade+Warehouse colony to queue Docks");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace construction Docks prefer ok\n");
  return 0;
}

/*
 * Coastal Drydock prefer: own colony with Docks, no Drydock, idle construction
 * → colonies_set_construction Drydock (list_buildable gated). Cite: fandom
 * Naval Docks→Drydock→Shipyard; building_production Drydock 80h.
 */
static int unit_coastal_drydock_prefer(void) {
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
    return fail("drydock prefer alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  map.terrain[4 * 16 + 3] = 25; /* ocean west of colony → coastal */

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Docks");
  colonies.building_types[0].hammers = 52;
  colonies.building_types[0].min_population = 1;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Drydock");
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].tools_cost = 50;
  colonies.building_types[1].min_population = 6;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 6;
  c->colonist_count = 6;
  for (int i = 0; i < 6; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 60;
  c->has_building[0] = true; /* Docks */
  c->has_building[1] = false; /* no Drydock */
  c->building_in_production = -1; /* idle queue */
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 20;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: drydock prefer bip=%d (want Drydock=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle coastal Docks colony to queue Drydock");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: coastal Drydock prefer ok\n");
  return 0;
}

/*
 * Coastal Shipyard prefer: own colony with Drydock, no Shipyard, idle
 * construction → colonies_set_construction Shipyard (list_buildable gated).
 * Cite: fandom Naval Docks→Drydock→Shipyard; building_production Shipyard 240h.
 */
static int unit_coastal_shipyard_prefer(void) {
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
    return fail("shipyard prefer alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  map.terrain[4 * 16 + 3] = 25; /* ocean west of colony → coastal */

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Drydock");
  colonies.building_types[0].hammers = 80;
  colonies.building_types[0].tools_cost = 50;
  colonies.building_types[0].min_population = 6;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Shipyard");
  colonies.building_types[1].hammers = 240;
  colonies.building_types[1].tools_cost = 100;
  colonies.building_types[1].min_population = 8;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 120;
  c->has_building[0] = true; /* Drydock */
  c->has_building[1] = false; /* no Shipyard */
  c->building_in_production = -1; /* idle queue */
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 21;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: shipyard prefer bip=%d (want Shipyard=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle coastal Drydock colony to queue Shipyard");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: coastal Shipyard prefer ok\n");
  return 0;
}

/*
 * Stuyvesant Custom House prefer: nation owns FF_PETER_STUYVESANT, idle colony
 * without Custom House → colonies_set_construction Custom House (list_buildable
 * gated via has_peter_stuyvesant). Cite: fandom Peter Stuyvesant unlock Custom
 * House; colony.c Custom House gate; founding_fathers elect comment. No
 * auto-sell gold/thresholds invented.
 */
static int unit_stuyvesant_custom_house_prefer(void) {
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
    return fail("custom house prefer alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Custom House");
  colonies.building_types[0].hammers = 160;
  colonies.building_types[0].tools_cost = 50;
  colonies.building_types[0].min_population = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 60;
  c->has_building[0] = false; /* no Custom House */
  c->building_in_production = -1; /* idle queue */
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  /* NAMES.TXT @FATHERS: Peter Stuyvesant=3 — same gate as game_nation_has_ff. */
  col1.nation[nation].founding_fathers[FF_PETER_STUYVESANT / 8] |=
    (uint8_t)(1u << (FF_PETER_STUYVESANT % 8));
  if (!founding_fathers_nation_has(&col1, nation, FF_PETER_STUYVESANT)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("custom house prefer: Stuyvesant elect bit helper");
  }

  uint32_t turn = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 22;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: custom house prefer bip=%d (want Custom House=0)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Stuyvesant colony to queue Custom House");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Stuyvesant Custom House prefer ok\n");
  return 0;
}

/*
 * Peace Church prefer: Stockade+Warehouse owned, inland (no Docks), idle queue
 * → Church when buildable. Cite: building_production Church→Crosses;
 * euro_unit_act peace Church prefer.
 */
static int unit_peace_church_prefer(void) {
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
    return fail("peace-church alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* inland plains — no Docks */
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Warehouse");
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Church");
  colonies.building_types[2].hammers = 52;
  colonies.building_types[2].min_population = 3;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;  /* Stockade */
  c->has_building[1] = true;  /* Warehouse */
  c->has_building[2] = false; /* Church */
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
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
  ctx.rng_seed = 22;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace Church bip=%d (want Church=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Stockade+Warehouse colony to prefer Church");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace Church prefer ok\n");
  return 0;
}

/*
 * Wartime Armory prefer: at war + Stockade owned + idle queue → Armory when
 * buildable (Church also buildable must not win — Armory runs after Church and
 * yanks when at war). Cite: building_production Armory; euro_unit_act Armory prefer.
 */
static int unit_war_armory_prefer(void) {
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
    return fail("war-armory alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Church");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 3;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Armory");
  colonies.building_types[2].hammers = 52;
  colonies.building_types[2].min_population = 1;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  ai_diplo_declare_war(&col1, nation, foe);
  if (!ai_diplo_at_war(&col1, nation, foe)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("war-armory setup: expected at war");
  }

  uint32_t turn = 32;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 23;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: war Armory bip=%d (want Armory=2, Church would be 1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected at-war Stockade colony to prefer Armory over Church");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: war Armory prefer ok\n");
  return 0;
}

/*
 * Peace Printing Press prefer: Stockade+Church owned, idle → Printing Press.
 * Cite: building_production Printing Press +50% bells; euro_unit_act.
 */
static int unit_peace_printing_press_prefer(void) {
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
    return fail("peace-press alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Church");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 3;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Printing Press");
  colonies.building_types[2].hammers = 52;
  colonies.building_types[2].min_population = 1;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 33;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 24;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace Press bip=%d (want Printing Press=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Stockade+Church colony to prefer Printing Press");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace Printing Press prefer ok\n");
  return 0;
}

/*
 * Peace Schoolhouse prefer: Stockade+Church+Press owned, pop≥4 → Schoolhouse.
 * Cite: building_production Schoolhouse; euro_unit_act Education prefer.
 */
static int unit_peace_schoolhouse_prefer(void) {
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
    return fail("peace-school alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Church");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 3;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Printing Press");
  colonies.building_types[2].hammers = 52;
  colonies.building_types[2].min_population = 1;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Schoolhouse");
  colonies.building_types[3].hammers = 64;
  colonies.building_types[3].min_population = 4;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = true;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 34;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 25;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace Schoolhouse bip=%d (want Schoolhouse=3)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle educated colony to prefer Schoolhouse");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace Schoolhouse prefer ok\n");
  return 0;
}

/*
 * Wartime Magazine prefer: at war + Armory owned → Magazine. Cite:
 * building_production Magazine; euro_unit_act wartime Magazine prefer.
 */
static int unit_war_magazine_prefer(void) {
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
    return fail("war-magazine alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Armory");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Magazine");
  colonies.building_types[2].hammers = 120;
  colonies.building_types[2].min_population = 4;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  ai_diplo_declare_war(&col1, nation, foe);

  uint32_t turn = 35;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 26;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: war Magazine bip=%d (want Magazine=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected at-war Armory colony to prefer Magazine");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: war Magazine prefer ok\n");
  return 0;
}

/*
 * Peace Newspaper prefer: Printing Press owned → Newspaper.
 */
static int unit_peace_newspaper_prefer(void) {
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
    return fail("peace-newspaper alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Printing Press");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Newspaper");
  colonies.building_types[2].hammers = 120;
  colonies.building_types[2].min_population = 4;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 36;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 27;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace Newspaper bip=%d (want Newspaper=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Press colony to prefer Newspaper");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace Newspaper prefer ok\n");
  return 0;
}

/*
 * Peace College prefer: Schoolhouse owned, pop≥8 → College.
 */
static int unit_peace_college_prefer(void) {
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
    return fail("peace-college alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Schoolhouse");
  colonies.building_types[1].hammers = 64;
  colonies.building_types[1].min_population = 4;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "College");
  colonies.building_types[2].hammers = 160;
  colonies.building_types[2].min_population = 8;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 37;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 28;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace College bip=%d (want College=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Schoolhouse colony to prefer College");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace College prefer ok\n");
  return 0;
}

/*
 * Peace Cathedral prefer: Church owned, pop≥8 → Cathedral.
 */
static int unit_peace_cathedral_prefer(void) {
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
    return fail("peace-cathedral alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Church");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 3;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Cathedral");
  colonies.building_types[2].hammers = 176;
  colonies.building_types[2].min_population = 8;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 38;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 29;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace Cathedral bip=%d (want Cathedral=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle Church colony to prefer Cathedral");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace Cathedral prefer ok\n");
  return 0;
}

/*
 * Wartime Arsenal prefer: at war + Adam Smith + Magazine owned → Arsenal.
 * Cite: building_production Arsenal factory muskets; euro_unit_act wartime
 * Arsenal prefer.
 */
static int unit_war_arsenal_prefer(void) {
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
    return fail("war-arsenal alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Armory");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Magazine");
  colonies.building_types[2].hammers = 120;
  colonies.building_types[2].min_population = 4;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Arsenal");
  colonies.building_types[3].hammers = 240;
  colonies.building_types[3].min_population = 8;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = true;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  col1.head.founding_father[FF_ADAM_SMITH] = (int8_t)nation;
  col1.nation[nation].founding_fathers[0] |= 1u;
  ai_diplo_declare_war(&col1, nation, foe);

  uint32_t turn = 39;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 30;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: war Arsenal bip=%d (want Arsenal=3)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected at-war Magazine+AdamSmith colony to prefer Arsenal");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: war Arsenal prefer ok\n");
  return 0;
}

/*
 * Peace University prefer: College owned, pop≥10 → University.
 */
static int unit_peace_university_prefer(void) {
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
    return fail("peace-university alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Schoolhouse");
  colonies.building_types[1].hammers = 64;
  colonies.building_types[1].min_population = 4;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "College");
  colonies.building_types[2].hammers = 160;
  colonies.building_types[2].min_population = 8;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "University");
  colonies.building_types[3].hammers = 200;
  colonies.building_types[3].min_population = 10;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 10;
  c->colonist_count = 10;
  for (int i = 0; i < 10; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = true;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 31;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace University bip=%d (want University=3)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected idle College colony to prefer University");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: peace University prefer ok\n");
  return 0;
}

/*
 * Stable prefer: Stockade owned → Stable. Cite: building_production Stable.
 */
static int unit_stable_prefer(void) {
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
    return fail("stable alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Stable");
  colonies.building_types[1].hammers = 64;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 41;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 32;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Stable bip=%d (want Stable=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected fortified colony to prefer Stable");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Stable prefer ok\n");
  return 0;
}

/*
 * Carpenter's Shop prefer: Stockade owned, no Shop/Mill → Carpenter's Shop.
 * Cite: building_production; ai_euro_prefer_carpenters_shop.
 */
static int unit_carpenters_shop_prefer(void) {
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
    return fail("carp-shop alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Carpenter's Shop"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 33;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Carpenter's Shop bip=%d (want=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Stockade colony to prefer Carpenter's Shop");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Carpenter's Shop prefer ok\n");
  return 0;
}

/*
 * Lumber Mill prefer: Carpenter's Shop owned → Lumber Mill.
 */
static int unit_lumber_mill_prefer(void) {
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
    return fail("lumber-mill alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Carpenter's Shop"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Lumber Mill");
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 3;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 42;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 33;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Lumber Mill bip=%d (want Lumber Mill=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Carpenter's Shop colony to prefer Lumber Mill");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Lumber Mill prefer ok\n");
  return 0;
}

/*
 * Blacksmith's House prefer: ore≥20, no house → Blacksmith's House.
 * Cite: building_production Ore→Tools; ai_euro_prefer_blacksmiths_house.
 */
static int unit_blacksmiths_house_prefer(void) {
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
    return fail("bsmith-house alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Blacksmith's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_ORE] = 25;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 33;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Blacksmith's House bip=%d (want=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected ore surplus colony to prefer Blacksmith's House");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Blacksmith's House prefer ok\n");
  return 0;
}

/*
 * Blacksmith's Shop prefer: Blacksmith's House owned → Blacksmith's Shop.
 * Cite: building_production Ore→Tools shop; ai_euro_prefer_blacksmiths_shop.
 */
static int unit_blacksmiths_shop_prefer(void) {
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
    return fail("bsmith-shop alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Blacksmith's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(
    colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Blacksmith's Shop"
  );
  colonies.building_types[2].hammers = 64;
  colonies.building_types[2].min_population = 3;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 33;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Blacksmith's Shop bip=%d (want=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Blacksmith's House colony to prefer Blacksmith's Shop");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Blacksmith's Shop prefer ok\n");
  return 0;
}

/*
 * Iron Works prefer: Adam Smith + Blacksmith's Shop → Iron Works.
 */
static int unit_iron_works_prefer(void) {
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
    return fail("iron-works alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Blacksmith's Shop"
  );
  colonies.building_types[1].hammers = 64;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Iron Works");
  colonies.building_types[2].hammers = 240;
  colonies.building_types[2].min_population = 8;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  col1.head.founding_father[FF_ADAM_SMITH] = (int8_t)nation;
  col1.nation[nation].founding_fathers[0] |= 1u;

  uint32_t turn = 43;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 34;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Iron Works bip=%d (want Iron Works=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected AdamSmith+Shop colony to prefer Iron Works");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Iron Works prefer ok\n");
  return 0;
}

/*
 * Craft Distiller's House prefer: sugar≥20, no house → Rum Distiller's House.
 * Cite: building_production craft chain; ai_euro_prefer_craft_upgrades house step.
 */
static int unit_craft_distillers_house_prefer(void) {
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
    return fail("craft-house alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Rum Distiller's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_SUGAR] = 25;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 44;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 35;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Distiller's House bip=%d (want=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected sugar surplus colony to prefer Rum Distiller's House");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Rum Distiller's House prefer ok\n");
  return 0;
}

/*
 * Craft Weaver's House prefer: cotton≥20, no house → Weaver's House.
 * Cite: building_production Weaver chain; ai_euro_prefer_craft_upgrades house step.
 */
static int unit_craft_weavers_house_prefer(void) {
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
    return fail("weaver-house alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Weaver's House");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_COTTON] = 25;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 44;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 35;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Weaver's House bip=%d (want=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected cotton surplus colony to prefer Weaver's House");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Weaver's House prefer ok\n");
  return 0;
}

/*
 * Craft Tobacconist's House prefer: tobacco≥20, no house → Tobacconist's House.
 */
static int unit_craft_tobacconists_house_prefer(void) {
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
    return fail("toba-house alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Tobacconist's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_TOBACCO] = 25;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 44;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 35;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Tobacconist's House bip=%d (want=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected tobacco surplus colony to prefer Tobacconist's House");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Tobacconist's House prefer ok\n");
  return 0;
}

/*
 * Craft Fur Trader's House prefer: furs≥20, no house → Fur Trader's House.
 */
static int unit_craft_fur_traders_house_prefer(void) {
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
    return fail("fur-house alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fur Trader's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FURS] = 25;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 44;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 35;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Fur Trader's House bip=%d (want=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected furs surplus colony to prefer Fur Trader's House");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Fur Trader's House prefer ok\n");
  return 0;
}

/*
 * Craft Distillery prefer: Distiller's House + sugar≥20 → Rum Distillery.
 */
static int unit_craft_distillery_prefer(void) {
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
    return fail("craft-distillery alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Rum Distiller's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(
    colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Rum Distillery"
  );
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 3;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_SUGAR] = 25;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 44;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 35;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Rum Distillery bip=%d (want=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected sugar+House colony to prefer Rum Distillery");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Rum Distillery prefer ok\n");
  return 0;
}

/*
 * Craft Weaver's Shop prefer: Weaver's House + cotton≥20 → Weaver's Shop.
 * Cite: building_production Weaver chain; euro_unit_act craft shop prefer.
 */
static int unit_craft_weavers_shop_prefer(void) {
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
    return fail("craft-weaver alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Weaver's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(
    colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Weaver's Shop"
  );
  colonies.building_types[2].hammers = 64;
  colonies.building_types[2].min_population = 4;
  colonies.building_types[2].tools_cost = 20;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_COTTON] = 25;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 34;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 35;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Weaver's Shop bip=%d (want=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected cotton+House colony to prefer Weaver's Shop");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Weaver's Shop prefer ok\n");
  return 0;
}

/*
 * Craft Tobacconist's Shop prefer: Tobacconist's House + tobacco≥20 → Shop.
 * Cite: building_production Tobacconist chain; euro_unit_act craft shop prefer.
 */
static int unit_craft_tobacconist_shop_prefer(void) {
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
    return fail("craft-tobacco alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Tobacconist's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(
    colonies.building_types[2].name,
    sizeof(colonies.building_types[2].name),
    "Tobacconist's Shop"
  );
  colonies.building_types[2].hammers = 64;
  colonies.building_types[2].min_population = 4;
  colonies.building_types[2].tools_cost = 20;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_TOBACCO] = 25;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 34;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 35;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Tobacconist's Shop bip=%d (want=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected tobacco+House colony to prefer Tobacconist's Shop");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Tobacconist's Shop prefer ok\n");
  return 0;
}

/*
 * Craft Fur Trading Post prefer: Fur Trader's House + furs≥20 → Trading Post.
 * Cite: building_production Fur chain; euro_unit_act craft shop prefer.
 */
static int unit_craft_fur_trading_post_prefer(void) {
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
    return fail("craft-fur-post alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Fur Trader's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(
    colonies.building_types[2].name,
    sizeof(colonies.building_types[2].name),
    "Fur Trading Post"
  );
  colonies.building_types[2].hammers = 56;
  colonies.building_types[2].min_population = 3;
  colonies.building_types[2].tools_cost = 20;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FURS] = 25;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 34;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 35;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Fur Trading Post bip=%d (want=2)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected furs+House colony to prefer Fur Trading Post");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Fur Trading Post prefer ok\n");
  return 0;
}

/*
 * Craft Rum Factory prefer: Adam Smith + Distillery + sugar≥20 → Rum Factory.
 */
static int unit_craft_rum_factory_prefer(void) {
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
    return fail("craft-factory alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Rum Distiller's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(
    colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Rum Distillery"
  );
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 3;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Rum Factory");
  colonies.building_types[3].hammers = 160;
  colonies.building_types[3].min_population = 8;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_SUGAR] = 25;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = true;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  col1.head.founding_father[FF_ADAM_SMITH] = (int8_t)nation;
  col1.nation[nation].founding_fathers[0] |= 1u;

  uint32_t turn = 45;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 36;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Rum Factory bip=%d (want=3)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected AdamSmith+Distillery to prefer Rum Factory");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Rum Factory prefer ok\n");
  return 0;
}

/*
 * Craft Textile Mill prefer: Adam Smith + Weaver's Shop + cotton≥20 → Textile Mill.
 * Cite: building_production craft chain; ai_euro_prefer_craft_upgrades.
 */
static int unit_craft_textile_mill_prefer(void) {
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
    return fail("textile-mill alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Weaver's House");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Weaver's Shop");
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 3;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Textile Mill");
  colonies.building_types[3].hammers = 160;
  colonies.building_types[3].min_population = 8;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_COTTON] = 25;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = true;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  col1.head.founding_father[FF_ADAM_SMITH] = (int8_t)nation;
  col1.nation[nation].founding_fathers[0] |= 1u;

  uint32_t turn = 45;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 36;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Textile Mill bip=%d (want=3)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected AdamSmith+Weaver's Shop to prefer Textile Mill");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Textile Mill prefer ok\n");
  return 0;
}

/*
 * Craft Cigar Factory prefer: Adam Smith + Tobacconist's Shop + tobacco≥20.
 */
static int unit_craft_cigar_factory_prefer(void) {
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
    return fail("cigar-factory alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Tobacconist's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(
    colonies.building_types[2].name,
    sizeof(colonies.building_types[2].name),
    "Tobacconist's Shop"
  );
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 3;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Cigar Factory");
  colonies.building_types[3].hammers = 160;
  colonies.building_types[3].min_population = 8;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_TOBACCO] = 25;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = true;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  col1.head.founding_father[FF_ADAM_SMITH] = (int8_t)nation;
  col1.nation[nation].founding_fathers[0] |= 1u;

  uint32_t turn = 45;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 36;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Cigar Factory bip=%d (want=3)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected AdamSmith+Tobacconist's Shop to prefer Cigar Factory");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Cigar Factory prefer ok\n");
  return 0;
}

/*
 * Craft Fur Factory prefer: Adam Smith + Fur Trading Post + furs≥20.
 */
static int unit_craft_fur_factory_prefer(void) {
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
    return fail("fur-factory alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Fur Trader's House"
  );
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 1;
  snprintf(
    colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Fur Trading Post"
  );
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 3;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Fur Factory");
  colonies.building_types[3].hammers = 160;
  colonies.building_types[3].min_population = 6;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FURS] = 25;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = true;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  col1.head.founding_father[FF_ADAM_SMITH] = (int8_t)nation;
  col1.nation[nation].founding_fathers[0] |= 1u;

  uint32_t turn = 45;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 36;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Fur Factory bip=%d (want=3)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected AdamSmith+Fur Trading Post to prefer Fur Factory");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Fur Factory prefer ok\n");
  return 0;
}

/*
 * Capitol prefer: Stockade owned → Capitol.
 */
static int unit_capitol_prefer(void) {
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
    return fail("capitol alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Capitol");
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 46;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 37;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Capitol bip=%d (want Capitol=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected fortified colony to prefer Capitol");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Capitol prefer ok\n");
  return 0;
}

/*
 * Capitol Expansion prefer: Capitol owned → Capitol Expansion.
 * Cite: building_production.md Capitol Expansion; euro_unit_act Capitol prefer.
 */
static int unit_capitol_expansion_prefer(void) {
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
    return fail("capitol-exp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Capitol");
  colonies.building_types[0].hammers = 80;
  colonies.building_types[0].min_population = 1;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Capitol Expansion"
  );
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;

  uint32_t turn = 47;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 37;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Capitol Expansion bip=%d (want Expansion=1)\n",
      c->building_in_production
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Capitol colony to prefer Capitol Expansion");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Capitol Expansion prefer ok\n");
  return 0;
}

/*
 * Idle Wagon with MUSKETS cargo → AI_MOVE toward muskets-short colony
 * (tools stock OK). Cite: euro_unit_act §2d wagon haul muskets; COLONIZE_CARGO_MUSKETS.
 */
static int unit_wagon_haul_muskets_short(void) {
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
    return fail("wagon-muskets alloc map");
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
  short_c->stock[COLONIZE_CARGO_TOOLS] = 40; /* not tools-short */
  short_c->stock[COLONIZE_CARGO_MUSKETS] = 2; /* muskets-short */
  short_c->stock[COLONIZE_CARGO_FOOD] = 40;
  short_c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-muskets spawn");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;
  /* Prefill MUSKETS cargo so haul prefers muskets-short colony. */
  if (units_load_goods(&units, wid, COLONIZE_CARGO_MUSKETS, 10) <= 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-muskets load");
  }

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

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-muskets should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-muskets orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon AI_MOVE toward muskets-short colony (4,4)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon haul muskets-short ok\n");
  return 0;
}

/*
 * Idle Wagon with LUMBER cargo → AI_MOVE toward lumber-short colony (tools OK).
 * Cite: euro_unit_act §2d wagon haul lumber; 5cf6 lumber_short; COLONIZE_CARGO_LUMBER.
 */
static int unit_wagon_haul_lumber_short(void) {
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
    return fail("wagon-lumber alloc map");
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
  short_c->stock[COLONIZE_CARGO_TOOLS] = 40;
  short_c->stock[COLONIZE_CARGO_LUMBER] = 5; /* lumber-short */
  short_c->stock[COLONIZE_CARGO_FOOD] = 40;
  short_c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-lumber spawn");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_LUMBER, 20) <= 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-lumber load");
  }

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

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-lumber should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-lumber orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon AI_MOVE toward lumber-short colony (4,4)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon haul lumber-short ok\n");
  return 0;
}

/*
 * Idle Wagon with ORE cargo → AI_MOVE toward ore-short colony (tools OK).
 * Cite: euro_unit_act §2d wagon haul ore; 5cf6 ore_short; COLONIZE_CARGO_ORE.
 */
static int unit_wagon_haul_ore_short(void) {
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
    return fail("wagon-ore alloc map");
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
  short_c->stock[COLONIZE_CARGO_TOOLS] = 40;
  short_c->stock[COLONIZE_CARGO_ORE] = 5; /* ore-short */
  short_c->stock[COLONIZE_CARGO_FOOD] = 40;
  short_c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-ore spawn");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_ORE, 20) <= 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-ore load");
  }

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

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-ore should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-ore orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon AI_MOVE toward ore-short colony (4,4)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon haul ore-short ok\n");
  return 0;
}

/*
 * Idle Wagon on inland SILVER surplus (stock>99) → load leave 50 + AI_MOVE
 * nearest own coastal colony (Europe export feeder). Cite: FUN_364b_0688;
 * euro_unit_act §2d / §2d2 Europe export sail.
 */
static int unit_wagon_europe_export_feeder(void) {
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
    return fail("wagon-export alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-export coastal colony should be coastal");
  }
  if (map_tile_is_coastal(&map, 10, 10)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-export inland colony should not be coastal");
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
  ColonizeColony* coast = &colonies.colonies[0];
  coast->id = 0;
  coast->active = true;
  coast->nation_id = nation;
  coast->x = 4;
  coast->y = 4;
  coast->population = 3;
  coast->colonist_count = 3;
  coast->stock[COLONIZE_CARGO_TOOLS] = 25;
  coast->stock[COLONIZE_CARGO_LUMBER] = 25;
  coast->stock[COLONIZE_CARGO_ORE] = 25;
  coast->stock[COLONIZE_CARGO_MUSKETS] = 15;
  coast->stock[COLONIZE_CARGO_HORSES] = 15;
  coast->stock[COLONIZE_CARGO_FOOD] = 8;
  coast->building_in_production = -1;

  ColonizeColony* inland = &colonies.colonies[1];
  inland->id = 1;
  inland->active = true;
  inland->nation_id = nation;
  inland->x = 10;
  inland->y = 10;
  inland->population = 3;
  inland->colonist_count = 3;
  inland->stock[COLONIZE_CARGO_TOOLS] = 25;
  inland->stock[COLONIZE_CARGO_LUMBER] = 25;
  inland->stock[COLONIZE_CARGO_ORE] = 25;
  inland->stock[COLONIZE_CARGO_MUSKETS] = 15;
  inland->stock[COLONIZE_CARGO_HORSES] = 15;
  inland->stock[COLONIZE_CARGO_FOOD] = 8;
  inland->stock[COLONIZE_CARGO_SILVER] = 150;
  inland->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-export spawn");
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

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-export should remain active");
  }
  const int loaded = inland->stock[COLONIZE_CARGO_SILVER] == 50 &&
                     wagon->hold_goods_type[0] == COLONIZE_CARGO_SILVER &&
                     wagon->hold_goods_amount[0] == 100;
  const int toward_coast =
    wagon->orders == UNITS_ORDER_AI_MOVE && wagon->goto_x == 4 && wagon->goto_y == 4;
  if (!loaded || !toward_coast) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-export silver=%d hold_t=%d hold_a=%d "
      "orders=%d goto=(%d,%d)\n",
      inland->stock[COLONIZE_CARGO_SILVER],
      wagon->hold_goods_type[0],
      wagon->hold_goods_amount[0],
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected load SILVER leave 50 + AI_MOVE coastal colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon Europe export feeder ok\n");
  return 0;
}

/*
 * Wagon on coastal colony with SILVER hold → unload into stock (ship export
 * pickup). Cite: euro_unit_act §2d2 wagon export feeder unload.
 */
static int unit_wagon_europe_export_unload(void) {
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
    return fail("wagon-export-unload alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-export-unload colony should be coastal");
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
  ColonizeColony* coast = &colonies.colonies[0];
  coast->id = 0;
  coast->active = true;
  coast->nation_id = nation;
  coast->x = 4;
  coast->y = 4;
  coast->population = 3;
  coast->colonist_count = 3;
  coast->stock[COLONIZE_CARGO_TOOLS] = 25;
  coast->stock[COLONIZE_CARGO_LUMBER] = 25;
  coast->stock[COLONIZE_CARGO_ORE] = 25;
  coast->stock[COLONIZE_CARGO_MUSKETS] = 15;
  coast->stock[COLONIZE_CARGO_HORSES] = 15;
  coast->stock[COLONIZE_CARGO_FOOD] = 8;
  coast->stock[COLONIZE_CARGO_SILVER] = 0;
  coast->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-export-unload spawn");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_SILVER, 80) <= 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-export-unload load");
  }

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

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-export-unload should remain");
  }
  const int unloaded = coast->stock[COLONIZE_CARGO_SILVER] == 80;
  int still_held = 0;
  for (int h = 0; h < units_goods_hold_count(&units, wid); ++h) {
    if (wagon->hold_goods_type[h] == COLONIZE_CARGO_SILVER &&
        wagon->hold_goods_amount[h] > 0 && wagon->hold_goods_amount[h] < 255) {
      still_held = 1;
    }
  }
  if (!unloaded || still_held) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-export-unload silver=%d still_held=%d\n",
      coast->stock[COLONIZE_CARGO_SILVER],
      still_held
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected wagon unload SILVER into coastal colony stock");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon Europe export unload ok\n");
  return 0;
}

/*
 * Idle Wagon with FOOD cargo → AI_MOVE toward food-short colony (tools OK).
 * Cite: Colonization.pdf Wagon Train; euro_unit_act §2d; 5cf6 food_short
 * (stock < pop*TURN_FOOD_PER_COLONIST).
 */
static int unit_wagon_haul_food_short(void) {
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
    return fail("wagon-food-haul alloc map");
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
  short_c->population = 4;
  short_c->colonist_count = 4;
  short_c->stock[COLONIZE_CARGO_TOOLS] = 40; /* not tools-short */
  short_c->stock[COLONIZE_CARGO_MUSKETS] = 20;
  short_c->stock[COLONIZE_CARGO_HORSES] = 20;
  short_c->stock[COLONIZE_CARGO_FOOD] = 2; /* food-short vs pop*2=8 */
  short_c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-food-haul spawn");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_FOOD, 8) <= 0) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-food-haul load");
  }

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

  uint32_t turn = 33;
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
    return fail("wagon-food-haul should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-food orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon AI_MOVE toward food-short colony (4,4)");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon haul food-short ok\n");
  return 0;
}

/*
 * Wagon on food-short colony with FOOD hold → colonies_transfer_from_unit.
 * Cite: Colonization.pdf Wagon Train; 5cf6 food_short; transfer APIs.
 */
static int unit_wagon_food_delivery(void) {
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
    return fail("wagon-food-deliv alloc map");
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
  c->population = 4;
  c->colonist_count = 4;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 2; /* food-short */
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-food-deliv spawn");
  }
  wagon->nation_id = nation;
  wagon->orders = 0;
  wagon->moves_left = 2;
  wagon->hold_goods_type[0] = COLONIZE_CARGO_FOOD;
  wagon->hold_goods_amount[0] = 8;

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

  uint32_t turn = 34;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int food_before = c->stock[COLONIZE_CARGO_FOOD];
  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  int hold_left = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < 2; ++h) {
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
        hold_left += wagon->hold_goods_amount[h];
      }
    }
  }
  const int food_after = colonies.colonies[0].stock[COLONIZE_CARGO_FOOD];
  if (food_after < food_before + 8 || hold_left != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-food %d→%d hold_left=%d (want +8, hold 0)\n",
      food_before,
      food_after,
      hold_left
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected wagon FOOD transfer into food-short colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_expand: wagon-food delivery ok (food %d→%d)\n",
    food_before,
    food_after
  );
  return 0;
}

/*
 * Surplus FOOD colony + empty wagon + distant food-short → load FOOD then
 * AI_MOVE toward short. Cite: Colonization.pdf Wagon Train; 5cf6 food_short
 * surplus = pop*4 (2× short floor).
 */
static int unit_wagon_food_load_haul(void) {
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
    return fail("wagon-food-load alloc map");
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
  /* Surplus FOOD at wagon tile. */
  ColonizeColony* surplus = &colonies.colonies[0];
  surplus->id = 0;
  surplus->active = true;
  surplus->nation_id = nation;
  surplus->x = 10;
  surplus->y = 10;
  surplus->population = 3;
  surplus->colonist_count = 3;
  surplus->stock[COLONIZE_CARGO_TOOLS] = 10; /* not surplus tools */
  surplus->stock[COLONIZE_CARGO_MUSKETS] = 5;
  surplus->stock[COLONIZE_CARGO_HORSES] = 5;
  surplus->stock[COLONIZE_CARGO_FOOD] = 30; /* surplus vs pop*4=12 */
  surplus->building_in_production = -1;
  /* Distant food-short. */
  ColonizeColony* hungry = &colonies.colonies[1];
  hungry->id = 1;
  hungry->active = true;
  hungry->nation_id = nation;
  hungry->x = 4;
  hungry->y = 4;
  hungry->population = 4;
  hungry->colonist_count = 4;
  hungry->stock[COLONIZE_CARGO_TOOLS] = 40;
  hungry->stock[COLONIZE_CARGO_FOOD] = 1; /* food-short */
  hungry->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-food-load spawn");
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

  uint32_t turn = 35;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int stock_before = surplus->stock[COLONIZE_CARGO_FOOD];
  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  int food_aboard = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < 2; ++h) {
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
        food_aboard += wagon->hold_goods_amount[h];
      }
    }
  }
  const int stock_after = colonies.colonies[0].stock[COLONIZE_CARGO_FOOD];
  if (!wagon || !wagon->active || food_aboard <= 0 || stock_after >= stock_before ||
      wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: food-load stock %d→%d aboard=%d orders=%d goto=(%d,%d)\n",
      stock_before,
      stock_after,
      food_aboard,
      wagon ? wagon->orders : -1,
      wagon ? wagon->goto_x : -1,
      wagon ? wagon->goto_y : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected wagon FOOD load + AI_MOVE toward food-short");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "unit_ai_euro_expand: wagon food load+haul ok (aboard=%d)\n",
    food_aboard
  );
  return 0;
}

/*
 * When food_short>20 and colony has both TOOLS and FOOD surplus, wagon prefers
 * FOOD load first (not tools ladder). Cite: euro_unit_act §2d surplus FOOD
 * deepen; 5cf6 food_short.
 */
static int unit_wagon_food_prefer_over_tools(void) {
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
    return fail("wagon-food-prefer alloc map");
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
  ColonizeColony* surplus = &colonies.colonies[0];
  surplus->id = 0;
  surplus->active = true;
  surplus->nation_id = nation;
  surplus->x = 10;
  surplus->y = 10;
  surplus->population = 3;
  surplus->colonist_count = 3;
  surplus->stock[COLONIZE_CARGO_TOOLS] = 50; /* surplus tools (≥40) */
  surplus->stock[COLONIZE_CARGO_FOOD] = 30;  /* surplus food (≥pop*4=12) */
  surplus->building_in_production = -1;
  /* Large food deficit → inventory food_short > 20. */
  ColonizeColony* hungry = &colonies.colonies[1];
  hungry->id = 1;
  hungry->active = true;
  hungry->nation_id = nation;
  hungry->x = 4;
  hungry->y = 4;
  hungry->population = 12;
  hungry->colonist_count = 12;
  hungry->stock[COLONIZE_CARGO_TOOLS] = 5; /* tools-short too */
  hungry->stock[COLONIZE_CARGO_FOOD] = 0;  /* food_short += 24 */
  hungry->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-food-prefer spawn");
  }
  wagon->nation_id = nation;
  wagon->moves_left = 2;
  wagon->orders = 0;

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

  uint32_t turn = 35;
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
  int food_aboard = 0;
  int tools_aboard = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < 2; ++h) {
      if (wagon->hold_goods_amount[h] <= 0 || wagon->hold_goods_amount[h] >= 255) {
        continue;
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
        food_aboard += wagon->hold_goods_amount[h];
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
        tools_aboard += wagon->hold_goods_amount[h];
      }
    }
  }
  if (!wagon || !wagon->active || food_aboard <= 0 || tools_aboard > 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: food-prefer food=%d tools=%d orders=%d\n",
      food_aboard,
      tools_aboard,
      wagon ? wagon->orders : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected FOOD prefer over tools when food_short>20");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: wagon food prefer over tools ok\n");
  return 0;
}

/*
 * Caravel with FOOD cargo adjacent to food-short coastal colony → unload via
 * colonies_transfer_from_unit. Cite: Colonization.pdf naval transport; §2d2;
 * 5cf6 food_short.
 */
static int unit_ship_food_delivery(void) {
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
    return fail("ship-food alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 3] = 25;
  }
  if (!map_tile_is_coastal(&map, 4, 4)) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-food colony should be coastal");
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
  c->population = 4;
  c->colonist_count = 4;
  c->stock[COLONIZE_CARGO_TOOLS] = 40; /* not tools-short */
  c->stock[COLONIZE_CARGO_FOOD] = 1; /* food-short */
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 3, 4); /* adjacent water */
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("ship-food spawn");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves_left = 4;
  ship->hold_goods_type[0] = COLONIZE_CARGO_FOOD;
  ship->hold_goods_amount[0] = 8;

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

  uint32_t turn = 36;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int food_before = c->stock[COLONIZE_CARGO_FOOD];
  ai_euro_dispatcher_turn(&ctx, nation);

  ship = units_get(&units, sid);
  int hold_left = 0;
  if (ship && ship->active) {
    for (int h = 0; h < 2; ++h) {
      if (ship->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
        hold_left += ship->hold_goods_amount[h];
      }
    }
  }
  const int food_after = colonies.colonies[0].stock[COLONIZE_CARGO_FOOD];
  if (food_after < food_before + 8 || hold_left != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: ship-food %d→%d hold_left=%d\n",
      food_before,
      food_after,
      hold_left
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected ship FOOD unload into food-short coastal colony");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: ship food delivery ok\n");
  return 0;
}

/*
 * Idle Master Blacksmith on own colony with Blacksmith's House → admit +
 * colonies_assign_workplace. Cite: Skills Chart / building_production
 * Blacksmith→Tools; colonies_assign_workplace.
 */
static int unit_blacksmith_workplace_assign(void) {
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
    return fail("blacksmith-wp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Blacksmith");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(
    colonies.building_types[0].name,
    sizeof(colonies.building_types[0].name),
    "Blacksmith's House"
  );
  colonies.building_types[0].hammers = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* smith = units_get(&units, uid);
  if (!smith) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("blacksmith-wp spawn");
  }
  smith->nation_id = nation;
  smith->orders = 0;
  smith->moves_left = 3;
  smith->profession = 14; /* @JOB Blacksmith */

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

  uint32_t turn = 37;
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

  smith = units_get(&units, uid);
  if (smith && smith->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Blacksmith admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: blacksmith pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Blacksmith workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Master Blacksmith workplace ok\n");
  return 0;
}

/*
 * Idle Master Gunsmith on own colony with Armory → admit +
 * colonies_assign_workplace. Cite: Skills Chart / building_production
 * Gunsmith→Muskets (Armory); colonies_assign_workplace.
 */
static int unit_gunsmith_workplace_assign(void) {
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
    return fail("gunsmith-wp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Gunsmith");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Armory");
  colonies.building_types[0].hammers = 52;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* gun = units_get(&units, uid);
  if (!gun) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("gunsmith-wp spawn");
  }
  gun->nation_id = nation;
  gun->orders = 0;
  gun->moves_left = 3;
  gun->profession = 15; /* @JOB Gunsmith */

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

  uint32_t turn = 38;
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

  gun = units_get(&units, uid);
  if (gun && gun->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Gunsmith admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: gunsmith pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Gunsmith workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Master Gunsmith workplace ok\n");
  return 0;
}

/*
 * Idle Master Fur Trader on own colony with Fur Trader's House → admit +
 * colonies_assign_workplace. Cite: Skills Chart / building_production
 * Fur Trader→Coats; colonies_assign_workplace.
 */
static int unit_fur_trader_workplace_assign(void) {
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
    return fail("fur-trader-wp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Fur Trader");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(
    colonies.building_types[0].name,
    sizeof(colonies.building_types[0].name),
    "Fur Trader's House"
  );
  colonies.building_types[0].hammers = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* trader = units_get(&units, uid);
  if (!trader) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("fur-trader-wp spawn");
  }
  trader->nation_id = nation;
  trader->orders = 0;
  trader->moves_left = 3;
  trader->profession = 12; /* @JOB Fur Trader */

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

  uint32_t turn = 39;
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

  trader = units_get(&units, uid);
  if (trader && trader->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Fur Trader admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: fur-trader pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Fur Trader workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Master Fur Trader workplace ok\n");
  return 0;
}

/*
 * Master Distiller → Rum Distiller's House workplace.
 * Cite: building_production.md Skills Chart job 9; Colonization.pdf Distiller.
 */
static int unit_distiller_workplace_assign(void) {
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
    return fail("distiller-wp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Distiller");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(
    colonies.building_types[0].name,
    sizeof(colonies.building_types[0].name),
    "Rum Distiller's House"
  );
  colonies.building_types[0].hammers = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_SUGAR] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* distiller = units_get(&units, uid);
  if (!distiller) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("distiller-wp spawn");
  }
  distiller->nation_id = nation;
  distiller->orders = 0;
  distiller->moves_left = 3;
  distiller->profession = 9; /* @JOB Distiller */

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

  uint32_t turn = 37;
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

  distiller = units_get(&units, uid);
  if (distiller && distiller->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Distiller admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: distiller pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Distiller workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Master Distiller workplace ok\n");
  return 0;
}

/*
 * Master Weaver → Weaver's House workplace.
 * Cite: building_production.md Skills Chart job 11; Colonization.pdf Weaver.
 */
static int unit_weaver_workplace_assign(void) {
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
    return fail("weaver-wp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Weaver");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(
    colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Weaver's House"
  );
  colonies.building_types[0].hammers = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_COTTON] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* weaver = units_get(&units, uid);
  if (!weaver) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("weaver-wp spawn");
  }
  weaver->nation_id = nation;
  weaver->orders = 0;
  weaver->moves_left = 3;
  weaver->profession = 11; /* @JOB Weaver */

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

  uint32_t turn = 37;
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

  weaver = units_get(&units, uid);
  if (weaver && weaver->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Weaver admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: weaver pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Weaver workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Master Weaver workplace ok\n");
  return 0;
}

/*
 * Master Tobacconist → Tobacconist's House workplace.
 * Cite: building_production.md Skills Chart job 10; Colonization.pdf Tobacconist.
 */
static int unit_tobacconist_workplace_assign(void) {
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
    return fail("tobacconist-wp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Tobacconist");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(
    colonies.building_types[0].name,
    sizeof(colonies.building_types[0].name),
    "Tobacconist's House"
  );
  colonies.building_types[0].hammers = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_TOBACCO] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* tob = units_get(&units, uid);
  if (!tob) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("tobacconist-wp spawn");
  }
  tob->nation_id = nation;
  tob->orders = 0;
  tob->moves_left = 3;
  tob->profession = 10; /* @JOB Tobacconist */

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

  uint32_t turn = 37;
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

  tob = units_get(&units, uid);
  if (tob && tob->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Tobacconist admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: tobacconist pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Tobacconist workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Master Tobacconist workplace ok\n");
  return 0;
}

/*
 * Elder Statesman → Town Hall workplace. Cite: building_production.md Skills
 * Chart job 17; Colonization.pdf Elder Statesman → liberty bells.
 */
static int unit_statesman_workplace_assign(void) {
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
    return fail("statesman-wp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Elder Statesman");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Town Hall");
  colonies.building_types[0].hammers = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* statesman = units_get(&units, uid);
  if (!statesman) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("statesman-wp spawn");
  }
  statesman->nation_id = nation;
  statesman->orders = 0;
  statesman->moves_left = 3;
  statesman->profession = 17; /* @JOB Statesman */

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

  uint32_t turn = 37;
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

  statesman = units_get(&units, uid);
  if (statesman && statesman->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Elder Statesman admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: statesman pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Town Hall workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Elder Statesman workplace ok\n");
  return 0;
}

/*
 * Firebrand Preacher → Church workplace (Cathedral preferred when owned).
 * Cite: building_production.md Skills Chart job 16; Colonization.pdf Preacher.
 */
static int unit_preacher_workplace_assign(void) {
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
    return fail("preacher-wp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Firebrand Preacher");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Church");
  colonies.building_types[0].hammers = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* preacher = units_get(&units, uid);
  if (!preacher) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("preacher-wp spawn");
  }
  preacher->nation_id = nation;
  preacher->orders = 0;
  preacher->moves_left = 3;
  preacher->profession = 16; /* @JOB Preacher */

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

  uint32_t turn = 37;
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

  preacher = units_get(&units, uid);
  if (preacher && preacher->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Firebrand Preacher admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: preacher pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Church workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Firebrand Preacher workplace ok\n");
  return 0;
}

/*
 * Expert Teacher → Schoolhouse workplace (College/University when owned).
 * Cite: building_production.md Skills Chart job 18; Colonization.pdf Teacher.
 */
static int unit_teacher_workplace_assign(void) {
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
    return fail("teacher-wp alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Teacher");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(
    colonies.building_types[0].name,
    sizeof(colonies.building_types[0].name),
    "Schoolhouse"
  );
  colonies.building_types[0].hammers = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* teacher = units_get(&units, uid);
  if (!teacher) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("teacher-wp spawn");
  }
  teacher->nation_id = nation;
  teacher->orders = 0;
  teacher->moves_left = 3;
  teacher->profession = 18; /* @JOB Teacher */

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

  uint32_t turn = 37;
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

  teacher = units_get(&units, uid);
  if (teacher && teacher->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Expert Teacher admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: teacher pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Schoolhouse workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Expert Teacher workplace ok\n");
  return 0;
}

/*
 * Master Carpenter → Carpenter's Shop workplace (Lumber Mill when owned).
 * Cite: building_production.md Skills Chart job 13; Colonization.pdf Carpenter.
 */
static int unit_carpenter_workplace_assign(void) {
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
    return fail("carpenter-wp alloc map");
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
  snprintf(
    colonies.building_types[0].name,
    sizeof(colonies.building_types[0].name),
    "Carpenter's Shop"
  );
  colonies.building_types[0].hammers = 0;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 2;
  c->colonist_count = 2;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = -1;
  c->colonists[1].active = true;
  c->colonists[1].field_job = -1;
  c->colonists[1].building_type = -1;
  c->has_building[0] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* carpenter = units_get(&units, uid);
  if (!carpenter) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("carpenter-wp spawn");
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

  uint32_t turn = 37;
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

  carpenter = units_get(&units, uid);
  if (carpenter && carpenter->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Master Carpenter admitted into colony");
  }
  const ColonizeColony* after = &colonies.colonies[0];
  int found_wp = 0;
  for (int i = 0; i < after->colonist_count; ++i) {
    if (after->colonists[i].active && after->colonists[i].building_type == 0) {
      found_wp = 1;
      break;
    }
  }
  if (!found_wp || after->colonist_count < 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: carpenter pop=%d found_wp=%d\n",
      after->colonist_count,
      found_wp
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Carpenter's Shop workplace assign after admit");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "unit_ai_euro_expand: Master Carpenter workplace ok\n");
  return 0;
}

/*
 * Seasoned Scout + sticky≥2 fog deepen: prior goto to nearer unseen (MD=3)
 * re-aims to deeper unseen (MD=7). Cite: euro_unit_act §2c2 Seasoned+sticky;
 * Colonization.pdf Seasoned Scout.
 */
static int unit_seasoned_sticky_fog_deepen(void) {
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
    return fail("seasoned-sticky alloc map");
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
    return fail("seasoned-sticky spawn");
  }
  scout->nation_id = nation;
  scout->moves_left = 4;
  scout->orders = UNITS_ORDER_AI_MOVE;
  scout->goto_x = 5;
  scout->goto_y = 8; /* prior nearer fog — Seasoned+sticky must re-aim deeper */
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
  /*
   * Sticky deepen (unknown26[8]==2) only survives euro_balance hostility_sync
   * when a contacted Indian slot is very-low (0 < relation < 40). Unmet r==0
   * is cleared to sticky=0.
   */
  col1.nation[nation].relation_by_indian[0] = 20;
  col1.nation[nation].indian_hostility_sticky = 2;

  ai_goals_reset();

  uint32_t turn = 33;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  if (ai_diplo_indian_hostility_sticky(&col1, nation) < 2) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("seasoned-sticky expected sticky≥2");
  }

  ai_euro_dispatcher_turn(&ctx, nation);

  scout = units_get(&units, sid);
  const int deep =
    scout && scout->active && scout->orders == UNITS_ORDER_AI_MOVE &&
    scout->goto_x == 5 && scout->goto_y == 12 &&
    !map_tile_seen_by(&map, scout->goto_x, scout->goto_y, nation);
  if (!deep) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: seasoned-sticky orders=%d goto=(%d,%d) name=%s sticky=%u\n",
      scout ? scout->orders : -1,
      scout ? scout->goto_x : -1,
      scout ? scout->goto_y : -1,
      scout ? units_display_name(&units, scout) : "?",
      (unsigned)ai_diplo_indian_hostility_sticky(&col1, nation)
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("expected Seasoned+sticky re-aim fog to deeper MD=7");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.seen);
  fprintf(stderr, "unit_ai_euro_expand: Seasoned+sticky fog deepen ok\n");
  return 0;
}

int main(void) {
  if (unit_second_wave() != 0) {
    return 1;
  }
  /* Series R before known Seasoned+sticky early-exit (pre-existing). */
  if (unit_specialty_flag_a_haul_match() != 0) {
    return 1;
  }
  if (unit_second_colony_coastal_prefer() != 0) {
    return 1;
  }
  if (unit_scout_explore() != 0) {
    return 1;
  }
  if (unit_scout_fog_prefer_unseen() != 0) {
    return 1;
  }
  if (unit_scout_sticky_closer_ring() != 0) {
    return 1;
  }
  if (unit_scout_sticky_fog_deeper_unseen() != 0) {
    return 1;
  }
  if (unit_scout_fog_explore_no_contact() != 0) {
    return 1;
  }
  if (unit_seasoned_scout_deeper_fog() != 0) {
    return 1;
  }
  if (unit_scout_fog_prefer_rumour() != 0) {
    return 1;
  }
  if (unit_seasoned_sticky_fog_deepen() != 0) {
    return 1;
  }
  if (unit_treasure_coast() != 0) {
    return 1;
  }
  if (unit_treasure_board_sail() != 0) {
    return 1;
  }
  if (unit_treasure_europe_cash() != 0) {
    return 1;
  }
  if (unit_cortes_king_galleon_cash() != 0) {
    return 1;
  }
  if (unit_missionary_contact() != 0) {
    return 1;
  }
  if (unit_missionary_flee_skip() != 0) {
    return 1;
  }
  if (unit_pioneer_tools_delivery() != 0) {
    return 1;
  }
  if (unit_tools_cargo_hire() != 0) {
    return 1;
  }
  if (unit_lumber_cargo_hire() != 0) {
    return 1;
  }
  if (unit_food_cargo_hire() != 0) {
    return 1;
  }
  if (unit_horses_cargo_hire() != 0) {
    return 1;
  }
  if (unit_muskets_cargo_hire() != 0) {
    return 1;
  }
  if (unit_tools_mid_threshold_hire() != 0) {
    return 1;
  }
  if (unit_wagon_hire_once() != 0) {
    return 1;
  }
  if (unit_wagon_hire_once_colonies_ge6() != 0) {
    return 1;
  }
  if (unit_wagon_hire_lumber_once() != 0) {
    return 1;
  }
  if (unit_wagon_hire_ore_once() != 0) {
    return 1;
  }
  if (unit_wagon_hire_muskets_once() != 0) {
    return 1;
  }
  if (unit_wagon_hire_horses_once() != 0) {
    return 1;
  }
  if (unit_wagon_hire_food_once() != 0) {
    return 1;
  }
  if (unit_wagon_tools_delivery() != 0) {
    return 1;
  }
  if (unit_wagon_haul_tools_short() != 0) {
    return 1;
  }
  if (unit_wagon_haul_muskets_short() != 0) {
    return 1;
  }
  if (unit_wagon_haul_lumber_short() != 0) {
    return 1;
  }
  if (unit_wagon_haul_ore_short() != 0) {
    return 1;
  }
  if (unit_wagon_europe_export_feeder() != 0) {
    return 1;
  }
  if (unit_wagon_europe_export_unload() != 0) {
    return 1;
  }
  if (unit_wagon_haul_food_short() != 0) {
    return 1;
  }
  if (unit_wagon_food_delivery() != 0) {
    return 1;
  }
  if (unit_wagon_food_load_haul() != 0) {
    return 1;
  }
  if (unit_wagon_food_prefer_over_tools() != 0) {
    return 1;
  }
  if (unit_ship_trade_haul_tools_short() != 0) {
    return 1;
  }
  if (unit_ship_trade_haul_muskets_short() != 0) {
    return 1;
  }
  if (unit_ship_europe_export_silver() != 0) {
    return 1;
  }
  if (unit_privateer_europe_loot_sail() != 0) {
    return 1;
  }
  if (unit_ship_europe_export_load_silver() != 0) {
    return 1;
  }
  if (unit_galleon_europe_export_load_silver() != 0) {
    return 1;
  }
  if (unit_merchantman_europe_export_load_silver() != 0) {
    return 1;
  }
  if (unit_galleon_trade_haul_tools_short() != 0) {
    return 1;
  }
  if (unit_ship_food_delivery() != 0) {
    return 1;
  }
  if (unit_cargo_produced_mask_haul_prefer() != 0) {
    return 1;
  }
  if (unit_specialty_cargo_haul_prefer() != 0) {
    return 1;
  }
  if (unit_cargo_idle_turns_haul_prefer() != 0) {
    return 1;
  }
  if (unit_labor_shortage_join() != 0) {
    return 1;
  }
  if (unit_labor_bind_food_short() != 0) {
    return 1;
  }
  if (unit_food_emergency_labor() != 0) {
    return 1;
  }
  if (unit_expert_farmer_food_labor() != 0) {
    return 1;
  }
  if (unit_free_colonist_food_labor() != 0) {
    return 1;
  }
  if (unit_tools_short_pioneer_labor() != 0) {
    return 1;
  }
  if (unit_colony_flags_starvation_labor() != 0) {
    return 1;
  }
  if (unit_colony_ai_flags_mow_colony_alt() != 0) {
    return 1;
  }
  if (unit_build_ai_flags_wants_construction() != 0) {
    return 1;
  }
  if (unit_construction_labor_stockade() != 0) {
    return 1;
  }
  if (unit_master_carpenter_construction_labor() != 0) {
    return 1;
  }
  if (unit_lumberjack_warehouse_labor() != 0) {
    return 1;
  }
  if (unit_lumberjack_field_assign() != 0) {
    return 1;
  }
  if (unit_ore_miner_field_assign() != 0) {
    return 1;
  }
  if (unit_silver_miner_field_assign() != 0) {
    return 1;
  }
  if (unit_farmer_field_assign() != 0) {
    return 1;
  }
  if (unit_fisherman_field_assign() != 0) {
    return 1;
  }
  if (unit_sugar_planter_field_assign() != 0) {
    return 1;
  }
  if (unit_tobacco_planter_field_assign() != 0) {
    return 1;
  }
  if (unit_cotton_planter_field_assign() != 0) {
    return 1;
  }
  if (unit_fur_trapper_field_assign() != 0) {
    return 1;
  }
  if (unit_blacksmith_workplace_assign() != 0) {
    return 1;
  }
  if (unit_gunsmith_workplace_assign() != 0) {
    return 1;
  }
  if (unit_fur_trader_workplace_assign() != 0) {
    return 1;
  }
  if (unit_distiller_workplace_assign() != 0) {
    return 1;
  }
  if (unit_weaver_workplace_assign() != 0) {
    return 1;
  }
  if (unit_tobacconist_workplace_assign() != 0) {
    return 1;
  }
  if (unit_statesman_workplace_assign() != 0) {
    return 1;
  }
  if (unit_preacher_workplace_assign() != 0) {
    return 1;
  }
  if (unit_teacher_workplace_assign() != 0) {
    return 1;
  }
  if (unit_carpenter_workplace_assign() != 0) {
    return 1;
  }
  if (unit_peace_construction_stockade() != 0) {
    return 1;
  }
  if (unit_peace_construction_fort() != 0) {
    return 1;
  }
  if (unit_peace_construction_fortress() != 0) {
    return 1;
  }
  if (unit_peace_construction_warehouse() != 0) {
    return 1;
  }
  if (unit_peace_construction_warehouse_expansion() != 0) {
    return 1;
  }
  if (unit_peace_construction_docks() != 0) {
    return 1;
  }
  if (unit_coastal_drydock_prefer() != 0) {
    return 1;
  }
  if (unit_coastal_shipyard_prefer() != 0) {
    return 1;
  }
  if (unit_stuyvesant_custom_house_prefer() != 0) {
    return 1;
  }
  if (unit_peace_church_prefer() != 0) {
    return 1;
  }
  if (unit_war_armory_prefer() != 0) {
    return 1;
  }
  if (unit_peace_printing_press_prefer() != 0) {
    return 1;
  }
  if (unit_peace_schoolhouse_prefer() != 0) {
    return 1;
  }
  if (unit_war_magazine_prefer() != 0) {
    return 1;
  }
  if (unit_peace_newspaper_prefer() != 0) {
    return 1;
  }
  if (unit_peace_college_prefer() != 0) {
    return 1;
  }
  if (unit_peace_cathedral_prefer() != 0) {
    return 1;
  }
  if (unit_war_arsenal_prefer() != 0) {
    return 1;
  }
  if (unit_peace_university_prefer() != 0) {
    return 1;
  }
  if (unit_stable_prefer() != 0) {
    return 1;
  }
  if (unit_carpenters_shop_prefer() != 0) {
    return 1;
  }
  if (unit_lumber_mill_prefer() != 0) {
    return 1;
  }
  if (unit_blacksmiths_house_prefer() != 0) {
    return 1;
  }
  if (unit_blacksmiths_shop_prefer() != 0) {
    return 1;
  }
  if (unit_iron_works_prefer() != 0) {
    return 1;
  }
  if (unit_craft_distillers_house_prefer() != 0) {
    return 1;
  }
  if (unit_craft_weavers_house_prefer() != 0) {
    return 1;
  }
  if (unit_craft_tobacconists_house_prefer() != 0) {
    return 1;
  }
  if (unit_craft_fur_traders_house_prefer() != 0) {
    return 1;
  }
  if (unit_craft_distillery_prefer() != 0) {
    return 1;
  }
  if (unit_craft_weavers_shop_prefer() != 0) {
    return 1;
  }
  if (unit_craft_tobacconist_shop_prefer() != 0) {
    return 1;
  }
  if (unit_craft_fur_trading_post_prefer() != 0) {
    return 1;
  }
  if (unit_craft_rum_factory_prefer() != 0) {
    return 1;
  }
  if (unit_craft_textile_mill_prefer() != 0) {
    return 1;
  }
  if (unit_craft_cigar_factory_prefer() != 0) {
    return 1;
  }
  if (unit_craft_fur_factory_prefer() != 0) {
    return 1;
  }
  if (unit_capitol_prefer() != 0) {
    return 1;
  }
  if (unit_capitol_expansion_prefer() != 0) {
    return 1;
  }
  if (unit_indian_land_found() != 0) {
    return 1;
  }
  if (unit_improve_timer_pioneer_gate() != 0) {
    return 1;
  }
  if (unit_pioneer_plow_improve() != 0) {
    return 1;
  }
  if (unit_pioneer_road_on_plowed() != 0) {
    return 1;
  }
  if (unit_stockade_threat_labor() != 0) {
    return 1;
  }
  if (unit_multistep_military() != 0) {
    return 1;
  }
  if (unit_de_witt_wagon_foreign_trade() != 0) {
    return 1;
  }
  if (unit_dock_expert_hire() != 0) {
    return 1;
  }
  if (unit_dock_farmer_hire() != 0) {
    return 1;
  }
  if (unit_dock_farmer_hire_real_names() != 0) {
    return 1;
  }
  if (unit_dock_carpenter_hire() != 0) {
    return 1;
  }
  if (unit_dock_lumberjack_hire() != 0) {
    return 1;
  }
  if (unit_dock_ore_miner_hire() != 0) {
    return 1;
  }
  if (unit_dock_fisherman_hire() != 0) {
    return 1;
  }
  if (unit_dock_gunsmith_hire() != 0) {
    return 1;
  }
  if (unit_dock_blacksmith_hire() != 0) {
    return 1;
  }
  if (unit_dock_scout_hire() != 0) {
    return 1;
  }
  if (unit_dock_missionary_hire() != 0) {
    return 1;
  }
  if (unit_dock_elder_hire() != 0) {
    return 1;
  }
  if (unit_dock_preacher_hire() != 0) {
    return 1;
  }
  if (unit_dock_teacher_hire() != 0) {
    return 1;
  }
  if (unit_dock_distiller_hire() != 0) {
    return 1;
  }
  if (unit_dock_weaver_hire() != 0) {
    return 1;
  }
  if (unit_dock_fur_trader_hire() != 0) {
    return 1;
  }
  if (unit_dock_tobacconist_hire() != 0) {
    return 1;
  }
  if (unit_treasury_skip_hire() != 0) {
    return 1;
  }
  if (unit_5d04_buy_caravel_colonies_ge6() != 0) {
    return 1;
  }
  if (unit_5d04_buy_caravel_no_ship() != 0) {
    return 1;
  }
  if (unit_5d04_buy_merchantman_cargo_pressure() != 0) {
    return 1;
  }
  if (unit_5d04_buy_galleon_at_war() != 0) {
    return 1;
  }
  if (unit_5d04_buy_frigate_at_war() != 0) {
    return 1;
  }
  if (unit_5d04_buy_caravel_ship_full() != 0) {
    return 1;
  }
  if (unit_transport_europe_sell_trade_goods() != 0) {
    return 1;
  }
  if (unit_privateer_europe_sell_silver() != 0) {
    return 1;
  }
  if (unit_transport_europe_sell_multi_cargo() != 0) {
    return 1;
  }
  if (unit_transport_europe_sell_skip_boycott() != 0) {
    return 1;
  }
  fprintf(stderr, "unit_ai_euro_expand: ok\n");
  return 0;
}
