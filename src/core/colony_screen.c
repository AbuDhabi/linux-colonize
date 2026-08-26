#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/colony_preview.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/turn.h"
#include "core/ui_button.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static void colony_screen_fill_rect(
  ColonizeFramebuffer8* framebuffer,
  int x0,
  int y0,
  int x1,
  int y1,
  uint8_t color
);

void colony_screen_set_status(ColonyScreenView* view, const char* text) {
  if (!view) {
    return;
  }
  snprintf(view->status, sizeof(view->status), "%s", text ? text : "");
}

void colony_screen_reset_ui(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  view->selected_colonist = -1;
  view->selected_outside_unit = -1;
  view->show_production_numbers = false;
  view->multi_mode = COLONY_MULTI_PRODUCTION;
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
  view->last_delta_valid = false;
  memset(&view->last_delta, 0, sizeof(view->last_delta));
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
  if (view->transport_unit_id < 0 && view->docked_transport_count == 1) {
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
}

void colony_screen_refresh_preview(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1
) {
  if (!view) {
    return;
  }
  if (!pool || !colony) {
    view->preview_valid = false;
    return;
  }
  colony_preview_compute(pool, colony, map, col1, &view->preview);
  view->preview_valid = true;
}

void colony_screen_set_delta(ColonyScreenView* view, const ColonizeColonyProdDelta* delta) {
  if (!view) {
    return;
  }
  if (!delta) {
    view->last_delta_valid = false;
    return;
  }
  view->last_delta = *delta;
  view->last_delta_valid = true;
}

void colony_screen_close_construction(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  view->construction_open = false;
  view->construction_selection = 0;
}

void colony_screen_open_construction(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  int colony_id,
  const ColoniesBuildableOpts* buildable_opts
) {
  if (!view) {
    return;
  }
  colony_screen_close_jobs(view);
  colony_screen_close_eject(view);
  colony_screen_close_message(view);
  colony_screen_close_dock_orders(view);
  colony_screen_close_custom_house(view);
  view->buildable_count = colonies_list_buildable(
    pool, colony_id, view->buildable_ids, COLONY_BUILDABLE_MAX, buildable_opts
  );
  view->construction_open = true;
  view->construction_selection = 0;
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
}

void colony_screen_open_custom_house(
  ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeMsgCatalog* messages
) {
  if (!view || !colony) {
    return;
  }
  colony_screen_close_jobs(view);
  colony_screen_close_construction(view);
  colony_screen_close_eject(view);
  colony_screen_close_message(view);
  colony_screen_close_dock_orders(view);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  popup_msg_fill(
    messages,
    "CUSTOM",
    &tok,
    "Which cargos shall our Custom House export?",
    view->custom_house_title,
    sizeof(view->custom_house_title)
  );
  /* Every cargo but Food gets a row (col1_save.h's ColonizeCol1CustomHouse
   * bitfield has all 16, Food included, so the save format itself treats
   * this as a full checklist) — player-reported: filtering to only
   * europe_cargo_export_eligible()'s autosell denylist left Tools and
   * Muskets (and Horses) missing from the popup. That denylist still gates
   * europe_custom_house_autosell()'s actual EOT sell — toggling one of
   * those rows on here just never has an effect, same as DOS's own
   * checklist presumably allows (the bit exists either way). */
  view->custom_house_count = 0;
  for (int c = 1;
       c < COLONIZE_CARGO_COUNT && view->custom_house_count < COLONIZE_CARGO_COUNT;
       ++c) {
    view->custom_house_cargo_ids[view->custom_house_count++] = c;
  }
  view->custom_house_open = true;
}

void colony_screen_open_message_ok(ColonyScreenView* view, const char* text) {
  if (!view) {
    return;
  }
  colony_screen_close_jobs(view);
  colony_screen_close_construction(view);
  colony_screen_close_eject(view);
  colony_screen_close_dock_orders(view);
  colony_screen_close_custom_house(view);
  view->message_kind = COLONY_MSG_OK;
  snprintf(view->message_text, sizeof(view->message_text), "%s", text ? text : "");
  view->message_choice0[0] = '\0';
  view->message_choice1[0] = '\0';
  view->message_selection = 0;
  view->pending_eject_colonist = -1;
}

void colony_screen_open_abandon_confirm(
  ColonyScreenView* view,
  int colonist_index,
  int role,
  const char* body,
  const char* choice_yes,
  const char* choice_no
) {
  if (!view) {
    return;
  }
  colony_screen_close_jobs(view);
  colony_screen_close_construction(view);
  colony_screen_close_eject(view);
  colony_screen_close_dock_orders(view);
  colony_screen_close_custom_house(view);
  view->message_kind = COLONY_MSG_CONFIRM;
  snprintf(
    view->message_text,
    sizeof(view->message_text),
    "%s",
    body && body[0] ? body : "Shall we abandon this colony?"
  );
  snprintf(
    view->message_choice0,
    sizeof(view->message_choice0),
    "%s",
    choice_yes && choice_yes[0] ? choice_yes : "Yes"
  );
  snprintf(
    view->message_choice1,
    sizeof(view->message_choice1),
    "%s",
    choice_no && choice_no[0] ? choice_no : "No"
  );
  view->message_selection = 1; /* @default=2 → No */
  view->pending_eject_colonist = colonist_index;
  view->pending_eject_role = role;
}

void colony_screen_open_eject(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index
) {
  if (!view || !pool || colonist_index < 0) {
    return;
  }
  colony_screen_close_jobs(view);
  colony_screen_close_construction(view);
  colony_screen_close_message(view);
  colony_screen_close_dock_orders(view);
  colony_screen_close_custom_house(view);
  view->eject_colonist_index = colonist_index;
  view->eject_unit_id = -1;
  view->eject_role_count = colonies_list_eject_roles(
    pool, colony_id, colonist_index, view->eject_roles, COLONIZE_EJECT_ROLE_COUNT
  );
  if (view->eject_role_count <= 0) {
    view->eject_roles[0] = COLONIZE_EJECT_COLONIST;
    view->eject_role_count = 1;
  }
  view->eject_open = true;
  view->eject_selection = 0;
}

void colony_screen_open_jobs(
  ColonyScreenView* view,
  const ColonizeWorldMap* map,
  const ColonizeColony* colony,
  int tile_index
) {
  if (!view || !colony || tile_index < 0 || tile_index >= COLONIZE_COLONY_FIELD_TILES) {
    return;
  }
  colony_screen_close_construction(view);
  colony_screen_close_eject(view);
  colony_screen_close_dock_orders(view);
  colony_screen_close_custom_house(view);
  view->jobs_tile_index = tile_index;
  view->job_count = 0;
  int dx = 0;
  int dy = 0;
  if (!colonies_field_tile_delta(tile_index, &dx, &dy)) {
    return;
  }
  const int tx = colony->x + dx;
  const int ty = colony->y + dy;
  for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT && view->job_count < COLONY_JOB_LIST_MAX; ++job) {
    const int yld = map ? colony_yield_for_tile(map, tx, ty, job) : 0;
    if (yld > 0) {
      view->job_ids[view->job_count++] = job;
    }
  }
  view->jobs_open = true;
  view->jobs_selection = 0;
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
  colony_screen_close_jobs(view);
  colony_screen_close_construction(view);
  colony_screen_close_eject(view);
  colony_screen_close_message(view);
  colony_screen_close_custom_house(view);

  const ColonizeUnitType* type = units_type(units, u->type_index);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = (type && type->name[0]) ? type->name : "Transport";
  tok.string1 = "";
  popup_msg_fill(
    messages,
    "COLONYUNIT",
    &tok,
    "Options for %STRING0%STRING1:",
    view->dock_orders_title,
    sizeof(view->dock_orders_title)
  );

  const bool sea = units_is_sea(units, unit_id);
  const ColonizeMsgSection* opts =
    messages ? assets_msg_find(messages, sea ? "SHIPOPTIONS" : "UNITOPTIONS") : NULL;

  /* GAME.TXT @SHIPOPTIONS / @UNITOPTIONS verbatim, if the catalog is missing. */
  static const char* const k_fallback_ship[] = {
    "Move to front.",
    "Clear orders.",
    "Sentry.",
    "Anchor in harbor (\"Fortify\").",
    "Unload all cargo.",
    "No changes."
  };
  static const char* const k_fallback_land[] = {
    "Move to front.", "Clear orders.", "Sentry / Board ship.", "Fortify.", "No changes."
  };
  const char* const* fallback = sea ? k_fallback_ship : k_fallback_land;
  const int fallback_count = sea ? 6 : 5;
  const int cancel_index = sea ? 5 : 4;

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
        action = COLONY_DOCK_ORDER_FORTIFY;
        enabled = (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED);
        break;
      case 4: /* sea only (cancel_index==5): "Unload all cargo" */
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
    view->dock_orders_actions[0] = COLONY_DOCK_ORDER_CANCEL;
    snprintf(
      view->dock_orders_labels[0], sizeof(view->dock_orders_labels[0]), "%s", "No changes."
    );
    view->dock_orders_count = 1;
  }
  view->dock_orders_unit_id = unit_id;
  view->dock_orders_selection = 0;
  view->dock_orders_open = true;
}

void colony_screen_minimap_origin(int* out_x, int* out_y) {
  const int grid_px = COLONY_MINIMAP_GRID * COLONY_MINIMAP_TILE;
  if (out_x) {
    *out_x = COLONY_MINIMAP_SECTION_X + (COLONY_MINIMAP_SECTION_W - grid_px) / 2;
  }
  if (out_y) {
    *out_y = COLONY_MINIMAP_SECTION_Y + (COLONY_MINIMAP_SECTION_H - grid_px) / 2;
  }
}

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

