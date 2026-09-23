#include "test_units_common.h"

#include "../common/test_runner.h"

/* Split into named cases 2026-09-23 (was one 6.7k-line main()).
 * Shared fixture: fx_open() rebuilds the NAMES/pool/map/new-world start,
 * fx_stage2() replays the land-unit + stacking + despawn spine that the
 * later cases were written on top of. Every case rebuilds its own fixture,
 * so `COLONIZE_TEST_ONLY=<case>` runs one in isolation. */

static int g_diag_ready;
static ColonizeMsgCatalog names;
static ColonizeUnitPool pool;
static ColonizeWorldMap map;
static ColonizeSpriteSheet icons;
static char err[256];
static int pioneer;
static int colonist;
static int caravel;
static ColonizeUnit* ship;
static ColonizeUnit* starter;
static int ship_id;
static int land_x;
static int land_y;
static int edge_x;
static int edge_y;
static int unload_x;
static int unload_y;
static int ship_icon;

static void fx_close(void) {
  map_free(&map);
  assets_msg_free(&names);
}

static int fx_open(void) {
  if (!g_diag_ready) {
    diag_init(0, NULL);
    g_diag_ready = 1;
  }
  /* Shadow state that outlives the pool (occupancy map pointer, per-id
   * combat/goto state, combat sinks) must be cleared, or a case inherits the
   * previous one's leftovers -- see tests/README.md "Hunting order
   * dependencies". */
  units_set_occupancy_map(NULL);
  units_reset_state();
  memset(&pool, 0, sizeof(pool));
  memset(&map, 0, sizeof(map));
  ship = NULL;
  starter = NULL;
  diag_init(0, NULL);

  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "failed to load NAMES.TXT\n");
    return 1;
  }

  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }

  pioneer = units_find_type(&pool, "Pioneers");
  colonist = units_find_type(&pool, "Colonists");
  caravel = units_find_type(&pool, "Caravel");
  if (pioneer < 0 || colonist < 0 || caravel < 0) {
    fprintf(stderr, "missing expected unit types\n");
    assets_msg_free(&names);
    return 1;
  }
  if (pool.types[pioneer].domain != COLONIZE_UNIT_DOMAIN_LAND ||
      pool.types[caravel].domain != COLONIZE_UNIT_DOMAIN_SEA) {
    fprintf(stderr, "unexpected unit domain\n");
    assets_msg_free(&names);
    return 1;
  }

  char mp_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "AMER2.MP", mp_path, sizeof(mp_path)) ||
      !map_load_mp(mp_path, &map, err, sizeof(err))) {
    fprintf(stderr, "map load failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }

  units_new_world_start(&pool, &map, 39, 10, 0, 0);
  if (pool.unit_count < 3 || pool.selected_id < 0) {
    fprintf(stderr, "expected starter ship+pioneer+soldier (count=%d)\n", pool.unit_count);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  ship = units_get(&pool, pool.selected_id);
  if (!ship || !units_is_sea(&pool, ship->id)) {
    fprintf(stderr, "selected starter should be the ship\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (!map_tile_is_high_seas(&map, ship->x, ship->y)) {
    fprintf(stderr, "starter ship not on high seas (%d,%d)\n", ship->x, ship->y);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  /*
   * FUN_48d3_048e ring hunt from the landfall tile: AMER2's @SCENARIO tiles are
   * themselves High Seas, so the fleet lands on the requested tile exactly (the
   * e=0 hit), never on some other rim tile the old eastern-half scan preferred.
   * (bugs.md #487)
   */
  if (ship->x != 39 || ship->y != 10) {
    fprintf(stderr, "starter ship want landfall tile (39,10) got (%d,%d)\n", ship->x, ship->y);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (ship->cargo_count < 2) {
    fprintf(stderr, "starter ship expected Pioneer+Soldier cargo (got %d)\n", ship->cargo_count);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  /* Discoverer (diff 0) England: plain Pioneer + Veteran Soldier (COLONY00).
   * Hardy Pioneer is French-only, not all Discoverer nations. */
  return 0;
}

static int fx_stage2(void) {
  ship_id = ship->id;

  /* Separate land unit for domain / stacking tests (ship is offshore). */
  land_x = 39;
  land_y = 10;
  if (!map_tile_is_land(&map, land_x, land_y) || units_id_at(&pool, land_x, land_y) >= 0) {
    land_x = -1;
    /*
     * Interior tiles only (map_coords_inset / DOS FUN_137f_000a): AMER2 row 0
     * carries land in columns 1..3, but the outer rim is not a playable tile
     * and no unit may stand there — bugs.md #429. The tests below then step
     * the unit one tile east and want a free water neighbour for the boarding
     * case, so require: (x,y) and (x+1,y) both interior land, and (x+1,y)
     * coastal.
     */
    for (int y = 0; y < map.height && land_x < 0; ++y) {
      for (int x = 0; x < map.width; ++x) {
        if (!map_coords_inset(&map, x, y) || !map_coords_inset(&map, x + 1, y)) {
          continue;
        }
        if (!map_tile_is_land(&map, x, y) || units_id_at(&pool, x, y) >= 0) {
          continue;
        }
        if (!map_tile_is_land(&map, x + 1, y) || units_id_at(&pool, x + 1, y) >= 0) {
          continue;
        }
        bool coastal = false;
        for (int dy = -1; dy <= 1 && !coastal; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int tx = x + 1 + dx;
            const int ty = y + dy;
            if ((dx == 0 && dy == 0) || !map_coords_inset(&map, tx, ty)) {
              continue;
            }
            if (map_tile_is_water(&map, tx, ty) && units_id_at(&pool, tx, ty) < 0) {
              coastal = true;
              break;
            }
          }
        }
        if (!coastal) {
          continue;
        }
        land_x = x;
        land_y = y;
        break;
      }
    }
  }
  if (land_x < 0) {
    fprintf(stderr, "no free land tile for move tests\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  const int land_id = units_spawn(&pool, pioneer >= 0 ? pioneer : colonist, land_x, land_y);
  if (land_id < 0) {
    fprintf(stderr, "failed to spawn land test unit\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  starter = units_get(&pool, land_id);
  if (!starter || !map_tile_is_land(&map, starter->x, starter->y)) {
    fprintf(stderr, "land test unit not on land\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  int ocean_x = -1;
  int ocean_y = -1;
  for (int y = 0; y < map.height && ocean_x < 0; ++y) {
    for (int x = 0; x < map.width; ++x) {
      if (map_tile_is_water(&map, x, y) && units_id_at(&pool, x, y) < 0) {
        ocean_x = x;
        ocean_y = y;
        break;
      }
    }
  }
  if (ocean_x < 0) {
    fprintf(stderr, "no free ocean tile found\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, starter->id, ocean_x, ocean_y)) {
    fprintf(stderr, "land unit should not enter ocean\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, ship_id, land_x, land_y)) {
    fprintf(stderr, "sea unit should not enter land\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (land_x + 1 < map.width &&
      units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, starter->id, land_x + 1, land_y)) {
    if (starter->x != land_x + 1) {
      fprintf(stderr, "move did not update position\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }

  /* Spawn still rejects occupied tiles; friendly stacks via try_move are OK. */
  {
    const int stack_id = units_spawn_allow_stack(&pool, colonist, starter->x, starter->y);
    if (stack_id < 0) {
      fprintf(stderr, "friendly stack spawn_allow_stack failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, colonist, starter->x, starter->y, stack_id)) {
      fprintf(stderr, "friendly stack should be enterable\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, stack_id);
  }

  if (units_spawn(&pool, colonist, starter->x, starter->y) >= 0) {
    fprintf(stderr, "units_spawn should still reject occupied tiles\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  edge_x = 0;
  edge_y = 0;
  if (!units_find_high_seas_tile(&pool, &map, 39, 10, &edge_x, &edge_y)) {
    fprintf(stderr, "no high-seas tile on map\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (!units_on_high_seas(&map, edge_x, edge_y) || !map_tile_is_high_seas(&map, edge_x, edge_y)) {
    fprintf(stderr, "high-seas helper returned non-high-seas tile (%d,%d)\n", edge_x, edge_y);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  const int before = pool.unit_count;
  const int cargo_n = ship->cargo_count;
  if (!units_despawn(&pool, ship_id) || pool.unit_count != before - 1 - cargo_n) {
    fprintf(stderr, "despawn failed (before=%d cargo=%d after=%d)\n", before, cargo_n, pool.unit_count);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (units_get(&pool, ship_id) != NULL) {
    fprintf(stderr, "despawned unit still resolvable\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  /* Respawn a caravel next to the pioneer and exercise boarding. */
  unload_x = land_x;
  unload_y = land_y;
  return 0;
}

static int case_starter_fleet(void) {
  if (fx_open() != 0) {
    return 1;
  }
  fx_close();
  return 0;
}

static int case_domain_stack_despawn(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
  fx_close();
  return 0;
}

static int case_unit_icons(void) {
  if (fx_open() != 0) {
    return 1;
  }

  char ss_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "ICONS.SS", ss_path, sizeof(ss_path)) ||
      !ss_load(ss_path, &icons, err, sizeof(err))) {
    fprintf(stderr, "ICONS load failed: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  const int icon = pool.types[pioneer].icon_sprite;
  ship_icon = pool.types[caravel].icon_sprite;
  /* NAMES Pioneers=102, Caravel=6 are 1-based; blit indices are 101 and 5. */
  if (icon != 101) {
    fprintf(stderr, "pioneer icon expected 101 got %d\n", icon);
    ss_free(&icons);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (ship_icon != 5) {
    fprintf(stderr, "caravel icon expected 5 got %d\n", ship_icon);
    ss_free(&icons);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (icon < 0 || icon >= icons.sprite_count || icons.sprites[icon].width <= 0) {
    fprintf(stderr, "pioneer icon %d invalid (sprites=%d)\n", icon, icons.sprite_count);
    ss_free(&icons);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (ship_icon < 0 || ship_icon >= icons.sprite_count || icons.sprites[ship_icon].width <= 0) {
    fprintf(stderr, "caravel icon %d invalid\n", ship_icon);
    ss_free(&icons);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  ss_free(&icons);
  fx_close();
  return 0;
}

  /* Discoverer (diff 0) England: plain Pioneer + Veteran Soldier (COLONY00).
   * Hardy Pioneer is French-only, not all Discoverer nations. */
static int case_starter_discoverer_england(void) {
  if (fx_open() != 0) {
    return 1;
  }
    const ColonizeUnit* p0 = units_get_const(&pool, ship->cargo_ids[0]);
    const ColonizeUnit* p1 = units_get_const(&pool, ship->cargo_ids[1]);
    if (!p0 || !p1 || p0->profession != UNITS_JOB_NONE || p1->profession != UNITS_JOB_SOLDIER) {
      fprintf(
        stderr,
        "Discoverer England expected plain+Veteran professions (got %d,%d)\n",
        p0 ? p0->profession : -1,
        p1 ? p1->profession : -1
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  fx_close();
  return 0;
}

  /* Discoverer French: Hardy Pioneer + Veteran Soldier. */
static int case_starter_discoverer_french(void) {
  if (fx_open() != 0) {
    return 1;
  }
    ColonizeUnitPool fr;
    memset(&fr, 0, sizeof(fr));
    memset(&fr, 0, sizeof(fr));
    fr.type_count = pool.type_count;
    memcpy(fr.types, pool.types, sizeof(pool.types));
    const int fid = units_spawn_euro_starter_fleet(&fr, 1, 0, true, ship->x + 1, ship->y, 40, 10);
    ColonizeUnit* fs = units_get(&fr, fid);
    if (!fs || fs->cargo_count < 2) {
      fprintf(stderr, "French Discoverer fleet missing cargo\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const ColonizeUnit* fp0 = units_get_const(&fr, fs->cargo_ids[0]);
    const ColonizeUnit* fp1 = units_get_const(&fr, fs->cargo_ids[1]);
    if (!fp0 || !fp1 || fp0->profession != UNITS_JOB_PIONEER ||
        fp1->profession != UNITS_JOB_SOLDIER) {
      fprintf(
        stderr,
        "Discoverer French expected Hardy+Veteran (got %d,%d)\n",
        fp0 ? fp0->profession : -1,
        fp1 ? fp1->profession : -1
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (fs->profession != 0) {
      fprintf(stderr, "starter ship profession want 0 got %d\n", fs->profession);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  fx_close();
  return 0;
}

  /*
   * FUN_75c2_235c raw 121644-121647: the Discoverer Veteran Soldier is gated on
   * `bVar1` = this nation's player control byte (0x543f) == 0, i.e. the human.
   * An AI English nation on Discoverer gets a plain Soldier. (bugs.md #488)
   */
static int case_starter_discoverer_ai_english(void) {
  if (fx_open() != 0) {
    return 1;
  }
    ColonizeUnitPool aiw;
    memset(&aiw, 0, sizeof(aiw));
    aiw.type_count = pool.type_count;
    memcpy(aiw.types, pool.types, sizeof(pool.types));
    const int aid = units_spawn_euro_starter_fleet(&aiw, 0, 0, false, ship->x + 2, ship->y, 40, 10);
    ColonizeUnit* as = units_get(&aiw, aid);
    if (!as || as->cargo_count < 2) {
      fprintf(stderr, "AI Discoverer fleet missing cargo\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const ColonizeUnit* ap0 = units_get_const(&aiw, as->cargo_ids[0]);
    const ColonizeUnit* ap1 = units_get_const(&aiw, as->cargo_ids[1]);
    if (!ap0 || !ap1 || ap0->profession != UNITS_JOB_NONE || ap1->profession != UNITS_JOB_NONE) {
      fprintf(
        stderr,
        "AI English Discoverer expected plain skills (got %d,%d)\n",
        ap0 ? ap0->profession : -1,
        ap1 ? ap1->profession : -1
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Spain keeps its Veteran Soldier even as an AI (`|| local_8 == 2`). */
    ColonizeUnitPool sp;
    memset(&sp, 0, sizeof(sp));
    sp.type_count = pool.type_count;
    memcpy(sp.types, pool.types, sizeof(pool.types));
    const int spid = units_spawn_euro_starter_fleet(&sp, 2, 3, false, ship->x + 3, ship->y, 47, 61);
    ColonizeUnit* sps = units_get(&sp, spid);
    const ColonizeUnit* sp1 = sps && sps->cargo_count >= 2
                                ? units_get_const(&sp, sps->cargo_ids[1])
                                : NULL;
    if (!sp1 || sp1->profession != UNITS_JOB_SOLDIER) {
      fprintf(
        stderr,
        "AI Spain expected Veteran Soldier (got %d)\n",
        sp1 ? sp1->profession : -1
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  fx_close();
  return 0;
}

  /* Conquistador (diff 2) Dutch: plain Pioneer + plain Soldier. */
static int case_starter_conquistador_dutch(void) {
  if (fx_open() != 0) {
    return 1;
  }
    ColonizeUnitPool hard;
    memset(&hard, 0, sizeof(hard));
    memset(&hard, 0, sizeof(hard));
    hard.type_count = pool.type_count;
    memcpy(hard.types, pool.types, sizeof(pool.types));
    const int sid = units_spawn_euro_starter_fleet(&hard, 3, 2, true, ship->x, ship->y, 39, 10);
    ColonizeUnit* hs = units_get(&hard, sid);
    if (!hs || hs->cargo_count < 2) {
      fprintf(stderr, "Dutch Conquistador fleet missing cargo\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const ColonizeUnit* hp0 = units_get_const(&hard, hs->cargo_ids[0]);
    const ColonizeUnit* hp1 = units_get_const(&hard, hs->cargo_ids[1]);
    if (!hp0 || !hp1 || hp0->profession != UNITS_JOB_NONE || hp1->profession != UNITS_JOB_NONE) {
      fprintf(
        stderr,
        "Dutch Conquistador expected plain skills (got %d,%d)\n",
        hp0 ? hp0->profession : -1,
        hp1 ? hp1->profession : -1
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  fx_close();
  return 0;
}

static int case_boarding_caravel(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int bx = -1;
    int by = -1;
    if (!units_find_water_tile(&pool, &map, starter->x, starter->y, -1, &bx, &by)) {
      fprintf(stderr, "no water for boarding test\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Prefer an adjacent water tile so board adjacency succeeds. */
    bool adjacent = false;
    for (int dy = -1; dy <= 1 && !adjacent; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int tx = starter->x + dx;
        const int ty = starter->y + dy;
        if (map_tile_is_water(&map, tx, ty) && units_id_at(&pool, tx, ty) < 0) {
          bx = tx;
          by = ty;
          adjacent = true;
          break;
        }
      }
    }
    if (!adjacent) {
      fprintf(stderr, "no adjacent water tile for boarding\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ship_id = units_spawn(&pool, caravel, bx, by);
    if (ship_id < 0) {
      fprintf(stderr, "respawn caravel failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int land_id = starter->id;
    const int land_tile_x = starter->x;
    const int land_tile_y = starter->y;
    if (!units_board(&pool, land_id, ship_id)) {
      fprintf(stderr, "board failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* boarded = units_get(&pool, land_id);
    ColonizeUnit* carrier = units_get(&pool, ship_id);
    if (!boarded || !carrier || boarded->aboard_ship_id != ship_id ||
        carrier->cargo_count != 1 || units_id_at(&pool, land_tile_x, land_tile_y) >= 0) {
      fprintf(stderr, "board state incorrect\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (boarded->orders != 1 || boarded->moves != 0) {
      fprintf(stderr, "board should set sentry and zero moves\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, land_id, land_tile_x, land_tile_y)) {
      fprintf(stderr, "boarded unit should not move on map\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    int cargo_types[COLONIZE_UNIT_CARGO_MAX];
    int cargo_count = 0;
    int exported_type = -1;
    char exported_name[32];
    const int count_before_sail = pool.unit_count;
    int hold_types[COLONIZE_UNIT_CARGO_MAX];
    int hold_amts[COLONIZE_UNIT_CARGO_MAX];
    memset(hold_types, 0, sizeof(hold_types));
    memset(hold_amts, 0, sizeof(hold_amts));
    if (!units_despawn_ship_with_cargo(
          &pool,
          ship_id,
          &exported_type,
          exported_name,
          sizeof(exported_name),
          cargo_types,
          &cargo_count,
          COLONIZE_UNIT_CARGO_MAX,
          hold_types,
          hold_amts,
          COLONIZE_UNIT_CARGO_MAX
        )) {
      fprintf(stderr, "despawn_ship_with_cargo failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (exported_type != caravel || cargo_count != 1 || cargo_types[0] != pioneer ||
        pool.unit_count != count_before_sail - 2 || units_get(&pool, land_id) != NULL) {
      fprintf(
        stderr,
        "sail cargo export wrong (type=%d cargo=%d units=%d)\n",
        exported_type,
        cargo_count,
        pool.unit_count
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    int hx = 0;
    int hy = 0;
    if (!units_find_high_seas_tile(&pool, &map, 39, 10, &hx, &hy)) {
      fprintf(stderr, "no high seas for cargo respawn\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int returned = units_spawn_ship_with_cargo(
      &pool, caravel, hx, hy, cargo_types, cargo_count, hold_types, hold_amts
    );
    if (returned < 0) {
      fprintf(stderr, "spawn_ship_with_cargo failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* returned_ship = units_get(&pool, returned);
    if (!returned_ship || returned_ship->cargo_count != 1) {
      fprintf(stderr, "returned ship missing cargo\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int pax_id = returned_ship->cargo_ids[0];
    ColonizeUnit* pax = units_get(&pool, pax_id);
    if (!pax || pax->aboard_ship_id != returned || pax->type_index != pioneer ||
        units_is_on_map(pax)) {
      fprintf(stderr, "returned passenger state wrong\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /* Find adjacent land for unload. */
    int ux = -1;
    int uy = -1;
    for (int dy = -1; dy <= 1 && ux < 0; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int tx = returned_ship->x + dx;
        const int ty = returned_ship->y + dy;
        if (units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, pioneer, tx, ty, -1)) {
          ux = tx;
          uy = ty;
          break;
        }
      }
    }
    if (ux < 0) {
      /* High-seas tile may not touch land — move ship toward land first. */
      int near_x = land_tile_x;
      int near_y = land_tile_y;
      if (!units_find_water_tile(&pool, &map, land_tile_x, land_tile_y, returned, &near_x, &near_y)) {
        fprintf(stderr, "cannot berth for unload\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      returned_ship->x = near_x;
      returned_ship->y = near_y;
      pax->x = near_x;
      pax->y = near_y;
      for (int dy = -1; dy <= 1 && ux < 0; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          const int tx = near_x + dx;
          const int ty = near_y + dy;
          if (units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, pioneer, tx, ty, -1)) {
            ux = tx;
            uy = ty;
            break;
          }
        }
      }
    }
    if (ux < 0 || !units_unload_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, returned, ux, uy)) {
      fprintf(stderr, "unload failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pax = units_get(&pool, pax_id);
    if (!pax || pax->aboard_ship_id >= 0 || pax->x != ux || pax->y != uy ||
        units_id_at(&pool, ux, uy) != pax_id) {
      fprintf(stderr, "unload state incorrect\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    unload_x = ux;
    unload_y = uy;
    ship_id = returned;
  fx_close();
  return 0;
}

  /* Landfall unload: cargo with moves onto adjacent land; ship stays put. */
static int case_landfall_unload(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int bx = -1;
    int by = -1;
    int lx = -1;
    int ly = -1;
    for (int y = 1; y < map.height - 1 && lx < 0; ++y) {
      for (int x = 1; x < map.width - 1 && lx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) || units_id_at(&pool, x, y) >= 0) {
          continue;
        }
        for (int dy = -1; dy <= 1 && lx < 0; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
              continue;
            }
            const int wx = x + dx;
            const int wy = y + dy;
            if (map_tile_is_water(&map, wx, wy) && units_id_at(&pool, wx, wy) < 0) {
              lx = x;
              ly = y;
              bx = wx;
              by = wy;
              break;
            }
          }
        }
      }
    }
    if (lx < 0) {
      fprintf(stderr, "no land/water pair for landfall test\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int lf_ship = units_spawn(&pool, caravel, bx, by);
    const int lf_pax = units_spawn_allow_stack(&pool, pioneer, lx, ly);
    if (lf_ship < 0 || lf_pax < 0 || !units_board(&pool, lf_pax, lf_ship)) {
      fprintf(stderr, "landfall setup failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* lf_cargo = units_get(&pool, lf_pax);
    ColonizeUnit* lf_boat = units_get(&pool, lf_ship);
    if (!lf_cargo || !lf_boat) {
      fprintf(stderr, "landfall units missing\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    lf_cargo->moves = 3 * UNITS_MP_PER_TILE;
    lf_boat->moves = 4 * UNITS_MP_PER_TILE;
    if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, lf_ship, lx, ly)) {
      fprintf(stderr, "ship must not enter plain land via try_move\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* UN-28: units_first_cargo_with_moves deleted — units_first_landfall_cargo
     * supersedes it (same first loop plus the DOS spent test). */
    if (units_first_landfall_cargo(&pool, lf_ship) != lf_pax) {
      fprintf(stderr, "first landfall cargo mismatch\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, lf_ship, lf_pax, lx, ly)) {
      fprintf(stderr, "landfall unload_passenger failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    lf_cargo = units_get(&pool, lf_pax);
    lf_boat = units_get(&pool, lf_ship);
    if (!lf_cargo || !lf_boat || lf_cargo->aboard_ship_id >= 0 || lf_cargo->x != lx ||
        lf_cargo->y != ly || lf_boat->x != bx || lf_boat->y != by) {
      fprintf(stderr, "landfall state wrong\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Shore step charges passenger MP (sentry 0 → allotment − cost). */
    if (lf_cargo->moves >= 3 * UNITS_MP_PER_TILE) {
      fprintf(
        stderr,
        "landfall must consume passenger MP got %d\n",
        lf_cargo->moves
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    fprintf(stderr, "landfall unload MP ok (pax moves=%d)\n", lf_cargo->moves);
    units_despawn(&pool, lf_pax);
    units_despawn(&pool, lf_ship);

    /* Sentry cargo (moves 0) must still landfall — DOS spent==0. */
    {
      const int lf_ship2 = units_spawn(&pool, caravel, bx, by);
      const int lf_pax2 = units_spawn_allow_stack(&pool, pioneer, lx, ly);
      if (lf_ship2 < 0 || lf_pax2 < 0 || !units_board(&pool, lf_pax2, lf_ship2)) {
        fprintf(stderr, "landfall sentry setup failed\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      ColonizeUnit* sentry_pax = units_get(&pool, lf_pax2);
      if (!sentry_pax || sentry_pax->moves != 0) {
        fprintf(stderr, "landfall sentry should board with 0 MP\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (units_first_landfall_cargo(&pool, lf_ship2) != lf_pax2) {
        fprintf(stderr, "landfall sentry cargo not eligible\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (!units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, lf_ship2, lf_pax2, lx, ly)) {
        fprintf(stderr, "landfall sentry unload failed\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      sentry_pax = units_get(&pool, lf_pax2);
      const ColonizeUnitType* pt = units_type(&pool, pioneer);
      const int max_mp = pt && pt->movement > 0 ? pt->movement : 1;
      if (!sentry_pax || sentry_pax->aboard_ship_id >= 0 || sentry_pax->moves >= max_mp) {
        fprintf(
          stderr,
          "landfall sentry MP want < %d got %d\n",
          max_mp,
          sentry_pax ? sentry_pax->moves : -1
        );
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      fprintf(stderr, "landfall sentry cargo MP ok (moves=%d)\n", sentry_pax->moves);
      units_despawn(&pool, lf_pax2);
      units_despawn(&pool, lf_ship2);
    }
  fx_close();
  return 0;
}

  /* Colony dock: ship may enter own colony; disembark clears sentry. */
static int case_colony_dock(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    colonies_set_occupancy_map(NULL);
    if (!colonies_load_names(&colonies, "COLONIZE/COLONY.TXT") ||
        !colonies_load_buildings(&colonies, &names)) {
      fprintf(stderr, "colony catalogs failed for dock test\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int cx = -1;
    int cy = -1;
    int wx = -1;
    int wy = -1;
    for (int y = 1; y < map.height - 1 && cx < 0; ++y) {
      for (int x = 1; x < map.width - 1 && cx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) || !map_tile_is_coastal(&map, x, y)) {
          continue;
        }
        if (units_id_at(&pool, x, y) >= 0) {
          continue;
        }
        for (int dy = -1; dy <= 1 && cx < 0; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
              continue;
            }
            const int tx = x + dx;
            const int ty = y + dy;
            if (map_tile_is_water(&map, tx, ty) && units_id_at(&pool, tx, ty) < 0) {
              cx = x;
              cy = y;
              wx = tx;
              wy = ty;
              break;
            }
          }
        }
      }
    }
    if (cx < 0) {
      fprintf(stderr, "no coastal tile for dock test\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int cid = colonies_found(&colonies, &map, cx, cy, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
    if (cid < 0) {
      fprintf(stderr, "colonies_found failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeColony* col = colonies_get_mut(&colonies, cid);
    if (col) {
      col->nation_id = 0;
    }
    const int dock_ship = units_spawn(&pool, caravel, wx, wy);
    if (dock_ship < 0) {
      fprintf(stderr, "dock ship spawn failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* dship = units_get(&pool, dock_ship);
    dship->nation_id = 0;
    const int dock_pax = units_spawn_allow_stack(&pool, pioneer, cx, cy);
    if (dock_pax < 0) {
      fprintf(stderr, "dock pax spawn failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* dpax = units_get(&pool, dock_pax);
    dpax->nation_id = 0;
    dpax->x = wx;
    dpax->y = wy;
    if (!units_board_stacked(&pool, dock_pax, dock_ship)) {
      fprintf(stderr, "dock board_stacked failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    dpax = units_get(&pool, dock_pax);
    if (!dpax || dpax->orders != 1) {
      fprintf(stderr, "board_stacked should set sentry\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /*
     * bugs.md #482 — a Wagon Train may never board. DOS-LITERAL
     * FUN_4720_00e0 (viceroy_unpacked_2.c raw 74644-74648) only considers a
     * non-ship unit whose @UNIT size column (DS:0x5238) is `< 99`; Wagon
     * Train's column is 99, so the fit pass skips it outright (same `< 99`
     * test in the FUN_4720_015c candidate walk, raw 74739).
     */
    {
      const int wagon_type = units_find_type(&pool, "Wagon Train");
      if (wagon_type < 0) {
        fprintf(stderr, "Wagon Train @UNIT type missing\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      const ColonizeUnitType* wt = units_type(&pool, wagon_type);
      if (!wt || wt->space != 99) {
        fprintf(stderr, "Wagon Train @UNIT size should be the 99 sentinel\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      const int wagon = units_spawn_allow_stack(&pool, wagon_type, cx, cy);
      ColonizeUnit* wu = units_get(&pool, wagon);
      if (wagon < 0 || !wu) {
        fprintf(stderr, "wagon spawn failed\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      wu->nation_id = 0;
      const int wagon_ship = units_spawn_allow_stack(&pool, caravel, cx, cy);
      ColonizeUnit* wsu = units_get(&pool, wagon_ship);
      if (wagon_ship < 0 || !wsu) {
        fprintf(stderr, "wagon-test ship spawn failed\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      wsu->nation_id = 0;
      if (units_board_stacked(&pool, wagon, wagon_ship) ||
          units_board(&pool, wagon, wagon_ship)) {
        fprintf(stderr, "Wagon Train must not be able to board a ship\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      wu = units_get(&pool, wagon);
      wsu = units_get(&pool, wagon_ship);
      if (!wu || wu->aboard_ship_id >= 0 || !wsu || wsu->cargo_count != 0) {
        fprintf(stderr, "refused wagon boarding must leave both units untouched\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      (void)units_despawn(&pool, wagon);
      (void)units_despawn(&pool, wagon_ship);
    }

    if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map)}, caravel, cx, cy, dock_ship)) {
      fprintf(stderr, "ship should enter own colony tile\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, dock_ship, cx, cy)) {
      fprintf(stderr, "ship try_move onto colony failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* bugs.md: docking already put the passenger ashore, with no orders — a
     * further disembark call has nothing left to do. */
    const ColonizeUnit* dock_ship_u = units_get_const(&pool, dock_ship);
    const int nd = units_disembark_all(&pool, dock_ship, cx, cy);
    dpax = units_get(&pool, dock_pax);
    if (nd != 0 || !dock_ship_u || dock_ship_u->cargo_count != 0 || !dpax ||
        dpax->aboard_ship_id >= 0 || dpax->orders != 0 || dpax->x != cx || dpax->y != cy) {
      fprintf(stderr, "disembark_all state wrong (n=%d)\n", nd);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    int stack_ids[UNITS_TILE_STACK_MAX];
    /* Re-board for stack collect: ship + passenger. */
    dpax->x = cx;
    dpax->y = cy;
    if (!units_board_stacked(&pool, dock_pax, dock_ship)) {
      /* ship is on colony land; board_stacked does not need water adjacency */
      fprintf(stderr, "reboard for stack collect failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int sn = units_collect_tile_stack(&pool, cx, cy, 0, stack_ids, UNITS_TILE_STACK_MAX);
    if (sn < 2) {
      fprintf(stderr, "tile stack should list ship+cargo (got %d)\n", sn);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, dock_pax);
    units_despawn(&pool, dock_ship);
  fx_close();
  return 0;
}

  /* Phase 7: terrain MP costs, pioneer plow/road, yield bonuses. */
static int case_terrain_mp_improvements(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    ColonizeWorldMap tmap;
    memset(&tmap, 0, sizeof(tmap));
    if (!map_alloc(&tmap, 8, 8, err, sizeof(err))) {
      fprintf(stderr, "phase7 map_alloc failed: %s\n", err);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int px = -1;
    int py = -1;
    /* Interior tiles only — the 1-tile rim is not playable (map_coords_inset /
     * DOS FUN_137f_000a; bugs.md #429), so a unit may not stand or step there. */
    for (int y = 1; y < 6 && px < 0; ++y) {
      for (int x = 1; x < 6; ++x) {
        if (!map_coords_inset(&tmap, x, y) || !map_coords_inset(&tmap, x + 1, y)) {
          continue;
        }
        if (units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
          px = x;
          py = y;
          break;
        }
      }
    }
    if (px < 0) {
      fprintf(stderr, "phase7: no free adjacent tiles\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int fx = px + 1;
    const int fy = py;
    tmap.terrain[py * tmap.width + px] = 2; /* plains */
    tmap.terrain[fy * tmap.width + fx] = 10; /* mixed forest */
    /* DOS terr_cost table (NAMES scale; Brave uses *3): class10=2, class9=1, class27=3. */
    if (map_move_cost_at(&tmap, fx, fy) != 2) {
      fprintf(stderr, "forest move cost expected 2 got %d\n", map_move_cost_at(&tmap, fx, fy));
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    {
      const uint8_t save = tmap.terrain[fy * tmap.width + fx];
      tmap.terrain[fy * tmap.width + fx] = 9; /* terr_cost[9]=1 */
      if (map_move_cost_at(&tmap, fx, fy) != 1) {
        fprintf(stderr, "class9 move cost expected 1 got %d\n", map_move_cost_at(&tmap, fx, fy));
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      tmap.terrain[fy * tmap.width + fx] = (uint8_t)(2 | 0xa0); /* mountain → class 27, cost 3 */
      if (map_dos_terr_class_at(&tmap, fx, fy) != 27 || map_move_cost_at(&tmap, fx, fy) != 3) {
        fprintf(
          stderr,
          "mountain class/cost expected 27/3 got %d/%d\n",
          map_dos_terr_class_at(&tmap, fx, fy),
          map_move_cost_at(&tmap, fx, fy)
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      tmap.terrain[fy * tmap.width + fx] = save;
    }
    const int pid = units_spawn(&pool, pioneer, px, py);
    ColonizeUnit* pu = units_get(&pool, pid);
    if (!pu || pu->tools != 100 || !units_is_pioneer(&pool, pid)) {
      fprintf(stderr, "phase7 pioneer spawn/tools failed\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu->moves = 1 * UNITS_MP_PER_TILE; /* pioneer max is 1 — full allotment */
    if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&tmap), .rng=(ColonizeDosRng*)(NULL)}, pid, fx, fy) || pu->moves != 0) {
      fprintf(
        stderr,
        "phase7 full-MP forest enter should succeed and exhaust (moves=%d)\n",
        pu->moves
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, pid);

    /* Partial MP without RNG: deny and do not charge. */
    const int pid_partial = units_spawn(&pool, pioneer, px, py);
    ColonizeUnit* pu_partial = units_get(&pool, pid_partial);
    if (!pu_partial) {
      fprintf(stderr, "phase7 partial pioneer spawn failed\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pool.types[pioneer].movement = 2;
    pu_partial->moves = 1 * UNITS_MP_PER_TILE;
    if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&tmap), .rng=(ColonizeDosRng*)(NULL)}, pid_partial, fx, fy) ||
        pu_partial->moves != 1 * UNITS_MP_PER_TILE) {
      fprintf(
        stderr,
        "phase7 partial-MP without RNG should fail uncharged (moves=%d)\n",
        pu_partial->moves
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /* DOS FUN_465b: range(1,cost) <= remaining; cost always charged. */
    {
      ColonizeDosRng rng;
      dos_rng_seed(&rng, 1u); /* first range(1,2) → 1 → success */
      const int ox = pu_partial->x;
      const int oy = pu_partial->y;
      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&tmap), .rng=(ColonizeDosRng*)(&rng)}, pid_partial, fx, fy) ||
          pu_partial->x != fx || pu_partial->y != fy || pu_partial->moves != 0) {
        fprintf(
          stderr,
          "phase7 RNG success should enter forest (pos=%d,%d moves=%d)\n",
          pu_partial->x,
          pu_partial->y,
          pu_partial->moves
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      /* Reset for fail case. */
      pu_partial->x = ox;
      pu_partial->y = oy;
      pu_partial->moves = 1 * UNITS_MP_PER_TILE;
      dos_rng_seed(&rng, 5006u); /* first range(1,2) → 2 → fail */
      if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&tmap), .rng=(ColonizeDosRng*)(&rng)}, pid_partial, fx, fy) ||
          pu_partial->x != ox || pu_partial->y != oy || pu_partial->moves != 0) {
        fprintf(
          stderr,
          "phase7 RNG fail should stay and exhaust (pos=%d,%d moves=%d)\n",
          pu_partial->x,
          pu_partial->y,
          pu_partial->moves
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
    pool.types[pioneer].movement = 1;
    units_despawn(&pool, pid_partial);

    const int pid2 = units_spawn(&pool, pioneer, px, py);
    ColonizeUnit* pu2 = units_get(&pool, pid2);
    if (!pu2) {
      fprintf(stderr, "phase7 second pioneer spawn failed\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    map_tile_set_road(&tmap, fx, fy, true);
    map_tile_set_road(&tmap, px, py, true); /* DOS FA road-pair both tiles */
    if (map_move_cost_step(&tmap, px, py, fx, fy) != 1) {
      fprintf(
        stderr,
        "roaded forest step cost expected 1 got %d\n",
        map_move_cost_step(&tmap, px, py, fx, fy)
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Single-tile query has no `from` tile, so the FA road pair cannot apply:
     * DOS 465b's cost head never discounts on the destination alone. */
    if (map_move_cost_at(&tmap, fx, fy) != 2) {
      fprintf(stderr, "roaded forest cost expected 2 got %d\n", map_move_cost_at(&tmap, fx, fy));
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu2->moves = 2 * UNITS_MP_PER_TILE;
    if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&tmap), .rng=(ColonizeDosRng*)(NULL)}, pid2, fx, fy) ||
        pu2->moves != 2 * UNITS_MP_PER_TILE - 1) {
      fprintf(stderr, "phase7 roaded forest move failed (moves=%d)\n", pu2->moves);
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, pid2);

    const int pid3 = units_spawn(&pool, pioneer, px, py);
    ColonizeUnit* pu3 = units_get(&pool, pid3);
    if (!pu3) {
      fprintf(stderr, "phase7 third pioneer spawn failed\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    map_tile_set_road(&tmap, px, py, false);
    char pmsg[64];
    pu3->moves = 1 * UNITS_MP_PER_TILE;
    /*
     * Plains road: real DS:0x2f78 threshold byte + 2 (5 turns for
     * non-Hardy, since the live 2026-08-20 capture landed
     * pioneer_threshold[2]=3), not the old "terr_cost 1, one tick"
     * approximation — drive ticks to completion.
     */
    bool road3_ok = true;
    while (!map_tile_has_road(&tmap, px, py)) {
      road3_ok = units_pioneer_road_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&tmap)}, pid3, pmsg, sizeof(pmsg), NULL, NULL);
      if (!road3_ok) {
        break;
      }
    }
    if (!road3_ok || !map_tile_has_road(&tmap, px, py) || pu3->tools != 80 || pu3->moves != 0) {
      fprintf(
        stderr,
        "phase7 road failed tools=%d road=%d moves=%d (%s)\n",
        pu3->tools,
        (int)map_tile_has_road(&tmap, px, py),
        pu3->moves,
        pmsg
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /*
     * FUN_479b_0526 road completion: nearest same-nation colony gets a flat
     * +10 hammers_purchased. Separate pioneer/tile so it doesn't disturb
     * pu3's own state ahead of the plow test below.
     */
    {
      ColonizeColonyPool colonies_road;
      colonies_init(&colonies_road);
      colonies_set_occupancy_map(NULL);
      ColonizeColony* c = &colonies_road.colonies[0];
      c->id = 0;
      c->active = true;
      c->nation_id = 0;
      c->x = px;
      c->y = py;
      c->hammers_purchased = 5;
      colonies_road.colony_count = 1;
      colonies_road.next_id = 1;
      const int rx = 7;
      const int ry = 7;
      tmap.terrain[ry * tmap.width + rx] = 2; /* plains, own tile away from px/py */
      map_tile_set_road(&tmap, rx, ry, false);
      const int pid4 = units_spawn(&pool, pioneer, rx, ry);
      ColonizeUnit* pu4 = units_get(&pool, pid4);
      if (!pu4) {
        fprintf(stderr, "phase7 fourth pioneer spawn failed\n");
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      pu4->nation_id = 0;
      pu4->moves = 1 * UNITS_MP_PER_TILE;
      pu4->tools = 100;
      bool road4_ok = true;
      while (!map_tile_has_road(&tmap, rx, ry)) {
        road4_ok = units_pioneer_road_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies_road), .map=(ColonizeWorldMap*)(&tmap)}, pid4, pmsg, sizeof(pmsg), NULL, NULL);
        if (!road4_ok) {
          break;
        }
      }
      if (!road4_ok || !map_tile_has_road(&tmap, rx, ry)) {
        fprintf(stderr, "phase7 road-reward setup failed (%s)\n", pmsg);
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (colonies_road.colonies[0].hammers_purchased != 15) {
        fprintf(
          stderr,
          "phase7 road completion hammers_purchased=%u want 15\n",
          (unsigned)colonies_road.colonies[0].hammers_purchased
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      units_despawn(&pool, pid4);
    }

    pu3->moves = 1 * UNITS_MP_PER_TILE;
    pu3->tools = 100;
    pu3->orders = UNITS_ORDER_NONE;
    pu3->col1_counter16 = 0;
    const int farm_base = colony_yield_for_tile(&tmap, px, py, COLONIZE_JOB_FARMER);
    /* Plains plow: terr_cost+2 = 3 turns for non-Hardy; drive ticks to completion. */
    if (!units_pioneer_plow_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&tmap)}, pid3, pmsg, sizeof(pmsg), NULL, NULL)) {
      fprintf(stderr, "phase7 plow start failed (%s)\n", pmsg);
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    while (pu3->orders == UNITS_ORDER_CLEAR_PLOW) {
      if (!units_pioneer_work_tick_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&tmap)}, pid3, pmsg, sizeof(pmsg), NULL, NULL)) {
        fprintf(stderr, "phase7 plow tick failed (%s)\n", pmsg);
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
    if (!map_tile_is_plowed(&tmap, px, py) || pu3->tools != 80) {
      fprintf(
        stderr,
        "phase7 plow failed plowed=%d tools=%d (%s)\n",
        (int)map_tile_is_plowed(&tmap, px, py),
        pu3->tools,
        pmsg
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /*
     * A non-expert Farmer's unconditional +1 (colony_yield_pipeline) DOES
     * stack with plow (+1 more) — the "doesn't stack" conclusion reached
     * 2026-08-18 used New Amsterdam/Fort Orange's aggregates as its two
     * anchors, but both were computed under the then-undiscovered
     * town-commons plow bug (+2 instead of the real +1), which inflated
     * commons by exactly +1 for those same two plowed colonies — so the
     * "no stacking" fit was curve-fit against a wrong baseline. Once
     * commons plow was corrected, both colonies needed their field total
     * back up by +1, which this stacking plow term supplies exactly,
     * restoring this test's original expectation.
     */
    const int farm_plowed = colony_yield_for_tile(&tmap, px, py, COLONIZE_JOB_FARMER);
    if (farm_plowed != farm_base + 1) {
      fprintf(stderr, "phase7 plow yield expected %d got %d\n", farm_base + 1, farm_plowed);
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int lumber_base = colony_yield_for_tile(&tmap, fx, fy, COLONIZE_JOB_LUMBERJACK);
    map_tile_set_road(&tmap, fx, fy, false);
    const int lumber_clear = colony_yield_for_tile(&tmap, fx, fy, COLONIZE_JOB_LUMBERJACK);
    map_tile_set_road(&tmap, fx, fy, true);
    /* Lumberjack's road bonus is base 2 (fur/lumber bucket, same as
     * river's — player-confirmed 2026-08-15, Viceroy, see
     * colony_yield_road_bonus). This call goes through colony_yield_for_tile
     * (profession=-1, no worker context), so the matching-expert-or-
     * Lumberjack "big_unit" x2 doesn't apply here — that only fires for a
     * specific worker via colony_yield_for_worker (see
     * test_colony_yield.c's own expert/road checks for that path); this one
     * is the plain non-expert +2. 2026-08-18: this bucket had regressed
     * back to the old flat "ore/silver +1" grouping for Lumberjack (see
     * colony_yield_road_bonus's fix comment) — re-fixed, and this
     * assertion (which had drifted to a stale "+1" while its own comment
     * already described "+2 base", a leftover from even earlier road-bonus
     * history) now matches it. See docs/terrain_yields.md "Plow / road /
     * river stacking". */
    if (lumber_base != lumber_clear + 2) {
      fprintf(
        stderr,
        "phase7 road lumber yield expected %d got base=%d clear=%d\n",
        lumber_clear + 2,
        lumber_base,
        lumber_clear
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Town commons: plains → food + cotton; forest → food + furs (not lumber). */
    {
      ColonizeTownCommonsYield tc;
      colony_yield_town_commons(&tmap, px, py, 0, 2, &tc);
      if (tc.food <= 0 || tc.secondary_cargo != COLONIZE_CARGO_COTTON) {
        fprintf(
          stderr,
          "town commons plains expected food+cotton got food=%d cargo=%d amt=%d\n",
          tc.food,
          tc.secondary_cargo,
          tc.secondary_amount
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      colony_yield_town_commons(&tmap, fx, fy, 0, 2, &tc);
      if (tc.food <= 0 || tc.secondary_cargo != COLONIZE_CARGO_FURS) {
        fprintf(
          stderr,
          "town commons forest expected food+furs got food=%d cargo=%d amt=%d\n",
          tc.food,
          tc.secondary_cargo,
          tc.secondary_amount
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
    units_despawn(&pool, pid3);
    map_free(&tmap);
  fx_close();
  return 0;
}

  /* Go-to pathfinding: next step, spend MP, keep order, resume after end_turn. */
static int case_goto_pathfinding(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int lx = -1;
    int ly = -1;
    for (int y = 20; y < (int)map.height - 20 && lx < 0; ++y) {
      for (int x = 20; x < (int)map.width - 20; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 3, y + 2) &&
            map_tile_is_land(&map, x + 1, y) &&
            units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, pioneer, x, y, -1) &&
            units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, pioneer, x + 1, y, -1) &&
            units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, pioneer, x + 3, y + 2, -1) &&
            map_move_cost_at(&map, x + 1, y) <= 1) {
          lx = x;
          ly = y;
          break;
        }
      }
    }
    if (lx < 0) {
      fprintf(stderr, "goto test: no land path found\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int uid = units_spawn_allow_stack(&pool, pioneer, lx, ly);
    ColonizeUnit* walker = units_get(&pool, uid);
    if (!walker) {
      fprintf(stderr, "goto spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int gx = lx + 3;
    const int gy = ly + 2;
    if (!units_set_goto_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, uid, gx, gy)) {
      fprintf(stderr, "units_set_goto failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (walker->orders != UNITS_ORDER_GOTO) {
      fprintf(stderr, "expected ORDER_GOTO got %d\n", walker->orders);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int nx = -1;
    int ny = -1;
    if (!units_next_goto_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, uid, &nx, &ny)) {
      fprintf(stderr, "units_next_goto_step failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int before_mp = walker->moves;
    units_advance_goto_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, uid);
    walker = units_get(&pool, uid);
    if (!walker || (walker->x == lx && walker->y == ly && walker->moves >= before_mp)) {
      fprintf(stderr, "advance_goto made no progress\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (walker->x == gx && walker->y == gy) {
      if (walker->orders != UNITS_ORDER_NONE) {
        fprintf(stderr, "arrival should clear orders\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    } else if (walker->orders != UNITS_ORDER_GOTO) {
      fprintf(stderr, "partial advance should keep GOTO\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    } else {
      /* Stand in for the engine's per-nation MP refresh
       * (turn_refresh_moves_for_nation); this target does not link turn.c. */
      walker = units_get(&pool, uid);
      if (walker) {
        walker->moves = units_max_mp(&pool, uid);
      }
      const int refreshed = walker ? walker->moves : 0;
      const int px = walker ? walker->x : -1;
      const int py = walker ? walker->y : -1;
      units_advance_goto_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, uid);
      walker = units_get(&pool, uid);
      if (!walker) {
        fprintf(stderr, "walker missing after resume\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (walker->moves >= refreshed && walker->x == px && walker->y == py &&
          !(walker->x == gx && walker->y == gy)) {
        fprintf(stderr, "resume after end_turn made no progress\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
  fx_close();
  return 0;
}

  /* Orders / allegiance chrome: corner table + @ORDERS letters + nation ink. */
static int case_orders_chrome(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    unit_chrome_load_orders(&names);
    const int caravel_t = units_find_type(&pool, "Caravel");
    const int frigate_t = units_find_type(&pool, "Frigate");
    const int treasure_t = units_find_type(&pool, "Treasure");
    const int dragoon_t = units_find_type(&pool, "Dragoons");
    if (unit_chrome_corner_for_type(colonist, false) != UNIT_CHROME_CORNER_BOTTOM_RIGHT ||
        unit_chrome_corner_for_type(caravel_t, false) != UNIT_CHROME_CORNER_TOP_LEFT ||
        unit_chrome_corner_for_type(frigate_t, false) != UNIT_CHROME_CORNER_TOP_RIGHT ||
        unit_chrome_corner_for_type(treasure_t, false) != UNIT_CHROME_CORNER_TOP_CENTER ||
        unit_chrome_corner_for_type(dragoon_t, false) != UNIT_CHROME_CORNER_TOP_LEFT) {
      fprintf(stderr, "unit_chrome corner table mismatch\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (unit_chrome_order_letter(0, 0) != '-' || unit_chrome_order_letter(1, 0) != 'S' ||
        unit_chrome_order_letter(3, 0) != 'G') {
      fprintf(stderr, "unit_chrome order letters mismatch\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Natives always '-'; Sentry letter uses NAMES color-8; England fill is saturated 112. */
    if (unit_chrome_order_letter(1, 5) != '-' ||
        unit_chrome_letter_color(0, 1) != (uint8_t)(12 - 8) ||
        unit_chrome_letter_color(0, 0) != 0 ||
        unit_chrome_nation_color(0) != 112 || unit_chrome_nation_color(1) != 9) {
      fprintf(stderr, "unit_chrome letter/nation color mismatch\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Equipment remap: horses → Scout (top-left). */
    {
      const int id = units_spawn_allow_stack(&pool, colonist, 10, 10);
      ColonizeUnit* u = units_get(&pool, id);
      if (!u) {
        fprintf(stderr, "chrome remap spawn failed\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      u->horses = 50;
      const int dtype = units_display_type_index(&pool, id);
      if (unit_chrome_corner_for_type(dtype, false) != UNIT_CHROME_CORNER_TOP_LEFT) {
        fprintf(stderr, "horses should display as Scout top-left (dtype=%d)\n", dtype);
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      /*
       * bugs.md: the move slide / combat lunge hide the piece from the base
       * frame by clearing `active`, and `units_display_type_index` resolves
       * through `units_get_const`, which skips inactive units. Reading the
       * chrome corner while the piece is hidden therefore yields -1, and
       * unit_chrome_corner_for_type falls through to BOTTOM_RIGHT — every
       * top-corner class (mounted, artillery, wagons, ships) wore the wrong
       * corner for the length of the animation. Both call sites now hoist
       * the lookup; this pins the trap so an in-loop call is caught here.
       */
      u->active = false;
      if (units_display_type_index(&pool, id) != -1 ||
          unit_chrome_corner_for_type(units_display_type_index(&pool, id), false) !=
            UNIT_CHROME_CORNER_BOTTOM_RIGHT) {
        fprintf(stderr, "expected -1 dtype (default corner) for a hidden unit\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      u->active = true;
      if (unit_chrome_corner_for_type(units_display_type_index(&pool, id), false) !=
          UNIT_CHROME_CORNER_TOP_LEFT) {
        fprintf(stderr, "hidden-unit dtype probe did not restore\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      units_despawn(&pool, id);
    }
  fx_close();
  return 0;
}

  /* Fortify / sentry / disband orders. */
static int case_fortify_sentry_disband(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    const int soldier = units_find_type(&pool, "Soldier");
    const int sid = units_spawn(&pool, soldier >= 0 ? soldier : pioneer, 12, 12);
    if (sid < 0) {
      fprintf(stderr, "orders spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* su = units_get(&pool, sid);
    su->nation_id = 0;
    su->moves = 3 * UNITS_MP_PER_TILE;
    if (!units_order_fortify(&pool, sid) || su->orders != UNITS_ORDER_FORTIFY ||
        su->moves != 0) {
      fprintf(stderr, "fortify order failed orders=%d mp=%d\n", su->orders, su->moves);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Overnight promotion (same as turn_refresh_moves_for_nation) — the
     * refresh also counts the night in park_nights, which is what lets
     * the wake refund the allotment (bugs.md: fortified on a PREVIOUS turn
     * moves on activation; same-turn dig-ins do not get refunds). Smell
     * #39: col1_counter16 is the shared DOS +0x16 clock, no longer used. */
    su->orders = UNITS_ORDER_FORTIFIED;
    su->moves = 0;
    su->park_nights = 1;
    if (su->orders != UNITS_ORDER_FORTIFIED || su->moves != 0) {
      fprintf(stderr, "fortify overnight failed orders=%d mp=%d\n", su->orders, su->moves);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /*
     * bugs.md #695: FUN_2b5a_1112's tail (raw 42418-42421) has no "already
     * fortified" early-out — re-issuing Fortify on a dug-in unit rewrites
     * order 5, zeroes +0x315a and exhausts the allotment all over again.
     */
    su->moves = 3 * UNITS_MP_PER_TILE;
    su->col1_counter16 = 7;
    if (!units_order_fortify(&pool, sid) || su->orders != UNITS_ORDER_FORTIFY ||
        su->moves != 0 || su->col1_counter16 != 0) {
      fprintf(stderr, "re-fortify no-op: orders=%d mp=%d counter=%d\n",
              su->orders, su->moves, (int)su->col1_counter16);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    su->orders = UNITS_ORDER_FORTIFIED;
    su->moves = 0;
    su->park_nights = 1;
    if (!units_wake(&pool, sid) || su->orders != UNITS_ORDER_NONE || su->moves <= 0) {
      fprintf(stderr, "wake fortified failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_order_sentry(&pool, sid) || su->orders != UNITS_ORDER_SENTRY) {
      fprintf(stderr, "sentry order failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_disband(&pool, sid) || units_get_const(&pool, sid) != NULL) {
      fprintf(stderr, "disband failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  fx_close();
  return 0;
}

  /* Dump overboard / anchor / trade route / pillage. */
static int case_dump_anchor_route_pillage(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    const int caravel_t = units_find_type(&pool, "Caravel");
    int sx = -1, sy = -1;
    for (int y = 1; y < (int)map.height - 1 && sx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && sx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) && !map_tile_is_high_seas(&map, x, y) &&
            units_id_at(&pool, x, y) < 0) {
          sx = x;
          sy = y;
        }
      }
    }
    if (caravel_t < 0 || sx < 0) {
      fprintf(stderr, "dump/anchor: no caravel or sea tile\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int ship = units_spawn(&pool, caravel_t, sx, sy);
    ColonizeUnit* sh = units_get(&pool, ship);
    if (!sh) {
      fprintf(stderr, "dump ship spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    sh->nation_id = 0;
    if (units_load_goods(&pool, ship, COLONIZE_CARGO_SUGAR, 40) != 40) {
      fprintf(stderr, "load sugar for dump failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int ctype = -1, amt = -1;
    if (units_dump_cargo_overboard(&pool, ship, &ctype, &amt) != 40 || ctype != COLONIZE_CARGO_SUGAR ||
        amt != 40 || units_first_goods_hold(&pool, ship) >= 0) {
      fprintf(stderr, "dump overboard failed ctype=%d amt=%d\n", ctype, amt);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_order_trade_route(&pool, ship) || sh->orders != UNITS_ORDER_TRADE_ROUTE ||
        sh->moves != 0) {
      fprintf(stderr, "trade route order failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_orders_follow_goto(sh->orders)) {
      fprintf(stderr, "trade route should follow goto for advance\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_clear_orders(&pool, ship);

    /* Anchor: need own colony adjacent/on tile — found a tiny colony next to ship. */
    ColonizeColonyPool cpool;
    colonies_init(&cpool);
    colonies_set_occupancy_map(NULL);
    int cx = -1, cy = -1;
    for (int dy = -1; dy <= 1 && cx < 0; ++dy) {
      for (int dx = -1; dx <= 1 && cx < 0; ++dx) {
        const int tx = sx + dx;
        const int ty = sy + dy;
        if (map_coords_inset(&map, tx, ty) && map_tile_is_land(&map, tx, ty)) {
          cx = tx;
          cy = ty;
        }
      }
    }
    if (cx < 0) {
      fprintf(stderr, "anchor: no land near ship\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (colonies_found(&cpool, &map, cx, cy, 0, -1, UNITS_JOB_NONE, 0, 0, 0) < 0) {
      fprintf(stderr, "anchor colony found failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    sh = units_get(&pool, ship);
    if (sh) {
      sh->moves = 4 * UNITS_MP_PER_TILE;
    }
    if (!units_order_fortify(&pool, ship) || !sh || sh->orders != UNITS_ORDER_FORTIFY) {
      fprintf(stderr, "anchor order failed orders=%d\n", sh ? sh->orders : -1);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /*
     * Open water, no colony anywhere near, and no colony pool at all: DOS's
     * Fortify (FUN_2b5a_1112) has no harbour or water condition, so both the
     * ORDERS row and the F key must take a ship wherever it floats. This used
     * to need an own colony within one tile, which is what made it look like
     * fortifying at sea worked only sometimes (bugs.md).
     */
    units_clear_orders(&pool, ship);
    sh = units_get(&pool, ship);
    if (sh) {
      sh->moves = 4 * UNITS_MP_PER_TILE;
    }
    if (!units_order_fortify(&pool, ship) || !sh || sh->orders != UNITS_ORDER_FORTIFY) {
      fprintf(stderr, "anchor at sea failed orders=%d\n", sh ? sh->orders : -1);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_clear_orders(&pool, ship);
    sh = units_get(&pool, ship);
    if (sh) {
      sh->moves = 4 * UNITS_MP_PER_TILE;
    }
    if (!units_order_fortify(&pool, ship) || !sh || sh->orders != UNITS_ORDER_FORTIFY) {
      fprintf(stderr, "F-key fortify on a ship failed orders=%d\n", sh ? sh->orders : -1);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, ship);

    /* bugs.md #631: pillage never destroys improvements in DOS — the
     * non-colony arm must refuse and leave the road standing. */
    int px = -1, py = -1;
    for (int y = 1; y < (int)map.height - 1 && px < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && px < 0; ++x) {
        if (map_tile_is_land(&map, x, y) && !map_tile_is_high_seas(&map, x, y)) {
          px = x;
          py = y;
        }
      }
    }
    const int soldier = units_find_type(&pool, "Soldiers");
    const int mil = units_spawn(&pool, soldier >= 0 ? soldier : pioneer, px, py);
    ColonizeUnit* mu = units_get(&pool, mil);
    if (!mu || px < 0) {
      fprintf(stderr, "pillage spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    mu->nation_id = 0;
    mu->moves = 1 * UNITS_MP_PER_TILE;
    map_tile_set_road(&map, px, py, true);
    char pmsg[64];
    if (units_pillage_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, mil, pmsg, sizeof(pmsg)) ||
        !map_tile_has_road(&map, px, py)) {
      fprintf(stderr, "pillage road: must refuse and keep the road: %s\n", pmsg);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, mil);
  fx_close();
  return 0;
}

  /* Land combat T0: Soldier (atk2) vs Brave (def1) — attacker wins without RNG. */
static int case_land_combat_t0(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    const int soldier = units_find_type(&pool, "Soldiers");
    const int brave = units_find_type(&pool, "Braves");
    if (soldier < 0 || brave < 0) {
      fprintf(stderr, "missing Soldiers/Braves for combat test\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int ax = -1, ay = -1, dx = -1, dy = -1;
    for (int y = 1; y < (int)map.height - 1 && ax < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && ax < 0; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y)) {
          ax = x;
          ay = y;
          dx = x + 1;
          dy = y;
        }
      }
    }
    if (ax < 0) {
      fprintf(stderr, "no adjacent land for combat\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int aid = units_spawn(&pool, soldier, ax, ay);
    const int did = units_spawn_allow_stack(&pool, brave, dx, dy);
    ColonizeUnit* a = units_get(&pool, aid);
    ColonizeUnit* d = units_get(&pool, did);
    if (!a || !d) {
      fprintf(stderr, "combat spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    a->nation_id = 0;
    a->moves = 3 * UNITS_MP_PER_TILE;
    d->nation_id = 4;
    d->moves = 1 * UNITS_MP_PER_TILE;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 1);
    if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(&rng)}, aid, dx, dy)) {
      fprintf(stderr, "combat move failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (units_last_combat_outcome() != 1 || units_get_const(&pool, did) != NULL) {
      fprintf(stderr, "expected attacker win, outcome=%d\n", units_last_combat_outcome());
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* bugs.md #243: a land attacker does NOT advance into the vacated tile
     * (only ships and colony-capturing attacks enter). */
    a = units_get(&pool, aid);
    if (!a || a->x != ax || a->y != ay) {
      fprintf(stderr, "attacker should stay put after land win\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (a->moves >= 3 * UNITS_MP_PER_TILE) {
      fprintf(stderr, "attack should drain MP even when staying put\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    (void)units_despawn(&pool, aid);
  fx_close();
  return 0;
}

  /* units_follow_unit + advance one step (Brave escort API). */
static int case_follow_unit(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int fx = -1;
    int fy = -1;
    for (int y = 1; y + 2 < map.height && fx < 0; ++y) {
      for (int x = 1; x + 2 < map.width && fx < 0; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 2, y) &&
            units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 2, y) < 0) {
          fx = x;
          fy = y;
        }
      }
    }
    if (fx < 0) {
      fprintf(stderr, "follow setup: no free land pair\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int a = units_spawn_allow_stack(&pool, pioneer, fx, fy);
    const int b = units_spawn_allow_stack(&pool, pioneer, fx + 2, fy);
    if (a < 0 || b < 0) {
      fprintf(stderr, "follow setup spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* ua = units_get(&pool, a);
    ColonizeUnit* ub = units_get(&pool, b);
    if (!ua || !ub) {
      fprintf(stderr, "follow unit lookup failed\n");
      return 1;
    }
    ua->moves = 4 * UNITS_MP_PER_TILE;
    if (!units_follow_unit(&pool, a, b)) {
      fprintf(stderr, "units_follow_unit failed\n");
      return 1;
    }
    if (ua->orders != UNITS_ORDER_FOLLOW || ua->follow_unit_id != b) {
      fprintf(stderr, "follow order not set\n");
      return 1;
    }
    const int ax0 = ua->x;
    (void)units_advance_follow_one_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, a);
    ua = units_get(&pool, a);
    if (!ua || ua->orders != UNITS_ORDER_FOLLOW || ua->follow_unit_id != b) {
      fprintf(stderr, "follow not retained after step\n");
      return 1;
    }
    if (ua->x == ax0 && abs(ua->x - ub->x) > 1) {
      fprintf(stderr, "follow did not step toward target\n");
      return 1;
    }
    units_despawn(&pool, a);
    units_despawn(&pool, b);
  fx_close();
  return 0;
}

  /* Naval hold plunder on combat resolve (FUN_5fef_016c-shaped). */
static int case_naval_hold_plunder(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int wx = -1;
    int wy = -1;
    int lx = -1;
    int ly = -1;
    for (int y = 0; y < map.height && wx < 0; ++y) {
      for (int x = 0; x < map.width; ++x) {
        if (map_tile_is_water(&map, x, y)) {
          if (wx < 0) {
            wx = x;
            wy = y;
          } else if (abs(x - wx) + abs(y - wy) == 1) {
            lx = x;
            ly = y;
            break;
          }
        }
      }
    }
    const int privateer = units_find_type(&pool, "Privateer");
    const int merchant = units_find_type(&pool, "Merchantman");
    const int sty = privateer >= 0 ? privateer : caravel;
    const int lty = merchant >= 0 ? merchant : caravel;
    if (wx >= 0 && lx >= 0 && sty >= 0 && lty >= 0) {
      const int wid = units_spawn_allow_stack(&pool, sty, wx, wy);
      const int lid = units_spawn_allow_stack(&pool, lty, lx, ly);
      if (wid >= 0 && lid >= 0) {
        ColonizeUnit* w = units_get(&pool, wid);
        ColonizeUnit* l = units_get(&pool, lid);
        if (w && l) {
          w->nation_id = 0;
          l->nation_id = 1;
            (void)units_load_goods(&pool, lid, COLONIZE_CARGO_SUGAR, 40);
            const bool won = units_resolve_naval_combat(&pool, wid, lid, NULL);
          w = units_get(&pool, wid);
          l = units_get(&pool, lid);
          if (won) {
            if (!w || !w->active || (l && l->active)) {
              fprintf(stderr, "naval winner/loser active state wrong\n");
              return 1;
            }
            int sugar = 0;
            for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
              if (w->hold_goods_amount[h] > 0 && w->hold_goods_type[h] == COLONIZE_CARGO_SUGAR) {
                sugar += w->hold_goods_amount[h];
              }
            }
            if (sugar < 40) {
              fprintf(stderr, "naval plunder expected sugar>=40 got %d\n", sugar);
              return 1;
            }
          }
          if (w && w->active) {
            units_despawn(&pool, wid);
          }
          if (l && l->active) {
            units_despawn(&pool, lid);
          }
        }
      }
    }
  fx_close();
  return 0;
}

  /* Treasure train spawn: value lives in DOS +0x315b (profession) = gold/100
   * (bugs.md #736); no LE16 hold mirror any more. */
static int case_treasure_train_spawn(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    const int tid = units_spawn_treasure_train(&pool, 3, 3, 2, 3400);
    if (tid < 0) {
      fprintf(stderr, "spawn_treasure_train failed\n");
      return 1;
    }
    ColonizeUnit* tr = units_get(&pool, tid);
    const ColonizeUnitType* tt = tr ? units_type(&pool, tr->type_index) : NULL;
    if (!tr || !tr->active || !tt || strcmp(tt->name, "Treasure") != 0) {
      fprintf(stderr, "spawn_treasure_train type/active mismatch\n");
      return 1;
    }
    if (tr->nation_id != 2 || tr->profession != 34) {
      fprintf(
        stderr, "spawn_treasure_train nation/profession got %d/%d\n", tr->nation_id, tr->profession
      );
      return 1;
    }
    /* The value must survive with the (retired) hold mirror cleared — that is
     * exactly what a save round trip hands back. */
    tr->hold_goods_amount[0] = 0;
    tr->hold_goods_amount[1] = 0;
    if (units_treasure_value_gold(tr) != 3400) {
      fprintf(stderr, "treasure value want 3400 got %d\n", units_treasure_value_gold(tr));
      return 1;
    }
    if (units_spawn_treasure_train(&pool, 4, 4, 0, -1) >= 0) {
      fprintf(stderr, "spawn_treasure_train must reject negative gold\n");
      return 1;
    }
    units_despawn(&pool, tid);
  fx_close();
  return 0;
}

  /*
   * A Treasure only boards a hull with six free holds. That is the DOS rule
   * (FUN_4720_00e0 raw 74637-74665: room = 0x5237[hull] − goods, charged
   * 0x5238[type] per passenger, candidate accepted on `size <= room`), and
   * the manual's "Treasure needs a Galleon" is its emergent form — @UNIT
   * cargo is Caravel 2, Merchantman 4, Galleon 6. The old test asserted a
   * type-name `require_galleon` flag that DOS does not have.
   */
static int case_treasure_ship_holds(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    const int caravel_t = units_find_type(&pool, "Caravel");
    const int galleon_t = units_find_type(&pool, "Galleon");
    if (caravel_t < 0 || galleon_t < 0) {
      fprintf(stderr, "boardable-ship: Caravel/Galleon type missing\n");
      return 1;
    }
    const int caravel_id = units_spawn(&pool, caravel_t, 20, 20);
    ColonizeUnit* caravel = units_get(&pool, caravel_id);
    if (!caravel) {
      fprintf(stderr, "boardable-ship: Caravel spawn failed\n");
      return 1;
    }
    caravel->nation_id = 1;
    /* Only a Caravel present: a size-1 unit finds it, a size-6 Treasure does not. */
    if (units_find_boardable_ship(&pool, 20, 20, 1, 1) != caravel_id) {
      fprintf(stderr, "boardable-ship: plain search should find the Caravel\n");
      return 1;
    }
    if (units_find_boardable_ship(&pool, 20, 20, 1, 6) >= 0) {
      fprintf(stderr, "boardable-ship: Treasure-size search must reject a Caravel\n");
      return 1;
    }
    const int galleon_id = units_spawn_allow_stack(&pool, galleon_t, 20, 20);
    ColonizeUnit* galleon = units_get(&pool, galleon_id);
    if (!galleon) {
      fprintf(stderr, "boardable-ship: Galleon spawn failed\n");
      return 1;
    }
    galleon->nation_id = 1;
    if (units_find_boardable_ship(&pool, 20, 20, 1, 6) != galleon_id) {
      fprintf(stderr, "boardable-ship: size-6 search should now find the Galleon\n");
      return 1;
    }
    /*
     * Hold charge: one Treasure fills a Galleon (6 of 6). Six used to fit.
     */
    const int tr_t = units_find_type(&pool, "Treasure");
    if (tr_t < 0) {
      fprintf(stderr, "boardable-ship: Treasure type missing\n");
      return 1;
    }
    const int tr1 = units_spawn_allow_stack(&pool, tr_t, 20, 20);
    const int tr2 = units_spawn_allow_stack(&pool, tr_t, 20, 20);
    ColonizeUnit* t1 = units_get(&pool, tr1);
    ColonizeUnit* t2 = units_get(&pool, tr2);
    if (!t1 || !t2) {
      fprintf(stderr, "boardable-ship: Treasure spawn failed\n");
      return 1;
    }
    t1->nation_id = 1;
    t2->nation_id = 1;
    if (units_ship_free_passenger_slots(&pool, galleon_id) != 6) {
      fprintf(stderr, "boardable-ship: empty Galleon should show 6 free holds\n");
      return 1;
    }
    if (!units_board_stacked(&pool, tr1, galleon_id)) {
      fprintf(stderr, "boardable-ship: first Treasure must board the Galleon\n");
      return 1;
    }
    if (units_ship_free_passenger_slots(&pool, galleon_id) != 0) {
      fprintf(
        stderr,
        "boardable-ship: Treasure aboard must cost 6 holds (free=%d)\n",
        units_ship_free_passenger_slots(&pool, galleon_id)
      );
      return 1;
    }
    if (units_board_stacked(&pool, tr2, galleon_id)) {
      fprintf(stderr, "boardable-ship: second Treasure must not fit\n");
      return 1;
    }
    units_despawn(&pool, tr1);
    units_despawn(&pool, tr2);
    units_despawn(&pool, caravel_id);
    units_despawn(&pool, galleon_id);
  fx_close();
  return 0;
}

  /* Native settlement conquer: tribe remove + Cortes peels FUN_5fef_31ea gold. */
static int case_native_settlement_conquer(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.tribe_count = 1;
    col1.head.difficulty = 0;
    col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
    if (!col1.tribe) {
      fprintf(stderr, "tribe alloc failed\n");
      return 1;
    }
    col1.tribe[0].x = 10;
    col1.tribe[0].y = 10;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE; /* no convert-join RNG before Cortes */
    col1.head.founding_father[FF_HERNAN_CORTES] = 0;
    col1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] |=
      (uint8_t)(1u << (FF_HERNAN_CORTES % 8));

    ColonizeWorldMap tmap;
    memset(&tmap, 0, sizeof(tmap));
    tmap.width = 20;
    tmap.height = 20;
    tmap.layer3 = calloc(400, 1);
    if (!tmap.layer3) {
      free(col1.tribe);
      fprintf(stderr, "tmap layer3 alloc failed\n");
      return 1;
    }
    tmap.layer3[10 * 20 + 10] = (uint8_t)((4u << 4) | 1u);

    const int brave_ti = units_find_type(&pool, "Braves");
    const int soldier_ti = units_find_type(&pool, "Soldiers");
    if (brave_ti < 0 || soldier_ti < 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "Brave/Soldier type missing for conquer smoke\n");
      return 1;
    }

    const int bid = units_spawn_allow_stack(&pool, brave_ti, 10, 10);
    const int sid = units_spawn_allow_stack(&pool, soldier_ti, 10, 10);
    ColonizeUnit* brave = units_get(&pool, bid);
    ColonizeUnit* soldier = units_get(&pool, sid);
    if (!brave || !soldier) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer spawn failed\n");
      return 1;
    }
    brave->nation_id = 4;
    soldier->nation_id = 0;
    pool.types[soldier_ti].attack = 99;
    pool.types[brave_ti].defense = 1;

    ColonizeDosRng crng;
    dos_rng_seed(&crng, 42);
    units_set_native_fallout_context(&col1, &tmap, -1);
    if (col1.nation[0].villages_burned != 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: villages_burned should start 0\n");
      return 1;
    }
    if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&crng)}, sid, bid)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer combat: attacker should win\n");
      return 1;
    }
    /* Killing a map Brave does not destroy the dwelling (FUN_5fef_1b0e). */
    if (col1.head.tribe_count != 1) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: tribe should remain after map Brave death\n");
      return 1;
    }
    if (!units_try_native_settlement_fallout_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&tmap), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&crng)}, 0, 4, 10, 10, -1)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: fallout should destroy empty dwelling\n");
      return 1;
    }
    if (col1.head.tribe_count != 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: tribe should be removed (count=%u)\n", col1.head.tribe_count);
      return 1;
    }
    /* col1_save.h nation.villages_burned; reports.c villages_penalty. */
    if (col1.nation[0].villages_burned != 1) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(
        stderr,
        "conquer: villages_burned should be 1 got %u\n",
        col1.nation[0].villages_burned
      );
      return 1;
    }
    if ((tmap.layer3[10 * 20 + 10] >> 4) != 0x0f) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: village tile should be unowned 0xf\n");
      return 1;
    }
    int peel_gold = -1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->aboard_ship_id >= 0) {
        continue;
      }
      if (u->x == 10 && u->y == 10) {
        const ColonizeUnitType* tt = units_type(&pool, u->type_index);
        if (tt && strcmp(tt->name, "Treasure") == 0) {
          peel_gold = units_treasure_value_gold(u);
          break;
        }
      }
    }
    /* Diff 0 + Cortes + non-Spanish: amount 2..4 then ×1.5 → gold 300/400/600. */
    if (peel_gold != 300 && peel_gold != 400 && peel_gold != 600) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "Cortes peel treasure gold got %d want 300|400|600\n", peel_gold);
      return 1;
    }

    /* rich_capital (-0xcc) ← tribe.state.capital doubles amount before Cortes boost. */
    {
      ColonizeDosRng r0;
      ColonizeDosRng r1;
      dos_rng_seed(&r0, 99);
      dos_rng_seed(&r1, 99);
      const int plain = units_conquest_treasure_gold(&col1, 0, 4, &r0, 0);
      const int rich = units_conquest_treasure_gold(&col1, 0, 4, &r1, 1);
      if (plain <= 0 || rich <= plain) {
        free(tmap.layer3);
        free(col1.tribe);
        fprintf(stderr, "rich_capital peel plain=%d rich=%d (want rich>plain>0)\n", plain, rich);
        return 1;
      }
      fprintf(stderr, "unit_units: Cortes rich_capital plain=%d rich=%d ok\n", plain, rich);
    }

    /* bugs.md #549: the band is the razed tribe's tech, not the difficulty — a
     * tech-1 (Agrarian, Arawak) village on Viceroy pays at most 8 * 1.5 * 100. */
    {
      const uint8_t saved_diff = col1.head.difficulty;
      col1.head.difficulty = 4;
      col1.indian[2].tech = 1;
      for (uint32_t seed = 1; seed <= 64; ++seed) {
        ColonizeDosRng r;
        dos_rng_seed(&r, seed);
        const int g = units_conquest_treasure_gold(&col1, 0, 6, &r, 0);
        if (g < 0 || g > 1200) {
          free(tmap.layer3);
          free(col1.tribe);
          fprintf(stderr, "#549 tech-1 treasure got %d want <= 1200\n", g);
          return 1;
        }
      }
      col1.indian[2].tech = 0;
      col1.head.difficulty = saved_diff;
    }

    /* Known gold path: Cortes spawns treasure when caller supplies amount. */
    col1.head.tribe_count = 1;
    col1.tribe[0].x = 11;
    col1.tribe[0].y = 10;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    tmap.layer3[10 * 20 + 11] = (uint8_t)((4u << 4) | 1u);
    const int bid2 = units_spawn_allow_stack(&pool, brave_ti, 11, 10);
    const int sid2 = units_spawn_allow_stack(&pool, soldier_ti, 11, 10);
    brave = units_get(&pool, bid2);
    soldier = units_get(&pool, sid2);
    brave->nation_id = 4;
    soldier->nation_id = 0;
    units_set_native_fallout_context(&col1, &tmap, 500);
    if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, sid2, bid2)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer combat2 failed\n");
      return 1;
    }
    if (!units_try_native_settlement_fallout_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&tmap), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, 0, 4, 11, 10, 500)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer fallout2 failed\n");
      return 1;
    }
    int treasure_id = -1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->x != 11 || u->y != 10) {
        continue;
      }
      const ColonizeUnitType* tt = units_type(&pool, u->type_index);
      if (tt && strcmp(tt->name, "Treasure") == 0) {
        treasure_id = u->id;
        if (units_treasure_value_gold(u) != 500) {
          free(tmap.layer3);
          free(col1.tribe);
          fprintf(stderr, "Cortes treasure value mismatch (want 500)\n");
          return 1;
        }
        break;
      }
    }
    if (treasure_id < 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "Cortes conquest expected treasure when gold supplied\n");
      return 1;
    }
    if (col1.nation[0].villages_burned != 2) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(
        stderr,
        "conquer2: villages_burned should be 2 got %u\n",
        col1.nation[0].villages_burned
      );
      return 1;
    }
    units_despawn(&pool, treasure_id);
    units_set_native_fallout_context(NULL, NULL, -1);

    /*
     * bugs.md #381: conquest treasure is NOT Cortes-gated. Strip Cortes and burn
     * a tech-2 (Advanced) village, where FUN_5fef_31ea's amount is
     * unconditional — (roll 2..6 + 0 Cortes + 0 Spanish) * 10 * 100, i.e.
     * 2000..6000 gold — and a Treasure Train must still appear. The band is
     * the tribe's tech, not the difficulty (bugs.md #549).
     */
    col1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] &=
      (uint8_t)~(1u << (FF_HERNAN_CORTES % 8));
    col1.head.founding_father[FF_HERNAN_CORTES] = -1; /* unclaimed */
    col1.head.difficulty = 0;
    col1.indian[0].tech = 2;
    col1.head.tribe_count = 1;
    col1.tribe[0].x = 12;
    col1.tribe[0].y = 10;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    col1.tribe[0].state.capital = 0;
    tmap.layer3[10 * 20 + 12] = (uint8_t)((4u << 4) | 1u);
    ColonizeDosRng nrng;
    dos_rng_seed(&nrng, 7);
    if (!units_try_native_settlement_fallout_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&tmap), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&nrng)}, 0, 4, 12, 10, -1)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "no-Cortes conquer: fallout should destroy dwelling\n");
      return 1;
    }
    int nc_gold = -1;
    int nc_id = -1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->x != 12 || u->y != 10) {
        continue;
      }
      const ColonizeUnitType* tt = units_type(&pool, u->type_index);
      if (tt && strcmp(tt->name, "Treasure") == 0) {
        nc_gold = units_treasure_value_gold(u);
        nc_id = u->id;
        break;
      }
    }
    if (nc_gold < 2000 || nc_gold > 6000) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(
        stderr, "no-Cortes conquest treasure got %d want 2000..6000\n", nc_gold
      );
      return 1;
    }
    fprintf(stderr, "unit_units: non-Cortes conquest treasure %d ok\n", nc_gold);
    if (nc_id >= 0) {
      units_despawn(&pool, nc_id);
    }
    free(tmap.layer3);
    free(col1.tribe);
  fx_close();
  return 0;
}

  /* FUN_5fef_31ea convert-join: mission-owned tribe + Sepulveda/Spanish/Jesuit. */
static int case_convert_join(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.tribe_count = 1;
    col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
    if (!col1.tribe) {
      fprintf(stderr, "convert-join tribe alloc failed\n");
      return 1;
    }
    col1.tribe[0].x = 12;
    col1.tribe[0].y = 12;
    col1.tribe[0].nation_id = 5;
    /* Spanish (2) + Jesuit bit + Sepulveda → threshold 8+4+4=16 → always join. */
    col1.tribe[0].mission =
      (uint8_t)(2u | COL1_TRIBE_MISSION_JESUIT_BIT);
    col1.head.founding_father[FF_JUAN_DE_SEPULVEDA] = 2;
    col1.nation[2].founding_fathers[FF_JUAN_DE_SEPULVEDA / 8] |=
      (uint8_t)(1u << (FF_JUAN_DE_SEPULVEDA % 8));

    ColonizeWorldMap tmap;
    memset(&tmap, 0, sizeof(tmap));
    tmap.width = 20;
    tmap.height = 20;
    tmap.layer3 = calloc(400, 1);
    if (!tmap.layer3) {
      free(col1.tribe);
      fprintf(stderr, "convert-join tmap alloc failed\n");
      return 1;
    }
    tmap.layer3[12 * 20 + 12] = (uint8_t)((5u << 4) | 1u);

    const int brave_ti = units_find_type(&pool, "Braves");
    const int soldier_ti = units_find_type(&pool, "Soldiers");
    const int colonist_ti = units_find_type(&pool, "Colonists");
    if (brave_ti < 0 || soldier_ti < 0 || colonist_ti < 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join types missing\n");
      return 1;
    }

    const int bid = units_spawn_allow_stack(&pool, brave_ti, 12, 12);
    const int sid = units_spawn_allow_stack(&pool, soldier_ti, 12, 12);
    ColonizeUnit* brave = units_get(&pool, bid);
    ColonizeUnit* soldier = units_get(&pool, sid);
    if (!brave || !soldier) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join spawn failed\n");
      return 1;
    }
    brave->nation_id = 5;
    soldier->nation_id = 2;
    pool.types[soldier_ti].attack = 99;
    pool.types[brave_ti].defense = 1;

    ColonizeDosRng crng;
    dos_rng_seed(&crng, 1);
    units_set_native_fallout_context(&col1, &tmap, -1);
    if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&crng)}, sid, bid)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join combat: attacker should win\n");
      return 1;
    }
    if (!units_try_native_settlement_fallout_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&tmap), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&crng)}, 2, 5, 12, 12, -1)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join fallout failed\n");
      return 1;
    }
    int convert_id = -1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->aboard_ship_id >= 0) {
        continue;
      }
      if (u->x == 12 && u->y == 12 && u->nation_id == 2 && u->profession == 27) {
        convert_id = u->id;
        break;
      }
    }
    if (convert_id < 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "Sepulveda convert-join: expected Convert profession 27 on tile\n");
      return 1;
    }
    units_despawn(&pool, convert_id);

    /* No mission → no convert even with Sepulveda. */
    col1.head.tribe_count = 1;
    col1.tribe[0].x = 13;
    col1.tribe[0].y = 12;
    col1.tribe[0].nation_id = 5;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    tmap.layer3[12 * 20 + 13] = (uint8_t)((5u << 4) | 1u);
    const int bid2 = units_spawn_allow_stack(&pool, brave_ti, 13, 12);
    const int sid2 = units_spawn_allow_stack(&pool, soldier_ti, 13, 12);
    brave = units_get(&pool, bid2);
    soldier = units_get(&pool, sid2);
    brave->nation_id = 5;
    soldier->nation_id = 2;
    dos_rng_seed(&crng, 1);
    if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&crng)}, sid2, bid2)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join no-mission combat failed\n");
      return 1;
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->aboard_ship_id >= 0) {
        continue;
      }
      if (u->x == 13 && u->y == 12 && u->nation_id == 2 && u->profession == 27) {
        free(tmap.layer3);
        free(col1.tribe);
        fprintf(stderr, "convert-join must not spawn without mission\n");
        return 1;
      }
    }

    units_set_native_fallout_context(NULL, NULL, -1);
    free(tmap.layer3);
    free(col1.tribe);
    fprintf(stderr, "unit_units: Sepulveda convert-join ok\n");
  fx_close();
  return 0;
}

  /*
   * FUN_3844_0004 (raw 58268, bugs.md #725): a LONE Indian Convert on an open
   * tile ages unit +0x16 and vanishes once the byte passes 8. Treasure trains
   * are untouched — no DOS site expires one.
   */
static int case_convert_desertion(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    ColonizeWorldMap cmap;
    memset(&cmap, 0, sizeof(cmap));
    char cerr[128];
    if (!map_alloc(&cmap, 12, 12, cerr, sizeof(cerr))) {
      fprintf(stderr, "convert tick map_alloc failed: %s\n", cerr);
      return 1;
    }
    /* Settlement on (3,3) — layer2 bit 1 stands for FUN_281f_06be >= 0. */
    cmap.layer2[3 * 12 + 3] |= MAP_OCCUPANCY_HAS_CITY;

    const int cti = units_kind_type_index(&pool, UNITS_KIND_COLONIST);
    if (cti < 0) {
      map_free(&cmap);
      fprintf(stderr, "convert tick: no Colonists type\n");
      return 1;
    }
    const int lone = units_spawn_allow_stack(&pool, cti, 7, 7);
    ColonizeUnit* cu = units_get(&pool, lone);
    if (!cu) {
      map_free(&cmap);
      fprintf(stderr, "convert tick spawn failed\n");
      return 1;
    }
    units_set_nation(cu, 0);
    cu->profession = UNITS_JOB_CONVERT;
    cu->col1_counter16 = 0;
    for (int t = 0; t < 8; ++t) {
      if (units_tick_convert_outside_colony(&pool, &cmap, 0) != 0) {
        map_free(&cmap);
        fprintf(stderr, "convert should survive tick %d\n", t + 1);
        return 1;
      }
      cu = units_get(&pool, lone);
      if (!cu || !cu->active || cu->col1_counter16 != t + 1) {
        map_free(&cmap);
        fprintf(stderr, "convert counter want %d\n", t + 1);
        return 1;
      }
    }
    if (units_tick_convert_outside_colony(&pool, &cmap, 0) != 1) {
      map_free(&cmap);
      fprintf(stderr, "convert should despawn on tick 9\n");
      return 1;
    }
    if (units_get(&pool, lone) && units_get(&pool, lone)->active) {
      map_free(&cmap);
      fprintf(stderr, "convert still active after tick 9\n");
      return 1;
    }

    /* On a settlement tile: never counted. */
    const int inside = units_spawn_allow_stack(&pool, cti, 3, 3);
    cu = units_get(&pool, inside);
    units_set_nation(cu, 0);
    cu->profession = UNITS_JOB_CONVERT;
    cu->col1_counter16 = 7;
    /* Stacked with an escort: FUN_281f_08bc(unit,2) >= 2, also skipped. */
    const int escorted = units_spawn_allow_stack(&pool, cti, 9, 9);
    const int escort = units_spawn_allow_stack(&pool, cti, 9, 9);
    units_set_nation(units_get(&pool, escorted), 0);
    units_set_nation(units_get(&pool, escort), 0);
    units_get(&pool, escorted)->profession = UNITS_JOB_CONVERT;
    units_get(&pool, escorted)->col1_counter16 = 7;
    /* A Treasure train alone in the open must NOT be touched. */
    const int tid = units_spawn_treasure_train(&pool, 5, 5, 0, 100);
    units_get(&pool, tid)->col1_counter16 = 8;

    if (units_tick_convert_outside_colony(&pool, &cmap, 0) != 0) {
      map_free(&cmap);
      fprintf(stderr, "convert tick removed a protected unit\n");
      return 1;
    }
    if (units_get(&pool, inside)->col1_counter16 != 7 ||
        units_get(&pool, escorted)->col1_counter16 != 7) {
      map_free(&cmap);
      fprintf(stderr, "convert tick must not age gated Converts\n");
      return 1;
    }
    if (!units_get(&pool, tid)->active || units_get(&pool, tid)->col1_counter16 != 8) {
      map_free(&cmap);
      fprintf(stderr, "Treasure train must be untouched by 3844_0004\n");
      return 1;
    }
    units_despawn(&pool, inside);
    units_despawn(&pool, escorted);
    units_despawn(&pool, escort);
    units_despawn(&pool, tid);
    map_free(&cmap);
    fprintf(stderr, "unit_units: lone-Convert 3844_0004 tick ok\n");
  fx_close();
  return 0;
}

  /* Stockade/Fort/Fortress defense bonus in land combat + Treasure capture loot. */
static int case_fort_defense_bonus(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    colonies_set_occupancy_map(NULL);
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
    snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fort");
    snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Fortress");
    colonies.building_type_count = 3;
    ColonizeColony* col = &colonies.colonies[0];
    col->id = 0;
    col->active = true;
    col->nation_id = 0;
    col->x = 5;
    col->y = 5;
    col->population = 3;
    colonies.colony_count = 1;
    if (colonies_fortification_defense_bonus_percent(&colonies, col) != 0) {
      fprintf(stderr, "fort bonus should be 0 without buildings\n");
      return 1;
    }
    col->has_building[0] = true;
    if (colonies_fortification_defense_bonus_percent(&colonies, col) != 100) {
      fprintf(stderr, "Stockade bonus want 100\n");
      return 1;
    }
    col->has_building[1] = true;
    if (colonies_fortification_defense_bonus_percent(&colonies, col) != 150) {
      fprintf(stderr, "Fort bonus want 150\n");
      return 1;
    }
    col->has_building[2] = true;
    if (colonies_fortification_defense_bonus_percent(&colonies, col) != 200) {
      fprintf(stderr, "Fortress bonus want 200\n");
      return 1;
    }

    const int soldier_ti = units_find_type(&pool, "Soldiers");
    if (soldier_ti < 0) {
      fprintf(stderr, "Soldiers missing for fort-defense smoke\n");
      return 1;
    }
    /* Deterministic: attack 2→×8=16 →×3/2=24 vs Stockade def base2→16 ×2=32 → loses. */
    pool.types[soldier_ti].attack = 2;
    pool.types[soldier_ti].defense = 2;
    const int atk_id = units_spawn_allow_stack(&pool, soldier_ti, 5, 5);
    const int def_id = units_spawn_allow_stack(&pool, soldier_ti, 5, 5);
    ColonizeUnit* atk = units_get(&pool, atk_id);
    ColonizeUnit* defu = units_get(&pool, def_id);
    if (!atk || !defu) {
      fprintf(stderr, "fort-defense spawn failed\n");
      return 1;
    }
    atk->nation_id = 1;
    defu->nation_id = 0;
    defu->orders = UNITS_ORDER_NONE;
    col->has_building[0] = true;
    col->has_building[1] = false;
    col->has_building[2] = false;
    units_set_combat_colonies(&colonies);
    /* FUN_157e_015e Stockade local_1a=4 → ((4+4)*16)>>2=32 > atk ((0+4)*16>>2)*3>>1=24. */
    if (units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL), .rng=(ColonizeDosRng*)(NULL)}, atk_id, def_id)) {
      fprintf(stderr, "Stockade defense should beat attack 2 vs base 2\n");
      return 1;
    }
    if (!units_get(&pool, def_id) || !units_get(&pool, def_id)->active) {
      fprintf(stderr, "Stockade defender should survive\n");
      return 1;
    }
    units_set_combat_colonies(NULL);

    /* Veteran + fortify stack peels (FUN_157e_004a / 015e). */
    {
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = &pool;
      sctx.colonies = &colonies;
      const int vti = units_find_type(&pool, "Soldiers");
      const int vid = units_spawn_allow_stack(&pool, vti, 6, 6);
      ColonizeUnit* vu = units_get(&pool, vid);
      if (!vu) {
        fprintf(stderr, "vet spawn failed\n");
        return 1;
      }
      vu->nation_id = 0;
      vu->profession = UNITS_JOB_SOLDIER;
      pool.types[vti].defense = 2;
      ColonizeCombatSideFlags fl;
      const int base = combat_unit_base_x8(&sctx, vid, 0, &fl);
      /* 2*8=16 +50% vet = 24 */
      if (base != 24 || (fl.flags & COMBAT_FLAG_VETERAN) == 0) {
        fprintf(stderr, "vet base×8 want 24+flag got %d flags=%x\n", base, fl.flags);
        return 1;
      }
      vu->orders = UNITS_ORDER_FORTIFIED;
      col->x = 6;
      col->y = 6;
      col->has_building[0] = true;
      col->has_building[1] = false;
      col->has_building[2] = false;
      const int eng = combat_engagement_strength(&sctx, vid, -1, &fl);
      /* Stockade local_1a=4 + fortify +2 → 6; ((6+4)*24)>>2 = 60 */
      if (eng != 60) {
        fprintf(stderr, "Stockade+fortify engagement want 60 got %d\n", eng);
        return 1;
      }
      units_despawn(&pool, vid);

      /*
       * Veteran DRAGOONS carry profession 0x17, never 0x15 — FUN_157e_004a
       * grants them the same +50%. (Regression: the peel used to test 0x15
       * only, so every veteran dragoon fought as a green one.)
       */
      const int dti = units_find_type(&pool, "Dragoons");
      if (dti < 0) {
        fprintf(stderr, "vet-dragoon: Dragoons type missing\n");
        return 1;
      }
      const int did2 = units_spawn_allow_stack(&pool, dti, 6, 7);
      ColonizeUnit* du = units_get(&pool, did2);
      if (!du) {
        fprintf(stderr, "vet-dragoon spawn failed\n");
        return 1;
      }
      du->nation_id = 0;
      pool.types[dti].defense = 2;
      ColonizeCombatSideFlags dfl;
      du->profession = UNITS_JOB_NONE;
      const int green = combat_unit_base_x8(&sctx, did2, 0, &dfl);
      if (green != 16 || (dfl.flags & COMBAT_FLAG_VETERAN) != 0) {
        fprintf(stderr, "green dragoon want 16 no-flag got %d flags=%x\n", green, dfl.flags);
        return 1;
      }
      /* bugs.md #503: a veteran dragoon body is @UNIT type 4 carrying
       * profession 0x15 — DOS never writes 0x17 to a unit, so 0x17 must NOT
       * peel (FUN_157e_004a raw 8942-8944). */
      du->profession = UNITS_JOB_DRAGOON;
      const int fake = combat_unit_base_x8(&sctx, did2, 0, &dfl);
      if (fake != 16 || (dfl.flags & COMBAT_FLAG_VETERAN) != 0) {
        fprintf(stderr, "prof 0x17 must not peel, got %d flags=%x\n", fake, dfl.flags);
        return 1;
      }
      du->profession = UNITS_JOB_SOLDIER;
      const int vet = combat_unit_base_x8(&sctx, did2, 0, &dfl);
      if (vet != 24 || (dfl.flags & COMBAT_FLAG_VETERAN) == 0) {
        fprintf(stderr, "vet dragoon want 24+flag got %d flags=%x\n", vet, dfl.flags);
        return 1;
      }
      units_despawn(&pool, did2);
    }

    /* Combat Analysis gate. */
    {
      ColonizeCol1Save ag;
      memset(&ag, 0, sizeof(ag));
      ag.player[0].control = 0;
      ag.player[1].control = 1;
      if (combat_analysis_should_show(&ag, 0, 1, 0)) {
        fprintf(stderr, "analysis should be off when option clear\n");
        return 1;
      }
      ag.head.game_options.combat_analysis = 1;
      if (!combat_analysis_should_show(&ag, 0, 1, 0)) {
        fprintf(stderr, "analysis should show for human attacker\n");
        return 1;
      }
      if (combat_analysis_should_show(&ag, 1, 1, 0)) {
        fprintf(stderr, "analysis should skip AI-only fight\n");
        return 1;
      }
      /* Header = baseline; Attack Bonus listed for land attacker. */
      {
        ColonizeCombatEngagement eng;
        memset(&eng, 0, sizeof(eng));
        eng.attacker_id = 0;
        eng.defender_id = 0;
        eng.atk_strength = 24;
        eng.def_strength = 16;
        eng.is_naval = false;
        eng.atk_flags.base_combat = 2;
        eng.atk_flags.flags = COMBAT_FLAG_MODE_ATK | COMBAT_FLAG_VETERAN;
        eng.def_flags.base_combat = 1;
        CombatAnalysisDialog dlg;
        memset(&dlg, 0, sizeof(dlg));
        if (!combat_analysis_open(&dlg, &pool, &eng)) {
          fprintf(stderr, "combat_analysis_open failed\n");
          return 1;
        }
        if (dlg.eng.atk_flags.base_combat != 2 || dlg.eng.def_flags.base_combat != 1) {
          fprintf(stderr, "analysis should keep baseline combat bytes\n");
          return 1;
        }
        int found_atk_bonus = 0;
        int found_vet = 0;
        for (int i = 0; i < dlg.atk_line_count; ++i) {
          if (strstr(dlg.atk_rows[i].label, "Attack Bonus")) {
            found_atk_bonus = 1;
          }
          if (strstr(dlg.atk_rows[i].label, "Veteran")) {
            found_vet = 1;
          }
        }
        if (!found_atk_bonus || !found_vet) {
          fprintf(stderr, "land analysis missing Attack Bonus/Veteran lines\n");
          return 1;
        }
        /* Village Attack same-click: unarmed until mouse up / idle frame. */
        {
          ColonizeInputState in;
          memset(&in, 0, sizeof(in));
          in.mouse_left_down = true;
          in.mouse_left_clicked = true;
          (void)combat_analysis_handle_input(&dlg, &in);
          if (!dlg.open || dlg.arm_input) {
            fprintf(stderr, "analysis should stay open unarmed while click held\n");
            return 1;
          }
          memset(&in, 0, sizeof(in));
          (void)combat_analysis_handle_input(&dlg, &in);
          if (!dlg.arm_input || !dlg.open) {
            fprintf(stderr, "analysis should arm after idle frame\n");
            return 1;
          }
          in.mouse_left_clicked = true;
          (void)combat_analysis_handle_input(&dlg, &in);
          if (dlg.open) {
            fprintf(stderr, "analysis should dismiss on armed click\n");
            return 1;
          }
        }
        if (!combat_analysis_open(&dlg, &pool, &eng)) {
          fprintf(stderr, "combat_analysis_open re-open failed\n");
          return 1;
        }
        for (int i = 0; i < dlg.def_line_count; ++i) {
          if (strstr(dlg.def_rows[i].label, "Attack Bonus")) {
            fprintf(stderr, "defender must not list Attack Bonus\n");
            return 1;
          }
        }
        combat_analysis_close(&dlg);
        eng.is_naval = true;
        if (!combat_analysis_open(&dlg, &pool, &eng)) {
          fprintf(stderr, "naval combat_analysis_open failed\n");
          return 1;
        }
        /*
         * Naval attackers DO get the ×3/2 attack factor and DOS prints its
         * row: FUN_5fef_1b0e applies the scale unconditionally
         * (viceroy_unpacked.c 100457-100458 — 1b0e is the one resolver for
         * both domains) and FUN_636c_0000 walks DS:0x8d00 bit 0
         * (viceroy_unpacked.c 101874-101891), which FUN_157e_004a sets on
         * every mode-1 evaluation (8926-8928). No domain test anywhere.
         */
        {
          int naval_atk_bonus = 0;
          for (int i = 0; i < dlg.atk_line_count; ++i) {
            if (strstr(dlg.atk_rows[i].label, "Attack Bonus")) {
              naval_atk_bonus = 1;
            }
          }
          if (!naval_atk_bonus) {
            fprintf(stderr, "naval analysis must list Attack Bonus (DOS 636c bit 0)\n");
            return 1;
          }
        }
        combat_analysis_close(&dlg);
      }
      fprintf(stderr, "unit_units: combat analysis gate ok\n");
    }

    /* bugs.md #660: treasure capture flips nation, credits no gold. */
    ColonizeCol1Save tcol1;
    memset(&tcol1, 0, sizeof(tcol1));
    tcol1.nation[1].gold = 50;
    int use_ti = units_find_type(&pool, "Treasure");
    if (use_ti < 0) {
      if (pool.type_count >= (int)(sizeof(pool.types) / sizeof(pool.types[0]))) {
        fprintf(stderr, "no room for Treasure type\n");
        return 1;
      }
      use_ti = pool.type_count;
      snprintf(pool.types[use_ti].name, sizeof(pool.types[use_ti].name), "Treasure");
      pool.types[use_ti].domain = COLONIZE_UNIT_DOMAIN_LAND;
      pool.types[use_ti].defense = 0;
      pool.types[use_ti].attack = 0;
      pool.type_count++;
    }
    const int capturer = units_spawn_allow_stack(&pool, soldier_ti, 6, 6);
    const int loot_id = units_spawn_allow_stack(&pool, use_ti, 6, 6);
    ColonizeUnit* cap = units_get(&pool, capturer);
    ColonizeUnit* loot = units_get(&pool, loot_id);
    if (!cap || !loot) {
      fprintf(stderr, "treasure capture spawn failed\n");
      return 1;
    }
    cap->nation_id = 1;
    loot->nation_id = 0;
    loot->hold_goods_amount[0] = 200 & 0xff;
    loot->hold_goods_amount[1] = (200 >> 8) & 0xff;
    pool.types[soldier_ti].attack = 5;
    pool.types[use_ti].defense = 1;
    if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&tcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, capturer, loot_id)) {
      fprintf(stderr, "treasure capture combat should win\n");
      return 1;
    }
    if (tcol1.nation[1].gold != 50) {
      fprintf(stderr, "treasure capture must credit no gold, got %u\n", tcol1.nation[1].gold);
      return 1;
    }
    {
      const ColonizeUnit* lu = units_get_const(&pool, loot_id);
      if (!lu || !lu->active || lu->nation_id != 1) {
        fprintf(stderr, "captured Treasure should change hands and live\n");
        return 1;
      }
    }
    fprintf(stderr, "unit_units: fortification defense + treasure capture ok\n");
  fx_close();
  return 0;
}

  /* Coastal Fort/Fortress naval fire (FUN_364b_03f6). */
static int case_coastal_fort_naval_fire(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    colonies_set_occupancy_map(NULL);
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
    snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fort");
    snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Fortress");
    colonies.building_type_count = 3;

    int cx = -1, cy = -1, wx = -1, wy = -1;
    for (int y = 1; y < map.height - 1 && cx < 0; ++y) {
      for (int x = 1; x < map.width - 1; ++x) {
        if (!map_tile_is_land(&map, x, y)) {
          continue;
        }
        static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        for (int d = 0; d < 8; ++d) {
          const int nx = x + dx[d];
          const int ny = y + dy[d];
          if (map_tile_is_water(&map, nx, ny)) {
            cx = x;
            cy = y;
            wx = nx;
            wy = ny;
            break;
          }
        }
        if (cx >= 0) {
          break;
        }
      }
    }
    if (cx < 0) {
      fprintf(stderr, "no coastal tile for fort-fire smoke\n");
      return 1;
    }

    ColonizeColony* col = &colonies.colonies[0];
    col->id = 0;
    col->active = true;
    col->nation_id = 0;
    col->x = cx;
    col->y = cy;
    col->population = 3;
    colonies.colony_count = 1;
    col->has_building[1] = true; /* Fort */

    if (units_coastal_fort_attack_strength(&colonies, col, &pool) != 4) {
      fprintf(stderr, "Fort strength want 4 got %d\n",
              units_coastal_fort_attack_strength(&colonies, col, &pool));
      return 1;
    }
    col->has_building[2] = true; /* Fortress overrides */
    if (units_coastal_fort_attack_strength(&colonies, col, &pool) != 8) {
      fprintf(stderr, "Fortress strength want 8 got %d\n",
              units_coastal_fort_attack_strength(&colonies, col, &pool));
      return 1;
    }
    const int art_ti = units_find_type(&pool, "Artillery");
    if (art_ti < 0) {
      fprintf(stderr, "Artillery type missing for fort-fire smoke\n");
      return 1;
    }
    const int art_id = units_spawn_allow_stack(&pool, art_ti, cx, cy);
    ColonizeUnit* art = units_get(&pool, art_id);
    if (!art) {
      fprintf(stderr, "Artillery spawn failed\n");
      return 1;
    }
    art->nation_id = 0;
    if (units_coastal_fort_attack_strength(&colonies, col, &pool) != 16) {
      fprintf(stderr, "Fortress+1 arty want 16 got %d\n",
              units_coastal_fort_attack_strength(&colonies, col, &pool));
      return 1;
    }
    units_despawn(&pool, art_id);
    col->has_building[2] = false; /* Fort only, strength 4 */

    ColonizeCol1Save fcol1;
    memset(&fcol1, 0, sizeof(fcol1));
    /* Unclaimed FF slots must be -1 (memset 0 → nation 0 falsely owns Franklin). */
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      fcol1.head.founding_father[i] = -1;
    }
    /* bugs.md #867 / FUN_5fef_0352 raw 99531-99548: an UNARMED loser now goes
     * through the AI fleet-pool bias, which forces "damaged" when the owner's
     * spare cargo capacity (stuff.ship_cargo_totals) is below clamp(pop>>2,3,6).
     * A blank fixture census reads 0 and would always save the hull, so give
     * nation 1 a fleet with room to spare and keep this case a SINK. */
    fcol1.stuff.ship_cargo_totals[1] = 60;
    ai_diplo_declare_war(&fcol1, 0, 1);
    if (!ai_diplo_at_war(&fcol1, 0, 1)) {
      fprintf(stderr, "fort-fire smoke: declare_war 0 vs 1 failed\n");
      return 1;
    }

    const int caravel_ti = units_find_type(&pool, "Caravel");
    if (caravel_ti < 0) {
      fprintf(stderr, "Caravel missing for fort-fire smoke\n");
      return 1;
    }
    const int old_def = pool.types[caravel_ti].defense;
    const int old_hull = pool.types[caravel_ti].hull;
    pool.types[caravel_ti].defense = 2; /* Fort atk 4 >= 2 → fort wins */
    /* bugs.md #249: fort win rolls damage-vs-sink on the hull (no-rng
     * fallback: hull >= atk → damaged). Hull 0 keeps this check a sink. */
    pool.types[caravel_ti].hull = 0;

    const int foe_id = units_spawn_allow_stack(&pool, caravel_ti, wx, wy);
    ColonizeUnit* foe = units_get(&pool, foe_id);
    if (!foe) {
      fprintf(stderr, "enemy ship spawn failed\n");
      return 1;
    }
    foe->nation_id = 1;

    char fort_status[80];
    fort_status[0] = '\0';
    const int sunk =
      units_coastal_fort_fire_pulse_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&fcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, 0, fort_status, sizeof(fort_status));
    if (sunk < 1 || (units_get(&pool, foe_id) && units_get(&pool, foe_id)->active)) {
      fprintf(stderr, "Fort at war should sink adjacent enemy ship (sunk=%d)\n", sunk);
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    if (strstr(fort_status, "sank") == NULL) {
      fprintf(stderr, "fort fire want sank status got '%s'\n", fort_status);
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }

    /* bugs.md #249: fort miss leaves the ship UNTOUCHED — no MP drain (DOS
     * undoes the temp attacker; the old ship-slow was invented). */
    ai_diplo_declare_war(&fcol1, 0, 1);
    pool.types[caravel_ti].defense = 100; /* fort atk 4 loses */
    const int slow_id = units_spawn_allow_stack(&pool, caravel_ti, wx, wy);
    foe = units_get(&pool, slow_id);
    if (!foe) {
      fprintf(stderr, "ship-slow spawn failed\n");
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    foe->nation_id = 1;
    foe->moves = 4 * UNITS_MP_PER_TILE;
    if (units_coastal_fort_fire_pulse_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&fcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, -1, NULL, 0) != 0) {
      fprintf(stderr, "ship-slow: fort should miss high-def ship\n");
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    foe = units_get(&pool, slow_id);
    if (!foe || !foe->active || foe->moves != 4 * UNITS_MP_PER_TILE) {
      fprintf(
        stderr,
        "fort miss: want untouched ship (moves=%d) got active=%d moves=%d\n",
        4 * UNITS_MP_PER_TILE,
        foe ? foe->active : 0,
        foe ? foe->moves : -1
      );
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    units_despawn(&pool, slow_id);
    pool.types[caravel_ti].defense = 2;

    /* Peace: no fire (unless Privateer). */
    ai_diplo_make_peace(&fcol1, 0, 1);
    const int peace_id = units_spawn_allow_stack(&pool, caravel_ti, wx, wy);
    foe = units_get(&pool, peace_id);
    foe->nation_id = 1;
    if (units_coastal_fort_fire_pulse_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&fcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, -1, NULL, 0) != 0) {
      fprintf(stderr, "Fort at peace should not sink Caravel\n");
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    units_despawn(&pool, peace_id);

    /* bugs.md #465 (user-observed: Spanish Caravel shot at while F8 showed
     * Spain at peace): the battery obeys DOS's PEACE bit (0x40, the byte F8
     * reads), not the Linux WAR bit — a stale WAR bit beside PEACE must not
     * fire. */
    ai_diplo_or_both(&fcol1, 0, 1, AI_DIPLO_WAR);
    const int stale_id = units_spawn_allow_stack(&pool, caravel_ti, wx, wy);
    foe = units_get(&pool, stale_id);
    foe->nation_id = 1;
    if (units_coastal_fort_fire_pulse_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&fcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, -1, NULL, 0) != 0) {
      fprintf(stderr, "Fort with PEACE set (stale WAR bit) must not fire\n");
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    units_despawn(&pool, stale_id);
    ai_diplo_clear_both(&fcol1, 0, 1, AI_DIPLO_WAR);

    const int priv_ti = units_find_type(&pool, "Privateer");
    if (priv_ti >= 0) {
      const int old_pdef = pool.types[priv_ti].defense;
      const int old_phull = pool.types[priv_ti].hull;
      pool.types[priv_ti].defense = 2;
      /* bugs.md #249: fort win now rolls damage-vs-sink on the ship's hull
       * (no-rng fallback: hull > fort atk → damaged). Zero the hull so this
       * deterministic check still ends in a sink. */
      pool.types[priv_ti].hull = 0;
      const int pid = units_spawn_allow_stack(&pool, priv_ti, wx, wy);
      ColonizeUnit* pr = units_get(&pool, pid);
      pr->nation_id = 1;
      const int psunk =
        units_coastal_fort_fire_pulse_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&fcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, -1, NULL, 0);
      if (psunk < 1 || (units_get(&pool, pid) && units_get(&pool, pid)->active)) {
        fprintf(stderr, "Fort should sink Privateer at peace\n");
        pool.types[priv_ti].defense = old_pdef;
        pool.types[priv_ti].hull = old_phull;
        pool.types[caravel_ti].defense = old_def;
        return 1;
      }
      pool.types[priv_ti].defense = old_pdef;
      pool.types[priv_ti].hull = old_phull;
    }

    pool.types[caravel_ti].defense = old_def;
    pool.types[caravel_ti].hull = old_hull;
    fprintf(stderr, "unit_units: coastal fort naval fire ok\n");
  fx_close();
  return 0;
}

  /* LCR rumour: clear + de Soto reveal path. AMER2's rumour nearest the
   * old (8,14) fixture moved to (9,15) on 2026-09-09 when
   * map_procedural_rumour_at dropped the unverified +1 coordinate bias its
   * resource-hash sibling had already lost (smell_audit #98). */
static int case_lcr_clear_desoto(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    if (!map_tile_has_rumour(&map, 9, 15)) {
      fprintf(stderr, "AMER2 (9,15) expected procedural rumour\n");
      return 1;
    }
    ColonizeCol1Save lcol1;
    memset(&lcol1, 0, sizeof(lcol1));
    const int scout_ti = units_find_type(&pool, "Scouts");
    if (scout_ti < 0) {
      fprintf(stderr, "Scout type missing for LCR smoke\n");
      return 1;
    }
    const int scid = units_spawn_allow_stack(&pool, scout_ti, 9, 15);
    ColonizeUnit* scout = units_get(&pool, scid);
    if (!scout) {
      fprintf(stderr, "LCR scout spawn failed\n");
      return 1;
    }
    scout->nation_id = 0;
    if (!units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&lcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(NULL)}, scid, -1)) {
      fprintf(stderr, "LCR resolve without de Soto failed\n");
      return 1;
    }
    if (map_tile_has_rumour(&map, 9, 15)) {
      fprintf(stderr, "LCR rumour should be cleared\n");
      return 1;
    }
    lcol1.head.founding_father[FF_HERNANDO_DE_SOTO] = 0;
    lcol1.nation[0].founding_fathers[FF_HERNANDO_DE_SOTO / 8] |=
      (uint8_t)(1u << (FF_HERNANDO_DE_SOTO % 8));
    ColonizeWorldMap lmap;
    memset(&lmap, 0, sizeof(lmap));
    lmap.width = map.width;
    lmap.height = map.height;
    const size_t n = (size_t)map.width * (size_t)map.height;
    lmap.terrain = malloc(n);
    lmap.layer2 = calloc(n, 1);
    lmap.layer3 = calloc(n, 1);
    lmap.seen = calloc(n, 1);
    if (!lmap.terrain || !lmap.layer2 || !lmap.layer3 || !lmap.seen) {
      free(lmap.terrain);
      free(lmap.layer2);
      free(lmap.layer3);
      free(lmap.seen);
      fprintf(stderr, "LCR mini-map alloc failed\n");
      return 1;
    }
    memcpy(lmap.terrain, map.terrain, n);
    if (!map_tile_has_rumour(&lmap, 9, 15)) {
      fprintf(stderr, "LCR fresh map (9,15) expected rumour\n");
      map_free(&lmap);
      return 1;
    }
    const int scid2 = units_spawn_allow_stack(&pool, scout_ti, 9, 15);
    scout = units_get(&pool, scid2);
    scout->nation_id = 0;
    if (!units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&lmap), .col1=(ColonizeCol1Save*)(&lcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(NULL)}, scid2, -1)) {
      map_free(&lmap);
      fprintf(stderr, "LCR resolve with de Soto failed\n");
      return 1;
    }
    if (map_tile_has_rumour(&lmap, 9, 15)) {
      map_free(&lmap);
      fprintf(stderr, "de Soto LCR rumour not cleared\n");
      return 1;
    }
    if (!map_tile_seen_by(&lmap, 9, 15, 0)) {
      map_free(&lmap);
      fprintf(stderr, "de Soto LCR should reveal scout tile\n");
      return 1;
    }
    units_despawn(&pool, scid);
    units_despawn(&pool, scid2);
    map_free(&lmap);
  fx_close();
  return 0;
}

static int case_lcr_case_matrix(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    /* lcr_case5_bonus_used: first vanish (raw case 5) roll → burial
     * (FUN_65dd_0004:103608). Non-Scout, no FF: units_lcr_roll_outcome's
     * base roll is a single un-rerolled pass (P7.1's real state machine —
     * old flat RNG(1,100) percentage table is gone), so the very first
     * dos_rng_range(rng,1,9) call landing on 5 is enough to hit raw case 5
     * directly (skill 0 means the case-5 "kicker" at 103476-103483 is a
     * trivial RNG(1,1), always sticks). Brute-force via the real resolve
     * call so this stays correct regardless of the exact RNG call shape. */
    ColonizeCol1Save c5col1;
    memset(&c5col1, 0, sizeof(c5col1));
    for (int i = 0; i < COLONIZE_COL1_FF_COUNT; ++i) {
      c5col1.head.founding_father[i] = -1;
    }
    const int case5_ti = units_find_type(&pool, "Colonists");
    if (case5_ti < 0) {
      fprintf(stderr, "case5 latch colonist type missing\n");
      return 1;
    }
    uint32_t trespass_seed = 0;
    for (uint32_t s = 1; s < 50000u && trespass_seed == 0; ++s) {
      ColonizeDosRng probe;
      dos_rng_seed(&probe, s);
      if (dos_rng_range(&probe, 1, 9) == 5) {
        trespass_seed = s;
      }
    }
    if (trespass_seed == 0) {
      fprintf(stderr, "case5 latch could not find raw-case-5 RNG seed\n");
      return 1;
    }
    int rx1 = -1;
    int ry1 = -1;
    int rx2 = -1;
    int ry2 = -1;
    for (int y = 0; y < (int)map.height; ++y) {
      for (int x = 0; x < (int)map.width; ++x) {
        if (!map_tile_has_rumour(&map, x, y)) {
          continue;
        }
        if (rx1 < 0) {
          rx1 = x;
          ry1 = y;
        } else if (x != rx1 || y != ry1) {
          rx2 = x;
          ry2 = y;
          break;
        }
      }
      if (rx2 >= 0) {
        break;
      }
    }
    if (rx1 < 0 || rx2 < 0) {
      fprintf(stderr, "case5 latch need two rumour tiles on AMER2\n");
      return 1;
    }
    ColonizeDosRng rng1;
    dos_rng_seed(&rng1, trespass_seed);
    const int sc1 = units_spawn_allow_stack(&pool, case5_ti, rx1, ry1);
    ColonizeUnit* u1 = units_get(&pool, sc1);
    if (!u1) {
      fprintf(stderr, "case5 latch scout spawn failed\n");
      return 1;
    }
    u1->nation_id = 0;
    if (!units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&c5col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng1), .europe=(EuropeScreen*)(NULL)}, sc1, 0)) {
      fprintf(stderr, "case5 latch first resolve failed\n");
      return 1;
    }
    if (!c5col1.player[0].lcr_case5_bonus_used) {
      fprintf(stderr, "case5 latch must set lcr_case5_bonus_used\n");
      return 1;
    }
    map.layer2[ry2 * map.width + rx2] &= (uint8_t)~MAP_LAYER2_LCR_CONSUMED;
    ColonizeDosRng rng2;
    dos_rng_seed(&rng2, trespass_seed);
    const int sc2 = units_spawn_allow_stack(&pool, case5_ti, rx2, ry2);
    ColonizeUnit* u2 = units_get(&pool, sc2);
    if (!u2) {
      fprintf(stderr, "case5 latch scout2 spawn failed\n");
      return 1;
    }
    u2->nation_id = 0;
    if (!units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&c5col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng2), .europe=(EuropeScreen*)(NULL)}, sc2, 0)) {
      fprintf(stderr, "case5 latch second resolve failed\n");
      return 1;
    }
    units_despawn(&pool, sc1);
    units_despawn(&pool, sc2);
    fprintf(stderr, "unit_units: lcr_case5_bonus_used latch ok\n");
  fx_close();
  return 0;
}

static int case_lcr_case5_latch(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    /*
     * P7.1: de Soto's LCR bonus is gated on the explorer being Scout-type
     * (FUN_65dd_0004:103454-103458 — the FF7 check only fires when
     * local_36!=0, and local_36 starts 0 for any non-Scout unit type).
     * A Colonist should therefore roll byte-identically (same RNG
     * consumption, same side effects) whether or not the nation owns de
     * Soto, since de_soto_reroll can never become true for it. Regression
     * test for that gate: run the same seed on the same tile with de Soto
     * off, then on, and require identical gold/colonist-join/vanish side
     * effects both times. */
    int rtx = -1;
    int rty = -1;
    for (int y = 0; y < (int)map.height && rtx < 0; ++y) {
      for (int x = 0; x < (int)map.width; ++x) {
        if (map_tile_has_rumour(&map, x, y)) {
          rtx = x;
          rty = y;
          break;
        }
      }
    }
    if (colonist < 0 || rtx < 0) {
      fprintf(stderr, "de Soto gate test: missing colonist type or rumour tile\n");
      return 1;
    }
    const uint32_t probe_seed = 4242;
    uint32_t gold_delta[2] = {0, 0};
    int colonist_delta[2] = {0, 0};
    bool vanished[2] = {false, false};
    for (int pass = 0; pass < 2; ++pass) {
      ColonizeCol1Save pcol1;
      memset(&pcol1, 0, sizeof(pcol1));
      for (int i = 0; i < COLONIZE_COL1_FF_COUNT; ++i) {
        pcol1.head.founding_father[i] = -1;
      }
      if (pass == 1) {
        pcol1.head.founding_father[FF_HERNANDO_DE_SOTO] = 0;
        pcol1.nation[0].founding_fathers[FF_HERNANDO_DE_SOTO / 8] |=
          (uint8_t)(1u << (FF_HERNANDO_DE_SOTO % 8));
      }
      ColonizeDosRng prng;
      dos_rng_seed(&prng, probe_seed);
      const int uid = units_spawn_allow_stack(&pool, colonist, rtx, rty);
      ColonizeUnit* pu = units_get(&pool, uid);
      if (!pu) {
        fprintf(stderr, "de Soto gate test: spawn failed (pass %d)\n", pass);
        return 1;
      }
      pu->nation_id = 0;
      const uint32_t gold_before = pcol1.nation[0].gold;
      int colonist_before = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        if (pool.units[i].active && pool.units[i].type_index == colonist) {
          colonist_before++;
        }
      }
      if (!units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&pcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(&prng), .europe=(EuropeScreen*)(NULL)}, uid, 0)) {
        fprintf(stderr, "de Soto gate test: resolve failed (pass %d)\n", pass);
        return 1;
      }
      ColonizeUnit* after = units_get(&pool, uid);
      vanished[pass] = !after || !after->active;
      gold_delta[pass] = pcol1.nation[0].gold - gold_before;
      int colonist_after = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        if (pool.units[i].active && pool.units[i].type_index == colonist) {
          colonist_after++;
        }
      }
      colonist_delta[pass] = colonist_after - colonist_before;
      if (!vanished[pass]) {
        units_despawn(&pool, uid);
      }
      map.layer2[rty * map.width + rtx] &= (uint8_t)~MAP_LAYER2_LCR_CONSUMED;
    }
    if (vanished[0] != vanished[1] || gold_delta[0] != gold_delta[1] ||
        colonist_delta[0] != colonist_delta[1]) {
      fprintf(
        stderr,
        "de Soto gate test: non-Scout outcome differs with/without de Soto "
        "(vanish %d/%d gold %u/%u colonist-delta %d/%d)\n",
        vanished[0], vanished[1], gold_delta[0], gold_delta[1],
        colonist_delta[0], colonist_delta[1]
      );
      return 1;
    }
    fprintf(stderr, "unit_units: LCR de Soto Scout-type gate ok\n");
  fx_close();
  return 0;
}

  /*
   * LCR outcome dispatch: real seeded RNG (not the rng==NULL fallback above)
   * over every rumour tile on the map, one fresh Scout per tile. Confirms
   * each real-effect branch actually fires at least once: gold credited,
   * Fountain of Youth dock immigrants, a Treasure train spawned (Cibola /
   * Burial3), a colonist joining (survivors), and a vanished scout.
   */
static int case_lcr_outcome_dispatch(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    EuropeScreen eu;
    char eerr[256];
    if (!europe_load(&eu, "COLONIZE", eerr, sizeof(eerr))) {
      fprintf(stderr, "LCR dispatch: europe_load failed: %s\n", eerr);
      return 1;
    }
    const int scout_ti = units_find_type(&pool, "Scouts");
    if (scout_ti < 0) {
      fprintf(stderr, "LCR dispatch: Scout type missing\n");
      europe_free(&eu);
      return 1;
    }
    ColonizeCol1Save dcol1;
    memset(&dcol1, 0, sizeof(dcol1));
    /* founding_father[]==0 would misread as "nation 0 elected FF 0" (the
     * real sentinel is -1 = unrecruited) and force every trial down the de
     * Soto positive-only branch. */
    for (int i = 0; i < COLONIZE_COL1_FF_COUNT; ++i) {
      dcol1.head.founding_father[i] = -1;
    }
    /* FUN_65dd_0004:103597 downgrades Vanishes to Nothing for a nation
     * with < 5 census pop and < 3 colonies — give nation 0 an established
     * footprint so the vanish branch is reachable at all. */
    dcol1.stuff.census_pop_proxy[0] = 20;
    dcol1.stuff.colony_counts[0] = 3;
    bool saw_gold = false;
    bool saw_fountain = false;
    bool saw_treasure = false;
    bool saw_survivor = false;
    bool saw_vanish = false;
    const int treasure_ti = units_find_type(&pool, "Treasure");
    /* One persistent RNG advanced across every trial — reseeding per trial
     * with sequential seeds would just resample the LCG's early outputs and
     * skew the distribution, not exercise it. */
    ColonizeDosRng drng;
    dos_rng_seed(&drng, 12345);
    int trials = 0;
    for (int pass = 0; pass < 15 && trials < 400; ++pass) {
      for (int y = 0; y < (int)map.height && trials < 400; ++y) {
        for (int x = 0; x < (int)map.width && trials < 400; ++x) {
          if (!map_tile_has_rumour(&map, x, y)) {
            continue;
          }
          trials++;
          const uint32_t gold_before = dcol1.nation[0].gold;
          const int dock_before = eu.dock_count;
          int treasure_before = 0;
          int colonist_before = 0;
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            if (!pool.units[i].active) continue;
            if (treasure_ti >= 0 && pool.units[i].type_index == treasure_ti) treasure_before++;
            if (pool.units[i].type_index == colonist) colonist_before++;
          }
          const int sid = units_spawn_allow_stack(&pool, scout_ti, x, y);
          ColonizeUnit* su = units_get(&pool, sid);
          if (!su) {
            continue;
          }
          su->nation_id = 0;
          if (!units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&dcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(&drng), .europe=(EuropeScreen*)(&eu)}, sid, 0)) {
            fprintf(stderr, "LCR dispatch: resolve failed at (%d,%d)\n", x, y);
            europe_free(&eu);
            return 1;
          }
        if (dcol1.nation[0].gold > gold_before) {
          saw_gold = true;
        }
        if (eu.dock_count >= dock_before + 8) {
          saw_fountain = true;
        }
        int treasure_after = 0;
        int colonist_after = 0;
        bool scout_active = false;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          if (!pool.units[i].active) continue;
          if (treasure_ti >= 0 && pool.units[i].type_index == treasure_ti) treasure_after++;
          if (pool.units[i].type_index == colonist) colonist_after++;
          if (pool.units[i].id == sid) scout_active = true;
        }
        if (treasure_after > treasure_before) {
          saw_treasure = true;
        }
        if (colonist_after > colonist_before) {
          saw_survivor = true;
        }
        if (!scout_active) {
          saw_vanish = true;
        }
        if (scout_active) {
          units_despawn(&pool, sid);
        }
        /* Re-arm for the next pass over the same tile set (test-only). */
        map.layer2[y * map.width + x] &= (uint8_t)~MAP_LAYER2_LCR_CONSUMED;
        }
      }
    }
    europe_free(&eu);
    if (trials < 10) {
      fprintf(stderr, "LCR dispatch: too few rumour tiles on AMER2 (%d)\n", trials);
      return 1;
    }
    if (!saw_gold || !saw_fountain || !saw_treasure || !saw_survivor || !saw_vanish) {
      fprintf(
        stderr,
        "LCR dispatch: missing outcome over %d trials (gold=%d fountain=%d treasure=%d survivor=%d vanish=%d)\n",
        trials,
        saw_gold,
        saw_fountain,
        saw_treasure,
        saw_survivor,
        saw_vanish
      );
      return 1;
    }
    fprintf(stderr, "unit_units: LCR outcome dispatch ok (%d trials)\n", trials);
  fx_close();
  return 0;
}

  /*
   * bugs.md #496: Fountain of Youth runs the eight FUN_291f_0d2c(1,0) picks
   * for an AI nation too (raw 103725-103731 sits outside the `local_a != 0`
   * human gate; 4884 just takes pool slot 1 for a non-human bound nation).
   * The eight land in the Europe limbo (200,100), never on the human dock.
   */
static int case_lcr_fountain_ai(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    EuropeScreen eu;
    char eerr[256];
    if (!europe_load(&eu, "COLONIZE", eerr, sizeof(eerr))) {
      fprintf(stderr, "LCR AI FoY: europe_load failed: %s\n", eerr);
      return 1;
    }
    const int scout_ti = units_find_type(&pool, "Scouts");
    bool foy_was_active[COLONIZE_UNITS_MAX];
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      foy_was_active[i] = pool.units[i].active;
    }
    ColonizeCol1Save acol1;
    memset(&acol1, 0, sizeof(acol1));
    for (int i = 0; i < COLONIZE_COL1_FF_COUNT; ++i) {
      acol1.head.founding_father[i] = -1; /* zeroed FF[] = "nation 0 has FF 0" */
    }
    acol1.player[0].control = 0; /* human */
    acol1.player[1].control = 1; /* the AI explorer's nation */
    acol1.stuff.census_pop_proxy[1] = 20;
    acol1.stuff.colony_counts[1] = 3;
    for (int s = 0; s < 3; ++s) {
      acol1.nation[1].recruit[s] = 0x13; /* Free Colonist in every pool slot */
    }
    ColonizeDosRng arng;
    dos_rng_seed(&arng, 12345 * 7);
    int trials = 0;
    bool saw_ai_fountain = false;
    const int dock_before_all = eu.dock_count;
    for (int pass = 0; pass < 15 && trials < 400 && !saw_ai_fountain; ++pass) {
      for (int y = 0; y < (int)map.height && trials < 400 && !saw_ai_fountain; ++y) {
        for (int x = 0; x < (int)map.width && trials < 400 && !saw_ai_fountain; ++x) {
          if (!map_tile_has_rumour(&map, x, y)) {
            continue;
          }
          trials++;
          int limbo_before = 0;
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            if (pool.units[i].active && pool.units[i].nation_id == 1 &&
                pool.units[i].x == 200 && pool.units[i].y == 100) {
              limbo_before++;
            }
          }
          const int sid = units_spawn_allow_stack(&pool, scout_ti, x, y);
          ColonizeUnit* su = units_get(&pool, sid);
          if (!su) {
            continue;
          }
          su->nation_id = 1;
          if (!units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&acol1), .col1_ok=true, .rng=(ColonizeDosRng*)(&arng), .europe=(EuropeScreen*)(&eu)}, sid, 0)) {
            fprintf(stderr, "LCR AI FoY: resolve failed at (%d,%d)\n", x, y);
            europe_free(&eu);
            return 1;
          }
          int limbo_after = 0;
          bool scout_active = false;
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            if (!pool.units[i].active) continue;
            if (pool.units[i].nation_id == 1 && pool.units[i].x == 200 &&
                pool.units[i].y == 100) {
              limbo_after++;
            }
            if (pool.units[i].id == sid) scout_active = true;
          }
          if (limbo_after >= limbo_before + 8) {
            saw_ai_fountain = true;
          } else if (limbo_after != limbo_before) {
            fprintf(
              stderr, "LCR AI FoY: partial limbo spawn %d -> %d\n", limbo_before, limbo_after
            );
            europe_free(&eu);
            return 1;
          }
          if (scout_active) {
            units_despawn(&pool, sid);
          }
          map.layer2[y * map.width + x] &= (uint8_t)~MAP_LAYER2_LCR_CONSUMED;
        }
      }
    }
    const int dock_after_all = eu.dock_count;
    europe_free(&eu);
    if (!saw_ai_fountain) {
      fprintf(stderr, "LCR AI FoY: no AI Fountain over %d trials\n", trials);
      return 1;
    }
    if (dock_after_all != dock_before_all) {
      fprintf(stderr, "LCR AI FoY: AI picks leaked onto the human dock\n");
      return 1;
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      if (pool.units[i].active && !foy_was_active[i]) {
        units_despawn(&pool, pool.units[i].id); /* limbo immigrants, treasures */
      }
    }
    fprintf(stderr, "unit_units: LCR AI Fountain of Youth picks ok (%d trials)\n", trials);
  fx_close();
  return 0;
}

  /*
   * bugs.md #497: case-8 burial trespass only stands (and only costs
   * relation) when the village's tribe has MET the explorer's nation
   * (FUN_281f_0a38 & 0x20, raw 103563-103565); unmet falls to "Nothing".
   * `lcr_case5_bonus_used` is pre-latched so the once-per-nation burial arm
   * (the only other alarm source in this routine) never fires: every alarm
   * point below is a trespass hit.
   */
static int case_lcr_burial_trespass(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    const int scout_ti = units_find_type(&pool, "Scouts");
    bool tre_was_active[COLONIZE_UNITS_MAX];
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      tre_was_active[i] = pool.units[i].active;
    }
    AiPopupState pops;
    ai_popup_init(&pops);
    units_set_combat_popups(&pops, test_game_txt());
    int trespass_popups[2] = {0, 0};
    int trial_count[2] = {0, 0};
    for (int met = 0; met < 2; ++met) {
      ColonizeCol1Save tcol1;
      memset(&tcol1, 0, sizeof(tcol1));
      for (int i = 0; i < COLONIZE_COL1_FF_COUNT; ++i) {
        tcol1.head.founding_father[i] = -1;
      }
      tcol1.stuff.census_pop_proxy[0] = 20;
      tcol1.stuff.colony_counts[0] = 3;
      ColonizeCol1Tribe tribes[1];
      memset(tribes, 0, sizeof(tribes));
      tribes[0].nation_id = 4;
      tcol1.tribe = tribes;
      tcol1.head.tribe_count = 1;
      tcol1.indian[0].euro_diplo[0] = met ? COL1_INDIAN_MET_BIT : 0;
      ColonizeDosRng trng;
      dos_rng_seed(&trng, 12345 * 11);
      int trials = 0;
      for (int pass = 0; pass < 8 && trials < 200; ++pass) {
        for (int y = 0; y < (int)map.height && trials < 200; ++y) {
          for (int x = 0; x < (int)map.width && trials < 200; ++x) {
            if (!map_tile_has_rumour(&map, x, y)) {
              continue;
            }
            trials++;
            /* Village on the rumour tile: distance 0, so every case-8 roll
             * reaches the met gate. */
            tribes[0].x = (uint8_t)x;
            tribes[0].y = (uint8_t)y;
            const int sid = units_spawn_allow_stack(&pool, scout_ti, x, y);
            ColonizeUnit* su = units_get(&pool, sid);
            if (!su) {
              continue;
            }
            su->nation_id = 0;
            pops.queue_count = 0;
            if (!units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&tcol1), .col1_ok=true, .rng=(ColonizeDosRng*)(&trng), .europe=(EuropeScreen*)(NULL)}, sid, 0)) {
              fprintf(stderr, "LCR trespass: resolve failed at (%d,%d)\n", x, y);
              units_set_combat_popups(NULL, NULL);
              return 1;
            }
            for (int q = 0; q < pops.queue_count; ++q) {
              if (test_body_is_section(pops.queue[q].body, "LOSTCITY8")) {
                trespass_popups[met]++;
              }
            }
            if (units_get(&pool, sid)) {
              units_despawn(&pool, sid);
            }
            map.layer2[y * map.width + x] &= (uint8_t)~MAP_LAYER2_LCR_CONSUMED;
          }
        }
      }
      trial_count[met] = trials;
      tcol1.tribe = NULL;
      tcol1.head.tribe_count = 0;
    }
    pops.queue_count = 0;
    units_set_combat_popups(NULL, NULL);
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      if (pool.units[i].active && !tre_was_active[i]) {
        units_despawn(&pool, pool.units[i].id);
      }
    }
    if (trespass_popups[0] != 0 || trespass_popups[1] <= 0) {
      fprintf(
        stderr,
        "LCR trespass: met gate not applied (unmet %d, met %d over %d/%d trials)\n",
        trespass_popups[0], trespass_popups[1], trial_count[0], trial_count[1]
      );
      return 1;
    }
    fprintf(
      stderr,
      "unit_units: LCR case-8 met gate ok (unmet %d, met %d @LOSTCITY8 over %d trials)\n",
      trespass_popups[0], trespass_popups[1], trial_count[1]
    );
  fx_close();
  return 0;
}

  /* Enter-probe matrix: bounce / domain / land combat / naval / capture. */
static int case_enter_probe_matrix(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    const int pioneer_t = pioneer;
    const int soldier = units_find_type(&pool, "Soldiers");
    const int brave = units_find_type(&pool, "Braves");
    const int caravel_t = caravel;
    const int colonist_t = colonist;
    if (pioneer_t < 0 || soldier < 0 || brave < 0 || caravel_t < 0) {
      fprintf(stderr, "enter-probe: missing unit types\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int ax = -1, ay = -1, dx = -1, dy = -1;
    for (int y = 1; y < (int)map.height - 1 && ax < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && ax < 0; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y) &&
            units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
          ax = x;
          ay = y;
          dx = x + 1;
          dy = y;
        }
      }
    }
    if (ax < 0) {
      fprintf(stderr, "enter-probe: no free land pair\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /* Friendly stack → OK; can_enter true. */
    {
      const int a = units_spawn(&pool, colonist_t, ax, ay);
      const int b = units_spawn_allow_stack(&pool, pioneer_t, ax, ay);
      ColonizeUnit* ua = units_get(&pool, a);
      ColonizeUnit* ub = units_get(&pool, b);
      if (!ua || !ub) {
        fprintf(stderr, "enter-probe friendly spawn failed\n");
        return 1;
      }
      ua->nation_id = 0;
      ub->nation_id = 0;
      const ColonizeEnterReason r =
        units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, ub->type_index, ax, ay, b);
      if (r != COLONIZE_ENTER_OK || !units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, ub->type_index, ax, ay, b)) {
        fprintf(stderr, "enter-probe friendly expected OK got %d\n", (int)r);
        return 1;
      }
      units_despawn(&pool, a);
      units_despawn(&pool, b);
    }

    /* Pioneer into Brave → bounce (non-combat). */
    {
      const int pid = units_spawn(&pool, pioneer_t, ax, ay);
      const int bid = units_spawn_allow_stack(&pool, brave, dx, dy);
      ColonizeUnit* p = units_get(&pool, pid);
      ColonizeUnit* br = units_get(&pool, bid);
      if (!p || !br) {
        fprintf(stderr, "enter-probe bounce spawn failed\n");
        return 1;
      }
      p->nation_id = 0;
      br->nation_id = 4;
      p->moves = 3 * UNITS_MP_PER_TILE;
      const ColonizeEnterReason r =
        units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, p->type_index, dx, dy, pid);
      if (r != COLONIZE_ENTER_BOUNCE_FOREIGN) {
        fprintf(stderr, "enter-probe pioneer bounce expected got %d\n", (int)r);
        return 1;
      }
      if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, pid, dx, dy)) {
        fprintf(stderr, "enter-probe pioneer should not enter Brave tile\n");
        return 1;
      }
      if (units_last_enter_reason() != COLONIZE_ENTER_BOUNCE_FOREIGN) {
        fprintf(stderr, "enter-probe last reason bounce got %d\n", (int)units_last_enter_reason());
        return 1;
      }
      units_despawn(&pool, pid);
      units_despawn(&pool, bid);
    }

    /* Soldier into Brave → combat land; can_enter false. */
    {
      const int sid = units_spawn(&pool, soldier, ax, ay);
      const int bid = units_spawn_allow_stack(&pool, brave, dx, dy);
      ColonizeUnit* s = units_get(&pool, sid);
      ColonizeUnit* br = units_get(&pool, bid);
      if (!s || !br) {
        fprintf(stderr, "enter-probe combat spawn failed\n");
        return 1;
      }
      s->nation_id = 0;
      br->nation_id = 4;
      s->moves = 3 * UNITS_MP_PER_TILE;
      const ColonizeEnterReason r =
        units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, s->type_index, dx, dy, sid);
      if (r != COLONIZE_ENTER_COMBAT_LAND) {
        fprintf(stderr, "enter-probe combat land expected got %d\n", (int)r);
        return 1;
      }
      if (units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, s->type_index, dx, dy, sid)) {
        fprintf(stderr, "enter-probe can_enter should be false for combat dest\n");
        return 1;
      }
      units_despawn(&pool, sid);
      units_despawn(&pool, bid);
    }

    /* Domain deny: land into ocean when adjacent water exists. */
    {
      int ox = -1, oy = -1;
      for (int y = 0; y < (int)map.height && ox < 0; ++y) {
        for (int x = 0; x < (int)map.width && ox < 0; ++x) {
          /* Interior tiles only: a rim tile denies with BLOCKED_EDGE before the
           * domain check ever runs (map_coords_inset / DOS FUN_137f_000a). */
          if (map_coords_inset(&map, x, y) && map_tile_is_water(&map, x, y) &&
              abs(x - ax) <= 1 && abs(y - ay) <= 1 && !(x == ax && y == ay)) {
            ox = x;
            oy = y;
          }
        }
      }
      if (ox >= 0) {
        const int pid = units_spawn(&pool, pioneer_t, ax, ay);
        ColonizeUnit* p = units_get(&pool, pid);
        if (p) {
          p->nation_id = 0;
          const ColonizeEnterReason r =
            units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, p->type_index, ox, oy, pid);
          if (r != COLONIZE_ENTER_BLOCKED_DOMAIN) {
            fprintf(stderr, "enter-probe domain deny expected got %d\n", (int)r);
            return 1;
          }
        }
        units_despawn(&pool, pid);
      }
    }

    /* Capture-on-enter: soldier onto empty foreign colony. */
    {
      ColonizeColonyPool colonies;
      colonies_init(&colonies);
      colonies_set_occupancy_map(NULL);
      if (!colonies_load_names(&colonies, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&colonies, &names)) {
        fprintf(stderr, "enter-probe colonies init failed\n");
        return 1;
      }
      const int cid =
        colonies_found(&colonies, &map, dx, dy, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
      if (cid < 0) {
        fprintf(stderr, "enter-probe found rival colony failed\n");
        return 1;
      }
      ColonizeColony* col = colonies_get_mut(&colonies, cid);
      if (col) {
        col->nation_id = 1;
        col->population = 1;
      }
      const int sid = units_spawn(&pool, soldier, ax, ay);
      ColonizeUnit* s = units_get(&pool, sid);
      if (!s) {
        fprintf(stderr, "enter-probe capture soldier spawn failed\n");
        return 1;
      }
      s->nation_id = 0;
      s->moves = 5 * UNITS_MP_PER_TILE;
      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, sid, dx, dy)) {
        fprintf(stderr, "enter-probe capture move failed reason=%d\n", (int)units_last_enter_reason());
        return 1;
      }
      col = colonies_get_mut(&colonies, cid);
      if (!col || col->nation_id != 0) {
        fprintf(
          stderr,
          "enter-probe capture expected nation 0 got %d\n",
          col ? col->nation_id : -1
        );
        return 1;
      }
      units_despawn(&pool, sid);
      (void)colonist_t;
    }

    /* Naval move-into combat. */
    {
      int wx = -1, wy = -1, wx2 = -1, wy2 = -1;
      for (int y = 1; y < (int)map.height - 1 && wx < 0; ++y) {
        for (int x = 1; x < (int)map.width - 1 && wx < 0; ++x) {
          if (map_tile_is_water(&map, x, y) && map_tile_is_water(&map, x + 1, y) &&
              units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
            wx = x;
            wy = y;
            wx2 = x + 1;
            wy2 = y;
          }
        }
      }
      if (wx < 0) {
        fprintf(stderr, "enter-probe: no water pair for naval\n");
        return 1;
      }
      const int a = units_spawn(&pool, caravel_t, wx, wy);
      const int b = units_spawn_allow_stack(&pool, caravel_t, wx2, wy2);
      ColonizeUnit* ua = units_get(&pool, a);
      ColonizeUnit* ub = units_get(&pool, b);
      if (!ua || !ub) {
        fprintf(stderr, "enter-probe naval spawn failed\n");
        return 1;
      }
      ua->nation_id = 0;
      ub->nation_id = 1;
      ua->moves = 4 * UNITS_MP_PER_TILE;
      /*
       * bugs.md: unarmed transports (Caravel attack=0 in @UNIT) must bounce
       * off a foreign ship, not fight — "Only Privateers and Frigates can
       * attack enemy ships" (GAME.TXT). Confirm the bounce before arming the
       * type below to exercise the actual-combat path.
       */
      const ColonizeEnterReason unarmed_r =
        units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, ua->type_index, wx2, wy2, a);
      if (unarmed_r != COLONIZE_ENTER_BOUNCE_FOREIGN) {
        fprintf(stderr, "enter-probe unarmed naval bounce expected got %d\n", (int)unarmed_r);
        return 1;
      }
      pool.types[caravel_t].attack = 99;
      pool.types[caravel_t].defense = 1;
      /* Real Caravels carry guns 0 (@UNIT col 9) — a gunless victor can only
       * damage, never sink (DOS 0352). Arm the type so the win clears the
       * tile and the move-through completes. */
      pool.types[caravel_t].guns = 99;
      const ColonizeEnterReason r =
        units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, ua->type_index, wx2, wy2, a);
      if (r != COLONIZE_ENTER_COMBAT_NAVAL) {
        fprintf(stderr, "enter-probe naval combat expected got %d\n", (int)r);
        return 1;
      }
      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, a, wx2, wy2)) {
        fprintf(stderr, "enter-probe naval move combat failed\n");
        return 1;
      }
      if (units_get_const(&pool, b) != NULL) {
        fprintf(stderr, "enter-probe naval defender should be gone\n");
        return 1;
      }
      ua = units_get(&pool, a);
      if (!ua || ua->x != wx2 || ua->y != wy2) {
        fprintf(stderr, "enter-probe naval winner should occupy tile\n");
        return 1;
      }
      units_despawn(&pool, a);

      /*
       * DOS: an attacker is FULLY exhausted for the turn, ships included —
       * 1b0e runs FUN_281f_0934 (spent = max allotment) under the attack
       * flag before the roll (viceroy_unpacked.c:100381-100383, FUN_1427_155e
       * at :8880-8888), and 465b's step cost is attack-skipped (75639-75640).
       * The old "ship-slow survives the surcharge" model was a port
       * invention (seventh-wave attack-MP pass, 2026-09-10).
       */
      const int saved_movement = pool.types[caravel_t].movement;
      pool.types[caravel_t].movement = 8;
      const int sa = units_spawn(&pool, caravel_t, wx, wy);
      const int sb = units_spawn_allow_stack(&pool, caravel_t, wx2, wy2);
      ColonizeUnit* sua = units_get(&pool, sa);
      ColonizeUnit* sub = units_get(&pool, sb);
      if (!sua || !sub) {
        fprintf(stderr, "attack-exhaust spawn failed\n");
        return 1;
      }
      sua->nation_id = 0;
      sub->nation_id = 1;
      sua->moves = 8 * UNITS_MP_PER_TILE;
      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, sa, wx2, wy2)) {
        fprintf(stderr, "attack-exhaust move combat failed\n");
        return 1;
      }
      sua = units_get(&pool, sa);
      if (!sua || sua->moves != 0) {
        fprintf(
          stderr,
          "attack-exhaust expected 0 moves left after naval win, got %d\n",
          sua ? sua->moves : -1
        );
        return 1;
      }
      units_despawn(&pool, sa);
      pool.types[caravel_t].movement = saved_movement;
      fprintf(stderr, "unit_units: naval attack full MP exhaust ok\n");
    }
  fx_close();
  return 0;
}

  /* Land → ocean with own ship → BOARD; sentry auto-load when ship leaves. */
static int case_board_from_land_sentry(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int lx = -1, ly = -1, wx = -1, wy = -1, wx2 = -1, wy2 = -1;
    for (int y = 1; y < (int)map.height - 1 && lx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && lx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y)) {
          continue;
        }
        static const int kdx[4] = {1, -1, 0, 0};
        static const int kdy[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
          const int nx = x + kdx[d];
          const int ny = y + kdy[d];
          /* @SHIPLAKE: skip inland lake water — this sub-test is exercising
           * plain boarding/sentry mechanics, not the lake gate. */
          if (!map_tile_is_water(&map, nx, ny) || map_tile_is_lake(&map, nx, ny) ||
              units_id_at(&pool, x, y) >= 0 || units_id_at(&pool, nx, ny) >= 0) {
            continue;
          }
          /* Need a second adjacent water tile for the ship to leave to. */
          int ox = -1, oy = -1;
          for (int e = 0; e < 8; ++e) {
            static const int edx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
            static const int edy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
            const int tx = nx + edx[e];
            const int ty = ny + edy[e];
            if (map_tile_is_water(&map, tx, ty) && !map_tile_is_lake(&map, tx, ty) &&
                (tx != x || ty != y) && units_id_at(&pool, tx, ty) < 0) {
              ox = tx;
              oy = ty;
              break;
            }
          }
          if (ox < 0) {
            continue;
          }
          lx = x;
          ly = y;
          wx = nx;
          wy = ny;
          wx2 = ox;
          wy2 = oy;
          break;
        }
      }
    }
    if (lx < 0) {
      fprintf(stderr, "board-enter: no land/water/water triple\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    const int land_id = units_spawn(&pool, pioneer, lx, ly);
    const int ship_id = units_spawn(&pool, units_find_type(&pool, "Caravel"), wx, wy);
    ColonizeUnit* land = units_get(&pool, land_id);
    ColonizeUnit* ship = units_get(&pool, ship_id);
    if (!land || !ship) {
      fprintf(stderr, "board-enter spawn failed\n");
      return 1;
    }
    land->nation_id = 0;
    ship->nation_id = 0;
    land->moves = 3 * UNITS_MP_PER_TILE;
    ship->moves = 4 * UNITS_MP_PER_TILE;

    const ColonizeEnterReason br =
      units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, land->type_index, wx, wy, land_id);
    if (br != COLONIZE_ENTER_BOARD) {
      fprintf(stderr, "board-enter probe expected BOARD got %d\n", (int)br);
      return 1;
    }
    if (units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, land->type_index, wx, wy, land_id)) {
      fprintf(stderr, "board-enter can_enter should stay false (goto)\n");
      return 1;
    }
    if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, land_id, wx, wy)) {
      fprintf(stderr, "board-enter try_move failed\n");
      return 1;
    }
    land = units_get(&pool, land_id);
    ship = units_get(&pool, ship_id);
    if (!land || !ship || land->aboard_ship_id != ship_id || ship->cargo_count != 1) {
      fprintf(stderr, "board-enter state wrong cargo=%d aboard=%d\n",
              ship ? ship->cargo_count : -1, land ? land->aboard_ship_id : -1);
      return 1;
    }
    /*
     * bugs.md #423: walking aboard from open shore is DOS's 465b_05ca
     * force-to-max — that passenger is spent, so FUN_4720_015c offers it no
     * landfall and the unload is refused until the next turn.
     */
    if (!land->mp_spent_turn) {
      fprintf(stderr, "board-enter: shore boarding must mark the pax spent\n");
      return 1;
    }
    if (units_cargo_can_landfall(&pool, land_id) ||
        units_first_landfall_cargo(&pool, ship_id) >= 0) {
      fprintf(stderr, "board-enter: spent pax must not be landfall-eligible\n");
      return 1;
    }
    if (units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, ship_id, land_id, lx, ly)) {
      fprintf(stderr, "board-enter: spent pax must not make landfall\n");
      return 1;
    }
    fprintf(stderr, "board-enter spent pax stays aboard ok\n");
    /* Next turn (DOS day top clears the spent byte): unload onto land for
     * the sentry auto-board test. */
    land->mp_spent_turn = 0;
    if (units_first_landfall_cargo(&pool, ship_id) != land_id) {
      fprintf(stderr, "board-enter: fresh parked pax must be landfall-eligible\n");
      return 1;
    }
    if (!units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, ship_id, land_id, lx, ly)) {
      fprintf(stderr, "board-enter unload failed\n");
      return 1;
    }
    land = units_get(&pool, land_id);
    ship = units_get(&pool, ship_id);
    if (!land || !ship) {
      fprintf(stderr, "board-enter after unload missing\n");
      return 1;
    }
    /* Ocean sentry orphan on ship tile; ship departs → auto-board. */
    land->x = wx;
    land->y = wy;
    land->orders = UNITS_ORDER_SENTRY;
    land->moves = 0;
    ship->x = wx;
    ship->y = wy;
    ship->moves = 4 * UNITS_MP_PER_TILE;
    if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, ship_id, wx2, wy2)) {
      fprintf(stderr, "sentry-board ship move failed reason=%d\n", (int)units_last_enter_reason());
      return 1;
    }
    land = units_get(&pool, land_id);
    ship = units_get(&pool, ship_id);
    if (!land || !ship || land->aboard_ship_id != ship_id || ship->cargo_count < 1) {
      fprintf(stderr, "sentry-board expected auto-load aboard=%d cargo=%d\n",
              land ? land->aboard_ship_id : -1, ship ? ship->cargo_count : -1);
      return 1;
    }
    units_despawn(&pool, ship_id); /* also clears cargo */

    /* Ship → village tile (HAS_CITY, no colony) → VILLAGE_SHIP not LANDFALL. */
    {
      int vx = -1, vy = -1, sx = -1, sy = -1;
      for (int y = 1; y < (int)map.height - 1 && vx < 0; ++y) {
        for (int x = 1; x < (int)map.width - 1 && vx < 0; ++x) {
          if (!map_tile_is_land(&map, x, y) || !map.layer2) {
            continue;
          }
          static const int dx4[4] = {1, -1, 0, 0};
          static const int dy4[4] = {0, 0, 1, -1};
          for (int d = 0; d < 4; ++d) {
            const int nx = x + dx4[d];
            const int ny = y + dy4[d];
            if (map_tile_is_water(&map, nx, ny) && units_id_at(&pool, nx, ny) < 0) {
              vx = x;
              vy = y;
              sx = nx;
              sy = ny;
              break;
            }
          }
        }
      }
      if (vx < 0) {
        fprintf(stderr, "village-ship: no coastal land\n");
        return 1;
      }
      const size_t idx = (size_t)vy * (size_t)map.width + (size_t)vx;
      map.layer2[idx] = (uint8_t)(map.layer2[idx] | MAP_OCCUPANCY_HAS_CITY);
      const int sid = units_spawn(&pool, units_find_type(&pool, "Caravel"), sx, sy);
      ColonizeUnit* boat = units_get(&pool, sid);
      if (!boat) {
        fprintf(stderr, "village-ship spawn failed\n");
        return 1;
      }
      boat->nation_id = 0;
      boat->moves = 4 * UNITS_MP_PER_TILE;
      /* Loaded or empty: village must not become landfall. */
      const ColonizeEnterReason vr =
        units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, boat->type_index, vx, vy, sid);
      if (vr != COLONIZE_ENTER_VILLAGE_SHIP) {
        fprintf(stderr, "village-ship expected VILLAGE_SHIP got %d\n", (int)vr);
        return 1;
      }
      if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, sid, vx, vy)) {
        fprintf(stderr, "village-ship try_move should deny\n");
        return 1;
      }
      map.layer2[idx] = (uint8_t)(map.layer2[idx] & (uint8_t)~MAP_OCCUPANCY_HAS_CITY);
      units_despawn(&pool, sid);
    }
  fx_close();
  return 0;
}

  /* Phase-2 combat: 1b0e peels, best-defender, capture, naval damage, popups. */
/* Phase-2 combat: 1b0e peels, best-defender, capture, naval damage, popups.
 * Split into one case per sub-matrix 2026-09-23; each rebuilds the fixture
 * and the shared popup sink below. */
static int combat_phase2_open(AiPopupState* pops) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
  ai_popup_init(pops);
  units_set_combat_popups(pops, NULL);
  units_set_combat_human_nation(0);
  return 0;
}

static void combat_phase2_close(void) {
  units_set_combat_popups(NULL, NULL);
  units_set_combat_human_nation(-1);
  fx_close();
}

    /* Spanish ambush +50% on colony vs Indian. */
static int case_combat_ambush_spanish(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ColonizeColonyPool cols;
      colonies_init(&cols);
      colonies_set_occupancy_map(NULL);
      ColonizeColony* col = &cols.colonies[0];
      col->id = 0;
      col->active = true;
      col->nation_id = 2;
      col->x = 20;
      col->y = 20;
      cols.colony_count = 1;
      units_set_combat_colonies(&cols);
      const int spa_ti = units_find_type(&pool, "Soldiers");
      const int ind_ti = units_find_type(&pool, "Braves");
      if (spa_ti < 0 || ind_ti < 0) {
        fprintf(stderr, "phase2 ambush types missing\n");
        return 1;
      }
      const int aid = units_spawn_allow_stack(&pool, spa_ti, 20, 20);
      const int did = units_spawn_allow_stack(&pool, ind_ti, 20, 20);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      if (!a || !d) {
        fprintf(stderr, "phase2 ambush spawn failed\n");
        return 1;
      }
      a->nation_id = 2;
      d->nation_id = 4;
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = &pool;
      sctx.colonies = &cols;
      ColonizeCombatEngageResult er;
      combat_land_engage(&sctx, aid, did, &er);
      if ((er.atk_flags.flags & COMBAT_FLAG_AMBUSH) == 0) {
        fprintf(stderr, "phase2 Spanish ambush flag missing\n");
        return 1;
      }
      (void)units_despawn(&pool, aid);
      units_despawn(&pool, did);
      units_set_combat_colonies(NULL);
      fprintf(stderr, "unit_units: Spanish ambush peel ok\n");
  combat_phase2_close();
  return 0;
}

    /* Terrain stash: Indian→Euro and human→AI-Euro under WoI (REF). */
static int case_combat_terrain_stash(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ColonizeWorldMap tmap;
      memset(&tmap, 0, sizeof(tmap));
      tmap.width = 16;
      tmap.height = 16;
      tmap.terrain = calloc(256, 1);
      tmap.layer2 = calloc(256, 1);
      tmap.layer3 = calloc(256, 1);
      if (!tmap.terrain || !tmap.layer2 || !tmap.layer3) {
        fprintf(stderr, "terrain-stash map alloc failed\n");
        return 1;
      }
      for (int i = 0; i < 256; ++i) {
        tmap.terrain[i] = 8; /* forest class → found_score 2 */
      }
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.difficulty = 4; /* Viceroy: difficulty peel adj=0 */
      c1.player[0].control = 0; /* human */
      c1.player[1].control = 1; /* AI Euro / REF-shaped */
      const int sol = units_find_type(&pool, "Soldiers");
      if (sol < 0) {
        fprintf(stderr, "terrain-stash Soldiers missing\n");
        free(tmap.terrain);
        free(tmap.layer2);
        free(tmap.layer3);
        return 1;
      }
      pool.types[sol].attack = 2;
      pool.types[sol].defense = 2;
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = &pool;
      sctx.map = &tmap;
      sctx.col1 = &c1;

      /* Indian attacks Euro: defender loses terrain; attacker absorbs stash. */
      {
        const int aid = units_spawn_allow_stack(&pool, sol, 5, 5);
        const int did = units_spawn_allow_stack(&pool, sol, 5, 5);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "terrain-stash Indian spawn failed\n");
          return 1;
        }
        a->nation_id = 4;
        d->nation_id = 0;
        d->orders = UNITS_ORDER_NONE;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        /* def base 16, local_1a=0 → 16; atk base 16 → ((2+4)*16>>2)*3>>1 = 36 */
        if (er.def_strength != 16 || (er.def_flags.flags & COMBAT_FLAG_TERRAIN) != 0) {
          fprintf(
            stderr,
            "Indian ambush: def want 16/no-terrain got %d flags=%x stash=%d\n",
            er.def_strength,
            er.def_flags.flags,
            er.def_flags.terrain_stash
          );
          return 1;
        }
        if (er.def_flags.terrain_stash != 2) {
          fprintf(stderr, "Indian ambush: stash want 2 got %d\n", er.def_flags.terrain_stash);
          return 1;
        }
        if (er.atk_strength != 36 || (er.atk_flags.flags & COMBAT_FLAG_TERRAIN) == 0) {
          fprintf(
            stderr,
            "Indian ambush: atk want 36+terrain got %d flags=%x\n",
            er.atk_strength,
            er.atk_flags.flags
          );
          return 1;
        }
        (void)units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      /* WoI: human attacks AI Euro (REF) — same stash path. */
      {
        c1.head.game_options.woi = 1;
        const int aid = units_spawn_allow_stack(&pool, sol, 6, 6);
        const int did = units_spawn_allow_stack(&pool, sol, 6, 6);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "terrain-stash WoI spawn failed\n");
          return 1;
        }
        a->nation_id = 0;
        d->nation_id = 1;
        d->orders = UNITS_ORDER_NONE;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        if (er.def_flags.terrain_stash != 2 || (er.def_flags.flags & COMBAT_FLAG_TERRAIN) != 0) {
          fprintf(
            stderr,
            "WoI REF ambush: def stash want 2/no-flag got stash=%d flags=%x\n",
            er.def_flags.terrain_stash,
            er.def_flags.flags
          );
          return 1;
        }
        if ((er.atk_flags.flags & COMBAT_FLAG_TERRAIN) == 0 || er.atk_strength != 36) {
          fprintf(
            stderr,
            "WoI REF ambush: atk want 36+terrain got %d flags=%x\n",
            er.atk_strength,
            er.atk_flags.flags
          );
          return 1;
        }
        (void)units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      /* Control: Euro vs Euro pre-WoI — defender keeps terrain, stash 0. */
      {
        c1.head.game_options.woi = 0;
        const int aid = units_spawn_allow_stack(&pool, sol, 7, 7);
        const int did = units_spawn_allow_stack(&pool, sol, 7, 7);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "terrain-stash euro spawn failed\n");
          return 1;
        }
        a->nation_id = 1;
        d->nation_id = 0;
        d->orders = UNITS_ORDER_NONE;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        /* def ((2+4)*16)>>2=24; atk ((0+4)*16>>2)*3>>1=24 */
        if (er.def_strength != 24 || (er.def_flags.flags & COMBAT_FLAG_TERRAIN) == 0) {
          fprintf(
            stderr,
            "euro terrain: def want 24+flag got %d flags=%x\n",
            er.def_strength,
            er.def_flags.flags
          );
          return 1;
        }
        if (er.def_flags.terrain_stash != 0 || er.atk_strength != 24) {
          fprintf(
            stderr,
            "euro terrain: stash/atk want 0/24 got stash=%d atk=%d\n",
            er.def_flags.terrain_stash,
            er.atk_strength
          );
          return 1;
        }
        (void)units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      free(tmap.terrain);
      free(tmap.layer2);
      free(tmap.layer3);
      fprintf(stderr, "unit_units: terrain stash ambush ok\n");
  combat_phase2_close();
  return 0;
}

    /* WoI REF +50% / Tory-Rebel support — colony only (FUN_5fef_1b0e). */
static int case_combat_woi_ref_support(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ColonizeColonyPool cols;
      colonies_init(&cols);
      colonies_set_occupancy_map(NULL);
      ColonizeColony* col = &cols.colonies[0];
      col->id = 0;
      col->active = true;
      col->nation_id = 0;
      col->x = 10;
      col->y = 10;
      cols.colony_count = 1;
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.difficulty = 4;
      c1.head.game_options.woi = 1;
      c1.head.game_options.ref_present = 1;
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
        c1.head.founding_father[i] = -1; /* unclaimed — memset 0 would look like nation 0 owns FF */
      }
      /* Colony SoL 60% via col1 colony record. */
      ColonizeCol1Colony cc;
      memset(&cc, 0, sizeof(cc));
      cc.x = 10;
      cc.y = 10;
      cc.rebel_dividend = 60;
      cc.rebel_divisor = 100;
      c1.colony = &cc;
      c1.head.colony_count = 1;

      const int sol = units_find_type(&pool, "Soldiers");
      if (sol < 0) {
        fprintf(stderr, "woi-ref Soldiers missing\n");
        return 1;
      }
      pool.types[sol].attack = 2;
      pool.types[sol].defense = 2;
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = &pool;
      sctx.colonies = &cols;
      sctx.col1 = &c1;

      /* Human rebel attacks on colony with ref_present → +50% REF. */
      {
        const int aid = units_spawn_allow_stack(&pool, sol, 10, 10);
        const int did = units_spawn_allow_stack(&pool, sol, 10, 10);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "woi-ref rebel spawn failed\n");
          return 1;
        }
        a->nation_id = 0;
        d->nation_id = 1;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        /* atk base 16 → ×3/2=24; +50% REF → 36; +60% Rebels → 57 */
        if ((er.atk_flags.flags & COMBAT_FLAG_REF) == 0) {
          fprintf(stderr, "woi-ref rebel: REF flag missing\n");
          return 1;
        }
        if ((er.atk_flags.flags2 & COMBAT_FLAG_REBELS) == 0 || er.atk_flags.sol_percent != 60) {
          fprintf(
            stderr,
            "woi-ref rebel: Rebels want 60 got flags2=%x sol=%d\n",
            er.atk_flags.flags2,
            er.atk_flags.sol_percent
          );
          return 1;
        }
        if (er.atk_strength != 57) {
          fprintf(stderr, "woi-ref rebel: atk want 57 got %d\n", er.atk_strength);
          return 1;
        }
        (void)units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      /* Crown attacks same colony → +50% REF + Tory share 40%. */
      {
        const int aid = units_spawn_allow_stack(&pool, sol, 10, 10);
        const int did = units_spawn_allow_stack(&pool, sol, 10, 10);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "woi-ref crown spawn failed\n");
          return 1;
        }
        a->nation_id = 1;
        d->nation_id = 0;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        /* 24 +50%=36; +40% Tories → 50 */
        if ((er.atk_flags.flags & COMBAT_FLAG_REF) == 0) {
          fprintf(stderr, "woi-ref crown: REF flag missing\n");
          return 1;
        }
        if ((er.atk_flags.flags2 & COMBAT_FLAG_TORIES) == 0 || er.atk_flags.sol_percent != 40) {
          fprintf(
            stderr,
            "woi-ref crown: Tories want 40 got flags2=%x sol=%d\n",
            er.atk_flags.flags2,
            er.atk_flags.sol_percent
          );
          return 1;
        }
        if (er.atk_strength != 50) {
          fprintf(stderr, "woi-ref crown: atk want 50 got %d\n", er.atk_strength);
          return 1;
        }
        (void)units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      /* Open field: no REF flag even with ref_present. */
      {
        c1.head.game_options.ref_present = 1;
        const int aid = units_spawn_allow_stack(&pool, sol, 11, 11);
        const int did = units_spawn_allow_stack(&pool, sol, 11, 11);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "woi-ref field spawn failed\n");
          return 1;
        }
        a->nation_id = 0;
        d->nation_id = 1;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        if ((er.atk_flags.flags & COMBAT_FLAG_REF) != 0) {
          fprintf(stderr, "woi-ref field: REF must not apply off colony\n");
          return 1;
        }
        (void)units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      fprintf(stderr, "unit_units: WoI REF colony peels ok\n");
  combat_phase2_close();
  return 0;
}

    /* Best defender: Artillery preferred over Colonist on same tile. */
static int case_combat_best_defender(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      const int sol = units_find_type(&pool, "Soldiers");
      const int col = units_find_type(&pool, "Colonists");
      const int arty = units_find_type(&pool, "Artillery");
      if (sol < 0 || col < 0 || arty < 0) {
        fprintf(stderr, "phase2 best-def types missing\n");
        return 1;
      }
      const int atk = units_spawn_allow_stack(&pool, sol, 30, 30);
      const int weak = units_spawn_allow_stack(&pool, col, 31, 30);
      const int strong = units_spawn_allow_stack(&pool, arty, 31, 30);
      ColonizeUnit* au = units_get(&pool, atk);
      ColonizeUnit* wu = units_get(&pool, weak);
      ColonizeUnit* su = units_get(&pool, strong);
      if (!au || !wu || !su) {
        fprintf(stderr, "phase2 best-def spawn failed\n");
        return 1;
      }
      au->nation_id = 0;
      wu->nation_id = 1;
      su->nation_id = 1;
      pool.types[arty].attack = 4;
      pool.types[arty].defense = 8;
      pool.types[col].attack = 0;
      pool.types[col].defense = 1;
      /* bugs.md #677: DOS FUN_5fef_0000 is one ranking; an UNFORTIFIED gun in
       * the open takes score >>= 3 (asm 0x0115) and loses the pick to the
       * colonist. Fortified, it ranks on full strength and wins. */
      int best = units_best_defender_at(&pool, NULL, 31, 30, atk, atk);
      if (best != weak) {
        fprintf(stderr, "phase2 best-def open-field arty want colonist id=%d got %d\n", weak, best);
        return 1;
      }
      su->orders = UNITS_ORDER_FORTIFIED;
      best = units_best_defender_at(&pool, NULL, 31, 30, atk, atk);
      if (best != strong) {
        fprintf(stderr, "phase2 best-def want arty id=%d got %d\n", strong, best);
        return 1;
      }
      units_despawn(&pool, atk);
      units_despawn(&pool, weak);
      units_despawn(&pool, strong);
      fprintf(stderr, "unit_units: best-defender pick ok\n");
  combat_phase2_close();
  return 0;
}

    /* Capture-alive Colonists. */
static int case_combat_capture_alive_colonist(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.difficulty = 2;
      c1.player[0].control = 0;
      const int sol = units_find_type(&pool, "Soldiers");
      const int fc = units_find_type(&pool, "Colonists");
      if (sol < 0 || fc < 0) {
        fprintf(stderr, "phase2 capture types missing\n");
        return 1;
      }
      const int aid = units_spawn_allow_stack(&pool, sol, 40, 40);
      const int did = units_spawn_allow_stack(&pool, fc, 40, 40);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      if (!a || !d) {
        fprintf(stderr, "phase2 capture spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      d->nation_id = 1;
      pool.types[sol].attack = 8;
      pool.types[fc].attack = 0;
      pool.types[fc].defense = 1;
      if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, aid, did)) {
        fprintf(stderr, "phase2 capture combat should win\n");
        return 1;
      }
      d = units_get(&pool, did);
      if (!d || !d->active || d->nation_id != 0) {
        fprintf(stderr, "phase2 Colonists should be captured\n");
        return 1;
      }
      {
        int found = 0;
        for (int i = 0; i < pops.queue_count; ++i) {
          if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_CAPTURE) {
            found = 1;
            break;
          }
        }
        if (!found) {
          fprintf(stderr, "phase2 COLONISTCAPTURE popup missing\n");
          return 1;
        }
      }
      (void)units_despawn(&pool, aid);
      units_despawn(&pool, did);
      fprintf(stderr, "unit_units: capture-alive ok\n");
  combat_phase2_close();
  return 0;
}

    /* Native win: Pioneer destroyed (not captured); Soldier demoted to Colonist. */
static int case_combat_native_win_destroy_demote(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.difficulty = 2;
      c1.player[0].control = 0;
      const int pio = units_find_type(&pool, "Pioneers");
      const int sol = units_find_type(&pool, "Soldiers");
      const int col_ti = units_find_type(&pool, "Colonists");
      const int brave = units_find_type(&pool, "Braves");
      if (pio < 0 || sol < 0 || col_ti < 0 || brave < 0) {
        fprintf(stderr, "phase2 native-outcome types missing\n");
        return 1;
      }
      pool.types[brave].attack = 8;
      pool.types[brave].defense = 8;
      pool.types[pio].attack = 0;
      pool.types[pio].defense = 1;
      pool.types[sol].attack = 2;
      pool.types[sol].defense = 1;
      {
        const int aid = units_spawn_allow_stack(&pool, brave, 42, 42);
        const int did = units_spawn_allow_stack(&pool, pio, 42, 42);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "phase2 native-pioneer spawn failed\n");
          return 1;
        }
        a->nation_id = 4;
        d->nation_id = 0;
        if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, aid, did)) {
          fprintf(stderr, "phase2 native should beat pioneer\n");
          return 1;
        }
        d = units_get(&pool, did);
        if (d && d->active) {
          fprintf(stderr, "phase2 native win should destroy pioneer (not capture)\n");
          return 1;
        }
        (void)units_despawn(&pool, aid);
      }
      {
        const int aid = units_spawn_allow_stack(&pool, brave, 43, 43);
        const int did = units_spawn_allow_stack(&pool, sol, 43, 43);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "phase2 native-soldier spawn failed\n");
          return 1;
        }
        a->nation_id = 4;
        d->nation_id = 0;
        if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, aid, did)) {
          fprintf(stderr, "phase2 native should beat soldier\n");
          return 1;
        }
        d = units_get(&pool, did);
        if (!d || !d->active || d->nation_id != 0 || d->type_index != col_ti) {
          fprintf(
            stderr,
            "phase2 soldier should demote to Colonist same nation (active=%d nat=%d ti=%d want %d)\n",
            d && d->active,
            d ? d->nation_id : -1,
            d ? d->type_index : -1,
            col_ti
          );
          return 1;
        }
        (void)units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }
      fprintf(stderr, "unit_units: native destroy-pioneer / demote-soldier ok\n");
  combat_phase2_close();
  return 0;
}

    /* Naval damage-not-always-sink (DOS 0352): loser hull vs winner guns —
     * a tough hull survives damaged deterministically when hull > guns.
     * Privateer (guns 12) beats Frigate (hull 32) → damaged, not sunk. */
static int case_combat_naval_damage_not_sink(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      const int frig = units_find_type(&pool, "Frigate");
      const int priv = units_find_type(&pool, "Privateer");
      if (frig < 0 || priv < 0) {
        fprintf(stderr, "phase2 naval types missing\n");
        return 1;
      }
      units_set_combat_colonies(NULL); /* no reroute target in this fixture */
      pool.types[priv].attack = 20; /* force the win; guns stay 12 */
      const int aid = units_spawn_allow_stack(&pool, priv, 2, 2);
      const int did = units_spawn_allow_stack(&pool, frig, 3, 2);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      if (!a || !d) {
        fprintf(stderr, "phase2 naval spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      d->nation_id = 1;
      if (!units_resolve_naval_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL), .rng=(ColonizeDosRng*)(NULL)}, aid, did)) {
        fprintf(stderr, "phase2 naval should win\n");
        return 1;
      }
      d = units_get(&pool, did);
      if (!d || !d->active || (d->col1_flags15 & 0x80u) == 0) {
        fprintf(stderr, "phase2 weaker ship should survive damaged\n");
        return 1;
      }
      (void)units_despawn(&pool, aid);
      units_despawn(&pool, did);
      fprintf(stderr, "unit_units: naval damage-escape ok\n");
  combat_phase2_close();
  return 0;
}

    /* Outcome popups enqueued for human side. */
static int case_combat_outcome_popups(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ai_popup_clear(&pops);
      ColonizeMsgCatalog game_txt;
      assets_msg_init(&game_txt);
      if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
        fprintf(stderr, "phase2 popup GAME.TXT load failed\n");
        return 1;
      }
      units_set_combat_popups(&pops, &game_txt);
      const int sol = units_find_type(&pool, "Soldiers");
      if (sol < 0) {
        fprintf(stderr, "phase2 popup types missing\n");
        assets_msg_free(&game_txt);
        return 1;
      }
      const int aid = units_spawn_allow_stack(&pool, sol, 50, 50);
      const int did = units_spawn_allow_stack(&pool, sol, 50, 50);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      a->nation_id = 0;
      d->nation_id = 1;
      pool.types[sol].attack = 8;
      pool.types[sol].defense = 1;
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, aid, did)) {
        fprintf(stderr, "phase2 popup combat should win\n");
        assets_msg_free(&game_txt);
        return 1;
      }
      int found = 0;
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_EUROPE) {
          found = 1;
          /* @EUROPEWIN: "{atk} defeat {def unit} near {place}!" */
          if (!strstr(pops.queue[i].body, "defeat") || !strstr(pops.queue[i].body, "English") ||
              !strstr(pops.queue[i].body, "French") || !strstr(pops.queue[i].body, "Soldiers") ||
              !strstr(pops.queue[i].body, "Wilderness")) {
            fprintf(
              stderr,
              "phase2 EUROPEWIN body tokens wrong: [%s]\n",
              pops.queue[i].body
            );
            assets_msg_free(&game_txt);
            return 1;
          }
          break;
        }
      }
      if (!found) {
        fprintf(stderr, "phase2 expected EUROPEWIN popup enqueue (queue=%d)\n", pops.queue_count);
        assets_msg_free(&game_txt);
        return 1;
      }
      (void)units_despawn(&pool, aid);
      if (units_get(&pool, did) && units_get(&pool, did)->active) {
        units_despawn(&pool, did);
      }
      units_set_combat_popups(&pops, NULL);
      assets_msg_free(&game_txt);
      fprintf(stderr, "unit_units: combat outcome popup enqueue ok\n");
  combat_phase2_close();
  return 0;
}

    /*
     * Village Attack empty tile: FUN_5fef_1b0e temp Brave + population drain.
     * pop>=2 survives (pop--); pop<2 destroys. Not nearby-Brave pull.
     */
static int case_combat_village_attack_pop_drain(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      const int soldier = units_find_type(&pool, "Soldiers");
      const int brave = units_find_type(&pool, "Braves");
      if (soldier < 0 || brave < 0) {
        fprintf(stderr, "village-temp types missing\n");
        return 1;
      }
      int vx = -1, vy = -1;
      for (int y = 2; y < (int)map.height - 3 && vx < 0; ++y) {
        for (int x = 2; x < (int)map.width - 3 && vx < 0; ++x) {
          if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y)) {
            vx = x + 1;
            vy = y;
          }
        }
      }
      if (vx < 0) {
        fprintf(stderr, "village-temp no land pair\n");
        return 1;
      }
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.tribe_count = 1;
      c1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
      if (!c1.tribe) {
        fprintf(stderr, "village-temp tribe alloc\n");
        return 1;
      }
      c1.tribe[0].x = (uint8_t)vx;
      c1.tribe[0].y = (uint8_t)vy;
      c1.tribe[0].nation_id = 4;
      c1.tribe[0].population = 3;
      /* calloc'd mission 0 = an ENGLISH mission, which the raze now returns
       * as a Missionary (bugs.md #551); this fixture has none. */
      c1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
      c1.player[0].control = 0;
      c1.head.game_options.combat_analysis = 1;

      const int aid = units_spawn(&pool, soldier, vx - 1, vy);
      ColonizeUnit* a = units_get(&pool, aid);
      if (!a) {
        free(c1.tribe);
        fprintf(stderr, "village-temp soldier spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      a->moves = 3 * UNITS_MP_PER_TILE;
      pool.types[soldier].attack = 8;
      pool.types[brave].attack = 1;
      pool.types[brave].defense = 1;

      units_set_ff_col1(&c1);
      units_set_combat_human_nation(0);
      units_set_native_fallout_context(&c1, &map, -1);
      units_set_occupancy_map(&map);

      /*
       * Raid-win MP (smell audit 2026-09-10 A3): the stay-put raid branch pays
       * step cost + the 3-third combat-entry surcharge, same as the loss and
       * the ordinary land-win stay-put branch. DOS charges the surcharge at
       * FUN_5fef_1b0e ENTRY (`*(char *)(iVar23 + 0x3149) += 3` under
       * `if (param_5 != 0)`, viceroy_unpacked.c:100341-100343) — before the
       * roll — so no outcome of one attack can be cheaper than another.
       */
      const int raid_cost = units_move_cost(&pool, aid, &map, vx, vy);
      const int raid_mp_before = a->moves;

      /* First empty-village attack: temp Brave, pop 3→2, dwelling remains. */
      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, aid, vx, vy)) {
        fprintf(
          stderr,
          "village-temp try_move should engage (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        free(c1.tribe);
        return 1;
      }
      if (units_last_combat_outcome() <= 0) {
        fprintf(stderr, "village-temp expected combat win\n");
        free(c1.tribe);
        return 1;
      }
      if (c1.head.tribe_count != 1 || c1.tribe[0].population != 2) {
        fprintf(
          stderr,
          "village-temp after win want pop=2 count=1 got pop=%u count=%u\n",
          c1.tribe[0].population,
          c1.head.tribe_count
        );
        free(c1.tribe);
        return 1;
      }

      /* Drain to pop=1 then destroy on next temp fight. */
      a = units_get(&pool, aid);
      if (!a || !a->active) {
        free(c1.tribe);
        fprintf(stderr, "village-temp soldier should survive\n");
        return 1;
      }
      {
        const int want_mp =
          raid_mp_before - (raid_cost + 3) < 0 ? 0 : raid_mp_before - (raid_cost + 3);
        if (a->moves != want_mp) {
          fprintf(
            stderr,
            "village-temp raid win MP: got %d want %d (before %d cost %d + 3)\n",
            a->moves,
            want_mp,
            raid_mp_before,
            raid_cost
          );
          free(c1.tribe);
          return 1;
        }
      }
      /* DOS: fight from adjacent and stay — do not enter the village tile. */
      if (a->x != vx - 1 || a->y != vy) {
        fprintf(
          stderr,
          "village-temp after win should stay at (%d,%d) got (%d,%d)\n",
          vx - 1,
          vy,
          a->x,
          a->y
        );
        free(c1.tribe);
        return 1;
      }
      a->moves = 3 * UNITS_MP_PER_TILE;
      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, aid, vx, vy) ||
          units_last_combat_outcome() <= 0) {
        fprintf(stderr, "village-temp second attack failed\n");
        free(c1.tribe);
        return 1;
      }
      if (c1.tribe[0].population != 1) {
        fprintf(stderr, "village-temp want pop=1 got %u\n", c1.tribe[0].population);
        free(c1.tribe);
        return 1;
      }
      a = units_get(&pool, aid);
      if (!a || a->x != vx - 1 || a->y != vy) {
        fprintf(stderr, "village-temp second attack should leave soldier adjacent\n");
        free(c1.tribe);
        return 1;
      }
      a->moves = 3 * UNITS_MP_PER_TILE;
      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, aid, vx, vy) ||
          units_last_combat_outcome() <= 0) {
        fprintf(stderr, "village-temp destroy attack failed\n");
        free(c1.tribe);
        return 1;
      }
      if (c1.head.tribe_count != 0) {
        fprintf(stderr, "village-temp pop<2 should destroy dwelling\n");
        free(c1.tribe);
        return 1;
      }

      (void)units_despawn(&pool, aid);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      units_set_native_fallout_context(NULL, NULL, -1);
      free(c1.tribe);
      fprintf(stderr, "unit_units: village temp Brave + pop drain ok\n");
  combat_phase2_close();
  return 0;
}

    /*
     * Undefended Euro colony: token militia defender (W1.8 / P5.4 fix,
     * 2026-08-26). A colony with colonists but no standing soldier and no
     * Paul Revere must still fight back with a weak civilian stand-in —
     * not hand over a free capture. Phantom: never touches the colony's
     * real colonist_count, never lingers on the map afterward.
     */
static int case_combat_undefended_colony_militia(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      const int soldier2 = units_find_type(&pool, "Soldiers");
      if (soldier2 < 0) {
        fprintf(stderr, "colony-temp-defender soldier type missing\n");
        return 1;
      }
      int cx = -1, cy = -1;
      for (int y = 2; y < (int)map.height - 3 && cx < 0; ++y) {
        for (int x = 2; x < (int)map.width - 3 && cx < 0; ++x) {
          if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y) &&
              units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
            cx = x + 1;
            cy = y;
          }
        }
      }
      if (cx < 0) {
        fprintf(stderr, "colony-temp-defender no land pair\n");
        return 1;
      }
      ColonizeColonyPool colonies;
      colonies_init(&colonies);
      colonies_set_occupancy_map(NULL);
      if (!colonies_load_names(&colonies, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&colonies, &names)) {
        fprintf(stderr, "colony-temp-defender colonies init failed\n");
        return 1;
      }
      const int cid = colonies_found(&colonies, &map, cx, cy, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
      if (cid < 0) {
        fprintf(stderr, "colony-temp-defender found rival colony failed\n");
        return 1;
      }
      ColonizeColony* col = colonies_get_mut(&colonies, cid);
      if (!col) {
        fprintf(stderr, "colony-temp-defender colony fetch failed\n");
        return 1;
      }
      col->nation_id = 1;
      col->population = 1;
      col->colonist_count = 1;
      col->colonists[0].active = true;

      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      /* No Revere owned, no muskets stock — Revere override must not apply. */

      const int aid = units_spawn(&pool, soldier2, cx - 1, cy);
      ColonizeUnit* a = units_get(&pool, aid);
      if (!a) {
        fprintf(stderr, "colony-temp-defender attacker spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      a->moves = 5 * UNITS_MP_PER_TILE;
      pool.types[soldier2].attack = 8;
      pool.types[soldier2].defense = 1;

      units_set_ff_col1(&c1);
      units_set_combat_human_nation(0);
      units_set_occupancy_map(&map);

      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, aid, cx, cy)) {
        fprintf(
          stderr,
          "colony-temp-defender attack should win (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      if (units_last_combat_outcome() <= 0) {
        fprintf(stderr, "colony-temp-defender expected a real combat win, not a free capture\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      col = colonies_get_mut(&colonies, cid);
      if (!col || col->nation_id != 0) {
        fprintf(stderr, "colony-temp-defender attacker should capture colony\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      if (col->colonist_count != 1 || !col->colonists[0].active) {
        fprintf(stderr, "colony-temp-defender phantom must not touch real colonist_count\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->id != aid && u->x == cx && u->y == cy) {
          fprintf(stderr, "colony-temp-defender phantom should not remain on the tile\n");
          units_set_ff_col1(NULL);
          return 1;
        }
      }

      (void)units_despawn(&pool, aid);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      fprintf(stderr, "unit_units: undefended colony token militia ok\n");
  combat_phase2_close();
  return 0;
}

    /*
     * smell_audit 2026-09-09 #2 — the militia / Paul Revere phantom is DOS's
     * scratch @UNIT row 0x17 (built by FUN_291f_0a20 = FUN_478c_002c, raw
     * 76545-76564) and FUN_5fef_1b0e deletes it with FUN_291f_0a06 at raw
     * 100636-100639, BEFORE the win/lose branch:
     *
     *   if (bVar28) { FUN_291f_0a06(0x281f); }
     *   if (bVar8) { ... if (bVar28) { town consequences } else { 0352 } }
     *
     * So the phantom never reaches FUN_5fef_0352 — no @COLONISTCAPTURE, no
     * @DEMOTE, no nation flip — and never reaches FUN_5fef_172c, whose
     * opening `type byte == 1 || type byte == 4` gate a 0x17 row fails
     * before FUN_281f_04d4, so a phantom that WINS draws no promotion RNG.
     */
static int case_combat_militia_phantom_row(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      const int mil_sol = units_find_type(&pool, "Soldiers");
      const int mil_atk_ty = units_find_type(&pool, "Dragoons");
      /*
       * Half 2's attacker must lose without any 0352 chrome of its OWN, or
       * the popup assertion cannot tell the attacker's legitimate @DEMOTE
       * from a phantom one: Artillery takes the damaged bit and @ARTILLERY
       * (tag COMBAT_SHIP), never CAPTURE/DEMOTE.
       */
      const int mil_loser_ty = units_find_type(&pool, "Artillery");
      if (mil_sol < 0 || mil_atk_ty < 0 || mil_loser_ty < 0) {
        fprintf(stderr, "militia-phantom types missing\n");
        return 1;
      }
      ColonizeColonyPool mcols;
      colonies_init(&mcols);
      colonies_set_occupancy_map(NULL);
      if (!colonies_load_names(&mcols, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&mcols, &names)) {
        fprintf(stderr, "militia-phantom colonies init failed\n");
        return 1;
      }
      int mx = -1, my = -1, mcid = -1;
      for (int y = (int)map.height / 2; y < (int)map.height - 4 && mcid < 0; ++y) {
        for (int x = 3; x < (int)map.width - 4 && mcid < 0; ++x) {
          if (!map_tile_is_land(&map, x, y) || !map_tile_is_land(&map, x + 1, y)) {
            continue;
          }
          const int cand =
            colonies_found(&mcols, &map, x + 1, y, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
          if (cand >= 0) {
            mx = x + 1;
            my = y;
            mcid = cand;
          }
        }
      }
      ColonizeColony* mcol = mcid >= 0 ? colonies_get_mut(&mcols, mcid) : NULL;
      if (!mcol) {
        fprintf(stderr, "militia-phantom found colony failed\n");
        return 1;
      }
      /* Shared pool: purge survivors parked on the two tiles under test. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->y == my && (u->x == mx || u->x == mx - 1)) {
          (void)units_despawn(&pool, u->id);
        }
      }
      mcol->nation_id = 1;
      mcol->population = 2;
      mcol->colonist_count = 2;
      mcol->colonists[0].active = true;
      mcol->colonists[1].active = true;

      ColonizeCol1Save mc1;
      memset(&mc1, 0, sizeof(mc1));
      for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
        mc1.head.founding_father[i] = -1; /* zeroed = "nation 0 owns them all" */
      }
      mc1.player[0].control = 0; /* human attacker */
      mc1.player[1].control = 1;
      mc1.head.difficulty = 3; /* off the Discoverer beginner shield */
      mc1.head.turn = 200;

      units_set_ff_col1(&mc1);
      units_set_combat_human_nation(0);
      units_set_occupancy_map(&map);
      units_set_combat_popups(&pops, NULL);

      /*
       * Half 1 — attacker WINS: the phantom must not be captured or demoted,
       * and neither popup may be enqueued. NULL rng makes the roll
       * deterministic (strength compare) and keeps every promotion path shut,
       * so a COMBAT_CAPTURE / COMBAT_DEMOTE entry could only come from the
       * phantom's own 0352 tail.
       */
      pool.types[mil_atk_ty].attack = 8;
      pool.types[mil_atk_ty].defense = 1;
      pool.types[mil_sol].attack = 2;
      pool.types[mil_sol].defense = 2;
      ai_popup_clear(&pops);
      const int mil_aid = units_spawn(&pool, mil_atk_ty, mx - 1, my);
      ColonizeUnit* mil_a = units_get(&pool, mil_aid);
      if (!mil_a) {
        fprintf(stderr, "militia-phantom attacker spawn failed\n");
        return 1;
      }
      mil_a->nation_id = 0;
      mil_a->moves = 5 * UNITS_MP_PER_TILE;
      (void)units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&mcols), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, mil_aid, mx, my);
      if (units_last_combat_outcome() <= 0) {
        fprintf(
          stderr,
          "militia-phantom attacker should beat the token militia (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_CAPTURE ||
            pops.queue[i].tag == AI_POPUP_TAG_COMBAT_DEMOTE) {
          fprintf(
            stderr,
            "militia-phantom: 0352 popup on the scratch 0x17 row (tag=%d body=[%s])\n",
            (int)pops.queue[i].tag,
            pops.queue[i].body
          );
          units_set_ff_col1(NULL);
          return 1;
        }
      }
      (void)units_despawn(&pool, mil_aid);

      /*
       * Half 2 — the PHANTOM wins. Arm it through Revere (FF 12 + muskets >
       * 0x31) so it spawns as the Soldiers body whose profession/kit would
       * otherwise satisfy units_promote_on_win; DOS's 172c type-byte gate
       * rejects it before FUN_281f_04d4, so the engagement must consume
       * exactly ONE draw — the combat roll itself.
       */
      mcol = colonies_get_mut(&mcols, mcid);
      mcol->nation_id = 1; /* half 1 flipped it — hand the town back */
      mcol->population = 2;
      mcol->colonist_count = 2;
      mcol->colonists[0].active = true;
      mcol->colonists[1].active = true;
      mcol->stock[COLONIZE_CARGO_MUSKETS] = 100;
      mc1.nation[1].founding_fathers[FF_PAUL_REVERE / 8] |=
        (uint8_t)(1u << (FF_PAUL_REVERE % 8));
      pool.types[mil_loser_ty].attack = 1;
      pool.types[mil_loser_ty].defense = 1;
      /* The phantom's own row: 2/2 in DOS, floored up here so the roll lands
       * on the defender for most seeds — the branch under test is the tail,
       * not the odds. */
      pool.types[mil_sol].defense = 60;

      int mil_seed = -1;
      int mil_draws = -1;
      for (uint32_t seed = 1; seed <= 400 && mil_seed < 0; ++seed) {
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &pool.units[i];
          if (u->active && u->y == my && (u->x == mx || u->x == mx - 1)) {
            (void)units_despawn(&pool, u->id);
          }
        }
        /* A previous iteration may have taken the town — hand it back. */
        mcol = colonies_get_mut(&mcols, mcid);
        mcol->nation_id = 1;
        mcol->population = 2;
        mcol->colonist_count = 2;
        mcol->colonists[0].active = true;
        mcol->colonists[1].active = true;
        mcol->stock[COLONIZE_CARGO_MUSKETS] = 100;
        const int lid = units_spawn(&pool, mil_loser_ty, mx - 1, my);
        ColonizeUnit* la = units_get(&pool, lid);
        if (!la) {
          fprintf(stderr, "militia-phantom loser spawn failed\n");
          return 1;
        }
        la->nation_id = 0;
        la->moves = 5 * UNITS_MP_PER_TILE;
        ai_popup_clear(&pops);
        ColonizeDosRng mrng;
        dos_rng_seed(&mrng, seed);
        const uint32_t start_state = mrng.state;
        (void)units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&mcols), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(&mrng)}, lid, mx, my);
        if (units_last_combat_outcome() >= 0) {
          if (units_get(&pool, lid)) {
            (void)units_despawn(&pool, lid);
          }
          continue; /* attacker survived/won — not the case under test */
        }
        /* Count LCG advances: the stream is a pure state machine. */
        ColonizeDosRng replay;
        replay.state = start_state;
        int steps = -1;
        for (int k = 0; k <= 32; ++k) {
          if (replay.state == mrng.state) {
            steps = k;
            break;
          }
          (void)dos_rng_next(&replay);
        }
        mil_seed = (int)seed;
        mil_draws = steps;
        if (units_get(&pool, lid)) {
          (void)units_despawn(&pool, lid);
        }
      }
      if (mil_seed < 0) {
        fprintf(stderr, "militia-phantom: no seed made the phantom win\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      if (mil_draws != 1) {
        fprintf(
          stderr,
          "militia-phantom: seed %d consumed %d RNG draws, expected 1 "
          "(combat roll only — 172c must not roll for a 0x17 row)\n",
          mil_seed,
          mil_draws
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_CAPTURE ||
            pops.queue[i].tag == AI_POPUP_TAG_COMBAT_DEMOTE) {
          fprintf(
            stderr,
            "militia-phantom: winning phantom produced a 0352/172c popup (tag=%d body=[%s])\n",
            (int)pops.queue[i].tag,
            pops.queue[i].body
          );
          units_set_ff_col1(NULL);
          return 1;
        }
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->x == mx && u->y == my) {
          fprintf(stderr, "militia-phantom: phantom must not survive the roll\n");
          units_set_ff_col1(NULL);
          return 1;
        }
      }

      ai_popup_clear(&pops);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      fprintf(
        stderr, "unit_units: militia phantom bypasses 0352/172c ok (seed %d)\n", mil_seed
      );
  combat_phase2_close();
  return 0;
}

    /*
     * Discoverer beginner shield — FUN_5fef_1b0e raw 100536-100545.
     * `if ((bVar28) && (*(char *)0x53a6 == '\0')) local_92 = 0;` inside
     * `0x53a6 < 2` / defender-is-a-human-European / turn < 0x50 / colony on
     * the tile. local_92 is the ATTACKER (it is what the roll compares
     * against), so on Discoverer an attack on a human player's UNDEFENDED
     * town — the militia phantom, bVar28 — can never be won, however strong
     * the attacker. Above difficulty 0 the same fixture must still fall.
     */
static int case_combat_discoverer_beginner_shield(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      const int soldier3 = units_find_type(&pool, "Soldiers");
      if (soldier3 < 0) {
        fprintf(stderr, "beginner-shield soldier type missing\n");
        return 1;
      }
      int sx = -1, sy = -1;
      for (int y = (int)map.height - 4; y > 2 && sx < 0; --y) {
        for (int x = (int)map.width - 4; x > 2 && sx < 0; --x) {
          if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x - 1, y) &&
              units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x - 1, y) < 0) {
            sx = x;
            sy = y;
          }
        }
      }
      if (sx < 0) {
        fprintf(stderr, "beginner-shield no land pair\n");
        return 1;
      }
      /* Shared pool: earlier sub-tests park survivors around. Purge the two
       * tiles this block asserts on. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->y == sy && (u->x == sx || u->x == sx - 1)) {
          (void)units_despawn(&pool, u->id);
        }
      }

      ColonizeColonyPool scol;
      colonies_init(&scol);
      colonies_set_occupancy_map(NULL);
      if (!colonies_load_names(&scol, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&scol, &names)) {
        fprintf(stderr, "beginner-shield colonies init failed\n");
        return 1;
      }
      const int scid = colonies_found(&scol, &map, sx, sy, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
      ColonizeColony* scolony = scid >= 0 ? colonies_get_mut(&scol, scid) : NULL;
      if (!scolony) {
        fprintf(stderr, "beginner-shield found colony failed\n");
        return 1;
      }
      scolony->nation_id = 1;
      scolony->population = 1;
      scolony->colonist_count = 1;
      scolony->colonists[0].active = true;

      ColonizeCol1Save sc1;
      memset(&sc1, 0, sizeof(sc1));
      sc1.player[0].control = 1; /* AI raider */
      sc1.player[1].control = 0; /* the human's town — DOS 0x543f == 0 */
      sc1.head.difficulty = 0; /* Discoverer */
      sc1.head.turn = 10; /* inside the DS:0x538e < 0x50 window */

      pool.types[soldier3].attack = 8;
      pool.types[soldier3].defense = 1;
      units_set_ff_col1(&sc1);
      units_set_combat_human_nation(1);
      units_set_occupancy_map(&map);

      const int shield_atk = units_spawn(&pool, soldier3, sx - 1, sy);
      ColonizeUnit* sa = units_get(&pool, shield_atk);
      if (!sa) {
        fprintf(stderr, "beginner-shield attacker spawn failed\n");
        return 1;
      }
      sa->nation_id = 0;
      sa->moves = 5 * UNITS_MP_PER_TILE;
      if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&scol), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, shield_atk, sx, sy)) {
        fprintf(stderr, "beginner-shield: Discoverer attacker must not take the town\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      scolony = colonies_get_mut(&scol, scid);
      if (!scolony || scolony->nation_id != 1) {
        fprintf(stderr, "beginner-shield: colony must stay with its owner\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      {
        /* The loser may be demoted rather than destroyed — what matters is
         * that it never set foot in the town, and that the phantom (which
         * always evaporates) left nothing behind. */
        const ColonizeUnit* loser = units_get(&pool, shield_atk);
        if (loser && loser->x == sx && loser->y == sy) {
          fprintf(stderr, "beginner-shield: beaten attacker must not enter the colony\n");
          units_set_ff_col1(NULL);
          return 1;
        }
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->x == sx && u->y == sy) {
          fprintf(stderr, "beginner-shield: phantom must not remain on the tile\n");
          units_set_ff_col1(NULL);
          return 1;
        }
      }

      /* Same fixture at difficulty 2: outside DOS's `0x53a6 < 2` gate, so the
       * shield is off and the identical attack carries the town. */
      sc1.head.difficulty = 2;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->x == sx - 1 && u->y == sy) {
          (void)units_despawn(&pool, u->id);
        }
      }
      const int hard_atk = units_spawn(&pool, soldier3, sx - 1, sy);
      ColonizeUnit* ha = units_get(&pool, hard_atk);
      if (!ha) {
        fprintf(stderr, "beginner-shield hard attacker spawn failed\n");
        return 1;
      }
      ha->nation_id = 0;
      ha->moves = 5 * UNITS_MP_PER_TILE;
      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&scol), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, hard_atk, sx, sy)) {
        fprintf(
          stderr,
          "beginner-shield: above Discoverer the same attack should win (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      scolony = colonies_get_mut(&scol, scid);
      if (!scolony || scolony->nation_id != 0) {
        fprintf(stderr, "beginner-shield: difficulty 2 attacker should capture the colony\n");
        units_set_ff_col1(NULL);
        return 1;
      }

      (void)units_despawn(&pool, hard_atk);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      fprintf(stderr, "unit_units: Discoverer undefended-town shield ok\n");
  combat_phase2_close();
  return 0;
}

    /*
     * FUN_5fef_1b0e port-ship fate + DS:0x54f6 discharge (2026-09-08).
     *
     * (1) A foreign hull berthed in a colony is invisible to a land assault:
     *     FUN_5fef_0000's domain gate (raw 99190-99196) skips it as a
     *     defender, and the capture arm (raw 100905-101034) never touches the
     *     unit array. So the town falls with the ship still in it, still
     *     flying the loser's flag — not sunk, not seized.
     * (2) FUN_5fef_1b0e site 1 (raw 101039-101041): a native attacker that
     *     resolves against a European clears tribe[t].alarm[euro] (the whole
     *     DS:0x54f6 attitude word),
     *     unless it LOST at a colony (DOS routes that to FUN_5fef_0f14).
     */
static int case_combat_port_ship_fate_discharge(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      const int soldier3 = units_find_type(&pool, "Soldiers");
      const int frig3 = units_find_type(&pool, "Frigate");
      const int brave3 = units_find_type(&pool, "Braves");
      if (soldier3 < 0 || frig3 < 0 || brave3 < 0) {
        fprintf(stderr, "1b0e-port-ship types missing\n");
        return 1;
      }
      int cx = -1, cy = -1;
      for (int y = 2; y < (int)map.height - 3 && cx < 0; ++y) {
        for (int x = 2; x < (int)map.width - 3 && cx < 0; ++x) {
          if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y) &&
              units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
            cx = x + 1;
            cy = y;
          }
        }
      }
      if (cx < 0) {
        fprintf(stderr, "1b0e-port-ship no land pair\n");
        return 1;
      }
      ColonizeColonyPool colonies;
      colonies_init(&colonies);
      colonies_set_occupancy_map(NULL);
      if (!colonies_load_names(&colonies, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&colonies, &names)) {
        fprintf(stderr, "1b0e-port-ship colonies init failed\n");
        return 1;
      }
      const int cid = colonies_found(&colonies, &map, cx, cy, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
      ColonizeColony* col = colonies_get_mut(&colonies, cid);
      if (cid < 0 || !col) {
        fprintf(stderr, "1b0e-port-ship found rival colony failed\n");
        return 1;
      }
      col->nation_id = 1;
      col->population = 1;
      col->colonist_count = 1;
      col->colonists[0].active = true;

      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;

      /* Rival Frigate berthed in the port (armed: attack > 0, so the
       * non-combat seizure sweep leaves it alone). */
      const int ship = units_spawn_allow_stack(&pool, frig3, cx, cy);
      ColonizeUnit* sv = units_get(&pool, ship);
      if (!sv) {
        fprintf(stderr, "1b0e-port-ship frigate spawn failed\n");
        return 1;
      }
      sv->nation_id = 1;

      const int aid = units_spawn(&pool, soldier3, cx - 1, cy);
      ColonizeUnit* a = units_get(&pool, aid);
      if (!a) {
        fprintf(stderr, "1b0e-port-ship attacker spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      a->moves = 5 * UNITS_MP_PER_TILE;
      pool.types[soldier3].attack = 8;
      pool.types[soldier3].defense = 1;

      units_set_ff_col1(&c1);
      units_set_combat_human_nation(0);
      units_set_occupancy_map(&map);

      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(NULL)}, aid, cx, cy)) {
        fprintf(
          stderr,
          "1b0e-port-ship attack should win (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      col = colonies_get_mut(&colonies, cid);
      if (!col || col->nation_id != 0) {
        fprintf(stderr, "1b0e-port-ship berthed hull must not block the capture\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      sv = units_get(&pool, ship);
      if (!sv || !sv->active) {
        fprintf(stderr, "1b0e-port-ship DOS does not sink the berthed hull\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      if (sv->nation_id != 1 || sv->x != cx || sv->y != cy) {
        fprintf(
          stderr,
          "1b0e-port-ship DOS does not seize/move the berthed hull (nation %d at %d,%d)\n",
          sv->nation_id, sv->x, sv->y
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      units_despawn(&pool, ship);
      (void)units_despawn(&pool, aid);

      /* --- DS:0x54f6 discharge, site 1 --- the slot is the settlement
       * record's own attitude[euro] word (tribe.alarm[euro]). */
      ColonizeCol1Tribe tribes[2];
      memset(tribes, 0, sizeof(tribes));
      tribes[0].nation_id = 4;
      tribes[1].nation_id = 4;
      c1.tribe = tribes;
      c1.head.tribe_count = 2;

      /* Open ground: native attacker wins → tribe 0's word toward euro 1 = 0
       * (BOTH bytes, friction and attacks). */
      units_set_combat_colonies(&colonies);
      tribes[0].alarm[1].friction = 96;
      tribes[0].alarm[1].attacks = 3;
      {
        const int bx = cx - 3;
        const int by = cy;
        const int bid = units_spawn_allow_stack(&pool, brave3, bx, by);
        const int vid = units_spawn_allow_stack(&pool, soldier3, bx, by);
        ColonizeUnit* b = units_get(&pool, bid);
        ColonizeUnit* v = units_get(&pool, vid);
        if (!b || !v) {
          fprintf(stderr, "1b0e-tension open-ground spawn failed\n");
          units_set_ff_col1(NULL);
          return 1;
        }
        b->nation_id = 4;
        b->home_tribe_id = 0;
        v->nation_id = 1;
        pool.types[brave3].attack = 8;
        pool.types[soldier3].defense = 1;
        if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, bid, vid)) {
          fprintf(stderr, "1b0e-tension brave should win on open ground\n");
          units_set_ff_col1(NULL);
          return 1;
        }
        if (col1_tribe_attitude(&tribes[0], 1) != 0) {
          fprintf(
            stderr,
            "1b0e-tension open-ground win must clear the slot, got %d\n",
            col1_tribe_attitude(&tribes[0], 1)
          );
          units_set_ff_col1(NULL);
          return 1;
        }
        units_despawn(&pool, bid);
      }

      /* Colony tile + native attacker LOSES: DOS hands the clear to
       * FUN_5fef_0f14's raid path, so this site leaves the slot alone. */
      col1_tribe_attitude_set(&tribes[1], 1, 77);
      {
        const int bid = units_spawn_allow_stack(&pool, brave3, cx, cy);
        const int vid = units_spawn_allow_stack(&pool, soldier3, cx, cy);
        ColonizeUnit* b = units_get(&pool, bid);
        ColonizeUnit* v = units_get(&pool, vid);
        if (!b || !v) {
          fprintf(stderr, "1b0e-tension colony spawn failed\n");
          units_set_ff_col1(NULL);
          return 1;
        }
        b->nation_id = 4;
        b->home_tribe_id = 1;
        v->nation_id = 1;
        pool.types[brave3].attack = 0;
        pool.types[soldier3].defense = 8;
        if (units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, bid, vid)) {
          fprintf(stderr, "1b0e-tension brave should lose at the colony\n");
          units_set_ff_col1(NULL);
          return 1;
        }
        if (col1_tribe_attitude(&tribes[1], 1) != 77) {
          fprintf(
            stderr,
            "1b0e-tension colony loss must NOT clear the slot, got %d\n",
            col1_tribe_attitude(&tribes[1], 1)
          );
          units_set_ff_col1(NULL);
          return 1;
        }
        units_despawn(&pool, vid);
      }

      c1.tribe = NULL;
      c1.head.tribe_count = 0;
      units_set_combat_colonies(NULL);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      fprintf(stderr, "unit_units: 1b0e port-ship fate + DS:0x54f6 discharge ok\n");
  combat_phase2_close();
  return 0;
}

    /* bugs.md #660: a beaten Treasure Train changes hands (no gold, no ransom).
     * DOS FUN_5fef_0352 raw 99392-99413. */
static int case_combat_treasure_capture(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ai_popup_clear(&pops);
      units_set_combat_popups(&pops, NULL);
      units_set_occupancy_map(NULL);
      units_set_native_fallout_context(NULL, NULL, 0);
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      c1.nation[0].gold = 10;
      int use_ti = units_find_type(&pool, "Treasure");
      const int sol = units_find_type(&pool, "Soldiers");
      if (use_ti < 0 || sol < 0) {
        fprintf(stderr, "treasure capture types missing\n");
        return 1;
      }
      const int aid = units_spawn_allow_stack(&pool, sol, 55, 55);
      const int did = units_spawn_allow_stack(&pool, use_ti, 55, 55);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      a->nation_id = 0;
      d->nation_id = 1;
      d->profession = 1; /* DOS +0x315b = gold/100 -> 100 */
      pool.types[sol].attack = 8;
      pool.types[use_ti].defense = 1;
      if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, aid, did)) {
        fprintf(stderr, "treasure capture combat should win\n");
        return 1;
      }
      if (c1.nation[0].gold != 10) {
        fprintf(stderr, "treasure capture must credit no gold (got %u)\n", c1.nation[0].gold);
        return 1;
      }
      const ColonizeUnit* dd = units_get_const(&pool, did);
      if (!dd || !dd->active) {
        fprintf(stderr, "captured treasure must stay alive\n");
        return 1;
      }
      if (dd->nation_id != 0) {
        fprintf(stderr, "captured treasure nation want 0 got %d\n", dd->nation_id);
        return 1;
      }
      int cap_q = -1;
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_CAPTURE && pops.queue[i].payload == 100) {
          cap_q = i;
          break;
        }
      }
      if (cap_q < 0) {
        fprintf(stderr, "@LOOTCAPTURE popup (number0 = value) not enqueued\n");
        return 1;
      }
      (void)units_despawn(&pool, aid);
      (void)units_despawn(&pool, did);
      fprintf(stderr, "unit_units: treasure capture-alive ok\n");
  combat_phase2_close();
  return 0;
}

    /* bugs.md #663: capture is disqualified on water (local_2a) — destroy. */
static int case_combat_capture_water_destroy(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ColonizeWorldMap wmap;
      memset(&wmap, 0, sizeof(wmap));
      wmap.width = 16;
      wmap.height = 16;
      wmap.tile_count = 256;
      wmap.terrain = calloc(256, 1);
      wmap.layer2 = calloc(256, 1);
      wmap.layer3 = calloc(256, 1);
      if (!wmap.terrain || !wmap.layer2 || !wmap.layer3) {
        fprintf(stderr, "#663 map alloc\n");
        return 1;
      }
      for (int i = 0; i < 256; ++i) {
        wmap.terrain[i] = 25; /* ocean */
        wmap.layer3[i] = 1;   /* continent 1: ocean, not lake */
      }
      ai_popup_clear(&pops);
      units_set_combat_popups(&pops, NULL);
      units_set_occupancy_map(&wmap);
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      const int use_ti = units_find_type(&pool, "Treasure");
      const int sol = units_find_type(&pool, "Soldiers");
      const int aid = units_spawn_allow_stack(&pool, sol, 5, 5);
      const int did = units_spawn_allow_stack(&pool, use_ti, 5, 5);
      units_get(&pool, aid)->nation_id = 0;
      ColonizeUnit* d = units_get(&pool, did);
      d->nation_id = 1;
      d->hold_goods_amount[0] = 100 & 0xff;
      pool.types[sol].attack = 8;
      pool.types[use_ti].defense = 1;
      (void)units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, aid, did);
      const ColonizeUnit* dd = units_get_const(&pool, did);
      if (dd && dd->active && dd->nation_id == 0) {
        fprintf(stderr, "#663: treasure must not be captured on water\n");
        units_set_occupancy_map(NULL);
        return 1;
      }
      units_set_occupancy_map(NULL);
      (void)units_despawn(&pool, aid);
      free(wmap.terrain);
      free(wmap.layer2);
      free(wmap.layer3);
      fprintf(stderr, "unit_units: #663 water capture gate ok\n");
  combat_phase2_close();
  return 0;
}

    /* Colony capture notify @CAPTURED*. */
static int case_combat_colony_capture_notify(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ai_popup_clear(&pops);
      units_set_combat_popups(&pops, NULL);
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      ColonizeColony col;
      memset(&col, 0, sizeof(col));
      col.active = true;
      col.nation_id = 1;
      snprintf(col.name, sizeof(col.name), "Jamestown");
      col.stock[COLONIZE_CARGO_FOOD] = 40;
      units_combat_notify_colony_captured(&c1, &col, 0, 40);
      int found = 0;
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_COLONY) {
          found = 1;
          break;
        }
      }
      if (!found) {
        fprintf(stderr, "colony CAPTURED popup missing\n");
        return 1;
      }
      fprintf(stderr, "unit_units: colony CAPTURED popup ok\n");
  combat_phase2_close();
  return 0;
}

    /* Privateer seizure tag. */
static int case_combat_privateer_seizure(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ai_popup_clear(&pops);
      units_set_combat_popups(&pops, NULL);
      const int priv = units_find_type(&pool, "Privateer");
      const int car = units_find_type(&pool, "Caravel");
      if (priv < 0 || car < 0) {
        fprintf(stderr, "seizure types missing\n");
        return 1;
      }
      pool.types[priv].attack = 16;
      pool.types[priv].defense = 8;
      /* bugs.md: seizure fires only for UNARMED transports (attack 0, as in
       * real NAMES.TXT); a warship loser is damage-or-sink only. */
      pool.types[car].attack = 0;
      pool.types[car].defense = 2;
      /*
       * The seizure arm needs the damage-vs-sink roll to land on "sunk", so
       * keep the winner's guns clear of the loser's hull: DOS rolls
       * 1..(guns+hull) and damages on <= hull (FUN_5fef_0352,
       * viceroy_unpacked.c 99527-99530), so guns == hull is a coin flip that
       * the port's no-RNG path resolves to "damaged". Stock NAMES.TXT has
       * Privateer guns 4 / Caravel hull 4 — exactly that tie.
       */
      pool.types[priv].guns = 12;
      pool.types[car].hull = 4;
      const int aid = units_spawn_allow_stack(&pool, priv, 4, 4);
      const int did = units_spawn_allow_stack(&pool, car, 5, 4);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      a->nation_id = 0;
      d->nation_id = 1;
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;
    /* bugs.md #867 / FUN_5fef_0352 raw 99531-99548: an UNARMED loser now goes
     * through the AI fleet-pool bias, which forces "damaged" when the owner's
     * spare cargo capacity (stuff.ship_cargo_totals) is below clamp(pop>>2,3,6).
     * A blank fixture census reads 0 and would always save the hull, so give
     * nation 1 a fleet with room to spare and keep this case a SINK. */
      c1.stuff.ship_cargo_totals[1] = 60;
      if (!units_resolve_naval_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, aid, did)) {
        fprintf(stderr, "seizure naval should win\n");
        return 1;
      }
      int found = 0;
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_SEIZURE) {
          found = 1;
          break;
        }
      }
      if (!found) {
        fprintf(stderr, "SEIZURE popup missing (queue=%d)\n", pops.queue_count);
        return 1;
      }
      (void)units_despawn(&pool, aid);
      if (units_get(&pool, did) && units_get(&pool, did)->active) {
        units_despawn(&pool, did);
      }
      fprintf(stderr, "unit_units: privateer SEIZURE popup ok\n");
  combat_phase2_close();
  return 0;
}

/*
 * bugs.md #867 — FUN_5fef_0352 raw 99562-99565: during the WoI the crown's
 * LAST Man-O-War cannot be sunk, only damaged. The same fight with a second
 * crown hull on the books sinks her.
 */
static int case_combat_crown_last_mow_unsinkable(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
  const int priv = units_find_type(&pool, "Privateer");
  const int mow_ti = units_kind_type_index(&pool, UNITS_KIND_MAN_O_WAR);
  if (priv < 0 || mow_ti < 0) {
    fprintf(stderr, "crown-MoW types missing\n");
    return 1;
  }
  pool.types[priv].attack = 99;
  pool.types[priv].guns = 99;
  pool.types[mow_ti].defense = 1;
  pool.types[mow_ti].hull = 0; /* no-rng path: hull < guns → SUNK */
  for (int pass = 0; pass < 2; ++pass) {
    ai_popup_clear(&pops);
    const int aid = units_spawn_allow_stack(&pool, priv, 4, 4);
    const int did = units_spawn_allow_stack(&pool, mow_ti, 5, 4);
    ColonizeUnit* a = units_get(&pool, aid);
    ColonizeUnit* d = units_get(&pool, did);
    if (!a || !d) {
      fprintf(stderr, "crown-MoW spawn failed\n");
      return 1;
    }
    a->nation_id = 0;
    d->nation_id = 1;
    ColonizeCol1Save c1;
    memset(&c1, 0, sizeof(c1));
    c1.head.human_player = 0;
    c1.head.crown_nation_id = 1;
    c1.head.game_options.woi = 1;
    c1.player[0].control = 0;
    c1.player[1].control = 2;
    /* 1 own Man-O-War = the last one (pass 0); 2 = expendable (pass 1). */
    c1.stuff.unit_type_counts[1][mow_ti] = (uint8_t)(pass == 0 ? 1 : 2);
    (void)units_resolve_naval_combat_ff_w(
      &(ColonizeWorld){.units = &pool, .col1 = &c1, .col1_ok = true, .rng = NULL}, aid, did
    );
    const ColonizeUnit* after = units_get(&pool, did);
    const int alive = after && after->active;
    if (pass == 0 && !alive) {
      fprintf(stderr, "crown last Man-O-War must survive damaged, was sunk\n");
      return 1;
    }
    if (pass == 1 && alive) {
      fprintf(stderr, "crown Man-O-War #2 should sink, survived\n");
      return 1;
    }
    if (pass == 0 && (after->col1_flags15 & 0x80u) == 0) {
      fprintf(stderr, "crown last Man-O-War survived without the damage bit\n");
      return 1;
    }
    (void)units_despawn(&pool, aid);
    if (units_get(&pool, did) && units_get(&pool, did)->active) {
      units_despawn(&pool, did);
    }
  }
  fprintf(stderr, "unit_units: crown last Man-O-War unsinkable ok\n");
  combat_phase2_close();
  return 0;
}

    /* Fort fire: miss → MP slow only; hit close → bit7 damage; Drydock repairs. */
static int case_combat_fort_fire_repair(void) {
  AiPopupState pops;
  if (combat_phase2_open(&pops) != 0) {
    return 1;
  }
      ColonizeColonyPool colonies;
      colonies_init(&colonies);
      colonies_set_occupancy_map(NULL);
      snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fort");
      snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Drydock");
      colonies.building_type_count = 4;
      int cx = 1, cy = 1, wx = 2, wy = 1;
      for (int y = 1; y < map.height - 1; ++y) {
        for (int x = 1; x < map.width - 1; ++x) {
          if (!map_tile_is_land(&map, x, y)) {
            continue;
          }
          static const int dx8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
          static const int dy8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
          for (int d = 0; d < 8; ++d) {
            if (map_tile_is_water(&map, x + dx8[d], y + dy8[d])) {
              cx = x;
              cy = y;
              wx = x + dx8[d];
              wy = y + dy8[d];
              goto found_coast_bit7;
            }
          }
        }
      }
    found_coast_bit7:
      ColonizeColony* col = &colonies.colonies[0];
      col->active = true;
      col->nation_id = 0;
      col->x = cx;
      col->y = cy;
      col->has_building[1] = true;
      colonies.colony_count = 1;
      const int car = units_find_type(&pool, "Caravel");
      const int sid_miss = units_spawn_allow_stack(&pool, car, wx, wy);
      ColonizeUnit* ship = units_get(&pool, sid_miss);
      ship->nation_id = 1;
      ship->moves = 4 * UNITS_MP_PER_TILE;
      ship->col1_flags15 = 0;
      ship->col1_counter16 = 0;
      /* Fort atk=4 (tier1, 0 arty); ship defense 99 → fort miss, MP drain only. */
      pool.types[car].attack = 2;
      pool.types[car].defense = 99;
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
        c1.head.founding_father[i] = -1;
      }
      ai_diplo_declare_war(&c1, 0, 1);
      if (!ai_diplo_at_war(&c1, 0, 1)) {
        fprintf(stderr, "fort bit7 smoke: declare_war failed\n");
        return 1;
      }
      char st[64];
      (void)units_coastal_fort_fire_pulse_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, -1, st, sizeof(st));
      ship = units_get(&pool, sid_miss);
      if (!ship || !ship->active) {
        fprintf(stderr, "fort miss should leave ship alive\n");
        return 1;
      }
      if ((ship->col1_flags15 & 0x80u) != 0) {
        fprintf(stderr, "fort miss must not set damaged bit7\n");
        return 1;
      }
      /* bugs.md #249: a fort miss leaves the ship untouched (no MP drain). */
      if (ship->moves == 0) {
        fprintf(stderr, "fort miss must NOT drain moves (got %d)\n", ship->moves);
        return 1;
      }
      units_despawn(&pool, sid_miss);

      /* Close fort hit: defense*2 > atk and atk >= defense (null rng) → bit7. */
      const int sid_hit = units_spawn_allow_stack(&pool, car, wx, wy);
      ship = units_get(&pool, sid_hit);
      ship->nation_id = 1;
      ship->moves = 4 * UNITS_MP_PER_TILE;
      ship->col1_flags15 = 0;
      ship->col1_counter16 = 0;
      pool.types[car].defense = 3; /* fort atk 4 wins the fight */
      /* Damage-vs-sink uses the loser's @UNIT hull against the fort's strength
       * (4): keep hull clear of it so this asserts the damaged path and not
       * the guns == hull coin flip. See units_ship_damage_vs_sink. */
      pool.types[car].hull = 8;
      (void)units_coastal_fort_fire_pulse_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL)}, -1, st, sizeof(st));
      ship = units_get(&pool, sid_hit);
      if (!ship || !ship->active) {
        fprintf(stderr, "close fort hit should damage not sink\n");
        return 1;
      }
      if ((ship->col1_flags15 & 0x80u) == 0) {
        fprintf(stderr, "close fort hit must set damaged bit7\n");
        return 1;
      }
      /* bugs.md #254: combat damage presets the repair TIMER below the
       * threshold (fort winner doubles the bill → remaining = full
       * threshold here) and marks repair_pending. */
      if (ship->col1_counter16 >= 3 || !ship->repair_pending) {
        fprintf(stderr, "combat damage should preset repair timer (worked=%d pending=%d)\n",
                ship->col1_counter16, ship->repair_pending);
        return 1;
      }
      /* The EOT ship tick counts the timer but never clears a repair —
       * the repair tick does that once the threshold is reached. */
      (void)units_tick_ship_build_ready(
        &pool, &colonies, 1, -1, st, sizeof(st), NULL
      );
      ship = units_get(&pool, sid_hit);
      if (!ship || (ship->col1_flags15 & 0x80u) == 0) {
        fprintf(stderr, "ship-build must leave combat bit7 set\n");
        return 1;
      }
      /* In port (double speed) the timer finishes; the repair tick clears. */
      ship->x = cx;
      ship->y = cy;
      ship->nation_id = 0;
      col->has_building[3] = true;
      int repaired = 0;
      for (int t = 0; t < 4 && !repaired; ++t) {
        (void)units_tick_ship_build_ready(&pool, &colonies, 0, -1, st, sizeof(st), NULL);
        repaired = units_tick_drydock_repair(
          &pool, &colonies, 0, 0, st, sizeof(st), NULL, NULL
        );
      }
      ship = units_get(&pool, sid_hit);
      if (repaired != 1 || !ship || (ship->col1_flags15 & 0x80u) != 0) {
        fprintf(stderr, "repair timer should clear combat bit7 (repaired=%d)\n", repaired);
        return 1;
      }
      units_despawn(&pool, sid_hit);
      fprintf(stderr, "unit_units: fort bit7 + timed repair ok\n");
  combat_phase2_close();
  return 0;
}


  /* Colony-tile visibility: a colony square never shows a non-selected
   * (idle garrison) unit — only the active/selected unit, and only while
   * it's actually visible (blink on, or mid-move). Player-reported bug on
   * the overland map; see units_top_on_map_tile. */
