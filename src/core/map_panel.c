#include "core/map_panel.h"

#include <stdio.h>
#include <string.h>

#include "core/colony.h"
#include "core/ss.h"
#include "core/turn.h"
#include "core/ui_colors.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/* ICONS.SS #22–37 match NAMES.TXT @CARGO (same as colony warehouse strip). */
#define MAP_PANEL_CARGO_ICON_BASE 22
/* World-map / sidebar markers: ICONS.SS European colonies #0–3, Indian villages #10–13. */
#define MAP_PANEL_COLONY_ICON_NONE 3
#define MAP_PANEL_TRIBE_ICON_BASE 10
#define MAP_PANEL_TRIBE_ICON_COUNT 4

enum {
  MAP_PANEL_COL_OCEAN = 1,
  MAP_PANEL_COL_HIGH_SEAS = 9,
  MAP_PANEL_COL_LAND = COLONIZE_COL_BASIC,
  MAP_PANEL_COL_COLONY = 15,
  MAP_PANEL_COL_TRIBE = 12,
  MAP_PANEL_COL_UNIT = 14,
  MAP_PANEL_COL_VIEW_RECT = 15,
  MAP_PANEL_COL_TEXT = COLONIZE_COL_BASIC,
  MAP_PANEL_COL_EMPHASIS = COLONIZE_COL_HILITE,
  MAP_PANEL_COL_LINE = 0,
  /* Dark orange on TERRAIN/WOODTILE palettes (index 90 is muted tan). */
  MAP_PANEL_COL_MINIMAP_BORDER = 6
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

void map_panel_clamp_view_origin(
  int map_w,
  int map_h,
  int center_x,
  int center_y,
  int view_cols,
  int view_rows,
  int* out_view_x,
  int* out_view_y
) {
  /* FUN_6ba1_000c zoom-0: origin ≥ 1 and ≤ map − view − 1 (never show rim). */
  int ox = center_x - (view_cols > 0 ? view_cols / 2 : 0);
  int oy = center_y - (view_rows > 0 ? view_rows / 2 : 0);
  const int min_o = 1;
  int max_ox = map_w - view_cols - 1;
  int max_oy = map_h - view_rows - 1;
  if (max_ox < min_o) {
    ox = min_o;
  } else {
    ox = map_panel_clamp_int(ox, min_o, max_ox);
  }
  if (max_oy < min_o) {
    oy = min_o;
  } else {
    oy = map_panel_clamp_int(oy, min_o, max_oy);
  }
  if (out_view_x) {
    *out_view_x = ox;
  }
  if (out_view_y) {
    *out_view_y = oy;
  }
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
  /* Window covers the visible interior (w−2 / h−2), never the 1-tile rim. */
  const int interior_w = map_w >= 2 ? map_w - 2 : map_w;
  const int interior_h = map_h >= 2 ? map_h - 2 : map_h;
  int w = interior_w < MAP_PANEL_MINIMAP_W ? interior_w : MAP_PANEL_MINIMAP_W;
  int h = interior_h < MAP_PANEL_MINIMAP_H ? interior_h : MAP_PANEL_MINIMAP_H;

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
  const int min_o = 1;
  int max_ox = map_w - 1 - w;
  int max_oy = map_h - 1 - h;
  if (max_ox < min_o) {
    origin_x = min_o;
  } else {
    origin_x = map_panel_clamp_int(origin_x, min_o, max_ox);
  }
  if (max_oy < min_o) {
    origin_y = min_o;
  } else {
    origin_y = map_panel_clamp_int(origin_y, min_o, max_oy);
  }

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
  int tx = origin_x + (mouse_x - ox);
  int ty = origin_y + (mouse_y - oy);
  if (!map_coords_inset(map, tx, ty)) {
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


static void map_panel_csv_field(const char* line, char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!line) {
    return;
  }
  size_t i = 0;
  while (line[i] == ' ' || line[i] == '\t') {
    ++i;
  }
  size_t n = 0;
  while (line[i] && line[i] != ',' && n + 1 < out_size) {
    out[n++] = line[i++];
  }
  while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) {
    --n;
  }
  out[n] = '\0';
}

static const char* map_panel_section_line(
  const ColonizeMsgCatalog* names, const char* section, int index
) {
  if (!names || !section || index < 0) {
    return NULL;
  }
  const ColonizeMsgSection* sec = assets_msg_find(names, section);
  if (!sec || index >= sec->line_count) {
    return NULL;
  }
  return sec->lines[index];
}

static void map_panel_terrain_name(
  const ColonizeMsgCatalog* names, int pedia_index, char* out, size_t out_size
) {
  const char* line = NULL;
  if (pedia_index >= 0 && pedia_index <= 7) {
    line = map_panel_section_line(names, "UNFORESTED", pedia_index);
  } else if (pedia_index >= 8 && pedia_index <= 15) {
    line = map_panel_section_line(names, "FORESTED", pedia_index - 8);
  } else if (pedia_index == 24) {
    line = map_panel_section_line(names, "OTHER", 0);
  } else if (pedia_index == 25) {
    line = map_panel_section_line(names, "OTHER", 1);
  } else if (pedia_index == 26) {
    line = map_panel_section_line(names, "OTHER", 2);
  } else if (pedia_index == 27) {
    line = map_panel_section_line(names, "OTHER", 3);
  } else if (pedia_index == 28) {
    line = map_panel_section_line(names, "OTHER", 4);
  }
  map_panel_csv_field(line ? line : "Unknown", out, out_size);
}

static void map_panel_resource_name(
  const ColonizeMsgCatalog* names, int resource_type, char* out, size_t out_size
) {
  const char* line = map_panel_section_line(names, "RESOURCE", resource_type);
  map_panel_csv_field(line ? line : "Resource", out, out_size);
}

static const char* map_panel_tribe_short(const ColonizeMsgCatalog* names, int nation_id) {
  static const char* k_fallback[] = {
    "Inca", "Aztec", "Arawak", "Iroquois", "Cherokee", "Apache", "Sioux", "Tupi"
  };
  const int idx = nation_id - 4;
  if (idx < 0 || idx >= 8) {
    return "Native";
  }
  const char* line = map_panel_section_line(names, "TRIBES", idx);
  if (!line) {
    return k_fallback[idx];
  }
  /* @TRIBES: Incas, Inca, Jewel..., tech, color — want second field. */
  char plural[32];
  map_panel_csv_field(line, plural, sizeof(plural));
  const char* p = strchr(line, ',');
  if (!p) {
    return k_fallback[idx];
  }
  ++p;
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  static char short_name[32];
  size_t n = 0;
  while (*p && *p != ',' && n + 1 < sizeof(short_name)) {
    short_name[n++] = *p++;
  }
  while (n > 0 && (short_name[n - 1] == ' ' || short_name[n - 1] == '\t')) {
    --n;
  }
  short_name[n] = '\0';
  return short_name[0] ? short_name : k_fallback[idx];
}

static const char* map_panel_euro_country(
  const ColonizeCol1Save* col1, const char* nation_name, int nation_id
) {
  static const char* k_euro[] = {"England", "France", "Spain", "Netherlands"};
  if (nation_id >= 0 && nation_id < 4 && col1) {
    if (col1->player[nation_id].country_name[0]) {
      return col1->player[nation_id].country_name;
    }
  }
  if (nation_name && nation_name[0] && nation_id == 0) {
    return nation_name;
  }
  if (nation_id >= 0 && nation_id < 4) {
    return k_euro[nation_id];
  }
  return "European";
}

static const ColonizeCol1Tribe* map_panel_tribe_at(const ColonizeCol1Save* col1, int x, int y) {
  if (!col1 || !col1->tribe) {
    return NULL;
  }
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &col1->tribe[i];
    if ((int)t->x == x && (int)t->y == y) {
      return t;
    }
  }
  return NULL;
}

