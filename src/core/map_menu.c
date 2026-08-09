#include "core/map_menu.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "core/map_panel.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/ui_colors.h"
#include "platform/diagnostics.h"

/* Menu bar / dropdown chrome under the map palette. */
enum {
  MAP_MENU_COL_BAR = 6,
  MAP_MENU_COL_TITLE = COLONIZE_COL_BASIC,
  MAP_MENU_COL_TITLE_ACTIVE = COLONIZE_COL_HILITE,
  MAP_MENU_COL_HOTKEY = COLONIZE_COL_HILITE, /* darker yellow / @COLORS hilite */
  MAP_MENU_COL_PANEL = 4, /* solid fallback when WOODTILE unavailable */
  MAP_MENU_COL_BORDER = 0, /* black outline around pull-down */
  MAP_MENU_COL_RULE = 0, /* black separator under the bar */
  MAP_MENU_COL_RULE_ITEM = COLONIZE_COL_BASIC, /* green item divider (PEDIA ---) */
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

/* Parse ~ hotkeys from MENU.TXT (space / Shift+D chords, else first ~letter). */
static void map_menu_parse_hotkey(const char* label_raw, MapMenuItem* item) {
  item->hotkey = 0;
  item->hotkey_shift = false;
  item->hotkey_space = false;
  if (!label_raw || !item) {
    return;
  }
  if (strstr(label_raw, "~s~p~a~c~e") != NULL || strstr(label_raw, "~S~P~A~C~E") != NULL) {
    item->hotkey_space = true;
    return;
  }
  if (strstr(label_raw, "~s~h~i~f~t") != NULL || strstr(label_raw, "~S~H~I~F~T") != NULL) {
    for (const char* p = label_raw; *p; ++p) {
      if (*p == '~' && p[1] && ((p[1] >= 'A' && p[1] <= 'Z') || (p[1] >= 'a' && p[1] <= 'z'))) {
        if ((p[1] == 'D' || p[1] == 'd') &&
            (p == label_raw || p[-1] == '-' || p[-1] == '~' || p[-1] == ' ')) {
          /* Prefer the trailing ~D after shift- */
          item->hotkey = 'D';
          item->hotkey_shift = true;
        }
      }
    }
    if (item->hotkey == 'D') {
      return;
    }
  }
  for (const char* p = label_raw; *p; ++p) {
    if (*p != '~' || !p[1]) {
      continue;
    }
    const unsigned char ch = (unsigned char)p[1];
    if (isalnum(ch)) {
      item->hotkey = (char)toupper(ch);
      return;
    }
  }
}

static char map_menu_parse_title_hotkey(const char* title_raw) {
  if (!title_raw) {
    return 0;
  }
  for (const char* p = title_raw; *p; ++p) {
    if (*p == '~' && p[1] && isalpha((unsigned char)p[1])) {
      return (char)toupper((unsigned char)p[1]);
    }
  }
  return 0;
}

static int map_menu_visible_item_count(const MapMenuPulldown* menu) {
  if (!menu) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < menu->item_count; ++i) {
    if (menu->items[i].visible) {
      ++n;
    }
  }
  return n;
}

/* Map visible row index → item index; -1 if out of range. */
static int map_menu_item_index_from_visible(const MapMenuPulldown* menu, int visible_row) {
  if (!menu || visible_row < 0) {
    return -1;
  }
  int row = 0;
  for (int i = 0; i < menu->item_count; ++i) {
    if (!menu->items[i].visible) {
      continue;
    }
    if (row == visible_row) {
      return i;
    }
    ++row;
  }
  return -1;
}

