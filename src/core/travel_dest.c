#include "core/travel_dest.h"

/* Region (layer3 low nibble) of a tile: FUN_281f_06b4 -> FUN_137f_01ca. */
static int travel_dest_region_at(const ColonizeWorldMap* map, int x, int y) {
  return (int)(map_get_layer3(map, x, y) & MAP_L3_CONTINENT_MASK);
}

bool travel_dest_who_unit(const ColonizeUnitPool* units, int unit_id, TravelDestWho* out) {
  const ColonizeUnit* u = units_get_const(units, unit_id);
  if (!u || !u->active || !out) {
    return false;
  }
  out->ship = units_is_sea(units, unit_id);
  out->x = u->x;
  out->y = u->y;
  return true;
}

/* FUN_15eb_00a2 raw 9340-9374; see the header. */
int travel_dest_water_region(const ColonizeWorldMap* map, int x, int y) {
  int best_rank = -1; /* local_a, seeded 0xffff */
  int best_region = -1;
  for (int d = 0; d < 8; ++d) {
    const int nx = x + MAP_DIR8_DX[d];
    const int ny = y + MAP_DIR8_DY[d];
    if (!map_coords_inset(map, nx, ny) || !map_tile_is_water(map, nx, ny)) {
      continue;
    }
    const int region = travel_dest_region_at(map, nx, ny);
    const int rank = region == 0 ? 0x10 : region; /* raw 9365 */
    if (best_rank < 0 || rank < best_rank) {
      best_rank = rank;
      best_region = region;
    }
  }
  return best_region;
}

/* FUN_647e_0094, asm 647e:0094-01c0. */
bool travel_dest_ok(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nation,
  int dest,
  const TravelDestWho* who
) {
  const ColonizeColony* c = NULL;
  if (dest != TRAVEL_DEST_EUROPE) {
    c = colonies_get(colonies, dest);
    if (!c || !c->active || c->nation_id != nation) {
      return false;
    }
  }
  if (!who) {
    return true;
  }
  if (!who->ship) {
    /* 647e:0186: both tiles' 06b4 must agree. DOS never asks the Europe row
     * for a land unit (its colony pointer would be stale). */
    return c && map_in_bounds(map, who->x, who->y) &&
           travel_dest_region_at(map, who->x, who->y) == travel_dest_region_at(map, c->x, c->y);
  }
  const int atlantic = travel_dest_region_at(map, (int)map->width - 2, 1); /* 647e:017c */
  int ship_region = atlantic;
  if (map_coords_inset(map, who->x, who->y)) { /* FUN_281f_0302 */
    ship_region = travel_dest_water_region(map, who->x, who->y);
    if (ship_region < 0) {
      ship_region = travel_dest_region_at(map, who->x, who->y);
    }
  }
  if (!c) {
    return ship_region == atlantic;
  }
  const int colony_region = travel_dest_water_region(map, c->x, c->y);
  return colony_region >= 0 && colony_region == ship_region;
}
