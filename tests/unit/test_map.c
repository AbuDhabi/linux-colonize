#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/map.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

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

/* Shared fixture: all cases below are read-only lookups against the same
 * loaded AMER2 map, so it is loaded once lazily and never mutated by any
 * case (each case that needs its own mutable map builds a small local one
 * instead: see case_plow_road_overlay / case_fog_edges). That read-only
 * property is what makes it safe to share across cases run in any order
 * (reverse/shuffle/only). */
static ColonizeWorldMap g_map;
static int g_map_ready = 0; /* 0 = not attempted, 1 = ok, -1 = failed */

static void free_g_map(void) {
  if (g_map_ready == 1) {
    map_free(&g_map);
  }
}

static int ensure_map(void) {
  if (g_map_ready != 0) {
    return g_map_ready == 1 ? 0 : 1;
  }
  diag_init(0, NULL);
  char err[256];
  if (!map_load_mp("COLONIZE/AMER2.MP", &g_map, err, sizeof(err))) {
    fprintf(stderr, "map load failed: %s\n", err);
    g_map_ready = -1;
    return 1;
  }
  g_map_ready = 1;
  atexit(free_g_map);
  return 0;
}

static int run_fixture_array(const MapTileExpectation* arr, size_t n, const char* label) {
  char err[256];
  if (ensure_map() != 0) {
    return 1;
  }
  for (size_t i = 0; i < n; ++i) {
    if (check_tile(&g_map, &arr[i], err, sizeof(err)) != 0) {
      fprintf(stderr, "%s: %s\n", label, err);
      return 1;
    }
  }
  return 0;
}

static int case_map_load_and_ocean(void) {
  if (ensure_map() != 0) {
    return 1;
  }
  if (g_map.width != 58 || g_map.height != 72 || g_map.tile_count != 58u * 72u) {
    fprintf(stderr, "unexpected map size %ux%u (%zu tiles)\n", g_map.width, g_map.height,
      g_map.tile_count);
    return 1;
  }

  const uint8_t ocean = map_get_terrain(&g_map, 0, 0);
  if ((ocean & 0x1f) != 25 || map_terrain_sprite_at(&g_map, 0, 0) != 10) {
    fprintf(stderr, "ocean tile expected index 25 sprite 10, got 0x%02x sprite %d\n",
      ocean, map_terrain_sprite_at(&g_map, 0, 0));
    return 1;
  }
  return 0;
}

static int case_amer2_fixtures(void) {
  /* Shared connectivity: base + mask (N=8,S=4,W=2,E=1); 0-based PHYS0 indices. */
  static const MapTileExpectation amer2_fixtures[] = {
    {1, 1, 0, 2, {69, 48}},
    {2, 11, 4, 1, {32}},
    {43, 68, 0, 0, {0}}, /* one-tile island: layer3 0x0e is continent id 14, not a peak */
    {5, 21, 1, 1, {48}},
    {4, 20, 8, 0, {0}},
    {8, 14, 8, 0, {0}}, /* scrub; its rumour moved to (9,15) — see #98 below */
    {9, 15, 3, 2, {46, 103}}, /* river + lost-city rumour */
    {1, 0, 0, 2, {64, 92}},
    {1, 2, 0, 1, {73}},
    {16, 2, 0, 1, {70}},
    {4, 18, 5, 1, {64}},
    {36, 4, 2, 1, {68}},
    {27, 14, 3, 1, {69}},
    {3, 3, 4, 1, {72}},
    {27, 20, 6, 1, {78}},
    {39, 28, 7, 1, {66}},
    {24, 19, 4, 2, {79, 52}},
    {24, 20, 4, 2, {79, 56}},
    {9, 26, 1, 1, {48}},
    {16, 3, 0, 2, {78, 21}},
  };
  return run_fixture_array(amer2_fixtures, sizeof(amer2_fixtures) / sizeof(amer2_fixtures[0]),
    "amer2 fixture");
}

