#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_euro.h"
#include "core/assets.h"
#include "core/colony_craft.h"
#include "core/colony_preview.h"
#include "core/colony_production.h"
#include "core/colony_screen_internal.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/fb.h"
#include "core/internal.h"
#include "core/font_file.h"
#include "core/founding_fathers.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/ui_colors.h"
#include "core/turn.h"
#include "core/ui_button.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * Sections:
 *  - Popup rendering: construction/jobs/custom house/eject/dock/message (colony_screen_open_dialog_frame .. colony_screen_draw_message_popup) (~line 37)
 *  - Hit testing & top-level render entry point (colony_screen_hit_test .. colony_screen_render_w) (~line 644)
 */

/* ===================== Popup rendering: construction/jobs/custom house/eject/dock/message (colony_screen_open_dialog_frame .. colony_screen_draw_message_popup) ===================== */

/*
 * One colony-screen sub-dialog laid out like the DOS compositor (port_bugs.md #1190). Every
 * one is a 6f74 dialog with no @X / @Y of its own (FUN_2f2b_5bd2 construction list,
 * FUN_2f2b_348c jobs / leave-as lists, FUN_2f2b_4424 @CUSTOM, FUN_2f2b_51ec @COLONYUNIT),
 * so FUN_6f74_14c6 sizes and places it: `content_w` is the caller's box[+0x20] = max(0x50,
 * the builder's @width, its widest row as FUN_6f74_0a00 measures it), `icon_w` the
 * FUN_6f74_0f3c unit-icon column (0, or 22 = box[+0x48] + 16 + box[+0x32]); width content +
 * 6 + icon_w, height popup_dos_list_height, centred by popup_place_dos (port_bugs.md #1188).
 * The prose wraps to content - 4 (FUN_6f74_1198) at (frame + 5 + icon_w, frame + 6); row
 * k's bar tops at popup_dos_list_y0 + k * (glyph_h + 3), spans frame + 4 + icon_w ..
 * frame + w - 5 and is glyph_h + 2 tall, its text one px lower at text_x + box[+0x22]
 * (FUN_6f74_1b7c). `footer` = the builder raised DS:0x1f66 (FUN_6f74_1a78, drawn in the
 * dialog's own FONTTINY face). `out` is the hit-test rect (list_y0 / line_h = bar top, pitch).
 */
typedef struct ColonyDosLayout {
  int glyph_h;
  int text_x; /* prose x; rows add box[+0x22] */
  int bar_x;
  int bar_w;
  ColonizePopupColors colors; /* the box style's shades 2 / 3 */
} ColonyDosLayout;

#define COLONY_DOS_PROSE_LINES 4

static ColonyDosLayout colony_screen_open_dos_dialog(
  ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
  const char* prose,
  int rows,
  int content_w,
  int icon_w,
  bool footer,
  ColonyDialogRect* out
) {
  ColonyDosLayout lay;
  memset(&lay, 0, sizeof(lay));
  lay.glyph_h = popup_dos_glyph_h(font);
  if (content_w + 6 + icon_w > framebuffer->width) {
    content_w = framebuffer->width - 6 - icon_w;
  }
  char lines[COLONY_DOS_PROSE_LINES][96];
  const int n = (font && prose && prose[0])
                  ? popup_wrap_text(font, prose, lines[0], sizeof(lines[0]), NULL,
                                    COLONY_DOS_PROSE_LINES, content_w - 4)
                  : 0;
  const int prose_h = n * (lay.glyph_h + 1);
  const int w = content_w + 6 + icon_w;
  int h = popup_dos_list_height(lay.glyph_h, prose_h, rows, footer, lay.glyph_h == 5);
  if (h > framebuffer->height) {
    h = framebuffer->height;
  }
  const PopupDosPlace pl = popup_place_dos(framebuffer->width, framebuffer->height, w, h, NULL);
  popup_colors_from_ui(&lay.colors);
  popup_draw(
    framebuffer, pl.x, pl.y, w, h, view->wood_tile_ok ? &view->wood_tile : NULL, &lay.colors,
    NULL, NULL, NULL, NULL
  );
  lay.text_x = pl.x + 5 + icon_w;
  lay.bar_x = pl.x + 4 + icon_w;
  lay.bar_w = w - 8 - icon_w;
  /* FUN_6f74_116c -> FUN_6f74_0538: the prose in the box style's ink (port_bugs.md #1200). */
  bool hi = false;
  for (int i = 0; i < n; ++i) {
    popup_draw_text_markup(
      font, framebuffer, lay.text_x, pl.y + 6 + i * (lay.glyph_h + 1), lines[i], COLONIZE_COL_BASIC,
      COLONIZE_COL_HILITE, &lay.colors, &hi
    );
  }
  if (footer) {
    popup_dos_draw_help_footer(
      framebuffer, font, pl.x, pl.y, w, h, lay.glyph_h == 5, reports_misc_display_word(189, ""),
      COLONIZE_COL_HILITE
    );
  }
  out->x = pl.x;
  out->y = pl.y;
  out->w = w;
  out->h = h;
  out->list_y0 = popup_dos_list_y0(pl.y, prose_h);
  out->line_h = lay.glyph_h + 3;
  return lay;
}

/* FUN_6f74_1b7c row k: bar top, selection bar, and the y its text takes (bar + 1). */
static int colony_screen_dos_row(
  ColonizeFramebuffer8* framebuffer,
  const ColonyDosLayout* lay,
  const ColonyDialogRect* rect,
  int k,
  bool selected
) {
  const int bar_y = rect->list_y0 + k * rect->line_h;
  if (selected) {
    fb_fill_rect(framebuffer, lay->bar_x, bar_y, lay->bar_w, lay->glyph_h + 2, 138);
  }
  return bar_y + 1;
}

/*
 * FUN_6f74_1b7c row text through FUN_6f74_0538 in the box style (port_bugs.md #1200): @COLORS
 * 68 ink, 149 inside {}, 8 on a disabled row (the 0x1 flag FUN_6f74_08fa sets; style+4 wins
 * over both), glyph shades 128 / 47 (FONTTINY has shade 1 only). `hilite` is the 0x2 flag
 * FUN_6f74_092a sets, which starts the row inside {} (1b7c: DS:0x1f62 = flags & 2). The text
 * before the '|' column tab starts at `x`; the rest ends at frame_x + w - 5 - box[+0x22]
 * (6f74:1d6d-1da8: box[+0x24] + box[+0x20] - box[+0x48] - box[+0x22] - its FUN_6f74_0486 width).
 */
static void colony_screen_dos_row_text(
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
  const ColonyDosLayout* lay,
  const ColonyDialogRect* rect,
  int x,
  int y,
  int row_dx,
  const char* text,
  bool hilite,
  bool disabled
) {
  if (!font || !text) {
    return;
  }
  const uint8_t ink = disabled ? COLONIZE_COL_GREY : COLONIZE_COL_BASIC;
  const uint8_t hi_ink = disabled ? COLONIZE_COL_GREY : COLONIZE_COL_HILITE;
  bool hi = hilite;
  const char* tab = strchr(text, '|');
  char left[96];
  snprintf(left, sizeof(left), "%.*s", tab ? (int)(tab - text) : (int)strlen(text), text);
  popup_draw_text_markup(font, framebuffer, x, y, left, ink, hi_ink, &lay->colors, &hi);
  if (tab) {
    const int w = popup_markup_text_width(font, tab + 1);
    popup_draw_text_markup(
      font, framebuffer, rect->x + rect->w - 5 - row_dx - w, y, tab + 1, ink, hi_ink,
      &lay->colors, &hi
    );
  }
}

/* FUN_6f74_0a00 row width: the text less ~{}| (FUN_6f74_0486) + 2 * box[+0x48] + box[+0x22],
 * + box[+0x22] again for a row carrying the '|' column tab. */
static int colony_screen_dos_row_w(const ColonizeFont* font, const char* text, int row_dx) {
  char plain[128];
  size_t n = 0;
  for (const char* p = text; *p && n + 1 < sizeof(plain); ++p) {
    if (*p != '|') {
      plain[n++] = *p;
    }
  }
  plain[n] = '\0';
  return popup_markup_text_width(font, plain) + 4 + row_dx + (strchr(text, '|') ? row_dx : 0);
}

/*
 * Click-to-row for one sub-dialog (audit CO-7): false when the point is
 * outside the frame, true otherwise with *out_row set to the row index under
 * it, or -1 when the click landed on the frame but above/below the rows.
 */
static bool colony_screen_dialog_row_hit(
  const ColonyDialogRect* rect, int rows, int mx, int my, int* out_row
) {
  if (out_row) {
    *out_row = -1;
  }
  if (!rect || !ui_rect_hit(rect->x, rect->y, rect->w, rect->h, mx, my)) {
    return false;
  }
  if (out_row) {
    *out_row = popup_row_at_y(rect->list_y0, rect->line_h, rows, my);
  }
  return true;
}

/*
 * The FUN_2f2b_5bd2 / FUN_2f2b_348c two-page list (port_bugs.md #1195): page 0 lists entries
 * [0, lead + 16) and ends with "(More)" (@CTITLE[6], DS:0x93aa; id 99 / 0x62), page 1 starts
 * with "(More)" (id 0x62, back) and lists [lead + 16, lead + 32); later entries are never
 * listed. `lead` is 348c's clear-specialty row, page 0 only and outside the 0x10 quota
 * (local_134 counts job rows). Unpaged, the one page holds everything and has no More row.
 */
