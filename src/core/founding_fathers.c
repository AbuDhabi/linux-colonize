#include "core/founding_fathers.h"

#include <stdint.h>
#include <stdio.h>

/*
 * Rough structural FF election (FUN_4345_0a22 / 0982 / 0342 stand-ins).
 * head.founding_father[i]: -1 unclaimed; 0..3 = owning European nation.
 * nation.founding_fathers[4]: bit i set when nation elected FF i.
 *
 * Jakob Fugger (1) boycott forgive stand-in: gold+50 plus clear Sugar
 * boycott bit (1<<1) and Furs embargo bit (1<<4) on boycott_bitmap;
 * for the human nation also clear head.unknown46[2] (king tax-refuse).
 *
 * Deeper hooks (ctx map/colonies/units when present):
 *   Magellan (5)  — +1 moves_left on nation's sea units (else gold)
 *   Coronado (6)  — map_reveal_radius 2 around owned colonies (else gold)
 *   de Soto (7)   — map_reveal_radius 1 around owned land units (else crosses)
 *   Hudson (8)    — +tools/+furs stock on owned colonies (else gold)
 *   Jones (14)    — free Frigate (Man-O-War fallback) near coast (else gold)
 * Remaining indices keep tiny gold/crosses/bells/tax stand-ins.
 * Wiki/decomp polish still OPEN (unpark #3); Congress debate UI PARKED.
 */

/* King tax-refuse stand-in byte (ai_king unknown46[2]). */
#define FF_KING_BOYCOTT_BYTE 2
/* Sugar cargo idx 1; Furs cargo idx 4 — same bits as king refuse / diplo embargo. */
#define FF_BOYCOTT_SUGAR_BIT (1u << 1)
#define FF_BOYCOTT_FURS_BIT (1u << 4)
#define FF_FUGGER_BOYCOTT_MASK (FF_BOYCOTT_SUGAR_BIT | FF_BOYCOTT_FURS_BIT)

#define FF_CORONADO_REVEAL_RADIUS 2
#define FF_DESOTO_REVEAL_RADIUS 1
#define FF_HUDSON_STOCK_BUMP 50

