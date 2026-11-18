"""Scan all-AI playtest snapshots for AI anti-patterns.

Usage: python3 tools/ai_antipatterns.py GAME_DIR [GAME_DIR ...]
Each GAME_DIR holds replay_turn_NNN.sav.json files (ai_replay --save-every,
then sav_json). Prints per-nation lines per snapshot plus `!` flags: idle
builds, hammer banks, full warehouses, small hoarders, big armies, units
stuck afield, colonies shrinking, frozen builds. Heuristics, not proof.
"""
import json, sys, glob, collections, os
N = ['Eng', 'Fra', 'Spa', 'Dut']
T = {0:'col',1:'sol',2:'pio',3:'mis',4:'drg',5:'sct',6:'reg',7:'cont',8:'cav',9:'reg2',10:'treas',11:'art',12:'wag',13:'car',14:'mer',15:'gal',16:'pvt',17:'frg',18:'mow',19:'brave',20:'armbr'}
G = ['food','sugar','tob','cot','fur','lum','ore','silv','hors','rum','cig','cloth','coat','tg','tools','musk']
SHIP = range(13, 19)

def load(p):
    return json.load(open(p))

def key(u):
    return (u['nation_id'], u['type'], u['x'], u['y'])

def scan(game):
    files = sorted(glob.glob(f'{game}/replay_turn_*.sav.json'))
    prev = None
    print(f'##### {os.path.basename(game)}')
    for p in files:
        d = load(p)
        t = d['head']['turn']
        cols = {(c['x'], c['y']): c for c in d['colonies']}
        out = []
        for n in range(4):
            cs = [c for c in d['colonies'] if c['nation_id'] == n]
            us = [u for u in d['units'] if u['nation_id'] == n]
            on_map = [u for u in us if u['x'] < 200]
            tc = collections.Counter(T.get(u['type'], u['type']) for u in us)
            pop = sum(c['population'] for c in cs)
            armed = tc['sol'] + tc['drg'] + tc['art']
            line = f"{N[n]} col {len(cs):2} pop {pop:3} gold {d['nations'][n]['gold']:6} tax {d['nations'][n]['tax_rate']:2} FF {d['nations'][n]['founding_father_count']:2} " + ' '.join(f'{k}{v}' for k, v in sorted(tc.items(), key=str))
            flags = []
            idle = [c['name'] for c in cs if c['building_in_production'] == 255 and c['hammers'] >= 50]
            if idle: flags.append(f"idle-build {idle}")
            capped = [c['name'] for c in cs if c['hammers'] >= 400]
            if capped: flags.append(f"hammer-bank {capped}")
            one = [c['name'] for c in cs if c['population'] <= 1]
            if len(one) >= 2: flags.append(f"{len(one)} one-pop colonies")
            for c in cs:
                cap = 100 * (1 + c.get('warehouse_level', 0))
                full = [G[i] for i in range(1, 16) if c['stock'][i] >= cap]
                if len(full) >= 2: flags.append(f"{c['name']} full {full}")
                if c['stock'][14] >= 150 and c['population'] <= 3: flags.append(f"{c['name']} pop{c['population']} hoards tools {c['stock'][14]}")
                if c['stock'][15] >= 150 and c['population'] <= 3: flags.append(f"{c['name']} pop{c['population']} hoards muskets {c['stock'][15]}")
                if c['stock'][0] < 5 and c['population'] >= 2: flags.append(f"{c['name']} food {c['stock'][0]}")
            if pop and armed * 2 > pop + 4: flags.append(f"army {armed} vs pop {pop}")
            free_col = [u for u in on_map if u['type'] == 0 and (u['x'], u['y']) not in cols]
            if len(free_col) >= 4: flags.append(f"{len(free_col)} colonists outside colonies")
            europe = [u for u in us if u['x'] >= 200 and u['type'] not in SHIP]
            if len(europe) >= 4: flags.append(f"{len(europe)} land units in Europe")
            if d['nations'][n]['gold'] > 5000: flags.append("gold hoard")
            if prev:
                pk = collections.Counter(key(u) for u in prev['units'] if u['nation_id'] == n and u['orders'] == 0 and u['x'] < 200 and u['type'] not in SHIP and (u['x'], u['y']) not in pcols)
                ck = collections.Counter(key(u) for u in on_map if u['orders'] == 0 and u['type'] not in SHIP and (u['x'], u['y']) not in cols)
                stuck = pk & ck
                if sum(stuck.values()) >= 2:
                    flags.append('stuck-afield ' + ' '.join(f"{T.get(k[1])}@{k[2]},{k[3]}x{v}" for k, v in stuck.items()))
                pship = {(u['type'], u['x'], u['y'], tuple(u['cargo_items']), tuple(u['cargo_hold'])) for u in prev['units'] if u['nation_id'] == n and u['type'] in SHIP and u['holds_occupied']}
                same = [s for s in ((u['type'], u['x'], u['y'], tuple(u['cargo_items']), tuple(u['cargo_hold'])) for u in us if u['type'] in SHIP and u['holds_occupied']) if s in pship]
                if same: flags.append(f"{len(same)} ship(s) unchanged cargo+pos")
                pc = {c['name']: c for c in prev['colonies'] if c['nation_id'] == n}
                for c in cs:
                    o = pc.get(c['name'])
                    if o and o['population'] >= 3 and c['population'] < o['population'] - 1:
                        flags.append(f"{c['name']} shrank {o['population']}->{c['population']}")
                    if o and o['building_in_production'] == c['building_in_production'] != 255 and o['hammers'] == c['hammers'] and c['hammers'] > 0:
                        flags.append(f"{c['name']} build frozen ({c['building_in_production']} ham {c['hammers']})")
            out.append((line, flags))
        print(f"== t{t}")
        for line, flags in out:
            print('  ' + line)
            for f in flags:
                print('      ! ' + f)
        prev = d
        pcols = cols

for g in sys.argv[1:]:
    scan(g)
