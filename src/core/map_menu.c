#include "core/map_menu.h"

/*
 * Map menu bar: MENU.TXT parsing, DOS item enable/refresh, hotkeys, layout, input, render.
 *
 * Sections:
 *  - Hotkey/title parsing, visible-item index helpers, action classify/enabled (map_menu_parse_hotkey .. map_menu_action_enabled)
 *  - Init/free, item append, separators & MENU.TXT section load (map_menu_init .. map_menu_set_cheat_visible)
 *  - DOS Orders-menu refresh, Alt/orders/view hotkeys (map_menu_refresh_orders_dos .. map_menu_view_hotkey)
 *  - Title/dropdown layout, hit tests & input handling (map_menu_layout_titles .. map_menu_handle_input)
 *  - Tile-rect screen phase, status message, render & action names (map_menu_tile_rect_screen_phase .. map_menu_action_name)
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "core/fb.h"
#include "core/font.h"
#include "core/mods.h"
#include "core/screen_geom.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/ui_colors.h"
#include "platform/diagnostics.h"

/* Menu bar / dropdown chrome under the map palette. */
enum {
  MAP_MENU_COL_BAR = 6,
  MAP_MENU_COL_TITLE = COLONIZE_COL_BASIC,
  MAP_MENU_COL_TITLE_OPEN = COLONIZE_COL_SELECT, /* DS:0x14a8 <- DS:0x835 */
  MAP_MENU_COL_HOTKEY = COLONIZE_COL_HILITE, /* darker yellow / @COLORS hilite */
  MAP_MENU_COL_PANEL = 4, /* solid fallback when WOODTILE unavailable */
  MAP_MENU_COL_BORDER = 0, /* black outline around pull-down */
  MAP_MENU_COL_RULE = 0, /* black separator under the bar */
  MAP_MENU_COL_RULE_ITEM = COLONIZE_COL_BASIC, /* green item divider (PEDIA ---) */
  MAP_MENU_COL_ITEM = COLONIZE_COL_BASIC,
  MAP_MENU_COL_ITEM_DISABLED = COLONIZE_COL_GREY,
  /* DS:0x14a4 <- DS:0x835 (@COLORS select), same as the open-title box. */
  MAP_MENU_COL_HOVER = COLONIZE_COL_SELECT
};




