# Seed-100 idle campaign gate (`golden_idle_campaign`)

STATUS: complete 2026-10-14. CTest `golden_idle_continuous` guards it; `golden_idle_reload` runs the
same years with a save/reload before every turn (byte-identical since 2026-10-19, the `RTST` chunk in
[savegame.md](savegame.md) carries the unsaved DOS tables). Fixtures:
`original_saves/1492-1600-seed-100/year_*.sav` (VR_SEED.EXE autosaves; the human idles in View
mode and never moves). Harness: [tests/README.md](../tests/README.md). Fix history:
[archive/idle_campaign_fixed_2026-10-06_08.md](archive/idle_campaign_fixed_2026-10-06_08.md),
[archive/idle_campaign_fixed_2026-10-10.md](archive/idle_campaign_fixed_2026-10-10.md),
[archive/idle_campaign_fixed_2026-10-10_14.md](archive/idle_campaign_fixed_2026-10-10_14.md)
(grep by `FUN_` name or year; do not read whole).

## What the gate checks

One port run loads `year_1492.sav` and plays every turn through 1599. Like DOS, it carries its own
in-memory state between turns. It plays through the years the archive skips (DOS made no autosave
there). After each archived year, the serialized save must be byte-identical to `year_Y.sav`,
except:

| Ignored | Why |
|---|---|
| `head.map_mode`, `head.tut2`, `stuff.x` / `stuff.y`, `stuff.viewport_x` / `viewport_y` | The human's first-turn UI (View mode, tutorial flag, map focus tile, viewport). The headless run never plays the human turn. These differ in every year. |
| `post_map.save_path_blob` | SS:local_8 stack residue of the DOS writer (FUN_75c2_0288), not state. |
| `nation[n].unknown23_pad` | DOS-dead bytes; the port keeps its WoI latches there (ai_king_latch_pad). |
| Port `COLNXEXT` tail past the DOS sections | Port-only state (village intel, docs/architecture.md); DOS never writes it. Present from 1595. |
| Years 1505 and 1506 (whole save) | DOS carried unsaved state here (below). Simulated, not compared; the run matches again from 1508. |

`compare_save` in `tests/golden/test_idle_campaign.c` owns this list. Everything else matches in
every archived year: units, colonies, nations, tribes, market, all map layers and RNG-driven state.
A new difference is a regression.

1505/1506 residue: ship 4's facing, Brave 26's step, and unit 5's goto plus one `path` byte. Port
from-load 1504 equals DOS from-load 1504. So the archive's continuous DOS session held something
unsaved that a load does not restore, and the port, having loaded nothing, does not model it. A DOS
replay from `year_1492` also departs from the archive (from 1497), so DOS itself is not repeatable
across sessions here.

## Running and debugging

```
make test T=golden_idle_continuous
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign 1492 1599
./build/debug/sav_json /tmp/x/sim_1591.sav /tmp/x/sim.json      # explicit output path!
./build/debug/sav_json original_saves/1492-1600-seed-100/year_1591.sav /tmp/x/dos.json
```

The run takes about 1.5 s. `%d` takes the year, and the dumps are written only for years that
mismatch or are missing from the archive. Diff field by field with a recursive JSON diff
(show `*_hex` blobs as differing byte offsets); byte offsets alone are useless.
`GOLDEN_IDLE_FROM=<save>` starts from another save, for example a DOSBox from-load autosave.
`GOLDEN_IDLE_RELOAD=1` serializes and reloads the port save before every turn (what a player who
quits and loads each turn gets; `keepseed` keeps the run's LCG seed instead of the load-time pick).
A year that matches continuously but not under reload names state the `RTST` chunk does not carry
yet: dump it, `jdiff.py`, then `AI_28C8_TRACE=1` both modes and diff the traces.
`GOLDEN_IDLE_PICKS="6:353;7:361"` replays wall-clock music-pick reseeds (srand(tick) on the game LCG) before those draw indices of the run into saves 6 and 7 (dos_rng.h; docs/idle_independence.md).
Without `GOLDEN_IDLE_KEEP_GOING` the run stops at the first mismatch or missing year.

Single-year transitions (`golden_idle_campaign Y Y+1` from the archived Y) are not a gate. The
archived Y+1 often carries unsaved DOS state that a load of Y cannot reach (the stance table below,
stale bytes in compacted unit slots). About 15 of the 77 archived transitions are byte-identical
that way.

### Tracing a regression in DOS

Load year_Y in DOSBox (docs/dos_trace.md: `setup --save year_Y.sav`, `load_slot`, then press space
until `DS:0x538e` changes). The human-slot autosave lands in `$W/C/COLONY09.SAV`. Copy it as soon
as its mtime changes: `setup` copies a stale COLONY09, and a popup can delay the write. Diff it
against `GOLDEN_IDLE_FROM` / from-load port output: whatever remains is a port bug.
- Use `BPM` on one record byte that differs, or `BP` in a resident routine. The resident CS is
  the Ghidra segment − 0x1000 + 0x0824.
- To trap an overlay routine, use `--patch-cc`. Turn EMS/XMS/UMB off in `dbx.conf`, or the
  overlay is re-cached without the trap and later calls are missed.
- DOS is not fully repeatable: treat a single-field mismatch as noise until a second DOS run
  reproduces it.

## What the autosave is (DOS year loop, raw 6330-6470)

Day top: `FUN_281f_0550`, `+0x3149 = 0` for every unit (spent MP), `0676` Indian mid-pass.
Then slot by slot: `0644` (= `FUN_3844_00f2` EOT, ending in 06ba lanes + `4962_0018` census),
for the human `FUN_130d_0172` autosave **before** Move Pieces (`FUN_2b5a_3b68`, which sets
`DS:0x5392 = 0xffff`). The calendar step is at the loop **end**. So with human slot 0 the
autosave holds: AI slots of year Y (old date), day top, human EOT of Y+1, nothing of the
human's Move Pieces. Head UI words at that point: `map_modal_active 0`, `no_unit_selected 0`,
`map_mode` = whatever the player left, `active_unit` = last AI hull that sailed for Europe
(raw 76484/77099 set 0x5392 before `291f_0208`), else 0xffff.

## Unsaved DOS state (why single-year loads differ)

- The `-0x6790` stance table (DS:0x9870) is not in the save and is zero after a DOS load; a
  continuous session carries last turn's values.
- Stale bytes in unit slots freed by compaction (for example a Europe Scout's goto/facing in
  1534) survive in memory but not across a load.
- DOS's music picker (`FUN_129f_0008`) draws twice at the human's slot start; harmless because
  every slot's 5e52 reseeds.
