# Foreign-colony trade (Jan de Witt)

DOS `FUN_5f7a_020e` (`viceroy_unpacked.c` raw 98885-99051; asm `CODE_128:5f7a:020e`,
`viceroy_unpacked.asm` 166656-167116). Owner of the Jan de Witt (FF 4) mechanic.
FF-4 is read in exactly two places in VICEROY: raw 98928 (here) and the Foreign
Affairs report (`FUN_3f41_2548`).

Before 2026-09-17 the port carried a fandom-sourced invention (foreign dock +
free cargo transfer + AI wagon/ship trade acts). That is deleted; nothing below
is inferred.

## Entry

`FUN_465b_0000` (raw 75491-75500), the move handler, on a step onto a tile that
holds a settlement:

```
if (bVar4 && mover_nation < 4 && tile_owner < 4) {          /* foreign Euro */
  cid = FUN_281f_07be(dst_x, dst_y);                        /* colony at tile */
  if (cid >= 0 && FUN_2a1f_015e(unit, cid)) goto abort_move;
}
```

`FUN_2a1f_015e` → `FUN_5f7a_0662(unit, colony)` (raw 99053):

* binds the colony record (`FUN_281f_09e6` → `DS:0x8542`);
* unit type `5` (Scouts) → `FUN_5f7a_000e` (`@SCOUTCOLONY`, ported, bugs.md #438);
* else unit-type table byte `DS:0x5237[type*0xe]` (**cargo capacity**) non-zero →
  `FUN_5f7a_020e` — i.e. every Wagon Train and every ship;
* non-zero result → `FUN_281f_0934(unit)` (spend the **whole** MP allotment) and
  the move is aborted: **the unit never enters the foreign colony.**
* WoI tail: if nothing above fired, the game is in the War of Independence, the
  colony owner is not a human-controlled Euro and is not the Crown
  (`DS:0x53d2`), and the mover is a human-controlled Euro → `@NOWARSDURINGREV`
  (tag `0x1af3`) and abort+spend.

`FUN_5f7a_020e` **always returns 1**, so a cargo unit bumping a foreign colony
always loses its whole allotment, even when all it got was a refusal popup.

## FUN_5f7a_020e

`actor` = unit nation nibble (`unit+0x3147 & 0xf`), `owner` = colony `+0x1a`.

1. `actor > 3` → return (natives).
2. `player[actor].control != 0` → return. **The AI never trades this way** —
   there is no other FF-4 site, so AI foreign-colony trade does not exist in DOS.
3. `FUN_281f_0a38(actor, owner) & 0x40` (peace treaty) clear → `@TRADEATWAR`
   (`0x1aa0`), return.
4. `FUN_281f_07b4(actor, 4)` (Jan de Witt) == 0 → `%STRING0` = `@LEADER2[owner]`
   (`FUN_2a1f_0618(0, 0x1aab, owner)`), `@TRADEMERCANTILISM` (`0x1ab3`), return.
5. `unit+0x3150` (holds occupied) == 0 → `@TRADENOCARGO` (`0x1ac5`), return.
6. holds > 1 → list dialog `@TRADEWHICH` (`0x1ad2`) of `"<qty> <Cargo>"` rows,
   ids `1..n`, plus a final row with id **99** taken from `DS:0x2dfa` (cancel).
   Result `0` or `99` → return; otherwise `hold = result - 1`. One hold → `hold = 0`.

### Price (gold offer)

```
sold  = cargo type in hold;  qty = hold amount
p(n,c)= DS:0x84bc[n*0x10 + c]            /* = max(0, nation[n].euro_price[c] - 1) */
gross = p(owner, sold) * qty
gold  = gross - gross * nation[owner].tax_rate / 100
if (!WoI && (FUN_281f_0a38(actor, owner) & 0x02))   gold >>= 1      /* at war */
if (!WoI || owner != DS:0x53d4)                                      /* 0x53d4 = intervention ally */
     gold += FUN_281f_04d4(10, (difficulty+1)*12) * gold / -100      /* haggle down 10..(d+1)*12 % */
else gold += (-5 - difficulty)             * gold /  100             /* ally: only 5+d % down */
if (gold < 1) gold = 1
```

### Counter-offer (goods)

```
best = none
for c in 0..15:
  if (!WoI && c in {0 Food, 5 Lumber, 14 Tools, 15 Muskets}) continue
  if (c == sold) continue
  avail = min(colony.stock[c], 100)
  if (WoI && sold in {15 Muskets, 8 Horses}) avail = (owner == DS:0x53d4) ? 100 : max(avail, 50)
  val = p(owner, c) * avail
  while (val > gross) { avail--; val -= p(owner, c); }
  if (avail > 0 && val > best.val) best = (c, avail, val)
```

No `best` → `%STRING0`/`%NUMBER0` = sold cargo/qty, `@TRADENOWANT` (`0x1add`), return.

### The offer

`@TRADEWITH` (`0x1ae9`), `%STRING0`/`%NUMBER0` = offered cargo/qty,
`%STRING1`/`%NUMBER1` = sold cargo/qty, `%NUMBER2` = gold. Choices are 1-based:

| choice | GAME.TXT | effect |
|---|---|---|
| 1 | "We'll take the {%NUMBER0 %STRING0}." | `FUN_281f_0cea/0ca4`: the hold becomes `(offer_cargo, offer_qty)` |
| 2 | "Just give us the {%NUMBER2$}." | `FUN_281f_0aec` empties+compacts the hold; `nation[actor].gold += gold` |
| >2 | "Do you take us for fools?" | nothing |

Both accepting arms then do `colony.stock[sold] += qty`.

**DOS never debits the colony's stock of the cargo it hands over** (raw
99021-99049 writes only `+0x9a + sold*2`). That is transcribed, not fixed.

## Port

* Sim: `colonies_foreign_trade_gate` / `_prepare` / `_apply` (`colony.c`,
  `colony.h`), tested by `unit_colonies` (`tests/unit/test_colonies.c`).
* UI: `game_move_native_and_scout_prompts` (`game_loop.c`) raises
  `AI_POPUP_TAG_FOREIGN_TRADE_WHICH` / `_OFFER`; results are applied in
  `game_dialogs.c`. Both sit in the shared `ai_popups` queue, so
  `game_modal_open` already covers them.
* Esc on `@TRADEWITH` is DOS-literal (raw 99024): any result `< 3` other than 1
  takes the gold arm, so Esc sells for gold.
* `@TRADEWHICH`'s cancel row text `DS:0x2dfa` = LABELS `@MISC[32]` = "Nothing".
