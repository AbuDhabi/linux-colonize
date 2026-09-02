#ifndef COLONIZE_SAVE_LOAD_DIALOG_H
#define COLONIZE_SAVE_LOAD_DIALOG_H

#include <stdbool.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Save / Load slot picker (DOS FUN_7562_030a / FUN_7562_04e8 over the
 * FUN_7562_0052 slot-list builder). Save: slots 0–7. Load: slots 0–9
 * (8/9 autosave — same row format, no special marker in DOS).
 * Rows: "<Difficulty> <Leader> of the <Nation>, <Season> <Year>" for an
 * occupied slot (leader pixel-trimmed under 0x65 px), "(EMPTY)" otherwise.
 * Title + @width come from GAME.TXT @SAVEGAME / @LOADGAME.
 * Esc / click-outside / right-click cancels; Enter/Space/click confirms.
 */

#define SAVE_LOAD_MAX_SLOTS 10
#define SAVE_LOAD_LABEL_LEN 80

typedef enum SaveLoadMode {
  SAVE_LOAD_MODE_SAVE = 0,
  SAVE_LOAD_MODE_LOAD
} SaveLoadMode;

typedef struct SaveLoadDialog {
  bool open;
  SaveLoadMode mode;
  int selection;
  int width; /* GAME.TXT @width content width (DOS 190) */
  char prompt[48];
  char options[SAVE_LOAD_MAX_SLOTS][SAVE_LOAD_LABEL_LEN];
  int slot_ids[SAVE_LOAD_MAX_SLOTS]; /* Col1 slot 0..9 */
  bool slot_occupied[SAVE_LOAD_MAX_SLOTS];
  int option_count;
  /* Set when the user confirms; survives close so the caller can apply. */
  bool has_result;
  SaveLoadMode result_mode;
  int result_slot;
  /* Last computed layout (for hit-testing). */
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int list_y0;
  int line_h;
  /* Last seen mouse position — hover moves the highlight only on actual
   * mouse movement, so it doesn't fight the arrow keys (bugs.md). */
  int hover_mx;
  int hover_my;
} SaveLoadDialog;

void save_load_init(SaveLoadDialog* dlg);
void save_load_close(SaveLoadDialog* dlg);

/*
 * Probe save_dir and open the picker. Returns false on invalid args.
 * messages = GAME.TXT catalog for the @SAVEGAME/@LOADGAME title + @width
 * (NULL → built-in fallbacks). font = popup font for the DOS leader-name
 * pixel trim (NULL → no trim).
 */
bool save_load_open(
  SaveLoadDialog* dlg,
  SaveLoadMode mode,
  const char* save_dir,
  const ColonizeMsgCatalog* messages,
  const ColonizeFont* font
);

/*
 * Keyboard / mouse. Returns true if the event was consumed.
 * On confirm: closes, sets has_result + result_mode/slot.
 * On cancel: closes without has_result.
 */
bool save_load_handle_input(SaveLoadDialog* dlg, const ColonizeInputState* input);

void save_load_render(
  SaveLoadDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
);

#endif
