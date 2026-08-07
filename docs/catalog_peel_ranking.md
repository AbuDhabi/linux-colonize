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
| Purpose one-liners | 1465 / 2380 | 2026-08-07 |
| Purpose unknown | 915 | ″ |
| Unknown by system | thunk 319 · platform 289 · mapgen/`2a1f` 236 · ui 68 · parked `205f`/`1d1c` 3 | ″ |

---

## Open queue (highest value first)

Take from the top. Split into parallel shards when counts are large and
symbols disjoint. Re-count unlabeled 1-hops before launching (numbers drift).

| # | Status | Layer | Target | ~N | Why |
|--:|--------|-------|--------|---:|-----|
| 1 | Done | C | `FUN_2f2b_6372` colony keyboard dispatcher | 0 | Closed via colony UI C batch (union w/ #2) |
| 2 | Done | C | `FUN_2f2b_628a` colony mouse/panel dispatcher | 0 | Closed via colony UI C batch |
| 3 | **Next** | C | `FUN_38fd_4f6e` ∪ `3746` ∪ `4e8e` Europe input | **14** | Union shrunk after colony shared thunks; finish leftover Europe hops |
| 4 | Open | C | `FUN_2b5a_2464` map menu-command mega-dispatch | ~23 | GAME/VIEW/ORDERS; complements closed `2b5a_3b68` |
| 5 | Done | C | `FUN_5952_035e` colony production tick | 0 | Closed (excl leftovers + shared via colony) |
| 6 | Done | C | `FUN_4d56_4528` Indian combat/raid cluster | 0 | Closed (excl 24 + shared via colony) |
| 7 | Done | C | `FUN_364b_0688` colony EOT production tick | 0 | Closed with #5 |
| 8 | Open | C | `FUN_2f2b_2f3e` / `FUN_2f2b_6cd4` assign + colony entry crumbs | ~5+5 | `348c` already 0; finish tiny leftovers |
| 9 | Open | B | Small UI leftovers (`19f6`/`1acb`/`1a0a`/`4720`/`6f30`/`74a4`/`7a65`/…) | ~68 | Sweep remaining purpose-dark UI crumbs |
| 10 | Open | B | Small platform leftovers (`1a29`/`19ef`/`78d8`/`7962`/`79a8`/`7421`/…) | ~40 | Non-mega DOS helpers after mid platform closed |
| 11 | Deferred | B/C | Megaseg bulk `281f` + `291f` (remaining thunks) | ~300+ | Prefer Layer C hops first |
| 12 | Deferred | B | `2a1f` mapgen-adjacent bulk | ~236 | Prefer hops from mapgen/AI entries |
| 13 | Deferred | B | Platform megasegs `1d1d` + `210d` | ~232 | DOS/EMS runtime; port when bringing up loaders |
| 14 | Parked | — | `205f` (opaque table) · `1d1c` (empty stub) · MAPEDIT | 3+ | No Layer A revisit without new evidence |

Suggested parallel batches (examples):

- Europe leftovers C: `#3` (14) ∪ map menu `#4` (23) ∪ colony crumbs `#8` (~10).
- Crumb B: `#9` ∪ `#10`.

---

## Done (newest first)

Mark finished peels here so the Open queue stays short. Keep one line per batch.

| When | Layer | Batch | N | Notes |
|------|-------|-------|--:|-------|
| 2026-08-07 | C | colony UI `6372`∪`628a` lo/hi + `4d56_4528` excl + `5952`∪`364b` excl | 122 | Colony dispatchers + raid + sim ticks 1-hop closed; Europe union →14 |
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