typedef struct ColonyDosPage {
  int start; /* first entry on the page */
  int n; /* entries on the page */
  int more_at; /* the More row's index on the page, -1 = none */
} ColonyDosPage;

static ColonyDosPage colony_screen_dos_page(int count, int lead, bool paged, int page) {
  ColonyDosPage p = {0, count, -1};
  if (!paged) {
    return p;
  }
  const int split = lead + 0x10;
  if (page == 0) {
    p.n = count < split ? count : split;
    p.more_at = p.n;
  } else {
    p.start = split;
    p.n = count - split > 0x10 ? 0x10 : count - split;
    p.more_at = 0;
  }
  return p;
}

static int colony_screen_dos_page_rows(const ColonyDosPage* p) {
  return p->n + (p->more_at >= 0 ? 1 : 0);
}

/* The entry page row k shows, or -1 for the More row. */
static int colony_screen_dos_page_entry(const ColonyDosPage* p, int k) {
  if (k == p->more_at) {
    return -1;
  }
  const int i = k - (p->more_at == 0 ? 1 : 0);
  return (i >= 0 && i < p->n) ? p->start + i : -1;
}

/* FUN_2f2b_5bd2 raw 52583-52586: more than 0x16 rows make two pages (local_a = 2, local_16 =
 * 0x10). The shown page is the selection's. */
static bool colony_screen_construction_paged(const ColonyScreenView* view) {
  return view->buildable_count + 1 > 0x16;
}

static ColonyDosPage colony_screen_construction_page(const ColonyScreenView* view) {
  const bool paged = colony_screen_construction_paged(view);
  return colony_screen_dos_page(
    view->buildable_count + 1, 0, paged, paged ? view->construction_page : 0
  );
}

/* Does the shown construction page list the current project (construction_default_row)? */
static bool colony_screen_construction_page_lists_current(const ColonyScreenView* view) {
  const int start = view->construction_page ? 0x10 : 0;
  const int cur = view->construction_default_row;
  return !colony_screen_construction_paged(view) || (cur >= start && cur < start + 0x10);
}

/*
 * The "(More)" rows: page 0 <-> page 1 (port_bugs.md #1215). FUN_2f2b_5bd2: the page that
 * lists the current project puts the bar and the hilite ink on it (raw 52607-52611).
 * Otherwise page 0 takes the 0x65 arm (raw 52620-52624) and page 1 the 100 arm (raw
 * 52627-52629): bar and hilite ink on the "(More)" row that leads to the project's page.
 * DOS bugs not reproduced: DOS clears local_8 once (raw 52575), not per page, so only the
 * first showing takes an arm (docs/dos_bugs.md DB-60); and the arms pass the More rows' ids
 * 0x63 / 0x62 with the row offset + 2 added again, ids no row has, so DOS shows no bar
 * (DB-63).
 */
void colony_screen_construction_next_page(ColonyScreenView* view) {
  if (!view || !colony_screen_construction_paged(view)) {
    return;
  }
  view->construction_page = 1 - view->construction_page;
  const bool listed = colony_screen_construction_page_lists_current(view);
  view->construction_selection = listed ? view->construction_default_row
                                        : (view->construction_page ? 0x10 : 0);
  view->more_selected = !listed;
}

/*
 * Construction row `gi` (0 = the clear row, -1 = "(More)"): the row text, the cost behind the
 * DS:0xd1d "|   " column tab (FUN_2f2b_5a68), and the port-only queue prefix.
 */
COLONIZE_INTERNAL void colony_screen_construction_row_text(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int gi,
  char* row,
  size_t row_cap,
  char* queue_prefix,
  size_t prefix_cap
) {
  queue_prefix[0] = 0;
  if (gi < 0) {
    snprintf(row, row_cap, "%s", reports_ctitle_word(6)); /* DS:0x93aa "(More)" */
    return;
  }
  if (gi == 0) {
    /* @CTITLE row 6 "(No Production)" (DS:0x93a8, FUN_2f2b_5a68 param -1). */
    snprintf(row, row_cap, "%s", reports_ctitle_word(5));
    return;
  }
  const int bid = view->buildable_ids[gi - 1];
  /* Port-only QOL build queue: a queued (not current) project shows its
   * 1-based queue position in yellow ahead of the name. */
  const int qpos = colony ? colonies_build_queue_pos(pool, colony->id, bid) : 0;
  if (qpos > 0) {
    snprintf(queue_prefix, prefix_cap, "%d ", qpos);
  }
  const ColonizeBuildingType* bt = colonies_building_type(pool, bid);
  const char* name = bt ? bt->name : NULL;
  int hammers = bt ? bt->hammers : 0;
  int tools = bt ? bt->tools_cost : 0;
  if (!bt && !colonies_unit_build_info(bid, &name, &hammers, &tools)) {
    snprintf(row, row_cap, "?");
    return;
  }
  /*
   * DOS-LITERAL FUN_2f2b_5a68 raw 52505-52540 (port_bugs.md #1202, user 2026-10-23 "switch to
   * DOS", superseding #436's "N Hammers M Tools"): the NAME + DS:0x50 " " + DS:0xd1d "|   " + "("
   * + the hammers still needed (cost - colony+0x92, min 0) + " " + @CARGO 16 (DS:0x97e0) +
   * ")", then, when tools are due, "(" + tools + " " + @CARGO 14 (DS:0x97dc) + ")" with no
   * space between the groups. A building the colony already owns (FUN_281f_0cc2 == 1 &&
   * FUN_281f_09fc) gets @MISC 127 (DS:0x2eb8) in place of the hammers and no tools.
   */
  int hammers_left = hammers - (colony ? colony->hammers : 0);
  if (hammers_left < 0) {
    hammers_left = 0;
  }
  /* raw 52508-52509: FUN_1d1d_117e copies the name, FUN_1d1d_0d64 upper-cases a..z. */
  char upper[48];
  snprintf(upper, sizeof(upper), "%s", name ? name : "");
  for (char* p = upper; *p; ++p) {
    if ((unsigned char)(*p - 'a') < 26) {
      *p = (char)(*p - 0x20);
    }
  }
  /* FUN_281f_09fc = FUN_15eb_038e, the +0x84 bit: a built Warehouse Expansion has none and
   * reads its cost (port_bugs.md #1223). */
  const bool owned = bt && colony && colonies_dos_owns_row(pool, colony,
                                                           colonies_building_type_row(pool, bid));
  size_t n = owned ? (size_t)snprintf(row, row_cap, "%s |   (%s)", upper,
                                      reports_misc_display_word(127, ""))
                   : (size_t)snprintf(row, row_cap, "%s |   (%d %s)", upper, hammers_left,
                                      colony_screen_hammers_word());
  if (!owned && tools != 0 && n < row_cap) {
    snprintf(row + n, row_cap - n, "(%d %s)", tools, reports_cargo_display_name(14));
  }
}

static void colony_screen_draw_construction_popup(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->construction_open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  /* port_bugs.md #436 / DOS 2f2b_5bd2: SINGLE column always, paged past 22 rows. */
  const ColonyDosPage pg = colony_screen_construction_page(view);
  const int drawn_rows = colony_screen_dos_page_rows(&pg);
  /*
   * FUN_2f2b_5bd2: FUN_291f_023c(0x800, FONTTINY) box (no @width: box[+0x28] 0x50, box[+0x22]
   * the 4 default), the @CTITLE[4] prose (DS:0x93a6), rows widen it (FUN_6f74_0a00), and it
   * raises DS:0x1f66 (raw 52632) for the footer.
   */
  char row[128];
  char queue_prefix[16];
  int content_w = 0x50;
  for (int k = 0; k < drawn_rows && font; ++k) {
    colony_screen_construction_row_text(view, pool, colony, colony_screen_dos_page_entry(&pg, k),
                                        row, sizeof(row), queue_prefix, sizeof(queue_prefix));
    const int w = colony_screen_dos_row_w(font, row, 4) + font_text_width(font, queue_prefix);
    if (w > content_w) {
      content_w = w;
    }
  }
  /* LABELS.TXT @CTITLE row 5 ("Select An Item To Build") — port_bugs.md #990. port_bugs.md
   * #436's "green" is the box style's ink (@COLORS 68), not EGA 10. */
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, reports_ctitle_word(4), drawn_rows, content_w, 0, true,
    &view->construction_rect
  );
  /* raw 52607-52610: the current project's row (the clear row when nothing is in production)
   * gets FUN_291f_033c's hilite flag as well as the default selection; on a page without it
   * the "(More)" row does (raw 52624 / 52629, DB-63). */
  const int current = colony ? colony->building_in_production : -1;
  const bool more_is_current = !colony_screen_construction_page_lists_current(view);
  for (int k = 0; k < drawn_rows; ++k) {
    const int gi = colony_screen_dos_page_entry(&pg, k);
    const int row_y = colony_screen_dos_row(
      framebuffer, &lay, &view->construction_rect, k,
      gi < 0 ? view->more_selected : !view->more_selected && gi == view->construction_selection
    );
    if (row_y + lay.glyph_h > framebuffer->height || !font) {
      continue;
    }
    colony_screen_construction_row_text(view, pool, colony, gi, row, sizeof(row), queue_prefix,
                                        sizeof(queue_prefix));
    int x = lay.text_x + 4;
    if (queue_prefix[0]) {
      x = popup_draw_text_markup(font, framebuffer, x, row_y, queue_prefix, 14u, 14u, &lay.colors,
                                 NULL);
    }
    const bool is_current = gi < 0    ? more_is_current
                            : gi == 0 ? current < 0
                                      : view->buildable_ids[gi - 1] == current;
    colony_screen_dos_row_text(framebuffer, font, &lay, &view->construction_rect, x, row_y, 4, row,
                               is_current, false);
  }
}