static int map_menu_visible_row_from_item(const MapMenuPulldown* menu, int item_index) {
  if (!menu || item_index < 0 || item_index >= menu->item_count) {
    return -1;
  }
  if (!menu->items[item_index].visible) {
    return -1;
  }
  int row = 0;
  for (int i = 0; i < item_index; ++i) {
    if (menu->items[i].visible) {
      ++row;
    }
  }
  return row;
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
    if (strcmp(label, "Pick Music") == 0) {
      return MAP_MENU_ACTION_PICK_MUSIC;
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
    if (strcmp(label, "Fortify") == 0) {
      /* First Fortify = land; second identical row = ship Anchor (GAME.TXT). */
      return MAP_MENU_ACTION_FORTIFY;
    }
    if (strcmp(label, "Sentry") == 0) {
      return MAP_MENU_ACTION_SENTRY;
    }
    if (strcmp(label, "Build Colony") == 0) {
      return MAP_MENU_ACTION_BUILD_COLONY;
    }
    if (strcmp(label, "Join Colony (B)") == 0) {
      return MAP_MENU_ACTION_JOIN_COLONY;
    }
    if (strcmp(label, "Clear Forest (P)") == 0) {
      return MAP_MENU_ACTION_CLEAR_FOREST;
    }
    if (strcmp(label, "Plow Fields  (P)") == 0 || strcmp(label, "Plow Fields (P)") == 0) {
      return MAP_MENU_ACTION_PLOW_FIELDS;
    }
    if (strcmp(label, "Build Road") == 0) {
      return MAP_MENU_ACTION_BUILD_ROAD;
    }
    if (strcmp(label, "Load Cargo") == 0) {
      return MAP_MENU_ACTION_LOAD_CARGO;
    }
    if (strcmp(label, "Unload Cargo") == 0) {
      return MAP_MENU_ACTION_UNLOAD_CARGO;
    }
    if (strcmp(label, "Pillage") == 0) {
      return MAP_MENU_ACTION_PILLAGE;
    }
    if (strcmp(label, "Go to Port") == 0) {
      return MAP_MENU_ACTION_GOTO_PORT;
    }
    if (strcmp(label, "Go to Place") == 0) {
      return MAP_MENU_ACTION_GOTO_PLACE;
    }
    if (strcmp(label, "Begin Trade Route") == 0) {
      return MAP_MENU_ACTION_TRADE_ROUTE;
    }
    if (strcmp(label, "Return to Europe") == 0) {
      return MAP_MENU_ACTION_RETURN_EUROPE;
    }
    if (strcmp(label, "No Orders (space bar)") == 0) {
      return MAP_MENU_ACTION_NO_ORDERS;
    }
    if (strcmp(label, "Dump Cargo Overboard") == 0) {
      return MAP_MENU_ACTION_DUMP_OVERBOARD;
    }
    if (strstr(label, "Disband Unit") != NULL) {
      return MAP_MENU_ACTION_DISBAND;
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

  if (strcmp(section, "CUP") == 0) {
    if (strstr(label, "Create Unit")) {
      return MAP_MENU_ACTION_CHEAT_CREATE_UNIT;
    }
    if (strstr(label, "Debug Info Flags")) {
      return MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS;
    }
    if (strstr(label, "Reveal Map")) {
      return MAP_MENU_ACTION_CHEAT_REVEAL_MAP;
    }
    if (strstr(label, "Set Human Player")) {
      return MAP_MENU_ACTION_CHEAT_SET_HUMAN;
    }
    if (strstr(label, "Kill Indians")) {
      return MAP_MENU_ACTION_CHEAT_KILL_INDIANS;
    }
    if (strstr(label, "Advance Revolution Status")) {
      return MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION;
    }
    if (strstr(label, "Sound Test")) {
      return MAP_MENU_ACTION_CHEAT_SOUND_TEST;
    }
    if (strstr(label, "Memory Check")) {
      return MAP_MENU_ACTION_CHEAT_MEMORY_CHECK;
    }
    if (strstr(label, "Show Strategy")) {
      return MAP_MENU_ACTION_CHEAT_SHOW_STRATEGY;
    }
    if (strstr(label, "Show Colony Sites")) {
      return MAP_MENU_ACTION_CHEAT_SHOW_COLONY_SITES;
    }
    if (strstr(label, "Test Routine")) {
      return MAP_MENU_ACTION_CHEAT_TEST_ROUTINE;
    }
    return MAP_MENU_ACTION_UNIMPLEMENTED;
  }

  if (strcmp(section, "DEBUG") == 0) {
    if (strcmp(label, "Sprite Viewer") == 0) {
      return MAP_MENU_ACTION_DEBUG_SPRITE_VIEWER;
    }
    if (strcmp(label, "Show Mouse Coords") == 0) {
      return MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS;
    }
    return MAP_MENU_ACTION_UNIMPLEMENTED;
  }

  /* TRADE — screens / features not wired yet. */
  return MAP_MENU_ACTION_UNIMPLEMENTED;
}

static bool map_menu_action_enabled(MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_SAVE:
    case MAP_MENU_ACTION_LOAD:
    case MAP_MENU_ACTION_RETIRE:
    case MAP_MENU_ACTION_EXIT:
    case MAP_MENU_ACTION_PICK_MUSIC:
    case MAP_MENU_ACTION_EUROPE:
    case MAP_MENU_ACTION_FIND_COLONY:
    case MAP_MENU_ACTION_CENTER_VIEW:
    case MAP_MENU_ACTION_ACTIVATE_UNIT:
    case MAP_MENU_ACTION_WAIT_UNIT:
    case MAP_MENU_ACTION_BUILD_COLONY:
    case MAP_MENU_ACTION_JOIN_COLONY:
    case MAP_MENU_ACTION_CLEAR_FOREST:
    case MAP_MENU_ACTION_PLOW_FIELDS:
    case MAP_MENU_ACTION_BUILD_ROAD:
    case MAP_MENU_ACTION_LOAD_CARGO:
    case MAP_MENU_ACTION_UNLOAD_CARGO:
    case MAP_MENU_ACTION_PILLAGE:
    case MAP_MENU_ACTION_GOTO_PORT:
    case MAP_MENU_ACTION_GOTO_PLACE:
    case MAP_MENU_ACTION_TRADE_ROUTE:
    case MAP_MENU_ACTION_RETURN_EUROPE:
    case MAP_MENU_ACTION_FORTIFY:
    case MAP_MENU_ACTION_ANCHOR:
    case MAP_MENU_ACTION_SENTRY:
    case MAP_MENU_ACTION_DISBAND:
    case MAP_MENU_ACTION_DUMP_OVERBOARD:
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
    case MAP_MENU_ACTION_DEBUG_SPRITE_VIEWER:
    case MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS:
    case MAP_MENU_ACTION_CHEAT_REVEAL_MAP:
    case MAP_MENU_ACTION_CHEAT_KILL_INDIANS:
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

static void map_menu_append_item(
  MapMenuPulldown* menu,
  const char* label_raw,
  MapMenuAction action
) {
  if (!menu || menu->item_count >= MAP_MENU_MAX_ITEMS || !label_raw) {
    return;
  }
  MapMenuItem* item = &menu->items[menu->item_count++];
  snprintf(item->label, sizeof(item->label), "%s", label_raw);
  item->action = action;
  item->separator = (action == MAP_MENU_ACTION_SEPARATOR);
  item->visible = true;
  item->enabled = !item->separator && map_menu_action_enabled(action);
  if (item->separator) {
    item->hotkey = 0;
    item->hotkey_shift = false;
    item->hotkey_space = false;
    item->label[0] = '\0';
  } else {
    map_menu_parse_hotkey(label_raw, item);
  }
}

static int map_menu_find_action_index(
  const MapMenuPulldown* menu,
  MapMenuAction action,
  int start
) {
  if (!menu) {
    return -1;
  }
  for (int i = start; i < menu->item_count; ++i) {
    if (menu->items[i].action == action) {
      return i;
    }
  }
  return -1;
}

/* Insert empty-label separator after the given item index (FUN_4b58_07d6). */
static bool map_menu_insert_separator_at(MapMenuPulldown* menu, int after_index) {
  if (!menu || after_index < 0 || after_index >= menu->item_count) {
    return false;
  }
  if (menu->item_count >= MAP_MENU_MAX_ITEMS) {
    return false;
  }
  if (after_index + 1 < menu->item_count && menu->items[after_index + 1].separator) {
    return true;
  }
  const int insert_at = after_index + 1;
  memmove(
    &menu->items[insert_at + 1],
    &menu->items[insert_at],
    (size_t)(menu->item_count - insert_at) * sizeof(menu->items[0])
  );
  MapMenuItem* sep = &menu->items[insert_at];
  memset(sep, 0, sizeof(*sep));
  sep->action = MAP_MENU_ACTION_SEPARATOR;
  sep->separator = true;
  sep->visible = true;
  sep->enabled = false;
  menu->item_count++;
  return true;
}

static void map_menu_insert_sep_after_action(MapMenuPulldown* menu, MapMenuAction after) {
  const int i = map_menu_find_action_index(menu, after, 0);
  if (i >= 0) {
    map_menu_insert_separator_at(menu, i);
  }
}

/*
 * DOS FUN_74a4_0000 inserts empty-label separators (not in MENU.TXT except PEDIA
 * --- which we skip on load and re-insert here). Cite: viceroy_unpacked.c:119308+.
 */
static void map_menu_insert_dos_separators(MapMenuPulldown* menu) {
  if (!menu) {
    return;
  }
  if (strcmp(menu->section_name, "GAME") == 0) {
    /*
     * Order: Options, Colony Report, SEP, Sound, Pick Music, SEP, Save, Load,
     * SEP, DECLARE, SEP, Retire, Exit. Cite: FUN_74a4_0000 @ 119308–119325.
     */
    if (menu->item_count >= 2) {
      map_menu_insert_separator_at(menu, 1); /* after Colony Report Options */
    }
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_PICK_MUSIC);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_LOAD);
    {
      const int retire = map_menu_find_action_index(menu, MAP_MENU_ACTION_RETIRE, 0);
      if (retire >= 1) {
        map_menu_insert_separator_at(menu, retire - 1); /* after DECLARE */
      }
    }
    return;
  }
  if (strcmp(menu->section_name, "VIEW") == 0) {
    /*
     * Move, View, Europe, SEP, Find, SEP, ZoomIn, ZoomOut, SEP,
     * four zoom levels, SEP, Hidden, Center. Cite: 119337–119358.
     */
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_EUROPE);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_FIND_COLONY);
    {
      const int find = map_menu_find_action_index(menu, MAP_MENU_ACTION_FIND_COLONY, 0);
      /* find, sep, ZoomIn, ZoomOut → sep after ZoomOut */
      if (find >= 0 && find + 3 < menu->item_count && menu->items[find + 1].separator) {
        map_menu_insert_separator_at(menu, find + 3);
      }
    }
    {
      const int center = map_menu_find_action_index(menu, MAP_MENU_ACTION_CENTER_VIEW, 0);
      /* … z15, Hidden, Center → sep after z15 (= center-2) */
      if (center >= 2) {
        map_menu_insert_separator_at(menu, center - 2);
      }
    }
    return;
  }
  if (strcmp(menu->section_name, "ORDERS") == 0) {
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_SENTRY);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_PILLAGE);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_RETURN_EUROPE);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_NO_ORDERS);
    return;
  }
  if (strcmp(menu->section_name, "REPORTS") == 0) {
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_REPORT_TERRAIN);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_REPORT_ECONOMIC);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_REPORT_INDIAN);
    return;
  }
  if (strcmp(menu->section_name, "CUP") == 0) {
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_CHEAT_SET_HUMAN);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION);
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_CHEAT_MEMORY_CHECK);
    return;
  }
  if (strcmp(menu->section_name, "PEDIA") == 0) {
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_PEDIA_TERRAIN);
    return;
  }
}

