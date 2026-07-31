#ifndef COLONIZE_PICK_MUSIC_H
#define COLONIZE_PICK_MUSIC_H

#include <stdbool.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * GAME → Pick Music dialog (GAME.TXT @PICKMUSIC + Independence / Military / Indian).
 * Wood popup chrome over the map; Esc backs out of a submenu or closes.
 */
#define PICK_MUSIC_MAX_OPTIONS 20
#define PICK_MUSIC_LABEL_LEN 48

typedef enum PickMusicKind {
  PICK_MUSIC_KIND_SONG = 0,
  PICK_MUSIC_KIND_SUB_INDEPENDENCE,
  PICK_MUSIC_KIND_SUB_MILITARY,
  PICK_MUSIC_KIND_SUB_INDIAN
} PickMusicKind;

typedef enum PickMusicView {
  PICK_MUSIC_VIEW_MAIN = 0,
  PICK_MUSIC_VIEW_INDEPENDENCE,
  PICK_MUSIC_VIEW_MILITARY,
  PICK_MUSIC_VIEW_INDIAN
} PickMusicView;

typedef struct PickMusicOption {
  char label[PICK_MUSIC_LABEL_LEN];
  PickMusicKind kind;
  int song_id; /* BGM id when kind == SONG; else unused */
} PickMusicOption;

typedef struct PickMusicDialog {
  bool open;
  PickMusicView view;
  int selection;
  int width;
  bool smallfont;
  char prompt[COLONIZE_MSG_LINE_LEN];
  PickMusicOption options[PICK_MUSIC_MAX_OPTIONS];
  int option_count;
  /* Last computed layout (for hit-testing). */
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int inner_x;
  int inner_y;
  int inner_w;
  int list_y0;
  int line_h;
} PickMusicDialog;

void pick_music_init(PickMusicDialog* dlg);
void pick_music_close(PickMusicDialog* dlg);

/* Load @PICKMUSIC (main list). Returns false if GAME.TXT section missing. */
bool pick_music_open(PickMusicDialog* dlg, const ColonizeMsgCatalog* game_txt);

/* Keyboard / mouse. Returns true if the event was consumed. */
bool pick_music_handle_input(
  PickMusicDialog* dlg,
  const ColonizeMsgCatalog* game_txt,
  const ColonizeInputState* input,
  const ColonizeFont* font,
  char* status,
  size_t status_size
);

void pick_music_render(
  PickMusicDialog* dlg,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
);

#endif
