#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
} ColonyDosLayout;

#define COLONY_DOS_PROSE_LINES 4

static ColonyDosLayout colony_screen_open_dos_dialog(
  ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
  const char* prose,
  uint8_t prose_ink,
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
  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  popup_draw(
    framebuffer, pl.x, pl.y, w, h, view->wood_tile_ok ? &view->wood_tile : NULL, &colors, NULL,
    NULL, NULL, NULL
  );
  lay.text_x = pl.x + 5 + icon_w;
  lay.bar_x = pl.x + 4 + icon_w;
  lay.bar_w = w - 8 - icon_w;
  bool hi = false;
  for (int i = 0; i < n; ++i) {
    popup_draw_text_markup(
      font, framebuffer, lay.text_x, pl.y + 6 + i * (lay.glyph_h + 1), lines[i], prose_ink,
      COLONIZE_COL_HILITE, &colors, &hi
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

/* FUN_6f74_0a00 row width: text + 2 * box[+0x48] + box[+0x22], + box[+0x22] again for a
 * row carrying the '|' column tab (DS:0xd1d "|   "). */
static int colony_screen_dos_row_w(const ColonizeFont* font, const char* text, int row_dx,
                                   const char* tab_column) {
  int w = font_text_width(font, text) + 4 + row_dx;
  if (tab_column && tab_column[0]) {
    w += font_text_width(font, "   ") + font_text_width(font, tab_column) + row_dx;
  }
  return w;
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

/* Construction row `gi` (0 = the clear row): label, its queue prefix and the cost column. */
static void colony_screen_construction_row_text(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int gi,
  char* label,
  size_t label_cap,
  char* queue_prefix,
  size_t prefix_cap,
  char* cost,
  size_t cost_cap
) {
  cost[0] = 0;
  queue_prefix[0] = 0;
  if (gi == 0) {
    /* @CTITLE row 6 "(No Production)" (DS:0x93a8, FUN_2f2b_5a68 param -1). */
    snprintf(label, label_cap, "%s", reports_ctitle_word(5));
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
  const char* uname = NULL;
  int uh = 0;
  int ut = 0;
  /* Player-reported: hammers shown here are what's still *needed*, i.e.
   * the requirement adjusted down by the colony's already-banked
   * hammers (min 0) — hammers carry over to whatever project is picked,
   * unlike tools, which are never adjusted away in this popup.
   * Cost words are DOS's own (2f2b_5a68 string build), right-aligned
   * behind the "|   " tab (port_bugs.md #436). */
  const int stored_hammers = colony ? colony->hammers : 0;
  if (bt) {
    int hammers_left = bt->hammers - stored_hammers;
    if (hammers_left < 0) {
      hammers_left = 0;
    }
    snprintf(label, label_cap, "%s", bt->name);
    /* NAMES.TXT @CARGO row 16 "Hammers", row 14 "Tools". */
    if (bt->tools_cost > 0) {
      snprintf(
        cost, cost_cap, "%d %s %d %s", hammers_left, colony_screen_hammers_word(),
        bt->tools_cost, reports_cargo_display_name(14)
      );
    } else {
      snprintf(cost, cost_cap, "%d %s", hammers_left, colony_screen_hammers_word());
    }
  } else if (colonies_unit_build_info(bid, &uname, &uh, &ut)) {
    /* Artillery (colonies_unit_build_info) — not a real @BUILDING row. */
    int hammers_left = uh - stored_hammers;
    if (hammers_left < 0) {
      hammers_left = 0;
    }
    snprintf(label, label_cap, "%s", uname);
    snprintf(
      cost, cost_cap, "%d %s %d %s", hammers_left, colony_screen_hammers_word(), ut,
      reports_cargo_display_name(14)
    );
  } else {
    snprintf(label, label_cap, "?");
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
  const int rows = view->buildable_count + 1; /* Clear + projects (Buy is multifunction) */
  /* port_bugs.md #436 / DOS 2f2b_5bd2: SINGLE column always. Past 0x16 = 22 rows
   * the DOS picker splits into PAGES of 0x10 = 16 rows chained with a
   * "More..." row — never side-by-side columns. */
  const int rows_per_page = rows > 22 ? 16 : rows;
  const int pages = rows_per_page > 0 ? (rows + rows_per_page - 1) / rows_per_page : 1;
  if (view->construction_selection >= 0 && rows_per_page > 0) {
    view->construction_page = view->construction_selection / rows_per_page;
  }
  if (view->construction_page < 0 || view->construction_page >= pages) {
    view->construction_page = 0;
  }
  const int page = view->construction_page;
  const int start = page * rows_per_page;
  int n_slice = rows - start;
  if (n_slice > rows_per_page) {
    n_slice = rows_per_page;
  }
  const int drawn_rows = n_slice + (pages > 1 ? 1 : 0); /* +More... */
  /*
   * FUN_2f2b_5bd2: FUN_291f_023c(0x800, FONTTINY) box (no @width: box[+0x28] 0x50, box[+0x22]
   * the 4 default), the @CTITLE[4] prose (DS:0x93a6), rows widen it (FUN_6f74_0a00), and it
   * raises DS:0x1f66 (raw 52632) for the footer.
   */
  char label[80];
  char cost[48];
  char queue_prefix[16];
  int content_w = 0x50;
  for (int i = 0; i < drawn_rows && font; ++i) {
    if (i >= n_slice) {
      snprintf(label, sizeof(label), "%s", reports_ctitle_word(6));
      cost[0] = queue_prefix[0] = 0;
    } else {
      colony_screen_construction_row_text(view, pool, colony, start + i, label, sizeof(label),
                                          queue_prefix, sizeof(queue_prefix), cost, sizeof(cost));
    }
    const int w =
      colony_screen_dos_row_w(font, label, 4, cost) + font_text_width(font, queue_prefix);
    if (w > content_w) {
      content_w = w;
    }
  }
  /* port_bugs.md #436: DOS draws this picker's text GREEN (the standard menu ink),
   * and the "|   " (DS:0xd1d) tab in 2f2b_5a68 right-justifies the cost. */
  const uint8_t ink = 10u;
  /* LABELS.TXT @CTITLE row 5 ("Select An Item To Build") — port_bugs.md #990. */
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, reports_ctitle_word(4), ink, drawn_rows, content_w, 0, true,
    &view->construction_rect
  );
  view->construction_rows_per_col = rows_per_page;
  view->construction_col_w = view->construction_rect.w - 6;

  for (int i = 0; i < drawn_rows; ++i) {
    const int gi = start + i; /* global row (More... row falls past rows) */
    const bool more_row = (i >= n_slice);
    const bool selected = (!more_row && gi == view->construction_selection);
    const int row_y =
      colony_screen_dos_row(framebuffer, &lay, &view->construction_rect, i, selected);
    if (row_y + lay.glyph_h > framebuffer->height) {
      continue;
    }
    if (more_row) {
      /* @CTITLE row 7 "(More)"; the clear row is row 6 "(No Production)". */
      snprintf(label, sizeof(label), "%s", reports_ctitle_word(6));
      cost[0] = queue_prefix[0] = 0;
    } else {
      colony_screen_construction_row_text(view, pool, colony, gi, label, sizeof(label),
                                          queue_prefix, sizeof(queue_prefix), cost, sizeof(cost));
    }
    if (font) {
      int label_x = lay.text_x + 4;
      if (queue_prefix[0]) {
        font_draw_text(font, framebuffer, label_x, row_y, queue_prefix, 14u); /* DOS 0xe */
        label_x += font_text_width(font, queue_prefix);
      }
      font_draw_text(font, framebuffer, label_x, row_y, label, ink);
      if (cost[0]) {
        /* The '|' column ends at frame + w - 5 - box[+0x22] (the Europe shots measure
         * frame + w - 13 with box[+0x22] 8, #1187). */
        const int cw = font_text_width(font, cost);
        font_draw_text(
          font, framebuffer,
          view->construction_rect.x + view->construction_rect.w - 5 - 4 - cw, row_y, cost, ink
        );
      }
    }
  }
}

/* port_bugs.md #436: advance the DOS "More..." page (wraps). */
void colony_screen_construction_next_page(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  const int rows = view->buildable_count + 1;
  const int per = rows > 22 ? 16 : rows;
  const int pages = per > 0 ? (rows + per - 1) / per : 1;
  if (pages <= 1) {
    return;
  }
  const int page = (view->construction_page + 1) % pages;
  view->construction_page = page;
  view->construction_selection = page * per;
}

/*
 * FUN_2f2b_348c raw 50694-50708: the jobs / leave-as prose, built the same in both modes:
 * @CTITLE[8] ("Select a Profession for", DS:0x93ae) + ' ' + the colonist's SPECIALTY as
 * @JOB column 1 (FUN_281f_0c54, 0x1c read as 0x13; -0x715c + job * 8) + " (" + the current
 * occupation's @JOB column 0 + ")" when it differs and is not 0x13 (FUN_281f_0c0e; -0x715e)
 * + DS:0x55 ": ". Catalog misses stay empty.
 */
static void colony_screen_jobs_header(char* out, size_t cap, int spec, int occ) {
  if (spec == COLONIZE_PROF_FREE_COLONIST) {
    spec = UNITS_JOB_COLONIST;
  }
  const char* spec_name = reports_names_field("JOB", spec, 1);
  int n = snprintf(out, cap, "%s %s", reports_ctitle_word(8), spec_name ? spec_name : "");
  if (n >= 0 && (size_t)n < cap && occ >= 0 && occ != spec && occ != UNITS_JOB_COLONIST) {
    n += snprintf(out + n, cap - (size_t)n, " (%s)", colonies_profession_name(occ));
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
  const int rows = view->job_count;

  int dx = 0;
  int dy = 0;
  colonies_field_tile_delta(view->jobs_tile_index, &dx, &dy);
  const int tx = colony ? colony->x + dx : 0;
  const int ty = colony ? colony->y + dy : 0;

  /* Docks (or an upgrade: Drydock/Shipyard) gates Fisherman yield to 0 —
   * FUN_15eb_18ec:11967-11969. Shared answer, so it cannot drift from
   * turn.c's check the way the AI scorer's copy did (audit E#2). This is why
   * a dockless colony's job list shows "Fisherman (0)" rather than hiding the
   * row: DOS lists the job and answers the pick with @NODOCKS (the port does
   * the same, game_loop.c's two assign paths), so colony_screen_open_jobs
   * deliberately keeps building the row list ungated. */
  const bool has_docks = colony_yield_colony_has_docks(pool, colony);
  int profession = COLONIZE_PROF_FREE_COLONIST;
  int occ = -1;
  colony_screen_colonist_spec_occ(pool, colony, view->selected_colonist, &profession, &occ);
  /* Full production pipeline, like turn.c's field loop — SoL bonus and
   * the colony's SoL latch bits included; the picker used to skip both
   * and could disagree with what the assignment then produced (port_bugs.md:
   * "verify the popup has the proper amounts"). */
  const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);
  char labels[COLONY_JOB_LIST_MAX][48];
  /* FUN_2f2b_348c: FUN_291f_023c(0x800, FONTTINY), box[+0x22] = 8, @width 0xbe = 190
   * (FUN_291f_08d2, raw 50692-50693), rows widen it, DS:0x1f66 raised (raw 50823). */
  int content_w = 0xbe;
  for (int i = 0; i < rows; ++i) {
    const int job = view->job_ids[i];
    if (job == COLONY_JOB_CLEAR_SPECIALTY) {
      /* LABELS.TXT @MISC row 44 — DOS's own jobs-menu entry for the action (DS:0x2e12). */
      snprintf(labels[i], sizeof(labels[i]), "%s", reports_misc_display_word(44, ""));
    } else if (job >= COLONIZE_FIELD_JOB_COUNT) {
      /* Indoor row (port_bugs.md #900). DOS appends a production estimate for
       * @JOB 9..0x11 (raw 50765-50781, `local_132 < 0x13 && != 0x12`) out of
       * the aiStack_12c table the head of FUN_2f2b_348c fills; the port does
       * not model that estimate yet, so the row carries the @JOB name only. */
      snprintf(labels[i], sizeof(labels[i]), "%s", colonies_profession_name(job));
    } else {
      /* Henry Hudson folds into the pipeline (smell audit #60) — same
       * gap/fix as colony_screen_draw_area_overlays above. */
      const int yld = (map && colony)
                  ? colony_yield_for_worker(
                      map, tx, ty, job, profession, has_docks, sol_b_field,
                      colony ? colony->colony_flags : 0,
                      col1 && founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON)
                    )
                  : 0;
      snprintf(labels[i], sizeof(labels[i]), "%s (%d)", colony_yield_job_name(job), yld);
    }
    if (font) {
      const int w = colony_screen_dos_row_w(font, labels[i], 8, NULL);
      if (w > content_w) {
        content_w = w;
      }
    }
  }
  char header[96];
  colony_screen_jobs_header(header, sizeof(header), profession, occ);
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, header, 15, rows, content_w, 0, true, &view->jobs_rect
  );
  for (int i = 0; i < rows; ++i) {
    const int row_y =
      colony_screen_dos_row(framebuffer, &lay, &view->jobs_rect, i, i == view->jobs_selection);
    if (row_y + lay.glyph_h > view->jobs_rect.y + view->jobs_rect.h) {
      /* DOS splits an over-long list across two pages (raw 50678-50682:
       * local_62 = 2, local_d4 = 0x10 plus a 0x62 "More" button). The port has
       * no page button here yet, so simply stop at the frame edge. */
      break;
    }
    if (font) {
      font_draw_text(font, framebuffer, lay.text_x + 8, row_y, labels[i], 15);
    }
  }
}

/*
 * The custom house rows are @checkbox rows: FUN_6f74_0a00 prefixes DS:0x1f91 "~[ " and
 * FUN_6f74_1b7c sets the second byte to ']' (on) or '[' (off); in FONTTINY those glyphs
 * are the filled / hollow circles (options_dialog uses the same rows).
 */
static void colony_screen_custom_house_row(
  const ColonyScreenView* view, const ColonizeColony* colony, int i, char* out, size_t cap
) {
  const int cargo = view->custom_house_cargo_ids[i];
  const bool on = europe_custom_house_cargo_enabled(colony->custom_house_bits, cargo);
  /* NAMES.TXT @CARGO via reports.c's accessor (audit CO-14) — the private
   * copy of the 16 names this replaced ignored a renamed catalog. */
  snprintf(out, cap, "%c %s", on ? ']' : '[',
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
  int content_w = view->custom_house_width > 0x50 ? view->custom_house_width : 0x50;
  char row[64];
  for (int i = 0; i < rows && font; ++i) {
    colony_screen_custom_house_row(view, colony, i, row, sizeof(row));
    const int w = colony_screen_dos_row_w(font, row, 4, NULL);
    if (w > content_w) {
      content_w = w;
    }
  }
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, view->custom_house_title, 15, rows, content_w, 0, false,
    &view->custom_house_rect
  );
  /* Uniform dark green (player-reported: not the brighter green some rows
   * used before — the state is the check glyph's job now, not the text color). */
  const uint8_t kRowColor = 2;
  for (int i = 0; i < rows; ++i) {
    const int row_y = colony_screen_dos_row(framebuffer, &lay, &view->custom_house_rect, i, false);
    colony_screen_custom_house_row(view, colony, i, row, sizeof(row));
    if (font) {
      font_draw_text(font, framebuffer, lay.text_x + 4, row_y, row, kRowColor);
    }
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
  int content_w = 0xbe;
  for (int i = 0; i < rows && font; ++i) {
    const int w =
      colony_screen_dos_row_w(font, colonies_eject_role_name(view->eject_roles[i]), 8, NULL);
    if (w > content_w) {
      content_w = w;
    }
  }
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, header, 15, rows, content_w, 0, true, &view->eject_rect
  );
  for (int i = 0; i < rows; ++i) {
    const int row_y =
      colony_screen_dos_row(framebuffer, &lay, &view->eject_rect, i, i == view->eject_selection);
    if (font) {
      /* DOS draws a short-stock row greyed rather than dropping it
       * (FUN_15eb_3454 → 0xffff, raw 50805 FUN_291f_01b6); colour 8 is the
       * same disabled grey the Europe dock menu uses. */
      font_draw_text(
        font, framebuffer, lay.text_x + 8, row_y, colonies_eject_role_name(view->eject_roles[i]),
        view->eject_role_enabled[i] ? 15 : 8
      );
    }
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
  int content_w = view->dock_orders_width > 0x50 ? view->dock_orders_width : 0x50;
  for (int i = 0; i < rows && font; ++i) {
    const int w = colony_screen_dos_row_w(font, view->dock_orders_labels[i], 4, NULL);
    if (w > content_w) {
      content_w = w;
    }
  }
  const ColonyDosLayout lay = colony_screen_open_dos_dialog(
    view, framebuffer, font, view->dock_orders_title, 15, rows, content_w, 22, false,
    &view->dock_orders_rect
  );
  /* FUN_6f74_201e: the icon at (frame + 3 + box[+0x48], frame + 3 + 3), box[+0x46] +
   * box[+0x4a] = 3 being the entry's top. */
  const ColonizeUnit* u = units_get_const(units, view->dock_orders_unit_id);
  const int sprite = (u && view->icons_ok) ? colony_screen_outside_display_sprite(units, u) : -1;
  if (sprite >= 0) {
    unit_chrome_blit_unit_for_palette(
      framebuffer, font, &view->icons, sprite, view->dock_orders_rect.x + 5,
      view->dock_orders_rect.y + 6, units_display_type_index(units, u->id), u->nation_id,
      u->orders, false, (u->col1_flags15 & 0x80u) != 0,
      (view->frame_ok && view->frame.has_palette) ? &view->frame.palette : NULL
    );
  }
  for (int i = 0; i < rows; ++i) {
    const int row_y = colony_screen_dos_row(
      framebuffer, &lay, &view->dock_orders_rect, i, i == view->dock_orders_selection
    );
    if (font) {
      font_draw_text(font, framebuffer, lay.text_x + 4, row_y, view->dock_orders_labels[i], 15);
    }
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
    view, framebuffer, font, view->message_text, 15, rows, 220, 0, false, &view->message_rect
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
    if (font) {
      font_draw_text(font, framebuffer, lay.text_x + 4, row_y, label, 15);
    }
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
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->jobs_rect, view->job_count, mx, my, &idx)) {
      a->hit.kind = COLONY_HIT_JOBS_OUTSIDE;
      return true;
    }
    if (idx >= 0) {
      a->hit.kind = COLONY_HIT_JOBS_ROW;
      a->hit.index = idx;
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

/* Construction popup (paged list with the More... row). */
COLONIZE_INTERNAL bool colony_screen_hit_construction_popup(ColonyScreenHitCtx* a) {
  const ColonyScreenView* view = a->view;
  const int mx = a->mx;
  const int my = a->my;
  if (view->construction_open) {
    /* Construction keeps its own row math below: paging adds the More...
     * row, so the row index is not a plain popup_row_at_y (audit CO-7). */
    if (!ui_rect_hit(
          view->construction_rect.x, view->construction_rect.y,
          view->construction_rect.w, view->construction_rect.h, mx, my)) {
      a->hit.kind = COLONY_HIT_CONSTRUCTION_OUTSIDE;
      return true;
    }
    if (view->construction_rect.line_h > 0 && my >= view->construction_rect.list_y0) {
      const int row_in_page = (my - view->construction_rect.list_y0) / view->construction_rect.line_h;
      const int rows = view->buildable_count + 1;
      const int per = view->construction_rows_per_col > 0 ? view->construction_rows_per_col : rows;
      const int start = view->construction_page * per;
      int n_slice = rows - start;
      if (n_slice > per) {
        n_slice = per;
      }
      const int pages = per > 0 ? (rows + per - 1) / per : 1;
      if (row_in_page >= 0 && pages > 1 && row_in_page == n_slice) {
        /* The More... row (port_bugs.md #436). */
        a->hit.kind = COLONY_HIT_CONSTRUCTION_MORE;
        a->hit.index = -1;
        return true;
      }
      const int idx = start + row_in_page;
      if (row_in_page >= 0 && row_in_page < n_slice && idx >= 0 && idx < rows) {
        if (idx == 0) {
          a->hit.kind = COLONY_HIT_CONSTRUCTION_CLEAR;
          a->hit.index = -1;
        } else {
          a->hit.kind = COLONY_HIT_CONSTRUCTION_ROW;
          a->hit.index = idx - 1;
        }
        return true;
      }
    }
    return true;
  }
  return false;
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
          a->hit.kind = COLONY_HIT_PEOPLE_COLONIST;
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
    colony_screen_render_minimap(map, terrain, phys0, colony->x, colony->y, framebuffer);
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
