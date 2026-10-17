#!/usr/bin/env python3
# trcmp.py TRACE K [show]: port vs a draw-traced DOSBox run (tr9-style), saves 4..K and interval K draws.
# Usage from the repo root: /usr/bin/python3 tools/trcmp.py original_saves/independence-seed-100/tr9/tr9.txt K [show]
# Output columns of the per-save diff are (DOS, port) despite jdiff's header.
import os, re, subprocess, sys
S = os.path.dirname(os.path.abspath(__file__))
tr, K = sys.argv[1], int(sys.argv[2]); show = int(sys.argv[3]) if len(sys.argv) > 3 else 0
L = open(tr).read().split('\n')
picks = {}
for l in L:
    m = re.match(r'PICK save (\d+) draw (\d+)', l)
    if m: picks.setdefault(int(m.group(1)), []).append(m.group(2))
spec = ';'.join('%d:%s' % (k, ','.join(v)) for k, v in sorted(picks.items()) if k <= K)
W = S + '/tc'; os.makedirs(W, exist_ok=True)
for f in os.listdir(W): os.remove(W + '/' + f)
env = dict(os.environ, GOLDEN_IDLE_KEEP_GOING='1', GOLDEN_IDLE_DUMP_ALL='1', GOLDEN_IDLE_DUMP=W + '/s%d.sav',
           GOLDEN_IDLE_PICKS=spec, RANGEDBG='1', PICKCTX='1')
p = subprocess.run(['build/debug/golden_idle_campaign', 'independence', '0', str(K)], env=env, capture_output=True, text=True)
# port interval K draws
pr = []; cur = 0
for l in p.stderr.split('\n'):
    m = re.match(r'save (\d+): (\d+) draws', l)
    if m: cur = int(m.group(1)); continue
    if cur == K - 1 and l.startswith('RANGE'):
        x = l.split(); pr.append((int(x[1]), int(x[2])))
dr = []; cur = 3; acts = []
for l in L:
    m = re.match(r'save (\d+) draws', l)
    if m: cur = int(m.group(1)); continue
    if cur == K - 1 and l.startswith('D'):
        x = l.split(); dr.append((int(x[1]), int(x[2])))
mm = next((i for i, (a, b) in enumerate(zip(pr, dr)) if a != b), None)
print('picks', spec)
print('draws port %d dos %d first mismatch %s' % (len(pr), len(dr), mm if mm is not None else 'none') +
      ('' if mm is None else ' port %s dos %s' % (pr[mm], dr[mm])))
base = '%s/%s_%%d.sav' % (os.path.dirname(tr), os.path.basename(tr).split('.')[0])
for k in range(4, K + 1):
    d = base % k
    if not os.path.exists(d) or not os.path.exists(W + '/s%d.sav' % k): continue
    for f in (d, W + '/s%d.sav' % k):
        subprocess.run(['build/debug/sav_json', f], capture_output=True)
    r = subprocess.run(['/usr/bin/python3', S + '/jdiff.py', d + '.json', W + '/s%d.sav.json' % k, str(show if k == K else 0)], capture_output=True, text=True).stdout
    print('save %d: %s' % (k, r.strip() if k == K else r.split('\n')[0]))
