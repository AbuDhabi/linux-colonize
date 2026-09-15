#include "core/pick_music.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/sound.h"
#include "core/strutil.h"
#include "core/ui_colors.h"

#include "core/pick_music_ids.h"
#include "core/popup.h"
#include "core/ui_button.h"

void pick_music_init(PickMusicDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
  dlg->width = 220;
  dlg->smallfont = true;
}

const ColonizeFont* pick_music_font(
  const PickMusicDialog* dlg, const ColonizeFont* tiny_font, const ColonizeFont* dialog_font
) {
  if (dlg && dlg->smallfont && tiny_font) {
    return tiny_font;
  }
  return dialog_font ? dialog_font : tiny_font;
}

void pick_music_close(PickMusicDialog* dlg) {
  if (!dlg) {
    return;
  }
  dlg->open = false;
  dlg->view = PICK_MUSIC_VIEW_MAIN;
  dlg->selection = 0;
  dlg->option_count = 0;
  dlg->prompt[0] = '\0';
}

static void pick_music_strip_quotes(char* text) {
  if (!text || text[0] != '"') {
    return;
  }
  size_t n = strlen(text);
  if (n >= 2 && text[n - 1] == '"') {
    memmove(text, text + 1, n - 2);
    text[n - 2] = '\0';
  }
}

static bool pick_music_is_directive(const char* line) {
  return line && line[0] == '@';
}

static bool pick_music_load_section(
  PickMusicDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  const char* section_name,
  PickMusicView view
) {
  if (!dlg || !game_txt) {
    return false;
  }
  const ColonizeMsgSection* section = assets_msg_find(game_txt, section_name);
  if (!section) {
    return false;
  }

  dlg->view = view;
  dlg->selection = 0;
  dlg->option_count = 0;
  dlg->prompt[0] = '\0';
  dlg->width = 220;
  dlg->smallfont = false;

  const int* song_ids = NULL;
  int song_id_count = 0;
  switch (view) {
    case PICK_MUSIC_VIEW_MAIN:
      song_ids = k_pick_music_main_song_ids;
      song_id_count = PICK_MUSIC_IDS_COUNT(k_pick_music_main_song_ids);
      break;
    case PICK_MUSIC_VIEW_INDEPENDENCE:
      song_ids = k_pick_music_independence_song_ids;
      song_id_count = PICK_MUSIC_IDS_COUNT(k_pick_music_independence_song_ids);
      break;
    case PICK_MUSIC_VIEW_MILITARY:
      song_ids = k_pick_music_military_song_ids;
      song_id_count = PICK_MUSIC_IDS_COUNT(k_pick_music_military_song_ids);
      break;
    case PICK_MUSIC_VIEW_INDIAN:
      song_ids = k_pick_music_indian_song_ids;
      song_id_count = PICK_MUSIC_IDS_COUNT(k_pick_music_indian_song_ids);
      break;
  }

  int song_index = 0;
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0') {
      continue;
    }
    if (pick_music_is_directive(line)) {
      if (strncmp(line, "@width=", 7) == 0) {
        dlg->width = atoi(line + 7);
        if (dlg->width < 80) {
          dlg->width = 80;
        }
        if (dlg->width > 320) {
          dlg->width = 320;
        }
      } else if (strcmp(line, "@smallfont") == 0) {
        dlg->smallfont = true;
      }
      continue;
    }

    /* First non-directive line is the prompt (may end with ':'). */
    if (dlg->prompt[0] == '\0') {
      str_copy_trunc(dlg->prompt, sizeof(dlg->prompt), line);
      continue;
    }

    if (dlg->option_count >= PICK_MUSIC_MAX_OPTIONS) {
      break;
    }

    PickMusicOption* opt = &dlg->options[dlg->option_count];
    memset(opt, 0, sizeof(*opt));
    str_copy_trunc(opt->label, sizeof(opt->label), line);
    pick_music_strip_quotes(opt->label);

    if (view == PICK_MUSIC_VIEW_MAIN) {
      if (strcmp(opt->label, "Independence Tunes") == 0) {
        opt->kind = PICK_MUSIC_KIND_SUB_INDEPENDENCE;
      } else if (strcmp(opt->label, "Military Tunes") == 0) {
        opt->kind = PICK_MUSIC_KIND_SUB_MILITARY;
      } else if (strcmp(opt->label, "Indian Tunes") == 0) {
        opt->kind = PICK_MUSIC_KIND_SUB_INDIAN;
      } else {
        opt->kind = PICK_MUSIC_KIND_SONG;
        if (song_index < song_id_count) {
          opt->song_id = song_ids[song_index++];
        }
      }
    } else {
      opt->kind = PICK_MUSIC_KIND_SONG;
      if (song_index < song_id_count) {
        opt->song_id = song_ids[song_index++];
      }
    }
    dlg->option_count++;
  }

  dlg->open = dlg->option_count > 0;
  return dlg->open;
}

