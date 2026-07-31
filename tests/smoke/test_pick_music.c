#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/pick_music.h"
#include "core/popup.h"
#include "core/sound.h"
#include "core/ui_colors.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static int fail(const char* msg) {
  fprintf(stderr, "%s\n", msg);
  return 1;
}

int main(void) {
  diag_init(0, NULL);

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    return fail("Failed to load GAME.TXT");
  }

  PickMusicDialog dlg;
  if (!pick_music_open(&dlg, &game_txt)) {
    assets_msg_free(&game_txt);
    return fail("pick_music_open failed");
  }
  if (dlg.width != 220 || !dlg.smallfont) {
    fprintf(stderr, "expected width=220 smallfont, got %d %d\n", dlg.width, dlg.smallfont ? 1 : 0);
    pick_music_close(&dlg);
    assets_msg_free(&game_txt);
    return 1;
  }
  /* 12 songs + 3 submenu rows. */
  if (dlg.option_count != 15) {
    fprintf(stderr, "PICKMUSIC expected 15 options, got %d\n", dlg.option_count);
    pick_music_close(&dlg);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strcmp(dlg.options[0].label, "Bird Song") != 0 || dlg.options[0].song_id != 0x21) {
    fprintf(
      stderr,
      "first song expected Bird Song/0x21 got '%s'/0x%02x\n",
      dlg.options[0].label,
      dlg.options[0].song_id
    );
    pick_music_close(&dlg);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (dlg.options[12].kind != PICK_MUSIC_KIND_SUB_INDEPENDENCE) {
    return fail("expected Independence Tunes submenu row");
  }

  /* Activate Independence submenu. */
  dlg.selection = 12;
  ColonizeInputState enter;
  memset(&enter, 0, sizeof(enter));
  enter.last_key = COLONIZE_KEY_ENTER;
  char status[128];
  pick_music_handle_input(&dlg, &game_txt, &enter, NULL, status, sizeof(status));
  if (dlg.view != PICK_MUSIC_VIEW_INDEPENDENCE || dlg.option_count != 5) {
    fprintf(
      stderr,
      "independence view expected 5 songs, view=%d count=%d\n",
      (int)dlg.view,
      dlg.option_count
    );
    pick_music_close(&dlg);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strcmp(dlg.options[0].label, "Love Forever") != 0 || dlg.options[0].song_id != 0x2d) {
    fprintf(
      stderr,
      "independence first expected Love Forever/0x2d got '%s'/0x%02x\n",
      dlg.options[0].label,
      dlg.options[0].song_id
    );
    pick_music_close(&dlg);
    assets_msg_free(&game_txt);
    return 1;
  }

  /* Esc returns to main. */
  enter.last_key = COLONIZE_KEY_ESCAPE;
  pick_music_handle_input(&dlg, &game_txt, &enter, NULL, status, sizeof(status));
  if (dlg.view != PICK_MUSIC_VIEW_MAIN || !dlg.open) {
    return fail("Esc should return to main Pick Music list");
  }

  /* Selecting a song uses preview path (audible when audio device is open). */
  sound_init("COLONIZE", false);
  dlg.selection = 0;
  enter.last_key = COLONIZE_KEY_ENTER;
  pick_music_handle_input(&dlg, &game_txt, &enter, NULL, status, sizeof(status));
  if (strstr(status, "Bird Song") == NULL) {
    fprintf(stderr, "status should mention Bird Song, got '%s'\n", status);
    sound_shutdown();
    pick_music_close(&dlg);
    assets_msg_free(&game_txt);
    return 1;
  }

  /* Preview decode should produce non-silent audio even while autoplay is parked. */
  enum { FRAMES = 44100 / 4 };
  int16_t* buf = (int16_t*)calloc(FRAMES, sizeof(int16_t));
  if (!buf) {
    sound_shutdown();
    pick_music_close(&dlg);
    assets_msg_free(&game_txt);
    return 1;
  }
  const int n = sound_render_offline_mono(0x21, buf, FRAMES, 44100);
  int nonzero = 0;
  for (int i = 0; i < n; ++i) {
    if (buf[i] != 0) {
      nonzero++;
    }
  }
  free(buf);
  if (n < FRAMES / 2 || nonzero < 50) {
    fprintf(stderr, "preview offline render weak (n=%d nonzero=%d)\n", n, nonzero);
    sound_shutdown();
    pick_music_close(&dlg);
    assets_msg_free(&game_txt);
    return 1;
  }

  sound_stop_preview();
  sound_shutdown();

  /* Render draws popup chrome (outer black). */
  uint8_t pixels[320 * 200];
  memset(pixels, 0xAA, sizeof(pixels));
  ColonizeFramebuffer8 fb = {320, 200, pixels};
  ColonizePopupColors cols;
  popup_colors_from_ui(&cols);
  pick_music_render(&dlg, NULL, NULL, &cols, COLONIZE_COL_BASIC, COLONIZE_COL_SELECT, &fb);
  if (pixels[dlg.dialog_y * 320 + dlg.dialog_x] != 0) {
    fprintf(
      stderr,
      "popup outer border expected black at %d,%d got %u\n",
      dlg.dialog_x,
      dlg.dialog_y,
      pixels[dlg.dialog_y * 320 + dlg.dialog_x]
    );
    pick_music_close(&dlg);
    assets_msg_free(&game_txt);
    return 1;
  }

  pick_music_close(&dlg);
  assets_msg_free(&game_txt);
  return 0;
}
