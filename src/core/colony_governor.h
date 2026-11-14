#ifndef COLONIZE_COLONY_GOVERNOR_H
#define COLONIZE_COLONY_GOVERNOR_H

/*
 * MOD_COLONY_GOVERNOR (port-only, no DOS counterpart; docs/colony_governor.md).
 * A human colony's ColonizeColony.governor re-seats its
 * colonists among the work plots and buildings it already has, and picks a
 * project when the current one cannot be built, as the human ends the turn
 * (game_do_end_turn). It moves nobody in or out of the colony.
 */
struct ColonizeWorld;

typedef enum ColonyGovernor {
  COLONY_GOVERNOR_NONE = 0,
  COLONY_GOVERNOR_FARMING = 1,
  COLONY_GOVERNOR_CASH = 2,
  COLONY_GOVERNOR_INDUSTRY = 3,
  COLONY_GOVERNOR_POLITICS = 4,
  COLONY_GOVERNOR_COUNT
} ColonyGovernor;

/* One colony, whatever its governor (NONE does nothing). */
void colony_governor_run_w(const struct ColonizeWorld* w, int colony_id);
/* Every governed colony of `nation`; no-op while the mod is off. */
void colony_governor_run_nation_w(const struct ColonizeWorld* w, int nation);

#endif
