#include "core/mapedit.h"

/*
 * MAPEDIT.EXE port (docs/mapedit.md). Every behaviour cites its MAPEDIT function
 * (original_sources_decompiled/mapedit.c); DS addresses are MAPEDIT's own.
 *
 * Sections:
 *  - Assets and names
 *  - Map file I/O, undo, post-load fix-up
 *  - Painting (FUN_1000_1be0 / 1d28) and the palette (10cc / 1226)
 *  - Find Continents (FUN_19c4_0002)
 *  - View, cursor and zoom
 *  - Dialogs (popups, name entry, file picker)
 *  - Input
 *  - Rendering
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/fb.h"
#include "core/font.h"
#include "core/howmuch_dialog.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/strutil.h"
#include "core/ui_colors.h"
#include "platform/diagnostics.h"

/* Screen layout (FUN_1a47_0006, FUN_1b56_03b4, FUN_1000_07a6 / 094e). */
#define ME_MENU_H 8
#define ME_VIEW_W 240
#define ME_VIEW_H 192
#define ME_PANEL_X 241
#define ME_MINI_FRAME_Y 8
#define ME_MINI_FRAME_W 0x4f
#define ME_MINI_FRAME_H 0x29
#define ME_MINI_X 252
#define ME_MINI_Y 9
#define ME_MINI_W 56
#define ME_MINI_H 39
#define ME_PANEL_Y 0x32
#define ME_PANEL_W 0x4f
#define ME_PANEL_H 0x96
#define ME_TEXT_X 0xf2
#define ME_TEXT_Y 0x33
#define ME_BLINK_MS 1100u /* FUN_1000_22b0: 20 BIOS ticks (18.2 Hz) per cursor phase */

/* Menu ids (FUN_1000_0196 / FUN_1000_17e0). */
enum {
  ME_ID_SAVEAS = 0x13, ME_ID_CREATE = 0x14, ME_ID_SAVE = 0x1a, ME_ID_LOAD = 0x1b,
  ME_ID_EXIT = 0x1f, ME_ID_ZOOM_IN = 0x24, ME_ID_ZOOM_OUT = 0x25, ME_ID_F1 = 0x26,
  ME_ID_F4 = 0x29, ME_ID_CENTER = 0x2b, ME_ID_CONTINENTS = 0x4a, ME_ID_TILES = 0x4b,
  ME_ID_FILL = 0x4c, ME_ID_COAST = 0x4d, ME_ID_UNDO = 0x4e, ME_ID_HELP1 = 0x51,
  ME_ID_HELP2 = 0x52, ME_ID_HELP3 = 0x53, ME_ID_HELP4 = 0x54, ME_ID_ABOUT = 0x5f
};

static bool me_is_water(int t) {
  return t == 0x19 || t == 0x1a;
}

/* ===================== Assets and names ===================== */
uint8_t* mapedit_exe_load(const char* data_dir, size_t* out_size) {
  char path[640];
  if (out_size) {
    *out_size = 0;
  }
  if (!dos_compat_normalize_asset_path(data_dir, "MAPEDIT.EXE", path, sizeof(path))) {
    return NULL;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  uint8_t* buf = NULL;
  long n = -1;
  if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0) {
    buf = (uint8_t*)malloc((size_t)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
      free(buf);
      buf = NULL;
    }
  }
  fclose(f);
  if (buf) {
    buf[n] = 0;
    if (out_size) {
      *out_size = (size_t)n;
    }
  }
  return buf;
}

const char* mapedit_exe_str(const uint8_t* exe, size_t exe_size, unsigned ds) {
  const size_t off = MAPEDIT_EXE_DS_BASE + ds;
  if (!exe || off >= exe_size) {
    return "";
  }
  return (const char*)exe + off; /* the buffer ends in a NUL */
}

void mapedit_fmt_ints(char* out, size_t out_size, const char* fmt, int a, int b) {
  size_t n = 0;
  int used = 0;
  for (const char* p = fmt; *p && n + 1 < out_size; ++p) {
    if (p[0] == '%' && p[1] == 'd' && used < 2) {
      n += (size_t)snprintf(out + n, out_size - n, "%d", used++ == 0 ? a : b);
      if (n >= out_size) {
        n = out_size - 1;
      }
      ++p;
      continue;
    }
    out[n++] = *p;
  }
  out[n < out_size ? n : out_size - 1] = '\0';
}

static const char* me_str(const MapEditor* ed, unsigned ds) {
  return mapedit_exe_str(ed->exe, ed->exe_size, ds);
}

static bool me_load_font(MapEditor* ed, const char* file, ColonizeFont* out) {
  char path[640];
  char err[256];
  if (!dos_compat_normalize_asset_path(ed->data_dir, file, path, sizeof(path))) {
    return false;
  }
  return font_file_load(path, out, err, sizeof(err));
}

static bool me_load_txt(MapEditor* ed, const char* file, ColonizeMsgCatalog* out) {
  char path[640];
  if (!dos_compat_normalize_asset_path(ed->data_dir, file, path, sizeof(path))) {
    return false;
  }
  return assets_msg_load_file(out, path);
}

/* A NAMES.TXT record's name: the text before the first comma. */
static void me_record_name(const char* line, char* out, size_t out_size) {
  size_t n = 0;
  while (line[n] && line[n] != ',' && n + 1 < out_size) {
    ++n;
  }
  memcpy(out, line, n);
  out[n] = '\0';
  str_trim(out);
}

/* FUN_1000_2336: @UNFORESTED -> 0-7, @FORESTED -> 8-15 (and 16-23), @OTHER -> 24-28,
 * @OTHER_NAMES -> five words. */
static void me_load_names(MapEditor* ed) {
  static const struct { const char* sec; int base; int count; } k_rows[] = {
    {"UNFORESTED", 0, 8}, {"FORESTED", 8, 8}, {"OTHER", 24, 5}
  };
  for (size_t s = 0; s < sizeof(k_rows) / sizeof(k_rows[0]); ++s) {
    const ColonizeMsgSection* sec = ed->names_txt_ok ? assets_msg_find(&ed->names_txt, k_rows[s].sec) : NULL;
    int row = 0;
    for (int i = 0; sec && i < sec->line_count && row < k_rows[s].count; ++i) {
      if (sec->lines[i][0] == '\0' || sec->lines[i][0] == ';') {
        continue;
      }
      me_record_name(sec->lines[i], ed->terrain_names[k_rows[s].base + row], sizeof(ed->terrain_names[0]));
      ++row;
    }
  }
  for (int i = 0; i < 8; ++i) {
    memcpy(ed->terrain_names[16 + i], ed->terrain_names[8 + i], sizeof(ed->terrain_names[0]));
  }
  const ColonizeMsgSection* other = ed->names_txt_ok ? assets_msg_find(&ed->names_txt, "OTHER_NAMES") : NULL;
  int row = 0;
  for (int i = 0; other && i < other->line_count && row < 5; ++i) {
    if (other->lines[i][0] != '\0') {
      me_record_name(other->lines[i], ed->other_names[row++], sizeof(ed->other_names[0]));
    }
  }
}

static uint8_t me_sprite_centre(const ColonizeSpriteSheet* sheet, int idx) {
  if (!sheet || idx < 0 || idx >= sheet->sprite_count) {
    return 0;
  }
  const ColonizeSprite* s = &sheet->sprites[idx];
  if (!s->pixels || s->width <= 8 || s->height <= 8) {
    return 0;
  }
  return s->pixels[8 * s->width + 8];
}

/* FUN_1b56_006c (asm 0xb5cc): the minimap colour of each class is the centre pixel (8,8)
 * of its sprite: TERRAIN i for 0-23 (i & 7 for forests), 24-26 TERRAIN, PHYS0 0x21 / 0x31
 * (1-based) for mountains / hills. */
static void me_build_minimap_colors(MapEditor* ed) {
  /* TERRAIN.SS holds 12 tiles (0-7, scrub, arctic, ocean, sea lane): map_terrain_index_to_sprite. */
  for (int i = 0; i < 24; ++i) {
    ed->minimap_color[i] =
      me_sprite_centre(ed->terrain_ok ? &ed->terrain : NULL, map_terrain_index_to_sprite(i & 7));
  }
  for (int i = 24; i < 27; ++i) {
    ed->minimap_color[i] =
      me_sprite_centre(ed->terrain_ok ? &ed->terrain : NULL, map_terrain_index_to_sprite(i));
  }
  ed->minimap_color[0x1b] = me_sprite_centre(ed->phys0_ok ? &ed->phys0 : NULL, 0x21 - 1);
  ed->minimap_color[0x1c] = me_sprite_centre(ed->phys0_ok ? &ed->phys0 : NULL, 0x31 - 1);
}

/* FUN_1000_0196: the bar from MAPMENU.TXT. The code reads the item lines in order and adds
 * its own separators; "Memory check" and "About Map Editor" are never added (only five
 * Help lines are read, the fifth opening @ABOUT). */
