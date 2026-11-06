# Fixed / refuted bugs awaiting user verification

Rows moved here from [port_bugs.md](../port_bugs.md) once an agent set FIXED or REFUTED. IDs are permanent; cite as `port_bugs.md #NNN`. User verifies, sets CLOSED, and moves the row to [bugs_closed.md](bugs_closed.md) (resolution text may be trimmed there; this file keeps the full text).

| # | Status | Bug Description | Resolution |
|---|--------|------------------|------------|
| 1451 | FIXED | Goto coarse walkability grid (`s_units_coarse`, units_move.c) survived New Game / Load: `units_coarse_build` keys the cache on map pointer + size, and the game reuses one `world_map` struct at the same size, so the next world pathed on the previous one's land/water cells (unit_ai: AI fleet landed at (2,2) after an AMER2 game in the same process). | 2026-11-06: `units_reset_state` (called by ai.c new-game reset and game_loop_saveload.c load) now drops the cache via `units_coarse_invalidate`. Guard: `unit_ai` case `case_new_world_after_america` (mutation-checked). |
