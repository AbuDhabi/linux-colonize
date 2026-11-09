/* setenv/unsetenv/fork (the UNITS_FAR_BFS re-exec case) are POSIX. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "test_units_common.h"
#include "../common/test_runner.h"
#include "core/units_internal.h"
#include "core/dos_buggy.h"
#include "core/mods.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h> /* _NSGetExecutablePath: macOS has no /proc/self/exe */
#endif


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
  units_vis_mask_after_move(&units, &map, b, 4, 4, -1);
  const uint8_t mb = units_get(&units, b)->col1_vis_mask;
  if ((mb & 1u) == 0 || (mb & 2u) == 0) {
    fprintf(stderr, "fog: mask after move next to watcher = 0x%02x\n", mb);
    return 1;
  }
  /* Move b away again: watcher bit drops, own bit stays. */
  units_vis_mask_after_move(&units, &map, b, 8, 8, -1);
  const uint8_t mb2 = units_get(&units, b)->col1_vis_mask;
  if ((mb2 & 1u) != 0 || (mb2 & 2u) == 0) {
    fprintf(stderr, "fog: mask after move away = 0x%02x\n", mb2);
    return 1;
  }
  /* 465b 07d6(06dc): the destination's pre-move owner nibble adds its bit. */
  units_vis_mask_after_move(&units, &map, b, 8, 8, 0);
  if ((units_get(&units, b)->col1_vis_mask & 1u) == 0) {
    fprintf(stderr, "fog: pre-move owner bit missing\n");
    return 1;
  }
  /*
   * port_bugs.md: an Indian Convert must carry its own sprite on the map, not the
   * plain Free Colonist one — DOS FUN_112b_0060 routes every @UNIT type 0
   * through the profession table (FUN_112b_0002 case 0x1b -> icon 0x43 =
   * sprite 66 here), the same icon the colony screen already used.
   */
  {
    ColonizeUnitPool cpool;
    memset(&cpool, 0, sizeof(cpool));
    units_reset(&cpool);
    units_set_occupancy_map(NULL);
    cpool.type_count = 3;
    snprintf(cpool.types[0].name, sizeof(cpool.types[0].name), "Colonists");
    cpool.types[0].movement = 1;
    cpool.types[0].icon_sprite = 100; /* Free Colonist */
    /* port_bugs.md #591: the name channel is the @UNIT row, so the fixture needs
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

    /* port_bugs.md #263: a mounted Veteran Soldier (profession 0x15) draws with the
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
    /* port_bugs.md #591: the NAME channel is the @UNIT row (a mounted armed body
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
     * port_bugs.md #420: FUN_112b_0060's tail downgrades a commissioned missionary
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

/* FUN_6662_0906 raw 104185 leaves Soldier in DS:0x1dd2. The far goto
 * flood therefore ignores terrain costs after a coarse snap; a near goto
 * still uses the Dragoon's own type. Traced in the 1548 idle campaign. */
static int unit_far_flood_keeps_probe_type(void) {
  units_reset_state();
  ColonizeUnitPool pool = {0};
  pool.type_count = 3;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  pool.types[2].movement = 3;
  pool.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map = {0};
  char err[128];
  if (!map_alloc(&map, 24, 24, err, sizeof(err))) {
    return 1;
  }
  for (int i = 0; i < 24 * 24; ++i) {
    map.terrain[i] = 2;
    map.layer3[i] = 0xf1;
  }
  map.terrain[9 * 24 + 9] = 27; /* mountains on the direct diagonal */
  const int id = units_spawn_allow_stack(&pool, 2, 8, 8);
  ColonizeUnit* u = units_get(&pool, id);
  if (!u) {
    map_free(&map);
    return 1;
  }
  u->nation_id = 2;
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = 9;
  u->goto_y = 20;
  ColonizeWorld w = {.units = &pool, .map = &map};
  int nx = -1, ny = -1;
  int rc = 0;
  if (!units_next_goto_step_w(&w, id, &nx, &ny) || nx != 9 || ny != 9) {
    fprintf(stderr, "probe_type: far flood should use Soldier costs, got (%d,%d)\n", nx, ny);
    rc = 1;
  }
  units_reset_state();
  u->goto_y = 12;
  if (!units_next_goto_step_w(&w, id, &nx, &ny) || nx != 8 || ny != 9) {
    fprintf(stderr, "probe_type: near flood should use Dragoon costs, got (%d,%d)\n", nx, ny);
    rc = 1;
  }
  map_free(&map);
  units_reset_state();
  return rc;
}

/*
 * port_bugs.md #1042: FUN_6662_0906 as the FUN_4962_0018 ship-pressure probe calls
 * it (cap 8, Caravel costs, shared 00f2 grid). One ocean tile from the
 * colony costs 1 + 3 = 4, two cost 7; a second probe toward the same colony
 * from a tile the first flood already priced reuses the grid and answers 0.
 */
static int unit_ship_pressure_0906_cost(void) {
  units_reset_state();
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    fprintf(stderr, "ship0906: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = 25; /* ocean */
    map.layer3[i] = 0xf1;
  }
  map.terrain[8 * 16 + 8] = 2; /* colony square, nation 0 */
  map.layer2[8 * 16 + 8] = MAP_OCCUPANCY_HAS_CITY;
  map.layer3[8 * 16 + 8] = 0x01;
  int rc = 0;
  const int one = units_short_sea_route_cost(&pool, &map, 9, 8, 8, 8);
  const int reused = units_short_sea_route_cost(&pool, &map, 10, 8, 8, 8);
  units_reset_state();
  const int two = units_short_sea_route_cost(&pool, &map, 10, 8, 8, 8);
  if (one != 4 || reused != 0 || two != 7) {
    fprintf(stderr, "ship0906: want 4/0/7, got %d/%d/%d\n", one, reused, two);
    rc = 1;
  }
  map_free(&map);
  return rc;
}

/*
 * port_bugs.md #696 / #697 / #701 — FUN_479b_0972 + FUN_6662_0f74 tails.
 *  #701 the adjacent tier is FUN_6662_0086, a bare sign->dir8 lookup: it
 *       returns the sign step even onto a tile the unit cannot enter.
 *  #696 a pathfinder miss spends the whole allotment (FUN_281f_0934) and
 *       clears +0x314c; #1444: TRADE_ROUTE too (0f74 never returns 8).
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
  /* #1444: 0f74 never returns 8, so TRADE_ROUTE is cleared on a miss too. */
  if (rc == 0) {
    const int trader = units_spawn_allow_stack(&pool, 0, 2, 2);
    u = units_get(&pool, trader);
    if (u) {
      u->nation_id = 0;
      u->moves = UNITS_MP_PER_TILE;
      u->orders = UNITS_ORDER_TRADE_ROUTE;
      u->goto_x = 13;
      u->goto_y = 13;
      if (units_advance_goto_one_step_w(&w, trader) || u->orders != UNITS_ORDER_NONE ||
          u->moves != 0) {
        fprintf(stderr, "goto_tails: #1444 trade-route miss must clear + spend, orders=%d\n",
                u->orders);
        rc = 1;
      }
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
 * port_bugs.md: waking a loaded unit out of a ship's hold (tile-stack picker /
 * ORDERS Activate Unit) has to leave it able to walk ashore even when the
 * ship itself has no moves left. Boarding parks the passenger at moves
 * 0 as a skip-select flag, so the wake has to restore its allotment.
 */
static int unit_wake_passenger_can_land(void) {
  ColonizeUnitPool pool;
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
 * port_bugs.md #1440: stepping off the ship is a FUN_465b_0000 step, so it has
 * the allow gate (raw 75649-75651 / 75825-75827): a passenger short of the
 * cost that loses the 04d4 roll stays aboard, charged (spent); one with its
 * full allotment always lands, no roll.
 */
static int unit_landfall_mp_gate(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].cargo = 2;
  pool.types[1].space = 1;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "landfall_gate: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = (i % 8 == 4) ? 1 : 25; /* land column x=4, ocean elsewhere */
  }
  const int ship = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int pax = units_spawn_allow_stack(&pool, 1, 4, 3);
  int rc = 0;
  if (!units_get(&pool, ship) || !units_get(&pool, pax)) {
    fprintf(stderr, "landfall_gate: spawn failed\n");
    rc = 1;
  }
  if (rc == 0) {
    units_get(&pool, ship)->nation_id = 0;
    units_get(&pool, pax)->nation_id = 0;
    units_get(&pool, pax)->moves = UNITS_MP_PER_TILE;
    if (!units_board(&pool, pax, ship)) {
      fprintf(stderr, "landfall_gate: board failed\n");
      rc = 1;
    }
  }
  const int cost = map_move_spent_thirds(&map, 3, 3, 4, 3);
  if (rc == 0 && cost <= 1) {
    fprintf(stderr, "landfall_gate: fixture needs a step cost > 1 third (got %d)\n", cost);
    rc = 1;
  }
  /* A 04ca word whose 04d4(1, cost) misses 1 third left. */
  uint32_t word = 0;
  for (uint32_t s = 1; s < 0x8000u && word == 0; ++s) { /* tiny seeds draw low */
    ColonizeDosRng probe;
    dos_rng_seed(&probe, s);
    if (dos_rng_range(&probe, 1, cost) > 1) {
      word = s;
    }
  }
  if (rc == 0 && word == 0) {
    fprintf(stderr, "landfall_gate: no reseed word misses 1 third\n");
    rc = 1;
  }
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 0);
  const ColonizeWorld w = {
    .units = &pool, .map = &map, .rng = &rng, .rng_reseed = word, .rng_reseed_set = true
  };
  ColonizeUnit* pu = units_get(&pool, pax);
  if (rc == 0) {
    pu->moves = 0;
    pu->aboard_moves = 1;
    pu->mp_spent_turn = 0;
    if (units_unload_passenger_w(&w, ship, pax, 4, 3)) {
      fprintf(stderr, "landfall_gate: a lost roll must refuse the step\n");
      rc = 1;
    } else if (pu->aboard_ship_id != ship || pu->moves != 0 || pu->aboard_moves != 0 ||
               !pu->mp_spent_turn || units_last_enter_reason() != COLONIZE_ENTER_NO_MP) {
      fprintf(stderr, "landfall_gate: refused pax must stay aboard, charged (aboard=%d mp=%d am=%d)\n",
              pu->aboard_ship_id, pu->moves, pu->aboard_moves);
      rc = 1;
    }
  }
  /* Next turn (turn.c refresh): full allotment, cost <= left, no roll. */
  if (rc == 0) {
    pu->aboard_moves = -1;
    pu->mp_spent_turn = 0;
    const uint32_t before = rng.state;
    if (!units_unload_passenger_w(&w, ship, pax, 4, 3) || pu->aboard_ship_id >= 0 ||
        pu->x != 4 || pu->y != 3) {
      fprintf(stderr, "landfall_gate: a full-allotment pax must land\n");
      rc = 1;
    } else if (rng.state != before) {
      fprintf(stderr, "landfall_gate: a full-allotment pax must not roll\n");
      rc = 1;
    }
  }
  map_free(&map);
  return rc;
}

