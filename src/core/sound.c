#include "core/sound.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"
#include "platform/platform.h"

#if defined(COLONIZE_HAS_FLUIDSYNTH)
#include <fluidsynth.h>
#endif

#define SOUND_MAX_TRACKS 12
#define SOUND_MAX_EVENTS 4096
#define SOUND_GSOUND_DS_PARAS 0x0322
#define SOUND_GSOUND_BGM_TABLE 0x2A6E
#define SOUND_GSOUND_BGM_BOUND 0x331A
#define SOUND_GSOUND_IMG_HDR 512
/* Seconds per F4 duration unit (driver time base). ~4× prior 15 ms guess. */
#define SOUND_TICK_SECONDS (0.015 / 4.0)

typedef struct SoundMidiEvent {
  uint32_t tick;
  uint8_t status; /* 0x90 note on, 0x80 note off, 0xC0 program */
  uint8_t data1;
  uint8_t data2;
  uint8_t channel;
} SoundMidiEvent;

typedef struct SoundSong {
  int id;
  int event_count;
  SoundMidiEvent events[SOUND_MAX_EVENTS];
  uint32_t duration_ticks;
} SoundSong;

typedef struct SoundState {
  bool inited;
  bool enable_audio;
  bool gsound_ok;
  bool backend_ok;
  ColonizeSoundOptions opts;

  uint8_t* gsound_img;
  size_t gsound_img_size;
  uint32_t ds_base;

  SoundSong songs[32]; /* ids 0x20..0x3f */
  int song_count;

  pthread_mutex_t lock;
  int active_song_id; /* -1 = none */
  int bgm_track;      /* requested DOS track number; 0 = none */
  int bgm_song_id;
  bool need_restart;
  bool preview_active; /* Pick Music / A-B listen; bypasses autoplay park */
  uint32_t play_tick;
  double tick_accum; /* samples → ticks */
  double ticks_per_sample;
  int program;

#if defined(COLONIZE_HAS_FLUIDSYNTH)
  fluid_settings_t* fluid_settings;
  fluid_synth_t* fluid_synth;
  int fluid_sfont_id;
#endif
  /* Soft fallback oscillator when FluidSynth is unavailable. */
  double phase;
  int fallback_note;
  int fallback_vel;
  double fallback_hz;
} SoundState;

static SoundState g_sound;

