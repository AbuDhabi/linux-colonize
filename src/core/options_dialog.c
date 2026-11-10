#include "core/options_dialog.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/mods.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/strutil.h"
#include "core/ui_button.h"
#include "platform/diagnostics.h"

void options_dialog_init(OptionsDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
  dlg->width = 190;
}

void options_dialog_close(OptionsDialog* dlg) {
  if (!dlg) {
    return;
  }
  dlg->open = false;
  dlg->kind = OPTIONS_KIND_NONE;
}


static bool options_load_section(
  OptionsDialog* dlg,
  OptionsDialogKind kind,
  const ColonizeMsgCatalog* game_txt,
  const char* section_name,
  const uint8_t* initial_values,
  int value_count,
  int width
) {
  if (!dlg) {
    return false;
  }
  options_dialog_init(dlg);
  dlg->has_result = false;
  dlg->kind = kind;
  dlg->width = width;
  dlg->option_count = 0;

  const ColonizeMsgSection* sec =
    game_txt ? assets_msg_find(game_txt, section_name) : NULL;
  int in_options = 0;
  if (sec) {
    for (int i = 0; i < sec->line_count; ++i) {
      const char* line = sec->lines[i];
      if (!line || line[0] == '\0') {
        continue;
      }
      if (popup_msg_is_directive(line)) {
        if (strncmp(line, "@width=", 7) == 0) {
          int w = 0;
          if (sscanf(line + 7, "%d", &w) == 1 && w >= 80 && w <= 320) {
            dlg->width = w;
          }
        } else if (strcmp(line, "@options") == 0) {
          in_options = 1;
        }
        continue;
      }
      if (!in_options) {
        if (dlg->prompt[0] == '\0' && strcmp(line, "@checkbox") != 0) {
          str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), line);
        }
        continue;
      }
      if (dlg->option_count >= OPTIONS_DIALOG_MAX) {
        break;
      }
      /* Raw label: the writer draws '~' hotkey letters and {} spans in the hilite ink. */
      str_copy_trunc(dlg->labels[dlg->option_count], sizeof(dlg->labels[0]), line);
      dlg->values[dlg->option_count] =
        (initial_values && dlg->option_count < value_count) ? initial_values[dlg->option_count]
                                                              : 0;
      dlg->option_count++;
    }
  }

  /* Catalog miss: value_count blank rows (no typed wording), so the toggles still work. */
  if (dlg->option_count <= 0) {
    dlg->prompt[0] = '\0';
    for (int i = 0; i < value_count && i < OPTIONS_DIALOG_MAX; ++i) {
      dlg->values[i] = initial_values ? initial_values[i] : 0;
      dlg->option_count++;
    }
  }
  if (dlg->option_count <= 0) {
    options_dialog_close(dlg);
    return false;
  }
  dlg->open = true;
  return true;
}

bool options_dialog_open_game(
  OptionsDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  const ColonizeCol1GameOptions* opts
) {
  uint8_t vals[8] = {0};
  if (opts) {
    vals[0] = opts->show_indian_moves ? 1 : 0;
    vals[1] = opts->show_foreign_moves ? 1 : 0;
    vals[2] = opts->fast_piece_slide ? 1 : 0;
    vals[3] = opts->end_of_turn ? 1 : 0;
    vals[4] = opts->autosave ? 1 : 0;
    vals[5] = opts->combat_analysis ? 1 : 0;
    /* DOS stores this checkbox as a disable bit: clear means cycling on. */
    vals[6] = opts->water_color_cycling_off ? 0 : 1;
    vals[7] = opts->tutorial_hints ? 1 : 0;
  }
  return options_load_section(
    dlg,
    OPTIONS_KIND_GAME,
    game_txt,
    "GAMEOPTIONS",
    vals,
    8,
    190
  );
}

