#include "core/howmuch_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/strutil.h"

void howmuch_init(HowmuchDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
}

void howmuch_close(HowmuchDialog* dlg) {
  if (!dlg) {
    return;
  }
  dlg->open = false;
  dlg->kind = HOWMUCH_KIND_NONE;
}

static void howmuch_sync_field(HowmuchDialog* dlg) {
  if (!dlg) {
    return;
  }
  snprintf(dlg->field, sizeof(dlg->field), "%d", dlg->amount);
  dlg->field_selected = true;
}

static void howmuch_clamp(HowmuchDialog* dlg) {
  if (!dlg) {
    return;
  }
  if (dlg->amount < 0) {
    dlg->amount = 0;
  }
  if (dlg->amount > dlg->max_amount) {
    dlg->amount = dlg->max_amount;
  }
}

static void howmuch_finish(HowmuchDialog* dlg, bool cancelled) {
  if (!dlg) {
    return;
  }
  dlg->has_result = true;
  dlg->result_cancelled = cancelled;
  dlg->result_amount = cancelled ? 0 : dlg->amount;
  dlg->result_kind = dlg->kind;
  dlg->open = false;
}

bool howmuch_open(
  HowmuchDialog* dlg,
  HowmuchKind kind,
  const char* prompt,
  int max_amount,
  int initial_amount,
  int cargo,
  int payload
) {
  if (!dlg || kind == HOWMUCH_KIND_NONE) {
    return false;
  }
  howmuch_init(dlg);
  dlg->has_result = false;
  dlg->kind = kind;
  dlg->max_amount = max_amount < 0 ? 0 : max_amount;
  dlg->amount = initial_amount;
  howmuch_clamp(dlg);
  dlg->result_cargo = cargo;
  dlg->result_payload = payload;
  str_copy_trunc(
    dlg->prompt, sizeof(dlg->prompt), prompt && prompt[0] ? prompt : "How much?"
  );
  howmuch_sync_field(dlg);
  dlg->open = true;
  return true;
}

bool howmuch_handle_input(HowmuchDialog* dlg, const ColonizeInputState* input) {
  if (!dlg || !dlg->open || !input) {
    return false;
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    howmuch_finish(dlg, true);
    return true;
  }
  if (input->last_key == COLONIZE_KEY_ENTER) {
    howmuch_finish(dlg, false);
    return true;
  }

  int delta = 0;
  if (colonize_key_up(input->last_key)) {
    delta = input->shift_held ? 10 : 1;
  } else if (colonize_key_down(input->last_key)) {
    delta = input->shift_held ? -10 : -1;
  }
  if (delta != 0) {
    dlg->amount += delta;
    howmuch_clamp(dlg);
    howmuch_sync_field(dlg);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_BACKSPACE) {
    if (dlg->field_selected) {
      dlg->amount = 0;
      howmuch_sync_field(dlg);
      dlg->field_selected = false;
    } else {
      size_t n = strlen(dlg->field);
      if (n > 0) {
        dlg->field[n - 1] = '\0';
        dlg->amount = dlg->field[0] ? atoi(dlg->field) : 0;
        howmuch_clamp(dlg);
      }
    }
    return true;
  }

  if (input->text_input_len > 0) {
    for (int i = 0; i < input->text_input_len; ++i) {
      const char ch = input->text_input[i];
      if (ch == '+' || ch == '=') {
        dlg->amount += input->shift_held ? 10 : 1;
        howmuch_clamp(dlg);
        howmuch_sync_field(dlg);
        continue;
      }
      if (ch == '-' || ch == '_') {
        dlg->amount -= input->shift_held ? 10 : 1;
        howmuch_clamp(dlg);
        howmuch_sync_field(dlg);
        continue;
      }
      if (ch < '0' || ch > '9') {
        continue;
      }
      if (dlg->field_selected) {
        dlg->field[0] = ch;
        dlg->field[1] = '\0';
        dlg->field_selected = false;
      } else {
        size_t n = strlen(dlg->field);
        if (n + 1 < sizeof(dlg->field)) {
          dlg->field[n] = ch;
          dlg->field[n + 1] = '\0';
        }
      }
      dlg->amount = atoi(dlg->field);
      howmuch_clamp(dlg);
      if (dlg->amount != atoi(dlg->field)) {
        howmuch_sync_field(dlg);
        dlg->field_selected = false;
      }
    }
    return true;
  }

  if (input->mouse_right_clicked) {
    howmuch_finish(dlg, true);
    return true;
  }
  if (input->mouse_left_clicked) {
    if (input->mouse_x < dlg->dialog_x || input->mouse_y < dlg->dialog_y ||
        input->mouse_x >= dlg->dialog_x + dlg->dialog_w ||
        input->mouse_y >= dlg->dialog_y + dlg->dialog_h) {
      howmuch_finish(dlg, true);
    } else {
      howmuch_finish(dlg, false);
    }
    return true;
  }
  return true;
}

void howmuch_render(
  HowmuchDialog* dlg,
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
  const int prompt_h = line_h * 3;
  const int dialog_h = POPUP_FRAME_INSET * 2 + pad + prompt_h + pad + line_h * 2 + pad;
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
  (void)ih;
  if (!font) {
    return;
  }
  int ty = iy + pad;
  /* Draw prompt truncated to ~3 lines by character budget. */
  {
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
      popup_draw_text_shadowed(font, framebuffer, ix + pad, ty, line, text_color);
      ty += line_h;
    }
  }
  ty = iy + pad + prompt_h + 2;
  popup_draw_text_shadowed(font, framebuffer, ix + pad, ty, "Amount:", text_color);
  ty += line_h;
  char shown[24];
  snprintf(shown, sizeof(shown), "%s_", dlg->field);
  popup_draw_text_shadowed(font, framebuffer, ix + pad, ty, shown, select_color);
  (void)iw;
}
