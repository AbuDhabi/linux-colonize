# Marathon6 — AI ↔ prereq alternate (2026-08-08)

Continuing the alternate cadence after Alternate2 (de Witt AI + Cortes capital).

## Done this pass

### A — AI: Indian war land hunt + capital prefer
- `at_war_land` includes Indian hostility sticky **when** a tribe/Brave hunt
  target exists (avoids memset relation=0 skipping peace fortify).
- `ai_euro_land_war_hunt_target` hunts at-war natives + tribe tiles; prefer
  `tribe.state.capital`.
- Adjacent land attack requires `ai_diplo_indian_at_war` for natives.
- Planning F: alarmed capital MILITARY prio 5 vs 3.
- Smoke: `smoke_indian_war_capital_hunt`.

### B — Prereq: Brewster dock filter
- `effect_brewster_filter_pool` also converts Indentured/Criminal dock slots
  to Free Colonists (pool filter already Done).
- Smoke in `test_founding_fathers.c`.

### C/D — Annotate + unpark
- `euro_unit_act.md` §2c; unpark log.

## Still PARKED
Deep `20e6` combat×8 / −0x6790; Sepulveda join%; full LCR RNG; KINGGALLEON2;
FA `3f41` F2–F9; dump-goods modal CHOICE; VGA letter cinematic.
