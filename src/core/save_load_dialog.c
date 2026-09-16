#include "core/save_load_dialog.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/savegame.h"
#include "core/strutil.h"
#include "core/ui_button.h"
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
  const char* diff = reports_difficulty_title(info->difficulty <= 4 ? info->difficulty : 4);
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
  dlg->hover_mx = -1;
  dlg->hover_my = -1;
  dlg->open = true;
  return true;
}

static int save_load_option_at_y(const SaveLoadDialog* dlg, int mouse_y) {
  return dlg ? popup_row_at_y(dlg->list_y0, dlg->line_h, dlg->option_count, mouse_y) : -1;
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
    if (!ui_rect_hit(dlg->dialog_x, dlg->dialog_y, dlg->dialog_w, dlg->dialog_h, mx, my)) {
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

  /* bugs.md: mousing over a row moves the highlight (only on real movement,
   * so it doesn't fight the arrow keys; unloadable rows don't take it). */
  if (input->mouse_x != dlg->hover_mx || input->mouse_y != dlg->hover_my) {
    dlg->hover_mx = input->mouse_x;
    dlg->hover_my = input->mouse_y;
    if (input->mouse_x >= dlg->dialog_x && input->mouse_x < dlg->dialog_x + dlg->dialog_w) {
      const int idx = save_load_option_at_y(dlg, input->mouse_y);
      if (idx >= 0 && save_load_can_confirm(dlg, idx)) {
        dlg->selection = idx;
      }
    }
  }

  return true; /* consume while open */
}

static const char* save_load_row_label(void* user, int index) {
  const SaveLoadDialog* dlg = (const SaveLoadDialog*)user;
  return dlg->options[index];
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
   * @width, frame adds 3 px per side, outer height = text + 12, centred and
   * clamped below the menu bar.
   *
   * Two pitches, off the same glyph height (FUN_6f74_0f16 = font[0], the 6-px
   * font counting as 5): the prompt is a body line at glyph_h + 1
   * (FUN_6f74_1198 @ OVL24 0x1234/0x124c), the slot rows are OPTION rows at
   * glyph_h + box[+0x46] (FUN_6f74_14c6 @ 0x1611-0x1628). FUN_7562_0052
   * appends each slot row with FUN_291f_0176 (decomp viceroy_unpacked.c
   * 119680), and FUN_291f_0176 is a straight thunk to FUN_6f74_0a00
   * (viceroy_unpacked_2.c 33285-33289) - the +0x54 option list - so the save
   * and load dialogs are +0x54 rows exactly like every ai_popup CHOICE.
   * box[+0x46] = (flags & 0x10) ? 0 : 3 (FUN_6f74_06d0 @ 0x078a-0x0799), and
   * GAME.TXT @SAVEGAME/@LOADGAME declare nothing but @width=190, so the box
   * is framed and the pitch is glyph_h + 3. All of that now lives in
   * popup_list_metrics_dos6f74, which also carries the FUN_6f74_14c6
   * widen-to-widest-row rule and the FUN_6f74_1b7c selection bar. */
  PopupListMetrics metrics;
  popup_list_metrics_dos6f74(font, &metrics);
  PopupListGeom geom;
  popup_list_render(
    framebuffer,
    font,
    wood_tile,
    colors,
    &metrics,
    dlg->width > 0 ? dlg->width : SAVE_LOAD_DEFAULT_WIDTH,
    dlg->prompt,
    dlg->option_count,
    dlg->selection,
    save_load_row_label,
    NULL,
    dlg,
    text_color,
    text_color,
    select_color,
    &geom
  );
  dlg->dialog_x = geom.frame.x;
  dlg->dialog_y = geom.frame.y;
  dlg->dialog_w = geom.frame.w;
  dlg->dialog_h = geom.frame.h;
  /* Hit-testing walks the slot rows, so this is their pitch, not the prompt's. */
  dlg->line_h = geom.line_h;
  dlg->list_y0 = geom.list_y0;
}
