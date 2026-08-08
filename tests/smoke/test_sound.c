#include <math.h>
#include <stdint.h>
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

  /* Golden decode: Bird Song (0x21) first melody note is D5 (74) vel 44 prog 22. */
  int events = 0;
  uint32_t dur = 0;
  uint8_t note = 0, vel = 0, prog = 0, ch = 0;
  if (!sound_gsound_song_stats(SOUND_BGM_ID_BASE + 1, &events, &dur, &note, &vel, &prog, &ch)) {
    fprintf(stderr, "song 0x21 stats failed\n");
    sound_shutdown();
    return 1;
  }
  if (events < 100) {
    fprintf(stderr, "song 0x21 too few events (%d)\n", events);
    sound_shutdown();
    return 1;
  }
  /* ~60 Hz ticks; one pass of lead is ~67s → expect thousands of ticks. */
  if (dur < 2000 || dur > 20000) {
    fprintf(stderr, "song 0x21 duration out of range (%u ticks)\n", dur);
    sound_shutdown();
    return 1;
  }
  if (note != 74 || vel != 44 || prog != 22) {
    fprintf(
      stderr,
      "song 0x21 first note mismatch note=%u vel=%u prog=%u (want 74/44/22)\n",
      note,
      vel,
      prog
    );
    sound_shutdown();
    return 1;
  }

  /* ED chord song 0x28: some tick must carry ≥2 note-ons (desync fix). */
  {
    int chord_events = 0;
    uint32_t chord_dur = 0;
    if (!sound_gsound_song_stats(0x28, &chord_events, &chord_dur, NULL, NULL, NULL, NULL) ||
        chord_events < 200) {
      fprintf(stderr, "song 0x28 weak decode events=%d\n", chord_events);
      sound_shutdown();
      return 1;
    }
    int same_tick_notes = 0;
    uint32_t prev_tick = UINT32_MAX;
    int notes_at_tick = 0;
    for (int i = 0; i < chord_events; ++i) {
      uint32_t tick = 0;
      uint8_t status = 0, d1 = 0, d2 = 0, c = 0;
      if (!sound_gsound_event_at(0x28, i, &tick, &status, &d1, &d2, &c)) {
        break;
      }
      if ((status & 0xf0) != 0x90 || d2 == 0) {
        continue;
      }
      if (tick == prev_tick) {
        notes_at_tick++;
      } else {
        if (notes_at_tick >= 2) {
          same_tick_notes = notes_at_tick;
        }
        prev_tick = tick;
        notes_at_tick = 1;
      }
    }
    if (notes_at_tick >= 2) {
      same_tick_notes = notes_at_tick;
    }
    if (same_tick_notes < 2) {
      fprintf(stderr, "song 0x28 expected ED chord (≥2 note-ons on one tick)\n");
      sound_shutdown();
      return 1;
    }
  }

  /* Event music table 0x2AC4 — at least one id in 0x40..0x5c. */
  if (!sound_gsound_has_song(0x40) && !sound_gsound_has_song(0x43)) {
    fprintf(stderr, "missing event music (0x40/0x43)\n");
    sound_shutdown();
    return 1;
  }

  /* Title intro must decode to a substantial event list. */
  int title_events = 0;
  uint32_t title_dur = 0;
  if (!sound_gsound_song_stats(SOUND_TITLE_ID, &title_events, &title_dur, NULL, NULL, NULL, NULL) ||
      title_events < 50 || title_dur < 500) {
    fprintf(
      stderr,
      "title 0x33 weak decode events=%d dur=%u\n",
      title_events,
      title_dur
    );
    sound_shutdown();
    return 1;
  }

  enum { FRAMES = 44100 / 2 }; /* 0.5s */
  int16_t* buf = (int16_t*)calloc(FRAMES, sizeof(int16_t));
  if (!buf) {
    sound_shutdown();
    return 1;
  }

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
    "sound tests ok songs=%d backend=%s events21=%d dur21=%u nonzero=%d\n",
    sound_gsound_song_count(),
    sound_backend_ok() ? "fluidsynth" : "fallback",
    events,
    dur,
    nonzero
  );
  free(buf);

  ColonizeSoundOptions opts = sound_get_options();
  opts.background_music = false;
  sound_set_options(opts);
  sound_play(SOUND_TITLE_ID); /* gated off by options */
  sound_set_bgm(1);
  sound_play_preview(SOUND_BGM_ID_BASE + 1);
  sound_service();
  sound_stop_preview();

  sound_shutdown();
  diag_shutdown();
  return 0;
}
