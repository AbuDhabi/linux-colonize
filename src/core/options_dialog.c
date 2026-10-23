#include "core/options_dialog.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/strutil.h"
#include "core/ui_button.h"

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
  const char* fallback_prompt,
  const char* const* fallback_labels,
  const uint8_t* initial_values,
  int fallback_count,
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
      char lab[OPTIONS_DIALOG_LABEL_LEN];
      str_copy_trunc(lab, sizeof(lab), line);
      str_strip_chars(lab, "~");
      /* {} kept — render colors braced spans with the hilite ink. */
      str_copy_trunc(dlg->labels[dlg->option_count], sizeof(dlg->labels[0]), lab);
      dlg->values[dlg->option_count] =
        (initial_values && dlg->option_count < fallback_count) ? initial_values[dlg->option_count]
                                                              : 0;
      dlg->option_count++;
    }
  }

  if (dlg->option_count <= 0 && fallback_labels && fallback_count > 0) {
    str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), fallback_prompt);
    for (int i = 0; i < fallback_count && i < OPTIONS_DIALOG_MAX; ++i) {
      str_copy_trunc(dlg->labels[i], sizeof(dlg->labels[i]), fallback_labels[i]);
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
  static const char* const k_fb[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    ""
  };
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
    "",
    k_fb,
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
  static const char* const k_fb[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    ""
  };
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
    "",
    k_fb,
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
  static const char* const k_fb[] = {
    "", "", ""
  };
  uint8_t vals[3] = {
    background_music ? 1 : 0, event_music ? 1 : 0, sound_effects ? 1 : 0
  };
  return options_load_section(
    dlg,
    OPTIONS_KIND_SOUND,
    game_txt,
    "SOUNDOPTIONS",
    "",
    k_fb,
    vals,
    3,
    190
  );
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
  dlg->result_cancelled = cancelled;
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
  if (colonize_key_up(input->last_key) && dlg->selection > 0) {
    dlg->selection--;
    return true;
  }
  if (colonize_key_down(input->last_key) && dlg->selection + 1 < dlg->option_count) {
    dlg->selection++;
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
  if (input->mouse_left_clicked) {
    if (!ui_rect_hit(
          dlg->dialog_x, dlg->dialog_y, dlg->dialog_w, dlg->dialog_h, input->mouse_x,
          input->mouse_y
        )) {
      /* port_bugs.md: clicking away should commit, like Enter — only Esc/right-
       * click truly discard. */
      options_finish(dlg, false);
      return true;
    }
    const int idx = options_option_at_y(dlg, input->mouse_y);
    if (idx >= 0) {
      dlg->selection = idx;
      dlg->values[idx] = dlg->values[idx] ? 0 : 1;
    }
    return true;
  }
  if (input->mouse_right_clicked) {
    options_finish(dlg, true);
    return true;
  }
  return true;
}

/*
 * DOS row text: FUN_6f74_0a00 prefixes a @checkbox row with DS:0x1f91 "~[ " and
 * FUN_6f74_1b7c sets its second byte to ']' when the row is on, '[' when off; the
 * font's '[' / ']' glyphs are the empty / filled check circles. The '~' hotkey
 * marker is stripped like the labels' own.
 */
static const char* options_row_label(void* user, int index) {
  static char buf[OPTIONS_DIALOG_LABEL_LEN + 2];
  const OptionsDialog* dlg = (const OptionsDialog*)user;
  snprintf(buf, sizeof(buf), "%c %s", dlg->values[index] ? ']' : '[', dlg->labels[index]);
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
}
