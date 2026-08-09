# Catalog peel ranking

Stable priority queue for light-catalog peels (Layers B/C; Layer A closed).
Use this instead of re-ranking every session. After a peel batch: move finished
items to **Done**, refresh **Counts**, and take the next **Open** row(s).

Companion: peel protocol + live metrics in
[`original_sources_annotated/README.md`](../original_sources_annotated/README.md).
AI 1:1 port status stays in [`ai_transcription.md`](ai_transcription.md)
(Layer D) — orthogonal to this queue.

**Workflow reminder** (if peeling again): draft purpose shards → merge
`scripts/fun_catalog_seed.json` → `python3 scripts/gen_fun_catalog.py` → update
this file + README metrics. VICEROY purpose bulk is already closed (below).

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
| Purpose one-liners | 2380 / 2380 | 2026-08-07 |
| Purpose unknown | 0 | ″ |
| Unknown by system | none (VICEROY) | ″ |

---

## Open queue (highest value first)

| # | Status | Layer | Target | ~N | Why |
|--:|--------|-------|--------|---:|-----|
| 1 | Done | A/B | Unpark `205f` + `1d1c` | — | All VICEROY systems + purposes labeled |
| 2 | Parked | — | MAPEDIT | 125+ | No Layer A on MAPEDIT without a dedicated track |

**VICEROY light catalog is closed** (Layers A–C purpose bulk). Remaining work is MAPEDIT (parked) or Layer D deep extracts when a port needs them.

---

## Done (newest first)

| When | Layer | Batch | N | Notes |
|------|-------|-------|--:|-------|
| 2026-08-07 | A/B | Unpark `205f`/`1d1c` | 3 | VGA A000 addr + DS:0x26f0 table lookups; VICEROY 2380/2380 |
| 2026-08-07 | B | platform megasegs `1d1d`+`210d` (lo/hi×2) | 182 | CRT stdio/DOS + EMS/overlay runtime purpose-closed |
| 2026-08-07 | B | `2a1f` mapgen megaseg thunk bulk (lo/midlo/mid/midhi/hi) | 195 | EMS far-thunk megaseg purpose-closed |
| 2026-08-07 | B | `291f` megaseg thunk bulk (lo/midlo/midhi/hi) | 161 | EMS far-thunk megaseg purpose-closed |
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
