/*
 * render_colony: standalone colony-screen renderer for golden comparison.
 *
 * Loads a Col1 .SAV, bridges it into live map/units/colonies pools the same
 * way the game does on load (col1_bridge_apply), finds a named colony, and
 * calls colony_screen_render() directly (no SDL/xvfb) — dumping the
 * resulting 320x200 indexed framebuffer, expanded through the frame's
 * palette, as a binary PPM.
 *
 *   render_colony <data_dir> <save.SAV> <colony_name> <multi_mode> <out.ppm>
 *
 *   data_dir      usually "COLONIZE"
 *   save.SAV      a Col1 .SAV to load
 *   colony_name   exact colony name to render (e.g. "New Amsterdam")
 *   multi_mode    0 Production  1 Units(military)  2 Construction
 *   out.ppm       output path; convert to PNG with `convert out.ppm out.png`
 *
 * See docs/report_screens.md / docs/colony_screen.md for the golden
 * comparison workflow this tool is part of.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_screen.h"
#include "core/europe.h"
#include "core/ff.h"
#include "core/map.h"
#include "core/ss.h"
#include "core/unit_chrome.h"
#include "core/units.h"
#include "platform/platform.h"

int main(int argc, char** argv) {
  if (argc < 6) {
    fprintf(
      stderr,
      "usage: %s <data_dir> <save.SAV> <colony_name> <multi_mode:0=prod,1=units,2=construct> "
      "<out.ppm> [debug_rects:0|1]\n",
      argv[0]
    );
    return 1;
  }
  const char* data_dir = argv[1];
  const char* save_path = argv[2];
  const char* colony_name = argv[3];
  const int multi_mode = atoi(argv[4]);
  const char* out_path = argv[5];
  const bool debug_rects = argc > 6 && atoi(argv[6]) != 0;

  char err[256];

  ColonizeCol1Save save;
  memset(&save, 0, sizeof(save));
  if (!col1_save_read_file(save_path, &save, err, sizeof(err))) {
    fprintf(stderr, "col1_save_read_file failed: %s\n", err);
    return 1;
  }
  const int human = save.head.human_player;

  ColonyScreenView view;
  memset(&view, 0, sizeof(view));
  if (!colony_screen_load(&view, data_dir, err, sizeof(err))) {
    fprintf(stderr, "colony_screen_load failed: %s\n", err);
    return 1;
  }
  view.multi_mode = (ColonyMultiMode)multi_mode;

  ColonizeMsgCatalog names;
  memset(&names, 0, sizeof(names));
  char names_path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    return 1;
  }

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
  bool europe_ok = europe_load(&europe, data_dir, err, sizeof(err));
  if (!europe_ok) {
    fprintf(stderr, "europe_load warning: %s\n", err);
  } else {
    europe.harbor_ships = 0;
    europe.dock_count = 0;
  }

  ColonizeCol1BridgeResult bridge_result;
  memset(&bridge_result, 0, sizeof(bridge_result));
  if (!europe_ok ||
      !col1_bridge_apply(
        &save, &map, &units_pool, &colonies_pool, &europe, &bridge_result, err, sizeof(err)
      )) {
    fprintf(stderr, "col1_bridge_apply failed: %s\n", err);
    return 1;
  }

  const ColonizeColony* colony = NULL;
  for (int i = 0; i < colonies_pool.colony_count; ++i) {
    const ColonizeColony* c = &colonies_pool.colonies[i];
    if (strcmp(c->name, colony_name) == 0) {
      colony = c;
      break;
    }
  }
  if (!colony) {
    fprintf(stderr, "colony '%s' not found in save\n", colony_name);
    return 1;
  }

  colony_screen_refresh_transports(&view, &units_pool, colony);
  colony_screen_refresh_outside(&view, &units_pool, colony);
  colony_screen_refresh_preview(&view, &colonies_pool, colony, &map, &save);

  ColonizeSpriteSheet terrain;
  memset(&terrain, 0, sizeof(terrain));
  char terrain_path[512];
  bool terrain_ok = false;
  if (dos_compat_normalize_asset_path(data_dir, "TERRAIN.SS", terrain_path, sizeof(terrain_path))) {
    terrain_ok = ss_load(terrain_path, &terrain, err, sizeof(err));
    if (!terrain_ok) {
      fprintf(stderr, "ss_load TERRAIN.SS warning: %s\n", err);
    }
  }

  ColonizeSpriteSheet phys0;
  memset(&phys0, 0, sizeof(phys0));
  char phys0_path[512];
  bool phys0_ok = false;
  if (dos_compat_normalize_asset_path(data_dir, "PHYS0.SS", phys0_path, sizeof(phys0_path))) {
    phys0_ok = ss_load(phys0_path, &phys0, err, sizeof(err));
    if (!phys0_ok) {
      fprintf(stderr, "ss_load PHYS0.SS warning: %s\n", err);
    }
  }

  ColonizeFont font;
  memset(&font, 0, sizeof(font));
  char ff_path[512];
  bool font_ok = false;
  if (dos_compat_normalize_asset_path(data_dir, "FONTTINY.FF", ff_path, sizeof(ff_path))) {
    font_ok = ff_load(ff_path, &font, err, sizeof(err));
    if (!font_ok) {
      fprintf(stderr, "ff_load warning: %s\n", err);
    }
  }

  const ColonizeCol1Nation* nat = &save.nation[colony->nation_id];

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};

  colony_screen_render(
    &view,
    &colonies_pool,
    colony,
    &units_pool,
    &map,
    terrain_ok ? &terrain : NULL,
    phys0_ok ? &phys0 : NULL,
    &save,
    bridge_result.year,
    bridge_result.autumn,
    (int)nat->gold,
    font_ok ? &font : NULL,
    debug_rects,
    NULL, /* LABELS.TXT not loaded here; fallback text is byte-identical to the live text */
    &fb
  );

  ColonizePalette pal = (ColonizePalette){0};
  if (view.frame_ok && view.frame.has_palette) {
    pal = view.frame.palette;
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
  fprintf(
    stderr,
    "wrote %s (colony=%s multi_mode=%d human=%d)\n",
    out_path,
    colony_name,
    multi_mode,
    human
  );
  (void)human;
  return 0;
}
