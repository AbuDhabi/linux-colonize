#include "core/founding_fathers.h"

#include "core/ai_diplo.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/units.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * FF election (FUN_4345_0a22 / 0982 / 0342 stand-ins for control flow).
 * head.founding_father[i]: -1 unclaimed; 0..3 = owning European nation.
 * nation.founding_fathers[4]: bit i set when nation elected FF i.
 *
 * Effect authority: Colonization.pdf (FF ~pp. 83–94) + docs/fandom_col1994.md.
 * No invented treasury/crosses/tools fiction when the real rule is known.
 */

/* King tax-refuse stand-in byte (ai_king unknown46[2]). */
#define FF_KING_BOYCOTT_BYTE 2

#define FF_CORONADO_REVEAL_RADIUS 2
#define FF_DESOTO_REVEAL_RADIUS 1
#define FF_BOLIVAR_SOL_PERCENT 20u
#define FF_LA_SALLE_STOCKADE_POP 3

unsigned founding_fathers_bells_needed(unsigned elected_count) {
  /* First at 40, second at 80, … — linear stand-in for FUN_4345_0982. */
  return 40u * (elected_count + 1u);
}

bool founding_fathers_nation_has(const ColonizeCol1Save* col1, int nation, int ff_index) {
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }
  if (ff_index < 0 || ff_index >= (int)COLONIZE_COL1_FF_COUNT) {
    return false;
  }
  if (col1->head.founding_father[ff_index] == (int8_t)nation) {
    return true;
  }
  const uint8_t byte = col1->nation[nation].founding_fathers[ff_index / 8];
  return (byte & (uint8_t)(1u << (ff_index % 8))) != 0;
}

bool founding_fathers_franklin_keeps_nw_peace(const ColonizeCol1Save* col1, int nation) {
  /*
   * docs/fandom_col1994.md Benjamin Franklin — NW peer peace gate.
   * Ownership via founding_fathers_nation_has (head owner or nation bitmask).
   * Requires head.founding_father[i]==-1 when unclaimed (col1_save_init).
   */
  return founding_fathers_nation_has(col1, nation, FF_BENJAMIN_FRANKLIN);
}

bool founding_fathers_brebeuf_missionaries_are_experts(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Father Jean de Brebeuf — all missionaries function
   * as experts. Ownership via founding_fathers_nation_has (head or bitmask).
   * No elect crosses; ai_contact Jesuit-grade mid convert consumes the gate.
   */
  return founding_fathers_nation_has(col1, nation, FF_JEAN_DE_BREBEUF);
}

bool founding_fathers_sepulveda_convert_join_bonus(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Juan de Sepulveda — higher convert-join chance.
   * Ownership only; raise-join call site PARKED (needs 2820/4528 path).
   */
  return founding_fathers_nation_has(col1, nation, FF_JUAN_DE_SEPULVEDA);
}

bool founding_fathers_de_soto_lcr_always_positive(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Hernando de Soto; decomp FUN_65dd_0004 FF index 7.
   * Ownership gate; resolve via units_resolve_lcr_rumour (thin positive-only).
   */
  return founding_fathers_nation_has(col1, nation, FF_HERNANDO_DE_SOTO);
}

bool founding_fathers_de_witt_allows_foreign_colony_trade(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Jan de Witt — foreign-colony trade allowed.
   * Ownership gate; cargo via colonies_de_witt_transfer_* (no gold invent).
   */
  return founding_fathers_nation_has(col1, nation, FF_JAN_DE_WITT);
}

bool founding_fathers_cortes_guarantees_conquest_treasure(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Hernan Cortes — conquered native settlements always
   * yield more treasure. Ownership gate; amount via units_cortes_conquest_treasure_gold
   * (FUN_5fef_31ea peel) when fallout gold_amount<=0.
   */
  return founding_fathers_nation_has(col1, nation, FF_HERNAN_CORTES);
}

