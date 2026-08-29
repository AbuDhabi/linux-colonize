#ifndef COLONIZE_REPORTS_H
#define COLONIZE_REPORTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/font.h"
#include "core/map.h"
#include "core/pik.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * DOS report screens (MENU.TXT @REPORTS / F2–F10).
 * F1 Terrain Information opens Colonizopedia for the cursor tile (not a report plate).
 *
 * Background mapping:
 *   F2  Religious Adviser       → REPORT2.PIK
 *   F3  Continental Congress    → CCBKGD.PIK
 *   F4  Labor Adviser           → REPORT4.PIK
 *   F5  Economic Adviser        → REPORT5.PIK
 *   F6  Colony Adviser          → REPORT6.PIK
 *   F7  Naval Adviser           → REPORT7.PIK
 *   F8  Foreign Affairs Advisor → REPORT8.PIK
 *   F9  Indian Adviser          → REPORT9.PIK
 *   F10 Colonization Score      → WOODPANL.PIK (full-screen wood)
 *
 * Content prefers ColonizeCol1Save when non-NULL; otherwise falls back to
 * runtime colony / unit / Europe pools.
 */
typedef enum ColonizeReportId {
  COLONIZE_REPORT_RELIGIOUS = 0,
  COLONIZE_REPORT_CONGRESS,
  COLONIZE_REPORT_LABOR,
  COLONIZE_REPORT_ECONOMIC,
  COLONIZE_REPORT_COLONY,
  COLONIZE_REPORT_NAVAL,
  COLONIZE_REPORT_FOREIGN,
  COLONIZE_REPORT_INDIAN,
  COLONIZE_REPORT_SCORE,
  COLONIZE_REPORT_COUNT
} ColonizeReportId;

typedef struct ColonizeReportsView {
  ColonizePikImage backgrounds[COLONIZE_REPORT_COUNT];
  bool background_ok[COLONIZE_REPORT_COUNT];
  ColonizeSpriteSheet icons; /* ICONS.SS, remapped to REPORT2.PIK's palette (cross counter). */
  bool icons_ok;
  ColonizeFont title_font; /* FONTTINY.FF — report titles (golden: religious.png / labor.png). */
  bool title_font_ok;
  /* Congress is two pages: p1 = REPORT3.PIK (this nation's own desk/study —
   * F3's natural REPORT-N slot, unused until now); p2 reuses backgrounds[
   * COLONIZE_REPORT_CONGRESS] (CCBKGD.PIK, the hall photo). */
  ColonizePikImage congress_page1_bg;
  bool congress_page1_bg_ok;
  ColonizePikImage exploits_bg; /* WOODPAN2.PIK — Retire exploits screen (FUN_41f2_0b70) */
  bool exploits_bg_ok;
  bool loaded;
  ColonizeReportId active;
  char data_dir[512];
} ColonizeReportsView;

void reports_init(ColonizeReportsView* view);
void reports_free(ColonizeReportsView* view);
bool reports_load(ColonizeReportsView* view, const char* data_dir, char* err, size_t err_size);

const char* reports_title(ColonizeReportId id);
const char* reports_background_name(ColonizeReportId id);

/*
 * Founding Father / job-expert / cargo / tribe / nation-adjective / tribe-
 * tech-level display name by index. Live from NAMES.TXT (@FATHERS / @JOB /
 * @CARGO / @TRIBES / @NATIONALITY / @LEVELS) after a successful
 * reports_load; falls back to a hand-typed static table otherwise (no
 * assets loaded, e.g. tests).
 */
const char* reports_ff_display_name(int idx);
const char* reports_job_display_name(int job);
const char* reports_cargo_display_name(int cargo);
const char* reports_tribe_display_name(int t);
const char* reports_nation_adjective_display_name(int nation);
const char* reports_tribe_level_display_name(uint8_t tech);
/* LABELS.TXT @MISC line `index` (0-based, non-blank lines) or `fallback`. Static buffer. */
const char* reports_misc_display_word(int index, const char* fallback);

/* Map F2–F10 → report id; returns false for F1 / non-report keys. */
bool reports_id_from_fkey(int fkey_number /*1..10*/, ColonizeReportId* out_id);

/*
 * Bottom-right "OK" button every F2–F9 report shares (native 320×200 coords).
 * F10 Colonization Score has no OK button (Retire/exit is separate); neither
 * does Congress page 2 (golden: continental_p2.png — full-bleed photo, no chrome).
 */
bool reports_ok_button_hit(ColonizeReportId id, bool congress_page2, int mx, int my);

/*
 * Labor report (F4) grid-cell hit test (native 320x200 coords). Returns the
 * job id (0..27, see reports.c's k_job_names) under (mx,my), or -1 if the
 * click misses every cell — including the always-empty bottom-left one.
 */