static void me_build_menu(MapEditor* ed) {
  static const int k_game[] = {ME_ID_SAVE, ME_ID_SAVEAS, ME_ID_LOAD, ME_ID_CREATE,
                               MAP_MENU_CUSTOM_SEP, ME_ID_EXIT};
  static const int k_view[] = {ME_ID_ZOOM_IN, ME_ID_ZOOM_OUT, MAP_MENU_CUSTOM_SEP, 0x26, 0x27,
                               0x28, 0x29, MAP_MENU_CUSTOM_SEP, ME_ID_CENTER};
  static const int k_cup[] = {ME_ID_TILES, ME_ID_FILL, MAP_MENU_CUSTOM_SEP, ME_ID_COAST,
                              ME_ID_CONTINENTS, MAP_MENU_CUSTOM_SEP, ME_ID_UNDO};
  static const int k_help[] = {ME_ID_HELP1, ME_ID_HELP2, ME_ID_HELP3, ME_ID_HELP4,
                               MAP_MENU_CUSTOM_SEP, ME_ID_ABOUT};
  const MapMenuCustomSection secs[] = {
    {"GAME", k_game, (int)(sizeof k_game / sizeof k_game[0])},
    {"VIEW", k_view, (int)(sizeof k_view / sizeof k_view[0])},
    {"CUP", k_cup, (int)(sizeof k_cup / sizeof k_cup[0])},
    {"HELP", k_help, (int)(sizeof k_help / sizeof k_help[0])},
  };
  (void)map_menu_load_custom(&ed->menu, ed->mapmenu_txt_ok ? &ed->mapmenu_txt : NULL, secs, 4);
}

/* ===================== Map file I/O, undo, post-load fix-up ===================== */
/* FUN_1c04_005c: cut at the last '.', append ".MP", upper-case. */
static void me_force_mp(const char* in, char* out, size_t out_size) {
  char buf[MAPEDIT_NAME_LEN];
  str_copy_trunc(buf, sizeof(buf), in);
  char* dot = strrchr(buf, '.');
  if (dot) {
    *dot = '\0';
  }
  snprintf(out, out_size, "%s.MP", buf);
  for (char* p = out; *p; ++p) {
    *p = (char)toupper((unsigned char)*p);
  }
}

static void me_map_path(const MapEditor* ed, const char* name, char* out, size_t out_size) {
  if (!dos_compat_normalize_asset_path(ed->data_dir, name, out, out_size)) {
    snprintf(out, out_size, "%s/%s", ed->data_dir, name);
  }
}

void mapedit_fix_loaded(ColonizeWorldMap* map) {
  for (size_t i = 0; map && i < map->tile_count; ++i) {
    const uint8_t b = map->terrain[i];
    const int t = b & 0x1f;
    if (t >= 0x18) {
      continue;
    }
    if (b & 0x20) {
      map->terrain[i] = (uint8_t)((b & 0xe0) | (t & 7));
    } else if (t >= 16) {
      map->terrain[i] = (uint8_t)(b - 8);
    }
  }
}

/* FUN_19f9_0170: version must be 4 and W*H at most 12000 (FUN_19f9_012c). */
static bool me_load(MapEditor* ed, const char* name) {
  char path[640];
  me_map_path(ed, name, path, sizeof(path));
  FILE* f = fopen(path, "rb");
  uint8_t hdr[COLONIZE_MAP_HEADER_SIZE];
  const bool hdr_ok = f && fread(hdr, 1, sizeof hdr, f) == sizeof hdr;
  if (f) {
    fclose(f);
  }
  if (!hdr_ok) {
    return false;
  }
  const int w = hdr[0] | (hdr[1] << 8);
  const int h = hdr[2] | (hdr[3] << 8);
  const int ver = hdr[4] | (hdr[5] << 8);
  if (ver != MAP_MP_VERSION || w <= 0 || h <= 0 || w > 255 || h > 255 ||
      w * h > MAPEDIT_UNDO_BYTES) {
    return false;
  }
  ColonizeWorldMap m;
  memset(&m, 0, sizeof m);
  char err[256];
  if (!map_load_mp(path, &m, err, sizeof err)) {
    return false;
  }
  map_free(&ed->map);
  ed->map = m;
  ed->map.specials_off = true;
  ed->map_ok = true;
  ed->undo_valid = false;
  me_force_mp(name, ed->file_name, sizeof ed->file_name);
  mapedit_fix_loaded(&ed->map);
  return true;
}

static bool me_save(MapEditor* ed) {
  char path[640];
  me_map_path(ed, ed->file_name, path, sizeof(path));
  char err[256];
  if (!ed->map_ok || !map_save_mp(path, &ed->map, err, sizeof err)) {
    return false;
  }
  ed->dirty = false;
  return true;
}

/* FUN_19f9_03ba(58, 72): ocean everywhere, layers 2 / 3 zero, dirty. */
static bool me_new_map(MapEditor* ed, const char* name) {
  ColonizeWorldMap m;
  memset(&m, 0, sizeof m);
  if (!map_alloc(&m, 0x3a, 0x48, NULL, 0)) {
    return false;
  }
  memset(m.terrain, 0x19, m.tile_count);
  memset(m.seen, 0xff, m.tile_count);
  map_free(&ed->map);
  ed->map = m;
  ed->map.specials_off = true;
  ed->map_ok = true;
  ed->undo_valid = false;
  me_force_mp(name, ed->file_name, sizeof ed->file_name);
  ed->dirty = true;
  return true;
}

/* FUN_1000_0750 / 077e: one terrain-plane snapshot. */
static void me_undo_snapshot(MapEditor* ed) {
  if (ed->map_ok && ed->map.tile_count <= MAPEDIT_UNDO_BYTES) {
    memcpy(ed->undo, ed->map.terrain, ed->map.tile_count);
    ed->undo_valid = true;
  }
}

static void me_undo_restore(MapEditor* ed) {
  if (ed->undo_valid && ed->map.tile_count <= MAPEDIT_UNDO_BYTES) {
    memcpy(ed->map.terrain, ed->undo, ed->map.tile_count);
  }
}

/* ===================== View, cursor and zoom ===================== */
typedef struct MeView {
  int left, top;   /* DS:0x49f2 / 0x49f4 */
  int vis_w, vis_h; /* DS:0x52d4 / 0x52d8, tiles shown */
  int off_x, off_y; /* DS:0x5ad8 / 0x5af4, tile margin when the map is narrower */
  int tile;         /* 16 >> level */
} MeView;

/* FUN_1a47_0006. */
static MeView me_view(const MapEditor* ed) {
  MeView v;
  const int W = ed->map.width;
  const int H = ed->map.height;
  v.vis_w = 0xf << ed->level;
  v.vis_h = 0xc << ed->level;
  v.tile = 0x10 >> ed->level;
  v.left = ed->view_cx - (v.vis_w >> 1);
  v.top = ed->view_cy - (v.vis_h >> 1);
  if (v.left < 1) v.left = 1;
  if (v.left > W - v.vis_w - 1) v.left = W - v.vis_w - 1;
  if (v.top < 1) v.top = 1;
  if (v.top > H - v.vis_h - 1) v.top = H - v.vis_h - 1;
  v.off_x = 0;
  v.off_y = 0;
  if (W - 2 < v.vis_w) {
    v.left = 1;
    v.off_x = (v.vis_w - W + 2) >> 1;
    v.vis_w = W - 2;
  }
  if (H - 2 < v.vis_h) {
    v.top = 1;
    v.off_y = (v.vis_h - H + 2) >> 1;
    v.vis_h = H - 2;
  }
  return v;
}

static bool me_inset(const MapEditor* ed, int x, int y) {
  return x >= 1 && y >= 1 && x <= ed->map.width - 2 && y <= ed->map.height - 2;
}

/* FUN_1000_1404: the view centres on (x, y); with DS:0x70 = 1 (its start value) the cursor
 * goes there too. */
static void me_center(MapEditor* ed, int x, int y) {
  if (!me_inset(ed, x, y)) {
    return;
  }
  ed->view_cx = x;
  ed->view_cy = y;
  ed->cur_x = x;
  ed->cur_y = y;
}

/* FUN_1000_145e(x, y, x, y): a cursor within two tiles of an edge the view can still
 * scroll past recentres the view on it. */
static bool me_keep_in_view(MapEditor* ed, int x, int y) {
  const MeView v = me_view(ed);
  const int right = v.left + v.vis_w - 1;
  const int bottom = v.top + v.vis_h - 1;
  bool move = false;
  if (x < v.left + 2 && v.left > 1) move = true;
  if (y < v.top + 2 && v.top > 1) move = true;
  if (x > right - 2 && right < ed->map.width - 2) move = true;
  if (y > bottom - 2 && bottom < ed->map.height - 2) move = true;
  if (move) {
    me_center(ed, x, y);
  }
  return move;
}

