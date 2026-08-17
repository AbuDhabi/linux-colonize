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
    /* Shared connectivity: base + mask (N=8,S=4,W=2,E=1); 0-based PHYS0 indices. */
    {1, 1, 0, 2, {69, 48}},
    {2, 11, 4, 1, {32}},
    {43, 68, 0, 1, {32}},
    {5, 21, 1, 1, {48}},
    {4, 20, 8, 0, {0}},
    {8, 14, 8, 1, {103}}, /* scrub + lost-city rumour */
    {1, 0, 0, 1, {64}},
    {1, 2, 0, 1, {73}},
    {16, 2, 0, 2, {70, 98}},
    {4, 18, 5, 1, {64}},
    {36, 4, 2, 2, {68, 97}},
    {27, 14, 3, 2, {69, 98}},
    {3, 3, 4, 1, {72}},
    {27, 20, 6, 1, {78}},
    {39, 28, 7, 1, {66}},
    {24, 19, 4, 2, {79, 52}},
    {24, 20, 4, 2, {79, 56}},
    {9, 26, 1, 1, {48}},
    {16, 3, 0, 2, {78, 21}},
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
   * MAPEDIT coast masks; corners 150–153; fragments 108+4*m+q (MAPEDIT 0x6d − 1).
   */
  static const MapTileExpectation amer2_coast_fixtures[] = {
    {6, 14, 10, 4, {136, 137, 138, 139}},
    {23, 2, 10, 1, {153}},
    {8, 2, 10, 4, {120, 117, 134, 139}},
    {1, 3, 10, 1, {151}},
    {18, 2, 10, 1, {150}},
    {33, 6, 10, 4, {116, 125, 130, 115}},
    {9, 25, 10, 4, {112, 117, 126, 131}},
    {8, 26, 10, 4, {132, 129, 114, 111}},
    {34, 7, 10, 4, {128, 121, 118, 127}},
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

  /* Land-land transitions (MAPEDIT 06da): PHYS0 104+q then neighbour TERRAIN fill. */
  {
    const int n = map_land_transition_count(&map, 4, 18);
    if (n != 3) {
      fprintf(stderr, "transition count (4,18) expected 3 got %d\n", n);
      map_free(&map);
      return 1;
    }
    if (map_land_transition_mask_sprite_at(&map, 4, 18, 0) != 104 ||
        map_land_transition_fill_terrain_at(&map, 4, 18, 0) != 3 ||
        map_land_transition_mask_sprite_at(&map, 4, 18, 1) != 105 ||
        map_land_transition_fill_terrain_at(&map, 4, 18, 1) != 1 ||
        map_land_transition_mask_sprite_at(&map, 4, 18, 2) != 106 ||
        map_land_transition_fill_terrain_at(&map, 4, 18, 2) != 8) {
      fprintf(stderr, "transition sprites (4,18) mismatch\n");
      map_free(&map);
      return 1;
    }
    /* (2,15) conifer vs ocean (2,16) filled from prairie (3,16). */
    {
      int found = 0;
      const int tn = map_land_transition_count(&map, 2, 15);
      for (int i = 0; i < tn; ++i) {
        if (map_land_transition_mask_sprite_at(&map, 2, 15, i) == 106 &&
            map_land_transition_fill_terrain_at(&map, 2, 15, i) == 3) {
          found = 1;
        }
      }
      if (!found) {
        fprintf(stderr, "transition (2,15)→ocean corner: expected S mask 106 fill prairie 3\n");
        map_free(&map);
        return 1;
      }
    }
  }

  /* Procedural resources / rumours (MAPEDIT 0458 / 0540, seed 100). */
  {
    int resources = 0;
    int rumours = 0;
    int fish = 0;
    int bad_gems = 0;
    for (int y = 0; y < (int)map.height; ++y) {
      for (int x = 0; x < (int)map.width; ++x) {
        const int n = map_phys0_overlay_count(&map, x, y);
        for (int i = 0; i < n; ++i) {
          const int s = map_phys0_overlay_sprite_at(&map, x, y, i);
          if (s >= 89 && s <= 102) {
            ++resources;
          }
          if (s == 96) {
            ++fish;
          }
          if (s == 95) {
            /* Table value 6: tundra/marsh/swamp and wetland/rain forests. */
            const int idx = map_get_terrain(&map, x, y) & 0x1f;
            if (idx != 0 && idx != 6 && idx != 7 && idx != 14 && idx != 15 && idx != 22 &&
                idx != 23) {
              ++bad_gems;
            }
          }
          if (s == 103) {
            ++rumours;
          }
        }
      }
    }
    /* 421, not 420: map_resource_type_at_ex's forest-range check
     * (FUN_12ab_0458 local_4) only covered pedia 8-15, missing pedia
     * 16-23 (the other forest half, same 8 types via &7) — asm-confirmed
     * against mapedit.c's decompile. One AMER2 tile (pedia 19, forest) was
     * silently dropping its resource because of it; fixed in map.c. */
    if (resources != 421 || rumours != 40) {
      fprintf(stderr, "resource/rumour count expected 421/40 got %d/%d\n", resources, rumours);
      map_free(&map);
      return 1;
    }
    if (fish != 275) {
      fprintf(stderr, "fish resource count expected 275 got %d\n", fish);
      map_free(&map);
      return 1;
    }
    if (bad_gems != 0) {
      fprintf(stderr, "minerals/gems (95) on unexpected terrain (%d tiles)\n", bad_gems);
      map_free(&map);
      return 1;
    }
  }

  /*
   * Minor-river chain on AMER2 (~14,22)–(18,25): shared mask → PHYS0 16–31.
   * Forest tiles may also report a canopy sprite ahead of the river overlay.
   */
  static const MapTileExpectation amer2_river_chain[] = {
    {14, 22, 1, 1, {17}},
    {15, 22, 8, 1, {22}},
    {15, 23, 8, 1, {25}},
    {16, 23, 3, 1, {19}},
    {17, 23, 5, 1, {22}},
    {17, 24, 3, 3, {64, 28, 98}},
    {17, 25, 8, 1, {25}},
    {18, 25, 5, 2, {68, 19}},
    {45, 50, 5, 3, {70, 24, 103}},
    {48, 46, 5, 1, {24}},
    {50, 49, 5, 2, {79, 24}},
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
    {7, 17, 1, 2, {28, 90}},
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
    {21, 18, 3, 2, {69, 11}},
    {22, 18, 3, 2, {79, 7}},
    {21, 20, 3, 2, {79, 19}},
    {22, 20, 3, 3, {79, 14, 98}},
    {29, 15, 3, 2, {79, 28}},
    {29, 14, 2, 2, {79, 20}},
  };

  for (size_t i = 0; i < sizeof(amer2_river_major) / sizeof(amer2_river_major[0]); ++i) {
    if (check_tile(&map, &amer2_river_major[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "river major regression: %s\n", err);
      map_free(&map);
      return 1;
    }
  }

  /*
   * River estuaries (MAPEDIT 0x8d+q → 0-based 140–147 after coast).
   */
#if MAP_ESTUARY_OVERLAYS_ENABLED
  static const MapTileExpectation amer2_river_estuary[] = {
    {19, 25, 10, 2, {150, 147}},
    {22, 23, 10, 5, {132, 113, 110, 111, 140}},
    {23, 22, 10, 6, {136, 137, 114, 127, 96, 143}},
    {46, 39, 10, 3, {152, 142, 143}},
    {13, 8, 10, 6, {136, 137, 114, 135, 145, 147}},
    {25, 15, 10, 5, {132, 129, 138, 123, 141}},
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

  /* Runtime plow overlay: PHYS0 149 when MAP_IMPROVE_PLOWED set. */
  {
    ColonizeWorldMap plow_map;
    memset(&plow_map, 0, sizeof(plow_map));
    plow_map.width = 4;
    plow_map.height = 4;
    plow_map.tile_count = 16;
    plow_map.terrain = calloc(16, 1);
    plow_map.layer2 = calloc(16, 1);
    plow_map.layer3 = calloc(16, 1);
    plow_map.improve = calloc(16, 1);
    plow_map.seen = calloc(16, 1);
    if (!plow_map.terrain || !plow_map.layer2 || !plow_map.layer3 || !plow_map.improve ||
        !plow_map.seen) {
      fprintf(stderr, "plow overlay alloc failed\n");
      map_free(&plow_map);
      map_free(&map);
      return 1;
    }
    plow_map.terrain[0] = 1; /* plains */
    if (map_phys0_plow_sprite_at(&plow_map, 0, 0) != -1) {
      fprintf(stderr, "plow overlay expected -1 before set\n");
      map_free(&plow_map);
      map_free(&map);
      return 1;
    }
    map_tile_set_plowed(&plow_map, 0, 0, true);
    if (map_phys0_plow_sprite_at(&plow_map, 0, 0) != 149) {
      fprintf(
        stderr,
        "plow overlay expected PHYS0 149 got %d\n",
        map_phys0_plow_sprite_at(&plow_map, 0, 0)
      );
      map_free(&plow_map);
      map_free(&map);
      return 1;
    }
    if (map_phys0_road_sprite_at(&plow_map, 0, 0) != -1) {
      fprintf(stderr, "road overlay expected -1 before set\n");
      map_free(&plow_map);
      map_free(&map);
      return 1;
    }
    map_tile_set_road(&plow_map, 0, 0, true);
    if (map_phys0_road_layer_count(&plow_map, 0, 0) != 1 ||
        map_phys0_road_layer_sprite_at(&plow_map, 0, 0, 0) != 80) {
      fprintf(
        stderr,
        "road overlay expected isolated PHYS0 80 (count=%d sprite=%d)\n",
        map_phys0_road_layer_count(&plow_map, 0, 0),
        map_phys0_road_layer_sprite_at(&plow_map, 0, 0, 0)
      );
      map_free(&plow_map);
      map_free(&map);
      return 1;
    }
    /* N neighbor → stub 81 only (FUN_6ba1_0938 multi-blit; no isolated 80). */
    map_tile_set_road(&plow_map, 0, 1, true); /* center (0,1) + north (0,0) */
    if (map_phys0_road_layer_count(&plow_map, 0, 1) != 1 ||
        map_phys0_road_layer_sprite_at(&plow_map, 0, 1, 0) != 81) {
      fprintf(
        stderr,
        "road N-connect expected PHYS0 81 (count=%d sprite=%d)\n",
        map_phys0_road_layer_count(&plow_map, 0, 1),
        map_phys0_road_layer_sprite_at(&plow_map, 0, 1, 0)
      );
      map_free(&plow_map);
      map_free(&map);
      return 1;
    }
    /* Add S neighbor of (0,1) at (0,2) → stubs 81 (N) + 85 (S). */
    map_tile_set_road(&plow_map, 0, 2, true);
    if (map_phys0_road_layer_count(&plow_map, 0, 1) != 2 ||
        map_phys0_road_layer_sprite_at(&plow_map, 0, 1, 0) != 81 ||
        map_phys0_road_layer_sprite_at(&plow_map, 0, 1, 1) != 85) {
      fprintf(
        stderr,
        "road N+S expected 81,85 (count=%d a=%d b=%d)\n",
        map_phys0_road_layer_count(&plow_map, 0, 1),
        map_phys0_road_layer_sprite_at(&plow_map, 0, 1, 0),
        map_phys0_road_layer_sprite_at(&plow_map, 0, 1, 1)
      );
      map_free(&plow_map);
      map_free(&map);
      return 1;
    }
    map_free(&plow_map);
    fprintf(stderr, "plow overlay PHYS0 149 ok; road 80–88 connectivity ok\n");
  }

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
    ", coast disabled"
#endif
    ,
#if MAP_ESTUARY_OVERLAYS_ENABLED
    ", estuary enabled"
#else
    ", estuary disabled"
#endif
  );

  map_free(&map);
  diag_shutdown();
  return 0;
}
