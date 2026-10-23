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
 *  - View state, refresh helpers & subpanel close (colony_screen_set_status .. colony_screen_close_construction) (~line 40)
 *  - Subpanel open/close & popup font/text helpers (colony_screen_open_construction .. colony_screen_open_custom_house) (~line 304)
 *  - Popup open: eject/jobs/dock-orders & minimap origin (colony_screen_open_eject .. colony_screen_minimap_origin) (~line 478)
 *  - PIK loading, view load/free (colony_screen_load_pik .. colony_screen_free) (~line 721)
 *
 * The rest of the colony screen lives in colony_screen_draw.c,
 * colony_screen_buildings.c, colony_screen_panels.c and
 * colony_screen_popups.c; cross-file seams are in colony_screen_internal.h.
 */

/* ===================== View state, refresh helpers & subpanel close (colony_screen_set_status .. colony_screen_close_construction) ===================== */

void colony_screen_set_status(ColonyScreenView* view, const char* text) {
  if (!view) {
    return;
  }
  snprintf(view->status, sizeof(view->status), "%s", text ? text : "");
  popup_msg_strip_markup(view->status); /* plain status text, no {} markup */
}

void colony_screen_reset_ui(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  view->selected_colonist = -1;
  view->selected_outside_unit = -1;
  view->drag_from_tile = -1;
  view->hover_building_row = -1;
  /* show_production_numbers deliberately NOT reset here: it is DOS's
   * game-wide DS:0x336 numbers toggle and survives between colonies. */
  /* port_bugs.md: DOS opens the colony screen on the construction view. */
  view->multi_mode = COLONY_MULTI_CONSTRUCTION;
  view->selected_cargo = -1;
  view->construction_open = false;
  view->construction_selection = 0;
  view->buildable_count = 0;
  view->jobs_open = false;
  view->jobs_tile_index = -1;
  view->jobs_selection = 0;
  view->job_count = 0;
  view->eject_open = false;
  view->eject_colonist_index = -1;
  view->eject_unit_id = -1;
  view->eject_selection = 0;
  view->eject_role_count = 0;
  view->dock_orders_open = false;
  view->dock_orders_unit_id = -1;
  view->dock_orders_selection = 0;
  view->dock_orders_count = 0;
  view->dock_orders_title[0] = '\0';
  view->custom_house_open = false;
  view->custom_house_count = 0;
  view->message_kind = COLONY_MSG_NONE;
  view->message_text[0] = '\0';
  view->message_selection = 0;
  view->pending_eject_colonist = -1;
  view->pending_eject_role = COLONIZE_EJECT_COLONIST;
  view->preview_valid = false;
  memset(&view->preview, 0, sizeof(view->preview));
  view->transport_unit_id = -1;
  view->docked_transport_count = 0;
  memset(view->docked_transport_ids, 0, sizeof(view->docked_transport_ids));
  view->outside_unit_count = 0;
  memset(view->outside_unit_ids, 0, sizeof(view->outside_unit_ids));
  view->multi_unit_selected_id = -1;
}

void colony_screen_refresh_transports(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeColony* colony
) {
  if (!view) {
    return;
  }
  view->docked_transport_count = 0;
  if (!units || !colony) {
    view->transport_unit_id = -1;
    return;
  }
  int stack[COLONIZE_UNITS_MAX];
  const int n =
    units_collect_tile_stack(units, colony->x, colony->y, colony->nation_id, stack, COLONIZE_UNITS_MAX);
  for (int i = 0; i < n && view->docked_transport_count < COLONY_TRANSPORT_MAX; ++i) {
    if (!units_is_transport(units, stack[i])) {
      continue;
    }
    const ColonizeUnit* u = units_get_const(units, stack[i]);
    if (!u || !units_is_on_map(u)) {
      continue;
    }
    view->docked_transport_ids[view->docked_transport_count++] = stack[i];
  }
  if (view->transport_unit_id >= 0) {
    bool still = false;
    for (int i = 0; i < view->docked_transport_count; ++i) {
      if (view->docked_transport_ids[i] == view->transport_unit_id) {
        still = true;
        break;
      }
    }
    if (!still) {
      view->transport_unit_id = -1;
    }
  }
  /*
   * Something is always selected while there is anything to select — the
   * screen never shows an unhighlighted row of units (port_bugs.md). This used
   * to auto-select only when exactly one transport was docked, so two ships
   * in port left neither highlighted until the player clicked one.
   */
  if (view->transport_unit_id < 0 && view->docked_transport_count > 0) {
    view->transport_unit_id = view->docked_transport_ids[0];
  }
}

