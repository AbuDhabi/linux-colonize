#!/usr/bin/env python3
"""Broad-strokes comparison of two or more Col1 saves, per Euro nation.

    python3 tools/save_compare.py [LABEL=]A.SAV [LABEL=]B.SAV ...

Sections: units by type, orders byte, AI plan byte (unit +7), colonies and
ownership (founder by COLONY.TXT name list), colony buildings, Indian
settlements burned, map improvements, gross production per turn (port colony
preview via colony_prod, approximate), Europe prices, gold, tax.

Run from the repo root after `make build` (uses build/debug/sav_json and
build/debug/colony_prod). Column header is `<nation>:<label>` per save.
"""
import collections
import json
import os
import subprocess
import sys
import tempfile

BIN = 'build/debug'
NAT = ['Eng', 'Fra', 'Spa', 'Dut']
# src/core/units.h UNITS_ORDER_*
ORD = {0: 'none', 1: 'sentry', 2: 'trade route', 3: 'goto', 5: 'fortify', 6: 'fortified',
       7: 'build colony', 8: 'plow', 9: 'road', 11: 'AI sail', 12: 'AI move', 13: 'follow'}
# Col1 colony building fields (sav_json key) -> first @BUILDING row; the field is a
# tier mask (1, 3, 7), so its bit length is the tier.
BUILDING_FIELDS = [
    ('fortification', 0), ('armory', 3), ('docks', 6), ('town_hall', 9), ('schoolhouse', 12),
    ('warehouse', 15), ('stables', 17), ('custom_house', 18), ('printing_press', 19),
    ('weavers_house', 21), ('tobacconists_house', 24), ('rum_distillers_house', 27),
    ('capitol', 30), ('fur_traders_house', 32), ('carpenters_shop', 35), ('church', 37),
    ('blacksmiths_house', 39),
]


def section(path, name):
    out, on = [], False
    for line in open(path, encoding='latin-1'):
        line = line.rstrip('\r\n')
        if line.startswith('@'):
            on = line.strip() == name
            continue
        if on and line.strip() and not line.startswith(';'):
            out.append(line.split(',')[0].strip())
    return out


def load(arg, tmp, i):
    label, _, path = arg.rpartition('=')
    label = label or os.path.basename(path)
    js = os.path.join(tmp, f'{i}.json')
    subprocess.run([f'{BIN}/sav_json', path, js], check=True, capture_output=True)
    prod = subprocess.run([f'{BIN}/colony_prod', path], check=True, capture_output=True, text=True).stdout
    rows = [l.split('\t') for l in prod.splitlines() if l and not l.startswith('#')]
    return label, json.load(open(js)), rows


