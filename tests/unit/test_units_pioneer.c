#include "test_units_common.h"


/* Helper: clear-forest grants lumber + @CLEARCUT (avoids main stack pressure). */
static int unit_clearcut_lumber(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "clearcut: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "clearcut: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  if (pioneer < 0) {
    fprintf(stderr, "clearcut: no Pioneers type\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map)); /* map_alloc frees the old buffers first */
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "clearcut: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  const int fx = 3;
  const int fy = 3;
  map.terrain[fy * map.width + fx] = 10; /* mixed forest */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  col->x = 2;
  col->y = 3;
  col->warehouse_level = 0;
  col->stock[COLONIZE_CARGO_LUMBER] = 5;
  snprintf(col->name, sizeof(col->name), "Timber");
  colonies.colony_count = 1;

  const int pid = units_spawn(&pool, pioneer, fx, fy);
  ColonizeUnit* u = units_get(&pool, pid);
  if (!u || !units_is_pioneer(&pool, pid)) {
    fprintf(stderr, "clearcut: pioneer spawn failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  u->nation_id = 0;
  u->tools = 100;
  u->moves = 1 * UNITS_MP_PER_TILE;
  u->profession = UNITS_JOB_NONE;
  u->orders = UNITS_ORDER_NONE;
  u->col1_counter16 = 0;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "clearcut: GAME.TXT load failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);

  char msg[96];
  msg[0] = '\0';
  if (!units_pioneer_plow_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map)}, pid, msg, sizeof(msg), &pops, &game_txt)) {
    fprintf(stderr, "clearcut: plow start failed (%s)\n", msg);
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  int guard = 0;
  while (u->orders == UNITS_ORDER_CLEAR_PLOW && guard++ < 16) {
    u->moves = 1 * UNITS_MP_PER_TILE;
    if (!units_pioneer_work_tick_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map)}, pid, msg, sizeof(msg), &pops, &game_txt)) {
      fprintf(stderr, "clearcut: work tick failed (%s)\n", msg);
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }
  if (u->orders == UNITS_ORDER_CLEAR_PLOW) {
    fprintf(stderr, "clearcut: never completed clear\n");
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  const int pedia = map_pedia_terrain_index_at(&map, fx, fy);
  if (pedia >= 8 && pedia <= 23) {
    fprintf(stderr, "clearcut: forest still present pedia=%d\n", pedia);
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (col->stock[COLONIZE_CARGO_LUMBER] != 25) {
    fprintf(
      stderr,
      "clearcut: lumber want 25 got %d\n",
      col->stock[COLONIZE_CARGO_LUMBER]
    );
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "lumber") == NULL &&
       strstr(pops.queue[0].body, "Lumber") == NULL &&
       strstr(pops.queue[0].body, "Timber") == NULL)) {
    fprintf(
      stderr,
      "clearcut: CLEARCUT popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  /* bugs.md #278: @DEFOREST is a dead GAME.TXT section (tag absent from
   * VICEROY.EXE) — a chop shows @CLEARCUT alone, never a second popup. */
  for (int i = 0; i < pops.queue_count; ++i) {
    if (strstr(pops.queue[i].body, "Deforestation") != NULL ||
        strstr(pops.queue[i].body, "deforestation") != NULL) {
      fprintf(stderr, "clearcut: unexpected DEFOREST popup body='%s'\n", pops.queue[i].body);
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }

  assets_msg_free(&game_txt);
  map_free(&map);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: CLEARCUT chrome ok (no DEFOREST double)\n");
  return 0;
}

/* Helper: tools=20 road complete → Colonists + @USEDUPTOOLS. */
static int unit_useduptools(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "usedup: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "usedup: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  const int colonist = units_find_type(&pool, "Colonists");
  if (pioneer < 0 || colonist < 0) {
    fprintf(stderr, "usedup: missing types\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map)); /* map_alloc frees the old buffers first */
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "usedup: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains — road terr_cost 1 → one tick */
  }
  const int x = 3;
  const int y = 3;
  map_tile_set_road(&map, x, y, false);

  const int pid = units_spawn(&pool, pioneer, x, y);
  ColonizeUnit* u = units_get(&pool, pid);
  if (!u || !units_is_pioneer(&pool, pid)) {
    fprintf(stderr, "usedup: pioneer spawn failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  u->nation_id = 0;
  u->tools = 20;
  u->moves = 1 * UNITS_MP_PER_TILE;
  u->profession = UNITS_JOB_NONE;
  u->orders = UNITS_ORDER_BUILD_ROAD;
  u->col1_counter16 = 0;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "usedup: GAME.TXT load failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);

  char msg[96];
  msg[0] = '\0';
  /*
   * Non-Hardy road-turns threshold is the real DS:0x2f78 terrain byte + 2
   * (5 turns for this test's terrain class, since the live 2026-08-20
   * capture landed pioneer_threshold[2]=3) — not a single-tick "terr_cost
   * 1" approximation any more, so drive the tick loop to completion rather
   * than assuming one call finishes the road.
   */
  for (int tick = 0; tick < 10 && u->orders == UNITS_ORDER_BUILD_ROAD; ++tick) {
    u->moves = 1 * UNITS_MP_PER_TILE;
    if (!units_pioneer_work_tick_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, pid, msg, sizeof(msg), &pops, &game_txt)) {
      fprintf(stderr, "usedup: work tick failed (%s)\n", msg);
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }
  if (!map_tile_has_road(&map, x, y) || u->type_index != colonist || u->tools != 0) {
    fprintf(
      stderr,
      "usedup: want road+Colonists tools=0 got road=%d type=%d tools=%d (%s)\n",
      (int)map_tile_has_road(&map, x, y),
      u->type_index,
      u->tools,
      msg
    );
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "tools") == NULL &&
       strstr(pops.queue[0].body, "colonist") == NULL &&
       strstr(pops.queue[0].body, "Colonist") == NULL)) {
    fprintf(
      stderr,
      "usedup: USEDUPTOOLS popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  /*
   * bugs.md #509 — FUN_479b_0158 raw 76706-76709: the type reverts to 0
   * (Colonists) EXCEPT when +0x315b == 0x18, which reverts to type 3
   * (Missionaries). The port always handed back Colonists.
   */
  {
    const int missionary = units_find_type(&pool, "Missionaries");
    const int x2 = 5;
    const int y2 = 3;
    map_tile_set_road(&map, x2, y2, false);
    const int jid = units_spawn(&pool, pioneer, x2, y2);
    ColonizeUnit* ju = units_get(&pool, jid);
    if (missionary < 0 || !ju) {
      fprintf(stderr, "usedup: jesuit pioneer setup failed\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ju->nation_id = 0;
    ju->tools = 20;
    ju->profession = UNITS_JOB_MISSIONARY;
    ju->orders = UNITS_ORDER_BUILD_ROAD;
    ju->col1_counter16 = 0;
    for (int tick = 0; tick < 10 && ju->orders == UNITS_ORDER_BUILD_ROAD; ++tick) {
      ju->moves = 1 * UNITS_MP_PER_TILE;
      (void)units_pioneer_work_tick_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, jid, msg, sizeof(msg), &pops, &game_txt);
    }
    if (ju->type_index != missionary) {
      fprintf(stderr, "usedup #509: jesuit should revert to Missionaries, got type=%d\n",
              ju->type_index);
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }

  assets_msg_free(&game_txt);
  map_free(&map);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: USEDUPTOOLS demotion + popup + #509 jesuit revert ok\n");
  return 0;
}

/*
 * FUN_521d_5b66 case 8 (= FUN_479b_01a6), the three arms the port was thin
 * on before 2026-09-06e:
 *   1. the lumber grant needs a same-nation colony within DOS distance 4
 *      (FUN_281f_0614 + DS:0x8db8 < 4) — a colony 5 away gets nothing;
 *   2. with a Lumber Mill the scale is the terrain +8 byte PLUS the colony
 *      tile's layer2 road|city bump, i.e. (byte + 1) * 20;
 *   3. LAB_479b_043b: finishing a clear on tribal land angers the tribe
 *      (base 5 + difficulty for a human, x2 within distance 3, +base again
 *      within 2), while an AI nation that can cover price + price/2 buys the
 *      tile through FUN_479b_00ca instead and takes no anger.
 */
static int unit_pioneer_case8_tail(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "case8: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "case8: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  if (pioneer < 0) {
    fprintf(stderr, "case8: no Pioneers type\n");
    assets_msg_free(&names);
    return 1;
  }

  int rc = 1;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    fprintf(stderr, "case8: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = 2; /* plains */
  }

  col1.head.tribe_count = 1;
  col1.head.difficulty = 0;
  /* col1_save_init: unclaimed FFs are -1. Ownership is the per-nation bitmask
   * (smell audit #83), so a zeroed head no longer grants anything; keep the
   * -1 fill so the first-claimer record matches a real save. */
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    col1.head.founding_father[i] = -1;
  }
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    fprintf(stderr, "case8: tribe alloc failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  col1.tribe[0].nation_id = 4; /* indian[0] */
  col1.tribe[0].x = 6;
  col1.tribe[0].y = 6;
  col1.indian[0].tech = 0; /* tech tier 1 -> homeland radius 1 */
  units_set_native_fallout_context(&col1, &map, -1);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  if (!colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "case8: colonies_load_buildings failed\n");
    goto done;
  }
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  col->warehouse_level = 0;
  snprintf(col->name, sizeof(col->name), "Timber");
  colonies.colony_count = 1;

  const int fx = 5;
  const int fy = 6; /* DOS distance 1 from the village at (6,6) */
  const int terr_scale = map_dos_terr_lumber_reward_byte(map_dos_terr_class_at(&map, fx, fy));

  /* --- arm 1 + 2: colony out of range, then a Lumber Mill colony in range. */
  for (int pass = 0; pass < 2; ++pass) {
    map.terrain[fy * map.width + fx] = 10; /* mixed forest again */
    col->x = pass == 0 ? (uint8_t)(fx + 5) : (uint8_t)(fx - 1);
    col->y = (uint8_t)fy;
    col->stock[COLONIZE_CARGO_LUMBER] = 0;
    const int mill = colonies_find_building(&colonies, "Lumber Mill");
    if (mill < 0) {
      fprintf(stderr, "case8: no Lumber Mill building type\n");
      goto done;
    }
    col->has_building[mill] = pass == 1;

    const int pid = units_spawn(&pool, pioneer, fx, fy);
    ColonizeUnit* u = units_get(&pool, pid);
    if (!u) {
      fprintf(stderr, "case8: pioneer spawn failed\n");
      goto done;
    }
    u->nation_id = 0;
    u->tools = 100;
    u->profession = UNITS_JOB_NONE;
    u->orders = UNITS_ORDER_CLEAR_PLOW;
    u->col1_counter16 = 0;
    int guard = 0;
    while (u->orders == UNITS_ORDER_CLEAR_PLOW && guard++ < 24) {
      u->moves = 1 * UNITS_MP_PER_TILE;
      (void)units_pioneer_work_tick_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map)}, pid, NULL, 0, NULL, NULL);
    }
    units_despawn(&pool, pid);
    const int got = (int)col->stock[COLONIZE_CARGO_LUMBER];
    if (pass == 0) {
      if (got != 0) {
        fprintf(stderr, "case8: colony 5 away must get no lumber, got %d\n", got);
        goto done;
      }
    } else {
      int want = (terr_scale + 1) * 20;
      const int cap = colonies_warehouse_capacity(&colonies, col, COLONIZE_CARGO_LUMBER);
      if (want > cap) {
        want = cap;
      }
      if (got != want) {
        fprintf(stderr, "case8: mill lumber want %d got %d\n", want, got);
        goto done;
      }
    }
  }

  /* --- arm 3a: human nation on tribal land -> alarm 5 * 3 (distance 1). */
  col1.player[0].control = 0; /* human */
  col1.indian[0].alarm_by_player[0] = 0;
  map.terrain[fy * map.width + fx] = 10;
  {
    const int pid = units_spawn(&pool, pioneer, fx, fy);
    ColonizeUnit* u = units_get(&pool, pid);
    if (!u) {
      fprintf(stderr, "case8: human pioneer spawn failed\n");
      goto done;
    }
    u->nation_id = 0;
    u->tools = 100;
    u->profession = UNITS_JOB_NONE;
    u->orders = UNITS_ORDER_CLEAR_PLOW;
    u->col1_counter16 = 0;
    int guard = 0;
    while (u->orders == UNITS_ORDER_CLEAR_PLOW && guard++ < 24) {
      u->moves = 1 * UNITS_MP_PER_TILE;
      (void)units_pioneer_work_tick_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map)}, pid, NULL, 0, NULL, NULL);
    }
    units_despawn(&pool, pid);
    if (col1.indian[0].alarm_by_player[0] != 15) {
      fprintf(
        stderr,
        "case8: human clear alarm want 15 got %d\n",
        (int)col1.indian[0].alarm_by_player[0]
      );
      goto done;
    }
  }

  /* --- arm 3b: AI nation with gold buys the land instead of angering it. */
  col1.player[1].control = 1; /* AI */
  col1.indian[0].alarm_by_player[1] = 0;
  col1.indian[0].lands_bought = 0;
  col1.nation[1].gold = 5000;
  map.terrain[fy * map.width + fx] = 10;
  {
    const int price = colonies_indian_land_purchase_gold(&col1, &map, fx, fy, 1);
    if (price <= 0) {
      fprintf(stderr, "case8: AI land price should be positive, got %d\n", price);
      goto done;
    }
    const int pid = units_spawn(&pool, pioneer, fx, fy);
    ColonizeUnit* u = units_get(&pool, pid);
    if (!u) {
      fprintf(stderr, "case8: AI pioneer spawn failed\n");
      goto done;
    }
    u->nation_id = 1;
    u->tools = 100;
    u->profession = UNITS_JOB_NONE;
    u->orders = UNITS_ORDER_CLEAR_PLOW;
    u->col1_counter16 = 0;
    int guard = 0;
    while (u->orders == UNITS_ORDER_CLEAR_PLOW && guard++ < 24) {
      u->moves = 1 * UNITS_MP_PER_TILE;
      (void)units_pioneer_work_tick_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map)}, pid, NULL, 0, NULL, NULL);
    }
    units_despawn(&pool, pid);
    if (col1.indian[0].alarm_by_player[1] != 0) {
      fprintf(
        stderr,
        "case8: AI buyer must not anger the tribe, alarm %d\n",
        (int)col1.indian[0].alarm_by_player[1]
      );
      goto done;
    }
    if (col1.nation[1].gold != (uint32_t)(5000 - price)) {
      fprintf(
        stderr,
        "case8: AI gold want %d got %u\n",
        5000 - price,
        col1.nation[1].gold
      );
      goto done;
    }
    if (col1.indian[0].lands_bought != 1) {
      fprintf(stderr, "case8: lands_bought want 1 got %d\n", (int)col1.indian[0].lands_bought);
      goto done;
    }
    if ((map.layer2[fy * map.width + fx] & MAP_LAYER2_PURCHASED) == 0) {
      fprintf(stderr, "case8: purchased bit not stamped\n");
      goto done;
    }
  }

  rc = 0;
  fprintf(stderr, "unit_units: 5b66 case-8 lumber radius + tribal-land tail ok\n");
