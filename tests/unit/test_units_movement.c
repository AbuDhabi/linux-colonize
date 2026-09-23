#include "test_units_common.h"


/*
 * DOS fog model: FUN_13f1_0158 reveal marks unit vis bits (FUN_1427_09ac) and
 * colony pop/fort snapshots (FUN_364b_1b4c); FUN_1427_0c9a/0968 reset a mover's
 * mask to "tile owner | who watches the tile"; rim tiles never reveal.
 */
static int unit_fog_vis_mask_and_snapshot(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 12, 12, err, sizeof(err))) {
    fprintf(stderr, "fog: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 12 * 12; ++i) {
    map.terrain[i] = 2;    /* plains */
    map.layer3[i] = 0xf1;  /* unowned, continent 1 */
  }
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Colonists");
  units.type_count = 1;
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(&map);

  const int a = units_spawn(&units, 0, 3, 3);
  const int b = units_spawn(&units, 0, 5, 5);
  const int brave = units_spawn(&units, 0, 3, 5); /* outer ring of a's radius-2 sight */
  units_set_nation(units_get(&units, a), 0);
  units_set_nation(units_get(&units, b), 1);
  units_set_nation(units_get(&units, brave), 6);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 2;
  col->x = 4;
  col->y = 3;
  col->population = 5;
  colonies.colony_count = 1;
  units_occupancy_rebuild(&units);

  /* Reveal radius 1 from a: b (5,5) is outside, colony (4,3) inside. */
  (void)units_reveal_sight_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&units), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL)}, units_get(&units, a));
  if ((units_get(&units, b)->col1_vis_mask & 1u) != 0) {
    fprintf(stderr, "fog: unit outside sight got viewer bit\n");
    return 1;
  }
  if (col->pop_on_map[0] != 5 || col->fort_on_map[0] != 0 || col->pop_on_map[1] != 0) {
    fprintf(stderr, "fog: colony snapshot wrong (%d/%d/%d)\n", col->pop_on_map[0],
            col->fort_on_map[0], col->pop_on_map[1]);
    return 1;
  }
  if (colonies_known_to(col, 1, false) || !colonies_known_to(col, 0, false) ||
      !colonies_known_to(col, 1, true)) {
    fprintf(stderr, "fog: colonies_known_to gate wrong\n");
    return 1;
  }
  /* Owner stamp: revealed unowned tile (2,2) now carries nation 0 (FUN_137f_0228). */
  if (((map_get_layer3(&map, 2, 2) >> 4) & 0x0f) != 0) {
    fprintf(stderr, "fog: reveal did not stamp owner nibble\n");
    return 1;
  }
  /* Rim never reveals (FUN_137f_000a inset gate). */
  if (map_tile_seen_by(&map, 0, 0, 0) && map_tile_seen_by(&map, 11, 11, 0)) {
    fprintf(stderr, "fog: rim tile revealed\n");
    return 1;
  }

  /* Outer ring (radius 2 via a fake Galleon-less path): use map_reveal_sight_each directly. */
  map_reveal_sight(&map, 3, 3, 3, 2, false);
  if (!map_tile_seen_by(&map, 3, 5, 3)) {
    fprintf(stderr, "fog: same-continent land outer ring not revealed\n");
    return 1;
  }

  /* Move b next to a: mask := watchers (nation 0 adjacent) | own bit. */
  units_vis_mask_after_move(&units, &map, b, 4, 4);
  const uint8_t mb = units_get(&units, b)->col1_vis_mask;
  if ((mb & 1u) == 0 || (mb & 2u) == 0) {
    fprintf(stderr, "fog: mask after move next to watcher = 0x%02x\n", mb);
    return 1;
  }
  /* Move b away again: watcher bit drops, own bit stays. */
  units_vis_mask_after_move(&units, &map, b, 8, 8);
  const uint8_t mb2 = units_get(&units, b)->col1_vis_mask;
  if ((mb2 & 1u) != 0 || (mb2 & 2u) == 0) {
    fprintf(stderr, "fog: mask after move away = 0x%02x\n", mb2);
    return 1;
  }
  /*
   * bugs.md: an Indian Convert must carry its own sprite on the map, not the
   * plain Free Colonist one — DOS FUN_112b_0060 routes every @UNIT type 0
   * through the profession table (FUN_112b_0002 case 0x1b -> icon 0x43 =
   * sprite 66 here), the same icon the colony screen already used.
   */
  {
    ColonizeUnitPool cpool;
    memset(&cpool, 0, sizeof(cpool));
    memset(&cpool, 0, sizeof(cpool));
    units_reset(&cpool);
    units_set_occupancy_map(NULL);
    cpool.type_count = 3;
    snprintf(cpool.types[0].name, sizeof(cpool.types[0].name), "Colonists");
    cpool.types[0].movement = 1;
    cpool.types[0].icon_sprite = 100; /* Free Colonist */
    /* bugs.md #591: the name channel is the @UNIT row, so the fixture needs
     * the Dragoons row an armed+mounted body displays as. */
    snprintf(cpool.types[2].name, sizeof(cpool.types[2].name), "Dragoons");
    cpool.types[2].movement = 4;
    cpool.types[2].kind_plus1 = (uint8_t)(UNITS_KIND_DRAGOON + 1);
    const int plain = units_spawn_allow_stack(&cpool, 0, 3, 3);
    const int conv = units_spawn_allow_stack(&cpool, 0, 4, 3);
    ColonizeUnit* cu = units_get(&cpool, conv);
    if (!cu || plain < 0) {
      fprintf(stderr, "convert sprite: spawn failed\n");
      return 1;
    }
    cu->profession = COLONIZE_PROF_CONVERT;
    if (units_map_sprite(&cpool, plain) != 100) {
      fprintf(stderr, "convert sprite: plain colonist map icon %d want 100\n",
              units_map_sprite(&cpool, plain));
      return 1;
    }
    if (units_map_sprite(&cpool, conv) != 66) {
      fprintf(stderr, "convert sprite: convert map icon %d want 66\n",
              units_map_sprite(&cpool, conv));
      return 1;
    }
    /* Equipment still wins, as it does in DOS's type != 0 overrides. */
    cu->muskets = UNITS_EQUIP_MUSKETS;
    if (units_map_sprite(&cpool, conv) != UNITS_ICON_SOLDIER) {
      fprintf(stderr, "convert sprite: armed convert should draw as a soldier\n");
      return 1;
    }
    cu->muskets = 0;
    fprintf(stderr, "convert map sprite ok\n");

    /* bugs.md #263: a mounted Veteran Soldier (profession 0x15) draws with the
     * Veteran Dragoon art, not the plain grey dragoon. */
    ColonizeUnit* vu = units_get(&cpool, plain);
    vu->profession = UNITS_JOB_SOLDIER;
    vu->muskets = UNITS_EQUIP_MUSKETS;
    if (units_map_sprite(&cpool, plain) != UNITS_ICON_VETERAN_SOLDIER) {
      fprintf(stderr, "veteran sprite: armed vet icon %d want %d\n",
              units_map_sprite(&cpool, plain), UNITS_ICON_VETERAN_SOLDIER);
      return 1;
    }
    vu->horses = UNITS_EQUIP_HORSES;
    if (units_map_sprite(&cpool, plain) != UNITS_ICON_VETERAN_DRAGOON) {
      fprintf(stderr, "veteran sprite: mounted vet icon %d want %d\n",
              units_map_sprite(&cpool, plain), UNITS_ICON_VETERAN_DRAGOON);
      return 1;
    }
    /* bugs.md #591: the NAME channel is the @UNIT row (a mounted armed body
     * displays as the Dragoons row); "Veteran" is the profession line. */
    {
      const int dragoon_ty = units_kind_type_index(&cpool, UNITS_KIND_DRAGOON);
      const ColonizeUnitType* dt = dragoon_ty >= 0 ? units_type(&cpool, dragoon_ty) : NULL;
      if (!dt || strcmp(units_display_name(&cpool, vu), dt->name) != 0) {
        fprintf(stderr, "veteran name: mounted vet reads '%s' want @UNIT dragoon row\n",
                units_display_name(&cpool, vu));
        return 1;
      }
    }
    vu->muskets = 0;
    vu->horses = 0;
    vu->profession = 0;
    fprintf(stderr, "veteran dragoon chrome ok\n");

    /*
     * bugs.md #420: FUN_112b_0060's tail downgrades a commissioned missionary
     * whose colonist is not a Jesuit (profession != 0x18) to icon 0x4e =
     * sprite 77; the Jesuit keeps the @UNIT icon 106 → sprite 105. And
     * FUN_112b_0002 case 5 gives a Jesuit *working colonist* icon 0x3e =
     * sprite 61, not either commissioned pose.
     */
    cpool.type_count = 2;
    snprintf(cpool.types[1].name, sizeof(cpool.types[1].name), "Missionaries");
    cpool.types[1].movement = 1;
    cpool.types[1].icon_sprite = UNITS_ICON_JESUIT_MISSIONARY;
    const int miss = units_spawn_allow_stack(&cpool, 1, 5, 3);
    ColonizeUnit* mu = units_get(&cpool, miss);
    if (!mu) {
      fprintf(stderr, "missionary sprite: spawn failed\n");
      return 1;
    }
    mu->profession = UNITS_JOB_NONE;
    if (units_map_sprite(&cpool, miss) != UNITS_ICON_MISSIONARY) {
      fprintf(stderr, "missionary sprite: unskilled icon %d want %d\n",
              units_map_sprite(&cpool, miss), UNITS_ICON_MISSIONARY);
      return 1;
    }
    mu->profession = UNITS_JOB_MISSIONARY;
    if (units_map_sprite(&cpool, miss) != UNITS_ICON_JESUIT_MISSIONARY) {
      fprintf(stderr, "missionary sprite: Jesuit icon %d want %d\n",
              units_map_sprite(&cpool, miss), UNITS_ICON_JESUIT_MISSIONARY);
      return 1;
    }
    if (units_job_icon_sprite(UNITS_JOB_MISSIONARY) != UNITS_ICON_JESUIT_MISSIONARY_WORK) {
      fprintf(stderr, "missionary sprite: working portrait %d want %d\n",
              units_job_icon_sprite(UNITS_JOB_MISSIONARY),
              UNITS_ICON_JESUIT_MISSIONARY_WORK);
      return 1;
    }
    fprintf(stderr, "missionary sprite split ok\n");
  }

  /*
   * Founding reveal is Coronado-only (FUN_364b_1dd6 gates FUN_13f1_00a6 on
   * FF 6 per nation): without him nothing is revealed; with him it is ±5 and
   * seeds pop_on_map=1 on colonies inside that had not been observed.
   */
  ColonizeColony* col2 = &colonies.colonies[1];
  col2->active = true;
  col2->id = 2;
  col2->nation_id = 3;
  col2->x = 8;
  col2->y = 8;
  colonies.colony_count = 2;

  ColonizeCol1Save fog_col1;
  memset(&fog_col1, 0, sizeof(fog_col1));
  colonies_reveal_founded_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&fog_col1), .col1_ok=true}, 2);
  if (col2->pop_on_map[3] != 0) {
    fprintf(stderr, "fog: founding revealed without Coronado\n");
    return 1;
  }

  fog_col1.nation[3].founding_fathers[FF_FRANCISCO_CORONADO / 8] =
    (uint8_t)(1u << (FF_FRANCISCO_CORONADO % 8));
  colonies_reveal_founded_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&fog_col1), .col1_ok=true}, 2);
  if (!map_tile_seen_by(&map, 3, 3, 3) || map_tile_seen_by(&map, 1, 9, 3) ||
      col->pop_on_map[3] != 1 || col2->pop_on_map[3] != 1) {
    fprintf(stderr, "fog: founding reveal/seed wrong\n");
    return 1;
  }

  units_set_occupancy_map(NULL);
  map_free(&map);
  fprintf(stderr, "fog vis mask / snapshot ok\n");
  return 0;
}

