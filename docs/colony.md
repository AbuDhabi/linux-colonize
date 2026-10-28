# colony.h design notes

Design/rationale/DOS-layout prose moved out of `src/core/colony.h` so the header stays
declaration-focused. Each section below corresponds to a one-line pointer comment left in
the header at the symbol's declaration; contents here are verbatim (delimiters stripped).

## COLONIZE_COLONY_FIELD_TILES

The work-plot ring. DOS sizes it per colony: `DS:0x329[FUN_15eb_0470()]`
over the table {0,4,8,12,20} (VICEROY.EXE file offset 121248+0x329), with
`FUN_15eb_0470` (raw 9636-9645) = `min(FUN_15eb_039e(10),2)+2`. `039e(10)`
(raw 9561-9578) walks the @BUILDING chain that starts at row 10 through the
`-0x707a` next-row byte and counts the rows the colony owns, i.e. the two
Town Hall upgrade rows 0x0a/0x0b (NAMES.TXT:177-178). DOS's own "can build"
gate FUN_15eb_3650 (raw ~13674) hard-zeroes both rows, so no colony stock
DOS can produce ever leaves tier 2 and the usual ring is the 8 adjacent
plots -- but a save may carry the bits (colony buildings mask bits 9-11 are
@BUILDING rows 9/10/11), so the ring is computed per colony from
`colonies_work_plot_count()` and the array is sized for all 20 slots.
port_bugs.md #593.

## ColonizeColonist.turns_in_job

FUN_364b_0688 education: turns in current workplace. This IS the DOS
colony record's per-colonist nibble at +0x60 (`FUN_15eb_0c7a` read /
`FUN_15eb_0cbc` write, raw 10202-10240) — reached as `FUN_281f_0d1c` /
`FUN_281f_0a7e`. The teaching loop (raw 57503-57539) reads it, adds 1
and writes it back for EVERY colonist each turn, and the writer clamps
at 15, so ordinary long-serving workers saturate at 15 — exactly what
DOS campaign saves show. `FUN_15eb_1068` (raw 11256-11258) zeroes it on
a real job change. Persisted through col1_bridge (permuted with the
canonical colonist reorder, DOS raw 47225/47241).

## ColonizeColony.tiles

Surrounding field slots: colonist index or -1. Slots 0..7 are the port's
clockwise ring N,NE,E,SE,S,SW,W,NW; slots 8..19 are the DOS outer ring in
DS:0xc8/0xde order, (0,-2),(2,0),(0,2),(-2,0),(-1,-2),(1,-2),(-1,2),
(1,2),(-2,-1),(-2,1),(2,-1),(2,1). Only the first
colonies_work_plot_count() plots are workable; the rest stay -1 and
round-trip into the save's 20-slot colony+0x70 array verbatim.

## COLONIZE_CUSTOM_HOUSE_DEFAULT_MASK

Custom House per-cargo enable mask (ColonizeCol1CustomHouse bit layout:
bit0=Food … bit15=Muskets). Default is 0x1ede (excludes food, horses,
lumber, tools, trade goods, muskets).

## ColonizeColony.labor_shortage

Col1 +0x8e LABOR demand counter. Unload/join decrements (FUN_521d_5b66
~91589). Stamped unconditionally by every AI colony tick from the real
FUN_5952_035e formula since 2026-09-09 (raw 94029-94045, ported in
ai_euro_colony_threat_seed_5952): it is a target headcount
`clamp(max((pop + on_tile_colonists - 1) / 2, garrison_quota), <= n / 2)`
(+1 under WoI, floored at 1 by an adjacent enemy), NOT a boolean
"this colony wants labor" — a hand-seeded value does not survive a tick.
Cite: save_format_map.md; euro_unit_act case 0x0b.

## ColonizeColony.garrison_quota

Col1 +0x1e garrison fortify quota. DEC on fortify/'A' assign; seeded by
threat>>3 (FUN_5952_035e, ai_euro_colony_threat_seed_5952).
Cite: save_format_map.md; euro_unit_act §2d3.

## ColonizeColony.pop_on_map / fort_on_map

Col1 +0xba / +0xbe fog-of-war snapshots per European viewer (smcol
population_on_map / fortification_on_map). Written by the tile-reveal
writer FUN_364b_1b4c whenever a nation's sight touches this tile, and
seeded to pop=1/fort=0 by FUN_13f1_00a6 (±5 reveal on founding). pop 0 =
that nation has never observed the colony (FUN_364b_1b76 gate).

## ColonizeColony.cargo_idle_turns

Col1 +0x8f cargo-idle turns. INC cap 0x7f each Euro inventory pulse
(FUN_5952_035e); cleared when goods unload into the colony. Haul target
score adds idle*8. Cite: save_format_map.md; viceroy ~87677 / ~90249.

## ColonizeColony.improve_timer

Col1 +0x8c improve timer. INC cap 0x7f (FUN_5952_035e); gates AI pioneer
plow/road until timer ≥ `DS:0x2f78[class] + 2` (`+ 4` for a forest clear when
the colony carries the wants-clear bit; raw 93663-93666 / 94492-94505,
ai_euro_land.c) and the schoolhouse expert election
(`((chain == 3) + chain) * 4 <= timer`, ai_euro_colony_jobs.c); cleared on a
successful improve and on that election. Cite: save_format_map.md.

