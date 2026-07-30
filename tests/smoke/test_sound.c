#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/sound.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  if (!sound_init("./COLONIZE", true)) {
    fprintf(stderr, "sound_init failed\n");
    return 1;
  }
  if (!sound_ok()) {
    fprintf(stderr, "GSOUND.COL not loaded\n");
    sound_shutdown();
    return 1;
  }
  if (sound_gsound_song_count() < 1) {
    fprintf(stderr, "expected decoded songs\n");
    sound_shutdown();
    return 1;
  }
  if (!sound_gsound_has_song(SOUND_TITLE_ID)) {
    fprintf(stderr, "missing title song 0x33\n");
    sound_shutdown();
    return 1;
  }
  if (!sound_gsound_has_song(SOUND_BGM_ID_BASE + 1)) {
    fprintf(stderr, "missing map BGM track 1 (id 0x21)\n");
    sound_shutdown();
    return 1;
  }

  enum { FRAMES = 44100 / 2 }; /* 0.5s */
  int16_t* buf = (int16_t*)calloc(FRAMES, sizeof(int16_t));
  if (!buf) {
    sound_shutdown();
    return 1;
  }

  if (COLONIZE_SOUND_PLAYBACK_ENABLED) {
    const int n = sound_render_offline_mono(SOUND_TITLE_ID, buf, FRAMES, 44100);
    if (n < FRAMES / 2) {
      fprintf(stderr, "offline render too short (%d)\n", n);
      free(buf);
      sound_shutdown();
      return 1;
    }
    double energy = 0.0;
    int nonzero = 0;
    for (int i = 0; i < n; ++i) {
      const double s = (double)buf[i];
      energy += s * s;
      if (buf[i] != 0) {
        nonzero++;
      }
    }
    if (nonzero < 100 || energy < 1.0) {
      fprintf(stderr, "rendered silence (nonzero=%d energy=%g)\n", nonzero, energy);
      free(buf);
      sound_shutdown();
      return 1;
    }
    fprintf(
      stderr,
      "sound tests ok songs=%d backend=%s nonzero=%d\n",
      sound_gsound_song_count(),
      sound_backend_ok() ? "fluidsynth" : "fallback",
      nonzero
    );
  } else {
    fprintf(
      stderr,
      "sound tests ok songs=%d playback=parked\n",
      sound_gsound_song_count()
    );
  }
  free(buf);

  /* Gating API still callable while parked (no-ops). */
  ColonizeSoundOptions opts = sound_get_options();
  opts.background_music = false;
  sound_set_options(opts);
  sound_play(SOUND_TITLE_ID);
  sound_set_bgm(1);
  sound_service();

  sound_shutdown();
  diag_shutdown();
  return 0;
}
