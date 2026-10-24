#include "core/cheat_list_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/map_menu.h"
#include "core/popup.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/strutil.h"
#include "core/ui_button.h"
#include "core/ui_colors.h"

void cheat_list_init(CheatListDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
  dlg->width = 190;
  dlg->mss = -1;
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

/*
 * Shared DEBUG.TXT list-section parser for @SETVIEW / @SETHUMAN (port_bugs.md #1111:
 * the two copies drifted on @width= and the overflow id). First text line is
 * the prompt, the rest are rows; row i gets ids[i], and any row past the
 * table keeps the last row's id (the "no special view" / "none" choice).
 */
static bool cheat_list_fill_from_section(
  CheatListDialog* dlg, const ColonizeMsgSection* section, const int* ids, int id_count
) {
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
    dlg->option_ids[idx] = ids[idx < id_count ? idx : id_count - 1];
    dlg->option_count++;
  }
  if (dlg->option_count <= 0) {
    cheat_list_close(dlg);
    return false;
  }
  dlg->open = true;
  return true;
}

bool cheat_list_open_setview(CheatListDialog* dlg, const ColonizeMsgCatalog* debug_txt) {
  if (!dlg) {
    return false;
  }
  cheat_list_init(dlg);
  dlg->has_result = false;
  dlg->kind = CHEAT_LIST_KIND_SETVIEW;
  dlg->mss = 3; /* FUN_2b5a_2866 `MOV word [0x1f5e],3` before @SETVIEW (ndisasm 0x23cb9) */
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

  return cheat_list_fill_from_section(dlg, section, k_fallback_ids, 6);
}

bool cheat_list_open_kill_indians(
  CheatListDialog* dlg, const ColonizeMsgCatalog* names, uint8_t alive_mask
) {
  if (!dlg || alive_mask == 0) {
    return false;
  }
  cheat_list_init(dlg);
  dlg->has_result = false;
  dlg->kind = CHEAT_LIST_KIND_KILL_INDIANS;
  dlg->width = 190;
  /* port_bugs.md #990: no DEBUG.TXT section carries a prompt for the kill-tribe
   * cheat, so the rows (the @TRIBES names) stand alone. */
  dlg->prompt[0] = '\0';

  /* @TRIBES field 1 — the singular tribe word ("Inca"), the same column the
   * map panel and the unit labels name one tribe by. The caller's catalog
   * wins; reports_tribe_singular_name supplies the literal fallback. */
  /* DOS case 0x67 (ndisasm 0x23bdc-0x23c9b): one row per tribe whose indian
   * record +3 bit 0x80 (extinct) is clear, id = tribe + 1; none left = the
   * cheat exits without a list. */
  int n = 0;
  for (int i = 0; i < 8; ++i) {
    if (!(alive_mask & (1u << i))) {
      continue;
    }
    char short_name[CHEAT_LIST_LABEL_LEN];
    if (!assets_msg_row_field(names, "TRIBES", i, 1, short_name, sizeof(short_name)) ||
        !short_name[0]) {
      str_copy_trunc(short_name, sizeof(short_name), reports_tribe_singular_name(i));
    }
    str_copy_trunc(dlg->options[n], sizeof(dlg->options[n]), short_name);
    dlg->option_ids[n] = 4 + i;
    ++n;
  }
  dlg->option_count = n;
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
  /* Catalog miss = empty prompt, never typed English (port_bugs.md #990). */
  str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), prompt ? prompt : "");
  for (int i = 0; i < count; ++i) {
    str_copy_trunc(
      dlg->options[i],
      sizeof(dlg->options[i]),
      labels[i] ? labels[i] : ""
    );
    dlg->option_ids[i] = ids[i];
  }
  dlg->option_count = count;
  dlg->open = true;
  return true;
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

/*
 * FUN_647e_01c6 raw 102368-102374 (647e:01d6-0227): n <= 10 colonies is one page of n + 1
 * slots, more is pages of 10. Slot k + e holds the k-th colony on every page (the Europe row is
 * slot 0, e = 1 when it is offered) and page p shows slots size * p .. size * p + size - 1
 * (raw 102380-102381). DOS counts (n + 9) / 10 pages, leaving slot n of Europe + 20 / 30 / 40
 * colonies on no page; the port counts the Europe slot too (docs/dos_bugs.md DB-53), which
 * changes only those three cases.
 */
void cheat_list_dest_page_math(int n, int e, int* page_size, int* page_count) {
  *page_count = 1;
  *page_size = n + 1;
  if (n >= 0xb) {
    *page_count = (n + e + 9) / 10;
    *page_size = 10;
  }
}

/* One FUN_647e_01c6 page (raw 102376-102446), rebuilt from scratch on every page turn: the
 * selection goes back to the first row unless FUN_291f_08ec finds the default on this page. */