static uint16_t rd_u16(const uint8_t* p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void sound_push_event(SoundSong* song, uint32_t tick, uint8_t status, uint8_t d1, uint8_t d2, uint8_t ch) {
  if (!song || song->event_count >= SOUND_MAX_EVENTS) {
    return;
  }
  SoundMidiEvent* e = &song->events[song->event_count++];
  e->tick = tick;
  e->status = status;
  e->data1 = d1;
  e->data2 = d2;
  e->channel = ch;
  if (tick > song->duration_ticks) {
    song->duration_ticks = tick;
  }
}

static void sound_decode_track(SoundSong* song, const uint8_t* data, size_t len, uint8_t channel) {
  if (!song || !data || len == 0) {
    return;
  }
  uint32_t time = 0;
  size_t i = 0;
  int program = 0;
  while (i < len && song->event_count < SOUND_MAX_EVENTS - 2) {
    const uint8_t op = data[i];
    if (op == 0xf4 && i + 3 < len) {
      const uint8_t note = data[i + 1];
      const uint8_t dur = data[i + 2] ? data[i + 2] : 1;
      uint8_t vel = data[i + 3];
      if (vel == 0 || vel > 127) {
        vel = 64;
      }
      if (note <= 127) {
        sound_push_event(song, time, 0x90, note, vel, channel);
        sound_push_event(song, time + (uint32_t)dur, 0x80, note, 0, channel);
      }
      time += (uint32_t)dur;
      i += 4;
      continue;
    }
    if (op == 0xc2 && i + 1 < len) {
      program = data[i + 1] & 0x7f;
      sound_push_event(song, time, 0xc0, (uint8_t)program, 0, channel);
      i += 2;
      continue;
    }
    if ((op == 0xf8 || op == 0xf1 || op == 0xf0 || op == 0xfa || op == 0xbf || op == 0xbe ||
         op == 0xff) &&
        i + 1 < len) {
      i += 2;
      continue;
    }
    if ((op == 0xfe || op == 0xfb) && i + 2 < len) {
      i += 3;
      continue;
    }
    if (op == 0xee) {
      i += 1;
      continue;
    }
    /* Untagged delay nibble-ish bytes: advance a little so streams still progress. */
    if (op < 0x80) {
      time += (op == 0) ? 1u : (uint32_t)(op > 32 ? 4 : 1);
    }
    i += 1;
  }
  (void)program;
}

static void sound_parse_handler_tracks(
  const uint8_t* img,
  size_t img_size,
  uint32_t handler,
  uint16_t* out_offs,
  int* out_count
) {
  *out_count = 0;
  uint32_t i = handler;
  const uint32_t end = handler + 96;
  while (i + 3 <= end && i < img_size && *out_count < SOUND_MAX_TRACKS) {
    if (img[i] == 0xc3) {
      break;
    }
    if (img[i] == 0xb9 && i + 3 <= img_size) {
      out_offs[*out_count] = rd_u16(img + i + 1);
      (*out_count)++;
      i += 3;
      continue;
    }
    if (img[i] == 0xe8) {
      i += 3;
      continue;
    }
    if (img[i] == 0xe9) {
      break;
    }
    i++;
  }
}

static bool sound_load_gsound(const char* data_dir) {
  char path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "GSOUND.COL", path, sizeof(path))) {
    diag_warn("sound: cannot resolve GSOUND.COL");
    return false;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    diag_warn("sound: failed to open %s", path);
    return false;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  const long sz = ftell(f);
  if (sz < SOUND_GSOUND_IMG_HDR + 0x3400) {
    fclose(f);
    return false;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }
  uint8_t* file = (uint8_t*)malloc((size_t)sz);
  if (!file) {
    fclose(f);
    return false;
  }
  if (fread(file, 1, (size_t)sz, f) != (size_t)sz) {
    free(file);
    fclose(f);
    return false;
  }
  fclose(f);

  const uint16_t hdr_paras = rd_u16(file + 8);
  const size_t hdr = (size_t)hdr_paras * 16u;
  if (hdr >= (size_t)sz) {
    free(file);
    return false;
  }
  g_sound.gsound_img_size = (size_t)sz - hdr;
  g_sound.gsound_img = (uint8_t*)malloc(g_sound.gsound_img_size);
  if (!g_sound.gsound_img) {
    free(file);
    return false;
  }
  memcpy(g_sound.gsound_img, file + hdr, g_sound.gsound_img_size);
  free(file);

  g_sound.ds_base = (uint32_t)SOUND_GSOUND_DS_PARAS * 16u;
  if (g_sound.ds_base >= g_sound.gsound_img_size) {
    free(g_sound.gsound_img);
    g_sound.gsound_img = NULL;
    return false;
  }

  const uint8_t* img = g_sound.gsound_img;
  const size_t img_size = g_sound.gsound_img_size;
  uint16_t bgm_max = 0x3f;
  if (SOUND_GSOUND_BGM_BOUND + 2 <= img_size) {
    bgm_max = rd_u16(img + SOUND_GSOUND_BGM_BOUND);
  }

  g_sound.song_count = 0;
  for (int id = SOUND_BGM_ID_BASE; id <= (int)bgm_max && id < SOUND_BGM_ID_BASE + 32; ++id) {
    const int idx = id - SOUND_BGM_ID_BASE;
    const uint32_t table_off = SOUND_GSOUND_BGM_TABLE + (uint32_t)idx * 2u;
    if (table_off + 2 > img_size) {
      break;
    }
    const uint32_t handler = rd_u16(img + table_off);
    if (handler == 0 || handler + 4 > img_size) {
      continue;
    }
    uint16_t tracks[SOUND_MAX_TRACKS];
    int track_count = 0;
    sound_parse_handler_tracks(img, img_size, handler, tracks, &track_count);
    if (track_count <= 0) {
      continue;
    }

    SoundSong* song = &g_sound.songs[g_sound.song_count];
    memset(song, 0, sizeof(*song));
    song->id = id;
    for (int t = 0; t < track_count; ++t) {
      const uint32_t abs_off = g_sound.ds_base + (uint32_t)tracks[t];
      if (abs_off >= img_size) {
        continue;
      }
      size_t span = 512;
      if (abs_off + span > img_size) {
        span = img_size - abs_off;
      }
      /* Prefer ending before the next track in the same song when possible. */
      for (int u = 0; u < track_count; ++u) {
        if (u == t) {
          continue;
        }
        const uint32_t other = g_sound.ds_base + (uint32_t)tracks[u];
        if (other > abs_off && other - abs_off < span) {
          span = (size_t)(other - abs_off);
        }
      }
      sound_decode_track(song, img + abs_off, span, (uint8_t)(t & 0x0f));
    }
    if (song->event_count > 0) {
      /* Stable event order by tick. */
      for (int a = 0; a < song->event_count - 1; ++a) {
        for (int b = a + 1; b < song->event_count; ++b) {
          if (song->events[b].tick < song->events[a].tick ||
              (song->events[b].tick == song->events[a].tick &&
               song->events[b].status < song->events[a].status)) {
            SoundMidiEvent tmp = song->events[a];
            song->events[a] = song->events[b];
            song->events[b] = tmp;
          }
        }
      }
      g_sound.song_count++;
    }
  }

  diag_info(
    "sound: GSOUND.COL loaded songs=%d ds_base=0x%x img=%zu",
    g_sound.song_count,
    g_sound.ds_base,
    g_sound.gsound_img_size
  );
  return g_sound.song_count > 0;
}