void colony_screen_refresh_outside(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeColony* colony
) {
  if (!view) {
    return;
  }
  view->outside_unit_count = 0;
  if (!units || !colony) {
    view->selected_outside_unit = -1;
    view->multi_unit_selected_id = -1;
    return;
  }
  int stack[COLONIZE_UNITS_MAX];
  const int n =
    units_collect_tile_stack(units, colony->x, colony->y, colony->nation_id, stack, COLONIZE_UNITS_MAX);
  for (int i = 0; i < n && view->outside_unit_count < COLONY_OUTSIDE_MAX; ++i) {
    if (units_is_transport(units, stack[i])) {
      continue;
    }
    const ColonizeUnit* u = units_get_const(units, stack[i]);
    if (!u || !units_is_on_map(u)) {
      continue;
    }
    view->outside_unit_ids[view->outside_unit_count++] = stack[i];
  }
  /* The pool's "front" unit (Move to front / freshly built Artillery) heads
   * the row. */
  if (units->board_first_slot >= 0 && units->board_first_slot < COLONIZE_UNITS_MAX) {
    const int front_id = units->units[units->board_first_slot].id;
    for (int i = 1; i < view->outside_unit_count; ++i) {
      if (view->outside_unit_ids[i] == front_id) {
        memmove(&view->outside_unit_ids[1], &view->outside_unit_ids[0], (size_t)i * sizeof(view->outside_unit_ids[0]));
        view->outside_unit_ids[0] = front_id;
        break;
      }
    }
  }
  if (view->selected_outside_unit >= 0) {
    bool still = false;
    for (int i = 0; i < view->outside_unit_count; ++i) {
      if (view->outside_unit_ids[i] == view->selected_outside_unit) {
        still = true;
        break;
      }
    }
    if (!still) {
      view->selected_outside_unit = -1;
    }
  }
  if (view->multi_unit_selected_id >= 0) {
    bool still = false;
    for (int i = 0; i < view->outside_unit_count; ++i) {
      if (view->outside_unit_ids[i] == view->multi_unit_selected_id) {
        still = true;
        break;
      }
    }
    for (int i = 0; !still && i < view->docked_transport_count; ++i) {
      if (view->docked_transport_ids[i] == view->multi_unit_selected_id) {
        still = true;
        break;
      }
    }
    if (!still) {
      view->multi_unit_selected_id = -1;
    }
  }
  /* The Units pane always has a highlighted unit while it has any. */
  if (view->multi_unit_selected_id < 0) {
    if (view->outside_unit_count > 0) {
      view->multi_unit_selected_id = view->outside_unit_ids[0];
    } else if (view->docked_transport_count > 0) {
      view->multi_unit_selected_id = view->docked_transport_ids[0];
    }
  }
  if (view->selected_colonist >= colony->colonist_count) {
    view->selected_colonist = -1;
  }
  /*
   * Settlement view and the People band both show colonists AND on-tile
   * units, so selected_colonist / selected_outside_unit are one mutually
   * exclusive selection, not two: auto-filling both painted two boxes at
   * once (port_bugs.md: "too many selections ... should be 1 and only 1 each").
   * The click handlers already clear one when setting the other
   * (game_colony_select_colonist / _select_outside); here only fill a
   * fallback when *neither* is set, preferring the colony population.
   */
  if (view->selected_colonist < 0 && view->selected_outside_unit < 0) {
    if (colony->colonist_count > 0) {
      view->selected_colonist = 0;
    } else if (view->outside_unit_count > 0) {
      view->selected_outside_unit = view->outside_unit_ids[0];
    }
  }
}

void colony_screen_refresh_preview_w(
  const ColonizeWorld* w,
  ColonyScreenView* view,
  const ColonizeColony* colony
) {
  const ColonizeColonyPool* pool = w->colonies;

  if (!view) {
    return;
  }
  if (!pool || !colony) {
    view->preview_valid = false;
    return;
  }
  colony_preview_compute_w(w, colony, &view->preview);
  view->preview_valid = true;
}


/*
 * Single close-all for the six mutually exclusive sub-panels — see
 * colony_screen.h. Every opener below calls it, so no opener can leave one of
 * its siblings flagged open underneath itself, and game_loop's panel hotkeys
 * call it for the same reason. Cheap and idempotent: each close_* is a plain
 * field reset that no-ops on an already-closed panel.
 */
void colony_screen_close_subpanels(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  colony_screen_close_message(view);
  colony_screen_close_jobs(view);
  colony_screen_close_eject(view);
  colony_screen_close_dock_orders(view);
  colony_screen_close_custom_house(view);
  colony_screen_close_construction(view);
  view->more_selected = false;
}

