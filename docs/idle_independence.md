# Independence idle campaign (`golden_idle_independence`)

STATUS: complete 2026-10-19. The port plays the whole re-recorded archive (VR_SOUND.EXE, saves
000001..000021, Autumn 1680 .. Spring 1691) byte-identical to DOS except the UI-only save 18
(below), and its draw stream matches the continuous DOS replay on every turn. CTests
`golden_idle_independence` and `golden_idle_independence_reload` (save/reload before every turn)
run `golden_idle_campaign independence 1 21`. Fix history:
[archive/idle_independence_fixed_2026-10-14_19.md](archive/idle_independence_fixed_2026-10-14_19.md)
(grep by `FUN_` name, interval or save number; do not read whole). Sister campaign (done):
[idle_campaign.md](idle_campaign.md), whose method and DOSBox recipe apply here too.

## Fixtures

`original_saves/independence-seed-100/` (readme.md there): VR_SOUND.EXE (VR_SEED.EXE with the
music pump jumped out, `tools/make_vr_sound.py`: seed 100, no wall-clock reseeds), Dutch human
(slot 3, `head.human_player = 3`). England (slot 0) is withdrawn (`control 2`).

| Save | Turn | Date | Note |
|---|---|---|---|
| `000001` | 269 | Autumn 1680 | Loaded from the main menu. Harness save 1 = campaign start. |
| `000002`..`000021` | 270..290 | Spring 1681..Spring 1691 | Every-turn autosaves (copies of COLONY09). Turn 288 (Spring 1690) has no save; the harness plays through it. |

The player declares independence during turn 269 (between saves 1 and 2: GAME menu, DECLARE
INDEPENDENCE, second option). Otherwise the player presses Space: popups take the first option,
units are skipped.

Harness: `golden_idle_campaign independence FIRST LAST` (save numbers 1..21), see
[tests/README.md](../tests/README.md). Before simulating toward save 2 it calls
`ai_king_menu_declare_independence`, which confirms on its own when there is no popup queue.

## What the gate checks

The run loads 000001, declares independence for the human during that turn (as the player did),
and carries its own in-memory state through every turn, including turn 288, which has no save.
After each archived turn the serialized save must be byte-identical to the archive, except the
fields `compare_save` (`tests/golden/test_idle_campaign.c`) ignores, the same list as the 1492
campaign ([idle_campaign.md](idle_campaign.md) "What the gate checks": human UI words, map
cursor and viewport, the save-path stack residue, the port's WoI latches in `unknown23_pad`, the
`COLNXEXT` tail). Save 18 is simulated but not compared (`known_residue_year`). Any other
difference is a regression.

Save 18 residue (self-healing, save 19 matches again): the order of Bahia's tile chain. It is DOS
UI state, traced 2026-10-19 in a restored-load session: an AI move near the viewport edge scrolls
the map cursor DS:0x8540/0x853e to Bahia (FUN_1984_02fc -> 029e -> 00e8 with DS:0x5390 = 1, actor
DOS index 85 at draw 445); Guadeloupe's fort fire on the crown ship (draw 478) then redraws the
sidebar, and FUN_49dd_0424 raw 78700-78701 04d6-sorts the cursor tile, which lifts the Wagon
Trains above the newly arrived Colonist. The port does not scroll the viewport for AI moves;
modelling it means emulating FUN_1984_02fc's edge test on the DOS viewport (DS:0x8328/0x832e,
size DS:0x8544/0x8546) for every move the human can see.

## Running and debugging

```
make test T=golden_idle_independence
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign independence 1 21
./build/debug/sav_json /tmp/x/sim_8.sav /tmp/x/sim.json
python3 tools/jdiff.py /tmp/x/sim.json dos.json
```

`%d` is the save number; dumps are written for mismatching and skipped saves. `RANGEDBG=1`
prints the port's draw stream (`RANGE lo hi draw N`), `PICKCTX=1` the Euro dispatcher's acts;
`tools/trcmp.py` compares one interval against a DOSBox trace. Other switches:
[debug_env_vars.md](debug_env_vars.md), harness flags: [tests/README.md](../tests/README.md).

## Earlier recordings (2026-10-14 .. 2026-10-17, deleted)

