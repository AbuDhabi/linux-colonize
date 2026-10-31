# Tracing the DOS game under DOSBox-X

How to watch real VICEROY.EXE run: set breakpoints in any routine (overlay
or resident), read registers and memory, and step a saved game through a
turn. This is evidence tier 2 in docs/conventions.md "Evidence hierarchy",
and it is the next step whenever a golden mismatch is a near-tie or an
"unknown term". Static reading plus another fitted term is not.

Tool: `tools/dosbox_trace.py` (DOSBox-X 2026.07, Xvfb (Xephyr fallback), ImageMagick
`import`, python3 `pexpect` and `PIL`, all installed here). Worked example:
docs/archive/ai_first_colony_trace.md "Brave 021a trace".

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
- `e` on the map opens the Europe screen.

## Mouse

Host pointer motion (XTest) never reaches the game; button events do. After a LOAD,
DS:0x92f8 reads -1, and FUN_1a58_038b (raw 14854) then replaces the INT 33 position
with DS:0x92fc/0x92fe and ORs in the buttons at DS:0x92fa. So drive a virtual mouse
by writing `SM 237e:92f8 01 00 <btn lo hi> <x lo hi> <y lo hi>` (x, y in 320x200
game coordinates). FUN_1acb_0056 derives the edges each poll: DS:0x7ec press,
DS:0x7f4 release, DS:0x7f6 held. A drag is: write the button down at the start
point, let a few polls pass, move x/y, then write the button up. Worked example:
the Europe ship drags of docs/dos_bugs.md DB-56 (port_bugs.md #1159).

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
   (`ndisasm -b16 -o <routine_off>`). Ghidra counts from the entry SP, so its
   `local_N` is `[bp-(N-2)]` (`local_38` = `[bp-0x36]`). `[bp+6]` is the first
   far-call argument.

Trap-handling rules (independence campaign, 2026-10-15):

- Handle every `BPINT 3` stop at `off+1` with `trap_overlay`, whatever the CS: the patched
  overlay can come back at a CS already seen. A script that skipped seen CSes let the INT 3 run
  on, and the game silently corrupted (garbage gold, an autosave rewritten, no further traps).
- An OVL15 (5952) run died on the second reload with RTLink "Fatal error evm012 ... vectored
  call to page 0005h"; breakpoints left on the old CS are the likely cause (see `BPDEL *` under
  "Data traps").
- The in-game load's final Enter needs some `step()` polls to take effect, and the resumed AI
  slot runs during them, so arm `BPINT 3` before that Enter or the patched INT 3 runs unhandled.
- `setup` writes `ems=false xms=false umb=false` to `dbx.conf`; with EMS the trapped overlay is
  re-cached and later calls are missed.

