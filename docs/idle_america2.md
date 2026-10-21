# Second Americas idle campaign (`golden_idle_america2`)

STATUS: being aligned (started 2026-10-21). CTests `golden_idle_america2` and
`golden_idle_america2_reload` (`golden_idle_campaign america2 1 406`) are `DISABLED` in
CMakeLists.txt until the continuous run passes. Sister campaigns (done): [idle_campaign.md](idle_campaign.md),
[idle_independence.md](idle_independence.md), [idle_america.md](idle_america.md); their method,
ignore list and DOSBox recipes apply here too.

## Fixtures

`original_saves/golden_idle_america_2-seed-100/` (readme.md there): VR_SOUND.EXE (seed 100),
original Americas map, Spanish human (`head.human_player = 2`), recorded after the first Americas
archive broke on the DB-4 stack residue ([dos_bugs.md](dos_bugs.md)). Saves `000001`..`000406`
(turn 80, 1572 .. turn 507, 1799) are every-turn autosaves from one DOS session that loaded
000001 and pressed Space. Every 20th turn's autosave went to the decade slot (turn 88, 98, ...,
488): the harness plays through the gap (`steps = 2`).

## Running

```
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign america2 1 406
GOLDEN_IDLE_E7C4_ORACLE=1 GOLDEN_IDLE_RESYNC=1 GOLDEN_IDLE_RESYNC_SAVE=/tmp/x/start_%d.sav \
  GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign america2 1 406
```

The whole archive takes about 4.5 minutes continuous (late saves carry ~300 units). Resync
(docs/idle_america.md "Resync") judges each pair from DOS's saved state plus the port's own
unsaved tables.

### The e7c4 oracle (triage only)

The DB-4 border park (FUN_521d_20e6's 0x46 arm reading the uninitialised SS:e7c4) decides park or
wander for a combat land unit alone beside a foreign colony. In this archive it first changes the
outcome at save 13 (England's Dragoon beside Quebec parks in DOS), and dozens of later pairs carry
it. `GOLDEN_IDLE_E7C4_ORACLE=1` makes that gate (and only that gate, after its three pure tests
pass) follow the archive: park when the next save shows a unit of that nation on that tile with
plan 0x46 and no orders (`ai_euro_e7c4_oracle`, `tests/golden/test_idle_campaign.c`). It keeps the
residue from hiding real bugs; it is not a gate.

A DOSBox replay of the archive is no oracle either: a continuous DOSBox session from a load of
000001 already parks a different Dragoon at save 2 (overlay history differs from the recording's
session).

### Restored-load DOSBox traces

A from-load DOSBox session starts with the unsaved tables at zero (stance DS:0x9870, goals
DS:0x98b0 + nation*0x100, rank DS:0x917c), so its first turn is not the archive's: from-load 17
registers New Amsterdam on a zero stance and sends an empty Merchantman on a work-queue haul the
archive never made. Write the port's tables (the RTST chunk of a resync start) back after
`load_slot` before tracing.

## Fixed so far (2026-10-21)

- Save 16: FUN_1427_06b4 stamps `+0x3159 = 100` on every new Pioneer, also in a slot no
  compaction reached; the export no longer zeroes a non-Pioneer's hold[5] <= 100 (DOS keeps the
  byte a unit created over a compaction copy inherits).
- Resync 76, 90, 106, 130 class: units off the map export their live visibility nibble. 007a's
  08da (FUN_1427_0968) clears it for a hull sailing to Europe and its passengers; 06b4 sets none
  off the map; a hull 5fef_0352 parked for repair keeps its map mask through the repair and the
  voyage back.

## Open

- Save 18 (turns 98 + 99): first continuous split after the oracle; under investigation.
