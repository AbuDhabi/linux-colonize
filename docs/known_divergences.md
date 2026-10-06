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

### #1081 — Unit-stack list and map sidebar use the WoI nation words
DOS `FUN_2b5a_1b5a` (stack list, raw 42732) and the `FUN_49dd_0424` unit
line index the raw @NATIONALITY / @NATIONABBREV tables, so after the
declaration DOS still prints "Spanish" ("Span." on the sidebar). The port
uses `reports_nation_adjective_woi` / `map_panel_nationality` ("Rebel" /
"Tory") on both, so they agree with the combat and popup text that DOS
itself routes through `FUN_281f_09a4` (user request 2026-10-06, #202).

### #1082 — WoI nation words in DOS texts the port has no counterpart for
DOS names a Euro nation via `FUN_281f_09a4` / `0a1a` (so "Rebel(s)" / "Tory/Tories"
after the declaration) in texts the port never draws: sidebar village mission-owner
and threat lines (`FUN_49dd_0424` raw 501/516), OVL10 native-village panel
(asm 119456-119903), Euro-vs-Euro colony burn @BURNED/@BURNED2/@BURNED3
(`FUN_5fef_1b0e` raw 100857), @CARGOCAPTURE (asm 153592), @LOSTTHEIRSCOUTS
(`FUN_5f7a_000e` raw 98871), @INDIANCOMMENT (raw 96981), Europe status line
`FUN_38fd_1bd2` raw 60109 (no port counterpart found; `europe_dock_caption` is the
separate 3694 site), 153e USA talk variants (asm 149562/149586/150456; see ai_diplo.c
"USA text variants"), unresolved popup at OVL12 asm 122653. If any is ever ported use
`reports_nation_adjective_woi` (09a4) / `reports_nation_plural_woi` (0a1a). The
colony-screen tribal-land demand `FUN_2f2b_2f3e` (2f2b:326a) lives in colony_screen_*
(other agent) and was not touched. Port sites that exist (combat, ai_diplo talk,
ai_contact) already route through the WoI accessors.

## Blocked on a live DOS capture

Static reading cannot settle these. Each names exactly what to capture. Do not
change the port's behaviour on a guess.

### #1009 — `@ATTITUDE` / `@ATTITUDINAL` have no located consumer
Alarm never reaches the player as words. `NAMES.TXT @ATTITUDE` (Content /
Uneasy / Restless / Angry / War) and `@ATTITUDINAL` (Extremely / Very / Rather
/ Somewhat / Slightly) are shipped catalogs that no port code reads, and
2026-10-01 established that **no decompiled DOS function reads them either**:
`FUN_75c2_10ae` (`viceroy_unpacked.c:121168-121184`) loads `@ATTITUDE`
(DS tag 0x2273) into `DS:0x9348..0x9351` and `@ATTITUDINAL` (DS tag 0x227c)
into `DS:0x9352..0x935b`, and every word offset of both arrays has zero reads
across all three decompiled files. The control is the `@LEVELS` table
(`DS:0x5230`), loaded a few lines earlier by the same function, which *is* read
back repeatedly in `viceroy_overlays.c` — so the sweep does surface real
consumers. Leads checked and empty: all ~25 `FUN_281f_030c` (alarm read) call
sites, and `FUN_281f_0a60`, which is a two-line thunk unrelated to NAMES.TXT.
The port's own alarm displays stay as they are: the F9 Indian Adviser's
`@LEVELS` tech word plus the `113 + alarm quartile` chief portrait
(`reports_indian_build_rows`, DOS-cited `3f41:0522..05d2`), the map sidebar
tier glyph (`FUN_112b_0790`), and the village-meet body band
(@VILLAGEHAPPY / MEDIUM / BAD / WAR).

**Capture:** DOSBox-X watchpoint on `DS:0x9348-0x935b` through a session that
reaches several alarm tiers, to see whether anything reads either array and on
which screen. Until then do not invent a threshold table — 25 gradations only
*suggest* modifier x label.

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

### #974 — F7 Naval: Europe-lane ships are EuropeScreen rows, not unit rows
DOS `FUN_3f41_220c` has no Europe special case: a ship waiting in port or
mid-Atlantic is an ordinary unit record on the nation's sentinel diagonal
(236+n in port, 232+n / 228+n westbound, 244+n / 240+n eastbound — see
`col1_bridge.c`'s lane notes), so it is listed by the same array pass as every
other ship. Its Location comes from the shared namer `FUN_291f_0f82`
(= `FUN_49dd_02d0`) applied to the sentinel coordinates, and its Destination
from four x-delta arms (asm OVL06_L0040 0027d4-00283c): `nation - x` of 0x18 or
0x1c takes the New World name at `DS:0x5426 + nation*0x34`, 0x0c or 0x10 takes
the Europe port name from the pointer table at `DS:0x838c + nation*2`, and
anything else draws an empty cell. A unit standing on a real map tile without a
goto order gets no Destination at all (`FUN_281f_0302(x, y) != 0` skips it).

The port cannot reproduce this as written: `col1_bridge_apply` consumes those
sentinel unit records into `EuropeScreen`'s harbor/expected/bound lists, so they
are not in the unit pool for `reports_naval_build_rows` to walk. The report
rebuilds them from those lists instead and labels them "High Seas" (@MISC 60) /
`europe->port_city` / `europe->colony_region` — the same two names DOS's two
arms resolve to, reached a different way, plus a Location string DOS would
have taken from the sentinel tile.

What is actually unknown is only that Location: what `FUN_49dd_02d0` prints for
an off-map sentinel coordinate. On-map it is settled and the port matches —
`naval.png` shows "New Amsterdam" for a colony tile and "(51, 48)" for open
water, which is exactly `reports_naval_location`. Settling the off-map case
needs a live DOS capture of F7 with a ship in port and one mid-ocean; until
then, re-plumbing the lanes would trade a working display for a guess.

Two claims filed with the original #974 were resolved rather than accepted:
the Location/Destination *formats* were called port inventions and are not
(golden-confirmed above), and the Destination order test is now the DOS triple
3 / 0xb / 2 rather than the shared `units_orders_follow_goto`, which also
accepts 12 (AI_MOVE) — fixed 2026-09-28 with [#973](archive/bugs_fixed_pending.md).

### #1018 — Europe buy with insufficient gold: partial fill vs outright reject
`FUN_38fd_1fa2`'s committed-buy arm (viceroy_unpacked.c raw 60377-60503, `param_4 != 0`)
clamps the requested amount to the hold's room (100 max), computes
`price * amount`, compares it against the treasury via `FUN_281f_0a92`, and on
`gold < cost` raises the insufficient-funds popup and returns **without buying
anything** — no partial fill. The port's `europe_buy_cargo_w`
(src/core/europe_market.c:793-882) instead computes `can_afford = eu->gold / ask`
and silently clamps the purchase down to it, so the player always gets as much as
they can pay for.

Not resolvable statically: the amount reaching that check has already passed
through `FUN_281f_035c(DS:0x9cc8, 0, amount)`, an unidentified clamp, so DOS's
later funds test may be a dead safety net behind a pre-clamp that already equals
"affordable amount" — in which case the port's behaviour is the DOS behaviour.
Needs a live DOS capture: on the Europe screen, order more of a good than the
treasury covers and record whether the hold fills partially or the buy is refused.
Identify DS:0x9cc8 in the same session.
