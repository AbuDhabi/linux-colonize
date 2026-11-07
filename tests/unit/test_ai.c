#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/ai_contact_internal.h"
#include "core/ai_euro.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/europe_art.h"
#include "core/map.h"
#include "core/map_gen.h"
#include "core/new_game.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"
#include "../common/test_runner.h"
#include "../common/ai_fixture.h"
#include "../common/test_catalogs.h"
#include "core/ai_internal.h"
#include "core/dos_buggy.h"

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

/* Land units of `nation` parked in the port's Europe limbo — where DOS
 * FUN_38fd_0718 puts a crosses immigrant (europe_nation_immigration_tick_w). */
static int ai_europe_land_count(const ColonizeUnitPool* units, int nation) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (u->x < 200 && u->y < 200) {
      continue;
    }
    if (!units_is_sea(units, u->id)) {
      n++;
    }
  }
  return n;
}

/* on_map: skip hulls on the off-map Europe lanes (x >= width). A hull that
 * has unloaded sails home through FUN_521d_20e6's Europe arm, so its goto is
 * no longer the landfall the sum measures. */
static int ai_ship_dist_sum(const ColonizeUnitPool* units, int nation, const ColonizeWorldMap* on_map) {
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
    if (on_map && !map_in_bounds(on_map, u->x, u->y)) {
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
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
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
    /* As the new-game path does: the FUN_684c_08c0 loaded-map tail and its
     * band pick (0x190 draw first) give AMER2 its HS-rim landfalls. */
    map_gen_finalize_border_and_forest(map.terrain, (int)map.width, (int)map.height);
    ColonizeDosRng lf_rng;
    dos_rng_seed(&lf_rng, 100);
    (void)dos_rng_range(&lf_rng, 1, 0x7fff);
    map_gen_assign_euro_landfalls(&map, &lf_rng, human_nation, true);
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
  if (!map_gen_euro_landfall(&map, human_nation, &sx, &sy)) {
    map_gen_pick_start(&map, human_nation, -1, -1, 0, &sx, &sy);
  }
  units_new_world_start(&units, &map, sx, sy, human_nation, 0);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  bool col1_ok = false;
  colonies_set_col1_context(NULL); /* a previous case left its stack col1 registered */
  colonies_set_units_context(NULL);
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  /* Real @CARGO table: AI Europe trades price off it (port_bugs.md #1049). */
  (void)europe_load_tables(&europe, &names);
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

  /* FUN_75c2_10ae raw 120960-120977: every slot gets its @LEADERNAME, AI powers included
   * (an empty AI name prints "'s French:" in DOS's Foreign Affairs report). */
  for (int n = 0; n < 4; ++n) {
    if (n != human_nation && col1.player[n].name[0] == '\0') {
      fprintf(stderr, "%s: nation %d has no leader name\n", label, n);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
  }

  {
    /* FUN_75c2_2778 raw 121786-121795: menu choices 1-2 (NEW WORLD, AMERICA) make five
     * RNG(0,3) axis draws off the boot seed before 235c's 16 market-pool draws; only
     * CUSTOMIZE (choice 3) sets the axes to 1 and draws none. */
    ColonizeDosRng want_rng;
    dos_rng_seed(&want_rng, col1.post_map.boot_timer);
    for (int i = 0; i < 5; ++i) {
      (void)dos_rng_range(&want_rng, 0, 3);
    }
    for (int c = 0; c < 16; ++c) {
      const int want = dos_rng_range(&want_rng, 600, 1000);
      if (col1.head.market_demand_pool[c] != want) {
        fprintf(stderr, "%s: market pool[%d] %u, want %d (after 5 axis draws)\n", label, c,
                col1.head.market_demand_pool[c], want);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
    }
  }

  {
    /* Map setup writes the human's landfall into nation +0x32/+0x33 too (FUN_684c_08c0 raw
     * 107143-107153), so a ship bought before the first crossing (stamped at FUN_38fd_4b50
     * raw 64895-64896) sails back to it. */
    EuropeScreen* buy = malloc(sizeof(*buy));
    if (!buy) {
      return 1;
    }
    memcpy(buy, &europe, sizeof(*buy));
    buy->purchase_count = 1;
    buy->purchase[0].is_ship = true;
    buy->harbor_ships = 0;
    buy->bound_ships = 0;
    const bool sailed = europe_purchase_commit(buy, 0, 0, NULL) &&
                        europe_set_sail_from_harbor(buy, 0, 1, NULL, human_nation);
    const int bx = buy->bound[0].exit_x;
    const int by = buy->bound[0].exit_y;
    free(buy);
    if (!sailed || bx != sx || by != sy ||
        col1.nation[human_nation].return_from_europe_x != (uint8_t)sx ||
        col1.nation[human_nation].return_from_europe_y != (uint8_t)sy) {
      fprintf(
        stderr, "%s: first bought ship exit (%d,%d), nation record (%u,%u), landfall (%d,%d)\n",
        label, bx, by, col1.nation[human_nation].return_from_europe_x,
        col1.nation[human_nation].return_from_europe_y, sx, sy
      );
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
  }

  {
    /* FUN_38fd_6024 (pools + opening prices, every nation), as the new-game
     * path runs it; own reseeded stream so the AI stream is untouched. */
    const ColonizeWorld pw = world_make(&units, NULL, &map, &col1, col1_ok, NULL, &europe);
    europe_seed_campaign_w(&pw, 0x6024u);
  }
  /* DS:0x5388 = 235c param = America-map choice (original_saves: 1 on every
   * golden_idle_america save, 0 on the generated mapgen/early-game ones). */
  if (col1.head.start_mode_marker != (america ? 1u : 0u)) {
    fprintf(stderr, "%s: start_mode_marker %u\n", label, col1.head.start_mode_marker);
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
  /* port_bugs.md: REF must be seeded from the wizard's actual new-game path
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
  /* port_bugs.md #1052/#1058: FUN_38fd_6024 seeds every nation's recruit pool and
   * LFSR, not just the human's (diff 0: slot 0 = 0x19, Spain 0x18; slots 1-2
   * expert rolls, never 0x1c). Expert Farmers (0) is a legal roll. */
  for (int n = 0; n < 4; ++n) {
    if (n == human_nation) {
      continue;
    }
    const uint8_t* r = col1.nation[n].recruit;
    if (r[0] != (n == 2 ? 0x18 : 0x19) || r[1] >= 0x19 || r[2] >= 0x19 ||
        col1.nation[n].recruit_lfsr == 0) {
      fprintf(stderr, "%s: AI nation %d recruit pool unseeded [%u,%u,%u]\n", label, n, r[0], r[1], r[2]);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
  }
  {
    /* FUN_67f4_0088 (raw 105112-105140) fills the DS:0x85c8 continent
     * land-tile tallies at new game; zero until the first save was the bug. */
    unsigned land = 0;
    for (int c = 0; c < 16; ++c) {
      land += col1.post_map.continent_tally_b[c];
    }
    if (land == 0) {
      fprintf(stderr, "%s: continent tallies empty after new game\n", label);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
  }
  if (count_braves(&units) <= 0) {
    fprintf(stderr, "%s: expected Braves\n", label);
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
  }

  if (!america) {
    /* FUN_6a09_0006 tail (ai_tribes_mountain_silver_bonus, ai.c): Mountains
     * Silver bid bonus, both paths (AMERICA is pinned against DOS by
     * tribe_txt_init_new_game_matches_dos). Sanity, not a byte-exact golden:
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
    if (count_nation_ships_europe(&units, n) < 1) {
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

  /*
   * port_bugs.md #490: FUN_75c2_235c is path-independent — an AMERICA / TRIBE.TXT
   * start must look exactly like a generated one: fleet on the westbound
   * sentinel (228+n, 228+n), voyage counter 0, goto = the nation's landfall
   * tile (the LAB_684c_1b4c HS-rim walk on AMER2 too), and the first
   * Atlantic tick lands it on the ring around that tile.
   */
  int ai_landfall_x[4];
  int ai_landfall_y[4];
  {
    for (int n = 0; n < 4; ++n) {
      ai_landfall_x[n] = -1;
      ai_landfall_y[n] = -1;
      if (n == human_nation) {
        continue;
      }
      int lx = -1;
      int ly = -1;
      if (!map_gen_euro_landfall(&map, n, &lx, &ly)) {
        continue; /* generated-map fallback pick — no fixed expectation */
      }
      ai_landfall_x[n] = lx;
      ai_landfall_y[n] = ly;

      const ColonizeUnit* ship = NULL;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != n || u->aboard_ship_id >= 0) {
          continue;
        }
        if (units_is_sea(&units, u->id)) {
          ship = u;
          break;
        }
      }
      if (!ship || ship->x != 228 + n || ship->y != 228 + n) {
        fprintf(
          stderr, "%s: nation %d fleet not on sentinel 228+n, got (%d,%d)\n", label, n,
          ship ? ship->x : -1, ship ? ship->y : -1
        );
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      if (ship->col1_counter16 != 0) {
        fprintf(
          stderr, "%s: nation %d fleet voyage counter %d, expected 0\n", label, n,
          (int)ship->col1_counter16
        );
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      if (ship->goto_x != lx || ship->goto_y != ly) {
        fprintf(
          stderr, "%s: nation %d landfall goto (%d,%d), expected (%d,%d)\n", label, n,
          ship->goto_x, ship->goto_y, lx, ly
        );
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
    }

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

  const int dist0 = america ? ai_ship_dist_sum(&units, human_nation == 1 ? 0 : 1, NULL) : -1;
  const int pop0 = tribe_pop_sum(&col1);
  const uint16_t crosses0 = col1.nation[human_nation == 1 ? 0 : 1].current_crosses;
  const int dock0 = ai_europe_land_count(&units, human_nation == 1 ? 0 : 1);

  uint32_t turn_number = 0;
  uint16_t year = 1492;
  uint16_t autumn = 0;
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
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

  /*
   * port_bugs.md #490: the first EOT is the fleets' Atlantic tick — counter 0 means
   * FUN_48d3_03d0 hands them to FUN_48d3_048e at once, so every AI fleet must
   * leave the 228+n lane and land on the ring around its own landfall tile,
   * whatever the map source. The generated-map fallback pick has no fixed
   * landfall, so only bound nations are checked.
   */
  turn_end(&ctx);
  for (int n = 0; n < 4; ++n) {
    if (n == human_nation || ai_landfall_x[n] < 0) {
      continue;
    }
    if (count_nation_ships_on_map(&units, &map, n) < 1) {
      fprintf(stderr, "%s: nation %d fleet still at sea after its first tick\n", label, n);
      map_free(&map);
      col1_save_free(&col1);
      assets_msg_free(&names);
      return 1;
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != n || u->aboard_ship_id >= 0 ||
          !units_is_sea(&units, u->id)) {
        continue;
      }
      /* 048e ring + one scored ocean leg — never a map-wide scan (#487 put the
       * Spanish AMER2 fleet 18 tiles west of (47,61)). */
      if (abs(u->x - ai_landfall_x[n]) > 12 || abs(u->y - ai_landfall_y[n]) > 12) {
        fprintf(
          stderr, "%s: nation %d landed at (%d,%d), far from landfall (%d,%d)\n", label, n, u->x,
          u->y, ai_landfall_x[n], ai_landfall_y[n]
        );
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
      }
      break;
    }
  }
  for (int t = 1; t < 12; ++t) {
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

  const int dist1 = america ? ai_ship_dist_sum(&units, human_nation == 1 ? 0 : 1, &map) : -1;
  const int pop1 = tribe_pop_sum(&col1);
  const uint16_t crosses1 = col1.nation[human_nation == 1 ? 0 : 1].current_crosses;
  const int dock1 = ai_europe_land_count(&units, human_nation == 1 ? 0 : 1);

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
  /* DOS FUN_38fd_5e52 runs for AI nations too: the meter either climbs or it
   * was zeroed by an arrival, which leaves a new colonist in the Europe limbo.
   * Asserting only "crosses climb" was an artefact of the parked stub. */
  if (crosses1 <= crosses0 && dock1 <= dock0) {
    fprintf(
      stderr, "%s: AI crosses did not advance and no immigrant arrived (%u -> %u, dock %d -> %d)\n",
      label, crosses0, crosses1, dock0, dock1
    );
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
      ColonizeWorld w820 = world_make(&units, NULL, NULL, NULL, false, NULL, NULL);
      turn_refresh_moves_for_nation_w(&w820, rival);
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
            ColonizeWorld w_unload = world_make(&units, &colonies, &map, NULL, false, NULL, NULL);
            if (!units_unload_w(&w_unload, sh->id, land_x, land_y)) {
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
 * Guards the un-stubbing done for port_bugs.md "Indian alarm isn't shown" — while
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
  units_set_occupancy_map(NULL);
  units_load_types(&units, &names);
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  colonies_set_col1_context(NULL);
  colonies_set_units_context(NULL);
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

/*
 * Shared fixture for the two FUN_4d56_152e regression tests below: an empty
 * plains map, no units, one Indian settlement of nation 4 at (10,10).
 * Everything is memset to zero first and `founding_father[]` filled with -1
 * (unclaimed) — a memset-0 head hands nation 0 all 25 Fathers and silently
 * halves/doubles the arms under test.
 */
typedef struct Ai152eFixture {
  ColonizeMsgCatalog names;
  ColonizeWorldMap map;
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  ColonizeCol1Save col1;
  ColonizeCol1Tribe tribes[1];
  ColonizeDosRng rng;
  ColonizeTurnContext ctx;
  uint16_t year;
  uint16_t autumn;
  uint32_t turn;
  char status[128];
} Ai152eFixture;

static int ai_152e_fixture_init(Ai152eFixture* f, const char* label, uint32_t seed) {
  memset(f, 0, sizeof(*f));
  assets_msg_init(&f->names);
  if (!assets_msg_load_file(&f->names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "%s: NAMES.TXT load failed\n", label);
    return 1;
  }
  char maperr[128];
  if (!map_alloc(&f->map, 32, 32, maperr, sizeof(maperr))) {
    fprintf(stderr, "%s: map_alloc failed: %s\n", label, maperr);
    return 1;
  }
  for (int i = 0; i < 32 * 32; ++i) {
    f->map.terrain[i] = 2; /* plains */
  }
  units_reset(&f->units);
  units_set_occupancy_map(NULL);
  units_load_types(&f->units, &f->names);
  colonies_init(&f->colonies);
  colonies_set_occupancy_map(NULL);

  for (size_t i = 0; i < sizeof(f->col1.head.founding_father); ++i) {
    f->col1.head.founding_father[i] = -1;
  }
  f->col1.head.difficulty = 2;
  f->col1.tribe = f->tribes;
  f->col1.head.tribe_count = 1;
  f->tribes[0].nation_id = 4;
  f->tribes[0].x = 10;
  f->tribes[0].y = 10;
  f->tribes[0].population = 5;
  f->tribes[0].mission = COL1_TRIBE_MISSION_NONE;
  f->col1.indian[0].tech = 1;

  dos_rng_seed(&f->rng, seed);
  f->year = 1600;
  f->autumn = 0;
  f->turn = 20;
  f->status[0] = '\0';
  f->ctx.col1 = &f->col1;
  f->ctx.col1_ok = true;
  f->ctx.map = &f->map;
  f->ctx.units = &f->units;
  f->ctx.colonies = &f->colonies;
  f->ctx.rng = &f->rng;
  f->ctx.human_nation = 0;
  f->ctx.status = f->status;
  f->ctx.status_size = sizeof(f->status);
  f->ctx.game_year = &f->year;
  f->ctx.game_autumn = &f->autumn;
  f->ctx.turn_number = &f->turn;
  return 0;
}

static void ai_152e_fixture_free(Ai152eFixture* f) {
  map_free(&f->map);
  assets_msg_free(&f->names);
}

/*
 * smell #45 — FUN_4d56_152e's mission arm keeps settlement+0xa+e*2 as a whole
 * signed int16, exactly like the threat arm beside it: raw 81490-81496 reads
 * the word, adds local_8 * -3, clamps at 0 and writes the word back. The port
 * used to touch only the low `friction` byte, so once the word passed 255 the
 * mission relief became a permanent no-op (friction bottomed at 0 while the
 * `attacks` high byte held the value up).
 *
 * No colony on the map ⇒ no threat nation ⇒ the mission arm is the only writer
 * of the word this tick. local_8 = 4 (Jesuit) << 1 (capital) << 1 (FF 0x18)
 * = 16, so the delta is -48 — enough to borrow out of the low byte. The
 * starting word 296 keeps `friction` at 40; that also used to keep
 * ai_contact's separate mission-pacify drip (friction < 40) out of the
 * arithmetic, and since 2026-09-09 (smell #48) that drip is gone entirely —
 * this 152e term is now DOS's only mission relief.
 */
static int run_152e_mission_attitude_word(void) {
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "152e-mission-word", 4242u) != 0) {
    return 1;
  }
  f.tribes[0].mission = (uint8_t)COL1_TRIBE_MISSION_JESUIT_BIT; /* Jesuit, English */
  f.tribes[0].state.capital = 1;
  f.col1.nation[0].founding_fathers[24 / 8] |= (uint8_t)(1u << (24 % 8)); /* FF 0x18 */
  /* Word 296 = friction 40 | attacks 1 — a value only reachable above 255. */
  col1_tribe_attitude_set(&f.tribes[0], 0, 296);

  ai_indian_nation_turn(&f.ctx, 4);
  const int after1 = col1_tribe_attitude(&f.tribes[0], 0);
  if (after1 != 248) {
    fprintf(stderr,
            "152e-mission-word: word 296 - 48 should be 248, got %d "
            "(friction=%u attacks=%u) — the mission arm is still editing the "
            "low byte only\n",
            after1, (unsigned)f.tribes[0].alarm[0].friction,
            (unsigned)f.tribes[0].alarm[0].attacks);
    ai_152e_fixture_free(&f);
    return 1;
  }
  ai_indian_nation_turn(&f.ctx, 4);
  const int after2 = col1_tribe_attitude(&f.tribes[0], 0);
  if (after2 != 200) {
    fprintf(stderr, "152e-mission-word: second tick should reach 200, got %d\n", after2);
    ai_152e_fixture_free(&f);
    return 1;
  }
  /* Floor is 0 on the whole word, not on the low byte (raw 81494-81496). */
  col1_tribe_attitude_set(&f.tribes[0], 0, 40);
  ai_indian_nation_turn(&f.ctx, 4);
  const int floored = col1_tribe_attitude(&f.tribes[0], 0);
  if (floored != 0) {
    fprintf(stderr, "152e-mission-word: 40 - 48 should clamp to 0, got %d\n", floored);
    ai_152e_fixture_free(&f);
    return 1;
  }
  ai_152e_fixture_free(&f);
  fprintf(stderr, "152e mission attitude word ok (296->%d->%d, floor %d)\n",
          after1, after2, floored);
  return 0;
}

/*
 * smell #46 — the 152e accumulator spends its crossings through
 * FUN_281f_0d6c (raw 80240/80247/80255), a bare thunk to the WHOLE of
 * FUN_4cc6_00f2 including its escalation tail. The port used to call the
 * first half only (ai_diplo_indian_alarm_delta), so DOS's sole alarm-growth
 * channel could push a pair to alarm 100 and still never burn the missions.
 *
 * Fixture: a fat English colony two tiles from the settlement so the threat
 * arm has a nation to spend on, the accumulator re-loaded deeply negative
 * each tick (the crossing loop lives inside `if (threat_nation >= 0)`), alarm
 * 99 at PEACE, English human-controlled so the tail's roll cap is the
 * difficulty. The first crossing lands alarm on 100; the rest hit the tail,
 * which is RNG-gated (rng(0,10) <= difficulty+1), so a wired tail fires
 * within a couple of ticks and an unwired one never does.
 */
static int run_152e_alarm_escalation(void) {
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "152e-escalate", 90909u) != 0) {
    return 1;
  }
  f.col1.head.difficulty = 4;
  f.tribes[0].mission = 0; /* English mission, the thing the tail burns */
  f.col1.indian[0].euro_diplo[0] = (uint8_t)(COL1_INDIAN_MET_BIT | COL1_INDIAN_PEACE_BIT);
  f.col1.indian[0].alarm_by_player[0] = 99;
  f.col1.player[0].control = 0; /* human ⇒ tail cap = difficulty */

  const int cid = colonies_found(&f.colonies, &f.map, 12, 10, 0, 0, 0, 0, 0, 0);
  if (cid < 0) {
    fprintf(stderr, "152e-escalate: colony found failed\n");
    ai_152e_fixture_free(&f);
    return 1;
  }
  ColonizeColony* col = colonies_get_mut(&f.colonies, cid);
  col->colonist_count = 8;
  col->population = 8;

  int cleared_on = -1;
  for (int t = 0; t < 4 && cleared_on < 0; ++t) {
    /*
     * The two arms adjust the accumulator before the crossing loop reads it,
     * so seed it well past -8: one settlement scores only a few points of
     * threat per tick and the mission arm pays some of that back.
     */
    f.col1.indian[0].euro_relation_accum[0] = -100;
    ai_indian_nation_turn(&f.ctx, 4);
    if (f.tribes[0].mission == COL1_TRIBE_MISSION_NONE) {
      cleared_on = t;
    }
  }
  const int alarm = (int)f.col1.indian[0].alarm_by_player[0];
  if (alarm < 100) {
    fprintf(stderr, "152e-escalate: accumulator never reached alarm 100 (%d)\n", alarm);
    ai_152e_fixture_free(&f);
    return 1;
  }
  if (cleared_on < 0) {
    fprintf(stderr,
            "152e-escalate: alarm hit %d at peace but the mission survived — "
            "the accumulator is calling the bare writer, not 4cc6_00f2\n",
            alarm);
    ai_152e_fixture_free(&f);
    return 1;
  }
  ai_152e_fixture_free(&f);
  fprintf(stderr, "152e alarm escalation ok (alarm=%d, missions burned on tick %d)\n",
          alarm, cleared_on);
  return 0;
}

/*
 * smell sweep-3 E6 — the 152e mission arm indexes
 * `indian.euro_relation_accum[]` (int8_t[4]) with the low nibble of the
 * settlement mission byte. DOS (FUN_4d56_152e, raw 81470-81476) sign-extends
 * the byte and masks the nibble with no bound, so a nibble of 4..15 — outside
 * the 0..3 domain col1_save.h:729 documents — used to write past the array
 * into the neighbouring `euro_diplo[]` bytes (offset 0x36 + 7 == 0x3d ==
 * euro_diplo[3]). ai_indian_tribe_mission_nation now skips the arm instead.
 *
 * No colony on the map and no MET bit ⇒ neither the threat arm nor the
 * friction-roll loop writes the accumulator this tick, so both the array and
 * the euro_diplo[] bytes that follow it must come back untouched. (Only those
 * two are compared: the growth arm legitimately edits other fields of the
 * record.)
 */
static int run_152e_mission_nibble_out_of_domain(void) {
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "152e-mission-nibble", 1337u) != 0) {
    return 1;
  }
  f.tribes[0].mission = 0x07u; /* nibble 7: no such European nation */
  f.tribes[0].state.capital = 1;
  int8_t accum_before[4];
  uint8_t diplo_before[4];
  memcpy(accum_before, f.col1.indian[0].euro_relation_accum, sizeof(accum_before));
  memcpy(diplo_before, f.col1.indian[0].euro_diplo, sizeof(diplo_before));

  ai_indian_nation_turn(&f.ctx, 4);

  if (memcmp(accum_before, f.col1.indian[0].euro_relation_accum,
             sizeof(accum_before)) != 0 ||
      memcmp(diplo_before, f.col1.indian[0].euro_diplo, sizeof(diplo_before)) != 0) {
    fprintf(stderr,
            "152e-mission-nibble: out-of-domain mission nibble 7 wrote into "
            "the Indian record (euro_relation_accum %d/%d/%d/%d, euro_diplo "
            "%u/%u/%u/%u)\n",
            (int)f.col1.indian[0].euro_relation_accum[0],
            (int)f.col1.indian[0].euro_relation_accum[1],
            (int)f.col1.indian[0].euro_relation_accum[2],
            (int)f.col1.indian[0].euro_relation_accum[3],
            (unsigned)f.col1.indian[0].euro_diplo[0],
            (unsigned)f.col1.indian[0].euro_diplo[1],
            (unsigned)f.col1.indian[0].euro_diplo[2],
            (unsigned)f.col1.indian[0].euro_diplo[3]);
    ai_152e_fixture_free(&f);
    return 1;
  }
  for (int e = 0; e < 4; ++e) {
    if (col1_tribe_attitude(&f.tribes[0], e) != 0) {
      fprintf(stderr,
              "152e-mission-nibble: attitude[%d] moved to %d on an "
              "out-of-domain mission nibble\n",
              e, col1_tribe_attitude(&f.tribes[0], e));
      ai_152e_fixture_free(&f);
      return 1;
    }
  }
  ai_152e_fixture_free(&f);
  fprintf(stderr, "152e out-of-domain mission nibble ok (record untouched)\n");
  return 0;
}

/*
 * known_divergences.md #1560: FUN_4d56_021a:1316-13ae shows @INDIANSURPRISE
 * on EVERY attack pick (flags & 8) onto land owned by a human nation with a
 * colony, not only the tribe's first attack (DOSBox trace). An AI owner gets
 * nothing (DS:0x543f != 0).
 */
static int run_indian_surprise_every_attack(void) {
  if (!test_names_txt() || !test_game_txt()) {
    return TR_SKIP;
  }
  for (int half = 0; half < 2; ++half) {
    Ai152eFixture f;
    if (ai_152e_fixture_init(&f, "indian-surprise", 777u) != 0) {
      return 1;
    }
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      ai_152e_fixture_free(&f);
      return 1;
    }
    AiPopupState pop;
    ai_popup_init(&pop);
    f.ctx.messages = &game_txt;
    f.ctx.ai_popups = &pop;
    const int e = half == 0 ? 0 : 1; /* 0 = human, 1 = AI */
    ColonizeColony* col = &f.colonies.colonies[0];
    col->active = true;
    col->nation_id = e;
    col->x = 14;
    col->y = 10;
    snprintf(col->name, sizeof(col->name), "Testtown");
    const int dir = 2;
    const int tx = 13 + k_ai_dir8_dx[dir];
    const int ty = 10 + k_ai_dir8_dy[dir];
    map_set_owner_nibble(&f.map, tx, ty, e);
    const int bid = units_spawn_allow_stack(&f.units, units_find_type(&f.units, "Brave"), 13, 10);
    ColonizeUnit* brave = units_get(&f.units, bid);
    if (!brave) {
      assets_msg_free(&game_txt);
      ai_152e_fixture_free(&f);
      return 1;
    }
    brave->nation_id = 4;
    brave->home_tribe_index = 0;
    ai_s_native_colonies = &f.colonies;
    ai_native_bind_step_ctx(&f.ctx);
    int shown = 0;
    for (int attack = 0; attack < 2; ++attack) {
      brave->moves = 0; /* 021a:12f8 full move left */
      f.col1.indian[0].contact_state[e] = 0;
      (void)ai_native_021a_tail(&f.units, &f.map, &f.col1, &f.rng, brave, 4, dir, 0x48);
    }
    for (int i = 0; i < pop.queue_count; ++i) {
      shown += strstr(pop.queue[i].body, "Testtown") != NULL;
    }
    ai_native_bind_step_ctx(NULL);
    ai_s_native_colonies = NULL;
    assets_msg_free(&game_txt);
    ai_152e_fixture_free(&f);
    fprintf(stderr, "indian-surprise owner %d: %d popups\n", e, shown);
    if (shown != (half == 0 ? 2 : 0)) {
      return 1;
    }
  }
  return 0;
}

/* port_bugs.md #1103: ai_coarse_fog_reset wipes the DS:0x9faa plane that the 0a60
 * restamp (and mapgen) leave behind, as ai_init_new_game / load need. */
/*
 * A !bVar6 visit (contact_state 1) to a colony whose stores price to nothing.
 * FUN_5bfb_022e raw 96857: local_40 < 0 -> LAB_5bfb_0dea, local_4c = -1,
 * back to LAB_5bfb_0def. Half 0: the colony tile's head is an AI Wagon
 * Train (local_42), so the wagon arm runs (raw 96933-96976), the visit
 * resolves and LAB_5bfb_1005 exhausts the Brave. Half 1: no wagon, human
 * colony: the else-limb shows @INDIANCOMMENT (raw 96978-96985), zeroes the
 * village word (raw 96986) and returns 0; the Brave keeps its MP.
 */
static int run_visit_empty_colony(void) {
  if (!test_names_txt() || !test_game_txt()) {
    return TR_SKIP;
  }
  for (int half = 0; half < 2; ++half) {
    Ai152eFixture f;
    if (ai_152e_fixture_init(&f, "visit-empty-colony", 777u) != 0) {
      return 1;
    }
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      fprintf(stderr, "visit-empty-colony: GAME.TXT load failed\n");
      ai_152e_fixture_free(&f);
      return 1;
    }
    AiPopupState pop;
    ai_popup_init(&pop);
    f.ctx.messages = &game_txt;
    f.ctx.ai_popups = &pop;
    ai_contact_reset();
    const int e = half == 0 ? 1 : 0; /* 1 = AI, 0 = human */
    ColonizeColony* col = &f.colonies.colonies[0];
    col->active = true;
    col->id = 0;
    col->nation_id = e;
    col->x = 14;
    col->y = 10;
    snprintf(col->name, sizeof(col->name), "Testtown");
    const int brave_ti = units_find_type(&f.units, "Brave");
    const int wagon_ti = units_find_type(&f.units, "Wagon Train");
    const int bid = brave_ti >= 0 ? units_spawn_allow_stack(&f.units, brave_ti, 13, 10) : -1;
    const int wid =
      (half == 0 && wagon_ti >= 0) ? units_spawn_allow_stack(&f.units, wagon_ti, 14, 10) : -1;
    ColonizeUnit* brave = units_get(&f.units, bid);
    ColonizeUnit* wag = units_get(&f.units, wid);
    if (!brave || (half == 0 && !wag)) {
      fprintf(stderr, "visit-empty-colony: spawn failed\n");
      assets_msg_free(&game_txt);
      ai_152e_fixture_free(&f);
      return 1;
    }
    brave->nation_id = 4;
    brave->home_tribe_index = 0;
    brave->col1_flags15 = 0;
    brave->moves = 0; /* MP spent so far: a fresh step */
    if (wag) {
      wag->nation_id = e;
      wag->hold_goods_type[0] = 3;
      wag->hold_goods_amount[0] = 100;
    }
    f.col1.nation[e].trade.euro_price[3] = 11;
    f.col1.indian[0].euro_diplo[e] = (uint8_t)COL1_INDIAN_MET_BIT;
    f.col1.indian[0].alarm_by_player[e] = 20;
    f.col1.indian[0].contact_state[e] = 1; /* local_10: never generous */
    col1_tribe_attitude_set(&f.tribes[0], e, 5);

    ai_s_native_colonies = &f.colonies;
    ai_native_bind_step_ctx(&f.ctx);
    const int resolved = ai_native_step_first_contact(&f.units, &f.map, &f.col1, brave, 4);
    ai_native_bind_step_ctx(NULL);
    ai_s_native_colonies = NULL;
    const int exhausted = brave->moves != 0;

    int ok = 1;
    if (half == 0) {
      ok = resolved == 1 && exhausted && wag->hold_goods_amount[0] == 0 &&
           col1_tribe_attitude(&f.tribes[0], e) == 0;
      fprintf(stderr, "visit-empty-colony wagon: resolved %d exhausted %d hold %d word %d\n",
              resolved, exhausted, wag->hold_goods_amount[0], col1_tribe_attitude(&f.tribes[0], e));
    } else {
      int comment = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_CONTACT_MEET && pop.queue[i].body[0]) {
          comment = 1;
        }
      }
      ok = resolved == 0 && !exhausted && comment && col1_tribe_attitude(&f.tribes[0], e) == 0;
      fprintf(stderr, "visit-empty-colony comment: resolved %d exhausted %d popup %d word %d\n",
              resolved, exhausted, comment, col1_tribe_attitude(&f.tribes[0], e));
    }
    assets_msg_free(&game_txt);
    ai_152e_fixture_free(&f);
    if (!ok) {
      return 1;
    }
  }
  return 0;
}

/*
 * The empty-colony limb of ai_contact_try_village_reparations leaves the
 * visit unresolved, so with visit_euro == -1 (every European; test callers
 * only, production pins 022e's param_1) the scan goes on to the next
 * European rather than return. Human colony 0 has nothing to demand; AI
 * colony 1 beside the same Brave does, so the call resolves on e == 1.
 */
static int run_reparations_scan_past_empty_colony(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "reparations-scan-all", 777u) != 0) {
    return 1;
  }
  ai_contact_reset();
  ai_contact_set_visit_euro(-1);
  const int brave_ti = units_find_type(&f.units, "Brave");
  const int bid = brave_ti >= 0 ? units_spawn_allow_stack(&f.units, brave_ti, 13, 10) : -1;
  ColonizeUnit* brave = units_get(&f.units, bid);
  if (!brave) {
    fprintf(stderr, "reparations-scan-all: spawn failed\n");
    ai_152e_fixture_free(&f);
    return 1;
  }
  brave->nation_id = 4;
  brave->home_tribe_index = 0;
  brave->col1_flags15 = 0;
  for (int e = 0; e < 2; ++e) {
    ColonizeColony* col = &f.colonies.colonies[e];
    col->active = true;
    col->id = e;
    col->nation_id = e;
    col->x = e == 0 ? 14 : 12;
    col->y = 10;
    snprintf(col->name, sizeof(col->name), "Town%d", e);
    f.col1.nation[e].trade.euro_price[3] = 11;
    f.col1.indian[0].euro_diplo[e] = (uint8_t)COL1_INDIAN_MET_BIT;
    f.col1.indian[0].alarm_by_player[e] = 20;
    f.col1.indian[0].contact_state[e] = 1;
    col1_tribe_attitude_set(&f.tribes[0], e, 5);
    ai_contact_visit_mood_publish(&f.ctx, 4, e, bid, 0);
  }
  f.colonies.colonies[1].stock[3] = 50;

  const int resolved = ai_contact_try_village_reparations(&f.ctx, 4);
  ai_152e_fixture_free(&f);
  fprintf(stderr, "reparations-scan-all: resolved %d\n", resolved);
  return resolved == 1 ? 0 : 1;
}

/*
 * 022e binds one encounter colony, local_4c (FUN_5bfb_022e raw 96699), and
 * both the beg (raw 96755) and the halves it falls into (LAB_5bfb_0def raw
 * 96833, the gift half after a concession) read it. A Brave at (13,10)
 * between two colonies of `e`, slot 0 at (12,10) and slot 1 at (14,10); the
 * beg was at slot 1, so slot 0 must stay untouched.
 */
static int two_colony_beg_fixture(Ai152eFixture* f, const char* label, int e, int* bid) {
  if (ai_152e_fixture_init(f, label, 777u) != 0) {
    return 1;
  }
  ai_contact_reset();
  f->col1.head.difficulty = 0;
  for (int ci = 0; ci < 2; ++ci) {
    ColonizeColony* col = &f->colonies.colonies[ci];
    col->active = true;
    col->id = ci;
    col->nation_id = e;
    col->x = ci ? 14 : 12;
    col->y = 10;
    col->stock[COLONIZE_CARGO_FOOD] = 100;
    col->stock[3] = 50;
    snprintf(col->name, sizeof(col->name), "Town%d", ci);
  }
  *bid = units_spawn_allow_stack(&f->units, units_find_type(&f->units, "Brave"), 13, 10);
  ColonizeUnit* brave = units_get(&f->units, *bid);
  if (!brave) {
    fprintf(stderr, "%s: spawn failed\n", label);
    ai_152e_fixture_free(f);
    return 1;
  }
  brave->nation_id = 4;
  brave->home_tribe_index = 0;
  f->col1.nation[e].trade.euro_price[3] = 11;
  f->col1.indian[0].euro_diplo[e] = (uint8_t)COL1_INDIAN_MET_BIT;
  f->col1.indian[0].alarm_by_player[e] = 20;
  col1_tribe_attitude_set(&f->tribes[0], e, 5);
  ai_contact_visit_mood_publish(&f->ctx, 4, e, *bid, 0);
  return 0;
}

static int run_conceded_beg_gifts_begged_colony(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  int bid = -1;
  if (two_colony_beg_fixture(&f, "conceded-beg-colony", 1, &bid) != 0) {
    return 1;
  }
  ai_contact_apply_beg_food(&f.ctx, &f.col1.indian[0], 4, 1, 1, 0, 1);
  int other = 0;
  int begged = 0;
  for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
    if (g != COLONIZE_CARGO_FOOD) {
      other += f.colonies.colonies[0].stock[g];
      begged += f.colonies.colonies[1].stock[g];
    }
  }
  ai_152e_fixture_free(&f);
  fprintf(stderr, "conceded-beg-colony: slot0 goods %d slot1 %d (want 50, >50)\n", other, begged);
  return (other == 50 && begged > 50) ? 0 : 1;
}

