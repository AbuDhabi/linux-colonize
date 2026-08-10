#include "core/col1_stuff_census.h"

#include <string.h>

#include "core/units.h"

bool col1_stuff_census_window_is_blank(const ColonizeCol1Stuff* stuff) {
  if (!stuff) {
    return true;
  }
  /* File offs 12..139 (+944e sibling): all zero ⇒ blank template. */
  const uint8_t* p = (const uint8_t*)&stuff->all_unit_counts[0];
  const uint8_t* end = (const uint8_t*)&stuff->unknown_ds_947e[0];
  for (; p < end; ++p) {
    if (*p != 0) {
      return false;
    }
  }
  for (size_t i = 0; i < sizeof(stuff->unknown_ds_944e); ++i) {
    if (stuff->unknown_ds_944e[i] != 0) {
      return false;
    }
  }
  return true;
}

static void col1_stuff_census_tally_units(
  ColonizeCol1Stuff* stuff,
  const ColonizeUnitPool* units
) {
  memset(stuff->all_unit_counts, 0, sizeof(stuff->all_unit_counts));
  memset(stuff->free_colonist_counts, 0, sizeof(stuff->free_colonist_counts));
  memset(stuff->census_pop_proxy, 0, sizeof(stuff->census_pop_proxy));
  memset(stuff->land_combat_totals, 0, sizeof(stuff->land_combat_totals));
  memset(stuff->ship_cargo_totals, 0, sizeof(stuff->ship_cargo_totals));
  memset(stuff->ship_counts, 0, sizeof(stuff->ship_counts));
  memset(stuff->land_combat_strength, 0, sizeof(stuff->land_combat_strength));
  memset(stuff->armed_ship_counts, 0, sizeof(stuff->armed_ship_counts));
  memset(stuff->field_combat_totals, 0, sizeof(stuff->field_combat_totals));
  memset(stuff->unit_type_counts, 0, sizeof(stuff->unit_type_counts));

  if (!units) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active) {
      continue;
    }
    const int n = u->nation_id;
    if (n < 0 || n > 3) {
      continue;
    }
    if (stuff->all_unit_counts[n] < 255u) {
      stuff->all_unit_counts[n]++;
    }
    const int t = u->type_index;
    if (t >= 0 && t < 19 && stuff->unit_type_counts[n][t] < 255u) {
      stuff->unit_type_counts[n][t]++;
    }
    if (t == 0 && stuff->free_colonist_counts[n] < 255u) {
      stuff->free_colonist_counts[n]++;
    }
    if (u->profession >= 0 && u->profession != UNITS_JOB_NONE &&
        stuff->census_pop_proxy[n] < 255u) {
      stuff->census_pop_proxy[n]++;
    }
    const ColonizeUnitType* ut = units_type(units, t);
    if (ut && ut->domain == COLONIZE_UNIT_DOMAIN_SEA) {
      if (stuff->ship_counts[n] < 255u) {
        stuff->ship_counts[n]++;
      }
      const unsigned cargo = (unsigned)(ut->cargo < 0 ? 0 : ut->cargo);
      unsigned sum = (unsigned)stuff->ship_cargo_totals[n] + cargo;
      if (sum > 255u) {
        sum = 255u;
      }
      stuff->ship_cargo_totals[n] = (uint8_t)sum;
      if (ut->attack > 0 && stuff->armed_ship_counts[n] < 255u) {
        stuff->armed_ship_counts[n]++;
      }
    } else if (ut && ut->domain == COLONIZE_UNIT_DOMAIN_LAND) {
      if (ut->attack > 0 || ut->defense > 0) {
        if (stuff->land_combat_totals[n] < 255u) {
          stuff->land_combat_totals[n]++;
        }
        unsigned str = (unsigned)stuff->land_combat_strength[n] + (unsigned)ut->attack +
                       (unsigned)ut->defense;
        if (str > 0xffffu) {
          str = 0xffffu;
        }
        stuff->land_combat_strength[n] = (uint16_t)str;
        if (u->aboard_ship_id < 0 && stuff->field_combat_totals[n] < 255u) {
          stuff->field_combat_totals[n]++;
        }
      }
    }
  }
}

static void col1_stuff_census_tally_colonies(
  ColonizeCol1Stuff* stuff,
  const ColonizeColonyPool* colonies,
  int add_pop_to_proxy
) {
  memset(stuff->colony_counts, 0, sizeof(stuff->colony_counts));
  memset(stuff->colony_pop_totals, 0, sizeof(stuff->colony_pop_totals));
  if (!colonies) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (!c->active) {
      continue;
    }
    const int n = c->nation_id;
    if (n < 0 || n > 3) {
      continue;
    }
    if (stuff->colony_counts[n] < 255u) {
      stuff->colony_counts[n]++;
    }
    const unsigned pop = (unsigned)(c->colonist_count < 0 ? 0 : c->colonist_count);
    unsigned pt = (unsigned)stuff->colony_pop_totals[n] + pop;
    if (pt > 255u) {
      pt = 255u;
    }
    stuff->colony_pop_totals[n] = (uint8_t)pt;
    if (add_pop_to_proxy) {
      unsigned proxy = (unsigned)stuff->census_pop_proxy[n] + pop;
      if (proxy > 255u) {
        proxy = 255u;
      }
      stuff->census_pop_proxy[n] = (uint8_t)proxy;
    }
  }
}

static void col1_stuff_census_write_mean_pop(ColonizeCol1Stuff* stuff) {
  for (int n = 0; n < 4; ++n) {
    uint16_t avg = 0;
    if (stuff->colony_counts[n] > 0) {
      avg = (uint16_t)((unsigned)stuff->colony_pop_totals[n] / (unsigned)stuff->colony_counts[n]);
    }
    stuff->unknown_ds_944e[n * 2] = (uint8_t)(avg & 0xffu);
    stuff->unknown_ds_944e[n * 2 + 1] = (uint8_t)((avg >> 8) & 0xffu);
  }
}

void col1_stuff_census_fill_blank(
  ColonizeCol1Stuff* stuff,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies
) {
  if (!stuff) {
    return;
  }
  col1_stuff_census_tally_units(stuff, units);
  col1_stuff_census_tally_colonies(stuff, colonies, 1);
  col1_stuff_census_write_mean_pop(stuff);
}

void col1_stuff_census_refresh_colony_counts(
  ColonizeCol1Stuff* stuff,
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units
) {
  if (!stuff) {
    return;
  }
  if (units) {
    col1_stuff_census_tally_units(stuff, units);
  }
  /* With units: also fold colony pop into census_pop_proxy (fill_blank shape). */
  col1_stuff_census_tally_colonies(stuff, colonies, units != NULL);
  col1_stuff_census_write_mean_pop(stuff);
}