The first two archives started from START_SAVE, a manual save the player had loaded from the
in-game menu of a running French game: DOS's year loop FUN_130d_0290 then finished the French
day from the next slot (extra production ticks before 000001), and the 2026-10-17 archive also
carried unsaved DOS state past save 3. Those saves, the DOSBox references recorded against them
(tr9, tr10, ref) and the harness crutches they needed (`resume_slot` / `turn_end_from_slot`,
`GOLDEN_IDLE_PICKS` pick replay, `GOLDEN_IDLE_RESYNC`) were deleted 2026-10-18. The port's
in-game load and main-menu load do the same clean init (`game_apply_col1_save`); DOS's mid-day
resume of the outgoing game is deliberately not modelled. The archived fix log cites those
runs (tr9 interval K, tr10 K->K+1, ref K) as evidence: the saves are gone, the DOS behaviour
stands.

## Quirks of this save set

- **The archive is a from-load chain.** A continuous DOSBox replay from a main-menu load of
  000001 (declare, idle) reproduced saves 2..21 byte for byte apart from 2 bytes of save-path
  string (2026-10-19). Loading a LATER autosave in DOSBox does not reproduce the continuous run:
  0a60's work-queue military arm reads the stance table DS:0x9870 (nation*16 + continent) and
  the AI goal tables DS:0x98b0.., which the save does not carry. Trace continuously from 000001.
- **Human slot 3 exposes AI spent MP.** In the 1492 campaign the human was slot 0, so every AI
  slot acted before the day-top MP clear, and AI spent bytes (+0x3149) were always zero at the
  autosave. Here France and Spain act after the clear, so their spent bytes are compared.
- **The rank table DS:0x917c is not saved.** A load starts with zeros; the day top's 0550
  refresh fills it (`turn_step_indian`), and the SETUP slice hands the slots above the human
  the previous day top's table.

## DOSBox replays

Replay the chain: `setup --exe VR_SOUND.EXE --save original_saves/independence-seed-100/
000001.SAV`, `load_slot(0)`, then Alt-G (`keys((0,0x22))`), Down x6 (DECLARE INDEPENDENCE),
Enter, Down, Enter, and Space every ~40 polls (200 polls per turn until `COLONY09.SAV` changes,
then copy it). RNG: `BP 1541:0E04` (rand) stops on every draw; the state dword is at 0x28ee in
the routine's DS, and the range wrapper's `(lo, hi)` sit at stack words 7 and 8. Compare with the
port's `RANGEDBG=1` stream through `tools/trcmp.py`.
Continuous replay (2026-10-19; local, gitignored copies of the session scripts, its per-turn
traces and DS dumps are in `tools/tc/`: `cont_replay.py`, `cont_declare.py`, `cont_tr_NN.txt`,
`ds/ds_NN.bin`): after `load_slot(0)` and the declaration, `BPDEL *`, `BPINT 16`,
`BP 1541:0E04`, then loop `run_until_break`: a rand stop logs `D lo hi uN` (plus `colXXXX` =
[0x8542] and 24 stack words for the turns of interest) to `tr_NN.txt`; every 40 keyboard polls
and every 20 draws stuff a Space; when `COLONY09.SAV` changes, copy it to `auto_NN.SAV`, compare
with the archive, and dump the whole game DS (`MEMDUMPBIN 237e:0 10000`) to `ds_NN.bin` (the
unsaved tables: stance DS:0x9870, goals DS:0x98b0, rank DS:0x917c). ~6 min per idle turn (the
breaks cost, not the cycles, so leave `cycles=max`); 1 -> 14 took 1 h 50. Traps: a Space at EVERY poll keeps the Declaration screen from
leaving (its key wait flushes the buffer first), and `BPINT 16 0`/`BPINT 16 10` (AH-filtered)
never stop on that screen, so keep the plain `BPINT 16` pump. Loading a later autosave is NOT
a reference (stance/goal tables start at zero; the archived 13 -> 14 entry shows the damage).

Restored-load trace (a from-load session that runs like the continuous one, ~10 min a turn): load
autosave NN, dump its DS, then write every byte of the continuous session's `ds_NN.bin` in
DS:0x800..0xa900 that differs back with `SM 237e:off bytes` (runs of up to 16 bytes; template
`tools/tc/restore_trace17.py`), set the breakpoints and press Space. Used for intervals 7 -> 8,
17 -> 18 and the save 18 residue; its draws matched the continuous trace.
Real caller of a draw: the stack above the range wrapper shows RTLink's return vector
`1931:1554`, not the caller. At the rand stop set `BP 1931:155D`, run once, and SS:SP then holds
the caller's runtime CS:IP (overlay CS varies per load); find its bytes in VICEROY.EXE or the
asm bytes column (`grep c78674ff1600 viceroy_unpacked.asm`) to name the function.

