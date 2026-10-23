# Second Americas idle campaign (`golden_idle_america2`)

STATUS: complete 2026-10-23. The port plays saves 000001..000012 (turn 80, 1572 .. turn 92, 1584)
byte-identical to DOS, continuously and with a save/reload before every turn. CTests
`golden_idle_america2` and `golden_idle_america2_reload` run `golden_idle_campaign america2 1 12`.
Save 13 turns on a DOS read of an uninitialised stack word ([dos_bugs.md](dos_bugs.md) DB-4), so
the archive was trimmed to 1..12. Triage and fix history:
[archive/idle_america2_fixed_2026-10-21_23.md](archive/idle_america2_fixed_2026-10-21_23.md) (grep
by `FUN_` name, save or resync pair number; do not read whole). Sister campaigns (done):
[idle_campaign.md](idle_campaign.md), [idle_independence.md](idle_independence.md),
[idle_america.md](idle_america.md), whose method, ignore list and DOSBox recipes apply here too.

## Fixtures

`original_saves/golden_idle_america_2-seed-100/` (readme.md there): VR_SOUND.EXE (seed 100),
original Americas map, Spanish human (`head.human_player = 2`), recorded after the first Americas
archive broke on the DB-4 residue. The archive is one continuous DOS session from a load of 000001;
the player only presses Space. The recording went on to `000406` (turn 507, 1799; every 20th
turn's autosave went to the decade slot, e.g. turn 88 between saves 8 and 9; the harness plays
through it). Saves 14..406 were removed in commit d87711a1 and save 13 on 2026-10-23; all are in
git history at `d87711a1^`.

## Running

```
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign america2 1 12
```

The whole archive takes under 1 s. Resync past the end works as in
[idle_america.md](idle_america.md) "Resync": restore the removed saves without staging them,
`git restore --source=d87711a1^ --worktree -- original_saves/golden_idle_america_2-seed-100/`,
raise `k_america2`'s max in `tests/golden/test_idle_campaign.c`, and delete them again afterwards.
Past save 12 the e7c4 residue changes outcomes in dozens of pairs; the triage oracle that forced
the park gate to the archive's outcome was deleted with the trim (archived log, "The e7c4
oracle", names the commit that has it).

## Save 13 (why the archive ends at 12)

Turn 93: England's Dragoon u61 beside Quebec border-parks ('F', plan 0x46) in DOS and wanders in
the port, the same unit and class as the first archive's save 64 ([idle_america.md](idle_america.md)).
The park arm of FUN_521d_20e6 reads the uninitialised `local_ea` (SS:e7c4); the port models only
its deterministic writers, and DOS's value is RTLink overlay-loader residue. With the archive's
outcome forced, 12 -> 13 is byte-identical.

A DOSBox from-load session starts with the unsaved tables at zero (stance DS:0x9870, goals
DS:0x98b0 + nation*0x100, rank DS:0x917c): write the port's RTST tables back after `load_slot`
before tracing (restored-load). A continuous DOSBox replay from 000001 already parks a different
Dragoon at save 2, so it is no oracle.

## Open leads (not reached by the trimmed archive)

From the archived log, all need the removed saves: save 46 (France's Merchantman from Europe
keeps its Pioneer in the port; DOS sails both Merchantmen home alone), resync 354 (explorer ring20
hop slot roll), 300 (history-dependent capture vs demotion), 105/113 (French hull goals after
Europe). The end-state comparison and the England/Tupi Pocahontas analysis (history divergence,
not a port bug) are in the archived log.
