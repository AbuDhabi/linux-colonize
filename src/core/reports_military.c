/*
 * Sections:
 *  - Naval/Military report (~line 89)
 *  - Foreign affairs report (~line 570)
 *  - Indian/tribe report (~line 910)
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
 * Naval report (F7) table (golden: naval.png). 4 columns: Ship (icon + class
 * name), Cargo (goods icons packed onto the ship's own row; a passenger gets
 * its own row ABOVE the ship's row, unit_chrome icon + type name, no ship
 * info in that row's Ship cell), Location, Destination. One flat row list is
 * built once (reports_naval_build_rows) and reused for both page_count and
 * render, same shape as the ship's own row/passenger-row split the golden
 * shows for a Caravel carrying one colonist + a full stack of Trade Goods.
 */
#define REPORTS_NAVAL_ROWS_PER_PAGE 7
#define REPORTS_NAVAL_ROW0_Y 40 /* first horizontal rule (golden: naval.png hline scan) */
#define REPORTS_NAVAL_ROW_STEP 20
#define REPORTS_NAVAL_HEADER_Y 27
#define REPORTS_NAVAL_VLINE_TOP_Y 25 /* column rules start a row above the data
   grid, level with the headers — same convention as REPORTS_ECON1_VLINE_TOP_Y */
#define REPORTS_NAVAL_DIV1_X 82 /* Ship | Cargo */
#define REPORTS_NAVAL_DIV2_X 162 /* Cargo | Location */
#define REPORTS_NAVAL_DIV3_X 242 /* Location | Destination */
#define REPORTS_NAVAL_SHIP_ICON_X 0
#define REPORTS_NAVAL_SHIP_NAME_X 26
#define REPORTS_NAVAL_CARGO_ICON_X (REPORTS_NAVAL_DIV1_X + 2)
#define REPORTS_NAVAL_CARGO_ICON_PITCH 14 /* same goods-hold pitch as colony_screen.c's COLONY_HOLD_PITCH */
#define REPORTS_NAVAL_CARGO_LABEL_X (REPORTS_NAVAL_DIV1_X + 30) /* passenger-row type name */
#define REPORTS_NAVAL_ICON_DY 2 /* icon top = row_top + this */
#define REPORTS_NAVAL_TEXT_DY 8 /* text top = row_top + this */
#define REPORTS_NAVAL_LINE_COLOR 119 /* dark red (134,0,0) — same index as REPORTS_ECON_LINE_COLOR */
#define REPORTS_NAVAL_HEADER_COLOR 14 /* bright yellow (255,243,93 in golden) */
#define REPORTS_NAVAL_TEXT_COLOR 97 /* pale cream (247,243,199) — same index as REPORTS_ECON_VALUE_COLOR */
#define REPORTS_NAVAL_CARGO_ICON_BASE 22 /* ICONS.SS — same as colony_screen.h COLONY_CARGO_ICON_BASE */
#define REPORTS_NAVAL_CARGO_GREY_BASE 38 /* ICONS.SS — same as colony_screen.h COLONY_CARGO_GREY_BASE */
#define REPORTS_NAVAL_ROWS_MAX 96

typedef struct NavalRow {
  bool has_ship;
  int ship_sprite;
  int ship_type;
  int ship_nation;
  int ship_orders;
  const char* ship_name;

  bool has_passenger;
  int pass_sprite;
  int pass_type;
  int pass_nation;
  int pass_orders;
  /* DOS badge arm 4 = Artillery + damaged (+0x3148 bit7), not "aboard" —
   * unit_chrome_corner_for_type. Europe-lane cargo has no unit, so false. */
  bool pass_damaged;
  const char* pass_label;

  int goods_icon[COLONIZE_UNIT_CARGO_MAX];
  int goods_count;

  char location[40];
  char destination[40];
} NavalRow;

/* Colony name at (x,y) if this nation (or any nation — golden only shows a
 * human colony, but a foreign port would read the same way) has one there;
 * else the raw coordinates, matching the report spec's Location column. */
/* ===================== Naval/Military report (reports_naval_location .. reports_render_naval) ===================== */
static void reports_naval_location(
  const ColonizeColonyPool* colonies, int x, int y, char* out, size_t out_sz
) {
  const ColonizeColony* c = colonies_find_at_xy(colonies, x, y);
  if (c) {
    snprintf(out, out_sz, "%s", c->name);
    return;
  }
  snprintf(out, out_sz, "(%d, %d)", x, y);
}

static int reports_naval_goods_icon(int cargo_type, int amount) {
  /* Grey vs colored: same "100-per-stack" rule as colony_screen.c's docked-
   * transport hold display (colony_screen_blit_cargo's `partial = amt<100`). */
  const bool grey = amount < 100;
  return (grey ? REPORTS_NAVAL_CARGO_GREY_BASE : REPORTS_NAVAL_CARGO_ICON_BASE) + cargo_type;
}

/*
 * Plural expert label for a passenger row.
 *
 * DOS has no text channel here: the Naval Adviser body `FUN_3f41_1ed8`
 * (raw 70555-70620) draws the ship name once (`FUN_281f_013c`, colony/ship
 * record +2) and then one SPRITE per qualifying passenger
 * (`FUN_281f_02bc(100, 0xffff)`), filtered on the @UNIT attack column
 * (`type*0xe + 0x5236 != 0`) and the type band `< 0xd || > 0x12`. So this
 * column is a port-side readability extension, not a transcription
 * (bugs.md #605).
 *
 * The rule it follows is DOS's own identity rule, the one the map panel's
 * `FUN_49dd_0386` (`units_profession_line`) uses: a unit is named by its
 * PROFESSION when that profession is a skilled one, and by its @UNIT row
 * otherwise. The old five-entry whitelist (professions 20-24, the equipment
 * kits) was a curve fit to naval.png's single example and left every other
 * expert — an Expert Fisherman most visibly — reading as plain "Colonists".
 *
 * `FUN_15eb_0002`'s unskilled set is 0x13 Colonist, 0x19 Ind. Servant,
 * 0x1a Criminal, 0x1b Convert and 0x1c none; those take the @UNIT plural.
 */
static const char* reports_naval_passenger_label(int profession, const char* base_name) {
  const bool skilled =
    profession >= 0 && profession != UNITS_JOB_NONE && profession != UNITS_JOB_COLONIST &&
    profession != UNITS_JOB_SERVANT && profession != UNITS_JOB_CRIMINAL &&
    profession != UNITS_JOB_CONVERT;
  if (skilled) {
    /* NAMES.TXT @JOB column 1 ("Expert Fishermen", "Hardy Pioneers"). */
    const char* job = reports_job_name(profession);
    if (job && job[0]) {
      return job;
    }
  }
  return (base_name && base_name[0]) ? base_name : "";
}

/* Builds the flat ship/passenger row list (on-mapboard ships from `units`,
 * Europe-side ships from `europe`'s harbor/expected/bound lists — a ship
 * mid-Atlantic exists only in the latter, never in `units`, until it
 * arrives). Returns the row count (<= max_rows). Shared by page_count and
 * render so pagination always matches what's actually drawn. */