static int case_coast_overlays(void) {
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
    {8, 26, 10, 5, {132, 129, 114, 111, 96}},
    {34, 7, 10, 4, {128, 121, 118, 127}},
  };
  if (run_fixture_array(amer2_coast_fixtures,
        sizeof(amer2_coast_fixtures) / sizeof(amer2_coast_fixtures[0]), "coast regression") != 0) {
    return 1;
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
    if (ensure_map() != 0) {
      return 1;
    }
    for (size_t i = 0; i < sizeof(under) / sizeof(under[0]); ++i) {
      const int got = map_coast_underlayer_sprite_at(&g_map, under[i].x, under[i].y);
      if (got != under[i].underlayer) {
        fprintf(
          stderr,
          "underlayer regression: (%d,%d) expected %d got %d\n",
          under[i].x,
          under[i].y,
          under[i].underlayer,
          got
        );
        return 1;
      }
    }
  }
  return 0;
#else
  /* Coast overlays stubbed off — shore tiles should have TERRAIN only. */
  static const MapTileExpectation amer2_coast_disabled[] = {
    {6, 14, 10, 0, {0}},
    {23, 2, 10, 0, {0}},
    {1, 3, 10, 0, {0}},
  };
  return run_fixture_array(amer2_coast_disabled,
    sizeof(amer2_coast_disabled) / sizeof(amer2_coast_disabled[0]), "coast disabled regression");
#endif
}

static int case_scrub_forest(void) {
  /* Scrub forest: only terrain indices 9 and 17 use TERRAIN sprite 8 (no PHYS0). */
  if (ensure_map() != 0) {
    return 1;
  }
  int scrub_sprite8_tiles = 0;
  for (int y = 0; y < (int)g_map.height; ++y) {
    for (int x = 0; x < (int)g_map.width; ++x) {
      const uint8_t byte = map_get_terrain(&g_map, x, y);
      const int terrain_index = (int)(byte & 0x1fu);
      const int terrain_sprite = map_terrain_sprite_at(&g_map, x, y);
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
        return 1;
      }
      if (map_phys0_forest_sprite_at(&g_map, x, y) >= 0) {
        fprintf(stderr, "scrub regression: (%d,%d) must not have PHYS0 forest overlay\n", x, y);
        return 1;
      }
    }
  }
  if (scrub_sprite8_tiles != 81) {
    fprintf(stderr, "scrub regression: expected 81 TERRAIN-8 tiles on AMER2, got %d\n",
      scrub_sprite8_tiles);
    return 1;
  }
  return 0;
}

static int case_land_transitions(void) {
  /* Land-land transitions (MAPEDIT 06da): PHYS0 104+q then neighbour TERRAIN fill. */
  if (ensure_map() != 0) {
    return 1;
  }
  const int n = map_land_transition_count(&g_map, 4, 18);
  if (n != 3) {
    fprintf(stderr, "transition count (4,18) expected 3 got %d\n", n);
    return 1;
  }
  if (map_land_transition_mask_sprite_at(&g_map, 4, 18, 0) != 104 ||
      map_land_transition_fill_terrain_at(&g_map, 4, 18, 0) != 3 ||
      map_land_transition_mask_sprite_at(&g_map, 4, 18, 1) != 105 ||
      map_land_transition_fill_terrain_at(&g_map, 4, 18, 1) != 1 ||
      map_land_transition_mask_sprite_at(&g_map, 4, 18, 2) != 106 ||
      map_land_transition_fill_terrain_at(&g_map, 4, 18, 2) != 8) {
    fprintf(stderr, "transition sprites (4,18) mismatch\n");
    return 1;
  }
  /* (2,15) conifer vs ocean (2,16) filled from prairie (3,16). */
  {
    int found = 0;
    const int tn = map_land_transition_count(&g_map, 2, 15);
    for (int i = 0; i < tn; ++i) {
      if (map_land_transition_mask_sprite_at(&g_map, 2, 15, i) == 106 &&
          map_land_transition_fill_terrain_at(&g_map, 2, 15, i) == 3) {
        found = 1;
      }
    }
    if (!found) {
      fprintf(stderr, "transition (2,15)→ocean corner: expected S mask 106 fill prairie 3\n");
      return 1;
    }
  }
  return 0;
}