static bool map_menu_load_section(
  MapMenuBar* bar,
  const ColonizeMsgCatalog* menu_txt,
  const char* section_name,
  bool visible
) {
  if (!bar || !menu_txt || !section_name || bar->menu_count >= MAP_MENU_MAX_MENUS) {
    return false;
  }
  const ColonizeMsgSection* sec = assets_msg_find(menu_txt, section_name);
  if (!sec || sec->line_count < 1) {
    return false;
  }
  MapMenuPulldown* menu = &bar->menus[bar->menu_count];
  memset(menu, 0, sizeof(*menu));
  str_copy_trunc(menu->section_name, sizeof(menu->section_name), section_name);
  menu->visible = visible;

  char title[MAP_MENU_TITLE_LEN];
  str_copy_trunc(title, sizeof(title), sec->lines[0]);
  map_menu_strip_hash_only(title);
  map_menu_trim(title);
  str_copy_trunc(menu->title, sizeof(menu->title), title);
  menu->title_hotkey = map_menu_parse_title_hotkey(title);

  for (int i = 1; i < sec->line_count && menu->item_count < MAP_MENU_MAX_ITEMS; ++i) {
    char label[MAP_MENU_LABEL_LEN];
    str_copy_trunc(label, sizeof(label), sec->lines[i]);
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
    MapMenuAction action = map_menu_classify(section_name, classify_label);
    /* MENU.TXT lists ~Fortify twice: land (0x302) then ship (0x303). */
    if (strcmp(section_name, "ORDERS") == 0 && action == MAP_MENU_ACTION_FORTIFY) {
      for (int j = 0; j < menu->item_count; ++j) {
        if (menu->items[j].action == MAP_MENU_ACTION_FORTIFY) {
          action = MAP_MENU_ACTION_ANCHOR;
          break;
        }
      }
    }
    /* Skip MENU.TXT --- for PEDIA — DOS inserts the sep in FUN_74a4_0000. */
    if (action == MAP_MENU_ACTION_SEPARATOR) {
      continue;
    }
    map_menu_append_item(menu, label, action);
  }
  map_menu_insert_dos_separators(menu);
  bar->menu_count++;
  return true;
}

