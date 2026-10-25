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


## C2-B4 (low): @VALOR promotion popup "from Free Colonists to Free Colonists", no promotion

- Spring 1698 (turn 305, George Washington in Congress): a Free Colonist
  dragoon (`prof 28`) attacked Tory-held Plymouth, won against Tory colonists
  and captured them. Popup: "Because of their valor in battle, our
  {Dragoons} soldiers have been promoted from {Free Colonists} to {Free
  Colonists} status." The unit is still `prof 28` afterwards.
- Other wins by Free Colonist dragoons in the same war correctly showed
  @VETERAN ("hardened to Veteran status") and set prof 21. The odd one was
  the win over unarmed colonists (the capture path), so a capture probably
  feeds `units_promote_on_win` (src/core/units_combat.c ~775) a winner or
  profession that falls into the @VALOR arm with an unchanged label
  (`units_promote_prof_label` maps everything unknown to Free Colonists).
- Save just before: `saves/attempt3/war3_t305.sav` (Plymouth assault).

## C2-B5 (low): "The Rebel and Tory are now at war." during the War of Independence

- Shown twice after independence had been declared: on the first attack on
  Tory-held Roanoke (1696) and on the first attack on Tory-held Plymouth
  (1698). We had been at war with the Crown since 1676; the DIPLO_WAR message
  for the crown nation reads like a fresh declaration (and "Rebel" / "Tory"
  as nation names reads oddly).

## C2-O2 (observation): Dutch frigates attack English ships without a war

- 1672-1675: after the War of the Spanish Succession handed New Spain to the
  Dutch, Dutch frigates attacked my Caravel (two artillery aboard lost), Frigate
  and Merchantman near Jamestown and Roanoke. My relation with the Dutch was 0
  (never met). There was no declaration and no popup apart from the
  combat results. "The English and Dutch are now at war." only appeared when
  my frigate attacked back.
- The ship-slow code (units_combat_resolve.c ~1090) documents DOS treating
  any non-peace pair as hostile, so AI attacks at "no contact" may be DOS
  behaviour. Worth a DOS check: as a player it looked like an undeclared war.

## C2-O3 (observation): Braves attacking with strength 0

- Several native attacks on the Tory-held Roanoke log `attacker base=1
  strength=0 mods: Attack Bonus +50%` (odds 0%), while other Braves making
  the same attack log strength 12. The zero cases were Braves spawned that
  same turn (`Spawned unit id=350 type=Braves at (41,7)`, then the attack).
  So it is presumably a moves-left scaling, but the Combat Analysis rows
  show nothing that explains the 0.
