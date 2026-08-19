with open('src/core/sound.c', 'r') as f:
    text = f.read()

import re

text = text.replace("""      if (note_raw == 0 && dur == 0) {
        /* Terminal rest used at track ends — stop expanding. */
        break;
      }""", """      if (note_raw == 0 && dur == 0) {
        /* Terminal rest used at track ends — stop expanding. */
        trk->active = false; break;
      }""")

text = text.replace("""    if (pos >= ds_size) {
      break;
    }""", """    if (pos >= ds_size) {
      trk->active = false; break;
    }""")

text = text.replace("""      if (++stuck > 8) {
        break;
      }""", """      if (++stuck > 8) {
        trk->active = false; break;
      }""")

with open('src/core/sound.c', 'w') as f:
    f.write(text)
