#include <stdio.h>

#include "core/map.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  ColonizeWorldMap map;
  char err[256];
  if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
    fprintf(stderr, "map load failed: %s\n", err);
    return 1;
  }

  if (map.width != 58 || map.height != 72 || map.tile_count != 58u * 72u) {
    fprintf(stderr, "unexpected map size %ux%u (%zu tiles)\n", map.width, map.height, map.tile_count);
    map_free(&map);
    return 1;
  }

  const uint8_t ocean = map_get_terrain(&map, 10, 10);
  const int sprite = map_terrain_sprite(ocean);
  fprintf(stderr, "sample tile (10,10)=0x%02x sprite=%d overlay=%u\n",
    ocean, sprite, map_terrain_overlay(ocean));

  map_free(&map);
  diag_shutdown();
  return 0;
}
