import os, time, shutil
out = open(S + '/out/tr9.txt', 'w')
DS = 0x237e
sav = os.path.join(S, 'C', 'COLONY09.SAV')
m0 = os.path.getmtime(sav) if os.path.exists(sav) else 0
print(d.load_slot(0))
d.cmd('BP 1541:0E04'); d.cmd('BP 0AC3:0138'); d.cmd('BP 0AC3:0250')
day = 3; n = 0; in_pick = False; polls = 0; t0 = time.time(); full = True
d.cmd('BPM %x:%x' % (DS, 0x2d12))
while time.time() - t0 < 80000 and day < 20:
    if not d.run_until_break(60):
        continue
    CS, IP, SS, SP = d.regs('CS', 'IP', 'SS', 'SP')
    if CS == 0x1541 and IP == 0xe04:
        if not in_pick:
            if full:
                stk = d.mem(SS, SP, 0x18)
                lo = stk[14] | (stk[15] << 8); hi = stk[16] | (stk[17] << 8)
                w9 = stk[18] | (stk[19] << 8)
                ua = d.word(DS, 0x2d12, signed=False)
                ub = d.mem(DS, 0x3144 + (ua if ua < 400 else 0) * 0x1c, 0x1c)
                out.write('D%d %d %d w9 %x u%d t%d @%d,%d f%d sp%d o%d p%d\n' % (n, lo, hi, w9, ua, ub[2], ub[0], ub[1], ub[11] if ub[11] < 128 else ub[11] - 256, ub[5], ub[8], ub[7]))
            n += 1
        continue
    if CS == 0x0ac3 and IP == 0x138:
        in_pick = True
        out.write('PICK save %d draw %d slot %d\n' % (day + 1, n, d.word(DS, 0x5394, signed=False))); out.flush()
        continue
    if CS == 0x0ac3 and IP == 0x250:
        in_pick = False
        continue
    if CS in (0x130b, 0xf000):
        polls += 1
        if polls % 40 == 0:
            d.keys('space')
        if os.path.exists(sav) and os.path.getmtime(sav) != m0:
            time.sleep(1); m0 = os.path.getmtime(sav) if os.path.exists(sav) else 0; day += 1
            shutil.copy(sav, S + '/out/tr9_%d.sav' % day)
            out.write('save %d draws %d\n' % (day, n)); out.flush()
            n = 0
        continue
    if full:
        u = d.word(DS, 0x2d12, signed=False)
        b = d.mem(DS, 0x3144 + (u if u < 400 else 0) * 0x1c, 0x1c)
        out.write('ACT %d slot %d t%d @%d,%d mv%d\n' % (u, d.word(DS, 0x5394, signed=False), b[2], b[0], b[1], b[5]))
        out.flush()
out.write('end\n'); out.flush()
print('end')
