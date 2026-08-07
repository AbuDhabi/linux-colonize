# Euro diplomacy (`15b3` / `5bfb`) — thin section-map

Layer D hygiene for bilateral flags + war/ally policy. Quiet Brave
`diplomacy_flags` stub: [`accessors.c`](accessors.c). Indian meet/raid:
[`indian_contact.md`](indian_contact.md).

Linux: [`src/core/ai_diplo.c`](../../src/core/ai_diplo.c) — **partial structural
port**. Odd deviations OK; not T3.

## `15b3` bilateral bytes

| Symbol | Thunk | Role |
|--------|-------|------|
| `FUN_15b3_0004` | `281f_0a38` | Read peer byte |
| `FUN_15b3_0032` | — | Write peer byte |
| `FUN_15b3_0066` | `281f_0a10` sibling | OR both directions; assert symmetry |
| `FUN_15b3_00d0` | `281f_0a10` | Clear both directions |

Decomp addressing:

- **Euro** (`nation < 4`): `*(peer + nation * 0x13c − 0x77c4)`
- **Indian** (`nation ≥ 4`): `*(peer + nation * 0x4e + 23000)` — full matrix **PORT DEBT** on Linux

Linux Euro×Euro stand-in (316-byte / `0x13c` nation record):

| Slot | Use |
|------|-----|
| `nation[a].unknown26[0..3]` | Treaty timers toward peer (6d8e §4) |
| `nation[a].unknown26[4..7]` | Diplo flag byte toward peer (`15b3` mirror) |
| `nation[a].unknown26[8]` | Indian hostility sticky (`1` once any `indian_at_war`) |

Exact DS `−0x77c4` Col1 field rename PARKED.

### Bit constants (Linux)

| Bit | Name | Notes |
|-----|------|-------|
| `0x01` | WAR | |
| `0x02` | PEACE | |
| `0x04` | ALLY | DOS `13b0` often discusses bit `0x40` for alliance chrome — do not conflate with MET |
| `0x40` | MET | Meet / known |

## `6d8e` §4 vs opportunistic `5bfb`

```
euro_nation_turn (6d8e)
  §4 treaty timers: 0a38 read + decrement peer timers; peaceful Indian drift
  plan 5d04 / 0342 / 0a60
  [opportunistic] 10ec → 13b0 (ally −25g + timer≥8) → declare_war_ctx (thin 153e + status);
                  at-war upkeep + privateer prize; near-parity → make_peace_ctx;
                  ally foreign aid + FA gift (thin 3f41)
  act 5b66 — combat may declare_war
```

