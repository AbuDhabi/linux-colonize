#ifndef COLONIZE_MAPEDIT_H
#define COLONIZE_MAPEDIT_H

/*
 * MAPEDIT.EXE, the Colonization map editor (docs/mapedit.md): a separate program in DOS,
 * the `opencol-mapedit` executable here. Paints .MP scenario maps (terrain, rivers,
 * hills, mountains) with the same MAPEDIT compositor the game draws its map with.
 */

#include <stdbool.h>
#include <stdint.h>

#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/font_file.h"
#include "core/map.h"
#include "core/map_menu.h"
#include "core/name_entry_dialog.h"
#include "core/ss.h"
#include "platform/platform.h"

#define MAPEDIT_UNDO_BYTES 12000 /* FUN_1000_2516: the 12000-byte snapshot, also the load cap */
#define MAPEDIT_NAME_LEN 32
#define MAPEDIT_PICK_MAX 256

typedef enum MapEditPending {
  MAPEDIT_PENDING_NONE = 0,
  MAPEDIT_PENDING_PICK_START, /* @MAPTOEDIT at startup: a file, or ESC = new map */
  MAPEDIT_PENDING_PICK_LOAD,  /* @MAPTOLOAD from the menu */
  MAPEDIT_PENDING_SAVE,       /* @SAVE Yes / No */
  MAPEDIT_PENDING_LOAD,       /* @LOAD (unsaved changes) Yes / No */
  MAPEDIT_PENDING_CREATENOW,  /* @CREATENOW Yes / No */
  MAPEDIT_PENDING_EXIT,       /* @EXIT 1 / 2 / 3 */
  MAPEDIT_PENDING_INFO        /* @ERROR, @CONTINENTS*, @HELP*, @ABOUT */
} MapEditPending;

typedef enum MapEditNameFor {
  MAPEDIT_NAME_NONE = 0,
  MAPEDIT_NAME_NEW_START, /* @NEWNAME at startup: cancel exits */
  MAPEDIT_NAME_NEW,       /* @NEWNAME from Create Map */
  MAPEDIT_NAME_SAVEAS
} MapEditNameFor;

typedef struct MapEditor {
  char data_dir[512];
  ColonizePalette palette;
  ColonizeFont font_intr; /* FONTINTR: dialogs */
  ColonizeFont font_tiny; /* FONTTINY: menu bar and panel */
  bool font_intr_ok;
  bool font_tiny_ok;
  ColonizeSpriteSheet terrain;
  ColonizeSpriteSheet phys0;
  ColonizeSpriteSheet icons;
  ColonizeSpriteSheet woodtile;
  ColonizeSpriteSheet cursor;
  bool terrain_ok, phys0_ok, icons_ok, woodtile_ok, cursor_ok;
  bool cursor_built;
  ColonizeMsgCatalog mapedit_txt;
  ColonizeMsgCatalog mapmenu_txt;
  ColonizeMsgCatalog names_txt;
  bool mapedit_txt_ok, mapmenu_txt_ok, names_txt_ok;
  char terrain_names[29][24]; /* @UNFORESTED 0-7, @FORESTED 8-15 (and 16-23), @OTHER 24-28 */
  char other_names[5][24];    /* @OTHER_NAMES: Forest, River, Major River, Minor River, Unexplored */
  uint8_t minimap_color[29];  /* FUN_1b56_006c: centre pixel of each class's sprite */

  ColonizeWorldMap map;
  bool map_ok;
  char file_name[MAPEDIT_NAME_LEN]; /* the current .MP, upper case (%STRING0) */
  bool dirty;                       /* DS:0x46 */
  uint8_t undo[MAPEDIT_UNDO_BYTES]; /* DS:0x5a */
  bool undo_valid;                  /* DS:0x52 */

  int cur_x, cur_y;   /* DS:0x4b52 / 0x4b54 */
  int view_cx, view_cy;
  int level;          /* zoom 0..3: tile 16 >> level */
  int8_t brush_type;  /* DS:0x56, < 0 = keep the tile's type */
  uint8_t brush_and;  /* DS:0x57 */
  uint8_t brush_or;   /* DS:0x58 */
  bool brush_feature; /* DS:0x59 */
  int fill_radius;    /* DS:0x4a */
  bool coast_protect; /* DS:0x4e */

  bool palette_open;
  int palette_idx; /* DS:0x1ce, 1-based hotspot */
  bool continents_open;

  MapMenuBar menu;
  AiPopupState popups;
  MapEditPending pending;
  NameEntryDialog name_entry;
  MapEditNameFor name_for;
  char pick_names[MAPEDIT_PICK_MAX][16];
  int pick_count;
  int pick_page;
  int pick_row_file[AI_POPUP_CHOICE_MAX]; /* choice id -> file index, -2 prev, -3 next */

  int press_region; /* FUN_1000_1b84 region latched on press, -1 none */
  bool prev_left, prev_right;
  uint32_t clock_ms;
  bool quit;
  int exit_code; /* DS:0x62 */
  uint8_t* exe;  /* COLONIZE/MAPEDIT.EXE: its wording lives only in its data segment */
  size_t exe_size;
} MapEditor;

