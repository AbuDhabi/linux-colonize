with open('src/core/sound.c', 'r') as f:
    text = f.read()

replacement = r'''
      if (song->id == 0x26 && op == 0xFD) {
        printf("T%d: op FD loop0_target=%x loop0_start=%x time=%d\n", current_t, (unsigned)trk->loop0_target, (unsigned)trk->loop0_start, (unsigned)time);
      }
      switch (op) {
'''
text = text.replace('      switch (op) {', replacement)

with open('src/core/sound.c', 'w') as f:
    f.write(text)
