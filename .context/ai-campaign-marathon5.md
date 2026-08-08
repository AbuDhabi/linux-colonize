# AI parallel campaign marathon 5 (2026-08-08)

- Started epoch: 1786177194
- Deadline: +1h → epoch 1786180794
- **Stopped:** thin structural AI exhausted after R2 (~50 min early); ctest 38/38 green.
- Goal: thin AI deepen using Pass5 unlocks (LCR seek, FF combat col1, dump bid>0, …)

## Exclusive files
| Track | Owns |
|-------|------|
| Euro | `ai_euro.c`, `test_ai_euro_expand.c`, annotate `euro_unit_act.md` |
| King | `ai_king.c/.h`, `test_ai_king.c`, annotate `king_ref.md` |
| Contact+FF | `units.c/.h`, `founding_fathers.c/.h` (comments), `test_units.c` / `test_founding_fathers.c` |
| Diplo | skip (thin exhausted Marathon4) |

## Accuracy bar
Prefer Colonization.pdf / fandom / NAMES/decomp. No invented gold/join%/treasure.
PARK missing hooks; cite source in comments.

## R1 targets
- Euro: Scout path-to-rumour (MD≤8); boycott-aware Europe hold sell skip
- King: dump-goods candidates require live bid>0 when europe set
- Contact+FF: `units_resolve_land_combat` use `g_units_ff_col1` (Washington); `villages_burned++` on fallout

## Still PARKED
Full 2820/4528/20e6; Cortes gold amounts; Sepulveda join%; full LCR RNG;
dump modal; FA F2–F9; VGA letter cinematic.

## Contact+FF R1 (done)
- `units_resolve_land_combat` → `g_units_ff_col1` (Washington promote for AI/king/contact).
- `villages_burned++` on successful `units_try_native_settlement_fallout` tribe destroy.
- Smokes: `test_founding_fathers` wrapper promote; `test_units` villages_burned 1→2.

## Contact+FF R2 (done)
- `units_resolve_naval_combat` → `g_units_ff_col1` (Drake privateer *3/2 for AI/king).
- Cite: PEDIA/wiki Francis Drake (+50%); `units_drake_scale_strength` *3/2.
- Smokes: `test_founding_fathers` Drake wrapper via `units_set_ff_col1` (not only `_ff`).
- PARK leftovers: Cortes gold amounts; Sepulveda join%; full LCR RNG; deep naval 20e6.

## King R1 (done)
- Dump-goods eligible = live Europe `bid > 0` when `ctx->europe` set (`ai_king_tax_refuse_hike` candidate mask + pick API); europe NULL → uniform prior. Cite: FUN_38fd_3dc8 / local_7a.
- Smokes: pick skips bid≤0; refuse sole-priced Tobacco; europe-NULL still second cargo.
- PARK leftovers: dump modal CHOICE / VGA letter cinematic.

## Euro R1 (done)
- Scout fog explore (`ai_euro_scout_fog_explore_target`): prefer `map_tile_has_rumour` over plain unseen within MD≤8; Seasoned still deeper within tier. Cite: Colonization.pdf LCR / Seasoned Scout; Pass5 LCR scaffold (resolve on stand only).
- Europe dump-sell (`ai_euro_try_transport_europe_sell`): skip holds whose cargo bit is in `nation.boycott_bitmap`. Cite: wiki Boycott / king refuse.
- Smokes: `smoke_scout_fog_prefer_rumour`; `smoke_transport_europe_sell_skip_boycott`.
- PARK: full LCR RNG / gold/FoY table; Custom House auto-sell (Stuyvesant) — still PARK after R2 prefer.

## R2 candidates (if R1 green + time)
- Euro: Stuyvesant Custom House construction prefer (`has_peter_stuyvesant`)
- King: thin leftover only if bid>0 leaves gap
- Contact+FF: only if R1 leaves a thin FF call-site
- Stop early if thin structural AI exhausted (like M4)

## R1 integrate
- ctest **38/38** at epoch ~1786177613 (~53 min left to deadline)

## R2 targets (launched)
- Euro: Stuyvesant Custom House construction prefer (Drydock/Shipyard pattern)
- Contact+FF: `units_resolve_naval_combat` → `g_units_ff_col1` (Drake *3/2)
- King: skip (no thin leftover after bid>0)

## Euro R2 (done)
- `ai_euro_prefer_custom_house` when Stuyvesant owned; after Drydock→Shipyard; carpenter LABOR stay. Cite: fandom Stuyvesant; colony.c gate.
- PARK: Custom House auto-sell gold/thresholds (wiki 100/50 — not invented).

## Contact+FF R2 (done)
- `units_resolve_naval_combat` → `g_units_ff_col1` (Drake privateer *3/2 for AI/king). Twin of land R1.

## R2 integrate + stop
- ctest **38/38** at epoch ~1786177764 (~50 min early vs deadline).
- **Stopped:** thin structural AI exhausted (same posture as Marathon4). Leftovers need invent amounts, missing APIs, or deep 2820/4528/20e6 / VGA.

## Euro R2 (done)
- Stuyvesant Custom House construction prefer: `ai_euro_prefer_custom_house`
  sets `ColoniesBuildableOpts.has_peter_stuyvesant` when
  `founding_fathers_nation_has(..., FF_PETER_STUYVESANT)`; idle colonies queue
  Custom House via `colonies_set_construction` (after Drydock→Shipyard).
  Carpenter LABOR stay includes Custom House. Cite: fandom Stuyvesant;
  colony.c Custom House gate; founding_fathers elect comment.
- Smoke: `smoke_stuyvesant_custom_house_prefer`.
- PARK leftovers: Custom House auto-sell gold/thresholds (wiki 100/50) — not invented.
