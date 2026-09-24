#include "test_units_common.h"

static int g_refit_sound_id;
static int g_refit_sound_calls;

static void capture_refit_sound(int id) {
  g_refit_sound_id = id;
  g_refit_sound_calls++;
}

static int unit_refit_drydock(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "refit: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "refit: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int caravel = units_find_type(&pool, "Caravel");
  if (caravel < 0) {
    fprintf(stderr, "refit: no Caravel\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Drydock");
  colonies.building_type_count = 1;
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  col->x = 2;
  col->y = 2;
  col->has_building[0] = true;
  snprintf(col->name, sizeof(col->name), "Harbor");
  colonies.colony_count = 1;

  const int sid = units_spawn_allow_stack(&pool, caravel, 2, 2);
  ColonizeUnit* ship = units_get(&pool, sid);
  if (!ship) {
    fprintf(stderr, "refit: ship spawn failed\n");
    assets_msg_free(&names);
    return 1;
  }
  ship->nation_id = 0;
  ship->col1_flags15 = 0x80u;
  ship->col1_counter16 = 99; /* past construction thresh */
  pool.types[caravel].defense = 4;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "refit: GAME.TXT load failed\n");
    assets_msg_free(&names);
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  char st[96];
  st[0] = '\0';
  g_refit_sound_id = -1;
  g_refit_sound_calls = 0;
  units_set_combat_music_hooks(capture_refit_sound, NULL);
  const int repaired = units_tick_drydock_repair(
    &pool, &colonies, 0, 0, st, sizeof(st), &pops, &game_txt
  );
  units_set_combat_music_hooks(NULL, NULL);
  ship = units_get(&pool, sid);
  if (repaired != 1 || !ship || (ship->col1_flags15 & 0x80u) != 0) {
    fprintf(stderr, "refit: repair failed repaired=%d bit7=%02x\n", repaired,
            ship ? (unsigned)ship->col1_flags15 : 0xffu);
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }
  if (g_refit_sound_calls != 1 || g_refit_sound_id != 0x54) {
    fprintf(
      stderr, "refit: sound calls=%d id=0x%x (want one 0x54)\n",
      g_refit_sound_calls, g_refit_sound_id
    );
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "repair") == NULL &&
       strstr(pops.queue[0].body, "Harbor") == NULL &&
       strstr(pops.queue[0].body, "Caravel") == NULL)) {
    fprintf(
      stderr,
      "refit: REFIT popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }

  assets_msg_free(&game_txt);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: REFIT Drydock popup ok\n");
  return 0;
}

/* Helper: full warehouse unload → @WAREHOUSEFULL. */
static int unit_warehouse_full(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "whfull: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "whfull: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int caravel = units_find_type(&pool, "Caravel");
  if (caravel < 0) {
    fprintf(stderr, "whfull: no Caravel\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  col->x = 1;
  col->y = 1;
  col->warehouse_level = 0; /* cap 100 */
  col->stock[COLONIZE_CARGO_LUMBER] = 100;
  snprintf(col->name, sizeof(col->name), "Packed");
  colonies.colony_count = 1;

  const int sid = units_spawn_allow_stack(&pool, caravel, 1, 1);
  ColonizeUnit* ship = units_get(&pool, sid);
  if (!ship) {
    fprintf(stderr, "whfull: ship spawn failed\n");
    assets_msg_free(&names);
    return 1;
  }
  ship->nation_id = 0;
  if (units_load_goods(&pool, sid, COLONIZE_CARGO_LUMBER, 20) <= 0) {
    fprintf(stderr, "whfull: load lumber failed\n");
    assets_msg_free(&names);
    return 1;
  }

  bool full = false;
  const int moved = colonies_transfer_from_unit(&colonies, 1, &pool, sid, 0, &full);
  /* bugs.md: full warehouse no longer blocks — the hold lands anyway and
   * the flag informs (excess spoils next turn). */
  if (moved != 20 || !full) {
    fprintf(stderr, "whfull: want moved=20 full=1 got moved=%d full=%d\n", moved, (int)full);
    assets_msg_free(&names);
    return 1;
  }

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "whfull: GAME.TXT load failed\n");
    assets_msg_free(&names);
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_warehouse_full_chrome(
    &colonies, col, COLONIZE_CARGO_LUMBER, "Lumber", 100, 0, &pops, &game_txt
  );
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "warehouse") == NULL &&
       strstr(pops.queue[0].body, "Warehouse") == NULL &&
       strstr(pops.queue[0].body, "Packed") == NULL)) {
    fprintf(
      stderr,
      "whfull: WAREHOUSEFULL popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }
  if (strstr(pops.queue[0].body, "Lumber") == NULL &&
      strstr(pops.queue[0].body, "lumber") == NULL &&
      strstr(pops.queue[0].body, "100") == NULL) {
    fprintf(stderr, "whfull: popup missing cargo/cap '%s'\n", pops.queue[0].body);
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }

  assets_msg_free(&game_txt);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: WAREHOUSEFULL popup ok\n");
  return 0;
}

/*
 * bugs.md: "I can't move a ship onto a sea lane either way." DOS
 * FUN_4720_015c (~76048) only raises reason 5 when the ship is ALREADY on a
 * high-seas tile and steps further east without a Go To / Trade Route order;
 * entering the lane from ordinary ocean is always legal, and a Go To may
 * target a lane tile (which then sails the ship to Europe, game_loop.c).
 */
static int unit_sea_lane_entry(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Caravel");
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "sea_lane: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  /* Lanes sit INSIDE the playable board, as on AMER2 (west lane = column 1,
   * east lane = columns 51..56; the outer rim, column 0 / w-1, is plain ocean
   * a unit may never occupy — DOS FUN_137f_000a, bugs.md #429). */
  for (int y = 0; y < 8; ++y) {
    map.terrain[y * 8 + 5] = 26; /* high seas / sea lane */
    map.terrain[y * 8 + 6] = 26;
  }

  const int id = units_spawn_allow_stack(&pool, 0, 4, 3);
  ColonizeUnit* u = units_get(&pool, id);
  if (!u) {
    map_free(&map);
    fprintf(stderr, "sea_lane: spawn failed\n");
    return 1;
  }
  u->nation_id = 0;
  u->moves = 4 * UNITS_MP_PER_TILE;

  int rc = 0;
  if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, u->type_index, 5, 3, id)) {
    fprintf(stderr, "sea_lane: ocean->lane must be allowed\n");
    rc = 1;
  }
  if (rc == 0 && !units_set_goto_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, id, 5, 3)) {
    fprintf(stderr, "sea_lane: Go To onto a lane tile must be accepted\n");
    rc = 1;
  }
  units_clear_orders(&pool, id);
  u = units_get(&pool, id);
  u->x = 5;
  u->y = 3;
  u->orders = UNITS_ORDER_NONE;
  if (rc == 0 && units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, u->type_index, 6, 3, id)) {
    fprintf(stderr, "sea_lane: lane->east without a sail order must be denied\n");
    rc = 1;
  }
  if (rc == 0 && units_last_enter_reason() != COLONIZE_ENTER_BLOCKED_HS_SAIL) {
    fprintf(stderr, "sea_lane: lane->east deny should be reason 5\n");
    rc = 1;
  }
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = 6;
  u->goto_y = 3;
  if (rc == 0 && !units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, u->type_index, 6, 3, id)) {
    fprintf(stderr, "sea_lane: lane->east with Go To must be allowed\n");
    rc = 1;
  }
  u->orders = UNITS_ORDER_NONE;
  if (rc == 0 && !units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, u->type_index, 4, 3, id)) {
    fprintf(stderr, "sea_lane: lane->west back to ocean must be allowed\n");
    rc = 1;
  }
  /*
   * bugs.md #429: the outer rim is not a playable tile. A ship on the lane may
   * not step onto column w-1 (nor column 0 / row 0 / row h-1) — DOS
   * FUN_137f_000a. Before the fix the port only tested the raw array bounds,
   * so a ship could slide off the west sea lane onto column 0, a tile the
   * viewport never scrolls to and no click can address.
   */
  u->x = 1;
  u->y = 3;
  if (rc == 0 && units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, u->type_index, 0, 3, id)) {
    fprintf(stderr, "sea_lane: step onto the west rim column must be denied\n");
    rc = 1;
  }
  /* bugs.md #717: the x-axis rim is DOS reason 4 for a SHIP (@SAILHOME with a
   * "No" that aborts), not the silent y-axis edge. Expectation updated from
   * COLONIZE_ENTER_BLOCKED_EDGE — the step is still denied either way. */
  if (rc == 0 && units_last_enter_reason() != COLONIZE_ENTER_EDGE_SAIL) {
    fprintf(stderr, "sea_lane: west rim deny should be reason EDGE_SAIL (#717)\n");
    rc = 1;
  }
  u->x = 3;
  u->y = 1;
  if (rc == 0 && units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, u->type_index, 3, 0, id)) {
    fprintf(stderr, "sea_lane: step onto the north rim row must be denied\n");
    rc = 1;
  }
  /* bugs.md #717: the y-axis rim returns 0 with no reason word in DOS —
   * silent BLOCKED_EDGE, never the @SAILHOME question. */
  if (rc == 0 && units_last_enter_reason() != COLONIZE_ENTER_BLOCKED_EDGE) {
    fprintf(stderr, "sea_lane: north rim deny should stay silent EDGE (#717)\n");
    rc = 1;
  }
  u->x = 3;
  u->y = 6;
  if (rc == 0 && units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, u->type_index, 3, 7, id)) {
    fprintf(stderr, "sea_lane: step onto the south rim row must be denied\n");
    rc = 1;
  }
  u->x = 4;
  u->y = 3;
  u->orders = UNITS_ORDER_NONE;

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: sea-lane entry rules ok\n");
  }
  return rc;
}

