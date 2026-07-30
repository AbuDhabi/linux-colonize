#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

void colony_screen_set_status(ColonyScreenView* view, const char* text) {
  if (!view) {
    return;
  }
  snprintf(view->status, sizeof(view->status), "%s", text ? text : "");
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

  if (!colony_screen_load_pik(data_dir, "COLONY.PIK", &view->bottom_panel, err, err_size)) {
    colony_screen_free(view);
    return false;
  }
  view->bottom_panel_ok = true;

  colony_screen_set_status(view, "Colony ready. Esc or C returns to map.");
  diag_info(
    "Colony screen loaded (WOODPANL %dx%d, PARCH %d, WOODTILE %d, BUILDING %d, COLONY.PIK %dx%d)",
    view->frame.width,
    view->frame.height,
    view->parch.sprite_count,
    view->wood_tile.sprite_count,
    view->buildings.sprite_count,
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

  const int grid_px = COLONY_MINIMAP_GRID * COLONY_MINIMAP_TILE;
  const int origin_x =
    COLONY_MINIMAP_SECTION_X + (COLONY_MINIMAP_SECTION_W - grid_px) / 2;
  const int origin_y =
    COLONY_MINIMAP_SECTION_Y + (COLONY_MINIMAP_SECTION_H - grid_px) / 2;
  const int half = COLONY_MINIMAP_GRID / 2;

  for (int dy = -half; dy <= half; ++dy) {
    for (int dx = -half; dx <= half; ++dx) {
      const int mx = colony_x + dx;
      const int my = colony_y + dy;
      const int tile_x = origin_x + (dx + half) * COLONY_MINIMAP_TILE;
      const int tile_y = origin_y + (dy + half) * COLONY_MINIMAP_TILE;
      const int sprite = map_terrain_sprite_at(map, mx, my);
      if (sprite >= 0 && sprite < terrain->sprite_count) {
        ss_blit_sprite(terrain, sprite, framebuffer, tile_x, tile_y);
      }
      if (!phys0) {
        continue;
      }
      const int forest = map_phys0_forest_sprite_at(map, mx, my);
      if (forest >= 0 && forest < phys0->sprite_count) {
        ss_blit_sprite(phys0, forest, framebuffer, tile_x, tile_y);
      }
      const int layers = map_phys0_overlay_count(map, mx, my);
      for (int layer = 0; layer < layers; ++layer) {
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

/*
 * Approximate collage positions inside the PARCH buildings section.
 * Exact DOS placement is not recovered yet; this is a readable bring-up layout.
 */
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

static void colony_screen_blit_buildings(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->buildings_ok || !pool || !colony || !framebuffer) {
    return;
  }

  typedef struct BuildingSlot {
    const char* name;
    int x;
    int y;
  } BuildingSlot;

  static const BuildingSlot k_slots[] = {
    {"Town Hall", 8, 8},
    {"Carpenter's Shop", 70, 8},
    {"Blacksmith's House", 130, 8},
    {"Warehouse", 8, 48},
    {"Weaver's House", 70, 48},
    {"Tobacconist's House", 130, 48},
    {"Rum Distiller's House", 8, 80},
    {"Fur Trader's House", 70, 80},
  };

  for (size_t i = 0; i < sizeof(k_slots) / sizeof(k_slots[0]); ++i) {
    const int idx = colony_screen_find_built(pool, colony, k_slots[i].name);
    if (idx < 0 || idx >= view->buildings.sprite_count) {
      continue;
    }
    const ColonizeSprite* spr = &view->buildings.sprites[idx];
    if (!spr || spr->width <= 2 || spr->height <= 2) {
      continue;
    }
    ss_blit_sprite(&view->buildings, idx, framebuffer, k_slots[i].x, k_slots[i].y);
  }
}

void colony_screen_render(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
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

  /* Beige parchment fills the entire upper-left buildings section. */
  colony_screen_fill_parch(view, framebuffer);
  colony_screen_blit_buildings(view, pool, colony, framebuffer);

  /* WOODTILE fill for the square top-right minimap panel, then 3×3 centered. */
  colony_screen_fill_wood_tile(view, framebuffer);
  if (colony && map && terrain) {
    colony_screen_render_minimap(map, terrain, phys0, colony->x, colony->y, framebuffer);
  }

  if (view && view->bottom_panel_ok) {
    pik_blit(&view->bottom_panel, framebuffer, 0, COLONY_BOTTOM_PANEL_Y);
  }

  if (colony && font) {
    char line[96];
    snprintf(line, sizeof(line), "%s", colony->name);
    font_draw_text(font, framebuffer, 8, 8, line, 15);

    snprintf(line, sizeof(line), "Pop %d", colony->population);
    font_draw_text(font, framebuffer, 8, 18, line, 14);

    snprintf(
      line,
      sizeof(line),
      "Food %d  Tools %d  Guns %d  Horses %d",
      colony->stock_food,
      colony->stock_tools,
      colony->stock_muskets,
      colony->stock_horses
    );
    font_draw_text(font, framebuffer, 8, COLONY_BOTTOM_PANEL_Y + 4, line, 14);

    int y = COLONY_BOTTOM_PANEL_Y + 16;
    font_draw_text(font, framebuffer, 8, y, "Colonists", 15);
    y += 10;
    for (int i = 0; i < colony->colonist_count && y < COLONY_SCREEN_HEIGHT - 10; ++i) {
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
      const char* bname = "Town";
      if (pool && c->building_type >= 0) {
        const ColonizeBuildingType* bt = colonies_building_type(pool, c->building_type);
        if (bt) {
          bname = bt->name;
        }
      }
      snprintf(line, sizeof(line), "%d. %s @ %s", i + 1, uname, bname);
      font_draw_text(font, framebuffer, 8, y, line, 12);
      y += 9;
    }
  }

  if (view && font) {
    if (!view->frame_ok) {
      font_draw_text(font, framebuffer, 4, 100, "WOODPANL.PIK failed to load", 12);
    }
    if (!view->buildings_ok) {
      font_draw_text(font, framebuffer, 4, 112, "BUILDING.SS failed to load", 12);
    }
    if (!view->parch_ok) {
      font_draw_text(font, framebuffer, 4, 116, "PARCH.SS failed to load", 12);
    }
    if (!view->wood_tile_ok) {
      font_draw_text(font, framebuffer, 4, 124, "WOODTILE.SS failed to load", 12);
    }
    if (!view->bottom_panel_ok) {
      font_draw_text(font, framebuffer, 4, 120, "COLONY.PIK failed to load", 12);
    }
    if (view->status[0]) {
      font_draw_text(font, framebuffer, 160, COLONY_BOTTOM_PANEL_Y + 4, view->status, 12);
    }
  }
}
