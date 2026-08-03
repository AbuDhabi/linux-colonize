# Seed-100 / Brave fidelity — session notes

Update this file whenever you learn something durable. Chat summaries drop detail.
Do **not** edit the plan file; plan lives at
`/home/jan/.cursor/plans/seed100_mapgen_fidelity_5b57654f.plan.md`.

## Goal

`smoke_mapgen_seed100` vs `test-saves-mapgen/SEED100.SAV`. Any-seed DOS-faithful gen;
seed 100 is golden only. **No** fixture-apply runtime path.

## Status board

| Area | State |
|------|--------|
| Terrain / land mask / rivers / arctic / HS | GREEN (all MP tiles) |
| Tribes capitals+sats | GREEN 34/34 |
| Euro fleets / human start / unit count | GREEN |
| Post-sat LCG | Locked `rng.state = 3528268925` (port smoke log) |
| `FUN_67bf` continents → `layer3` | PORTED (`map_gen_assign_continents`) |
| Brave checks shape (06b4/0768/0754, OR1) | PORTED in `ai.c` |
| Brave coordinates | **RED** — first miss `unit[12] type=19 nation=4 at (10,24)`; port accepts `(10,25)` attempt 0 |
| Debug `fprintf`s in `ai.c` | Still present — strip on green |
| Docs claiming Braves match | Premature — fix on green |

## VR_SEED.EXE (verified)

- Diff vs `VICEROY.EXE`: **15 bytes** @ file `0xe4d2` only.
- Patches sole BIOS timer read `FUN_1c0c_0012` → `MOV AX,100; MOV DX,0; RETF`.
- Chain: `FUN_6a09` entry → `04ca` → `002c` → `0008` → `1c0c_0012` → `0df2(AX&0x7fff)` = **seed 100**.
- Pushed `[83a6]` ignored (`ADD SP,2`). No Brave-specific unpatched seed.
- **Braves are on seed 100.** Coordinate bug is not a missed seed stub.

## Tribe RNG (correct this vs stale docs)

- DOS+VR_SEED: `6a09` **reseeds from timer (=100)** at entry.
- Port: `dos_rng_seed(rng, params->rng_seed)` at tribe entry (matches).
- Smoke log: `tribe-entry rng=100` → cargo → capitals → `sat done ... rng=3528268925`.
- Older “post-axes restore” wording in docs/plan is outdated for this path.

## Axes from seed 100

Fixture/`smoke` constants: `(mass,form,temp,climate,forest) = (0,0,0,2,2)` via `range(0,3)`.
(Some plan text still says climate `1` — trust `src/data/seed100_fixture.h`.)

## Brave loop (ASM-locked, `FUN_6a09`)

Per village, ≤100 tries:

1. Cap continent = `06b4` → `137f_01ca` = **`layer3/path & 0x0f`** (AND in `01ca`)
2. `range(-2,2)` dx then dy
3. `0302` inset → same continent → `0768` ocean/HS only → `0754 & 3` flags
4. Spawn type `0x13` via `095c` → `02ca` OR flag bit0 + owner high nibble

No other checks in ASM between those. `095c` is **outside** the retry loop.

## Why `(10,25)` is not rejected by documented checks

From `3528268925`:

| attempt | offset | tile |
|---------|--------|------|
| 0–1 | `(+1,+2)` → `(10,25)` | |
| 2 | `(+1,+1)` → `(10,24)` | golden |

Port at accept time: `(10,25) t=02 l2=00 l3=f9` (continent 9 = capital low nibble).
Golden planes: `(10,25) tile=02 mask=00 path=f8` (low nibble **8**, same as capital `48`).
Both port and golden: continent match, not water, flags clear → **must accept** under ASM rules.

`SEED100.SAV` is turn 0 / year 1492 — Braves have not wandered.

## Simulation results (do not rediscover blindly)

- Base checks + golden planes + village flags + OR1 from `3528268925`: **0/34** Brave matches.
- Force-reject only `(10,25)`: tribe0 fixed, still **~1/34**.
- Geometric / coastal / forest / burn sweeps: max **~6–8/34**; no 34/34.
- **Full-byte path compare** looks like it fixes tribe0 only because golden **final** path at `(10,24)` is already `48` (post-Brave owner). At Brave-time that tile is still unowned (`f8`) → full-byte would reject all unowned candidates. **False lead.**
- Extra `next()` burns before Braves: burn=4 gives brave0 `(10,24)` but still **1/34**.

## Plane wiring

- `layer2` ≈ flags `0x160`; `layer3` ≈ continent/owner `0x164`.
- After `67bf`: port IDs differ from golden path low nibbles (e.g. **9** vs **8**) but neighborhood connectivity matches for Brave0 tiles.
- Villages OR bit2; before tribes set all `layer3` high nibbles to `0xf`.

## Key files

- `src/core/map_gen.c` — `map_gen_assign_continents`
- `src/core/ai.c` — tribes/Braves + debug prints
- `src/core/dos_rng.c`
- `tests/smoke/test_mapgen_seed100.c`
- `COLONIZE/VR_SEED.EXE`, `viceroy_unpacked.c` / `.asm`

## Next digs (ordered)

1. Find a **DOS-faithful** reject (or RNG burn) that is **not** in the obvious 4 checks — re-read `095c`/`02ca`/`15eb_0a76` and anything between sat end and Brave for-loop; confirm `03ac` does not touch LCG.
2. Re-verify sat-loop RNG burn count vs DOS (including `03ac` per sat iteration, empty `n_cand` paths) — tribe xy match makes large divergence unlikely, but off-by-a-few draws is still open.
3. Do **not** hardcode `(10,24)` or invent geometric heuristics.
4. When green: strip debug prints; fix premature Brave wording in `docs/assets.md` / `docs/decomp_inventory.md`.

## Smoke command

```bash
cmake --build build --target smoke_mapgen_seed100
./build/smoke_mapgen_seed100   # cwd = repo root
```
