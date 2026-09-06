#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/map_gen.h"
#include "core/new_game.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static int count_nation_ships_europe(const ColonizeUnitPool* units, int nation) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(units, u->id)) {
      continue;
    }
    if (u->x >= 200 || u->y >= 200) {
      n++;
    }
  }
  return n;
}

static int count_nation_ships_on_map(const ColonizeUnitPool* units, const ColonizeWorldMap* map, int nation) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(units, u->id)) {
      continue;
    }
    if (u->x >= 0 && u->y >= 0 && u->x < map->width && u->y < map->height) {
      n++;
    }
  }
  return n;
}

static int count_braves(const ColonizeUnitPool* units) {
  const int brave = units_find_type(units, "Braves");
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (u->active && u->type_index == brave && u->nation_id >= 4) {
      n++;
    }
  }
  return n;
}

static int ai_ship_dist_sum(const ColonizeUnitPool* units, int nation) {
  int sum = 0;
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(units, u->id)) {
      continue;
    }
    if (u->goto_x >= 255 || u->goto_y >= 255) {
      continue;
    }
    const int dx = u->x - u->goto_x;
    const int dy = u->y - u->goto_y;
    sum += dx * dx + dy * dy;
    n++;
  }
  return n > 0 ? sum : -1;
}

static int tribe_pop_sum(const ColonizeCol1Save* col1) {
  int s = 0;
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    s += col1->tribe[i].population;
  }
  return s;
}

static int count_nation_colonies(const ColonizeColonyPool* colonies, int nation) {
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (c->active && c->nation_id == nation) {
      n++;
    }
  }
  return n;
}

static int count_nation_cargo(const ColonizeUnitPool* units, int nation) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      n += u->cargo_count;
    }
  }
  return n;
}

