#ifndef COLONIZE_COLONY_CRAFT_H
#define COLONIZE_COLONY_CRAFT_H

#include "core/colony.h"
#include "core/turn.h"

/*
 * Settlement manufacturing: workplace colonists convert warehouse raw → goods.
 * Called from turn production after field harvest, before carpenter hammers.
 */
void colony_craft_one_colony(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeColonyProdDelta* delta
);

#endif
