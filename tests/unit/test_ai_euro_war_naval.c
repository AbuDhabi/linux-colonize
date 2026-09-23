/* Slice of the former tests/unit/test_ai_euro_war.c (split by feature 2026-09-23):
 * naval hunt, fort fire flee, privateer hunts and sighting bit, multistep sail, ambush. */
#include "test_ai_euro_war_common.h"

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
 * bugs.md #747 — DOS-LITERAL FUN_465b_0000 raw 75527-75545, the
 * Privateer-sighting relation bit. Mover type 0x10 is @UNIT row 16 =
 * Privateer (the old port read it as "Treasure", 0x0a, and scanned the 8
 * neighbours after a Treasure's act instead). The bit is
 * `nation[occupant_owner].euro_relation[mover_owner] |= 0x80`.
 */
static int unit_privateer_sighting_bit(void) {
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].movement = 8;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].movement = 4;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Treasure");
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].movement = 1;

  const int priv = units_spawn(&units, 0, 5, 5);
  const int car = units_spawn(&units, 1, 6, 5);
  const int trea = units_spawn(&units, 2, 7, 5);
  ColonizeUnit* pu = units_get(&units, priv);
  ColonizeUnit* cu = units_get(&units, car);
  ColonizeUnit* tu = units_get(&units, trea);
  if (!pu || !cu || !tu) {
    return fail("privateer-sighting spawn");
  }
  pu->nation_id = 1;
  cu->nation_id = 2;
  tu->nation_id = 1;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  col1.head.difficulty = 0; /* rng(0,100) < 1 — the follow-up almost never */

  /* Privateer of nation 1 enters nation 2's Caravel tile. */
  ai_euro_465b_privateer_sighting(&col1, &units, NULL, priv, car);
  if (!(ai_diplo_read(&col1, 2, 1) & AI_DIPLO_PRIVATEER_SIGHTED)) {
    return fail("expected nation[2].relation[1] |= 0x80 for a Privateer mover");
  }
  /* The old (wrong) direction must stay clear. */
  if (ai_diplo_read(&col1, 1, 2) & AI_DIPLO_PRIVATEER_SIGHTED) {
    return fail("sighting bit written on the wrong side");
  }

  /* A Treasure mover must NOT set it (the refuted reading). */
  ai_diplo_write(&col1, 2, 1, 0);
  ai_euro_465b_privateer_sighting(&col1, &units, NULL, trea, car);
  if (ai_diplo_read(&col1, 2, 1) & AI_DIPLO_PRIVATEER_SIGHTED) {
    return fail("Treasure mover must not set the Privateer sighting bit");
  }

  /* Occupant that is itself a Privateer: DOS's `occupant type != 0x10` skip. */
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  cu->nation_id = 2;
  const int priv2 = units_spawn(&units, 0, 8, 5);
  ColonizeUnit* p2 = units_get(&units, priv2);
  if (!p2) {
    return fail("privateer-sighting spawn 2");
  }
  p2->nation_id = 2;
  ai_euro_465b_privateer_sighting(&col1, &units, NULL, priv, priv2);
  if (ai_diplo_read(&col1, 2, 1) & AI_DIPLO_PRIVATEER_SIGHTED) {
    return fail("Privateer-on-Privateer must not set the sighting bit");
  }

  fprintf(stderr, "unit_ai_euro_war: 465b privateer sighting bit ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_naval_war_hunt", unit_naval_war_hunt},
    {"unit_naval_flee_fort_fire", unit_naval_flee_fort_fire},
    {"unit_privateer_sighting_bit", unit_privateer_sighting_bit},
    {"unit_privateer_war_hunt", unit_privateer_war_hunt},
    {"unit_privateer_station_keep_hunt", unit_privateer_station_keep_hunt},
    {"unit_naval_multistep_sail", unit_naval_multistep_sail},
    {"unit_naval_ambush", unit_naval_ambush},
};
TEST_MAIN(k_cases)
