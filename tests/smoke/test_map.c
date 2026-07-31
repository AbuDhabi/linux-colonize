#include <stdio.h>
#include <string.h>

#include "core/map.h"
#include "platform/diagnostics.h"

#define MAP_FIXTURE_PHYS0_MAX 8

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
    {43, 68, 0, 1, {36}},
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
    /* Regression anchors from earlier passes. */
    {9, 26, 1, 1, {48}},
    {16, 3, 0, 1, {21}},
  };

  for (size_t i = 0; i < sizeof(amer2_fixtures) / sizeof(amer2_fixtures[0]); ++i) {
    if (check_tile(&map, &amer2_fixtures[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "%s\n", err);
      map_free(&map);
      return 1;
    }
  }

#if MAP_COAST_OVERLAYS_ENABLED
  /*
   * MAPEDIT coast masks; corners use PHYS0 150–153 (NW/NE/SW/SE land).
   */
  static const MapTileExpectation amer2_coast_fixtures[] = {
    {6, 14, 10, 4, {137, 138, 139, 140}},
    {23, 2, 10, 1, {153}},
    {8, 2, 10, 4, {121, 118, 135, 140}},
    {1, 3, 10, 1, {151}},
    {18, 2, 10, 1, {150}},
    {33, 6, 10, 4, {117, 126, 131, 116}},
    {9, 25, 10, 4, {113, 118, 127, 132}},
    {8, 26, 10, 4, {133, 130, 115, 112}},
    {34, 7, 10, 4, {129, 122, 119, 128}},
  };

  for (size_t i = 0; i < sizeof(amer2_coast_fixtures) / sizeof(amer2_coast_fixtures[0]); ++i) {
    if (check_tile(&map, &amer2_coast_fixtures[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "coast regression: %s\n", err);
      map_free(&map);
      return 1;
    }
  }

  /* MAPEDIT land underlayer (last cardinal land neighbour TERRAIN sprite). */
  {
    static const struct {
      int x, y, underlayer;
    } under[] = {
      {6, 14, 1},
      {23, 2, 0},
      {1, 3, 4},
      {18, 2, 0},
      {33, 6, 2},
      {8, 26, 1},
      {19, 25, 5},
      {0, 0, 0}, /* coastal: land to the E on tundra row → underlayer 0 */
      {29, 0, -1}, /* open ocean */
    };
    for (size_t i = 0; i < sizeof(under) / sizeof(under[0]); ++i) {
      const int got = map_coast_underlayer_sprite_at(&map, under[i].x, under[i].y);
      if (got != under[i].underlayer) {
        fprintf(
          stderr,
          "underlayer regression: (%d,%d) expected %d got %d\n",
          under[i].x,
          under[i].y,
          under[i].underlayer,
          got
        );
        map_free(&map);
        return 1;
      }
    }
  }
#else
  /* Coast overlays stubbed off — shore tiles should have TERRAIN only. */
  static const MapTileExpectation amer2_coast_disabled[] = {
    {6, 14, 10, 0, {0}},
    {23, 2, 10, 0, {0}},
    {1, 3, 10, 0, {0}},
  };

  for (size_t i = 0; i < sizeof(amer2_coast_disabled) / sizeof(amer2_coast_disabled[0]); ++i) {
    if (check_tile(&map, &amer2_coast_disabled[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "coast disabled regression: %s\n", err);
      map_free(&map);
      return 1;
    }
  }
#endif

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

  /*
   * Minor-river chain on AMER2 (~14,22)–(18,25): cardinal connectivity → PHYS0 17–31.
   * Locks phys0_river_sprite() against the old mask-% count heuristic.
   */
  static const MapTileExpectation amer2_river_chain[] = {
    {14, 22, 1, 1, {17}},
    {15, 22, 8, 1, {22}},
    {15, 23, 8, 1, {25}},
    {16, 23, 3, 1, {19}},
    {17, 23, 5, 1, {22}},
    {17, 24, 3, 1, {28}},
    {17, 25, 8, 1, {25}},
    {18, 25, 5, 1, {19}},
    /* Minor N-only links (~45,50)–(50,49). */
    {45, 50, 5, 1, {24}},
    {48, 46, 5, 1, {24}},
    {50, 49, 5, 1, {24}},
  };

  for (size_t i = 0; i < sizeof(amer2_river_chain) / sizeof(amer2_river_chain[0]); ++i) {
    if (check_tile(&map, &amer2_river_chain[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "river chain regression: %s\n", err);
      map_free(&map);
      return 1;
    }
  }

  /* Minor-river segment on AMER2 (~6,19)–(8,16). */
  static const MapTileExpectation amer2_river_north[] = {
    {6, 19, 1, 1, {21}},
    {7, 19, 8, 1, {26}},
    {7, 18, 1, 1, {28}},
    {7, 17, 1, 1, {28}},
    {7, 16, 1, 1, {21}},
    {8, 16, 8, 1, {18}},
  };

  for (size_t i = 0; i < sizeof(amer2_river_north) / sizeof(amer2_river_north[0]); ++i) {
    if (check_tile(&map, &amer2_river_north[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "river north regression: %s\n", err);
      map_free(&map);
      return 1;
    }
  }

  /* Major/minor junction on AMER2 (~21,18)–(22,20), minor fork at (21,20). */
  static const MapTileExpectation amer2_river_major[] = {
    {21, 18, 3, 1, {27}},
    {22, 18, 3, 1, {7}},
    {21, 20, 3, 1, {19}},
    {22, 20, 3, 1, {14}},
    {29, 15, 3, 1, {28}},
    {29, 14, 2, 1, {20}},
  };

  for (size_t i = 0; i < sizeof(amer2_river_major) / sizeof(amer2_river_major[0]); ++i) {
    if (check_tile(&map, &amer2_river_major[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "river major regression: %s\n", err);
      map_free(&map);
      return 1;
    }
  }

  /*
   * River estuaries (MAPEDIT: ocean & 0xc0 → PHYS0 141–148 after coast).
   */
#if MAP_ESTUARY_OVERLAYS_ENABLED
  static const MapTileExpectation amer2_river_estuary[] = {
    {19, 25, 10, 2, {150, 148}},
    {22, 23, 10, 5, {133, 114, 111, 112, 141}},
    {23, 22, 10, 5, {137, 138, 115, 128, 144}},
    {46, 39, 10, 3, {152, 143, 144}},
    {13, 8, 10, 6, {137, 138, 115, 136, 146, 148}},
    {25, 15, 10, 5, {133, 130, 139, 124, 142}},
  };

  for (size_t i = 0; i < sizeof(amer2_river_estuary) / sizeof(amer2_river_estuary[0]); ++i) {
    if (check_tile(&map, &amer2_river_estuary[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "river estuary regression: %s\n", err);
      map_free(&map);
      return 1;
    }
  }
#else
#if !MAP_COAST_OVERLAYS_ENABLED
  /* Estuary + coast both off — ocean+river tiles draw TERRAIN only. */
  static const MapTileExpectation amer2_estuary_disabled[] = {
    {19, 25, 10, 0, {0}},
    {22, 23, 10, 0, {0}},
    {23, 22, 10, 0, {0}},
    {46, 39, 10, 0, {0}},
  };

  for (size_t i = 0; i < sizeof(amer2_estuary_disabled) / sizeof(amer2_estuary_disabled[0]); ++i) {
    if (check_tile(&map, &amer2_estuary_disabled[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "estuary disabled regression: %s\n", err);
      map_free(&map);
      return 1;
    }
  }
#endif
#endif

  fprintf(stderr,
    "map tests ok (%zu amer2 fixtures, %d scrub, %zu + %zu + %zu river tiles%s%s)\n",
    sizeof(amer2_fixtures) / sizeof(amer2_fixtures[0]),
    scrub_sprite8_tiles,
    sizeof(amer2_river_chain) / sizeof(amer2_river_chain[0]),
    sizeof(amer2_river_north) / sizeof(amer2_river_north[0]),
    sizeof(amer2_river_major) / sizeof(amer2_river_major[0]),
#if MAP_COAST_OVERLAYS_ENABLED
    ", coast enabled"
#else
    ", coast parked"
#endif
    ,
#if MAP_ESTUARY_OVERLAYS_ENABLED
    ", estuary enabled"
#else
    ", estuary parked"
#endif
  );

  map_free(&map);
  diag_shutdown();
  return 0;
}
