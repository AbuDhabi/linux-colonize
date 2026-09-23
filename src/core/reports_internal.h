#ifndef COLONIZE_CORE_REPORTS_INTERNAL_H
#define COLONIZE_CORE_REPORTS_INTERNAL_H

/* ===== Cross-file seams of the reports.c split (2026-09-23) =====
 * reports.c was 4403 lines; it is now split along its banners into
 * reports{,_congress,_economy,_military,_score}.c. The declarations below
 * are the only symbols used across those files; everything else stayed
 * `static` in its own file. Code moved verbatim - these are the only
 * de-static'd names, and they keep the `reports_` file-scope prefix.
 * The four REPORTS_CROSS_* geometry defines are the only `#define`s that
 * moved (from the draw-primitives banner, which is in reports.c, to here):
 * the Religious report in reports_congress.c draws that bar.
 * ============================================================== */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/fb.h"
#include "core/font.h"
#include "core/reports.h"
#include "core/units.h"

#define REPORTS_CROSS_ICON 56 /* ICONS.SS #56 (DOS sprite id 0x39, 1-based) */
#define REPORTS_CROSS_X 10 /* `3f41:0670` pushes x=10, y=25, w=0x12c */
#define REPORTS_CROSS_Y 25
#define REPORTS_CROSS_W 300


/* --- owned by reports.c --- */
const ColonizeSpriteSheet* reports_icons_for(
  const ColonizeReportsView* view, int dest
);
void reports_draw_line(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t color
);
int reports_line_step(const ColonizeFont* font);
int reports_clamp_nation(int human_nation);
bool reports_unit_in_europe(int x, int y);
bool reports_xy_is_own_colony(const ColonizeCol1Save* col1, int human, int x, int y);
int reports_colony_rebel_pct(const ColonizeCol1Colony* c);
void reports_draw_icon_bar(
  const ColonizeSpriteSheet* icons,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int icon,
  int x,
  int y,
  int w,
  int h,
  int amount,
  bool always_show_number,
  int max_icons
);
void reports_draw_dos_icon_bar(
  const ColonizeSpriteSheet* icons,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int icon,
  int x,
  int y,
  int w,
  int min_w,
  int amount,
  int denom
);
void reports_draw_icon_bar_pair(
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* fb,
  int icon0,
  int amount0,
  int icon1,
  int amount1,
  int x,
  int y,
  int w,
  int h
);
void reports_draw_hline(ColonizeFramebuffer8* fb, int x0, int x1, int y, uint8_t color);
void reports_draw_vline(ColonizeFramebuffer8* fb, int x, int y0, int y1, uint8_t color);
void reports_draw_right(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int right_x,
  int y,
  const char* text,
  uint8_t color
);
void reports_draw_line_shadowed(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, int x, int y, const char* text, uint8_t color
);
void reports_draw_right_shadowed(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, int right_x, int y, const char* text, uint8_t color
);

/* --- owned by reports_congress.c --- */
void reports_render_religious(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
);
void reports_render_congress_page1(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* body_font,
  ColonizeFramebuffer8* fb,
  char* line,
  size_t line_sz
);
void reports_render_congress_page2(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  ColonizeFramebuffer8* fb
);
void reports_render_labor_grid(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y
);
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
);

/* --- owned by reports_economy.c --- */
void reports_render_economic_trade(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y,
  char* line,
  size_t line_sz
);
void reports_render_economic_cargo(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y,
  int cargo_page,
  char* line,
  size_t line_sz
);
void reports_render_colony_garrisons(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y,
  int page,
  char* line,
  size_t line_sz
);
void reports_render_colony_sol(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y,
  int page,
  char* line,
  size_t line_sz
);

/* --- owned by reports_military.c --- */
void reports_render_naval(
  const ColonizeReportsView* view,
  int human,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int page
);
void reports_render_foreign(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
);
void reports_render_indian(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  const ColonizeUnitPool* units,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
);

/* --- owned by reports_score.c --- */
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
);

#endif /* COLONIZE_CORE_REPORTS_INTERNAL_H */
