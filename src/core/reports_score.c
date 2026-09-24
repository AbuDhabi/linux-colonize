/*
 * Sections:
 *  - Score computation (~line 33)
 *  - Hall of Fame & Exploits report (~line 754)
 */

#include "core/ai_diplo.h"
#include "core/ai_king.h"
#include "core/assets.h"
#include "core/colony_production.h"
#include "core/combat_strength.h"
#include "core/founding_fathers.h"
#include "core/reports.h"
#include "core/reports_names.h"

#include "core/fb.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

#include "core/reports_internal.h"

/* Unit type → default @JOB index when profession is out of range. DOS
 * DS:0x30e[type] (FUN_15eb_0902), the one table the whole game uses; the
 * port's hand-written copy of it here had @UNIT 6 (Regulars) → 21 and 8
 * (Cavalry) → 23 where DOS has -1, which promoted the King's army to
 * scoring citizens (bugs.md #574). */
/* ===================== Score computation (reports_profession_from_unit_type .. reports_render_score) ===================== */
static int reports_profession_from_unit_type(int type) {
  return units_type_default_job(type);
}

/* FUN_41f2_0092 raw 71172-71178: the Score citizen loop admits a unit on
 * `FUN_281f_0b78(unit) >= 0` alone — the profession-slot predicate — and
 * nothing else. */
static bool reports_unit_type_is_scored_colonist(int type) {
  return units_type_has_profession_slot(type);
}

/* FUN_41f2_0092 raw 71188-71198: profession 0x1c +2; 0x19/0x1a/0x1b
 * (servant/criminal/convert) +1; anything else +4. */
static int reports_citizen_points_for_job(int job) {
  if (job == UNITS_JOB_SERVANT || job == UNITS_JOB_CRIMINAL || job == UNITS_JOB_CONVERT) {
    return 1; /* Indentured Servants, Petty Criminals, Indian Converts */
  }
  if (job == UNITS_JOB_COLONIST) {
    return 2; /* Free Colonists (port alias of 0x1c) */
  }
  if (job >= 0 && job < k_job_count) {
    return 4; /* specialists / veterans / teachers / etc. */
  }
  return 2;
}

static int reports_resolve_job(int profession, int unit_type) {
  if (profession >= 0 && profession < k_job_count) {
    return profession;
  }
  if (unit_type >= 0) {
    const int fallback = reports_profession_from_unit_type(unit_type);
    if (fallback >= 0) {
      return fallback;
    }
  }
  return 19; /* Free Colonists */
}

static int reports_count_ff_for_nation(const ColonizeCol1Save* col1, int human) {
  if (!col1) {
    return 0;
  }
  /* DOS FUN_41f2_0092 (viceroy_unpacked.c 71217-71233) runs one 25-iteration
   * loop: `FUN_281f_07b4(nation, i)` decides both `local_5a += 5` and whether
   * the name is drawn, so the plate's number and its grid can never disagree.
   * FUN_281f_07b4 -> FUN_15eb_3960 (13832-13844) is exactly the per-nation
   * bitmask read `nation[n].founding_fathers[i >> 3] & (1 << (i & 7))` that
   * founding_fathers_nation_has ports. There is no count field and no
   * head.founding_father[] reading anywhere in the chain: the old
   * founding_father_count / head-equality fallbacks (the reading audit #83
   * removed elsewhere — head.founding_father[] holds the FIRST CLAIMER, not
   * per-nation membership) are gone. Same predicate as the page-1 name grid
   * (:1462), page-2 portraits (:1524) and the Score FF strip (:4229). */
  int ff = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if (founding_fathers_nation_has(col1, human, i)) {
      ff++;
    }
  }
  return ff;
}

/* Rebel Sentiment on this screen is DS:0x53d0 (rebel_sentiment_report),
 * confirmed byte-for-byte against dutch-reports.SAV (94 = score.png's
 * "Rebel Sentiment: +94"; nation+0x19 happens to hold the same value there,
 * a pop-weighted recompute from colony rebel_dividend/rebel_divisor gives 91
 * — trust the stored field, don't re-derive it). */

#define REPORTS_SCORE_CITIZENS_MAX 1024

/*
 * Every counted "citizen" for the Score screen's Citizens line/icon strip,
 * as a @JOB id (0..27) suitable for reports_citizen_points_for_job /
 * units_job_icon_sprite. Two different rules for the two sources, both
 * confirmed against dutch-reports.SAV (colony population sums to 142,
 * +16 more from 4 qualifying map/Europe units = golden's exact +158):
 *
 * - Colony population: every occupied slot is unconditionally a person, so
 *   an invalid/sentinel profession byte (28, UNITS_JOB_NONE) still falls
 *   back to Free Colonist (reports_resolve_job's default).
 * - Map/Europe units: only counted when the unit's raw `profession` byte is
 *   itself a genuine assigned job (0..27) — a unit created without one
 *   (profession==28, the common case for a plain freshly-recruited/promoted
 *   unit) contributes nothing here, *no* type-based fallback. Synthesizing
 *   a job from unit type (as reports_profession_from_unit_type does for
 *   other purposes) overcounts: applying it to every scored-colonist unit
 *   in that save gives 180, not the golden's 158.
 */
