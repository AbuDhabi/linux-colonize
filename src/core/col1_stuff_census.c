#include <stdio.h>
#include <stdlib.h>
#include "core/col1_stuff_census.h"

#include <string.h>

#include "core/ai_contact.h"
#include "core/combat_strength.h"
#include "core/map.h"
#include "core/units.h"

bool col1_stuff_census_window_is_blank(const ColonizeCol1Stuff* stuff) {
  if (!stuff) {
    return true;
  }
  /* File offs 12..139 (+944e sibling): all zero ⇒ blank template. */
  const uint8_t* p = (const uint8_t*)&stuff->all_unit_counts[0];
  const uint8_t* end = (const uint8_t*)&stuff->village_counts_by_continent[0];
  for (; p < end; ++p) {
    if (*p != 0) {
      return false;
    }
  }
  for (size_t i = 0; i < sizeof(stuff->avg_colony_pop); ++i) {
    if (stuff->avg_colony_pop[i] != 0) {
      return false;
    }
  }
  return true;
}


static void census_sat_add(uint8_t* b, int v) {
  int t = (int)*b + v; /* FUN_4962_0006: AX += byte, clamp at 0xff */
  *b = (uint8_t)(t > 0xff ? 0xff : t);
}

/* bind_origin = the raw 78193-78197 unit write; the blank-template fill on the
 * save-writing path passes false so a save never mutates the live pool. */
