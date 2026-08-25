/*
 * render_report: standalone report-screen renderer for golden comparison.
 *
 * Calls reports_load()/reports_render() directly (bypasses SDL/xvfb entirely)
 * and dumps the resulting 320x200 indexed framebuffer, expanded through the
 * right palette, as a binary PPM. Convert/view with ImageMagick:
 *
 *   convert out.ppm out.png
 *
 * See docs/report_screens.md for the full report-porting workflow this tool
 * is part of (grid_overlay.sh / render_diff.sh live in scripts/).
 *
 *   render_report <data_dir> <save.SAV> <out.ppm> [report_id] [congress_page2] [labor_detail_job]
 *
 *   data_dir        usually "COLONIZE"
 *   save.SAV        a Col1 .SAV to load (report content needs one)
 *   report_id       ColonizeReportId, default 0 (Religious). See reports.h:
 *                     0 Religious  1 Congress  2 Labor  3 Economic
 *                     4 Colony     5 Naval      6 Foreign 7 Indian  8 Score
 *   congress_page2  1 to render Continental Congress page 2 (ignored
 *                   otherwise); default 0
 *   labor_detail_job  job id (0..27, see reports.c k_job_names) to render the
 *                   Labor report's zoomed detail view instead of the grid
 *                   (ignored otherwise); default -1
 *
 * Also prints the founding-fathers bells pool/need to stderr (useful when
 * working on the Congress bells bar) after seeding the pool the same way
 * the live game does on save load (founding_fathers_sync_from_col1_after_load).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_save.h"
#include "core/ff.h"
#include "core/founding_fathers.h"
#include "core/reports.h"
#include "platform/platform.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(
      stderr,
      "usage: %s <data_dir> <save.SAV> <out.ppm> [report_id] [congress_page2]\n",
      argv[0]
    );
    return 1;
  }
  const char* data_dir = argv[1];
  const char* save_path = argv[2];
  const char* out_path = argv[3];
  const int report_id = argc > 4 ? atoi(argv[4]) : COLONIZE_REPORT_RELIGIOUS;
  const bool congress_page2 = argc > 5 && atoi(argv[5]) != 0;
  const int labor_detail_job = argc > 6 ? atoi(argv[6]) : -1;

  char err[256];
  ColonizeReportsView view;
  reports_init(&view);
  if (!reports_load(&view, data_dir, err, sizeof(err))) {
    fprintf(stderr, "reports_load failed: %s\n", err);
    return 1;
  }

  ColonizeCol1Save save;
  memset(&save, 0, sizeof(save));
  if (!col1_save_read_file(save_path, &save, err, sizeof(err))) {
    fprintf(stderr, "col1_save_read_file failed: %s\n", err);
    return 1;
  }
  const int human = save.head.human_player;

  /* Same sync the live game does in col1_bridge_apply() after loading a
   * save — without this, founding_fathers_bells_since_last_elect() reads 0. */
  founding_fathers_sync_from_col1_after_load(&save);
  fprintf(
    stderr,
    "bells pool=%u need=%u (human=%d)\n",
    founding_fathers_bells_since_last_elect(human),
    founding_fathers_bells_needed(&save, human),
    human
  );

  /* Report body text uses menu_font (FONTSMAL) in the live game; report
   * TITLES and Congress page 1's body both actually use view.title_font
   * (FONTTINY) once loaded — reports_render() picks that automatically. */
  ColonizeFont font;
  char ff_path[512];
  char ff_err[256];
  bool font_ok = false;
  if (dos_compat_normalize_asset_path(data_dir, "FONTSMAL.FF", ff_path, sizeof(ff_path))) {
    font_ok = ff_load(ff_path, &font, ff_err, sizeof(ff_err));
    if (!font_ok) {
      fprintf(stderr, "ff_load warning: %s\n", ff_err);
    }
  }

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};

  reports_render(
    &view,
    (ColonizeReportId)report_id,
    congress_page2,
    labor_detail_job,
    NULL,
    NULL,
    NULL,
    NULL,
    &save,
    human,
    0,
    0,
    0,
    font_ok ? &font : NULL,
    &fb
  );

  /* Palette is per-background, not global — Congress page 1 uniquely uses
   * its own REPORT3.PIK palette (see reports.h / game_loop.c's palette
   * selection), not backgrounds[report_id]'s (that's page 2 / CCBKGD.PIK). */
  ColonizePalette pal = (ColonizePalette){0};
  if (report_id == COLONIZE_REPORT_CONGRESS && !congress_page2 && view.congress_page1_bg_ok) {
    pal = view.congress_page1_bg.palette;
  } else if (view.background_ok[report_id] && view.backgrounds[report_id].has_palette) {
    pal = view.backgrounds[report_id].palette;
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
  fprintf(stderr, "wrote %s (report_id=%d congress_page2=%d)\n", out_path, report_id, congress_page2);
  return 0;
}