int reports_labor_cell_hit(int mx, int my);

/*
 * Economic report (F5) page count: 1 (European Trade) + however many
 * "Cargo in Port" pages it takes to list every one of this nation's
 * colonies, 17 rows per page (golden: economic_p2.png), minimum 1 even
 * with zero colonies — so the report always has at least 2 pages.
 */
int reports_economic_page_count(const ColonizeCol1Save* col1, int human_nation);

/*
 * Colony report (F6) page count: 2 * however many 9-colony pages it takes
 * to list every one of this nation's colonies — pages [0..k) are "Military
 * Garrisons" (golden: colony_p1.png), pages [k..2k) are "Sons of Liberty"
 * (golden: colony_p2.png), minimum 1 colony-page each even with zero
 * colonies (so the report always has at least 2 pages).
 */
int reports_colony_page_count(const ColonizeCol1Save* col1, int human_nation);

/*
 * Naval report (F7) page count: ceil(row_count / 7), minimum 1. row_count
 * counts one row per on-mapboard/Europe-harbor ship plus one extra row per
 * passenger it carries (golden: naval.png — a passenger sits on its own row
 * above its ship's row). Mirrors reports_render's own row builder, so this
 * always agrees with what actually paginates.
 */
int reports_naval_page_count(
  int human_nation,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe
);

/*
 * Colonization Score — byte-faithful port of DOS FUN_41f2_0092 (score
 * composer, F10 + Retire) and FUN_41f2_0b70 (Colonization Rating / exploits
 * tier). Every component and its gate mirrors the DOS block order:
 *
 *   citizens        colony population + qualifying map/Europe units
 *   congress        +5 per Founding Father (FUN_281f_07b4 over 25 slots)
 *   treasury        gold / 1000, only when gold >= 1000
 *   villages_*      villages_burned * (-1 - difficulty)
 *   rebel_sentiment DS:0x53d0 (rebel_sentiment_report), only when != 0
 *   early_revolution (1780 - declare_year) * 2 — gated on the independence
 *                   ACHIEVED bit (0x5382|0x08) and declare_year < 1780;
 *                   declare_year is the DS:0x53a7/0x53a8 byte pair FUN_43f7_1a26
 *                   latches at declaration (year/100, year%100)
 *   bells           min(100, liberty_bells_total / 100) — gated on REF present
 *                   (0x5382|0x02) and bells >= 100 (bells_total is zeroed at
 *                   declaration, so this is "bells since declaring")
 *   recognition     100 >> prior_nations, only once achieved; prior_nations =
 *                   other Euro powers whose nation_flags bit 0x04 is set
 *   total           sum, then *(8 + (8 >> prior_nations)) / 8 when recognition
 *                   != 0 (x2 / x1.5 / x1.25 / x1.125 / x1 for 0..4 prior)
 *   rating          FUN_41f2_0b70: ((mult * total) / 100) >> 1 with
 *                   mult = {4,5,6,8,10}[difficulty]; exploits_tier = largest
 *                   n-1 (n in 1..24) with n*n/3 < (mult*total)/100, capped 23,
 *                   -1 when none (no exploits screen)
 *   scoring_complete 0x5382|0x10 — DOS F10 shows only "SCORING COMPLETE"
 */
typedef struct ColonizeScoreBreakdown {
  int year;
  int difficulty; /* 0 Discoverer .. 4 Viceroy */
  int citizens; /* population score */
  int congress; /* +5 per founding father */
  int treasury; /* gold / 1000 (0 below 1000 gold) */
  int rebel_sentiment; /* DS:0x53d0 rebel_sentiment_report, 0..100 */
  int villages_burned;
  int villages_penalty; /* negative: -(difficulty+1) * burned */
  int early_revolution_pts; /* (1780 - declare_year) * 2 once achieved */
  int bells_pts; /* min(100, bells/100) once REF present */
  int base_total;
  bool independence_declared;
  bool independence_achieved;
  bool scoring_complete;
  int declare_year; /* 0 unless declared */
  int prior_nations; /* other Euro powers already independent (nation_flags & 4) */
  int foreign_recognition_pct; /* 100 >> prior_nations once achieved, else 0 */
  int total;
  int rating; /* FUN_41f2_0b70 Colonization Rating (percent) */
  int exploits_tier; /* -1 none, else 0..23 = @SCORE lines shown - 1 */
} ColonizeScoreBreakdown;

void reports_compute_score(
  ColonizeScoreBreakdown* out,
  const ColonizeCol1Save* col1,
  int human_nation,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe
);

/* DOS FUN_41f2_0092 tail: total after the foreign-recognition multiplier. */
int reports_score_apply_recognition(int base_total, int prior_nations, bool achieved);