void map_panel_render_tribes_on_map(
  const ColonizeCol1Save* col1,
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* framebuffer,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  const ColonizeWorldMap* fog_map,
  int fog_nation
) {
  if (!col1 || !col1->tribe || !framebuffer || !icons ||
      icons->sprite_count < MAP_PANEL_TRIBE_ICON_BASE + MAP_PANEL_TRIBE_ICON_COUNT) {
    return;
  }

  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &col1->tribe[i];
    if (fog_map && !map_tile_seen_by(fog_map, (int)t->x, (int)t->y, fog_nation)) {
      continue;
    }
    const int sx = (int)t->x - view_x;
    const int sy = (int)t->y - view_y;
    if (sx < 0 || sy < 0 || sx >= view_cols || sy >= view_rows) {
      continue;
    }

    int tech = 0;
    if (t->nation_id >= 4 && t->nation_id < 12) {
      tech = (int)col1->indian[t->nation_id - 4].tech;
    }
    if (tech < 0) {
      tech = 0;
    }
    if (tech >= MAP_PANEL_TRIBE_ICON_COUNT) {
      tech = MAP_PANEL_TRIBE_ICON_COUNT - 1;
    }
    const int sprite = MAP_PANEL_TRIBE_ICON_BASE + tech;
    const ColonizeSprite* sp = &icons->sprites[sprite];
    const int tile_px = origin_x + sx * tile_w;
    const int tile_py = origin_y + sy * tile_h;
    const int px = tile_px + (tile_w - sp->width) / 2;
    const int py = tile_py + (tile_h - sp->height) / 2;
    ss_blit_sprite(icons, sprite, framebuffer, px, py);
  }
}

