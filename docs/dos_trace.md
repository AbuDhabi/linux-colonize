# Tracing the DOS game under DOSBox-X

How to watch real VICEROY.EXE run: set breakpoints in any routine (overlay
or resident), read registers and memory, and step a saved game through a
turn. This is evidence tier 2 in docs/conventions.md "Evidence hierarchy",
and it is the next step whenever a golden mismatch is a near-tie or an
"unknown term". Static reading plus another fitted term is not.

Tool: `tools/dosbox_trace.py` (DOSBox-X 2026.07, Xvfb (Xephyr fallback), ImageMagick
`import`, python3 `pexpect` and `PIL`, all installed here). Worked example:
docs/ai_first_colony_trace.md "Brave 021a trace".

## Quick start

```bash
W=/tmp/.../scratchpad/dt          # any private dir; never COLONIZE/ itself
python3 tools/dosbox_trace.py setup $W --save test-saves-ai/TURN3.SAV --patch-cc 0x46ffa
python3 tools/dosbox_trace.py start $W        # ~5 s; debugger halted at viceroy.exe
python3 tools/dosbox_trace.py send $W load.py  # load.py: print(d.load_slot(0))
python3 tools/dosbox_trace.py send $W trace.py
python3 tools/dosbox_trace.py stop $W
```

- `setup` copies `COLONIZE/` to `$W/C`, renames the chosen EXE (default
  `VR_SEED.EXE`, the seed-locked build the seed-100 goldens came from) to
  VICEROY.EXE, and installs the save as COLONY00.SAV. `VR_SOUND.EXE`
  (`tools/make_vr_sound.py`) is VR_SEED plus a no-op music pump: no
  wall-clock `srand(tick)` on the game LCG, so a DOS run is deterministic whatever the
  Sound options say; use `--exe VR_SOUND.EXE` for reference runs (docs/idle_independence.md).
  Each `--patch-cc`
  turns one EXE file byte into INT 3 (see "Overlay code"). It also writes
  `$W/dbx.conf`.
- `start` launches a virtual X server (`Xvfb :57` when installed, headless; otherwise a nested `Xephyr :57` window; set e.g. `DOSBOX_TRACE_DISPLAY=:59` on every
  command of a second, concurrent session), DOSBox-X inside it, and
  a controller that keeps the debugger session open. PIDs go to
  `$W/pids.json`.
- `send` runs a Python file inside the controller. Available names: `d`
  (a `Dbg`), `S` (= `$W`), and the module's globals. Whatever it prints
  comes back. Globals persist between sends, so a long job can be resumed.
  The default wait is 540 s, which keeps it under the 600 s tool limit. On
  timeout the job keeps running and its output lands in `$W/res.txt`. Don't
  send again until that file exists.
- `stop` SIGKILLs only the PIDs in `pids.json`.

`load_slot(0)` takes about 1.5 minutes: VICEROY is started directly (no OPENING intro) and lands on
the main menu. `setup` drops the ~30 `VR_*.EXE` variants and other extras from `$W/C`; in a crowded
directory VICEROY boots very slowly (user-observed, 2026-10-07).

## `Dbg` API

| Call | Does |
|---|---|
| `run_until_break(t)` | `RUN`, then wait for the next break. Returns False if still running after `t` s (see "Pump") |
| `step(n)` | `n` breaks in a row |
| `cmd(s)` | Raw debugger command (`BP seg:off`, `BPINT 3`, `BPM seg:off`, `BPDEL *`, `SM seg:off bytes`, `SR reg val`) |
| `regs('CS','IP','AX',...)` | Register values via `EV` |
| `mem(seg, off, n)` / `word(seg, off)` | Memory via `MEMDUMPBIN` (writes `$W/MEMDUMP.BIN`) |
| `unit(ds, idx)` | Decoded unit record at DS:3144 + 0x1c*idx |
| `shot(name)` | PNG of the game window (640x400 = 2x VGA) |
| `keys('down', 'enter', (ascii, scan))` | Put keys in the BIOS keyboard buffer |
| `until(pred)` | Run until `pred(gray_image)` is truthy |
| `load_slot(n)` | Main menu, LOAD Game, slot n, through to the map |
| `trap_overlay(off, byte)` | At a `BPINT 3` stop from a patched byte: restore it, rewind IP, return the overlay CS |

## Pump: how the script regains control

The debugger only accepts commands while the game is stopped. A headless
debugger cannot be interrupted from outside, so some breakpoint must keep
firing:

- `BPINT 16` (keyboard poll) fires constantly while the game waits for
  input: menus, the map, AI turns that check for a keypress. `load_slot`
  sets it. Each `RUN` then advances one poll, about 0.1 s of wall time.
- Loading screens do not poll. A `run_until_break` timeout there is
  normal. Just call `run_until_break` again.
- If the game sits in code that never polls and none of your breakpoints
  fire, the session is lost: `stop` and `start` again. `BPINT 8` (timer)
  always fires, but only advances 55 ms of game time per `RUN`, which is
  far too slow for anything but short spans.