static SoundSong* sound_find_song(int id) {
  for (int i = 0; i < g_sound.song_count; ++i) {
    if (g_sound.songs[i].id == id) {
      return &g_sound.songs[i];
    }
  }
  return NULL;
}

static const char* sound_find_soundfont(void) {
  const char* env = getenv("COLONIZE_SOUNDFONT");
  if (env && env[0]) {
    FILE* f = fopen(env, "rb");
    if (f) {
      fclose(f);
      return env;
    }
    diag_warn("sound: COLONIZE_SOUNDFONT not readable: %s", env);
  }
  static const char* candidates[] = {
    "/usr/share/sounds/sf2/FluidR3_GM.sf2",
    "/usr/share/sounds/sf2/default-GM.sf2",
    "/usr/share/sounds/sf2/TimGM6mb.sf2",
    "/usr/share/soundfonts/FluidR3_GM.sf2",
    "/usr/share/soundfonts/default.sf2",
    NULL
  };
  for (int i = 0; candidates[i]; ++i) {
    FILE* f = fopen(candidates[i], "rb");
    if (f) {
      fclose(f);
      return candidates[i];
    }
  }
  return NULL;
}

static bool sound_init_fluidsynth(void) {
#if defined(COLONIZE_HAS_FLUIDSYNTH)
  const char* sf = sound_find_soundfont();
  if (!sf) {
    diag_warn("sound: no soundfont found (set COLONIZE_SOUNDFONT); using soft fallback");
    return false;
  }
  g_sound.fluid_settings = new_fluid_settings();
  if (!g_sound.fluid_settings) {
    return false;
  }
  fluid_settings_setnum(g_sound.fluid_settings, "synth.sample-rate", 44100.0);
  fluid_settings_setnum(g_sound.fluid_settings, "synth.gain", 0.6);
  g_sound.fluid_synth = new_fluid_synth(g_sound.fluid_settings);
  if (!g_sound.fluid_synth) {
    delete_fluid_settings(g_sound.fluid_settings);
    g_sound.fluid_settings = NULL;
    return false;
  }
  g_sound.fluid_sfont_id = fluid_synth_sfload(g_sound.fluid_synth, sf, 1);
  if (g_sound.fluid_sfont_id == FLUID_FAILED) {
    diag_warn("sound: fluid_synth_sfload failed for %s", sf);
    delete_fluid_synth(g_sound.fluid_synth);
    delete_fluid_settings(g_sound.fluid_settings);
    g_sound.fluid_synth = NULL;
    g_sound.fluid_settings = NULL;
    return false;
  }
  diag_info("sound: FluidSynth ready soundfont=%s", sf);
  return true;
#else
  (void)sound_find_soundfont;
  return false;
#endif
}

