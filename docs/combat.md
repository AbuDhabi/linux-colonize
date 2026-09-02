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
| Best defender | `units_best_defender_at` (`FUN_5fef_0000`) | Highest `combat_engagement_strength`; arty×2 vs Indian atk; skip `attack==0`. Unarmed fallback tier only on NON-colony tiles; on a colony tile with no armed defender the attack routes to militia/Revere, then entry seizure (`units_seize_noncombat_at`) |
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
atk = combat_unit_base_x8(mode=1)                    // FUN_157e_004a attack
def = combat_engagement_strength()                   // FUN_157e_015e (may stash)
atk = ((terrain_stash + 4) * atk >> 2) * 3 >> 1      // FUN_5fef_1b0e open-field
both: combat_apply_1b0e_peels()
roll: dos_rng_range(1, atk+def); attacker wins if roll <= atk
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
   - **C.** Open terrain (`map_dos_terr_found_score_byte` / DS:0x2f77):
     - **Apply to defender** (`8d02|0x80`): native defender, **or** foe is Euro and
       (not WoI **or** foe is AI)
     - **Stash to attacker** (`8d04` / `8d00|0x80`): Euro defender vs **native**
       attacker, or vs **human** Euro under WoI (player attacking REF) — unless
       either tile is a village (then apply to defender) or defender is Fortified
       (then neither side gets terrain)
3. Fortify / Fortified, land, `local_1a < 5` → `+2`
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
| Artillery open-field | Land, not on colony; arty and (not fortified **or** foe native) | `>>=2` (−75%) |
| Arty vs natives on colony | Defender arty, attacker native | `<<=1` |
| Spanish ambush | Attacker nation 2, defender native, on colony | +50% |
| WoI crown open-field | WoI, **crown** attacker, land tile (not ocean) | `+= difficulty * atk / 20` |
| WoI REF +50% | WoI, Euro attacker, **on colony**, and (attacker is **crown** **or** `ref_present`) | +50% (`0x8d01\|0x80`) |
| WoI support % | WoI, Euro attacker, **on colony** | Crown: +`(100−SoL)%` (Tories); else +`SoL%` (Rebels) |
| Discoverer damper | diff==0, human atk vs AI Euro | −25% |
| Scout vs Artillery | Land | `force_defender_wins` |

Crown nation = DS:`0x53d2` (Linux: peer of human Euro slot, same as
`ai_king_crown_nation`). WoI / `ref_present` read the real `game_options`
bits (the old `unknown46[0]/[1]` stand-ins were retired 2026-08-28 — that
array is DOS `price_group_state`, see king_ref.md).

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
| `1b0e` | Full land peels | Difficulty (+ Discoverer); no arty/ambush/SoL | Ambush if Spanish on colony |
| Trigger | move-enter / AI adjacent | move-enter / AI / king | Fight units on tile |
| Empty village | — | `ENTER_VILLAGE_SHIP` abort | Temp Brave from dwelling (`1b0e`); fight from adjacent (no enter); pop drain |

Village **attack** empty-tile defense is **Done** thin (`units_spawn_village_temp_defender`):
DOS `FUN_5fef_1b0e` spawns a phantom Brave (Armed / Mtd. from nation muskets /
`horse_breeding>24`), fights, undoes the phantom, then `population--` or destroy
when `population < 2`. Killing a map Brave on the tile does **not** burn the
dwelling. Deep `4528` mid-body / VGA still **PARKED**.

---

## Outcomes

### Resolve order (land and naval)

1. Optional Combat Analysis after strengths, **before** the roll
   (`combat_analysis_should_show` + presenter)
2. Roll / apply outcome
3. Structural outcome popups (`units_combat_outcome_popups`) — [popups.md](popups.md) §9

### Land win (`units_resolve_land_combat_ff`)