/*
 * FUN_2f2b_348c raw 50694-50708: the jobs / leave-as prose, built the same in both modes:
 * @CTITLE[8] ("Select a Profession for", DS:0x93ae) + ' ' + the colonist's SPECIALTY as
 * @JOB column 1 (FUN_281f_0c54, 0x1c read as 0x13; -0x715c + job * 8) + DS:0x5e "(" + the
 * current occupation's @JOB column 0 + DS:0x60 ")" when it differs and is not 0x13
 * (FUN_281f_0c0e; -0x715e) + DS:0x55 ": ". No space before the "(". Catalog misses stay empty.
 */
static void colony_screen_jobs_header(char* out, size_t cap, int spec, int occ) {
  if (spec == COLONIZE_PROF_FREE_COLONIST) {
    spec = UNITS_JOB_COLONIST;
  }
  const char* spec_name = reports_names_field("JOB", spec, 1);
  int n = snprintf(out, cap, "%s %s", reports_ctitle_word(8), spec_name ? spec_name : "");
  if (n >= 0 && (size_t)n < cap && occ >= 0 && occ != spec && occ != UNITS_JOB_COLONIST) {
    n += snprintf(out + n, cap - (size_t)n, "(%s)", colonies_profession_name(occ));
  }
  if (n >= 0 && (size_t)n < cap) {
    snprintf(out + n, cap - (size_t)n, ": ");
  }
}

/* The jobs / leave-as colonist's specialty and current occupation (-1 = none). */
static void colony_screen_colonist_spec_occ(
  const ColonizeColonyPool* pool, const ColonizeColony* colony, int index, int* spec, int* occ
) {
  *spec = COLONIZE_PROF_FREE_COLONIST;
  *occ = -1;
  if (colony && index >= 0 && index < colony->colonist_count) {
    const ColonizeColonist* sc = &colony->colonists[index];
    *spec = sc->profession;
    *occ = (sc->field_job >= 0)
             ? sc->field_job
             : ((sc->building_type >= 0) ? colonies_building_occupation(pool, sc->building_type)
                                         : -1);
  }
}

/* raw 50805-50806: the specialty's own row (and Dragoon for a Veteran Soldier) gets the
 * FUN_291f_033c hilite flag. */
static bool colony_screen_job_is_specialty(int job, int spec) {
  return job == spec || (job == UNITS_JOB_DRAGOON && spec == UNITS_JOB_SOLDIER);
}

static int colony_screen_jobs_lead(const ColonyScreenView* view) {
  return view->job_count > 0 && view->job_ids[0] == COLONY_JOB_CLEAR_SPECIALTY ? 1 : 0;
}

/* raw 50681-50683: more than 0x10 job rows (local_a) make two pages of 0x10. */
int colony_screen_jobs_page_of(const ColonyScreenView* view, int row) {
  const int lead = colony_screen_jobs_lead(view);
  return view->job_count - lead > 0x10 && row >= lead + 0x10 ? 1 : 0;
}

static ColonyDosPage colony_screen_jobs_page(const ColonyScreenView* view) {
  const int lead = colony_screen_jobs_lead(view);
  return colony_screen_dos_page(view->job_count, lead, view->job_count - lead > 0x10,
                                colony_screen_jobs_page_of(view, view->jobs_selection));
}

/* The 0x62 "(More)" row (raw 50828-50829: local_6 = 1 - local_6): the other page, selected on
 * the occupation's row when that page lists it (FUN_291f_08ec), else on its first row (the
 * first added, FUN_6f74_0a00): page 0's first entry, page 1's "(More)" row (raw 50731-50734). */
void colony_screen_jobs_next_page(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  const int lead = colony_screen_jobs_lead(view);
  if (view->job_count - lead <= 0x10) {
    return;
  }
  const int page = 1 - colony_screen_jobs_page_of(view, view->jobs_selection);
  const bool dflt = colony_screen_jobs_page_of(view, view->jobs_default_row) == page;
  view->jobs_selection = dflt ? view->jobs_default_row : (page ? lead + 0x10 : 0);
  view->more_selected = !dflt && page;
}

/* FUN_6f74_2580 Up / Down over a jobs / construction page's rows: the bar can rest on the
 * "(More)" row (more_selected; *sel keeps a page entry so the page stays). */
static void colony_screen_dos_page_key(
  ColonyScreenView* view, const ColonyDosPage* pg, int* sel, ColonizeKey key, uint32_t greyed
) {
  int k = view->more_selected ? pg->more_at : *sel - pg->start + (pg->more_at == 0 ? 1 : 0);
  /* `greyed` is per entry; the bar skips those rows (FUN_291f_01b6, raw 50801). */
  uint32_t skip = 0;
  for (int r = 0; r < colony_screen_dos_page_rows(pg); ++r) {
    const int e = colony_screen_dos_page_entry(pg, r);
    skip |= e >= 0 && (greyed >> e & 1u) ? 1u << r : 0u;
  }
  popup_list_key_step(key, &k, colony_screen_dos_page_rows(pg), skip);
  const int e = colony_screen_dos_page_entry(pg, k);
  view->more_selected = e < 0;
  if (e >= 0) {
    *sel = e;
  }
}

bool colony_screen_list_key(ColonyScreenView* view, ColonizeKey key) {
  if (!view || !(colonize_key_up(key) || colonize_key_down(key))) {
    return false;
  }
  if (view->jobs_open) {
    const ColonyDosPage pg = colony_screen_jobs_page(view);
    colony_screen_dos_page_key(view, &pg, &view->jobs_selection, key, view->jobs_greyed);
  } else if (view->construction_open) {
    const ColonyDosPage pg = colony_screen_construction_page(view);
    colony_screen_dos_page_key(view, &pg, &view->construction_selection, key, 0);
  } else if (view->eject_open) {
    /* 348c's FUN_291f_01b6 rows (raw 50801, eject_role_enabled false) are skipped. */
    uint32_t greyed = 0;
    for (int i = 0; i < view->eject_role_count; ++i) {
      greyed |= view->eject_role_enabled[i] ? 0u : 1u << i;
    }
    popup_list_key_step(key, &view->eject_selection, view->eject_role_count, greyed);
  } else if (view->dock_orders_open) {
    popup_list_key_step(key, &view->dock_orders_selection, view->dock_orders_count, 0);
  } else if (view->custom_house_open) {
    popup_list_key_step(key, &view->custom_house_selection, view->custom_house_count, 0);
  } else {
    return false;
  }
  return true;
}

/*
 * FUN_2f2b_4424's @CUSTOM box is a FUN_6f74_2580 @checkbox box (FUN_6f74_32a4 flags | 5,
 * port_bugs.md #1213). Space and F1 (0x20 / 0x13b -> 6f74:2cfc, raw 116852-116864) flip the
 * barred row and leave the box up (Enter takes the same arm and closes it, Esc closes it too:
 * game_loop_update.c); any other key is upper-folded and matched against each row's
 * FUN_6f74_0642 hotkey, which FUN_6f74_0a00 takes from the raw @CARGO label before the "~[ "
 * prefix (none in the shipped NAMES.TXT): the first match gets the bar and flips (6f74:2df6,
 * raw 116877-116898).
 */
int colony_screen_custom_house_key(ColonyScreenView* view, const ColonizeInputState* input) {
  if (!view || !view->custom_house_open || !input) {
    return -1;
  }
  const int key = input->last_key == COLONIZE_KEY_SPACE ? 0x20 : popup_input_dos_key(input);
  if (key == 0x20 || key == 0x13b) {
    const int s = view->custom_house_selection;
    return s >= 0 && s < view->custom_house_count ? s : -1;
  }
  for (int i = 0; key && i < view->custom_house_count; ++i) {
    if (popup_hotkey_of(reports_cargo_display_name(view->custom_house_cargo_ids[i])) == key) {
      view->custom_house_selection = i;
      return i;
    }
  }
  return -1;
}

/*
 * FUN_2f2b_348c raw 50758-50778: @JOB column 0 + DS:0xca3 "|   " and, for job < 0x13 other
 * than 0x12, DS:0xca8 " = " + [the yield on the colonist's own plot (aiStack_ec, field jobs,
 * when he works one) + DS:0xcac "/"] + the estimate (aiStack_12c) + ' ' + its @CARGO name
 * (the DS:0x97c0 table, aiStack_c8). `here` NULL = no plot part.
 */
static void colony_screen_jobs_row_text(
  char* out, size_t cap, int job, const int* here, int estimate, int cargo
) {
  size_t n = (size_t)snprintf(out, cap, "%s|   ", reports_job_short_name(job));
  if (job >= UNITS_JOB_COLONIST || job == COLONIES_JOB_TEACHER || n >= cap) {
    return;
  }
  n += (size_t)snprintf(out + n, cap - n, " = ");
  if (here && n < cap) {
    n += (size_t)snprintf(out + n, cap - n, "%d/", *here);
  }
  const char* name = cargo >= 0 ? reports_names_field("CARGO", cargo, 0) : NULL;
  if (n < cap) {
    snprintf(out + n, cap - n, "%d %s", estimate, name ? name : "");
  }
}

COLONIZE_INTERNAL void colony_screen_eject_row_text(
  char* out, size_t cap, int role, int occ, bool gear, int stock_tools
);

