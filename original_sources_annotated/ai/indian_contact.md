# Indian contact / nation-turn contact path — thin section-map

Layer D hygiene for meet/trade/raid attachment. Quiet Brave pulse remains
[`indian_nation_turn.c`](indian_nation_turn.c) (`1816` → `14fe`). Raid **outcome**
arms: [`indian_raid_outcomes.md`](indian_raid_outcomes.md).

Linux: [`src/core/ai_contact.c`](../../src/core/ai_contact.c) +
`ai_indian_nation_turn` in [`ai.c`](../../src/core/ai.c). **Partial structural
port** — odd deviations OK; not T3.

## `FUN_4d56_1816` phase checklist

Annotated shell (quiet path only for act):

| # | DOS section | Linux |
|---|-------------|-------|
| 1 | Reseed LCG (`04ca`); set active nation = indian+4 | `ai_nation_reseed` |
| 2 | Select indian context + chrome | (no-op / turn cursor) |
| 3 | Alarm prelude (NEW WORLD) | `ai_contact_indian_prelude` — flag body thin (dialog PARKED); encroachment + mission pacify; LCG burns stay in pulse |
| 4 | Clamp alarm byte ≥ 0 | prelude clamp |
| 5 | Tribe growth loop (`41f2_0280` / `152e`) | `ai_grow_villages` |
| 6 | Relation / goods tick (`2a1f_0270` → `4962_06b6`) | `ai_contact_indian_relation_tick` |
| 7 | Clear act_counter | pulse clears `turns_worked` |
| 8 | Act loop → quiet `14fe` only | `ai_native_nation_pulse` (+ seed-100 overlays) |
| 9 | Meet / trade / raid (other paths; not inside `14fe`) | post-pulse `ai_contact_indian_meet_trade` / `…_raids` |

Alarmed / mission branches inside unit act: **PARKED** (`2154` / `2820` / `4528`).
Thin Linux meet arm (**after** first-contact treaty): human Brave×Euro
adjacency does **not** write refuse-talk / gift chrome (village dialog
PARKED). AI Euros skip auto-trade/gift when `alarm_by_player >= 55` or
relation `< 40`.
Unmet first contact uses `@INDIANWELCOME` on **land** units only (not ships)
— see checklist §0.
Teach-skill / missionary convert remain tribe-adjacency pulses with status
when the Euro is human.

### Prelude deepen (Linux `ai_contact_indian_prelude`)

1. Clamp `alarm_by_player` band; thin NEW WORLD flag body (isolated RNG; dialog PARKED).
2. **Encroachment:** Euro land unit whose display name contains `Soldier` / `Scout` /
   `Pioneer`, Chebyshev distance ≤ 2 from a tribe of this nation, and
   `tribe.mission == 0xff` → bump that tribe's `alarm[euro].friction` and
   `indian.alarm_by_player[euro]` by **+2** each (cap **100**). Per unit×tribe.
   **Pocahontas** (`founding_fathers_nation_has` / `FF_POCAHONTAS`): bumps
   **halved** (floor; wiki/fandom — alarm generated half as fast).
   Flag-body escalate bump (difficulty-scaled, sticky `unknown31_flags` bit 0x20)
   uses the same half-rate helper.
3. **Mission pacifies:** tribe with mission set to Euro `e`, and friction/alarm
   toward `e` low (`< 40`) → extra **−1** on tribe friction (and on
   `alarm_by_player` if also low). Floor 0.
4. Mission clear / burn when friction or `alarm_by_player` toward mission Euro
   **≥ 80** (`FUN_4cc6_0000`; `tribe.mission` → `0xff`). Human mission owner
   gets thin status **"Natives burn your mission."** (ai_popup Done; VGA PARKED).

### Meet-pulse mission pacify deepen

Mission owner present and mid-range friction/alarm (**40..79**, below burn/clear
at **≥80**) → **−2** once per tribe per `ai_contact_indian_meet_trade` call
(stronger than prelude low-band −1; no free crosses). Source: fandom Alarm —
missions slow hostility.

