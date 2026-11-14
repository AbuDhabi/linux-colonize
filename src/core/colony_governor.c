#include "core/colony_governor.h"

#include <string.h>

#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_preview.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/mods.h"
#include "core/world.h"

/*
 * Port-only (MOD_COLONY_GOVERNOR, docs/colony.md "Colony governor"). Seating
 * is a hill climb: every colonist tries every free work plot / field job and
 * every indoor job, then every pair of colonists tries trading places, and a
 * change is kept when colony_preview_compute_w scores better. The score is a
 * per-governor weighting of the preview's net goods, bells, crosses and
 * hammers; a good counts only up to the warehouse room left for it, and a food
 * deficit costs far more than anything else is worth. No specialist bonus: a
 * specialist out-produces anyone on his own job, so the swaps put him there
 * unless his output is unwanted or has no room.
 */

#define GOV_FOOD_DEFICIT_COST 1000.0
#define GOV_PASSES 8

typedef struct GovWeights {
  double goods[COLONIZE_CARGO_COUNT]; /* food here = the net surplus */
  double bells;
  double crosses;
  double hammers;
} GovWeights;

/* One seat: a work plot with a field job, or an indoor workplace. */
typedef struct GovSeat {
  int tile;     /* -1 = indoors */
  int job;      /* field job on a plot */
  int building; /* workplace slot indoors */
} GovSeat;

typedef struct GovCtx {
  const ColonizeWorld* w;
  const GovWeights* wt;
  int room[COLONIZE_CARGO_COUNT]; /* warehouse room left; -1 = unlimited */
  bool pinned[COLONIZE_COLONY_POP_MAX];
  /* Plots the governor may seat on (unblocked, unclaimed or already ours). */
  bool plot_ok[COLONIZE_COLONY_FIELD_TILES_MAX];
  int plots;
} GovCtx;

static int gov_colonist_tile(const ColonizeColony* c, int i) {
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    if (c->tiles[t] == i) {
      return t;
    }
  }
  return -1;
}

static GovSeat gov_seat_of(const ColonizeColony* c, int i) {
  GovSeat s = {gov_colonist_tile(c, i), c->colonists[i].field_job, c->colonists[i].building_type};
  if (s.tile >= 0) {
    s.building = -1;
  } else {
    s.job = -1;
  }
  return s;
}

static bool gov_seat_eq(GovSeat a, GovSeat b) {
  return a.tile == b.tile && (a.tile >= 0 ? a.job == b.job : a.building == b.building);
}

/* The struct edit the assign calls make, without their side effects. */
static void gov_seat_put(ColonizeColony* c, int i, GovSeat s) {
  const int old = gov_colonist_tile(c, i);
  if (old >= 0) {
    c->tiles[old] = -1;
  }
  ColonizeColonist* p = &c->colonists[i];
  if (s.tile >= 0) {
    c->tiles[s.tile] = (int8_t)i;
    p->field_job = s.job;
    p->building_type = -1;
  } else {
    p->field_job = -1;
    p->building_type = s.building;
  }
}

static double gov_score(const GovCtx* g, const ColonizeColony* c) {
  ColonizeColonyPreview pv;
  colony_preview_compute_w(g->w, c, &pv);
  const GovWeights* wt = g->wt;
  double s = pv.food_net < 0 ? pv.food_net * GOV_FOOD_DEFICIT_COST
                             : pv.food_net * wt->goods[COLONIZE_CARGO_FOOD];
  for (int k = 1; k < COLONIZE_CARGO_COUNT; ++k) {
    const int d = pv.goods[k] > g->room[k] && g->room[k] >= 0 ? g->room[k] : pv.goods[k];
    s += d * wt->goods[k];
  }
  s += pv.bells * wt->bells + pv.crosses * wt->crosses + pv.hammers * wt->hammers;
  return s;
}

/* Try colonist i on seat s; keep it when the score beats *best. */
static bool gov_try(const GovCtx* g, ColonizeColony* c, int i, GovSeat s, double* best) {
  const GovSeat old = gov_seat_of(c, i);
  if (gov_seat_eq(old, s)) {
    return false;
  }
  gov_seat_put(c, i, s);
  const double v = gov_score(g, c);
  if (v > *best + 1e-9) {
    *best = v;
    return true;
  }
  gov_seat_put(c, i, old);
  return false;
}