static void sound_all_notes_off_unlocked(void) {
#if defined(COLONIZE_HAS_FLUIDSYNTH)
  if (g_sound.fluid_synth) {
    for (int ch = 0; ch < 16; ++ch) {
      fluid_synth_all_notes_off(g_sound.fluid_synth, ch);
      fluid_synth_all_sounds_off(g_sound.fluid_synth, ch);
    }
  }
#endif
  g_sound.fallback_note = 0;
  g_sound.fallback_vel = 0;
  g_sound.fallback_hz = 0.0;
}

static void sound_apply_event_unlocked(const SoundMidiEvent* e) {
  if (!e) {
    return;
  }
#if defined(COLONIZE_HAS_FLUIDSYNTH)
  if (g_sound.fluid_synth) {
    const int ch = e->channel & 0x0f;
    if (e->status == 0x90 && e->data2 > 0) {
      fluid_synth_noteon(g_sound.fluid_synth, ch, e->data1, e->data2);
    } else if (e->status == 0x80 || (e->status == 0x90 && e->data2 == 0)) {
      fluid_synth_noteoff(g_sound.fluid_synth, ch, e->data1);
    } else if (e->status == 0xc0) {
      fluid_synth_program_change(g_sound.fluid_synth, ch, e->data1);
      g_sound.program = e->data1;
    }
    return;
  }
#endif
  if (e->status == 0x90 && e->data2 > 0) {
    g_sound.fallback_note = e->data1;
    g_sound.fallback_vel = e->data2;
    g_sound.fallback_hz = 440.0 * pow(2.0, ((double)e->data1 - 69.0) / 12.0);
  } else if (e->status == 0x80 || (e->status == 0x90 && e->data2 == 0)) {
    if (g_sound.fallback_note == e->data1) {
      g_sound.fallback_note = 0;
      g_sound.fallback_vel = 0;
      g_sound.fallback_hz = 0.0;
    }
  }
}

static void sound_seek_events_unlocked(SoundSong* song, uint32_t from_tick, uint32_t to_tick) {
  if (!song) {
    return;
  }
  for (int i = 0; i < song->event_count; ++i) {
    const SoundMidiEvent* e = &song->events[i];
    if (e->tick < from_tick) {
      continue;
    }
    if (e->tick > to_tick) {
      break;
    }
    sound_apply_event_unlocked(e);
  }
}