/*
 * FUN_OVL20_L0000__0015bc neighbour pick (viceroy_overlays.c:86760-86840):
 * score = flood cost[cand] + edge(unit->cand), where a cardinal step
 * between two river tiles costs 1 (else 3 for a `movement < 4` unit).
 * Pioneer at (4,2), goal (2,4), minor river on (4,2),(3,2),(2,2),(2,3),(2,4):
 * (3,2) costs 4+1 = 5 along the river; (3,3) costs 4+3 = 7 diagonally.
 * DOS steps west onto the river; picking by cost[] alone would take (3,3).
 */
static int unit_flood_river_pair_step(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Pioneers");
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "river_pair: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  static const int k_river[5][2] = {{4, 2}, {3, 2}, {2, 2}, {2, 3}, {2, 4}};
  for (int i = 0; i < 5; ++i) {
    map.terrain[k_river[i][1] * 8 + k_river[i][0]] = 2 | 0x40; /* minor river */
  }

  const int id = units_spawn_allow_stack(&pool, 0, 4, 2);
  ColonizeUnit* u = units_get(&pool, id);
  if (!u) {
    map_free(&map);
    fprintf(stderr, "river_pair: spawn failed\n");
    return 1;
  }
  u->nation_id = 0;
  u->moves = 1 * UNITS_MP_PER_TILE;
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = 2;
  u->goto_y = 4;

  int rc = 0;
  int nx = -1;
  int ny = -1;
  if (!units_next_goto_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, id, &nx, &ny)) {
    fprintf(stderr, "river_pair: no step found\n");
    rc = 1;
  } else if (nx != 3 || ny != 2) {
    fprintf(stderr, "river_pair: expected (3,2) along the river, got (%d,%d)\n", nx, ny);
    rc = 1;
  }
  /* Without the river the diagonal (3,3) wins (4+3 = 7 vs (3,2) 7+3). */
  for (int i = 0; i < 5; ++i) {
    map.terrain[k_river[i][1] * 8 + k_river[i][0]] = 2;
  }
  if (rc == 0 && (!units_next_goto_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, id, &nx, &ny) || nx != 3 || ny != 3)) {
    fprintf(stderr, "river_pair: plain map should step (3,3), got (%d,%d)\n", nx, ny);
    rc = 1;
  }

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: flood river-pair neighbour pick ok\n");
  }
  return rc;
}

