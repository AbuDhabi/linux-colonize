#include <stdio.h>

#include "core/map.h"
#include "platform/diagnostics.h"

typedef struct MapTileExpectation {
  int x;
  int y;
  int terrain_sprite;
  int phys0_sprite; /* -1 = no phys0 overlay expected */
} MapTileExpectation;

static int check_tile(
  const ColonizeWorldMap* map,
  const MapTileExpectation* expect,
  char* err,
  size_t err_size
) {
  const int terrain_sprite = map_terrain_sprite_at(map, expect->x, expect->y);
  if (terrain_sprite != expect->terrain_sprite) {
    snprintf(
      err,
      err_size,
      "(%d,%d) terrain sprite expected %d got %d (byte=0x%02x)",
      expect->x,
      expect->y,
      expect->terrain_sprite,
      terrain_sprite,
      map_get_terrain(map, expect->x, expect->y)
    );
    return 1;
  }

  const int forest_sprite = map_phys0_forest_sprite_at(map, expect->x, expect->y);
  const int feature_sprite = map_phys0_overlay_sprite(map, expect->x, expect->y);
  const int phys0_sprite = forest_sprite >= 0 ? forest_sprite : feature_sprite;

  if (expect->phys0_sprite < 0) {
    if (forest_sprite >= 0 || feature_sprite >= 0) {
      snprintf(
        err,
        err_size,
        "(%d,%d) expected no phys0 overlay, got forest=%d feature=%d",
        expect->x,
        expect->y,
        forest_sprite,
        feature_sprite
      );
      return 1;
    }
    return 0;
  }

  if (phys0_sprite != expect->phys0_sprite) {
    snprintf(
      err,
      err_size,
      "(%d,%d) phys0 sprite expected %d got %d (forest=%d feature=%d)",
      expect->x,
      expect->y,
      expect->phys0_sprite,
      phys0_sprite,
      forest_sprite,
      feature_sprite
    );
    return 1;
  }
  return 0;
}

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
  if ((ocean & 0x1f) != 25 || map_terrain_sprite_at(&map, 0, 0) != 10) {
    fprintf(stderr, "ocean tile expected index 25 sprite 10, got 0x%02x sprite %d\n",
      ocean, map_terrain_sprite_at(&map, 0, 0));
    map_free(&map);
    return 1;
  }

  static const MapTileExpectation amer2_fixtures[] = {
    /* User-reported fixtures for iterative map rendering. */
    {1, 1, 0, 36},
    {2, 11, 4, 36},
    {5, 21, 1, 48},
    {4, 20, 8, -1},
    {1, 0, 0, 65},
    {1, 2, 4, 40},
    {4, 18, 5, 99},
    {24, 19, 4, 52},
    {24, 20, 4, 56},
    /* Regression anchors from earlier passes. */
    {9, 26, 1, 48},
    {16, 3, 0, 23},
  };

  for (size_t i = 0; i < sizeof(amer2_fixtures) / sizeof(amer2_fixtures[0]); ++i) {
    if (check_tile(&map, &amer2_fixtures[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "%s\n", err);
      map_free(&map);
      return 1;
    }
  }

  fprintf(stderr, "map tests ok (%zu amer2 fixtures)\n", sizeof(amer2_fixtures) / sizeof(amer2_fixtures[0]));

  map_free(&map);
  diag_shutdown();
  return 0;
}
