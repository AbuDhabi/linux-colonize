#include "core/cheat_list_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/popup.h"
#include "core/reports.h"
#include "core/strutil.h"
#include "core/ui_button.h"
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

/*
 * Live-catalog row loader shared by the CHEAT Create Unit list stages
 * (DEBUG.TXT @CREATE / @CSHIP / @FOREIGN / @FOREIGN2). Same "live wins,
 * literal fallback" shape as cheat_list_open_setview, but returns plain
 * strings for a caller that builds its own labels[]/ids[] pair rather than
 * opening the dialog directly. Counts non-directive, non-blank rows (blank
 * lines are already dropped by assets_msg_load_file); `skip_rows` is how
 * many of those rows to skip before copying `count` of them into `out[]`
 * (skip_rows=0 count=1 fetches just the section's own prompt row). Missing
 * catalog/section/row falls back to fallback[i] per row; never mixes live
 * and fallback text within a single row.
 */
bool cheat_list_catalog_rows(
  const ColonizeMsgCatalog* catalog,
  const char* section_name,
  int skip_rows,
  const char* const* fallback,
  char out[][CHEAT_LIST_LABEL_LEN],
  int count
) {
  if (!out || count <= 0) {
    return false;
  }
  for (int i = 0; i < count; ++i) {
    str_copy_trunc(out[i], CHEAT_LIST_LABEL_LEN, fallback ? fallback[i] : "");
  }
  const ColonizeMsgSection* section = catalog ? assets_msg_find(catalog, section_name) : NULL;
  if (!section) {
    return false;
  }
  int row = 0;
  int filled = 0;
  for (int i = 0; i < section->line_count && filled < count; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0' || cheat_list_is_directive(line)) {
      continue;
    }
    if (row < skip_rows) {
      row++;
      continue;
    }
    str_copy_trunc(out[filled], CHEAT_LIST_LABEL_LEN, line);
    filled++;
    row++;
  }
  return true;
}