## Call-graph (authoritative vs catalog myths)

| Symbol | Thunk | Real callers (decomp) | Not |
|--------|-------|----------------------|-----|
| `FUN_4d56_1b3a` | `281f_0676` | Mid-turn helper: clear `0x5b04` tables, tribe `41f2_0266`, colony ownership probes | Does **not** call `2154` |
| `FUN_4d56_2154` | `2a1f_0434` | From **`5bfb` neighborhood** (~96088) after meet/diplo scoring | Not from `1b3a` |
| `FUN_4d56_2820` | `2a1f_044c` | Heavy decision + nested trade `2aac…311e`; also ~86766 | Full body PARKED |
| `FUN_4d56_4528` | `2a1f_016c` | Settlement enter/raid; from **move foreign** / contact (`move_spent` §3) | Not quiet `14fe` |
| `FUN_5bfb_022e` | `2a1f_066c` | Indian unit meet/contact (~96565); also ~98793 | First-contact WELCOME **Done** (land only; ends at PEACE/COME); village Meet CHOICE trigger PARKED |
| `FUN_4cc6_00f2` / `0000` | `0d6c` / `0398` | Relation delta / mission clear | — |

## Meet / trade `5bfb_022e` checklist (Linux)

0. **First contact** (`FUN_5bfb_022e` unmet / `FUN_5bfb_0182`): when
   `met_by_player` is clear, **land** unit adjacency to a village
   (`col1_contact_adjacent_tribe` / `game_loop`) **or** Brave adjacent to a
   **land** Euro during Indian turn enqueues **CONTACT_WELCOME** Yes/No
   (`@INDIANWELCOME`). Ships are skipped (natives do not hail vessels; DOS
   meet gates ocean). DOS ORs met bit `0x20` before the dialog — Linux sets
   `met_by_player` when welcome is shown.
   - **Yes** → `FUN_5bfb_0182` stand-in: peace bit `unknown33[euro] |= 0x40`,
     relation floor so refuse-talk (`< 40`) cannot fire next tick; OK
     `@INDIANPEACE`; if pre-accept relation `< 0x19` also OK `@INDIANCOME`.
     **Ends here** (DOS `goto LAB_5bfb_1005`) — no Meet CHOICE / gift chain.
     Land grant is **copy only** (WELCOME text); no tile-ownership write.
   - **No / cancel** → hostility (`FUN_4cc6_00f2` +100 hostility-axis stand-in)
     so `ai_diplo_indian_at_war`; OK `@INDIANSHUN` (“Prepare for WAR!”).
   - AI Euro: auto-Accept (no popup), same state writes.
   - Human **already-met** Brave adjacency: **no** spontaneous refuse-talk /
     gift / demand / trade chrome (village enter / deep `2820` PARKED).
     AI Euros keep silent auto-trade / gift-demand stand-ins.

1. Adjacent Euro land unit (already met + treaty resolved) → relation bump;
   human-facing `ctx->status` **"You meet the …"** (tribe `@TRIBES` short name)
2. Peaceful meet (alarm/friction < 40): slight tribe `alarm[].friction` decay (−1)
3. Optional mission assign if friction low (teach/convert **widgets** still OPEN)
4. **Missionary convert pulse** (structural deepen): Euro unit whose display name
   contains `"Mission"` or `"Jesuit"` adjacent to a tribe of this nation.
   Convert/crosses only when `tribe.mission == 0xff` and not alarmed
   (`alarm_by_player` / tribe friction both `< 55`).
   **Jesuit-grade gate**: mid-band (`40..54`) convert succeeds only for
   Jesuit-grade units (name contains `"Jesuit"`, NAMES `@JOB` profession **24**,
   **or** euro nation owns Father Jean de Brebeuf — fandom: all missionaries
   function as experts). Plain Missionary mid without Brebeuf → refuse
   **"Natives refuse conversion."** (PEDIA @JOB24 — Jesuits more effective).
   Peaceful (`<40`): any missionary establishes. Sets `tribe.mission = euro
   nation id`, decay alarm/friction by **1** (peaceful) or **2** (Jesuit mid),
   bump `nation[euro].current_crosses` by 1.
   One pulse per tribe per call. **Mission already set** (own or foreign) →
   skip convert pulse entirely (one-shot; no re-crosses / no steal).
   Alarmed (`>= 55`) → refuse with **"Natives refuse conversion."** (no crosses).
   Human success status **"Natives accept conversion."** (teach already had
   success chrome).
   **Missionary flee** (structural): when still adjacent to an alarmed tribe
   (`>= 55`) and not converting → nudge **1** free land tile away (greater
   Chebyshev distance) and set `AI_MOVE` goto. Full flee dialog PARKED.
