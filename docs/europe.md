# europe.h design notes

Design/rationale/DOS-layout prose moved out of `src/core/europe.h` comment blocks, so the header stays declaration-focused. Each section below is a verbatim copy of a comment that used to sit above the named symbol in the header.

## EUROPE_DOCK_MAX

Dock queue depth. NOT the drawn-slot count (that is EUROPE_DOCK_ROW0 +
EUROPE_DOCK_ROW1 = 8, see europe_dock_slot_pos): DOS keeps the whole queue
and simply stops blitting past tier 1, so a real save can carry far more
than eight colonists waiting in Europe. Survey of the French originals
(nation 1 = human, dock lane 236+n = 237,237): COLONY02 20 units,
COLONY03 18, COLONY04 / COLONY09 13 each. 8 truncated every one of them.
Cite: smell audit 2026-09-10 F3.

## EUROPE_DOCK_X dock layout

Dock immigrants: DOS FUN_38fd_146c lays them out in TWO rows off one base x
(FUN_38fd_15aa passes 0xe9 = 233), 17px pitch, 16x16 sprites — 3 slots on the
upper quay (y = 0x8a = 138) and 5 on the lower one (y = 0xa1 = 161). Index 8
and up gets tier 2, which FUN_38fd_14e2 never draws — so 8 is the drawn
count, but NOT the queue depth (EUROPE_DOCK_MAX): DOS keeps the extra
immigrants on the dock lane and just leaves them unpainted.
The 233/138 origin is confirmed by original_screenshots/europe (the selection
frame there runs x 232..249, y 137..154 — 1px outside a 16x16 at 233,138).

## EUROPE_DOCK_TYPE_*

DOS @UNIT type codes a dock unit can hold (FUN_38fd_0718 / the @ARMOPTIONS
menu moves an immigrant between exactly these six). The kit follows from
the type: Soldiers and Dragoons carry 50 Muskets, Dragoons and Scouts 50
Horses, Pioneers 100 Tools — DOS stores only the Tools byte and lets the
type imply the rest.

## EuropeHarborShip cargo_treasure_gold

Per-passenger Treasure gold for Europe cash-in (0 = unknown / not treasure).
Source: the DOS unit value byte `+0x315b` = COL1 `profession` = gold/100
(units_treasure_value_gold), captured by
game_loop.c's game_europe_capture_pax_treasure_gold and filled onto the
newest Expected slot by game_europe_fill_expected_treasure_gold on both
H/sail-to-Europe and Return-to-Europe paths (2026-08 — stale "does not
fill this yet" wording removed; cash-in itself is europe_cash_treasure /
europe_cash_treasure_passengers below).

## EuropeHarborShip departed_this_turn

Set the moment a ship enters a transit lane, cleared (instead of
decrementing turns_left) by the first europe_tick_voyages that sees it.
The turn a ship sails is a turn at sea: in DOS the lane counter written
by FUN_48d3_0002 is 1 for an ordinary crossing, yet ordering a Caravel
home and pressing End Turn twice is what actually docks it (player-
verified) — the departure itself happens inside the same end-of-turn
pass that has already run its FUN_48d3_03d0 decrement step, so the
counter's first tick lands on the *next* turn. Modelling that here
keeps turns_left holding DOS's own 1/2 for the save's col1_counter16
byte instead of inflating it. Runtime only; a save taken between the
order and the next tick reloads without the grace turn.

## EuropeHarborShip trade_route_plus1

