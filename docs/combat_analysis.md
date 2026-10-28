# Combat Analysis

Deep dive into DOS's `FUN_636c_0000` Combat Analysis dialog and related combat-outcome mechanics.

Reference: [combat.md](combat.md) for overview and entry points.

## Contents

- [Combat Analysis](#combat-analysis)
- [Ship-slow](#ship-slow-units_ship_slow_scan-2026-09-16)
- [Coastal fort fire](#coastal-fort-fire)
- [Founding Father hooks](#founding-father-hooks)
- [Pixel reference](#pixel-reference)
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
- Layout (FUN_636c_0000 draw pass; **pixel-verified against a DOSBox-X capture
  2026-10-28**, every text/icon/frame pixel identical): frame `x=0x35 w=0xd6`
  fixed (never widens; a too-long label overdraws like DOS), `h = (2 + tallest
  column's rows) * 0x14 + 6`, `y = 100 - (h >> 1)` (asm 636c:003c-0059). Title
  centred in `[0x38, 0x38+0xd0)` at `frame_y+9` using FUN_104b_0216 width
  (pixel width - 1). Column pen x = `0x3a` (attacker) / `0xa0` (defender);
  rows start at `frame_y+0x17`, pitch `0x14`, text y = row top + 6; unit
  chrome and row icons blit at (pen, row top) (muskets +2); label at
  pen + indent, value right-aligned at pen + `0x50` (636c:10b1-10b9, 01aa).
  Row pictures: the 20px colony / village sprites are centred in a 16x16 cell
  at the pen (`x + (16-w)/2`, 2px left of it), as 112b_0c64 / 0790 do on the map; the
  village additionally gets the tribe tint marks (tech 0: 3, tech 1: 2, tech 2+: none,
  same table as `map_panel.c`) and, for a capital, the ICONS.SS #17 starburst.
  See "Pixel reference".
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
  | Ambush / Terrain (`0x80`) | `FUN_281f_033a` → `1baa_0006` | Engagement tile (both columns show the same tile). `1baa_0006` is a plain surface-to-surface rect copy off the **rendered map**, so the tile carries its overlays — a mountain over Savannah shows the mountain. The port redraws it from `map_tile_layer_cmds` (TERRAIN.SS + PHYS0.SS, the same list the map viewport and colony minimap blit); `terrain_x`/`terrain_y` on `ColonizeCombatSideFlags` carry the tile, and the bare `terrain_sprite` base tile is only the fallback when the map / PHYS0.SS are missing | `0x11` |
  | Fort tier (`0x40`) | `FUN_281f_02a8` → `112b_0c64` @100 | ICONS.SS #0–3 + owner flag (`colonies_blit_settlement_icon`) | `0x14` |
  | Village (`0x8`) | `FUN_281f_02b2` → `112b_0790` @100 | ICONS.SS #10–13 by tribe tech (`112b_0790` reads `indian[tribe].tech` from `DS:0x5ad8` stride `0x4e`, clamps it to 3 and blits `11 + tech` 1-based; `DS:0x84c` is indexed by *tribe* and supplies both the relation-bar colour and the tribe tint dots painted onto the shared low-tech sprites at `112b:0815` — 3 marks for tech 0, 2 for tech 1, none above), plus the capital starburst ICONS.SS #17 when the settlement's `+3` bit `0x04` is set | `0x14` |

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
  one-shot latch (`g_units_revere_muskets_latch`, `combat_analysis.c` — DOS writes a
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
- **Cheat footer (port_bugs.md #1177c).** With `head.game_options.cheats_enabled`
  (`0x5383&0x20`) 636c:0f00-1088 adds two 0x14-pitch rows per column: the
  header's own name with `+<final weight>` (`ColonizeCombatEngagement.w_atk` /
  `w_def`, the post-1b0e-handicap weights = FUN_2a1f_0704's params 11/12), then
  an empty label with `+<roll>` (attacker) / `+<w_atk + w_def>` (defender); no
  percent suffix. DOS rolls BEFORE the dialog (`FUN_281f_04d4` raw 91467, then
  `FUN_2a1f_0704` raw 91483 gets the roll as its last argument), so
  `units_combat_present_and_roll` and the coastal-fort path in
  `units_fort_vs_ship` now draw the roll ahead of the presenter (same RNG
  stream; the popup pump runs first, so answered popups still precede the draw)
  and hand it over as `cheat_roll`; the dialog's own copy still zeroes
  `roll` / `atk_wins`. `COMBAT_ANALYSIS_LINES_MAX` is 14 for the two extra rows.
- **WoI support row labels — fixed 2026-09-08.** DOS's rows read `0x2ec2` /
  `0x2ec4` = LABELS lines 147/148 **"Tory Unrest" / "Rebel Unrest"**. Address →
  line is `addr = 0x2d9c + 2*line` (anchors: `0x2e52` = line 91 "Fatigue",
  `0x2e8a` = line 119 "Bombard" per port_bugs.md #242, `0x2e3c` = line 80 "Veteran",
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

`FUN_5bfb_3180` naval half (decomp 98519-98624), run by 465b's commit tail after **every ship step**, human and AI. Port: called from `units_try_move_w`'s commit tail, so all movers get it per step.

| | |
|---|---|
| Gate | mover is a ship with MP left; pair not at PEACE (`0a38 & 0x40` clear) **or** mover is a Privateer. A Privateer therefore slows and is slowed regardless of relations |
| A. Warship | adjacent foreign ship on a **water** tile (a docked ship is on land → branch B). Per ship in that stack: drain by the **neighbour's** type — Privateer 4, Frigate 6, Man-O-War 8 thirds, other hulls nothing. Roll `1..(P_self + P_foe + 2)` with `P = 312e` = max MP thirds + 3, ×2 Privateer, +3 Galleon, −4 per hold in use, floor 1. roll < P_self → no slow, `@SHIPRUN` if either side human; roll == P_self → half drain; else full. `@SHIPSLOW` (0x1a51) when the mover is human. Stops once the mover is out of MP |
| B. Fort | adjacent foreign colony: Fortress → +50 on the spent byte (a dead stop; the overspend is kept, so a Man-O-War at spent 6 saves 56), Fort → +2 thirds, Stockade nothing. No roll. `@SHIPSLOW` (0x1a5a) with the building name when the mover is human |
| Not | fort fire (below) — that is an end-of-turn temp-attacker combat and never touches MP |
| Test | `test_ai_euro_war_*.c` (split 2026-09-23) `unit_naval_ambush` (drain 0/4/8 sweep, PEACE, Privateer, Stockade/Fort/Fortress) |

## Coastal fort fire

| Piece | API |
|-------|-----|
| Strength | `units_coastal_fort_attack_strength` = `4 * tier * (1 + arty)`; Fort tier1, Fortress tier2 |
| Pulse | `units_coastal_fort_fire_pulse_w` — all Fort/Fortress colonies, 8 ocean dirs |
| Hostile | at war (Euro/Indian) **or** Privateer |
| Resolve | `units_fort_vs_ship`: fort atk vs ship defense (Drake scales Privateer); Combat Analysis is presented first when a human is involved (port_bugs.md #261), then `roll(1, atk+def) <= atk` |
| Fort wins | port_bugs.md #249 — the same outcomes as a naval fight: holds lost, then the DOS `0352` damage-vs-sink roll with the fort's strength standing in for the winner's guns column. Damaged → `col1_flags15` bit7, `moves=0`, `repair_pending=2`, relocate to the nearest own Drydock colony with the DOS repair timer doubled (non-ship winner), `@SHIPDAMAGE`; a WoI human with no repair port sinks instead. Undamaged → sink, `@SHIPSUNK`, no plunder |
| Fort loses | port_bugs.md #249 — **nothing happens**: DOS undoes the temp attacker and the ship sails on. The old `moves=0` ship-slow here was invented; the real MP drain is the separate per-step `FUN_5bfb_3180` branch, see "Ship-slow" below |
| Repair | `units_tick_drydock_repair` clears combat bit7 for finished ships on own Drydock colony (EOT after ship-build tick); human `@REFIT` ai_popup OK |
| Turn | `units_coastal_fort_fire_colony_w` at the top of each colony's EOT |
| AI | none: the invented flee step / -800 ocean penalty were deleted (port_bugs.md #1033); DOS fears a battery only in the LAB_4d2e wander (goods aboard) |

Deep DOS notes: [`coastal_fort_fire.md`](../original_sources_annotated/turn/coastal_fort_fire.md).

**Bit7 collision:** same latch as ship construction. Distinguisher:
`col1_counter16 < type.defense` → construction (`units_tick_ship_build_ready`);
`>=` → combat damage (fort/naval), repaired only by Drydock.

**Analysis (port_bugs.md #261):** `units_fort_vs_ship` presents Combat Analysis
pre-roll (attacker_id −1, `eng.atk_label` = "<Colony> Fort/Fortress", battery
strength as base; Drake row on scaled defense). **Camera ported (D4):** 03f6's `FUN_281f_0352` (-> `FUN_1984_02fc` edge-test scroll onto the colony, raw 57100) fires before @FORTFIRE via `units_set_camera_edge_hook` (`game_camera_edge_center`), and the 1b0e defender-tile centre plus lunge go through `units_set_phantom_lunge_hook` (`game_phantom_lunge`; plain combat watch with attacker -1 only when the pool has no Artillery row). **Lunge sprite ported:** 03f6's attacker is the `FUN_478c_002c` phantom (type 0x17, at the colony tile). Its graphic byte is `*DS:0x52cc` = `@UNIT` row 11 (Artillery) icon, passed `0a20` -> `thunk 2a1f_01bc` -> `FUN_478c_0002` which stamps it into row 0x17 `+2` (`0x5374`); `0x5372` (+0) = the Fort/Fortress name word, `0x5377/78` = strength. So the 1b0e bump draws the Artillery sprite from the colony toward the ship. The port has no row 0x17 and never puts the phantom in the pool (a spawn could raise @TOOMANYUNITS, and the destroy path touches the census and settlement bits), so `units_fort_vs_ship` hands the Artillery row, fort nation and colony tile to the phantom-lunge hook, which draws the bump UI-only; fired only when the combat watch is installed and a human is involved. Row 0x17's attack/defense/`0x537d` are unused by the port (strength computed in `units_fort_vs_ship`).

---

## Founding Father hooks

| FF | Combat effect | Where |
|----|---------------|-------|
| George Washington | Non-veteran Soldier/Dragoon who wins always promotes | `units_resolve_land_combat_ff` |
| Francis Drake | Privateer combat +50% | `combat_unit_base_x8` |
| Paul Revere | Auto-arm empty colony under attack | `units_revere_defend_colony_tile` |
| Cortes / Sepulveda | Conquest treasure **bonus** (not a gate — see above) / convert-join on native fallout | `units_try_native_settlement_fallout` |

---

## Pixel reference

`unit_combat_analysis_pixels` (`tests/unit/test_combat_analysis_pixels.c`) renders six real
engagements and compares the whole frame with DOSBox-X screenshots of VICEROY.EXE:
`test-saves-combat/NAME.SAV` is the fixture the DOS run loaded, `NAME.dos.ppm` the 214xN crop of
the DOS screenshot (cursor included, masked in the test). Cases: Soldier vs Soldier in the open
(cheat footer, terrain row icon), Soldier vs empty colony with Stockade / Fort, Artillery vs empty
colony, Soldier vs Sioux camp, vs Sioux capital (village rows). The cheat footer roll is DOS's wall-
clock draw: the test searches a seed for the captured value.

Recipe (docs/dos_trace.md): `sav_json` the fixture (add a human unit next to the target; units
appended to `units[]`, `head.unit_count`, `head.active_unit`), `dosbox_trace.py setup W --save X.SAV`,
`start`, `send load.py` (`d.load_slot(0)`), keys `w` then Enter (dismiss the soldier hint), then
the direction key (village: Enter on "Attack Village" after the woodcut), `d.shot('u.png')`;
crop (53, 100-(h>>1), 214, h) at 1x. Four sessions can run side by side on `DOSBOX_TRACE_DISPLAY=:62..`.
A DOS unit moved by hand is not on the colony tile chain: a Soldier edited onto an empty
colony tile was ignored as defender by DOS (it still spawned the militia phantom).

All known deltas are fixed (only the mouse cursor is masked). Fixed 2026-10-28:
- *Militia header (0x200).* FUN_5fef_1b0e raw 100418-100431: `local_b0 = FUN_281f_04d4(0, colony+0x1f - 1)`
  (rand over the colonists), `local_ae = FUN_281f_0c54(local_b0)` = FUN_15eb_0e52 (colony `+0x40+idx`
  profession byte), stored at the phantom's `+0x315b` after FUN_291f_0a20. 636c reads that byte back, so the
  header, icon (FUN_112b_0002 jump table) and cheat footer show the PROFESSION (@JOB col 0). Port:
  `units_revere_defend_colony_tile` copies the picked colonist's profession onto the phantom,
  `COMBAT_FLAG_MILITIA` (0x200, set when the militia path runs) makes `combat_analysis_side_names` use
  `colonies_profession_name`. The phantom's plan glyph is '-' in the captures (only 0x2d is pixel-exact; the
  origin is not in the decomp).
- *Fortification byte 2.* The save mask is bit-per-row (byte 2 = Fort only, no Stockade). The percent is
  FUN_157e_0008 = number of owned rows 0..2 (Fort alone = 1, so +100%), but the label is FUN_15eb_0434(0), which
  walks the chain 0 -> 1 -> 2 and stops at the first unowned link (Stockade missing -> -1 -> LABELS "Colony").
  Port: `fort_name_tier` (consecutive tiers from Stockade) picks the label, `fort_tier` (count) the percent.
  No DOS capture of its own; pinned by `unit_combat_strength` `test_fort_without_stockade_labels_colony`.

## PARKED

| Gap | Where |
|-----|-------|
| ~~Village `2820` chief portrait frame + BGM cue~~ DONE: portrait via `ai_contact_chief_flair`, cue via `ai_contact_village_bgm_cue` | `ai_contact_demand.c` |
| ~~Fort-fire temp unit + camera / VGA chrome~~ DONE 2026-10-28 (port_plan.md D4: camera + Artillery-sprite lunge) | `units_coastal_fort_fire_pulse_w`, `game_loop.c` |

---

## Implementation map

| Concern | Module |
|---------|--------|
| Strength / peels / engage | [`combat_strength.c`](../src/core/combat_strength.c) |
| Combat Analysis UI | [`combat_analysis.c`](../src/core/combat_analysis.c), [`combat_analysis_render.c`](../src/core/combat_analysis_render.c) |
| Resolve / promote / demote / plunder / fort fire | [`units_combat.c`](../src/core/units_combat.c), [`units_combat_resolve.c`](../src/core/units_combat_resolve.c), [`units_capture.c`](../src/core/units_capture.c) |
| Enter trigger | [`units_move.c`](../src/core/units_move.c) `units_try_move_w` — [move_enter.md](move_enter.md) |
| Euro / king / raid callers | [`ai_euro.c`](../src/core/ai_euro.c), [`ai_king.c`](../src/core/ai_king.c), [`ai_contact.c`](../src/core/ai_contact.c) |
| EOT fort pulse | [`turn_production.c`](../src/core/turn_production.c) `turn_run_colony_production_with_w` → `units_coastal_fort_fire_colony_w` |

`colonies_fortification_defense_bonus_percent` (100/150/200) is for AI / UI
only; live land combat uses `combat_colony_fort_strength`.

### Tests

| File | Coverage |
|------|----------|
| `tests/unit/test_units_*.c (split 2026-09-23)` | move-enter combat; naval; fort fire; analysis gate; land engage/colony; best defender; capture/loot; popups |
| `tests/unit/test_founding_fathers.c` | Washington promote; Drake naval; Revere; fallout |
| `tests/unit/test_ai_euro_war_*.c` (split 2026-09-23) | naval/land hunt; adjacent combat chain; Stockade note |
| `tests/unit/test_ai_king_*.c` (split 2026-09-23) | Cont. promote (king path) |
| `tests/unit/test_ai_contact_*.c` (split 2026-09-23) | raid resolve fallout |

---

## FUN_* index

| DOS | OpenCol |
|-----|-------|
| `FUN_157e_004a` | `combat_unit_base_x8` |
| `FUN_157e_015e` | `combat_engagement_strength` |
| `FUN_157e_0008` / `15eb_038e` | colony fortification tier 0..3 (Stockade/Fort/Fortress chain probe) — `combat_colony_fort_strength` |
| `FUN_5fef_1b0e` | `combat_apply_1b0e_peels` + resolve roll shell + `units_try_move_w`'s combat-entry full MP exhaust (`combat_attack_entry`) |
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
| `FUN_4d56_4528` / `5fef_0f14` | village warn→Attack + raid/fallout Done thin; `2820` ported (`ai_contact_trade.c`) |

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
- [ai_euro_logic_map.yaml](ai_euro_logic_map.yaml) — AI combat callers / `20e6`
