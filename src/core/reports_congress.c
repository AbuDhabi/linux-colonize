/*
 * Sections:
 *  - Religious & Continental Congress report (~line 28)
 *  - Labor report (~line 510)
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

/* ===================== Religious & Continental Congress report (reports_render_religious .. reports_render_congress_page2) ===================== */
void reports_render_religious(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_RELIGIOUS);
  if (!col1) {
    return;
  }
  const ColonizeCol1Nation* nat = &col1->nation[human];
  const unsigned needed = nat->needed_crosses;
  const unsigned current = nat->current_crosses;
  if (needed == 0) {
    return;
  }
  /* `3f41:0670`: BX = nation+0x2e (current), DX = nation+0x30 (needed),
   * sprite 0x39 (= ICONS.SS #56), pushed args x=10, y=25, w=0x12c, min_w=0,
   * split=0, flags=1. Same routine as the Congress bells bar. */
  reports_draw_dos_icon_bar(
    icons, font, fb, REPORTS_CROSS_ICON, REPORTS_CROSS_X, REPORTS_CROSS_Y, REPORTS_CROSS_W, 0,
    (int)current, (int)needed
  );
}

/*
 * Congress FF portraits (CC-xx.SS) rarely change once loaded and page 2 blits
 * them every frame it's open — cache per index so each sheet is parsed from
 * disk once, not per frame.
 */
static const ColonizeSpriteSheet* reports_ff_portrait_sheet(const char* data_dir, int index) {
  static bool s_tried[COLONIZE_COL1_FF_COUNT];
  static ColonizeSpriteSheet s_sheet[COLONIZE_COL1_FF_COUNT];
  if (index < 0 || index >= (int)COLONIZE_COL1_FF_COUNT) {
    return NULL;
  }
  if (s_tried[index]) {
    return s_sheet[index].sprite_count > 0 ? &s_sheet[index] : NULL;
  }
  s_tried[index] = true;
  char name[32];
  char path[512];
  char err[128];
  snprintf(name, sizeof(name), "CC-%02d.SS", index);
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    return NULL;
  }
  if (!ss_load(path, &s_sheet[index], err, sizeof(err)) || s_sheet[index].sprite_count <= 0) {
    ss_free(&s_sheet[index]);
    memset(&s_sheet[index], 0, sizeof(s_sheet[index]));
    return NULL;
  }
  return &s_sheet[index];
}

/*
 * Page 1 (golden: continental_p1.png). Native 320×200 coords, all measured
 * off the golden — DOS's FUN_3f41_0618/06d0 (Congress F3) is unannotated
 * asm-derived C with no meaningful names, so positions are pixel-measured
 * rather than transcribed. FUN_3f41_0ae6's 25-slot FF-name loop *did* decode
 * cleanly: 4 columns, col step 0x4e (78px), row step font-height+2 — used
 * verbatim below.
 */
#define REPORTS_CONGRESS_BELL_ICON 62 /* ICONS.SS #62 */
#define REPORTS_CONGRESS_FLAG_ICON 123 /* ICONS.SS #123 */
#define REPORTS_CONGRESS_CROWN_ICON 124 /* ICONS.SS #124 */
/* 0-based ICONS.SS index for NAMES.TXT @UNIT icon field (1-based) minus 1. */
#define REPORTS_CONGRESS_ICON_REGULARS 125 /* @UNIT Regulars icon 126 */
#define REPORTS_CONGRESS_ICON_CAVALRY 126 /* @UNIT Cavalry icon 127 */
#define REPORTS_CONGRESS_ICON_ARTILLERY 9 /* @UNIT Artillery icon 10 */
#define REPORTS_CONGRESS_ICON_MANOWAR 127 /* @UNIT Man-O-War icon 128 */

#define REPORTS_CONGRESS_TEXT1_Y 25 /* "Next Continental Congress Session: (...)" — DOS local_5c init */
/* `3f41:0890`: x = local_58 (initialised to 4 at 3f41:0709), w = 0x12c,
 * min_w = 0; the bar's y is the header line's y advanced by one text row
 * (`ADD local_5c, font_height + 2`) right before the call. */
#define REPORTS_CONGRESS_BELLS_X 4
#define REPORTS_CONGRESS_BELLS_W 300

