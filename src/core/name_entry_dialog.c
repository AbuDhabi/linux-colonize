#include "core/name_entry_dialog.h"

#include <string.h>

#include "core/ai_popup.h"
#include "core/map_menu.h"
#include "core/popup.h"
#include "core/strutil.h"
#include "core/ui_button.h"

void name_entry_init(NameEntryDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
  dlg->mss = -1;
}

/* Single close point for both the finish paths. Clearing `kind` matters only
 * while the dialog is open; the caller reads result_kind, captured above. */
static void name_entry_close(NameEntryDialog* dlg) {
  if (!dlg) {
    return;
  }
  dlg->open = false;
  dlg->kind = NAME_ENTRY_KIND_NONE;
}

static void name_entry_finish(NameEntryDialog* dlg, bool cancelled) {
  if (!dlg) {
    return;
  }
  dlg->has_result = true;
  dlg->result_cancelled = cancelled;
  dlg->result_kind = dlg->kind;
  if (!cancelled) {
    /* Empty LANDHO name: caller fills the NAMES.TXT @COLONYNAME nation default. */
    str_copy_trunc(dlg->result_name, sizeof(dlg->result_name), dlg->name);
  } else {
    /* Cancel: keep typed/seed text for LANDHO so discovery still names the land. */
    if (dlg->kind == NAME_ENTRY_KIND_LANDHO) {
      str_copy_trunc(dlg->result_name, sizeof(dlg->result_name), dlg->name);
    } else {
      dlg->result_name[0] = '\0';
    }
  }
  name_entry_close(dlg);
}

bool name_entry_open(
  NameEntryDialog* dlg,
  NameEntryKind kind,
  const char* prompt,
  const char* initial_name,
  int target
) {
  if (!dlg || kind == NAME_ENTRY_KIND_NONE) {
    return false;
  }
  name_entry_init(dlg);
  dlg->has_result = false;
  dlg->kind = kind;
  dlg->result_target = target;
  str_copy_trunc(
    dlg->prompt,
    sizeof(dlg->prompt),
    prompt && prompt[0] ? prompt : ""
  );
  /* FUN_291f_0120's max length: 0x17 for @COLONY / @RENAMECOLONY / @LANDHO
   * (raw 40204, 76999), 0x1f for @TRADENAME (647e:1126). */
  dlg->name_cap = kind == NAME_ENTRY_KIND_TRADE_NAME || kind == NAME_ENTRY_KIND_TRADE_RENAME
                    ? sizeof(dlg->name)
                    : NAME_ENTRY_COLONY_NAME_CHARS + 1;
  /* MAPEDIT FUN_133d_380a lengths: @NEWNAME 0x14 (FUN_1000_15fc), @SAVEAS 0xe. */
  if (kind == NAME_ENTRY_KIND_MAP_NEW) {
    dlg->name_cap = 0x14 + 1;
  } else if (kind == NAME_ENTRY_KIND_MAP_SAVEAS) {
    dlg->name_cap = 0xe + 1;
  }
  dlg->width = popup_msg_take_pending_width();
  /* @LANDHO latches MSS0 (2b5a:0028), @COLONY MSS5 (479b:0823); the rename and
   * trade-name prompts leave 0x1f5e at -1 (port_bugs.md #1183). */
  dlg->mss = popup_msg_take_pending_graphic();
  str_copy_trunc(dlg->name, dlg->name_cap, initial_name ? initial_name : "");
  /* DOS opens the field with bit 0x80 set — whole text selected. */
  text_edit_reset(&dlg->edit, dlg->name, true);
  dlg->open = true;
  return true;
}

void name_entry_set_label(NameEntryDialog* dlg, const char* label) {
  if (dlg) {
    str_copy_trunc(dlg->label, sizeof(dlg->label), label ? label : "");
  }
}

bool name_entry_handle_input(NameEntryDialog* dlg, const ColonizeInputState* input) {
  if (!dlg || !dlg->open || !input) {
    return false;
  }

  if (text_edit_handle_mouse(
        &dlg->edit, dlg->name, dlg->field_font, input, dlg->field_x, dlg->field_y, dlg->field_h
      )) {
    return true;
  }

  switch (text_edit_handle_input(&dlg->edit, dlg->name, dlg->name_cap, input)) {
    case TEXT_EDIT_ACTION_CANCEL:
      name_entry_finish(dlg, true);
      return true;
    case TEXT_EDIT_ACTION_CONFIRM:
      name_entry_finish(dlg, false);
      return true;
    case TEXT_EDIT_ACTION_EDIT:
      return true;
    case TEXT_EDIT_ACTION_NONE:
      break;
  }

  if (input->mouse_right_clicked) {
    name_entry_finish(dlg, true);
    return true;
  }
  if (input->mouse_left_clicked) {
    /* Click outside the box cancels, inside accepts. */
    name_entry_finish(
      dlg,
      !ui_rect_hit(dlg->dialog_x, dlg->dialog_y, dlg->dialog_w, dlg->dialog_h, input->mouse_x, input->mouse_y)
    );
    return true;
  }
  return true;
}

void name_entry_render(
  NameEntryDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
) {
  if (!dlg || !dlg->open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  /* The DOS text-entry layout (FUN_291f_0120): caption, then the field box,
   * the whole default pre-selected. See popup.h popup_field_frame. */
  PopupFieldGeom g;
  popup_field_frame(
    framebuffer, font, wood_tile, colors, dlg->width, dlg->prompt, dlg->label,
    (int)dlg->name_cap - 1, text_color,
    dlg->mss >= 0 ? ai_popup_decoration_sheet(dlg->mss, -1) : NULL, &g
  );
  dlg->dialog_x = g.frame.x;
  dlg->dialog_y = g.frame.y;
  dlg->dialog_w = g.frame.w;
  dlg->dialog_h = g.frame.h;
  if (!font) {
    return;
  }
  dlg->field_font = font;
  dlg->field_x = g.text_x;
  dlg->field_y = g.text_y;
  dlg->field_h = g.text_h;
  TextEditColors edit_colors;
  text_edit_default_colors(&edit_colors);
  edit_colors.text = text_color;
  edit_colors.selection = select_color;
  text_edit_render(
    &dlg->edit, dlg->name, font, framebuffer, g.text_x, g.text_y, g.text_h, &edit_colors
  );
}