static int case_colony_tile_visibility(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int tx = -1;
    int ty = -1;
    for (int y = 5; y < (int)map.height - 5 && tx < 0; ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (!map_tile_is_land(&map, x, y)) {
          continue;
        }
        bool occupied = false;
        for (int i = 0; i < pool.unit_count; ++i) {
          if (pool.units[i].active && pool.units[i].x == x && pool.units[i].y == y) {
            occupied = true;
            break;
          }
        }
        if (!occupied) {
          tx = x;
          ty = y;
          break;
        }
      }
    }
    if (tx < 0) {
      fprintf(stderr, "colony-tile visibility test: no free land tile found\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int saved_selected = pool.selected_id;
    const int garrison = units_spawn_allow_stack(&pool, pioneer, tx, ty);
    const int active = units_spawn_allow_stack(&pool, pioneer, tx, ty);
    pool.selected_id = active;

    /*
     * Blink-off leaves the tile empty even on plain ground: the active unit
     * owns its tile, and letting the rest of the stack stand in for it made
     * the tile alternate between two units so you could not tell which was
     * active (bugs.md). This used to assert the opposite.
     */
    int top = units_top_on_map_tile(&pool, tx, ty, false, &map);
    if (top != -1) {
      fprintf(
        stderr, "non-colony blink-off should leave the tile empty, got %d (garrison %d)\n",
        top, garrison
      );
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* With nothing selected there, the stack's top unit shows as before. */
    pool.selected_id = -1;
    top = units_top_on_map_tile(&pool, tx, ty, false, &map);
    if (top != garrison && top != active) {
      fprintf(stderr, "unselected stack should still draw its top unit, got %d\n", top);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pool.selected_id = active;

    map_occupancy_set_layer2(&map, tx, ty, MAP_OCCUPANCY_HAS_CITY, true);

    /* Colony tile, blink-off: nothing shows — not even the garrison unit. */
    top = units_top_on_map_tile(&pool, tx, ty, false, &map);
    if (top != -1) {
      fprintf(stderr, "colony tile blink-off should hide garrison unit, got %d\n", top);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /* Colony tile, blink-on: the selected unit shows normally. */
    top = units_top_on_map_tile(&pool, tx, ty, true, &map);
    if (top != active) {
      fprintf(
        stderr, "colony tile blink-on expected selected unit %d, got %d\n", active, top
      );
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    map_occupancy_set_layer2(&map, tx, ty, MAP_OCCUPANCY_HAS_CITY, false);
    units_despawn(&pool, garrison);
    units_despawn(&pool, active);
    pool.selected_id = saved_selected;
    fprintf(stderr, "unit_units: colony-tile garrison visibility ok\n");
  fx_close();
  return 0;
}

  /* bugs.md: a selected passenger is not on the map, so the tile scan never
   * found it and the ship drew steadily — no blink. The passenger owns its
   * ship's tile like any active unit: own sprite blink-on, empty off. */
static int case_selected_passenger_blink(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int wx = -1, wy = -1;
    for (int y = 5; y < (int)map.height - 5 && wx < 0; ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (map_tile_is_water(&map, x, y)) {
          wx = x;
          wy = y;
          break;
        }
      }
    }
    if (wx < 0) {
      fprintf(stderr, "pax blink test: no water tile\n");
      return 1;
    }
    const int saved_selected = pool.selected_id;
    const int caravel = units_find_type(&pool, "Caravel");
    const int ship = units_spawn_allow_stack(&pool, caravel, wx, wy);
    const int pax = units_spawn_allow_stack(&pool, pioneer, wx, wy);
    ColonizeUnit* pu = units_get(&pool, pax);
    pu->aboard_ship_id = ship;
    pu->orders = 0;
    ColonizeUnit* su = units_get(&pool, ship);
    su->cargo_count = 1;
    pool.selected_id = pax;
    if (units_top_on_map_tile(&pool, wx, wy, true, &map) != pax) {
      fprintf(stderr, "selected passenger should show on blink-on\n");
      return 1;
    }
    if (units_top_on_map_tile(&pool, wx, wy, false, &map) != -1) {
      fprintf(stderr, "selected passenger tile should be empty off-blink\n");
      return 1;
    }
    pool.selected_id = ship;
    if (units_top_on_map_tile(&pool, wx, wy, true, &map) != ship) {
      fprintf(stderr, "selected ship should still show itself\n");
      return 1;
    }
    units_despawn(&pool, pax);
    units_despawn(&pool, ship);
    pool.selected_id = saved_selected;
    fprintf(stderr, "unit_units: passenger blink ownership ok\n");
  fx_close();
  return 0;
}

  /* DOS ship-switch quirk (bugs.md): the first ship to leave a shared tile
   * scoops the tile's loaded units first come, first served; the rest stay
   * with the remaining ship(s). Awake passengers stay put. */
static int case_ship_switch_quirk(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int wx = -1, wy = -1;
    for (int y = 5; y < (int)map.height - 5 && wx < 0; ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (map_tile_is_water(&map, x, y)) {
          wx = x;
          wy = y;
          break;
        }
      }
    }
    if (wx < 0) {
      fprintf(stderr, "ship-switch test: no water tile\n");
      return 1;
    }
    const int caravel = units_find_type(&pool, "Caravel");
    /* Spawn order fixes id order: shipA (empty), then shipB with 2 pax. */
    const int shipA = units_spawn_allow_stack(&pool, caravel, wx, wy);
    const int shipB = units_spawn_allow_stack(&pool, caravel, wx, wy);
    const int pax1 = units_spawn_allow_stack(&pool, pioneer, wx, wy);
    const int pax2 = units_spawn_allow_stack(&pool, pioneer, wx, wy);
    ColonizeUnit* sB = units_get(&pool, shipB);
    for (int p = 0; p < 2; ++p) {
      ColonizeUnit* pu = units_get(&pool, p == 0 ? pax1 : pax2);
      pu->aboard_ship_id = shipB;
      pu->orders = UNITS_ORDER_SENTRY;
      pu->moves = 0;
      sB->cargo_ids[sB->cargo_count++] = pu->id;
    }
    /* Ship A departs first: takes both of B's sentried passengers. */
    if (units_ship_departure_pickup(&pool, shipA, wx, wy) != 2) {
      fprintf(stderr, "ship-switch: departing ship should scoop both passengers\n");
      return 1;
    }
    ColonizeUnit* sA = units_get(&pool, shipA);
    if (sA->cargo_count != 2 || sB->cargo_count != 0 ||
        units_get(&pool, pax1)->aboard_ship_id != shipA ||
        units_get(&pool, pax2)->aboard_ship_id != shipA) {
      fprintf(stderr, "ship-switch: passengers should ride the departing ship\n");
      return 1;
    }
    /* Awake passenger stays with its own ship. */
    units_get(&pool, pax1)->orders = UNITS_ORDER_NONE;
    if (units_ship_departure_pickup(&pool, shipB, wx, wy) != 1 ||
        units_get(&pool, pax1)->aboard_ship_id != shipA ||
        units_get(&pool, pax2)->aboard_ship_id != shipB) {
      fprintf(stderr, "ship-switch: awake passenger must not transfer\n");
      return 1;
    }
    units_despawn(&pool, pax1);
    units_despawn(&pool, pax2);
    units_despawn(&pool, shipA);
    units_despawn(&pool, shipB);
    fprintf(stderr, "unit_units: ship-switch departure pickup ok\n");
  fx_close();
  return 0;
}

  /* bugs.md: Go To onto a fogged square is always legal — the destination
   * check must not peek under the fog. Seen, it still validates. */
static int case_goto_fogged_square(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int wx = -1, wy = -1, lx = -1, ly = -1;
    for (int y = 5; y < (int)map.height - 5 && (wx < 0 || lx < 0); ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (wx < 0 && map_tile_is_water(&map, x, y)) {
          wx = x;
          wy = y;
        } else if (lx < 0 && map_tile_is_land(&map, x, y)) {
          lx = x;
          ly = y;
        }
      }
    }
    if (wx < 0 || lx < 0) {
      fprintf(stderr, "fog goto test: tiles not found\n");
      return 1;
    }
    const int caravel = units_find_type(&pool, "Caravel");
    const int ship = units_spawn_allow_stack(&pool, caravel, wx, wy);
    ColonizeUnit* su = units_get(&pool, ship);
    su->nation_id = 0;
    const size_t li = (size_t)ly * (size_t)map.width + (size_t)lx;
    const uint8_t saved_seen = map.seen[li];
    map.seen[li] = 0; /* fog the land tile for everyone */
    if (!units_set_goto_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, ship, lx, ly)) {
      fprintf(stderr, "fog goto: order onto an unseen tile must be accepted\n");
      return 1;
    }
    units_clear_orders(&pool, ship);
    map.seen[li] = saved_seen; /* fully explored again */
    if (units_set_goto_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, ship, lx, ly)) {
      fprintf(stderr, "fog goto: a SEEN land tile must still refuse a ship goto\n");
      return 1;
    }
    units_despawn(&pool, ship);
    fprintf(stderr, "unit_units: fogged goto destination ok\n");
  fx_close();
  return 0;
}

  /*
   * bugs.md #418: a Go To aimed at an Indian settlement is a move command INTO
   * the village — on arrival its final step must be dispatched through the
   * normal entry flow (game_loop hands it to game_try_unit_move →
   * FUN_4d56_4528 @ACTIONS), not stopped one tile short. The raw pacer has no
   * popup channel and must still refuse to walk in silently.
   */
