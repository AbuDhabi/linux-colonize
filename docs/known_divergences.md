# Known divergences from DOS

STATUS: living record, not an audit dump. Authoritative.

Deliberate, recorded differences between this port and VICEROY.EXE that nobody
intends to change, plus static questions that are blocked on a live DOS capture
rather than on code. These are **not** bugs and must never be filed in
[port_bugs.md](port_bugs.md), which holds actionable OPEN rows only. Each entry keeps
its original `port_bugs.md #` id so existing citations (`port_bugs.md #NNN`) still resolve;
ids are permanent and never reused.

This file is for differences where DOS is *right* and the port still differs
(data-model cost, unresolved capture). Where DOS itself is wrong and the port
deliberately does not copy the bug, the entry belongs in
[dos_bugs.md](dos_bugs.md) instead.

When working a feature, grep this file for that feature's name before "fixing"
something that looks wrong — several entries below exist precisely because the
DOS-literal write would be worse in the port's data model.

## Accepted divergences (do not change)

### #665 — REF demote runs the equip sync
The demote arm calls `units_sync_equip_after_type_change` after the type write
(`units_combat.c` ~1070); DOS `FUN_5fef_0352` raw 99437-99464 writes only `+0x3146`.
Harmless for Cavalry->Regulars (sprite and name derive from type); required by
the port's armed-colonist body model for Dragoons->Soldiers. Leave unless kit
bytes ever feed a REF display.

### #780 — moved to dos_bugs_ai.md DB-10
DOS writes profession 0xff; the port guards the write. DOS is the buggy side,
so the entry now lives in [dos_bugs_ai.md](dos_bugs_ai.md) DB-10 (2026-10-22).

### #908 — Colonist jobs menu is narrower than FUN_2f2b_348c
DOS's jobs menu also lists the leave-as rows @JOB 0x13..0x18 in the same list
(the port keeps them in the eject popup only, `colonies_eject_row_offered`).
Its two-page split therefore counts more rows in DOS (`local_a` includes
0x13..0x18) than in the port, which pages only once its own job rows pass 0x10.
The indoor estimates and the paging themselves are ported (port_bugs.md #1195,
#1196).

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
(`FUN_5fef_1b0e` raw 100857), @CARGOCAPTURE (asm 153592), @LOSTTHEIRSCOUTS (dead in DOS too)
(`FUN_5f7a_000e` raw 98871), Europe status line
`FUN_38fd_1bd2` raw 60109 (no port counterpart found; `europe_dock_caption` is the
separate 3694 site), unresolved popup at OVL12 asm 122653 (the 153e USA talk
variants were ported 2026-10-28: `ai_talk_usa_tag`, PEACEUSA via `ai_talk_plural`). If any is ever ported use
`reports_nation_adjective_woi` (09a4) / `reports_nation_plural_woi` (0a1a). Not one
of them: the colony-screen tribal-land demand `FUN_2f2b_2f3e` (2f2b:326a), ported as
`GAME_INDIAN_LAND_PLOT` (`game_dialogs.c`), passes 09a4 the claiming tribe's nation,
and `FUN_15b3_01e0` has no WoI arm above nation 3. Port sites that exist (combat,
ai_diplo talk, ai_contact) already route through the WoI accessors.

### #1160 — Sea-route create wizard offers Europe at the second stop
DOS's create wizard asks both destination pickers with unit -1 (ndisasm
000610EC / 000612B0: `FUN_647e_01c6` with no traveller), so it lists every own
colony and no Europe row; Europe is only reachable later, in the EDIT TRADE
ROUTE screen. The port's second pick of a SEA route already offers the Europe
row and the colonies a ship reaches from the first stop
(`game_trade_open_dest_picker`, `src/core/game_loop_menus.c`). Port-only UI
convenience (user, #300 and 2026-10-22): the route it creates is one DOS can
also build through the editor.

### #1236 — A load keeps a live saved active unit
DOS resumes a loaded game at the human's Move Pieces entry `FUN_2b5a_3b68`
(load sets DS:0x829, raw 121852 / 121944), which writes `0x5392 = 0xffff` and
then picks the first unit awaiting orders through `FUN_2b5a_0e52(0)` (raw
46831-46833), so the saved `head.active_unit` is never used. The port runs the
same entry only when the saved active unit is missing or not selectable
(`game_load_col1_loaded`, `src/core/game_loop_saveload.c`); a mid-turn save
resumes on the unit that was active. Kept so the input-replay regressions
(recorded against that behaviour) keep driving the units they were written for.

### #974 — F7 Naval: Europe-lane ships are EuropeScreen rows, not unit rows
DOS `FUN_3f41_220c` lists a ship in port or mid-Atlantic as an ordinary unit
record on the nation's sentinel diagonal (236+n in port, 232+n / 228+n
westbound, 244+n / 240+n eastbound), in unit-array order. The port's
`col1_bridge_apply` consumes those records into `EuropeScreen`'s
harbor/expected/bound lists, so `reports_naval_build_rows` appends them after
the map rows instead. Only the row ORDER differs; the cells match, settled
statically 2026-11-03:
- Location: `FUN_49dd_02d0` (viceroy_unpacked_2.c:77292-77324) first asks
  `FUN_281f_0302` = `FUN_137f_000a` (in-bounds test, 1 <= x < width-1), which
  fails for every sentinel. Off-map, `(char)(nation - x) == 0x14` (x = 236+n,
  in port) prints the Europe port name `[DS:0x838c + nation*2]`
  (= `eu->port_city`); every other off-map x prints `[DS:0x2e32]` =
  LABELS.TXT line 75 = @MISC row 60 "High Seas" (`addr = 0x2d9c + 2*line`).