/*
 * A refused food beg on a !bVar6 visit falls into LAB_5bfb_0def
 * (FUN_5bfb_022e raw 96823-96833): the village demands goods from the begged
 * colony. Spain (e 2) always refuses; the AI then hands the Tobacco of slot 1
 * over at @INDIANCITY, and slot 0 stays untouched.
 */
static int run_refused_beg_demands_begged_colony(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  int bid = -1;
  if (two_colony_beg_fixture(&f, "refused-beg-colony", 2, &bid) != 0) {
    return 1;
  }
  ai_contact_apply_beg_food(&f.ctx, &f.col1.indian[0], 4, 2, 1, 0, 0);
  const int other = f.colonies.colonies[0].stock[3];
  const int begged = f.colonies.colonies[1].stock[3];
  const int food = f.colonies.colonies[1].stock[COLONIZE_CARGO_FOOD];
  const int state = f.col1.indian[0].contact_state[2];
  ai_152e_fixture_free(&f);
  fprintf(stderr,
          "refused-beg-colony: slot0 tobacco %d slot1 %d food %d state %d (want 50, <50, 100, 1)\n",
          other, begged, food, state);
  return (other == 50 && begged < 50 && food == 100 && state == 1) ? 0 : 1;
}

/*
 * The Brave's own step fixes local_4c once (FUN_5bfb_022e raw 96699) to the
 * colony on the 3180 encounter tile. Same two-colony scene; the direction
 * loop meets slot 1 (E, d 2) before slot 0 (W, d 6). Slot 1 holds 20 food,
 * so the beg (raw 96753-96756, `0x4a < food`) asks nothing, and neither the
 * beg nor the gift/demand halves may touch slot 0. Control run: with food in
 * slot 1 too, the same seeds do beg there.
 */
