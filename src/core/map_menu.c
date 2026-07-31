#include "core/map_menu.h"

#include <stdio.h>
#include <string.h>

#include "core/map_panel.h"
#include "core/ss.h"
#include "core/ui_colors.h"
#include "platform/diagnostics.h"

/* Menu bar / dropdown chrome under the map palette. */
enum {
  MAP_MENU_COL_BAR = 6,
  MAP_MENU_COL_TITLE = COLONIZE_COL_BASIC,
  MAP_MENU_COL_TITLE_ACTIVE = COLONIZE_COL_HILITE,
  MAP_MENU_COL_HOTKEY = COLONIZE_COL_HILITE, /* darker yellow / @COLORS hilite */
  MAP_MENU_COL_PANEL = 4,
  MAP_MENU_COL_BORDER = 15,
  MAP_MENU_COL_RULE = 0, /* black separator under the bar */
  MAP_MENU_COL_ITEM = COLONIZE_COL_BASIC,
  MAP_MENU_COL_ITEM_DISABLED = COLONIZE_COL_GREY,
  MAP_MENU_COL_HOVER = 1
};

static void map_menu_strip_all_markers(char* text) {
  char* dst = text;
  for (char* src = text; *src; ++src) {
    if (*src == '~' || *src == '#') {
      continue;
    }
    *dst++ = *src;
  }
  *dst = '\0';
}

/* Keep '~' hotkey markers for rendering; drop '#' disable/zoom tags. */
static void map_menu_strip_hash_only(char* text) {
  char* dst = text;
  for (char* src = text; *src; ++src) {
    if (*src == '#') {
      continue;
    }
    *dst++ = *src;
  }
  *dst = '\0';
}

static void map_menu_trim(char* text) {
  char* start = text;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != text) {
    memmove(text, start, strlen(start) + 1);
  }
  size_t n = strlen(text);
  while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t' || text[n - 1] == '\r')) {
    text[--n] = '\0';
  }
}

static int map_menu_text_width(const ColonizeFont* font, const char* text) {
  if (!text) {
    return 0;
  }
  int w = 0;
  for (const char* p = text; *p; ++p) {
    const unsigned char ch = (unsigned char)*p;
    if (ch == '~' || ch == '#') {
      continue;
    }
    if (font && font->section_data && ch < 128 && font->char_widths[ch] > 0) {
      w += font->char_widths[ch];
    } else {
      w += 6;
    }
  }
  return w;
}

static void map_menu_fill_rect(
  ColonizeFramebuffer8* fb,
  int x0,
  int y0,
  int x1,
  int y1,
  uint8_t color
) {
  if (!fb || !fb->pixels) {
    return;
  }
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 >= fb->width) {
    x1 = fb->width - 1;
  }
  if (y1 >= fb->height) {
    y1 = fb->height - 1;
  }
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      fb->pixels[y * fb->width + x] = color;
    }
  }
}

static void map_menu_hline(ColonizeFramebuffer8* fb, int y, int x0, int x1, uint8_t color) {
  map_menu_fill_rect(fb, x0, y, x1, y, color);
}

static void map_menu_vline(ColonizeFramebuffer8* fb, int x, int y0, int y1, uint8_t color) {
  map_menu_fill_rect(fb, x, y0, x, y1, color);
}

