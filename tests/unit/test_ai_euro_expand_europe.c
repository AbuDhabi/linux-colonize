/* Slice of the former tests/unit/test_ai_euro_expand.c (split by feature 2026-09-23):
 * Europe export runs, privateer loot sail, treasure coast/board/cash-in. */
#include "test_ai_euro_expand_common.h"

/*
 * NAMES.TXT @CARGO start_lo column — the bid every nation's
 * trade.euro_price[16] (DS:0x84bc, the decomp's −0x7b44) carries on turn 1.
 * Fixtures that `memset(col1.nation, 0, ...)` leave that table all zeros,
 * which no DOS game ever sees; the FUN_521d_20e6 load matrix scores
 * price × stock, so an all-zero table makes every cargo tie at 0 and the
 * lowest cargo id wins by DOS's first-wins tie rule. Seed the real row in
 * fixtures whose expectation depends on the matrix picking a specific good.
 */
static void seed_dos_start_prices(ColonizeCol1Save* col1, int nation) {
  static const uint8_t k_start_lo[COLONIZE_CARGO_COUNT] = {
    1, 4, 3, 2, 4, 2, 3, 20, 2, 11, 11, 11, 11, 2, 2, 3
  };
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    col1->nation[nation].trade.euro_price[c] = k_start_lo[c];
  }
}

/*
 * RETARGETED 2026-09-23 (bugs.md #745). Was "idle Treasure → AI_MOVE toward
 * own COASTAL colony (Colonization.pdf)". DOS-LITERAL FUN_521d_20e6 raw
 * 90016-90017: a Treasure not standing in an own colony walks to
 * `local_62` — the prologue's NEAREST own colony by FUN_281f_037a distance,
 * with no coastline term — provided it is on the same landmass
 * (local_2c == local_38). This fixture puts a nearer INLAND colony (8,8)
 * behind the coastal one (4,4); DOS aims at (8,8), the deleted
 * ai_euro_treasure_coast_target aimed at (4,4).
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
    fx_map_free(&map);
    return fail("treasure colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Treasure");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 2);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  /* Nearer, NON-coastal own colony — the one DOS picks. */
  ColonizeColony* inland = fx_colony_add(&colonies, nation, 8, 8, 2);
  inland->stock[COLONIZE_CARGO_FOOD] = 40;
  inland->stock[COLONIZE_CARGO_LUMBER] = 25;
  if (map_tile_is_coastal(&map, 8, 8)) {
    fx_map_free(&map);
    return fail("inland colony (8,8) must not be coastal");
  }

  /* Inland Treasure — not on coast, not in a colony. */
  const int tid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* treasure = units_get(&units, tid);
  if (!treasure) {
    fx_map_free(&map);
    return fail("treasure spawn");
  }
  treasure->nation_id = nation;
  treasure->moves = 1 * UNITS_MP_PER_TILE;
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

  treasure = units_get(&units, tid);
  if (!treasure || !treasure->active) {
    fx_map_free(&map);
    return fail("treasure should remain active");
  }
  if (treasure->orders != UNITS_ORDER_AI_MOVE || treasure->goto_x != 8 ||
      treasure->goto_y != 8) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: treasure orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      treasure->orders,
      treasure->goto_x,
      treasure->goto_y,
      treasure->x,
      treasure->y
    );
    fx_map_free(&map);
    return fail("expected Treasure AI_MOVE toward NEAREST own colony (8,8)");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_expand: treasure nearest-colony walk ok (goto=(%d,%d))\n",
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
/*
 * AI Treasure cash-in is FUN_521d_20e6's unconditional in-colony band
 * (units_ai_treasure_cash_in_colony, raw ~2315-2331): any Treasure standing
 * in ANY own colony (not just coastal) cashes at full face value, no tax
 * and no FF gate. The King-galleon offer / Cortes "free transport" tax
 * mechanic (FUN_465b_0000 raw 75798, FUN_2a1f_0186 -> FUN_5fef_1908) is
 * gated on the mover's nation being human-controlled
 * (nation*0x34-0x543f == 0) and never runs for AI — confirmed 2026-09-17;
 * an earlier AI-side Cortes stand-in (removed) taxed AI treasuries for no
 * DOS reason. This test asserts the real band: full value, Cortes or not.
 */