static void colony_screen_draw_jobs_popup(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColony* colony,
  const ColonizeCol1Save* col1,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->jobs_open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const ColonyDosPage pg = colony_screen_jobs_page(view);
  const int drawn_rows = colony_screen_dos_page_rows(&pg);

  /* Docks (or an upgrade: Drydock/Shipyard) gates Fisherman yield to 0 —
   * FUN_15eb_18ec:11967-11969. Shared answer, so it cannot drift from
   * turn.c's check the way the AI scorer's copy did (audit E#2). This is why
   * a dockless colony's job list shows "Fisherman = 0 Food" rather than hiding
   * the row: DOS lists the job and answers the pick with @NODOCKS (the port does
   * the same, game_loop.c's two assign paths), so colony_screen_open_jobs
   * deliberately keeps building the row list ungated. */
  const bool has_docks = colony_yield_colony_has_docks(pool, colony);
  const int slot = view->selected_colonist;
  int profession = COLONIZE_PROF_FREE_COLONIST;
  int occ = -1;
  colony_screen_colonist_spec_occ(pool, colony, slot, &profession, &occ);
  /* raw 50600-50609: the colonist's own plot (FUN_281f_0d30 -> FUN_15eb_1646), local_ee. */
  const int plot = (colony && slot >= 0 && slot < colony->colonist_count)
                     ? colonies_colonist_tile(colony, slot)
                     : -1;
  int dx = 0;
  int dy = 0;
  const bool on_plot = map && plot >= 0 && colonies_field_tile_delta(plot, &dx, &dy);
  /* Full production pipeline, like turn.c's field loop — SoL bonus and
   * the colony's SoL latch bits included; the picker used to skip both
   * and could disagree with what the assignment then produced (port_bugs.md:
   * "verify the popup has the proper amounts"). */
  const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);
  char labels[COLONY_JOB_LIST_MAX + 1][80];
  /* FUN_2f2b_348c: FUN_291f_023c(0x800, FONTTINY), box[+0x22] = 8, @width 0xbe = 190
   * (FUN_291f_08d2, raw 50692-50693), rows widen it, DS:0x1f66 raised (raw 50823). */
  int content_w = 0xbe;
  for (int k = 0; k < drawn_rows; ++k) {
    const int e = colony_screen_dos_page_entry(&pg, k);
    const int job = e >= 0 ? view->job_ids[e] : 0;
    if (e < 0) {
      snprintf(labels[k], sizeof(labels[k]), "%s", reports_ctitle_word(6)); /* DS:0x93aa */
    } else if (job == COLONY_JOB_CLEAR_SPECIALTY) {
      /* LABELS.TXT @MISC row 44 — DOS's own jobs-menu entry for the action (DS:0x2e12). */
      snprintf(labels[k], sizeof(labels[k]), "%s", reports_misc_display_word(44, ""));
    } else if (job >= UNITS_JOB_COLONIST) {
      /* A leave-as row (port_bugs.md #1224): the same raw 50758-50794 text as leave-as mode;
       * never the occupation here (he works inside), so the gear part always shows. */
      colony_screen_eject_row_text(labels[k], sizeof(labels[k]), job - UNITS_JOB_COLONIST, occ,
                                   true, colony ? colony->stock[COLONIZE_CARGO_TOOLS] : 0);
    } else {
      int cargo = -1;
      const int estimate =
        ai_euro_jobs_menu_estimate(pool, map, col1, colony, slot, job, &cargo);
      /* Henry Hudson folds into the pipeline (smell audit #60) — same
       * gap/fix as colony_screen_draw_area_overlays above. */
      const int here =
        (on_plot && job < COLONIZE_FIELD_JOB_COUNT)
          ? colony_yield_for_worker(
              map, colony->x + dx, colony->y + dy, job, profession, has_docks, sol_b_field,
              colony->colony_flags,
              col1 && founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON)
            )
          : 0;
      colony_screen_jobs_row_text(
        labels[k], sizeof(labels[k]), job,
        (on_plot && job < COLONIZE_FIELD_JOB_COUNT) ? &here : NULL, estimate, cargo
      );
    }
    if (font) {
      const int w = colony_screen_dos_row_w(font, labels[k], 8);
      if (w > content_w) {
        content_w = w;
      }
    }
  }
  char header[96];
  colony_screen_jobs_header(header, sizeof(header), profession, occ);
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, header, drawn_rows, content_w, 0, true, &view->jobs_rect
  );
  for (int k = 0; k < drawn_rows; ++k) {
    const int e = colony_screen_dos_page_entry(&pg, k);
    const int row_y = colony_screen_dos_row(
      framebuffer, &lay, &view->jobs_rect, k,
      e < 0 ? view->more_selected : !view->more_selected && e == view->jobs_selection
    );
    if (row_y + lay.glyph_h > framebuffer->height) {
      continue;
    }
    colony_screen_dos_row_text(
      framebuffer, font, &lay, &view->jobs_rect, lay.text_x + 8, row_y, 8, labels[k],
      e >= 0 && colony_screen_job_is_specialty(view->job_ids[e], profession),
      e >= 0 && (view->jobs_greyed >> e & 1u)
    );
  }
}

/*
 * The custom house rows are @checkbox rows: FUN_6f74_0a00 prefixes DS:0x1f91 "~[ " and
 * FUN_6f74_1b7c sets the second byte to ']' (on) or '[' (off); in FONTTINY those glyphs
 * are the filled / hollow circles (options_dialog uses the same rows). The '~' is kept:
 * FUN_6f74_0538 draws the circle in the hilite ink (port_bugs.md #1199).
 */
static void colony_screen_custom_house_row(
  const ColonyScreenView* view, const ColonizeColony* colony, int i, char* out, size_t cap
) {
  const int cargo = view->custom_house_cargo_ids[i];
  const bool on = europe_custom_house_cargo_enabled(colony->custom_house_bits, cargo);
  /* NAMES.TXT @CARGO via reports.c's accessor (audit CO-14) — the private
   * copy of the 16 names this replaced ignored a renamed catalog. */
  snprintf(out, cap, "~%c %s", on ? ']' : '[',
           (cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) ? reports_cargo_display_name(cargo) : "?");
}

static void colony_screen_draw_custom_house_popup(
  ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeFont* screen_font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->custom_house_open || !colony || !framebuffer || !framebuffer->pixels) {
    return;
  }
  /* @CUSTOM carries @smallfont, so the rows render in FONTTINY (the screen
   * font); drop the directive from GAME.TXT and they take FONTINTR. */
  const ColonizeFont* font =
    colony_screen_popup_font(view, screen_font, view->custom_house_smallfont);
  const int rows = view->custom_house_count;
  /* FUN_2f2b_4424: the GAME.TXT @CUSTOM box through the generic parser (@width=190,
   * @checkbox, @smallfont, latched at open time; 0x50 without @width), box[+0x22] 4. */
  int content_w = view->custom_house_width > 0 ? view->custom_house_width : 0x50;
  char row[64];
  for (int i = 0; i < rows && font; ++i) {
    colony_screen_custom_house_row(view, colony, i, row, sizeof(row));
    const int w = colony_screen_dos_row_w(font, row, 4);
    if (w > content_w) {
      content_w = w;
    }
  }
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, view->custom_house_title, rows, content_w, 0, false,
    &view->custom_house_rect
  );
  /* One uniform ink (player-reported 2026-08-27: not the brighter green some rows used
   * before; the state is the check glyph's job): the box style's @COLORS 68. */
  for (int i = 0; i < rows; ++i) {
    const int row_y = colony_screen_dos_row(framebuffer, &lay, &view->custom_house_rect, i,
                                            i == view->custom_house_selection);
    colony_screen_custom_house_row(view, colony, i, row, sizeof(row));
    colony_screen_dos_row_text(framebuffer, font, &lay, &view->custom_house_rect, lay.text_x + 4,
                               row_y, 4, row, false, false);
  }
}

/*
 * FUN_2f2b_348c leave-as row (param_1 != 0, port_bugs.md #1203). The occupation row
 * (COLONY_EJECT_ROLE_STAY) is the bare @JOB column 0 name (raw 50743). A role row is @JOB
 * column 0 of 0x13 + role + the DS:0xca3 "|   " tab (raw 50764; no " = " at or past 0x13)
 * and, unless it is the body's own occupation (`gear`), DOS-LITERAL raw 50781-50794: for
 * each cargo of the job's FUN_15eb_0d8e list (Pioneer Tools; Soldier Muskets; Scout Horses;
 * Dragoon Muskets, Horses) " (" + amount + " " + @CARGO name + ")". The amounts are the
 * local_76 table (raw 50569-50573): Horses and Muskets 0x32, Tools clamp(stock / 0x14 * 0x14,
 * 0x14, 100) of the stock after the body's own tools went in.
 */
COLONIZE_INTERNAL void colony_screen_eject_row_text(
  char* out, size_t cap, int role, int occ, bool gear, int stock_tools
) {
  if (role == COLONY_EJECT_ROLE_STAY) {
    snprintf(out, cap, "%s", occ >= 0 ? reports_job_short_name(occ) : "");
    return;
  }
  size_t n = (size_t)snprintf(out, cap, "%s|   ", colonies_eject_role_name(role));
  int cargo[2];
  int count = 0;
  if (role == COLONIZE_EJECT_PIONEER) {
    cargo[count++] = COLONIZE_CARGO_TOOLS;
  }
  if (role == COLONIZE_EJECT_SOLDIER || role == COLONIZE_EJECT_DRAGOON) {
    cargo[count++] = COLONIZE_CARGO_MUSKETS;
  }
  if (role == COLONIZE_EJECT_SCOUT || role == COLONIZE_EJECT_DRAGOON) {
    cargo[count++] = COLONIZE_CARGO_HORSES;
  }
  int tools = stock_tools / 0x14 * 0x14;
  tools = tools < 0x14 ? 0x14 : (tools > 100 ? 100 : tools);
  for (int k = 0; gear && k < count && n < cap; ++k) {
    n += (size_t)snprintf(out + n, cap - n, " (%d %s)",
                          cargo[k] == COLONIZE_CARGO_TOOLS ? tools : 0x32,
                          reports_cargo_display_name(cargo[k]));
  }
}