static int case_resources_rumours(void) {
  /* Procedural resources / rumours (MAPEDIT 0458 / 0540, seed 100). */
  if (ensure_map() != 0) {
    return 1;
  }
  int resources = 0;
  int rumours = 0;
  int fish = 0;
  int bad_gems = 0;
  for (int y = 0; y < (int)g_map.height; ++y) {
    for (int x = 0; x < (int)g_map.width; ++x) {
      const int n = map_phys0_overlay_count(&g_map, x, y);
      for (int i = 0; i < n; ++i) {
        const int s = map_phys0_overlay_sprite_at(&g_map, x, y, i);
        if (s >= 89 && s <= 102) {
          ++resources;
        }
        if (s == 96) {
          ++fish;
        }
        if (s == 95) {
          /* Table value 6: tundra/marsh/swamp and wetland/rain forests. */
          const int idx = map_get_terrain(&g_map, x, y) & 0x1f;
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
  /* 420 -> 421: map_resource_type_at_ex's forest-range check
   * (FUN_12ab_0458 local_4) only covered pedia 8-15, missing pedia
   * 16-23 (the other forest half, same 8 types via &7) — asm-confirmed
   * against mapedit.c's decompile. One AMER2 tile (pedia 19, forest) was
   * silently dropping its resource because of it; fixed in map.c.
   * 421 -> 425: two more fixes, player-confirmed 2026-08-18 via
   * colony_prod02's New Holland (real single-turn DOS capture): (1) the
   * coordinate hash's `+1` ("DOS is 1-based") was never actually in
   * FUN_12ab_0458 itself, just this port's unverified guess about its
   * caller — dropping it moves the whole resource pattern by one tile
   * in every direction; (2) the hash-match gate was wrongly skipped
   * whenever a `_for_yield` (settlement-transparent) call landed on a
   * tile that also had the settlement bit set — every colony center —
   * so centers always reported a match regardless of the real hash.
   * Rumours 40 -> 38 (2026-09-09, smell_audit #98): map_procedural_rumour_at
   * still carried the same unverified `+1` after the resource hash dropped
   * it, even though mapedit.c:9406/9410 hand FUN_12ab_0458 and
   * FUN_12ab_0540 the identical coordinate pair, so both hashes must share
   * a coordinate space. The hash lattice is unchanged, but each lattice
   * point is now terrain-tested one tile SE of where it used to be, and two
   * points moved onto terrain 0540 skips (ocean / high seas / arctic). */
  if (resources != 425 || rumours != 38) {
    fprintf(stderr, "resource/rumour count expected 425/38 got %d/%d\n", resources, rumours);
    return 1;
  }
  /* 267, not 275 — same 2026-08-18 coordinate-hash fixes as the
   * resource/rumour count above shifted which tiles match. */
  if (fish != 267) {
    fprintf(stderr, "fish resource count expected 267 got %d\n", fish);
    return 1;
  }
  if (bad_gems != 0) {
    fprintf(stderr, "minerals/gems (95) on unexpected terrain (%d tiles)\n", bad_gems);
    return 1;
  }
  return 0;
}

static int case_river_chain(void) {
  /*
   * Minor-river chain on AMER2 (~14,22)–(18,25): shared mask → PHYS0 16–31.
   * Forest tiles may also report a canopy sprite ahead of the river overlay.
   * Resource sprites (89-102) in this block shifted 2026-08-18 along with
   * the rest of the map's resource pattern — see the whole-map resource
   * count comment above for why.
   */
  static const MapTileExpectation amer2_river_chain[] = {
    {14, 22, 1, 1, {17}},
    {15, 22, 8, 1, {22}},
    {15, 23, 8, 2, {25, 90}},
    {16, 23, 3, 1, {19}},
    {17, 23, 5, 2, {22, 94}},
    {17, 24, 3, 2, {64, 28}},
    {17, 25, 8, 1, {25}},
    {18, 25, 5, 3, {68, 19, 99}},
    {45, 50, 5, 2, {70, 24}},
    {46, 51, 5, 2, {67, 103}}, /* rumour shifted here from (45,50) — see #98 */
    {48, 46, 5, 1, {24}},
    {50, 49, 5, 3, {79, 24, 99}},
  };
  return run_fixture_array(amer2_river_chain,
    sizeof(amer2_river_chain) / sizeof(amer2_river_chain[0]), "river chain regression");
}

static int case_river_north(void) {
  /* Minor-river segment on AMER2 (~6,19)–(8,16). Resource sprite shifted
   * from (7,17) to (6,19) with the 2026-08-18 coordinate-hash fix — see
   * the whole-map resource count comment above. */
  static const MapTileExpectation amer2_river_north[] = {
    {6, 19, 1, 2, {21, 90}},
    {7, 19, 8, 1, {26}},
    {7, 18, 1, 1, {28}},
    {7, 17, 1, 1, {28}},
    {7, 16, 1, 1, {21}},
    {8, 16, 8, 1, {18}},
  };
  return run_fixture_array(amer2_river_north,
    sizeof(amer2_river_north) / sizeof(amer2_river_north[0]), "river north regression");
}

static int case_river_major(void) {
  /* Major/minor junction on AMER2 (~21,18)–(22,20), minor fork at (21,20). */
  static const MapTileExpectation amer2_river_major[] = {
    {21, 18, 3, 2, {69, 11}},
    {22, 18, 3, 2, {79, 7}},
    {21, 20, 3, 2, {79, 19}},
    {22, 20, 3, 2, {79, 14}},
    {29, 15, 3, 2, {79, 28}},
    {29, 14, 2, 2, {79, 20}},
  };
  return run_fixture_array(amer2_river_major,
    sizeof(amer2_river_major) / sizeof(amer2_river_major[0]), "river major regression");
}

static int case_river_estuary(void) {
  /*
   * River estuaries (MAPEDIT 0x8d+q → 0-based 140–147 after coast).
   */
#if MAP_ESTUARY_OVERLAYS_ENABLED
  static const MapTileExpectation amer2_river_estuary[] = {
    {19, 25, 10, 2, {150, 147}},
    {22, 23, 10, 5, {132, 113, 110, 111, 140}},
    {23, 22, 10, 5, {136, 137, 114, 127, 143}},
    {46, 39, 10, 3, {152, 142, 143}},
    {13, 8, 10, 6, {136, 137, 114, 135, 145, 147}},
    {25, 15, 10, 5, {132, 129, 138, 123, 141}},
  };
  return run_fixture_array(amer2_river_estuary,
    sizeof(amer2_river_estuary) / sizeof(amer2_river_estuary[0]), "river estuary regression");
#else
#if !MAP_COAST_OVERLAYS_ENABLED
  /* Estuary + coast both off — ocean+river tiles draw TERRAIN only. */
  static const MapTileExpectation amer2_estuary_disabled[] = {
    {19, 25, 10, 0, {0}},
    {22, 23, 10, 0, {0}},
    {23, 22, 10, 0, {0}},
    {46, 39, 10, 0, {0}},
  };
  return run_fixture_array(amer2_estuary_disabled,
    sizeof(amer2_estuary_disabled) / sizeof(amer2_estuary_disabled[0]),
    "estuary disabled regression");
#else
  return 0;
#endif
#endif
}

static int case_plow_road_overlay(void) {
  /* Runtime plow overlay: PHYS0 149 when MAP_IMPROVE_PLOWED set. Uses its
   * own small local map: this exercises mutation (map_tile_set_plowed /
   * map_tile_set_road), so it must not share g_map with the read-only
   * cases above. */
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
    return 1;
  }
  plow_map.terrain[0] = 1; /* plains */
  if (map_phys0_plow_sprite_at(&plow_map, 0, 0) != -1) {
    fprintf(stderr, "plow overlay expected -1 before set\n");
    map_free(&plow_map);
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
    return 1;
  }
  if (map_phys0_road_layer_sprite_at(&plow_map, 0, 0, 0) != -1) {
    fprintf(stderr, "road overlay expected -1 before set\n");
    map_free(&plow_map);
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
    return 1;
  }
  /*
   * bugs.md: a settlement tile carries road art (DOS FA mask 0x0a). Put a
   * colony at (1,1), diagonally NE of (0,2): (0,2) must gain that stub, and
   * the colony tile itself must render as a road tile.
   */
  plow_map.layer2[1 * plow_map.width + 1] |= MAP_OCCUPANCY_HAS_CITY;
  if (map_phys0_road_layer_count(&plow_map, 1, 1) <= 0) {
    fprintf(stderr, "colony tile should carry road art\n");
    map_free(&plow_map);
    return 1;
  }
  {
    int saw_ne = 0;
    const int n = map_phys0_road_layer_count(&plow_map, 0, 2);
    for (int i = 0; i < n; ++i) {
      if (map_phys0_road_layer_sprite_at(&plow_map, 0, 2, i) == 82) {
        saw_ne = 1; /* PHYS0 81 + dir 1 (NE) */
      }
    }
    if (!saw_ne) {
      fprintf(stderr, "road should stub NE into the colony tile (count=%d)\n", n);
      map_free(&plow_map);
      return 1;
    }
  }
  map_free(&plow_map);
  return 0;
}

static int case_fog_edges(void) {
  /* bugs.md fog edges: VICEROY FUN_6ba1_06e0 mask+fill pairs across the
   * seen/unseen boundary. 5x5 board: plains everywhere, ocean at (3,2);
   * only (2,2) seen by nation 0. Uses its own local map (mutated in
   * place), independent of g_map. */
  ColonizeWorldMap fog_map;
  char err2[128];
  memset(&fog_map, 0, sizeof(fog_map)); /* map_alloc map_free()s the struct first */
  memset(err2, 0, sizeof(err2));
  if (!map_alloc(&fog_map, 5, 5, err2, sizeof(err2))) {
    fprintf(stderr, "fog: map_alloc failed: %s\n", err2);
    return 1;
  }
  for (int i = 0; i < 25; ++i) {
    fog_map.terrain[i] = 2; /* plains */
  }
  fog_map.terrain[2 * 5 + 3] = 25; /* ocean at (3,2) */
  memset(fog_map.seen, 0, fog_map.tile_count);
  memset(fog_map.layer2, 0, fog_map.tile_count);
  memset(fog_map.layer3, 0, fog_map.tile_count);
  memset(fog_map.improve, 0, fog_map.tile_count);
  fog_map.seen[2 * 5 + 2] = MAP_SEEN_NATION_BIT(0);
  /* Seen tile (2,2): 4 unseen neighbours → 4 mask+fill pairs. */
  if (map_fog_edge_count(&fog_map, 2, 2, 0) != 4) {
    fprintf(stderr, "fog: seen tile should have 4 fog edges\n");
    map_free(&fog_map);
    return 1;
  }
  if (map_fog_edge_fill_sprite_at(&fog_map, 2, 2, 0, 0) != 2) {
    fprintf(stderr, "fog: north edge should fill with plains sprite\n");
    map_free(&fog_map);
    return 1;
  }
  /*
   * smell #96 / FUN_6ba1_06e0: the ocean resolve is gated on param_2 == 0
   * (the DRAWN tile is land) alone, never on the neighbour's visibility, so
   * the SEEN side rescans an ocean neighbour just like the fog side does.
   * (3,2) is ocean; its W cardinal is the land tile (2,2) itself, so the
   * east edge dithers plains (2), not the ocean sprite (10).
   */
  if (map_fog_edge_mask_sprite_at(&fog_map, 2, 2, 0, 1) != 105 ||
      map_fog_edge_fill_sprite_at(&fog_map, 2, 2, 0, 1) != 2) {
    fprintf(stderr, "fog: east edge should be mask 105 + rescanned land fill\n");
    map_free(&fog_map);
    return 1;
  }
  /* Unseen land tile (2,1): one seen neighbour to the south → mask 106 +
   * that neighbour's plains dither. */
  if (map_fog_reveal_edge_count(&fog_map, 2, 1, 0) != 1 ||
      map_fog_reveal_edge_mask_sprite_at(&fog_map, 2, 1, 0, 0) != 106 ||
      map_fog_reveal_edge_fill_sprite_at(&fog_map, 2, 1, 0, 0) != 2) {
    fprintf(stderr, "fog: unseen tile should dither the seen neighbour in\n");
    map_free(&fog_map);
    return 1;
  }
  /*
   * Same (2,2)/(3,2) land/ocean pair, fog flipped: the fog LAND tile with a
   * seen ocean neighbour resolves via the neighbour's W/S/E/N cardinals.
   * Both directions must agree — that agreement is the point of #96.
   */
  {
    const int seen_side = map_fog_edge_fill_sprite_at(&fog_map, 2, 2, 0, 1);
    fog_map.seen[2 * 5 + 2] = 0;
    fog_map.seen[2 * 5 + 3] = MAP_SEEN_NATION_BIT(0); /* the ocean is seen */
    if (map_fog_reveal_edge_count(&fog_map, 2, 2, 0) != 1 ||
        map_fog_reveal_edge_mask_sprite_at(&fog_map, 2, 2, 0, 0) != 105 ||
        map_fog_reveal_edge_fill_sprite_at(&fog_map, 2, 2, 0, 0) != 2) {
      fprintf(stderr, "fog: ocean neighbour should resolve to a land cardinal fill\n");
      map_free(&fog_map);
      return 1;
    }
    if (map_fog_reveal_edge_fill_sprite_at(&fog_map, 2, 2, 0, 0) != seen_side) {
      fprintf(stderr, "fog: seen→fog and fog→seen edges must agree (%d vs %d)\n",
        seen_side, map_fog_reveal_edge_fill_sprite_at(&fog_map, 2, 2, 0, 0));
      map_free(&fog_map);
      return 1;
    }
  }
  /*
   * param_2 == 1 (the drawn tile is itself ocean): no rescan, the ocean
   * neighbour's art is taken as-is on both sides. Make (2,2) ocean too.
   */
  fog_map.terrain[2 * 5 + 2] = 25;
  fog_map.seen[2 * 5 + 3] = 0;
  fog_map.seen[2 * 5 + 2] = MAP_SEEN_NATION_BIT(0);
  if (map_fog_edge_count(&fog_map, 2, 2, 0) != 4 ||
      map_fog_edge_fill_sprite_at(&fog_map, 2, 2, 0, 1) != 10) {
    fprintf(stderr, "fog: ocean tile should take the ocean neighbour art as-is\n");
    map_free(&fog_map);
    return 1;
  }
  fog_map.seen[2 * 5 + 2] = 0;
  fog_map.seen[2 * 5 + 3] = MAP_SEEN_NATION_BIT(0);
  if (map_fog_reveal_edge_fill_sprite_at(&fog_map, 2, 2, 0, 0) != 10) {
    fprintf(stderr, "fog: fog ocean tile should take the ocean neighbour art as-is\n");
    map_free(&fog_map);
    return 1;
  }
  map_free(&fog_map);
  return 0;
}

/*
 * bugs.md #613: the Pioneer clear/plow lumber reward reads DS:0x2f80 =
 * terrain record +0x0a = NAMES.TXT yield column 5 (Lumberjack), not the +0x8
 * Cotton column it used to read.
 */
static int case_lumber_reward_column(void) {
  /* class -> NAMES.TXT Lumber column. Forests 8..15 repeat at 16..23. */
  static const struct { int cls; int want; const char* what; } k[] = {
    {2, 0, "Plains (unforested: no lumber)"},
    {3, 0, "Prairie (was 3 from the Cotton column)"},
    {8, 2, "Boreal"},
    {9, 1, "Scrub"},
    {10, 3, "Mixed Forest"},
    {11, 2, "Broadleaf"},
    {12, 3, "Conifer"},
    {13, 2, "Tropical"},
    {14, 2, "Wetland"},
    {15, 2, "Rain"},
    {18, 3, "Mixed Forest (alt half)"},
    {24, 0, "Arctic"},
    {28, 0, "Hills"},
  };
  for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
    const int got = map_dos_terr_lumber_reward_byte(k[i].cls);
    if (got != k[i].want) {
      fprintf(
        stderr, "lumber reward class %d (%s): want %d got %d\n", k[i].cls, k[i].what, k[i].want,
        got
      );
      return 1;
    }
  }
  return 0;
}

/*
 * bugs.md #622: FUN_6ba1_0938 raw 109621-109665 blits the plow art right after
 * the forest canopy, before the hill/river/resource/rumour overlays. A plowed
 * river tile must therefore list PHYS0 149 BEFORE its river overlay, with no
 * second copy of any overlay above it (the old #396 resource re-blit).
 */
static int case_plow_layer_order(void) {
  ColonizeWorldMap m;
  memset(&m, 0, sizeof(m));
  m.width = 4;
  m.height = 4;
  m.tile_count = 16;
  m.terrain = calloc(16, 1);
  m.layer2 = calloc(16, 1);
  m.layer3 = calloc(16, 1);
  m.improve = calloc(16, 1);
  m.seen = calloc(16, 1);
  if (!m.terrain || !m.layer2 || !m.layer3 || !m.improve || !m.seen) {
    map_free(&m);
    return 1;
  }
  m.terrain[1 * 4 + 1] = (uint8_t)(2 | 0x40); /* Plains + river */
  map_tile_set_plowed(&m, 1, 1, true);
  const int overlays = map_phys0_overlay_count(&m, 1, 1);
  if (overlays < 1) {
    fprintf(stderr, "plow order: fixture tile has no overlay to sit under\n");
    map_free(&m);
    return 1;
  }
  ColonizeMapLayerCmd cmds[MAP_LAYER_CMDS_MAX];
  const int n = map_tile_layer_cmds(&m, 1, 1, 0, cmds, MAP_LAYER_CMDS_MAX);
  int plow_at = -1;
  int plows = 0;
  for (int i = 0; i < n; ++i) {
    if (cmds[i].sheet == MAP_LAYER_SHEET_PHYS0 && cmds[i].sprite == 149) {
      if (plow_at < 0) {
        plow_at = i;
      }
      ++plows;
    }
  }
  if (plows != 1 || plow_at < 0) {
    fprintf(stderr, "plow order: expected exactly one PHYS0 149, got %d\n", plows);
    map_free(&m);
    return 1;
  }
  const int river = map_phys0_overlay_sprite_at(&m, 1, 1, 0);
  int river_at = -1;
  for (int i = 0; i < n; ++i) {
    if (cmds[i].sheet == MAP_LAYER_SHEET_PHYS0 && cmds[i].sprite == river && i != plow_at) {
      river_at = i;
      break;
    }
  }
  if (river_at < 0 || river_at < plow_at) {
    fprintf(
      stderr, "plow order: plow at %d, river sprite %d at %d (plow must come first)\n", plow_at,
      river, river_at
    );
    map_free(&m);
    return 1;
  }
  /* No resource / rumour sprite (PHYS0 89-103) may be listed twice any more:
   * that was the #396 re-blit this row deleted. */
  for (int i = 0; i < n; ++i) {
    if (cmds[i].sheet != MAP_LAYER_SHEET_PHYS0 || cmds[i].sprite < 89 || cmds[i].sprite > 103) {
      continue;
    }
    for (int j = i + 1; j < n; ++j) {
      if (cmds[j].sheet == MAP_LAYER_SHEET_PHYS0 && cmds[j].sprite == cmds[i].sprite) {
        fprintf(stderr, "plow order: sprite %d blitted twice (%d and %d)\n", cmds[i].sprite, i, j);
        map_free(&m);
        return 1;
      }
    }
  }
  map_free(&m);

  /* Same, on a tile that really carries a special resource: a plowed resource
   * tile must list 149 once, below the resource icon. */
  ColonizeWorldMap r;
  memset(&r, 0, sizeof(r));
  r.width = 40;
  r.height = 40;
  r.tile_count = 1600;
  r.terrain = calloc(1600, 1);
  r.layer2 = calloc(1600, 1);
  r.layer3 = calloc(1600, 1);
  r.improve = calloc(1600, 1);
  r.seen = calloc(1600, 1);
  if (!r.terrain || !r.layer2 || !r.layer3 || !r.improve || !r.seen) {
    map_free(&r);
    return 1;
  }
  memset(r.terrain, 2, 1600); /* all Plains */
  int rx = -1;
  int ry = -1;
  int rlayer = -1;
  for (int y = 1; y < 39 && rx < 0; ++y) {
    for (int x = 1; x < 39 && rx < 0; ++x) {
      const int cnt = map_phys0_overlay_count(&r, x, y);
      for (int l = 0; l < cnt; ++l) {
        if (map_phys0_overlay_kind_at(&r, x, y, l) == MAP_OVERLAY_KIND_RESOURCE) {
          rx = x;
          ry = y;
          rlayer = l;
          break;
        }
      }
    }
  }
  if (rx < 0) {
    fprintf(stderr, "plow order: no procedural resource tile in the 40x40 fixture\n");
    map_free(&r);
    return 1;
  }
  const int res_sprite = map_phys0_overlay_sprite_at(&r, rx, ry, rlayer);
  map_tile_set_plowed(&r, rx, ry, true);
  ColonizeMapLayerCmd rc[MAP_LAYER_CMDS_MAX];
  const int rn = map_tile_layer_cmds(&r, rx, ry, 0, rc, MAP_LAYER_CMDS_MAX);
  int rplow = -1;
  int rres = -1;
  int rres_count = 0;
  int rplow_count = 0;
  for (int i = 0; i < rn; ++i) {
    if (rc[i].sheet != MAP_LAYER_SHEET_PHYS0) {
      continue;
    }
    if (rc[i].sprite == 149) {
      if (rplow < 0) {
        rplow = i;
      }
      ++rplow_count;
    }
    if (rc[i].sprite == res_sprite) {
      if (rres < 0) {
        rres = i;
      }
      ++rres_count;
    }
  }
  if (rplow_count != 1 || rres_count != 1 || rplow < 0 || rres < 0 || rplow > rres) {
    fprintf(
      stderr,
      "plow order (resource tile %d,%d): plow x%d at %d, resource %d x%d at %d\n", rx, ry,
      rplow_count, rplow, res_sprite, rres_count, rres
    );
    map_free(&r);
    return 1;
  }
  map_free(&r);
  return 0;
}

static const TestCase k_cases[] = {
  {"map_load_and_ocean", case_map_load_and_ocean},
  {"amer2_fixtures", case_amer2_fixtures},
  {"coast_overlays", case_coast_overlays},
  {"scrub_forest", case_scrub_forest},
  {"land_transitions", case_land_transitions},
  {"resources_rumours", case_resources_rumours},
  {"river_chain", case_river_chain},
  {"river_north", case_river_north},
  {"river_major", case_river_major},
  {"river_estuary", case_river_estuary},
  {"plow_road_overlay", case_plow_road_overlay},
  {"fog_edges", case_fog_edges},
  {"lumber_reward_column", case_lumber_reward_column},
  {"plow_layer_order", case_plow_layer_order},
};

TEST_MAIN(k_cases)
