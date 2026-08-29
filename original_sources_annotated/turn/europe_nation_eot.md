# Europe nation EOT + market dynamics

Decomp: `original_sources_decompiled/viceroy_unpacked.c`.
Caller (DOS): `FUN_3844_00f2` via `291f_0a90` (`5e52`) and nested
`291f_0cbc` (`0058`). Linux reshape: `europe_tick_market_prices` /
`europe_apply_volume_price` in FINISH; full `5e52` immigrant/tax/FF arms
mostly **PARKED**.

Bridge: [`between_turns.md`](between_turns.md) ·
[`docs/turn_between_players.md`](../../docs/turn_between_players.md).

---

## `FUN_38fd_5e52` — Europe nation EOT

| Item | Value |
|------|-------|
| Lines | **68539–68623** (~85) |
| Thunk | `FUN_291f_0a90` |
| Arg | `param_1` = nation id |
| Early out | `DS:0x5382 & 1` (war) → return |

### Phases

| # | Lines | Role |
|---|-------|------|
| 0 | 68553 | Skip if wartime |
| 1 | 68554–55 | Page Europe `281f_0582`→`38fd_0000`; reseed `04ca` from `0x83a6` |
| 2 | 68556 | Clear Europe-block flag bit `0x20` at `*[0x84fc]` |
| 3 | 68557 | **Market EOT** `291f_0cbc`→`38fd_0058(0, 0xffff)` |
| 4 | 68558–67 | Immigration pressure `0b34`→`38fd_584a` → `+0x30` / accumulate `+0x2e` |
| 5 | 68568–615 | If score < pressure: pick dock slot; roll profession `0afc`→`46d4`; spawn harbor `0b26`→`0718`; dialogs; else Recruit UI `0d2c`→`4884`; may set `DS:0x14c` |
| 6 | 68617–20 | King tax-raise `0b7a`→`5be8`; on reject FF cargo gift `0c84`→`5930` |

### Key DS

| Addr | Use |
|------|-----|
| `0x5382` | War / chrome gates |
| `0x538e` | Turn (immigrant season `&3`) |
| `0x83a6` | RNG reseed |
| `0x84fc` | Nation Europe block (`nation*0x13c`) |
| `0x543f+n*0x34` | Player control |

### Linux

| DOS | Linux | Fidelity |
|-----|-------|----------|
| Phase 3 market | `europe_tick_market_prices` (FINISH) | **Partial** |
| Pressure / recruit / tax / FF | dock immigrants + `europe_tick_immigration_pressure` (584a +2/tick + phase5 pool→dock; `@UNREST` not `open_on_dock`) **Done** thin; phase-5 slot roll (`04d4` RNG(0,2), see below) **Done**; king tax elsewhere | Interactive **R** Recruit UI (3-slot picker) **Done** (`game_loop.c` `EUROPE_MENU_RECRUIT`, re-checked 2026-08-24 — stale "Recruit UI PARKED" removed); atomic `5e52` phase-6 tax/FF chrome **PARKED** (not Europe-screen scope — see phase 6 below) |

---

## `FUN_38fd_0058` — market dynamics

**Disassembly verified clean (2026-08-13).** Carried a Ghidra
`Removing unreachable block` disassembly-fault warning in the canonical
export (`docs/decomp_inventory.md`). Re-disassembled via the
overlay-addressing project (`tools/address_mapping.csv` →
`OVL05_L0040:458`): clean, self-contained, 1420 bytes / 275 decompiled
lines, 3 unrelated minor unreachable-block warnings left (ordinary
decompiler noise). Calls `thunk_FUN_1000_9bda` — target not resolved this
pass. Confirms the market-dynamics mapping below is working from
trustworthy source.

| Item | Value |
|------|-------|
| Lines | **58741–59005** (~265) |
| Thunk | `FUN_291f_0cbc` |
| Args | `param_1==0` full EOT peel; `param_2` cargo or `0xffff` = all |

### Phases

