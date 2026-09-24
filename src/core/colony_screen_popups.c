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
#include "core/ff.h"
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
 *  - Hit testing & top-level render entry point (colony_screen_hit_test .. colony_screen_render_w) (~line 584)
 */

/* ===================== Popup rendering: construction/jobs/custom house/eject/dock/message (colony_screen_open_dialog_frame .. colony_screen_draw_message_popup) ===================== */

static void colony_screen_open_dialog_frame(
  ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  int dialog_w,
  int dialog_h,
  int dialog_y,
  int line_h,
  ColonyDialogRect* out,
  int* inner_x,
  int* inner_y,
  int* inner_w,
  int* inner_h
) {
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  const int dialog_x = (framebuffer->width - dialog_w) / 2;
  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    view->wood_tile_ok ? &view->wood_tile : NULL,
    &colors,
    inner_x,
    inner_y,
    inner_w,
    inner_h
  );
  out->x = dialog_x;
  out->y = dialog_y;
  out->w = dialog_w;
  out->h = dialog_h;
  out->line_h = line_h;
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
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 4;
  /* bugs.md #436 / DOS 2f2b_5bd2: SINGLE column always. Past 0x16 = 22 rows
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
  int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h + drawn_rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  int dialog_w = 200;
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  colony_screen_open_dialog_frame(
    view, framebuffer, dialog_w, dialog_h, 24, line_h, &view->construction_rect,
    &inner_x, &inner_y, &inner_w, &inner_h
  );
  view->construction_rows_per_col = rows_per_page;
  view->construction_col_w = inner_w;

  /* bugs.md #436: DOS draws this picker's text GREEN (the standard menu ink),
   * and the "|   " (DS:0xd1d) tab in 2f2b_5a68 right-justifies the cost. */
  const uint8_t ink = 10u;
  if (font && inner_w > 0) {
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, "Construction", ink);
  }
  const int list_y0 = inner_y + pad + line_h;
  view->construction_rect.list_y0 = list_y0;

  for (int i = 0; i < drawn_rows; ++i) {
    const int gi = start + i; /* global row (More... row falls past rows) */
    const int row_y = list_y0 + i * line_h;
    if (row_y + line_h > framebuffer->height) {
      continue;
    }
    const bool more_row = (i >= n_slice);
    const bool selected = (!more_row && gi == view->construction_selection);
    if (selected) {
      fb_fill_rect(framebuffer, inner_x + 1, row_y - 1, inner_w - 2, line_h, 138);
    }
    char label[80];
    char cost[48];
    cost[0] = 0;
    if (more_row) {
      snprintf(label, sizeof(label), "More...");
    } else if (gi == 0) {
      snprintf(label, sizeof(label), "Clear project");
    } else {
      const int bid = view->buildable_ids[gi - 1];
      const ColonizeBuildingType* bt = colonies_building_type(pool, bid);
      const char* uname = NULL;
      int uh = 0;
      int ut = 0;
      /* Player-reported: hammers shown here are what's still *needed*, i.e.
       * the requirement adjusted down by the colony's already-banked
       * hammers (min 0) — hammers carry over to whatever project is picked,
       * unlike tools, which are never adjusted away in this popup.
       * Cost words are DOS's own (2f2b_5a68 string build), right-aligned
       * behind the "|   " tab (bugs.md #436). */
      const int stored_hammers = colony ? colony->hammers : 0;
      if (bt) {
        int hammers_left = bt->hammers - stored_hammers;
        if (hammers_left < 0) {
          hammers_left = 0;
        }
        snprintf(label, sizeof(label), "%s", bt->name);
        /* NAMES.TXT @CARGO row 16 "Hammers", row 14 "Tools". */
        if (bt->tools_cost > 0) {
          snprintf(
            cost, sizeof(cost), "%d %s %d %s", hammers_left, colony_screen_hammers_word(),
            bt->tools_cost, reports_cargo_display_name(14)
          );
        } else {
          snprintf(cost, sizeof(cost), "%d %s", hammers_left, colony_screen_hammers_word());
        }
      } else if (colonies_unit_build_info(bid, &uname, &uh, &ut)) {
        /* Artillery (colonies_unit_build_info) — not a real @BUILDING row. */
        int hammers_left = uh - stored_hammers;
        if (hammers_left < 0) {
          hammers_left = 0;
        }
        snprintf(label, sizeof(label), "%s", uname);
        snprintf(
          cost, sizeof(cost), "%d %s %d %s", hammers_left, colony_screen_hammers_word(), ut,
          reports_cargo_display_name(14)
        );
      } else {
        snprintf(label, sizeof(label), "?");
      }
    }
    if (font) {
      font_draw_text(font, framebuffer, inner_x + pad, row_y + 1, label, ink);
      if (cost[0]) {
        const int cw = font_text_width(font, cost);
        font_draw_text(font, framebuffer, inner_x + inner_w - pad - cw, row_y + 1, cost, ink);
      }
    }
  }
}