/* Remap sprite pixels from src_pal colors onto nearest indices in dst_pal. */
static void remap_sheet_to_palette(
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

bool colony_screen_load(ColonyScreenView* view, const char* data_dir, char* err, size_t err_size) {
  if (!view || !data_dir) {
    snprintf(err, err_size, "colony_screen_load bad args");
    return false;
  }
  memset(view, 0, sizeof(*view));

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
  remap_sheet_to_palette(&view->parch, &view->frame.palette);
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
  remap_sheet_to_palette(&view->wood_tile, &view->frame.palette);
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
  remap_sheet_to_palette(&view->buildings, &view->frame.palette);
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
  remap_sheet_to_palette(&view->icons, &view->frame.palette);
  view->icons_ok = true;

  if (!colony_screen_load_pik(data_dir, "COLONY.PIK", &view->bottom_panel, err, err_size)) {
    colony_screen_free(view);
    return false;
  }
  view->bottom_panel_ok = true;

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
  memset(view, 0, sizeof(*view));
}

static void colony_screen_tile_rect(
  const ColonizeSpriteSheet* sheet,
  int origin_x,
  int origin_y,
  int rect_w,
  int rect_h,
  ColonizeFramebuffer8* framebuffer
) {
  if (!sheet || sheet->sprite_count < 1 || !framebuffer || rect_w <= 0 || rect_h <= 0) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[0];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int x1 = origin_x + rect_w;
  const int y1 = origin_y + rect_h;
  for (int y = origin_y; y < y1; y += tile->height) {
    for (int x = origin_x; x < x1; x += tile->width) {
      for (int sy = 0; sy < tile->height; ++sy) {
        const int dy = y + sy;
        if (dy < origin_y || dy >= y1 || dy < 0 || dy >= framebuffer->height) {
          continue;
        }
        for (int sx = 0; sx < tile->width; ++sx) {
          const int dx = x + sx;
          if (dx < origin_x || dx >= x1 || dx < 0 || dx >= framebuffer->width) {
            continue;
          }
          const uint8_t px = tile->pixels[sy * tile->width + sx];
          if (px == COLONIZE_SS_TRANSPARENT) {
            continue;
          }
          framebuffer->pixels[dy * framebuffer->width + dx] = px;
        }
      }
    }
  }
}

static void colony_screen_fill_parch(const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer) {
  if (!view || !view->parch_ok) {
    return;
  }
  colony_screen_tile_rect(
    &view->parch,
    COLONY_VIEWPORT_X,
    COLONY_VIEWPORT_Y,
    COLONY_PARCH_FILL_W,
    COLONY_PARCH_FILL_H,
    framebuffer
  );
}

static void colony_screen_fill_wood_tile(const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer) {
  if (!view || !view->wood_tile_ok) {
    return;
  }
  colony_screen_tile_rect(
    &view->wood_tile,
    COLONY_MINIMAP_SECTION_X,
    COLONY_MINIMAP_SECTION_Y,
    COLONY_MINIMAP_SECTION_W,
    COLONY_MINIMAP_SECTION_H,
    framebuffer
  );
}

static void colony_screen_draw_hline(ColonizeFramebuffer8* framebuffer, int y, int color) {
  if (!framebuffer || !framebuffer->pixels || y < 0 || y >= framebuffer->height) {
    return;
  }
  uint8_t c = (uint8_t)color;
  for (int x = 0; x < framebuffer->width; ++x) {
    framebuffer->pixels[y * framebuffer->width + x] = c;
  }
}

static void colony_screen_draw_vline(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y0,
  int y1,
  int color
) {
  if (!framebuffer || !framebuffer->pixels || x < 0 || x >= framebuffer->width) {
    return;
  }
  if (y0 > y1) {
    const int t = y0;
    y0 = y1;
    y1 = t;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (y1 >= framebuffer->height) {
    y1 = framebuffer->height - 1;
  }
  uint8_t c = (uint8_t)color;
  for (int y = y0; y <= y1; ++y) {
    framebuffer->pixels[y * framebuffer->width + x] = c;
  }
}

static void colony_screen_draw_top_bar(
  const ColonizeColony* colony,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!font || !framebuffer) {
    return;
  }
  /* DOS FUN_2f2b_0fce: one centered string "<Name>.  <Season>, <Year>.
   * Gold: <N>$" — not three separately-positioned fields (golden-measured:
   * single green (WOODPANL.PIK idx 68, exact RGB match) run, native width
   * ~158px centered at x~160, y=2; period/comma placement confirmed by
   * zooming the golden's punctuation glyphs — the mark after the colony
   * name and after the year is a plain baseline dot (period), the one after
   * the season has a trailing hooked descender (comma)). Built locally
   * rather than via turn_format_date() (shared by other screens that want
   * plain "Season Year" with no punctuation). */
  const char* name = (colony && colony->name[0]) ? colony->name : "Colony";
  char date[32];
  turn_format_date(game_year, game_autumn, date, sizeof(date));
  char season[16] = "";
  unsigned year = 0;
  sscanf(date, "%15s %u", season, &year);
  char line[96];
  snprintf(line, sizeof(line), "%s.  %s, %u.  Gold: %d$", name, season, year, gold);
  const int w = font_text_width(font, line);
  const int x = (COLONY_SCREEN_WIDTH - w) / 2;
  /* bugs.md item 1: golden (new_amsterdam_production.png) ink top edge
   * measures native y=1, not 2 — title sat 1px too low. */
  font_draw_text(font, framebuffer, x, 1, line, 68);
}

static void colony_screen_draw_selection_box(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  uint8_t color
) {
  if (!framebuffer || w <= 0 || h <= 0) {
    return;
  }
  colony_screen_fill_rect(framebuffer, x, y, x + w, y + 1, color);
  colony_screen_fill_rect(framebuffer, x, y + h - 1, x + w, y + h, color);
  colony_screen_fill_rect(framebuffer, x, y, x + 1, y + h, color);
  colony_screen_fill_rect(framebuffer, x + w - 1, y, x + w, y + h, color);
}

/* Tight green box around an ICONS.SS sprite at (x,y), 1px margin. */
static void colony_screen_draw_icon_selection(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  int sprite,
  int x,
  int y
) {
  if (!view || !view->icons_ok || !framebuffer || sprite < 0 || sprite >= view->icons.sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &view->icons.sprites[sprite];
  if (!sp || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  colony_screen_draw_selection_box(framebuffer, x - 1, y - 1, sp->width + 2, sp->height + 2, 10);
}

/* Selection frame for unit_chrome_blit_unit art (shadow + orders + sprite). */
static void colony_screen_draw_chrome_selection(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  int sprite,
  int x,
  int y
) {
  if (!view || !view->icons_ok || !framebuffer || sprite < 0 || sprite >= view->icons.sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &view->icons.sprites[sprite];
  if (!sp || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  int fx = 0;
  int fy = 0;
  int fw = 0;
  int fh = 0;
  unit_chrome_selection_frame(x, y, sp->width, sp->height, &fx, &fy, &fw, &fh);
  colony_screen_draw_selection_box(framebuffer, fx, fy, fw, fh, 10);
}

static void colony_screen_blit_icon(
  const ColonyScreenView* view,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y
) {
  if (!view || !view->icons_ok || !framebuffer || sprite < 0 || sprite >= view->icons.sprite_count) {
    return;
  }
  unit_chrome_blit(
    framebuffer, NULL, &view->icons, sprite, x, y, UNIT_CHROME_PLAIN_SPRITE, 0, 0, -1, 0, false, false,
    -1, -1
  );
}

/*
 * Colonist/on-tile-unit figures only (not buildings, cargo, or badge
 * icons): the same black 2px-left shadow silhouette unit_chrome uses for
 * units on the overland map (UNIT_CHROME_SHADOW_DX) — same amount of
 * shadow, just without the orders/allegiance box this screen doesn't draw
 * on its own figures. */
static void colony_screen_blit_icon_shadowed(
  const ColonyScreenView* view,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y
) {
  if (!view || !view->icons_ok || !framebuffer || sprite < 0 || sprite >= view->icons.sprite_count) {
    return;
  }
  unit_chrome_blit(
    framebuffer, NULL, &view->icons, sprite, x, y, UNIT_CHROME_SPRITE_WITH_SHADOW, 0, 0, -1, 0, false,
    false, -1, -1
  );
}

static void colony_screen_blit_cargo(
  const ColonyScreenView* view,
  int cargo,
  bool grey,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y
) {
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }
  const int sprite = (grey ? COLONY_CARGO_GREY_BASE : COLONY_CARGO_ICON_BASE) + cargo;
  colony_screen_blit_icon(view, sprite, framebuffer, x, y);
}

/*
 * Note 1: one icon per unit of resource, evenly spaced in [x,y,w,h].
 * Number (left, black outline + number_color) appears when always_show_number,
 * or when start-to-start spacing between icons is <= 1px (nearly total overlap).
 */
static void colony_screen_draw_outlined_number(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t fg_color
) {
  if (!font || !framebuffer || !text) {
    return;
  }
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      font_draw_text(font, framebuffer, x + dx, y + dy, text, 0);
    }
  }
  font_draw_text(font, framebuffer, x, y, text, fg_color);
}

/*
 * Note 1 layout: evenly spaced icon x positions in [x, x+w).
 * ref_iw is the reference sprite width (usually the first icon).
 * Returns start-to-start step (0 if fully stacked).
 */
static int colony_screen_icon_strip_layout(int x, int w, int count, int ref_iw, int* out_x) {
  if (count <= 0 || !out_x || ref_iw <= 0) {
    return 0;
  }
  if (count == 1) {
    out_x[0] = x + (w - ref_iw) / 2;
    return ref_iw;
  }
  if (w <= ref_iw) {
    for (int i = 0; i < count; ++i) {
      out_x[i] = x;
    }
    return 0;
  }
  const int span = w - ref_iw;
  const int start_step = span / (count - 1);
  for (int i = 0; i < count; ++i) {
    out_x[i] = x + (i * span) / (count - 1);
  }
  return start_step;
}

/*
 * One icon per entry, evenly spaced (Note 1). Heterogeneous sprites allowed.
 * selected_index >= 0 draws a selection box on that item (colonists / units);
 * pass -1 for non-selectable resource strips.
 */
static void colony_screen_draw_icon_strip(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  const int* icons,
  int count,
  int selected_index,
  uint8_t number_color,
  bool always_show_number
) {
  if (!view || !view->icons_ok || !framebuffer || !icons || count <= 0 || w <= 0 || h <= 0) {
    return;
  }
  if (count > COLONY_OUTSIDE_MAX) {
    count = COLONY_OUTSIDE_MAX;
  }
  int ref_iw = 12;
  {
    const int first = icons[0];
    if (first >= 0 && first < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[first];
      if (sp && sp->width > 0) {
        ref_iw = sp->width;
      }
    }
  }
  int xs[COLONY_OUTSIDE_MAX];
  const int start_step = colony_screen_icon_strip_layout(x, w, count, ref_iw, xs);
  for (int i = 0; i < count; ++i) {
    const int icon = icons[i];
    if (icon < 0 || icon >= view->icons.sprite_count) {
      continue;
    }
    const ColonizeSprite* sp = &view->icons.sprites[icon];
    if (!sp || !sp->pixels || sp->width <= 0 || sp->height <= 0) {
      continue;
    }
    const int iy = y + (h - sp->height) / 2;
    colony_screen_blit_icon_shadowed(view, icon, framebuffer, xs[i], iy);
    if (selected_index == i) {
      colony_screen_draw_icon_selection(view, framebuffer, icon, xs[i], iy);
    }
  }
  if ((always_show_number || start_step <= 1) && font) {
    char num[12];
    snprintf(num, sizeof(num), "%d", count);
    colony_screen_draw_outlined_number(
      font, framebuffer, x + 1, y + (h > 6 ? 1 : 0), num, number_color
    );
  }
}

/*
 * One icon per unit, evenly spaced. When amount0+amount1 > 0, sprites for
 * amount0 use icon0 first, then amount1 use icon1 (e.g. fish then grain).
 * Not selectable (resources).
 */
static void colony_screen_draw_resource_count_pair(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  int icon0,
  int amount0,
  int icon1,
  int amount1,
  uint8_t number_color,
  bool always_show_number
) {
  if (!view || !view->icons_ok || !framebuffer || w <= 0 || h <= 0) {
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
  /* One icon per unit of resource, spread across [x,x+w) (Note 1) — golden-
   * confirmed via the newer "numberless" reference captures (`new_amsterdam
   * _production_numberless.png` / `recife_..._numberless.png`): DOS really
   * does repeat the icon `amount` times, not draw one static icon. What a
   * prior pass read off the (number-mode) goldens as a deliberately painted
   * "content-sized black background pill" was actually just this: dozens of
   * black-bordered icon copies overlapping almost completely, so only the
   * last (topmost) one's art is visible and everything else fuses into a
   * black smear — real, and it *scales with amount* (a bigger stock badge
   * genuinely smears wider), which a fixed single-icon-plus-box never did.
   * That box is gone; `amount` copies are blit for real, exactly like
   * `colony_screen_draw_icon_strip` does for worker/unit strips, except this
   * function's number is unconditional — confirmed against *both* the
   * numbered and numberless goldens that resource-count badges (settlement/
   * Production-tab/People-band/area-view) always show their number
   * regardless of amount; the "always show numbers" toggle only changes the
   * area-view field-tile badges (a different code path), not these. See
   * docs/colony_screen.md. */
  const int first_icon = amount0 > 0 ? icon0 : icon1;
  if (first_icon < 0 || first_icon >= view->icons.sprite_count) {
    return;
  }
  if (amount1 > 0 && (icon1 < 0 || icon1 >= view->icons.sprite_count)) {
    return;
  }
  if (amount0 > 0 && (icon0 < 0 || icon0 >= view->icons.sprite_count)) {
    return;
  }
  const ColonizeSprite* sp = &view->icons.sprites[first_icon];
  if (!sp || !sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  const int iw = sp->width;
  const int ih = sp->height;
  const int iy = y + (h - ih) / 2;
  if (amount == 1) {
    const int ix = x + (w - iw) / 2;
    ss_blit_sprite(&view->icons, first_icon, framebuffer, ix, iy);
  } else if (w <= iw) {
    for (int i = 0; i < amount; ++i) {
      const int icon = (i < amount0) ? icon0 : icon1;
      ss_blit_sprite(&view->icons, icon, framebuffer, x, iy);
    }
  } else {
    const int span = w - iw;
    for (int i = 0; i < amount; ++i) {
      const int icon = (i < amount0) ? icon0 : icon1;
      const int ix = x + (i * span) / (amount - 1);
      ss_blit_sprite(&view->icons, icon, framebuffer, ix, iy);
    }
  }
  (void)always_show_number; /* number is unconditional here — see comment above */
  if (font) {
    char num[12];
    snprintf(num, sizeof(num), "%d", amount);
    colony_screen_draw_outlined_number(
      font, framebuffer, x + 1, y + (h > 6 ? 1 : 0), num, number_color
    );
  }
}

static void colony_screen_draw_resource_count(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  int icon_sprite,
  int amount,
  uint8_t number_color,
  bool always_show_number
) {
  colony_screen_draw_resource_count_pair(
    view,
    font,
    framebuffer,
    x,
    y,
    w,
    h,
    icon_sprite,
    amount,
    icon_sprite,
    0,
    number_color,
    always_show_number
  );
}

/* Nearest-neighbor 1.5× blit (16→24 for standard terrain cells). */
static void colony_screen_blit_scaled_15(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y
) {
  if (!sheet || !framebuffer || !framebuffer->pixels || sprite_index < 0 ||
      sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* sprite = &sheet->sprites[sprite_index];
  if (!sprite->pixels || sprite->width <= 0 || sprite->height <= 0) {
    return;
  }
  const int dw = (sprite->width * 3) / 2;
  const int dh = (sprite->height * 3) / 2;
  for (int dy = 0; dy < dh; ++dy) {
    const int sy = dy * sprite->height / dh;
    const int fy = dst_y + dy;
    if (fy < 0 || fy >= framebuffer->height) {
      continue;
    }
    for (int dx = 0; dx < dw; ++dx) {
      const int sx = dx * sprite->width / dw;
      const int fx = dst_x + dx;
      if (fx < 0 || fx >= framebuffer->width) {
        continue;
      }
      const uint8_t color = sprite->pixels[sy * sprite->width + sx];
      if (color == COLONIZE_SS_TRANSPARENT) {
        continue;
      }
      framebuffer->pixels[fy * framebuffer->width + fx] = color;
    }
  }
}

static void colony_screen_blit_scaled_15_where_dest(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y,
  uint8_t match_color
) {
  if (!sheet || !framebuffer || !framebuffer->pixels || sprite_index < 0 ||
      sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* sprite = &sheet->sprites[sprite_index];
  if (!sprite->pixels || sprite->width <= 0 || sprite->height <= 0) {
    return;
  }
  const int dw = (sprite->width * 3) / 2;
  const int dh = (sprite->height * 3) / 2;
  for (int dy = 0; dy < dh; ++dy) {
    const int sy = dy * sprite->height / dh;
    const int fy = dst_y + dy;
    if (fy < 0 || fy >= framebuffer->height) {
      continue;
    }
    for (int dx = 0; dx < dw; ++dx) {
      const int sx = dx * sprite->width / dw;
      const int fx = dst_x + dx;
      if (fx < 0 || fx >= framebuffer->width) {
        continue;
      }
      const int di = fy * framebuffer->width + fx;
      if (framebuffer->pixels[di] != match_color) {
        continue;
      }
      const uint8_t color = sprite->pixels[sy * sprite->width + sx];
      if (color == COLONIZE_SS_TRANSPARENT) {
        continue;
      }
      framebuffer->pixels[di] = color;
    }
  }
}

static void colony_screen_debug_building_rect(
  const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer, int sprite, int x, int y
);

/*
 * Approximate collage positions inside the PARCH buildings section.
 * Exact DOS placement is not recovered yet; this is a readable bring-up layout.
 *
 * BUILDING.SS notes:
 *   #16 — full pre-stockade fence (bottom-right of buildings section)
 *   #45 — empty coastal placeholder (trees + shore); docks/drydock/shipyard replace it
 *   #42–44,46–47 — empty-slot tree clumps (large/med/small)
 *
 * Classic bottom-right stack: coast/docks (75×48) above fence/stockade (73×18).
 */
enum {
  COLONY_FENCE_SPRITE = 16,
  COLONY_TREE_LARGE = 42,
  COLONY_TREE_MED = 43,
  COLONY_TREE_SMALL = 44,
  COLONY_COAST_PLACEHOLDER = 45,
  COLONY_FENCE_W = 73,
  COLONY_FENCE_H = 18,
  COLONY_COAST_W = 75,
  COLONY_COAST_H = 48,
  COLONY_BUILDING_WORKERS_MAX = 3
};

/*
 * Golden-exact overrides for New Amsterdam and Recife (dutch-reports.SAV),
 * the two colonies with reference screenshots. Casuistry, not a general
 * solution — this placeholder algorithm is meant to be replaced by a real
 * port of DOS's own tables later (see the block comment above
 * colony_screen_assign_slot_positions); until then, these two colonies
 * should look exactly like their goldens so the rest of the colony-screen
 * work can be tuned against real DOS pixels instead of this port's
 * invented layout. Positions came from brute-force template matching each
 * real building's actual sprite against the golden PNGs (BUILDING.SS
 * sprite, palette-converted, slid over the downscaled-to-native golden
 * until pixel-SAD is minimized — exact-zero score for most). Only *built*
 * structures are overridden; unbuilt categories (drawn as a decorative
 * tree-clump placeholder) keep the general algorithm — template matching
 * them was inconclusive (DOS scatters filler trees pretty freely, not from
 * the same fixed per-class pool used for real buildings), and they're not
 * interactive, so exact placement doesn't matter the way it does for a
 * real building. docks_x/y and fence_x/y (both colonies: (123,55) and
 * (123,106)) matched exactly too, including on Recife's *unbuilt* coast
 * placeholder — strong evidence that corner is a genuinely fixed screen
 * slot, not random; a good candidate to promote to the general formula
 * later. Indices in `pos[]` follow k_building_slots[] order (0 town_hall …
 * 13 custom); -1 = no override, use the algorithm.
 *
 * `pos[]` values are relative to the viewport origin (added to slot_ox/
 * slot_oy — COLONY_VIEWPORT_X/Y — same as the general algorithm's pool
 * points), NOT absolute framebuffer coordinates. The matcher searched the
 * full 320×200 golden frame and returned absolute hits; every value here
 * has already had (COLONY_VIEWPORT_X, COLONY_VIEWPORT_Y) = (1,8)
 * subtracted out (an earlier pass skipped that and every override sat 1px
 * right/8px down from its real golden spot — player-caught). Once
 * corrected, most values landed exactly on an existing `k_group_*_slots`
 * pool point (real DOS reuses the same candidate pool this port's general
 * algorithm draws from) — nice independent confirmation the pools
 * themselves are right, only DOS's per-colony *assignment* differs from
 * this port's synthetic one. */
#define COLONY_OVERRIDE_NONE (-1)
#define COLONY_OVERRIDE_HIDDEN (-2) /* don't draw this (unbuilt) placeholder at all */
#define COLONY_SLOT_HIDDEN (-30000) /* sentinel xs[]/ys[] value for a hidden slot */
typedef struct ColonyPlacementOverride {
  int x, y; /* colony's fixed map position, keys the override */
  int pos[14][2]; /* k_building_slots order; {-1,-1} = not overridden, {-2,-2} = hidden */
  int docks_x, docks_y, fence_x, fence_y; /* -1 = use the formula default (already viewport-absolute) */
} ColonyPlacementOverride;

static const ColonyPlacementOverride k_colony_overrides[] = {
  { /* New Amsterdam */
    50, 43,
    {
      {65, 79},   /* town_hall */
      {-1, -1},   /* church (unbuilt) */
      {14, 94},   /* school (unbuilt, but tree matched exactly) */
      {127, 45},  /* carpenter */
      {4, 33},    /* blacksmith */
      {55, 5},    /* weaver */
      {172, 10},  /* tobacco */
      {144, 7},   /* rum */
      {36, 37},   /* fur */
      {9, 68},    /* warehouse */
      {5, 6},     /* armory */
      {95, 45},   /* press */
      {-2, -2},   /* stable (unbuilt; every SMALL pool point is claimed by
                     the other 7 — real DOS evidently fits it somewhere,
                     this port's pool doesn't have the slack; hide rather
                     than force an overlap onto a real building) */
      {66, 46},   /* custom */
    },
    123, 55, 123, 106
  },
  { /* Recife */
    41, 38,
    {
      {86, 3},    /* town_hall */
      {-1, -1},   /* church (unbuilt) */
      {-1, -1},   /* school (unbuilt) */
      {127, 45},  /* carpenter */
      {172, 10},  /* blacksmith */
      {66, 46},   /* weaver */
      {95, 45},   /* tobacco */
      {4, 33},    /* rum */
      {144, 7},   /* fur */
      {-1, -1},   /* warehouse (unbuilt) */
      {-1, -1},   /* armory (unbuilt) */
      {-1, -1},   /* press (unbuilt) */
      {-1, -1},   /* stable (unbuilt) */
      {-2, -2},   /* custom (unbuilt; exactly 3 SMALL points remain free for
                     3 unbuilt categories here — no slack, so the one bad
                     point always gets forced onto whichever is processed
                     last. Same fix as New Amsterdam's stable: hide it. */
    },
    123, 55, 123, 106
  },
};
static const int k_colony_override_count =
  (int)(sizeof(k_colony_overrides) / sizeof(k_colony_overrides[0]));

/*
 * Every colony now gets one of these two golden-verified layouts — not
 * just New Amsterdam and Recife themselves. Player's ask: since both
 * layouts are now known-good (no overlaps, exact golden match on their
 * source colony), reuse them everywhere instead of the general RNG-pool
 * algorithm, to see how two *real* arrangements read against other
 * colonies' actual built/unbuilt mixes. New Amsterdam/Recife keep their
 * own exact table (matches `k_colony_overrides[]` order: index 0, 1); any
 * other colony gets one of the two picked deterministically from its own
 * (x,y) — stable across reloads, arbitrary-looking across colonies. The
 * general algorithm doesn't disappear: it still resolves any category
 * either table leaves at COLONY_OVERRIDE_NONE (church's slot on both, for
 * instance), and colony_screen_assign_slot_positions() falls back to it
 * for a HIDDEN category too if this *other* colony actually has it built
 * (HIDDEN only ever meant "no room in this table for the tree filler" —
 * it must never swallow a real building).
 */
#define COLONY_LAYOUT_PICK_SALT 0x9e17u

static const ColonyPlacementOverride* colony_screen_find_override(const ColonizeColony* colony) {
  if (!colony) {
    return NULL;
  }
  for (int i = 0; i < k_colony_override_count; ++i) {
    if (k_colony_overrides[i].x == colony->x && k_colony_overrides[i].y == colony->y) {
      return &k_colony_overrides[i];
    }
  }
  ColonizeDosRng rng;
  const uint32_t xy = ((uint32_t)colony->y << 8) | (uint32_t)colony->x;
  dos_rng_seed(&rng, xy ^ COLONY_LAYOUT_PICK_SALT);
  const int pick = dos_rng_range(&rng, 0, k_colony_override_count - 1);
  return &k_colony_overrides[pick];
}

/* Shared by the draw path and both hit-test call sites so clicks always
 * match what's drawn (mirrors colony_screen_assign_slot_positions). */
static void colony_screen_docks_fence_anchor(
  const ColonizeColony* colony,
  int fence_w,
  int fence_h,
  int* fence_x,
  int* fence_y,
  int* coast_x,
  int* coast_y
) {
  *fence_x = COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - fence_w;
  *fence_y = COLONY_VIEWPORT_Y + COLONY_VIEWPORT_H - fence_h;
  *coast_x = COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - COLONY_COAST_W;
  *coast_y = *fence_y - COLONY_COAST_H;
  const ColonyPlacementOverride* ovr = colony_screen_find_override(colony);
  if (ovr) {
    if (ovr->fence_x != COLONY_OVERRIDE_NONE) {
      *fence_x = ovr->fence_x;
      *fence_y = ovr->fence_y;
    }
    if (ovr->docks_x != COLONY_OVERRIDE_NONE) {
      *coast_x = ovr->docks_x;
      *coast_y = ovr->docks_y;
    }
  }
}

static void colony_screen_draw_area_overlays(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  const ColonizeFont* font,
  bool debug_rects,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !colony || !map || !framebuffer) {
    return;
  }
  int origin_x = 0;
  int origin_y = 0;
  colony_screen_minimap_origin(&origin_x, &origin_y);
  const int half = COLONY_MINIMAP_GRID / 2;
  const int tile = COLONY_MINIMAP_TILE;

  /* Field-yield SoL bonus/latch bits — same inputs colony_preview.c folds in
   * (turn_produce_one_colony's real formula), needed so these on-map badges
   * agree with the golden and with the Production multifunction pane
   * instead of showing the unboosted base rate. */
  const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);

  /* Center settlement icon + auto-yield rows (Note 1), on the center tile. */
  {
    const int tile_x = origin_x + half * tile;
    const int tile_y = origin_y + half * tile;
    const int icon = colonies_settlement_icon(pool, colony);
    if (view->icons_ok && icon >= 0 && icon < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[icon];
      const int px = tile_x + (tile - sp->width) / 2;
      const int py = tile_y + (tile - sp->height) / 2;
      ss_blit_sprite(&view->icons, icon, framebuffer, px, py);
    }
    ColonizeTownCommonsYield tc;
    colony_yield_town_commons(map, colony->x, colony->y, sol_b_field, colony->colony_flags, &tc);
    int row = 0;
    if (tc.food > 0) {
      colony_screen_draw_resource_count(
        view,
        font,
        framebuffer,
        tile_x,
        tile_y + row * 10,
        tile,
        10,
        COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_FOOD,
        tc.food,
        15,
        false
      );
      row++;
    }
    if (tc.secondary_amount > 0 && tc.secondary_cargo >= 0) {
      colony_screen_draw_resource_count(
        view,
        font,
        framebuffer,
        tile_x,
        tile_y + row * 10,
        tile,
        10,
        COLONY_CARGO_ICON_BASE + tc.secondary_cargo,
        tc.secondary_amount,
        15,
        false
      );
    }
  }

  /* Docks (or an upgrade: Drydock/Shipyard) gates Fisherman yield to 0 —
   * FUN_15eb_18ec ~11925-11939. Must match turn.c's check. */
  bool has_docks = false;
  if (pool) {
    for (int bi = 0; bi < pool->building_type_count && bi < COLONIZE_BUILDING_TYPES_MAX; ++bi) {
      if (!colony->has_building[bi]) {
        continue;
      }
      const char* dn = pool->building_types[bi].name;
      if (dn && (strstr(dn, "Docks") != NULL || strstr(dn, "Drydock") != NULL ||
                 strstr(dn, "Shipyard") != NULL)) {
        has_docks = true;
        break;
      }
    }
  }

  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    const int who = (int)colony->tiles[ti];
    if (who < 0 || who >= colony->colonist_count) {
      continue;
    }
    const ColonizeColonist* c = &colony->colonists[who];
    if (!c->active || c->field_job < 0) {
      continue;
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tile_x = origin_x + (dx + half) * tile;
    const int tile_y = origin_y + (dy + half) * tile;
    const int cargo = colony_yield_job_cargo(c->field_job);
    int yld = colony_yield_for_worker(
      map,
      colony->x + dx,
      colony->y + dy,
      c->field_job,
      c->profession,
      has_docks,
      sol_b_field,
      colony->colony_flags
    );
    /* Henry Hudson: fur trapper output +100% — matches turn.c/colony_preview.c
     * (2026-08-15: badges previously missed this, a known gap — see
     * building_production.md "UI: settlement badges vs Production tab"). */
    if (yld > 0 && c->field_job == COLONIZE_JOB_FUR_TRAPPER && col1 &&
        founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON)) {
      yld *= 2;
    }
    if (cargo >= 0 && yld > 0) {
      const int icon = (c->field_job == COLONIZE_JOB_FISHERMAN)
                         ? COLONY_ICON_FISH
                         : (COLONY_CARGO_ICON_BASE + cargo);
      colony_screen_draw_resource_count(
        view,
        font,
        framebuffer,
        tile_x,
        tile_y,
        tile,
        12,
        icon,
        yld,
        15,
        false
      );
    }
    if (units) {
      const int sprite =
        units_working_colonist_sprite(units, c->unit_type_index, c->profession);
      if (sprite >= 0) {
        const ColonizeSprite* sp =
          (view->icons_ok && sprite < view->icons.sprite_count) ? &view->icons.sprites[sprite]
                                                               : NULL;
        const int iw = sp ? sp->width : 12;
        const int ih = sp ? sp->height : 12;
        const int ix = tile_x + (tile - iw) / 2;
        const int iy = tile_y + tile - ih - 1;
        colony_screen_blit_icon_shadowed(view, sprite, framebuffer, ix, iy);
        if (view->selected_colonist == who) {
          colony_screen_draw_selection_box(framebuffer, tile_x, tile_y, tile, tile, 10);
        }
      }
    }
  }
}

static void colony_screen_render_minimap(
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  int colony_x,
  int colony_y,
  ColonizeFramebuffer8* framebuffer
) {
  if (!map || !terrain || !framebuffer) {
    return;
  }

  int origin_x = 0;
  int origin_y = 0;
  colony_screen_minimap_origin(&origin_x, &origin_y);
  const int half = COLONY_MINIMAP_GRID / 2;
  const int tile = COLONY_MINIMAP_TILE;

  for (int dy = -half; dy <= half; ++dy) {
    for (int dx = -half; dx <= half; ++dx) {
      const int mx = colony_x + dx;
      const int my = colony_y + dy;
      const int tile_x = origin_x + (dx + half) * tile;
      const int tile_y = origin_y + (dy + half) * tile;
      const int underlayer = map_coast_underlayer_sprite_at(map, mx, my);
      const int coast_layers = map_phys0_coast_layer_count(map, mx, my);
      const int sprite = (underlayer >= 0) ? underlayer : map_terrain_sprite_at(map, mx, my);
      if (sprite >= 0 && sprite < terrain->sprite_count) {
        colony_screen_blit_scaled_15(terrain, sprite, framebuffer, tile_x, tile_y);
      }
      if (!phys0) {
        continue;
      }
      if (underlayer < 0) {
        const int transitions = map_land_transition_count(map, mx, my);
        for (int ti = 0; ti < transitions; ++ti) {
          const int mask = map_land_transition_mask_sprite_at(map, mx, my, ti);
          const int fill = map_land_transition_fill_terrain_at(map, mx, my, ti);
          if (mask >= 0 && mask < phys0->sprite_count) {
            colony_screen_blit_scaled_15(phys0, mask, framebuffer, tile_x, tile_y);
          }
          if (fill >= 0 && fill < terrain->sprite_count) {
            colony_screen_blit_scaled_15_where_dest(terrain, fill, framebuffer, tile_x, tile_y, 0);
          }
        }
      }
      const int forest = map_phys0_forest_sprite_at(map, mx, my);
      if (forest >= 0 && forest < phys0->sprite_count) {
        colony_screen_blit_scaled_15(phys0, forest, framebuffer, tile_x, tile_y);
      }
      const int layers = map_phys0_overlay_count(map, mx, my);
      const int coast_end = (underlayer >= 0) ? coast_layers : layers;
      for (int layer = 0; layer < coast_end; ++layer) {
        const int overlay = map_phys0_overlay_sprite_at(map, mx, my, layer);
        if (overlay < 0 || overlay >= phys0->sprite_count) {
          continue;
        }
        int ox = 0;
        int oy = 0;
        map_phys0_overlay_offset_at(map, mx, my, layer, &ox, &oy);
        colony_screen_blit_scaled_15(
          phys0, overlay, framebuffer, tile_x + (ox * 3) / 2, tile_y + (oy * 3) / 2
        );
      }
      if (underlayer >= 0) {
        const int ocean_sprite = map_terrain_sprite_at(map, mx, my);
        if (ocean_sprite >= 0 && ocean_sprite < terrain->sprite_count) {
          colony_screen_blit_scaled_15_where_dest(
            terrain, ocean_sprite, framebuffer, tile_x, tile_y, 0
          );
        }
        for (int layer = coast_layers; layer < layers; ++layer) {
          const int overlay = map_phys0_overlay_sprite_at(map, mx, my, layer);
          if (overlay < 0 || overlay >= phys0->sprite_count) {
            continue;
          }
          int ox = 0;
          int oy = 0;
          map_phys0_overlay_offset_at(map, mx, my, layer, &ox, &oy);
          colony_screen_blit_scaled_15(
            phys0, overlay, framebuffer, tile_x + (ox * 3) / 2, tile_y + (oy * 3) / 2
          );
        }
      }
      /* Runtime plow / road overlays after static MAPEDIT layers. */
      {
        const int plow = map_phys0_plow_sprite_at(map, mx, my);
        if (plow >= 0 && plow < phys0->sprite_count) {
          colony_screen_blit_scaled_15(phys0, plow, framebuffer, tile_x, tile_y);
        }
        const int road_n = map_phys0_road_layer_count(map, mx, my);
        for (int ri = 0; ri < road_n; ++ri) {
          const int road = map_phys0_road_layer_sprite_at(map, mx, my, ri);
          if (road >= 0 && road < phys0->sprite_count) {
            colony_screen_blit_scaled_15(phys0, road, framebuffer, tile_x, tile_y);
          }
        }
      }
    }
  }

  /* Black 1px frame around the whole 3x3 tile grid — golden-measured
   * (new_amsterdam_production.png: a 73x73 native square, exactly the
   * grid's own COLONY_MINIMAP_GRID*COLONY_MINIMAP_TILE bounding box) —
   * was missing entirely. Drawn after the tiles so it sits on the grid's
   * edge; the cursor's green selection box and unit/badge overlays are
   * drawn after this call (colony_screen_draw_area_overlays) and correctly
   * layer on top. */
  colony_screen_draw_selection_box(
    framebuffer, origin_x, origin_y, COLONY_MINIMAP_GRID * tile, COLONY_MINIMAP_GRID * tile, 0
  );
}


typedef struct ColonyBuildingSlot {
  const char* const* chain;
  int tree_sprite; /* also selects this slot's size-class pool — see colony_screen_slot_group */
} ColonyBuildingSlot;

typedef struct ColonyPoint {
  int x;
  int y;
} ColonyPoint;

static const char* k_slot_town_hall[] = {"Town Hall", NULL};
static const char* k_slot_church[] = {"Church", "Cathedral", NULL};
static const char* k_slot_school[] = {"Schoolhouse", "College", "University", NULL};
static const char* k_slot_carpenter[] = {"Carpenter's Shop", "Lumber Mill", NULL};
static const char* k_slot_blacksmith[] = {"Blacksmith's House", "Blacksmith's Shop", "Iron Works", NULL};
static const char* k_slot_weaver[] = {"Weaver's House", "Weaver's Shop", "Textile Mill", NULL};
static const char* k_slot_tobacco[] = {"Tobacconist's House", "Tobacconist's Shop", "Cigar Factory", NULL};
static const char* k_slot_rum[] = {"Rum Distiller's House", "Rum Distillery", "Rum Factory", NULL};
static const char* k_slot_fur[] = {"Fur Trader's House", "Fur Trading Post", "Fur Factory", NULL};
static const char* k_slot_warehouse[] = {"Warehouse", NULL};
static const char* k_slot_armory[] = {"Armory", "Magazine", "Arsenal", NULL};
static const char* k_slot_press[] = {"Printing Press", "Newspaper", NULL};
static const char* k_slot_stable[] = {"Stable", NULL};
static const char* k_slot_custom[] = {"Custom House", NULL};
static const char* k_slot_stockade[] = {"Stockade", "Fort", "Fortress", NULL};
static const char* k_slot_docks[] = {"Docks", "Drydock", "Shipyard", NULL};

/*
 * Real DOS placement (FUN_2f2b_0434, original_sources_decompiled/
 * viceroy_unpacked.c:47259) is genuinely pseudorandom: each building
 * category is grouped into one of 5 size classes with its own fixed pool
 * of candidate screen slots, and gets rejection-sample-assigned (DOS RNG,
 * reseeded per-colony from `(colony.y<<8)|colony.x` — FUN_15eb_1476 — plus
 * a second term this port can't recover, so not reproducible from a save
 * file) to one free slot in its class's pool. Extracting DOS's own actual
 * slot-pool tables and RNG stream turned out to be a dead end this session
 * — the decompile's `FUN_SSSS_OOOO` tags are compile-time overlay labels,
 * not real runtime segments (confirmed live: breakpoints on the literal
 * address, and on the standard DOS overlay-load interrupt, both never
 * fired — this game's overlay manager isn't the standard one), and static
 * file extraction landed on unrelated code bytes (the file-offset formula
 * that works for the map/terrain tables in `docs/viceroy_tables.md` is
 * anchored to a *different* segment's data, confirmed by decoding straight
 * into an `int 21h` opcode).
 *
 * So this doesn't reproduce DOS's actual output — it reproduces DOS's
 * *algorithm shape*: same size-class grouping, same reject-sampled random
 * assignment from a fixed per-class pool, same per-colony-stable-but-
 * cross-colony-different result (seeded from colony x,y, matching the
 * real mechanism, XORed with an arbitrary salt since the real second seed
 * term isn't recoverable). The pools themselves are this port's own
 * invention — sized and spaced to reuse the good real estate found by
 * template-matching New Amsterdam's golden (see git history) — not
 * extracted DOS data. `colony_screen_assign_slot_positions` does the
 * actual assignment; `k_group_*_slots` below are the three pools.
 * `k_building_slots[]` only records each category's upgrade chain and
 * which pool it draws from now (via `tree_sprite`'s existing LARGE/MED/
 * SMALL enum). See docs/colony_screen.md.
 */
static const ColonyBuildingSlot k_building_slots[] = {
  {k_slot_town_hall, COLONY_TREE_LARGE},
  {k_slot_church, COLONY_TREE_LARGE},
  {k_slot_school, COLONY_TREE_MED},
  {k_slot_carpenter, COLONY_TREE_MED},
  {k_slot_blacksmith, COLONY_TREE_SMALL},
  {k_slot_weaver, COLONY_TREE_SMALL},
  {k_slot_tobacco, COLONY_TREE_SMALL},
  {k_slot_rum, COLONY_TREE_SMALL},
  {k_slot_fur, COLONY_TREE_SMALL},
  {k_slot_warehouse, COLONY_TREE_MED},
  {k_slot_armory, COLONY_TREE_MED},
  {k_slot_press, COLONY_TREE_SMALL},
  {k_slot_stable, COLONY_TREE_SMALL},
  {k_slot_custom, COLONY_TREE_SMALL},
};
static const int k_building_slot_count =
  (int)(sizeof(k_building_slots) / sizeof(k_building_slots[0]));

/* The three size-class pools — screen real estate only, not DOS data (see
 * comment above). Count must be >= the number of k_building_slots entries
 * using that class, so every category always finds a free slot. */
static const ColonyPoint k_group_large_slots[] = {{65, 79}, {86, 3}};
static const ColonyPoint k_group_med_slots[] = {{14, 94}, {127, 10}, {9, 68}, {5, 6}};
static const ColonyPoint k_group_small_slots[] = {
  /* {110,20} (this session's earlier replacement for the reserved-corner-
   * violating {173,45}) sat squarely under the LARGE pool's {86,3} slot —
   * whichever category ends up there (usually church/town_hall's forced
   * "only slot left" pick) would draw its tree canopy through the real
   * building's roof. {60,27} clears both LARGE points and 6 of the other
   * 7 SMALL points outright; the one exception ({36,37}, fur) only nicks
   * a 2×18px corner — real building sprites have enough transparent
   * margin inside their bounding box that this doesn't show. See
   * colony_screen_slot_overlaps_placed() for the general cross-group
   * guard this pairs with. */
  {4, 33}, {55, 5}, {172, 10}, {144, 7}, {36, 37}, {95, 45}, {60, 27}, {66, 46}
};

enum { COLONY_GROUP_LARGE = 0, COLONY_GROUP_MED = 1, COLONY_GROUP_SMALL = 2, COLONY_GROUP_COUNT = 3 };

static int colony_screen_slot_group(int tree_sprite) {
  if (tree_sprite == COLONY_TREE_LARGE) {
    return COLONY_GROUP_LARGE;
  }
  if (tree_sprite == COLONY_TREE_MED) {
    return COLONY_GROUP_MED;
  }
  return COLONY_GROUP_SMALL;
}

/* Docks/Drydock/Shipyard (k_slot_docks) and the Stockade/Fort/Fortress fence
 * aren't part of the random pool at all — they're drawn separately at a
 * fixed slot hugging the settlement's bottom-right corner (see coast_x/
 * coast_y and fence_x/fence_y in colony_screen_draw_area_overlays), same
 * corner every colony, because that's where the water/shore tile the docks
 * sit on actually is. Player-verified real-DOS anchor: drydock's sprite
 * center sits around native (188,66), shipyard's around (142,90) — both
 * inside this reserved box, consistent with one fixed top-left-anchored
 * slot whose visible "center" shifts a bit with each tier's sprite art.
 * The random building pool must never place a category on top of this
 * corner (or its trailing shore/tree art) — reject any candidate whose
 * footprint would overlap it, in *addition* to keeping the pool data
 * itself clear of the box (belt and suspenders: a future pool edit that
 * strays back in here gets skipped instead of silently overlapping). */
#define COLONY_RESERVED_X0 (COLONY_MINIMAP_SECTION_X - COLONY_VIEWPORT_X - COLONY_COAST_W)
#define COLONY_RESERVED_Y0 (COLONY_VIEWPORT_H - COLONY_COAST_H - COLONY_FENCE_H)
#define COLONY_RESERVED_X1 COLONY_VIEWPORT_W
#define COLONY_RESERVED_Y1 COLONY_VIEWPORT_H

/* Real sprite sizes run LARGE ~53x37, MED ~44x22, SMALL ~23x27 (golden-
 * measured); these round up a little so both the reserved-corner check and
 * the cross-slot overlap check below never under-cover a real sprite. */
static void colony_screen_group_footprint_wh(int group, int* w, int* h) {
  if (group == COLONY_GROUP_LARGE) {
    *w = 56;
    *h = 40;
  } else if (group == COLONY_GROUP_MED) {
    *w = 46;
    *h = 24;
  } else {
    *w = 26;
    *h = 28;
  }
}

static bool colony_screen_slot_reserved(int group, int x, int y) {
  int w, h;
  colony_screen_group_footprint_wh(group, &w, &h);
  return x + w > COLONY_RESERVED_X0 && x < COLONY_RESERVED_X1 && y + h > COLONY_RESERVED_Y0 &&
    y < COLONY_RESERVED_Y1;
}

typedef struct ColonyPlacedRect {
  int x, y, w, h;
} ColonyPlacedRect;

static bool colony_rects_overlap(
  int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh
) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

/* True if a candidate (group-sized) slot at x,y would overlap any
 * already-placed building — same colony, other groups included. Building
 * placement used to only guard against same-group duplicates and the
 * docks/fence corner; nothing stopped, say, an unbuilt SMALL tree from
 * landing on top of a LARGE building assigned from a different pool
 * (player-caught: a placeholder copse rendered on top of Town Hall / the
 * church in both New Amsterdam and Recife). */
static bool colony_screen_slot_overlaps_placed(
  int group, int x, int y, const ColonyPlacedRect* placed, int placed_count
) {
  int w, h;
  colony_screen_group_footprint_wh(group, &w, &h);
  for (int i = 0; i < placed_count; ++i) {
    if (colony_rects_overlap(x, y, w, h, placed[i].x, placed[i].y, placed[i].w, placed[i].h)) {
      return true;
    }
  }
  return false;
}

/*
 * One group's worth of the rejection-sampling fallback chain: same-group
 * dup + reserved corner + cross-group overlap, then drop the overlap
 * check, then drop reserved too, then accept any free same-group slot —
 * guarantees a pick even if the pool is fully boxed in, while preferring
 * a clean one. Shared by the main per-category loop and the HIDDEN-but-
 * actually-built fallback below (a category a golden layout's table marks
 * unbuildable-here can still be genuinely built in some *other* colony
 * reusing that layout — see colony_screen_find_override()). */
static int colony_screen_pick_pool_slot(
  int group,
  const ColonyPoint* const pools[COLONY_GROUP_COUNT],
  const int pool_counts[COLONY_GROUP_COUNT],
  bool taken[COLONY_GROUP_COUNT][8],
  const ColonyPlacedRect* placed,
  int placed_count,
  ColonizeDosRng* rng
) {
  const int n = pool_counts[group];
  int pick = -1;
  for (int guard = 0; guard < n * 4 && pick < 0; ++guard) {
    const int c = dos_rng_range(rng, 0, n - 1);
    if (!taken[group][c] && !colony_screen_slot_reserved(group, pools[group][c].x, pools[group][c].y) &&
        !colony_screen_slot_overlaps_placed(group, pools[group][c].x, pools[group][c].y, placed, placed_count)) {
      pick = c;
    }
  }
  if (pick < 0) {
    for (int c = 0; c < n; ++c) {
      if (!taken[group][c] && !colony_screen_slot_reserved(group, pools[group][c].x, pools[group][c].y) &&
          !colony_screen_slot_overlaps_placed(group, pools[group][c].x, pools[group][c].y, placed, placed_count)) {
        pick = c;
        break;
      }
    }
  }
  if (pick < 0) {
    for (int c = 0; c < n; ++c) {
      if (!taken[group][c] && !colony_screen_slot_reserved(group, pools[group][c].x, pools[group][c].y)) {
        pick = c;
        break;
      }
    }
  }
  if (pick < 0) {
    for (int c = 0; c < n; ++c) {
      if (!taken[group][c]) {
        pick = c;
        break;
      }
    }
  }
  return pick < 0 ? 0 : pick;
}

/*
 * Fills xs[]/ys[] (each sized k_building_slot_count) with this colony's
 * building positions: same algorithm shape as DOS (see block comment
 * above) — reseed a DOS-LCG RNG from this colony's fixed map (x,y), then
 * for each k_building_slots[] entry in order, rejection-sample a free slot
 * from its size class's pool. Deterministic per colony (stable across
 * calls/reloads, matching the real game's observed behavior), different
 * across colonies, cheap enough to recompute on every call (14 slots) —
 * no caching needed.
 */
static int colony_screen_best_built(
  const ColonizeColonyPool* pool, const ColonizeColony* colony, const char* const* names, size_t name_count
);

static void colony_screen_assign_slot_positions(
  const ColonizeColonyPool* pool, const ColonizeColony* colony, int* xs, int* ys
) {
  const ColonyPoint* pools[COLONY_GROUP_COUNT] = {
    k_group_large_slots, k_group_med_slots, k_group_small_slots
  };
  const int pool_counts[COLONY_GROUP_COUNT] = {
    (int)(sizeof(k_group_large_slots) / sizeof(k_group_large_slots[0])),
    (int)(sizeof(k_group_med_slots) / sizeof(k_group_med_slots[0])),
    (int)(sizeof(k_group_small_slots) / sizeof(k_group_small_slots[0]))
  };
  bool taken[COLONY_GROUP_COUNT][8] = {{false}};
  ColonyPlacedRect placed[14];
  int placed_count = 0;

  /* A colony-specific override (see below) already fixes some of these
   * slots to an exact golden pixel — almost always one of the pool points
   * above (DOS reuses the same real estate, just assigns it differently).
   * Mark those pool points taken, and record their footprint in `placed[]`,
   * *before* the RNG runs, so an unbuilt category's tree placeholder never
   * lands on top of an overridden neighbor's real building — same-group
   * duplicate (`taken`) or cross-group overlap (`placed`, player-caught:
   * a placeholder copse rendered on top of Town Hall / the church in both
   * New Amsterdam and Recife). Overridden indices themselves are skipped
   * below and filled in from `ovr->pos[]` afterward. */
  const ColonyPlacementOverride* ovr = colony_screen_find_override(colony);
  if (ovr) {
    for (int i = 0; i < k_building_slot_count && i < 14; ++i) {
      if (ovr->pos[i][0] == COLONY_OVERRIDE_NONE || ovr->pos[i][0] == COLONY_OVERRIDE_HIDDEN) {
        continue; /* HIDDEN doesn't occupy pool real estate — nothing to mark/reserve */
      }
      const int group = colony_screen_slot_group(k_building_slots[i].tree_sprite);
      const int n = pool_counts[group];
      for (int c = 0; c < n; ++c) {
        if (pools[group][c].x == ovr->pos[i][0] && pools[group][c].y == ovr->pos[i][1]) {
          taken[group][c] = true;
          break;
        }
      }
      int w, h;
      colony_screen_group_footprint_wh(group, &w, &h);
      placed[placed_count++] = (ColonyPlacedRect){ovr->pos[i][0], ovr->pos[i][1], w, h};
    }
  }

  ColonizeDosRng rng;
  const uint32_t xy = colony ? (((uint32_t)colony->y << 8) | (uint32_t)colony->x) : 0;
  /* 0x434 salts this port's synthetic roll away from DOS's own (unrecoverable
   * second seed term) reseed value — arbitrary, just needs to be fixed. */
  dos_rng_seed(&rng, xy ^ 0x434u);

  for (int i = 0; i < k_building_slot_count; ++i) {
    if (ovr && ovr->pos[i][0] != COLONY_OVERRIDE_NONE) {
      continue; /* filled in from ovr->pos[] below */
    }
    const int group = colony_screen_slot_group(k_building_slots[i].tree_sprite);
    const int pick =
      colony_screen_pick_pool_slot(group, pools, pool_counts, taken, placed, placed_count, &rng);
    taken[group][pick] = true;
    xs[i] = pools[group][pick].x;
    ys[i] = pools[group][pick].y;
    if (placed_count < 14) {
      int w, h;
      colony_screen_group_footprint_wh(group, &w, &h);
      placed[placed_count++] = (ColonyPlacedRect){xs[i], ys[i], w, h};
    }
  }

  if (ovr) {
    for (int i = 0; i < k_building_slot_count && i < 14; ++i) {
      if (ovr->pos[i][0] == COLONY_OVERRIDE_HIDDEN) {
        /* HIDDEN means "this table has no room for the tree filler" — it
         * must never swallow a real building. If some *other* colony
         * reusing this layout actually has the category built, give it a
         * genuine slot via the same fallback the general algorithm uses. */
        size_t n = 0;
        while (k_building_slots[i].chain && k_building_slots[i].chain[n]) {
          ++n;
        }
        const int built = pool ? colony_screen_best_built(pool, colony, k_building_slots[i].chain, n) : -1;
        if (built < 0) {
          xs[i] = COLONY_SLOT_HIDDEN;
          ys[i] = COLONY_SLOT_HIDDEN;
        } else {
          const int group = colony_screen_slot_group(k_building_slots[i].tree_sprite);
          const int pick =
            colony_screen_pick_pool_slot(group, pools, pool_counts, taken, placed, placed_count, &rng);
          taken[group][pick] = true;
          xs[i] = pools[group][pick].x;
          ys[i] = pools[group][pick].y;
          if (placed_count < 14) {
            int w, h;
            colony_screen_group_footprint_wh(group, &w, &h);
            placed[placed_count++] = (ColonyPlacedRect){xs[i], ys[i], w, h};
          }
        }
      } else if (ovr->pos[i][0] != COLONY_OVERRIDE_NONE) {
        xs[i] = ovr->pos[i][0];
        ys[i] = ovr->pos[i][1];
      }
    }
  }
}

static int colony_screen_find_built(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* name
) {
  if (!pool || !colony || !name) {
    return -1;
  }
  const int idx = colonies_find_building(pool, name);
  if (idx < 0 || !colony->has_building[idx]) {
    return -1;
  }
  return idx;
}

/* Highest present building in an upgrade chain (names ordered low → high). */
static int colony_screen_best_built(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* const* names,
  size_t name_count
) {
  int best = -1;
  for (size_t i = 0; i < name_count; ++i) {
    const int idx = colony_screen_find_built(pool, colony, names[i]);
    if (idx >= 0) {
      best = idx;
    }
  }
  return best;
}

static void colony_screen_blit_slot(
  const ColonyScreenView* view,
  int sprite_index,
  int x,
  int y,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !framebuffer || sprite_index < 0 || sprite_index >= view->buildings.sprite_count) {
    return;
  }
  const ColonizeSprite* spr = &view->buildings.sprites[sprite_index];
  if (!spr || !spr->pixels || spr->width <= 2 || spr->height <= 2) {
    return;
  }
  ss_blit_sprite(&view->buildings, sprite_index, framebuffer, x, y);
}

static int colony_screen_outside_display_sprite(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
);

/* Icon metrics for an outside (on-tile) unit; defaults if sheet/sprite missing. */
static void colony_screen_outside_icon_metrics(
  const ColonyScreenView* view,
  const ColonizeUnitPool* units,
  int unit_id,
  int* out_w,
  int* out_h
) {
  int w = 12;
  int h = 16;
  if (units && view && view->icons_ok) {
    const ColonizeUnit* u = units_get_const(units, unit_id);
    const int sprite = colony_screen_outside_display_sprite(units, u);
    if (sprite >= 0 && sprite < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[sprite];
      if (sp && sp->width > 0 && sp->height > 0) {
        w = sp->width;
        h = sp->height;
      }
    }
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
}

static int colony_screen_outside_display_sprite(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
) {
  if (!units || !u) {
    return -1;
  }
  const ColonizeUnitType* type = units_type(units, u->type_index);
  int sprite = units_map_sprite(units, u->id);
  if (type && strstr(type->name, "Colonist") != NULL &&
      u->muskets <= 0 && u->horses <= 0 && u->tools <= 0) {
    sprite = units_working_colonist_sprite(units, u->type_index, u->profession);
  }
  return sprite;
}

/* True for Artillery (and other non-colonist land ordnance) — player-caught:
 * the fortification (Stockade/Fort/Fortress) strip drawn on the fence
 * corner is a row of walking colonist figures in DOS, never an artillery
 * piece; Artillery still belongs on-tile (outside_unit_ids) for the Units-
 * Present / Military tab (colony_screen_multi_units_layout, which wants it
 * deliberately), just not on this one strip. */
static bool colony_screen_unit_is_artillery(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return false;
  }
  const ColonizeUnitType* type = units_type(units, u->type_index);
  return type && strstr(type->name, "Artillery") != NULL;
}

int colony_screen_multi_units_layout(
  const ColonyScreenView* view,
  const ColonizeUnitPool* units,
  int px,
  int py,
  int pane_w,
  int pane_h,
  ColonyMultiUnitSlot* out,
  int max
) {
  if (!view || !units || !out || max <= 0 || pane_w <= 0 || pane_h <= 0) {
    return 0;
  }
  /* Land units only (colonist-class + Artillery); ships/wagons stay on the
   * Transport strip. outside_unit_ids already excludes units_is_transport. */
  int ids[COLONY_MULTI_UNITS_SLOT_MAX];
  int n = 0;
  for (int i = 0; i < view->outside_unit_count && n < COLONY_MULTI_UNITS_SLOT_MAX; ++i) {
    ids[n++] = view->outside_unit_ids[i];
  }

  const int row_h = 16;
  int x = px;
  int y = py;
  int count = 0;
  for (int i = 0; i < n && count < max; ++i) {
    const ColonizeUnit* u = units_get_const(units, ids[i]);
    const int sprite = u ? colony_screen_outside_display_sprite(units, u) : -1;
    if (sprite < 0) {
      continue;
    }
    const ColonizeSprite* sp =
      (view->icons_ok && sprite < view->icons.sprite_count) ? &view->icons.sprites[sprite] : NULL;
    const int iw = (sp && sp->width > 0) ? sp->width : 12;
    const int slot_w = iw + UNIT_CHROME_SPRITE_DX + 2;
    if (x + slot_w > px + pane_w && x > px) {
      x = px;
      y += row_h;
    }
    if (y + row_h > py + pane_h) {
      break;
    }
    out[count].unit_id = ids[i];
    out[count].x = x;
    out[count].y = y;
    out[count].w = slot_w;
    out[count].h = row_h;
    count++;
    x += slot_w;
  }
  return count;
}

static int colony_screen_building_production_badge(
  const ColonizeColonyPool* pool,
  int built
) {
  if (!pool || built < 0 || built >= pool->building_type_count) {
    return -1;
  }
  const char* name = pool->building_types[built].name;
  if (!name) {
    return -1;
  }
  /* Printing Press / Newspaper are colony-wide bell *multipliers* — a
   * separate building slot (k_slot_press) nobody can ever be assigned to
   * work (no @JOB for them). Player-caught: matching them here drew a bell
   * badge on the Press/Newspaper sprite itself, duplicating Town Hall's own
   * badge — only Town Hall has a worker slot and a bell count to show. */
  if (strstr(name, "Town Hall")) {
    return COLONY_ICON_BELL;
  }
  if (strstr(name, "Church") || strstr(name, "Cathedral")) {
    return COLONY_ICON_CROSS;
  }
  if (strstr(name, "Carpenter") || strstr(name, "Lumber Mill")) {
    return COLONY_ICON_HAMMER;
  }
  if (strstr(name, "Rum")) {
    return COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_RUM;
  }
  if (strstr(name, "Tobacconist")) {
    return COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_CIGARS;
  }
  if (strstr(name, "Weaver") || strstr(name, "Textile")) {
    return COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_CLOTH;
  }
  if (strstr(name, "Fur")) {
    return COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_COATS;
  }
  if (strstr(name, "Blacksmith") || strstr(name, "Iron")) {
    return COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_TOOLS;
  }
  if (strstr(name, "Armory") || strstr(name, "Magazine") || strstr(name, "Arsenal")) {
    return COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_MUSKETS;
  }
  return -1;
}

/* DEBUG menu "Building Rects": violet (EGA bright magenta, WOODPANL.PIK
 * idx 13 — not used anywhere else on this screen) outline around a
 * building sprite's actual bounds, so placement can be tweaked by eye. */
#define COLONY_DEBUG_RECT_COLOR 13

static void colony_screen_debug_building_rect(
  const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer, int sprite, int x, int y
) {
  if (!view || sprite < 0 || sprite >= view->buildings.sprite_count) {
    return;
  }
  const ColonizeSprite* spr = &view->buildings.sprites[sprite];
  if (!spr || spr->width <= 0 || spr->height <= 0) {
    return;
  }
  colony_screen_draw_selection_box(framebuffer, x, y, spr->width, spr->height, COLONY_DEBUG_RECT_COLOR);
}

static void colony_screen_blit_buildings(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  bool debug_rects,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->buildings_ok || !pool || !colony || !framebuffer) {
    return;
  }

  const int slot_ox = COLONY_VIEWPORT_X;
  const int slot_oy = COLONY_VIEWPORT_Y;
  int slot_x[32];
  int slot_y[32];
  colony_screen_assign_slot_positions(pool, colony, slot_x, slot_y);
  for (int i = 0; i < k_building_slot_count; ++i) {
    const ColonyBuildingSlot* slot = &k_building_slots[i];
    size_t n = 0;
    while (slot->chain && slot->chain[n]) {
      ++n;
    }
    const int built = colony_screen_best_built(pool, colony, slot->chain, n);
    if (built < 0 && slot_x[i] == COLONY_SLOT_HIDDEN) {
      continue; /* per-colony override: no pool slot fits this placeholder cleanly — skip it */
    }
    const int drawn_sprite = built >= 0 ? built : slot->tree_sprite;
    if (built >= 0) {
      colony_screen_blit_slot(view, built, slot_ox + slot_x[i], slot_oy + slot_y[i], framebuffer);
    } else {
      colony_screen_blit_slot(
        view, slot->tree_sprite, slot_ox + slot_x[i], slot_oy + slot_y[i], framebuffer
      );
    }
    if (debug_rects) {
      colony_screen_debug_building_rect(
        view, framebuffer, drawn_sprite, slot_ox + slot_x[i], slot_oy + slot_y[i]
      );
    }

    /* Workers in this building (up to 3): Note 1 strip, bottom-center of sprite. */
    if (built < 0 || !units) {
      continue;
    }
    int worker_ci[COLONY_BUILDING_WORKERS_MAX];
    int worker_icons[COLONY_BUILDING_WORKERS_MAX];
    int workers = 0;
    for (int ci = 0; ci < colony->colonist_count && workers < COLONY_BUILDING_WORKERS_MAX; ++ci) {
      const ColonizeColonist* c = &colony->colonists[ci];
      if (!c->active || c->building_type != built) {
        continue;
      }
      const int sprite =
        units_working_colonist_sprite(units, c->unit_type_index, c->profession);
      if (sprite < 0) {
        continue;
      }
      worker_ci[workers] = ci;
      worker_icons[workers] = sprite;
      workers++;
    }
    const ColonizeSprite* bspr =
      (built >= 0 && built < view->buildings.sprite_count) ? &view->buildings.sprites[built] : NULL;
    const int bw = (bspr && bspr->width > 2) ? bspr->width : COLONY_BUILDING_SLOT_W;
    const int bh = (bspr && bspr->height > 2) ? bspr->height : COLONY_BUILDING_SLOT_H;
    const int bx = slot_ox + slot_x[i];
    const int by = slot_oy + slot_y[i];
    int strip_h = 16;
    for (int wi = 0; wi < workers; ++wi) {
      if (worker_icons[wi] >= 0 && worker_icons[wi] < view->icons.sprite_count) {
        const int ih = view->icons.sprites[worker_icons[wi]].height;
        if (ih > strip_h) {
          strip_h = ih;
        }
      }
    }
    const int strip_y = by + bh - strip_h;
    if (workers > 0) {
      int selected = -1;
      for (int wi = 0; wi < workers; ++wi) {
        if (view->selected_colonist == worker_ci[wi]) {
          selected = wi;
          break;
        }
      }
      colony_screen_draw_icon_strip(
        view,
        NULL,
        framebuffer,
        bx,
        strip_y,
        bw,
        strip_h,
        worker_icons,
        workers,
        selected,
        15,
        false
      );
    }
    /*
     * Production strip: worker output + Town Hall / Church / Cathedral free
     * bells/crosses (still shown when the building is empty).
     */
    if (view->icons_ok) {
      const int badge = colony_screen_building_production_badge(pool, built);
      if (badge >= 0) {
        /* golden-confirmed (New Amsterdam): every building badge here shows
         * the colony's real per-tick total for that resource, not this one
         * function's own local (sol_bonus=0, and — for bells/crosses — pre-
         * FF/AI-subsidy) estimate: Blacksmith's "24" is the Production
         * tab's craft_capacity[TOOLS], and Church's "19"/Town Hall's "82" are
         * exactly the People band's crosses/bells (colony_prod_building_
         * display_output's own calc gave 7/13, undercounting both — see
         * colony_prod_colony_crosses_ff/_bells_ff's FF+AI-subsidy folding,
         * building_production.md). Reuse view->preview throughout rather
         * than a second, drifting local calc.
         *
         * craft_capacity (not craft_gross): a manufacturing badge shows the
         * staffed worker's maximum *potential* output, not this tick's
         * stock-clamped actual — player-caught (Weaver's House, dutch-
         * reports.SAV): a shortfall of cotton makes craft_gross[CLOTH] read
         * 5 (what actually got made) where DOS shows 10 (what the worker
         * can make, cotton permitting) — the shortfall itself already shows
         * separately on the Production tab. */
        int amount = colony_prod_building_display_output(pool, colony, built);
        if (badge >= COLONY_CARGO_ICON_BASE && badge < COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_COUNT &&
            view->preview_valid) {
          const int cargo = badge - COLONY_CARGO_ICON_BASE;
          if (view->preview.craft_capacity[cargo] > 0) {
            amount = view->preview.craft_capacity[cargo];
          }
        } else if (view->preview_valid) {
          if (badge == COLONY_ICON_BELL && view->preview.bells > 0) {
            amount = view->preview.bells;
          } else if (badge == COLONY_ICON_CROSS && view->preview.crosses > 0) {
            amount = view->preview.crosses;
          } else if (badge == COLONY_ICON_HAMMER && view->preview.hammers > 0) {
            amount = view->preview.hammers;
          }
        }
        if (amount > 0) {
          colony_screen_draw_resource_count(
            view,
            font,
            framebuffer,
            bx,
            strip_y - 12,
            bw,
            10,
            badge,
            amount,
            15,
            false
          );
        }
      }
    }
  }

  const int fort = colony_screen_best_built(pool, colony, k_slot_stockade, 3);
  int fence_w = COLONY_FENCE_W;
  int fence_h = COLONY_FENCE_H;
  {
    const int fort_sprite = fort >= 0 ? fort : COLONY_FENCE_SPRITE;
    if (fort_sprite >= 0 && fort_sprite < view->buildings.sprite_count) {
      const ColonizeSprite* spr = &view->buildings.sprites[fort_sprite];
      if (spr && spr->width > 2 && spr->height > 2) {
        fence_w = spr->width;
        fence_h = spr->height;
      }
    }
  }
  int fence_x, fence_y, coast_x, coast_y;
  colony_screen_docks_fence_anchor(colony, fence_w, fence_h, &fence_x, &fence_y, &coast_x, &coast_y);

  const int docks = colony_screen_best_built(pool, colony, k_slot_docks, 3);
  if (docks >= 0) {
    colony_screen_blit_slot(view, docks, coast_x, coast_y, framebuffer);
    if (debug_rects) {
      colony_screen_debug_building_rect(view, framebuffer, docks, coast_x, coast_y);
    }
  } else {
    /* DOS draws this placeholder in every colony's dock corner, coastal or
     * not — inland colonies just never get to replace it with a real
     * Docks/Drydock/Shipyard (player-caught: this port was gating it on
     * `coastal`, leaving inland colonies with a blank corner instead). */
    colony_screen_blit_slot(view, COLONY_COAST_PLACEHOLDER, coast_x, coast_y, framebuffer);
    if (debug_rects) {
      colony_screen_debug_building_rect(view, framebuffer, COLONY_COAST_PLACEHOLDER, coast_x, coast_y);
    }
  }

  if (fort >= 0) {
    colony_screen_blit_slot(view, fort, fence_x, fence_y, framebuffer);
    if (debug_rects) {
      colony_screen_debug_building_rect(view, framebuffer, fort, fence_x, fence_y);
    }
  } else {
    colony_screen_blit_slot(view, COLONY_FENCE_SPRITE, fence_x, fence_y, framebuffer);
    if (debug_rects) {
      colony_screen_debug_building_rect(view, framebuffer, COLONY_FENCE_SPRITE, fence_x, fence_y);
    }
  }

  /* Outside units: Note 1 strip centered on the fortification. */
  if (units && view->outside_unit_count > 0 && view->icons_ok) {
    int icons[COLONY_OUTSIDE_MAX];
    int n = 0;
    int selected = -1;
    for (int i = 0; i < view->outside_unit_count && n < COLONY_OUTSIDE_MAX; ++i) {
      const ColonizeUnit* u = units_get_const(units, view->outside_unit_ids[i]);
      if (!u || colony_screen_unit_is_artillery(units, u)) {
        continue;
      }
      const int sprite = colony_screen_outside_display_sprite(units, u);
      if (sprite < 0) {
        continue;
      }
      if (view->selected_outside_unit == u->id) {
        selected = n;
      }
      icons[n++] = sprite;
    }
    if (n > 0) {
      colony_screen_draw_icon_strip(
        view,
        NULL,
        framebuffer,
        fence_x,
        fence_y,
        fence_w,
        fence_h,
        icons,
        n,
        selected,
        15,
        false
      );
    }
  }
}

static int colony_screen_text_width(const ColonizeFont* font, const char* text) {
  if (!text) {
    return 0;
  }
  int w = 0;
  for (const char* p = text; *p; ++p) {
    const unsigned char ch = (unsigned char)*p;
    if (font && font->section_data && ch < 128 && font->char_widths[ch] > 0) {
      w += font->char_widths[ch];
    } else {
      w += 6;
    }
  }
  return w;
}


/* Warehouse strip: icon centered in each COLONY.PIK slot, amount below. */
static void colony_screen_draw_cargo_strip(
  const ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!colony || !framebuffer) {
    return;
  }

  /*
   * Digit colors, golden-measured (New Amsterdam, exact RGB match against
   * WOODPANL.PIK's palette, not a nearest-color guess): a 3-digit stock's
   * hundreds digit is always gold, independent of the cargo; the rest of
   * the digits are green when this cargo is currently toggled on in the
   * colony's Custom House per-cargo mask (will be auto-sold this EOT) and
   * navy otherwise — the "per-cargo UI chrome" europe.h's
   * europe_custom_house_autosell comment had PARKed. Player-reported: the
   * port was instead drawing every digit in one flat color (white/green/
   * red keyed off an unrelated this-turn production delta). */
  const uint8_t kHundredsColor = 148;
  for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
    const int slot_x = COLONY_CARGO_SLOT_X0 + i * COLONY_CARGO_PITCH;
    const int sprite = COLONY_CARGO_ICON_BASE + i;
    if (view && view->icons_ok && sprite < view->icons.sprite_count) {
      const ColonizeSprite* spr = &view->icons.sprites[sprite];
      const int icon_x = slot_x + (COLONY_CARGO_SLOT_W - spr->width) / 2;
      /* Player-reported: 1px lower than before. */
      ss_blit_sprite(&view->icons, sprite, framebuffer, icon_x, COLONY_CARGO_STRIP_Y + 1);
    }

    if (font) {
      char amount[16];
      int delta = 0;
      if (view && view->last_delta_valid) {
        delta = view->last_delta.goods[i];
      }
      /* Player-reported: 2px lower than before. */
      const int num_y = COLONY_CARGO_NUM_Y + 2;
      if (delta != 0) {
        /* This-turn production delta suffix — a separate, not golden-
         * verified display mode; left as a single flat color (unaffected
         * by this fix) rather than guessing how it'd interact with the
         * hundreds/Custom-House split above. */
        snprintf(amount, sizeof(amount), "%d%+d", colony->stock[i], delta);
        const int tw = colony_screen_text_width(font, amount);
        const int tx = slot_x + (COLONY_CARGO_SLOT_W - tw) / 2;
        const uint8_t col = delta > 0 ? 10 : 12;
        font_draw_text(font, framebuffer, tx, num_y, amount, col);
        continue;
      }
      snprintf(amount, sizeof(amount), "%d", colony->stock[i]);
      const int tw = colony_screen_text_width(font, amount);
      const int tx = slot_x + (COLONY_CARGO_SLOT_W - tw) / 2;
      const uint8_t base_col =
        europe_custom_house_cargo_enabled(colony->custom_house_bits, i) ? 10 : 61;
      const size_t len = strlen(amount);
      if (len > 2) {
        char hundreds[16];
        const size_t hlen = len - 2;
        memcpy(hundreds, amount, hlen);
        hundreds[hlen] = '\0';
        font_draw_text(font, framebuffer, tx, num_y, hundreds, kHundredsColor);
        const int hw = colony_screen_text_width(font, hundreds);
        font_draw_text(font, framebuffer, tx + hw, num_y, amount + hlen, base_col);
      } else {
        font_draw_text(font, framebuffer, tx, num_y, amount, base_col);
      }
    }
  }
}

