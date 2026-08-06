#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/units.h"
#include "platform/platform.h"

static void dump_xy(const ColonizeWorldMap* map, int x, int y) {
  size_t i = (size_t)y * (size_t)map->width + (size_t)x;
  uint8_t terr = map->terrain[i];
  uint8_t l2 = map->layer2[i];
  uint8_t l3 = map->layer3[i];
  int own = ((l3 >> 4) & 0xf) == 0xf ? -1 : (int)((l3 >> 4) & 0xf);
  int cls = (terr & 0x20) ? ((terr & 0x80) ? 27 : 28) : (terr & 0x1f);
  printf(
    "  (%d,%d) terr=%02x class=%d river=%d l2=%02x tribe=%d own=%d ocean=%d\n",
    x,
    y,
    terr,
    cls,
    (terr & 0x40) != 0,
    l2,
    (l2 & 2) != 0,
    own,
    cls == 0x19 || cls == 0x1a
  );
}

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "test-saves-ai/TURN2.SAV";
  ColonizeCol1Save save;
  char err[256];
  col1_save_init(&save);
  if (!col1_save_read_file(path, &save, err, sizeof err)) {
    fprintf(stderr, "load %s: %s\n", path, err);
    return 1;
  }
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES\n");
    return 1;
  }
  ColonizeUnitPool units;
  units_reset(&units);
  units_load_types(&units, &names);
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_load_buildings(&colonies, &names);
  ColonizeWorldMap map;
  memset(&map, 0, sizeof map);
  EuropeScreen europe;
  memset(&europe, 0, sizeof europe);
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(&save, &map, &units, &colonies, &europe, &br, err, sizeof err)) {
    fprintf(stderr, "bridge: %s\n", err);
    return 1;
  }
  printf("%s tiles:\n", path);
  int pts[][2] = {{49, 41}, {49, 40}, {49, 39}, {45, 52}, {46, 53}, {47, 54}};
  for (int i = 0; i < 6; i++) {
    dump_xy(&map, pts[i][0], pts[i][1]);
  }
  static const uint8_t cost[32] = {
    1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 2, 3, 3,
    2, 1, 2, 2, 2, 2, 3, 3, 2, 1, 1, 3, 2, 13, 255, 255
  };
  struct {
    int fx, fy, tx, ty;
  } steps[] = {{49, 40, 49, 39}, {45, 52, 46, 53}, {49, 41, 49, 40}};
  for (int s = 0; s < 3; s++) {
    int fx = steps[s].fx, fy = steps[s].fy, tx = steps[s].tx, ty = steps[s].ty;
    size_t ti = (size_t)ty * map.width + (size_t)tx;
    size_t fi = (size_t)fy * map.width + (size_t)fx;
    uint8_t terr = map.terrain[ti];
    int cls = (terr & 0x20) ? ((terr & 0x80) ? 27 : 28) : (terr & 0x1f);
    int spent = cost[cls] * 3;
    int rf = map.terrain[fi] & 0x40, rt = terr & 0x40;
    int own =
      ((map.layer3[ti] >> 4) & 0xf) == 0xf ? -1 : (int)((map.layer3[ti] >> 4) & 0xf);
    printf(
      "step (%d,%d)->(%d,%d) class=%d spent=%d river=%d/%d l2=%02x own_to=%d\n",
      fx,
      fy,
      tx,
      ty,
      cls,
      spent,
      !!rf,
      !!rt,
      map.layer2[ti],
      own
    );
  }
  printf("Sioux/Apache units:\n");
  for (int i = 0; i < units.unit_count; i++) {
    ColonizeUnit* u = &units.units[i];
    if (!u->active) {
      continue;
    }
    if (u->nation_id == 10 || u->nation_id == 7) {
      printf(
        "  n=%d t=%d (%d,%d) mv=%d tw=%d\n",
        u->nation_id,
        u->type_index,
        u->x,
        u->y,
        u->moves_left,
        u->turns_worked
      );
    }
  }
  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&save);
  return 0;
}