done:
  units_set_native_fallout_context(NULL, NULL, -1);
  free(col1.tribe);
  map_free(&map);
  assets_msg_free(&names);
  return rc;
}

/* Pioneer order gates: silent non-pioneer refusal (#621), @NOPLOW / @NOROAD. */
/*
 * Pioneer DOS-fidelity fixes: bugs.md #615 (no MP gate), #617 (road hammers go
 * to the nearest colony of ANY nation, and only if it is ours), #618 (road
 * refused on a settlement tile), #619 (Arctic plowable, Hills not),
 * #620 (progress counter survives an order change).
 */
static int unit_pioneer_dos_gates_2026_09_22(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "piodos: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "piodos: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (pioneer < 0 || !map_alloc(&map, 16, 16, err, sizeof(err))) {
    fprintf(stderr, "piodos: setup failed\n");
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  int rc = 0;

  /* #615: a pioneer at 0 MP may still take (and tick) the order. */
  {
    const int pid = units_spawn(&pool, pioneer, 4, 4);
    ColonizeUnit* pu = units_get(&pool, pid);
    pu->nation_id = 0;
    pu->tools = 100;
    pu->moves = 0;
    err[0] = '\0';
    if (!units_pioneer_plow_w(
          &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map},
          pid, err, sizeof(err), NULL, NULL)) {
      fprintf(stderr, "piodos #615: 0-MP plow order refused: %s\n", err);
      rc = 1;
    } else if (units_get(&pool, pid)->col1_counter16 != 1) {
      fprintf(stderr, "piodos #615: no work tick at 0 MP\n");
      rc = 1;
    }
    /* #620: switching to Build Road must NOT reset the progress counter. */
    if (!rc) {
      err[0] = '\0';
      units_pioneer_road_w(
        &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map},
        pid, err, sizeof(err), NULL, NULL);
      if (units_get(&pool, pid)->col1_counter16 < 2) {
        fprintf(
          stderr,
          "piodos #620: counter reset on order change (%d)\n",
          units_get(&pool, pid)->col1_counter16
        );
        rc = 1;
      }
    }
    units_despawn(&pool, pid);
  }

  /* #619: Hills (pedia 0x1c) deny clear/plow; Arctic (24) allows it. */
  if (!rc) {
    const int pid = units_spawn(&pool, pioneer, 6, 6);
    ColonizeUnit* pu = units_get(&pool, pid);
    pu->nation_id = 0;
    pu->tools = 100;
    map.terrain[6 * 16 + 6] = (uint8_t)((map.terrain[6 * 16 + 6] & ~0x1f) | 7); /* Arctic base */
    if (map_pedia_terrain_index_at(&map, 6, 6) == 24) {
      err[0] = '\0';
      if (!units_pioneer_plow_w(
            &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map},
            pid, err, sizeof(err), NULL, NULL)) {
        fprintf(stderr, "piodos #619: Arctic plow refused: %s\n", err);
        rc = 1;
      }
    }
    units_despawn(&pool, pid);
  }

  /* #617 / #618: road beside a nearer FOREIGN colony pays nobody, and a road
   * cannot be started on a settlement tile at all. */
  if (!rc) {
    const int foreign = colonies_found(&colonies, &map, 9, 9, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
    const int mine = colonies_found(&colonies, &map, 13, 13, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
    if (foreign < 0 || mine < 0) {
      fprintf(stderr, "piodos: colonies_found failed\n");
      rc = 1;
    } else {
      ColonizeColony* fc = colonies_get_mut(&colonies, foreign);
      ColonizeColony* mc = colonies_get_mut(&colonies, mine);
      const uint16_t h0 = mc->hammers_purchased;
      const int pid = units_spawn(&pool, pioneer, 10, 9);
      ColonizeUnit* pu = units_get(&pool, pid);
      pu->nation_id = 0;
      pu->tools = 100;
      pu->profession = UNITS_JOB_NONE;
      for (int t = 0; t < 40 && units_get(&pool, pid)->orders == UNITS_ORDER_BUILD_ROAD; ++t) {
        units_pioneer_work_tick_w(
          &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map},
          pid, NULL, 0, NULL, NULL);
      }
      err[0] = '\0';
      units_pioneer_road_w(
        &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map},
        pid, err, sizeof(err), NULL, NULL);
      for (int t = 0; t < 40 && units_get(&pool, pid)->orders == UNITS_ORDER_BUILD_ROAD; ++t) {
        units_pioneer_work_tick_w(
          &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map},
          pid, NULL, 0, NULL, NULL);
      }
      if (!map_tile_has_road(&map, 10, 9)) {
        fprintf(stderr, "piodos #617: road never completed\n");
        rc = 1;
      } else if (mc->hammers_purchased != h0) {
        fprintf(stderr, "piodos #617: distant own colony was credited\n");
        rc = 1;
      }
      (void)fc;
      units_despawn(&pool, pid);

      /* #618: on the foreign colony's own tile, Build Road is refused. */
      const int pid2 = units_spawn(&pool, pioneer, 9, 9);
      ColonizeUnit* p2 = units_get(&pool, pid2);
      p2->nation_id = 0;
      p2->tools = 100;
      err[0] = '\0';
      if (units_pioneer_road_w(
            &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map},
            pid2, err, sizeof(err), NULL, NULL) ||
          units_get(&pool, pid2)->orders == UNITS_ORDER_BUILD_ROAD) {
        fprintf(stderr, "piodos #618: road accepted on a settlement tile\n");
        rc = 1;
      }
      units_despawn(&pool, pid2);
    }
  }

  map_free(&map);
  assets_msg_free(&names);
  if (rc == 0) {
    printf("unit_units: pioneer DOS gates (#615/#617/#618/#619/#620) ok\n");
  }
  return rc;
}