- Screen redraws lag. A key often shows up only 50-100 breaks later.

## Keyboard

`keys()` writes the BIOS buffer at 0040:001E and resets head/tail
(0040:001A/1C). This works because the game reads INT 16.

- A key sent while the game is mid-transition is dropped, and so is one
  sent during the intro's last frame, which looks like the menu. Send one
  key at a time and confirm its effect on screen before sending the next.
  `select()` does this for menus.
- Extended keys use ascii 0 with the scan code: Down = (0, 0x50).
- Unattended turn loops: send `space` every ~200 polls. It dismisses popups and, on an idle
  human turn, skips the selected unit until the turn ends. A loop that presses once stalls
  at the first popup.
- After the opening animation the screen goes black and waits for a key.
  `until()` sends a Space whenever the screen is black.

## Seeing the screen

- Video goes to `Xephyr :57`, and screenshots are taken with
  `import -window <id>` on that display. The window title shows which
  program is running (`OPENING`, later `VICEROY`).
- **Do not** read VRAM with `MEMDUMPBIN a000:0`. Under this DOSBox-X it
  returns zeros.
- **Do not** run DOSBox-X headed on the user's `:0` display. The window
  pops up on their desktop, a covered window captures as black, and
  closing it asks for confirmation instead of exiting.
- Screen-state detectors in `Dbg` (`menu_row`, `list_row`, `on_map`) compare
  reference crops in `tools/dosbox_trace_ref/` (window coordinates) and find
  the dark highlight bar. Add a crop there when you automate a new screen.

## Debugger console quirks

- The full DOSBox-X command list is `docs/dosbox-x-debugger-commands.txt`;
  some commands are unavailable in this build.
- F10 never reaches the debugger (the terminal eats it); F11 does.
- The code view shows partial instructions: only the bytes in view are
  disassembled, so the first and last line can be wrong.
- The Output pane carries noise; memory breakpoints report there as
  `DEBUG: Memory breakpoint : seg:off - old -> new`.

## Launch traps

- `viceroy.exe` with no arguments runs and shows the main menu (COLONIZE.BAT
  goes through `opening.exe -g`, which plays the intro and chains
  `viceroy -o ...`; `viceroy -o` alone hangs in text mode).
- Keep DOSBox-X's default Sound Blaster and MPU-401. With `sbtype=none` /
  `mpu401=none` the game hangs probing the configured card. Silence
  belongs on the host side instead: `SDL_AUDIODRIVER=dummy` and
  `mididevice=none`.

## Overlay code

Overlay routines (most AI, e.g. `FUN_4d56_021a`) have no fixed CS; RTLink
loads them on demand. To catch one:

1. Find its file offset. `python3 tools/rtlink_overlay_extract.py
   COLONIZE/VICEROY.EXE OUT` writes `segments.json`: file offset =
   segment `codeOffset` + routine offset. `tools/address_mapping.csv`
   names the overlay (`OVL13_...`). Check that the first bytes there are
   the expected prologue (often `C8 xx xx 00`, an `enter`).
2. `setup --patch-cc <offset>`. On the first call, INT 3 runs with
   `BPINT 3` set, and `trap_overlay(routine_off, original_byte)` puts the
   byte back and returns the CS.
3. Set `BP cs:off` anywhere in the routine. The overlay stays at that CS
   while it is resident. If it is evicted and reloaded from disk, the
   patched byte traps again, so keep `BPINT 3` and keep calling
   `trap_overlay`.
4. Locals are `[bp-N]` with `N` from an ndisasm of the extracted segment
   (`ndisasm -b16 -o <routine_off>`). Ghidra's `local_NN` names follow the
   same offsets. `[bp+6]` is the first far-call argument.

Resident routines have fixed addresses per session but are relocated. Get
the real CS from a return address on the stack, or from a known thunk.
Avoid `BPM` (memory-change breakpoint) on hot data: it slows every
instruction. On one byte it is fine. It found the 3180 facing writer.

## Data traps

- Unit records: DS:3144, stride 0x1c. Bytes +0/+1 xy, +2 type, +3 nation
  (low nibble), +5 spent MP, +8 order, +9/+a goto, +b facing.
- DOS compacts the unit array when a unit is deleted (e.g. a colony is
  founded mid-turn). Every later index shifts down by one, so match units
  by xy and type, not by save index.