/*
 * bugs.md #696 / #697 / #701 — FUN_479b_0972 + FUN_6662_0f74 tails.
 *  #701 the adjacent tier is FUN_6662_0086, a bare sign->dir8 lookup: it
 *       returns the sign step even onto a tile the unit cannot enter.
 *  #696 a pathfinder miss spends the whole allotment (FUN_281f_0934) and
 *       clears +0x314c.
 *  #697 on arrival AI_MOVE (0x0c) keeps its order like TRADE_ROUTE (0x02),
 *       and AI_SAIL (0x0b) is finished on the spot.
 */
static int unit_goto_dos_tails(void) {
  units_reset_state();

  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Free Colonist");
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Caravel");
  pool.types[1].movement = 4;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    fprintf(stderr, "goto_tails: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = 25; /* ocean */
    map.layer3[i] = 1;   /* continent 1: open sea, not a lake */
  }
  /* One 3x3 plains island at (4..6, 4..6). */
  for (int y = 4; y <= 6; ++y) {
    for (int x = 4; x <= 6; ++x) {
      map.terrain[y * 16 + x] = 2; /* plains */
    }
  }
  map.terrain[2 * 16 + 2] = 2; /* lone 1x1 plains islet for the #696 case */

  int rc = 0;
  const ColonizeWorld w = {
    .units = &pool, .colonies = NULL, .map = &map, .rng = NULL
  };

  /* #701: adjacent goal on water — the sign step is returned anyway. */
  const int land = units_spawn_allow_stack(&pool, 0, 6, 6);
  ColonizeUnit* u = units_get(&pool, land);
  if (!u) {
    map_free(&map);
    fprintf(stderr, "goto_tails: land spawn failed\n");
    return 1;
  }
  u->nation_id = 0;
  u->moves = UNITS_MP_PER_TILE;
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = 7;
  u->goto_y = 7; /* ocean, diagonally adjacent to the island corner */
  int nx = -1;
  int ny = -1;
  if (!units_next_goto_step_w(&w, land, &nx, &ny) || nx != 7 || ny != 7) {
    fprintf(stderr, "goto_tails: #701 adjacent sign step expected (7,7), got (%d,%d)\n", nx, ny);
    rc = 1;
  }

  /* #696: a land unit marooned on a 1x1 islet — every tier misses, so the
   * order is dropped and the whole allotment is spent. */
  if (rc == 0) {
    const int marooned = units_spawn_allow_stack(&pool, 0, 2, 2);
    u = units_get(&pool, marooned);
    if (!u) {
      map_free(&map);
      fprintf(stderr, "goto_tails: islet spawn failed\n");
      return 1;
    }
    u->nation_id = 0;
    u->moves = UNITS_MP_PER_TILE;
    u->orders = UNITS_ORDER_GOTO;
    u->goto_x = 13;
    u->goto_y = 13;
    if (units_advance_goto_one_step_w(&w, marooned)) {
      fprintf(stderr, "goto_tails: #696 unreachable goal must not step\n");
      rc = 1;
    } else if (u->orders != UNITS_ORDER_NONE) {
      fprintf(stderr, "goto_tails: #696 order must be cleared, got %d\n", u->orders);
      rc = 1;
    } else if (u->moves != 0) {
      fprintf(stderr, "goto_tails: #696 allotment must be spent, moves=%d\n", u->moves);
      rc = 1;
    }
  }

  /* #697: arrival — AI_MOVE keeps its order, AI_SAIL is finished. */
  if (rc == 0) {
    u = units_get(&pool, land);
    u->x = 5;
    u->y = 5;
    u->moves = UNITS_MP_PER_TILE;
    u->orders = UNITS_ORDER_AI_MOVE;
    u->goto_x = 6;
    u->goto_y = 5;
    if (!units_advance_goto_one_step_w(&w, land)) {
      fprintf(stderr, "goto_tails: #697 AI_MOVE step must commit\n");
      rc = 1;
    } else if (u->x != 6 || u->y != 5) {
      fprintf(stderr, "goto_tails: #697 AI_MOVE landed at (%d,%d)\n", u->x, u->y);
      rc = 1;
    } else if (u->orders != UNITS_ORDER_AI_MOVE) {
      fprintf(stderr, "goto_tails: #697 AI_MOVE must survive arrival, got %d\n", u->orders);
      rc = 1;
    }
  }
  if (rc == 0) {
    const int ship = units_spawn_allow_stack(&pool, 1, 8, 8);
    ColonizeUnit* s = units_get(&pool, ship);
    if (!s) {
      fprintf(stderr, "goto_tails: ship spawn failed\n");
      rc = 1;
    } else {
      s->nation_id = 0;
      s->moves = 4 * UNITS_MP_PER_TILE;
      s->orders = UNITS_ORDER_AI_SAIL;
      s->goto_x = 9;
      s->goto_y = 8;
      if (!units_advance_goto_one_step_w(&w, ship)) {
        fprintf(stderr, "goto_tails: #697 AI_SAIL step must commit\n");
        rc = 1;
      } else if (s->orders != UNITS_ORDER_NONE) {
        fprintf(stderr, "goto_tails: #697 AI_SAIL order must clear, got %d\n", s->orders);
        rc = 1;
      } else if (s->moves != 0) {
        fprintf(stderr, "goto_tails: #697 AI_SAIL must be finished, moves=%d\n", s->moves);
        rc = 1;
      }
    }
  }

  map_free(&map);
  units_reset_state();
  if (rc == 0) {
    fprintf(stderr, "unit_units: goto DOS tails (#696/#697/#701) ok\n");
  }
  return rc;
}