static int reports_naval_build_rows(
  int human,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  NavalRow* rows,
  int max_rows
) {
  int n = 0;
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX && n < max_rows; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != human) {
        continue;
      }
      if (!units_is_sea(units, u->id)) {
        continue;
      }
      /* Docked-in-Europe ships are represented separately (and more
       * completely — resolved cargo/hold state) via europe->harbor[]. */
      if (reports_unit_in_europe(u->x, u->y)) {
        continue;
      }
      for (int c = 0; c < u->cargo_count && c < COLONIZE_UNIT_CARGO_MAX && n < max_rows; ++c) {
        const ColonizeUnit* pax = units_get_const(units, u->cargo_ids[c]);
        if (!pax) {
          continue;
        }
        NavalRow* r = &rows[n++];
        memset(r, 0, sizeof(*r));
        r->has_passenger = true;
        r->pass_sprite = units_map_sprite(units, pax->id);
        r->pass_type = units_display_type_index(units, pax->id);
        r->pass_nation = pax->nation_id;
        r->pass_orders = pax->orders;
        r->pass_damaged = (pax->col1_flags15 & 0x80u) != 0;
        const ColonizeUnitType* pt = units_type(units, pax->type_index);
        r->pass_label = reports_naval_passenger_label(pax->profession, pt ? pt->name : NULL);
        reports_naval_location(colonies, u->x, u->y, r->location, sizeof(r->location));
      }
      if (n >= max_rows) {
        break;
      }
      NavalRow* r = &rows[n++];
      memset(r, 0, sizeof(*r));
      r->has_ship = true;
      r->ship_sprite = units_map_sprite(units, u->id);
      r->ship_type = units_display_type_index(units, u->id);
      r->ship_nation = u->nation_id;
      r->ship_orders = u->orders;
      const ColonizeUnitType* st = units_type(units, u->type_index);
      r->ship_name = (st && st->name[0]) ? st->name : "";
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX && r->goods_count < COLONIZE_UNIT_CARGO_MAX; ++h) {
        const int amt = u->hold_goods_amount[h];
        const int gtype = u->hold_goods_type[h];
        /* 255 is the DOS empty-hold sentinel, not a quantity — same guard as
         * every europe.c hold consumer (europe_goods_slots_used :141,
         * europe_sell_cargo :3238, col1_bridge import :1103/:1393). Without
         * it a sentinel hold paints a phantom cargo icon on this row. */
        if (amt > 0 && amt < 255 && gtype >= 0 && gtype < (int)COLONIZE_CARGO_COUNT) {
          r->goods_icon[r->goods_count++] = reports_naval_goods_icon(gtype, amt);
        }
      }
      reports_naval_location(colonies, u->x, u->y, r->location, sizeof(r->location));
      if (units_orders_follow_goto(u->orders) && u->goto_x != UNITS_GOTO_NONE &&
          u->goto_y != UNITS_GOTO_NONE) {
        snprintf(r->destination, sizeof(r->destination), "(%d, %d)", u->goto_x, u->goto_y);
      }
    }
  }

  /* Europe-side ships — harbor (docked, Location = port), expected (sailing
   * back to Europe, Location = High Seas / Destination = port), bound
   * (sailing to the New World, Location = High Seas / Destination = this
   * nation's named colony region). Not independently golden-verified (no
   * Europe-side ship in naval.png's save) — built from EuropeScreen's own
   * already-resolved harbor/expected/bound lists by the same shape as the
   * on-mapboard loop above. */
  if (europe) {
    /* LABELS.TXT @MISC row 60 "High Seas". */
    const char* high_seas = reports_misc_display_word(60, "");
    struct {
      const EuropeHarborShip* list;
      int count;
      const char* loc;
      const char* dest;
    } lanes[3] = {
      {europe->harbor, europe->harbor_ships, europe->port_city, ""},
      {europe->expected, europe->expected_ships, high_seas, europe->port_city},
      {europe->bound, europe->bound_ships, high_seas, europe->colony_region},
    };
    for (int lane = 0; lane < 3; ++lane) {
      for (int i = 0; i < lanes[lane].count && n < max_rows; ++i) {
        const EuropeHarborShip* s = &lanes[lane].list[i];
        for (int c = 0; c < s->cargo_count && c < EUROPE_SHIP_CARGO_MAX && n < max_rows; ++c) {
          NavalRow* r = &rows[n++];
          memset(r, 0, sizeof(*r));
          r->has_passenger = true;
          const ColonizeUnitType* pt = units ? units_type(units, s->cargo_types[c]) : NULL;
          r->pass_sprite = pt ? europe_passenger_icon_sprite(units, s->cargo_types[c], s->cargo_professions[c]) : -1;
          r->pass_type = s->cargo_types[c];
          r->pass_nation = human;
          r->pass_orders = 1; /* Sentry — aboard, matching the docked/undirected passenger look */
          r->pass_label = reports_naval_passenger_label(
            s->cargo_professions[c], pt ? pt->name : NULL
          );
          snprintf(r->location, sizeof(r->location), "%s", lanes[lane].loc);
        }
        if (n >= max_rows) {
          break;
        }
        NavalRow* r = &rows[n++];
        memset(r, 0, sizeof(*r));
        r->has_ship = true;
        const ColonizeUnitType* st = (s->type_index >= 0 && units) ? units_type(units, s->type_index) : NULL;
        r->ship_sprite = st ? st->icon_sprite : -1;
        r->ship_type = s->type_index;
        r->ship_nation = human;
        r->ship_orders = 1;
        r->ship_name = (st && st->name[0]) ? st->name : (s->name[0] ? s->name : "");
        for (int h = 0; h < EUROPE_SHIP_CARGO_MAX && r->goods_count < COLONIZE_UNIT_CARGO_MAX; ++h) {
          const int amt = s->hold_goods_amount[h];
          const int gtype = s->hold_goods_type[h];
          /* 255 = empty-hold sentinel; see the mapboard loop above. */
          if (amt > 0 && amt < 255 && gtype >= 0 && gtype < (int)COLONIZE_CARGO_COUNT) {
            r->goods_icon[r->goods_count++] = reports_naval_goods_icon(gtype, amt);
          }
        }
        snprintf(r->location, sizeof(r->location), "%s", lanes[lane].loc);
        snprintf(r->destination, sizeof(r->destination), "%s", lanes[lane].dest);
      }
    }
  }
  return n;
}

int reports_naval_page_count_w(
  const ColonizeWorld* w,
  int human_nation
) {
  const ColonizeUnitPool* units = w->units;
  const ColonizeColonyPool* colonies = w->colonies;
  const EuropeScreen* europe = w->europe;

  NavalRow rows[REPORTS_NAVAL_ROWS_MAX];
  const int n = reports_naval_build_rows(human_nation, units, colonies, europe, rows, REPORTS_NAVAL_ROWS_MAX);
  int pages = (n + REPORTS_NAVAL_ROWS_PER_PAGE - 1) / REPORTS_NAVAL_ROWS_PER_PAGE;
  if (pages < 1) {
    pages = 1;
  }
  return pages;
}


/* Column-centered text, e.g. the header row and the Location/Destination
 * cells (golden: both header and body text sit centered in their column,
 * unlike the left-aligned Ship/Cargo name text next to an icon). */
static void reports_naval_draw_centered(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int col_left,
  int col_right,
  int y,
  const char* text,
  uint8_t color
) {
  const int w = font ? font_text_width(font, text) : 0;
  reports_draw_line(font, fb, col_left + ((col_right - col_left) - w) / 2, y, text, color);
}