void colony_screen_close_construction(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  view->construction_open = false;
  view->construction_selection = 0;
}

/* ===================== Subpanel open/close & popup font/text helpers (colony_screen_open_construction .. colony_screen_open_custom_house) ===================== */

void colony_screen_open_construction(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  int colony_id,
  const ColoniesBuildableOpts* buildable_opts
) {
  if (!view) {
    return;
  }
  colony_screen_close_subpanels(view);
  view->buildable_count = colonies_list_buildable(
    pool, colony_id, view->buildable_ids, COLONY_BUILDABLE_MAX, buildable_opts
  );
  view->construction_open = true;
  view->more_selected = false;
  /* Row 0 is "(no production)" (code -1, so it is the current row when nothing is in
   * production); rows 1.. are buildable_ids. */
  const ColonizeColony* col = colonies_get(pool, colony_id);
  view->construction_default_row = col && col->building_in_production < 0 ? 0 : -1;
  for (int i = 0; col && i < view->buildable_count; ++i) {
    if (view->buildable_ids[i] == col->building_in_production) {
      view->construction_default_row = i + 1;
      break;
    }
  }
  /* FUN_2f2b_5bd2 always opens page 0 (local_4 = 0, raw 52587). The current project's row
   * has the bar when page 0 lists it (raw 52608); a two-page list that does not list it calls
   * FUN_291f_08ec(0x65) (raw 52621), an id no row has (rows are 1, code + 2 <= 0x32, 0x62,
   * 0x63), so FUN_6f74_089a finds nothing and box+0x4c is cleared: no bar (-1). One page
   * keeps the first row added (FUN_6f74_0a00), the clear row. */
  const int cur = view->construction_default_row;
  if (cur >= 0 && cur < 0x10) {
    view->construction_selection = cur;
  } else {
    view->construction_selection = view->buildable_count + 1 > 0x16 ? -1 : (cur >= 0 ? cur : 0);
  }
}

void colony_screen_close_jobs(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  view->jobs_open = false;
  view->jobs_tile_index = -1;
  view->jobs_selection = 0;
  view->job_count = 0;
}

void colony_screen_close_eject(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  view->eject_open = false;
  view->eject_colonist_index = -1;
  view->eject_unit_id = -1;
  view->eject_selection = 0;
  view->eject_role_count = 0;
}

void colony_screen_close_message(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  view->message_kind = COLONY_MSG_NONE;
  view->message_text[0] = '\0';
  view->message_choice0[0] = '\0';
  view->message_choice1[0] = '\0';
  view->message_selection = 0;
  view->pending_eject_colonist = -1;
  view->pending_eject_role = COLONIZE_EJECT_COLONIST;
}

void colony_screen_close_dock_orders(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  view->dock_orders_open = false;
  view->dock_orders_unit_id = -1;
  view->dock_orders_selection = 0;
  view->dock_orders_count = 0;
}

void colony_screen_close_custom_house(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  view->custom_house_open = false;
  view->custom_house_count = 0;
  view->custom_house_selection = 0;
}

/* GAME.TXT's bare directives ("@checkbox", "@smallfont") are kept as ordinary
 * section lines by the catalog parser, so an owner that wants one scans for
 * it — the same read pick_music.c:126 and game_loop.c:4428 do. (@width is not
 * scanned here: popup_msg_fill already latches it, see the two call sites.) */
/* NAMES.TXT @CARGO row 16 — the production-point noun. Past the 16 tradeable
 * cargos, so reports_cargo_display_name does not cover it. */
const char* colony_screen_hammers_word(void) {
  static char word[24];
  const char* live = reports_names_field("CARGO", 16, 0);
  snprintf(word, sizeof(word), "%s", live ? live : "");
  return word;
}

static bool colony_screen_section_has_directive(
  const ColonizeMsgSection* section,
  const char* directive
) {
  if (!section || !directive) {
    return false;
  }
  for (int i = 0; i < section->line_count; ++i) {
    /* lines[i] is a char array inside the block, never a NULL pointer. */
    if (strcmp(section->lines[i], directive) == 0) {
      return true;
    }
  }
  return false;
}

/* DOS keeps FONTINTR in the dialog font slot and swaps to FONTTINY only for a
 * section carrying @smallfont. `screen_font` is the colony screen's own font,
 * which game_loop.c fills with FONTTINY — so it *is* the small font here. */
const ColonizeFont* colony_screen_popup_font(
  const ColonyScreenView* view,
  const ColonizeFont* screen_font,
  bool smallfont
) {
  if (smallfont && screen_font) {
    return screen_font;
  }
  if (view && view->dialog_font_ok) {
    return &view->dialog_font;
  }
  return screen_font;
}

