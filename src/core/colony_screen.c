#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/colony_preview.h"
#include "core/colony_yield.h"
#include "core/turn.h"
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
  view->last_delta_valid = false;
  memset(&view->last_delta, 0, sizeof(view->last_delta));
  view->preview_valid = false;
  memset(&view->preview, 0, sizeof(view->preview));
  view->transport_unit_id = -1;
  view->docked_transport_count = 0;
  memset(view->docked_transport_ids, 0, sizeof(view->docked_transport_ids));
  view->outside_unit_count = 0;
  memset(view->outside_unit_ids, 0, sizeof(view->outside_unit_ids));
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
}

void colony_screen_refresh_preview(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map
) {
  if (!view) {
    return;
  }
  if (!pool || !colony) {
    view->preview_valid = false;
    return;
  }
  colony_preview_compute(pool, colony, map, &view->preview);
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
  int colony_id
) {
  if (!view) {
    return;
  }
  colony_screen_close_jobs(view);
  view->buildable_count =
    colonies_list_buildable(pool, colony_id, view->buildable_ids, COLONY_BUILDABLE_MAX);
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
      ss_blit_sprite(sheet, 0, framebuffer, x, y);
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
    COLONY_VIEWPORT_W,
    COLONY_VIEWPORT_H,
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
  char line[96];
  const char* name = (colony && colony->name[0]) ? colony->name : "Colony";
  char date[32];
  turn_format_date(game_year, game_autumn, date, sizeof(date));
  snprintf(line, sizeof(line), "%s", name);
  font_draw_text(font, framebuffer, 4, 2, line, 15);
  snprintf(line, sizeof(line), "%s", date);
  font_draw_text(font, framebuffer, 120, 2, line, 15);
  snprintf(line, sizeof(line), "Gold %d$", gold);
  font_draw_text(font, framebuffer, 240, 2, line, 15);
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
  ss_blit_sprite(&view->icons, sprite, framebuffer, x, y);
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

static void colony_screen_draw_area_overlays(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  (void)units;
  if (!view || !colony || !map || !framebuffer) {
    return;
  }
  int origin_x = 0;
  int origin_y = 0;
  colony_screen_minimap_origin(&origin_x, &origin_y);
  const int half = COLONY_MINIMAP_GRID / 2;

  /* Center settlement icon. */
  {
    const int tile_x = origin_x + half * COLONY_MINIMAP_TILE;
    const int tile_y = origin_y + half * COLONY_MINIMAP_TILE;
    const int icon = colonies_settlement_icon(pool, colony);
    if (view->icons_ok && icon >= 0 && icon < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[icon];
      const int px = tile_x + (COLONY_MINIMAP_TILE - sp->width) / 2;
      const int py = tile_y + (COLONY_MINIMAP_TILE - sp->height) / 2;
      ss_blit_sprite(&view->icons, icon, framebuffer, px, py);
    }
    /* Up to two auto-yield cargo rows above the settlement. */
    const int j0 = colony_preview_best_job(map, colony->x, colony->y);
    int row = 0;
    if (j0 >= 0) {
      const int cargo = colony_yield_job_cargo(j0);
      const int yld = colony_yield_for_tile(map, colony->x, colony->y, j0);
      if (cargo >= 0 && yld > 0) {
        colony_screen_blit_cargo(view, cargo, false, framebuffer, tile_x + 1, tile_y - 10);
        if (view->show_production_numbers && font) {
          char num[8];
          snprintf(num, sizeof(num), "%d", yld);
          font_draw_text(font, framebuffer, tile_x + 10, tile_y - 8, num, 15);
        }
        row = 1;
      }
      const int j1 = colony_preview_second_job(map, colony->x, colony->y, j0);
      if (j1 >= 0) {
        const int cargo1 = colony_yield_job_cargo(j1);
        const int yld1 = colony_yield_for_tile(map, colony->x, colony->y, j1);
        if (cargo1 >= 0 && yld1 > 0) {
          colony_screen_blit_cargo(
            view, cargo1, false, framebuffer, tile_x + 1, tile_y - 10 - row * 10
          );
          if (view->show_production_numbers && font) {
            char num[8];
            snprintf(num, sizeof(num), "%d", yld1);
            font_draw_text(font, framebuffer, tile_x + 10, tile_y - 8 - row * 10, num, 15);
          }
        }
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
    const int tile_x = origin_x + (dx + half) * COLONY_MINIMAP_TILE;
    const int tile_y = origin_y + (dy + half) * COLONY_MINIMAP_TILE;
    const int cargo = colony_yield_job_cargo(c->field_job);
    const int yld = colony_yield_for_tile(map, colony->x + dx, colony->y + dy, c->field_job);
    if (cargo >= 0 && yld > 0) {
      colony_screen_blit_cargo(view, cargo, false, framebuffer, tile_x + 4, tile_y + 1);
      if (view->show_production_numbers && font) {
        char num[8];
        snprintf(num, sizeof(num), "%d", yld);
        font_draw_text(font, framebuffer, tile_x + 4, tile_y + 1, num, 15);
      }
    }
    /* Colonist icon in front of resources. */
    if (units) {
      const ColonizeUnitType* ut = units_type(units, c->unit_type_index);
      if (ut && ut->icon_sprite >= 0) {
        const int ix = tile_x + 2;
        const int iy = tile_y + 4;
        colony_screen_blit_icon(view, ut->icon_sprite, framebuffer, ix, iy);
        if (view->selected_colonist == who) {
          colony_screen_draw_selection_box(framebuffer, ix - 1, iy - 1, 14, 14, 10);
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

  for (int dy = -half; dy <= half; ++dy) {
    for (int dx = -half; dx <= half; ++dx) {
      const int mx = colony_x + dx;
      const int my = colony_y + dy;
      const int tile_x = origin_x + (dx + half) * COLONY_MINIMAP_TILE;
      const int tile_y = origin_y + (dy + half) * COLONY_MINIMAP_TILE;
      const int underlayer = map_coast_underlayer_sprite_at(map, mx, my);
      const int coast_layers = map_phys0_coast_layer_count(map, mx, my);
      const int sprite = (underlayer >= 0) ? underlayer : map_terrain_sprite_at(map, mx, my);
      if (sprite >= 0 && sprite < terrain->sprite_count) {
        ss_blit_sprite(terrain, sprite, framebuffer, tile_x, tile_y);
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
            ss_blit_sprite(phys0, mask, framebuffer, tile_x, tile_y);
          }
          if (fill >= 0 && fill < terrain->sprite_count) {
            ss_blit_sprite_where_dest(terrain, fill, framebuffer, tile_x, tile_y, 0);
          }
        }
      }
      const int forest = map_phys0_forest_sprite_at(map, mx, my);
      if (forest >= 0 && forest < phys0->sprite_count) {
        ss_blit_sprite(phys0, forest, framebuffer, tile_x, tile_y);
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
        ss_blit_sprite(phys0, overlay, framebuffer, tile_x + ox, tile_y + oy);
      }
      if (underlayer >= 0) {
        const int ocean_sprite = map_terrain_sprite_at(map, mx, my);
        if (ocean_sprite >= 0 && ocean_sprite < terrain->sprite_count) {
          ss_blit_sprite_where_dest(terrain, ocean_sprite, framebuffer, tile_x, tile_y, 0);
        }
        for (int layer = coast_layers; layer < layers; ++layer) {
          const int overlay = map_phys0_overlay_sprite_at(map, mx, my, layer);
          if (overlay < 0 || overlay >= phys0->sprite_count) {
            continue;
          }
          int ox = 0;
          int oy = 0;
          map_phys0_overlay_offset_at(map, mx, my, layer, &ox, &oy);
          ss_blit_sprite(phys0, overlay, framebuffer, tile_x + ox, tile_y + oy);
        }
      }
    }
  }
}

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
  COLONY_COAST_H = 48
};

typedef struct ColonyBuildingSlot {
  const char* const* chain;
  int tree_sprite;
  int x;
  int y;
} ColonyBuildingSlot;

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

static const ColonyBuildingSlot k_building_slots[] = {
  {k_slot_town_hall, COLONY_TREE_LARGE, 70, 4},
  {k_slot_church, COLONY_TREE_LARGE, 8, 4},
  {k_slot_school, COLONY_TREE_MED, 130, 8},
  {k_slot_carpenter, COLONY_TREE_MED, 8, 44},
  {k_slot_blacksmith, COLONY_TREE_SMALL, 56, 42},
  {k_slot_weaver, COLONY_TREE_SMALL, 88, 42},
  {k_slot_tobacco, COLONY_TREE_SMALL, 120, 42},
  {k_slot_rum, COLONY_TREE_SMALL, 152, 42},
  {k_slot_fur, COLONY_TREE_SMALL, 56, 72},
  {k_slot_warehouse, COLONY_TREE_MED, 88, 72},
  {k_slot_armory, COLONY_TREE_MED, 140, 72},
  {k_slot_press, COLONY_TREE_SMALL, 8, 72},
  {k_slot_stable, COLONY_TREE_SMALL, 8, 100},
  {k_slot_custom, COLONY_TREE_SMALL, 40, 100},
};
static const int k_building_slot_count =
  (int)(sizeof(k_building_slots) / sizeof(k_building_slots[0]));

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

static void colony_screen_blit_buildings(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  bool coastal,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->buildings_ok || !pool || !colony || !framebuffer) {
    return;
  }

  const int slot_ox = COLONY_VIEWPORT_X;
  const int slot_oy = COLONY_VIEWPORT_Y;
  for (int i = 0; i < k_building_slot_count; ++i) {
    const ColonyBuildingSlot* slot = &k_building_slots[i];
    size_t n = 0;
    while (slot->chain && slot->chain[n]) {
      ++n;
    }
    const int built = colony_screen_best_built(pool, colony, slot->chain, n);
    if (built >= 0) {
      colony_screen_blit_slot(view, built, slot_ox + slot->x, slot_oy + slot->y, framebuffer);
    } else {
      colony_screen_blit_slot(
        view, slot->tree_sprite, slot_ox + slot->x, slot_oy + slot->y, framebuffer
      );
    }

    /* Workers in this building (up to 3) + production badges. */
    if (built < 0 || !units) {
      continue;
    }
    int workers = 0;
    for (int ci = 0; ci < colony->colonist_count && workers < 3; ++ci) {
      const ColonizeColonist* c = &colony->colonists[ci];
      if (!c->active || c->building_type != built) {
        continue;
      }
      const ColonizeUnitType* ut = units_type(units, c->unit_type_index);
      const int wx = slot_ox + slot->x + 2 + workers * 12;
      const int wy = slot_oy + slot->y + COLONY_BUILDING_SLOT_H - 18;
      if (ut && ut->icon_sprite >= 0) {
        colony_screen_blit_icon(view, ut->icon_sprite, framebuffer, wx, wy);
        if (view->selected_colonist == ci) {
          colony_screen_draw_selection_box(framebuffer, wx - 1, wy - 1, 14, 14, 10);
        }
      }
      /* Production badge above worker. */
      int badge = -1;
      if (strstr(pool->building_types[built].name, "Town Hall") ||
          strstr(pool->building_types[built].name, "Printing") ||
          strstr(pool->building_types[built].name, "Newspaper")) {
        badge = COLONY_ICON_BELL;
      } else if (
        strstr(pool->building_types[built].name, "Church") ||
        strstr(pool->building_types[built].name, "Cathedral")
      ) {
        badge = COLONY_ICON_CROSS;
      } else if (strstr(pool->building_types[built].name, "Carpenter")) {
        badge = COLONY_ICON_HAMMER;
      } else if (strstr(pool->building_types[built].name, "Rum")) {
        badge = COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_RUM;
      } else if (strstr(pool->building_types[built].name, "Tobacconist")) {
        badge = COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_CIGARS;
      } else if (
        strstr(pool->building_types[built].name, "Weaver") ||
        strstr(pool->building_types[built].name, "Textile")
      ) {
        badge = COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_CLOTH;
      } else if (strstr(pool->building_types[built].name, "Fur")) {
        badge = COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_COATS;
      } else if (
        strstr(pool->building_types[built].name, "Blacksmith") ||
        strstr(pool->building_types[built].name, "Iron")
      ) {
        badge = COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_TOOLS;
      } else if (
        strstr(pool->building_types[built].name, "Armory") ||
        strstr(pool->building_types[built].name, "Magazine") ||
        strstr(pool->building_types[built].name, "Arsenal")
      ) {
        badge = COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_MUSKETS;
      }
      if (badge >= 0) {
        colony_screen_blit_icon(view, badge, framebuffer, wx + 2, wy - 10);
      }
      workers++;
    }
  }

  const int fence_x = COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - COLONY_FENCE_W;
  const int fence_y = COLONY_VIEWPORT_Y + COLONY_VIEWPORT_H - COLONY_FENCE_H;
  const int coast_x = COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - COLONY_COAST_W;
  const int coast_y = fence_y - COLONY_COAST_H;

  const int docks = colony_screen_best_built(pool, colony, k_slot_docks, 3);
  if (docks >= 0) {
    colony_screen_blit_slot(view, docks, coast_x, coast_y, framebuffer);
  } else if (coastal) {
    colony_screen_blit_slot(view, COLONY_COAST_PLACEHOLDER, coast_x, coast_y, framebuffer);
  }

  const int fort = colony_screen_best_built(pool, colony, k_slot_stockade, 3);
  if (fort >= 0) {
    colony_screen_blit_slot(view, fort, fence_x, fence_y, framebuffer);
  } else {
    colony_screen_blit_slot(view, COLONY_FENCE_SPRITE, fence_x, fence_y, framebuffer);
  }

  /* Outside units in front of fortification. */
  if (units) {
    for (int i = 0; i < view->outside_unit_count; ++i) {
      const ColonizeUnit* u = units_get_const(units, view->outside_unit_ids[i]);
      if (!u) {
        continue;
      }
      const ColonizeUnitType* ut = units_type(units, u->type_index);
      const int ux = fence_x + 4 + i * 14;
      const int uy = fence_y - 2;
      if (ut && ut->icon_sprite >= 0) {
        colony_screen_blit_icon(view, ut->icon_sprite, framebuffer, ux, uy);
        if (view->selected_outside_unit == u->id) {
          colony_screen_draw_selection_box(framebuffer, ux - 1, uy - 1, 14, 14, 10);
        }
      }
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

  for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
    const int slot_x = COLONY_CARGO_SLOT_X0 + i * COLONY_CARGO_PITCH;
    const int sprite = COLONY_CARGO_ICON_BASE + i;
    if (view && view->icons_ok && sprite < view->icons.sprite_count) {
      const ColonizeSprite* spr = &view->icons.sprites[sprite];
      const int icon_x = slot_x + (COLONY_CARGO_SLOT_W - spr->width) / 2;
      ss_blit_sprite(&view->icons, sprite, framebuffer, icon_x, COLONY_CARGO_STRIP_Y);
    }

    if (font) {
      char amount[16];
      int delta = 0;
      if (view && view->last_delta_valid) {
        delta = view->last_delta.goods[i];
      }
      if (delta != 0) {
        snprintf(amount, sizeof(amount), "%d%+d", colony->stock[i], delta);
      } else {
        snprintf(amount, sizeof(amount), "%d", colony->stock[i]);
      }
      const int tw = colony_screen_text_width(font, amount);
      const int tx = slot_x + (COLONY_CARGO_SLOT_W - tw) / 2;
      const uint8_t col = delta > 0 ? 10 : (delta < 0 ? 12 : 15);
      font_draw_text(font, framebuffer, tx, COLONY_CARGO_NUM_Y, amount, col);
    }
  }
}

static void colony_screen_draw_empty_hold(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h
) {
  if (!framebuffer) {
    return;
  }
  colony_screen_fill_rect(framebuffer, x, y, x + w, y + 1, 0);
  colony_screen_fill_rect(framebuffer, x, y + h - 1, x + w, y + h, 0);
  colony_screen_fill_rect(framebuffer, x, y, x + 1, y + h, 0);
  colony_screen_fill_rect(framebuffer, x + w - 1, y, x + w, y + h, 0);
  colony_screen_fill_rect(framebuffer, x + 1, y + 1, x + w - 1, y + h - 1, 138);
}

static void colony_screen_draw_transports(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  (void)font;
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
    const bool sel = view->transport_unit_id == u->id;
    if (sel) {
      colony_screen_draw_selection_box(framebuffer, x - 1, y - 1, 16, 16, 10);
    }
    if (type && type->icon_sprite >= 0) {
      colony_screen_blit_icon(view, type->icon_sprite, framebuffer, x, y);
    }
  }

  if (view->transport_unit_id < 0) {
    return;
  }
  const ColonizeUnit* ship = units_get_const(units, view->transport_unit_id);
  if (!ship) {
    return;
  }
  const int goods_holds = units_goods_hold_count(units, view->transport_unit_id);
  for (int i = 0; i < goods_holds; ++i) {
    const int x = COLONY_HOLD_X + 4 + i * COLONY_HOLD_PITCH;
    const int y = COLONY_HOLD_Y;
    const int amt = ship->hold_goods_amount[i];
    const int gtype = ship->hold_goods_type[i];
    if (amt > 0 && amt < 255 && gtype >= 0 && gtype < COLONIZE_CARGO_COUNT) {
      const bool partial = amt < 100;
      colony_screen_blit_cargo(view, gtype, partial, framebuffer, x, y);
    } else {
      colony_screen_draw_empty_hold(framebuffer, x, y, COLONY_HOLD_W, COLONY_HOLD_H);
    }
  }
  /* Passengers drawn after goods holds. */
  for (int i = 0; i < ship->cargo_count; ++i) {
    const int x = COLONY_HOLD_X + 4 + (goods_holds + i) * COLONY_HOLD_PITCH;
    const int y = COLONY_HOLD_Y;
    const ColonizeUnit* pass = units_get_const(units, ship->cargo_ids[i]);
    if (!pass) {
      continue;
    }
    const ColonizeUnitType* pt = units_type(units, pass->type_index);
    if (pt && pt->icon_sprite >= 0) {
      colony_screen_blit_icon(view, pt->icon_sprite, framebuffer, x, y);
    }
  }
}

static int colony_screen_sol_percent(const ColonizeCol1Save* col1, const ColonizeColony* colony) {
  if (!col1 || !colony || !col1->colony) {
    return 0;
  }
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &col1->colony[i];
    if ((int)c->x == colony->x && (int)c->y == colony->y) {
      if (c->rebel_divisor == 0) {
        return 0;
      }
      return (int)((c->rebel_dividend * 100u) / c->rebel_divisor);
    }
  }
  return 0;
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
  colony_screen_blit_icon(view, COLONY_ICON_FLAG, framebuffer, COLONY_PEOPLE_X + 2, COLONY_PANEL_CONTENT_Y);
  colony_screen_blit_icon(
    view, COLONY_ICON_CROWN, framebuffer, COLONY_PEOPLE_X + COLONY_PEOPLE_W - 16, COLONY_PANEL_CONTENT_Y
  );
  if (font) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", sol);
    font_draw_text(font, framebuffer, COLONY_PEOPLE_X + 16, COLONY_PANEL_CONTENT_Y + 2, buf, 15);
    snprintf(buf, sizeof(buf), "%d%%", tory);
    font_draw_text(
      font, framebuffer, COLONY_PEOPLE_X + COLONY_PEOPLE_W - 40, COLONY_PANEL_CONTENT_Y + 2, buf, 15
    );
  }

  int x = COLONY_PEOPLE_X + 2;
  const int y_people = COLONY_PANEL_CONTENT_Y + 16;
  for (int i = 0; i < colony->colonist_count; ++i) {
    const ColonizeColonist* c = &colony->colonists[i];
    if (!c->active) {
      continue;
    }
    const ColonizeUnitType* ut = units ? units_type(units, c->unit_type_index) : NULL;
    if (ut && ut->icon_sprite >= 0) {
      colony_screen_blit_icon(view, ut->icon_sprite, framebuffer, x, y_people);
      if (view->selected_colonist == i) {
        colony_screen_draw_selection_box(framebuffer, x - 1, y_people - 1, 14, 14, 10);
      }
    }
    x += 12;
    if (x > COLONY_PEOPLE_X + COLONY_PEOPLE_W - 14) {
      break;
    }
  }

  x = COLONY_PEOPLE_X + 2;
  const int y_out = y_people + 16;
  for (int i = 0; i < view->outside_unit_count; ++i) {
    const ColonizeUnit* u = units_get_const(units, view->outside_unit_ids[i]);
    if (!u) {
      continue;
    }
    const ColonizeUnitType* ut = units_type(units, u->type_index);
    if (ut && ut->icon_sprite >= 0) {
      colony_screen_blit_icon(view, ut->icon_sprite, framebuffer, x, y_out);
      if (view->selected_outside_unit == u->id) {
        colony_screen_draw_selection_box(framebuffer, x - 1, y_out - 1, 14, 14, 10);
      }
    }
    x += 12;
    if (x > COLONY_PEOPLE_X + COLONY_PEOPLE_W - 14) {
      break;
    }
  }

  if (!view->preview_valid) {
    return;
  }
  const ColonizeColonyPreview* p = &view->preview;
  int meter_y = COLONY_CARGO_STRIP_Y - 14;
  int mx = COLONY_PEOPLE_X + 2;
  const int food_show = p->food_produced > 8 ? 8 : p->food_produced;
  for (int i = 0; i < food_show; ++i) {
    colony_screen_blit_cargo(view, COLONIZE_CARGO_FOOD, false, framebuffer, mx, meter_y);
    mx += 8;
  }
  if (p->food_net > 0) {
    mx += 4;
    const int excess = p->food_net > 4 ? 4 : p->food_net;
    for (int i = 0; i < excess; ++i) {
      colony_screen_blit_cargo(view, COLONIZE_CARGO_FOOD, false, framebuffer, mx, meter_y);
      mx += 8;
    }
  } else if (p->food_net < 0) {
    mx += 4;
    const int shortf = (-p->food_net) > 4 ? 4 : -p->food_net;
    for (int i = 0; i < shortf; ++i) {
      colony_screen_blit_cargo(view, COLONIZE_CARGO_FOOD, true, framebuffer, mx, meter_y);
      mx += 8;
    }
  }
  mx = COLONY_PEOPLE_X + 2;
  meter_y -= 12;
  const int crosses = p->crosses > 6 ? 6 : p->crosses;
  for (int i = 0; i < crosses; ++i) {
    colony_screen_blit_icon(view, COLONY_ICON_CROSS, framebuffer, mx, meter_y);
    mx += 8;
  }
  mx = COLONY_PEOPLE_X + 50;
  const int bells = p->bells > 6 ? 6 : p->bells;
  for (int i = 0; i < bells; ++i) {
    colony_screen_blit_icon(view, COLONY_ICON_BELL, framebuffer, mx, meter_y);
    mx += 8;
  }
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
  /* Mode buttons. */
  colony_screen_blit_icon(
    view, 3, framebuffer, COLONY_MULTI_BTN_X, COLONY_PANEL_CONTENT_Y
  ); /* unfortified colony as "house" */
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
  int py = COLONY_PANEL_CONTENT_Y;
  if (view->multi_mode == COLONY_MULTI_PRODUCTION && view->preview_valid) {
    const ColonizeColonyPreview* p = &view->preview;
    int x = px;
    int y = py;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      if (p->goods[c] <= 0 && p->shortfall[c] <= 0) {
        continue;
      }
      if (p->goods[c] > 0) {
        colony_screen_blit_cargo(view, c, false, framebuffer, x, y);
        if (view->show_production_numbers && font) {
          char num[8];
          snprintf(num, sizeof(num), "%d", p->goods[c]);
          font_draw_text(font, framebuffer, x + 8, y, num, 15);
        }
        x += 16;
      }
      if (p->shortfall[c] > 0) {
        colony_screen_blit_cargo(view, c, true, framebuffer, x, y);
        x += 16;
      }
      if (x > COLONY_MULTI_X + COLONY_MULTI_W - 20) {
        x = px;
        y += 14;
      }
    }
    if (p->hammers > 0) {
      colony_screen_blit_icon(view, COLONY_ICON_HAMMER, framebuffer, px, y + 16);
      if (font) {
        char num[12];
        snprintf(num, sizeof(num), "%d", p->hammers);
        font_draw_text(font, framebuffer, px + 12, y + 18, num, 15);
      }
    }
  } else if (view->multi_mode == COLONY_MULTI_UNITS && units) {
    int x = px;
    int y = py;
    for (int i = 0; i < view->outside_unit_count; ++i) {
      const ColonizeUnit* u = units_get_const(units, view->outside_unit_ids[i]);
      if (!u) {
        continue;
      }
      const ColonizeUnitType* ut = units_type(units, u->type_index);
      if (ut && ut->icon_sprite >= 0) {
        colony_screen_blit_icon(view, ut->icon_sprite, framebuffer, x, y);
        if (view->selected_outside_unit == u->id) {
          colony_screen_draw_selection_box(framebuffer, x - 1, y - 1, 14, 14, 10);
        }
      }
      x += 14;
      if (x > COLONY_MULTI_X + COLONY_MULTI_W - 16) {
        x = px;
        y += 16;
      }
    }
  } else if (view->multi_mode == COLONY_MULTI_CONSTRUCTION && colony && pool && font) {
    char line[64];
    if (colony->building_in_production >= 0) {
      const ColonizeBuildingType* bt =
        colonies_building_type(pool, colony->building_in_production);
      snprintf(
        line,
        sizeof(line),
        "%s",
        bt ? bt->name : "?"
      );
      font_draw_text(font, framebuffer, px, py, line, 15);
      snprintf(line, sizeof(line), "%d/%d H", colony->hammers, bt ? bt->hammers : 0);
      font_draw_text(font, framebuffer, px, py + 10, line, 15);
      if (bt && bt->tools_cost > 0) {
        snprintf(line, sizeof(line), "%d tools", bt->tools_cost);
        font_draw_text(font, framebuffer, px, py + 20, line, 15);
      }
    } else {
      font_draw_text(font, framebuffer, px, py, "(none)", 15);
    }
    font_draw_text(font, framebuffer, px, py + 32, "Change [C]", 14);
    font_draw_text(font, framebuffer, px, py + 42, "Buy [B]", 14);
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

static void colony_screen_draw_construction_banner(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!colony || !font || !framebuffer) {
    return;
  }
  char line[80];
  if (colony->building_in_production >= 0 && pool) {
    const ColonizeBuildingType* bt =
      colonies_building_type(pool, colony->building_in_production);
    const int need = bt ? bt->hammers : 0;
    const int gold = colonies_construction_gold_cost(pool, colony);
    const int tools = bt ? bt->tools_cost : 0;
    if (gold > 0) {
      snprintf(
        line,
        sizeof(line),
        "Build: %s %d/%d  $%d  [3]/B",
        bt ? bt->name : "?",
        colony->hammers,
        need,
        gold
      );
    } else if (tools > 0) {
      snprintf(
        line,
        sizeof(line),
        "Build: %s %d/%d  %dT  [3]/B",
        bt ? bt->name : "?",
        colony->hammers,
        need,
        tools
      );
    } else {
      snprintf(
        line,
        sizeof(line),
        "Build: %s %d/%d  [3]/B",
        bt ? bt->name : "?",
        colony->hammers,
        need
      );
    }
  } else {
    snprintf(line, sizeof(line), "Build: (none)  [3]");
  }
  if (view && view->last_delta_valid && view->last_delta.hammers_added > 0) {
    char extra[24];
    snprintf(extra, sizeof(extra), " %+dH", view->last_delta.hammers_added);
    size_t n = strlen(line);
    if (n + strlen(extra) < sizeof(line)) {
      memcpy(line + n, extra, strlen(extra) + 1);
    }
  }
  colony_screen_fill_rect(
    framebuffer,
    COLONY_VIEWPORT_X + 2,
    COLONY_CONSTRUCTION_BANNER_Y,
    COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - 2,
    COLONY_CONSTRUCTION_BANNER_Y + COLONY_CONSTRUCTION_BANNER_H,
    4
  );
  font_draw_text(
    font, framebuffer, COLONY_VIEWPORT_X + 4, COLONY_CONSTRUCTION_BANNER_Y + 1, line, 15
  );
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
  const int buy_row = (colony && colony->building_in_production >= 0) ? 1 : 0;
  const int rows = view->buildable_count + 1 + buy_row;
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
    char label[56];
    if (i == 0) {
      snprintf(label, sizeof(label), "Clear project");
    } else if (buy_row && i == 1) {
      const int gold = colonies_construction_gold_cost(pool, colony);
      const ColonizeBuildingType* bt =
        colonies_building_type(pool, colony->building_in_production);
      const int tools = bt ? bt->tools_cost : 0;
      snprintf(label, sizeof(label), "Buy now ($%d, %dT)", gold, tools);
    } else {
      const int bid = view->buildable_ids[i - 1 - buy_row];
      const ColonizeBuildingType* bt = colonies_building_type(pool, bid);
      if (bt && bt->tools_cost > 0) {
        snprintf(
          label, sizeof(label), "%s (%dH, %dT)", bt->name, bt->hammers, bt->tools_cost
        );
      } else {
        snprintf(
          label,
          sizeof(label),
          "%s (%dH)",
          bt ? bt->name : "?",
          bt ? bt->hammers : 0
        );
      }
    }
    if (font) {
      font_draw_text(font, framebuffer, inner_x + pad, row_y + 1, label, 15);
    }
  }
}

