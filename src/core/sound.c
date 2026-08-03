#include "core/sound.h"

#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/sound_opl.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

#if defined(COLONIZE_HAS_FLUIDSYNTH)
#include <fluidsynth.h>
#endif

#define SOUND_MAX_TRACKS 12
#define SOUND_MAX_EVENTS 16384
#define SOUND_GSOUND_DS_PARAS 0x0322
#define SOUND_GSOUND_BGM_TABLE 0x2A6E
#define SOUND_GSOUND_BGM_BOUND 0x331A
#define SOUND_ASOUND_DS_PARAS 0x03c0
#define SOUND_ASOUND_BGM_TABLE 0x1B6B
#define SOUND_ASOUND_BGM_MAX 0x3f
#define SOUND_MZ_IMG_HDR 512
/*
 * GSOUND.COL stores PIT divisor 0x4DBF at DS:0081 → ~60 Hz voice ticks
 * (1193182 / 19903 ≈ 59.95 Hz). Duration bytes are counts of these ticks.
 * ASOUND uses the same duration scale for its parallel arrangements.
 */
#define SOUND_PIT_DIVISOR 0x4DBF
#define SOUND_TICK_HZ (1193182.0 / (double)SOUND_PIT_DIVISOR)
#define SOUND_TICK_SECONDS (1.0 / SOUND_TICK_HZ)
#define SOUND_MAX_TRACK_TICKS 14400u /* 4 minutes @ ~60 Hz */
#define SOUND_MAX_CALL_DEPTH 8
#define SOUND_MAX_LOOP_ITERS 64

static SoundDriver g_driver_override = SOUND_DRIVER_G;
static bool g_driver_override_set = false;

