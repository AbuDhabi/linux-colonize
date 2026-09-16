#ifndef COLONIZE_CORE_EUROPE_ART_H
#define COLONIZE_CORE_EUROPE_ART_H

/*
 * Internal seam between europe.c (SIM: market, pool, docks, voyages) and
 * europe_art.c (UI: EUROPE.PIK / WOODTILE.SS load + free), created by the
 * 2026-09-16 sim/UI split. europe_load / europe_free stay declared in
 * europe.h; this header only exposes the two helpers they call that used to
 * be `static` in the one file.
 */

#include <stdbool.h>

#include "core/assets.h"
#include "core/europe.h"

bool europe_load_tables(EuropeScreen* eu, const ColonizeMsgCatalog* names);
void europe_init_purchase_table(EuropeScreen* eu);

#endif /* COLONIZE_CORE_EUROPE_ART_H */