/* FUN_1000_1514: the move first lets the old position recentre the view. */
static void me_move_cursor(MapEditor* ed, int dx, int dy) {
  const int nx = ed->cur_x + dx;
  const int ny = ed->cur_y + dy;
  if (nx < 1 || ny < 1 || nx >= ed->map.width - 1 || ny >= ed->map.height - 1) {
    return;
  }
  (void)me_keep_in_view(ed, ed->cur_x, ed->cur_y);
  ed->cur_x = nx;
  ed->cur_y = ny;
  ed->clock_ms = 0; /* the redraw shows the cursor at once */
}

/* FUN_1000_1582. */
static void me_zoom(MapEditor* ed, int level) {
  ed->level = level < 0 ? 0 : (level > 3 ? 3 : level);
  (void)me_keep_in_view(ed, ed->cur_x, ed->cur_y);
}

/* ===================== Painting (FUN_1000_1be0 / 1d28) and the palette (10cc / 1226) ===================== */
void mapedit_paint_tile(MapEditor* ed, int x, int y, bool rmb) {
  if (!ed || !ed->map_ok || x <= 0 || y <= 0 || x >= ed->map.width - 1 || y >= ed->map.height - 1) {
    return;
  }
  /* DOS-LITERAL FUN_1000_1be0 (mapedit.c 1490-1543): only tiles inside the shown view. */
  const MeView v = me_view(ed);
  if (x < v.left || x > v.left + v.vis_w - 1 || y < v.top || y > v.top + v.vis_h - 1) {
    return;
  }
  uint8_t* p = &ed->map.terrain[y * ed->map.width + x];
  uint8_t v4 = *p;
  const uint8_t t = (uint8_t)(*p & 0x1f);
  uint8_t nt = t;
  if ((!rmb || ed->brush_feature) && (!me_is_water(t) || !ed->coast_protect || ed->brush_type < 0)) {
    if (!me_is_water(t) || (ed->brush_or & 0x20) == 0) {
      v4 = (uint8_t)(v4 & ed->brush_and);
      if (!rmb) {
        v4 = (uint8_t)(v4 | ed->brush_or);
      }
    }
    if (ed->brush_type >= 0 && !rmb) {
      nt = (uint8_t)ed->brush_type;
    }
  }
  if (rmb && !ed->brush_feature) {
    ed->brush_type = (int8_t)nt;
    ed->brush_and = me_is_water(nt) ? 0x40 : 0xff;
    ed->brush_or = 0;
    return;
  }
  if (ed->brush_feature) {
    nt = t;
  }
  const uint8_t n = (uint8_t)((v4 & 0xe0) | nt);
  if (n != *p) {
    *p = n;
    ed->dirty = true;
  }
}

/* FUN_1000_1d28: the (2r+1)^2 square around (x, y). */
static void me_brush(MapEditor* ed, int x, int y, bool rmb) {
  const int r = ed->fill_radius;
  for (int yy = y - r; yy <= y + r; ++yy) {
    for (int xx = x - r; xx <= x + r; ++xx) {
      if (me_inset(ed, xx, yy)) {
        mapedit_paint_tile(ed, xx, yy, rmb);
      }
    }
  }
}

/* Palette hotspots (FUN_1000_0e14): 1-16 = the land rows (row, column), 17-23 = row 3
 * columns 0-6 (Arctic, Ocean, Sea Lane, major river, minor river, mountains, hills). */
static void me_hotspot(int idx, int* row, int* col) {
  if (idx <= 16) {
    *row = (idx - 1) / 8;
    *col = (idx - 1) % 8;
  } else {
    *row = 3;
    *col = idx - 17;
  }
}

void mapedit_palette_pick(MapEditor* ed, int idx) {
  if (!ed || idx < 1 || idx > 23) {
    return;
  }
  int row = 0;
  int col = 0;
  me_hotspot(idx, &row, &col);
  int8_t type = -1;
  uint8_t and_mask = 0xff;
  uint8_t or_bits = 0;
  bool feature = false;
  if (row < 3) {
    type = (int8_t)(col + row * 8);
    and_mask = 0xff;
  } else {
    switch (col) {
    case 0: type = 0x18; and_mask = 0xff; break;
    case 1: type = 0x19; and_mask = 0x40; break;
    case 2: type = 0x1a; and_mask = 0x40; break;
    case 3: and_mask = 0x1f; or_bits = 0xc0; feature = true; type = 0; break;
    case 4: and_mask = 0x3f; or_bits = 0x40; feature = true; type = 0; break;
    case 5: and_mask = 0x1f; or_bits = 0xa0; feature = true; type = 0; break;
    case 6: and_mask = 0x5f; or_bits = 0x20; feature = true; type = 0; break;
    default: return;
    }
  }
  ed->brush_type = type;
  ed->brush_and = and_mask;
  ed->brush_or = or_bits;
  ed->brush_feature = feature;
  ed->palette_idx = idx;
}

/* DOS-LITERAL FUN_1000_1226 (mapedit.c 959-991): the arrows walk the hotspot index. */
void mapedit_palette_key(MapEditor* ed, ColonizeKey key) {
  int d = 0;
  if (key == COLONIZE_KEY_UP || key == COLONIZE_KEY_KP8) {
    d = ed->palette_idx == 8 ? 8 : -8;
  } else if (key == COLONIZE_KEY_LEFT || key == COLONIZE_KEY_KP4) {
    d = -1;
  } else if (key == COLONIZE_KEY_RIGHT || key == COLONIZE_KEY_KP6) {
    d = 1;
  } else if (key == COLONIZE_KEY_DOWN || key == COLONIZE_KEY_KP2) {
    d = ed->palette_idx == 0x10 ? -8 : 8;
  }
  if (d == 0) {
    return;
  }
  int i = ed->palette_idx + d - 1;
  if (i == 0x17) {
    i = 0;
  }
  if (i > 0x17) {
    i -= 0x18;
  }
  if (i < 0) {
    i += 8;
  }
  mapedit_palette_pick(ed, i + 1);
}

/* ===================== Find Continents (FUN_19c4_0002) ===================== */
int mapedit_find_continents(ColonizeWorldMap* map) {
  if (!map || !map->terrain || !map->layer3) {
    return 0;
  }
  const int W = map->width;
  const int H = map->height;
  const int n = W * H;
  uint16_t* lab = (uint16_t*)calloc((size_t)n + (size_t)W + 2, sizeof(uint16_t));
  int* cnt = (int*)calloc(0x4002, sizeof(int));
  if (!lab || !cnt) {
    free(lab);
    free(cnt);
    return 0;
  }
  int flags = 0;
  /* DOS-LITERAL mapedit.c 8361-8476: water pass (1) then land (0); y 1..H-2, x right to
   * left; the run label carries across the row end like DOS's locals do. */
  for (int pass = 1; pass >= 0; --pass) {
    memset(cnt, 0, 0x4000 * sizeof(int));
    memset(lab, 0, (size_t)n * sizeof(uint16_t));
    unsigned nid = 0;
    unsigned e = 0;
    bool run = false;
    for (int y = 1; y < H - 1; ++y) {
      for (int x = W - 2; x > 0; --x) {
        const int t = map->terrain[y * W + x] & 0x1f;
        if ((me_is_water(t) ? 1 : 0) != pass) {
          run = false;
          e = 0;
          continue;
        }
        for (int dx = -1; dx < 2; ++dx) {
          unsigned a = lab[(y - 1) * W + x + dx];
          if (a == 0) {
            continue;
          }
          if (e != 0 && a != e) {
            const unsigned hi = a > e ? a : e;
            const unsigned lo = a < e ? a : e;
            cnt[lo] += cnt[hi];
            cnt[hi] = 0;
            for (int yy = 1; yy <= y; ++yy) {
              for (int xx = 1; xx <= W && W * yy + xx < n; ++xx) {
                if (lab[W * yy + xx] == hi) {
                  lab[W * yy + xx] = (uint16_t)lo;
                }
              }
            }
            a = lo;
          }
          e = a;
        }
        if (e == 0) {
          if (!run) {
            nid = ((y == 1 || H - y == 2) && pass == 0) ? 0x10 : 0;
            do {
              ++nid;
            } while (nid < 0x4002 - 1 && cnt[nid] != 0);
            if (nid > 0x3fff) {
              nid = 0x3fff;
            }
          }
          e = nid;
        }
        lab[W * y + x] = (uint16_t)e;
        cnt[e]++;
        run = true;
      }
    }
    for (int i = 0; i < n; ++i) {
      unsigned id = lab[i];
      if (id == 0) {
        continue;
      }
      if (id > 0xf) {
        const int c = cnt[id];
        if (c < 1) {
          id = (unsigned)(-c); /* 0 stays 0, as DOS's ~c + 1 */
        } else {
          unsigned s = 0;
          do {
            ++s;
          } while (cnt[s] != 0);
          if (s > 0xf) {
            flags |= 1 << pass;
            lab[i] = 0xf;
            map->layer3[i] = 0xf;
            continue;
          }
          cnt[s] = cnt[id];
          cnt[id] = -(int)s;
          id = s;
        }
        lab[i] = (uint16_t)id;
      }
      map->layer3[i] = (uint8_t)id;
    }
  }
  free(lab);
  free(cnt);
  return flags;
}

