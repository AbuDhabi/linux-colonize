# colony.h design notes: warehouse and trade

Split out of [colony.md](colony.md) on 2026-10-23 (doc-length policy): the
warehouse, spoilage, foreign-colony trade and trade-route sections, i.e. the
`src/core/colony_goods.c` API. Same contract as colony.md: each section matches a
one-line pointer comment in `src/core/colony.h` naming
`docs/colony_goods.md#<anchor>`.

## colonies_warehouse_capacity

Warehouse capacity (100 base; +100 Warehouse; +100 Expansion).
FUN_15eb_0a50 takes no cargo argument — one capacity for all sixteen goods,
Food included. Food's exemptions live at the three sites that consume the
cap (EOT spoilage, @WAREHOUSEFULL unload confirm, colony-screen alert ink),
not here; cargo_type is unused by the body (`(void)` cast) and is kept only
so the ~10 call sites stay readable (audit CO-28).

## colonies_emit_warehouse_full_chrome

Human unload chrome: GAME.TXT @WAREHOUSEFULL when warehouse has no room.
cargo_name optional (miss = empty string). No-op if ai_popups NULL, and for
Food (FUN_479b_0f60 never warns on Food).
@WAREHOUSEFULL's numbers (port_bugs.md #433): NUMBER0 = what the warehouse
already held BEFORE this deposit, NUMBER1 = capacity, NUMBER2 = the
deposit itself. `deposited` = units the player just unloaded (or tried
to); `already_included` = how much of that has already been added to
colony->stock by the time this is called (0 when the transfer failed).

## colonies_specialty_cargo_update

FUN_5952_0306: set/clear specialty_cargo (+0x8d).
want_set cleared when stock >= warehouse capacity, or when the colony
already produces the cargo (DS:0x8dc8 gross-production ledger != 0 —
`already_produced`). The only DOS caller is FUN_5952_035e's five-call
build-preference block (ai_euro_5952_build_pref_0306).
Cite: viceroy_unpacked.c:93760 FUN_5952_0306; FUN_15eb_0a50 capacity.

## colonies_apply_warehouse_spoilage

EOT spoilage: clamp each stock (Food excepted) to warehouse capacity
(FUN_364b_0688's cargo loop). Call after production + Custom House (wiki:
auto-sell before spoilage).

`stock_before` is this colony's per-cargo stock as it stood *before* this
turn's production, COLONIZE_CARGO_COUNT entries, or NULL to treat the whole
overflow as pre-existing. DOS only *reports* the part of the overflow that
predates production — goods a colony simply out-produced its warehouse for
are clamped silently. Returns the reportable total (0 when nothing but
production overflowed, or when the loss is under 2 tons).

A pre-existing overflow of exactly 1 ton is removed but not reported. DOS
zeroes the loss below 2 tons before the subtract, so that ton stays in the
warehouse forever; the port does not reproduce that ([dos_bugs.md](dos_bugs.md)
DB-34, FUN_364b_0688 raw 57847-57868).

When out_first_cargo != NULL and any reportable spoil occurs, writes the
first such cargo index. When out_type_count != NULL, writes how many
distinct cargo types spoiled reportably.

## Foreign-colony trade (Jan de Witt)

Foreign-colony trade — DOS FUN_5f7a_020e (raw 98885-99051). The Jan de Witt
(FF 4) mechanic, and the ONLY one: a human-controlled Euro nation's cargo
unit bumping a foreign Euro colony haggles one hold away for gold or barter,
never enters, and always loses its whole MP allotment.
Spec: docs/foreign_colony_trade.md.

## colonies_foreign_trade_gate

raw 98915-98936. `unit_id` must stand on / be stepping onto the tile of
`foreign_colony_id`; the caller (the move handler) owns that test, as DOS's
FUN_465b_0000 does.

## colonies_foreign_trade_prepare

raw 98963-99012: price the hold and find the colony's best counter-offer.
Draws once from `rng` (FUN_281f_04d4(10, (difficulty+1)*12)) unless the WoI
intervention-ally arm applies. Returns 0 (and leaves *out zeroed) when the
gate is not OK or the hold is empty.

## colonies_foreign_trade_apply

raw 99014-99049. `take_goods` != 0 → the hold becomes (offer_cargo,
offer_qty) (FUN_281f_0cea/0ca4); else the hold is emptied (FUN_281f_0aec)
and `deal->gold` is credited to the unit's nation. Both arms then do
`colony.stock[sold_cargo] += sold_qty`. On the barter arm the port also
debits the handed-over goods from the colony (clamped at 0); DOS never does
([dos_bugs.md](dos_bugs.md) DB-13). Returns 1 when applied.

## colonies_trade_route_service_stop

TRADE stop cargo at own colony — DOS FUN_479b_0bd0 arrival body:
unload exactly the stop's unload-list cargos (all matching holds), then
load exactly the load-list cargos greedily by colony stock (highest stock
first, 100-unit holds) until the transport is full or stock runs out.
Empty lists move nothing — no unload-all / surplus-ladder fallback.
Returns 1 if any transfer happened. Cite: ColonizeCol1TradeStop.

## colonies_trade_stop_autofill

Thin TRADE Edit helper: fill unload nibbles from selected unit holds;
for a colony stop, fill load nibbles from surplus ladder (tools…food).
Europe (colony NULL): unload only. Cite: ColonizeCol1TradeStop.