/*
 * @LANDFIRST / no amphibious assault. DOS FUN_4720_015c reason 9 (raw
 * 74720-74728, tag DS:0x1429 via the FUN_4720_049e jump table at 4720:060a):
 * a land unit whose own tile is ocean/high-seas — i.e. one riding a ship —
 * may not enter a square occupied by another nation. GAME.TXT: "Land units
 * cannot enter an enemy occupied square from on board a ship. You must first
 * unload them into an empty or friendly-occupied square."
 */
static int unit_amphibious_landfirst(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Caravel");
  pool.types[0].movement = 4;
  pool.types[0].cargo = 2;
  pool.types[0].space = 99;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Soldiers");
  pool.types[1].movement = 1;
  pool.types[1].attack = 2;
  pool.types[1].defense = 2;
  pool.types[1].space = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "amphib: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 25; /* ocean */
    map.layer3[i] = 1;   /* region 1 = the main sea, not a lake */
  }
  map.terrain[3 * 8 + 4] = 1; /* one land tile east of the ship */
  units_set_occupancy_map(NULL);

  const ColonizeWorld w = {
    .units = &pool, .colonies = NULL, .map = &map
  };

  const int ship = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int pax = units_spawn_allow_stack(&pool, 1, 3, 3);
  const int foe = units_spawn_allow_stack(&pool, 1, 4, 3);
  ColonizeUnit* su = units_get(&pool, ship);
  ColonizeUnit* pu = units_get(&pool, pax);
  ColonizeUnit* fu = units_get(&pool, foe);
  int rc = 0;
  if (!su || !pu || !fu) {
    fprintf(stderr, "amphib: spawn failed\n");
    map_free(&map);
    return 1;
  }
  su->nation_id = 0;
  pu->nation_id = 0;
  fu->nation_id = 1;
  if (!units_board_stacked(&pool, pax, ship)) {
    fprintf(stderr, "amphib: passenger failed to board\n");
    map_free(&map);
    return 1;
  }
  pu = units_get(&pool, pax);
  pu->moves = UNITS_MP_PER_TILE;

  units_enter_probe_w(&w, pu->type_index, 4, 3, pax);
  if (units_last_enter_reason() != COLONIZE_ENTER_LANDFIRST) {
    fprintf(
      stderr, "amphib: shipboard attack must be @LANDFIRST, got %d\n",
      (int)units_last_enter_reason()
    );
    rc = 1;
  }
  /* Same tile, own nation: friendly-occupied is allowed (landfall). */
  fu = units_get(&pool, foe);
  fu->nation_id = 0;
  if (rc == 0) {
    units_enter_probe_w(&w, pu->type_index, 4, 3, pax);
    if (units_last_enter_reason() == COLONIZE_ENTER_LANDFIRST) {
      fprintf(stderr, "amphib: friendly-occupied square must not be blocked\n");
      rc = 1;
    }
  }
  /* Ashore, the same attack is ordinary land combat. */
  if (rc == 0) {
    fu->nation_id = 1;
    pu = units_get(&pool, pax);
    units_unload_passenger_w(&w, ship, pax, 4, 3);
    pu = units_get(&pool, pax);
    pu->x = 4;
    pu->y = 4;
    map.terrain[4 * 8 + 4] = 1;
    pu->aboard_ship_id = -1;
    pu->moves = UNITS_MP_PER_TILE;
    units_enter_probe_w(&w, pu->type_index, 4, 3, pax);
    const ColonizeEnterReason r = units_last_enter_reason();
    if (r == COLONIZE_ENTER_LANDFIRST) {
      fprintf(stderr, "amphib: a unit standing on land must not see @LANDFIRST\n");
      rc = 1;
    }
  }
  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: amphibious @LANDFIRST gate ok\n");
  }
  return rc;
}

