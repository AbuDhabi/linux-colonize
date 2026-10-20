This is a user-maintained list of bugs. User puts reports in. Agents may annotate with a resolution (Status FIXED + one-sentence resolution) and then MOVE the row to [archive/bugs_fixed_pending.md](archive/bugs_fixed_pending.md) so this file stays small (OPEN rows only). User verifies rows there, then they are moved to [archive/bugs_closed.md](archive/bugs_closed.md).

IDs in the `#` column are permanent and never reused; the archive holds #1-#1085. **Next free ID: 1128.** Cite rows as `port_bugs.md #NNN`. Look an archived row up by grepping `| NNN |` in the archive file; never read `docs/archive/bugs_closed.md` whole.

Only actionable rows live here. An accepted divergence from DOS, or a static question blocked on a live DOS capture, belongs in [docs/known_divergences.md](docs/known_divergences.md) (it keeps its `#` id); a finding that resolves to "the port is right" is REFUTED into the pending archive. Never leave an unclosable row OPEN here.

Status: OPEN = no resolution yet (this file). FIXED / REFUTED = agent claims a fix or refutation, awaiting user verification (docs/archive/bugs_fixed_pending.md, full resolution text there). CLOSED / REFUTED = verified, archived.

| # | Status | Bug Description | Resolution |
|---|--------|------------------|------------|
| 1124 | OPEN | Colony screen BUY/CHANGE button colours: DOS FUN_2f2b_208c (viceroy_unpacked.c:48726, from FUN_2f2b_21da) draws the edges with literal palette indices 0x30/0x39 and text 0x0f (pressed: swapped, text 0, fill 0x0f); the port's RGB (20,40,120)/(180,200,255) resolve to WOODPANL.PIK 0x3d/0x32 (colony_screen_panels.c ~681). Pixel change, needs the user's eye: switch to the DOS indices and compare render_colony PPMs. Found 2026-10-18 while closing #1122. | |
