/*
 * Sections:
 *  - View lifecycle, asset loading & id/availability lookups (~line 43)
 *  - Shared draw primitives & report-common helpers (~line 236)
 *  - Shadowed line-drawing helpers (~line 658)
 *  - Top-level render dispatch (~line 704)
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

static const char* k_report_files[COLONIZE_REPORT_COUNT] = {
  "REPORT2.PIK", /* Religious Adviser */
  "CCBKGD.PIK",  /* Continental Congress */
  "REPORT4.PIK", /* Labor Adviser */
  "REPORT5.PIK", /* Economic Adviser */
  "REPORT6.PIK", /* Colony Adviser */
  "REPORT7.PIK", /* Naval Adviser */
  "REPORT8.PIK", /* Foreign Affairs */
  "REPORT9.PIK", /* Indian Adviser */
  "WOODPANL.PIK" /* Colonization Score — full-screen wood */
};


/* ===================== View lifecycle, asset loading & id/availability lookups (reports_init .. reports_unavailable_tag) ===================== */
void reports_init(ColonizeReportsView* view) {
  if (!view) {
    return;
  }
  memset(view, 0, sizeof(*view));
  view->active = COLONIZE_REPORT_RELIGIOUS;
}

void reports_free(ColonizeReportsView* view) {
  if (!view) {
    return;
  }
  for (int i = 0; i < COLONIZE_REPORT_COUNT; ++i) {
    pik_free(&view->backgrounds[i]);
  }
  for (int i = 0; i < COLONIZE_REPORT_ICONS_DEST_COUNT; ++i) {
    ss_free(&view->icons[i]);
  }
  ff_free(&view->title_font);
  pik_free(&view->congress_page1_bg);
  pik_free(&view->exploits_bg);
  memset(view, 0, sizeof(*view));
  reports_names_free_catalogs();
}

/*
 * The ICONS.SS instance remapped for one screen's palette (see reports.h).
 * NULL when that screen's copy never loaded — every caller treats NULL as
 * "no icons", which is what view->icons_ok used to mean.
 */
const ColonizeSpriteSheet* reports_icons_for(
  const ColonizeReportsView* view, int dest
) {
  if (!view || dest < 0 || dest >= COLONIZE_REPORT_ICONS_DEST_COUNT || !view->icons_ok[dest]) {
    return NULL;
  }
  return &view->icons[dest];
}