static bool gov_move_pass(const GovCtx* g, ColonizeColony* c, double* best) {
  const ColonizeColonyPool* pool = g->w->colonies;
  const ColonizeWorldMap* map = g->w->map;
  bool moved = false;
  for (int i = 0; i < c->colonist_count; ++i) {
    if (g->pinned[i]) {
      continue;
    }
    for (int t = 0; t < g->plots; ++t) {
      if (!g->plot_ok[t] || (c->tiles[t] >= 0 && c->tiles[t] != i)) {
        continue;
      }
      int dx = 0;
      int dy = 0;
      colonies_field_tile_delta(t, &dx, &dy);
      for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT; ++job) {
        if (colony_yield_for_tile_in_colony(pool, c, map, c->x + dx, c->y + dy, job) <= 0) {
          continue;
        }
        moved |= gov_try(g, c, i, (GovSeat){t, job, -1}, best);
      }
    }
    /* Indoor jobs 9..17; teaching is left to the player. */
    for (int job = COLONIZE_FIELD_JOB_COUNT; job < COLONIES_JOB_TEACHER; ++job) {
      const int b = colonies_job_workplace_building(pool, c, job);
      if (b < 0 || !colonies_building_workable(pool, b) ||
          !colonies_job_row_offered(pool, c, job, c->colonists[i].profession) ||
          colonies_occupation_worker_count(pool, c, job, i) >= COLONIZE_BUILDING_MAX_WORKERS) {
        continue;
      }
      moved |= gov_try(g, c, i, (GovSeat){-1, -1, b}, best);
    }
  }
  return moved;
}

static bool gov_swap_pass(const GovCtx* g, ColonizeColony* c, double* best) {
  bool moved = false;
  for (int i = 0; i < c->colonist_count; ++i) {
    for (int j = i + 1; j < c->colonist_count; ++j) {
      if (g->pinned[i] || g->pinned[j]) {
        continue;
      }
      const GovSeat si = gov_seat_of(c, i);
      const GovSeat sj = gov_seat_of(c, j);
      if (gov_seat_eq(si, sj)) {
        continue;
      }
      /* Vacate first so neither put sees the other's plot as taken. */
      gov_seat_put(c, i, (GovSeat){-1, -1, -1});
      gov_seat_put(c, j, si);
      gov_seat_put(c, i, sj);
      const double v = gov_score(g, c);
      if (v > *best + 1e-9) {
        *best = v;
        moved = true;
      } else {
        gov_seat_put(c, j, (GovSeat){-1, -1, -1});
        gov_seat_put(c, i, si);
        gov_seat_put(c, j, sj);
      }
    }
  }
  return moved;
}

static bool gov_nation_has_custom_house(const ColonizeColonyPool* pool, int nation) {
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (c->active && c->nation_id == nation &&
        colonies_has_building_row(pool, c, COLONY_BUILDING_CUSTOM_HOUSE)) {
      return true;
    }
  }
  return false;
}

/* What a unit of `cargo` fetches after tax: 0 while Europe boycotts it and no
 * colony of the nation has a Custom House (which sells past a boycott). */
static double gov_sale_value(const ColonizeWorld* w, int nation, int cargo, bool custom_house) {
  const ColonizeCol1Save* col1 = w->col1_ok ? w->col1 : NULL;
  int price = europe_nation_sell_price(w->europe, col1, nation, cargo);
  if (!col1 && !w->europe) {
    price = 1; /* no market bound (headless): every good alike */
  }
  if (!custom_house && europe_cargo_boycotted_ex(w->europe, col1, nation, cargo)) {
    return 0.0;
  }
  int tax = w->europe ? w->europe->tax_percent : 0;
  if (col1) {
    tax = col1->head.game_options.woi ? 0 : (int)col1->nation[nation].tax_rate;
  }
  return price * (100 - tax) / 100.0;
}

