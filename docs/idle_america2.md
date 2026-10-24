# Second Americas idle campaign (`golden_idle_america2`)

STATUS: complete 2026-10-23. The port plays saves 000001..000014 (turn 80, 1572 .. turn 94, 1586)
byte-identical to DOS, continuously and with a save/reload before every turn. CTests
`golden_idle_america2` and `golden_idle_america2_reload` run `golden_idle_campaign america2 1 14`.
Save 15 turns on a DOS read of an uninitialised stack word ([dos_bugs.md](dos_bugs.md) DB-4), so
the archive ends at 14 (saves 13 and 14 restored 2026-10-23 after the hull-step writer,
port_bugs.md #1135). Triage and fix history:
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
through it). Saves 15..406 were removed in commit d87711a1; all are in git history at
`d87711a1^`.

## Running

```
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign america2 1 14
```

The whole archive takes under 1 s. Resync past the end works as in
[idle_america.md](idle_america.md) "Resync": restore the removed saves without staging them,
`git restore --source=d87711a1^ --worktree -- original_saves/golden_idle_america_2-seed-100/`,
raise `k_america2`'s max in `tests/golden/test_idle_campaign.c`, and delete them again afterwards.
Past save 14 the e7c4 residue changes outcomes in dozens of pairs; the triage oracle that forced
the park gate to the archive's outcome was deleted with the trim (archived log, "The e7c4
oracle", names the commit that has it).

## Save 15 (why the archive ends at 14)

Save 13 (turn 93: England's Dragoon u61 beside Quebec border-parks) became exact with the
hull-step writer: the English ship's step off a shared tile leaves 0x020e at SS:e7c4.

Turn 95: France's Dragoon u56, parked 'F' at (46,41) beside an English colony, un-parks and walks
in DOS; the port re-parks it on its modelled word (France's flood, 0x13c). DOSBox from-load 14,
`BPM` on e7c4: after the French Merchantman's walk the last writer is a visible-move redraw
(FUN_112b_01ba unit-icon draw -> FUN_4cc6_03f8 zeroing its local array, e7c4 = `[bp-0x1c]` at
BP e7e0), not game state, so the word is 0 at the Dragoon's 20e6 entry. Not modelled (DB-4).

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
