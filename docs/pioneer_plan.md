# Pioneer plan and Automate (port-only)

Divergence, off by default: `settings.json` `divergences.pioneer_plan`
(docs/settings.md). Nothing here exists in DOS. Code: `src/core/pioneer_plan.c`
(sim: plan grid, validity, reconcile, Automate, `PPLN` chunk); UI hooks in
`game_loop_eot.c` (menu actions, rotation), `game_loop_update.c` (pacer, per-frame
reconcile), `game_loop.c` (overlay, menu context), `map_menu.c` (rows).
Test: `unit_pioneer_plan`.

## Plan Pioneer view

- VIEW row `Plan Pioneer: On/Off` (`N`), right after View Pieces. Turning it on enters
  View Pieces; any return to Move Pieces turns it off.
- While on, ORDERS Clear Forest / Plow Fields / Build Road (menu or `P` / `R`) edit
  the plan of the cursor tile instead of ordering a unit:
  - forest: Clear -> Clear + Plow -> nothing;
  - open unplowed land: Plow toggles;
  - Road toggles where there is no road, colony or village;
  - Hills / Mountains: Road only; water, unexplored tiles, native settlements and
    rival colonies take nothing.
- Overlay, in a cyan remap of the map palette: plowed-field art (PHYS0 149) for
  Plow, road art (PHYS0 80 / 81+d) for Road joined to real and planned roads (real
  roads get cyan stubs toward planned ones; outside the view nothing changes), the
  Tools cargo icon centred for Clear.
- Reconcile: every frame, tiles the player has in sight (own units' sight radius,
  own colonies radius 1) drop plan bits that are done or impossible (a cleared
  forest keeps its Plow). Tiles out of sight keep a stale plan until seen.

## Automate

- ORDERS `Automate` (`Q`) for a Pioneer with at least
  20 tools. The order box shows `A` (plan-glyph slot, `unit_chrome_unit_badge_index`).
- Each decision (rotation turn, or the pacer when the unit is idle with moves): on a
  planned tile it starts Road, then Clear, then Plow through `units_pioneer_road_w` /
  `units_pioneer_plow_w` (same cost, tools, lumber, native anger as by hand);
  otherwise it Goes To the nearest planned tile on its continent (`map_dos_dist`),
  skipping tiles another automated Pioneer has claimed and Clear/Road jobs on tribal
  land that would raise @INDIANFOREST / @INDIANROAD (left for the player).
- Cancels on: no tools / no longer a Pioneer, no reachable plan, waking or clicking
  the unit (`units_wake`), or any other ORDERS command given to it.
- Saved in `PPLN` with the plan (docs/savegame.md); a DOS re-save drops both.