#define REPORTS_CONGRESS_TEXT2_Y 59 /* "Rebel Sentiment: XX%  Tory Sentiment: YY%" */
#define REPORTS_CONGRESS_SENT_X 4
#define REPORTS_CONGRESS_SENT_Y 71
#define REPORTS_CONGRESS_SENT_W 285 /* measured; always full (rounded rebel:tory split) */
#define REPORTS_CONGRESS_SENT_H 9
#define REPORTS_CONGRESS_SENT_SLOTS 50 /* rounding budget for the flag/crown split */

#define REPORTS_CONGRESS_TEXT3_Y 92 /* "<Nation> Expeditionary Force:" */
#define REPORTS_CONGRESS_FORCE_Y 102
#define REPORTS_CONGRESS_FORCE_H 13
/* Natural (unstretched) tally width per unit, px ×10 — measured avg ~2.2px/unit. */
#define REPORTS_CONGRESS_FORCE_STEP_X10 22

#define REPORTS_CONGRESS_FF_HEADER_Y 125 /* "Founding Fathers:" */
#define REPORTS_CONGRESS_FF_COL_X0 8
#define REPORTS_CONGRESS_FF_COL_STEP 78 /* FUN_3f41_0ae6: unaff_BP-0x68 += 0x4e */

void reports_render_congress_page1(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* body_font,
  ColonizeFramebuffer8* fb,
  char* line,
  size_t line_sz
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_ICONS_CONGRESS_P1);
  /* Golden text is compact (e.g. "Peter Stuyvesant" fits a 78px column) —
   * FONTTINY, like report titles, not the FONTSMAL other reports' bodies use. */
  const ColonizeFont* font = (view && view->title_font_ok) ? &view->title_font : body_font;
  if (!col1) {
    reports_draw_line(font, fb, 8, REPORTS_CONGRESS_TEXT1_Y, "(no Col1 save loaded)", 14);
    return;
  }

  const ColonizeCol1Nation* nat = &col1->nation[human];
  const int step = reports_line_step(font);

  /* LABELS.TXT @MISC: #112 "Next Continental Congress Session", #69 "Rebel",
   * #70 "Tory", #71 "Sentiment", #85 "Expeditionary Force", #89 "Founding
   * Fathers" — composed with the golden's punctuation/spacing. */
  char w1[48], w2[48], w3[48];
  const int woi = col1->head.game_options.woi != 0;
  const int ref_arrived = col1->head.game_options.ref_present != 0;
  /* DOS reads DS:0x53d4 raw and hands it straight to the nationality-string
   * sub (FUN_281f_09a4), both here and at the intervention lineup below
   * (viceroy_unpacked.c 69686 / 69777). No crown/self exclusion, no "pick
   * another slot" search — the slot is written once by the King module when
   * independence is declared (ai_king_write_rival_nation_slots). An unset
   * slot (-1) simply yields an empty adjective. */
  const int ally = (int)col1->head.rival_nation_slot_1;
  /*
   * DOS FUN_3f41_06d0 (viceroy_unpacked.c 69670-69695) builds ONE header
   * string with three outcomes:
   *   0x5382&1 clear                -> "Next Continental Congress Session:"
   *                                    (+ " (name)" when head+0x12 >= 0)
   *   0x5382&1 set, 0x5382&2 clear  -> "<0x53d4 adjective> Intervention:"
   *   both set (REF has arrived)    -> the buffer stays empty; nothing drawn.
   * The port used to draw the Intervention header for the whole war.
   */
  line[0] = '\0';
  if (!woi) {
    snprintf(
      line,
      line_sz,
      "%s:  (%s)",
      reports_misc_word(112, "", w1, sizeof(w1)),
      nat->next_founding_father >= 0
        ? reports_ff_name(nat->next_founding_father)
        : reports_misc_display_word(3, "")
    );
  } else if (!ref_arrived) {
    /* bugs.md #229/257: after declaring, FF elections are over — the bell bar
     * counts toward the foreign intervention instead. DOS's top header is
     * just "<nation> Intervention:" (@MISC 113); "Intervention Force" (@MISC
     * 111) is the header of the soldier lineup row further down. */
    snprintf(
      line,
      line_sz,
      "%s %s:",
      reports_nation_adjective(ally),
      reports_misc_word(113, "", w1, sizeof(w1))
    );
  }
  if (line[0]) {
    reports_draw_line(font, fb, 8, REPORTS_CONGRESS_TEXT1_Y, line, 15);
  }

  {
    const unsigned pool = founding_fathers_bells_pool(col1, human);
    const unsigned need = founding_fathers_bells_needed(col1, human);
    if (need > 0 && pool > 0) {
      /*
       * `3f41:07c8`..`3f41:08a4`: DX = the next-FF threshold
       * (`FUN_4345_0982`), BX = min(pool, threshold) — DOS clamps the drawn
       * count so an over-full pool cannot run past the bar. The bar itself is
       * always the full 300px and its step comes from the threshold, so the
       * fill is the length of the drawn run. On continental_p1.png (pool
       * 1135, need 1849) that is step 1 / shift 3 / 141 bells from x=4 to
       * x=179 — of which the 36 that get a 2px gap show their 2px left edge.
       */
      const unsigned drawn = pool < need ? pool : need;
      reports_draw_dos_icon_bar(
        icons,
        font,
        fb,
        REPORTS_CONGRESS_BELL_ICON,
        REPORTS_CONGRESS_BELLS_X,
        REPORTS_CONGRESS_TEXT1_Y + step,
        REPORTS_CONGRESS_BELLS_W,
        0 /* min_w: no centring */,
        (int)drawn,
        (int)need
      );
    }
  }

  const unsigned rebel_pct = nat->rebel_sentiment > 100 ? 100 : nat->rebel_sentiment;
  snprintf(
    line,
    line_sz,
    "%s %s: %u%%  %s %s: %u%%",
    reports_misc_word(69, "", w1, sizeof(w1)),
    reports_misc_word(71, "", w3, sizeof(w3)),
    rebel_pct,
    reports_misc_word(70, "", w2, sizeof(w2)),
    w3,
    100 - rebel_pct
  );
  reports_draw_line(font, fb, 8, REPORTS_CONGRESS_TEXT2_Y, line, 15);

  {
    const int flags = (int)((REPORTS_CONGRESS_SENT_SLOTS * rebel_pct + 50) / 100);
    const int crowns = REPORTS_CONGRESS_SENT_SLOTS - flags;
    reports_draw_icon_bar_pair(
      icons,
      fb,
      REPORTS_CONGRESS_FLAG_ICON,
      flags,
      REPORTS_CONGRESS_CROWN_ICON,
      crowns,
      REPORTS_CONGRESS_SENT_X,
      REPORTS_CONGRESS_SENT_Y,
      REPORTS_CONGRESS_SENT_W,
      REPORTS_CONGRESS_SENT_H
    );
  }

  snprintf(
    line,
    line_sz,
    "%s %s:",
    reports_nation_adjective(human),
    reports_misc_word(85, "", w1, sizeof(w1))
  );
  reports_draw_line(font, fb, 8, REPORTS_CONGRESS_TEXT3_Y, line, 15);

  {
    /* Storage order is [regulars, dragoons, man-o-wars, artillery]; the
     * golden displays regulars, cavalry, artillery, man-o-war. */
    static const int kForceIndex[4] = {0, 1, 3, 2};
    static const int kForceIcon[4] = {
      REPORTS_CONGRESS_ICON_REGULARS,
      REPORTS_CONGRESS_ICON_CAVALRY,
      REPORTS_CONGRESS_ICON_ARTILLERY,
      REPORTS_CONGRESS_ICON_MANOWAR
    };
    static const int kForceX[4] = {4, 128, 193, 260};
    static const int kForceXEnd = 316;
    /*
     * bugs.md "REF units squished unnecessarily": DOS draws this row through
     * the shared icon-row drawer (FUN_1097_0174 — the 3f41 builder brackets
     * the force line with DS:0x70 = 1), which packs icons at their natural
     * width and only shrinks the step when the run would overflow its cell.
     * The old width = count * 2.2px spread reproduced the 56-regular
     * golden's 2px pitch but crushed small counts too. Natural width capped
     * at the cell (next column start, screen margin for the last) gives both:
     * 56 regulars in a 124px cell still squeeze to ~2px, a fresh campaign's
     * 15 stand shoulder to shoulder.
     */
    for (int i = 0; i < 4; ++i) {
      const int amount = (int)col1->head.expeditionary_force[kForceIndex[i]];
      if (amount <= 0) {
        continue;
      }
      const int avail = ((i < 3) ? kForceX[i + 1] : kForceXEnd) - kForceX[i];
      int iw = 16;
      if (icons && kForceIcon[i] >= 0 && kForceIcon[i] < icons->sprite_count) {
        iw = icons->sprites[kForceIcon[i]].width;
      }
      int w = amount * iw;
      if (w > avail) {
        w = avail;
      }
      reports_draw_icon_bar(
        icons,
        font,
        fb,
        kForceIcon[i],
        kForceX[i],
        REPORTS_CONGRESS_FORCE_Y,
        w,
        REPORTS_CONGRESS_FORCE_H,
        amount,
        true,
        0
      );
    }
  }

  /*
   * bugs.md / DOS 3f41 (viceroy_unpacked.c ~69774): when the foreign-
   * intervention pool (DS:0x53e2.. = head.backup_force, seeded on declaring
   * independence and drained by bell spending) is non-zero, a second force
   * row appears under the REF lines: "<Ally nationality> Intervention Force"
   * (@MISC 111, DS string 0x2e98) with the pool's four counts. The port's
   * fixed layout has 10px here, so the counts are drawn as one text line.
   */
  /*
   * bugs.md #230 / DOS 3f41 (~69774): when the foreign-intervention pool
   * (DS:0x53e2 = head.backup_force) is non-zero, a second force row appears
   * under the REF lines in the SAME format — icon lineup of regulars,
   * cavalry, artillery, man-o-wars (bugs.md #264: it gets a full-height band
   * and pushes the Founding Fathers block down).
   */
  int pool_sum = 0;
  for (int i = 0; i < 4; ++i) {
    pool_sum += (int)col1->head.backup_force[i];
  }
  /* bugs.md #264: the intervention row needs its own full band — when it is
   * present, the Founding Fathers header (and list) move down below it
   * instead of sharing the fixed 10px gap and overlapping both neighbors. */
  const int interv_row_y = REPORTS_CONGRESS_FORCE_Y + REPORTS_CONGRESS_FORCE_H + 2;
  /* bugs.md: the "<Ally> Intervention Force:" header gets its own row, the
   * way the REF's "<Nation> Expeditionary Force:" header does (text at
   * TEXT3_Y, lineup at FORCE_Y) — the lineup then owns the full row width
   * below it instead of starting after the header at x=110. */
  const int interv_icons_y = interv_row_y + step;
  const int ff_header_y = pool_sum > 0
    ? interv_icons_y + REPORTS_CONGRESS_FORCE_H + 2
    : REPORTS_CONGRESS_FF_HEADER_Y;
  {
    if (pool_sum > 0) {
      /* bugs.md #251: the intervention lineup is Continental Army /
       * Continental Cavalry (@UNIT icons 129/130), not Regulars/Cavalry,
       * and it carries its own "<Ally> Intervention Force:" header. */
      static const int kForceIndex2[4] = {0, 1, 3, 2};
      static const int kForceIcon2[4] = {
        128 /* Cont. Army icon 129 */,
        129 /* Cont. Cav. icon 130 */,
        REPORTS_CONGRESS_ICON_ARTILLERY,
        REPORTS_CONGRESS_ICON_MANOWAR
      };
      /* Same four cells as the REF lineup above, now that the header has
       * moved off this row. */
      static const int kForceX2[4] = {4, 128, 193, 260};
      const int row_y = interv_icons_y;
      const int row_h = REPORTS_CONGRESS_FORCE_H;
      snprintf(
        line, line_sz, "%s %s:",
        reports_nation_adjective(ally), /* DOS: FUN_281f_09a4(DS:0x53d4), raw */
        reports_misc_word(111, "", w1, sizeof(w1))
      );
      reports_draw_line(font, fb, 8, interv_row_y, line, 15);
      for (int i = 0; i < 4; ++i) {
        const int amount = (int)col1->head.backup_force[kForceIndex2[i]];
        if (amount <= 0) {
          continue;
        }
        const int avail = ((i < 3) ? kForceX2[i + 1] : 316) - kForceX2[i];
        int iw = 16;
        if (icons && kForceIcon2[i] >= 0 && kForceIcon2[i] < icons->sprite_count) {
          iw = icons->sprites[kForceIcon2[i]].width;
        }
        int w = amount * iw;
        if (w > avail) {
          w = avail;
        }
        reports_draw_icon_bar(
          icons, font, fb, kForceIcon2[i], kForceX2[i], row_y, w,
          row_h > 6 ? row_h : 6, amount, true, 0
        );
      }
    }
  }

  snprintf(line, line_sz, "%s:", reports_misc_word(89, "", w1, sizeof(w1)));
  reports_draw_line(font, fb, 8, ff_header_y, line, 15);
  {
    int shown = 0;
    int y = ff_header_y + step;
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      if (!founding_fathers_nation_has(col1, human, i)) {
        continue;
      }
      const int col = shown % 4;
      const int row = shown / 4;
      reports_draw_line(
        font,
        fb,
        REPORTS_CONGRESS_FF_COL_X0 + col * REPORTS_CONGRESS_FF_COL_STEP,
        y + row * step,
        reports_ff_name(i),
        15
      );
      shown++;
    }
  }
}