static void colony_screen_draw_eject_popup(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->eject_open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int rows = view->eject_role_count;
  /* FUN_2f2b_348c's leave-as mode (param_1 != 0): the jobs box and its prose, footer
   * included (the header and DS:0x1f66 are not gated on the mode). */
  int spec = COLONIZE_PROF_FREE_COLONIST;
  int occ = -1;
  const ColonizeUnit* fence_unit =
    view->eject_unit_id >= 0 ? units_get_const(units, view->eject_unit_id) : NULL;
  if (fence_unit) {
    spec = fence_unit->profession;
  } else {
    colony_screen_colonist_spec_occ(pool, colony, view->eject_colonist_index, &spec, &occ);
  }
  char header[96];
  colony_screen_jobs_header(header, sizeof(header), spec, occ);
  /* raw 50553-50571: the fence body's own gear goes into the stock first (only a Pioneer
   * carries tools, FUN_15eb_0d8e), and the Tools amount is read off that stock. */
  int stock_tools = colony ? colony->stock[COLONIZE_CARGO_TOOLS] : 0;
  if (fence_unit && units_type_kind(units_type(units, fence_unit->type_index)) ==
                      UNITS_KIND_PIONEER && fence_unit->tools > 0) {
    stock_tools += fence_unit->tools;
  }
  const int current = fence_unit ? colony_screen_unit_eject_role(units, fence_unit) : -1;
  char labels[COLONIZE_EJECT_ROLE_COUNT + 1][64];
  int content_w = 0xbe;
  for (int i = 0; i < rows; ++i) {
    colony_screen_eject_row_text(labels[i], sizeof(labels[i]), view->eject_roles[i], occ,
                                 view->eject_roles[i] != current, stock_tools);
    const int w = font ? colony_screen_dos_row_w(font, labels[i], 8) : 0;
    if (w > content_w) {
      content_w = w;
    }
  }
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, header, rows, content_w, 0, true, &view->eject_rect
  );
  for (int i = 0; i < rows; ++i) {
    const int row_y =
      colony_screen_dos_row(framebuffer, &lay, &view->eject_rect, i, i == view->eject_selection);
    /* DOS draws a short-stock row greyed rather than dropping it (FUN_15eb_3454 -> 0xffff,
     * raw 50801 FUN_291f_01b6). The roles are @JOB 0x13 + role, DOS's 0x13..0x18 order. */
    /* The occupation row's hilite is raw 50746's `local_136 == local_8` (the specialty is
     * the occupation). DOS bug not reproduced (docs/dos_bugs.md DB-61): DOS reads local_136
     * there before its first write at raw 50799, so the first page-0 draw tests a stale stack
     * word; the port takes the value every later draw sees. */
    const int role = view->eject_roles[i];
    colony_screen_dos_row_text(
      framebuffer, font, &lay, &view->eject_rect, lay.text_x + 8, row_y, 8, labels[i],
      role == COLONY_EJECT_ROLE_STAY ? spec == occ
                                     : colony_screen_job_is_specialty(UNITS_JOB_COLONIST + role, spec),
      !view->eject_role_enabled[i]
    );
  }
}

static void colony_screen_draw_dock_orders_popup(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeFont* screen_font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->dock_orders_open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  /* FUN_2f2b_51ec: the GAME.TXT @COLONYUNIT box through the generic parser (no @smallfont:
   * FONTINTR; @width=190 latched at open time, 0x50 without it; box[+0x22] 4), flags |= 3
   * and one FUN_291f_0230 (FUN_6f74_0f3c) entry with the unit's icon and no text, so the
   * box gains the 22 px icon column; no footer. */
  const ColonizeFont* font = colony_screen_popup_font(view, screen_font, false);
  const int rows = view->dock_orders_count;
  int content_w = view->dock_orders_width > 0 ? view->dock_orders_width : 0x50;
  for (int i = 0; i < rows && font; ++i) {
    const int w = colony_screen_dos_row_w(font, view->dock_orders_labels[i], 4);
    if (w > content_w) {
      content_w = w;
    }
  }
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, view->dock_orders_title, rows, content_w, 22, false,
    &view->dock_orders_rect
  );
  /* FUN_6f74_201e: the icon at (frame + 3 + box[+0x48], frame + 3 + 3), box[+0x46] +
   * box[+0x4a] = 3 being the entry's top. */
  const ColonizeUnit* u = units_get_const(units, view->dock_orders_unit_id);
  const int sprite = (u && view->icons_ok) ? colony_screen_outside_display_sprite(units, u) : -1;
  if (sprite >= 0) {
    /* FUN_112b_01ba draws the orders letter in FONTTINY (DS:0x89e, raw 2171-2173,
     * 2304) whatever the box font is (port_bugs.md #1226). */
    unit_chrome_blit_unit_for_palette(
      framebuffer, screen_font, &view->icons, sprite, view->dock_orders_rect.x + 5,
      view->dock_orders_rect.y + 6, units_display_type_index(units, u->id), u->nation_id,
      u->orders, false, (u->col1_flags15 & 0x80u) != 0,
      (view->frame_ok && view->frame.has_palette) ? &view->frame.palette : NULL
    );
  }
  for (int i = 0; i < rows; ++i) {
    const int row_y = colony_screen_dos_row(
      framebuffer, &lay, &view->dock_orders_rect, i, i == view->dock_orders_selection
    );
    colony_screen_dos_row_text(framebuffer, font, &lay, &view->dock_orders_rect, lay.text_x + 4,
                               row_y, 4, view->dock_orders_labels[i], false, false);
  }
}

/* No DOS builder: nothing opens this popup today (only a unit test sets message_kind).
 * Laid out as a plain compositor box in the dialog font, box[+0x22] 4. */
static void colony_screen_draw_message_popup(
  ColonyScreenView* view,
  const ColonizeFont* screen_font,
  const ColonizeMsgCatalog* labels,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || view->message_kind == COLONY_MSG_NONE || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const ColonizeFont* font = colony_screen_popup_font(view, screen_font, false);
  const int rows = (view->message_kind == COLONY_MSG_CONFIRM) ? 2 : 1;
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, view->message_text, rows, 220, 0, false, &view->message_rect
  );
  for (int i = 0; i < rows; ++i) {
    const int row_y = colony_screen_dos_row(
      framebuffer, &lay, &view->message_rect, i, i == view->message_selection
    );
    const char* label =
      (view->message_kind == COLONY_MSG_OK)
        ? assets_msg_line_or(labels, "MISC", 46, "")
        : (i == 0
             ? view->message_choice0
             : view->message_choice1);
    colony_screen_dos_row_text(framebuffer, font, &lay, &view->message_rect, lay.text_x + 4,
                               row_y, 4, label, false, false);
  }
}

/* ===================== Hit testing & top-level render entry point (colony_screen_hit_test .. colony_screen_render_w) ===================== */

/* Modal list popups: message box, custom house, jobs, eject, dock orders.
 * Each one swallows every click while open (outside = dismiss). */
COLONIZE_INTERNAL bool colony_screen_hit_modal_dialogs(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  const int mx = a->mx;
  const int my = a->my;
  if (view->message_kind != COLONY_MSG_NONE) {
    const int rows = (view->message_kind == COLONY_MSG_CONFIRM) ? 2 : 1;
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->message_rect, rows, mx, my, &idx)) {
      a->hit.kind = COLONY_HIT_MESSAGE_OUTSIDE;
      return true;
    }
    if (idx >= 0) {
      if (view->message_kind == COLONY_MSG_OK) {
        a->hit.kind = COLONY_HIT_MESSAGE_OK;
      } else if (idx == 0) {
        a->hit.kind = COLONY_HIT_MESSAGE_YES;
      } else {
        a->hit.kind = COLONY_HIT_MESSAGE_NO;
      }
    }
    return true;
  }

  if (view->custom_house_open) {
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->custom_house_rect, view->custom_house_count, mx, my, &idx)) {
      a->hit.kind = COLONY_HIT_CUSTOM_HOUSE_OUTSIDE;
      return true;
    }
    if (idx >= 0) {
      a->hit.kind = COLONY_HIT_CUSTOM_HOUSE_ROW;
      a->hit.index = idx;
    }
    return true;
  }

  if (view->jobs_open) {
    const ColonyDosPage pg = colony_screen_jobs_page(view);
    int k = -1;
    if (!colony_screen_dialog_row_hit(&view->jobs_rect, colony_screen_dos_page_rows(&pg), mx, my,
                                      &k)) {
      a->hit.kind = COLONY_HIT_JOBS_OUTSIDE;
      return true;
    }
    const int e = k >= 0 ? colony_screen_dos_page_entry(&pg, k) : -1;
    if (k >= 0 && k == pg.more_at) {
      a->hit.kind = COLONY_HIT_JOBS_MORE;
    } else if (e >= 0 && !(view->jobs_greyed >> e & 1u)) { /* a greyed row is inert */
      a->hit.kind = COLONY_HIT_JOBS_ROW;
      a->hit.index = e;
    }
    return true;
  }

  if (view->eject_open) {
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->eject_rect, view->eject_role_count, mx, my, &idx)) {
      a->hit.kind = COLONY_HIT_EJECT_OUTSIDE;
      return true;
    }
    if (idx >= 0) {
      a->hit.kind = COLONY_HIT_EJECT_ROW;
      a->hit.index = idx;
    }
    return true;
  }

  if (view->dock_orders_open) {
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->dock_orders_rect, view->dock_orders_count, mx, my, &idx)) {
      a->hit.kind = COLONY_HIT_DOCK_ORDERS_OUTSIDE;
      return true;
    }
    if (idx >= 0) {
      a->hit.kind = COLONY_HIT_DOCK_ORDERS_ROW;
      a->hit.index = idx;
    }
    return true;
  }
  return false;
}