static void colony_screen_draw_transports(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !units || !framebuffer) {
    return;
  }
  for (int i = 0; i < view->docked_transport_count; ++i) {
    const ColonizeUnit* u = units_get_const(units, view->docked_transport_ids[i]);
    if (!u) {
      continue;
    }
    const ColonizeUnitType* type = units_type(units, u->type_index);
    const int x = COLONY_TRANSPORT_X + 4 + i * COLONY_TRANSPORT_PITCH;
    const int y = COLONY_TRANSPORT_ICON_Y;
    if (type && type->icon_sprite >= 0 && view->icons_ok) {
      unit_chrome_blit_unit_for_palette(
        framebuffer,
        font,
        &view->icons,
        type->icon_sprite,
        x,
        y,
        units_display_type_index(units, u->id),
        u->nation_id,
        u->orders,
        view->docked_transport_count > 1,
        false,
        (view->frame_ok && view->frame.has_palette) ? &view->frame.palette : NULL
      );
      if (view->transport_unit_id == u->id) {
        colony_screen_draw_chrome_selection(view, framebuffer, type->icon_sprite, x, y);
      }
    }
  }

  if (view->transport_unit_id >= 0 && font) {
    const ColonizeUnit* ship = units_get_const(units, view->transport_unit_id);
    const ColonizeUnitType* type = ship ? units_type(units, ship->type_index) : NULL;
    if (type && type->name[0]) {
      font_draw_text(
        font, framebuffer, COLONY_TRANSPORT_X + 4, COLONY_PANEL_CONTENT_Y, type->name, 15
      );
    }
  }

  const int max_holds = 6;
  int open_holds = 0;
  if (view->transport_unit_id >= 0) {
    const ColonizeUnit* ship = units_get_const(units, view->transport_unit_id);
    if (ship) {
      const int goods_holds = units_goods_hold_count(units, view->transport_unit_id);
      open_holds = goods_holds < 0 ? 0 : (goods_holds > max_holds ? max_holds : goods_holds);
      for (int i = 0; i < open_holds; ++i) {
        /* bugs.md: loaded-cargo goods icon nudged +3px x/+10px y, then
         * player-reported 4px too low after that — net +6px y. */
        const int x = COLONY_HOLD_X + 4 + i * COLONY_HOLD_PITCH + 3;
        const int y = COLONY_HOLD_Y + 6;
        const int amt = ship->hold_goods_amount[i];
        const int gtype = ship->hold_goods_type[i];
        if (amt > 0 && amt < 255 && gtype >= 0 && gtype < COLONIZE_CARGO_COUNT) {
          const bool partial = amt < 100;
          colony_screen_blit_cargo(view, gtype, partial, framebuffer, x, y);
        }
      }
      for (int i = 0; i < ship->cargo_count; ++i) {
        const int slot = open_holds + i;
        if (slot >= max_holds) {
          break;
        }
        const int x = COLONY_HOLD_X + 4 + slot * COLONY_HOLD_PITCH;
        const int y = COLONY_HOLD_Y;
        const ColonizeUnit* pass = units_get_const(units, ship->cargo_ids[i]);
        if (!pass) {
          continue;
        }
        const int sprite = units_map_sprite(units, pass->id);
        if (sprite >= 0 && view->icons_ok) {
          unit_chrome_blit_unit_for_palette(
            framebuffer,
            font,
            &view->icons,
            sprite,
            x,
            y,
            units_display_type_index(units, pass->id),
            pass->nation_id,
            pass->orders,
            false,
            true,
            (view->frame_ok && view->frame.has_palette) ? &view->frame.palette : NULL
          );
        }
      }
    }
  }
  /* Cover unused holds; with no ship selected, all six are covered. */
  int cover_w = COLONY_HOLD_W;
  if (view->icons_ok && COLONY_ICON_EMPTY_HOLD >= 0 && COLONY_ICON_EMPTY_HOLD < view->icons.sprite_count) {
    const ColonizeSprite* cov = &view->icons.sprites[COLONY_ICON_EMPTY_HOLD];
    if (cov && cov->width > 0) {
      cover_w = cov->width;
    }
  }
  const int cover_pitch = cover_w + 2; /* keep exactly 2px between hold covers */
  /* All-closed row sits 4px further right than the partial-cover base. */
  const int cover_x0 =
    COLONY_HOLD_X + 2 + (open_holds == 0 ? 4 : 0) + open_holds * COLONY_HOLD_PITCH;
  for (int i = open_holds; i < max_holds; ++i) {
    const int x = cover_x0 + (i - open_holds) * cover_pitch;
    const int y = COLONY_HOLD_Y + 7;
    colony_screen_blit_icon(view, COLONY_ICON_EMPTY_HOLD, framebuffer, x, y);
  }
}

