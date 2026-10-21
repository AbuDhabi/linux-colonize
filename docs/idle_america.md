# Americas idle campaign (`golden_idle_america`)

STATUS: complete 2026-10-21. The port plays saves 000001..000063 (turn 9, 1501 .. turn 77, 1569)
byte-identical to DOS, continuously and with a save/reload before every turn. CTests
`golden_idle_america` and `golden_idle_america_reload` run `golden_idle_campaign america 1 63`.
Save 64 turns on a DOS read of an uninitialised stack word ([dos_bugs.md](dos_bugs.md) DB-4), so
the archive was trimmed to 1..63. Fix history:
[archive/idle_america_fixed_2026-10-19_21.md](archive/idle_america_fixed_2026-10-19_21.md) (grep
by `FUN_` name, save or resync pair number; do not read whole). Sister campaigns (done):
[idle_campaign.md](idle_campaign.md), [idle_independence.md](idle_independence.md), whose method,
ignore list and DOSBox recipes apply here too.

## Fixtures

`original_saves/golden_idle_america_seed-100/` (readme.md there): VR_SOUND.EXE (seed 100, no
music picks), original Americas map, Spanish human (`head.human_player = 2`). Saves
`000001`..`000063` are every-turn autosaves (COLONY09). Turns whose autosave went to the decade
slot are missing (for example turn 78); the harness plays through them. The archive is one
continuous DOS session from a load of 000001; the player only presses Space. The recording went
on to `000465` (turn 507, 1799); saves 64..465 were removed after commit aa5854a9 and are in git
history at `c9c2eacb^`.

Human slot 2 means England (0) and France (1) act after the day-top spent-MP clear and before
the autosave, so their spent bytes are compared; the Netherlands (3) act after it.

## Running

```
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign america 1 63
./build/debug/golden_idle_campaign america N-1 N
```

The whole archive takes under 2 s. Single pairs from a DOS save are hints only: a load drops
unsaved DOS state (DB-2), so only the continuous run from save 1 is the gate.

### Resync (triage past the first divergence)

```
GOLDEN_IDLE_RESYNC=1 GOLDEN_IDLE_RESYNC_SAVE=/tmp/x/start_%d.sav GOLDEN_IDLE_KEEP_GOING=1 \
  GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign america 1 63
GOLDEN_IDLE_FROM=/tmp/x/start_K.sav ./build/debug/golden_idle_campaign america K N   # one pair
```

Before every step from an archived save K the harness swaps the run's DOS sections for save K
and keeps the run's own COLNXEXT tail (RTST: stance and goal tables, unit tail bytes, path and
rank caches, fog, LCG). Each pair is then judged from DOS's state plus the port's best model of
the unsaved tables, so a diff names a bug of that turn, not the cascade of an earlier one. Every
nation slot reseeds the LCG (DS:0x83a6 = 100 under VR_SOUND), so the carried LCG does not matter
and a replay from `start_K.sav` reproduces the pair. This is how the pass ran past save 64 (final
full resync 1..465, 2026-10-21: 243 of 464 pairs byte-identical, 67 more artifact- or park-only).
To re-run it, restore the removed saves without staging them:
`git restore --source=c9c2eacb^ --worktree -- original_saves/golden_idle_america_seed-100/`, and
raise `k_america`'s max in `tests/golden/test_idle_campaign.c`; delete them again afterwards.

Resync artifacts, not bugs: bytes DOS keeps in memory past `unit_count` are not in the save, so a
unit created this turn into a reused slot carries the port's guess of the previous occupant's
leftovers (goto, facing, holds at or past `holds_occupied`, hold[5] 100/80/236/216 on non-
Pioneers); `.units[].vis_mask` and `.map.path_hex` diffs are mostly downstream of a unit that
moved differently.

## Save 64 (why the archive ends at 63)

Turn 79 (no save for 78): England's Dragoon u61 beside Quebec border-parks ('F') in DOS and
wanders in the port; France's u80 then gets a LABOR goal (0a60's 047c probe no longer sees the
parked Dragoon) and Quebec's build follows. The park arm reads 20e6's uninitialised `local_ea`
(SS:e7c4). The port models its deterministic writers; the value in the archive (0x4232) is
RTLink overlay-loader residue from the ship u70's 0972 walk reloading OVL20 over OVL14, which
depends on overlay history since process start. A from-load DOSBox session evicts differently
(dt64c parked all three Dragoons), so no trace can settle it. Full analysis: the save-64 row of
the archived log and [dos_trace.md](dos_trace.md) "Uninitialised-local hunts".

## Open leads (not reached by the trimmed archive)

- port_bugs.md #1135: one more deterministic e7c4 writer after a case-7 founding (resync 106).
- Archived rows marked "(open)": resync 206 (Scout 4d2e step), 232 (combat strengths), 283
  (birth at the unit cap), 292/316/376 (decision cascades), 435 (England's horse demand); plus
  the archive's "Open leads" list (reload gaps, 317c's DS:0x8dc4 write). All need the removed
  saves (see Resync).
