# Indians: Port Status

Development snapshots tracking native-nation / Indian-contact porting progress.

## Contents

- [2026-08-27 — single alarm store](#2026-08-27--single-alarm-store)
- [2026-09-06d — 152e / 1816 / 1b3a deepening](#2026-09-06d--152e-1816-1b3a-deepening)

---

### 2026-08-27 — single alarm store (relation/alarm consolidation)

`FUN_1000_84fc` / `FUN_15dc_00e0` (read) and `FUN_4cc6_00f2` (write) operate
on `indian[idx].alarm_by_player[euro]` — 0..100, high = hostile, map-gen seed
`RNG(0,14)` (+2×difficulty for AI nations), first contact clamps ≤20, **no
per-turn decay** (byte-stable across seed-100 TURN3..7). `nation.relation_by_indian`
is a flag byte (`0x60` once met) in every DOS save, never a scalar. Linux
therefore now has one store:

- `ai_diplo_indian_alarm` / `ai_diplo_indian_alarm_delta` — DOS-native; used at
  sites transcribed from DOS (152e accumulator ±1, 1816 WoI defect ±100 —
  direction was inverted before: the tribe turns *hostile to the rebels*,
  content with the Crown; 2820/417e price operands; `0x4b` MADAT gates).
- `ai_diplo_indian_relation` = `100 − alarm`, `_delta(d)` = `alarm_delta(−d)` —
  the Linux-side view for fandom-derived sites (thresholds <40 refuse-talk,
  <50 thin at-war, raids/attacks −5, trade +2).
- Retired as fiction: peaceful drift (+1/turn), peace feeler (+2), the
  relation ±1 arm of `ai_contact_indian_relation_tick`, the raid −3/−5 double
  push, the trade `alarm--` double push.
- "At war with tribe" (same day, second pass) = met ∧ (`euro_diplo & 0x02`
  ∨ alarm > 0x4a), i.e. `FUN_5bfb_153e`'s own test. Bit `0x02` = WAR on
  `euro_diplo` (`COL1_INDIAN_WAR_BIT`): DOS sets it only in `FUN_5bfb_153e`'s FA branch
  (`@SMITEINDIANS`/`@SMITEEUROPE` — pay an AI nation to declare war;
  unported) and clears it in `FUN_4cc6_00f2` when alarm cools below 75
  (mirrored in `ai_diplo_indian_alarm_delta`). Linux sticky bands moved to
  the DOS scale: at-war relation < 26, very-low < 16.

### 2026-09-06d — 152e / 1816 / 1b3a deepening (all three chains closed)

Result + trap + citation. Raw bodies: `viceroy_unpacked.c` 81387-81534
(`152e`), 81543-81683 (`1816`), 81684-81738 (`1b3a`).

**Resolved-symbol table** (all via `FUN_281f_X` → `FUN_1000_X`
(`tools/address_mapping.csv`) → the thunk's own `JMPF <reloc>:Y` → canonical;
raw thunk bytes read out of `viceroy_overlays.asm`, since Ghidra's C export
of several of these thunks is reloc-`0000` garbage):

| DOS call in 152e/1816/1b3a | Real target | Meaning | Linux |
|---|---|---|---|
| `FUN_281f_095c` | `FUN_1427_06b4` | unit CREATE (type, nation, x, y) | `ai_indian_152e_spawn_brave` |
| `FUN_281f_07b4` | `FUN_15eb_3960` | **Founding-Father bit test** (per-nation bitmap, stride `0x13c`) | `founding_fathers_nation_has` |
| `FUN_2a1f_0270` | `FUN_4962_06b6` | per-tribe-type census recount | `ai_contact_indian_census_4962_06b6` |
| `FUN_281f_09c8` | `FUN_157e_004a` | combat value ×8, mode 1 | `combat_unit_base_x8(.., 1, ..)` |
| `FUN_281f_081c` | `FUN_1427_0f0e` | continent id of a unit's tile | `map_continent_id_at` |
| `FUN_281f_0722` | `FUN_137f_02a0` | continent id of (x,y) | `map_continent_id_at` |
| `FUN_281f_06dc` | `FUN_137f_0200` | tile owner nibble (`0xf` → −1) | `ai_owner_nibble` |
| `FUN_281f_06d2` | `FUN_137f_0428` | settlement (layer2 `0x02`) **or** unit (`0x01`) at tile | `ai_layer2_at & 3` |
| `FUN_281f_0704` | `FUN_137f_0228` | stamp tile owner nibble | `ai_set_owner_nibble_move` |
| `FUN_281f_0c5e` | `FUN_15eb_0470` | Town-Hall ring tier → `DS:0x329[2..4]` | 8 tiles (P4.2 decision) |

**`152e` — no stubs left.**
- `FUN_281f_095c`'s argument was mis-transcribed as a "cost": DOS's `local_4`
  is the **unit type** — `0x13` Brave, `+1` when a musket is spent, `+2` when
  50 horse-breeding is spent, i.e. Armed / Mtd. Brave / `0x16` Mtd. Warrior.
  The village arms its founding Brave out of the nation's own stock. Branch is
  still unreachable (its gate `state.needs_colonist` has no Linux producer —
  village CREATE `FUN_4d56_0038` is unported), ported anyway.
- `FUN_281f_07b4(nation, 0x18/0x17)` = **FF 24 Las Casas doubles / FF 23
  Sepulveda halves** the mission goodwill a village grants (applied in that
  order, so owning both nets ×1). Matches their PEDIA text.
- Capital-only gate on the growth block kept (see the code comment: removing
  it regresses `golden_ai_turns` TURN1→2).

**Two real defects found in the already-"ported" threat scorer**
(`ai_indian_village_threat` = `FUN_4cc6_03f8`, the *only* DOS producer of
Indian alarm):
1. `min(capitol_x, pop/2)` was the wrong field. DOS reads
   `byte[tribe.nation_id * 0x4e + 0x59a0]`, and `0x59a0 + 4*0x4e == 0x5ad8 ==
   indian_base + 2`, so the operand is **`ind->tech`** (+2), not `capitol_x`
   (+0) — adjacent fields of the same record.
2. `FUN_281f_07b4(nation, 0x10)` was inert. It is **FF 16 Pocahontas**, and
   this halving *is* PEDIA's "all Indian alarm is generated half as fast" —
   the effect had no wiring anywhere before.

**`1816` — three missing state mutations ported, one mis-mapped clamp fixed.**
- **§4** `if ((char)indian[+7] < 0) = 0` is a clamp on **`muskets`**, not on
  alarm. DOS treats `muskets` as a signed byte everywhere (152e's spend test
  is `'\0' < muskets` too). The old `ai_contact_clamp_alarms` alarm clamp was
  a stand-in; kept, but relabelled Linux-only.
- **§6a goods decay** (was missing): `indian.tons[16]` (+0x0e, the 16 Col1
  cargo types) walks toward zero by `tech + 1` every Indian turn, clamped at
  0 and never crossing; a zero entry is not written at all. This is the trade
  memory that makes a flooded village pay well again later.
- **§6b census** (was missing): `FUN_4962_06b6` zeroes and recomputes
  `tribe_data_9184`, `tribe_population_totals`, `tribe_village_counts`,
  `tribe_dwellings_91cc[slot*16+continent]` and `village_counts_by_continent`.
  **DOS quirk kept literal:** `village_counts_by_continent` is wiped on every
  call but refilled from the current tribe type only, so after the eight 1816
  calls it holds the last type's villages — confirmed against real
  `TURN7.SAV`, which stores exactly one `3` in it while slot 7's village count
  is 3.
- **§6c horse breeding** (was missing): `horse_breeding += horse_herds`,
  capped at `(tribe_population_totals[slot] + 0x19) * 2` — which is why a
  small tribe never accumulates enough stock to field Mounted Braves. Reads
  the census §6b just refreshed, hence the ordering.
- **WoI defect windfall clamp fixed**: DOS clamps muskets/herds against
  `DS:0x962a[slot]` = **`tribe_village_counts`**, not `ind->tech`
  (`*(char *)(*(int *)0x8d52 - 0x69d6)`; save_format_map.md file offset 581).
  A large low-tech tribe was being disarmed on defection, a small high-tech
  one over-armed.

*Deliberately not ported from `1816`* (display-only / already covered):
`FUN_281f_04ca` timer reseed (Linux `ai_nation_reseed`), `FUN_281f_0a42`
context select and `FUN_281f_0590` turn-owner chrome colour, `FUN_281f_0470`
UI pump (all three are DOS presentation state Linux keeps elsewhere), the
`0x5394` active-nation word (`turn_set_active_nation`), and the act-loop /
`act_counter` shape, which lives in `ai_native_nation_pulse` unchanged — it is
golden-pinned and this pass deliberately did not touch it.

**`1b3a` — phases 1 and 3 ported** (`ai_indian_midpass_clear_tables` /
`ai_indian_midpass_claim_worked_tiles`, called from `turn.c` around the eight
`ai_indian_nation_turn` calls; phase 2 *is* that loop — the `FUN_41f2_0266`
label is the reloc-`0000` stub misresolve documented in
`mid_pass_indian_rank.md`).
- **Phase 1**: `DS:0x5b04` = `0x5ad6 + 0x2e` = `indian.contact_state[4]`.
  **Behavioural correction:** `ai_contact.c` treated `contact_state` as a
  permanent per-(tribe, Euro) latch ("a tribe that has ever brought gifts
  never begs again"). DOS wipes all 32 words at the top of every year, so it
  is a **per-year** latch — one gift-or-beg resolution per pair per turn.
  That is why DOS villages keep visiting.
- **Phase 3**: a colonist *actually working* a plot the natives still own
  transfers that plot's owner nibble to the colony — unless a settlement or
  any unit stands on it. `0704`'s @SEIZURE popup arm cannot fire here (it
  needs a native settlement on the tile, which the `06d2` gate already
  excluded), so this is a silent ownership stamp.
- **Trap (table mislabel, corrected):** `colonist_work_plot_28c8.md` labels
  `DS:0xde` as dx and `DS:0xc8` as dy. Every consumer disagrees —
  `1b3a`, `2f2b_0b97`, `15eb`'s build gate and `4cc6_03f8` all compute
  `x = colony.x + DS:0xc8[i]`, `y = colony.y + DS:0xde[i]`. So **`DS:0xc8` is
  dx and `DS:0xde` is dy**; with that swap the dumped bytes are exactly
  `ai.c`'s existing `k_ring_dx`/`k_ring_dy`. Harmless until now (the two
  tables are the same multiset, and both prior consumers only needed the
  *set*), but it matters the moment an index is paired with `colony.tiles[i]`.
  Linux drives phase 3 off `colonies_field_tile_delta` + `colony->tiles[]`
  instead of re-deriving the DS index, so the claimed tile set is identical
  regardless of enumeration order.

**Test evidence.** `tests/unit/test_ai_indian_census.c` (new,
`unit_ai_indian_census`) loads real `test-saves-ai/TURN7.SAV`, wipes the
census block, runs the tick for all eight slots and requires the DOS-stored
arrays back byte for byte — `tribe_village_counts = 06 01 07 05 06 03 03 03`,
`tribe_population_totals = 39 09 24 1a 1f 0a 0a 0a`,
`tribe_data_9184 = 30 08 38 28 30 18 18 18` (= 8 × Brave count) — plus the
continent quirk, the goods decay signs, the horse cap, 1b3a phase 1, and
phase 3's claim / unit-blocked / unworked verdicts. Full `ctest` **58/58**;
`golden_ai_turns` TURN1→7, `golden_mapgen_seed100`, `golden_ai_joint` all
unchanged and green.

*Scenario rewrites required by this pass* (old fixtures encoded the old
behavior): `test_ai.c` village-threat and `test_map_panel.c` village-chrome
fixtures `memset` their `ColonizeCol1Save`, which says nation 0 owns all 25
Fathers — unclaimed is `-1` (`col1_save_init`), and the scorer now really
reads FF 16; both now seed `head.founding_father[] = -1`.
`test_ai_contact.c`'s WoI-windfall case now seeds
`stuff.tribe_village_counts[0]` instead of relying on `tech`.
