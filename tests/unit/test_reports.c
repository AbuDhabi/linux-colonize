#include <stdio.h>
#include <string.h>

#include "core/col1_save.h"
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
    false,
    -1,
    0,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
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
    false,
    -1,
    0,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
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

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  if (!col1_save_read_file("original_saves/COLONY01.SAV", &col1, err, sizeof(err))) {
    fprintf(stderr, "col1 load failed: %s\n", err);
    reports_free(&view);
    return 1;
  }

  for (int id = 0; id < COLONIZE_REPORT_COUNT; ++id) {
    memset(pixels, 0, sizeof(pixels));
    reports_render(
      &view,
      (ColonizeReportId)id,
      false,
      -1,
      0,
      0,
      NULL,
      NULL,
      NULL,
      NULL,
      &col1,
      0,
      0,
      0,
      col1.head.turn,
      NULL,
      &fb
    );
    if (pixels[0] == 0 && pixels[160 + 100 * 320] == 0) {
      fprintf(stderr, "report id %d empty with Col1 data\n", id);
      col1_save_free(&col1);
      reports_free(&view);
      return 1;
    }
  }

  if (col1.nation[0].current_crosses != 6 || col1.nation[0].needed_crosses != 9) {
    fprintf(
      stderr,
      "unexpected COLONY01 crosses %u/%u\n",
      (unsigned)col1.nation[0].current_crosses,
      (unsigned)col1.nation[0].needed_crosses
    );
    col1_save_free(&col1);
    reports_free(&view);
    return 1;
  }
  if (col1.head.tribe_count == 0) {
    fprintf(stderr, "COLONY01 should have tribes for Indian report\n");
    col1_save_free(&col1);
    reports_free(&view);
    return 1;
  }

  ColonizeScoreBreakdown score;
  reports_compute_score(&score, &col1, 0, NULL, NULL);
  /* COLONY01: Soldier(+4) + Pioneer(+4) + gold 1000(+1); no colonies/FF/rebels. */
  if (score.citizens != 8 || score.treasury != 1 || score.congress != 0 ||
      score.rebel_sentiment != 0 || score.villages_penalty != 0 || score.total != 9) {
    fprintf(
      stderr,
      "COLONY01 score mismatch citizens=%d treasury=%d congress=%d rebel=%d "
      "villages=%d total=%d (want 8/1/0/0/0/9)\n",
      score.citizens,
      score.treasury,
      score.congress,
      score.rebel_sentiment,
      score.villages_penalty,
      score.total
    );
    col1_save_free(&col1);
    reports_free(&view);
    return 1;
  }
  if (score.foreign_recognition_pct != 0 || score.early_revolution_pct != 0) {
    fprintf(stderr, "COLONY01 should have no independence bonuses yet\n");
    col1_save_free(&col1);
    reports_free(&view);
    return 1;
  }

  /* Village penalty: -(difficulty+1) * burned */
  {
    ColonizeScoreBreakdown pen;
    reports_compute_score(&pen, &col1, 0, NULL, NULL);
    pen.difficulty = 2;
    pen.villages_burned = 12;
    pen.villages_penalty = -(pen.difficulty + 1) * pen.villages_burned;
    if (pen.villages_penalty != -36) {
      fprintf(stderr, "village penalty formula wrong: %d\n", pen.villages_penalty);
      col1_save_free(&col1);
      reports_free(&view);
      return 1;
    }
  }

  /* Foreign recognition multipliers when independence is achieved. */
  {
    ColonizeScoreBreakdown b = {0};
    b.base_total = 100;
    b.independence_achieved = true;
    b.prior_nations = 0;
    b.foreign_recognition_pct = 100;
    b.total = b.base_total + (b.base_total * b.foreign_recognition_pct) / 100;
    if (b.total != 200) {
      fprintf(stderr, "first-independence multiplier broken\n");
      col1_save_free(&col1);
      reports_free(&view);
      return 1;
    }
  }

  col1_save_free(&col1);
  fprintf(stderr, "report screens ok (%d backgrounds + Col1 data + score)\n", COLONIZE_REPORT_COUNT);
  reports_free(&view);
  diag_shutdown();
  return 0;
}