/* Port-only Shift-pick: Shift+click marks rows with moves left, letting go of
 * Shift closes with the marked ids packed at the front. */
static int unit_stack_shift_pick_group(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  int ids[3];
  for (int i = 0; i < 3; ++i) {
    ids[i] = units_spawn_allow_stack(&pool, 0, 3, 3);
    units_get(&pool, ids[i])->nation_id = 0;
  }
  UnitStackPopup dlg;
  memset(&dlg, 0, sizeof(dlg));
  if (!unit_stack_try_open(&dlg, &pool, 3, 3, 0) || dlg.count != 3) {
    fprintf(stderr, "shift_pick: popup should open with three units\n");
    return 1;
  }
  units_get(&pool, dlg.ids[1])->moves = 0; /* spent: not pickable */
  dlg.dialog_w = dlg.dialog_h = 100;
  dlg.line_h = 16;
  dlg.col_w = 100;
  const int want0 = dlg.ids[0];
  const int want2 = dlg.ids[2];
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.shift_held = true;
  in.mouse_left_clicked = true;
  for (int row = 0; row < 3; ++row) {
    in.mouse_y = 16 * row + 8;
    unit_stack_handle_input(&dlg, &pool, &in, NULL);
  }
  in.mouse_left_clicked = false;
  unit_stack_handle_input(&dlg, &pool, &in, NULL);
  if (!dlg.open || dlg.group_count != 0) {
    fprintf(stderr, "shift_pick: stays open while Shift is held\n");
    return 1;
  }
  in.shift_held = false;
  int sel = -1;
  unit_stack_handle_input(&dlg, &pool, &in, &sel);
  if (dlg.open || dlg.group_count != 2 || dlg.ids[0] != want0 || dlg.ids[1] != want2 ||
      sel != -1) {
    fprintf(stderr, "shift_pick: release must close with the two units that have moves\n");
    return 1;
  }
  fprintf(stderr, "unit_units: stack picker Shift group ok\n");
  return 0;
}

/*
 * port_bugs.md: the tile-stack picker must work like DOS FUN_2b5a_1b5a — ONE pick
 * wakes a sentried passenger AND hands it back for selection, closing the
 * dialog. The old two-step (first click only woke, dialog stayed open with
 * no selection) read as "impossible to wake up unit loaded onto a ship
 * without moves".
 */
static int unit_stack_one_click_wakes_and_selects(void) {
  ColonizeUnitPool pool;
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
  /* port_bugs.md two-step: the first pick on an ordered row only cancels its
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
  /* An automated unit with no real order: the first pick cancels Automate. */
  pu->automated = true;
  (void)unit_stack_try_open(&dlg, &pool, 3, 3, 0);
  dlg.selection = (dlg.ids[0] == pax) ? 0 : 1;
  sel = -1;
  unit_stack_handle_input(&dlg, &pool, &in, &sel);
  if (sel != -1 || pu->automated) {
    fprintf(stderr, "stack_pick: first pick must cancel Automate (sel=%d)\n", sel);
    return 1;
  }
  fprintf(stderr, "unit_units: stack picker two-step cancel+activate ok\n");
  return 0;
}


