#include "core/name_entry_dialog.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/strutil.h"

void name_entry_init(NameEntryDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
}

void name_entry_close(NameEntryDialog* dlg) {
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
    str_copy_trunc(dlg->result_name, sizeof(dlg->result_name), dlg->name);
    if (dlg->result_name[0] == '\0') {
      str_copy_trunc(dlg->result_name, sizeof(dlg->result_name), "Colony");
    }
  } else {
    dlg->result_name[0] = '\0';
  }
  dlg->open = false;
}

bool name_entry_open(
  NameEntryDialog* dlg,
  NameEntryKind kind,
  const char* prompt,
  const char* initial_name,
  int colony_id
) {
  if (!dlg || kind == NAME_ENTRY_KIND_NONE) {
    return false;
  }
  name_entry_init(dlg);
  dlg->has_result = false;
  dlg->kind = kind;
  dlg->result_colony_id = colony_id;
  str_copy_trunc(
    dlg->prompt,
    sizeof(dlg->prompt),
    prompt && prompt[0] ? prompt : "What shall we name this colony?"
  );
  str_copy_trunc(dlg->name, sizeof(dlg->name), initial_name ? initial_name : "");
  dlg->name_selected = dlg->name[0] != '\0';
  dlg->open = true;
  return true;
}

bool name_entry_handle_input(NameEntryDialog* dlg, const ColonizeInputState* input) {
  if (!dlg || !dlg->open || !input) {
    return false;
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    name_entry_finish(dlg, true);
    return true;
  }
  if (input->last_key == COLONIZE_KEY_ENTER) {
    name_entry_finish(dlg, false);
    return true;
  }
  if (input->last_key == COLONIZE_KEY_BACKSPACE) {
    if (dlg->name_selected) {
      dlg->name[0] = '\0';
      dlg->name_selected = false;
    } else {
      size_t n = strlen(dlg->name);
      if (n > 0) {
        dlg->name[n - 1] = '\0';
      }
    }
    return true;
  }
  if (input->text_input_len > 0) {
    if (dlg->name_selected) {
      dlg->name[0] = '\0';
      dlg->name_selected = false;
    }
    for (int i = 0; i < input->text_input_len; ++i) {
      const char ch = input->text_input[i];
      if (ch < 32 || ch >= 127) {
        continue;
      }
      size_t n = strlen(dlg->name);
      if (n + 1 < sizeof(dlg->name)) {
        dlg->name[n] = ch;
        dlg->name[n + 1] = '\0';
      }
    }
    return true;
  }
  if (input->mouse_right_clicked) {
    name_entry_finish(dlg, true);
    return true;
  }
  if (input->mouse_left_clicked) {
    if (input->mouse_x < dlg->dialog_x || input->mouse_y < dlg->dialog_y ||
        input->mouse_x >= dlg->dialog_x + dlg->dialog_w ||
        input->mouse_y >= dlg->dialog_y + dlg->dialog_h) {
      name_entry_finish(dlg, true);
    } else {
      name_entry_finish(dlg, false);
    }
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
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad = 6;
  const int dialog_w = 200;
  const int dialog_h = POPUP_FRAME_INSET * 2 + pad + line_h * 3 + pad + line_h * 2 + pad;
  dlg->dialog_w = dialog_w;
  dlg->dialog_h = dialog_h;
  dlg->dialog_x = (framebuffer->width - dialog_w) / 2;
  dlg->dialog_y = (framebuffer->height - dialog_h) / 2;
  if (dlg->dialog_y < MAP_MENU_BAR_H + 2) {
    dlg->dialog_y = MAP_MENU_BAR_H + 2;
  }

  ColonizePopupColors local;
  if (!colors) {
    popup_colors_from_ui(&local);
    colors = &local;
  }
  int ix = 0, iy = 0, iw = 0, ih = 0;
  popup_draw(
    framebuffer,
    dlg->dialog_x,
    dlg->dialog_y,
    dialog_w,
    dialog_h,
    wood_tile,
    colors,
    &ix,
    &iy,
    &iw,
    &ih
  );
  (void)iw;
  (void)ih;
  if (!font) {
    return;
  }
  int ty = iy + pad;
  const char* p = dlg->prompt;
  for (int row = 0; row < 3 && *p; ++row) {
    char line[64];
    size_t n = 0;
    while (*p && n + 1 < sizeof(line) && n < 36) {
      line[n++] = *p++;
    }
    line[n] = '\0';
    while (*p == ' ') {
      ++p;
    }
    font_draw_text(font, framebuffer, ix + pad, ty, line, text_color);
    ty += line_h;
  }
  ty = iy + pad + line_h * 3 + 2;
  font_draw_text(font, framebuffer, ix + pad, ty, "Name:", text_color);
  ty += line_h;
  char shown[NAME_ENTRY_NAME_LEN + 2];
  snprintf(shown, sizeof(shown), "%s_", dlg->name);
  font_draw_text(font, framebuffer, ix + pad, ty, shown, select_color);
}
