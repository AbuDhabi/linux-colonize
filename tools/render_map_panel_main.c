/*
 * render_map_panel: standalone main-map sidebar renderer for golden comparison.
 *
 * Loads a Col1 .SAV, bridges it into live map/units/colonies pools the same way
 * the game does on load (col1_bridge_apply), points the panel at a tile (and
 * optionally selects the unit standing there), then calls map_panel_render()
 * directly — no SDL — dumping the 320x200 indexed framebuffer through the
 * terrain palette as a binary PPM.
 *
 *   render_map_panel <data_dir> <save.SAV> <x> <y> <select_unit:0|1> <out.ppm>
 *                    [load=<cargo>:<amount>,...]
 *
 *   select_unit 0 reproduces View Pieces (cursor on the tile, no unit block),
 *   1 reproduces Move Pieces with the tile's top unit active.
 *   load=      fills the selected transport's holds (@CARGO index : amount) so
 *              the "With:" row can be checked without a save that has one —
 *              e.g. load=1:100,4:37 is a full sugar hold and a part-full furs
 *              hold, which DOS draws colored and grey respectively.
 *
 * See docs/assets.md for the sidebar layout this tool is used to check.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/font.h"
#include "core/map.h"
#include "core/map_panel.h"
#include "core/ss.h"
#include "core/unit_chrome.h"
#include "core/units.h"
#include "platform/platform.h"

static bool load_sheet(const char* dir, const char* name, ColonizeSpriteSheet* out) {
  char path[512];
  char err[256];
  memset(out, 0, sizeof(*out));
  if (!dos_compat_normalize_asset_path(dir, name, path, sizeof(path))) {
    return false;
  }
  if (!ss_load(path, out, err, sizeof(err))) {
    fprintf(stderr, "ss_load %s warning: %s\n", name, err);
    return false;
  }
  return true;
}

static bool load_msg(const char* dir, const char* name, ColonizeMsgCatalog* out) {
  char path[512];
  memset(out, 0, sizeof(*out));
  if (!dos_compat_normalize_asset_path(dir, name, path, sizeof(path))) {
    return false;
  }
  return assets_msg_load_file(out, path);
}

int main(int argc, char** argv) {
  if (argc < 7) {
    fprintf(
      stderr,
      "usage: %s <data_dir> <save.SAV> <x> <y> <select_unit:0|1> <out.ppm> "
      "[load=<cargo>:<amount>,...]\n",
      argv[0]
    );
    return 1;
  }
  const char* data_dir = argv[1];
  const char* save_path = argv[2];
  const int tile_x = atoi(argv[3]);
  const int tile_y = atoi(argv[4]);
  const bool select_unit = atoi(argv[5]) != 0;
  const char* out_path = argv[6];

  char err[256];

  ColonizeCol1Save save;
  memset(&save, 0, sizeof(save));
  if (!col1_save_read_file(save_path, &save, err, sizeof(err))) {
    fprintf(stderr, "col1_save_read_file failed: %s\n", err);
    return 1;
  }
  const int human = col1_save_human_nation(&save);

  ColonizeMsgCatalog names;
  ColonizeMsgCatalog labels;
  if (!load_msg(data_dir, "NAMES.TXT", &names)) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    return 1;
  }
  const bool labels_ok = load_msg(data_dir, "LABELS.TXT", &labels);

  ColonizeUnitPool units_pool;
  ColonizeColonyPool colonies_pool;
  memset(&units_pool, 0, sizeof(units_pool));
  memset(&colonies_pool, 0, sizeof(colonies_pool));
  colonies_init(&colonies_pool);
  units_load_types(&units_pool, &names);
  colonies_load_buildings(&colonies_pool, &names);
  unit_chrome_load_orders(&names);

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  if (!europe_load(&europe, data_dir, err, sizeof(err))) {
    fprintf(stderr, "europe_load failed: %s\n", err);
    return 1;
  }

  ColonizeCol1BridgeResult bridge;
  memset(&bridge, 0, sizeof(bridge));
  if (!col1_bridge_apply(
        &save, &map, &units_pool, &colonies_pool, &europe, &bridge, err, sizeof(err)
      )) {
    fprintf(stderr, "col1_bridge_apply failed: %s\n", err);
    return 1;
  }

  MapPanel panel;
  memset(&panel, 0, sizeof(panel));
  if (!map_panel_load(&panel, data_dir, labels_ok ? &labels : NULL)) {
    fprintf(stderr, "map_panel_load warning (WOODTILE.SS missing)\n");
  }

  ColonizeSpriteSheet icons;
  const bool icons_ok = load_sheet(data_dir, "ICONS.SS", &icons);

  ColonizeFont font;
  memset(&font, 0, sizeof(font));
  bool font_ok = false;
  char ff_path[512];
  if (dos_compat_normalize_asset_path(data_dir, "FONTTINY.FF", ff_path, sizeof(ff_path))) {
    font_ok = ff_load(ff_path, &font, err, sizeof(err));
    if (!font_ok) {
      fprintf(stderr, "ff_load warning: %s\n", err);
    }
  }

  int selected = -1;
  if (select_unit) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units_pool.units[i];
      if (units_is_on_map(u) && u->x == tile_x && u->y == tile_y && u->nation_id == human) {
        selected = u->id;
        break;
      }
    }
    if (selected < 0) {
      fprintf(stderr, "no human unit at (%d,%d); rendering View Pieces\n", tile_x, tile_y);
    }
  }

  if (selected >= 0 && argc > 7 && strncmp(argv[7], "load=", 5) == 0) {
    ColonizeUnit* ship = units_get(&units_pool, selected);
    if (ship) {
      const char* p = argv[7] + 5;
      int slot = 0;
      while (*p && slot < COLONIZE_UNIT_CARGO_MAX) {
        int type = 0;
        int amt = 0;
        if (sscanf(p, "%d:%d", &type, &amt) != 2) {
          break;
        }
        ship->hold_goods_type[slot] = type;
        ship->hold_goods_amount[slot] = amt;
        slot++;
        const char* comma = strchr(p, ',');
        if (!comma) {
          break;
        }
        p = comma + 1;
      }
    }
  }

  int view_x = 0;
  int view_y = 0;
  map_panel_clamp_view_origin(
    (int)map.width,
    (int)map.height,
    tile_x,
    tile_y,
    MAP_VIEW_TILE_COLS,
    MAP_VIEW_TILE_ROWS,
    &view_x,
    &view_y
  );

  uint8_t pixels[320 * 200];
  memset(pixels, 0, sizeof(pixels));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};

  ColonizeSpriteSheet terrain;
  const bool terrain_ok = load_sheet(data_dir, "TERRAIN.SS", &terrain);
  const ColonizePalette* pal_ptr =
    (terrain_ok && terrain.has_palette) ? &terrain.palette : NULL;

  map_panel_render(
    &panel,
    &map,
    &units_pool,
    &colonies_pool,
    icons_ok ? &icons : NULL,
    font_ok ? &font : NULL,
    &names,
    labels_ok ? &labels : NULL,
    &save,
    view_x,
    view_y,
    MAP_VIEW_TILE_COLS,
    MAP_VIEW_TILE_ROWS,
    tile_x,
    tile_y,
    selected,
    human,
    bridge.year,
    bridge.autumn,
    (int)save.nation[human].gold,
    (int)save.nation[human].tax_rate,
    save.player[human].country_name,
    pal_ptr,
    selected < 0,
    true,
    &fb
  );

  ColonizePalette pal = (ColonizePalette){0};
  if (pal_ptr) {
    pal = *pal_ptr;
  }

  FILE* f = fopen(out_path, "wb");
  if (!f) {
    fprintf(stderr, "cannot open %s for writing\n", out_path);
    return 1;
  }
  fprintf(f, "P6\n320 200\n255\n");
  for (int i = 0; i < 320 * 200; ++i) {
    const uint8_t idx = pixels[i];
    const unsigned char rgb[3] = {pal.rgb[idx][0], pal.rgb[idx][1], pal.rgb[idx][2]};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  fprintf(stderr, "wrote %s (tile=%d,%d selected=%d human=%d)\n", out_path, tile_x, tile_y, selected, human);
  return 0;
}
