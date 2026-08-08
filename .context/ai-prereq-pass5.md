# AI prerequisites pass 5 (2026-08-08)

- Started epoch: 1786176483
- Deadline: +1h → 1786180083
- Goal: non-AI hooks that unblock AI (post-Marathon4 blockers)
- Result: **Done** — APIs + call-site wire; ctest **38/38**

## Priority unlocks (cited / no invent)
1. `lumber_short` inventory + Expert Lumberjack Europe dock hire — **Done**
2. Dump-goods Europe-bid price weights (`local_7a` stand-in via live `europe.cargo[].bid`) — **Done**
3. Native settlement-conquer structural API (tribe remove + tension); Cortes treasure spawn only with cited amount path — else PARK amount — **Done**
4. Thin LCR resolve scaffold gated by de Soto (clear rumour + always-positive branch) — **Done**
5. Wire thin AI / human call sites + smokes — **Done**

## Call-site wiring (post-API)
- `turn_refresh_moves_for_nation(..., map)` → `units_set_native_fallout_context(col1, map, -1)`
- Human `game_try_unit_move` arms the same fallout + FF context
- `game_after_unit_action` → Scout on rumour → `units_resolve_lcr_rumour`
- `ai_euro_unit_act` Scout on rumour → same LCR resolve

## Still PARKED
FUN_5fef_31ea conquest gold amounts; full FUN_65dd_0004 LCR RNG table;
Sepulveda join %; deep `20e6`/`2820`/`4528`; dump modal CHOICE; VGA F2–F9;
rumour-cleared bit is layer2 `0x08` stand-in.