For overlay traps add `[dos] ems=false xms=false umb=false` to `dbx.conf`. Without it the trapped
overlay is re-cached and later calls are missed. OVL14 (20e6/0a60): patch 0x4e2d6 (20e6) and
0x4cc50 (0a60), original byte 0xc8. The 4d2e wander score is final at CS:5805 ([bp-0x4e] dir,
[bp-0x26] score). France's primary goal table is DS:0x98b0 + nation*0x100 (64 x {x, y, code,
prio}). Unit indices shift mid-turn (compaction), so identify units by xy and type.

Overlay-trap traps (2026-10-15): the patched overlay can be reloaded at a CS the script has already
seen. Handle every stop at `off+1` with `trap_overlay`, whatever the CS. A script that skipped
seen CSes let the INT 3 run on: the game silently corrupted (garbage gold, COLONY08 rewritten
in the scratch copy, no further traps). A run with OVL15 (5952, `--patch-cc 0x53b7e`) died on
the second reload with RTLink "Fatal error evm012 ... vectored call to page 0005h"; breakpoints
left on the old CS are the likely cause. Delete them (`BPDEL *`, then re-set) when a new CS
traps. One-hit traces (first 5952 call per colony, first French act) are reliable. The
in-game load's final Enter needs some `step()` polls to take effect, and the resumed Spanish slot
runs during them, so arm `BPINT 3` before that Enter or the patched INT 3 runs unhandled.

Prefer data breakpoints for this chain (no code patch, no overlay trouble): game DS is 0x237e,
colony record i is at DS:0x5d46 + i*0xca, unit i at DS:0x3144 + i*0x1c. Set `BPM` on the bytes of
interest (pop +0x1f, food +0x9a, horses +0xaa, flags +0x1b, build +0x94, rebel pair +0xc2/+0xc6)
before pressing the final Enter. Each change then stops with the writer's CS:IP, and `SS:BP`
locals can be read at that stop. Keyboard polls stop at F000 or 130B. This gave clean per-tick
timelines for Plymouth and Jamestown.

Unit creation: `BPM` on the unit count DS:0x539c stops inside FUN_1427_06b4 (CS 0c4b) before the
new record is written, so its bytes are a deleted unit's leftovers. Read the record at the next
keyboard poll, and expect later deletes to shift indices. The caller's frame is `[BP]` of the
stop; overlay returns show as the RTLink stub 1931:1554, so identify the caller by its args and
locals (Ghidra `local_N` sits at BP-(N-2)). Walk BP with `d.word(..., signed=False)`.

`tools/dosbox_trace.py` runs under either `python3` (both have `pexpect` since 2026-10-19); a
second concurrent session needs its own display (`DOSBOX_TRACE_DISPLAY=:59`). `load_slot` works on this save list since the 2026-10-14 `list_row` fix (the list
header sits a few pixels off, and long save names cover the old highlight sample). Loop: Space
every 200 polls until `COLONY09.SAV` changes, then copy it.

## Open leads (not reached by this archive)

- The crown's sail-home lane count (`ai_king_mow_sail_home_20e6`, #954 arm: live lane count vs
  DOS's census-derived DS:0x9456 from the EOT tail): a lead from the deleted ref chain.
- 5952 `local_1b0` guesses (DS:0x84bb = 0, tribe owner price rows, `ai_euro_5952_ds_byte`):
  check against a trace that hits them.
- FUN_43f7_1528's DS:0x8542 rebind (`g_turn_1528_last_walked_colony`): the Custom House gate /
  AI-human branch nation mix and the compose-time clamps against the rebound stock are not
  ported (never exercised).
- `units_tile_stack_sink` ranks loose units only; its cargo-unload and europe_harbor callers may
  need the 0352 plunder relink's oldest-end rule too (unchecked).
- Opening a colony screen reseeds the DOS game LCG (FUN_15eb_1476); the port lays buildings out
  with a private LCG, so a human who opens a colony diverges from DOS from there. Not an idle
  item.
