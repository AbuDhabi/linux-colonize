/*
 * Sections:
 *  - Economic/Trade & Cargo report (~line 56)
 *  - Colony report (~line 303)
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

/*
 * Economic report (F5, golden: economic_p1.png / economic_p2.png) — page 1
 * is a 16-row "European Trade" ledger (row = cargo, columns Tons/Gold/Bid
 * Price/Ask Price); page 2+ is a "Cargo in Port" grid (row = colony, column
 * = cargo icon), 17 colonies per page, another page added past 17.
 *
 * Both tables share the same dark-red rule-line style; geometry (native
 * 320x200) measured directly off the goldens.
 */
#define REPORTS_ECON_LINE_COLOR 119 /* dark red (134,0,0) row/column rules */
#define REPORTS_ECON_LABEL_COLOR 14 /* yellow row/column labels */
#define REPORTS_ECON_VALUE_COLOR 97 /* pale cream (247,243,199) Bid/Ask + stock values */
#define REPORTS_ECON_POS_COLOR 10 /* green (85,255,85): net sold (tons/gold >= 0) */
#define REPORTS_ECON_NEG_COLOR 112 /* red (243,0,0): net bought (tons/gold < 0) */

#define REPORTS_ECON1_ROWS 16
#define REPORTS_ECON1_ROW0_Y 33 /* first horizontal rule (also last row's bottom) */
#define REPORTS_ECON1_ROW_STEP 8
#define REPORTS_ECON1_LABEL_X 2
#define REPORTS_ECON1_DIVIDER_X 67
#define REPORTS_ECON1_VLINE_TOP_Y 25 /* the divider (and page 2's column rules)
   start a row above the rule lines, level with the column headers — golden-measured, not a typo of ROW0_Y */
#define REPORTS_ECON1_HEADER_Y 24
#define REPORTS_ECON1_TONS_RIGHT 90
#define REPORTS_ECON1_GOLD_RIGHT 144
#define REPORTS_ECON1_BID_RIGHT 199
#define REPORTS_ECON1_ASK_RIGHT 251