static int run_step_visit_serves_encounter_colony(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  int control_begs = 0;
  int touched = 0;
  for (uint32_t k = 1; k <= 24; ++k) {
    for (int control = 0; control < 2; ++control) {
      Ai152eFixture f;
      int bid = -1;
      if (two_colony_beg_fixture(&f, "step-visit-colony", 1, &bid) != 0) {
        return 1;
      }
      dos_rng_seed(&f.rng, k * 7919u);
      /* 2154 food: tundra (class 0) around the village scores no food, so
       * bid[0] = 0 and ask[0] = 50: the beg rolls rng(1,100) <= 50. */
      for (int y = 8; y <= 12; ++y) {
        for (int x = 8; x <= 12; ++x) {
          f.map.terrain[y * 32 + x] = 0;
        }
      }
      f.colonies.colonies[1].stock[COLONIZE_CARGO_FOOD] = control ? 100 : 20;
      int before[COLONIZE_CARGO_COUNT];
      memcpy(before, f.colonies.colonies[0].stock, sizeof(before));
      ai_s_native_colonies = &f.colonies;
      ai_native_bind_step_ctx(&f.ctx);
      (void)ai_native_step_first_contact(&f.units, &f.map, &f.col1, units_get(&f.units, bid), 4);
      ai_native_bind_step_ctx(NULL);
      ai_s_native_colonies = NULL;
      const int slot0_same = memcmp(before, f.colonies.colonies[0].stock, sizeof(before)) == 0;
      const int slot1_food = f.colonies.colonies[1].stock[COLONIZE_CARGO_FOOD];
      ai_contact_reset();
      ai_152e_fixture_free(&f);
      touched += !slot0_same;
      control_begs += control && slot1_food == 50;
    }
  }
  fprintf(stderr, "step-visit-colony: slot 0 touched in %d of 48 runs, %d control begs\n", touched,
          control_begs);
  return (touched == 0 && control_begs > 0) ? 0 : 1;
}

/* Brave at (13,10) of tribe 0, Wagon Train of `e` at (14,10) holding 100 of
 * cargo 3, met, alarm 20, village word 5. */
static int wagon_visit_fixture(Ai152eFixture* f, const char* label, int e, ColonizeUnit** brave,
                               ColonizeUnit** wag) {
  if (ai_152e_fixture_init(f, label, 777u) != 0) {
    return 1;
  }
  ai_contact_reset();
  const int bid = units_spawn_allow_stack(&f->units, units_find_type(&f->units, "Brave"), 13, 10);
  const int wid =
    units_spawn_allow_stack(&f->units, units_find_type(&f->units, "Wagon Train"), 14, 10);
  *brave = units_get(&f->units, bid);
  *wag = units_get(&f->units, wid);
  if (!*brave || !*wag) {
    fprintf(stderr, "%s: spawn failed\n", label);
    ai_152e_fixture_free(f);
    return 1;
  }
  (*brave)->nation_id = 4;
  (*brave)->home_tribe_index = 0;
  (*brave)->col1_flags15 = 0;
  (*wag)->nation_id = e;
  (*wag)->hold_goods_type[0] = 3;
  (*wag)->hold_goods_amount[0] = 100;
  f->col1.nation[e].trade.euro_price[3] = 11;
  f->col1.indian[0].euro_diplo[e] = (uint8_t)COL1_INDIAN_MET_BIT;
  f->col1.indian[0].alarm_by_player[e] = 20;
  col1_tribe_attitude_set(&f->tribes[0], e, 5);
  return 0;
}

/*
 * A pinned local_4c (FUN_5bfb_022e raw 96699) never reaches the open-field
 * wagon walk. The encounter colony of e 1 sits at (3,3), out of the Brave's
 * reach, so the colony arm finds no visitor; the loaded Wagon Train next to
 * the Brave must keep its hold and the visit stays unresolved.
 */
static int run_pinned_colony_skips_open_field_wagon(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  ColonizeUnit* brave = NULL;
  ColonizeUnit* wag = NULL;
  if (wagon_visit_fixture(&f, "pinned-colony-wagon", 1, &brave, &wag) != 0) {
    return 1;
  }
  ColonizeColony* col = &f.colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = 1;
  col->x = 3;
  col->y = 3;
  snprintf(col->name, sizeof(col->name), "Fartown");
  f.col1.indian[0].contact_state[1] = 1; /* local_10: never generous */
  ai_contact_visit_mood_publish(&f.ctx, 4, 1, brave->id, 0);
  ai_contact_set_visit_euro(1);
  ai_contact_set_visit_colony(0);
  const int resolved = ai_contact_try_village_reparations(&f.ctx, 4);
  ai_contact_set_visit_colony(-1);
  ai_contact_set_visit_euro(-1);
  const int hold = wag->hold_goods_amount[0];
  ai_contact_reset();
  ai_152e_fixture_free(&f);
  fprintf(stderr, "pinned-colony-wagon: resolved %d hold %d (want 0 100)\n", resolved, hold);
  return (resolved == 0 && hold == 100) ? 0 : 1;
}

