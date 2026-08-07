#!/usr/bin/env python3
"""LOAD TURN2 under VR_B465D in Xephyr :99; autosave; wait for hang dump at DS:7000.

Isolation: all XTest keys/mouse stay on DISPLAY=:99 (nested Xephyr), not the login session.
"""
from __future__ import annotations

import ctypes
import ctypes.util
import os
import shutil
import signal
import subprocess
import time
import zipfile
from pathlib import Path

ROOT = Path('/home/jan/projects/linux-colonize')
COLONIZE = ROOT / 'COLONIZE'
CONF = ROOT / 'tools/brave_dump/dosbox-midturn-dump.conf'
SAVE_DIR = ROOT / 'original_memory_dumps/dosbox_save_state_brave_t2'
TURN2 = ROOT / 'test-saves-ai/TURN2.SAV'
NESTED_DISPLAY = ':99'
HDR = 8
DS = 0x2385

SAVE_DIR.mkdir(parents=True, exist_ok=True)
for old in SAVE_DIR.iterdir():
    if old.is_file():
        old.unlink()
    elif old.is_dir() and old.name == 'unz':
        shutil.rmtree(old)

shutil.copy(TURN2, COLONIZE / 'COLONY00.SAV')
# Remove prior dump file if any
for name in ('SPENT.DMP', 'S.DMP', 'BRAVE.DMP'):
    p = COLONIZE / name
    if p.exists():
        p.unlink()

CONF.write_text(
    f"""[sdl]
fullscreen=false
vsync=false
output=surface
autolock=true
windowresolution=640x480

[cpu]
core=normal
cycles=max

[dosbox]
title=VR_B465D midturn spent
memsize=16
autosave=3 1
saveslot=1
saveremark=false

[render]
scaler=none

[midi]
mpu401=none
mididevice=none

[sblaster]
sbtype=none

[speaker]
pcspeaker=false

[autoexec]
@echo off
mount c {COLONIZE}
c:
VR_B465D.EXE
"""
)