| # | Role |
|---|------|
| 1 | Price groups `0..0xf`: copy `DS:0x53ea[g]`; sum nation ledgers; human decays `0x53ea` |
| 2 | Cargos **9..12**: ratio vs sum; nudge pressure `+0x5c` or clamp bid `+0x4c` |
| 3 | `param_1==0`: cargos **1..4**; year bonuses; pressure ± |
| 4 | All cargos **0..15**: attrition; rise/fall ±1 bid + dialogs `0xfa8`/`0xfb0`; write ask preview `0x84bc`; if `param_2≥0` undo attrition |

### Key DS

| Addr | Use |
|------|-----|
| `0x53ea[16]` | `price_group_state` |
| `0x84fc+0x4c` / `+0x5c` | Bid / pressure per cargo |
| `0x96fe..` | `@CARGO` low/rise/fall |
| `0x538a` / `0x53a6` | Year / difficulty |

### Linux

| DOS | Linux |
|-----|-------|
| EOT all-cargo | `europe_tick_market_prices` | **Done, byte-exact vs 2 real DOS turn pairs (2026-08-28, `golden_market_prices01`)** |
| Post buy/sell | `europe_apply_volume_price` (threshold shed fixed 2026-08-28; volume term itself still unvalidated) |
| Phase 1 pool decay | **Done exact** — ledger `= price_group (signed) + Σ_n max(0, trade.tons2[n][c])` (`+0xfc` — the earlier "nation ledgers" were read as `tons`; wrong field), `price_group −= ledger >> 7` **only in nation 0's pass** (`0x9e12==0`, so never while nation 0 is withdrawn — the no-transports pair proves it); the colony-stock approximation is gone |
| Phase 2 cargos 9..12 | **Done exact** — `trade_nr += sign * mid * 100` (confirmed real for the human pass) |
| Phase 3 cargos 1..4 | **Done exact** — `trade_nr += mid * sign`; fur year &lt;1700/&lt;1600 |
| Phase 4 attrition / rise / fall | **Done exact** — `nr += attrition` (×2 for `0x9e12==3` on odd post-increment turns); threshold sheds `rise*100`/`fall*100` unconditionally, only the bid ±1 is gated by `[low,high]` (Linux used to gate both); `@PRICEUP`/`@PRICEDOWN` now real OK popups via `EuropeScreen.price_event_*` → `turn.c` FINISH |
| Phase 4 AI-only arms | **Not ported** (AI records aren't ticked): `high += (diff−4)*2 + (turn−600)/100` for cargos ≥14, bid caps for Horses/Tools/Muskets `((diff−4)*−3>>1)+3`, per-nation `DS:-0x7b44` table `= bid − 1` (this is the Custom House sale price — P4.4) |
| Sale / purchase ledger `38fd_1dfa` / `1d80` | **Done exact 2026-08-28** (`europe_apply_trade_volume`): `term = (amt << volatility) + 1d44(amt)`, `1d44 = ((0x9e12 human ? difficulty−2 : −2)·16·amt)/100` (C truncation); every nation's `nr[c] += term` (buy `−=`), the Dutch record (slot 3) gets `(term·2)/3`; seller `tons`/`tons2 ±= amt`; `gold[c] += (price·amt·(100−tax))/100` (sell) / `−= ask·amt` (buy). Sell price `38fd_0040 = euro_price − 1`, buy price `38fd_0016 = euro_price + burden` (Linux `bid`/`ask` were both +1; fixed, screen now shows Food 0/8 like the 1494 screenshot). Verified on the dutch2 pair: lumber sellers 54 (human, Viceroy) + 12 + 18 (AI) → +93 on nations 0–2, +61 on the Dutch; treasury +36 vs ledger +35 (different rounding, both real). Only the harbor buy/sell path follows with `0058(0, cargo)`; Custom House / AI dump-sell do not. |

---

## Deep — `5e52` phases 4–6

Cite: **68558–68620**.

### Phase 4 — immigration pressure (`584a`)

| Item | Detail |
|------|--------|
| Lines | **68558–67** |
| Call | `0b34`→`38fd_584a(DS:0x9e12, &local_c)` |
| Write | `Europe+0x30 = score`; `Europe+0x2e += delta`; clamp `+0x2e ≥ 0` |

`584a` (**68248–68300**): sum colony pops + unit count; `<<1` if &lt;4000; `+8`;
cap 4000; non-human `((8−diff)*score)>>3`; nation0 `*2/3`. Out-delta `*param_2`
defaults to **+2** (treasure can force −2 — PARKED).

**Port:** score → `needed_crosses`; idle **+2/tick** → `current_crosses` until
first dock immigrant (TURN5–7 stay 0 without churches); church crosses add to
`current` before the tick; spawn when `current > needed`. Separate
`immigration_pressure` fields are mirrors only.

### Phase 5 — dock immigrant vs Recruit

Gate: **`+0x30 < +0x2e`** (**68568**).

| Branch | Steps |
|--------|-------|
| Spawn path | Clear `+0x2e`. Slot `04d4(0,2)`. **`46d4((0x538e&3)==0)`** season quad. **`0718(old_prof)`** harbor spawn; human msgs `0x1190` / tip `0x1197`; Europe flags `\|0x40` |
| Else | **`4884(0,1)`** Recruit UI |
| Human follow | May set **`DS:0x14c=1`** (open Europe — see [`europe_finish_bridge.md`](europe_finish_bridge.md)) |

Callee args: `46d4(int season_force)`; `0718(int profession)`; `4884(0,1)`.

**`04d4` resolved (2026-08-15):** thunk → `FUN_281f_04d4`, the wrapped
DOS RNG-range helper (`FUN_281f_04d4` catalog entry; `src/core/dos_rng.c`'s
`dos_rng_range`) — so the spawn path picks the dock immigrant's pool slot via
a genuine `RNG(0,2)` roll, not always slot 0. Ported:
`europe_immigrant_from_pool(eu, rng)` rolls the slot when `rng` is non-NULL
(threaded from `turn_run_nation_ticks`'s `ctx->rng`); NULL keeps the old
first-filled fallback for fixture/no-rng callers (e.g. Fountain of Youth's
8x grant, which funnels through `4884` instead — separate, unexamined path,
left alone).

**`FUN_38fd_4884` real Recruit passage formula, ported (2026-08-15).** Read
the raw decompile (**64660-64794**, clean, no warnings) end to end instead of
trusting the catalog's thin "Recruit dialog" label. The actual price
computation (**64682-64694**):

```
base    = (nation.recruit_count[+6] + difficulty[0x53a6] + 7) * 20
floor   = max(base / 5, 100)
discount = (base - floor) * nation.current_crosses[+0x2e]
           / -(nation.needed_crosses[+0x30] + 1)   // FUN_1d1d_0ec6 signed div
passage = max(10, base + discount)
```

`+0x2e`/`+0x30` are the *same* per-nation Europe-block words `europe.h`
already names `current_crosses`/`needed_crosses` (immigration pressure,
phase 4 above) — the passage a player pays gets cheaper the closer the
colony is to its next free crosses-driven immigrant, floored at 10 gold.
The `-(...+1)` divisor is a deliberate DOS divide-by-zero guard (ones'-
complement encoding `~X == -X-1`, not a plain negation) — confirmed by
reading `FUN_1d1d_0ec6`'s own body (`viceroy_unpacked.c:20742`, a genuine
signed 32-bit/32-bit division, no sign trick of its own) rather than
guessing from the caller's bit-twiddling alone.

`nation.recruit_count` (Europe`+6`, capped 180/`0xb4`) only increments on a
*real* interactive Recruit click (**64778-64784**, gated
`param_1==0 && param_2==0`) — the free-immigrant `0718` harbor-spawn path
(phase 5 above) never touches it, confirmed by the gate sitting inside
`4884` itself, not shared code.

Ported: `europe_compute_recruit_passage()` (pure formula, `europe.h`) +
`EuropeScreen.recruit_count`/`.difficulty` (cached from `col1->head.difficulty`
each EOT tick in `europe_tick_immigration_pressure`, since
`europe_recruit_from_pool` has no `col1` pointer of its own) — replaces the
old linear "start 100, +16/recruit" placeholder flagged Unverified in
`manual_gap.md`. Verified against hand-traced values in `test_europe.c`;
full `ctest` clean (only the known pre-existing `unit_ai_euro_expand`
baseline failure). `4884`'s human CHOICE dialog chrome (3-slot picker UI
itself, param combos 2/3/4) still not ported — out of scope, this pass only
needed the price math.