/*
 * bugs.md (score_screen.SAV): Score-strip icon for a counted UNIT. DOS
 * FUN_41f2_0092 draws each unit with its map icon (FUN_281f_02da — type icon
 * incl. equipment, downgraded to the generic 0x4a..0x4e pose when the
 * profession isn't the kit's own expert) and only swaps a GENERIC pose for
 * the profession portrait (02c6). So Veteran Soldiers/Dragoons/Scouts/
 * Pioneers keep their full kit, Regulars/Cavalry/Continentals keep their own
 * @UNIT art, and a plain-equipped expert shows his skill portrait instead.
 * Icon constants: NAMES @UNIT icon column − 1 (1-based sheet).
 */
static int reports_score_unit_icon(int type, int prof) {
  /* bugs.md #656: 0x17 (UNITS_JOB_DRAGOON) is never written by DOS
   * (#503/#639); units.c's veteran gates (units_profession_line,
   * units_map_sprite/FUN_112b_0060, the 172c promote test) are 0x15-only,
   * so this sibling display matches them instead of tolerating 0x17. */
  const bool vet = (prof == UNITS_JOB_SOLDIER);
  int icon = -1;
  switch (type) {
  case 1: /* Soldiers */
    icon = vet ? UNITS_ICON_VETERAN_SOLDIER : UNITS_ICON_SOLDIER;
    break;
  case 2: /* Pioneers */
    icon = (prof == UNITS_JOB_PIONEER) ? UNITS_ICON_HARDY_PIONEER : UNITS_ICON_PIONEER;
    break;
  case 3: /* Missionaries: NAMES icon 106, generic pose 77 */
    icon = (prof == UNITS_JOB_MISSIONARY) ? 105 : 77;
    break;
  case 4: /* Dragoons */
    icon = vet ? UNITS_ICON_VETERAN_DRAGOON : UNITS_ICON_DRAGOON;
    break;
  case 5: /* Scouts */
    icon = (prof == UNITS_JOB_SCOUT) ? UNITS_ICON_SEASONED_SCOUT : UNITS_ICON_SCOUT;
    break;
  case 6: /* Regulars */
    return 125;
  case 7: /* Cont. Cav. */
    return 129;
  case 8: /* Cavalry */
    return 126;
  case 9: /* Cont. Army */
    return 128;
  default:
    break;
  }
  /* Colonists-type, or a generic equipped pose → profession portrait. */
  if (icon < 0 || (icon >= UNITS_ICON_PIONEER && icon <= 77)) {
    const int by_job = units_job_icon_sprite(reports_resolve_job(prof, type));
    if (by_job >= 0) {
      return by_job;
    }
  }
  return icon;
}

static int reports_score_collect_citizen_jobs(
  const ColonizeCol1Save* col1,
  int human,
  int* jobs_out,
  int* icons_out, /* optional: per-citizen strip sprite (equipped for units) */
  int max_out
) {
  int count = 0;
  if (!col1 || !jobs_out || max_out <= 0) {
    return 0;
  }
  for (uint16_t i = 0; i < col1->head.colony_count && count < max_out; ++i) {
    const ColonizeCol1Colony* c = &col1->colony[i];
    if (c->nation_id != (uint8_t)human) {
      continue;
    }
    const int pop =
      c->population > COLONIZE_COL1_COLONY_POP_MAX ? COLONIZE_COL1_COLONY_POP_MAX
                                                   : (int)c->population;
    for (int p = 0; p < pop && count < max_out; ++p) {
      const int job = reports_resolve_job((int)c->profession[p], -1);
      if (icons_out) {
        icons_out[count] = units_job_icon_sprite(job);
      }
      jobs_out[count++] = job;
    }
  }
  for (uint16_t i = 0; i < col1->head.unit_count && count < max_out; ++i) {
    const ColonizeCol1Unit* u = &col1->unit[i];
    if ((int)u->nation_id != human) {
      continue;
    }
    if (!reports_unit_type_is_scored_colonist((int)u->type)) {
      continue;
    }
    /*
     * bugs.md #574 asked for 0x1c units to score +2 here, reading raw
     * 71188-71198 (`if (prof == 0x1c) +2; else if 0x19/0x1a/0x1b +1; else
     * +4`) as applying to every slot-bearing unit. REFUTED by the DOS
     * capture: original_saves/report-screen-goldens/score.png reads
     * "Dutch Citizens: +158" for dutch-reports.SAV, which is colony
     * population (142) + the 4 units whose profession byte is a genuine
     * assigned job (4 x 4). Counting that save's 0x1c-profession units at
     * +2 each gives 166. So a unit with the "no specialty" sentinel does
     * not reach the ladder at all — the scored value in the decomp
     * (`in_stack_0000ff8e`, a stack slot Ghidra only shows assigned inside
     * the generic-icon branch) is not simply +0x315b.
     */
    const int prof = (int)u->profession;
    if (prof < 0 || prof >= k_job_count) {
      continue;
    }
    if (icons_out) {
      icons_out[count] = reports_score_unit_icon((int)u->type, prof);
    }
    jobs_out[count++] = prof;
  }
  return count;
}

