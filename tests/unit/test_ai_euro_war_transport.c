/* Slice of the former tests/unit/test_ai_euro_war.c (split by feature 2026-09-23):
 * threatened-colony unload and war transport (MoW / frigate). */
#include "test_ai_euro_war_common.h"

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

static const TestCase k_cases[] = {
    {"unit_unload_stance0_no_sticky", unit_unload_stance0_no_sticky},
    {"unit_unload_regular_threatened", unit_unload_regular_threatened},
    {"unit_unload_continental_army_threatened", unit_unload_continental_army_threatened},
    {"unit_unload_continental_cavalry_threatened", unit_unload_continental_cavalry_threatened},
    {"unit_war_transport_threatened_colony", unit_war_transport_threatened_colony},
    {"unit_mow_war_transport_threatened", unit_mow_war_transport_threatened},
    {"unit_frigate_war_transport_threatened", unit_frigate_war_transport_threatened},
};
TEST_MAIN(k_cases)
