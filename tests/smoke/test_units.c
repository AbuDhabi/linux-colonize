#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

int main(void) {
  diag_init(0, NULL);

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "failed to load NAMES.TXT\n");
    return 1;
  }

  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }

  const int pioneer = units_find_type(&pool, "Pioneers");
  const int colonist = units_find_type(&pool, "Colonists");
  const int caravel = units_find_type(&pool, "Caravel");
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

  ColonizeWorldMap map;
  char err[256];
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

  ColonizeUnit* ship = units_get(&pool, pool.selected_id);
  if (!ship || !units_is_sea(&pool, ship->id)) {
    fprintf(stderr, "selected starter should be the ship\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (!map_tile_is_high_seas(&map, ship->x, ship->y)) {
    fprintf(stderr, "starter ship not on eastern high seas (%d,%d)\n", ship->x, ship->y);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  /* Western rim: tile to the west should not be high seas. */
  if (ship->x > 0 && map_tile_is_high_seas(&map, ship->x - 1, ship->y)) {
    fprintf(
      stderr,
      "starter ship not on western rim of eastern high seas (%d,%d)\n",
      ship->x,
      ship->y
    );
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
  /* Discoverer (diff 0) England: Hardy Pioneer + Veteran Soldier. */
  {
    const ColonizeUnit* p0 = units_get_const(&pool, ship->cargo_ids[0]);
    const ColonizeUnit* p1 = units_get_const(&pool, ship->cargo_ids[1]);
    if (!p0 || !p1 || p0->profession != UNITS_JOB_PIONEER || p1->profession != UNITS_JOB_SOLDIER) {
      fprintf(
        stderr,
        "Discoverer England expected Hardy+Veteran professions (got %d,%d)\n",
        p0 ? p0->profession : -1,
        p1 ? p1->profession : -1
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }
  /* Conquistador (diff 2) Dutch: plain Pioneer + plain Soldier. */
  {
    ColonizeUnitPool hard;
    memset(&hard, 0, sizeof(hard));
    hard.type_count = pool.type_count;
    memcpy(hard.types, pool.types, sizeof(pool.types));
    const int sid = units_spawn_euro_starter_fleet(&hard, 3, 2, ship->x, ship->y, 39, 10);
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
  }
  int ship_id = ship->id;

  /* Separate land unit for domain / stacking tests (ship is offshore). */
  int land_x = 39;
  int land_y = 10;
  if (!map_tile_is_land(&map, land_x, land_y) || units_id_at(&pool, land_x, land_y) >= 0) {
    land_x = -1;
    for (int y = 0; y < map.height && land_x < 0; ++y) {
      for (int x = 0; x < map.width; ++x) {
        if (map_tile_is_land(&map, x, y) && units_id_at(&pool, x, y) < 0) {
          land_x = x;
          land_y = y;
          break;
        }
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
  ColonizeUnit* starter = units_get(&pool, land_id);
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

  if (units_try_move(&pool, starter->id, &map, ocean_x, ocean_y, NULL)) {
    fprintf(stderr, "land unit should not enter ocean\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (units_try_move(&pool, ship_id, &map, land_x, land_y, NULL)) {
    fprintf(stderr, "sea unit should not enter land\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (land_x + 1 < map.width &&
      units_try_move(&pool, starter->id, &map, land_x + 1, land_y, NULL)) {
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
    if (!units_can_enter(&pool, colonist, &map, starter->x, starter->y, stack_id, NULL)) {
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

  int edge_x = 0;
  int edge_y = 0;
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
  int unload_x = land_x;
  int unload_y = land_y;
  {
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
    if (boarded->orders != 1 || boarded->moves_left != 0) {
      fprintf(stderr, "board should set sentry and zero moves\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (units_try_move(&pool, land_id, &map, land_tile_x, land_tile_y, NULL)) {
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
    if (!units_despawn_ship_with_cargo(
          &pool,
          ship_id,
          &exported_type,
          exported_name,
          sizeof(exported_name),
          cargo_types,
          &cargo_count,
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
      &pool, caravel, hx, hy, cargo_types, cargo_count
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
        if (units_can_enter(&pool, pioneer, &map, tx, ty, -1, NULL)) {
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
          if (units_can_enter(&pool, pioneer, &map, tx, ty, -1, NULL)) {
            ux = tx;
            uy = ty;
            break;
          }
        }
      }
    }
    if (ux < 0 || !units_unload(&pool, returned, &map, ux, uy, NULL)) {
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
  }

  /* Landfall unload: cargo with moves onto adjacent land; ship stays put. */
  {
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
    lf_cargo->moves_left = 1;
    if (units_try_move(&pool, lf_ship, &map, lx, ly, NULL)) {
      fprintf(stderr, "ship must not enter plain land via try_move\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (units_first_cargo_with_moves(&pool, lf_ship) != lf_pax) {
      fprintf(stderr, "first cargo with moves mismatch\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_unload_passenger(&pool, lf_ship, lf_pax, &map, lx, ly, NULL)) {
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
    units_despawn(&pool, lf_pax);
    units_despawn(&pool, lf_ship);
  }

  /* Colony dock: ship may enter own colony; disembark clears sentry. */
  {
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
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
    const int cid = colonies_found(&colonies, &map, cx, cy, -1, 0, 0, 0);
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
    if (!units_can_enter(&pool, caravel, &map, cx, cy, dock_ship, &colonies)) {
      fprintf(stderr, "ship should enter own colony tile\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_try_move(&pool, dock_ship, &map, cx, cy, &colonies)) {
      fprintf(stderr, "ship try_move onto colony failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int nd = units_disembark_all(&pool, dock_ship, cx, cy);
    dpax = units_get(&pool, dock_pax);
    if (nd != 1 || !dpax || dpax->aboard_ship_id >= 0 || dpax->orders != 0 ||
        dpax->x != cx || dpax->y != cy) {
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
  }

  ColonizeSpriteSheet icons;
  char ss_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "ICONS.SS", ss_path, sizeof(ss_path)) ||
      !ss_load(ss_path, &icons, err, sizeof(err))) {
    fprintf(stderr, "ICONS load failed: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  const int icon = pool.types[pioneer].icon_sprite;
  const int ship_icon = pool.types[caravel].icon_sprite;
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

  fprintf(
    stderr,
    "units tests ok (types=%d pioneer@%d,%d caravel_icon=%d edge=%d,%d)\n",
    pool.type_count,
    unload_x,
    unload_y,
    ship_icon,
    edge_x,
    edge_y
  );

  ss_free(&icons);
  map_free(&map);
  assets_msg_free(&names);
  diag_shutdown();
  return 0;
}