5. **Teach-skill pulse** (structural deepen): Free Colonist or Scout (display-name
   match) adjacent to tribe, peaceful band (`alarm_by_player` / tribe friction
   both < 40), and `tribe.state.learned` clear → set **`tribe.state.learned = 1`**
   (real Col1 bit). If unit `profession == UNITS_JOB_NONE`: Scout →
   `UNITS_JOB_SCOUT` (Seasoned); else → tribe-appropriate outdoor `@JOB`
   (see mapping below). One pulse per tribe per call. Human status
   **"Natives teach …"**; teach **widgets** **Done** structural (`ai_popup`).
   Alarmed (`>= 55`) **or mid (`40..54`)** → refuse teach with
   **"Natives refuse to teach."** (mid-alarm refuse polish).
   **Already learned** (`state.learned` set) → skip teach and do **not** write
   teach/refuse status (Col1 one-shot; preserves gift/trade chrome).
6. Peaceful trade: colony trade-goods → lower alarm/friction (auto-haggle stand-in
   for `2aac…311e`); human status **"Trade accepted."** Meet CHOICE Trade:
   alarmed (`alarm_by_player ≥ 50`) or very-low relation (`< 40`) → haggle
   refuse OK **"Natives refuse to trade."** (`CONTACT_REFUSE`; `2aac` refuse
   stand-in); no goods otherwise → haggle stub OK **"Trade concluded."** (deep
   `2820` buy/hard-bargain **PARKED**).
7. **Gift / demand** structural stand-in after peaceful meet (`5bfb_102a` /
   `1092` **widgets** still OPEN). Human Meet→Gift with `ai_popups` enqueues
   **CONTACT_GIFT** amount CHOICE: **Small (−5 gold / friction −1)** or
   **Large (−10 gold / friction −2)** when purse allows (≥5 / ≥10). Auto path
   (no popup) keeps fixed Large −10 when gold **≥20**. Friction = max(
   `alarm_by_player`, tribe `alarm[].friction`):
   - **Alarmed** (`>= 55` on `alarm_by_player` or pair friction) → refuse
     gift/demand; status **"Natives refuse gifts."** when tribe friction is
     gift-band (`< 40`), else **"Natives refuse demands."** (tribe band — not
     pair friction — so alarm alone does not force the demand line)
     (no invented gold penalties)
   - **Low** (`< 40`) + Euro `nation.gold < 10` → refuse gift (cannot pay auto
     **−10**); status **"Natives refuse gifts."** (amount CHOICE still offers
     Small when gold **≥5**)
   - **Low** (`< 40`) + Euro `nation.gold >= 20` → auto gift/tribute: Euro
     **−10 gold**, friction **−2**; human status **"Gift of gold eases tensions."**
   - **Mid** (`40..54`) + tools/gold available → demand/payoff: Euro loses **10 tools**
     from nearest colony warehouse when stock **≥ 20**, else **10** from adjacent unit
     `tools` when **≥ 20**, else **15 gold** when treasury **≥ 50**; friction **−3**;
     human status **"Tribute paid; tensions ease."** (gift gold≥20 band mirrored
     for tools; gold stand-in needs a fuller purse when tools are short).
     When neither tools nor gold can pay → refuse **"Natives refuse demands."**
     Human Meet→Demand with `ai_popups` enqueues **CONTACT_DEMAND** amount
     CHOICE: **Pay tools (−10)** and/or **Pay gold (−15)** when each path can
     pay (auto path still prefers tools then gold).
   - **Very high** covered by alarmed refuse / raids.

