#include <stdio.h>
#include <string.h>

#include "core/ai.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
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

/* Place AI ship on water adjacent to landfall so one turn can unload+found. */
static bool place_ai_ship_for_settle(
  ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nation
) {
  ColonizeUnit* ship = NULL;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (units_is_sea(units, u->id) && u->cargo_count > 0) {
      ship = u;
      break;
    }
  }
  if (!ship) {
    return false;
  }
  int gx = ship->goto_x;
  int gy = ship->goto_y;
  if (gx < 0 || gy < 0 || gx >= 255 || gy >= 255 || gx >= (int)map->width ||
      gy >= (int)map->height) {
    /* Goto cleared / Europe sentinel — use current position as search center. */
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
  /* Prefer water beside foundable (non-arctic) land; scan full map if needed. */
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
  if (best_wx < 0) {
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
  /* Station-keep on coastal water so AI unload sees adjacent foundable land
   * (do not goto the land tile — advance_goto would burn the turn). */
  ship->orders = UNITS_ORDER_AI_SAIL;
  ship->goto_x = best_wx;
  ship->goto_y = best_wy;
  return true;
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
    map_gen_params_random(&params, 0xA1A1A1u);
    params.land_mass = 1;
    params.land_form = 1;
    params.temperature = 1;
    params.climate = 1;
    params.seed = 0xA1A1A1u;
    if (!map_generate(&map, &params, err, sizeof(err))) {
      fprintf(stderr, "%s: map_generate failed: %s\n", label, err);
      assets_msg_free(&names);
      return 1;
    }
  }

  int sx = 39, sy = 10;
  if (america) {
    new_game_scenario_start(&names, "AMER2", human_nation, &sx, &sy);
  } else {
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
  if (count_braves(&units) <= 0) {
    fprintf(stderr, "%s: expected Braves\n", label);
    map_free(&map);
    col1_save_free(&col1);
    assets_msg_free(&names);
    return 1;
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
      if (!place_ai_ship_for_settle(&units, &map, &colonies, rival)) {
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
      turn_refresh_moves_for_nation(&units, rival, NULL, NULL);
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
      const int cargo1 = count_nation_cargo(&units, rival);
      const int cols = count_nation_colonies(&colonies, rival);
      if (cargo1 >= cargo0) {
        fprintf(stderr, "%s: rival %d did not unload (cargo %d -> %d)\n", label, rival, cargo0, cargo1);
        map_free(&map);
        col1_save_free(&col1);
        assets_msg_free(&names);
        return 1;
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

int main(void) {
  diag_init(0, NULL);
  const char* data = "COLONIZE";
  if (run_init_and_turns(data, false, 0, "NEW_WORLD") != 0) {
    return 1;
  }
  if (run_init_and_turns(data, true, 2, "AMERICA") != 0) {
    return 1;
  }
  return 0;
}