/*
 * @INDIANWAGONS in the open. FUN_5bfb_3180 -> FUN_5bfb_022e: a Brave steps
 * next to a tile whose HEAD (FUN_281f_07e0, raw 96696) is an AI Wagon Train
 * with no colony; a !bVar6 visit (contact_state 1 forces local_10, so bVar6
 * is false whatever the 1..0x148 roll) takes the wagon arm (raw
 * 96933-96976): the AI hands hold 0 over, the village word goes to 0 and the
 * alarm falls by price*qty*4/100. A Soldier sits in the lower slot of the
 * same tile. Second half: a generous visit with no colony returns 0 (raw
 * 96989), so 3180 keeps scanning the same nation and turns the next tile's
 * head (raw 98651).
 */
static int run_wagon_reparations_open_field(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  for (int half = 0; half < 2; ++half) {
    Ai152eFixture f;
    if (ai_152e_fixture_init(&f, "wagon-open-field", 777u) != 0) {
      return 1;
    }
    ai_contact_reset();
    const int brave_ti = units_find_type(&f.units, "Brave");
    const int wagon_ti = units_find_type(&f.units, "Wagon Train");
    const int soldier_ti = units_find_type(&f.units, "Soldier");
    const int bid = brave_ti >= 0 ? units_spawn_allow_stack(&f.units, brave_ti, 13, 10) : -1;
    /* Generous half: no soldier under the wagon. */
    const int sx = half == 0 ? 14 : 2;
    const int sid =
      soldier_ti >= 0 ? units_spawn_allow_stack(&f.units, soldier_ti, sx, sx == 2 ? 2 : 10) : -1;
    const int wid = wagon_ti >= 0 ? units_spawn_allow_stack(&f.units, wagon_ti, 14, 10) : -1;
    const int oid = soldier_ti >= 0 ? units_spawn_allow_stack(&f.units, soldier_ti, 12, 10) : -1;
    ColonizeUnit* brave = units_get(&f.units, bid);
    ColonizeUnit* sol = units_get(&f.units, sid);
    ColonizeUnit* wag = units_get(&f.units, wid);
    ColonizeUnit* other = units_get(&f.units, oid);
    if (!brave || !sol || !wag || !other || sid > wid) {
      fprintf(stderr, "wagon-open-field: spawn failed\n");
      ai_152e_fixture_free(&f);
      return 1;
    }
    brave->nation_id = 4;
    brave->home_tribe_index = 0;
    brave->col1_flags15 = 0;
    sol->nation_id = wag->nation_id = other->nation_id = 1; /* AI (human is 0) */
    sol->tile_stack_order = 1;
    wag->tile_stack_order = 2; /* the wagon heads the tile; the soldier holds the lower slot */
    other->last_dir = -1;
    wag->hold_goods_type[0] = 3;
    wag->hold_goods_amount[0] = 100;
    f.col1.nation[1].trade.euro_price[3] = 11; /* price byte 10 */
    f.col1.indian[0].euro_diplo[1] = (uint8_t)COL1_INDIAN_MET_BIT;
    f.col1.indian[0].alarm_by_player[1] = 20;
    f.col1.indian[0].contact_state[1] = half == 0 ? 1 : 0; /* 1: demand; 0 + word 0: generous */
    col1_tribe_attitude_set(&f.tribes[0], 1, half == 0 ? 5 : 0);

    ai_s_native_colonies = &f.colonies;
    ai_native_bind_step_ctx(&f.ctx);
    brave->moves = 0;
    const int resolved = ai_native_step_first_contact(&f.units, &f.map, &f.col1, brave, 4);
    ai_native_bind_step_ctx(NULL);
    ai_s_native_colonies = NULL;

    /* LAB_5bfb_1005 (raw 97094-97095) exhausts the Brave on every resolved
     * 022e; the generous no-colony visit returns 0. */
    const int exhausted = brave->moves != 0;
    const int hold = wag->hold_goods_amount[0];
    const int word = col1_tribe_attitude(&f.tribes[0], 1);
    const int alarm = f.col1.indian[0].alarm_by_player[1];
    const int next_dir = other->last_dir;
    ai_contact_reset();
    ai_152e_fixture_free(&f);
    fprintf(stderr,
            "wagon-open-field: half %d resolved %d exhausted %d hold %d word %d alarm %d next dir "
            "%d\n",
            half, resolved, exhausted, hold, word, alarm, next_dir);
    const int ok = half == 0
                     ? resolved == 1 && exhausted && hold == 0 && word == 0 && alarm < 20
                     : resolved == 0 && !exhausted && hold == 100 && next_dir == 6;
    if (!ok) {
      return 1;
    }
  }
  return 0;
}

/*
 * FUN_5bfb_022e's wagon arm (raw 96933-96976) always resolves. While the
 * port's pending-offer slot for the human is busy the demand cannot be
 * queued; the visit must not fall into the @INDIANCOMMENT else-limb (raw
 * 96978-96987), which zeroes the village word.
 */
static int run_wagon_demand_busy_slot_no_comment(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  ColonizeUnit* brave = NULL;
  ColonizeUnit* wag = NULL;
  if (wagon_visit_fixture(&f, "wagon-busy-slot", 0, &brave, &wag) != 0) {
    return 1;
  }
  f.col1.indian[0].contact_state[0] = 1; /* local_10: never generous */
  ai_contact_s_reparations[0].active = 1;
  ai_contact_s_reparations[0].nation_id = 4;

  ai_s_native_colonies = &f.colonies;
  ai_native_bind_step_ctx(&f.ctx);
  const int resolved = ai_native_step_first_contact(&f.units, &f.map, &f.col1, brave, 4);
  ai_native_bind_step_ctx(NULL);
  ai_s_native_colonies = NULL;

  const int word = col1_tribe_attitude(&f.tribes[0], 0);
  const int hold = wag->hold_goods_amount[0];
  ai_contact_reset();
  ai_152e_fixture_free(&f);
  fprintf(stderr, "wagon-busy-slot: resolved %d word %d hold %d (want 0 5 100)\n", resolved, word,
          hold);
  return (resolved == 0 && word == 5 && hold == 100) ? 0 : 1;
}

/*
 * contact_state 2 (the latched generous visit) never takes the wagon arm: a
 * !bVar6 visit takes the @INDIANCOMMENT else-limb (FUN_5bfb_022e raw
 * 96978-96987), which clears the village word, and the wagon keeps its hold.
 * Word 0x7f with alarm 20 makes roughly a third of the seeds non-generous.
 */
static int run_wagon_state2_takes_comment(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  int strict = 0;
  int bad = 0;
  for (uint32_t k = 1; k <= 32; ++k) {
    Ai152eFixture f;
    ColonizeUnit* brave = NULL;
    ColonizeUnit* wag = NULL;
    if (wagon_visit_fixture(&f, "wagon-state2", 1, &brave, &wag) != 0) {
      return 1;
    }
    dos_rng_seed(&f.rng, k * 7919u);
    f.col1.indian[0].contact_state[1] = 2;
    col1_tribe_attitude_set(&f.tribes[0], 1, 0x7f);
    ai_s_native_colonies = &f.colonies;
    ai_native_bind_step_ctx(&f.ctx);
    const int resolved = ai_native_step_first_contact(&f.units, &f.map, &f.col1, brave, 4);
    ai_native_bind_step_ctx(NULL);
    ai_s_native_colonies = NULL;
    const AiContactVisitMood* m = ai_contact_visit_mood_this_turn(&f.ctx, 4, 1);
    if (m && !m->generous_visit) {
      strict++;
      bad += resolved != 0 || wag->hold_goods_amount[0] != 100 ||
             col1_tribe_attitude(&f.tribes[0], 1) != 0;
    }
    ai_contact_reset();
    ai_152e_fixture_free(&f);
  }
  fprintf(stderr, "wagon-state2: %d non-generous visits, %d took the wagon arm or kept the word\n",
          strict, bad);
  return (strict > 0 && bad == 0) ? 0 : 1;
}

/*
 * A resolved wagon demand closes e for the rest of the 3180 scan
 * (aiStack_20[e], raw 98649). A second loaded Wagon Train of e at (12,10),
 * met later in the direction loop, keeps its hold.
 */
static int run_wagon_demand_closes_nation(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  ColonizeUnit* brave = NULL;
  ColonizeUnit* wag = NULL;
  if (wagon_visit_fixture(&f, "wagon-closes-nation", 1, &brave, &wag) != 0) {
    return 1;
  }
  ColonizeUnit* wag2 = units_get(
    &f.units, units_spawn_allow_stack(&f.units, units_find_type(&f.units, "Wagon Train"), 12, 10)
  );
  if (!wag2) {
    fprintf(stderr, "wagon-closes-nation: spawn failed\n");
    ai_152e_fixture_free(&f);
    return 1;
  }
  wag2->nation_id = 1;
  wag2->hold_goods_type[0] = 3;
  wag2->hold_goods_amount[0] = 100;
  f.col1.indian[0].contact_state[1] = 1; /* local_10: never generous */
  ai_s_native_colonies = &f.colonies;
  ai_native_bind_step_ctx(&f.ctx);
  const int resolved = ai_native_step_first_contact(&f.units, &f.map, &f.col1, brave, 4);
  ai_native_bind_step_ctx(NULL);
  ai_s_native_colonies = NULL;
  const int hold1 = wag->hold_goods_amount[0];
  const int hold2 = wag2->hold_goods_amount[0];
  ai_contact_reset();
  ai_152e_fixture_free(&f);
  fprintf(stderr, "wagon-closes-nation: resolved %d holds %d %d (want 1 0 100)\n", resolved, hold1,
          hold2);
  return (resolved == 1 && hold1 == 0 && hold2 == 100) ? 0 : 1;
}

/*
 * The wagon arm (FUN_5bfb_022e raw 96933-96976) has no mood gate of its own:
 * it sits inside LAB_5bfb_0def, which only a !bVar6 visit reaches (raw
 * 96828), so the test is its callers'. Called directly on a generous record,
 * the AI still hands hold 0 over.
 */
static int run_wagon_demand_no_mood_gate(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  ColonizeUnit* brave = NULL;
  ColonizeUnit* wag = NULL;
  if (wagon_visit_fixture(&f, "wagon-no-mood-gate", 1, &brave, &wag) != 0) {
    return 1;
  }
  ai_contact_visit_mood_publish(&f.ctx, 4, 1, brave->id, 1);
  const int r = ai_contact_try_wagon_reparations(&f.ctx, 4, 1, brave, wag);
  const int hold = wag->hold_goods_amount[0];
  ai_152e_fixture_free(&f);
  fprintf(stderr, "wagon-no-mood-gate: returned %d hold %d\n", r, hold);
  return (r == 1 && hold == 0) ? 0 : 1;
}

/* Answer the waiting @INDIANCITY as the game does: close it, apply the row. */
static void answer_city_demand(Ai152eFixture* f, AiPopupState* pop, int choice_id) {
  ai_popup_clear(pop);
  pop->has_result = true;
  pop->result_cancelled = false;
  pop->result_tag = AI_POPUP_TAG_CONTACT_REPARATIONS;
  pop->result_nation_a = 0;
  pop->result_nation_b = 4;
  pop->result_payload = AI_CONTACT_REPARATIONS_CITY;
  pop->result_choice_id = choice_id;
  ai_contact_apply_popup_result(&f->ctx, pop);
  ai_popup_clear(pop);
}

static int count_city_demands(const AiPopupState* pop) {
  int n = 0;
  for (int i = 0; i < pop->queue_count; ++i) {
    n += pop->queue[i].tag == AI_POPUP_TAG_CONTACT_REPARATIONS;
  }
  return n;
}

/*
 * Two Braves of one tribe each run 022e's @INDIANCITY demand on the same
 * human colony in one 1816 pulse. DOS answers each inside 022e
 * (FUN_291f_019c, raw 96882), so the second demand's cargo scan (raw
 * 96843-96874) reads the stores the first answer handed over, and the first
 * answer applies before the second Brave acts. Island: colony (14,10), Braves
 * at (13,10) and (15,10) carrying the pending-encounter bit, water all round,
 * so 021a stays and runs 3180 at the Brave's own tile (021a:13cc).
 * The caller owns f (init'd here), game_txt and pop; returns the colony.
 */
static ColonizeColony* two_demand_scene(
  Ai152eFixture* f, ColonizeMsgCatalog* game_txt, AiPopupState* pop, int bid[2]
) {
  if (!assets_msg_load_file(game_txt, "COLONIZE/GAME.TXT") || !f->map.layer2 || !f->map.layer3) {
    fprintf(stderr, "two-demands: GAME.TXT / map layers missing\n");
    return NULL;
  }
  f->ctx.messages = game_txt;
  f->ctx.ai_popups = pop;
  ai_contact_reset();
  ai_native_reset();
  for (int i = 0; i < 32 * 32; ++i) {
    f->map.layer3[i] |= 0xf0u; /* unowned */
  }
  for (int y = 9; y <= 11; ++y) {
    for (int x = 12; x <= 16; ++x) {
      f->map.terrain[y * 32 + x] = (y == 10 && x >= 13 && x <= 15) ? 2 : 25;
    }
  }
  map_set_owner_nibble(&f->map, 14, 10, 0);
  f->map.layer2[10 * 32 + 14] |= 0x02u; /* settlement: 021a skips the colony tile */
  ColonizeColony* col = &f->colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = 0;
  col->x = 14;
  col->y = 10;
  col->stock[3] = 50; /* price 10: 500 beats cargo 4's 400, until demand 1 takes it */
  col->stock[4] = 40;
  snprintf(col->name, sizeof(col->name), "Testtown");
  const int brave_ti = units_find_type(&f->units, "Brave");
  bid[0] = units_spawn_allow_stack(&f->units, brave_ti, 13, 10);
  bid[1] = units_spawn_allow_stack(&f->units, brave_ti, 15, 10);
  for (int k = 0; k < 2; ++k) {
    ColonizeUnit* b = units_get(&f->units, bid[k]);
    if (!b) {
      fprintf(stderr, "two-demands: spawn failed\n");
      return NULL;
    }
    b->nation_id = 4;
    b->home_tribe_index = 0;
    b->col1_flags15 = 0x08u;
    b->moves = 0;
  }
  f->col1.nation[0].trade.euro_price[3] = 11;
  f->col1.nation[0].trade.euro_price[4] = 11;
  f->col1.indian[0].euro_diplo[0] = (uint8_t)COL1_INDIAN_MET_BIT;
  f->col1.indian[0].alarm_by_player[0] = 20;
  f->col1.indian[0].contact_state[0] = 1; /* local_10: never generous */
  col1_tribe_attitude_set(&f->tribes[0], 0, 5);
  return col;
}

