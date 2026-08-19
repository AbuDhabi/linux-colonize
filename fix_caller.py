with open('src/core/sound.c', 'r') as f:
    text = f.read()

old_loop = """    for (int t = 0; t < track_count; ++t) {
      if ((size_t)tracks[t] >= ds_size) {
        continue;
      }
      sound_decode_track(song, ds_img, ds_size, tracks[t], (uint8_t)(t & 0x0f));
    }"""
new_loop = """    sound_decode_tracks(song, ds_img, ds_size, tracks, track_count);"""

text = text.replace(old_loop, new_loop)

with open('src/core/sound.c', 'w') as f:
    f.write(text)