bool sound_init(const char* data_dir, bool enable_audio) {
  if (g_sound.inited) {
    return true;
  }
  memset(&g_sound, 0, sizeof(g_sound));
  pthread_mutex_init(&g_sound.lock, NULL);
  /* Open the device whenever the platform allows it so Pick Music previews work
   * even while ambient autoplay (COLONIZE_SOUND_PLAYBACK_ENABLED) is parked. */
  g_sound.enable_audio = enable_audio;
  g_sound.opts.background_music = true;
  g_sound.opts.event_music = true;
  g_sound.opts.sound_effects = true;
  g_sound.active_song_id = -1;
  g_sound.bgm_track = 0;
  g_sound.bgm_song_id = -1;
  g_sound.preview_active = false;
  /*
   * F4 duration bytes are driver time-base units. Empirically ~3.75 ms/tick
   * (about 4x faster than a naive 15 ms guess) — still needs DOS validation.
   */
  g_sound.ticks_per_sample = 1.0 / (SOUND_TICK_SECONDS * 44100.0);

  g_sound.gsound_ok = sound_load_gsound(data_dir ? data_dir : "./COLONIZE");
  if (g_sound.enable_audio) {
    g_sound.backend_ok = sound_init_fluidsynth();
    if (!g_sound.backend_ok) {
      diag_info("sound: using square-wave fallback renderer");
    }
    if (!COLONIZE_SOUND_PLAYBACK_ENABLED) {
      diag_info(
        "sound: autoplay parked (COLONIZE_SOUND_PLAYBACK_ENABLED=0); "
        "Pick Music preview available"
      );
    }
  } else {
    diag_info("sound: audio disabled");
  }

  g_sound.inited = true;
  return true;
}

bool sound_playback_enabled(void) {
  return COLONIZE_SOUND_PLAYBACK_ENABLED != 0;
}

bool sound_audio_output_ready(void) {
  return g_sound.inited && g_sound.enable_audio;
}

void sound_shutdown(void) {
  if (!g_sound.inited) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  sound_all_notes_off_unlocked();
#if defined(COLONIZE_HAS_FLUIDSYNTH)
  if (g_sound.fluid_synth) {
    delete_fluid_synth(g_sound.fluid_synth);
    g_sound.fluid_synth = NULL;
  }
  if (g_sound.fluid_settings) {
    delete_fluid_settings(g_sound.fluid_settings);
    g_sound.fluid_settings = NULL;
  }
#endif
  free(g_sound.gsound_img);
  g_sound.gsound_img = NULL;
  pthread_mutex_unlock(&g_sound.lock);
  pthread_mutex_destroy(&g_sound.lock);
  memset(&g_sound, 0, sizeof(g_sound));
}

bool sound_ok(void) {
  return g_sound.inited && g_sound.gsound_ok;
}

bool sound_backend_ok(void) {
  return g_sound.inited && g_sound.backend_ok;
}

void sound_set_options(ColonizeSoundOptions opts) {
  pthread_mutex_lock(&g_sound.lock);
  g_sound.opts = opts;
  if (!opts.background_music) {
    g_sound.bgm_track = 0;
    g_sound.active_song_id = -1;
    sound_all_notes_off_unlocked();
  }
  pthread_mutex_unlock(&g_sound.lock);
}

ColonizeSoundOptions sound_get_options(void) {
  return g_sound.opts;
}

static void sound_start_song_unlocked(int id) {
  SoundSong* song = sound_find_song(id);
  sound_all_notes_off_unlocked();
  if (!song) {
    g_sound.active_song_id = -1;
    return;
  }
  g_sound.active_song_id = id;
  g_sound.play_tick = 0;
  g_sound.tick_accum = 0.0;
  /* Apply program / initial notes at tick 0. */
  sound_seek_events_unlocked(song, 0, 0);
}

void sound_play(int id) {
  if (!g_sound.inited || !COLONIZE_SOUND_PLAYBACK_ENABLED) {
    return;
  }
  /* FUN_12d8_000e gating. */
  if (id >= 0x10) {
    if ((id & 0x20) != 0 && !g_sound.opts.background_music) {
      return;
    }
    if ((id & 0x40) != 0 && !g_sound.opts.event_music) {
      return;
    }
  }
  if (id == 1 || id == 0) {
    pthread_mutex_lock(&g_sound.lock);
    g_sound.preview_active = false;
    g_sound.active_song_id = -1;
    g_sound.bgm_track = 0;
    sound_all_notes_off_unlocked();
    pthread_mutex_unlock(&g_sound.lock);
    return;
  }
  if (id >= SOUND_BGM_ID_BASE && id < SOUND_EVENT_ID_BASE) {
    pthread_mutex_lock(&g_sound.lock);
    g_sound.preview_active = false;
    sound_start_song_unlocked(id);
    pthread_mutex_unlock(&g_sound.lock);
  }
  /* Event / SFX IDs: not decoded in this pass (music-focused). */
}