typedef struct MapEditOptions {
  const char* data_dir;
  bool force_new;        /* -c */
  const char* map_file;  /* -m:file */
} MapEditOptions;

/* MAPEDIT.EXE data-segment strings (DS base = file offset 0x17470). */
enum {
  MAPEDIT_DS_MORE = 0xd5, MAPEDIT_DS_MORE2 = 0xdc, MAPEDIT_DS_SIZE = 0xeb, MAPEDIT_DS_CURS = 0xfa,
  MAPEDIT_DS_TERRAIN_AT = 0x109, MAPEDIT_DS_SELECTED = 0x11c, MAPEDIT_DS_ADD = 0x126,
  MAPEDIT_DS_REMOVE = 0x137, MAPEDIT_DS_PAINT = 0x170, MAPEDIT_DS_PICKUP = 0x183,
  MAPEDIT_DS_FILL = 0x198, MAPEDIT_DS_COAST_ON = 0x1a8, MAPEDIT_DS_COAST_OFF = 0x1ba,
  MAPEDIT_DS_UNTITLED = 0x1dd, MAPEDIT_DS_TITLE = 0x3aa, MAPEDIT_DS_COPYRIGHT = 0x3c3,
  MAPEDIT_DS_OPTIONS = 0x3ef, MAPEDIT_DS_USAGE1 = 0x3f9, MAPEDIT_DS_USAGE2 = 0x427,
  MAPEDIT_DS_USAGE3 = 0x458, MAPEDIT_DS_EXIT_VALUE = 0x487
};
#define MAPEDIT_EXE_DS_BASE 0x17470u
/* Read MAPEDIT.EXE from data_dir (malloc'd; NULL when missing). */
uint8_t* mapedit_exe_load(const char* data_dir, size_t* out_size);
/* The NUL-terminated string at `ds` ("" when out of range). */
const char* mapedit_exe_str(const uint8_t* exe, size_t exe_size, unsigned ds);
/* `fmt` with each "%d" replaced by the next of a, b (a format read from data never reaches
 * printf). */
void mapedit_fmt_ints(char* out, size_t out_size, const char* fmt, int a, int b);

bool mapedit_init(MapEditor* ed, const MapEditOptions* opt);
void mapedit_free(MapEditor* ed);
/* One frame. False once the editor has exited. */
bool mapedit_update(MapEditor* ed, ColonizePlatform* platform, const ColonizeInputState* input, uint32_t dt_ms);
void mapedit_render(MapEditor* ed, ColonizeFramebuffer8* fb);

/* FUN_1000_1be0: one tile under the brush (rmb = Shift-Right / Backspace). Exposed for tests. */
void mapedit_paint_tile(MapEditor* ed, int x, int y, bool rmb);
/* FUN_1000_10cc: pick palette hotspot `idx` (1-based). */
void mapedit_palette_pick(MapEditor* ed, int idx);
/* FUN_1000_1226: one arrow on the palette (key = COLONIZE_KEY_UP/DOWN/LEFT/RIGHT). */
void mapedit_palette_key(MapEditor* ed, ColonizeKey key);
/* FUN_19c4_0002: label continents and oceans into layer 3; bit 0 / 1 = more than 15. */
int mapedit_find_continents(ColonizeWorldMap* map);
/* FUN_1000_00b6: hills / mountains drop their forest, types 16-23 fold to 8-15. */
void mapedit_fix_loaded(ColonizeWorldMap* map);

#endif
