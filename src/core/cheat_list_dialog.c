#include "core/cheat_list_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/strutil.h"
#include "core/ui_colors.h"

void cheat_list_init(CheatListDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
  dlg->width = 190;
}

void cheat_list_close(CheatListDialog* dlg) {
  if (!dlg) {
    return;
  }
  dlg->open = false;
  dlg->kind = CHEAT_LIST_KIND_NONE;
  dlg->selection = 0;
  dlg->option_count = 0;
  dlg->prompt[0] = '\0';
  /* has_result / result_* intentionally preserved until the next open/init. */
}

static bool cheat_list_is_directive(const char* line) {
  return line && line[0] == '@';
}

static void cheat_list_csv_field2(const char* line, char* out, size_t out_sz) {
  if (!out || out_sz == 0) {
    return;
  }
  out[0] = '\0';
  if (!line) {
    return;
  }
  const char* p = strchr(line, ',');
  if (!p) {
    str_copy_trunc(out, out_sz, line);
    return;
  }
  ++p;
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  size_t n = 0;
  while (*p && *p != ',' && n + 1 < out_sz) {
    out[n++] = *p++;
  }
  while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) {
    --n;
  }
  out[n] = '\0';
}

bool cheat_list_open_setview(CheatListDialog* dlg, const ColonizeMsgCatalog* debug_txt) {
  if (!dlg) {
    return false;
  }
  cheat_list_init(dlg);
  dlg->has_result = false;
  dlg->kind = CHEAT_LIST_KIND_SETVIEW;
  dlg->width = 190;

  static const char* k_fallback_prompt = "Select Viewpoint";
  static const char* k_fallback[] = {
    "English Map",
    "French Map",
    "Spanish Map",
    "Dutch Map",
    "Complete Map",
    "No Special View"
  };
  /* option_ids: 0..3 nation, -1 complete, -2 normal */
  static const int k_fallback_ids[] = {0, 1, 2, 3, -1, -2};

  const ColonizeMsgSection* section = debug_txt ? assets_msg_find(debug_txt, "SETVIEW") : NULL;
  if (!section) {
    str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), k_fallback_prompt);
    for (int i = 0; i < 6; ++i) {
      str_copy_trunc(dlg->options[i], sizeof(dlg->options[i]), k_fallback[i]);
      dlg->option_ids[i] = k_fallback_ids[i];
    }
    dlg->option_count = 6;
    dlg->open = true;
    return true;
  }

  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0') {
      continue;
    }
    if (cheat_list_is_directive(line)) {
      if (strncmp(line, "@width=", 7) == 0) {
        dlg->width = atoi(line + 7);
        if (dlg->width < 80) {
          dlg->width = 80;
        }
        if (dlg->width > 320) {
          dlg->width = 320;
        }
      }
      continue;
    }
    if (dlg->prompt[0] == '\0') {
      str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), line);
      continue;
    }
    if (dlg->option_count >= CHEAT_LIST_MAX_OPTIONS) {
      break;
    }
    str_copy_trunc(
      dlg->options[dlg->option_count], sizeof(dlg->options[dlg->option_count]), line
    );
    const int idx = dlg->option_count;
    if (idx < 6) {
      dlg->option_ids[idx] = k_fallback_ids[idx];
    } else {
      dlg->option_ids[idx] = -2;
    }
    dlg->option_count++;
  }

  if (dlg->option_count <= 0) {
    cheat_list_close(dlg);
    return false;
  }
  dlg->open = true;
  return true;
}

bool cheat_list_open_kill_indians(CheatListDialog* dlg, const ColonizeMsgCatalog* names) {
  if (!dlg) {
    return false;
  }
  cheat_list_init(dlg);
  dlg->has_result = false;
  dlg->kind = CHEAT_LIST_KIND_KILL_INDIANS;
  dlg->width = 190;
  str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), "Select Tribe To Kill");

  static const char* k_fallback[] = {
    "Inca", "Aztec", "Arawak", "Iroquois", "Cherokee", "Apache", "Sioux", "Tupi"
  };

  const ColonizeMsgSection* tribes = names ? assets_msg_find(names, "TRIBES") : NULL;
  for (int i = 0; i < 8; ++i) {
    char short_name[CHEAT_LIST_LABEL_LEN];
    short_name[0] = '\0';
    if (tribes && i < tribes->line_count && tribes->lines[i][0]) {
      cheat_list_csv_field2(tribes->lines[i], short_name, sizeof(short_name));
    }
    if (!short_name[0]) {
      str_copy_trunc(short_name, sizeof(short_name), k_fallback[i]);
    }
    str_copy_trunc(dlg->options[i], sizeof(dlg->options[i]), short_name);
    dlg->option_ids[i] = 4 + i;
  }
  dlg->option_count = 8;
  dlg->open = true;
  return true;
}

static int cheat_list_option_at_y(const CheatListDialog* dlg, int mouse_y) {
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

static void cheat_list_cancel(CheatListDialog* dlg) {
  dlg->has_result = false;
  cheat_list_close(dlg);
}

static void cheat_list_confirm(CheatListDialog* dlg, int idx) {
  if (!dlg || idx < 0 || idx >= dlg->option_count) {
    cheat_list_cancel(dlg);
    return;
  }
  dlg->has_result = true;
  dlg->result_kind = dlg->kind;
  dlg->result_id = dlg->option_ids[idx];
  str_copy_trunc(dlg->result_label, sizeof(dlg->result_label), dlg->options[idx]);
  cheat_list_close(dlg);
}

bool cheat_list_handle_input(CheatListDialog* dlg, const ColonizeInputState* input) {
  if (!dlg || !dlg->open || !input) {
    return false;
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    cheat_list_cancel(dlg);
    return true;
  }
  if (input->last_key == COLONIZE_KEY_UP && dlg->selection > 0) {
    dlg->selection--;
    return true;
  }
  if (input->last_key == COLONIZE_KEY_DOWN && dlg->selection + 1 < dlg->option_count) {
    dlg->selection++;
    return true;
  }
  if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
    if (dlg->selection >= 0 && dlg->selection < dlg->option_count) {
      cheat_list_confirm(dlg, dlg->selection);
    }
    return true;
  }

  if (input->mouse_left_clicked) {
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    if (mx < dlg->dialog_x || my < dlg->dialog_y || mx >= dlg->dialog_x + dlg->dialog_w ||
        my >= dlg->dialog_y + dlg->dialog_h) {
      cheat_list_cancel(dlg);
      return true;
    }
    const int idx = cheat_list_option_at_y(dlg, my);
    if (idx >= 0) {
      dlg->selection = idx;
      cheat_list_confirm(dlg, idx);
    }
    return true;
  }

  if (input->mouse_right_clicked) {
    cheat_list_cancel(dlg);
    return true;
  }

  return true; /* consume while open */
}

void cheat_list_render(
  CheatListDialog* dlg,
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
