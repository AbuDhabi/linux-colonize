import re
with open('src/core/sound.c', 'r') as f:
    text = f.read()

# We need to replace `return;` with `trk->active = false; break;` 
# ONLY inside the `while (tracks[current_t].active && tracks[current_t].wait_ticks == 0)` loop!
# Let's find the loop boundaries.
start_str = "        uint32_t op_dur = 0;"
end_str = "        if (op_dur > 0) {"

start_idx = text.find(start_str)
end_idx = text.find(end_str, start_idx)

body = text[start_idx:end_idx]

# Replace return;
body = body.replace('return;', 'trk->active = false; break;')

# Remove continue; in the `if (op <= 0xBA)` block!
# But only the one inside `if (op <= 0xBA) { ... stuck = 0; continue; }`
body = body.replace('stuck = 0;\n      continue;', 'stuck = 0;')

text = text[:start_idx] + body + text[end_idx:]

with open('src/core/sound.c', 'w') as f:
    f.write(text)