void colony_screen_open_custom_house(
  ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeMsgCatalog* messages
) {
  if (!view || !colony) {
    return;
  }
  colony_screen_close_subpanels(view);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  popup_msg_fill(
    messages,
    "CUSTOM",
    &tok,
    "",
    view->custom_house_title,
    sizeof(view->custom_house_title)
  );
  /* Both of @CUSTOM's layout directives come from GAME.TXT, not literals:
   * popup_msg_fill latched @width=190 (ai_popup.c:205 / save_load_dialog.c
   * / game_loop.c take the same latch), and @smallfont swaps the rows to
   * FONTTINY. A catalog without either leaves the draw measuring the title. */
  view->custom_house_width = popup_msg_take_pending_width();
  view->custom_house_smallfont = colony_screen_section_has_directive(
    messages ? assets_msg_find(messages, "CUSTOM") : NULL, "@smallfont"
  );
  /* Every cargo gets a row: FUN_2f2b_4424 adds all 16 (@CARGO via DS:0x97c0, Food
   * included, raw loop `local_8 < 0x10`, 2f2b:4447-448a) with each one's bit as the
   * checkbox (FUN_281f_0cfe / FUN_291f_0262), port_bugs.md #1190. Player-reported:
   * filtering to only europe_cargo_export_eligible()'s autosell denylist left Tools and
   * Muskets (and Horses) missing from the popup. That denylist still gates
   * europe_custom_house_autosell()'s actual EOT sell — toggling one of
   * those rows on here just never has an effect (the bit exists either way). */
  view->custom_house_count = 0;
  for (int c = 0;
       c < COLONIZE_CARGO_COUNT && view->custom_house_count < COLONIZE_CARGO_COUNT;
       ++c) {
    view->custom_house_cargo_ids[view->custom_house_count++] = c;
  }
  /* FUN_6f74_0a00 makes the first row added the box's selection (box+0x4c). */
  view->custom_house_selection = 0;
  view->custom_house_open = true;
}

/* ===================== Popup open: eject/jobs/dock-orders & minimap origin (colony_screen_open_eject .. colony_screen_minimap_origin) ===================== */

void colony_screen_open_eject(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index
) {
  if (!view || !pool || colonist_index < 0) {
    return;
  }
  colony_screen_close_subpanels(view);
  view->eject_colonist_index = colonist_index;
  view->eject_unit_id = -1;
  for (int i = 0; i < COLONIZE_EJECT_ROLE_COUNT + 1; ++i) {
    view->eject_role_enabled[i] = true;
  }
  /* FUN_2f2b_348c raw 50741-50750: page 0 of the leave-as list starts with the colonist's
   * current occupation when it is below 0x13 (FUN_281f_0c0e), and that row is the default
   * (FUN_291f_08ec). A colonist always works a plot or a building here, so it is his
   * field job or his building's job. */
  const ColonizeColony* col = colonies_get(pool, colony_id);
  const ColonizeColonist* c =
    (col && colonist_index < col->colonist_count) ? &col->colonists[colonist_index] : NULL;
  const int occ = !c ? -1
                  : c->field_job >= 0 ? c->field_job
                  : c->building_type >= 0 ? colonies_building_occupation(pool, c->building_type)
                                          : -1;
  const int lead = occ >= 0 && occ < UNITS_JOB_COLONIST ? 1 : 0;
  view->eject_roles[0] = COLONY_EJECT_ROLE_STAY;
  view->eject_role_count = lead + colonies_list_eject_roles_ex(
    pool,
    colony_id,
    colonist_index,
    view->eject_roles + lead,
    view->eject_role_enabled + lead,
    COLONIZE_EJECT_ROLE_COUNT
  );
  if (view->eject_role_count <= 0) {
    view->eject_roles[0] = COLONIZE_EJECT_COLONIST;
    view->eject_role_enabled[0] = true;
    view->eject_role_count = 1;
  }
  view->eject_open = true;
  view->eject_selection = 0;
}

int colony_screen_unit_eject_role(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  /* FUN_281f_0c0e for a fence body: its type's @JOB 0x13..0x18 = leave-as roles 0..5. */
  switch (u ? units_type_kind(units_type(units, u->type_index)) : UNITS_KIND_UNKNOWN) {
  case UNITS_KIND_COLONIST: return COLONIZE_EJECT_COLONIST;
  case UNITS_KIND_PIONEER: return COLONIZE_EJECT_PIONEER;
  case UNITS_KIND_SOLDIER: return COLONIZE_EJECT_SOLDIER;
  case UNITS_KIND_SCOUT: return COLONIZE_EJECT_SCOUT;
  case UNITS_KIND_DRAGOON: return COLONIZE_EJECT_DRAGOON;
  case UNITS_KIND_MISSIONARY: return COLONIZE_EJECT_MISSIONARY;
  default: return -1;
  }
}

