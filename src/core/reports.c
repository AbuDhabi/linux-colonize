#include "core/founding_fathers.h"
#include "core/reports.h"
#include "core/strutil.h"

#include <stdio.h>
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

static const char* k_tribe_names[COLONIZE_COL1_INDIAN_COUNT] = {
  "Inca", "Aztec", "Arawak", "Iroquois", "Cherokee", "Apache", "Sioux", "Tupi"
};

static const char* k_tribe_levels[] = {"Semi-Nomadic", "Agrarian", "Advanced", "Civilized"};

static const char* k_attitudes[] = {"Content", "Uneasy", "Restless", "Angry", "War"};

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
  reports_draw_line(title_font, fb, (fb->width - title_w) / 2, 5, title, 15);
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

static bool reports_ff_joined(int8_t status) {
  return status > 0;
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

static const char* reports_attitude_from_alarm(unsigned alarm) {
  /* Rough bands matching @ATTITUDE labels Content..War. */
  if (alarm <= 2) {
    return k_attitudes[0];
  }
  if (alarm <= 5) {
    return k_attitudes[1];
  }
  if (alarm <= 8) {
    return k_attitudes[2];
  }
  if (alarm <= 11) {
    return k_attitudes[3];
  }
  return k_attitudes[4];
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
        /* Two-pixel drop shadow, same convention as unit_chrome_blit_unit:
         * a same-shape black underlay 2px to the sprite's left, drawn first. */
        ss_blit_sprite_color(&view->icons, icon, fb, cx - 2, cy, 0);
        ss_blit_sprite(&view->icons, icon, fb, cx, cy);
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
    ss_blit_sprite_color(&view->icons, icon, fb, 4 - 2, header_y, 0);
    ss_blit_sprite(&view->icons, icon, fb, 4, header_y);
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

static void reports_render_colony(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "Colony warehouses / status", 15);
  *y += step;
  reports_draw_line(font, fb, 8, *y, "(Click on item to zoom — not wired)", 14);
  *y += step;

  int totals[COLONIZE_COL1_CARGO_TYPES];
  memset(totals, 0, sizeof(totals));
  int n = 0;

  if (col1) {
    for (uint16_t i = 0; i < col1->head.colony_count && *y < 150; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if (c->nation_id != (uint8_t)human) {
        continue;
      }
      snprintf(
        line,
        line_sz,
        "%s (%u,%u) pop %u  rebel %d%%",
        c->name,
        (unsigned)c->x,
        (unsigned)c->y,
        (unsigned)c->population,
        reports_colony_rebel_pct(c)
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      snprintf(
        line,
        line_sz,
        "  food %u  lumber %u  tools %u  muskets %u  horses %u",
        (unsigned)c->stock[0],
        (unsigned)c->stock[5],
        (unsigned)c->stock[14],
        (unsigned)c->stock[15],
        (unsigned)c->stock[8]
      );
      reports_draw_line(font, fb, 8, *y, line, 14);
      *y += step;
      for (int g = 0; g < (int)COLONIZE_COL1_CARGO_TYPES; ++g) {
        totals[g] += (int)c->stock[g];
      }
      n++;
    }
  } else if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX && *y < 160; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active) {
        continue;
      }
      snprintf(
        line,
        line_sz,
        "%s (%d,%d) pop %d",
        c->name,
        c->x,
        c->y,
        c->population
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      snprintf(
        line,
        line_sz,
        "  food %d  lumber %d  tools %d  muskets %d  horses %d",
        c->stock[COLONIZE_CARGO_FOOD],
        c->stock[COLONIZE_CARGO_LUMBER],
        c->stock[COLONIZE_CARGO_TOOLS],
        c->stock[COLONIZE_CARGO_MUSKETS],
        c->stock[COLONIZE_CARGO_HORSES]
      );
      reports_draw_line(font, fb, 8, *y, line, 14);
      *y += step;
      for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
        totals[g] += c->stock[g];
      }
      n++;
    }
  }

  if (n == 0) {
    reports_draw_line(font, fb, 8, *y, "No colonies founded.", 14);
    return;
  }

  snprintf(line, line_sz, "Total colonies: %d", n);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  reports_draw_line(font, fb, 8, *y, "Warehouse totals:", 15);
  *y += step;
  for (int g = 0; g < (int)COLONIZE_COL1_CARGO_TYPES && *y < 195; ++g) {
    if (totals[g] <= 0) {
      continue;
    }
    snprintf(line, line_sz, "  %s: %d", k_cargo_names[g], totals[g]);
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
  }
}

