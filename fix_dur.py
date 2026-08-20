with open('src/core/sound.c', 'r') as f:
    text = f.read()

text = text.replace('op_dur = dur;', 'op_dur = dur + 1u;')
text = text.replace('op_dur = dur ? (uint32_t)dur : 1u;', 'op_dur = dur + 1u;')

with open('src/core/sound.c', 'w') as f:
    f.write(text)