/*
 * port_bugs.md #720 / #719 / #714 — MP model + entry gating (docs/move_enter.md).
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
/*
 * port_bugs.md #1437 (supersedes #791) / #792 — DOS-LITERAL FUN_465b_0000 raw
 * 75721: the colony-enter arm (MP exhaust, col1_counter16 reset, arriving-group
 * sentry wake) is gated on `FUN_281f_088a(unit) || type == 0x0c`. 088a walks
 * the mover's own chain after FUN_281f_0916 pulled it off the origin tile, so
 * the gate is "mover is a hull || mover is a wagon". A land unit stepping in
 * from a tile that holds a ship keeps its MP (minus the step); a ship and a
 * wagon are exhausted. Docking leaves Sentry units already in the colony asleep.
 */
static int unit_colony_enter_hull_stack_gate_791_792(void) {
  int rc = 0;
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 4;
  pool.types[3].kind_plus1 = UNITS_KIND_WAGON + 1;
  pool.types[3].movement = 2;
  pool.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Colonists");
  pool.types[0].movement = 2;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Soldiers");
  pool.types[1].movement = 1;
  pool.types[1].attack = 2;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(pool.types[2].name, sizeof(pool.types[2].name), "Caravel");
  pool.types[2].movement = 4;
  pool.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "colony_enter_gate: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 1; /* plains */
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(&map);
  colonies_set_occupancy_map(&map);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 0; /* own colony: no foreign bump */
  col->x = 4;
  col->y = 3;
  colonies.colony_count = 1;

  /* #1437: colonist leaves a tile that ALSO holds a ship -> keeps its MP
   * (only the step is charged) and col1_counter16 is untouched. */
  const int hull_id = units_spawn_allow_stack(&pool, 2, 3, 3);
  const int mover_id = units_spawn_allow_stack(&pool, 0, 3, 3);
  ColonizeUnit* hull = units_get(&pool, hull_id);
  ColonizeUnit* mover = units_get(&pool, mover_id);
  if (!hull || !mover) {
    map_free(&map);
    fprintf(stderr, "colony_enter_gate: spawn failed\n");
    return 1;
  }
  hull->nation_id = 0;
  mover->nation_id = 0;
  mover->moves = 6;
  mover->col1_counter16 = 7;
  if (!units_try_move_w(
        &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map}, mover_id, 4, 3
      )) {
    fprintf(stderr, "colony_enter_gate: colonist-with-hull move refused\n");
    rc = 1;
  }
  mover = units_get(&pool, mover_id);
  if (rc == 0 && mover->moves == 0) {
    fprintf(stderr, "colony_enter_gate #1437: land unit from a ship tile must keep MP\n");
    rc = 1;
  }
  if (rc == 0 && mover->col1_counter16 != 7) {
    fprintf(stderr, "colony_enter_gate #1437: land unit's col1_counter16 must be untouched\n");
    rc = 1;
  }

  /* Wagon (type 0x0c): exhausted on entry, col1_counter16 reset (#792). */
  const int wagon_id = units_spawn_allow_stack(&pool, 3, 3, 4);
  ColonizeUnit* wagon = units_get(&pool, wagon_id);
  if (wagon) {
    wagon->nation_id = 0;
    wagon->moves = 6;
    wagon->col1_counter16 = 7;
    if (!units_try_move_w(
          &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map}, wagon_id, 4, 3
        )) {
      fprintf(stderr, "colony_enter_gate: wagon move refused\n");
      rc = 1;
    }
    wagon = units_get(&pool, wagon_id);
    if (rc == 0 && (wagon->moves != 0 || wagon->col1_counter16 != 0)) {
      fprintf(stderr, "colony_enter_gate: wagon must be exhausted + reset, moves=%d c16=%d\n",
              wagon->moves, wagon->col1_counter16);
      rc = 1;
    }
  }

  /* Control: a plain colonist leaving a tile with NO hull is not exhausted. */
  const int mover2_id = units_spawn_allow_stack(&pool, 0, 3, 5);
  ColonizeUnit* mover2 = units_get(&pool, mover2_id);
  if (mover2) {
    mover2->nation_id = 0;
    mover2->moves = 6;
    if (!units_try_move_w(
          &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map}, mover2_id, 4, 5
        )) {
      fprintf(stderr, "colony_enter_gate: plain colonist move refused\n");
      rc = 1;
    }
    mover2 = units_get(&pool, mover2_id);
    if (rc == 0 && mover2->moves == 0) {
      fprintf(stderr, "colony_enter_gate #791: no-hull colonist must NOT be exhausted\n");
      rc = 1;
    }
  }

  /* FUN_465b_0000 raw 75735-75741: docking wakes only the arriving group; a
   * Sentry unit already sitting in the colony stays sentried. */
  const int sentry_id = units_spawn_allow_stack(&pool, 1, 4, 3);
  ColonizeUnit* sentry = units_get(&pool, sentry_id);
  if (sentry) {
    sentry->nation_id = 0;
    sentry->orders = UNITS_ORDER_SENTRY;
  }
  const int mover3_id = units_spawn_allow_stack(&pool, 2, 3, 3);
  ColonizeUnit* mover3 = units_get(&pool, mover3_id);
  if (mover3) {
    mover3->nation_id = 0;
    mover3->moves = 12;
    if (!units_try_move_w(
          &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map}, mover3_id, 4, 3
        )) {
      fprintf(stderr, "colony_enter_gate: hull docking refused\n");
      rc = 1;
    }
    sentry = units_get(&pool, sentry_id);
    if (rc == 0 && sentry && sentry->orders != UNITS_ORDER_SENTRY) {
      fprintf(stderr, "colony_enter_gate: resident sentry woken by docking\n");
      rc = 1;
    }
    mover3 = units_get(&pool, mover3_id);
    if (rc == 0 && mover3 && mover3->moves != 0) {
      fprintf(stderr, "colony_enter_gate: docking hull must be exhausted, moves=%d\n",
              mover3->moves);
      rc = 1;
    }
  }

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: colony-enter hull/wagon gate (#1437/#792) ok\n");
  }
  return rc;
}

/*
 * port_bugs.md #1057a (reversed 2026-10-06): FUN_4720_049e raw 76126-76135 is the
 * human UI move handler (it moves DS:0x5392); AI moves go through
 * FUN_465b_0000 and never set player+0x30 bit 0x80. The seed-100 idle
 * campaign keeps every AI bit clear for 107 years of AI landfalls. The
 * human's bit belongs to the @LANDHO naming dialog, so the mover sets none.
 */
