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

#include "core/colony_screen.h"

#include "tools/render_common.h"

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

  RenderSaveBundle rs;
  if (!render_load_save(data_dir, save_path, &rs)) {
    return 1;
  }
  const int human = rs.human;

  ColonyScreenView view;
  memset(&view, 0, sizeof(view));
  if (!colony_screen_load(&view, data_dir, err, sizeof(err))) {
    fprintf(stderr, "colony_screen_load failed: %s\n", err);
    return 1;
  }
  view.multi_mode = (ColonyMultiMode)multi_mode;

  const ColonizeColony* colony = NULL;
  for (int i = 0; i < rs.colonies.colony_count; ++i) {
    const ColonizeColony* c = &rs.colonies.colonies[i];
    if (strcmp(c->name, colony_name) == 0) {
      colony = c;
      break;
    }
  }
  if (!colony) {
    fprintf(stderr, "colony '%s' not found in save\n", colony_name);
    return 1;
  }

  colony_screen_refresh_transports(&view, &rs.units, colony);
  colony_screen_refresh_outside(&view, &rs.units, colony);
  colony_screen_refresh_preview(&view, &rs.colonies, colony, &rs.map, &rs.save);

  ColonizeSpriteSheet terrain;
  const bool terrain_ok = render_load_sheet(data_dir, "TERRAIN.SS", &terrain);

  ColonizeSpriteSheet phys0;
  const bool phys0_ok = render_load_sheet(data_dir, "PHYS0.SS", &phys0);

  ColonizeFont font;
  const bool font_ok = render_load_font(data_dir, "FONTTINY.FF", &font);

  const ColonizeCol1Nation* nat = &rs.save.nation[colony->nation_id];

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb;
  render_fb_init(&fb, pixels);

  colony_screen_render(
    &view,
    &rs.colonies,
    colony,
    &rs.units,
    &rs.map,
    terrain_ok ? &terrain : NULL,
    phys0_ok ? &phys0 : NULL,
    &rs.save,
    rs.bridge.year,
    rs.bridge.autumn,
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

  if (!render_write_ppm(out_path, pixels, &pal)) {
    return 1;
  }
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
