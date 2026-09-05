#define _DEFAULT_SOURCE
/*
 * Offline GSOUND → WAV (and optional Type-0 SMF) dump.
 *
 * Usage:
 *   dump_gsound_wav [--data-dir DIR] [--out-dir DIR] [--seconds N] [--midi]
 *                   [--rename-only] [--ab] [--backend fluidsynth|tsf]
 *                   [--soundfont FILE.sf2] [song_id ...]
 *
 * Defaults: every decoded BGM song id, one full pass (+2s release tail) into
 * ./ripped_sound as stereo 44.1 kHz PCM WAV (same render path as gameplay).
 * --seconds N forces a fixed length instead of duration_ticks.
 * Filenames: 0xID_Title.wav (titles from GAME.TXT @PICKMUSIC* + known extras).
 * --rename-only renames existing song_XX.wav / 0xXX*.wav without re-rendering.
 * --ab dumps the four reference songs into build/music-ab as 0xID_port.wav
 *   at reference lengths (for tools/compare_music_ab.py).
 * --sfx writes every COLDIG.BIN digital sample as <out-dir>/sfx/sfxNN.wav.
 */
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/sound.h"
#include "platform/diagnostics.h"

/* Match GSOUND PIT tick rate used in sound.c (~59.95 Hz). */
#define DUMP_TICK_HZ (1193182.0 / 19903.0)
#define DUMP_TAIL_SECONDS 2
#define DUMP_MAX_SECONDS 300
#define DUMP_TITLE_LEN 64

/*
 * Same id ↔ label mapping as pick_music.c. Titles for ids outside Pick Music
 * (Introduction / national anthems / FOY / ending / Cibola) follow the DOS OST
 * naming used by community track lists; GAME.TXT overrides when present.
 */
typedef struct DumpSongTitle {
  int id;
  const char* title;
} DumpSongTitle;

static const DumpSongTitle k_default_titles[] = {
  /* DOS Pick Music handler (2b5a:264c jump table + sublist adds) — verified
   * against DOSBox captures of Jine the Cavalry / Hole In The Wall. */
  {0x20, "Bird Song"},
  {0x21, "Smoky Tune"},
  {0x22, "Cornwall"},
  {0x23, "Shady Grove"},
  {0x24, "Fiddler's Dance"},
  {0x25, "Jine the Cavalry"},
  {0x26, "Joe Clark"},
  {0x27, "Little Fiddle"},
  {0x28, "Unlisted 28"},
  {0x29, "Love Forever"},
  {0x2a, "York Fusiliers"},
  {0x2b, "Washington Artillery March"},
  {0x2c, "Road to Boston"},
  {0x2d, "Independence Way"},
  {0x2e, "The Reveille"},
  {0x2f, "Successful Campaign"},
  {0x30, "Morelli's Lesson"},
  {0x31, "To Arms"},
  {0x32, "Indian Victory"},
  {0x33, "Natives"},
  {0x34, "Unlisted 34"},
  {0x35, "Tenochtitlan"},
  {0x36, "Pizarro at Cuzco"},
  {0x37, "Unlisted 37"},
  {0x38, "Bonny Morn"},
  {0x39, "Hornpipe"},
  {0x3a, "Hole In The Wall"},
  {0x3b, "Nightingale"},
  {0x3c, "Unlisted 3c"},
  {0x3d, "Unlisted 3d"},
  {0x3e, "Unlisted 3e"},
  {0x3f, "Unlisted 3f"},
};

static char g_titles[0x40][DUMP_TITLE_LEN];

static void dump_titles_init_defaults(void) {
  memset(g_titles, 0, sizeof(g_titles));
  for (size_t i = 0; i < sizeof(k_default_titles) / sizeof(k_default_titles[0]); ++i) {
    const int id = k_default_titles[i].id;
    if (id >= 0 && id < 0x40) {
      snprintf(g_titles[id], DUMP_TITLE_LEN, "%s", k_default_titles[i].title);
    }
  }
}

static void dump_strip_quotes(char* text) {
  if (!text || text[0] != '"') {
    return;
  }
  size_t n = strlen(text);
  if (n >= 2 && text[n - 1] == '"') {
    memmove(text, text + 1, n - 2);
    text[n - 2] = '\0';
  }
}

