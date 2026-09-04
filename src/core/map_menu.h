#ifndef COLONIZE_MAP_MENU_H
#define COLONIZE_MAP_MENU_H

#include <stdbool.h>
#include <stddef.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * In-game map menu bar from MENU.TXT (@GAME @VIEW @ORDERS @REPORTS @TRADE @PEDIA).
 * @CUP (CHEAT) is always loaded into a fixed bar slot but hidden until Alt-W/I/N.
 * Optional DEBUG pulldown (COLONIZE_DEBUG_MENU) sits after CHEAT with a fixed slot.
 *
 * Layout matches the DOS map display: pull-down titles across the top strip;
 * open a menu to drop a list of items. Mouse-driven (manual: menu bar commands).
 */
#define MAP_MENU_BAR_H 8
#define MAP_MENU_MAX_MENUS 8
#define MAP_MENU_MAX_ITEMS 32 /* DOS ORDERS = 20 labels + 4 seps; leave headroom */
#define MAP_MENU_TITLE_LEN 24
#define MAP_MENU_LABEL_LEN 40

#ifndef COLONIZE_DEBUG_MENU
#define COLONIZE_DEBUG_MENU 0
#endif

typedef enum MapMenuAction {
  MAP_MENU_ACTION_NONE = 0,
  MAP_MENU_ACTION_UNIMPLEMENTED, /* known stub — do not navigate */
  MAP_MENU_ACTION_SEPARATOR,     /* visual rule in dropdown; not clickable */

  /* GAME */
  MAP_MENU_ACTION_PICK_MUSIC,
  MAP_MENU_ACTION_OPTIONS,
  MAP_MENU_ACTION_COLONY_OPTIONS,
  MAP_MENU_ACTION_SOUND_OPTIONS,
  MAP_MENU_ACTION_SAVE,
  MAP_MENU_ACTION_LOAD,
  MAP_MENU_ACTION_DECLARE_INDEPENDENCE,
  MAP_MENU_ACTION_RETIRE,
  MAP_MENU_ACTION_EXIT,

  /* VIEW */
  MAP_MENU_ACTION_MOVE_PIECES, /* ~Move Pieces — M hotkey */
  MAP_MENU_ACTION_VIEW_PIECES, /* ~View Pieces — V hotkey */
  MAP_MENU_ACTION_EUROPE,
  MAP_MENU_ACTION_FIND_COLONY,
  MAP_MENU_ACTION_ZOOM_IN,
  MAP_MENU_ACTION_ZOOM_OUT,
  MAP_MENU_ACTION_ZOOM_LEVEL_120X96, /* zoom 3 */
  MAP_MENU_ACTION_ZOOM_LEVEL_60X48,  /* zoom 2 */
  MAP_MENU_ACTION_ZOOM_LEVEL_30X24,  /* zoom 1 */
  MAP_MENU_ACTION_ZOOM_LEVEL_15X12,  /* zoom 0 */
  MAP_MENU_ACTION_VIEW_HIDDEN_TERRAIN, /* Show ~Hidden Terrain — H hotkey */
  MAP_MENU_ACTION_CENTER_VIEW,

  /* ORDERS */
  MAP_MENU_ACTION_ACTIVATE_UNIT,
  MAP_MENU_ACTION_WAIT_UNIT, /* wait for next unit with moves */
  MAP_MENU_ACTION_BUILD_COLONY,
  MAP_MENU_ACTION_JOIN_COLONY,
  MAP_MENU_ACTION_CLEAR_FOREST,
  MAP_MENU_ACTION_PLOW_FIELDS,
  MAP_MENU_ACTION_BUILD_ROAD,
  MAP_MENU_ACTION_LOAD_CARGO,
  MAP_MENU_ACTION_UNLOAD_CARGO,
  MAP_MENU_ACTION_PILLAGE,
  MAP_MENU_ACTION_GOTO_PORT,
  MAP_MENU_ACTION_GOTO_PLACE,
  MAP_MENU_ACTION_TRADE_ROUTE,
  MAP_MENU_ACTION_TRADE_CREATE,
  MAP_MENU_ACTION_TRADE_EDIT,
  MAP_MENU_ACTION_TRADE_DELETE,
  MAP_MENU_ACTION_RETURN_EUROPE,
  MAP_MENU_ACTION_FORTIFY, /* land Fortify (MENU first ~Fortify / cmd 0x302) */
  MAP_MENU_ACTION_ANCHOR,  /* ship Fortify (MENU second ~Fortify / cmd 0x303) */
  MAP_MENU_ACTION_SENTRY,
  MAP_MENU_ACTION_DISBAND,
  MAP_MENU_ACTION_DUMP_OVERBOARD,
  MAP_MENU_ACTION_NO_ORDERS, /* end turn / space */

  /* PEDIA */
  MAP_MENU_ACTION_PEDIA_CARGO,
  MAP_MENU_ACTION_PEDIA_UNIT,
  MAP_MENU_ACTION_PEDIA_TERRAIN,
  MAP_MENU_ACTION_PEDIA_JOB,
  MAP_MENU_ACTION_PEDIA_BUILDING,
  MAP_MENU_ACTION_PEDIA_FATHER,
  MAP_MENU_ACTION_PEDIA_MISC,

  /* REPORTS (F1–F10) */
  MAP_MENU_ACTION_REPORT_TERRAIN,
  MAP_MENU_ACTION_REPORT_RELIGIOUS,
  MAP_MENU_ACTION_REPORT_CONGRESS,
  MAP_MENU_ACTION_REPORT_LABOR,
  MAP_MENU_ACTION_REPORT_ECONOMIC,
  MAP_MENU_ACTION_REPORT_COLONY,
  MAP_MENU_ACTION_REPORT_NAVAL,
  MAP_MENU_ACTION_REPORT_FOREIGN,
  MAP_MENU_ACTION_REPORT_INDIAN,
  MAP_MENU_ACTION_REPORT_SCORE,

  /* CHEAT (@CUP) — all 11 items implemented (game_loop.c MAP_MENU_ACTION_CHEAT_*) */
  MAP_MENU_ACTION_CHEAT_CREATE_UNIT,
  MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS,
  MAP_MENU_ACTION_CHEAT_REVEAL_MAP,
  MAP_MENU_ACTION_CHEAT_SET_HUMAN,
  MAP_MENU_ACTION_CHEAT_KILL_INDIANS,
  MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION,
  MAP_MENU_ACTION_CHEAT_SOUND_TEST,
  MAP_MENU_ACTION_CHEAT_MEMORY_CHECK,
  MAP_MENU_ACTION_CHEAT_SHOW_STRATEGY,
  MAP_MENU_ACTION_CHEAT_SHOW_COLONY_SITES,
  MAP_MENU_ACTION_CHEAT_TEST_ROUTINE,

  /* DEBUG (port-only; COLONIZE_DEBUG_MENU) */
  MAP_MENU_ACTION_DEBUG_SPRITE_VIEWER,
  MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS,
  MAP_MENU_ACTION_DEBUG_BUILDING_RECTS,
  MAP_MENU_ACTION_DEBUG_LOGS
} MapMenuAction;