bool options_dialog_open_colony(
  OptionsDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  const ColonizeCol1ColonyReportOptions* opts
) {
  /*
   * Every DS:0x5384/0x5385 bit is a *suppress* flag: FUN_2b5a_223a sets it
   * when the checkbox is clear, and the EOT reporters fire on `bit == 0`
   * (turn_report_ok_*). A fresh save is all-zero, i.e. every report on.
   */
  uint8_t vals[10] = {0};
  if (opts) {
    vals[0] = opts->labels_on_buildings ? 0 : 1;
    vals[1] = opts->labels_on_cargo_and_terrain ? 0 : 1;
    vals[2] = opts->report_when_colonists_trained ? 0 : 1;
    vals[3] = opts->report_food_shortages ? 0 : 1;
    vals[4] = opts->report_raw_materials_shortages ? 0 : 1;
    vals[5] = opts->report_tools_needed_for_production ? 0 : 1;
    vals[6] = opts->report_inefficient_government ? 0 : 1;
    vals[7] = opts->report_new_cargos_available ? 0 : 1;
    vals[8] = opts->report_sons_of_liberty_membership ? 0 : 1;
    vals[9] = opts->report_rebel_majorities ? 0 : 1;
  }
  return options_load_section(
    dlg,
    OPTIONS_KIND_COLONY,
    game_txt,
    "COLONYOPTIONS",
    vals,
    10,
    220
  );
}

bool options_dialog_open_sound(
  OptionsDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  bool background_music,
  bool event_music,
  bool sound_effects
) {
  uint8_t vals[3] = {
    background_music ? 1 : 0, event_music ? 1 : 0, sound_effects ? 1 : 0
  };
  return options_load_section(
    dlg,
    OPTIONS_KIND_SOUND,
    game_txt,
    "SOUNDOPTIONS",
    vals,
    3,
    190
  );
}

/*
 * One data/mods @MODOPTIONS* section per page, grouped by kind of change; the
 * section's rows follow its id list here. With FONTINTR (glyph_h 9) a row
 * advances 12 px and a prompt of up to two 10 px lines, so
 * popup_dos_list_height is 35 + 12 * rows: 13 rows is the most that fits the
 * 200 px screen (all 17 would need 239), and the OK button takes the last two
 * (OPTIONS_MODS_OK_ROWS). Every ModId sits on exactly one page (checked by the
 * popup dialog tests).
 */
#define OPTIONS_MODS_OK_ROWS 2
#define OPTIONS_MODS_PAGE_ROWS_MAX (13 - OPTIONS_MODS_OK_ROWS)
static const ModId k_mods_restored[] = {
  MOD_KINGS_AUDIENCE, MOD_ROYAL_MERCY, MOD_TOWN_HALL_TIERS, MOD_TRIBE_MOOD,
  MOD_TREATY_CANCEL_POPUP, MOD_WAGON_LOOT,      MOD_RAID_SCALPS,     MOD_SCOUT_SPYING,
};
static const ModId k_mods_rules[] = {
  MOD_POPULATION_FLOOR,  MOD_LATE_FATHERS,     MOD_BUILDING_UPKEEP, MOD_HARD_KING,
  MOD_PIONEER_PLAN,      MOD_SCOUT_AUTOEXPLORE, MOD_SHIP_AUTOEXPLORE, MOD_CURRENT_VISION,
  MOD_CONSTRUCTION_QUEUE,
};
static const ModId k_mods_fixes[] = {MOD_GOTO_NATIVE_WAIT};
#define MODS_PAGE_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
static const struct {
  const char* section;
  const ModId* ids;
  int count;
} k_mods_pages[OPTIONS_MODS_PAGE_COUNT] = {
  {"MODOPTIONS", k_mods_restored, MODS_PAGE_LEN(k_mods_restored)},
  {"MODOPTIONS2", k_mods_rules, MODS_PAGE_LEN(k_mods_rules)},
  {"MODOPTIONS3", k_mods_fixes, MODS_PAGE_LEN(k_mods_fixes)},
};
_Static_assert(OPTIONS_MODS_PAGE_ROWS_MAX <= OPTIONS_DIALOG_MAX, "a mods page fits the dialog");
_Static_assert(MODS_PAGE_LEN(k_mods_restored) <= OPTIONS_MODS_PAGE_ROWS_MAX, "mods page 1 fits 200 px");
_Static_assert(MODS_PAGE_LEN(k_mods_rules) <= OPTIONS_MODS_PAGE_ROWS_MAX, "mods page 2 fits 200 px");
_Static_assert(MODS_PAGE_LEN(k_mods_fixes) <= OPTIONS_MODS_PAGE_ROWS_MAX, "mods page 3 fits 200 px");
_Static_assert(
  MODS_PAGE_LEN(k_mods_restored) + MODS_PAGE_LEN(k_mods_rules) + MODS_PAGE_LEN(k_mods_fixes) ==
    MOD_COUNT,
  "the mods pages hold MOD_COUNT rows"
);