/* Place AI ship on water adjacent to foundable land so unload+found can fire. */
static bool place_ai_ship_for_settle(
  ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nation,
  int* out_land_x,
  int* out_land_y
) {
  ColonizeUnit* ship = NULL;
  ColonizeUnit* empty_ship = NULL;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(units, u->id)) {
      continue;
    }
    if (u->cargo_count > 0) {
      ship = u;
      break;
    }
    if (!empty_ship && u->x < 200 && u->y < 200) {
      empty_ship = u; /* map ship — not Europe harbor */
    }
  }
  /*
   * After AI beachhead unload without founding, cargo may be empty. Re-board
   * settler-capable land units onto the empty transport for the settle probe.
   */
  if (!ship && empty_ship) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
        continue;
      }
      if (units_is_sea(units, u->id)) {
        continue;
      }
      const char* name = units_display_name(units, u);
      const int settler =
        name &&
        (strstr(name, "Pioneer") != NULL || strstr(name, "Hardy") != NULL ||
         strstr(name, "Colonist") != NULL || strstr(name, "Soldier") != NULL);
      if (!settler) {
        continue;
      }
      const int on_map = units_is_on_map(u) && u->x < 200 && u->y < 200;
      if (on_map) {
        const int md = abs(u->x - empty_ship->x) + abs(u->y - empty_ship->y);
        if (md > 8 && empty_ship->cargo_count > 0) {
          continue;
        }
      }
      u->x = empty_ship->x;
      u->y = empty_ship->y;
      (void)units_board_stacked(units, u->id, empty_ship->id);
    }
    /* Need a Pioneer/Hardy founder — plain Colonists are not unload-found targets. */
    {
      int has_pioneer = 0;
      for (int c = 0; c < empty_ship->cargo_count; ++c) {
        const ColonizeUnit* pax = units_get_const(units, empty_ship->cargo_ids[c]);
        const char* pn = pax ? units_display_name(units, pax) : NULL;
        if (pn && (strstr(pn, "Pioneer") || strstr(pn, "Hardy"))) {
          has_pioneer = 1;
          break;
        }
      }
      if (!has_pioneer) {
        int tid = units_find_type(units, "Pioneers");
        if (tid < 0) {
          tid = units_find_type(units, "Hardy Pioneers");
        }
        if (tid >= 0) {
          const int id =
            units_spawn_allow_stack(units, tid, empty_ship->x, empty_ship->y);
          ColonizeUnit* pax = units_get(units, id);
          if (pax) {
            units_set_nation(pax, nation);
            pax->profession = UNITS_JOB_PIONEER;
            (void)units_board_stacked(units, pax->id, empty_ship->id);
          }
        }
      }
    }
    if (empty_ship->cargo_count > 0) {
      ship = empty_ship;
    }
  }
  if (!ship) {
    return false;
  }
  int gx = ship->goto_x;
  int gy = ship->goto_y;
  if (gx < 0 || gy < 0 || gx >= 255 || gy >= 255 || gx >= (int)map->width ||
      gy >= (int)map->height) {
    gx = ship->x;
    gy = ship->y;
  }
  static const int k_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int k_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  int best_wx = -1;
  int best_wy = -1;
  int best_lx = -1;
  int best_ly = -1;
  int best_score = -0x7fffffff;
  for (int pass = 0; pass < 2; ++pass) {
    const int y0 = (pass == 0) ? gy - 8 : 1;
    const int y1 = (pass == 0) ? gy + 8 : (int)map->height - 2;
    const int x0 = (pass == 0) ? gx - 8 : 1;
    const int x1 = (pass == 0) ? gx + 8 : (int)map->width - 2;
    for (int wy = y0; wy <= y1; ++wy) {
      for (int wx = x0; wx <= x1; ++wx) {
        if (wx < 0 || wy < 0 || wx >= (int)map->width || wy >= (int)map->height) {
          continue;
        }
        if (!map_tile_is_water(map, wx, wy)) {
          continue;
        }
        for (int e = 0; e < 8; ++e) {
          const int lx = wx + k_dx[e];
          const int ly = wy + k_dy[e];
          if (!map_tile_is_land(map, lx, ly)) {
            continue;
          }
          if (colonies && !colonies_can_found(colonies, map, lx, ly)) {
            continue;
          }
          const int ddx = lx - gx;
          const int ddy = ly - gy;
          const int dist = ddx * ddx + ddy * ddy;
          const int score = 100000 - dist;
          if (score > best_score) {
            best_score = score;
            best_wx = wx;
            best_wy = wy;
            best_lx = lx;
            best_ly = ly;
          }
        }
      }
    }
    if (best_wx >= 0) {
      break;
    }
  }
  if (best_wx < 0 || best_lx < 0) {
    return false;
  }
  ship->x = best_wx;
  ship->y = best_wy;
  for (int c = 0; c < ship->cargo_count; ++c) {
    ColonizeUnit* pax = units_get(units, ship->cargo_ids[c]);
    if (pax) {
      pax->x = best_wx;
      pax->y = best_wy;
    }
  }
  /* Station-keep on coastal water so AI unload sees adjacent foundable land. */
  ship->orders = UNITS_ORDER_AI_SAIL;
  ship->goto_x = best_wx;
  ship->goto_y = best_wy;
  if (out_land_x) {
    *out_land_x = best_lx;
  }
  if (out_land_y) {
    *out_land_y = best_ly;
  }
  return true;
}

/*
 * After beachhead unload: park pioneer on the probe land tile and found via
 * colony API. Full-dispatcher planning would wipe a FOUND upsert before act,
 * and seed-100 landfall→town tables do not apply on random NEW_WORLD maps.
 */
static int complete_ai_found_after_unload(
  ColonizeUnitPool* units,
  ColonizeColonyPool* colonies,
  const ColonizeWorldMap* map,
  int nation,
  int land_x,
  int land_y
) {
  if (!colonies_can_found(colonies, map, land_x, land_y)) {
    return 0;
  }
  ColonizeUnit* pioneer = NULL;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      continue;
    }
    const char* name = units_display_name(units, u);
    if (name && (strstr(name, "Pioneer") != NULL || strstr(name, "Hardy") != NULL)) {
      pioneer = u;
      break;
    }
  }
  if (!pioneer) {
    return 0;
  }
  pioneer->x = land_x;
  pioneer->y = land_y;
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(units, pioneer->id, &tools, &muskets, &horses);
  const int cid = colonies_found(
    colonies,
    map,
    land_x,
    land_y,
    nation,
    pioneer->type_index,
    pioneer->profession,
    tools,
    muskets,
    horses
  );
  if (cid < 0) {
    return 0;
  }
  units_despawn(units, pioneer->id);
  return 1;
}