static int colony_screen_sol_percent(const ColonizeCol1Save* col1, const ColonizeColony* colony) {
  return colony_prod_sol_percent(col1, colony);
}

static void colony_screen_draw_people(
  ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !colony || !framebuffer) {
    return;
  }
  const int sol = colony_screen_sol_percent(col1, colony);
  const int tory = 100 - sol;
  const int pop = colony->colonist_count > 0 ? colony->colonist_count : colony->population;
  const int sol_count = (pop > 0) ? ((pop * sol + 50) / 100) : 0;
  const int tory_count = pop > sol_count ? (pop - sol_count) : 0;
  const int sol_y = COLONY_PANEL_CONTENT_Y + 1;
  colony_screen_blit_icon(view, COLONY_ICON_FLAG, framebuffer, COLONY_PEOPLE_X + 2, sol_y);
  /* Tory flush to the right of the (widened) people band. */
  colony_screen_blit_icon(
    view, COLONY_ICON_CROWN, framebuffer, COLONY_PEOPLE_X + COLONY_PEOPLE_W - 14, sol_y
  );
  if (font) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d%% (%d)", sol, sol_count);
    font_draw_text(font, framebuffer, COLONY_PEOPLE_X + 16, sol_y + 2, buf, 15);
    snprintf(buf, sizeof(buf), "%d%% (%d)", tory, tory_count);
    const int tw = colony_screen_text_width(font, buf);
    font_draw_text(
      font, framebuffer, COLONY_PEOPLE_X + COLONY_PEOPLE_W - 16 - tw, sol_y + 2, buf, 15
    );
  }

  int x = COLONY_PEOPLE_X + 2;
  const int y_people = COLONY_PANEL_CONTENT_Y + 16;
  const int x_limit = COLONY_PEOPLE_X + COLONY_PEOPLE_W - 14;
  int colonists_drawn = 0;
  for (int i = 0; i < colony->colonist_count; ++i) {
    const ColonizeColonist* c = &colony->colonists[i];
    if (!c->active) {
      continue;
    }
    if (x > x_limit) {
      break;
    }
    const int sprite =
      units_working_colonist_sprite(units, c->unit_type_index, c->profession);
    if (sprite >= 0) {
      colony_screen_blit_icon_shadowed(view, sprite, framebuffer, x, y_people);
      if (view->selected_colonist == i) {
        colony_screen_draw_icon_selection(view, framebuffer, sprite, x, y_people);
      }
      const ColonizeSprite* sp =
        (view->icons_ok && sprite < view->icons.sprite_count) ? &view->icons.sprites[sprite]
                                                             : NULL;
      x += (sp ? sp->width : 12) + 2;
      colonists_drawn++;
    }
  }

  /* Fence / on-tile units: same row, separate group to the right of colonists. */
  if (view->outside_unit_count > 0 && x <= x_limit) {
    if (colonists_drawn > 0) {
      x += 6; /* extra gap between colony pop and outside group */
    }
    for (int i = 0; i < view->outside_unit_count; ++i) {
      const ColonizeUnit* u = units_get_const(units, view->outside_unit_ids[i]);
      if (!u) {
        continue;
      }
      if (x > x_limit) {
        break;
      }
      const int sprite = colony_screen_outside_display_sprite(units, u);
      if (sprite >= 0) {
        colony_screen_blit_icon_shadowed(view, sprite, framebuffer, x, y_people);
        if (view->selected_outside_unit == u->id) {
          colony_screen_draw_icon_selection(view, framebuffer, sprite, x, y_people);
        }
        const ColonizeSprite* sp =
          (view->icons_ok && sprite < view->icons.sprite_count) ? &view->icons.sprites[sprite]
                                                               : NULL;
        x += (sp ? sp->width : 12) + 2;
      }
    }
  }

  if (!view->preview_valid) {
    return;
  }
  const ColonizeColonyPreview* p = &view->preview;
  const int meter_y = COLONY_CARGO_STRIP_Y - 16;
  const int meter_h = 12;
  const int band = COLONY_PEOPLE_W - 4;
  const int gap = 4;
  /* golden-confirmed (New Amsterdam: 32, not food_produced's pre-breeding
   * 34): this badge shows the same post-breeding `goods[FOOD]` the
   * Production tab would net to (raw field/town-commons food minus the
   * turn's horse-breeding feed, since that subtraction lands in `goods[]`
   * not a separate field), not the raw pre-breeding `food_produced`. */
  int food_amt = p->goods[COLONIZE_CARGO_FOOD];
  if (p->food_net < 0) {
    /* Show production; shortfall drawn as grey in same strip width. */
    food_amt = p->goods[COLONIZE_CARGO_FOOD] > 0 ? p->goods[COLONIZE_CARGO_FOOD] : (-p->food_net);
  }
  const bool surplus_active = p->food_net > 0;
  /*
   * Columns, in DOS reading order: fish/grain, (new) net food surplus,
   * crosses, bells. Player-reported placement ("between food-produced-and-
   * eaten and crosses") and player-reported sizing (width mostly
   * proportional to each column's amount, not a flat 1/n split, with a
   * floor so a small amount like a 2-surplus still reads). The surplus
   * column only exists when there's a surplus to show — a deficit already
   * shows via the fish/grain pair's own grey shortfall mode. */
  const int slot_count = surplus_active ? 4 : 3;
  long weight[4];
  int wi = 0;
  const int food_weight_idx = wi;
  weight[wi++] = food_amt > 0 ? food_amt : 1;
  int surplus_weight_idx = -1;
  if (surplus_active) {
    surplus_weight_idx = wi;
    weight[wi++] = p->food_net;
  }
  const int cross_weight_idx = wi;
  weight[wi++] = p->crosses > 0 ? p->crosses : 1;
  const int bell_weight_idx = wi;
  weight[wi++] = p->bells > 0 ? p->bells : 1;

  const int min_w = 14; /* floor: smallest column still fits an icon + number */
  const int avail = band - (slot_count - 1) * gap;
  long weight_sum = 0;
  for (int i = 0; i < slot_count; ++i) {
    weight_sum += weight[i];
  }
  int width[4];
  int width_sum = 0;
  for (int i = 0; i < slot_count; ++i) {
    width[i] = weight_sum > 0 ? (int)((long)avail * weight[i] / weight_sum) : avail / slot_count;
    if (width[i] < min_w) {
      width[i] = min_w;
    }
    width_sum += width[i];
  }
  /* Rounding remainder (positive or negative, from the min-width floor)
   * goes to the heaviest column — bells, golden-confirmed as the largest
   * New Amsterdam value, so it's the safest place to absorb slack. */
  const int diff = avail - width_sum;
  if (diff != 0) {
    width[bell_weight_idx] += diff;
    if (width[bell_weight_idx] < min_w) {
      width[bell_weight_idx] = min_w;
    }
  }

  int meter_x = COLONY_PEOPLE_X + 2;
  {
    const bool grey_only = (p->food_net < 0 && p->goods[COLONIZE_CARGO_FOOD] <= 0);
    const int fish_amt =
      (!grey_only && p->goods[COLONIZE_CARGO_FOOD] > 0)
        ? (p->food_fish > food_amt ? food_amt : p->food_fish)
        : 0;
    const int grain_amt = food_amt - fish_amt;
    const int fish_icon = COLONY_ICON_FISH;
    const int grain_icon =
      grey_only ? (COLONY_CARGO_GREY_BASE + COLONIZE_CARGO_FOOD)
                : (COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_FOOD);
    colony_screen_draw_resource_count_pair(
      view,
      font,
      framebuffer,
      meter_x,
      meter_y,
      width[food_weight_idx],
      meter_h,
      fish_icon,
      fish_amt,
      grain_icon,
      grain_amt,
      (p->food_net < 0) ? 12 : 15,
      false
    );
    meter_x += width[food_weight_idx] + gap;
  }
  if (surplus_active) {
    colony_screen_draw_resource_count(
      view,
      font,
      framebuffer,
      meter_x,
      meter_y,
      width[surplus_weight_idx],
      meter_h,
      COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_FOOD,
      p->food_net,
      15,
      false
    );
    meter_x += width[surplus_weight_idx] + gap;
  }
  colony_screen_draw_resource_count(
    view, font, framebuffer, meter_x, meter_y, width[cross_weight_idx], meter_h,
    COLONY_ICON_CROSS, p->crosses, 15, false
  );
  meter_x += width[cross_weight_idx] + gap;
  colony_screen_draw_resource_count(
    view, font, framebuffer, meter_x, meter_y, width[bell_weight_idx], meter_h, COLONY_ICON_BELL,
    p->bells, 15, false
  );
}