RTLink also moves resident overlays in memory, carrying a restored byte along, so after a
`trap_overlay` later calls can go untrapped. To catch every call, leave the INT 3 in place and
emulate the routine's `ENTER size,0` at each stop instead: `SM ss:(SP-2)` = BP, `SR BP` SP-2,
`SR SP` SP-2-size, `SR IP` entry+4 (port_bugs.md #1135 scripts; keep `BPINT 3` set at all times).

Shortcut when the overlay is already resident: search memory for the routine's first
bytes (from the extracted segment) to get its CS; OVL05 (Europe, seg 38fd) sat at CS
0x8c8d and OVL13 (seg 4d56) at CS 0x8c65 in the 2026-10-23 runs (per session, not
fixed).

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
  the direction and `[bp-0x26]` its score, `[bp+6]` the unit. FUN_521d_0a60 (same overlay):
  `--patch-cc 0x4cc50`; both original bytes are 0xc8. A nation's primary goal table is
  DS:0x98b0 + nation*0x100 (64 x {x, y, code, prio}).
- Data breakpoints need no code patch and no overlay handling. Colony record i is at
  DS:0x5d46 + i*0xca (pop +0x1f, flags +0x1b, build +0x94, food +0x9a, horses +0xaa, rebel pair
  +0xc2/+0xc6); `BPM` on the bytes of interest, set before the load's final Enter, stops at each
  writer's CS:IP with its `SS:BP` locals readable, which gives per-tick colony timelines.
  Keyboard polls stop at F000 or 130B.
- Unit creation: `BPM` on the unit count DS:0x539c stops inside FUN_1427_06b4 (CS 0c4b) before
  the new record is written, so the record's bytes are a deleted unit's leftovers; read it at the
  next keyboard poll. The caller's frame is `[BP]` of the stop; walk BP with
  `d.word(..., signed=False)` and identify an overlay caller by its args and locals.
- Real caller of an overlay call (e.g. of a rand draw): the stack shows RTLink's return vector
  1931:1554, not the caller. Set `BP 1931:155D` and run once: SS:SP then holds the caller's
  runtime CS:IP (overlay CS varies per load). Find its bytes in VICEROY.EXE or in the asm bytes
  column (`grep c78674ff1600 viceroy_unpacked.asm`) to name the function.
- RNG-dependent outcomes need the archive's EXE: the America and independence archives are
  VR_SOUND runs, so use `setup --exe VR_SOUND.EXE` before trusting any roll. VR_SEED and VR_SOUND
  differ only in 7 bytes at file offset 20202; both patch the BIOS tick read `FUN_1c0c_0012` (file
  offset 58578) to `mov ax,100; retf`, so neither reseeds from the wall clock (port_bugs.md #1321).
  Two VR_SEED from-loads of america save 97 once disagreed on England's Galleon roll; the cause is
  not known. Resident thunk segments are relocated at run time by -0x7dc: FUN_281f_04d4
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
- A from-load is not the continuous run for anything that reads an UNSAVED table. DS:0x9870 (the
  4x16 per-continent stance FUN_521d_0a60 writes, `local_2a` in FUN_5952_035e) is all zeros after
  LOAD until each nation's 0a60 runs, which is AFTER its colony ticks: the first turn's ticks read
  stance 0, absorb colonists and draw an extra rng(0,3) at 5952:0dfa, and the autosave no longer
  matches the fixture (america 128->129: 99 field diffs). Patch the table before RUN (`SM 237e:9870
  ...` with the values a prior from-load left at turn end; the port's RTST chunk carries the same
  table) and the run tracks the fixture again. The LCG state is the dword DS:0x28ee in the game DS
  (0x237e here), readable at the 2043:04d4 stop; logging it beats a return BP per draw.
- 1931:1554 is RTLink's shared overlay-return trampoline: a BP on a return address that lands there
  fires on EVERY overlay return, not just the call it was set for. DOSBox-X `BPDEL seg:off` is not
  accepted (only `BPDEL #` / `BPDEL *`); drop a BP with `BPDEL *` and re-add the rest.
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
- Finding a stack-word writer: `BPM ss:e7c4` (and e7c5) in a window between two routine entries,
  logging CS:IP and the BP chain (`[bp]`, `[bp+2]`, `[bp+4]` per frame; 1931:1554 is the overlay
  trampoline) at each value change. It found the founding (0xe7d6) and hull-step (0x020e)
  writers; `f000:d105` stops are the BIOS disk read of an overlay load.
- The unsaved DS:0x9e98 rival-strength table is one shared [16] byte array: patching it with the
  port's 4x16 RTST block overruns 48 bytes (the year and tax went wrong). Patch DS:0x9870 only.
- A `BP 1541:0E04` rand stop costs ~0.5 s each; 750 draws exceed the 540 s send budget. Read the
  LCG state dword (0x28ee in the rand routine's DS) at the stops you care about instead.
- Colony screen by keyboard (2026-10-23, `dutch-reports.SAV`): after `load_slot` the map cursor
  (DS:0x8540 x, DS:0x853e y) sits on the active unit; Enter on an own colony opens it
  (FUN_2b5a_32ee); elsewhere `v` (0x76, scan 0x2f) is View mode, keypad keys move the cursor.
  On the colony screen Tab (0x09, scan 0x0f) sets DS:0x32e = 1 (colonist focus), Left / Right
  step DS:0x8d7c, Enter opens the jobs menu (FUN_2f2b_348c), `c` the build list. Colony records
  DS:0x5d46 stride 0xca, DS:0x8542 = the open one; the resident 15eb routines break at
  `0e0f:offset` (06d2, 28c8, 0668; DS:0x34d = the trial flag).
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