bool cheat_list_open_setview(CheatListDialog* dlg, const ColonizeMsgCatalog* debug_txt) {
  if (!dlg) {
    return false;
  }
  cheat_list_init(dlg);
  dlg->has_result = false;
  dlg->kind = CHEAT_LIST_KIND_SETVIEW;
  dlg->width = 190;

  static const char* k_fallback_prompt = "";
  static const char* k_fallback[] = {
    "",
    "",
    "",
    "",
    "",
    ""
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

  /* @TRIBES field 1 — the singular tribe word ("Inca"), the same column the
   * map panel and the unit labels name one tribe by. The caller's catalog
   * wins; reports_tribe_singular_name supplies the literal fallback. */
  for (int i = 0; i < 8; ++i) {
    char short_name[CHEAT_LIST_LABEL_LEN];
    if (!assets_msg_row_field(names, "TRIBES", i, 1, short_name, sizeof(short_name)) ||
        !short_name[0]) {
      str_copy_trunc(short_name, sizeof(short_name), reports_tribe_singular_name(i));
    }
    str_copy_trunc(dlg->options[i], sizeof(dlg->options[i]), short_name);
    dlg->option_ids[i] = 4 + i;
  }
  dlg->option_count = 8;
  dlg->open = true;
  return true;
}

static bool cheat_list_open_simple_list(
  CheatListDialog* dlg,
  CheatListKind kind,
  const char* prompt,
  const char* const* labels,
  const int* ids,
  int count,
  int width
) {
  if (!dlg || !labels || !ids || count <= 0) {
    return false;
  }
  if (count > CHEAT_LIST_MAX_OPTIONS) {
    count = CHEAT_LIST_MAX_OPTIONS;
  }
  cheat_list_init(dlg);
  dlg->has_result = false;
  dlg->kind = kind;
  dlg->width = width;
  dlg->multi_select = false;
  str_copy_trunc(
    dlg->prompt, sizeof(dlg->prompt), prompt && prompt[0] ? prompt : "Choose entry"
  );
  for (int i = 0; i < count; ++i) {
    str_copy_trunc(
      dlg->options[i],
      sizeof(dlg->options[i]),
      labels[i] && labels[i][0] ? labels[i] : "(unnamed)"
    );
    dlg->option_ids[i] = ids[i];
  }
  dlg->option_count = count;
  dlg->open = true;
  return true;
}

bool cheat_list_open_find_colony(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* colony_ids,
  int count
) {
  return cheat_list_open_simple_list(
    dlg, CHEAT_LIST_KIND_FIND_COLONY, prompt, labels, colony_ids, count, 190
  );
}

bool cheat_list_open_trade_select(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* route_ids,
  int count
) {
  return cheat_list_open_simple_list(
    dlg, CHEAT_LIST_KIND_TRADE_SELECT, prompt, labels, route_ids, count, 190
  );
}

bool cheat_list_open_trade_dest(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* dest_ids,
  int count
) {
  return cheat_list_open_simple_list(
    dlg, CHEAT_LIST_KIND_TRADE_DEST, prompt, labels, dest_ids, count, 190
  );
}

bool cheat_list_open_goto_port(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* dest_ids,
  int count
) {
  return cheat_list_open_simple_list(
    dlg, CHEAT_LIST_KIND_GOTO_PORT, prompt, labels, dest_ids, count, 190
  );
}

void cheat_list_set_selection(CheatListDialog* dlg, int index) {
  if (!dlg || !dlg->open || dlg->option_count <= 0) {
    return;
  }
  if (index < 0) {
    index = 0;
  }
  if (index >= dlg->option_count) {
    index = dlg->option_count - 1;
  }
  dlg->selection = index;
}

bool cheat_list_open_trade_cargo_one(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* cargo_ids,
  int count
) {
  return cheat_list_open_simple_list(
    dlg, CHEAT_LIST_KIND_TRADE_CARGO_ONE, prompt, labels, cargo_ids, count, 190
  );
}

bool cheat_list_open_create_unit(
  CheatListDialog* dlg,
  const char* prompt,
  const char* const* labels,
  const int* ids,
  int count
) {
  return cheat_list_open_simple_list(
    dlg, CHEAT_LIST_KIND_CREATE_UNIT, prompt, labels, ids, count, 190
  );
}

bool cheat_list_open_set_human(CheatListDialog* dlg, const ColonizeMsgCatalog* debug_txt) {
  static const char* k_fallback_prompt = "";
  static const int k_fallback_ids[] = {0, 1, 2, 3, -1};
  if (!dlg) {
    return false;
  }
  cheat_list_init(dlg);
  dlg->has_result = false;
  dlg->kind = CHEAT_LIST_KIND_SET_HUMAN;
  dlg->width = 190;

  const ColonizeMsgSection* section = debug_txt ? assets_msg_find(debug_txt, "SETHUMAN") : NULL;
  if (!section) {
    str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), k_fallback_prompt);
    /* @NATIONALITY rows 0-3 plus LABELS.TXT @MISC row 3 for no human. */
    for (int i = 0; i < 5; ++i) {
      str_copy_trunc(
        dlg->options[i],
        sizeof(dlg->options[i]),
        i < 4 ? reports_nation_adjective_display_name(i) : reports_misc_display_word(3, "")
      );
      dlg->option_ids[i] = k_fallback_ids[i];
    }
    dlg->option_count = 5;
    dlg->open = true;
    return true;
  }

  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0') {
      continue;
    }
    if (cheat_list_is_directive(line)) {
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
    dlg->option_ids[idx] = idx < 5 ? k_fallback_ids[idx] : -1;
    dlg->option_count++;
  }
  if (dlg->option_count <= 0) {
    cheat_list_close(dlg);
    return false;
  }
  dlg->open = true;
  return true;
}

/* Strip a single leading '~' hotkey-underline marker (MENU.TXT/DEBUG.TXT convention). */
static const char* cheat_list_strip_tilde(const char* s) {
  return (s && s[0] == '~') ? s + 1 : s;
}

bool cheat_list_open_debug_flags(
  CheatListDialog* dlg,
  const ColonizeMsgCatalog* debug_txt,
  uint16_t initial_mask
) {
  static const char* k_fallback_prompt = "";
  static const char* k_fallback[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    ""
  };
  if (!dlg) {
    return false;
  }
  cheat_list_init(dlg);
  dlg->has_result = false;
  dlg->kind = CHEAT_LIST_KIND_DEBUG_FLAGS;
  dlg->width = 220;
  dlg->multi_select = true;
  dlg->selected_mask = initial_mask;

  const ColonizeMsgSection* section = debug_txt ? assets_msg_find(debug_txt, "OPTIONS") : NULL;
  int n = 0;
  const char* labels[7];
  if (section) {
    for (int i = 0; i < section->line_count && n < 7; ++i) {
      const char* line = section->lines[i];
      if (!line || line[0] == '\0' || cheat_list_is_directive(line)) {
        continue;
      }
      if (dlg->prompt[0] == '\0' && n == 0) {
        /* First non-directive, non-checkbox line is the prompt; checkbox
         * items all carry a leading '~' in @OPTIONS. */
        if (line[0] != '~') {
          str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), line);
          continue;
        }
      }
      labels[n++] = cheat_list_strip_tilde(line);
    }
  }
  if (n < 7) {
    n = 7;
    for (int i = 0; i < 7; ++i) {
      labels[i] = k_fallback[i];
    }
  }
  if (dlg->prompt[0] == '\0') {
    str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), k_fallback_prompt);
  }
  {
    /* Multi-select only saves on Enter (Esc / click-outside cancels and
     * discards toggles) — say so, matching the TRADE cargo picker hint. */
    char with_hint[COLONIZE_MSG_LINE_LEN + 32];
    snprintf(with_hint, sizeof(with_hint), "%s (Space toggle, Enter OK)", dlg->prompt);
    str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), with_hint);
  }
  for (int i = 0; i < n; ++i) {
    const int on = (initial_mask & (uint16_t)(1u << i)) != 0;
    snprintf(
      dlg->options[i], sizeof(dlg->options[i]), "%s %s", on ? "[x]" : "[ ]", labels[i]
    );
    dlg->option_ids[i] = i;
  }
  dlg->option_count = n;
  dlg->open = true;
  return true;
}