static int case_goto_into_village(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int ux = -1, uy = -1, vx = -1, vy = -1;
    for (int y = 5; y < (int)map.height - 5 && ux < 0; ++y) {
      for (int x = 5; x < (int)map.width - 6; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y) &&
            !map_tile_has_city(&map, x, y) && !map_tile_has_city(&map, x + 1, y) &&
            units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
          ux = x;
          uy = y;
          vx = x + 1;
          vy = y;
          break;
        }
      }
    }
    if (ux < 0) {
      fprintf(stderr, "goto-village: no adjacent land pair\n");
      return 1;
    }
    const size_t vi = (size_t)vy * (size_t)map.width + (size_t)vx;
    const uint8_t saved_l2 = map.layer2[vi];
    map.layer2[vi] |= MAP_OCCUPANCY_HAS_CITY; /* native village, no colony */

    const int walker = units_spawn(&pool, pioneer, ux, uy);
    ColonizeUnit* wu = units_get(&pool, walker);
    if (walker < 0 || !wu) {
      fprintf(stderr, "goto-village: spawn failed\n");
      return 1;
    }
    wu->nation_id = 0;
    wu->moves = units_max_mp(&pool, walker);
    /* bugs.md #129: the order itself is legal even though the tile is not
     * enterable by a plain settler. */
    if (!units_set_goto_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, walker, vx, vy)) {
      fprintf(stderr, "goto-village: village destination must be accepted\n");
      return 1;
    }
    if (!units_goto_dest_is_village_entry_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, walker)) {
      fprintf(stderr, "goto-village: adjacent arrival must dispatch the entry step\n");
      return 1;
    }
    /* Not adjacent yet → still an ordinary en-route step, no dispatch. */
    wu->x = ux;
    wu->y = uy + 3;
    wu->orders = UNITS_ORDER_GOTO;
    wu->goto_x = (uint8_t)vx;
    wu->goto_y = (uint8_t)vy;
    if (units_goto_dest_is_village_entry_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, walker)) {
      fprintf(stderr, "goto-village: en-route step must not dispatch the entry\n");
      return 1;
    }
    /* Adjacent to a village the path merely brushes past (not the ordered
     * destination) → no dispatch either. */
    wu->x = ux;
    wu->y = uy;
    wu->goto_x = (uint8_t)ux;
    wu->goto_y = (uint8_t)(uy + 2);
    if (units_goto_dest_is_village_entry_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, walker)) {
      fprintf(stderr, "goto-village: only the ORDERED village tile dispatches\n");
      return 1;
    }
    units_despawn(&pool, walker);
    map.layer2[vi] = saved_l2;
    fprintf(stderr, "unit_units: goto village entry dispatch ok\n");
  fx_close();
  return 0;
}

  /*
   * bugs.md #423 / DOS FUN_4720_015c (viceroy_unpacked.c:76010-76026): landfall
   * is offered only to cargo whose spent byte is below its max. A passenger
   * parked at moves 0 by boarding is still fresh (DOS spent 0) and may
   * land; one that burnt its allotment this turn stays aboard.
   */