static void cheat_list_dest_build_page(CheatListDialog* dlg) {
  const int first = dlg->page_size * dlg->page;
  const int last = first + dlg->page_size - 1;
  const char* more = reports_ctitle_word(6); /* DS:0x93aa = LABELS @CTITLE 6 "(More)" */
  int n = 0;
  dlg->selection = 0;
  dlg->disabled_mask = 0;
  if (dlg->page > 0) {
    str_copy_trunc(dlg->options[n], sizeof(dlg->options[n]), more);
    dlg->option_ids[n++] = 0x3e5;
  }
  for (int i = 0; i < dlg->dest_count; ++i) {
    if (i < first || i > last || dlg->dest_at[i] == CHEAT_LIST_DEST_OMITTED) {
      continue;
    }
    str_copy_trunc(dlg->options[n], sizeof(dlg->options[n]), dlg->dest_labels[i]);
    dlg->option_ids[n] = dlg->dest_ids[i];
    if (dlg->dest_at[i] == CHEAT_LIST_DEST_GREYED) {
      dlg->disabled_mask |= (uint16_t)(1u << n);
    } else if (dlg->dest_ids[i] == dlg->dest_default) {
      dlg->selection = n;
    }
    n++;
  }
  if (dlg->page < dlg->page_count - 1) {
    str_copy_trunc(dlg->options[n], sizeof(dlg->options[n]), more);
    dlg->option_ids[n++] = 0x3e6;
  } else if (dlg->dest_delete) {
    /* DS:0x93ee = LABELS @ROUTE 8, row id 0x3e9: 01c6 returns 1000. */
    str_copy_trunc(dlg->options[n], sizeof(dlg->options[n]), dlg->dest_delete_label);
    dlg->option_ids[n++] = 1000;
  }
  dlg->option_count = n;
}

bool cheat_list_open_dest_pages(
  CheatListDialog* dlg,
  CheatListKind kind,
  const char* prompt,
  const char* const* labels,
  const int* ids,
  const uint8_t* at,
  int count,
  bool europe,
  int default_id,
  const char* delete_label
) {
  if (!dlg || count < 0 || count > CHEAT_LIST_DEST_MAX || (count > 0 && (!labels || !ids))) {
    return false;
  }
  cheat_list_init(dlg);
  dlg->kind = kind;
  str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), prompt ? prompt : "");
  dlg->paged = true;
  dlg->dest_europe = europe && count > 0;
  dlg->dest_delete = delete_label != NULL;
  str_copy_trunc(dlg->dest_delete_label, sizeof(dlg->dest_delete_label), delete_label ? delete_label : "");
  dlg->dest_default = default_id;
  dlg->dest_count = count;
  for (int i = 0; i < count; ++i) {
    str_copy_trunc(dlg->dest_labels[i], sizeof(dlg->dest_labels[i]), labels[i] ? labels[i] : "");
    dlg->dest_ids[i] = ids[i];
    dlg->dest_at[i] = at ? at[i] : CHEAT_LIST_DEST_PLAIN;
  }
  const int e = dlg->dest_europe ? 1 : 0;
  cheat_list_dest_page_math(count - e, e, &dlg->page_size, &dlg->page_count);
  cheat_list_dest_build_page(dlg);
  if (dlg->option_count <= 0) {
    /* 647e:048f: box+2 < 1, the box is freed unseen and 01c6 returns -1. */
    cheat_list_close(dlg);
    return false;
  }
  dlg->open = true;
  return true;
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
  int count,
  int mss
) {
  /* DEBUG.TXT @CREATE / @CREATE2 / @CSHIP / @FOREIGN carry no @width: the generic section box
   * (181f:0998, ndisasm 0x239b2 / 0x23a94 / 0x23b55) keeps the FUN_6f74_06d0 0x50 default and
   * the rows widen it (port_bugs.md #1221). */
  const bool ok = cheat_list_open_simple_list(
    dlg, CHEAT_LIST_KIND_CREATE_UNIT, prompt, labels, ids, count, 0
  );
  dlg->mss = ok ? mss : -1;
  return ok;
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
  dlg->mss = 3; /* FUN_2b5a_2866 `MOV word [0x1f5e],3` before @SETHUMAN (ndisasm 0x23d06) */
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

  return cheat_list_fill_from_section(dlg, section, k_fallback_ids, 5);
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
      labels[n++] = line;
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
    /* DOS @checkbox row text (FUN_6f74_0a00 / 1b7c, as options_dialog): DS:0x1f91 "~[ "
     * with ']' on, '[' off (the font's filled / empty check circles) + the raw label. */
    snprintf(dlg->options[i], sizeof(dlg->options[i]), "~%c %s", on ? ']' : '[', labels[i]);
    dlg->option_ids[i] = i;
  }
  dlg->option_count = n;
  dlg->open = true;
  return true;
}

/*
 * A multi-select option is rendered "~] Label" / "~[ Label" (the DOS check glyphs);
 * flip the glyph in place (FUN_6f74_1b7c rewrites byte 1) instead of rebuilding the label.
 */