static void reports_render_naval(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeUnitPool* units,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "Ships — cargo / location / destination", 15);
  *y += step;

  int sea = 0;

  if (col1) {
    for (uint16_t i = 0; i < col1->head.unit_count && *y < 180; ++i) {
      const ColonizeCol1Unit* u = &col1->unit[i];
      if ((int)u->nation_id != human) {
        continue;
      }
      if (u->type < 13 || u->type > 18) {
        continue;
      }
      const char* ship_name = "Ship";
      if (units) {
        const ColonizeUnitType* ut = units_type(units, u->type);
        if (ut) {
          ship_name = ut->name;
        }
      }
      const char* loc = reports_unit_in_europe(u->x, u->y) ? "Off Mapboard (Europe)" : "On Mapboard";
      if (u->orders == 0 && u->goto_x == 0xFF) {
        snprintf(
          line,
          line_sz,
          "  %s  %s (%u,%u)  holds %u",
          ship_name,
          loc,
          (unsigned)u->x,
          (unsigned)u->y,
          (unsigned)u->holds_occupied
        );
      } else {
        snprintf(
          line,
          line_sz,
          "  %s  %s (%u,%u)  holds %u  dest (%u,%u)",
          ship_name,
          loc,
          (unsigned)u->x,
          (unsigned)u->y,
          (unsigned)u->holds_occupied,
          (unsigned)u->goto_x,
          (unsigned)u->goto_y
        );
      }
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      sea++;
    }
  } else if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX && *y < 170; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!u->active || !units_is_sea(units, u->id)) {
        continue;
      }
      if (u->nation_id >= 0 && u->nation_id < 4 && u->nation_id != human) {
        continue;
      }
      const ColonizeUnitType* ut = units_type(units, u->type_index);
      const char* loc =
        reports_unit_in_europe(u->x, u->y) ? "Off Mapboard (Europe)" : "On Mapboard";
      snprintf(
        line,
        line_sz,
        "  %s  %s (%d,%d)  hold %d",
        ut ? ut->name : "Ship",
        loc,
        u->x,
        u->y,
        u->cargo_count
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      sea++;
    }
  }

  if (europe) {
    snprintf(line, line_sz, "Europe harbor ships: %d", europe->harbor_ships);
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
    for (int i = 0; i < europe->harbor_ships && *y < 190; ++i) {
      snprintf(
        line,
        line_sz,
        "  Harbor: %s (+%d passengers)",
        europe->harbor[i].name,
        europe->harbor[i].cargo_count
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      sea++;
    }
  }

  if (sea == 0) {
    reports_draw_line(font, fb, 8, *y, "No ships in play.", 14);
  }
}

static void reports_count_nation_forces(
  const ColonizeCol1Save* col1,
  int nation,
  int* out_colonies,
  int* out_pop,
  int* out_military,
  int* out_naval,
  int* out_merchant
) {
  int colonies = 0;
  int pop = 0;
  int military = 0;
  int naval = 0;
  int merchant = 0;
  if (!col1) {
    *out_colonies = 0;
    *out_pop = 0;
    *out_military = 0;
    *out_naval = 0;
    *out_merchant = 0;
    return;
  }
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    if (col1->colony[i].nation_id == (uint8_t)nation) {
      colonies++;
      pop += col1->colony[i].population;
    }
  }
  for (uint16_t i = 0; i < col1->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &col1->unit[i];
    if ((int)u->nation_id != nation) {
      continue;
    }
    if (u->type >= 13 && u->type <= 18) {
      if (u->type == 13 || u->type == 14 || u->type == 15) {
        merchant++;
      } else {
        naval++;
      }
    } else if (u->type == 1 || u->type == 4 || u->type == 6 || u->type == 7 || u->type == 8 ||
               u->type == 9 || u->type == 11) {
      military++;
    }
  }
  *out_colonies = colonies;
  *out_pop = pop;
  *out_military = military;
  *out_naval = naval;
  *out_merchant = merchant;
}

