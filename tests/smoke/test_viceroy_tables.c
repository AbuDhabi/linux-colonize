#include <stdio.h>

#include "data/viceroy_tables.h"

int main(void) {
  if (viceroy_ds_to_file_offset != 0x186e6u) {
    fprintf(stderr, "unexpected ds_to_file offset 0x%x\n", viceroy_ds_to_file_offset);
    return 1;
  }

  if (viceroy_terrain_meta[0][0] != 0 || viceroy_terrain_meta[0][1] != 100 || viceroy_terrain_meta[0][3] != 10) {
    fprintf(stderr, "unexpected tundra terrain_meta header\n");
    return 1;
  }

  if (viceroy_terrain_class(25) != VICEROY_TERRAIN_CLASS_LAND) {
    fprintf(stderr, "ocean class expected %u got %u\n", VICEROY_TERRAIN_CLASS_LAND, viceroy_terrain_class(25));
    return 1;
  }

  if (viceroy_terrain_class(27) != VICEROY_TERRAIN_CLASS_MOUNTAIN) {
    fprintf(stderr, "mountain class mismatch\n");
    return 1;
  }

  if (viceroy_terrain_class(28) != VICEROY_TERRAIN_CLASS_HILLS) {
    fprintf(stderr, "hills class mismatch\n");
    return 1;
  }

  static const uint8_t expected_river[VICEROY_RIVER_TRANSITION_COUNT] = {
    7, 4, 2, 1, 1, 0, 0, 7, 11, 13, 14, 0, 23, 44, 53, 73, 75, 0, 27, 22, 37, 18, 48, 0, 3, 20, 25, 5
  };
  for (size_t i = 0; i < VICEROY_RIVER_TRANSITION_COUNT; ++i) {
    if (viceroy_river_transition[i] != expected_river[i]) {
      fprintf(stderr, "river_transition[%zu] expected %u got %u\n", i, expected_river[i], viceroy_river_transition[i]);
      return 1;
    }
  }

  static const uint8_t expected_a[4] = {12, 8, 22, 5};
  static const uint8_t expected_b[4] = {17, 21, 25, 65};
  for (int i = 0; i < 4; ++i) {
    if (viceroy_feature_sprite_bases_a[i] != expected_a[i] ||
        viceroy_feature_sprite_bases_b[i] != expected_b[i]) {
      fprintf(stderr, "feature_sprite_bases mismatch at %d\n", i);
      return 1;
    }
  }

  static const uint8_t expected_conn[VICEROY_CONNECTIVITY_TRANSITION_COUNT] = {
    11, 11, 11, 10, 10, 10, 9, 9, 9, 17, 17, 12, 12, 12, 13, 13, 16, 16, 14, 14, 14
  };
  for (size_t i = 0; i < VICEROY_CONNECTIVITY_TRANSITION_COUNT; ++i) {
    if (viceroy_connectivity_transition[i] != expected_conn[i]) {
      fprintf(stderr, "connectivity_transition[%zu] mismatch\n", i);
      return 1;
    }
  }

  fprintf(stderr, "viceroy table tests ok\n");
  return 0;
}
