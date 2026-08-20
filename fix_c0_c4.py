with open('src/core/sound.c', 'r') as f:
    text = f.read()

import re

# Replace 0xC4 far call
text = re.sub(r'case 0xC4: /\* far call via stream word.*?\n.*?\n.*?\n.*?\n.*?\n.*?\n        break;',
              r'case 0xC4: {\n        pos += 3;\n        break;\n      }',
              text, flags=re.MULTILINE|re.DOTALL)

# Replace 0xC0 return
text = re.sub(r'case 0xC0: { /\* return from 0xC4 far call or C5\+ cond call \*/\n.*?\n.*?\n.*?\n.*?\n.*?\n.*?\n      }',
              r'case 0xC0: { /* CC 0 Bank Select */\n        if (pos + 1 >= ds_size) {\n          trk->active = false; break;\n        }\n        sound_push_event(song, time, 0xb0, 0, ds_img[pos + 1] & 0x7f, channel);\n        pos += 2;\n        break;\n      }',
              text, flags=re.MULTILINE|re.DOTALL)

with open('src/core/sound.c', 'w') as f:
    f.write(text)