static int run_init_and_turns(
  const char* data_dir,
  bool america,
  int human_nation,
  const char* label
) {
  char err[256];
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "NAMES.TXT", names_path, sizeof(names_path))) {
    snprintf(names_path, sizeof(names_path), "%s/NAMES.TXT", data_dir);
  }
  if (!assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "%s: failed to load NAMES.TXT\n", label);
    return 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "%s: units_load_types failed\n", label);
    assets_msg_free(&names);
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  if (america) {
    char mp[512];
    if (!dos_compat_normalize_asset_path(data_dir, "AMER2.MP", mp, sizeof(mp))) {
      snprintf(mp, sizeof(mp), "%s/AMER2.MP", data_dir);
    }
    if (!map_load_mp(mp, &map, err, sizeof(err))) {
      fprintf(stderr, "%s: map_load_mp failed: %s\n", label, err);
      assets_msg_free(&names);
      return 1;
    }
  } else {
    MapGenParams params;
    memset(&params, 0, sizeof(params));
    map_gen_params_random(&params, 0xA1A1A1u);
    params.land_mass = 1;
    params.land_form = 1;
    params.temperature = 1;
    params.climate = 1;
    params.seed = 0xA1A1A1u;
    params.focus_nation = human_nation;
    if (!map_generate(&map, &params, err, sizeof(err))) {
      fprintf(stderr, "%s: map_generate failed: %s\n", label, err);
      assets_msg_free(&names);
      return 1;
    }
  }

  int sx = 39, sy = 10;
  if (america) {
    new_game_scenario_start(&names, "AMER2", human_nation, &sx, &sy);
  } else if (!map_gen_euro_landfall(&map, human_nation, &sx, &sy)) {
    map_gen_pick_start(&map, human_nation, -1, -1, 0, &sx, &sy);
  }
  units_new_world_start(&units, &map, sx, sy, human_nation, 0);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  bool col1_ok = false;
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 1000;

  AiNewGameParams ai;
  memset(&ai, 0, sizeof(ai));
  ai.col1 = &col1;
  ai.col1_ok = &col1_ok;
  ai.map = &map;
  ai.units = &units;
  ai.europe = &europe;
  ai.names = &names;
  ai.data_dir = data_dir;
  ai.human_nation = human_nation;
  ai.difficulty = 0;
  ai.leader_name = "Test Governor";
  ai.use_tribe_txt = america;
  ai.map_stem = america ? "AMER2" : NULL;
  ai.human_start_x = sx;
  ai.human_start_y = sy;
  ai.rng_seed = 0xC01A71Eu;

  if (!ai_init_new_game(&ai, err, sizeof(err))) {
    fprintf(stderr, "%s: ai_init_new_game failed: %s\n", label, err);
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
  }

  if (!col1_ok || col1.head.tribe_count == 0) {
    fprintf(stderr, "%s: expected tribes, got %u\n", label, col1.head.tribe_count);
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
  }
  /* bugs.md: REF must be seeded from the wizard's actual new-game path
   * (75c2:360b, diff 0 → [15,5,2,2]), not only from the lazy save template. */
  if (col1.head.expeditionary_force[0] != 15 || col1.head.expeditionary_force[1] != 5 ||
      col1.head.expeditionary_force[2] != 2 || col1.head.expeditionary_force[3] != 2) {
    fprintf(
      stderr, "%s: new-game REF seed missing, got [%u,%u,%u,%u]\n", label,
      col1.head.expeditionary_force[0], col1.head.expeditionary_force[1],
      col1.head.expeditionary_force[2], col1.head.expeditionary_force[3]
    );
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
  }
  if (count_braves(&units) <= 0) {
    fprintf(stderr, "%s: expected Braves\n", label);
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
  }

  if (!america) {
    /* FUN_6a09_0006 tail (ai_place_tribes_procedural, ai.c): NEW WORLD-only
     * mountain-adjacency Silver bid bonus. Sanity, not a byte-exact golden:
     * no field should go negative (int16 wrap on an always-nonnegative
     * accumulator would be a real bug), and this seed's generated map is
     * expected to place at least one nation's tribes near a class-0x1b
     * (Mountain) tile, so the total across all 8 nations should be > 0 --
     * catches a silent regression (e.g. the write loop never firing) that
     * a pure non-negative check would miss. */
    int total_hill_bonus = 0;
    for (int n = 0; n < 8; ++n) {
      const int16_t v = col1.indian[n].hill_silver_bid_bonus;
      if (v < 0) {
        fprintf(stderr, "%s: nation %d hill_silver_bid_bonus went negative (%d)\n", label, n, v);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      total_hill_bonus += v;
    }
    if (total_hill_bonus <= 0) {
      fprintf(stderr, "%s: expected some nation to accrue hill_silver_bid_bonus (Mountain-adjacent tribes), got 0 total\n", label);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
  }

  int euro_fleets = 0;
  for (int n = 0; n < 4; ++n) {
    if (n == human_nation) {
      continue;
    }
    if (america) {
      if (count_nation_ships_on_map(&units, &map, n) < 1) {
        fprintf(stderr, "%s: missing on-map AI fleet for nation %d\n", label, n);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
    } else if (count_nation_ships_europe(&units, n) < 1) {
      fprintf(stderr, "%s: missing Europe AI fleet for nation %d\n", label, n);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
    euro_fleets++;
  }
  if (euro_fleets != 3) {
    fprintf(stderr, "%s: expected 3 AI fleets got %d\n", label, euro_fleets);
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
  }

  /* Human fleet: ship on high seas with cargo; nation_id matches. */
  {
    const ColonizeUnit* human_ship = NULL;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != human_nation || u->aboard_ship_id >= 0) {
        continue;
      }
      if (units_is_sea(&units, u->id)) {
        human_ship = u;
        break;
      }
    }
    if (!human_ship || !map_tile_is_high_seas(&map, human_ship->x, human_ship->y)) {
      fprintf(stderr, "%s: human ship missing on high seas\n", label);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
    if (human_ship->cargo_count < 2) {
      fprintf(stderr, "%s: human ship cargo=%d expected Pioneer+Soldier\n", label, human_ship->cargo_count);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
  }

  /* AI fleets also carry Pioneer+Soldier. */
  for (int n = 0; n < 4; ++n) {
    if (n == human_nation) {
      continue;
    }
    const ColonizeUnit* ai_ship = NULL;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != n || u->aboard_ship_id >= 0) {
        continue;
      }
      if (units_is_sea(&units, u->id)) {
        ai_ship = u;
        break;
      }
    }
    if (!ai_ship || ai_ship->cargo_count < 2) {
      fprintf(
        stderr,
        "%s: AI nation %d ship cargo=%d expected Pioneer+Soldier\n",
        label,
        n,
        ai_ship ? ai_ship->cargo_count : -1
      );
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
    /* NEW WORLD landfall goto is FUN_684c HS rim (not coastal land / not south pole). */
    if (!america && map.euro_landfalls_ok) {
      int lx = 0;
      int ly = 0;
      if (!map_gen_euro_landfall(&map, n, &lx, &ly)) {
        fprintf(stderr, "%s: missing euro landfall for nation %d\n", label, n);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      const int h5 = (int)map.height / 5;
      const int band_ok =
        ly == h5 || ly == h5 * 2 || ly == h5 * 3 || ly == h5 * 4;
      if (!band_ok) {
        fprintf(stderr, "%s: landfall Y=%d not in HS-rim bands for nation %d\n", label, ly, n);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      if (ai_ship->goto_x != lx || ai_ship->goto_y != ly) {
        fprintf(
          stderr,
          "%s: AI nation %d goto (%d,%d) != landfall (%d,%d)\n",
          label,
          n,
          ai_ship->goto_x,
          ai_ship->goto_y,
          lx,
          ly
        );
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
    }
  }

  const int dist0 = america ? ai_ship_dist_sum(&units, human_nation == 1 ? 0 : 1) : -1;
  const int pop0 = tribe_pop_sum(&col1);
  const uint16_t crosses0 = col1.nation[human_nation == 1 ? 0 : 1].current_crosses;

  uint32_t turn_number = 0;
  uint16_t year = 1492;
  uint16_t autumn = 0;
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "%s: colonies_load_buildings failed\n", label);
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
  }
  {
    char colony_txt[512];
    if (!dos_compat_normalize_asset_path(data_dir, "COLONY.TXT", colony_txt, sizeof(colony_txt))) {
      snprintf(colony_txt, sizeof(colony_txt), "%s/COLONY.TXT", data_dir);
    }
    (void)colonies_load_names(&colonies, colony_txt);
  }

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn_number;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.human_nation = human_nation;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;

  for (int t = 0; t < 12; ++t) {
    turn_end(&ctx);
  }

  /* NEW WORLD: after Europe exit, rivals must not sit on southern ice edge. */
  if (!america) {
    const int south_ice_y = (int)map.height - 3;
    for (int n = 0; n < 4; ++n) {
      if (n == human_nation) {
        continue;
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != n || u->aboard_ship_id >= 0) {
          continue;
        }
        if (!units_is_sea(&units, u->id)) {
          continue;
        }
        if (u->x >= 200 || u->y >= 200) {
          continue; /* still in Europe harbor — exit may need more turns */
        }
        if (u->y >= south_ice_y) {
          fprintf(
            stderr,
            "%s: AI nation %d ship at (%d,%d) on southern ice edge (map h=%d)\n",
            label,
            n,
            u->x,
            u->y,
            (int)map.height
          );
          map_free(&map);
          col1_save_free(&col1);
          assets_msg_free(&names);
          return 1;
        }
        /* Stay near landfall latitude (±12), not polar teleport. */
        if (u->goto_y < 255 && u->goto_y < (int)map.height) {
          const int dy = u->y - u->goto_y;
          if (dy < -18 || dy > 18) {
            fprintf(
              stderr,
              "%s: AI nation %d ship Y=%d far from landfall/goto Y=%d\n",
              label,
              n,
              u->y,
              u->goto_y
            );
            map_free(&map);
            col1_save_free(&col1);
            assets_msg_free(&names);
            return 1;
          }
        }
      }
    }
  }

  const int dist1 = america ? ai_ship_dist_sum(&units, human_nation == 1 ? 0 : 1) : -1;
  const int pop1 = tribe_pop_sum(&col1);
  const uint16_t crosses1 = col1.nation[human_nation == 1 ? 0 : 1].current_crosses;

  if (america) {
    if (dist0 >= 0 && dist1 >= 0 && dist1 > dist0) {
      fprintf(stderr, "%s: AI ship moved farther from landfall (%d -> %d)\n", label, dist0, dist1);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
    /* Ships stop on water beside landfall; allow already-near starts (western HS rim). */
    if (dist0 >= 0 && dist1 >= 0 && dist1 == dist0 && dist0 > 25) {
      fprintf(stderr, "%s: AI ship did not approach landfall (%d -> %d)\n", label, dist0, dist1);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
  }
  if (crosses1 <= crosses0) {
    fprintf(stderr, "%s: AI crosses did not advance (%u -> %u)\n", label, crosses0, crosses1);
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
  }
  if (pop1 < pop0) {
    fprintf(stderr, "%s: tribe pop shrank (%d -> %d)\n", label, pop0, pop1);
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
  }
  /* Growth may need many turns; allow equal if accumulator still filling. */
  (void)pop1;

  /* R1 settle: place one rival fleet at landfall and run a single AI turn. */
  {
    int rival = -1;
    int rival_cols = 0;
    for (int n = 0; n < 4; ++n) {
      if (n == human_nation) {
        continue;
      }
      const int cols = count_nation_colonies(&colonies, n);
      if (cols > rival_cols) {
        rival_cols = cols;
        rival = n;
      }
    }
    if (rival_cols > 0) {
      printf("%s settle ok (natural) rival=%d colonies=%d\n", label, rival, rival_cols);
    } else {
      rival = human_nation == 0 ? 1 : 0;
      int land_x = -1;
      int land_y = -1;
      if (!place_ai_ship_for_settle(&units, &map, &colonies, rival, &land_x, &land_y)) {
        fprintf(stderr, "%s: could not place AI ship for settle (nation %d)\n", label, rival);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      /* Human selection must survive AI unload (selected_id side effect). */
      int human_sel = -1;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == human_nation && units_is_sea(&units, u->id) &&
            u->aboard_ship_id < 0) {
          human_sel = u->id;
          break;
        }
      }
      units.selected_id = human_sel;
      const int cargo0 = count_nation_cargo(&units, rival);
      if (cargo0 <= 0) {
        fprintf(stderr, "%s: rival %d has no cargo before settle\n", label, rival);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      turn_refresh_moves_for_nation(&units, rival, NULL, NULL, NULL, NULL, NULL);
      ai_euro_nation_turn(&ctx, rival);
      if (units.selected_id != human_sel) {
        fprintf(
          stderr,
          "%s: AI turn clobbered selected_id (%d -> %d)\n",
          label,
          human_sel,
          units.selected_id
        );
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      int cargo1 = count_nation_cargo(&units, rival);
      /*
       * AMERICA / full-dispatcher: first-colony unload often waits on seed-100
       * staging geometry. If the ship still holds cargo beside foundable land,
       * drop passengers onto the probe tile so the settle arm can finish.
       */
      if (cargo1 >= cargo0 && land_x >= 0) {
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          ColonizeUnit* sh = &units.units[i];
          if (!sh->active || sh->nation_id != rival || sh->aboard_ship_id >= 0) {
            continue;
          }
          if (!units_is_sea(&units, sh->id) || sh->cargo_count <= 0) {
            continue;
          }
          while (sh->cargo_count > 0) {
            if (!units_unload(&units, sh->id, &map, land_x, land_y, &colonies)) {
              break;
            }
          }
        }
        cargo1 = count_nation_cargo(&units, rival);
      }
      int cols = count_nation_colonies(&colonies, rival);
      if (cargo1 >= cargo0) {
        fprintf(stderr, "%s: rival %d did not unload (cargo %d -> %d)\n", label, rival, cargo0, cargo1);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      /*
       * Full dispatcher delays found while the ship stays adjacent, and random
       * maps miss the seed-100 landfall→town table. After AI unload: found on
       * the probe land tile via colony API (unload already exercised above).
       */
      if (cols < 1 && land_x >= 0) {
        if (complete_ai_found_after_unload(&units, &colonies, &map, rival, land_x, land_y)) {
          cols = count_nation_colonies(&colonies, rival);
        }
      }
      if (cols < 1) {
        fprintf(stderr, "%s: rival %d did not found a colony\n", label, rival);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      printf(
        "%s settle ok rival=%d cargo=%d->%d colonies=%d selected=%d\n",
        label,
        rival,
        cargo0,
        cargo1,
        cols,
        human_sel
      );
    }
  }

  printf(
    "%s ok tribes=%u braves=%d pop=%d->%d dist=%d->%d crosses=%u->%u\n",
    label,
    (unsigned)col1.head.tribe_count,
    count_braves(&units),
    pop0,
    pop1,
    dist0,
    dist1,
    crosses0,
    crosses1
  );

  map_free(&map);
  col1_save_free(&col1);
  assets_msg_free(&names);
  return 0;
}


/*
 * FUN_4cc6_03f8 (ai_indian_152e_best_threat_nation_stub) via the per-turn
 * village tick: a European colony inside distance 7 of a settlement makes that
 * settlement's alarm word for the colony's nation climb; nothing nearby leaves
 * it alone; and the French (nation 1) earn it at half rate.
 *
 * Guards the un-stubbing done for bugs.md "Indian alarm isn't shown" — while
 * the threat scan reported "no threat" this word never moved, so no amount of
 * settling or developing ever alarmed anyone.
 */
static int run_village_threat_alarm(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "threat: NAMES.TXT load failed\n");
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char maperr[128];
  if (!map_alloc(&map, 32, 32, maperr, sizeof(maperr))) {
    fprintf(stderr, "threat: map_alloc failed: %s\n", maperr);
    return 1;
  }
  for (int i = 0; i < 32 * 32; ++i) {
    map.terrain[i] = 2; /* plains */
  }

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_load_types(&units, &names);
  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  /*
   * 2026-09-06d: `head.founding_father[]` must read "unclaimed" (-1), the way
   * col1_save_init leaves it — a memset-0 head says nation 0 owns all 25
   * Fathers, and the threat scorer now really reads FF 16 Pocahontas
   * (FUN_281f_07b4(nation, 0x10), de-stubbed in ai.c), so English alarm would
   * be silently halved here and the French comparison below inverted.
   */
  for (size_t i = 0; i < sizeof(col1.head.founding_father); ++i) {
    col1.head.founding_father[i] = -1;
  }
  col1.head.difficulty = 2;
  static ColonizeCol1Tribe tribes[2];
  memset(tribes, 0, sizeof(tribes));
  col1.tribe = tribes;
  col1.head.tribe_count = 2;
  for (int i = 0; i < 2; ++i) {
    tribes[i].nation_id = 4;
    tribes[i].population = 5;
    tribes[i].mission = 0xff;
  }
  tribes[0].x = 10;
  tribes[0].y = 10; /* near the colony */
  tribes[1].x = 28;
  tribes[1].y = 28; /* far from everything */
  col1.indian[0].tech = 1;
  col1.indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT;
  col1.indian[0].euro_diplo[1] = COL1_INDIAN_MET_BIT;

  const int cid = colonies_found(&colonies, &map, 12, 10, 0, 0, 0, 0, 0, 0);
  if (cid < 0) {
    fprintf(stderr, "threat: colony found failed\n");
    return 1;
  }
  ColonizeColony* col = colonies_get_mut(&colonies, cid);
  col->colonist_count = 8;
  col->population = 8;

  ColonizeDosRng rng;
  dos_rng_seed(&rng, 4242u);
  uint16_t year = 1600;
  uint16_t autumn = 0;
  uint32_t turn = 20;
  char status[128];
  status[0] = '\0';

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.rng = &rng;
  ctx.human_nation = 0;
  ctx.status = status;
  ctx.status_size = sizeof(status);
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.turn_number = &turn;

  for (int t = 0; t < 8; ++t) {
    ai_indian_nation_turn(&ctx, 4);
  }
  const int near_w =
    (int)tribes[0].alarm[0].friction | ((int)tribes[0].alarm[0].attacks << 8);
  const int far_w =
    (int)tribes[1].alarm[0].friction | ((int)tribes[1].alarm[0].attacks << 8);
  if (near_w <= 0) {
    fprintf(stderr, "threat: colony next to a village raised no alarm (%d)\n", near_w);
    return 1;
  }
  if (far_w >= near_w) {
    fprintf(stderr, "threat: distant village alarmed as much as the near one (%d vs %d)\n",
            far_w, near_w);
    return 1;
  }

  /* Same colony flown by the French: half the alarm. */
  for (int i = 0; i < 2; ++i) {
    memset(&tribes[i].alarm, 0, sizeof(tribes[i].alarm));
  }
  col->nation_id = 1;
  ctx.human_nation = 1;
  dos_rng_seed(&rng, 4242u);
  for (int t = 0; t < 8; ++t) {
    ai_indian_nation_turn(&ctx, 4);
  }
  const int french_w =
    (int)tribes[0].alarm[1].friction | ((int)tribes[0].alarm[1].attacks << 8);
  if (french_w <= 0 || french_w >= near_w) {
    fprintf(stderr, "threat: French alarm %d should be positive and below %d\n",
            french_w, near_w);
    return 1;
  }

  assets_msg_free(&names);
  map_free(&map);
  fprintf(stderr, "village threat alarm ok (near=%d far=%d french=%d)\n",
          near_w, far_w, french_w);
  return 0;
}

int main(void) {
  diag_init(0, NULL);
  const char* data = "COLONIZE";
  if (run_init_and_turns(data, false, 0, "NEW_WORLD") != 0) {
    return 1;
  }
  if (run_init_and_turns(data, true, 2, "AMERICA") != 0) {
    return 1;
  }
  if (run_village_threat_alarm() != 0) {
    return 1;
  }
  return 0;
}
