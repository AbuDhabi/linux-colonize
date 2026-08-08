# Marathon8 AI ↔ prereq (2026-08-08)

Continue Marathon7 cadence (~2h). Unpark stale Done docs between slices.
Cite-only; no invent gold/join%/LCR tables.

## Landed

### Unpark (docs)
- `manual_gap.md`: Stockade/Fort/Fortress land defense % **Done**; FF Cortes/de Witt
  no longer “hooks OPEN”.
- `ai_transcription.md` unpark #3: Cortes cash + de Witt delivery **Done**.
- `building_production.md` / `fandom_col1994.md`: SoL ± production + coastal fort
  naval fire status aligned with code.

### A — Prereq: SoL craft + bells/crosses deepen
- `colony_craft_*` takes `sol_bonus`; +1/+2 per manufacturing worker on output.
- Bells/crosses: `sol_b * workers` (passive once if no workers).
- Smoke: `smoke_colonies` SoL craft tools uplift.
- Cite: `building_production.md` SoL ≥50%/+1, =100%/+2. PARK: Tory −1;
  preview craft still sol=0 without Col1 bridge.

### B — Prereq: coastal Fort/Fortress naval fire
- Peel `FUN_364b_03f6`: strength `4 * tier * (1+arty)` (Fort tier=1, Fortress=2).
- `units_coastal_fort_attack_strength` / `units_coastal_fort_fire_pulse`.
- Hostile: Euro×Euro war, Indian at-war, or Privateer (peace ignored).
- Fort win → sink ship (no temp attacker / no hold plunder).
- Wired `turn_run_coastal_fort_fire` after production in EOT SETUP.
- Smoke: `smoke_units` strength + war sink + peace no-fire + Privateer.
- Cite: decomp 57016; fandom Fort 4+4/arty, Fortress 8+8/arty.
- PARK: ship-slow formula; LOS/facing chrome from `0970`.

### C — AI: flee + hunt avoid fort batteries
- `ai_euro_naval_try_flee_fort_fire` before war hunt/attack.
- War hunt skips foe/coast tiles under fort fire.
- Ocean score −800 for battery tiles.
- Smoke: `smoke_naval_flee_fort_fire`.

### D — Prereq: warehouse spoilage + preview SoL
- `colonies_apply_warehouse_spoilage` after Custom House in EOT (FUN_15eb_0a50).
- Colony preview takes Col1 SoL for field/craft/hammers.
- AI: near-cap with Warehouse owned prefers Warehouse Expansion (not before Fort).

## Still PARKED
Deep `20e6` combat×8; Sepulveda join%; full LCR RNG; KINGGALLEON2; FA F2–F9;
Tory −1; ship-slow (no numeric formula); Custom House per-cargo UI;
VGA/2820/letter cinematic; map road blit.

## Stop
Ended early: remaining AI↔prereq slices need deep peels or invent amounts.
ctest **38/38**.
