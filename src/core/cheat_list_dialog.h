#ifndef COLONIZE_CHEAT_LIST_DIALOG_H
#define COLONIZE_CHEAT_LIST_DIALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Generic wood list popup for cheat menus (Reveal Map @SETVIEW, Kill Indians).
 * Esc / click-outside cancels; Enter/Space/click selects and closes.
 */

#define CHEAT_LIST_MAX_OPTIONS 16
#define CHEAT_LIST_LABEL_LEN 48

typedef enum CheatListKind {
  CHEAT_LIST_KIND_NONE = 0,
  CHEAT_LIST_KIND_SETVIEW,
  CHEAT_LIST_KIND_KILL_INDIANS,
  CHEAT_LIST_KIND_TRADE_SELECT,
  /* TRADE destination picker (@TRADESTART / editor row): ids = colony id,
   * 999 = Europe, 1000 = "(Delete Destination)" (LABELS @ROUTE 8). */
  CHEAT_LIST_KIND_TRADE_DEST,
  /* Single-cargo append picker (@CARGOLOAD / @CARGOUNLOAD): ids = cargo 0..15. */
  CHEAT_LIST_KIND_TRADE_CARGO_ONE,
  /* CHEAT Create Unit (@CREATE/@CSHIP/@FOREIGN/@FOREIGN2): multi-stage;
   * caller tracks stage, this dialog just returns whichever list was shown. */
  CHEAT_LIST_KIND_CREATE_UNIT,
  /* CHEAT Set Human Player (@SETHUMAN). */
  CHEAT_LIST_KIND_SET_HUMAN,
  /* Its "None" row's follow-up (@SETAUTO): ids 1 = Yes, 2 = No. */
  CHEAT_LIST_KIND_SET_AUTO,
  /* CHEAT Debug Info Flags (@OPTIONS checkbox). */
  CHEAT_LIST_KIND_DEBUG_FLAGS,
  /* Unit ORDERS "Go to Port" (@SAILPORT, DOS FUN_647e_01c6 unit>=0 branch):
   * ids = colony id, 999 = Europe. */
  CHEAT_LIST_KIND_GOTO_PORT
} CheatListKind;

/* A FUN_647e_01c6 destination row's "at colony" state (647e:03c4-0436): the unit stands on
 * that colony and the caller asked for the test (Go to Port). */
typedef enum CheatListDestAt {
  CHEAT_LIST_DEST_PLAIN = 0,
  CHEAT_LIST_DEST_GREYED, /* a ship there: row added greyed (FUN_291f_01b6) */
  CHEAT_LIST_DEST_OMITTED /* a land unit there: no row, its slot still counts */
} CheatListDestAt;

/* Every destination one FUN_647e_01c6 call can offer: the Europe row + all colonies. */
#define CHEAT_LIST_DEST_MAX (COLONIZE_COLONIES_MAX + 1)

typedef struct CheatListDialog {
  bool open;
  CheatListKind kind;
  int selection;
  int width;
  int mss; /* DS:0x1f5e MSS figure latched before the box, -1 = none */
  char prompt[COLONIZE_MSG_LINE_LEN];
  char options[CHEAT_LIST_MAX_OPTIONS][CHEAT_LIST_LABEL_LEN];
  int option_ids[CHEAT_LIST_MAX_OPTIONS]; /* fog nation / tribe nation_id / etc. */
  int option_count;
  /* Multi-select (TRADE unload/load): Space/click toggles; Enter confirms. */
  bool multi_select;
  uint16_t selected_mask; /* bit i = option i selected */
  /* Set when the user confirms; survives close so the caller can apply. */
  bool has_result;
  CheatListKind result_kind;
  int result_id;
  uint16_t result_mask; /* multi_select confirm bitmask */
  char result_label[CHEAT_LIST_LABEL_LEN];
  /* FUN_647e_01c6 pager (cheat_list_open_dest_pages): every destination the caller's filter
   * passed, one page of them in options[]; the Previous / More rows turn the page in place. */
  bool paged;
  int page;
  int page_count;
  int page_size;
  bool dest_europe; /* dest 0 is the Europe row */
  bool dest_delete; /* "(Delete Destination)" closes the last page */
  int dest_default; /* id preselected on whatever page shows it, -1 = none */
  int dest_count;
  int dest_ids[CHEAT_LIST_DEST_MAX];
  uint8_t dest_at[CHEAT_LIST_DEST_MAX]; /* CheatListDestAt */
  char dest_labels[CHEAT_LIST_DEST_MAX][CHEAT_LIST_LABEL_LEN];
  char dest_delete_label[CHEAT_LIST_LABEL_LEN];
  uint16_t disabled_mask; /* bit i = options[i] greyed: skipped by Up/Down, clicks ignored */
  /* Last computed layout (for hit-testing). */
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int list_y0;
  int line_h;
} CheatListDialog;

void cheat_list_init(CheatListDialog* dlg);
void cheat_list_close(CheatListDialog* dlg);