bool founding_fathers_cortes_free_king_galleon(const ColonizeCol1Save* col1, int nation) {
  /*
   * docs/fandom_col1994.md Hernan Cortes — king's galleons transport treasure
   * free. GAME.TXT @KINGGALLEON3: Crown share = current tax rate (already
   * europe_cash_treasure); "for no extra charge" — do NOT invent KINGGALLEON2
   * non-Cortes royal-galleon extra %. Ownership documents free transport for
   * voyage hooks when wired.
   */
  return founding_fathers_nation_has(col1, nation, FF_HERNAN_CORTES);
}

bool founding_fathers_revere_should_auto_arm(
  const ColonizeCol1Save* col1,
  int nation,
  bool colony_has_soldier_defender,
  int muskets_stock
) {
  /* PEDIA: "When a colony with no standing soldiers is attacked, a colonist
   * automatically takes up any stockpiled muskets in defense of the colony."
   * Equip step matches colony arm path (UNITS_EQUIP_MUSKETS = 50). */
  if (!founding_fathers_nation_has(col1, nation, FF_PAUL_REVERE)) {
    return false;
  }
  if (colony_has_soldier_defender) {
    return false;
  }
  return muskets_stock >= UNITS_EQUIP_MUSKETS;
}

int founding_fathers_revere_auto_arm(
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  int colony_id
) {
  ColonizeColony* col = colonies_get_mut(colonies, colony_id);
  if (!col || !col->active || !units) {
    return -1;
  }
  if (col->colonist_count <= 0 || col->stock[COLONIZE_CARGO_MUSKETS] < UNITS_EQUIP_MUSKETS) {
    return -1;
  }
  /* First active colonist takes up muskets (PEDIA auto-arm). */
  int idx = -1;
  for (int i = 0; i < col->colonist_count; ++i) {
    if (col->colonists[i].active) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    return -1;
  }
  return colonies_eject_colonist(colonies, colony_id, idx, units, COLONIZE_EJECT_SOLDIER);
}

/*
 * Franklin elect: clear Euro×Euro WAR with all New World peers (make_peace).
 * Source: docs/fandom_col1994.md — king's European wars no longer affect NW
 * relations; ongoing gate lives in ai_diplo declare / euro_balance.
 */
static void effect_franklin_nw_peace(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation_id) {
      continue;
    }
    if (ai_diplo_at_war(col1, nation_id, peer)) {
      ai_diplo_make_peace(col1, nation_id, peer);
    }
  }
}

/* Pocahontas elect: all native tension → content for this European nation. */
static void effect_pocahontas_reset_alarm(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id > 3) {
    return;
  }
  if (col1->tribe) {
    for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
      col1->tribe[i].alarm[nation_id].friction = 0;
      col1->tribe[i].alarm[nation_id].attacks = 0;
    }
  }
  for (int ind = 0; ind < 8; ++ind) {
    col1->indian[ind].alarm_by_player[nation_id] = 0;
  }
}

static bool ff_unclaimed(const ColonizeCol1Save* col1, int idx) {
  return col1->head.founding_father[idx] < 0;
}

static int pick_candidate(const ColonizeCol1Save* col1, const ColonizeCol1Nation* nat) {
  const int next = (int)nat->next_founding_father;
  if (next >= 0 && next < (int)COLONIZE_COL1_FF_COUNT && ff_unclaimed(col1, next)) {
    return next;
  }
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if (ff_unclaimed(col1, i)) {
      return i;
    }
  }
  return -1;
}

static int16_t advance_next_candidate(const ColonizeCol1Save* col1, int elected_idx) {
  for (int i = elected_idx + 1; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if (ff_unclaimed(col1, i)) {
      return (int16_t)i;
    }
  }
  for (int i = 0; i < elected_idx; ++i) {
    if (ff_unclaimed(col1, i)) {
      return (int16_t)i;
    }
  }
  return -1;
}