/* bugs.md #436: advance the DOS "More..." page (wraps). */
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
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 4;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h + rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  int dialog_w = 170;
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  colony_screen_open_dialog_frame(
    view, framebuffer, dialog_w, dialog_h, 28, line_h, &view->jobs_rect,
    &inner_x, &inner_y, &inner_w, &inner_h
  );

  /* FUN_2f2b_348c raw 50694-50708 builds the dialog header out of the
   * colonist's own SPECIALTY name (FUN_281f_0c54, with 0x1c remapped to 0x13)
   * and appends the current occupation when it differs and is not 0x13 — no
   * typed English ("Field job" was port-invented, and the list is not
   * field-only since bugs.md #900). Catalog miss = empty header. */
  if (font && inner_w > 0) {
    int spec = COLONIZE_PROF_FREE_COLONIST;
    int occ = -1;
    if (colony && view->selected_colonist >= 0 &&
        view->selected_colonist < colony->colonist_count) {
      const ColonizeColonist* sc = &colony->colonists[view->selected_colonist];
      spec = sc->profession;
      occ = (sc->field_job >= 0)
              ? sc->field_job
              : ((sc->building_type >= 0) ? colonies_building_occupation(pool, sc->building_type)
                                          : -1);
    }
    if (spec == COLONIZE_PROF_FREE_COLONIST) {
      spec = UNITS_JOB_COLONIST;
    }
    char header[64];
    const char* spec_name = colonies_profession_name(spec);
    if (occ >= 0 && occ != spec && occ != UNITS_JOB_COLONIST) {
      snprintf(header, sizeof(header), "%s (%s)", spec_name, colonies_profession_name(occ));
    } else {
      snprintf(header, sizeof(header), "%s", spec_name);
    }
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, header, 15);
  }
  const int list_y0 = inner_y + pad + line_h;
  view->jobs_rect.list_y0 = list_y0;

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

  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    if (row_y + line_h > inner_y + inner_h) {
      /* DOS splits an over-long list across two pages (raw 50678-50682:
       * local_62 = 2, local_d4 = 0x10 plus a 0x62 "More" button). The port has
       * no page button here yet, so simply stop at the frame edge. */
      break;
    }
    const bool selected = (i == view->jobs_selection);
    if (selected) {
      fb_fill_rect(framebuffer, inner_x + 1, row_y - 1, inner_w - 2, line_h, 138);
    }
    char label[48];
    const int job = view->job_ids[i];
    if (job == COLONY_JOB_CLEAR_SPECIALTY) {
      if (font) {
        /* LABELS.TXT @MISC row 44 — DOS's own jobs-menu entry for the action. */
        font_draw_text(
          font, framebuffer, inner_x + pad, row_y + 1, reports_misc_display_word(44, ""), 15
        );
      }
      continue;
    }
    if (job >= COLONIZE_FIELD_JOB_COUNT) {
      /* Indoor row (bugs.md #900). DOS appends a production estimate for
       * @JOB 9..0x11 (raw 50765-50781, `local_132 < 0x13 && != 0x12`) out of
       * the aiStack_12c table the head of FUN_2f2b_348c fills; the port does
       * not model that estimate yet, so the row carries the @JOB name only. */
      if (font) {
        font_draw_text(
          font, framebuffer, inner_x + pad, row_y + 1, colonies_profession_name(job), 15
        );
      }
      continue;
    }
    int profession = COLONIZE_PROF_FREE_COLONIST;
    if (colony && view->selected_colonist >= 0 &&
        view->selected_colonist < colony->colonist_count) {
      profession = colony->colonists[view->selected_colonist].profession;
    }
    /* Full production pipeline, like turn.c's field loop — SoL bonus and
     * the colony's SoL latch bits included; the picker used to skip both
     * and could disagree with what the assignment then produced (bugs.md:
     * "verify the popup has the proper amounts"). */
    const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);
    /* Henry Hudson folds into the pipeline (smell audit #60) — same
     * gap/fix as colony_screen_draw_area_overlays above. */
    const int yld = (map && colony)
                ? colony_yield_for_worker(
                    map, tx, ty, job, profession, has_docks, sol_b_field,
                    colony ? colony->colony_flags : 0,
                    col1 && founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON)
                  )
                : 0;
    snprintf(label, sizeof(label), "%s (%d)", colony_yield_job_name(job), yld);
    if (font) {
      font_draw_text(font, framebuffer, inner_x + pad, row_y + 1, label, 15);
    }
  }
}