/*
 * bugs.md #553: "Sailing a ship into a captured colony is impossible." The
 * live save had a native Brave standing fortified inside the colony (it had
 * walked in before ai_native_brave_step ported the FUN_465b_0000
 * foreign-destination arm), and the probe's foreign-unit scan bounced the
 * owner's ship off it as BLOCKED_DOMAIN. DOS reads the settlement owner
 * (FUN_281f_06be) for the tile: an own colony always docks.
 */
static int unit_own_colony_dock_ignores_squatter(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Merchantman");
  pool.types[0].movement = 5;
  pool.types[0].cargo = 4;
  pool.types[0].space = 99;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Brave");
  pool.types[1].movement = 1;
  pool.types[1].attack = 1;
  pool.types[1].defense = 1;
  pool.types[1].space = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "own-colony dock: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 25;
    map.layer3[i] = 1;
  }
  map.terrain[3 * 8 + 4] = 1; /* colony tile east of the ship */
  units_set_occupancy_map(NULL);

  static ColonizeColonyPool colonies;
  memset(&colonies, 0, sizeof(colonies));
  colonies.colonies[0].active = true;
  colonies.colonies[0].id = 7;
  colonies.colonies[0].x = 4;
  colonies.colonies[0].y = 3;
  colonies.colonies[0].nation_id = 2; /* captured: now ours */

  const ColonizeWorld w = {.units = &pool, .colonies = &colonies, .map = &map};
  const int ship = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int brave = units_spawn_allow_stack(&pool, 1, 4, 3);
  ColonizeUnit* su = units_get(&pool, ship);
  ColonizeUnit* bu = units_get(&pool, brave);
  int rc = 0;
  if (!su || !bu) {
    fprintf(stderr, "own-colony dock: spawn failed\n");
    map_free(&map);
    return 1;
  }
  su->nation_id = 2;
  bu->nation_id = 6;
  bu->orders = UNITS_ORDER_FORTIFIED;

  if (units_enter_probe_w(&w, su->type_index, 4, 3, ship) != COLONIZE_ENTER_DOCK) {
    fprintf(
      stderr, "own-colony dock: ship must dock past a squatting Brave, got %d\n",
      (int)units_last_enter_reason()
    );
    rc = 1;
  }
  /* A foreign colony still refuses the ship (FUN_5f7a_0662 trade path). */
  colonies.colonies[0].nation_id = 1;
  if (rc == 0 && units_enter_probe_w(&w, su->type_index, 4, 3, ship) == COLONIZE_ENTER_DOCK) {
    fprintf(stderr, "own-colony dock: a foreign colony must not dock\n");
    rc = 1;
  }
  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: own-colony dock past squatter ok\n");
  }
  return rc;
}

