#include "core/colony_governor.h"

#include <limits.h>
#include <string.h>

#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_preview.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/dos_buggy.h"
#include "core/europe.h"
#include "core/units.h"
#include "core/founding_fathers.h"
#include "core/mods.h"
#include "core/turn.h"
#include "core/world.h"

/*
 * Port-only (MOD_COLONY_GOVERNOR, docs/colony_governor.md). Seating
 * is a hill climb: every colonist tries every free work plot / field job and
 * every indoor job (teaching too), then every pair of colonists tries trading
 * places, then every pair tries a raw job + its processor (gov_chain_pass),
 * and a change is kept when the plan scores better. The score plays
 * GOV_HORIZON turns of colony_preview_compute_w on a scratch copy, stocking
 * each turn's net goods into the warehouse (clamped to its capacity, so a full
 * good earns nothing) and weighting what went in per governor. Food may run a
 * deficit while the stock covers it; starving, or a stock low enough for
 * turn_produce_food's @FOODLOW popup, costs far more than anything else is
 * worth. Hammers count fully toward the project and its queue, half beyond;
 * tools first toward the tools the project still lacks. Converts indoors pay
 * a per-turn penalty (a third of the output there, a bonus in the field). No specialist
 * bonus: a specialist out-produces anyone on his own job, so the swaps put
 * him there unless his output is unwanted or has no room.
 */

#define GOV_STARVE_COST 1000.0
#define GOV_PASSES 8
#define GOV_HORIZON 3
/* Hammers past the project and its queue carry to the next pick (TB-57;
 * DOS zeroes the bank, so there they are worthless). */
#define GOV_HAMMER_SPARE 0.5
/* AI colonies (colony_governor_seat_w): their hammers, at the human
 * weights, lost to food, so projects stalled a few hammers short. Measured
 * with ai_replay (docs/ai_euro_improvement_spec.md section 7). */
#define GOV_AI_HAMMERS 2.0
/* A convert indoors makes a third of a free colonist; in the field he gets
 * +1. Per turn, so the swaps send converts out unless indoors pays clearly. */
#define GOV_CONVERT_INDOORS 3.0
/* Staying on a cash-crop plot that can still teach the job (turn_produce_
 * discover's roll), so the governor does not shuffle a learner off it. */
#define GOV_STAY_BONUS 3.0
/* A teacher with a student, per turn, plus this per non-expert colonist
 * working the teacher's own expert job. */
#define GOV_TEACH_VALUE 4.0
#define GOV_TEACH_NEED 2.0
/* Plus the teacher's Europe Train price (@JOB column 3) over this, so with
 * nothing better to teach the school seats the dearest expert on offer
 * (Veteran Soldier 2000 -> +1.67, Expert Ore Miner 600 -> +0.5; untrainable
 * rows price 0). The widest gap stays under one GOV_TEACH_NEED, so a teacher
 * whose job a non-expert already works wins over a dearer one. */
#define GOV_TEACH_PRICE 1200.0
/* A field expert the school turns out keeps his surplus over a non-expert
 * for the rest of the game: per teaching turn, this many turns of it (a
 * Schoolhouse graduate after 4). ponytail: flat payback, not the real
 * remaining game or the tier's turn count. */
#define GOV_TEACH_FUTURE 2.5
/* A school or church tier is worth building when seating with it scores
 * this much more a turn (teachers for spare students, preachers for hands
 * whose best job is worth little). */
#define GOV_BUILD_GAIN 2.0
/* Bells past a 100% colony's upkeep: DOS clamps the rebel dividend to the
 * divisor each turn (FUN_364b_0688, viceroy_unpacked.c:57408), so they only
 * feed the nation's Congress / intervention pool. */
#define GOV_BELLS_POOL 0.5
/* Per unit of any output (goods gained, bells, crosses, hammers), so when
 * nothing better is on offer a worker idle in a building without its input
 * moves to one that makes something, even something the weights value at 0
 * (bells past a closed pool, say). Small next to every real weight. */
#define GOV_OUTPUT_TIEBREAK 0.01
/* Per expert on his own job: when trading seats changes no output (a master
 * carpenter and a free colonist in a Lumber Mill that has more hands than
 * lumber), the expert takes his job, so the next pass can move the spare
 * hand to something productive. Below one unit of output. */