void sound_play_preview(int id) {
  if (!g_sound.inited) {
    return;
  }
  if (id == 0 || id == 1) {
    sound_stop_preview();
    return;
  }
  if (id < SOUND_BGM_ID_BASE || id >= SOUND_EVENT_ID_BASE) {
    return;
  }
  if (!sound_gsound_has_song(id)) {
    diag_warn("sound: preview missing song id 0x%02x", id);
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  g_sound.preview_active = true;
  sound_start_song_unlocked(id);
  pthread_mutex_unlock(&g_sound.lock);
}

void sound_stop_preview(void) {
  if (!g_sound.inited) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  if (g_sound.preview_active) {
    g_sound.preview_active = false;
    g_sound.active_song_id = -1;
    sound_all_notes_off_unlocked();
  }
  pthread_mutex_unlock(&g_sound.lock);
}

void sound_set_bgm(int track) {
  if (!g_sound.inited || !COLONIZE_SOUND_PLAYBACK_ENABLED) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  if (track <= 0) {
    g_sound.bgm_track = 0;
    g_sound.bgm_song_id = -1;
    g_sound.need_restart = false;
    if (!g_sound.preview_active) {
      g_sound.active_song_id = -1;
      sound_all_notes_off_unlocked();
    }
    pthread_mutex_unlock(&g_sound.lock);
    return;
  }
  g_sound.bgm_track = track;
  g_sound.bgm_song_id = SOUND_BGM_ID_BASE + track;
  g_sound.need_restart = true;
  pthread_mutex_unlock(&g_sound.lock);
}

void sound_stop_bgm(void) {
  sound_set_bgm(0);
}

void sound_service(void) {
  if (!g_sound.inited) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  const bool autoplay = COLONIZE_SOUND_PLAYBACK_ENABLED != 0;
  if (autoplay && g_sound.need_restart) {
    g_sound.need_restart = false;
    if (g_sound.opts.background_music && g_sound.bgm_song_id >= 0 && !g_sound.preview_active) {
      sound_start_song_unlocked(g_sound.bgm_song_id);
    }
  }
  /* Loop BGM / preview when the decoded song ends. */
  if (g_sound.active_song_id >= 0 &&
      (g_sound.preview_active || (autoplay && g_sound.opts.background_music))) {
    SoundSong* song = sound_find_song(g_sound.active_song_id);
    if (song && song->duration_ticks > 0 && g_sound.play_tick >= song->duration_ticks + 8) {
      sound_start_song_unlocked(g_sound.active_song_id);
    }
  }
  pthread_mutex_unlock(&g_sound.lock);
}

static void sound_advance_unlocked(int frames, int sample_rate) {
  if (g_sound.active_song_id < 0 || sample_rate <= 0) {
    return;
  }
  SoundSong* song = sound_find_song(g_sound.active_song_id);
  if (!song) {
    return;
  }
  const double tps = 1.0 / (SOUND_TICK_SECONDS * (double)sample_rate);
  const uint32_t start = g_sound.play_tick;
  g_sound.tick_accum += (double)frames * tps;
  const uint32_t advance = (uint32_t)g_sound.tick_accum;
  if (advance == 0) {
    return;
  }
  g_sound.tick_accum -= (double)advance;
  const uint32_t end = start + advance;
  sound_seek_events_unlocked(song, start + (start == 0 ? 1u : 0u), end);
  g_sound.play_tick = end;
}

void sound_render_s16(int16_t* dst, int frames, int channels, int sample_rate) {
  if (!dst || frames <= 0 || channels <= 0) {
    return;
  }
  memset(dst, 0, (size_t)frames * (size_t)channels * sizeof(int16_t));
  if (!g_sound.inited || !g_sound.enable_audio) {
    return;
  }

  pthread_mutex_lock(&g_sound.lock);
  /* Ambient path parked: only emit when Pick Music (or offline) set preview_active. */
  if (!COLONIZE_SOUND_PLAYBACK_ENABLED && !g_sound.preview_active) {
    pthread_mutex_unlock(&g_sound.lock);
    return;
  }

  sound_advance_unlocked(frames, sample_rate);

#if defined(COLONIZE_HAS_FLUIDSYNTH)
  if (g_sound.fluid_synth) {
    if (channels == 2) {
      fluid_synth_write_s16(g_sound.fluid_synth, frames, dst, 0, 2, dst, 1, 2);
    } else {
      int16_t* tmp = (int16_t*)malloc((size_t)frames * 2u * sizeof(int16_t));
      if (tmp) {
        fluid_synth_write_s16(g_sound.fluid_synth, frames, tmp, 0, 2, tmp, 1, 2);
        for (int i = 0; i < frames; ++i) {
          const int32_t m = ((int32_t)tmp[i * 2] + (int32_t)tmp[i * 2 + 1]) / 2;
          dst[i] = (int16_t)m;
        }
        free(tmp);
      }
    }
    pthread_mutex_unlock(&g_sound.lock);
    return;
  }
#endif

  /* Square-wave fallback so music is still audible without FluidSynth. */
  for (int i = 0; i < frames; ++i) {
    int16_t s = 0;
    if (g_sound.fallback_hz > 0.0 && g_sound.fallback_vel > 0) {
      g_sound.phase += g_sound.fallback_hz / (double)sample_rate;
      if (g_sound.phase >= 1.0) {
        g_sound.phase -= 1.0;
      }
      const double amp = (g_sound.fallback_vel / 127.0) * 5000.0;
      s = (int16_t)((g_sound.phase < 0.5) ? amp : -amp);
    }
    for (int c = 0; c < channels; ++c) {
      dst[i * channels + c] = s;
    }
  }
  pthread_mutex_unlock(&g_sound.lock);
}

int sound_gsound_song_count(void) {
  return g_sound.song_count;
}

bool sound_gsound_has_song(int id) {
  return sound_find_song(id) != NULL;
}

int sound_render_offline_mono(int song_id, int16_t* dst, int max_frames, int sample_rate) {
  if (!dst || max_frames <= 0 || !g_sound.inited) {
    return 0;
  }
  pthread_mutex_lock(&g_sound.lock);
  const int prev = g_sound.active_song_id;
  const uint32_t prev_tick = g_sound.play_tick;
  const double prev_acc = g_sound.tick_accum;
  const bool prev_preview = g_sound.preview_active;
  const bool prev_enable = g_sound.enable_audio;
  g_sound.enable_audio = true; /* allow render path without an SDL device */
  g_sound.preview_active = true;
  sound_start_song_unlocked(song_id);
  pthread_mutex_unlock(&g_sound.lock);

  const int chunk = 512;
  int written = 0;
  while (written + chunk <= max_frames) {
    sound_render_s16(dst + written, chunk, 1, sample_rate);
    written += chunk;
  }

  pthread_mutex_lock(&g_sound.lock);
  g_sound.active_song_id = prev;
  g_sound.play_tick = prev_tick;
  g_sound.tick_accum = prev_acc;
  g_sound.preview_active = prev_preview;
  g_sound.enable_audio = prev_enable;
  sound_all_notes_off_unlocked();
  pthread_mutex_unlock(&g_sound.lock);
  return written;
}