static void colony_screen_draw_multifunction(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !framebuffer) {
    return;
  }
  colony_screen_blit_icon(
    view, COLONY_ICON_HOUSE, framebuffer, COLONY_MULTI_BTN_X, COLONY_PANEL_CONTENT_Y
  );
  colony_screen_blit_icon(
    view, COLONY_ICON_RIFLE, framebuffer, COLONY_MULTI_BTN_X, COLONY_PANEL_CONTENT_Y + 16
  );
  colony_screen_blit_icon(
    view, COLONY_ICON_HAMMER_BTN, framebuffer, COLONY_MULTI_BTN_X, COLONY_PANEL_CONTENT_Y + 32
  );
  {
    const int by = COLONY_PANEL_CONTENT_Y + (int)view->multi_mode * 16;
    colony_screen_draw_selection_box(framebuffer, COLONY_MULTI_BTN_X - 1, by - 1, 14, 14, 10);
  }

  const int px = COLONY_MULTI_X + 2;
  const int pane_w = COLONY_MULTI_W - 19; /* keep 15px clear before mode buttons */
  const int py = COLONY_PANEL_CONTENT_Y;
  const int pane_h = COLONY_PANEL_CONTENT_H;
  if (view->multi_mode == COLONY_MULTI_PRODUCTION && view->preview_valid) {
    /* Cargo goods + shortfalls + hammers (not crosses/bells). One slot per
     * cargo type — player-reported (New Amsterdam golden, pixel-checked):
     * a resource's produced/shortfall/potential numbers belong in a single
     * cell together, not scattered across separate grid cells:
     *   1. produced, nothing downstream wants it: one plain number.
     *   2. not produced, something wants it: one grey/red "short" number.
     *   3. produced, but less than something downstream wants: produced
     *      (white) + short (red) together, one cell, two side-by-side
     *      boxes with a spacer.
     *   4. produced in surplus of what's used: used + stored together,
     *      one cell, two side-by-side boxes with a spacer (both white).
     * Cases 3 and 4 share the same side-by-side rendering — the only
     * difference is the second box's icon/color (grey+red vs the same
     * cargo icon in white again).
     *
     * 2026-08-27, player-directed departure from DOS pixel-fidelity: case 4
     * now applies to *every* cargo a craft recipe draws on this tick, not
     * just Lumber->hammers (the golden only shows the split for Lumber —
     * Ore/Tools stay plain single numbers there even though the Blacksmith/
     * Armory visibly consume part of them). Explicitly not matching DOS
     * here — the player asked for the split everywhere as a UI
     * improvement, this pane only, not a "we got DOS wrong" fix.
     */
    const ColonizeColonyPreview* p = &view->preview;
    typedef struct ColonyProdSlot {
      int icon0;
      int amount0;
      uint8_t color0;
      int icon1; /* < 0 => single value, cases 1/2 */
      int amount1;
      uint8_t color1;
    } ColonyProdSlot;
    ColonyProdSlot slots[COLONIZE_CARGO_COUNT + 1];
    int slot_count = 0;
    /* Food is shown on the People band's fish/grain meter, not repeated
     * here — golden-confirmed (no Food badge in this pane). Every other
     * cargo shows its GROSS production this tick (field-worker output
     * plus, for a manufactured good, the building's own gross craft
     * output) rather than `goods[]`'s net-after-further-consumption — see
     * ColonizeColonyPreview.field_gross/craft_gross's header comment.
     * Horses has no craft recipe of its own (breeding only), so `goods[]`
     * is already the right (and only) figure for it. */
    for (int c = 1; c < COLONIZE_CARGO_COUNT; ++c) {
      if (slot_count >= (int)(sizeof(slots) / sizeof(slots[0]))) {
        break;
      }
      const int produced =
        (c == COLONIZE_CARGO_HORSES) ? p->goods[c] : (p->field_gross[c] + p->craft_gross[c]);
      const int short_amt = p->shortfall[c];

      if (c == COLONIZE_CARGO_LUMBER && short_amt <= 0 && p->hammers > 0 && produced > 0) {
        /* Case 4: Lumber->Hammers isn't a colony_craft_preview() recipe
         * (the Carpenter's hammers bank is `colony_prod_colony_hammers`, a
         * separate computation), so it never earns a shortfall[] entry —
         * but it's the one real surplus-of-what's-used case in this game.
         * Player-reported (New Amsterdam golden): 22 Lumber = 16 spent on
         * this tick's hammers + 6 left over, shown as two adjacent white
         * counters, not one plain "22". */
        int used = p->hammers;
        if (used > produced) {
          used = produced;
        }
        const int stored = produced - used;
        if (stored > 0) {
          ColonyProdSlot* s = &slots[slot_count++];
          s->icon0 = COLONY_CARGO_ICON_BASE + c;
          s->amount0 = used;
          s->color0 = 15;
          s->icon1 = COLONY_CARGO_ICON_BASE + c;
          s->amount1 = stored;
          s->color1 = 15;
          continue;
        }
      }

      if (short_amt <= 0 && produced > 0) {
        /* Case 4, general form: some *other* cargo's craft recipe (not
         * Lumber's hammers — that's the special case above) drew on this
         * tick's production as its raw input. Not DOS-accurate — DOS shows
         * Ore/Tools here as one plain number even when the Blacksmith/
         * Armory visibly consume part of it (checked against the golden:
         * 28 Ore, 24 Tools, both single) — a deliberate departure from
         * pixel-fidelity, player-requested: the Production tab is
         * explicitly not staying 1:1 with DOS here, splitting every
         * resource this way as a UI improvement. `goods[c]` is already the
         * net-of-consumption warehouse delta, so `produced - goods[c]` is
         * exactly what got drawn off this tick and `goods[c]` itself is
         * exactly what's left to store — no separate bookkeeping needed. */
        const int used = produced - p->goods[c];
        const int stored = p->goods[c];
        if (used > 0 && stored > 0) {
          ColonyProdSlot* s = &slots[slot_count++];
          s->icon0 = COLONY_CARGO_ICON_BASE + c;
          s->amount0 = used;
          s->color0 = 15;
          s->icon1 = COLONY_CARGO_ICON_BASE + c;
          s->amount1 = stored;
          s->color1 = 15;
          continue;
        }
      }

      if (short_amt > 0) {
        /* Cases 2/3: produced (white, 0 if nothing produced) paired with
         * the shortfall (red) in one cell — not summed into one number,
         * not two separate cells. */
        ColonyProdSlot* s = &slots[slot_count++];
        s->icon0 = produced > 0 ? COLONY_CARGO_ICON_BASE + c : -1;
        s->amount0 = produced;
        s->color0 = 15;
        s->icon1 = COLONY_CARGO_GREY_BASE + c;
        s->amount1 = short_amt;
        s->color1 = 12;
      } else if (produced > 0) {
        /* Case 1. */
        ColonyProdSlot* s = &slots[slot_count++];
        s->icon0 = COLONY_CARGO_ICON_BASE + c;
        s->amount0 = produced;
        s->color0 = 15;
        s->icon1 = -1;
        s->amount1 = 0;
        s->color1 = 0;
      }
    }
    if (p->hammers > 0 && slot_count < (int)(sizeof(slots) / sizeof(slots[0]))) {
      ColonyProdSlot* s = &slots[slot_count++];
      s->icon0 = COLONY_ICON_HAMMER;
      s->amount0 = p->hammers;
      s->color0 = 15;
      s->icon1 = -1;
      s->amount1 = 0;
      s->color1 = 0;
    }
    if (slot_count > 0 && pane_w > 0 && pane_h > 0) {
      /* Prefer a single column; add columns when rows would be shorter than icons. */
      const int min_row_h = 8;
      int cols = 1;
      int rows = slot_count;
      while (cols < slot_count && pane_h / rows < min_row_h) {
        cols++;
        rows = (slot_count + cols - 1) / cols;
      }
      const int cell_w = pane_w / cols;
      const int cell_h = pane_h / rows;
      for (int i = 0; i < slot_count; ++i) {
        const int col = i / rows;
        const int row = i % rows;
        const int sx = px + col * cell_w;
        const int sy = py + row * cell_h;
        const ColonyProdSlot* s = &slots[i];
        if (s->icon1 < 0 || s->icon0 < 0) {
          /* Single value: cases 1/2 outright, and case 2's "nothing
           * produced" collapses here too rather than splitting an empty
           * left half. */
          const int icon = s->icon1 < 0 ? s->icon0 : s->icon1;
          const int amount = s->icon1 < 0 ? s->amount0 : s->amount1;
          const uint8_t color = s->icon1 < 0 ? s->color0 : s->color1;
          colony_screen_draw_resource_count(
            view, font, framebuffer, sx, sy, cell_w, cell_h, icon, amount, color, false
          );
        } else {
          /* Cases 3/4: two independent boxes sharing this cell's width,
           * with the gap between them the spacer the player asked for —
           * produced/used on the left, shortfall/stored on the right. */
          const int half = cell_w / 2;
          colony_screen_draw_resource_count(
            view, font, framebuffer, sx, sy, half, cell_h, s->icon0, s->amount0, s->color0, false
          );
          colony_screen_draw_resource_count(
            view, font, framebuffer, sx + half, sy, cell_w - half, cell_h, s->icon1, s->amount1,
            s->color1, false
          );
        }
      }
    }
  } else if (view->multi_mode == COLONY_MULTI_UNITS && units) {
    /* Land units at the colony (soldiers, colonists, scouts, artillery, …);
     * ships/wagons stay on the Transport strip — see
     * colony_screen_multi_units_layout. LABELS.TXT @CMISC "Units Present"
     * title (golden-confirmed: New Amsterdam's Military tab), centered,
     * dark blue (WOODPANL.PIK idx 57, exact RGB match against the golden's
     * sampled ink color). */
    if (font) {
      const char* title = "Units Present";
      const int tw = font_text_width(font, title);
      font_draw_text(font, framebuffer, px + (pane_w - tw) / 2, py, title, 57);
    }
    ColonyMultiUnitSlot slots[COLONY_MULTI_UNITS_SLOT_MAX];
    const int slot_count = colony_screen_multi_units_layout(
      view, units, px, py + COLONY_MULTI_UNITS_TITLE_H, pane_w, pane_h - COLONY_MULTI_UNITS_TITLE_H,
      slots, COLONY_MULTI_UNITS_SLOT_MAX
    );
    for (int i = 0; i < slot_count; ++i) {
      const ColonizeUnit* u = units_get_const(units, slots[i].unit_id);
      const int sprite = u ? colony_screen_outside_display_sprite(units, u) : -1;
      if (!u || sprite < 0) {
        continue;
      }
      unit_chrome_blit_unit_for_palette(
        framebuffer,
        font,
        &view->icons,
        sprite,
        slots[i].x,
        slots[i].y,
        units_display_type_index(units, u->id),
        u->nation_id,
        u->orders,
        false,
        u->aboard_ship_id >= 0,
        (view->frame_ok && view->frame.has_palette) ? &view->frame.palette : NULL
      );
      if (view->multi_unit_selected_id == u->id) {
        colony_screen_draw_chrome_selection(view, framebuffer, sprite, slots[i].x, slots[i].y);
      }
    }
  } else if (view->multi_mode == COLONY_MULTI_CONSTRUCTION && colony && pool) {
    char line[64];
    const ColonizeBuildingType* bt =
      (colony->building_in_production >= 0)
        ? colonies_building_type(pool, colony->building_in_production)
        : NULL;
    /* Col1 also encodes buildable *units* (only Artillery modeled) in this
     * same field, using codes past the @BUILDING table's own range
     * (colonies_building_type / colonies_find_building only cover real
     * buildings) — col1_bridge_apply copies the raw code through verbatim
     * for anything that isn't the one special-cased Stockade remap.
     * colonies_unit_build_info is the single source of truth for name/
     * hammers/tools_cost, shared with colonies_set_construction/_list_
     * buildable/_buy_construction/_try_complete_unit_construction. */
    const char* unit_name = NULL;
    int unit_hammers = 0;
    int unit_tools = 0;
    if (!bt) {
      colonies_unit_build_info(colony->building_in_production, &unit_name, &unit_hammers, &unit_tools);
    }
    if (font) {
      if (bt) {
        snprintf(line, sizeof(line), "%s", bt->name);
      } else if (unit_name) {
        snprintf(line, sizeof(line), "%s", unit_name);
      } else {
        snprintf(line, sizeof(line), "none");
      }
      const int title_w = font_text_width(font, line);
      font_draw_text(font, framebuffer, px + (pane_w - title_w) / 2, py, line, 57);
      {
        UiButtonColors bc;
        bc.dark = 0x31;
        bc.light = 0x3f;
        bc.text = 15;
        bc.hotkey = 14;
        if (view->frame_ok && view->frame.has_palette) {
          /* Remap Europe-style blues into the colony frame palette. */
          const ColonizePalette* pal = &view->frame.palette;
          int best_d = 1 << 30;
          int best_l = 1 << 30;
          uint8_t dark = bc.dark;
          uint8_t light = bc.light;
          for (int j = 0; j < 256; ++j) {
            const int r = pal->rgb[j][0];
            const int g = pal->rgb[j][1];
            const int b = pal->rgb[j][2];
            const int dd = (r - 20) * (r - 20) + (g - 40) * (g - 40) + (b - 120) * (b - 120);
            const int ld = (r - 180) * (r - 180) + (g - 200) * (g - 200) + (b - 255) * (b - 255);
            if (dd < best_d) {
              best_d = dd;
              dark = (uint8_t)j;
            }
            if (ld < best_l) {
              best_l = ld;
              light = (uint8_t)j;
            }
          }
          bc.dark = dark;
          bc.light = light;
        }
        int buy_w = 0;
        int buy_h = 0;
        int chg_w = 0;
        int chg_h = 0;
        ui_button_measure(font, "~BUY", &buy_w, &buy_h);
        ui_button_measure(font, "~CHANGE", &chg_w, &chg_h);
        /* Player-reported: BUY aligned vertically with CHANGE (both 4px up
         * from the original placement). */
        ui_button_draw(font, framebuffer, px, py + 10 - 4, buy_w, buy_h, "~BUY", &bc);
        const int change_x = COLONY_MULTI_X + COLONY_MULTI_W - chg_w - 4 - 10;
        ui_button_draw(font, framebuffer, change_x, py + 10 - 4, chg_w, chg_h, "~CHANGE", &bc);
      }
    }
    /* Accumulated carpenter hammers toward the current project, as four
     * rows of one-fourth of the total need each — player-requested (was a
     * single packed row spanning the pane width, golden-confirmed for that
     * shape, but the player asked for a 4-row quartile layout instead, a
     * deliberate UI improvement over DOS here same as the Production tab's
     * surplus-split departure). Hammers fill row 0 first, then row 1, etc.
     * — not a proportional bar, an actual fill order. Each row's capacity
     * is need/4 (remainder spread across the first rows so all four
     * capacities sum to exactly `need`). */
    const int need = bt ? bt->hammers : unit_hammers;
    const int have = colony->hammers > 0 ? colony->hammers : 0;
    const int show = (need > 0 && have > need) ? need : have;
    if (need > 0 && view->icons_ok && COLONY_ICON_HAMMER < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[COLONY_ICON_HAMMER];
      const int iw = (sp && sp->width > 0) ? sp->width : 8;
      const int ih = (sp && sp->height > 0) ? sp->height : 12;
      /* Player-reported: 3px lower (row 0 was overlapping the BUY/CHANGE
       * buttons above it). */
      const int area_y = py + 19;
      const int area_h = 24; /* leaves room above (buttons) and below (tools line) in the box */
      const int row_h = area_h / 4;
      const int base = need / 4;
      const int rem = need % 4;
      /* Player-reported: either every row with hammers shows a number, or
       * none do — never a mix. One density check for the whole bar (all
       * four rows share ~the same capacity, off by at most 1), based on the
       * fullest row: if that many icons can't fit without overlapping,
       * icons stop being individually countable, so every non-empty row
       * gets a number instead of icons; otherwise every row stays icons-only
       * with no numbers at all. */
      const int max_row_capacity = base + (rem > 0 ? 1 : 0);
      const bool dense = max_row_capacity > 0 && max_row_capacity * iw > pane_w;
      int row_filled[4] = {0, 0, 0, 0};
      int remaining = show;
      for (int r = 0; r < 4; ++r) {
        const int row_capacity = base + (r < rem ? 1 : 0);
        int filled = remaining;
        if (filled > row_capacity) {
          filled = row_capacity;
        }
        remaining -= filled;
        row_filled[r] = filled;
        if (filled <= 0 || row_capacity <= 0) {
          continue;
        }
        const int row_y = area_y + r * row_h;
        /* No black backing rect (player-reported, earlier fix) — draws
         * straight onto the pane background like every other resource
         * counter here. */
        const int iy = row_y + (row_h - ih) / 2;
        /* Player-reported: this is a progress bar, not a stretched fill —
         * icon slot positions are fixed by the row's total capacity, not by
         * how many are currently filled, so hammers pack in from the left
         * and only reach the right edge once the row is actually full
         * (instead of re-spreading across the whole width on every icon
         * added). */
        if (row_capacity <= 1 || pane_w <= iw) {
          ss_blit_sprite(&view->icons, COLONY_ICON_HAMMER, framebuffer, px, iy);
        } else {
          const int span = pane_w - iw;
          for (int i = 0; i < filled; ++i) {
            const int ix = px + (i * span) / (row_capacity - 1);
            ss_blit_sprite(&view->icons, COLONY_ICON_HAMMER, framebuffer, ix, iy);
          }
        }
      }
      /* Dense rows also get a number, drawn last so it sits on top of that
       * row's (overlapping, hard-to-count) icons instead of replacing them —
       * player-reported: numbers had been replacing the icons outright. */
      if (dense && font) {
        for (int r = 0; r < 4; ++r) {
          if (row_filled[r] <= 0) {
            continue;
          }
          char num[12];
          snprintf(num, sizeof(num), "%d", row_filled[r]);
          colony_screen_draw_outlined_number(font, framebuffer, px + 1, area_y + r * row_h, num, 15);
        }
      }
    }
    const int tools_cost = bt ? bt->tools_cost : unit_tools;
    if (tools_cost > 0 && font) {
      snprintf(line, sizeof(line), "(Requires %d Tools)", tools_cost);
      const int line_w = font_text_width(font, line);
      /* Player-reported: 6px higher (fits inside the box now — the old
       * py+46 sat 1px past COLONY_PANEL_CONTENT_H's bottom edge) and
       * centered horizontally. Grey when the colony already has enough
       * tools in store, white (the "needs attention" color used elsewhere
       * in this screen) when short. Index 8 (dark grey), not 7 — font.c's
       * draw_ff_glyph hardcodes color==7 to the same white AA blend as
       * color==15 (FF_COLOR_MAP, "unbold white"), so 7 renders
       * indistinguishable from white here; 8 hits the plain solid-ink path
       * instead. */
      const bool tools_ok = colony->stock[COLONIZE_CARGO_TOOLS] >= tools_cost;
      font_draw_text(
        font, framebuffer, px + (pane_w - line_w) / 2, py + 46 - 6, line, tools_ok ? 8 : 15
      );
    }
  }
}

