#include "core/options_dialog.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/popup_msg.h"
#include "core/strutil.h"

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

static void options_strip_tilde(char* s) {
  if (!s) {
    return;
  }
  char* w = s;
  for (char* r = s; *r; ++r) {
    if (*r == '~') {
      continue;
    }
    *w++ = *r;
  }
  *w = '\0';
}

static void options_strip_braces(char* s) {
  if (!s) {
    return;
  }
  char* w = s;
  for (char* r = s; *r; ++r) {
    if (*r == '{' || *r == '}') {
      continue;
    }
    *w++ = *r;
  }
  *w = '\0';
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
      options_strip_tilde(lab);
      options_strip_braces(lab);
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
    "Show Indian Moves",
    "Show Foreign Moves",
    "Fast Piece Slide",
    "End of Turn",
    "Autosave",
    "Combat Analysis",
    "Water Color Cycling",
    "Tutorial Hints"
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
    vals[6] = opts->water_color_cycling ? 0 : 1;
    vals[7] = opts->tutorial_hints ? 1 : 0;
  }
  return options_load_section(
    dlg,
    OPTIONS_KIND_GAME,
    game_txt,
    "GAMEOPTIONS",
    "Set Game Options",
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
    "Labels on buildings",
    "Labels on cargo and terrain",
    "Report when colonists trained",
    "Report food shortages",
    "Report raw materials shortages",
    "Report tools needed for production",
    "Report inefficient government",
    "Report new cargos available",
    "Report Sons of Liberty membership",
    "Report rebel majorities"
  };
  uint8_t vals[10] = {0};
  if (opts) {
    vals[0] = opts->labels_on_buildings ? 1 : 0;
    vals[1] = opts->labels_on_cargo_and_terrain ? 1 : 0;
    vals[2] = opts->report_when_colonists_trained ? 1 : 0;
    vals[3] = opts->report_food_shortages ? 1 : 0;
    vals[4] = opts->report_raw_materials_shortages ? 1 : 0;
    vals[5] = opts->report_tools_needed_for_production ? 1 : 0;
    vals[6] = opts->report_inefficient_government ? 1 : 0;
    vals[7] = opts->report_new_cargos_available ? 1 : 0;
    vals[8] = opts->report_sons_of_liberty_membership ? 1 : 0;
    vals[9] = opts->report_rebel_majorities ? 1 : 0;
  }
  return options_load_section(
    dlg,
    OPTIONS_KIND_COLONY,
    game_txt,
    "COLONYOPTIONS",
    "Set Colony Report Options",
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
    "Background Music", "Event Music", "Sound Effects"
  };
  uint8_t vals[3] = {
    background_music ? 1 : 0, event_music ? 1 : 0, sound_effects ? 1 : 0
  };
  return options_load_section(
    dlg,
    OPTIONS_KIND_SOUND,
    game_txt,
    "SOUNDOPTIONS",
    "Set Sound Options",
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
  opts->water_color_cycling = dlg->result_values[6] ? 0 : 1;
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
  opts->labels_on_buildings = dlg->result_values[0] ? 1 : 0;
  opts->labels_on_cargo_and_terrain = dlg->result_values[1] ? 1 : 0;
  opts->report_when_colonists_trained = dlg->result_values[2] ? 1 : 0;
  opts->report_food_shortages = dlg->result_values[3] ? 1 : 0;
  opts->report_raw_materials_shortages = dlg->result_values[4] ? 1 : 0;
  opts->report_tools_needed_for_production = dlg->result_values[5] ? 1 : 0;
  opts->report_inefficient_government = dlg->result_values[6] ? 1 : 0;
  opts->report_new_cargos_available = dlg->result_values[7] ? 1 : 0;
  opts->report_sons_of_liberty_membership = dlg->result_values[8] ? 1 : 0;
  opts->report_rebel_majorities = dlg->result_values[9] ? 1 : 0;
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
  if (!dlg || dlg->line_h <= 0) {
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

static void options_draw_checkbox(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  bool checked,
  uint8_t color
) {
  if (!framebuffer || !framebuffer->pixels) {
    return;
  }
  for (int py = 0; py < 7; ++py) {
    for (int px = 0; px < 7; ++px) {
      const bool edge = px == 0 || px == 6 || py == 0 || py == 6;
      if (edge || checked) {
        const int dx = x + px;
        const int dy = y + py;
        if (dx >= 0 && dy >= 0 && dx < framebuffer->width && dy < framebuffer->height) {
          framebuffer->pixels[dy * framebuffer->width + dx] = color;
        }
      }
    }
  }
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
    if (input->mouse_x < dlg->dialog_x || input->mouse_y < dlg->dialog_y ||
        input->mouse_x >= dlg->dialog_x + dlg->dialog_w ||
        input->mouse_y >= dlg->dialog_y + dlg->dialog_h) {
      /* bugs.md: clicking away should commit, like Enter — only Esc/right-
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

void options_dialog_render(
  OptionsDialog* dlg,
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

  ColonizePopupColors local;
  if (!colors) {
    popup_colors_from_ui(&local);
    colors = &local;
  }
  int ix = 0, iy = 0, iw = 0, ih = 0;
  popup_draw(
    framebuffer, dialog_x, dialog_y, dialog_w, dialog_h, wood_tile, colors, &ix, &iy, &iw, &ih
  );
  (void)ih;

  dlg->dialog_x = dialog_x;
  dlg->dialog_y = dialog_y;
  dlg->dialog_w = dialog_w;
  dlg->dialog_h = dialog_h;
  dlg->line_h = line_h;

  int text_y = iy + pad_y;
  if (dlg->prompt[0] && font) {
    popup_draw_text_shadowed(font, framebuffer, ix + pad_x, text_y, dlg->prompt, text_color);
    text_y += prompt_h;
  }
  dlg->list_y0 = text_y;

  for (int i = 0; i < dlg->option_count; ++i) {
    const int row_y = text_y + i * line_h;
    if (i == dlg->selection) {
      for (int y = row_y - 1; y <= row_y + line_h - 2; ++y) {
        for (int x = ix + 1; x <= ix + iw - 2; ++x) {
          if (x >= 0 && y >= 0 && x < framebuffer->width && y < framebuffer->height) {
            framebuffer->pixels[y * framebuffer->width + x] = select_color;
          }
        }
      }
    }
    options_draw_checkbox(
      framebuffer,
      ix + pad_x,
      row_y + (line_h > 7 ? (line_h - 7) / 2 : 0),
      dlg->values[i] != 0,
      text_color
    );
    if (font) {
      popup_draw_text_shadowed(font, framebuffer, ix + pad_x + 10, row_y, dlg->labels[i], text_color);
    }
  }
}
