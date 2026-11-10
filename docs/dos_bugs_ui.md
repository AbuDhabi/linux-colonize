# DOS bugs the port does not reproduce: screens, dialogs and MAPEDIT

STATUS: living record, not an audit dump. Authoritative.

Third file of [dos_bugs.md](dos_bugs.md), split off 2026-11-08 to keep each file
under the doc-length limit. It holds the entries about screens, popups and menus
(colony screen, Europe screen, pickers, encyclopedia), cheat keys and MAPEDIT. The
rules, the id scheme and the entry format are the ones in dos_bugs.md; `DB-N` ids
are shared across dos_bugs.md, [dos_bugs_ai.md](dos_bugs_ai.md) and this file and
never reused.

## Entries

### DB-45 — View-mode Shift-D steps the active-unit index back twice
**DOS:** `FUN_2b5a_199e` (raw 42609-42690) disbands the stack head at the cursor in View mode
(`DS:0x5390 != 0`). The pool compaction `FUN_1427_0824` already does
`if (DS:0x5392 != 0 && idx <= DS:0x5392) DS:0x5392--` (raw 7834-7836), then 199e's tail
runs its own `if (idx <= DS:0x5392) { if (idx < DS:0x5392) DS:0x5392--; ...}`. Disbanding a
unit at a lower pool index than the active one steps the active index back twice, so the
active unit silently becomes the one before it. Unit mode never double-steps (the deleted
unit IS the active one, the second test is false). Read statically at the #1173 part 4 port; accepted by the user 2026-10-23.