void reports_render_naval(
  const ColonizeReportsView* view,
  int human,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int page
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_NAVAL);
  /* Body text needs FONTTINY, not FONTSMAL — golden's mixed-case, ~2px/char
   * ship/cargo/location text is far narrower than FONTSMAL renders (which
   * also turned out to be upper-case-only at this size); same pitfall as
   * Congress page 1's body (docs/report_screens.md). */
  font = (view && view->title_font_ok) ? &view->title_font : font;
  /* Column headers live from LABELS.TXT @MISC (2026-08-27 fix): Ship #61,
   * Cargo #62, Location #63, Destination #64 — a clean consecutive block. */
  const char* ship_w = reports_labels_field("MISC", 61);
  const char* cargo_w = reports_labels_field("MISC", 62);
  const char* location_w = reports_labels_field("MISC", 63);
  const char* dest_w = reports_labels_field("MISC", 64);
  reports_naval_draw_centered(
    font, fb, 0, REPORTS_NAVAL_DIV1_X, REPORTS_NAVAL_HEADER_Y, ship_w ? ship_w : "",
    REPORTS_NAVAL_HEADER_COLOR
  );
  reports_naval_draw_centered(
    font, fb, REPORTS_NAVAL_DIV1_X, REPORTS_NAVAL_DIV2_X, REPORTS_NAVAL_HEADER_Y,
    cargo_w ? cargo_w : "", REPORTS_NAVAL_HEADER_COLOR
  );
  reports_naval_draw_centered(
    font, fb, REPORTS_NAVAL_DIV2_X, REPORTS_NAVAL_DIV3_X, REPORTS_NAVAL_HEADER_Y,
    location_w ? location_w : "", REPORTS_NAVAL_HEADER_COLOR
  );
  reports_naval_draw_centered(
    font, fb, REPORTS_NAVAL_DIV3_X, fb->width, REPORTS_NAVAL_HEADER_Y, dest_w ? dest_w : "",
    REPORTS_NAVAL_HEADER_COLOR
  );

  const int table_bottom = REPORTS_NAVAL_ROW0_Y + REPORTS_NAVAL_ROWS_PER_PAGE * REPORTS_NAVAL_ROW_STEP;
  for (int i = 0; i <= REPORTS_NAVAL_ROWS_PER_PAGE; ++i) {
    reports_draw_hline(
      fb, 0, fb->width, REPORTS_NAVAL_ROW0_Y + i * REPORTS_NAVAL_ROW_STEP, REPORTS_NAVAL_LINE_COLOR
    );
  }
  reports_draw_vline(fb, REPORTS_NAVAL_DIV1_X, REPORTS_NAVAL_VLINE_TOP_Y, table_bottom, REPORTS_NAVAL_LINE_COLOR);
  reports_draw_vline(fb, REPORTS_NAVAL_DIV2_X, REPORTS_NAVAL_VLINE_TOP_Y, table_bottom, REPORTS_NAVAL_LINE_COLOR);
  reports_draw_vline(fb, REPORTS_NAVAL_DIV3_X, REPORTS_NAVAL_VLINE_TOP_Y, table_bottom, REPORTS_NAVAL_LINE_COLOR);

  NavalRow rows[REPORTS_NAVAL_ROWS_MAX];
  const int total = reports_naval_build_rows(human, units, colonies, europe, rows, REPORTS_NAVAL_ROWS_MAX);
  const int skip = page * REPORTS_NAVAL_ROWS_PER_PAGE;
  const ColonizePalette* active_palette =
    (view && view->background_ok[COLONIZE_REPORT_NAVAL] && view->backgrounds[COLONIZE_REPORT_NAVAL].has_palette)
      ? &view->backgrounds[COLONIZE_REPORT_NAVAL].palette
      : NULL;

  for (int row = 0; row < REPORTS_NAVAL_ROWS_PER_PAGE; ++row) {
    const int idx = skip + row;
    if (idx >= total) {
      break;
    }
    const NavalRow* r = &rows[idx];
    const int row_top = REPORTS_NAVAL_ROW0_Y + row * REPORTS_NAVAL_ROW_STEP;
    const int icon_y = row_top + REPORTS_NAVAL_ICON_DY;
    const int text_y = row_top + REPORTS_NAVAL_TEXT_DY;

    if (r->has_ship && icons && r->ship_sprite >= 0) {
      unit_chrome_blit_unit_for_palette(
        fb, font, icons, r->ship_sprite, REPORTS_NAVAL_SHIP_ICON_X, icon_y,
        r->ship_type, r->ship_nation, r->ship_orders, false, false, active_palette
      );
    }
    if (r->has_ship && r->ship_name) {
      reports_draw_line(font, fb, REPORTS_NAVAL_SHIP_NAME_X, text_y, r->ship_name, REPORTS_NAVAL_TEXT_COLOR);
    }

    if (r->has_passenger) {
      if (icons && r->pass_sprite >= 0) {
        unit_chrome_blit_unit_for_palette(
          fb, font, icons, r->pass_sprite, REPORTS_NAVAL_CARGO_ICON_X, icon_y,
          r->pass_type, r->pass_nation, r->pass_orders, false, r->pass_damaged, active_palette
        );
      }
      if (r->pass_label) {
        reports_draw_line(
          font, fb, REPORTS_NAVAL_CARGO_LABEL_X, text_y, r->pass_label, REPORTS_NAVAL_TEXT_COLOR
        );
      }
    } else if (icons) {
      for (int g = 0; g < r->goods_count; ++g) {
        const int icon = r->goods_icon[g];
        if (icon >= 0 && icon < icons->sprite_count) {
          ss_blit_sprite(icons, icon, fb, REPORTS_NAVAL_CARGO_ICON_X + g * REPORTS_NAVAL_CARGO_ICON_PITCH, icon_y);
        }
      }
    }

    if (r->location[0]) {
      reports_naval_draw_centered(
        font, fb, REPORTS_NAVAL_DIV2_X, REPORTS_NAVAL_DIV3_X, text_y, r->location, REPORTS_NAVAL_TEXT_COLOR
      );
    }
    if (r->destination[0]) {
      reports_naval_draw_centered(
        font, fb, REPORTS_NAVAL_DIV3_X, fb->width, text_y, r->destination, REPORTS_NAVAL_TEXT_COLOR
      );
    }
  }
}

