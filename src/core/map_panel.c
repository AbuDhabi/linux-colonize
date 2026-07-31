#include "core/map_panel.h"

#include <stdio.h>
#include <string.h>

#include "core/ss.h"
#include "core/turn.h"
#include "core/ui_colors.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

enum {
  MAP_PANEL_COL_OCEAN = 1,
  MAP_PANEL_COL_HIGH_SEAS = 9,
  MAP_PANEL_COL_LAND = COLONIZE_COL_BASIC,
  MAP_PANEL_COL_COLONY = 15,
  MAP_PANEL_COL_UNIT = 14,
  MAP_PANEL_COL_VIEW_RECT = 15,
  MAP_PANEL_COL_TEXT = COLONIZE_COL_BASIC,
  MAP_PANEL_COL_EMPHASIS = COLONIZE_COL_HILITE,
  MAP_PANEL_COL_LINE = 0,
  /* Light tan/brown on the map palette (index 20 is grey). */
  MAP_PANEL_COL_MINIMAP_BORDER = 90
};

void map_panel_tile_rect(
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
        const int fy = y + sy;
        if (fy < origin_y || fy >= y1 || fy < 0 || fy >= framebuffer->height) {
          continue;
        }
        for (int sx = 0; sx < tile->width; ++sx) {
          const int fx = x + sx;
          if (fx < origin_x || fx >= x1 || fx < 0 || fx >= framebuffer->width) {
            continue;
          }
          const uint8_t color = tile->pixels[sy * tile->width + sx];
          if (color == COLONIZE_SS_TRANSPARENT) {
            continue;
          }
          framebuffer->pixels[fy * framebuffer->width + fx] = color;
        }
      }
    }
  }
}

static void map_panel_put(ColonizeFramebuffer8* fb, int x, int y, uint8_t color) {
  if (!fb || !fb->pixels || x < 0 || y < 0 || x >= fb->width || y >= fb->height) {
    return;
  }
  fb->pixels[y * fb->width + x] = color;
}

static void map_panel_hline(ColonizeFramebuffer8* fb, int y, int x0, int x1, uint8_t color) {
  if (!fb || y < 0 || y >= fb->height) {
    return;
  }
  if (x0 > x1) {
    const int t = x0;
    x0 = x1;
    x1 = t;
  }
  if (x0 < 0) {
    x0 = 0;
  }
  if (x1 >= fb->width) {
    x1 = fb->width - 1;
  }
  for (int x = x0; x <= x1; ++x) {
    fb->pixels[y * fb->width + x] = color;
  }
}

static void map_panel_vline(ColonizeFramebuffer8* fb, int x, int y0, int y1, uint8_t color) {
  if (!fb || x < 0 || x >= fb->width) {
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
  if (y1 >= fb->height) {
    y1 = fb->height - 1;
  }
  for (int y = y0; y <= y1; ++y) {
    fb->pixels[y * fb->width + x] = color;
  }
}

static void map_panel_copy_label(char* dst, size_t dst_size, const char* src) {
  if (!dst || dst_size == 0) {
    return;
  }
  if (!src) {
    dst[0] = '\0';
    return;
  }
  snprintf(dst, dst_size, "%s", src);
}

static uint8_t map_panel_terrain_color(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map_tile_is_land(map, x, y)) {
    if (map_tile_is_high_seas(map, x, y)) {
      return MAP_PANEL_COL_HIGH_SEAS;
    }
    return MAP_PANEL_COL_OCEAN;
  }
  return MAP_PANEL_COL_LAND;
}

