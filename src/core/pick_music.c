#include "core/pick_music.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/sound.h"
#include "core/strutil.h"
#include "core/ui_colors.h"

/*
 * Song id table (GAME.TXT order within each section).
 *
 * Main list = BGM tracks 1..12 → ids 0x21..0x2c (same mapping as sound_set_bgm).
 * Menu order matches the OST listing after Introduction (Bird Song … Nightingale).
 *
 * Submenu ids continue through the remaining BGM slots, skipping Introduction (0x33):
 *   Independence 0x2d..0x31, Military 0x32/0x34..0x36, Indian 0x37..0x3a.
 * Nation anthems / FOY / ending occupy other ids and are not in Pick Music.
 */
static const int k_main_song_ids[] = {
  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c
};
static const int k_independence_song_ids[] = {0x2d, 0x2e, 0x2f, 0x30, 0x31};
static const int k_military_song_ids[] = {0x32, 0x34, 0x35, 0x36};
static const int k_indian_song_ids[] = {0x37, 0x38, 0x39, 0x3a};

void pick_music_init(PickMusicDialog* dlg) {
  if (!dlg) {
    return;
  }
  memset(dlg, 0, sizeof(*dlg));
  dlg->width = 220;
  dlg->smallfont = true;
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
      song_ids = k_main_song_ids;
      song_id_count = (int)(sizeof(k_main_song_ids) / sizeof(k_main_song_ids[0]));
      break;
    case PICK_MUSIC_VIEW_INDEPENDENCE:
      song_ids = k_independence_song_ids;
      song_id_count = (int)(sizeof(k_independence_song_ids) / sizeof(k_independence_song_ids[0]));
      break;
    case PICK_MUSIC_VIEW_MILITARY:
      song_ids = k_military_song_ids;
      song_id_count = (int)(sizeof(k_military_song_ids) / sizeof(k_military_song_ids[0]));
      break;
    case PICK_MUSIC_VIEW_INDIAN:
      song_ids = k_indian_song_ids;
      song_id_count = (int)(sizeof(k_indian_song_ids) / sizeof(k_indian_song_ids[0]));
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
    if (mx < dlg->dialog_x || my < dlg->dialog_y || mx >= dlg->dialog_x + dlg->dialog_w ||
        my >= dlg->dialog_y + dlg->dialog_h) {
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
  dlg->inner_x = inner_x;
  dlg->inner_y = inner_y;
  dlg->inner_w = inner_w;
  dlg->line_h = line_h;

  int text_y = inner_y + pad_y;
  if (dlg->prompt[0]) {
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
    font_draw_text(
      font, framebuffer, inner_x + pad_x, row_y, dlg->options[i].label, text_color
    );
  }
}
