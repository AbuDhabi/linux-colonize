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
  (still true 2026-08-14 — see the `−0x77c4` note further down for what
  this blocks: `FUN_4cc6_0092`'s Indian-nation-elimination handler; full
  map of this table now in [`indian_euro_23000_matrix.md`](indian_euro_23000_matrix.md))

Linux Euro×Euro stand-in (316-byte / `0x13c` nation record):

| Slot | Use |
|------|-----|
| `nation[a].euro_relation[b]` | **Peer diplo flags** (DS −0x77c4 / `FUN_15b3_*`) — WAR/PEACE/ALLY/MET |
| `nation[a].unknown26[0..3]` | Treaty timers toward peer (6d8e §4) — Linux stand-in |
| `nation[a].unknown26[8]` | Indian hostility sticky (`0` clear / `1` at-war / `2` very-low deepen) |
| `nation[a].unknown26[9]` | Wartime Privateer spawn mask (`bit peer` = commissioned once this war) |

**2026-08-10:** Flags moved off `unknown26[4..7]`. Those save bytes are not
`15b3` peer flags — reading them as WAR false-commissioned Privateers on
seed-100 TURN1→2 while `euro_relation[]` stayed zero (peace).

### Bit constants (DOS-derived 2026-08-27, T1.19 — Linux `ai_diplo.h` matches)

| Bit | Name | DOS writers |
|-----|------|-------|
| `0x01` | WAR_INTENT | `521d_6d8e` planner (1-in-4 while `0x08` up, cooldown 0); `465b` clears after the attack |
| `0x02` | WAR | attack sites `465b_0000`/`5fef_1b0e`/`684c_08c0`/`6cb2_24b8`; `5bfb_13b0` paid `@SMITE*`; cleared `43f7_0108` (`0xb`), `3844_0442` (`0xbb`) |
| `0x04` | ALLY (Linux-only on Euro pairs) | DOS uses `0x04` only on Indian pairs: "attack this village?" confirmed (`465b` CHOICE `0x13ad`), cleared by `4cc6_00f2` on cooling |
| `0x10` | crown-arms | `38fd_5930` |
| `0x20` | MET | `5bfb_022e` first contact, `5bfb_3180`, `43f7_0108` (`0x60` toward human + REF) |
| `0x40` | PEACE | `5bfb_0182`, `13b0` peace branch, `3844_0442`; cleared at every attack site |

Real saves carry `00/20/22/60/a0/e0/e2/e8`; bits are directional. `13b0` is the
AI-initiated treaty sign/cancel (`@SIGNTREATY`/`@CANCELTREATY`/`@DECLAREWAR`),
not an alliance offer; the paid `@SMITE*` war-hire lives in `153e`'s FA
negotiation. See `ai_port_plan.md` T1.20.

## `6d8e` §4 vs opportunistic `5bfb`

```
euro_nation_turn (6d8e)
  §4 treaty timers: 0a38 read + decrement peer timers; peaceful Indian drift
  plan 5d04 / 0342 / 0a60
  [opportunistic] 10ec → 13b0 (ally −25g + timer≥8) → declare_war_ctx (thin 153e + status);
                  at-war: Franklin → always peace (skip upkeep/privateer);
                  else upkeep + privateer spawn-only|PARK 8g; war-fatigue → make_peace_ctx;
                  ally foreign aid + FA gift (thin 3f41)
  act 5b66 — combat may declare_war (Franklin pair no-op)
```

