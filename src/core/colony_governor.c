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
#include "core/turn.h"
#include "core/world.h"

/*
 * Port-only (MOD_COLONY_GOVERNOR, docs/colony.md "Colony governor"). Seating
 * is a hill climb: every colonist tries every free work plot / field job and
 * every indoor job (teaching too), then every pair of colonists tries trading
 * places, and a change is kept when the plan scores better. The score plays
 * GOV_HORIZON turns of colony_preview_compute_w on a scratch copy, stocking
 * each turn's net goods into the warehouse (clamped to its capacity, so a full
 * good earns nothing) and weighting what went in per governor. Food may run a
 * deficit while the stock covers it; going below zero (starvation) costs far
 * more than anything else is worth. Hammers count only toward the project's
 * remaining cost, tools toward the tools it still lacks first. No specialist
 * bonus: a specialist out-produces anyone on his own job, so the swaps put
 * him there unless his output is unwanted or has no room.
 */

#define GOV_STARVE_COST 1000.0
#define GOV_PASSES 8
#define GOV_HORIZON 3
/* Hammers past the project's cost: they bank, but are mostly worthless. */
#define GOV_HAMMER_SPARE 0.1
/* Staying on a cash-crop plot that can still teach the job (turn_produce_
 * discover's roll), so the governor does not shuffle a learner off it. */
#define GOV_STAY_BONUS 3.0
/* A teacher with a student, per turn, plus this per non-expert colonist
 * working the teacher's own expert job. */
#define GOV_TEACH_VALUE 4.0
#define GOV_TEACH_NEED 2.0