static void gov_weights(const ColonizeWorld* w, const ColonizeColony* col, int gov, GovWeights* wt) {
  memset(wt, 0, sizeof(*wt));
  /* Sale value, scaled down for the governors that are not after money. */
  const bool custom_house = gov_nation_has_custom_house(w->colonies, col->nation_id);
  for (int k = 1; k < COLONIZE_CARGO_COUNT; ++k) {
    wt->goods[k] = gov_sale_value(w, col->nation_id, k, custom_house) *
                   (gov == COLONY_GOVERNOR_CASH ? 1.0 : 0.25);
  }
  wt->goods[COLONIZE_CARGO_FOOD] = 1.0;
  wt->bells = 1.5;
  wt->crosses = 0.5;
  wt->hammers = 1.0;
  switch (gov) {
    case COLONY_GOVERNOR_FARMING:
      wt->goods[COLONIZE_CARGO_FOOD] = 3.0;
      wt->goods[COLONIZE_CARGO_HORSES] = 4.0;
      wt->hammers = 1.5;
      wt->bells = 1.0;
      break;
    case COLONY_GOVERNOR_INDUSTRY:
      wt->hammers = 4.0;
      wt->goods[COLONIZE_CARGO_TOOLS] = 5.0;
      wt->goods[COLONIZE_CARGO_MUSKETS] = 7.0;
      wt->goods[COLONIZE_CARGO_ORE] = 2.0;
      wt->goods[COLONIZE_CARGO_LUMBER] = 1.0;
      break;
    case COLONY_GOVERNOR_POLITICS:
      wt->bells = 6.0;
      wt->crosses = 5.0;
      break;
    default:
      break;
  }
  /* SoL: bells buy the +1 / +2 production bonus (and shed the Tory
   * penalty) until the colony is at 100%. ponytail: flat factor, not the
   * bonus each bell actually moves. */
  const int sol = colony_prod_sol_percent(w->col1, col);
  wt->bells *= sol < 50 ? 1.5 : sol < 100 ? 1.0 : 0.25;
}

/* Warehouse room left for `cargo`, -1 = unlimited. Food has no cap (DOS
 * clamps every good but Food); a Custom House sells an exported good down to
 * 50 each turn, so it never fills. */
static int gov_room(const ColonizeColonyPool* pool, const ColonizeColony* col, int cargo) {
  const bool exported = colonies_has_building_row(pool, col, COLONY_BUILDING_CUSTOM_HOUSE) &&
                        (col->custom_house_bits & (1u << cargo)) != 0;
  if (cargo == COLONIZE_CARGO_FOOD || exported) {
    return -1;
  }
  const int left = colonies_warehouse_capacity(pool, col, cargo) - col->stock[cargo];
  return left > 0 ? left : 0;
}

static void gov_seat_colonists(const ColonizeWorld* w, ColonizeColony* col, const GovWeights* wt) {
  ColonizeColonyPool* pool = w->colonies;
  GovCtx g;
  memset(&g, 0, sizeof(g));
  g.w = w;
  g.wt = wt;
  for (int k = 0; k < COLONIZE_CARGO_COUNT; ++k) {
    g.room[k] = gov_room(pool, col, k);
  }
  g.plots = colonies_work_plot_count(pool, col);
  if (g.plots > COLONIZE_COLONY_FIELD_TILES_MAX) {
    g.plots = COLONIZE_COLONY_FIELD_TILES_MAX;
  }
  for (int t = 0; t < g.plots; ++t) {
    int dx = 0;
    int dy = 0;
    colonies_field_tile_delta(t, &dx, &dy);
    /* Native land is the player's call (@INDIANLAND); only plots the colony
     * already works may stay worked. */
    g.plot_ok[t] = colonies_plot_blocked_mask(w, col, t) == 0 &&
                   (col->tiles[t] >= 0 ||
                    colonies_indian_claim_tribe_from_w(w, col->nation_id, col->x, col->y,
                                                       col->x + dx, col->y + dy) < 0);
  }
  for (int i = 0; i < col->colonist_count; ++i) {
    const ColonizeColonist* p = &col->colonists[i];
    /* Teachers keep their class; an unseated worker waits for his building. */
    g.pinned[i] = !p->active || p->idle_job1 != 0 ||
                  (p->building_type >= 0 &&
                   colonies_building_occupation(pool, p->building_type) == COLONIES_JOB_TEACHER);
  }

  ColonizeColony plan = *col;
  double best = gov_score(&g, &plan);
  for (int pass = 0; pass < GOV_PASSES; ++pass) {
    const bool moved = gov_move_pass(&g, &plan, &best);
    if (!gov_swap_pass(&g, &plan, &best) && !moved) {
      break;
    }
  }

  /* Commit through the player's own assign calls: everyone who moves gets
   * up first, so no seat or building cap is still held by its old worker. */
  GovSeat target[COLONIZE_COLONY_POP_MAX];
  bool moves[COLONIZE_COLONY_POP_MAX];
  for (int i = 0; i < col->colonist_count; ++i) {
    target[i] = gov_seat_of(&plan, i);
    moves[i] = !gov_seat_eq(target[i], gov_seat_of(col, i));
    if (moves[i]) {
      const int t = gov_colonist_tile(col, i);
      if (t >= 0) {
        colonies_clear_field(pool, col->id, t);
      }
      col->colonists[i].building_type = -1;
    }
  }
  for (int i = 0; i < col->colonist_count; ++i) {
    if (!moves[i]) {
      continue;
    }
    if (target[i].tile >= 0) {
      (void)colonies_assign_field_w(w, col->id, i, target[i].tile, target[i].job);
    } else if (target[i].building >= 0) {
      (void)colonies_assign_workplace(pool, col->id, i, target[i].building);
    }
  }
  colonies_auto_assign_idle(pool, col->id); /* a refused seat never leaves anyone idle */
}

