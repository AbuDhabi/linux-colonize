# Indian raid outcomes — thin section-map

Maps settlement-raid / loot clusters for a **reasonable** Linux port in
`ai_contact_indian_raids`. **Correction 2026-08-13**: the "~3073 lines,
PARKED" figure below was from the *corrupted* canonical decompile export
— re-disassembled clean this session via the overlay-addressing project:
`FUN_4d56_4528`'s real body is a self-contained **312 lines**, fully
mapped, and its 9-case dispatch tail resolved as all routing to one
shared 21-byte value-clamp utility (no hidden action tree) — see
[`indian_settlement_4528.md`](indian_settlement_4528.md) "Case-dispatch
tail". Nothing left PARKED in `4528`'s own body. Player raid/warn
**status chrome thinned**; dialog **widgets** **Done** structural (`ai_popup`;
VGA PARKED).

Related: [`indian_contact.md`](indian_contact.md),
[`indian_trade_2820.md`](indian_trade_2820.md).

## Entry / wiring

| Symbol | Role |
|--------|------|
| `FUN_4d56_4528` | Settlement enter / raid contact (thunk `2a1f_016c`) — head: human warn CHOICE + ship abort; **no direct `5fef` calls** |
| `FUN_5fef_0f14` | Colony raid loot + tension (goods/building/unit/gold) — **mapped** [`indian_raid_loot.md`](indian_raid_loot.md) |
| `FUN_5fef_016c` / `0352` / `0ec0` | Plunder pick (**mapped** `016c`) / apply outcome / sweep — `0352`/`0ec0` still PARKED deep |
| `FUN_4d56_359c` | Relation-gated kill / warn / displace by RNG — thin: displace Scout, despawn if blocked |
| `FUN_4d56_2154` | Meet economics scorer from `5bfb_022e` — **mapped** [`indian_meet_scoring_2154.md`](indian_meet_scoring_2154.md); **not** raid |

### `4528` head vs Linux phase arms

DOS head (mapped): bind tribe → relation/friction → human warn strings
`0x1710`…`0x172e` + CHOICE → early `LAB_4d56_4bf2` aborts (ship / cancel).
Linux skips ship/warn VGA and jumps to structural raid phases below (combat /
approach / `@RAID*` loot via `5fef`-shaped helpers).

## Linux phase arms (`ai_contact_indian_raids`)

1. **Gate** — among Euros with max(`alarm_by_player`, tribe friction) ≥ ~40
   (**Spain ≥35** — fandom conquest bias),
   prefer Indian×Euro **at-war** (`ai_diplo_indian_at_war` / relation `<50`);
   then highest friction; tie-break lower `ai_diplo_indian_relation`
   (very-low hostility). Mid friction: prefer **non-mission** villages —
   mission tribes only raise the gate in the burn band (**≥80**). Cite: fandom
   Alarm — missions slow hostility; nation bias (Spanish conquest). **Done (thin):** post-pulse Brave escort via
   `units_follow_unit` — same-nation AI_MOVE/GOTO within MD≤3; lead pick prefers
   goto toward raid-gate Euro colony when known, else nearest-lead. Deep escort
   inside quiet `14fe` still **PARKED**.