static void colony_screen_draw_jobs_popup(
  ColonyScreenView* view,
  const ColonizeWorldMap* map,
  const ColonizeColony* colony,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->jobs_open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int rows = view->job_count + 1;
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

  for (int i = 0; i < rows; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == view->jobs_selection);
    if (selected) {
      colony_screen_fill_rect(
        framebuffer, inner_x + 1, row_y - 1, inner_x + inner_w - 1, row_y + line_h - 1, 138
      );
    }
    char label[48];
    if (i == 0) {
      snprintf(label, sizeof(label), "Clear");
    } else {
      const int job = view->job_ids[i - 1];
      const int yld = (map && colony) ? colony_yield_for_tile(map, tx, ty, job) : 0;
      snprintf(label, sizeof(label), "%s (%d)", colony_yield_job_name(job), yld);
    }
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

  if (view->jobs_open) {
    if (mx < view->jobs_dialog_x || my < view->jobs_dialog_y ||
        mx >= view->jobs_dialog_x + view->jobs_dialog_w ||
        my >= view->jobs_dialog_y + view->jobs_dialog_h) {
      hit.kind = COLONY_HIT_JOBS_OUTSIDE;
      return hit;
    }
    if (view->jobs_line_h > 0 && my >= view->jobs_list_y0) {
      const int idx = (my - view->jobs_list_y0) / view->jobs_line_h;
      const int rows = view->job_count + 1;
      if (idx >= 0 && idx < rows) {
        hit.kind = (idx == 0) ? COLONY_HIT_JOBS_CLEAR : COLONY_HIT_JOBS_ROW;
        hit.index = (idx == 0) ? -1 : (idx - 1);
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
      const int buy_row = (colony->building_in_production >= 0) ? 1 : 0;
      const int idx = (my - view->construction_list_y0) / view->construction_line_h;
      const int rows = view->buildable_count + 1 + buy_row;
      if (idx >= 0 && idx < rows) {
        if (idx == 0) {
          hit.kind = COLONY_HIT_CONSTRUCTION_CLEAR;
          hit.index = -1;
        } else if (buy_row && idx == 1) {
          hit.kind = COLONY_HIT_CONSTRUCTION_BUY;
          hit.index = 0;
        } else {
          hit.kind = COLONY_HIT_CONSTRUCTION_ROW;
          hit.index = idx - 1 - buy_row;
        }
        return hit;
      }
    }
    return hit;
  }

  if (my >= COLONY_BOTTOM_PANEL_Y && mx >= COLONY_EXIT_X) {
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

  /* Multifunction pane / Construction Change click. */
  if (mx >= COLONY_MULTI_X && mx < COLONY_MULTI_BTN_X && my >= COLONY_PANEL_CONTENT_Y &&
      my < COLONY_CARGO_STRIP_Y) {
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

  /* Outside units on fortification strip. */
  if (view->outside_unit_count > 0 && my >= COLONY_VIEWPORT_Y + COLONY_VIEWPORT_H - 20 &&
      my < COLONY_BOTTOM_SEPARATOR_Y && mx >= COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - COLONY_FENCE_W) {
    const int fence_x = COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - COLONY_FENCE_W;
    const int idx = (mx - (fence_x + 4)) / 14;
    if (idx >= 0 && idx < view->outside_unit_count) {
      hit.kind = COLONY_HIT_OUTSIDE_UNIT;
      hit.index = idx;
      return hit;
    }
  }

  if (my >= COLONY_CONSTRUCTION_BANNER_Y &&
      my < COLONY_CONSTRUCTION_BANNER_Y + COLONY_CONSTRUCTION_BANNER_H &&
      mx >= COLONY_VIEWPORT_X && mx < COLONY_VIEWPORT_X + COLONY_VIEWPORT_W &&
      view->multi_mode != COLONY_MULTI_CONSTRUCTION) {
    hit.kind = COLONY_HIT_CONSTRUCTION_BANNER;
    return hit;
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

  /* People-view colonist icons (first row) and outside units (second row). */
  if (my >= COLONY_PANEL_CONTENT_Y + 16 && my < COLONY_PANEL_CONTENT_Y + 32 &&
      mx >= COLONY_PEOPLE_X && mx < COLONY_PEOPLE_X + COLONY_PEOPLE_W) {
    const int idx = (mx - (COLONY_PEOPLE_X + 2)) / 12;
    if (idx >= 0 && idx < colony->colonist_count && colony->colonists[idx].active) {
      hit.kind = COLONY_HIT_PEOPLE_COLONIST;
      hit.index = idx;
      return hit;
    }
  }
  if (view->outside_unit_count > 0 && my >= COLONY_PANEL_CONTENT_Y + 32 &&
      my < COLONY_PANEL_CONTENT_Y + 48 && mx >= COLONY_PEOPLE_X &&
      mx < COLONY_PEOPLE_X + COLONY_PEOPLE_W) {
    const int idx = (mx - (COLONY_PEOPLE_X + 2)) / 12;
    if (idx >= 0 && idx < view->outside_unit_count) {
      hit.kind = COLONY_HIT_OUTSIDE_UNIT;
      hit.index = idx;
      return hit;
    }
  }

  if (pool && mx >= COLONY_VIEWPORT_X && mx < COLONY_VIEWPORT_X + COLONY_VIEWPORT_W &&
      my >= COLONY_VIEWPORT_Y && my < COLONY_BOTTOM_SEPARATOR_Y) {
    const int lx = mx - COLONY_VIEWPORT_X;
    const int ly = my - COLONY_VIEWPORT_Y;
    for (int i = 0; i < k_building_slot_count; ++i) {
      const ColonyBuildingSlot* slot = &k_building_slots[i];
      if (lx >= slot->x && lx < slot->x + COLONY_BUILDING_SLOT_W && ly >= slot->y &&
          ly < slot->y + COLONY_BUILDING_SLOT_H) {
        size_t n = 0;
        while (slot->chain && slot->chain[n]) {
          ++n;
        }
        const int built = colony_screen_best_built(pool, colony, slot->chain, n);
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
    colony_screen_refresh_preview(view, pool, colony, map);
  }

  colony_screen_draw_top_bar(colony, game_year, game_autumn, gold, font, framebuffer);

  colony_screen_fill_parch(view, framebuffer);
  {
    const bool coastal =
      colony && map && map_tile_is_coastal(map, colony->x, colony->y);
    colony_screen_blit_buildings(view, pool, colony, units, coastal, framebuffer);
  }

  colony_screen_fill_wood_tile(view, framebuffer);
  if (colony && map && terrain) {
    colony_screen_render_minimap(map, terrain, phys0, colony->x, colony->y, framebuffer);
    colony_screen_draw_area_overlays(view, pool, colony, units, map, font, framebuffer);
  }

  if (view && view->bottom_panel_ok) {
    pik_blit(&view->bottom_panel, framebuffer, 0, COLONY_BOTTOM_PANEL_Y);
  }

  colony_screen_draw_hline(framebuffer, COLONY_TOP_SEPARATOR_Y, 0);
  colony_screen_draw_hline(framebuffer, COLONY_BOTTOM_SEPARATOR_Y, 0);
  colony_screen_draw_vline(
    framebuffer,
    COLONY_VIEWPORT_X + COLONY_VIEWPORT_W,
    COLONY_MIDDLE_Y,
    COLONY_BOTTOM_SEPARATOR_Y - 1,
    0
  );

  if (view && view->multi_mode != COLONY_MULTI_CONSTRUCTION) {
    colony_screen_draw_construction_banner(view, pool, colony, font, framebuffer);
  }

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
    colony_screen_draw_jobs_popup(view, map, colony, font, framebuffer);
  }

  if (view && font) {
    if (!view->frame_ok) {
      font_draw_text(font, framebuffer, 4, 100, "WOODPANL.PIK failed to load", 12);
    }
    if (!view->buildings_ok) {
      font_draw_text(font, framebuffer, 4, 112, "BUILDING.SS failed to load", 12);
    }
    if (view->status[0]) {
      font_draw_text(font, framebuffer, 160, COLONY_BOTTOM_PANEL_Y + 4, view->status, 12);
    }
  }
}