- Treasure: LE16 hold gold; human → Accept/Refuse ransom CHOICE (DOS source unidentified — `FUN_5fef_1908` was miscited here; it is the King's Galleon offer, see `euro_unit_act.md` 2026-08-27)
  (`AI_POPUP_TAG_COMBAT_RANSOM`) before credit; AI → silent full credit + `@LOOTCAPTURE`
- Loser: `units_apply_land_loss_outcome` (`FUN_5fef_0352`)
  - Artillery: first loss → damage bit7; second → despawn
  - **Capture** (Euro winner only, `attack>0`): Colonists / Wagon → nation flip +
    `@COLONISTCAPTURE*` / `@WAGONCAPTURE` / `@CARGOCAPTURE`. Veteran Colonist
    specialty stripped → `@COLONISTCAPTURE2`. **Natives never capture.**
  - **Type demote** (keep nation): Dragoon→Soldier, Soldier→Colonist,
    Cont.Cav→Cont.Army, Cavalry→Regulars, Cont.Army→Colonist (+ Jesuit →
    Missionary); `@DEMOTE` if human-facing
  - else despawn (Pioneers, Missionaries, Scouts, Regulars, …)
- `units_sweep_stack_after_loss` (`FUN_5fef_0ec0`) apply loss to leftover
  non-combat same-nation stackmates (skips the primary loser already resolved).
  **Gated on a defender loss** (DOS 5fef ~0x2532): runs only when the
  attacker's type attack byte is 0 or a ship (type 0xd..0x12) is party — never
  for a normal land attack. On an ATTACKER loss it runs unconditionally.
- Winner: Washington always-promote; else chance promote (`FUN_5fef_172c`)
- Native def: settlement fallout (`FUN_5fef_31ea`) + `@LOOT` (treasure) /
  `@LOOT2` (burn, no treasure)

### Land loss

Same loss apply on attacker; defender may promote.

### Demote (`FUN_5fef_0352` type table; `FUN_5fef_16ea` promote-path specialty)

Combat loss remaps **unit type** (not merely profession). Cite:
`viceroy_unpacked.c` `FUN_5fef_0352` demote arm.

### Naval (`units_apply_naval_loss_outcome`) — DOS `FUN_5fef_0352` model (2026-09-02)

- **Evasion first** (`FUN_5bfb_312e`, DOS 1b0e tail; preempts the rolled
  outcome): defender with type attack BELOW the attacker's escapes on
  `roll(1, atk_pow+def_pow) <= def_pow`, power = movement+3, Privateer ×2,
  Galleon +3, −4 per occupied hold, min 1 → `@EVASIVE`, no outcome.
- Both ships → `units_plunder_ship_holds` (`FUN_5fef_016c`) runs BEFORE the
  damage/sink split; the loser's holds are then zeroed either way (goods not
  lifted vanish, passengers are lost — `units_ship_lose_holds`).
- **Damage vs sink is a roll on @UNIT guns/hull** (DS `0x523b`/`0x523c`):
  `roll(1, winner.guns + loser.hull) <= loser.hull` → survives damaged
  (bit7, `@SHIPDAMAGE`, teleport to nearest own Drydock colony, else nearest
  own colony as the stored-port stand-in, else EOT routing); otherwise
  despawn (`@SHIPSUNK`). A gunless victor (guns 0: all transports) can only
  drive off damaged, never sink. No "already damaged" special: a re-loss
  re-rolls, as in DOS.
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
  `0x5235`-column repair-turn formula (Linux keeps its drydock-tick model),
  and the per-nation stored-port coords (`DS -0x77c6`) for the damaged
  teleport target.

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

---

## Combat Analysis

[`combat_analysis.c`](../src/core/combat_analysis.c) — `FUN_636c_0000`-shaped
dual column. Shown **before** the combat roll (strengths known; no outcome yet).

- Gate: `game_options.combat_analysis` + human side (`FUN_5fef_1b0e` gate
  `0x5383&2`)
- Input: armed after mouse-up so village Attack CHOICE click cannot dismiss
  the dialog on the same press
- Village Attack (empty tile): `FUN_5fef_1b0e` temp Brave spawn (not nearby
  pull) so strengths / Analysis run before the roll; dwelling `population`
  drains on win
- Layout (FUN_636c_0000 draw pass): frame `w=0xd6` at `x=0x35`, row pitch
  `0x14`, height by tallest column, vertically centered (frame widens past
  DOS 214 only when a label+value row can’t fit its half column in
  FONTINTR — DOS overdraws there instead).
  1. Centered title `COMBAT ANALYSIS` (LABELS.TXT)
  2. Header row per column: unit chrome + type name, **baseline** strength
     right-aligned (`NAMES` attack/defense byte from `base_combat` / DOS
     `-0x72fa` — not the post-×8 roll weight)
  3. Modifier rows per side: label left, `±N%` value right-aligned at the
     column edge (DOS 013c label / 0150 value split)