int reports_score_apply_recognition(int base_total, int prior_nations, bool achieved) {
  if (!achieved || prior_nations < 0) {
    return base_total;
  }
  /* FUN_41f2_0092 tail: local_56 = 100 >> prior; when non-zero the sum is
   * scaled by (8 + (8 >> prior)) / 8 via a 32-bit multiply then >> 3. */
  const int pct = prior_nations >= 31 ? 0 : (100 >> prior_nations);
  if (pct == 0) {
    return base_total;
  }
  const int mult = 8 + (prior_nations >= 31 ? 0 : (8 >> prior_nations));
  return (int)(((long)base_total * (long)mult) >> 3);
}

int reports_score_rating(int total, int difficulty, int* tier_out) {
  if (tier_out) {
    *tier_out = -1;
  }
  if (total <= 0) {
    return 0;
  }
  /* FUN_41f2_0b70: mult = diff+4, +1 past Conquistador, +1 more past Governor
   * -> {4,5,6,8,10}. */
  if (difficulty < 0) {
    difficulty = 0;
  }
  if (difficulty > 4) {
    difficulty = 4;
  }
  int mult = difficulty + 4;
  if (difficulty > 2) {
    mult = difficulty + 5;
  }
  if (difficulty > 3) {
    mult++;
  }
  const int pre = (mult * total) / 100;
  int tier = -1;
  for (int n = 1; n < 25; ++n) {
    if ((n * n) / 3 < pre) {
      tier = n - 1;
    }
  }
  if (tier > 23) {
    tier = 23;
  }
  if (tier_out) {
    *tier_out = tier;
  }
  return pre >> 1;
}

/* File-local since 2026-09-14 (audit SC-26): reports_compute_score is the
 * only caller anywhere in src/, tests/ or tools/. */
static int reports_score_declare_year(const ColonizeCol1Save* col1) {
  if (!col1 || !col1->head.game_options.woi) {
    return 0;
  }
  /* FUN_43f7_1a26: 0x53a7 = year / 100, 0x53a8 = year % 100 (signed chars). */
  return (int)(int8_t)col1->head.king_audience_streak * 100 +
         (int)(int8_t)col1->head.king_audience_last_pick;
}

void reports_compute_score_w(
  const ColonizeWorld* w,
  ColonizeScoreBreakdown* out,
  int human_nation
) {
  const ColonizeCol1Save* col1 = w->col1;
  const ColonizeColonyPool* colonies = w->colonies;
  const EuropeScreen* europe = w->europe;

  memset(out, 0, sizeof(*out));
  out->exploits_tier = -1;
  const int human = reports_clamp_nation(human_nation);

  if (col1) {
    const ColonizeCol1Nation* nat = &col1->nation[human];
    out->year = (int)col1->head.year;
    out->difficulty = (int)col1->head.difficulty;
    if (out->difficulty < 0) {
      out->difficulty = 0;
    }
    if (out->difficulty > 4) {
      out->difficulty = 4;
    }
    out->independence_declared = col1->head.game_options.woi != 0;
    out->independence_achieved = col1->head.game_options.independence_chrome != 0;
    out->scoring_complete = col1->head.game_options.calendar_latch != 0;
    out->declare_year = reports_score_declare_year(col1);

    /* prior_nations: every other Euro power whose nation_flags bit 0x04
     * (independence achieved) is set — current state, not "before us". */
    for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
      if (n != human && (col1->nation[n].nation_flags & 0x04u) != 0) {
        out->prior_nations++;
      }
    }

    /* DOS: with 0x5382|0x10 the report prints only "SCORING COMPLETE" and
     * composes nothing — every component stays 0. */
    if (out->scoring_complete) {
      return;
    }

    /* Citizens: colony population + qualifying map/Europe units — see
     * reports_score_collect_citizen_jobs for the two source-specific rules. */
    {
      int jobs[REPORTS_SCORE_CITIZENS_MAX];
      const int n =
        reports_score_collect_citizen_jobs(col1, human, jobs, NULL, REPORTS_SCORE_CITIZENS_MAX);
      for (int i = 0; i < n; ++i) {
        out->citizens += reports_citizen_points_for_job(jobs[i]);
      }
    }

    out->congress = reports_count_ff_for_nation(col1, human) * 5;
    /* Audit G3: the live store for the human is EuropeScreen.gold — the
     * no-col1 fallback 35 lines below already read it. */
    const uint32_t treasury_gold = europe_nation_gold(europe, col1, human);
    if (treasury_gold >= 1000u) {
      out->treasury = (int)(treasury_gold / 1000u);
    }
    out->villages_burned = (int)nat->villages_burned;
    out->villages_penalty = out->villages_burned * (-1 - out->difficulty);
    out->rebel_sentiment = (int)col1->head.rebel_sentiment_report;
    if (out->independence_achieved && out->declare_year < 1780) {
      out->early_revolution_pts = (1780 - out->declare_year) * 2;
    }
    if (col1->head.game_options.ref_present && nat->liberty_bells_pool >= 100) {
      out->bells_pts = (int)nat->liberty_bells_pool / 100;
      if (out->bells_pts > 100) {
        out->bells_pts = 100;
      }
    }
    if (out->independence_achieved) {
      out->foreign_recognition_pct =
        out->prior_nations >= 31 ? 0 : (100 >> out->prior_nations);
    }
  } else {
    if (colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &colonies->colonies[i];
        if (!c->active) {
          continue;
        }
        for (int p = 0; p < c->colonist_count; ++p) {
          if (!c->colonists[p].active) {
            continue;
          }
          /* Runtime pool lacks profession; count as free colonists. */
          out->citizens += 2;
        }
      }
    }
    const uint32_t gold = europe ? (uint32_t)europe->gold : 0u;
    out->treasury = (int)(gold / 1000u);
  }

  out->base_total = out->early_revolution_pts + out->congress + out->villages_penalty +
                    out->treasury + out->rebel_sentiment + out->bells_pts + out->citizens;
  out->total = reports_score_apply_recognition(
    out->base_total, out->prior_nations, out->independence_achieved
  );
  out->rating = reports_score_rating(out->total, out->difficulty, &out->exploits_tier);
}

