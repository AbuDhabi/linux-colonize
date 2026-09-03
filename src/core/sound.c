#include "core/sound.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/gsound_vm.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

#if defined(COLONIZE_HAS_FLUIDSYNTH)
#include <fluidsynth.h>
#endif

/*
 * Playback runs the GSOUND.COL driver emulator (gsound_vm) in real time from
 * the audio callback: every ~59.95 Hz PIT tick advances the nine voice
 * blocks exactly like the DOS IRQ, and the MIDI bytes it produces go straight
 * to FluidSynth. Songs loop, fade and chain by themselves (FD loop opcode,
 * 0x1819 fade, DS:E6/EA segment callbacks), so nothing here restarts tracks.
 *
 * The offline decode used by tests / the dump tool runs a scratch VM into an
 * event list until the song loops back to its start (or ends).
 */

#define SOUND_MAX_EVENTS 32768
#define SOUND_GSOUND_IMG_HDR 512
#define SOUND_TICK_HZ GSOUND_TICK_HZ
#define SOUND_MAX_TRACK_TICKS 14400u /* 4 minutes @ ~60 Hz */
#define SOUND_SFX_MAX 64
#define SOUND_SFX_QUEUE 16 /* driver ring DS:1B71..1C71 = 16 slots */
#define SOUND_SFX_GAIN 96.0 /* 8-bit PCM → s16, ~37% full scale beside the halved synth */

typedef struct SoundMidiEvent {
  uint32_t tick;
  uint8_t status; /* 0x90/0xB0/0xC0/0xE0 with channel in the low nibble */
  uint8_t data1;
  uint8_t data2;
} SoundMidiEvent;

typedef struct SoundSong {
  int id;
  int event_count;
  uint32_t duration_ticks;
  SoundMidiEvent* events;
} SoundSong;

typedef struct SoundState {
  bool inited;
  bool enable_audio;
  bool gsound_ok;
  bool backend_ok;
  ColonizeSoundOptions opts;

  uint8_t* gsound_img;
  size_t gsound_img_size;
  GsoundVm* vm;

  pthread_mutex_t lock;
  /*
   * DOS BGM scheduler state (segment 129f). Names follow the DS offsets:
   *   current_id   DS:0x96  id last handed to the driver
   *   pending_id   DS:0x94  explicit id queued by sound_play (-1 = none)
   *   pending      DS:0x9e  pump must act on the next idle poll
   *   category     DS:0x9a  tune pool requested by the current screen (1..7)
   *   category_next DS:0x98 queued pool applied after the next pick
   *   category_applied DS:0x9c pool the last pick was drawn from
   */
  int current_id;
  int pending_id;
  bool pending;
  int category;
  int category_next;
  int category_applied;
  uint32_t pick_rng; /* DOS reseeds from DS:0x83a8 around each pick; private LCG here */
  bool preview_active; /* Pick Music: selection plays immediately (FUN_281f_04c0) */
  double samples_to_tick; /* audio frames left before the next PIT tick */

  SoundSong decoded; /* one-entry cache for the offline event API */

  /* COLDIG.BIN digital samples: driver plays queued samples one after another. */
  uint8_t* sfx_data;
  size_t sfx_size;
  int sfx_count;
  uint32_t sfx_off[SOUND_SFX_MAX];
  uint32_t sfx_len[SOUND_SFX_MAX];
  int sfx_queue[SOUND_SFX_QUEUE];
  int sfx_queue_len;
  int sfx_playing; /* -1 = idle */
  double sfx_pos;  /* source sample position */
  int sfx_last_index; /* diagnostics */

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

  g_sound.vm = gsound_vm_create(g_sound.gsound_img, g_sound.gsound_img_size);
  if (!g_sound.vm) {
    diag_warn("sound: GSOUND.COL image rejected (%zu bytes)", g_sound.gsound_img_size);
    free(g_sound.gsound_img);
    g_sound.gsound_img = NULL;
    return false;
  }
  int songs = 0;
  for (int id = SOUND_BGM_ID_BASE; id < 0x60; ++id) {
    if (gsound_vm_has_song(g_sound.vm, id)) {
      songs++;
    }
  }
  diag_info("sound: GSOUND.COL loaded songs=%d img=%zu", songs, g_sound.gsound_img_size);
  return songs > 0;
}

