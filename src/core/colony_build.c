#include "core/colony.h"

/*
 * Sections:
 *  - Fortification bonus, abandon & colony capture (colonies_has_fortification .. colonies_capture)
 *  - Build queue: unit build info, wagon caps & construction set/complete/buy (colonies_unit_build_info .. colonies_buy_construction)
 *  - Building catalog, chains & buildability rules (colonies_has_building_named .. colonies_list_buildable)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/ai_euro.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/ai_diplo.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/units_cargo.h"
#include "platform/diagnostics.h"

#include "core/colony_internal.h"

/* ===================== Fortification bonus, abandon & colony capture (colonies_has_fortification .. colonies_capture) ===================== */
bool colonies_has_fortification(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  if (!pool || !colony) {
    return false;
  }
  const int* k_forts = colonies_building_chain_rows(COLONIES_CHAIN_FORTIFICATION);
  for (size_t i = 0; k_forts && k_forts[i] >= 0; ++i) {
    const int idx = colonies_building_row(pool, (ColonizeBuildingRow)k_forts[i]);
    if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[idx]) {
      return true;
    }
  }
  return false;
}

int colonies_fortification_defense_bonus_percent(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
) {
  if (!pool || !colony || !colony->active) {
    return 0;
  }
  /* Highest tier wins (Fortress upgrades Fort upgrades Stockade). */
  const int fortress = colonies_building_row(pool, COLONY_BUILDING_FORTRESS);
  if (fortress >= 0 && fortress < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[fortress]) {
    return 200;
  }
  const int fort = colonies_building_row(pool, COLONY_BUILDING_FORT);
  if (fort >= 0 && fort < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[fort]) {
    return 150;
  }
  const int stockade = colonies_building_row(pool, COLONY_BUILDING_STOCKADE);
  if (stockade >= 0 && stockade < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[stockade]) {
    return 100;
  }
  return 0;
}

bool colonies_abandon(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !col->active) {
    return false;
  }
  colonies_mark_settlement_tile(col->x, col->y, false);
  memset(col, 0, sizeof(*col));
  int active = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active) {
      active++;
    }
  }
  pool->colony_count = active;
  return true;
}

void colonies_set_col1_context(ColonizeCol1Save* col1) {
  g_colonies_col1 = col1;
}

/*
 * FUN_5fef_1b0e colony-capture tail (viceroy_unpacked.c ~100905-101030),
 * Euro→Euro only. In DOS order: crown capture during WoI sets 0x5382|0x40
 * (REF unit threshold); colony_counts/colony_pop_totals move with the
 * colony; colony nation byte swaps; rebel dividend (+0xc2) = old*2/3;
 * peacetime: loser's treasury share gold*pop/(pop + Σ pop of the loser's
 * remaining colonies) moves to the captor (the @CAPTURED %NUMBER0);
 * nation_relation words of both zeroed; WAR bit set between them when not
 * already at war (DOS toggles the single byte; mirrored both ways here).
 * @HOWTOWIN once-latch (0x5386 bit0) is left to ai_king's declare path.
 */
static int colonies_capture_col1_effects(
  ColonizeCol1Save* col1, const ColonizeColony* col, int old_nation, int new_nation
) {
  if (!col1 || !col1->colony || old_nation < 0 || old_nation > 3 || new_nation < 0 ||
      new_nation > 3) {
    return 0;
  }
  const int pop = col->colonist_count > 0 ? col->colonist_count : 0;
  const bool woi = col1->head.game_options.woi != 0;
  if (woi && new_nation == (int)col1->head.crown_nation_id) {
    col1->head.game_options.ref_unit_threshold = 1;
  }
  if (col1->stuff.colony_counts[old_nation] > 0) {
    col1->stuff.colony_counts[old_nation]--;
  }
  col1->stuff.colony_counts[new_nation]++;
  col1->stuff.colony_pop_totals[old_nation] =
    (uint8_t)(col1->stuff.colony_pop_totals[old_nation] > pop
                ? col1->stuff.colony_pop_totals[old_nation] - pop
                : 0);
  col1->stuff.colony_pop_totals[new_nation] =
    (uint8_t)(col1->stuff.colony_pop_totals[new_nation] + pop > 255
                ? 255
                : col1->stuff.colony_pop_totals[new_nation] + pop);
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    ColonizeCol1Colony* c = &col1->colony[i];
    if ((int)c->x == col->x && (int)c->y == col->y) {
      c->nation_id = (uint8_t)new_nation;
      c->rebel_dividend = (uint32_t)(((uint64_t)c->rebel_dividend * 2u) / 3u);
      break;
    }
  }
  int plunder = 0;
  if (!woi) {
    int total = pop;
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if ((int)c->nation_id == old_nation) {
        total += c->population;
      }
    }
    if (total < 1) {
      total = 1;
    }
    /* FUN_5fef_1b0e moves the share between the two records' +0x2a — one word
     * per nation. The port's human treasury is live in EuropeScreen.gold, so
     * route both halves through the accessor or the human's plunder lands in a
     * copy the next export overwrites (audit G3). */
    const uint32_t gold = europe_nation_gold(NULL, col1, old_nation);
    plunder = (int)(((uint64_t)gold * (uint64_t)pop) / (uint64_t)total);
    europe_nation_gold_add(NULL, col1, old_nation, -(long)plunder);
    europe_nation_gold_add(NULL, col1, new_nation, (long)plunder);
  }
  col1->head.nation_relation[old_nation] = 0;
  col1->head.nation_relation[new_nation] = 0;
  if ((ai_diplo_read(col1, old_nation, new_nation) & AI_DIPLO_WAR) == 0) {
    ai_diplo_or_both(col1, old_nation, new_nation, AI_DIPLO_WAR);
  }
  return plunder;
}

