#!/usr/bin/env python3
"""Launch VR_BRAVE under dosbox-x and drive NEW WORLD to the Brave dump."""
import ctypes
import ctypes.util
import os
import struct
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COLONIZE = ROOT / 'COLONIZE'
DUMP = COLONIZE / 'BRAVE.DMP'
CONF = ROOT / 'tools/brave_dump/dosbox-brave.conf'

if DUMP.exists():
    DUMP.unlink()

# XTest helpers
X = ctypes.CDLL(ctypes.util.find_library('X11'))
T = ctypes.CDLL(ctypes.util.find_library('Xtst'))
X.XOpenDisplay.restype = ctypes.c_void_p
X.XStringToKeysym.argtypes = [ctypes.c_char_p]
X.XStringToKeysym.restype = ctypes.c_ulong
X.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
X.XKeysymToKeycode.restype = ctypes.c_uint
T.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
X.XFlush.argtypes = [ctypes.c_void_p]

disp = X.XOpenDisplay(None)
if not disp:
    raise SystemExit('no X display')

def key(name, down=True):
    ks = X.XStringToKeysym(name.encode())
    kc = X.XKeysymToKeycode(disp, ks)
    T.XTestFakeKeyEvent(disp, kc, 1 if down else 0, 0)
    X.XFlush(disp)

def tap(name, pause=0.15):
    key(name, True)
    time.sleep(0.05)
    key(name, False)
    time.sleep(pause)

def tap_many(name, n, pause=0.2):
    for _ in range(n):
        tap(name, pause)

# Start dosbox-x
env = os.environ.copy()
env['SDL_VIDEO_WINDOW_POS'] = '100,100'
proc = subprocess.Popen(
    ['dosbox-x', '-conf', str(CONF), '-nogui', '-nomenu', '-fastlaunch'],
    env=env,
    cwd=str(COLONIZE),
)
print('dosbox pid', proc.pid)
time.sleep(4)  # boot + mount + start exe

# Drive UI: mash Escape through intros, then Enter through wizard.
# Timing is generous; VR_SEED makes mapgen deterministic once started.
print('sending keys...')
for _ in range(20):
    tap('Escape', 0.25)
time.sleep(1)
# Enter through menus (NEW WORLD is first option)
for i in range(40):
    tap('Return', 0.4)
    if DUMP.exists():
        print('dump appeared at step', i)
        break
    # also press space sometimes (skip)
    if i % 5 == 4:
        tap('space', 0.3)

# Wait for dump up to 3 minutes (mapgen can take a bit even at cycles=max)
deadline = time.time() + 180
while time.time() < deadline and not DUMP.exists():
    tap('Return', 0.5)
    tap('Escape', 0.2)
    if proc.poll() is not None:
        print('dosbox exited', proc.returncode)
        break

time.sleep(1)
if proc.poll() is None:
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

if not DUMP.exists():
    print('FAIL: no BRAVE.DMP')
    raise SystemExit(1)

raw = DUMP.read_bytes()
print('DUMP bytes', raw.hex(), 'len', len(raw))
if len(raw) < 21 or raw[:4] != b'BRV0':
    print('bad magic', raw[:4])
    raise SystemExit(2)
lcg = struct.unpack_from('<I', raw, 4)[0]
width = struct.unpack_from('<H', raw, 8)[0]
tribes = struct.unpack_from('<H', raw, 10)[0]
print(f'LCG={lcg} (port post-sat 3528268925) width={width} tribes={tribes}')
print(f'LCG match={lcg == 3528268925}')
tiles = [(9, 23), (10, 25), (10, 24)]
off = 12
for t in tiles:
    cont, flags, terr = raw[off], raw[off + 1], raw[off + 2]
    print(f'  {t}: cont={cont:02x} flags={flags:02x} terr={terr:02x}')
    off += 3