/* ===================== Economic/Trade & Cargo report (reports_render_economic_trade .. reports_render_economic_cargo) ===================== */
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
) {
  font = (view && view->title_font_ok) ? &view->title_font : font;
  if (font) {
    /* LABELS.TXT @MISC index 206 (2026-08-27 fix, same helper as the
     * column headers below). */
    const char* live = reports_labels_field("MISC", 206);
    const char* kSubtitle = live ? live : "";
    const int w = font_text_width(font, kSubtitle);
    reports_draw_line(font, fb, (fb->width - w) / 2, y - 1, kSubtitle, REPORTS_ECON_LABEL_COLOR);
  }

  /* Column headers live from LABELS.TXT @MISC (2026-08-27 fix, same
   * reports_labels_field helper as the report titles / Hall of Fame
   * header): Tons #58, Gold #59, Bid Price #203, Ask Price #204. */
  {
    const char* tons_w = reports_labels_field("MISC", 58);
    const char* gold_w = reports_labels_field("MISC", 59);
    const char* bid_w = reports_labels_field("MISC", 203);
    const char* ask_w = reports_labels_field("MISC", 204);
    reports_draw_right(
      font, fb, REPORTS_ECON1_TONS_RIGHT, REPORTS_ECON1_HEADER_Y, tons_w ? tons_w : "",
      REPORTS_ECON_LABEL_COLOR
    );
    reports_draw_right(
      font, fb, REPORTS_ECON1_GOLD_RIGHT, REPORTS_ECON1_HEADER_Y, gold_w ? gold_w : "",
      REPORTS_ECON_LABEL_COLOR
    );
    reports_draw_right(
      font, fb, REPORTS_ECON1_BID_RIGHT, REPORTS_ECON1_HEADER_Y, bid_w ? bid_w : "",
      REPORTS_ECON_LABEL_COLOR
    );
    reports_draw_right(
      font, fb, REPORTS_ECON1_ASK_RIGHT, REPORTS_ECON1_HEADER_Y, ask_w ? ask_w : "",
      REPORTS_ECON_LABEL_COLOR
    );
  }

  const int table_bottom = REPORTS_ECON1_ROW0_Y + REPORTS_ECON1_ROWS * REPORTS_ECON1_ROW_STEP;
  for (int i = 0; i <= REPORTS_ECON1_ROWS; ++i) {
    reports_draw_hline(fb, 0, fb->width, REPORTS_ECON1_ROW0_Y + i * REPORTS_ECON1_ROW_STEP, REPORTS_ECON_LINE_COLOR);
  }
  reports_draw_vline(fb, REPORTS_ECON1_DIVIDER_X, REPORTS_ECON1_VLINE_TOP_Y, table_bottom, REPORTS_ECON_LINE_COLOR);

  const ColonizeCol1Nation* nat = col1 ? &col1->nation[human] : NULL;
  for (int c = 0; c < (int)COLONIZE_COL1_CARGO_TYPES; ++c) {
    const int row_top = REPORTS_ECON1_ROW0_Y + c * REPORTS_ECON1_ROW_STEP;
    const int text_y = row_top + 2;
    reports_draw_line(font, fb, REPORTS_ECON1_LABEL_X, text_y, reports_cargo_name(c), REPORTS_ECON_LABEL_COLOR);

    const int32_t tons = nat ? nat->trade.tons[c] : 0;
    const int32_t g = nat ? nat->trade.gold[c] : 0;
    const bool net_bought = tons < 0 || (tons == 0 && g < 0);
    const uint8_t sign_color = net_bought ? REPORTS_ECON_NEG_COLOR : REPORTS_ECON_POS_COLOR;
    snprintf(line, line_sz, "%d", tons < 0 ? -tons : tons);
    reports_draw_right(font, fb, REPORTS_ECON1_TONS_RIGHT, text_y, line, sign_color);
    snprintf(line, line_sz, "%d$", g < 0 ? -g : g);
    reports_draw_right(font, fb, REPORTS_ECON1_GOLD_RIGHT, text_y, line, sign_color);

    int bid;
    int ask;
    if (europe && c < europe->cargo_count) {
      bid = europe_sell_price(europe, c);
      ask = europe_buy_price(europe, c);
    } else if (nat) {
      /* Same DOS pair as europe_sell_price/europe_buy_price: bid =
       * max(euro_price − 1, 0), ask = max(euro_price + burden, 0). The ask
       * was missing its @CARGO burden term (smell audit #63) — that is the
       * whole Food 0/8 and Lumber 1/6 spread on the real Europe screen. */
      bid = nat->trade.euro_price[c] > 0 ? nat->trade.euro_price[c] - 1 : 0;
      ask = (int)nat->trade.euro_price[c] + europe_cargo_burden(c);
      if (ask < 0) {
        ask = 0;
      }
    } else {
      bid = 0;
      ask = 0;
    }
    snprintf(line, line_sz, "%d$", bid);
    reports_draw_right(font, fb, REPORTS_ECON1_BID_RIGHT, text_y, line, REPORTS_ECON_VALUE_COLOR);
    snprintf(line, line_sz, "%d$", ask);
    reports_draw_right(font, fb, REPORTS_ECON1_ASK_RIGHT, text_y, line, REPORTS_ECON_VALUE_COLOR);
  }
}

#define REPORTS_ECON2_ROWS_PER_PAGE 17
#define REPORTS_ECON2_ROW0_Y 42
#define REPORTS_ECON2_ROW_STEP 8
#define REPORTS_ECON2_ICON_Y (REPORTS_ECON2_ROW0_Y - 11)
#define REPORTS_ECON2_LABEL_X 2
#define REPORTS_ECON2_DIVIDER_X 87
#define REPORTS_ECON2_COL_STEP 14
#define REPORTS_ECON2_COLS (int)COLONIZE_COL1_CARGO_TYPES