Trade-route automation (bugs.md: "A ship trying to go to Europe via
trade route just goes to the sea lane tile, then returns. Never visits
Europe."): 0 = not on a route; else route slot + 1. trade_stop is the
stop index the ship is travelling to service (going east: the Europe
stop; going west after service: the next stop, re-armed as TRADE_ROUTE
orders when the ship spawns back on the map). Zero-init = none.

## EuropeScreen current_crosses

Crosses meter = DOS Europe +0x2e / +0x30 (same words as immigration pressure).
needed = FUN_38fd_584a score each EOT; idle +2 until first dock immigrant;
then church crosses only; spawn when current > needed.
Cite: europe_nation_eot.md; TURN1–7 goldens.

## EuropeScreen pool_force_expert

DOS `FUN_38fd_5e52` phase 5 refills the emptied pool slot with
`46d4((DS:0x538e & 3) == 0)` — one turn in four the tier roll is skipped
and the slot is guaranteed to come from the expert half. DS:0x538e is
the turn counter, which this screen has no other way to see, so turn.c
stamps the quad here right before the immigration tick.

## EuropeScreen bound_nation

DS:0x9e12 — the nation this Europe module is bound to (FUN_38fd_0000
sets 0x9e12 = nation and 0x84fc = its record). Every port caller of
europe_set_nation binds the human nation, so the live `trade_nr`/quote
record is that nation's, and the 1d44 human test / the 1dfa Dutch
(slot 3) damping key off this index. Cite: viceroy_unpacked.c
60186-60201 (1d44 reads 0x9e12), 58696-58702 (FUN_38fd_0000).

## EuropeScreen bound_human

DOS FUN_38fd_0718's own human test (raw 59120-59122):
`*(int*)0x9e12 < 4 && *(char*)(0x9e12*0x34 + 0x543f) == 0` — the bound
nation is a Euro nation under player control. Only then does the
Dragoon roll use the difficulty byte as its bound; an AI nation uses 1.
Refreshed wherever `difficulty` is (europe_nation_eot_tick), true until
a col1 says otherwise.

## EuropeScreen purchase_confirming

@REALLYBUY confirm state (FUN_38fd_4b50 raw 64874-64887): a PURCHASE
row pick opens this Yes/No popup instead of buying outright. GAME.TXT
order verbatim — UI row 0 is "Yes" (raw 64874 `iVar6 == 1`, DOS's
default choice), row 1 "No"; europe_menu_confirm_ex special-cases
purchase_confirming ahead of its generic sel==0-cancels shortcut, since
that would otherwise treat "Yes" as a cancel. purchase_confirm_index/
cost freeze the item and price the popup was opened for so the answer
never re-reads a table that may have moved. bugs.md #753.

## EuropeScreen dock_menu_label

GAME.TXT @ARMOPTIONS rows for the clicked dock immigrant, built by
europe_build_dock_menu. DOS omits a row it has disabled rather than
greying it, and greys one the player cannot afford, so `count` is the
number of rows actually shown and `row[]` remembers each one's 1-based
DOS row id — the id the action switch (and DOS's own dialog) works in.

## EuropeScreen price_event_cargo

This EOT tick's rise/fall events (@PRICEUP/@PRICEDOWN, FUN_38fd_0058
phase 4): DOS calls FUN_281f_0652(0xfa8/0xfb0) inline, once per cargo
that crosses its threshold, inside the same 0..15 loop — so two
different cargos changing the same turn both get their own dialog.
turn.c walks price_event_cargo[0..price_event_count) in order and
queues one OK popup per entry (dir +1 rose / -1 fell); collapsing this
to a single "last event" dropped every popup but the last.

## EuropeScreen docked_with_goods

FUN_48d3_08bf's `local_a` gate: a ship that docked this tick was carrying
goods, so the caller (human turn only) fires woodcut 9, CARGO FROM THE
NEW WORLD. Reset by every europe_tick_voyages call, like open_on_dock.

## EuropeScreen boycott_bitmap

RENDER MIRROR of the human nation's ColonizeCol1Nation.boycott_bitmap
(nation+0x20) — refreshed each frame the Europe screen renders, and NOT a
source of truth: ai_king.c's tea party and ai_diplo.c's wartime embargo
write the nation word, so this copy is stale for anything that runs
without a render in between (smell audit G6 — an EOT trade-route unload
sold cargo the same turn's tea party had just boycotted). Every trade
gate now goes through europe_cargo_boycotted_ex with the save in hand;
what is left reading this field is Europe-screen chrome (market strip
colour, @ARMOPTIONS row visibility) and callers with no save bound
(tests). Bit c set = cargo type c blocked from Europe trade until the
boycott is lifted. Source: fandom Boycott (Col) — "goods blocked in
Europe until penalty paid or Fugger"; Custom House bypasses this
(europe_custom_house_autosell intentionally does not check it).

## EuropeScreen bar_event

DOS status-line lines this screen has composed but the game loop has not
handed to the strip yet (FUN_38fd_23c4 tail: compose into DS:0x2d54, arm
with FUN_38fd_19d8(1, 0x78, 0), repaint). Drained by the Europe frame in
game_loop.c; see bugs.md #376.

## Europe sale status line

Compose DOS's Europe-sale status line for one sale and queue it in
`bar_event` — "<amount> <Cargo> sold for <gross>. <tax>% Tax: <paid>. Net:
<net>" (FUN_38fd_23c4: @CMESSAGE 1 / DS:0xfef "." / @CMESSAGE 0x11 / 0x12).
Call it with the price still at the pre-sale bid; `net` is the gold that
was actually credited.

## europe_voyage_turns_roll

FUN_48d3_0002 voyage roll. rng NULL → 1 (no roll). The x<3 west-edge
branch in DOS only burns RNG(0,1)+an FF test and discards both — there
is no west-edge sail penalty in the shipped code (PEDIA's Magellan
"west edge" line describes the 10% delay this FF removes).

## europe_compute_recruit_passage

DOS `FUN_38fd_4884` real Recruit passage formula (was a linear
start-100/+16-per-recruit placeholder — see manual_gap.md). base =
(recruit_count+difficulty+7)*20; floor = max(base/5,100); discount =
(base-floor)*current_crosses / -(needed_crosses+1) [`FUN_1d1d_0ec6`
signed division; the +1 is the DOS divide-by-zero guard when
needed_crosses==0]; passage = max(10, base+discount) — cheaper the
closer current_crosses is to the next free immigrant.
Cite: viceroy_unpacked.c 64682-64694; europe_nation_eot.md "Phase 5".

## Pool refill RNG stream

The pool refill these three leave behind is DOS `FUN_38fd_46d4`, whose
tier rolls come off the shared game stream (`FUN_281f_04d4`) — pass the
game rng (ColonizeTurnContext.rng / ColonizeGameState.move_rng) through
the `_ex` forms. The plain forms are the NULL-rng fixture shorthand and
fall back to europe.c's local LCG. Smell audit 2026-09-10 G5.

## europe_immigrant_from_pool

Crosses / unrest: move one pool slot to docks; refill. DOS `5e52` phase 5
picks the slot via `FUN_281f_04d4` RNG(0,2) before rerolling it, not
always pool[0] — pass `rng` to match; NULL falls back to first-filled
(fixture / no-rng callers). Cite: europe_nation_eot.md "Phase 5".

## europe_refill_pool_slot_rng

Same refill on the real game stream: DOS's `46d4` tier rolls are
`FUN_281f_04d4(1,15)/(1,10)/(1,8)` off the shared RNG (viceroy_unpacked.c
64632/64636/64640). The expert half stays on europe.c's local generator —
DOS draws that from a per-nation LFSR whose two state bytes the port
repurposed, and like the LFSR it consumes no shared-stream draw, so a
force-expert refill leaves `rng` exactly where DOS leaves it.

## europe_seed_campaign_prices

New-campaign opening prices: FUN_38fd_6024 (viceroy_unpacked.c 68645-68654)
rolls bid = RNG(start_lo..start_hi) inclusive for each of the 16 cargo
slots (16 LCG draws, cargo order, hi==lo still draws), one roll shared by
all four nations. New game only — the load path keeps the save's
euro_price. Smell audit #55.

## Europe dock arrival Dragoon roll (_ex forms)

The `_ex` forms take the shared DOS stream: FUN_38fd_0718 (raw
59117-59128) rolls `04d4(0, bound + 4) == 0` -> Dragoons for every
Soldier-profession dock arrival, training and purchase included. The
plain forms pass NULL (plain Soldiers, no draw) and exist for callers
with no rng at hand.

## europe_purchase_open_confirm

Open the @REALLYBUY confirm popup for an affordable row (FUN_38fd_4b50
raw 64874-64883): computes the price — bumping the artillery escalation
counter right here, unconditionally of the eventual Yes/No, exactly as
DOS does before it ever shows the popup — and freezes it in
purchase_confirm_cost. Returns false (no-op) for an out-of-range or
unaffordable index. bugs.md #753.

## europe_dock_caption

DOS FUN_38fd_3694 (raw 61190-61195), the Europe dock caption on the
status line: "<@NATIONALITY adjective> <@UNIT plural>", then
" (<@JOB singular>)" unless the immigrant's profession is 0x1c (none).
Writes at most `cap` bytes; returns false on a bad index.

## europe_dock_push_load

Push a save-loaded Europe-dock colonist straight onto the dock (append at
back, present, default sentry) — for col1_bridge_apply restoring a human
nation's waiting-in-Europe colonists on load. Unlike europe_recruit_from_
pool/europe_train/europe_purchase this charges no gold and posts no status
message. Returns false if the dock is full.

## europe_remove_dock_mirror_unit

Drop the (236,236) Europe-map mirror unit that shadows a dock immigrant
(turn.c immigrant spawn / col1_bridge load create one per dock entry so
Col1 capture keeps the colonist). Prefers the matching profession.

## europe_build_dock_menu

Build the @ARMOPTIONS row list for dock[dock_index] into eu->dock_menu_*.
Cite: DOS FUN_38fd_37xx — prices and quantities at 38fd:3745..3830, the
per-row enable switch at 38fd:388e..3a04, and the add/grey tail at
38fd:3a32..3a7b (a disabled row is not added at all; an unaffordable one
is added greyed).

## Europe dock menu row with col1 ledger

As above, plus the col1 save the arm buy/sell rows book their trade into.
The three sell rows and the three buy rows each call DOS's bare volume
routine (291f_0a2e = FUN_38fd_1dfa, 291f_0c14 = FUN_38fd_1d80), and those
write the per-cargo tons/tons2/gold ledger on nation `col1->nation[bound]`
just as the harbor channels do — so a caller that has the save in hand
should use this form. `col1` may be NULL, which keeps the price-pool move
and skips the ledger (the plain form above is exactly that call).

## europe_dock_unit_dos_type

DOS FUN_38fd_0718 (the Europe harbor spawn behind every dock arrival):
the @UNIT type a dock immigrant of this @JOB profession is created as.
Pioneer (0x14) -> Pioneers, Missionary (0x18) -> Missionaries, Scout
(0x16) -> Scouts, Soldier (0x15) -> Soldiers, or Dragoons on a
`rng(0, bound + 4) == 0` roll where bound is the difficulty for a human
nation and 1 otherwise; anything else -> Colonists. Returns the DOS type
code 0..5, which europe_dock_unit_type_index maps to a pool type.
Pass rng = NULL to skip the Dragoon roll (plain Soldiers).

## europe_dock_unit_type_index_ex

Same lookup with ai_euro's singular fallbacks (audit AE-18): "Soldiers"
→ "Soldier", "Colonists" → "Free Colonist" → "Colonist", and so on, for
pools that carry the singular @UNIT spelling. The Europe screen passes
false; the 5d04 AI purchase path wants true.

## Europe purchase table

The Europe purchase table (DOS FUN_521d_5c3c, DS:0x978d stride 6) — the
single owner of the six prices (audit AE-17). europe_purchase_price
returns 0 for a name that is not on the list.

## europe_dock_type_for

DOS @UNIT type for a dock entry. A name that is itself one of the six
(an arriving passenger keeps the type it sailed with) wins; otherwise the
profession decides, the way FUN_38fd_0718 decides it for a fresh
immigrant. Anything else — Artillery, a purchased hull — is Colonists.
A Continental Army / Cavalry (kinds 9 / 7) disembarked in Europe keeps its
own @UNIT row as `dos_type` (bugs.md #669; `europe_dock_dos_type_is_valid`),
the way DOS keeps +0x3146; the kit helpers know both rows.

## europe_dock_icon_sprite

ICONS.SS sprite for a dock entry: what the immigrant now is (kit type),
with DOS FUN_112b_0060's expert/generic pose split keyed by profession.

## europe_passenger_icon_sprite

ICONS.SS sprite for a Europe-side ship passenger (pool type + profession):
colonists get their working portrait, the five kit types go through
europe_dock_icon_sprite (bugs.md #452), anything else its @UNIT icon.

## europe_pax_type_index

@UNIT type of a transit-box passenger tag: -2 is the Artillery kit, an
out-of-range tag falls back to Colonists (bugs.md #452).

## EuropeIconFlow

Left-to-right, top-to-bottom icon flow inside an Expected / Bound / Loading
water box (ships, each followed by its passengers). One layout for the
hit test and the renderer so a click lands on the icon that was drawn
(duplication audit round 2: the hit test used to skip passengers and so
disagreed with the draw once any ship carried one).

## europe_dock_display_type_index

@UNIT display type for a dock entry, for unit_chrome's box corner. Reads
dos_type (what @ARMOPTIONS moves around), not the profession name, so an
armed/mounted immigrant gets its own corner. -1 only without a pool.

## europe_spawn_dock_mirror_unit

Spawn (or re-kit) the Europe-map mirror unit for a dock immigrant, exactly
as FUN_38fd_0718 does: right @UNIT type, orders = Sentry (DOS +0x314c = 1),
profession stamped, and 100 Tools on a Pioneers-type unit (DOS +0x3159 =
100) — without which a Hardy Pioneer reached the New World empty-handed
(bugs.md). Returns the spawned unit id, or -1.

## europe_cash_treasure

Treasure Train cash-in on Europe arrival.
Cite: Colonization.pdf Treasure Trains; GAME.TXT @LOOTCASH (Crown takes
NUMBER1% share, remainder to treasury); @KINGGALLEON3 (Cortes: share =
current tax rate). Fee = eu->tax_percent — same Crown cut as
europe_sell_proceeds. KINGGALLEON2 non-Cortes share lives in
units_king_galleon_share_pct (FUN_5fef_1908), not here.
Returns gold credited (0 if value <= 0).

## ONE treasury per nation

ONE treasury per nation — the single door onto it.

DOS FUN_38fd_0000(nation) (viceroy_unpacked.c 58695-58703) is the whole of
the Europe module's per-nation state: `DS:0x9e12 = nation` and
`DS:0x84fc = nation*0x13c + 0x8808`, i.e. a POINTER to that nation's
316-byte record. The treasury it spends is that record's 32-bit +0x2a/+0x2c
word (FUN_38fd_2dfe debits it at viceroy 60931-60935; ai_euro.c's 1dfa notes
document the same offset). DOS has no Europe-side copy of it.

The port has two stores: `EuropeScreen.gold` — live for whoever the module
is bound to (`eu->bound_nation`, DS:0x9e12, always the human here) — and
`ColonizeCol1Nation.gold`, live for every other nation and the word that
goes out to the save file. Smell audit G3: readers and writers were split
between them with no per-turn sync, so gold earned outside Europe (colony
plunder, ransom, loot, King gifts, Indian raid drains) landed in the human's
stale copy and was overwritten by the next europe→col1 push, while
gold-gated decisions read a stale purse.

Read with europe_nation_gold: the bound nation answers from `eu->gold`
(live), everyone else from the record. Write with europe_nation_gold_add:
the delta lands on `eu->gold` for the bound nation with the record
re-stamped from it, and on the record alone for everyone else. Both accept
NULL for either store. Outside these two, never assign one store from the
other: an absolute copy is what discards a credit the other store already
took (that is the bug), and the delta form is also what keeps the AI borrow
pattern (units.c / ai_euro.c park an AI treasury in `eu->gold` for one call
and assign it back) from ever touching the human's purse.

## europe_set_live_screen

Register the live Europe screen for the two accessors above, so writers that
legitimately hold only a save — colony-capture plunder (colony.c), combat
ransom/loot (units.c), Indian raid drains — can move the one treasury
without an EuropeScreen threaded through combat. Both accessors fall back to
this pointer when their `eu` argument is NULL; with neither, they degrade to
the record alone (which is what they did before, i.e. the bug). Same
register-once idiom as colonies_set_col1_context / units_set_combat_*.
Pass NULL to unregister. The registered screen must be the LIVE one for the
save being written; a purse borrowed by an AI (units.c / ai_euro.c) is only
ever credited for that AI's own nation, never for the human.

## europe_set_live_save

Register the live save and the popup queue for the two things
europe_cash_treasure does that `eu` alone cannot: book the Crown's fee on
nation+0x22 (and the +0x26 counter) and raise @LOOTCASH as a modal, exactly
as FUN_48d3_06ba raw 78005-78021 does. Pass NULL to unregister; with NULL
the cash-in degrades to the purse alone and shows only the status line.

## europe_gold_stamp_record

Stamp the bound nation's record from the purse.

The five Europe actions that hold no save — europe_recruit_from_pool,
europe_train, europe_purchase, europe_cash_treasure, europe_cheat_add_gold —
can only move `eu->gold`, so the record lags until something writes through
again. All five are reachable from the Europe screen only, which is why the
port already open-coded this stamp at four game_loop sites; this is that
one write, named, so the invariant has a single home. Everything else must
use europe_nation_gold_add instead: a stamp is an absolute copy and will
discard a credit the record took on its own.

## europe_cargo_boycotted_ex

True when `nation` has cargo_type under a Parliamentary boycott.

DOS's one boycott accessor is FUN_38fd_05e8 (viceroy_unpacked.c 59010-59015,
reached from other segments as thunk_FUN_291f_0cd8):
  `return 1 << (cargo & 0x1f) & *(uint *)(*(int *)0x84fc + 0x20);`
— the bound nation record's +0x20 word, the same store the tea party ORs
into (FUN_38fd_3dc8, viceroy 64208/64306) and the buy-back clears
(FUN_38fd_2dfe, viceroy 60943). Every DOS trade path calls it, the
trade-route Europe arrival FUN_479b_0bd0 included (viceroy 77260).

So the authoritative word is `col1->nation[nation].boycott_bitmap`
(nation+0x20), and `EuropeScreen.boycott_bitmap` is only its render-side
mirror — refreshed while the Europe screen draws (game_loop.c
render_europe_screen), which is why the sell paths that never touch the
screen must not read it (smell audit G6). Pass col1 + nation whenever a
save is in hand; the `col1 == NULL` form falls back to the mirror for
tests, chrome and callers with no save bound. Out-of-range cargo_type reads
as not boycotted; a 0xFFFF word (the removed all-cargo-embargo fingerprint,
bugs.md all_boycotted.SAV) reads as no boycott, as both bridge directions
already heal it to 0.

## europe_buyback_boycott

FUN_38fd_2dfe: pay back taxes to lift a Parliamentary boycott on
cargo_type. Cost = eu->cargo[cargo_type].ask * 500 ("500 tons of that
good" — fandom Boycott (Col); GAME.TXT @KISSUP). On success: deducts cost
from gold, credits it to nation.royal_money (Crown REF budget — the DOS
write really does land on that field, see col1_save.h), clears the
boycott bit. Insufficient funds / not boycotted / bad args: no-op,
returns 0. Real trigger: GAME.TXT @SOMEBOYCOTT — click the boycotted
cargo cell on the Europe market strip (game_loop.c EUROPE_HIT_MARKET).
@KISSUP/@KISSSORRY CHOICE dialog chrome PARKED — ported as immediate
action + eu->status line. Returns gold paid (>0) on success.

## europe_buyback_boycott_cost

The same cost without paying it: what @KISSUP quotes in %NUMBER0 before the
player answers. 0 when the cargo is not boycotted or has no price, i.e.
when DOS would not raise the dialog at all.

## Europe price accessors

DOS price accessors (FUN_38fd_0040 / FUN_38fd_0016). `bid` stores the
save-canonical `euro_price` word (nation +0x4c); the port PAYS
`euro_price − 1` when you sell and CHARGES `euro_price + burden` when you
buy (`ask`). Real DOS Europe screen at 1494: Food 0/8, Lumber 1/6,
Silver 19/20 — see original_screenshots/europe/main_with_caravel_*.png.

## europe_cargo_burden

@CARGO burden column for `cargo_type`, cached from the last NAMES.TXT load
(0 when no table has been loaded). For callers that must reproduce the ask
price `euro_price + burden` without an EuropeScreen in hand.

## europe_sell_hold_partial

Partial sale from one hold (shift+drag split, '-' key). Same full path as
europe_sell_hold — boycott gate, tax credit, sale status, volume price
move — for `amount` tons; the hold keeps the remainder (type cleared when
emptied). Returns net proceeds, 0 if nothing sold. Smell audit #52/#53.

## europe_apply_trade_volume

FUN_38fd_1dfa (sell) / FUN_38fd_1d80 (buy) volume ledger, exact:
  term = (amount << volatility) + 1d44(amount)
  1d44 = (difficulty − 2)·16·amount/100 when the seller is human,
         −32·amount/100 for an AI seller (C truncation toward zero);
  every nation's nr[cargo] += term (buy: −=), and on the SELL side only the
  Dutch record (slot 3) gets (term·2)/3 — 1d80 (buy) has no such case;
  seller's tons/tons2 += amount (buy: −=), and gold[cargo]
  += price·amount·(100−tax)/100.
Only the human's record is live in `eu->trade_nr`; `col1` (optional) gets
the seller's tons/tons2/gold ledgers. Verified 2026-08-28 against the
dutch2 t169→t170 pair: three lumber sellers (54 human @ Viceroy, 12 + 18
AI) → +93 on every non-Dutch nr[5], +61 on the Dutch one.
`immediate_threshold` runs the FUN_38fd_0058(0, cargo) single-cargo
rise/fall step the harbor buy/sell path calls afterwards; the Custom
House / AI dump-sell arms do NOT call it (they only get the EOT tick).

## europe_tick_market_prices_w

FUN_38fd_0058 EOT peel (param_2 < 0): optional col1/colonies apply colony
ledger → market_demand_pool half (DS:0x53ea); phases 2–3 nudge trade_nr
(Europe +0x5c pressure) for cargos 9..12 (*100) and 1..4 (no *100); then
nr += attrition per cargo and rise/fall ±1 within [low,high].
Cite: viceroy_unpacked.c FUN_38fd_0058; turn/europe_nation_eot.md.

## europe_tick_immigration_pressure_w design

FUN_38fd_584a / 5e52 phases 4–5: needed_crosses = score; idle +2 until first
dock immigrant; spawn when current > needed (+0x2e/+0x30). Returns 1 if spawned.
Caller adds church crosses to current_crosses first.
Cite: europe_nation_eot.md; TURN1–7 goldens.

## europe_nation_immigration_tick_w

The same DOS FUN_38fd_5e52 tick for a nation with no EuropeScreen — i.e.
every AI nation, whose pool is its own `ColonizeCol1Nation.recruit[3]`
(nation+2..+4) and whose "docks" are the port's (200,100) Europe limbo.
DOS runs 5e52 for every nation from the nation EOT FUN_3844_00f2 (:58375)
and gates only the popups/sound on control == 0. Accrues the 584a tick,
rewrites needed_crosses, and on a crossing empties one pool slot into a
real unit record (FUN_38fd_0718), refills it (FUN_38fd_46d4) and zeroes
the crosses. Returns 1 when an immigrant was created, else 0.

## europe_sell_unit_hold_w

Sell one commodity hold from a map/transport ColonizeUnit into eu->gold.
No harbor UI — proceeds via europe_sell_proceeds (bid × amount × (100−tax)/100).
Cite: Colonization.pdf Europe buy/sell + tax; same Crown cut as harbor
europe_sell_hold / GAME.TXT tax rate path. Clears the hold on success.
The withheld tax is credited to the hold owner's royal_money (DOS
`nation+0x22 += tax`); `col1` may be NULL. Smell audit #51.
Returns gold credited (0 if empty/invalid).

## europe_custom_house_autosell_w

FUN_364b_0688 Custom House auto-sell (colony EOT after production).
Requires Custom House building. Per cargo: mask (0=all eligible) +
FUN_364b_0636 denylist (not Food/Lumber/Horses/Tools/Muskets) + stock>99
→ sell stock-50 (leave 50). Boycott does not block. Tax via eu tax /
nation tax_rate unless WoI (col1 head.market_demand_pool_raw[0]). Credits
col1->nation[n].gold; also eu->gold when n==human_nation.
Returns total gold credited. PARK: per-cargo UI chrome (FUN_15eb_0326).

## europe_custom_house_autosell_ex_w

As above, but also reports each cargo's sale. DOS composes one status line
per cargo inside the same loop (FUN_364b_0688), so the caller needs the
per-cargo numbers, not just the total. `out_count` may exceed `out_max`
only in the sense that extra sales are simply not recorded.

## europe_ai_colony_dump_sell_w

FUN_364b_0688 phase O — AI / non-human Euro dump-sell before spoilage.
For cargo 1..15 with stock > warehouse cap: credit the nation treasury the
full UNTAXED gross `euro_price[nation][cargo] × amount` (the raw DS:0x84BC
byte, no `−1`; no tax split and no royal_money write — viceroy 57834-57846,
unlike the Custom House arm at 57277-57302 which taxes),
apply volume price, leave stock for spoilage clamp. Horses: DOS transfers
surplus to Europe horses word (no gold); muskets in 50-batches then sell
remainder. Cite: colony_eot_production.md O.
Muskets: DOS batches of 50 → Europe musket counter then sell remainder —
thin sells full surplus (counter PARKED). Returns total gold credited.
Cite: viceroy_unpacked.c ~57806–57848; turn/colony_eot_production.md.

## europe_custom_house_cargo_enabled

Is `cargo_type` currently toggled on in this colony's Custom House
per-cargo mask (europe_custom_house_autosell's own enable check,
exposed read-only) — bits==0 (nothing configured) reads as "no cargo
enabled", matching autosell's own behavior. Colony-screen cargo strip
uses this to color a cargo's stock number (green = will be auto-sold
this EOT, matching the DOS golden) — the "per-cargo UI chrome" this
header's europe_custom_house_autosell comment had PARKed.

## europe_harbor_cargo_room

Tons of room for `cargo_type` in a harbor ship — DOS FUN_15eb_3208 via the
FUN_281f_0b96 thunk, the check FUN_38fd_1fa2 runs before charging a buy.
free = @UNIT cargo capacity − goods holds used − passengers; room =
free*100, plus the part-full matching holds ONLY when free == 0. A NULL
`units` falls back to the six-slot maximum. 0 = the "no room" arm.

## europe_buy_unit_cargo_w

Trade-route load list at a Europe stop: buy up to `amount` (≤100) of
cargo_type straight into a map/transport unit's holds. Flat ask price,
boycott gated. Cite: DOS FUN_479b_0bd0 load phase → FUN_38fd_1fa2.
Returns units bought (0 = no gold / boycott / no room).

## europe_hit_test_ex

Like europe_hit_test, but Expected/Bound resolve the ship icon under the pointer
when units + icons are provided (matches Loading/Expected/Bound render layout).
transit_line_h is font line height used for the two-line header (default 8).