/*
 * bugs.md #421: "Newly bought Merchantman sent to the New World spawned in an
 * unexplored sea lane tile, and did not even insta-reveal the fog."
 *
 * DOS FUN_48d3_048e (viceroy_unpacked.c:77810) is the Europe->map placement:
 * an expanding ring hunt around the nation's landfall goal accepting the first
 * tile that passes FUN_48d3_0434 (terrain 0x1a AND empty-or-own-nation), then
 * unconditionally
 *   FUN_281f_0948 (set x/y) -> FUN_281f_084e -> FUN_281f_07a0
 * where FUN_281f_07a0 == FUN_13f1_02f8, the same sight reveal a normal move
 * runs. This test pins both halves of the sequence game_loop.c's
 * game_europe_deliver_bound_ships now performs.
 */
static int unit_europe_arrival_reveals(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Merchantman");
  pool.types[0].movement = 5;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 12, 12, err, sizeof(err))) {
    fprintf(stderr, "arrival: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 12 * 12; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  for (int y = 0; y < 12; ++y) {
    map.terrain[y * 12 + 10] = 26; /* eastern high-seas lane */
  }
  memset(map.seen, 0, map.tile_count); /* whole map unexplored */

  int rc = 0;
  const int nation = 2;
  const int goal_x = 10;
  const int goal_y = 5;

  /* An own ship already parked on the landfall goal must not push the arrival
   * across the map: DOS 48d3_0434 accepts an own-nation occupant, and the ring
   * walk keeps the pick adjacent. (units_find_high_seas_tile, the old pick,
   * refuses ANY occupied tile.) */
  const int blocker = units_spawn_allow_stack(&pool, 0, goal_x, goal_y);
  if (blocker < 0) {
    fprintf(stderr, "arrival: blocker spawn failed\n");
    map_free(&map);
    return 1;
  }
  units_get(&pool, blocker)->nation_id = nation;

  int fx = -1;
  int fy = -1;
  if (!units_spiral_place_hs_near(&pool, &map, goal_x, goal_y, nation, &fx, &fy)) {
    fprintf(stderr, "arrival: spiral place found no high-seas tile\n");
    map_free(&map);
    return 1;
  }
  if (!map_tile_is_high_seas(&map, fx, fy)) {
    fprintf(stderr, "arrival: placed on non-high-seas tile (%d,%d)\n", fx, fy);
    rc = 1;
  }
  if (rc == 0 && (fx != goal_x || abs(fy - goal_y) > 1)) {
    fprintf(stderr, "arrival: ring pick (%d,%d) strayed from goal (%d,%d)\n", fx, fy, goal_x, goal_y);
    rc = 1;
  }

  if (rc == 0) {
    const int ship = units_spawn_allow_stack(&pool, 0, fx, fy);
    if (ship < 0) {
      fprintf(stderr, "arrival: ship spawn failed\n");
      map_free(&map);
      return 1;
    }
    ColonizeUnit* u = units_get(&pool, ship);
    u->nation_id = nation;

    /* Pre-condition: the arrival tile is still fogged for the arriving nation
     * — this is exactly the state the player was left in. */
    if (map_tile_seen_by(&map, fx, fy, nation)) {
      fprintf(stderr, "arrival: fixture tile was already explored\n");
      rc = 1;
    }
    if (rc == 0) {
      (void)units_reveal_sight_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL)}, u);
    }
    if (rc == 0 && !map_tile_seen_by(&map, fx, fy, nation)) {
      fprintf(stderr, "arrival: own tile still fogged after reveal\n");
      rc = 1;
    }
    /* Sight radius 1 inner box: every inset neighbour is revealed. */
    for (int dy = -1; rc == 0 && dy <= 1; ++dy) {
      for (int dx = -1; rc == 0 && dx <= 1; ++dx) {
        const int tx = fx + dx;
        const int ty = fy + dy;
        if (!map_coords_inset(&map, tx, ty)) {
          continue;
        }
        if (!map_tile_seen_by(&map, tx, ty, nation)) {
          fprintf(stderr, "arrival: neighbour (%d,%d) still fogged after reveal\n", tx, ty);
          rc = 1;
        }
      }
    }
    /* Other nations gained nothing. */
    if (rc == 0 && map_tile_seen_by(&map, fx, fy, 0)) {
      fprintf(stderr, "arrival: reveal leaked to another nation\n");
      rc = 1;
    }
  }

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: Europe arrival place+reveal ok\n");
  }
  return rc;
}