int reports_economic_page_count(const ColonizeCol1Save* col1, int human) {
  int colony_count = 0;
  if (col1) {
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      if (col1->colony[i].nation_id == (uint8_t)human) {
        colony_count++;
      }
    }
  }
  int cargo_pages = (colony_count + REPORTS_ECON2_ROWS_PER_PAGE - 1) / REPORTS_ECON2_ROWS_PER_PAGE;
  if (cargo_pages < 1) {
    cargo_pages = 1;
  }
  return 1 + cargo_pages;
}

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
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_ECONOMIC);
  font = (view && view->title_font_ok) ? &view->title_font : font;
  if (font) {
    /* LABELS.TXT @MISC index 207 (2026-08-27 fix). */
    const char* live = reports_labels_field("MISC", 207);
    const char* kSubtitle = live ? live : "";
    const int w = font_text_width(font, kSubtitle);
    reports_draw_line(font, fb, (fb->width - w) / 2, y - 1, kSubtitle, REPORTS_ECON_LABEL_COLOR);
  }

  /* Column rules run the full height of the icon header row too, not just
   * the data grid below it — same top (REPORTS_ECON1_VLINE_TOP_Y) as page
   * 1's row-label/Tons divider. Row rules stay confined to the data grid
   * (no line above the icons; they float). */
  const int table_bottom = REPORTS_ECON2_ROW0_Y + REPORTS_ECON2_ROWS_PER_PAGE * REPORTS_ECON2_ROW_STEP;
  for (int i = 0; i <= REPORTS_ECON2_ROWS_PER_PAGE; ++i) {
    reports_draw_hline(
      fb, REPORTS_ECON2_DIVIDER_X, fb->width, REPORTS_ECON2_ROW0_Y + i * REPORTS_ECON2_ROW_STEP, REPORTS_ECON_LINE_COLOR
    );
  }
  for (int c = 0; c <= REPORTS_ECON2_COLS; ++c) {
    reports_draw_vline(
      fb,
      REPORTS_ECON2_DIVIDER_X + c * REPORTS_ECON2_COL_STEP,
      REPORTS_ECON1_VLINE_TOP_Y,
      table_bottom,
      REPORTS_ECON_LINE_COLOR
    );
  }

  for (int c = 0; c < REPORTS_ECON2_COLS; ++c) {
    const int icon = 22 + c;
    if (icons && icon < icons->sprite_count) {
      const ColonizeSprite* sp = &icons->sprites[icon];
      const int col_left = REPORTS_ECON2_DIVIDER_X + c * REPORTS_ECON2_COL_STEP;
      const int icon_x = col_left + (REPORTS_ECON2_COL_STEP - sp->width) / 2;
      ss_blit_sprite(icons, icon, fb, icon_x, REPORTS_ECON2_ICON_Y);
    }
  }

  if (!col1) {
    return;
  }
  int shown = 0;
  const int skip = cargo_page * REPORTS_ECON2_ROWS_PER_PAGE;
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &col1->colony[i];
    if (c->nation_id != (uint8_t)human) {
      continue;
    }
    if (shown < skip) {
      shown++;
      continue;
    }
    const int row = shown - skip;
    if (row >= REPORTS_ECON2_ROWS_PER_PAGE) {
      break;
    }
    const int row_top = REPORTS_ECON2_ROW0_Y + row * REPORTS_ECON2_ROW_STEP;
    const int text_y = row_top + 2;
    reports_draw_line(font, fb, REPORTS_ECON2_LABEL_X, text_y, c->name, REPORTS_ECON_LABEL_COLOR);
    for (int cg = 0; cg < REPORTS_ECON2_COLS; ++cg) {
      const int col_left = REPORTS_ECON2_DIVIDER_X + cg * REPORTS_ECON2_COL_STEP;
      const unsigned stock = c->stock[cg];
      /* Golden: 0 is black (de-emphasized), 1-99 is the usual pale cream,
       * >=100 (near/at the 100-per-good warehouse cap) switches to the
       * bright yellow used for labels — a "getting full" warning. */
      const uint8_t color = stock == 0 ? 0 : (stock >= 100 ? REPORTS_ECON_LABEL_COLOR : REPORTS_ECON_VALUE_COLOR);
      snprintf(line, line_sz, "%u", stock);
      const int w = font ? font_text_width(font, line) : 0;
      reports_draw_line(font, fb, col_left + (REPORTS_ECON2_COL_STEP - w) / 2, text_y, line, color);
    }
    shown++;
  }
}

/*
 * Colony Adviser (F6): two paginated pages, each listing this nation's
 * colonies (golden: colony_p1.png "Military Garrisons", colony_p2.png
 * "Sons of Liberty"). Both share the same left-hand icon+digit+name sidebar
 * (colony_p1.png / colony_p2.png measured identical x/y for that column).
 */
#define REPORTS_COLONY_ROWS_PER_PAGE 9
#define REPORTS_COLONY_ROW0_Y 27
#define REPORTS_COLONY_ROW_STEP 17
#define REPORTS_COLONY_ICON_X 0
#define REPORTS_COLONY_ICON_W 21 /* ICONS.SS #0-3: 21x16 fortification markers */
#define REPORTS_COLONY_POP_DX 11 /* population digit: fixed x, not centered — see call site */
#define REPORTS_COLONY_NAME_X 19
/* Yellow name/value label — REPORT6.PIK's own remap of the usual report
 * yellow (index differs per background palette; see reports_render_colony
 * palette probe). */
#define REPORTS_COLONY_LABEL_COLOR 146
#define REPORTS_COLONY_DIGIT_WHITE 15
#define REPORTS_COLONY_DIGIT_GREEN 10
#define REPORTS_COLONY_DIGIT_BLUE 11