typedef struct MapMenuItem {
  char label[MAP_MENU_LABEL_LEN];
  MapMenuAction action;
  bool enabled; /* false = grayed; click does nothing useful */
  bool visible; /* false = omitted from dropdown (context alternate rows) */
  bool separator; /* horizontal rule; label ignored */
  char hotkey; /* 0 = none; A–Z / 0–9 from ~ markers */
  bool hotkey_shift; /* Shift+hotkey (Disband) */
  bool hotkey_space; /* Space bar (No Orders) */
} MapMenuItem;

typedef struct MapMenuPulldown {
  char title[MAP_MENU_TITLE_LEN];
  char section_name[COLONIZE_MSG_SECTION_LEN];
  MapMenuItem items[MAP_MENU_MAX_ITEMS];
  int item_count;
  int title_x;
  int title_w;
  bool visible; /* false = reserve layout slot but do not draw/hit */
  char title_hotkey; /* Alt+letter to open; 0 if none */
} MapMenuPulldown;

typedef struct MapMenuBar {
  MapMenuPulldown menus[MAP_MENU_MAX_MENUS];
  int menu_count;
  int open_index;    /* -1 = closed */
  int hover_item;    /* item under cursor in open menu, or -1 */
  bool cheat_visible;
  bool loaded;
} MapMenuBar;

