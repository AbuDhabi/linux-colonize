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

#include "core/map_panel.h"

#include "tools/render_common.h"

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

  RenderSaveBundle rs;
  if (!render_load_save(data_dir, save_path, &rs)) {
    return 1;
  }
  const int human = rs.human;

  MapPanel panel;
  memset(&panel, 0, sizeof(panel));
  if (!map_panel_load(&panel, data_dir, rs.labels_ok ? &rs.labels : NULL)) {
    fprintf(stderr, "map_panel_load warning (WOODTILE.SS missing)\n");
  }

  ColonizeSpriteSheet icons;
  const bool icons_ok = render_load_sheet(data_dir, "ICONS.SS", &icons);

  ColonizeFont font;
  const bool font_ok = render_load_font(data_dir, "FONTTINY.FF", &font);

  int selected = -1;
  if (select_unit) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &rs.units.units[i];
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
    ColonizeUnit* ship = units_get(&rs.units, selected);
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
    (int)rs.map.width,
    (int)rs.map.height,
    tile_x,
    tile_y,
    MAP_VIEW_TILE_COLS,
    MAP_VIEW_TILE_ROWS,
    &view_x,
    &view_y
  );

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb;
  render_fb_init(&fb, pixels);

  ColonizeSpriteSheet terrain;
  const bool terrain_ok = render_load_sheet(data_dir, "TERRAIN.SS", &terrain);
  const ColonizePalette* pal_ptr =
    (terrain_ok && terrain.has_palette) ? &terrain.palette : NULL;

  map_panel_render(
    &panel,
    &rs.map,
    &rs.units,
    &rs.colonies,
    icons_ok ? &icons : NULL,
    font_ok ? &font : NULL,
    &rs.names,
    rs.labels_ok ? &rs.labels : NULL,
    &rs.save,
    view_x,
    view_y,
    MAP_VIEW_TILE_COLS,
    MAP_VIEW_TILE_ROWS,
    tile_x,
    tile_y,
    selected,
    human,
    rs.bridge.year,
    rs.bridge.autumn,
    (int)rs.save.nation[human].gold,
    (int)rs.save.nation[human].tax_rate,
    rs.save.player[human].country_name,
    pal_ptr,
    selected < 0,
    true,
    &fb
  );

  ColonizePalette pal = (ColonizePalette){0};
  if (pal_ptr) {
    pal = *pal_ptr;
  }

  if (!render_write_ppm(out_path, pixels, &pal)) {
    return 1;
  }
  fprintf(stderr, "wrote %s (tile=%d,%d selected=%d human=%d)\n", out_path, tile_x, tile_y, selected, human);
  return 0;
}