static MapMenuAction map_menu_classify(const char* section, const char* label) {
  if (!section || !label) {
    return MAP_MENU_ACTION_UNIMPLEMENTED;
  }

  if (strcmp(section, "GAME") == 0) {
    if (strcmp(label, "Save Game") == 0) {
      return MAP_MENU_ACTION_SAVE;
    }
    if (strcmp(label, "Load Game") == 0) {
      return MAP_MENU_ACTION_LOAD;
    }
    if (strcmp(label, "Retire") == 0) {
      return MAP_MENU_ACTION_RETIRE;
    }
    if (strcmp(label, "Exit to DOS") == 0) {
      return MAP_MENU_ACTION_EXIT;
    }
    return MAP_MENU_ACTION_UNIMPLEMENTED;
  }

  if (strcmp(section, "VIEW") == 0) {
    if (strcmp(label, "European Status") == 0) {
      return MAP_MENU_ACTION_EUROPE;
    }
    if (strcmp(label, "Find Colony") == 0) {
      return MAP_MENU_ACTION_FIND_COLONY;
    }
    if (strcmp(label, "Center View") == 0) {
      return MAP_MENU_ACTION_CENTER_VIEW;
    }
    return MAP_MENU_ACTION_UNIMPLEMENTED;
  }

  if (strcmp(section, "ORDERS") == 0) {
    if (strcmp(label, "Activate unit") == 0) {
      return MAP_MENU_ACTION_ACTIVATE_UNIT;
    }
    if (strcmp(label, "Wait for next unit") == 0) {
      return MAP_MENU_ACTION_WAIT_UNIT;
    }
    if (strcmp(label, "Build Colony") == 0) {
      return MAP_MENU_ACTION_BUILD_COLONY;
    }
    if (strcmp(label, "Join Colony (B)") == 0) {
      return MAP_MENU_ACTION_JOIN_COLONY;
    }
    if (strcmp(label, "Load Cargo") == 0) {
      return MAP_MENU_ACTION_LOAD_CARGO;
    }
    if (strcmp(label, "Unload Cargo") == 0) {
      return MAP_MENU_ACTION_UNLOAD_CARGO;
    }
    if (strcmp(label, "Return to Europe") == 0) {
      return MAP_MENU_ACTION_RETURN_EUROPE;
    }
    if (strcmp(label, "No Orders (space bar)") == 0) {
      return MAP_MENU_ACTION_NO_ORDERS;
    }
    return MAP_MENU_ACTION_UNIMPLEMENTED;
  }

  if (strcmp(section, "PEDIA") == 0) {
    if (strcmp(label, "---") == 0 || strcmp(label, "-") == 0) {
      return MAP_MENU_ACTION_SEPARATOR;
    }
    if (strcmp(label, "Cargo Types") == 0) {
      return MAP_MENU_ACTION_PEDIA_CARGO;
    }
    if (strcmp(label, "Unit Types") == 0) {
      return MAP_MENU_ACTION_PEDIA_UNIT;
    }
    if (strcmp(label, "Terrain Types") == 0) {
      return MAP_MENU_ACTION_PEDIA_TERRAIN;
    }
    if (strcmp(label, "Colonist Skills") == 0) {
      return MAP_MENU_ACTION_PEDIA_JOB;
    }
    if (strcmp(label, "Colony Buildings") == 0) {
      return MAP_MENU_ACTION_PEDIA_BUILDING;
    }
    if (strcmp(label, "Founding Fathers") == 0) {
      return MAP_MENU_ACTION_PEDIA_FATHER;
    }
    if (strcmp(label, "Miscellaneous") == 0) {
      return MAP_MENU_ACTION_PEDIA_MISC;
    }
    return MAP_MENU_ACTION_UNIMPLEMENTED;
  }

  if (strcmp(section, "REPORTS") == 0) {
    if (strstr(label, "Terrain Information")) {
      return MAP_MENU_ACTION_REPORT_TERRAIN;
    }
    if (strstr(label, "Religious Adviser")) {
      return MAP_MENU_ACTION_REPORT_RELIGIOUS;
    }
    if (strstr(label, "Continental Congress")) {
      return MAP_MENU_ACTION_REPORT_CONGRESS;
    }
    if (strstr(label, "Labor Adviser")) {
      return MAP_MENU_ACTION_REPORT_LABOR;
    }
    if (strstr(label, "Economic Adviser")) {
      return MAP_MENU_ACTION_REPORT_ECONOMIC;
    }
    if (strstr(label, "Colony Adviser")) {
      return MAP_MENU_ACTION_REPORT_COLONY;
    }
    if (strstr(label, "Naval Adviser")) {
      return MAP_MENU_ACTION_REPORT_NAVAL;
    }
    if (strstr(label, "Foreign Affairs")) {
      return MAP_MENU_ACTION_REPORT_FOREIGN;
    }
    if (strstr(label, "Indian Adviser")) {
      return MAP_MENU_ACTION_REPORT_INDIAN;
    }
    if (strstr(label, "Colonization Score")) {
      return MAP_MENU_ACTION_REPORT_SCORE;
    }
    return MAP_MENU_ACTION_UNIMPLEMENTED;
  }

  /* TRADE, CUP — screens / features not wired yet. */
  return MAP_MENU_ACTION_UNIMPLEMENTED;
}