int options_dialog_mods_page_id(int page, int row) {
  if (page < 0 || page >= OPTIONS_MODS_PAGE_COUNT || row < 0 || row >= k_mods_pages[page].count) {
    return -1;
  }
  return (int)k_mods_pages[page].ids[row];
}

bool options_dialog_open_mods(
  OptionsDialog* dlg, const ColonizeMsgCatalog* game_txt, uint32_t mask, int page
) {
  if (page < 0 || page >= OPTIONS_MODS_PAGE_COUNT) {
    return false;
  }
  const ModId* ids = k_mods_pages[page].ids;
  const int count = k_mods_pages[page].count;
  uint8_t vals[OPTIONS_DIALOG_MAX];
  for (int i = 0; i < count; ++i) {
    vals[i] = (uint8_t)(mask >> ids[i] & 1u);
  }
  if (!options_load_section(
        dlg, OPTIONS_KIND_MODS, game_txt, k_mods_pages[page].section, vals, count, 220
      )) {
    return false;
  }
  dlg->mods_page = page;
  for (int i = 0; i < count; ++i) {
    dlg->mods_ids[i] = (uint8_t)ids[i];
  }
  if (dlg->option_count != count) {
    diag_warn("@%s has %d rows, expected %d (options_dialog.c k_mods_pages)",
              k_mods_pages[page].section, dlg->option_count, count);
  }
  /* Rows past the page (a newer data file) would toggle nothing. */
  if (dlg->option_count > count) {
    dlg->option_count = count;
  }
  str_copy_trunc(
    dlg->ok_label, sizeof(dlg->ok_label),
    assets_msg_line_or(game_txt, "MODOPTIONSDONE", page + 1 < OPTIONS_MODS_PAGE_COUNT ? 0 : 1, "")
  );
  return true;
}

bool options_dialog_apply_mods(const OptionsDialog* dlg, uint32_t* mask) {
  if (!dlg || !mask || dlg->result_cancelled || dlg->result_kind != OPTIONS_KIND_MODS) {
    return false;
  }
  uint32_t m = *mask;
  if (dlg->mods_page < 0 || dlg->mods_page >= OPTIONS_MODS_PAGE_COUNT) {
    return false;
  }
  const int count = k_mods_pages[dlg->mods_page].count;
  for (int i = 0; i < dlg->result_count && i < count; ++i) {
    const uint32_t bit = 1u << dlg->mods_ids[i];
    m = dlg->result_values[i] ? (m | bit) : (m & ~bit);
  }
  *mask = m;
  return true;
}

void options_dialog_apply_game(const OptionsDialog* dlg, ColonizeCol1GameOptions* opts) {
  if (!dlg || !opts || dlg->result_cancelled || dlg->result_kind != OPTIONS_KIND_GAME) {
    return;
  }
  if (dlg->result_count < 8) {
    return;
  }
  opts->show_indian_moves = dlg->result_values[0] ? 1 : 0;
  opts->show_foreign_moves = dlg->result_values[1] ? 1 : 0;
  opts->fast_piece_slide = dlg->result_values[2] ? 1 : 0;
  opts->end_of_turn = dlg->result_values[3] ? 1 : 0;
  opts->autosave = dlg->result_values[4] ? 1 : 0;
  opts->combat_analysis = dlg->result_values[5] ? 1 : 0;
  /* Preserve DOS polarity in the Col1 bitfield. */
  opts->water_color_cycling_off = dlg->result_values[6] ? 0 : 1;
  opts->tutorial_hints = dlg->result_values[7] ? 1 : 0;
}

void options_dialog_apply_colony(
  const OptionsDialog* dlg,
  ColonizeCol1ColonyReportOptions* opts
) {
  if (!dlg || !opts || dlg->result_cancelled || dlg->result_kind != OPTIONS_KIND_COLONY) {
    return;
  }
  if (dlg->result_count < 10) {
    return;
  }
  /* Inverted on the way back out too — see options_dialog_open_colony. */
  opts->labels_on_buildings = dlg->result_values[0] ? 0 : 1;
  opts->labels_on_cargo_and_terrain = dlg->result_values[1] ? 0 : 1;
  opts->report_when_colonists_trained = dlg->result_values[2] ? 0 : 1;
  opts->report_food_shortages = dlg->result_values[3] ? 0 : 1;
  opts->report_raw_materials_shortages = dlg->result_values[4] ? 0 : 1;
  opts->report_tools_needed_for_production = dlg->result_values[5] ? 0 : 1;
  opts->report_inefficient_government = dlg->result_values[6] ? 0 : 1;
  opts->report_new_cargos_available = dlg->result_values[7] ? 0 : 1;
  opts->report_sons_of_liberty_membership = dlg->result_values[8] ? 0 : 1;
  opts->report_rebel_majorities = dlg->result_values[9] ? 0 : 1;
}