#if COLONIZE_DEBUG_MENU
static bool map_menu_load_debug(MapMenuBar* bar) {
  if (!bar || bar->menu_count >= MAP_MENU_MAX_MENUS) {
    return false;
  }
  MapMenuPulldown* menu = &bar->menus[bar->menu_count];
  memset(menu, 0, sizeof(*menu));
  str_copy_trunc(menu->section_name, sizeof(menu->section_name), "DEBUG");
  str_copy_trunc(menu->title, sizeof(menu->title), "~DEBUG");
  menu->visible = true;
  map_menu_append_item(menu, "Sprite Viewer", MAP_MENU_ACTION_DEBUG_SPRITE_VIEWER);
  map_menu_append_item(menu, "Show Mouse Coords", MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS);
  bar->menu_count++;
  return true;
}
#endif

bool map_menu_load(MapMenuBar* bar, const ColonizeMsgCatalog* menu_txt) {
  if (!bar) {
    return false;
  }
  map_menu_init(bar);
  if (!menu_txt) {
    return false;
  }

  /* Left-to-right slots; CHEAT/DEBUG keep fixed positions whether visible. */
  static const char* k_before_cheat[] = {"GAME", "VIEW", "ORDERS", "REPORTS", "TRADE"};
  for (size_t s = 0; s < sizeof(k_before_cheat) / sizeof(k_before_cheat[0]); ++s) {
    map_menu_load_section(bar, menu_txt, k_before_cheat[s], true);
  }
  map_menu_load_section(bar, menu_txt, "CUP", false);
  bar->cheat_visible = false;
#if COLONIZE_DEBUG_MENU
  map_menu_load_debug(bar);
#endif
  map_menu_load_section(bar, menu_txt, "PEDIA", true);

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
    const int inner_x0 = MAP_PANEL_X + 2;
    const int inner_w = 319 - inner_x0 + 1;
    const int mx = inner_x0 + (inner_w - MAP_PANEL_MINIMAP_W) / 2;
    menu->title_x = mx + MAP_PANEL_MINIMAP_W / 2 - tw / 2;
    if (menu->title_x < MAP_PANEL_X) {
      menu->title_x = MAP_PANEL_X;
    }
  }

  bar->loaded = bar->menu_count > 0;
  if (bar->loaded) {
    diag_info("Map menu bar loaded (%d menus from MENU.TXT)", bar->menu_count);
  } else {
    diag_warn("MENU.TXT produced no map menus");
  }
  return bar->loaded;
}