typedef struct GovWeights {
  double goods[COLONIZE_CARGO_COUNT]; /* food here = the net surplus */
  double bells;
  double crosses;
  double hammers;
  double tools_needed; /* a tool the project still lacks */
  double balance;      /* Industry: per added tool matched by an added musket */
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
  int cap[COLONIZE_CARGO_COUNT]; /* warehouse capacity; -1 = unlimited */
  int hammers_need;              /* project hammers left; 0 = nothing to build */
  int tools_need;                /* project tools not yet in stock */
  int students;                  /* colonists a teacher may take */
  bool pinned[COLONIZE_COLONY_POP_MAX];
  bool learner[COLONIZE_COLONY_POP_MAX]; /* sits where he may learn the job */
  /* Plots the governor may seat on (unblocked, unclaimed or already ours). */
  bool plot_ok[COLONIZE_COLONY_FIELD_TILES_MAX];
  int plots;
  int teach_cap; /* faculty: colonies_school_owned_tier */
  const ColonizeColony* orig;
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

static bool gov_is_teacher(const ColonizeColonyPool* pool, const ColonizeColonist* p) {
  return p->building_type >= 0 &&
         colonies_building_occupation(pool, p->building_type) == COLONIES_JOB_TEACHER;
}

/* Free colonists, servants and criminals: whom a school can teach. */
static bool gov_is_student(const ColonizeColonist* p) {
  return p->active && (p->profession == COLONIZE_PROF_FREE_COLONIST ||
                       p->profession == UNITS_JOB_COLONIST || p->profession == UNITS_JOB_NONE ||
                       p->profession == COLONIZE_PROF_INDENTURED ||
                       p->profession == COLONIZE_PROF_CRIMINAL);
}

/* One turn of teaching: every teacher with a student, worth more the more
 * non-experts work his own job (@JOB row = expert profession). */
static double gov_teach_value(const GovCtx* g, const ColonizeColony* c) {
  const ColonizeColonyPool* pool = g->w->colonies;
  double s = 0.0;
  int taught = 0;
  for (int i = 0; i < c->colonist_count && taught < g->students; ++i) {
    const ColonizeColonist* t = &c->colonists[i];
    if (!gov_is_teacher(pool, t)) {
      continue;
    }
    ++taught;
    s += GOV_TEACH_VALUE;
    for (int j = 0; j < c->colonist_count; ++j) {
      const ColonizeColonist* p = &c->colonists[j];
      const int job = p->building_type >= 0 ? colonies_building_occupation(pool, p->building_type)
                                            : p->field_job;
      if (j != i && job == t->profession && p->profession != t->profession) {
        s += GOV_TEACH_NEED;
      }
    }
  }
  return s;
}

static double gov_score(const GovCtx* g, const ColonizeColony* plan) {
  const GovWeights* wt = g->wt;
  ColonizeColony c = *plan;
  int hammers_need = g->hammers_need;
  int tools_need = g->tools_need;
  double s = GOV_HORIZON * gov_teach_value(g, plan);
  for (int i = 0; i < plan->colonist_count; ++i) {
    if (g->learner[i] && gov_seat_eq(gov_seat_of(plan, i), gov_seat_of(g->orig, i))) {
      s += GOV_STAY_BONUS;
    }
  }
  for (int turn = 0; turn < GOV_HORIZON; ++turn) {
    ColonizeColonyPreview pv;
    colony_preview_compute_w(g->w, &c, &pv);
    int food = c.stock[COLONIZE_CARGO_FOOD] + pv.food_net;
    s += pv.food_net * wt->goods[COLONIZE_CARGO_FOOD];
    if (food < 0) {
      s += food * GOV_STARVE_COST;
      food = 0;
    }
    c.stock[COLONIZE_CARGO_FOOD] = food;
    int added[COLONIZE_CARGO_COUNT] = {0};
    for (int k = 1; k < COLONIZE_CARGO_COUNT; ++k) {
      int after = c.stock[k] + pv.goods[k];
      if (after < 0) {
        after = 0;
      }
      if (g->cap[k] >= 0 && after > g->cap[k]) {
        after = g->cap[k];
      }
      int d = after - c.stock[k];
      c.stock[k] = after;
      added[k] = d;
      if (k == COLONIZE_CARGO_TOOLS && d > 0 && tools_need > 0) {
        const int u = d < tools_need ? d : tools_need;
        s += u * wt->tools_needed;
        tools_need -= u;
        d -= u;
      }
      s += d * wt->goods[k];
    }
    const int t = added[COLONIZE_CARGO_TOOLS];
    const int m = added[COLONIZE_CARGO_MUSKETS];
    if (t > 0 && m > 0) {
      s += wt->balance * (t < m ? t : m);
    }
    const int h = pv.hammers < hammers_need ? pv.hammers : hammers_need;
    hammers_need -= h;
    if (g->hammers_need > 0) {
      s += (h + (pv.hammers - h) * GOV_HAMMER_SPARE) * wt->hammers;
    }
    s += pv.bells * wt->bells + pv.crosses * wt->crosses;
  }
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
    /* Indoor jobs 9..18; a teacher only while someone is left to teach. */
    for (int job = COLONIZE_FIELD_JOB_COUNT; job <= COLONIES_JOB_TEACHER; ++job) {
      const int b = colonies_job_workplace_building(pool, c, job);
      const int cap = job == COLONIES_JOB_TEACHER ? (g->students > 0 ? g->teach_cap : 0)
                                                  : COLONIZE_BUILDING_MAX_WORKERS;
      if (b < 0 || !colonies_building_workable(pool, b) ||
          !colonies_job_row_offered(pool, c, job, c->colonists[i].profession) ||
          colonies_occupation_worker_count(pool, c, job, i) >= cap) {
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

/* What a unit of `cargo` fetches after tax: 0 while Europe boycotts it, or
 * during the War of Independence, and no colony of the nation has a Custom
 * House (which sells past both). */
static double gov_sale_value(const ColonizeWorld* w, int nation, int cargo, bool custom_house) {
  const ColonizeCol1Save* col1 = w->col1_ok ? w->col1 : NULL;
  int price = europe_nation_sell_price(w->europe, col1, nation, cargo);
  if (!col1 && !w->europe) {
    price = 1; /* no market bound (headless): every good alike */
  }
  if (!custom_house && ((col1 && col1->head.game_options.woi) ||
                        europe_cargo_boycotted_ex(w->europe, col1, nation, cargo))) {
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
      /* Tools and muskets in step: two blacksmiths feeding one gunsmith
       * beat either alone (tools are the musket input). */
      wt->hammers = 4.0;
      wt->goods[COLONIZE_CARGO_TOOLS] = 4.0;
      wt->goods[COLONIZE_CARGO_MUSKETS] = 4.0;
      wt->balance = 6.0;
      wt->goods[COLONIZE_CARGO_ORE] = 1.0;
      wt->goods[COLONIZE_CARGO_LUMBER] = 0.5;
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
  /* Tools the project lacks come before anything they could be sold for. */
  wt->tools_needed = 2.0 * wt->hammers;
  if (wt->tools_needed < wt->goods[COLONIZE_CARGO_TOOLS]) {
    wt->tools_needed = wt->goods[COLONIZE_CARGO_TOOLS];
  }
}

/* Warehouse capacity for `cargo`, -1 = unlimited. Food has no cap (DOS
 * clamps every good but Food); a Custom House sells an exported good down to
 * 50 each turn, so it never fills. */
static int gov_cap(const ColonizeColonyPool* pool, const ColonizeColony* col, int cargo) {
  const bool exported = colonies_has_building_row(pool, col, COLONY_BUILDING_CUSTOM_HOUSE) &&
                        (col->custom_house_bits & (1u << cargo)) != 0;
  if (cargo == COLONIZE_CARGO_FOOD || exported) {
    return -1;
  }
  return colonies_warehouse_capacity(pool, col, cargo);
}

/* Warehouse room left for `cargo`, -1 = unlimited. */
static int gov_room(const ColonizeColonyPool* pool, const ColonizeColony* col, int cargo) {
  const int cap = gov_cap(pool, col, cargo);
  if (cap < 0) {
    return -1;
  }
  return cap > col->stock[cargo] ? cap - col->stock[cargo] : 0;
}

static void gov_seat_colonists(const ColonizeWorld* w, ColonizeColony* col, const GovWeights* wt) {
  ColonizeColonyPool* pool = w->colonies;
  GovCtx g;
  memset(&g, 0, sizeof(g));
  g.w = w;
  g.wt = wt;
  g.orig = col;
  for (int k = 0; k < COLONIZE_CARGO_COUNT; ++k) {
    g.cap[k] = gov_cap(pool, col, k);
  }
  int hammers = 0;
  int tools = 0;
  if (colonies_construction_cost(pool, col, &hammers, &tools)) {
    g.hammers_need = hammers > col->hammers ? hammers - col->hammers : 0;
    g.tools_need = tools > col->stock[COLONIZE_CARGO_TOOLS] ? tools - col->stock[COLONIZE_CARGO_TOOLS] : 0;
  }
  g.teach_cap = colonies_school_owned_tier(pool, col);
  /* On-the-job learning (turn_produce_discover): a non-expert on a cash-crop
   * plot while the nation has no expert of that job yet. */
  uint8_t census[32];
  turn_tally_professions_w(w, col->nation_id, census);
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
    g.students += gov_is_student(&col->colonists[i]);
  }
  for (int i = 0; i < col->colonist_count; ++i) {
    const ColonizeColonist* p = &col->colonists[i];
    /* Teachers keep their class while anyone is left to teach; an unseated
     * worker waits for his building. */
    g.pinned[i] = !p->active || p->idle_job1 != 0 || (g.students > 0 && gov_is_teacher(pool, p));
    g.learner[i] = gov_is_student(p) &&
                   gov_colonist_tile(col, i) >= 0 && p->field_job >= 1 &&
                   p->field_job <= COLONIZE_JOB_FUR_TRAPPER && census[p->field_job] == 0;
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
  /* Project first: the seating aims its hammers and tools at it. */
  gov_pick_project(w, col, col->governor);
  gov_seat_colonists(w, col, &wt);
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