- Flag rows (LABELS-shaped, DOS check order): Veteran, Cargo, **Attack
  Bonus** (land ×3/2), Expeditionary Force, Tories/Rebels (WoI support %),
  Ambush (attacker terrain, DOS `0x2e56`) / Terrain (defender `0x2e58`),
  Colony/Stockade/Fortress, village row labeled with the **tribe name**
  (LABELS has no "Village"), Artillery In Open, Artillery Vs. Raid,
  Fortified, Spain Bonus, Drake. No roll, no Victory/Defeat.
- Not ported from DOS 636c: Fatigue −33%/−66% rows (bits `0x100`/`a156&8` —
  strength calc doesn’t model fatigue), the 0x400 sprite row (label
  `0x97de`, unidentified), row icons for terrain/colony/village lines, and
  the cheat-mode (`0x5383&0x20`) final-weight footer rows.
- Roll still uses post-modifier odds weights (`atk` / `def` in
  `roll 1..(atk+def)`); those values are not printed in the header.
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
| Resolve | `units_fort_vs_ship`: fort atk vs ship defense (Drake on Privateer); close win→`col1_unknown15` bit7 + MP drain; else win→sink (no plunder); miss→`moves_left=0` |
| Repair | `units_tick_drydock_repair` clears combat bit7 for finished ships on own Drydock colony (EOT after ship-build tick); human `@REFIT` ai_popup OK |
| Turn | `turn_run_coastal_fort_fire` after colony production |
| AI | `ai_euro_tile_under_enemy_fort_fire` / flee |

Deep DOS notes: [`coastal_fort_fire.md`](../original_sources_annotated/turn/coastal_fort_fire.md).

**Bit7 collision:** same latch as ship construction. Distinguisher:
`turns_worked < type.defense` → construction (`units_tick_ship_build_ready`);
`>=` → combat damage (fort/naval), repaired only by Drydock.

**Analysis (bugs.md 267):** `units_fort_vs_ship` presents Combat Analysis
pre-roll (attacker_id −1, `eng.atk_label` = "<Colony> Fort/Fortress", battery
strength as base; Drake row on scaled defense). **PARKED:** DOS temp-attacker
spawn + fort VGA chrome.

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
| `1b0e` peels | Done | Colony REF +50%; Tory/Rebel support % Done |
| `1b0e` combat-entry MP surcharge ("ship-slow") | Done | 2026-08-24: `unit+0x3149 += 3` on every attack, win or lose, stacked on the normal step cost — `units_try_move`'s `combat_attack_mp_surcharge`. Land units usually fully drained either way; ships retain leftover MP (genuinely "slowed") |
| Promote / demote / capture / treasure | Done | Ransom Accept/Refuse Done; wagon/colonist capture Done |
| Naval damage / sink / plunder | Done | Close-fight escape path Done; Privateer `@SEIZURESEA` |
| Combat Analysis | Done | Options-gated dual column |
| Coastal fort fire | Done | Miss→MP drain; close hit→bit7; Drydock repair + `@REFIT` Done thin; temp unit/VGA PARKED |
| Outcome popups `@EUROPE*` / `@SHIP*` / `@LOOT*` / `@CAPTURED*` / `@BURNED*` | Done | Playable matrix; Europe `@LOOTCASH` separate — [popups.md](popups.md) |
| Village settlement battle `4528` | Done thin | Warn→Attack/Leave; empty-tile temp Brave from adjacent (stay put) + pop drain / destroy; fallout `@LOOT`/`@LOOT2`; deep mid-body/VGA PARKED. 2026-08-24: `1b0e`'s "no live defender" arm re-verified field-for-field (tribe struct `+7` muskets / `+10` horse_breeding → `units_spawn_village_temp_defender`) — confirmed already correct, not a stub. Sibling arm (undefended **Euro** colony, not a village) spawns a *different* temp defender — see next row, closed 2026-08-26 |
| Undefended Euro colony token-militia | Done | `units_spawn_colony_temp_defender` (`units.c`) — phantom civilian defender fielded whenever a Euro colony has colonists but no live defender and Paul Revere's armed-soldier override doesn't apply; was previously a free capture. See [port_plan.md](port_plan.md) P5.4 (was W1.8) for the full DOS trace |
| Euro mid combat scoring `20e6` | Done thin | Settlement/siege peels + adjacent toughness; deep −0x6790 matrix PARKED |
| VGA-identical combat chrome | PARKED | — |