/* Load DEBUG.TXT @SETVIEW (prompt + 6 viewpoint options). */
bool cheat_list_open_setview(CheatListDialog* dlg, const ColonizeMsgCatalog* debug_txt);

/*
 * Kill Indians tribe picker. Labels from NAMES.TXT @TRIBES field 2 (short name);
 * option_ids are native nation ids 4..11. alive_mask bit i = tribe i not extinct;
 * only those rows are listed, and an empty mask opens nothing (returns false).
 */
bool cheat_list_open_kill_indians(
  CheatListDialog* dlg, const ColonizeMsgCatalog* names, uint8_t alive_mask
);

/* Trade route picker (@TRADESELECT). option_ids = route slot index. */
bool cheat_list_open_trade_select(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* route_ids,
  int count
);

/* Trade destination picker (@TRADESTART / editor stop row — see kind note). */
bool cheat_list_open_trade_dest(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* dest_ids,
  int count
);

/*
 * FUN_647e_01c6 (raw 102334-102461) destination pager: Go to Port (CHEAT_LIST_KIND_GOTO_PORT)
 * and the trade-route create / editor stop pickers (CHEAT_LIST_KIND_TRADE_DEST). labels / ids /
 * at (NULL = all plain) are every destination the caller's FUN_647e_0094 filter passed, in DOS
 * colony order, the Europe row first when `europe`. Pages per cheat_list_dest_page_math; page 0
 * opens (DOS never jumps to the default's page). Rows per page: "(More)" (LABELS @CTITLE 6) as
 * Previous on page > 0, the page's destinations (greyed / omitted per at[]; default_id
 * preselected unless greyed), then "(More)" before the last page or, on it, `delete_label`
 * (NULL = none) with id 1000. Returns false, showing nothing, when the page has no row.
 */
bool cheat_list_open_dest_pages(
  CheatListDialog* dlg,
  CheatListKind kind,
  const char* prompt,
  const char* const* labels,
  const int* ids,
  const uint8_t* at,
  int count,
  bool europe,
  int default_id,
  const char* delete_label
);

/* FUN_647e_01c6 page math for n colonies, e = 1 with the Europe row (docs/dos_bugs.md DB-53). */
void cheat_list_dest_page_math(int n, int e, int* page_size, int* page_count);

/*
 * Set the initial highlighted row after opening a list (DOS @default /
 * FUN_291f_08ec preselect). Clamped to the option range; no-op if closed.
 */
void cheat_list_set_selection(CheatListDialog* dlg, int index);

/* Single-cargo append picker (@CARGOLOAD / @CARGOUNLOAD, 16 goods). */
bool cheat_list_open_trade_cargo_one(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* cargo_ids,
  int count
);

/*
 * Live-catalog row loader shared by the CHEAT Create Unit list stages
 * (DEBUG.TXT @CREATE / @CSHIP / @FOREIGN / @FOREIGN2). Copies `count`
 * non-directive, non-blank rows starting after `skip_rows` earlier ones
 * (skip_rows=0 count=1 fetches the section's own prompt row) into out[];
 * falls back to fallback[i] per row when the catalog/section/row is missing.
 * out[] entries are CHEAT_LIST_LABEL_LEN-sized.
 */
bool cheat_list_catalog_rows(
  const ColonizeMsgCatalog* catalog,
  const char* section_name,
  int skip_rows,
  const char* const* fallback,
  char out[][CHEAT_LIST_LABEL_LEN],
  int count
);

/*
 * CHEAT Create Unit generic list step (@CREATE / @CSHIP / @FOREIGN / @FOREIGN2).
 * Caller supplies whichever stage's labels/ids and the stage's MSS figure (`mss`, -1 none);
 * result_id round-trips ids[i].
 */
bool cheat_list_open_create_unit(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* ids,
  int count,
  int mss
);

/*
 * CHEAT Set Human Player (@SETHUMAN). option_ids: 0..3 nation, -1 = None.
 */
bool cheat_list_open_set_human(CheatListDialog* dlg, const ColonizeMsgCatalog* debug_txt);
/* DEBUG.TXT @SETAUTO "Automate game?" (FUN_2b5a dispatcher case 0x66, ND 0x23D68-0x23D7D):
 * FUN_1000_8b88's row, 1 = Yes. No section, no dialog. */
bool cheat_list_open_set_auto(CheatListDialog* dlg, const ColonizeMsgCatalog* debug_txt);

/*
 * CHEAT Debug Info Flags (@OPTIONS checkbox). multi_select; preselect bits
 * from initial_mask; Enter confirms result_mask (7 bits, catalog order).
 */
bool cheat_list_open_debug_flags(
  CheatListDialog* dlg,
  const ColonizeMsgCatalog* debug_txt,
  uint16_t initial_mask
);

/*
 * Keyboard / mouse. Returns true if the event was consumed.
 * On confirm: closes, sets has_result + result_kind/id/label.
 * On cancel: closes without has_result.
 */
bool cheat_list_handle_input(CheatListDialog* dlg, const ColonizeInputState* input);

void cheat_list_render(
  CheatListDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
);

#endif