void map_menu_set_cheat_visible(MapMenuBar* bar, bool visible) {
  if (!bar) {
    return;
  }
  bar->cheat_visible = visible;
  for (int i = 0; i < bar->menu_count; ++i) {
    MapMenuPulldown* menu = &bar->menus[i];
    if (strcmp(menu->section_name, "CUP") != 0) {
      continue;
    }
    menu->visible = visible;
    if (!visible && bar->open_index == i) {
      bar->open_index = -1;
      bar->hover_item = -1;
    }
    return;
  }
}

static void map_menu_refresh_orders_dos(
  MapMenuPulldown* orders,
  const MapMenuOrdersContext* ctx
) {
  if (!orders) {
    return;
  }

  /* Reset: separators visible; items visible+enabled until rules hide/gray. */
  for (int i = 0; i < orders->item_count; ++i) {
    MapMenuItem* it = &orders->items[i];
    if (it->separator) {
      it->visible = true;
      it->enabled = false;
      continue;
    }
    it->visible = true;
    it->enabled = true;
  }

  const ColonizeUnit* u = NULL;
  const bool have_unit =
    ctx && ctx->units && ctx->selected_id >= 0 &&
    (u = units_get_const(ctx->units, ctx->selected_id)) != NULL && u->active &&
    units_is_on_map(u);

  /* FUN_2b5a_0902 View Pieces — no active map unit selected. */
  if (!have_unit) {
    for (int i = 0; i < orders->item_count; ++i) {
      MapMenuItem* it = &orders->items[i];
      if (it->separator) {
        continue;
      }
      switch (it->action) {
        case MAP_MENU_ACTION_WAIT_UNIT:
        case MAP_MENU_ACTION_NO_ORDERS:
        case MAP_MENU_ACTION_FORTIFY:
        case MAP_MENU_ACTION_SENTRY:
        case MAP_MENU_ACTION_BUILD_COLONY:
          it->enabled = false;
          break;
        case MAP_MENU_ACTION_DUMP_OVERBOARD:
        case MAP_MENU_ACTION_ANCHOR:
        case MAP_MENU_ACTION_JOIN_COLONY:
        case MAP_MENU_ACTION_CLEAR_FOREST:
        case MAP_MENU_ACTION_PLOW_FIELDS:
        case MAP_MENU_ACTION_BUILD_ROAD:
        case MAP_MENU_ACTION_LOAD_CARGO:
        case MAP_MENU_ACTION_UNLOAD_CARGO:
        case MAP_MENU_ACTION_PILLAGE:
        case MAP_MENU_ACTION_RETURN_EUROPE:
        case MAP_MENU_ACTION_GOTO_PORT:
        case MAP_MENU_ACTION_GOTO_PLACE:
        case MAP_MENU_ACTION_TRADE_ROUTE:
          it->visible = false;
          it->enabled = false;
          break;
        default:
          break;
      }
    }
    /* DOS also hides sep cmds 0xff / 0x100 (after Pillage / after Return). */
    {
      int after_pillage = -1;
      int after_return = -1;
      for (int i = 0; i < orders->item_count; ++i) {
        if (orders->items[i].action == MAP_MENU_ACTION_PILLAGE) {
          after_pillage = i;
        }
        if (orders->items[i].action == MAP_MENU_ACTION_RETURN_EUROPE) {
          after_return = i;
        }
      }
      if (after_pillage >= 0 && after_pillage + 1 < orders->item_count &&
          orders->items[after_pillage + 1].separator) {
        orders->items[after_pillage + 1].visible = false;
      }
      if (after_return >= 0 && after_return + 1 < orders->item_count &&
          orders->items[after_return + 1].separator) {
        orders->items[after_return + 1].visible = false;
      }
    }
    return;
  }

  /* FUN_2b5a_0b34 Move Pieces. */
  const bool sea = units_is_sea(ctx->units, ctx->selected_id);
  const bool land = !sea;
  const bool pioneer = land && units_is_pioneer(ctx->units, ctx->selected_id);
  const bool transport = units_is_transport(ctx->units, ctx->selected_id);
  const int ux = u->x;
  const int uy = u->y;
  const int pedia =
    ctx->map ? map_pedia_terrain_index_at(ctx->map, ux, uy) : -1;
  const bool forest = pedia >= 8 && pedia <= 23;
  /* DOS 0x1b/0x1c hill classes; also arctic/mountains via pedia 24/27. */
  const bool hills = (pedia == 0x1b || pedia == 0x1c || pedia == 24 || pedia == 27);
  /*
   * DOS local_c via FUN_281f_0b78: unit appears in profession/founder table.
   * Approximate: land non-transport (colonists / military / pioneers).
   */
  const bool can_found_unit = land && !transport;
  const int cid_here =
    ctx->colonies ? colonies_id_at(ctx->colonies, ux, uy) : -1;
  const ColonizeColony* col_here =
    (cid_here >= 0) ? colonies_get(ctx->colonies, cid_here) : NULL;
  const bool on_own_colony =
    col_here && col_here->nation_id == u->nation_id;
  const bool on_euro_settlement = col_here != NULL;
  const bool high_seas =
    ctx->map && units_on_high_seas(ctx->map, ux, uy);
  const bool has_goods =
    transport && units_first_goods_hold(ctx->units, ctx->selected_id) >= 0;
  const int cargo_cap = transport ? units_goods_hold_count(ctx->units, ctx->selected_id) : 0;

  /* 0x317 Pillage — always hidden in 0b34. */
  for (int i = 0; i < orders->item_count; ++i) {
    if (orders->items[i].action == MAP_MENU_ACTION_PILLAGE) {
      orders->items[i].visible = false;
      orders->items[i].enabled = false;
    }
  }

  /* Build / Join (0x310 / 0x311). */
  for (int i = 0; i < orders->item_count; ++i) {
    MapMenuItem* it = &orders->items[i];
    if (it->action == MAP_MENU_ACTION_BUILD_COLONY) {
      if (!can_found_unit) {
        it->visible = false;
      } else if (on_own_colony) {
        it->visible = false; /* hide Build when on colony */
      } else {
        it->enabled = ctx->map && ctx->colonies &&
                      colonies_can_found(ctx->colonies, ctx->map, ux, uy);
      }
    } else if (it->action == MAP_MENU_ACTION_JOIN_COLONY) {
      if (!can_found_unit) {
        it->visible = false;
      } else if (!on_own_colony) {
        it->visible = false;
      }
    }
  }

  /* Clear / Plow / Road — disable if not Pioneer; hide Clear↔Plow by terrain. */
  for (int i = 0; i < orders->item_count; ++i) {
    MapMenuItem* it = &orders->items[i];
    if (it->action != MAP_MENU_ACTION_CLEAR_FOREST &&
        it->action != MAP_MENU_ACTION_PLOW_FIELDS &&
        it->action != MAP_MENU_ACTION_BUILD_ROAD) {
      continue;
    }
    if (!pioneer) {
      it->enabled = false;
    }
  }
  for (int i = 0; i < orders->item_count; ++i) {
    MapMenuItem* it = &orders->items[i];
    if (forest) {
      if (it->action == MAP_MENU_ACTION_PLOW_FIELDS) {
        it->visible = false;
      }
    } else {
      if (it->action == MAP_MENU_ACTION_CLEAR_FOREST) {
        it->visible = false;
      }
    }
    if (hills &&
        (it->action == MAP_MENU_ACTION_CLEAR_FOREST ||
         it->action == MAP_MENU_ACTION_PLOW_FIELDS)) {
      it->visible = false;
    }
  }

  /* Fortify land (0x302) vs ship (0x303). */
  for (int i = 0; i < orders->item_count; ++i) {
    MapMenuItem* it = &orders->items[i];
    if (land) {
      if (it->action == MAP_MENU_ACTION_ANCHOR) {
        it->visible = false;
      }
    } else {
      if (it->action == MAP_MENU_ACTION_FORTIFY) {
        it->visible = false;
      }
      if (it->action == MAP_MENU_ACTION_RETURN_EUROPE && !high_seas) {
        it->enabled = false;
      }
      if (it->action == MAP_MENU_ACTION_GOTO_PLACE) {
        it->visible = false;
      }
    }
  }
  if (land) {
    for (int i = 0; i < orders->item_count; ++i) {
      MapMenuItem* it = &orders->items[i];
      if (it->action == MAP_MENU_ACTION_RETURN_EUROPE ||
          it->action == MAP_MENU_ACTION_GOTO_PORT) {
        it->visible = false;
      }
    }
  }

  /* Load/Unload/Trade/Dump — hide if no cargo capacity. */
  if (cargo_cap <= 0) {
    for (int i = 0; i < orders->item_count; ++i) {
      MapMenuItem* it = &orders->items[i];
      if (it->action == MAP_MENU_ACTION_LOAD_CARGO ||
          it->action == MAP_MENU_ACTION_UNLOAD_CARGO ||
          it->action == MAP_MENU_ACTION_TRADE_ROUTE ||
          it->action == MAP_MENU_ACTION_DUMP_OVERBOARD) {
        it->visible = false;
      }
    }
  } else {
    if (!on_euro_settlement) {
      for (int i = 0; i < orders->item_count; ++i) {
        MapMenuItem* it = &orders->items[i];
        if (it->action == MAP_MENU_ACTION_LOAD_CARGO ||
            it->action == MAP_MENU_ACTION_UNLOAD_CARGO) {
          it->enabled = false;
        }
      }
    }
    if (!has_goods) {
      for (int i = 0; i < orders->item_count; ++i) {
        if (orders->items[i].action == MAP_MENU_ACTION_DUMP_OVERBOARD) {
          orders->items[i].enabled = false;
        }
      }
    }
  }

  /* Activate: unit under cursor. */
  for (int i = 0; i < orders->item_count; ++i) {
    if (orders->items[i].action == MAP_MENU_ACTION_ACTIVATE_UNIT) {
      orders->items[i].enabled =
        ctx->units && units_id_at(ctx->units, ctx->cursor_x, ctx->cursor_y) >= 0;
    }
  }
}