static bool map_panel_tile_plowed(
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  int x,
  int y
) {
  if (map && map->improve) {
    return map_tile_is_plowed(map, x, y);
  }
  if (!col1 || !col1->map.mask || x < 0 || y < 0 ||
      x >= (int)col1->map.width || y >= (int)col1->map.height) {
    return false;
  }
  const uint8_t m = col1->map.mask[y * col1->map.width + x];
  return (m & 0x40u) != 0; /* plowed bit in ColonizeCol1Mask */
}

static bool map_panel_tile_road(
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  int x,
  int y
) {
  if (map && map->improve) {
    return map_tile_has_road(map, x, y);
  }
  if (!col1 || !col1->map.mask || x < 0 || y < 0 ||
      x >= (int)col1->map.width || y >= (int)col1->map.height) {
    return false;
  }
  const uint8_t m = col1->map.mask[y * col1->map.width + x];
  return (m & 0x08u) != 0; /* road bit */
}

static const char* map_panel_order_label(const ColonizeMsgCatalog* names, int orders_index) {
  const char* line = map_panel_section_line(names, "ORDERS", orders_index);
  static char buf[24];
  map_panel_csv_field(line ? line : "No Orders", buf, sizeof(buf));
  return buf;
}

static int map_panel_count_units_at(const ColonizeUnitPool* units, int x, int y) {
  int n = 0;
  if (!units) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (units_is_on_map(u) && u->x == x && u->y == y) {
      n++;
    }
  }
  return n;
}

static int map_panel_draw_line(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int* y,
  int line_h,
  int y_limit,
  const char* text
) {
  if (!text || !text[0] || *y + line_h > y_limit) {
    return 0;
  }
  font_draw_text(font, fb, x, *y, text, MAP_PANEL_COL_TEXT);
  *y += line_h;
  return 1;
}

/* Empty cargo-hold recess (COL1 sidebar shows vacant holds as inset boxes). */
static void map_panel_draw_empty_hold(ColonizeFramebuffer8* fb, int x, int y, int w, int h) {
  if (!fb || w < 3 || h < 3) {
    return;
  }
  for (int dy = 0; dy < h; ++dy) {
    for (int dx = 0; dx < w; ++dx) {
      uint8_t c = 8; /* mid recess */
      if (dy == 0 || dx == 0) {
        c = 0; /* top/left shadow */
      } else if (dy == h - 1 || dx == w - 1) {
        c = 15; /* bottom/right highlight */
      }
      map_panel_put(fb, x + dx, y + dy, c);
    }
  }
}

/*
 * COL1 information sidebar: "With:" then hold icons (passengers from ICONS.SS
 * unit sprites; goods from ICONS.SS #22–37; empty holds as recessed slots).
 */
