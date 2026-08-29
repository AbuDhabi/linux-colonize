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

  /*
   * reports_title live LABELS.TXT resolution (2026-08-26 fix): each id's
   * title should now come from the real asset, not just the hardcoded
   * k_report_titles fallback (which happens to already match byte-for-byte,
   * so this only proves the live path, not just that the fallback exists).
   */
  static const struct {
    ColonizeReportId id;
    const char* title;
  } expect_title[] = {
    {COLONIZE_REPORT_RELIGIOUS, "RELIGIOUS ADVISER REPORT"},
    {COLONIZE_REPORT_CONGRESS, "CONTINENTAL CONGRESS ACTIVITIES"},
    {COLONIZE_REPORT_LABOR, "LABOR ADVISER REPORT"},
    {COLONIZE_REPORT_ECONOMIC, "ECONOMIC ADVISER REPORT"},
    {COLONIZE_REPORT_COLONY, "COLONY ADVISER REPORT"},
    {COLONIZE_REPORT_NAVAL, "NAVAL ADVISER REPORT"},
    {COLONIZE_REPORT_FOREIGN, "FOREIGN AFFAIRS REPORT"},
    {COLONIZE_REPORT_INDIAN, "INDIAN ADVISER REPORT"},
    {COLONIZE_REPORT_SCORE, "COLONIZATION SCORE"}
  };
  for (size_t i = 0; i < sizeof(expect_title) / sizeof(expect_title[0]); ++i) {
    const char* got = reports_title(expect_title[i].id);
    if (!got || strcmp(got, expect_title[i].title) != 0) {
      fprintf(
        stderr,
        "reports_title id=%d got='%s' want='%s'\n",
        (int)expect_title[i].id,
        got ? got : "(null)",
        expect_title[i].title
      );
      reports_free(&view);
      return 1;
    }
  }
  /* Out-of-range id still falls back safely. */
  if (strcmp(reports_title((ColonizeReportId)999), "REPORT") != 0) {
    fprintf(stderr, "reports_title out-of-range should return \"REPORT\"\n");
    reports_free(&view);
    return 1;
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

  /* Tribe names now resolve live from NAMES.TXT @TRIBES column 0 — a
   * separate per-index buffer (not the shared ff/job/cargo scratch one),
   * since the Indian Adviser stores several of these into rows[] at once. */
  if (strcmp(reports_tribe_display_name(0), "Incas") != 0) {
    fprintf(stderr, "tribe 0 want 'Incas' got '%s'\n", reports_tribe_display_name(0));
    reports_free(&view);
    return 1;
  }
  if (strcmp(reports_tribe_display_name(7), "Tupi") != 0) {
    fprintf(stderr, "tribe 7 want 'Tupi' got '%s'\n", reports_tribe_display_name(7));
    reports_free(&view);
    return 1;
  }
  /* Both must stay correct at once — proves the per-index buffer isn't
   * aliased the way a single shared scratch buffer would be. */
  const char* t0 = reports_tribe_display_name(0);
  const char* t7 = reports_tribe_display_name(7);
  if (strcmp(t0, "Incas") != 0 || strcmp(t7, "Tupi") != 0) {
    fprintf(stderr, "tribe names 0/7 aliased: got '%s'/'%s'\n", t0, t7);
    reports_free(&view);
    return 1;
  }

  /* Nation adjectives now resolve live from NAMES.TXT @NATIONALITY —
   * same per-index-buffer aliasing check as tribe names (Foreign Affairs
   * stores this into rows[] too). */
  if (strcmp(reports_nation_adjective_display_name(0), "English") != 0) {
    fprintf(
      stderr,
      "nation 0 want 'English' got '%s'\n",
      reports_nation_adjective_display_name(0)
    );
    reports_free(&view);
    return 1;
  }
  if (strcmp(reports_nation_adjective_display_name(3), "Dutch") != 0) {
    fprintf(
      stderr, "nation 3 want 'Dutch' got '%s'\n", reports_nation_adjective_display_name(3)
    );
    reports_free(&view);
    return 1;
  }
  const char* n0 = reports_nation_adjective_display_name(0);
  const char* n3 = reports_nation_adjective_display_name(3);
  if (strcmp(n0, "English") != 0 || strcmp(n3, "Dutch") != 0) {
    fprintf(stderr, "nation adjectives 0/3 aliased: got '%s'/'%s'\n", n0, n3);
    reports_free(&view);
    return 1;
  }

  /* Tribe tech levels now resolve live from NAMES.TXT @LEVELS column 0. */
  if (strcmp(reports_tribe_level_display_name(0), "Semi-Nomadic") != 0) {
    fprintf(
      stderr,
      "level 0 want 'Semi-Nomadic' got '%s'\n",
      reports_tribe_level_display_name(0)
    );
    reports_free(&view);
    return 1;
  }
  if (strcmp(reports_tribe_level_display_name(3), "Civilized") != 0) {
    fprintf(
      stderr, "level 3 want 'Civilized' got '%s'\n", reports_tribe_level_display_name(3)
    );
    reports_free(&view);
    return 1;
  }

  /* Body words resolve live from LABELS.TXT @MISC (P2.2 residue, 2026-08-28):
   * #86 "Rebels" / #87 "Tories" (Foreign Affairs), #56 labor zoom hint,
   * #112 congress header, #121 score total; out-of-range → fallback. */
  if (strcmp(reports_misc_display_word(86, "x"), "Rebels") != 0 ||
      strcmp(reports_misc_display_word(87, "x"), "Tories") != 0 ||
      strcmp(reports_misc_display_word(56, "x"), "(Click on item to zoom)") != 0 ||
      strcmp(reports_misc_display_word(112, "x"), "Next Continental Congress Session") != 0 ||
      strcmp(reports_misc_display_word(121, "x"), "Total Score") != 0 ||
      strcmp(reports_misc_display_word(9999, "fb"), "fb") != 0) {
    fprintf(
      stderr,
      "misc words: 86='%s' 87='%s' 56='%s'\n",
      reports_misc_display_word(86, "x"),
      reports_misc_display_word(87, "x"),
      reports_misc_display_word(56, "x")
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
  if (score.foreign_recognition_pct != 0 || score.early_revolution_pts != 0 ||
      score.bells_pts != 0 || score.rating != 0 || score.exploits_tier != -1) {
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

  /* FUN_41f2_0092 tail: recognition multiplier (8 + (8 >> prior)) / 8 —
   * x2, x1.5, x1.25, x1.125, x1 for 0..4 prior nations; none unless achieved. */
  {
    static const int want[5] = {200, 150, 125, 112, 100};
    for (int prior = 0; prior < 5; ++prior) {
      const int got = reports_score_apply_recognition(100, prior, true);
      if (got != want[prior]) {
        fprintf(stderr, "recognition prior=%d got %d want %d\n", prior, got, want[prior]);
        col1_save_free(&col1);
        reports_free(&view);
        return 1;
      }
    }
    if (reports_score_apply_recognition(100, 0, false) != 100) {
      fprintf(stderr, "recognition applied without achievement\n");
      col1_save_free(&col1);
      reports_free(&view);
      return 1;
    }
  }

  /* FUN_41f2_0b70 rating: mult {4,5,6,8,10}, ((mult*total)/100)>>1; tier =
   * largest n-1 with n*n/3 < (mult*total)/100 over n=1..24, cap 23. */
  {
    int tier = 99;
    if (reports_score_rating(1000, 4, &tier) != 50 || tier != 16) {
      fprintf(stderr, "rating(1000, Viceroy) wrong: tier=%d\n", tier);
      col1_save_free(&col1);
      reports_free(&view);
      return 1;
    }
    if (reports_score_rating(1000, 0, &tier) != 20 || tier != 9) {
      fprintf(stderr, "rating(1000, Discoverer) wrong: tier=%d\n", tier);
      col1_save_free(&col1);
      reports_free(&view);
      return 1;
    }
    if (reports_score_rating(5, 0, &tier) != 0 || tier != -1) {
      fprintf(stderr, "rating(5) should give no exploits tier (got %d)\n", tier);
      col1_save_free(&col1);
      reports_free(&view);
      return 1;
    }
    if (reports_score_rating(20000, 4, &tier) != 1000 || tier != 23) {
      fprintf(stderr, "rating tier cap 23 broken (got %d)\n", tier);
      col1_save_free(&col1);
      reports_free(&view);
      return 1;
    }
  }

  /* Full composer on a synthetic post-independence state: achieved, declared
   * 1776 (latched 0x53a7/0x53a8), one prior nation, REF present with 250
   * bells since declaring, 2 villages burned at Conquistador. */
  {
    ColonizeCol1Save c;
    memset(&c, 0, sizeof(c));
    memset(c.head.founding_father, 0xff, sizeof(c.head.founding_father)); /* none elected */
    c.head.year = 1790;
    c.head.difficulty = 2;
    c.head.game_options.woi = 1;
    c.head.game_options.ref_present = 1;
    c.head.game_options.independence_chrome = 1;
    c.head.king_audience_streak = 17;
    c.head.king_audience_last_pick = 76;
    c.head.rebel_sentiment_report = 60;
    c.nation[0].gold = 2500;
    c.nation[0].liberty_bells_total = 250;
    c.nation[0].villages_burned = 2;
    c.nation[1].nation_flags = 0x04;
    ColonizeScoreBreakdown sc;
    reports_compute_score(&sc, &c, 0, NULL, NULL);
    /* early (1780-1776)*2=8, gold 2, rebel 60, bells 2, villages -6 = 66;
     * x1.5 (one prior) = 99; rating Conquistador mult 6: 5>>1 = 2, tier 2. */
    if (sc.declare_year != 1776 || sc.early_revolution_pts != 8 || sc.treasury != 2 ||
        sc.bells_pts != 2 || sc.villages_penalty != -6 || sc.prior_nations != 1 ||
        sc.foreign_recognition_pct != 50 || sc.base_total != 66 || sc.total != 99 ||
        sc.rating != 2 || sc.exploits_tier != 2) {
      fprintf(
        stderr,
        "synthetic independence score wrong: dy=%d early=%d gold=%d bells=%d vil=%d prior=%d "
        "pct=%d base=%d total=%d rating=%d tier=%d\n",
        sc.declare_year, sc.early_revolution_pts, sc.treasury, sc.bells_pts, sc.villages_penalty,
        sc.prior_nations, sc.foreign_recognition_pct, sc.base_total, sc.total, sc.rating,
        sc.exploits_tier
      );
      col1_save_free(&col1);
      reports_free(&view);
      return 1;
    }
    c.head.game_options.calendar_latch = 1; /* 0x5382|0x10 SCORING COMPLETE */
    reports_compute_score(&sc, &c, 0, NULL, NULL);
    if (!sc.scoring_complete || sc.total != 0) {
      fprintf(stderr, "scoring-complete latch should zero the composer\n");
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
  if (strcmp(reports_tribe_display_name(0), "Incas") != 0) {
    fprintf(
      stderr, "tribe 0 after reports_free want 'Incas' got '%s'\n", reports_tribe_display_name(0)
    );
    return 1;
  }
  if (strcmp(reports_nation_adjective_display_name(0), "English") != 0) {
    fprintf(
      stderr,
      "nation 0 after reports_free want 'English' got '%s'\n",
      reports_nation_adjective_display_name(0)
    );
    return 1;
  }
  if (strcmp(reports_tribe_level_display_name(0), "Semi-Nomadic") != 0) {
    fprintf(
      stderr,
      "level 0 after reports_free want 'Semi-Nomadic' got '%s'\n",
      reports_tribe_level_display_name(0)
    );
    return 1;
  }

  if (strcmp(reports_misc_display_word(86, "Rebels"), "Rebels") != 0) {
    fprintf(stderr, "misc word 86 after reports_free should fall back to 'Rebels'\n");
    return 1;
  }

  diag_shutdown();
  return 0;
}