bool options_dialog_apply_sound(
  const OptionsDialog* dlg,
  bool* background_music,
  bool* event_music,
  bool* sound_effects
) {
  if (!dlg || dlg->result_cancelled || dlg->result_kind != OPTIONS_KIND_SOUND ||
      dlg->result_count < 3) {
    return false;
  }
  if (background_music) {
    *background_music = dlg->result_values[0] != 0;
  }
  if (event_music) {
    *event_music = dlg->result_values[1] != 0;
  }
  if (sound_effects) {
    *sound_effects = dlg->result_values[2] != 0;
  }
  return true;
}

static void options_finish(OptionsDialog* dlg, bool cancelled) {
  if (!dlg) {
    return;
  }
  dlg->has_result = true;
  /* port_bugs.md #1474: FUN_6f74_2580 rebuilds DS:0x1f54 from the live checkboxes
   * on every exit, Esc included (LAB_6f74_3018), and
   * FUN_2b5a_20f6 / 223a / 23ce (raw 42934-43090) read every row from it whatever
   * the dialog answered. Only the port-only Mods pages keep a real cancel. */
  dlg->result_cancelled = cancelled && dlg->kind == OPTIONS_KIND_MODS;
  dlg->result_kind = dlg->kind;
  dlg->result_count = dlg->option_count;
  memcpy(dlg->result_values, dlg->values, sizeof(dlg->result_values));
  options_dialog_close(dlg);
}

static int options_option_at_y(const OptionsDialog* dlg, int mouse_y) {
  return dlg ? popup_row_at_y(dlg->list_y0, dlg->line_h, dlg->option_count, mouse_y) : -1;
}

bool options_dialog_handle_input(OptionsDialog* dlg, const ColonizeInputState* input) {
  if (!dlg || !dlg->open || !input) {
    return false;
  }
  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    options_finish(dlg, true);
    return true;
  }
  /* The FUN_281f_03fe @CHECKBOX lists show through FUN_6f74_2580: Up / Down wrap. */
  if (popup_list_key_step(input->last_key, &dlg->selection, dlg->option_count, 0)) {
    return true;
  }
  if (input->last_key == COLONIZE_KEY_SPACE) {
    if (dlg->selection >= 0 && dlg->selection < dlg->option_count) {
      dlg->values[dlg->selection] = dlg->values[dlg->selection] ? 0 : 1;
    }
    return true;
  }
  if (input->last_key == COLONIZE_KEY_ENTER) {
    options_finish(dlg, false);
    return true;
  }
  /* FUN_6f74_2580 default case (6f74:2df6): the first row whose FUN_6f74_0642 hotkey is
   * the key becomes the selection; a @checkbox box toggles it and stays open (6f74:2dcd). */
  const int key = popup_input_dos_key(input);
  for (int i = 0; key && i < dlg->option_count; ++i) {
    if (popup_hotkey_of(dlg->labels[i]) == key) {
      dlg->selection = i;
      dlg->values[i] = dlg->values[i] ? 0 : 1;
      return true;
    }
  }
  /* FUN_6f74_2580 treats both buttons alike. FUN_1acb_0056 keeps DS:0x7f6 set on the
   * release poll, so the held arm (raw 116705-116758) runs on the press, the hold and the
   * release alike: the bar follows the row under the pointer, leaving the box clears it,
   * and a pointer on the box but off every row sets bVar4. The release (raw 117036-117060,
   * #1724) then toggles the barred row of a @checkbox box (flag bit 2), or with the bar
   * cleared (flag bit 0, |= 5 raw 117354) leaves and commits like Enter (Esc commits too,
   * except on the Mods pages: options_finish); bVar4 skips it. So press A, drag to B flips
   * B, and a release on the title does nothing. */
  if (input->mouse_left_clicked || input->mouse_right_clicked) {
    dlg->press_seen = true;
  }
  const bool released = input->mouse_left_released || input->mouse_right_released;
  if (!dlg->press_seen ||
      !(released || input->mouse_left_down || input->mouse_right_down ||
        input->mouse_left_clicked || input->mouse_right_clicked)) {
    return true;
  }
  bool off_row = false; /* bVar4 */
  bool on_ok = false;
  if (!ui_rect_hit(
        dlg->dialog_x, dlg->dialog_y, dlg->dialog_w, dlg->dialog_h, input->mouse_x,
        input->mouse_y
      )) {
    dlg->selection = -1;
  } else {
    on_ok = dlg->ok_label[0] &&
            ui_rect_hit(dlg->ok_x, dlg->ok_y, dlg->ok_w, dlg->ok_h, input->mouse_x, input->mouse_y);
    const int idx = options_option_at_y(dlg, input->mouse_y);
    if (idx >= 0) {
      dlg->selection = idx;
    } else {
      off_row = true;
    }
  }
  if (released) {
    dlg->press_seen = false;
    if (on_ok || (!off_row && (dlg->selection < 0 || dlg->selection >= dlg->option_count))) {
      options_finish(dlg, false);
    } else if (!off_row) {
      dlg->values[dlg->selection] = dlg->values[dlg->selection] ? 0 : 1;
    }
  }
  return true;
}

