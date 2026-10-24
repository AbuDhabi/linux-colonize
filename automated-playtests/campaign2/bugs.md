# Campaign 2 bugs and oddities

Numbered C2-Bn. Severity: high = loses the game / blocks play, medium = wrong
rules or misleading UI, low = cosmetic or driver-only.

## C2-B1 (high): Escape on the main map quits to the title menu, no confirm

- 1502 (turn 10), right after "Set sail for the New World" in London the game
  returned to the main map; I pressed Escape (expecting to leave Europe) and the
  game went straight to the title menu. No "are you sure", no way back: the
  unsaved turn was lost (reloaded `saves/trace_autosave_turn_010.sav`).
- Code: `game_update_map_keys` (src/core/game_loop_update.c ~3180) sets
  `game->in_menu = true` on any Escape that is not cancelling a goto drag.
- DOS: Esc on the map does not throw the game away (needs checking what it does;
  the Game menu has the quit entries with a confirm).
- Hit again in 1619 (turn 146): Escape closes the colony screen, and a second
  Escape (natural "back, back") lands on the map and quits. The game state is
  still in memory after the quit (the driver's `save` from the title menu kept
  everything), but a player has no way back to it from the title menu.
- The play driver now refuses `key Escape` on the bare main map (`key Escape
  force` overrides).

## C2-B2 (medium): Treasure arriving by Go-To gets no King's Galleon offer

- 1518 (turn 26): the 5800 Cibola treasure walked to Jamestown (coastal, no
  galleon owned) under a Go To order. It arrived, but no @KINGGALLEON2 offer
  came up; the treasure just sits in the colony with "No Orders".
- Code: the offer is raised only from the tail of `game_try_unit_move`
  (src/core/game_loop_orders.c ~1072, "FUN_465b_0000 tail"). The goto pacer
  (`game_pacer_goto_step`, game_loop_update.c ~735) only hands a step to
  `game_try_unit_move` for village / hostile destinations; ordinary steps move
  silently, so the final step into the colony never reaches the offer.
  The pacer's own comments say DOS routes every goto step through
  FUN_465b_0000, so DOS would raise the offer on arrival.
- Workaround: move the treasure out of the colony and back in by hand.

## C2-O1 (observation): every Lost City Rumour in a game rolls the same outcome

- 1495-1543: 10 rumours explored (2 Seasoned Scouts, 1 pioneer). Nine gave
  "You find nothing but rumors."; the one on a mountain gave Cibola (5800).
- Cause: `units_lcr_explore` reseeds the LCG with `dos_rng_reseed_83a6`
  (FUN_281f_04ca, DS:0x83a6) right before `units_lcr_roll_outcome`, and the port
  keeps DS:0x83a6 = `game->ai_rng_seed`, fixed for the whole game (new game:
  wall clock or `--seed`; load: RTST). So every roll draws the same numbers and
  the outcome depends only on terrain / explorer skill / de Soto. In this game
  the fixed raw roll is case 2 (Cibola) with gate >= 25: Cibola wherever the
  terrain qualifies (hills, mountains, desert / scrub forest, at most 7 per
  game), plain "nothing" everywhere else.
- docs/settings.md presents the replay as DOS behaviour (`divergences.modern_prng`
  is the opt-out). If real DOS updates DS:0x83a6 from the timer, rumours there
  vary and this is a port divergence worth a DOSBox check: in practice a player
  either never finds anything or always does.

## C2-B3 (low): "independence from New England" in the SoL 50% message

- 1605: "Sons of Liberty membership in Jamestown is up to 50% ... A majority of
  the colonists there support the idea of independence from **New England**!"
  New England is the name I gave the new land at Land Ho; the mother country is
  England. The rebel-sentiment message (@REBELUP) in the same game says
  "independence from England".
- Code: `turn_report_colony_sol_message` (src/core/turn_production.c ~190) fills
  %STRING1 of @REBELMAJORITY / @REBELUNANIMOUS / @TORY* from
  `col1_player_country_name`, i.e. the player record's new-land name, and only
  falls back to `europe->nation_name`. Worth checking which DS string DOS
  passes; the @REBELUP path (ai_king_war.c) produces the nation name.

