# Scout and ship Automate (port-only)

Divergences, off by default: `settings.json` `divergences.scout_autoexplore` (Scouts)
and `divergences.ship_autoexplore` (ships) (docs/settings.md). Nothing here exists in DOS. Code: `src/core/explore_auto.c`; UI
hooks shared with Pioneer Automate (docs/pioneer_plan.md): `game_loop_eot.c`
(ORDERS action, rotation), `game_loop_update.c` (pacer, village note, no-attack
guard, no sail-for-Europe), `map_menu.c` (row). Test: `unit_explore_auto`.

- ORDERS `Automate` (`Q`), the same row and flag (`ColonizeUnit.automated`, badge `A`)
  as Pioneer Automate; which behaviour runs depends on the unit.

## Ships

- Each decision Goes To the nearest seen ocean tile (water region 1, never a lake,
  `map_tile_is_lake`) next to an unseen ocean tile; unexplored land and lakes do not
  count, a ship cannot clear them. Distance `map_dos_dist`; a target the pathfinder
  cannot reach is skipped.
- A target on the sea lane does not sail the ship to Europe (the high-seas Go To sail
  intent is off for automated units).
- No unseen ocean left in reach = automation ends. Same hostile avoidance as Scouts.

## Scouts

- Each decision (rotation turn, or the pacer when the Scout is idle with moves) Goes
  To the lowest-scoring target on its continent, score = `map_dos_dist`, minus 4 for
  points of interest:
  - a seen Lost City Rumour (entered through the ordinary move tail, so the rumour
    resolves as by hand);
  - a seen native settlement the player is not known to have visited with a Scout,
    and whose tribe is not at war. The final step raises the ordinary entry (woodcut
    + @ACTIONS menu); the player answers it;
  - else a seen land tile next to unexplored ground.
- "Visited" is a proxy: the settlement's skill is known to the nation
  (`village_trade_intel_get_skill`, VTIN; set by @CHIEFHOWDY on a Scout's Speak With
  Chief, but also by @LEARNSTAY / @LEARNDONE when a colonist visits), or automation
  already entered it (`SAUT` set; a dismissed @ACTIONS menu counts). The DOS
  `scouted` bit (settlement record +3 0x08) is deliberately not read: it is one bit
  shared by every nation, set only when the chief's reward roll fires, and never
  shown to the player, so a village another European scouted unbeknownst to the
  player is still visited. Villages scouted in saves from before VTIN read as unvisited.
- Never targets a tile with a foreign unit or colony, or one next to a unit of a
  nation at war with the Scout's (natives: `ai_diplo_indian_at_war`). Go To never
  attacks en route; if the destination turns hostile before arrival, the pacer drops
  the order instead of attacking and the next decision retargets.
- Cancels on: nothing left to explore on its continent, waking or clicking the unit, or any other
  ORDERS command given to it. Aboard a ship it waits and resumes once landed.
- Saved in `SAUT` (docs/savegame.md) with the entered-settlement set; a DOS load keeps
  the current Go To and drops the automation.