/*
 * DOS row text: FUN_6f74_0a00 prefixes a @checkbox row with DS:0x1f91 "~[ " and
 * FUN_6f74_1b7c sets its second byte to ']' when the row is on, '[' when off; the
 * font's '[' / ']' glyphs are the empty / filled check circles, drawn in the hilite
 * ink by the '~' like the label's own hotkey letter.
 */
static const char* options_row_label(void* user, int index) {
  static char buf[OPTIONS_DIALOG_LABEL_LEN + 3];
  const OptionsDialog* dlg = (const OptionsDialog*)user;
  if (index >= dlg->option_count) {
    return NULL; /* the rows kept free for the OK button */
  }
  snprintf(buf, sizeof(buf), "~%c %s", dlg->values[index] ? ']' : '[', dlg->labels[index]);
  return buf;
}

void options_dialog_render(
  OptionsDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
) {
  if (!dlg || !dlg->open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  /* @GAMEOPTIONS / @COLONYOPTIONS / @SOUNDOPTIONS: the generic parser box (FUN_281f_03fe ->
   * FUN_281f_0998), default FONTINTR, box[+0x22] 4, rows marked up. */
  PopupListMetrics metrics;
  popup_list_metrics_dos6f74(font, &metrics);
  PopupListGeom geom;
  const int ok_rows = dlg->ok_label[0] ? OPTIONS_MODS_OK_ROWS : 0;
  popup_list_render(
    framebuffer,
    font,
    wood_tile,
    colors,
    &metrics,
    dlg->width,
    dlg->prompt,
    dlg->option_count + ok_rows,
    dlg->selection,
    options_row_label,
    dlg,
    text_color,
    hilite_color,
    select_color,
    &geom
  );
  dlg->dialog_x = geom.frame.x;
  dlg->dialog_y = geom.frame.y;
  dlg->dialog_w = geom.frame.w;
  dlg->dialog_h = geom.frame.h;
  dlg->line_h = geom.line_h;
  dlg->list_y0 = geom.list_y0;
  if (ok_rows) {
    /* Centred in the free rows: a small copy of the dialog's own frame (wood,
     * three outline layers), the label drawn like a row (its edge shades). */
    const int tw = popup_markup_text_width(font, dlg->ok_label);
    const int th = popup_dos_glyph_h(font);
    dlg->ok_w = tw + 2 * POPUP_FRAME_INSET + 16;
    dlg->ok_h = th + 2 * POPUP_FRAME_INSET + 4;
    dlg->ok_x = geom.frame.x + (geom.frame.w - dlg->ok_w) / 2;
    dlg->ok_y = geom.list_y0 + dlg->option_count * geom.line_h +
                (ok_rows * geom.line_h - dlg->ok_h) / 2;
    int ix = 0, iy = 0, iw = 0, ih = 0;
    popup_draw(
      framebuffer, dlg->ok_x, dlg->ok_y, dlg->ok_w, dlg->ok_h, wood_tile, colors, &ix, &iy, &iw,
      &ih
    );
    (void)popup_draw_text_markup(
      font, framebuffer, ix + (iw - tw) / 2, iy + (ih - th) / 2, dlg->ok_label, text_color,
      hilite_color, colors, NULL
    );
  }
}
