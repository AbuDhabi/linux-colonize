#ifndef COLONIZE_NAME_ENTRY_DIALOG_H
#define COLONIZE_NAME_ENTRY_DIALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/font.h"
#include "core/popup.h"
#include "core/text_edit.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Wood name-entry dialog (@COLONY / @RENAMECOLONY / @LANDHO).
 * Enter confirms; Esc cancels. The field itself is a full caret + selection
 * editor — see core/text_edit.h.
 */

#define NAME_ENTRY_PROMPT_LEN 240
#define NAME_ENTRY_NAME_LEN 24

typedef enum NameEntryKind {
  NAME_ENTRY_KIND_NONE = 0,
  NAME_ENTRY_KIND_FOUND = 1,  /* @COLONY */
  NAME_ENTRY_KIND_RENAME = 2, /* @RENAMECOLONY */
  NAME_ENTRY_KIND_LANDHO = 3  /* @LANDHO — name the New World */
} NameEntryKind;

typedef struct NameEntryDialog {
  bool open;
  NameEntryKind kind;
  char prompt[NAME_ENTRY_PROMPT_LEN];
  char name[NAME_ENTRY_NAME_LEN];
  TextEditState edit;

  bool has_result;
  bool result_cancelled;
  NameEntryKind result_kind;
  int result_colony_id;
  char result_name[NAME_ENTRY_NAME_LEN];

  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  /* Field geometry / font last rendered with — mouse caret placement. */
  const ColonizeFont* field_font;
  int field_x;
  int field_y;
  int field_h;
} NameEntryDialog;

void name_entry_init(NameEntryDialog* dlg);
void name_entry_close(NameEntryDialog* dlg);

bool name_entry_open(
  NameEntryDialog* dlg,
  NameEntryKind kind,
  const char* prompt,
  const char* initial_name,
  int colony_id
);

bool name_entry_handle_input(NameEntryDialog* dlg, const ColonizeInputState* input);

void name_entry_render(
  NameEntryDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
);

#endif
