/*
 * render_report: standalone report-screen renderer for golden comparison.
 * Usage: render_report <data_dir> <save.SAV> <out.ppm> [report_id] [params...]
 * External requirements: Built by CMake; outputs 320×200 PPM (convert to PNG with ImageMagick)
 *
 * Calls reports_load()/reports_render() directly (bypasses SDL/xvfx entirely)
 * and dumps the resulting 320x200 indexed framebuffer, expanded through the
 * right palette, as a binary PPM. Convert/view with ImageMagick:
 *
 *   convert out.ppm out.png
 *
 * See docs/report_screens.md for the full report-porting workflow this tool
 * is part of (grid_overlay.sh / render_diff.sh live in scripts/).
 *
 *   render_report <data_dir> <save.SAV> <out.ppm> [report_id] [congress_page2] [labor_detail_job] [economic_page] [colony_page] [naval_page]
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
 *   economic_page   0 = European Trade, N>=1 = Cargo in Port page N (ignored
 *                   otherwise); default 0
 *   colony_page     Colony report page index — see reports_colony_page_count
 *                   (ignored otherwise); default 0
 *   naval_page      Naval report page index — see reports_naval_page_count
 *                   (ignored otherwise); default 0
 *
 * Also prints the founding-fathers bells pool/need to stderr (useful when
 * working on the Congress bells bar) after seeding the pool the same way
 * the live game does on save load (founding_fathers_sync_from_col1_after_load).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/founding_fathers.h"
#include "core/reports.h"

#include "tools/render_common.h"

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
  const int economic_page = argc > 7 ? atoi(argv[7]) : 0;
  const int colony_page = argc > 8 ? atoi(argv[8]) : 0;
  const int naval_page = argc > 9 ? atoi(argv[9]) : 0;

  char err[256];
  ColonizeReportsView view;
  reports_init(&view);
  if (!reports_load(&view, data_dir, err, sizeof(err))) {
    fprintf(stderr, "reports_load failed: %s\n", err);
    return 1;
  }

  RenderSaveBundle rs;
  if (!render_load_save(data_dir, save_path, &rs)) {
    return 1;
  }
  const int human = rs.human;

  /* Same sync col1_bridge_apply() already did on load — repeated here only so
   * the bells line below reads the pool the live game would have. */
  founding_fathers_sync_from_col1_after_load(&rs.save);
  fprintf(
    stderr,
    "bells pool=%u need=%u (human=%d)\n",
    founding_fathers_bells_since_last_elect(human),
    founding_fathers_bells_needed(&rs.save, human),
    human
  );

  /*
   * Economic report (F5) Bid/Ask comes straight from col1_bridge_apply()'s
   * own sync (bid = this nation's euro_price, ask = bid + @CARGO burden,
   * FUN_38fd_0016). This tool used to re-derive it by hand with "+ 1" on the
   * ask before the bridge ran — dead while the bridge overwrote it, and one
   * gold too high the moment it did not. Removed with the shared prologue
   * (duplication audit TT-34).
   */

  /* Report body text uses menu_font (FONTSMAL) in the live game; report
   * TITLES and Congress page 1's body both actually use view.title_font
   * (FONTTINY) once loaded — reports_render() picks that automatically. */
  ColonizeFont font;
  const bool font_ok = render_load_font(data_dir, "FONTSMAL.FF", &font);

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb;
  render_fb_init(&fb, pixels);

  reports_render(
    &view,
    (ColonizeReportId)report_id,
    congress_page2,
    labor_detail_job,
    economic_page,
    colony_page,
    naval_page,
    &rs.colonies,
    &rs.units,
    &rs.map,
    &rs.europe,
    &rs.save,
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

  if (!render_write_ppm(out_path, pixels, &pal)) {
    return 1;
  }
  fprintf(stderr, "wrote %s (report_id=%d congress_page2=%d)\n", out_path, report_id, congress_page2);
  return 0;
}