Peaceful auto-trade remains a thin trade-goods→alarm stand-in; deep meet-trade
auto-haggle (`FUN_4d56_2820` / `2aac…311e`, thunk `2a1f_044c`) stays **PARKED**
(~1.4k decision matrix + nested bargain arms — not ported).

Raid gate at mid friction prefers **non-mission** villages (mission tribes only
raise the raid gate in the burn band **≥80**). Cite: fandom Alarm — missions
slow hostility.

Status lines only when `ctx->status` is present and the Euro is the human nation
(`ctx->human_nation`, else `player.control == 0`). When `ctx->ai_popups` is set,
**first unmet land contact** enqueues CONTACT_WELCOME Yes/No
(`@INDIANWELCOME` → `@INDIANPEACE`/`@INDIANCOME` or `@INDIANSHUN`); peaceful
Accept **ends** after those OKs (no Meet CHOICE). Meet CHOICE Trade/Gift/…
handlers remain for synthetic/village-enter apply (PARKED trigger). Human
Brave adjacency after meet does **not** auto-enqueue refuse/gift/trade chrome.
Teach CHOICE refuse (≥55) enqueues CONTACT_TEACH OK when applied; convert
success enqueues CONTACT_CONVERT OK. Mission burn (prelude ≥80) enqueues
CONTACT_RAID OK with status. Deep DOS dialog chrome (VGA-identical) stays
**PARKED**. Deep `FUN_4d56_2820` (~1.4k; thunk `2a1f_044c`) meet/raid decision
+ nested `2aac…311e` haggle stays **PARK only**.
Scout `359c` warn-on-displace already thinned; DOS RNG kill-with-flee-tile
stays **PARK** (Linux kills only when displace is blocked).

**Las Casas** (PEDIA `@FATHER24` / `docs/fandom_col1994.md`): existing Indian
converts (`NAMES` `@JOB` Convert / profession 27) assimilate as free colonists
(profession 19) on elect + FF ownership tick in `founding_fathers.c` — not the
missionary convert-pulse path.  **Sepulveda** convert-join remains **PARKED** (no 2820/4528 join hook).
 **Cortes** conquest treasure: **Done** — `units_try_native_settlement_fallout`
 wired from `units_resolve_land_combat_ff` when `units_set_native_fallout_context`
 is set; `units_cortes_conquest_treasure_gold` peels FUN_5fef_31ea when
 `gold_amount<=0` (caller-known amount still wins). AI live path verified via
 `units_resolve_land_combat` + turn_refresh fallout context (smoke). rich_capital
 (-0xcc) ← `ColonizeCol1TribeState.capital` (Done). KINGGALLEON2 / non-Cortes
 treasure chrome **PARKED**. On destroy, `nation.villages_burned++` (col1_save /
 reports villages_penalty). Thin Brave escort via `units_follow_unit` (raid-gate
 goto preference) and @RAIDBURN `colonies_destroy_building` + building-name
 status: **Done** thin.

**de Witt** foreign-colony cargo: **Done** — `colonies_de_witt_transfer_*` +
AI wagon/ship TRADE_GOODS act (`euro_unit_act` §2d4); ships enter foreign docks
when FF + peace.

**Washington AI combat col1:** **Done** — `units_resolve_land_combat` uses
`g_units_ff_col1` (was always NULL).

**Drake AI naval combat col1:** **Done** — `units_resolve_naval_combat` uses
`g_units_ff_col1` (was always NULL). Privateer *3/2 when nation owns Drake
(PEDIA/wiki +50%).

