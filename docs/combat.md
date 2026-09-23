# Combat mechanics

Player-visible combat resolution for Sid Meier's Colonization (1994): land and
naval odds, fortification / terrain / peel modifiers, Combat Analysis, promote /
demote / capture / loot, and coastal Fort/Fortress batteries.

Move-into-tile fight-vs-bounce gates stay in [move_enter.md](move_enter.md).
Village warn→Attack (`4528`) Done thin; Euro `20e6` combat peels Done thin;
deep −0x6790 / `2820` VGA stay AI docs.

Authority: [project_goals.md](project_goals.md) (decomp / `NAMES` → manual →
fandom). Feature checklist: [manual_gap.md](manual_gap.md) §Combat.

---

## Sources

| Source | Role |
|--------|------|
| `FUN_157e_004a` / `015e` in [`viceroy_unpacked.c`](../original_sources_decompiled/viceroy_unpacked.c) (~8914–9051) | **Authoritative** base ×8 and engagement site multipliers |
| `FUN_5fef_1b0e` / `0000` / `0352` / `016c` / `16ea` / `172c` / `1908` / `31ea` | Peels, best defender, loss apply, plunder, demote/promote, treasure, native fallout |
| `FUN_636c_0000` / `FUN_2a1f_0704` | Combat Analysis dual-column UI |
| `FUN_364b_03f6` | Coastal Fort/Fortress fire |
| Port [`combat_strength.c`](../src/core/combat_strength.c), [`combat_analysis.c`](../src/core/combat_analysis.c), [`units.c`](../src/core/units.c) | Wired resolve paths; PARK comments name gaps |
| [`Colonization.pdf`](../COLONIZE/Colonization.pdf) pp.30–32 | Manual “COMBAT IN THE NEW WORLD” |
| [fandom_col1994.md](fandom_col1994.md) §Colonies / defense | Tier-3 — Unverified until reconciled |
| Annotated | [`coastal_fort_fire.md`](../original_sources_annotated/turn/coastal_fort_fire.md); [`FUNCTION_CATALOG.md`](../original_sources_annotated/FUNCTION_CATALOG.md) segments `157e` / `5fef` / `636c` |

`NAMES.TXT` `@UNIT` attack / defense bytes feed `004a`. Col1 fields used for
site / FF / difficulty live in [save_format_map.md](save_format_map.md).

---

## Entry points