/*
 * Live unit/map context for menu enable/hide (DOS FUN_2b5a_0b34 Move Pieces /
 * FUN_2b5a_0902 View Pieces). When selected_id is invalid → View Pieces baseline.
 */
typedef struct MapMenuOrdersContext {
  const ColonizeUnitPool* units;
  const ColonizeWorldMap* map;
  const ColonizeColonyPool* colonies;
  int selected_id;
  int cursor_x;
  int cursor_y;
  int human_nation;
  bool europe_ok;
} MapMenuOrdersContext;

void map_menu_init(MapMenuBar* bar);
void map_menu_free(MapMenuBar* bar);
bool map_menu_load(MapMenuBar* bar, const ColonizeMsgCatalog* menu_txt, bool show_debug);

/* Show/hide the CHEAT title; layout slot stays reserved either way. */
void map_menu_set_cheat_visible(MapMenuBar* bar, bool visible);

/*
 * Refresh all pull-downs: DOS empty-label separators stay visible; ORDERS
 * hide/gray from unit/tile (0b34/0902); other menus gray unimplemented rows.
 * Cite: FUN_74a4_0000 seps; FUN_4b58_0552/05c6; FUN_2b5a_0b34.
 */
void map_menu_refresh(MapMenuBar* bar, const MapMenuOrdersContext* ctx);

/* Back-compat alias used by game_loop. */
static inline void map_menu_refresh_orders(MapMenuBar* bar, const MapMenuOrdersContext* ctx) {
  map_menu_refresh(bar, ctx);
}

/* Alt+letter opens the matching bar menu (title ~hotkey). True if opened/toggled. */
bool map_menu_open_alt_hotkey(MapMenuBar* bar, char letter);

/*
 * Plain ORDERS hotkey (no Alt): first visible+enabled item matching key/shift/space.
 * Returns MAP_MENU_ACTION_NONE if none.
 */
MapMenuAction map_menu_orders_hotkey(const MapMenuBar* bar, char letter, bool shift, bool space);

/*
 * Plain VIEW hotkey (no Alt, no menu open needed): first visible+enabled
 * VIEW item matching letter — Zoom In (Z) / Zoom Out (X) (MENU.TXT ~Z / ~X).
 * Returns MAP_MENU_ACTION_NONE if none.
 */
MapMenuAction map_menu_view_hotkey(const MapMenuBar* bar, char letter);

/* True if (x,y) is over the bar or an open dropdown (consumes map clicks). */
bool map_menu_hit_ui(const MapMenuBar* bar, int x, int y);

/*
 * Handle mouse motion / click. Returns an action when an enabled item is chosen.
 * Esc closes an open menu (call with close_request).
 */
MapMenuAction map_menu_handle_input(
  MapMenuBar* bar,
  const ColonizeInputState* input,
  const ColonizeFont* font,
  bool close_request
);

void map_menu_render(
  MapMenuBar* bar,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile, /* nullable; WOODTILE fill when set */
  ColonizeFramebuffer8* framebuffer
);

/* Human-readable label for status line when stubbing. */
const char* map_menu_action_name(MapMenuAction action);

#endif
