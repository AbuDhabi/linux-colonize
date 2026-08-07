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
| Purpose one-liners | 1575 / 2380 | 2026-08-07 |
| Purpose unknown | 805 | ″ |
| Unknown by system | thunk 295 · platform 271 · mapgen/`2a1f` 236 · parked `205f`/`1d1c` 3 | ″ |

---

## Open queue (highest value first)

Take from the top. Split into parallel shards when counts are large and
symbols disjoint. Re-count unlabeled 1-hops before launching (numbers drift).

| # | Status | Layer | Target | ~N | Why |
|--:|--------|-------|--------|---:|-----|
| 1 | Done | C | Colony keyboard/mouse `6372`∪`628a` | 0 | Closed prior batch |
| 2 | Done | C | Europe input `38fd_4f6e`∪`3746`∪`4e8e` | 0 | Closed with colony crumbs |
| 3 | Done | C | Map menu `FUN_2b5a_2464` | 0 | Closed (ORDERS chrome + `210d` reloc) |
| 4 | Done | C | Colony assign/entry `2f2b_2f3e`∪`6cd4` | 0 | Closed with Europe crumbs |
| 5 | Done | C | Sim ticks / raid `5952`/`364b`/`4d56_4528` | 0 | Closed prior batch |
| 6 | Done | B | Small UI leftovers | 0 | Purpose-closed (68) |
| 7 | **Next** | B | Small platform leftovers (non-mega) | **~59** | Timer/XMS/resource/stream crumbs after mid platform |
| 8 | Open | C | `FUN_75c2_2d46` game boot video/memory/asset init | ~26 | Bring-up neighborhood |
| 9 | Open | C | `FUN_129f_0008` BGM/event id pick | ~14 | Sound leftovers |
| 10 | Deferred | B/C | Megaseg bulk `281f` + `291f` | ~295 thunk unk | Prefer targeted C hops |
| 11 | Deferred | B | `2a1f` mapgen-adjacent bulk | ~236 | Prefer hops from mapgen/AI entries |
| 12 | Deferred | B | Platform megasegs `1d1d` + `210d` rest | — | DOS/EMS runtime |
| 13 | Parked | — | `205f` · `1d1c` · MAPEDIT | 3+ | No Layer A revisit without new evidence |

Suggested parallel batches (examples):

- Platform B crumbs `#7` (split lo/hi) ∪ boot C `#8` ∪ BGM C `#9`.

---

## Done (newest first)

Mark finished peels here so the Open queue stays short. Keep one line per batch.

| When | Layer | Batch | N | Notes |
|------|-------|-------|--:|-------|
| 2026-08-07 | B+C | Europe+colony crumbs C + `2b5a_2464` C + UI B lo/hi | 110 | Europe/menu/colony entry C closed; all small UI purpose-closed |
| 2026-08-07 | C | colony UI `6372`∪`628a` lo/hi + `4d56_4528` excl + `5952`∪`364b` excl | 122 | Colony dispatchers + raid + sim ticks 1-hop closed |
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