/* Coronado: reveal radius around each owned colony. Returns colonies touched. */
static int effect_coronado_reveal(
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  int nation_id
) {
  if (!map || !map->seen || !colonies) {
    return 0;
  }
  int touched = 0;
  for (int i = 0; i < colonies->colony_count; ++i) {
    ColonizeColony* col = &colonies->colonies[i];
    if (!col->active || col->nation_id != nation_id) {
      continue;
    }
    map_reveal_radius(map, col->x, col->y, nation_id, FF_CORONADO_REVEAL_RADIUS);
    touched++;
  }
  return touched;
}

/* de Soto partial: extended sight stand-in via land-unit reveal.
 * LCR always-positive: wired via units_resolve_lcr_rumour (reveal radius). */
static int effect_desoto_reveal(
  ColonizeWorldMap* map,
  ColonizeUnitPool* units,
  int nation_id
) {
  if (!map || !map->seen || !units) {
    return 0;
  }
  int touched = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || !units_is_on_map(u)) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      continue;
    }
    map_reveal_radius(map, u->x, u->y, nation_id, FF_DESOTO_REVEAL_RADIUS);
    touched++;
  }
  return touched;
}

/*
 * Magellan: permanent naval +1 — bump current sea moves_left once on elect;
 * turn_refresh_moves_for_nation adds +1 each turn while owned (see turn.c).
 */
static int effect_magellan_sea_moves(ColonizeUnitPool* units, int nation_id) {
  if (!units) {
    return 0;
  }
  int bumped = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (!units_is_sea(units, u->id)) {
      continue;
    }
    if (u->moves_left < 0x7fffffff) {
      u->moves_left++;
    }
    bumped++;
  }
  return bumped;
}

/* La Salle: Stockade when colony population >= 3 (wiki / manual). */
static int effect_la_salle_stockades(ColonizeColonyPool* colonies, int nation_id) {
  if (!colonies) {
    return 0;
  }
  const int stock_idx = colonies_find_building(colonies, "Stockade");
  if (stock_idx < 0 || stock_idx >= COLONIZE_BUILDING_TYPES_MAX) {
    return 0;
  }
  int touched = 0;
  for (int i = 0; i < colonies->colony_count; ++i) {
    ColonizeColony* col = &colonies->colonies[i];
    if (!col->active || col->nation_id != nation_id) {
      continue;
    }
    if (col->population < FF_LA_SALLE_STOCKADE_POP) {
      continue;
    }
    if (!col->has_building[stock_idx]) {
      col->has_building[stock_idx] = true;
      touched++;
    }
  }
  return touched;
}

/* Bolivar: +20% SoL via Col1 rebel_dividend on owned colonies. */
static int effect_bolivar_rebel(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || !col1->colony || col1->head.colony_count == 0) {
    return 0;
  }
  int touched = 0;
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    ColonizeCol1Colony* c = &col1->colony[i];
    if ((int)c->nation_id != nation_id) {
      continue;
    }
    const uint32_t div = c->rebel_divisor > 0 ? c->rebel_divisor : 100u;
    uint32_t bump = (div * FF_BOLIVAR_SOL_PERCENT) / 100u;
    if (bump == 0) {
      bump = 1;
    }
    if (c->rebel_dividend < 0xffffffffu - bump) {
      c->rebel_dividend += bump;
    } else {
      c->rebel_dividend = 0xffffffffu;
    }
    if (c->rebel_dividend > div) {
      c->rebel_dividend = div;
    }
    touched++;
  }
  return touched;
}

/*
 * Brewster: no Petty Criminals / Indentured Servants in Europe recruit pool.
 * (Player pick among pool→dock UI still PARKED.)
 */
