#ifndef COLONIZE_SOUND_H
#define COLONIZE_SOUND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * DOS-faithful sound/music facade (FUN_12d8_000e gating + FUN_129f_* BGM).
 *
 * Music data is loaded from GSOUND.COL (General MIDI MicroProse driver). Song
 * IDs 0x20..0x3f are background music; 0x40..0x5c are event music; IDs < 0x10
 * are always forwarded (stop / system).
 *
 * Streams are decoded from the GSOUND voice bytecode (note/dur pairs, ED chords,
 * F4 velocity, F8 program, F3 volume envelope, BB pitch-bend RPN, CC ops).
 * Synthesis uses FluidSynth with an SC-55-ish SoundFont when available
 * (COLONIZE_SOUNDFONT overrides).
 */
#define COLONIZE_SOUND_PLAYBACK_ENABLED 1

#define SOUND_BGM_ID_BASE 0x20
#define SOUND_EVENT_ID_BASE 0x40
#define SOUND_TITLE_ID 0x33
/*
 * Situational "Military" BGM cue (@PICKMUSIC Military sublist, first song).
 * DOS combat engagement code (segment 5fef) pushes this literal id into the
 * BGM-change path (FUN_281f_048e -> FUN_129f_02cc) when a land/naval attack
 * begins; the real driver only restarts playback when the id actually
 * changes (see sound_active_song_id), so callers should gate on that rather
 * than calling sound_play() unconditionally on every attack.
 */
#define SOUND_MILITARY_BGM_ID 0x32

/* True when ambient autoplay is allowed (not the Pick Music preview path). */
bool sound_playback_enabled(void);
/* True when the audio device/backend was opened (previews can be heard). */
bool sound_audio_output_ready(void);

typedef struct ColonizeSoundOptions {
  bool background_music;
  bool event_music;
  bool sound_effects;
} ColonizeSoundOptions;

bool sound_init(const char* data_dir, bool enable_audio);
void sound_shutdown(void);
bool sound_ok(void);
bool sound_backend_ok(void); /* FluidSynth + soundfont ready */

void sound_set_options(ColonizeSoundOptions opts);
ColonizeSoundOptions sound_get_options(void);

/* Mirror FUN_12d8_000e: gate by option bits then play. */
void sound_play(int id);

/*
 * Play a song for A/B testing from Pick Music even when autoplay is disabled.
 * Does not change the map BGM track. Stop with sound_stop_preview().
 */
void sound_play_preview(int id);
void sound_stop_preview(void);

/* BGM channel (FUN_129f_*): track 1..N maps to sound id SOUND_BGM_ID_BASE+track. */
void sound_set_bgm(int track);
void sound_stop_bgm(void);
void sound_service(void);

/* Currently playing song id (BGM/event/title), or -1 if none. Ambient BGM
 * (sound_set_bgm) and one-shot sound_play() share this state, matching DOS
 * FUN_129f_02cc/0318's "skip restart if id unchanged" gate. */
int sound_active_song_id(void);

/* Fill interleaved S16 samples for the SDL audio callback (thread-safe). */
void sound_render_s16(int16_t* dst, int frames, int channels, int sample_rate);

/* Test helpers: song count recovered from GSOUND.COL; decode one song to events. */
int sound_gsound_song_count(void);
bool sound_gsound_has_song(int id);
/*
 * Inspect a decoded song for golden tests.
 * out_first_* describe the first note-on (status 0x90) if any.
 */
bool sound_gsound_song_stats(
  int id,
  int* out_events,
  uint32_t* out_duration_ticks,
  uint8_t* out_first_note,
  uint8_t* out_first_vel,
  uint8_t* out_first_program,
  uint8_t* out_first_channel
);
/* Enumerate decoded events (index 0..events-1) for MIDI/export tools. */
bool sound_gsound_event_at(
  int id,
  int index,
  uint32_t* out_tick,
  uint8_t* out_status,
  uint8_t* out_data1,
  uint8_t* out_data2,
  uint8_t* out_channel
);
/* Render a short offline buffer (mono S16) for smoke tests; returns frames written. */
int sound_render_offline_mono(int song_id, int16_t* dst, int max_frames, int sample_rate);

#endif
