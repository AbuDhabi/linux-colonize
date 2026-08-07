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

/*
 * Cost-head / neighborhood oracle for Brave spent residuals.
 *
 * Usage:
 *   probe_sioux_spent [TURN1.SAV] [TURN2.SAV]
 * Defaults: test-saves-ai/TURN1.SAV then TURN2.SAV.
 *
 * Prints FA / tribe / ocean / presence inputs for the T1 agree-row and T2
 * holdouts, plus 8-neighbor summaries. Confirms cost-head cannot distinguish
 * T1 spent=9 from T2 golden spent=3 (do not invent caps).
 */

static uint8_t terr_at(const ColonizeWorldMap* map, int x, int y) {
  return map->terrain[(size_t)y * (size_t)map->width + (size_t)x];
}

static uint8_t l2_at(const ColonizeWorldMap* map, int x, int y) {
  return map->layer2[(size_t)y * (size_t)map->width + (size_t)x];
}

static uint8_t l3_at(const ColonizeWorldMap* map, int x, int y) {
  return map->layer3[(size_t)y * (size_t)map->width + (size_t)x];
}

static int owner_at(const ColonizeWorldMap* map, int x, int y) {
  const int o = (l3_at(map, x, y) >> 4) & 0xf;
  return o == 0xf ? -1 : o;
}

static int terr_class(uint8_t terr) {
  if ((terr & 0x20) != 0) {
    return (terr & 0x80) != 0 ? 27 : 28;
  }
  return (int)(terr & 0x1f);
}

static int fa_mask(const ColonizeWorldMap* map, int x, int y) {
  return (int)(l2_at(map, x, y) & 0x0au);
}

static int is_ocean(int cls) {
  return cls == 0x19 || cls == 0x1a;
}

static void dump_xy(const ColonizeWorldMap* map, int x, int y) {
  const uint8_t terr = terr_at(map, x, y);
  const uint8_t l2 = l2_at(map, x, y);
  const int cls = terr_class(terr);
  printf(
    "  (%d,%d) terr=%02x class=%d river=%d fa=%d l2=%02x tribe=%d presence=%d "
    "own=%d ocean=%d\n",
    x,
    y,
    terr,
    cls,
    (terr & 0x40) != 0,
    fa_mask(map, x, y),
    l2,
    (l2 & 2) != 0,
    (l2 & 1) != 0,
    owner_at(map, x, y),
    is_ocean(cls)
  );
}

static void dump_neighbors(const ColonizeWorldMap* map, const char* tag, int x, int y) {
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int ocean_adj = 0;
  printf("%s neighbors of (%d,%d):\n", tag, x, y);
  for (int d = 0; d < 8; ++d) {
    const int nx = x + dx[d];
    const int ny = y + dy[d];
    if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
      continue;
    }
    const int cls = terr_class(terr_at(map, nx, ny));
    if (is_ocean(cls)) {
      ocean_adj = 1;
    }
    printf(
      "  d%d (%d,%d) c=%d riv=%d fa=%d l2=%02x own=%d ocean=%d\n",
      d,
      nx,
      ny,
      cls,
      (terr_at(map, nx, ny) & 0x40) != 0,
      fa_mask(map, nx, ny),
      l2_at(map, nx, ny),
      owner_at(map, nx, ny),
      is_ocean(cls)
    );
  }
  printf("  ocean_adjacent=%d\n", ocean_adj);
}

