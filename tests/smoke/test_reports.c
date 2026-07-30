#include <stdio.h>
#include <string.h>

#include "core/reports.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  ColonizeReportsView view;
  char err[256];
  if (!reports_load(&view, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "reports_load failed: %s\n", err);
    return 1;
  }

  static const struct {
    ColonizeReportId id;
    const char* file;
  } expect[] = {
    {COLONIZE_REPORT_RELIGIOUS, "REPORT2.PIK"},
    {COLONIZE_REPORT_CONGRESS, "CCBKGD.PIK"},
    {COLONIZE_REPORT_LABOR, "REPORT4.PIK"},
    {COLONIZE_REPORT_ECONOMIC, "REPORT5.PIK"},
    {COLONIZE_REPORT_COLONY, "REPORT6.PIK"},
    {COLONIZE_REPORT_NAVAL, "REPORT7.PIK"},
    {COLONIZE_REPORT_FOREIGN, "REPORT8.PIK"},
    {COLONIZE_REPORT_INDIAN, "REPORT9.PIK"},
    {COLONIZE_REPORT_SCORE, "WOODPANL.PIK"}
  };

  for (size_t i = 0; i < sizeof(expect) / sizeof(expect[0]); ++i) {
    if (!view.background_ok[expect[i].id]) {
      fprintf(stderr, "missing background for %s\n", expect[i].file);
      reports_free(&view);
      return 1;
    }
    if (strcmp(reports_background_name(expect[i].id), expect[i].file) != 0) {
      fprintf(
        stderr,
        "bg name mismatch id=%d got=%s want=%s\n",
        (int)expect[i].id,
        reports_background_name(expect[i].id),
        expect[i].file
      );
      reports_free(&view);
      return 1;
    }
    if (view.backgrounds[expect[i].id].width != 320 ||
        view.backgrounds[expect[i].id].height != 200) {
      fprintf(stderr, "%s bad size\n", expect[i].file);
      reports_free(&view);
      return 1;
    }
  }

  ColonizeReportId mapped = COLONIZE_REPORT_COUNT;
  if (reports_id_from_fkey(1, &mapped)) {
    fprintf(stderr, "F1 should not map to a report plate\n");
    reports_free(&view);
    return 1;
  }
  if (!reports_id_from_fkey(8, &mapped) || mapped != COLONIZE_REPORT_FOREIGN) {
    fprintf(stderr, "F8 should map to foreign affairs\n");
    reports_free(&view);
    return 1;
  }
  if (!reports_id_from_fkey(10, &mapped) || mapped != COLONIZE_REPORT_SCORE) {
    fprintf(stderr, "F10 should map to score\n");
    reports_free(&view);
    return 1;
  }

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  reports_render(
    &view,
    COLONIZE_REPORT_CONGRESS,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    0,
    1,
    NULL,
    &fb
  );
  if (pixels[0] == 0 && pixels[160 + 100 * 320] == 0) {
    fprintf(stderr, "congress render looks empty\n");
    reports_free(&view);
    return 1;
  }

  memset(pixels, 0, sizeof(pixels));
  reports_render(
    &view,
    COLONIZE_REPORT_SCORE,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    0,
    1,
    NULL,
    &fb
  );
  if (pixels[0] == 0 && pixels[160 + 100 * 320] == 0) {
    fprintf(stderr, "score/wood render looks empty\n");
    reports_free(&view);
    return 1;
  }

  fprintf(stderr, "report screens ok (%d backgrounds)\n", COLONIZE_REPORT_COUNT);
  reports_free(&view);
  diag_shutdown();
  return 0;
}