static int unit_ai_move_marks_new_world(void) {
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[1].control = 1;
  col1.player[0].control = 0;
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Caravel");
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  map.terrain[3 * 8 + 6] = 1; /* one land tile at (6,3) */
  units_set_occupancy_map(&map);
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  const ColonizeWorld w = {.units = &pool, .colonies = &colonies, .map = &map, .col1 = &col1};
  int rc = 0;
  const int ai = units_spawn_allow_stack(&pool, 0, 2, 3);
  const int hu = units_spawn_allow_stack(&pool, 0, 2, 5);
  units_get(&pool, ai)->nation_id = 1;
  units_get(&pool, ai)->moves = 12;
  units_get(&pool, hu)->nation_id = 0;
  units_get(&pool, hu)->moves = 12;
  if (!units_try_move_w(&w, ai, 3, 3) || col1.player[1].named_new_world) {
    fprintf(stderr, "new_world: open-sea move must not set the bit\n");
    rc = 1;
  }
  if (rc == 0 && (!units_try_move_w(&w, ai, 4, 3) || col1.player[1].named_new_world)) {
    fprintf(stderr, "new_world: land two tiles off must not set the bit\n");
    rc = 1;
  }
  if (rc == 0 && (!units_try_move_w(&w, ai, 5, 3) || col1.player[1].named_new_world)) {
    fprintf(stderr, "new_world: AI move next to land must not set the bit\n");
    rc = 1;
  }
  if (rc == 0 && (!units_try_move_w(&w, hu, 3, 4) || !units_try_move_w(&w, hu, 4, 4) ||
                  !units_try_move_w(&w, hu, 5, 4) || col1.player[0].named_new_world)) {
    fprintf(stderr, "new_world: human bit is the @LANDHO dialog's, not the mover's\n");
    rc = 1;
  }
  units_set_occupancy_map(NULL);
  map_free(&map);
  return rc;
}

/* Manifest restore skips the capacity gate but must still stop at the
 * cargo_ids[] array, or the 7th passenger overwrites the next unit. */
static int unit_board_restore_bounded(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].cargo = 2;
  pool.types[1].space = 1;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  const int ship = units_spawn_allow_stack(&pool, 0, 3, 3);
  int boarded = 0;
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX + 3; ++i) {
    const int pax = units_spawn_allow_stack(&pool, 1, 3, 3);
    boarded += units_board_stacked_restore(&pool, pax, ship) ? 1 : 0;
  }
  const ColonizeUnit* su = units_get_const(&pool, ship);
  if (!su || boarded != COLONIZE_UNIT_CARGO_MAX || su->cargo_count != COLONIZE_UNIT_CARGO_MAX) {
    fprintf(stderr, "board_restore: boarded %d, cargo_count %d\n", boarded, su ? su->cargo_count : -1);
    return 1;
  }
  return 0;
}

/* port_bugs.md #1107 seam: units_try_move_w's stages are callable on a hand-built
 * units_move_ctx (COLONIZE_TESTING). validate = the port pre-checks, denied =
 * the no-fight probe aborts. */
static int unit_move_stage_seam_1107(void) {
  units_set_occupancy_map(NULL); /* an earlier case leaves it on its stack map */
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "seam_1107: map_alloc failed: %s\n", err);
    return 1;
  }
  const int id = units_spawn_allow_stack(&pool, 0, 3, 3);
  ColonizeUnit* u = units_get(&pool, id);
  const ColonizeWorld w = {.units = &pool, .map = &map};
  units_move_ctx c = {
    .w = &w, .pool = &pool, .map = &map, .unit_id = id, .dest_x = 4, .dest_y = 3,
    .village_temp = -1, .village_nation = -1, .foe = -1,
  };
  int rc = 0;
  u->moves = 0;
  if (units_move_validate(&c) != UNITS_MOVE_RETURN_FALSE ||
      g_units_last_enter_reason != COLONIZE_ENTER_NO_MP) {
    fprintf(stderr, "seam_1107: no-MP unit must stop at validate with NO_MP\n");
    rc = 1;
  }
  u->moves = UNITS_MP_PER_TILE;
  c.dest_x = 5; /* two tiles away */
  if (units_move_validate(&c) != UNITS_MOVE_RETURN_FALSE) {
    fprintf(stderr, "seam_1107: non-adjacent dest must stop at validate\n");
    rc = 1;
  }
  c.dest_x = 4;
  if (units_move_validate(&c) != UNITS_MOVE_CONTINUE || c.unit != u) {
    fprintf(stderr, "seam_1107: adjacent move must pass validate and bind c.unit\n");
    rc = 1;
  }
  c.reason = COLONIZE_ENTER_LANDFALL;
  if (units_move_denied(&c) != UNITS_MOVE_RETURN_FALSE) {
    fprintf(stderr, "seam_1107: LANDFALL must abort at denied\n");
    rc = 1;
  }
  c.reason = COLONIZE_ENTER_OK;
  if (units_move_denied(&c) != UNITS_MOVE_CONTINUE) {
    fprintf(stderr, "seam_1107: OK must pass denied\n");
    rc = 1;
  }
  map_free(&map);
  return rc;
}

/*
 * TB-44 fixed path: the 00f2 grid is keyed on the mover's flood rules too.
 * A uniform-cost flood (the 67f4 populator's DS:0x1dd4 = 1) toward (8,14)
 * is not reused by a real-cost flood toward the same goal: DOS (flag on)
 * reuses it and steps S (3); the fixed path rebuilds and, like a fresh
 * real-cost flood, finds no step under cap 0x12 past the mountains.
 */
static int unit_tb44_flood_key_rules(void) {
  ColonizeUnitPool pool = {0};
  pool.type_count = 2;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map = {0};
  char err[128];
  if (!map_alloc(&map, 24, 24, err, sizeof err)) {
    return 1;
  }
  for (int i = 0; i < 24 * 24; ++i) {
    map.terrain[i] = 2;
    map.layer3[i] = 0xf1;
  }
  map.terrain[9 * 24 + 9] = 27;
  map.terrain[10 * 24 + 9] = 27;
  map.terrain[10 * 24 + 10] = 27;
  ColonizeWorld w = {.units = &pool, .map = &map};
  int got[2];
  for (int k = 0; k < 2; ++k) {
    dos_buggy_behavior_set(k == 0);
    units_reset_state();
    (void)units_flood_step_dir(&w, 8, 8, 8, 14, 1, -1, 0x12, true);
    got[k] = units_flood_step_dir(&w, 8, 8, 8, 14, 1, -1, 0x12, false);
  }
  dos_buggy_behavior_set(true);
  units_reset_state();
  map_free(&map);
  if (got[0] != 3 || got[1] != -1) {
    fprintf(stderr, "TB-44: buggy %d (want 3) fixed %d (want -1)\n", got[0], got[1]);
    return 1;
  }
  return 0;
}

/*
 * TB-1 fixed path: the live coarse planes flood every link fresh. With the
 * 2x2 sample block of coarse cell (3,3) under water, cell (3,4)'s NE test
 * reuses cell (3,2)'s SE flood in DOS and the NE link is dropped, so a far
 * Go To from (14,17) toward (30,1) steps E to (15,16); with the link kept
 * the waypoint is the NE cell and the step is N to (14,16). Two live maps
 * so each call rebuilds the coarse grid for its own mode.
 */
