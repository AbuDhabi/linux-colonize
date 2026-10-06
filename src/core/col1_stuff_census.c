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


static void col1_stuff_census_tally_units(
  ColonizeCol1Stuff* stuff,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1
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
  for (int i = 0; i < units_slot_end(units); ++i) {
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
    /*
     * DOS FUN_4962_0018 (4962:0183..0196) counts a unit toward DS:0x9410 when
     * `FUN_281f_0b78(unit) >= 0` — that is DS:0x30e[unit_type], the per-TYPE
     * profession slot, not the unit's own profession byte. Keying it off the
     * byte made a unit's own job decide whether it was a person, which is why
     * the Foreign Affairs population (and so the Rebels/Tories split built on
     * it) disagreed with the units actually on the board (bugs.md). DOS scans
     * the whole unit array, so a passenger sitting in a ship's hold counts
     * exactly like one standing on the map — the loop above already walks
     * every active unit, aboard_ship_id or not.
     */
    if (units_type_has_profession_slot(t) && stuff->census_pop_proxy[n] < 255u) {
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
      /*
       * FUN_4962_0018 non-hull arm (viceroy_unpacked.asm 4962:01b8-0294;
       * decompile 78213-78231). DOS takes the FUN_281f_09c8 → FUN_157e_004a
       * combat VALUE — combat_unit_base_x8: type combat byte ×8 plus the
       * veteran / Drake / damaged-artillery peels — and files it three ways:
       *
       *   0x9180 land_combat_totals   += 09c8(u, 0)   FUN_4962_0006
       *   0x941c land_combat_strength += 09c8(u, 1)   ADD word ptr, plain
       *   0x942c field_combat_totals  += 09c8(u, 1)   FUN_4962_0006, gated
       *
       * FUN_4962_0006 (4962:0006) is a SATURATING byte add — CL=[BX],
       * AX+=CX, `CMP AX,0xff / JLE / MOV AX,0xff`, `MOV [BX],AL` — so the
       * two byte rows clamp at 255 while the word row wraps at 16 bits.
       *
       * All three were invented before: a unit count, a Σ(attack+defense),
       * and a second count. The ×8 scale is live in ai_king's REF /
       * intervention math (`14L * land_combat_strength`, ai_king.c) and in
       * ai_diplo_00f8_top_ranked_nation.
       */
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = units;
      sctx.colonies = colonies;
      sctx.col1 = col1;
      /* combat_unit_base_x8 takes a unit ID, and this loop walks pool SLOTS
       * (ids are 1-based and do not track the slot index). */
      const int v0 = combat_unit_base_x8(&sctx, u->id, 0, NULL);
      const int v1 = combat_unit_base_x8(&sctx, u->id, 1, NULL);
      unsigned tot = (unsigned)stuff->land_combat_totals[n] + (unsigned)(v0 > 0 ? v0 : 0);
      if (tot > 255u) {
        tot = 255u;
      }
      stuff->land_combat_totals[n] = (uint8_t)tot;
      stuff->land_combat_strength[n] =
        (uint16_t)((unsigned)stuff->land_combat_strength[n] + (unsigned)(v1 > 0 ? v1 : 0));
      /*
       * 0x942c gate (4962:022f-026e) — one copy, exported by ai_contact.c,
       * which needs the same predicate for its exposed row (audit AC-9); the
       * two used to be hand-synced. See ai_contact.h for the DOS citation.
       */
      const int counts_as_field = ai_contact_unit_counts_as_field(colonies, col1, u);
      if (counts_as_field) {
        unsigned fc = (unsigned)stuff->field_combat_totals[n] + (unsigned)(v1 > 0 ? v1 : 0);
        if (fc > 255u) {
          fc = 255u;
        }
        stuff->field_combat_totals[n] = (uint8_t)fc;
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
    stuff->avg_colony_pop[n * 2] = (uint8_t)(avg & 0xffu);
    stuff->avg_colony_pop[n * 2 + 1] = (uint8_t)((avg >> 8) & 0xffu);
  }
}

void col1_stuff_census_fill_blank_w(
  const ColonizeWorld* w,
  ColonizeCol1Stuff* stuff
) {
  const ColonizeUnitPool* units = w->units;
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeCol1Save* col1 = w->col1;

  if (!stuff) {
    return;
  }
  col1_stuff_census_tally_units(stuff, units, colonies, col1);
  col1_stuff_census_tally_colonies(stuff, colonies, 1);
  col1_stuff_census_write_mean_pop(stuff);
}


void col1_stuff_census_refresh_colony_counts_w(
  const ColonizeWorld* w,
  ColonizeCol1Stuff* stuff
) {
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeUnitPool* units = w->units;
  const ColonizeCol1Save* col1 = w->col1;

  if (!stuff) {
    return;
  }
  if (units) {
    col1_stuff_census_tally_units(stuff, units, colonies, col1);
  }
  /* With units: also fold colony pop into census_pop_proxy (fill_blank shape). */
  col1_stuff_census_tally_colonies(stuff, colonies, units != NULL);
  col1_stuff_census_write_mean_pop(stuff);
}


static void census_sat_add(uint8_t* b, int v) {
  int t = (int)*b + v; /* FUN_4962_0006: AX += byte, clamp at 0xff */
  *b = (uint8_t)(t > 0xff ? 0xff : t);
}

void col1_stuff_census_4962_w(const ColonizeWorld* w, ColonizeCol1Stuff* stuff, int n) {
  if (!w || !stuff || n < 0 || n > 3) {
    return;
  }
  const ColonizeUnitPool* units = w->units;
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeCol1Save* col1 = w->col1;
  const ColonizeWorldMap* map = w->map;
  /* raw 78129-78158: this nation's slices, and all sixteen 0x95f2 bytes. */
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