/* ===================== Dialogs (popups, name entry, file picker) ===================== */
static void me_popup(MapEditor* ed, const char* section, MapEditPending pending) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof tok);
  tok.string0 = ed->file_name; /* upper-cased current file, DS:0x634e */
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ed->mapedit_txt_ok ? &ed->mapedit_txt : NULL, section, &tok, "", body, sizeof body);
  const ColonizeMsgSection* sec = ed->mapedit_txt_ok ? assets_msg_find(&ed->mapedit_txt, section) : NULL;
  char rows[AI_POPUP_CHOICE_MAX][POPUP_MSG_CHOICE_LEN];
  const int nrows = sec ? popup_msg_choices(sec, rows, AI_POPUP_CHOICE_MAX) : 0;
  ed->pending = pending;
  /* The catalog keeps the bare @smallfont directive as a section line. */
  ed->popup_small = false;
  for (int i = 0; sec && i < sec->line_count; ++i) {
    if (strcmp(sec->lines[i], "@smallfont") == 0) {
      ed->popup_small = true;
    }
  }
  if (nrows <= 0) {
    (void)ai_popup_enqueue_ok(&ed->popups, AI_POPUP_TAG_INFO, NULL, body);
    return;
  }
  const char* labels[AI_POPUP_CHOICE_MAX];
  int ids[AI_POPUP_CHOICE_MAX];
  for (int i = 0; i < nrows; ++i) {
    labels[i] = rows[i];
    ids[i] = i + 1;
  }
  (void)ai_popup_enqueue_choice(&ed->popups, AI_POPUP_TAG_INFO, NULL, body, labels, ids, nrows);
}

static void me_name_entry(MapEditor* ed, const char* section, NameEntryKind kind, MapEditNameFor why,
                          const char* initial) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof tok);
  tok.string0 = ed->file_name;
  char prompt[AI_POPUP_BODY_LEN];
  popup_msg_fill(ed->mapedit_txt_ok ? &ed->mapedit_txt : NULL, section, &tok, "", prompt, sizeof prompt);
  if (name_entry_open(&ed->name_entry, kind, prompt, initial, -1)) {
    /* "Name:", the section's last line (a trailing caption, not a choice row; port_bugs.md #1307). */
    name_entry_set_label(&ed->name_entry,
                         howmuch_amount_label(ed->mapedit_txt_ok ? &ed->mapedit_txt : NULL, section, ""));
    ed->name_for = why;
  }
}

static int me_cmp_name(const void* a, const void* b) {
  return strcmp((const char*)a, (const char*)b);
}

/* FUN_1000_056a: *.MP files, ten a page, a "(More)" row before them on later pages and
 * after them while more follow. The port sorts the names (DOS lists findfirst order). */
static void me_open_picker(MapEditor* ed, const char* section, MapEditPending pending, bool rescan) {
  if (rescan) {
    ed->pick_count = 0;
    ed->pick_page = 0;
    DIR* dir = opendir(ed->data_dir);
    struct dirent* ent;
    while (dir && (ent = readdir(dir)) != NULL && ed->pick_count < MAPEDIT_PICK_MAX) {
      const size_t len = strlen(ent->d_name);
      if (len > 3 && len < sizeof(ed->pick_names[0]) && strcasecmp(ent->d_name + len - 3, ".MP") == 0) {
        str_copy_trunc(ed->pick_names[ed->pick_count++], sizeof(ed->pick_names[0]), ent->d_name);
      }
    }
    if (dir) {
      closedir(dir);
    }
    qsort(ed->pick_names, (size_t)ed->pick_count, sizeof(ed->pick_names[0]), me_cmp_name);
  }
  ed->pending = pending;
  ed->popup_small = false;
  if (ed->pick_count == 0) {
    /* No file: as ESC (startup creates a new map). */
    ai_popup_clear(&ed->popups);
    ed->popups.has_result = true;
    ed->popups.result_cancelled = true;
    ed->popups.result_tag = AI_POPUP_TAG_INFO;
    return;
  }
  const int pages = (ed->pick_count + 9) / 10;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ed->mapedit_txt_ok ? &ed->mapedit_txt : NULL, section, NULL, "", body, sizeof body);
  const char* labels[AI_POPUP_CHOICE_MAX];
  int ids[AI_POPUP_CHOICE_MAX];
  int n = 0;
  if (ed->pick_page > 0) {
    labels[n] = me_str(ed, MAPEDIT_DS_MORE);
    ed->pick_row_file[n] = -2;
    ids[n] = n + 1;
    ++n;
  }
  for (int i = ed->pick_page * 10; i < ed->pick_count && i < ed->pick_page * 10 + 10; ++i) {
    labels[n] = ed->pick_names[i];
    ed->pick_row_file[n] = i;
    ids[n] = n + 1;
    ++n;
  }
  if (ed->pick_page < pages - 1) {
    labels[n] = me_str(ed, MAPEDIT_DS_MORE2);
    ed->pick_row_file[n] = -3;
    ids[n] = n + 1;
    ++n;
  }
  (void)ai_popup_enqueue_choice(&ed->popups, AI_POPUP_TAG_INFO, NULL, body, labels, ids, n);
}

static void me_recenter_new(MapEditor* ed) {
  ed->view_cx = ed->cur_x = ed->map.width >> 1;
  ed->view_cy = ed->cur_y = ed->map.height >> 1;
}

/* The exit flow (FUN_1000_17e0 case 0x1f, FUN_1000_1db6 ESC): @EXIT when dirty. */
static void me_exit_request(MapEditor* ed) {
  if (!ed->dirty) {
    ed->quit = true;
    return;
  }
  me_popup(ed, "EXIT", MAPEDIT_PENDING_EXIT);
}

static void me_menu_action(MapEditor* ed, int id) {
  switch (id) {
  case ME_ID_SAVEAS:
    me_name_entry(ed, "SAVEAS", NAME_ENTRY_KIND_MAP_SAVEAS, MAPEDIT_NAME_SAVEAS, ed->file_name);
    break;
  case ME_ID_CREATE:
    if (ed->dirty) {
      me_popup(ed, "CREATENOW", MAPEDIT_PENDING_CREATENOW);
    } else {
      me_name_entry(ed, "NEWNAME", NAME_ENTRY_KIND_MAP_NEW, MAPEDIT_NAME_NEW, me_str(ed, MAPEDIT_DS_UNTITLED));
    }
    break;
  case ME_ID_SAVE:
    me_popup(ed, "SAVE", MAPEDIT_PENDING_SAVE);
    break;
  case ME_ID_LOAD:
    if (ed->dirty) {
      me_popup(ed, "LOAD", MAPEDIT_PENDING_LOAD);
    } else {
      me_open_picker(ed, "MAPTOLOAD", MAPEDIT_PENDING_PICK_LOAD, true);
    }
    break;
  case ME_ID_EXIT:
    me_exit_request(ed);
    break;
  case ME_ID_ZOOM_IN:
    me_zoom(ed, ed->level - 1);
    break;
  case ME_ID_ZOOM_OUT:
    me_zoom(ed, ed->level + 1);
    break;
  case ME_ID_CENTER:
    me_center(ed, ed->cur_x, ed->cur_y);
    break;
  case ME_ID_CONTINENTS: {
    const int flags = mapedit_find_continents(&ed->map);
    if (flags & 1) {
      me_popup(ed, "CONTINENTS1", MAPEDIT_PENDING_INFO);
    }
    if (flags & 2) {
      me_popup(ed, "CONTINENTS2", MAPEDIT_PENDING_INFO);
    }
    ed->continents_open = true;
    ed->dirty = true;
    break;
  }
  case ME_ID_TILES:
    ed->palette_open = true;
    break;
  case ME_ID_FILL:
    ed->fill_radius = (ed->fill_radius + 1) % 3;
    break;
  case ME_ID_COAST:
    ed->coast_protect = !ed->coast_protect;
    break;
  case ME_ID_UNDO:
    me_undo_restore(ed);
    break;
  case ME_ID_HELP1: me_popup(ed, "HELP1", MAPEDIT_PENDING_INFO); break;
  case ME_ID_HELP2: me_popup(ed, "HELP2", MAPEDIT_PENDING_INFO); break;
  case ME_ID_HELP3: me_popup(ed, "HELP3", MAPEDIT_PENDING_INFO); break;
  case ME_ID_HELP4: me_popup(ed, "HELP4", MAPEDIT_PENDING_INFO); break;
  case ME_ID_ABOUT: me_popup(ed, "ABOUT", MAPEDIT_PENDING_INFO); break;
  default:
    if (id >= ME_ID_F1 && id <= ME_ID_F4) {
      me_zoom(ed, -(id - ME_ID_F4));
    }
    break;
  }
}

