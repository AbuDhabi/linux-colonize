#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
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

  if (units_try_move(&pool, starter->id, &map, ocean_x, ocean_y, NULL, NULL)) {
    fprintf(stderr, "land unit should not enter ocean\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (units_try_move(&pool, ship_id, &map, land_x, land_y, NULL, NULL)) {
    fprintf(stderr, "sea unit should not enter land\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (land_x + 1 < map.width &&
      units_try_move(&pool, starter->id, &map, land_x + 1, land_y, NULL, NULL)) {
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
    if (units_try_move(&pool, land_id, &map, land_tile_x, land_tile_y, NULL, NULL)) {
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
    if (units_try_move(&pool, lf_ship, &map, lx, ly, NULL, NULL)) {
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
    const int cid = colonies_found(&colonies, &map, cx, cy, -1, UNITS_JOB_NONE, 0, 0, 0);
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
    if (!units_try_move(&pool, dock_ship, &map, cx, cy, &colonies, NULL)) {
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

  /* Phase 7: terrain MP costs, pioneer plow/road, yield bonuses. */
  {
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
    for (int y = 0; y < 6 && px < 0; ++y) {
      for (int x = 0; x < 6; ++x) {
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
    if (map_move_cost_at(&tmap, fx, fy) != 2) {
      fprintf(stderr, "forest move cost expected 2 got %d\n", map_move_cost_at(&tmap, fx, fy));
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
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
    pu->moves_left = 1; /* pioneer max is 1 — full allotment */
    if (!units_try_move(&pool, pid, &tmap, fx, fy, NULL, NULL) || pu->moves_left != 0) {
      fprintf(
        stderr,
        "phase7 full-MP forest enter should succeed and exhaust (moves_left=%d)\n",
        pu->moves_left
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
    pu_partial->moves_left = 1;
    if (units_try_move(&pool, pid_partial, &tmap, fx, fy, NULL, NULL) ||
        pu_partial->moves_left != 1) {
      fprintf(
        stderr,
        "phase7 partial-MP without RNG should fail uncharged (moves=%d)\n",
        pu_partial->moves_left
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
      if (!units_try_move(&pool, pid_partial, &tmap, fx, fy, NULL, &rng) ||
          pu_partial->x != fx || pu_partial->y != fy || pu_partial->moves_left != 0) {
        fprintf(
          stderr,
          "phase7 RNG success should enter forest (pos=%d,%d moves=%d)\n",
          pu_partial->x,
          pu_partial->y,
          pu_partial->moves_left
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      /* Reset for fail case. */
      pu_partial->x = ox;
      pu_partial->y = oy;
      pu_partial->moves_left = 1;
      dos_rng_seed(&rng, 5006u); /* first range(1,2) → 2 → fail */
      if (units_try_move(&pool, pid_partial, &tmap, fx, fy, NULL, &rng) ||
          pu_partial->x != ox || pu_partial->y != oy || pu_partial->moves_left != 0) {
        fprintf(
          stderr,
          "phase7 RNG fail should stay and exhaust (pos=%d,%d moves=%d)\n",
          pu_partial->x,
          pu_partial->y,
          pu_partial->moves_left
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
    if (map_move_cost_at(&tmap, fx, fy) != 1) {
      fprintf(stderr, "roaded forest cost expected 1 got %d\n", map_move_cost_at(&tmap, fx, fy));
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu2->moves_left = 2;
    if (!units_try_move(&pool, pid2, &tmap, fx, fy, NULL, NULL) || pu2->moves_left != 1) {
      fprintf(stderr, "phase7 roaded forest move failed (moves_left=%d)\n", pu2->moves_left);
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
    pu3->moves_left = 1;
    if (!units_pioneer_road(&pool, pid3, &tmap, pmsg, sizeof(pmsg)) ||
        !map_tile_has_road(&tmap, px, py) || pu3->tools != 80 || pu3->moves_left != 0) {
      fprintf(
        stderr,
        "phase7 road failed tools=%d road=%d moves=%d (%s)\n",
        pu3->tools,
        (int)map_tile_has_road(&tmap, px, py),
        pu3->moves_left,
        pmsg
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu3->moves_left = 1;
    pu3->tools = 100;
    const int farm_base = colony_yield_for_tile(&tmap, px, py, COLONIZE_JOB_FARMER);
    if (!units_pioneer_plow(&pool, pid3, &tmap, pmsg, sizeof(pmsg)) ||
        !map_tile_is_plowed(&tmap, px, py) || pu3->tools != 80) {
      fprintf(stderr, "phase7 plow failed (%s)\n", pmsg);
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
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
    if (lumber_base != lumber_clear + 1) {
      fprintf(
        stderr,
        "phase7 road lumber yield expected %d got base=%d clear=%d\n",
        lumber_clear + 1,
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
      colony_yield_town_commons(&tmap, px, py, &tc);
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
      colony_yield_town_commons(&tmap, fx, fy, &tc);
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

  /* Go-to pathfinding: next step, spend MP, keep order, resume after end_turn. */
  {
    int lx = -1;
    int ly = -1;
    for (int y = 20; y < (int)map.height - 20 && lx < 0; ++y) {
      for (int x = 20; x < (int)map.width - 20; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 3, y + 2) &&
            map_tile_is_land(&map, x + 1, y) &&
            units_can_enter(&pool, pioneer, &map, x, y, -1, NULL) &&
            units_can_enter(&pool, pioneer, &map, x + 1, y, -1, NULL) &&
            units_can_enter(&pool, pioneer, &map, x + 3, y + 2, -1, NULL) &&
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
    if (!units_set_goto(&pool, uid, &map, gx, gy, NULL)) {
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
    if (!units_next_goto_step(&pool, uid, &map, NULL, &nx, &ny)) {
      fprintf(stderr, "units_next_goto_step failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int before_mp = walker->moves_left;
    units_advance_goto(&pool, uid, &map, NULL);
    walker = units_get(&pool, uid);
    if (!walker || (walker->x == lx && walker->y == ly && walker->moves_left >= before_mp)) {
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
      units_end_turn(&pool);
      walker = units_get(&pool, uid);
      const int refreshed = walker ? walker->moves_left : 0;
      const int px = walker ? walker->x : -1;
      const int py = walker ? walker->y : -1;
      units_advance_all_goto(&pool, &map, NULL);
      walker = units_get(&pool, uid);
      if (!walker) {
        fprintf(stderr, "walker missing after resume\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (walker->moves_left >= refreshed && walker->x == px && walker->y == py &&
          !(walker->x == gx && walker->y == gy)) {
        fprintf(stderr, "resume after end_turn made no progress\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
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