static int unit_tb1_coarse_ne_link(void) {
  ColonizeUnitPool pool = {0};
  pool.type_count = 2;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap m[2];
  char err[128];
  for (int k = 0; k < 2; ++k) {
    memset(&m[k], 0, sizeof m[k]);
    if (!map_alloc(&m[k], 44, 44, err, sizeof err)) {
      return 1;
    }
    for (int i = 0; i < 44 * 44; ++i) {
      m[k].terrain[i] = 2;
      m[k].layer3[i] = 0xf1;
    }
    for (int x = 13; x <= 14; ++x) {
      for (int y = 13; y <= 14; ++y) {
        m[k].terrain[y * 44 + x] = 25;
      }
    }
  }
  const int id = units_spawn_allow_stack(&pool, 1, 14, 17);
  ColonizeUnit* u = units_get(&pool, id);
  int st[2][2] = {{-1, -1}, {-1, -1}};
  for (int k = 0; u && k < 2; ++k) {
    dos_buggy_behavior_set(k == 0);
    units_reset_state();
    u->nation_id = 2;
    u->x = 14;
    u->y = 17;
    u->orders = UNITS_ORDER_GOTO;
    u->goto_x = 30;
    u->goto_y = 1;
    ColonizeWorld w = {.units = &pool, .map = &m[k]};
    (void)units_next_goto_step_w(&w, id, &st[k][0], &st[k][1]);
  }
  dos_buggy_behavior_set(true);
  units_reset_state();
  map_free(&m[0]);
  map_free(&m[1]);
  if (st[0][0] != 15 || st[0][1] != 16 || st[1][0] != 14 || st[1][1] != 16) {
    fprintf(stderr, "TB-1: buggy (%d,%d) want (15,16), fixed (%d,%d) want (14,16)\n", st[0][0],
            st[0][1], st[1][0], st[1][1]);
    return 1;
  }
  return 0;
}

/*
 * DB-81 / port_bugs.md #1576: campaign 21 Caravel, Go To (33,30) from (12,23)
 * next to the land block of x12-16 y21-23 (save terrain copied). Coarse cell
 * (3,5) has no link to (3,6), so its waypoint flood steps NW to (11,22) in
 * cell (2,5), whose waypoint (3,6) floods back to (12,23). DOS alternates
 * forever; the port takes the whole-map BFS step when the far flood undoes the
 * last step, and arrives.
 */
static int unit_db81_far_waypoint_pingpong(void) {
  static const struct { int y, x0, x1; } land[] = {
    {18, 8, 8}, {19, 8, 9}, {20, 17, 18}, {20, 21, 22}, {21, 12, 13}, {21, 16, 23},
    {22, 12, 16}, {22, 18, 23}, {23, 13, 15}, {23, 19, 23}, {24, 14, 14}, {24, 20, 20},
    {24, 22, 23}, {25, 23, 23},
  };
  ColonizeUnitPool pool = {0};
  pool.type_count = 2;
  pool.types[1].movement = 4;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  ColonizeWorldMap m;
  char err[128];
  memset(&m, 0, sizeof m);
  if (!map_alloc(&m, 58, 72, err, sizeof err)) {
    return 1;
  }
  for (int i = 0; i < 58 * 72; ++i) {
    m.terrain[i] = 25;
    m.layer3[i] = 0xf1;
  }
  for (size_t r = 0; r < sizeof land / sizeof land[0]; ++r) {
    for (int x = land[r].x0; x <= land[r].x1; ++x) {
      m.terrain[land[r].y * 58 + x] = 2;
    }
  }
  units_reset_state();
  const int id = units_spawn_allow_stack(&pool, 1, 12, 23);
  ColonizeUnit* u = units_get(&pool, id);
  int steps = 0;
  if (u) {
    u->nation_id = 2;
    u->orders = UNITS_ORDER_GOTO;
    u->goto_x = 33;
    u->goto_y = 30;
    ColonizeWorld w = {.units = &pool, .map = &m};
    for (; steps < 60 && (u->x != 33 || u->y != 30); ++steps) {
      int nx = -1;
      int ny = -1;
      if (!units_next_goto_step_w(&w, id, &nx, &ny)) {
        break;
      }
      u->x = (uint8_t)nx;
      u->y = (uint8_t)ny;
    }
  }
  const bool arrived = u && u->x == 33 && u->y == 30;
  units_reset_state();
  map_free(&m);
  if (!arrived) {
    fprintf(stderr, "DB-81: after %d steps at (%d,%d), want (33,30)\n", steps, u ? u->x : -1,
            u ? u->y : -1);
    return 1;
  }
  return 0;
}

/*
 * port_bugs.md #1323: FUN_465b_0000 LAB_465b_05ca raw 75643-75649. A move into
 * an undefended foreign colony is a bVar4 arm for a human mover too: a mover
 * short of the step cost with something spent reseeds (04ca DS:0x83a6) before
 * the fight; a fresh mover does not.
 */
static int unit_colony_entry_bvar4_reseed_1323(void) {
  units_set_occupancy_map(NULL);
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "reseed_1323: map_alloc failed: %s\n", err);
    return 1;
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = 1;
  col->x = 4;
  col->y = 3;
  colonies.colony_count = 1;
  units_set_combat_human_nation(0);
  const int id = units_spawn_allow_stack(&pool, 0, 3, 3);
  ColonizeUnit* u = units_get(&pool, id);
  units_set_nation(u, 0);
  ColonizeDosRng rng;
  ColonizeWorld w = {.units = &pool, .map = &map, .colonies = &colonies, .rng = &rng};
  w.rng_reseed = 1234;
  w.rng_reseed_set = true;
  int rc = 0;
  for (int pass = 0; pass < 2; ++pass) {
    u->moves = pass == 0 ? 1 : UNITS_MP_PER_TILE; /* short + spent, then fresh */
    dos_rng_seed(&rng, 77);
    units_move_ctx c = {
      .w = &w, .pool = &pool, .map = &map, .colonies = &colonies, .rng = &rng,
      .unit_id = id, .unit = u, .dest_x = 4, .dest_y = 3, .reason = COLONIZE_ENTER_OK,
      .village_temp = -1, .village_nation = -1, .foe = -1,
    };
    if (units_move_cost(&pool, id, &map, 4, 3) <= 1) {
      fprintf(stderr, "reseed_1323: fixture step cost must exceed 1\n");
      rc = 1;
      break;
    }
    (void)units_move_colony_entry(&c);
    ColonizeDosRng want;
    dos_rng_seed(&want, pass == 0 ? 1234 : 77);
    if (rng.state != want.state) {
      fprintf(stderr, "reseed_1323: pass %d rng state %u want %u\n", pass,
              (unsigned)rng.state, (unsigned)want.state);
      rc = 1;
    }
  }
  map_free(&map);
  return rc;
}

/*
 * FUN_465b_0000 raw 75505-75508 and 75527-75595: a computer Euro mover
 * entering a foreign colony with no armed defender takes the same entry head
 * as an attack on a unit. Under 3 thirds left it stands down (spent, no band,
 * no 0673 vis OR: #1438);
 * with full moves the relation band runs on the settlement owner (PEACE
 * cleared both ways). Both the plain entry (units_move_colony_entry) and the
 * combat-reason entry (units_move_combat_no_foe) are covered. The move gate
 * reads only the attacker's PEACE bit (raw 75545-75557), never WAR.
 */
