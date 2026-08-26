#include "core/colony_production.h"
#include "core/combat_strength.h"
#include "core/founding_fathers.h"
#include "core/reports.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

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

static const char* k_report_titles[COLONIZE_REPORT_COUNT] = {
  "RELIGIOUS ADVISER REPORT",
  "CONTINENTAL CONGRESS ACTIVITIES",
  "LABOR ADVISER REPORT",
  "ECONOMIC ADVISER REPORT",
  "COLONY ADVISER REPORT",
  "NAVAL ADVISER REPORT",
  "FOREIGN AFFAIRS REPORT",
  "INDIAN ADVISER REPORT",
  "COLONIZATION SCORE"
};

/* NAMES.TXT @FATHERS order. */
static const char* k_ff_names[COLONIZE_COL1_FF_COUNT] = {
  "Adam Smith",
  "Jakob Fugger",
  "Peter Minuit",
  "Peter Stuyvesant",
  "Jan de Witt",
  "Ferdinand Magellan",
  "Francisco Coronado",
  "Hernando de Soto",
  "Henry Hudson",
  "Sieur De La Salle",
  "Hernan Cortes",
  "George Washington",
  "Paul Revere",
  "Francis Drake",
  "John Paul Jones",
  "Thomas Jefferson",
  "Pocahontas",
  "Thomas Paine",
  "Simon Bolivar",
  "Benjamin Franklin",
  "William Brewster",
  "William Penn",
  "Jean de Brebeuf",
  "Juan de Sepulveda",
  "Bartolome de las Casas"
};

/* NAMES.TXT @JOB column 2 (recruit / specialty display names). */
static const char* k_job_names[] = {
  "Expert Farmers",
  "Master Sugar Planters",
  "Master Tobacco Planters",
  "Master Cotton Planters",
  "Expert Fur Trappers",
  "Expert Lumberjacks",
  "Expert Ore Miners",
  "Expert Silver Miners",
  "Expert Fishermen",
  "Master Distiller",
  "Master Tobacconists",
  "Master Weavers",
  "Master Fur Traders",
  "Master Carpenters",
  "Master Blacksmiths",
  "Master Gunsmiths",
  "Firebrand Preachers",
  "Elder Statesmen",
  "Expert Teachers",
  "Free Colonists",
  "Hardy Pioneers",
  "Veteran Soldiers",
  "Seasoned Scouts",
  "Veteran Dragoons",
  "Jesuit Missionaries",
  "Indentured Servants",
  "Petty Criminals",
  "Indian Converts"
};
static const int k_job_count = (int)(sizeof(k_job_names) / sizeof(k_job_names[0]));

static const char* k_cargo_names[COLONIZE_COL1_CARGO_TYPES] = {
  "Food",
  "Sugar",
  "Tobacco",
  "Cotton",
  "Furs",
  "Lumber",
  "Ore",
  "Silver",
  "Horses",
  "Rum",
  "Cigars",
  "Cloth",
  "Coats",
  "Trade Goods",
  "Tools",
  "Muskets"
};

/* NAMES.TXT @TRIBES column 0 (plural display name — golden: indian.png
 * "Arawaks:" / "Cherokee:"). Only Inca/Aztec/Arawak actually change in
 * plural; the rest are already the same word. */
static const char* k_tribe_names[COLONIZE_COL1_INDIAN_COUNT] = {
  "Incas", "Aztecs", "Arawaks", "Iroquois", "Cherokee", "Apache", "Sioux", "Tupi"
};

static const char* k_tribe_levels[] = {"Semi-Nomadic", "Agrarian", "Advanced", "Civilized"};

static const char* k_euro_short[COLONIZE_COL1_NATION_COUNT] = {
  "English", "French", "Spanish", "Dutch"
};

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
  ss_free(&view->icons);
  ff_free(&view->title_font);
  pik_free(&view->congress_page1_bg);
  memset(view, 0, sizeof(*view));
}

/* Nearest-color remap of a sprite sheet's own palette onto dst_pal (see
 * colony_screen.c / europe.c: same per-file pattern, no shared header). */