void colony_screen_open_jobs(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColony* colony,
  int tile_index
) {
  /*
   * Bounded by the 3x3 area the screen draws, not by the colony's work ring.
   * port_bugs.md #593: a colony that owns @BUILDING row 0x0a/0x0b works 12 or 20
   * plots and the sim counts all of them, but the DOS colony screen's area
   * panel is a 3x3 grid (FUN_2f2b_0a74, raw ~47500, walks the full
   * DS:0x329 count yet the panel geometry never grew), and no save stock DOS
   * can write ever carries those rows. So the port draws and clicks the 3x3
   * and does not invent an outer-plot UI.
   */
  /* tile_index -1 = the colonist works no plot (Enter on him, or a click on him in his
   * building: FUN_2f2b_6372 / FUN_2f2b_44d4 raw 51280-51286); a field pick then takes the
   * plot FUN_15eb_28c8 elects (port_bugs.md #1204). */
  if (!view || !colony || tile_index < -1 || tile_index >= COLONIZE_COLONY_FIELD_TILES) {
    return;
  }
  colony_screen_close_subpanels(view);
  view->jobs_tile_index = tile_index;
  view->job_count = 0;
  /* Row list is deliberately NOT docks-gated: DOS offers Fisherman on a
   * dockless colony and answers the pick with GAME.TXT @NODOCKS, which is
   * what game_loop.c's two assign paths do. The drawn number IS gated, so
   * the row reads "Fisherman (0)" — see colony_screen_draw_jobs_popup. */
  /* DOS-LITERAL FUN_15eb_3454 raw 13518-13554 (reached from the row loop of
   * FUN_2f2b_348c raw 50753-50757, `local_e = FUN_281f_0bb4(job)`): row
   * eligibility is a BUILDING test, never a yield test. For job < 0x13 the
   * required-building byte is DS:0x2f4[job] (FUN_15eb_0aec raw 10087-10093);
   * that table (VICEROY.EXE raw 122004, 19 bytes) is
   *   -1 -1 -1 -1 -1 -1 -1 -1 -1 27 24 21 32 35 39 3 37 9 12
   * so every field job 0..8 has no building requirement and is ALWAYS listed;
   * a job is dropped only when its building is absent (FUN_15eb_038e == 0).
   * DOS then fills each row's number with the colonist trial-assigned
   * (FUN_281f_0c36 = FUN_15eb_1068, then FUN_281f_0b3c = FUN_15eb_18ec, raw
   * 50609-50615) — which is what colony_screen_draw_jobs_popup already does
   * via colony_yield_for_worker. So: no yield filter here. */
  (void)map;
  const int sel_prof =
    (colony && view->selected_colonist >= 0 && view->selected_colonist < colony->colonist_count)
      ? colony->colonists[view->selected_colonist].profession
      : COLONIZE_PROF_FREE_COLONIST;
  /* port_bugs.md #900: the row loop is FUN_2f2b_348c raw 50753-50817's, over @JOB
   * 0..0x18 — not the nine field jobs. Jobs 0..8 have no required building and
   * are always listed; 9..0x12 (the indoor jobs, incl. @JOB 18 Teacher) appear
   * once their DS:0x2f4 building stands. DOS DROPS an unowned row (FUN_15eb_3454
   * returns 0), it does not grey it. Rows 0x13..0x18 are the leave-as list and
   * are served by the port's eject popup. */
  /* FUN_2f2b_348c raw 50736-50739: a colonist with a specialty (FUN_15eb_0002 —
   * profession outside 0x13/0x19/0x1a/0x1b/0x1c) gets "Clear Specialty" (menu id
   * 0x61) as page 0's FIRST row, ahead of the jobs; picking it asks @LOBOTOMIZE
   * before wiping to 0x1c. */
  if (view->selected_colonist >= 0 && view->selected_colonist < colony->colonist_count) {
    const int prof = sel_prof;
    if (prof != UNITS_JOB_COLONIST && prof != COLONIZE_PROF_INDENTURED &&
        prof != COLONIZE_PROF_CRIMINAL &&
        prof != COLONIZE_PROF_CONVERT && prof != COLONIZE_PROF_FREE_COLONIST) {
      view->job_ids[view->job_count++] = COLONY_JOB_CLEAR_SPECIALTY;
    }
  }
  for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT && view->job_count < COLONY_JOB_LIST_MAX; ++job) {
    view->job_ids[view->job_count++] = job;
  }
  for (int job = COLONIZE_FIELD_JOB_COUNT;
       job <= COLONIES_JOB_TEACHER && view->job_count < COLONY_JOB_LIST_MAX; ++job) {
    if (colonies_job_row_offered(pool, colony, job, sel_prof)) {
      view->job_ids[view->job_count++] = job;
    }
  }
  /* raw 50812: the box's default row is the colonist's current occupation
   * (FUN_291f_08ec) on the page that lists it, else the page's first row. */
  view->jobs_default_row = 0;
  if (view->selected_colonist >= 0 && view->selected_colonist < colony->colonist_count) {
    const ColonizeColonist* c = &colony->colonists[view->selected_colonist];
    const int occ = c->field_job >= 0 ? c->field_job
                    : (pool && c->building_type >= 0)
                      ? colonies_building_occupation(pool, c->building_type)
                      : -1;
    for (int i = 0; i < view->job_count; ++i) {
      if (occ >= 0 && view->job_ids[i] == occ) {
        view->jobs_default_row = i;
      }
    }
  }
  view->jobs_open = true;
  /* DOS opens on page 0 (local_6 = 0, raw 50676). */
  view->jobs_selection =
    colony_screen_jobs_page_of(view, view->jobs_default_row) == 0 ? view->jobs_default_row : 0;
}