## ColonizeColony.pending_build_reveal

DOS DS:0x34a: the building that just finished construction here, stored
as index+1 so a zeroed colony means "nothing to reveal". FUN_2f2b_6cd4
(colony-screen bring-up) only runs the "new building appears" reveal —
clear bit, redraw, set bit, redraw — and only then pushes event 0x54
(COLDIG 13 hammering + cheering), when DS:0x34a >= 0. Set by
colonies_try_complete_building, consumed on colony-screen open.

## ColonizeColony.food_shortfall_latch

Port-only within-turn food latch: set by the production tick when this
colony actually went negative on food, read by the same tick's starve
kill and by the @FOOD1/@FOOD2 "depleted" nag suppressor. It used to ride
in colony_flags bit3, but DOS spends that bit on the inefficient-
government latch (FUN_364b_0688 phase D, +0x1c & 8) — see
COLONIZE_COLONY_FLAG_INEFFICIENT_GOV — so it lives here instead. Not
bridged: it carries no state DOS keeps.

## ColonizeColony.prod_bells_phase_a

Port-only Phase A compose snapshot (smell audit #62). DOS composes this
colony's bells and crosses inside FUN_364b_0688's own prologue
(`FUN_281f_0c22` → `15eb_1f72`) and hands the SAME word to both
consumers — the nation/Congress tally (`FUN_291f_09f8`, viceroy 57231)
and the rebel dividend (57392). The port instead re-tallies bells and
crosses for the whole nation in `turn_run_nation_ticks`, which for AI
nations runs AFTER their colonies have ticked, i.e. after Phase C/D
moved the SoL accumulators and latch bits and after F/G/H/J rewrote the
roster — a systematically one-step-ahead number. `turn_produce_one_colony`
stamps what it composed at the Phase A boundary here; the nation tally
prefers it when `prod_compose_stamp` matches this turn, and otherwise
(human colonies, whose EOT runs later in TURN_PROC_FINISH; direct
callers with no col1) falls back to a live read, which for them already
IS the pre-tick state. Not bridged: DOS keeps no such field.

## COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP / _FRIGATE

DOS colony +0x1b bits 0/1 — FUN_4962_0018 census phase 3 (11×11 box scan
around each own colony): a foreign ship (type 0x0d..0x12, combat byte != 0)
with a short navigable route (FUN_6662_0906 sea flood cost 0..5) sets bit
0x02 when it is a Frigate (type == 0x11, literal check) and bit 0x01
otherwise. Earlier port naming had bit 0x02 as "Man-O-War" — wrong; census
trace 2026-08-19 (census_tally.md) pinned the 0x11 literal.

## COLONIZE_COLONY_AI_MILITARY_SURPLUS / _SHORT_DEFENDERS

DOS colony +0x1b bits 0x04 / 0x08 — the defender-count pair, both written by
FUN_5952_035e (viceroy_unpacked.c 94195-94199) from the same comparison:
  local_74 = wanted defenders, local_82 = defenders present
  local_82 < local_74                            -> set 0x08 (short of want)
  local_74 + (local_74 > 1) < local_82           -> set 0x04 (surplus)
So 0x08 is the "short of defenders" side and 0x04 the "has spare military"
side. (0x04 was once named NEEDS_MILITARY — inverted.)

Both writers are ported in
ai_euro_colony_threat_seed_5952 — the same DOS body that already produced
garrison_quota. `local_74` (wanted defenders) is raw 94150-94193;
`local_82` is NOT a raw defender count but this nation's LAND military
homed to the colony (+0x314a origin) minus the ones the on-tile walk
already counted as garrison, i.e. the OFF-STATION surplus (raw 94063-94071).

The DOS per-tick clear is `+0x1b &= 7` (raw 94142) — it keeps 0x01/0x02
(the census's own disjoint mask) AND 0x04. 0x04 is therefore sticky by
design: only its consumers clear it.

Readers of 0x04 (all DOS's own; grep the macro, not a line number):
  - ai_euro_20e6_surplus_recall_arm — FUN_521d_20e6's land-unit act (raw
    85332, emitted twice by the decompiler, again at 90168): read-and-clear,
    one surplus unit recalled home per tick;
  - ai_euro_20e6_wander_step's flag ladder;
  - ai_euro_5952_absorb_equip — FUN_5952_035e's join-colonist pass (raw
    94247): one of three disjuncts that admit a Soldier/Dragoon, then
    cleared. Its `local_42`/`local_3e` are by-profession census buckets
    `aiStack_68[0x13]`/`[0x15]`, not independent locals (decode in
    ai_euro_colony_jobs.c).

## COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR

DOS colony +0x1b bit 0x20 — "send a Pioneer to CLEAR this colony's ring",
distinct from 0x80's road/plow errand. FUN_5952_035e writes the PAIR
`|= 0xa0` (0x80|0x20) at raw 94200-94206, from its full-ring scan
(raw 94082-94117), in two cases:
  1. `ring_tiles - 1 <= unproductive` && `forest_tiles > 1` — the ring is
     essentially all forest/water/poor ground;
  2. `good_food_tiles < (pop + 3) >> 2` && `clearable_forest != 0` &&
     `forest_tiles > 1` — too few open food tiles for the head count, with
     at least one forest worth clearing.
"unproductive" (local_144) = off-map + water + forest + cleared ground
whose DS:0x2f7b food byte is < 2; "good_food" (local_e) = water +
cleared ground with food >= 3; "clearable" (local_c) = forest whose
CLEARED counterpart (class & 7) has food > 2.

The writer is ported (ai_euro_refresh_colony_ai_flags). DOS's four readers
are all inside FUN_5952_035e's later building/expert passes (raw 94422,
94454, 94499, 94751). Raw 94751 IS ported since 2026-09-10 — the
field-specialist restore pass at the tail of
ai_euro_colony_tick_28c8_reassign, where this bit (or a live lumber
shortfall, DS:0x8e64) is what lets a colony take a SECOND Expert
Lumberjack onto its ring. The other three live in the colony-tick
pioneer-improve pass (raw 94330-94560), which spawns a phantom worker and
has no OpenCol counterpart; see docs/archive/smell_audit_2026-09-10.md, the
"FUN_5952_035e building / expert passes" section.

