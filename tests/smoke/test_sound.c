#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/sound.h"
#include "core/sound_opl.h"
#include "platform/diagnostics.h"

static int test_driver_g(void) {
  sound_configure_driver(SOUND_DRIVER_G);
  if (!sound_init("./COLONIZE", true)) {
    fprintf(stderr, "sound_init(G) failed\n");
    return 1;
  }
  if (!sound_ok()) {
    fprintf(stderr, "GSOUND.COL not loaded\n");
    sound_shutdown();
    return 1;
  }
  if (sound_current_driver() != SOUND_DRIVER_G) {
    fprintf(stderr, "expected driver G\n");
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

  enum { FRAMES = 44100 / 2 };
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
    fprintf(stderr, "G rendered silence (nonzero=%d energy=%g)\n", nonzero, energy);
    free(buf);
    sound_shutdown();
    return 1;
  }
  fprintf(
    stderr,
    "G ok songs=%d backend=%s events21=%d dur21=%u nonzero=%d\n",
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
  sound_play(SOUND_TITLE_ID);
  sound_set_bgm(1);
  sound_play_preview(SOUND_BGM_ID_BASE + 1);
  sound_service();
  sound_stop_preview();

  sound_shutdown();
  return 0;
}

static int test_driver_a(void) {
  sound_configure_driver(SOUND_DRIVER_A);
  if (!sound_init("./COLONIZE", true)) {
    fprintf(stderr, "sound_init(A) failed\n");
    return 1;
  }
  if (!sound_ok()) {
    fprintf(stderr, "ASOUND.COL not loaded\n");
    sound_shutdown();
    return 1;
  }
  if (sound_current_driver() != SOUND_DRIVER_A) {
    fprintf(stderr, "expected driver A\n");
    sound_shutdown();
    return 1;
  }
  if (!sound_backend_ok()) {
    fprintf(stderr, "OPL backend not ready\n");
    sound_shutdown();
    return 1;
  }
  if (!sound_opl_bank_loaded()) {
    fprintf(stderr, "ASOUND instrument bank not loaded\n");
    sound_shutdown();
    return 1;
  }

  /* Prog 6 packed regs from DS:0x5376 bank (Bird Song lead). */
  uint8_t m_char = 0, c_char = 0, c0 = 0, m_tl = 0, c_tl = 0;
  if (!sound_opl_program_regs(6, &m_char, &c_char, &c0, &m_tl, &c_tl)) {
    fprintf(stderr, "prog 6 regs failed\n");
    sound_shutdown();
    return 1;
  }
  if (m_char != 0x31 || c_char != 0x31 || c0 != 0x09) {
    fprintf(
      stderr,
      "prog 6 pack mismatch m20=%02x c20=%02x c0=%02x (want 31/31/09)\n",
      m_char,
      c_char,
      c0
    );
    sound_shutdown();
    return 1;
  }
  if (sound_gsound_song_count() < 16) {
    fprintf(stderr, "ASOUND expected many BGM songs, got %d\n", sound_gsound_song_count());
    sound_shutdown();
    return 1;
  }
  if (!sound_gsound_has_song(SOUND_TITLE_ID) || !sound_gsound_has_song(SOUND_BGM_ID_BASE + 1)) {
    fprintf(stderr, "ASOUND missing title or 0x21\n");
    sound_shutdown();
    return 1;
  }

  /* ASOUND Bird Song: first note 55 vel 87 prog 6 (parallel to GSOUND arrangement). */
  int events = 0;
  uint32_t dur = 0;
  uint8_t note = 0, vel = 0, prog = 0, ch = 0;
  if (!sound_gsound_song_stats(SOUND_BGM_ID_BASE + 1, &events, &dur, &note, &vel, &prog, &ch)) {
    fprintf(stderr, "ASOUND song 0x21 stats failed\n");
    sound_shutdown();
    return 1;
  }
  if (events < 100 || dur < 2000) {
    fprintf(stderr, "ASOUND 0x21 weak events=%d dur=%u\n", events, dur);
    sound_shutdown();
    return 1;
  }
  if (note != 55 || vel != 87 || prog != 6) {
    fprintf(
      stderr,
      "ASOUND 0x21 first note mismatch note=%u vel=%u prog=%u (want 55/87/6)\n",
      note,
      vel,
      prog
    );
    sound_shutdown();
    return 1;
  }

  enum { FRAMES = 44100 / 2 };
  int16_t* buf = (int16_t*)calloc(FRAMES, sizeof(int16_t));
  if (!buf) {
    sound_shutdown();
    return 1;
  }
  const int n = sound_render_offline_mono(SOUND_TITLE_ID, buf, FRAMES, 44100);
  double energy = 0.0;
  int nonzero = 0;
  for (int i = 0; i < n; ++i) {
    const double s = (double)buf[i];
    energy += s * s;
    if (buf[i] != 0) {
      nonzero++;
    }
  }
  if (n < FRAMES / 2 || nonzero < 100 || energy < 1.0) {
    fprintf(stderr, "A rendered silence n=%d nonzero=%d energy=%g\n", n, nonzero, energy);
    free(buf);
    sound_shutdown();
    return 1;
  }
  fprintf(
    stderr,
    "A ok songs=%d events21=%d dur21=%u note=%u prog=%u nonzero=%d\n",
    sound_gsound_song_count(),
    events,
    dur,
    note,
    prog,
    nonzero
  );
  free(buf);
  sound_shutdown();
  return 0;
}

int main(void) {
  diag_init(0, NULL);

  if (test_driver_g() != 0) {
    diag_shutdown();
    return 1;
  }
  if (test_driver_a() != 0) {
    diag_shutdown();
    return 1;
  }

  fprintf(stderr, "sound tests ok (G + A)\n");
  diag_shutdown();
  return 0;
}