/* DOS FUN_41f2_0b70: Colonization Rating percent + exploits tier for a
 * total score at a difficulty. *tier_out gets -1 when no exploits line
 * qualifies. Returns the rating. */
int reports_score_rating(int total, int difficulty, int* tier_out);

/* Declaration year latched by FUN_43f7_1a26 into DS:0x53a7/0x53a8
 * (king_audience_streak / king_audience_last_pick reuse); 0 unless WoI. */
int reports_score_declare_year(const ColonizeCol1Save* col1);

/* congress_page2: true shows Continental Congress page 2 (golden:
 * continental_p2.png); ignored for every id but COLONIZE_REPORT_CONGRESS.
 * labor_detail_job: >=0 shows the Labor report (F4) detail view for that
 * job id (golden: labor_detail.png) instead of the profession grid; ignored
 * for every id but COLONIZE_REPORT_LABOR.
 * economic_page: 0 shows European Trade (golden: economic_p1.png); N>=1
 * shows Cargo in Port page N (golden: economic_p2.png), colonies
 * [(N-1)*17 .. N*17). Ignored for every id but COLONIZE_REPORT_ECONOMIC —
 * see reports_economic_page_count for how many pages exist.
 * colony_page: page index into the Colony report (F6) — [0..k) shows
 * Military Garrisons, [k..2k) shows Sons of Liberty (k = reports_colony_
 * page_count(...)/2), 9 colonies per page each. Ignored for every id but
 * COLONIZE_REPORT_COLONY — see reports_colony_page_count for how many
 * pages exist.
 * naval_page: page index into the Naval report (F7), 7 rows per page.
 * Ignored for every id but COLONIZE_REPORT_NAVAL — see
 * reports_naval_page_count for how many pages exist. */
void reports_render(
  const ColonizeReportsView* view,
  ColonizeReportId id,
  bool congress_page2,
  int labor_detail_job,
  int economic_page,
  int colony_page,
  int naval_page,
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const EuropeScreen* europe,
  const ColonizeCol1Save* col1,
  int human_nation,
  int cursor_x,
  int cursor_y,
  uint32_t turn_number,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

/*
 * Hall of Fame screen (DOS FUN_41f2_0f56 presenter): WOODPANL.PIK, title
 * LABELS @MISC #192 centered, then up to 5 entries of three centered lines:
 *   "<n>. <Difficulty> <Leader> of the [Free ]<Nation>"
 *   "<President, <@INDEPENDENT name> | General, Continental Army |
 *     Leader, <Nation> Colonies> to A.D. <year>. Score: <score>"
 *   "--- <@MISC #199> <rating>% ---"
 * DOS keeps 6 slots in HALLFAME.DAT and shows 5; ranks by Colonization
 * Rating (word 19 of the 42-byte record), not raw score.
 */
#define COLONIZE_HOF_ROW_MAX 10
#define COLONIZE_HOF_SHOWN_MAX 5

typedef struct ColonizeHofRow {
  char leader[32];
  char nation[24]; /* nation adjective ("Dutch") */
  char independent_name[48]; /* NAMES.TXT @INDEPENDENT row, "" if unknown */
  int score;
  int year;
  int difficulty;
  int rating;
  bool declared;
  bool achieved;
} ColonizeHofRow;

void reports_render_hall_of_fame(
  const ColonizeReportsView* view,
  const ColonizeHofRow* entries,
  int entry_count,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

/*
 * Retire "exploits" screen (DOS FUN_41f2_0b70 dialog): WOODPAN2.PIK, GAME.TXT
 * @EXPLOITS header (%NUMBER0 = rating, %STRING0 = nation name), then the
 * first exploits_tier+1 @SCORE category fields stacked upward from y=195,
 * the last line's name field (%STRING0 = leader's last name) at y=142, and
 * SCORE<tier+1>.SS frame 0 at x=100. Caller resolves the text.
 */
typedef struct ColonizeExploitsView {
  char header[3][96]; /* @EXPLOITS lines, tokens applied */
  int header_count;
  char categories[24][160]; /* @SCORE line[i] field 0 */
  int category_count; /* exploits_tier + 1 */
  char named[96]; /* last @SCORE line field 1 with %STRING0 applied */
  const ColonizeSpriteSheet* sheet; /* SCORE<tier+1>.SS or NULL */
} ColonizeExploitsView;

void reports_render_exploits(
  const ColonizeReportsView* view,
  const ColonizeExploitsView* ex,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

/* Nearest-color remap of a freshly loaded SCORE<nn>.SS onto WOODPAN2.PIK's
 * palette (same treatment ICONS.SS gets for REPORT2.PIK). */
void reports_remap_exploits_sheet(const ColonizeReportsView* view, ColonizeSpriteSheet* sheet);

#endif