/* ===== Construction ===== */

/* Project codes: @BUILDING rows, or COLONIZE_UNIT_BUILD_* | GOV_UNIT. */
#define GOV_UNIT 0x100
#define GOV_END (-1)

static const int k_gov_farming[] = {
  COLONY_BUILDING_STABLE, COLONY_BUILDING_DOCKS, COLONY_BUILDING_LUMBER_MILL,
  COLONY_BUILDING_PRINTING_PRESS, COLONY_BUILDING_WAREHOUSE, COLONY_BUILDING_NEWSPAPER,
  COLONY_BUILDING_WAREHOUSE_EXPANSION, COLONY_BUILDING_SCHOOLHOUSE, GOV_END,
};
static const int k_gov_cash[] = {
  COLONY_BUILDING_WAREHOUSE, COLONY_BUILDING_DOCKS, COLONY_BUILDING_PRINTING_PRESS,
  COLONY_BUILDING_CUSTOM_HOUSE, COLONY_BUILDING_LUMBER_MILL, COLONY_BUILDING_WAREHOUSE_EXPANSION,
  COLONY_BUILDING_NEWSPAPER, GOV_END,
};
static const int k_gov_industry[] = {
  COLONY_BUILDING_LUMBER_MILL, COLONY_BUILDING_BLACKSMITHS_SHOP, COLONY_BUILDING_ARMORY,
  COLONY_BUILDING_DOCKS, COLONY_BUILDING_PRINTING_PRESS, COLONY_BUILDING_DRYDOCK,
  COLONY_BUILDING_IRON_WORKS, COLONY_BUILDING_MAGAZINE, COLONY_BUILDING_NEWSPAPER,
  COLONY_BUILDING_SHIPYARD, COLONY_BUILDING_ARSENAL, COLONY_BUILDING_WAREHOUSE,
  GOV_UNIT | COLONIZE_UNIT_BUILD_ARTILLERY, GOV_UNIT | COLONIZE_UNIT_BUILD_FRIGATE,
  GOV_UNIT | COLONIZE_UNIT_BUILD_GALLEON, GOV_UNIT | COLONIZE_UNIT_BUILD_MERCHANTMAN, GOV_END,
};
static const int k_gov_politics[] = {
  COLONY_BUILDING_PRINTING_PRESS, COLONY_BUILDING_NEWSPAPER, COLONY_BUILDING_CHURCH,
  COLONY_BUILDING_TOWN_HALL_2, COLONY_BUILDING_TOWN_HALL_3, COLONY_BUILDING_CATHEDRAL,
  COLONY_BUILDING_LUMBER_MILL, COLONY_BUILDING_DOCKS, COLONY_BUILDING_WAREHOUSE, GOV_END,
};

/* Cash: the processing chain of the colony's most valuable raw crop. */
static int gov_cash_chain(const ColonizeWorld* w, const ColonizeColony* col) {
  static const struct { int cargo; int row; } k_chain[] = {
    {COLONIZE_CARGO_SUGAR, COLONY_BUILDING_RUM_DISTILLERS_HOUSE},
    {COLONIZE_CARGO_TOBACCO, COLONY_BUILDING_TOBACCONISTS_HOUSE},
    {COLONIZE_CARGO_COTTON, COLONY_BUILDING_WEAVERS_HOUSE},
    {COLONIZE_CARGO_FURS, COLONY_BUILDING_FUR_TRADERS_HOUSE},
  };
  ColonizeColonyPreview pv;
  colony_preview_compute_w(w, col, &pv);
  int best = -1;
  double best_value = 0.0;
  for (size_t i = 0; i < sizeof(k_chain) / sizeof(k_chain[0]); ++i) {
    const double value =
      pv.field_gross[k_chain[i].cargo] *
      gov_sale_value(w, col->nation_id, k_chain[i].cargo,
                     gov_nation_has_custom_house(w->colonies, col->nation_id));
    if (value > best_value) {
      best_value = value;
      best = colonies_building_row_chain(k_chain[i].row);
    }
  }
  return best;
}

