#include "core/save_load_dialog.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/savegame.h"
#include "core/strutil.h"
#include "core/ui_colors.h"

void save_load_init(SaveLoadDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
  dlg->width = 200;
}

void save_load_close(SaveLoadDialog* dlg) {
  if (!dlg) {
    return;
  }
  dlg->open = false;
  dlg->selection = 0;
  dlg->option_count = 0;
  dlg->prompt[0] = '\0';
  /* has_result / result_* intentionally preserved until the next open. */
}

static void save_load_format_label(
  char* out,
  size_t out_sz,
  int display_index /*1-based*/,
  int slot,
  const ColonizeSaveSlotInfo* info
) {
  if (!out || out_sz == 0) {
    return;
  }
  if (!info || !info->occupied) {
    snprintf(out, out_sz, "%d. Empty", display_index);
    return;
  }
  if (slot == 8) {
    snprintf(
      out,
      out_sz,
      "%d. %s  %u [Decade]",
      display_index,
      info->leader_name,
      (unsigned)info->year
    );
  } else if (slot == 9) {
    snprintf(
      out,
      out_sz,
      "%d. %s  %u [Turn]",
      display_index,
      info->leader_name,
      (unsigned)info->year
    );
  } else {
    snprintf(
      out, out_sz, "%d. %s  %u", display_index, info->leader_name, (unsigned)info->year
    );
  }
}

bool save_load_open(SaveLoadDialog* dlg, SaveLoadMode mode, const char* save_dir) {
  if (!dlg || !save_dir) {
    return false;
  }
  save_load_init(dlg);
  dlg->has_result = false;
  dlg->mode = mode;
  dlg->width = 200;

  if (mode == SAVE_LOAD_MODE_SAVE) {
    str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), "Save Game");
  } else {
    str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), "Load Game");
  }

  const int slot_max = (mode == SAVE_LOAD_MODE_SAVE) ? 7 : 9;
  int first_usable = -1;
  for (int slot = 0; slot <= slot_max; ++slot) {
    ColonizeSaveSlotInfo info;
    if (!savegame_probe_col1_slot(save_dir, slot, &info)) {
      memset(&info, 0, sizeof(info));
    }
    const int idx = dlg->option_count;
    if (idx >= SAVE_LOAD_MAX_SLOTS) {
      break;
    }
    dlg->slot_ids[idx] = slot;
    dlg->slot_occupied[idx] = info.occupied;
    save_load_format_label(
      dlg->options[idx], sizeof(dlg->options[idx]), slot + 1, slot, &info
    );
    if (mode == SAVE_LOAD_MODE_LOAD) {
      if (info.occupied && first_usable < 0) {
        first_usable = idx;
      }
    } else if (first_usable < 0) {
      first_usable = idx;
    }
    dlg->option_count++;
  }

  if (dlg->option_count <= 0) {
    save_load_close(dlg);
    return false;
  }
  dlg->selection = (first_usable >= 0) ? first_usable : 0;
  dlg->open = true;
  return true;
}

static int save_load_option_at_y(const SaveLoadDialog* dlg, int mouse_y) {
  if (!dlg || dlg->line_h <= 0 || dlg->option_count <= 0) {
    return -1;
  }
  const int rel = mouse_y - dlg->list_y0;
  if (rel < 0) {
    return -1;
  }
  const int idx = rel / dlg->line_h;
  if (idx < 0 || idx >= dlg->option_count) {
    return -1;
  }
  return idx;
}

static bool save_load_can_confirm(const SaveLoadDialog* dlg, int idx) {
  if (!dlg || idx < 0 || idx >= dlg->option_count) {
    return false;
  }
  if (dlg->mode == SAVE_LOAD_MODE_LOAD) {
    return dlg->slot_occupied[idx];
  }
  return true;
}

static void save_load_cancel(SaveLoadDialog* dlg) {
  dlg->has_result = false;
  save_load_close(dlg);
}

static void save_load_confirm(SaveLoadDialog* dlg, int idx) {
  if (!save_load_can_confirm(dlg, idx)) {
    return;
  }
  dlg->has_result = true;
  dlg->result_mode = dlg->mode;
  dlg->result_slot = dlg->slot_ids[idx];
  save_load_close(dlg);
}