#define GOV_EXPERT_TIEBREAK 0.005
/* Industry: tools and muskets past a hold's worth in store are a stockpile,
 * worth what they sell for, so the project's hammers come first. */
#define GOV_INDUSTRY_STOCK 100
/* A staffed processor that makes nothing for want of its input: Phase K's
 * "ran out of X" crumb (turn_produce_crumbs). Per turn and processor, so a
 * carpenter seated to burn the last few lumber leaves once it is gone. */
#define GOV_RAN_OUT_COST 20.0
/* A tool the project lacks once its hammers are all in holds every one of
 * them back (#1863: a blacksmith with ore idled 12 turns two tools short of
 * a Galleon). ponytail: flat factor, not the hammers it frees. */
#define GOV_TOOLS_BLOCKED 4.0
/* Military: a Veteran Soldier graduate counts as this many muskets (a
 * veteran fights at 3 where a colonist with muskets fights at 2), spread over
 * the six turns a College teacher takes. ponytail: flat musket-equivalent,
 * not a model of the nation's army needs. */
#define GOV_VETERAN_MUSKETS 50.0
#define GOV_VETERAN_TURNS 6.0

typedef struct GovWeights {
  double goods[COLONIZE_CARGO_COUNT]; /* food here = the net surplus */
  int full_upto[COLONIZE_CARGO_COUNT]; /* stock that weighs goods[]; INT_MAX = all */
  double goods_past[COLONIZE_CARGO_COUNT]; /* weight past full_upto */
  double bells;
  int bells_upkeep;   /* per turn at that weight; INT_MAX under 100% SoL */
  double bells_pool;  /* weight past bells_upkeep */
  double crosses;
  double hammers;
  double tools_needed; /* a tool the project still lacks */
  double balance;      /* Industry: per added tool matched by an added musket */
  double veteran;      /* Military: per turn a Veteran Soldier teaches */
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
  bool has_project;
  int hammers_need;              /* project and queue hammers left */
  double hammer_spare;           /* weight factor past hammers_need */
  bool food_report;              /* @FOODLOW is on (Colony Report options) */
  bool raw_report;               /* Phase K "ran out of X" is on, human seating only */
  int tools_need;                /* project tools not yet in stock */
  int tools_cost;                /* project tools, 0 = none */
  bool tools_blocked;            /* the project's own hammers are all in */
  int students;                  /* colonists a teacher may take */
  bool pinned[COLONIZE_COLONY_POP_MAX];
  bool learner[COLONIZE_COLONY_POP_MAX]; /* sits where he may learn the job */
  /* Plots the governor may seat on (unblocked, unclaimed or already ours). */
  bool plot_ok[COLONIZE_COLONY_FIELD_TILES_MAX];
  int plots;
  int teach_cap; /* faculty: colonies_school_owned_tier */
  /* Per teaching turn, a teacher of field expert j: his surplus over a
   * non-expert on the best open plot, while plots outnumber the experts. */
  double teach_gain[COLONIZE_FIELD_JOB_COUNT];
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
    s += GOV_TEACH_VALUE + colonies_job_train_cost(t->profession) / GOV_TEACH_PRICE;
    if (t->profession >= 0 && t->profession < COLONIZE_FIELD_JOB_COUNT) {
      s += g->teach_gain[t->profession];
    }
    if (t->profession == UNITS_JOB_SOLDIER) {
      s += g->wt->veteran;
    }
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

/* What a stock of `n` of good k is worth. */
static double gov_stock_value(const GovWeights* wt, int k, int n) {
  const int full = n < wt->full_upto[k] ? n : wt->full_upto[k];
  return full * wt->goods[k] + (n - full) * wt->goods_past[k];
}

static double gov_score(const GovCtx* g, const ColonizeColony* plan) {
  const GovWeights* wt = g->wt;
  ColonizeColony c = *plan;
  int hammers_need = g->hammers_need;
  int tools_need = g->tools_need;
  double s = GOV_HORIZON * gov_teach_value(g, plan);
  for (int i = 0; i < plan->colonist_count; ++i) {
    const ColonizeColonist* p = &plan->colonists[i];
    if (g->learner[i] && gov_seat_eq(gov_seat_of(plan, i), gov_seat_of(g->orig, i))) {
      s += GOV_STAY_BONUS;
    }
    if (p->profession == COLONIZE_PROF_CONVERT && p->building_type >= 0) {
      s -= GOV_HORIZON * GOV_CONVERT_INDOORS;
    }
    const int job = p->building_type >= 0
                      ? colonies_building_occupation(g->w->colonies, p->building_type)
                      : p->field_job;
    if (job >= 0 && job == p->profession) {
      s += GOV_EXPERT_TIEBREAK;
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
    /* turn_produce_food's @FOODLOW: a shortfall with under four turns of it
     * left in store. The governor never earns the player that popup. */
    const int short_by = -pv.food_net;
    if (g->food_report && short_by > 0 && food < short_by * 4) {
      s -= (short_by * 4 - food) * GOV_STARVE_COST;
    }
    c.stock[COLONIZE_CARGO_FOOD] = food;
    int added[COLONIZE_CARGO_COUNT] = {0};
    /* Tools added while under full_upto (the balance counts only those). */
    const int tools_room = wt->full_upto[COLONIZE_CARGO_TOOLS] - c.stock[COLONIZE_CARGO_TOOLS];
    for (int k = 1; k < COLONIZE_CARGO_COUNT; ++k) {
      int after = c.stock[k] + pv.goods[k];
      if (after < 0) {
        after = 0;
      }
      if (g->cap[k] >= 0 && after > g->cap[k]) {
        after = g->cap[k];
      }
      const int d = after - c.stock[k];
      int before = c.stock[k];
      c.stock[k] = after;
      added[k] = d;
      /* Tools that fill the project's need earn tools_needed; tools a
       * gunsmith takes back below the project's cost lose it (#1863: the
       * stock see-sawed 92/98 under a 100-tool Galleon). */
      if (k == COLONIZE_CARGO_TOOLS && g->tools_cost > 0) {
        const int need_after = after < g->tools_cost ? g->tools_cost - after : 0;
        const int u = tools_need - need_after;
        s += u * wt->tools_needed * (g->tools_blocked ? GOV_TOOLS_BLOCKED : 1.0);
        tools_need = need_after;
        before += u;
      }
      s += gov_stock_value(wt, k, after) - gov_stock_value(wt, k, before);
    }
    const int t = added[COLONIZE_CARGO_TOOLS] < tools_room ? added[COLONIZE_CARGO_TOOLS] : tools_room;
    const int m = added[COLONIZE_CARGO_MUSKETS];
    if (t > 0 && m > 0) {
      s += wt->balance * (t < m ? t : m);
    }
    const int h = pv.hammers < hammers_need ? pv.hammers : hammers_need;
    hammers_need -= h;
    if (g->has_project) {
      s += (h + (pv.hammers - h) * g->hammer_spare) * wt->hammers;
    }
    int output = pv.bells + pv.crosses + pv.hammers;
    for (int k = 1; k < COLONIZE_CARGO_COUNT; ++k) {
      output += pv.goods[k] > 0 ? pv.goods[k] : 0;
    }
    s += output * GOV_OUTPUT_TIEBREAK;
    if (g->raw_report) {
      int dry = pv.hammers_capacity > 0 && pv.hammers == 0;
      for (int k = 1; k < COLONIZE_CARGO_COUNT; ++k) {
        dry += pv.craft_capacity[k] > 0 && pv.craft_gross[k] == 0;
      }
      s -= dry * GOV_RAN_OUT_COST;
    }
    const int kept = pv.bells < wt->bells_upkeep ? pv.bells : wt->bells_upkeep;
    s += kept * wt->bells + (pv.bells - kept) * wt->bells_pool + pv.crosses * wt->crosses;
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

/* A raw producer and his processor in one step (@JOB 9..14, Distiller ..
 * Blacksmith, work the good of field job J - 8): a lumberjack alone stocks
 * lumber nobody uses and a carpenter alone runs dry, so neither single move
 * nor swap finds the pair. Colonist i takes the best free plot for the raw
 * job, j the workplace. */
static bool gov_chain_pass(const GovCtx* g, ColonizeColony* c, double* best) {
  const ColonizeColonyPool* pool = g->w->colonies;
  const ColonizeWorldMap* map = g->w->map;
  bool moved = false;
  for (int raw = COLONIZE_JOB_SUGAR_PLANTER; raw <= COLONIZE_JOB_ORE_MINER; ++raw) {
    const int job = raw + COLONIZE_FIELD_JOB_COUNT - 1;
    const int b = colonies_job_workplace_building(pool, c, job);
    if (b < 0 || !colonies_building_workable(pool, b)) {
      continue;
    }
    for (int i = 0; i < c->colonist_count; ++i) {
      for (int j = 0; j < c->colonist_count; ++j) {
        if (i == j || g->pinned[i] || g->pinned[j] ||
            !colonies_job_row_offered(pool, c, job, c->colonists[j].profession)) {
          continue;
        }
        const GovSeat si = gov_seat_of(c, i);
        const GovSeat sj = gov_seat_of(c, j);
        if ((si.tile >= 0 && si.job == raw) || sj.building == b) {
          continue; /* half the pair is there already: the single moves cover it */
        }
        gov_seat_put(c, i, (GovSeat){-1, -1, -1});
        gov_seat_put(c, j, (GovSeat){-1, -1, -1});
        int plot = -1;
        int yield = 0;
        for (int t = 0; t < g->plots; ++t) {
          int dx = 0;
          int dy = 0;
          colonies_field_tile_delta(t, &dx, &dy);
          const int y = g->plot_ok[t] && c->tiles[t] < 0
                          ? colony_yield_for_tile_in_colony(pool, c, map, c->x + dx, c->y + dy, raw)
                          : 0;
          if (y > yield) {
            yield = y;
            plot = t;
          }
        }
        bool kept = false;
        if (plot >= 0 && colonies_occupation_worker_count(pool, c, job, j) < COLONIZE_BUILDING_MAX_WORKERS) {
          gov_seat_put(c, i, (GovSeat){plot, raw, -1});
          gov_seat_put(c, j, (GovSeat){-1, -1, b});
          const double v = gov_score(g, c);
          kept = v > *best + 1e-9;
          if (kept) {
            *best = v;
            moved = true;
          } else {
            gov_seat_put(c, i, (GovSeat){-1, -1, -1});
            gov_seat_put(c, j, (GovSeat){-1, -1, -1});
          }
        }
        if (!kept) {
          gov_seat_put(c, i, si);
          gov_seat_put(c, j, sj);
        }
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

/* Whether the nation's bell pool still buys anything (a Father, intervention). */
static bool gov_bells_pool_open(const ColonizeCol1Save* col1, int nation) {
  if (!col1 || nation < 0 || nation > 3) {
    return true;
  }
  if (col1->head.game_options.woi) {
    return !col1->head.game_options.intervention_announced ||
           col1->nation[nation].liberty_bells_pool < 0x7fff;
  }
  for (unsigned ff = 0; ff < COLONIZE_COL1_FF_COUNT; ++ff) {
    if (!founding_fathers_nation_has(col1, nation, (int)ff)) {
      return true;
    }
  }
  return false;
}

static void gov_weights(const ColonizeWorld* w, const ColonizeColony* col, int gov, GovWeights* wt) {
  memset(wt, 0, sizeof(*wt));
  /* Sale value, scaled down for the governors that are not after money. */
  const bool custom_house = gov_nation_has_custom_house(w->colonies, col->nation_id);
  for (int k = 1; k < COLONIZE_CARGO_COUNT; ++k) {
    wt->goods[k] = gov_sale_value(w, col->nation_id, k, custom_house) *
                   (gov == COLONY_GOVERNOR_CASH ? 1.0 : 0.25);
    wt->goods_past[k] = wt->goods[k];
    wt->full_upto[k] = INT_MAX;
  }
  /* Food grows the colony: 200 make a colonist worth some 500 gold. Every
   * governor wants its project built, listed or not. */
  wt->goods[COLONIZE_CARGO_FOOD] = 2.5;
  wt->bells = 1.0;
  wt->crosses = 0.5;
  wt->hammers = 2.0;
  switch (gov) {
    case COLONY_GOVERNOR_FARMING:
      wt->goods[COLONIZE_CARGO_FOOD] = 4.0;
      wt->bells = 0.75;
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
      wt->full_upto[COLONIZE_CARGO_TOOLS] = GOV_INDUSTRY_STOCK;
      wt->full_upto[COLONIZE_CARGO_MUSKETS] = GOV_INDUSTRY_STOCK;
      break;
    case COLONY_GOVERNOR_MILITARY:
      /* Growth (food, horses via the 1.5x below), muskets with the tools
       * that make them, hammers for walls and artillery. */
      wt->goods[COLONIZE_CARGO_FOOD] = 3.5;
      wt->goods[COLONIZE_CARGO_MUSKETS] = 5.0;
      wt->goods[COLONIZE_CARGO_TOOLS] = 2.0;
      wt->goods[COLONIZE_CARGO_ORE] = 1.0;
      wt->hammers = 3.0;
      wt->veteran = GOV_VETERAN_MUSKETS * wt->goods[COLONIZE_CARGO_MUSKETS] / GOV_VETERAN_TURNS;
      break;
    case COLONY_GOVERNOR_POLITICS:
      wt->bells = 6.0;
      wt->crosses = 5.0;
      break;
    default:
      break;
  }
  /* SoL: bells buy the +1 / +2 production bonus (and shed the Tory
   * penalty), more so under 50%. At 100% the 2 * pop a turn the divisor grows
   * by weigh as much as under 50% (at less, SoL slipped to 99% and called the
   * statesman back every other turn; the +2 latch itself holds down to 95%);
   * the rest only feed the nation's pool, worth nothing once every Father is
   * in before the declaration, or once intervention is announced and the
   * pool's signed score word is full. ponytail: flat factors, not the bonus
   * each bell moves. */
  /* Horses: half again what the governor pays for food (breeding eats the
   * surplus 1:1); the warehouse cap still stops them counting when full. */
  wt->goods[COLONIZE_CARGO_HORSES] = 1.5 * wt->goods[COLONIZE_CARGO_FOOD];
  /* Crosses only draw immigrants, and Europe sends none once the war is on. */
  if (w->col1_ok && w->col1 && w->col1->head.game_options.woi) {
    wt->crosses = 0.0;
  }
  const int sol = colony_prod_sol_percent(w->col1, col);
  wt->bells_pool = gov_bells_pool_open(w->col1, col->nation_id) ? GOV_BELLS_POOL * wt->bells : 0.0;
  wt->bells *= sol >= 50 && sol < 100 ? 1.0 : 1.5;
  wt->bells_upkeep = sol < 100 ? INT_MAX : 2 * col->colonist_count;
  /* A tool the project lacks is worth a tool and a hammer: it needs both. */
  wt->tools_needed = wt->hammers + wt->goods[COLONIZE_CARGO_TOOLS];
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

/* Whether the governor may seat plot t. Native land is the player's call
 * (@INDIANLAND, or Peter Minuit); only plots the colony already works may
 * stay worked. Unbought homeland of a tribe not yet met counts too: DOS seats
 * it without asking, and the seat stamps it bought (colonies_stamp_worked_plot). */
static bool gov_plot_ok(const ColonizeWorld* w, const ColonizeColony* col, int t) {
  const ColonizeCol1Save* col1 = w->col1_ok ? w->col1 : NULL;
  int dx = 0;
  int dy = 0;
  colonies_field_tile_delta(t, &dx, &dy);
  const int x = col->x + dx;
  const int y = col->y + dy;
  return colonies_plot_blocked_mask(w, col, t) == 0 &&
         (col->tiles[t] >= 0 ||
          (colonies_indian_claim_tribe_from_w(w, col->nation_id, col->x, col->y, x, y) < 0 &&
           (map_tile_is_water(w->map, x, y) ||
            colonies_indian_land_purchase_gold(col1, w->map, x, y, col->nation_id) <= 0)));
}

/* rolling: the caller re-picks the project every turn (the AI build
 * cascade), so every hammer counts as project hammers. */
static double gov_plan(const ColonizeWorld* w, const ColonizeColony* col, const GovWeights* wt,
                       bool rolling, ColonizeColony* out) {
  const ColonizeColonyPool* pool = w->colonies;
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
    /* The queue follows the project, so its hammers are wanted too. */
    ColonizeColony next = *col;
    for (int q = 0; q < col->build_queue_count; ++q) {
      int h = 0;
      next.building_in_production = col->build_queue[q];
      if (colonies_construction_cost(pool, &next, &h, NULL)) {
        hammers += h;
      }
    }
    g.has_project = true;
    g.hammers_need = hammers > col->hammers ? hammers - col->hammers : 0;
    g.tools_cost = tools;
    g.tools_blocked = col->hammers >= hammers;
    g.tools_need = tools > col->stock[COLONIZE_CARGO_TOOLS] ? tools - col->stock[COLONIZE_CARGO_TOOLS] : 0;
  }
  g.hammer_spare = dos_buggy_behavior() ? 0.0 : GOV_HAMMER_SPARE;
  if (rolling) {
    g.has_project = true;
    g.hammers_need = INT_MAX;
    /* Past the TB-57 bank cap the carpenters bank nothing (pt3 O3: a pop 4-7
     * AI colony with no project kept carpenters seated at 512). */
    if (!dos_buggy_behavior()) {
      g.hammers_need = col->hammers < COLONIZE_HAMMER_BANK_MAX ? COLONIZE_HAMMER_BANK_MAX - col->hammers : 0;
      g.hammer_spare = 0.0;
    }
  }
  const ColonizeCol1Save* col1 = w->col1_ok ? w->col1 : NULL;
  g.food_report = !col1 || !col1->head.colony_report_options.report_food_shortages;
  g.raw_report = !rolling && (!col1 || !col1->head.colony_report_options.report_raw_materials_shortages);
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
    g.plot_ok[t] = gov_plot_ok(w, col, t);
  }
  for (int i = 0; i < col->colonist_count; ++i) {
    g.students += gov_is_student(&col->colonists[i]);
  }
  /* What training one more field expert is worth (fishermen with Docks and
   * spare water, farmers with spare land, ...). */
  const bool docks = colony_yield_colony_has_docks(pool, col);
  for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT; ++job) {
    int experts = 0;
    for (int i = 0; i < col->colonist_count; ++i) {
      experts += col->colonists[i].active && col->colonists[i].profession == job;
    }
    int open = 0;
    int extra = 0;
    for (int t = 0; t < g.plots; ++t) {
      int dx = 0;
      int dy = 0;
      colonies_field_tile_delta(t, &dx, &dy);
      const int x = col->x + dx;
      const int y = col->y + dy;
      const int plain = colony_yield_for_worker(w->map, x, y, job, COLONIZE_PROF_FREE_COLONIST, docks, 0, false);
      if (!g.plot_ok[t] || plain <= 0) {
        continue;
      }
      ++open;
      const int d = colony_yield_for_worker(w->map, x, y, job, job, docks, 0, false) - plain;
      extra = d > extra ? d : extra;
    }
    if (experts > 0 && open > experts) {
      g.teach_gain[job] = extra * wt->goods[colony_yield_job_cargo(job)] * GOV_TEACH_FUTURE;
    }
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
    const bool swapped = gov_swap_pass(&g, &plan, &best);
    if (!gov_chain_pass(&g, &plan, &best) && !swapped && !moved) {
      break;
    }
  }
  *out = plan;
  return best;
}

static void gov_seat_colonists(const ColonizeWorld* w, ColonizeColony* col, const GovWeights* wt, bool rolling) {
  ColonizeColonyPool* pool = w->colonies;
  ColonizeColony plan;
  (void)gov_plan(w, col, wt, rolling, &plan);

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
  COLONY_BUILDING_STABLE, COLONY_BUILDING_LUMBER_MILL, COLONY_BUILDING_PRINTING_PRESS, COLONY_BUILDING_WAREHOUSE, COLONY_BUILDING_NEWSPAPER,
  COLONY_BUILDING_WAREHOUSE_EXPANSION, COLONY_BUILDING_SCHOOLHOUSE, GOV_END,
};
static const int k_gov_cash[] = {
  COLONY_BUILDING_WAREHOUSE, COLONY_BUILDING_PRINTING_PRESS, COLONY_BUILDING_CUSTOM_HOUSE, COLONY_BUILDING_LUMBER_MILL, COLONY_BUILDING_WAREHOUSE_EXPANSION,
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
static const int k_gov_military[] = {
  COLONY_BUILDING_STOCKADE, COLONY_BUILDING_ARMORY, COLONY_BUILDING_FORT, COLONY_BUILDING_STABLE,
  COLONY_BUILDING_FORTRESS, COLONY_BUILDING_MAGAZINE, COLONY_BUILDING_LUMBER_MILL, COLONY_BUILDING_ARSENAL, COLONY_BUILDING_PRINTING_PRESS,
  COLONY_BUILDING_WAREHOUSE, GOV_UNIT | COLONIZE_UNIT_BUILD_ARTILLERY, GOV_END,
};
static const int k_gov_politics[] = {
  COLONY_BUILDING_PRINTING_PRESS, COLONY_BUILDING_NEWSPAPER, COLONY_BUILDING_CHURCH,
  COLONY_BUILDING_TOWN_HALL_2, COLONY_BUILDING_TOWN_HALL_3, COLONY_BUILDING_CATHEDRAL,
  COLONY_BUILDING_LUMBER_MILL, COLONY_BUILDING_WAREHOUSE, GOV_END,
};

/* Cash: every crop chain that pays, most valuable first, each with all its
 * tiers (house, shop, factory; the buildable list keeps out what is built or
 * needs Adam Smith). A chain is worth the crop the colony grows now, or its
 * best seatable plot if more, sold as the processed good (rum .. coats, cargo
 * raw + 8). Appends to want[] and returns the new count. */
static int gov_cash_chains(const ColonizeWorld* w, const ColonizeColony* col, int* want, int nw, int max) {
  static const int k_house[] = {
    COLONY_BUILDING_RUM_DISTILLERS_HOUSE, COLONY_BUILDING_TOBACCONISTS_HOUSE,
    COLONY_BUILDING_WEAVERS_HOUSE, COLONY_BUILDING_FUR_TRADERS_HOUSE,
  };
  const ColonizeColonyPool* pool = w->colonies;
  const bool custom_house = gov_nation_has_custom_house(pool, col->nation_id);
  int plots = colonies_work_plot_count(pool, col);
  if (plots > COLONIZE_COLONY_FIELD_TILES_MAX) {
    plots = COLONIZE_COLONY_FIELD_TILES_MAX;
  }
  ColonizeColonyPreview pv;
  colony_preview_compute_w(w, col, &pv);
  double value[4];
  for (int i = 0; i < 4; ++i) {
    const int raw = COLONIZE_CARGO_SUGAR + i; /* = field job SUGAR_PLANTER + i */
    int grow = pv.field_gross[raw];
    for (int t = 0; t < plots; ++t) {
      int dx = 0;
      int dy = 0;
      colonies_field_tile_delta(t, &dx, &dy);
      if (gov_plot_ok(w, col, t)) {
        const int y = colony_yield_for_tile_in_colony(pool, col, w->map, col->x + dx, col->y + dy, raw);
        grow = y > grow ? y : grow;
      }
    }
    value[i] = grow * gov_sale_value(w, col->nation_id, raw + 8, custom_house);
  }
  for (;;) {
    int best = -1;
    for (int i = 0; i < 4; ++i) {
      if (value[i] > 0.0 && (best < 0 || value[i] > value[best])) {
        best = i;
      }
    }
    if (best < 0) {
      return nw;
    }
    value[best] = 0.0;
    const int* rows = colonies_building_chain_rows(colonies_building_row_chain(k_house[best]));
    for (int r = 0; rows && rows[r] >= 0 && nw < max; ++r) {
      want[nw++] = rows[r];
    }
  }
}

/* The next unbuilt tier of the school and church chains, and the Docks (the
 * fishing tier only: Drydock and Shipyard seat no one), each wanted when the
 * seating, run as if it stood, beats today's by GOV_BUILD_GAIN a turn.
 * Appends to want[] and returns the new count. */
static int gov_probe_rows(const ColonizeWorld* w, const ColonizeColony* col, int gov, int* want,
                          int nw, int max) {
  static const int k_chain[] = {COLONY_BUILDING_DOCKS, COLONY_BUILDING_SCHOOLHOUSE,
                                 COLONY_BUILDING_CHURCH};
  const ColonizeColonyPool* pool = w->colonies;
  GovWeights wt;
  gov_weights(w, col, gov, &wt);
  ColonizeColony plan;
  const double base = gov_plan(w, col, &wt, false, &plan);
  for (int k = 0; k < 3 && nw < max; ++k) {
    const int* rows = colonies_building_chain_rows(colonies_building_row_chain(k_chain[k]));
    int r = 0;
    while (rows && rows[r] >= 0 && colonies_has_building_row(pool, col, rows[r])) {
      ++r;
    }
    const int bid = rows && rows[r] >= 0 ? colonies_building_row(pool, rows[r]) : -1;
    if ((k_chain[k] == COLONY_BUILDING_DOCKS && r > 0) || bid < 0 || bid >= COLONIZE_BUILDING_TYPES_MAX) {
      continue;
    }
    ColonizeColony probe = *col;
    probe.has_building[bid] = true;
    if (gov_plan(w, &probe, &wt, false, &plan) - base > GOV_HORIZON * GOV_BUILD_GAIN) {
      want[nw++] = rows[r];
    }
  }
  return nw;
}

static bool gov_listed(const int* ids, int n, int bid) {
  for (int i = 0; i < n; ++i) {
    if (ids[i] == bid) {
      return true;
    }
  }
  return false;
}

/* rethink: a repeating unit project just completed; pick anew once, keeping
 * it when nothing on the lists is wanted. */
static void gov_pick_project(const ColonizeWorld* w, ColonizeColony* col, int gov, bool rethink) {
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
  const bool keep = cur >= 0 && !built && colonies_build_project_startable(pool, col->id, cur, &opts);
  if (keep && !rethink) {
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
  nw = gov_probe_rows(w, col, gov, want, nw, 48);
  if (gov == COLONY_GOVERNOR_CASH) {
    nw = gov_cash_chains(w, col, want, nw, 48);
  }
  const int* list = gov == COLONY_GOVERNOR_FARMING  ? k_gov_farming
                    : gov == COLONY_GOVERNOR_CASH     ? k_gov_cash
                    : gov == COLONY_GOVERNOR_INDUSTRY ? k_gov_industry
                    : gov == COLONY_GOVERNOR_MILITARY ? k_gov_military
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
  if (!keep) {
    colonies_clear_construction(pool, col->id);
  }
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
  gov_pick_project(w, col, col->governor, col->governor_rethink);
  col->governor_rethink = false;
  gov_seat_colonists(w, col, &wt, false);
}

bool colony_governor_feeds_one_more(const ColonizeWorld* w, const ColonizeColony* col) {
  if (!w || !w->map || !w->colonies || !col) {
    return false;
  }
  ColonizeColonyPreview pv;
  colony_preview_compute_w(w, col, &pv);
  int best = 0;
  const int plots = colonies_work_plot_count(w->colonies, col);
  for (int t = 0; t < plots && t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    if (col->tiles[t] >= 0 || !gov_plot_ok(w, col, t)) {
      continue;
    }
    int dx = 0;
    int dy = 0;
    colonies_field_tile_delta(t, &dx, &dy);
    for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT; ++job) {
      if (colony_yield_job_cargo(job) == COLONIZE_CARGO_FOOD) {
        const int y = colony_yield_for_tile_in_colony(w->colonies, col, w->map, col->x + dx, col->y + dy, job);
        best = y > best ? y : best;
      }
    }
  }
  return pv.food_net + best >= TURN_FOOD_PER_COLONIST;
}

void colony_governor_seat_w(const ColonizeWorld* w, int colony_id, int gov) {
  ColonizeColony* col = w ? colonies_get_mut(w->colonies, colony_id) : NULL;
  if (!col || !col->active || gov <= COLONY_GOVERNOR_NONE || gov >= COLONY_GOVERNOR_COUNT) {
    return;
  }
  GovWeights wt;
  gov_weights(w, col, gov, &wt);
  wt.hammers *= GOV_AI_HAMMERS;
  wt.tools_needed = wt.hammers + wt.goods[COLONIZE_CARGO_TOOLS];
  gov_seat_colonists(w, col, &wt, true);
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