bool reports_load(ColonizeReportsView* view, const char* data_dir, char* err, size_t err_size) {
  if (!view || !data_dir) {
    if (err && err_size) {
      snprintf(err, err_size, "reports_load bad args");
    }
    return false;
  }
  reports_free(view);
  reports_init(view);

  int ok_count = 0;
  for (int i = 0; i < COLONIZE_REPORT_COUNT; ++i) {
    char path[512];
    char pik_err[256];
    if (!dos_compat_normalize_asset_path(data_dir, k_report_files[i], path, sizeof(path))) {
      diag_warn("Report background path failed: %s", k_report_files[i]);
      continue;
    }
    if (!pik_load(path, &view->backgrounds[i], pik_err, sizeof(pik_err))) {
      diag_warn("Failed to load %s: %s", k_report_files[i], pik_err);
      continue;
    }
    view->background_ok[i] = true;
    ok_count++;
  }
  view->loaded = ok_count > 0;
  str_copy_trunc(view->data_dir, sizeof(view->data_dir), data_dir);
  {
    /* WOODPAN2.PIK — Retire exploits screen (FUN_41f2_0b70 loads it by name). */
    char path[512];
    char pik_err[256] = "path build failed";
    if (dos_compat_normalize_asset_path(data_dir, "WOODPAN2.PIK", path, sizeof(path)) &&
        pik_load(path, &view->exploits_bg, pik_err, sizeof(pik_err))) {
      view->exploits_bg_ok = true;
    } else {
      diag_warn("Failed to load WOODPAN2.PIK: %s", pik_err);
    }
  }
  if (!view->loaded) {
    snprintf(err, err_size, "no report backgrounds loaded");
    return false;
  }

  reports_names_load_catalogs(data_dir);


  /* Report titles use FONTTINY, not the FONTSMAL body/menu font (golden:
   * religious.png / labor.png — bolder, wider-spaced glyphs). */
  char font_path[512];
  char font_err[256];
  if (dos_compat_normalize_asset_path(data_dir, "FONTTINY.FF", font_path, sizeof(font_path)) &&
      ff_load(font_path, &view->title_font, font_err, sizeof(font_err))) {
    view->title_font_ok = true;
    /* FONTINTR is deliberately NOT loaded here: the whole 3f41 report overlay
     * touches only the FONTTINY font pointer (DS:0x89e) — see
     * reports_render_indian (bugs.md #428). */
  } else {
    diag_warn("Failed to load FONTTINY.FF for reports: %s", font_err);
  }

  /* Congress page 1's own background (golden: continental_p1.png) — F3's
   * natural REPORT-N slot, orphaned until now (Congress used CCBKGD.PIK,
   * page 2's hall photo, for both pages). */
  char cc_p1_path[512];
  char cc_p1_err[256];
  if (dos_compat_normalize_asset_path(data_dir, "REPORT3.PIK", cc_p1_path, sizeof(cc_p1_path)) &&
      pik_load(cc_p1_path, &view->congress_page1_bg, cc_p1_err, sizeof(cc_p1_err))) {
    view->congress_page1_bg_ok = true;
  } else {
    diag_warn("Failed to load REPORT3.PIK for Congress page 1: %s", cc_p1_err);
  }

  /*
   * ICONS.SS — the game's standard resource-count / unit / job icons, used on
   * nearly every report screen. Loaded once PER DESTINATION SCREEN and
   * remapped to that screen's palette.
   *
   * Why per screen: the remap bakes destination palette indices into the
   * sheet's pixels, so one sheet remapped to a single screen and then blitted
   * over the others resolves some indices to whatever those palettes happen
   * to hold — ICONS index 13 (255,113,0) picks REPORT2 slot 177, which reads
   * (178,73,24) on REPORT3 and (211,130,65) on REPORT4 rather than their own
   * nearest matches. (That was latent, not visible: index 13 is 11 px of the
   * sheet.)
   *
   * Why remap rather than the reserved-DAC-block MERGE used for popup art
   * (crown-europe batch; reports_remap_exploits_sheet for SCORE<nn>.SS):
   * merging is for sheets that ship their own entries for a DAC block the
   * host screen leaves black. ICONS.SS is itself black across 152..251, so it
   * reserves nothing, while REPORT2/3/4 etc. paint their photos in exactly
   * that range — merging would blacken them.
   */
  {
    char ss_path[512];
    char ss_err[256] = "path resolve failed";
    if (dos_compat_normalize_asset_path(data_dir, "ICONS.SS", ss_path, sizeof(ss_path))) {
      for (int d = 0; d < COLONIZE_REPORT_ICONS_DEST_COUNT; ++d) {
        const ColonizePalette* dest_pal = NULL;
        if (d == COLONIZE_REPORT_ICONS_CONGRESS_P1) {
          dest_pal = view->congress_page1_bg_ok ? &view->congress_page1_bg.palette : NULL;
        } else {
          dest_pal = view->background_ok[d] ? &view->backgrounds[d].palette : NULL;
        }
        if (!ss_load(ss_path, &view->icons[d], ss_err, sizeof(ss_err))) {
          diag_warn("Failed to load ICONS.SS for reports: %s", ss_err);
          break;
        }
        if (dest_pal) {
          assets_sheet_remap_to_palette(&view->icons[d], dest_pal);
        }
        view->icons_ok[d] = true;
      }
    } else {
      diag_warn("Failed to load ICONS.SS for reports: %s", ss_err);
    }
  }

  diag_info("Report screens loaded (%d/%d backgrounds)", ok_count, COLONIZE_REPORT_COUNT);
  return true;
}


const char* reports_background_name(ColonizeReportId id) {
  if (id < 0 || id >= COLONIZE_REPORT_COUNT) {
    return "";
  }
  return k_report_files[id];
}

bool reports_id_from_fkey(int fkey_number, ColonizeReportId* out_id) {
  if (fkey_number < 2 || fkey_number > 10 || !out_id) {
    return false;
  }
  *out_id = (ColonizeReportId)(fkey_number - 2);
  return true;
}

/*
 * F8 Foreign Affairs is withdrawn once the War of Independence has begun —
 * FUN_3f41_2548 raw :70792, `*(byte *)0x5382 & 1` = head.game_options.woi.
 * See reports.h for the citation; no other F-key report carries a gate.
 */