static void save_load_move_selection(SaveLoadDialog* dlg, int delta) {
  if (!dlg || dlg->option_count <= 0 || delta == 0) {
    return;
  }
  int idx = dlg->selection;
  for (int step = 0; step < dlg->option_count; ++step) {
    idx += delta;
    if (idx < 0) {
      idx = dlg->option_count - 1;
    } else if (idx >= dlg->option_count) {
      idx = 0;
    }
    if (dlg->mode == SAVE_LOAD_MODE_SAVE || dlg->slot_occupied[idx]) {
      dlg->selection = idx;
      return;
    }
  }
}

bool save_load_handle_input(SaveLoadDialog* dlg, const ColonizeInputState* input) {
  if (!dlg || !dlg->open || !input) {
    return false;
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    save_load_cancel(dlg);
    return true;
  }
  if (colonize_key_up(input->last_key)) {
    save_load_move_selection(dlg, -1);
    return true;
  }
  if (colonize_key_down(input->last_key)) {
    save_load_move_selection(dlg, 1);
    return true;
  }
  if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
    if (dlg->selection >= 0 && dlg->selection < dlg->option_count) {
      save_load_confirm(dlg, dlg->selection);
    }
    return true;
  }

  if (input->mouse_left_clicked) {
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    if (mx < dlg->dialog_x || my < dlg->dialog_y || mx >= dlg->dialog_x + dlg->dialog_w ||
        my >= dlg->dialog_y + dlg->dialog_h) {
      save_load_cancel(dlg);
      return true;
    }
    const int idx = save_load_option_at_y(dlg, my);
    if (idx >= 0) {
      dlg->selection = idx;
      save_load_confirm(dlg, idx);
    }
    return true;
  }

  if (input->mouse_right_clicked) {
    save_load_cancel(dlg);
    return true;
  }

  return true; /* consume while open */
}

void save_load_render(
  SaveLoadDialog* dlg,
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
  const int pad_x = 6;
  const int pad_y = 4;
  const int prompt_h = dlg->prompt[0] ? line_h + 2 : 0;
  const int options_h = dlg->option_count * line_h;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad_y + prompt_h + options_h + pad_y;
  if (dialog_h < 40) {
    dialog_h = 40;
  }
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }

  int dialog_w = dlg->width;
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  int dialog_x = (framebuffer->width - dialog_w) / 2;
  int dialog_y = (framebuffer->height - dialog_h) / 2;
  if (dialog_y < MAP_MENU_BAR_H + 2) {
    dialog_y = MAP_MENU_BAR_H + 2;
  }

  ColonizePopupColors local_colors;
  if (!colors) {
    popup_colors_from_ui(&local_colors);
    colors = &local_colors;
  }

  int inner_x = 0;
  int inner_y = 0;
  int inner_w = 0;
  int inner_h = 0;
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    wood_tile,
    colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );

  dlg->dialog_x = dialog_x;
  dlg->dialog_y = dialog_y;
  dlg->dialog_w = dialog_w;
  dlg->dialog_h = dialog_h;
  dlg->line_h = line_h;

  int text_y = inner_y + pad_y;
  if (dlg->prompt[0] && font) {
    font_draw_text(font, framebuffer, inner_x + pad_x, text_y, dlg->prompt, text_color);
    text_y += prompt_h;
  }
  dlg->list_y0 = text_y;

  for (int i = 0; i < dlg->option_count; ++i) {
    const int row_y = text_y + i * line_h;
    if (i == dlg->selection) {
      for (int y = row_y - 1; y <= row_y + line_h - 2; ++y) {
        for (int x = inner_x + 1; x <= inner_x + inner_w - 2; ++x) {
          if (x >= 0 && y >= 0 && x < framebuffer->width && y < framebuffer->height) {
            framebuffer->pixels[y * framebuffer->width + x] = select_color;
          }
        }
      }
    }
    if (font) {
      font_draw_text(
        font, framebuffer, inner_x + pad_x, row_y, dlg->options[i], text_color
      );
    }
  }
}
