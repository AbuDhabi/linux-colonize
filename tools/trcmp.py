#!/usr/bin/env python3
# trcmp.py TRACE K [show] [FIRST]: port vs a draw-traced DOSBox run of the independence campaign
# (docs/idle_independence.md): per-save field diffs from the trace's first `save N draws` line up
# to K, and the interval-K draw stream (trace lines `D lo hi ...` vs the port's RANGEDBG lines).
# Usage from the repo root: /usr/bin/python3 tools/trcmp.py TRACE.txt K [show] [FIRST=1]
# Output columns of the per-save diff are (DOS, port) despite jdiff's header.
import os, re, subprocess, sys
S = os.path.dirname(os.path.abspath(__file__))
tr, K = sys.argv[1], int(sys.argv[2]); show = int(sys.argv[3]) if len(sys.argv) > 3 else 0
first = sys.argv[4] if len(sys.argv) > 4 else '1'
L = open(tr).read().split('\n')
W = S + '/tc'; os.makedirs(W, exist_ok=True)
for f in os.listdir(W): os.remove(W + '/' + f)
env = dict(os.environ, GOLDEN_IDLE_KEEP_GOING='1', GOLDEN_IDLE_DUMP_ALL='1', GOLDEN_IDLE_DUMP=W + '/s%d.sav',
           RANGEDBG='1')
p = subprocess.run(['build/debug/golden_idle_campaign', 'independence', first, str(K)], env=env, capture_output=True, text=True)
def port_lines():
    cur = int(first); out = []  # the harness prints its first `save N:` line at FIRST+1
    for l in p.stderr.split('\n'):
        m = re.match(r'save (\d+): (\d+) draws', l)
        if m: cur = int(m.group(1)); continue
        if cur == K - 1 and l.startswith('RANGE'): out.append(l)
    return out
m0 = next((int(m.group(1)) for m in map(lambda l: re.match(r'save (\d+) draws', l), L) if m), int(first) + 1)
def dos_lines():
    cur = m0 - 1; out = []
    for l in L:
        m = re.match(r'save (\d+) draws', l)
        if m: cur = int(m.group(1)); continue
        if cur == K - 1 and l.startswith('D'): out.append(l)
    return out
pl, dl = port_lines(), dos_lines()
pr = [(int(l.split()[1]), int(l.split()[2])) for l in pl]
dr = [(int(l.split()[1]), int(l.split()[2])) for l in dl]
mm = next((i for i, (a, b) in enumerate(zip(pr, dr)) if a != b), None)
print('draws port %d dos %d first mismatch %s' % (len(pr), len(dr), mm if mm is not None else 'none') +
      ('' if mm is None else ' port %s dos %s' % (pr[mm], dr[mm])))
if mm is not None:
    for l in dl[max(0, mm - 4):mm + 3]: print('  dos', l)
    for l in pl[max(0, mm - 4):mm + 3]: print('  port', l)
for k in range(m0, K + 1):
    d = 'original_saves/independence-seed-100/%06d.SAV' % k
    if not os.path.exists(d) or not os.path.exists(W + '/s%d.sav' % k): continue
    dj, pj = W + '/d%d.sav.json' % k, W + '/s%d.sav.json' % k
    subprocess.run(['build/debug/sav_json', d, dj], capture_output=True)
    subprocess.run(['build/debug/sav_json', W + '/s%d.sav' % k, pj], capture_output=True)
    r = subprocess.run(['/usr/bin/python3', S + '/jdiff.py', dj, pj, str(show if k == K else 0)], capture_output=True, text=True).stdout
    print('save %d: %s' % (k, r.strip() if k == K else r.split('\n')[0]))