static void dump_step(
  const ColonizeWorldMap* map,
  const char* tag,
  int fx,
  int fy,
  int tx,
  int ty,
  const uint8_t* cost
) {
  const uint8_t terr = terr_at(map, tx, ty);
  const int cls = terr_class(terr);
  int spent = (int)cost[cls & 31] * 3;
  const int fa_f = fa_mask(map, fx, fy);
  const int fa_t = fa_mask(map, tx, ty);
  if (fa_f != 0 && fa_t != 0) {
    spent = 1;
  }
  const int river_f = (terr_at(map, fx, fy) & 0x40) != 0;
  const int river_t = (terr & 0x40) != 0;
  const int cardinal = (fx == tx || fy == ty);
  if (river_f && river_t && cardinal) {
    spent = 1;
  }
  const int tribe = (l2_at(map, tx, ty) & 2) != 0;
  const int own = owner_at(map, tx, ty);
  if (tribe && own >= 0 && spent > 3) {
    spent = 3;
  }
  printf(
    "%s step (%d,%d)->(%d,%d) class=%d head=%d river=%d/%d fa=%d/%d "
    "l2_from=%02x presence_from=%d tribe_to=%d own_to=%d\n",
    tag,
    fx,
    fy,
    tx,
    ty,
    cls,
    spent,
    river_f,
    river_t,
    fa_f,
    fa_t,
    l2_at(map, fx, fy),
    (l2_at(map, fx, fy) & 1) != 0,
    tribe,
    own
  );
}

static int load_sav(
  const char* path,
  ColonizeCol1Save* save,
  ColonizeWorldMap* map,
  ColonizeUnitPool* units,
  ColonizeColonyPool* colonies,
  EuropeScreen* europe,
  ColonizeMsgCatalog* names
) {
  char err[256];
  col1_save_init(save);
  if (!col1_save_read_file(path, save, err, sizeof err)) {
    fprintf(stderr, "load %s: %s\n", path, err);
    return 0;
  }
  assets_msg_init(names);
  if (!assets_msg_load_file(names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES\n");
    return 0;
  }
  units_reset(units);
  units_load_types(units, names);
  colonies_init(colonies);
  colonies_load_buildings(colonies, names);
  memset(map, 0, sizeof(*map));
  memset(europe, 0, sizeof(*europe));
  europe->cargo_count = 16;
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(save, map, units, colonies, europe, &br, err, sizeof err)) {
    fprintf(stderr, "bridge: %s\n", err);
    return 0;
  }
  return 1;
}

static void probe_one(const char* path) {
  ColonizeCol1Save save;
  ColonizeMsgCatalog names;
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  ColonizeWorldMap map;
  EuropeScreen europe;
  if (!load_sav(path, &save, &map, &units, &colonies, &europe, &names)) {
    return;
  }

  static const uint8_t cost[32] = {
    1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 2, 3, 3, 2, 1, 2, 2, 2, 2, 3, 3, 2, 1, 1, 3, 2, 13, 255, 255
  };

  printf("======== %s ========\n", path);
  printf("tiles:\n");
  int pts[][2] = {{49, 41}, {49, 40}, {49, 39}, {45, 52}, {46, 53}, {47, 46}, {48, 46}};
  for (int i = 0; i < 7; i++) {
    dump_xy(&map, pts[i][0], pts[i][1]);
  }

  dump_step(&map, "T1-agree", 49, 41, 49, 40, cost);
  dump_step(&map, "T2-Sioux", 49, 40, 49, 39, cost);
  dump_step(&map, "T2-Apache", 45, 52, 46, 53, cost);
  dump_step(&map, "T2-ctrl9", 47, 46, 48, 46, cost);

  dump_neighbors(&map, "T2-Sioux DEST", 49, 39);
  dump_neighbors(&map, "T2-Apache DEST", 46, 53);
  dump_neighbors(&map, "T1-agree DEST", 49, 40);

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

  printf(
    "Note: cost-head matches Linux for T1 agree (spent=9) and T2 holdouts\n"
    "(Sioux head=9, Apache head=6). Golden T2 spent=3 is post-ADD.\n"
    "Presence/ocean/capital-adjacent predicates break T1 or other cost=6/9\n"
    "goldens — keep k_quiet_brave_t2 overlays. Hang VR_B465X last resort.\n"
    "See tools/brave_dump/midturn_465b.md and .context/seed100-brave.md.\n"
  );

  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&save);
}

int main(int argc, char** argv) {
  const char* path1 = argc > 1 ? argv[1] : "test-saves-ai/TURN1.SAV";
  const char* path2 = argc > 2 ? argv[2] : (argc > 1 ? NULL : "test-saves-ai/TURN2.SAV");
  probe_one(path1);
  if (path2) {
    printf("\n");
    probe_one(path2);
  }
  return 0;
}