/*
 * bugs.md: waking a loaded unit out of a ship's hold (tile-stack picker /
 * ORDERS Activate Unit) has to leave it able to walk ashore even when the
 * ship itself has no moves left. Boarding parks the passenger at moves
 * 0 as a skip-select flag, so the wake has to restore its allotment.
 */
static int unit_wake_passenger_can_land(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Caravel");
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].cargo = 2;
  pool.types[1].space = 1;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Colonists");
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "wake_pax: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  for (int y = 0; y < 8; ++y) {
    map.terrain[y * 8 + 4] = 1; /* land column */
  }

  const int ship = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int pax = units_spawn_allow_stack(&pool, 1, 4, 3);
  ColonizeUnit* su = units_get(&pool, ship);
  ColonizeUnit* pu = units_get(&pool, pax);
  if (!su || !pu) {
    map_free(&map);
    fprintf(stderr, "wake_pax: spawn failed\n");
    return 1;
  }
  su->nation_id = 0;
  pu->nation_id = 0;
  su->moves = 4 * UNITS_MP_PER_TILE;
  pu->moves = UNITS_MP_PER_TILE;

  int rc = 0;
  if (!units_board(&pool, pax, ship)) {
    fprintf(stderr, "wake_pax: board failed\n");
    rc = 1;
  }
  pu = units_get(&pool, pax);
  su = units_get(&pool, ship);
  if (rc == 0 && (pu->aboard_ship_id != ship || pu->moves != 0)) {
    fprintf(stderr, "wake_pax: boarding should park the passenger at 0 MP\n");
    rc = 1;
  }
  /* The ship is out of moves: only the passenger's own allotment can land it. */
  if (rc == 0) {
    su->moves = 0;
  }
  if (rc == 0 && !units_wake(&pool, pax)) {
    fprintf(stderr, "wake_pax: units_wake should report the sentry cleared\n");
    rc = 1;
  }
  pu = units_get(&pool, pax);
  if (rc == 0 && (pu->orders != 0 || pu->moves <= 0)) {
    fprintf(stderr, "wake_pax: wake must clear orders and restore MP (orders=%d mp=%d)\n",
            pu->orders, pu->moves);
    rc = 1;
  }
  if (rc == 0 &&
      !units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, ship, pax, 4, 3)) {
    fprintf(stderr, "wake_pax: woken passenger must be able to step ashore\n");
    rc = 1;
  }
  pu = units_get(&pool, pax);
  if (rc == 0 && (pu->aboard_ship_id >= 0 || pu->x != 4 || pu->y != 3)) {
    fprintf(stderr, "wake_pax: passenger should stand on land at (4,3)\n");
    rc = 1;
  }

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: wake loaded passenger -> land ok\n");
  }
  return rc;
}