/*
 * Geometry below is measured directly off score.png (native = golden/2, per
 * report_screens.md's golden-measurement recipe), not derived from the
 * generic per-report step/margin conventions the F2-F9 reports share — F10
 * has its own hand-tuned layout with a lot of unused wood in the middle.
 */
#define REPORTS_SCORE_TITLE_COLOR 149 /* (199,162,32) — same ink for title/subtitle/Total Score */
#define REPORTS_SCORE_GREEN_COLOR 68 /* (85,150,52) — Citizens/Congress/FF names/Gold/Rebel */
#define REPORTS_SCORE_BAR_FILL_COLOR 68
#define REPORTS_SCORE_BAR_TRACK_COLOR 138 /* (60,32,24) */
#define REPORTS_SCORE_LEFT_X 16
#define REPORTS_SCORE_SUBTITLE_Y 12
#define REPORTS_SCORE_CITIZENS_Y 24
#define REPORTS_SCORE_ICON_X 16
#define REPORTS_SCORE_ICON_Y 32
#define REPORTS_SCORE_ICON_W 288
#define REPORTS_SCORE_CONGRESS_Y 60
#define REPORTS_SCORE_FF_ROW0_Y 67
#define REPORTS_SCORE_FF_ROW_STEP 7
#define REPORTS_SCORE_FF_COL0_X 16
#define REPORTS_SCORE_FF_COL_STEP 72
#define REPORTS_SCORE_FF_COLS 4
#define REPORTS_SCORE_GOLD_Y 150
#define REPORTS_SCORE_REBEL_Y 157
#define REPORTS_SCORE_TOTAL_Y 164
#define REPORTS_SCORE_BAR_X 35
#define REPORTS_SCORE_BAR_Y 186
#define REPORTS_SCORE_BAR_W 250
#define REPORTS_SCORE_BAR_H 7
#define REPORTS_SCORE_BAR_MAX 1000 /* fill = min(total,MAX)/MAX of the track — measured 305/1000 on the golden */

/* Citizens icon strip: one units_job_icon_sprite() portrait per counted
 * citizen (same job list reports_compute_score sums points from), packed
 * left-to-right at a fixed pitch, wrapping to a new row when a row would
 * exceed [x, x+w). Golden (score.png, 48 icons) measured via its row-1
 * boot-shadow pixels: a uniform REPORTS_SCORE_ICON_PITCH_X=8 native advance
 * per icon, 37 icons filling one full row ((37-1)*8=288=w exactly), with
 * the remaining 11 spilling onto a second row — confirming DOS wraps
 * rather than compressing pitch to force everything onto one line. Row 2
 * (and every following even 1-indexed row) is shifted right by half an
 * icon's width, and each row starts half an icon's height below the last
 * — both directly visible in the golden's brick-offset overlap and
 * confirmed against the sprite sheet's own reported 6x16 portrait size. */