#define REPORTS_COLONY_UNIT_X 110
#define REPORTS_COLONY_UNIT_PITCH 18
/* DOS row-sizing constants (FUN_3f41_1ed8): the icon pitch is
 * clamp(0xd2 / stack_count, 1, 0x12) and icons keep being emitted while
 * x <= 0x12c, so a large garrison packs tighter rather than being cut off. */
#define REPORTS_COLONY_UNIT_SPAN 210
#define REPORTS_COLONY_UNIT_X_MAX 300

#define REPORTS_COLONY_FLAG_X 111
#define REPORTS_COLONY_FLAG_ICON 123 /* ICONS.SS — same SoL flag as colony_screen.c */
#define REPORTS_COLONY_PCT_X 129
#define REPORTS_COLONY_BUILDING_X 153
#define REPORTS_COLONY_BELL_X 206
#define REPORTS_COLONY_BELL_ICON 62 /* ICONS.SS — same bell as the Congress bar */
#define REPORTS_COLONY_BELL_NUM_X 225
#define REPORTS_COLONY_WORKER_X 249
#define REPORTS_COLONY_WORKER_PITCH 21
#define REPORTS_COLONY_WORKER_MAX 6

/* ===================== Colony report (reports_colony_page_count .. reports_render_colony_sol) ===================== */
int reports_colony_page_count(const ColonizeCol1Save* col1, int human) {
  int n = 0;
  if (col1) {
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      if (col1->colony[i].nation_id == (uint8_t)human) {
        n++;
      }
    }
  }
  int pages = (n + REPORTS_COLONY_ROWS_PER_PAGE - 1) / REPORTS_COLONY_ROWS_PER_PAGE;
  if (pages < 1) {
    pages = 1;
  }
  return 2 * pages;
}

/* @BUILDING fortification tier bitfield (col1_bridge.c's k_fort convention:
 * popcount of the raw bits = how many of {Stockade,Fort,Fortress} are set,
 * lowest-to-highest) -> ICONS.SS settlement marker (colony.c's
 * COLONY_MAP_ICON_* — 0 stockade, 1 fort, 2 fortress, 3 none). */
static int reports_colony_fort_icon(unsigned bits) {
  int tier = 0;
  while (bits) {
    tier += (int)(bits & 1u);
    bits >>= 1;
  }
  if (tier >= 3) {
    return 2; /* Fortress */
  }
  if (tier >= 2) {
    return 1; /* Fort */
  }
  if (tier >= 1) {
    return 0; /* Stockade */
  }
  return 3; /* None */
}

/* Map population-digit color (docs/sons_of_liberty.md: "white <50 / green
 * >=50 / blue 100") superimposed on the colony's fortification icon. */
static uint8_t reports_colony_pop_color(int sol_pct) {
  if (sol_pct >= 100) {
    return REPORTS_COLONY_DIGIT_BLUE;
  }
  if (sol_pct >= 50) {
    return REPORTS_COLONY_DIGIT_GREEN;
  }
  return REPORTS_COLONY_DIGIT_WHITE;
}

/*
 * Pool colony matching a col1 colony record. Keyed on (x,y), the same key
 * col1_bridge_export uses to carry Col1-only fields across a save/load
 * ("preserve Col1-only fields by xy", col1_bridge.c) — a map tile holds at
 * most one settlement, so it is unique, and unlike the name it survives a
 * capture. Index-pairing (what this used to do) only holds on a freshly
 * imported save: colonies_abandon zeroes a pool slot in place and shrinks
 * colony_count, so after any abandon/raze the arrays slide out of step and
 * SoL/bells get attributed to the wrong colony.
 */
static const ColonizeColony* reports_pool_colony_for(
  const ColonizeColonyPool* colonies, const ColonizeCol1Colony* c
) {
  return c ? colonies_find_at_xy(colonies, (int)c->x, (int)c->y) : NULL;
}

/*
 * One page of the Colony report's rows (audit SC-15). Both Colony pages —
 * Military Garrisons and Sons of Liberty — walked col1->colony[] with the
 * identical own-nation filter, `skip = page * ROWS_PER_PAGE` counter,
 * row_top arithmetic and reports_pool_colony_for pairing. Fills `out` with
 * up to REPORTS_COLONY_ROWS_PER_PAGE rows and returns how many.
 */
typedef struct ReportsColonyRow {
  const ColonizeCol1Colony* c;
  const ColonizeColony* colony; /* pool colony paired by tile; may be NULL */
  int row_top;
} ReportsColonyRow;