### Phase 6 — tax then FF gift

| Lines | Call | Meaning |
|-------|------|---------|
| 68617 | `0b7a`→`5be8()` | King tax audience; **1** = dialog ran |
| 68618–19 | if **0**: `0c84`→`5930()` | FF cargo/gold grant path |

Linux: dock immigrants / crosses in `turn_run_nation_ticks`; tax in `ai_king`;
atomic `5e52` **PARKED** — `5be8`/`5930` are king-audience and FF-grant
functions, out of `europe.c`'s domain (`ai_king.c`/`founding_fathers.c`), not
attempted from the Europe-screen side (2026-08-24 re-check).

Also re-checked 2026-08-24: europe.c now enforces boycotts on the human
trade path (`europe_cargo_boycotted` gates `europe_buy_cargo` /
`europe_sell_hold` / `europe_sell_unit_hold`; `EuropeScreen.boycott_bitmap`
mirrors `nation.boycott_bitmap` live each Europe-screen render). Previously
`nation.boycott_bitmap` was written by `ai_king.c`/`ai_diplo.c` and read by
`ai_euro.c`/`reports.c`, but nothing on the human buy/sell path checked it —
a player could freely trade goods Parliament had boycotted. Fandom source:
Boycott (Col) — "goods blocked in Europe until penalty paid or Fugger".