/*
 * Every multi-select option is rendered "[x] Label" / "[ ] Label" (trade
 * cargo, debug flags, …); flip the bracket char in place instead of
 * rebuilding the label from a kind-specific name table.
 */
static void cheat_list_flip_checkbox(CheatListDialog* dlg, int idx) {
  if (!dlg || idx < 0 || idx >= dlg->option_count) {
    return;
  }
  char* opt = dlg->options[idx];
  if (opt[0] == '[' && opt[2] == ']') {
    opt[1] = (dlg->selected_mask & (uint16_t)(1u << idx)) ? 'x' : ' ';
  }
}

static int cheat_list_option_at_y(const CheatListDialog* dlg, int mouse_y) {
  return dlg ? popup_row_at_y(dlg->list_y0, dlg->line_h, dlg->option_count, mouse_y) : -1;
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
  dlg->result_mask = dlg->multi_select ? dlg->selected_mask : 0;
  str_copy_trunc(dlg->result_label, sizeof(dlg->result_label), dlg->options[idx]);
  cheat_list_close(dlg);
}

static void cheat_list_confirm_multi(CheatListDialog* dlg) {
  if (!dlg) {
    return;
  }
  dlg->has_result = true;
  dlg->result_kind = dlg->kind;
  dlg->result_id = -1;
  dlg->result_mask = dlg->selected_mask;
  dlg->result_label[0] = '\0';
  cheat_list_close(dlg);
}

static void cheat_list_toggle_multi(CheatListDialog* dlg, int idx) {
  if (!dlg || !dlg->multi_select || idx < 0 || idx >= dlg->option_count) {
    return;
  }
  dlg->selected_mask ^= (uint16_t)(1u << idx);
  cheat_list_flip_checkbox(dlg, idx);
}

bool cheat_list_handle_input(CheatListDialog* dlg, const ColonizeInputState* input) {
  if (!dlg || !dlg->open || !input) {
    return false;
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    cheat_list_cancel(dlg);
    return true;
  }
  if (colonize_key_up(input->last_key) && dlg->selection > 0) {
    dlg->selection--;
    return true;
  }
  if (colonize_key_down(input->last_key) && dlg->selection + 1 < dlg->option_count) {
    dlg->selection++;
    return true;
  }
  if (dlg->multi_select) {
    if (input->last_key == COLONIZE_KEY_SPACE) {
      cheat_list_toggle_multi(dlg, dlg->selection);
      return true;
    }
    if (input->last_key == COLONIZE_KEY_ENTER) {
      cheat_list_confirm_multi(dlg);
      return true;
    }
  } else if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
    if (dlg->selection >= 0 && dlg->selection < dlg->option_count) {
      cheat_list_confirm(dlg, dlg->selection);
    }
    return true;
  }

  if (input->mouse_left_clicked) {
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    if (!ui_rect_hit(dlg->dialog_x, dlg->dialog_y, dlg->dialog_w, dlg->dialog_h, mx, my)) {
      cheat_list_cancel(dlg);
      return true;
    }
    const int idx = cheat_list_option_at_y(dlg, my);
    if (idx >= 0) {
      dlg->selection = idx;
      if (dlg->multi_select) {
        cheat_list_toggle_multi(dlg, idx);
      } else {
        cheat_list_confirm(dlg, idx);
      }
    }
    return true;
  }

  if (input->mouse_right_clicked) {
    cheat_list_cancel(dlg);
    return true;
  }

  return true; /* consume while open */
}

static const char* cheat_list_row_label(void* user, int index) {
  const CheatListDialog* dlg = (const CheatListDialog*)user;
  return dlg->options[index];
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
  PopupListMetrics metrics;
  popup_list_metrics_classic(font, &metrics);
  PopupListGeom geom;
  popup_list_render(
    framebuffer,
    font,
    wood_tile,
    colors,
    &metrics,
    dlg->width,
    dlg->prompt,
    dlg->option_count,
    dlg->selection,
    cheat_list_row_label,
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
  dlg->line_h = geom.line_h;
  dlg->list_y0 = geom.list_y0;
}
