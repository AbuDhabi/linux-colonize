# Catalog peel ranking

Stable priority queue for light-catalog peels (Layers B/C; Layer A closed).
Use this instead of re-ranking every session. After a peel batch: move finished
items to **Done**, refresh **Counts**, and take the next **Open** row(s).

Companion: peel protocol + live metrics in
[`original_sources_annotated/README.md`](../original_sources_annotated/README.md).
AI 1:1 port status stays in [`ai_transcription.md`](ai_transcription.md)
(Layer D) — orthogonal to this queue.

**Workflow reminder:** shard JSON under `.context/peel_shards/` → merge
`scripts/fun_catalog_seed.json` → `python3 scripts/gen_fun_catalog.py` → update
this file + README metrics.

---

## Ranking criteria (fixed)

Prefer targets that maximize **port / play fidelity per symbol**, not raw count.

| Rank weight | Prefer | Deprioritize |
|-------------|--------|--------------|
| 1 | Game simulation / AI / combat / colony / trade / turn logic | Pure blit chrome already purpose-closed nearby |
| 2 | Mid-size segments or Layer C hops from known entries (≤~70 new labels) | Megaseg bulk (`281f`/`291f`/`2a1f`/`1d1d`/`210d`) |
| 3 | Unlocks a port neighborhood or resolves thunk→body for AI docs | MAPEDIT (parked) |
| 4 | Disjoint symbol sets for parallel shards | Overlapping ownership |

**Layer choice:** B when a segment cluster is still purpose-dark; C when the
bodies are labeled but callees (often thunks) are not.

---

## Counts (refresh after each merge)

| Metric | Value | As of |
|--------|------:|-------|
| Purpose one-liners | 1343 / 2380 | 2026-08-07 |
| Purpose unknown | 1037 | ″ |
| Unknown by system | thunk 436 · platform 292 · mapgen/`2a1f` 238 · ui 68 · parked `205f`/`1d1c` 3 | ″ |

---

## Open queue (highest value first)

Take from the top. Split into parallel shards when counts are large and
symbols disjoint. Re-count unlabeled 1-hops before launching (numbers drift).

| # | Status | Layer | Target | ~N | Why |
|--:|--------|-------|--------|---:|-----|
| 1 | **Next** | C | `FUN_2f2b_6372` colony keyboard dispatcher | ~63 | Largest remaining colony 1-hop; mostly `281f`/`291f` thunks into labeled `2f2b`/`15eb` |
| 2 | Open | C | `FUN_2f2b_628a` colony mouse/panel dispatcher | ~44 | Complements #1; same colony-screen neighborhood |
| 3 | Open | C | `FUN_38fd_4f6e` Europe keyboard dispatcher | ~61 | Trade UI input; Europe bodies already purpose-closed |
| 4 | Open | C | `FUN_38fd_3746` Europe dock immigrant mega-dialog | ~53 | Board/orders path; noisy decomp — resolve via ASM |
| 5 | Open | C | `FUN_38fd_4e8e` Europe mouse/drag dispatcher | ~31 | Complements #3–4 |
| 6 | Open | C | `FUN_5952_035e` colony production/buildings/stock tick | ~41 | Simulation tick; port-relevant |
| 7 | Open | C | `FUN_4d56_4528` Indian combat/raid cluster | ~31 | AI/combat fidelity; extends closed `4d56_1816` hop |
| 8 | Open | C | `FUN_2b5a_2464` map menu-command mega-dispatch | ~29 | GAME/VIEW/ORDERS; complements closed `2b5a_3b68` |
| 9 | Open | C | `FUN_364b_0688` colony EOT production/SoL/construction | ~28 | Turn/colony sim |
| 10 | Open | C | `FUN_2f2b_348c` / `FUN_2f2b_2f3e` field-jobs / assign workplace | ~24+21 | Colonist job UI |
| 11 | Open | C | `FUN_2f2b_6cd4` colony screen entry | ~9 | Mostly closed by earlier peels; finish leftover thunks when touching colony |
| 12 | Open | B | Small UI leftovers (`19f6`/`1acb`/`1a0a`/`4720`/`6f30`/`74a4`/`7a65`/…) | ~68 | Sweep remaining purpose-dark UI crumbs |
| 13 | Open | B | Small platform leftovers (`1a29`/`19ef`/`78d8`/`7962`/`79a8`/`7421`/…) | ~40 | Non-mega DOS helpers after mid platform closed |
| 14 | Deferred | B/C | Megaseg bulk `281f` + `291f` (remaining thunks) | ~400+ | Label via Layer C hops (#1–11) first; bulk only if a port needs a cold symbol |
| 15 | Deferred | B | `2a1f` mapgen-adjacent bulk | ~238 | Prefer hops from mapgen/AI entries over segment sweep |
| 16 | Deferred | B | Platform megasegs `1d1d` + `210d` | ~232 | DOS/EMS runtime; port only when bringing up loaders |
| 17 | Parked | — | `205f` (opaque table) · `1d1c` (empty stub) · MAPEDIT | 3+ | No Layer A revisit without new evidence |

Suggested parallel batches (examples):

- Colony UI C: `#1` ∪ `#2` (disjoint if ownership assigned for any shared thunk).
- Europe UI C: `#3` ∪ `#4` ∪ `#5`.
- Sim ticks C: `#6` ∪ `#7` ∪ `#9`.
- Crumb B: `#12` ∪ `#13`.

---

## Done (newest first)

Mark finished peels here so the Open queue stays short. Keep one line per batch.

| When | Layer | Batch | N | Notes |
|------|-------|-------|--:|-------|
| 2026-08-07 | B+C | `15eb` lo/hi + platform mid + `2b5a_3b68`∪`2f2b_51ec` | 208 | `15eb` purpose-closed; platform PATH/config/heap/abort; map/colony UI thunks |
| 2026-08-07 | B+C | `43f7` + CUSTOMIZE/input/sound + `1a58` + `465b_0000` C | ~90 | Nation UI; mouse; move-spent hop |
| 2026-08-07 | B+C | `6cb2`/`4b58`/text blit + turn/EOT C | ~123 | Dialog widgets; turn neighborhood |
| earlier | B+C | map viewport + `6f74` + `684c_08c0` C | — | Viewport; dialog compositor; mapgen 1-hop |
| earlier | B | map accessors / trade-diplo UI / path-orders / `2b5a` | — | Mid sim/UI closeout |
| earlier | B | AI diplo / combat / `1427` / colony+landfall | — | High-value game logic |
| earlier | B | SAVEGAME / colony / Europe / turn / combat residual | — | First purpose waves |
| earlier | C | `4d56_1816` / `521d_6d8e` | — | Indian + Euro AI entries |
| earlier | A | VICEROY segment systems | 164/166 | Park `205f`/`1d1c` |

---

## How to update this file

1. After merge + `gen_fun_catalog.py`, refresh **Counts** from README / catalog.
2. Move completed Open rows to **Done** (or strike `#` and set Status=Done).
3. Re-measure `~N` for the new top Open Layer C entries (unlabeled 1-hop count).
4. Only reorder Open rows when criteria change or a port emergency jumps the queue — note why in the Notes column / commit message.
5. Point README “Next” bullets at this file rather than inventing a new order.
