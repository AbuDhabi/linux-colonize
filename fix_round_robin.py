with open('src/core/sound.c', 'r') as f:
    text = f.read()

import re

# find the execution loop
old_exec = """    for (int current_t = 0; current_t < track_count; ++current_t) {
      while (tracks[current_t].active && tracks[current_t].wait_ticks == 0) {
        SoundTrackState* trk = &tracks[current_t];"""

new_exec = """    bool any_zero_delay = false;
    for (int current_t = 0; current_t < track_count; ++current_t) {
      if (tracks[current_t].active && tracks[current_t].wait_ticks == 0) {
        any_zero_delay = true;
        SoundTrackState* trk = &tracks[current_t];"""

text = text.replace(old_exec, new_exec)

# find the end of the execution loop
old_end = """        if (op_dur > 0) {
          trk->wait_ticks += op_dur;
        }

#undef pos"""

new_end = """        if (op_dur > 0) {
          trk->wait_ticks += op_dur;
        }

#undef pos"""

text = text.replace("      }\n    }\n  }\n}\n", "      }\n    }\n    if (any_zero_delay) continue;\n  }\n}\n")

with open('src/core/sound.c', 'w') as f:
    f.write(text)