void map_menu_refresh(MapMenuBar* bar, const MapMenuOrdersContext* ctx) {
  if (!bar) {
    return;
  }
  for (int m = 0; m < bar->menu_count; ++m) {
    MapMenuPulldown* menu = &bar->menus[m];
    if (strcmp(menu->section_name, "DEBUG") == 0) {
      continue; /* port-only */
    }
    if (strcmp(menu->section_name, "ORDERS") == 0) {
      map_menu_refresh_orders_dos(menu, ctx);
      continue;
    }
    for (int i = 0; i < menu->item_count; ++i) {
      MapMenuItem* it = &menu->items[i];
      if (it->separator) {
        it->visible = true;
        it->enabled = false;
        continue;
      }
      it->visible = true;
      if (strcmp(menu->section_name, "CUP") == 0) {
        it->enabled =
          (it->action == MAP_MENU_ACTION_CHEAT_REVEAL_MAP ||
           it->action == MAP_MENU_ACTION_CHEAT_KILL_INDIANS);
        continue;
      }
      it->enabled = map_menu_action_enabled(it->action);
    }
  }
}

bool map_menu_open_alt_hotkey(MapMenuBar* bar, char letter) {
  if (!bar || !bar->loaded || !letter) {
    return false;
  }
  const char want = (char)toupper((unsigned char)letter);
  for (int i = 0; i < bar->menu_count; ++i) {
    MapMenuPulldown* menu = &bar->menus[i];
    if (!menu->visible || menu->title_hotkey != want) {
      continue;
    }
    if (bar->open_index == i) {
      bar->open_index = -1;
      bar->hover_item = -1;
    } else {
      bar->open_index = i;
      bar->hover_item = -1;
    }
    return true;
  }
  return false;
}