bool pick_music_open(PickMusicDialog* dlg, const ColonizeMsgCatalog* game_txt) {
  pick_music_init(dlg);
  return pick_music_load_section(dlg, game_txt, "PICKMUSIC", PICK_MUSIC_VIEW_MAIN);
}

static void pick_music_play_song(const PickMusicOption* opt, char* status, size_t status_size) {
  if (!opt || opt->kind != PICK_MUSIC_KIND_SONG || opt->song_id <= 0) {
    if (status && status_size > 0) {
      snprintf(status, status_size, "No song mapped for %s", opt ? opt->label : "?");
    }
    return;
  }
  /* Preview path: plays immediately without changing map BGM. */
  sound_play_preview(opt->song_id);
  if (status && status_size > 0) {
    if (sound_audio_output_ready()) {
      snprintf(status, status_size, "Playing: %s (0x%02x)", opt->label, opt->song_id);
    } else {
      snprintf(
        status,
        status_size,
        "Selected: %s (0x%02x; no audio device — try without --nosound)",
        opt->label,
        opt->song_id
      );
    }
  }
}

static bool pick_music_activate(
  PickMusicDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  char* status,
  size_t status_size
) {
  if (!dlg || dlg->selection < 0 || dlg->selection >= dlg->option_count) {
    return true;
  }
  const PickMusicOption* opt = &dlg->options[dlg->selection];
  switch (opt->kind) {
    case PICK_MUSIC_KIND_SONG:
      pick_music_play_song(opt, status, status_size);
      return true;
    case PICK_MUSIC_KIND_SUB_INDEPENDENCE:
      pick_music_load_section(dlg, game_txt, "PICKINDEPENDENCE", PICK_MUSIC_VIEW_INDEPENDENCE);
      return true;
    case PICK_MUSIC_KIND_SUB_MILITARY:
      pick_music_load_section(dlg, game_txt, "PICKMILITARY", PICK_MUSIC_VIEW_MILITARY);
      return true;
    case PICK_MUSIC_KIND_SUB_INDIAN:
      pick_music_load_section(dlg, game_txt, "PICKINDIAN", PICK_MUSIC_VIEW_INDIAN);
      return true;
  }
  return true;
}

static int pick_music_option_at_y(const PickMusicDialog* dlg, int mouse_y) {
  return dlg ? popup_row_at_y(dlg->list_y0, dlg->line_h, dlg->option_count, mouse_y) : -1;
}

bool pick_music_handle_input(
  PickMusicDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  const ColonizeInputState* input,
  const ColonizeFont* font,
  char* status,
  size_t status_size
) {
  (void)font;
  if (!dlg || !dlg->open || !input) {
    return false;
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    if (dlg->view != PICK_MUSIC_VIEW_MAIN) {
      pick_music_load_section(dlg, game_txt, "PICKMUSIC", PICK_MUSIC_VIEW_MAIN);
    } else {
      sound_stop_preview();
      pick_music_close(dlg);
    }
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
  if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
    pick_music_activate(dlg, game_txt, status, status_size);
    return true;
  }

  if (input->mouse_left_clicked) {
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    if (!ui_rect_hit(dlg->dialog_x, dlg->dialog_y, dlg->dialog_w, dlg->dialog_h, mx, my)) {
      sound_stop_preview();
      pick_music_close(dlg);
      return true;
    }
    const int idx = pick_music_option_at_y(dlg, my);
    if (idx >= 0) {
      dlg->selection = idx;
      pick_music_activate(dlg, game_txt, status, status_size);
    }
    return true;
  }

  return true; /* consume all input while open */
}

static const char* pick_music_row_label(void* user, int index) {
  const PickMusicDialog* dlg = (const PickMusicDialog*)user;
  return dlg->options[index].label;
}

void pick_music_render(
  PickMusicDialog* dlg,
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
    pick_music_row_label,
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
  dlg->inner_x = geom.frame.inner_x;
  dlg->inner_y = geom.frame.inner_y;
  dlg->inner_w = geom.frame.inner_w;
  dlg->line_h = geom.line_h;
  dlg->list_y0 = geom.list_y0;
}