static void map_panel_draw_with_holds(
  const ColonizeUnitPool* units,
  const ColonizeUnit* ship,
  const ColonizeSpriteSheet* icons,
  const ColonizeFont* font,
  const char* with_label,
  ColonizeFramebuffer8* fb,
  int text_x,
  int* text_y,
  int line_h,
  int y_limit
) {
  if (!ship || !units || !text_y) {
    return;
  }

  int goods_slots = 0;
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (ship->hold_goods_amount[i] > 0 && ship->hold_goods_amount[i] < 255) {
      goods_slots++;
    }
  }

  int cap = units_ship_capacity(units, ship->id);
  if (cap <= 0) {
    /* Wagons / incomplete @UNIT rows: size hold row from contents. */
    cap = ship->cargo_count + goods_slots;
  }
  if (cap <= 0 && ship->cargo_count <= 0 && goods_slots <= 0) {
    return;
  }
  const int used = ship->cargo_count + goods_slots;
  if (cap < used) {
    cap = used;
  }
  if (cap > COLONIZE_UNIT_CARGO_MAX) {
    cap = COLONIZE_UNIT_CARGO_MAX;
  }

  char line[32];
  snprintf(line, sizeof(line), "%s", with_label ? with_label : "With:");
  if (!map_panel_draw_line(font, fb, text_x, text_y, line_h, y_limit, line)) {
    return;
  }

  const int panel_right = MAP_PANEL_X + MAP_PANEL_W - 2;
  const int hold_h = 14;
  const int hold_w = 12;
  const int pitch = 13;
  if (*text_y + hold_h > y_limit) {
    return;
  }

  int x = text_x;
  int y = *text_y;
  int drawn = 0;

  for (int i = 0; i < ship->cargo_count && drawn < cap; ++i) {
    const ColonizeUnit* pax = units_get_const(units, ship->cargo_ids[i]);
    const int sprite = (pax && icons) ? units_map_sprite(units, pax->id) : -1;

    if (x + hold_w > panel_right) {
      x = text_x;
      y += hold_h + 1;
      if (y + hold_h > y_limit) {
        break;
      }
    }

    if (sprite >= 0 && icons && sprite < icons->sprite_count) {
      ss_blit_sprite(icons, sprite, fb, x, y);
      const ColonizeSprite* sp = &icons->sprites[sprite];
      const int step = sp->width > 0 ? sp->width + 1 : pitch;
      x += step > pitch ? step : pitch;
    } else {
      const char* name = units_display_name(units, pax);
      font_draw_text(font, fb, x, y + 2, name ? name : "?", MAP_PANEL_COL_TEXT);
      x = panel_right;
    }
    drawn++;
  }

  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX && drawn < cap; ++i) {
    const int amt = ship->hold_goods_amount[i];
    if (amt <= 0 || amt >= 255) {
      continue;
    }
    const int gtype = ship->hold_goods_type[i];
    if (gtype < 0 || gtype >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    if (x + hold_w > panel_right) {
      x = text_x;
      y += hold_h + 1;
      if (y + hold_h > y_limit) {
        break;
      }
    }
    const int sprite = MAP_PANEL_CARGO_ICON_BASE + gtype;
    if (icons && sprite >= 0 && sprite < icons->sprite_count) {
      ss_blit_sprite(icons, sprite, fb, x, y);
      const ColonizeSprite* sp = &icons->sprites[sprite];
      const int step = sp->width > 0 ? sp->width + 1 : pitch;
      x += step > pitch ? step : pitch;
    } else {
      x += pitch;
    }
    drawn++;
  }

  while (drawn < cap) {
    if (x + hold_w > panel_right) {
      x = text_x;
      y += hold_h + 1;
      if (y + hold_h > y_limit) {
        break;
      }
    }
    map_panel_draw_empty_hold(fb, x, y, hold_w, hold_h);
    x += pitch;
    drawn++;
  }

  *text_y = y + hold_h + 2;
}

