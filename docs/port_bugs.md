This is a user-maintained list of bugs. User puts reports in. Agents may annotate with a resolution (Status FIXED + one-sentence resolution) and then MOVE the row to [archive/bugs_fixed_pending.md](archive/bugs_fixed_pending.md) so this file stays small (OPEN rows only). User verifies rows there, then they are moved to [archive/bugs_closed.md](archive/bugs_closed.md).

IDs in the `#` column are permanent and never reused; the archive holds #1-#1361 (verified rows in `bugs_closed.md`, rows awaiting verification in `bugs_fixed_pending.md`). **Next free ID: 1362.** Cite rows as `port_bugs.md #NNN`. Look an archived row up by grepping `| NNN |` in the archive file; never read `docs/archive/bugs_closed.md` whole.

Only actionable rows live here. An accepted divergence from DOS, or a static question blocked on a live DOS capture, belongs in [docs/known_divergences.md](known_divergences.md) (it keeps its `#` id); a finding that resolves to "the port is right" is REFUTED into the pending archive. Never leave an unclosable row OPEN here.

Status: OPEN = no resolution yet (this file). FIXED / REFUTED = agent claims a fix or refutation, awaiting user verification (docs/archive/bugs_fixed_pending.md, full resolution text there). CLOSED / REFUTED = verified, archived.

| # | Status | Bug Description | Resolution |
|---|--------|------------------|------------|
| 1323 | OPEN | Static lead (fandom wiki check 2026-10-30, not DOSBox-verified): a move into an undefended foreign colony is DOS's bVar4 settlement arm (`FUN_465b_0000` raw 75467), so the LAB_465b_05ca gate is `cost <= left \|\| spent == 0 \|\| (04ca reseed, true)`: no partial-MP roll, no step-cost charge (`!bVar4` block), and a reseed before 0a14 when short. The port sets `foreign_colony_entry` (non-human only) but `units_move_step_gate` still charges the cost and rolls `rng(1,cost) <= remaining` for it, and `units_move_colony_entry` has no reseed before the Revere fight. Reachable for an AI with >=3 thirds left entering a colony tile that costs more (no road on the origin), and for a human (flag not set at all). Needs a DOSBox capture before changing. | |