### Boycott buy-back — `FUN_38fd_2dfe`, ported 2026-08-24

Found the real UI trigger by reading `GAME.TXT` instead of guessing a menu:
`@SOMEBOYCOTT` ("Some of the cargo could not be unloaded because of a
parliamentary boycott. If you want to ask that the boycott be lifted, click
on the cargo type in question.") names the click site directly — the
Europe market strip cell for the boycotted cargo. `FUNCTION_CATALOG.md`
already had the callee labeled ("pay to lift cargo boycott", thunk
`FUN_291f_0c06`) but nothing had read or ported it yet.

`FUN_38fd_2dfe` (**60904–60945**, clean disassembly, no warnings):

```
if (nation invalid or AI-controlled) { clear a display flag; return; }  // human-only gate
price = FUN_38fd_0016(cargo)        // effective ask price — already ported
                                     // as eu->cargo[cargo_type].ask
cost  = price * 500                 // matches fandom's "500 tons of that
                                     // good at current price" exactly
if (nation.gold < cost) {           // GAME.TXT @KISSSORRY
  show "only {gold}$ available"; return;
}
nation.gold        -= cost;         // nation+0x2a (32-bit)
nation.royal_money  += cost;        // nation+0x22 — col1_save.h already
                                     // names this exact offset royal_money
                                     // (Crown/REF budget, FUN_43f7_1d42) —
                                     // paying back taxes literally funds
                                     // the King's war chest
nation.boycott_bitmap &= ~(1<<cargo) ; // nation+0x20
```

The `nation+0x22`/`royal_money` match is the load-bearing confirmation here:
that offset was independently named from `col1_save.h`'s own DOS-export
tracing before this pass touched it, and the newly-read function writes to
exactly that field — not a coincidence, a real cross-check.

Ported: `europe_buyback_boycott(eu, col1, human_nation, cargo_type)`
(`europe.c`/`europe.h`), wired at the click site in `game_loop.c`'s
`EUROPE_HIT_MARKET` handler — clicking a boycotted market cell calls it
instead of the normal buy/select flow, regardless of whether a harbor ship
is selected (matching `@SOMEBOYCOTT`'s wording, no ship needed). `@KISSUP`'s
Pay/Cancel CHOICE dialog and `@KISSSORRY`'s insufficient-funds dialog are
**not** modal-ported — implemented as an immediate action + `eu->status`
line instead, the same chrome-PARKED tradeoff already accepted elsewhere on
this screen (e.g. `europe_custom_house_autosell`, the `+`/`U` immediate
buy/sell keys). `tests/unit/test_europe.c` covers insufficient-funds no-op,
successful pay (gold debited, `royal_money` credited, bit cleared), and
no-op on an already-unboycotted cargo.

---

## Deep — `0058` phases 1–3

Cite: **58787–58929** (phase 4 attrition already in table above).

### Phase 1 — price_group + ledger half

For `g=0..0xf`: seed from `DS:0x53ea[g]`; add nation ledgers at
`(n*0x4f+g)*4 − 0x76fc`. If `param_1==0` and `0x9e12==0`:
`0x53ea[g] −= (sum >> 7)`. Linux approximates with colony stock `>>7`.

### Phase 2 — cargos **9..12**

`sum = au[9]+…+au[12]`; per cargo ratio vs `0ec6(sum×3, cargo)`;
`sign = sgn(bid[+0x4c] − ratio)`. EOT (`param_1==0`):
`pressure[+0x5c] += sign * ((rise+fall)/2) * **100**`. Else clamp bid into
`[low,high]`.

### Phase 3 — cargos **1..4** (`param_1==0` only)

Denom from half cargo-0 + au[1..3]; cargo **4** halves ledger before ratio;
year &lt;`0x6a4` / &lt;`0x640` → +1/+2 to target. Nudge:
`pressure[+0x5c] += ((rise+fall)/2) * sign` — **no ×100**.

Cross-links: [`nation_ticks_bells_ff.md`](nation_ticks_bells_ff.md) ·
[`ai/king_ref.md`](../ai/king_ref.md).

---

## Transit turns (voyage duration) — dead end, 2026-08-16, don't re-chase blind

`docs/assets.md` still flags the Europe↔New World voyage turn count
(`europe_voyage_turns` — east 2, west 4, −1 if ship MP≥6, clamped 1-4) as
**Unverified vs DOS**. Chased the natural lead — the Harbor ship context
menu, `FUN_38fd_2bfe` (**60787–60900**, catalog "sail / sell / unload"),
whose `iVar2==1` branch (first menu item, unconditionally offered) calls
`FUN_281f_089e(nation)` then clears two globals (`0x9e20`/`0x9e1c`).

**Resolved the real call target (previous pass's "next session should
start there" note) — it's not departure logic.** `address_mapping.csv`
gives `281f:089e` → resident `FUN_1000_8a8e` (`exact` match); decompiled
directly against the overlay-correct `OverlayTest` project
(`GhidraDecompileAt.java 0000:18a8e`, see command below) — clean, 2 calls:
```c
void FUN_1000_8a8e(void) {
  FUN_1000_1e61();   // RTLink/interrupt-flavored trampoline (LAB_1000_39e1
                      // state byte, jumptable warnings) — infra, not game logic
  FUN_0000_45ee();    // → FUN_0000_45d2(unit, unit.x[+0x3144], unit.y[+0x3145])
}                      // i.e. "re-place/redraw unit at its own xy"
```
So selecting "Sail" in this CHOICE menu just redraws/re-registers the ship
at its current position (probably marking it as now in-transit for the
minimap/sprite layer) — it does **not** compute or store a turn count
anywhere in this call chain. `FUNCTION_CATALOG.md`'s old label for
`FUN_281f_089e` ("re-place unit at its current xy") turns out to have had
the right verb, just attached to the wrong symbol/segment — corrected in
place.

**Implication:** the real turn-count math (if it's a discrete lookup at
all, rather than the ship's normal movement-point allowance being consumed
against a fixed "High Seas" distance every EOT — the general-Colonization-
knowledge explanation for why faster ships arrive sooner) lives somewhere
in the generic per-unit turn-processing loop, not in this UI click handler.
That's a much larger, different function to find (on the order of `5b66`/
`20e6`'s per-unit-act dispatchers, but for *player* units, not the AI path
this project has already mapped) — genuinely out of scope for a quick
pass. **Don't re-enter through this menu function again** — it's a
confirmed dead end now, not just an unexamined lead.

Reproduction (needs `~/projects/ghidra_overlay_scratch/OverlayTest`, no
`.lock` files):
```
analyzeHeadless ~/projects/ghidra_overlay_scratch OverlayTest \
  -process seg_data_resident.bin -readOnly -noanalysis \
  -postScript GhidraDecompileAt.java 0000:18a8e -scriptPath tools
```
(`0000:18a8e` = resident linear address for DOS `1000:8a8e`, i.e.
`(0x1000<<4) + 0x8a8e` — `GhidraDecompileAt.java`'s `0000:` space is a flat
linear address, unlike the canonical project's own segment-prefixed
`FUN_1000_*` naming.)

---

## Dock-immigrant "equip before boarding" — investigated, disproven, don't build

User asked whether a dock colonist can be equipped with tools/muskets/
horses before boarding a ship (Linux currently can't — `EuropeDockImmigrant`
only carries `profession`). The only DOS candidate is the dock-immigrant
action mega-dialog, `FUN_38fd_3746` (**61212-64066**, 2854 lines, catalog
"board/orders", real Ghidra corruption warnings above its declaration:
`Instruction at (ram,0x0003ff0b) overlaps instruction at (ram,0x0003ff07)`
+ `Control flow encountered bad instruction data` — same signature class as
the original `4528`/`417e` blockers).

**Re-peeled via the overlay-correct project, per the standing method.**
`address_mapping.csv` gives `38fd:3746` → `OVL05_L0040:3b46` (`gap` match —
canonical entry point unconfirmed, same flag the four originally-blocked
functions all carried). Raw-disassembled a window around that address
(`GhidraListInstrs.java`, no `createFunction()`, avoids the reachability-
walk misresolution bug) and found a genuine `RETF` at `0x3b45` immediately
followed by a clean `ENTER 0x6c,0x0` prologue at exactly `0x3b46` — so the
mapped address **is** the real entry point here (unlike `5b66`); the
corruption is purely mid-body.

**First legible block after the prologue looked exactly like a gear-price
table** — three near-`CALL`s to `0x006740` and three to `0x0066c3`, each
preceded by pushing `0xf`/`0xe`/`0x8` (== `COLONIZE_CARGO_MUSKETS` /
`_TOOLS` / `_HORSES`), each result `IMUL`'d by `0x32`/`0x64`/`0x32` (50/
100/50 — the real Col1 equip quantities) into 6 stack locals, later picked
by a unit-class (`+0x3146`) compare and passed to a dialog/gold-deduct
call. A very plausible read — **and wrong.** Both `0x006740` and `0x0066c3`
are themselves unpatched-RTLink-placeholder thunks (`CALLF <loader>; JMPF
0x0000:XXXX`, the same mechanism documented in `euro_unit_act.md`'s `a6e4`
writeup); resolved both through `rtlink_decode`'s own jump-table (built a
debug print of `listInfo()`'s jump list, matched by the `CALLF`'s own file
offset = `ram_address + 0x2400` — verified against `euro_unit_act.md`'s
already-confirmed `a6e4` pair first) rather than trusting the placeholder
bytes: both land in **`OVL03_L0000`**, offsets `0x0016` and `0x0040`.

