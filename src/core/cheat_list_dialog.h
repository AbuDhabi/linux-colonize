#ifndef COLONIZE_CHEAT_LIST_DIALOG_H
#define COLONIZE_CHEAT_LIST_DIALOG_H

#include <stdbool.h>
#include <stddef.h>

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
  CHEAT_LIST_KIND_KILL_INDIANS
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
  /* Set when the user confirms; survives close so the caller can apply. */
  bool has_result;
  CheatListKind result_kind;
  int result_id;
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
