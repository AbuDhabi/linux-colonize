# Indian raid outcomes — thin section-map

Maps settlement-raid / loot clusters for a **reasonable** Linux port in
`ai_contact_indian_raids`. Full `FUN_4d56_4528` (~3073 lines) stays PARKED;
player raid/warn **status chrome thinned**; **widgets** still **OPEN** (unpark #1).

Related: [`indian_contact.md`](indian_contact.md). Peels:
`.context/peel_shards/layer_c_4d56_4528.json`, `layer_b_combat_raid.json`.

## Entry / wiring

| Symbol | Role |
|--------|------|
| `FUN_4d56_4528` | Settlement enter / raid contact (thunk `2a1f_016c`) |
| `FUN_5fef_0f14` | Colony raid loot + tension (goods/building/unit/gold) |
| `FUN_5fef_016c` / `0352` / `0ec0` | Plunder pick / apply outcome / sweep — PARKED deep |
| `FUN_4d56_359c` | Relation-gated kill / warn / displace by RNG — thin: displace Scout, despawn if blocked |
| `FUN_4d56_2154` | Larger action body from `5bfb` — raid-adjacent; not `1b3a` |

## Linux phase arms (`ai_contact_indian_raids`)

1. **Gate** — among Euros with max(`alarm_by_player`, tribe friction) ≥ ~40,
   prefer Indian×Euro **at-war** (`ai_diplo_indian_at_war` / relation `<50`);
   then highest friction; tie-break lower `ai_diplo_indian_relation`
   (very-low hostility). Mid friction: prefer **non-mission** villages —
   mission tribes only raise the gate in the burn band (**≥80**). Cite: fandom
   Alarm — missions slow hostility. Deep Brave escort inside quiet `14fe`
   stays **PARKED** (no unit-follow API; `units_set_goto` is tile goto only).
2. **Adjacent combat** — `units_resolve_land_combat` vs target-nation land unit
3. **Colony approach** — Chebyshev walk toward colony ≤6; among equal distance,
   prefer colony whose warehouse holds muskets (≥5) or horses (≥1) so secondary
   military loot can fire; else prefer tools ≥10 (high-friction secondary −1);
   else prefer higher **silver** stock (GOLD-kind / wealth approach — colony
   precious-metal cargo; nation treasury `@RAIDGOLD` drain stays separate)
4. **Loot outcome** — `@RAID*` kind picker (below); mutates stock / pop / gold.
   Kinds gated on colony stock / Euro gold actually present: empty warehouses
   do not pick STORES/WREAK or fake muskets secondary loot; GOLD only when
   target Euro treasury > 0 (no Indian-nation treasury fiction).
   **STORES** primary: `FUN_5fef_016c`-shaped goods-value pick among lootable
   warehouse cargos (silver/muskets/trade-goods/tools/… ahead of food); horses
   stay on secondary military loot.
5. **Multi-loot (secondary)** — on successful loot (`kind != NOTHING`):
   - military side-steal: −5 muskets stock, else −1 horse stock, else same from
     target-nation unit gear on the colony tile
   - high friction (≥80): also −1 tools (second cargo type beside primary)
6. **Capture** — high band + tiny pop → `colonies_capture` (Indian → abandon)
7. **Friction/alarm escalate** — successful loot (`kind != NOTHING`) → tribe
   `alarm[].friction` and `indian.alarm_by_player` **+2** each (cap **100**).
   **Pocahontas** halves the bump (wiki/fandom half-rate). Cite:
   `docs/fandom_col1994.md` Pocahontas / Alarm.
8. **Hostility tick** — successful loot (`kind != NOTHING`) + friction ≥55 →
   `ai_diplo_indian_relation_delta` (−3, or −5 if ≥80). Deep 4528/2820 PARKED.
   Human target thin status: loot → **"Natives raid your colony."**;
   `NOTHING` (empty warehouse / no lootable stock) → **"Native raiding party
   wiped out."** (`GAME.TXT` `@RAIDNOTHING`). Full `@RAID*` dialog widgets
   **Done** structural (`ai_popup`); DOS body / VGA chrome PARKED.
9. **Scout hostility** (`359c`-shaped) — alarm ≥90 + Scout name adjacent to Brave:
   prefer **displace** 1–2 free land tiles away (direct xy nudge + `AI_MOVE` goto);
   when displaced (not despawned) and status buffer present → human
   **"Scout warned away from village."**; **despawn only if** no free tile
   (**"Natives kill your Scout."**). DOS RNG kill/warn/displace when a flee
   tile exists stays **PARKED** (Marathon2 R6 — warn already; Linux never
   kills if displace succeeds). Dialog warn **widgets** **Done** structural
   (`ai_popup`); VGA chrome PARKED.
10. **PARKED** — deep `FUN_4d56_2820` (~1.4k; thunk `2a1f_044c`) meet/raid
   decision matrix + nested `2aac…311e` haggle (not this post-pulse path;
   Marathon2 R6 keeps PARK — no body port); full `4528` settlement body; ship
   harbor deep; full DOS dialog chrome; `@RAIDBURN` non-Town-Hall **built**
   building loot when stock empty (no safe `colonies_*` destroy API for
   workplace colonists). Status-line chrome for raid/warn is **thinned**. Cite:
   `indian_contact.md` PORT DEBT; `docs/ai_transcription.md` FUN_4d56_2820.

## `@RAID*` message tags (`COLONIZE/GAME.TXT`)

UI strings, not numeric tables. Linux uses the **kind enum** to pick loot:

| Tag | Kind | Linux loot stand-in |
|-----|------|---------------------|
| `@RAIDNOTHING` | NOTHING | No stock change; thin status **"Native raiding party wiped out."** (`GAME.TXT`) |
| `@RAIDWREAK` | WREAK | Multi: food + tools + friction bump |
| `@RAIDSTORES` | STORES | Decrement highest-value lootable cargo stock |
| `@RAIDBURN` | BURN | Kind gated on construction **or** lumber stock; clear production / drain lumber; non-Town-Hall built-building loot **PARKED** (no safe destroy API) |
| `@RAIDSCALP` | SCALP | Population −1 if pop > 1 |
| `@RAIDSHIP` | SHIP | Coastal harbor: damage nearby Euro ship MP/HP stub |
| `@RAIDGOLD` | GOLD | Nation gold −N (treasury raid) |

## Exit criteria for deeper extract

- Sectioned `4528` with threat / combat / loot / dialog clusters named
- `5fef_0f14` line-faithful goods picker
- Status chrome **thinned**; dialog **widgets** still **OPEN** (unpark #1)
- Full `4528` / `5fef` line-faithful bodies still PARKED