#define REPORTS_SCORE_ICON_PITCH_X 8
static void reports_score_draw_citizen_icons(
  const ColonizeReportsView* view,
  ColonizeFramebuffer8* fb,
  const int* icon_ids,
  int count,
  int x,
  int y,
  int w
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_SCORE);
  if (!view || !icons || count <= 0 || w <= 0) {
    return;
  }
  int icon_w = 6;
  int icon_h = 16;
  for (int i = 0; i < count; ++i) {
    const int probe = icon_ids[i];
    if (probe >= 0 && probe < icons->sprite_count) {
      icon_w = icons->sprites[probe].width;
      icon_h = icons->sprites[probe].height;
      break;
    }
  }
  const int per_row = w / REPORTS_SCORE_ICON_PITCH_X + 1;
  const int row_dy = icon_h / 2;
  const int row_dx = icon_w / 2;
  for (int i = 0; i < count; ++i) {
    const int icon = icon_ids[i];
    if (icon < 0 || icon >= icons->sprite_count) {
      continue;
    }
    const int row = i / per_row;
    const int col = i % per_row;
    int ix = x + col * REPORTS_SCORE_ICON_PITCH_X;
    if (row % 2 == 1) {
      ix += row_dx;
    }
    const int iy = y + row * row_dy;
    ss_blit_sprite(icons, icon, fb, ix, iy);
  }
}