typedef struct SoundMidiEvent {
  uint32_t tick;
  uint8_t status; /* 0x80/0x90/0xB0/0xC0/0xE0 */
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
  bool songs_ok;
  bool backend_ok;
  SoundDriver driver;
  ColonizeSoundOptions opts;

  uint8_t* driver_img;
  size_t driver_img_size;
  uint32_t ds_base;

  SoundSong songs[32]; /* ids 0x20..0x3f */
  int song_count;

  pthread_mutex_t lock;
  int active_song_id; /* -1 = none */
  int bgm_track;      /* requested DOS track number; 0 = none */
  int bgm_song_id;
  bool need_restart;
  bool preview_active; /* Pick Music preview; independent of map BGM */
  uint32_t play_tick;
  double tick_accum; /* samples → ticks */
  double ticks_per_sample;
  double tick_scale; /* ASOUND C0 a/b → b/a; default 1.0 */
  uint8_t tempo_a;
  uint8_t tempo_b;
  int program;

#if defined(COLONIZE_HAS_FLUIDSYNTH)
  fluid_settings_t* fluid_settings;
  fluid_synth_t* fluid_synth;
  int fluid_sfont_id;
#endif
  /* Soft fallback oscillator when FluidSynth is unavailable (G only). */
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

/*
 * Decode one voice stream (DS-relative). Opcodes reverse-engineered from
 * GSOUND MZ jump table at image 0xEF2 (ops 0xBB..0xFF). ASOUND uses the same
 * note/dur/F4/F8/FA family; tempo setup is C0 (3 bytes) / C1 (2 bytes) instead
 * of BE / BF. Bytes <= 0xBA are note/duration pairs.
 */
static void sound_decode_track(
  SoundSong* song,
  const uint8_t* ds_img,
  size_t ds_size,
  uint16_t start_off,
  uint8_t channel,
  bool asound_mode
) {
  if (!song || !ds_img || ds_size == 0 || (size_t)start_off >= ds_size) {
    return;
  }

  uint32_t time = 0;
  size_t pos = start_off;
  uint8_t velocity = 64;
  uint8_t artic_sub = 0; /* F7: gate = dur - artic_sub */
  uint8_t artic_abs = 0; /* F6: gate = artic_abs when nonzero */
  uint8_t transpose = 0; /* EE */

  size_t loop_start = start_off;
  int loop_count = 0;
  int nest_count = 0;
  size_t nest_start = start_off;
  size_t call_stack[SOUND_MAX_CALL_DEPTH];
  int call_depth = 0;
  int stuck = 0;

  while (time < SOUND_MAX_TRACK_TICKS && song->event_count < SOUND_MAX_EVENTS - 4) {
    if (pos >= ds_size) {
      break;
    }
    const size_t pos_before = pos;
    const uint8_t op = ds_img[pos];

    if (op <= 0xBA) {
      if (pos + 1 >= ds_size) {
        break;
      }
      const uint8_t note_raw = op;
      const uint8_t dur = ds_img[pos + 1];
      pos += 2;

      if (note_raw == 0 && dur == 0) {
        /* Terminal rest used at track ends — stop expanding. */
        break;
      }

      uint8_t gate;
      if (artic_abs != 0) {
        gate = artic_abs;
      } else {
        gate = (uint8_t)(dur - artic_sub);
      }
      if (dur != 0 && gate > dur) {
        gate = dur;
      }

      if (note_raw == 0) {
        time += dur ? (uint32_t)dur : 1u;
      } else {
        int midi_note = (int)note_raw + (int8_t)transpose;
        if (midi_note < 0) {
          midi_note = 0;
        }
        if (midi_note > 127) {
          midi_note = 127;
        }
        uint8_t vel = velocity;
        if (vel == 0 || vel > 127) {
          vel = 64;
        }
        sound_push_event(song, time, 0x90, (uint8_t)midi_note, vel, channel);
        const uint32_t off_at = time + (gate ? (uint32_t)gate : (uint32_t)(dur ? dur : 1u));
        sound_push_event(song, off_at, 0x80, (uint8_t)midi_note, 0, channel);
        time += dur ? (uint32_t)dur : 1u;
      }
      stuck = 0;
      continue;
    }

    /* Opcode 0xBB..0xFF */
    switch (op) {
      case 0xF4: /* velocity */
        if (pos + 1 >= ds_size) {
          return;
        }
        velocity = ds_img[pos + 1];
        pos += 2;
        break;
      case 0xF8: /* program change */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xc0, ds_img[pos + 1] & 0x7f, 0, channel);
        pos += 2;
        break;
      case 0xC3: /* far patch helper — treat as program change */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xc0, ds_img[pos + 1] & 0x7f, 0, channel);
        pos += 2;
        break;
      case 0xF1: /* CC 7 volume */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 7, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xF0: /* CC 10 pan */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 10, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xC2: /* CC 91 reverb (GSOUND) */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 91, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xC1:
        if (asound_mode) {
          /* ASOUND: master volume scale (handler ~0x1D86). */
          if (pos + 1 >= ds_size) {
            return;
          }
          sound_push_event(song, time, 0xb0, 0x70, ds_img[pos + 1], channel);
          pos += 2;
          break;
        }
        /* GSOUND: CC 93 chorus */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 93, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xC0:
        if (asound_mode) {
          /* ASOUND: tempo / clock scale (like GSOUND BE) — 3 bytes. */
          if (pos + 2 >= ds_size) {
            return;
          }
          sound_push_event(song, time, 0xb0, 0x71, ds_img[pos + 1], channel);
          sound_push_event(song, time, 0xb0, 0x72, ds_img[pos + 2], channel);
          pos += 3;
          break;
        }
        /* GSOUND: CC 0 bank select */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 0, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xF2: /* pitch bend (high byte; low forced 0 like driver) */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xe0, 0, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xF6: /* absolute gate */
        if (pos + 1 >= ds_size) {
          return;
        }
        artic_abs = ds_img[pos + 1];
        artic_sub = 0;
        pos += 2;
        break;
      case 0xF7: /* subtractive articulation */
        if (pos + 1 >= ds_size) {
          return;
        }
        artic_sub = ds_img[pos + 1];
        artic_abs = 0;
        pos += 2;
        break;
      case 0xEE: /* per-voice transpose */
        if (pos + 1 >= ds_size) {
          return;
        }
        transpose = ds_img[pos + 1];
        pos += 2;
        break;
      case 0xBF: /* master volume scale (driver-internal) */
      case 0xBC:
      case 0xBD:
      case 0xBB:
      case 0xC4:
      case 0xC5:
        pos += 2;
        break;
      case 0xBE: /* tempo / clock scale (driver-internal) */
        if (pos + 2 >= ds_size) {
          return;
        }
        pos += 3;
        break;
      case 0xF3: /* envelope params */
      case 0xEF:
        if (pos + 2 >= ds_size) {
          return;
        }
        pos += 3;
        break;
      case 0xF5:
        if (pos + 3 >= ds_size) {
          return;
        }
        pos += 4;
        break;
      case 0xFC: { /* set loop/stream anchors to absolute DS offset */
        if (pos + 2 >= ds_size) {
          return;
        }
        const uint16_t abs = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
        loop_start = abs;
        nest_start = abs;
        pos = abs;
        break;
      }
      case 0xFB: { /* jump absolute */
        if (pos + 2 >= ds_size) {
          return;
        }
        const uint16_t abs = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
        pos = abs;
        break;
      }
      case 0xFA: { /* call absolute */
        if (pos + 2 >= ds_size) {
          return;
        }
        const uint16_t abs = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
        const size_t ret = pos + 3;
        if (call_depth < SOUND_MAX_CALL_DEPTH) {
          call_stack[call_depth++] = ret;
          pos = abs;
        } else {
          pos = ret;
        }
        break;
      }
      case 0xF9: /* return from FA */
        if (call_depth > 0) {
          pos = call_stack[--call_depth];
        } else {
          pos += 1;
        }
        break;
      case 0xFD: /* jump to FC loop start */
        pos = loop_start;
        break;
      case 0xFF: { /* counted loop to loop_start */
        if (pos + 1 >= ds_size) {
          return;
        }
        const uint8_t count = ds_img[pos + 1];
        if (count == 0) {
          pos += 2;
          loop_start = pos;
        } else if (loop_count <= 0) {
          loop_count = count;
          if (loop_count > SOUND_MAX_LOOP_ITERS) {
            loop_count = SOUND_MAX_LOOP_ITERS;
          }
          pos = loop_start;
        } else {
          loop_count--;
          if (loop_count > 0) {
            pos = loop_start;
          } else {
            pos += 2;
          }
        }
        break;
      }
      case 0xFE: { /* nested counted loop */
        if (pos + 1 >= ds_size) {
          return;
        }
        const uint8_t count = ds_img[pos + 1];
        if (count == 0) {
          pos += 2;
          nest_start = pos;
        } else if (nest_count <= 0) {
          nest_count = count;
          if (nest_count > SOUND_MAX_LOOP_ITERS) {
            nest_count = SOUND_MAX_LOOP_ITERS;
          }
          pos = nest_start;
        } else {
          nest_count--;
          if (nest_count > 0) {
            pos = nest_start;
          } else {
            pos += 2;
          }
        }
        break;
      }
      default:
        /* Unknown high opcode: skip opcode + 1 data byte (common size). */
        pos += 2;
        break;
    }

    if (pos == pos_before) {
      if (++stuck > 8) {
        break;
      }
    } else {
      stuck = 0;
    }
  }
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