bool reports_is_available(ColonizeReportId id, const ColonizeCol1Save* col1) {
  if (id == COLONIZE_REPORT_FOREIGN && col1 && col1->head.game_options.woi) {
    return false;
  }
  return true;
}

const char* reports_unavailable_tag(ColonizeReportId id) {
  return id == COLONIZE_REPORT_FOREIGN ? "FOREIGNNOTAVAIL" : NULL;
}

/* ===================== Shared draw primitives & report-common helpers (reports_draw_line .. reports_draw_icon_bar_pair) ===================== */
void reports_draw_line(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  font_draw_text(font, fb, x, y, text, color);
}

int reports_line_step(const ColonizeFont* font) {
  const int h = font ? (font->max_height + 2) : 8;
  return h < 8 ? 8 : h;
}

static void reports_render_body_start(
  const ColonizeReportsView* view,
  ColonizeReportId id,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* out_y
) {
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (id == COLONIZE_REPORT_CONGRESS) {
    /* Page 1 uses its own REPORT3.PIK background, not CCBKGD.PIK (that's
     * page 2's hall photo) — see reports_render_congress_page1. */
    if (view && view->congress_page1_bg_ok) {
      pik_blit(&view->congress_page1_bg, fb, 0, 0);
    }
  } else if (view && view->background_ok[id]) {
    pik_blit(&view->backgrounds[id], fb, 0, 0);
  }

  const ColonizeFont* title_font =
    (view && view->title_font_ok) ? &view->title_font : font;
  const char* title = reports_title(id);
  const int title_w = title_font ? font_text_width(title_font, title) : 0;
  /* WOODPANL.PIK (F10 Score) doesn't remap palette index 15 to a gold shade
   * the way every other report background does — its index 15 is literal
   * EGA white. Score's title/body ink is index 149 (199,162,32), confirmed
   * against score.png. */
  const uint8_t title_color = (id == COLONIZE_REPORT_SCORE) ? 149 : 15;
  /* Foreign Affairs' title sits ~3px higher in the golden than every other
   * report (measured: foreign.png's title top is native y=2, everyone
   * else's is y=5) — REPORT8.PIK's own baked title position, not a shared
   * font/layout quirk. */
  const int title_y = (id == COLONIZE_REPORT_FOREIGN) ? 2 : 5;
  reports_draw_line(title_font, fb, (fb->width - title_w) / 2, title_y, title, title_color);
  *out_y = 4 + reports_line_step(font) + 4;
}

/* Bottom-right "OK" button (native 320×200 coords), shared by every F2–F9
 * report (golden: religious.png / labor.png; not drawn on F10 Score). */
#define REPORTS_OK_X 286
#define REPORTS_OK_Y 184
#define REPORTS_OK_W 30
#define REPORTS_OK_H 14

bool reports_ok_button_hit(ColonizeReportId id, bool congress_page2, int mx, int my) {
  if (id == COLONIZE_REPORT_SCORE) {
    return false;
  }
  if (id == COLONIZE_REPORT_CONGRESS && congress_page2) {
    return false;
  }
  return mx >= REPORTS_OK_X && mx < REPORTS_OK_X + REPORTS_OK_W && my >= REPORTS_OK_Y &&
    my < REPORTS_OK_Y + REPORTS_OK_H;
}

/* Exclusive-corner outline (x0..x1-1, y0..y1-1). */
static void reports_draw_rect_outline(
  ColonizeFramebuffer8* fb,
  int x0,
  int y0,
  int x1,
  int y1,
  uint8_t color
) {
  fb_rect_outline(fb, x0, y0, x1 - x0, y1 - y0, color, color);
}

static void reports_render_ok_button(const ColonizeFont* font, ColonizeFramebuffer8* fb) {
  reports_draw_rect_outline(
    fb, REPORTS_OK_X, REPORTS_OK_Y, REPORTS_OK_X + REPORTS_OK_W, REPORTS_OK_Y + REPORTS_OK_H, 4
  );
  if (!font) {
    return;
  }
  char ok_w[16];
  reports_misc_word(46, "", ok_w, sizeof(ok_w));
  const int tw = font_text_width(font, ok_w);
  const int tx = REPORTS_OK_X + (REPORTS_OK_W - tw) / 2;
  const int ty = REPORTS_OK_Y + (REPORTS_OK_H - font->max_height) / 2;
  reports_draw_line(font, fb, tx, ty, ok_w, 14);
}