/* DOS's own Custom House checklist uses a filled/hollow circle as its
 * checkbox (GAME.TXT @CUSTOM's @checkbox directive) — this pixel font has
 * no usable circle/bullet glyph (same gap as '[' ']', see below), so draw
 * one directly: an 8-pixel ring, filled in with 5 more interior pixels
 * when checked. (cx, cy) is the circle's center. */
static void colony_screen_draw_bullet(
  ColonizeFramebuffer8* fb, int cx, int cy, bool filled, uint8_t color
) {
  if (!fb || !fb->pixels) {
    return;
  }
  static const int8_t kRing[8][2] = {
    {0, -2}, {-1, -1}, {1, -1}, {-2, 0}, {2, 0}, {-1, 1}, {1, 1}, {0, 2}
  };
  static const int8_t kInterior[5][2] = {{0, -1}, {-1, 0}, {0, 0}, {1, 0}, {0, 1}};
  for (int i = 0; i < 8; ++i) {
    const int x = cx + kRing[i][0];
    const int y = cy + kRing[i][1];
    if (x >= 0 && y >= 0 && x < fb->width && y < fb->height) {
      fb->pixels[y * fb->width + x] = color;
    }
  }
  if (filled) {
    for (int i = 0; i < 5; ++i) {
      const int x = cx + kInterior[i][0];
      const int y = cy + kInterior[i][1];
      if (x >= 0 && y >= 0 && x < fb->width && y < fb->height) {
        fb->pixels[y * fb->width + x] = color;
      }
    }
  }
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
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 4;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h + rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  /* GAME.TXT @CUSTOM's own @width=190, latched at open time. Without the
   * directive, fall back to measuring: the title ("Which cargos shall our
   * Custom House export?") is the widest line, not any cargo name. */
  int dialog_w = view->custom_house_width;
  if (dialog_w <= 0) {
    dialog_w = 130;
    if (font) {
      const int title_w = popup_markup_text_width(font, view->custom_house_title) + pad * 2;
      if (title_w > dialog_w) {
        dialog_w = title_w;
      }
    }
  }
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  colony_screen_open_dialog_frame(
    view, framebuffer, dialog_w, dialog_h, 20, line_h, &view->custom_house_rect,
    &inner_x, &inner_y, &inner_w, &inner_h
  );

  if (font && inner_w > 0) {
    popup_draw_text_markup(
      font, framebuffer, inner_x + pad, inner_y + pad, view->custom_house_title,
      15, COLONIZE_COL_HILITE, false, false, NULL
    );
  }
  const int list_y0 = inner_y + pad + line_h;
  view->custom_house_rect.list_y0 = list_y0;

  /* Uniform dark green (player-reported: not the brighter green some rows
   * used before — the state is the bullet's job now, not the text color). */
  const uint8_t kRowColor = 2;
  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const int cargo = view->custom_house_cargo_ids[i];
    const bool on = europe_custom_house_cargo_enabled(colony->custom_house_bits, cargo);
    /* NAMES.TXT @CARGO via reports.c's accessor (audit CO-14) — the private
     * copy of the 16 names this replaced ignored a renamed catalog. */
    const char* name = (cargo >= 0 && cargo < COLONIZE_CARGO_COUNT)
      ? reports_cargo_display_name(cargo)
      : "?";
    colony_screen_draw_bullet(
      framebuffer, inner_x + pad + 2, row_y + line_h / 2, on, kRowColor
    );
    if (font) {
      font_draw_text(font, framebuffer, inner_x + pad + 7, row_y + 1, name, kRowColor);
    }
  }
}

