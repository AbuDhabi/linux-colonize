# Tracing the DOS game under DOSBox-X

How to watch real VICEROY.EXE run: set breakpoints in any routine (overlay
or resident), read registers and memory, and step a saved game through a
turn. This is evidence tier 2 in docs/conventions.md "Evidence hierarchy",
and it is the next step whenever a golden mismatch is a near-tie or an
"unknown term". Static reading plus another fitted term is not.

Tool: `tools/dosbox_trace.py` (DOSBox-X 2026.07, Xephyr, ImageMagick
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
  VICEROY.EXE, and installs the save as COLONY00.SAV. Each `--patch-cc`
  turns one EXE file byte into INT 3 (see "Overlay code"). It also writes
  `$W/dbx.conf`.
- `start` launches a nested X server (`Xephyr :57`), DOSBox-X inside it, and
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
- The RNG state (FUN_1d1d_0e04, resident 1541:0e04) is the dword at 0x28ee in the routine's own
  DS (read `DS` at the break), not the game DS.
- 20e6 wander scorer (OVL14): `--patch-cc 0x4e2d6`, trap offset 0x20e6; at CS:5805 `[bp-0x4e]` is
  the direction and `[bp-0x26]` its score, `[bp+6]` the unit.
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
