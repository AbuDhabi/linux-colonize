This is a user-maintained list of bugs. User puts reports in. Agents may annotate with a resolution (Status FIXED + one-sentence resolution) and then MOVE the row to [archive/bugs_fixed_pending.md](archive/bugs_fixed_pending.md) so this file stays small (OPEN rows only). User verifies rows there, then they are moved to [archive/bugs_closed.md](archive/bugs_closed.md).

IDs in the `#` column are permanent and never reused; the archive holds #1-#1364 (verified rows in `bugs_closed.md`, rows awaiting verification in `bugs_fixed_pending.md`). **Next free ID: 1432.** Cite rows as `port_bugs.md #NNN`. Look an archived row up by grepping `| NNN |` in the archive file; never read `docs/archive/bugs_closed.md` whole.

Only actionable rows live here. An accepted divergence from DOS, or a static question blocked on a live DOS capture, belongs in [docs/known_divergences.md](known_divergences.md) (it keeps its `#` id); a finding that resolves to "the port is right" is REFUTED into the pending archive. Never leave an unclosable row OPEN here.

Status: OPEN = no resolution yet (this file). FIXED / REFUTED = agent claims a fix or refutation, awaiting user verification (docs/archive/bugs_fixed_pending.md, full resolution text there). CLOSED / REFUTED = verified, archived.

| # | Status | Bug Description | Resolution |
|---|--------|------------------|------------|
| 1431 | OPEN | The F10 score during play counts units from game->col1.unit[], which only a save capture refreshes, so a unit lost or gained since the last load or save still scores. DOS FUN_41f2_0092's unit loop (asm 41f2:0329-0345) reads the one live unit table. Colonies are already read live (reports_score_collect_citizen_jobs), and the retire path captures first (game_open_retire_score). Fix: a live unit walk in the capture's record order (the stale `scored` local makes unit points order-dependent), covering the off-pool Europe dock rows and harbor ships. From the PR #8 notes. | |