**de Soto LCR:** **Done** thin — `units_resolve_lcr_rumour` clears rumour via
`map_clear_rumour`; with FF 7 reveals radius (no invented treasure/FoY). Full
FUN_65dd_0004 RNG table **PARKED**.

### Teach-skill profession map (Linux)

Prefer `tribe.last_sold` when it is raw cargo **1..7** (sugar..silver) → matching
field job. Food(0) is **not** treated as an override so zeroed Col1 tribes still
hit the nation table. Else static map by `tribe.nation_id` (4..11 = `@TRIBES`
order). Unmapped → Expert Farmer. Full `@TRIBES` flavor-good string parse
(**Jewelled Relics**, …) remains **PARKED**.

| Driver | Value | Profession (`@JOB`) |
|--------|-------|---------------------|
| `last_sold` | Sugar (1) | Sugar Planter (1) |
| `last_sold` | Tobacco (2) | Tobacco Planter (2) |
| `last_sold` | Cotton (3) | Cotton Planter (3) |
| `last_sold` | Furs (4) | Fur Trapper (4) |
| `last_sold` | Lumber (5) | Lumberjack (5) |
| `last_sold` | Ore (6) | Ore Miner (6) |
| `last_sold` | Silver (7) | Silver Miner (7) |
| nation 4 | Inca | Silver Miner (7) |
| nation 5 | Aztec | Ore Miner (6) |
| nation 6 | Arawak | Fisherman (8) |
| nation 7 | Iroquois | Fur Trapper (4) |
| nation 8 | Cherokee | Tobacco Planter (2) |
| nation 9 | Apache | Cotton Planter (3) |
| nation 10 | Sioux | Fur Trapper (4) |
| nation 11 | Tupi | Sugar Planter (1) |
| fallback | — | Farmer (0) / Scout→Seasoned |

Raid hostility deepen (loot success + high friction → `ai_diplo_indian_relation_delta`):
see [`indian_raid_outcomes.md`](indian_raid_outcomes.md). Full `2820`/`4528` bodies
remain **PARKED**. Player meet/trade/gift/teach **status chrome thinned**; **widgets**
**Done** structural (`ai_popup` OK/CHOICE). Full DOS / VGA dialog chrome **PARKED**.

## PORT DEBT

- Full `2154` (~321), `2820` (~1.4k), `4528` (~3k) — **PARKED** (deep bodies)
- **Deep `FUN_4d56_2820` (thunk `2a1f_044c`) specifically PARKED:** ~1.4k-line
  meet/raid decision matrix; nested trade `2aac` (good dispatch) → `2af6` /
  `2bbc` (AI buy) / `2b92` / `311e` (demand / no-deal); choice loops,
  hard-bargain tension, per-good price arms; alarmed-branch dialog dispatch.
  Linux meet path keeps thin trade-goods→alarm + gift/demand / teach /
  convert status only — **not** a 2820 port. Peels: `layer_b_combat_raid`,
  `layer_b_2a1f_midlo`. Cite: `docs/ai_transcription.md` FUN_4d56_2820.
- **Done (structural unpark #1):** player meet/trade/raid/gift/teach **dialog widgets**
  (`5bfb_102a` / `1092`, teach chrome) — Linux: status + `ai_popups` OK/CHOICE
  enqueue + `ai_contact_apply_popup_result` (thin handlers incl. gift/demand
  amount CHOICE); VGA-identical dialog chrome still PARKED
- Full skill-from-`@TRIBES` flavor-good string parse — still PARKED
- Folding alarmed act into quiet `14fe` (would fight seed-100 T2) — still PARKED
- **Done (thin):** Brave escort post-pulse — `units_follow_unit` on same-nation
  AI_MOVE/GOTO within MD≤3; lead pick prefers goto toward raid-gate Euro colony
  when known, else nearest-lead (`ai_contact_indian_raids`). Deep alarmed escort
  scoring inside quiet `14fe` still PARKED. Cite: indian_raid_outcomes.md §1.
