with open('src/core/sound.c', 'r') as f:
    text = f.read()

replacement = r'''
      if (song->id == 0x26 && time > 14600) {
        printf("T%d: op %02x time=%d pos=%d\n", current_t, op, (int)time, (int)pos);
      }
      switch (op) {
'''
text = text.replace('      switch (op) {', replacement)

with open('src/core/sound.c', 'w') as f:
    f.write(text)
