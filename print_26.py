with open('src/core/sound.c', 'r') as f:
    text = f.read()

replacement = r'''
      if (song->id == 0x26) {
        if (op < 0xBA) printf("T%d: ch%d %02x dur=%d time=%d\n", current_t, channel, op, ds_img[pos+1], (int)time);
        else if (op == 0xFE || op == 0xFF) printf("T%d: op %02x count=%d time=%d\n", current_t, op, ds_img[pos+1], (int)time);
        else if (op == 0xFD) printf("T%d: op FD target=%d time=%d\n", current_t, trk->loop0_target, (int)time);
      }
      switch (op) {
'''
text = text.replace('      switch (op) {', replacement)
with open('src/core/sound.c', 'w') as f:
    f.write(text)