/*
 * Foreign Affairs report (F8) — one block per Euro nation, fixed English/
 * French/Spanish/Dutch order (golden: foreign.png). Each block: a header
 * rule, "<Leader>'s <Adjective>:" (leader name yellow, adjective cream —
 * two draw calls split at the leader segment's measured width, same idiom
 * as a two-color same-line label elsewhere in this file), then either a
 * centered "(Withdrawn from New World)" (LABELS.TXT @MISC index 190,
 * live-resolved) for the Crown's slot, or the body: an optional Jan de Witt
 * detail grid, a row of "<peer country>: Peace|War", and a
 * "Rebels: N   Tories: N" line.
 *
 * DOS: FUN_3f41_2548 (viceroy_unpacked.c:70787; the per-cell x/y and the
 * argument order Ghidra drops were read off the raw .asm at 3f41:2548..2aca).
 *
 * Block geometry, all from that .asm — `local_60` is the running baseline
 * and every advance is `FONTTINY.height + 1` == LINE_STEP:
 *
 *   header      block_top + 3    "<Leader>'s <Adjective>:"  (x=2)
 *   de Witt A   header + 1 step  Colonies / Average Colony / Population
 *   de Witt B   header + 2 steps Military Power / Naval Power / Merchant Marine
 *   peers       + 1 step         up to 3 cells on ONE line
 *   rebels      + 1 step         (only when at least one peer was drawn)
 *
 * Without de Witt the two detail rows are absent and the peer line lands at
 * block_top + 17, which is what the golden shows.
 *
 * Column x for both the detail grid and the peer line: the .asm starts at
 * `local_5c` (2) and then does "if (x < 0x50) x = 0x50; else x += 0x50",
 * i.e. 2 / 80 / 160 / 240 — a single row, never a 2-column wrap. Only 3
 * peers can ever exist so the peer line stops at 160; the detail grid uses
 * the first three fixed.
 *
 * Crown slot: DOS compares the block's nation to `head.crown_nation_id`
 * (DS:0x53d2), NOT to player.control — that block prints @MISC 190 centered
 * and nothing else. In dutch-reports.SAV crown_nation_id==2 and Spain also
 * has control==2, which is why the earlier control-based reading fit the
 * golden; the DOS gate is the crown one.
 *
 * "Free": when nation_flags bit 0x04 is set DOS splices @MISC 191 ("Free")
 * into the header between the leader's name and the adjective, and drops
 * that block's Rebels/Tories line. Neutral on the golden (no nation has the
 * bit) but ported so a post-independence report reads as DOS does.
 *
 * War/Peace: read straight from `nation[a].euro_relation[b]`, one byte, one
 * direction (the .asm calls the same DS accessor FUN_281f_0a38(a, b) for
 * both tests): bit 0x20 = met (an unmet peer is not listed at all), bit
 * 0x40 = at peace, clear = at war. This supersedes the earlier report-local
 * "(ab|ba) & 0x02" empirical fit, which happened to agree on every pair in
 * dutch-reports.SAV; ai_diplo.h's AI_DIPLO_WAR=0x01 names are still not
 * this byte's DOS decode and are still not touched from here.
 *
 * Rebels/Tories: total = col1->stuff.census_pop_proxy[nation] (DS:0x9410,
 * "+1 skilled unit + Σ colony pop" per col1_save.h — a DOS-computed census
 * byte, RMW-preserved from the loaded save, not recomputed by this port
 * during live play). rebels = floor(total * rebel_sentiment / 100); tories
 * = total - rebels. Confirmed exact for all 3 surviving nations in the
 * golden (75/45/54 total, 21/24/50 rebels, 54/21/4 tories) — colony
 * population alone (col1->colony[].population summed) undercounts by the
 * nation's field colonist-type units (Soldiers/Dragoons/etc., counted in
 * census_pop_proxy but not colony population), which is why this reads
 * the census byte rather than re-deriving the total from colonies+units.
 *
 * Jan de Witt detail grid: gated on the *viewing* nation owning FF #4, or
 * on head.show_entire_map (DOS: `FUN_281f_07b4(viewer, 4) || DS:0x53a2`).
 * Six cells, each drawn as one "<@MISC label>: <n>" string in the same
 * light yellow as Rebels/Tories, values straight off the DOS census block
 * (`stuff`, written by FUN_4962_0018) so they carry whatever the save
 * recorded, exactly like the Rebels/Tories row above:
 *
 *   Colonies        @MISC 95   stuff.colony_counts[n]          (DS:0x9298)
 *   Average Colony  @MISC 97   stuff.avg_colony_pop[n]         (DS:0x944e, u16)
 *   Population      @MISC 96   stuff.census_pop_proxy[n]       (DS:0x9410)
 *   Military Power  @MISC 98   stuff.land_combat_strength[n]>>3(DS:0x941c)
 *   Naval Power     @MISC 99   (privateers + frigates) * 8     (DS:0x924c,
 *                              stuff.unit_type_counts[n][16] and [17] —
 *                              @UNIT ids 16/17; Man-O-War 18 is excluded)
 *   Merchant Marine @MISC 100  stuff.ship_cargo_totals[n]      (DS:0x9414)
 *
 * Note the label order on screen is NOT the @MISC order: row A is 95/97/96
 * and row B is 98/99/100, exactly as the .asm pushes them.
 */
#define REPORTS_FOREIGN_BLOCK0_Y 10 /* first block's rule (golden: foreign.png hline scan) */
#define REPORTS_FOREIGN_BLOCK_STEP 45 /* divider-to-divider spacing, 4 fixed nation blocks */
#define REPORTS_FOREIGN_LINE_STEP 7 /* FONTTINY line pitch within a block */
#define REPORTS_FOREIGN_HEADER_DY 3 /* header line y = block_top + this */
#define REPORTS_FOREIGN_BODY_DY 17 /* first body line (relation grid / withdrawn) = block_top + this;
   header_dy + 2*LINE_STEP — golden shows one blank line between header and body */
#define REPORTS_FOREIGN_COL1_X 2
#define REPORTS_FOREIGN_COL2_X 80
#define REPORTS_FOREIGN_COL3_X 160 /* third cell of the de Witt grid / peer line (.asm x += 0x50) */
#define REPORTS_FOREIGN_DE_WITT_FF 4 /* FF_JAN_DE_WITT — DOS FUN_281f_07b4(viewer, 4) */
#define REPORTS_FOREIGN_NATION_FLAG_FREE 0x04u /* nation_flags bit: splice @MISC 191 into the header */
#define REPORTS_FOREIGN_MET_BIT 0x20u /* euro_relation[b]: peer has been met */
#define REPORTS_FOREIGN_PEACE_BIT 0x40u /* euro_relation[b]: at peace (clear = at war) */
#define REPORTS_FOREIGN_PRIVATEER_TYPE 16 /* @UNIT ids summed for Naval Power */
#define REPORTS_FOREIGN_FRIGATE_TYPE 17
#define REPORTS_FOREIGN_NAVAL_SCALE 8 /* DOS: (privateers + frigates) << 3 */
#define REPORTS_FOREIGN_MILITARY_SHIFT 3 /* DOS: land_combat_strength >> 3 */
#define REPORTS_FOREIGN_LEADER_COLOR 146 /* bright yellow (255,243,93) — REPORT8.PIK palette probe */
#define REPORTS_FOREIGN_ADJ_COLOR 97 /* pale cream (247,243,199) — same index as REPORTS_NAVAL_TEXT_COLOR */
#define REPORTS_FOREIGN_LABEL_COLOR 145 /* light yellow (255,255,142) — peer/nation names, Rebels/Tories */
#define REPORTS_FOREIGN_PEACE_COLOR 15 /* white — REPORT8.PIK palette probe */
#define REPORTS_FOREIGN_WAR_COLOR 112 /* red (243,0,0) — same index as REPORTS_ECON_NEG_COLOR */
#define REPORTS_FOREIGN_RULE_COLOR 119 /* dark red (134,0,0) — same index as REPORTS_NAVAL_LINE_COLOR */

/* The six de Witt cells, in the .asm's draw order (row A then row B). */
typedef enum ForeignDetail {
  FOREIGN_DETAIL_COLONIES = 0,
  FOREIGN_DETAIL_AVG_COLONY,
  FOREIGN_DETAIL_POPULATION,
  FOREIGN_DETAIL_MILITARY,
  FOREIGN_DETAIL_NAVAL,
  FOREIGN_DETAIL_MERCHANT,
  FOREIGN_DETAIL_COUNT
} ForeignDetail;

/* @MISC index + English fallback per cell, same order as ForeignDetail. */
static const int k_foreign_detail_labels[FOREIGN_DETAIL_COUNT] = {95, 97, 96, 98, 99, 100};
static const char* k_foreign_detail_fallbacks[FOREIGN_DETAIL_COUNT] = {
  "", "", "", "", "", ""
};

typedef struct ForeignRow {
  const char* leader;
  const char* adjective;
  bool is_crown; /* head.crown_nation_id — prints @MISC 190, nothing else */
  bool free_nation; /* nation_flags 0x04 — "Free" in the header, no Rebels/Tories line */
  bool detail; /* viewer has de Witt (or the complete-map cheat) */
  int detail_value[FOREIGN_DETAIL_COUNT];
  int peer_nation[COLONIZE_COL1_NATION_COUNT - 1];
  bool peer_war[COLONIZE_COL1_NATION_COUNT - 1];
  int peer_count;
  int rebels;
  int tories;
} ForeignRow;

