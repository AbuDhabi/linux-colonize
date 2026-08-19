with open('track.c', 'r') as f:
    lines = f.readlines()

body_start = -1
for i, line in enumerate(lines):
    if "while (time < SOUND_MAX_TRACK_TICKS" in line:
        body_start = i
        break

body = lines[body_start+1 : -2]
body_str = "".join(body)

import re
body_str = re.sub(r'\breturn;\b', 'trk->active = false; break;', body_str)
body_str = re.sub(r'sound_advance_time\([^,]+,\s*&time,\s*([^,]+),\s*[^;]+\);', r'op_dur = \1;', body_str)

new_func = """
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
  size_t call_stack[SOUND_MAX_CALL_DEPTH];
  int call_depth;
  bool active;
  uint8_t channel;
  int stuck;
} SoundTrackState;

static void sound_decode_tracks(
  SoundSong* song, const uint8_t* ds_img, size_t ds_size, uint16_t* track_offs, int track_count
) {
  if (!song || !ds_img || ds_size == 0 || track_count == 0) return;

  SoundTrackState tracks[SOUND_MAX_TRACKS];
  memset(tracks, 0, sizeof(tracks));
  for (int t = 0; t < track_count; ++t) {
    tracks[t].pos = track_offs[t];
    tracks[t].active = true;
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

    for (int current_t = 0; current_t < track_count; ++current_t) {
      while (tracks[current_t].active && tracks[current_t].wait_ticks == 0) {
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
#define stuck (trk->stuck)

        uint32_t op_dur = 0;
        if (pos >= ds_size) { trk->active = false; break; }
""" + body_str + """
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
#undef stuck

      }
    }
  }
}
"""

with open('src/core/sound.c', 'r') as f:
    text = f.read()
    
start_sig = "static void sound_decode_track("
start_idx = text.find(start_sig)
end_idx = text.find("static void sound_parse_handler_tracks(", start_idx)

text = text[:start_idx] + new_func + text[end_idx:]

caller_old = """      for (int t = 0; t < track_count; ++t) {
        sound_decode_track(song, img, img_size, track_offs[t], (uint8_t)(t & 0x0f));
      }"""
caller_new = """      sound_decode_tracks(song, img, img_size, track_offs, track_count);"""
text = text.replace(caller_old, caller_new)

with open('src/core/sound_new.c', 'w') as f:
    f.write(text)