bool colonies_capture_ex(
  ColonizeColonyPool* pool, int colony_id, int new_nation_id, int* plunder_gold
) {
  if (plunder_gold) {
    *plunder_gold = 0;
  }
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !col->active) {
    return false;
  }
  if (new_nation_id < 0 || new_nation_id > 11) {
    return false;
  }
  /* Indian capture: abandon (natives don't run Euro colonies T0). */
  if (new_nation_id >= 4) {
    return colonies_abandon(pool, colony_id);
  }
  const int old_nation = col->nation_id;
  col->nation_id = new_nation_id;
  if (old_nation != new_nation_id) {
    const int plunder = colonies_capture_col1_effects(g_colonies_col1, col, old_nation, new_nation_id);
    if (plunder_gold) {
      *plunder_gold = plunder;
    }
  }
  return true;
}

bool colonies_capture(ColonizeColonyPool* pool, int colony_id, int new_nation_id) {
  return colonies_capture_ex(pool, colony_id, new_nation_id, NULL);
}

/* ===================== Build queue: unit build info, wagon caps & construction set/complete/buy (colonies_unit_build_info .. colonies_buy_construction) ===================== */
bool colonies_unit_build_info(int raw_code, const char** name, int* hammers, int* tools_cost) {
  /*
   * DOS FUN_15eb_33aa kind-2 arm (raw 13489-13504) reads the @UNIT cost/tools
   * columns; units_build_project_info owns that arithmetic. The golden
   * Artillery pair (192/40, New Amsterdam, dutch-reports.SAV) and the
   * long-standing Wagon Train 40/0 both fall out of it, which is what pinned
   * the ×32 / ×10 scale factors.
   */
  return units_build_project_info(raw_code, name, hammers, tools_cost);
}

int colonies_nation_colony_count_census(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  return (int)col1->stuff.colony_counts[nation_id];
}

bool colonies_wagon_cap_reached(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return false;
  }
  /* DOS-LITERAL FUN_15eb_3650 raw 13736-13740:
   *   colony_counts[n] <= unit_type_counts[n][12]  ->  unavailable.
   * `<=` makes the cap exactly "wagons < colonies". */
  return (int)col1->stuff.colony_counts[nation_id] <=
         (int)col1->stuff.unit_type_counts[nation_id][12];
}

/*
 * Unit-project availability — DOS-LITERAL FUN_15eb_3650 kind-2 arm
 * (viceroy_unpacked.c raw 13714-13735):
 *
 *   avail = 1;
 *   if (idx < 0xd || idx > 0x12) {             // not a ship row
 *     if (idx != 0xc) {                        // not Wagon Train
 *       if (idx != 0xb) { gate = 0x24; goto check; }
 *       avail = has_building(3);               // Artillery <- Armory
 *     }
 *   } else { gate = 8; check: if (!has_building(gate)) avail = 0; }
 *
 * So every ship row (Caravel..Frigate) needs @BUILDING index 8 = Shipyard —
 * not Docks, not Drydock, and there is no population or coastal test of its
 * own (the Shipyard's own @BUILDING row carries the coastal requirement).
 *
 * The Artillery gate is has_building(3) = the Armory bit **literally**, not an
 * Armory/Magazine/Arsenal fold: FUN_15eb_035e tests one bit
 * (`bits[n>>3] & 1<<(n&7)` at colony +0x84), the completion writer
 * FUN_15eb_1030(idx, 1) only ORs the new bit in and never clears the tier
 * below, and FUN_15eb_3650's building arm refuses an upgrade whose
 * predecessor byte (DS:0x8f86 = -0x707a, + idx*0xc) is not owned. So a normally-grown
 * Arsenal colony still has the Armory bit set and the two readings agree —
 * confirmed over 977 colonies in `original_saves` (armory mask is only ever
 * 0/1/3). They differ only where a lone upper bit is real, i.e. after a
 * pillage clears one tier (`docs/save_format_map.md` records real lone-upper
 * carpenters_shop / printing_press / church masks), and there DOS says no.
 *
 * Wagon Train's only gate is the per-nation cap (raw 13736-13740).
 */
static bool colonies_unit_project_available(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int raw_code,
  const ColoniesBuildableOpts* opts
) {
  const int idx = units_build_code_to_index(raw_code);
  if (idx < 0 || !col) {
    return false;
  }
  if (idx >= COLONIZE_UNIT_INDEX_SHIP_FIRST && idx <= COLONIZE_UNIT_INDEX_SHIP_LAST) {
    return colonies_has_building_row(pool, col, COLONY_BUILDING_SHIPYARD);
  }
  if (idx == COLONIZE_UNIT_INDEX_ARTILLERY) {
    return colonies_has_building_row(pool, col, COLONY_BUILDING_ARMORY);
  }
  if (idx == COLONIZE_UNIT_INDEX_WAGON_TRAIN) {
    return !colonies_wagon_cap_reached(opts ? opts->col1 : NULL, col->nation_id);
  }
  return false;
}