/*
 * DOS reads one byte, one direction: nation[a].euro_relation[b] bit 0x40 is
 * "at peace", so a met peer without it is at war (see the block comment).
 */
/* ===================== Foreign affairs report (reports_foreign_at_war .. reports_render_foreign) ===================== */
static bool reports_foreign_at_war(const ColonizeCol1Save* col1, int a, int b) {
  if (!col1 || a == b || a < 0 || a >= (int)COLONIZE_COL1_NATION_COUNT || b < 0 ||
      b >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }
  return (col1->nation[a].euro_relation[b] & REPORTS_FOREIGN_PEACE_BIT) == 0;
}

/* x of the nth cell on a body line — the .asm's 2 / 80 / 160 / 240 ladder. */
static int reports_foreign_cell_x(int cell) {
  return cell <= 0 ? REPORTS_FOREIGN_COL1_X : REPORTS_FOREIGN_COL2_X * cell;
}

/* stuff.avg_colony_pop is 4 packed little-endian u16 (DS:0x944e). */
static int reports_foreign_avg_colony(const ColonizeCol1Save* col1, int nation) {
  const uint8_t* p = &col1->stuff.avg_colony_pop[nation * 2];
  return (int)((unsigned)p[0] | ((unsigned)p[1] << 8));
}

/* Fills one block's de Witt cells from the DOS census block. */
static void reports_foreign_fill_detail(const ColonizeCol1Save* col1, int nation, ForeignRow* r) {
  const ColonizeCol1Stuff* st = &col1->stuff;
  const int privateers = st->unit_type_counts[nation][REPORTS_FOREIGN_PRIVATEER_TYPE];
  const int frigates = st->unit_type_counts[nation][REPORTS_FOREIGN_FRIGATE_TYPE];
  r->detail_value[FOREIGN_DETAIL_COLONIES] = st->colony_counts[nation];
  r->detail_value[FOREIGN_DETAIL_AVG_COLONY] = reports_foreign_avg_colony(col1, nation);
  r->detail_value[FOREIGN_DETAIL_POPULATION] = st->census_pop_proxy[nation];
  r->detail_value[FOREIGN_DETAIL_MILITARY] =
    (int)(st->land_combat_strength[nation] >> REPORTS_FOREIGN_MILITARY_SHIFT);
  r->detail_value[FOREIGN_DETAIL_NAVAL] = (privateers + frigates) * REPORTS_FOREIGN_NAVAL_SCALE;
  r->detail_value[FOREIGN_DETAIL_MERCHANT] = st->ship_cargo_totals[nation];
  r->detail = true;
}

/* Builds one row per Euro nation, fixed English/French/Spanish/Dutch order.
 * Shared shape with reports_naval_build_rows even though this report never
 * paginates (always exactly COLONIZE_COL1_NATION_COUNT rows) — kept as its
 * own build step for the same reason: render draws exactly what this
 * function decided, nothing recomputed inline. */
static int reports_foreign_build_rows(
  const ColonizeCol1Save* col1,
  int human,
  ForeignRow* rows,
  int max_rows
) {
  if (!col1) {
    return 0;
  }
  /* DOS: FUN_281f_07b4(viewer, 4) || DS:0x53a2 — the viewer's own FF, so the
   * gate is the same for all four blocks. */
  const bool reveal =
    (human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT &&
     founding_fathers_nation_has(col1, human, REPORTS_FOREIGN_DE_WITT_FF)) ||
    col1->head.show_entire_map != 0;
  /*
   * The Crown's slot is by definition a nation the player is not playing
   * (ai_king_crown_nation always returns a peer), so the viewer's own
   * block can never be the withdrawn one. Guarding here as well as at the
   * source keeps saves written before col1_save_reset_nation_slots existed
   * — where the whole head was zero-filled, making England the "Crown" —
   * from printing "(Withdrawn from New World)" over the player's own
   * nation (bugs.md).
   */
  int crown = (int)col1->head.crown_nation_id;
  if (crown == human) {
    crown = -1;
  }
  int n = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT && n < max_rows; ++i) {
    ForeignRow* r = &rows[n++];
    memset(r, 0, sizeof(*r));
    const ColonizeCol1Player* p = &col1->player[i];
    r->leader = p->name[0] ? p->name : reports_nation_adjective(i);
    r->adjective = reports_nation_adjective(i);
    r->is_crown = (crown == i);
    if (r->is_crown) {
      continue;
    }
    r->free_nation = (col1->nation[i].nation_flags & REPORTS_FOREIGN_NATION_FLAG_FREE) != 0;
    if (reveal) {
      reports_foreign_fill_detail(col1, i, r);
    }
    for (int j = 0; j < (int)COLONIZE_COL1_NATION_COUNT &&
                    r->peer_count < (int)(COLONIZE_COL1_NATION_COUNT - 1);
         ++j) {
      /* Own slot, the Crown's slot, and never-met peers are all skipped. */
      if (j == i || crown == j ||
          (col1->nation[i].euro_relation[j] & REPORTS_FOREIGN_MET_BIT) == 0) {
        continue;
      }
      r->peer_nation[r->peer_count] = j;
      r->peer_war[r->peer_count] = reports_foreign_at_war(col1, i, j);
      r->peer_count++;
    }
    if (r->free_nation) {
      continue; /* DOS skips the Rebels/Tories line for a free nation. */
    }
    const int total = col1->stuff.census_pop_proxy[i];
    const int rebels = (total * (int)col1->nation[i].rebel_sentiment) / 100;
    r->rebels = rebels;
    r->tories = total - rebels;
  }
  return n;
}

