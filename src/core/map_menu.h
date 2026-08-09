#ifndef COLONIZE_MAP_MENU_H
#define COLONIZE_MAP_MENU_H

#include <stdbool.h>
#include <stddef.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/ss.h"
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
#define MAP_MENU_MAX_ITEMS 24
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
  MAP_MENU_ACTION_SAVE,
  MAP_MENU_ACTION_LOAD,
  MAP_MENU_ACTION_RETIRE,
  MAP_MENU_ACTION_EXIT,

  /* VIEW */
  MAP_MENU_ACTION_EUROPE,
  MAP_MENU_ACTION_FIND_COLONY,
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
  MAP_MENU_ACTION_RETURN_EUROPE,
  MAP_MENU_ACTION_FORTIFY,
  MAP_MENU_ACTION_ANCHOR, /* 2nd ~Fortify row: ship Anchor in harbor */
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

  /* CHEAT (@CUP) — Reveal Map + Kill Indians enabled; rest grayed */
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
  MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS
} MapMenuAction;

typedef struct MapMenuItem {
  char label[MAP_MENU_LABEL_LEN];
  MapMenuAction action;
  bool enabled; /* false = grayed; click does nothing useful */
  bool separator; /* horizontal rule; label ignored */
} MapMenuItem;

typedef struct MapMenuPulldown {
  char title[MAP_MENU_TITLE_LEN];
  char section_name[COLONIZE_MSG_SECTION_LEN];
  MapMenuItem items[MAP_MENU_MAX_ITEMS];
  int item_count;
  int title_x;
  int title_w;
  bool visible; /* false = reserve layout slot but do not draw/hit */
} MapMenuPulldown;

typedef struct MapMenuBar {
  MapMenuPulldown menus[MAP_MENU_MAX_MENUS];
  int menu_count;
  int open_index;    /* -1 = closed */
  int hover_item;    /* item under cursor in open menu, or -1 */
  bool cheat_visible;
  bool loaded;
} MapMenuBar;

void map_menu_init(MapMenuBar* bar);
void map_menu_free(MapMenuBar* bar);
bool map_menu_load(MapMenuBar* bar, const ColonizeMsgCatalog* menu_txt);

/* Show/hide the CHEAT title; layout slot stays reserved either way. */
void map_menu_set_cheat_visible(MapMenuBar* bar, bool visible);

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