void colony_screen_open_dock_orders(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeMsgCatalog* messages,
  int unit_id
) {
  if (!view || !units) {
    return;
  }
  const ColonizeUnit* u = units_get_const(units, unit_id);
  if (!u) {
    return;
  }
  colony_screen_close_subpanels(view);

  const ColonizeUnitType* type = units_type(units, u->type_index);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = (type && type->name[0]) ? type->name : "";
  tok.string1 = "";
  popup_msg_fill(
    messages,
    "COLONYUNIT",
    &tok,
    "",
    view->dock_orders_title,
    sizeof(view->dock_orders_title)
  );
  /* @COLONYUNIT's own @width=190, off the same latch as @CUSTOM above —
   * the 190 used to be a literal here. 0 = no directive → measured rows. */
  view->dock_orders_width = popup_msg_take_pending_width();

  /* Ships and cargo transports use @SHIPOPTIONS (including Wagon Trains,
   * port_bugs.md #782); ordinary land units use @UNITOPTIONS. Feeding Artillery
   * through the ship section exposed its Fortify row as "Anchor in harbor"
   * (port_bugs.md #941). */
  const bool transport = units_is_transport(units, unit_id);
  const char* options_section = transport ? "SHIPOPTIONS" : "UNITOPTIONS";
  const ColonizeMsgSection* opts = messages ? assets_msg_find(messages, options_section) : NULL;

  /* No MicroProse text in the binary: a missing options section = empty rows. */
  static const char* const k_fallback_options[] = {
    "",
    "",
    "",
    "",
    "",
    ""
  };
  const char* const* fallback = k_fallback_options;
  const int fallback_count = 6;
  const int cancel_index = 5;

  bool has_goods = false;
  const int holds = units_goods_hold_count(units, unit_id);
  for (int i = 0; i < holds; ++i) {
    if (u->hold_goods_amount[i] > 0 && u->hold_goods_amount[i] < 255) {
      has_goods = true;
      break;
    }
  }

  view->dock_orders_count = 0;
  const int line_count = (opts && opts->line_count > 0) ? opts->line_count : fallback_count;
  for (int i = 0; i < line_count && view->dock_orders_count < COLONY_DOCK_ORDERS_MAX; ++i) {
    const char* label = (opts && i < opts->line_count) ? opts->lines[i] : fallback[i];
    if (popup_msg_is_directive(label)) {
      continue;
    }
    ColonyDockOrderAction action = COLONY_DOCK_ORDER_CANCEL;
    bool enabled = true;
    if (i < cancel_index) {
      switch (i) {
      case 0:
        action = COLONY_DOCK_ORDER_ACTIVATE;
        enabled = (unit_id != view->transport_unit_id);
        break;
      case 1:
        action = COLONY_DOCK_ORDER_CLEAR;
        enabled = (u->orders != UNITS_ORDER_NONE);
        break;
      case 2:
        action = COLONY_DOCK_ORDER_SENTRY;
        enabled = (u->orders != UNITS_ORDER_SENTRY);
        break;
      case 3:
        /* port_bugs.md #783 / 99b8 case 4: enabled iff
         * `orders != 5 && orders != 6 && *(char*)(idx*0x1c+0x3146) != '\f'`
         * — a Wagon Train (@UNIT row 0x0c) never gets the Fortify /
         * "Anchor in harbor" row. */
        action = COLONY_DOCK_ORDER_FORTIFY;
        enabled =
          (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED &&
           u->type_index != UNITS_KIND_WAGON);
        break;
      case 4: /* "Unload all cargo" (99b8 case 5: holds_occupied != 0) */
        action = COLONY_DOCK_ORDER_UNLOAD_ALL;
        enabled = has_goods;
        break;
      default:
        break;
      }
    }
    if (!enabled) {
      /* DOS FUN_2f2b_5746 omits ineligible rows rather than graying them. */
      continue;
    }
    view->dock_orders_actions[view->dock_orders_count] = action;
    snprintf(
      view->dock_orders_labels[view->dock_orders_count],
      sizeof(view->dock_orders_labels[view->dock_orders_count]),
      "%s",
      label
    );
    view->dock_orders_count++;
  }
  if (view->dock_orders_count <= 0) {
    /* The cancel row is the section's own last line (@SHIPOPTIONS /
     * @UNITOPTIONS ""), not a second hardcoded copy of it. */
    const char* cancel_label = fallback[cancel_index];
    if (opts && cancel_index < opts->line_count && opts->lines[cancel_index][0]) {
      cancel_label = opts->lines[cancel_index];
    }
    view->dock_orders_actions[0] = COLONY_DOCK_ORDER_CANCEL;
    snprintf(
      view->dock_orders_labels[0], sizeof(view->dock_orders_labels[0]), "%s", cancel_label
    );
    view->dock_orders_count = 1;
  }
  view->dock_orders_unit_id = unit_id;
  view->dock_orders_selection = 0;
  view->dock_orders_open = true;
}

