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

1. **Gate** — among Euros with max(`alarm_by_player`, tribe friction) ≥ 40
   (**uniform; no per-nation term** — the old "Spain ≥35" was invented and
   backwards, retired 2026-09-09 smell #76: DOS's euro-2 specials are all
   Euro-aggressor multipliers, e.g. `FUN_5952_035e` viceroy 94172-94186),
   prefer Indian×Euro **at-war** (`ai_diplo_indian_at_war` / relation `<50`);
   then highest friction; tie-break lower `ai_diplo_indian_relation`
   (very-low hostility). Mid friction: prefer **non-mission** villages —
   mission tribes only raise the gate in the burn band (**≥80**). Cite: fandom
   Alarm — missions slow hostility. **Done (thin):** post-pulse Brave escort via
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
4. **Loot outcome — DOS-LITERAL since 2026-09-23 (bugs.md #827-#833).**
   `FUN_5fef_0f14` raw 99777-99893 is now transcribed whole:
   walls roll → `k = rand(1,4)` (1 goods, 2 building, 3 ship, 4 gold; **four
   kinds, no fifth**) → the five-line demote chain → each kind's own target
   check. The demote chain is **fortification-gated**, not alarm- or
   year-gated: `FUN_281f_09fc(n)` is the active colony's **building-bit test**
   (far thunk → `FUN_15eb_038e`), and rows 0/1/2 are Stockade / Fort /
   Fortress — a Fort demotes the building kind to goods, a Stockade demotes
   gold to goods and can demote goods to nothing, a Fortress kills the ship
   kind outright. The **turn** grace (`turn <= (difficulty-2)*-0x28`, Discoverer
   / Explorer only) kills the building and ship kinds early in the game.
   **STORES** cargo is a **retry roll**, not a value sort: `rand(0,0xf)`,
   rerolled while the colony holds < 10 of it, 100 tries then NOTHING, with a
   first-iteration horses special for a horseless tribe and a dummy
   `rand(0,200)` on a muskets roll (raw 99819-99833). Amount:
   `h = stock>>1; amt = rand(min(h,10), h)`, clamped to stock, floor 1 — a
   200-stock warehouse loses 10..100 (raw 99913-99926). Stolen horses and
   muskets then join the raiding tribe's record (raw 99939-99947).
   Gold amount is rolled in the HEAD: `cap = gold * colony_pop /
   (census_pop_proxy[euro] + 1) + 10` saturated at `0x7fff`, `amt = rand(50,
   cap)`, and a roll the victim cannot pay collapses the raid to kind 0.
5. **Multi-loot (secondary) — DELETED 2026-09-23 (bugs.md #833).** This used to
   describe a −5 muskets / −1 horse / −1 tools side-steal (from stock or from a
   unit's gear on the tile) applied after every non-NOTHING kind. `0f14` mutates
   **exactly one thing per raid** and never touches a unit's gear.
6. **Capture** — high band + tiny pop → `colonies_capture` (Indian → abandon);
   human thin **"The %s overrun %s!"** when abandoned colony is named
   (non-SCALP/BURN); SCALP/BURN abandon → **"The %s burn %s to the ground!"**
   (`@INDIANBURNCOLONY` thin).
7. **Friction/alarm — the tail is a NEGATIVE discharge**, see
   `ai_contact_raid_alarm_tail` below (raw 99961/99992/100006/100031). The
   "+2 bump, Pocahontas halves it" entry that stood here was fandom-derived and
   was retired 2026-09-08.
8. **Hostility tick** — successful loot (`kind != NOTHING`) + friction ≥55 →
   `ai_diplo_indian_relation_delta` (−3, or −5 if ≥80). Deep 4528/2820 PARKED.
   Human target thin status: loot → **"The %s raid your colony."** (tribe name)
   when already at war; else **@INDIANSURPRISE** **"… surprise raid near %s! … chief
   denies involvement."** when not at war; **@INDIANWAR** **"… declare war!"**
   when peace bit cleared by high-friction escalate;
   `GOLD` → **"%s raiding party seizes strongboxes in %s!"** (`@RAIDGOLD`);
   `SHIP` → **"%s raiding party attacks harbor in %s!"** (`@RAIDSHIP`);
   `STORES`/`BURN` → tribe+colony stores/buildings lines;
   a raid on a colony that is **not** human-controlled fires `@RAIDWREAK`
   (0x1b8a) at the human instead — the "Spies report…" third-party bulletin,
   once per successful raid of any kind (raw 99897-99899);
   `NOTHING` (empty warehouse / no lootable stock) → **"%s raiding party wiped
   out in %s!"** (`GAME.TXT` `@RAIDNOTHING`, tribe + colony). Full `@RAID*` dialog widgets
   **Done** structural (`ai_popup`); DOS body / VGA chrome PARKED.
9. **Scout hostility — REMOVED 2026-09-18 (bugs.md #499).** This entry used to
   describe a `359c`-shaped alarm >= 90 anti-Scout warn / displace / kill arm.
   It had no DOS source: `FUN_4d56_359c` (raw 83481-83505) is the
   **Enter-Hostile-Village wagon outcome** — `iVar1 = FUN_281f_030c(tribe, euro)`,
   `iVar2 = rand(0, 500)`, then `r <= alarm` → `@KILLWAGONS` (DS:0x15c3) +
   `FUN_281f_0808(unit)`; `r <= 2*alarm` → `@MADATWAGONS` (0x15ce); else
   `@GRUDGEWAGONS` (0x15da) + `thunk_FUN_2a1f_044c` (the 2820 trade). That body
   is already ported as `ai_contact_enter_hostile_village` (`a5e8`). No DOS code
   harasses Scouts on the Indian move pulse, so the arm was deleted.

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
| `@RAIDWREAK` | *(not a kind)* | **Corrected 2026-09-23 (bugs.md #829).** 0x1b8a is not a loot kind at all: raw 99897-99899 fires it once per **successful** raid whose victim colony is NOT human-controlled. It loots nothing. The port's old `AI_RAID_WREAK` (food −1, tools −1, construction cleared) was an invention and is deleted. | Real body, at the HUMAN: "Spies report: {tribe} raiding party wreaks havoc in the {nation} colony of {colony}." |
| `@RAIDSTORES` | STORES (DOS kind 1) | **Rewritten 2026-09-23 (#830/#831/#832):** cargo by DOS's retry roll (>= 10 stock floor), amount `rand(min(h,10), h)` over half the pile, stolen horses/muskets credited to the tribe record | Real body incl. the actually-drained cargo's name: "{tribe}... in {colony}! Large quantities of {cargo} stolen. Colonists outraged!" |
| `@RAIDBURN` | BURN | Kind gated on construction **or** lumber stock **or** non-Town-Hall built building; clear production / drain lumber; `colonies_destroy_building` when stock empty | Real body only when a building was actually destroyed (named, via `s_last_burn_building`); construction-cleared/lumber-drained sub-cases keep the paraphrase (no object to name — real `@RAIDBURN` text assumes a destroyed building) |
| `@RAIDSCALP` | **DELETED 2026-09-23 (bugs.md #828)** | Dead GAME.TXT text: `@RAIDSCALP` has **no DS string in VICEROY.EXE** (byte search: `RAIDWREAK` 0x1f52a, `RAIDSTORES` 0x1f534, `RAIDNOTHING` 0x1f55a, `RAIDSCALP` absent) and no popup-id row. `0f14` emits only 0x1b94 / 0x1b9f / 0x1ba8 / 0x1bb1 / 0x1bba and has no population-loss kind anywhere. The port's `AI_RAID_SCALP` (population−−) was an invention. | — |
| `@RAIDSHIP` | SHIP | **Fixed 2026-09-09** (was: zero the MP of any ship within 2 tiles + dump 1 cargo ton — nearly no damage for 0f14's biggest −16 alarm vent). DOS kind 3 picks a ship standing **on the colony tile** (`07e0` + `088a` + the `02e4` walk to type 0xd..0x12, no nation filter) and runs it through `5fef_0352` with `param_2 = 0xffff`: raw 99567 forces the damage arm, so the hull is **always damaged** — holds and passengers lost, bit7, repair timer, relocated to the nearest own repair port (`units_raid_damage_ship`, `units.c`). No ship in port → `LAB_5fef_123a`, kind collapses to **NOTHING** (no loot, no alarm vent) — `ai_contact_raid_kind_demote` | Real body incl. the actual ship's `units_display_name` (via new `s_last_ship_type`): "{tribe}... in {colony}! {ship} damaged. Colonists appalled!" |
| `@RAIDGOLD` | GOLD (DOS kind 4) | **DOS-LITERAL since 2026-09-23:** amount rolled in the 0f14 head (`gold * colony_pop / (census_pop_proxy+1) + 10`, saturate `0x7fff`, `rand(50, cap)`); an unaffordable roll → kind 0 | Real body incl. the actual amount drained (via new `s_last_gold_drained`, `%NUMBER0$`): "{tribe}... in {colony}! Merchants report {N}$ plundered. Colonists enraged!" |

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