static void reports_render_foreign(
  const ColonizeCol1Save* col1,
  int human,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "European rivals", 15);
  *y += step;

  if (!col1) {
    reports_draw_line(font, fb, 8, *y, "Other powers require a loaded Col1 save.", 14);
    *y += step;
    if (europe) {
      snprintf(line, line_sz, "Your nation: %s", europe->nation_name);
      reports_draw_line(font, fb, 8, *y, line, 15);
    }
    return;
  }

  const bool detailed = reports_ff_joined(col1->head.founding_father[4]); /* Jan de Witt */
  if (!detailed) {
    reports_draw_line(
      font, fb, 8, *y, "Detailed strength unlocks with Jan de Witt.", 14
    );
    *y += step;
  }

  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT && *y < 170; ++n) {
    const ColonizeCol1Player* p = &col1->player[n];
    const char* ctrl =
      p->control == 0 ? "Player" : (p->control == 2 ? "Withdrawn" : "AI");
    int colonies = 0, pop = 0, mil = 0, nav = 0, mer = 0;
    reports_count_nation_forces(col1, n, &colonies, &pop, &mil, &nav, &mer);
    const int avg = colonies > 0 ? pop / colonies : 0;

    snprintf(
      line,
      line_sz,
      "%s%s (%s)",
      p->country_name[0] ? p->country_name : k_euro_short[n],
      n == human ? " *" : "",
      ctrl
    );
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;

    if (detailed || n == human) {
      snprintf(
        line,
        line_sz,
        "  colonies %d  pop %d  avg %d  mil %d  naval %d  merchants %d",
        colonies,
        pop,
        avg,
        mil,
        nav,
        mer
      );
      reports_draw_line(font, fb, 8, *y, line, 14);
      *y += step;
    } else {
      snprintf(line, line_sz, "  colonies founded: %u", (unsigned)p->founded_colonies);
      reports_draw_line(font, fb, 8, *y, line, 14);
      *y += step;
    }
  }

  snprintf(
    line,
    line_sz,
    "Royal Expeditionary Force: %u/%u/%u/%u",
    (unsigned)col1->head.expeditionary_force[0],
    (unsigned)col1->head.expeditionary_force[1],
    (unsigned)col1->head.expeditionary_force[2],
    (unsigned)col1->head.expeditionary_force[3]
  );
  reports_draw_line(font, fb, 8, *y, line, 15);
}