/* A dialog answer. */
static void me_popup_result(MapEditor* ed) {
  const bool cancelled = ed->popups.result_cancelled;
  const int choice = cancelled ? 0 : ed->popups.result_choice_id;
  ai_popup_consume_result(&ed->popups);
  const MapEditPending why = ed->pending;
  ed->pending = MAPEDIT_PENDING_NONE;
  switch (why) {
  case MAPEDIT_PENDING_PICK_START:
  case MAPEDIT_PENDING_PICK_LOAD: {
    const int file = (choice >= 1 && choice <= AI_POPUP_CHOICE_MAX) ? ed->pick_row_file[choice - 1] : -1;
    if (file == -2 || file == -3) {
      ed->pick_page += file == -2 ? -1 : 1;
      me_open_picker(ed, why == MAPEDIT_PENDING_PICK_START ? "MAPTOEDIT" : "MAPTOLOAD", why, false);
      return;
    }
    if (why == MAPEDIT_PENDING_PICK_START) {
      if (file < 0) {
        me_name_entry(ed, "NEWNAME", NAME_ENTRY_KIND_MAP_NEW, MAPEDIT_NAME_NEW_START, me_str(ed, MAPEDIT_DS_UNTITLED));
      } else if (me_load(ed, ed->pick_names[file])) {
        me_recenter_new(ed);
      } else {
        ed->exit_code = 1; /* the load error ends the program (DS:0x4a4) */
        ed->quit = true;
      }
      return;
    }
    if (file >= 0) {
      if (me_load(ed, ed->pick_names[file])) {
        me_recenter_new(ed);
      } else {
        me_popup(ed, "ERROR", MAPEDIT_PENDING_INFO);
      }
    }
    return;
  }
  case MAPEDIT_PENDING_SAVE:
    if (choice == 1 && !me_save(ed)) {
      me_popup(ed, "ERROR", MAPEDIT_PENDING_INFO);
    }
    return;
  case MAPEDIT_PENDING_LOAD:
    if (choice == 1) {
      me_open_picker(ed, "MAPTOLOAD", MAPEDIT_PENDING_PICK_LOAD, true);
    }
    return;
  case MAPEDIT_PENDING_CREATENOW:
    if (choice == 1) {
      me_name_entry(ed, "NEWNAME", NAME_ENTRY_KIND_MAP_NEW, MAPEDIT_NAME_NEW, me_str(ed, MAPEDIT_DS_UNTITLED));
    } else {
      me_recenter_new(ed); /* FUN_1000_17e0 case 0x14 falls to LAB_1000_196c either way (port_bugs.md #1310) */
    }
    return;
  case MAPEDIT_PENDING_EXIT:
    if (choice == 1) {
      ed->quit = true;
    } else if (choice == 2) {
      if (me_save(ed)) {
        ed->quit = true;
      } else {
        me_popup(ed, "ERROR", MAPEDIT_PENDING_INFO);
      }
    }
    return;
  default:
    return;
  }
}

static void me_name_result(MapEditor* ed) {
  const bool cancelled = ed->name_entry.result_cancelled;
  char name[NAME_ENTRY_NAME_LEN];
  str_copy_trunc(name, sizeof name, ed->name_entry.result_name);
  ed->name_entry.has_result = false;
  const MapEditNameFor why = ed->name_for;
  ed->name_for = MAPEDIT_NAME_NONE;
  if (why == MAPEDIT_NAME_SAVEAS) {
    if (!cancelled && name[0]) {
      me_force_mp(name, ed->file_name, sizeof ed->file_name);
      if (!me_save(ed)) {
        me_popup(ed, "ERROR", MAPEDIT_PENDING_INFO);
      }
    }
    return;
  }
  if (cancelled || !name[0]) {
    if (why == MAPEDIT_NAME_NEW_START) {
      ed->quit = true; /* FUN_1000_247a: no map, no editor */
    } else {
      me_recenter_new(ed);
    }
    return;
  }
  if (!me_new_map(ed, name)) {
    if (why == MAPEDIT_NAME_NEW_START) {
      ed->quit = true;
    }
    return;
  }
  me_recenter_new(ed);
}

/* ===================== Input ===================== */
/* FUN_1000_1b84: 1 map, 2 minimap, 3 panel, 0 elsewhere. */
static int me_region(int x, int y) {
  if (x < ME_VIEW_W && y >= ME_MENU_H && y < 200) {
    return 1;
  }
  if (x >= ME_MINI_X && x < ME_MINI_X + ME_MINI_W && y >= ME_MINI_Y && y < ME_MINI_Y + ME_MINI_H) {
    return 2;
  }
  if (x >= ME_PANEL_X && x < 320 && y >= ME_PANEL_Y && y < 200) {
    return 3;
  }
  return 0;
}

static void me_minimap_origin(const MapEditor* ed, int* left, int* top) {
  /* FUN_1b56_00dc. */
  int l = ed->view_cx - 28;
  int t = ed->view_cy - 19;
  if (l > ed->map.width - 57) l = ed->map.width - 57;
  if (l < 1) l = 1;
  if (t > ed->map.height - 40) t = ed->map.height - 40;
  if (t < 1) t = 1;
  *left = l;
  *top = t;
}

