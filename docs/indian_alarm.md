# Indian alarm

Alarm model: what moves the nation-level number, thresholds, the two layers, bands,
escalation. Split from [indians.md](indians.md) 2026-10-18 (hub: graphics, unit and
settlement types, tribe personality, European contact).

## Alarm

### What moves the nation-level number

Every write goes through `FUN_4cc6_00f2` (`ai_diplo_indian_alarm_delta` +
`ai_contact_alarm_delta_00f2`), which applies, in order: France (euro 1) halves
any **increase**; Pocahontas (FF 0x10) halves any **increase** again; clamp
0..100; a **decrease** clears the attack-confirmed bit and, below 0x4b, the WAR
bit both ways; landing on exactly 100 while at PEACE rolls
`rand(0,10) <= (human ? difficulty : 1) + 1` and burns every mission that euro
holds in the nation (@INDIANBURN). Decreases are never scaled.

Map-gen seeds the pair at `rand(0,14) + (human ? difficulty*2 : 0)`
(`FUN_6a09_0006`, port_bugs.md #1001). First contact clamps it down to 20.

Raisers, all verified against the decomp 2026-09-30:

| Event | DOS | Delta |
|---|---|---|
| Seat a colonist on tribal land | `FUN_0000_6582` | `difficulty + 5`, x2 within village distance 3, **+base again** within 2, x2 on a prime-resource tile. Once per plot (port_bugs.md #1002) |
| Pioneer clear / plow / road on tribal land | `thunk_FUN_1000_96aa` (orders dispatch cases 0xb/0xc) | `(human ? difficulty : 0) + 3`, x2 within 3, +base again within 2; **Peter Minuit (FF 2) exempts entirely**; reason code 1 |
| Attack a native unit | `FUN_465b_0000` | `difficulty + 5`, x2 on a village tile, **x6** on a capital; also `attacks++` on that village's word |
| Per-turn encroachment | `FUN_4d56_152e` via `thunk_FUN_1000_a5d0` | +1 per -8 of threat accumulated into `euro_relation_accum` |
| Establish a mission | `thunk_FUN_1000_a5dc` | `count*8 - {25,15,10,5}[band]`, +/-8 at a capital — **negative** (goodwill) for the first missions, positive once you hold several |
| Denounce heresy | `thunk_FUN_1000_a594` | `+/-(quartile+1)`, doubled at a capital, doubled for a Jesuit mission; winner's nation gains, loser's loses |
| Learn a skill (@LEARNSTAY) | `thunk_FUN_1000_a618` | +3 |
| Village trade | `thunk_FUN_1000_a63c` | six arms: `price/0x19 + 1`, `+2`, `+1`, `(tier2>>1)+1`, and the cooling `-2*c4` / `-4*c4` |
| Demand tribute from them | `thunk_FUN_1000_a5f4` | difficulty-scaled (`e == 2` x1.5) |
| Incite a village against a rival | `thunk_FUN_1000_a5b8` (417e mode 2) | +100 |
| Tribe defects to the Crown (WoI) | `thunk_FUN_1000_a5a0` | **+100** to the human, **-100** to the Crown |
| Refuse a food beg | `FUN_5bfb_022e` | village word x1.5 |
| Refuse goods / wagon reparations | `FUN_5bfb_022e` | village word +0x80 (permanently moves that village onto the demand arm) |

Coolers:

| Event | DOS | Delta |
|---|---|---|
| Mission goodwill, per mission village per turn | `FUN_4d56_152e` | `(jesuit ? 4 : 1) << capital`, x2 Las Casas, /2 Sepulveda into the purse; **-1 per +8**. Stacked Jesuit missions outrun almost any raiser |
| Natural cooling roll | `FUN_4d56_152e` | `quartile^2 + 1` draws of `rand(0, 12 - quartile^2)`, each zero = +1 goodwill. ~1/13 per village/turn below alarm 25, ~2.5 at 75+ — **high alarm decays fast, low alarm barely moves** |
| Concede a goods demand | five `FUN_OVL16_L0040__003b*` arms | `-(price[cargo] * qty * 4) / 100`, proportional to what you hand over |
| ~~Pay a gold demand~~ | — | **MISATTRIBUTED, corrected 2026-09-30 (port_bugs.md #1007):** `thunk_FUN_1000_a8b8` (`viceroy_overlays.c:84426-84726`) is the OVERLAY COPY of `FUN_5fef_0f14`, the colony-raid loot resolver — same `rand(0,0x29)` building pick, same `0x8542` colony record, same kind-1..4 arms with `0xfffc/0xfff4/0xfff0/0xfff8`, same `(tribe*9+euro)*2+0x54f6 = 0` tail. So this row duplicated the raid-tail row below and there is **no DOS gold-demand payoff body**. The port's mid-band payoff is a pure stand-in — see the row below |
| **Buy** the plot instead of taking it | `FUN_0000_6582` non-human arm | 0 — no charge at all |
| Gift of gold (022e auto-gift only) | **port stand-in, NO DOS BODY** | -2 (10 gold) / -3 (20 gold), through the same `ai_contact_pair_cool` as the demand arms (port_bugs.md #1007). The cited `FUN_5bfb_102a` (`viceroy_unpacked.c:97105-97125`) is a plural/singular TEXT FORMATTER with no gold or alarm arithmetic — citation dropped. `@ACTIONS` has no gift row and nothing ever enqueued `AI_CONTACT_CHOICE_GIFT`, so the **player-driven** gift action (its amount CHOICE, its three typed English labels and the -5/-1 Small band) was invented and dead, and was deleted 2026-10-01; what remains is the Indian-initiated `FUN_5bfb_022e` visit arm reached through `ai_contact_gift_or_demand` (port_bugs.md #1006) |
| Concede a tools / gold demand | **port stand-in, NO DOS BODY** | flat -3, now spent through `ai_contact_pair_cool` -> `ai_contact_alarm_delta_00f2` (so it clears the attack-confirmed bit, clears the war bit below 0x4b, and runs the 5-point tier clamp) and writing **no** village word: DOS's coolers either zero ONE village's whole word (the acting Brave's home, which these arms never hold) or spend through 0d6c, and no DOS site decays every village of a nation. The **amount** is still invented — there is no DOS demand-payoff body at all (port_bugs.md #1007) |
| Any resolved native-vs-Euro land fight, or a raid | `FUN_5fef_1b0e` / `FUN_5fef_0f14` | that village's whole word -> 0 |
| Raze their capital | `FUN_5fef_1b0e` | alarm clamped down to 15; every village word of the nation zeroed |
| Elect Pocahontas | FF 0x10 | alarm -> 0 with every tribe, and all future increases halved |

### Thresholds that read the number

**Which number, though.** The nation scalar, and only that. `FUN_4d56_4528`
gates on `FUN_281f_030c(bound tribe slot, euro)` — the `alarm_by_player` read —
against `0x4b`, and never touches the per-village word. Across all three
decompiled exports the only DOS *gate* reads of `DS:0x54f6` are
`FUN_5bfb_022e` raw 87333 (`0x7f < word`, the demand-vs-gift arm) and
`FUN_4962_0018` raw 94967 (`word < 0x80`, the AI garrison-quota threat seed) —
both keyed on `unit +0x314a`, the **acting Brave's own home village**, never a
nation-wide maximum. Everything else at that address is a write or the 152e
read-modify-write. Never gate on a nation-wide maximum of village words: that
coupling let one angry village license attacks by Braves homed in calm ones
(port_bugs.md #1003; the gate went with the raid pass, #1093 below).

The village word still matters, and diverges sharply from the nation number,
because the two have very different dampers: the nation scalar is fed through
the `euro_relation_accum` +/-8 spill and the France / Pocahontas halving, while
the village word takes `threat + alarm/5` raw every turn (`FUN_4d56_152e`, a
bare int16 add) with relief only from a mission on that same village, gifts,
conceded demands or a fight. An un-missioned village next to developed colonies therefore climbs
more or less forever while the nation scalar stays low. Note also that the 152e
scorer is **geometric, not topological** — `map_dos_dist < 7`, no pathfinding
and no landmass-shape term — so a village an impassable walk away still accrues
from colonies that are close as the crow flies.

**Deleted 2026-10-18 (port_bugs.md #1093): the post-pulse raid pass
`ai_contact_indian_raids`.** It ran after the ported 021a/465b pulse with
invented nation-scalar bands (>= 40 raid target, >= 55 adjacent ambush with
its own @INDIANWIN chrome, >= 70 march on a colony, >= 80 "hot"), an invented
escort FOLLOW peel (MD 3/4/5, weights 1/2/3) and its own entry into
`FUN_5fef_0f14`. DOS has no such pass: `FUN_4d56_1816` ends with the per-unit
act loop, a Brave's only attack is 021a -> 465b -> `FUN_5fef_1b0e`, and 0f14's
one caller is 1b0e's loser limb (raw 101142), ported as
`ai_contact_colony_raid_repelled_w`. That entry now also draws 0f14's
human-victim cues and @RAID* dialog and the @RAIDWREAK bulletin.

Note on the discharge: a resolved fight zeroes the word of the **attacking
Brave's home** village (`unit +0x314a`), not necessarily the village that is
angriest. That is DOS (raw 101039-101041).

The gift/demand gates (`ai_contact_apply_gift_gold`, `ai_contact_demand_band_ok`,
`ai_contact_gift_or_demand`, the village-menu demand band in `ai_contact_actions.c`)
read `ai_diplo_indian_alarm` alone, as DOS's `FUN_4d56_4528` does (port_bugs.md
#1005). First contact (`FUN_5bfb_022e` raw 96620-96636) only ORs the MET bit and
clamps alarm to 20; it writes no `0x54f6` word, so a pre-contact grudge survives the
welcome (port_bugs.md #1008).

**The `FUN_4cc6_00f2` escalation tail fires from every raiser.** The port splits
00f2 into
`ai_diplo_indian_alarm_delta` (halving / clamp / clears / tier clamp) plus the
alarm-100 peace roll that burns missions (@INDIANBURN), but the roll has two
entry points instead of one: `ai_contact_alarm_delta_00f2(ctx, ...)` and
`ai_contact_alarm_delta_00f2_w(w, ...)` for the positive-delta sites that hold
a `ColonizeWorld` but no turn context — the land-work bill
(`colony_workers.c`, the dominant early-game raiser), the pioneer charge
(`units_pioneer.c`) and the 465b trespass bump (`units_move.c`). DOS has no half
writer (`FUN_281f_0d6c` is a bare thunk to the whole of 00f2) and rolls on every
charge that lands on 100 (port_bugs.md #1004). `ColonizeWorld` carries
`ai_popups` / `messages` for this; a `world_make` view leaves them NULL, which
means mechanics without the dialog. The roll is also skipped outright when the
world or context carries no RNG stream — `dos_rng_range(NULL)` answers its low
bound, which would have made the burn unconditional.
The negative-delta bare callers (`ai_diplo_indian_capital_surrender`,
`effect_pocahontas_reset_alarm`, `units_indian_attack_alarm_vent`) stay on the
bare writer: the tail needs a positive delta landing on 100.

Consumers of the nation scalar read `ai_relation_quartile`
(<25 / <50 / <75 / else), not the raw value.

- **0x4b (75) — the hostility line.** Natives will attack (`units_combat_resolve`);
  village trade and visits refuse with @MADATSHIPS; raid demands bail; the Euro AI
  marks the village a military target (`ai_euro_goals`) and stops building roads
  near it; the map sidebar paints it red.
- `0x4a` (74): village visit and demand visitor blocked.
- `0x47` (71): trade delta cap.
- 55: the **AI**'s auto-trade / gift stops (the human path returns before this gate).
- `0x32` (50): AI defensive schoolhouse build. `0x31` (49): they gift rather than sell.
- 40: gifts refused (`ai_contact_apply_gift_gold`, nation alarm alone).
- `0x19` (25): quartile 1 — the @MISSION band leaves "curiousity"; friendly-tribe
  unit threat stops being zeroed.
- 100: the mission-burn roll above.

Continuous, no threshold: land purchase cost `x(quartile + 1)` (so x1..x4);
village trade `tier2 = quartile << 1` subtracted from what they will pay you
(zeroed for muskets and horses); village goods price `+ alarm * 4`; popup portrait
tier and the Indian Nations report icon. Alarm does not pick the raid kind.

### Two layers

| Layer | Field | Scope |
|-------|-------|-------|
| Nation × Euro | `ColonizeCol1Indian.alarm_by_player[4]` | Whole native nation toward each European |
| Village × Euro | `ColonizeCol1Tribe.alarm[4].{friction,attacks}` | Per settlement; `attacks` also tracks retaliation budget (save atlas) |

Nation turn prelude (`FUN_4d56_1816` → `ai_contact_indian_prelude`) bumps both
layers together on escalation. Clamp alarm ≥ 0 after prelude.

**DS:0x54f6 "grudge/tension" is NOT a third layer — corrected 2026-09-08.**
It is the *village × Euro* row above, read as a whole 16-bit word. The
settlement record lives at `DS:0x54ec` with stride `0x12`, and DOS's own
indexing

```
(tribe * 9 + nation) * 2 + 0x54f6   ==   tribe * 0x12 + 0x54ec + 10 + nation * 2
```

is byte-for-byte field **+10** of that record: `int16_t attitude[4]`, one
**signed** word per European nation. (The apparent "stride 9 words" *is* the
record stride — 9 × 2 = 0x12.) In the port that word is
`ColonizeCol1Tribe.alarm[nation]` = `{uint8_t friction; uint8_t attacks;}`,
low byte `friction`, high byte `attacks` — already byte-exact, already inside
the saved tribe blob (`col1_save.c` tribes read/write), and fed by **all four**
DOS raisers:

| DOS raiser | Amount | Port |
|---|---|---|
| `FUN_465b_0000` trespass | `attacks++` = **+0x100** | `units_move.c` |
| `FUN_4d56_152e` encroachment arm | accumulator | `ai_indian.c` |
| `FUN_5bfb_022e` @INDIANBEGFOOD refused | **×1.5** (`w += w >> 1`) | `ai_contact_apply_beg_food` |
| `FUN_5bfb_022e` LAB_5bfb_0ff2 — @INDIANCITY / @INDIANWAGONS reparations refused | **+0x80** | `ai_contact_apply_reparations` |

The last row is the shared `else` limb of LAB_5bfb_0def's two demand sites (raw
`viceroy_unpacked.c:96973`): the village names a good it wants — out of a
colony's stores (`@INDIANCITY`, tag `0x1866`) or the whole of hold 0 of a
Wagon Train it walked up to (`@INDIANWAGONS`, tag `0x1871`) — and a refusal
costs no goods but adds `0x80` to that settlement's word. Since `022e` reads
`0x7f < word` at its head (`local_10`) as the "this village is sore at you"
latch, **one refusal permanently moves that village off the gift arm and onto
the demand arm** — which is what makes this raiser worth having.

Accepting runs the other limb: the word is zeroed outright and the nation
alarm takes a **negative** `price[cargo] * qty * 4 / -100` delta
(`FUN_281f_0d6c`), walked down in steps of 5 while `alarm + delta >= 0x47` at
the `@INDIANCITY` site only. `price[]` is the DS:0x84BC (`−0x7b44`) byte row
the port already carries as `k_2820_throttle`. `@INDIANCITY` accepts also run
DOS's arming tail — Muskets arm the visiting Brave (type +1) or bank
`indian.muskets`; Horses mount it (type +2) or bank `horse_breeding += 0x32`,
and always `horse_herds += 1`.

The two GAME.TXT sections print their rows in **opposite** order, which is why
DOS reads `local_c != 2 → refuse` at `0x1866` and `local_c == 1 → accept` at
`0x1871`: `@INDIANCITY` is "Man the stockade." / "Hand them over.",
`@INDIANWAGONS` is "Hand them over." / "Circle the wagons.". That text settles
the accept/decline polarity.

Read and write the word through `col1_tribe_attitude` /
`col1_tribe_attitude_set` (`col1_save.h`), which sign-extend / re-split the
two bytes the way DOS's signed `int` accesses do. The word rides the tribe record,
so the tribe-array compactions carry it for free.

DOS's clamping write site is `FUN_4cc6_00f2` (relation-delta,
`ai_diplo_indian_relation_delta` in `ai_diplo.c`): on a negative relation delta
that crosses a 5-point tier boundary, clamp every tribe-of-that-Indian-nation's
attitude word down to `0x20` (new relation <50) or `0x60` (≥50) — the write
rebuilds *both* bytes, so a clamp also drops the `attacks` count. See
`docs/archive/mysteries_catalog.md`'s "0x54f6" entry for the older formula
trace (ignore its "separate table" framing).

**Read side wired 2026-09-08 — `FUN_521d_0896`, the hostility gate**
(`ai_goals.c`, `ai_goals_filter_profession_by_distance_wealth`; the catalog
name is an inferred mislabel kept for SYMBOL_MAP continuity). Raw body,
`viceroy_unpacked.c:87319-87340`:

```
if (3 < param_2) {                       /* owner id: 0..3 Euro, >=4 Indian */
  if (param_3 == 0) return -1;
  iVar2 = FUN_281f_030c(0x521d, param_2 + -4, param_1);   /* DS:0x5b1c alarm */
  bVar1 = 0x4a < iVar2;
  if ((-1 < param_4) &&
     (0x7f < *(int *)((*(char *)(param_4 * 0x1c + 0x314a) * 9 + param_1)
                      * 2 + 0x54f6))) bVar1 = true;       /* DS:0x54f6 tension */
  if (!bVar1) return -1;
}
return param_2;
```

Ghidra prepends the far-call segment word, so at the two `FUN_521d_0906`
call sites (`thunk_FUN_2a1f_056c(0x281f, param_3, iVar5, param_4, local_10)`)
the real order is `param_1` = acting Euro nation, `param_2` = the adjacent
tile's owner id, `param_3` = 0906's own flag, `param_4` = the unit index on
that tile (`-1` for Euro owners and for the settlement probe). The tension
row key is the *tile unit's* `+0x06` home settlement id (`DS:0x314a`,
`ColonizeUnit.home_tribe_id`), not the Indian nation — same key
`ai_contact_colony_raid_repelled_w` and `units_combat.c` write with. The "stride 9" is the
settlement-record stride, so the cell is `col1->tribe[home].alarm[nation]`
read as one signed word (`col1_tribe_attitude`).

Meaning: **an adjacent native raises a Euro-AI contact claim only when that
Indian nation is already hostile (alarm > 74) or that specific village
carries a grudge (attitude word > 127 — so any village with one recorded
trespass, `attacks ≥ 1` = word ≥ 0x100, qualifies, as does friction > 127).**
`param_3 == 0` rejects outright — that is
the `20e6` explorer-flag probe (`ai_euro_20e6_probe_adjacent`), so only the
`0a60` tile-housekeeping probe (`ai_euro.c`, flag 1, sets `act_state = 10`)
can ever see a native claim.

Two more DOS reads of DS:0x54f6; the first needs no port of its own once the word
is identified:

* `FUN_112b_0790` (village map/settlement chrome, `viceroy_unpacked.c:2411-2425`)
  — `iVar6 = (tribe * 9 + nation) * 2; iVar7 = *(int*)(iVar6 + 0x54f6); if
  (iVar7 < 0) iVar7 = 0; *(int*)(iVar6+0x54f6) = iVar7;` (a clamping RMW),
  then `tier = min(iVar7 >> 5, 3)`, forced to 3 when
  `FUN_15dc_00e0(indian, nation) > 0x4a`. Tier picks glyph 10/11/14/12. The
  OpenCol port (`map_panel.c`, `map_panel_draw_tribe_chrome`) sources the tier
  from `tribe.alarm[nation].{friction,attacks}`, which **is** the DS:0x54f6
  word. (DOS's RMW clamps a negative word to 0
  before `>> 5`; with `attacks < 0x80` the word is never negative, so the port
  matches.) There is also a debug-only branch
  (`DS:0x894 & 1`) that prints the raw word via `FUN_1d1d_08fa`.

**`FUN_5952_035e`'s colony threat accumulator.** `ai_euro_goals.c`'s
`ai_euro_colony_threat_seed_5952` is the
DOS-literal transcription of the clean recovery
(`original_sources_annotated/ai/colony_tick_5952_035e.md`, raw 254-324;
`viceroy_unpacked.c:94940-94975` corroborates the accumulator body). It walks
an 11×11 box around each own colony, and for every non-ship unit on a tile
whose **stack head** is foreign it accumulates
`(8 − dos_dist(dx,dy)) × combat_value_x8(unit, mode 1) >> 3`, halved first when
the unit stands on a settlement tile (`FUN_281f_06be`). Euro-owned units get
`v < 2 → 0` and a `+50%` bump when their nation is human (`DS:0x543f`
control byte 0). **Indian-owned units are zeroed by either gate:**
`FUN_281f_030c` alarm (`ai_diplo_indian_alarm`) `< 0x19`, or
`DS:0x54f6[unit.+0x314a][colony nation]`
(`col1_tribe_attitude(&col1->tribe[home_tribe_id], euro)`) `< 0x80` — the row key
is the adjacent unit's own home settlement, the same key the `0906` claim
above and the raid/combat writers use. The tail is
`floor = min(threat, 0x10); threat = max(threat / (walls + 1), floor);
garrison_quota = threat >> 3`, where `walls` = `FUN_281f_0ab0(0)` = owned
Stockade/Fort/Fortress, so walls only cap an already-large threat and never
push the quota below 2. Covered by
`tests/unit/test_ai_euro_war_garrison.c:unit_garrison_quota_threat_seed`
(`make test T=unit_ai_euro_war_garrison`). Still not
ported from the same loop: the `iStack_22` ring-1 counter and the
`iStack_76` `labor_shortage` (+0x8e) formula that consumes it.

**Second write site — raid discharges the attitude word.** `FUN_5fef_0f14` (colony raid loot) unconditionally zeroes
the raiding tribe's word toward the raided Euro nation right before it
returns, for every loot kind (including "Nothing"). Its one entry,
`ai_contact_colony_raid_repelled_w`, does it via
`col1_tribe_attitude_set(&col1->tribe[home_tribe_id], target_euro, 0)` after
`ai_contact_apply_raid_loot`. DOS stores 0 into the **whole word**, so a raid
wipes the village's `friction` *and* its `attacks` count toward that European.

**Third and fourth write sites — `FUN_5fef_1b0e`, both ported in
`units_combat.c`** (raw = `viceroy_unpacked.c`):

| Site | Raw | DOS gate | Index cleared | OpenCol |
|------|-----|----------|---------------|-------|
| combat discharge | 101039-101041 | attacker nation ≥ 4 **and** defender nation < 4, **and** (`local_10 < 0` ∥ `DS:0x8db8 != 0` ∥ attacker won). `local_10` = `FUN_281f_0614(x, y, -1, -1)` nearest-colony scan on the **defender's** tile, `0x8db8` = that scan's distance output, so the two leading terms together read "the fight was not on a colony tile". The `else` limb is DOS's handoff to `FUN_5fef_0f14`, which carries its own clear. | `tribe[attacker.+0x314a].alarm[defender_nation]`, whole word | `units_indian_attack_tension_clear`, called from both arms of `units_resolve_land_combat_ff` |
| capital razed | 101289-101298 | `local_c != 0` (dwelling destroyed) **and** `local_ce != 0` (record +3 bit 2, capital). Same block first clamps alarm **down** to 15 when above (`FUN_281f_030c` → `FUN_281f_0d6c(-(alarm-15))`) and then draws `@INDIANBOW` (0x1cd7 — the DS string at 121248+0x1cd7 is
`INDIANBOW`, not `INDIANDEAD`; the port's `rich_capital` arm draws it with the
tribe chief portrait selected from the post-surrender alarm tier). The `0d6c` delta lands in `FUN_4cc6_00f2`, whose negative arm clears the WAR bit **both ways** whenever the new alarm is < 0x4b (the clamp lands at 15, so always) — this war-clear side effect, not any peace-bit store, is DOS's forced peace on capital razing; the port routes the clamp through `ai_diplo_indian_alarm_delta` so the same clear fires. | walks the whole DS:0x539a settlement array, zeroing the attitude word of every record whose `+2` nation byte (4..11) equals DS:0x8d52 — but 0x8d52 is the TRIBE index (0..7, the value 0d6c takes), so DOS clears the dwellings of nation `tribe index`, never the razed tribe's own (those keep 00f2's 0x20/0x60 tier clamp; America seed-100 resync 236). The port compares with tribe index + 4 and clears the razed tribe's own dwellings ([dos_bugs.md](dos_bugs.md) DB-22) | the `rich_capital` arm of `units_combat.c`'s village-destroy path, beside `ai_diplo_indian_capital_surrender` (which models the alarm half) |

Net rule for the combat site: **every** resolved native-vs-European land
fight discharges the raiding tribe's tension toward that European, except a
native attack that *loses at a colony* — DOS routes that limb to
`FUN_5fef_0f14` instead, whose own tail clear then fires.

**Colony-raid handoff.** DOS's `else` limb (raw 101142):
a native attacker that *loses* while the defender stands on a European colony
tile does not simply die. 1b0e skips its whole alarm/tension block and calls
`thunk_FUN_2a1f_06c8(indian_nation, colony, home_tribe, local_ca,
attacker_type)` = **`FUN_5fef_0f14` in full** (this limb is 0f14's one caller),
and `local_a6` stays 0 so there is no vent here either. Ported as
`ai_contact_colony_raid_repelled_w` (`ai_contact_raid.c`, called from
`units_resolve_land_combat_ff`'s attacker-loses arm). `local_ca` = 0f14's
`param_4`, which bypasses the walls check at 0f14's head; it is set by the
Brave-vs-human-Artillery auto-loss (raw `100573-100577`, ported as
`units_combat_brave_vs_human_arty` — see [combat.md](combat.md)), and the
same latch is passed straight through as this handoff's `forced` argument.

**0f14's own alarm tail.** Each of the four
loot arms ends with the same two lines and then falls into one shared call
(raw 99961 / 99992 / 100006 / 100031 for the constants, 100033 for the call):

```
uVar10 = FUN_281f_0a38(0x281f,*(undefined2 *)0x8d50,uVar5);
if ((uVar10 & 2) != 0) goto LAB_5fef_16d2;
uVar6 = 0xfffc;                    /* per-kind constant */
...
FUN_281f_0d6c(0x281f,param_1 + -4,uVar5,uVar6,0);
LAB_5fef_16d2:
*(undefined2 *)((param_3 * 9 + uVar5) * 2 + 0x54f6) = 0;
```

so the real argument list is `0d6c(indian_nation, euro, delta, 0)` and every
delta is **negative** — a raid *discharges* alarm, exactly like the
unconditional tension zero on the next line and like 1b0e's `local_a6` vent
below:

| DOS `local_6` | outcome | `uVar6` | delta | port `AiRaidKind` |
|---|---|---|---|---|
| 1 | warehouse goods stolen | `0xfffc` | −4 | `AI_RAID_STORES` |
| 2 | building razed (index 0..0x29) | `0xfff4` | −12 | `AI_RAID_BURN` |
| 3 | ship on the colony tile damaged | `0xfff0` | −16 | `AI_RAID_SHIP` |
| 4 | gold plundered | `0xfff8` | −8 | `AI_RAID_GOLD` |
| 0 | nothing looted | — | no call at all (kind-0 arm jumps straight to `LAB_5fef_16d2`, raw 100013) | `AI_RAID_NOTHING` |

Kind 3 is the **ship** kind: its walk only accepts unit types `0xd..0x12`,
the same ship band `FUN_4cc6_03f8`'s threat ring skips (see the "Units at
sea" row below), and hands the hull to `FUN_5fef_0352` for damage. There is no
colonist-kill or "wreak" kind (the port's invented `SCALP` / `WREAK` kinds were
deleted, port_bugs.md #828/#829).

The gate is the same one the `local_a6` vent uses: `FUN_281f_0a38` =
`FUN_15b3_0004(indian_nation, euro)` = `indian[].euro_diplo[euro]`, bit 1 =
WAR, and the branch **skips** the `0d6c` call — so there is no relief once
the tribe is at war with that European. `FUN_281f_0d6c` = `FUN_4cc6_00f2` =
`ai_diplo_indian_alarm_delta` (the France/Pocahontas halving inside it is
positive-delta-only, so these pass through raw).

Ported as `ai_contact_raid_alarm_tail` (`ai_contact_raid.c`), called from
`ai_contact_colony_raid_repelled_w` immediately before the DS:0x54f6 clear, as
DOS orders them. A raid never raises alarm: the old raid pulse's positive
fandom bump (+4/+12/+16/+8) had no DOS site and went with the fandom alarm
drips (port_bugs.md #291).

**`local_a6` alarm vent** (raw 101043-101196), applied as
`FUN_281f_0d6c(nation, euro, local_a6, 0)`. All values are **negative** —
attacking vents alarm, matching the tension clear in the same block:

| Situation | `local_a6` | Port |
|-----------|-----------|------|
| natives raze the Euro colony (`bVar28 && local_c`) | −50 flat | `units_try_capture_foreign_colony` Indian arm, burn limb |
| natives beat an undefended colony, dwelling survives | `difficulty − 10` (the `difficulty` term only when the Euro side is human) | same arm, colonist-kill limb |
| natives win an ordinary field fight (`!bVar28 && bVar8`) | `difficulty/2 − 5` (same human-only term) | `units_resolve_land_combat_ff` attacker-wins arm |
| natives lose a field fight | 0 — no delta | nothing to apply |

`local_a6` at raw 101137 and the capital clamp-to-15 at raw 101290-101292 are
the **only** two alarm writes in the whole of `FUN_5fef_1b0e` (100251-101310,
scanned 2026-10-01 for `FUN_281f_0d6c` / `FUN_281f_030c` / direct `0x5b1c`).
The capital arm's gate is `local_c != 0 && local_ce != 0` — destroyed **and**
capital — so a razed NON-capital settlement takes no alarm or relation delta
at all: DOS falls straight through to `LAB_5fef_362a` (port_bugs.md #1010).

Note the polarity of the human term: a **higher** difficulty gives back
*less* alarm relief.

The gate is `FUN_281f_0a38(DS:0x8d50, euro) & 2` == 0.
`FUN_281f_0a38` = `FUN_15b3_0004(a, b)`, which for `a >= 4` reads
`a * 0x4e + 23000 + b` — the Indian record's relation byte toward that
European, i.e. the port's `indian[].euro_diplo[euro]`, whose bit 1 is the WAR
bit. So the vent applies **only while the tribe is not already at war with
that European** (`units_indian_attack_alarm_vent`, `units_combat.c`); it is not a
treaty check. `FUN_281f_0d6c` is `FUN_4cc6_00f2` = `ai_diplo_indian_alarm_delta`.

**Model note.** DOS runs 1b0e once per attack and picks exactly ONE row; the
port splits the same beat, resolving the fight and then walking the winner
into the colony, where the Indian arm plays the `bVar28` rows. Two statics in
`units_combat.c` keep that one-row-per-attack property: `g_units_colony_autodefender`
(DOS `bVar28`, raised by `units_revere_defend_colony_tile` around its resolve
call, so an auto-spawned colony defender does not take the field-fight row)
and `g_units_indian_combat_vent_done` (cleared at the top of every
`units_try_move`, so a native that beats a real garrison and steps in does not
also take the undefended-colony row).

### Gameplay bands

Used by contact / mission / raid gates ([indian_contact.md](../original_sources_annotated/ai/indian_contact.md)):

| Band | Typical use |
|------|-------------|
| **&lt; 40** | Cool: gift / teach (no friction decay: the port's old meet-pulse `friction--` had no DOS writer, deleted port_bugs.md #1106) |
| **40–54** | Mid: demand/payoff; hard-bargain trade (~45–49); teach refused |
| **≥ 55** | Refuse trade / gifts / convert |
| **≥ 80** | (Mission burn `FUN_4cc6_0000` is NOT this band: it needs alarm == 100 AND the at-peace bit `0a38 & 0x40`, then `rng(0,10) <= difficulty+1`, raw 80900-80912) |
| **≥ 75** | Speak With Chief kill band — `thunk_FUN_1000_a60c` (overlays.c 77494-77654) gates the whole friendly switch on `alarm < 0x4b`; at or above it, and on a failed `alarm >> 2 < rng(0, seasoned ? 140 : 100)` roll below it, the outcome is **deterministic** `@CHIEFKILL` unless the nation owns FF 6 Coronado (`@CHIEFBORED`). The old "≥ 90 / 95 scout displace, ~¼ RNG kill" figures were never in DOS — corrected 2026-09-27. |

First-contact **reject** (`@INDIANSHUN`) is the plain `FUN_4cc6_00f2` +100 on an alarm already
clamped to 20, so France lands on 70 and others on 100. It writes no village word (port_bugs.md #1228).

### Escalation and pacify

| Driver | Effect |
|--------|--------|
| Encroachment | **RETIRED 2026-09-03** (port_bugs.md "alarm rises incredibly fast"). The former fandom +2/turn unit/colony bump was not DOS; DOS's only per-turn encroachment raiser is the `152e` threat accumulator (colonies within distance 7 + military ring → `euro_relation_accum`, −8 crossing = alarm +1). The friction ±1/turn relation-tick band drift retired with it. |
| Units at sea | **Never counted** (verified 2026-09-04). `FUN_4cc6_03f8`'s 20-tile threat ring skips a tile when `FUN_281f_0768` → `FUN_13e4_0074` says terrain class `0x19`/`0x1a` (water), and its stack walk ignores unit types `0xd..0x12` (ships). So combat units riding a ship raise no alarm; `@MADATSHIPS` / `@DONTKNOWSHIPS` are refusal chrome with no alarm bump. `ai_indian_village_threat` already matched (`map_tile_is_water`, `units_is_sea`, and `units_is_on_map` excluding aboard passengers). |
| French (Euro nation 1) | Half-rate bumps; +1 auto-trade reach |
| Pocahontas | Half-rate bumps; elect zeros this nation's tribe friction/attacks + `alarm_by_player` |
| Missions | Establish (`a5dc`) has no alarm gate; alarm only picks the quartile of the `{-25,-15,-10,-5}` delta and the `@MISSION0..3` variant. Burn at alarm 100 + peace bit (see band table). Audit 2026-09-22: port_bugs.md #557 |
| Difficulty prelude | **RETIRED 2026-09-09** (smell #72): the `2+(4-diff)` chance / `5+(4-diff)` bump table was invented and ran off indian record +6, a byte no DOS export touches. `ai_contact_indian_prelude` now only clamps. The difficulty terms that *are* real: `FUN_465b_0000` trespass (`difficulty+5`), `FUN_0000_6582` land work (`difficulty+5`), WoI defect roll `rng(0,(5-difficulty)*2)==0`, map-gen seed `rand(0,14)+difficulty*2` — [difficulty.md](difficulty.md) §Indians |

`@ATTITUDE` labels: Content, Uneasy, Restless, Angry, War (+ `@ATTITUDINAL`
modifiers Extremely/Very/Rather/Somewhat/Slightly). **Nothing in the port or the
decomp reads either catalog**: the F9 Indian Adviser prints the `@LEVELS` tech word
and picks the chief portrait `113 + alarm quartile` (`reports_indian_build_rows`),
so alarm never surfaces to the player as text. Method, dead leads and the capture
that would settle it: [known_divergences.md](known_divergences.md) #1009. Do not
invent a threshold table.
