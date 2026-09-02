#include "core/save_load_dialog.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/savegame.h"
#include "core/strutil.h"
#include "core/ui_colors.h"

/* GAME.TXT @SAVEGAME/@LOADGAME @width (DOS 6f74 content width). */
#define SAVE_LOAD_DEFAULT_WIDTH 190

void save_load_init(SaveLoadDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
  dlg->width = SAVE_LOAD_DEFAULT_WIDTH;
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

/*
 * FUN_7562_0052 row: "<Difficulty> <Leader> of the <Nation>, <Season> <Year>";
 * empty slot = DS:0x20ee "(EMPTY)". Difficulty titles = DS:0x8394 table,
 * nation = NAMES.TXT @NATIONALITY, "of the" = LABELS.TXT @MISC 19. The DOS
 * builder trims the leader name until it measures under 0x65 px.
 */
static void save_load_format_label(
  char* out,
  size_t out_sz,
  const ColonizeSaveSlotInfo* info,
  const ColonizeFont* font
) {
  if (!out || out_sz == 0) {
    return;
  }
  if (!info || !info->occupied) {
    str_copy_trunc(out, out_sz, "(EMPTY)");
    return;
  }
  static const char* k_titles[5] = {
    "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
  };
  const char* diff =
    k_titles[info->difficulty <= 4 ? info->difficulty : 4];
  char leader[24];
  str_copy_trunc(leader, sizeof(leader), info->leader_name);
  if (font) {
    size_t n = strlen(leader);
    while (n > 0 && font_text_width(font, leader) >= 0x65) {
      leader[--n] = '\0';
    }
  }
  snprintf(
    out,
    out_sz,
    "%s %s of the %s, %s %u",
    diff,
    leader,
    reports_nation_adjective_display_name(info->human_nation),
    info->autumn ? "Autumn" : "Spring",
    (unsigned)info->year
  );
}

bool save_load_open(
  SaveLoadDialog* dlg,
  SaveLoadMode mode,
  const char* save_dir,
  const ColonizeMsgCatalog* messages,
  const ColonizeFont* font
) {
  if (!dlg || !save_dir) {
    return false;
  }
  save_load_init(dlg);
  dlg->has_result = false;
  dlg->mode = mode;

  /* GAME.TXT @SAVEGAME "Select Save Slot" / @LOADGAME "Select Game To Load". */
  const char* section = (mode == SAVE_LOAD_MODE_SAVE) ? "SAVEGAME" : "LOADGAME";
  const char* fallback =
    (mode == SAVE_LOAD_MODE_SAVE) ? "Select Save Slot" : "Select Game To Load";
  str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), fallback);
  if (messages) {
    const ColonizeMsgSection* sec = assets_msg_find(messages, section);
    if (sec) {
      char body[64];
      if (popup_msg_section_body(sec, body, sizeof(body), true) > 0) {
        str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), body);
      }
      const int w = popup_msg_section_width(sec);
      if (w > 0) {
        dlg->width = w;
      }
    }
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
    save_load_format_label(dlg->options[idx], sizeof(dlg->options[idx]), &info, font);
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

  /* DOS 6f74 compositor metrics (same as ai_popup_render): content width =
   * @width, frame adds 3 px per side, line pitch = glyph height + 1 (6-px
   * font counts as 5), outer height = text + 12, centred and clamped below
   * the menu bar. */
  int glyph_h = font ? font->max_height : 6;
  if (glyph_h == 6) {
    glyph_h = 5;
  }
  const int line_h = glyph_h + 1;
  const int pad_x = 2;
  const int title_gap = dlg->prompt[0] ? 2 : 0;

  int content_w = dlg->width > 0 ? dlg->width : SAVE_LOAD_DEFAULT_WIDTH;
  /* FUN_6f74_14c6: content width = max(@width, widest emitted row). */
  if (font) {
    for (int i = -1; i < dlg->option_count; ++i) {
      const char* row = (i < 0) ? dlg->prompt : dlg->options[i];
      const int w = font_text_width(font, row) + 2 * pad_x;
      if (w > content_w) {
        content_w = w;
      }
    }
  }
  if (content_w + 6 > framebuffer->width) {
    content_w = framebuffer->width - 6;
  }
  const int dialog_w = content_w + 6;
  const int prompt_h = dlg->prompt[0] ? line_h + title_gap : 0;
  const int options_h = dlg->option_count * line_h;
  int dialog_h = 12 + prompt_h + options_h;
  if (dialog_h > framebuffer->height) {
    dialog_h = framebuffer->height;
  }

  const int dialog_x = (framebuffer->width - dialog_w) / 2;
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

  /* FONTINTR unbold + black drop-shadow, like every other wood popup. */
  int text_y = inner_y + 3;
  if (dlg->prompt[0] && font) {
    popup_draw_text_shadowed(
      font, framebuffer, inner_x + pad_x, text_y, dlg->prompt, text_color
    );
    text_y += prompt_h;
  }
  dlg->list_y0 = text_y;

  for (int i = 0; i < dlg->option_count; ++i) {
    const int row_y = text_y + i * line_h;
    if (i == dlg->selection) {
      for (int y = row_y - 1; y <= row_y + line_h - 2; ++y) {
        for (int x = inner_x + 2; x <= inner_x + inner_w - 3; ++x) {
          if (x >= 0 && y >= 0 && x < framebuffer->width && y < framebuffer->height) {
            framebuffer->pixels[y * framebuffer->width + x] = select_color;
          }
        }
      }
    }
    if (font) {
      popup_draw_text_shadowed(
        font, framebuffer, inner_x + pad_x, row_y, dlg->options[i], text_color
      );
    }
  }
}
