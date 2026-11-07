This is a user-maintained list of bugs. User puts reports in. Agents may annotate with a resolution (Status FIXED + one-sentence resolution) and then MOVE the row to [archive/bugs_fixed_pending.md](archive/bugs_fixed_pending.md) so this file stays small (OPEN rows only). User verifies rows there, then they are moved to [archive/bugs_closed.md](archive/bugs_closed.md).

IDs in the `#` column are permanent and never reused; the archive holds #1-#1364 (verified rows in `bugs_closed.md`, rows awaiting verification in `bugs_fixed_pending.md`). **Next free ID: 1564.** Cite rows as `port_bugs.md #NNN`. Look an archived row up by grepping `| NNN |` in the archive file; never read `docs/archive/bugs_closed.md` whole.

Only actionable rows live here. An accepted divergence from DOS, or a static question blocked on a live DOS capture, belongs in [docs/known_divergences.md](known_divergences.md) (it keeps its `#` id); a finding that resolves to "the port is right" is REFUTED into the pending archive. Never leave an unclosable row OPEN here.

Status: OPEN = no resolution yet (this file). FIXED / REFUTED = agent claims a fix or refutation, awaiting user verification (docs/archive/bugs_fixed_pending.md, full resolution text there). CLOSED / REFUTED = verified, archived.

| # | Status | Bug Description | Resolution |
|---|--------|------------------|------------|
| 1556 | OPEN | [audit 2026-11-06 N9, low] Europe key / menu under Complete Map: DOS (asm 0x236B2-0x2371A) shows @EUROPENOTAVAIL only for WoI && show_entire_map == 0; with show_entire_map != 0 it raises a DEBUG @SETEUROPE nation pick (MSS 4, default = current view + 1) and opens that nation's Europe (`FUN_281f_05fa(pick-1, -1)`); otherwise it opens fixed_nation_map_view's (DS:0x53a4) Europe when >= 0. The port gates on WoI alone and opens only the human's Europe (game_dialogs.c:127/217). Needs a Europe view of another nation (mirror rebind like Set Human). | |