static void reports_render_indian(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "Native tribes contacted", 15);
  *y += step;

  if (!col1 || !col1->tribe) {
    reports_draw_line(font, fb, 8, *y, "Indian villages require a loaded Col1 save.", 14);
    return;
  }

  int shown = 0;
  for (int t = 0; t < (int)COLONIZE_COL1_INDIAN_COUNT && *y < 185; ++t) {
    const ColonizeCol1Indian* ind = &col1->indian[t];
    const uint8_t nation_id = (uint8_t)(t + 4);
    int villages = 0;
    int pop = 0;
    int missions = 0;
    int capitals = 0;
    int alarm_sum = 0;
    int alarm_n = 0;

    for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* tr = &col1->tribe[i];
      if (tr->nation_id != nation_id) {
        continue;
      }
      villages++;
      pop += tr->population;
      if (tr->mission != 0xFF) {
        missions++;
      }
      if (tr->state.capital) {
        capitals++;
      }
      if (human >= 0 && human < 4) {
        alarm_sum += tr->alarm[human].friction;
        alarm_n++;
      }
    }

    const bool met = ind->euro_diplo[human] != 0 || villages > 0;
    if (!met && villages == 0) {
      continue;
    }

    const unsigned alarm =
      ind->alarm_by_player[human] != 0
        ? (unsigned)ind->alarm_by_player[human]
        : (alarm_n > 0 ? (unsigned)(alarm_sum / alarm_n) : 0u);

    snprintf(
      line,
      line_sz,
      "%s (%s)  villages %d  pop %d",
      k_tribe_names[t],
      reports_tribe_level(ind->tech),
      villages,
      pop
    );
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
    snprintf(
      line,
      line_sz,
      "  %s  missions %d  capitals %d  alarm %u",
      reports_attitude_from_alarm(alarm),
      missions,
      capitals,
      alarm
    );
    reports_draw_line(font, fb, 8, *y, line, 14);
    *y += step;
    shown++;
  }

  if (shown == 0) {
    reports_draw_line(font, fb, 8, *y, "No tribes contacted yet.", 14);
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

static int reports_rebel_sentiment_pct(const ColonizeCol1Save* col1, int human) {
  if (!col1) {
    return 0;
  }
  uint64_t weighted = 0;
  uint64_t pop = 0;
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &col1->colony[i];
    if (c->nation_id != (uint8_t)human || c->population == 0) {
      continue;
    }
    weighted += (uint64_t)reports_colony_rebel_pct(c) * (uint64_t)c->population;
    pop += c->population;
  }
  if (pop == 0) {
    return 0;
  }
  int pct = (int)(weighted / pop);
  if (pct < 0) {
    pct = 0;
  }
  if (pct > 100) {
    pct = 100;
  }
  return pct;
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

    /* Colony citizens by profession. */
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if (c->nation_id != (uint8_t)human) {
        continue;
      }
      const int pop =
        c->population > COLONIZE_COL1_COLONY_POP_MAX ? COLONIZE_COL1_COLONY_POP_MAX
                                                     : (int)c->population;
      for (int p = 0; p < pop; ++p) {
        const int job = reports_resolve_job((int)c->profession[p], -1);
        out->citizens += reports_citizen_points_for_job(job);
      }
    }

    /* Map / Europe land colonists (not ships, wagons, artillery, treasure). */
    for (uint16_t i = 0; i < col1->head.unit_count; ++i) {
      const ColonizeCol1Unit* u = &col1->unit[i];
      if ((int)u->nation_id != human) {
        continue;
      }
      if (!reports_unit_type_is_scored_colonist((int)u->type)) {
        continue;
      }
      const int job = reports_resolve_job((int)u->profession, (int)u->type);
      out->citizens += reports_citizen_points_for_job(job);
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

static void reports_render_score(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  uint32_t turn_number,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  ColonizeScoreBreakdown sc;
  reports_compute_score(&sc, col1, human, colonies, europe);

  static const char* k_diff[] = {
    "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
  };
  const char* diff_name =
    (sc.difficulty >= 0 && sc.difficulty <= 4) ? k_diff[sc.difficulty] : "?";

  if (col1) {
    snprintf(
      line,
      line_sz,
      "Year %d%s   Turn %u   %s",
      sc.year,
      col1->head.autumn ? " Autumn" : " Spring",
      (unsigned)(turn_number ? turn_number : col1->head.turn),
      diff_name
    );
  } else {
    snprintf(line, line_sz, "Turn %u", (unsigned)turn_number);
  }
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step + 2;

  snprintf(line, line_sz, "Citizens                %d", sc.citizens);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(line, line_sz, "Continental Congress    %d", sc.congress);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(line, line_sz, "Gold                    %d", sc.treasury);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(line, line_sz, "Rebel Sentiment         %d", sc.rebel_sentiment);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(
    line,
    line_sz,
    "Villages Burned         %d  (%d)",
    sc.villages_penalty,
    sc.villages_burned
  );
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  if (sc.intervention_bells != 0) {
    snprintf(line, line_sz, "Intervention Bells      %d", sc.intervention_bells);
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
  }

  *y += 2;
  reports_draw_line(font, fb, 8, *y, "Independence", 15);
  *y += step;
  snprintf(
    line,
    line_sz,
    "  Declared              %s",
    sc.independence_declared ? "Yes" : "No"
  );
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step;
  snprintf(
    line,
    line_sz,
    "  Achieved              %s",
    sc.independence_achieved ? "Yes" : "No"
  );
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step;
  snprintf(line, line_sz, "  Early Revolution      +%d%%", sc.early_revolution_pct);
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step;
  snprintf(
    line,
    line_sz,
    "  Foreign Recognition   +%d%%  (%d prior nations)",
    sc.foreign_recognition_pct,
    sc.prior_nations
  );
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step + 2;

  snprintf(line, line_sz, "Subtotal                %d", sc.base_total);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(line, line_sz, "Total Score             %d", sc.total);
  reports_draw_line(font, fb, 8, *y, line, 15);

  if (!sc.independence_achieved) {
    *y += step + 2;
    reports_draw_line(
      font, fb, 8, *y, "Win independence to apply revolution bonuses.", 14
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
  const int step = reports_line_step(font);
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
    case COLONIZE_REPORT_COLONY:
      /* When unit icon rows are added, draw with unit_chrome_draw (FUN_112b_01ba). */
      reports_render_colony(
        col1, human, colonies, font, framebuffer, &y, step, line, sizeof(line)
      );
      break;
    case COLONIZE_REPORT_NAVAL:
      /* When ship icon rows are added, draw with unit_chrome_draw (FUN_112b_01ba). */
      reports_render_naval(
        col1, human, units, europe, font, framebuffer, &y, step, line, sizeof(line)
      );
      break;
    case COLONIZE_REPORT_FOREIGN:
      reports_render_foreign(
        col1, human, europe, font, framebuffer, &y, step, line, sizeof(line)
      );
      break;
    case COLONIZE_REPORT_INDIAN:
      reports_render_indian(col1, human, font, framebuffer, &y, step, line, sizeof(line));
      break;
    case COLONIZE_REPORT_SCORE:
      reports_render_score(
        col1,
        human,
        colonies,
        europe,
        turn_number,
        font,
        framebuffer,
        &y,
        step,
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