static int unit_ai_treasure_colony_cash(void) {
  const int nation = 1;
  const int treasure_value = 1000;
  const int tax = 20;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("ai-treasure-cash alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("ai-treasure-cash colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Treasure");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 2);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  /* Closes FUN_5952_035e's emergency lumber buy (raw 94680: stock < 2), which
   * would otherwise take 200 gold off this nation on turn 0. */
  c->stock[COLONIZE_CARGO_LUMBER] = 25;

  const int tid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* treasure = units_get(&units, tid);
  if (!treasure) {
    fx_map_free(&map);
    return fail("ai-treasure-cash spawn treasure");
  }
  treasure->nation_id = nation;
  treasure->moves = 1; /* idle with moves left: eligible to act this turn */
  treasure->orders = 0;
  /* Both representations of the DOS value: the +0x315b byte (gold/100) and
   * the port's LE16 mirror — units_treasure_value_gold reads either. */
  treasure->profession = (uint8_t)(treasure_value / 100);
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
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[nation].tax_rate = (uint8_t)tax;
  /* Cortes owned — must make no difference to the AI band (no tax gate). */
  col1.head.founding_father[FF_HERNAN_CORTES] = 0;
  col1.nation[nation].founding_fathers[FF_HERNAN_CORTES / 8] |=
    (uint8_t)(1u << (FF_HERNAN_CORTES % 8));

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

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  treasure = units_get(&units, tid);
  const int treasure_gone = (!treasure || !treasure->active);
  const uint32_t gold_after = col1.nation[nation].gold;
  const unsigned delta =
    gold_after >= gold_before ? (unsigned)(gold_after - gold_before) : 0u;
  const int cash_ok = treasure_gone && delta == (unsigned)treasure_value;
  if (!cash_ok) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: ai treasure cash gone=%d gold before=%u after=%u delta=%u want +%d\n",
      treasure_gone,
      (unsigned)gold_before,
      (unsigned)gold_after,
      delta,
      treasure_value
    );
    fx_map_free(&map);
    return fail("expected AI in-colony treasure cash at full value, no tax");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_expand: AI in-colony treasure cash ok (delta=%u, full value, Cortes owned but irrelevant)\n",
    delta
  );
  return 0;
}

/*
 * RETARGETED 2026-09-06 (was "board + AI_SAIL Europe"): FUN_521d_20e6's
 * treasure act band cashes an AI Treasure standing in ANY own colony before
 * every other treasure arm (move_scoring_20e6_full.md raw ~2315). The
 * board+sail arm this fixture was written for was deleted outright on
 * 2026-09-23 (bugs.md #746) — DOS has no AI treasure-to-ship site at all.
 * DOS expectations: treasury += value, unit destroyed, Galleon left empty
 * and un-tasked.
 */
