#include <stdio.h>
#include <string.h>

#include "core/map.h"
#include "platform/diagnostics.h"

#define MAP_FIXTURE_PHYS0_MAX 4

typedef struct MapTileExpectation {
  int x;
  int y;
  int terrain_sprite;
  int phys0_count; /* 0 = none; >0 = exact overlay list in phys0_sprites */
  int phys0_sprites[MAP_FIXTURE_PHYS0_MAX];
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
  const int overlay_count = map_phys0_overlay_count(map, expect->x, expect->y);
  int overlays[MAP_FIXTURE_PHYS0_MAX];
  int got_count = 0;
  if (forest_sprite >= 0) {
    overlays[got_count++] = forest_sprite;
  }
  for (int layer = 0; layer < overlay_count && got_count < MAP_FIXTURE_PHYS0_MAX; ++layer) {
    const int sprite = map_phys0_overlay_sprite_at(map, expect->x, expect->y, layer);
    if (sprite >= 0) {
      overlays[got_count++] = sprite;
    }
  }

  if (expect->phys0_count == 0) {
    if (got_count != 0) {
      snprintf(
        err,
        err_size,
        "(%d,%d) expected no phys0 overlay, got %d sprite(s) (first=%d)",
        expect->x,
        expect->y,
        got_count,
        overlays[0]
      );
      return 1;
    }
    return 0;
  }

  if (got_count != expect->phys0_count) {
    snprintf(
      err,
      err_size,
      "(%d,%d) phys0 count expected %d got %d (first got=%d)",
      expect->x,
      expect->y,
      expect->phys0_count,
      got_count,
      got_count > 0 ? overlays[0] : -1
    );
    return 1;
  }

  for (int i = 0; i < expect->phys0_count; ++i) {
    if (overlays[i] != expect->phys0_sprites[i]) {
      snprintf(
        err,
        err_size,
        "(%d,%d) phys0[%d] expected %d got %d",
        expect->x,
        expect->y,
        i,
        expect->phys0_sprites[i],
        overlays[i]
      );
      return 1;
    }
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
    {1, 1, 0, 1, {36}},
    {2, 11, 4, 1, {36}},
    {5, 21, 1, 1, {48}},
    {4, 20, 8, 0, {0}},
    {8, 14, 8, 0, {0}},
    {1, 0, 0, 1, {65}},
    {1, 2, 0, 1, {70}},
    {16, 2, 0, 1, {70}},
    {4, 18, 5, 1, {69}},
    {36, 4, 2, 1, {64}},
    {27, 14, 3, 1, {65}},
    {3, 3, 4, 1, {66}},
    {27, 20, 6, 1, {67}},
    {39, 28, 7, 1, {68}},
    {24, 19, 4, 1, {52}},
    {24, 20, 4, 1, {56}},
    /* 4-quadrant 8x8 coast system (sprites 108-139, groups of 8 per quadrant).
     * Each ocean tile gets up to 4 quadrant overlays (NW=108, NE=116, SW=124, SE=132).
     * 3-bit variant: bit0=first-cardinal-land, bit1=second-cardinal-land, bit2=diag-land. */
    /* (6,14): ocean with land on all 4 cardinal sides -> all quadrants fully set. */
    {6, 14, 10, 4, {115, 123, 131, 139}},
    /* (23,2): ocean with land only to the E -> NE+SW+SE quadrants. */
    {23, 2, 10, 3, {118, 125, 139}},
    /* (8,2): ocean with land only to the W -> NW+NE+SW+SE quadrants. */
    {8, 2, 10, 4, {114, 120, 131, 138}},
    /* (1,3): ocean with land only to the N -> NW+NE+SE quadrants. */
    {1, 3, 10, 3, {109, 123, 133}},
    /* (18,2): ocean with land only to the W+NW -> NW+NE+SW quadrants. */
    {18, 2, 10, 3, {115, 117, 130}},
    /* (33,6): ocean with diagonal land to SE -> all 4 quadrants. */
    {33, 6, 10, 4, {112, 118, 125, 135}},
    /* (9,25): ocean with diagonal land to SW -> all 4 quadrants. */
    {9, 25, 10, 4, {110, 120, 127, 134}},
    /* (8,26): ocean with diagonal land to NE -> NW+NE+SE quadrants. */
    {8, 26, 10, 3, {113, 119, 133}},
    /* (34,7): ocean with diagonal land to NW -> NW+NE+SW+SE quadrants. */
    {34, 7, 10, 4, {111, 121, 126, 136}},
    /* Regression anchors from earlier passes. */
    {9, 26, 1, 1, {48}},
    {16, 3, 0, 1, {23}},
  };

  for (size_t i = 0; i < sizeof(amer2_fixtures) / sizeof(amer2_fixtures[0]); ++i) {
    if (check_tile(&map, &amer2_fixtures[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "%s\n", err);
      map_free(&map);
      return 1;
    }
  }

  /* Scrub forest: only terrain indices 9 and 17 use TERRAIN sprite 8 (no PHYS0). */
  int scrub_sprite8_tiles = 0;
  for (int y = 0; y < (int)map.height; ++y) {
    for (int x = 0; x < (int)map.width; ++x) {
      const uint8_t byte = map_get_terrain(&map, x, y);
      const int terrain_index = (int)(byte & 0x1fu);
      const int terrain_sprite = map_terrain_sprite_at(&map, x, y);
      if (terrain_sprite != 8) {
        continue;
      }
      ++scrub_sprite8_tiles;
      if (terrain_index != 9 && terrain_index != 17) {
        fprintf(
          stderr,
          "scrub regression: (%d,%d) sprite 8 but terrain index %d (byte=0x%02x)\n",
          x,
          y,
          terrain_index,
          byte
        );
        map_free(&map);
        return 1;
      }
      if (map_phys0_forest_sprite_at(&map, x, y) >= 0) {
        fprintf(stderr, "scrub regression: (%d,%d) must not have PHYS0 forest overlay\n", x, y);
        map_free(&map);
        return 1;
      }
    }
  }
  if (scrub_sprite8_tiles != 81) {
    fprintf(stderr, "scrub regression: expected 81 TERRAIN-8 tiles on AMER2, got %d\n", scrub_sprite8_tiles);
    map_free(&map);
    return 1;
  }

  fprintf(stderr, "map tests ok (%zu amer2 fixtures, %d scrub tiles)\n",
    sizeof(amer2_fixtures) / sizeof(amer2_fixtures[0]), scrub_sprite8_tiles);

  map_free(&map);
  diag_shutdown();
  return 0;
}
