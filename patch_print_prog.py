with open('src/core/sound.c', 'r') as f:
    text = f.read()

replacement = r'''
      if (song->id == 0x33 && op == 0xC0) {
        printf("T%d: PROG %d time=%d\n", current_t, (unsigned)ds_img[pos+1], (unsigned)time);
      }
      switch (op) {
'''
text = text.replace('switch (op) {', replacement.strip())

with open('src/core/sound.c', 'w') as f:
    f.write(text)
