# AI parallel campaign marathon 3 (2026-08-08)

- Started epoch: 1786171007
- Deadline: +1h → epoch 1786174607
- **Stopped:** structural OPEN/unparkable AI tracks exhausted (~32 min early); ctest 38/38 green.

## Hook survey (R0)
Unparked this hour: Euro Drydock/Shipyard + planters + FOOD + workplace + construction pick; Franklin; Brebeuf; Las Casas; King garrison/capital-rally/siege/SoL50; Privateer spawn-only.
Still correctly PARKED: 2820/4528, Sepulveda, Cortes (no conquest-treasure hook), dump-goods/160a, VGA, Brave escort, FA 3f41 UI, privateer 8g, deep 20e6.

## Rounds
- R1: Euro Drydock+Cotton/Fur; King Dragoon garrison; Franklin; Jesuit mid-convert; `col1_save_init` FF=-1 fix.
- R2: Euro FOOD haul + ship FOOD + workplace; Brebeuf; Cont. capital-rally; Privateer spawn-only (8g PARK).
- R3: Euro Stockade→Warehouse→Docks; Las Casas assimilate; Artillery siege spawn smoke; SoL50 promote; Diplo alliance longevity smoke.
- R4: Euro Shipyard + Gunsmith/Fur Trader workplace; Cortes PARK (no hook) + Pocahontas comment sync; King/Diplo docs.

## Parent fixes
- `col1_save_init`: `founding_father[i]=-1` (zero-fill falsely owned all FFs as nation 0 → broke declare_war).

## Explicitly out
T3 / LCG / seed-100 golden edits; VGA-identical dialogs; inventing gold/crosses fiction.