static int unit_treasure_board_sail(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("treasure-sail alloc map");
  }
  /* Water west of coastal colony (4,4) — ship sits at (3,4). */
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("treasure-sail colony should be coastal");
  }
  /* More eastern water for Europe-sail target. */
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Treasure");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Galleon");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 2);
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  /* Closes FUN_5952_035e's emergency lumber buy (raw 94680: stock < 2), which
   * would otherwise take 200 gold off this nation on turn 0. */
  c->stock[COLONIZE_CARGO_LUMBER] = 25;

  const int tid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* treasure = units_get(&units, tid);
  if (!treasure) {
    fx_map_free(&map);
    return fail("treasure-sail spawn treasure");
  }
  treasure->nation_id = nation;
  treasure->moves = 1 * UNITS_MP_PER_TILE;
  treasure->orders = 0;
  /* DOS unit+0x315b (COL1 record +0x17 = profession) = gold/100. */
  treasure->profession = 7; /* 700 gold */

  const int sid = units_spawn(&units, 1, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("treasure-sail spawn ship");
  }
  ship->nation_id = nation;
  ship->moves = 4 * UNITS_MP_PER_TILE;
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
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
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
  if (!ship || !ship->active) {
    fx_map_free(&map);
    return fail("treasure-sail ship missing after turn");
  }
  /* LAB_OVL14_L0000__0047b9: the cash-in destroys the Treasure on the spot. */
  if (treasure && treasure->active) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: treasure still alive aboard=%d pos=(%d,%d)\n",
      treasure->aboard_ship_id,
      treasure->x,
      treasure->y
    );
    fx_map_free(&map);
    return fail("expected in-colony Treasure cashed + destroyed (DOS 20e6)");
  }
  if (ship->cargo_count != 0) {
    fx_map_free(&map);
    return fail("Galleon must stay empty — DOS never ships an AI Treasure");
  }
  /* nation+0x2a += +0x315b * 100, no Crown cut. */
  const uint32_t gold_after = col1.nation[nation].gold;
  const unsigned gold_delta =
    gold_after >= gold_before ? (unsigned)(gold_after - gold_before) : 0u;
  if (gold_delta != 700u) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: treasure gold %u→%u delta=%u want 700\n",
      (unsigned)gold_before,
      (unsigned)gold_after,
      gold_delta
    );
    fx_map_free(&map);
    return fail("expected full-value in-colony Treasure cash-in");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_expand: treasure in-colony cash-in ok (gold_delta=%u)\n",
    gold_delta
  );
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
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("ship-export alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("ship-export colony should be coastal");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
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
    fx_map_free(&map);
    return fail("ship-export spawn");
  }
  ship->nation_id = nation;
  ship->moves = 4 * UNITS_MP_PER_TILE;
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
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

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
    fx_map_free(&map);
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
    fx_map_free(&map);
    return fail("expected Caravel AI_SAIL eastward with SILVER (Europe export)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: ship Europe export silver ok\n");
  return 0;
}