| Symbol | Thunk | Role |
|--------|-------|------|
| `FUN_5bfb_10ec` | `2a1f_067a` | Euro A↔B war/ally eligibility by military balance |
| `FUN_5bfb_13b0` | `2a1f_065e` | Form or break alliance |
| `FUN_5bfb_153e` | `2a1f_05fc` | Large war-declare body (~1112, size estimate confirmed accurate) — thin gold+tax+upkeep; **full clean recovery 2026-08-14**, [`euro_diplo_153e_full.md`](euro_diplo_153e_full.md) — real overlap with the deep G-table + `0a60` goal-engine tables, not yet section-mapped or ported |
| `FUN_5bfb_0000` / `00f8` / `312e` | census / rank / combat factor | Score stand-ins |
| `FUN_5bfb_102a` / `1092` / `0182` | dialogs | thin `ctx->status` **Done**; widgets **OPEN** (unpark #1 / #5) |
| `FUN_3f41_*` | FA advisor | **PARKED** (R15: no further thin gap — ally-aid + FA gift only; full F2–F9 report bodies / dialog UI stay parked) |

### Benjamin Franklin NW peace (Linux)

Cite: `docs/fandom_col1994.md` — King’s European wars no longer affect New World
relations; Europeans in the New World always offer peace in negotiations.
Gate: `founding_fathers_franklin_keeps_nw_peace` (either peer owns FF).

- `ai_diplo_declare_war` — **no-op** when either Euro in the pair owns Franklin
  (blocks sting / tax / Indian war-hit / embargo / fatigue seed). King or
  opportunistic callers share this gate so Euro wars cannot poison NW peers.
- `ai_diplo_euro_balance` — skip `10ec` declare pressure against Franklin pairs;
  if already at war → always offer/conclude peace (`make_peace_ctx` / AI→human
  CHOICE) and **skip** upkeep + privateer for that peer (negotiations stay
  peaceful). Elect via `founding_fathers_tick` clears Euro×Euro WAR.
- FA `3f41` full UI stays **PARKED**. No gold fiction.

### Thin `153e` war sting (Linux)

On first `ai_diplo_declare_war` (not already at war; Franklin pair already returned):

- Drain **100** gold from `nation[a].gold` and `nation[b].gold` (floor 0)
- Bump each side's `nation[].tax_rate` by **+1**, capped at **75** (same ceiling as king tax path)
- **−5** on each of `nation[].relation_by_indian[0..7]` for both warring Euros (Indians dislike Euro×Euro war; scalar via `ai_diplo_indian_relation_delta`, clamp **0..255** — war −5 / deepen −10 must not underflow)
- OR **all 16** `@CARGO` cargos into both nations' `nation[].boycott_bitmap` — **Food** (idx **0**), **Sugar** (idx **1**, same bit1 as king refuse / `ai_king` / `king_ref.md`), **Tobacco** (idx **2**), **Cotton** (idx **3**, `COLONIZE_CARGO_COTTON`; R11 leftover), **Furs** (idx **4**), **Lumber** (idx **5**), **Ore** (idx **6**), **Silver** (idx **7**), **Horses** (idx **8**), **Rum** (idx **9**), **Cigars** (idx **10**), **Cloth** (idx **11**), **Coats** (idx **12**), **Trade Goods** (idx **13**), **Tools** (idx **14**), **Muskets** (idx **15**) (`colony.h` / NAMES.TXT). Stand-in for wartime trade embargo (Europe screen freezes those cargos). Fuller per-rival `153e` trade body **OPEN** (unpark #5)
- WAR / PEACE / ALLY / MET flag writes unchanged
- Relation summary still via mirror (`nation_relation` → −50 while at war)
- Re-declare does **not** re-sting gold, re-bump tax, re-hit Indian relations, or re-OR the embargo bit (OR is idempotent; gated with other first-declare effects)
- **War fatigue:** if peer treaty timer (`unknown26[peer]`) is **0**, seed it to **8** both dirs (live timers left alone). `euro_balance` near-parity peace requires `timer==0` (war aged) before the rare `1/30` `make_peace_ctx`. No invented gold.
- **Colony-gap deepen:** if `|colony_count_a − colony_count_b| ≥ 2`, drain **25** gold from the richer treasury (floor 0). Tools is already OR'd on every first declare (R10); gap no longer gates Tools.

Ongoing (in `ai_diplo_euro_balance`, while already at war with a peer):

- **Franklin pair first:** always offer/conclude peace; **continue** (no upkeep /
  privateer for that peer). See Benjamin Franklin section above.
- If `nation[nation_id].gold > 0`, drain **5** gold (floor 0) once per war peer visited
- Human status once per tick when upkeep drains and human is the actor: `"War upkeep costs gold."` (later privateer / peace may overwrite). FA UI **PARKED**
- Wartime **Privateer unit spawn** (before prize): when `ctx->units` set and `units_find_type("Privateer")` exists, spawn once per war peer via `units_spawn_allow_stack` on **hunt-ready** coastal water by own colony (New World water, not `x|y≥200`), else stack on own New World sea unit (skip Europe-dock stacks), else Europe `(236,236)`. Read-only check refuses bad New World tiles before arming `unknown26[9]`. Gate: `unknown26[9]` bit for peer (clear on `make_peace` / alliance that clears WAR). Human status: `"Privateer commissioned against %s"` + INFO OK (once per spawn; `unknown26[9]` blocks re-spam). Cite: Europe Privateer purchase; fandom Drake; `euro_unit_act` §2b (`ai_euro` naval hunt needs `!in_europe`). **Spawn-only when units set** — real cargo-raid loot is `units_plunder_ship_holds` in naval combat (`FUN_5fef_016c`-shaped; not wired in diplo).
- **PARKED** privateer treasury prize (**null-units only**): once per war peer, transfer **8** gold from the richer treasury of the pair to the poorer when donor gold **≥ 8** (no-op if equal). Accuracy debt when `ctx->units` is null — **do not invent a different gold rate** (naval hold plunder is live). Human status when prize fires and human is a party: `"Privateer prize from %s"`. With `ctx->units` set → **no** treasury prize (spawn + naval combat plunder path only).
- No new declare / ally logic for that peer that turn

Embargo lift (thin):

- On `ai_diplo_make_peace` or `ai_diplo_form_alliance` (both clear WAR): clear **all 16** wartime `@CARGO` bits (Food…Cotton…Tools…Muskets) on each side that has **no remaining** Euro×Euro war (shared lift mask)
- Tools bit is OR'd on every first declare (with the other wartime cargos); Cotton is the R11 leftover that completes the 16-bit mask; peace/alliance must lift all sixteen with the same gate
- Sugar lift may clear a lingering king refuse Sugar bit while `unknown46[2]` still holds tax refuse (thin shared-bitmap stand-in)
- Raw PEACE-only writes (clear WAR without those APIs) do **not** lift; Jakob Fugger / FF boycott forgive may clear bits later — full lift chrome **PARKED**
- Privateer prize is WAR-gated in `euro_balance` and **null-units only** — `make_peace` stops further prizes (no dedicated prize-clear flag); with units → spawn-only

### Thin make-peace (Linux)

`ai_diplo_make_peace(col1, a, b)` — dedicated PEACE path (not ally):

- Clear WAR both directions; OR PEACE|MET
- Lift all 16 wartime `@CARGO` boycotts via the shared helper when a nation has no remaining Euro wars
- **No gold cost** (war sting + upkeep already drained treasury; optional 10g each not used)
- When sticky was **at-war (==1)** on either side and the pair **was** at war: nudge Indian peace feeler once after WAR clear (existing feeler path; restores improve-relations Euro war blocked), then sync sticky. **sticky==2** refuses feeler (self-gated inside `ai_diplo_indian_peace_feeler`)
- Idempotent if already peaceful (WAR clear + PEACE|MET + lift check; no second feeler)
- Full `153e` peace dialog widgets (`102a`/`1092`) **OPEN** (unpark #5); thin status via `_ctx`

`ai_diplo_euro_balance` at-war peer visit: after upkeep, if military scores are in the ally-eligible near-parity band (`self>10`, `other>10`, `|self−other|<15`) **and** peer treaty timer is **0** (war fatigue / aged) and RNG `1/30`, call `make_peace_ctx` (status when human involved: `"Peace concluded with %s"` / Tools lift chrome). No low-gold / invented tribute.

### Thin war/peace status chrome (Linux)

Contact/King pattern — thin `ctx->status` stand-in for `102a`/`1092` (widgets **OPEN**):

- `ai_diplo_declare_war_ctx(ctx, a, b)` → `declare_war` then, on first declare, if human is a party: `"War declared with %s"`; if Sugar/Tobacco/Tools newly OR'd on the human nation: prefer `"Sugar/Tobacco/Tools boycott imposed."`; else if Sugar/Tobacco newly OR'd (Tools already present): prefer `"Sugar/Tobacco boycott imposed."`; else if any other wartime `@CARGO` newly OR'd: prefer `"%s boycott imposed."` naming the first new cargo by index (Food…Muskets; colony.h / NAMES.TXT); else if Indian sticky newly rose from the −5 war-hit: prefer `"Natives grow hostile."`
- `ai_diplo_make_peace_ctx(ctx, a, b)` → `make_peace` then, if was at war and human is a party: `"Peace concluded with %s"`; if Tools bit cleared on the human nation: prefer `"Tools embargo lifted."` (war-fatigue path uses this `_ctx`)
- `ai_diplo_break_alliance_ctx(ctx, a, b)` → `break_alliance` then, if was allied and human is a party: `"Alliance broken with %s"`; if Indian sticky newly rose from the −5 break hit: prefer `"Natives grow hostile."` (wired from `euro_balance` 13b0 break + treaty-timer expiry)
- `ai_diplo_form_alliance_ctx(ctx, a, b)` → `form_alliance` then, if human is a party and pair was not already allied: `"Alliance formed with %s"`; if 25g alliance cost drained human treasury: prefer `"Alliance with %s costs gold."` (`euro_balance` 13b0 form uses this `_ctx`)
- FA gift / longevity (timer==1, sticky≠2): gift success → `"Alliance with %s strengthened."`; longevity-only → `"Alliance with %s holds."`
- `%s` = peer `player.country_name` when non-empty, else `"rival"`
- Existing `declare_war` / `make_peace` / `form_alliance` / `break_alliance` unchanged (AI callers stay status-free)
- `euro_balance` RNG war/peace/form/break uses the `_ctx` wrappers
- **AI popups (map wood OK / CHOICE):** when `ctx->ai_popups` is set, human-facing
  status lines **also** enqueue OK (`DIPLO_WAR` / `PEACE` / `ALLIANCE` / `BREAK` /
  `BOYCOTT` / `INFO`). Alliance offer to human peer enqueues CHOICE Accept/Refuse;
  Accept → `form_alliance_ctx` follow-up OK `"Alliance formed with %s"` (or
  gold-drain chrome). War-fatigue AI→human peace offer enqueues CHOICE
  Accept/Refuse → `make_peace_ctx` on Accept; Refuse → status
  `"Peace refused with %s"` + follow-up OK (`ai_diplo_apply_popup_result`).
  War OK both human-as-a / human-as-b once (`!already` re-declare gate).
  AI→human `10ec` war eligibility may enqueue CHOICE Accept/Refuse
  (`DIPLO_WAR`) before `declare_war_ctx` (same pattern as alliance/peace);
  Refuse → `"War refused with %s"` + follow-up OK (Marathon2 R6).
  AI→human `13b0` break may enqueue CHOICE Accept/Refuse (`DIPLO_BREAK`)
  before `break_alliance_ctx`; Refuse → `"Alliance break refused with %s"` + OK
  (timer-expiry break stays automatic). Alliance Accept also bumps treaty
  timer to **≥8** when was 0 (same `form_alliance` path). Native sticky
  deepen / remain-hostile status enqueues INFO OK. Privateer commission /
  prize status also enqueues INFO OK (once per spawn / prize). FA `3f41` full
  report UI still **PARKED**; thin gift/longevity OK uses
  `AI_POPUP_TAG_DIPLO_FA` + title `"Foreign Affairs"`. Cite
  `FUN_15b3` / `FUN_5bfb`.

**Done (structural unpark #5):** AI popup OK/CHOICE enqueue + wartime Privateer
**unit spawn** + score/trade deepen + thin status chrome. **Still PARKED:** full
multi-line VGA `102a`/`1092` dialog widgets; FA `3f41` full UI. Order-clear
`12d0` — **resolved 2026-08-19** (`euro_diplo_153e_full.md`): cancels
roam/reevaluate orders (state 5/6→0) on the other nation's combat-capable
land units adjacent to your settlements, fired both directions on alliance
form and once from `153e`'s outcome table; small, not yet ported (would need
a unit-loop + `euro_settlement_owner` adjacency check, no new struct fields).

**`−0x77c4` resolved (2026-08-14, this list wasn't updated when it happened
— it's actually already mapped above, §"`15b3` bilateral bytes"):**
`FUN_15b3_0004`/`0032` is dual-mode — Euro owner (`<4`) addresses
`*(peer + nation*0x13c − 0x77c4)`, exactly `nation[a].euro_relation[b]`
(confirmed, already the live Linux field, `ai_diplo_read`/`write`'s
backing store); Indian owner (`≥4`) addresses a **separate** table at
`*(peer + nation*0x4e + 23000)` — that Indian branch is the real remaining
gap, already correctly called out below as full-matrix PORT DEBT.
`ai_diplo_or_both`/`clear_both`/`read`/`write` currently hard-reject any
`nation_a`/`nation_b ≥ 4` (checked in `ai_diplo.c`), so callers that need
the Indian branch (e.g. `FUN_4cc6_0092`, Indian-nation-type elimination
paralleling the already-ported `FUN_43f7_0108`) can't be safely ported by
just widening the existing bounds check — the `23000`/`0x4e` table itself
has no Linux mirror yet, and it's unclear which of that record's 78 bytes
are meaningful vs. reserved. Real next-step candidate, but a new-struct
project (comparable to the `0x8d4a` settlement-record pass), not a quick
follow-up — flagging rather than guessing at the table layout.

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
- **−5** on each of `nation[].relation_by_indian[0..7]` both sides, then sync Indian hostility sticky (same scalar as Euro×Euro war hit; no very-low extra). When relations were near the at-war floor, sticky rises **0→1** (Indians wary of Euro treachery; fandom / war-hit stand-in)
- Re-break when not allied does **not** re-penalize or re-hit Indians
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
- If gift gold gates fail (timer still **1**) → longevity **+1** both treaty timers (**no** second gold transfer)
- Goodwill refresh even if peer is not "poor" (unlike the 10g aid path); aid and gift stay independent

### Thin peaceful Indian relation drift (Linux)

Called at end of `ai_diplo_treaty_timers` (6d8e §4 path):

- If the Euro nation is at war with **any** other Euro → no-op
- Else for each of 8 `relation_by_indian[i]`: if `< 160`, **+1** (cap **160**)
- Stand-in only; full Indian×Euro `15b3` bilateral matrix is **OPEN** (unpark #5)

### Thin Indian×Euro matrix stand-in (Linux)

Gates reuse contact conventions (not new combat numbers):

| Gate | Value | Source |
|------|------:|--------|
| `indian_at_war` | relation **< 50** | contact mission block `alarm≥50` inverted |
| very-low deepen | relation **< 40** | contact peaceful-gift friction **< 40** inverted |
| content floor | **100** | mid-content; drift still climbs to 160 |
| feeler heal | **+2** | contact trade already bumps relation +2 |

API / behavior:

- `ai_diplo_indian_read` / `ai_diplo_indian_relation` (read-only `indian_nation` 4..11) /
  `ai_diplo_indian_at_war` / `ai_diplo_indian_any_at_war`
- `ai_diplo_indian_hostility_sticky` / `ai_diplo_indian_hostility_sync` —
  `unknown26[8]`: **0** clear, **1** any at-war, **2** any very-low while at-war
- On `declare_war`: after −5 Indian hit (+ extra −10 if still **< 40**), sync sticky both sides
- `ai_diplo_euro_balance` Indian matrix arm (once per nation tick):
  1. **Peace feeler** — if Euro at peace with all Euro peers (`!ai_diplo_at_war_with_any`)
     **and sticky ≠ 2**, each mid/high slot (`≥ 50` and `< 100`) heals **+2** toward
     content floor 100 (fandom peace → gifts / improve relations; **no gold**)
  2. **Sticky→pressure** — sticky **== 2** skips feeler; human status
     `"Natives remain hostile."` (fandom alarmed refuse gifts; no gold fiction)
  3. **Sticky sync** — set / clear / deepen from matrix
  4. **Harassment** −2 gold if any `indian_at_war` (skip when gold already 0;
     floor once to 0 when gold < 2 — no invent-below-zero more than once per
     balance tick)
  5. Human status chrome: sticky 0→nonzero → `"Natives grow hostile."`;
     sticky nonzero→0 → `"Native tensions ease."`; sticky stays clear and
     feeler heals ≥1 mid-band slot → `"Native relations improve."` (widgets **OPEN**)
- `euro_balance` **13b0** ally form: sticky **== 2** refuses **new** alliances this
  balance (existing ALLY kept); when RNG would form and human is actor, status
  `"Native unrest precludes new alliances."` (fandom alarmed refuse diplomacy path)
- sticky **== 2** also skips **FA gift** to allied peers (no 15g transfer); longevity
  timer+1 still applies when timer==1 (no gold). Ally-aid 10g unchanged.

## Linux checklist

1. `ai_diplo_read` / `write` / `or_both` / `clear_both` — peer-correct bytes
2. `ai_diplo_treaty_timers` — decrement; on expiry break ally (trust −20g) or peace tweak; peaceful Indian drift
3. `ai_diplo_euro_balance` — `10ec`/`13b0`-shaped; ally aid + FA gift / longevity (timer==1); `declare_war_ctx` → thin `153e` + status; at-war → Franklin peace (skip upkeep/privateer) else upkeep + Privateer spawn-only / PARK 8g prize + war-fatigue (`timer==0`) near-parity `make_peace_ctx`; Indian feeler + sticky→pressure + harassment
4. `ai_diplo_make_peace` / `_ctx` — clear WAR, set PEACE|MET, lift Furs+Tobacco+Sugar+Rum+Cigars+Tools if no Euro wars; no gold cost; `_ctx` thin status (+ Tools lift chrome)
5. `ai_diplo_declare_war` / `_ctx` — Franklin pair → no-op (no sting); else thin `"War declared with …"` / boycott chrome when human involved
6. `ai_diplo_form_alliance` / `_ctx` — ALLY flags + 25 gold each + treaty timer ≥8 if 0; lift Horses+Muskets+…+Tools if no Euro wars remain; `_ctx` statuses `"Alliance formed with %s"` on first form; prefer `"Alliance with %s costs gold."` when human treasury drains
7. `ai_diplo_break_alliance` — clear ALLY + −20 gold trust penalty + Indian −5/sticky sync if was allied
8. `ai_diplo_fa_gift` — 15g + timer +2 when donor ≥100 and peer < donor×2 (FA UI still PARKED); else longevity +1; sticky2 skips gift
9. `ai_diplo_indian_alarm_delta` / `ai_diplo_indian_alarm` — `4cc6_00f2` / `15dc_00e0` on `indian.alarm_by_player` (2026-08-27; `ai_diplo_indian_relation*` = 100−alarm view)
10. First `declare_war` — all 16 `@CARGO` `boycott_bitmap` bits both sides (incl. Cotton); colony-gap ≥2 → extra 25g rich sting; fatigue timer seed 8 if 0
11. Indian matrix helpers — read / relation / at_war / any_at_war / sticky sync (0/1/2) + feeler skip on sticky2 (self-gated) + sticky2 refuse new alliances + sticky2 skip FA gift + human status + harassment gold floor + war −5 relation floor 0
12. `ai_diplo_at_war_with` / `ai_diplo_at_war_with_any` — war-turn helpers (pair alias + any-Euro gate for feeler/drift/lift)

## PORT DEBT

- **Done (structural unpark #5):** thin Indian×Euro `15b3` matrix helpers
  (read/at_war/drift/feeler/war-hit/harassment/sticky) + `ai_popup` war/peace/
  alliance widgets + Privateer spawn. Deeper matrix / VGA widgets remain PARKED
  (see leftovers below).
- **Done this pass:** sticky→pressure + ally longevity + Tools lift parity + `indian_relation` getter
- **Done R3:** sticky2 refuse new alliances; Sugar wartime boycott (king bit1) + lift; export `at_war_with` / `at_war_with_any`; feeler gated on `!any_euro_war`
- **Done R4:** Rum+Cigars wartime boycott set/lift; sticky2 skip FA gift; war-declare boycott status names Sugar/Tobacco/Tools
- **Done R6:** Ore+Silver wartime boycott set/lift; FA gift/longevity human status; war-fatigue Peace status smoke; Indian −5 war-hit status when sticky rises
- **Done R7:** Food+Trade Goods wartime boycott set/lift; make_peace restores Indian feeler when sticky elevated; privateer prize human status
- **Done R8:** Lumber wartime boycott set/lift; make_peace stops privateer prize (smoke); Indian feeler human status `"Native relations improve."`
- **Done R9:** Horses+Muskets wartime boycott set/lift; sticky==2 feeler self-gate (make_peace + matrix); `form_alliance_ctx` gold-drain status
- **Done R10:** Tools wartime boycott always (`COLONIZE_CARGO_TOOLS`) + lift; war upkeep human status once/tick; break_alliance raises Indian sticky (−5 + sync) + smoke; peace feeler mid-band already smoked (R8)
- **Done R11:** Cotton wartime boycott leftover (`COLONIZE_CARGO_COTTON`) set/lift — full 16-bit wartime mask; make_peace smoke clears full wartime bitmap; war −5 Indian relation floor at 0; sticky2 FA gift skip already smoked (R4)
- **Done R12:** declare status names first newly boycotted `@CARGO` when Sugar/Tobacco/Tools chrome quiet; alliance timer≥8 smoke; privateer prize peace-stop + sticky −5 sync already complete
- **Done R13:** war-fatigue human chrome when either party (peer smoke + Peace concluded when Tools clear); sticky==2 refuse-alliance status smoke; treaty-timer expiry break human status; FA gift gold transfer already smoked
- **Done R14:** full wartime boycott mask declare OR + make_peace clear already smoked; `form_alliance_ctx` success chrome `"Alliance formed with %s"` (gold-drain preferred)
- **Done R15 (thin final):** no code gap — alliance-formed status smoke already present (R14 zero-gold path); FA `3f41` full body/UI confirmed **PARKED** (thin ally-aid 10g + FA gift 15g / longevity only; Accuracy bar: FA UI parked, no invented chrome)
- **Done popup marathon R3 (thin final):** Alliance Accept CHOICE → follow-up OK `"Alliance formed with %s"` smoke; privateer prize OK enqueue smoke; FA `3f41` full body/UI stays **PARKED** (cite `FUN_15b3` / `FUN_5bfb`; no F2–F9 report chrome)
- **Done Marathon2 R1:** wartime Privateer **unit spawn** once/war peer (`unknown26[9]` + coast/Europe); treasury prize kept; thin FA report OK title `"Foreign Affairs"`
- **Done Marathon2 R3:** Privateer spawn prefers hunt-ready New World water (skip Europe-dock stacks); smoke asserts water/`!in_europe`; FA OK uses `DIPLO_FA` + `"Foreign Affairs"`; AI→human war declare CHOICE Accept/Refuse
- **Done Marathon2 R5:** peace CHOICE Refuse status + follow-up OK; Privateer once/war smoke asserts `unknown26[9]` blocks second; AI→human break-alliance CHOICE Accept/Refuse; FA `3f41` full UI stays **PARKED**
- **Done Marathon2 R6:** war CHOICE Refuse status + follow-up OK; Alliance Accept treaty timer ≥8 smoke; native sticky deepen INFO OK enqueue smoke; FA `3f41` full UI stays **PARKED**
- **Done Marathon3 R1:** Benjamin Franklin NW peace gate (`founding_fathers_franklin_keeps_nw_peace`
  via `founding_fathers_nation_has`; `col1_save_init` sets `head.founding_father[i]=-1`):
  `declare_war` no-op when either peer owns FF (blocks sting/war-hit/embargo —
  king Euro wars must not poison NW relations); `euro_balance` skips 10ec declare
  pressure; at-war → always offer/conclude peace; elect clears Euro×Euro WAR.
  Cite: `docs/fandom_col1994.md` Benjamin Franklin. FA `3f41` full UI stays **PARKED**
- **Done Marathon3 R2:** Privateer **spawn-only** when `ctx->units` set (skip PARKED 8g
  treasury prize — accuracy debt / no `FUN_5fef` hold-plunder API; do not invent
  another gold rate); null-units keep 8g stand-in + chrome; Franklin at-war
  `make_peace_ctx` human status `"Peace concluded with %s"` smoked. FA `3f41` full
  UI stays **PARKED**
- **Done Marathon3 R3 (thin final):** no structural diplo gap — embargo chrome,
  sticky deepen INFO, peace refuse, alliance longevity status already smoked;
  defensive smoke adds longevity Foreign Affairs OK (`"Alliance with %s holds."`
  + `DIPLO_FA` / `"Foreign Affairs"`, mirrors gift strengthened OK).
- **Done Marathon3 R4 (doc sync + thin defensive):** `euro_diplo.md` body synced
  for Franklin NW peace + Privateer spawn-only / PARK 8g (null-units only);
  defensive smoke: Franklin at-war peace path skips upkeep + PARK prize (gold
  unchanged). No invented privateer gold. FA `3f41` full UI stays **PARKED**.
- **Done Marathon4 R1 (doc sync + thin defensive):** `DIPLO_FA` tag for FA
  gift/longevity OK; naval hold plunder (`units_plunder_ship_holds` / naval
  combat) documented as real Privateer cargo path; 8g treasury prize **PARKED
  null-units only**; defensive smoke: Privateer commission INFO OK enqueue
  (status string already smoked). **No further thin diplo unpark.**
## FA negotiation screen — one unified mechanic, structure confirmed, function not yet found

**2026-08-14, real gap identified via the "ask the user" method**
(`decomp_inventory.md`): `docs/popups.md` carried a whole cluster of
`GAME.TXT` tags as generic "Partial — thin `DIPLO_FA` or status; full
`3f41` PARKED" boilerplate — but they're **completely unwired**
(confirmed: zero references anywhere in `src/core/*.c`, not even a thin
stand-in), not actually implemented at all. Two rounds of asking the user
directly revealed this is **one single negotiation screen with multiple
entry points**, not several separate mechanics — every offer variant
below is followed by the *same* 4-option response menu ("Go in peace,
{nation} brothers." / "First you must withdraw your forces from our
colonies!" / "How much do you value your worthless lives, heathen
swine?" / "We suggest an alliance.") — confirmed by their literal
identical wording appearing after `@PEACEMANLY`, `@PEACEMEEK`,
`@OLDPEACEMEEK`, `@OLDPEACEMANLY`, and `@PEACEUSA` alike in `GAME.TXT`.

**Two-stage architecture, confirmed by the user (2026-08-14, third round of
asking)**: AI Euro nations, on their own turn, can spontaneously initiate
any of the specific demand/threat/offer events below at the player (not
just player-initiated) — and mere first contact with another European
power also triggers this same flow (`@HELLO*`, below). After the specific
event resolves, **if the pair is still at peace**, it falls through into
the shared general negotiation menu ("Go in peace" / "First you must
withdraw..." / "How much do you value your worthless lives..." / "We
suggest an alliance.") as a *second*, separate step — same menu whether
the player initiated contact or the AI did. So this is genuinely one
coherent encounter-resolution system with many possible opening prompts,
not several independent mechanics.

**Entry points (the initial offer / situation) — all confirmed unwired,
zero references anywhere in `src/core/*.c`:**
- `@HELLOFIRST`/`@HELLOUSA`/`@HELLOAHOY` (sea variant) — first contact
  with another European power (land or sea).
- `@HELLOMEEK`/`@HELLOMANLY` — subsequent-contact greeting variants
  (same MEEK/MANLY tone split as the peace offers below).
- `@WORTHY` — "we propose a demarcation treaty, dividing... into
  perpetual and inviolable spheres of influence. Will you agree?"
  Yes/No — very likely the actual proposal step that *leads to*
  `@PEACEMANLY`/`@PEACEMEEK` on "Yes" (not independently confirmed, but
  the only unaccounted-for "propose peace" trigger found).
- `@PEACEMANLY`/`@PEACEMEEK` — first-time peace negotiation, dividing
  territory ("MANLY" = confident/generous division, "MEEK" = submissive
  division — plausibly tone mirrors relative military strength, not
  confirmed).
- `@OLDPEACEMEEK`/`@OLDPEACEMANLY`/`@PEACEUSA` — already-at-peace
  greeting, same tone split (+`@PEACEUSA` a third, nation-specific
  variant).
- `@PIRACY`/`@PIRACYUSA` — AI complains about the player's privateers off
  their coast, demands withdrawal. "What pirates? We have NEVER condoned
  piracy!" (deny) / "Very well, we shall withdraw our privateers to
  Europe." (comply).
- `@SIEGES`/`@SIEGESUSA` — the *mirror* of "Demand Withdrawal" below: the
  AI demands the *player* withdraw military units near *their* colonies.
  "Our forces protect valid interests and shall stay." (refuse) /
  "Very well, we shall withdraw our forces to Europe." (comply).
- `@HEATHEN`/`@HEATHENUSA` — AI asks the player to jointly attack a named
  native tribe. "Never! The {tribe} are a harmless and peaceful people!"
  / "Yes! Let us teach the {tribe} a lesson!"
- `@APOSTATES`/`@APOSTATESUSA` — AI demands the player cancel a treaty
  with a third nation it considers heretical. "Never! They are our
  friends!" / "Yes! We shall crush them together!"
- `@TRIBUTE`/`@TRIBUTEUSA` — AI threatens to drive the player out (on
  orders from its Crown) but offers to overlook it for a gold bribe.
  "Not a penny..." (refuse) / "We will gladly donate {$N}..." (pay).
- `@WANTSTUFF`/`@WANTSTUFFUSA` — same threat-and-overlook shape as
  `@TRIBUTE`, demanding specific goods instead of gold. "We laugh at your
  puny threats." / "We gladly share {N goods}..."
- `@RID`/`@RIDUSA` — a blunt ultimatum, "leave {colony}/this hemisphere
  immediately... or we shall drive you into the sea" — no response
  options shown in `GAME.TXT` at this tag itself; most likely a
  standalone warning that (like the others) falls through to the shared
  negotiation menu rather than carrying its own choice.
- Adjacent-military "Demand Withdrawal" — the player-initiated direction,
  confirmed by the user, below.

**Demand Withdrawal, player-initiated action — pick an adjacent European
power with military units next to one of your colonies, demand they
withdraw. Four outcomes** (user-confirmed, decisive):
1. **Refuse** (`@NOTWITHDRAW`, "Our forces protect valid interests and
   shall not be moved.") — their units stay put.
2. **Withdraw** (`@WITHDRAW`, "In the interest of peace, we shall
   withdraw our forces.") — their units are teleported back to their own
   colonies (exact mechanics of the teleport, e.g. which colony, not
   confirmed by the user).
3. **No units there** (`@NOTHINGWITHDRAW`, "We have no forces adjacent to
   your colonies.") — a no-op informational response.
4. **Counter-offer** (`@MAYBEWITHDRAW`, "...willing to move them in
   exchange for `{$N}` to cover the cost of demobilization.") — a
   sub-CHOICE: pay the bribe (→ same result as Withdraw) or refuse
   (→ same result as Refuse, "Withdraw or perish, heathen pigs!"/"Oh.
   Never mind then.").

**War-declare outcomes from the shared response menu** (user-confirmed
for the trigger; the tone-pairing below is my own reasoned-but-unconfirmed
reading, not independently verified): picking "How much do you value your
worthless lives, heathen swine?" — a threat, not a plain decline — after
being offered peace triggers a war declaration. User confirms
`@WARMEEK`/`@WARMANLY` fire in exactly this circumstance ("the other Euro
offers you peace, and you refuse"); which of the two fires isn't
independently confirmed by the user ("maybe it's relation-based, IDK"),
but the wording is suggestive: `@WARMANLY` ("You reject our *generous*
offer?... Prepare for WAR!") reads as mirroring a rejected `@PEACEMANLY`-
toned offer, `@WARMEEK` ("Very well, then... Prepare for WAR!") mirroring
a rejected `@PEACEMEEK`-toned one — i.e. the war-declare tone likely
mirrors whichever tone the original peace offer used, not an independent
roll. `@PROVOKE` ("We can no longer tolerate your foul provocations.
Prepare for WAR!") reads as structurally different — no "rejected offer"
framing at all, more likely an independent trigger (accumulated
hostility/provocation crossing a threshold over multiple turns) than a
direct response to a specific dialog choice; user recalls the tag but not
precisely when it fires, consistent with this reading (an AI-initiated,
not player-choice-triggered, declaration).

**Real gap, genuinely not a quick fix, and bigger than it first looked**:
this is not one small mechanic but a whole unimplemented "AI diplomatic
pressure events" system — at least 17 distinct entry-point tags (first
contact, peace offers/greetings at two tone levels, privateer/military
withdrawal demands in both directions, joint-attack-a-tribe requests,
treaty-cancellation demands, gold/goods tribute-or-be-driven-out threats,
a bare ultimatum, and a territory-partition proposal), all funneling into
one shared response menu, all still zero-referenced anywhere in Linux.
The underlying DOS function(s) are unknown. Not part of
`FUN_5bfb_3180`'s already-mapped already-met-adjacency dispatch
(`euro_diplo_3180_full.md` — checked, no "withdraw"/"demobiliz"/"peace"
hits), and most likely part of the Foreign Affairs advisor screen
(`FA`/`3f41` segment, per `docs/popups.md`'s own citation) — a segment
this project has **never recovered a single function from** (`grep` for
`FUN_3f41_` in the canonical decompile: zero hits), unlike everything
else touched this session. Finding it would mean the same from-scratch
overlay-recovery investment as `153e` or `0a60` originally needed, likely
larger given the number of distinct branches — not a quick lookup.
Every formula involved (bribe/tribute amounts, "demobilization cost",
which specific event fires when, the peace-tone selector, the
war-declare dispatch) is completely unknown; no candidate function
traced for any of it.

**Not attempted this pass** — the mechanic's *shape* is now fully
specified (real value, doesn't need re-asking; three separate rounds of
user testimony nailed down the trigger direction, the fall-through
architecture, and the specific event texts), but implementing it
faithfully needs either the real DOS formula (a proper `3f41` recovery
pass — now a well-scoped, worthwhile target given the size of what it'd
unlock) or an explicit decision to ship an approximated version, which
would need the same care as any other "thin first draft" mechanic in
this project. Flagging as the single largest well-specified gap this
session surfaced, worth prioritizing over other `3f41`-adjacent leads
(`12d0`, the FA report UI) if anyone picks up a `3f41` recovery pass —
this is the part of that segment with confirmed, decisive real-world
value already in hand.

- **Still PARKED (leftovers — no thin unpark left):**
  - FA `3f41` full body/UI (F2–F9 report dialogs; thin ally-aid 10g + FA gift
    15g / longevity + `DIPLO_FA` OK only)
  - Privateer 8g treasury prize (**null-units only** when `ctx->units` null;
    real cargo loot is `units_plunder_ship_holds` in naval combat — do not
    invent another gold rate)
  - Full `102a`/`1092` dialog **widgets** (thin `ctx->status` + AI OK/CHOICE Done)
  - Full Indian×Euro bilateral `15b3` matrix beyond thin read/at_war/drift/
    feeler/war-hit/harassment/sticky
  - Exact save-field rename for DS `−0x77c4`
  - Quiet Brave `diplomacy_flags` −10 goldens
  - Order clear `12d0` deep; Jakob Fugger / FF boycott-forgive full lift chrome
