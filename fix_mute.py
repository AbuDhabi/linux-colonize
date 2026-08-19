with open('src/core/sound.c', 'r') as f:
    text = f.read()

# Add mute to TrackState
text = text.replace('bool active;', 'bool active;\n  bool mute;')

# Initialize mute to false (meaning notes play)
# Wait, let's call it `play_notes` and initialize to true!
text = text.replace('bool active;\n  bool mute;', 'bool active;\n  bool play_notes;')

text = text.replace('tracks[t].active = true;', 'tracks[t].active = true;\n    tracks[t].play_notes = true;')

# Add play_notes to the macro definitions
text = text.replace('#define channel (trk->channel)', '#define channel (trk->channel)\n#define play_notes (trk->play_notes)')
text = text.replace('#undef channel', '#undef channel\n#undef play_notes')

# Implement 0xC2 and 0xC3
c2_old = """      case 0xC2: { /* ??? */
        pos += 1;
        break;
      }"""
c2_new = """      case 0xC2: { /* play notes */
        play_notes = true;
        pos += 1;
        break;
      }"""
text = text.replace(c2_old, c2_new)

c3_old = """      case 0xC3: { /* ??? */
        pos += 1;
        break;
      }"""
c3_new = """      case 0xC3: { /* mute notes */
        play_notes = false;
        pos += 1;
        break;
      }"""
text = text.replace(c3_old, c3_new)

# Modify note playing condition
note_old = """      if (note_raw == 0) {"""
note_new = """      if (note_raw == 0 || !play_notes) {"""
text = text.replace(note_old, note_new)

with open('src/core/sound.c', 'w') as f:
    f.write(text)
