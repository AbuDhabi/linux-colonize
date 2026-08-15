#ifndef COLONIZE_CHEAT_LIST_DIALOG_H
#define COLONIZE_CHEAT_LIST_DIALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
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
  CHEAT_LIST_KIND_TRADE_UNLOAD,
  CHEAT_LIST_KIND_TRADE_LOAD,
  CHEAT_LIST_KIND_FIND_COLONY,
  CHEAT_LIST_KIND_TRADE_SELECT,
  /* CHEAT Create Unit (@CREATE/@CSHIP/@FOREIGN/@FOREIGN2): multi-stage;
   * caller tracks stage, this dialog just returns whichever list was shown. */
  CHEAT_LIST_KIND_CREATE_UNIT,
  /* CHEAT Set Human Player (@SETHUMAN). */
  CHEAT_LIST_KIND_SET_HUMAN,
  /* CHEAT Debug Info Flags (@OPTIONS checkbox). */
  CHEAT_LIST_KIND_DEBUG_FLAGS
} CheatListKind;

typedef struct CheatListDialog {
  bool open;
  CheatListKind kind;
  int selection;
  int width;
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
 * option_ids are native nation ids 4..11.
 */
bool cheat_list_open_kill_indians(CheatListDialog* dlg, const ColonizeMsgCatalog* names);

/*
 * Thin TRADE cargo picker (@CARGO 0..15). multi_select; preselect bits from
 * initial_mask. Enter confirms result_mask; Esc cancels. Cite: TRADE Edit.
 */
bool cheat_list_open_trade_cargos(
  CheatListDialog* dlg,
  CheatListKind kind,
  uint16_t initial_mask
);

/*
 * Find Colony picker (@FINDCITY). labels[i] = colony name; option_ids = colony id.
 * Caller supplies up to CHEAT_LIST_MAX_OPTIONS entries.
 */
bool cheat_list_open_find_colony(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* colony_ids,
  int count
);

/* Trade route picker (@TRADESELECT). option_ids = route slot index. */
bool cheat_list_open_trade_select(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* route_ids,
  int count
);

/*
 * CHEAT Create Unit generic list step (@CREATE / @CSHIP / @FOREIGN / @FOREIGN2).
 * Caller supplies whichever stage's labels/ids; result_id round-trips ids[i].
 */
bool cheat_list_open_create_unit(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* ids,
  int count
);

/*
 * CHEAT Set Human Player (@SETHUMAN). option_ids: 0..3 nation, -1 = None.
 */
bool cheat_list_open_set_human(CheatListDialog* dlg, const ColonizeMsgCatalog* debug_txt);

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