/* Construction popup (paged list with the "(More)" row). */
COLONIZE_INTERNAL bool colony_screen_hit_construction_popup(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  if (!view->construction_open) {
    return false;
  }
  const ColonyDosPage pg = colony_screen_construction_page(view);
  int k = -1;
  if (!colony_screen_dialog_row_hit(&view->construction_rect, colony_screen_dos_page_rows(&pg),
                                    a->mx, a->my, &k)) {
    a->hit.kind = COLONY_HIT_CONSTRUCTION_OUTSIDE;
    return true;
  }
  const int gi = colony_screen_dos_page_entry(&pg, k);
  if (k >= 0 && k == pg.more_at) {
    a->hit.kind = COLONY_HIT_CONSTRUCTION_MORE;
    a->hit.index = -1;
  } else if (gi == 0) {
    a->hit.kind = COLONY_HIT_CONSTRUCTION_CLEAR;
    a->hit.index = -1;
  } else if (gi > 0) {
    a->hit.kind = COLONY_HIT_CONSTRUCTION_ROW;
    a->hit.index = gi - 1;
  }
  return true;
}

/* Screen chrome: title bar, exit button, warehouse cargo strip, transport holds. */
COLONIZE_INTERNAL bool colony_screen_hit_chrome(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  const ColonizeUnitPool* units = a->units;
  const int mx = a->mx;
  const int my = a->my;
  /* Title bar: thunk_FUN_1000_98bc rect (0,0,0x140,7) = zone 10, the rename
   * click (thunk_FUN_1000_9898). */
  if (mx >= 0 && mx < COLONY_SCREEN_WIDTH && my >= 0 && my < 7) {
    a->hit.kind = COLONY_HIT_TITLE;
    return true;
  }

  if (mx >= COLONY_EXIT_X && mx < COLONY_SCREEN_WIDTH &&
      my >= COLONY_EXIT_Y && my < COLONY_SCREEN_HEIGHT) {
    a->hit.kind = COLONY_HIT_EXIT;
    return true;
  }

  /* Warehouse cargo strip (load into selected transport). */
  if (my >= COLONY_CARGO_STRIP_Y && my < COLONY_SCREEN_HEIGHT && mx < COLONY_EXIT_X) {
    if (mx >= COLONY_CARGO_SLOT_X0) {
      const int idx = (mx - COLONY_CARGO_SLOT_X0) / COLONY_CARGO_PITCH;
      if (idx >= 0 && idx < COLONIZE_CARGO_COUNT) {
        a->hit.kind = COLONY_HIT_CARGO_SLOT;
        a->hit.index = idx;
        return true;
      }
    }
  }

  /* Goods holds of selected transport. */
  if (units && view->transport_unit_id >= 0 && my >= COLONY_HOLD_Y &&
      my < COLONY_HOLD_Y + COLONY_HOLD_H) {
    const int holds = units_goods_hold_count(units, view->transport_unit_id);
    if (mx >= COLONY_HOLD_X && holds > 0) {
      /* Bucket by pitch, not by icon width: the 3px gaps between hold covers
       * stay live so a drop landing between two holds picks the nearer one
       * (player-reported: drop boxes too strict). Matches EUROPE_HIT_HOLD. */
      const int idx = (mx - COLONY_HOLD_X) / COLONY_HOLD_PITCH;
      if (idx >= 0 && idx < holds) {
        a->hit.kind = COLONY_HIT_HOLD;
        a->hit.index = idx;
        return true;
      }
    }
  }
  return false;
}

/* Multifunction panel: mode buttons, Units-tab icons, pane / BUY+CHANGE. */
COLONIZE_INTERNAL bool colony_screen_hit_multifunction(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  const ColonizeColony* colony = a->colony;
  const ColonizeUnitPool* units = a->units;
  const int mx = a->mx;
  const int my = a->my;
  /* Multifunction mode buttons. */
  if (mx >= COLONY_MULTI_BTN_X && mx < COLONY_MULTI_BTN_X + COLONY_MULTI_BTN_W &&
      my >= COLONY_PANEL_CONTENT_Y && my < COLONY_PANEL_CONTENT_Y + 48) {
    const int idx = (my - COLONY_PANEL_CONTENT_Y) / 16;
    if (idx >= 0 && idx < 3) {
      a->hit.kind = COLONY_HIT_MULTI_BTN;
      a->hit.index = idx;
      return true;
    }
  }

  /* Units-tab unit icons (checked before the generic multi-pane catch-all
   * below; hit.index is the unit id, not an array index — callers act on it
   * directly without recomputing the layout). */
  if (view->multi_mode == COLONY_MULTI_UNITS && units && mx >= COLONY_MULTI_X &&
      mx < COLONY_MULTI_BTN_X && my >= COLONY_PANEL_CONTENT_Y && my < COLONY_CARGO_STRIP_Y) {
    ColonyMultiUnitSlot slots[COLONY_MULTI_UNITS_SLOT_MAX];
    const int px = COLONY_MULTI_X + 2;
    const int pane_w = COLONY_MULTI_W - 19;
    const int py = COLONY_PANEL_CONTENT_Y;
    const int pane_h = COLONY_PANEL_CONTENT_H;
    const int slot_count = colony_screen_multi_units_layout(
      view, units, px, py + COLONY_MULTI_UNITS_TITLE_H, pane_w, pane_h - COLONY_MULTI_UNITS_TITLE_H,
      slots, COLONY_MULTI_UNITS_SLOT_MAX
    );
    for (int i = 0; i < slot_count; ++i) {
      if (mx >= slots[i].x && mx < slots[i].x + slots[i].w && my >= slots[i].y &&
          my < slots[i].y + slots[i].h) {
        a->hit.kind = COLONY_HIT_MULTI_UNIT_ICON;
        a->hit.index = slots[i].unit_id;
        return true;
      }
    }
  }

  /* Multifunction pane / Construction BUY+CHANGE. */
  if (mx >= COLONY_MULTI_X && mx < COLONY_MULTI_BTN_X && my >= COLONY_PANEL_CONTENT_Y &&
      my < COLONY_CARGO_STRIP_Y) {
    /* Player-reported: BUY moved up 4px to align with CHANGE — hit region
     * follows (was [10,26), now [6,22)). */
    if (view->multi_mode == COLONY_MULTI_CONSTRUCTION &&
        my >= COLONY_PANEL_CONTENT_Y + 6 && my < COLONY_PANEL_CONTENT_Y + 22) {
      const int mid = COLONY_MULTI_X + COLONY_MULTI_W / 2;
      if (mx >= mid) {
        a->hit.kind = COLONY_HIT_MULTI_CHANGE;
        return true;
      }
      /* FUN_2f2b_5fc6: the BUY button only hit-tests when +0x94 >= 0 (no
       * project, e.g. a captured AI colony's 0xFF, has no BUY; port_bugs.md #548). */
      if (colony->building_in_production >= 0) {
        a->hit.kind = COLONY_HIT_MULTI_BUY;
        return true;
      }
    }
    a->hit.kind = COLONY_HIT_MULTI_PANE;
    a->hit.index = (int)view->multi_mode;
    return true;
  }
  return false;
}

/* Docked transport icons. */
COLONIZE_INTERNAL bool colony_screen_hit_transports(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  const int mx = a->mx;
  const int my = a->my;
  /* Docked transport icons. */
  if (view->docked_transport_count > 0 && my >= COLONY_TRANSPORT_ICON_Y &&
      my < COLONY_TRANSPORT_ICON_Y + 16 && mx >= COLONY_TRANSPORT_X &&
      mx < COLONY_TRANSPORT_X + COLONY_TRANSPORT_W) {
    int tr_pitch = COLONY_TRANSPORT_PITCH;
    if (view->docked_transport_count > 1) {
      const int avail = COLONY_TRANSPORT_W - 8 - 16;
      if ((view->docked_transport_count - 1) * tr_pitch > avail) {
        tr_pitch = avail / (view->docked_transport_count - 1);
        if (tr_pitch < 2) {
          tr_pitch = 2;
        }
      }
    }
    /* Walk back-to-front so overlapped (squeezed) icons resolve to the
     * one drawn on top. */
    int idx = -1;
    for (int i = view->docked_transport_count - 1; i >= 0; --i) {
      const int x0 = COLONY_TRANSPORT_X + 4 + i * tr_pitch;
      if (mx >= x0 && mx < x0 + 16) {
        idx = i;
        break;
      }
    }
    if (idx >= 0 && idx < view->docked_transport_count) {
      a->hit.kind = COLONY_HIT_TRANSPORT;
      a->hit.index = idx;
      return true;
    }
  }
  return false;
}