static int reports_colony_page_rows(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  int page,
  ReportsColonyRow* out
) {
  if (!col1 || !out) {
    return 0;
  }
  int shown = 0;
  int n = 0;
  const int skip = page * REPORTS_COLONY_ROWS_PER_PAGE;
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &col1->colony[i];
    if (c->nation_id != (uint8_t)human) {
      continue;
    }
    if (shown < skip) {
      shown++;
      continue;
    }
    const int row = shown - skip;
    if (row >= REPORTS_COLONY_ROWS_PER_PAGE) {
      break;
    }
    out[n].c = c;
    out[n].colony = reports_pool_colony_for(colonies, c);
    out[n].row_top = REPORTS_COLONY_ROW0_Y + row * REPORTS_COLONY_ROW_STEP;
    n++;
    shown++;
  }
  return n;
}

/* Shared icon+digit+name sidebar cell, identical on both Colony pages.
 * `colony` is the pool colony matching `c` (paired by tile —
 * reports_pool_colony_for), or NULL when
 * unavailable; passing it gets the Bolivar +20% SoL bonus folded in via
 * colony_prod_sol_percent (colony_prod_sol_percent is authoritative —
 * colony_screen.c's own SoL display uses it), matching golden exactly
 * (reports_colony_rebel_pct alone under-reports by Bolivar's +20). */