/* Parse ~ hotkeys from MENU.TXT (space / Shift+D chords, else first ~letter). */
static void map_menu_parse_hotkey(const char* label_raw, MapMenuItem* item) {
  if (!item) {
    return;
  }
  item->hotkey = 0;
  item->hotkey_shift = false;
  item->hotkey_space = false;
  if (!label_raw) {
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


/*
 * MENU.TXT row -> action, per section. `row` is the section's stored-line
 * index (row 0 = the "~TITLE" line, items from 1), i.e. DOS's own item order.
 * Dispatch is by ROW, never by label: the port compiles none of the game's
 * wording, and a translated or re-worded MENU.TXT must still work. DOS does
 * the same — its menu handler switches on the item ordinal.
 */
#define U MAP_MENU_ACTION_UNIMPLEMENTED
static const MapMenuAction k_menu_rows_game[] = {
  U, MAP_MENU_ACTION_OPTIONS, MAP_MENU_ACTION_COLONY_OPTIONS, MAP_MENU_ACTION_SOUND_OPTIONS,
  MAP_MENU_ACTION_PICK_MUSIC, MAP_MENU_ACTION_SAVE, MAP_MENU_ACTION_LOAD,
  MAP_MENU_ACTION_DECLARE_INDEPENDENCE, MAP_MENU_ACTION_RETIRE, MAP_MENU_ACTION_EXIT
};
static const MapMenuAction k_menu_rows_view[] = {
  U, MAP_MENU_ACTION_MOVE_PIECES, MAP_MENU_ACTION_VIEW_PIECES, MAP_MENU_ACTION_EUROPE,
  MAP_MENU_ACTION_FIND_COLONY, MAP_MENU_ACTION_ZOOM_IN, MAP_MENU_ACTION_ZOOM_OUT,
  MAP_MENU_ACTION_ZOOM_LEVEL_120X96, MAP_MENU_ACTION_ZOOM_LEVEL_60X48,
  MAP_MENU_ACTION_ZOOM_LEVEL_30X24, MAP_MENU_ACTION_ZOOM_LEVEL_15X12,
  MAP_MENU_ACTION_VIEW_HIDDEN_TERRAIN, MAP_MENU_ACTION_CENTER_VIEW,
  MAP_MENU_ACTION_KING_AUDIENCE /* mods.kings_audience: stock stops at row 12; data/mods appends 13 */
};
/* Rows 3 and 4 are both "Fortify" (land 0x302, ship 0x303): the second is
 * the ship's anchor order. */
static const MapMenuAction k_menu_rows_orders[] = {
  U, MAP_MENU_ACTION_ACTIVATE_UNIT, MAP_MENU_ACTION_WAIT_UNIT, MAP_MENU_ACTION_FORTIFY,
  MAP_MENU_ACTION_ANCHOR, MAP_MENU_ACTION_SENTRY, MAP_MENU_ACTION_BUILD_COLONY,
  MAP_MENU_ACTION_JOIN_COLONY, MAP_MENU_ACTION_CLEAR_FOREST, MAP_MENU_ACTION_PLOW_FIELDS,
  MAP_MENU_ACTION_BUILD_ROAD, MAP_MENU_ACTION_LOAD_CARGO, MAP_MENU_ACTION_UNLOAD_CARGO,
  MAP_MENU_ACTION_PILLAGE, MAP_MENU_ACTION_GOTO_PORT, MAP_MENU_ACTION_GOTO_PLACE,
  MAP_MENU_ACTION_TRADE_ROUTE, MAP_MENU_ACTION_RETURN_EUROPE, MAP_MENU_ACTION_NO_ORDERS,
  MAP_MENU_ACTION_DUMP_OVERBOARD, MAP_MENU_ACTION_DISBAND
};
static const MapMenuAction k_menu_rows_reports[] = {
  U, MAP_MENU_ACTION_REPORT_TERRAIN, MAP_MENU_ACTION_REPORT_RELIGIOUS,
  MAP_MENU_ACTION_REPORT_CONGRESS, MAP_MENU_ACTION_REPORT_LABOR,
  MAP_MENU_ACTION_REPORT_ECONOMIC, MAP_MENU_ACTION_REPORT_COLONY, MAP_MENU_ACTION_REPORT_NAVAL,
  MAP_MENU_ACTION_REPORT_FOREIGN, MAP_MENU_ACTION_REPORT_INDIAN, MAP_MENU_ACTION_REPORT_SCORE
};
static const MapMenuAction k_menu_rows_trade[] = {
  U, MAP_MENU_ACTION_TRADE_EDIT, MAP_MENU_ACTION_TRADE_CREATE, MAP_MENU_ACTION_TRADE_DELETE
};
static const MapMenuAction k_menu_rows_cup[] = {
  U, MAP_MENU_ACTION_CHEAT_CREATE_UNIT, MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS,
  MAP_MENU_ACTION_CHEAT_REVEAL_MAP, MAP_MENU_ACTION_CHEAT_SET_HUMAN,
  MAP_MENU_ACTION_CHEAT_KILL_INDIANS, MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION,
  MAP_MENU_ACTION_CHEAT_SOUND_TEST, MAP_MENU_ACTION_CHEAT_MEMORY_CHECK,
  MAP_MENU_ACTION_CHEAT_SHOW_STRATEGY, MAP_MENU_ACTION_CHEAT_SHOW_COLONY_SITES,
  MAP_MENU_ACTION_CHEAT_TEST_ROUTINE
};
/* Row 8 is "Complete", which DOS FUN_74a4_0000 never adds (ids 0x70-0x75 only) — left out of the table so
 * the loader drops it. Miscellaneous (0x76) is a port addition. */
static const MapMenuAction k_menu_rows_pedia[] = {
  U, MAP_MENU_ACTION_PEDIA_CARGO, MAP_MENU_ACTION_PEDIA_UNIT, MAP_MENU_ACTION_PEDIA_TERRAIN,
  MAP_MENU_ACTION_PEDIA_JOB, MAP_MENU_ACTION_PEDIA_BUILDING, MAP_MENU_ACTION_PEDIA_FATHER,
  MAP_MENU_ACTION_PEDIA_MISC
};
#undef U

static MapMenuAction map_menu_classify(const char* section, int row) {
  if (!section) {
    return MAP_MENU_ACTION_UNIMPLEMENTED;
  }
  static const struct {
    const char* section;
    const MapMenuAction* rows;
    int count;
  } k_sections[] = {
#define ROWS(name, table) {name, table, (int)(sizeof(table) / sizeof(table[0]))}
    ROWS("GAME", k_menu_rows_game),       ROWS("VIEW", k_menu_rows_view),
    ROWS("ORDERS", k_menu_rows_orders),   ROWS("REPORTS", k_menu_rows_reports),
    ROWS("TRADE", k_menu_rows_trade),     ROWS("CUP", k_menu_rows_cup),
    ROWS("PEDIA", k_menu_rows_pedia),
#undef ROWS
  };
  for (size_t i = 0; i < sizeof(k_sections) / sizeof(k_sections[0]); ++i) {
    if (strcmp(section, k_sections[i].section) == 0) {
      return (row >= 0 && row < k_sections[i].count) ? k_sections[i].rows[row]
                                                     : MAP_MENU_ACTION_UNIMPLEMENTED;
    }
  }

  return MAP_MENU_ACTION_UNIMPLEMENTED;
}

static bool map_menu_action_enabled(MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_USER:
    case MAP_MENU_ACTION_SAVE:
    case MAP_MENU_ACTION_LOAD:
    case MAP_MENU_ACTION_DECLARE_INDEPENDENCE:
    case MAP_MENU_ACTION_RETIRE:
    case MAP_MENU_ACTION_EXIT:
    case MAP_MENU_ACTION_PICK_MUSIC:
    case MAP_MENU_ACTION_OPTIONS:
    case MAP_MENU_ACTION_COLONY_OPTIONS:
    case MAP_MENU_ACTION_SOUND_OPTIONS:
    case MAP_MENU_ACTION_MOVE_PIECES:
    case MAP_MENU_ACTION_VIEW_PIECES:
    case MAP_MENU_ACTION_EUROPE:
    case MAP_MENU_ACTION_FIND_COLONY:
    case MAP_MENU_ACTION_ZOOM_IN:
    case MAP_MENU_ACTION_ZOOM_OUT:
    case MAP_MENU_ACTION_ZOOM_LEVEL_120X96:
    case MAP_MENU_ACTION_ZOOM_LEVEL_60X48:
    case MAP_MENU_ACTION_ZOOM_LEVEL_30X24:
    case MAP_MENU_ACTION_ZOOM_LEVEL_15X12:
    case MAP_MENU_ACTION_CENTER_VIEW:
    case MAP_MENU_ACTION_VIEW_HIDDEN_TERRAIN:
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
    case MAP_MENU_ACTION_TRADE_CREATE:
    case MAP_MENU_ACTION_TRADE_EDIT:
    case MAP_MENU_ACTION_TRADE_DELETE:
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
    case MAP_MENU_ACTION_DEBUG_BUILDING_RECTS:
    case MAP_MENU_ACTION_DEBUG_LOGS:
    case MAP_MENU_ACTION_CHEAT_CREATE_UNIT:
    case MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS:
    case MAP_MENU_ACTION_CHEAT_REVEAL_MAP:
    case MAP_MENU_ACTION_CHEAT_SET_HUMAN:
    case MAP_MENU_ACTION_CHEAT_KILL_INDIANS:
    case MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION:
    case MAP_MENU_ACTION_CHEAT_SOUND_TEST:
    case MAP_MENU_ACTION_CHEAT_MEMORY_CHECK:
    case MAP_MENU_ACTION_CHEAT_SHOW_STRATEGY:
    case MAP_MENU_ACTION_CHEAT_SHOW_COLONY_SITES:
    case MAP_MENU_ACTION_CHEAT_TEST_ROUTINE:
    case MAP_MENU_ACTION_PLAN_PIONEER:
    case MAP_MENU_ACTION_AUTOMATE:
    case MAP_MENU_ACTION_KING_AUDIENCE:
      return true;
    default:
      return false;
  }
}

/* The mods.kings_audience and mods.pioneer_plan rows are hidden (so their
 * hotkeys are dead too) unless the running campaign has the mod on. */
static bool map_menu_action_visible(MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_KING_AUDIENCE: return mods_on(MOD_KINGS_AUDIENCE);
    case MAP_MENU_ACTION_PLAN_PIONEER: return mods_on(MOD_PIONEER_PLAN);
    default: return true;
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
  item->visible = map_menu_action_visible(action);
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

/* A port-only row right after `after`, hidden until map_menu_refresh shows it. */
static void map_menu_insert_port_item(
  MapMenuPulldown* menu, MapMenuAction after, const char* label, MapMenuAction action
) {
  const int at = map_menu_find_action_index(menu, after, 0) + 1;
  if (at <= 0 || menu->item_count >= MAP_MENU_MAX_ITEMS) {
    return;
  }
  map_menu_append_item(menu, label, action);
  const MapMenuItem item = menu->items[menu->item_count - 1];
  memmove(
    &menu->items[at + 1], &menu->items[at],
    (size_t)(menu->item_count - 1 - at) * sizeof(menu->items[0])
  );
  menu->items[at] = item;
  menu->items[at].visible = false;
}

/*
 * DOS FUN_74a4_0000 inserts empty-label separators (not in MENU.TXT). Cite: viceroy_unpacked.c:119308+.
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
    /* Port addition (user-requested): set Miscellaneous off from the rest. */
    map_menu_insert_sep_after_action(menu, MAP_MENU_ACTION_PEDIA_FATHER);
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
  str_strip_chars(title, "#");
  str_trim(title);
  str_copy_trunc(menu->title, sizeof(menu->title), title);
  menu->title_hotkey = map_menu_parse_title_hotkey(title);

  for (int i = 1; i < sec->line_count && menu->item_count < MAP_MENU_MAX_ITEMS; ++i) {
    char label[MAP_MENU_LABEL_LEN];
    str_copy_trunc(label, sizeof(label), sec->lines[i]);
    str_trim(label); /* '#' stays: a drawn glyph (mm_draw_label) */
    if (label[0] == '\0') {
      continue;
    }
    char classify_label[MAP_MENU_LABEL_LEN];
    snprintf(classify_label, sizeof(classify_label), "%s", label);
    str_strip_chars(classify_label, FONT_SKIP_HOTKEY);
    if (classify_label[0] == '\0') {
      continue;
    }
    MapMenuAction action = map_menu_classify(section_name, i);
    if (action == MAP_MENU_ACTION_UNIMPLEMENTED && strcmp(section_name, "PEDIA") == 0) {
      continue; /* "Complete": not in the DOS pulldown */
    }
    map_menu_append_item(menu, label, action);
  }
  map_menu_insert_dos_separators(menu);
  /* Port-only rows (mods.pioneer_plan etc.); map_menu_refresh hides them while off. */
  if (strcmp(section_name, "VIEW") == 0) {
    map_menu_insert_port_item(menu, MAP_MENU_ACTION_VIEW_PIECES, "Pla~n Pioneer", MAP_MENU_ACTION_PLAN_PIONEER);
  } else if (strcmp(section_name, "ORDERS") == 0) {
    map_menu_insert_port_item(menu, MAP_MENU_ACTION_BUILD_ROAD, "Automate (~Q)", MAP_MENU_ACTION_AUTOMATE);
  }
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
  map_menu_append_item(menu, "Building Rects", MAP_MENU_ACTION_DEBUG_BUILDING_RECTS);
  map_menu_append_item(menu, "Debug Logs", MAP_MENU_ACTION_DEBUG_LOGS);
  bar->menu_count++;
  return true;
}
#endif

bool map_menu_load_custom(
  MapMenuBar* bar, const ColonizeMsgCatalog* catalog, const MapMenuCustomSection* sections,
  int section_count
) {
  if (!bar) {
    return false;
  }
  map_menu_init(bar);
  for (int s = 0; catalog && s < section_count && bar->menu_count < MAP_MENU_MAX_MENUS; ++s) {
    const ColonizeMsgSection* sec = assets_msg_find(catalog, sections[s].section);
    if (!sec || sec->line_count < 1) {
      continue;
    }
    MapMenuPulldown* menu = &bar->menus[bar->menu_count];
    memset(menu, 0, sizeof(*menu));
    str_copy_trunc(menu->section_name, sizeof(menu->section_name), sections[s].section);
    menu->visible = true;
    char title[MAP_MENU_TITLE_LEN];
    str_copy_trunc(title, sizeof(title), sec->lines[0]);
    str_strip_chars(title, "#");
    str_trim(title);
    str_copy_trunc(menu->title, sizeof(menu->title), title);
    menu->title_hotkey = map_menu_parse_title_hotkey(title);
    int line = 1;
    for (int k = 0; k < sections[s].id_count && menu->item_count < MAP_MENU_MAX_ITEMS; ++k) {
      const int id = sections[s].ids[k];
      if (id == MAP_MENU_CUSTOM_SEP) {
        map_menu_append_item(menu, "", MAP_MENU_ACTION_SEPARATOR);
        continue;
      }
      while (line < sec->line_count && sec->lines[line][0] == '\0') {
        ++line;
      }
      if (line >= sec->line_count) {
        break;
      }
      char label[MAP_MENU_LABEL_LEN];
      str_copy_trunc(label, sizeof(label), sec->lines[line++]);
      str_strip_chars(label, "#");
      str_trim(label);
      if (id <= 0) {
        continue;
      }
      map_menu_append_item(menu, label, MAP_MENU_ACTION_USER);
      MapMenuItem* item = &menu->items[menu->item_count - 1];
      item->user_id = id;
      if (strncmp(label, "~F~", 3) == 0) {
        item->hotkey = 0;
      }
    }
    bar->menu_count++;
  }
  bar->loaded = bar->menu_count > 0;
  return bar->loaded;
}

int map_menu_custom_hotkey(const MapMenuBar* bar, char letter) {
  if (!bar || letter == 0) {
    return 0;
  }
  const char want = (char)toupper((unsigned char)letter);
  for (int m = 0; m < bar->menu_count; ++m) {
    const MapMenuPulldown* menu = &bar->menus[m];
    for (int i = 0; i < menu->item_count; ++i) {
      const MapMenuItem* it = &menu->items[i];
      if (it->user_id > 0 && it->enabled && it->visible &&
          toupper((unsigned char)it->hotkey) == want) {
        return it->user_id;
      }
    }
  }
  return 0;
}

bool map_menu_load(MapMenuBar* bar, const ColonizeMsgCatalog* menu_txt, bool show_debug) {
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
  if (show_debug) {
    map_menu_load_debug(bar);
  }
#else
  (void)show_debug;
#endif
  map_menu_load_section(bar, menu_txt, "PEDIA", true);

  /*
   * Title hit-boxes stay zero here: map_menu_layout_titles() is the single
   * answer and both readers (map_menu_handle_input, map_menu_render) call it
   * before touching title_x/title_w. A load-time copy could only measure
   * titles without the font, and its constants had already drifted from the
   * live helper (gap +4 vs +6, no >320 PEDIA clamp).
   */

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
          it->enabled = false;
          break;
        /* port_bugs.md #693: FUN_2b5a_0902 greys Build (0x310) at raw 42124 and
         * then HIDES it at raw 42127 — the hide wins, so View Pieces shows no
         * Build Colony row at all. */
        case MAP_MENU_ACTION_BUILD_COLONY:
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
    if (ctx && ctx->plan_view && ctx->map) {
      /* Plan Pioneer: the improvement rows edit the cursor tile's plan. Clear
       * on forest, Plow elsewhere, neither on Hills/Mountains (as 0b34). */
      const int pedia = map_pedia_terrain_index_at(ctx->map, ctx->cursor_x, ctx->cursor_y);
      const bool forest = pedia >= 8 && pedia <= 23;
      for (int i = 0; i < orders->item_count; ++i) {
        MapMenuItem* it = &orders->items[i];
        if (it->action == MAP_MENU_ACTION_CLEAR_FOREST && forest) {
          it->visible = true;
          it->enabled = (ctx->plan_bits & 0x01u) != 0; /* PIONEER_PLAN_CLEAR */
        } else if (it->action == MAP_MENU_ACTION_PLOW_FIELDS && !forest) {
          it->visible = true;
          it->enabled = (ctx->plan_bits & 0x02u) != 0; /* PIONEER_PLAN_PLOW */
        } else if (it->action == MAP_MENU_ACTION_BUILD_ROAD) {
          it->visible = true;
          it->enabled = (ctx->plan_bits & 0x04u) != 0; /* PIONEER_PLAN_ROAD */
        }
      }
    }
    for (int i = 0; i < orders->item_count; ++i) {
      if (orders->items[i].action == MAP_MENU_ACTION_AUTOMATE) {
        orders->items[i].visible = false;
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
  /*
   * FUN_2b5a_0b34 (viceroy_unpacked.c 42219-42225): after the forest test
   * picks which of Clear (0x312) / Plow (0x313) to hide, the only extra
   * hide-both clause is `local_6 == 0x1b || local_6 == 0x1c` — mountains and
   * hills, on the FUN_281f_078c class scale that map_pedia_terrain_index_at
   * mirrors (27 / 28). There is no arctic arm, and `27` was just `0x1b`
   * written twice (smell_audit_2026-09-09 #101).
   */
  const bool hills = (pedia == 0x1b || pedia == 0x1c);
  /*
   * DOS local_c via FUN_281f_0b78 = FUN_15eb_0902 (raw 42188-42196,
   * 9939-9944): `DS:0x30e[type] >= 0` — the unit has a profession slot.
   * Regulars / Cavalry / Treasure / Artillery (-1) hide Build and Join;
   * Cont. Army / Cont. Cav. (21 / 23) keep them. port_bugs.md #667.
   */
  const bool can_found_unit = units_type_has_profession_slot(u->type_index);
  const int cid_here =
    ctx->colonies ? colonies_id_at(ctx->colonies, ux, uy) : -1;
  const ColonizeColony* col_here =
    (cid_here >= 0) ? colonies_get(ctx->colonies, cid_here) : NULL;
  const bool on_euro_settlement = col_here != NULL;
  const bool high_seas =
    ctx->map && units_on_high_seas(ctx->map, ux, uy);
  const bool has_goods =
    transport && units_first_goods_hold(ctx->units, ctx->selected_id) >= 0;
  const int cargo_cap = transport ? units_goods_hold_count(ctx->units, ctx->selected_id) : 0;

  /* 0x317 Pillage — always hidden in 0b34 (raw 42193), so the port has no
   * Pillage body. DOS has no improvement-destroying pillage either: every
   * FUN_281f_068c call sets or clears only purchase / occupancy bits
   * (port_bugs.md #631). */
  for (int i = 0; i < orders->item_count; ++i) {
    if (orders->items[i].action == MAP_MENU_ACTION_PILLAGE) {
      orders->items[i].visible = false;
      orders->items[i].enabled = false;
    }
  }

  /* Build / Join (0x310 / 0x311). */
  for (int i = 0; i < orders->item_count; ++i) {
    MapMenuItem* it = &orders->items[i];
    /*
     * DOS-LITERAL FUN_2b5a_0b34 raw 42199-42211 (port_bugs.md #686 / #692):
     * `FUN_281f_07be(x,y)` resolves ANY Euro colony on the tile (not just an
     * own one). Colony present -> hide Build (0x310), Join (0x311) stays
     * visible and enabled; no colony -> hide Join, and the ONLY grey on
     * Build is profession 0x1b (Indian Convert). Terrain is never consulted
     * at enable time — the site popups (@SEACOLONY / @TOOMOUNTAIN / …) do
     * that work at apply time.
     */
    if (it->action == MAP_MENU_ACTION_BUILD_COLONY) {
      if (!can_found_unit || on_euro_settlement) {
        it->visible = false;
      } else {
        it->enabled = (u->profession != UNITS_JOB_CONVERT);
      }
    } else if (it->action == MAP_MENU_ACTION_JOIN_COLONY) {
      if (!can_found_unit || !on_euro_settlement) {
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

  /* Automate: Pioneers (mods.pioneer_plan), Scouts / ships
   * (mods.scout_autoexplore / ship_autoexplore), per the running campaign. */
  const bool scout = land && units_type_is_scout(units_type(ctx->units, u->type_index));
  for (int i = 0; i < orders->item_count; ++i) {
    MapMenuItem* it = &orders->items[i];
    if (it->action == MAP_MENU_ACTION_AUTOMATE) {
      it->visible = (mods_on(MOD_PIONEER_PLAN) && pioneer) ||
                    (mods_on(MOD_SCOUT_AUTOEXPLORE) && scout) ||
                    (mods_on(MOD_SHIP_AUTOEXPLORE) && sea);
      it->enabled = it->visible && (!pioneer || u->tools >= 20);
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

  /*
   * port_bugs.md #694: Activate (0x300) is never greyed or hidden. Neither
   * FUN_2b5a_0b34 (a unit is selected) nor FUN_2b5a_0902 (View Pieces) names
   * 0x300 in any of its FUN_291f_0146 / FUN_291f_013a lists, so the row stays
   * live even with no unit under the cursor. The port used to gate it on
   * units_id_at(cursor), which is the port's own invention.
   */
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
      it->visible = map_menu_action_visible(it->action);
      it->enabled = map_menu_action_enabled(it->action);
      if (it->action == MAP_MENU_ACTION_PLAN_PIONEER) {
        snprintf(
          it->label, sizeof(it->label), "Pla~n Pioneer: %s", ctx && ctx->plan_view ? "On" : "Off"
        );
      }
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

MapMenuAction map_menu_view_hotkey(const MapMenuBar* bar, char letter) {
  if (!bar || !letter) {
    return MAP_MENU_ACTION_NONE;
  }
  const MapMenuPulldown* view = NULL;
  for (int i = 0; i < bar->menu_count; ++i) {
    if (strcmp(bar->menus[i].section_name, "VIEW") == 0) {
      view = &bar->menus[i];
      break;
    }
  }
  if (!view) {
    return MAP_MENU_ACTION_NONE;
  }
  const char want = (char)toupper((unsigned char)letter);
  for (int i = 0; i < view->item_count; ++i) {
    const MapMenuItem* it = &view->items[i];
    if (!it->visible || !it->enabled || it->separator) {
      continue;
    }
    if (it->hotkey == want && !it->hotkey_shift && !it->hotkey_space) {
      return it->action;
    }
  }
  return MAP_MENU_ACTION_NONE;
}

/* DOS FUN_4b58_0000: a font's line height is its header byte, 6 read as 5
 * (FONTTINY -> 5). */
static int map_menu_font_h(const ColonizeFont* font) {
  if (!font || font->max_height <= 0) {
    return 5;
  }
  return font->max_height == 6 ? 5 : font->max_height;
}

/* DOS FUN_4b58_02f6 bar record (raw 79565): +4 title y = 1, +6 title gap = 12,
 * +8 item gap = 3, +0xa title pad = 1, +0xc item pad = 4. */
enum {
  MM_TITLE_Y = 1,
  MM_TITLE_GAP = 12,
  MM_ITEM_GAP = 3,
  MM_TITLE_PAD = 1,
  MM_ITEM_PAD = 4
};

/* Row pitch: FUN_4b58_0b7a advances fh + bar[+8]. */
static int map_menu_item_height(const ColonizeFont* font) {
  return map_menu_font_h(font) + MM_ITEM_GAP;
}

/* DOS-LITERAL FUN_4b58_063a raw 79816: each title starts bar[+6] = 12 px past
 * the previous one's end, width = text + 2 * bar[+0xa]; the last (PEDIA,
 * FUN_74a4_0000 passes 1) is right-aligned: x = 320 - w - 12. Hidden titles
 * (CHEAT) keep their slot. 320 -> live width so PEDIA stays over the minimap. */
static void map_menu_layout_titles(MapMenuBar* bar, const ColonizeFont* font) {
  int x = MM_TITLE_GAP;
  for (int i = 0; i < bar->menu_count; ++i) {
    MapMenuPulldown* menu = &bar->menus[i];
    menu->title_w = font_text_width(font, menu->title) + 2 * MM_TITLE_PAD;
    if (strcmp(menu->section_name, "PEDIA") == 0) {
      menu->title_x = screen_geom_w() - menu->title_w - MM_TITLE_GAP;
      continue;
    }
    menu->title_x = x;
    x += menu->title_w + MM_TITLE_GAP;
  }
}

/* DOS-LITERAL FUN_4b58_07d6 raw 79893: title[+6] = max(10, text + 2 * bar[+0xc])
 * over every item ever appended (hidden rows and empty-label rules included);
 * the box is that + 2 (FUN_4b58_0a64). */
static int map_menu_dropdown_width(const MapMenuPulldown* menu, const ColonizeFont* font) {
  int max_w = 10;
  for (int i = 0; i < menu->item_count; ++i) {
    const char* label = menu->items[i].separator ? "" : menu->items[i].label;
    const int w = font_text_width_skip(font, label, "~") + 2 * MM_ITEM_PAD;
    if (w > max_w) {
      max_w = w;
    }
  }
  return max_w + 2;
}

/* DOS-LITERAL FUN_4b58_0a64 raw 80024 (asm OVL11 0a64..0b79): x = title x,
 * y = fh + bar[+4] + 3, h = (fh + 3) * visible + 3 + 2; right edge clamped
 * to 0x13d once it reaches 0x13e, bottom to 0xc7 once it reaches 0xc6. */
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
  const int vis = map_menu_visible_item_count(menu);
  int x = menu->title_x;
  int y = map_menu_font_h(font) + MM_TITLE_Y + 3;
  const int w = map_menu_dropdown_width(menu, font);
  const int h = map_menu_item_height(font) * vis + MM_ITEM_GAP + 2;
  if (x + w - 1 > screen_geom_w() - 3) {
    x = screen_geom_w() - 2 - w;
  }
  if (y + h - 1 >= screen_geom_h() - 2) { /* 0xc6 -> bottom = 0xc7 (sic) */
    y = screen_geom_h() - h;
  }
  if (out_x) {
    *out_x = x;
  }
  if (out_y) {
    *out_y = y;
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
  /* DOS FUN_4b58_13ac raw 80484: the first shown title whose right end
   * (x + w) is at or past the pointer, so a title owns the gap before it. */
  for (int i = 0; i < bar->menu_count; ++i) {
    const MapMenuPulldown* m = &bar->menus[i];
    if (m->visible && x <= m->title_x + m->title_w) {
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
  /* FUN_4b58_0d94: row r spans y + 3 + pitch * r .. + pitch - 1. */
  const int rel = y - dy - MM_ITEM_GAP;
  if (rel < 0) {
    return -1;
  }
  return map_menu_item_index_from_visible(menu, rel / map_menu_item_height(font));
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
      bar->picked_id = mi->user_id;
      return action;
    }
    /* Click outside dropdown closes it. */
    bar->open_index = -1;
    bar->hover_item = -1;
    return MAP_MENU_ACTION_NONE;
  }

  return MAP_MENU_ACTION_NONE;
}

/* Screen-aligned WOODTILE (menu bar, side panel, colony screen). */

void map_menu_tile_rect_screen_phase(
  const ColonizeSpriteSheet* sheet,
  int origin_x,
  int origin_y,
  int rect_w,
  int rect_h,
  ColonizeFramebuffer8* framebuffer
) {
  map_menu_tile_rect_screen_phase_lut(sheet, origin_x, origin_y, rect_w, rect_h, framebuffer, NULL);
}

/* Tile pixel (0,0) lands on (phase_x, phase_y); the screen-phase fills pass 0,0. */
static void mm_tile_rect(
  const ColonizeSpriteSheet* sheet,
  int origin_x,
  int origin_y,
  int rect_w,
  int rect_h,
  int phase_x,
  int phase_y,
  ColonizeFramebuffer8* framebuffer,
  const uint8_t* lut
) {
  if (!sheet || sheet->sprite_count < 1 || !framebuffer || rect_w <= 0 || rect_h <= 0) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[0];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  int x1 = origin_x + rect_w;
  int y1 = origin_y + rect_h;
  if (origin_x < 0) {
    origin_x = 0;
  }
  if (origin_y < 0) {
    origin_y = 0;
  }
  if (x1 > framebuffer->width) {
    x1 = framebuffer->width;
  }
  if (y1 > framebuffer->height) {
    y1 = framebuffer->height;
  }
  /* The screen-phase index is `dy % th`, `dx % tw`; at window sizes the two
   * divisions per pixel dominated the frame (a full-window wood border cost
   * ~31ms). Same pixels, walked with wrapping counters instead. */
  const int tw = tile->width;
  const int th = tile->height;
  const int run = x1 - origin_x;
  int ty = ((origin_y - phase_y) % th + th) % th;
  for (int dy = origin_y; dy < y1; ++dy) {
    const uint8_t* trow = &tile->pixels[(size_t)ty * (size_t)tw];
    uint8_t* frow = &framebuffer->pixels[(size_t)dy * (size_t)framebuffer->width];
    int tx = ((origin_x - phase_x) % tw + tw) % tw;
    bool opaque = true;
    for (int i = 0; i < tw; ++i) {
      if (trow[i] == COLONIZE_SS_TRANSPARENT) {
        opaque = false;
        break;
      }
    }
    if (opaque && run > tw) {
      /* No holes in this tile row: lay one period down pixel by pixel, then
       * let memcpy double it across the rest. WOODTILE is opaque, so this is
       * the path every wood fill takes. */
      for (int i = 0; i < tw; ++i) {
        const uint8_t px = trow[tx];
        frow[origin_x + i] = lut ? lut[px] : px;
        if (++tx == tw) {
          tx = 0;
        }
      }
      int done = tw;
      while (done < run) {
        int n = (done < run - done) ? done : run - done;
        memcpy(&frow[origin_x + done], &frow[origin_x], (size_t)n);
        done += n;
      }
    } else {
      for (int dx = origin_x; dx < x1; ++dx) {
        const uint8_t px = trow[tx];
        if (px != COLONIZE_SS_TRANSPARENT) {
          frow[dx] = lut ? lut[px] : px;
        }
        if (++tx == tw) {
          tx = 0;
        }
      }
    }
    if (++ty == th) {
      ty = 0;
    }
  }
}

void map_menu_tile_rect_screen_phase_lut(
  const ColonizeSpriteSheet* sheet,
  int origin_x,
  int origin_y,
  int rect_w,
  int rect_h,
  ColonizeFramebuffer8* framebuffer,
  const uint8_t* lut
) {
  mm_tile_rect(sheet, origin_x, origin_y, rect_w, rect_h, 0, 0, framebuffer, lut);
}

void map_menu_set_message(MapMenuBar* bar, const char* text, uint8_t color) {
  if (!bar) {
    return;
  }
  if (!text || !text[0]) {
    bar->message[0] = '\0';
    return;
  }
  snprintf(bar->message, sizeof(bar->message), "%s", text);
  bar->message_color = color ? color : (uint8_t)MAP_MENU_COL_TITLE;
  /* The strip is one rect in DOS: a status line and a dropped-down menu
   * cannot share it. */
  bar->open_index = -1;
  bar->hover_item = -1;
}

static uint8_t mm_ink(const MapMenuBar* bar, int stock) {
  if (!bar->inks_set) {
    return (uint8_t)stock;
  }
  switch (stock) {
    case COLONIZE_COL_BASIC: return bar->ink_basic;
    case COLONIZE_COL_HILITE: return bar->ink_hilite;
    case COLONIZE_COL_GREY: return bar->ink_grey;
    case COLONIZE_COL_SELECT: return bar->ink_select;
    default: return (uint8_t)stock;
  }
}

/* FUN_4b58_0104 (raw 79471) treats only '~' as markup: a MENU.TXT '#' (the
 * figure space in "Zoom Level #60 x 48") is a FONTTINY glyph with its own
 * advance, which font_draw_text_hotkey would drop. */
static void mm_draw_label(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* label,
  uint8_t color,
  uint8_t hotkey
) {
  char seg[MAP_MENU_LABEL_LEN];
  while (label) {
    const char* hash = strchr(label, '#');
    const size_t n = hash ? (size_t)(hash - label) : strlen(label);
    snprintf(seg, sizeof(seg), "%.*s", (int)n, label);
    font_draw_text_hotkey(font, framebuffer, x, y, seg, color, hotkey);
    x += font_text_width(font, seg);
    if (hash) {
      x += font_char_advance(font, '#');
    }
    label = hash ? hash + 1 : NULL;
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
    map_menu_tile_rect_screen_phase(wood_tile, 0, 0, framebuffer->width, MAP_MENU_BAR_H, framebuffer);
  } else {
    fb_fill_rect(framebuffer, 0, 0, framebuffer->width, MAP_MENU_BAR_H, MAP_MENU_COL_BAR);
  }
  /* Black rule under the menu bar (full width; separates bar from map + minimap). */
  fb_hline(framebuffer, MAP_MENU_BAR_H - 1, 0, framebuffer->width - 1, MAP_MENU_COL_RULE);

  /* Status line owns the whole strip while it is up (DOS FUN_0000_035c: the
   * message buffer is drawn instead of the strip's normal content). */
  if (bar->message[0]) {
    const int tw = font_text_width(font, bar->message);
    int tx = (framebuffer->width - tw) / 2;
    if (tx < 2) {
      tx = 2;
    }
    font_draw_text(
      font,
      framebuffer,
      tx,
      1,
      bar->message,
      bar->message_color ? bar->message_color : (uint8_t)MAP_MENU_COL_TITLE
    );
    return;
  }

  /* DOS-LITERAL FUN_4b58_093c raw 79972 (asm OVL11 093c..0a63): the open
   * title gets a bar[+0x1a] box (DS:0x14a8 = @COLORS select, set from DS:0x835)
   * at (x, 0, w + 2 * bar[+0xa], fh + bar[+4] + 1); every title keeps its
   * normal ink, text at x + bar[+0xa], y = bar[+4]. */
  const int bar_fill_h = map_menu_font_h(font) + MM_TITLE_Y + 1;
  for (int i = 0; i < bar->menu_count; ++i) {
    const MapMenuPulldown* menu = &bar->menus[i];
    if (!menu->visible) {
      continue;
    }
    if (i == bar->open_index) {
      fb_fill_rect(
        framebuffer, menu->title_x, 0, menu->title_w + 2 * MM_TITLE_PAD, bar_fill_h,
        mm_ink(bar, MAP_MENU_COL_TITLE_OPEN)
      );
    }
    font_draw_text_hotkey(
      font, framebuffer, menu->title_x + MM_TITLE_PAD, MM_TITLE_Y, menu->title,
      mm_ink(bar, MAP_MENU_COL_TITLE), mm_ink(bar, MAP_MENU_COL_HOTKEY)
    );
  }

  if (bar->open_index < 0 || bar->open_index >= bar->menu_count ||
      !bar->menus[bar->open_index].visible) {
    return;
  }

  const MapMenuPulldown* open = &bar->menus[bar->open_index];
  int dx, dy, dw, dh;
  map_menu_dropdown_rect(bar, font, bar->open_index, &dx, &dy, &dw, &dh);
  /* FUN_4b58_0b7a: bar[+0x1e] frame, then the inside filled through
   * FUN_4b58_004a with the wood tile anchored at the frame's own corner. */
  fb_hline(framebuffer, dy, dx, dx + dw - 1, MAP_MENU_COL_BORDER);
  fb_hline(framebuffer, dy + dh - 1, dx, dx + dw - 1, MAP_MENU_COL_BORDER);
  fb_vline(framebuffer, dx, dy, dy + dh - 1, MAP_MENU_COL_BORDER);
  fb_vline(framebuffer, dx + dw - 1, dy, dy + dh - 1, MAP_MENU_COL_BORDER);
  if (wood_tile && wood_tile->sprite_count > 0) {
    mm_tile_rect(wood_tile, dx + 1, dy + 1, dw - 2, dh - 2, dx, dy, framebuffer, NULL);
  } else {
    fb_fill_rect(framebuffer, dx + 1, dy + 1, dw - 2, dh - 2, MAP_MENU_COL_PANEL);
  }

  /* DOS-LITERAL FUN_4b58_0b7a raw 80080 (asm OVL11 0b7a..0d93): text at
   * x + 1 + bar[+0xc], first row y + 1 + bar[+8], pitch fh + bar[+8]; an
   * empty label is a 1px rule in the item ink across x + 1 .. x + w - 2 at
   * row + fh / 2; the hover box is (x + 2, row - 1, w - 4, fh + 2). */
  const int fh = map_menu_font_h(font);
  const int item_h = map_menu_item_height(font);
  for (int i = 0; i < open->item_count; ++i) {
    if (!open->items[i].visible) {
      continue;
    }
    const int vis_row = map_menu_visible_row_from_item(open, i);
    if (vis_row < 0) {
      continue;
    }
    const int iy = dy + 1 + MM_ITEM_GAP + vis_row * item_h;
    if (open->items[i].separator) {
      fb_hline(framebuffer, iy + fh / 2, dx + 1, dx + dw - 2, mm_ink(bar, MAP_MENU_COL_RULE_ITEM));
      continue;
    }
    if (i == bar->hover_item && open->items[i].enabled) {
      fb_fill_rect(framebuffer, dx + 2, iy - 1, dw - 4, fh + 2, mm_ink(bar, MAP_MENU_COL_HOVER));
    }
    /* FUN_4b58_0104: a greyed row is one run in the disabled ink, hotkey too. */
    const bool on = open->items[i].enabled;
    const uint8_t color =
      on ? mm_ink(bar, MAP_MENU_COL_ITEM) : mm_ink(bar, MAP_MENU_COL_ITEM_DISABLED);
    mm_draw_label(
      font, framebuffer, dx + 1 + MM_ITEM_PAD, iy, open->items[i].label, color,
      on ? mm_ink(bar, MAP_MENU_COL_HOTKEY) : color
    );
  }
}

/* Log-only: the port's own action ids, not MENU.TXT labels. */
const char* map_menu_action_name(MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_NONE:
      return "none";
    case MAP_MENU_ACTION_SEPARATOR:
      return "separator";
    case MAP_MENU_ACTION_UNIMPLEMENTED:
      return "unimplemented";
    case MAP_MENU_ACTION_SAVE:
      return "save";
    case MAP_MENU_ACTION_LOAD:
      return "load";
    case MAP_MENU_ACTION_DECLARE_INDEPENDENCE:
      return "declare_independence";
    case MAP_MENU_ACTION_RETIRE:
      return "retire";
    case MAP_MENU_ACTION_EXIT:
      return "exit";
    case MAP_MENU_ACTION_PICK_MUSIC:
      return "pick_music";
    case MAP_MENU_ACTION_OPTIONS:
      return "options";
    case MAP_MENU_ACTION_COLONY_OPTIONS:
      return "colony_options";
    case MAP_MENU_ACTION_SOUND_OPTIONS:
      return "sound_options";
    case MAP_MENU_ACTION_EUROPE:
      return "europe";
    case MAP_MENU_ACTION_FIND_COLONY:
      return "find_colony";
    case MAP_MENU_ACTION_ZOOM_IN:
      return "zoom_in";
    case MAP_MENU_ACTION_ZOOM_OUT:
      return "zoom_out";
    case MAP_MENU_ACTION_ZOOM_LEVEL_120X96:
      return "zoom_level_120x96";
    case MAP_MENU_ACTION_ZOOM_LEVEL_60X48:
      return "zoom_level_60x48";
    case MAP_MENU_ACTION_ZOOM_LEVEL_30X24:
      return "zoom_level_30x24";
    case MAP_MENU_ACTION_ZOOM_LEVEL_15X12:
      return "zoom_level_15x12";
    case MAP_MENU_ACTION_CENTER_VIEW:
      return "center_view";
    case MAP_MENU_ACTION_PLAN_PIONEER:
      return "plan_pioneer";
    case MAP_MENU_ACTION_KING_AUDIENCE:
      return "king_audience";
    case MAP_MENU_ACTION_AUTOMATE:
      return "automate";
    case MAP_MENU_ACTION_VIEW_HIDDEN_TERRAIN:
      return "view_hidden_terrain";
    case MAP_MENU_ACTION_ACTIVATE_UNIT:
      return "activate_unit";
    case MAP_MENU_ACTION_WAIT_UNIT:
      return "wait_unit";
    case MAP_MENU_ACTION_BUILD_COLONY:
      return "build_colony";
    case MAP_MENU_ACTION_JOIN_COLONY:
      return "join_colony";
    case MAP_MENU_ACTION_CLEAR_FOREST:
      return "clear_forest";
    case MAP_MENU_ACTION_PLOW_FIELDS:
      return "plow_fields";
    case MAP_MENU_ACTION_BUILD_ROAD:
      return "build_road";
    case MAP_MENU_ACTION_LOAD_CARGO:
      return "load_cargo";
    case MAP_MENU_ACTION_UNLOAD_CARGO:
      return "unload_cargo";
    case MAP_MENU_ACTION_PILLAGE:
      return "pillage";
    case MAP_MENU_ACTION_GOTO_PORT:
      return "goto_port";
    case MAP_MENU_ACTION_GOTO_PLACE:
      return "goto_place";
    case MAP_MENU_ACTION_TRADE_ROUTE:
      return "trade_route";
    case MAP_MENU_ACTION_TRADE_CREATE:
      return "trade_create";
    case MAP_MENU_ACTION_TRADE_EDIT:
      return "trade_edit";
    case MAP_MENU_ACTION_TRADE_DELETE:
      return "trade_delete";
    case MAP_MENU_ACTION_RETURN_EUROPE:
      return "return_europe";
    case MAP_MENU_ACTION_FORTIFY:
      return "fortify";
    case MAP_MENU_ACTION_ANCHOR:
      return "anchor"; /* ship row — same label in MENU.TXT */
    case MAP_MENU_ACTION_SENTRY:
      return "sentry";
    case MAP_MENU_ACTION_DISBAND:
      return "disband";
    case MAP_MENU_ACTION_DUMP_OVERBOARD:
      return "dump_overboard";
    case MAP_MENU_ACTION_NO_ORDERS:
      return "no_orders";
    case MAP_MENU_ACTION_PEDIA_CARGO:
      return "pedia_cargo";
    case MAP_MENU_ACTION_PEDIA_UNIT:
      return "pedia_unit";
    case MAP_MENU_ACTION_PEDIA_TERRAIN:
      return "pedia_terrain";
    case MAP_MENU_ACTION_PEDIA_JOB:
      return "pedia_job";
    case MAP_MENU_ACTION_PEDIA_BUILDING:
      return "pedia_building";
    case MAP_MENU_ACTION_PEDIA_FATHER:
      return "pedia_father";
    case MAP_MENU_ACTION_PEDIA_MISC:
      return "pedia_misc";
    case MAP_MENU_ACTION_REPORT_TERRAIN:
      return "report_terrain";
    case MAP_MENU_ACTION_REPORT_RELIGIOUS:
      return "report_religious";
    case MAP_MENU_ACTION_REPORT_CONGRESS:
      return "report_congress";
    case MAP_MENU_ACTION_REPORT_LABOR:
      return "report_labor";
    case MAP_MENU_ACTION_REPORT_ECONOMIC:
      return "report_economic";
    case MAP_MENU_ACTION_REPORT_COLONY:
      return "report_colony";
    case MAP_MENU_ACTION_REPORT_NAVAL:
      return "report_naval";
    case MAP_MENU_ACTION_REPORT_FOREIGN:
      return "report_foreign";
    case MAP_MENU_ACTION_REPORT_INDIAN:
      return "report_indian";
    case MAP_MENU_ACTION_REPORT_SCORE:
      return "report_score";
    case MAP_MENU_ACTION_CHEAT_CREATE_UNIT:
      return "cheat_create_unit";
    case MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS:
      return "cheat_debug_flags";
    case MAP_MENU_ACTION_CHEAT_REVEAL_MAP:
      return "cheat_reveal_map";
    case MAP_MENU_ACTION_CHEAT_SET_HUMAN:
      return "cheat_set_human";
    case MAP_MENU_ACTION_CHEAT_KILL_INDIANS:
      return "cheat_kill_indians";
    case MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION:
      return "cheat_advance_revolution";
    case MAP_MENU_ACTION_CHEAT_SOUND_TEST:
      return "cheat_sound_test";
    case MAP_MENU_ACTION_CHEAT_MEMORY_CHECK:
      return "cheat_memory_check";
    case MAP_MENU_ACTION_CHEAT_SHOW_STRATEGY:
      return "cheat_show_strategy";
    case MAP_MENU_ACTION_CHEAT_SHOW_COLONY_SITES:
      return "cheat_show_colony_sites";
    case MAP_MENU_ACTION_CHEAT_TEST_ROUTINE:
      return "cheat_test_routine";
    case MAP_MENU_ACTION_DEBUG_SPRITE_VIEWER:
      return "debug_sprite_viewer";
    case MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS:
      return "debug_toggle_mouse_coords";
    case MAP_MENU_ACTION_DEBUG_BUILDING_RECTS:
      return "debug_building_rects";
    case MAP_MENU_ACTION_DEBUG_LOGS:
      return "debug_logs";
    default:
      return "unknown";
  }
}
