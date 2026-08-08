# Marathon7 AI ↔ prereq (2026-08-08)

Continue Alternate/Marathon6 cadence toward playability (cite-only; no invent
gold/join%/LCR tables).

## Landed

### A — AI: Cortes KINGGALLEON3 coastal cash
- `ai_euro_try_cortes_king_galleon_cash`: FF Cortes + Treasure on own coastal
  colony → `europe_cash_treasure` (tax = Crown share) + despawn; no ship.
- Early dispatcher loop + act-level before board/sail.
- Smoke: `smoke_cortes_king_galleon_cash`.
- Cite: fandom Hernan Cortes; GAME.TXT `@KINGGALLEON3`.
- PARK: KINGGALLEON2 non-Cortes share / voyage chrome.

### B — AI: de Witt TRADE_GOODS delivery loop
- Wagon: after load → `AI_MOVE` own colony → `colonies_transfer_from_unit` unload.
- Ship: with TRADE_GOODS → `AI_SAIL` Europe (existing dump-sell).
- Smoke extended: load + deliver.
- Cite: fandom Jan de Witt; existing transfer APIs.

### C — Prereq: Stockade/Fort/Fortress land defense
- `colonies_fortification_defense_bonus_percent` (100/150/200).
- `units_set_combat_colonies` + apply in `units_resolve_land_combat_ff`
  (replaces fortify ×2 on fortified colony tile).
- `units_try_move` / `ai_euro_dispatcher_turn` set context.
- Smoke in `test_units`.

### D — Prereq: Treasure combat capture
- Win vs Treasure → credit LE16 hold gold to winner treasury; despawn.
- Cite: FUN_5fef_1908 / `@LOOTCAPTURE`. PARK: ransom dialog chrome.

### Fixes / polish
- Missionary CONTACT gated on **Euro peer war** only (not false
  `indian_war_hunt` from `relation_by_indian==0` after memset/euro_balance).
- Missionary smoke sets Indian relations to content floor 100.
- Stockade+ voluntary min pop **2→3** (fandom Colony/Stockade).

## Still PARKED
Deep `20e6` combat×8; Sepulveda join%; full LCR RNG; KINGGALLEON2; FA F2–F9;
coastal fort naval fire.

## Also (same session)
- EOT Treasure outside-colony tick (`units_tick_treasure_outside_colony`,
  FUN_3844_0004; >8 → despawn). Wired from turn refresh Euro/human.
- Shared `units_cortes_cash_coastal_treasures` for AI + human turn finish.
- AI peace construction prefer: Stockade→**Fort→Fortress**→Warehouse→Docks.
- SoL ≥50%/+1 and =100%/+2 on field yields + carpenter hammers (+ thin bells/crosses).
  Craft manufacturing SoL deepen + Tory −1 still PARK.
