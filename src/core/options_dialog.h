#ifndef COLONIZE_OPTIONS_DIALOG_H
#define COLONIZE_OPTIONS_DIALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/col1_save_layout.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Checkbox options wood dialog (@GAMEOPTIONS / @COLONYOPTIONS / @SOUNDOPTIONS).
 * Space/click or the row's ~ hotkey letter toggles; Enter confirms (writes bits); Esc cancels.
 */

#define OPTIONS_DIALOG_MAX 16
#define OPTIONS_DIALOG_LABEL_LEN 48

typedef enum OptionsDialogKind {
  OPTIONS_KIND_NONE = 0,
  OPTIONS_KIND_GAME = 1,
  OPTIONS_KIND_COLONY = 2,
  OPTIONS_KIND_SOUND = 3,
  OPTIONS_KIND_MODS = 4 /* port-only: data/mods GAME.TXT @MODOPTIONS, settings.json "mods" */
} OptionsDialogKind;

typedef struct OptionsDialog {
  bool open;
  OptionsDialogKind kind;
  int selection;
  int width;
  char prompt[COLONIZE_MSG_LINE_LEN];
  char labels[OPTIONS_DIALOG_MAX][OPTIONS_DIALOG_LABEL_LEN];
  uint8_t values[OPTIONS_DIALOG_MAX]; /* 0/1 */
  int option_count;
  int mods_page;                       /* OPTIONS_KIND_MODS: the page shown */
  uint8_t mods_ids[OPTIONS_DIALOG_MAX]; /* OPTIONS_KIND_MODS: the ModId of each row */
  /* OPTIONS_KIND_MODS: LABELS.TXT @MISC 46 "OK" button under the rows ("" = none; the
   * stock checklists have none in DOS) and its last drawn rect. */
  char ok_label[OPTIONS_DIALOG_LABEL_LEN];
  int ok_x, ok_y, ok_w, ok_h;

  bool has_result;
  bool result_cancelled;
  OptionsDialogKind result_kind;
  uint8_t result_values[OPTIONS_DIALOG_MAX];
  int result_count;

  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int list_y0;
  int line_h;
} OptionsDialog;

void options_dialog_init(OptionsDialog* dlg);
void options_dialog_close(OptionsDialog* dlg);

bool options_dialog_open_game(
  OptionsDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  const ColonizeCol1GameOptions* opts
);
bool options_dialog_open_colony(
  OptionsDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  const ColonizeCol1ColonyReportOptions* opts
);
bool options_dialog_open_sound(
  OptionsDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  bool background_music,
  bool event_music,
  bool sound_effects
);

/* Mod toggle defaults as a ModId bit mask (mods.h), one row per ModId. The
 * toggles are grouped by kind of change, one page per group (they do not fit
 * one 200 px dialog anyway): page 0 = @MODOPTIONS (the title menu row), then
 * @MODOPTIONS2 and @MODOPTIONS3, each opened by OK on the page before. */
#define OPTIONS_MODS_PAGE_COUNT 3
/* The ModId on `row` of mods page `page`, or -1 past the page. */
int options_dialog_mods_page_id(int page, int row);
bool options_dialog_open_mods(
  OptionsDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  const ColonizeMsgCatalog* labels_txt,
  uint32_t mask,
  int page
);
/* True and *mask written when a MODS result was confirmed; only the shown
 * page's bits change. */
bool options_dialog_apply_mods(const OptionsDialog* dlg, uint32_t* mask);

/* Apply confirmed GAME bits into Col1 options (no-op if cancelled/wrong kind). */
void options_dialog_apply_game(const OptionsDialog* dlg, ColonizeCol1GameOptions* opts);
void options_dialog_apply_colony(
  const OptionsDialog* dlg,
  ColonizeCol1ColonyReportOptions* opts
);
/* Returns true if sound triple written. */
bool options_dialog_apply_sound(
  const OptionsDialog* dlg,
  bool* background_music,
  bool* event_music,
  bool* sound_effects
);

bool options_dialog_handle_input(OptionsDialog* dlg, const ColonizeInputState* input);

void options_dialog_render(
  OptionsDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
);

#endif
