#include <stdio.h>
#include <string.h>

#include "core/assets.h"
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

  units_new_world_start(&pool, &map);
  if (pool.unit_count != 1 || pool.selected_id < 0) {
    fprintf(stderr, "expected starter pioneer on map\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  ColonizeUnit* starter = units_get(&pool, pool.selected_id);
  if (!starter || !map_tile_is_land(&map, starter->x, starter->y)) {
    fprintf(stderr, "starter not on land\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  const int land_x = starter->x;
  const int land_y = starter->y;
  int ocean_x = -1;
  int ocean_y = -1;
  for (int y = 0; y < map.height && ocean_x < 0; ++y) {
    for (int x = 0; x < map.width; ++x) {
      if (map_tile_is_water(&map, x, y)) {
        ocean_x = x;
        ocean_y = y;
        break;
      }
    }
  }
  if (ocean_x < 0) {
    fprintf(stderr, "no ocean tile found\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (units_try_move(&pool, starter->id, &map, ocean_x, ocean_y)) {
    fprintf(stderr, "land unit should not enter ocean\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (land_x + 1 < map.width &&
      units_try_move(&pool, starter->id, &map, land_x + 1, land_y)) {
    if (starter->x != land_x + 1) {
      fprintf(stderr, "move did not update position\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }

  const int colonist_id = units_spawn(&pool, colonist, starter->x, starter->y);
  if (colonist_id >= 0) {
    fprintf(stderr, "should not stack two units on one tile\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
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
  if (icon < 0 || icon >= icons.sprite_count || icons.sprites[icon].width <= 0) {
    fprintf(
      stderr,
      "pioneer icon %d invalid (sprites=%d)\n",
      icon,
      icons.sprite_count
    );
    ss_free(&icons);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  fprintf(
    stderr,
    "units tests ok (types=%d pioneer@%d,%d icon=%dx%d)\n",
    pool.type_count,
    starter->x,
    starter->y,
    icons.sprites[icon].width,
    icons.sprites[icon].height
  );

  ss_free(&icons);
  map_free(&map);
  assets_msg_free(&names);
  diag_shutdown();
  return 0;
}