/*
 * bugs.md: the tile-stack picker must work like DOS FUN_2b5a_1b5a — ONE pick
 * wakes a sentried passenger AND hands it back for selection, closing the
 * dialog. The old two-step (first click only woke, dialog stayed open with
 * no selection) read as "impossible to wake up unit loaded onto a ship
 * without moves".
 */
static int unit_stack_one_click_wakes_and_selects(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Caravel");
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].cargo = 2;
  pool.types[1].space = 1;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Colonists");
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  const int ship = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int pax = units_spawn_allow_stack(&pool, 1, 3, 3);
  ColonizeUnit* su = units_get(&pool, ship);
  ColonizeUnit* pu = units_get(&pool, pax);
  if (!su || !pu) {
    fprintf(stderr, "stack_pick: spawn failed\n");
    return 1;
  }
  su->nation_id = 0;
  pu->nation_id = 0;
  su->moves = 0; /* ship exhausted — the reported scenario */
  if (!units_board_stacked(&pool, pax, ship)) {
    fprintf(stderr, "stack_pick: board failed\n");
    return 1;
  }

  UnitStackPopup dlg;
  memset(&dlg, 0, sizeof(dlg));
  if (!unit_stack_try_open(&dlg, &pool, 3, 3, 0) || dlg.count != 2) {
    fprintf(stderr, "stack_pick: popup should open with ship+passenger\n");
    return 1;
  }
  /* bugs.md two-step: the first pick on an ordered row only cancels its
   * orders and keeps the popup open; the second pick activates and closes. */
  dlg.selection = (dlg.ids[0] == pax) ? 0 : 1;
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  int sel = -1;
  unit_stack_handle_input(&dlg, &pool, &in, &sel);
  pu = units_get(&pool, pax);
  if (sel != -1 || !dlg.open) {
    fprintf(stderr, "stack_pick: first pick must only cancel (sel=%d open=%d)\n", sel, dlg.open);
    return 1;
  }
  if (pu->orders != 0 || pu->moves <= 0) {
    fprintf(
      stderr,
      "stack_pick: first pick must wake the passenger (orders=%d mp=%d)\n",
      pu->orders,
      pu->moves
    );
    return 1;
  }
  unit_stack_handle_input(&dlg, &pool, &in, &sel);
  if (sel != pax || dlg.open) {
    fprintf(stderr, "stack_pick: second pick must select and close (sel=%d open=%d)\n", sel, dlg.open);
    return 1;
  }
  fprintf(stderr, "unit_units: stack picker two-step cancel+activate ok\n");
  return 0;
}