- Map layers are far pointers, not DS offsets: DS:0x160/0x162 (layer2 off:seg), DS:0x164/0x166
  (layer3, the save's `path`). A `BPM seg:off` on one tile byte finds its writer.
- Nation records: DS:0x8808, stride 0x13c (+0x2a treasury, +0x4c euro_price[16] bytes,
  +0x5c trade.nr[16] words, +0x7c gold[16] dwords, +0xbc tons). `BPM` on one word of the
  human row (Dutch nr[15] = DS:0x8c36) catches 38fd_1dfa / 1d80 / 0058 (resident CS e1bc in
  that session) in one idle turn; game DS was 0x237e there, read with `regs('DS')` at an
  INT 16 break.
- The RNG state (FUN_1d1d_0e04, resident 1541:0e04) is the dword at 0x28ee in the routine's own
  DS (read `DS` at the break), not the game DS.
- 20e6 wander scorer (OVL14): `--patch-cc 0x4e2d6`, trap offset 0x20e6; at CS:5805 `[bp-0x4e]` is
  the direction and `[bp-0x26]` its score, `[bp+6]` the unit.
- RNG-dependent outcomes need the archive's EXE: the America and independence archives are
  VR_SOUND runs. A VR_SEED from-load reseeds every nation slot from the wall clock (two from-loads
  of america save 97 disagreed on England's Galleon roll), so use `setup --exe VR_SOUND.EXE` before
  trusting any roll. Resident thunk segments are relocated at run time by -0x7dc: FUN_281f_04d4
  (rand range) is `BP 2043:04d4`, FUN_281f_04ca (slot reseed) `BP 2043:04ca`; the stack holds the
  caller's ret seg:off and the args. `BPM` on a cold byte works (one stop per read/write): the
  indian record's herd byte found FUN_5bfb_3180's native-contact spread in one turn.
- Other traps used by the America campaign (2026-10-20; file offset = segment `codeOffset` +
  routine offset, the `tools/address_mapping.csv` "offset" column is NOT that for L0040 overlays):
  FUN_479b_0972 goto mover = OVL09 `0x40e22` (trap 0x972); FUN_6662_0f74 pathfinder = OVL20
  `0x62d84` (trap 0xf74; its flood FUN_6662_00f2 is a near call in the same CS, so `BP cs:00f2`
  works without a patch; `[bp-0x1a]` = the returned direction at the 1599 exit, `[bp-0x48]` the
  unit); FUN_465b_0000 = OVL08 `0x3ecf0`; FUN_5952_035e = OVL15 `0x53b7e`. Resident routines
  need no patch: FUN_15eb_28c8 is `BP 0e0f:28c8`, its plot election `0e0f:2d14` (AX = score,
  `[bp-0x22]` job, `[bp-0x36]` local_38).
- Overlays move: with EMS off RTLink reloads them often and each reload re-traps. DOSBox-X
  breakpoints are linear addresses, so a BP left on an old CS fires inside whatever code is loaded
  there later (stops at nonsense CS:IP). `BPDEL *` and re-add every BP whenever a trap reports a
  new CS. The RUN after `trap_overlay` does not stop on a BP at the rewound IP, so log the first
  call from the trap handler itself.
- An overlay routine can be evicted in the middle of its own call: FUN_6662_00f2's resident
  helper FUN_281f_06e6 reaches FUN_1000_88d6 in another overlay, and OVL20 came back at a new CS
  before 00f2 returned (america 129->130), so a return-address BP set at entry never fires. Read a
  routine's result at the NEXT entry of the same routine instead (the 00f2 grid at DS:0xa270 with
  its key DS:0x2d1a/0x2d1c survives until the next rebuild).
- `BPM` is unusable on a hot byte: this build also stops on reads (a 5x5 scan loop at 9e0f:0452
  fired 80 times on a unit record byte before the turn began). Use code breakpoints.
- Uninitialised-local hunts (america save 64, 20e6's `local_ea` = SS:e7c4): log the word at every
  routine entry you care about (`E` lines) and at `BPINT 21 3f/42` (overlay loader reads: the
  seek offset names the overlay, `codeOffset - 0x170`); a value equal to an overlay's read length
  (0x2fe0) is loader residue. Frame depths are fixed per call chain: 20e6 and 0972 enter at SP
  e8ae, 0f74 at e8a2, 00f2 at e852 (its `PUSH SI` is e7c4) or e78e (from 0b4e). Overlay
  placement is history-dependent (the same call sequence from a fresh load evicts different
  overlays than the archive's long session), so loader residues cannot be reproduced; a map
  redraw (OVL22, file 0x66680) between AI acts is one eviction trigger. Breakpoints on a routine
  reached through a different CS base (5b66 at 8d2c:58a6 for an 8d00 trap) report that CS, so
  match stops by linear address.
- A `BP 1541:0E04` rand stop costs ~0.5 s each; 750 draws exceed the 540 s send budget. Read the
  LCG state dword (0x28ee in the rand routine's DS) at the stops you care about instead.
- Save files hold the state at the start of the human's turn. When a turn
  runs, all four European nations move before the Indians. So anything a
  Brave reads may already have been changed by Euro moves made that same
  turn. That was the cause of the facing mismatch.

## Process hygiene

- Stop sessions only with `dosbox_trace.py stop`, which kills recorded PIDs.
- Never `pkill -f <pattern>`: the pattern also matches your own shell's
  command line, which kills the tool call (exit 144).
- A `dosbox-x` that you did not start (no entry in `pids.json`) may be the
  user's own. Leave it alone.