static void dump_trim(char* text) {
  if (!text) {
    return;
  }
  char* s = text;
  while (*s && isspace((unsigned char)*s)) {
    ++s;
  }
  if (s != text) {
    memmove(text, s, strlen(s) + 1);
  }
  size_t n = strlen(text);
  while (n > 0 && isspace((unsigned char)text[n - 1])) {
    text[--n] = '\0';
  }
}

/* Load quoted song labels from a GAME.TXT @SECTION in pick_music order. */
static void dump_load_section_titles(
  FILE* f,
  const char* section,
  const int* ids,
  int id_count
) {
  char line[256];
  bool in_section = false;
  bool saw_prompt = false;
  int song_index = 0;

  rewind(f);
  while (fgets(line, sizeof(line), f)) {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n')) {
      line[--n] = '\0';
    }
    dump_trim(line);
    if (line[0] == '@') {
      if (in_section) {
        break;
      }
      if (strcmp(line + 1, section) == 0) {
        in_section = true;
        saw_prompt = false;
        song_index = 0;
      }
      continue;
    }
    if (!in_section || line[0] == '\0') {
      continue;
    }
    if (!saw_prompt) {
      saw_prompt = true;
      continue;
    }
    if (line[0] != '"') {
      continue; /* submenu labels, etc. */
    }
    dump_strip_quotes(line);
    dump_trim(line);
    if (song_index < id_count) {
      const int id = ids[song_index++];
      if (id >= 0 && id < 0x40 && line[0]) {
        strncpy(g_titles[id], line, DUMP_TITLE_LEN - 1);
        g_titles[id][DUMP_TITLE_LEN - 1] = '\0';
      }
    }
  }
}

static void dump_load_titles_from_game_txt(const char* data_dir) {
  static const int k_main[] = {
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c
  };
  static const int k_ind[] = {0x2d, 0x2e, 0x2f, 0x30, 0x31};
  static const int k_mil[] = {0x32, 0x34, 0x35, 0x36};
  static const int k_indian[] = {0x37, 0x38, 0x39, 0x3a};

  char path[512];
  snprintf(path, sizeof(path), "%s/GAME.TXT", data_dir);
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "warning: %s missing — using built-in song titles\n", path);
    return;
  }
  dump_load_section_titles(f, "PICKMUSIC", k_main, (int)(sizeof(k_main) / sizeof(k_main[0])));
  dump_load_section_titles(
    f, "PICKINDEPENDENCE", k_ind, (int)(sizeof(k_ind) / sizeof(k_ind[0]))
  );
  dump_load_section_titles(f, "PICKMILITARY", k_mil, (int)(sizeof(k_mil) / sizeof(k_mil[0])));
  dump_load_section_titles(
    f, "PICKINDIAN", k_indian, (int)(sizeof(k_indian) / sizeof(k_indian[0]))
  );
  fclose(f);
}

static const char* dump_song_title(int id) {
  if (id >= 0 && id < 0x40 && g_titles[id][0]) {
    return g_titles[id];
  }
  return NULL;
}

/* Build a filesystem-safe stem: 0x21_Bird_Song */
static void dump_song_stem(int id, char* out, size_t out_size) {
  const char* title = dump_song_title(id);
  char slug[DUMP_TITLE_LEN];
  slug[0] = '\0';
  if (title && title[0]) {
    size_t o = 0;
    for (const char* p = title; *p && o + 1 < sizeof(slug); ++p) {
      unsigned char c = (unsigned char)*p;
      if (isalnum(c)) {
        slug[o++] = (char)c;
      } else if (c == '\'' || c == '`') {
        continue;
      } else if (o > 0 && slug[o - 1] != '_') {
        slug[o++] = '_';
      }
    }
    while (o > 0 && slug[o - 1] == '_') {
      --o;
    }
    slug[o] = '\0';
  }
  if (slug[0]) {
    snprintf(out, out_size, "0x%02x_%s", id & 0xff, slug);
  } else {
    snprintf(out, out_size, "0x%02x", id & 0xff);
  }
}

static void wr_u16(FILE* f, uint16_t v) {
  fputc(v & 0xff, f);
  fputc((v >> 8) & 0xff, f);
}

static void wr_u32(FILE* f, uint32_t v) {
  wr_u16(f, (uint16_t)(v & 0xffffu));
  wr_u16(f, (uint16_t)((v >> 16) & 0xffffu));
}