static void colony_screen_draw_eject_popup(
  ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->eject_open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int rows = view->eject_role_count;
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 4;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h + rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  int dialog_w = 180;
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  colony_screen_open_dialog_frame(
    view, framebuffer, dialog_w, dialog_h, 28, line_h, &view->eject_rect,
    &inner_x, &inner_y, &inner_w, &inner_h
  );

  if (font && inner_w > 0) {
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, "Leave as", 15);
  }
  const int list_y0 = inner_y + pad + line_h;
  view->eject_rect.list_y0 = list_y0;

  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == view->eject_selection);
    if (selected) {
      fb_fill_rect(framebuffer, inner_x + 1, row_y - 1, inner_w - 2, line_h, 138);
    }
    const char* name = colonies_eject_role_name(view->eject_roles[i]);
    if (font) {
      /* DOS draws a short-stock row greyed rather than dropping it
       * (FUN_15eb_3454 → 0xffff, raw 50805 FUN_291f_01b6); colour 8 is the
       * same disabled grey the Europe dock menu uses. */
      font_draw_text(
        font, framebuffer, inner_x + pad, row_y + 1, name,
        view->eject_role_enabled[i] ? 15 : 8
      );
    }
  }
}

static void colony_screen_draw_dock_orders_popup(
  ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->dock_orders_open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int rows = view->dock_orders_count;
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 4;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h + rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  /* GAME.TXT @COLONYUNIT's own @width=190, latched at open time; without the
   * directive, measure the title and the rows (same fallback as @CUSTOM). */
  int dialog_w = view->dock_orders_width;
  if (dialog_w <= 0) {
    dialog_w = 130;
    if (font) {
      const int title_w = popup_markup_text_width(font, view->dock_orders_title) + pad * 2;
      if (title_w > dialog_w) {
        dialog_w = title_w;
      }
      for (int i = 0; i < rows; ++i) {
        const int row_w = font_text_width(font, view->dock_orders_labels[i]) + pad * 2;
        if (row_w > dialog_w) {
          dialog_w = row_w;
        }
      }
    }
  }
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  colony_screen_open_dialog_frame(
    view, framebuffer, dialog_w, dialog_h, 28, line_h, &view->dock_orders_rect,
    &inner_x, &inner_y, &inner_w, &inner_h
  );

  if (font && inner_w > 0) {
    popup_draw_text_markup(
      font, framebuffer, inner_x + pad, inner_y + pad, view->dock_orders_title,
      15, COLONIZE_COL_HILITE, false, false, NULL
    );
  }
  const int list_y0 = inner_y + pad + line_h;
  view->dock_orders_rect.list_y0 = list_y0;

  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == view->dock_orders_selection);
    if (selected) {
      fb_fill_rect(framebuffer, inner_x + 1, row_y - 1, inner_w - 2, line_h, 138);
    }
    if (font) {
      font_draw_text(font, framebuffer, inner_x + pad, row_y + 1, view->dock_orders_labels[i], 15);
    }
  }
}

static void colony_screen_draw_message_popup(
  ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || view->message_kind == COLONY_MSG_NONE || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int rows = (view->message_kind == COLONY_MSG_CONFIRM) ? 2 : 1;
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 4;
  const int text_lines = 2;
  int dialog_h =
    POPUP_FRAME_INSET * 2 + pad + text_lines * line_h + pad + rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  int dialog_w = 220;
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  colony_screen_open_dialog_frame(
    view, framebuffer, dialog_w, dialog_h, 36, line_h, &view->message_rect,
    &inner_x, &inner_y, &inner_w, &inner_h
  );

  if (font && inner_w > 0) {
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, view->message_text, 15);
  }
  const int list_y0 = inner_y + pad + text_lines * line_h;
  view->message_rect.list_y0 = list_y0;
  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == view->message_selection);
    if (selected) {
      fb_fill_rect(framebuffer, inner_x + 1, row_y - 1, inner_w - 2, line_h, 138);
    }
    const char* label =
      (view->message_kind == COLONY_MSG_OK)
        ? "OK"
        : (i == 0
             ? (view->message_choice0[0] ? view->message_choice0 : "Yes")
             : (view->message_choice1[0] ? view->message_choice1 : "No"));
    if (font) {
      font_draw_text(font, framebuffer, inner_x + pad, row_y + 1, label, 15);
    }
  }
}