/*
 * Idle Privateer carrying capture loot (both holds full of SILVER) → AI_SAIL
 * Europe, where the FUN_521d_5d04 dock loop sells it. This is DOS's own route
 * for a laden hull: FUN_521d_20e6 raw 2166-2168 (`occupied == capacity` here)
 * jumps to LAB_003fa6 = FUN_48d3_015e, the High Seas hunt, whose port stand-in
 * is ai_euro_try_ship_europe_export — whose hull test is DOS's raw-1691 one
 * (any type 0x0d..0x12 with holds), not a cargo-ship name list.
 * Cite: FUN_521d_20e6 raw 1691 / 2166-2168; FUN_5fef_0352 loot transfer
 * (viceroy_overlays.c:85035-85050); FUN_521d_5d04 dock sell
 * (viceroy_overlays.c:83168-83192).
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
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
  units.types[0].movement = 8;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("priv-loot-sail spawn");
  }
  ship->nation_id = nation;
  ship->moves = 8 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  /* Both holds occupied: DOS's raw 2166-2168 gate is
   * `occupied > 1 || occupied == capacity`, not "carries anything". */
  ship->hold_goods_type[0] = COLONIZE_CARGO_SILVER;
  ship->hold_goods_amount[0] = 40;
  ship->hold_goods_type[1] = COLONIZE_CARGO_SILVER;
  ship->hold_goods_amount[1] = 40;

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
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

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
    fx_map_free(&map);
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
    fx_map_free(&map);
    return fail("expected Privateer AI_SAIL eastward with SILVER loot");
  }

  fx_map_free(&map);
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
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("ship-export-load alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("ship-export-load colony should be coastal");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_TOOLS] = 25;
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  c->stock[COLONIZE_CARGO_ORE] = 25;
  c->stock[COLONIZE_CARGO_MUSKETS] = 15;
  c->stock[COLONIZE_CARGO_HORSES] = 15;
  c->stock[COLONIZE_CARGO_FOOD] = 8;
  c->stock[COLONIZE_CARGO_SILVER] = 150;

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("ship-export-load spawn");
  }
  ship->nation_id = nation;
  ship->moves = 4 * UNITS_MP_PER_TILE;
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
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  /* DOS @CARGO start bids: the 20e6 load matrix scores price x stock. */
  seed_dos_start_prices(&col1, nation);

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
    fx_map_free(&map);
    return fail("ship-export-load should remain active");
  }
  /*
   * 2026-09-06e: the FUN_521d_20e6 load matrix now owns ship loading, and it
   * fills EVERY free hold (min(stock, 100) each) while a cargo still scores.
   * A 2-hold Caravel therefore takes 100 + 50 and leaves the colony at 0 —
   * the old "leave 50" expectation came from the port's own export ladder,
   * which borrowed FUN_364b_0636's colony-autosell eligibility rule; DOS has
   * no leave-50 rule on the ship-loading path. Destination is unchanged.
   */
  const int loaded = c->stock[COLONIZE_CARGO_SILVER] == 0 &&
                     ship->hold_goods_type[0] == COLONIZE_CARGO_SILVER &&
                     ship->hold_goods_amount[0] == 100 &&
                     ship->hold_goods_type[1] == COLONIZE_CARGO_SILVER &&
                     ship->hold_goods_amount[1] == 50;
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
    fx_map_free(&map);
    return fail("expected load SILVER leave 50 + AI_SAIL Europe");
  }

  fx_map_free(&map);
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
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("galleon-export-load alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("galleon-export-load colony should be coastal");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Galleon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_TOOLS] = 25;
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  c->stock[COLONIZE_CARGO_ORE] = 25;
  c->stock[COLONIZE_CARGO_MUSKETS] = 15;
  c->stock[COLONIZE_CARGO_HORSES] = 15;
  c->stock[COLONIZE_CARGO_FOOD] = 8;
  c->stock[COLONIZE_CARGO_SILVER] = 150;

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("galleon-export-load spawn");
  }
  ship->nation_id = nation;
  ship->moves = 4 * UNITS_MP_PER_TILE;
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
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  /* DOS @CARGO start bids: the 20e6 load matrix scores price x stock. */
  seed_dos_start_prices(&col1, nation);

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
    fx_map_free(&map);
    return fail("galleon-export-load should remain active");
  }
  /*
   * 2026-09-06e: the FUN_521d_20e6 load matrix now owns ship loading, and it
   * fills EVERY free hold (min(stock, 100) each) while a cargo still scores.
   * A 2-hold Caravel therefore takes 100 + 50 and leaves the colony at 0 —
   * the old "leave 50" expectation came from the port's own export ladder,
   * which borrowed FUN_364b_0636's colony-autosell eligibility rule; DOS has
   * no leave-50 rule on the ship-loading path. Destination is unchanged.
   */
  const int loaded = c->stock[COLONIZE_CARGO_SILVER] == 0 &&
                     ship->hold_goods_type[0] == COLONIZE_CARGO_SILVER &&
                     ship->hold_goods_amount[0] == 100 &&
                     ship->hold_goods_type[1] == COLONIZE_CARGO_SILVER &&
                     ship->hold_goods_amount[1] == 50;
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
    fx_map_free(&map);
    return fail("expected load SILVER leave 50 + AI_SAIL Europe");
  }

  fx_map_free(&map);
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
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("mm-export-load alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("mm-export-load colony should be coastal");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Merchantman");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_TOOLS] = 25;
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  c->stock[COLONIZE_CARGO_ORE] = 25;
  c->stock[COLONIZE_CARGO_MUSKETS] = 15;
  c->stock[COLONIZE_CARGO_HORSES] = 15;
  c->stock[COLONIZE_CARGO_FOOD] = 8;
  c->stock[COLONIZE_CARGO_SILVER] = 150;

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("mm-export-load spawn");
  }
  ship->nation_id = nation;
  ship->moves = 4 * UNITS_MP_PER_TILE;
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
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  /* DOS @CARGO start bids: the 20e6 load matrix scores price x stock. */
  seed_dos_start_prices(&col1, nation);

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
    fx_map_free(&map);
    return fail("mm-export-load should remain active");
  }
  /*
   * 2026-09-06e: the FUN_521d_20e6 load matrix now owns ship loading, and it
   * fills EVERY free hold (min(stock, 100) each) while a cargo still scores.
   * A 2-hold Caravel therefore takes 100 + 50 and leaves the colony at 0 —
   * the old "leave 50" expectation came from the port's own export ladder,
   * which borrowed FUN_364b_0636's colony-autosell eligibility rule; DOS has
   * no leave-50 rule on the ship-loading path. Destination is unchanged.
   */
  const int loaded = c->stock[COLONIZE_CARGO_SILVER] == 0 &&
                     ship->hold_goods_type[0] == COLONIZE_CARGO_SILVER &&
                     ship->hold_goods_amount[0] == 100 &&
                     ship->hold_goods_type[1] == COLONIZE_CARGO_SILVER &&
                     ship->hold_goods_amount[1] == 50;
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
    fx_map_free(&map);
    return fail("expected load SILVER leave 50 + AI_SAIL Europe");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: merchantman Europe export load silver ok\n");
  return 0;
}

