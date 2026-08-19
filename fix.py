import re
with open('src/core/sound.c', 'r') as f:
    text = f.read()

text = text.replace('printf("Tick %u Track %d reads reg %d\\n", time, channel, a); const uint8_t ra = regs[a];', 'const uint8_t ra = regs[a]; printf("Tick %u Track %d reads reg %d = %d\\n", time, channel, a, ra);')
text = text.replace('if (!(b & 0x80)) printf("Tick %u Track %d reads reg %d\\n", time, channel, b & 63); const uint8_t rb = (b & 0x80) ? (b & 0x7F) : regs[b & 63];', 'const uint8_t rb = (b & 0x80) ? (b & 0x7F) : regs[b & 63]; if (!(b & 0x80)) printf("Tick %u Track %d reads reg %d = %d\\n", time, channel, b & 63, rb);')

with open('src/core/sound.c', 'w') as f:
    f.write(text)
