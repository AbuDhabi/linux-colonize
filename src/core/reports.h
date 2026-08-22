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
  bool loaded;
  ColonizeReportId active;
  char data_dir[512];
} ColonizeReportsView;

void reports_init(ColonizeReportsView* view);
void reports_free(ColonizeReportsView* view);
bool reports_load(ColonizeReportsView* view, const char* data_dir, char* err, size_t err_size);

const char* reports_title(ColonizeReportId id);
const char* reports_background_name(ColonizeReportId id);

/* Map F2–F10 → report id; returns false for F1 / non-report keys. */
bool reports_id_from_fkey(int fkey_number /*1..10*/, ColonizeReportId* out_id);

/*
 * Live Colonization Score (manual / FAQ rules) for F10.
 * Independence bonuses apply only once declare/achieve are tracked in save;
 * until then those fields stay 0 and the base total is still shown.
 */
typedef struct ColonizeScoreBreakdown {
  int year;
  int difficulty; /* 0 Discoverer .. 4 Viceroy */
  int citizens; /* population score */
  int congress; /* +5 per founding father */
  int treasury; /* gold / 1000 */
  int rebel_sentiment; /* 0..100 */
  int villages_burned;
  int villages_penalty; /* negative: -(difficulty+1) * burned */
  int intervention_bells; /* +1 each after foreign intervention (stub) */
  int base_total;
  bool independence_declared;
  bool independence_achieved;
  int declare_year; /* 0 if unknown / not declared */
  int prior_nations; /* European powers that achieved independence first */
  int early_revolution_pct; /* max(0, 1780 - declare_year) when declared before 1780 */
  int foreign_recognition_pct; /* 100 / 50 / 25 / 0 from prior_nations when achieved */
  int total;
} ColonizeScoreBreakdown;

void reports_compute_score(
  ColonizeScoreBreakdown* out,
  const ColonizeCol1Save* col1,
  int human_nation,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe
);

void reports_render(
  const ColonizeReportsView* view,
  ColonizeReportId id,
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
 * Title-menu Hall of Fame screen: ranked retired-game scores, full-screen
 * wood (shares COLONIZE_REPORT_SCORE's WOODPANL.PIK — DOS's HALLFAME.DAT
 * writer also opens a WOODPANL screen; see viceroy_unpacked.asm string table
 * around "HALLFAME.DAT" / "INDEPENDENT" / "NAMES"). LABELS.TXT #207
 * "COLONIZATION HALL OF FAME" is the real DOS title string.
 */
#define COLONIZE_HOF_ROW_MAX 10

typedef struct ColonizeHofRow {
  char leader[32];
  char nation[24];
  int score;
  int year;
} ColonizeHofRow;

void reports_render_hall_of_fame(
  const ColonizeReportsView* view,
  const ColonizeHofRow* entries,
  int entry_count,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

#endif