/* Fortification strip: outside units and the empty fence (eject target). */
COLONIZE_INTERNAL bool colony_screen_hit_fence(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  const ColonizeColonyPool* pool = a->pool;
  const ColonizeColony* colony = a->colony;
  const ColonizeUnitPool* units = a->units;
  const int mx = a->mx;
  const int my = a->my;
  const int* slot_x = a->slot_x;
  const int* slot_y = a->slot_y;
  /* Outside units on fortification strip (Note 1; per-icon selectable). */
  if (view->outside_unit_count > 0 && units && view->icons_ok) {
    int fence_x, fence_y, fence_w, fence_h;
    colony_screen_fence_rect(
      view, pool, colony, slot_x, slot_y, &fence_x, &fence_y, &fence_w, &fence_h
    );
    int icons[COLONY_OUTSIDE_MAX];
    int map_i[COLONY_OUTSIDE_MAX];
    int n = 0;
    for (int i = 0; i < view->outside_unit_count && n < COLONY_OUTSIDE_MAX; ++i) {
      const ColonizeUnit* u = units_get_const(units, view->outside_unit_ids[i]);
      if (!colony_screen_unit_on_fence(units, u)) {
        continue;
      }
      const int sprite = colony_screen_outside_display_sprite(units, u);
      if (sprite < 0) {
        continue;
      }
      icons[n] = sprite;
      map_i[n] = i;
      n++;
    }
    if (n > 0) {
      int ref_iw = 12;
      if (icons[0] >= 0 && icons[0] < view->icons.sprite_count) {
        ref_iw = view->icons.sprites[icons[0]].width;
      }
      int xs[COLONY_OUTSIDE_MAX];
      colony_screen_icon_strip_layout(fence_x, fence_w, n, ref_iw, xs);
      /* port_bugs.md: walk BACK-to-front — squeezed icons overlap, and the one
       * drawn on top (later index) must win the click, or the units behind
       * the pile were unselectable. */
      for (int i = n - 1; i >= 0; --i) {
        int iw = 12;
        int ih = 16;
        colony_screen_outside_icon_metrics(view, units, view->outside_unit_ids[map_i[i]], &iw, &ih);
        const int uy = fence_y + (fence_h - ih) / 2;
        if (mx >= xs[i] && my >= uy && mx < xs[i] + iw && my < uy + ih) {
          a->hit.kind = COLONY_HIT_OUTSIDE_UNIT;
          a->hit.index = map_i[i];
          return true;
        }
      }
    }
    /* Empty fortification strip (eject target when a colony colonist is selected). */
    if (mx >= fence_x && my >= fence_y && mx < fence_x + fence_w && my < fence_y + fence_h) {
      a->hit.kind = COLONY_HIT_FENCE;
      return true;
    }
  } else if (view->buildings_ok) {
    /* No outside units: still allow fence clicks for eject. */
    int fence_x, fence_y, fence_w, fence_h;
    colony_screen_fence_rect(
      view, pool, colony, slot_x, slot_y, &fence_x, &fence_y, &fence_w, &fence_h
    );
    if (mx >= fence_x && my >= fence_y && mx < fence_x + fence_w && my < fence_y + fence_h) {
      a->hit.kind = COLONY_HIT_FENCE;
      return true;
    }
  }
  return false;
}

/* Building worker strips (before whole-building hit). */
COLONIZE_INTERNAL bool colony_screen_hit_building_workers(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  const ColonizeColonyPool* pool = a->pool;
  const ColonizeColony* colony = a->colony;
  const ColonizeUnitPool* units = a->units;
  const int mx = a->mx;
  const int my = a->my;
  const int* slot_x = a->slot_x;
  const int* slot_y = a->slot_y;
  /* Building workers (Note 1 strip) — before whole-building hit. */
  if (pool && view->buildings_ok && units && mx >= COLONY_VIEWPORT_X &&
      mx < COLONY_VIEWPORT_X + COLONY_VIEWPORT_W && my >= COLONY_VIEWPORT_Y &&
      my < COLONY_BOTTOM_SEPARATOR_Y) {
    const int slot_ox = COLONY_VIEWPORT_X;
    const int slot_oy = COLONY_VIEWPORT_Y;
    for (int i = 0; i < colony_screen_building_slot_count; ++i) {
      const int built = colony_screen_category_built(pool, colony, i);
      if (built < 0 || built >= view->buildings.sprite_count) {
        continue;
      }
      const ColonizeSprite* bspr = &view->buildings.sprites[built];
      if (!bspr || bspr->width <= 2 || bspr->height <= 2) {
        continue;
      }
      int worker_ci[COLONIZE_COLONY_POP_MAX];
      int worker_icons[COLONIZE_COLONY_POP_MAX];
      int strip_h = 16;
      const int workers = colony_screen_building_worker_strip(
        view, colony, units, built, worker_ci, worker_icons, &strip_h
      );
      if (workers <= 0) {
        continue;
      }
      const int bx = slot_ox + slot_x[i];
      const int by = slot_oy + slot_y[i];
      const int strip_y = by + bspr->height - strip_h;
      int ref_iw = 12;
      if (worker_icons[0] < view->icons.sprite_count) {
        ref_iw = view->icons.sprites[worker_icons[0]].width;
      }
      int xs[COLONIZE_COLONY_POP_MAX];
      int span_x = bx;
      int span_w = bspr->width;
      colony_screen_building_strip_span(bx, bspr->width, workers, ref_iw, &span_x, &span_w);
      colony_screen_icon_strip_layout(span_x, span_w, workers, ref_iw, xs);
      for (int wi = 0; wi < workers; ++wi) {
        int iw = ref_iw;
        int ih = strip_h;
        if (worker_icons[wi] < view->icons.sprite_count) {
          iw = view->icons.sprites[worker_icons[wi]].width;
          ih = view->icons.sprites[worker_icons[wi]].height;
        }
        const int iy = strip_y + (strip_h - ih) / 2;
        if (mx >= xs[wi] && my >= iy && mx < xs[wi] + iw && my < iy + ih) {
          a->hit.kind = COLONY_HIT_COLONIST; /* a building worker (FUN_2f2b_44d4 local_10) */
          a->hit.index = worker_ci[wi];
          return true;
        }
      }
    }
  }
  return false;
}

/* Area-view minimap tiles. */
COLONIZE_INTERNAL bool colony_screen_hit_area_tiles(ColonyScreenHitCtx* a) {
  const int mx = a->mx;
  const int my = a->my;
  /* Area-view tiles (surround only; center is not assignable). */
  int origin_x = 0;
  int origin_y = 0;
  colony_screen_minimap_origin(&origin_x, &origin_y);
  const int grid_px = COLONY_MINIMAP_GRID * COLONY_MINIMAP_TILE;
  if (mx >= origin_x && my >= origin_y && mx < origin_x + grid_px && my < origin_y + grid_px) {
    const int col = (mx - origin_x) / COLONY_MINIMAP_TILE;
    const int row = (my - origin_y) / COLONY_MINIMAP_TILE;
    const int half = COLONY_MINIMAP_GRID / 2;
    const int dx = col - half;
    const int dy = row - half;
    const int ti = colonies_field_tile_index(dx, dy);
    if (ti >= 0) {
      a->hit.kind = COLONY_HIT_AREA_TILE;
      a->hit.index = ti;
      return true;
    }
    /* Centre tile (not assignable): DOS's click-to-toggle-numbers spot
     * (2f2b:609e flips DS:0x336 and refreshes; port_bugs.md). */
    a->hit.kind = COLONY_HIT_AREA_INTERIOR;
    return true;
  }
  return false;
}

/* People band. */
COLONIZE_INTERNAL bool colony_screen_hit_people(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  const ColonizeColony* colony = a->colony;
  const ColonizeUnitPool* units = a->units;
  const int mx = a->mx;
  const int my = a->my;
  /* People-view: shares colony_screen_people_layout with the draw. */
  if (colony && my >= COLONY_PANEL_CONTENT_Y + 16 && my < COLONY_PANEL_CONTENT_Y + 32 &&
      mx >= COLONY_PEOPLE_X && mx < COLONY_PEOPLE_X + COLONY_PEOPLE_W) {
    PeopleEntry ent[COLONIZE_COLONY_POP_MAX + COLONY_OUTSIDE_MAX];
    const int n = colony_screen_people_layout(
      view, colony, units, ent, (int)(sizeof(ent) / sizeof(ent[0]))
    );
    /* Walk back-to-front so an overlapped (squeezed) icon resolves to the
     * one drawn on top. */
    for (int i = n - 1; i >= 0; --i) {
      if (mx >= ent[i].px && mx < ent[i].px + ent[i].iw) {
        if (ent[i].sel_colonist >= 0) {
          a->hit.kind = COLONY_HIT_PEOPLE_COLONIST;
          a->hit.index = ent[i].sel_colonist;
        } else {
          a->hit.kind = COLONY_HIT_OUTSIDE_UNIT;
          /* callers expect the outside ARRAY index */
          a->hit.index = -1;
          for (int oi = 0; oi < view->outside_unit_count; ++oi) {
            if (view->outside_unit_ids[oi] == ent[i].sel_unit) {
              a->hit.index = oi;
              break;
            }
          }
          if (a->hit.index < 0) {
            continue;
          }
        }
        return true;
      }
    }
  }
  return false;
}