static void colony_screen_fill_rect(
  ColonizeFramebuffer8* framebuffer,
  int x0,
  int y0,
  int x1,
  int y1,
  uint8_t color
) {
  if (!framebuffer || !framebuffer->pixels) {
    return;
  }
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      if (x >= 0 && y >= 0 && x < framebuffer->width && y < framebuffer->height) {
        framebuffer->pixels[y * framebuffer->width + x] = color;
      }
    }
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
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 4;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h + rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  int dialog_w = 200;
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  const int dialog_x = (framebuffer->width - dialog_w) / 2;
  const int dialog_y = 24;

  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    view->wood_tile_ok ? &view->wood_tile : NULL,
    &colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  view->construction_dialog_x = dialog_x;
  view->construction_dialog_y = dialog_y;
  view->construction_dialog_w = dialog_w;
  view->construction_dialog_h = dialog_h;
  view->construction_line_h = line_h;

  if (font && inner_w > 0) {
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, "Construction", 15);
  }
  const int list_y0 = inner_y + pad + line_h;
  view->construction_list_y0 = list_y0;

  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == view->construction_selection);
    if (selected) {
      colony_screen_fill_rect(
        framebuffer, inner_x + 1, row_y - 1, inner_x + inner_w - 1, row_y + line_h - 1, 138
      );
    }
    char label[80];
    if (i == 0) {
      snprintf(label, sizeof(label), "Clear project");
    } else {
      const int bid = view->buildable_ids[i - 1];
      const ColonizeBuildingType* bt = colonies_building_type(pool, bid);
      const char* uname = NULL;
      int uh = 0;
      int ut = 0;
      /* Player-reported: hammers shown here are what's still *needed*, i.e.
       * the requirement adjusted down by the colony's already-banked
       * hammers (min 0) — hammers carry over to whatever project is picked,
       * unlike tools, which are never adjusted away in this popup. */
      const int stored_hammers = colony ? colony->hammers : 0;
      if (bt) {
        int hammers_left = bt->hammers - stored_hammers;
        if (hammers_left < 0) {
          hammers_left = 0;
        }
        if (bt->tools_cost > 0) {
          snprintf(label, sizeof(label), "%s (%dH, %dT)", bt->name, hammers_left, bt->tools_cost);
        } else {
          snprintf(label, sizeof(label), "%s (%dH)", bt->name, hammers_left);
        }
      } else if (colonies_unit_build_info(bid, &uname, &uh, &ut)) {
        /* Artillery (colonies_unit_build_info) — not a real @BUILDING row. */
        int hammers_left = uh - stored_hammers;
        if (hammers_left < 0) {
          hammers_left = 0;
        }
        snprintf(label, sizeof(label), "%s (%dH, %dT)", uname, hammers_left, ut);
      } else {
        snprintf(label, sizeof(label), "%s (%dH)", "?", 0);
      }
    }
    if (font) {
      font_draw_text(font, framebuffer, inner_x + pad, row_y + 1, label, 15);
    }
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
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 4;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h + rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  int dialog_w = 170;
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  const int dialog_x = (framebuffer->width - dialog_w) / 2;
  const int dialog_y = 28;

  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    view->wood_tile_ok ? &view->wood_tile : NULL,
    &colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  view->jobs_dialog_x = dialog_x;
  view->jobs_dialog_y = dialog_y;
  view->jobs_dialog_w = dialog_w;
  view->jobs_dialog_h = dialog_h;
  view->jobs_line_h = line_h;

  if (font && inner_w > 0) {
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, "Field job", 15);
  }
  const int list_y0 = inner_y + pad + line_h;
  view->jobs_list_y0 = list_y0;

  int dx = 0;
  int dy = 0;
  colonies_field_tile_delta(view->jobs_tile_index, &dx, &dy);
  const int tx = colony ? colony->x + dx : 0;
  const int ty = colony ? colony->y + dy : 0;

  /* Docks (or an upgrade: Drydock/Shipyard) gates Fisherman yield to 0 —
   * FUN_15eb_18ec ~11925-11939. Must match turn.c's check. */
  bool has_docks = false;
  if (pool && colony) {
    for (int bi = 0; bi < pool->building_type_count && bi < COLONIZE_BUILDING_TYPES_MAX; ++bi) {
      if (!colony->has_building[bi]) {
        continue;
      }
      const char* dn = pool->building_types[bi].name;
      if (dn && (strstr(dn, "Docks") != NULL || strstr(dn, "Drydock") != NULL ||
                 strstr(dn, "Shipyard") != NULL)) {
        has_docks = true;
        break;
      }
    }
  }

  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == view->jobs_selection);
    if (selected) {
      colony_screen_fill_rect(
        framebuffer, inner_x + 1, row_y - 1, inner_x + inner_w - 1, row_y + line_h - 1, 138
      );
    }
    char label[48];
    const int job = view->job_ids[i];
    int profession = COLONIZE_PROF_FREE_COLONIST;
    if (colony && view->selected_colonist >= 0 &&
        view->selected_colonist < colony->colonist_count) {
      profession = colony->colonists[view->selected_colonist].profession;
    }
    int yld = (map && colony) ? colony_yield_for_worker(map, tx, ty, job, profession, has_docks, 0, 0) : 0;
    /* Henry Hudson: fur trapper output +100% — same gap/fix as
     * colony_screen_draw_area_overlays above. */
    if (yld > 0 && job == COLONIZE_JOB_FUR_TRAPPER && colony && col1 &&
        founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON)) {
      yld *= 2;
    }
    snprintf(label, sizeof(label), "%s (%d)", colony_yield_job_name(job), yld);
    if (font) {
      font_draw_text(font, framebuffer, inner_x + pad, row_y + 1, label, 15);
    }
  }
}

