#!/usr/bin/env python3
"""Author an input-replay regression test (tests/README.md "Input-replay regressions")
from a step script instead of a live recording.

  python3 tools/regression_script.py NAME [--bless]

compiles tests-automated-regression/NAME/input.steps into input.jsonl, runs
`regression_replay` on it with REGRESSION_SHOTS=1 and turns the screenshots
into PNGs in build/debug/regression_out/NAME/ (shot_start.png, shot_<frame>.png
= the screen right before each press, shot_end.png). A missing output.sav is
seeded with input.sav, so the harness's field diff shows what the steps
changed. --bless copies the run's actual.sav to output.sav.

Step script, one step per line, `#` starts a comment; x/y are 320x200
logical screen pixels:

  key NAME [shift|ctrl|alt]   tap a key: a-z, 0-9, Return, Escape, space,
                              Backspace, Delete, Tab, Up/Down/Left/Right,
                              KP_1..KP_9, KP_Enter, F1..F10, Home, End
  click X Y [shift|ctrl|alt]  left click
  rclick X Y                  right click
  dclick X Y                  double click
  drag X1 Y1 X2 Y2 [shift]    left-button drag
  move X Y                    pointer move only
  wait N                      N idle frames
  shot                        screenshot here (taps F12, which the game ignores)
  gap N                       idle frames after each later step (default 10)
"""
import json
import os
import shutil
import subprocess
import sys

ROOT = 'tests-automated-regression'
OUT = 'build/debug/regression_out'
FRAME_MS = 16

KEYS = {'Return': 13, 'Escape': 27, 'space': 32, 'Backspace': 8, 'Tab': 9, 'Delete': 127,
        'Right': 0x4000004F, 'Left': 0x40000050, 'Down': 0x40000051, 'Up': 0x40000052,
        'KP_Enter': 0x40000058, 'Home': 0x4000004A, 'End': 0x4000004D}
KEYS.update({f'KP_{n}': 0x40000059 + n - 1 for n in range(1, 10)})
KEYS.update({f'F{n}': 0x4000003A + n - 1 for n in range(1, 11)})
# SDL left modifier keys and their KMOD bits.
MODS = {'shift': (0x400000E1, 0x0001), 'ctrl': (0x400000E0, 0x0040), 'alt': (0x400000E2, 0x0100)}


def keycode(name):
    if name in KEYS:
        return KEYS[name]
    if len(name) == 1 and (name.isdigit() or name.islower()):
        return ord(name)
    raise ValueError(f'unknown key {name!r}')