**Decompiled/disassembled both real targets — neither is a price lookup.**
Both iterate the 8 native tribes (`FUN_1000_89d0` tribe lookup, `unit+
0x3147 & 0xf` tribe id, `FUN_1000_8c28` diplomacy-flags gate — the exact
same resident helpers `indian_incite_417e.md` already named), tracking a
per-tribe alarm-like value and its max/sum. **This is a native-alarm scan,
not a gear-price table** — the `0xf`/`0xe`/`0x8` args pushed by the caller
are not cargo-type ids at all (numeric coincidence only), and the `50`/
`100`/`50` multipliers are unrelated to equip quantities. The whole
"gear-price table" reading was a pattern-matched false lead, cleanly
disproven by the resolved bytes, not left merely unconfirmed.

**Conclusion: no evidence `FUN_38fd_3746` (or any part of it examined so
far — only the first ~150 of ~2854 lines) implements dock-side equip.**
Didn't chase further: fully verifying "equip-at-dock exists nowhere in
this function" would mean re-peeling the entire corrupted 2854-line body,
disproportionate effort for one UI nice-to-have. **Don't build the
tools/muskets/horses-at-dock feature** — not confirmed as real DOS
behavior, and the one lead that looked like it pointed there didn't pan
out. If this is revisited, the efficient next step is a **live DOSBox-X
capture** (same technique `indian_incite_417e.md`'s "Live debugger
capture" section used) — drag Tools onto a dock colonist in the real game
and see whether anything fires — not more static RE of this specific
function.