/*
 * Page 2 — CCBKGD.PIK hall, full-bleed group portrait, no title/text/OK
 * chrome. Transcribed from DOS `FUN_4345_01a6` (4345:01b5..0246), which is a
 * plain 25-iteration loop: take the next father id out of the draw-order
 * table at DS:0x123a, skip it unless this nation owns it (FUN_281f_07b4),
 * build "CC-" + a leading "0" below 10 + the id (DS strings 0x1234 / 0x1238),
 * load that sheet and blit it at the position the sheet itself carries
 * (`ES:[BX+0x46]` / `ES:[BX+0x48]` — the per-sprite anchor `ss_load` already
 * parses out of the file header).
 *
 * So there is no coordinate table to measure: every father's position is in
 * his own CC-xx.SS. The earlier pass template-matched 10 of them off
 * continental_p2.png and skipped the other 15 as unknown, which is why Paul
 * Revere and Francis Drake never appeared (bugs.md). All 25 now draw, and the
 * four positions that pass had matched cleanly (De La Salle, Washington,
 * Jefferson, Franklin) come out byte-identical from the anchors.
 */
/*
 * DS:0x123a, verbatim: the back-to-front paint order. It matters because the
 * sprites are photo cutouts with opaque canvas margins rather than clean
 * mattes, so a later sprite can blank an earlier one outside its silhouette.
 * (Sanity check: the order tracks each sheet's anchor baseline ascending,
 * 118 -> 199, which is what a hand-authored depth order looks like.)
 */