def main(argv):
    if len(argv) < 3:
        print(__doc__.strip())
        return 2
    unit_names = section('COLONIZE/NAMES.TXT', '@UNIT')
    cargo_names = section('COLONIZE/NAMES.TXT', '@CARGO')[:16]
    building_names = section('COLONIZE/NAMES.TXT', '@BUILDING')
    founder = {n: set(section('COLONIZE/COLONY.TXT', '@' + k))
               for n, k in enumerate(['ENGLISH', 'FRENCH', 'SPANISH', 'DUTCH'])}
    with tempfile.TemporaryDirectory() as tmp:
        runs = [load(a, tmp, i) for i, a in enumerate(argv[1:])]
    labels = [r[0] for r in runs]

    def row(name, vals):
        print(f'  {name[:24]:<24}' + ''.join(f'{v:>9}' for v in vals))

    def head(title):
        print(f'\n## {title}')
        row('', [f'{n}:{l[:4]}' for n in NAT for l in labels])

    def per_nation(fn, keys, name=str, skip_zero=True):
        for key in keys:
            vals = [fn(d, n, key) for n in range(4) for _, d, _ in runs]
            if not skip_zero or any(vals):
                row(name(key), vals)

    def units(d, n):
        return [u for u in d['units'] if u['nation_id'] == n]

    print('saves: ' + ', '.join(f"{l} = {d['head']['year']} turn {d['head']['turn']}" for l, d, _ in runs))

    head('Units by type (all locations; ships included)')
    per_nation(lambda d, n, t: sum(u['type'] == t for u in units(d, n)), range(len(unit_names)),
               lambda t: unit_names[t])
    per_nation(lambda d, n, _: len(units(d, n)), ['TOTAL units'])
    per_nation(lambda d, n, _: sum(u['x'] >= 200 or u['y'] >= 200 or u['x'] == 0 for u in units(d, n)),
               ['off map (Europe)'])
    per_nation(lambda d, n, _: sum(c['population'] for c in d['colonies'] if c['nation_id'] == n),
               ['colonists in colonies'])

    head('Unit orders byte')
    per_nation(lambda d, n, o: sum(u['orders'] == o for u in units(d, n)), range(256),
               lambda o: f'{o} ' + ORD.get(o, '?'))
    head('AI plan byte (unit +7)')
    plans = sorted({u['ai_plan'] for _, d, _ in runs for u in d['units'] if u['nation_id'] < 4})
    per_nation(lambda d, n, p: sum(u['ai_plan'] == p for u in units(d, n)), plans,
               lambda p: f"'{chr(p)}' 0x{p:02x}" if 32 < p < 127 else f'0x{p:02x}')

    head('Colonies')
    per_nation(lambda d, n, _: sum(c['nation_id'] == n for c in d['colonies']), ['count'])
    print('\n  held by a nation other than the founder (COLONY.TXT name list):')
    for label, d, _ in runs:
        moved = []
        for c in d['colonies']:
            f = [k for k in range(4) if c['name'] in founder[k]]
            if f and c['nation_id'] not in f:
                moved.append(f"{c['name']} ({NAT[f[0]]}->{NAT[c['nation_id']]})")
        print(f'   {label}: ' + (', '.join(moved) or '-'))
    print('\n  colonies (population):')
    for label, d, _ in runs:
        for n in range(4):
            print(f'   {label} {NAT[n]}: ' + ', '.join(
                f"{c['name']}({c['population']})" for c in d['colonies'] if c['nation_id'] == n))

    head('Colony buildings (colonies at each top tier)')
    firsts = [f for _, f in BUILDING_FIELDS] + [len(building_names)]
    for j, (key, first) in enumerate(BUILDING_FIELDS):
        tiers = firsts[j + 1] - first
        for t in range(1, tiers + 1):
            per_nation(lambda d, n, _: sum(c['nation_id'] == n and c['buildings'][key].bit_length() == t
                                           for c in d['colonies']),
                       [f'{building_names[first + t - 1]} ({t})' if tiers > 1 else building_names[first]])
    per_nation(lambda d, n, _: sum(c['buildings'][k].bit_length() for c in d['colonies'] if c['nation_id'] == n
                                   for k, _ in BUILDING_FIELDS), ['TOTAL tiers'])

    head('Indian settlements')
    per_nation(lambda d, n, _: d['nations'][n]['villages_burned'], ['burned counter'])
    for label, d, _ in runs:
        c = collections.Counter(t['nation_id'] for t in d['tribes'])
        print(f"   {label}: {len(d['tribes'])} settlements, by tribe id {dict(sorted(c.items()))}")

    print('\n## Map improvements')
    for label, d, _ in runs:
        m = d['map']
        tile, mask, w = bytes.fromhex(m['tile_hex']), bytes.fromhex(m['mask_hex']), m['width']
        towns = {(c['x'], c['y']) for c in d['colonies']} | {(v['x'], v['y']) for v in d['tribes']}
        road = sum(1 for i, b in enumerate(mask) if b & 0x08 and (i % w, i // w) not in towns)
        plow = sum(1 for b in mask if b & 0x40)
        forest = sum(1 for b in tile if b & 0x18 == 0x08)
        print(f'   {label}: roads (off colony/village) {road}, plowed {plow}, forested tiles {forest}')

    head('Gross production per turn (port colony preview, approximate)')
    for i, name in [(2, 'Bells'), (3, 'Crosses'), (4, 'Hammers')] + [(5 + c, cargo_names[c]) for c in range(16)]:
        vals = [sum(int(r[i]) for r in rows if int(r[0]) == n) for n in range(4) for _, _, rows in runs]
        if any(vals):
            row(name, vals)
    head('Europe price by cargo')
    for c in range(16):
        row(cargo_names[c], [d['nations'][n]['trade']['euro_price'][c] for n in range(4) for _, d, _ in runs])
    per_nation(lambda d, n, _: d['nations'][n]['gold'], ['gold'], skip_zero=False)
    per_nation(lambda d, n, _: d['nations'][n]['tax_rate'], ['tax'], skip_zero=False)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