static int me_clamp(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/* FUN_1000_20e0 and its region handlers 1f8e / 2082 / panel. */
static void me_mouse(MapEditor* ed, const ColonizeInputState* in) {
  const bool left = in->mouse_left_down;
  const bool right = in->mouse_right_down;
  const bool any = left || right;
  const bool press = (left && !ed->prev_left) || (right && !ed->prev_right);
  const bool release = (!left && ed->prev_left) || (!right && ed->prev_right);
  ed->prev_left = left;
  ed->prev_right = right;
  const int region = me_region(in->mouse_x, in->mouse_y);
  if (press) {
    ed->press_region = region;
  }
  if (ed->press_region == 1 && region == 1) {
    if (press) {
      me_undo_snapshot(ed);
    }
    const MeView v = me_view(ed);
    const int tx = in->mouse_x / v.tile - v.off_x + v.left;
    const int ty = (in->mouse_y - ME_MENU_H) / v.tile - v.off_y + v.top;
    if ((any || release) && me_inset(ed, tx, ty)) {
      if (any) {
        ed->cur_x = tx;
        ed->cur_y = ty;
      }
      if (!in->shift_held) {
        if (release || right) {
          me_center(ed, tx, ty);
        }
      } else if (any) {
        if (!right || ed->brush_feature) {
          me_brush(ed, tx, ty, right);
        } else {
          mapedit_paint_tile(ed, tx, ty, true);
        }
      }
    }
  } else if (ed->press_region == 2 && release) {
    int ml = 0;
    int mt = 0;
    me_minimap_origin(ed, &ml, &mt);
    const int x = me_clamp(in->mouse_x + ml - ME_MINI_X, 1, ed->map.width - 2);
    const int y = me_clamp(in->mouse_y + mt - ME_MINI_Y, 1, ed->map.height - 2);
    if (x != ed->view_cx || y != ed->view_cy) {
      me_center(ed, x, y);
    }
  } else if (ed->press_region == 3 && release) {
    ed->palette_open = true;
  }
  if (release && !any) {
    ed->press_region = -1;
  }
}

/* Palette swatch rects (FUN_1000_0e14): land at (col*17+1, row*17+1), row 3 at y 0x30. */
static int me_palette_hit(int x, int y) {
  for (int idx = 1; idx <= 23; ++idx) {
    int row = 0;
    int col = 0;
    me_hotspot(idx, &row, &col);
    const int sx = col * 0x11 + 1;
    const int sy = row < 3 ? row * 0x11 + 1 : 0x30;
    if (x >= sx && x < sx + 16 && y >= sy && y < sy + 16) {
      return idx;
    }
  }
  return 0;
}

/* FUN_1000_1226: arrows pick at once, a click picks and closes, any other key closes. */
static void me_palette_input(MapEditor* ed, const ColonizeInputState* in) {
  if (in->last_key == COLONIZE_KEY_UP || in->last_key == COLONIZE_KEY_DOWN ||
      in->last_key == COLONIZE_KEY_LEFT || in->last_key == COLONIZE_KEY_RIGHT ||
      in->last_key == COLONIZE_KEY_KP8 || in->last_key == COLONIZE_KEY_KP2 ||
      in->last_key == COLONIZE_KEY_KP4 || in->last_key == COLONIZE_KEY_KP6) {
    mapedit_palette_key(ed, in->last_key);
    return;
  }
  if (in->last_key != COLONIZE_KEY_NONE || in->text_input_len > 0) { /* any key (port_bugs.md #1309) */
    ed->palette_open = false;
    return;
  }
  const bool any = in->mouse_left_down || in->mouse_right_down;
  if (any) {
    const int idx = me_palette_hit(in->mouse_x, in->mouse_y);
    if (idx > 0) {
      mapedit_palette_pick(ed, idx);
    }
  }
  if (in->mouse_left_released || in->mouse_right_released) {
    ed->palette_open = false;
    ed->prev_left = ed->prev_right = false;
    ed->press_region = -1;
  }
}

/* FUN_1000_1db6 and the menu accelerators (FUN_16d7_1452 / 14e6). */
static void me_keys(MapEditor* ed, const ColonizeInputState* in) {
  const ColonizeKey k = in->last_key;
  const char ch = in->text_input_len > 0 ? in->text_input[0] : 0;
  if (in->alt_held && (k == COLONIZE_KEY_Q || k == COLONIZE_KEY_X)) {
    me_exit_request(ed); /* 0x110 / 0x12d */
    return;
  }
  if (in->ctrl_held && (k == COLONIZE_KEY_Q || k == COLONIZE_KEY_X)) {
    me_exit_request(ed); /* ^Q / ^X */
    return;
  }
  if (in->alt_held && ch) {
    (void)map_menu_open_alt_hotkey(&ed->menu, (char)toupper((unsigned char)ch));
    return;
  }
  if (k >= COLONIZE_KEY_F1 && k <= COLONIZE_KEY_F1 + 3) {
    me_menu_action(ed, ME_ID_F1 + (int)(k - COLONIZE_KEY_F1)); /* "~F~1".."~F~4" */
    return;
  }
  int dx = 0;
  int dy = 0;
  switch (k) {
  case COLONIZE_KEY_UP: case COLONIZE_KEY_KP8: dy = -1; break;
  case COLONIZE_KEY_DOWN: case COLONIZE_KEY_KP2: dy = 1; break;
  case COLONIZE_KEY_LEFT: case COLONIZE_KEY_KP4: dx = -1; break;
  case COLONIZE_KEY_RIGHT: case COLONIZE_KEY_KP6: dx = 1; break;
  case COLONIZE_KEY_HOME: case COLONIZE_KEY_KP7: dx = -1; dy = -1; break;
  case COLONIZE_KEY_PAGEUP: case COLONIZE_KEY_KP9: dx = 1; dy = -1; break;
  case COLONIZE_KEY_END: case COLONIZE_KEY_KP1: dx = -1; dy = 1; break;
  case COLONIZE_KEY_PAGEDOWN: case COLONIZE_KEY_KP3: dx = 1; dy = 1; break;
  default: break;
  }
  if (dx == 0 && dy == 0 && ch >= '1' && ch <= '9' && ch != '5') {
    static const int k_dx[] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
    static const int k_dy[] = {1, 1, 1, 0, 0, 0, -1, -1, -1};
    dx = k_dx[ch - '1'];
    dy = k_dy[ch - '1'];
  }
  if (dx != 0 || dy != 0) {
    me_move_cursor(ed, dx, dy);
    return;
  }
  if (k == COLONIZE_KEY_ESCAPE) {
    me_exit_request(ed);
    return;
  }
  if (k == COLONIZE_KEY_ENTER || k == COLONIZE_KEY_SPACE) {
    me_undo_snapshot(ed);
    me_brush(ed, ed->cur_x, ed->cur_y, false);
    return;
  }
  if (k == COLONIZE_KEY_BACKSPACE) {
    if (!ed->brush_feature) {
      mapedit_paint_tile(ed, ed->cur_x, ed->cur_y, true);
    } else {
      me_undo_snapshot(ed);
      me_brush(ed, ed->cur_x, ed->cur_y, true);
    }
    return;
  }
  if (ch) {
    const int id = map_menu_custom_hotkey(&ed->menu, ch);
    if (id > 0) {
      me_menu_action(ed, id);
    }
  }
}

bool mapedit_update(MapEditor* ed, ColonizePlatform* platform, const ColonizeInputState* in, uint32_t dt_ms) {
  if (!ed || ed->quit) {
    return false;
  }
  ed->clock_ms += dt_ms;
  ai_popup_set_now_ms(ed->clock_ms);
  if (platform && ed->cursor_ok && !ed->cursor_built && ed->cursor.sprite_count > 0) {
    const ColonizeSprite* s = &ed->cursor.sprites[0];
    ed->cursor_built = platform_set_mouse_cursor_indexed(
      platform, s->pixels, s->width, s->height, 0, 0, ed->cursor.has_palette ? &ed->cursor.palette : &ed->palette
    );
    platform_show_game_mouse_cursor(platform, ed->cursor_built);
  }
  if (ed->name_entry.open) {
    (void)name_entry_handle_input(&ed->name_entry, in);
    if (ed->name_entry.has_result) {
      me_name_result(ed);
    }
    return !ed->quit;
  }
  if (ed->popups.has_result) {
    me_popup_result(ed);
    return !ed->quit;
  }
  if (ai_popup_busy(&ed->popups)) {
    if (!ed->popups.open) {
      ai_popup_try_present_next(&ed->popups);
    }
    (void)ai_popup_handle_input(&ed->popups, in);
    if (ed->popups.has_result) {
      me_popup_result(ed);
    }
    return !ed->quit;
  }
  if (!ed->map_ok) {
    return !ed->quit;
  }
  /* The window close is the port's ESC: it waits for any open dialog, which owns the one
   * `pending` slot (port_bugs.md #1303). */
  if (in->quit_requested) {
    me_exit_request(ed);
    return !ed->quit;
  }
  if (ed->continents_open) {
    if (colonize_input_any_key_or_click(in) || in->text_input_len > 0) {
      ed->continents_open = false; /* FUN_1000_0060: any key, mapped or not (port_bugs.md #1309) */
    }
    return true;
  }
  if (ed->palette_open) {
    me_palette_input(ed, in);
    return true;
  }
  /* ESC on an open pull-down only closes it; the menu loop eats the key (port_bugs.md #1304). */
  const bool menu_was_open = ed->menu.open_index >= 0;
  const MapMenuAction act = map_menu_handle_input(&ed->menu, in, ed->font_tiny_ok ? &ed->font_tiny : NULL,
                                                  in->last_key == COLONIZE_KEY_ESCAPE);
  if (menu_was_open && in->last_key == COLONIZE_KEY_ESCAPE) {
    return true;
  }
  if (act == MAP_MENU_ACTION_USER) {
    me_menu_action(ed, ed->menu.picked_id);
    return !ed->quit;
  }
  if (ed->menu.open_index >= 0) {
    return true;
  }
  if (in->last_key != COLONIZE_KEY_NONE || in->text_input_len > 0) {
    me_keys(ed, in);
  } else if (!map_menu_hit_ui(&ed->menu, in->mouse_x, in->mouse_y) || ed->press_region >= 0) {
    me_mouse(ed, in);
  }
  return !ed->quit;
}

/* ===================== Rendering ===================== */
static void me_composite_tile(const MapEditor* ed, int mx, int my, ColonizeFramebuffer8* tile) {
  ColonizeMapLayerCmd cmds[MAP_LAYER_CMDS_MAX];
  const int ncmd = map_tile_layer_cmds(&ed->map, mx, my, 0, cmds, MAP_LAYER_CMDS_MAX);
  memset(tile->pixels, 0, 16 * 16);
  for (int ci = 0; ci < ncmd; ++ci) {
    const ColonizeMapLayerCmd* c = &cmds[ci];
    /* FUN_1a47_0932: land transitions (PHYS0 104-107 and the fill into their holes) only
     * at zoom 0. */
    if (ed->level > 0 && c->sheet == MAP_LAYER_SHEET_PHYS0 && c->sprite >= 104 && c->sprite <= 107) {
      if (ci + 1 < ncmd && cmds[ci + 1].into_holes) {
        ++ci;
      }
      continue;
    }
    const ColonizeSpriteSheet* sheet = c->sheet == MAP_LAYER_SHEET_TERRAIN ? &ed->terrain : &ed->phys0;
    if (c->sprite < 0 || c->sprite >= sheet->sprite_count) {
      continue;
    }
    const ColonizeSprite* s = &sheet->sprites[c->sprite];
    const int ox = c->offset ? c->ox : (16 - s->width) / 2;
    const int oy = c->offset ? c->oy : (16 - s->height) / 2;
    for (int y = 0; y < s->height; ++y) {
      for (int x = 0; x < s->width; ++x) {
        const int px = ox + x;
        const int py = oy + y;
        if (px < 0 || py < 0 || px >= 16 || py >= 16) {
          continue;
        }
        const uint8_t col = s->pixels[y * s->width + x];
        if (col == COLONIZE_SS_TRANSPARENT) {
          continue;
        }
        uint8_t* d = &tile->pixels[py * 16 + px];
        if (c->into_holes && *d != 0) {
          continue;
        }
        *d = col;
      }
    }
  }
}

static void me_render_view(const MapEditor* ed, ColonizeFramebuffer8* fb) {
  const MeView v = me_view(ed);
  /* FUN_1a47_1022: wood in the margin of a map narrower than the view. */
  if ((v.off_x || v.off_y) && ed->woodtile_ok) {
    ss_tile_rect(&ed->woodtile, 0, ME_MENU_H, ME_VIEW_W, ME_VIEW_H, fb);
  }
  uint8_t px[256];
  ColonizeFramebuffer8 tile = {.width = 16, .height = 16, .pixels = px};
  for (int ty = 0; ty < v.vis_h; ++ty) {
    for (int tx = 0; tx < v.vis_w; ++tx) {
      me_composite_tile(ed, v.left + tx, v.top + ty, &tile);
      const int sx = (tx + v.off_x) * v.tile;
      const int sy = ME_MENU_H + (ty + v.off_y) * v.tile;
      for (int y = 0; y < v.tile; ++y) {
        for (int x = 0; x < v.tile; ++x) {
          fb_put(fb, sx + x, sy + y, px[(y << ed->level) * 16 + (x << ed->level)]);
        }
      }
    }
  }
  /* FUN_1000_1310: ICONS sprite 0x13 + level (1-based), blinking. */
  if (ed->icons_ok && (ed->clock_ms % (2 * ME_BLINK_MS)) < ME_BLINK_MS &&
      ed->cur_x >= v.left && ed->cur_x < v.left + v.vis_w && ed->cur_y >= v.top &&
      ed->cur_y < v.top + v.vis_h) {
    ss_blit_sprite(&ed->icons, 0x13 + ed->level - 1, fb, (ed->cur_x - v.left + v.off_x) * v.tile,
                   ME_MENU_H + (ed->cur_y - v.top + v.off_y) * v.tile);
  }
}

static int me_minimap_class(uint8_t b) {
  if (b & 0x20) {
    return (b & 0x80) ? 0x1b : 0x1c; /* mountain / hill (mapedit.c 9829) */
  }
  return b & 0x1f;
}

static void me_render_side(const MapEditor* ed, ColonizeFramebuffer8* fb) {
  if (ed->woodtile_ok) {
    ss_tile_rect(&ed->woodtile, ME_PANEL_X, ME_MINI_FRAME_Y, ME_MINI_FRAME_W, ME_MINI_FRAME_H, fb);
    ss_tile_rect(&ed->woodtile, ME_PANEL_X, ME_PANEL_Y, ME_PANEL_W, ME_PANEL_H, fb);
  }
  int ml = 0;
  int mt = 0;
  me_minimap_origin(ed, &ml, &mt);
  for (int y = 0; y < ME_MINI_H; ++y) {
    for (int x = 0; x < ME_MINI_W; ++x) {
      const int mx = ml + x;
      const int my = mt + y;
      if (mx >= ed->map.width || my >= ed->map.height) {
        continue;
      }
      const int cls = me_minimap_class(ed->map.terrain[my * ed->map.width + mx]);
      fb_put(fb, ME_MINI_X + x, ME_MINI_Y + y, cls < 29 ? ed->minimap_color[cls] : 0);
    }
  }
  /* The view box (FUN_1b56_03b4, colour 0xf), clipped to the minimap: DOS caps its bottom at
   * min(minimap top + 0x26, view bottom) (port_bugs.md #1306); the other edges get the same
   * clip (their args are lost in the decomp). */
  {
    const MeView v = me_view(ed);
    const int x0 = me_clamp(v.left - ml, 0, ME_MINI_W - 1);
    const int y0 = me_clamp(v.top - mt, 0, ME_MINI_H - 1);
    const int x1 = me_clamp(v.left + v.vis_w - 1 - ml, 0, ME_MINI_W - 1);
    const int y1 = me_clamp(v.top + v.vis_h - 1 - mt, 0, ME_MINI_H - 1);
    fb_rect_outline(fb, ME_MINI_X + x0, ME_MINI_Y + y0, x1 - x0 + 1, y1 - y0 + 1, 0xf, 0xf);
  }
  if (!ed->font_tiny_ok) {
    return;
  }
  const ColonizeFont* f = &ed->font_tiny;
  const int fh = f->max_height;
  const uint8_t ink = COLONIZE_COL_BASIC;
  char line[64];
  int y = ME_TEXT_Y;
  const int x = ME_TEXT_X;
  mapedit_fmt_ints(line, sizeof line, me_str(ed, MAPEDIT_DS_SIZE), ed->map.width, ed->map.height);
  font_draw_text(f, fb, x, y, line, ink);
  y += fh + 1;
  mapedit_fmt_ints(line, sizeof line, me_str(ed, MAPEDIT_DS_CURS), ed->cur_x, ed->cur_y);
  font_draw_text(f, fb, x, y, line, ink);
  y += fh + 6;
  font_draw_text(f, fb, x, y, me_str(ed, MAPEDIT_DS_TERRAIN_AT), ink);
  y += fh + 1;
  const uint8_t b = ed->map.terrain[ed->cur_y * ed->map.width + ed->cur_x];
  /* FUN_19b7_0006 + FUN_1000_0842: "(Name[ Forest])", then the river line. */
  const int name_idx = (b & 0x20) ? ((b & 0x80) ? 0x1b : 0x1c) : (b & 0x1f);
  snprintf(line, sizeof line, "(%s%s%s)", name_idx < 29 ? ed->terrain_names[name_idx] : "",
           (name_idx > 7 && name_idx < 0x18) ? " " : "", (name_idx > 7 && name_idx < 0x18) ? ed->other_names[0] : "");
  font_draw_text(f, fb, x, y, line, ink);
  y += fh + 1;
  if (b & 0x40) {
    snprintf(line, sizeof line, "(%s)", ed->other_names[(b & 0x80) ? 2 : 3]);
    font_draw_text(f, fb, x, y, line, ink);
    y += fh + 1;
  }
  y += 10;
  font_draw_text(f, fb, x, y, me_str(ed, MAPEDIT_DS_SELECTED), ink);
  y += fh + 1;
  /* The swatch (FUN_1000_094e). */
  if (!ed->brush_feature) {
    const int bt = ed->brush_type;
    const int base = (bt < 8 || bt == 0x11 || bt > 0x17) ? bt : (bt & 7);
    if (ed->terrain_ok && base >= 0) {
      ss_blit_sprite(&ed->terrain, map_terrain_index_to_sprite(base), fb, x, y);
    }
    if (ed->phys0_ok && base != bt) {
      ss_blit_sprite(&ed->phys0, 0x41 - 1, fb, x, y);
    }
  } else if (ed->phys0_ok) {
    int spr = 0;
    if (ed->brush_or & 0x40) {
      spr = (ed->brush_or & 0x80) ? 4 : 0x14;
    } else if (ed->brush_or & 0x20) {
      spr = (ed->brush_or & 0x80) ? 0x24 : 0x34;
    }
    if (spr) {
      ss_blit_sprite(&ed->phys0, spr - 1, fb, x, y);
    }
  }
  y += 0x14;
  const char* l1;
  const char* l2;
  if (ed->brush_or & 0x20) {
    snprintf(line, sizeof line, "(%s)", ed->terrain_names[(ed->brush_or & 0x80) ? 0x1b : 0x1c]);
    l1 = me_str(ed, MAPEDIT_DS_ADD);
    l2 = me_str(ed, MAPEDIT_DS_REMOVE);
  } else if (ed->brush_or & 0x40) {
    snprintf(line, sizeof line, "(%s)", ed->other_names[(ed->brush_or & 0x80) ? 2 : 3]);
    l1 = me_str(ed, MAPEDIT_DS_ADD);
    l2 = me_str(ed, MAPEDIT_DS_REMOVE);
  } else {
    const int bt = ed->brush_type;
    snprintf(line, sizeof line, "(%s%s%s)", (bt >= 0 && bt < 29) ? ed->terrain_names[bt] : "",
             (bt > 7 && bt < 0x18) ? " " : "", (bt > 7 && bt < 0x18) ? ed->other_names[0] : "");
    l1 = me_str(ed, MAPEDIT_DS_PAINT);
    l2 = me_str(ed, MAPEDIT_DS_PICKUP);
  }
  font_draw_text(f, fb, x, y, line, ink);
  y += fh + 5;
  font_draw_text(f, fb, x, y, l1, ink);
  y += fh + 1;
  font_draw_text(f, fb, x, y, l2, ink);
  y += 0x14;
  mapedit_fmt_ints(line, sizeof line, me_str(ed, MAPEDIT_DS_FILL), ed->fill_radius, 0);
  font_draw_text(f, fb, x, y, line, ink);
  y += fh + 1;
  font_draw_text(f, fb, x, y, me_str(ed, ed->coast_protect ? MAPEDIT_DS_COAST_ON : MAPEDIT_DS_COAST_OFF), ink);
}

/* FUN_1000_0e14. */
static void me_render_palette(const MapEditor* ed, ColonizeFramebuffer8* fb) {
  fb_fill_rect(fb, 0, 0, 320, 200, 0);
  for (int idx = 1; idx <= 23; ++idx) {
    int row = 0;
    int col = 0;
    me_hotspot(idx, &row, &col);
    const int sx = col * 0x11 + 1;
    const int sy = row < 3 ? row * 0x11 + 1 : 0x30;
    if (row < 3) {
      const int type = (row == 1 && col == 1) ? 0x11 : col;
      if (ed->terrain_ok) {
        ss_blit_sprite(&ed->terrain, map_terrain_index_to_sprite(type), fb, sx, sy);
      }
      if (row == 1 && col != 1 && ed->phys0_ok) {
        ss_blit_sprite(&ed->phys0, 0x41 - 1, fb, sx, sy);
      }
    } else if (col < 3) {
      if (ed->terrain_ok) {
        ss_blit_sprite(&ed->terrain, map_terrain_index_to_sprite(0x18 + col), fb, sx, sy);
      }
    } else if (ed->phys0_ok) {
      static const int k_spr[] = {4, 0x14, 0x24, 0x34};
      ss_blit_sprite(&ed->phys0, k_spr[col - 3] - 1, fb, sx, sy);
    }
    if (idx == ed->palette_idx) {
      fb_rect_outline(fb, sx - 1, sy - 1, 18, 18, 0xf, 0xf);
    }
  }
  if (ed->font_tiny_ok) {
    char line[64];
    if (ed->brush_or & 0x20) {
      snprintf(line, sizeof line, "(%s)", ed->terrain_names[(ed->brush_or & 0x80) ? 0x1b : 0x1c]);
    } else if (ed->brush_or & 0x40) {
      snprintf(line, sizeof line, "(%s)", ed->other_names[(ed->brush_or & 0x80) ? 2 : 3]);
    } else {
      const int bt = ed->brush_type;
      snprintf(line, sizeof line, "(%s%s%s)", (bt >= 0 && bt < 29) ? ed->terrain_names[bt] : "",
               (bt > 7 && bt < 0x18) ? " " : "", (bt > 7 && bt < 0x18) ? ed->other_names[0] : "");
    }
    font_draw_text(&ed->font_tiny, fb, 0xa0, 10, line, COLONIZE_COL_BASIC);
  }
}

void mapedit_render(MapEditor* ed, ColonizeFramebuffer8* fb) {
  if (!ed || !fb || !fb->pixels) {
    return;
  }
  fb_fill_rect(fb, 0, 0, fb->width, fb->height, 0);
  ColonizePopupColors cols;
  popup_colors_from_ui(&cols);
  if (ed->map_ok) {
    if (ed->palette_open) {
      me_render_palette(ed, fb);
    } else if (ed->continents_open) {
      /* FUN_1000_1670: layer 3, low nibble, water 0, one pixel a tile at the origin. */
      for (int y = 0; y < ed->map.height; ++y) {
        for (int x = 0; x < ed->map.width; ++x) {
          const int i = y * ed->map.width + x;
          const int t = ed->map.terrain[i] & 0x1f;
          fb_put(fb, x, y, me_is_water(t) ? 0 : (uint8_t)(ed->map.layer3[i] & 0xf));
        }
      }
    } else {
      me_render_view(ed, fb);
      me_render_side(ed, fb);
      map_menu_render(&ed->menu, ed->font_tiny_ok ? &ed->font_tiny : NULL,
                      ed->woodtile_ok ? &ed->woodtile : NULL, fb);
    }
  }
  const ColonizeFont* dlg = ed->font_intr_ok ? &ed->font_intr : (ed->font_tiny_ok ? &ed->font_tiny : NULL);
  const ColonizeFont* popup_font = (ed->popup_small && ed->font_tiny_ok) ? &ed->font_tiny : dlg;
  ai_popup_render(&ed->popups, popup_font, ed->font_tiny_ok ? &ed->font_tiny : NULL,
                  ed->woodtile_ok ? &ed->woodtile : NULL, &cols, COLONIZE_COL_BASIC,
                  COLONIZE_COL_HILITE, COLONIZE_COL_SELECT, fb);
  if (ed->name_entry.open) {
    name_entry_render(&ed->name_entry, dlg, ed->woodtile_ok ? &ed->woodtile : NULL, &cols,
                      COLONIZE_COL_BASIC, COLONIZE_COL_SELECT, fb);
  }
}

/* ===================== Init ===================== */
bool mapedit_init(MapEditor* ed, const MapEditOptions* opt) {
  if (!ed) {
    return false;
  }
  memset(ed, 0, sizeof(*ed));
  if (!assets_resolve_data_dir(opt && opt->data_dir ? opt->data_dir : "./COLONIZE", ed->data_dir,
                               sizeof ed->data_dir)) {
    str_copy_trunc(ed->data_dir, sizeof ed->data_dir, opt && opt->data_dir ? opt->data_dir : ".");
  }
  ed->exe = mapedit_exe_load(ed->data_dir, &ed->exe_size);
  if (!assets_load_palette(ed->data_dir, &ed->palette)) {
    ed->exit_code = 0x13;
  }
  ed->font_intr_ok = me_load_font(ed, "FONTINTR.FF", &ed->font_intr);
  ed->font_tiny_ok = me_load_font(ed, "FONTTINY.FF", &ed->font_tiny);
  ed->terrain_ok = ss_load_asset(ed->data_dir, "TERRAIN.SS", &ed->terrain, "mapedit");
  ed->phys0_ok = ss_load_asset(ed->data_dir, "PHYS0.SS", &ed->phys0, "mapedit");
  ed->icons_ok = ss_load_asset(ed->data_dir, "ICONS.SS", &ed->icons, "mapedit");
  ed->woodtile_ok = ss_load_asset(ed->data_dir, "WOODTILE.SS", &ed->woodtile, "mapedit");
  ed->cursor_ok = ss_load_asset(ed->data_dir, "CURSOR.SS", &ed->cursor, "mapedit");
  ed->mapedit_txt_ok = me_load_txt(ed, "MAPEDIT.TXT", &ed->mapedit_txt);
  ed->mapmenu_txt_ok = me_load_txt(ed, "MAPMENU.TXT", &ed->mapmenu_txt);
  ed->names_txt_ok = me_load_txt(ed, "NAMES.TXT", &ed->names_txt);
  if (!ed->terrain_ok || !ed->phys0_ok) {
    ed->exit_code = ed->terrain_ok ? 0x18 : 0x321; /* FUN_1000_2516 error codes */
    return false;
  }
  me_load_names(ed);
  me_build_minimap_colors(ed);
  me_build_menu(ed);
  ai_popup_init(&ed->popups);
  name_entry_init(&ed->name_entry);
  /* Brush start values: DS:0x56 = 0x19 (Ocean), 0x57 = 0x40, 0x58 = 0, 0x59 = 0. */
  ed->brush_type = 0x19;
  ed->brush_and = 0x40;
  ed->palette_idx = 1; /* DS:0x1ce */
  ed->press_region = -1;
  /* FUN_1000_247a. */
  if (opt && opt->map_file && opt->map_file[0]) {
    if (!me_load(ed, opt->map_file)) {
      ed->exit_code = 1;
      return false;
    }
    me_recenter_new(ed);
  } else if (opt && opt->force_new) {
    me_name_entry(ed, "NEWNAME", NAME_ENTRY_KIND_MAP_NEW, MAPEDIT_NAME_NEW_START, me_str(ed, MAPEDIT_DS_UNTITLED));
  } else {
    me_open_picker(ed, "MAPTOEDIT", MAPEDIT_PENDING_PICK_START, true);
  }
  return true;
}

void mapedit_free(MapEditor* ed) {
  if (!ed) {
    return;
  }
  map_free(&ed->map);
  ss_free(&ed->terrain);
  ss_free(&ed->phys0);
  ss_free(&ed->icons);
  ss_free(&ed->woodtile);
  ss_free(&ed->cursor);
  if (ed->font_intr_ok) {
    font_file_free(&ed->font_intr);
  }
  if (ed->font_tiny_ok) {
    font_file_free(&ed->font_tiny);
  }
  assets_msg_free(&ed->mapedit_txt);
  assets_msg_free(&ed->mapmenu_txt);
  assets_msg_free(&ed->names_txt);
  map_menu_free(&ed->menu);
  free(ed->exe);
  ed->exe = NULL;
}
