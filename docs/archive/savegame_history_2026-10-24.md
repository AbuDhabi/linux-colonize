# savegame.md history (archived 2026-10-24)

STATUS: archived 2026-10-24 from docs/savegame.md — history, not a spec. The living doc is [savegame.md](../savegame.md). Lookup keys: W1.5, `founding_fathers_stash_*`, `FF_POOL_STASH_MARKER`.

## Phase 5 codec drift (resolved 2026-08-24, W1.5)

**Phase 5 codec drift — resolved (2026-08-24, W1.5).** A 2026-08-22 pass
found lategame `valid-lategame-saves/COLONY{00–08,10}.SAV` and AI
`TURN1`/`TURN5`–`TURN7` not byte-identical on re-encode and documented it
here as open drift. Unnoticed at the time: the *same-day* commit `753662d`
"Fix FF + I work" (2026-08-22, 42 minutes after the drift note was written)
had already fixed it — `founding_fathers_stash_pools_into_col1`/
`_restore_col1_last_turn` (both since retired: the FF bell pool now lives in
col1's `liberty_bells_pool`, so there is nothing to stash) gained a
`saved_pad21` parameter that stashes and
restores nation `unknown21_pad` (the `FF_POOL_STASH_MARKER` byte) alongside
`liberty_bells_last_turn`; before that fix, a save write that stashed the FF
pool into `liberty_bells_last_turn` left `unknown21_pad` un-restored, so a
second write of the same in-memory save produced different bytes there.
Only the doc note calling it "drift" survived uncorrected. W1.5 re-verified
with a full-range byte diff (not just first-offset) across all 19 fixtures
(2 starters + 10 lategame + 7 TURN) plus `mapgen/SEED100.SAV` and
`tests-save-misc/unit flags error.sav`: zero differing bytes anywhere.
`unit_col1_save`'s `report_codec_roundtrip_diff` was also upgraded
same-pass to print every contiguous diff range (was: first byte only) —
kept as the tool for any future fixture that does regress.
