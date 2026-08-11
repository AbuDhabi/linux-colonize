# Combat mechanics

Player-visible combat resolution for Sid Meier's Colonization (1994): land and
naval odds, fortification / terrain / peel modifiers, Combat Analysis, promote /
demote / capture / loot, and coastal Fort/Fortress batteries.

Move-into-tile fight-vs-bounce gates stay in [move_enter.md](move_enter.md).
Village raid body `4528` and deep Euro AI scoring `20e6` stay Indian / AI docs.

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
| Best defender | `units_best_defender_at` (`FUN_5fef_0000`) | Highest `combat_engagement_strength`; arty×2 vs Indian atk; skip `attack==0` |
| Euro AI attack | `ai_euro_try_attack` | Declare war if needed → land/naval resolve; optional `colonies_capture` |
| King / REF | `ai_king.c` | Land / naval resolve on invasion paths |
| Indian raid | `ai_contact.c` raid pulse | Adjacent `units_resolve_land_combat` → seize / move / abandon |
| Paul Revere | `units_revere_defend_colony_tile` | Empty foreign colony + muskets → auto-arm → land combat |
| Coastal fort | `turn_run_coastal_fort_fire` → `units_coastal_fort_fire_pulse` | EOT battery vs adjacent hostile ships |

Globals for resolve: `units_set_ff_col1`, `units_set_combat_colonies`,
`units_set_combat_human_nation`, `units_set_combat_popups`,
`units_set_native_fallout_context`.

---

## Strength pipeline

### Land (`combat_land_engage`)

```
attacker: combat_unit_base_x8(mode=1)      // FUN_157e_004a attack
defender: combat_engagement_strength()     // FUN_157e_015e (004a defense + site)
both:     combat_apply_1b0e_peels()        // FUN_5fef_1b0e
roll:     dos_rng_range(1, atk+def); attacker wins if roll <= atk
```

Special: `force_defender_wins` if Scout attacks Artillery (no roll favor to atk).
No rng → attacker wins if `atk >= def`. `units_last_combat_outcome`: `1` / `-1` / `0`.

### Naval (`combat_naval_engage`)

```
both: combat_unit_base_x8(atk mode=1, def mode=0)
      combat_apply_1b0e_peels()   // land-only peels skipped by domain checks
```

No `015e` colony / village / terrain / fortify for ships.

### Base ×8 (`FUN_157e_004a` / `combat_unit_base_x8`)

1. `type.defense` (mode 0) or `type.attack` (mode 1)
2. Privateer + `col1_unknown15` bit7 (damaged) → −2
3. ×8
4. Veteran Soldier/Dragoon (`profession == UNITS_JOB_SOLDIER`) → +50%
5. Drake Privateer FF → +50%
6. Ship: −`holds_occupied` (nonempty cargo slots)

### Defender site (`FUN_157e_015e` / `combat_engagement_strength`)

1. Base = `004a(mode=0)`
2. Site multiplier `local_1a`:
   - **A.** Own Euro colony: bare `2`; Stockade **or** Fort **or** Fortress `4`; Fortress then `<<=1` → `8`
   - **B.** Native village: `(village_probe_n + 1) * 2` (tech probes 1/2/3)
   - **C.** Open terrain: `map_dos_terr_found_score_byte` under DOS-shaped gates (AI/native/WoI/village/fortify stash)
3. Fortify / Fortified, land, `local_1a < 5` → `+2`
4. Result: `((local_1a + 4) * base) >> 2`

Colony effective multipliers vs base×8: bare ×1.5, Stockade/Fort ×2, Fortress ×3.
Fortify on bare colony: `2+2=4` (×2). Fortify on Stockade/Fort: `4+2=6` (×2.5,
because `local_1a < 5` still holds before the add). Fortress `local_1a=8` →
fortify does not stack.

### Peels (`FUN_5fef_1b0e` / `combat_apply_1b0e_peels`)