/* ===================== Hit testing & top-level render entry point (colony_screen_hit_test .. colony_screen_render_w) ===================== */

ColonyScreenHitResult colony_screen_hit_test(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  int mx,
  int my
) {
  ColonyScreenHitResult hit;
  hit.kind = COLONY_HIT_NONE;
  hit.index = -1;
  if (!view || !colony) {
    return hit;
  }

  /* Shared with both building-related hit checks below — must match
   * colony_screen_blit_buildings' own call so click regions never drift
   * from what's drawn (same per-colony-deterministic assignment).
   * slot_of_cat[] is the DS:0x266 SLOT each category landed in: DOS scans
   * slots, not categories (see the building loop at the end of this
   * function). */
  int slot_x[32];
  int slot_y[32];
  int slot_of_cat[32];
  colony_screen_assign_slot_positions_ex(
    pool, colony, slot_x, slot_y, slot_of_cat, colony_screen_layout_seed(view)
  );

  if (view->message_kind != COLONY_MSG_NONE) {
    const int rows = (view->message_kind == COLONY_MSG_CONFIRM) ? 2 : 1;
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->message_rect, rows, mx, my, &idx)) {
      hit.kind = COLONY_HIT_MESSAGE_OUTSIDE;
      return hit;
    }
    if (idx >= 0) {
      if (view->message_kind == COLONY_MSG_OK) {
        hit.kind = COLONY_HIT_MESSAGE_OK;
      } else if (idx == 0) {
        hit.kind = COLONY_HIT_MESSAGE_YES;
      } else {
        hit.kind = COLONY_HIT_MESSAGE_NO;
      }
    }
    return hit;
  }

  if (view->custom_house_open) {
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->custom_house_rect, view->custom_house_count, mx, my, &idx)) {
      hit.kind = COLONY_HIT_CUSTOM_HOUSE_OUTSIDE;
      return hit;
    }
    if (idx >= 0) {
      hit.kind = COLONY_HIT_CUSTOM_HOUSE_ROW;
      hit.index = idx;
    }
    return hit;
  }

  if (view->jobs_open) {
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->jobs_rect, view->job_count, mx, my, &idx)) {
      hit.kind = COLONY_HIT_JOBS_OUTSIDE;
      return hit;
    }
    if (idx >= 0) {
      hit.kind = COLONY_HIT_JOBS_ROW;
      hit.index = idx;
    }
    return hit;
  }

  if (view->eject_open) {
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->eject_rect, view->eject_role_count, mx, my, &idx)) {
      hit.kind = COLONY_HIT_EJECT_OUTSIDE;
      return hit;
    }
    if (idx >= 0) {
      hit.kind = COLONY_HIT_EJECT_ROW;
      hit.index = idx;
    }
    return hit;
  }

  if (view->dock_orders_open) {
    int idx = -1;
    if (!colony_screen_dialog_row_hit(&view->dock_orders_rect, view->dock_orders_count, mx, my, &idx)) {
      hit.kind = COLONY_HIT_DOCK_ORDERS_OUTSIDE;
      return hit;
    }
    if (idx >= 0) {
      hit.kind = COLONY_HIT_DOCK_ORDERS_ROW;
      hit.index = idx;
    }
    return hit;
  }

  if (view->construction_open) {
    /* Construction keeps its own row math below: paging adds the More...
     * row, so the row index is not a plain popup_row_at_y (audit CO-7). */
    if (!ui_rect_hit(
          view->construction_rect.x, view->construction_rect.y,
          view->construction_rect.w, view->construction_rect.h, mx, my)) {
      hit.kind = COLONY_HIT_CONSTRUCTION_OUTSIDE;
      return hit;
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
        /* The More... row (bugs.md #436). */
        hit.kind = COLONY_HIT_CONSTRUCTION_MORE;
        hit.index = -1;
        return hit;
      }
      const int idx = start + row_in_page;
      if (row_in_page >= 0 && row_in_page < n_slice && idx >= 0 && idx < rows) {
        if (idx == 0) {
          hit.kind = COLONY_HIT_CONSTRUCTION_CLEAR;
          hit.index = -1;
        } else {
          hit.kind = COLONY_HIT_CONSTRUCTION_ROW;
          hit.index = idx - 1;
        }
        return hit;
      }
    }
    return hit;
  }

  if (mx >= COLONY_EXIT_X && mx < COLONY_SCREEN_WIDTH &&
      my >= COLONY_EXIT_Y && my < COLONY_SCREEN_HEIGHT) {
    hit.kind = COLONY_HIT_EXIT;
    return hit;
  }

  /* Warehouse cargo strip (load into selected transport). */
  if (my >= COLONY_CARGO_STRIP_Y && my < COLONY_SCREEN_HEIGHT && mx < COLONY_EXIT_X) {
    if (mx >= COLONY_CARGO_SLOT_X0) {
      const int idx = (mx - COLONY_CARGO_SLOT_X0) / COLONY_CARGO_PITCH;
      if (idx >= 0 && idx < COLONIZE_CARGO_COUNT) {
        hit.kind = COLONY_HIT_CARGO_SLOT;
        hit.index = idx;
        return hit;
      }
    }
  }

  /* Goods holds of selected transport. */
  if (units && view->transport_unit_id >= 0 && my >= COLONY_HOLD_Y &&
      my < COLONY_HOLD_Y + COLONY_HOLD_H) {
    const int holds = units_goods_hold_count(units, view->transport_unit_id);
    if (mx >= COLONY_HOLD_X && holds > 0) {
      const int idx = (mx - COLONY_HOLD_X) / COLONY_HOLD_PITCH;
      if (idx >= 0 && idx < holds &&
          mx < COLONY_HOLD_X + idx * COLONY_HOLD_PITCH + COLONY_HOLD_W) {
        hit.kind = COLONY_HIT_HOLD;
        hit.index = idx;
        return hit;
      }
    }
  }

  /* Multifunction mode buttons. */
  if (mx >= COLONY_MULTI_BTN_X && mx < COLONY_MULTI_BTN_X + COLONY_MULTI_BTN_W &&
      my >= COLONY_PANEL_CONTENT_Y && my < COLONY_PANEL_CONTENT_Y + 48) {
    const int idx = (my - COLONY_PANEL_CONTENT_Y) / 16;
    if (idx >= 0 && idx < 3) {
      hit.kind = COLONY_HIT_MULTI_BTN;
      hit.index = idx;
      return hit;
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
        hit.kind = COLONY_HIT_MULTI_UNIT_ICON;
        hit.index = slots[i].unit_id;
        return hit;
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
        hit.kind = COLONY_HIT_MULTI_CHANGE;
        return hit;
      }
      /* FUN_2f2b_5fc6: the BUY button only hit-tests when +0x94 >= 0 (no
       * project, e.g. a captured AI colony's 0xFF, has no BUY; bugs.md #548). */
      if (colony->building_in_production >= 0) {
        hit.kind = COLONY_HIT_MULTI_BUY;
        return hit;
      }
    }
    hit.kind = COLONY_HIT_MULTI_PANE;
    hit.index = (int)view->multi_mode;
    return hit;
  }

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
      hit.kind = COLONY_HIT_TRANSPORT;
      hit.index = idx;
      return hit;
    }
  }

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
      if (!u || colony_screen_unit_is_artillery(units, u)) {
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
      /* bugs.md: walk BACK-to-front — squeezed icons overlap, and the one
       * drawn on top (later index) must win the click, or the units behind
       * the pile were unselectable. */
      for (int i = n - 1; i >= 0; --i) {
        int iw = 12;
        int ih = 16;
        colony_screen_outside_icon_metrics(view, units, view->outside_unit_ids[map_i[i]], &iw, &ih);
        const int uy = fence_y + (fence_h - ih) / 2;
        if (mx >= xs[i] && my >= uy && mx < xs[i] + iw && my < uy + ih) {
          hit.kind = COLONY_HIT_OUTSIDE_UNIT;
          hit.index = map_i[i];
          return hit;
        }
      }
    }
    /* Empty fortification strip (eject target when a colony colonist is selected). */
    if (mx >= fence_x && my >= fence_y && mx < fence_x + fence_w && my < fence_y + fence_h) {
      hit.kind = COLONY_HIT_FENCE;
      return hit;
    }
  } else if (view->buildings_ok) {
    /* No outside units: still allow fence clicks for eject. */
    int fence_x, fence_y, fence_w, fence_h;
    colony_screen_fence_rect(
      view, pool, colony, slot_x, slot_y, &fence_x, &fence_y, &fence_w, &fence_h
    );
    if (mx >= fence_x && my >= fence_y && mx < fence_x + fence_w && my < fence_y + fence_h) {
      hit.kind = COLONY_HIT_FENCE;
      return hit;
    }
  }

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
      int worker_ci[COLONY_BUILDING_WORKERS_MAX];
      int worker_icons[COLONY_BUILDING_WORKERS_MAX];
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
      int xs[COLONY_BUILDING_WORKERS_MAX];
      colony_screen_icon_strip_layout(bx, bspr->width, workers, ref_iw, xs);
      for (int wi = 0; wi < workers; ++wi) {
        int iw = ref_iw;
        int ih = strip_h;
        if (worker_icons[wi] < view->icons.sprite_count) {
          iw = view->icons.sprites[worker_icons[wi]].width;
          ih = view->icons.sprites[worker_icons[wi]].height;
        }
        const int iy = strip_y + (strip_h - ih) / 2;
        if (mx >= xs[wi] && my >= iy && mx < xs[wi] + iw && my < iy + ih) {
          hit.kind = COLONY_HIT_PEOPLE_COLONIST;
          hit.index = worker_ci[wi];
          return hit;
        }
      }
    }
  }

  /* Area-view tiles (surround only; center is not assignable). */
  {
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
        hit.kind = COLONY_HIT_AREA_TILE;
        hit.index = ti;
        return hit;
      }
      /* Centre tile (not assignable): DOS's click-to-toggle-numbers spot
       * (2f2b:609e flips DS:0x336 and refreshes; bugs.md). */
      hit.kind = COLONY_HIT_AREA_INTERIOR;
      return hit;
    }
  }

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
          hit.kind = COLONY_HIT_PEOPLE_COLONIST;
          hit.index = ent[i].sel_colonist;
        } else {
          hit.kind = COLONY_HIT_OUTSIDE_UNIT;
          /* callers expect the outside ARRAY index */
          hit.index = -1;
          for (int oi = 0; oi < view->outside_unit_count; ++oi) {
            if (view->outside_unit_ids[oi] == ent[i].sel_unit) {
              hit.index = oi;
              break;
            }
          }
          if (hit.index < 0) {
            continue;
          }
        }
        return hit;
      }
    }
  }

  if (pool && view->buildings_ok && mx >= COLONY_VIEWPORT_X &&
      mx < COLONY_VIEWPORT_X + COLONY_VIEWPORT_W && my >= COLONY_VIEWPORT_Y &&
      my < COLONY_BOTTOM_SEPARATOR_Y) {
    const int slot_ox = COLONY_VIEWPORT_X;
    const int slot_oy = COLONY_VIEWPORT_Y;
    /*
     * bugs.md #430 — DOS's own building-slot scan, FUN_2f2b_44d4
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
        hit.kind = COLONY_HIT_BUILDING;
        hit.index = built;
        return hit;
      }
    }
  }

  return hit;
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
    colony_screen_refresh_preview_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(pool), .map=(ColonizeWorldMap*)(map), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL)}, view, colony);
  }

  colony_screen_fill_top_bar_wood(view, framebuffer);
  colony_screen_draw_top_bar(colony, game_year, game_autumn, gold, font, framebuffer);

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
    colony_screen_draw_eject_popup(view, font, framebuffer);
  }
  if (view && view->custom_house_open && colony) {
    colony_screen_draw_custom_house_popup(view, colony, font, framebuffer);
  }
  if (view && view->dock_orders_open) {
    colony_screen_draw_dock_orders_popup(view, font, framebuffer);
  }
  if (view && view->message_kind != COLONY_MSG_NONE) {
    colony_screen_draw_message_popup(view, font, framebuffer);
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