2. **Adjacent combat** — `units_resolve_land_combat` vs target-nation land unit;
   on Brave win, snapshot foe `muskets`/`horses` and transfer onto Brave
   (muskets prefer, else horses — GAME.TXT `@INDIANWIN1` / `@INDIANWIN2`);
   human status **"The %s ambush {%nation %unit} near %place!"** (+ **"Muskets/Horses seized
   by %s braves!"** when gear seized) / **"{%nation %unit} {defeat} {%tribe} near %place!"**
   (`@INDIANWIN0` / `@INDIANLOSE` GAME.TXT tokens).
3. **Colony approach** — Chebyshev walk toward colony ≤6 **only when**
   friction/alarm **≥70** (capture band). Mid gate 40..69 keeps on-tile loot /
   combat but must not march (seed-100 TURN4→5 already at-war for some tribes).
   Among equal distance, prefer colony whose warehouse holds muskets (≥5) or horses (≥1) so secondary
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
6. **Capture** — high band + tiny pop → `colonies_capture` (Indian → abandon);
   human thin **"The %s overrun %s!"** when abandoned colony is named
   (non-SCALP/BURN); SCALP/BURN abandon → **"The %s burn %s to the ground!"**
   (`@INDIANBURNCOLONY` thin).
7. **Friction/alarm escalate** — successful loot (`kind != NOTHING`) → tribe
   `alarm[].friction` and `indian.alarm_by_player` **+2** each (cap **100**).
   **Pocahontas** halves the bump (wiki/fandom half-rate). Cite:
   `docs/fandom_col1994.md` Pocahontas / Alarm.
8. **Hostility tick** — successful loot (`kind != NOTHING`) + friction ≥55 →
   `ai_diplo_indian_relation_delta` (−3, or −5 if ≥80). Deep 4528/2820 PARKED.
   Human target thin status: loot → **"The %s raid your colony."** (tribe name)
   when already at war; else **@INDIANSURPRISE** **"… surprise raid near %s! … chief
   denies involvement."** when not at war; **@INDIANWAR** **"… declare war!"**
   when peace bit cleared by high-friction escalate;
   `SCALP` → **"%s raiding party takes scalps in %s!"** (`@RAIDSCALP`);
   `GOLD` → **"%s raiding party seizes strongboxes in %s!"** (`@RAIDGOLD`);
   `SHIP` → **"%s raiding party attacks harbor in %s!"** (`@RAIDSHIP`);
   `STORES`/`WREAK`/`BURN` → tribe+colony stores/havoc/buildings lines;
   `NOTHING` (empty warehouse / no lootable stock) → **"%s raiding party wiped
   out in %s!"** (`GAME.TXT` `@RAIDNOTHING`, tribe + colony). Full `@RAID*` dialog widgets
   **Done** structural (`ai_popup`); DOS body / VGA chrome PARKED.
9. **Scout hostility** (`359c`-shaped) — alarm ≥90 + Scout name adjacent to Brave:
   prefer **displace** 1–2 free land tiles away (direct xy nudge + `AI_MOVE` goto);
   when displaced (not despawned) and status buffer present → human
   **"The %s warn your Scout away from their village."**; **despawn only if** no free tile
   (**"The %s kill your Scout."**). **Thin RNG kill-with-flee Done:** at alarm
   **≥95**, ~1/4 chance kill even when a flee tile exists (90..94 prefer
   displace). Dialog warn **widgets** **Done** structural (`ai_popup`); VGA
   chrome PARKED.
10. **PARKED** — deep `FUN_4d56_2820` (~1.4k; thunk `2a1f_044c`) meet/raid
   decision matrix + nested `2aac…311e` haggle (not this post-pulse path;
   Marathon2 R6 keeps PARK — no body port); full `4528` settlement body; ship
   harbor deep; full DOS dialog chrome; `@RAIDBURN` non-Town-Hall **built**
   building loot when stock empty (no safe `colonies_*` destroy API for
   workplace colonists). **Done thin:** `colonies_destroy_building` + human
   status naming the building. **Status-line chrome upgraded 2026-08-26**
   (was thinned paraphrase for every kind): 6 of 7 kinds now pull the real
   `GAME.TXT` `@RAID*` body via `popup_msg_fill` (old paraphrase kept only
   as the no-catalog fallback) — see the updated table below. Cite:
   `indian_contact.md` PORT DEBT; `docs/ai_transcription.md` FUN_4d56_2820.

## `@RAID*` message tags (`COLONIZE/GAME.TXT`)

UI strings, not numeric tables. Linux uses the **kind enum** to pick loot.
**2026-08-26: 6 of 7 kinds now render the real `GAME.TXT` body** (via
`popup_msg_fill`, `ai_contact.c`'s human-status block) instead of the old
hand-typed paraphrase, which now only serves as the fallback when no
message catalog is loaded. `@RAIDWREAK` is the deliberate exception (its
real DOS text is a third-party "Spies report... {nation-adjective} colony
of..." frame — wrong register for the raid's own victim, so it keeps the
paraphrase on purpose):

| Tag | Kind | Linux loot stand-in | Human status |
|-----|------|---------------------|---------------|
| `@RAIDNOTHING` | NOTHING | No stock change | Real body: "{tribe} raiding party wiped out in {colony}! Colonists jubilant!" |
| `@RAIDWREAK` | WREAK | Multi: food + tools + friction bump | **Kept thin on purpose** — "{tribe} raiding party wreaks havoc in {colony}!" (real body's "Spies report... {adjective} colony" framing doesn't fit the victim's own status line) |
| `@RAIDSTORES` | STORES | Decrement highest-value lootable cargo stock | Real body incl. the actually-drained cargo's name: "{tribe}... in {colony}! Large quantities of {cargo} stolen. Colonists outraged!" |
| `@RAIDBURN` | BURN | Kind gated on construction **or** lumber stock **or** non-Town-Hall built building; clear production / drain lumber; `colonies_destroy_building` when stock empty | Real body only when a building was actually destroyed (named, via `s_last_burn_building`); construction-cleared/lumber-drained sub-cases keep the paraphrase (no object to name — real `@RAIDBURN` text assumes a destroyed building) |
| `@RAIDSCALP` | SCALP | Population −1 if pop > 1 | Real body: "{tribe} raiding party takes scalps in {colony}! Colonists scream for revenge!" |
| `@RAIDSHIP` | SHIP | Coastal harbor: zero nearby Euro ship MP; dump 1 hold cargo ton | Real body incl. the actual ship's `units_display_name` (via new `s_last_ship_type`): "{tribe}... in {colony}! {ship} damaged. Colonists appalled!" |
| `@RAIDGOLD` | GOLD | Nation gold −N (treasury raid) | Real body incl. the actual amount drained (via new `s_last_gold_drained`, `%NUMBER0$`): "{tribe}... in {colony}! Merchants report {N}$ plundered. Colonists enraged!" |

## Exit criteria for deeper extract

- Sectioned `4528` with threat / combat / loot / dialog clusters named
- `5fef_0f14` line-faithful goods picker
- Status chrome: 6/7 kinds now render the real `GAME.TXT` body (2026-08-26,
  see table above); dialog **widgets** **Done** structural (`ai_popup`)
- Full `4528` / `5fef` line-faithful bodies still PARKED. **Walls gate
  ported 2026-08-28** (`FUN_5fef_0f14` head → `ai_contact_pick_raid_kind`:
  Stockade/Fort/Fortress chain count vs `rand(0,12)-1` → `@RAIDNOTHING`;
  early-game kind demotion) — `@RAIDWIN*` does not exist (P8.4 note); the
  defender-wins text is `@RAIDNOTHING` itself