static int map_panel_clamp_int(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

bool map_panel_load(MapPanel* panel, const char* data_dir, const ColonizeMsgCatalog* labels) {
  if (!panel || !data_dir) {
    return false;
  }
  memset(panel, 0, sizeof(*panel));
  map_panel_copy_label(panel->label_moves, sizeof(panel->label_moves), "Moves:");
  map_panel_copy_label(panel->label_locat, sizeof(panel->label_locat), "Locat:");
  map_panel_copy_label(panel->label_with, sizeof(panel->label_with), "With:");

  if (labels) {
    const ColonizeMsgSection* info = assets_msg_find(labels, "INFO");
    if (info && info->line_count >= 1) {
      map_panel_copy_label(panel->label_moves, sizeof(panel->label_moves), info->lines[0]);
    }
    if (info && info->line_count >= 2) {
      map_panel_copy_label(panel->label_locat, sizeof(panel->label_locat), info->lines[1]);
    }
    if (info && info->line_count >= 3) {
      map_panel_copy_label(panel->label_with, sizeof(panel->label_with), info->lines[2]);
    }
  }

  char path[512];
  char err[256];
  if (dos_compat_normalize_asset_path(data_dir, "WOODTILE.SS", path, sizeof(path))) {
    if (ss_load(path, &panel->wood_tile, err, sizeof(err))) {
      panel->wood_ok = true;
    } else {
      diag_warn("map_panel: WOODTILE.SS: %s", err);
    }
  }
  if (dos_compat_normalize_asset_path(data_dir, "NAMEPLAT.SS", path, sizeof(path))) {
    if (ss_load(path, &panel->nameplat, err, sizeof(err))) {
      panel->nameplat_ok = true;
    } else {
      diag_warn("map_panel: NAMEPLAT.SS: %s", err);
    }
  }
  return true;
}

void map_panel_free(MapPanel* panel) {
  if (!panel) {
    return;
  }
  ss_free(&panel->wood_tile);
  ss_free(&panel->nameplat);
  memset(panel, 0, sizeof(*panel));
}

bool map_panel_contains_xy(int mouse_x, int mouse_y) {
  return mouse_x >= MAP_PANEL_X && mouse_x < 320 && mouse_y >= MAP_MENU_BAR_H && mouse_y < 200;
}

void map_panel_minimap_rect(
  const ColonizeWorldMap* map,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h,
  int* out_origin_x,
  int* out_origin_y
) {
  const int map_w = (map && map->width > 0) ? (int)map->width : 58;
  const int map_h = (map && map->height > 0) ? (int)map->height : 72;
  int w = map_w < MAP_PANEL_MINIMAP_W ? map_w : MAP_PANEL_MINIMAP_W;
  int h = map_h < MAP_PANEL_MINIMAP_H ? map_h : MAP_PANEL_MINIMAP_H;

  /* Center horizontally in the panel interior (leave room for the left rule + brown). */
  const int inner_x0 = MAP_PANEL_X + 2;
  const int inner_x1 = 319;
  const int inner_w = inner_x1 - inner_x0 + 1;
  const int px = inner_x0 + (inner_w - w) / 2;
  /* Brown top border at MAP_MENU_BAR_H touches the menu black rule above. */
  const int py = MAP_PANEL_MINIMAP_ORIGIN_Y;

  const int view_cx = view_x + (view_cols > 0 ? view_cols / 2 : 0);
  const int view_cy = view_y + (view_rows > 0 ? view_rows / 2 : 0);
  int origin_x = view_cx - w / 2;
  int origin_y = view_cy - h / 2;
  origin_x = map_panel_clamp_int(origin_x, 0, map_w > w ? map_w - w : 0);
  origin_y = map_panel_clamp_int(origin_y, 0, map_h > h ? map_h - h : 0);

  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  if (out_x) {
    *out_x = px;
  }
  if (out_y) {
    *out_y = py;
  }
  if (out_origin_x) {
    *out_origin_x = origin_x;
  }
  if (out_origin_y) {
    *out_origin_y = origin_y;
  }
}

bool map_panel_minimap_click(
  const ColonizeWorldMap* map,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int mouse_x,
  int mouse_y,
  int* out_tile_x,
  int* out_tile_y
) {
  if (!map || map->width <= 0 || map->height <= 0) {
    return false;
  }
  int ox, oy, mw, mh, origin_x, origin_y;
  map_panel_minimap_rect(
    map, view_x, view_y, view_cols, view_rows, &ox, &oy, &mw, &mh, &origin_x, &origin_y
  );
  if (mouse_x < ox || mouse_y < oy || mouse_x >= ox + mw || mouse_y >= oy + mh) {
    return false;
  }
  const int tx = origin_x + (mouse_x - ox);
  const int ty = origin_y + (mouse_y - oy);
  if (tx < 0 || ty < 0 || tx >= (int)map->width || ty >= (int)map->height) {
    return false;
  }
  if (out_tile_x) {
    *out_tile_x = tx;
  }
  if (out_tile_y) {
    *out_tile_y = ty;
  }
  return true;
}

void map_panel_render(
  const MapPanel* panel,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeSpriteSheet* icons,
  const ColonizeFont* font,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int cursor_x,
  int cursor_y,
  int selected_unit_id,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  ColonizeFramebuffer8* framebuffer
) {
  if (!framebuffer || !framebuffer->pixels) {
    return;
  }

  const int panel_y = MAP_MENU_BAR_H;
  const int panel_h = framebuffer->height - panel_y;
  if (panel && panel->wood_ok) {
    map_panel_tile_rect(&panel->wood_tile, MAP_PANEL_X, panel_y, MAP_PANEL_W, panel_h, framebuffer);
  } else {
    for (int y = panel_y; y < framebuffer->height; ++y) {
      for (int x = MAP_PANEL_X; x < framebuffer->width; ++x) {
        framebuffer->pixels[y * framebuffer->width + x] = 4;
      }
    }
  }

  int mx, my, mw, mh, origin_x, origin_y;
  map_panel_minimap_rect(
    map, view_x, view_y, view_cols, view_rows, &mx, &my, &mw, &mh, &origin_x, &origin_y
  );

  /* Light-brown border around the minimap (outside the terrain pixels). */
  map_panel_hline(framebuffer, my - 1, mx - 1, mx + mw, MAP_PANEL_COL_MINIMAP_BORDER);
  map_panel_hline(framebuffer, my + mh, mx - 1, mx + mw, MAP_PANEL_COL_MINIMAP_BORDER);
  map_panel_vline(framebuffer, mx - 1, my - 1, my + mh, MAP_PANEL_COL_MINIMAP_BORDER);
  map_panel_vline(framebuffer, mx + mw, my - 1, my + mh, MAP_PANEL_COL_MINIMAP_BORDER);

  if (map && map->terrain && map->width > 0 && map->height > 0) {
    for (int ly = 0; ly < mh; ++ly) {
      for (int lx = 0; lx < mw; ++lx) {
        const int tx = origin_x + lx;
        const int ty = origin_y + ly;
        map_panel_put(framebuffer, mx + lx, my + ly, map_panel_terrain_color(map, tx, ty));
      }
    }

    if (colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &colonies->colonies[i];
        if (!c->active) {
          continue;
        }
        const int lx = c->x - origin_x;
        const int ly = c->y - origin_y;
        if (lx < 0 || ly < 0 || lx >= mw || ly >= mh) {
          continue;
        }
        map_panel_put(framebuffer, mx + lx, my + ly, MAP_PANEL_COL_COLONY);
      }
    }

    if (units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units->units[i];
        if (!units_is_on_map(u)) {
          continue;
        }
        const int lx = u->x - origin_x;
        const int ly = u->y - origin_y;
        if (lx < 0 || ly < 0 || lx >= mw || ly >= mh) {
          continue;
        }
        map_panel_put(framebuffer, mx + lx, my + ly, MAP_PANEL_COL_UNIT);
      }
    }

    /*
     * Viewport outline: white pixels sit ON the edge tiles of the current view
     * (not one pixel outside). Local coords relative to the minimap window.
     */
    if (view_cols > 0 && view_rows > 0) {
      const int lx0 = view_x - origin_x;
      const int ly0 = view_y - origin_y;
      const int lx1 = lx0 + view_cols - 1;
      const int ly1 = ly0 + view_rows - 1;
      for (int lx = lx0; lx <= lx1; ++lx) {
        if (lx >= 0 && lx < mw) {
          if (ly0 >= 0 && ly0 < mh) {
            map_panel_put(framebuffer, mx + lx, my + ly0, MAP_PANEL_COL_VIEW_RECT);
          }
          if (ly1 >= 0 && ly1 < mh) {
            map_panel_put(framebuffer, mx + lx, my + ly1, MAP_PANEL_COL_VIEW_RECT);
          }
        }
      }
      for (int ly = ly0; ly <= ly1; ++ly) {
        if (ly >= 0 && ly < mh) {
          if (lx0 >= 0 && lx0 < mw) {
            map_panel_put(framebuffer, mx + lx0, my + ly, MAP_PANEL_COL_VIEW_RECT);
          }
          if (lx1 >= 0 && lx1 < mw) {
            map_panel_put(framebuffer, mx + lx1, my + ly, MAP_PANEL_COL_VIEW_RECT);
          }
        }
      }
    }
  }

  /* Black separator immediately under the brown bottom border (no wood gap). */
  const int section_bottom = my + mh + 1;
  map_panel_hline(
    framebuffer, section_bottom, MAP_PANEL_X, framebuffer->width - 1, MAP_PANEL_COL_LINE
  );

  /* Sidebar left edge. */
  map_panel_vline(
    framebuffer, MAP_PANEL_X, panel_y, framebuffer->height - 1, MAP_PANEL_COL_LINE
  );

  int text_y = section_bottom + 2;
  const int text_x = MAP_PANEL_X + MAP_PANEL_TEXT_MARGIN;
  const int line_h = font ? (font->max_height + 2) : 8;

  char date[48];
  turn_format_date(game_year, game_autumn, date, sizeof(date));
  char gold_line[48];
  snprintf(gold_line, sizeof(gold_line), "Gold: %d$", gold);
  font_draw_text(font, framebuffer, text_x, text_y, date, MAP_PANEL_COL_TEXT);
  text_y += line_h;
  font_draw_text(font, framebuffer, text_x, text_y, gold_line, MAP_PANEL_COL_TEXT);
  text_y += line_h + 2;

  const ColonizeUnit* selected =
    (units && selected_unit_id >= 0) ? units_get_const(units, selected_unit_id) : NULL;
  if (selected) {
    const ColonizeUnitType* ut = units_type(units, selected->type_index);
    if (panel && panel->nameplat_ok && panel->nameplat.sprite_count > 0) {
      ss_blit_sprite(&panel->nameplat, 0, framebuffer, text_x, text_y);
    }
    if (icons && ut && ut->icon_sprite >= 0) {
      ss_blit_sprite(icons, ut->icon_sprite, framebuffer, text_x + 2, text_y + 1);
    }
    const char* uname = ut ? ut->name : "Unit";
    font_draw_text(font, framebuffer, text_x + 20, text_y + 3, uname, MAP_PANEL_COL_TEXT);
    text_y += 16;

    char line[64];
    snprintf(
      line,
      sizeof(line),
      "%s %d",
      panel ? panel->label_moves : "Moves:",
      selected->moves_left
    );
    font_draw_text(font, framebuffer, text_x, text_y, line, MAP_PANEL_COL_TEXT);
    text_y += line_h;

    snprintf(
      line,
      sizeof(line),
      "%s (%d,%d)",
      panel ? panel->label_locat : "Locat:",
      selected->x,
      selected->y
    );
    font_draw_text(font, framebuffer, text_x, text_y, line, MAP_PANEL_COL_TEXT);
    text_y += line_h;

    if (selected->cargo_count > 0) {
      snprintf(
        line,
        sizeof(line),
        "%s %d",
        panel ? panel->label_with : "With:",
        selected->cargo_count
      );
      font_draw_text(font, framebuffer, text_x, text_y, line, MAP_PANEL_COL_TEXT);
    }
  } else {
    char line[64];
    snprintf(
      line,
      sizeof(line),
      "%s (%d,%d)",
      panel ? panel->label_locat : "Locat:",
      cursor_x,
      cursor_y
    );
    font_draw_text(font, framebuffer, text_x, text_y, line, MAP_PANEL_COL_TEXT);
  }
}