static int case_landfall_spent_gate(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    int wx = -1, wy = -1, lx = -1, ly = -1;
    for (int y = 5; y < (int)map.height - 5 && wx < 0; ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (!map_tile_is_water(&map, x, y) || units_id_at(&pool, x, y) >= 0) {
          continue;
        }
        static const int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        static const int dy8[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        for (int d = 0; d < 8; ++d) {
          const int tx = x + dx8[d];
          const int ty = y + dy8[d];
          if (map_tile_is_land(&map, tx, ty) && !map_tile_has_city(&map, tx, ty) &&
              units_id_at(&pool, tx, ty) < 0) {
            wx = x;
            wy = y;
            lx = tx;
            ly = ty;
            break;
          }
        }
        if (wx >= 0) {
          break;
        }
      }
    }
    if (wx < 0) {
      fprintf(stderr, "landfall-spent: no coast pair\n");
      return 1;
    }
    const int boat = units_spawn(&pool, caravel, wx, wy);
    const int spent_pax = units_spawn_allow_stack(&pool, pioneer, lx, ly);
    const int fresh_pax = units_spawn_allow_stack(&pool, pioneer, lx, ly);
    if (boat < 0 || spent_pax < 0 || fresh_pax < 0 ||
        !units_board(&pool, spent_pax, boat) || !units_board(&pool, fresh_pax, boat)) {
      fprintf(stderr, "landfall-spent: setup failed\n");
      return 1;
    }
    units_get(&pool, boat)->nation_id = 0;
    units_get(&pool, spent_pax)->nation_id = 0;
    units_get(&pool, fresh_pax)->nation_id = 0;
    /* Both parked at 0 by boarding: both eligible, first one wins. */
    if (units_first_landfall_cargo(&pool, boat) != spent_pax) {
      fprintf(stderr, "landfall-spent: parked-fresh cargo must be eligible\n");
      return 1;
    }
    /* Now mark the first one as having spent its allotment this turn. */
    units_get(&pool, spent_pax)->mp_spent_turn = 1;
    units_get(&pool, spent_pax)->aboard_moves = 0; /* what try_move's shore arm writes */
    if (units_cargo_can_landfall(&pool, spent_pax)) {
      fprintf(stderr, "landfall-spent: spent pax must not be eligible\n");
      return 1;
    }
    if (units_first_landfall_cargo(&pool, boat) != fresh_pax) {
      fprintf(stderr, "landfall-spent: offer must skip to the fresh passenger\n");
      return 1;
    }
    if (units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, boat, spent_pax, lx, ly)) {
      fprintf(stderr, "landfall-spent: spent pax must not be put ashore\n");
      return 1;
    }
    if (units_get(&pool, spent_pax)->aboard_ship_id != boat) {
      fprintf(stderr, "landfall-spent: refused pax must stay on the ship\n");
      return 1;
    }
    if (!units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map)}, boat, fresh_pax, lx, ly)) {
      fprintf(stderr, "landfall-spent: parked-fresh pax must be able to land\n");
      return 1;
    }
    /* Every passenger spent → no offer at all (DOS leaves reason 0). */
    if (units_first_landfall_cargo(&pool, boat) >= 0) {
      fprintf(stderr, "landfall-spent: all-spent cargo must offer nobody\n");
      return 1;
    }
    /*
     * bugs.md #544: docking is not a refill. DOS keeps the passenger's own
     * spent byte aboard: the shore-boarded (spent) pioneer lands in the colony
     * with no moves, one that boarded with 1 third left lands with 1.
     */
    if (!units_board(&pool, fresh_pax, boat)) {
      fprintf(stderr, "dock-spent: re-board failed\n");
      return 1;
    }
    units_get(&pool, fresh_pax)->mp_spent_turn = 0;
    units_get(&pool, fresh_pax)->aboard_moves = 1;
    if (units_disembark_all(&pool, boat, wx, wy) != 2) {
      fprintf(stderr, "dock-spent: both passengers must go ashore\n");
      return 1;
    }
    if (units_get(&pool, spent_pax)->moves != 0) {
      fprintf(stderr, "dock-spent: spent pax must land with no moves\n");
      return 1;
    }
    if (units_get(&pool, fresh_pax)->moves != 1) {
      fprintf(stderr, "dock-spent: partly-spent pax must land with its remainder\n");
      return 1;
    }
    units_despawn(&pool, fresh_pax);
    units_despawn(&pool, spent_pax);
    units_despawn(&pool, boat);
    fprintf(stderr, "unit_units: landfall spent-passenger gate ok\n");
  fx_close();
  return 0;
}

  /*
   * P7.2 Fountain of Youth = 8× FUN_38fd_4884(1,0): a 3-way @RECRUIT CHOICE
   * per pick, free passage, no recruit-count bump, chained until 8 landed.
   */
