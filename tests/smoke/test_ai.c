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
  units_new_world_start(&units, &map, sx, sy, human_nation);

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
    if (count_nation_ships_on_map(&units, &map, n) < 1) {
      fprintf(stderr, "%s: missing on-map AI fleet for nation %d\n", label, n);
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

  const int dist0 = ai_ship_dist_sum(&units, human_nation == 1 ? 0 : 1);
  const int pop0 = tribe_pop_sum(&col1);
  const uint16_t crosses0 = col1.nation[human_nation == 1 ? 0 : 1].current_crosses;

  uint32_t turn_number = 0;
  uint16_t year = 1492;
  uint16_t autumn = 0;
  ColonizeColonyPool colonies;
  colonies_init(&colonies);

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

  const int dist1 = ai_ship_dist_sum(&units, human_nation == 1 ? 0 : 1);
  const int pop1 = tribe_pop_sum(&col1);
  const uint16_t crosses1 = col1.nation[human_nation == 1 ? 0 : 1].current_crosses;

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