/*
 * bugs.md #720 / #719 / #714 — MP model + entry gating (docs/move_enter.md).
 *
 * #720 FUN_465b_0000 raw 75510-75516: a land unit whose @UNIT attack column
 *      (DS:0x5236) is 0 never enters a foreign Euro colony, defended or not.
 * #719 FUN_4720_015c raw 76012-76020: the landfall passenger is the FIRST
 *      cargo-chain entry with size < 99 and spent < max — one pass.
 * #714 FUN_465b_0000 LAB_465b_05ca: boarding runs the ordinary cost gate.
 */
static int unit_mp_entry_gating_713_724(void) {
  int rc = 0;
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 3;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Colonists");
  pool.types[0].movement = 1;
  pool.types[0].attack = 0;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Soldiers");
  pool.types[1].movement = 1;
  pool.types[1].attack = 2;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(pool.types[2].name, sizeof(pool.types[2].name), "Caravel");
  pool.types[2].movement = 4;
  pool.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[2].space = 2;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "mp_entry: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  map.terrain[3 * 8 + 5] = 25; /* ocean berth */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 1; /* foreign */
  col->x = 4;
  col->y = 3;
  colonies.colony_count = 1;

  /* #720: plain colonist bumping the UNDEFENDED foreign colony. */
  const int cid = units_spawn_allow_stack(&pool, 0, 3, 3);
  ColonizeUnit* cu = units_get(&pool, cid);
  if (!cu) {
    map_free(&map);
    fprintf(stderr, "mp_entry: colonist spawn failed\n");
    return 1;
  }
  cu->nation_id = 0;
  cu->moves = UNITS_MP_PER_TILE;
  const ColonizeEnterReason r_col = units_enter_probe_w(
    &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map},
    cu->type_index, 4, 3, cid
  );
  if (r_col != COLONIZE_ENTER_BOUNCE_FOREIGN) {
    fprintf(stderr, "mp_entry #720: colonist onto empty foreign colony -> %d\n", (int)r_col);
    rc = 1;
  }

  /* A combat-role mover still walks in (capture path). */
  const int sid = units_spawn_allow_stack(&pool, 1, 3, 3);
  ColonizeUnit* su = units_get(&pool, sid);
  if (su) {
    su->nation_id = 0;
    su->moves = UNITS_MP_PER_TILE;
    const ColonizeEnterReason r_sol = units_enter_probe_w(
      &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map},
      su->type_index, 4, 3, sid
    );
    if (rc == 0 && r_sol != COLONIZE_ENTER_OK) {
      fprintf(stderr, "mp_entry #720: soldier must still enter -> %d\n", (int)r_sol);
      rc = 1;
    }
  }

  /* #719: landfall pick is the FIRST chain entry that passes the spent test. */
  const int shid = units_spawn_allow_stack(&pool, 2, 5, 3);
  ColonizeUnit* sh = units_get(&pool, shid);
  const int p1 = units_spawn_allow_stack(&pool, 0, 5, 3);
  const int p2 = units_spawn_allow_stack(&pool, 0, 5, 3);
  if (sh && p1 >= 0 && p2 >= 0) {
    sh->nation_id = 0;
    ColonizeUnit* u1 = units_get(&pool, p1);
    ColonizeUnit* u2 = units_get(&pool, p2);
    u1->nation_id = 0;
    u2->nation_id = 0;
    if (units_board(&pool, p1, shid) && units_board(&pool, p2, shid)) {
      u1 = units_get(&pool, p1);
      u2 = units_get(&pool, p2);
      u1->moves = 0;               /* parked by boarding, spent byte clear */
      u1->mp_spent_turn = 0;
      u2->moves = UNITS_MP_PER_TILE; /* live MP: the old tier-1 would pick it */
      u2->mp_spent_turn = 0;
      if (rc == 0 && units_first_landfall_cargo(&pool, shid) != p1) {
        fprintf(stderr, "mp_entry #719: landfall pick must be the first chain entry\n");
        rc = 1;
      }
    }
  }

  /* #714: boarding from shore runs the cost gate — 1 third left, cost 3, the
   * unit has already spent part of its allotment, so only the roll can pass
   * it; with a 1-move unit the DOS "spent == 0" clause is what normally lets
   * a full-MP unit aboard. Here the gate must refuse and still charge. */
  const int bid = units_spawn_allow_stack(&pool, 0, 4, 3);
  ColonizeUnit* bu = units_get(&pool, bid);
  if (bu) {
    /* Move it off the foreign colony tile the fixture put at (4,3). */
    bu->x = 6;
    bu->y = 3;
    map.terrain[3 * 8 + 7] = 25; /* ocean next to it */
    bu->nation_id = 0;
    bu->moves = 1;               /* < cost 3, and NOT a full allotment */
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 1);
    const int shid2 = units_spawn_allow_stack(&pool, 2, 7, 3);
    ColonizeUnit* sh2 = units_get(&pool, shid2);
    if (sh2) {
      sh2->nation_id = 0;
      int denied = 0;
      for (int i = 0; i < 24 && !denied; ++i) {
        ColonizeUnit* b = units_get(&pool, bid);
        b->x = 6;
        b->y = 3;
        b->aboard_ship_id = -1;
        b->moves = 1;
        b->mp_spent_turn = 0;
        if (!units_try_move_w(
              &(ColonizeWorld){
                .units = &pool, .colonies = &colonies, .map = &map, .rng = &rng
              },
              bid, 7, 3
            )) {
          denied = 1;
        }
      }
      if (rc == 0 && !denied) {
        fprintf(stderr, "mp_entry #714: boarding must be able to fail the DOS cost roll\n");
        rc = 1;
      }
    }
  }

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: MP model + entry gating (#714/#719/#720) ok\n");
  }
  return rc;
}
int main(void) {
  diag_init(0, NULL);
  if (unit_mp_entry_gating_713_724() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_flood_river_pair_step() != 0) {
    return 1;
  }
  if (unit_goto_dos_tails() != 0) {
    return 1;
  }
  if (unit_wake_passenger_can_land() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_stack_one_click_wakes_and_selects() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_fog_vis_mask_and_snapshot() != 0) {
    diag_shutdown();
    return 1;
  }
  diag_shutdown();
  return 0;
}
