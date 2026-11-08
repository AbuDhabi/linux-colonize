This is a user-maintained list of bugs. User puts reports in. Agents may annotate with a resolution (Status FIXED + one-sentence resolution) and then MOVE the row to [archive/bugs_fixed_pending.md](archive/bugs_fixed_pending.md) so this file stays small (OPEN rows only). User verifies rows there, then they are moved to [archive/bugs_closed.md](archive/bugs_closed.md).

IDs in the `#` column are permanent and never reused; the archive holds #1-#1364 (verified rows in `bugs_closed.md`, rows awaiting verification in `bugs_fixed_pending.md`). **Next free ID: 1609.** Cite rows as `port_bugs.md #NNN`. Look an archived row up by grepping `| NNN |` in the archive file; never read `docs/archive/bugs_closed.md` whole.

Only actionable rows live here. An accepted divergence from DOS, or a static question blocked on a live DOS capture, belongs in [docs/known_divergences.md](known_divergences.md) (it keeps its `#` id); a finding that resolves to "the port is right" is REFUTED into the pending archive. Never leave an unclosable row OPEN here.

Status: OPEN = no resolution yet (this file). FIXED / REFUTED = agent claims a fix or refutation, awaiting user verification (docs/archive/bugs_fixed_pending.md, full resolution text there). CLOSED / REFUTED = verified, archived.

| # | Status | Bug Description | Resolution |
|---|--------|------------------|------------|
| 1607 | OPEN | (leaf fidelity review 2026-11-08, residue of #1584) Message-bar strip clear: DOS FUN_1009_00b4 also runs FUN_1009_0036 on the shown line when it clears the strip, which may cut the last line of a run to now+0x1e ticks. The port keeps the last line for the full 0x78-tick dwell (1971 ms) in `ai_popup_bar_service` (src/core/ai_popup.c). Check which callers reach 00b4 while a line is up before porting. | |