static int unit_ai_undefended_colony_band(void) {
  units_set_occupancy_map(NULL);
  static ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  pool.types[0].movement = 1;
  pool.types[0].attack = 2;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "undefended_band: map_alloc failed: %s\n", err);
    return 1;
  }
  map.layer3[3 * 8 + 4] = 0x01; /* colony tile owner nibble 0: 07fe would OR bit 0 */
  static ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = 0;
  col->x = 4;
  col->y = 3;
  colonies.colony_count = 1;
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[0].control = 0;
  col1.player[1].control = 1;
  units_set_combat_human_nation(0);
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 77);
  ColonizeWorld w = {.units = &pool, .map = &map, .colonies = &colonies, .rng = &rng};
  int rc = 0;
  for (int pass = 0; pass < 4 && rc == 0; ++pass) {
    const bool short_mp = (pass & 1) == 0;
    const bool no_foe = pass >= 2;
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
    /* A fresh mover each pass: the full-MP passes fight the militia. */
    const int id = units_spawn_allow_stack(&pool, 0, 3, 3);
    ColonizeUnit* u = units_get(&pool, id);
    units_set_nation(u, 1);
    u->moves = short_mp ? 2 : UNITS_MP_PER_TILE;
    u->col1_vis_mask = 0;
    units_move_ctx c = {
      .w = &w, .pool = &pool, .map = &map, .colonies = &colonies, .rng = &rng,
      .col1 = &col1, .unit_id = id, .unit = u, .dest_x = 4, .dest_y = 3,
      .reason = no_foe ? COLONIZE_ENTER_COMBAT_LAND : COLONIZE_ENTER_OK,
      .village_temp = -1, .village_nation = -1, .foe = -1,
    };
    const UnitsMoveStatus st =
      no_foe ? units_move_combat_no_foe(&c) : units_move_colony_entry(&c);
    const bool peace_kept = (ai_diplo_read(&col1, 0, 1) & AI_DIPLO_PEACE) != 0 &&
                            (ai_diplo_read(&col1, 1, 0) & AI_DIPLO_PEACE) != 0;
    if (short_mp) {
      if (st != UNITS_MOVE_RETURN_FALSE || units_remaining_mp(&pool, id) != 0 || !peace_kept) {
        fprintf(stderr, "undefended_band: pass %d stand-down st=%d mp=%d peace=%d\n", pass,
                (int)st, units_remaining_mp(&pool, id), (int)peace_kept);
        rc = 1;
      }
      /* port_bugs.md #1438: 01ce exits before LAB_465b_0673's 07fe vis OR. */
      if (rc == 0 && (u->col1_vis_mask & 1u) != 0) {
        fprintf(stderr, "undefended_band: pass %d stand-down must not OR the owner bit\n", pass);
        rc = 1;
      }
    } else if (peace_kept) {
      fprintf(stderr, "undefended_band: pass %d band must clear PEACE both ways\n", pass);
      rc = 1;
    }
  }
  /* Move gate: computer under PEACE with a running cooldown aborts even
   * when WAR is also set; a human under PEACE|WAR is not let through here.
   * The @HAVETREATY prompt itself is unit_stage_seams
   * test_human_peace_war_asks_havetreaty. */
  if (rc == 0) {
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE | AI_DIPLO_WAR);
    col1.nation[1].treaty_timer[0] = 3;
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE | AI_DIPLO_WAR);
    if (units_at_war_for_move(&col1, 1, 0) || units_at_war_for_move(&col1, 0, 1)) {
      fprintf(stderr, "undefended_band: move gate must not read WAR under PEACE\n");
      rc = 1;
    }
    col1.nation[1].treaty_timer[0] = 0;
    if (rc == 0 && !units_at_war_for_move(&col1, 1, 0)) {
      fprintf(stderr, "undefended_band: expired cooldown must let the computer attack\n");
      rc = 1;
    }
  }
  units_set_combat_human_nation(-1);
  map_free(&map);
  return rc;
}

/* port_bugs.md #1837 / #1838 on one ocean strip:
 * - the walk-on board is 465b's plain step, so bVar5 = true (raw 75829);
 * - a hull step re-sentries every rider (raw 75706-75710, asm 465b:080a). */
static int unit_465b_board_settles_hull_resentries(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].cargo = 2;
  pool.types[1].space = 1;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "board_settle: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = (i % 8 == 4) ? 1 : 25; /* land column x=4, ocean elsewhere */
    if (map.layer3) {
      map.layer3[i] = 1;
    }
  }
  units_set_occupancy_map(&map);
  const int own = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int other = units_spawn_allow_stack(&pool, 0, 1, 3);
  const int pax = units_spawn_allow_stack(&pool, 1, 4, 3);
  const ColonizeWorld w = {.units = &pool, .map = &map};
  int rc = 0;
  if (!units_get(&pool, own) || !units_get(&pool, other) || !units_get(&pool, pax)) {
    fprintf(stderr, "board_settle: spawn failed\n");
    rc = 1;
  }
  if (rc == 0) {
    units_get(&pool, own)->nation_id = 0;
    units_get(&pool, other)->nation_id = 0;
    units_get(&pool, pax)->nation_id = 0;
    units_get(&pool, pax)->moves = UNITS_MP_PER_TILE;
    g_units_465b_settled = false;
    if (!units_try_move_w(&w, pax, 3, 3) || units_get(&pool, pax)->aboard_ship_id != own) {
      fprintf(stderr, "board_settle: walk-on board refused\n");
      rc = 1;
    } else if (!g_units_465b_settled) {
      fprintf(stderr, "board_settle: a board must leave bVar5 set (no 0bd1 tail)\n");
      rc = 1;
    }
  }
  if (rc == 0) {
    units_get(&pool, pax)->orders = UNITS_ORDER_NONE; /* woken at sea */
    units_get(&pool, own)->moves = units_max_mp(&pool, own);
    if (!units_try_move_w(&w, own, 2, 3)) {
      fprintf(stderr, "board_settle: hull step refused\n");
      rc = 1;
    } else if (units_get(&pool, pax)->orders != UNITS_ORDER_SENTRY) {
      fprintf(stderr, "board_settle: hull step must re-sentry its rider (orders %d)\n",
              units_get(&pool, pax)->orders);
      rc = 1;
    }
  }
  units_set_occupancy_map(NULL);
  map_free(&map);
  return rc;
}

/* Boarding is FUN_465b_0000's plain step (raw 75711-75723), so its tail runs
 * FUN_5bfb_3180's sentry-wake arm (raw 98628-98646): a foreign sentried ship
 * beside the receiving ship wakes. */
