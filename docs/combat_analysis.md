# Combat Analysis

Deep dive into DOS's `FUN_6f74` Combat Analysis dialog and related combat-outcome mechanics.

Reference: [combat.md](combat.md) for overview and entry points.

## Contents

- [Combat Analysis](#combat-analysis)
- [Ship-slow](#ship-slow)
- [Coastal fort fire](#coastal-fort-fire)
- [Founding Father hooks](#founding-father-hooks)
- [Status matrix](#status-matrix)
- [Conflicts resolved](#conflicts-resolved)
- [PARKED](#parked)
- [Implementation map](#implementation-map)
- [FUN_* index](#fun-index)
- [See also](#see-also)

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
- Font: **FONTTINY** (the map HUD font). DOS `636c` sets no font of its own,
  so it inherits whatever the map left current. The port used the FONTINTR
  dialog font, which also oversized the header unit chrome — the orders box
  sizes itself from the font metrics (`unit_chrome.c` `box_w`/`box_h`).
- Layout (FUN_636c_0000 draw pass): frame `w=0xd6` at `x=0x35`, row pitch
  `0x14`, height by tallest column, vertically centered (frame widens past
  DOS 214 only when a label+value row can’t fit its half column in the
  current font — DOS overdraws there instead).
  1. Centered title `COMBAT ANALYSIS` (LABELS.TXT)
  2. Header row per column: unit chrome + type name, **baseline** strength
     right-aligned (`NAMES` attack/defense byte from `base_combat` / DOS
     `-0x72fa` — not the post-×8 roll weight)
  3. Modifier rows per side: label left, `±N%` value right-aligned at the
     column edge (DOS 013c label / 0150 value split)
- Flag rows (LABELS-shaped, DOS check order): **Muskets** (`0x400`, first row —
  see below), Veteran, Cargo, **Attack
  Bonus** (the ×3/2 attack factor, `DS:0x8d00` bit 0 — **land and naval**,
  see "Naval" under Strength pipeline), Bombard, Tory Unrest / Rebel Unrest (WoI support %),
  Ambush (attacker terrain, DOS `0x2e56`) / Terrain (defender `0x2e58`),
  the **fort-tier** row — label = the topmost built fortification's own name
  (`FUN_281f_0bdc` = `FUN_15eb_0434(0)` walking the `DS:0x8f82+4` chain), or
  LABELS `Colony` (`0x2e5a`) with none; value = `(FUN_157e_0008 + 1) * 50`, so
  Colony +50 / Stockade +100 / **Fort +150** / Fortress +200 —
  village row labeled with the NAMES **@LEVELS**
  noun by tribe tech — Camp/Village/City, or "Capital" (`@LEVELS` row 5,
  DS:0x964c) when the dwelling is a capital, at +50 / +100 / ×2 (636c bit-8
  row; corrected 2026-09-03 — it never prints the tribe name),
  Artillery In Open, Artillery Vs. Raid,
  Fortified, Spain Bonus, Drake. No roll, no Victory/Defeat.
- Fatigue −33%/−66% rows (bits `0x100`/`a156&8`, both labelled `0x2e52`)
  ported 2026-09-04 with the penalty itself — see **Attacker fatigue** below.
  Placed in DOS's bit-walk position 2026-09-07: they sit **between Cargo and
  Attack Bonus** (asm `636c:02ff` / `636c:03da`), not at the end of the column,
  and DOS tests them as two independent `if`s.
- **Row icons** (ported 2026-09-07). Four flag rows blit a picture at the
  column's left edge and push their own label right by the indent DOS adds to
  its label pen (`local_76`) straight after the blit:

  | Row | DOS blit | Sprite | Indent |
  |-----|----------|--------|--------|
  | Bombard (`0x8000`) | `FUN_281f_0254` → `1c36_000a` | ICONS.SS: Man-O-War (`DS:0x532e` = `@UNIT` 18 icon) when the colony's `+0x1c` bit `0x40` (coastal) is set or there is no colony, else Artillery (`DS:0x52cc` = `@UNIT` 11) | `0x10` |
  | Ambush / Terrain (`0x80`) | `FUN_281f_033a` → `1baa_0006` | TERRAIN.SS engagement tile (both columns show the same tile) | `0x11` |
  | Fort tier (`0x40`) | `FUN_281f_02a8` → `112b_0c64` @100 | ICONS.SS #0–3 + owner flag (`colonies_blit_settlement_icon`) | `0x14` |
  | Village (`0x8`) | `FUN_281f_02b2` → `112b_0790` @100 | ICONS.SS #10–13 by tribe tech (`112b_0790` reads `indian[tribe].tech` from `DS:0x5ad8` stride `0x4e`, clamps it to 3 and blits `11 + tech` 1-based; `DS:0x84c` is indexed by *tribe* and only supplies the relation-bar colour), plus the capital starburst ICONS.SS #17 when the settlement's `+3` bit `0x04` is set | `0x14` |

  `DS:0x83e` is ICONS.SS (loaded from the `'icons'` name at `DS:0x23d6`), and
  the `@UNIT` icon byte lives at `+2` of the stride-`0xe` unit table `DS:0x5230`.
- **0x400 = the Paul Revere Muskets row** (identified + render side ported
  2026-09-08). It is the **first** modifier row, ahead of Veteran (asm
  `636c:01d4`-`0288`). `DS:0x97de` is not a LABELS slot at all — it is the
  runtime pointer the Indian Adviser (`FUN_3f41_010a`, asm `3f41:...ff36de97`)
  prints its Muskets tally with, i.e. NAMES.TXT **@CARGO 15 "Muskets"**. The
  blit pushes the literal `AX = 0x26`, DOS's 1-based icon space, so the port
  blits ICONS.SS **37** (`@CARGO0–15` = #22–37 ⇒ 15 = Muskets); indent `8`.
  The value is `0146` (sign) + `0182` (1) with **no** `010a`, so DOS prints a
  bare **"+1"** — an additive point of base combat, not a percentage.
  **Trigger** (`FUN_5fef_1b0e`, asm `5fef:1d2f`-`1d5f`): on the auto-spawned
  defender of an undefended colony, if the colony's nation has Founding Father
  **12 = Paul Revere** (`FUN_281f_07b4` → `FUN_15eb_3960(nation, 0xc)`) and the
  colony's Muskets stock (colony record `+0xb8` = `stock[15]`) is `>= 0x32`
  (50), DOS swaps the defender graphic to `0x4b`, `INC`s the base combat byte
  and sets `0x8d03 |= 4` (= flags `0x400`).
  **Trigger wired 2026-09-08.** `units_revere_defend_colony_tile` sets a
  one-shot latch (`g_units_revere_muskets_latch`, `units.c` — DOS writes a
  global for the same reason: the arm runs while the defender is being built,
  before the panel is drawn) whenever the gate fires;
  `units_resolve_land_combat_ff` consumes it into
  `eng.def_flags.flags |= COMBAT_FLAG_MUSKETS` right after
  `combat_land_engage`, i.e. before the analysis is presented. The port's gate
  is byte-identical to DOS's (`founding_fathers_revere_should_auto_arm`: FF 12
  owned, no standing soldier, `stock[15] >= UNITS_EQUIP_MUSKETS` = 50 =
  `> 0x31`).
  **Model realigned 2026-09-08 (phantom, not a real Soldier).** Revere is not
  a separate defender in DOS — it only *overrides* the militia phantom 1b0e
  spawns anyway. The scratch defender is `FUN_291f_0a20` = `FUN_478c_002c`
  (raw 76545-76564): it stamps unit type **`0x17`** and calls `FUN_478c_0002`
  (raw 76530-76543), which writes the stride-`0xe` `@UNIT` table row `0x17`
  (`0x5230 + 0x17*0xe` = `0x5372`) on the fly — graphic at `+2`, and **both**
  combat columns (`+5` defense, `+6` attack) from the passed base combat.
  Undo is `FUN_291f_0a06` = `FUN_478c_00d0` (raw 76594-76601): *"if the last
  unit's type byte is `0x17`, delete it"* — the phantom always evaporates.
  Base combat is `local_de = *(byte*)0x5235` = `@UNIT` row 0 (Colonists)
  **defense** = 1; Revere makes it 2 and the graphic `0x4b` (= port sprite 74,
  `UNITS_ICON_SOLDIER`). Those are exactly the port's `Soldiers` row
  (`NAMES.TXT`: attack 2 / defense 2), so `units_spawn_colony_temp_defender`
  now takes a `revere_armed` flag and picks `Soldiers` instead of `Colonists`;
  the colonist stays at work and the phantom is despawned unconditionally.
  The +1 also decides DOS's halving peel `*(byte*)(def_type*0xe+0x5235) < 2`
  (`combat_strength.c` `dt->defense < 2`), which the Revere phantom escapes —
  same here.
  **Muskets are never spent.** 1b0e reads colony `+0xb8` exactly twice — raw
  100425 as this gate, raw 100708 as the "tribe loots muskets" test on a
  burned town — and writes it on no outcome. The old eject model debited 50.
  **Loss consequence** is DOS's walk-in, raw 100680-100713 inside
  `if (bVar8) { … if (bVar28) { … } }`: Euro attacker → colony **captured**,
  no colonist dies; native attacker with pop > 1 → `FUN_281f_0a9c(local_b0)`
  = `FUN_15eb_0d04` shifts the colonist arrays down and does `colony+0x1f -=
  1`; native attacker with pop == 1 → colony destroyed (`FUN_291f_0254`),
  tribe gains horses/muskets if the town held any. All three already live in
  `units_try_capture_foreign_colony`.
- Still not ported from DOS 636c: the cheat-mode (`0x5383&0x20`) final-weight
  footer rows.
- **WoI support row labels — fixed 2026-09-08.** DOS's rows read `0x2ec2` /
  `0x2ec4` = LABELS lines 147/148 **"Tory Unrest" / "Rebel Unrest"**. Address →
  line is `addr = 0x2d9c + 2*line` (anchors: `0x2e52` = line 91 "Fatigue",
  `0x2e8a` = line 119 "Bombard" per bugs.md #242, `0x2e3c` = line 80 "Veteran",
  `0x2e54` = line 92 "Attack Bonus"). The port printed "Tories"/"Rebels"
  (LABELS lines 101/102, which 636c never reads).
- Roll still uses post-modifier odds weights (`atk` / `def` in
  `roll 1..(atk+def)`); those values are not printed in the header.
- Input: Esc / Enter / Space / click dismiss
- Presenter hook: tests / AI skip when unset (`combat_analysis_set_presenter`)

UI labels use manual-shaped percents (e.g. “Fortress +200%”, “Fortified +50%”)
even when the live multiplier comes from `local_1a` arithmetic above.

---

## Ship-slow (`units_ship_slow_scan`, 2026-09-16)

`FUN_5bfb_3180` naval half (decomp 98519-98624), run by 465b's commit tail after **every ship step**, human and AI. Port: called from `units_try_move`'s commit tail, so all movers get it per step.

| | |
|---|---|
| Gate | mover is a ship with MP left; pair not at PEACE (`0a38 & 0x40` clear) **or** mover is a Privateer. A Privateer therefore slows and is slowed regardless of relations |
| A. Warship | adjacent foreign ship on a **water** tile (a docked ship is on land → branch B). Per ship in that stack: drain by the **neighbour's** type — Privateer 4, Frigate 6, Man-O-War 8 thirds, other hulls nothing. Roll `1..(P_self + P_foe + 2)` with `P = 312e` = max MP thirds + 3, ×2 Privateer, +3 Galleon, −4 per hold in use, floor 1. roll < P_self → no slow, `@SHIPRUN` if either side human; roll == P_self → half drain; else full. `@SHIPSLOW` (0x1a51) when the mover is human. Stops once the mover is out of MP |
| B. Fort | adjacent foreign colony: Fortress → +50 spent (dead stop), Fort → +2 thirds, Stockade nothing. No roll. `@SHIPSLOW` (0x1a5a) with the building name when the mover is human |
| Not | fort fire (below) — that is an end-of-turn temp-attacker combat and never touches MP |
| Test | `test_ai_euro_war.c` `unit_naval_ambush` (drain 0/4/8 sweep, PEACE, Privateer, Stockade/Fort/Fortress) |

The pre-2026-09-16 port had only an AI-only end-of-act "naval ambush" keyed on the mover's own type with tie = no slow; deleted.

## Coastal fort fire

| Piece | API |
|-------|-----|
| Strength | `units_coastal_fort_attack_strength` = `4 * tier * (1 + arty)`; Fort tier1, Fortress tier2 |
| Pulse | `units_coastal_fort_fire_pulse` — all Fort/Fortress colonies, 8 ocean dirs |
| Hostile | at war (Euro/Indian) **or** Privateer |
| Resolve | `units_fort_vs_ship`: fort atk vs ship defense (Drake scales Privateer); Combat Analysis is presented first when a human is involved (bugs.md #261), then `roll(1, atk+def) <= atk` |
| Fort wins | bugs.md #249 — the same outcomes as a naval fight: holds lost, then the DOS `0352` damage-vs-sink roll with the fort's strength standing in for the winner's guns column. Damaged → `col1_unknown15` bit7, `moves_left=0`, `repair_pending=2`, relocate to the nearest own Drydock colony with the DOS repair timer doubled (non-ship winner), `@SHIPDAMAGE`; a WoI human with no repair port sinks instead. Undamaged → sink, `@SHIPSUNK`, no plunder |
| Fort loses | bugs.md #249 — **nothing happens**: DOS undoes the temp attacker and the ship sails on. The old `moves_left=0` ship-slow here was invented; the real MP drain is the separate per-step `FUN_5bfb_3180` branch, see "Ship-slow" below |
| Repair | `units_tick_drydock_repair` clears combat bit7 for finished ships on own Drydock colony (EOT after ship-build tick); human `@REFIT` ai_popup OK |
| Turn | `turn_run_coastal_fort_fire` after colony production |
| AI | `ai_euro_tile_under_enemy_fort_fire` / flee |

Deep DOS notes: [`coastal_fort_fire.md`](../original_sources_annotated/turn/coastal_fort_fire.md).

**Bit7 collision:** same latch as ship construction. Distinguisher:
`turns_worked < type.defense` → construction (`units_tick_ship_build_ready`);
`>=` → combat damage (fort/naval), repaired only by Drydock.

**Analysis (bugs.md #261):** `units_fort_vs_ship` presents Combat Analysis
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
| Cortes / Sepulveda | Conquest treasure **bonus** (not a gate — see above) / convert-join on native fallout | `units_try_native_settlement_fallout` |

---

## Status matrix

| Piece | Status | Notes |
|-------|--------|-------|
| Land / naval engage + roll | Done | `combat_*_engage` + resolve |
| Best defender | Done | `units_best_defender_at` |
| Colony / village / terrain / fortify site | Done | `015e` |
| `1b0e` peels | Done | Colony REF +50%; Tory/Rebel support % Done |
| `1b0e` combat-entry MP charge (full exhaust) | Done | **Re-derived 2026-09-10; there is no "ship-slow".** `FUN_5fef_1b0e` does `spent += 3` at entry (raw 100341-100343) but then calls `FUN_281f_0934` under the same attack flag (100381-100383) → `FUN_1427_155e` writes `spent = FUN_1427_065a(unit)`, the full max allotment (raw 8880-8888), so the `+3` is a dead store. Attacking exhausts the piece — win or lose, ships included. `FUN_465b`'s per-tile step cost and shore-crossing exhaust both sit inside `if (!bVar4)` (raw 75639-75648), so an attacker pays neither. Port: `units_try_move`'s `combat_attack_entry` flag routes all four outcome sites through `units_mp_exhaust`, charged *after* the resolve because the fatigue peel reads entry-remaining. Entry gate (raw 100359-100372): under 3 thirds remaining the attack is refused outright for natives/crown and for any AI-controlled Euro slot; only the interactive human gets the `@HALF` tired-attack CHOICE. 2026-09-03: the 465b MP overspend ROLL never denies an attack (`(04ca, bVar4)` third clause of the gate is the attack flag) |
| Promote / demote / capture / treasure | Done | Ransom Accept/Refuse Done; wagon/colonist capture Done |
| Naval damage / sink / plunder | Done | Close-fight escape path Done; Privateer `@SEIZURESEA` |
| Combat Analysis | Done | Options-gated dual column |
| Fizzle dissolve present (`FUN_12d6_0000` / thunk `FUN_281f_03ea`) | Done | 2026-09-07: destroyed/sunk/demoted piece "pixelates" away — DOS presents the outcome redraw by copying all 64000 px in 16-bit LFSR order (poly `0xB400`, asm file 0xF5E6; catalog mislabels 0016 "delay"). Fired dur 8 in `1b0e` tail, `36fe`, `31ea`, LCR `65dd` vanish. Port: `units_set_combat_dissolve` two-phase hook (pre-snapshot / post-animate) around land+naval outcome and LCR despawn; `game_combat_dissolve` diff-gates identical frames so out-of-sight or in-colony fights stay instant |
| Coastal fort fire | Done | Miss→MP drain; close hit→bit7; Drydock repair + `@REFIT` Done thin; temp unit/VGA PARKED |
| Outcome popups `@EUROPE*` / `@SHIP*` / `@LOOT*` / `@CAPTURED*` / `@BURNED*` | Done | Playable matrix; Europe `@LOOTCASH` separate — [popups.md](popups.md) |
| Village settlement battle `4528` | Done thin | Human `@ACTIONS` menu (P8.8) incl. Attack Village; empty-tile temp Brave from adjacent (stay put) + pop drain / destroy; fallout `@LOOT`/`@LOOT2`; deep mid-body/VGA PARKED. 2026-08-24: `1b0e`'s "no live defender" arm re-verified field-for-field (tribe struct `+7` muskets / `+10` horse_breeding → `units_spawn_village_temp_defender`) — confirmed already correct, not a stub. Sibling arm (undefended **Euro** colony, not a village) spawns a *different* temp defender — see next row, closed 2026-08-26 |
| Undefended Euro colony token-militia | Done | `units_spawn_colony_temp_defender` (`units.c`) — phantom defender fielded whenever a Euro colony has colonists but no live defender; was previously a free capture. 2026-09-08: **Paul Revere is the same phantom**, not a separate ejected Soldier — the FF only swaps its graphic to `0x4b` and its base combat 1 → 2 (port: `Soldiers` type instead of `Colonists`), never touches population and never spends the warehouse muskets. DOS shape: spawn `FUN_478c_002c` (type `0x17` + on-the-fly `@UNIT` row write `FUN_478c_0002`), undo `FUN_478c_00d0`. See [port_plan.md](port_plan.md) P5.4 (was W1.8) for the full DOS trace |
| Euro mid combat scoring `20e6` | Done thin | Settlement/siege peels + adjacent toughness; deep −0x6790 matrix PARKED |
| VGA-identical combat chrome | PARKED | — |

---

## Conflicts resolved

| Topic | Rejected / stale | Authoritative |
|-------|------------------|---------------|
| Fort land defense | Manual / wiki Fort **+150%** as a distinct `015e` tier | Decomp: Fort shares Stockade `local_1a=4` (×2). Wiki +150% ≈ fortified Stockade path (`local_1a=6` → ×2.5). Fortress `local_1a=8` (×3) |
| `colonies_fortification_defense_bonus_percent` | Live land combat | Helper returns 100/150/200 for AI/UI; **live land combat uses `combat_colony_local_1a`** |
| Fandom “Port: combat Missing” | Stale Units row | Land/naval Partial — this hub + [manual_gap.md](manual_gap.md) |
| Difficulty “combat unaffected” | Old [difficulty.md](difficulty.md) note | Human Euro `str -= (difficulty-4)` in `1b0e` peels, plus the resolve-only handicap group (`combat_apply_1b0e_resolve_handicaps`, raw 100534-100556): attacker of a human Euro damped/halved, beginner shield zeroes it, diff-0 human attacker doubled |
| SoL popular support | Manual SoL/Tory share by side | **Done**: crown `+(100−SoL)%` (Tories), rebel `+SoL%` (Rebels) on colony — [sons_of_liberty.md](sons_of_liberty.md) |

---

## PARKED

| Gap | Where |
|-----|-------|
| Village raid / settlement deep `2820` + VGA | `ai_contact.c`, [move_enter.md](move_enter.md), [indians.md](indians.md) |
| Deep Euro combat −0x6790 — **still open**; the explore ring and the rest of the `20e6` land arms shipped 2026-08-27 (`port_plan.md` T1.18), so only the combat core is left here | `ai_euro.c` / [move_scoring_land.md](../original_sources_annotated/ai/move_scoring_land.md) |
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
| `FUN_5fef_1b0e` | `combat_apply_1b0e_peels` + resolve roll shell + `units_try_move`'s combat-entry full MP exhaust (`combat_attack_entry`) |
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
- [port_plan.md](port_plan.md) — AI combat callers / `20e6`