void reports_render_foreign(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
) {
  /* Body needs FONTTINY, not FONTSMAL — same pitfall as Naval/Congress
   * page 1 (docs/report_screens.md): golden's text is far narrower than
   * FONTSMAL renders at this size. */
  font = (view && view->title_font_ok) ? &view->title_font : font;

  if (!col1) {
    reports_draw_line(
      font, fb, REPORTS_FOREIGN_COL1_X, 20, "Foreign Affairs requires a loaded Col1 save.",
      REPORTS_FOREIGN_LABEL_COLOR
    );
    return;
  }

  ForeignRow rows[COLONIZE_COL1_NATION_COUNT];
  const int n = reports_foreign_build_rows(col1, human, rows, (int)COLONIZE_COL1_NATION_COUNT);
  char line[64];

  for (int i = 0; i < n; ++i) {
    const ForeignRow* r = &rows[i];
    const int block_top = REPORTS_FOREIGN_BLOCK0_Y + i * REPORTS_FOREIGN_BLOCK_STEP;
    reports_draw_hline(fb, 0, fb->width, block_top, REPORTS_FOREIGN_RULE_COLOR);

    const int header_y = block_top + REPORTS_FOREIGN_HEADER_DY;
    snprintf(line, sizeof(line), "%s's", r->leader);
    reports_draw_line(font, fb, REPORTS_FOREIGN_COL1_X, header_y, line, REPORTS_FOREIGN_LEADER_COLOR);
    int leader_w = font ? font_text_width(font, line) : 0;
    if (r->free_nation) {
      /* @MISC 191 "Free", spliced between the name and the adjective. */
      const char* live = reports_labels_field("MISC", 191);
      snprintf(line, sizeof(line), " %s", live ? live : "");
      reports_draw_line(
        font, fb, REPORTS_FOREIGN_COL1_X + leader_w, header_y, line, REPORTS_FOREIGN_LEADER_COLOR
      );
      leader_w += font ? font_text_width(font, line) : 0;
    }
    snprintf(line, sizeof(line), " %s:", r->adjective);
    reports_draw_line(
      font, fb, REPORTS_FOREIGN_COL1_X + leader_w, header_y, line, REPORTS_FOREIGN_ADJ_COLOR
    );

    /* One step below the header; each drawn body line advances it again. */
    int body_y = header_y + REPORTS_FOREIGN_LINE_STEP;
    if (r->is_crown) {
      /* LABELS.TXT @MISC index 190 (was cited as raw line "#205" — same
       * line-number-vs-index mix-up P2.2's title fix corrected elsewhere;
       * now actually live-resolved, 2026-08-26). DOS advances one extra
       * step before centering this, landing it on the normal body line. */
      body_y += REPORTS_FOREIGN_LINE_STEP;
      const char* live = reports_labels_field("MISC", 190);
      const char* msg = live ? live : "";
      const int w = font ? font_text_width(font, msg) : 0;
      reports_draw_line(font, fb, (fb->width - w) / 2, body_y, msg, REPORTS_FOREIGN_LABEL_COLOR);
      continue;
    }

    if (r->detail) {
      char w[32];
      for (int d = 0; d < FOREIGN_DETAIL_COUNT; ++d) {
        const int row_y = body_y + (d / 3) * REPORTS_FOREIGN_LINE_STEP;
        snprintf(
          line, sizeof(line), "%s: %d",
          reports_misc_word(k_foreign_detail_labels[d], k_foreign_detail_fallbacks[d], w, sizeof(w)),
          r->detail_value[d]
        );
        reports_draw_line(
          font, fb, reports_foreign_cell_x(d % 3), row_y, line, REPORTS_FOREIGN_LABEL_COLOR
        );
      }
      /* Row A sat on body_y, row B one step below; leave body_y on row B so
       * the shared advance below lands the peer line one step under it. */
      body_y += REPORTS_FOREIGN_LINE_STEP;
    }

    /* Peer relations: one line, cells at 2 / 80 / 160 — DOS never wraps. */
    body_y += REPORTS_FOREIGN_LINE_STEP;
    for (int p = 0; p < r->peer_count; ++p) {
      const int col_x = reports_foreign_cell_x(p);
      snprintf(line, sizeof(line), "%s:", reports_nation_country_name(r->peer_nation[p]));
      reports_draw_line(font, fb, col_x, body_y, line, REPORTS_FOREIGN_LABEL_COLOR);
      const int label_w = font ? font_text_width(font, line) : 0;
      /* "War"/"Peace" live from LABELS.TXT @MISC #101/#102 (2026-08-27 fix).
       * Must go through reports_misc_word's caller-owned buffer: resolving
       * both indices up front aliased reports_labels_field's single static
       * buffer, so a War pair printed the word "Peace" (in the War colour). */
      char state[32];
      const char* state_w = r->peer_war[p] ? reports_misc_word(101, "", state, sizeof(state))
                                           : reports_misc_word(102, "", state, sizeof(state));
      snprintf(line, sizeof(line), " %s", state_w);
      reports_draw_line(
        font, fb, col_x + label_w, body_y, line,
        r->peer_war[p] ? REPORTS_FOREIGN_WAR_COLOR : REPORTS_FOREIGN_PEACE_COLOR
      );
    }

    if (r->free_nation) {
      continue; /* DOS drops the Rebels/Tories line once the nation is free. */
    }
    /* DOS only advances again when at least one peer cell was drawn. */
    const int rebels_y = body_y + (r->peer_count > 0 ? REPORTS_FOREIGN_LINE_STEP : 0);
    /* LABELS.TXT @MISC #86 "Rebels" / #87 "Tories" (plural forms are real). */
    char w[32];
    snprintf(line, sizeof(line), "%s: %d", reports_misc_word(86, "", w, sizeof(w)), r->rebels);
    reports_draw_line(font, fb, REPORTS_FOREIGN_COL1_X, rebels_y, line, REPORTS_FOREIGN_LABEL_COLOR);
    snprintf(line, sizeof(line), "%s: %d", reports_misc_word(87, "", w, sizeof(w)), r->tories);
    reports_draw_line(font, fb, REPORTS_FOREIGN_COL2_X, rebels_y, line, REPORTS_FOREIGN_LABEL_COLOR);
  }
}

/*
 * Indian Adviser report (F9) — golden: indian.png. One two-line block per
 * listed tribe. DOS's row gate (FUN_3f41_010a, viceroy_unpacked.c:69480 /
 * asm `3f41:...` after the per-tribe `FUN_281f_0a38`) is
 *
 *     if (((uVar1 & 0x20) != 0) || ((*(byte *)(*(int *)0x8d4e + 3) & 0x80) != 0))
 *
 * i.e. **met** (euro_diplo[human] bit 0x20, COL1_INDIAN_MET_BIT) OR **extinct**
 * (record +3 bit 0x80, ColonizeCol1Indian.extinct) — not "euro_diplo != 0".
 * The distinction is live: euro_diplo carries other bits (0x02 war,
 * 0x04 attack-confirmed, 0x40 peace) that DOS itself can set without the met
 * bit, so a `!= 0` test listed never-met tribes; and it dropped extinct tribes
 * that DOS lists unconditionally (2026-09-09, smell audit #85).
 *
 * Extinct rows are a different shape, straight from the same function: the
 * name line gets DS:0x2ebe (@MISC #130 "Extinct") appended after the colon,
 * and the `TEST [0x8d4e+3],0x80 / JMP LAB_3f41_04e7` right after that line is
 * drawn skips BOTH the right-aligned tech level and the whole stats line
 * (villages/missions/muskets/horse herds) before advancing y by 0x15.
 *
 * Non-extinct rows:
 *
 *   line 1: headband portrait icon + "<PluralTribeName>:" (NAMES.TXT
 *     @TRIBES column 0), colored per tribe (k_indian_tribe_colors below —
 *     unit_chrome.c's own k_tribe_colors, duplicated per the project's
 *     established no-shared-header convention; confirmed an *exact*
 *     0-distance RGB match against REPORT9.PIK's own palette at both
 *     golden indices, no remap needed here unlike ICONS.SS sprites).
 *     Right-aligned: tribe level (Semi-Nomadic/Agrarian/Advanced/Civilized
 *     by indian.tech, reports_tribe_level), same color.
 *   line 2 (fixed columns, black — golden-sampled (0,0,0) exactly), each
 *     column skipped when its count is 0:
 *     "<n> Villages"    — tribe[].nation_id count, always shown.
 *     "<n> Missions"    — villages whose mission's low nibble equals
 *                         `human` (DOS: local_58, `*(byte*)(sel+5)&0xf ==
 *                         param_1` — a per-*viewing-nation* mission count,
 *                         not "any mission"; COL1_TRIBE_MISSION_NATION_MASK).
 *     "<n> Muskets"     — (indian.muskets + count of this tribe's Armed
 *                         Brave/Mtd. Warrior units) * 50. Reverse-engineered
 *                         from FUN_3f41_010a's `local_6c` (viceroy_unpacked.c
 *                         :69545-69553: seeds from `*(char*)(sel+7)`
 *                         [muskets], scans the unit array for
 *                         `type==0x14||type==0x16` [Armed Brave=20 / Mtd.
 *                         Warrior=22 — the two musket-equipped native unit
 *                         types, indians.md's @UNIT table] owned by this
 *                         tribe, `*= 0x32` [50]) — confirmed exact against
 *                         dutch-reports.SAV: Arawak (muskets=0 + 3 Mtd.
 *                         Warriors)*50 = 150, Cherokee (muskets=5 + 7 Armed
 *                         Braves + 2 Mtd. Warriors)*50 = 700, both matching
 *                         indian.png's printed numbers exactly.
 *     "<n> Horse Herds" — indian.horse_herds, raw (DOS: `*(char*)(sel+8)`,
 *                         no scale — confirmed exact, Arawak 5 / Cherokee 6).
 *
 * DOS draws up to all 8 tribes in one unpaginated pass (no page state in
 * FUN_3f41_010a) — this port matches that rather than inventing pagination;
 * with 8 tribes at 21px/row from y=28 the list can in principle run under
 * the OK button exactly like DOS's own screen would.
 *
 * Icon: ICONS.SS #113 + alarm quartile — one expression ramp over five
 * near-identical headband portraits (#113-117, 16x16), of which this call
 * site can reach #113 (calm) .. #116 (hostile). Resolved 2026-09-07 (T5.3)
 * from `3f41:0522`..`3f41:05d2`; see the decode note in
 * reports_indian_build_rows and docs/reports.md "Chief portrait".
 */