| Symbol | Thunk | Role |
|--------|-------|------|
| `FUN_5bfb_10ec` | `2a1f_067a` | Euro A↔B war/ally eligibility by military balance |
| `FUN_5bfb_13b0` | `2a1f_065e` | Form or break alliance |
| `FUN_5bfb_153e` | `2a1f_05fc` | Large war-declare body (~1112) — thin gold+tax+upkeep |
| `FUN_5bfb_0000` / `00f8` / `312e` | census / rank / combat factor | Score stand-ins |
| `FUN_5bfb_102a` / `1092` / `0182` | dialogs | thin `ctx->status` **Done**; widgets **OPEN** (unpark #1 / #5) |
| `FUN_3f41_*` | FA advisor | **PARKED** (thin ally-aid + FA gift; dialog UI parked) |

### Thin `153e` war sting (Linux)

On first `ai_diplo_declare_war` (not already at war):

- Drain **100** gold from `nation[a].gold` and `nation[b].gold` (floor 0)
- Bump each side's `nation[].tax_rate` by **+1**, capped at **75** (same ceiling as king tax path)
- **−5** on each of `nation[].relation_by_indian[0..7]` for both warring Euros (Indians dislike Euro×Euro war; scalar via `ai_diplo_indian_relation_delta`, clamp 0..255)
- OR **Furs** into both nations' `nation[].boycott_bitmap` — cargo index **4**, bit `(1u << 4)` / `0x0010`. Stand-in for wartime trade embargo (Europe screen freezes that cargo). Distinct from king refuse **Sugar** bit1 (`ai_king`). Fuller per-rival `153e` trade body **OPEN** (unpark #5)
- WAR / PEACE / ALLY / MET flag writes unchanged
- Relation summary still via mirror (`nation_relation` → −50 while at war)
- Re-declare does **not** re-sting gold, re-bump tax, re-hit Indian relations, or re-OR the embargo bit (OR is idempotent; gated with other first-declare effects)

Ongoing (in `ai_diplo_euro_balance`, while already at war with a peer):

- If `nation[nation_id].gold > 0`, drain **5** gold (floor 0) once per war peer visited
- Thin privateer prize (separate from upkeep): once per war peer, transfer **8** gold from the richer treasury of the pair to the poorer when donor gold **≥ 8** (no-op if equal). If `ctx->units` is null → treasury-only stand-in; if units are present → only when **this** nation has any sea unit. Full privateer unit spawn **PARKED**
- No new declare / ally logic for that peer that turn

Embargo lift (thin):

- On `ai_diplo_make_peace` or `ai_diplo_form_alliance` (both clear WAR): clear Furs bit on each side that has **no remaining** Euro×Euro war
- Raw PEACE-only writes (clear WAR without those APIs) do **not** lift; Jakob Fugger / FF boycott forgive may clear bits later — full lift chrome **PARKED**

### Thin make-peace (Linux)

`ai_diplo_make_peace(col1, a, b)` — dedicated PEACE path (not ally):

- Clear WAR both directions; OR PEACE|MET
- Lift Furs+Tools embargo via the shared helper when a nation has no remaining Euro wars
- **No gold cost** (war sting + upkeep already drained treasury; optional 10g each not used)
- Idempotent if already peaceful (WAR clear + PEACE|MET + lift check)
- Full `153e` peace dialog widgets (`102a`/`1092`) **OPEN** (unpark #5); thin status via `_ctx`

`ai_diplo_euro_balance` at-war peer visit: after upkeep, if military scores are in the ally-eligible near-parity band (`self>10`, `other>10`, `|self−other|<15`) and RNG `1/30`, call `make_peace_ctx` (status when human involved). No low-gold / long-war gates in this thin pass.

### Thin war/peace status chrome (Linux)

Contact/King pattern — thin `ctx->status` stand-in for `102a`/`1092` (widgets **OPEN**):

- `ai_diplo_declare_war_ctx(ctx, a, b)` → `declare_war` then, on first declare, if human is a party: `"War declared with %s"`
- `ai_diplo_make_peace_ctx(ctx, a, b)` → `make_peace` then, if was at war and human is a party: `"Peace concluded with %s"`
- `%s` = peer `player.country_name` when non-empty, else `"rival"`
- Existing `declare_war` / `make_peace` unchanged (AI callers stay status-free)
- `euro_balance` RNG war/peace uses the `_ctx` wrappers

**OPEN (unpark #5):** full multi-line `102a`/`1092` dialog widgets; FA `3f41`, order clear
`12d0` deep, privateer units, exact `−0x77c4` still PARKED. Score/trade deepen + thin
status chrome **Done** this pass.

### Thin alliance treasury + treaty timer (Linux)

On `ai_diplo_form_alliance` (Euro×Euro):

- Clears WAR then lifts Furs embargo if that nation has no other Euro wars (same helper as make_peace)
- Each side pays **25** gold if able (floor 0)
- If either direction's treaty timer (`unknown26[peer]`) is **0**, set it to **≥8** (exactly 8); live timers left alone
- Flags still clear WAR and set ALLY|PEACE|MET as before
- Full `13b0` gold/score gates **PARKED**

### Thin break-alliance trust penalty (Linux)

On `ai_diplo_break_alliance` when the pair **was** allied:

- Each side loses **20** gold (floor 0) — chosen over −1 `tax_rate` so trust loss stays on the treasury path (war already owns tax bump)
- Re-break when not allied does **not** re-penalize
- Timer-expiry and `euro_balance` RNG break both go through this path

### Thin FA / ally foreign aid (Linux)

Stand-in for Foreign Affairs ally-aid chrome; full `FUN_3f41_*` body + dialogs **PARKED**.

In `ai_diplo_euro_balance`, once per allied peer visit (before break check):

- If allied with peer, this nation's gold **≥ 50**, and peer gold **<** this nation's gold **/ 2**
- Transfer **10** gold from this nation to the ally
- At-war peers skip (upkeep-only path); no aid when donor below 50 or peer already ≥ half

### Thin FA goodwill gift (Linux)

Separate from ally-aid; still a thin `3f41` stand-in (FA dialog UI **PARKED**).

Exported `ai_diplo_fa_gift(col1, from, to)`:

- If donor gold **≥ 100** and peer gold **<** donor gold **× 2**, transfer **15** gold from→to
- Both treaty timers (`unknown26[peer]`) **+2** (saturate 255)
- No-op when donor below 100, peer already ≥ donor×2, or bad nation ids

Wired from `ai_diplo_euro_balance` (after ally-aid, before break check):

- When ALLY and this nation's treaty timer toward peer is **1** (expiring) → call `ai_diplo_fa_gift`
- Goodwill refresh even if peer is not "poor" (unlike the 10g aid path); aid and gift stay independent

### Thin peaceful Indian relation drift (Linux)

Called at end of `ai_diplo_treaty_timers` (6d8e §4 path):

- If the Euro nation is at war with **any** other Euro → no-op
- Else for each of 8 `relation_by_indian[i]`: if `< 160`, **+1** (cap **160**)
- Stand-in only; full Indian×Euro `15b3` bilateral matrix is **OPEN** (unpark #5)

### Thin Indian×Euro matrix stand-in (Linux)

- `ai_diplo_indian_read` / `ai_diplo_indian_at_war` — cell = `relation_by_indian[idx]`;
  at war when relation **< 50**
- On `declare_war`: after the usual −5 Indian hit, if a slot is still **< 40**,
  extra **−10** once per declare (hostile deepen)
- `ai_diplo_euro_balance`: if any Indian slot is at war, **−2** gold harassment
  (once per nation tick); also set `unknown26[8]` sticky to **1** once (idempotent)
- Full Indian×Euro `15b3` bilateral matrix still **OPEN** (unpark #5)

## Linux checklist

1. `ai_diplo_read` / `write` / `or_both` / `clear_both` — peer-correct bytes
2. `ai_diplo_treaty_timers` — decrement; on expiry break ally (trust −20g) or peace tweak; peaceful Indian drift
3. `ai_diplo_euro_balance` — `10ec`/`13b0`-shaped; ally aid + FA gift (timer==1); `declare_war_ctx` → thin `153e` + status; at-war → upkeep + privateer prize + near-parity `make_peace_ctx`; Indian harassment −2g + sticky
4. `ai_diplo_make_peace` / `_ctx` — clear WAR, set PEACE|MET, lift Furs+Tools if no Euro wars; no gold cost; `_ctx` thin status
5. `ai_diplo_declare_war_ctx` — thin `"War declared with …"` when human involved
6. `ai_diplo_form_alliance` — ALLY flags + 25 gold each + treaty timer ≥8 if 0; lift Furs embargo if no Euro wars remain
7. `ai_diplo_break_alliance` — clear ALLY + −20 gold trust penalty if was allied
8. `ai_diplo_fa_gift` — 15g + timer +2 when donor ≥100 and peer < donor×2 (FA UI still PARKED)
9. `ai_diplo_indian_relation_delta` — `4cc6_00f2` / `15dc_00e0` scalar (not full Indian `15b3`)
10. First `declare_war` — Furs `boycott_bitmap` bit4 both sides (wartime embargo stand-in)
11. `ai_diplo_indian_read` / `indian_at_war` — thin matrix cell + harassment + `unknown26[8]` sticky

## PORT DEBT

- **OPEN (unpark #5):** full Indian×Euro bilateral `15b3` matrix (beyond thin read/at_war / drift / war-hit / harassment / sticky); real `102a`/`1092` dialog **widgets** (thin `ctx->status` chrome **Done**)
- **Done this pass:** military score weights; colony-gap Tools embargo; war/peace `_ctx` status lines
- **Still PARKED:** FA `3f41` full body/UI; wartime privateer **unit spawn** / raid path (thin treasury prize only); exact save-field rename for `−0x77c4`; quiet Brave `diplomacy_flags` −10 goldens
