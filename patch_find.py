with open('src/core/sound.c', 'r') as f:
    text = f.read()

replacement = r'''
      if (song->id == 0x33 && (int)time < 200) {
        printf("T%d: op=%02x dur=%02x time=%d\n", current_t, op, ds_img[pos+1], (int)time);
      }
      switch (op) {
'''
import re
text = re.sub(r'      switch \(op\) \{', replacement, text, count=1)
with open('src/core/sound.c', 'w') as f:
    f.write(text)