static void effect_brewster_filter_pool(EuropeScreen* europe) {
  if (!europe) {
    return;
  }
  europe->brewster_no_criminals = true;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (!europe->pool[i].filled) {
      continue;
    }
    if (europe->pool[i].profession == COLONIZE_PROF_CRIMINAL ||
        europe->pool[i].profession == COLONIZE_PROF_INDENTURED ||
        strstr(europe->pool[i].name, "Criminal") != NULL ||
        strstr(europe->pool[i].name, "Indentured") != NULL ||
        strstr(europe->pool[i].name, "Servant") != NULL) {
      europe_refill_pool_slot(europe, i, NULL);
    }
  }
}

/*
 * Las Casas: existing Indian converts → free colonists (profession only).
 * Cite: COLONIZE/PEDIA.TXT @FATHER24; docs/fandom_col1994.md Religious table.
 * Representation: NAMES @JOB Convert (27) / Free Colonists (19) — no separate
 * @UNIT; map units use Colonists type + convert profession.
 */
static int effect_las_casas_assimilate(
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  int nation_id
) {
  int touched = 0;
  if (nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }

  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* col = &colonies->colonies[i];
      if (!col->active || col->nation_id != nation_id) {
        continue;
      }
      for (int c = 0; c < col->colonist_count; ++c) {
        ColonizeColonist* person = &col->colonists[c];
        if (!person->active) {
          continue;
        }
        if (person->profession == COLONIZE_PROF_CONVERT) {
          person->profession = COLONIZE_PROF_FREE_COLONIST;
          touched++;
        }
      }
    }
  }

  if (units) {
    int free_ty = units_find_type(units, "Free Colonist");
    if (free_ty < 0) {
      free_ty = units_find_type(units, "Colonists");
    }
    if (free_ty < 0) {
      free_ty = units_find_type(units, "Colonist");
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      bool changed = false;
      if (u->profession == COLONIZE_PROF_CONVERT) {
        u->profession = COLONIZE_PROF_FREE_COLONIST;
        changed = true;
      }
      /* Name-based type swap when a Convert/@JOB display type was used. */
      const ColonizeUnitType* ut = units_type(units, u->type_index);
      if (ut && free_ty >= 0 &&
          (strstr(ut->name, "Indian Convert") != NULL ||
           strcmp(ut->name, "Convert") == 0 ||
           strcmp(ut->name, "Converts") == 0)) {
        u->type_index = free_ty;
        if (u->profession == COLONIZE_PROF_CONVERT) {
          u->profession = COLONIZE_PROF_FREE_COLONIST;
        }
        changed = true;
      }
      if (changed) {
        touched++;
      }
    }
  }
  return touched;
}

static bool ff_find_coastal_water(
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  int nation_id,
  int* out_x,
  int* out_y
) {
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  if (!map || !out_x || !out_y) {
    return false;
  }

  if (colonies) {
    for (int i = 0; i < colonies->colony_count; ++i) {
      const ColonizeColony* col = &colonies->colonies[i];
      if (!col->active || col->nation_id != nation_id) {
        continue;
      }
      if (!map_tile_is_coastal(map, col->x, col->y)) {
        continue;
      }
      for (int d = 0; d < 8; ++d) {
        const int nx = col->x + dx[d];
        const int ny = col->y + dy[d];
        if (map_tile_is_water(map, nx, ny)) {
          *out_x = nx;
          *out_y = ny;
          return true;
        }
      }
    }
  }

  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id || !units_is_on_map(u)) {
        continue;
      }
      if (!units_is_sea(units, u->id)) {
        continue;
      }
      *out_x = u->x;
      *out_y = u->y;
      return true;
    }
  }
  return false;
}

/* John Paul Jones: free Frigate (Man-O-War fallback). Returns true on spawn. */
static bool effect_jones_frigate(
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  int nation_id
) {
  if (!units || !map) {
    return false;
  }
  int ship_ty = units_find_type(units, "Frigate");
  if (ship_ty < 0) {
    ship_ty = units_find_type(units, "Man-O-War");
  }
  if (ship_ty < 0) {
    return false;
  }
  int sx = 0;
  int sy = 0;
  if (!ff_find_coastal_water(map, colonies, units, nation_id, &sx, &sy)) {
    return false;
  }
  const int sid = units_spawn_allow_stack(units, ship_ty, sx, sy);
  if (sid < 0) {
    return false;
  }
  ColonizeUnit* ship = units_get(units, sid);
  if (ship) {
    ship->nation_id = nation_id;
  }
  return true;
}