| Peel | When | Effect |
|------|------|--------|
| Difficulty | Human Euro side | `str -= (difficulty - 4)` (Discoverer +4 … Viceroy 0) |
| Artillery open-field | Land, not on colony; arty and (not fortified **or** foe native) | `>>=2` (−75%) |
| Arty vs natives on colony | Defender arty, attacker native | `<<=1` |
| Spanish ambush | Attacker nation 2, defender native, on colony | +50% |
| WoI REF | `ref_present`, land, Euro attacker | +50% |
| WoI SoL | Land, Euro attacker | +`sol%` of strength (**thin**: always SoL, not Tory-share branch) |
| Discoverer damper | diff==0, human atk vs AI Euro | −25% |
| Scout vs Artillery | Land | `force_defender_wins` |

`combat_unit_toughness` = always `015e` (AI scoring).

---

## Flags (`ColonizeCombatSideFlags`)

Mirror DOS `0x8d00` / `0x8d02` / high / `a156` for Combat Analysis.

| Flag | Word | Meaning |
|------|------|---------|
| `COMBAT_FLAG_MODE_ATK` | `flags` | `004a` attack mode |
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
| `COMBAT_FLAG_REF` | `flags` / hi | REF +50% |
| `COMBAT_FLAG_SOL` | `flags2` | SoL % applied |
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
| `1b0e` | Full land peels | Difficulty (+ Discoverer); no arty/ambush/SoL | Ambush if Spanish on colony |
| Trigger | move-enter / AI adjacent | move-enter / AI / king | Fight units on tile |
| Empty village | — | `ENTER_VILLAGE_SHIP` abort | Meet / raid (`4528`) — **not** this engage |

Village **attack** as a distinct settlement battle body is **PARKED**; Linux
fights whoever stands on the tile via normal land combat.

---

## Outcomes

### After roll (land and naval)

1. Optional Combat Analysis (`combat_analysis_should_show` + presenter)
2. Structural outcome popups (`units_combat_outcome_popups`) — [popups.md](popups.md) §9

### Land win (`units_resolve_land_combat_ff`)

- Treasure: LE16 hold gold → treasury (`FUN_5fef_1908`); `@LOOTCAPTURE` — ransom CHOICE **PARKED**
- Loser: `units_apply_land_loss_outcome` (`FUN_5fef_0352`)
  - Artillery: first loss → damage bit7; second → despawn
  - Euro non-combat (`attack==0`, not Treasure/Wagon) → nation flip + demote
  - else despawn
- `units_sweep_stack_after_loss` (`FUN_5fef_0ec0`) capture leftover non-combat
- Winner: Washington always-promote; else chance promote (`FUN_5fef_172c`)
- Native def: settlement fallout (`FUN_5fef_31ea`) when fallout context set

### Land loss

Same loss apply on attacker; defender may promote.

### Demote (`FUN_5fef_16ea`)

Profession remap / strip Soldier→Free Colonist under WoI; `@DEMOTE` if
human-facing.

### Naval (`units_apply_naval_loss_outcome`)

- Close fight (`loser_str * 2 > winner_str`) + weaker type attack + undamaged →
  damage bit7 + escape (`@SHIPDAMAGE`)
- else `units_plunder_ship_holds` (`FUN_5fef_016c`) then despawn (`@SHIPSUNK`)

### Colony capture

After successful enter: `units_try_capture_foreign_colony` → `colonies_capture`.
Euro→Euro: nation swap. Indian capturer: `colonies_abandon`. Also AI euro /
king REF / raid paths.

---

## Combat Analysis

[`combat_analysis.c`](../src/core/combat_analysis.c) — `FUN_636c_0000`-shaped
dual column.

- Gate: `game_options.combat_analysis` + human side (`FUN_5fef_1b0e` gate
  `0x5383&2`)
- Lines from flags: Veteran, Drake, Cargo, Terrain, Village, Colony/Stockade/
  Fortress, Fortified, Artillery, Ambush, REF, SoL + Strength + Roll +
  Victory/Defeat