static int unit_manowar_no_goods_load_818(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("mow-noload alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("mow-noload colony should be coastal");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 14] = 25;
    map.terrain[y * 16 + 15] = 25;
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Man-O-War");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 6;
  units.types[0].attack = 24;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_TOOLS] = 25;
  c->stock[COLONIZE_CARGO_LUMBER] = 25;
  c->stock[COLONIZE_CARGO_ORE] = 25;
  c->stock[COLONIZE_CARGO_MUSKETS] = 15;
  c->stock[COLONIZE_CARGO_HORSES] = 15;
  c->stock[COLONIZE_CARGO_FOOD] = 8;
  c->stock[COLONIZE_CARGO_SILVER] = 150;

  const int sid = units_spawn(&units, 0, 3, 4);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("mow-noload spawn");
  }
  ship->nation_id = nation;
  ship->moves = 4 * UNITS_MP_PER_TILE;
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
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  /* DOS @CARGO start bids: the 20e6 load matrix scores price x stock. */
  seed_dos_start_prices(&col1, nation);

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
    fx_map_free(&map);
    return fail("mow-noload should remain active");
  }
  /*
   * bugs.md #818: the DOS goods-load loop is `while (local_d2 != 0 && bVar20)`
   * (raw 90295) and bVar20 is seeded `type != 0x12` (raw 88556): a Man-O-War
   * berthed at its own colony never loads goods, whatever the silver stock.
   */
  if (c->stock[COLONIZE_CARGO_SILVER] != 150 || ship->hold_goods_amount[0] != 0 ||
      ship->hold_goods_amount[1] != 0) {
    fprintf(
      stderr, "unit_ai_euro_expand: 818 silver=%d hold0=%d/%d hold1=%d/%d\n",
      c->stock[COLONIZE_CARGO_SILVER], ship->hold_goods_type[0], ship->hold_goods_amount[0],
      ship->hold_goods_type[1], ship->hold_goods_amount[1]
    );
    fx_map_free(&map);
    return fail("818: Man-O-War must not load colony goods (bVar20 seed)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: 818 Man-O-War no goods load ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_treasure_coast", unit_treasure_coast},
    {"unit_treasure_board_sail", unit_treasure_board_sail},
    {"unit_ai_treasure_colony_cash", unit_ai_treasure_colony_cash},
    {"unit_ship_europe_export_silver", unit_ship_europe_export_silver},
    {"unit_privateer_europe_loot_sail", unit_privateer_europe_loot_sail},
    {"unit_ship_europe_export_load_silver", unit_ship_europe_export_load_silver},
    {"unit_galleon_europe_export_load_silver", unit_galleon_europe_export_load_silver},
    {"unit_merchantman_europe_export_load_silver", unit_merchantman_europe_export_load_silver},
    {"unit_manowar_no_goods_load_818", unit_manowar_no_goods_load_818},
};
TEST_MAIN(k_cases)
