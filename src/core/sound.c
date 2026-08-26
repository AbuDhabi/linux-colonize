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
#define SOUND_MAX_EVENTS 16384
#define SOUND_MAX_SONGS 96
#define SOUND_GSOUND_DS_PARAS 0x0322
#define SOUND_GSOUND_BGM_TABLE 0x2A6E
#define SOUND_GSOUND_EVENT_TABLE 0x2AC4
#define SOUND_GSOUND_BGM_BOUND 0x331A /* image alias of DS:00FA (= 0x3f) */
#define SOUND_GSOUND_DS_EVENT_MAX 0x00FC /* FUN_1000_19bc event id ceiling */
#define SOUND_GSOUND_IMG_HDR 512
#define SOUND_ED_MAX_NOTES 4 /* driver chord slots per voice */
/*
 * GSOUND.COL stores PIT divisor 0x4DBF at DS:0081 → ~60 Hz voice ticks
 * (1193182 / 19903 ≈ 59.95 Hz). Duration bytes are counts of these ticks.
 * IRQ path calls the voice interpreter every PIT tick; BE/BF product is
 * written to unread BSS (see annotated gsound notes) — not a tick scaler.
 */
#define SOUND_PIT_DIVISOR 0x4DBF
#define SOUND_TICK_HZ (1193182.0 / (double)SOUND_PIT_DIVISOR)
#define SOUND_TICK_SECONDS (1.0 / SOUND_TICK_HZ)
#define SOUND_MAX_TRACK_TICKS 14400u /* 4 minutes @ ~60 Hz */
#define SOUND_MAX_CALL_DEPTH 8
#define SOUND_MAX_LOOP_ITERS 64

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
  bool gsound_ok;
  bool backend_ok;
  ColonizeSoundOptions opts;

  uint8_t* gsound_img;
  size_t gsound_img_size;
  uint32_t ds_base;

  SoundSong songs[SOUND_MAX_SONGS]; /* BGM 0x20.. + event 0x40.. */
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

static uint8_t sound_note_gate(uint8_t dur, uint8_t artic_abs, uint8_t artic_sub) {
  uint8_t gate;
  if (artic_abs != 0) {
    gate = artic_abs;
  } else {
    gate = (uint8_t)(dur - artic_sub);
  }
  if (dur != 0 && gate > dur) {
    gate = dur;
  }
  return gate;
}

static void sound_emit_note(
  SoundSong* song,
  uint32_t time,
  uint8_t channel,
  int midi_note,
  uint8_t velocity,
  uint8_t dur,
  uint8_t gate
) {
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
}

/*
 * Decode one GSOUND voice stream (DS-relative). Opcode semantics from
 * FUN_1000_01fd in original_sources_decompiled/gsound.c (annotated under
 * original_sources_annotated/sound/). Bytes <= 0xBA are note/duration pairs;
 * F8 is program change (C2 is CC 91 reverb; C3 is hardware patch — unused in songs).
 */

typedef struct {
  uint32_t wait_ticks;
  size_t pos;
  uint8_t velocity;
  uint8_t artic_sub;
  uint8_t artic_abs;
  uint8_t transpose;
  uint8_t volume;
  int8_t vol_delta;
  uint8_t vol_period;
  uint8_t vol_count;
  size_t loop_start;
  int loop_count;
  size_t nest_start;
  int nest_count;
  size_t loop0_start;
  size_t loop0_target;
  size_t call_stack[SOUND_MAX_CALL_DEPTH];
  int call_depth;
  bool active;
  bool play_notes;
  uint8_t channel;
  int stuck;
} SoundTrackState;