static bool map_menu_action_enabled(MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_SAVE:
    case MAP_MENU_ACTION_LOAD:
    case MAP_MENU_ACTION_RETIRE:
    case MAP_MENU_ACTION_EXIT:
    case MAP_MENU_ACTION_EUROPE:
    case MAP_MENU_ACTION_FIND_COLONY:
    case MAP_MENU_ACTION_CENTER_VIEW:
    case MAP_MENU_ACTION_ACTIVATE_UNIT:
    case MAP_MENU_ACTION_WAIT_UNIT:
    case MAP_MENU_ACTION_BUILD_COLONY:
    case MAP_MENU_ACTION_JOIN_COLONY:
    case MAP_MENU_ACTION_LOAD_CARGO:
    case MAP_MENU_ACTION_UNLOAD_CARGO:
    case MAP_MENU_ACTION_RETURN_EUROPE:
    case MAP_MENU_ACTION_NO_ORDERS:
    case MAP_MENU_ACTION_PEDIA_CARGO:
    case MAP_MENU_ACTION_PEDIA_UNIT:
    case MAP_MENU_ACTION_PEDIA_TERRAIN:
    case MAP_MENU_ACTION_PEDIA_JOB:
    case MAP_MENU_ACTION_PEDIA_BUILDING:
    case MAP_MENU_ACTION_PEDIA_FATHER:
    case MAP_MENU_ACTION_PEDIA_MISC:
    case MAP_MENU_ACTION_REPORT_TERRAIN:
    case MAP_MENU_ACTION_REPORT_RELIGIOUS:
    case MAP_MENU_ACTION_REPORT_CONGRESS:
    case MAP_MENU_ACTION_REPORT_LABOR:
    case MAP_MENU_ACTION_REPORT_ECONOMIC:
    case MAP_MENU_ACTION_REPORT_COLONY:
    case MAP_MENU_ACTION_REPORT_NAVAL:
    case MAP_MENU_ACTION_REPORT_FOREIGN:
    case MAP_MENU_ACTION_REPORT_INDIAN:
    case MAP_MENU_ACTION_REPORT_SCORE:
      return true;
    default:
      return false;
  }
}

void map_menu_init(MapMenuBar* bar) {
  if (!bar) {
    return;
  }
  memset(bar, 0, sizeof(*bar));
  bar->open_index = -1;
  bar->hover_item = -1;
}

void map_menu_free(MapMenuBar* bar) {
  map_menu_init(bar);
}

