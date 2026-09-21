# Move-into-tile authority matrix

Player-facing rules for attempting to enter a destination tile. Authority:
DOS `FUN_465b_0000` ([`move_spent.c`](../original_sources_annotated/ai/move_spent.c)),
naval validity `FUN_4720_015c` / UI `FUN_4720_049e` (reason `DS:0x9e4e`),
combat `FUN_5fef_1b0e`, colony join `FUN_647e_0094`, village `FUN_4d56_4528`.

Linux entry: `units_enter_probe` → `units_can_enter` / `units_try_move` /
`game_try_unit_move` ([`units.c`](../src/core/units.c), [`game_loop.c`](../src/core/game_loop.c)).

Status: **Done** / **Partial** / **Missing** / **PARKED**.

---

## Outcomes

| Code | Meaning |
|------|---------|
| `OK` | Enter / stack; charge MP |
| `DOCK` | Ship onto own colony land |
| `LANDFALL` | Ship → bare coastal land → unload **one** passenger (ship stays) |
| `BOARD` | Land → ocean/HS with own ship that has room → embark |
| `VILLAGE_SHIP` | Ship → native village → `@DONTKNOWSHIPS` / `@MADATSHIPS` (not landfall) |
| `COMBAT_LAND` | Resolve land combat then enter if attacker wins |
| `COMBAT_NAVAL` | Resolve naval combat then enter if attacker wins |
| `BOUNCE` | Deny; foreign non-combat / peace gate (no fight) |
| `DENY` | Domain, edge, MP, village squat, etc. |

---

## Terrain × domain