**Port:** the active unit is held by id (`units.selected_id`), so disbanding another unit
in View mode leaves the selection alone; `game_do_disband` (`src/core/game_dialogs.c`)
re-selects nothing in View mode.
**Status:** accepted (2026-10-23, port_bugs.md #1173).

### DB-53 — Go to Port / route stop pickers lose the last colony at 20, 30 or 40 (port_bugs.md #1194)
**DOS:** FUN_647e_01c6 (raw 102368-102374) pages more than 10 destination colonies
into (n + 9) / 10 pages of 10 slots, but with the Europe row offered (slot 0, colony
k in slot k + 1) the colonies need n + 1 slots. For n = 20, 30 or 40 the last
colony's slot n is past the last page, so it is on no page and cannot be picked.

**Port:** `cheat_list_dest_page_math` (`src/core/cheat_list_dialog.c`) counts the
Europe slot, (n + e + 9) / 10 pages; only those three cases gain a last page holding
Previous and that colony. Every other page is DOS's. Test `smoke_popup_dialogs
dest_pager_is_dos`.
**Status:** done (2026-10-23, user-approved 2026-10-23).

### DB-54 — Opening the colony jobs menu stamps plots and resets the job counter (port_bugs.md #1205)
**DOS:** FUN_2f2b_348c's jobs-mode build-up (param_1 == 0, raw 50599-50675) computes
each field job's figure by really seating the colonist: FUN_15eb_28c8's final
FUN_15eb_06d2 seat runs with DS:0x34d cleared (raw 13138-13147), then a re-seat on his
own plot (raw 50666-50668), and every job change goes through FUN_15eb_1068, which
zeroes the colony +0x60 turns-in-job nibble (raw 11256-11258). So merely opening the
menu, Escape included, stamps the colony's owner nibble and mask 0x10 on each elected
plot and zeroes the colonist's job counter (the teaching loop's progress).
DOSBox-confirmed 2026-10-23.

**Port:** `game_colony_open_jobs` (`src/core/game_loop_colony.c`) only opens the popup;
the estimates come from the read-only `ai_euro_jobs_menu_estimate`. A real pick still
seats and resets through the normal assign path.
[Trace and tests](archive/dos_bugs_evidence_2026-10-23.md#db-54).
**Status:** done (2026-10-23, user-approved 2026-10-23).

### DB-55 — Escape in the Incite Indians target menu picks the English (port_bugs.md #1157)
**DOS:** FUN_4d56_417e (raw 83595-83617) runs the @INDIANWARPATH menu through
FUN_291f_016a = FUN_6f74_2580, which returns -1 on Esc (LAB_6f74_2b9a). The caller
clamps it (`cmp ax,1; jnl; mov ax,1; dec`, 4d56:436c-437a; rows are nation + 1), so Esc
reads as nation 0. With no English contact DOS shows @NOCONTACT; otherwise
@INDIANWARPATH2 offers to drive the English away for the price (8800 in the trace), and
Pay sets the tribe's tension against England to 100. There is no self check, so a human
England player's Esc offers to incite the tribe against himself. DOSBox-confirmed
2026-10-23. [Trace](archive/dos_bugs_evidence_2026-10-23.md#db-55).

**Port:** Esc cancels: the `AI_CONTACT_INCITE_STAGE_TARGET` arm in
`src/core/ai_contact_actions.c` enqueues the confirm only when the result is not
cancelled, and `ai_contact_enqueue_incite_confirm` (`src/core/ai_contact_demand.c`)
refuses target == the inciter.
**Status:** done (2026-10-23, user-approved 2026-10-23).

### DB-56 — Europe transit-box releases clear orders and the Expected box sails a harbor ship (port_bugs.md #1159)
**DOS:** FUN_38fd_3228 mode 9 zeroes a Bound / Expected ship's orders byte +0x314c
(38fd:326f) before it compares drop and start zone, so any release over its own box, a
plain click included, cancels its orders and trade route; mode 8 sails a harbor ship
dropped on either transit box. DOSBox-confirmed 2026-10-23. [Details](archive/dos_bugs_evidence_2026-10-24.md#db-56).

**Port:** the UI_DRAG_EUROPE_*_SHIP drop handler (`src/core/game_loop_colony.c`) acts
only when the box changes (`europe_reverse_transit`, `src/core/europe_harbor.c`); a
harbor ship sails only from a drop on Bound for the New World.
**Status:** done (2026-10-23, user-approved 2026-10-23).

### DB-57 — Hall of Fame 6th-place retire never shows the new entry (port_bugs.md #1219)
**DOS:** FUN_41f2_0f56 loads 5 records; a Retire entry that outranks none takes the catch-all
slot 5 (local_156). The presenter numbers the last line local_6 + 1 = "6." and draws it in
DS:0x831 hilite when local_6 == local_156 (asm 41f2:12b2-12ef), but reads every field from the
loop counter's slot (SI at 41f2:12ca / 1336 / 13a4), so line 5 shows the old 5th-place entry
labelled "6." and the new entry is never visible.

**Port:** `reports_render_hall_of_fame` (`src/core/reports_score.c`) draws the retired entry's
own fields on that "6." line (`ColonizeGameState.hof_retired`, set in
`src/core/game_loop_saveload.c`); numbering and hilite stay DOS's. Test `unit_reports
hall_of_fame_layout_is_dos`.
**Status:** done (2026-10-24, user-approved 2026-10-24).

### DB-58 — Unload all cargo warns about a boycott only in slot 0
**DOS:** row 3 of the Europe ship menu, FUN_38fd_2bfe (raw 60866-60888), walks the hold
slots from the last down to 0, skips a boycotted one (FUN_38fd_05e8) and sells the rest.
The notice flag `local_62` is cleared at the top of every pass (raw 60872, asm
38fd:2d8c) instead of once before the loop, so @SOMEBOYCOTT (0x1027) shows only when
slot 0 is boycotted; a boycotted hold in any other slot stays aboard without a word.

**Port:** `game_europe_unload_all` (`src/core/game_dialogs.c`) keeps the flag set once
any hold was boycotted. Test `unit_stage_seams test_europe_unload_all_boycott_any_slot`
(mutation-checked). Full ctest and `make golden` green with the fix.
**Status:** done (2026-10-24, user-approved 2026-10-24). Found in the 2026-10-24
post-sweep scan (from port_bugs.md #1176).

### DB-59 — Build list drops the current project by an unrelated unit's type
**DOS:** FUN_15eb_3650's tail (raw 13741-13746, asm 15eb:3889-38b4) lists the colony's
current project (+0x94) whatever the gates said, unless `byte [code * 0x1c + 0x3146]
== 1` and the colony owns `local_e`. That byte is the type of unit record #code: the
build code is used as an index into the unit array. So whether a just-built current
project shows as its "(COMPLETE)" row (FUN_2f2b_5a68) depends on which unit happens to
sit in that slot.

**Port:** `colonies_build_keeps_current` (`src/core/colony_build.c`) always keeps the
current project; `ColoniesBuildableOpts.units`, added only for that read, is gone. The
Euro AI lists with +0x94 = -1 (raw 95711), so AI behaviour is unchanged. Test
`unit_colonies unit_build_list_keeps_current`. Full ctest and `make golden` green.
**Status:** done (2026-10-24, user-approved 2026-10-24). Found in the 2026-10-24
post-sweep scan (from port_bugs.md #1218).

### DB-60 — Construction list pager never resets its default-row flag
**DOS:** FUN_2f2b_5bd2 clears `local_8` once (raw 52575) and every page-0 pass of a
two-page list sets it, so after the first showing neither fallback arm (raw 52620-52629,
DB-63) runs and a turned page without the current project bars its first row.

**Port:** `colony_screen_construction_next_page` (`src/core/colony_screen_popups.c`)
treats every page alike; the page is its own field (`ColonyScreenView.construction_page`).
[Details and tests](archive/dos_bugs_evidence_2026-10-24.md#db-60).
**Status:** done (2026-10-24, user-approved 2026-10-24). Found in the 2026-10-24
post-sweep scan (from port_bugs.md #1215).

### DB-61 — Jobs / leave-as menu reads the occupation before setting it
**DOS:** FUN_2f2b_348c's occupation row (raw 50746) highlights when `local_136 ==
local_8`, but `local_136` is first written at raw 50799, so the first page-0 draw tests
a stale stack word.

**Port:** the eject rows in `src/core/colony_screen_popups.c` use the occupation every
later DOS draw sees. The port never reproduced the stale read; recorded 2026-10-24 at
the user's call.
**Status:** done (2026-10-24). Found in the 2026-10-24 post-sweep scan.

### DB-62 — Colony-screen `T` cheat refunds a departed colonist's goods
**DOS:** the `T` cheat (FUN_2f2b_6372 tail, 2f2b:6820) raises the head count first,
then calls FUN_15eb_1068(old, 0). 1068's refund at its top (raw 11229-11247, through
FUN_15eb_0e18) reads the occupation byte at the new index, a leftover of whichever
colonist last held that slot, and credits the colony the goods of that stale occupation.

**Port:** `colonies_cheat_add_colonist` (`src/core/colony.c`) pays no refund; the port
keeps no byte past the head count. Never reproduced; recorded 2026-10-24 at the user's
call.
**Status:** done (2026-10-24). Found in the 2026-10-24 post-sweep scan.

### DB-63 — Construction list fallback bars a row id that does not exist
**DOS:** on a page without the current project FUN_2f2b_5bd2 calls `FUN_291f_08ec` /
`033c` with 0x65 (page 0, raw 52620-52624) or 100 (page 1, raw 52627-52629): the
"(More)" ids 0x63 / 0x62 (raw 52640-52645) with the row offset + 2 added again. No row
matches, so DOS shows no bar.

**Port:** `colony_screen_open_construction` (`src/core/colony_screen.c`) and
`colony_screen_construction_next_page` bar that "(More)" row and draw it in the hilite
ink. [Evidence and tests](archive/dos_bugs_evidence_2026-10-24.md#db-63).
**Status:** done (2026-10-24, user-approved 2026-10-24). Found while filing DB-60.

### DB-82 — Captured treasure of 12800 gold or more shows a negative amount
**DOS:** `FUN_5fef_0352` (raw 99404) builds the @LOOTCAPTURE NUMBER0 from the
treasure's value byte with `MOV AL,0x64; IMUL byte ptr [SI+0x315b]; CWD`
(viceroy_unpacked.asm:167858), a signed byte multiply. A value byte of 0x80 or
more (12800+ gold) prints negative: 0x96 (15000 gold) shows -10600. The other four
`*100` sites on +0x315b use the unsigned `MUL`, so only this message is wrong and
no gold is affected.

**Port:** `units_combat.c` reads the value through `units_treasure_value_gold`
(unsigned), so the message shows the real amount. Ungated (display only).
**Status:** done (user 2026-11-08, port_bugs.md #1602).

### DB-83 — MAPEDIT minimap on a map under 58x41 reads outside the terrain plane
**DOS:** `FUN_1b56_00dc` clamps the minimap origin through `FUN_1865_000e`, floor 1
first, then ceiling w-57 / h-40, so a narrower or shorter map gets a negative
origin; `FUN_1b56_011c` then draws 56x39 bytes linearly from it with stride = map
width (memory before the plane, wrapped rows).

**Port:** `me_minimap_origin` (`src/core/mapedit.c`) applies the ceiling first, then
the floor (origin >= 1), and `me_render_side` skips tiles past the map edge.
**Status:** done (user 2026-11-08, port_bugs.md #1604 item 9 / #1608).

### DB-84 — MAPEDIT Undo after Load or New pastes the previous map
**DOS:** the undo-valid word DS:0x52 is only ever set to 1 (`FUN_1000_0750`, the
snapshot) and never cleared, so Undo (`FUN_1000_077e`) after Load or New copies the
old map's 12000-byte terrain snapshot over the new map.

**Port:** `me_load` and `me_new_map` (`src/core/mapedit.c`) clear `undo_valid`, so
Undo does nothing until the next snapshot.
**Status:** done (user 2026-11-08, port_bugs.md #1604 item 10 / #1608).

### DB-85 — Colony-screen `T` cheat has no 32-colonist cap
**DOS:** the T arm of `FUN_2f2b_6372` (raw 55699-55705) increments the colony head
count +0x1f with no limit. At 32, `FUN_15eb_1068` writes occupation slot 32 at +0x40
(profession[0]) and `FUN_15eb_0e8c` writes into the turns-nibble bytes at +0x60:
the record overruns.

**Port:** `colonies_cheat_add_colonist` (`src/core/colony.c`) refuses past
COLONIZE_COLONY_POP_MAX (32).
**Status:** done (user 2026-11-08, port_bugs.md #1604 item 11 / #1608).

### DB-86 — Colony-screen `T` cheat keeps a departed colonist's turns-in-job nibble
**DOS:** `FUN_15eb_1068(old, 0)` resets the turns nibble (`FUN_15eb_0cbc`, raw
10220-10244) only when the stale occupation byte at the new index (`FUN_15eb_0e18`)
differs from 0, so a stale occupation of 0 keeps the leftover nibble at +0x60 + idx/2.

**Port:** `colonies_cheat_add_colonist` zero-fills the new colonist (turns 0); like
DB-62, the port keeps no bytes past the head count.
**Status:** done (user 2026-11-08, port_bugs.md #1604 item 11 / #1608).

### DB-88 — Escape at @SCOUTCOLONY attempts the infiltration (port_bugs.md #1727)
**DOS:** FUN_5f7a_000e (raw 98822-98852, asm 5f7a:0067-007f) stores the
`FUN_281f_0652(0x1a64, 3)` answer, which is -1 on Esc (FUN_6f74_2580). The signed
tests `CMP 3; JL` then `CMP 1; JNZ` send -1 into the Infiltrate arm: the difficulty
term, the 281f_04ca reseed, RNG(1,0x24), and on failure @LOSTOURSCOUTS, +100 horses
and the scout despawned.

**Port:** Esc reads as "Nothing": the `AI_POPUP_TAG_SCOUT_COLONY` arm of
`game_apply_popup_diplo_and_scout` (`src/core/game_dialogs.c`) maps a cancel to
choice 4, which only spends the scout's moves.
**Status:** done (user 2026-11-08).

### DB-89 — Escape at @TRADEWHICH prices a hold that does not exist (port_bugs.md #1728)
**DOS:** FUN_5f7a_020e (raw 98955-98965, asm 5f7a:0396-03ab) lets only 0 and 99
leave the cargo pick; Esc (-1) becomes hold index -2. 0be6 / 0c68 then read the
cargo type and amount from out-of-range bytes, a @TRADEWITH price is computed from
them, and the `FUN_281f_04d4(10, (diff+1)*0xc)` draw at raw 98975 still runs (an
RNG desync).

**Port:** Esc reads as "never mind": the `AI_POPUP_TAG_FOREIGN_TRADE_WHICH` arm of
`game_apply_popup_diplo_and_scout` (`src/core/game_dialogs.c`) maps a cancel to 99,
so there is no offer and no draw.
**Status:** done (user 2026-11-08).

### DB-90 — A ship with only land routes is told there are no Land routes (port_bugs.md #1718)
**DOS:** with no trade route of the unit's kind, FUN_647e_0796 (asm 647e:0894-08bf)
indexes @ROUTE (DS:0x93de) with `(param_1 == 2) == last_route.sea ? 3 : 4`. The last
route already failed that same kind filter (647e:07f8), so the index is always 4:
a ship reads "You have not yet defined any Land trade routes."

**Port:** `game_open_trade_route_picker` (`src/core/game_dialogs.c`) picks the label
from the unit's own kind (`filter == 2` gives Sea), so a ship reads "Sea".
**Status:** done (user 2026-11-08).

### DB-91 — The Artillery encyclopedia caption has no closing bracket (port_bugs.md #1767)
**DOS:** for type 0xb, FUN_6cb2_07e6 (raw 110451, asm 6cb2:0b06-0b85) builds
`name " (" @MISC106 " " @MISC201 " " name` and goes straight to the 013c draw at
0b85 without the FUN_281f_0128 ")" append, so the caption reads "Artillery (and
Damaged Artillery".

**Port:** `pedia_article_unit` (`src/core/pedia.c`) formats "%s (%s %s %s)" and
closes the bracket.
**Status:** done (user 2026-11-08).

### DB-93 — The encyclopedia unit page is blank when the unit pool is full (port_bugs.md #1767)
**DOS:** FUN_6cb2_07e6 (raw 110451) starts the unit article by creating a temporary
unit (FUN_2a1f_01ca(0, -6, -6, DS:0x5398)) and draws nothing when that fails, so with
every unit slot taken the page stays blank.

**Port:** `pedia_article_unit` (`src/core/pedia.c`) needs no temporary unit and always
draws the page.
**Status:** done (user 2026-11-10; port already differed).

### DB-94 — @TUTORIAL4 names a random goods type for a colonist in a building
**DOS:** the colony-screen bring-up FUN_2f2b_6cd4 (raw 56026-56070) fills @TUTORIAL4
%STRING1 from colonist 0's plot via FUN_281f_0d30 -> FUN_15eb_1646, which leaves the
x/y outputs unwritten when the colonist works in a building, so the "other cargo" scan
reads whatever tile the stack junk points at.

**Port:** `game_tutorial_colony_opened` (`src/core/game_tutorial.c`) keeps Food when
colonist 0 has no plot.
**Status:** done (user 2026-11-10; port already differed).

### DB-96 — MAPEDIT keeps a bad file's name and size after a failed Load Map (port_bugs.md #1615)
**DOS:** FUN_1000_17e0 case 0x1b copies the picked name into DS:0x4a18 before
FUN_19f9_0170 runs, and 0170 reads the header's W/H straight into DS:0x4b12/0x4b14
before it checks the version. After @ERROR the old map therefore carries the new file
name, so a later Save overwrites the bad file; a version-3 file also leaves the old
planes read with the new W/H.

**Port:** `me_load` (`src/core/mapedit.c`) changes nothing on failure.
**Status:** done (user 2026-11-10; port already differed).

### DB-97 — Opening a colonist's jobs menu spends the unit (port_bugs.md #1748)
**DOS:** FUN_2f2b_348c's trial loop (raw 50599-50663) admits the fence unit through
FUN_15eb_1068 arm 3, and the tail restore FUN_15eb_1068(sel, current job) (raw
50669-50675; again on cancel) rebuilds it through arm 2 (FUN_1427_06b4 + FUN_1427_155e)
or, in a full colony, re-equips it in place (arm 1). Merely opening the menu, Escape
included, sets spent = full allotment, clears the orders (fortify too), moves the unit to
a new record at the end of the array, and makes a Pioneer return its tools and re-take
min(100, floor(stock/20)*20). DOSBox 2026-11-10: Pioneer with 40 tools, colony with 120:
unit 66 -> 155, spent 3, orders 0, tools 100, colony 60. Same mechanism as DB-54.

**Port:** `game_colony_open_jobs` (`src/core/game_loop_colony.c`) only opens the popup;
a picked role row exhausts and re-equips as DOS does (#650).
**Status:** done (user 2026-11-10; port already differed).