static void sound_decode_tracks(
  SoundSong* song, uint8_t* ds_img, size_t ds_size, uint16_t* track_offs, int track_count
) {
  if (!song || !ds_img || ds_size == 0 || track_count == 0) return;

  SoundTrackState tracks[SOUND_MAX_TRACKS];
  memset(tracks, 0, sizeof(tracks));
  for (int t = 0; t < track_count; ++t) {
    tracks[t].pos = track_offs[t];
    tracks[t].active = true;
    tracks[t].play_notes = true;
    tracks[t].channel = t & 0x0f;
    tracks[t].velocity = 64;
    tracks[t].volume = 100;
    tracks[t].loop_start = track_offs[t];
    tracks[t].nest_start = track_offs[t];
  }

  uint32_t time = 0;
  uint8_t regs[64];
  memset(regs, 0, sizeof(regs));

  while (time < SOUND_MAX_TRACK_TICKS && song->event_count < SOUND_MAX_EVENTS - 8) {
    uint32_t min_wait = 0xFFFFFFFF;
    int active_count = 0;
    for (int t = 0; t < track_count; ++t) {
      if (tracks[t].active) {
        active_count++;
        if (tracks[t].wait_ticks < min_wait) min_wait = tracks[t].wait_ticks;
      }
    }
    if (active_count == 0) break;

    if (min_wait > 0 && min_wait != 0xFFFFFFFF) {
      for (uint32_t tick = 0; tick < min_wait; ++tick) {
        time++;
        for (int t = 0; t < track_count; ++t) {
          if (tracks[t].active && tracks[t].vol_delta != 0) {
            tracks[t].vol_count++;
            if (tracks[t].vol_count >= tracks[t].vol_period) {
              tracks[t].vol_count = 0;
              int new_vol = (int)tracks[t].volume + tracks[t].vol_delta;
              if (new_vol < 0) new_vol = 0;
              if (new_vol > 127) { new_vol = 127; tracks[t].vol_delta = 0; }
              if (new_vol == 0) { tracks[t].vol_delta = 0; }
              tracks[t].volume = (uint8_t)new_vol;
              sound_push_event(song, time, 0xb0, 7, tracks[t].volume, tracks[t].channel);
            }
          }
        }
      }
      for (int t = 0; t < track_count; ++t) {
        if (tracks[t].active) tracks[t].wait_ticks -= min_wait;
      }
    }

    bool any_zero_delay = false;
    for (int current_t = 0; current_t < track_count; ++current_t) {
      if (tracks[current_t].active && tracks[current_t].wait_ticks == 0) {
        any_zero_delay = true;
        SoundTrackState* trk = &tracks[current_t];

#define pos (trk->pos)
#define velocity (trk->velocity)
#define artic_sub (trk->artic_sub)
#define artic_abs (trk->artic_abs)
#define transpose (trk->transpose)
#define volume (trk->volume)
#define vol_delta (trk->vol_delta)
#define vol_period (trk->vol_period)
#define vol_count (trk->vol_count)
#define loop_start (trk->loop_start)
#define loop_count (trk->loop_count)
#define nest_start (trk->nest_start)
#define nest_count (trk->nest_count)
#define call_stack (trk->call_stack)
#define call_depth (trk->call_depth)
#define channel (trk->channel)
#define play_notes (trk->play_notes)
#define stuck (trk->stuck)

        uint32_t op_dur = 0;
        if (pos >= ds_size) { trk->active = false; break; }
    if (pos >= ds_size) {
      trk->active = false; break;
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
        trk->active = false; break;
      }

      const uint8_t gate = sound_note_gate(dur, artic_abs, artic_sub);
      if (note_raw == 0 || !play_notes) {
        op_dur = dur ? (uint32_t)dur : 1u;
      } else {
        sound_emit_note(
          song, time, channel, (int)note_raw + (int8_t)transpose, velocity, dur, gate
        );
        op_dur = dur ? (uint32_t)dur : 1u;
      }
      stuck = 0;
    }

    /* Opcode 0xBB..0xFF — FUN_1000_01fd */
    switch (op) {
      case 0xF4: /* velocity → voice+6 */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        velocity = ds_img[pos + 1];
        pos += 2;
        break;
      case 0xF8: /* program change */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        sound_push_event(song, time, 0xc0, ds_img[pos + 1] & 0x7f, 0, channel);
        pos += 2;
        break;
      case 0xC3: /* FUN_1000_01bf → hardware patch queue; not in song streams */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        pos += 2;
        break;
      case 0xC4: {
        /*
         * "Call [imm16]" through a data-embedded code pointer (FUN_1000_01d6
         * → indirect call). Ghidra never resolved the callee (only reached
         * via this trick), so it shows as raw bytes in gsound.asm. Verified
         * by hand with ndisasm at image offset 0x2c66 (the only callee seen
         * across the A/B corpus): mov bx,0x3532; call <PRNG>; and ax,4; jz;
         * xchg bl,bh; then poke BL/BH into 4 fixed image offsets that are
         * themselves the note bytes of the instructions right after this
         * one — i.e. "pick 0x32 or 0x35 at random, patch it into the notes
         * about to be played" (a drone/trill flourish), not code we can run.
         * Only fire on that exact, verified byte signature; anything else
         * at the callee address falls through to the old safe no-op skip.
         */
        if (pos + 2 < ds_size) {
          /* The 2-byte operand is a CS-relative code address (image offset,
           * not DS-relative like the track stream) — the callee lives
           * outside the DS window the interpreter otherwise reads through. */
          const uint16_t target = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
          const uint8_t* img_full = ds_img - g_sound.ds_base;
          if ((size_t)target + 2 < g_sound.gsound_img_size && img_full[target] == 0xBB &&
              img_full[target + 1] == 0x32 && img_full[target + 2] == 0x35) {
            uint8_t lo = 0x32, hi = 0x35;
            if (rand() & 4) {
              const uint8_t t = lo;
              lo = hi;
              hi = t;
            }
            static const uint16_t k_pokes_lo[] = {0x2d47, 0x2d76};
            static const uint16_t k_pokes_hi[] = {0x2d72, 0x2d4b};
            for (size_t i = 0; i < 2; ++i) {
              if (k_pokes_lo[i] < ds_size) ds_img[k_pokes_lo[i]] = lo;
              if (k_pokes_hi[i] < ds_size) ds_img[k_pokes_hi[i]] = hi;
            }
          }
        }
        pos += 3;
        break;
      }
      case 0xC5: /* reg[a] <= reg[b] ? skip : jump */
      case 0xC6:
      case 0xC7:
      case 0xC8:
      case 0xC9:
      case 0xCA:
      case 0xCB:
      case 0xCC:
      case 0xCD:
      case 0xCE:
      case 0xCF:
      case 0xD0:
      case 0xD1:
      case 0xD2:
      case 0xD3:
      case 0xD4: {
        /* 5-byte cond jump: op, a, b|imm, tgt_lo, tgt_hi (FUN_1000_01fd). */
        if (pos + 4 >= ds_size) {
          trk->active = false; break;
        }
        const uint8_t a = ds_img[pos + 1] & 63;
        const uint8_t b = ds_img[pos + 2];
        const uint8_t ra = regs[a];
        const uint8_t rb = (op <= 0xc8 || (op >= 0xcd && op <= 0xd0)) ? regs[b & 63] : b;
        bool take = false;
        switch (op) {
          case 0xc5: take = !(ra < rb || ra == rb); break; /* ja */
          case 0xc6: take = ra < rb; break;                /* jb */
          case 0xc7: take = ra != rb; break;               /* jne */
          case 0xc8: take = ra == rb; break;               /* je */
          case 0xc9: take = !(ra < rb || ra == rb); break;
          case 0xca: take = ra < rb; break;
          case 0xcb: take = ra != rb; break;
          case 0xcc: take = ra == rb; break;
          case 0xcd: take = !(ra < rb || ra == rb); break;
          case 0xce: take = ra < rb; break;
          case 0xcf: take = ra != rb; break;
          case 0xd0: take = ra == rb; break;
          case 0xd1: take = !(ra < rb || ra == rb); break;
          case 0xd2: take = ra < rb; break;
          case 0xd3: take = ra != rb; break;
          case 0xd4: take = ra == rb; break;
          default: break;
        }
        if (take) {
          if (call_depth < SOUND_MAX_CALL_DEPTH) {
            call_stack[call_depth++] = pos + 5; /* return after insn (driver +0x22) */
          }
          pos = (size_t)(ds_img[pos + 3] | ((uint16_t)ds_img[pos + 4] << 8));
        } else {
          pos += 5;
        }
        break;
      }
      case 0xD5: /* reg[a] ^= reg[b] */
      case 0xD6: /* reg[a] ^= imm */
      case 0xD7:
      case 0xD8:
      case 0xD9:
      case 0xDA:
      case 0xDB:
      case 0xDC:
      case 0xDD:
      case 0xDE:
      case 0xDF:
      case 0xE0:
      case 0xE1:
      case 0xE2:
      case 0xE3:
      case 0xE4:
      case 0xE7:
      case 0xE8:
      case 0xE9: {
        if (pos + 2 >= ds_size) {
          trk->active = false; break;
        }
        const uint8_t a = ds_img[pos + 1] & 63;
        const uint8_t b = ds_img[pos + 2];
        const uint8_t rb = (op == 0xd5 || op == 0xd7 || op == 0xd9 || op == 0xdb || op == 0xdd ||
                            op == 0xdf || op == 0xe1 || op == 0xe3 || op == 0xe8)
                             ? regs[b & 63]
                             : b;
        switch (op) {
          case 0xd5:
          case 0xd6: regs[a] = (uint8_t)(regs[a] ^ rb); break;
          case 0xd7:
          case 0xd8: regs[a] = (uint8_t)(regs[a] | rb); break;
          case 0xd9:
          case 0xda: regs[a] = (uint8_t)(regs[a] & rb); break;
          case 0xdb:
          case 0xdc: regs[a] = rb ? (uint8_t)(regs[a] % rb) : 0; break;
          case 0xdd:
          case 0xde: regs[a] = rb ? (uint8_t)(regs[a] / rb) : 0; break;
          case 0xdf:
          case 0xe0: regs[a] = (uint8_t)(regs[a] * rb); break;
          case 0xe1:
          case 0xe2: regs[a] = (uint8_t)(regs[a] - rb); break;
          case 0xe3:
          case 0xe4: regs[a] = (uint8_t)(regs[a] + rb); break;
          case 0xe8: regs[a] = rb; break;
          case 0xe9: regs[a] = b; break;
          case 0xe7: /* stream poke — size-only */ break;
          default: break;
        }
        pos += 3;
        break;
      }
      case 0xE5: /* reg[a]-- */
      case 0xE6: /* reg[a]++ */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        {
          const uint8_t a = ds_img[pos + 1] & 63;
          if (op == 0xe5) {
            regs[a]--;
          } else {
            regs[a]++;
          }
        }
        pos += 2;
        break;
      case 0xEA: /* indexed stream poke — 4 bytes */
      case 0xEB: /* random in [lo,hi] written ahead — 4 bytes */
        if (pos + 3 >= ds_size) {
          trk->active = false; break;
        }
        pos += 4;
        break;
      case 0xEC: { /* pick random of n bytes into stream; then duration */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        const uint8_t n = ds_img[pos + 1];
        if (pos + 2u + (size_t)n >= ds_size) {
          trk->active = false; break;
        }
        pos += 2u + (size_t)n + 1u;
        break;
      }
      case 0xF1: /* CC 7 volume */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        volume = ds_img[pos + 1] & 0x7f;
        sound_push_event(song, time, 0xb0, 7, volume, channel);
        pos += 2;
        break;
      case 0xF0: /* CC 10 pan */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        sound_push_event(song, time, 0xb0, 10, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xC2: /* CC 91 reverb */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        sound_push_event(song, time, 0xb0, 91, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xC1: /* CC 93 chorus */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        sound_push_event(song, time, 0xb0, 93, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xC0: /* CC 0 bank select */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        sound_push_event(song, time, 0xb0, 0, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xF2: /* pitch bend (high byte; low forced 0 like driver) */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        sound_push_event(song, time, 0xe0, 0, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xF6: /* absolute gate */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        artic_abs = ds_img[pos + 1];
        artic_sub = 0;
        pos += 2;
        break;
      case 0xF7: /* subtractive articulation */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        artic_sub = ds_img[pos + 1];
        artic_abs = 0;
        pos += 2;
        break;
      case 0xEE: /* per-voice transpose */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        transpose = ds_img[pos + 1];
        pos += 2;
        break;
      case 0xED: { /* chord: ED n note×n dur — up to 4 slots (FUN_1000_01fd) */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        const uint8_t n_raw = ds_img[pos + 1];
        const uint8_t n_play = n_raw > SOUND_ED_MAX_NOTES ? SOUND_ED_MAX_NOTES : n_raw;
        if (pos + 2u + (size_t)n_raw >= ds_size) {
          trk->active = false; break;
        }
        const uint8_t dur = ds_img[pos + 2u + (size_t)n_raw];
        const uint8_t gate = sound_note_gate(dur, artic_abs, artic_sub);
        for (uint8_t i = 0; i < n_play; ++i) {
          const uint8_t note_raw = ds_img[pos + 2u + (size_t)i];
          sound_emit_note(
            song, time, channel, (int)note_raw + (int8_t)transpose, velocity, dur, gate
          );
        }
        pos += 3u + (size_t)n_raw;
        op_dur = dur ? (uint32_t)dur : 1u;
        break;
      }
      case 0xBB: /* RPN pitch-bend range: CC101=0, CC100=0, CC6=n */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        sound_push_event(song, time, 0xb0, 101, 0, channel);
        sound_push_event(song, time, 0xb0, 100, 0, channel);
        sound_push_event(song, time, 0xb0, 6, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xF3: /* volume envelope: period, delta */
        if (pos + 2 >= ds_size) {
          trk->active = false; break;
        }
        vol_period = ds_img[pos + 1];
        vol_delta = (int8_t)ds_img[pos + 2];
        vol_count = 1; /* fire on next tick, matching driver init of +0xa = 1 */
        pos += 3;
        break;
      case 0xBF: /* master scale factor → unread product with BE (no tick effect) */
      case 0xBC: /* sets DS:0x50 countdown seed; stream-skip only */
      case 0xBD: /* sets DS:0x52; stream-skip only */
        pos += 2;
        break;
      case 0xBE: /* tempo pair → unread BSS product; IRQ still 60 Hz */
        if (pos + 2 >= ds_size) {
          trk->active = false; break;
        }
        pos += 3;
        break;
      case 0xEF: /* pan envelope (unused in BGM corpus) */
        if (pos + 2 >= ds_size) {
          trk->active = false; break;
        }
        pos += 3;
        break;
      case 0xF5: /* pitch envelope (rare) */
        if (pos + 3 >= ds_size) {
          trk->active = false; break;
        }
        pos += 4;
        break;
      case 0xFC: { /* absolute loop/stream jump */
        if (pos + 2 >= ds_size) {
          trk->active = false; break;
        }
        const uint16_t abs = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
        trk->loop0_start = abs;
        loop_start = abs;
        nest_start = abs;
        trk->loop0_target = abs;
        pos = abs;
        break;
      }
      case 0xFB: { /* jump absolute */
        if (pos + 2 >= ds_size) {
          trk->active = false; break;
        }
        const uint16_t abs = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
        pos = abs;
        break;
      }
      case 0xFA: { /* call absolute */
        if (pos + 2 >= ds_size) {
          trk->active = false; break;
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
      case 0xFD: { /* loop 0 */
        if (trk->loop0_target == 0) {
          pos = trk->loop0_start;
        } else {
          trk->loop0_start = trk->loop0_target;
          pos = trk->loop0_target;
          loop_start = trk->loop0_target;
          nest_start = trk->loop0_target;
        }
        break;
      }
      case 0xFE: { /* loop 1 (nest) */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        const uint8_t count = ds_img[pos + 1];
        if (nest_count <= 0) {
          if (count == 0) {
            pos += 2;
            nest_start = pos;
            loop_start = pos;
            nest_count = 0;
            loop_count = 0;
          } else {
            nest_count = count;
            pos = nest_start;
            loop_start = pos;
          }
        } else {
          nest_count--;
          if (nest_count > 0) {
            pos = nest_start;
            loop_start = pos;
          } else {
            pos += 2;
            nest_start = pos;
            loop_start = pos;
          }
        }
        break;
      }
      case 0xFF: { /* loop 2 (loop) */
        if (pos + 1 >= ds_size) {
          trk->active = false; break;
        }
        const uint8_t count = ds_img[pos + 1];
        if (loop_count <= 0) {
          if (count == 0) {
            pos += 2;
            loop_start = pos;
            loop_count = 0;
          } else {
            loop_count = count;
            pos = loop_start;
          }
        } else {
          loop_count--;
          if (loop_count > 0) {
            pos = loop_start;
          } else {
            pos += 2;
            loop_start = pos;
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
        trk->active = false;
        break;
      }
    } else {
      stuck = 0;
    }

    if (op_dur > 0) {
      trk->wait_ticks += op_dur;
    }

#undef pos
#undef velocity
#undef artic_sub
#undef artic_abs
#undef transpose
#undef volume
#undef vol_delta
#undef vol_period
#undef vol_count
#undef loop_start
#undef loop_count
#undef nest_start
#undef nest_count
#undef call_stack
#undef call_depth
#undef channel
#undef play_notes
#undef stuck

      }
    }
    if (any_zero_delay) continue;
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
  /*
   * Some table entries (e.g. Fiddler's Dance 0x25) point at a warm-restart stub:
   *   cmp word [DS:E6], 0 / … / ret
   * The cold-start (tempo init + B9 track list) begins at the next byte after that
   * ret — same pattern as FUN_1000_19bc song setup.
   */
  uint32_t start = handler;
  if (handler + 5 <= img_size && img[handler] == 0x83 && img[handler + 1] == 0x3e &&
      img[handler + 2] == 0xe6 && img[handler + 3] == 0x00) {
    uint32_t p = handler;
    const uint32_t lim = handler + 64;
    while (p < lim && p < img_size && img[p] != 0xc3) {
      p++;
    }
    if (p < img_size && img[p] == 0xc3) {
      start = p + 1;
    }
  }

  uint32_t i = start;
  const uint32_t end = start + 120;
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
    if (img[i] == 0xc7 && i + 6 <= img_size) {
      /* mov word [imm16], imm16 — tempo seed on cold-start stubs */
      i += 6;
      continue;
    }
    if (img[i] == 0xe9) {
      break;
    }
    i++;
  }
}

static int sound_event_priority(uint8_t status) {
  /* At equal ticks: program/CC/pitch before note-off/note-on. */
  if (status == 0xc0) {
    return 0;
  }
  if (status == 0xb0) {
    return 1;
  }
  if (status == 0xe0) {
    return 2;
  }
  if (status == 0x80) {
    return 3;
  }
  return 4;
}

static int sound_event_cmp(const void* a, const void* b) {
  const SoundMidiEvent* ea = (const SoundMidiEvent*)a;
  const SoundMidiEvent* eb = (const SoundMidiEvent*)b;
  if (ea->tick < eb->tick) {
    return -1;
  }
  if (ea->tick > eb->tick) {
    return 1;
  }
  const int pri_a = sound_event_priority(ea->status);
  const int pri_b = sound_event_priority(eb->status);
  if (pri_a != pri_b) {
    return pri_a - pri_b;
  }
  if (ea->status < eb->status) {
    return -1;
  }
  if (ea->status > eb->status) {
    return 1;
  }
  return 0;
}

static void sound_finalize_song_events(SoundSong* song) {
  if (!song || song->event_count <= 1) {
    return;
  }
  /* Songs can exceed 10k events; O(n²) sorting made startup take several seconds. */
  qsort(song->events, (size_t)song->event_count, sizeof(song->events[0]), sound_event_cmp);
}

/* FUN_1000_19bc tables: BGM at 0x2A6E (ids 0x20..), event at 0x2AC4 (ids 0x40..). */
static void sound_load_id_table(
  uint8_t* img,
  size_t img_size,
  uint32_t table_off,
  int id_lo,
  int id_hi
) {
  uint8_t* ds_img = img + g_sound.ds_base;
  const size_t ds_size = img_size - g_sound.ds_base;
  for (int id = id_lo; id <= id_hi && g_sound.song_count < SOUND_MAX_SONGS; ++id) {
    const int idx = id - id_lo;
    const uint32_t entry = table_off + (uint32_t)idx * 2u;
    if (entry + 2 > img_size) {
      break;
    }
    const uint32_t handler = rd_u16(img + entry);
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
    sound_decode_tracks(song, ds_img, ds_size, tracks, track_count);
    if (song->event_count > 0) {
      sound_finalize_song_events(song);
      g_sound.song_count++;
    }
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

  uint8_t* img = g_sound.gsound_img;
  const size_t img_size = g_sound.gsound_img_size;
  uint16_t bgm_max = 0x3f;
  if (SOUND_GSOUND_BGM_BOUND + 2 <= img_size) {
    bgm_max = rd_u16(img + SOUND_GSOUND_BGM_BOUND);
  }
  uint16_t event_max = 0x5c;
  if (g_sound.ds_base + SOUND_GSOUND_DS_EVENT_MAX + 2 <= img_size) {
    event_max = rd_u16(img + g_sound.ds_base + SOUND_GSOUND_DS_EVENT_MAX);
  }

  g_sound.song_count = 0;
  sound_load_id_table(img, img_size, SOUND_GSOUND_BGM_TABLE, SOUND_BGM_ID_BASE, (int)bgm_max);
  sound_load_id_table(img, img_size, SOUND_GSOUND_EVENT_TABLE, SOUND_EVENT_ID_BASE, (int)event_max);

  diag_info(
    "sound: GSOUND.COL loaded songs=%d ds_base=0x%x img=%zu bgm_max=0x%x event_max=0x%x",
    g_sound.song_count,
    g_sound.ds_base,
    g_sound.gsound_img_size,
    bgm_max,
    event_max
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
  /* Open the device whenever the platform allows so title/map BGM and Pick Music work. */
  g_sound.enable_audio = enable_audio;
  g_sound.opts.background_music = true;
  g_sound.opts.event_music = true;
  g_sound.opts.sound_effects = true;
  g_sound.active_song_id = -1;
  g_sound.bgm_track = 0;
  g_sound.bgm_song_id = -1;
  g_sound.preview_active = false;
  g_sound.ticks_per_sample = 1.0 / (SOUND_TICK_SECONDS * 44100.0);

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
  if (id >= SOUND_BGM_ID_BASE && sound_gsound_has_song(id)) {
    pthread_mutex_lock(&g_sound.lock);
    g_sound.preview_active = false;
    sound_start_song_unlocked(id);
    pthread_mutex_unlock(&g_sound.lock);
  }
}

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

int sound_active_song_id(void) {
  if (!g_sound.inited) {
    return -1;
  }
  pthread_mutex_lock(&g_sound.lock);
  /* Pending sound_set_bgm() not yet applied by sound_service() still counts
   * as "active" for id-unchanged gating, matching DOS's pending/current pair
   * (FUN_129f_0300 stores DS:0x9a before the idle pump applies it). */
  const int id = g_sound.need_restart ? g_sound.bgm_song_id : g_sound.active_song_id;
  pthread_mutex_unlock(&g_sound.lock);
  return id;
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
  /* When autoplay is compiled off, only Pick Music / offline preview may emit. */
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

bool sound_gsound_event_at(
  int id,
  int index,
  uint32_t* out_tick,
  uint8_t* out_status,
  uint8_t* out_data1,
  uint8_t* out_data2,
  uint8_t* out_channel
) {
  SoundSong* song = sound_find_song(id);
  if (!song || index < 0 || index >= song->event_count) {
    return false;
  }
  const SoundMidiEvent* e = &song->events[index];
  if (out_tick) {
    *out_tick = e->tick;
  }
  if (out_status) {
    *out_status = e->status;
  }
  if (out_data1) {
    *out_data1 = e->data1;
  }
  if (out_data2) {
    *out_data2 = e->data2;
  }
  if (out_channel) {
    *out_channel = e->channel;
  }
  return true;
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