/*
 * Apply manual/wiki FF effect. Prefer real gates/hooks; PARK when missing —
 * never invent gold/crosses/tools as a substitute power.
 */
static void apply_effect(
  ColonizeTurnContext* ctx,
  ColonizeCol1Save* col1,
  ColonizeCol1Nation* nat,
  EuropeScreen* europe,
  int nation_id,
  int human_nation,
  int ff_index
) {
  ColonizeWorldMap* map = ctx ? ctx->map : NULL;
  ColonizeColonyPool* colonies = ctx ? ctx->colonies : NULL;
  ColonizeUnitPool* units = ctx ? ctx->units : NULL;
  (void)nat;

  switch (ff_index) {
    case FF_ADAM_SMITH:
      /* Manual/wiki: unlock factory-tier + 1.5× factory throughput.
       * Gate already via game_nation_has_ff → ColoniesBuildableOpts.has_adam_smith;
       * factory 6→9 in colony_production. No elect treasury fiction. */
      break;
    case FF_JAKOB_FUGGER:
      /* Manual/wiki: clear all Europe boycotts (no back taxes). */
      nat->boycott_bitmap = 0;
      if (nation_id == human_nation && col1) {
        col1->head.unknown46[FF_KING_BOYCOTT_BYTE] = 0;
      }
      break;
    case FF_PETER_MINUIT:
      /* Manual/wiki: Indians no longer demand payment for land.
       * Decomp FUN_4cc6_07c2 zeros land-buy gold when FF 2 owned.
       * Wired via founding_fathers_nation_has → colonies_indian_land_purchase_gold
       * / colonies_found_with_indian_land (pioneer plow/road buy still PORT). */
      break;
    case FF_PETER_STUYVESANT:
      /* Manual/wiki: unlock Custom House — gated via has_peter_stuyvesant.
       * Auto-sell: europe_custom_house_autosell from turn_produce (FUN_364b_0688
       * stock>99 leave 50; FUN_364b_0636 denylist). Per-cargo UI PARKED. */
      break;
    case FF_JAN_DE_WITT:
      /* docs/fandom_col1994.md: trade with foreign colonies; FA more revealing.
       * Ownership gate: founding_fathers_de_witt_allows_foreign_colony_trade.
       * FA detailed strength already peeks head.founding_father[4] (reports).
       * Cargo: colonies_de_witt_transfer_* (stock only; AI trade act PARKED). */
      break;
    case FF_FERDINAND_MAGELLAN:
      /* Manual/wiki: all naval vessels +1 movement (permanent). */
      (void)effect_magellan_sea_moves(units, nation_id);
      break;
    case FF_FRANCISCO_CORONADO:
      /* Manual/wiki: reveal owned colonies and surroundings. */
      (void)effect_coronado_reveal(map, colonies, nation_id);
      break;
    case FF_HERNANDO_DE_SOTO:
      /* docs/fandom_col1994.md: LCR always positive + extended sight.
       * Partial: extended sight via land-unit reveal on elect.
       * LCR: units_resolve_lcr_rumour + founding_fathers_de_soto_lcr_always_positive;
       * full FUN_65dd_0004 RNG table PARKED (no invented treasure/FoY). */
      (void)effect_desoto_reveal(map, units, nation_id);
      break;
    case FF_HENRY_HUDSON:
      /* Manual/wiki: fur trapper output +100% — applied in turn harvest
       * when founding_fathers_nation_has(..., FF_HENRY_HUDSON). */
      break;
    case FF_SIEUR_DE_LA_SALLE:
      /* Manual/wiki: Stockade at population >= 3 (existing + future via elect). */
      (void)effect_la_salle_stockades(colonies, nation_id);
      break;
    case FF_HERNAN_CORTES:
      /* API ready (docs/fandom_col1994.md Hernan Cortes; Colonization.pdf FF):
       * founding_fathers_cortes_guarantees_conquest_treasure +
       * units_cortes_conquest_treasure_gold (FUN_5fef_31ea peel) +
       * units_spawn_treasure_train; free king-galleon via
       * founding_fathers_cortes_free_king_galleon. Fallout wired from
       * units_resolve_land_combat_ff when fallout context set. rich_capital
       * (-0xcc) still PARKED as 0. */
      break;
    case FF_GEORGE_WASHINGTON:
      /* PEDIA/wiki: non-veteran soldiers/dragoons who win combat always upgrade.
       * Ownership bit; promote-on-win in units_resolve_land_combat_ff.
       * AI/contact path: units_resolve_land_combat passes g_units_ff_col1
       * (units_set_ff_col1 from turn_refresh_moves_for_nation). */
      break;
    case FF_PAUL_REVERE:
      /* PEDIA/wiki: colony with no soldiers auto-arms from musket stock when
       * attacked. Ownership bit; gate + eject via founding_fathers_revere_*;
       * combat spawn wired in units_try_move when FF col1 context is set
       * (turn_refresh_moves_for_nation → units_set_ff_col1). */
      break;
    case FF_FRANCIS_DRAKE:
      /* PEDIA/wiki: Privateer combat strength +50%.
       * Ownership bit; multiplier in units_resolve_naval_combat_ff (*3/2).
       * AI/king path: units_resolve_naval_combat passes g_units_ff_col1
       * (units_set_ff_col1 from turn_refresh_moves_for_nation). */
      break;
    case FF_JOHN_PAUL_JONES:
      /* Manual/wiki: free Frigate. No gold fallback. */
      (void)effect_jones_frigate(map, colonies, units, nation_id);
      break;
    case FF_THOMAS_JEFFERSON:
      /* Wiki: liberty bell production of statesmen +50%.
       * Ownership bit; applied in colony_prod_colony_bells_ff via turn nation ticks. */
      break;
    case FF_POCAHONTAS:
      /* Wiki/fandom: all native tension → content; Indian alarm half as fast.
       * Elect: zero this nation's tribe friction/attacks + alarm_by_player.
       * Half-rate ongoing growth: ai_contact_alarm_bump_amount (encroachment /
       * prelude escalate / raid bump) — not PARKED. */
      effect_pocahontas_reset_alarm(col1, nation_id);
      break;
    case FF_THOMAS_PAINE:
      /* Wiki: liberty bell production in all colonies + current tax rate %.
       * Ownership bit; applied in colony_prod_colony_bells_ff via turn nation ticks. */
      break;
    case FF_SIMON_BOLIVAR:
      /* Manual/wiki: SoL membership in all colonies +20%. */
      (void)effect_bolivar_rebel(col1, nation_id);
      break;
    case FF_BENJAMIN_FRANKLIN:
      /* docs/fandom_col1994.md: king's European wars no longer affect NW
       * relations; Europeans in the New World always offer peace.
       * Elect: make_peace with all Euro peers. Ongoing: ownership gate via
       * founding_fathers_franklin_keeps_nw_peace → ai_diplo declare /
       * euro_balance / war-hit (no gold fiction). FA 3f41 UI PARKED. */
      effect_franklin_nw_peace(col1, nation_id);
      break;
    case FF_WILLIAM_BREWSTER:
      /* Manual/wiki: no criminals/servants on docks + recruit pick.
       * Filter Europe pool now; pick-among-pool UI PARKED. */
      if (nation_id == human_nation) {
        effect_brewster_filter_pool(europe);
      }
      break;
    case FF_WILLIAM_PENN:
      /* Wiki: cross production in all colonies +50%.
       * Ownership bit; applied in colony_prod_colony_crosses_ff via turn nation ticks. */
      break;
    case FF_JEAN_DE_BREBEUF:
      /* docs/fandom_col1994.md: all missionaries function as experts.
       * Ownership bit only — no elect crosses fiction. Ongoing gate:
       * founding_fathers_brebeuf_missionaries_are_experts → ai_contact
       * Jesuit-grade mid convert for plain Missionary. */
      break;
    case FF_JUAN_DE_SEPULVEDA:
      /* docs/fandom_col1994.md: higher chance subjugated Indians convert/join.
       * Ownership gate: founding_fathers_sepulveda_convert_join_bonus.
       * PARKED call site: needs FUN_4d56_2820/4528 convert-join outcome —
       * ai_contact missionary convert pulse ≠ subjugated join; no invent %. */
      break;
    case FF_BARTOLOME_DE_LAS_CASAS:
      /* PEDIA @FATHER24 / fandom_col1994.md: existing Indian converts
       * assimilate as free colonists. Elect: profession Convert→Free
       * Colonist on owned colony colonists + map units. Ownership tick
       * in founding_fathers_tick re-runs for late converts. No gold/crosses. */
      (void)effect_las_casas_assimilate(colonies, units, nation_id);
      break;
    default:
      break;
  }
}