static void reports_render_colony_sidebar(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  const ColonizeCol1Colony* c,
  const ColonizeColony* colony,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int row_top,
  char* line,
  size_t line_sz
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_COLONY);
  const int icon = reports_colony_fort_icon(c->buildings.fortification);
  if (icons && icon >= 0 && icon < icons->sprite_count) {
    const ColonizePalette* active_palette =
      (view->background_ok[COLONIZE_REPORT_COLONY] && view->backgrounds[COLONIZE_REPORT_COLONY].has_palette)
        ? &view->backgrounds[COLONIZE_REPORT_COLONY].palette
        : NULL;
    colonies_blit_settlement_icon(
      icons, icon, fb, REPORTS_COLONY_ICON_X, row_top - 3, c->nation_id, active_palette
    );
  }
  const int sol_pct = colony ? colony_prod_sol_percent(col1, colony) : reports_colony_rebel_pct(c);
  snprintf(line, line_sz, "%u", (unsigned)c->population);
  /* Golden: population digits sit at a fixed x regardless of digit count
   * (measured: every single-digit row's ink starts at the same native x,
   * a 2-digit row's "1" glyph just has some left-side padding within its
   * own cell) — left-aligned on the icon, not centered in its width. */
  reports_draw_line(
    font, fb, REPORTS_COLONY_ICON_X + REPORTS_COLONY_POP_DX, row_top,
    line, reports_colony_pop_color(sol_pct)
  );
  reports_draw_line(font, fb, REPORTS_COLONY_NAME_X, row_top, c->name, REPORTS_COLONY_LABEL_COLOR);
}

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
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_COLONY);
  font = (view && view->title_font_ok) ? &view->title_font : font;
  if (font) {
    /* LABELS.TXT @MISC index 208 (2026-08-27 fix). */
    const char* live = reports_labels_field("MISC", 208);
    const char* kSubtitle = live ? live : "";
    const int w = font_text_width(font, kSubtitle);
    reports_draw_line(font, fb, (fb->width - w) / 2, y - 1, kSubtitle, REPORTS_COLONY_LABEL_COLOR);
  }
  if (!col1) {
    return;
  }

  ReportsColonyRow rows[REPORTS_COLONY_ROWS_PER_PAGE];
  const int row_count = reports_colony_page_rows(col1, human, colonies, page, rows);
  for (int ri = 0; ri < row_count; ++ri) {
    const ColonizeCol1Colony* c = rows[ri].c;
    const ColonizeColony* colony = rows[ri].colony;
    const int row_top = rows[ri].row_top;
    reports_render_colony_sidebar(view, col1, c, colony, font, fb, row_top, line, line_sz);

    /* Units on this colony's own tile, drawn exactly as on the map
     * (allegiance/orders chrome + real map sprite — unit_chrome_blit_unit,
     * same call shape as colony_screen.c's docked-transport row). */
    if (units && icons) {
      /* col1_bridge_apply's transport_chain walk (col1_find_ship_root)
       * conflates "linked to a ship elsewhere in this tile's stacking
       * chain" with "actually boarded" — a land unit merely standing next
       * to a docked ship on a colony tile gets u->orders forced to Sentry
       * (aboard_ship_id set) even though the raw save's own orders byte is
       * untouched (verified: Fortified=6 in save, Sentry=1 in the bridged
       * pool). Read the *raw* orders straight from col1 here instead of
       * trusting the pool's, so the drawn letter matches the golden
       * (colony_p1.png: garrisoned units show 'F', not 'S'). Sprite/type
       * selection is unaffected by this bug and still comes from the pool. */
      bool* raw_used = col1->head.unit_count > 0 ? calloc(col1->head.unit_count, sizeof(bool)) : NULL;
      const ColonizePalette* active_palette =
        (view && view->background_ok[COLONIZE_REPORT_COLONY] &&
         view->backgrounds[COLONIZE_REPORT_COLONY].has_palette)
          ? &view->backgrounds[COLONIZE_REPORT_COLONY].palette
          : NULL;
      /* DOS sizes the row before drawing it (FUN_3f41_1ed8): the pitch is
       * clamp(0xd2 / n, 1, 0x12), where n is the tile stack's "field 10"
       * count (FUN_1427_0d38 case 10 — non-ship units whose @UNIT attack is
       * strictly greater than 1, so a Scout is drawn but doesn't tighten the
       * row), and icons are emitted from x=110 for as long as x <= 0x12c.
       * There is no fixed slot cap: a 10-unit garrison draws all 10. */
      int stack_n = 0;
      for (int u = 0; u < COLONIZE_UNITS_MAX; ++u) {
        const ColonizeUnit* unit = &units->units[u];
        if (!unit->active || unit->nation_id != human) {
          continue;
        }
        if (unit->x != c->x || unit->y != c->y || units_is_sea(units, unit->id)) {
          continue;
        }
        const ColonizeUnitType* ut = units_type(units, unit->type_index);
        if (ut && ut->attack > 1) {
          stack_n++;
        }
      }
      if (stack_n < 1) {
        stack_n = 1;
      }
      int pitch = REPORTS_COLONY_UNIT_SPAN / stack_n;
      if (pitch < 1) {
        pitch = 1;
      }
      if (pitch > REPORTS_COLONY_UNIT_PITCH) {
        pitch = REPORTS_COLONY_UNIT_PITCH;
      }
      /*
       * DOS re-sorts the tile's unit chain before drawing it
       * (FUN_3f41_1ed8 raw 110106-110108, `FUN_1427_04d6(head, 1)`).
       * FUN_1427_04d6 (raw ~7612-7683) buckets the chain by a pseudo-size
       * key, walking size classes 6 down to 1 and moving each match to the
       * front of the list, so the *last*-processed (smallest, 1) bucket
       * ends up frontmost and size 6 ends up at the back — i.e. the chain,
       * read front-to-back, is *ascending* by size, and FUN_3f41_1ed8 draws
       * it front-to-back, so the visible row is size-ascending too. With
       * `param_2 == 1` the size key (normally the `@UNIT` size/space column,
       * `DS:0x5238`) is overridden for four types: Dragoon/Scout/Cont.Cav.
       * (@UNIT 4/5/7) force to pseudo-size 2, Artillery (@UNIT 0xb) to
       * pseudo-size 3. Reproduced here as a stable ascending sort of a
       * copied id list (same idiom as map_panel_stack_rank), leaving the
       * pool untouched.
       */
      int stack_ids[COLONIZE_UNITS_MAX];
      int stack_count = 0;
      for (int u = 0; u < COLONIZE_UNITS_MAX; ++u) {
        const ColonizeUnit* unit = &units->units[u];
        if (!unit->active || unit->nation_id != human) {
          continue;
        }
        if (unit->x != c->x || unit->y != c->y) {
          continue;
        }
        /* Ships docked at the colony's tile aren't part of the garrison
         * (golden: colony_p1.png never shows a hull here, only land units). */
        if (units_is_sea(units, unit->id)) {
          continue;
        }
        /* Only attack-capable land units defend a colony (Soldiers,
         * Dragoons, Scouts, Regulars/Cont.Cav/Cavalry/Cont.Army, Artillery
         * — NAMES.TXT @UNIT attack column > 0); unarmed Colonists,
         * Pioneers, Missionaries, Treasure, and Wagon Trains never show on
         * the defenders list even when standing on the colony's tile. */
        if (!combat_unit_is_combat_role(units, unit->id)) {
          continue;
        }
        if (units_map_sprite(units, unit->id) < 0) {
          continue;
        }
        stack_ids[stack_count++] = unit->id;
      }
      /* Stable insertion sort, pseudo-size ascending (ties keep pool order,
       * matching the chain-order preservation of FUN_1427_04d6's per-bucket
       * front-insert walk). */
      for (int i = 1; i < stack_count; ++i) {
        const int id = stack_ids[i];
        const ColonizeUnit* unit = units_get_const(units, id);
        const int dos_type = unit ? units_display_type_index(units, id) : 0;
        const ColonizeUnitType* ut = unit ? units_type(units, unit->type_index) : NULL;
        int rank = ut ? ut->space : 0;
        if (dos_type == 4 || dos_type == 5 || dos_type == 7) {
          rank = 2;
        } else if (dos_type == 11) {
          rank = 3;
        }
        int j = i - 1;
        while (j >= 0) {
          const ColonizeUnit* pu = units_get_const(units, stack_ids[j]);
          const int pdos_type = pu ? units_display_type_index(units, stack_ids[j]) : 0;
          const ColonizeUnitType* put = pu ? units_type(units, pu->type_index) : NULL;
          int prank = put ? put->space : 0;
          if (pdos_type == 4 || pdos_type == 5 || pdos_type == 7) {
            prank = 2;
          } else if (pdos_type == 11) {
            prank = 3;
          }
          if (prank <= rank) {
            break;
          }
          stack_ids[j + 1] = stack_ids[j];
          --j;
        }
        stack_ids[j + 1] = id;
      }

      int x = REPORTS_COLONY_UNIT_X;
      for (int si = 0; si < stack_count && x <= REPORTS_COLONY_UNIT_X_MAX; ++si) {
        const ColonizeUnit* unit = units_get_const(units, stack_ids[si]);
        if (!unit) {
          continue;
        }
        const int sprite = units_map_sprite(units, unit->id);
        /*
         * WARNING — pool index used as a DOS @UNIT id. units_display_type_index
         * returns a Linux POOL INDEX; `ru->type` below is the col1 save's raw
         * DOS type byte, and unit_chrome_corner_for_type's argument is a DOS
         * @UNIT id. The three only agree because of the NAMES.TXT ordering
         * invariant documented at unit_chrome.c's unit_chrome_corner_for_type
         * (units_load_types appends the @UNIT rows in file order, and the
         * shipped section is exactly Colonists 0 … Mtd. Warriors 0x16). This is
         * the third consumer of that invariant (chrome corner, europe.c's dock
         * display type, this row match); it is cosmetic here — a mismatch picks
         * the wrong raw record's orders letter or the wrong badge corner — so
         * the numeric compare is kept rather than pushed through a name lookup.
         * Do NOT copy this pattern into a rules path (units.c:5117).
         */
        const int dos_unit_type_id = units_display_type_index(units, unit->id);
        int orders = unit->orders;
        if (raw_used) {
          for (uint16_t ri = 0; ri < col1->head.unit_count; ++ri) {
            if (raw_used[ri]) {
              continue;
            }
            const ColonizeCol1Unit* ru = &col1->unit[ri];
            if (ru->nation_id == (uint8_t)human && ru->x == c->x && ru->y == c->y &&
                ru->type == (uint8_t)dos_unit_type_id) {
              orders = ru->orders;
              raw_used[ri] = true;
              break;
            }
          }
        }
        unit_chrome_blit_unit_for_palette(
          fb, font, icons, sprite, x, row_top - 3, dos_unit_type_id, unit->nation_id, orders,
          false,
          /* Badge arm 4 = Artillery + damaged (+0x3148 bit7), not aboard. */
          (unit->col1_flags15 & 0x80u) != 0, active_palette
        );
        x += pitch;
      }
      free(raw_used);
    }
  }
}

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
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_COLONY);
  font = (view && view->title_font_ok) ? &view->title_font : font;
  if (font) {
    /* LABELS.TXT @MISC index 209 (2026-08-27 fix). */
    const char* live = reports_labels_field("MISC", 209);
    const char* kSubtitle = live ? live : "";
    const int w = font_text_width(font, kSubtitle);
    reports_draw_line(font, fb, (fb->width - w) / 2, y - 1, kSubtitle, REPORTS_COLONY_LABEL_COLOR);
  }
  if (!col1) {
    return;
  }

  const int town_hall_idx = colonies ? colonies_building_row(colonies, COLONY_BUILDING_TOWN_HALL) : -1;
  /* DOS-LITERAL FUN_15eb_1f72 raw 12634-12641: AI subsidy also needs Bolivar. #938b. */
  const bool nation_is_ai =
    human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT && col1->player[human].control != 0 &&
    founding_fathers_nation_has(col1, human, FF_SIMON_BOLIVAR);
  const int statesmen_pct =
    founding_fathers_nation_has(col1, human, FF_THOMAS_JEFFERSON) ? 50 : 0;
  const int paine_tax_pct =
    (founding_fathers_nation_has(col1, human, FF_THOMAS_PAINE) &&
     human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT)
      ? (int)col1->nation[human].tax_rate
      : 0;

  /* `rows[].colony` is the pool colony paired by tile — needed for both the
   * Bolivar-aware SoL% and the bell-production formula below, same formula
   * turn.c's EOT bells tally uses. */
  ReportsColonyRow rows[REPORTS_COLONY_ROWS_PER_PAGE];
  const int row_count = reports_colony_page_rows(col1, human, colonies, page, rows);
  for (int ri = 0; ri < row_count; ++ri) {
    const ColonizeCol1Colony* c = rows[ri].c;
    const ColonizeColony* colony = rows[ri].colony;
    const int row_top = rows[ri].row_top;
    reports_render_colony_sidebar(view, col1, c, colony, font, fb, row_top, line, line_sz);

    const int sol_pct = colony ? colony_prod_sol_percent(col1, colony) : reports_colony_rebel_pct(c);
    if (icons) {
      ss_blit_sprite(icons, REPORTS_COLONY_FLAG_ICON, fb, REPORTS_COLONY_FLAG_X, row_top - 3);
    }
    snprintf(line, line_sz, "%d%%", sol_pct);
    reports_draw_line(font, fb, REPORTS_COLONY_PCT_X, row_top, line, REPORTS_COLONY_LABEL_COLOR);

    /* Printing Press/Newspaper — 2-bit tier bitfield, same popcount
     * convention as fortification; Newspaper implies Press. NAMES.TXT
     * @BUILDING col 0: row 20 "Newspaper", row 19 "Printing Press". */
    const unsigned press_bits = c->buildings.printing_press;
    const char* press_label = NULL;
    if (press_bits & 2u) {
      const char* live = reports_names_field("BUILDING", 20, 0);
      press_label = live ? live : "";
    } else if (press_bits & 1u) {
      /* The short form is its own catalog row — LABELS.TXT @MISC 202 "Press"
       * — not @BUILDING row 19 ("Printing Press"), which would overflow the
       * 153px column. */
      press_label = reports_misc_display_word(202, "");
    }
    if (press_label) {
      reports_draw_line(font, fb, REPORTS_COLONY_BUILDING_X, row_top, press_label, REPORTS_COLONY_LABEL_COLOR);
    }

    /* Bell production — same formula turn.c's EOT bells tally uses. */
    int bells = 0;
    if (colonies && colony) {
      const int sol_bonus = colony_prod_sol_bonus(col1, colony);
      bells = colony_prod_colony_bells_ff(
        colonies, colony, statesmen_pct, paine_tax_pct, nation_is_ai, sol_bonus
      );
    }
    if (icons) {
      ss_blit_sprite(icons, REPORTS_COLONY_BELL_ICON, fb, REPORTS_COLONY_BELL_X, row_top - 3);
    }
    snprintf(line, line_sz, "%d", bells);
    reports_draw_line(font, fb, REPORTS_COLONY_BELL_NUM_X, row_top, line, REPORTS_COLONY_LABEL_COLOR);

    /* Colonists currently working the Town Hall — same drop-shadow icon
     * convention as the Labor report's profession icons
     * (units_job_icon_sprite + unit_chrome_blit's SPRITE_WITH_SHADOW mode). */
    if (colony && town_hall_idx >= 0 && icons) {
      int slot = 0;
      for (int p = 0; p < colony->colonist_count && slot < REPORTS_COLONY_WORKER_MAX; ++p) {
        const ColonizeColonist* col = &colony->colonists[p];
        if (!col->active || col->building_type != town_hall_idx) {
          continue;
        }
        const int sprite = units_job_icon_sprite(col->profession);
        if (sprite < 0 || sprite >= icons->sprite_count) {
          continue;
        }
        const int x = REPORTS_COLONY_WORKER_X + slot * REPORTS_COLONY_WORKER_PITCH;
        unit_chrome_blit(
          fb, NULL, icons, sprite, x, row_top - 3, UNIT_CHROME_SPRITE_WITH_SHADOW, 0, 0, -1, 0,
          false, false, -1, -1
        );
        slot++;
      }
    }
  }
}
