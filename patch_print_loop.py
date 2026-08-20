with open('src/core/sound.c', 'r') as f:
    text = f.read()

replacement = r'''
      if (song->id == 0x33 && op >= 0xFC && current_t == 4) {
        printf("T4: op %02x time=%d\n", (unsigned)op, (unsigned)time);
      }
      switch (op) {
'''
text = text.replace('switch (op) {', replacement.strip())

with open('src/core/sound.c', 'w') as f:
    f.write(text)