#define REPORTS_INDIAN_ROW0_Y 28 /* first tribe name line (golden: indian.png text-color scan) */
#define REPORTS_INDIAN_ROW_STEP 21
/* Icon box, fitted against indian.png by palette-consistency search over
 * (sprite, x, y): x=10, first row top y=25 is the unique zero-violation fit
 * (x=11 scores 120+), i.e. icon top = name_y - 3. */
#define REPORTS_INDIAN_ICON_X 10
#define REPORTS_INDIAN_ICON_DY (-3) /* icon top = name_y + this */
/* Calm face (quartile 0); +quartile picks #114..#116 as alarm rises (see the
 * ramp note in reports_indian_build_rows). */
#define REPORTS_INDIAN_ICON_SPRITE 113
#define REPORTS_INDIAN_NAME_X 30
/*
 * Stats line y = name_y + this. DOS does not hardcode it: at `3f41:02dc` it
 * does `LES BX,[0x89e]` (the FONTTINY far pointer) / `MOV AL,ES:[BX]` (FF
 * header byte 0 = max height) / `INC AX` twice / `ADD [local_6e],AX` — i.e.
 * FONTTINY.max_height + 2 (= 8, and the golden indian.png measures 8: names
 * ink rows 28-32, stats ink rows 36-40). reports_render_indian computes that
 * from the live font; this constant is only the fallback when no font is
 * loaded, and is left at the old 9 for that degenerate case. (Its old
 * comment claimed the gap came from a 9px FONTINTR name row — that font is
 * not on this screen at all; see reports_render_indian.)
 */
#define REPORTS_INDIAN_STATS_DY 9
#define REPORTS_INDIAN_VILLAGES_X 40
#define REPORTS_INDIAN_MISSIONS_X 96
#define REPORTS_INDIAN_MUSKETS_X 153
#define REPORTS_INDIAN_HORSES_X 208
#define REPORTS_INDIAN_LEVEL_RIGHT 310
#define REPORTS_INDIAN_TEXT_COLOR 0 /* black — golden-sampled (0,0,0) exactly */
#define REPORTS_INDIAN_MUSKET_UNIT_SCALE 50

/* VICEROY.EXE initialized DS:0x848[4..11]; duplicated from unit_chrome.c
 * (see that file's note on why the tables aren't shared via a header).
 * Arawak 54 and Cherokee 67 also match the REPORT9.PIK golden exactly. */
static const uint8_t k_indian_tribe_colors[COLONIZE_COL1_INDIAN_COUNT] = {
  15, 149, 54, 11, 67, 111, 117, 71
};

typedef struct IndianRow {
  int nation_id;
  const char* name;
  uint8_t color;
  const char* level;
  uint8_t tech; /* indian.tech — row of NAMES.TXT @LEVELS */
  int villages;
  int missions;
  int muskets;
  int horse_herds;
  int icon_sprite; /* 113 + alarm quartile — chief expression ramp (see build_rows) */
  int extinct;     /* record +3 bit 0x80 — name line only, "Extinct" suffix */
} IndianRow;

/* ===================== Indian/tribe report (reports_indian_tribe_listed .. reports_render_indian) ===================== */
bool reports_indian_tribe_listed(const ColonizeCol1Save* col1, int tribe, int human) {
  if (!col1 || tribe < 0 || tribe >= (int)COLONIZE_COL1_INDIAN_COUNT || human < 0 ||
      human >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }
  const ColonizeCol1Indian* ind = &col1->indian[tribe];
  /* DOS: `(FUN_281f_0a38(...) & 0x20) || ([0x8d4e+3] & 0x80)` — met OR
   * extinct. Never "euro_diplo != 0": the war (0x02) / attack-confirmed
   * (0x04) bits can stand alone on an unmet tribe. */
  return (ind->euro_diplo[human] & COL1_INDIAN_MET_BIT) != 0 || ind->extinct != 0;
}

/* Builds the flat contacted-tribe row list — shared shape with
 * reports_naval_build_rows even though this report has no pagination. */
static int reports_indian_build_rows(
  const ColonizeCol1Save* col1,
  const ColonizeUnitPool* units,
  int human,
  IndianRow* rows,
  int max_rows
) {
  int n = 0;
  if (!col1 || human < 0 || human >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  for (int t = 0; t < (int)COLONIZE_COL1_INDIAN_COUNT && n < max_rows; ++t) {
    const ColonizeCol1Indian* ind = &col1->indian[t];
    if (!reports_indian_tribe_listed(col1, t, human)) {
      continue;
    }
    const int nation_id = t + 4;
    int villages = 0;
    int missions = 0;
    if (col1->tribe) {
      for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
        const ColonizeCol1Tribe* tr = &col1->tribe[i];
        if (tr->nation_id != (uint8_t)nation_id) {
          continue;
        }
        villages++;
        if (tr->mission != COL1_TRIBE_MISSION_NONE &&
            (tr->mission & COL1_TRIBE_MISSION_NATION_MASK) == (uint8_t)human) {
          missions++;
        }
      }
    }
    int armed_units = 0;
    if (units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units->units[i];
        if (!u->active || u->nation_id != nation_id) {
          continue;
        }
        const ColonizeUnitKind kind = units_type_kind(units_type(units, u->type_index));
        if (kind == UNITS_KIND_ARMED_BRAVE || kind == UNITS_KIND_MTD_WARRIOR) {
          armed_units++;
        }
      }
    }
    IndianRow* r = &rows[n++];
    r->nation_id = nation_id;
    r->name = reports_tribe_name(t);
    r->color = k_indian_tribe_colors[t];
    r->level = reports_tribe_level(ind->tech);
    r->tech = ind->tech > 3 ? 3 : ind->tech;
    r->villages = villages;
    r->missions = missions;
    r->muskets = ((int)ind->muskets + armed_units) * REPORTS_INDIAN_MUSKET_UNIT_SCALE;
    r->horse_herds = ind->horse_herds;
    /*
     * Chief face vs alarm, decoded from the raw asm (`3f41:0522`..`3f41:05d2`):
     * `FUN_281f_0254` is handed AX = quartile + 0x72, where the quartile is
     * `FUN_281f_0a60`(= `FUN_15dc_00a2`, cuts at 25/50/75) over
     * `FUN_281f_030c` = the Indian->Euro alarm word toward the *viewing*
     * nation, forced to the top tier when the record's +3 bit 0x80
     * (`extinct`) is set. The five portraits #113..#117 differ only in the
     * mouth (colour 136 spreading from a flat line to a full grin), so they
     * are one expression ramp, not five tribes.
     *
     * 0x72 is a DOS sprite id and those are 1-based over ICONS.SS — the same
     * +1 that makes the F2 crosses bar's 0x39 sheet index #56 and the F3
     * bells bar's 0x3f index #62, both re-confirmed pixel-exact against
     * religious.png / continental_p1.png in this pass. So the sheet index is
     * 113 + quartile, and #113 (not #114) is the calm face: indian.png's two
     * rows are Arawak and Cherokee, both `alarm_by_player[3] == 0` in
     * dutch-reports.SAV, and both fit sprite #113 exactly (0 palette
     * violations over 226 opaque pixels; #114 scores 2, #117 scores 12).
     */
    {
      const int alarm = ai_diplo_indian_alarm(col1, nation_id, human);
      int q = alarm >= 75 ? 3 : alarm >= 50 ? 2 : alarm >= 25 ? 1 : 0;
      if (ind->extinct) {
        q = 3; /* [0x8d4e+3] & 0x80 forces the top tier */
      }
      r->icon_sprite = REPORTS_INDIAN_ICON_SPRITE + q;
    }
    r->extinct = ind->extinct ? 1 : 0;
  }
  return n;
}