int reports_clamp_nation(int human_nation) {
  if (human_nation < 0 || human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  return human_nation;
}


bool reports_unit_in_europe(int x, int y) {
  return x >= 200 || y >= 200;
}

/* True if (x,y) is one of this nation's own colony tiles — a unit standing
 * there (garrison, e.g.) counts as "In Colonies" for the labor report even
 * though it's a separate col1->unit[] record, not colony population. */
bool reports_xy_is_own_colony(const ColonizeCol1Save* col1, int human, int x, int y) {
  return col1_colony_at_xy(col1, human, x, y) != NULL;
}

int reports_colony_rebel_pct(const ColonizeCol1Colony* c) {
  if (!c || c->rebel_divisor == 0) {
    return 0;
  }
  return (int)((c->rebel_dividend * 100u) / c->rebel_divisor);
}

/* Cross counter (native 320×200 coords; golden religious.png). The bar box
 * is always the full 300px: `needed_crosses` sets the step and
 * `current_crosses` sets how many crosses are actually drawn, so the "fill"
 * is the length of the drawn run, not a scaled width. Geometry lives in
 * reports_draw_dos_icon_bar (FUN_1097_0004/0174) — see its header. Corrected
 * 2026-09-07 (T5.3); the earlier scaled-width spread put the icons at
 * 10,12,14,16,18,20,… where DOS and the golden have 10,12,14,16,19,21,23,26. */
/* REPORTS_CROSS_* moved to reports_internal.h (shared with reports_congress.c). */

static void reports_draw_outlined_number(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t fg_color
) {
  if (!font || !fb || !text) {
    return;
  }
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      font_draw_text(font, fb, x + dx, y + dy, text, 0);
    }
  }
  font_draw_text(font, fb, x, y, text, fg_color);
}

