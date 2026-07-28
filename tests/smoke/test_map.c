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

  const uint8_t ocean = map_get_terrain(&map, 0, 0);
  if ((ocean & 0x1f) != 25 || map_terrain_base_sprite(ocean) != 10) {
    fprintf(stderr, "ocean tile expected index 25 sprite 10, got 0x%02x sprite %d\n",
      ocean, map_terrain_base_sprite(ocean));
    map_free(&map);
    return 1;
  }

  if (map_phys0_overlay_count(&map, 0, 0) != 0) {
    fprintf(stderr, "plain ocean tile should have no phys0 overlay\n");
    map_free(&map);
    return 1;
  }

  if (map_phys0_overlay_count(&map, 1, 1) != 1 ||
      map_phys0_overlay_sprite(&map, 1, 1) != 48) {
    fprintf(stderr, "isolated hill expected phys0 sprite 48, got count=%d sprite %d\n",
      map_phys0_overlay_count(&map, 1, 1),
      map_phys0_overlay_sprite(&map, 1, 1));
    map_free(&map);
    return 1;
  }

  if (map_phys0_overlay_count(&map, 9, 26) != 1 ||
      map_phys0_overlay_sprite(&map, 9, 26) != 48) {
    fprintf(stderr, "desert hill at (9,26) expected sprite 48, got count=%d sprite %d\n",
      map_phys0_overlay_count(&map, 9, 26),
      map_phys0_overlay_sprite(&map, 9, 26));
    map_free(&map);
    return 1;
  }

  if (map_phys0_overlay_count(&map, 11, 23) != 1 ||
      map_phys0_overlay_sprite(&map, 11, 23) != 35) {
    fprintf(stderr, "mountain at (11,23) expected sprite 35, got count=%d sprite %d\n",
      map_phys0_overlay_count(&map, 11, 23),
      map_phys0_overlay_sprite(&map, 11, 23));
    map_free(&map);
    return 1;
  }

  if (map_phys0_overlay_count(&map, 16, 3) != 1 ||
      map_phys0_overlay_sprite(&map, 16, 3) != 23) {
    fprintf(stderr, "minor river at (16,3) expected sprite 23, got count=%d sprite %d\n",
      map_phys0_overlay_count(&map, 16, 3),
      map_phys0_overlay_sprite(&map, 16, 3));
    map_free(&map);
    return 1;
  }

  fprintf(stderr, "map tests ok sample ocean=0x%02x overlay=%u\n", ocean, map_terrain_overlay(ocean));

  map_free(&map);
  diag_shutdown();
  return 0;
}