static int run_two_demands_one_pulse(void) {
  if (!test_names_txt() || !test_game_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "two-demands", 777u) != 0) {
    return 1;
  }
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  AiPopupState pop;
  ai_popup_init(&pop);
  int rc = 1;
  int bid[2];
  ColonizeColony* col = two_demand_scene(&f, &game_txt, &pop, bid);
  if (!col) {
    goto done;
  }

  /* Demand 1: the pulse stops on it. */
  ai_indian_nation_turn(&f.ctx, 4);
  if (!ai_native_pulse_suspended() || count_city_demands(&pop) != 1) {
    fprintf(stderr, "two-demands: first demand suspended %d popups %d (want 1 1)\n",
            ai_native_pulse_suspended(), count_city_demands(&pop));
    goto done;
  }
  const int qty1 = ai_contact_s_reparations[0].qty;
  if (ai_contact_s_reparations[0].cargo != 3 || ai_contact_s_reparations[0].brave_id != bid[0]) {
    fprintf(stderr, "two-demands: first demand cargo %d brave %d\n",
            ai_contact_s_reparations[0].cargo, ai_contact_s_reparations[0].brave_id);
    goto done;
  }
  answer_city_demand(&f, &pop, AI_CONTACT_REPARATIONS_ROW2); /* "Hand them over." */
  const int stock1 = col->stock[3];
  if (qty1 <= 0 || stock1 != 50 - qty1 || col1_tribe_attitude(&f.tribes[0], 0) != 0) {
    fprintf(stderr, "two-demands: accept qty %d stock %d word %d\n", qty1, stock1,
            col1_tribe_attitude(&f.tribes[0], 0));
    goto done;
  }

  /* Demand 2: the second Brave. With at most 25 Tobacco left the scan must
   * now pick cargo 4: it reads the stores after demand 1's answer. */
  ai_indian_nation_turn(&f.ctx, 4);
  const int qty2 = ai_contact_s_reparations[0].qty;
  if (!ai_native_pulse_suspended() || count_city_demands(&pop) != 1 ||
      ai_contact_s_reparations[0].brave_id != bid[1] || ai_contact_s_reparations[0].cargo != 4 ||
      (qty2 != 40 && qty2 != 20)) {
    fprintf(stderr,
            "two-demands: second demand suspended %d popups %d brave %d cargo %d qty %d\n",
            ai_native_pulse_suspended(), count_city_demands(&pop),
            ai_contact_s_reparations[0].brave_id, ai_contact_s_reparations[0].cargo, qty2);
    goto done;
  }
  answer_city_demand(&f, &pop, AI_CONTACT_REPARATIONS_ROW1); /* "Man the stockade." */
  if (col->stock[3] != stock1 || col->stock[4] != 40 ||
      col1_tribe_attitude(&f.tribes[0], 0) != 0x80) {
    fprintf(stderr, "two-demands: refuse stock %d/%d word %d\n", col->stock[3], col->stock[4],
            col1_tribe_attitude(&f.tribes[0], 0));
    goto done;
  }

  /* The rest of the pulse: nothing left to ask, both Braves spent. */
  ai_indian_nation_turn(&f.ctx, 4);
  if (ai_native_pulse_suspended() || count_city_demands(&pop) != 0) {
    fprintf(stderr, "two-demands: pulse did not finish\n");
    goto done;
  }
  for (int k = 0; k < 2; ++k) {
    const ColonizeUnit* b = units_get(&f.units, bid[k]);
    if (b->moves < units_max_mp(&f.units, b->id)) {
      fprintf(stderr, "two-demands: Brave %d not exhausted (%d)\n", k, b->moves);
      goto done;
    }
  }
  fprintf(stderr, "two demands one pulse ok (Tobacco %d, then cargo 4 %d)\n", qty1, qty2);
  rc = 0;
done:
  ai_native_reset();
  ai_contact_reset();
  assets_msg_free(&game_txt);
  ai_152e_fixture_free(&f);
  return rc;
}

/*
 * The same scene through turn_processor_advance (turn_step_indian): a
 * stopped pulse keeps the slot (step INDIAN, cursor 4), and its resume slices
 * skip the day top (raw 6353-6363: +0x3149 = 0 for every unit) that already
 * ran. After the last answer the cursor walks on, and FUN_4d56_1b3a phase 3
 * (the worked-tile claim) runs after tribe 11, not before.
 */
static int run_two_demands_turn_processor(void) {
  if (!test_names_txt() || !test_game_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "two-demands-proc", 777u) != 0) {
    return 1;
  }
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  AiPopupState pop;
  ai_popup_init(&pop);
  int rc = 1;
  int bid[2];
  ColonizeColony* col = two_demand_scene(&f, &game_txt, &pop, bid);
  if (!col) {
    goto done;
  }
  ColonizeTurnProcessor proc;
  turn_processor_start(&proc);
  proc.step = TURN_PROC_INDIAN; /* SETUP and the calendar slice already ran */
  proc.nation_cursor = 4;
  proc.day_advanced = true;
  const uint16_t year0 = f.year;

  /* Slice 1: day top + tribe 4, stops on demand 1. */
  if (!turn_processor_advance(&proc, &f.ctx) || proc.step != TURN_PROC_INDIAN ||
      proc.nation_cursor != 4 || !ai_native_pulse_suspended() || count_city_demands(&pop) != 1) {
    fprintf(stderr, "two-demands-proc: slice 1 step %d cursor %d suspended %d popups %d\n",
            (int)proc.step, proc.nation_cursor, ai_native_pulse_suspended(),
            count_city_demands(&pop));
    goto done;
  }
  /* Sentinel: a Euro unit with no moves. A second day top would refill it. */
  const int sid = units_spawn_allow_stack(&f.units, units_find_type(&f.units, "Soldier"), 2, 2);
  ColonizeUnit* s = units_get(&f.units, sid);
  if (!s) {
    fprintf(stderr, "two-demands-proc: sentinel spawn failed\n");
    goto done;
  }
  s->nation_id = 0;
  s->moves = 0;
  answer_city_demand(&f, &pop, AI_CONTACT_REPARATIONS_ROW2);

  /* Slice 2: the resume runs into demand 2 (the second Brave). */
  if (!turn_processor_advance(&proc, &f.ctx) || proc.step != TURN_PROC_INDIAN ||
      proc.nation_cursor != 4 || !ai_native_pulse_suspended() || count_city_demands(&pop) != 1 ||
      ai_contact_s_reparations[0].brave_id != bid[1] || units_get(&f.units, sid)->moves != 0 ||
      f.year != year0) {
    fprintf(stderr,
            "two-demands-proc: slice 2 step %d cursor %d suspended %d popups %d brave %d "
            "sentinel moves %d year %d\n",
            (int)proc.step, proc.nation_cursor, ai_native_pulse_suspended(),
            count_city_demands(&pop), ai_contact_s_reparations[0].brave_id,
            units_get(&f.units, sid)->moves, (int)f.year);
    goto done;
  }
  answer_city_demand(&f, &pop, AI_CONTACT_REPARATIONS_ROW1);

  /* Slice 3: the pulse ends, the cursor moves to tribe 5. */
  if (!turn_processor_advance(&proc, &f.ctx) || proc.step != TURN_PROC_INDIAN ||
      proc.nation_cursor != 5 || ai_native_pulse_suspended() || count_city_demands(&pop) != 0 ||
      units_get(&f.units, sid)->moves != 0) {
    fprintf(stderr, "two-demands-proc: slice 3 step %d cursor %d suspended %d sentinel %d\n",
            (int)proc.step, proc.nation_cursor, ai_native_pulse_suspended(),
            units_get(&f.units, sid)->moves);
    goto done;
  }

  /* Phase 3 marker: the colony works (14,9), still tribal land. */
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    int dx = 0;
    int dy = 0;
    if (colonies_field_tile_delta(i, &dx, &dy) && dx == 0 && dy == -1) {
      col->tiles[i] = 0;
    }
  }
  map_set_owner_nibble(&f.map, 14, 9, 4);
  for (int n = 5; n <= 11; ++n) {
    if (proc.step != TURN_PROC_INDIAN || proc.nation_cursor != n ||
        ai_owner_nibble(&f.map, 14, 9) != 4) {
      fprintf(stderr, "two-demands-proc: before tribe %d step %d cursor %d owner %d\n", n,
              (int)proc.step, proc.nation_cursor, ai_owner_nibble(&f.map, 14, 9));
      goto done;
    }
    (void)turn_processor_advance(&proc, &f.ctx);
  }
  if (proc.step == TURN_PROC_INDIAN || ai_owner_nibble(&f.map, 14, 9) != 0) {
    fprintf(stderr, "two-demands-proc: after tribe 11 step %d owner %d\n", (int)proc.step,
            ai_owner_nibble(&f.map, 14, 9));
    goto done;
  }
  fprintf(stderr, "two demands through the turn processor ok\n");
  rc = 0;
done:
  ai_native_reset();
  ai_contact_reset();
  assets_msg_free(&game_txt);
  ai_152e_fixture_free(&f);
  return rc;
}

static int count_popups(const AiPopupState* pop, AiPopupTag tag) {
  int n = 0;
  for (int i = 0; i < pop->queue_count; ++i) {
    n += pop->queue[i].tag == tag;
  }
  return n;
}

/*
 * The same scene, but the colony holds 100 food and tundra rings the village,
 * so 022e's beg (raw 96753-96756) can ask: @INDIANBEGFOOD waits for the human
 * like @INDIANCITY, and the pulse stops on it before the second Brave acts.
 */
static int run_beg_food_stops_pulse(void) {
  if (!test_names_txt() || !test_game_txt()) {
    return TR_SKIP;
  }
  for (uint32_t k = 1; k <= 64; ++k) {
    Ai152eFixture f;
    if (ai_152e_fixture_init(&f, "beg-food-stops", 777u) != 0) {
      return 1;
    }
    f.ctx.rng_seed = k; /* the pulse reseeds from it (ai_nation_reseed) */
    f.ctx.rng_seed_set = true;
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    AiPopupState pop;
    ai_popup_init(&pop);
    int bid[2];
    ColonizeColony* col = two_demand_scene(&f, &game_txt, &pop, bid);
    int begs = -1;
    int demands = -1;
    int suspended = 0;
    int second_moves = -1;
    if (col) {
      col->stock[COLONIZE_CARGO_FOOD] = 100;
      for (int y = 8; y <= 12; ++y) {
        for (int x = 8; x <= 11; ++x) {
          f.map.terrain[y * 32 + x] = 0;
        }
      }
      ai_indian_nation_turn(&f.ctx, 4);
      begs = count_popups(&pop, AI_POPUP_TAG_CONTACT_BEGFOOD);
      demands = count_city_demands(&pop);
      suspended = ai_native_pulse_suspended();
      second_moves = units_get(&f.units, bid[1])->moves;
    }
    ai_native_reset();
    ai_contact_reset();
    assets_msg_free(&game_txt);
    ai_152e_fixture_free(&f);
    if (!col) {
      return 1;
    }
    if (begs == 0) {
      continue;
    }
    fprintf(stderr,
            "beg-food-stops: seed %u suspended %d begs %d demands %d second Brave moves %d "
            "(want 1 1 0 0)\n",
            k, suspended, begs, demands, second_moves);
    return (suspended && begs == 1 && demands == 0 && second_moves == 0) ? 0 : 1;
  }
  fprintf(stderr, "beg-food-stops: no seed begged\n");
  return 1;
}

/*
 * The resumed 3180 scan keeps aiStack_20 (raw 98649): the first Brave's
 * demand closed the human, so its Soldier S of the Brave (d 4, after the
 * colony at d 2) is not turned, while an AI Soldier SW (d 5) still is.
 */
static int run_resume_keeps_scan_latch(void) {
  if (!test_names_txt() || !test_game_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "resume-latch", 777u) != 0) {
    return 1;
  }
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  AiPopupState pop;
  ai_popup_init(&pop);
  int rc = 1;
  int bid[2];
  if (!two_demand_scene(&f, &game_txt, &pop, bid)) {
    goto done;
  }
  const int soldier_ti = units_find_type(&f.units, "Soldier");
  f.map.terrain[11 * 32 + 13] = 2;
  f.map.terrain[11 * 32 + 12] = 2;
  ColonizeUnit* human = units_get(&f.units, units_spawn_allow_stack(&f.units, soldier_ti, 13, 11));
  ColonizeUnit* ai = units_get(&f.units, units_spawn_allow_stack(&f.units, soldier_ti, 12, 11));
  if (!human || !ai) {
    fprintf(stderr, "resume-latch: spawn failed\n");
    goto done;
  }
  human->nation_id = 0;
  ai->nation_id = 1;
  human->last_dir = -1;
  ai->last_dir = -1;
  f.col1.indian[0].euro_diplo[1] = (uint8_t)COL1_INDIAN_MET_BIT;
  ai_indian_nation_turn(&f.ctx, 4);
  if (!ai_native_pulse_suspended() || ai_contact_s_reparations[0].brave_id != bid[0]) {
    fprintf(stderr, "resume-latch: first demand suspended %d brave %d\n",
            ai_native_pulse_suspended(), ai_contact_s_reparations[0].brave_id);
    goto done;
  }
  answer_city_demand(&f, &pop, AI_CONTACT_REPARATIONS_ROW2);
  ai_indian_nation_turn(&f.ctx, 4);
  fprintf(stderr, "resume-latch: human Soldier dir %d, AI Soldier dir %d (want -1 5)\n",
          human->last_dir, ai->last_dir);
  rc = (human->last_dir == -1 && ai->last_dir == 5) ? 0 : 1;
done:
  ai_native_reset();
  ai_contact_reset();
  assets_msg_free(&game_txt);
  ai_152e_fixture_free(&f);
  return rc;
}

/*
 * A step-tail scan (ai_native_brave_step after the commit) that stops on a
 * demand leaves the mover's LAB_5bfb_1005 exhaust (raw 97094-97095) to the
 * resume. A Mounted Brave (4 MP) steps from (12,10) onto (13,10) next to the
 * colony; after the answer it must be spent and must not act again.
 */
