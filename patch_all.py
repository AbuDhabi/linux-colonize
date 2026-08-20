with open('src/core/sound.c', 'r') as f:
    text = f.read()

replacement = r'''
      if (time > 14650) {
        printf("T%d: ch%d op=%02x time=%d pos=%d song=%d\n", current_t, channel, op, (int)time, (int)pos, (int)song->id);
      }
      switch (op) {
'''
import re
text = re.sub(r'      if \(song->id == 0x26.*?      switch \(op\) \{', replacement, text, flags=re.DOTALL)
with open('src/core/sound.c', 'w') as f:
    f.write(text)