static int unit_pioneer_order_gates(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "ordgate: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "ordgate: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  const int colonist = units_find_type(&pool, "Colonists");
  if (pioneer < 0 || colonist < 0) {
    fprintf(stderr, "ordgate: missing types\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map)); /* map_alloc frees the old buffers first */
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "ordgate: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains */
  }

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "ordgate: GAME.TXT load failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  char msg[96];
  AiPopupState pops;

  /* 1) Non-pioneer → ONLYPIO */
  {
    const int cid = units_spawn(&pool, colonist, 2, 2);
    ColonizeUnit* cu = units_get(&pool, cid);
    if (!cu) {
      fprintf(stderr, "ordgate: colonist spawn failed\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    cu->nation_id = 0;
    cu->moves = 1 * UNITS_MP_PER_TILE;
    ai_popup_init(&pops);
    msg[0] = '\0';
    if (units_pioneer_plow_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, cid, msg, sizeof(msg), &pops, &game_txt)) {
      fprintf(stderr, "ordgate: non-pioneer plow should fail\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* bugs.md #621: no @ONLYPIO popup — DOS greys the menu row instead, so
     * the refusal must be silent. */
    if (pops.queue_count != 0) {
      fprintf(
        stderr,
        "ordgate: non-pioneer must not raise a popup, q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, cid);
  }

  /* 2) Pioneer on plowed → NOPLOW */
  {
    map_tile_set_plowed(&map, 3, 3, true);
    map_tile_set_road(&map, 3, 3, false);
    const int pid = units_spawn(&pool, pioneer, 3, 3);
    ColonizeUnit* pu = units_get(&pool, pid);
    if (!pu) {
      fprintf(stderr, "ordgate: pioneer spawn failed\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu->nation_id = 0;
    pu->tools = 100;
    pu->moves = 1 * UNITS_MP_PER_TILE;
    ai_popup_init(&pops);
    msg[0] = '\0';
    if (units_pioneer_plow_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, pid, msg, sizeof(msg), &pops, &game_txt)) {
      fprintf(stderr, "ordgate: plowed plow should fail\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "plow") == NULL &&
         strstr(pops.queue[0].body, "plowed") == NULL)) {
      fprintf(
        stderr,
        "ordgate: NOPLOW weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, pid);
  }

  /* 3) Pioneer on road → NOROAD */
  {
    map_tile_set_plowed(&map, 4, 4, false);
    map_tile_set_road(&map, 4, 4, true);
    const int pid = units_spawn(&pool, pioneer, 4, 4);
    ColonizeUnit* pu = units_get(&pool, pid);
    if (!pu) {
      fprintf(stderr, "ordgate: pioneer2 spawn failed\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu->nation_id = 0;
    pu->tools = 100;
    pu->moves = 1 * UNITS_MP_PER_TILE;
    ai_popup_init(&pops);
    msg[0] = '\0';
    if (units_pioneer_road_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, pid, msg, sizeof(msg), &pops, &game_txt)) {
      fprintf(stderr, "ordgate: existing road should fail\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (pops.queue_count < 1 || strstr(pops.queue[0].body, "road") == NULL) {
      fprintf(
        stderr,
        "ordgate: NOROAD weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, pid);
  }

  assets_msg_free(&game_txt);
  map_free(&map);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: pioneer order-gate popups ok\n");
  return 0;
}
int main(void) {
  diag_init(0, NULL);
  if (unit_clearcut_lumber() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_useduptools() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_pioneer_order_gates() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_pioneer_dos_gates_2026_09_22() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_pioneer_case8_tail() != 0) {
    diag_shutdown();
    return 1;
  }
  diag_shutdown();
  return 0;
}
