#ifndef COLONIZE_HOWMUCH_DIALOG_H
#define COLONIZE_HOWMUCH_DIALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Wood amount-entry dialog (@HOWMUCH1…5). Enter confirms; Esc cancels.
 * Digits / Backspace edit; Up/Down or +/- nudge by 1; Shift+Up/Down by 10.
 */

#define HOWMUCH_PROMPT_LEN 240
#define HOWMUCH_FIELD_LEN 16
#define HOWMUCH_LABEL_LEN 32

typedef enum HowmuchKind {
  HOWMUCH_KIND_NONE = 0,
  HOWMUCH_KIND_LOAD = 1,     /* @HOWMUCH1 */
  HOWMUCH_KIND_UNLOAD = 2,   /* @HOWMUCH2 */
  HOWMUCH_KIND_MOVE = 3,     /* @HOWMUCH3 */
  HOWMUCH_KIND_BUY = 4,      /* @HOWMUCH4 */
  HOWMUCH_KIND_SELL = 5,     /* @HOWMUCH5 */
  HOWMUCH_KIND_SOUND_TEST = 6 /* CHEAT Sound Test (@SOUND: "Play what sound #?") */
} HowmuchKind;

typedef struct HowmuchDialog {
  bool open;
  HowmuchKind kind;
  int max_amount;
  int amount;
  char prompt[HOWMUCH_PROMPT_LEN];
  /* The entry-field caption under the prompt. DOS keeps it in the same
   * GAME.TXT / DEBUG.TXT section as the prompt, as the section's last line:
   * "Amount:" for @HOWMUCH1..5, "Sound:" for DEBUG.TXT @SOUND. */
  char amount_label[HOWMUCH_LABEL_LEN];
  char field[HOWMUCH_FIELD_LEN];
  bool field_selected; /* next digit replaces */

  bool has_result;
  bool result_cancelled;
  int result_amount;
  HowmuchKind result_kind;
  int result_cargo; /* caller context */
  int result_payload;

  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
} HowmuchDialog;

void howmuch_init(HowmuchDialog* dlg);

/*
 * The entry-field caption of `section`: its last stored line, skipping the
 * lowercase @directive lines assets_msg_load_file keeps as content. Returns
 * `fallback` when the catalog or section is missing (tests, no data dir).
 * Static return buffer: use or copy it before the next call.
 */
const char* howmuch_amount_label(
  const ColonizeMsgCatalog* catalog,
  const char* section,
  const char* fallback
);

bool howmuch_open(
  HowmuchDialog* dlg,
  HowmuchKind kind,
  const char* prompt,
  const char* amount_label,
  int max_amount,
  int initial_amount,
  int cargo,
  int payload
);

bool howmuch_handle_input(HowmuchDialog* dlg, const ColonizeInputState* input);

void howmuch_render(
  HowmuchDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
);

#endif