bool map_menu_load(MapMenuBar* bar, const ColonizeMsgCatalog* menu_txt) {
  if (!bar) {
    return false;
  }
  map_menu_init(bar);
  if (!menu_txt) {
    return false;
  }

  /* Display order on the DOS map bar (CHEAT omitted until unlocked). */
  static const char* k_sections[] = {"GAME", "VIEW", "ORDERS", "REPORTS", "TRADE", "PEDIA"};
  static const char* k_cheat = "CUP";

  for (size_t s = 0; s < sizeof(k_sections) / sizeof(k_sections[0]); ++s) {
    const ColonizeMsgSection* sec = assets_msg_find(menu_txt, k_sections[s]);
    if (!sec || sec->line_count < 1 || bar->menu_count >= MAP_MENU_MAX_MENUS) {
      continue;
    }
    MapMenuPulldown* menu = &bar->menus[bar->menu_count];
    snprintf(menu->section_name, sizeof(menu->section_name), "%s", k_sections[s]);

    char title[MAP_MENU_TITLE_LEN];
    snprintf(title, sizeof(title), "%s", sec->lines[0]);
    map_menu_strip_hash_only(title);
    map_menu_trim(title);
    snprintf(menu->title, sizeof(menu->title), "%s", title);

    for (int i = 1; i < sec->line_count && menu->item_count < MAP_MENU_MAX_ITEMS; ++i) {
      char label[MAP_MENU_LABEL_LEN];
      snprintf(label, sizeof(label), "%s", sec->lines[i]);
      map_menu_strip_hash_only(label);
      map_menu_trim(label);
      if (label[0] == '\0') {
        continue;
      }
      char classify_label[MAP_MENU_LABEL_LEN];
      snprintf(classify_label, sizeof(classify_label), "%s", label);
      map_menu_strip_all_markers(classify_label);
      if (classify_label[0] == '\0') {
        continue;
      }
      MapMenuItem* item = &menu->items[menu->item_count++];
      snprintf(item->label, sizeof(item->label), "%s", label);
      item->action = map_menu_classify(menu->section_name, classify_label);
      item->separator = (item->action == MAP_MENU_ACTION_SEPARATOR);
      item->enabled = !item->separator && map_menu_action_enabled(item->action);
    }
    bar->menu_count++;
  }

  /* Keep CHEAT data available for later unlock without showing it. */
  (void)k_cheat;
  (void)assets_msg_find(menu_txt, k_cheat);

  /* Lay out title hit-boxes; PEDIA is placed later above the right panel. */
  int x = 12;
  for (int i = 0; i < bar->menu_count; ++i) {
    MapMenuPulldown* menu = &bar->menus[i];
    if (strcmp(menu->section_name, "PEDIA") == 0) {
      continue;
    }
    const int tw = (int)strlen(menu->title) * 6; /* approx; refined at render with font */
    menu->title_x = x;
    menu->title_w = tw + 8;
    x += menu->title_w + 4;
  }
  for (int i = 0; i < bar->menu_count; ++i) {
    MapMenuPulldown* menu = &bar->menus[i];
    if (strcmp(menu->section_name, "PEDIA") != 0) {
      continue;
    }
    const int tw = (int)strlen(menu->title) * 6;
    menu->title_w = tw + 8;
    menu->title_x = MAP_PANEL_X;
  }

  bar->loaded = bar->menu_count > 0;
  if (bar->loaded) {
    diag_info("Map menu bar loaded (%d menus from MENU.TXT)", bar->menu_count);
  } else {
    diag_warn("MENU.TXT produced no map menus");
  }
  return bar->loaded;
}

static void map_menu_layout_titles(MapMenuBar* bar, const ColonizeFont* font) {
  /* 8px right of the old origin (4); COLONIZOPEDIA sits above the minimap strip. */
  int x = 12;
  int pedia = -1;
  for (int i = 0; i < bar->menu_count; ++i) {
    MapMenuPulldown* menu = &bar->menus[i];
    if (strcmp(menu->section_name, "PEDIA") == 0) {
      pedia = i;
      continue;
    }
    const int tw = map_menu_text_width(font, menu->title);
    menu->title_x = x;
    menu->title_w = tw + 8;
    x += menu->title_w + 6;
  }
  if (pedia >= 0) {
    MapMenuPulldown* menu = &bar->menus[pedia];
    const int tw = map_menu_text_width(font, menu->title);
    menu->title_w = tw + 8;
    menu->title_x = MAP_PANEL_X;
    if (menu->title_x + menu->title_w > 320) {
      menu->title_x = 320 - menu->title_w;
      if (menu->title_x < MAP_PANEL_X) {
        menu->title_x = MAP_PANEL_X;
      }
    }
  }
}