static const uint8_t k_ff_portrait_draw_order[COLONIZE_COL1_FF_COUNT] = {
  6, 20, 1, 23, 24, 22, 7, 3, 8, 18, 4, 21, 10,
  13, 0, 17, 5, 12, 15, 11, 2, 9, 14, 19, 16
};

void reports_render_congress_page2(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  ColonizeFramebuffer8* fb
) {
  if (view && view->background_ok[COLONIZE_REPORT_CONGRESS]) {
    pik_blit(&view->backgrounds[COLONIZE_REPORT_CONGRESS], fb, 0, 0);
  }
  if (!col1 || !view || !view->data_dir[0]) {
    return;
  }
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    const int ff = (int)k_ff_portrait_draw_order[i];
    if (!founding_fathers_nation_has(col1, human, ff)) {
      continue;
    }
    const ColonizeSpriteSheet* sheet = reports_ff_portrait_sheet(view->data_dir, ff);
    if (!sheet || sheet->sprite_count <= 0) {
      continue;
    }
    const ColonizeSprite* sp = &sheet->sprites[0];
    /* ss.h: anchor_x is a horizontal centre, anchor_y a bottom baseline. */
    ss_blit_sprite(
      sheet, 0, fb, sp->anchor_x - sp->width / 2, sp->anchor_y - sp->height + 1
    );
  }
}