void colony_screen_minimap_origin(int* out_x, int* out_y) {
  /* Golden-measured grid origin (223,31): both margins use the section
   * height (the 120-wide section keeps the same 23px margin as vertically,
   * leaving the extra width on the right). */
  const int grid_px = COLONY_MINIMAP_GRID * COLONY_MINIMAP_TILE;
  const int margin = (COLONY_MINIMAP_SECTION_H - grid_px) / 2;
  if (out_x) {
    *out_x = COLONY_MINIMAP_SECTION_X + margin;
  }
  if (out_y) {
    *out_y = COLONY_MINIMAP_SECTION_Y + margin;
  }
}

/* ===================== PIK loading, view load/free (colony_screen_load_pik .. colony_screen_free) ===================== */

static bool colony_screen_load_pik(
  const char* data_dir,
  const char* filename,
  ColonizePikImage* out_image,
  char* err,
  size_t err_size
) {
  char pik_path[512];
  char pik_err[256];
  if (!dos_compat_normalize_asset_path(data_dir, filename, pik_path, sizeof(pik_path))) {
    snprintf(err, err_size, "%s path resolve failed", filename);
    return false;
  }
  if (!pik_load(pik_path, out_image, pik_err, sizeof(pik_err))) {
    snprintf(err, err_size, "%s: %s", filename, pik_err);
    return false;
  }
  return true;
}

/*
 * REMAP, not merge, for all four colony-screen sheets (PARCH / WOODTILE /
 * BUILDING / ICONS over WOODPANL.PIK).
 *
 * The reserved-DAC-block rule ("merge, never remap" — crown-europe batch,
 * reports_remap_exploits_sheet) applies to art sheets that OWN a block of DAC
 * slots the host screen leaves black. Checked against the shipped data: every
 * one of these four sheets is itself black across 152..251, which is exactly
 * the block WOODPANL.PIK leaves black — none of them reserves anything, they
 * all live in the shared low block, so merging would copy black over the
 * screen's own slots. Nearest-colour remap is the right treatment here, and
 * with WOODPANL as the destination it is very nearly the identity: of the
 * indices these sheets actually paint, only ICONS.SS 5 and 13 (11 px each)
 * disagree with the screen palette at all.
 */