MapMenuAction map_menu_orders_hotkey(
  const MapMenuBar* bar,
  char letter,
  bool shift,
  bool space
) {
  if (!bar) {
    return MAP_MENU_ACTION_NONE;
  }
  const MapMenuPulldown* orders = NULL;
  for (int i = 0; i < bar->menu_count; ++i) {
    if (strcmp(bar->menus[i].section_name, "ORDERS") == 0) {
      orders = &bar->menus[i];
      break;
    }
  }
  if (!orders) {
    return MAP_MENU_ACTION_NONE;
  }
  const char want = letter ? (char)toupper((unsigned char)letter) : 0;
  for (int i = 0; i < orders->item_count; ++i) {
    const MapMenuItem* it = &orders->items[i];
    if (!it->visible || !it->enabled || it->separator) {
      continue;
    }
    if (space && it->hotkey_space) {
      return it->action;
    }
    if (!space && want && it->hotkey == want && it->hotkey_shift == shift &&
        !it->hotkey_space) {
      return it->action;
    }
  }
  return MAP_MENU_ACTION_NONE;
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
    /* Center over the minimap (same horizontal math as map_panel_minimap_rect). */
    const int inner_x0 = MAP_PANEL_X + 2;
    const int inner_w = 319 - inner_x0 + 1;
    const int mx = inner_x0 + (inner_w - MAP_PANEL_MINIMAP_W) / 2;
    menu->title_x = mx + MAP_PANEL_MINIMAP_W / 2 - tw / 2;
    if (menu->title_x < MAP_PANEL_X) {
      menu->title_x = MAP_PANEL_X;
    }
    if (menu->title_x + menu->title_w > 320) {
      menu->title_x = 320 - menu->title_w;
    }
  }
}