/* Whole-building click: DOS FUN_2f2b_44d4 slot scan. */
COLONIZE_INTERNAL bool colony_screen_hit_buildings(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  const ColonizeColonyPool* pool = a->pool;
  const ColonizeColony* colony = a->colony;
  const int mx = a->mx;
  const int my = a->my;
  const int* slot_x = a->slot_x;
  const int* slot_y = a->slot_y;
  const int* slot_of_cat = a->slot_of_cat;
  if (pool && view->buildings_ok && mx >= COLONY_VIEWPORT_X &&
      mx < COLONY_VIEWPORT_X + COLONY_VIEWPORT_W && my >= COLONY_VIEWPORT_Y &&
      my < COLONY_BOTTOM_SEPARATOR_Y) {
    const int slot_ox = COLONY_VIEWPORT_X;
    const int slot_oy = COLONY_VIEWPORT_Y;
    /*
     * port_bugs.md #430 — DOS's own building-slot scan, FUN_2f2b_44d4
     * (viceroy_unpacked.c:51139, loop 2f2b:45ac..45b5):
     *
     *   local_e = 0;
     *   while (local_e < 0xf && local_4 < 0) {
     *     x = [0x266 + local_e*4]; y = [0x268 + local_e*4] + 8;
     *     cls = [0x8d62 + local_e];
     *     if (FUN_281f_03ca(cursor, x, y, [0x230+cls], [0x236+cls]))
     *       local_4 = (char)[0x8e82 + local_e];   // -1 when unbuilt
     *     local_e++;
     *   }
     *
     * Four things that matter, all of which this port had wrong:
     *
     *  1. It walks SLOT POSITIONS 0..14 ascending, not categories. Only one
     *     of the 105 slot pairs overlaps at all — slot 8 (class 1,
     *     (128,53)-(171,74)) under slot 14 (the class-4 dock corner,
     *     (123,55)-(197,102)) — and slot 8 comes first, so whatever class-1
     *     building the shuffle put there beats the dock corner.
     *  2. The rect is the SIZE-CLASS BOX (DS:0x230 / DS:0x236 = colony_screen_class_box),
     *     not the sprite struct's dims — which is moot in BUILDING.SS, where
     *     every real building sprite is exactly its class box, but not moot
     *     for the placeholder trees.
     *  3. There is no per-pixel test: FUN_281f_03ca -> FUN_1262_00f6 is a
     *     bare inclusive AABB. Transparent margin is live click area.
     *  4. `local_4 < 0` keeps the scan RUNNING past a slot whose category is
     *     unbuilt (DS:0x8e82[slot] stays 0xff), so a placeholder never
     *     swallows a click — the scan falls through to the next slot, and if
     *     nothing matches DOS returns having done nothing at all: no popup,
     *     no status, no sound.
     *
     * (4) is the reported bug. Montreal in missing_carpenters.SAV has its
     * Carpenter's Shop in slot 8, under the unbuilt Docks category's coast
     * placeholder in slot 14; the port's old category-ascending scan matched
     * Docks (category 2) first, took its -1, and answered a plainly-visible,
     * long-owned Carpenter's Shop with "Build it first".
     *
     * The one exception DOS carves out: FUN_2f2b_0434's writer is
     * `if (owned(b) || b == 0)`, so building 0 (Stockade) is registered in
     * the class-3 fence corner even when unbuilt — that corner is always a
     * live target and always resolves to the Stockade. Mirrored below,
     * though in practice this port's earlier COLONY_HIT_FENCE branch already
     * claims that rect and returns first; the arm is here so the fence slot
     * is never mistaken for an unbuilt one if that branch ever narrows.
     */
    for (int slot = 0; slot < COLONY_DOS_SLOT_COUNT; ++slot) {
      for (int i = 0; i < colony_screen_building_slot_count; ++i) {
        if (slot_of_cat[i] != slot) {
          continue;
        }
        int built = colony_screen_category_built(pool, colony, i);
        if (built < 0 && i == COLONY_CAT_FORTIFICATION) {
          built = colonies_building_row(pool, COLONY_BUILDING_STOCKADE); /* DOS's `|| b == 0` */
        }
        if (built < 0) {
          break; /* unbuilt slot: DOS keeps scanning the later slots */
        }
        const int cls = colony_screen_building_slots[i].size_class;
        const int bx = slot_ox + slot_x[i];
        const int by = slot_oy + slot_y[i];
        if (mx < bx || mx >= bx + colony_screen_class_box[cls][0] || my < by ||
            my >= by + colony_screen_class_box[cls][1]) {
          break;
        }
        a->hit.kind = COLONY_HIT_BUILDING;
        a->hit.index = built;
        return true;
      }
    }
  }
  return false;
}

ColonyScreenHitResult colony_screen_hit_test(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  int mx,
  int my
) {
  ColonyScreenHitCtx a;
  a.hit.kind = COLONY_HIT_NONE;
  a.hit.index = -1;
  if (!view || !colony) {
    return a.hit;
  }
  a.view = view;
  a.pool = pool;
  a.colony = colony;
  a.units = units;
  a.mx = mx;
  a.my = my;

  /* Shared with both building-related hit checks below — must match
   * colony_screen_blit_buildings' own call so click regions never drift
   * from what's drawn (same per-colony-deterministic assignment).
   * slot_of_cat[] is the DS:0x266 SLOT each category landed in: DOS scans
   * slots, not categories (see colony_screen_hit_buildings). */
  colony_screen_assign_slot_positions_ex(
    pool, colony, a.slot_x, a.slot_y, a.slot_of_cat, colony_screen_layout_seed(view)
  );

  /* Hit priority = stage order; the first stage returning true owns the click. */
  static bool (*const stages[])(ColonyScreenHitCtx*) = {
    colony_screen_hit_modal_dialogs,
    colony_screen_hit_construction_popup,
    colony_screen_hit_chrome,
    colony_screen_hit_multifunction,
    colony_screen_hit_transports,
    colony_screen_hit_fence,
    colony_screen_hit_building_workers,
    colony_screen_hit_area_tiles,
    colony_screen_hit_people,
    colony_screen_hit_buildings,
  };
  for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); ++i) {
    if (stages[i](&a)) {
      break;
    }
  }
  return a.hit;
}

void colony_screen_render_w(
  const ColonizeWorld* w,
  ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  const ColonizeFont* font,
  bool debug_building_rects,
  const ColonizeMsgCatalog* labels,
  ColonizeFramebuffer8* framebuffer
) {
  const ColonizeColonyPool* pool = w->colonies;
  const ColonizeUnitPool* units = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeCol1Save* col1 = w->col1;

  if (!framebuffer || !framebuffer->pixels) {
    return;
  }
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);

  if (view && view->frame_ok) {
    pik_blit(&view->frame, framebuffer, 0, 0);
  }

  if (view && colony && units) {
    colony_screen_refresh_transports(view, units, colony);
    colony_screen_refresh_outside(view, units, colony);
  }
  if (view && pool && colony) {
    colony_screen_refresh_preview_w(w, view, colony);
  }

  colony_screen_fill_top_bar_wood(view, framebuffer);
  colony_screen_draw_top_bar(colony, game_year, game_autumn, gold, view, font, framebuffer);

  colony_screen_fill_parch(view, framebuffer);
  colony_screen_blit_buildings(view, pool, colony, units, col1, font, debug_building_rects, framebuffer);

  colony_screen_fill_wood_tile(view, framebuffer);
  if (colony && map && terrain) {
    colony_screen_render_minimap(
      w, map, terrain, phys0, (view && view->icons_ok) ? &view->icons : NULL,
      (view && view->frame_ok && view->frame.has_palette) ? &view->frame.palette : NULL,
      colony->x, colony->y, framebuffer
    );
    colony_screen_draw_area_overlays(view, pool, colony, units, map, col1, font, framebuffer);
  }
  if (view && view->bottom_panel_ok) {
    pik_blit(&view->bottom_panel, framebuffer, 0, COLONY_BOTTOM_PANEL_Y);
  }

  colony_screen_draw_hline(framebuffer, COLONY_TOP_SEPARATOR_Y, 0);
  colony_screen_draw_hline(framebuffer, COLONY_BOTTOM_SEPARATOR_Y, 0);
  /* Golden: divider column sits at x=199, directly left of the wood section. */
  colony_screen_draw_vline(
    framebuffer,
    COLONY_MINIMAP_SECTION_X - 1,
    COLONY_MIDDLE_Y,
    COLONY_BOTTOM_SEPARATOR_Y - 1,
    0
  );

  if (view) {
    colony_screen_draw_people(view, colony, units, col1, font, framebuffer);
    colony_screen_draw_transports(view, units, font, framebuffer);
    colony_screen_draw_multifunction(view, pool, colony, units, font, labels, framebuffer);
  }

  if (colony) {
    colony_screen_draw_cargo_strip(view, pool, colony, font, framebuffer);
  }

  if (view && view->construction_open) {
    colony_screen_draw_construction_popup(view, pool, colony, font, framebuffer);
  }
  if (view && view->jobs_open) {
    colony_screen_draw_jobs_popup(view, pool, map, colony, col1, font, framebuffer);
  }
  if (view && view->eject_open) {
    colony_screen_draw_eject_popup(view, pool, colony, units, font, framebuffer);
  }
  if (view && view->custom_house_open && colony) {
    colony_screen_draw_custom_house_popup(view, colony, font, framebuffer);
  }
  if (view && view->dock_orders_open) {
    colony_screen_draw_dock_orders_popup(view, units, font, framebuffer);
  }
  if (view && view->message_kind != COLONY_MSG_NONE) {
    colony_screen_draw_message_popup(view, font, labels, framebuffer);
  }

  if (view && font) {
    if (!view->frame_ok) {
      font_draw_text(font, framebuffer, 4, 100, "WOODPANL.PIK failed to load", 12);
    }
    if (!view->buildings_ok) {
      font_draw_text(font, framebuffer, 4, 112, "BUILDING.SS failed to load", 12);
    }
  }
}