def compile_steps(lines):
    recs = []
    frame = 0
    gap = 10
    mx = my = 0

    def rec(kind, **kw):
        recs.append(dict(type=kind, t_ms=1000 + frame * FRAME_MS, frame=frame, **kw))

    def mouse(kind, x, y, **kw):
        rec(kind, x=x * 2, y=y * 2, lx=x, ly=y, **kw)

    def with_mod(mod, body):
        nonlocal frame
        if mod:
            code, bit = MODS[mod]
            rec('key_down', keycode=code, mod=bit)
            frame += 1
        body()
        if mod:
            rec('key_up', keycode=code, mod=0)
            frame += 1

    def press(x, y, button, clicks=1):
        nonlocal frame, mx, my
        if (x, y) != (mx, my):
            mouse('mouse_move', x, y, buttons=0)
            frame += 1
        mx, my = x, y
        mouse('mouse_down', x, y, button=button, clicks=clicks)
        frame += 3
        mouse('mouse_up', x, y, button=button, clicks=clicks)
        frame += 1

    for n, raw in enumerate(lines, 1):
        words = raw.split('#', 1)[0].split()
        if not words:
            continue
        op, args = words[0], words[1:]
        try:
            if op == 'key':
                code = keycode(args[0])

                def tap():
                    nonlocal frame
                    mod = MODS[args[1]][1] if len(args) > 1 else 0
                    rec('key_down', keycode=code, mod=mod)
                    if 32 <= code < 127 and not (mod & 0x0140):
                        ch = chr(code).upper() if mod & 1 else chr(code)
                        rec('text', text=ch)
                    frame += 3
                    rec('key_up', keycode=code, mod=mod)
                    frame += 1
                with_mod(args[1] if len(args) > 1 else None, tap)
            elif op in ('click', 'rclick', 'dclick'):
                x, y = int(args[0]), int(args[1])
                button = 'right' if op == 'rclick' else 'left'
                if op == 'dclick':
                    press(x, y, button)
                    frame += 2
                    press(x, y, button, clicks=2)
                else:
                    with_mod(args[2] if len(args) > 2 else None, lambda: press(x, y, button))
            elif op == 'drag':
                x1, y1, x2, y2 = map(int, args[:4])

                def pull():
                    nonlocal frame, mx, my
                    if (x1, y1) != (mx, my):
                        mouse('mouse_move', x1, y1, buttons=0)
                        frame += 1
                    mouse('mouse_down', x1, y1, button='left', clicks=1)
                    frame += 2
                    steps = 8
                    for i in range(1, steps + 1):
                        mouse('mouse_move', x1 + (x2 - x1) * i // steps, y1 + (y2 - y1) * i // steps,
                              buttons=1)
                        frame += 1
                    frame += 2
                    mouse('mouse_up', x2, y2, button='left', clicks=1)
                    frame += 1
                    mx, my = x2, y2
                with_mod(args[4] if len(args) > 4 else None, pull)
            elif op == 'move':
                mx, my = int(args[0]), int(args[1])
                mouse('mouse_move', mx, my, buttons=0)
                frame += 1
            elif op == 'shot':
                # F12 maps to no game key; its press only makes the harness dump a screenshot.
                rec('key_down', keycode=0x40000045, mod=0)
                frame += 1
                rec('key_up', keycode=0x40000045, mod=0)
                frame += 1
                continue
            elif op == 'wait':
                frame += int(args[0])
                continue
            elif op == 'gap':
                gap = int(args[0])
                continue
            else:
                raise ValueError(f'unknown step {op!r}')
        except (IndexError, ValueError, KeyError) as e:
            sys.exit(f'line {n}: {raw.strip()}: {e}')
        frame += gap
    if not recs:
        sys.exit('no steps')
    session = dict(type='session', version=1, t_ms=0, window_scale=2, screen_w=320, screen_h=200,
                   audio=False)
    return [session] + recs


def png(ppm):
    try:
        from PIL import Image
    except ImportError:
        return
    Image.open(ppm).resize((640, 400), Image.NEAREST).save(ppm[:-4] + '.png')
    os.remove(ppm)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if len(args) != 1:
        sys.exit(__doc__)
    name = args[0]
    test = os.path.join(ROOT, name)
    with open(os.path.join(test, 'input.steps')) as f:
        recs = compile_steps(f.readlines())
    with open(os.path.join(test, 'input.jsonl'), 'w') as f:
        f.writelines(json.dumps(r, separators=(',', ':')) + '\n' for r in recs)
    if not os.path.exists(os.path.join(test, 'output.sav')):
        shutil.copy(os.path.join(test, 'input.sav'), os.path.join(test, 'output.sav'))
    out = os.path.join(OUT, name)
    shutil.rmtree(out, ignore_errors=True)
    env = dict(os.environ, COLONIZE_TEST_ONLY=name, REGRESSION_SHOTS='1')
    rc = subprocess.call(['build/debug/regression_replay'], env=env)
    for f in sorted(os.listdir(out)) if os.path.isdir(out) else []:
        if f.endswith('.ppm'):
            png(os.path.join(out, f))
    if '--bless' in sys.argv:
        actual = os.path.join(out, 'actual.sav')
        if not os.path.exists(actual):
            sys.exit('no actual.sav to bless (the run failed before saving)')
        shutil.copy(actual, os.path.join(test, 'output.sav'))
        print(f'blessed {test}/output.sav')
    sys.exit(rc)


if __name__ == '__main__':
    main()