xephyr = subprocess.Popen(
    [
        'Xephyr',
        NESTED_DISPLAY,
        '-title',
        'brave-spent-dump',
        '-screen',
        '800x600x24',
        '-ac',
        '-nolisten',
        'tcp',
    ],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
time.sleep(1.0)
if xephyr.poll() is not None:
    raise SystemExit('Xephyr failed')

env = os.environ.copy()
env['DISPLAY'] = NESTED_DISPLAY
env['SDL_RENDER_DRIVER'] = 'software'
env['LIBGL_ALWAYS_SOFTWARE'] = '1'

wm = subprocess.Popen(
    ['bash', '-c', 'command -v openbox >/dev/null && exec openbox; command -v twm >/dev/null && exec twm; exec sleep infinity'],
    env=env,
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
time.sleep(0.8)

X = ctypes.CDLL(ctypes.util.find_library('X11'))
T = ctypes.CDLL(ctypes.util.find_library('Xtst'))
X.XOpenDisplay.restype = ctypes.c_void_p
X.XOpenDisplay.argtypes = [ctypes.c_char_p]
X.XStringToKeysym.argtypes = [ctypes.c_char_p]
X.XStringToKeysym.restype = ctypes.c_ulong
X.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
X.XKeysymToKeycode.restype = ctypes.c_uint
T.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
T.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
T.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
X.XFlush.argtypes = [ctypes.c_void_p]

disp = X.XOpenDisplay(NESTED_DISPLAY.encode())
if not disp:
    raise SystemExit('no nested display')


def flush() -> None:
    X.XFlush(disp)


def key(name: str, down: bool = True) -> None:
    ks = X.XStringToKeysym(name.encode())
    kc = X.XKeysymToKeycode(disp, ks)
    if kc == 0:
        return
    T.XTestFakeKeyEvent(disp, kc, 1 if down else 0, 0)
    flush()


def tap(name: str, pause: float = 0.15) -> None:
    key(name, True)
    time.sleep(0.05)
    key(name, False)
    time.sleep(pause)


def click(x: int, y: int) -> None:
    T.XTestFakeMotionEvent(disp, 0, x, y, 0)
    flush()
    time.sleep(0.05)
    T.XTestFakeButtonEvent(disp, 1, 1, 0)
    flush()
    time.sleep(0.05)
    T.XTestFakeButtonEvent(disp, 1, 0, 0)
    flush()
    time.sleep(0.1)


def focus_game() -> None:
    # Click center of Xephyr screen to focus SDL window
    click(400, 300)


def latest_save() -> Path | None:
    cands = list(SAVE_DIR.glob('*.sav')) + list(SAVE_DIR.glob('*.zip'))
    if not cands:
        return None
    return max(cands, key=lambda p: p.stat().st_mtime)


def read_state(sav: Path) -> dict:
    out = {'unit_count': -1, 'cost': -1, 'spent_dump': -1, 'sioux': None, 'path': str(sav)}
    try:
        with zipfile.ZipFile(sav) as z:
            mem = z.read('Memory')
    except Exception as e:
        out['err'] = str(e)
        return out

    def ds(off: int, n: int = 1) -> bytes:
        return mem[HDR + DS * 16 + off : HDR + DS * 16 + off + n]

    out['unit_count'] = int.from_bytes(ds(0x539c, 2), 'little')
    blob = ds(0x7000, 2)
    out['cost'] = blob[0]
    out['spent_dump'] = blob[1]
    uc = out['unit_count']
    if 0 < uc < 500:
        for i in range(uc):
            base = 0x3144 + i * 0x1c
            x, y = ds(base, 1)[0], ds(base + 1, 1)[0]
            if x == 49 and y == 40:
                out['sioux'] = {
                    'i': i,
                    'spent': ds(base + 5, 1)[0],
                    'type': ds(base + 2, 1)[0],
                    'nat': ds(base + 3, 1)[0] & 0x0F,
                }
                break
    return out


proc = subprocess.Popen(
    [
        'dosbox-x',
        '-conf',
        str(CONF),
        '-nogui',
        '-nomenu',
        '-fastlaunch',
        '-savedir',
        str(SAVE_DIR),
    ],
    env=env,
    cwd=str(COLONIZE),
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
print('Xephyr', xephyr.pid, 'dosbox', proc.pid, 'on', NESTED_DISPLAY)
time.sleep(6)
focus_game()

print('skip intros')
for _ in range(40):
    tap('Escape', 0.1)
focus_game()
time.sleep(0.5)

# Title menu: New World is often first; Load is next.
# Try Down → Enter (Load), then Enter on slot 0. Also try L.
print('load sequence')
for _ in range(2):
    tap('Down', 0.25)
tap('Return', 1.0)
tap('Return', 1.0)
focus_game()
tap('l', 0.5)
tap('Return', 1.0)
tap('Return', 1.0)
# Slot list: ensure slot 0
tap('Home', 0.3)
tap('Return', 1.0)
time.sleep(5)

# Wait until autosave shows a loaded game (unit_count > 0)
print('waiting for loaded game via autosave...')
loaded = False
for wait_i in range(40):
    focus_game()
    tap('Escape', 0.1)  # dismiss dialogs
    sav = latest_save()
    if sav:
        st = read_state(sav)
        print(f'  autosave#{wait_i} {st}')
        if st.get('unit_count', 0) > 0:
            loaded = True
            break
    time.sleep(3)

if not loaded:
    print('FAIL: game never loaded (unit_count still 0)')
else:
    print('EOT drive until DS:7000 dump or timeout')
    deadline = time.time() + 120
    n = 0
    while time.time() < deadline:
        focus_game()
        tap('n', 0.15)
        if n % 5 == 4:
            tap('Return', 0.15)
        if n % 10 == 9:
            tap('space', 0.15)
        sav = latest_save()
        if sav:
            st = read_state(sav)
            if n % 15 == 0:
                print(f'  eot#{n} {st}')
            # Hang dump written when Sioux ADD matched
            if st.get('cost', 0) not in (0, -1) or st.get('spent_dump', 0) not in (0, -1):
                # Prefer nonzero dump; cost 0 spent 0 is unset
                if st['cost'] != 0 or st['spent_dump'] != 0:
                    print('DUMP', st)
                    break
            # If Sioux already left (49,40), hang may have happened then continued? (we hang forever)
            if st.get('sioux') is None and st.get('unit_count', 0) > 0:
                # Check (49,39) presence after move — hang should freeze before move completes
                pass
        n += 1

# Final report
sav = latest_save()
if sav:
    # Extract for inspection
    unz = SAVE_DIR / 'unz'
    if unz.exists():
        shutil.rmtree(unz)
    unz.mkdir()
    with zipfile.ZipFile(sav) as z:
        z.extractall(unz)
    st = read_state(sav)
    print('FINAL', st)
    print('extracted Memory to', unz / 'Memory')
else:
    print('FINAL: no save')

for p in (proc, wm, xephyr):
    if p.poll() is None:
        p.send_signal(signal.SIGTERM)
time.sleep(1)
for p in (proc, wm, xephyr):
    if p.poll() is None:
        p.kill()

if not sav:
    raise SystemExit(1)
st = read_state(sav)
if st.get('unit_count', 0) <= 0:
    raise SystemExit(2)
if st.get('cost', 0) == 0 and st.get('spent_dump', 0) == 0:
    raise SystemExit(3)  # loaded but dump never fired
print('OK cost=%s spent=%s' % (st.get('cost'), st.get('spent_dump')))