static int unit_board_wakes_foreign_sentry(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].cargo = 2;
  pool.types[1].space = 1;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "board_wake: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = (i % 8 == 4) ? 1 : 25; /* land column x=4, ocean elsewhere */
    if (map.layer3) {
      map.layer3[i] = 1; /* ocean, not lake */
    }
  }
  units_set_occupancy_map(&map);
  const int own = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int foe = units_spawn_allow_stack(&pool, 0, 2, 3);
  const int pax = units_spawn_allow_stack(&pool, 1, 4, 3);
  int rc = 0;
  if (!units_get(&pool, own) || !units_get(&pool, foe) || !units_get(&pool, pax)) {
    fprintf(stderr, "board_wake: spawn failed\n");
    rc = 1;
  }
  const ColonizeWorld w = {.units = &pool, .map = &map};
  if (rc == 0) {
    units_get(&pool, own)->nation_id = 0;
    units_get(&pool, foe)->nation_id = 1;
    units_get(&pool, foe)->orders = UNITS_ORDER_SENTRY;
    units_get(&pool, pax)->nation_id = 0;
    units_get(&pool, pax)->moves = UNITS_MP_PER_TILE;
    if (!units_try_move_w(&w, pax, 3, 3) || units_get(&pool, pax)->aboard_ship_id != own) {
      fprintf(stderr, "board_wake: walk-on board refused\n");
      rc = 1;
    } else if (units_get(&pool, foe)->orders == UNITS_ORDER_SENTRY) {
      fprintf(stderr, "board_wake: a walk-on board must wake the foreign sentry\n");
      rc = 1;
    }
  }
  /* The ship-to-ship transfer is the same board step. */
  const int own2 = rc == 0 ? units_spawn_allow_stack(&pool, 0, 3, 4) : -1;
  if (rc == 0 && !units_get(&pool, own2)) {
    fprintf(stderr, "board_wake: second ship spawn failed\n");
    rc = 1;
  }
  if (rc == 0) {
    units_get(&pool, own2)->nation_id = 0;
    units_get(&pool, foe)->orders = UNITS_ORDER_SENTRY;
    units_get(&pool, pax)->orders = UNITS_ORDER_NONE;
    units_get(&pool, pax)->moves = UNITS_MP_PER_TILE;
    if (!units_transfer_passenger_w(&w, own, pax, 3, 4) ||
        units_get(&pool, pax)->aboard_ship_id != own2) {
      fprintf(stderr, "board_wake: ship-to-ship transfer refused\n");
      rc = 1;
    } else if (units_get(&pool, foe)->orders == UNITS_ORDER_SENTRY) {
      fprintf(stderr, "board_wake: a ship-to-ship transfer must wake the foreign sentry\n");
      rc = 1;
    }
  }
  units_set_occupancy_map(NULL);
  map_free(&map);
  return rc;
}

/*
 * port_bugs.md #1443: boarding is 465b's plain step, so its LAB_465b_05ca gate
 * reseeds (FUN_281f_04ca DS:0x83a6, raw 75649) before the raw 75825
 * range(1, cost) roll when the mover is short of the step cost with something
 * spent, exactly like the step gate (#842).
 */
static int unit_board_gate_reseed_1443(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].cargo = 2;
  pool.types[1].space = 1;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "board_reseed: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = (i % 8 == 4) ? 1 : 25; /* land column x=4, ocean elsewhere */
    map.layer3[i] = 1;
  }
  units_set_occupancy_map(&map);
  static ColonizeColonyPool colonies;
  colonies_init(&colonies);
  const int ship = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int pax = units_spawn_allow_stack(&pool, 1, 4, 3);
  ColonizeUnit* pu = units_get(&pool, pax);
  int rc = 0;
  if (!units_get(&pool, ship) || !pu) {
    fprintf(stderr, "board_reseed: spawn failed\n");
    rc = 1;
  }
  ColonizeDosRng rng;
  ColonizeWorld w = {.units = &pool, .map = &map, .colonies = &colonies, .rng = &rng};
  w.rng_reseed = 1234;
  w.rng_reseed_set = true;
  if (rc == 0) {
    units_get(&pool, ship)->nation_id = 0;
    pu->nation_id = 0;
    pu->moves = 1; /* short of the ocean step, something spent */
    const int cost = units_move_cost(&pool, pax, &map, 3, 3);
    if (cost <= 1) {
      fprintf(stderr, "board_reseed: fixture step cost must exceed 1\n");
      rc = 1;
    }
    dos_rng_seed(&rng, 77);
    units_move_ctx c = {
      .w = &w, .pool = &pool, .map = &map, .colonies = &colonies, .rng = &rng,
      .unit_id = pax, .unit = pu, .dest_x = 3, .dest_y = 3, .reason = COLONIZE_ENTER_BOARD,
      .village_temp = -1, .village_nation = -1, .foe = -1,
    };
    if (rc == 0) {
      (void)units_move_board(&c);
      ColonizeDosRng want;
      dos_rng_seed(&want, 1234);
      (void)dos_rng_range(&want, 1, cost);
      if (rng.state != want.state) {
        fprintf(stderr, "board_reseed: rng state %u want %u (reseed + one roll)\n",
                (unsigned)rng.state, (unsigned)want.state);
        rc = 1;
      }
    }
  }
  units_set_occupancy_map(NULL);
  map_free(&map);
  return rc;
}

/*
 * units_order_name: the @ORDERS byte (unit +0x314c, FUN_479b_0972's switch
 * key) to its debug-log name; port-only (no DOS text), unknown bytes = "?".
 */
static int unit_order_name_table(void) {
  static const struct {
    int orders;
    const char* name;
  } k_rows[] = {
    {UNITS_ORDER_NONE, "none"},          {UNITS_ORDER_SENTRY, "sentry"},
    {UNITS_ORDER_TRADE_ROUTE, "trade route"}, {UNITS_ORDER_GOTO, "goto"},
    {UNITS_ORDER_FORTIFY, "fortify"},    {UNITS_ORDER_FORTIFIED, "fortified"},
    {UNITS_ORDER_BUILD_COLONY, "build colony"}, {UNITS_ORDER_CLEAR_PLOW, "clear/plow"},
    {UNITS_ORDER_BUILD_ROAD, "build road"}, {UNITS_ORDER_AI_SAIL, "ai sail"},
    {UNITS_ORDER_AI_MOVE, "ai move"},    {UNITS_ORDER_FOLLOW, "follow"},
    {4, "?"}, {10, "?"}, {-1, "?"}, {255, "?"},
  };
  for (size_t i = 0; i < sizeof(k_rows) / sizeof(k_rows[0]); ++i) {
    const char* got = units_order_name(k_rows[i].orders);
    if (!got || strcmp(got, k_rows[i].name) != 0) {
      fprintf(stderr, "order name %d: want '%s' got '%s'\n", k_rows[i].orders, k_rows[i].name,
              got ? got : "(null)");
      return 1;
    }
  }
  return 0;
}

/*
 * Far-BFS fixture: 20x20 plains split by an ocean column x=10 open only at
 * y=1; a Colonist at (5,10) bound for (15,10) (far tier, |dx| = 10).
 */
static bool far_bfs_fixture_step(int* sx, int* sy) {
  units_set_occupancy_map(NULL);
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  units_reset(&pool);
  pool.type_count = 1;
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 20, 20, err, sizeof(err))) {
    return false;
  }
  for (int i = 0; i < 20 * 20; ++i) {
    map.terrain[i] = 2;
    map.layer3[i] = 0xf1;
  }
  for (int y = 0; y < 20; ++y) {
    if (y != 1) {
      map.terrain[y * 20 + 10] = 25;
    }
  }
  const int id = units_spawn_allow_stack(&pool, 0, 5, 10);
  ColonizeUnit* u = units_get(&pool, id);
  bool ok = false;
  if (u) {
    units_reset_state();
    u->nation_id = 0;
    u->orders = UNITS_ORDER_GOTO;
    u->goto_x = 15;
    u->goto_y = 10;
    ColonizeWorld w = {.units = &pool, .map = &map};
    ok = units_next_goto_step_w(&w, id, sx, sy);
  }
  map_free(&map);
  units_reset_state();
  return ok;
}

/*
 * units_bfs_next_step: the UNITS_FAR_BFS=1 diagnostic whole-map BFS tier of
 * the FUN_6662_0f74 goto director (port-only fallback, pre-2026-08-27). The
 * env switch is latched once per process, so the case re-execs itself with
 * UNITS_FAR_BFS=1 and COLONIZE_TEST_ONLY set; the parent runs the same
 * fixture on the default DOS tiers. The BFS first step heads up toward the
 * gap at (10,1): (4,9), the first-discovered tile of a shortest 8-way path
 * (dy = -1 row scanned dx = -1 first). Pinned 2026-11-06.
 */
