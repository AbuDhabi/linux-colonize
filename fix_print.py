with open('src/core/sound.c', 'r') as f:
    text = f.read()

text = text.replace('if (++stuck > 8) {', 'if (++stuck > 8) { printf("Track %d STUCK at pos %x\\n", current_t, pos);')
text = text.replace('printf("Track %d died at pos %x (opcode %x)\\n", current_t, pos, ds_img[pos]); trk->active = false; break;', 'trk->active = false; break;')

with open('src/core/sound.c', 'w') as f:
    f.write(text)
