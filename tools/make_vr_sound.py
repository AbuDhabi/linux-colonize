#!/usr/bin/env python3
"""Build COLONIZE/VR_SOUND.EXE from COLONIZE/VR_SEED.EXE.

VR_SEED fixes the timer word (seed 100). VR_SOUND additionally makes the music
pump FUN_129f_00f6 (file offset 0x4ee6, `enter 8,0`) behave as if Background
Music and Event Music were off: its gate `cmp word [0xa2],0 / jnz` at 0x4eea is
replaced by `jmp 0x50ba` (the gate's own exit: leave / retf), so the pump never
runs and never calls srand(BIOS tick) on the game LCG (asm 129f:0138 / 0250).
Verified 2026-10-17: one idle DOS turn from independence 000003 made 0 picks and
349 draws, and the autosave equals the port's run with no GOLDEN_IDLE_PICKS.

Usage: python3 tools/make_vr_sound.py [COLONIZE/VR_SEED.EXE] [COLONIZE/VR_SOUND.EXE]
"""
import sys

src = sys.argv[1] if len(sys.argv) > 1 else 'COLONIZE/VR_SEED.EXE'
dst = sys.argv[2] if len(sys.argv) > 2 else 'COLONIZE/VR_SOUND.EXE'
b = bytearray(open(src, 'rb').read())
GATE = 0x4EEA
assert b[GATE - 4:GATE] == bytes.fromhex('C8080000'), 'pump prologue not at 0x4ee6'
assert b[GATE:GATE + 0x11] == bytes.fromhex('833EA20000750A833E9E00007503E9BF01'), 'gate bytes differ'
assert b[0x50BA:0x50BC] == b'\xc9\xcb', 'pump exit not leave/retf'
b[GATE:GATE + 7] = bytes.fromhex('E9CD01') + b'\x90' * 4  # jmp 0x50ba; nop x4
open(dst, 'wb').write(b)
print('wrote', dst)