static int case_fountain_of_youth(void) {
  if (fx_open() != 0 || fx_stage2() != 0) {
    return 1;
  }
    EuropeScreen feu;
    char eerr[256];
    if (!europe_load(&feu, "COLONIZE", eerr, sizeof(eerr))) {
      fprintf(stderr, "fountain: europe_load failed: %s\n", eerr);
      return 1;
    }
    feu.gold = 123;
    feu.dock_count = 0;
    const uint8_t rc0 = feu.recruit_count;
    AiPopupState fpops;
    ai_popup_init(&fpops);
    units_fountain_youth_enqueue_pick(&feu, &fpops, NULL, 0, 8);
    int picks = 0;
    for (int guard = 0; guard < 20 && ai_popup_busy(&fpops); ++guard) {
      if (!fpops.open && !ai_popup_try_present_next(&fpops)) {
        break;
      }
      if (fpops.current.choice_count != 3 || fpops.current.tag != AI_POPUP_TAG_FOUNTAIN_YOUTH) {
        fprintf(stderr, "fountain: pick %d want 3-way FOUNTAIN_YOUTH choice\n", picks);
        return 1;
      }
      ColonizeInputState in = {0};
      in.last_key = COLONIZE_KEY_DOWN; /* pick slot 1 each time */
      ai_popup_handle_input(&fpops, &in);
      in.last_key = COLONIZE_KEY_ENTER;
      ai_popup_handle_input(&fpops, &in);
      if (!fpops.has_result) {
        fprintf(stderr, "fountain: no result after Enter\n");
        return 1;
      }
      if (!units_fountain_youth_apply_popup(&feu, &fpops, NULL)) {
        fprintf(stderr, "fountain: apply rejected\n");
        return 1;
      }
      ai_popup_consume_result(&fpops);
      picks++;
    }
    if (picks != 8 || feu.dock_count != 8 || feu.gold != 123 || feu.recruit_count != rc0 ||
        ai_popup_busy(&fpops)) {
      fprintf(
        stderr, "fountain: picks=%d dock=%d gold=%d rc=%u/%u busy=%d\n", picks, feu.dock_count,
        feu.gold, feu.recruit_count, rc0, ai_popup_busy(&fpops)
      );
      return 1;
    }
    for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
      if (!feu.pool[i].filled) {
        fprintf(stderr, "fountain: pool slot %d not refilled\n", i);
        return 1;
      }
    }
    europe_free(&feu);
    fprintf(stderr, "fountain of youth 8x free recruit pick ok\n");
  fx_close();
  return 0;
}