## COLONIZE_COLONY_AI_WANTS_PIONEER_WORK

DOS colony +0x1b bit 0x80 — "worked surround tiles want Pioneer work":
FUN_5952_035e sets it when any worked ring tile lacks road (fa_flags &
0x0a == 0) or any worked farmland tile (terr class < 8) lacks plow
(& 0x40). FUN_521d_0a60's +800 registration arm counts an idle Pioneer at
the colony only when this is CLEAR (no work here → ship him out).

## COLONIZE_COLONY_FLAG_INEFFICIENT_GOV

DOS +0x1c bit3 (FUN_364b_0688 phase D, ~57470): the inefficient-government
latch. Set the turn a colony's Tory head count reaches 10-difficulty and
cleared when it drops back under, so @INEFFICIENT / @EFFICIENT each fire
once per crossing — and, because it is a save field, they stay fired across
a save/load. The port's food latch is `food_shortfall_latch`, not this bit.

## COLONIZE_COLONY_FLAG_SMALL_AI / _WAGON_TRAIN

DOS +0x1c bit 0x10 (col1_save.h `small_colony_ai`) — a one-shot latch.
Both DOS sites are in FUN_5952_035e: the
read-and-clear hand-off at raw 94143-94146 (`(+0x1c & 0x10) && pop < 0x20
-> +0x1b |= 0x10 (NEEDS_COLONISTS); +0x1c &= 0xef`), which is ported in
ai_euro_colony_threat_seed_5952 and consumed in `ai_euro_5952_absorb_equip` (ai_euro_goals.c), and the
writer at raw 95845-95847 (`pop < 10 -> +0x1c |= 0x10`), ported in place
at the bottom of the 5952 cascade (`ai_euro_colony_jobs.c`). Do not re-add
an unconditional `pop < 10` stamp outside the cascade: that pinned the flag
on every turn (smell audit C3).
The "chain of 2a1f_05b4 probability gates" this note used to name is a
misreading corrected 2026-09-10: thunk_FUN_2a1f_05b4 is the RTLink stub for
FUN_5952_0214, the tick's build-candidate helper (`try_build(id)`, id in AX,
returns 0 when it picked), not an RNG roll. The writer sits at the bottom of
that helper's 24-candidate cascade — asm LAB_OVL15_L0000__00270d — which is
ported whole (port_bugs.md #483). Every dropped id is
recovered in docs/archive/smell_audit_2026-09-10.md (seventh-wave lead 5).

DOS +0x1c bit 0x20. DOS derives it in the
FUN_521d_6d8e prelude: raw 93142 clears it on each own colony, then the unit
loop at raw 93148-93157 sets it on the colony a Wagon Train (type 0x0c) is
HOMED to (+0x314a origin), not the one it is standing on. The port
re-derives it every AI turn in ai_euro_refresh_colony_ai_flags. Its only
reader is the 5952 Wagon Train unit-project gate at raw 95762
(`(+0x1c & 0x20) != 0 || turn > 0x63f || ...`), ported as the `2385` arm in
`ai_euro_colony_jobs.c`.

## COLONIZE_COLONY_FLAG_COASTAL

DOS +0x1c bit 0x40 — the coastal bit. Written exactly once, at founding,
by FUN_364b_1ba8 (raw 58105-58110) from map_tile_is_open_sea_adjacent()'s
predicate: an inset 8-neighbour is ocean/high-seas AND the lowest-region
such neighbour is water region 1 (the open sea), so lake-only and map-edge
sites do not qualify. Nothing in the image recomputes or clears it (the
tick's flag-byte clear at raw 94145 is `&= 0xef`, bit 0x10 only), so after
founding it is pure save-carried state. Port writers: colonies_found
(the stamp) and a set-only self-heal in ai_euro_refresh_colony_ai_flags —
never a per-turn recompute that can clear it. DOS readers include the Docks
buildability filter (raw 13688, building id 7) and the 20e6 delivery pick
(raw 2054). Smell audit 2026-09-10 D4.

## colonies_reveal_founded_w

FUN_364b_1dd6 founding tail: for each nation 0..3 that owns FF 6 (Coronado —
`FUN_15eb_3960(nation, 6)`), call FUN_13f1_00a6 on the new colony, revealing
the ±5 square around it for that nation and seeding pop_on_map=1 /
fort_on_map=0 on every colony inside it that nation has not observed yet.

The ±5 sweep is Coronado's effect, not a plain founding effect: without him
founding reveals nothing beyond the founder's own unit sight. col1 may be
NULL (then nobody has Coronado and this is a no-op), as may map.

## colonies_reveal_all_for_nation

Coronado's elect-time sweep: FUN_4345_0342 case `param_2 == 6`
(viceroy_unpacked.c 73155-73159) walks every colony index 0..colony_count
with no owner test and runs FUN_13f1_00a6 (the ±5 square + pop_on_map=1
seeding) for the electing nation — foreign colonies included.

## colonies_indian_land_owner_tribe

Tribal-land owner of a tile: index into col1->tribe of the nearest village
(same continent) whose tech-tier radius covers (x,y); -1 when the tile is
not tribal land. FUN_15eb_26e4 / FUN_15dc_006a rule. Ignores the
purchased bit — pair with colonies_indian_land_purchase_gold for that.

## colonies_indian_claim_tribe_from_w

Same test as the colony screen actually runs it (port_bugs.md #366): DOS hoists the
continent lookup out of the 5x5 loop and reads it at the COLONY tile
(origin_x, origin_y), and clears every Ocean / Sea Lane cell outright.
Also answers the plain "what claims this tile" question with origin ==
the queried tile.

## colonies_assign_field_w

DOS's `FUN_0000_6582`, the shared colony `tiles[]` writer. Besides seating the
colonist it carries the native land-work reaction, because DOS runs that from
this writer and not from the colony screen:

- the plot's claim comes from `colonies_indian_claim_tribe_from_w`
  (DOS's 5x5 table at DS:-0x7262, `FUN_15eb_26e4`);
- a **non-human** colony first tries to buy the plot outright at
  `colonies_indian_land_purchase_gold`, and pays only when it holds 1.5x the
  price (`gold - price >= price/2`, raw 8785-8798) — a successful buy raises no
  alarm at all;
- otherwise alarm rises by `base = (human ? difficulty : 0) + 5`, doubled when
  the nearest village (DS:0x8db8, the distance `FUN_4cc6_0356` wrote) is within
  3, plus `base` again within 2, and doubled once more when the tile holds a
  prime resource (`FUN_0000_3ca0`, the same hash as `map_resource_type_at` —
  it is **not** a road test).

The charge is once per plot: `FUN_15eb_26e4` (raw 12851-12854) leaves a plot
that already has a worker out of the claim table, and it builds that table at
colony-pass entry, before the AI placement section's
`memset(colony+0x70, 0xff, 0x14)` wipe. `ColonizeColony.plot_was_worked_mask`
carries that fact across the port's equivalent clear, so the AI's yearly
clear-and-reseat does not re-charge. Verified against real DOS saves: replaying
`original_saves/early-game-seed-100/TURN1.SAV` six turns headless reproduces
TURN7.SAV's `alarm_by_player` on all 8 tribes x 4 nations exactly.

DOS's `*(char *)0x34d` gate is `FUN_15eb_28c8`'s trial-seat flag; the port scores
without seating. The committing seat's tail is `colonies_plot_seat_tail_w`. DOS's
jobs menu also runs it on opening (28c8 for each field job, a re-seat, the job counter
zeroed; DOSBox-confirmed, port_bugs.md #1205); the port does not (docs/dos_bugs.md DB-54).

## colonies_indian_land_pay

Pay for tribal land at (x,y): debit *gold by cost (when gold non-NULL),
INC indian.lands_bought (FUN_479b_00ca), stamp the purchased bit on the
Col1 mask (0x10) and map layer2 (MAP_LAYER2_PURCHASED) — the
FUN_281f_068c(...,0x10,1) / FUN_15eb_0668 write every DOS "offer gold"
arm (@INDIANLAND/@INDIANROAD/@INDIANFOREST → @INDIANBRIBE) performs. @INDIANLAND is
asked only by the colony-screen plot drop (`game_colony_plot_land_gate`, DOS
FUN_2f2b_2f3e), never by Build Colony, which founds free (port_bugs.md #1175).

## colonies_indian_land_purchase_gold

Gold to buy Indian homeland tile (FUN_4cc6_07c2). Manual/wiki Minuit:
Indians no longer demand payment → 0 via founding_fathers_nation_has(FF 2).
Returns 0 outside homeland radius (village 1 / capital city 2; pdf Indian Land),
or when tile already has MAP_LAYER2_PURCHASED / Col1 mask 0x10 (WELCOME gift
or prior buy). DOS also spends this from pioneer plow/road + colony tile-buy;
those callers remain PORT outside this module.

## colonies_found — the founder's seat

DOS FUN_364b_1ba8 raw 58075-58086: the founding body clears the plot map
(`FUN_1d1d_0dae(colony+0x70, 0xffff, 0x14)` — no tile owned by anyone) and
seats the founder through `FUN_281f_0c36 -> FUN_15eb_1068(slot, 0)`, i.e.
**occupation byte 0 = field job 0 (food)**, never the Town Hall; the
unit-less arm also sets pop 1 and `FUN_281f_0cae -> FUN_15eb_0e8c(slot,
0x1c)` (Free Colonist). DOS picks no plot here — the plot comes from the
shared 28c8 auto-assign the colony screen / AI colony tick runs next. The
port therefore runs `colonies_seat_new_colonist` (its 28c8 picker) on the
founder and falls back to field job 0 when no map is bound. port_bugs.md #929.

## colonies_found_with_indian_land_w

Found with FUN_4cc6_07c2 Indian land charge when tile is homeland.
Deducts from *gold; fails (−1) if short. Minuit → free. col1/gold NULL →
same as colonies_found (plain found still does not charge).

## ColonizeBuildingRow

NAMES.TXT @BUILDING row indices. The order is DOS's own (the colony
building bitfield is indexed by it — see docs/save_format_map.md), so the
row number, not the English name, is the stable identifier. Use these with
colonies_building_row instead of colonies_find_building(pool, "Fortress"):
the port keeps no MicroProse names in the binary, and a renamed row must
still resolve.

## colonies_building_name_row

@BUILDING row for a building NAME read from the catalog (exact match against
the names the last colonies_load_buildings call saw; a fixture-built
pool resolves through the test-only hook below). This is how code that is
handed a name asks "which building is this?" without the port carrying any
building names of its own. -1 when unknown.

## colonies_toggle_custom_house_cargo

Toggle `cargo_type` in this colony's Custom House per-cargo autosell mask
(custom_house_bits — see europe_custom_house_autosell). Player-facing:
clicking the Custom House opens a checklist of eligible cargoes; this is
what a row-click flips. False (no-op) without a built Custom House, or
for a cargo outside 0..COLONIZE_CARGO_COUNT.

## colonies_profession_may_teach

True when the @JOB school level of `profession` is 1..3 — DOS's `level < 4`
teach gate. False for Free / Indentured / Criminal / Convert / @JOB 18
"Teacher" / unset.

## colonies_field_scan_order

port_bugs.md #584 — DOS plot scan order. DS:0xc8/0xde list the work plots as
N,E,S,W,NW,NE,SE,SW; the port's own slot order (colonies_field_tile_delta)
is the clockwise MAP_DIR8 one. `colonies_field_scan_order(step)` maps a DOS
scan step 0..7 to the runtime tile_index, so a scan that must reproduce a
DOS tie-break ("first index wins", FUN_15eb_28c8 raw 13126 elects strictly
greater) iterates `colonies_field_scan_order(step)` instead of `step`.
Returns -1 outside 0..7.

## colonies_plot_blocked_mask

DOS-LITERAL FUN_15eb_23f2 (raw 12695-12800): the "may this colony work this
plot" bitmask for ring slot `tile_index` (0..7, the port's slot order), 0 =
workable. Bits: 0x10 off-map / outside the work radius / unexplored, 0x80
foreign-owned tile held by a fortified armed unit, 0x02 Lost City Rumour,
0x04 Indian village, 0x20 another colony's centre, 0x40 plot already worked
by another colony, 0x08 own centre (unreachable here). Both DOS consumers —
the AI plot scan FUN_15eb_28c8 (raw 12993) and the human area-view click
FUN_2f2b_3fa6 (raw 50917) — require the byte to be 0, and 3fa6 simply
ignores the click otherwise (no popup, no status line).

## plot-eviction

`colonies_recompute_plots_w` (colony_workers.c) = DOS FUN_15eb_3930 (raw
13794-13808, port_bugs.md #1080): snapshot the mask above for all 20 slots (268e),
clear every worked plot whose mask is non-zero (287e; the colonist keeps his
field job), then reseat each plotless field-job colonist via 28c8, else the
Carpenter fallback (2ea0). No workers-per-building cap there: the 3-cap is
UI-only, so a 4th+ Carpenter is legal and the building strip shows them all
(DOS FUN_2f2b_12cc is uncapped and steps each worker by a clamped pitch, so a
crowded strip overlaps; `colony_screen_building_worker_layout`). Wired at colony-screen entry, EOT production (per colony, before
`turn_produce_one_colony`) and the AI colony tick. DOS also runs it on join
(364b_1aec/1b1a) and in the F6 adviser (3f41_1bec); the port does only on
the FUN_521d_20e6 AI join (ai_euro_land.c), not the other joins or the adviser.
Fixture trap: a synthetic map with an unrevealed `seen` plane makes every
plot 0x10 and evicts everyone — call `map_reveal_all`.

## colonies_admit_unit_w

Admit a land unit on the colony tile into the colony (despawn map unit).
Transfers founder-style loot into the warehouse. Returns colonist index or -1.
`col1` is optional: when non-NULL, also runs the La Salle immediate-Stockade
check (founding_fathers_la_salle_check) if this join crosses pop 3.

## colonies_eject_colonist

Remove a colonist onto the colony map tile as the given role (spends warehouse
gear). Compacts the colonist list. Returns new unit id or -1.
role: 0=Colonist, 1=Pioneer, 2=Soldier, 3=Scout, 4=Dragoon.

## colonies_fortification_defense_bonus_percent

Colony fortification defense bonus percent for land combat (0 / 100 / 150 / 200).
Cite: docs/building_production.md + fandom Stockade/Fort/Fortress —
Stockade +100%, Fort +150%, Fortress +200% (highest built). Wiki: Stockade
replaces Fortify benefit inside — callers should not also ×2 fortify when >0.

## colonies_set_occupancy_map

Bind the live world map so founding and abandoning a colony can keep
layer2's MAP_OCCUPANCY_HAS_CITY bit current. That bit is what the map
renderer reads to hide a settlement's garrison and what the road art keys
off. NULL unbinds.

## colonies_set_col1_context

Col1 context for colonies_capture's DOS side effects (FUN_5fef_1b0e capture
tail): rebel dividend ×2/3, per-nation colony/pop tallies, treasury share
plunder (peacetime), war-relation reset, crown-capture REF-threshold bit.
NULL = plain owner swap (tests without a save). The game loop re-binds it every
frame (`game_update`, next to the occupancy map), so founding, join/leave
(rebel divisor +-100), abandon and the `colony_counts[]` tallies see it from the
first action after a load / new game (port_bugs.md #1175c).

## colonies_abandon

DOS colony delete (FUN_364b_1e64, raw 58154-58200) minus the array compaction:
the port zeroes the slot in place, so the DOS colony index is the ordinal among
live slots (`colonies_dos_index` / `colonies_id_at_dos_index`), which is also the
order col1_bridge writes the COL1 colony array. Live
`game->col1.trade_route[].stop[].colony_index` holds that DOS index (999 =
Europe) and `colonies_abandon` runs the DOS remap on it (stops at the dead
colony removed with the unit cursor walk, later stops `-1`); `colonies_found`'s
hole-fill arm shifts them `+1`. Readers resolve through `colonies_id_at_dos_index`,
writers convert the picker's colony id (`game_trade_stop_word`). Unit homes stay
colony ids at runtime and are translated at capture. Needs the Col1 and unit-pool
contexts bound (`game_update` does, every frame). Ceilings in the code comment
(port_bugs.md #1179).

## colonies_equip_tools_take

Tools an equip actually takes out of `available` (warehouse stock plus
anything the unit is already carrying): whole 20-tool steps, capped at 100 —
a Pioneer legitimately walks with 20/40/60/80/100. Returns 0 when there is
not even one step. Shared by both equip paths (a colonist leaving the
colony, and a unit already standing outside it).

## colonies_list_eject_roles_ex

Fill out_roles with the "Leave as" rows DOS offers this colonist, and
out_enabled (optional) with each row's enabled state: DOS lists a row whose
cargo the colony cannot cover but draws it GREYED (FUN_15eb_3454 → 0xffff),
and offers an Indian Convert nothing but the Colonist row. See the comment
on the definition in colony.c. The short form passes out_enabled = NULL.

## colonies_list_eject_roles_gear

Same row list for a body that carries its own gear (a unit outside the
colony): add_* is what the unit already holds and counts toward the row
gates, profession is the body's own @JOB (every >= 0x13 row gate in
FUN_15eb_3454 reads it: Convert 0x1b gets the Colonist row only, a Jesuit
0x18 always gets the Missionary row and may lose the Colonist row).
colonies_list_eject_roles_ex is this with add_* = 0 and the colonist's own
profession.

## colonies_eject_row_offered

Is a "Leave as" row offered at all for this body (FUN_15eb_3454's three
`return 0` arms, raw 13556-13570)? Shared by both row builders and both
appliers. DOS's Jesuit Colonist-row arm (DS:0x8dc6 slot-indexed nation test)
is dropped: [dos_bugs.md](dos_bugs.md) DB-9.

## colonies_eject_role_gear

Gear a "Leave as" row takes from the stock: Pioneer = whole 20-tool steps
capped at 100 (FUN_15eb_1068 raw 11250-11253, via colonies_equip_tools_take),
Soldier 50 muskets, Scout 50 horses, Dragoon both, Colonist / Missionary
nothing. Returns false for a role FUN_2f2b_348c never offers. Shared by
colonies_eject_colonist and the outside-unit path in game_loop (GL-12).

## COLONIZE_UNIT_BUILD_ARTILLERY etc.

Col1 stores buildable *units* as `building_in_production` raw codes past the
real @BUILDING table's range — NAMES.TXT's @BUILDING section never lists
them (they live in @UNIT instead), so colonies_building_type() returns NULL
for these. FUN_15eb_32f8 (viceroy_unpacked.c raw 13423) decodes exactly
seven of them, 42..48 = @UNIT rows 11..17; see units.h
(COLONIZE_UNIT_BUILD_CODE_*) for the decode and the cost arithmetic.
Man-O-War is @UNIT row 18 and deliberately out of range.

## colonies_unit_build_info

True + fills name/hammers/tools_cost if raw_code is a unit-type
construction project; false for a real building_type index or anything
else. Thin wrapper over units_build_project_info (DOS FUN_15eb_33aa).

## colonies_destroy_building

Destroy a built building (not Town Hall). Clears has_building and moves any
workplace colonists in that building to idle (building_type=-1). Cancels
matching construction project. Returns false if missing/Town Hall/invalid.
Cite: @RAIDBURN building loot needs safe destroy + workplace clear.

## Colony-screen cheat keys

DOS `FUN_2f2b_6372`'s tail (`2f2b:63c2`, raw 55699-55765; port_bugs.md #1173
part 3): reached after the normal arms, which consume none of these keys, and
only with `head.game_options.cheats_enabled` (DS:0x5383 bit 5,
`game_cheats_enabled`). The port dispatches them in `game_colony_cheat_key`
(game_loop_colony.c) from the text-input loop of `game_colony_screen_keys`, on
the bare colony screen only (DOS runs every sub-panel in its own loop). The
state logic is `colonies_cheat_*` (colony.c, `unit_colony_cheat_helpers`).
COLONIZE/README.TXT lists them as the "useful cheat functions on colony screen".

| Key | DOS arm | Port |
|---|---|---|
| `T` / `t` | 6820: pop++, `FUN_15eb_1068(old, 0)` (occupation 0, no plot), `FUN_15eb_0e8c(old, 0x1c)` (profession Free Colonist) | `colonies_cheat_add_colonist`: unit-less colonist, `field_job` 0, `UNITS_JOB_NONE`, cap `COLONIZE_COLONY_POP_MAX`; no rebel_divisor change (that +100 is in 1068's other arm); the plot comes from the redraw's 3930 recompute |
| `S` / `s` | 6882: `0e8c(sel, 0e18(sel))` | `colonies_cheat_profession_from_job` (occupation -> profession, 0x17 -> 0x15); an outside unit selected (sel >= pop) gets its type's default @JOB (`units_type_default_job`) |
| `$` | 6896: the owner's (`colony+0x1a`) treasury dword, `nation * 0x13c + 0x8832`, += 1000 | `europe_nation_gold_add` on the colony's owner |
| `%` / `^` | 68b4 / 68c8: `+0x9a + 2 * DS:0x33a` += 100 / -= 100 clamped at 0 | `colonies_cheat_stock_add` on `csv->selected_cargo` (set by pressing a warehouse good); no capacity cap, no 16-bit wrap |
| `!` | 68e6: bits `+0x84..+0x89` := 0xff, clear 0x10 / 0x1f / 0x0b / 0x0a / 0x1e, `++(+0x95)`, `++(+0x96)` | `colonies_cheat_all_buildings`: every `@BUILDING` row 0..41 except 10, 11, 16, 30, 31; `warehouse_level` / `capitol_level` byte `++` (Custom House goods mask untouched) |
| Space | 6950: `FUN_364b_0688(DS:0x8dc6)` | `game_colony_cheat_produce` -> `turn_cheat_colony_tick` (turn_colony.c): the EOT per-colony body (`turn_run_one_colony_production_w`: coastal fort, claim stamp, plot recompute, bell step, production) plus the unit / building completion sweeps for that colony. Twice = two ticks. Leaves the screen if the colony died |

T, S, `!` and Space end in `FUN_291f_0648` = `FUN_2f2b_2c92`, the full redraw,
which opens with `FUN_281f_0c22` -> `FUN_15eb_3930`; the port re-runs
`colonies_recompute_plots_w` there. `$ % ^` end in the partial redraw
`FUN_291f_078c`, so no recompute. `turn_colony_free_production` (no col1 /
Europe) is a unit-test helper only; no UI path reaches it. Replays
`colony_cheat_keys` / `colony_cheat_keys_locked`.

## ColoniesBuildableOpts.col1

DOS FUN_15eb_3650 wagon arm reads colony_counts[nation] (DS:0x9298) and
unit_type_counts[nation][12] (DS:0x924c + n*0x13 + 12) straight out of the
census window; NULL simply skips the cap.

## colonies_list_buildable keep-current tail

colonies_list_buildable ports FUN_15eb_3650 (raw 13641-13746) per code. Its
tail (raw 13741-13746, port_bugs.md #1218) lists the colony's current project
(+0x94) whatever the gates said. DOS drops it when unit record #code (its unit
array indexed by the build code) is type 1 and the colony owns @BUILDING
`local_e`; the port does not reproduce that (dos_bugs.md DB-59). So a just
built project stays in the CHANGE list as its "(COMPLETE)" row, and a colony
with 14 buildings and all 7 unit projects open reaches 23 rows and two pages.
The building arm is literal too: predecessor owned and the row not owned (no
"nothing above it owned" test, so a pillaged lower tier can be rebuilt), Docks
needs a water plot in the DS:0x329 ring inside the map's 1-tile inset, Drydock
the +0x1c coastal bit, Shipyard only its predecessor, Warehouse Expansion
capacity <= 299. The Euro AI's cascade (FUN_5952_0214 -> 0b8c -> 3650) runs
after its prologue stored 0xff in +0x94 (raw 95711), so
ai_euro_5952_cascade_setup lists with no current project.

## colonies_wagon_cap_reached

DOS-LITERAL FUN_15eb_3650 (raw 13736-13740) / FUN_364b_0114 (raw 56926-56933):
a nation may not own more Wagon Trains than colonies —
`colony_counts[n] <= unit_type_counts[n][12]` blocks the project and, at
completion, pops @NOMOREWAGONS. Both counters are the FUN_4962_0018 census
window, refreshed every EOT. False when `col1` is NULL.

## colonies_construction_gold_cost

Gold to rush-buy the current project's remaining hammers+tools, or 0 if
none/nothing missing. DOS formula (FUN_2f2b_5e44, clean decompile —
original_sources_decompiled/viceroy_unpacked.c:52683):
  hammers_deficit × 13, plus tools_deficit × (nation's current Europe tools
  price + 4)
  when tools are short, the whole sum DOUBLED outright if the colony
  hasn't banked any hammers at all yet (colony->hammers == 0) — DOS
  charges a steep premium for rushing an unstarted project. The DOS price
  byte is at nation record +0x5a (`DS:0x8862 + nation*0x13c`): trade begins
  at +0x4c and cargo 14 is Tools (port_bugs.md #917).

## colonies_try_complete_unit_construction

Unit-type construction completion (colonies_unit_build_info) — same
hammers/tools gate and bookkeeping as colonies_try_complete_building, but
spawns the map unit at the colony tile instead of setting has_building[]
(needs `units`, which colonies_try_complete_building doesn't take, since
a real building never spawns anything). Returns the new unit id on
success, -1 otherwise (no project, not a unit-type project, or short on
hammers/tools/spawn).

DOS-LITERAL FUN_364b_0688 raw 57748-57769: when the colony is short on
tools, a human colony (nation < 4 and player[nation].control == 0) is
refused (@NEEDTOOLS, turn_emit_needtools_notice's job); any other colony
(nation > 3, or player[nation].control != 0 — AI Euro or Crown) has its
tools stock set to the requirement right here and completion proceeds.
col1 may be NULL (headless callers with no save loaded), which is treated
as "no player table" and falls back to the short-on-tools refusal for
every colony — the same as the port's pre-#755 behaviour.

## ColonizeColony.build_queue

Port-only QOL (no DOS counterpart): the projects a colony starts, in order, as
each completion frees `building_in_production`. Shift-clicking a row in the
Construction picker — or Shift+Enter on the selected row — appends to it
(`game_colony_queue_construction`) and leaves the picker open; with nothing in
production that Shift pick starts the project instead, still without closing, so
the next Shift pick queues behind it. A
plain pick or "(No Production)" wipes the queue
(`game_colony_commit_construction`, `colonies_build_queue_clear`). Queued rows
are drawn with their 1-based position in yellow ahead of the name.

Both completion paths (`colonies_try_complete_building_ex`,
`colonies_try_complete_unit_construction`) pop the head through
`colonies_set_construction_ex`, so a queued start passes the same gates and
clears the same `+0x1c` build-complete latch as a manual pick; a head that no
longer qualifies (already built via an upgrade, wagon cap reached, population
dropped) is dropped and the next one tried, so the queue cannot wedge. Every
caller — the EOT sweeps in `turn_colony.c` and the inline complete in
`turn_production.c` — routes through those two functions, so nothing else
needs to know about the queue.

Persisted in the save's port extension block (`BQUE` chunk, docs/savegame.md)
keyed by colony tile, the same way `VTIN` carries village intel: DOS never sees
it and drops it on a re-save. Depth `COLONIZE_COLONY_BUILD_QUEUE_MAX` = 8; a
payload from a deeper build is truncated on load. Covered by `unit_colonies`
(`unit_build_queue`) and the input replays `colony_build_queue` (queuing in the
picker) and `colony_build_queue_complete` (BQUE load, the head starting on each
completion through the Space cheat tick, a head dropped for population).

## colonies_buy_construction

Rush-buy the current project: tops hammers up to the completion
threshold and tools up to the requirement (deducting colonies_construction_
gold_cost's formula from *gold), same as DOS's FUN_2f2b_5e44 — it does
NOT complete the project. Completion only happens through the normal
per-turn checks (turn_run_colony_building_completion /
turn_run_colony_unit_construction, both unconditional once-per-turn
passes), matching DOS: rushing a project just fills the tank, and the
next turn's construction processing notices it's full and finishes the
job. Fails if no project or insufficient gold. Updates *gold on success.

## Warehouse, spoilage and trade

Moved to [colony_goods.md](colony_goods.md) (warehouse capacity / full chrome / specialty cargo / spoilage, Jan de Witt foreign-colony trade, trade-route stops).

## COLONIES_CHAIN_* enum

Shared building upgrade chains (one copy, in colony_build.c; col1_bridge.c and
the colony screen read it; duplication audit CO-13).

colonies_building_chain(chain) returns the chain's NULL-terminated name
list, lowest tier first, or NULL for an out-of-range id.

***THE ENUM ORDER IS A SAVE-FORMAT CONTRACT.*** col1_bridge.c maps chain
position i onto bit i of the matching ColonizeCol1Buildings group word and
walks the chains in this order. Reordering the enum, or inserting a tier
into the middle of a chain, rewrites every colony's building mask on the
next export. Appending a new chain at the end is safe; nothing else is.

17 entries for DOS's 15 screen categories: Capitol and Stable are split
out of the Town Hall / Warehouse categories they share a colony-screen
slot with, because the save format gives each its own bit group and its
own level byte (+0x96 / +0x95).

## colonies_blit_settlement_icon

Draw ICONS.SS settlement marker `sprite` (0-3) at (px,py) — the sprite's
own top-left, not a tile origin — then recolor its stored blue flag
pixels to `nation_id`'s own color (nearest match within
`active_palette`; no-op if `active_palette` is NULL or nation_id isn't
0..3). Every colony-icon draw site should go through this, not a plain
ss_blit_sprite, so the flag always matches the owning nation. See
unit_chrome_nation_shades_for_palette (unit_chrome.h) for why a
palette-aware recolor is needed at all.

## colonies_render_on_map

DOS FUN_112b_0c64 draws the two labels with two different fonts, from two
separate DS font pointers: the colony name with FONTINTR (DS:0x268a) and the
population badge with FONTTINY (DS:0x89e). Both were loaded at startup by
CODE_153:75c2:2e5b / 2e78 from the DS strings "fontintr" / "fonttiny".