- Destination: `nation - x` 0x18/0x1c (westbound) → New World name
  `DS:0x5426 + nation*0x34` (`eu->colony_region`), 0x0c/0x10 (eastbound) →
  port name, 0x14 (in port) → empty (asm OVL06_L0040 0027d4-00283c).

The port's harbor (port_city / empty), expected (High Seas / port_city) and
bound (High Seas / colony_region) rows are exactly those strings. Re-plumbing
the lanes into the unit pool just to reproduce the interleaving is not worth it.

### @REFIT order across harbor and map hulls
`FUN_3844_00f2` (raw 58326-58373) shows @REFIT in descending unit-record order,
one loop over map and harbor (236+n) hulls. The port keeps harbor hulls in
`eu->harbor` with no record index, so `turn_human_ship_repairs` uses the order
the port saves them in (`col1_bridge_capture_europe_lanes` writes them after the
map units): harbor hulls first, last slot first, then map hulls from the highest
slot down. Exact for a game the port saved; after loading a DOS save, a turn in
which a harbor hull and a map hull with a higher DOS record index both finish
repairs shows the two popups in swapped order. Same data-model cost as #974.

### Burial search treasure and the move tail's count test
DOS asks @LOSTCITY4 modally inside `FUN_65dd_0004` (65dd:04eb), so a "search"
whose roll lands on @BURIAL3 has created its Treasure (`FUN_281f_095c`, raw 103685) before
`FUN_465b_0000` tests the unit count at raw 75792; that move then skips the
`FUN_5bfb_3180` contact work and its sentry wake. The port queues the question
(`units_lcr_burial_enqueue_choice`) and pays out after the move tail has run,
so on that move it meets the neighbours and wakes the sentries that DOS leaves
alone. The roll's `gate` is known at queue time but the player's answer is not,
so the count change cannot be settled before the tail without moving the tail
behind the popup. The Fountain of Youth picks, also queued, are counted at
queue time (`game_loop_orders.c`) because they do not depend on an answer.

### Hot-seat edge cases (ported 2026-10-26 as port_bugs.md #1290)
Two narrow differences after a hot-seat `@MULTIREV` "Yes" are listed in
[hotseat.md](hotseat.md) "Divergences".

## Blocked on a live DOS capture

Static reading cannot settle these. Each names exactly what to capture. Do not
change the port's behaviour on a guess.

### #915(c) — moved to tolerated_dos_bugs_ai.md TB-17
The 5952 indoor pass `avail` carry-over and its unverified 0x7fff seed live in
[tolerated_dos_bugs_ai.md](tolerated_dos_bugs_ai.md) TB-17.
