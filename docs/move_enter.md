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
| `DOCK` | Ship onto own (or de Witt peace) colony land |
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
| Land | Land (clear/forest/hills/mtn) | Cost `terr_cost[class]*3`; enter if afford / full MP / gamble | DOS `terr_cost` table via `map_move_cost_*` (NAMES MP scale; Brave keeps `*3`); road/river pair→1; full-MP + `range(1,cost)` | Partial (`*3` unit-MP scale PARK) |
| Land | Road pair (both FA `&0x0a`) | Cost **1** | `map_move_cost_step` both roads → 1; else dest road still halves | Done |
| Land | River both + cardinal | Cost **1** | Both river + axis → 1; else dest river still halves | Done |
| Land | Ocean / HS (25/26) | Embark if own ship has room (`4720`); else domain deny | `BOARD` via `units_find_boardable_ship`; else domain deny | Done |
| Ship | Ocean / HS | OK (`4720`); HS eastward without sail → reason **5** | Water OK; HS east without goto/sail → deny | Done |
| Ship | Map edge | Reason **4** | Out-of-bounds → edge | Partial |
| Ship | Own colony land | Dock | `can_enter` + disembark | Done |
| Ship | Foreign Euro dock | Peace / de Witt | de Witt peace berth | Partial |
| Ship | Bare land | Landfall UI reasons 2/3 | `@LANDFALL` Stay / Make Landfall (one unit) | Done |
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
| Missionary / combatish | Native village | Meet / raid | Enter + Meet CHOICE | Partial (deep `4528` PARKED) |
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

Village raid `4528`/`2820` VGA, full `465b` foreign diplo/war UI, Euro mid-planner
`20e6`, coastal fort ship-slow bit7/repair, VGA-identical combat chrome, full
treasure ransom CHOICE.

Combat strength `FUN_157e_004a`/`015e` + `1b0e` peels (artillery/ambush/SoL/REF/
difficulty) + best-defender `0000` + promote/demote + outcome apply `0352`/
naval damage + structural `@EUROPEWIN`/`@LOOT*`/`@SHIP*`/`@DEMOTE` popups are
wired. Combat Analysis (`636c`) Done.

## See also

- [manual_gap.md](manual_gap.md) — Units / Combat rows
- [assets.md](assets.md) — map keys, landfall note
- [ai_transcription.md](ai_transcription.md) — AI move / combat
- [`original_sources_annotated/ai/move_spent.c`](../original_sources_annotated/ai/move_spent.c)