void reports_render_score(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  uint32_t turn_number,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  char* line,
  size_t line_sz
) {
  (void)turn_number;
  ColonizeScoreBreakdown sc;
  reports_compute_score_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(colonies), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL), .europe=(EuropeScreen*)(europe)}, &sc, human);

  const ColonizeFont* body_font = (view && view->title_font_ok) ? &view->title_font : font;

  const char* diff_name = reports_difficulty_title(sc.difficulty);
  const char* nation_adj = reports_nation_adjective(human);

  /* Subtitle: "<difficulty rank> <leader> of the <nation>:  <Season> <year>"
   * (golden: "Viceroy Michiel De Ruyter of the Dutch:  Autumn 1630"). */
  if (col1) {
    const char* leader =
      (human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT && col1->player[human].name[0])
        ? col1->player[human].name
        : "";
    snprintf(
      line,
      line_sz,
      "%s %s of the %s:  %s %d",
      diff_name,
      leader,
      nation_adj,
      reports_season_name(col1->head.autumn), /* NAMES.TXT @SEASONS rows 0/1 */
      sc.year
    );
  } else {
    snprintf(line, line_sz, "Turn %u", (unsigned)turn_number);
  }
  if (body_font) {
    const int w = font_text_width(body_font, line);
    reports_draw_line(
      body_font, fb, (fb->width - w) / 2, REPORTS_SCORE_SUBTITLE_Y, line, REPORTS_SCORE_TITLE_COLOR
    );
  }

  /* LABELS.TXT @MISC: #59 "Gold", #121 "Total Score", #115 "Citizens",
   * #134 "Continental Congress", #69 "Rebel" + #71 "Sentiment". */
  char w1[48], w2[48];
  if (sc.scoring_complete) {
    /* FUN_41f2_0092: 0x5382|0x10 -> only @MISC #126 "SCORING COMPLETE",
     * centered at y=0x61, nothing else composed. */
    reports_misc_word(126, "", w1, sizeof(w1));
    if (body_font) {
      const int w = font_text_width(body_font, w1);
      reports_draw_line(body_font, fb, (fb->width - w) / 2, 0x61, w1, REPORTS_SCORE_TITLE_COLOR);
    }
    return;
  }
  if (!col1) {
    snprintf(line, line_sz, "%-24s%d", reports_misc_word(59, "", w1, sizeof(w1)), sc.treasury);
    reports_draw_line(
      body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_GOLD_Y, line, REPORTS_SCORE_GREEN_COLOR
    );
    snprintf(
      line, line_sz, "%-24s%d", reports_misc_word(121, "", w1, sizeof(w1)), sc.total
    );
    reports_draw_line(
      body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_TOTAL_Y, line, REPORTS_SCORE_TITLE_COLOR
    );
    return;
  }

  /* Citizens line + icon strip. */
  snprintf(
    line,
    line_sz,
    "%s %s:  +%d",
    nation_adj,
    reports_misc_word(115, "", w1, sizeof(w1)),
    sc.citizens
  );
  reports_draw_line(
    body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_CITIZENS_Y, line, REPORTS_SCORE_GREEN_COLOR
  );
  {
    /* bugs.md (score_screen.SAV): strip icons are per-citizen sprites —
     * equipped map art for units (Continentals, veterans with kit), job
     * portraits for colony population. */
    int jobs[REPORTS_SCORE_CITIZENS_MAX];
    int icons[REPORTS_SCORE_CITIZENS_MAX];
    const int n =
      reports_score_collect_citizen_jobs(col1, human, jobs, icons, REPORTS_SCORE_CITIZENS_MAX);
    reports_score_draw_citizen_icons(
      view, fb, icons, n, REPORTS_SCORE_ICON_X, REPORTS_SCORE_ICON_Y, REPORTS_SCORE_ICON_W
    );
  }

  /* Continental Congress line + 4-column Founding Father name grid. */
  snprintf(
    line,
    line_sz,
    "%s %s:  +%d",
    nation_adj,
    reports_misc_word(134, "", w1, sizeof(w1)),
    sc.congress
  );
  reports_draw_line(
    body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_CONGRESS_Y, line, REPORTS_SCORE_GREEN_COLOR
  );
  {
    int shown = 0;
    for (int idx = 0; idx < (int)COLONIZE_COL1_FF_COUNT; ++idx) {
      if (!founding_fathers_nation_has(col1, human, idx)) {
        continue;
      }
      const int col = shown % REPORTS_SCORE_FF_COLS;
      const int row = shown / REPORTS_SCORE_FF_COLS;
      const int x = REPORTS_SCORE_FF_COL0_X + col * REPORTS_SCORE_FF_COL_STEP;
      const int y = REPORTS_SCORE_FF_ROW0_Y + row * REPORTS_SCORE_FF_ROW_STEP;
      reports_draw_line(body_font, fb, x, y, reports_ff_name(idx), REPORTS_SCORE_GREEN_COLOR);
      shown++;
    }
  }

  /*
   * Component lines in DOS block order (FUN_41f2_0092), each printed only
   * when its gate holds — the golden (score.png) has exactly Gold / Rebel
   * Sentiment / Total Score because that save has >= 1000 gold, a non-zero
   * 0x53d0 and nothing else. Gold's y=150 is the golden's; extra lines push
   * the block up so Total Score never runs into the y=186 bar.
   */
  char lines[8][96];
  uint8_t colors[8];
  int n_lines = 0;
  const uint32_t score_gold = europe_nation_gold(europe, col1, human); /* audit G3 */
  if (sc.treasury > 0 || score_gold >= 1000u) {
    /* "$" — same coin-glyph convention as the map sidebar's own gold line
     * (map_panel.c: "Gold: %d$"), not a literal "g" suffix. */
    snprintf(
      lines[n_lines],
      sizeof(lines[0]),
      "%s:  (%u$) +%d",
      reports_misc_word(59, "", w1, sizeof(w1)),
      (unsigned)score_gold,
      sc.treasury
    );
    colors[n_lines++] = REPORTS_SCORE_GREEN_COLOR;
  }
  if (sc.villages_burned != 0) {
    snprintf(
      lines[n_lines],
      sizeof(lines[0]),
      "%d %s:  %d",
      sc.villages_burned,
      reports_misc_word(117, "", w1, sizeof(w1)),
      sc.villages_penalty
    );
    colors[n_lines++] = REPORTS_SCORE_GREEN_COLOR;
  }
  if (sc.rebel_sentiment != 0) {
    snprintf(
      lines[n_lines],
      sizeof(lines[0]),
      "%s %s:  +%d",
      reports_misc_word(69, "", w1, sizeof(w1)),
      reports_misc_word(71, "", w2, sizeof(w2)),
      sc.rebel_sentiment
    );
    colors[n_lines++] = REPORTS_SCORE_GREEN_COLOR;
  }
  if (sc.independence_achieved && sc.declare_year < 1780) {
    /* DOS prints the CURRENT season/year in the parenthesis while scoring
     * from the latched declaration year. */
    snprintf(
      lines[n_lines],
      sizeof(lines[0]),
      "%s (%s %d):  +%d",
      reports_misc_word(142, "", w1, sizeof(w1)),
      reports_season_name(col1->head.autumn), /* NAMES.TXT @SEASONS rows 0/1 */
      sc.year,
      sc.early_revolution_pts
    );
    colors[n_lines++] = REPORTS_SCORE_GREEN_COLOR;
  }
  if (col1->head.game_options.ref_present && col1->nation[human].liberty_bells_pool >= 100) {
    /* DOS label is a runtime pointer (DS:0x97e4) outside the @MISC table;
     * text unresolved statically. */
    snprintf(lines[n_lines], sizeof(lines[0]), "Liberty Bells:  +%d", sc.bells_pts);
    colors[n_lines++] = REPORTS_SCORE_GREEN_COLOR;
  }
  if (sc.independence_achieved) {
    /* "Independence Achieved [(N prior nations)]:  +NN%" (#116 #119 #143). */
    char prior[48] = "";
    if (sc.prior_nations != 0) {
      snprintf(
        prior,
        sizeof(prior),
        " (%d %s)",
        sc.prior_nations,
        reports_misc_word(143, "", w2, sizeof(w2))
      );
    }
    snprintf(
      lines[n_lines],
      sizeof(lines[0]),
      "%s %s%s:  +%d%%",
      reports_misc_word(116, "", w1, sizeof(w1)),
      reports_misc_word(119, "", w2, sizeof(w2)),
      prior,
      sc.foreign_recognition_pct
    );
    colors[n_lines++] = REPORTS_SCORE_GREEN_COLOR;
  }
  snprintf(
    lines[n_lines], sizeof(lines[0]), "%s: %d", reports_misc_word(121, "", w1, sizeof(w1)), sc.total
  );
  colors[n_lines++] = REPORTS_SCORE_TITLE_COLOR;
  {
    const int step = REPORTS_SCORE_REBEL_Y - REPORTS_SCORE_GOLD_Y;
    int y0 = REPORTS_SCORE_GOLD_Y;
    const int last_max = REPORTS_SCORE_BAR_Y - 8;
    if (y0 + step * (n_lines - 1) > last_max) {
      y0 = last_max - step * (n_lines - 1);
    }
    for (int i = 0; i < n_lines; ++i) {
      reports_draw_line(body_font, fb, REPORTS_SCORE_LEFT_X, y0 + i * step, lines[i], colors[i]);
    }
  }

  /* Bottom progress bar: proportional fill toward a nominal 1000-point
   * score, not toward anything display-labeled — golden's fill measures
   * 76/250px = 30.4% against a Total Score of 305/1000 = 30.5%. */
  fb_fill_rect(
    fb,
    REPORTS_SCORE_BAR_X,
    REPORTS_SCORE_BAR_Y,
    REPORTS_SCORE_BAR_W,
    REPORTS_SCORE_BAR_H,
    REPORTS_SCORE_BAR_TRACK_COLOR
  );
  int fill_total = sc.total;
  if (fill_total < 0) {
    fill_total = 0;
  }
  if (fill_total > REPORTS_SCORE_BAR_MAX) {
    fill_total = REPORTS_SCORE_BAR_MAX;
  }
  const int fill_w = (REPORTS_SCORE_BAR_W * fill_total) / REPORTS_SCORE_BAR_MAX;
  if (fill_w > 0) {
    fb_fill_rect(
      fb,
      REPORTS_SCORE_BAR_X,
      REPORTS_SCORE_BAR_Y,
      fill_w,
      REPORTS_SCORE_BAR_H,
      REPORTS_SCORE_BAR_FILL_COLOR
    );
  }
}

