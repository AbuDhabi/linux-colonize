#include <stdio.h>

#include "data/viceroy_tables.h"

#include "../common/test_runner.h"

static int case_tundra_header(void) {
  if (viceroy_terrain_meta[0][0] != 0 || viceroy_terrain_meta[0][1] != 100 || viceroy_terrain_meta[0][3] != 10) {
    fprintf(stderr, "unexpected tundra terrain_meta header\n");
    return 1;
  }
  return 0;
}

static int case_terrain_class_land(void) {
  if (viceroy_terrain_class(25) != VICEROY_TERRAIN_CLASS_LAND) {
    fprintf(stderr, "ocean class expected %u got %u\n", VICEROY_TERRAIN_CLASS_LAND, viceroy_terrain_class(25));
    return 1;
  }
  return 0;
}

static int case_terrain_class_mountain(void) {
  if (viceroy_terrain_class(27) != VICEROY_TERRAIN_CLASS_MOUNTAIN) {
    fprintf(stderr, "mountain class mismatch\n");
    return 1;
  }
  return 0;
}

static int case_terrain_class_hills(void) {
  if (viceroy_terrain_class(28) != VICEROY_TERRAIN_CLASS_HILLS) {
    fprintf(stderr, "hills class mismatch\n");
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"case_tundra_header", case_tundra_header},
    {"case_terrain_class_land", case_terrain_class_land},
    {"case_terrain_class_mountain", case_terrain_class_mountain},
    {"case_terrain_class_hills", case_terrain_class_hills},
};
TEST_MAIN(k_cases)