static void cheat_list_flip_checkbox(CheatListDialog* dlg, int idx) {
  if (!dlg || idx < 0 || idx >= dlg->option_count) {
    return;
  }
  char* opt = dlg->options[idx];
  if (opt[0] == '~' && (opt[1] == '[' || opt[1] == ']')) {
    opt[1] = (dlg->selected_mask & (uint16_t)(1u << idx)) ? ']' : '[';
  }
}

static int cheat_list_option_at_y(const CheatListDialog* dlg, int mouse_y) {
  return dlg ? popup_row_at_y(dlg->list_y0, dlg->line_h, dlg->option_count, mouse_y) : -1;
}

static void cheat_list_cancel(CheatListDialog* dlg) {
  if (!dlg) {
    return;
  }
  dlg->has_result = false;
  if (dlg->kind == CHEAT_LIST_KIND_SETVIEW) {
    /* DOS case 0x65 (ND 0x23CDE-0x23CF0): choice 0 (Esc) resets the view
     * exactly like the No Special View row (port_bugs.md #1170). */
    dlg->has_result = true;
    dlg->result_kind = CHEAT_LIST_KIND_SETVIEW;
    dlg->result_id = -2;
    dlg->result_mask = 0;
    dlg->result_label[0] = '\0';
  }
  cheat_list_close(dlg);
}

static void cheat_list_confirm(CheatListDialog* dlg, int idx) {
  if (!dlg || idx < 0 || idx >= dlg->option_count) {
    cheat_list_cancel(dlg);
    return;
  }
  /* FUN_647e_01c6 raw 102450-102456 (647e:04ab-04d3): Previous (first row past page 0) =
   * page - 1, More (last row before the last page) = page + 1; the box is rebuilt, stays up. */
  const int turn = !dlg->paged                                                  ? 0
                   : dlg->page > 0 && idx == 0                                  ? -1
                   : dlg->page < dlg->page_count - 1 && idx == dlg->option_count - 1 ? 1
                                                                                : 0;
  if (turn) {
    dlg->page += turn;
    cheat_list_dest_build_page(dlg);
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
  /* Every kind is a FUN_6f74_2580 list: Up / Down wrap and skip disabled rows. */
  if (popup_list_key_step(input->last_key, &dlg->selection, dlg->option_count, dlg->disabled_mask)) {
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

  /* FUN_6f74_2580 default case (6f74:2df6): the first row whose FUN_6f74_0642 hotkey (from
   * the raw label, past the "~[ " of a @checkbox row) is the key becomes the selection; a
   * @checkbox box toggles it and stays open (6f74:2dcd), any other box returns that row. */
  const int key = popup_input_dos_key(input);
  for (int i = 0; key && i < dlg->option_count; ++i) {
    if (popup_hotkey_of(dlg->options[i] + (dlg->multi_select ? 3 : 0)) == key) {
      dlg->selection = i;
      if (dlg->multi_select) {
        cheat_list_toggle_multi(dlg, i);
      } else {
        cheat_list_confirm(dlg, i);
      }
      return true;
    }
  }

  if (input->mouse_left_clicked) {
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    if (!ui_rect_hit(dlg->dialog_x, dlg->dialog_y, dlg->dialog_w, dlg->dialog_h, mx, my)) {
      cheat_list_cancel(dlg);
      return true;
    }
    const int idx = cheat_list_option_at_y(dlg, my);
    /* A disabled row takes no click and the box stays up (FUN_6f74_2580). */
    if (idx >= 0 && !(dlg->disabled_mask & (1u << idx))) {
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
  /*
   * Every kind is a generic compositor box. The DEBUG.TXT / GAME.TXT section lists keep
   * the FUN_6f74_06d0 box[+0x22] default 4; the FUN_647e_* destination / route / cargo
   * pickers set it to 10 (FUN_647e_01c6 raw 102383 Go to Port and the route stops,
   * FUN_647e_0796 raw 102708 route picker, FUN_647e_090a raw 102771 starting stop,
   * FUN_647e_0e80 raw 102958 cargo picker). The caller passes the font (FONTINTR; the
   * cargo picker FONTTINY, FUN_2a1f_07c4(DS:0x89e) raw 102951).
   */
  PopupListMetrics metrics;
  popup_list_metrics_dos6f74(font, &metrics);
  switch (dlg->kind) {
    case CHEAT_LIST_KIND_GOTO_PORT:
    case CHEAT_LIST_KIND_TRADE_SELECT:
    case CHEAT_LIST_KIND_TRADE_DEST:
    case CHEAT_LIST_KIND_TRADE_CARGO_ONE:
      metrics.label_dx = 10;
      break;
    default:
      break;
  }
  metrics.disabled_mask = dlg->disabled_mask;
  metrics.figure = dlg->mss >= 0 ? ai_popup_decoration_sheet(dlg->mss, -1) : NULL;
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
    dlg,
    text_color,
    COLONIZE_COL_HILITE,
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