static int map_menu_dropdown_width(const MapMenuPulldown* menu, const ColonizeFont* font) {
  int max_w = map_menu_text_width(font, menu->title) + 12;
  for (int i = 0; i < menu->item_count; ++i) {
    const int w = map_menu_text_width(font, menu->items[i].label) + 12;
    if (w > max_w) {
      max_w = w;
    }
  }
  if (max_w < 80) {
    max_w = 80;
  }
  if (max_w > 200) {
    max_w = 200;
  }
  return max_w;
}

static int map_menu_item_height(const ColonizeFont* font) {
  const int h = font ? (font->max_height + 2) : 8;
  return h < 8 ? 8 : h;
}

static void map_menu_dropdown_rect(
  const MapMenuBar* bar,
  const ColonizeFont* font,
  int menu_index,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h
) {
  const MapMenuPulldown* menu = &bar->menus[menu_index];
  const int item_h = map_menu_item_height(font);
  int x = menu->title_x;
  int w = map_menu_dropdown_width(menu, font);
  int h = 2 + menu->item_count * item_h + 2;
  if (x + w > 318) {
    x = 318 - w;
  }
  if (x < 1) {
    x = 1;
  }
  if (out_x) {
    *out_x = x;
  }
  if (out_y) {
    *out_y = MAP_MENU_BAR_H;
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
}

bool map_menu_hit_ui(const MapMenuBar* bar, int x, int y) {
  if (!bar || !bar->loaded) {
    return false;
  }
  if (y >= 0 && y < MAP_MENU_BAR_H) {
    return true;
  }
  if (bar->open_index < 0 || bar->open_index >= bar->menu_count) {
    return false;
  }
  int dx, dy, dw, dh;
  map_menu_dropdown_rect(bar, NULL, bar->open_index, &dx, &dy, &dw, &dh);
  return x >= dx && x < dx + dw && y >= dy && y < dy + dh;
}

static int map_menu_title_at(const MapMenuBar* bar, int x, int y) {
  if (y < 0 || y >= MAP_MENU_BAR_H) {
    return -1;
  }
  for (int i = 0; i < bar->menu_count; ++i) {
    const MapMenuPulldown* m = &bar->menus[i];
    if (x >= m->title_x && x < m->title_x + m->title_w) {
      return i;
    }
  }
  return -1;
}

static int map_menu_item_at(
  const MapMenuBar* bar,
  const ColonizeFont* font,
  int x,
  int y
) {
  if (bar->open_index < 0) {
    return -1;
  }
  int dx, dy, dw, dh;
  map_menu_dropdown_rect(bar, font, bar->open_index, &dx, &dy, &dw, &dh);
  if (x < dx || x >= dx + dw || y < dy || y >= dy + dh) {
    return -1;
  }
  const int item_h = map_menu_item_height(font);
  const int idx = (y - dy - 2) / item_h;
  if (idx < 0 || idx >= bar->menus[bar->open_index].item_count) {
    return -1;
  }
  return idx;
}

MapMenuAction map_menu_handle_input(
  MapMenuBar* bar,
  const ColonizeInputState* input,
  const ColonizeFont* font,
  bool close_request
) {
  if (!bar || !bar->loaded || !input) {
    return MAP_MENU_ACTION_NONE;
  }

  if (close_request) {
    bar->open_index = -1;
    bar->hover_item = -1;
    return MAP_MENU_ACTION_NONE;
  }

  map_menu_layout_titles(bar, font);

  if (bar->open_index >= 0) {
    bar->hover_item = map_menu_item_at(bar, font, input->mouse_x, input->mouse_y);
  } else {
    bar->hover_item = -1;
  }

  if (!input->mouse_left_clicked) {
    return MAP_MENU_ACTION_NONE;
  }

  const int title = map_menu_title_at(bar, input->mouse_x, input->mouse_y);
  if (title >= 0) {
    if (bar->open_index == title) {
      bar->open_index = -1;
      bar->hover_item = -1;
    } else {
      bar->open_index = title;
      bar->hover_item = -1;
    }
    return MAP_MENU_ACTION_NONE;
  }

  if (bar->open_index >= 0) {
    const int item = map_menu_item_at(bar, font, input->mouse_x, input->mouse_y);
    if (item >= 0) {
      const MapMenuItem* mi = &bar->menus[bar->open_index].items[item];
      const MapMenuAction action = mi->action;
      bar->open_index = -1;
      bar->hover_item = -1;
      if (!mi->enabled || mi->separator || action == MAP_MENU_ACTION_SEPARATOR) {
        return MAP_MENU_ACTION_NONE;
      }
      return action;
    }
    /* Click outside dropdown closes it. */
    bar->open_index = -1;
    bar->hover_item = -1;
    return MAP_MENU_ACTION_NONE;
  }

  return MAP_MENU_ACTION_NONE;
}

static void map_menu_tile_wood(
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

void map_menu_render(
  MapMenuBar* bar,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  ColonizeFramebuffer8* framebuffer
) {
  if (!bar || !bar->loaded || !framebuffer || !framebuffer->pixels) {
    return;
  }

  map_menu_layout_titles(bar, font);

  if (wood_tile && wood_tile->sprite_count > 0) {
    map_menu_tile_wood(wood_tile, 0, 0, framebuffer->width, MAP_MENU_BAR_H, framebuffer);
  } else {
    map_menu_fill_rect(framebuffer, 0, 0, framebuffer->width - 1, MAP_MENU_BAR_H - 1, MAP_MENU_COL_BAR);
  }
  /* Black rule under the menu bar (full width; separates bar from map + minimap). */
  map_menu_hline(framebuffer, MAP_MENU_BAR_H - 1, 0, framebuffer->width - 1, MAP_MENU_COL_RULE);

  for (int i = 0; i < bar->menu_count; ++i) {
    const MapMenuPulldown* menu = &bar->menus[i];
    const uint8_t color =
      (i == bar->open_index) ? MAP_MENU_COL_TITLE_ACTIVE : MAP_MENU_COL_TITLE;
    font_draw_text_hotkey(
      font, framebuffer, menu->title_x + 2, 1, menu->title, color, MAP_MENU_COL_HOTKEY
    );
  }

  if (bar->open_index < 0 || bar->open_index >= bar->menu_count) {
    return;
  }

  const MapMenuPulldown* open = &bar->menus[bar->open_index];
  int dx, dy, dw, dh;
  map_menu_dropdown_rect(bar, font, bar->open_index, &dx, &dy, &dw, &dh);
  map_menu_fill_rect(framebuffer, dx, dy, dx + dw - 1, dy + dh - 1, MAP_MENU_COL_PANEL);
  map_menu_hline(framebuffer, dy, dx, dx + dw - 1, MAP_MENU_COL_BORDER);
  map_menu_hline(framebuffer, dy + dh - 1, dx, dx + dw - 1, MAP_MENU_COL_BORDER);
  map_menu_vline(framebuffer, dx, dy, dy + dh - 1, MAP_MENU_COL_BORDER);
  map_menu_vline(framebuffer, dx + dw - 1, dy, dy + dh - 1, MAP_MENU_COL_BORDER);

  const int item_h = map_menu_item_height(font);
  for (int i = 0; i < open->item_count; ++i) {
    const int iy = dy + 2 + i * item_h;
    if (open->items[i].separator) {
      const int mid = iy + item_h / 2;
      map_menu_hline(framebuffer, mid, dx + 4, dx + dw - 5, MAP_MENU_COL_BORDER);
      continue;
    }
    if (i == bar->hover_item && open->items[i].enabled) {
      map_menu_fill_rect(framebuffer, dx + 1, iy, dx + dw - 2, iy + item_h - 1, MAP_MENU_COL_HOVER);
    }
    const uint8_t color =
      open->items[i].enabled ? MAP_MENU_COL_ITEM : MAP_MENU_COL_ITEM_DISABLED;
    font_draw_text_hotkey(
      font, framebuffer, dx + 4, iy, open->items[i].label, color, MAP_MENU_COL_HOTKEY
    );
  }
}

const char* map_menu_action_name(MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_NONE:
      return "none";
    case MAP_MENU_ACTION_SEPARATOR:
      return "separator";
    case MAP_MENU_ACTION_UNIMPLEMENTED:
      return "unimplemented";
    case MAP_MENU_ACTION_SAVE:
      return "Save Game";
    case MAP_MENU_ACTION_LOAD:
      return "Load Game";
    case MAP_MENU_ACTION_RETIRE:
      return "Retire";
    case MAP_MENU_ACTION_EXIT:
      return "Exit to DOS";
    case MAP_MENU_ACTION_EUROPE:
      return "European Status";
    case MAP_MENU_ACTION_FIND_COLONY:
      return "Find Colony";
    case MAP_MENU_ACTION_CENTER_VIEW:
      return "Center View";
    case MAP_MENU_ACTION_ACTIVATE_UNIT:
      return "Activate unit";
    case MAP_MENU_ACTION_WAIT_UNIT:
      return "Wait for next unit";
    case MAP_MENU_ACTION_BUILD_COLONY:
      return "Build Colony";
    case MAP_MENU_ACTION_JOIN_COLONY:
      return "Join Colony";
    case MAP_MENU_ACTION_LOAD_CARGO:
      return "Load Cargo";
    case MAP_MENU_ACTION_UNLOAD_CARGO:
      return "Unload Cargo";
    case MAP_MENU_ACTION_RETURN_EUROPE:
      return "Return to Europe";
    case MAP_MENU_ACTION_NO_ORDERS:
      return "No Orders";
    case MAP_MENU_ACTION_PEDIA_CARGO:
      return "Cargo Types";
    case MAP_MENU_ACTION_PEDIA_UNIT:
      return "Unit Types";
    case MAP_MENU_ACTION_PEDIA_TERRAIN:
      return "Terrain Types";
    case MAP_MENU_ACTION_PEDIA_JOB:
      return "Colonist Skills";
    case MAP_MENU_ACTION_PEDIA_BUILDING:
      return "Colony Buildings";
    case MAP_MENU_ACTION_PEDIA_FATHER:
      return "Founding Fathers";
    case MAP_MENU_ACTION_PEDIA_MISC:
      return "Miscellaneous";
    case MAP_MENU_ACTION_REPORT_TERRAIN:
      return "Terrain Information";
    case MAP_MENU_ACTION_REPORT_RELIGIOUS:
      return "Religious Adviser";
    case MAP_MENU_ACTION_REPORT_CONGRESS:
      return "Continental Congress";
    case MAP_MENU_ACTION_REPORT_LABOR:
      return "Labor Adviser";
    case MAP_MENU_ACTION_REPORT_ECONOMIC:
      return "Economic Adviser";
    case MAP_MENU_ACTION_REPORT_COLONY:
      return "Colony Adviser";
    case MAP_MENU_ACTION_REPORT_NAVAL:
      return "Naval Adviser";
    case MAP_MENU_ACTION_REPORT_FOREIGN:
      return "Foreign Affairs Advisor";
    case MAP_MENU_ACTION_REPORT_INDIAN:
      return "Indian Adviser";
    case MAP_MENU_ACTION_REPORT_SCORE:
      return "Colonization Score";
    default:
      return "unknown";
  }
}
