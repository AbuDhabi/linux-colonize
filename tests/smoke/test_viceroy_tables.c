#include <stdio.h>

#include "data/viceroy_tables.h"

int main(void) {
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

  fprintf(stderr, "viceroy table tests ok\n");
  return 0;
}
