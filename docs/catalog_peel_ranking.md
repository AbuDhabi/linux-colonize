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
| Purpose one-liners | 1726 / 2380 | 2026-08-07 |
| Purpose unknown | 654 | ″ |
| Unknown by system | thunk 249 · mapgen/`2a1f` 210 · platform 192 · parked `205f`/`1d1c` 3 | ″ |

---

## Open queue (highest value first)

Take from the top. Split into parallel shards when counts are large and
symbols disjoint. Re-count unlabeled 1-hops before launching (numbers drift).

| # | Status | Layer | Target | ~N | Why |
|--:|--------|-------|--------|---:|-----|
| 1 | Done | C/B | Mid/high game UI + platform crumbs + boot/BGM + embark/title/pedia/RM | — | Closed through prior batches |
| 2 | **Next** | C | `FUN_2b5a_3252` extended map-key dispatch | ~9 | Last mid map-UI hop |
| 3 | Open | C | Small ≥8 hops: `75c2_235c` / `6f74_2580` / `6f30_0062` / `6b7e_0004` / `2b5a_3ae6` | ~8 each | Finish remaining mid-size 1-hops in parallel |
| 4 | Deferred | B/C | Megaseg bulk `281f` + `291f` | ~249 thunk unk | Prefer targeted C hops first |
| 5 | Deferred | B | `2a1f` mapgen-adjacent bulk | ~210 | Prefer hops from mapgen/AI entries |
| 6 | Deferred | B | Platform megasegs `1d1d` + `210d` rest | ~192 | DOS/EMS runtime bulk |
| 7 | Parked | — | `205f` · `1d1c` · MAPEDIT | 3+ | No Layer A revisit without new evidence |

Suggested parallel batches (examples):

- Map-key C `#2` ∪ union of `#3` hops (exclusive ownership) — closes remaining mid Layer C.
- Then selective megaseg bulk or Layer D as ports need.

---

## Done (newest first)

Mark finished peels here so the Open queue stays short. Keep one line per batch.

| When | Layer | Batch | N | Notes |
|------|-------|-------|--:|-------|
| 2026-08-07 | C | embark `4720_049e` + title `75c2_2778` + pedia `6cb2_2322` excl + RM `78ef_0002` excl | 52 | Naval UI, title menu, pedia index, archive open 1-hops closed |
| 2026-08-07 | B+C | platform crumbs lo/hi + boot `75c2_2d46` + BGM `129f_0008` | 99 | Non-mega platform purpose-closed; boot/BGM 1-hop closed |
| 2026-08-07 | B+C | Europe+colony crumbs C + `2b5a_2464` C + UI B lo/hi | 110 | Europe/menu/colony entry C; small UI purpose-closed |
| 2026-08-07 | C | colony UI `6372`∪`628a` + `4d56_4528` + `5952`∪`364b` excl | 122 | Colony dispatchers + raid + sim ticks |
| 2026-08-07 | B+C | `15eb` + platform mid + `2b5a_3b68`∪`2f2b_51ec` | 208 | `15eb` closed |
| earlier | — | Prior Layer A/B/C waves | — | See git history / older Done rows |

---

## How to update this file

1. After merge + `gen_fun_catalog.py`, refresh **Counts** from README / catalog.
2. Move completed Open rows to **Done** (or strike `#` and set Status=Done).
3. Re-measure `~N` for the new top Open Layer C entries (unlabeled 1-hop count).
4. Only reorder Open rows when criteria change or a port emergency jumps the queue — note why in the Notes column / commit message.
5. Point README “Next” bullets at this file rather than inventing a new order.