/* FUN_465b_0000 → FUN_5fef_1908 King's Galleon offer (@KINGGALLEON2/3, @CASHTREASURE). */
static int unit_king_galleon_offer(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "galleon: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "galleon: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int treasure_ti = units_find_type(&pool, "Treasure");
  const int galleon_ti = units_find_type(&pool, "Galleon");
  if (treasure_ti < 0 || galleon_ti < 0) {
    fprintf(stderr, "galleon: types missing\n");
    assets_msg_free(&names);
    return 1;
  }
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map)); /* map_alloc frees the old buffers first */
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "galleon: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  for (int y = 0; y < 8; ++y) {
    map.terrain[y * map.width + 0] = 25; /* ocean column */
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  const int cx = 1;
  const int cy = 3;
  if (!map_tile_is_coastal(&map, cx, cy)) {
    fprintf(stderr, "galleon: (1,3) should be coastal\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  const int cid = colonies_found(&colonies, &map, cx, cy, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
  if (cid < 0) {
    fprintf(stderr, "galleon: colonies_found failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  /* DOS reads the colony record's COASTAL bit (+0x1c & 0x40), not the map;
   * the 8x8 fixture has no open-sea region so stamp it as a real save would. */
  colonies_get_mut(&colonies, cid)->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
  ColonizeCol1Save c1;
  memset(&c1, 0, sizeof(c1));
  c1.player[0].control = 0;
  memset(c1.head.founding_father, -1, sizeof(c1.head.founding_father)); /* unclaimed */
  c1.nation[0].gold = 100;
  c1.nation[0].tax_rate = 20;
  c1.head.difficulty = 2; /* Conquistador: (2+10)*5 = 60 > 2*20 */
  AiPopupState pops;
  ai_popup_clear(&pops);

  /* No Cortes: share = max(60, 40) = 60. */
  if (units_king_galleon_share_pct(&c1, 0) != 60) {
    fprintf(stderr, "galleon: share want 60 got %d\n", units_king_galleon_share_pct(&c1, 0));
    goto fail;
  }
  c1.nation[0].tax_rate = 45; /* 2*45 = 90 wins; cap holds at 90 */
  if (units_king_galleon_share_pct(&c1, 0) != 90) {
    fprintf(stderr, "galleon: share cap want 90\n");
    goto fail;
  }
  c1.nation[0].tax_rate = 20;
  c1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] |= (uint8_t)(1u << (FF_HERNAN_CORTES % 8));
  if (units_king_galleon_share_pct(&c1, 0) != 20) {
    fprintf(stderr, "galleon: Cortes share want tax 20\n");
    goto fail;
  }
  c1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] = 0;

  const int tid = units_spawn_allow_stack(&pool, treasure_ti, cx, cy);
  ColonizeUnit* t = units_get(&pool, tid);
  t->nation_id = 0;
  t->profession = 10; /* DOS +0x315b = gold/100 → 1000 */
  if (units_treasure_value_gold(t) != 1000) {
    fprintf(stderr, "galleon: value want 1000 got %d\n", units_treasure_value_gold(t));
    goto fail;
  }

  /* Owning a Galleon without Cortes → no offer. */
  const int gid = units_spawn_allow_stack(&pool, galleon_ti, 0, 3);
  units_get(&pool, gid)->nation_id = 0;
  if (units_king_galleon_offer_for_unit_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .europe=(EuropeScreen*)(NULL)}, 0, tid, &pops, NULL) != 0 ||
      pops.queue_count != 0) {
    fprintf(stderr, "galleon: own Galleon should suppress the offer\n");
    goto fail;
  }
  units_despawn(&pool, gid);

  /* Offer enqueued; Refuse leaves the Treasure. */
  if (units_king_galleon_offer_for_unit_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .europe=(EuropeScreen*)(NULL)}, 0, tid, &pops, NULL) != 1 ||
      pops.queue_count != 1 || pops.queue[0].tag != AI_POPUP_TAG_KING_GALLEON ||
      pops.queue[0].payload != tid) {
    fprintf(stderr, "galleon: KINGGALLEON2 CHOICE not enqueued\n");
    goto fail;
  }
  /* Re-running while queued must not stack a duplicate. */
  (void)units_king_galleon_offer_for_unit_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .europe=(EuropeScreen*)(NULL)}, 0, tid, &pops, NULL);
  if (pops.queue_count != 1) {
    fprintf(stderr, "galleon: duplicate offer queued\n");
    goto fail;
  }
  pops.has_result = true;
  pops.result_tag = AI_POPUP_TAG_KING_GALLEON;
  pops.result_nation_a = 0;
  pops.result_payload = tid;
  pops.result_choice_id = 0;
  pops.result_cancelled = false;
  if (!units_king_galleon_apply_popup_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .europe=(EuropeScreen*)(NULL)}, &pops, NULL)) {
    fprintf(stderr, "galleon: apply should consume tag\n");
    goto fail;
  }
  if (c1.nation[0].gold != 100 || !units_get(&pool, tid) || !units_get(&pool, tid)->active) {
    fprintf(stderr, "galleon: Refuse must leave gold/treasure untouched\n");
    goto fail;
  }
  /* Accept: 60% share → 600 to royal_money, 400 to gold, Treasure gone. */
  pops.result_choice_id = 1;
  (void)units_king_galleon_apply_popup_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .europe=(EuropeScreen*)(NULL)}, &pops, NULL);
  if (c1.nation[0].gold != 500 || c1.nation[0].royal_money != 600) {
    fprintf(stderr, "galleon: Accept want gold 500 royal 600 got %u/%d\n", c1.nation[0].gold,
            c1.nation[0].royal_money);
    goto fail;
  }
  {
    const ColonizeUnit* gone = units_get_const(&pool, tid);
    if (gone && gone->active) {
      fprintf(stderr, "galleon: Treasure should be despawned\n");
      goto fail;
    }
  }
  /* WoI declared: full value, no CHOICE. */
  c1.head.game_options.woi = 1;
  const int tid2 = units_spawn_allow_stack(&pool, treasure_ti, cx, cy);
  units_get(&pool, tid2)->nation_id = 0;
  units_get(&pool, tid2)->profession = 2; /* 200 gold */
  ai_popup_clear(&pops);
  if (units_king_galleon_offer_for_unit_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .europe=(EuropeScreen*)(NULL)}, 0, tid2, &pops, NULL) != 1 ||
      c1.nation[0].gold != 700 || c1.nation[0].royal_money != 600) {
    fprintf(stderr, "galleon: WoI should cash full value at once (gold %u)\n", c1.nation[0].gold);
    goto fail;
  }
  fprintf(stderr, "unit_units: King's Galleon offer ok\n");
  map_free(&map);
  assets_msg_free(&names);
  return 0;
fail:
  map_free(&map);
  assets_msg_free(&names);
  return 1;
}
int main(void) {
  diag_init(0, NULL);
  if (unit_king_galleon_offer() != 0) {
    return 1;
  }
  if (unit_refit_drydock() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_warehouse_full() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_sea_lane_entry() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_amphibious_landfirst() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_own_colony_dock_ignores_squatter() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_europe_arrival_reveals() != 0) {
    diag_shutdown();
    return 1;
  }
  diag_shutdown();
  return 0;
}