/*
 * Labor report (F4, golden: labor.png / labor_detail.png) — a 9-row x
 * 3-column table of profession icon + name + headcount (bottom-left cell
 * empty), plus a per-profession detail view reached by clicking a cell.
 *
 * DOS lays out a fixed 26-slot table, not a straight scan of job ids 0..27:
 * Expert Teachers (18, a colonist type cut from the final DOS game — see
 * europe_pool_remap) and Veteran Dragoons (23) never appear, and Free
 * Colonists (19) is pulled out of numeric order to the bottom of column 3.
 * Measured off labor.png (native 320x200): icon rows start y=26, step=18;
 * columns start x=2/107/212.
 */
#define REPORTS_LABOR_ROWS 9
#define REPORTS_LABOR_COLS 3
#define REPORTS_LABOR_ROW0_Y 26
#define REPORTS_LABOR_ROW_STEP 18
#define REPORTS_LABOR_COL0_X 2
#define REPORTS_LABOR_COL_STEP 105
#define REPORTS_LABOR_CELL_W 100
#define REPORTS_LABOR_TEXT_DX 18
/* Count-number x, relative to text_x — a fixed column offset (golden: every
 * row's number starts at the same x, measured off single- and double-digit
 * counts alike), not centered under each row's own (variable-width) name. */
#define REPORTS_LABOR_NUM_DX 22

static const int8_t k_labor_layout[REPORTS_LABOR_COLS][REPORTS_LABOR_ROWS] = {
  {0, 1, 2, 3, 4, 5, 6, 7, -1},
  {8, 9, 10, 11, 12, 13, 14, 15, 16},
  {17, 20, 21, 22, 24, 25, 26, 27, 19}
};