bool colonies_set_construction(ColonizeColonyPool* pool, int colony_id, int building_type) {
  return colonies_set_construction_ex(pool, colony_id, building_type, NULL);
}

bool colonies_set_construction_ex(
  ColonizeColonyPool* pool,
  int colony_id,
  int building_type,
  const ColoniesBuildableOpts* opts
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (units_build_code_to_index(building_type) >= 0) {
    const char* uname = NULL;
    if (!col || !colonies_unit_project_available(pool, col, building_type, opts)) {
      return false;
    }
    colonies_unit_build_info(building_type, &uname, NULL, NULL);
    col->building_in_production = building_type;
    col->colony_flags =
      (uint8_t)(col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
    diag_info(
      "COLONY %s: building %s", col->name[0] ? col->name : "colony", uname ? uname : "unit"
    );
    return true;
  }
  if (!col || !pool) {
    return false;
  }
  if (building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  if (col->has_building[building_type]) {
    return false;
  }
  const ColonizeBuildingType* bt = &pool->building_types[building_type];
  if (bt->min_population > 0 && col->population < bt->min_population) {
    return false;
  }
  col->building_in_production = building_type;
  /* DOS FUN_5952_0214 (~93714) clears +0x1c bit 0x80 whenever a project is
   * successfully assigned — the "finished, pick something new" latch is spent. */
  col->colony_flags =
    (uint8_t)(col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
  diag_info(
    "COLONY %s: building %s (hammers %d, pop %d)",
    col->name[0] ? col->name : "colony", bt->name, col->hammers, col->population
  );
  return true;
}

bool colonies_clear_construction(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col) {
    return false;
  }
  col->building_in_production = -1;
  /* DOS FUN_5952_02f4 (~93754) clears the build-complete latch here too. */
  col->colony_flags =
    (uint8_t)(col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
  /* FUN_5952 ~95710: drop wants_construction with queue clear. */
  col->build_ai_flags =
    (uint8_t)(col->build_ai_flags & (uint8_t)~COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
  return true;
}

bool colonies_destroy_building(ColonizeColonyPool* pool, int colony_id, int building_type) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  if (!col->has_building[building_type]) {
    return false;
  }
  const ColonizeBuildingType* bt = &pool->building_types[building_type];
  /* Town Hall is the colony core — never burn/remove via raid destroy. */
  if (bt && colonies_building_name_row(bt->name) == COLONY_BUILDING_TOWN_HALL) {
    return false;
  }
  col->has_building[building_type] = false;
  if (col->building_in_production == building_type) {
    col->building_in_production = -1;
  }
  for (int i = 0; i < col->colonist_count; ++i) {
    ColonizeColonist* c = &col->colonists[i];
    if (!c->active) {
      continue;
    }
    if (c->building_type == building_type) {
      c->building_type = -1;
    }
  }
  return true;
}

/* Shared by colonies_construction_gold_cost/_tools_needed/_buy_construction:
 * the current project's hammers/tools_cost, whether it's a real building or
 * a unit-type project (colonies_unit_build_info). False if there's no
 * project or its cost can't be resolved either way. */
static bool colonies_construction_cost(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int* hammers,
  int* tools_cost
) {
  if (!pool || !colony || colony->building_in_production < 0) {
    return false;
  }
  const char* uname = NULL;
  if (colonies_unit_build_info(colony->building_in_production, &uname, hammers, tools_cost)) {
    return true;
  }
  const ColonizeBuildingType* bt = colonies_building_type(pool, colony->building_in_production);
  if (!bt || bt->hammers <= 0) {
    return false;
  }
  if (hammers) {
    *hammers = bt->hammers;
  }
  if (tools_cost) {
    *tools_cost = bt->tools_cost;
  }
  return true;
}

static int colonies_construction_gold_cost_impl(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeCol1Save* col1,
  int fallback_tools_price
) {
  int hammers_need = 0;
  int tools_cost = 0;
  if (!colonies_construction_cost(pool, colony, &hammers_need, &tools_cost)) {
    return 0;
  }
  int hammers_deficit = hammers_need - colony->hammers;
  if (hammers_deficit < 0) {
    hammers_deficit = 0;
  }
  int tools_deficit = tools_cost - colony->stock[COLONIZE_CARGO_TOOLS];
  if (tools_deficit < 0) {
    tools_deficit = 0;
  }
  /* DOS-LITERAL FUN_2f2b_5e44 raw 52709-52722: tools use the owning
   * nation's current trade.euro_price[TOOLS] byte, plus four. */
  int cost = hammers_deficit * 13;
  if (tools_deficit > 0) {
    const int nation = colony->nation_id;
    const int tools_price = col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT
                              ? (int)col1->nation[nation].trade.euro_price[COLONIZE_CARGO_TOOLS]
                              : fallback_tools_price;
    cost += tools_deficit * (tools_price + 4);
  }
  if (colony->hammers == 0) {
    cost *= 2;
  }
  return cost;
}

int colonies_construction_gold_cost_ex(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeCol1Save* col1
) {
  return colonies_construction_gold_cost_impl(pool, colony, col1, 0);
}

int colonies_construction_gold_cost(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int difficulty
) {
  /* Preserve the pre-Col1 estimate for legacy/headless callers. The live UI
   * passes its save to _ex and never uses this difficulty approximation. */
  return colonies_construction_gold_cost_impl(pool, colony, g_colonies_col1, difficulty);
}

bool colonies_try_complete_building_ex(
  ColonizeColonyPool* pool, int colony_id, const ColonizeCol1Save* col1
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool || col->building_in_production < 0) {
    return false;
  }
  const int bid = col->building_in_production;
  const ColonizeBuildingType* bt = colonies_building_type(pool, bid);
  if (!bt || bt->hammers <= 0 || col->hammers < bt->hammers) {
    return false;
  }
  /* Already own the selected project (a later call the same turn it
   * completed would otherwise re-fire; the selection itself is never
   * cleared below, matching DOS — see that comment). Treat as nothing to
   * complete, same as the player revisiting Construction on an
   * already-owned item (@ALREADYHAVE). */
  if (bid >= 0 && bid < COLONIZE_BUILDING_TYPES_MAX && col->has_building[bid]) {
    return false;
  }
  if (bt->tools_cost > 0 && col->stock[COLONIZE_CARGO_TOOLS] < bt->tools_cost) {
    /* DOS-LITERAL FUN_364b_0688 Phase L raw 57748-57774: human colonies
     * stop for @NEEDTOOLS; AI and Crown colonies are given the missing tools
     * by setting stock to the requirement, then continue through completion. */
    const bool human = col1 && col->nation_id >= 0 &&
                       col->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
                       col1->player[col->nation_id].control == 0;
    if (human || !col1) {
      return false;
    }
    col->stock[COLONIZE_CARGO_TOOLS] = bt->tools_cost;
  }
  col->pending_build_reveal = bid + 1; /* DS:0x34a — colony-screen reveal + 0x54 */
  if (bt->tools_cost > 0) {
    col->stock[COLONIZE_CARGO_TOOLS] -= bt->tools_cost;
  }
  if (bid >= 0 && bid < COLONIZE_BUILDING_TYPES_MAX) {
    col->has_building[bid] = true;
  }
  /* Col1 +0x95/+0x96: INC warehouse / capitol levels on matching completes. */
  if (bt->name[0] != '\0') {
    if (colonies_building_name_row(bt->name) == COLONY_BUILDING_WAREHOUSE && col->warehouse_level < 1u) {
      col->warehouse_level = 1;
    } else if (colonies_building_name_row(bt->name) == COLONY_BUILDING_WAREHOUSE_EXPANSION) {
      col->warehouse_level = 2;
    } else if (colonies_building_name_row(bt->name) == COLONY_BUILDING_CAPITOL && col->capitol_level < 1u) {
      col->capitol_level = 1;
    } else if (colonies_building_name_row(bt->name) == COLONY_BUILDING_CAPITOL_EXPANSION) {
      col->capitol_level = 2;
    } else if (colonies_building_name_row(bt->name) == COLONY_BUILDING_CUSTOM_HOUSE && col->custom_house_bits == 0) {
      col->custom_house_bits = COLONIZE_CUSTOM_HOUSE_DEFAULT_MASK;
    }
  }
  col->hammers = 0;
  /*
   * bugs.md: an upgrade takes its workers with it — DOS has no per-building
   * membership at all (occupation is the @JOB; the worker always works the
   * best tier owned), so completing e.g. a Lumber Mill must move the
   * Carpenter's Shop crew over. Same chain families the save bridge maps.
   */
  {
    /* A worked chain = every chain with a crew (see colonies_building_workable):
     * the crew of any lower tier follows the upgrade. */
    const int fam = colonies_building_row_chain(colonies_building_type_row(pool, bid));
    if (fam >= 0 && colonies_building_workable(pool, bid)) {
      for (int p = 0; p < col->colonist_count; ++p) {
        ColonizeColonist* c = &col->colonists[p];
        if (!c->active || c->building_type < 0 || c->building_type == bid ||
            c->building_type >= pool->building_type_count) {
          continue;
        }
        if (colonies_building_row_chain(colonies_building_type_row(pool, c->building_type)) == fam) {
          c->building_type = bid;
        }
      }
    }
  }
  /*
   * Col1 colony +0x1c bit 0x80 (COLONIZE_COLONY_FLAG_BUILD_COMPLETE):
   * FUN_364b_0114 ORs it in on every construction-completed arm — the
   * Warehouse-Expansion special case (build id 0x10, ~56925) and the generic
   * `FUN_281f_0bbe(item, 1)` grant right below it (~56935). It means "this
   * colony finished its project and has not been given a new one"; DOS clears
   * it again the moment a project is assigned (FUN_5952_0214 / 5952_02f4,
   * ~93714 / ~93754), and reads it as the colony-screen construction-pane
   * chrome (FUN_2f2b_2d1c ~49333, gated on pane DS:0x337 == 2). The port never
   * wrote it, so an export lost the state DOS would have carried (smell audit
   * #70). Set here; cleared in colonies_set_construction /
   * colonies_clear_construction, exactly like DOS's two clear sites.
   */
  col->colony_flags = (uint8_t)(col->colony_flags | COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
  /* Player-confirmed 2026-08-17 (colony_prod02 golden, a real single DOS
   * turn): building_in_production stays pointed at the just-completed
   * project — DOS never clears it on completion, only has_building[] and
   * hammers change. The guard above stops this function from re-firing on
   * a later call against the same still-selected, now-owned project. */
  col->build_ai_flags =
    (uint8_t)(col->build_ai_flags & (uint8_t)~COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
  return true;
}

bool colonies_try_complete_building(ColonizeColonyPool* pool, int colony_id) {
  return colonies_try_complete_building_ex(pool, colony_id, g_colonies_col1);
}

int colonies_try_complete_unit_construction(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  ColonizeCol1Save* col1
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units || col->building_in_production < 0) {
    return -1;
  }
  const char* name = NULL;
  int hammers_need = 0;
  int tools_cost = 0;
  if (!colonies_unit_build_info(col->building_in_production, &name, &hammers_need, &tools_cost)) {
    return -1;
  }
  if (hammers_need <= 0 || col->hammers < hammers_need) {
    return -1;
  }
  if (tools_cost > 0 && col->stock[COLONIZE_CARGO_TOOLS] < tools_cost) {
    /*
     * DOS-LITERAL FUN_364b_0688 raw 57748-57769: a non-human colony (Euro
     * AI or Crown) is simply handed the tools it needs and the project
     * proceeds; only a human colony is refused (@NEEDTOOLS elsewhere).
     * bugs.md #755.
     */
    const bool human = col1 && col->nation_id >= 0 &&
                        col->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
                        col1->player[col->nation_id].control == 0;
    if (human || !col1) {
      return -1;
    }
    col->stock[COLONIZE_CARGO_TOOLS] = tools_cost;
  }
  const int type_index = units_find_type(units, name);
  if (type_index < 0) {
    return -1;
  }
  const int uid = units_spawn_allow_stack(units, type_index, col->x, col->y);
  if (uid < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(units, uid);
  if (u) {
    units_set_nation(u, col->nation_id);
    /* Player request: freshly built Artillery heads the colony's Units-
     * Present / Military row (same "front" slot as the Move-to-front order). */
    if (units_type_is_artillery(units_type(units, type_index))) {
      units->board_first_slot = (int)(u - units->units);
      /*
       * DOS-LITERAL FUN_364b_0114 raw 56943-56946: `if (type == 0xb &&
       * (nation > 3 || player[nation].control != 0)) nation[+0x48]++` — an
       * Artillery completion by a non-human colony (Euro AI or Crown)
       * bumps the king_grace_counter byte. bugs.md #766.
       */
      const bool non_human = col1 && (col->nation_id > 3 ||
                                       (col->nation_id >= 0 &&
                                        col->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
                                        col1->player[col->nation_id].control != 0));
      if (non_human && col->nation_id < (int)COLONIZE_COL1_NATION_COUNT) {
        col1->nation[col->nation_id].king_grace_counter++;
      }
    }
  }
  if (tools_cost > 0) {
    col->stock[COLONIZE_CARGO_TOOLS] -= tools_cost;
  }
  /* Unlike colonies_try_complete_building, no has_building[]/re-fire guard
   * needed: a unit is never "owned" by the colony, and resetting hammers to
   * 0 here is itself the guard (next call reads hammers_need > 0 again). */
  col->hammers = 0;
  return uid;
}

static bool colonies_buy_construction_impl(
  ColonizeColonyPool* pool,
  int colony_id,
  const ColonizeCol1Save* col1,
  int fallback_tools_price,
  int* gold
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool || !gold || col->building_in_production < 0) {
    return false;
  }
  int hammers_need = 0;
  int tools_cost = 0;
  if (!colonies_construction_cost(pool, col, &hammers_need, &tools_cost)) {
    return false;
  }
  int hammers_deficit = hammers_need - col->hammers;
  if (hammers_deficit < 0) {
    hammers_deficit = 0;
  }
  int tools_deficit = tools_cost - col->stock[COLONIZE_CARGO_TOOLS];
  if (tools_deficit < 0) {
    tools_deficit = 0;
  }
  const int gold_cost = colonies_construction_gold_cost_impl(
    pool, col, col1, fallback_tools_price
  );
  if (*gold < gold_cost) {
    return false;
  }
  *gold -= gold_cost;
  /* FUN_2f2b_5e44: accumulate hammers *deficit* (not gold spent — the two
   * only used to be numerically equal back when gold_cost was 1:1 with
   * hammers; not any more) into +0x98. */
  if (hammers_deficit > 0) {
    /* Col1 +0x98 is a DOS word; FUN_2f2b_5e44 adds the deficit with 16-bit
     * wrap, not saturation. */
    col->hammers_purchased =
      (uint16_t)((unsigned)col->hammers_purchased + (unsigned)hammers_deficit);
  }
  /* Tops hammers/tools only — does NOT complete the project (matches
   * FUN_2f2b_5e44, which never touches has_building[]/spawns a unit
   * itself). Completion happens next turn via turn_run_colony_building_
   * completion / turn_run_colony_unit_construction. */
  col->hammers = hammers_need;
  if (tools_deficit > 0) {
    col->stock[COLONIZE_CARGO_TOOLS] += tools_deficit;
  }
  diag_info(
    "COLONY %s: bought construction for %d$ (hammers +%d, tools +%d, gold=%d)",
    col->name[0] ? col->name : "colony", gold_cost, hammers_deficit, tools_deficit, *gold
  );
  return true;
}

bool colonies_buy_construction_ex(
  ColonizeColonyPool* pool, int colony_id, const ColonizeCol1Save* col1, int* gold
) {
  return colonies_buy_construction_impl(pool, colony_id, col1, 0, gold);
}

bool colonies_buy_construction(
  ColonizeColonyPool* pool, int colony_id, int difficulty, int* gold
) {
  /* Legacy/headless call shape only. Game UI uses the explicit Col1 _ex API. */
  return colonies_buy_construction_impl(pool, colony_id, g_colonies_col1, difficulty, gold);
}

/* ===================== Building catalog, chains & buildability rules (colonies_has_building_named .. colonies_list_buildable) ===================== */
bool colonies_has_building_named(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const char* name
) {
  if (!pool || !col || !name) {
    return false;
  }
  const int idx = colonies_find_building(pool, name);
  return idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && col->has_building[idx];
}

bool colonies_has_building_name_contains(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const char* needle
) {
  if (!pool || !col || !needle) {
    return false;
  }
  for (int i = 0; i < pool->building_type_count && i < COLONIZE_BUILDING_TYPES_MAX; ++i) {
    if (!col->has_building[i]) {
      continue;
    }
    const char* bn = pool->building_types[i].name;
    if (bn && strstr(bn, needle) != NULL) {
      return true;
    }
  }
  return false;
}

/*
 * ---------------------------------------------------------------------
 * The building upgrade chains (audit CO-13 / IN-24).
 *
 * DOS groups the 42 NAMES.TXT @BUILDING rows into 15 CATEGORIES (the static
 * setup table FUN_75c2_144c). Each category is one upgrade chain, low tier
 * first, and each owns exactly one colony-screen slot. This table has 17
 * entries: the 15 screen categories, plus Capitol and Stable broken out of
 * the Town Hall and Warehouse categories they share a slot with, because
 * the save format gives each of those its own bit group.
 *
 * ***THE ORDER OF THIS TABLE IS A SAVE-FORMAT CONTRACT.*** col1_bridge.c's
 * col1_apply_colony_buildings / col1_encode_colony_buildings walk the chains
 * in this order and map chain position i to bit i of the matching
 * ColonizeCol1Buildings group word. Reordering the table, or inserting a
 * tier in the middle of a chain, silently rewrites every colony's building
 * mask on the next export. Appending a whole new chain at the end is safe;
 * nothing else is.
 *
 * Two chains carry DOS quirks the bridge handles itself and that are
 * deliberately NOT folded in here:
 *   - Warehouse: "Warehouse Expansion" has no bit of its own; DOS counts it
 *     on the colony's warehouse_level byte (+0x95). The chain still lists
 *     it because the colony screen draws it as a tier.
 *   - Capitol / Capitol Expansion: the tail of the Town Hall category in
 *     DOS's own table, but unbuildable (see colonies_building_is_buildable)
 *     and tracked on capitol_level (+0x96), so they are a separate chain
 *     here rather than three Town Hall tiers.
 * The colony screen also draws the Stable in the warehouse slot; that is a
 * rendering rule (colony_screen_category_sprite), not a chain tier, so the
 * Stable keeps its own one-entry chain.
 * ---------------------------------------------------------------------
 */
#define R(x) COLONY_BUILDING_##x
static const int k_chain_rows[COLONIES_BUILDING_CHAIN_COUNT][4] = {
  {R(STOCKADE), R(FORT), R(FORTRESS), -1},                      /* FORTIFICATION */
  {R(ARMORY), R(MAGAZINE), R(ARSENAL), -1},                     /* ARMORY */
  {R(DOCKS), R(DRYDOCK), R(SHIPYARD), -1},                      /* DOCKS */
  {R(TOWN_HALL), -1, -1, -1},                                   /* TOWN_HALL */
  {R(SCHOOLHOUSE), R(COLLEGE), R(UNIVERSITY), -1},              /* SCHOOL */
  {R(WAREHOUSE), R(WAREHOUSE_EXPANSION), -1, -1},               /* WAREHOUSE */
  {R(CAPITOL), R(CAPITOL_EXPANSION), -1, -1},                   /* CAPITOL */
  {R(STABLE), -1, -1, -1},                                      /* STABLE */
  {R(CUSTOM_HOUSE), -1, -1, -1},                                /* CUSTOM_HOUSE */
  {R(PRINTING_PRESS), R(NEWSPAPER), -1, -1},                    /* PRESS */
  {R(WEAVERS_HOUSE), R(WEAVERS_SHOP), R(TEXTILE_MILL), -1},     /* WEAVER */
  {R(TOBACCONISTS_HOUSE), R(TOBACCONISTS_SHOP), R(CIGAR_FACTORY), -1}, /* TOBACCONIST */
  {R(RUM_DISTILLERS_HOUSE), R(RUM_DISTILLERY), R(RUM_FACTORY), -1},    /* RUM */
  {R(FUR_TRADERS_HOUSE), R(FUR_TRADING_POST), R(FUR_FACTORY), -1},     /* FUR */
  {R(CARPENTERS_SHOP), R(LUMBER_MILL), -1, -1},                 /* CARPENTER */
  {R(CHURCH), R(CATHEDRAL), -1, -1},                            /* CHURCH */
  {R(BLACKSMITHS_HOUSE), R(BLACKSMITHS_SHOP), R(IRON_WORKS), -1},      /* BLACKSMITH */
};
#undef R

const int* colonies_building_chain_rows(int chain) {
  if (chain < 0 || chain >= COLONIES_BUILDING_CHAIN_COUNT) {
    return NULL;
  }
  return k_chain_rows[chain];
}

/* Chain a @BUILDING row belongs to, or -1. */
int colonies_building_row_chain(int row) {
  if (row < 0) {
    return -1;
  }
  for (int c = 0; c < COLONIES_BUILDING_CHAIN_COUNT; ++c) {
    for (int t = 0; t < 4 && k_chain_rows[c][t] >= 0; ++t) {
      if (k_chain_rows[c][t] == row) {
        return c;
      }
    }
  }
  return -1;
}

/* The catalog's own spelling of @BUILDING `row` ("" when none is known). */
const char* colonies_building_row_name(int row) {
  if (row >= 0 && row < colony_building_row_name_count && colony_building_row_names[row][0]) {
    return colony_building_row_names[row];
  }
  if (colony_building_row_name_resolver) {
    const char* n = colony_building_row_name_resolver(row);
    if (n) {
      return n;
    }
  }
  return "";
}

/*
 * Name view of a chain, for the callers that still walk names: built from
 * the row table and the names the catalog supplied — the port carries no
 * building names of its own.
 */
const char* const* colonies_building_chain(int chain) {
  static const char* names[COLONIES_BUILDING_CHAIN_COUNT][4];
  const int* rows = colonies_building_chain_rows(chain);
  if (!rows) {
    return NULL;
  }
  for (int i = 0; i < 4; ++i) {
    names[chain][i] = rows[i] >= 0 ? colonies_building_row_name(rows[i]) : NULL;
  }
  return names[chain];
}

int colonies_building_chain_length(int chain) {
  const char* const* c = colonies_building_chain(chain);
  int n = 0;
  while (c && c[n]) {
    ++n;
  }
  return n;
}

/*
 * Standard tier gate for a chain position: the tier below it is owned (or
 * this is the first tier) and no tier from this one up is. Every chain arm
 * of colonies_building_is_buildable below is this rule plus at most a
 * Founding-Father / coastal side condition.
 */
static bool colonies_chain_tier_open(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int chain, int tier
) {
  const char* const* names = colonies_building_chain(chain);
  if (!names || tier < 0 || !names[tier]) {
    return false;
  }
  if (tier > 0 && !colonies_has_building_named(pool, col, names[tier - 1])) {
    return false;
  }
  for (int i = tier; names[i]; ++i) {
    if (colonies_has_building_named(pool, col, names[i])) {
      return false;
    }
  }
  return true;
}

/* True if this type is currently a legal construction project for the colony. */
static bool colonies_building_is_buildable(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int type_index,
  const ColoniesBuildableOpts* opts
) {
  if (!pool || !col || type_index < 0 || type_index >= pool->building_type_count) {
    return false;
  }
  if (col->has_building[type_index]) {
    return false;
  }
  const ColonizeBuildingType* bt = &pool->building_types[type_index];
  if (bt->name[0] == '\0' || bt->hammers <= 0) {
    return false;
  }
  if (bt->min_population > 0 && col->population < bt->min_population) {
    return false;
  }

  const char* n = bt->name;
  const bool adam = opts && opts->has_adam_smith;
  const bool stuy = opts && opts->has_peter_stuyvesant;
  const bool coastal =
    opts && opts->map && map_tile_is_coastal(opts->map, col->x, col->y);

  /* Duplicate Town Hall rows in NAMES.TXT — never list once any Town Hall exists. */
  if (colonies_building_name_row(n) == COLONY_BUILDING_TOWN_HALL) {
    return false;
  }

  /*
   * Cut construction rows. DOS's own "can this colony build it" gate
   * (FUN_15eb_3650) hard-zeroes three @BUILDING file indices no matter what
   * the colony has: 0x0a and 0x0b (the two unfinished Town Hall upgrades —
   * PEDIA.TXT calls 0x0b "COLONIAL ASSEMBLY") and 0x1e (Capitol). 0x1f
   * (Capitol Expansion) is not zeroed there but is unreachable anyway: its
   * prerequisite is the Capitol, which can never be owned. NAMES.TXT still
   * carries all four rows and PEDIA.TXT keeps title-only stubs for them, but
   * no DOS colony can ever start one — the port used to offer Capitol /
   * Capitol Expansion in the construction picker.
   */
  if (colonies_building_name_row(n) == COLONY_BUILDING_CAPITOL || colonies_building_name_row(n) == COLONY_BUILDING_CAPITOL_EXPANSION) {
    return false;
  }

  /*
   * Chain tiers (audit CO-13): "the tier below is owned and nothing from
   * this tier up is" is colonies_chain_tier_open; the only per-row extras
   * are Adam Smith on every factory tier and coastal on the port chain.
   * Spelled out arm-by-arm below this block are the rows that are NOT that
   * rule (the Warehouse and the six starter houses, which DOS lets you
   * build regardless of what sits above them in the chain).
   */
  if (colonies_building_name_row(n) == COLONY_BUILDING_STOCKADE) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FORTIFICATION, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_FORT) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FORTIFICATION, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_FORTRESS) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FORTIFICATION, 2);
  }

  /* Military production chain. */
  if (colonies_building_name_row(n) == COLONY_BUILDING_ARMORY) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_ARMORY, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_MAGAZINE) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_ARMORY, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_ARSENAL) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_ARMORY, 2);
  }

  /* Port chain (coastal only). */
  if (colonies_building_name_row(n) == COLONY_BUILDING_DOCKS) {
    return coastal && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_DOCKS, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_DRYDOCK) {
    return coastal && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_DOCKS, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_SHIPYARD) {
    return coastal && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_DOCKS, 2);
  }

  /* Education chain. */
  if (colonies_building_name_row(n) == COLONY_BUILDING_SCHOOLHOUSE) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_SCHOOL, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_COLLEGE) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_SCHOOL, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_UNIVERSITY) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_SCHOOL, 2);
  }

  /* Warehouse tier 0 is deliberately NOT colonies_chain_tier_open: DOS gates
   * it on the Warehouse alone and ignores the Expansion above it (an
   * Expansion without its Warehouse is unreachable anyway — col1_bridge
   * derives tier 1 from warehouse_level, which implies tier 0). */
  if (colonies_building_name_row(n) == COLONY_BUILDING_WAREHOUSE) {
    return !colonies_has_building_row(pool, col, COLONY_BUILDING_WAREHOUSE);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_WAREHOUSE_EXPANSION) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_WAREHOUSE, 1);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_CUSTOM_HOUSE) {
    return stuy && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CUSTOM_HOUSE, 0);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_PRINTING_PRESS) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_PRESS, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_NEWSPAPER) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_PRESS, 1);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_WEAVERS_SHOP) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_WEAVER, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_TEXTILE_MILL) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_WEAVER, 2);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_TOBACCONISTS_SHOP) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_TOBACCONIST, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_CIGAR_FACTORY) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_TOBACCONIST, 2);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_RUM_DISTILLERY) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_RUM, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_RUM_FACTORY) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_RUM, 2);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_FUR_TRADING_POST) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FUR, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_FUR_FACTORY) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FUR, 2);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_CARPENTERS_SHOP) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CARPENTER, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_LUMBER_MILL) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CARPENTER, 1);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_CHURCH) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CHURCH, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_CATHEDRAL) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CHURCH, 1);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_BLACKSMITHS_SHOP) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_BLACKSMITH, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_IRON_WORKS) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_BLACKSMITH, 2);
  }

  /* Starter houses and other leaf buildings: available if not already owned. */
  if (colonies_building_name_row(n) == COLONY_BUILDING_WEAVERS_HOUSE || colonies_building_name_row(n) == COLONY_BUILDING_TOBACCONISTS_HOUSE ||
      colonies_building_name_row(n) == COLONY_BUILDING_RUM_DISTILLERS_HOUSE || colonies_building_name_row(n) == COLONY_BUILDING_FUR_TRADERS_HOUSE ||
      colonies_building_name_row(n) == COLONY_BUILDING_BLACKSMITHS_HOUSE || colonies_building_name_row(n) == COLONY_BUILDING_STABLE) {
    return !colonies_has_building_named(pool, col, n);
  }

  /* Unknown name: allow if unmet and not owned (forward-compatible). */
  return true;
}