/* Centered text across the 320px screen (DOS FUN_281f_0100 / 01c8). */
/* ===================== Hall of Fame & Exploits report (reports_draw_centered .. reports_render_exploits) ===================== */
static void reports_draw_centered(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y,
  const char* text,
  uint8_t color
) {
  if (!fb || !text) {
    return;
  }
  const int w = font ? font_text_width(font, text) : 0;
  reports_draw_line(font, fb, (fb->width - w) / 2, y, text, color);
}

void reports_render_hall_of_fame(
  const ColonizeReportsView* view,
  const ColonizeHofRow* entries,
  int entry_count,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
) {
  if (!fb || !fb->pixels) {
    return;
  }
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (view && view->background_ok[COLONIZE_REPORT_SCORE]) {
    pik_blit(&view->backgrounds[COLONIZE_REPORT_SCORE], fb, 0, 0);
  }
  const ColonizeFont* body_font = (view && view->title_font_ok) ? &view->title_font : font;

  /*
   * FUN_41f2_0f56 presenter: title (@MISC #192) centered at y=3, then each
   * of the first 5 entries as three centered lines, line pitch = font
   * height + 2 starting at y=0x10. Strings from LABELS.TXT @MISC — "of the"
   * #19, "Free" #191, "President" #195, "General, Continental Army" #196,
   * "Leader" #197, "Colonies" #95, "to" #193, "A.D." #194, "Score" #198,
   * "Colonization_Rating" #199 — the difficulty name table and the nation
   * adjective / @INDEPENDENT name are resolved by the caller.
   */
  char w1[64], w2[64], w3[64];
  reports_draw_centered(
    body_font, fb, 3, reports_misc_word(192, "", w1, sizeof(w1)),
    REPORTS_SCORE_TITLE_COLOR
  );

  const int step = body_font ? body_font->max_height + 2 : 8;
  int y = 0x10;
  char line[200];
  const int shown = entry_count > COLONIZE_HOF_SHOWN_MAX ? COLONIZE_HOF_SHOWN_MAX : entry_count;
  for (int i = 0; i < shown; ++i) {
    const ColonizeHofRow* e = &entries[i];
    const char* diff_name = reports_difficulty_title(e->difficulty);
    snprintf(
      line,
      sizeof(line),
      "%d. %s %s %s %s%s%s",
      i + 1,
      diff_name,
      e->leader,
      reports_misc_word(19, "", w1, sizeof(w1)),
      e->declared ? reports_misc_word(191, "", w2, sizeof(w2)) : "",
      e->declared ? " " : "",
      e->nation
    );
    reports_draw_centered(body_font, fb, y, line, REPORTS_SCORE_GREEN_COLOR);
    y += step;

    char title[120];
    if (e->achieved) {
      snprintf(
        title,
        sizeof(title),
        "%s, %s",
        reports_misc_word(195, "", w1, sizeof(w1)),
        e->independent_name[0] ? e->independent_name : e->nation
      );
    } else if (e->declared) {
      snprintf(
        title, sizeof(title), "%s", reports_misc_word(196, "", w1, sizeof(w1))
      );
    } else {
      snprintf(
        title,
        sizeof(title),
        "%s, %s %s",
        reports_misc_word(197, "", w1, sizeof(w1)),
        e->nation,
        reports_misc_word(95, "", w2, sizeof(w2))
      );
    }
    snprintf(
      line,
      sizeof(line),
      "%s %s %s %d. %s: %d",
      title,
      reports_misc_word(193, "", w1, sizeof(w1)),
      reports_misc_word(194, "", w2, sizeof(w2)),
      e->year,
      reports_misc_word(198, "", w3, sizeof(w3)),
      e->score
    );
    reports_draw_centered(body_font, fb, y, line, REPORTS_SCORE_GREEN_COLOR);
    y += step;

    snprintf(
      line,
      sizeof(line),
      "--- %s: %d%% ---",
      reports_misc_word(199, "", w1, sizeof(w1)),
      e->rating
    );
    reports_draw_centered(body_font, fb, y, line, REPORTS_SCORE_TITLE_COLOR);
    y += step;
  }
}