/*
 * Shared resource-count template (colony_screen_draw_resource_count, ported
 * standalone since reports.c has no ColonyScreenView): `amount` copies of
 * one ICONS.SS icon, evenly spread across [x, x+w). When start-to-start
 * spacing collapses to <=1px (icons fully overlapping) — or always_show_number
 * is forced — the count is overlaid as a black-outlined number instead.
 *
 * max_icons (0 = unbounded) caps how many icons are actually drawn without
 * changing the number overlaid on the bar. Bars whose amount is a raw
 * resource total rather than a unit count (Congress liberty bells: a
 * four-digit pool over a ~180px bar) would otherwise blit thousands of
 * fully-overlapping sprites and read as a solid block; DOS draws a fixed
 * number of them instead. See the bells call site for the measurement.
 */
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
) {
  if (!icons || amount <= 0 || w <= 0 || h <= 0) {
    return;
  }
  if (icon < 0 || icon >= icons->sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &icons->sprites[icon];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  int drawn = amount;
  if (max_icons > 0 && drawn > max_icons) {
    drawn = max_icons;
  }
  const int iw = sp->width;
  const int ih = sp->height;
  const int iy = y + (h - ih) / 2;
  int start_step = iw;
  if (drawn == 1) {
    ss_blit_sprite(icons, icon, fb, x + (w - iw) / 2, iy);
  } else if (w <= iw) {
    for (int i = 0; i < drawn; ++i) {
      ss_blit_sprite(icons, icon, fb, x, iy);
    }
    start_step = 0;
  } else {
    const int span = w - iw;
    start_step = span / (drawn - 1);
    for (int i = 0; i < drawn; ++i) {
      ss_blit_sprite(icons, icon, fb, x + (i * span) / (drawn - 1), iy);
    }
  }
  if ((always_show_number || start_step <= 1) && font) {
    char num[12];
    snprintf(num, sizeof(num), "%d", amount);
    reports_draw_outlined_number(font, fb, x + 1, y + (h > 6 ? 1 : 0), num, 15);
  }
}

/*
 * DOS proportional-fill bar, transcribed from `FUN_1097_0004` (geometry) and
 * `FUN_1097_0174` (draw loop) — the routine the report plates reach through
 * the `FUN_281f_0236` thunk (`281f:023b` JMPFs straight at `1097:0174`).
 *
 * The shape is *not* "spread `amount` icons over a proportionally shortened
 * width", which is what `reports_draw_icon_bar` above does. DOS always lays
 * the bar out across the full `w`, derives its step from the **denominator**
 * (`denom` = what a full bar would hold) and then draws only `amount` icons,
 * so the fill is a run length, not a scaled width:
 *
 *   iw    = sprite width                       (`+2` when flags&2; unused here)
 *   step  = clamp((w - iw) / (denom - 1), 1, iw + 1)      (1 when denom < 2)
 *   span  = (denom - 1) * step
 *   shift = smallest s with (span >> s) <= w - iw   ("halve until it fits")
 *   rem   = w - ((span >> shift) + iw)
 *   count = amount >> shift,  den = denom >> shift
 *   per icon: blit at (x, y+1); x += step; acc += rem; while (acc >= den)
 *             { acc -= den; ++x; }                        (Bresenham remainder)
 *
 * With a four-digit denominator (Congress liberty bells, need 1849) `step`
 * collapses to 1 and `shift` reaches 3, so DOS draws 141 bells at a 1–2px
 * pitch. Every bell whose neighbour lands 1px away is completely painted
 * over; only the ~36 that get a 2px gap survive, and of those all that shows
 * is the 2px left edge — the "thin bell mark" on `continental_p1.png` is
 * ICONS.SS #62 clipped by its own neighbour, not a separate 2×7 glyph.
 *
 * Verified call sites (`viceroy_unpacked.asm`, DOS sprite ids are 1-based
 * over ICONS.SS so 0x39 -> #56 and 0x3f -> #62):
 *   F2 crosses  `3f41:0670` — x=10, y=25, w=300, min_w=0, split=0, flags=1,
 *               sprite 0x39, BX = nation+0x2e (current), DX = +0x30 (needed)
 *   F3 bells    `3f41:0890` — x=4, y=25+font_h+2, w=300, min_w=0, split=0,
 *               flags=1, sprite 0x3f, BX = min(pool, need), DX = need
 * Both pass min_w = 0 (so no `x += rem/2` centring) and split = 0 (so the
 * second, hardcoded-sprite-0x38 overlay pass in `1097:0228` never fires);
 * neither is implemented here for want of a call site to check it against.
 *
 * `flags & 1` (the Bresenham remainder) is set at both sites. The number
 * overlay follows `1097:028e`: DOS shows it when its global numbers toggle
 * (DS:0x70) is on *or* when `step == 1 && amount > 1` — the "icons fused
 * into an unreadable smear" override. DS:0x70 is a colony-screen toggle the
 * report plates do not carry, so only the override is reproduced: it is what
 * puts "1135" on the golden bells bar and leaves the golden crosses bar
 * (step 2) bare.
 */
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
) {
  /* FUN_1097_0004 bails on a zero count or zero denominator. */
  if (!icons || !fb || amount <= 0 || denom <= 0 || w <= 0) {
    return;
  }
  if (icon < 0 || icon >= icons->sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &icons->sprites[icon];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  const int iw = sp->width;
  const int avail = w - iw;
  if (avail < 0) {
    return;
  }
  int step = 1;
  if (denom > 1) {
    step = avail / (denom - 1);
    if (step > iw + 1) {
      step = iw + 1;
    }
    if (step < 1) {
      step = 1;
    }
  }
  const int span = (denom - 1) * step;
  int shift = 0;
  while (shift < 15 && (span >> shift) > avail) {
    ++shift;
  }
  const int total = (span >> shift) + iw;
  const int rem = ((min_w - 1) > total ? min_w : w) - total;
  int px = x;
  if (min_w != 0) {
    px += rem >> 1;
  }
  const int start_x = px;
  const int count = amount >> shift;
  const int den = denom >> shift;
  int acc = 0;
  for (int i = 0; i < count; ++i) {
    ss_blit_sprite(icons, icon, fb, px, y + 1);
    px += step;
    if (den > 0) {
      acc += rem;
      while (acc >= den) {
        acc -= den;
        ++px;
      }
    }
  }
  if (font && fb->pixels && step == 1 && amount > 1) {
    /*
     * `1097:02ab`: FUN_1097_00de(amount, start_x + 2, y, 15, 1). That helper
     * bumps its y by 2 on entry (`1097:00eb`), then — because the flag
     * argument is 1 — paints a plate of (text_width + 1) x 7 at (x, y + 2)
     * (`1097:011e`, BX = width - 1 + 2, colour arg 0) and finally draws the
     * glyphs one pixel further down-right (`1097:0160`: AX = x + 1,
     * DX = y + 1). So: plate at (start_x + 2, y + 2), text at
     * (start_x + 3, y + 3). Golden-confirmed against the "1135" on
     * continental_p1.png (white ink from x=8, rows 36..40, over a black
     * plate spanning rows 35..41, for a bar at x=4, y=33).
     */
    char num[12];
    snprintf(num, sizeof(num), "%d", amount);
    const int tw = font_text_width(font, num);
    const int plate_x = start_x + 2;
    const int plate_y = y + 2;
    for (int yy = plate_y; yy < plate_y + 7; ++yy) {
      if (yy < 0 || yy >= fb->height) {
        continue;
      }
      for (int xx = plate_x; xx < plate_x + tw + 1; ++xx) {
        if (xx < 0 || xx >= fb->width) {
          continue;
        }
        fb->pixels[yy * fb->width + xx] = 0;
      }
    }
    font_draw_text(font, fb, plate_x + 1, plate_y + 1, num, 15);
  }
}

/*
 * Two-icon variant (golden: continental_p1.png rebel/tory bar — flags then
 * crowns, back to back, stretched evenly across the shared width; no gap by
 * construction). Mirrors colony_screen_draw_resource_count_pair.
 */
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
) {
  if (!icons || w <= 0 || h <= 0) {
    return;
  }
  if (amount0 < 0) {
    amount0 = 0;
  }
  if (amount1 < 0) {
    amount1 = 0;
  }
  const int amount = amount0 + amount1;
  if (amount <= 0) {
    return;
  }
  const int first_icon = amount0 > 0 ? icon0 : icon1;
  if (first_icon < 0 || first_icon >= icons->sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &icons->sprites[first_icon];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  const int iw = sp->width;
  const int ih = sp->height;
  const int iy = y + (h - ih) / 2;
  if (amount == 1) {
    ss_blit_sprite(icons, first_icon, fb, x + (w - iw) / 2, iy);
  } else if (w <= iw) {
    for (int i = 0; i < amount; ++i) {
      ss_blit_sprite(icons, (i < amount0) ? icon0 : icon1, fb, x, iy);
    }
  } else {
    const int span = w - iw;
    for (int i = 0; i < amount; ++i) {
      const int icon = (i < amount0) ? icon0 : icon1;
      ss_blit_sprite(icons, icon, fb, x + (i * span) / (amount - 1), iy);
    }
  }
}

/* Exclusive-end lines (x0..x1-1 / y0..y1-1). */
/* ===================== Shadowed line-drawing helpers (reports_draw_hline .. reports_draw_right_shadowed) ===================== */
void reports_draw_hline(ColonizeFramebuffer8* fb, int x0, int x1, int y, uint8_t color) {
  if (x1 > x0) {
    fb_hline(fb, y, x0, x1 - 1, color);
  }
}

void reports_draw_vline(ColonizeFramebuffer8* fb, int x, int y0, int y1, uint8_t color) {
  if (y1 > y0) {
    fb_vline(fb, x, y0, y1 - 1, color);
  }
}

/* Right-aligned text ending at right_x. */
void reports_draw_right(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int right_x,
  int y,
  const char* text,
  uint8_t color
) {
  const int w = font ? font_text_width(font, text) : 0;
  reports_draw_line(font, fb, right_x - w, y, text, color);
}

/* Black drop shadow, offset +0,+1 (straight down, no horizontal shift —
 * a +1,+1 diagonal offset was tried first and looked odd, player-reported)
 * — Indian's tribe name/level line (indian.png shows a 1px black trailing
 * edge). Same two-pass idea as popup_draw_text_shadowed, kept local since
 * that one calls font_draw_text_unbold and every other report draws with
 * reports_draw_line's bold font_draw_text. */
void reports_draw_line_shadowed(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, int x, int y, const char* text, uint8_t color
) {
  reports_draw_line(font, fb, x, y + 1, text, 0);
  reports_draw_line(font, fb, x, y, text, color);
}

void reports_draw_right_shadowed(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, int right_x, int y, const char* text, uint8_t color
) {
  const int w = font ? font_text_width(font, text) : 0;
  reports_draw_line_shadowed(font, fb, right_x - w, y, text, color);
}

/* ===================== Top-level render dispatch (reports_render_w) ===================== */
void reports_render_w(
  const ColonizeWorld* w,
  const ColonizeReportsView* view,
  ColonizeReportId id,
  bool congress_page2,
  int labor_detail_job,
  int economic_page,
  int colony_page,
  int naval_page,
  int human_nation,
  int cursor_x,
  int cursor_y,
  uint32_t turn_number,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeUnitPool* units = w->units;
  const ColonizeWorldMap* map = w->map;
  const EuropeScreen* europe = w->europe;
  const ColonizeCol1Save* col1 = w->col1;

  (void)map;
  (void)cursor_x;
  (void)cursor_y;
  if (!framebuffer || !framebuffer->pixels) {
    return;
  }
  if (id < 0 || id >= COLONIZE_REPORT_COUNT) {
    id = COLONIZE_REPORT_RELIGIOUS;
  }

  /*
   * DOS FUN_3f41_2548 returns before loading its plate when the WoI bit is
   * set, so nothing of F8 is drawn after independence (the caller pops
   * @FOREIGNNOTAVAIL instead — see reports_is_available). Kept here too so a
   * stale report_id can never render the withdrawn screen.
   */
  if (!reports_is_available(id, col1)) {
    return;
  }

  const int human = reports_clamp_nation(human_nation);

  /* Congress page 2: full-bleed hall photo, no title/text/OK chrome at all. */
  if (id == COLONIZE_REPORT_CONGRESS && congress_page2) {
    reports_render_congress_page2(view, col1, human, framebuffer);
    return;
  }

  int y = 0;
  char line[160];
  reports_render_body_start(view, id, font, framebuffer, &y);

  switch (id) {
    case COLONIZE_REPORT_RELIGIOUS:
      reports_render_religious(view, col1, human, font, framebuffer);
      break;
    case COLONIZE_REPORT_CONGRESS:
      reports_render_congress_page1(view, col1, human, font, framebuffer, line, sizeof(line));
      break;
    case COLONIZE_REPORT_LABOR:
      if (labor_detail_job >= 0) {
        reports_render_labor_detail(
          view, col1, human, colonies, font, framebuffer, y, labor_detail_job, line, sizeof(line)
        );
      } else {
        reports_render_labor_grid(view, col1, human, colonies, font, framebuffer, y);
      }
      break;
    case COLONIZE_REPORT_ECONOMIC:
      if (economic_page <= 0) {
        reports_render_economic_trade(view, col1, human, europe, font, framebuffer, y, line, sizeof(line));
      } else {
        reports_render_economic_cargo(
          view, col1, human, font, framebuffer, y, economic_page - 1, line, sizeof(line)
        );
      }
      break;
    case COLONIZE_REPORT_COLONY: {
      const int total_pages = reports_colony_page_count(col1, human);
      const int garrison_pages = total_pages / 2;
      if (colony_page < garrison_pages) {
        reports_render_colony_garrisons(
          view, col1, human, units, colonies, font, framebuffer, y, colony_page, line, sizeof(line)
        );
      } else {
        reports_render_colony_sol(
          view, col1, human, colonies, font, framebuffer, y, colony_page - garrison_pages, line,
          sizeof(line)
        );
      }
      break;
    }
    case COLONIZE_REPORT_NAVAL:
      reports_render_naval(view, human, units, colonies, europe, font, framebuffer, naval_page);
      break;
    case COLONIZE_REPORT_FOREIGN:
      reports_render_foreign(view, col1, human, font, framebuffer);
      break;
    case COLONIZE_REPORT_INDIAN:
      reports_render_indian(view, col1, units, human, font, framebuffer);
      break;
    case COLONIZE_REPORT_SCORE:
      reports_render_score(
        view,
        col1,
        human,
        colonies,
        europe,
        turn_number,
        font,
        framebuffer,
        line,
        sizeof(line)
      );
      break;
    default:
      break;
  }

  if (id != COLONIZE_REPORT_SCORE) {
    reports_render_ok_button(font, framebuffer);
  }
}

