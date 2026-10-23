# dos_bugs.md evidence archive (2026-10-23 batch)

STATUS: archived 2026-10-23, closed. Evidence, trace and test-history text moved
verbatim (links re-rooted, short lead-ins added) out of
[docs/dos_bugs.md](../dos_bugs.md) entries to keep that file under the doc-length
limit. Not a spec: each entry in `dos_bugs.md` stays authoritative and keeps its
id, DOS citation, port site and approval note. Look up one id:
`grep -n '^## DB-NN$' docs/archive/dos_bugs_evidence_2026-10-23.md`.

## DB-1

Evidence: the first two idle-independence recordings
([idle_independence.md](../idle_independence.md) "Earlier recordings") started
from such a load and drifted from a clean main-menu load of the same slot.

## DB-2

A DOSBox load of a mid-campaign autosave does not reproduce the continuous run
(save 5: 355 RNG draws vs 381).

Port, nine records (2026-10-19): RNG state + the `FUN_281f_04ca` reseed word,
stance `DS:0x9870` + rival strength `DS:0x9e98`, unit-slot compaction residue
(`ColonizeUnitPool.dos_tail`), the AI goal / work-queue / inventory / plan tables,
the rank cache `DS:0x917c` + profession census, the coarse fog plane `DS:0x9faa`,
Euro AI scratch (0a60 masks, explorers, village-visited bits, ship pressure,
sticky anti-spin), per-colony `plot_was_worked_mask`, and the pathfinder word
`DS:0x1dd2` + flood cache. The current list is the `RTST` row of
[savegame.md](../savegame.md) "Port extension block".

## DB-4

Some writers are deterministic: FUN_6662_00f2's `PUSH SI` (nation*0x13c, only when
00f2 is entered at SP e852 through 6d8e -> 0972 -> 0f74), the FUN_5bfb_022e met
arm (the mover's x), and the previous 20e6 scan's own saw-foe flag. The others are
RTLink overlay-loader residue (an overlay size or relocation word, e.g. 0x2fe0,
0x4232). Which overlays reload, and so the residue value, depends on the overlay
area's history since process start.

Evidence: america seed-100 save 63 -> 64 (England's Dragoon u61 beside Quebec
parks in the archive; a from-load parks all three Dragoons), and dozens of resync
pairs past it
([idle_america_fixed_2026-10-19_21.md](idle_america_fixed_2026-10-19_21.md), "e7c4
class").

## DB-14

Unit test cases of `unit_ai_king_core` `dump_goods_pick_api`: weighted 7:9 over
200 seeds, zero-weight pick over 50 seeds, exactly one draw burned in both,
campaign4 weights.

## DB-16

The same block fills every token of @KINGMERCY but never shows it, and VICEROY.EXE
has no KINGMERCY tag string (dead text), so the feature was removed on purpose.

## DB-23

The DOS evidence for the carry-over (america resync 129: Penobscot's walk to
Barbados ended at (44,39), so Providence was skipped, one `rng(0,2)` fewer than a
per-candidate reset draws; america2 resync 37) lies past the enabled golden saves
of both Americas archives.

## DB-25

(The DOS evidence for the wrap, america resyncs 105-119 and 151+, lies past the
enabled golden saves of both Americas archives and would need the old behaviour.)

## DB-28

The enabled goldens never complete a ring hop, so they do not exercise this (the
hop evidence in the code comment, america resync 92->93, is past the enabled
golden saves).

## DB-33

Only one golden buy happens (`golden_idle_america`, save 31, free-colonist count
0), so the goldens do not exercise a non-zero add.

## DB-35

A 2026-10-22 trial moved the test to the cleared tile (goldens passed), but the
user chose the DOS behaviour; the live DOS capture (+60 at Bahia) matches it.

## DB-37

The goldens reach this fallback only about once per campaign, with a ship, so they
barely exercise the change.

## DB-40

The input-replay regression `europe_buy_ship_and_sail` pinned the extra draw: its
`output.sav` was re-recorded from the port (only the 4 LCG-state bytes of the port
extension block differ).

## DB-43

User-observed 2026-10-22 (campaign5): Jamestown's fortress fired on a French
Caravel; England and France had relation byte 0 through turn 123.

## DB-46

The 5952 phantom Pioneer's order latch: DOS calls the work bodies directly there;
zeroing it broke every idle golden.

Unit case `unit_pioneer_dos_gates_2026_09_22` and the replay `map_unit_orders`
(its fixture preloaded the counters for an instant finish; re-blessed: the two
jobs now start at progress 1) were updated.
