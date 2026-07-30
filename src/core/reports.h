#ifndef COLONIZE_REPORTS_H
#define COLONIZE_REPORTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/colony.h"
#include "core/europe.h"
#include "core/font.h"
#include "core/map.h"
#include "core/pik.h"
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
} ColonizeReportsView;

void reports_init(ColonizeReportsView* view);
void reports_free(ColonizeReportsView* view);
bool reports_load(ColonizeReportsView* view, const char* data_dir, char* err, size_t err_size);

const char* reports_title(ColonizeReportId id);
const char* reports_background_name(ColonizeReportId id);

/* Map F2–F10 → report id; returns false for F1 / non-report keys. */
bool reports_id_from_fkey(int fkey_number /*1..10*/, ColonizeReportId* out_id);

void reports_render(
  const ColonizeReportsView* view,
  ColonizeReportId id,
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const EuropeScreen* europe,
  int cursor_x,
  int cursor_y,
  uint32_t turn_number,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

#endif