unsigned founding_fathers_bells_needed(unsigned elected_count) {
  /* First at 40, second at 80, … — linear stand-in for FUN_4345_0982. */
  return 40u * (elected_count + 1u);
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

static void bump_gold(ColonizeCol1Nation* nat, EuropeScreen* europe, uint32_t amount) {
  if (nat->gold < 0xffffffffu - amount) {
    nat->gold += amount;
  } else {
    nat->gold = 0xffffffffu;
  }
  if (europe) {
    europe->gold = (int)nat->gold;
  }
}

static void bump_crosses(ColonizeCol1Nation* nat, EuropeScreen* europe, uint16_t amount) {
  if (nat->current_crosses < 65535u - amount) {
    nat->current_crosses = (uint16_t)(nat->current_crosses + amount);
  } else {
    nat->current_crosses = 65535u;
  }
  if (europe) {
    europe->current_crosses = nat->current_crosses;
  }
}

static void bump_bells(ColonizeCol1Nation* nat, EuropeScreen* europe, uint16_t amount) {
  if (nat->liberty_bells_total < 65535u - amount) {
    nat->liberty_bells_total = (uint16_t)(nat->liberty_bells_total + amount);
  } else {
    nat->liberty_bells_total = 65535u;
  }
  if (europe) {
    europe->liberty_bells_total = nat->liberty_bells_total;
  }
}

static void cut_tax(ColonizeCol1Nation* nat, EuropeScreen* europe, uint8_t amount) {
  if (nat->tax_rate > amount) {
    nat->tax_rate = (uint8_t)(nat->tax_rate - amount);
  } else {
    nat->tax_rate = 0;
  }
  if (europe) {
    europe->tax_percent = nat->tax_rate;
  }
}

static void bump_stock(ColonizeColony* col, int cargo, int amount) {
  if (!col || amount <= 0 || cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }
  if (col->stock[cargo] > 0x7fffffff - amount) {
    col->stock[cargo] = 0x7fffffff;
  } else {
    col->stock[cargo] += amount;
  }
}

/* Magellan: one-shot +1 moves_left on nation's sea units. Returns bumped count. */
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

/* de Soto: reveal radius 1 around owned land units on the map. */
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

/* Hudson: tools + furs stock on owned colonies. Returns colonies bumped. */
static int effect_hudson_stock(ColonizeColonyPool* colonies, int nation_id) {
  if (!colonies) {
    return 0;
  }
  int touched = 0;
  for (int i = 0; i < colonies->colony_count; ++i) {
    ColonizeColony* col = &colonies->colonies[i];
    if (!col->active || col->nation_id != nation_id) {
      continue;
    }
    bump_stock(col, COLONIZE_CARGO_TOOLS, FF_HUDSON_STOCK_BUMP);
    bump_stock(col, COLONIZE_CARGO_FURS, FF_HUDSON_STOCK_BUMP);
    touched++;
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

  /* Fallback: stack on an existing owned sea unit (Europe berth / fleet). */
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
 * Apply FF effect. Deeper structural hooks when ctx provides map/colonies/units;
 * otherwise tiny gold/crosses/bells/tax stand-ins.
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

  switch (ff_index) {
    case 0: /* Adam Smith — factory / industry stand-in */
      bump_gold(nat, europe, 25u);
      break;
    case 1: /* Jakob Fugger — boycott forgive stand-in */
      bump_gold(nat, europe, 50u);
      nat->boycott_bitmap =
        (uint16_t)(nat->boycott_bitmap & (uint16_t)~FF_FUGGER_BOYCOTT_MASK);
      if (nation_id == human_nation && col1) {
        col1->head.unknown46[FF_KING_BOYCOTT_BYTE] = 0;
      }
      break;
    case 2: /* Peter Minuit — cheap land purchase stand-in */
      bump_gold(nat, europe, 30u);
      break;
    case 3: /* Peter Stuyvesant — custom house / trade stand-in */
      bump_gold(nat, europe, 40u);
      break;
    case 4: /* Jan de Witt — trade / finance stand-in */
      cut_tax(nat, europe, 1u);
      break;
    case 5: /* Ferdinand Magellan — naval +1 moves (permanent type bonus OPEN) */
      if (effect_magellan_sea_moves(units, nation_id) <= 0) {
        bump_gold(nat, europe, 35u);
      }
      break;
    case 6: /* Francisco Coronado — colony surround reveal */
      if (effect_coronado_reveal(map, colonies, nation_id) <= 0) {
        bump_gold(nat, europe, 20u);
      }
      break;
    case 7: /* Hernando de Soto — land-unit sight reveal */
      if (effect_desoto_reveal(map, units, nation_id) <= 0) {
        bump_crosses(nat, europe, 6u);
      }
      break;
    case 8: /* Henry Hudson — fur-trade stock stand-in (+tools/+furs) */
      if (effect_hudson_stock(colonies, nation_id) <= 0) {
        bump_gold(nat, europe, 45u);
      }
      break;
    case 9: /* Sieur De La Salle — claim / expansion stand-in */
      bump_gold(nat, europe, 20u);
      break;
    case 10: /* Hernan Cortes — conquest plunder stand-in */
      bump_gold(nat, europe, 100u);
      break;
    case 11: /* George Washington — veteran / REF pressure stand-in */
      if (col1 && col1->head.expeditionary_force[0] > 0) {
        col1->head.expeditionary_force[0]--;
      }
      break;
    case 12: /* Paul Revere — fort / tools stand-in via gold */
      bump_gold(nat, europe, 25u);
      break;
    case 13: /* Francis Drake — privateer plunder stand-in */
      bump_gold(nat, europe, 75u);
      break;
    case 14: /* John Paul Jones — free Frigate / MoW */
      if (!effect_jones_frigate(map, colonies, units, nation_id)) {
        bump_gold(nat, europe, 60u);
      }
      break;
    case 15: /* Thomas Jefferson — liberty bells stand-in */
      bump_bells(nat, europe, 15u);
      break;
    case 16: /* Pocahontas — Indian relations stand-in */
      bump_crosses(nat, europe, 10u);
      break;
    case 17: /* Thomas Paine — tax-weighted bells stand-in */
      bump_bells(nat, europe, (uint16_t)nat->tax_rate);
      break;
    case 18: /* Simon Bolivar — SoL / bells stand-in */
      bump_bells(nat, europe, 20u);
      break;
    case 19: /* Benjamin Franklin — diplomacy / tax ease stand-in */
      cut_tax(nat, europe, 2u);
      break;
    case 20: /* William Brewster — immigration help stand-in */
      bump_crosses(nat, europe, 8u);
      break;
    case 21: /* William Penn — crosses / goodwill stand-in */
      bump_crosses(nat, europe, 5u);
      break;
    case 22: /* Jean de Brebeuf — missionary stand-in */
      bump_crosses(nat, europe, 12u);
      break;
    case 23: /* Juan de Sepulveda — convert / conquest stand-in */
      bump_gold(nat, europe, 30u);
      break;
    case 24: /* Bartolome de las Casas — convert assimilate stand-in */
      bump_crosses(nat, europe, 8u);
      cut_tax(nat, europe, 1u);
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
}
