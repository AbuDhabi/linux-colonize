# Scout Automate (port-only)

Divergence, off by default: `settings.json` `divergences.scout_autoexplore`
(docs/settings.md). Nothing here exists in DOS. Code: `src/core/scout_auto.c`; UI
hooks shared with Pioneer Automate (docs/pioneer_plan.md): `game_loop_eot.c`
(ORDERS action, rotation), `game_loop_update.c` (pacer, village note, no-attack
guard), `map_menu.c` (row). Test: `unit_scout_auto`.

- ORDERS `Automate` (`Q`), the same row and flag (`ColonizeUnit.automated`, badge `A`)
  as Pioneer Automate; which behaviour runs depends on the unit (a Scout explores).
- Each decision (rotation turn, or the pacer when the Scout is idle with moves) Goes
  To the lowest-scoring target on its continent, score = `map_dos_dist`, minus 4 for
  points of interest:
  - a seen Lost City Rumour (entered through the ordinary move tail, so the rumour
    resolves as by hand);
  - a seen native settlement the nation has not visited: its skill is still unknown
    (`village_trade_intel_get_skill`, set by @CHIEFHOWDY), automation has not entered
    it before, and the tribe is not at war. The final step raises the ordinary
    entry (woodcut + @ACTIONS menu); the player answers it;
  - else a seen land tile next to unexplored ground.
- Never targets a tile with a foreign unit or colony, or one next to a unit of a
  nation at war with the Scout's (natives: `ai_diplo_indian_at_war`). Go To never
  attacks en route; if the destination turns hostile before arrival, the pacer drops
  the order instead of attacking and the next decision retargets.
- Cancels on: nothing left to explore, waking or clicking the unit, or any other
  ORDERS command given to it. Aboard a ship it waits and resumes once landed.
- Saved in `SAUT` (docs/savegame.md) with the entered-settlement set; a DOS load keeps
  the current Go To and drops the automation.
