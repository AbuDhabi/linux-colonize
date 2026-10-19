# Archive

Superseded docs kept for history, not as living references. Consult only
when tracing why something is the way it is; don't cite as current fact.

**Read discipline (agents):** never open a file here to "get context", to
browse for work, or to check whether something was already done — the living
docs and `port_bugs.md` answer that, and these files are large enough to eat a
context window on their own. Open one only with a concrete lookup key (a bug
`#` id, a `FUN_ssss_oooo` name, an audit item number, a symbol) and read only
the matching lines: `grep -n '| NNN |' docs/archive/bugs_closed.md`,
`grep -n FUN_5952_035e docs/archive/<file>.md`, then `sed -n 'A,Bp'`. If the
grep misses, the answer is not here — stop, don't scan the file. Nothing in
this directory is a spec; a conflict with a living doc is resolved in the
living doc's favour.

| File | What it is | Archived | Consult when |
|------|-----------|----------|--------------|
| [bugs_fixed_pending.md](bugs_fixed_pending.md) | FIXED / REFUTED bug rows awaiting user verification (full resolution text) | rolling | Agents move rows here from `port_bugs.md` on FIXED; user closes from here |
| [bugs_closed.md](bugs_closed.md) | User-verified CLOSED / REFUTED bug rows with full resolution text (#1-#950) | rolling | Looking up one bug id you already have: `grep -n '| NNN |'`. Never read whole |
| [idle_campaign_fixed_2026-10-06_08.md](idle_campaign_fixed_2026-10-06_08.md) | Idle-campaign gate fix log, passes 2026-10-06..08 (split from docs/idle_campaign.md) | 2026-10-12 | Tracing why an idle-campaign fix was made: grep by FUN_ name / transition |
| [idle_campaign_fixed_2026-10-10.md](idle_campaign_fixed_2026-10-10.md) | Idle-campaign gate fix log, 2026-10-10 pass (split from docs/idle_campaign.md) | 2026-10-13 | Tracing why an idle-campaign fix was made: grep by FUN_ name / transition |
| [idle_campaign_fixed_2026-10-10_14.md](idle_campaign_fixed_2026-10-10_14.md) | Idle-campaign gate fix log 2026-10-10..14 (passes, two-turn targets, continuous run) and retired open leads; archived when the gate completed | 2026-10-14 | Tracing why an idle-campaign fix was made: grep by FUN_ name / year |
| [idle_independence_fixed_2026-10-14_19.md](idle_independence_fixed_2026-10-14_19.md) | Independence idle campaign fix log 2026-10-14..19 (from-load references, tr9/tr10 passes, re-recorded archive intervals), split from docs/idle_independence.md when the campaign completed | 2026-10-19 | Tracing why an independence-campaign fix was made: grep by FUN_ name / interval / save |
| [bugs_resolutions_full_2026-09-05.md](bugs_resolutions_full_2026-09-05.md) | Full user bug list with agent resolutions, pre-2026-09-05 | 2026-09-05 | Looking up how an old bug row was diagnosed/fixed; current bugs live in `port_bugs.md` |
| [mysteries_catalog.md](mysteries_catalog.md) | Catalog of DS globals/flags whose meaning was unresolved as of 2026-08-19 | ~2026-08-27 (closed statically per MEMORY) | Investigating an "unknownNN" field name still lingering in code/saves |
| [port_plan_w_tier_archive.md](port_plan_w_tier_archive.md) | Pre-2026-08-24 W-tier work queue, verbatim from old `port_plan.md` | 2026-09-05 | Mapping an old `W*.*` citation elsewhere in the repo to its current track in `port_plan.md` |
| [smell_audit_2026-09-08.md](smell_audit_2026-09-08.md) | First mechanics-smell sweep dump (107 findings, inline FIXED/REFUTED/WONTFIX notes) | 2026-10-18 (closed 2026-09-09) | Citations like "smell_audit_2026-09-08 #N" |
| [smell_audit_2026-09-09.md](smell_audit_2026-09-09.md) | Second mechanics-smell sweep dump (106 findings; outcomes mostly in commits, not inline) | 2026-10-18 (closed 2026-09-10) | Citations like "smell_audit_2026-09-09 #N" in `src/core` comments |
| [smell_audit_2026-09-10.md](smell_audit_2026-09-10.md) | Third mechanics-smell sweep dump (106 findings) | 2026-09-23 (findings closed 2026-09-10 through 2026-09-15) | Tracing why a fix exists; citations like "smell_audit_2026-09-10 item N" |
| [ai_euro_uncited_arms_2026-09-18.md](ai_euro_uncited_arms_2026-09-18.md) | Triage list of manual/fandom-cited arms left in the pre-split `ai_euro.c` (36 functions) with the two 2026-09-18 resolution waves | 2026-10-18 (superseded by port_bugs.md #1034 / #1105) | Checking whether an AI arm was deleted as invented and under which bug id |
| [indians_port_snapshots.md](indians_port_snapshots.md) | Indian-contact port development snapshots (2026-08-27 single alarm store; 2026-09-06d 152e/1816/1b3a deepening); was `docs/indians_port.md` | 2026-10-18 | Why the alarm store was consolidated; `indians.md` / `indian_alarm.md` are the living docs |
| [duplication_audit_2026-09-14.md](duplication_audit_2026-09-14.md) | Code duplication audit + Resolution/Round-2 fix tables | 2026-09-23 (findings closed 2026-09-14/15) | Checking whether a duplicate-looking pair was "kept split on DOS grounds" |
| [port_plan_deferred_ai_track.md](port_plan_deferred_ai_track.md) | "Deferred AI track — detail" section moved out of `port_plan.md` | 2026-09-23 | Reading full history of the AI porting track; current open items are summarized in `port_plan.md` |