void reports_remap_exploits_sheet(const ColonizeReportsView* view, ColonizeSpriteSheet* sheet) {
  /* bugs.md #402: SCORE<nn>.SS shares WOODPAN2.PIK's low palette block and
   * carries the painting's own colours in the slots WOODPAN2 leaves black —
   * the reserved-DAC-block pattern (crown-europe batch: merge, never remap).
   * The old nearest-colour remap here crushed the painting into the 115
   * woodpanel colours. Keep the sheet indices raw; game_render merges the
   * sheet palette into the screen palette's black slots instead. */
  (void)view;
  (void)sheet;
}

void reports_render_exploits(
  const ColonizeReportsView* view,
  const ColonizeExploitsView* ex,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
) {
  if (!fb || !fb->pixels || !ex) {
    return;
  }
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (view && view->exploits_bg_ok) {
    pik_blit(&view->exploits_bg, fb, 0, 0);
  } else if (view && view->background_ok[COLONIZE_REPORT_SCORE]) {
    pik_blit(&view->backgrounds[COLONIZE_REPORT_SCORE], fb, 0, 0);
  }
  const ColonizeFont* body_font = (view && view->title_font_ok) ? &view->title_font : font;
  /*
   * FUN_41f2_0b70 (OVL06 asm 0x3887..0x39d0, bugs.md #402): all inks are
   * WOODPAN2.PIK DAC indices — 0xfc (gold 199,162,32) for the @EXPLOITS
   * headers, the named line and the picked-tier row, 0xfe (green 85,150,52)
   * for the other @SCORE rows. Headers centred full-width from y=5; @SCORE
   * rows centred within x=0xa0 w=0xa0 (the RIGHT half) at
   * y = 0xc3 - (h+1)*(i+1); the named line centred within x=0x22 w=0x8c at
   * y=0x8e; SCORE<tier+1>.SS frame 0 at x=100.
   */
  const int h = body_font ? body_font->max_height + 1 : 8;
  int y = 5;
  for (int i = 0; i < ex->header_count && i < 3; ++i) {
    reports_draw_centered(body_font, fb, y, ex->header[i], 0xfc);
    y += h;
  }
  for (int i = 0; i < ex->category_count && i < 24; ++i) {
    const uint8_t ink = (i == ex->category_count - 1) ? 0xfc : 0xfe;
    const int tw = body_font ? font_text_width(body_font, ex->categories[i]) : 0;
    reports_draw_line(
      body_font, fb, 0xa0 + (0xa0 - tw) / 2, 0xc3 - h * (i + 1), ex->categories[i], ink
    );
  }
  if (ex->sheet && ex->sheet->sprite_count > 0) {
    /* SCORE<nn>.SS is a painting of the nn-th @SCORE item (SCORE17 = the
     * university), drawn AFTER the category list so the build-up ends hidden
     * under the reveal. Placement is the sheet's own anchor pair (ax=104,
     * ay=138 in every SCORE sheet) through the DOS centring rule
     * (anchor_x - w/2, anchor_y - h + 1) — centre x 104 lines up exactly
     * with the named-line box below (0x22 + 0x8c/2), bottom edge one row
     * above the named line at y=0x8e (bugs.md: sprite was misaligned vs its
     * caption at the old fixed x=100). */
    const ColonizeSprite* sp = &ex->sheet->sprites[0];
    ss_blit_sprite(
      ex->sheet, 0, fb, sp->anchor_x - sp->width / 2, sp->anchor_y - sp->height + 1
    );
  }
  if (ex->named[0]) {
    const int tw = body_font ? font_text_width(body_font, ex->named) : 0;
    reports_draw_line(body_font, fb, 0x22 + (0x8c - tw) / 2, 0x8e, ex->named, 0xfc);
  }
}