- Input: Esc / Enter / Space / click dismiss
- Presenter hook: tests / AI skip when unset (`combat_analysis_set_presenter`)

UI labels use manual-shaped percents (e.g. “Fortress +200%”, “Fortified +50%”)
even when the live multiplier comes from `local_1a` arithmetic above.

---

## Coastal fort fire

| Piece | API |
|-------|-----|
| Strength | `units_coastal_fort_attack_strength` = `4 * tier * (1 + arty)`; Fort tier1, Fortress tier2 |
| Pulse | `units_coastal_fort_fire_pulse` — all Fort/Fortress colonies, 8 ocean dirs |
| Hostile | at war (Euro/Indian) **or** Privateer |
| Resolve | `units_fort_vs_ship`: fort atk vs ship defense (Drake on Privateer); win→sink (no plunder); lose→`moves_left=0` |
| Turn | `turn_run_coastal_fort_fire` after colony production |
| AI | `ai_euro_tile_under_enemy_fort_fire` / flee |

Deep DOS notes: [`coastal_fort_fire.md`](../original_sources_annotated/turn/coastal_fort_fire.md).

**PARKED:** damaged bit7 / deep ship-slow formula (bit7 shared with ship-build);
DOS temp-attacker + chrome.

---

## Founding Father hooks

| FF | Combat effect | Where |
|----|---------------|-------|
| George Washington | Non-veteran Soldier/Dragoon who wins always promotes | `units_resolve_land_combat_ff` |
| Francis Drake | Privateer combat +50% | `combat_unit_base_x8` |
| Paul Revere | Auto-arm empty colony under attack | `units_revere_defend_colony_tile` |
| Cortes / Sepulveda | Conquest treasure / convert-join on native fallout | `units_try_native_settlement_fallout` |

---

## Status matrix

| Piece | Status | Notes |
|-------|--------|-------|
| Land / naval engage + roll | Done | `combat_*_engage` + resolve |
| Best defender | Done | `units_best_defender_at` |
| Colony / village / terrain / fortify site | Done | `015e` |
| `1b0e` peels | Done | SoL Tory-share branch thin |
| Promote / demote / capture / treasure | Partial | Structural; ransom CHOICE PARKED |
| Naval damage / sink / plunder | Done | Close-fight escape path Done |
| Combat Analysis | Done | Options-gated dual column |
| Coastal fort fire | Done | Ship-slow thin; bit7 PARKED |
| Outcome popups `@EUROPE*` / `@SHIP*` / `@LOOT*` | Partial | Structural; full `@LOOT*` / burn matrix Missing — [popups.md](popups.md) |
| Village settlement battle `4528` | PARKED | Enter / Indian docs |
| Euro mid combat scoring `20e6` | PARKED / OPEN | [ai_transcription.md](ai_transcription.md) |
| VGA-identical combat chrome | PARKED | — |

---

## Conflicts resolved

| Topic | Rejected / stale | Authoritative |
|-------|------------------|---------------|
| Fort land defense | Manual / wiki Fort **+150%** as a distinct `015e` tier | Decomp: Fort shares Stockade `local_1a=4` (×2). Wiki +150% ≈ fortified Stockade path (`local_1a=6` → ×2.5). Fortress `local_1a=8` (×3) |
| `colonies_fortification_defense_bonus_percent` | Live land combat | Helper returns 100/150/200 for AI/UI; **live land combat uses `combat_colony_local_1a`** |
| Fandom “Port: combat Missing” | Stale Units row | Land/naval Partial — this hub + [manual_gap.md](manual_gap.md) |
| Difficulty “combat unaffected” | Old [difficulty.md](difficulty.md) note | Human Euro `str -= (difficulty-4)` + Discoverer −25% in `1b0e` |
| SoL popular support | Manual SoL/Tory share by side | Port thin: always adds SoL% ([sons_of_liberty.md](sons_of_liberty.md)) |

---