int colonies_list_buildable(
  const ColonizeColonyPool* pool,
  int colony_id,
  int* out_ids,
  int out_max,
  const ColoniesBuildableOpts* opts
) {
  if (!pool || !out_ids || out_max <= 0) {
    return 0;
  }
  const ColonizeColony* col = colonies_get(pool, colony_id);
  if (!col) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < pool->building_type_count && n < out_max; ++i) {
    if (colonies_building_is_buildable(pool, col, i, opts)) {
      out_ids[n++] = i;
    }
  }
  /*
   * The seven unit projects, in DOS's own code order — FUN_15eb_38e8
   * (viceroy_unpacked.c raw 13773) walks codes -1..0x30 and keeps whatever
   * FUN_15eb_3650 calls available, so Artillery, Wagon Train and the five
   * buildable ships follow the @BUILDING rows in that order. None of them is
   * a real @BUILDING row, so none is ever "owned": no has_building[] dedup,
   * and the colony can queue another one the moment the last one spawns.
   */
  for (int code = COLONIZE_UNIT_BUILD_CODE_FIRST;
       code < COLONIZE_UNIT_BUILD_CODE_FIRST + COLONIZE_UNIT_BUILD_CODE_COUNT && n < out_max;
       ++code) {
    if (colonies_unit_project_available(pool, col, code, opts)) {
      out_ids[n++] = code;
    }
  }
  return n;
}