static bool gov_listed(const int* ids, int n, int bid) {
  for (int i = 0; i < n; ++i) {
    if (ids[i] == bid) {
      return true;
    }
  }
  return false;
}

static void gov_pick_project(const ColonizeWorld* w, ColonizeColony* col, int gov) {
  ColonizeColonyPool* pool = w->colonies;
  /* Never over a player's choice: only "(No Production)" or a project that
   * cannot be built (a finished building DOS leaves in the slot, a lost
   * prerequisite), never a queued plan. */
  if (col->build_queue_count > 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = w->map;
  opts.col1 = w->col1_ok ? w->col1 : NULL;
  opts.has_adam_smith = founding_fathers_nation_has(opts.col1, col->nation_id, 0);
  opts.has_peter_stuyvesant = founding_fathers_nation_has(opts.col1, col->nation_id, 3);
  const int cur = col->building_in_production;
  /* The list keeps a finished current project (port_bugs.md #1218). */
  const bool built = colonies_building_type(pool, cur) && cur < COLONIZE_BUILDING_TYPES_MAX &&
                     col->has_building[cur];
  if (cur >= 0 && !built && colonies_build_project_startable(pool, col->id, cur, &opts)) {
    return;
  }
  int ids[64];
  const int n = colonies_list_buildable(pool, col->id, ids, 64, &opts);
  int want[48];
  int nw = 0;
  /* Storage first when a good is full or fills within three turns. */
  ColonizeColonyPreview pv;
  colony_preview_compute_w(w, col, &pv);
  for (int k = 1; k < COLONIZE_CARGO_COUNT; ++k) {
    const int room = gov_room(pool, col, k);
    if (room >= 0 && room <= 3 * (pv.goods[k] > 0 ? pv.goods[k] : 0)) {
      want[nw++] = COLONY_BUILDING_WAREHOUSE;
      want[nw++] = COLONY_BUILDING_WAREHOUSE_EXPANSION;
      break;
    }
  }
  if (gov == COLONY_GOVERNOR_CASH) {
    const int* rows = colonies_building_chain_rows(gov_cash_chain(w, col));
    for (int i = 0; rows && rows[i] >= 0 && nw < 8; ++i) {
      want[nw++] = rows[i];
    }
  }
  const int* list = gov == COLONY_GOVERNOR_FARMING  ? k_gov_farming
                    : gov == COLONY_GOVERNOR_CASH     ? k_gov_cash
                    : gov == COLONY_GOVERNOR_INDUSTRY ? k_gov_industry
                                                      : k_gov_politics;
  for (int i = 0; list[i] != GOV_END && nw < 48; ++i) {
    want[nw++] = list[i];
  }
  for (int i = 0; i < nw; ++i) {
    const int bid = (want[i] & GOV_UNIT) ? (want[i] & ~GOV_UNIT)
                                         : colonies_building_row(pool, (ColonizeBuildingRow)want[i]);
    if (bid >= 0 && gov_listed(ids, n, bid) &&
        colonies_set_construction_ex(pool, col->id, bid, &opts)) {
      return;
    }
  }
  /* Nothing wanted: "(No Production)"; the hammers bank for a later pick. */
  colonies_clear_construction(pool, col->id);
}

void colony_governor_run_w(const ColonizeWorld* w, int colony_id) {
  ColonizeColony* col = w ? colonies_get_mut(w->colonies, colony_id) : NULL;
  if (!col || !col->active || col->governor == COLONY_GOVERNOR_NONE ||
      col->governor >= COLONY_GOVERNOR_COUNT) {
    return;
  }
  GovWeights wt;
  gov_weights(w, col, col->governor, &wt);
  gov_seat_colonists(w, col, &wt);
  gov_pick_project(w, col, col->governor);
}

void colony_governor_run_nation_w(const ColonizeWorld* w, int nation) {
  if (!w || !w->colonies || !mods_on(MOD_COLONY_GOVERNOR) || nation < 0 || nation > 3) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* col = &w->colonies->colonies[i];
    if (col->active && col->nation_id == nation && col->governor) {
      colony_governor_run_w(w, col->id);
    }
  }
}