void map_panel_render(
  const MapPanel* panel,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeSpriteSheet* icons,
  const ColonizeFont* font,
  const ColonizeMsgCatalog* names,
  const ColonizeMsgCatalog* labels,
  const ColonizeCol1Save* col1,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int cursor_x,
  int cursor_y,
  int selected_unit_id,
  int human_nation,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  int tax_percent,
  const char* nation_name,
  const ColonizePalette* active_palette,
  bool end_turn_active,
  bool end_turn_blink_white,
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

  map_panel_hline(framebuffer, my - 1, mx - 1, mx + mw, MAP_PANEL_COL_MINIMAP_BORDER);
  map_panel_hline(framebuffer, my + mh, mx - 1, mx + mw, MAP_PANEL_COL_MINIMAP_BORDER);
  map_panel_vline(framebuffer, mx - 1, my - 1, my + mh, MAP_PANEL_COL_MINIMAP_BORDER);
  map_panel_vline(framebuffer, mx + mw, my - 1, my + mh, MAP_PANEL_COL_MINIMAP_BORDER);

  if (map && map->terrain && map->width > 0 && map->height > 0) {
    for (int ly = 0; ly < mh; ++ly) {
      for (int lx = 0; lx < mw; ++lx) {
        const int tx = origin_x + lx;
        const int ty = origin_y + ly;
        if (!map_tile_seen_by(map, tx, ty, human_nation)) {
          map_panel_put(framebuffer, mx + lx, my + ly, 0);
          continue;
        }
        map_panel_put(framebuffer, mx + lx, my + ly, map_panel_terrain_color(map, tx, ty));
      }
    }

    if (colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &colonies->colonies[i];
        if (!c->active) {
          continue;
        }
        if (!map_tile_seen_by(map, c->x, c->y, human_nation)) {
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

    if (col1 && col1->tribe) {
      for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
        const ColonizeCol1Tribe* t = &col1->tribe[i];
        if (!map_tile_seen_by(map, (int)t->x, (int)t->y, human_nation)) {
          continue;
        }
        const int lx = (int)t->x - origin_x;
        const int ly = (int)t->y - origin_y;
        if (lx < 0 || ly < 0 || lx >= mw || ly >= mh) {
          continue;
        }
        map_panel_put(framebuffer, mx + lx, my + ly, MAP_PANEL_COL_TRIBE);
      }
    }

    if (units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units->units[i];
        if (!units_is_on_map(u)) {
          continue;
        }
        if (!map_tile_seen_by(map, u->x, u->y, human_nation)) {
          continue;
        }
        /* Same vis-bit gate as the main map (FUN_2f2b_6372). */
        if (u->nation_id != human_nation && human_nation >= 0 && human_nation <= 3 &&
            (u->col1_vis_mask & (1u << human_nation)) == 0) {
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

  const int section_bottom = my + mh + 1;
  map_panel_hline(
    framebuffer, section_bottom, MAP_PANEL_X, framebuffer->width - 1, MAP_PANEL_COL_LINE
  );
  map_panel_vline(
    framebuffer, MAP_PANEL_X, panel_y, framebuffer->height - 1, MAP_PANEL_COL_LINE
  );

  int text_y = section_bottom + 2;
  const int text_x = MAP_PANEL_X + MAP_PANEL_TEXT_MARGIN;
  const int line_h = font ? (font->max_height + 2) : 8;
  const int y_limit = framebuffer->height - 4;

  char date[48];
  turn_format_date(game_year, game_autumn, date, sizeof(date));
  char gold_line[64];
  snprintf(gold_line, sizeof(gold_line), "Gold: %d$  Tax: %d%%", gold, tax_percent);
  map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, date);
  map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, gold_line);
  text_y += 2;

  const ColonizeUnit* selected =
    (units && selected_unit_id >= 0) ? units_get_const(units, selected_unit_id) : NULL;
  const int info_x = selected ? selected->x : cursor_x;
  const int info_y = selected ? selected->y : cursor_y;

  if (selected) {
    if (panel && panel->nameplat_ok && panel->nameplat.sprite_count > 0) {
      ss_blit_sprite(&panel->nameplat, 0, framebuffer, text_x, text_y);
    }
    const int sel_sprite = units_map_sprite(units, selected->id);
    if (icons && sel_sprite >= 0) {
      const int ix = text_x + 2;
      const int iy = text_y + 1;
      const int stack_n = map_panel_count_units_at(units, selected->x, selected->y);
      unit_chrome_blit_unit_for_palette(
        framebuffer,
        font,
        icons,
        sel_sprite,
        ix,
        iy,
        units_display_type_index(units, selected->id),
        selected->nation_id,
        selected->orders,
        stack_n > 1,
        selected->aboard_ship_id >= 0,
        active_palette
      );
    }
    const char* uname = units_display_name(units, selected);
    font_draw_text(font, framebuffer, text_x + 20, text_y + 3, uname, MAP_PANEL_COL_TEXT);
    text_y += 16;

    char line[64];
    if (selected->tools > 0) {
      snprintf(line, sizeof(line), "Tools: %d", selected->tools);
      map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, line);
    }
    snprintf(
      line, sizeof(line), "%s %d", panel ? panel->label_moves : "Moves:", selected->moves_left
    );
    map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, line);
    snprintf(
      line,
      sizeof(line),
      "%s (%d,%d)",
      panel ? panel->label_locat : "Locat:",
      selected->x,
      selected->y
    );
    map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, line);
    /* COL1: transport holds under With: as unit/cargo icons (LABELS.TXT @INFO). */
    {
      int goods = 0;
      for (int g = 0; g < COLONIZE_UNIT_CARGO_MAX; ++g) {
        if (selected->hold_goods_amount[g] > 0 && selected->hold_goods_amount[g] < 255) {
          goods++;
        }
      }
      if (units_is_sea(units, selected->id) || selected->cargo_count > 0 || goods > 0 ||
          units_ship_capacity(units, selected->id) > 0) {
        map_panel_draw_with_holds(
          units,
          selected,
          icons,
          font,
          panel ? panel->label_with : "With:",
          framebuffer,
          text_x,
          &text_y,
          line_h,
          y_limit
        );
      }
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
    map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, line);
  }

  /* Tile details under Locat (manual Information Sidebar). */
  if (map && map->terrain && info_x >= 0 && info_y >= 0 && info_x < map->width &&
      info_y < map->height) {
    char line[72];

    if (map_tile_is_land(map, info_x, info_y)) {
      const ColonizeColony* col_here =
        colonies ? colonies_get(colonies, colonies_id_at(colonies, info_x, info_y)) : NULL;
      const ColonizeCol1Tribe* tribe = map_panel_tribe_at(col1, info_x, info_y);
      if (col_here && col_here->active) {
        snprintf(
          line,
          sizeof(line),
          "%s",
          map_panel_euro_country(col1, nation_name, col_here->nation_id)
        );
        map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, line);
      } else if (tribe) {
        snprintf(line, sizeof(line), "%s Land", map_panel_tribe_short(names, tribe->nation_id));
        map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, line);
      } else {
        const char* wild = "Wilderness";
        if (labels) {
          const ColonizeMsgSection* misc = assets_msg_find(labels, "MISC");
          /* LABELS @MISC: Wilderness is among the early lines — search. */
          if (misc) {
            for (int i = 0; i < misc->line_count; ++i) {
              if (strcmp(misc->lines[i], "Wilderness") == 0) {
                wild = misc->lines[i];
                break;
              }
            }
          }
        }
        map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, wild);
      }
    }

    {
      char tname[40];
      map_panel_terrain_name(names, map_pedia_terrain_index_at(map, info_x, info_y), tname, sizeof(tname));
      snprintf(line, sizeof(line), "(%s)", tname);
      map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, line);
    }

    if (map_panel_tile_plowed(map, col1, info_x, info_y)) {
      map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, "(Plowed)");
    }
    if (map_panel_tile_road(map, col1, info_x, info_y)) {
      map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, "(Road)");
    }
    if (map_tile_has_major_river(map, info_x, info_y)) {
      map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, "(Major River)");
    } else if (map_tile_has_river(map, info_x, info_y)) {
      map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, "(River)");
    }
    {
      const int rtype = map_resource_type_at(map, info_x, info_y);
      if (rtype >= 0) {
        char rname[40];
        map_panel_resource_name(names, rtype, rname, sizeof(rname));
        snprintf(line, sizeof(line), "(%s)", rname);
        map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, line);
      }
    }
    if (map_tile_has_rumour(map, info_x, info_y)) {
      map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, "(Lost City Rumor)");
    }

    /* Colony / native camp under the tile. */
    if (colonies) {
      const int cid = colonies_id_at(colonies, info_x, info_y);
      const ColonizeColony* col = colonies_get(colonies, cid);
      if (col && col->active) {
        int colony_icon = MAP_PANEL_COLONY_ICON_NONE;
        const int fortress = colonies_find_building(colonies, "Fortress");
        const int fort = colonies_find_building(colonies, "Fort");
        const int stockade = colonies_find_building(colonies, "Stockade");
        if (fortress >= 0 && col->has_building[fortress]) {
          colony_icon = 2;
        } else if (fort >= 0 && col->has_building[fort]) {
          colony_icon = 1;
        } else if (stockade >= 0 && col->has_building[stockade]) {
          colony_icon = 0;
        }
        int label_x = text_x + 18;
        if (icons && icons->sprite_count > MAP_PANEL_COLONY_ICON_NONE) {
          colonies_blit_settlement_icon(
            icons, colony_icon, framebuffer, text_x, text_y, col->nation_id, active_palette
          );
          const ColonizeSprite* sp = &icons->sprites[colony_icon];
          if (sp->width > 0) {
            label_x = text_x + sp->width + 2;
          }
        }
        snprintf(line, sizeof(line), "%s", col->name[0] ? col->name : "Colony");
        font_draw_text(font, framebuffer, label_x, text_y + 2, line, MAP_PANEL_COL_TEXT);
        text_y += 16;
        /* Cargo summary stub: first non-zero stock. */
        for (int c = 0; c < COLONIZE_CARGO_COUNT && text_y + line_h <= y_limit; ++c) {
          if (col->stock[c] > 0) {
            snprintf(line, sizeof(line), "  cargo %d", col->stock[c]);
            map_panel_draw_line(font, framebuffer, text_x, &text_y, line_h, y_limit, line);
            break;
          }
        }
      }
    }
    {
      const ColonizeCol1Tribe* tribe = map_panel_tribe_at(col1, info_x, info_y);
      if (tribe) {
        const char* tshort = map_panel_tribe_short(names, tribe->nation_id);
        const char* settlement = "Camp";
        int tech = 0;
        if (names) {
          const ColonizeMsgSection* levels = assets_msg_find(names, "LEVELS");
          tech = (tribe->nation_id >= 4 && tribe->nation_id < 12)
                   ? (col1 && col1->indian[tribe->nation_id - 4].tech)
                   : 0;
          if (levels && tech >= 0 && tech < levels->line_count) {
            /* tech-level, singular, plural */
            const char* lp = strchr(levels->lines[tech], ',');
            if (lp) {
              ++lp;
              while (*lp == ' ') {
                ++lp;
              }
              static char settle[24];
              size_t n = 0;
              while (*lp && *lp != ',' && n + 1 < sizeof(settle)) {
                settle[n++] = *lp++;
              }
              while (n > 0 && settle[n - 1] == ' ') {
                --n;
              }
              settle[n] = '\0';
              if (settle[0]) {
                settlement = settle;
              }
            }
          }
        }
        if (tech < 0) {
          tech = 0;
        }
        if (tech >= MAP_PANEL_TRIBE_ICON_COUNT) {
          tech = MAP_PANEL_TRIBE_ICON_COUNT - 1;
        }
        const int tribe_icon = MAP_PANEL_TRIBE_ICON_BASE + tech;
        int label_x = text_x + 18;
        if (icons && icons->sprite_count > tribe_icon) {
          ss_blit_sprite(icons, tribe_icon, framebuffer, text_x, text_y);
          const ColonizeSprite* sp = &icons->sprites[tribe_icon];
          if (sp->width > 0) {
            label_x = text_x + sp->width + 2;
          }
        }
        snprintf(line, sizeof(line), "%s %s", tshort, settlement);
        font_draw_text(font, framebuffer, label_x, text_y + 2, line, MAP_PANEL_COL_TEXT);
        text_y += 16;
      }
    }

    /* Units on the selected tile with order label. */
    if (units) {
      const int tile_n = map_panel_count_units_at(units, info_x, info_y);
      for (int i = 0; i < COLONIZE_UNITS_MAX && text_y + 14 <= y_limit; ++i) {
        const ColonizeUnit* u = &units->units[i];
        if (!units_is_on_map(u) || u->x != info_x || u->y != info_y) {
          continue;
        }
        const int sprite = units_map_sprite(units, u->id);
        if (icons && sprite >= 0) {
          unit_chrome_blit_unit_for_palette(
            framebuffer,
            font,
            icons,
            sprite,
            text_x,
            text_y,
            units_display_type_index(units, u->id),
            u->nation_id,
            u->orders,
            tile_n > 1,
            u->aboard_ship_id >= 0,
            active_palette
          );
        }
        const char* orders = map_panel_order_label(names, u->orders);
        snprintf(
          line, sizeof(line), "%s %s", units_display_name(units, u), orders
        );
        font_draw_text(font, framebuffer, text_x + 18, text_y + 2, line, MAP_PANEL_COL_TEXT);
        text_y += 14;
      }
    }
  }

  /* Player-requested: "End Turn" prompt pinned to the bottom of the
   * sidebar (not inline with the flowing unit-info text), flashing white
   * (15) / black (0) — not a show/hide blink, the text is always drawn
   * while active, just alternating color. */
  if (end_turn_active) {
    const uint8_t flash_color = end_turn_blink_white ? 15 : 0;
    const int end_turn_y = framebuffer->height - line_h - 2;
    font_draw_text(font, framebuffer, text_x, end_turn_y, "End Turn", flash_color);
  }
}