static int map_menu_dropdown_width(const MapMenuPulldown* menu, const ColonizeFont* font) {
  int max_w = map_menu_text_width(font, menu->title) + 12;
  for (int i = 0; i < menu->item_count; ++i) {
    if (!menu->items[i].visible) {
      continue;
    }
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
  const int vis = map_menu_visible_item_count(menu);
  int x = menu->title_x;
  int w = map_menu_dropdown_width(menu, font);
  int h = 2 + vis * item_h + 2;
  if (h < 4) {
    h = 4;
  }
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
    /* 1px gap between bar black rule (y=BAR_H-1) and dropdown top border. */
    *out_y = MAP_MENU_BAR_H + 1;
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
    if (!m->visible) {
      continue;
    }
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
  const MapMenuPulldown* menu = &bar->menus[bar->open_index];
  const int item_h = map_menu_item_height(font);
  const int vis_row = (y - dy - 2) / item_h;
  return map_menu_item_index_from_visible(menu, vis_row);
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

  if (bar->open_index >= 0 &&
      (bar->open_index >= bar->menu_count || !bar->menus[bar->open_index].visible)) {
    bar->open_index = -1;
    bar->hover_item = -1;
  }

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
      if (!mi->enabled || mi->separator || !mi->visible || action == MAP_MENU_ACTION_SEPARATOR) {
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

/* Screen-aligned WOODTILE so bar and pull-down grain continue seamlessly. */
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
  const int tw = tile->width;
  const int th = tile->height;
  const int x1 = origin_x + rect_w;
  const int y1 = origin_y + rect_h;
  const int x0 = (origin_x / tw) * tw;
  const int y0 = (origin_y / th) * th;
  for (int y = y0; y < y1; y += th) {
    for (int x = x0; x < x1; x += tw) {
      for (int sy = 0; sy < th; ++sy) {
        const int fy = y + sy;
        if (fy < origin_y || fy >= y1 || fy < 0 || fy >= framebuffer->height) {
          continue;
        }
        for (int sx = 0; sx < tw; ++sx) {
          const int fx = x + sx;
          if (fx < origin_x || fx >= x1 || fx < 0 || fx >= framebuffer->width) {
            continue;
          }
          const uint8_t color = tile->pixels[sy * tw + sx];
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
    if (!menu->visible) {
      continue;
    }
    const uint8_t color =
      (i == bar->open_index) ? MAP_MENU_COL_TITLE_ACTIVE : MAP_MENU_COL_TITLE;
    font_draw_text_hotkey(
      font, framebuffer, menu->title_x + 2, 1, menu->title, color, MAP_MENU_COL_HOTKEY
    );
  }

  if (bar->open_index < 0 || bar->open_index >= bar->menu_count ||
      !bar->menus[bar->open_index].visible) {
    return;
  }

  const MapMenuPulldown* open = &bar->menus[bar->open_index];
  int dx, dy, dw, dh;
  map_menu_dropdown_rect(bar, font, bar->open_index, &dx, &dy, &dw, &dh);
  if (wood_tile && wood_tile->sprite_count > 0) {
    map_menu_tile_wood(wood_tile, dx, dy, dw, dh, framebuffer);
  } else {
    map_menu_fill_rect(framebuffer, dx, dy, dx + dw - 1, dy + dh - 1, MAP_MENU_COL_PANEL);
  }
  map_menu_hline(framebuffer, dy, dx, dx + dw - 1, MAP_MENU_COL_BORDER);
  map_menu_hline(framebuffer, dy + dh - 1, dx, dx + dw - 1, MAP_MENU_COL_BORDER);
  map_menu_vline(framebuffer, dx, dy, dy + dh - 1, MAP_MENU_COL_BORDER);
  map_menu_vline(framebuffer, dx + dw - 1, dy, dy + dh - 1, MAP_MENU_COL_BORDER);

  const int item_h = map_menu_item_height(font);
  for (int i = 0; i < open->item_count; ++i) {
    if (!open->items[i].visible) {
      continue;
    }
    const int vis_row = map_menu_visible_row_from_item(open, i);
    if (vis_row < 0) {
      continue;
    }
    const int iy = dy + 2 + vis_row * item_h;
    if (open->items[i].separator) {
      const int mid = iy + item_h / 2;
      map_menu_hline(framebuffer, mid, dx + 4, dx + dw - 5, MAP_MENU_COL_RULE_ITEM);
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
    case MAP_MENU_ACTION_PICK_MUSIC:
      return "Pick Music";
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
    case MAP_MENU_ACTION_CLEAR_FOREST:
      return "Clear Forest";
    case MAP_MENU_ACTION_PLOW_FIELDS:
      return "Plow Fields";
    case MAP_MENU_ACTION_BUILD_ROAD:
      return "Build Road";
    case MAP_MENU_ACTION_LOAD_CARGO:
      return "Load Cargo";
    case MAP_MENU_ACTION_UNLOAD_CARGO:
      return "Unload Cargo";
    case MAP_MENU_ACTION_PILLAGE:
      return "Pillage";
    case MAP_MENU_ACTION_GOTO_PORT:
      return "Go to Port";
    case MAP_MENU_ACTION_GOTO_PLACE:
      return "Go to Place";
    case MAP_MENU_ACTION_TRADE_ROUTE:
      return "Begin Trade Route";
    case MAP_MENU_ACTION_RETURN_EUROPE:
      return "Return to Europe";
    case MAP_MENU_ACTION_FORTIFY:
      return "Fortify";
    case MAP_MENU_ACTION_ANCHOR:
      return "Fortify"; /* ship row — same label in MENU.TXT */
    case MAP_MENU_ACTION_SENTRY:
      return "Sentry";
    case MAP_MENU_ACTION_DISBAND:
      return "Disband Unit";
    case MAP_MENU_ACTION_DUMP_OVERBOARD:
      return "Dump Cargo Overboard";
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
    case MAP_MENU_ACTION_CHEAT_CREATE_UNIT:
      return "Create Unit";
    case MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS:
      return "Debug Info Flags";
    case MAP_MENU_ACTION_CHEAT_REVEAL_MAP:
      return "Reveal Map";
    case MAP_MENU_ACTION_CHEAT_SET_HUMAN:
      return "Set Human Player";
    case MAP_MENU_ACTION_CHEAT_KILL_INDIANS:
      return "Kill Indians";
    case MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION:
      return "Advance Revolution Status";
    case MAP_MENU_ACTION_CHEAT_SOUND_TEST:
      return "Sound Test";
    case MAP_MENU_ACTION_CHEAT_MEMORY_CHECK:
      return "Memory Check";
    case MAP_MENU_ACTION_CHEAT_SHOW_STRATEGY:
      return "Show Strategy";
    case MAP_MENU_ACTION_CHEAT_SHOW_COLONY_SITES:
      return "Show Colony Sites";
    case MAP_MENU_ACTION_CHEAT_TEST_ROUTINE:
      return "Test Routine";
    case MAP_MENU_ACTION_DEBUG_SPRITE_VIEWER:
      return "Sprite Viewer";
    case MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS:
      return "Show Mouse Coords";
    default:
      return "unknown";
  }
}
