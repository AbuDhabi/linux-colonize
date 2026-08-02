#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  view->construction_open = false;
  view->construction_selection = 0;
  view->buildable_count = 0;
  view->jobs_open = false;
  view->jobs_tile_index = -1;
  view->jobs_selection = 0;
  view->job_count = 0;
  view->last_delta_valid = false;
  memset(&view->last_delta, 0, sizeof(view->last_delta));
  view->transport_unit_id = -1;
  view->docked_transport_count = 0;
  memset(view->docked_transport_ids, 0, sizeof(view->docked_transport_ids));
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
    /* Skip passengers — transports must be on-map. */
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

static void colony_screen_draw_field_workers(
  const ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!colony || !framebuffer) {
    return;
  }
  int origin_x = 0;
  int origin_y = 0;
  colony_screen_minimap_origin(&origin_x, &origin_y);
  const int half = COLONY_MINIMAP_GRID / 2;
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
    const bool sel = view && view->selected_colonist == who;
    colony_screen_fill_rect(
      framebuffer, tile_x + 1, tile_y + 1, tile_x + 6, tile_y + 6, sel ? 15 : 14
    );
    if (font && map) {
      const int yld = colony_yield_for_tile(map, colony->x + dx, colony->y + dy, c->field_job);
      char num[8];
      snprintf(num, sizeof(num), "%d", yld);
      font_draw_text(font, framebuffer, tile_x + 7, tile_y + 4, num, 15);
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
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
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
  }

  /*
   * Bottom-right stack (DOS colony collage):
   *   coast / docks / drydock / shipyard (75×48) above
   *   fence / stockade / fort / fortress (73×18) below, right-aligned
   */
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
}