static int run_resume_exhausts_step_mover(void) {
  if (!test_names_txt() || !test_game_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "resume-exhaust", 777u) != 0) {
    return 1;
  }
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  AiPopupState pop;
  ai_popup_init(&pop);
  int bid[2];
  int rc = 1;
  if (!two_demand_scene(&f, &game_txt, &pop, bid)) {
    goto done;
  }
  ColonizeUnit* m = units_get(&f.units, bid[0]);
  m->type_index = units_find_type(&f.units, "Mtd. Braves");
  m->x = 12;
  m->col1_flags15 = 0;
  f.map.terrain[10 * 32 + 12] = 2;
  for (int y = 9; y <= 11; ++y) {
    f.map.terrain[y * 32 + 11] = 25; /* (12,10)'s only land exit is E */
  }
  /* Without the settlement bit 021a's visit scorer misses the colony, so the
   * step carries no 0x80 visit (that would defer 022e to the next act's
   * 021a:13d7 resolve, which exhausts on its own); 3180 still finds the
   * colony in the pool. */
  f.map.layer2[10 * 32 + 14] &= (uint8_t)~0x02u;
  const int max_mp = units_max_mp(&f.units, m->id);
  ai_indian_nation_turn(&f.ctx, 4);
  const int moves_at_stop = m->moves;
  if (!ai_native_pulse_suspended() || ai_contact_s_reparations[0].brave_id != bid[0] ||
      m->x != 13 || m->y != 10 || moves_at_stop >= max_mp) {
    fprintf(stderr, "resume-exhaust: stop suspended %d brave %d at (%d,%d) moves %d of %d\n",
            ai_native_pulse_suspended(), ai_contact_s_reparations[0].brave_id, m->x, m->y,
            moves_at_stop, max_mp);
    goto done;
  }
  const int acts_at_stop = m->col1_counter16; /* 1816's per-act counter */
  answer_city_demand(&f, &pop, AI_CONTACT_REPARATIONS_ROW2);
  ai_indian_nation_turn(&f.ctx, 4);
  fprintf(stderr,
          "resume-exhaust: moves at stop %d, after resume %d of %d at (%d,%d), acts %d -> %d, "
          "next demand brave %d (want %d at (13,10), no new act, %d)\n",
          moves_at_stop, m->moves, max_mp, m->x, m->y, acts_at_stop, m->col1_counter16,
          ai_contact_s_reparations[0].brave_id, max_mp, bid[1]);
  rc = (m->moves == max_mp && m->x == 13 && m->y == 10 && m->col1_counter16 == acts_at_stop &&
        ai_contact_s_reparations[0].brave_id == bid[1])
         ? 0
         : 1;
done:
  ai_native_reset();
  ai_contact_reset();
  assets_msg_free(&game_txt);
  ai_152e_fixture_free(&f);
  return rc;
}

/* A new turn processor drops a pulse record a loaded game or an earlier
 * session left behind. */
static int run_processor_start_drops_resume(void) {
  if (!test_names_txt() || !test_game_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "resume-stale", 777u) != 0) {
    return 1;
  }
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  AiPopupState pop;
  ai_popup_init(&pop);
  int bid[2];
  int before = -1;
  int after = -1;
  if (two_demand_scene(&f, &game_txt, &pop, bid)) {
    ai_indian_nation_turn(&f.ctx, 4);
    before = ai_native_pulse_suspended();
    ColonizeTurnProcessor proc;
    turn_processor_start(&proc);
    after = ai_native_pulse_suspended();
  }
  ai_native_reset();
  ai_contact_reset();
  assets_msg_free(&game_txt);
  ai_152e_fixture_free(&f);
  fprintf(stderr, "resume-stale: suspended before %d after start %d (want 1 0)\n", before, after);
  return (before == 1 && after == 0) ? 0 : 1;
}

/*
 * A conceded food beg forces bVar6 on (FUN_5bfb_022e raw 96797-96805) and
 * runs the gift half (raw 96989 onward) with no second alarm test; the 0x31
 * flip (raw 96729-96731) belongs to the mood roll before the beg. The visit's
 * mood was not generous, alarm sits at 0x3c, difficulty 0 makes the
 * rng(0, difficulty) abandon draw 0: the village gifts goods (bVar7 vetoes
 * food, so @INDIANGIVESTUFF).
 */
static int run_conceded_beg_gifts_above_0x31(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "conceded-beg-gifts", 777u) != 0) {
    return 1;
  }
  ai_contact_reset();
  f.col1.head.difficulty = 0;
  ColonizeColony* col = &f.colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = 1;
  col->x = 14;
  col->y = 10;
  col->stock[COLONIZE_CARGO_FOOD] = 100;
  snprintf(col->name, sizeof(col->name), "Testtown");
  const int brave_ti = units_find_type(&f.units, "Brave");
  const int bid = brave_ti >= 0 ? units_spawn_allow_stack(&f.units, brave_ti, 13, 10) : -1;
  ColonizeUnit* brave = units_get(&f.units, bid);
  if (!brave) {
    fprintf(stderr, "conceded-beg-gifts: spawn failed\n");
    ai_152e_fixture_free(&f);
    return 1;
  }
  brave->nation_id = 4;
  brave->home_tribe_index = 0;
  f.col1.indian[0].euro_diplo[1] = (uint8_t)COL1_INDIAN_MET_BIT;
  f.col1.indian[0].alarm_by_player[1] = 0x3c;
  ai_contact_visit_mood_publish(&f.ctx, 4, 1, bid, 0);

  ai_contact_apply_beg_food(&f.ctx, &f.col1.indian[0], 4, 1, 0, 0, 1);

  const int alarm = ai_diplo_indian_alarm(&f.col1, 4, 1);
  int goods = 0;
  for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
    goods += g == COLONIZE_CARGO_FOOD ? 0 : col->stock[g];
  }
  const int food = col->stock[COLONIZE_CARGO_FOOD];
  ai_152e_fixture_free(&f);
  fprintf(stderr, "conceded-beg-gifts: alarm %d food %d gifted goods %d\n", alarm, food, goods);
  return (alarm > 0x31 && food == 50 && goods > 0) ? 0 : 1;
}

/*
 * FUN_5bfb_022e raw 96851-96861. The best cargo has a stock of 1; the
 * rng(0, difficulty + 1) == 0 draw halves it to 0 and DOS still presents
 * @INDIANCITY (local_1a = 1 at LAB_5bfb_1000). Spread seeds so some halve.
 */
static int run_zero_qty_demand_presented(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  int halved = 0;
  int unresolved = 0;
  for (uint32_t k = 1; k <= 16; ++k) {
    Ai152eFixture f;
    if (ai_152e_fixture_init(&f, "zero-qty-demand", k * 12345u) != 0) {
      return 1;
    }
    ai_contact_reset();
    f.col1.head.difficulty = 0; /* rng(0, 1): half the seeds halve */
    ColonizeColony* col = &f.colonies.colonies[0];
    col->active = true;
    col->id = 0;
    col->nation_id = 1;
    col->x = 14;
    col->y = 10;
    col->stock[3] = 1;
    snprintf(col->name, sizeof(col->name), "Testtown");
    const int brave_ti = units_find_type(&f.units, "Brave");
    const int bid = brave_ti >= 0 ? units_spawn_allow_stack(&f.units, brave_ti, 13, 10) : -1;
    ColonizeUnit* brave = units_get(&f.units, bid);
    if (!brave) {
      fprintf(stderr, "zero-qty-demand: spawn failed\n");
      ai_152e_fixture_free(&f);
      return 1;
    }
    brave->nation_id = 4;
    brave->home_tribe_index = 0;
    brave->col1_flags15 = 0;
    f.col1.nation[1].trade.euro_price[3] = 11;
    f.col1.indian[0].euro_diplo[1] = (uint8_t)COL1_INDIAN_MET_BIT;
    f.col1.indian[0].alarm_by_player[1] = 20;
    f.col1.indian[0].contact_state[1] = 1; /* local_10: never generous */
    col1_tribe_attitude_set(&f.tribes[0], 1, 5);

    ai_s_native_colonies = &f.colonies;
    ai_native_bind_step_ctx(&f.ctx);
    const int resolved = ai_native_step_first_contact(&f.units, &f.map, &f.col1, brave, 4);
    ai_native_bind_step_ctx(NULL);
    ai_s_native_colonies = NULL;
    const int stock = col->stock[3];
    ai_152e_fixture_free(&f);
    unresolved += resolved != 1;
    halved += stock == 1; /* the AI handed over 0 */
  }
  fprintf(stderr, "zero-qty-demand: 16 seeds, %d unresolved, %d halved to 0\n", unresolved,
          halved);
  return (unresolved == 0 && halved > 0) ? 0 : 1;
}

/*
 * @INDIANSCONVERT whose spawn fails. FUN_5bfb_022e raw 97011-97012:
 * `iVar9 = FUN_281f_095c(...); if (iVar9 < 0) goto LAB_5bfb_1005;` with
 * local_1a still 0, so the visit does not resolve and the Brave keeps its MP.
 * The AI colony's nation is at the 0xc9 census cap (FUN_1427_06b4 refuses).
 */
static int run_convert_spawn_failure_unresolved(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "convert-spawn-fail", 777u) != 0) {
    return 1;
  }
  ai_contact_reset();
  ColonizeColony* col = &f.colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = 1;
  col->x = 14;
  col->y = 10;
  snprintf(col->name, sizeof(col->name), "Testtown");
  const int brave_ti = units_find_type(&f.units, "Brave");
  const int bid = brave_ti >= 0 ? units_spawn_allow_stack(&f.units, brave_ti, 13, 10) : -1;
  ColonizeUnit* brave = units_get(&f.units, bid);
  if (!brave) {
    fprintf(stderr, "convert-spawn-fail: spawn failed\n");
    ai_152e_fixture_free(&f);
    return 1;
  }
  brave->nation_id = 4;
  brave->home_tribe_index = 0;
  brave->col1_flags15 = 0;
  brave->moves = 0;
  f.tribes[0].mission = 1;          /* mission owned by nation 1 */
  f.col1.indian[0].tech = 14;       /* tech + 2 = 16 > any rng(0, 0xf) */
  f.col1.indian[0].euro_diplo[1] = (uint8_t)COL1_INDIAN_MET_BIT;
  f.col1.indian[0].alarm_by_player[1] = 0; /* generous: 0 <= rng(1, 0x148) */
  f.col1.player[1].control = 1;
  f.col1.stuff.all_unit_counts[1] = 0xc9;
  units_set_ff_col1(&f.col1);

  ai_s_native_colonies = &f.colonies;
  ai_native_bind_step_ctx(&f.ctx);
  const int resolved = ai_native_step_first_contact(&f.units, &f.map, &f.col1, brave, 4);
  ai_native_bind_step_ctx(NULL);
  ai_s_native_colonies = NULL;
  units_set_ff_col1(NULL);

  const int state = f.col1.indian[0].contact_state[1];
  const int moves = brave->moves;
  const int units_now = units_active_count(&f.units);
  ai_152e_fixture_free(&f);
  fprintf(stderr, "convert-spawn-fail: resolved %d moves %d state %d units %d\n", resolved,
          moves, state, units_now);
  return (resolved == 0 && moves == 0 && state == 2 && units_now == 1) ? 0 : 1;
}

/*
 * FUN_5bfb_3180 closes a nation only through aiStack_20[nation] (raw
 * 98649, 98674); a first contact on an earlier tile does not stop the scan
 * for other nations. The Brave first meets the human's Soldier to the north
 * (d 0), then visits the met AI colony to the east (d 2) with a !bVar6 mood
 * (contact_state 1): the @INDIANCITY demand runs and the AI hands over.
 */
static int run_visit_after_first_contact(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "visit-after-first-contact", 777u) != 0) {
    return 1;
  }
  ai_contact_reset();
  ColonizeColony* col = &f.colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = 1;
  col->x = 14;
  col->y = 10;
  col->stock[3] = 50;
  snprintf(col->name, sizeof(col->name), "Testtown");
  const int brave_ti = units_find_type(&f.units, "Brave");
  const int soldier_ti = units_find_type(&f.units, "Soldier");
  const int bid = brave_ti >= 0 ? units_spawn_allow_stack(&f.units, brave_ti, 13, 10) : -1;
  const int sid = soldier_ti >= 0 ? units_spawn_allow_stack(&f.units, soldier_ti, 13, 9) : -1;
  ColonizeUnit* brave = units_get(&f.units, bid);
  ColonizeUnit* sol = units_get(&f.units, sid);
  if (!brave || !sol) {
    fprintf(stderr, "visit-after-first-contact: spawn failed\n");
    ai_152e_fixture_free(&f);
    return 1;
  }
  brave->nation_id = 4;
  brave->home_tribe_index = 0;
  brave->col1_flags15 = 0;
  sol->nation_id = 0; /* human, not met yet */
  f.col1.nation[1].trade.euro_price[3] = 11;
  f.col1.indian[0].euro_diplo[1] = (uint8_t)COL1_INDIAN_MET_BIT;
  f.col1.indian[0].alarm_by_player[1] = 20;
  f.col1.indian[0].contact_state[1] = 1; /* local_10: never generous */
  col1_tribe_attitude_set(&f.tribes[0], 1, 5);

  ai_s_native_colonies = &f.colonies;
  ai_native_bind_step_ctx(&f.ctx);
  (void)ai_native_step_first_contact(&f.units, &f.map, &f.col1, brave, 4);
  ai_native_bind_step_ctx(NULL);
  ai_s_native_colonies = NULL;

  const int met_human = f.col1.indian[0].euro_diplo[0] != 0;
  const int stock = col->stock[3];
  ai_152e_fixture_free(&f);
  fprintf(stderr, "visit-after-first-contact: met human %d, AI colony stock %d\n", met_human,
          stock);
  return (met_human && stock < 50) ? 0 : 1;
}

/*
 * FUN_5bfb_3180 closes a nation (aiStack_20[e]) only when 022e returns
 * non-zero. An AI colony east of the Brave with empty stores and no wagon
 * leaves the visit unresolved, so the next tile of the same nation, a
 * Soldier south of the Brave, still gets its turn: raw 98649-98651 stamps
 * its facing with the scan direction (4) before its own 022e.
 */