/* Icon selection is shared with the colony/dock renderer (units_job_icon_
 * sprite — see its table comment in units.c for how each portrait was
 * identified); this report never shows Expert Teachers/Veteran Dragoons
 * (k_labor_layout skips them, so their -1 there never gets drawn). */
/* ===================== Labor report (reports_labor_icon_for_job .. reports_render_labor_detail) ===================== */
static int reports_labor_icon_for_job(int job) {
  return units_job_icon_sprite(job);
}

/* Sums to the same total the DOS golden shows (colonies + on-map + Europe);
 * bucketed separately so the detail view's "Off Mapboard (Europe) / On
 * Mapboard / In Colonies" breakdown and the grid's per-cell total share one
 * scan of the save. */
/* profession byte -> report job id: clamp to the counts[64] table, and fold
 * UNITS_JOB_NONE (28, "no expert skill" — DOS's raw encoding for a plain,
 * unspecialized colonist) into Free Colonists (19). Without this fold every
 * unspecialized colonist's job byte (28) fell outside the report's 0..27
 * job-id table and was silently dropped from every bucket — Free Colonists
 * read 0 even in a save full of them. */
static int reports_labor_normalize_job(int job) {
  if (job == UNITS_JOB_NONE) {
    return UNITS_JOB_COLONIST;
  }
  if (job < 0) {
    return 0;
  }
  if (job >= 64) {
    return 63;
  }
  return job;
}

/* bugs.md #575: DOS's labor-report unit filter (`FUN_3f41_10d8` ->
 * `FUN_281f_0b28` -> FUN_15eb_0902) is the DS:0x30e[type] >= 0 predicate —
 * "this @UNIT type carries a profession slot" — not a type cut-off. That
 * keeps ships (13-18), Artillery, Wagon Train, Treasure and the King's
 * Regulars (6) / Cavalry (8) out, but it KEEPS Continental Cavalry (7,
 * DS:0x30e = 23) and Continental Army (9, = 21): a drafted Expert Farmer is
 * still a person and must not vanish from the report. */
static bool reports_labor_unit_is_person(const ColonizeCol1Unit* u) {
  return units_type_has_profession_slot((int)u->type);
}

static void reports_labor_job_counts(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  int* colony_counts,
  int* mapboard_counts,
  int* europe_counts,
  int* total_out
) {
  memset(colony_counts, 0, 64 * sizeof(int));
  memset(mapboard_counts, 0, 64 * sizeof(int));
  memset(europe_counts, 0, 64 * sizeof(int));
  int total = 0;

  if (col1) {
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if (c->nation_id != (uint8_t)human) {
        continue;
      }
      const int pop = c->population > COLONIZE_COL1_COLONY_POP_MAX ? COLONIZE_COL1_COLONY_POP_MAX
                                                                   : (int)c->population;
      for (int p = 0; p < pop; ++p) {
        const int job = reports_labor_normalize_job(c->profession[p]);
        colony_counts[job]++;
        total++;
      }
    }
    for (uint16_t i = 0; i < col1->head.unit_count; ++i) {
      const ColonizeCol1Unit* u = &col1->unit[i];
      if ((int)u->nation_id != human) {
        continue;
      }
      if (!reports_labor_unit_is_person(u)) {
        continue;
      }
      /* DOS FUN_3f41_10d8 (viceroy_unpacked.c 70113) files a map unit under
       * the RAW profession byte — `local_c6[*(char *)(i*0x1c + 0x315b)]++` —
       * and the detail view FUN_3f41_0d3e (69967) matches on that same raw
       * byte. There is no unit-type fallback anywhere in the chain, and there
       * could not be: `u->type` is a NAMES.TXT @UNIT id (5 = Scout) while this
       * table is indexed by @JOB (5 = Expert Lumberjacks), so the old fallback
       * filed units under an unrelated profession. A byte outside the job
       * space is skipped, the rule the Score citizen collector already uses
       * (reports_score_collect_citizen_jobs). */
      int job = (int)u->profession;
      if (job != UNITS_JOB_NONE && (job < 0 || job >= k_job_count)) {
        continue;
      }
      /* A Soldier/Pioneer/Missionary/Dragoon/Scout unit with no expert skill
       * still counts — as its base type, Free Colonists — same as a plain
       * Colonists-type unit. reports_labor_normalize_job folds UNITS_JOB_
       * NONE into that regardless of @UNIT type. */
      job = reports_labor_normalize_job(job);
      if (reports_unit_in_europe(u->x, u->y)) {
        europe_counts[job]++;
      } else if (reports_xy_is_own_colony(col1, human, u->x, u->y)) {
        /* On the colony's own tile (garrison, e.g.) but not colony
         * population — still "In Colonies" for this report, not Mapboard. */
        colony_counts[job]++;
      } else {
        mapboard_counts[job]++;
      }
      total++;
    }
  } else if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active) {
        continue;
      }
      for (int p = 0; p < c->colonist_count; ++p) {
        const ColonizeColonist* col = &c->colonists[p];
        if (!col->active) {
          continue;
        }
        int t = col->unit_type_index;
        if (t < 0) {
          t = 0;
        }
        if (t >= 64) {
          t = 63;
        }
        colony_counts[t]++;
        total++;
      }
    }
  }
  *total_out = total;
}

