#!/usr/bin/env python3
"""Automate VR_B465X hang dump on nested Xephyr (best-effort).

Plan prefers manual dumps; this is a fallback. Saves via DOSBox-X mapper
Ctrl+Alt+F5 after hang is detected (IP spinning on EB FE).
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

ROOT = Path(__file__).resolve().parents[2]
COLONIZE = ROOT / "COLONIZE"
CONF = ROOT / "tools/brave_dump/dosbox-b465x.conf"
SAVE_DIR = ROOT / "original_memory_dumps/dosbox_save_state_b465x"
TURN2 = ROOT / "test-saves-ai/TURN2.SAV"
OUT = ROOT / "dosbox-x-dumps" / "dump_b465x3"
NESTED = ":98"
HDR = 8
DS = 0x2385

SAVE_DIR.mkdir(parents=True, exist_ok=True)
for old in SAVE_DIR.iterdir():
    if old.is_file():
        old.unlink()

shutil.copy(TURN2, COLONIZE / "COLONY00.SAV")

CONF.write_text(
    f"""[sdl]
fullscreen=false
vsync=false
output=surface
autolock=false
windowresolution=640x480

[cpu]
core=normal
cycles=max

[dosbox]
title=VR_B465X
memsize=16
autosave=5 1
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
VR_B465X.EXE
"""
)

xephyr = subprocess.Popen(
    ["Xephyr", NESTED, "-title", "b465x", "-screen", "800x600x24", "-ac", "-nolisten", "tcp"],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
time.sleep(1.2)
if xephyr.poll() is not None:
    raise SystemExit("Xephyr failed")

env = os.environ.copy()
env["DISPLAY"] = NESTED
env["SDL_RENDER_DRIVER"] = "software"
env["LIBGL_ALWAYS_SOFTWARE"] = "1"

wm = subprocess.Popen(
    ["bash", "-c", "command -v openbox >/dev/null && exec openbox; exec sleep infinity"],
    env=env,
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
time.sleep(0.8)

X = ctypes.CDLL(ctypes.util.find_library("X11"))
T = ctypes.CDLL(ctypes.util.find_library("Xtst"))
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

disp = X.XOpenDisplay(NESTED.encode())
if not disp:
    raise SystemExit("no display")


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


def focus() -> None:
    click(400, 300)


def latest() -> Path | None:
    cands = list(SAVE_DIR.glob("*.sav")) + list(SAVE_DIR.glob("*.zip"))
    return max(cands, key=lambda p: p.stat().st_mtime) if cands else None


def parse(sav: Path) -> dict:
    out: dict = {"uc": -1, "sioux": None, "hang": False, "path": str(sav)}
    try:
        with zipfile.ZipFile(sav) as z:
            mem = z.read("Memory")
            cpu = z.read("CPU")
    except Exception as e:
        out["err"] = str(e)
        return out

    def ds(off: int, n: int = 1) -> bytes:
        return mem[HDR + DS * 16 + off : HDR + DS * 16 + off + n]

    out["uc"] = int.from_bytes(ds(0x539c, 2), "little")
    # Hang pattern for X
    out["hang"] = mem.find(bytes.fromhex("81fbf80174fe5e5fc9cb")) >= 0
    bx = int.from_bytes(cpu[3 * 4 : 3 * 4 + 2], "little")
    out["bx"] = bx
    if 0 < out["uc"] < 500:
        for i in range(out["uc"]):
            base = 0x3144 + i * 0x1C
            x, y = ds(base, 1)[0], ds(base + 1, 1)[0]
            nat = ds(base + 3, 1)[0] & 0x0F
            if nat == 10 and ds(base + 2, 1)[0] == 19:
                out.setdefault("sioux_all", []).append(
                    {
                        "i": i,
                        "xy": (x, y),
                        "spent": ds(base + 5, 1)[0],
                    }
                )
            if (x, y) in ((49, 40), (49, 39)):
                out["sioux"] = {
                    "i": i,
                    "xy": (x, y),
                    "spent": ds(base + 5, 1)[0],
                    "nat": nat,
                }
    # At hang BX should be 1F8
    if bx == 0x1F8:
        out["unit"] = {
            "spent": ds(0x3149 + 0x1F8, 1)[0] if False else ds(0x1F8 + 0x3149, 1)[0],
            "xy": (ds(0x1F8 + 0x3144, 1)[0], ds(0x1F8 + 0x3145, 1)[0]),
        }
        # Correct: unit base is 0x3144 + idx*0x1c; BX is frame offset from 0?
        # Prior dumps: BX=0x1F8 means offset into unit table from 0x3144? 
        # parse uses ds(bx+0x3144) — so BX is index*0x1c only, add 0x3144
        out["unit"] = {
            "spent": ds(bx + 0x3149, 1)[0],
            "xy": (ds(bx + 0x3144, 1)[0], ds(bx + 0x3145, 1)[0]),
        }
    return out


proc = subprocess.Popen(
    [
        "dosbox-x",
        "-conf",
        str(CONF),
        "-nogui",
        "-nomenu",
        "-fastlaunch",
        "-savedir",
        str(SAVE_DIR),
    ],
    env=env,
    cwd=str(COLONIZE),
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
print("started", proc.pid)
time.sleep(8)
focus()
for _ in range(50):
    tap("Escape", 0.08)
focus()
# Load slot 0
for _ in range(2):
    tap("Down", 0.3)
tap("Return", 1.2)
tap("Return", 1.2)
tap("l", 0.4)
tap("Return", 1.0)
tap("Home", 0.3)
tap("Return", 2.0)
time.sleep(4)

loaded = False
for i in range(30):
    focus()
    tap("Escape", 0.1)
    sav = latest()
    if sav:
        st = parse(sav)
        print(f"load#{i}", st.get("uc"), st.get("sioux"))
        if st.get("uc", 0) > 0:
            loaded = True
            break
    time.sleep(2)

if not loaded:
    print("FAIL load")
else:
    print("EOT until hang (BX=1F8 or Sioux moved)")
    for n in range(180):
        focus()
        tap("n", 0.12)
        if n % 8 == 7:
            tap("Return", 0.12)
        sav = latest()
        if sav:
            st = parse(sav)
            if n % 10 == 0:
                print(f"eot#{n}", st.get("sioux"), "bx", hex(st.get("bx", 0)))
            if st.get("bx") == 0x1F8 and st.get("unit"):
                print("HANG?", st)
                # copy out
                if OUT.exists():
                    if OUT.is_dir():
                        shutil.rmtree(OUT)
                    else:
                        OUT.unlink()
                OUT.mkdir(parents=True)
                with zipfile.ZipFile(sav) as z:
                    z.extractall(OUT)
                print("wrote", OUT)
                break
            # Sioux already at (49,39) spent 3 without hang — X missed
            if st.get("sioux") and st["sioux"]["xy"] == (49, 39):
                print("NO HANG; Sioux already moved", st)
                break
        time.sleep(0.5)

for p in (proc, wm, xephyr):
    if p.poll() is None:
        p.send_signal(signal.SIGTERM)
time.sleep(1)
for p in (proc, wm, xephyr):
    if p.poll() is None:
        p.kill()
print("done")