| Mover | Dest | DOS | Linux | Status |
|-------|------|-----|-------|--------|
| Land | Land (clear/forest/hills/mtn) | Cost `terr_cost[class]*3`; enter if afford / full MP / gamble | DOS `terr_cost` table via `map_move_cost_*` (NAMES MP scale; Brave keeps `*3`); road/river pair→1; full-MP + `range(1,cost)` | Done (unit MP = DOS thirds for every unit, `UNITS_MP_PER_TILE = 3`; the `*3`-scale PARK closed 2026-08-29 with T1.8) |
| Land | Road pair (both FA `&0x0a`) | Cost **1** | `map_move_cost_step` both roads → 1; else dest road still halves | Done |
| Land | River both + cardinal | Cost **1** | Both river + axis → 1; else dest river still halves | Done |
| Land | Ocean / HS (25/26) | Embark if own ship has room for the boarder's `@UNIT` **size** (`4720_00e0`) | `BOARD` via `units_find_boardable_ship(..., need_space)`; else domain deny | Done |
| Land **aboard a ship** | Land occupied by another nation | Reason **9** = `@LANDFIRST` — no amphibious assault | `COLONIZE_ENTER_LANDFIRST` (bugs.md #485) | Done |
| Ship | Ocean / HS | OK (`4720`); **already on HS** + eastward without sail order → reason **5** | Same (`units_can_enter`); entering the lane from ocean is always legal, and a Go To may target a lane tile → `game_ship_sail_to_europe` on arrival | Done |
| Ship | Map edge | Reason **4** | Out-of-bounds → edge | Partial |
| Ship | Own colony land | Dock (`465b` reads the settlement owner `281f_06be`; a DOS own-colony stack is always own) | `can_enter` + disembark; foreign units squatting on the tile (port leftovers) are ignored (bugs.md #553) | Done |
| Ship / Wagon | Foreign Euro colony | — | Never enters: `FUN_5f7a_0662` → `FUN_5f7a_020e` trades from outside and spends the whole allotment ([foreign_colony_trade.md](foreign_colony_trade.md)) | Done |
| Ship | Bare land | Landfall UI reasons 2/3 | `@LANDFALL` Stay / Make Landfall (one unit; sentry cargo OK); passenger spends its **whole** allotment (465b_05ca shore crossing, 2026-09-04); ship −1 MP on Make Landfall | Done |
| Ship | Native village | `4528` ship abort (`@DONTKNOWSHIPS` / `@MADATSHIPS`) | `VILLAGE_SHIP` + `ai_contact_try_ship_village` | Done |

---

## Occupancy

| Mover | Occupant | DOS (`465b`) | Linux | Status |
|-------|----------|--------------|-------|--------|
| Any | Same nation | Stack | Stack | Done |
| Land combat (`5236≠0` / attack>0) | Foreign land unit | Fight → `5fef_1b0e` | `157e` strength + combat analysis | Partial |
| Land non-combat (`5236==0`) | Foreign unit | Bounce (+ optional dialog) | Bounce via probe | Done |
| Land | Foreign unit at peace | Diplo/war UI tails | Bounce if `!ai_diplo_at_war` | Partial |
| Ship | Foreign ship | Naval fight path | Naval `157e_004a` + analysis | Done |
| Ship | Foreign land | Block / landfall | Block / landfall | Done |
| Any | Empty | Enter | Enter | Done |
| Ship leaves tile | Same-tile Sentry land | Auto-board to capacity | `units_board_sentries_from_tile` on ship move | Done |

---

## Settlements

| Mover | Tile | DOS | Linux | Status |
|-------|------|-----|-------|--------|
| Land | Own colony | Stack / join | Enter | Done |
| Land | Foreign Euro + defender | Combat | Combat | Partial |
| Land | Empty foreign Euro | Capture / Revere | Capture-on-enter + Revere | Done (thin) |
| Land settler | Native village (no colony) | Illegal squat | Deny | Done |
| Missionary / combatish | Native village | Meet / raid | Meet from adjacent (stay put): real `@ACTIONS` menu per unit type (P8.8); combatish Attack Village commits the move; the invented Attack/Leave warn survives only as the unmet-tribe fallback | Done (`2820` haggle / VGA PARKED) |
| Wagon | Colony tile | Exhaust MP (`465b:08f8`) | Exhaust on enter | Done |

---

## AI vs human

| Topic | Difference |
|-------|------------|
| Partial overspend | Human: `rng`; AI Brave pulse often commits |
| Go-To | Skips gamble steps (`units_can_afford_move_cost` only) |
| Village Meet | Human popup; AI auto-Accept |
| Naval combat | Shared `units_resolve_naval_combat*`; human move now probes combat |
| Fog | Does **not** block enter (reveal after move) — intentional for port |

---

## PARKED (not this track)

Village deep `2820` VGA trade, full `465b` foreign diplo/war UI, Euro mid-planner
`20e6`, VGA-identical combat chrome. Coastal fort bit7 + Drydock Done; village
`4528` is fully dispatched (human `@ACTIONS` menu P8.8 + AI arm, 2026-08-27/28) — [combat.md](combat.md).

**Playable combat bar Done** — see [combat.md](combat.md) (ransom CHOICE, `@CAPTURED*`/
`@BURNED*`, unit capture / seizure popups, attack-exhausts-MP).

## See also

- [combat.md](combat.md) — odds, peels, promote/demote, coastal fort
- [manual_gap.md](manual_gap.md) — Units / Combat rows
- [assets.md](assets.md) — map keys, landfall note
- [port_plan.md](port_plan.md) — AI move / combat
- [`original_sources_annotated/ai/move_spent.c`](../original_sources_annotated/ai/move_spent.c)

## Shore crossing spends everything — 2026-09-04

`FUN_465b_0000` at `LAB_465b_05ca` adds the step cost, then:

```
if (ocean_or_hs(from) != ocean_or_hs(dest)
    && euro_settlement_owner(from) < 0
    && euro_settlement_owner(dest) < 0)
    moves_spent = unit_max_mp(unit);
```

So any move that crosses the water/land boundary outside a colony spends the
unit's entire allotment — a landfall onto bare coast, and equally boarding a
ship from open shore. A colony on either end (a dock) exempts the step, which
is why loading and unloading in port stays cheap; an Indian village does not
(`FUN_281f_0696` clamps owners above 3 to −1). Linux:
`units_move_crosses_shore`, applied in `units_try_move` and
`units_unload_passenger` (bugs.md: "dragoons should have their entire movement
spent from stepping off a ship onto land").

## Hold charge = `@UNIT` size, and no amphibious assault — 2026-09-17

`FUN_4720_00e0` (viceroy_unpacked_2.c raw 74628-74665) is the "does this
tile's stack fit on these ships" pass and the authority on hold accounting:

```
for each ship on tile:  room[i] = 0x5237[type] - unit[+0x3150]   /* cargo cap - goods holds */
for each non-ship unit on tile with 0x5238[type] < 99:
    first ship with size <= room[i]:  room[i] -= size            /* else the stack does not fit */
return (param_2 <= room)                                          /* candidate's own size */
```

So a passenger costs its **size** column, not one slot. `@UNIT` sizes: land
units 1, **Treasure 6**, ships + Wagon Train 99 (the cannot-board sentinel,
bugs.md #482). With cargo capacities Caravel 2 / Merchantman 4 / Galleon 6 /
Privateer 2 / Frigate 4 / Man-O-War 6, only a Galleon (or a Man-O-War) can
ever lift a Treasure, and it then has no room for anything else — the
manual's "treasure needs a Galleon" is that arithmetic, not a type check
(bugs.md #484). Col1 interop is untouched: `holds_occupied` is goods-only and
riders travel on the aboard link chain, so DOS recomputes the size charges
itself.

`FUN_4720_015c` reason **9** (raw 74720-74728) fires when a land unit whose
own tile is ocean/high-seas — i.e. one riding a ship — steps onto a tile
whose `tile_tribe_or_presence` owner is another nation: enemy stack, native
village, or an empty enemy colony alike. The `FUN_4720_049e` jump table at
`4720:060a` maps reason 9 to `push 0x1429` = **@LANDFIRST**, "Land units
cannot enter an enemy occupied square from on board a ship." There is no
amphibious assault in DOS and no shipboard walk-in capture (bugs.md #485).
The port keeps `@LANDFIRST` on the *ship*-mover direction too as a stand-in;
DOS refuses that step with no reason word and shows nothing (bugs.md #486).

Ship **sight radius** is not a `@UNIT` column either: `FUN_13f1_02f8`
(`CODE_17:13f1:02f8-0356`) builds it in DI from hardcoded type compares —
1 by default, 2 for types `0xf/0x10/0x11` (Galleon / Privateer / Frigate),
2 for any non-ship under de Soto (`FUN_15eb_3960(nation, 7)`), +1 for type
5 (Scouts) — and passes it to `FUN_13f1_02b4` in DX, which only derives the
ship/land flag. Man-O-War (`0x12`) is deliberately absent: radius 1. The
port's `units_sight_radius` already matched; the citation now names the asm.

## Sea lane (high seas) — corrected 2026-08-28

`FUN_4720_015c` (viceroy_unpacked.c ~76048) raises reason **5** only when
**all** of these hold: destination terrain class `0x1a` (high seas), the
ship's *current* tile is also `0x1a`, the step is eastward
(`ship.x < dest.x`), and the ship's order (`+0x314c`) is neither 3 (Go To)
nor 2 (Trade Route). The port tested the destination alone, so the first
step from ocean onto the lane was denied, and because `units_set_goto`
validates with the same probe *before* the order is set, a Go To aimed at a
lane tile was refused too (bugs.md: "I can't move a ship onto a sea lane
either way").

Arrival behaviour: a **Go To** whose destination is a lane tile sails the
ship to Europe when it reaches **that destination tile**
(`game_ship_sail_to_europe`, the same tail the `H` command uses —
passengers, holds and treasure ride along). Lane tiles merely crossed on
the way do not sail it: the reason-5 gate is suppressed outright while the
order byte is 3 (Go To) or 2 (Trade Route), so a DOS ship under orders
sails through the lane, and DOS's only order-3 Europe departure is the goto
menu's own "Europe" entry (destination 999 → `FUN_2b5a_1dfc`,
viceroy_unpacked.c 42798-42819), which leaves immediately from wherever the
ship stands. Plain arrow-key steps onto the lane do not sail either: DOS's
own sail intent is exactly the order byte the reason-5 gate reads.

Reason 5 is not a hard deny (2026-09-03): the UI handler (`FUN_4720_049e`
case 4, `viceroy_ndisasm.asm` 0x3FEA6) asks **@SAILHOME** — Yes sails for
Europe, No **commits the eastward step anyway** (lanes are fully
traversable); with the WoI declared (`DS:0x5382` bit0) it shows
@EUROPENOTLEAVE and still commits the step. Linux:
`game_try_unit_move` intercepts `COLONIZE_ENTER_BLOCKED_HS_SAIL` →
`AI_POPUP_TAG_SAILHOME` / `game_commit_sea_lane_step`.

A **Trade Route** Europe stop (colony_index 999) really crosses now: the
ship sails from the lane tile (`game_trade_route_retarget` stamps
`EuropeHarborShip.trade_route_plus1/trade_stop`), is serviced in harbor
(`game_europe_service_trade_harbor` — DOS `FUN_479b_0bd0` sell-unload /
buy-load), sails back and re-arms TRADE_ROUTE orders on spawn
(`game_europe_deliver_bound_ships`).