static int unit_far_bfs_next_step(void) {
#if defined(_WIN32)
  return TR_SKIP;
#else
  int sx = -1;
  int sy = -1;
  const char* e = getenv("UNITS_FAR_BFS");
  if (e && e[0] == '1') {
    if (!far_bfs_fixture_step(&sx, &sy) || sx != 4 || sy != 9) {
      fprintf(stderr, "far BFS: step (%d,%d) want (4,9)\n", sx, sy);
      return 1;
    }
    return 0;
  }
  if (!far_bfs_fixture_step(&sx, &sy)) {
    fprintf(stderr, "far BFS: default tiers found no step\n");
    return 1;
  }
  if (sx == 4 && sy == 9) {
    fprintf(stderr, "far BFS: default tiers already step (4,9); fixture proves nothing\n");
    return 1;
  }
  fflush(NULL);
  const pid_t pid = fork();
  if (pid < 0) {
    return 1;
  }
  if (pid == 0) {
    setenv("UNITS_FAR_BFS", "1", 1);
    setenv("COLONIZE_TEST_ONLY", "unit_far_bfs_next_step", 1);
    unsetenv("COLONIZE_TEST_REVERSE");
    unsetenv("COLONIZE_TEST_SHUFFLE");
#ifdef __APPLE__
    char self[4096];
    uint32_t self_size = sizeof(self);
    if (_NSGetExecutablePath(self, &self_size) == 0) {
      execl(self, "unit_units_movement", (char*)NULL);
    }
#else
    execl("/proc/self/exe", "unit_units_movement", (char*)NULL);
#endif
    _exit(127);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "far BFS: UNITS_FAR_BFS=1 child failed (status 0x%x)\n", status);
    return 1;
  }
  return 0;
#endif
}

/* mods.h MOD_GOTO_NATIVE_WAIT: a human Go To whose goal holds a Brave stops
 * next to it with the order and its moves kept. Off: the step goes on to the
 * move/attack resolver as before. */
static int unit_goto_native_wait_mod(void) {
  units_reset_state();
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  for (int t = 0; t < 2; ++t) {
    pool.types[t].movement = 1;
    pool.types[t].attack = (uint8_t)(2 - t);
    pool.types[t].domain = COLONIZE_UNIT_DOMAIN_LAND;
  }
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 12, 12, err, sizeof(err))) {
    fprintf(stderr, "goto_native: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 12 * 12; ++i) {
    map.terrain[i] = 2; /* plains */
    map.layer3[i] = 0xf1;
  }
  const ColonizeWorld w = {.units = &pool, .colonies = NULL, .map = &map, .rng = NULL};
  const int sid = units_spawn(&pool, 0, 4, 4);
  const int bid = units_spawn(&pool, 1, 5, 4);
  ColonizeUnit* s = units_get(&pool, sid);
  ColonizeUnit* b = units_get(&pool, bid);
  if (!s || !b) {
    map_free(&map);
    fprintf(stderr, "goto_native: spawn failed\n");
    return 1;
  }
  s->nation_id = 0;
  b->nation_id = 6;
  /* Presence bits + owner nibbles, so the enter probe sees the Brave. */
  map.layer3[4 * 12 + 4] = 0x01;
  map.layer3[4 * 12 + 5] = 0x61;
  units_set_occupancy_map(&map);
  units_occupancy_rebuild(&pool);
  units_set_combat_human_nation(0);
  int rc = 0;
  s->moves = UNITS_MP_PER_TILE;
  s->orders = UNITS_ORDER_GOTO;
  s->goto_x = 5;
  s->goto_y = 4;
  /* On first: the off half may kill either unit. */
  mods_campaign_set(1u << MOD_GOTO_NATIVE_WAIT);
  bool stepped = units_advance_goto_one_step_w(&w, sid);
  if (stepped || s->x != 4 || s->y != 4 || s->orders != UNITS_ORDER_GOTO || !b->active ||
      s->moves != UNITS_MP_PER_TILE) {
    fprintf(stderr, "goto_native on: stepped %d at (%d,%d) orders %d brave %d moves %d\n",
            stepped, s->x, s->y, s->orders, b->active, s->moves);
    rc = 1;
  }
  mods_campaign_set(0);
  if (rc == 0) {
    stepped = units_advance_goto_one_step_w(&w, sid);
    const ColonizeUnit* bs = units_get_const(&pool, bid);
    const ColonizeUnit* ss = units_get_const(&pool, sid);
    if (!stepped && bs && bs->active && ss && ss->active && ss->moves > 0) {
      fprintf(stderr, "goto_native off: the Go To must attack, not wait\n");
      rc = 1;
    }
  }
  units_set_combat_human_nation(-1);
  units_set_occupancy_map(NULL);
  map_free(&map);
  units_reset_state();
  return rc;
}

static const TestCase k_cases[] = {
  {"unit_order_name_table", unit_order_name_table},
  {"unit_far_bfs_next_step", unit_far_bfs_next_step},
  {"unit_board_gate_reseed_1443", unit_board_gate_reseed_1443},
  {"unit_goto_native_wait_mod", unit_goto_native_wait_mod},
  {"unit_ai_undefended_colony_band", unit_ai_undefended_colony_band},
  {"unit_colony_entry_bvar4_reseed_1323", unit_colony_entry_bvar4_reseed_1323},
  {"unit_tb44_flood_key_rules", unit_tb44_flood_key_rules},
  {"unit_tb1_coarse_ne_link", unit_tb1_coarse_ne_link},
  {"unit_db81_far_waypoint_pingpong", unit_db81_far_waypoint_pingpong},
  {"unit_far_flood_keeps_probe_type", unit_far_flood_keeps_probe_type},
  {"unit_board_restore_bounded", unit_board_restore_bounded},
  {"unit_ai_move_marks_new_world", unit_ai_move_marks_new_world},
  {"unit_mp_entry_gating_713_724", unit_mp_entry_gating_713_724},
  {"unit_flood_river_pair_step", unit_flood_river_pair_step},
  {"unit_goto_dos_tails", unit_goto_dos_tails},
  {"unit_ship_pressure_0906_cost", unit_ship_pressure_0906_cost},
  {"unit_wake_passenger_can_land", unit_wake_passenger_can_land},
  {"unit_landfall_mp_gate", unit_landfall_mp_gate},
  {"unit_stack_one_click_wakes_and_selects", unit_stack_one_click_wakes_and_selects},
  {"unit_stack_shift_pick_group", unit_stack_shift_pick_group},
  {"unit_fog_vis_mask_and_snapshot", unit_fog_vis_mask_and_snapshot},
  {"unit_colony_enter_hull_stack_gate_791_792", unit_colony_enter_hull_stack_gate_791_792},
  {"unit_move_stage_seam_1107", unit_move_stage_seam_1107},
  {"unit_board_wakes_foreign_sentry", unit_board_wakes_foreign_sentry},
  {"unit_465b_board_settles_hull_resentries", unit_465b_board_settles_hull_resentries},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
