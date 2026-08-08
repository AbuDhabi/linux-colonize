# AI parallel campaign marathon 4 (2026-08-08)

- Started epoch: 1786175205
- Deadline: +1h → epoch 1786178805
- **Stopped:** thin structural AI unparks exhausted after R2 (~25 min early); ctest 38/38 green.

Prerequisites landed before start: follow-unit, destroy-building, naval plunder,
`europe_sell_unit_hold`, treasure spawn, dump-goods RNG, FF remainder gates,
presentation tags.

## Exclusive files
| Track | Owns |
|-------|------|
| Euro | `ai_euro.c`, `test_ai_euro_expand.c` |
| King | `ai_king.c/.h`, `test_ai_king.c` |
| Diplo | `ai_diplo.c/.h`, `test_ai_diplo.c` |
| Contact+FF | `ai_contact.c/.h`, `founding_fathers.c/.h`, smokes |

## Rounds
- R1 (parallel, ctest 38/38):
  - Euro: all-hold Europe sell; indian-land short-gold status; Expert Farmer dock hire
  - King: dump-goods named second; multi-garrison fortify cap 2
  - Diplo: DIPLO_FA/plunder docs; Privateer commission smoke; **no further thin diplo**
  - Contact+FF: escort goto-prefer; BURN building status; Cortes/Sepulveda still PARK
- R2 (Euro+King only):
  - Euro: Master Carpenter dock hire on construction LABOR; Expert Lumberjack dock PARKED (no lumber_short inv)
  - King: list all boycott_bitmap cargo names; Cont. Army/Cav capital fortify into cap-2

## Still correctly PARKED
Full `2820`/`4528`/`20e6`; Cortes treasure amount + settlement-conquer; Sepulveda join%;
dump-goods price-weight modal; FA F2–F9; VGA letter cinematic; privateer 8g
(null-units only); Expert Lumberjack dock (no inv field); T3/LCG/seed-100.

## Explicitly out
Invented gold/join%/treasure rates; VGA-identical dialogs; deep decomp bodies.