void reports_render_indian(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  const ColonizeUnitPool* units,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
) {
  const ColonizeSpriteSheet* icons = reports_icons_for(view, COLONIZE_REPORT_INDIAN);
  /*
   * bugs.md #428 — EVERY line on this screen is FONTTINY, the tribe name and
   * the tech-level word included. Player-observed in DOS, and the overlay
   * agrees byte for byte: module 104b exposes two hard-wired drawer families,
   * one bound to the FONTTINY far pointer at DS:0x89e (FUN_104b_0216 measure,
   * 024e/0288/02c2/0318 draw) and one bound to the FONTINTR pointer at
   * DS:0x268a (0232 measure, 035c/039a/03d2 draw) — the font is chosen purely
   * by which entry point is called, there is no mutable font slot in this
   * path. Across the whole 3f41 report overlay, DS:0x89e is referenced 20+
   * times and DS:0x268a exactly zero times; the Indian Adviser
   * (FUN_3f41_010a) reaches the text layer only through the FONTTINY thunks
   * FUN_281f_0100 / 0114 / 013c, and never through the FONTINTR ones
   * (FUN_281f_018c / 01aa). The tribe name draws at 3f41:022c and the level
   * word at 3f41:02cb, both via FUN_281f_013c.
   *
   * An earlier pass read the tribe name as a heavier font off a screenshot
   * and pointed it at FONTINTR.FF; that was the error, so the separate
   * intro-font slot this used is gone with it.
   */
  font = (view && view->title_font_ok) ? &view->title_font : font;
  const ColonizeFont* name_font = font;
  /* stats_y = name_y + FONTTINY.max_height + 2 (3f41:02dc, see the
   * REPORTS_INDIAN_STATS_DY note) — derived from the live font, not fixed. */
  const int stats_dy = font ? (int)font->max_height + 2 : REPORTS_INDIAN_STATS_DY;

  IndianRow rows[COLONIZE_COL1_INDIAN_COUNT];
  const int n = reports_indian_build_rows(col1, units, human, rows, COLONIZE_COL1_INDIAN_COUNT);

  for (int i = 0; i < n; ++i) {
    const IndianRow* r = &rows[i];
    const int name_y = REPORTS_INDIAN_ROW0_Y + i * REPORTS_INDIAN_ROW_STEP;
    const int stats_y = name_y + stats_dy;

    if (icons) {
      /* The base sprite is #113 (quartile 0), so the floor is
       * REPORTS_INDIAN_ICON_SPRITE, not 114 — the old `>= 114` test sent
       * every calm tribe down the fallback branch, which skipped the
       * sprite_count bound the guard exists to apply. */
      const int icon =
        (r->icon_sprite >= REPORTS_INDIAN_ICON_SPRITE &&
         r->icon_sprite < icons->sprite_count)
          ? r->icon_sprite
          : REPORTS_INDIAN_ICON_SPRITE;
      if (icon < icons->sprite_count) {
        ss_blit_sprite(
          icons, icon, fb, REPORTS_INDIAN_ICON_X, name_y + REPORTS_INDIAN_ICON_DY
        );
      }
    }

    char name_buf[40];
    if (r->extinct) {
      /* DOS appends DS:0x2ebe (@MISC #130 "Extinct") into the same name
       * buffer right after FUN_281f_01be's colon, then the
       * `TEST [0x8d4e+3],0x80 / JMP LAB_3f41_04e7` skips level + stats. */
      const char* extinct_w = reports_labels_field("MISC", 130);
      snprintf(name_buf, sizeof(name_buf), "%s: %s", r->name, extinct_w ? extinct_w : "");
      reports_draw_line_shadowed(name_font, fb, REPORTS_INDIAN_NAME_X, name_y, name_buf, r->color);
      continue;
    }
    snprintf(name_buf, sizeof(name_buf), "%s:", r->name);
    reports_draw_line_shadowed(name_font, fb, REPORTS_INDIAN_NAME_X, name_y, name_buf, r->color);
    reports_draw_right_shadowed(
      name_font, fb, REPORTS_INDIAN_LEVEL_RIGHT, name_y, r->level, r->color
    );

    /*
     * "Missions" (@MISC #28) and "Horse Herds" (@MISC #45) are real
     * LABELS.TXT words, live-resolved 2026-08-27. "Muskets" is the
     * NAMES.TXT @CARGO name (reports_cargo_name, 2026-08-28). "Villages"
     * alone has no match anywhere in LABELS.TXT (only "Villages Burned"),
     * so it stays hardcoded.
     */
    char buf[32];
    /* DOS-LITERAL FUN_3f41_010a raw 69557-69564: the settlement noun is the
     * tribe's own NAMES.TXT @LEVELS row (indexed by indian.tech) — column 1
     * when the count is exactly 1 ("1 City"), column 2 otherwise ("3 Camps").
     * The port printed a fixed "Villages" for every tribe until 2026-09-21. */
    {
      const char* noun = reports_names_field("LEVELS", r->tech, r->villages == 1 ? 1 : 2);
      snprintf(buf, sizeof(buf), "%d %s", r->villages, noun ? noun : "");
    }
    reports_draw_line(font, fb, REPORTS_INDIAN_VILLAGES_X, stats_y, buf, REPORTS_INDIAN_TEXT_COLOR);
    if (r->missions > 0) {
      const char* missions_w = reports_labels_field("MISC", 28);
      snprintf(buf, sizeof(buf), "%d %s", r->missions, missions_w ? missions_w : "");
      reports_draw_line(font, fb, REPORTS_INDIAN_MISSIONS_X, stats_y, buf, REPORTS_INDIAN_TEXT_COLOR);
    }
    if (r->muskets > 0) {
      snprintf(buf, sizeof(buf), "%d %s", r->muskets, reports_cargo_name(COLONIZE_CARGO_MUSKETS));
      reports_draw_line(font, fb, REPORTS_INDIAN_MUSKETS_X, stats_y, buf, REPORTS_INDIAN_TEXT_COLOR);
    }
    if (r->horse_herds > 0) {
      const char* horses_w = reports_labels_field("MISC", 45);
      snprintf(buf, sizeof(buf), "%d %s", r->horse_herds, horses_w ? horses_w : "");
      reports_draw_line(font, fb, REPORTS_INDIAN_HORSES_X, stats_y, buf, REPORTS_INDIAN_TEXT_COLOR);
    }
  }

  if (n == 0) {
    reports_draw_line(font, fb, 8, REPORTS_INDIAN_ROW0_Y, "No tribes contacted yet.", 14);
  }
}