/* ASOUND handlers use `lea cx, [track]` (8D 0E xx xx) instead of `mov cx, imm`. */
static void sound_parse_handler_tracks_lea(
  const uint8_t* img,
  size_t img_size,
  uint32_t handler,
  uint16_t* out_offs,
  int* out_count
) {
  *out_count = 0;
  uint32_t i = handler;
  const uint32_t end = handler + 160;
  while (i + 4 <= end && i < img_size && *out_count < SOUND_MAX_TRACKS) {
    if (img[i] == 0xc3) {
      break;
    }
    if (img[i] == 0x8d && img[i + 1] == 0x0e) {
      out_offs[*out_count] = rd_u16(img + i + 2);
      (*out_count)++;
      i += 4;
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

static void sound_sort_song_events(SoundSong* song) {
  if (!song) {
    return;
  }
  for (int a = 0; a < song->event_count - 1; ++a) {
    for (int b = a + 1; b < song->event_count; ++b) {
      const SoundMidiEvent* ea = &song->events[a];
      const SoundMidiEvent* eb = &song->events[b];
      int pri_a = (ea->status == 0xc0)   ? 0
                  : (ea->status == 0xb0) ? 1
                  : (ea->status == 0xe0) ? 2
                  : (ea->status == 0x80) ? 3
                                         : 4;
      int pri_b = (eb->status == 0xc0)   ? 0
                  : (eb->status == 0xb0) ? 1
                  : (eb->status == 0xe0) ? 2
                  : (eb->status == 0x80) ? 3
                                         : 4;
      const bool swap = eb->tick < ea->tick ||
                        (eb->tick == ea->tick && (pri_b < pri_a || (pri_b == pri_a && eb->status < ea->status)));
      if (swap) {
        SoundMidiEvent tmp = song->events[a];
        song->events[a] = song->events[b];
        song->events[b] = tmp;
      }
    }
  }
}

static bool sound_load_mz_driver(const char* data_dir, const char* filename, uint8_t** out_img, size_t* out_size) {
  char path[512];
  if (!dos_compat_normalize_asset_path(data_dir, filename, path, sizeof(path))) {
    diag_warn("sound: cannot resolve %s", filename);
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
  if (sz < SOUND_MZ_IMG_HDR + 0x2000) {
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
  *out_size = (size_t)sz - hdr;
  *out_img = (uint8_t*)malloc(*out_size);
  if (!*out_img) {
    free(file);
    return false;
  }
  memcpy(*out_img, file + hdr, *out_size);
  free(file);
  return true;
}

static bool sound_load_gsound(const char* data_dir) {
  if (!sound_load_mz_driver(data_dir, "GSOUND.COL", &g_sound.driver_img, &g_sound.driver_img_size)) {
    return false;
  }

  g_sound.ds_base = (uint32_t)SOUND_GSOUND_DS_PARAS * 16u;
  if (g_sound.ds_base >= g_sound.driver_img_size) {
    free(g_sound.driver_img);
    g_sound.driver_img = NULL;
    return false;
  }

  const uint8_t* img = g_sound.driver_img;
  const size_t img_size = g_sound.driver_img_size;
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
    const uint8_t* ds_img = img + g_sound.ds_base;
    const size_t ds_size = img_size - g_sound.ds_base;
    for (int t = 0; t < track_count; ++t) {
      if ((size_t)tracks[t] >= ds_size) {
        continue;
      }
      sound_decode_track(song, ds_img, ds_size, tracks[t], (uint8_t)(t & 0x0f), false);
    }
    if (song->event_count > 0) {
      sound_sort_song_events(song);
      g_sound.song_count++;
    }
  }

  diag_info(
    "sound: GSOUND.COL loaded songs=%d ds_base=0x%x img=%zu",
    g_sound.song_count,
    g_sound.ds_base,
    g_sound.driver_img_size
  );
  return g_sound.song_count > 0;
}

static bool sound_load_asound(const char* data_dir) {
  if (!sound_load_mz_driver(data_dir, "ASOUND.COL", &g_sound.driver_img, &g_sound.driver_img_size)) {
    return false;
  }

  g_sound.ds_base = (uint32_t)SOUND_ASOUND_DS_PARAS * 16u;
  if (g_sound.ds_base >= g_sound.driver_img_size) {
    free(g_sound.driver_img);
    g_sound.driver_img = NULL;
    return false;
  }

  const uint8_t* img = g_sound.driver_img;
  const size_t img_size = g_sound.driver_img_size;
  const uint16_t bgm_max = SOUND_ASOUND_BGM_MAX;

  g_sound.song_count = 0;
  for (int id = SOUND_BGM_ID_BASE; id <= (int)bgm_max && id < SOUND_BGM_ID_BASE + 32; ++id) {
    const int idx = id - SOUND_BGM_ID_BASE;
    const uint32_t table_off = SOUND_ASOUND_BGM_TABLE + (uint32_t)idx * 2u;
    if (table_off + 2 > img_size) {
      break;
    }
    const uint32_t handler = rd_u16(img + table_off);
    if (handler == 0 || handler + 4 > img_size) {
      continue;
    }
    uint16_t tracks[SOUND_MAX_TRACKS];
    int track_count = 0;
    sound_parse_handler_tracks_lea(img, img_size, handler, tracks, &track_count);
    if (track_count <= 0) {
      continue;
    }

    SoundSong* song = &g_sound.songs[g_sound.song_count];
    memset(song, 0, sizeof(*song));
    song->id = id;
    const uint8_t* ds_img = img + g_sound.ds_base;
    const size_t ds_size = img_size - g_sound.ds_base;
    for (int t = 0; t < track_count; ++t) {
      if ((size_t)tracks[t] >= ds_size) {
        continue;
      }
      /* OPL melodic channels 0..8; clamp track index. */
      sound_decode_track(song, ds_img, ds_size, tracks[t], (uint8_t)(t % 9), true);
    }
    if (song->event_count > 0) {
      sound_sort_song_events(song);
      g_sound.song_count++;
    }
  }

  {
    const uint8_t* ds_img = img + g_sound.ds_base;
    const size_t ds_size = img_size - g_sound.ds_base;
    if (!sound_opl_load_tables(ds_img, ds_size)) {
      diag_warn("sound: ASOUND instrument/fnum tables missing or truncated");
    } else {
      diag_info("sound: ASOUND bank+fnum+vel curve loaded from DS");
    }
  }

  diag_info(
    "sound: ASOUND.COL loaded songs=%d ds_base=0x%x img=%zu",
    g_sound.song_count,
    g_sound.ds_base,
    g_sound.driver_img_size
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
  if (g_sound.driver == SOUND_DRIVER_A) {
    sound_opl_all_notes_off();
  }
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
  if (g_sound.driver == SOUND_DRIVER_A) {
    const int ch = e->channel % 9;
    if (e->status == 0x90 && e->data2 > 0) {
      sound_opl_note_on(ch, e->data1, e->data2);
    } else if (e->status == 0x80 || (e->status == 0x90 && e->data2 == 0)) {
      sound_opl_note_off(ch, e->data1);
    } else if (e->status == 0xc0) {
      sound_opl_program(ch, e->data1);
      g_sound.program = e->data1;
    } else if (e->status == 0xb0 && e->data1 == 7) {
      sound_opl_volume(ch, e->data2);
    } else if (e->status == 0xb0 && e->data1 == 0x70) {
      sound_opl_master_volume(e->data2);
    } else if (e->status == 0xb0 && e->data1 == 0x71) {
      g_sound.tempo_a = e->data2;
      if (g_sound.tempo_a > 0 && g_sound.tempo_b > 0) {
        g_sound.tick_scale = (double)g_sound.tempo_b / (double)g_sound.tempo_a;
      }
    } else if (e->status == 0xb0 && e->data1 == 0x72) {
      g_sound.tempo_b = e->data2;
      if (g_sound.tempo_a > 0 && g_sound.tempo_b > 0) {
        g_sound.tick_scale = (double)g_sound.tempo_b / (double)g_sound.tempo_a;
      }
    }
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
    } else if (e->status == 0xb0) {
      fluid_synth_cc(g_sound.fluid_synth, ch, e->data1, e->data2);
    } else if (e->status == 0xe0) {
      fluid_synth_pitch_bend(g_sound.fluid_synth, ch, ((int)e->data2 << 7) | (int)e->data1);
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

static SoundDriver sound_resolve_driver(void) {
  if (g_driver_override_set) {
    return g_driver_override;
  }
  const char* env = getenv("COLONIZE_SOUND_DRIVER");
  if (env && env[0]) {
    SoundDriver d = SOUND_DRIVER_G;
    if (sound_parse_driver(env, &d)) {
      return d;
    }
    diag_warn("sound: ignoring invalid COLONIZE_SOUND_DRIVER=%s (use A or G)", env);
  }
  return SOUND_DRIVER_G;
}

bool sound_parse_driver(const char* s, SoundDriver* out) {
  if (!s || !s[0] || !out) {
    return false;
  }
  if (s[1] == '\0') {
    const char c = (char)toupper((unsigned char)s[0]);
    if (c == 'A') {
      *out = SOUND_DRIVER_A;
      return true;
    }
    if (c == 'G') {
      *out = SOUND_DRIVER_G;
      return true;
    }
    return false;
  }
  if (strcasecmp(s, "adlib") == 0 || strcasecmp(s, "opl") == 0 || strcasecmp(s, "sb") == 0) {
    *out = SOUND_DRIVER_A;
    return true;
  }
  if (strcasecmp(s, "gm") == 0 || strcasecmp(s, "midi") == 0 || strcasecmp(s, "roland") == 0) {
    *out = SOUND_DRIVER_G;
    return true;
  }
  return false;
}

void sound_configure_driver(SoundDriver driver) {
  g_driver_override = driver;
  g_driver_override_set = true;
}

SoundDriver sound_current_driver(void) {
  return g_sound.inited ? g_sound.driver : sound_resolve_driver();
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
  g_sound.enable_audio = enable_audio;
  g_sound.opts.background_music = true;
  g_sound.opts.event_music = true;
  g_sound.opts.sound_effects = true;
  g_sound.active_song_id = -1;
  g_sound.bgm_track = 0;
  g_sound.bgm_song_id = -1;
  g_sound.preview_active = false;
  g_sound.ticks_per_sample = 1.0 / (SOUND_TICK_SECONDS * 44100.0);
  g_sound.tick_scale = 1.0;
  g_sound.tempo_a = 4;
  g_sound.tempo_b = 4;
  g_sound.driver = sound_resolve_driver();

  const char* dir = data_dir ? data_dir : "./COLONIZE";
  if (g_sound.driver == SOUND_DRIVER_A) {
    g_sound.songs_ok = sound_load_asound(dir);
    diag_info("sound: driver=A (ASOUND / AdLib OPL)");
  } else {
    g_sound.songs_ok = sound_load_gsound(dir);
    diag_info("sound: driver=G (GSOUND / General MIDI)");
  }

  if (g_sound.driver == SOUND_DRIVER_A) {
    /* OPL is needed for offline render even without an SDL device. */
    g_sound.backend_ok = sound_opl_init(44100);
    if (!g_sound.backend_ok) {
      diag_warn("sound: OPL init failed");
    } else {
      diag_info("sound: Nuked-OPL3 ready");
    }
    if (!g_sound.enable_audio) {
      diag_info("sound: audio device disabled");
    }
  } else if (g_sound.enable_audio) {
    g_sound.backend_ok = sound_init_fluidsynth(dir);
    if (!g_sound.backend_ok) {
      diag_info("sound: using square-wave fallback renderer");
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
  if (g_sound.driver == SOUND_DRIVER_A) {
    sound_opl_shutdown();
  }
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
  free(g_sound.driver_img);
  g_sound.driver_img = NULL;
  pthread_mutex_unlock(&g_sound.lock);
  pthread_mutex_destroy(&g_sound.lock);
  memset(&g_sound, 0, sizeof(g_sound));
}

bool sound_ok(void) {
  return g_sound.inited && g_sound.songs_ok;
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
  g_sound.tick_scale = 1.0;
  g_sound.tempo_a = 4;
  g_sound.tempo_b = 4;
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
  const double scale = (g_sound.tick_scale > 0.0) ? g_sound.tick_scale : 1.0;
  const double tps = scale / (SOUND_TICK_SECONDS * (double)sample_rate);
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
  /* When autoplay is compiled off, only Pick Music / offline preview may emit. */
  if (!COLONIZE_SOUND_PLAYBACK_ENABLED && !g_sound.preview_active) {
    pthread_mutex_unlock(&g_sound.lock);
    return;
  }

  sound_advance_unlocked(frames, sample_rate);

  if (g_sound.driver == SOUND_DRIVER_A) {
    sound_opl_render_s16(dst, frames, channels);
    pthread_mutex_unlock(&g_sound.lock);
    return;
  }

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

bool sound_gsound_song_stats(
  int id,
  int* out_events,
  uint32_t* out_duration_ticks,
  uint8_t* out_first_note,
  uint8_t* out_first_vel,
  uint8_t* out_first_program,
  uint8_t* out_first_channel
) {
  SoundSong* song = sound_find_song(id);
  if (!song) {
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
    const uint8_t ch = e->channel & 0x0f;
    if (e->status == 0xc0) {
      prog_by_ch[ch] = e->data1;
    }
    if (!found_note && e->status == 0x90 && e->data2 > 0) {
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
  return found_note || song->event_count > 0;
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
