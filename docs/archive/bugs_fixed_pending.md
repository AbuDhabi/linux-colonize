# Fixed / refuted bugs awaiting user verification

Rows moved here from [bugs.md](../../bugs.md) once an agent set FIXED or REFUTED. IDs are permanent; cite as `bugs.md #NNN`. User verifies, sets CLOSED, and moves the row to [bugs_closed.md](bugs_closed.md) (resolution text may be trimmed there; this file keeps the full text).

| # | Status | Bug Description | Resolution |
|---|--------|------------------|------------|
| 951 | FIXED | @NODOCKS popup (picking Fisherman on a water plot in a colony without Docks) was deferred until the colony screen closed instead of opening immediately. | 2026-09-26 The mouse/plot-click path in `game_loop_colony.c` (~795) enqueued the popup but never presented it, so `colony_zoom_popup_hold` kept it queued until the screen closed; the keyboard jobs-list path already called `game_colony_present_now`. Added the same `game_colony_present_now(game, AI_POPUP_TAG_INFO)` call after the enqueue. |