static const TestCase k_cases[] = {
  {"starter_fleet", case_starter_fleet},
  {"starter_discoverer_england", case_starter_discoverer_england},
  {"starter_discoverer_french", case_starter_discoverer_french},
  {"starter_discoverer_ai_english", case_starter_discoverer_ai_english},
  {"starter_conquistador_dutch", case_starter_conquistador_dutch},
  {"domain_stack_despawn", case_domain_stack_despawn},
  {"boarding_caravel", case_boarding_caravel},
  {"landfall_unload", case_landfall_unload},
  {"colony_dock", case_colony_dock},
  {"terrain_mp_improvements", case_terrain_mp_improvements},
  {"unit_icons", case_unit_icons},
  {"goto_pathfinding", case_goto_pathfinding},
  {"orders_chrome", case_orders_chrome},
  {"fortify_sentry_disband", case_fortify_sentry_disband},
  {"dump_anchor_route_pillage", case_dump_anchor_route_pillage},
  {"land_combat_t0", case_land_combat_t0},
  {"follow_unit", case_follow_unit},
  {"naval_hold_plunder", case_naval_hold_plunder},
  {"treasure_train_spawn", case_treasure_train_spawn},
  {"treasure_ship_holds", case_treasure_ship_holds},
  {"native_settlement_conquer", case_native_settlement_conquer},
  {"convert_join", case_convert_join},
  {"convert_desertion", case_convert_desertion},
  {"fort_defense_bonus", case_fort_defense_bonus},
  {"coastal_fort_naval_fire", case_coastal_fort_naval_fire},
  {"lcr_clear_desoto", case_lcr_clear_desoto},
  {"lcr_case_matrix", case_lcr_case_matrix},
  {"lcr_case5_latch", case_lcr_case5_latch},
  {"lcr_outcome_dispatch", case_lcr_outcome_dispatch},
  {"lcr_fountain_ai", case_lcr_fountain_ai},
  {"lcr_burial_trespass", case_lcr_burial_trespass},
  {"enter_probe_matrix", case_enter_probe_matrix},
  {"board_from_land_sentry", case_board_from_land_sentry},
  {"combat_ambush_spanish", case_combat_ambush_spanish},
  {"combat_terrain_stash", case_combat_terrain_stash},
  {"combat_woi_ref_support", case_combat_woi_ref_support},
  {"combat_best_defender", case_combat_best_defender},
  {"combat_capture_alive_colonist", case_combat_capture_alive_colonist},
  {"combat_native_win_destroy_demote", case_combat_native_win_destroy_demote},
  {"combat_naval_damage_not_sink", case_combat_naval_damage_not_sink},
  {"combat_outcome_popups", case_combat_outcome_popups},
  {"combat_village_attack_pop_drain", case_combat_village_attack_pop_drain},
  {"combat_undefended_colony_militia", case_combat_undefended_colony_militia},
  {"combat_militia_phantom_row", case_combat_militia_phantom_row},
  {"combat_discoverer_beginner_shield", case_combat_discoverer_beginner_shield},
  {"combat_port_ship_fate_discharge", case_combat_port_ship_fate_discharge},
  {"combat_treasure_capture", case_combat_treasure_capture},
  {"combat_capture_water_destroy", case_combat_capture_water_destroy},
  {"combat_colony_capture_notify", case_combat_colony_capture_notify},
  {"combat_privateer_seizure", case_combat_privateer_seizure},
  {"combat_crown_last_mow_unsinkable", case_combat_crown_last_mow_unsinkable},
  {"combat_fort_fire_repair", case_combat_fort_fire_repair},
  {"colony_tile_visibility", case_colony_tile_visibility},
  {"selected_passenger_blink", case_selected_passenger_blink},
  {"ship_switch_quirk", case_ship_switch_quirk},
  {"goto_fogged_square", case_goto_fogged_square},
  {"goto_into_village", case_goto_into_village},
  {"landfall_spent_gate", case_landfall_spent_gate},
  {"fountain_of_youth", case_fountain_of_youth},
};
TEST_MAIN(k_cases)