static bool sound_path_readable(const char* path) {
  if (!path || !path[0]) {
    return false;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  fclose(f);
  return true;
}

/* Returns a durable path string (static buffer or literal / env). */
static const char* sound_find_soundfont(const char* data_dir) {
  static char path_buf[768];
  const char* env = getenv("COLONIZE_SOUNDFONT");
  if (env && env[0]) {
    if (sound_path_readable(env)) {
      return env;
    }
    diag_warn("sound: COLONIZE_SOUNDFONT not readable: %s", env);
  }

  /* Bundled default: data/soundfonts/Roland_SC-55.sf2 (GPL-3+, see COPYRIGHT). */
  {
    const char* exe = diag_exe_dir();
    const char* bundled[] = {
      "data/soundfonts/Roland_SC-55.sf2",
      "./data/soundfonts/Roland_SC-55.sf2",
      NULL
    };
    for (int i = 0; bundled[i]; ++i) {
      if (sound_path_readable(bundled[i])) {
        return bundled[i];
      }
    }
    if (exe && exe[0]) {
      snprintf(path_buf, sizeof(path_buf), "%s/data/soundfonts/Roland_SC-55.sf2", exe);
      if (sound_path_readable(path_buf)) {
        return path_buf;
      }
      snprintf(path_buf, sizeof(path_buf), "%s/soundfonts/Roland_SC-55.sf2", exe);
      if (sound_path_readable(path_buf)) {
        return path_buf;
      }
    }
    if (data_dir && data_dir[0]) {
      snprintf(path_buf, sizeof(path_buf), "%s/../data/soundfonts/Roland_SC-55.sf2", data_dir);
      if (sound_path_readable(path_buf)) {
        return path_buf;
      }
      snprintf(path_buf, sizeof(path_buf), "%s/soundfonts/Roland_SC-55.sf2", data_dir);
      if (sound_path_readable(path_buf)) {
        return path_buf;
      }
    }
  }

  /*
   * System fallbacks: SC-55-character banks first (MicroProse GM / Sound Canvas),
   * then GeneralUser GS, then common distro FluidR3.
   */
  static const char* candidates[] = {
    "/usr/share/scummvm/Roland_SC-55.sf2",
    "/usr/share/sounds/sf2/SC-55.sf2",
    "/usr/share/sounds/sf2/SC55.sf2",
    "/usr/share/soundfonts/SC-55.sf2",
    "/usr/share/sounds/sf2/GeneralUser-GS.sf2",
    "/usr/share/sounds/sf2/GeneralUser_GS.sf2",
    "/usr/share/sounds/sf2/GeneralUser.sf2",
    "/usr/share/soundfonts/GeneralUser-GS.sf2",
    "/usr/share/soundfonts/GeneralUser_GS.sf2",
    "/usr/local/share/sounds/sf2/GeneralUser-GS.sf2",
    "/usr/share/sounds/sf2/FluidR3_GM.sf2",
    "/usr/share/sounds/sf2/default-GM.sf2",
    "/usr/share/sounds/sf2/TimGM6mb.sf2",
    "/usr/share/soundfonts/FluidR3_GM.sf2",
    "/usr/share/soundfonts/default.sf2",
    NULL
  };
  for (int i = 0; candidates[i]; ++i) {
    if (sound_path_readable(candidates[i])) {
      return candidates[i];
    }
  }
  return NULL;
}

static bool sound_init_fluidsynth(const char* data_dir) {
#if defined(COLONIZE_HAS_FLUIDSYNTH)
  const char* sf = sound_find_soundfont(data_dir);
  if (!sf) {
    diag_warn("sound: no soundfont found (set COLONIZE_SOUNDFONT); using soft fallback");
    return false;
  }
  g_sound.fluid_settings = new_fluid_settings();
  if (!g_sound.fluid_settings) {
    return false;
  }
  fluid_settings_setnum(g_sound.fluid_settings, "synth.sample-rate", 44100.0);
  fluid_settings_setnum(g_sound.fluid_settings, "synth.gain", 0.3); /* halved 2026-08-27 per listen test */
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
  /* SC-55-ish room: Staging uses similar defaults for GeneralUser / SC-55 SF2. */
  fluid_synth_reverb_on(g_sound.fluid_synth, -1, 1);
  fluid_synth_chorus_on(g_sound.fluid_synth, -1, 1);
  fluid_synth_set_reverb_group_roomsize(g_sound.fluid_synth, -1, 0.7);
  fluid_synth_set_reverb_group_damp(g_sound.fluid_synth, -1, 0.3);
  fluid_synth_set_reverb_group_width(g_sound.fluid_synth, -1, 0.5);
  fluid_synth_set_reverb_group_level(g_sound.fluid_synth, -1, 0.8);
  fluid_synth_set_chorus_group_nr(g_sound.fluid_synth, -1, 3);
  fluid_synth_set_chorus_group_level(g_sound.fluid_synth, -1, 1.2);
  fluid_synth_set_chorus_group_speed(g_sound.fluid_synth, -1, 0.3);
  fluid_synth_set_chorus_group_depth(g_sound.fluid_synth, -1, 8.0);
  fluid_synth_set_chorus_group_type(g_sound.fluid_synth, -1, FLUID_CHORUS_MOD_SINE);
  diag_info("sound: FluidSynth ready soundfont=%s", sf);
  return true;
#else
  (void)data_dir;
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

static void sound_apply_midi_unlocked(uint8_t status, uint8_t d1, uint8_t d2) {
  const int ch = status & 0x0f;
  const uint8_t kind = status & 0xf0;
#if defined(COLONIZE_HAS_FLUIDSYNTH)
  if (g_sound.fluid_synth) {
    if (kind == 0x90 && d2 > 0) {
      fluid_synth_noteon(g_sound.fluid_synth, ch, d1, d2);
    } else if (kind == 0x80 || (kind == 0x90 && d2 == 0)) {
      fluid_synth_noteoff(g_sound.fluid_synth, ch, d1);
    } else if (kind == 0xc0) {
      fluid_synth_program_change(g_sound.fluid_synth, ch, d1);
    } else if (kind == 0xb0) {
      fluid_synth_cc(g_sound.fluid_synth, ch, d1, d2);
    } else if (kind == 0xe0) {
      fluid_synth_pitch_bend(g_sound.fluid_synth, ch, ((int)d2 << 7) | (int)d1);
    }
    return;
  }
#endif
  if (kind == 0x90 && d2 > 0) {
    g_sound.fallback_note = d1;
    g_sound.fallback_vel = d2;
    g_sound.fallback_hz = 440.0 * pow(2.0, ((double)d1 - 69.0) / 12.0);
  } else if (kind == 0x80 || (kind == 0x90 && d2 == 0)) {
    if (g_sound.fallback_note == d1) {
      g_sound.fallback_note = 0;
      g_sound.fallback_vel = 0;
      g_sound.fallback_hz = 0.0;
    }
  }
}

static void sound_vm_midi_cb(void* user, uint8_t status, uint8_t d1, uint8_t d2) {
  (void)user;
  sound_apply_midi_unlocked(status, d1, d2);
}

static void sound_load_coldig(const char* data_dir) {
  char path[512];
  g_sound.sfx_count = 0;
  g_sound.sfx_playing = -1;
  if (!dos_compat_normalize_asset_path(data_dir, "COLDIG.BIN", path, sizeof(path))) {
    diag_info("sound: COLDIG.BIN not found; digital SFX off");
    return;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    return;
  }
  fseek(f, 0, SEEK_END);
  const long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0) {
    fclose(f);
    return;
  }
  g_sound.sfx_data = (uint8_t*)malloc((size_t)sz);
  if (!g_sound.sfx_data || fread(g_sound.sfx_data, 1, (size_t)sz, f) != (size_t)sz) {
    free(g_sound.sfx_data);
    g_sound.sfx_data = NULL;
    fclose(f);
    return;
  }
  fclose(f);
  g_sound.sfx_size = (size_t)sz;
  g_sound.sfx_count =
    gsound_vm_sfx_table(g_sound.vm, g_sound.sfx_size, g_sound.sfx_off, g_sound.sfx_len, SOUND_SFX_MAX);
  if (g_sound.sfx_count > SOUND_SFX_MAX) {
    g_sound.sfx_count = SOUND_SFX_MAX;
  }
  diag_info("sound: COLDIG.BIN %ld bytes, %d samples", sz, g_sound.sfx_count);
}

/* FUN_1000_27b4: queue a sample; rejected when the 16-slot ring is full. */
static void sound_vm_sfx_cb(void* user, int index) {
  (void)user;
  if (index < 0 || index >= g_sound.sfx_count || !g_sound.opts.sound_effects) {
    return;
  }
  if (g_sound.sfx_playing < 0) {
    g_sound.sfx_playing = index;
    g_sound.sfx_pos = 0.0;
    return;
  }
  if (g_sound.sfx_queue_len < SOUND_SFX_QUEUE) {
    g_sound.sfx_queue[g_sound.sfx_queue_len++] = index;
  }
}

static void sound_sfx_stop_all_unlocked(void) {
  g_sound.sfx_playing = -1;
  g_sound.sfx_queue_len = 0;
}

/* Mix the running sample into an interleaved s16 buffer (linear resampling). */
static void sound_sfx_mix_unlocked(int16_t* dst, int frames, int channels, int sample_rate) {
  int i = 0;
  while (i < frames && g_sound.sfx_playing >= 0) {
    const int idx = g_sound.sfx_playing;
    const uint8_t* src = g_sound.sfx_data + g_sound.sfx_off[idx];
    const uint32_t len = g_sound.sfx_len[idx];
    const double step = (double)gsound_vm_sfx_rate(idx) / (double)sample_rate;
    for (; i < frames; ++i) {
      const double pos = g_sound.sfx_pos;
      const uint32_t p0 = (uint32_t)pos;
      if (p0 + 1 >= len) {
        g_sound.sfx_playing = -1;
        g_sound.sfx_pos = 0.0;
        if (g_sound.sfx_queue_len > 0) {
          g_sound.sfx_playing = g_sound.sfx_queue[0];
          memmove(g_sound.sfx_queue, g_sound.sfx_queue + 1, (size_t)(g_sound.sfx_queue_len - 1) * sizeof(int));
          g_sound.sfx_queue_len--;
        }
        break;
      }
      const double frac = pos - (double)p0;
      const double a = (double)src[p0] - 128.0;
      const double b = (double)src[p0 + 1] - 128.0;
      const int32_t v = (int32_t)((a + (b - a) * frac) * SOUND_SFX_GAIN);
      for (int c = 0; c < channels; ++c) {
        int32_t m = (int32_t)dst[(size_t)i * (size_t)channels + (size_t)c] + v;
        if (m > 32767) m = 32767;
        if (m < -32768) m = -32768;
        dst[(size_t)i * (size_t)channels + (size_t)c] = (int16_t)m;
      }
      g_sound.sfx_pos += step;
    }
  }
}

/* ---- offline decode (tests / dump tool) --------------------------------- */

static void sound_decode_cb(void* user, uint8_t status, uint8_t d1, uint8_t d2) {
  SoundSong* song = (SoundSong*)user;
  if (song->event_count >= SOUND_MAX_EVENTS) {
    return;
  }
  SoundMidiEvent* e = &song->events[song->event_count++];
  e->tick = song->duration_ticks;
  e->status = status;
  e->data1 = d1;
  e->data2 = d2;
}

static SoundSong* sound_decoded_song(int id) {
  if (!g_sound.inited || !g_sound.gsound_img || !gsound_vm_has_song(g_sound.vm, id) ||
      id < SOUND_BGM_ID_BASE) {
    return NULL;
  }
  SoundSong* song = &g_sound.decoded;
  if (song->events && song->id == id) {
    return song;
  }
  if (!song->events) {
    song->events = (SoundMidiEvent*)calloc(SOUND_MAX_EVENTS, sizeof(SoundMidiEvent));
    if (!song->events) {
      return NULL;
    }
  }
  song->id = id;
  song->event_count = 0;
  song->duration_ticks = 0;

  GsoundVm* vm = gsound_vm_create(g_sound.gsound_img, g_sound.gsound_img_size);
  if (!vm) {
    return NULL;
  }
  gsound_vm_set_midi(vm, sound_decode_cb, song);
  gsound_vm_play(vm, id);
  while (song->duration_ticks < SOUND_MAX_TRACK_TICKS && song->event_count < SOUND_MAX_EVENTS) {
    gsound_vm_tick(vm);
    if (gsound_vm_loop_tick(vm) != 0) {
      /* Song wrapped to its start on this tick: one full pass captured. */
      break;
    }
    song->duration_ticks++;
    if (!gsound_vm_active(vm)) {
      break;
    }
  }
  diag_info(
    "sound: song 0x%02x %s at tick %u (%d events)",
    id,
    gsound_vm_loop_tick(vm) ? "loops" : "ends",
    song->duration_ticks,
    song->event_count
  );
  if (gsound_vm_unsupported_count(vm) > 0) {
    diag_warn("sound: song 0x%02x hit %d unsupported driver paths", id, gsound_vm_unsupported_count(vm));
  }
  gsound_vm_destroy(vm);
  return song;
}

/* ---- lifecycle ---------------------------------------------------------- */

bool sound_init(const char* data_dir, bool enable_audio) {
  if (g_sound.inited) {
    return true;
  }
  memset(&g_sound, 0, sizeof(g_sound));
  pthread_mutex_init(&g_sound.lock, NULL);
  /* Open the device whenever the platform allows so title/map BGM and Pick Music work. */
  g_sound.enable_audio = enable_audio;
  g_sound.opts.background_music = true;
  g_sound.opts.event_music = true;
  g_sound.opts.sound_effects = true;
  g_sound.current_id = -1;
  g_sound.pending_id = -1;
  g_sound.pending = false;
  g_sound.category = 0;
  g_sound.category_next = 0;
  g_sound.category_applied = 0;
  g_sound.pick_rng = 0x2545u;
  g_sound.preview_active = false;
  g_sound.samples_to_tick = 0.0;

  const char* dir = data_dir ? data_dir : "./COLONIZE";
  g_sound.gsound_ok = sound_load_gsound(dir);
  if (g_sound.enable_audio) {
    g_sound.backend_ok = sound_init_fluidsynth(dir);
    if (!g_sound.backend_ok) {
      diag_info("sound: using square-wave fallback renderer");
    }
  } else {
    diag_info("sound: audio disabled");
  }
  if (g_sound.vm) {
    gsound_vm_set_midi(g_sound.vm, sound_vm_midi_cb, NULL);
    gsound_vm_set_sfx(g_sound.vm, sound_vm_sfx_cb, NULL);
    gsound_vm_reset_channels(g_sound.vm);
    sound_load_coldig(dir);
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
  gsound_vm_destroy(g_sound.vm);
  g_sound.vm = NULL;
  free(g_sound.sfx_data);
  g_sound.sfx_data = NULL;
  free(g_sound.decoded.events);
  g_sound.decoded.events = NULL;
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
  if (!opts.background_music && g_sound.vm) {
    g_sound.current_id = -1;
    gsound_vm_play(g_sound.vm, 1);
  }
  pthread_mutex_unlock(&g_sound.lock);
}

ColonizeSoundOptions sound_get_options(void) {
  return g_sound.opts;
}

/* ---- transport (DOS segment 129f BGM scheduler) ------------------------- */

/* FUN_12d8_000e: option gate in front of the driver dispatcher. */
static void sound_dispatch_gated_unlocked(int id) {
  if (!g_sound.vm || id < 0) {
    return;
  }
  if (id >= 0x8020) {
    /* Chord stings (0x8020 war declaration, 0x8024 assign colonist): the
     * driver's fourth table; treated like event music for the option gate. */
    if (!g_sound.opts.event_music) {
      return;
    }
  } else if (id >= 0x10) {
    if ((id & 0x20) != 0 && !g_sound.opts.background_music) {
      return;
    }
    if ((id & 0x40) != 0 && !g_sound.opts.event_music) {
      return;
    }
  }
  gsound_vm_play(g_sound.vm, id);
  if (id == 0) {
    sound_all_notes_off_unlocked();
    sound_sfx_stop_all_unlocked();
  }
}

/*
 * FUN_129f_0008: tune number (1..26, Pick Music order: 12 main, 0x28, five
 * Independence, four Military, Natives, Indian Victory, Tenochtitlan,
 * Pizarro) → driver id. Out of range → 0x25.
 */
static int sound_tune_to_id(int tune) {
  static const uint8_t k_ids[27] = {
    0x25, 0x20, 0x21, 0x22, 0x23, 0x3a, 0x3b, 0x38, 0x24, 0x25, 0x26, 0x27, 0x39,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x33, 0x32, 0x35, 0x36,
  };
  if (tune < 1 || tune > 26) {
    return 0x25;
  }
  return k_ids[tune];
}

static uint32_t sound_pick_rand(uint32_t n) {
  g_sound.pick_rng = g_sound.pick_rng * 1103515245u + 12345u;
  return n ? ((g_sound.pick_rng >> 16) & 0x7fffu) % n : 0;
}

/* FUN_129f_00f6 tune-pool selection: DS:0x9a category → (first tune, count). */
static int sound_pick_next_tune_id(void) {
  int base = 1;
  int count = 12;
  if (sound_pick_rand(8) == 0) {
    base = 13;
    count = 11;
  }
  switch (g_sound.category) {
    case 1: base = 1; count = 7; break;   /* map: calm tunes */
    case 2: base = 8; count = 5; break;   /* colony: fiddle tunes */
    case 3: base = 13; count = 6; break;  /* Europe: 0x28 + Independence set */
    case 4: base = 19; count = 4; break;  /* Military set */
    case 5: if (g_sound.current_id != 0x33) { base = 23; count = 1; } break;
    case 6: if (g_sound.current_id != 0x35) { base = 25; count = 1; } break;
    case 7: if (g_sound.current_id != 0x36) { base = 26; count = 1; } break;
    default: break;
  }
  int tune = base;
  int id = sound_tune_to_id(tune);
  for (int tries = 0; tries < 32; ++tries) {
    tune = base + (int)sound_pick_rand((uint32_t)count);
    id = sound_tune_to_id(tune);
    if (id != g_sound.current_id || count == 1) {
      break;
    }
  }
  if (g_sound.category == 0) {
    g_sound.category = tune <= 6 ? 1 : tune <= 12 ? 2 : tune <= 18 ? 3 : tune <= 22 ? 4
                     : tune <= 24 ? 5 : tune <= 25 ? 6 : 7;
  }
  g_sound.category_applied = g_sound.category;
  g_sound.category = g_sound.category_next;
  g_sound.category_next = 0;
  return id;
}

/*
 * FUN_129f_00f6 idle pump: once the driver has no voice left, play the queued
 * explicit id, else draw the next tune from the current pool. DOS only polls
 * this with sound effects enabled or a pending change. Category 0 is "no
 * pool" (DS:0x9a); do not invent a random song until sound_play / sound_set_bgm
 * arms one — otherwise the audio callback starts a map tune at launch before
 * the intro can queue 0x34.
 */
static void sound_pump_unlocked(void) {
  if (!g_sound.vm || gsound_vm_active(g_sound.vm)) {
    return;
  }
  if (!g_sound.pending && !g_sound.opts.background_music) {
    return;
  }
  g_sound.pending = false;
  int id;
  if (g_sound.pending_id >= 0) {
    id = g_sound.pending_id;
    g_sound.pending_id = -1;
  } else {
    if (!g_sound.opts.background_music || g_sound.preview_active) {
      return;
    }
    if (g_sound.category <= 0 && g_sound.category_applied <= 0) {
      return;
    }
    id = sound_pick_next_tune_id();
  }
  g_sound.current_id = id;
  sound_dispatch_gated_unlocked(id);
}

/* FUN_129f_02cc: queue an explicit id; fade the running song so the pump can start it. */
static void sound_queue_unlocked(int id) {
  if (id == g_sound.current_id) {
    return;
  }
  g_sound.pending_id = id;
  g_sound.pending = true;
  if (id >= 0) {
    sound_dispatch_gated_unlocked(1);
  }
}

void sound_play(int id) {
  if (!g_sound.inited || !COLONIZE_SOUND_PLAYBACK_ENABLED) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  g_sound.preview_active = false;
  if (id == 0 || id == 1) {
    /* System ids act at once (FUN_12d8_000e forwards ids < 0x10 unconditionally). */
    g_sound.pending_id = -1;
    g_sound.current_id = -1;
    sound_dispatch_gated_unlocked(id);
  } else if (id >= SOUND_EVENT_ID_BASE) {
    /* Event ids go straight to the driver (FUN_281f_04c0 → FUN_12d8_000e):
     * a COLDIG sample + MIDI sting on channels 7/8, BGM state untouched. */
    sound_dispatch_gated_unlocked(id);
  } else if (g_sound.category <= 0 && g_sound.category_applied <= 0) {
    /* No VICEROY pool: OPENING.EXE / CLOSING.EXE call FUN_12d8_000e and the
     * cue starts now. sound_queue waits for the driver to go idle, so a
     * leftover Independence tune plus firework stings would postpone 0x3d
     * until the cinematic ended. */
    g_sound.pending_id = -1;
    g_sound.pending = false;
    g_sound.current_id = id;
    sound_dispatch_gated_unlocked(id);
  } else {
    sound_queue_unlocked(id);
  }
  pthread_mutex_unlock(&g_sound.lock);
}

int sound_driver_song_id(void) {
  if (!g_sound.inited) {
    return -1;
  }
  pthread_mutex_lock(&g_sound.lock);
  const int id = g_sound.current_id;
  pthread_mutex_unlock(&g_sound.lock);
  return id;
}

/* Pick Music: DOS stores the chosen id in DS:0x96 and plays it immediately (FUN_281f_04c0). */
void sound_play_preview(int id) {
  if (!g_sound.inited) {
    return;
  }
  if (id == 0 || id == 1) {
    sound_stop_preview();
    return;
  }
  if (id < SOUND_BGM_ID_BASE) {
    return;
  }
  if (!sound_gsound_has_song(id)) {
    diag_warn("sound: preview missing song id 0x%02x", id);
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  g_sound.preview_active = true;
  g_sound.pending_id = -1;
  g_sound.pending = false;
  g_sound.current_id = id;
  if (g_sound.vm) {
    gsound_vm_play(g_sound.vm, id);
  }
  pthread_mutex_unlock(&g_sound.lock);
}

/* Closing Pick Music keeps the chosen tune playing as BGM, like DOS. */
void sound_stop_preview(void) {
  if (!g_sound.inited) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  g_sound.preview_active = false;
  pthread_mutex_unlock(&g_sound.lock);
}

/* FUN_129f_0318: request a tune pool; a change fades the current song. */
void sound_set_bgm(int track) {
  if (!g_sound.inited || !COLONIZE_SOUND_PLAYBACK_ENABLED) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  if (track <= 0) {
    g_sound.category = 0;
    g_sound.category_next = 0;
    g_sound.category_applied = 0;
    g_sound.pending_id = -1;
    g_sound.pending = false;
    g_sound.current_id = -1;
    if (!g_sound.preview_active) {
      sound_dispatch_gated_unlocked(1);
    }
    pthread_mutex_unlock(&g_sound.lock);
    return;
  }
  g_sound.category = track;
  if (g_sound.category_applied != track) {
    g_sound.pending = true;
    sound_dispatch_gated_unlocked(1);
  }
  pthread_mutex_unlock(&g_sound.lock);
}

void sound_stop_bgm(void) {
  sound_set_bgm(0);
}

int sound_active_song_id(void) {
  if (!g_sound.inited) {
    return -1;
  }
  pthread_mutex_lock(&g_sound.lock);
  /* A queued explicit id already counts for FUN_129f_02cc's "unchanged" gate. */
  const int id = g_sound.pending_id >= 0 ? g_sound.pending_id : g_sound.current_id;
  pthread_mutex_unlock(&g_sound.lock);
  return id;
}

void sound_service(void) {
  if (!g_sound.inited) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  sound_pump_unlocked();
  pthread_mutex_unlock(&g_sound.lock);
}

/* ---- rendering ---------------------------------------------------------- */

static void sound_render_frames_unlocked(int16_t* dst, int frames, int channels) {
#if defined(COLONIZE_HAS_FLUIDSYNTH)
  if (g_sound.fluid_synth) {
    if (channels == 2) {
      fluid_synth_write_s16(g_sound.fluid_synth, frames, dst, 0, 2, dst, 1, 2);
    } else {
      int16_t tmp[2 * 256];
      int done = 0;
      while (done < frames) {
        const int n = (frames - done) > 256 ? 256 : (frames - done);
        fluid_synth_write_s16(g_sound.fluid_synth, n, tmp, 0, 2, tmp, 1, 2);
        for (int i = 0; i < n; ++i) {
          const int32_t m = ((int32_t)tmp[i * 2] + (int32_t)tmp[i * 2 + 1]) / 2;
          for (int c = 0; c < channels; ++c) {
            dst[(done + i) * channels + c] = (int16_t)m;
          }
        }
        done += n;
      }
    }
    return;
  }
#endif
  (void)channels;
  (void)dst;
  (void)frames;
}

void sound_render_s16(int16_t* dst, int frames, int channels, int sample_rate) {
  if (!dst || frames <= 0 || channels <= 0) {
    return;
  }
  memset(dst, 0, (size_t)frames * (size_t)channels * sizeof(int16_t));
  if (!g_sound.inited || !g_sound.enable_audio || sample_rate <= 0) {
    return;
  }

  pthread_mutex_lock(&g_sound.lock);
  /* When autoplay is compiled off, only Pick Music / offline preview may emit. */
  if (!COLONIZE_SOUND_PLAYBACK_ENABLED && !g_sound.preview_active) {
    pthread_mutex_unlock(&g_sound.lock);
    return;
  }

  const double samples_per_tick = (double)sample_rate / SOUND_TICK_HZ;
  int done = 0;
  while (done < frames) {
    if (g_sound.samples_to_tick <= 0.0) {
      if (g_sound.vm) {
        gsound_vm_tick(g_sound.vm);
        sound_pump_unlocked();
      }
      g_sound.samples_to_tick += samples_per_tick;
    }
    int n = (int)ceil(g_sound.samples_to_tick);
    if (n > frames - done) {
      n = frames - done;
    }
    if (n <= 0) {
      n = 1;
    }
#if defined(COLONIZE_HAS_FLUIDSYNTH)
    if (g_sound.fluid_synth) {
      sound_render_frames_unlocked(dst + (size_t)done * (size_t)channels, n, channels);
    } else
#endif
    {
      /* Square-wave fallback so music is still audible without FluidSynth. */
      for (int i = 0; i < n; ++i) {
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
          dst[(size_t)(done + i) * (size_t)channels + (size_t)c] = s;
        }
      }
    }
    g_sound.samples_to_tick -= (double)n;
    done += n;
  }
  if (g_sound.sfx_data) {
    sound_sfx_mix_unlocked(dst, frames, channels, sample_rate);
  }
  pthread_mutex_unlock(&g_sound.lock);
}

int sound_sfx_count(void) {
  return g_sound.inited ? g_sound.sfx_count : 0;
}

bool sound_sfx_sample(int index, const uint8_t** out_pcm, uint32_t* out_len, int* out_rate) {
  if (!g_sound.inited || !g_sound.sfx_data || index < 0 || index >= g_sound.sfx_count) {
    return false;
  }
  if (out_pcm) *out_pcm = g_sound.sfx_data + g_sound.sfx_off[index];
  if (out_len) *out_len = g_sound.sfx_len[index];
  if (out_rate) *out_rate = gsound_vm_sfx_rate(index);
  return true;
}

void sound_play_sfx(int index) {
  if (!g_sound.inited) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  sound_vm_sfx_cb(NULL, index);
  pthread_mutex_unlock(&g_sound.lock);
}

void sound_stop_sfx(void) {
  if (!g_sound.inited) {
    return;
  }
  pthread_mutex_lock(&g_sound.lock);
  sound_sfx_stop_all_unlocked();
  if (g_sound.vm) {
    gsound_vm_stop_events(g_sound.vm);
  }
#if defined(COLONIZE_HAS_FLUIDSYNTH)
  if (g_sound.fluid_synth) {
    /* GSOUND channels 7/8 → FluidSynth 0-based 6/7. */
    fluid_synth_all_notes_off(g_sound.fluid_synth, 6);
    fluid_synth_all_sounds_off(g_sound.fluid_synth, 6);
    fluid_synth_all_notes_off(g_sound.fluid_synth, 7);
    fluid_synth_all_sounds_off(g_sound.fluid_synth, 7);
  }
#endif
  pthread_mutex_unlock(&g_sound.lock);
}

/* ---- inspection API ----------------------------------------------------- */

int sound_gsound_song_count(void) {
  if (!g_sound.inited || !g_sound.vm) {
    return 0;
  }
  int n = 0;
  for (int id = SOUND_BGM_ID_BASE; id < 0x60; ++id) {
    if (gsound_vm_has_song(g_sound.vm, id)) {
      n++;
    }
  }
  return n;
}

bool sound_gsound_has_song(int id) {
  return g_sound.inited && g_sound.vm && id >= SOUND_BGM_ID_BASE && gsound_vm_has_song(g_sound.vm, id);
}

bool sound_gsound_song_stats(
  int id,
  int* out_events,
  uint32_t* out_duration_ticks,
  uint8_t* out_first_note,
  uint8_t* out_first_vel,
  uint8_t* out_first_program,
  uint8_t* out_first_channel
) {
  pthread_mutex_lock(&g_sound.lock);
  SoundSong* song = sound_decoded_song(id);
  if (!song) {
    pthread_mutex_unlock(&g_sound.lock);
    return false;
  }
  if (out_events) {
    *out_events = song->event_count;
  }
  if (out_duration_ticks) {
    *out_duration_ticks = song->duration_ticks;
  }
  uint8_t prog_by_ch[16];
  memset(prog_by_ch, 0, sizeof(prog_by_ch));
  bool found_note = false;
  for (int i = 0; i < song->event_count; ++i) {
    const SoundMidiEvent* e = &song->events[i];
    const uint8_t ch = e->status & 0x0f;
    const uint8_t kind = e->status & 0xf0;
    if (kind == 0xc0) {
      prog_by_ch[ch] = e->data1;
    }
    if (!found_note && kind == 0x90 && e->data2 > 0) {
      if (out_first_note) {
        *out_first_note = e->data1;
      }
      if (out_first_vel) {
        *out_first_vel = e->data2;
      }
      if (out_first_program) {
        *out_first_program = prog_by_ch[ch];
      }
      if (out_first_channel) {
        *out_first_channel = ch;
      }
      found_note = true;
    }
  }
  const bool ok = found_note || song->event_count > 0;
  pthread_mutex_unlock(&g_sound.lock);
  return ok;
}

bool sound_gsound_event_at(
  int id,
  int index,
  uint32_t* out_tick,
  uint8_t* out_status,
  uint8_t* out_data1,
  uint8_t* out_data2,
  uint8_t* out_channel
) {
  pthread_mutex_lock(&g_sound.lock);
  SoundSong* song = sound_decoded_song(id);
  if (!song || index < 0 || index >= song->event_count) {
    pthread_mutex_unlock(&g_sound.lock);
    return false;
  }
  const SoundMidiEvent* e = &song->events[index];
  if (out_tick) {
    *out_tick = e->tick;
  }
  if (out_status) {
    *out_status = (uint8_t)(e->status & 0xf0);
  }
  if (out_data1) {
    *out_data1 = e->data1;
  }
  if (out_data2) {
    *out_data2 = e->data2;
  }
  if (out_channel) {
    *out_channel = (uint8_t)(e->status & 0x0f);
  }
  pthread_mutex_unlock(&g_sound.lock);
  return true;
}

int sound_render_offline_mono(int song_id, int16_t* dst, int max_frames, int sample_rate) {
  if (!dst || max_frames <= 0 || !g_sound.inited) {
    return 0;
  }
  pthread_mutex_lock(&g_sound.lock);
  const bool prev_preview = g_sound.preview_active;
  const bool prev_enable = g_sound.enable_audio;
  g_sound.enable_audio = true; /* allow render path without an SDL device */
  g_sound.preview_active = true;
  g_sound.pending_id = -1;
  g_sound.pending = false;
  g_sound.current_id = song_id;
  if (g_sound.vm) {
    gsound_vm_play(g_sound.vm, song_id);
  }
  pthread_mutex_unlock(&g_sound.lock);

  const int chunk = 512;
  int written = 0;
  while (written + chunk <= max_frames) {
    sound_render_s16(dst + written, chunk, 1, sample_rate);
    written += chunk;
  }

  pthread_mutex_lock(&g_sound.lock);
  g_sound.current_id = -1;
  sound_dispatch_gated_unlocked(0);
  g_sound.preview_active = prev_preview;
  g_sound.enable_audio = prev_enable;
  pthread_mutex_unlock(&g_sound.lock);
  return written;
}
