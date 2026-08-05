#include "core/ai.h"
#include "core/col1_save.h"
#include "core/map.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Smoke: col1_kill_indian_nation removes villages + units for one native nation.
 */
int main(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.tribe_count = 3;
  col1.tribe = calloc(3, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    fprintf(stderr, "alloc tribe failed\n");
    return 1;
  }
  col1.tribe[0].x = 10;
  col1.tribe[0].y = 10;
  col1.tribe[0].nation_id = 4; /* Inca */
  col1.tribe[1].x = 12;
  col1.tribe[1].y = 10;
  col1.tribe[1].nation_id = 5; /* Aztec — keep */
  col1.tribe[2].x = 14;
  col1.tribe[2].y = 10;
  col1.tribe[2].nation_id = 4; /* Inca */

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 20;
  map.height = 20;
  map.tile_count = 400;
  map.layer3 = calloc(400, 1);
  if (!map.layer3) {
    fprintf(stderr, "alloc layer3 failed\n");
    col1_save_free(&col1);
    return 1;
  }
  map.layer3[10 * 20 + 10] = (uint8_t)((4u << 4) | 1u);
  map.layer3[10 * 20 + 14] = (uint8_t)((4u << 4) | 1u);
  map.layer3[10 * 20 + 12] = (uint8_t)((5u << 4) | 1u);

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Brave");
  units.types[0].movement = 3;

  const int id0 = units_spawn_allow_stack(&units, 0, 10, 10);
  const int id1 = units_spawn_allow_stack(&units, 0, 12, 12);
  ColonizeUnit* u0 = units_get(&units, id0);
  ColonizeUnit* u1 = units_get(&units, id1);
  if (!u0 || !u1) {
    fprintf(stderr, "spawn failed (%d %d)\n", id0, id1);
    free(map.layer3);
    col1_save_free(&col1);
    return 1;
  }
  u0->nation_id = 4;
  u0->home_tribe_id = 0;
  u1->nation_id = 5;
  u1->home_tribe_id = 1;

  const int removed = col1_kill_indian_nation(&col1, &units, &map, 4);
  if (removed != 2) {
    fprintf(stderr, "expected 2 villages removed, got %d\n", removed);
    free(map.layer3);
    col1_save_free(&col1);
    return 1;
  }
  if (col1.head.tribe_count != 1 || col1.tribe[0].nation_id != 5) {
    fprintf(stderr, "Aztec village should remain alone\n");
    free(map.layer3);
    col1_save_free(&col1);
    return 1;
  }
  u0 = units_get(&units, id0);
  u1 = units_get(&units, id1);
  if (u0 && u0->active) {
    fprintf(stderr, "Inca unit should be despawned\n");
    free(map.layer3);
    col1_save_free(&col1);
    return 1;
  }
  if (!u1 || !u1->active || u1->home_tribe_id != 0) {
    fprintf(
      stderr,
      "Aztec unit should survive with remapped home_tribe_id=0 (active=%d home=%d)\n",
      u1 ? u1->active : 0,
      u1 ? u1->home_tribe_id : -99
    );
    free(map.layer3);
    col1_save_free(&col1);
    return 1;
  }
  if ((map.layer3[10 * 20 + 10] >> 4) != 0x0f || (map.layer3[10 * 20 + 14] >> 4) != 0x0f) {
    fprintf(stderr, "Inca village tiles should be unowned (0xf)\n");
    free(map.layer3);
    col1_save_free(&col1);
    return 1;
  }
  if ((map.layer3[10 * 20 + 12] >> 4) != 5) {
    fprintf(stderr, "Aztec village ownership should remain\n");
    free(map.layer3);
    col1_save_free(&col1);
    return 1;
  }

  free(map.layer3);
  col1_save_free(&col1);
  printf("smoke_kill_indians ok\n");
  return 0;
}