static bool write_wav_s16_stereo(const char* path, const int16_t* interleaved, int frames, int rate) {
  FILE* f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  const uint32_t data_bytes = (uint32_t)frames * 4u;
  fwrite("RIFF", 1, 4, f);
  wr_u32(f, 36u + data_bytes);
  fwrite("WAVEfmt ", 1, 8, f);
  wr_u32(f, 16);
  wr_u16(f, 1); /* PCM */
  wr_u16(f, 2); /* stereo */
  wr_u32(f, (uint32_t)rate);
  wr_u32(f, (uint32_t)rate * 4u);
  wr_u16(f, 4);
  wr_u16(f, 16);
  fwrite("data", 1, 4, f);
  wr_u32(f, data_bytes);
  fwrite(interleaved, 2, (size_t)frames * 2u, f);
  fclose(f);
  return true;
}

static bool write_midi_type0(const char* path, int song_id) {
  int events = 0;
  uint32_t dur = 0;
  if (!sound_gsound_song_stats(song_id, &events, &dur, NULL, NULL, NULL, NULL) || events < 1) {
    return false;
  }

  size_t cap = (size_t)events * 16u + 64u;
  uint8_t* body = (uint8_t*)malloc(cap);
  if (!body) {
    return false;
  }
  size_t len = 0;
  uint32_t prev_tick = 0;
  uint8_t running = 0;

  for (int i = 0; i < events; ++i) {
    uint32_t tick = 0;
    uint8_t status = 0, d1 = 0, d2 = 0, ch = 0;
    if (!sound_gsound_event_at(song_id, i, &tick, &status, &d1, &d2, &ch)) {
      break;
    }
    uint32_t delta = tick - prev_tick;
    prev_tick = tick;

    uint8_t vlq[5];
    int vn = 0;
    {
      uint32_t v = delta;
      vlq[vn++] = (uint8_t)(v & 0x7fu);
      while (v >>= 7) {
        vlq[vn++] = (uint8_t)((v & 0x7fu) | 0x80u);
      }
    }
    if (len + (size_t)vn + 4u > cap) {
      cap *= 2u;
      uint8_t* nbody = (uint8_t*)realloc(body, cap);
      if (!nbody) {
        free(body);
        return false;
      }
      body = nbody;
    }
    for (int k = vn - 1; k >= 0; --k) {
      body[len++] = vlq[k];
    }

    const uint8_t st = (uint8_t)((status & 0xf0) | (ch & 0x0f));
    const bool one_data = ((status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0);
    if (st != running) {
      body[len++] = st;
      running = st;
    }
    body[len++] = d1;
    if (!one_data) {
      body[len++] = d2;
    }
  }

  if (len + 4 > cap) {
    uint8_t* nbody = (uint8_t*)realloc(body, len + 4);
    if (!nbody) {
      free(body);
      return false;
    }
    body = nbody;
  }
  body[len++] = 0x00;
  body[len++] = 0xff;
  body[len++] = 0x2f;
  body[len++] = 0x00;

  FILE* f = fopen(path, "wb");
  if (!f) {
    free(body);
    return false;
  }
  fwrite("MThd", 1, 4, f);
  wr_u32(f, 6);
  wr_u16(f, 0); /* type 0 */
  wr_u16(f, 1);
  wr_u16(f, 60); /* ticks per quarter ≈ dump tick Hz for rough DAW view */
  fwrite("MTrk", 1, 4, f);
  wr_u32(f, (uint32_t)len);
  fwrite(body, 1, len, f);
  fclose(f);
  free(body);
  return true;
}

static void dump_events_csv(int song_id) {
  int events = 0;
  if (!sound_gsound_song_stats(song_id, &events, NULL, NULL, NULL, NULL, NULL)) {
    return;
  }
  for (int i = 0; i < events; ++i) {
    uint32_t tick = 0;
    uint8_t status = 0, d1 = 0, d2 = 0, ch = 0;
    if (!sound_gsound_event_at(song_id, i, &tick, &status, &d1, &d2, &ch)) {
      break;
    }
    fprintf(
      stdout, "%.4f,0x%02x,%u,%u,%u\n", (double)tick / DUMP_TICK_HZ, status, d1, d2, ch
    );
  }
}

static double rms_s16_stereo(const int16_t* interleaved, int frames) {
  long double acc = 0.0;
  const int n = frames * 2;
  for (int i = 0; i < n; ++i) {
    const long double s = (long double)interleaved[i];
    acc += s * s;
  }
  if (n <= 0) {
    return 0.0;
  }
  return (double)sqrtl(acc / (long double)n);
}

static int16_t peak_s16_stereo(const int16_t* interleaved, int frames) {
  int16_t peak = 0;
  const int n = frames * 2;
  for (int i = 0; i < n; ++i) {
    int16_t a = interleaved[i];
    if (a < 0) {
      a = (int16_t)(-a);
    }
    if (a > peak) {
      peak = a;
    }
  }
  return peak;
}

static int song_seconds(uint32_t duration_ticks, int forced_seconds) {
  if (forced_seconds > 0) {
    return forced_seconds;
  }
  int sec = (int)((double)duration_ticks / DUMP_TICK_HZ) + DUMP_TAIL_SECONDS;
  if (sec < 1) {
    sec = 1;
  }
  if (sec > DUMP_MAX_SECONDS) {
    sec = DUMP_MAX_SECONDS;
  }
  return sec;
}

static const struct {
  int id;
  int seconds;
  const char* ref_wav;
} k_ab_songs[] = {
  {0x20, 46, "reference_music/wav/02 - Bird Song.wav"},
  {0x25, 71, "reference_music/wav/07 - Jine the Cavalry.wav"},
  {0x3a, 72, "reference_music/wav/12 - Hole in the Wall.wav"},
  {0x32, 48, "reference_music/wav/20 - Indian Victory.wav"},
};

int main(int argc, char** argv) {
  const char* data_dir = "./COLONIZE";
  const char* out_dir = "./ripped_sound";
  int forced_seconds = 0;
  bool want_midi = false;
  bool want_csv = false;
  bool rename_only = false;
  bool ab_mode = false;
  bool sfx_mode = false;
  int songs[64];
  int song_count = 0;
  int ab_seconds[64];

  for (int i = 0; i < 64; ++i) {
    ab_seconds[i] = 0;
  }

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--data-dir") == 0 && i + 1 < argc) {
      data_dir = argv[++i];
    } else if (strcmp(argv[i], "--out-dir") == 0 && i + 1 < argc) {
      out_dir = argv[++i];
    } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      forced_seconds = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--midi") == 0) {
      want_midi = true;
    } else if (strcmp(argv[i], "--rename-only") == 0) {
      rename_only = true;
    } else if (strcmp(argv[i], "--ab") == 0) {
      ab_mode = true;
    } else if (strcmp(argv[i], "--csv") == 0) {
      want_csv = true;
    } else if (strcmp(argv[i], "--sfx") == 0) {
      sfx_mode = true;
    } else if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
      sound_set_midi_backend(argv[++i]); /* fluidsynth | tsf */
    } else if (strcmp(argv[i], "--soundfont") == 0 && i + 1 < argc) {
      sound_set_soundfont(argv[++i]);
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "unknown flag %s\n", argv[i]);
      return 1;
    } else {
      if (song_count < (int)(sizeof(songs) / sizeof(songs[0]))) {
        songs[song_count++] = (int)strtol(argv[i], NULL, 0);
      }
    }
  }

  if (ab_mode) {
    out_dir = "./build/music-ab";
    song_count = 0;
    for (size_t i = 0; i < sizeof(k_ab_songs) / sizeof(k_ab_songs[0]); ++i) {
      songs[song_count] = k_ab_songs[i].id;
      ab_seconds[song_count] = k_ab_songs[i].seconds;
      song_count++;
    }
    mkdir("build", 0755);
    mkdir(out_dir, 0755);
    for (size_t i = 0; i < sizeof(k_ab_songs) / sizeof(k_ab_songs[0]); ++i) {
      char ref_link[512];
      char abs_ref[1024];
      snprintf(ref_link, sizeof(ref_link), "%s/0x%02x_ref.wav", out_dir, k_ab_songs[i].id & 0xff);
      unlink(ref_link);
      if (realpath(k_ab_songs[i].ref_wav, abs_ref)) {
        if (symlink(abs_ref, ref_link) != 0) {
          fprintf(stderr, "warning: symlink %s failed\n", ref_link);
        }
      } else {
        fprintf(stderr, "warning: missing reference %s\n", k_ab_songs[i].ref_wav);
      }
    }
  }

  if (forced_seconds < 0) {
    forced_seconds = 0;
  }
  if (forced_seconds > DUMP_MAX_SECONDS) {
    forced_seconds = DUMP_MAX_SECONDS;
  }

  dump_titles_init_defaults();
  dump_load_titles_from_game_txt(data_dir);

  if (rename_only) {
    mkdir(out_dir, 0755);
    if (song_count == 0) {
      for (int id = SOUND_BGM_ID_BASE; id < SOUND_EVENT_ID_BASE; ++id) {
        if (song_count < (int)(sizeof(songs) / sizeof(songs[0]))) {
          songs[song_count++] = id;
        }
      }
    }
    int renamed = 0;
    for (int s = 0; s < song_count; ++s) {
      const int id = songs[s];
      char stem[160];
      char dest[512];
      dump_song_stem(id, stem, sizeof(stem));
      snprintf(dest, sizeof(dest), "%s/%s.wav", out_dir, stem);

      char candidates[3][512];
      snprintf(candidates[0], sizeof(candidates[0]), "%s/song_%02x.wav", out_dir, id & 0xff);
      snprintf(candidates[1], sizeof(candidates[1]), "%s/0x%02x.wav", out_dir, id & 0xff);
      snprintf(candidates[2], sizeof(candidates[2]), "%s", dest);

      const char* src = NULL;
      for (int c = 0; c < 3; ++c) {
        struct stat st;
        if (stat(candidates[c], &st) == 0 && S_ISREG(st.st_mode)) {
          src = candidates[c];
          break;
        }
      }
      if (!src) {
        continue;
      }
      if (strcmp(src, dest) == 0) {
        fprintf(stderr, "ok %s\n", dest);
        renamed++;
        continue;
      }
      if (rename(src, dest) != 0) {
        fprintf(stderr, "rename failed %s -> %s\n", src, dest);
        continue;
      }
      fprintf(stderr, "renamed %s -> %s\n", src, dest);
      renamed++;

      char mid_src[512];
      char mid_dst[512];
      snprintf(mid_src, sizeof(mid_src), "%s/song_%02x.mid", out_dir, id & 0xff);
      snprintf(mid_dst, sizeof(mid_dst), "%s/%s.mid", out_dir, stem);
      struct stat st;
      if (stat(mid_src, &st) == 0 && S_ISREG(st.st_mode)) {
        rename(mid_src, mid_dst);
      }
    }
    fprintf(stderr, "renamed %d files in %s\n", renamed, out_dir);
    return renamed > 0 ? 0 : 1;
  }

  diag_init(0, NULL);
  if (!sound_init(data_dir, true) || !sound_ok()) {
    fprintf(stderr, "sound_init / GSOUND failed (data_dir=%s)\n", data_dir);
    return 1;
  }

  if (sfx_mode) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/sfx", out_dir);
    mkdir(out_dir, 0755);
    mkdir(dir, 0755);
    const int n = sound_sfx_count();
    int written = 0;
    for (int i = 0; i < n; ++i) {
      const uint8_t* pcm = NULL;
      uint32_t len = 0;
      int rate = 0;
      if (!sound_sfx_sample(i, &pcm, &len, &rate)) {
        continue;
      }
      int16_t* buf = (int16_t*)malloc((size_t)len * 2u * sizeof(int16_t));
      if (!buf) {
        break;
      }
      for (uint32_t k = 0; k < len; ++k) {
        const int16_t v = (int16_t)(((int)pcm[k] - 128) * 256);
        buf[k * 2] = v;
        buf[k * 2 + 1] = v;
      }
      char path[600];
      snprintf(path, sizeof(path), "%s/sfx%02d.wav", dir, i);
      if (write_wav_s16_stereo(path, buf, (int)len, rate)) {
        fprintf(stderr, "sfx %2d len=%u rate=%d %.2fs -> %s\n", i, len, rate, (double)len / rate, path);
        written++;
      }
      free(buf);
    }
    fprintf(stderr, "wrote %d/%d samples\n", written, n);
    sound_shutdown();
    diag_shutdown();
    return written > 0 ? 0 : 1;
  }

  if (song_count == 0) {
    for (int id = SOUND_BGM_ID_BASE; id < SOUND_EVENT_ID_BASE; ++id) {
      if (sound_gsound_has_song(id) &&
          song_count < (int)(sizeof(songs) / sizeof(songs[0]))) {
        songs[song_count++] = id;
      }
    }
  }
  if (song_count == 0) {
    fprintf(stderr, "no songs to dump\n");
    sound_shutdown();
    return 1;
  }

  if (want_csv) {
    for (int s = 0; s < song_count; ++s) {
      dump_events_csv(songs[s]);
    }
    sound_shutdown();
    diag_shutdown();
    return 0;
  }

  mkdir(out_dir, 0755);

  const int rate = 44100;
  int max_frames = 0;
  for (int s = 0; s < song_count; ++s) {
    uint32_t dur = 0;
    int sec = forced_seconds;
    if (ab_mode && ab_seconds[s] > 0) {
      sec = ab_seconds[s];
    }
    if (sec <= 0 && sound_gsound_song_stats(songs[s], NULL, &dur, NULL, NULL, NULL, NULL)) {
      sec = song_seconds(dur, 0);
    }
    if (sec < 1) {
      sec = 1;
    }
    const int frames = rate * sec;
    if (frames > max_frames) {
      max_frames = frames;
    }
  }
  if (max_frames < rate) {
    max_frames = rate;
  }

  int16_t* buf = (int16_t*)calloc((size_t)max_frames * 2u, sizeof(int16_t));
  if (!buf) {
    sound_shutdown();
    return 1;
  }

  int ok_count = 0;
  for (int s = 0; s < song_count; ++s) {
    const int id = songs[s];
    int events = 0;
    uint32_t dur = 0;
    uint8_t note = 0, vel = 0, prog = 0, ch = 0;
    if (!sound_gsound_has_song(id) ||
        !sound_gsound_song_stats(id, &events, &dur, &note, &vel, &prog, &ch)) {
      fprintf(stderr, "song 0x%02x missing\n", id);
      continue;
    }

    int seconds = forced_seconds;
    if (ab_mode && ab_seconds[s] > 0) {
      seconds = ab_seconds[s];
    }
    if (seconds <= 0) {
      seconds = song_seconds(dur, 0);
    }
    const int frames = rate * seconds;
    memset(buf, 0, (size_t)frames * 2u * sizeof(int16_t));
    sound_play_preview(id);
    const int chunk = 512;
    int written = 0;
    while (written + chunk <= frames) {
      sound_render_s16(buf + written * 2, chunk, 2, rate);
      written += chunk;
    }
    sound_stop_preview();

    char stem[160];
    char wav_path[512];
    if (ab_mode) {
      snprintf(stem, sizeof(stem), "0x%02x_port", id & 0xff);
    } else {
      dump_song_stem(id, stem, sizeof(stem));
    }
    snprintf(wav_path, sizeof(wav_path), "%s/%s.wav", out_dir, stem);
    if (!write_wav_s16_stereo(wav_path, buf, written, rate)) {
      fprintf(stderr, "failed writing %s\n", wav_path);
      continue;
    }

    const double rms = rms_s16_stereo(buf, written);
    const int peak = (int)peak_s16_stereo(buf, written);
    const char* title = dump_song_title(id);
    fprintf(
      stderr,
      "0x%02x \"%s\" events=%d dur_ticks=%u sec=%d first=note%u vel%u prog%u ch%u  "
      "wav=%s rms=%.1f peak=%d backend=%s\n",
      id,
      title ? title : "?",
      events,
      dur,
      seconds,
      note,
      vel,
      prog,
      ch,
      wav_path,
      rms,
      peak,
      sound_backend_name()
    );
    ok_count++;

    if (want_midi) {
      char mid_path[512];
      snprintf(mid_path, sizeof(mid_path), "%s/%s.mid", out_dir, stem);
      if (write_midi_type0(mid_path, id)) {
        fprintf(stderr, "  midi=%s\n", mid_path);
      } else {
        fprintf(stderr, "  midi write failed\n");
      }
    }
  }

  free(buf);
  sound_shutdown();
  diag_shutdown();
  fprintf(stderr, "wrote %d/%d songs to %s\n", ok_count, song_count, out_dir);
  return ok_count > 0 ? 0 : 1;
}
