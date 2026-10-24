# dos_bugs.md evidence archive (2026-10-24 batch)

STATUS: archived 2026-10-24, closed. Full entry text, moved verbatim (links re-rooted)
when [docs/dos_bugs.md](../dos_bugs.md) was trimmed back under the doc-length limit.
Not a spec: each entry in `dos_bugs.md` stays authoritative and keeps its id, DOS
citation, port site and approval note; this file holds the longer evidence, port
detail and test history it used to carry. Older trims:
[dos_bugs_evidence_2026-10-23.md](dos_bugs_evidence_2026-10-23.md). Look up one id:
`grep -n '^## DB-NN$' docs/archive/dos_bugs_evidence_2026-10-24.md`.

## DB-2

Entry as it stood before the 2026-10-24 trim (Save files omit live state, so save+reload diverges from continuous play):

**DOS:** the save (`FUN_75c2_0288`) does not write several tables the turn loop
reads, so a game played straight through and the same game saved and reloaded
take different paths. Known holes (idle_independence.md "Quirks of this save
set"): the Euro AI stance table `DS:0x9870` (nation*16 + continent) and the AI
goal tables `DS:0x98b0..` read by the 0a60 work-queue military arm; the Indian
rank table `DS:0x917c` (a load starts with zeros, the day top's 0550 refresh
refills it). Evidence: [archive](dos_bugs_evidence_2026-10-23.md#db-2).

**Port:** the `RTST` chunk of the port extension block (`COLNXEXT`,
[savegame.md](../savegame.md) "Port extension block", which lists the records;
`src/core/runtime_ext.c`) carries every such table plus the port's LCG, written
on every save and restored by `col1_bridge_apply` on top of the module resets
(first nine records, 2026-10-19: [archive](dos_bugs_evidence_2026-10-23.md#db-2)). Every
port save therefore carries the block and the `*` slot mark; DOS still loads it
(ignores the tail) and drops it on re-save, after which the port loads it the DOS
way (tables zero, LCG from the turn number).
**Status:** done (2026-10-19). Guarded by CTest `golden_idle_reload` (1492-1599),
`golden_idle_independence_reload` (saves 1-21) and `golden_idle_america_reload`
(saves 1-63): the idle campaigns with a save/reload before every turn (`GOLDEN_IDLE_RELOAD=1`) stay byte-identical to
DOS wherever the continuous gates do. A later hole shows up there first; the
recipe is in [idle_campaign.md](../idle_campaign.md) "Running and debugging".

## DB-3

Entry as it stood before the 2026-10-24 trim (Independence rename overflows the country-name field):

**DOS:** when a rival nation is granted independence (@OTHERGRANTED,
FUN_3844_0442, raw 58602) DOS `strcpy`s the NAMES.TXT republic name
into `player.country_name` (DS `nation*0x34 + 0x5426`, 24 bytes) with no bound.
"United States of America" is exactly 24 characters, so its NUL lands on
`player+0x30` and zeroes that byte: `lcr_case5_bonus_used` (bit 6) and
`named_new_world` (bit 7) are wiped as a side effect. Evidence: america seed-100
216->217, England renamed, player 0 byte +0x30 goes from 0x40 to 0.

**Port:** `turn_year_end_rival_independence` (`src/core/turn_year_end.c`) fills
the 24-byte field (no NUL when the name is exactly 24 characters) and leaves
`player+0x30` alone. Every reader takes the name through
`col1_player_country_name` (`src/core/col1_save.c`), which returns a terminated
copy. A DOS load of such a save reads the name up to the next zero byte, so it
can show a stray character after "America" when bit 6 or 7 is set.
**Status:** done (2026-10-21). The idle America resync pair 217 differs from
DOS only in `players[0].lcr_case5_bonus_used` (plus the hold[5] artifact).

## DB-4

Entry as it stood before the 2026-10-24 trim (Euro AI border park reads an uninitialised stack word):

**DOS:** the 0x46 border-park arm of FUN_521d_20e6 (raw 89011-89029, the LAB_4d2e
tail) tests `local_ea` (BP-0xE8 = SS:e7c4), which 20e6 never writes before that
read. The word is whatever the last frame at that stack address left: a few
deterministic writers (FUN_6662_00f2, the FUN_5bfb_022e met arm, the previous 20e6
scan), otherwise RTLink overlay-loader residue that depends on the overlay area's
history since process start. So a combat land unit beside a foreign colony parks
('F') or wanders depending on memory history, not game state: a DOSBox from-load
of the same save and the continuous archive disagree on the same turn (america
seed-100 save 63 -> 64; writers and evidence: [archive](dos_bugs_evidence_2026-10-23.md#db-4)).

**Port:** `ColonizeTurnContext.ai_euro_wander_stack_word` (`src/core/turn.h`)
models the deterministic writers only (`ai_euro_act.c` 00f2 flood and, after it,
a hull's step off a shared tile (0x020e, the FUN_1427_12f6 re-place), `ai_euro.c`
022e met arm, `ai_euro_found_with_unit` in `ai_euro_expand.c` (0xe7d6, the
founding's 1f72 tail), `ai_euro_20e6_wander_step` in `ai_euro_land.c` for its own
scan, which is also the reader). `ai_euro_dispatcher_turn` resets it to 0 at each
nation's turn. The loader residue and the visible-move redraw frames are not
modelled, so the park decision is a function of game state only, and a
save/reload gives the same result.
**Status:** done (2026-10-21; founding and hull writers 2026-10-23, port_bugs.md
#1135). The America idle archives end at save 63 and save 14 for this reason
(CTests `golden_idle_america` and `golden_idle_america2`,
[idle_america.md](../idle_america.md), [idle_america2.md](../idle_america2.md)).

## DB-9

Entry as it stood before the 2026-10-24 trim (Jesuit "leave as Colonist" row keyed on the colony's record slot):

**DOS:** FUN_15eb_3454 (raw 13561-13565) decides whether the "leave as Colonist" row is
offered to a unit leaving a colony. For a Jesuit Missionary (profession 0x18) it refuses
the row when `*(int*)0x8dc6 < 4` and the control byte
`*(char*)(0x543f + *(int*)0x8dc6 * 0x34)` is 0 (human). `DS:0x8dc6` is the active
colony's record index (FUN_15eb_002c raw 9318), but `0x543f + n*0x34` is the nation
table. So a Jesuit can leave as a plain Colonist almost everywhere, except in colonies
whose record slot (0..3) happens to equal a human nation's number, whoever owns the
colony.

**Port:** `colonies_eject_row_offered` (`src/core/colony_workers.c`) drops the arm: any
Jesuit, human or AI, is offered the Colonist row (user decision 2026-10-21, matching
what DOS does in nearly every colony). Unchanged from DOS: a Jesuit is always offered
the Missionary row, church or not; at the Europe dock a born Jesuit cannot unbless
(38fd:3f4a). Came from port_bugs.md #559.
[Tests](dos_bugs_evidence_2026-10-23.md#db-9).
**Status:** done (2026-10-21).

## DB-14

Entry as it stood before the 2026-10-24 trim (Tea Party picks the cargo without using its random draw):

**DOS:** FUN_38fd_3dc8 (raw 64176-64200, asm 38fd:3f63-3fbc). The cargo weights of the
eligible goods are summed into a total, and `rand(1, LOWORD(total))` is stored in
`local_84` and never read. The walk then subtracts each weight from the original total,
so the pick is always the last stocked, non-boycotted cargo with a nonzero weight. The
intended code is the usual roulette: `r = rand(); r -= w[c]; pick when r <= 0`. The
seized or boycotted good is predictable instead of weighted by traded tonnage.

**Port:** `ai_king_pick_dump_goods_cargo` (`src/core/ai_king.c`) keeps the one draw and
starts the walk from it, so each eligible cargo is picked in proportion to its weight.
If the weights sum to zero (no good with 100+ tons traded), the one draw picks uniformly
among the eligible goods instead (user decision 2026-10-22): any stocked, not yet
boycotted good in a coastal colony stays a protest option, and only with none is the
hike forced without one. DOS took the last eligible cargo with a nonzero weight, or the
first eligible one when all weights were zero. Unit test: `unit_ai_king_core`
`dump_goods_pick_api` ([cases](dos_bugs_evidence_2026-10-23.md#db-14)).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-3.

## DB-23

Entry as it stood before the 2026-10-24 trim (Euro AI road-connect walk never restarts at the colony):

**DOS:** FUN_5952_0000 (raw 93589-93685), the AI's inter-colony road-connect helper,
copies the colony's x and y into `local_18` / `local_1e` once (raw 93599-93600). Each
candidate's distance test (raw 93610-93621) and its flood-walk toward the other colony
(raw 93632-93652) then step those two locals in place and nothing resets them. After one
candidate's walk fails (no gap found, tile occupied, rival's improvement), the next
candidate is measured, and walked, from wherever the last walk stopped instead of from
the colony. The helper is meant to look for links out of this colony, so every candidate
should start at the colony.

**Port:** `ai_euro_5952_road_connect_0000` in `src/core/ai_euro_land.c` takes the walk
origin `cx` / `cy` from the colony at the top of each candidate. Nothing else changes:
the same AND distance test (see tolerated_dos_bugs_ai.md TB-15), the same
one-in-(count-1) roll and the same walk.
[Evidence and tests](dos_bugs_evidence_2026-10-23.md#db-23).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-14.

## DB-24

Entry as it stood before the 2026-10-24 trim (Founding join scores the forest penalty from stack residue):

**DOS:** FUN_15eb_28c8 (raw 13119-13122) reads `local_32` for its forest penalty, but
assigns it only in the food-emergency arm (raw 13037). See tolerated_dos_bugs_ai.md
TB-16 for the tick. When a colony is founded, the founder's plot is picked by the same
function (the founding join, after FUN_364b_1ba8 raw 58088), and the stack word there is
the colony x left by FUN_137f_015e's pushed argument, not a terrain class. The forest
-10 therefore never fires for the founder, whatever tile it considers.

**Port:** `colonies_found` in `src/core/colony.c` no longer sets the residue before
`colonies_seat_new_colonist`; the founding join scores the candidate tile's own terrain
class, like the human auto-assign path. The residue setter
`ai_euro_28c8_set_l32_residue` (`ai_euro.h`, `ai_euro_colony_jobs.c`,
`ai_contact_link_stubs.c`) is deleted. The tick's residue (tolerated_dos_bugs_ai.md
TB-16) is unchanged.
[Evidence and tests](dos_bugs_evidence_2026-10-23.md#db-24).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-15.

## DB-28

Entry as it stood before the 2026-10-24 trim (Euro AI explorer hop countdown stores a signed max):

**DOS:** when an AI explorer hops to a plot-ring slot, FUN_521d_20e6 latches the hop
length into the unit's countdown byte (+0x3155) as `max(dx*4, dy*4)` on the signed ring
offsets (DS:0xc8 / 0xde; raw 90126-90138, duplicate body at raw 85300). For a slot whose
offsets are both negative the result is negative: the NW slot (-1,-1) stores -4, which
the unsigned byte reads as 252. A slot with one zero offset and one negative offset
stores 0. The countdown is decremented once per step (raw 89294) and the site scan is
skipped while it is nonzero, so an explorer sent north-west scans for sites only after
about 252 steps instead of 4. The hop length is a distance in tiles: the larger absolute
value was meant.

**Port:** `ai_euro_20e6_ring_hop` (`src/core/ai_euro_land.c`) stores
`max(|dx|*4, |dy|*4)` in the hop scratch byte `sc[1]`; the countdown in
`ai_euro_move_scoring_gate` (`sc[1]--`) is unchanged.
[Evidence and tests](dos_bugs_evidence_2026-10-23.md#db-28).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-27.

## DB-31

Entry as it stood before the 2026-10-24 trim (Tribe threat accumulator is a signed byte):

**DOS:** FUN_4d56_152e (raw 81387-81534) keeps a per-Euro accumulator at
`indian+0x36+e`, a signed `char`. The threat arm subtracts the tribe's threat score
(doubled at a capital) from it with `*pcVar2 = *pcVar2 - (char)local_c` (raw 81503), and
later loops spend it: below -7 it adds 8 and raises the alarm by 1 (heating), above 7 it
subtracts 8 and lowers the alarm (cooling). A threat of 128 or more truncates to a
negative `char`, so the subtraction adds and the accumulator goes positive: a very large
threat cools the tribe instead of heating it. It needs about 22 Dragoons or 13 Artillery
next to a capital.

**Port:** the threat arm of `ai_indian_152e_village_growth` (`src/core/ai_indian.c`)
saturates the accumulator at -128 instead of wrapping it. Below that range the result is
identical to DOS. The alarm word itself already used the unwrapped threat.
[Tests](dos_bugs_evidence_2026-10-23.md#db-31).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-34.

## DB-46

Entry as it stood before the 2026-10-24 trim (Pioneer work counter carries over to a new job):

**DOS:** the Pioneer progress counter +0x315a is raised by the work bodies FUN_479b_01a6
(raw 76753) and FUN_479b_0526 (raw 76886) and zeroed only on completion (raw 76773 /
76899), Fortify (raw 42418) and colony entry (465b raw 76733). The order entries
FUN_2b5a_123e (raw 42511) and FUN_2b5a_1454 (raw 42602) write only the order byte
+0x314c. A Pioneer that chops 3 turns on one tile, walks away and starts Clear Forest on
another finishes next turn; road progress likewise counts toward a later plow on the
same tile.

**Port:** `units_pioneer_plow_w` / `units_pioneer_road_w` (`src/core/units_pioneer.c`)
zero the counter when the order changes to the job, and the AI 20e6 road arm
(`ai_euro_land.c`) does the same. The 5952 phantom Pioneer, which seeds 99 to finish in
one call, latches its order first so the entry leaves the 99 alone. Re-issuing the same
job after an interruption also restarts it.
[Tests](dos_bugs_evidence_2026-10-23.md#db-46).
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-72 (port_bugs.md #620).

## DB-56

Entry as it stood before the 2026-10-24 trim (Europe transit-box releases clear orders and the Expected box sails a harbor ship (port_bugs.md #1159)):

**DOS:** FUN_38fd_3228 mode 9 (the release poll of a Bound / Expected ship drag,
dispatched by FUN_38fd_4e8e for drop zones 2 and 3) writes the hull's orders byte
+0x314c = 0 (38fd:326f) before it compares the drop zone with the start zone
(`cmp [bp+6],[0x9e44]`), so any left release over the ship's own box, a plain click on
the ship included, cancels its orders and trade route without moving it. Mode 8 (a
harbor ship drag) calls the sail handler FUN_38fd_62ff on release with no zone test
(38fd:3232-324b), and 4e8e routes it for zones 2 and 3, so dropping a harbor ship on
Expected Soon sails it like a drop on Bound for the New World. DOSBox-confirmed
2026-10-23. [Trace](dos_bugs_evidence_2026-10-23.md#db-56).

**Port:** the drop handler in `src/core/game_loop_colony.c` (UI_DRAG_EUROPE_*_SHIP)
acts only when the box changes: Expected -> Bound and Bound -> Expected reverse the
transit and clear the orders and route (`europe_reverse_transit`,
`src/core/europe_harbor.c`, as DOS does on a real move); a release on the ship's own
box does nothing; a harbor ship sails only from a drop on Bound for the New World,
and a drop on Expected Soon does nothing.
**Status:** done (2026-10-23, user-approved 2026-10-23).

## DB-60

Entry as it stood before the 2026-10-24 trim (Construction list pager never resets its default-row flag):

**DOS:** FUN_2f2b_5bd2 clears `local_8` once (raw 52575) and sets it on every page-0
pass of a two-page list (raw 52611 or 52624), so after the first showing neither
fallback runs: page 0's `08ec(0x65)` (raw 52620-52624) and page 1's `08ec(100)` (raw
52627-52629). A turned page that does not list the current project then bars its first
row (the clear row, or the back "(More)") instead of showing no bar as the first
showing does.

**Port:** `colony_screen_construction_next_page` (`src/core/colony_screen_popups.c`)
treats every page alike: the current project's row if the page lists it, else the
fallback arm (as fixed by DB-63). The page is now its own field
(`ColonyScreenView.construction_page`). Tests
`unit_stage_seams test_colony_construction_page_default` /
`test_colony_construction_two_pages_live`, `unit_colony_screen
unit_sub_dialog_pages_are_dos`. Full ctest and `make golden` green.
**Status:** done (2026-10-24, user-approved 2026-10-24). Found in the 2026-10-24
post-sweep scan (from port_bugs.md #1215).

## DB-63

Entry as it stood before the 2026-10-24 trim (Construction list fallback bars a row id that does not exist):

**DOS:** when a page of FUN_2f2b_5bd2's two-page list does not list the current project,
page 0 calls `FUN_291f_08ec` / `033c` with 0x65 (raw 52620-52624, asm 2f2b:5d28) and
page 1 with 100 (raw 52627-52629, asm 2f2b:5d5b). Rows carry the build code + 2; the
"(More)" rows are 0x63 (forward) and 0x62 (back), the ids the result switch tests (raw
52640-52645). 0x65 and 0x64 are those ids with the + 2 added again, so no row matches,
`FUN_6f74_089a` returns NULL and the box shows no bar (Enter does nothing). Each arm runs
only on a page that has its "(More)" row, so the intent is the bar and hilite ink on
the row toward the project's page.

**Port:** `colony_screen_open_construction` (`src/core/colony_screen.c`) and
`colony_screen_construction_next_page` (`src/core/colony_screen_popups.c`) bar that
"(More)" row and draw it in the hilite ink; the no-bar state is gone. Tests
`unit_stage_seams test_colony_construction_page_default` /
`test_colony_construction_two_pages_live` (mutation-checked), `unit_colony_screen
unit_sub_dialog_pages_are_dos`. Full ctest and `make golden` green.
**Status:** done (2026-10-24, user-approved 2026-10-24). Found while filing DB-60.