| Path | Function | What happens |
|------|----------|--------------|
| Human / shared move | `units_enter_probe` → `units_try_move` | Foreign stack → `COMBAT_LAND` / `COMBAT_NAVAL` → best defender → `units_resolve_*_ff` → enter on win → colony capture |
| Land resolve | `units_resolve_land_combat` / `_ff` | `combat_land_engage` → roll → analysis → outcome |
| Naval resolve | `units_resolve_naval_combat` / `_ff` | `combat_naval_engage` → roll → analysis → naval outcome |
| Best defender | `units_best_defender_at` (`FUN_5fef_0000`) | Highest `combat_engagement_strength`; skip `attack==0`. Artillery pick-score (2026-09-03, OVL17 asm 0x80–0x11c — the Ghidra `.c` export mangles this block): on a Euro colony tile ×2 vs Indian attacker; off-colony, unless the arty itself is Fortify/Fortified, score `>>= 3` (so arty in the open no longer outranks dragoons). Unarmed fallback tier only on NON-colony tiles; on a colony tile with no armed defender the attack routes to militia/Revere, then entry seizure (`units_seize_noncombat_at`) |
| Euro AI attack | `ai_euro_try_attack` | Declare war if needed → land/naval resolve; optional `colonies_capture` |
| King / REF | `ai_king.c` | Land / naval resolve on invasion paths |
| Indian raid | `ai_contact.c` raid pulse | Adjacent `units_resolve_land_combat` → seize / move / abandon |
| Paul Revere | `units_revere_defend_colony_tile` | Empty foreign colony + muskets → auto-arm → land combat |
| Coastal fort | `turn_run_coastal_fort_fire` → `units_coastal_fort_fire_pulse` | EOT battery vs adjacent ships whose PEACE bit (0x40, the byte F8 reads) is clear, or any Privateer (raw 57082, bugs.md #465); **one** salvo per neighbour tile, at the stack's first ship (`FUN_364b_03f6` raw 57068-57076), resolved between the two dissolve phases |

Globals for resolve: `units_set_ff_col1`, `units_set_combat_colonies`,
`units_set_combat_human_nation`, `units_set_combat_popups`,
`units_set_native_fallout_context`.

---

## Strength pipeline

### Land (`combat_land_engage`)

```
atk = combat_unit_base_x8(mode=1)                    // FUN_157e_004a attack
def = combat_engagement_strength()                   // FUN_157e_015e (may stash)
atk = ((terrain_stash + 4) * atk >> 2) * 3 >> 1      // FUN_5fef_1b0e open-field
both: combat_apply_1b0e_peels()
roll: dos_rng_range(1, atk+def); attacker wins if roll <= atk
```

Special: a plain Brave (@UNIT 0x13) attacking a human-controlled European's
Artillery never wins — the roll is still drawn, then `atk_wins` is forced false
(DOS `local_ca`, raw 100573-100577). The old Scout-vs-Artillery
`force_defender_wins` was invented and is retired (smell #6).
No rng → attacker wins if `atk >= def`. `units_last_combat_outcome`: `1` / `-1` / `0`.

### Naval (`combat_naval_engage`)

```
both: combat_unit_base_x8(atk mode=1, def mode=0)
atk = atk * 3 >> 1                                   // same 1b0e attack factor
      combat_apply_1b0e_peels()   // land-only peels skipped by domain checks
```

No `015e` colony / village / terrain / fortify for ships.

**The ×3/2 attack factor applies at sea too.** `FUN_5fef_1b0e` is the single
resolver for both domains and its scale line is unconditional
(`viceroy_unpacked.c` 100457-100458):

```
iVar24 = FUN_281f_09c8(0x281f,param_1,1);          // → FUN_157e_004a mode 1
local_92 = ((*(int *)0x8d04 + 4) * iVar24 >> 2) * 3 >> 1;
```

The is-ship flags `bVar9` / `bVar10` (100347, 100451) gate later clauses, never
this one. Combat Analysis prints the matching **Attack Bonus +50%** row for
naval attackers as well — `FUN_636c_0000` walks `DS:0x8d00` bit 0
(`viceroy_unpacked.c` 101874-101891) and `FUN_157e_004a` sets exactly that bit
on every mode-1 evaluation (`*puVar1 = *puVar1 | (param_2 != 0)`, 8926-8928).
There is no domain test on either side. (The port used to apply the factor but
suppress the row at sea, so the displayed rows stopped summing to the shown
strengths — fixed 2026-09-09, smell audit #11.)

### Base ×8 (`FUN_157e_004a` / `combat_unit_base_x8`)

1. `type.defense` (mode 0) or `type.attack` (mode 1)
2. Artillery (@UNIT type 0x0b) + `col1_flags15` bit7 (damaged) → −2 —
   Artillery only (viceroy 8936-8938); ships never take this peel
3. ×8
4. Veteran Soldier/Dragoon → +50%. DOS gate (viceroy 8942-8944): @UNIT **type
   1 (Soldiers) or 4 (Dragoons)** *and* veteran profession. Promoted tiers
   (Regulars 6, Cont. Cav. 7, Cavalry 8, Cont. Army 9) are excluded — their
   @UNIT rows already carry the promoted attack/defense. Port: type matched by
   name ("Soldier"/"Dragoon"), profession `UNITS_JOB_SOLDIER` or
   `UNITS_JOB_DRAGOON` (0x15 / 0x17).
5. Drake Privateer FF → +50%
6. Ship: −`holds_occupied` (nonempty **goods** cargo slots; DOS `+0x3150`,
   viceroy 8957–8959 — passengers are not counted, so a troop transport takes
   no strength penalty)

### Defender site (`FUN_157e_015e` / `combat_engagement_strength`)

1. Base = `004a(mode=0)`
2. Site multiplier `local_1a`:
   - **A.** Euro colony on the defender's tile: `(FUN_157e_0008 + 1) * 2` →
     **2 / 4 / 6 / 8** for none / Stockade / Fort / Fortress (viceroy 9009–9011).
     The probe is `FUN_137f_0358(x, y)` = `euro_settlement_owner` (viceroy
     6793–6810) taken on `-1 < iVar6`, so the arm's only conditions are "a
     settlement stands here" and "its owner is European (< 4)". **Corrected
     2026-09-09** (smell audit #9): DOS never compares that owner to the
     defending unit's nation and never tests the unit's own nation — the port's
     `col->nation_id == u->nation_id` / `u->nation_id <= 3` guards were invented
     and disagreed with `combat_unit_on_colony`, which is a bare tile probe.
     **Open question closed 2026-09-07:** `0008`'s three `038e(0..2)` probes *are* the
     fortification chain — the building table at `DS:0x8f82` (stride `0xc`) links each record
     to its next tier at `+4`, and records 0/1/2 chain `0→1→2→-1` with name ids
     `0x4b`/`0x4c`/`0x4d`. So Fort is `6` (×2.5 = the manual's +150%), not `4`; the port used
     to collapse Fort into Stockade's `4`.
   - **B.** Native village (corrected 2026-09-03, viceroy 8989–9002 — the old `(probe+1)*2`
     formula here was the COLONY arm's): `2`; tribe `tech > 1` → `4` (8d02|0x10); capital
     dwelling (tribe rec +3 bit4) → `<<=1` (8d02|0x20)
   - **C.** Open terrain (`map_dos_terr_found_score_byte` / DS:0x2f77):
     - **Apply to defender** (`8d02|0x80`): native defender, **or** foe is Euro and
       (not WoI **or** foe is AI)
     - **Stash to attacker** (`8d04` / `8d00|0x80`): Euro defender vs **native**
       attacker, or vs **human** Euro under WoI (player attacking REF) — with two
       exceptions:
       - **human Euro attacker**: a Euro **colony** on the defender's tile, else on
         the **attacker's** tile (`FUN_137f_0358` twice — viceroy 9025–9029) → apply
         to defender instead, no stash. Corrected 2026-09-09: the port used to test
         for a *village* on either tile, which DOS never does — the village probe
         `FUN_137f_0392` already claimed the defender tile at 8988, and no village
         probe is ever run on the attacker's tile. No WoI re-test either; reaching
         this arm already implies `0x5382 & 1` (viceroy 9017)
       - **native attacker**: defender Fortified — orders 6 only, Fortify(5) does
         not deny (viceroy 9035) → neither side gets terrain
3. Fortified (orders 6 ONLY — viceroy 9045; Fortify(5) still digging in gets
   nothing), land, `local_1a < 5` → `+2`
4. Result: `((local_1a + 4) * base) >> 2`

Land attacker strength (after `004a`, before peels) is always
`((terrain_stash + 4) * atk >> 2) * 3 >> 1` (`FUN_5fef_1b0e`): the ×3/2 is the
standing attack factor; non-zero `terrain_stash` is the Indian / WoI-REF
**ambush** (terrain denied to the defender, given to the attacker). Colony /
village / absorbed-terrain paths leave stash at 0.

Colony effective multipliers vs base×8: bare ×1.5, Stockade/Fort ×2, Fortress ×3.
Fortify on bare colony: `2+2=4` (×2). Fortify on Stockade/Fort: `4+2=6` (×2.5,
because `local_1a < 5` still holds before the add). Fortress `local_1a=8` →
fortify does not stack.

**Disassembly verified clean (2026-08-13).** `FUN_5fef_1b0e` carried a
Ghidra `Removing unreachable block` disassembly-fault warning in the
canonical export (`docs/decomp_inventory.md`). Re-disassembled via the
overlay-addressing project (`tools/address_mapping.csv` →
`OVL17_L0000:1b0e`): clean, self-contained, 7270 bytes / 1116 decompiled
lines, one unrelated minor unreachable-block warning + one "type
propagation not settling" note left (ordinary decompiler noise, not the
corruption class). Calls several `thunk_FUN_1000_*` stubs whose exact
targets weren't resolved this pass (see `euro_unit_act.md`'s method note
if chasing them — verify via `rtlink_decode`'s jump table before trusting
a decompile through any of them). Confirms the extensive peel/resolve
mapping below is working from trustworthy source.

### Peels (`FUN_5fef_1b0e` / `combat_apply_1b0e_peels`)

| Peel | When | Effect |
|------|------|--------|
| Difficulty | Human Euro side | `str -= (difficulty - 4)` (Discoverer +4 … Viceroy 0) |
| Weak defender | Land×land; atk type attack (5236) > 1, def type defense (5235) < 2 | def `>>=1` (no 8d00 flag / no Analysis row; ported 2026-09-03) |
| Artillery open-field | Land, defender tile has **no settlement** (`06be` layer2&2 — Euro colony AND village tiles both count, NOT the `07be` colony lookup); arty; skip only when defender is fortified Euro (asm reads the DEFENDER's orders for BOTH clauses) | `>>=2` (−75%) — so arty attacking a village is NOT "in the open" |
| Arty vs natives on settlement | Defender arty, attacker native, settlement tile | `<<=1` |
| Spanish ambush | Attacker nation 2, defender native, settlement tile (villages) | +50% |
| WoI crown open-field | WoI, **crown** attacker, land tile (not ocean — `FUN_281f_0768 == 0`) | `+= difficulty * atk / 20` |
| WoI REF +50% | WoI, Euro attacker, **on colony**, and (attacker is **crown** **or** `ref_present`) | +50% (`0x8d01\|0x80`) |
| WoI support % | WoI, Euro attacker, **on colony** | Crown: +`(100−SoL)%` (Tories); else +`SoL%` (Rebels) |

**The three WoI rows have NO domain gate (corrected 2026-09-09).** The block's
only gate is raw 100494, verbatim `if (((*(byte *)0x5382 & 1) != 0) && (uVar16
< 4))` — WoI active and a **Euro attacker**, nothing more; the is-ship flags
`bVar9`/`bVar10` gate the weak-defender row at raw 100468, never this one. The
port carried an invented `&& land`, which cost a crown/REF **ship** both the
+50% bombardment bonus and the Tory share when it attacked a hull berthed in a
rebel colony (a berthed defender puts the fight on a colony tile, so DOS's
`iVar18 = FUN_281f_07be` is `>= 0` and the colony arm runs exactly as it does
on land). The only domain test in the block is the open-field row's own
`FUN_281f_0768(x,y) == 0`, so an engagement in open water still collects
nothing. Pinned by `tests/unit/test_combat_strength.c`
`test_woi_crown_ship_bombards_colony`.
| Human-colony damper (raw 100536-100543) — **resolve-only** | `difficulty < 2`, no WoI (or no colony on the defended tile, or attacker is a hull 0xd-0x12); defender a **human-controlled** European (`uVar15 < 4 && 0x543f[uVar15] == 0`), **colony** on the attacked tile (`-1 < iVar18`), turn `DS:0x538e < 0x50` | attacker −25% (diff 0) / `>>1` (diff 1) |
| **Discoverer beginner shield** (raw 100544-100545, ported 2026-09-08) — **resolve-only** | as the row above, plus `difficulty == 0` and the defender is the **auto-spawned** stand-in (`bVar28` — militia/Revere phantom) | attacker `= 0` → the roll `RNG(1, def+0) <= 0` always loses. A human player's undefended town cannot be taken on Discoverer in the first 80 turns |
| Any-attacker-of-human damper (raw 100546-100548) — **resolve-only** | same `difficulty < 2` / WoI gate; defender human-controlled European (**no colony needed**) and (attacker is Euro **or** turn `< 0x50`) | attacker `>>1`, stacks on the colony damper above |
| Discoverer human-attacker doubling (raw 100549) — **resolve-only** | `difficulty == 0` and the **ATTACKER** is a human-controlled European; no other gate (runs even under WoI) | attacker `<<1` |
| Brave vs human Artillery (raw 100573-100577) — **resolve-only** | plain Brave (@UNIT 0x13) attacks human-controlled Euro Artillery (@UNIT 0xb) | roll drawn, then `atk_wins` forced false (`local_ca` latch, also FUN_5fef_0f14 param_4) |

Crown nation = DS:`0x53d2` (Linux: peer of human Euro slot, same as
`ai_king_crown_nation`). WoI / `ref_present` read the real `game_options`
bits (the old `market_demand_pool_raw[0]/[1]` stand-ins were retired 2026-08-28 — that
array is DOS `market_demand_pool`, see king_ref.md).

**`bVar28` and the difficulty-handicap group (2026-09-08).** `bVar28` is
1b0e's "I built this defender myself" flag, set by **both** auto-spawn arms of
the `local_c8 < 0` branch: the empty-dwelling Brave (raw 100405-100416) and
the colony militia / Paul Revere phantom (raw 100417-100432). The port raises
the same latch on both (`combat_set_auto_defender`, called from
`units_revere_defend_colony_tile` and from the village-temp arm of
`units_try_move`), but only the colony arm can reach the shield above — the
village arm runs with `iVar18 < 0` and an Indian defender, which the gate
excludes. `local_92` is the **attacker**: it is built from `param_1`
(`FUN_281f_09c8(param_1, 1)`, the ×3/2 attack scale) and it is the value the
roll compares against — `iVar23 = FUN_281f_04d4(1, local_a8 + local_92);
bVar8 = iVar23 <= local_92`. Note the whole handicap group sits **after** the
`param_5 == 0` early return (raw 100523), so Combat Analysis odds are computed
*without* it — the preview can show a winnable fight the resolver then zeroes.

The three siblings from the same raw lines are **ported** (2026-09-08), with
the shield, into `combat_apply_1b0e_resolve_handicaps`
(`combat_strength.c`; the decomp is quoted verbatim above it). Placement is
modeled too: the resolvers (`combat_land_engage` / naval, from `units.c`)
call it **after** Combat Analysis is presented and before the roll, so the
peels function — which is also what AI scoring calls — carries none of the
group. The port's old "Discoverer damper" (−25% on a human ATTACKER vs an AI
Euro) tested the wrong side and is **deleted**: DOS dampens the attacker OF a
human defender, and at diff 0 *doubles* a human attacker.

`combat_unit_toughness` = always `015e` (AI scoring).

---

## Flags (`ColonizeCombatSideFlags`)

Mirror DOS `0x8d00` / `0x8d02` / high / `a156` for Combat Analysis.

| Flag | Word | Meaning |
|------|------|---------|
| `COMBAT_FLAG_MODE_ATK` | `flags` | `004a` attack mode; land Analysis lists Attack Bonus +50% |
| `COMBAT_FLAG_VETERAN` | `flags` | Veteran +50% |
| `COMBAT_FLAG_HOLDS` | `flags` | Cargo holds penalty |
| `COMBAT_FLAG_COLONY` | `flags` | Defending on own colony |
| `COMBAT_FLAG_STOCKADE` | `flags` | Stockade+ tier (`local_1a≥4`) |
| `COMBAT_FLAG_FORTRESS` | `flags` | Fortress doubled |
| `COMBAT_FLAG_VILLAGE` | `flags` | Village site |
| `COMBAT_FLAG_DRAKE` | `flags_hi` | Drake +50% |
| `COMBAT_FLAG_TERRAIN` | `flags` | Terrain bonus |
| `COMBAT_FLAG_ARTILLERY` | `flags` / hi | Open-field ÷4 |
| `COMBAT_FLAG_AMBUSH` | `flags` / hi | Spanish +50% |
| `COMBAT_FLAG_FORTIFY` | `flags` | Fortify +2 `local_1a` |
| `COMBAT_FLAG_REF` | `flags` / hi | Colony WoI +50% (crown or `ref_present`) |
| `COMBAT_FLAG_TORIES` | `flags2` | Crown support % = 100−SoL |
| `COMBAT_FLAG_REBELS` / `SOL` | `flags2` | Rebel support % = SoL |
| `COMBAT_FLAG_ARTY_COLONY` | `flags2` | Arty×2 vs natives |

Also stored: `base_combat`, `local_1a`, `terrain_byte`, `village_n`,
`holds_occupied`, `sol_percent`.

---

## Land vs naval vs village

| | Land | Naval | Village / natives |
|--|------|-------|-------------------|
| Engage | `combat_land_engage` | `combat_naval_engage` | Same land engage; village is **defender site** in `015e` |
| Atk strength | `004a` attack | `004a` attack | same |
| Def strength | `015e` full | `004a` defense only | Village `local_1a` + fortify |
| `1b0e` | Full land peels | Difficulty (+ Discoverer) **and attacker fatigue**; no arty/ambush/SoL | Ambush if Spanish on colony |
| Trigger | move-enter / AI adjacent | move-enter / AI / king | Fight units on tile |
| Empty village | — | `ENTER_VILLAGE_SHIP` abort | Temp Brave from dwelling (`1b0e`); fight from adjacent (no enter); pop drain |

Village **attack** empty-tile defense is **Done** thin (`units_spawn_village_temp_defender`):
DOS `FUN_5fef_1b0e` spawns a phantom Brave (Armed / Mtd. from nation muskets /
`horse_breeding>24`), fights, undoes the phantom, then `population--` or destroy
when `population < 2`. Killing a map Brave on the tile does **not** burn the
dwelling. Human `4528` `@ACTIONS` arm **Done** (P8.8). Deep `4528` mid-body / VGA still **PARKED**.

---

## Outcomes

### Resolve order (land and naval)

1. Optional Combat Analysis after strengths, **before** the roll
   (`combat_analysis_should_show` + presenter)
2. Roll / apply outcome
3. Structural outcome popups (`units_combat_outcome_popups`) — [popups.md](popups.md) §9

### Land win (`units_resolve_land_combat_ff`)

- Treasure: **captured alive, no gold** (bugs.md #660). `FUN_5fef_0352` raw 99344
  puts loser type 0x0a in the same capture set as Colonists/Wagon; raw 99392-99413
  unlink / nation-flip / re-place / orders-none, then `@LOOTCAPTURE` with
  STRING0 = loser nation, STRING1 = winner nation, NUMBER0 =
  `units_treasure_value_gold` (display only). The port's Accept/Refuse "ransom"
  CHOICE and its gold credit were inventions and are gone.
- Loser: `units_apply_land_loss_outcome` (`FUN_5fef_0352`)
  - Artillery: first loss → damage bit7; second → despawn. Gated (bugs.md #756,
    raw 99435-99436): winner not a hull AND neither tile water, else the gun is
    destroyed outright with no @ARTILLERY popup. No moves write on damage (#757).
  - **Capture** (Euro winner only, `attack>0`): Colonists / Treasure / Wagon →
    nation flip + `@COLONISTCAPTURE*` / `@LOOTCAPTURE` / `@WAGONCAPTURE` /
    `@CARGOCAPTURE`. Veteran Colonist specialty stripped → `@COLONISTCAPTURE2`.
    **Natives never capture.** Disqualifiers (raw 99381-99383, bugs.md #663):
    a losing hull (type 0xd..0x12) or either tile ocean/high seas → destroy.
  - **Type demote** (keep nation): Dragoon→Soldier, Soldier→Colonist,
    Cont.Cav→Cont.Army, Cavalry→Regulars, Cont.Army→Colonist (+ Jesuit →
    Missionary); `@DEMOTE` if human-facing
  - else despawn (Pioneers, Missionaries, Scouts, Regulars, …)
- `units_sweep_stack_after_loss` (`FUN_5fef_0ec0`) apply loss to leftover
  non-combat same-nation stackmates (skips the primary loser already resolved).
  **Gated on a defender loss** (DOS 5fef ~0x2532): runs only when the
  attacker's type attack byte is 0 or a ship (type 0xd..0x12) is party — never
  for a normal land attack. On an ATTACKER loss it runs unconditionally.
  **A berthed hull swept here is DAMAGED, not destroyed and not skipped**
  (ported 2026-09-09): 0ec0 (raw 99719-99730) carries no per-unit predicate,
  and `FUN_5fef_0352` tests the LOSER's type byte first — `if ((0xc < type) &&
  (type < 0x13))`, raw 99520 — so any hull takes the naval damage arm whatever
  domain the fight was. None of 0352's land rows could match a ship anyway
  (capture set 0/0xa/0xc at raw 99345, demote set 4/1/9/7/8 at raw 99437-99451,
  artillery row 0xb). The damage-vs-sink roll at raw 99527 is guarded by
  `winner_type*0xe + 0x523b != 0` — the @UNIT **guns** column, which is 0 for
  every land type in NAMES.TXT — so **a land winner never draws and the hull is
  always damaged** (no RNG-stream shift). It then takes the ordinary tail: holds
  and passengers lost, bit7, repair timer, relocation to the nearest own Drydock
  colony or the Europe lane, `@SHIPDAMAGE`. The port routes it to
  `units_apply_naval_loss_outcome`; the `attack == 0` rail on the land arm is
  deliberately not applied to hulls (DOS keys on the type range alone, so an
  armed Privateer moored alongside is damaged exactly like a Caravel).
  `units_seize_noncombat_at` still skips hulls: it stands in for the colony
  walk-in, and DOS's own colony-fall purge `FUN_43f7_0512` destroys hulls
  outright with `@SEIZURESEA` rather than repairing them. Pinned by
  `tests/unit/test_units_*.c (split 2026-09-23)` (`0352 hull arm damages berthed ships in a land
  sweep`).
- Winner: Washington always-promote; else chance promote (`FUN_5fef_172c`)
- Native def: settlement fallout (`FUN_5fef_31ea`) + `@LOOT` (treasure, DOS tag
  `0x1ccc`) / `@LOOT2` (burn, no treasure, `0x1cd1`). The treasure peel is **not
  Cortes-gated** (bugs.md #381): everyone rolls it. Cortes (`local -6`) is one of
  the three "or" terms (`roll == 0 || rich || cortes`) that let the tech
  0/1 roll pay out at all, plus a bonus — +50% at tech 0/1, +6 units at
  tech 2, +10 at tech 3. The band is the razed tribe's Indian record `tech`
  (`*(0x8d4e)+2`, NAMES.TXT @TRIBES column 4), **not** the game difficulty
  (bugs.md #549). **At tech 2 and 3 the amount is unconditional for every
  conqueror.** Spanish (`local -0xa8`) shorten the tech-0 roll (`rng(0,3)` vs
  `rng(0,6)`) and add +3/+5 units at 2/3; a capital (`local -0xcc`,
  `tribe.state.capital`) always pays out and doubles / widens the amount.
  `%STRING1 %STRING2` = tribe name + NAMES.TXT @LEVELS column 1 by tech (row 4
  "Capital" for a capital).
  widens the amount. Gold = amount × 100.
- Razing order (`FUN_5fef_1b0e` dwelling arm): `FUN_4d56_00e0` destroy →
  mission return → convert-join → treasure. `00e0` clears only settlement bit
  `0x02` on the tile (`FUN_281f_068c(x,y,2,0)`, raw 81307): a real road `0x08`
  stays, the village's implied road art goes (bugs.md #550). It deletes every
  Indian unit whose home village `+0x314a` is the razed one, wherever it
  stands; braves of the tribe's other villages stay (#552). If the mission
  byte's low nibble is the conqueror, a Missionary (`@UNIT` 3) of the
  conqueror spawns on the site, Jesuit bit `0x10` → profession `0x18`
  (raw 100667-100672); a rival's mission is lost (#551).

### Land loss

Same loss apply on attacker; defender may promote.

### Demote (`FUN_5fef_0352` type table; `FUN_5fef_16ea` promote-path specialty)

Combat loss remaps **unit type** (not merely profession). Cite:
`viceroy_unpacked.c` `FUN_5fef_0352` demote arm.

### Naval (`units_apply_naval_loss_outcome`) — DOS `FUN_5fef_0352` model (2026-09-02)

- **Evasion first** (`FUN_5bfb_312e`, DOS 1b0e tail; preempts the rolled
  outcome): defender with type attack BELOW the attacker's escapes on
  `roll(1, atk_pow+def_pow) <= def_pow`, power = movement+3, Privateer ×2,
  Galleon +3, −4 per occupied **goods** hold, min 1 → `@EVASIVE`, no outcome.
  (`FUN_5bfb_312e` viceroy 98448 reads unit `+0x3150` raw, and that byte counts
  goods holds only — passengers never bump it; corrected 2026-09-09.)
- Both ships → `units_plunder_ship_holds` (`FUN_5fef_016c`) runs BEFORE the
  damage/sink split; the loser's holds are then zeroed either way (goods not
  lifted vanish, passengers are lost — `units_ship_lose_holds`).
- **The roll is then overridden by `units_naval_damage_gate`** (DOS
  `FUN_5fef_0352` raw 99527-99570, bugs.md #867): an unarmed hull goes through
  the AI fleet-pool keep/lose bias (`stuff.ship_cargo_totals` less the frigate
  and privateer pools, versus `clamp(census_pop_proxy>>2, 3, 6)`; a Caravel
  past turn 0x4f always sinks); an armed hull outside the WoI has the colony /
  armed-ship / Frigate-vs-Frigate clauses; and during the WoI the crown's LAST
  Man-O-War can never be sunk. Both port call sites apply it.
- **Damage vs sink is a roll on @UNIT guns/hull** (DS `0x523b`/`0x523c`):
  `roll(1, winner.guns + loser.hull) <= loser.hull` → survives damaged
  (bit7, `@SHIPDAMAGE`, teleport to the nearest own Drydock/Shipyard colony);
  otherwise despawn (`@SHIPSUNK`). A gunless victor (guns 0: all transports)
  can only drive off damaged, never sink. No "already damaged" special: a
  re-loss re-rolls, as in DOS. Both call sites (naval loss and coastal-fort
  fire, where the fort's strength stands in for "guns") go through the single
  `units_ship_damage_vs_sink` helper; its `rng == NULL` branch is port-only and
  damages ties, matching the `atk_wins = attack_str >= defense` convention the
  same file uses for the other DOS `roll(1, X+Y) <= X` decision. `guns == hull`
  is a genuine coin flip in DOS, so no fixture should sit on it (smell audit
  #12: the two copies used to break the tie in opposite directions).
- **No repair port → Europe, on the spot** (bugs.md #462/#463): DOS's colony
  scan tests feature bit 7 only, and when it finds nothing substitutes the
  nation's home-port name (`DS -0x7c74`, crown slot borrowing the human's)
  into `%STRING2`. BOTH arms then unlink the hull from its tile
  (`FUN_281f_0812`) and re-place it (`FUN_281f_0844`) — the decompile drops
  those register args at raw 99643-99644, the asm at `5fef:0ceb-5fef:0cf9`
  loads them from `local_28`/`local_2c`: the winning colony's x/y
  (`0x5d46`/`0x5d47`) when the scan found a port, else `loser_nation - 0x14`
  in **both** coordinates, DOS's off-map Europe slot (`5fef:0bc0-5fef:0bcb`).
  So a damaged ship is in Europe the instant it loses, on the repair timer;
  it never lingers on its tile (which also left the sprite standing under the
  combat dissolve). `units_ship_enter_repair` does the teleport for both call
  sites; the port's equivalent of the off-map slot is the Europe Expected lane
  with the repair timer as the wait (`units_set_combat_europe` supplies the
  screen; without it — headless — the hull stays put and
  `turn_route_damaged_ships` remains the fallback). WoI human keeps the DOS
  `goto`-to-sunk (no friendly Europe).
- **Repair timer** (`0x5235` column): `col1_counter16` preset to
  `max(0, loser.combat - winner.combat)`, the winner's value doubled when it
  is not a ship (fort fire), Frigate floor 4 / Man-O-War floor 8; the EOT ship
  tick counts +1, +2 on a colony tile. The damage turn itself does not count
  (`repair_pending == 2`), so a damaged ship is always out at least one turn.
- **Stack sweep** (`units_sweep_naval_stack_after_loss`, DOS `FUN_5fef_0ec0`):
  every stackmate on the loser's tile takes its own 0352 — each ship its own
  plunder + damage-vs-sink roll; land stackmates are destroyed (a ship winner
  can neither capture nor demote in 0352). Runs on BOTH the defender's tile
  (win) and the attacker's tile (loss). Colony tiles are exempt (DOS naval
  fights never occur there; a port garrison must not drown). With the whole
  defender stack removed/rerouted, the attacker's move-through completes.
- Privateer winner + human → `@SEIZURESEA` — only when the transport is
  actually taken (sunk), not when it escapes damaged.
- Unported DOS residue: AI fleet-pool keep/lose biases, crown-MoW forced-
  damage exemption, late-game (turn>0x4f) forced Caravel sink, the
  and the per-nation stored-port *coords* (`DS -0x77c6` = the Europe landfall
  tile, written to the loser's goto fields +0x314d/e so DOS's repaired hull
  knows where to come back to — the Linux Europe lane carries its own exit
  tile instead).

### Artillery: two different settlement gates — 2026-09-04

The two artillery rules read the tile through **different** accessors, and
mixing them up is what row 337 / row 353 of bugs.md kept circling:

| Rule | Where | Gate | Village counts? |
|---|---|---|---|
| Strength: open-field `>>2` | `FUN_5fef_1b0e` (`combat_apply_1b0e_peels`) | `FUN_281f_06be` → `FUN_137f_03e4` — layer2 bit 2 settlement owner | **yes** (no penalty at a village) |
| Strength: defender `<<1` vs a native attacker | same | same | yes, but only a colony can hold a gun |
| Pick score: `<<1` / `>>3` | `FUN_5fef_0000` (`units_best_defender_at`) | `FUN_1000_8886` → `FUN_281f_0696` — **Euro** settlement owner | **no** (a village is open ground) |

So an artillery *attacking* a native settlement gets no bonus — only the
absence of the open-field penalty. There is no attacker-side artillery
multiplier anywhere in 1b0e. The `>>3` pick-score demotion and the colony gate
on the `<<1` were described in the Best-defender row above on 2026-09-03 but
only reached the code on 2026-09-04.

### Attacker fatigue (`@HALF`) — 2026-09-04

`FUN_5fef_1b0e` (viceroy_unpacked.c ~100340) reads `rem = max_mp −
moves_spent` **before** charging the attack's 3 thirds. When `rem < 3`:

- a human-controlled European attacker is asked `@HALF` (id `0x1c06`) —
  "Charge!" proceeds, "Then let them rest." aborts (the 3 thirds are already
  charged, so declining ends the unit's turn). AI and native attackers are
  never asked.
- the fatigue bits are set — 2 thirds → `0x8d01` bit0, 1 third → `a156` bit3
  (`COMBAT_FLAG_FATIGUE_33` / `_66`, two Combat Analysis rows).
- the attacker's strength is scaled `atk = atk * rem / 3`, immediately after
  the difficulty peel and before the weak-defender / artillery / ambush
  clauses (`combat_apply_1b0e_peels`).

**Fatigue has no domain gate — it applies at sea too** (smell audit
2026-09-09 #11; the "Land vs naval" table used to imply otherwise). DOS:

```
if (local_98 != 0) { local_92 = (int)(local_92 * local_98) / 3; }   /* 100459 */
```

`local_98` is set from `rem` at 100388-100393 with no is-ship test, and the
is-ship flags `bVar9` / `bVar10` (100347, 100451) gate only the later
artillery clauses. `combat_naval_engage` calls the same
`combat_apply_1b0e_peels`, so the port already matches; nothing to gate.

Linux: `ai_contact_try_tired_attack_confirm` + `AI_POPUP_TAG_COMBAT_HALF`
(the last of `game_try_unit_move`'s pre-move confirms, matching 1b0e's own
order), `units_remaining_mp` (native units keep DOS's spent byte in
`moves`; Europeans keep the remainder).

**Unported residue:** DOS also calls `unit_exhaust_mp` on the attacker after
the prompt, so a DOS attack always ends the unit's turn; the port still drains
step cost + 3, which can leave a fast unit enough for a second attack.

### Colony capture

After successful enter: `units_try_capture_foreign_colony` → `colonies_capture_ex` +
`units_combat_notify_colony_captured` (`@CAPTURED` / `2` / `3`). Euro→Euro: nation
swap **plus the `FUN_5fef_1b0e` capture tail (ported 2026-08-28,
`colonies_capture_col1_effects`)**: rebel dividend ×2/3, colony/pop tallies
move, peacetime treasury share `gold×pop/(pop+Σ loser pop)` to the captor (=
`@CAPTURED %NUMBER0`; no plunder under WoI), `nation_relation` words zeroed,
WAR set, crown capture under WoI sets `0x5382|0x40`. No building/fort damage in
DOS. Indian capturer: `colonies_abandon` + `@BURNED` on raid burn paths. Also AI
euro / king REF / raid paths (all through `colonies_capture`, same effects when
`colonies_set_col1_context` has a save).

**Ships berthed in a fallen port — refutation, 2026-09-08.** The long-standing
PARK ("does the euro seize arm sink foreign warships in a fallen port?")
resolves to **nothing happens to them**. `FUN_5fef_1b0e` is the only resolver
on that path and its capture arm (raw `viceroy_unpacked.c:100905-101034`)
never touches the unit array except for the WoI crown neighbour re-home
(`100949-100963`). The hull is invisible to the assault at every earlier
stage too: `FUN_5fef_0000`'s domain gate (raw `99190-99196`) skips any
candidate whose ship-ness (type `0x0d..0x12`) differs from the attacker's
tile water test, so it never defends, and DOS has no "tile clear of
foreigners" test to fail. The town changes hands with the loser's ship still
sitting in it, under its old flag. Nothing is sunk and nothing is seized —
the `0512` destroy texture belongs to `FUN_43f7_0512` (the crown REF landing
seizure, `ai_king.c`), not here.

The port could not express that: `units_foreign_unit_at` counted the hull, and an
armed hull (attack > 0) also survives `units_seize_noncombat_at`, so a berthed
Frigate held the tile "contested" forever and no land force could take the
port. Both post-win gates now use `units_domain_blocker_at` (`units.c`), which
applies `FUN_5fef_0000`'s own domain rule — the walk-in gate in
`units_try_move` and the contested test in `units_try_capture_foreign_colony`.
`ai_euro.c`'s adjacent-walk-in arm carries a Linux-only "sink every foreign
hull in the port" loop written against the old PARK; it is now unnecessary
and DOS-contradicting (owner's call to remove).

**Capture-arm audit, 2026-09-08** (raw = `viceroy_unpacked.c`). Everything the
`bVar12` arm mutates is accounted for: colony/pop tallies, owner byte, rebel
dividend ×2/3, treasury share, `nation_relation` zeroing, WAR bit and the WoI
`0x5382|0x40` are all in `colonies_capture_col1_effects`; `@HOWTOWIN`
(`0x5386` bit 0) and the crown neighbour re-home are in
`units_try_capture_foreign_colony`. The two residue rows **closed
2026-09-08**, both in that same function:

| Raw | DOS | Port |
|-----|-----|------|
| `100937-100948` | 8-neighbour loop over the DS:0xb4/0xbe dir8 tables: `if (FUN_281f_06d2(x,y) < 0) FUN_281f_0704(x,y,new_owner)`. `06d2` = `FUN_137f_0428` = `03e4` (layer2 settlement bit `0x02` → its owner nibble) falling back to `0314` (layer2 unit bit `0x01`), i.e. "nothing stands here"; `0704` = `FUN_137f_0228`, the owner-nibble stamp. Not a fog reveal — the ring of **territory** the prize brings with it | `units_capture_claim_ring`. Same idiom `ai.c`'s `ai_indian_midpass_claim_worked_tiles` already carries. Not gated on DOS's `param_4`, so AI captures stamp it too. The centre tile's own stamp (raw `100901`) is not repeated: the captor's step onto the colony square already ran it through `units_occupancy_refresh_tile` → `units_claim_tile_owner_from_stack`. `0228`'s `@SEIZURE` arm can't fire (it needs a native settlement, which the `06d2` gate excluded) |
| `101032-101034` | `FUN_281f_0608(colony)` (far thunk → `FUN_2f2b_6cd4`, colony screen) fires when the captor is a **human-controlled European** — DOS drops you straight into the town you just took, *after* the blocking `@CAPTURED` dialog at `101030` | `units_combat_pump_popups()` (drains `@CAPTURED` / `@HOWTOWIN` first, DOS's order) then `ai_popup_colony_zoom_elect`. No `game_loop.c` edit was needed: the elected zoom is drained by the existing `ai_popup_take_colony_zoom` → `game_enter_colony` pair, which is how DOS's **other** `FUN_281f_0608` caller (`FUN_364b_0688`'s colony-event tail) is already wired. Headless callers install neither pump nor popup state, so both calls are inert |

**Brave vs human Artillery auto-loss — ported 2026-09-08.** Raw
`100573-100577`, immediately after the roll: when the attacker is a native
(`3 < uVar16`), the defender a **human-controlled** European
(`uVar15 < 4 && *(char *)(uVar15 * 0x34 + 0x543f) == 0`), the attacker type
`0x13` (a plain Brave — **not** Armed 0x14 / Mtd. 0x15 / Mtd. Warriors 0x16)
and the defender type `0xb` (Artillery), DOS forces `bVar8 = false` — the
Brave **always** loses, whatever the roll said — and sets `local_ca = 1`,
which is the flag it then hands `FUN_5fef_0f14` to bypass that resolver's
walls check. Port: `units_combat_brave_vs_human_arty` (`units.c`), evaluated
in `units_resolve_land_combat_ff` right after the roll block, so the RNG draw
still happens and `eng.roll` keeps the value DOS's `iVar23` keeps; only
`eng.atk_wins` is cleared. The predicate is DOS's *only* write of `local_ca`
(`100336` zeroes it, `100576` sets it, `101142` reads it), so the same local
is reused as the raid handoff's `forced`/`param_4` argument to
`ai_contact_colony_raid_repelled` — that argument used to be re-derived from
unit *names*, which also matched Armed/Mtd. Braves and so was wider than DOS.
Type ids are `type_index` = NAMES.TXT `@UNIT` line order (Artillery 11,
Braves 19), the identity the typed attack-fire sound already relies on. This
is on top of, not instead of, the Artillery ×2-vs-natives strength term
(`combat_strength.c`, `COMBAT_FLAG_ARTY_COLONY`), which only made the pairing
*usually* fatal. Test: `unit_brave_vs_human_artillery_autoloss`
(`tests/unit/test_units_*.c (split 2026-09-23)`) — 40 seeds never win against a human European's
Artillery, while the same seeds against an **AI** European (control != 0), and
Armed Braves against the human one, still win sometimes.

---


---

## Combat Analysis and Outcomes Detail

Deep combat mechanics including analysis dialog, fatigue, capture logic, and related systems: **See [combat_analysis.md](combat_analysis.md)**