bool colony_screen_load(ColonyScreenView* view, const char* data_dir, char* err, size_t err_size) {
  if (!view || !data_dir) {
    snprintf(err, err_size, "colony_screen_load bad args");
    return false;
  }
  memset(view, 0, sizeof(*view));
  /*
   * DOS DS:0x336 — the game-wide "show numbers on production badges"
   * toggle (colony 'N' key at 2f2b:667c, click at 2f2b:609e). Numbers on
   * is the normal state; the shared badge drawer also forces the number
   * whenever the icons pack into an uncountable smear (FUN_1097_0174's
   * step==1 override), so a toggled-off screen still labels big amounts.
   */
  view->show_production_numbers = true;

  if (!colony_screen_load_pik(data_dir, "WOODPANL.PIK", &view->frame, err, err_size)) {
    return false;
  }
  view->frame_ok = true;

  char ss_path[512];
  char ss_err[256];
  if (!dos_compat_normalize_asset_path(data_dir, "PARCH.SS", ss_path, sizeof(ss_path))) {
    snprintf(err, err_size, "PARCH.SS path resolve failed");
    colony_screen_free(view);
    return false;
  }
  if (!ss_load(ss_path, &view->parch, ss_err, sizeof(ss_err))) {
    snprintf(err, err_size, "PARCH.SS: %s", ss_err);
    colony_screen_free(view);
    return false;
  }
  assets_sheet_remap_to_palette(&view->parch, &view->frame.palette);
  view->parch_ok = true;

  if (!dos_compat_normalize_asset_path(data_dir, "WOODTILE.SS", ss_path, sizeof(ss_path))) {
    snprintf(err, err_size, "WOODTILE.SS path resolve failed");
    colony_screen_free(view);
    return false;
  }
  if (!ss_load(ss_path, &view->wood_tile, ss_err, sizeof(ss_err))) {
    snprintf(err, err_size, "WOODTILE.SS: %s", ss_err);
    colony_screen_free(view);
    return false;
  }
  assets_sheet_remap_to_palette(&view->wood_tile, &view->frame.palette);
  view->wood_tile_ok = true;

  if (!dos_compat_normalize_asset_path(data_dir, "BUILDING.SS", ss_path, sizeof(ss_path))) {
    snprintf(err, err_size, "BUILDING.SS path resolve failed");
    colony_screen_free(view);
    return false;
  }
  if (!ss_load(ss_path, &view->buildings, ss_err, sizeof(ss_err))) {
    snprintf(err, err_size, "BUILDING.SS: %s", ss_err);
    colony_screen_free(view);
    return false;
  }
  assets_sheet_remap_to_palette(&view->buildings, &view->frame.palette);
  view->buildings_ok = true;

  if (!dos_compat_normalize_asset_path(data_dir, "ICONS.SS", ss_path, sizeof(ss_path))) {
    snprintf(err, err_size, "ICONS.SS path resolve failed");
    colony_screen_free(view);
    return false;
  }
  if (!ss_load(ss_path, &view->icons, ss_err, sizeof(ss_err))) {
    snprintf(err, err_size, "ICONS.SS: %s", ss_err);
    colony_screen_free(view);
    return false;
  }
  assets_sheet_remap_to_palette(&view->icons, &view->frame.palette);
  view->icons_ok = true;

  if (!colony_screen_load_pik(data_dir, "COLONY.PIK", &view->bottom_panel, err, err_size)) {
    colony_screen_free(view);
    return false;
  }
  view->bottom_panel_ok = true;

  /*
   * FONTINTR.FF — the dialog font slot DOS restores after every FONTTINY
   * swap (game_loop.c's begin_menu_font / europe list dialogs). Popups over
   * this screen already use it (game_loop.c picks intro_font for howmuch /
   * name entry), so the screen's own GAME.TXT popups take it too unless
   * their section carries @smallfont. Non-fatal: without it the popups fall
   * back to the screen font, which is what they used before.
   */
  char font_path[512];
  char font_err[256];
  if (dos_compat_normalize_asset_path(data_dir, "FONTINTR.FF", font_path, sizeof(font_path)) &&
      font_file_load(font_path, &view->dialog_font, font_err, sizeof(font_err))) {
    view->dialog_font_ok = true;
  } else {
    diag_warn("Colony screen: FONTINTR.FF unavailable — popups keep the screen font");
  }

  colony_screen_set_status(view, "Colony ready. Esc or C returns to map.");
  diag_info(
    "Colony screen loaded (WOODPANL %dx%d, PARCH %d, WOODTILE %d, BUILDING %d, ICONS %d, COLONY.PIK %dx%d)",
    view->frame.width,
    view->frame.height,
    view->parch.sprite_count,
    view->wood_tile.sprite_count,
    view->buildings.sprite_count,
    view->icons.sprite_count,
    view->bottom_panel.width,
    view->bottom_panel.height
  );
  return true;
}

void colony_screen_free(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  pik_free(&view->frame);
  ss_free(&view->parch);
  ss_free(&view->wood_tile);
  ss_free(&view->buildings);
  ss_free(&view->icons);
  pik_free(&view->bottom_panel);
  if (view->dialog_font_ok) {
    font_file_free(&view->dialog_font);
  }
  memset(view, 0, sizeof(*view));
}