static void census_4962(const ColonizeWorld* w, ColonizeCol1Stuff* stuff, int n, bool bind_origin) {
  const ColonizeUnitPool* units = w->units;
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeCol1Save* col1 = w->col1;
  const ColonizeWorldMap* map = w->map;
  /* raw 78129-78158: this nation's slices, and all sixteen 0x95f2 bytes. */
  if (getenv("AI_28C8_TRACE")) fprintf(stderr, "[census] n%d (old land %d)\n", n, (int)stuff->land_combat_strength[n]);
  stuff->land_combat_strength[n] = 0;
  stuff->all_unit_counts[n] = 0;
  stuff->colony_counts[n] = 0;
  stuff->free_colonist_counts[n] = 0;
  stuff->colony_pop_totals[n] = 0;
  stuff->census_pop_proxy[n] = 0;
  stuff->land_combat_totals[n] = 0;
  stuff->ship_cargo_totals[n] = 0;
  stuff->ship_counts[n] = 0;
  stuff->armed_ship_counts[n] = 0;
  stuff->field_combat_totals[n] = 0;
  memset(stuff->unit_type_counts[n], 0, sizeof(stuff->unit_type_counts[n]));
  int pop_sum = 0; /* DS:0x944e word */
  for (int c = 0; c < 16; ++c) {
    const int k = n * 16 + c;
    stuff->continent_presence_flags[c] = 0;
    stuff->land_unit_counts_by_continent[k] = 0;
    stuff->colony_counts_by_continent[k] = 0;
    stuff->skilled_unit_counts_by_continent[k] = 0;
    stuff->unit_value_sum_by_continent[k] = 0;
    stuff->combat_value_sum_by_continent[k] = 0;
    stuff->field_combat_strength_by_continent[k] = 0;
  }
  ColonizeCombatStrengthCtx sctx;
  memset(&sctx, 0, sizeof(sctx));
  sctx.units = units;
  sctx.colonies = colonies;
  sctx.col1 = col1;
  /* Unit array, record order (raw 78159). */
  for (int i = 0; units && i < units_slot_end(units); ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active) {
      continue;
    }
    /* FUN_281f_081c: the unit's tile continent; a passenger sits at the
     * (-2,-2) sentinel and water/off-map reads -1. */
    const int cont = (map && u->aboard_ship_id < 0 && units_is_on_map(u))
                       ? map_continent_id_at(map, u->x, u->y)
                       : -1;
    const int owner = u->nation_id & 0xf;
    if (owner != n) {
      if (cont >= 0 && cont < 16 && owner < 4) {
        stuff->continent_presence_flags[cont] |= 2; /* raw 78234-78235 */
      }
      continue;
    }
    const int t = u->type_index;
    const bool hull = t >= 0xd && t <= 0x12;
    const ColonizeUnitType* ut = units_type(units, t);
    if (t >= 0 && t < 0x13) {
      census_sat_add(&stuff->unit_type_counts[n][t], 1);
    }
    if (hull) {
      census_sat_add(&stuff->ship_cargo_totals[n], ut && ut->cargo > 0 ? ut->cargo : 0);
      census_sat_add(&stuff->ship_counts[n], 1);
      if (ut && ut->attack > 0) {
        stuff->armed_ship_counts[n]++; /* INC, DS:0x5236 != 0 */
      }
    } else if (cont >= 0 && cont < 16 && ut && ut->defense > 1 &&
               (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) &&
               !(colonies && colonies_id_at(colonies, u->x, u->y) >= 0)) {
      stuff->continent_presence_flags[cont] |= 8; /* raw 78167-78180 */
    }
    /* raw 78193-78197: an unbound unit (+0x314a < 0) binds to the colony on
     * its tile, FUN_281f_07be -> FUN_15eb_0a76 (tr9 1687: the intervention
     * force in New Amsterdam saves origin 1). */
    if (bind_origin && u->col1_origin >= 0x80 && colonies && u->aboard_ship_id < 0 &&
        units_is_on_map(u)) {
      const int cid = colonies_id_at(colonies, u->x, u->y);
      if (cid >= 0 && cid < 0x80) {
        w->units->units[i].col1_origin = (uint8_t)cid;
      }
    }
    stuff->all_unit_counts[n]++;
    if (cont >= 0 && cont < 16) {
      stuff->land_unit_counts_by_continent[n * 16 + cont]++;
    }
    if (t == 0) {
      stuff->free_colonist_counts[n]++;
    }
    if (units_type_has_profession_slot(t)) { /* FUN_281f_0b78 >= 0 */
      census_sat_add(&stuff->census_pop_proxy[n], 1);
      if (cont >= 0 && cont < 16) {
        census_sat_add(&stuff->skilled_unit_counts_by_continent[n * 16 + cont], 1);
      }
    }
    if (!hull) {
      const int v0 = combat_unit_base_x8(&sctx, u->id, 0, NULL); /* 09c8(u,0) */
      census_sat_add(&stuff->land_combat_totals[n], v0);
      if (cont >= 0 && cont < 16) {
        census_sat_add(&stuff->unit_value_sum_by_continent[n * 16 + cont], v0);
      }
      const int v1 = combat_unit_base_x8(&sctx, u->id, 1, NULL); /* 09c8(u,1) */
      stuff->land_combat_strength[n] = (uint16_t)(stuff->land_combat_strength[n] + v1);
      if (cont >= 0 && cont < 16) {
        census_sat_add(&stuff->combat_value_sum_by_continent[n * 16 + cont], v1);
      }
      if (ai_contact_unit_counts_as_field(colonies, col1, u)) {
        census_sat_add(&stuff->field_combat_totals[n], v1);
        if (cont >= 0 && cont < 16) {
          census_sat_add(&stuff->field_combat_strength_by_continent[n * 16 + cont], v1);
        }
      }
    }
  }
  /* Colony array (raw 78238-78303). */
  for (int i = 0; colonies && i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (!c->active) {
      continue;
    }
    const int cont = map ? map_continent_id_at(map, c->x, c->y) : -1;
    if (c->nation_id != n) {
      if (cont >= 0 && cont < 16) {
        stuff->continent_presence_flags[cont] |= 4;
      }
      continue;
    }
    const int pop = c->colonist_count < 0 ? 0 : c->colonist_count;
    stuff->colony_counts[n]++;
    pop_sum += pop;
    census_sat_add(&stuff->census_pop_proxy[n], pop);
    census_sat_add(&stuff->colony_pop_totals[n], pop);
    if (cont >= 0 && cont < 16) {
      stuff->colony_counts_by_continent[n * 16 + cont]++;
      census_sat_add(&stuff->skilled_unit_counts_by_continent[n * 16 + cont], pop);
    }
  }
  /* Settlement records (raw 78306-78312). */
  if (col1 && col1->tribe && map) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const int cont = map_continent_id_at(map, (int)col1->tribe[ti].x, (int)col1->tribe[ti].y);
      if (cont >= 0 && cont < 16) {
        stuff->continent_presence_flags[cont] |= 1;
      }
    }
  }
  /* raw 78323-78326: the word becomes the mean colony size. */
  if (stuff->colony_counts[n] != 0) {
    pop_sum /= (int)stuff->colony_counts[n];
  }
  stuff->avg_colony_pop[n * 2] = (uint8_t)(pop_sum & 0xff);
  stuff->avg_colony_pop[n * 2 + 1] = (uint8_t)((pop_sum >> 8) & 0xff);
}

void col1_stuff_census_4962_w(const ColonizeWorld* w, ColonizeCol1Stuff* stuff, int n) {
  if (!w || !stuff || n < 0 || n > 3) {
    return;
  }
  census_4962(w, stuff, n, true);
}

/* A blank template = no nation censused yet: run each nation's 4962 in 00f2
 * order, so the presence row ends as nation 3's (as after a DOS turn). */
void col1_stuff_census_fill_blank_w(const ColonizeWorld* w, ColonizeCol1Stuff* stuff) {
  if (!w || !stuff) {
    return;
  }
  for (int n = 0; n < 4; ++n) {
    census_4962(w, stuff, n, false);
  }
}