---

## Conflicts resolved

| Topic | Rejected / stale | Authoritative |
|-------|------------------|---------------|
| Fort land defense | Manual / wiki Fort **+150%** as a distinct `015e` tier | Decomp: Fort shares Stockade `local_1a=4` (×2). Wiki +150% ≈ fortified Stockade path (`local_1a=6` → ×2.5). Fortress `local_1a=8` (×3) |
| `colonies_fortification_defense_bonus_percent` | Live land combat | Helper returns 100/150/200 for AI/UI; **live land combat uses `combat_colony_local_1a`** |
| Fandom “Port: combat Missing” | Stale Units row | Land/naval Partial — this hub + [manual_gap.md](manual_gap.md) |
| Difficulty “combat unaffected” | Old [difficulty.md](difficulty.md) note | Human Euro `str -= (difficulty-4)` + Discoverer −25% in `1b0e` |
| SoL popular support | Manual SoL/Tory share by side | **Done**: crown `+(100−SoL)%` (Tories), rebel `+SoL%` (Rebels) on colony — [sons_of_liberty.md](sons_of_liberty.md) |

---

## PARKED

| Gap | Where |
|-----|-------|
| Village raid / settlement deep `2820` + VGA | `ai_contact.c`, [move_enter.md](move_enter.md), [indians.md](indians.md) |
| Deep Euro combat −0x6790 — **still open**; the explore ring and the rest of the `20e6` land arms shipped 2026-08-27 (`ai_port_plan.md` T1.18), so only the combat core is left here | `ai_euro.c` / [move_scoring_land.md](../original_sources_annotated/ai/move_scoring_land.md) |
| Fort-fire temp unit + camera / VGA chrome | `units_coastal_fort_fire_pulse` |
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
| `tests/unit/test_units.c` | move-enter combat; naval; fort fire; analysis gate; land engage/colony; best defender; capture/loot; popups |
| `tests/unit/test_founding_fathers.c` | Washington promote; Drake naval; Revere; fallout |
| `tests/unit/test_ai_euro_war.c` | naval/land hunt; adjacent combat chain; Stockade note |
| `tests/unit/test_ai_king.c` | Cont. promote (king path) |
| `tests/unit/test_ai_contact.c` | raid resolve fallout |

---

## FUN_* index

| DOS | Linux |
|-----|-------|
| `FUN_157e_004a` | `combat_unit_base_x8` |
| `FUN_157e_015e` | `combat_engagement_strength` |
| `FUN_157e_0008` / `15eb_038e` | village probe count |
| `FUN_5fef_1b0e` | `combat_apply_1b0e_peels` + resolve roll shell + `units_try_move`'s combat-entry MP surcharge (ship-slow) |
| `FUN_5fef_0000` | `units_best_defender_at` |
| `FUN_5fef_0352` | `units_apply_land_loss_outcome` |
| `FUN_5fef_0ec0` | `units_sweep_stack_after_loss` |
| `FUN_5fef_016c` | `units_plunder_ship_holds` |
| `FUN_5fef_16ea` / `172c` | demote / chance promote |
| `FUN_5fef_1908` | **not combat** — King's Galleon treasure offer (@KINGGALLEON2/3), see `euro_unit_act.md` 2026-08-27 |
| `FUN_5fef_31ea` | native settlement fallout |
| `FUN_636c_0000` / `2a1f_0704` | Combat Analysis |
| `FUN_364b_03f6` | coastal fort fire |
| `FUN_465b_*` / `FUN_4720_*` | move-enter / combat trigger |
| `FUN_4d56_4528` / `5fef_0f14` | village warn→Attack + raid/fallout Done thin; deep `2820` PARKED |

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