static void reports_remap_sheet_to_palette(
  ColonizeSpriteSheet* sheet,
  const ColonizePalette* dst_pal
) {
  if (!sheet || !dst_pal || !sheet->has_palette) {
    return;
  }
  uint8_t lut[256];
  for (int i = 0; i < 256; ++i) {
    if (i == COLONIZE_SS_TRANSPARENT) {
      lut[i] = (uint8_t)COLONIZE_SS_TRANSPARENT;
      continue;
    }
    const int sr = sheet->palette.rgb[i][0];
    const int sg = sheet->palette.rgb[i][1];
    const int sb = sheet->palette.rgb[i][2];
    int best = 0;
    int best_d = 1 << 30;
    for (int j = 0; j < 256; ++j) {
      const int dr = sr - dst_pal->rgb[j][0];
      const int dg = sg - dst_pal->rgb[j][1];
      const int db = sb - dst_pal->rgb[j][2];
      const int d = dr * dr + dg * dg + db * db;
      if (d < best_d) {
        best_d = d;
        best = j;
      }
    }
    lut[i] = (uint8_t)best;
  }
  for (int s = 0; s < sheet->sprite_count; ++s) {
    ColonizeSprite* spr = &sheet->sprites[s];
    if (!spr->pixels) {
      continue;
    }
    const int n = spr->width * spr->height;
    for (int p = 0; p < n; ++p) {
      spr->pixels[p] = lut[spr->pixels[p]];
    }
  }
  sheet->palette = *dst_pal;
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
  if (!view->loaded) {
    snprintf(err, err_size, "no report backgrounds loaded");
    return false;
  }

  /* Cross counter (Religious report) reuses the game's standard resource-count
   * icon (ICONS.SS #56), remapped to REPORT2.PIK's palette. */
  char ss_path[512];
  char ss_err[256];
  if (dos_compat_normalize_asset_path(data_dir, "ICONS.SS", ss_path, sizeof(ss_path)) &&
      ss_load(ss_path, &view->icons, ss_err, sizeof(ss_err))) {
    if (view->background_ok[COLONIZE_REPORT_RELIGIOUS]) {
      reports_remap_sheet_to_palette(
        &view->icons, &view->backgrounds[COLONIZE_REPORT_RELIGIOUS].palette
      );
    }
    view->icons_ok = true;
  } else {
    diag_warn("Failed to load ICONS.SS for reports: %s", ss_err);
  }

  /* Report titles use FONTTINY, not the FONTSMAL body/menu font (golden:
   * religious.png / labor.png — bolder, wider-spaced glyphs). */
  char font_path[512];
  char font_err[256];
  if (dos_compat_normalize_asset_path(data_dir, "FONTTINY.FF", font_path, sizeof(font_path)) &&
      ff_load(font_path, &view->title_font, font_err, sizeof(font_err))) {
    view->title_font_ok = true;
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

  diag_info("Report screens loaded (%d/%d backgrounds)", ok_count, COLONIZE_REPORT_COUNT);
  return true;
}

const char* reports_title(ColonizeReportId id) {
  if (id < 0 || id >= COLONIZE_REPORT_COUNT) {
    return "REPORT";
  }
  return k_report_titles[id];
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

static void reports_draw_line(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  font_draw_text(font, fb, x, y, text, color);
}

static int reports_line_step(const ColonizeFont* font) {
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

static void reports_draw_rect_outline(
  ColonizeFramebuffer8* fb,
  int x0,
  int y0,
  int x1,
  int y1,
  uint8_t color
) {
  if (!fb || !fb->pixels) {
    return;
  }
  for (int x = x0; x < x1; ++x) {
    if (x >= 0 && x < fb->width) {
      if (y0 >= 0 && y0 < fb->height) {
        fb->pixels[y0 * fb->width + x] = color;
      }
      if (y1 - 1 >= 0 && y1 - 1 < fb->height) {
        fb->pixels[(y1 - 1) * fb->width + x] = color;
      }
    }
  }
  for (int y = y0; y < y1; ++y) {
    if (y >= 0 && y < fb->height) {
      if (x0 >= 0 && x0 < fb->width) {
        fb->pixels[y * fb->width + x0] = color;
      }
      if (x1 - 1 >= 0 && x1 - 1 < fb->width) {
        fb->pixels[y * fb->width + x1 - 1] = color;
      }
    }
  }
}

static void reports_render_ok_button(const ColonizeFont* font, ColonizeFramebuffer8* fb) {
  reports_draw_rect_outline(
    fb, REPORTS_OK_X, REPORTS_OK_Y, REPORTS_OK_X + REPORTS_OK_W, REPORTS_OK_Y + REPORTS_OK_H, 4
  );
  if (!font) {
    return;
  }
  const int tw = font_text_width(font, "OK");
  const int tx = REPORTS_OK_X + (REPORTS_OK_W - tw) / 2;
  const int ty = REPORTS_OK_Y + (REPORTS_OK_H - font->max_height) / 2;
  reports_draw_line(font, fb, tx, ty, "OK", 14);
}

static int reports_clamp_nation(int human_nation) {
  if (human_nation < 0 || human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  return human_nation;
}

static const char* reports_job_name(int job) {
  if (job < 0 || job >= k_job_count) {
    return "Colonist";
  }
  return k_job_names[job];
}

static const char* reports_ff_name(int idx) {
  if (idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT) {
    return "(none)";
  }
  return k_ff_names[idx];
}

/*
 * head.founding_father[i] (-1 unclaimed, else 0..3) is NOT "does this
 * nation have FF i" — nation.founding_fathers[4] is the real per-nation
 * bitmask (bit i set = elected). Confirmed against dutch-reports.SAV: the
 * Dutch bitmask's 10 set bits are exactly the golden's 10 names (including
 * Franklin/Brewster, whose head.founding_father[] entry reads nation 2 —
 * apparently FFs aren't nation-exclusive the way that array alone implies).
 */
static bool reports_ff_owned_by_nation(const ColonizeCol1Nation* nat, int ff_index) {
  if (!nat || ff_index < 0 || ff_index >= (int)COLONIZE_COL1_FF_COUNT) {
    return false;
  }
  return (nat->founding_fathers[ff_index / 8] >> (ff_index % 8)) & 1;
}

static const char* reports_nation_adjective(int nation) {
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return "";
  }
  return k_euro_short[nation];
}

static const char* reports_tribe_level(uint8_t tech) {
  if (tech > 3) {
    tech = 3;
  }
  return k_tribe_levels[tech];
}

static bool reports_unit_in_europe(int x, int y) {
  return x >= 200 || y >= 200;
}

/* True if (x,y) is one of this nation's own colony tiles — a unit standing
 * there (garrison, e.g.) counts as "In Colonies" for the labor report even
 * though it's a separate col1->unit[] record, not colony population. */
static bool reports_xy_is_own_colony(const ColonizeCol1Save* col1, int human, int x, int y) {
  if (!col1) {
    return false;
  }
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &col1->colony[i];
    if (c->nation_id == (uint8_t)human && c->x == (uint8_t)x && c->y == (uint8_t)y) {
      return true;
    }
  }
  return false;
}

static int reports_colony_rebel_pct(const ColonizeCol1Colony* c) {
  if (!c || c->rebel_divisor == 0) {
    return 0;
  }
  return (int)((c->rebel_dividend * 100u) / c->rebel_divisor);
}

/* Cross counter (native 320×200 coords; golden religious.png). A real
 * progress bar: full width (needed_crosses, rarely seen — resets on
 * threshold) spans left margin to right margin; current width
 * (current_crosses) is that scaled by current/needed. Crosses pack evenly
 * across the scaled width — the game's standard resource-count template
 * (colony_screen_draw_resource_count), reused here with its own copy since
 * reports.c has no ColonyScreenView. */
#define REPORTS_CROSS_ICON 56 /* ICONS.SS #56 */
#define REPORTS_CROSS_X 10
#define REPORTS_CROSS_Y 27
#define REPORTS_CROSS_RIGHT_MARGIN 10 /* assumed symmetric with left; never seen at 100% fill */
#define REPORTS_CROSS_MAX_W (320 - REPORTS_CROSS_X - REPORTS_CROSS_RIGHT_MARGIN)
#define REPORTS_CROSS_H 11

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
 */
static void reports_draw_icon_bar(
  const ColonizeReportsView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int icon,
  int x,
  int y,
  int w,
  int h,
  int amount,
  bool always_show_number
) {
  if (!view || !view->icons_ok || amount <= 0 || w <= 0 || h <= 0) {
    return;
  }
  if (icon < 0 || icon >= view->icons.sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &view->icons.sprites[icon];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  const int iw = sp->width;
  const int ih = sp->height;
  const int iy = y + (h - ih) / 2;
  int start_step = iw;
  if (amount == 1) {
    ss_blit_sprite(&view->icons, icon, fb, x + (w - iw) / 2, iy);
  } else if (w <= iw) {
    for (int i = 0; i < amount; ++i) {
      ss_blit_sprite(&view->icons, icon, fb, x, iy);
    }
    start_step = 0;
  } else {
    const int span = w - iw;
    start_step = span / (amount - 1);
    for (int i = 0; i < amount; ++i) {
      ss_blit_sprite(&view->icons, icon, fb, x + (i * span) / (amount - 1), iy);
    }
  }
  if ((always_show_number || start_step <= 1) && font) {
    char num[12];
    snprintf(num, sizeof(num), "%d", amount);
    reports_draw_outlined_number(font, fb, x + 1, y + (h > 6 ? 1 : 0), num, 15);
  }
}

/*
 * Two-icon variant (golden: continental_p1.png rebel/tory bar — flags then
 * crowns, back to back, stretched evenly across the shared width; no gap by
 * construction). Mirrors colony_screen_draw_resource_count_pair.
 */
static void reports_draw_icon_bar_pair(
  const ColonizeReportsView* view,
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
  if (!view || !view->icons_ok || w <= 0 || h <= 0) {
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
  if (first_icon < 0 || first_icon >= view->icons.sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &view->icons.sprites[first_icon];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  const int iw = sp->width;
  const int ih = sp->height;
  const int iy = y + (h - ih) / 2;
  if (amount == 1) {
    ss_blit_sprite(&view->icons, first_icon, fb, x + (w - iw) / 2, iy);
  } else if (w <= iw) {
    for (int i = 0; i < amount; ++i) {
      ss_blit_sprite(&view->icons, (i < amount0) ? icon0 : icon1, fb, x, iy);
    }
  } else {
    const int span = w - iw;
    for (int i = 0; i < amount; ++i) {
      const int icon = (i < amount0) ? icon0 : icon1;
      ss_blit_sprite(&view->icons, icon, fb, x + (i * span) / (amount - 1), iy);
    }
  }
}

static void reports_render_religious(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
) {
  if (!col1) {
    return;
  }
  const ColonizeCol1Nation* nat = &col1->nation[human];
  const unsigned needed = nat->needed_crosses;
  const unsigned current = nat->current_crosses;
  if (needed == 0) {
    return;
  }
  int w = (int)(((uint32_t)REPORTS_CROSS_MAX_W * current) / needed);
  if (w > REPORTS_CROSS_MAX_W) {
    w = REPORTS_CROSS_MAX_W;
  }
  reports_draw_icon_bar(
    view, font, fb, REPORTS_CROSS_ICON, REPORTS_CROSS_X, REPORTS_CROSS_Y, w, REPORTS_CROSS_H,
    (int)current, false
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

#define REPORTS_CONGRESS_TEXT1_Y 25 /* "Next Continental Congress Session: (...)" */
#define REPORTS_CONGRESS_BELLS_X 6
#define REPORTS_CONGRESS_BELLS_Y 36
#define REPORTS_CONGRESS_BELLS_RIGHT_MARGIN 20 /* measured; pool rarely reaches need, like crosses */
#define REPORTS_CONGRESS_BELLS_MAX_W (320 - REPORTS_CONGRESS_BELLS_X - REPORTS_CONGRESS_BELLS_RIGHT_MARGIN)
#define REPORTS_CONGRESS_BELLS_H 10

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

static void reports_render_congress_page1(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* body_font,
  ColonizeFramebuffer8* fb,
  char* line,
  size_t line_sz
) {
  /* Golden text is compact (e.g. "Peter Stuyvesant" fits a 78px column) —
   * FONTTINY, like report titles, not the FONTSMAL other reports' bodies use. */
  const ColonizeFont* font = (view && view->title_font_ok) ? &view->title_font : body_font;
  if (!col1) {
    reports_draw_line(font, fb, 8, REPORTS_CONGRESS_TEXT1_Y, "(no Col1 save loaded)", 14);
    return;
  }

  const ColonizeCol1Nation* nat = &col1->nation[human];
  const int step = reports_line_step(font);

  snprintf(
    line,
    line_sz,
    "Next Continental Congress Session:  (%s)",
    nat->next_founding_father >= 0 ? reports_ff_name(nat->next_founding_father) : "none"
  );
  reports_draw_line(font, fb, 8, REPORTS_CONGRESS_TEXT1_Y, line, 15);

  {
    const unsigned pool = founding_fathers_bells_since_last_elect(human);
    const unsigned need = founding_fathers_bells_needed(col1, human);
    if (need > 0 && pool > 0) {
      int w = (int)(((uint32_t)REPORTS_CONGRESS_BELLS_MAX_W * pool) / need);
      if (w > REPORTS_CONGRESS_BELLS_MAX_W) {
        w = REPORTS_CONGRESS_BELLS_MAX_W;
      }
      if (w < 1) {
        w = 1; /* pool>0 must still show something (at least the number overlay) */
      }
      reports_draw_icon_bar(
        view,
        font,
        fb,
        REPORTS_CONGRESS_BELL_ICON,
        REPORTS_CONGRESS_BELLS_X,
        REPORTS_CONGRESS_BELLS_Y,
        w,
        REPORTS_CONGRESS_BELLS_H,
        (int)pool,
        true /* golden always shows the pool number, e.g. "1135" */
      );
    }
  }

  const unsigned rebel_pct = nat->rebel_sentiment > 100 ? 100 : nat->rebel_sentiment;
  snprintf(
    line, line_sz, "Rebel Sentiment: %u%%  Tory Sentiment: %u%%", rebel_pct, 100 - rebel_pct
  );
  reports_draw_line(font, fb, 8, REPORTS_CONGRESS_TEXT2_Y, line, 15);

  {
    const int flags = (int)((REPORTS_CONGRESS_SENT_SLOTS * rebel_pct + 50) / 100);
    const int crowns = REPORTS_CONGRESS_SENT_SLOTS - flags;
    reports_draw_icon_bar_pair(
      view,
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

  snprintf(line, line_sz, "%s Expeditionary Force:", reports_nation_adjective(human));
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
    for (int i = 0; i < 4; ++i) {
      const int amount = (int)col1->head.expeditionary_force[kForceIndex[i]];
      if (amount <= 0) {
        continue;
      }
      const int w = (amount * REPORTS_CONGRESS_FORCE_STEP_X10) / 10;
      reports_draw_icon_bar(
        view,
        font,
        fb,
        kForceIcon[i],
        kForceX[i],
        REPORTS_CONGRESS_FORCE_Y,
        w,
        REPORTS_CONGRESS_FORCE_H,
        amount,
        true
      );
    }
  }

  reports_draw_line(font, fb, 8, REPORTS_CONGRESS_FF_HEADER_Y, "Founding Fathers:", 15);
  {
    int shown = 0;
    int y = REPORTS_CONGRESS_FF_HEADER_Y + step;
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      if (!reports_ff_owned_by_nation(nat, i)) {
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
 * Page 2 (golden: continental_p2.png) — CCBKGD.PIK hall, full-bleed group
 * portrait, no title/text/OK chrome. Positions below are template-matched
 * directly off the golden for the 10 Founding Fathers it shows (each CC-xx.SS
 * blitted at native size, no cropping); the other 15 have no known position
 * yet (no second golden to cross-reference) and are skipped rather than guessed.
 */
typedef struct ReportsFfPortraitSlot {
  int8_t ff_index;
  int16_t x;
  int16_t y;
} ReportsFfPortraitSlot;

/*
 * Draw order matters: sprites are photo cutouts with opaque (non-transparent)
 * canvas margins, not clean alpha mattes, so an overlapping later sprite can
 * blank out an earlier one even outside its "person" silhouette. Ordered
 * back-to-front by (y + height) ascending so foreground figures (DeLaSalle,
 * Washington, Franklin, ...) paint over the background row behind them,
 * matching the golden's apparent layering.
 */
static const ReportsFfPortraitSlot k_ff_portrait_slots[] = {
  {20, 268, 38}, /* William Brewster */
  {3, 83, 34},   /* Peter Stuyvesant */
  {17, 2, 22},   /* Thomas Paine */
  {18, 190, 35}, /* Simon Bolivar */
  {2, 49, 39},   /* Peter Minuit */
  {16, 223, 37}, /* Pocahontas */
  {15, 96, 56},  /* Thomas Jefferson */
  {11, 154, 60}, /* George Washington */
  {9, 39, 65},   /* Sieur De La Salle */
  {19, 118, 86}  /* Benjamin Franklin */
};
#define REPORTS_FF_PORTRAIT_SLOT_COUNT \
  (int)(sizeof(k_ff_portrait_slots) / sizeof(k_ff_portrait_slots[0]))

static void reports_render_congress_page2(
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
  const ColonizeCol1Nation* nat = &col1->nation[human];
  for (int i = 0; i < REPORTS_FF_PORTRAIT_SLOT_COUNT; ++i) {
    const ReportsFfPortraitSlot* slot = &k_ff_portrait_slots[i];
    if (!reports_ff_owned_by_nation(nat, slot->ff_index)) {
      continue;
    }
    const ColonizeSpriteSheet* sheet = reports_ff_portrait_sheet(view->data_dir, slot->ff_index);
    if (!sheet) {
      continue;
    }
    ss_blit_sprite(sheet, 0, fb, slot->x, slot->y);
  }
}

/*
 * Labor report (F4, golden: labor.png / labor_detail.png) — a 9-row x
 * 3-column table of profession icon + name + headcount (bottom-left cell
 * empty), plus a per-profession detail view reached by clicking a cell.
 *
 * DOS lays out a fixed 26-slot table, not a straight scan of job ids 0..27:
 * Expert Teachers (18) and Veteran Dragoons (23) never appear, and Free
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
      /* Only @UNIT types 0-5 (Colonists, Soldiers, Pioneers, Missionaries,
       * Dragoons, Scouts — NAMES.TXT @UNIT rows 0-5) are colonist-derived
       * persons the labor report should ever count; everything else (ships
       * 13-18, Artillery, Wagon Train, Treasure, Regulars/Cavalry/Continental
       * Army 6-9) is equipment/vehicles/King's-army units with no colonist
       * behind them, even though some carry a leftover profession byte. */
      if (u->type > 5) {
        continue;
      }
      int job = u->profession;
      if (job < 0 || job >= 64) {
        job = u->type < 64 ? u->type : 0;
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

static void reports_render_labor_grid(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y
) {
  /* Golden shows job names/counts in FONTTINY (mixed case, narrow) like the
   * title and Congress page 1 — not body_font (FONTSMAL, all-caps, wide
   * enough to overlap the next column). */
  const ColonizeFont* body_font = font;
  font = (view && view->title_font_ok) ? &view->title_font : body_font;

  int colony_counts[64], mapboard_counts[64], europe_counts[64], total;
  reports_labor_job_counts(col1, human, colonies, colony_counts, mapboard_counts, europe_counts, &total);
  /* Centered, matching every other report's centered-title convention. */
  if (font) {
    const int w = font_text_width(font, "(Click on item to zoom)");
    reports_draw_line(font, fb, (fb->width - w) / 2, y, "(Click on item to zoom)", 14);
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
      if (view && view->icons_ok && icon >= 0 && icon < view->icons.sprite_count) {
        unit_chrome_blit(
          fb, NULL, &view->icons, icon, cx, cy, UNIT_CHROME_SPRITE_WITH_SHADOW, 0, 0, -1, 0, false,
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

static void reports_render_labor_detail(
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
  if (view && view->icons_ok && icon >= 0 && icon < view->icons.sprite_count) {
    unit_chrome_blit(
      fb, NULL, &view->icons, icon, 4, header_y, UNIT_CHROME_SPRITE_WITH_SHADOW, 0, 0, -1, 0, false,
      false, -1, -1
    );
  }
  snprintf(line, line_sz, "%s: %d", reports_job_name(job), sum);
  reports_draw_line(font, fb, 22, header_y, line, 14);

  const int bd_x_label = 165;
  const int bd_x_value = 295;
  snprintf(line, line_sz, "Off Mapboard (Europe):");
  reports_draw_line(font, fb, bd_x_label, header_y, line, 14);
  snprintf(line, line_sz, "%d", europe_n);
  reports_draw_line(font, fb, bd_x_value, header_y, line, 14);
  snprintf(line, line_sz, "On Mapboard:");
  reports_draw_line(font, fb, bd_x_label, header_y + REPORTS_LABOR_ROW_STEP / 2, line, 14);
  snprintf(line, line_sz, "%d", mapboard_n);
  reports_draw_line(font, fb, bd_x_value, header_y + REPORTS_LABOR_ROW_STEP / 2, line, 14);
  snprintf(line, line_sz, "In Colonies:");
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
      /* Only @UNIT types 0-5 (Colonists, Soldiers, Pioneers, Missionaries,
       * Dragoons, Scouts — NAMES.TXT @UNIT rows 0-5) are colonist-derived
       * persons the labor report should ever count; everything else (ships
       * 13-18, Artillery, Wagon Train, Treasure, Regulars/Cavalry/Continental
       * Army 6-9) is equipment/vehicles/King's-army units with no colonist
       * behind them, even though some carry a leftover profession byte. */
      if (u->type > 5) {
        continue;
      }
      int uj = u->profession;
      if (uj < 0 || uj >= 64) {
        uj = u->type < 64 ? u->type : 0;
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

static void reports_draw_hline(ColonizeFramebuffer8* fb, int x0, int x1, int y, uint8_t color) {
  if (!fb || !fb->pixels || y < 0 || y >= fb->height) {
    return;
  }
  for (int x = x0; x < x1; ++x) {
    if (x >= 0 && x < fb->width) {
      fb->pixels[y * fb->width + x] = color;
    }
  }
}

static void reports_draw_vline(ColonizeFramebuffer8* fb, int x, int y0, int y1, uint8_t color) {
  if (!fb || !fb->pixels || x < 0 || x >= fb->width) {
    return;
  }
  for (int y = y0; y < y1; ++y) {
    if (y >= 0 && y < fb->height) {
      fb->pixels[y * fb->width + x] = color;
    }
  }
}

/* Right-aligned text ending at right_x. */
static void reports_draw_right(
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
static void reports_draw_line_shadowed(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, int x, int y, const char* text, uint8_t color
) {
  reports_draw_line(font, fb, x, y + 1, text, 0);
  reports_draw_line(font, fb, x, y, text, color);
}

static void reports_draw_right_shadowed(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, int right_x, int y, const char* text, uint8_t color
) {
  const int w = font ? font_text_width(font, text) : 0;
  reports_draw_line_shadowed(font, fb, right_x - w, y, text, color);
}

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

static void reports_render_economic_trade(
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
    static const char kSubtitle[] = "European Trade";
    const int w = font_text_width(font, kSubtitle);
    reports_draw_line(font, fb, (fb->width - w) / 2, y - 1, kSubtitle, REPORTS_ECON_LABEL_COLOR);
  }

  reports_draw_right(font, fb, REPORTS_ECON1_TONS_RIGHT, REPORTS_ECON1_HEADER_Y, "Tons", REPORTS_ECON_LABEL_COLOR);
  reports_draw_right(font, fb, REPORTS_ECON1_GOLD_RIGHT, REPORTS_ECON1_HEADER_Y, "Gold", REPORTS_ECON_LABEL_COLOR);
  reports_draw_right(
    font, fb, REPORTS_ECON1_BID_RIGHT, REPORTS_ECON1_HEADER_Y, "Bid Price", REPORTS_ECON_LABEL_COLOR
  );
  reports_draw_right(
    font, fb, REPORTS_ECON1_ASK_RIGHT, REPORTS_ECON1_HEADER_Y, "Ask Price", REPORTS_ECON_LABEL_COLOR
  );

  const int table_bottom = REPORTS_ECON1_ROW0_Y + REPORTS_ECON1_ROWS * REPORTS_ECON1_ROW_STEP;
  for (int i = 0; i <= REPORTS_ECON1_ROWS; ++i) {
    reports_draw_hline(fb, 0, fb->width, REPORTS_ECON1_ROW0_Y + i * REPORTS_ECON1_ROW_STEP, REPORTS_ECON_LINE_COLOR);
  }
  reports_draw_vline(fb, REPORTS_ECON1_DIVIDER_X, REPORTS_ECON1_VLINE_TOP_Y, table_bottom, REPORTS_ECON_LINE_COLOR);

  const ColonizeCol1Nation* nat = col1 ? &col1->nation[human] : NULL;
  for (int c = 0; c < (int)COLONIZE_COL1_CARGO_TYPES; ++c) {
    const int row_top = REPORTS_ECON1_ROW0_Y + c * REPORTS_ECON1_ROW_STEP;
    const int text_y = row_top + 2;
    reports_draw_line(font, fb, REPORTS_ECON1_LABEL_X, text_y, k_cargo_names[c], REPORTS_ECON_LABEL_COLOR);

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
      bid = europe->cargo[c].bid;
      ask = europe->cargo[c].ask;
    } else if (nat) {
      bid = nat->trade.euro_price[c];
      ask = bid + 1;
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

static void reports_render_economic_cargo(
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
  font = (view && view->title_font_ok) ? &view->title_font : font;
  if (font) {
    static const char kSubtitle[] = "Cargo in Port";
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
    if (view && view->icons_ok && icon < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[icon];
      const int col_left = REPORTS_ECON2_DIVIDER_X + c * REPORTS_ECON2_COL_STEP;
      const int icon_x = col_left + (REPORTS_ECON2_COL_STEP - sp->width) / 2;
      ss_blit_sprite(&view->icons, icon, fb, icon_x, REPORTS_ECON2_ICON_Y);
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

/* Shared icon+digit+name sidebar cell, identical on both Colony pages.
 * `colony` is the pool-matched colony for `c` (same index i as col1's
 * colony array — see reports_render_colony_sol's comment), or NULL when
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
  const int icon = reports_colony_fort_icon(c->buildings.fortification);
  if (view && view->icons_ok && icon >= 0 && icon < view->icons.sprite_count) {
    const ColonizePalette* active_palette =
      (view->background_ok[COLONIZE_REPORT_COLONY] && view->backgrounds[COLONIZE_REPORT_COLONY].has_palette)
        ? &view->backgrounds[COLONIZE_REPORT_COLONY].palette
        : NULL;
    colonies_blit_settlement_icon(
      &view->icons, icon, fb, REPORTS_COLONY_ICON_X, row_top - 3, c->nation_id, active_palette
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

static void reports_render_colony_garrisons(
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
  font = (view && view->title_font_ok) ? &view->title_font : font;
  if (font) {
    static const char kSubtitle[] = "Military Garrisons";
    const int w = font_text_width(font, kSubtitle);
    reports_draw_line(font, fb, (fb->width - w) / 2, y - 1, kSubtitle, REPORTS_COLONY_LABEL_COLOR);
  }
  if (!col1) {
    return;
  }

  int shown = 0;
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
    const int row_top = REPORTS_COLONY_ROW0_Y + row * REPORTS_COLONY_ROW_STEP;
    const ColonizeColony* colony =
      (colonies && i < COLONIZE_COLONIES_MAX && i < colonies->colony_count) ? &colonies->colonies[i] : NULL;
    reports_render_colony_sidebar(view, col1, c, colony, font, fb, row_top, line, line_sz);

    /* Units on this colony's own tile, drawn exactly as on the map
     * (allegiance/orders chrome + real map sprite — unit_chrome_blit_unit,
     * same call shape as colony_screen.c's docked-transport row). */
    if (units && view && view->icons_ok) {
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
      int slot = 0;
      for (int u = 0; u < COLONIZE_UNITS_MAX && slot < 8; ++u) {
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
        const int sprite = units_map_sprite(units, unit->id);
        if (sprite < 0) {
          continue;
        }
        const int display_type = units_display_type_index(units, unit->id);
        int orders = unit->orders;
        if (raw_used) {
          for (uint16_t ri = 0; ri < col1->head.unit_count; ++ri) {
            if (raw_used[ri]) {
              continue;
            }
            const ColonizeCol1Unit* ru = &col1->unit[ri];
            if (ru->nation_id == (uint8_t)human && ru->x == c->x && ru->y == c->y &&
                ru->type == (uint8_t)display_type) {
              orders = ru->orders;
              raw_used[ri] = true;
              break;
            }
          }
        }
        const int x = REPORTS_COLONY_UNIT_X + slot * REPORTS_COLONY_UNIT_PITCH;
        unit_chrome_blit_unit_for_palette(
          fb, font, &view->icons, sprite, x, row_top - 3,
          display_type, unit->nation_id, orders, false, false, active_palette
        );
        slot++;
      }
      free(raw_used);
    }
    shown++;
  }
}

static void reports_render_colony_sol(
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
  font = (view && view->title_font_ok) ? &view->title_font : font;
  if (font) {
    static const char kSubtitle[] = "Sons of Liberty";
    const int w = font_text_width(font, kSubtitle);
    reports_draw_line(font, fb, (fb->width - w) / 2, y - 1, kSubtitle, REPORTS_COLONY_LABEL_COLOR);
  }
  if (!col1) {
    return;
  }

  const int town_hall_idx = colonies ? colonies_find_building(colonies, "Town Hall") : -1;
  const bool nation_is_ai =
    human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT && col1->player[human].control != 0;
  const int statesmen_pct =
    founding_fathers_nation_has(col1, human, FF_THOMAS_JEFFERSON) ? 50 : 0;
  const int paine_tax_pct =
    (founding_fathers_nation_has(col1, human, FF_THOMAS_PAINE) &&
     human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT)
      ? (int)col1->nation[human].tax_rate
      : 0;

  int shown = 0;
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
    const int row_top = REPORTS_COLONY_ROW0_Y + row * REPORTS_COLONY_ROW_STEP;
    /* Pool-based colony matched 1:1 by import order (col1_bridge_apply
     * appends colonies in save order, no skipping under
     * COLONIZE_COLONIES_MAX) — needed for both the Bolivar-aware SoL% and
     * the bell-production formula below, same formula turn.c's EOT bells
     * tally uses. */
    const ColonizeColony* colony =
      (colonies && i < COLONIZE_COLONIES_MAX && i < colonies->colony_count) ? &colonies->colonies[i] : NULL;
    reports_render_colony_sidebar(view, col1, c, colony, font, fb, row_top, line, line_sz);

    const int sol_pct = colony ? colony_prod_sol_percent(col1, colony) : reports_colony_rebel_pct(c);
    if (view && view->icons_ok) {
      ss_blit_sprite(&view->icons, REPORTS_COLONY_FLAG_ICON, fb, REPORTS_COLONY_FLAG_X, row_top - 3);
    }
    snprintf(line, line_sz, "%d%%", sol_pct);
    reports_draw_line(font, fb, REPORTS_COLONY_PCT_X, row_top, line, REPORTS_COLONY_LABEL_COLOR);

    /* Printing Press/Newspaper — 2-bit tier bitfield, same popcount
     * convention as fortification; Newspaper implies Press. */
    const unsigned press_bits = c->buildings.printing_press;
    const char* press_label = (press_bits & 2u) ? "Newspaper" : ((press_bits & 1u) ? "Press" : NULL);
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
    if (view && view->icons_ok) {
      ss_blit_sprite(&view->icons, REPORTS_COLONY_BELL_ICON, fb, REPORTS_COLONY_BELL_X, row_top - 3);
    }
    snprintf(line, line_sz, "%d", bells);
    reports_draw_line(font, fb, REPORTS_COLONY_BELL_NUM_X, row_top, line, REPORTS_COLONY_LABEL_COLOR);

    /* Colonists currently working the Town Hall — same drop-shadow icon
     * convention as the Labor report's profession icons
     * (units_job_icon_sprite + unit_chrome_blit's SPRITE_WITH_SHADOW mode). */
    if (colony && town_hall_idx >= 0 && view && view->icons_ok) {
      int slot = 0;
      for (int p = 0; p < colony->colonist_count && slot < REPORTS_COLONY_WORKER_MAX; ++p) {
        const ColonizeColonist* col = &colony->colonists[p];
        if (!col->active || col->building_type != town_hall_idx) {
          continue;
        }
        const int sprite = units_job_icon_sprite(col->profession);
        if (sprite < 0 || sprite >= view->icons.sprite_count) {
          continue;
        }
        const int x = REPORTS_COLONY_WORKER_X + slot * REPORTS_COLONY_WORKER_PITCH;
        unit_chrome_blit(
          fb, NULL, &view->icons, sprite, x, row_top - 3, UNIT_CHROME_SPRITE_WITH_SHADOW, 0, 0, -1, 0,
          false, false, -1, -1
        );
        slot++;
      }
    }
    shown++;
  }
}

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
  const char* pass_label;

  int goods_icon[COLONIZE_UNIT_CARGO_MAX];
  int goods_count;

  char location[40];
  char destination[40];
} NavalRow;

/* Colony name at (x,y) if this nation (or any nation — golden only shows a
 * human colony, but a foreign port would read the same way) has one there;
 * else the raw coordinates, matching the report spec's Location column. */
static void reports_naval_location(
  const ColonizeColonyPool* colonies, int x, int y, char* out, size_t out_sz
) {
  if (colonies) {
    for (int i = 0; i < colonies->colony_count && i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (c->active && c->x == x && c->y == y) {
        snprintf(out, out_sz, "%s", c->name);
        return;
      }
    }
  }
  snprintf(out, out_sz, "(%d, %d)", x, y);
}

static int reports_naval_goods_icon(int cargo_type, int amount) {
  /* Grey vs colored: same "100-per-stack" rule as colony_screen.c's docked-
   * transport hold display (colony_screen_blit_cargo's `partial = amt<100`). */
  const bool grey = amount < 100;
  return (grey ? REPORTS_NAVAL_CARGO_GREY_BASE : REPORTS_NAVAL_CARGO_ICON_BASE) + cargo_type;
}

/* Plural expert-profession label for a passenger row (golden: naval.png's
 * one passenger example reads "Colonists", the @UNIT plural, matching
 * `base_name` for a no-profession Free Colonist) — same identity
 * `units_display_name` exists for elsewhere (singular, combat-log style),
 * but the report needs NAMES.TXT's own plural @JOB expert names
 * ("Hardy Pioneers" etc.) to match that convention instead. Player-
 * reported: a toolless Pioneer-professioned Colonist (dutch-reports.SAV,
 * (44,39)) should read as a Hardy Pioneer, not a generic Colonist, even
 * without tools equipped — profession names the unit regardless of
 * carried equipment. */
static const char* reports_naval_passenger_label(int profession, const char* base_name) {
  switch (profession) {
    case UNITS_JOB_PIONEER:
      return "Hardy Pioneers";
    case UNITS_JOB_SOLDIER:
      return "Veteran Soldiers";
    case UNITS_JOB_SCOUT:
      return "Seasoned Scouts";
    case UNITS_JOB_DRAGOON:
      return "Veteran Dragoons";
    case UNITS_JOB_MISSIONARY:
      return "Jesuit Missionaries";
    default:
      return (base_name && base_name[0]) ? base_name : "Colonists";
  }
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
      r->ship_name = (st && st->name[0]) ? st->name : "Ship";
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX && r->goods_count < COLONIZE_UNIT_CARGO_MAX; ++h) {
        const int amt = u->hold_goods_amount[h];
        const int gtype = u->hold_goods_type[h];
        if (amt > 0 && gtype >= 0 && gtype < (int)COLONIZE_CARGO_COUNT) {
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
    struct {
      const EuropeHarborShip* list;
      int count;
      const char* loc;
      const char* dest;
    } lanes[3] = {
      {europe->harbor, europe->harbor_ships, europe->port_city, ""},
      {europe->expected, europe->expected_ships, "High Seas", europe->port_city},
      {europe->bound, europe->bound_ships, "High Seas", europe->colony_region},
    };
    for (int lane = 0; lane < 3; ++lane) {
      for (int i = 0; i < lanes[lane].count && n < max_rows; ++i) {
        const EuropeHarborShip* s = &lanes[lane].list[i];
        for (int c = 0; c < s->cargo_count && c < EUROPE_SHIP_CARGO_MAX && n < max_rows; ++c) {
          NavalRow* r = &rows[n++];
          memset(r, 0, sizeof(*r));
          r->has_passenger = true;
          const ColonizeUnitType* pt = units ? units_type(units, s->cargo_types[c]) : NULL;
          r->pass_sprite = pt ? pt->icon_sprite : -1;
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
        r->ship_name = (st && st->name[0]) ? st->name : (s->name[0] ? s->name : "Ship");
        for (int h = 0; h < EUROPE_SHIP_CARGO_MAX && r->goods_count < COLONIZE_UNIT_CARGO_MAX; ++h) {
          const int amt = s->hold_goods_amount[h];
          const int gtype = s->hold_goods_type[h];
          if (amt > 0 && gtype >= 0 && gtype < (int)COLONIZE_CARGO_COUNT) {
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

int reports_naval_page_count(
  int human_nation,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe
) {
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

static void reports_render_naval(
  const ColonizeReportsView* view,
  int human,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int page
) {
  /* Body text needs FONTTINY, not FONTSMAL — golden's mixed-case, ~2px/char
   * ship/cargo/location text is far narrower than FONTSMAL renders (which
   * also turned out to be upper-case-only at this size); same pitfall as
   * Congress page 1's body (docs/report_screens.md). */
  font = (view && view->title_font_ok) ? &view->title_font : font;
  reports_naval_draw_centered(
    font, fb, 0, REPORTS_NAVAL_DIV1_X, REPORTS_NAVAL_HEADER_Y, "Ship", REPORTS_NAVAL_HEADER_COLOR
  );
  reports_naval_draw_centered(
    font, fb, REPORTS_NAVAL_DIV1_X, REPORTS_NAVAL_DIV2_X, REPORTS_NAVAL_HEADER_Y, "Cargo",
    REPORTS_NAVAL_HEADER_COLOR
  );
  reports_naval_draw_centered(
    font, fb, REPORTS_NAVAL_DIV2_X, REPORTS_NAVAL_DIV3_X, REPORTS_NAVAL_HEADER_Y, "Location",
    REPORTS_NAVAL_HEADER_COLOR
  );
  reports_naval_draw_centered(
    font, fb, REPORTS_NAVAL_DIV3_X, fb->width, REPORTS_NAVAL_HEADER_Y, "Destination",
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

    if (r->has_ship && view && view->icons_ok && r->ship_sprite >= 0) {
      unit_chrome_blit_unit_for_palette(
        fb, font, &view->icons, r->ship_sprite, REPORTS_NAVAL_SHIP_ICON_X, icon_y,
        r->ship_type, r->ship_nation, r->ship_orders, false, false, active_palette
      );
    }
    if (r->has_ship && r->ship_name) {
      reports_draw_line(font, fb, REPORTS_NAVAL_SHIP_NAME_X, text_y, r->ship_name, REPORTS_NAVAL_TEXT_COLOR);
    }

    if (r->has_passenger) {
      if (view && view->icons_ok && r->pass_sprite >= 0) {
        unit_chrome_blit_unit_for_palette(
          fb, font, &view->icons, r->pass_sprite, REPORTS_NAVAL_CARGO_ICON_X, icon_y,
          r->pass_type, r->pass_nation, r->pass_orders, false, true, active_palette
        );
      }
      if (r->pass_label) {
        reports_draw_line(
          font, fb, REPORTS_NAVAL_CARGO_LABEL_X, text_y, r->pass_label, REPORTS_NAVAL_TEXT_COLOR
        );
      }
    } else if (view && view->icons_ok) {
      for (int g = 0; g < r->goods_count; ++g) {
        const int icon = r->goods_icon[g];
        if (icon >= 0 && icon < view->icons.sprite_count) {
          ss_blit_sprite(&view->icons, icon, fb, REPORTS_NAVAL_CARGO_ICON_X + g * REPORTS_NAVAL_CARGO_ICON_PITCH, icon_y);
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
 * centered "(Withdrawn from New World)" (LABELS.TXT #205) for a nation
 * with player.control==2, or a 2-column grid of "<peer country>: Peace|War"
 * for every OTHER non-withdrawn nation (own-nation and withdrawn peers are
 * skipped — confirmed against the golden: every block lists exactly its 2
 * surviving peers, never the withdrawn Spanish), followed by a
 * "Rebels: N   Tories: N" line.
 *
 * War/Peace: nation[a].euro_relation[b] is the DS -0x77c4 peer byte
 * ai_diplo.h also reads, but that module's AI_DIPLO_WAR=0x01/PEACE=0x02
 * bit *names* don't reproduce this golden's War pairs when applied at
 * face value (ai_diplo.c's own semantics are a structural port of the
 * war/ally state *machine*, not a byte-verified decode of a real DOS
 * save's raw values). Empirically, against dutch-reports.SAV, bit 0x02
 * being set in *either* direction's byte (nation[a].euro_relation[b] or
 * nation[b].euro_relation[a]) exactly identifies every War pair the
 * golden shows (French/Dutch) and excludes every Peace pair — used here
 * as a report-local reading, deliberately not fed back into ai_diplo.h's
 * shared bit constants (that module drives live AI turn processing; this
 * finding is a single-save empirical fit, not a confirmed DOS decode).
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
 */
#define REPORTS_FOREIGN_BLOCK0_Y 10 /* first block's rule (golden: foreign.png hline scan) */
#define REPORTS_FOREIGN_BLOCK_STEP 45 /* divider-to-divider spacing, 4 fixed nation blocks */
#define REPORTS_FOREIGN_LINE_STEP 7 /* FONTTINY line pitch within a block */
#define REPORTS_FOREIGN_HEADER_DY 3 /* header line y = block_top + this */
#define REPORTS_FOREIGN_BODY_DY 17 /* first body line (relation grid / withdrawn) = block_top + this;
   header_dy + 2*LINE_STEP — golden shows one blank line between header and body */
#define REPORTS_FOREIGN_COL1_X 2
#define REPORTS_FOREIGN_COL2_X 80
#define REPORTS_FOREIGN_LEADER_COLOR 146 /* bright yellow (255,243,93) — REPORT8.PIK palette probe */
#define REPORTS_FOREIGN_ADJ_COLOR 97 /* pale cream (247,243,199) — same index as REPORTS_NAVAL_TEXT_COLOR */
#define REPORTS_FOREIGN_LABEL_COLOR 145 /* light yellow (255,255,142) — peer/nation names, Rebels/Tories */
#define REPORTS_FOREIGN_PEACE_COLOR 15 /* white — REPORT8.PIK palette probe */
#define REPORTS_FOREIGN_WAR_COLOR 112 /* red (243,0,0) — same index as REPORTS_ECON_NEG_COLOR */
#define REPORTS_FOREIGN_RULE_COLOR 119 /* dark red (134,0,0) — same index as REPORTS_NAVAL_LINE_COLOR */

/* NAMES.TXT country names (europe.c / map_panel.c / new_game.c share this
 * exact set) — distinct from k_euro_short's nationality adjectives; the
 * golden uses country names ("France", "Netherlands") for peer relations
 * but adjectives ("French", "Dutch") in the leader header line. */
static const char* k_euro_country[COLONIZE_COL1_NATION_COUNT] = {
  "England", "France", "Spain", "Netherlands"
};

typedef struct ForeignRow {
  const char* leader;
  const char* adjective;
  bool withdrawn;
  int peer_nation[COLONIZE_COL1_NATION_COUNT - 1];
  bool peer_war[COLONIZE_COL1_NATION_COUNT - 1];
  int peer_count;
  int rebels;
  int tories;
} ForeignRow;

/* True if euro_relation's War bit (0x02, empirically — see block comment
 * above) is set in either direction between a and b. */
static bool reports_foreign_at_war(const ColonizeCol1Save* col1, int a, int b) {
  if (!col1 || a == b || a < 0 || a >= (int)COLONIZE_COL1_NATION_COUNT || b < 0 ||
      b >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }
  const uint8_t ab = col1->nation[a].euro_relation[b];
  const uint8_t ba = col1->nation[b].euro_relation[a];
  return ((ab | ba) & 0x02u) != 0;
}

/* Builds one row per Euro nation, fixed English/French/Spanish/Dutch order.
 * Shared shape with reports_naval_build_rows even though this report never
 * paginates (always exactly COLONIZE_COL1_NATION_COUNT rows) — kept as its
 * own build step for the same reason: render draws exactly what this
 * function decided, nothing recomputed inline. */
static int reports_foreign_build_rows(
  const ColonizeCol1Save* col1,
  ForeignRow* rows,
  int max_rows
) {
  if (!col1) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT && n < max_rows; ++i) {
    ForeignRow* r = &rows[n++];
    memset(r, 0, sizeof(*r));
    const ColonizeCol1Player* p = &col1->player[i];
    r->leader = p->name[0] ? p->name : reports_nation_adjective(i);
    r->adjective = reports_nation_adjective(i);
    r->withdrawn = (p->control == 2);
    if (r->withdrawn) {
      continue;
    }
    for (int j = 0; j < (int)COLONIZE_COL1_NATION_COUNT &&
                    r->peer_count < (int)(COLONIZE_COL1_NATION_COUNT - 1);
         ++j) {
      if (j == i || col1->player[j].control == 2) {
        continue;
      }
      r->peer_nation[r->peer_count] = j;
      r->peer_war[r->peer_count] = reports_foreign_at_war(col1, i, j);
      r->peer_count++;
    }
    const int total = col1->stuff.census_pop_proxy[i];
    const int rebels = (total * (int)col1->nation[i].rebel_sentiment) / 100;
    r->rebels = rebels;
    r->tories = total - rebels;
  }
  return n;
}

static void reports_render_foreign(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
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
  const int n = reports_foreign_build_rows(col1, rows, (int)COLONIZE_COL1_NATION_COUNT);
  char line[64];

  for (int i = 0; i < n; ++i) {
    const ForeignRow* r = &rows[i];
    const int block_top = REPORTS_FOREIGN_BLOCK0_Y + i * REPORTS_FOREIGN_BLOCK_STEP;
    reports_draw_hline(fb, 0, fb->width, block_top, REPORTS_FOREIGN_RULE_COLOR);

    const int header_y = block_top + REPORTS_FOREIGN_HEADER_DY;
    snprintf(line, sizeof(line), "%s's", r->leader);
    reports_draw_line(font, fb, REPORTS_FOREIGN_COL1_X, header_y, line, REPORTS_FOREIGN_LEADER_COLOR);
    const int leader_w = font ? font_text_width(font, line) : 0;
    snprintf(line, sizeof(line), " %s:", r->adjective);
    reports_draw_line(
      font, fb, REPORTS_FOREIGN_COL1_X + leader_w, header_y, line, REPORTS_FOREIGN_ADJ_COLOR
    );

    const int body_y = block_top + REPORTS_FOREIGN_BODY_DY;
    if (r->withdrawn) {
      const char* msg = "(Withdrawn from New World)";
      const int w = font ? font_text_width(font, msg) : 0;
      reports_draw_line(font, fb, (fb->width - w) / 2, body_y, msg, REPORTS_FOREIGN_LABEL_COLOR);
      continue;
    }

    for (int p = 0; p < r->peer_count; ++p) {
      const int col_x = (p % 2 == 0) ? REPORTS_FOREIGN_COL1_X : REPORTS_FOREIGN_COL2_X;
      const int row_y = body_y + (p / 2) * REPORTS_FOREIGN_LINE_STEP;
      snprintf(line, sizeof(line), "%s:", k_euro_country[r->peer_nation[p]]);
      reports_draw_line(font, fb, col_x, row_y, line, REPORTS_FOREIGN_LABEL_COLOR);
      const int label_w = font ? font_text_width(font, line) : 0;
      snprintf(line, sizeof(line), " %s", r->peer_war[p] ? "War" : "Peace");
      reports_draw_line(
        font, fb, col_x + label_w, row_y, line,
        r->peer_war[p] ? REPORTS_FOREIGN_WAR_COLOR : REPORTS_FOREIGN_PEACE_COLOR
      );
    }

    const int peer_rows = (r->peer_count + 1) / 2;
    const int rebels_y = body_y + peer_rows * REPORTS_FOREIGN_LINE_STEP;
    snprintf(line, sizeof(line), "Rebels: %d", r->rebels);
    reports_draw_line(font, fb, REPORTS_FOREIGN_COL1_X, rebels_y, line, REPORTS_FOREIGN_LABEL_COLOR);
    snprintf(line, sizeof(line), "Tories: %d", r->tories);
    reports_draw_line(font, fb, REPORTS_FOREIGN_COL2_X, rebels_y, line, REPORTS_FOREIGN_LABEL_COLOR);
  }
}

/*
 * Indian Adviser report (F9) — golden: indian.png. One two-line block per
 * contacted tribe (indian.euro_diplo[human] != 0 — bit 0x20 met/0x40 peace;
 * DOS: FUN_3f41_010a's `(uVar1 & 0x20) != 0` gate, viceroy_unpacked.c:69480):
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
 * Icon: ICONS.SS #113, one of five near-identical headband portraits
 * (#113-117, 16x16) DOS appears to pick between via an alarm-derived 0-4
 * index (an unidentified `0x281f`-segment helper, not the unrelated
 * `FUN_521d_0a60` euro-goal function of similar name) — indian.png's only
 * two examples (alarm 0 vs 34-48 toward the viewing nation) both render
 * pixel-identical #113, so this port always uses #113 pending a golden
 * that actually shows a different variant.
 */
#define REPORTS_INDIAN_ROW0_Y 28 /* first tribe name line (golden: indian.png text-color scan) */
#define REPORTS_INDIAN_ROW_STEP 21
#define REPORTS_INDIAN_ICON_X 8
#define REPORTS_INDIAN_ICON_DY (-2) /* icon top = name_y + this */
#define REPORTS_INDIAN_ICON_SPRITE 113 /* ICONS.SS headband portrait, 16x16 — see comment above */
#define REPORTS_INDIAN_NAME_X 30
#define REPORTS_INDIAN_STATS_DY 8 /* stats line y = name_y + this */
#define REPORTS_INDIAN_VILLAGES_X 40
#define REPORTS_INDIAN_MISSIONS_X 96
#define REPORTS_INDIAN_MUSKETS_X 153
#define REPORTS_INDIAN_HORSES_X 208
#define REPORTS_INDIAN_LEVEL_RIGHT 310
#define REPORTS_INDIAN_TEXT_COLOR 0 /* black — golden-sampled (0,0,0) exactly */
#define REPORTS_INDIAN_UNIT_ARMED_BRAVE 20 /* Viceroy type id, indians.md @UNIT table */
#define REPORTS_INDIAN_UNIT_MTD_WARRIOR 22
#define REPORTS_INDIAN_MUSKET_UNIT_SCALE 50

/* unit_chrome.c's k_tribe_colors, duplicated (see that file's own comment
 * on why these tables aren't shared via a header). Confirmed an exact
 * (0-distance) RGB match against REPORT9.PIK for tribe 2 (Arawak, 54) and
 * tribe 4 (Cherokee, 67) — the two golden examples. */
static const uint8_t k_indian_tribe_colors[COLONIZE_COL1_INDIAN_COUNT] = {
  97, 149, 54, 87, 67, 111, 118, 71
};

typedef struct IndianRow {
  int nation_id;
  const char* name;
  uint8_t color;
  const char* level;
  int villages;
  int missions;
  int muskets;
  int horse_herds;
} IndianRow;

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
    if (ind->euro_diplo[human] == 0) {
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
        if (u->type_index == REPORTS_INDIAN_UNIT_ARMED_BRAVE ||
            u->type_index == REPORTS_INDIAN_UNIT_MTD_WARRIOR) {
          armed_units++;
        }
      }
    }
    IndianRow* r = &rows[n++];
    r->nation_id = nation_id;
    r->name = k_tribe_names[t];
    r->color = k_indian_tribe_colors[t];
    r->level = reports_tribe_level(ind->tech);
    r->villages = villages;
    r->missions = missions;
    r->muskets = ((int)ind->muskets + armed_units) * REPORTS_INDIAN_MUSKET_UNIT_SCALE;
    r->horse_herds = ind->horse_herds;
  }
  return n;
}

static void reports_render_indian(
  const ColonizeReportsView* view,
  const ColonizeCol1Save* col1,
  const ColonizeUnitPool* units,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
) {
  /* Body text needs FONTTINY, not FONTSMAL — same pitfall as Naval/Congress
   * page 1 (docs/report_screens.md): the tight fixed-column stats line
   * ("<n> Villages" at 56px available width) only fits at FONTTINY size. */
  font = (view && view->title_font_ok) ? &view->title_font : font;

  IndianRow rows[COLONIZE_COL1_INDIAN_COUNT];
  const int n = reports_indian_build_rows(col1, units, human, rows, COLONIZE_COL1_INDIAN_COUNT);

  for (int i = 0; i < n; ++i) {
    const IndianRow* r = &rows[i];
    const int name_y = REPORTS_INDIAN_ROW0_Y + i * REPORTS_INDIAN_ROW_STEP;
    const int stats_y = name_y + REPORTS_INDIAN_STATS_DY;

    if (view && view->icons_ok) {
      ss_blit_sprite(
        &view->icons, REPORTS_INDIAN_ICON_SPRITE, fb, REPORTS_INDIAN_ICON_X,
        name_y + REPORTS_INDIAN_ICON_DY
      );
    }

    char name_buf[40];
    snprintf(name_buf, sizeof(name_buf), "%s:", r->name);
    reports_draw_line_shadowed(font, fb, REPORTS_INDIAN_NAME_X, name_y, name_buf, r->color);
    reports_draw_right_shadowed(font, fb, REPORTS_INDIAN_LEVEL_RIGHT, name_y, r->level, r->color);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d Villages", r->villages);
    reports_draw_line(font, fb, REPORTS_INDIAN_VILLAGES_X, stats_y, buf, REPORTS_INDIAN_TEXT_COLOR);
    if (r->missions > 0) {
      snprintf(buf, sizeof(buf), "%d Missions", r->missions);
      reports_draw_line(font, fb, REPORTS_INDIAN_MISSIONS_X, stats_y, buf, REPORTS_INDIAN_TEXT_COLOR);
    }
    if (r->muskets > 0) {
      snprintf(buf, sizeof(buf), "%d Muskets", r->muskets);
      reports_draw_line(font, fb, REPORTS_INDIAN_MUSKETS_X, stats_y, buf, REPORTS_INDIAN_TEXT_COLOR);
    }
    if (r->horse_herds > 0) {
      snprintf(buf, sizeof(buf), "%d Horse Herds", r->horse_herds);
      reports_draw_line(font, fb, REPORTS_INDIAN_HORSES_X, stats_y, buf, REPORTS_INDIAN_TEXT_COLOR);
    }
  }

  if (n == 0) {
    reports_draw_line(font, fb, 8, REPORTS_INDIAN_ROW0_Y, "No tribes contacted yet.", 14);
  }
}

/* Unit type → default @JOB index when profession is out of range. */
static int reports_profession_from_unit_type(int type) {
  static const int k_map[] = {
    19, /* Colonists → Free Colonists */
    21, /* Soldiers */
    20, /* Pioneers */
    24, /* Missionaries */
    23, /* Dragoons */
    22, /* Scouts */
    21, /* Regulars → Veteran Soldiers */
    23, /* Cont. Cav. */
    23, /* Cavalry */
    21, /* Cont. Army */
    -1, /* Treasure */
    -1, /* Artillery */
    -1, /* Wagon Train */
    -1, /* Caravel … ships */
    -1,
    -1,
    -1,
    -1,
    -1
  };
  if (type < 0 || type >= (int)(sizeof(k_map) / sizeof(k_map[0]))) {
    return -1;
  }
  return k_map[type];
}

static bool reports_unit_type_is_scored_colonist(int type) {
  return reports_profession_from_unit_type(type) >= 0;
}

/* Manual schedule: criminal/servant +1, free/convert +2, skilled +4. */
static int reports_citizen_points_for_job(int job) {
  if (job == 25 || job == 26) {
    return 1; /* Indentured Servants, Petty Criminals */
  }
  if (job == 19 || job == 27) {
    return 2; /* Free Colonists, Indian Converts */
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
  /* nation.founding_fathers[4] (per-nation bitmask) is authoritative — see
   * reports_ff_owned_by_nation. head.founding_father[i] (elsewhere read as
   * "owning nation") is NOT nation-exclusive membership: confirmed against
   * dutch-reports.SAV, it undercounts (misses FFs it also credits to another
   * nation), so it's a last-resort fallback only, not tried first. */
  int ff = 0;
  for (int b = 0; b < 4; ++b) {
    uint8_t bits = col1->nation[human].founding_fathers[b];
    while (bits) {
      ff += bits & 1u;
      bits >>= 1;
    }
  }
  if (ff > 0) {
    return ff > (int)COLONIZE_COL1_FF_COUNT ? (int)COLONIZE_COL1_FF_COUNT : ff;
  }
  const uint16_t counted = col1->nation[human].founding_father_count;
  if (counted > 0 && counted <= COLONIZE_COL1_FF_COUNT) {
    return (int)counted;
  }
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if (col1->head.founding_father[i] == (int8_t)human) {
      ff++;
    }
  }
  return ff;
}

/* nation.rebel_sentiment (nation+0x19) is the DOS-maintained empire-wide
 * value the Score screen actually shows — confirmed byte-for-byte against
 * dutch-reports.SAV (94, matching score.png's "Rebel Sentiment: +94"
 * exactly). A pop-weighted recompute from colony rebel_dividend/rebel_divisor
 * looked equivalent but isn't (91 on that same save) — trust the stored
 * field, don't re-derive it. */
static int reports_rebel_sentiment_pct(const ColonizeCol1Save* col1, int human) {
  if (!col1) {
    return 0;
  }
  int pct = (int)col1->nation[human].rebel_sentiment;
  if (pct < 0) {
    pct = 0;
  }
  if (pct > 100) {
    pct = 100;
  }
  return pct;
}

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
static int reports_score_collect_citizen_jobs(
  const ColonizeCol1Save* col1,
  int human,
  int* jobs_out,
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
      jobs_out[count++] = reports_resolve_job((int)c->profession[p], -1);
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
    const int prof = (int)u->profession;
    if (prof < 0 || prof >= k_job_count) {
      continue;
    }
    jobs_out[count++] = prof;
  }
  return count;
}

static int reports_foreign_recognition_pct(int prior_nations, bool achieved) {
  if (!achieved) {
    return 0;
  }
  if (prior_nations <= 0) {
    return 100;
  }
  if (prior_nations == 1) {
    return 50;
  }
  if (prior_nations == 2) {
    return 25;
  }
  return 0;
}

static int reports_early_revolution_pct(bool declared, int declare_year) {
  if (!declared || declare_year <= 0 || declare_year >= 1780) {
    return 0;
  }
  return 1780 - declare_year;
}

void reports_compute_score(
  ColonizeScoreBreakdown* out,
  const ColonizeCol1Save* col1,
  int human_nation,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe
) {
  memset(out, 0, sizeof(*out));
  const int human = reports_clamp_nation(human_nation);

  if (col1) {
    out->year = (int)col1->head.year;
    out->difficulty = (int)col1->head.difficulty;
    if (out->difficulty < 0) {
      out->difficulty = 0;
    }
    if (out->difficulty > 4) {
      out->difficulty = 4;
    }

    /* Citizens: colony population + qualifying map/Europe units — see
     * reports_score_collect_citizen_jobs for the two source-specific rules. */
    {
      int jobs[REPORTS_SCORE_CITIZENS_MAX];
      const int n =
        reports_score_collect_citizen_jobs(col1, human, jobs, REPORTS_SCORE_CITIZENS_MAX);
      for (int i = 0; i < n; ++i) {
        out->citizens += reports_citizen_points_for_job(jobs[i]);
      }
    }

    out->congress = reports_count_ff_for_nation(col1, human) * 5;
    out->treasury = (int)(col1->nation[human].gold / 1000u);
    out->rebel_sentiment = reports_rebel_sentiment_pct(col1, human);
    out->villages_burned = (int)col1->nation[human].villages_burned;
    out->villages_penalty = -(out->difficulty + 1) * out->villages_burned;
    out->intervention_bells = (int)founding_fathers_intervention_bells(human);

    /*
     * Independence: WoI latch head.unknown46[0] (ai_king). Declare year not
     * separately latched yet — early-revolution % stays 0 until a year field
     * is wired. Achieve stays false until revolution victory sequence.
     * AI "withdrawn" (control==2) still counts toward foreign recognition.
     */
    for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
      if (n == human) {
        continue;
      }
      if (col1->player[n].control == 2) {
        out->prior_nations++;
      }
    }
    out->independence_declared = col1->head.game_options.woi != 0;
    out->independence_achieved = col1->head.unknown46[4] == 1;
    out->declare_year = 0;
  } else {
    out->year = 0;
    out->difficulty = 0;
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

  out->early_revolution_pct =
    reports_early_revolution_pct(out->independence_declared, out->declare_year);
  out->foreign_recognition_pct =
    reports_foreign_recognition_pct(out->prior_nations, out->independence_achieved);

  out->base_total = out->citizens + out->congress + out->treasury + out->rebel_sentiment +
                    out->villages_penalty + out->intervention_bells;

  const int bonus_pct = out->early_revolution_pct + out->foreign_recognition_pct;
  if (bonus_pct > 0 && out->base_total > 0) {
    out->total = out->base_total + (out->base_total * bonus_pct) / 100;
  } else {
    out->total = out->base_total;
  }
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

static void reports_score_fill_rect(
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int w,
  int h,
  uint8_t color
) {
  if (!fb || !fb->pixels || w <= 0 || h <= 0) {
    return;
  }
  for (int yy = y; yy < y + h; ++yy) {
    if (yy < 0 || yy >= fb->height) {
      continue;
    }
    for (int xx = x; xx < x + w; ++xx) {
      if (xx < 0 || xx >= fb->width) {
        continue;
      }
      fb->pixels[yy * fb->width + xx] = color;
    }
  }
}

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
  const int* jobs,
  int count,
  int x,
  int y,
  int w
) {
  if (!view || !view->icons_ok || count <= 0 || w <= 0) {
    return;
  }
  int icon_w = 6;
  int icon_h = 16;
  for (int i = 0; i < count; ++i) {
    const int probe = units_job_icon_sprite(jobs[i]);
    if (probe >= 0 && probe < view->icons.sprite_count) {
      icon_w = view->icons.sprites[probe].width;
      icon_h = view->icons.sprites[probe].height;
      break;
    }
  }
  const int per_row = w / REPORTS_SCORE_ICON_PITCH_X + 1;
  const int row_dy = icon_h / 2;
  const int row_dx = icon_w / 2;
  for (int i = 0; i < count; ++i) {
    const int icon = units_job_icon_sprite(jobs[i]);
    if (icon < 0 || icon >= view->icons.sprite_count) {
      continue;
    }
    const int row = i / per_row;
    const int col = i % per_row;
    int ix = x + col * REPORTS_SCORE_ICON_PITCH_X;
    if (row % 2 == 1) {
      ix += row_dx;
    }
    const int iy = y + row * row_dy;
    ss_blit_sprite(&view->icons, icon, fb, ix, iy);
  }
}

static void reports_render_score(
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
  reports_compute_score(&sc, col1, human, colonies, europe);

  const ColonizeFont* body_font = (view && view->title_font_ok) ? &view->title_font : font;

  static const char* k_diff[] = {
    "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
  };
  const char* diff_name =
    (sc.difficulty >= 0 && sc.difficulty <= 4) ? k_diff[sc.difficulty] : "?";
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
      col1->head.autumn ? "Autumn" : "Spring",
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

  if (!col1) {
    snprintf(line, line_sz, "Gold                    %d", sc.treasury);
    reports_draw_line(
      body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_GOLD_Y, line, REPORTS_SCORE_GREEN_COLOR
    );
    snprintf(line, line_sz, "Total Score             %d", sc.total);
    reports_draw_line(
      body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_TOTAL_Y, line, REPORTS_SCORE_TITLE_COLOR
    );
    return;
  }

  /* Citizens line + icon strip. */
  snprintf(line, line_sz, "%s Citizens:  +%d", nation_adj, sc.citizens);
  reports_draw_line(
    body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_CITIZENS_Y, line, REPORTS_SCORE_GREEN_COLOR
  );
  {
    int jobs[REPORTS_SCORE_CITIZENS_MAX];
    const int n = reports_score_collect_citizen_jobs(col1, human, jobs, REPORTS_SCORE_CITIZENS_MAX);
    reports_score_draw_citizen_icons(
      view, fb, jobs, n, REPORTS_SCORE_ICON_X, REPORTS_SCORE_ICON_Y, REPORTS_SCORE_ICON_W
    );
  }

  /* Continental Congress line + 4-column Founding Father name grid. */
  snprintf(line, line_sz, "%s Continental Congress:  +%d", nation_adj, sc.congress);
  reports_draw_line(
    body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_CONGRESS_Y, line, REPORTS_SCORE_GREEN_COLOR
  );
  {
    int shown = 0;
    for (int idx = 0; idx < (int)COLONIZE_COL1_FF_COUNT; ++idx) {
      if (!reports_ff_owned_by_nation(&col1->nation[human], idx)) {
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

  /* Gold / Rebel Sentiment / Total Score — golden shows exactly these three,
   * no Villages/Intervention/Independence breakout lines (this golden has
   * zero villages burned and independence undeclared, but DOS appears to
   * fold every other component silently into Total Score rather than list
   * them; nothing in score.png suggests those rows ever appear here). */
  /* "$" — same coin-glyph convention as the map sidebar's own gold line
   * (map_panel.c: "Gold: %d$"), not a literal "g" suffix. */
  snprintf(line, line_sz, "Gold:  (%u$) +%d", (unsigned)(col1->nation[human].gold), sc.treasury);
  reports_draw_line(
    body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_GOLD_Y, line, REPORTS_SCORE_GREEN_COLOR
  );
  snprintf(line, line_sz, "Rebel Sentiment:  +%d", sc.rebel_sentiment);
  reports_draw_line(
    body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_REBEL_Y, line, REPORTS_SCORE_GREEN_COLOR
  );
  snprintf(line, line_sz, "Total Score: %d", sc.total);
  reports_draw_line(
    body_font, fb, REPORTS_SCORE_LEFT_X, REPORTS_SCORE_TOTAL_Y, line, REPORTS_SCORE_TITLE_COLOR
  );

  /* Bottom progress bar: proportional fill toward a nominal 1000-point
   * score, not toward anything display-labeled — golden's fill measures
   * 76/250px = 30.4% against a Total Score of 305/1000 = 30.5%. */
  reports_score_fill_rect(
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
    reports_score_fill_rect(
      fb,
      REPORTS_SCORE_BAR_X,
      REPORTS_SCORE_BAR_Y,
      fill_w,
      REPORTS_SCORE_BAR_H,
      REPORTS_SCORE_BAR_FILL_COLOR
    );
  }
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

  const int step = reports_line_step(font);
  int y = 4;
  reports_draw_line(font, fb, 8, y, "COLONIZATION HALL OF FAME", 15); /* LABELS.TXT #207 */
  y += step;
  reports_draw_line(font, fb, 8, y, "Esc / Enter returns to menu", 14);
  y += step + 4;

  reports_draw_line(font, fb, 8, y, "     Leader                    Nation      Score  A.D.", 15);
  y += step;

  char line[160];
  if (entry_count <= 0) {
    reports_draw_line(font, fb, 8, y, "  No retired games yet.", 14);
    return;
  }
  const int shown = entry_count > COLONIZE_HOF_ROW_MAX ? COLONIZE_HOF_ROW_MAX : entry_count;
  for (int i = 0; i < shown && y < 190; ++i) {
    const ColonizeHofRow* e = &entries[i];
    snprintf(
      line,
      sizeof(line),
      "%2d.  %-24s %-10s %6d  %d",
      i + 1,
      e->leader,
      e->nation,
      e->score,
      e->year
    );
    reports_draw_line(font, fb, 8, y, line, 15);
    y += step;
  }
}

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
) {
  (void)map;
  (void)cursor_x;
  (void)cursor_y;
  if (!framebuffer || !framebuffer->pixels) {
    return;
  }
  if (id < 0 || id >= COLONIZE_REPORT_COUNT) {
    id = COLONIZE_REPORT_RELIGIOUS;
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
      reports_render_foreign(view, col1, font, framebuffer);
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