/* Grid-cell hit test (native 320x200 coords) for "(Click on item to zoom)".
 * Returns the job id (0..27) under (mx,my), or -1 if the click misses every
 * cell (including the empty bottom-left one). */
int reports_labor_cell_hit(int mx, int my) {
  if (my < REPORTS_LABOR_ROW0_Y) {
    return -1;
  }
  const int row = (my - REPORTS_LABOR_ROW0_Y) / REPORTS_LABOR_ROW_STEP;
  if (row < 0 || row >= REPORTS_LABOR_ROWS) {
    return -1;
  }
  if (mx < REPORTS_LABOR_COL0_X) {
    return -1;
  }
  const int col = (mx - REPORTS_LABOR_COL0_X) / REPORTS_LABOR_COL_STEP;
  if (col < 0 || col >= REPORTS_LABOR_COLS) {
    return -1;
  }
  if (mx - (REPORTS_LABOR_COL0_X + col * REPORTS_LABOR_COL_STEP) >= REPORTS_LABOR_CELL_W) {
    return -1;
  }
  return k_labor_layout[col][row];
}

void reports_render_labor_grid(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_LABOR);
  /* Golden shows job names/counts in FONTTINY (mixed case, narrow) like the
   * title and Congress page 1 — not body_font (FONTSMAL, all-caps, wide
   * enough to overlap the next column). */
  const ColonizeFont* body_font = font;
  font = (view && view->title_font_ok) ? &view->title_font : body_font;

  int colony_counts[64], mapboard_counts[64], europe_counts[64], total;
  reports_labor_job_counts(col1, human, colonies, colony_counts, mapboard_counts, europe_counts, &total);
  /* Centered, matching every other report's centered-title convention. */
  if (font) {
    char hint[48];
    reports_misc_word(56, "", hint, sizeof(hint));
    const int w = font_text_width(font, hint);
    reports_draw_line(font, fb, (fb->width - w) / 2, y, hint, 14);
  }

  for (int col = 0; col < REPORTS_LABOR_COLS; ++col) {
    for (int row = 0; row < REPORTS_LABOR_ROWS; ++row) {
      const int job = k_labor_layout[col][row];
      if (job < 0) {
        continue;
      }
      const int cx = REPORTS_LABOR_COL0_X + col * REPORTS_LABOR_COL_STEP;
      const int cy = REPORTS_LABOR_ROW0_Y + row * REPORTS_LABOR_ROW_STEP;
      const int icon = reports_labor_icon_for_job(job);
      if (icons && icon >= 0 && icon < icons->sprite_count) {
        unit_chrome_blit(
          fb, NULL, icons, icon, cx, cy, UNIT_CHROME_SPRITE_WITH_SHADOW, 0, 0, -1, 0, false,
          false, -1, -1
        );
      }
      const int count = colony_counts[job] + mapboard_counts[job] + europe_counts[job];
      char num[16];
      snprintf(num, sizeof(num), "%d", count);
      const int text_x = cx + REPORTS_LABOR_TEXT_DX;
      reports_draw_line(font, fb, text_x, cy, reports_job_name(job), 14);
      /* Count sits at a fixed x per column (golden: every row's number lines
       * up under the same point regardless of name length — not centered
       * under each name individually) and a paler/whiter 15 (not 14 — the
       * name's yellow) in this palette. */
      reports_draw_line(font, fb, text_x + REPORTS_LABOR_NUM_DX, cy + 9, num, 15);
    }
  }
}

