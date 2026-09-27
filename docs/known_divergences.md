# Known divergences from DOS

STATUS: living record, not an audit dump. Authoritative.

Deliberate, recorded differences between this port and VICEROY.EXE that nobody
intends to change, plus static questions that are blocked on a live DOS capture
rather than on code. These are **not** bugs and must never be filed in
[bugs.md](../bugs.md), which holds actionable OPEN rows only. Each entry keeps
its original `bugs.md #` id so existing citations (`bugs.md #NNN`) still resolve;
ids are permanent and never reused.

When working a feature, grep this file for that feature's name before "fixing"
something that looks wrong — several entries below exist precisely because the
DOS-literal write would be worse in the port's data model.

## Accepted divergences (do not change)

### #665 — REF demote runs the equip sync
The demote arm calls `units_sync_equip_after_type_change` after the type write
(`units.c` ~3220); DOS `FUN_5fef_0352` raw 99437-99464 writes only `+0x3146`.
Harmless for Cavalry->Regulars (sprite and name derive from type); required by
the port's armed-colonist body model for Dragoons->Soldiers. Leave unless kit
bytes ever feed a REF display.

### #780 — 5952 ARM 1 profession write is guarded
ARM 1 guards the profession write with `if (elected >= 0)`
(`ai_euro_colony_jobs.c`:2429-2434); DOS raw 95908-95911 writes unconditionally,
so an idle-occupation pick gets profession 0xff. Rare — ARM 1 runs after all
placement passes. The literal write would put a -1 profession into the port's
colonist record, which other readers treat as unset. Leave unless a DOS save
shows 0xff colonist bytes.

### #908 — Colonist jobs menu is narrower than FUN_2f2b_348c
DOS's jobs menu also (a) lists the leave-as rows @JOB 0x13..0x18 in the same
list (the port keeps them in the eject popup only,
`colonies_eject_row_offered`), (b) prints a production estimate on indoor rows
9..0x11 from the `aiStack_12c` table filled at the head of 348c (the port shows
the name only), and (c) pages the list past 16 rows (`local_62 = 2`,
`local_d4 = 0x10`, 0x62 "More" button; the port stops drawing at the frame edge).

(b) and (c) are real DOS features, deferred by cost, not refuted:

- (b) raw 50762-50775 shows the printed string is not just `(n)` — it is
  `FUN_1d1d_07a4(local_5e,0xca8)` plus, for field rows < 9, an extra
  "n each"-style fragment via resource 0xcac, plus the number
  (`aiStack_12c[row]`), plus a goods-name suffix looked up via
  `aiStack_c8[row]*2-0x6840`. `aiStack_c8` for indoor rows (>= 9) is itself
  filled by `FUN_281f_0cd6` (raw ~50726), not a simple constant, so porting it
  needs that good-id resolution plus a goods-name catalog lookup — not just the
  existing `colony_yield_for_worker` number, and bigger than the row note at
  `colony_screen_popups.c`:334-345 assumed.
- (c) DOS reflows the same `job_ids` list at a `local_6` page toggle (0/1) with a
  literal "More" / page-2 label. `ColonyJobsView`
  (`src/core/colony_screen.h`:304-308) has no page-offset field and
  `game_loop_colony.c` has no extra-row click handler. A port needs a
  `jobs_page` int in the view struct, a 17th pseudo-row hit-test in the click
  handler, and reflowing `job_ids[]` by `page * local_d4` in
  `colony_screen_popups.c`'s draw loop (~311-318).

Promote either back to bugs.md only if the user asks for the feature.

## Blocked on a live DOS capture

Static reading cannot settle these. Each names exactly what to capture. Do not
change the port's behaviour on a guess.

### #909 — Craft audit statics
- The origin of the skill-match flag `[bp-0x1c]` is unlocated.
- Train dialog row order for the 1000/1100/1200 ties
  (`FUN_291f_0ed0` = 210d:0d91 sort stub unread).
- University-without-College seating of a level-2 teacher: DOS tests 8bec(0xd)
  only, the port's `colonies_school_owned_tier` allows it. Unreachable by normal
  play.
- @TRAINFAIL %STRING0 in DOS is whatever token 0 held from a previous phase
  (0416 sets it only inside the graduation arm); the port fills the current
  colony.

### #915(c) — 5952 indoor pass `avail` carry-over
Seed `0x7fff` at `ai_euro_colony_jobs.c` ~1015 still needs a live capture.
Parts (a) and (b) of the original #915 bundle were REFUTED — see
[bugs_fixed_pending.md](archive/bugs_fixed_pending.md).