/* colony.h's COLONIZE_CARGO_* order — see reports.c's k_cargo_names for the
 * same list (kept as its own local copy, matching this file's existing
 * per-module convention rather than a shared header array). */
static const char* const k_custom_house_cargo_names[COLONIZE_CARGO_COUNT] = {
  "Food",   "Sugar",  "Tobacco",     "Cotton", "Furs",    "Lumber", "Ore",    "Silver",
  "Horses", "Rum",    "Cigars",      "Cloth",  "Coats",   "Trade Goods",
  "Tools",  "Muskets"
};

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
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->custom_house_open || !colony || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int rows = view->custom_house_count;
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 4;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h + rows * line_h + pad;
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }
  /* GAME.TXT @CUSTOM's own @width=190 — the title ("Which cargos shall our
   * Custom House export?") is the widest line, not any cargo name. */
  int dialog_w = 130;
  if (font) {
    const int title_w = font_text_width(font, view->custom_house_title) + pad * 2;
    if (title_w > dialog_w) {
      dialog_w = title_w;
    }
  }
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  const int dialog_x = (framebuffer->width - dialog_w) / 2;
  const int dialog_y = 20;

  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    view->wood_tile_ok ? &view->wood_tile : NULL,
    &colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  view->custom_house_dialog_x = dialog_x;
  view->custom_house_dialog_y = dialog_y;
  view->custom_house_dialog_w = dialog_w;
  view->custom_house_dialog_h = dialog_h;
  view->custom_house_line_h = line_h;

  if (font && inner_w > 0) {
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, view->custom_house_title, 15);
  }
  const int list_y0 = inner_y + pad + line_h;
  view->custom_house_list_y0 = list_y0;

  /* Uniform dark green (player-reported: not the brighter green some rows
   * used before — the state is the bullet's job now, not the text color). */
  const uint8_t kRowColor = 2;
  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const int cargo = view->custom_house_cargo_ids[i];
    const bool on = europe_custom_house_cargo_enabled(colony->custom_house_bits, cargo);
    const char* name = (cargo >= 0 && cargo < COLONIZE_CARGO_COUNT)
      ? k_custom_house_cargo_names[cargo]
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
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  const int dialog_x = (framebuffer->width - dialog_w) / 2;
  const int dialog_y = 28;

  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    view->wood_tile_ok ? &view->wood_tile : NULL,
    &colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  view->eject_dialog_x = dialog_x;
  view->eject_dialog_y = dialog_y;
  view->eject_dialog_w = dialog_w;
  view->eject_dialog_h = dialog_h;
  view->eject_line_h = line_h;

  if (font && inner_w > 0) {
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, "Leave as", 15);
  }
  const int list_y0 = inner_y + pad + line_h;
  view->eject_list_y0 = list_y0;

  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == view->eject_selection);
    if (selected) {
      colony_screen_fill_rect(
        framebuffer, inner_x + 1, row_y - 1, inner_x + inner_w - 1, row_y + line_h - 1, 138
      );
    }
    const char* name = colonies_eject_role_name(view->eject_roles[i]);
    if (font) {
      font_draw_text(font, framebuffer, inner_x + pad, row_y + 1, name, 15);
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
  int dialog_w = 190;
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  const int dialog_x = (framebuffer->width - dialog_w) / 2;
  const int dialog_y = 28;

  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    view->wood_tile_ok ? &view->wood_tile : NULL,
    &colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  view->dock_orders_dialog_x = dialog_x;
  view->dock_orders_dialog_y = dialog_y;
  view->dock_orders_dialog_w = dialog_w;
  view->dock_orders_dialog_h = dialog_h;
  view->dock_orders_line_h = line_h;

  if (font && inner_w > 0) {
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, view->dock_orders_title, 15);
  }
  const int list_y0 = inner_y + pad + line_h;
  view->dock_orders_list_y0 = list_y0;

  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == view->dock_orders_selection);
    if (selected) {
      colony_screen_fill_rect(
        framebuffer, inner_x + 1, row_y - 1, inner_x + inner_w - 1, row_y + line_h - 1, 138
      );
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
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  const int dialog_x = (framebuffer->width - dialog_w) / 2;
  const int dialog_y = 36;

  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    view->wood_tile_ok ? &view->wood_tile : NULL,
    &colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  view->message_dialog_x = dialog_x;
  view->message_dialog_y = dialog_y;
  view->message_dialog_w = dialog_w;
  view->message_dialog_h = dialog_h;
  view->message_line_h = line_h;

  if (font && inner_w > 0) {
    font_draw_text(font, framebuffer, inner_x + pad, inner_y + pad, view->message_text, 15);
  }
  const int list_y0 = inner_y + pad + text_lines * line_h;
  view->message_list_y0 = list_y0;
  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == view->message_selection);
    if (selected) {
      colony_screen_fill_rect(
        framebuffer, inner_x + 1, row_y - 1, inner_x + inner_w - 1, row_y + line_h - 1, 138
      );
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
   * from what's drawn (same per-colony-deterministic assignment). */
  int slot_x[32];
  int slot_y[32];
  colony_screen_assign_slot_positions(pool, colony, slot_x, slot_y);

  if (view->message_kind != COLONY_MSG_NONE) {
    if (mx < view->message_dialog_x || my < view->message_dialog_y ||
        mx >= view->message_dialog_x + view->message_dialog_w ||
        my >= view->message_dialog_y + view->message_dialog_h) {
      hit.kind = COLONY_HIT_MESSAGE_OUTSIDE;
      return hit;
    }
    if (view->message_line_h > 0 && my >= view->message_list_y0) {
      const int idx = (my - view->message_list_y0) / view->message_line_h;
      const int rows = (view->message_kind == COLONY_MSG_CONFIRM) ? 2 : 1;
      if (idx >= 0 && idx < rows) {
        if (view->message_kind == COLONY_MSG_OK) {
          hit.kind = COLONY_HIT_MESSAGE_OK;
        } else if (idx == 0) {
          hit.kind = COLONY_HIT_MESSAGE_YES;
        } else {
          hit.kind = COLONY_HIT_MESSAGE_NO;
        }
        return hit;
      }
    }
    return hit;
  }

  if (view->custom_house_open) {
    if (mx < view->custom_house_dialog_x || my < view->custom_house_dialog_y ||
        mx >= view->custom_house_dialog_x + view->custom_house_dialog_w ||
        my >= view->custom_house_dialog_y + view->custom_house_dialog_h) {
      hit.kind = COLONY_HIT_CUSTOM_HOUSE_OUTSIDE;
      return hit;
    }
    if (view->custom_house_line_h > 0 && my >= view->custom_house_list_y0) {
      const int idx = (my - view->custom_house_list_y0) / view->custom_house_line_h;
      if (idx >= 0 && idx < view->custom_house_count) {
        hit.kind = COLONY_HIT_CUSTOM_HOUSE_ROW;
        hit.index = idx;
        return hit;
      }
    }
    return hit;
  }

  if (view->jobs_open) {
    if (mx < view->jobs_dialog_x || my < view->jobs_dialog_y ||
        mx >= view->jobs_dialog_x + view->jobs_dialog_w ||
        my >= view->jobs_dialog_y + view->jobs_dialog_h) {
      hit.kind = COLONY_HIT_JOBS_OUTSIDE;
      return hit;
    }
    if (view->jobs_line_h > 0 && my >= view->jobs_list_y0) {
      const int idx = (my - view->jobs_list_y0) / view->jobs_line_h;
      const int rows = view->job_count;
      if (idx >= 0 && idx < rows) {
        hit.kind = COLONY_HIT_JOBS_ROW;
        hit.index = idx;
        return hit;
      }
    }
    return hit;
  }

  if (view->eject_open) {
    if (mx < view->eject_dialog_x || my < view->eject_dialog_y ||
        mx >= view->eject_dialog_x + view->eject_dialog_w ||
        my >= view->eject_dialog_y + view->eject_dialog_h) {
      hit.kind = COLONY_HIT_EJECT_OUTSIDE;
      return hit;
    }
    if (view->eject_line_h > 0 && my >= view->eject_list_y0) {
      const int idx = (my - view->eject_list_y0) / view->eject_line_h;
      if (idx >= 0 && idx < view->eject_role_count) {
        hit.kind = COLONY_HIT_EJECT_ROW;
        hit.index = idx;
        return hit;
      }
    }
    return hit;
  }

  if (view->dock_orders_open) {
    if (mx < view->dock_orders_dialog_x || my < view->dock_orders_dialog_y ||
        mx >= view->dock_orders_dialog_x + view->dock_orders_dialog_w ||
        my >= view->dock_orders_dialog_y + view->dock_orders_dialog_h) {
      hit.kind = COLONY_HIT_DOCK_ORDERS_OUTSIDE;
      return hit;
    }
    if (view->dock_orders_line_h > 0 && my >= view->dock_orders_list_y0) {
      const int idx = (my - view->dock_orders_list_y0) / view->dock_orders_line_h;
      if (idx >= 0 && idx < view->dock_orders_count) {
        hit.kind = COLONY_HIT_DOCK_ORDERS_ROW;
        hit.index = idx;
        return hit;
      }
    }
    return hit;
  }

  if (view->construction_open) {
    if (mx < view->construction_dialog_x || my < view->construction_dialog_y ||
        mx >= view->construction_dialog_x + view->construction_dialog_w ||
        my >= view->construction_dialog_y + view->construction_dialog_h) {
      hit.kind = COLONY_HIT_CONSTRUCTION_OUTSIDE;
      return hit;
    }
    if (view->construction_line_h > 0 && my >= view->construction_list_y0) {
      const int idx = (my - view->construction_list_y0) / view->construction_line_h;
      const int rows = view->buildable_count + 1;
      if (idx >= 0 && idx < rows) {
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
    if (mx >= COLONY_HOLD_X + 4 && holds > 0) {
      const int idx = (mx - (COLONY_HOLD_X + 4)) / COLONY_HOLD_PITCH;
      if (idx >= 0 && idx < holds &&
          mx < COLONY_HOLD_X + 4 + idx * COLONY_HOLD_PITCH + COLONY_HOLD_W) {
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
      if (mx < mid) {
        hit.kind = COLONY_HIT_MULTI_BUY;
      } else {
        hit.kind = COLONY_HIT_MULTI_CHANGE;
      }
      return hit;
    }
    hit.kind = COLONY_HIT_MULTI_PANE;
    hit.index = (int)view->multi_mode;
    return hit;
  }

  /* Docked transport icons. */
  if (view->docked_transport_count > 0 && my >= COLONY_TRANSPORT_ICON_Y &&
      my < COLONY_TRANSPORT_ICON_Y + 16 && mx >= COLONY_TRANSPORT_X &&
      mx < COLONY_TRANSPORT_X + COLONY_TRANSPORT_W) {
    const int idx = (mx - (COLONY_TRANSPORT_X + 4)) / COLONY_TRANSPORT_PITCH;
    if (idx >= 0 && idx < view->docked_transport_count) {
      hit.kind = COLONY_HIT_TRANSPORT;
      hit.index = idx;
      return hit;
    }
  }

  /* Outside units on fortification strip (Note 1; per-icon selectable). */
  if (view->outside_unit_count > 0 && units && view->icons_ok) {
    int fence_w = COLONY_FENCE_W;
    int fence_h = COLONY_FENCE_H;
    const int fort = colony_screen_best_built(pool, colony, k_slot_stockade, 3);
    const int fort_sprite = fort >= 0 ? fort : COLONY_FENCE_SPRITE;
    if (view->buildings_ok && fort_sprite >= 0 && fort_sprite < view->buildings.sprite_count) {
      const ColonizeSprite* spr = &view->buildings.sprites[fort_sprite];
      if (spr && spr->width > 2 && spr->height > 2) {
        fence_w = spr->width;
        fence_h = spr->height;
      }
    }
    int fence_x, fence_y, coast_x_unused, coast_y_unused;
    colony_screen_docks_fence_anchor(
      colony, fence_w, fence_h, &fence_x, &fence_y, &coast_x_unused, &coast_y_unused
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
      for (int i = 0; i < n; ++i) {
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
    int fence_w = COLONY_FENCE_W;
    int fence_h = COLONY_FENCE_H;
    const int fort = colony_screen_best_built(pool, colony, k_slot_stockade, 3);
    const int fort_sprite = fort >= 0 ? fort : COLONY_FENCE_SPRITE;
    if (fort_sprite >= 0 && fort_sprite < view->buildings.sprite_count) {
      const ColonizeSprite* spr = &view->buildings.sprites[fort_sprite];
      if (spr && spr->width > 2 && spr->height > 2) {
        fence_w = spr->width;
        fence_h = spr->height;
      }
    }
    int fence_x, fence_y, coast_x_unused, coast_y_unused;
    colony_screen_docks_fence_anchor(
      colony, fence_w, fence_h, &fence_x, &fence_y, &coast_x_unused, &coast_y_unused
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
    for (int i = 0; i < k_building_slot_count; ++i) {
      const ColonyBuildingSlot* slot = &k_building_slots[i];
      size_t nchain = 0;
      while (slot->chain && slot->chain[nchain]) {
        ++nchain;
      }
      const int built = colony_screen_best_built(pool, colony, slot->chain, nchain);
      if (built < 0 || built >= view->buildings.sprite_count) {
        continue;
      }
      const ColonizeSprite* bspr = &view->buildings.sprites[built];
      if (!bspr || bspr->width <= 2 || bspr->height <= 2) {
        continue;
      }
      int worker_ci[COLONY_BUILDING_WORKERS_MAX];
      int worker_icons[COLONY_BUILDING_WORKERS_MAX];
      int workers = 0;
      for (int ci = 0; ci < colony->colonist_count && workers < COLONY_BUILDING_WORKERS_MAX; ++ci) {
        const ColonizeColonist* c = &colony->colonists[ci];
        if (!c->active || c->building_type != built) {
          continue;
        }
        const int sprite =
          units_working_colonist_sprite(units, c->unit_type_index, c->profession);
        if (sprite < 0) {
          continue;
        }
        worker_ci[workers] = ci;
        worker_icons[workers] = sprite;
        workers++;
      }
      if (workers <= 0) {
        continue;
      }
      const int bx = slot_ox + slot_x[i];
      const int by = slot_oy + slot_y[i];
      int strip_h = 16;
      for (int wi = 0; wi < workers; ++wi) {
        if (worker_icons[wi] < view->icons.sprite_count) {
          const int ih = view->icons.sprites[worker_icons[wi]].height;
          if (ih > strip_h) {
            strip_h = ih;
          }
        }
      }
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
    }
  }

  /* People-view: colony colonists then outside (fence) units on one row. */
  if (colony && my >= COLONY_PANEL_CONTENT_Y + 16 && my < COLONY_PANEL_CONTENT_Y + 32 &&
      mx >= COLONY_PEOPLE_X && mx < COLONY_PEOPLE_X + COLONY_PEOPLE_W) {
    int x = COLONY_PEOPLE_X + 2;
    const int x_limit = COLONY_PEOPLE_X + COLONY_PEOPLE_W - 14;
    int colonists_drawn = 0;
    for (int i = 0; i < colony->colonist_count; ++i) {
      const ColonizeColonist* c = &colony->colonists[i];
      if (!c->active) {
        continue;
      }
      if (x > x_limit) {
        break;
      }
      const int sprite =
        units_working_colonist_sprite(units, c->unit_type_index, c->profession);
      int iw = 12;
      if (view->icons_ok && sprite >= 0 && sprite < view->icons.sprite_count) {
        const ColonizeSprite* sp = &view->icons.sprites[sprite];
        if (sp && sp->width > 0) {
          iw = sp->width;
        }
      }
      if (mx >= x && mx < x + iw) {
        hit.kind = COLONY_HIT_PEOPLE_COLONIST;
        hit.index = i;
        return hit;
      }
      x += iw + 2;
      colonists_drawn++;
    }
    if (view->outside_unit_count > 0 && x <= x_limit) {
      if (colonists_drawn > 0) {
        x += 6;
      }
      for (int i = 0; i < view->outside_unit_count; ++i) {
        const ColonizeUnit* u = units_get_const(units, view->outside_unit_ids[i]);
        if (!u) {
          continue;
        }
        if (x > x_limit) {
          break;
        }
        const int sprite = colony_screen_outside_display_sprite(units, u);
        int iw = 12;
        if (view->icons_ok && sprite >= 0 && sprite < view->icons.sprite_count) {
          const ColonizeSprite* sp = &view->icons.sprites[sprite];
          if (sp && sp->width > 0) {
            iw = sp->width;
          }
        }
        if (mx >= x && mx < x + iw) {
          hit.kind = COLONY_HIT_OUTSIDE_UNIT;
          hit.index = i;
          return hit;
        }
        x += iw + 2;
      }
    }
  }

  if (pool && view->buildings_ok && mx >= COLONY_VIEWPORT_X &&
      mx < COLONY_VIEWPORT_X + COLONY_VIEWPORT_W && my >= COLONY_VIEWPORT_Y &&
      my < COLONY_BOTTOM_SEPARATOR_Y) {
    const int slot_ox = COLONY_VIEWPORT_X;
    const int slot_oy = COLONY_VIEWPORT_Y;
    for (int i = 0; i < k_building_slot_count; ++i) {
      const ColonyBuildingSlot* slot = &k_building_slots[i];
      size_t n = 0;
      while (slot->chain && slot->chain[n]) {
        ++n;
      }
      const int built = colony_screen_best_built(pool, colony, slot->chain, n);
      if (built < 0 && slot_x[i] == COLONY_SLOT_HIDDEN) {
        continue; /* per-colony override: this placeholder isn't drawn */
      }
      const int sprite = (built >= 0) ? built : slot->tree_sprite;
      if (sprite < 0 || sprite >= view->buildings.sprite_count) {
        continue;
      }
      const ColonizeSprite* spr = &view->buildings.sprites[sprite];
      if (!spr || spr->width <= 2 || spr->height <= 2) {
        continue;
      }
      const int bx = slot_ox + slot_x[i];
      const int by = slot_oy + slot_y[i];
      if (mx >= bx && mx < bx + spr->width && my >= by && my < by + spr->height) {
        hit.kind = COLONY_HIT_BUILDING;
        hit.index = built;
        return hit;
      }
    }
  }

  return hit;
}

void colony_screen_render(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  const ColonizeCol1Save* col1,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  const ColonizeFont* font,
  bool debug_building_rects,
  ColonizeFramebuffer8* framebuffer
) {
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
    colony_screen_refresh_preview(view, pool, colony, map, col1);
  }

  colony_screen_draw_top_bar(colony, game_year, game_autumn, gold, font, framebuffer);

  colony_screen_fill_parch(view, framebuffer);
  colony_screen_blit_buildings(view, pool, colony, units, font, debug_building_rects, framebuffer);

  colony_screen_fill_wood_tile(view, framebuffer);
  if (colony && map && terrain) {
    colony_screen_render_minimap(map, terrain, phys0, colony->x, colony->y, framebuffer);
    colony_screen_draw_area_overlays(
      view, pool, colony, units, map, col1, font, debug_building_rects, framebuffer
    );
  }
  if (view && view->bottom_panel_ok) {
    pik_blit(&view->bottom_panel, framebuffer, 0, COLONY_BOTTOM_PANEL_Y);
  }

  colony_screen_draw_hline(framebuffer, COLONY_TOP_SEPARATOR_Y, 0);
  colony_screen_draw_hline(framebuffer, COLONY_BOTTOM_SEPARATOR_Y, 0);
  colony_screen_draw_vline(
    framebuffer,
    COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - 1,
    COLONY_MIDDLE_Y,
    COLONY_BOTTOM_SEPARATOR_Y - 1,
    0
  );

  if (view) {
    colony_screen_draw_people(view, colony, units, col1, font, framebuffer);
    colony_screen_draw_transports(view, units, font, framebuffer);
    colony_screen_draw_multifunction(view, pool, colony, units, font, framebuffer);
  }

  if (colony) {
    colony_screen_draw_cargo_strip(view, colony, font, framebuffer);
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