void reports_render_labor_detail(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y,
  int job,
  char* line,
  size_t line_sz
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_LABOR);
  font = (view && view->title_font_ok) ? &view->title_font : font;

  snprintf(line, line_sz, "(%s)", reports_job_name(job));
  if (font) {
    const int w = font_text_width(font, line);
    reports_draw_line(font, fb, (fb->width - w) / 2, y, line, 14);
  }

  int colony_counts[64], mapboard_counts[64], europe_counts[64], total;
  reports_labor_job_counts(col1, human, colonies, colony_counts, mapboard_counts, europe_counts, &total);
  (void)total;
  const int europe_n = europe_counts[job];
  const int mapboard_n = mapboard_counts[job];
  const int colony_n = colony_counts[job];
  const int sum = europe_n + mapboard_n + colony_n;

  const int header_y = y + REPORTS_LABOR_ROW_STEP / 2;
  const int icon = reports_labor_icon_for_job(job);
  if (icons && icon >= 0 && icon < icons->sprite_count) {
    unit_chrome_blit(
      fb, NULL, icons, icon, 4, header_y, UNIT_CHROME_SPRITE_WITH_SHADOW, 0, 0, -1, 0, false,
      false, -1, -1
    );
  }
  snprintf(line, line_sz, "%s: %d", reports_job_name(job), sum);
  reports_draw_line(font, fb, 22, header_y, line, 14);

  const int bd_x_label = 165;
  const int bd_x_value = 295;
  /* LABELS.TXT @MISC #53/#54/#55. */
  char w[48];
  snprintf(line, line_sz, "%s:", reports_misc_word(53, "", w, sizeof(w)));
  reports_draw_line(font, fb, bd_x_label, header_y, line, 14);
  snprintf(line, line_sz, "%d", europe_n);
  reports_draw_line(font, fb, bd_x_value, header_y, line, 14);
  snprintf(line, line_sz, "%s:", reports_misc_word(54, "", w, sizeof(w)));
  reports_draw_line(font, fb, bd_x_label, header_y + REPORTS_LABOR_ROW_STEP / 2, line, 14);
  snprintf(line, line_sz, "%d", mapboard_n);
  reports_draw_line(font, fb, bd_x_value, header_y + REPORTS_LABOR_ROW_STEP / 2, line, 14);
  snprintf(line, line_sz, "%s:", reports_misc_word(55, "", w, sizeof(w)));
  reports_draw_line(font, fb, bd_x_label, header_y + REPORTS_LABOR_ROW_STEP, line, 14);
  snprintf(line, line_sz, "%d", colony_n);
  reports_draw_line(font, fb, bd_x_value, header_y + REPORTS_LABOR_ROW_STEP, line, 14);

  if (!col1) {
    return;
  }
  int shown = 0;
  const int list_y0 = header_y + 3 * REPORTS_LABOR_ROW_STEP;
  static const int kListColX[3] = {4, 124, 244};
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &col1->colony[i];
    if (c->nation_id != (uint8_t)human) {
      continue;
    }
    const int pop = c->population > COLONIZE_COL1_COLONY_POP_MAX ? COLONIZE_COL1_COLONY_POP_MAX
                                                                 : (int)c->population;
    int n = 0;
    for (int p = 0; p < pop; ++p) {
      if (reports_labor_normalize_job(c->profession[p]) == job) {
        n++;
      }
    }
    /* Garrison units standing on this colony's own tile (not colony
     * population) still count as "In Colonies" here — see
     * reports_xy_is_own_colony's comment at the grid/detail total. */
    for (uint16_t j = 0; j < col1->head.unit_count; ++j) {
      const ColonizeCol1Unit* u = &col1->unit[j];
      if ((int)u->nation_id != human || u->x != c->x || u->y != c->y) {
        continue;
      }
      if (!reports_labor_unit_is_person(u)) {
        continue;
      }
      /* Raw profession byte, no @UNIT-id fallback — see the grid collector
       * (reports_labor_job_counts) for the DOS citation. */
      const int uj = (int)u->profession;
      if (uj != UNITS_JOB_NONE && (uj < 0 || uj >= k_job_count)) {
        continue;
      }
      if (reports_labor_normalize_job(uj) == job) {
        n++;
      }
    }
    if (n <= 0) {
      continue;
    }
    const int col = shown % 3;
    const int row = shown / 3;
    const int ly = list_y0 + row * REPORTS_LABOR_ROW_STEP;
    if (ly >= 185) {
      break;
    }
    snprintf(line, line_sz, "%s: %d", c->name, n);
    reports_draw_line(font, fb, kListColX[col], ly, line, 14);
    shown++;
  }
}
