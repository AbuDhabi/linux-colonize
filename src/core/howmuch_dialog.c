#include "core/howmuch_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/popup.h"
#include "core/strutil.h"
#include "platform/diagnostics.h"

/* @HOWMUCHn kind -> readable name, for the debug log only. */
static const char* howmuch_kind_name(HowmuchKind kind) {
  switch (kind) {
    case HOWMUCH_KIND_LOAD: return "LOAD";
    case HOWMUCH_KIND_UNLOAD: return "UNLOAD";
    case HOWMUCH_KIND_MOVE: return "MOVE";
    case HOWMUCH_KIND_BUY: return "BUY";
    case HOWMUCH_KIND_SELL: return "SELL";
    case HOWMUCH_KIND_SOUND_TEST: return "SOUND_TEST";
    default: return "NONE";
  }
}

void howmuch_init(HowmuchDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
}

/* Single close point for the finish path; see name_entry_dialog.c. */
static void howmuch_close(HowmuchDialog* dlg) {
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
  howmuch_close(dlg);
  diag_info(
    "POPUP answered tag=HOWMUCH_%s %s amount=%d/%d cargo=%d",
    howmuch_kind_name(dlg->result_kind), cancelled ? "cancelled" : "picked",
    dlg->result_amount, dlg->max_amount, dlg->result_cargo
  );
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
  diag_info(
    "POPUP show tag=HOWMUCH_%s kind=amount max=%d start=%d cargo=%d prompt=\"%s\"",
    howmuch_kind_name(dlg->kind), dlg->max_amount, dlg->amount, dlg->result_cargo, dlg->prompt
  );
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
  const int line_h = popup_dialog_line_h(font);
  const int pad = 6;
  const int dialog_w = 200;
  const int prompt_h = line_h * 3;
  const int dialog_h = POPUP_FRAME_INSET * 2 + pad + prompt_h + pad + line_h * 2 + pad;
  /* The prompt is chunked to 3 rows of 36 characters, not word-wrapped; that
   * character budget is what DOS's How Much box has always drawn. */
  PopupPromptGeom g;
  popup_prompt_frame(
    framebuffer, font, wood_tile, colors, dialog_w, dialog_h, pad, dlg->prompt, 3, 36,
    text_color, &g
  );
  dlg->dialog_x = g.frame.x;
  dlg->dialog_y = g.frame.y;
  dlg->dialog_w = dialog_w;
  dlg->dialog_h = dialog_h;
  if (!font) {
    return;
  }
  const int ix = g.frame.inner_x;
  int ty = g.text_y + 2;
  popup_draw_text_shadowed(font, framebuffer, ix + pad, ty, "Amount:", text_color);
  ty += line_h;
  char shown[24];
  snprintf(shown, sizeof(shown), "%s_", dlg->field);
  popup_draw_text_shadowed(font, framebuffer, ix + pad, ty, shown, select_color);
}