/* Returns true if a founding father was elected for this nation. */
static bool try_elect_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }

  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  const unsigned needed = founding_fathers_bells_needed(nat->founding_father_count);
  if ((unsigned)nat->liberty_bells_total < needed) {
    return false;
  }

  const int idx = pick_candidate(col1, nat);
  if (idx < 0) {
    return false;
  }

  col1->head.founding_father[idx] = (int8_t)nation_id;
  nat->founding_fathers[idx / 8] |= (uint8_t)(1u << (idx % 8));
  if (nat->founding_father_count < 65535u) {
    nat->founding_father_count++;
  }
  nat->next_founding_father = advance_next_candidate(col1, idx);

  EuropeScreen* europe = (nation_id == ctx->human_nation) ? ctx->europe : NULL;
  apply_effect(ctx, col1, nat, europe, nation_id, ctx->human_nation, idx);

  if (ctx->status && ctx->status_size > 0 && nation_id == ctx->human_nation) {
    snprintf(ctx->status, ctx->status_size, "Founding Father elected (#%d)", idx);
  }
  /*
   * Thin Continental Congress elect chrome (presentation only). Debate pick
   * UI / F3 full report PARKED. Cite: reports F3 Congress; founding_fathers.h.
   */
  if (ctx->ai_popups && nation_id == ctx->human_nation) {
    char body[AI_POPUP_BODY_LEN];
    snprintf(body, sizeof(body), "Founding Father #%d joins the Continental Congress.", idx);
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups,
      AI_POPUP_TAG_FF_CONGRESS,
      nation_id,
      -1,
      idx,
      "Continental Congress",
      body
    );
  }
  return true;
}

void founding_fathers_tick(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (ctx->human_nation < 0 || ctx->human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }

  ColonizeCol1Save* col1 = ctx->col1;

  /* Human first (one elect max). */
  try_elect_nation(ctx, ctx->human_nation);

  /* Then each AI Euro nation (control==1), one elect each max. */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (n == ctx->human_nation) {
      continue;
    }
    if (col1->player[n].control != 1) {
      continue;
    }
    try_elect_nation(ctx, n);
  }

  /* Las Casas ownership tick: re-assimilate Convert→Free Colonist while owned
   * (PEDIA elect is one-shot; tick catches late joins without inventing join). */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (!founding_fathers_nation_has(col1, n, FF_BARTOLOME_DE_LAS_CASAS)) {
      continue;
    }
    (void)effect_las_casas_assimilate(ctx->colonies, ctx->units, n);
  }
}