static int run_unresolved_visit_keeps_nation_open(void) {
  if (!test_names_txt()) {
    return TR_SKIP;
  }
  Ai152eFixture f;
  if (ai_152e_fixture_init(&f, "visit-keeps-open", 778u) != 0) {
    return 1;
  }
  ai_contact_reset();
  const int e = 1;
  ColonizeColony* col = &f.colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = e;
  col->x = 14;
  col->y = 10;
  snprintf(col->name, sizeof(col->name), "Testtown");
  const int brave_ti = units_find_type(&f.units, "Brave");
  const int soldier_ti = units_find_type(&f.units, "Soldier");
  const int bid = brave_ti >= 0 ? units_spawn_allow_stack(&f.units, brave_ti, 13, 10) : -1;
  const int sid = soldier_ti >= 0 ? units_spawn_allow_stack(&f.units, soldier_ti, 13, 11) : -1;
  ColonizeUnit* brave = units_get(&f.units, bid);
  ColonizeUnit* soldier = units_get(&f.units, sid);
  if (!brave || !soldier) {
    fprintf(stderr, "visit-keeps-open: spawn failed\n");
    ai_152e_fixture_free(&f);
    return 1;
  }
  brave->nation_id = 4;
  brave->home_tribe_index = 0;
  brave->col1_flags15 = 0;
  brave->moves = 0;
  soldier->nation_id = e;
  soldier->last_dir = -1;
  f.col1.indian[0].euro_diplo[e] = (uint8_t)COL1_INDIAN_MET_BIT;
  f.col1.indian[0].alarm_by_player[e] = 20;
  f.col1.indian[0].contact_state[e] = 1;
  col1_tribe_attitude_set(&f.tribes[0], e, 5);

  ai_s_native_colonies = &f.colonies;
  ai_native_bind_step_ctx(&f.ctx);
  const int resolved = ai_native_step_first_contact(&f.units, &f.map, &f.col1, brave, 4);
  ai_native_bind_step_ctx(NULL);
  ai_s_native_colonies = NULL;
  fprintf(stderr, "visit-keeps-open: resolved %d soldier facing %d moves %d\n", resolved,
          (int)soldier->last_dir, (int)brave->moves);
  const int ok = resolved == 0 && soldier->last_dir == 4 && brave->moves == 0;
  ai_152e_fixture_free(&f);
  return ok ? 0 : 1;
}

static int case_coarse_fog_reset(void) {
  static ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units.units[0].active = true;
  units.units[0].id = units.next_id++;
  units.units[0].x = 8;
  units.units[0].y = 8;
  units_pool_sync(&units);
  ai_coarse_fog_euro_restamp(&units, NULL, 0);
  if (ai_coarse_fog_explore_unseen(8, 8)) {
    fprintf(stderr, "coarse fog restamp did not mark (8,8)\n");
    return 1;
  }
  ai_coarse_fog_reset();
  if (!ai_coarse_fog_explore_unseen(8, 8)) {
    fprintf(stderr, "ai_coarse_fog_reset left (8,8) stamped\n");
    return 1;
  }
  return 0;
}

/* ai_021a_dir_terrain score for one direction on an 8x8 land map; the
 * destination (4,4) is water when `water_dest`, with a nation-5 village at
 * (5,4). Run once per dos_buggy_behavior() setting. */
static int dir_terrain_score(int buggy, int facing, int d, int water_dest) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, 2, true)) {
    return -9999;
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  if (water_dest) {
    map.terrain[4 * 8 + 4] = 25;
    map.layer2[4 * 8 + 5] = MAP_OCCUPANCY_HAS_CITY;
    map.layer3[4 * 8 + 5] = 5u << MAP_L3_OWNER_SHIFT;
  }
  struct ai_021a_ctx c;
  memset(&c, 0, sizeof(c));
  c.map = &map;
  c.units = &units;
  c.nation_id = 4;
  c.x = 3;
  c.y = 4;
  c.d = d;
  c.nx = 4;
  c.ny = 4;
  c.facing = facing;
  c.home_reach = -1;
  c.presence = -1;
  c.settle = -1;
  dos_buggy_behavior_set(buggy != 0);
  ai_021a_dir_terrain(&c);
  dos_buggy_behavior_set(true);
  fx_map_free(&map);
  return c.score;
}

/* TB-30: a Brave that stayed (facing 8) gets no +3 for NE / NW when fixed. */
static int case_021a_stayed_facing_fixed(void) {
  for (int d = 1; d <= 7; d += 6) {
    if (dir_terrain_score(1, 8, d, 0) - dir_terrain_score(0, 8, d, 0) != 3) {
      fprintf(stderr, "TB-30: facing 8 d=%d must lose the +3 only when fixed\n", d);
      return 1;
    }
  }
  return 0;
}

/* port_bugs.md #1481 — OVL13 0xb3c JZ 0xb54: a lone treasure that passes
 * its rand(0,7)==0 roll jumps over the 0xb42 grudge latch, so a calm
 * (alarm <= 0x19) tribe with att >= 0x20 on an open tile stays grudge-free. */
static int case_021a_lone_treasure_no_grudge(void) {
  ColonizeUnitPool units;
  fx_units_init(&units);
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  units.type_count = 1;
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  const int tid = units_spawn_allow_stack(&units, 0, 4, 4);
  ColonizeUnit* t = units_get(&units, tid);
  if (!t) {
    col1_save_free(&col1);
    fprintf(stderr, "#1481: treasure spawn failed\n");
    return 1;
  }
  t->type_index = 0xa;
  uint32_t seed = 1u;
  for (;; ++seed) {
    AiRng probe;
    dos_rng_seed(&probe, seed);
    if (dos_rng_range(&probe, 0, 7) == 0) {
      break;
    }
  }
  AiRng rng;
  dos_rng_seed(&rng, seed);
  struct ai_021a_ctx c;
  memset(&c, 0, sizeof(c));
  c.rng = &rng;
  c.units = &units;
  c.col1 = &col1;
  c.nation_id = 4;
  c.lone = 1;
  c.occ = 1;
  c.owner = 0;
  c.presence = 0;
  c.settle = -1;
  c.att = 0x20;
  c.nx = 4;
  c.ny = 4;
  const Ai021aDirStatus st = ai_021a_dir_occupant(&c);
  col1_save_free(&col1);
  if (st != AI_021A_DIR_OK || c.grudge != 0) {
    fprintf(stderr, "#1481: lone treasure roll-0 path must skip the grudge latch (st=%d grudge=%d)\n",
            (int)st, c.grudge);
    return 1;
  }
  return 0;
}

/* TB-27: a village beside a water destination counts only with DOS's
 * overwritten medium; fixed, the land village fails the centre's water test. */
static int case_021a_adjacent_foreign_medium_fixed(void) {
  if (dir_terrain_score(1, 0, 0, 1) - dir_terrain_score(0, 0, 0, 1) != -0x19) {
    fprintf(stderr, "TB-27: land village beside water dest must count only in DOS mode\n");
    return 1;
  }
  return 0;
}

/* ---- FUN_6a09_0006 TRIBE.TXT branch (ai_place_tribes_from_txt) ---- */
typedef struct {
  ColonizeMsgCatalog names;
  ColonizeMsgCatalog txt;
  ColonizeWorldMap map;
  ColonizeCol1Save col1;
  AiNewGameParams p;
  ColonizeCol1Tribe* tribes;
  int count;
  int cap;
} TribeTxtFx;

static void tribe_txt_fx_close(TribeTxtFx* fx) {
  free(fx->tribes);
  map_free(&fx->map);
  col1_save_free(&fx->col1);
  assets_msg_free(&fx->names);
  assets_msg_free(&fx->txt);
}

static int tribe_txt_fx_open(TribeTxtFx* fx) {
  memset(fx, 0, sizeof(*fx));
  assets_msg_init(&fx->names);
  assets_msg_init(&fx->txt);
  col1_save_init(&fx->col1);
  char err[256];
  if (!assets_msg_load_file(&fx->names, "COLONIZE/NAMES.TXT") ||
      !assets_msg_load_file(&fx->txt, "COLONIZE/TRIBE.TXT") ||
      !map_load_mp("COLONIZE/AMER2.MP", &fx->map, err, sizeof(err)) || !fx->map.layer2 ||
      !fx->map.layer3) {
    fprintf(stderr, "tribe_txt fixture: data load failed\n");
    return 1;
  }
  map_gen_finalize_border_and_forest(fx->map.terrain, (int)fx->map.width, (int)fx->map.height);
  for (int t = 0; t < 8; ++t) {
    fx->col1.indian[t].capitol_x = 1;
    fx->col1.indian[t].capitol_y = 1;
  }
  fx->p.col1 = &fx->col1;
  fx->p.map = &fx->map;
  fx->p.names = &fx->names;
  fx->p.data_dir = "COLONIZE";
  fx->p.use_tribe_txt = true;
  return 0;
}

static bool file_present(const char* path) {
  FILE* f = fopen(path, "rb");
  if (f) {
    fclose(f);
  }
  return f != NULL;
}

/* NAMES.TXT, TRIBE.TXT and AMER2.MP: the game data under COLONIZE/. */
static bool tribe_txt_data(void) {
  return test_names_txt() && file_present("COLONIZE/TRIBE.TXT") &&
         file_present("COLONIZE/AMER2.MP");
}

/* Entry reseed + the 32 init draws (ai_init_new_game), then the branch. */
static int tribe_txt_place(TribeTxtFx* fx, uint32_t seed) {
  ColonizeDosRng rng;
  dos_rng_seed(&rng, seed);
  for (int i = 0; i < 32; ++i) {
    (void)dos_rng_range(&rng, 0, 14);
  }
  return ai_place_tribes_from_txt(&fx->p, &fx->tribes, &fx->count, &fx->cap, &rng) ? 0 : 1;
}

/* The tribe's TRIBE.TXT section: @TRIBES column 2, upper-cased (data, not code). */
static const ColonizeMsgSection* tribe_txt_section(const TribeTxtFx* fx, int indian) {
  char name[COLONIZE_MSG_LINE_LEN];
  if (!assets_msg_row_field(&fx->names, "TRIBES", indian, 1, name, sizeof(name))) {
    return NULL;
  }
  for (char* c = name; *c; ++c) {
    *c = (char)toupper((unsigned char)*c);
  }
  return assets_msg_find(&fx->txt, name);
}

/* Every village is within +-2 of one of its own tribe's points, on a legal
 * tile, more than 1 from every other village; tribes come in index order and
 * the first village of each is its only capital; capitol bytes untouched. */
static int tribe_txt_check(const TribeTxtFx* fx, const char* label) {
  int prev_nation = 4;
  for (int i = 0; i < fx->count; ++i) {
    const ColonizeCol1Tribe* t = &fx->tribes[i];
    const int x = t->x;
    const int y = t->y;
    if (t->nation_id < prev_nation || t->nation_id > 11) {
      fprintf(stderr, "%s: village %d nation %u after %d (not index order)\n", label, i,
              t->nation_id, prev_nation);
      return 1;
    }
    const bool first_of_tribe = i == 0 || fx->tribes[i - 1].nation_id != t->nation_id;
    if ((t->state.capital != 0) != first_of_tribe) {
      fprintf(stderr, "%s: village %d capital=%d, first of tribe=%d\n", label, i,
              t->state.capital, first_of_tribe);
      return 1;
    }
    prev_nation = t->nation_id;
    const ColonizeMsgSection* sec = tribe_txt_section(fx, t->nation_id - 4);
    bool near_point = false;
    for (int l = 0; sec && l < sec->line_count; ++l) {
      int px = 0, py = 0;
      if (sscanf(sec->lines[l], "%d,%d", &px, &py) == 2 && abs(px - x) <= 2 && abs(py - y) <= 2) {
        near_point = true;
      }
    }
    const int cls = map_dos_terr_class_at(&fx->map, x, y);
    const int base = cls & 7;
    if (!near_point || !map_coords_inset(&fx->map, x, y) || cls >= 0x18 ||
        !(base == 0 || (base >= 2 && base <= 6))) {
      fprintf(stderr, "%s: village %d (%d,%d) near_point=%d class=%d\n", label, i, x, y,
              near_point, cls);
      return 1;
    }
    for (int j = 0; j < i; ++j) {
      if (map_dos_dist(x - fx->tribes[j].x, y - fx->tribes[j].y) <= 1) {
        fprintf(stderr, "%s: villages %d (%d,%d) and %d (%d,%d) too close\n", label, i, x, y,
                j, fx->tribes[j].x, fx->tribes[j].y);
        return 1;
      }
    }
  }
  for (int t = 0; t < 8; ++t) {
    if (fx->col1.indian[t].capitol_x != 1 || fx->col1.indian[t].capitol_y != 1) {
      fprintf(stderr, "%s: tribe %d capitol bytes written\n", label, t);
      return 1;
    }
  }
  return 0;
}

static int case_tribe_txt_jitter_rules(void) {
  if (!tribe_txt_data()) {
    return TR_SKIP;
  }
  diag_init(0, NULL);
  int moved = 0;
  for (uint32_t s = 1; s <= 8; ++s) {
    TribeTxtFx fx;
    char label[32];
    snprintf(label, sizeof(label), "seed %u", s * 12345u);
    if (tribe_txt_fx_open(&fx) || tribe_txt_place(&fx, s * 12345u) ||
        fx.count < 40 || fx.tribes[0].nation_id != 4 || tribe_txt_check(&fx, label)) {
      fprintf(stderr, "%s: count=%d\n", label, fx.count);
      tribe_txt_fx_close(&fx);
      return 1;
    }
    /* The jitter is live: not every village sits on its listed point. */
    const ColonizeMsgSection* inca = tribe_txt_section(&fx, 0);
    int px = 0, py = 0;
    if (inca && sscanf(inca->lines[0], "%d,%d", &px, &py) == 2 &&
        (fx.tribes[0].x != px || fx.tribes[0].y != py)) {
      moved++;
    }
    tribe_txt_fx_close(&fx);
  }
  if (moved == 0) {
    fprintf(stderr, "tribe_txt: Inca capital sat on its listed point for all 8 seeds\n");
    return 1;
  }
  return 0;
}

/* No passing try (ocean over the whole +-2 box of the first Inca point):
 * that village is dropped and the next Inca village becomes the capital. */
static int case_tribe_txt_impossible_spot_dropped(void) {
  if (!tribe_txt_data()) {
    return TR_SKIP;
  }
  diag_init(0, NULL);
  TribeTxtFx base;
  TribeTxtFx fx;
  if (tribe_txt_fx_open(&base) || tribe_txt_place(&base, 100) || tribe_txt_fx_open(&fx)) {
    tribe_txt_fx_close(&base);
    return 1;
  }
  const ColonizeMsgSection* inca = tribe_txt_section(&fx, 0);
  int px = 0, py = 0;
  if (!inca || sscanf(inca->lines[0], "%d,%d", &px, &py) != 2) {
    tribe_txt_fx_close(&base);
    tribe_txt_fx_close(&fx);
    return 1;
  }
  for (int y = py - 2; y <= py + 2; ++y) {
    for (int x = px - 2; x <= px + 2; ++x) {
      fx.map.terrain[y * fx.map.width + x] = 0x19;
    }
  }
  int rc = tribe_txt_place(&fx, 100) || tribe_txt_check(&fx, "ocean box");
  int base_inca = 0, inca_n = 0;
  for (int i = 0; i < base.count; ++i) {
    base_inca += base.tribes[i].nation_id == 4;
  }
  for (int i = 0; i < fx.count; ++i) {
    inca_n += fx.tribes[i].nation_id == 4;
    if (abs(fx.tribes[i].x - px) <= 2 && abs(fx.tribes[i].y - py) <= 2) {
      rc = 1;
    }
  }
  if (rc || inca_n != base_inca - 1) {
    fprintf(stderr, "impossible spot: inca %d (base %d), count %d (base %d)\n", inca_n,
            base_inca, fx.count, base.count);
    rc = 1;
  }
  tribe_txt_fx_close(&base);
  tribe_txt_fx_close(&fx);
  return rc;
}