/* Tiny +N craft output under settlement craft slots (from last production delta). */
static void colony_screen_draw_craft_deltas(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->last_delta_valid || !font || !framebuffer) {
    return;
  }
  typedef struct {
    const char* const* chain;
    int out_cargo;
  } CraftLabel;
  static const CraftLabel k_labels[] = {
    {k_slot_rum, COLONIZE_CARGO_RUM},
    {k_slot_tobacco, COLONIZE_CARGO_CIGARS},
    {k_slot_weaver, COLONIZE_CARGO_CLOTH},
    {k_slot_fur, COLONIZE_CARGO_COATS},
    {k_slot_blacksmith, COLONIZE_CARGO_TOOLS},
    {k_slot_armory, COLONIZE_CARGO_MUSKETS},
  };
  for (size_t li = 0; li < sizeof(k_labels) / sizeof(k_labels[0]); ++li) {
    const int g = view->last_delta.goods[k_labels[li].out_cargo];
    if (g <= 0) {
      continue;
    }
    for (int i = 0; i < k_building_slot_count; ++i) {
      if (k_building_slots[i].chain != k_labels[li].chain) {
        continue;
      }
      char buf[12];
      snprintf(buf, sizeof(buf), "+%d", g);
      font_draw_text(
        font,
        framebuffer,
        COLONY_VIEWPORT_X + k_building_slots[i].x + 2,
        COLONY_VIEWPORT_Y + k_building_slots[i].y + COLONY_BUILDING_SLOT_H - 8,
        buf,
        10
      );
      break;
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
  if (!view || !units || !framebuffer) {
    return;
  }
  if (font && view->docked_transport_count > 0) {
    font_draw_text(font, framebuffer, COLONY_TRANSPORT_X, COLONY_BOTTOM_PANEL_Y + 4, "Ship", 15);
  }
  for (int i = 0; i < view->docked_transport_count; ++i) {
    const ColonizeUnit* u = units_get_const(units, view->docked_transport_ids[i]);
    if (!u) {
      continue;
    }
    const ColonizeUnitType* type = units_type(units, u->type_index);
    const int x = COLONY_TRANSPORT_X + i * COLONY_TRANSPORT_PITCH;
    const int y = COLONY_TRANSPORT_Y;
    const bool sel = view->transport_unit_id == u->id;
    if (sel) {
      colony_screen_fill_rect(framebuffer, x - 1, y - 1, x + 17, y + 17, 10);
    }
    if (type && view->icons_ok && type->icon_sprite >= 0 &&
        type->icon_sprite < view->icons.sprite_count) {
      ss_blit_sprite(&view->icons, type->icon_sprite, framebuffer, x, y);
    } else if (font) {
      font_draw_text(font, framebuffer, x, y + 4, "?", 15);
    }
  }

  if (view->transport_unit_id < 0) {
    return;
  }
  const ColonizeUnit* ship = units_get_const(units, view->transport_unit_id);
  if (!ship) {
    return;
  }
  const int holds = units_goods_hold_count(units, view->transport_unit_id);
  for (int i = 0; i < holds; ++i) {
    const int x = COLONY_HOLD_X + i * COLONY_HOLD_PITCH;
    const int y = COLONY_HOLD_Y;
    const int amt = ship->hold_goods_amount[i];
    const int gtype = ship->hold_goods_type[i];
    if (amt > 0 && amt < 255 && gtype >= 0 && gtype < COLONIZE_CARGO_COUNT) {
      const int sprite = COLONY_CARGO_ICON_BASE + gtype;
      if (view->icons_ok && sprite < view->icons.sprite_count) {
        ss_blit_sprite(&view->icons, sprite, framebuffer, x, y);
      } else {
        colony_screen_draw_empty_hold(framebuffer, x, y, COLONY_HOLD_W, COLONY_HOLD_H);
      }
    } else {
      colony_screen_draw_empty_hold(framebuffer, x, y, COLONY_HOLD_W, COLONY_HOLD_H);
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
    snprintf(
      line,
      sizeof(line),
      "Build: %s %d/%d  [3]",
      bt ? bt->name : "?",
      colony->hammers,
      need
    );
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
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->construction_open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int rows = view->buildable_count + 1;
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
    char label[48];
    if (i == 0) {
      snprintf(label, sizeof(label), "Clear project");
    } else {
      const int bid = view->buildable_ids[i - 1];
      const ColonizeBuildingType* bt = colonies_building_type(pool, bid);
      snprintf(
        label,
        sizeof(label),
        "%s (%dH)",
        bt ? bt->name : "?",
        bt ? bt->hammers : 0
      );
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
      const int idx = (my - view->construction_list_y0) / view->construction_line_h;
      const int rows = view->buildable_count + 1;
      if (idx >= 0 && idx < rows) {
        hit.kind = (idx == 0) ? COLONY_HIT_CONSTRUCTION_CLEAR : COLONY_HIT_CONSTRUCTION_ROW;
        hit.index = (idx == 0) ? -1 : (idx - 1);
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
    if (mx >= COLONY_HOLD_X && holds > 0) {
      const int idx = (mx - COLONY_HOLD_X) / COLONY_HOLD_PITCH;
      if (idx >= 0 && idx < holds && mx < COLONY_HOLD_X + idx * COLONY_HOLD_PITCH + COLONY_HOLD_W) {
        hit.kind = COLONY_HIT_HOLD;
        hit.index = idx;
        return hit;
      }
    }
  }

  /* Docked transport icons. */
  if (view->docked_transport_count > 0 && my >= COLONY_TRANSPORT_Y &&
      my < COLONY_TRANSPORT_Y + 16 && mx >= COLONY_TRANSPORT_X) {
    const int idx = (mx - COLONY_TRANSPORT_X) / COLONY_TRANSPORT_PITCH;
    if (idx >= 0 && idx < view->docked_transport_count &&
        mx < COLONY_TRANSPORT_X + idx * COLONY_TRANSPORT_PITCH + 16) {
      hit.kind = COLONY_HIT_TRANSPORT;
      hit.index = idx;
      return hit;
    }
  }

  if (my >= COLONY_CONSTRUCTION_BANNER_Y &&
      my < COLONY_CONSTRUCTION_BANNER_Y + COLONY_CONSTRUCTION_BANNER_H &&
      mx >= COLONY_VIEWPORT_X && mx < COLONY_VIEWPORT_X + COLONY_VIEWPORT_W) {
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

  if (colony->colonist_count > 0 && my >= COLONY_COLONIST_LIST_Y0) {
    const int cargo_limit = COLONY_CARGO_STRIP_Y - 2;
    if (my < cargo_limit && mx >= COLONY_COLONIST_LIST_X && mx < 200) {
      const int idx = (my - COLONY_COLONIST_LIST_Y0) / COLONY_COLONIST_ROW_H;
      if (idx >= 0 && idx < colony->colonist_count && colony->colonists[idx].active) {
        hit.kind = COLONY_HIT_COLONIST;
        hit.index = idx;
        return hit;
      }
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

  colony_screen_draw_top_bar(colony, game_year, game_autumn, gold, font, framebuffer);

  colony_screen_fill_parch(view, framebuffer);
  {
    const bool coastal =
      colony && map && map_tile_is_coastal(map, colony->x, colony->y);
    colony_screen_blit_buildings(view, pool, colony, coastal, framebuffer);
    colony_screen_draw_craft_deltas(view, font, framebuffer);
  }

  colony_screen_fill_wood_tile(view, framebuffer);
  if (colony && map && terrain) {
    colony_screen_render_minimap(map, terrain, phys0, colony->x, colony->y, framebuffer);
    colony_screen_draw_field_workers(view, colony, map, font, framebuffer);
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

  colony_screen_draw_construction_banner(view, pool, colony, font, framebuffer);

  if (view && colony && units) {
    colony_screen_refresh_transports(view, units, colony);
    colony_screen_draw_transports(view, units, font, framebuffer);
  }

  if (colony) {
    colony_screen_draw_cargo_strip(view, colony, font, framebuffer);
  }

  if (colony && font) {
    char line[96];
    const int cargo_limit = COLONY_CARGO_STRIP_Y - 2;
    font_draw_text(font, framebuffer, 8, COLONY_BOTTOM_PANEL_Y + 4, "Colonists", 15);
    int y = COLONY_COLONIST_LIST_Y0;
    for (int i = 0; i < colony->colonist_count && y + 8 < cargo_limit; ++i) {
      const ColonizeColonist* c = &colony->colonists[i];
      if (!c->active) {
        continue;
      }
      const char* uname = "Colonist";
      if (units) {
        const ColonizeUnitType* ut = units_type(units, c->unit_type_index);
        if (ut) {
          uname = ut->name;
        }
      }
      const char* place = "Idle";
      if (c->field_job >= 0) {
        place = colony_yield_job_name(c->field_job);
      } else if (pool && c->building_type >= 0) {
        const ColonizeBuildingType* bt = colonies_building_type(pool, c->building_type);
        if (bt) {
          place = bt->name;
        }
      }
      snprintf(line, sizeof(line), "%d. %s @ %s", i + 1, uname, place);
      const bool sel = view && view->selected_colonist == i;
      if (sel) {
        colony_screen_fill_rect(framebuffer, 6, y - 1, 200, y + COLONY_COLONIST_ROW_H - 1, 138);
      }
      font_draw_text(font, framebuffer, 8, y, line, sel ? 15 : 12);
      y += COLONY_COLONIST_ROW_H;
    }
  }

  if (view && view->construction_open) {
    colony_screen_draw_construction_popup(view, pool, font, framebuffer);
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