## PARKED

| Gap | Where |
|-----|-------|
| Village raid / settlement `4528` / `2820` + VGA | `ai_contact.c`, [move_enter.md](move_enter.md), [indians.md](indians.md) |
| Deep Euro combat scoring `20e6` | `ai_euro.c` |
| Treasure ransom CHOICE | `units_resolve_land_combat_ff` |
| Full `@LOOT*` / capture / burn chrome matrix | [popups.md](popups.md) |
| Coastal fort bit7 damage / repair | `units_fort_vs_ship` |
| SoL Tory-share branch | `combat_apply_1b0e_peels` |
| VGA-identical combat chrome | — |

---

## Implementation map

| Concern | Module |
|---------|--------|
| Strength / peels / engage | [`combat_strength.c`](../src/core/combat_strength.c) |
| Combat Analysis UI | [`combat_analysis.c`](../src/core/combat_analysis.c) |
| Resolve / promote / demote / plunder / fort fire | [`units.c`](../src/core/units.c) |
| Enter trigger | [`units.c`](../src/core/units.c) `units_try_move` — [move_enter.md](move_enter.md) |
| Euro / king / raid callers | [`ai_euro.c`](../src/core/ai_euro.c), [`ai_king.c`](../src/core/ai_king.c), [`ai_contact.c`](../src/core/ai_contact.c) |
| EOT fort pulse | [`turn.c`](../src/core/turn.c) `turn_run_coastal_fort_fire` |

### Tests

| File | Coverage |
|------|----------|
| `tests/smoke/test_units.c` | move-enter combat; naval; fort fire; analysis gate; land engage/colony; best defender; capture/loot; popups |
| `tests/smoke/test_founding_fathers.c` | Washington promote; Drake naval; Revere; fallout |
| `tests/smoke/test_ai_euro_war.c` | naval/land hunt; adjacent combat chain; Stockade note |
| `tests/smoke/test_ai_king.c` | Cont. promote (king path) |
| `tests/smoke/test_ai_contact.c` | raid resolve fallout |

---

## FUN_* index

| DOS | Linux |
|-----|-------|
| `FUN_157e_004a` | `combat_unit_base_x8` |
| `FUN_157e_015e` | `combat_engagement_strength` |
| `FUN_157e_0008` / `15eb_038e` | village probe count |
| `FUN_5fef_1b0e` | `combat_apply_1b0e_peels` + resolve roll shell |
| `FUN_5fef_0000` | `units_best_defender_at` |
| `FUN_5fef_0352` | `units_apply_land_loss_outcome` |
| `FUN_5fef_0ec0` | `units_sweep_stack_after_loss` |
| `FUN_5fef_016c` | `units_plunder_ship_holds` |
| `FUN_5fef_16ea` / `172c` | demote / chance promote |
| `FUN_5fef_1908` | treasure loot gold |
| `FUN_5fef_31ea` | native settlement fallout |
| `FUN_636c_0000` / `2a1f_0704` | Combat Analysis |
| `FUN_364b_03f6` | coastal fort fire |
| `FUN_465b_*` / `FUN_4720_*` | move-enter / combat trigger |
| `FUN_4d56_4528` / `5fef_0f14` | village/raid (thin; body PARKED) |

---

## See also

- [move_enter.md](move_enter.md) — enter outcomes `COMBAT_LAND` / `COMBAT_NAVAL`
- [unit_orders.md](unit_orders.md) — Fortify / Fortified
- [building_production.md](building_production.md) — Stockade / Fort / Fortress rows
- [sons_of_liberty.md](sons_of_liberty.md) — WoI popular-support peel
- [difficulty.md](difficulty.md) — difficulty peel values
- [indians.md](indians.md) — raid / fallout ownership
- [popups.md](popups.md) — combat / loot `@SECTION`s
- [manual_gap.md](manual_gap.md) — feature checklist
- [ai_transcription.md](ai_transcription.md) — AI combat callers / `20e6`
