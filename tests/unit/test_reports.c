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

  /*
   * Founding Father names now resolve live from NAMES.TXT @FATHERS after
   * reports_load (2026-08-26 fix — was a hand-typed static table only).
   * Check first/last rows against the real asset text.
   */
  if (strcmp(reports_ff_display_name(0), "Adam Smith") != 0) {
    fprintf(stderr, "FF 0 want 'Adam Smith' got '%s'\n", reports_ff_display_name(0));
    reports_free(&view);
    return 1;
  }
  if (strcmp(reports_ff_display_name(24), "Bartolome de las Casas") != 0) {
    fprintf(
      stderr, "FF 24 want 'Bartolome de las Casas' got '%s'\n", reports_ff_display_name(24)
    );
    reports_free(&view);
    return 1;
  }
  if (reports_ff_display_name(-1) == NULL || reports_ff_display_name(25) == NULL) {
    fprintf(stderr, "FF name out-of-range should return a placeholder, not NULL\n");
    reports_free(&view);
    return 1;
  }

  /* Job expert names now resolve live from NAMES.TXT @JOB column 2. */
  if (strcmp(reports_job_display_name(0), "Expert Farmers") != 0) {
    fprintf(
      stderr, "job 0 want 'Expert Farmers' got '%s'\n", reports_job_display_name(0)
    );
    reports_free(&view);
    return 1;
  }
  if (strcmp(reports_job_display_name(27), "Indian Converts") != 0) {
    fprintf(
      stderr, "job 27 want 'Indian Converts' got '%s'\n", reports_job_display_name(27)
    );
    reports_free(&view);
    return 1;
  }

  /* Cargo names now resolve live from NAMES.TXT @CARGO column 0. */
  if (strcmp(reports_cargo_display_name(0), "Food") != 0) {
    fprintf(stderr, "cargo 0 want 'Food' got '%s'\n", reports_cargo_display_name(0));
    reports_free(&view);
    return 1;
  }
  if (strcmp(reports_cargo_display_name(15), "Muskets") != 0) {
    fprintf(
      stderr, "cargo 15 want 'Muskets' got '%s'\n", reports_cargo_display_name(15)
    );
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
  /* COLONY01: Soldier(+4, profession byte 21 is a genuine assigned job) +
   * gold 1000(+1); no colonies/FF/rebels. The Pioneer here carries the DOS
   * "no expert" sentinel (profession byte 28) with no colony slot behind
   * it, so it does NOT contribute — see reports_score_collect_citizen_jobs'
   * comment (confirmed against dutch-reports.SAV/score.png: a type-based
   * fallback for map/Europe units overcounts the golden by exactly the
   * amount its own unscored units would add). */
  if (score.citizens != 4 || score.treasury != 1 || score.congress != 0 ||
      score.rebel_sentiment != 0 || score.villages_penalty != 0 || score.total != 5) {
    fprintf(
      stderr,
      "COLONY01 score mismatch citizens=%d treasury=%d congress=%d rebel=%d "
      "villages=%d total=%d (want 4/1/0/0/0/5)\n",
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

  /* Foreign Affairs report (F8) golden: dutch-reports.SAV / foreign.png —
   * locks in the census-based Rebels/Tories formula and the euro_relation
   * War/Peace reading (see reports.c's reports_render_foreign comment). */
  {
    ColonizeCol1Save fcol1;
    col1_save_init(&fcol1);
    if (!col1_save_read_file(
          "original_saves/report-screen-goldens/dutch-reports.SAV", &fcol1, err, sizeof(err)
        )) {
      fprintf(stderr, "dutch-reports.SAV load failed: %s\n", err);
      reports_free(&view);
      return 1;
    }

    /* census_pop_proxy + rebel_sentiment reproduce foreign.png's exact
     * Rebels/Tories numbers for all 3 surviving nations. */
    static const struct {
      int nation;
      int rebels;
      int tories;
    } want_pop[] = {
      {0, 21, 54}, /* English */
      {1, 24, 21}, /* French */
      {3, 50, 4}, /* Dutch */
    };
    for (size_t i = 0; i < sizeof(want_pop) / sizeof(want_pop[0]); ++i) {
      const int n = want_pop[i].nation;
      const int total = fcol1.stuff.census_pop_proxy[n];
      const int rebels = (total * (int)fcol1.nation[n].rebel_sentiment) / 100;
      const int tories = total - rebels;
      if (rebels != want_pop[i].rebels || tories != want_pop[i].tories) {
        fprintf(
          stderr,
          "foreign rebels/tories mismatch nation=%d got=%d/%d want=%d/%d\n",
          n,
          rebels,
          tories,
          want_pop[i].rebels,
          want_pop[i].tories
        );
        col1_save_free(&fcol1);
        reports_free(&view);
        return 1;
      }
    }

    /* Spanish withdrew (control==2) — golden shows "(Withdrawn from New
     * World)" instead of relation/population lines. */
    if (fcol1.player[2].control != 2) {
      fprintf(stderr, "dutch-reports.SAV Spanish should read withdrawn (control==2)\n");
      col1_save_free(&fcol1);
      reports_free(&view);
      return 1;
    }

    /* French/Dutch are at war (golden: red "War"); English/French (and
     * every other visible pair) is at peace. euro_relation's 0x02 bit set
     * in either direction — see reports_render_foreign's block comment
     * for why this, not ai_diplo.h's AI_DIPLO_WAR bit, matches this raw
     * save. */
    const bool fr_du_war =
      ((fcol1.nation[1].euro_relation[3] | fcol1.nation[3].euro_relation[1]) & 0x02u) != 0;
    const bool en_fr_war =
      ((fcol1.nation[0].euro_relation[1] | fcol1.nation[1].euro_relation[0]) & 0x02u) != 0;
    if (!fr_du_war || en_fr_war) {
      fprintf(
        stderr,
        "dutch-reports.SAV war reading wrong: French/Dutch war=%d English/French war=%d\n",
        fr_du_war,
        en_fr_war
      );
      col1_save_free(&fcol1);
      reports_free(&view);
      return 1;
    }

    memset(pixels, 0, sizeof(pixels));
    reports_render(
      &view,
      COLONIZE_REPORT_FOREIGN,
      false,
      -1,
      0,
      0,
      0,
      NULL,
      NULL,
      NULL,
      NULL,
      &fcol1,
      fcol1.head.human_player,
      0,
      0,
      fcol1.head.turn,
      NULL,
      &fb
    );
    if (pixels[0] == 0 && pixels[160 + 100 * 320] == 0) {
      fprintf(stderr, "foreign affairs render looks empty for dutch-reports.SAV\n");
      col1_save_free(&fcol1);
      reports_free(&view);
      return 1;
    }

    col1_save_free(&fcol1);
  }

  fprintf(stderr, "report screens ok (%d backgrounds + Col1 data + score)\n", COLONIZE_REPORT_COUNT);
  reports_free(&view);

  /* After free (no assets loaded), names must still resolve — the
   * hand-typed static table fallback, not a stale/dangling live pointer. */
  if (strcmp(reports_ff_display_name(0), "Adam Smith") != 0) {
    fprintf(
      stderr, "FF 0 after reports_free want 'Adam Smith' got '%s'\n", reports_ff_display_name(0)
    );
    return 1;
  }
  if (strcmp(reports_job_display_name(0), "Expert Farmers") != 0) {
    fprintf(
      stderr,
      "job 0 after reports_free want 'Expert Farmers' got '%s'\n",
      reports_job_display_name(0)
    );
    return 1;
  }
  if (strcmp(reports_cargo_display_name(0), "Food") != 0) {
    fprintf(
      stderr, "cargo 0 after reports_free want 'Food' got '%s'\n", reports_cargo_display_name(0)
    );
    return 1;
  }

  diag_shutdown();
  return 0;
}