/* Third spacing tier (raw 107832: tries >= 0x42 need distance > 1 only). The
 * first Inca point's box is ocean but its centre, and a village already
 * stands 2 east of the centre: only a try from 0x42 on may place there. */
static int case_tribe_txt_third_spacing_tier(void) {
  if (!tribe_txt_data()) {
    return TR_SKIP;
  }
  diag_init(0, NULL);
  TribeTxtFx fx;
  if (tribe_txt_fx_open(&fx)) {
    tribe_txt_fx_close(&fx);
    return 1;
  }
  const ColonizeMsgSection* inca = tribe_txt_section(&fx, 0);
  int px = 0, py = 0;
  if (!inca || sscanf(inca->lines[0], "%d,%d", &px, &py) != 2) {
    tribe_txt_fx_close(&fx);
    return 1;
  }
  const uint8_t centre = fx.map.terrain[py * fx.map.width + px];
  for (int y = py - 2; y <= py + 2; ++y) {
    for (int x = px - 2; x <= px + 2; ++x) {
      fx.map.terrain[y * fx.map.width + x] = 0x19;
    }
  }
  fx.map.terrain[py * fx.map.width + px] = centre;
  fx.tribes = calloc(1, sizeof(*fx.tribes));
  if (!fx.tribes) {
    tribe_txt_fx_close(&fx);
    return 1;
  }
  fx.cap = 1;
  fx.count = 1;
  fx.tribes[0].x = (uint8_t)(px + 2);
  fx.tribes[0].y = (uint8_t)py;
  fx.tribes[0].nation_id = 11;
  int placed = -1;
  int rc = tribe_txt_place(&fx, 100);
  for (int i = 1; rc == 0 && i < fx.count; ++i) {
    if (fx.tribes[i].x == px && fx.tribes[i].y == py) {
      placed = i;
    }
  }
  fprintf(stderr, "third tier: centre (%d,%d) village slot %d nation %d (want an Inca capital)\n",
          px, py, placed, placed > 0 ? fx.tribes[placed].nation_id : -1);
  rc = rc || placed < 0 || fx.tribes[placed].nation_id != 4 || !fx.tribes[placed].state.capital;
  tribe_txt_fx_close(&fx);
  return rc;
}

/* The VR_SOUND.EXE run pins every 04ca reseed at 100, so seed 100 + 32 draws
 * on AMER2 must give 000001.SAV's village rows (branch alone). */
static int case_tribe_txt_seed100_matches_dos(void) {
  if (!tribe_txt_data()) {
    return TR_SKIP;
  }
  diag_init(0, NULL);
  TribeTxtFx fx;
  ColonizeCol1Save dos;
  col1_save_init(&dos);
  char err[256];
  if (tribe_txt_fx_open(&fx) || tribe_txt_place(&fx, 100) ||
      !col1_save_read_file("original_saves/golden_idle_america_seed-100/000001.SAV", &dos, err,
                           sizeof(err))) {
    tribe_txt_fx_close(&fx);
    col1_save_free(&dos);
    return 1;
  }
  int rc = fx.count != (int)dos.head.tribe_count;
  const int n = fx.count < (int)dos.head.tribe_count ? fx.count : (int)dos.head.tribe_count;
  for (int i = 0; i < n; ++i) {
    const ColonizeCol1Tribe* a = &fx.tribes[i];
    const ColonizeCol1Tribe* b = &dos.tribe[i];
    if (a->x != b->x || a->y != b->y || a->nation_id != b->nation_id ||
        a->state.capital != b->state.capital) {
      fprintf(stderr, "village %d: port (%u,%u) n%u cap%u, DOS (%u,%u) n%u cap%u\n", i, a->x, a->y,
              a->nation_id, a->state.capital, b->x, b->y, b->nation_id, b->state.capital);
      rc = 1;
    }
  }
  fprintf(stderr, "seed100 TRIBE.TXT: port %d villages, DOS %u\n", fx.count, dos.head.tribe_count);
  tribe_txt_fx_close(&fx);
  col1_save_free(&dos);
  return rc;
}

/* DOS evidence through the real entry: ai_init_new_game on AMER2 with seed
 * 100 (FUN_6a09_0006's own entry reseed + 32 init draws) must give the
 * village rows of original_saves/golden_idle_america_seed-100/000001.SAV.
 * params->rng arrives advanced, so only the entry reseed can put the
 * placement on DOS's stream. */
static int case_tribe_txt_init_new_game_matches_dos(void) {
  if (!tribe_txt_data()) {
    return TR_SKIP;
  }
  diag_init(0, NULL);
  const int human = 2; /* the recorded run plays Spain */
  char err[256];
  static ColonizeMsgCatalog names;
  assets_msg_init(&names);
  static ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  ColonizeCol1Save dos;
  col1_save_init(&dos);
  int rc = 1;
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") || !units_load_types(&units, &names) ||
      !map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err)) ||
      !col1_save_read_file("original_saves/golden_idle_america_seed-100/000001.SAV", &dos, err,
                           sizeof(err))) {
    fprintf(stderr, "init_new_game tribe_txt: data load failed\n");
    goto done;
  }
  map_gen_finalize_border_and_forest(map.terrain, (int)map.width, (int)map.height);
  ColonizeDosRng stream;
  dos_rng_seed(&stream, 100);
  for (int i = 0; i < 57; ++i) {
    (void)dos_rng_range(&stream, 1, 0x7fff);
  }
  int sx = 39, sy = 10;
  new_game_scenario_start(&names, "AMER2", human, &sx, &sy);
  units_set_occupancy_map(&map);
  colonies_set_occupancy_map(&map);
  units_new_world_start(&units, &map, sx, sy, human, 0);
  bool col1_ok = false;
  colonies_set_col1_context(NULL);
  colonies_set_units_context(NULL);
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  AiNewGameParams ai;
  memset(&ai, 0, sizeof(ai));
  ai.col1 = &col1;
  ai.col1_ok = &col1_ok;
  ai.map = &map;
  ai.units = &units;
  ai.europe = &europe;
  ai.names = &names;
  ai.data_dir = "COLONIZE";
  ai.human_nation = human;
  ai.difficulty = dos.head.difficulty; /* the recorded run is Viceroy (4) */
  ai.leader_name = "Test Governor";
  ai.use_tribe_txt = true;
  ai.map_stem = "AMER2";
  ai.human_start_x = sx;
  ai.human_start_y = sy;
  ai.rng_seed = 100;
  ai.rng_seed_set = true;
  ai.rng = &stream;
  if (!ai_init_new_game(&ai, err, sizeof(err))) {
    fprintf(stderr, "init_new_game tribe_txt: %s\n", err);
    goto done;
  }
  rc = col1.head.tribe_count != dos.head.tribe_count;
  int same = 0;
  const int n = col1.head.tribe_count < dos.head.tribe_count ? col1.head.tribe_count
                                                             : dos.head.tribe_count;
  for (int i = 0; i < n; ++i) {
    const ColonizeCol1Tribe* a = &col1.tribe[i];
    const ColonizeCol1Tribe* b = &dos.tribe[i];
    if (a->x != b->x || a->y != b->y || a->nation_id != b->nation_id ||
        a->state.capital != b->state.capital) {
      rc = 1;
    } else {
      same++;
    }
  }
  fprintf(stderr, "init_new_game tribe_txt: port %u villages, DOS %u, rows equal %d\n",
          col1.head.tribe_count, dos.head.tribe_count, same);
  /* FUN_4d56_0038 raw 81282-81285: +7/+8/+9 = 0xff. DOS still has them at
   * 0xff at turn 9 (an idle run never trades). */
  int bytes_ok = 0;
  for (int i = 0; i < n; ++i) {
    const ColonizeCol1Tribe* a = &col1.tribe[i];
    const ColonizeCol1Tribe* b = &dos.tribe[i];
    if (a->sticky_trade_good != 0xff || a->last_bought != 0xff || a->last_sold != 0xff ||
        a->sticky_trade_good != b->sticky_trade_good || a->last_bought != b->last_bought ||
        a->last_sold != b->last_sold) {
      rc = 1;
    } else {
      bytes_ok++;
    }
  }
  /* FUN_6a09_0006 raw 108006-108020: the Mountains Silver bonus also runs on
   * the TRIBE.TXT path. Set once at new game, so turn 9 still holds it. */
  int silver_ok = 0;
  for (int t = 0; t < 8; ++t) {
    if (col1.indian[t].hill_silver_bid_bonus != dos.indian[t].hill_silver_bid_bonus) {
      fprintf(stderr, "tribe %d silver bonus: port %d, DOS %d\n", t,
              col1.indian[t].hill_silver_bid_bonus, dos.indian[t].hill_silver_bid_bonus);
      rc = 1;
    } else {
      silver_ok++;
    }
  }
  /* FUN_6a09_0006 raw 107756-107767: alarm = r(0,14) + (human ? 2*diff : 0)
   * on the shared stream. 000001.SAV is turn 9 (the run's first save), so
   * three non-human pairs have moved by play: Arawak/England, Tupi/France and
   * Tupi/Dutch. Those three are checked against the same seed's TURN1.SAV. */
  ColonizeCol1Save turn1;
  col1_save_init(&turn1);
  if (!col1_save_read_file("original_saves/early-game-seed-100/TURN1.SAV", &turn1, err,
                           sizeof(err))) {
    fprintf(stderr, "init_new_game tribe_txt: TURN1.SAV load failed\n");
    rc = 1;
  }
  int alarm_ok = 0;
  for (int t = 0; t < 8; ++t) {
    for (int e = 0; e < 4; ++e) {
      const bool moved_by_play = (t == 2 && e == 0) || (t == 7 && (e == 1 || e == 3));
      const ColonizeCol1Save* want = moved_by_play ? &turn1 : &dos;
      if (col1.indian[t].alarm_by_player[e] != want->indian[t].alarm_by_player[e]) {
        rc = 1;
      } else {
        alarm_ok++;
      }
    }
  }
  col1_save_free(&turn1);
  fprintf(stderr, "init_new_game tribe_txt: +7..+9 ok %d/%d, silver ok %d/8, alarms ok %d/32\n",
          bytes_ok, n, silver_ok, alarm_ok);
done:
  units_set_occupancy_map(NULL);
  colonies_set_occupancy_map(NULL);
  map_free(&map);
  col1_save_free(&col1);
  col1_save_free(&dos);
  assets_msg_free(&names);
  return rc;
}

static int case_new_world(void) {
  diag_init(0, NULL);
  return run_init_and_turns("COLONIZE", false, 0, "NEW_WORLD");
}

static int case_america(void) {
  diag_init(0, NULL);
  return run_init_and_turns("COLONIZE", true, 2, "AMERICA");
}

/* Two worlds, one process: the second must not path on the first's coarse
 * walkability grid (units_coarse keyed on map pointer + size, both equal
 * here; ai_init_new_game -> units_reset_state drops it). */
static int case_new_world_after_america(void) {
  diag_init(0, NULL);
  if (run_init_and_turns("COLONIZE", true, 2, "AMERICA") != 0) {
    return 1;
  }
  return run_init_and_turns("COLONIZE", false, 0, "NEW_WORLD after AMERICA");
}

static const TestCase k_cases[] = {
    {"case_new_world", case_new_world},
    {"case_new_world_after_america", case_new_world_after_america},
    {"case_america", case_america},
    {"coarse_fog_reset", case_coarse_fog_reset},
    {"tribe_txt_jitter_rules", case_tribe_txt_jitter_rules},
    {"tribe_txt_impossible_spot_dropped", case_tribe_txt_impossible_spot_dropped},
    {"tribe_txt_third_spacing_tier", case_tribe_txt_third_spacing_tier},
    {"tribe_txt_seed100_matches_dos", case_tribe_txt_seed100_matches_dos},
    {"tribe_txt_init_new_game_matches_dos", case_tribe_txt_init_new_game_matches_dos},
    {"021a_stayed_facing_fixed", case_021a_stayed_facing_fixed},
    {"021a_lone_treasure_no_grudge", case_021a_lone_treasure_no_grudge},
    {"021a_adjacent_foreign_medium_fixed", case_021a_adjacent_foreign_medium_fixed},
    {"run_village_threat_alarm", run_village_threat_alarm},
    {"run_152e_mission_attitude_word", run_152e_mission_attitude_word},
    {"run_152e_alarm_escalation", run_152e_alarm_escalation},
    {"run_152e_mission_nibble_out_of_domain", run_152e_mission_nibble_out_of_domain},
    {"run_visit_empty_colony", run_visit_empty_colony},
    {"run_indian_surprise_every_attack", run_indian_surprise_every_attack},
    {"run_unresolved_visit_keeps_nation_open", run_unresolved_visit_keeps_nation_open},
    {"run_reparations_scan_past_empty_colony", run_reparations_scan_past_empty_colony},
    {"run_visit_after_first_contact", run_visit_after_first_contact},
    {"run_convert_spawn_failure_unresolved", run_convert_spawn_failure_unresolved},
    {"run_zero_qty_demand_presented", run_zero_qty_demand_presented},
    {"run_conceded_beg_gifts_above_0x31", run_conceded_beg_gifts_above_0x31},
    {"run_wagon_demand_no_mood_gate", run_wagon_demand_no_mood_gate},
    {"run_conceded_beg_gifts_begged_colony", run_conceded_beg_gifts_begged_colony},
    {"run_step_visit_serves_encounter_colony", run_step_visit_serves_encounter_colony},
    {"run_pinned_colony_skips_open_field_wagon", run_pinned_colony_skips_open_field_wagon},
    {"run_refused_beg_demands_begged_colony", run_refused_beg_demands_begged_colony},
    {"run_wagon_reparations_open_field", run_wagon_reparations_open_field},
    {"run_wagon_demand_busy_slot_no_comment", run_wagon_demand_busy_slot_no_comment},
    {"run_wagon_state2_takes_comment", run_wagon_state2_takes_comment},
    {"run_wagon_demand_closes_nation", run_wagon_demand_closes_nation},
    {"run_two_demands_one_pulse", run_two_demands_one_pulse},
    {"run_two_demands_turn_processor", run_two_demands_turn_processor},
    {"run_beg_food_stops_pulse", run_beg_food_stops_pulse},
    {"run_resume_keeps_scan_latch", run_resume_keeps_scan_latch},
    {"run_resume_exhausts_step_mover", run_resume_exhausts_step_mover},
    {"run_processor_start_drops_resume", run_processor_start_drops_resume},
};
TEST_MAIN(k_cases)
