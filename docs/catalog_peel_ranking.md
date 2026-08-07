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
| 2 | Mid-size segments or Layer C hops from known entries (≤~70 new labels) | Megaseg bulk (`291f`/`2a1f`/`1d1d`/`210d`) |
| 3 | Unlocks a port neighborhood or resolves thunk→body for AI docs | MAPEDIT (parked) |
| 4 | Disjoint symbol sets for parallel shards | Overlapping ownership |

**Layer choice:** B when a segment cluster is still purpose-dark; C when the
bodies are labeled but callees (often thunks) are not.

---

## Counts (refresh after each merge)

| Metric | Value | As of |
|--------|------:|-------|
| Purpose one-liners | 1839 / 2380 | 2026-08-07 |
| Purpose unknown | 541 | ″ |
| Unknown by system | mapgen/`2a1f` 195 · platform 182 · thunk 161 · parked `205f`/`1d1c` 3 | ″ |

---

## Open queue (highest value first)

Take from the top. Split into parallel shards when counts are large and
symbols disjoint. Re-count unlabeled 1-hops before launching (numbers drift).

| # | Status | Layer | Target | ~N | Why |
|--:|--------|-------|--------|---:|-----|
| 1 | Done | C/B | Mid ≥8 hops + `281f` megaseg | — | Mid Layer C closed; `281f` purpose-closed (75) |
| 2 | **Next** | B | `291f` megaseg thunk bulk | **~161** | Next EMS far-thunk megaseg after `281f` |
| 3 | Open | B | `2a1f` mapgen-adjacent bulk | ~195 | Prefer after `291f`, or hop-driven if a port needs it |
| 4 | Open | B | Platform megasegs `1d1d` + `210d` rest | ~92+90 | DOS/EMS CRT + overlay runtime |
| 5 | Parked | — | `205f` · `1d1c` · MAPEDIT | 3+ | No Layer A revisit without new evidence |

Suggested parallel batches (examples):

- Split `291f` by address into 3–4 shards (~40 each).
- Or `291f` lo/hi ∪ start of `1d1d` / `210d` if wanting mixed megaseg progress.

---

## Done (newest first)

| When | Layer | Batch | N | Notes |
|------|-------|-------|--:|-------|
| 2026-08-07 | B+C | mid ≥8 hops (mapkey/move, newgame/splash, dialog/mapref) + `281f` megaseg | 113 | Mid Layer C closed; first megaseg slice purpose-closed |
| 2026-08-07 | C | embark + title + pedia + RM* | 52 | Naval UI, title menu, pedia index, archive open |
| 2026-08-07 | B+C | platform crumbs + boot + BGM | 99 | Non-mega platform closed |
| 2026-08-07 | B+C | Europe/colony crumbs + menu + UI B | 110 | Small UI closed |
| earlier | — | Prior waves | — | Colony/AI/mapdraw/platform mid peels |

---

## How to update this file

1. After merge + `gen_fun_catalog.py`, refresh **Counts** from README / catalog.
2. Move completed Open rows to **Done**.
3. Re-measure `~N` for the new top Open entries.
4. Only reorder Open rows when criteria change — note why.
5. Point README “Next” bullets at this file rather than inventing a new order.
