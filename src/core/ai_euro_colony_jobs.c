/*
 * Euro AI — FUN_15eb_28c8 plot scorer + FUN_5952_035e job/indoor/lumber/carpenter/build-cascade/specialist arms
 *
 * Split out of ai_euro.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_euro_internal.h. See ai_euro.c for the dispatcher entry point.
 *
 * Sections:
 *   FUN_15eb_28c8 colonist work-plot scorer + auto-assign
 *   FUN_5952_035e job chains / producible / want weights
 *   FUN_5952_035e indoor-workplace pass (raw 94784-94860)
 *   FUN_5952_035e forced-lumberjack pass + lumber buy (raw 94659-94689)
 *   FUN_5952_035e carpenter-staffing arm (raw 94690-94740)
 *   FUN_5952_035e construction-project cascade (asm 5952:21d4-5952:2747)
 *   FUN_5952_035e train/specialist arms + 28c8 colony-tick reassign
 */

#include "core/internal.h"
#include "core/ai_euro.h"

#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_king.h"
#include "core/ai_goals.h"
#include "core/assets.h"
#include "core/ai_euro_internal.h"
#include "core/colony.h"
#include "core/colony_craft.h"
#include "core/colony_yield.h"
#include "core/colony_production.h"
#include "core/col1_save.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/units.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defined further down with the FUN_5952_035e tick; 28c8 needs the same
 * DS:0x8dc8 / DS:0x8e0a ledger pair. */
void ai_euro_5952_ledgers(
  const ColonizeWorld* world,
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const ColonizeCol1Save* col1,
  int gross[AI_EURO_5952_LEDGER_SLOTS],
  int demand[AI_EURO_5952_LEDGER_SLOTS]
);

/*
 * FUN_15eb_28c8 — colonist work-plot scorer, DOS-literal (raw 12908-13158).
 * bugs.md #570 (2026-09-22): the previous body was a structural sketch and
 * diverged from the decomp in six ways (no distance term, no food-emergency
 * branch, no cargo-weight branch, labour penalty applied unconditionally and
 * AI-only, sticky x2 unconditional, colonist headcount used where DOS clamps
 * against WAREHOUSE stock). Ported here term for term.
 *
 * Shape of the DOS body (`param_1` = colonist slot, `param_2` = restrict):
 *   bVar1 = colony is HUMAN-controlled (colony+0x1a < 4 && DS:0x543f[nation
 *           *0x34] == 0, raw 12952-12958) — this is NOT an AI-only routine:
 *           FUN_15eb_2ea0 runs it for every colony (see
 *           ai_euro_28c8_auto_assign_plots).
 *   bVar2 = food emergency: `DS:0x35e == 0 && DS:0x8dc8 < DS:0x8e0a`
 *           (food gross < food demand) and, for an AI colony, also
 *           `colony+0x9a <= capacity && !(DS:0x8e32*0x10 < colony+0x9a)`.
 *           DS:0x35e is set to 1 for the whole FUN_5952_035e AI colony tick
 *           (raw 94628 set / 95975 clear), so the emergency branch is
 *           unreachable from the tick — hence `in_ai_tick` here.
 *   score = yld*8 + (7 - |dx| - |dy|)                       (raw 13017-13024)
 *   sticky x2 when the colonist already holds this job — HUMAN only (bVar1).
 *   then either the food-emergency branch (jobs 0/8 get <<5, Fisherman +8,
 *   minus the DS:0x2f7a labour byte with a +0x18 forest surcharge, floor 1;
 *   an Indian-claimed plot halves) or the cargo-weight branch (per-nation
 *   DS:0x84bc price row, shortfall/unmet bumps, consumer-building term,
 *   Indian-alarm subtraction, `score = (local_38 + local_4) * score`).
 *
 * Deliberate substitutions (no DOS constant invented):
 *   - DS:0x8e32 "production shortfall" / DS:0x8e5a "unmet after stock" are
 *     recomputed from the same ledger pair the 5952 tick uses
 *     (ai_euro_5952_ledgers = DS:0x8dc8/0x8e0a) with FUN_15eb_0b52's own
 *     rule (colony_craft.c header).
 *   - FUN_15eb_15c6(DS:0x2b6[job]) is 0 when the field good has no consumer
 *     job, else 1 (+1 when that job's base building itself has a parent tier,
 *     which no chain root in the port's @BUILDING table has) — see
 *     k_ai_euro_28c8_consumer_chain.
 *   - byte[FUN_15eb_0470()+0x329] is the colony's work-plot count. Read off
 *     VICEROY.EXE (file offset 121248 + 0x329): {0, 4, 8, 12, 20}, indexed by
 *     FUN_15eb_0470() = min(FUN_15eb_039e(10), 2) + 2. 039e(10) is 0 in every
 *     state stock DOS can reach, so the count is 8 there — but a save may
 *     carry the cut rows, so it is read per colony from
 *     colonies_work_plot_count (bugs.md #570, #593).
 */

/* DS:0x2b6 read as 28c8 reads it: field job -> the JOB that consumes its
 * good (jobs 9..12/14 there are input cargos; 28c8 only indexes 0..8).
 * {-1,9,10,11,12,-1,14,-1,-1} -> building chain via DS:0x2f4 (FUN_15eb_0aec). */
static int ai_euro_28c8_consumer_chain(int field_job) {
  switch (field_job) {
    case 1: return COLONIES_CHAIN_RUM;         /* Sugar   -> Distiller  (9) */
    case 2: return COLONIES_CHAIN_TOBACCONIST; /* Tobacco -> Tobacconist(10) */
    case 3: return COLONIES_CHAIN_WEAVER;      /* Cotton  -> Weaver    (11) */
    case 4: return COLONIES_CHAIN_FUR;         /* Furs    -> Fur Trader(12) */
    case COLONIZE_JOB_ORE_MINER: return COLONIES_CHAIN_BLACKSMITH; /* Ore (14) */
    default: return -1;
  }
}

/* FUN_15eb_039e(b): owned buildings walking b -> parent -> ... `tiers` is how
 * many tiers from the chain root that walk covers (039e(3) = the Armory row
 * alone, 039e(0x28) = Blacksmith's House + Shop). */
static int ai_euro_28c8_chain_owned_upto(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int chain, int tiers
) {
  const char* const* names = colonies_building_chain(chain);
  if (!pool || !col || !names) {
    return 0;
  }
  int n = 0;
  for (int i = 0; names[i] && i < tiers; ++i) {
    const int idx = colonies_find_building(pool, names[i]);
    if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && col->has_building[idx]) {
      ++n;
    }
  }
  return n;
}

typedef struct AiEuro28c8Env {
  bool human;       /* bVar1 */
  bool food_emerg;  /* bVar2 */
  int capacity;     /* local_6 = FUN_15eb_0a50 */
  int continent;    /* local_e = FUN_137f_02a0(colony) */
  int mil_count;    /* local_14 = own units with @UNIT defense > 1 */
  int lumber_gross; /* DS:0x8dd2 = gross[Lumber] */
  int turn;         /* DS:0x538e */
  int shortfall[COLONIZE_CARGO_COUNT]; /* DS:0x8e32 */
  int unmet[COLONIZE_CARGO_COUNT];     /* DS:0x8e5a */
} AiEuro28c8Env;

static void ai_euro_28c8_env(
  const ColonizeTurnContext* ctx, const ColonizeColony* col, int in_ai_tick,
  AiEuro28c8Env* env
) {
  const ColonizeCol1Save* col1 = ctx->col1_ok ? ctx->col1 : NULL;
  memset(env, 0, sizeof(*env));
  env->human = col->nation_id == ctx->human_nation;
  env->capacity = colonies_warehouse_capacity(ctx->colonies, col, COLONIZE_CARGO_FOOD);
  env->continent = ctx->map ? map_continent_id_at(ctx->map, col->x, col->y) : -1;
  env->turn = col1 ? (int)col1->head.turn : 0;

  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  const ColonizeWorld w = world_from_turn_ctx(ctx);
  ai_euro_5952_ledgers(&w, ctx->colonies, col, col1, gross, demand);
  env->lumber_gross = gross[COLONIZE_CARGO_LUMBER];
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    /* DS:0x8e32 production shortfall and DS:0x8e5a unmet-after-stock,
     * FUN_15eb_0b52's own rule (colony_craft.c header). */
    const int miss = demand[c] - gross[c];
    env->shortfall[c] = miss > 0 ? miss : 0;
    const int after = miss - col->stock[c];
    env->unmet[c] = after > 0 ? after : 0;
  }

  /* bVar2, raw 12974-12980. */
  bool emerg = !in_ai_tick && gross[COLONIZE_CARGO_FOOD] < demand[COLONIZE_CARGO_FOOD];
  if (!env->human) {
    const int food_stock = col->stock[COLONIZE_CARGO_FOOD];
    emerg = emerg && food_stock <= env->capacity;
    if (env->shortfall[COLONIZE_CARGO_FOOD] * 0x10 < food_stock) {
      emerg = false;
    }
  }
  env->food_emerg = emerg;

  /* local_14 (AI only, raw 12960-12966): own units whose @UNIT defense
   * (DOS type*0xe + 0x5235) is > 1. */
  if (!env->human && ctx->units) {
    for (int i = 0; i < ctx->units->unit_count; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != col->nation_id) {
        continue;
      }
      if (u->type_index >= 0 && u->type_index < ctx->units->type_count &&
          ctx->units->types[u->type_index].defense > 1) {
        ++env->mil_count;
      }
    }
  }
}

/*
 * 28c8 scorer body. `profession` < 0 scores plain tile yields (the
 * structural/test entry point); otherwise the colonist's real profession
 * goes through colony_yield_for_worker (DOS 1068 trial-assigns the job, so
 * 18ec sees the expert) — that is what the live tick and the human
 * auto-assign use.
 *
 * `restrict_job` is DOS's `param_2` when it is a real job index rather than
 * one of the −1 / −2 modes: the search is then confined to that one field
 * job. `in_ai_tick` is DS:0x35e (1 inside FUN_5952_035e).
 */
static int ai_euro_28c8_score_full(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* col,
  int colonist_slot,
  int profession,
  int restrict_job,
  int in_ai_tick,
  AiEuro28c8JobCandidate* out_best
) {
  const ColonizeColonist* self = &col->colonists[colonist_slot];
  if (!self->active) {
    return 0;
  }
  const int current_job = self->field_job; /* iVar4 = FUN_15eb_0e18 */
  AiEuro28c8Env env;
  ai_euro_28c8_env(ctx, col, in_ai_tick, &env);
  const ColonizeCol1Save* col1 = ctx->col1_ok ? ctx->col1 : NULL;
  const ColonizeWorld world = world_from_turn_ctx(ctx);
  /* The scorer must answer the Fisherman gate exactly as the tick does, or it
   * assigns plots the tick then pays 0 for. DOS's 18ec gate is
   * FUN_15eb_038e(6), the building alone (smell audit 2026-09-10 E#2). */
  /* The docks gate (raw 11955) is unconditional in DOS: it never depends on
   * whether a worker profession is known, so answer it from the colony in both
   * cases (bugs.md #609). */
  const bool has_docks = colony_yield_colony_has_docks(ctx->colonies, col);
  const int sol_b_field =
    profession >= 0 ? colony_prod_sol_bonus_field(col1, col) : 0;
  /* Same for Hudson (raw 11970-11973): `local_14 == 4 && FF_has(nation, 8)`
   * keys off the JOB, never off a known worker profession — bugs.md #857,
   * the #609 defect class. */
  const bool has_hudson =
    col1 && founding_fathers_nation_has(col1, col->nation_id, FF_HENRY_HUDSON);

  /* local_22 (raw 12987) = DS:0x329[FUN_15eb_0470()] — the colony's own ring
   * size, 8 unless it owns the cut Town Hall rows (bugs.md #593). */
  const int ring_28c8 = colonies_work_plot_count(ctx->colonies, col);

  out_best->job = -1;
  out_best->tile = -1;
  out_best->score = 0; /* local_12 = 0: DOS elects only a strictly positive score */
  out_best->yield = 0;

  /* DOS-LITERAL FUN_15eb_28c8 raw 12987-12996: the plot walk is over the
   * DS:0xc8/0xde delta tables (N,E,S,W,NW,NE,SE,SW), and since the election at
   * raw 13126 is strictly greater the earliest table index wins ties — so the
   * port must visit its own slots in that DOS order (bugs.md #584). */
  for (int step = 0; step < ring_28c8; ++step) {
    const int ti = colonies_field_scan_order(step);
    if (ti < 0) {
      continue;
    }
    /* raw 12993: `*(char *)(iVar5 + iVar12 * 5 + -0x7210) == '\0'` — the
     * FUN_15eb_23f2 blocked bitmask cached in DS:0x8df0 must be 0 for both
     * human and AI colonies (bugs.md #579). The Indian-claim table at
     * -0x7262 below is the separate, human-only test. */
    if (colonies_plot_blocked_mask(&world, col, ti) != 0) {
      continue;
    }
    if (col->tiles[ti] >= 0 && col->tiles[ti] != colonist_slot) {
      continue; /* worked by a different colonist already */
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = col->x + dx;
    const int ty = col->y + dy;
    /* DS:0x8d9e (-0x7262), FUN_15eb_26e4's 5x5 Indian-claim table. A HUMAN
     * colony skips a claimed plot outright (raw 12993-12995); an AI colony
     * scores it and pays the alarm term below. */
    int claim_village = -1;
    int claim_tribe = -1;
    if (col1) {
      claim_village = colonies_indian_claim_tribe_from_w(
        &world, col->nation_id, col->x, col->y, tx, ty
      );
      if (claim_village >= 0 && col1->tribe) {
        claim_tribe = (int)col1->tribe[claim_village].nation_id - 4;
      }
    }
    if (env.human && claim_village >= 0) {
      continue;
    }
    const int terr = map_dos_terr_class_at(ctx->map, tx, ty);
    /* DOS reads local_32 (the terrain class) in both branches although the
     * decompiler only shows it assigned inside the emergency one — a
     * register-reuse artefact; it is the same FUN_13e4_003a(tile) call. */
    const bool forest = terr > 7 && terr < 0x18;
    const bool lumber_idle =
      col->stock[COLONIZE_CARGO_LUMBER] < 0xb && env.lumber_gross == 0;
    for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT; ++job) {
      if (restrict_job >= 0 && job != restrict_job) {
        continue;
      }
      /* DOS-LITERAL FUN_15eb_28c8 raw 13000: `FUN_15eb_18ec(x,y,&local_24,0)`.
       * 18ec writes the *job* index into local_24 and only remaps a water job
       * to food when param_4 != 0 (raw 11983), which this call site passes as
       * 0 — so for the Fisherman (job 8) local_24 stays 8 and every later use
       * (warehouse clamp `colony+0x9a+8*2`, the DS -0x71ce / -0x71a6 rows, the
       * `local_24 != 5` lumber test) reads cargo slot 8 = HORSES. A DOS quirk,
       * kept verbatim (bugs.md #582). Jobs 0..7 map to cargo 0..7 identically. */
      const int cargo = job;
      int yld = profession >= 0
                  ? colony_yield_for_worker(
                      ctx->map, tx, ty, job, profession, has_docks, sol_b_field,
                      col->colony_flags, has_hudson
                    )
                  : colony_yield_for_tile_in_colony(
                      ctx->colonies, col, ctx->map, tx, ty, job
                    );
      if (yld <= 0) {
        continue; /* score would be 0, which never beats local_12 */
      }
      const int raw_yield = yld;
      if (restrict_job < 0 && cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) {
        /* raw 13004-13011: clamp to the warehouse room left for this good
         * (capacity − colony+0x9a+cargo*2), floor 1. Not AI-gated. */
        int room = env.capacity - col->stock[cargo];
        if (room < 1) {
          room = 1;
        }
        if (yld > room) {
          yld = room;
        }
      }
      /* raw 13017-13024. */
      int score = yld * 8 + (7 - (dx < 0 ? -dx : dx) - (dy < 0 ? -dy : dy));
      if (restrict_job < 0 && job == current_job && env.human) {
        score <<= 1; /* raw 13025 — human colonies only */
      }
      if (restrict_job < 0) {
        if (env.food_emerg) {
          if (job == COLONIZE_JOB_FARMER || job == COLONIZE_JOB_FISHERMAN) {
            if (job == COLONIZE_JOB_FISHERMAN && score != 0) {
              score += 8;
            }
            score <<= 5;
            if (score != 0) {
              int pen = map_dos_terr_labor_penalty_byte(terr);
              if (forest && lumber_idle) {
                pen += 0x18;
              }
              score -= pen;
              if (score < 1) {
                score = 1;
              }
            }
          }
          if (claim_village >= 0) {
            score >>= 1;
          }
        } else {
          int w4 = 0; /* local_4 */
          if (job == COLONIZE_JOB_FARMER || job == COLONIZE_JOB_FISHERMAN) {
            if (col->population < ring_28c8 * 2 && in_ai_tick) {
              w4 = 4;
            }
            if (env.human && w4 == 0) {
              w4 = 1;
            }
          } else if (cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) {
            /* DS:0x84bc[nation*0x10 + cargo] — the per-nation Europe SELL
             * price byte (euro_price − 1, clamped at 0; europe.c). */
            if (col1) {
              const int p = (int)col1->nation[col->nation_id].trade.euro_price[cargo] - 1;
              w4 = p < 0 ? 0 : p;
            }
            /* DOS-LITERAL FUN_15eb_28c8 raw ~13073-13085: the body is a comma
             * expression — `(local_4 = local_4 + 2, byte[DS:0x5398+0x917c] <=
             * byte[nation+0x917c])`. The flat +2 fires unconditionally once
             * !human && cargo==ORE && pop>7 && turn>0x4f; only the two
             * FUN_15eb_039e chain terms sit behind the wealth-rank compare
             * (bugs.md #889). */
            if (!env.human && cargo == COLONIZE_CARGO_ORE && col->population > 7 &&
                env.turn > 0x4f) {
              w4 += 2;
              if (ctx->euro_power_rank_ok &&
                  ctx->euro_power_rank[ctx->human_nation] <= ctx->euro_power_rank[col->nation_id]) {
                /* FUN_15eb_039e(0x28) / (3): owned tiers of the Blacksmith and
                 * Armory chains — not an RNG draw. */
                w4 += ai_euro_28c8_chain_owned_upto(
                  ctx->colonies, col, COLONIES_CHAIN_BLACKSMITH, 2
                );
                w4 += ai_euro_28c8_chain_owned_upto(
                        ctx->colonies, col, COLONIES_CHAIN_ARMORY, 1
                      ) * 2;
              }
            }
          }
          int m = w4 + 1; /* local_38 */
          const int sc = (cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) ? cargo : 0;
          if (env.shortfall[sc] == 0 && env.unmet[sc] == 0) {
            if (job == COLONIZE_JOB_LUMBERJACK &&
                col->stock[COLONIZE_CARGO_LUMBER] + env.lumber_gross > 1) {
              m = w4;
            }
          } else {
            m = w4 + 2;
            if (env.unmet[sc] != 0) {
              score <<= 1;
            }
          }
          /* FUN_15eb_15c6(DS:0x2b6[job]) raw 11536-11551: 0 with no consumer
           * job, 1 when its @BUILDING (DS:0x2f4) is a chain root, 2 when that
           * row has a predecessor. No colony input. Every consumer row DOS
           * ships (27/24/21/32/39) is a root, so the term is the literal 1
           * here (bugs.md #776). */
          {
            const int chain = ai_euro_28c8_consumer_chain(job);
            if (chain >= 0) {
              m += 1;
            }
          }
          if (claim_tribe >= 0) {
            const int alarm =
              ai_diplo_indian_alarm(col1, claim_tribe + 4, col->nation_id);
            int t = -(alarm - 4);
            const int cont = env.continent;
            if (cont >= 0 && cont < 16 &&
                col1->stuff.unit_value_sum_by_continent[col->nation_id * 0x10 + cont] <
                  col1->stuff.tribe_dwellings_91cc[claim_tribe * 0x10 + cont]) {
              t = (alarm - 4) * -2;
            }
            if (col1->stuff.land_combat_totals[col->nation_id] <
                col1->stuff.tribe_data_9184[claim_tribe]) {
              t = t * 3 >> 1;
            }
            t -= env.mil_count;
            if (t < 0) {
              t = 0;
            }
            m -= t;
          }
          if (m < 0) {
            m = 0;
          }
          score = (m + w4) * score;
          if (cargo != COLONIZE_CARGO_LUMBER && forest && lumber_idle) {
            score -= 10; /* raw 13124-13130 */
          }
        }
      }
      if (score > out_best->score) {
        out_best->score = score;
        out_best->job = job;
        out_best->tile = ti;
        out_best->yield = raw_yield;
      }
    }
  }
  return out_best->job >= 0 ? 1 : 0;
}

/*
 * DOS `FUN_15eb_28c8(slot, job)` as the FUN_5952_035e colony tick calls it:
 * DS:0x35e is 1 for the whole tick, so the food-emergency branch is off.
 */
static int ai_euro_28c8_score_job(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* col,
  int colonist_slot,
  int profession,
  int restrict_job,
  AiEuro28c8JobCandidate* out_best
) {
  return ai_euro_28c8_score_full(
    ctx, col, colonist_slot, profession, restrict_job, 1, out_best
  );
}

/* DOS `FUN_15eb_28c8(slot, −1)` — the unrestricted all-jobs search. */
static int ai_euro_28c8_score(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* col,
  int colonist_slot,
  int profession,
  AiEuro28c8JobCandidate* out_best
) {
  return ai_euro_28c8_score_job(ctx, col, colonist_slot, profession, -1, out_best);
}

int ai_euro_28c8_colonist_job_score_structural(
  const ColonizeTurnContext* ctx,
  int colony_id,
  int colonist_slot,
  AiEuro28c8JobCandidate* out_best
) {
  if (!ctx || !ctx->colonies || !ctx->map || !out_best) {
    return 0;
  }
  const ColonizeColony* col = colonies_get(ctx->colonies, colony_id);
  if (!col || !col->active || colonist_slot < 0 || colonist_slot >= col->colonist_count) {
    return 0;
  }
  return ai_euro_28c8_score_full(ctx, col, colonist_slot, -1, -1, 0, out_best);
}

/*
 * FUN_15eb_2ea0 (raw 13162-13196) — the plot pass of DOS's generic colony
 * recompute FUN_15eb_3930 (`268e(); 287e(); 2ea0();`), run for HUMAN colonies
 * too (bugs.md #562). Every colonist not standing on a plot whose occupation
 * is a field job (FUN_15eb_0e18 < 9) is put through 28c8; when that finds
 * nothing the colonist becomes a Carpenter (`FUN_15eb_1068(slot, 0xd)`),
 * never a bell-ringer.
 *
 * `colonist_slot` >= 0 restricts the pass to one colonist — the join path
 * (colonies_admit / ORDERS Join Colony), which is the caller bugs.md #562 is
 * about. -1 walks the whole roster.
 *
 * Colonists carrying DOS occupation 0x13 (@JOB row 19, plain "Colonist") are
 * NOT in scope: 0e18 returns 19 for them, which fails the `< 9` gate, and
 * tests/golden/colony_prod01 (a DOS COLONY00->01 capture) proves DOS leaves
 * such a colonist unproductive rather than seating him. The port spells that
 * out at the call sites — colonies_auto_assign_idle keeps its own stale-save
 * sweep for them (colony.c).
 */
void ai_euro_28c8_auto_assign_plots(ColonizeTurnContext* ctx, int colony_id, int colonist_slot) {
  if (!ctx || !ctx->colonies || !ctx->map) {
    return;
  }
  ColonizeColony* col = colonies_get_mut(ctx->colonies, colony_id);
  if (!col || !col->active) {
    return;
  }
  for (int s = 0; s < col->colonist_count; ++s) {
    if (colonist_slot >= 0 && s != colonist_slot) {
      continue;
    }
    ColonizeColonist* c = &col->colonists[s];
    if (!c->active || c->building_type >= 0 || colonies_colonist_tile(col, s) >= 0) {
      continue;
    }
    AiEuro28c8JobCandidate best;
    const int ok =
      ai_euro_28c8_score_full(ctx, col, s, c->profession, -1, 0, &best);
    if (ok && colonies_assign_field(ctx->colonies, colony_id, s, best.tile, best.job)) {
      continue;
    }
    colonies_assign_carpenter_fallback(ctx->colonies, colony_id, s);
  }
}


/*
 * DOS FUN_281f_0c9a → FUN_15eb_0002 (viceroy_unpacked.c 9298-9307): the
 * expert/class test the colony tick uses everywhere it buckets colonists.
 * False for @JOB 0x13 (the "Colonist" row), 0x19 Indentured, 0x1a Criminal,
 * 0x1b Indian Convert and 0x1c Free Colonist; true for every real skill.
 * europe.c has the same predicate for the dock pool (europe_job_is_expert) —
 * kept separate rather than cross-included so the two files stay independent.
 */
bool ai_euro_5952_job_is_expert(int job) {
  /* DOS reads a profession byte, so -1 ("no profession set") never reaches it;
   * the port's sentinel must not fall through as an expert (bugs.md #600). */
  return job >= 0 && job != UNITS_JOB_COLONIST && job != COLONIZE_PROF_INDENTURED &&
         job != COLONIZE_PROF_CRIMINAL && job != COLONIZE_PROF_CONVERT &&
         job != COLONIZE_PROF_FREE_COLONIST;
}

/* ===== FUN_5952_035e indoor-workplace pass (raw 94784-94860) ===== */

/*
 * DS:0x2f4 (FUN_15eb_0aec) — @JOB -> base @BUILDING index, read straight out
 * of VICEROY.EXE (file offset 121248 + 0x2f4):
 *   9 Distiller 27, 10 Tobacconist 24, 11 Weaver 21, 12 Fur Trader 32,
 *  13 Carpenter 35, 14 Blacksmith 39, 15 Gunsmith 3 (Armory),
 *  16 Preacher 37 (Church), 17 Statesman 9 (Town Hall), 18 Teacher 12.
 * Each of those is the first tier of one of the port's building chains, so
 * the chain id carries the same information without hard-coding a NAMES.TXT
 * row number (colony.h's chain enum is a save-format contract).
 */
static int ai_euro_5952_job_chain(int job) {
  switch (job) {
    case COLONIZE_PROF_DISTILLER: return COLONIES_CHAIN_RUM;
    case COLONIZE_PROF_TOBACCONIST: return COLONIES_CHAIN_TOBACCONIST;
    case COLONIZE_PROF_WEAVER: return COLONIES_CHAIN_WEAVER;
    case COLONIZE_PROF_FUR_TRADER: return COLONIES_CHAIN_FUR;
    case COLONIZE_PROF_CARPENTER: return COLONIES_CHAIN_CARPENTER;
    case COLONIZE_PROF_BLACKSMITH: return COLONIES_CHAIN_BLACKSMITH;
    case COLONIZE_PROF_GUNSMITH: return COLONIES_CHAIN_ARMORY;
    case COLONIZE_PROF_PREACHER: return COLONIES_CHAIN_CHURCH;
    case COLONIZE_PROF_STATESMAN: return COLONIES_CHAIN_TOWN_HALL;
    default: return -1;
  }
}

/*
 * FUN_281f_0ab0 -> FUN_15eb_039e "count owned buildings along parent chain".
 * Returns the count and, through `out_name` / `out_index`, the highest tier
 * this colony actually owns (the workplace a colonist would be put into).
 */
static int ai_euro_5952_chain_owned(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int chain,
  const char** out_name,
  int* out_index
) {
  if (out_name) {
    *out_name = NULL;
  }
  if (out_index) {
    *out_index = -1;
  }
  if (!pool || !col || chain < 0) {
    return 0;
  }
  const char* const* names = colonies_building_chain(chain);
  if (!names) {
    return 0;
  }
  int count = 0;
  for (int i = 0; names[i]; ++i) {
    const int idx = colonies_find_building(pool, names[i]);
    if (idx < 0 || idx >= COLONIZE_BUILDING_TYPES_MAX || !col->has_building[idx]) {
      continue;
    }
    ++count;
    if (out_name) {
      *out_name = names[i];
    }
    if (out_index) {
      *out_index = idx;
    }
  }
  return count;
}

/*
 * FUN_15eb_3454 (via FUN_281f_0bb4), the `aiStack_16a[job]` gate the pass
 * reads: for a job below 0x13 it is non-zero unless the job's base building
 * (FUN_15eb_0aec) exists and the colony does NOT own it — i.e. "this colony
 * has a workplace for this job".
 */
static bool ai_euro_5952_job_available(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int job
) {
  const int chain = ai_euro_5952_job_chain(job);
  if (chain < 0) {
    return false;
  }
  const char* const* names = colonies_building_chain(chain);
  if (!names || !names[0]) {
    return false;
  }
  const int base = colonies_find_building(pool, names[0]);
  if (base < 0 || base >= COLONIZE_BUILDING_TYPES_MAX) {
    return false;
  }
  return col->has_building[base];
}

/*
 * FUN_281f_0cd6 -> FUN_15eb_1d4c: what colonist `slot` would produce in
 * `job`, plus the output ledger slot through `out_cargo` (DOS's out-param;
 * 0xffff for a job outside 9..17). The three special bodies and the shared
 * craft body are already ported one-for-one in colony_production.c — see
 * original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md — so this
 * is only the dispatcher DOS's jump table at 15eb:1f44 performs.
 */
static int ai_euro_5952_producible(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const ColonizeCol1Save* col1,
  int slot,
  int job,
  int* out_cargo
) {
  if (out_cargo) {
    *out_cargo = -1;
  }
  const int chain = ai_euro_5952_job_chain(job);
  const char* name = NULL;
  if (ai_euro_5952_chain_owned(pool, col, chain, &name, NULL) <= 0 || !name) {
    return 0;
  }
  const int prof = col->colonists[slot].profession;
  const int sol_bonus = colony_prod_sol_bonus(col1, col);
  switch (job) {
    case COLONIZE_PROF_CARPENTER: {
      if (out_cargo) {
        *out_cargo = AI_EURO_5952_HAMMERS;
      }
      const bool mill = colonies_has_building_row(pool, col, COLONY_BUILDING_LUMBER_MILL);
      return colony_prod_hammers_worker(name, prof, sol_bonus, mill);
    }
    case COLONIZE_PROF_PREACHER: {
      if (out_cargo) {
        *out_cargo = AI_EURO_5952_CROSSES;
      }
      const bool cathedral = colonies_has_building_row(pool, col, COLONY_BUILDING_CATHEDRAL);
      const bool penn = founding_fathers_nation_has(col1, col->nation_id, FF_WILLIAM_PENN);
      return colony_prod_crosses_worker(name, prof, sol_bonus, cathedral, penn);
    }
    case COLONIZE_PROF_STATESMAN:
      if (out_cargo) {
        *out_cargo = AI_EURO_5952_BELLS;
      }
      return colony_prod_bells_worker(name, prof, sol_bonus);
    default: break;
  }
  const ColonizeCraftRecipe* r = colony_craft_recipe_for_building(name);
  if (!r) {
    return 0;
  }
  if (out_cargo) {
    *out_cargo = r->out_cargo;
  }
  return colony_prod_manufacturing_output(name, prof, r->craft_profession, sol_bonus);
}

/*
 * DS:0x84b4 (asm 5952:1e8f `MOV CL,byte ptr [BX + 0x84b4]`, decomp raw 94817
 * `-0x7b4c`) is read with the SAME `owner*0x10 + cargo` index as the sell
 * price table at DS:0x84bc, i.e. eight bytes ahead of it — the only read of
 * that address in the whole binary, and there is no writer. So for every
 * index >= 8 it is the price table read back shifted by eight; the first
 * eight bytes are whatever global sits immediately before DS:0x84bc, which
 * static analysis cannot name (no other code touches DS:0x84b4..0x84bb). The
 * port reads 0 there, the one modelled unknown in this pass.
 */
static int ai_euro_5952_price_row_minus8(const AiEuro5952Want* w, int index) {
  return index >= 8 ? (int)w->sell_price[index - 8] : 0;
}

/* FUN_124c_000c via FUN_281f_035c — clamp(v, lo, hi). */
static int ai_euro_5952_clamp(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/*
 * The want weight (`uStack_1bc`), asm 5952:1e72 (price arm) and
 * 5952:1fa3-5952:2115 (the three civic arms). DOS-LITERAL FUN_5952_035e
 * raw 94810-94856. No clamps, no guards beyond DOS's own.
 */
COLONIZE_INTERNAL int ai_euro_5952_want_weight(
  const AiEuro5952Want* w, int job, int out_cargo
) {
  if (!w) {
    return 0;
  }
  const int human = (w->human_nation >= 0 && w->human_nation < 4) ? w->human_nation : 0;
  const int owner = (w->owner_nation >= 0 && w->owner_nation < 4) ? w->owner_nation : 0;
  if (out_cargo < 0x10) {
    /* asm 5952:1e72 — BX = owner*0x10 + out. */
    const int index = owner * 0x10 + out_cargo;
    int v = (int)w->sell_price[index];
    if (out_cargo != 0x0f && out_cargo != 0x0e) {
      v -= ai_euro_5952_price_row_minus8(w, index);
    }
    if (out_cargo == 0x0e || out_cargo == 0x0f) {
      v += 4; /* asm 5952:1eaf, unconditional for Tools/Muskets */
      if (w->turn >= 0x32 && w->wealth_rank[human] <= w->wealth_rank[owner]) {
        v *= 2;
      }
    }
    return v;
  }
  int v = 3; /* asm 5952:1fa3 */
  if (job == COLONIZE_PROF_STATESMAN) { /* asm 5952:1fb2 — bells */
    v = w->press_chain_count * 4 + w->tories + 7 + w->capitol_level * 4;
    if (w->tories >= 10) {
      v *= 2;
    }
    if (w->jefferson) { /* FF 15, asm 5952:1fe4 — a ×2 here, NOT the ×1.5 of 2f2b:37cd */
      v <<= 1;
    }
    if (w->year < 0x604) {
      v = 0;
    }
    if (w->year > 0x640) {
      v <<= 1;
    }
    if (w->year > 0x6a4) {
      v <<= 1;
    }
    if (w->independence) {
      v = 0;
    }
    if (w->population <= 3) {
      v >>= 1;
    }
    if (w->population < 6) {
      v >>= 1;
    }
    if (w->wealth_rank[human] < w->wealth_rank[owner]) {
      v >>= 1;
    }
    if (w->wealth_rank[owner] < w->wealth_rank[human]) {
      v <<= 1;
    }
    if (w->nation_flag_bit4) {
      v >>= 1;
    }
    v = ai_euro_5952_clamp(v - w->gross[out_cargo], 1, 100); /* asm 5952:2080 */
  }
  if (job == COLONIZE_PROF_CARPENTER) { /* asm 5952:20a1 — hammers */
    v = -((int)((unsigned)w->gross[out_cargo] / 3u) - 5);
    if (w->wants_construction) {
      v >>= 1;
    }
    if (v < 1) {
      v = 1;
    }
  }
  if (job == COLONIZE_PROF_PREACHER) { /* asm 5952:20e5 — crosses */
    v -= (w->gross[out_cargo] >> 1) + w->turn / 100 - 6;
    if (v < 1) {
      v = 1;
    }
  }
  return v;
}

/* asm 5952:1ed1 tail — `(qty*8 + 5) * weight`. */
COLONIZE_INTERNAL int ai_euro_5952_job_score(
  const AiEuro5952Want* w, int job, int out_cargo, int qty
) {
  return (qty * 8 + 5) * ai_euro_5952_want_weight(w, job, out_cargo);
}

/*
 * FUN_5952_035e indoor pass — env switch. Default ON; AI_5952_INDOOR=0
 * restores the pre-2026-09-17 stand-ins (the leftovers field arm). Documented
 * in docs/debug_env_vars.md.
 */
COLONIZE_INTERNAL int ai_euro_5952_indoor_pass_enabled(void) {
  const char* v = getenv("AI_5952_INDOOR");
  return !(v && v[0] == '0');
}

/*
 * raw 94864-94872 (asm 5952:2139-5952:2174) — the fallback a slot takes when
 * it lost the election to its own field plot AND the plot commit found
 * nothing (DS:0x8dbe == 0): Carpenter, unless the colony owns a Church
 * (FUN_281f_09fc(0x25)), FUN_281f_0d08(5) reports a live lumber surplus
 * (FUN_15eb_0c52: demand[lumber] < stock[lumber] + gross[lumber]) and the
 * colony has fewer than three preachers.
 */
COLONIZE_INTERNAL int ai_euro_5952_fallback_job(
  int has_church, int lumber_surplus, int preacher_count
) {
  if (has_church && lumber_surplus && preacher_count < COLONIZE_BUILDING_MAX_WORKERS) {
    return COLONIZE_PROF_PREACHER;
  }
  return COLONIZE_PROF_CARPENTER;
}

/*
 * DS:0x2b6 (VICEROY.EXE file offset 121248 + 0x2b6) — @JOB -> INPUT cargo,
 * 0xff/-1 for a job that consumes nothing. Read straight off the image:
 *   {-1,9,10,11,12,-1,14,-1,-1, 1,2,3,4, -1, 6, -1,-1,-1,-1, 0}
 * The pass overrides the two -1 entries it cares about itself (Gunsmith <-
 * Tools at raw 94802, Carpenter <- Lumber at raw 94804).
 */
static const signed char k_ai_euro_5952_job_input[20] = {
  -1, 9, 10, 11, 12, -1, 14, -1, -1, 1, 2, 3, 4, -1, 6, -1, -1, -1, -1, 0
};

/*
 * DOS's two 20-word scratch ledgers for the current colony: DS:0x8dc8
 * (-0x7238) gross production and DS:0x8e0a (-0x71f6) demand, both refreshed
 * by FUN_281f_0c04 -> FUN_15eb_1f72 after every assignment. Slots 0..15 are
 * cargo, 16/17/18 hammers/crosses/bells (colony_craft.c's header).
 */
void ai_euro_5952_ledgers(
  const ColonizeWorld* world,
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const ColonizeCol1Save* col1,
  int gross[AI_EURO_5952_LEDGER_SLOTS],
  int demand[AI_EURO_5952_LEDGER_SLOTS]
) {
  memset(gross, 0, sizeof(int) * AI_EURO_5952_LEDGER_SLOTS);
  memset(demand, 0, sizeof(int) * AI_EURO_5952_LEDGER_SLOTS);
  const int sol_bonus = colony_prod_sol_bonus(col1, col);
  const int sol_field = colony_prod_sol_bonus_field(col1, col);
  const bool docks = colony_yield_colony_has_docks(pool, col);
  const bool hudson =
    col1 && founding_fathers_nation_has(col1, col->nation_id, FF_HENRY_HUDSON);

  /* Field production (the town commons' own yields included — DOS's ledger
   * has no separate centre-tile row). */
  {
    ColonizeTownCommonsYield tc;
    memset(&tc, 0, sizeof tc);
    colony_yield_town_commons(
      world->map, col->x, col->y, col->colony_flags,
      col1 ? (int)col1->head.difficulty : 4, &tc
    );
    if (tc.food > 0) {
      gross[COLONIZE_CARGO_FOOD] += tc.food;
    }
    if (tc.secondary_cargo >= 0 && tc.secondary_cargo < COLONIZE_CARGO_COUNT) {
      gross[tc.secondary_cargo] += tc.secondary_amount;
    }
  }
  const int ring_ledger = colonies_work_plot_count(pool, col); /* DS:0x329, #593 */
  for (int ti = 0; ti < ring_ledger; ++ti) {
    const int occ = col->tiles[ti];
    if (occ < 0 || occ >= COLONIZE_COLONY_POP_MAX) {
      continue;
    }
    const ColonizeColonist* c = &col->colonists[occ];
    if (!c->active || c->field_job < 0 || c->field_job >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    gross[c->field_job] += colony_yield_for_worker(
      world->map, col->x + dx, col->y + dy, c->field_job, c->profession, docks,
      sol_field, col->colony_flags, hudson
    );
  }

  /* Craft workers: uncapped worker capacity out, tier-scaled input in — the
   * FUN_15eb_0bd4 ledger rows. */
  for (int s = 0; s < COLONIZE_COLONY_POP_MAX; ++s) {
    const ColonizeColonist* c = &col->colonists[s];
    if (!c->active || c->building_type < 0 ||
        c->building_type >= COLONIZE_BUILDING_TYPES_MAX) {
      continue;
    }
    const char* name = pool->building_types[c->building_type].name;
    const ColonizeCraftRecipe* r = name ? colony_craft_recipe_for_building(name) : NULL;
    if (!r) {
      continue;
    }
    gross[r->out_cargo] +=
      colony_prod_manufacturing_output(name, c->profession, r->craft_profession, sol_bonus);
    demand[r->in_cargo] +=
      colony_prod_manufacturing_input(name, c->profession, r->craft_profession, sol_bonus);
  }

  /* FUN_15eb_1f72's own three rows: hammers (lumber demand), crosses, bells. */
  int lumber_use = 0;
  gross[AI_EURO_5952_HAMMERS] = colony_prod_colony_hammers(pool, col, sol_bonus, &lumber_use);
  demand[COLONIZE_CARGO_LUMBER] += gross[AI_EURO_5952_HAMMERS];
  gross[AI_EURO_5952_CROSSES] = colony_prod_colony_crosses_ff(
    pool, col, col1 && founding_fathers_nation_has(col1, col->nation_id, FF_WILLIAM_PENN),
    sol_bonus
  );
  gross[AI_EURO_5952_BELLS] = colony_prod_colony_bells_ff(
    pool, col, 0, 0,
    col1 ? (col1->player[col->nation_id].control != 0) : true, sol_bonus
  );
  demand[COLONIZE_CARGO_FOOD] = col->population * 2;
}

/*
 * DOS-LITERAL FUN_5952_035e raw 94784-94860 (asm 5952:1ef7-5952:2193) — the
 * indoor-workplace pass, the last placement arm of the AI colony tick. For
 * every colonist the field passes left unplaced it elects one indoor @JOB
 * (9..0x11, Teacher 0x12 excluded by DOS itself) by
 * `(producible*8 + 5) * want_weight`, compares the winner against the tile
 * score the 28c8 probe just produced (DS:0x8dc0), and commits whichever won;
 * a slot that loses to its plot takes the plot, and a slot that loses to
 * nothing falls back to Carpenter — or Preacher, when the colony owns a
 * Church (@BUILDING 0x25), has a live lumber surplus (FUN_15eb_0c52(5)) and
 * fewer than three preachers.
 *
 * This retires two invented stand-ins: the "leftovers" field arm that used
 * to sit here (smell audit #42 — DOS's own leftovers arm at LAB_5952_17a9 is
 * dead code, and the port kept it only because this pass was unported), and
 * the name-matched craft chains of the colony-tick staffing pass for
 * colonists already inside a colony.
 */
static void ai_euro_5952_indoor_pass(
  ColonizeTurnContext* ctx, ColonizeColony* col, bool* placed, int n
) {
  ColonizeColonyPool* pool = ctx->colonies;
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const ColonizeWorld world = world_from_turn_ctx(ctx);
  const int owner = (col->nation_id >= 0 && col->nation_id < 4) ? col->nation_id : 0;

  /* aiStack_e4 — per-@JOB count of this tick's placements. Every colonist was
   * unassigned at raw 94561 and re-placed since, so the live roster IS that
   * count. */
  int placed_count[COLONIZE_PROF_TEACHER + 1];
  memset(placed_count, 0, sizeof placed_count);
  for (int job = COLONIZE_PROF_DISTILLER; job <= COLONIZE_PROF_STATESMAN; ++job) {
    const char* const* names = colonies_building_chain(ai_euro_5952_job_chain(job));
    for (int i = 0; names && names[i]; ++i) {
      const int idx = colonies_find_building(pool, names[i]);
      if (idx >= 0) {
        placed_count[job] += colonies_building_worker_count(col, idx);
      }
    }
  }

  /*
   * DOS's `iStack_78`. It is a function-level local that the loop writes only
   * on a job with an input cargo, while the clamp at raw 94806 reads it
   * unconditionally — so Preacher and Statesman are clamped by whatever the
   * last input job (normally Gunsmith/Tools) left behind, and the very first
   * read in a colony is an uninitialised stack word. That carry-over is
   * DOS-LITERAL and kept; the one thing the port cannot reproduce is the
   * garbage seed, which reads as "no clamp" here (the single modelled unknown
   * of this pass, with DS:0x84b4's first eight bytes).
   */
  int avail = 0x7fff;

  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];

  for (int s = 0; s < n; ++s) {
    if (placed[s] || !col->colonists[s].active) {
      continue;
    }
    /* raw 94785 `FUN_1000_8d5e(slot, 0xfffe)` — mode −2 probe: scores the
     * best plot and leaves it in DS:0x8dc0/0x8dbe without assigning. */
    AiEuro28c8JobCandidate probe;
    memset(&probe, 0, sizeof probe);
    const int probe_ok =
      ai_euro_28c8_score(ctx, col, s, col->colonists[s].profession, &probe);
    const int field_score = probe_ok ? probe.score : 0;

    ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);

    AiEuro5952Want want;
    memset(&want, 0, sizeof want);
    memcpy(want.gross, gross, sizeof want.gross);
    want.owner_nation = owner;
    want.human_nation = (ctx->human_nation >= 0 && ctx->human_nation < 4) ? ctx->human_nation : 0;
    for (int i = 0; i < 4; ++i) {
      want.wealth_rank[i] = ctx->euro_power_rank_ok ? ctx->euro_power_rank[i] : 0;
    }
    want.year = col1 ? (int)col1->head.year : 0;
    want.turn = col1 ? (int)col1->head.turn : 0;
    want.independence = col1 ? ai_king_independence_declared(col1) : 0;
    want.jefferson = col1 && founding_fathers_nation_has(col1, owner, FF_THOMAS_JEFFERSON);
    want.nation_flag_bit4 = col1 ? ((col1->nation[owner].nation_flags & 0x04) != 0) : 0;
    want.capitol_level = (int)col->capitol_level;
    want.population = col->population;
    want.wants_construction =
      (col->build_ai_flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) != 0;
    want.press_chain_count =
      ai_euro_5952_chain_owned(pool, col, COLONIES_CHAIN_PRESS, NULL, NULL);
    /* iStack_7c, raw 294-296: tories = round(pop*(100-SoL%)/100), 0 under WoI. */
    {
      const int sol = colony_prod_sol_percent(col1, col);
      want.tories = want.independence ? 0 : (col->population * (100 - sol) + 50) / 100;
    }
    /* DS:0x84bc — the per-nation SELL price row (every writer stores
     * euro_price − 1 clamped at 0; europe.c's dump-sell note). */
    for (int nn = 0; nn < 4 && col1; ++nn) {
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        const int p = (int)col1->nation[nn].trade.euro_price[c] - 1;
        want.sell_price[nn * 0x10 + c] = (unsigned char)(p < 0 ? 0 : p);
      }
    }

    int best = 0;
    int best_job = COLONIZE_PROF_CARPENTER; /* iStack_ee = 0xd, raw 94793 */
    for (int job = COLONIZE_PROF_DISTILLER; job <= COLONIZE_PROF_TEACHER; ++job) {
      if (job == COLONIZE_PROF_TEACHER) {
        continue; /* raw 94795 `iStack_7a != 0x12` */
      }
      if (!ai_euro_5952_job_available(pool, col, job) ||
          placed_count[job] >= COLONIZE_BUILDING_MAX_WORKERS) {
        continue;
      }
      int in = (int)k_ai_euro_5952_job_input[job];
      if (job == COLONIZE_PROF_GUNSMITH) {
        in = COLONIZE_CARGO_TOOLS; /* raw 94802 */
      }
      if (job == COLONIZE_PROF_CARPENTER) {
        in = COLONIZE_CARGO_LUMBER; /* raw 94804 */
      }
      if (in >= 0) {
        avail = col->stock[in] - demand[in] + gross[in];
        if (avail < 0) {
          continue; /* raw 94805, asm 5952:1f71 */
        }
        if (avail == 0) {
          avail = 1;
        }
      }
      int out = -1;
      int qty = ai_euro_5952_producible(pool, col, col1, s, job, &out);
      if (avail < qty) {
        qty = avail; /* raw 94806-94808, unconditional — see `avail` above */
      }
      if (out < 0) {
        continue; /* unreachable: job_available guarantees a workplace */
      }
      const int score = ai_euro_5952_job_score(&want, job, out, qty);
      if (score > best) { /* asm 5952:1ee7 JLE — strictly greater wins */
        best = score;
        best_job = job;
      }
    }

    int commit_job = -1;
    if (field_score < best) {
      commit_job = best_job; /* raw 94858 `if (*0x8dc0 < best)` */
    } else {
      /* raw 94861 `FUN_1000_8d5e(slot, 0xffff)` — mode −1 commits the plot. */
      if (probe_ok && probe.yield != 0 &&
          colonies_assign_field(ctx->colonies, col->id, s, probe.tile, probe.job)) {
        placed[s] = true;
        continue;
      }
      /* DS:0x8dbe == 0: no plot taken. raw 94864-94872. */
      const int church = colonies_building_row(pool, COLONY_BUILDING_CHURCH); /* @BUILDING 0x25 */
      const bool has_church = church >= 0 && church < COLONIZE_BUILDING_TYPES_MAX &&
                              col->has_building[church];
      /* FUN_15eb_0c52(5): demand[lumber] < stock[lumber] + gross[lumber]. */
      const bool lumber_surplus =
        demand[COLONIZE_CARGO_LUMBER] <
        col->stock[COLONIZE_CARGO_LUMBER] + gross[COLONIZE_CARGO_LUMBER];
      commit_job = ai_euro_5952_fallback_job(
        has_church, lumber_surplus, placed_count[COLONIZE_PROF_PREACHER]
      );
    }

    /* raw 94859/94873 `FUN_1000_8e26(slot, job)` = FUN_15eb_1068 set job. */
    int workplace = -1;
    (void)ai_euro_5952_chain_owned(
      pool, col, ai_euro_5952_job_chain(commit_job), NULL, &workplace
    );
    if (workplace >= 0 && colonies_assign_workplace(pool, col->id, s, workplace)) {
      placed[s] = true;
      if (commit_job >= 0 && commit_job <= COLONIZE_PROF_TEACHER) {
        ++placed_count[commit_job];
      }
    }
  }
}

/* ===== FUN_5952_035e forced-lumberjack pass + lumber buy (raw 94659-94689) ===== */

/*
 * DOS-LITERAL FUN_5952_035e raw 94659-94679 (annotated
 * colony_tick_5952_035e.md:1074-1099) — per-pass slot election of the
 * forced-lumberjack arm.
 *
 * DOS's three passes (`iStack_e6` 0..2) over the still-unplaced slots, on the
 * cached profession array `aiStack_12e`:
 *   0: profession == 5 (an existing Expert Lumberjack)
 *   1: `FUN_1000_8e8a(prof) == 0` = FUN_281f_0c9a, i.e. NOT an expert —
 *      @JOB 0x13 and 0x19..0x1c (Free Colonist, Servant, Criminal, Convert)
 *   2: no test at all — the DOS `else if (iStack_e6 == 1)` chain falls
 *      straight through to the 28c8 call for every remaining pass value.
 * Unlike the carpenter arm there is NO profession rewrite here: DOS never
 * calls 0cae in this arm, so an Indentured Servant (0x19) or Petty Criminal
 * (0x1a) sent to the woods keeps its identity.
 */
COLONIZE_INTERNAL int ai_euro_5952_lumberjack_pick(
  const ColonizeColony* c, const bool* placed, int n, int pass, int start
) {
  if (!c || !placed) {
    return -1;
  }
  for (int s = start < 0 ? 0 : start; s < n && s < COLONIZE_COLONY_POP_MAX; ++s) {
    if (placed[s] || !c->colonists[s].active) {
      continue;
    }
    const int prof = (int)c->colonists[s].profession;
    if (pass == 0 && prof != COLONIZE_PROF_LUMBERJACK) {
      continue; /* raw 94665 */
    }
    if (pass == 1 && ai_euro_5952_job_is_expert(prof)) {
      continue; /* raw 94670 */
    }
    return s;
  }
  return -1;
}

/*
 * DOS-LITERAL FUN_5952_035e raw 94680-94689 (md:1101-1112) — the AI's
 * emergency lumber purchase, the arm that feeds the carpenter arm's gate.
 *
 * Gates, all three: no lumberjack was placed this tick (`iStack_8c == 0`),
 * the colony holds fewer than 2 lumber (`colony+0xa4 < 2`; +0xa4 = +0x9a +
 * 2*5 = the Lumber stock word) and the turn counter `DS:0x538e & 7 == 0`,
 * i.e. one turn in eight.
 *
 * What it does — and the order matters: the colony is credited 100 lumber
 * UNCONDITIONALLY (`*piVar3 = *piVar3 + 100`), and only THEN is the bound
 * nation record's 32-bit purse (`DS:0x84fc + 0x2a` low word, `+0x2c` high
 * word) debited 200, gated on `high >= 0 && (high > 0 || low > 199)` — plain
 * "signed gold >= 200". A broke AI therefore still gets its 100 lumber free;
 * that asymmetry is DOS, not a port shortcut. The debit is a delta, so it
 * goes through europe_nation_gold_add and the read through
 * europe_nation_gold (europe.h single-treasury rule); 0x84fc is the colony
 * owner's record, the nation the tick is bound to.
 */
COLONIZE_INTERNAL void ai_euro_5952_lumber_purchase(
  struct EuropeScreen* eu, struct ColonizeCol1Save* col1, ColonizeColony* col, int turn,
  bool lumber_producer_placed
) {
  if (!col || lumber_producer_placed) {
    return;
  }
  if (col->stock[COLONIZE_CARGO_LUMBER] >= 2 || (turn & 7) != 0) {
    return;
  }
  col->stock[COLONIZE_CARGO_LUMBER] += 100; /* raw 94683-94684 */
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return;
  }
  const long gold = (long)(int32_t)europe_nation_gold(eu, col1, nation);
  if (gold >= 200) { /* raw 94686-94687 */
    europe_nation_gold_add(eu, col1, nation, -200);
  }
}

/*
 * The forced-lumberjack arm itself, raw 94659-94679. Runs inside the same
 * `(colony+0x1d & 0x80) == 0` block as the carpenter arm and immediately
 * before it, gated on `iStack_8c == 0 && colony+0xa4 < 10` — under 10 lumber
 * in stock and nobody chopping.
 *
 * `iStack_8c` is seeded 0 at raw 94257 and has exactly two writers: this arm
 * (raw 94676) and the LAB_5952_17a9 leftovers election at raw 94657 — which
 * the port has already proved dead (its `1 < DS:0x8dbe` threshold can never
 * hold after a mode −2 probe; see the AI_5952_INDOOR=0 stand-in's comment).
 * So on entry it is always 0, and the flag's only live role is the gate on
 * the purchase arm below it, which this function returns.
 *
 * The while-guard is `DS:0x8dd2 == 0` = gross production[Lumber], refreshed
 * by FUN_281f_0c04 after each assignment, so in practice the arm seats
 * exactly ONE lumberjack and every remaining pass finds the guard already
 * false. The 28c8 call is `FUN_1000_8d5e(0x181f, slot, 5)` — the third
 * argument is a real job index, so the search is confined to Lumberjack and
 * only the best TILE for it is elected.
 */
static bool ai_euro_5952_forced_lumberjack(
  ColonizeTurnContext* ctx, ColonizeColony* col, bool* placed, int n
) {
  ColonizeColonyPool* pool = ctx->colonies;
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const ColonizeWorld world = world_from_turn_ctx(ctx);
  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  bool any = false;

  if (col->stock[COLONIZE_CARGO_LUMBER] >= 10) {
    return false; /* raw 94661 */
  }
  ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);

  for (int pass = 0; pass < 3; ++pass) {
    int from = 0;
    while (gross[COLONIZE_CARGO_LUMBER] == 0) {
      const int s = ai_euro_5952_lumberjack_pick(col, placed, n, pass, from);
      if (s < 0) {
        break;
      }
      from = s + 1;
      AiEuro28c8JobCandidate best;
      col->colonists[s].field_job = -1;
      const int ok = ai_euro_28c8_score_job(
        ctx, col, s, (int)col->colonists[s].profession, COLONIZE_JOB_LUMBERJACK, &best
      );
      if (!ok) {
        continue; /* 8d5e != 0 — DOS just walks on to the next slot */
      }
      if (colonies_assign_field(pool, col->id, s, best.tile, best.job)) {
        placed[s] = true;
        any = true; /* iStack_8c = 1, raw 94676 */
        /* FUN_281f_0c04 — the refresh that ends the loop. */
        ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);
      }
    }
  }
  return any;
}

/* ===== FUN_5952_035e carpenter-staffing arm (raw 94690-94740) ===== */

/*
 * DOS-LITERAL FUN_5952_035e raw 94690-94740 (asm 5952:1ac7-5952:1be2,
 * annotated colony_tick_5952_035e.md:1114-1160) — the pass-ordered pick of
 * ONE colonist for the Carpenter's House when the colony has lumber but is
 * making no hammers.
 *
 * `ai_euro_5952_carpenter_pick` is the per-pass slot election plus DOS's own
 * profession rewrite at raw 94716-94718 (`FUN_1000_8e9e(slot, 0x1c)` =
 * FUN_281f_0cae, the same "clear specialty" writer bugs.md #431 uses): an
 * Indentured Servant (0x19) or a Petty Criminal (0x1a) chosen by this arm is
 * turned into a Free Colonist BEFORE it is put to work. The four passes are
 * DOS's `iStack_e6` 0..3 over the still-unplaced slots:
 *   0: profession == 0x0d (an existing Master Carpenter)
 *   1: profession == 0x1c (Free Colonist)
 *   2: profession == 0x19 (Indentured Servant)
 *   3: anyone left
 * `out_prof` hands back the pre-rewrite profession because DOS's own expert
 * test one line later (`FUN_281f_0c9a(aiStack_12e[slot])`) reads the cached
 * array, which the 0cae writes never update.
 */
COLONIZE_INTERNAL int ai_euro_5952_carpenter_pick(
  ColonizeColony* c, const bool* placed, int n, int pass, int start, int* out_prof
) {
  if (out_prof) {
    *out_prof = -1;
  }
  if (!c || !placed) {
    return -1;
  }
  for (int s = start < 0 ? 0 : start; s < n && s < COLONIZE_COLONY_POP_MAX; ++s) {
    if (placed[s] || !c->colonists[s].active) {
      continue;
    }
    const int prof = (int)c->colonists[s].profession;
    if (pass == 0 && prof != COLONIZE_PROF_CARPENTER) {
      continue; /* raw 94696 */
    }
    if (pass == 1 && prof != COLONIZE_PROF_FREE_COLONIST) {
      continue; /* raw 94701 */
    }
    if (pass == 2 && prof != COLONIZE_PROF_INDENTURED) {
      continue; /* raw 94705 */
    }
    /* raw 94716-94718 — 0cae(slot, 0x1c). */
    if (prof == COLONIZE_PROF_CRIMINAL || prof == COLONIZE_PROF_INDENTURED) {
      c->colonists[s].profession = (uint8_t)COLONIZE_PROF_FREE_COLONIST;
    }
    if (out_prof) {
      *out_prof = prof;
    }
    return s;
  }
  return -1;
}

/*
 * The arm itself. DOS gate, raw 94690: `iStack_6a = colony+0xa4 + DS:0x8dd2`
 * = stock[lumber] + gross production[lumber], and the arm runs only when that
 * is > 1. Its loop condition is `DS:0x8de8 == 0` — gross production[hammers],
 * refreshed by FUN_281f_0c04 after every assignment — so in practice it staffs
 * exactly ONE carpenter and then stops, in every remaining pass too.
 *
 * Between the rewrite and the assignment DOS may hand the colonist the
 * Master Carpenter specialty outright (raw 94719-94726): a non-expert
 * (`FUN_281f_0c9a == 0`) in a colony that has NO Master Carpenter yet
 * (`iStack_4e` — BP-0x4c, i.e. `aiStack_68[0x0d]`, the by-profession census
 * the tick builds at raw 94170; the frame slot is one word past Ghidra's
 * `local_4e` label) and a population above 5, on a
 * `FUN_281f_04d4(0, 0x10 - DS:0x53a6) == 0` roll — 1-in-17 at Discoverer,
 * 1-in-13 at Viceroy. `FUN_1000_8e26(slot, 0x0d)` then seats him.
 *
 * DOS's two preceding arms in the same `(+0x1d & 0x80) == 0` block — the
 * forced-lumberjack pass (raw 94659-94679) and the AI's 200-gold /
 * 100-lumber emergency purchase (raw 94680-94689) — are ported above and run
 * first, so this arm sees the bought 100 lumber exactly as DOS does.
 * `iStack_ca` (raw 94733) is a counter nothing in the function ever reads.
 */
static void ai_euro_5952_carpenter_arm(
  ColonizeTurnContext* ctx, ColonizeColony* col, bool* placed, int n
) {
  ColonizeColonyPool* pool = ctx->colonies;
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const ColonizeWorld world = world_from_turn_ctx(ctx);
  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];

  ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);
  if (col->stock[COLONIZE_CARGO_LUMBER] + gross[COLONIZE_CARGO_LUMBER] < 2) {
    return; /* raw 94691 `if (1 < iStack_6a)` */
  }

  /* aiStack_68[0x0d]: Master Carpenters by profession, the tick's snapshot. */
  int master_carpenters = 0;
  for (int s = 0; s < n && s < COLONIZE_COLONY_POP_MAX; ++s) {
    if (col->colonists[s].active &&
        (int)col->colonists[s].profession == COLONIZE_PROF_CARPENTER) {
      ++master_carpenters;
    }
  }
  const int difficulty = col1 ? (int)col1->head.difficulty : 4; /* DS:0x53a6 */

  int workplace = -1;
  (void)ai_euro_5952_chain_owned(pool, col, COLONIES_CHAIN_CARPENTER, NULL, &workplace);
  if (workplace < 0) {
    return;
  }

  for (int pass = 0; pass < 4; ++pass) {
    int from = 0;
    while (gross[AI_EURO_5952_HAMMERS] == 0) {
      int prof = -1;
      const int s = ai_euro_5952_carpenter_pick(col, placed, n, pass, from, &prof);
      if (s < 0) {
        break;
      }
      from = s + 1;
      /* raw 94719-94726 — the free Master Carpenter specialty. */
      if (!ai_euro_5952_job_is_expert(prof) && master_carpenters == 0 &&
          (int)col->population > 5 && ctx->rng &&
          dos_rng_range(ctx->rng, 0, 0x10 - difficulty) == 0) {
        col->colonists[s].profession = (uint8_t)COLONIZE_PROF_CARPENTER;
        ++master_carpenters;
      }
      /* raw 94731 `FUN_1000_8e26(slot, 0x0d)` = set job Carpenter. */
      if (colonies_assign_workplace(pool, col->id, s, workplace)) {
        placed[s] = true;
        /* FUN_281f_0c04 — refresh the ledgers, which is what ends the loop. */
        ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);
      }
    }
  }
}

/*
 * FUN_5952_035e colonist placement block (viceroy_unpacked.c ~94560-94640),
 * the AI-turn caller of 28c8 (via resident stub FUN_281f_0b6e). Per AI
 * colony each turn DOS clears every work plot (`colony+0x70..0x83 = 0xff`)
 * and re-places colonists through 28c8: a food pass first (slots whose
 * previous job was Farmer, or Fisherman on a fishable colony) until the
 * food target holds, then two general passes. Building workers keep their
 * workplaces here — DOS's later statesman/carpenter passes in the same
 * function are the existing expert-workplace heuristics' territory.
 * Food target: Linux population×2 consumption vs town commons + placed food.
 *
 * STOP CONDITION (both halves fixed 2026-09-09):
 *   - It is a `goto LAB_5952_178f` (raw 94596 in the food pass, raw 94614 in
 *     the general passes), and LAB_5952_178f sits OUTSIDE both loops, just
 *     ahead of the leftovers arm at LAB_5952_17a9. So a bad winner ends the
 *     WHOLE placement section, not the current pass: after the food pass
 *     trips it, DOS never runs the two general passes at all. The port used
 *     `break`, which only left the innermost loop.
 *   - The general passes' condition is
 *     `(yield < 3) || (local_84 && yield < 5)` (raw 94613-94614), where
 *     `local_84 = (DS:0x8e32 * 0x10 < colony+0x9a)` is recomputed per slot
 *     (raw 94608): DS:0x8e32 is the colony's food shortfall for the turn
 *     (consumption − field food, 0 when production covers it — turn.c:1095-
 *     1102) and colony+0x9a is the food stock. So a colony sitting on more
 *     than 16 turns' worth of deficit — in practice any well-fed colony —
 *     demands yield >= 5 from a general-pass winner and otherwise stops.
 *     The `< 5` half was unported.
 * STILL NOT PORTED here, deliberately (all DOS-side detail with no Linux
 * counterpart yet): the loop-entry guards `local_7e`/`local_80`/`local_1e`
 * (pop vs ring size, horses stock, gross production vs demand).
 * (`local_84`'s second use at raw 94609-94611, which flips the per-slot
 * eligibility test between FUN_281f_0c9a (expert) and profession 0x1b
 * (Indian Convert), IS ported — see :2626-2637 below, bugs.md #568.)
 */
int ai_euro_20e6_nearest_village(
  const ColonizeTurnContext* ctx, int x, int y, int* out_dist
);

/* ===== FUN_5952_035e construction-project cascade (asm 5952:21d4-5952:2747) ===== */

/*
 * The AI colony tick's build-decision tail, transcribed from
 * `viceroy_overlays.asm:145550-146167` (`OVL15_L0000` = `5952`) with the
 * clean recovery `original_sources_annotated/ai/colony_tick_5952_035e.md`
 * md:1394-1600 as the control-flow cross-check. Every `FUN_5952_0214(id)`
 * argument arrives in **AX**, which is why Ghidra dropped it at all 21 call
 * sites; each one is the `MOV AX,imm` immediately ahead of
 * `CALL FUN_OVL15_L0000__002a6e`, and `002a6e` / `002a73` / `002a78` are the
 * overlay thunks for `FUN_5952_0214` / `FUN_5952_0280` / `FUN_5952_02f4`
 * (`JMPF 0000:0214 / 0280 / 02f4` behind the loader stub at `1000:a7a3+`).
 *
 * This replaces the nineteen-row `ai_euro_prefer_*` stand-in table, which was
 * a name-keyed invention: DOS has exactly one ordered cascade and it ends in
 * five UNIT projects (bugs.md #483).
 *
 * Resolutions this port needed, none of them previously on record:
 *  - `FUN_5952_0214` is **recursive**: when the candidate is not owned and
 *    not buildable it retries the @BUILDING predecessor byte
 *    (`DS:0x8f86 + id*0xc` = `-0x707a`, the chain parent `docs/building_production.md`
 *    already names). Return 1 = "keep scanning", 0 = "stop" (either a project
 *    was set or the chain dead-ended); on 0 it clears colony `+0x1c` bit 0x80.
 *  - `DS:0x864` = six 4-byte craft-chain rows, read straight off the image
 *    (`VICEROY.EXE` offset 121248 + 0x864):
 *      03 0f 0e | 27 0e 06 | 20 0c 04 | 1b 09 01 | 18 0a 02 | 15 0b 03
 *    i.e. {root @BUILDING, @JOB, input @CARGO} = Armory/Gunsmith/Tools,
 *    Blacksmith's House/Blacksmith/Ore, Fur Trader's House/Fur Trader/Furs,
 *    Rum Distiller's House/Distiller/Sugar, Tobacconist's House/Tobacconist/
 *    Tobacco, Weaver's House/Weaver/Cotton. (The 4th byte repeats the @JOB.)
 *  - `DS:0x8ea6` (stride 8, indexed by @JOB) is the **third @JOB column of
 *    NAMES.TXT** — the loader at raw 121044-121053 reads section 0x224e into
 *    {name, plural, col3, col4} records, so `0x8ea6[job] % 4` is that 1..4
 *    class with Teacher/Colonist/Servant/Criminal/Convert (4) folding to 0.
 *    Classes 1/2/3 are exactly the Schoolhouse/College/University tiers the
 *    cascade then asks for (0xc/0xd/0xe).
 *  - `byte[nation*0x10 + 0x84cb]` = `DS:0x84bc` + cargo 0xf, i.e. the
 *    per-nation Europe SELL row for **Muskets** (europe.h's `euro_price − 1`).
 *  - `aiStack_68` is based at **BP-0x66** (`LEA AX,[BP-0x66]` ahead of the
 *    0x32-byte memset at asm 5952:0bb8) and is indexed by the colonist's
 *    PROFESSION with every non-expert folded to 0x13, so the asm's
 *    `[BP-0x48]` / `[BP-0x46]` gates are counts of **Master Gunsmiths** and
 *    **Firebrand Preachers** — which is why they guard the Armory and the
 *    Church. (The clean recovery's `iStack_4a`/`iStack_48` names are shifted
 *    one word against the overlay listing's frame; the asm is authoritative.)
 *  - `FUN_1000_8d90` = `FUN_281f_0ba0` -> `FUN_15eb_0410`, which walks the
 *    SUCCESSOR column `DS:0x8f86` to the deepest tier of a chain, and
 *    `FUN_1000_8ca0` = `FUN_281f_0ab0` -> `FUN_15eb_039e` counts owned tiers
 *    from its argument downward (so `== 3` means a full craft chain).
 *  - `FUN_1000_8d72(job)` / `FUN_1000_8de0(prof)` = `FUN_15eb_1376` /
 *    `FUN_15eb_13ac`, per-colony counts of colonists by JOB and by
 *    PROFESSION.
 *  - `uStack_a2` = `byte[0x329 + tech_tier]` = {0,4,8,12,20}; a European
 *    colony's tier is 2, so the ring is the port's 8 field tiles.
 *
 * DIVERGENCES, deliberate and minimal:
 *  - `iStack_22` (the ring-1 European threat count that gates the Wagon
 *    Train) is produced by `ai_euro_colony_threat_seed_5952`, a different
 *    port pass of the same DOS body; it is stashed per colony in
 *    `ai_euro_s_5952_ring1` (fresh every nation turn, DOS order: the seed runs in
 *    `ai_euro_colony_goals`, before this tail).
 *  - the Wagon Train arm's alarm read is `FUN_1000_84fc(DS:0x8d52, nation)`,
 *    where `DS:0x8d52` is the tribe the tick last bound. The bind is the
 *    nearest-village lookup `FUN_1000_8f74` five lines above it (md:298-302),
 *    so the port reads the alarm of `ai_euro_20e6_nearest_village`'s tribe.
 *  - `FUN_5952_02f4` assigns the unit code with **no** availability gate of
 *    its own (it only clears `+0x1c` bit 0x80 and returns `arg + 0x1f`), so
 *    this path deliberately bypasses `colonies_unit_project_available`: the
 *    Shipyard / Armory requirements are carried by the cascade's own
 *    `try(8)` / `try(3)` steps, and the per-nation Wagon cap in
 *    `FUN_15eb_3650` is the human build MENU's gate, never reached here.
 */

/* iStack_22 — see the header note. */
int ai_euro_s_5952_ring1[COLONIZE_COLONIES_MAX];

/* Test seam: the threat-seed pass is what fills this in production. */
COLONIZE_INTERNAL void ai_euro_5952_set_ring1_threat(int colony_id, int ring1) {
  if (colony_id >= 0 && colony_id < COLONIZE_COLONIES_MAX) {
    ai_euro_s_5952_ring1[colony_id] = ring1;
  }
}

/*
 * `aiStack_68[0x13]` / `aiStack_68[0x15]` — the two by-profession census
 * cells the absorption arm's Soldier/Dragoon case consumes (raw 94242,
 * 94248-94255; md:566-577 builds the array, md:589-604 reads it).
 *
 * VARIABLE RESOLUTION (2026-09-18, disassembly-confirmed). Ghidra names
 * these `iStack_42` / `iStack_3e` and shows no initialiser for either,
 * because they are interior slots of the census array, not locals. The
 * overlay disassembly (`viceroy_overlays.asm`, OVL15_L0000 body of
 * `FUN_5952_035e`) pins all three frame facts:
 *   - Ghidra's `*Stack_NN` labels sit exactly one WORD below the real BP
 *     offsets in this frame (`uStack_24` → `[BP-0x22]`, `iStack_8c` →
 *     `[BP-0x8a]`, `iStack_76` → `[BP-0x74]`, at the four entry zero
 *     stores), so `aiStack_68` is really based at `[BP-0x66]`.
 *   - `LEA AX,[BP-0x66]` + `PUSH 0x32` into `FUN_0000_df7e` clears 50 bytes
 *     = 25 words, i.e. the array is `int[25]` covering @JOB 0..0x18 — the
 *     twin `aiStack_e4` memset is the same 0x32 and ends exactly at the
 *     next declared local, so 25 is the real length, not 13.
 *   - the census store is `INC word ptr [BP+SI-0x66]` with `SI = 2*@JOB`.
 * `iStack_42` = `[BP-0x40]` = index (0x66-0x40)/2 = 0x13, `iStack_3e` =
 * `[BP-0x3c]` = index 0x15 — and both offsets appear verbatim in the gate
 * (`CMP word ptr [BP-0x40],0x0` / `[BP-0x3c],0x0`) and in the decrement
 * pair (`DEC word ptr [BP-0x40]` / `[BP-0x3c]`). So they are the colony's
 * count of non-expert colonists (every non-expert folds to 0x13 at md:574)
 * and of Veteran Soldiers (@JOB 0x15).
 *
 * Stashed per colony for the same reason `ai_euro_s_5952_ring1` is: DOS builds the
 * census inside the tick, immediately before the absorption loop, and the
 * port runs that loop from each arriving unit's act instead. The
 * decrement-on-absorb is carried here so a second absorption in the same
 * turn sees DOS's consumed cell, not a fresh recount.
 */
/*
 * DOS-LITERAL FUN_5952_035e, OVL15 asm 0x22bc-0x22ea: the Docks arm of the
 * build cascade stores AX = 0 into [BP+0xff62] — the tick-local `train_flag`
 * (`local_a0`) — when its FUN_OVL15_002a6e(6) commit returns 0, so ARM 2 (buy
 * an expert, raw 95918 `if (local_a0 != 0)`) cannot fire on the turn Docks is
 * started. The Stockade arm at 0x22ec has no such store. The port splits the
 * one DOS body into ai_euro_5952_build_cascade (the plan step) and
 * ai_euro_5952_specialist_arms (the colony tick), which run in that DOS order,
 * so the store is carried across as this per-colony latch (bugs.md #586).
 */
int ai_euro_s_5952_docks_started[COLONIZE_COLONIES_MAX];

/* Test seam for the latch above. */
COLONIZE_INTERNAL int ai_euro_5952_docks_started(int colony_id) {
  return (colony_id >= 0 && colony_id < COLONIZE_COLONIES_MAX)
           ? ai_euro_s_5952_docks_started[colony_id]
           : 0;
}

int ai_euro_s_5952_census_nonexpert[COLONIZE_COLONIES_MAX]; /* aiStack_68[0x13] */
int ai_euro_s_5952_census_vet_soldier[COLONIZE_COLONIES_MAX]; /* aiStack_68[0x15] */

/* Test seam: the threat-seed pass is what fills these in production. */
COLONIZE_INTERNAL void ai_euro_5952_set_absorb_census(
  int colony_id, int nonexpert, int vet_soldier
) {
  if (colony_id >= 0 && colony_id < COLONIZE_COLONIES_MAX) {
    ai_euro_s_5952_census_nonexpert[colony_id] = nonexpert;
    ai_euro_s_5952_census_vet_soldier[colony_id] = vet_soldier;
  }
}

/*
 * DS:0x864, 6 rows of 4 bytes. Recovered verbatim from VICEROY.EXE at EXE
 * offset 121248 + 0x864 (bugs.md #572):
 *   03 0f 0e 0f | 27 0e 06 0e | 20 0c 04 0c
 *   1b 09 01 09 | 18 0a 02 0a | 15 0b 03 0b
 * (the next bytes are the "GAME" literal, so the table really is 6 rows).
 * Columns: {root @BUILDING, @JOB, input @CARGO, @JOB again} — the fourth
 * byte duplicates the second in every row, and only columns 0/1/2 are read
 * (asm OVL15 0x27cf-0x2837: `[BX+0x864]` -> FUN_1000_8ca0, `[BX+0x866]`*2 ->
 * `[BX+0x8dc8]` gross ledger, `[BX+0x865]`*2 -> the aiStack_68 census).
 */
typedef struct AiEuro5952Craft {
  int root;  /* DOS @BUILDING index */
  int chain; /* COLONIES_CHAIN_* */
  int cargo; /* input @CARGO */
  int job;   /* DS:0x864 column 1 = @JOB */
} AiEuro5952Craft;

static const AiEuro5952Craft k_5952_craft[6] = {
  {0x03, COLONIES_CHAIN_ARMORY, COLONIZE_CARGO_TOOLS, COLONIZE_PROF_GUNSMITH},
  {0x27, COLONIES_CHAIN_BLACKSMITH, COLONIZE_CARGO_ORE, COLONIZE_PROF_BLACKSMITH},
  {0x20, COLONIES_CHAIN_FUR, COLONIZE_CARGO_FURS, COLONIZE_PROF_FUR_TRADER},
  {0x1b, COLONIES_CHAIN_RUM, COLONIZE_CARGO_SUGAR, COLONIZE_PROF_DISTILLER},
  {0x18, COLONIES_CHAIN_TOBACCONIST, COLONIZE_CARGO_TOBACCO, COLONIZE_PROF_TOBACCONIST},
  {0x15, COLONIES_CHAIN_WEAVER, COLONIZE_CARGO_COTTON, COLONIZE_PROF_WEAVER},
};

/*
 * @BUILDING index -> ColonizeBuildingRow, for the ids this cascade names.
 * The predecessor column is `DS:0x8f86 + id*0xc` (`-0x707a`, walked by FUN_15eb_0410); it is the same chain
 * parent colonies_building_chain() already models, so the table carries it
 * directly (−1 = chain root). Town Hall (9..0xb) and Capitol (0x1e/0x1f) are
 * absent because the cascade never asks for them and DOS refuses both.
 * `row` == the DOS @BUILDING index itself (ColonizeBuildingRow shares that
 * numbering 1:1), so this table is really just the predecessor column plus
 * a `has_row` flag for the three unused Town Hall / Capitol slots.
 */
typedef struct AiEuro5952Bld {
  bool has_row;
  int pred;
} AiEuro5952Bld;

static const AiEuro5952Bld k_5952_bld[0x2a] = {
  {true, -1},   {true, 0x00},
  {true, 0x01}, {true, -1},
  {true, 0x03}, {true, 0x04},
  {true, -1},   {true, 0x06},
  {true, 0x07}, {false, -1},
  {false, -1},  {false, -1},
  {true, -1},   {true, 0x0c},
  {true, 0x0d}, {true, -1},
  {true, 0x0f}, {true, -1},
  {true, -1},   {true, -1},
  {true, 0x13}, {true, -1},
  {true, 0x15}, {true, 0x16},
  {true, -1},   {true, 0x18},
  {true, 0x19}, {true, -1},
  {true, 0x1b}, {true, 0x1c},
  {false, -1},  {false, -1},
  {true, -1},   {true, 0x20},
  {true, 0x21}, {true, -1},
  {true, 0x23}, {true, -1},
  {true, 0x25}, {true, -1},
  {true, 0x27}, {true, 0x28},
};

typedef struct AiEuro5952Cascade {
  ColonizeTurnContext* ctx;
  ColonizeColonyPool* pool;
  ColonizeColony* col;
  const ColonizeCol1Save* col1;
  int nation;
  int buildable[COLONIZE_BUILDING_TYPES_MAX];
  int n_buildable;
} AiEuro5952Cascade;

static int ai_euro_5952_bld_index(const ColonizeColonyPool* pool, int dos_id) {
  if (dos_id < 0 || dos_id >= 0x2a || !k_5952_bld[dos_id].has_row) {
    return -1;
  }
  return colonies_building_row(pool, (ColonizeBuildingRow)dos_id);
}

/* FUN_281f_0b8c -> FUN_15eb_3650, via the port's own gate. */
static bool ai_euro_5952_can_build(const AiEuro5952Cascade* s, int idx) {
  for (int i = 0; i < s->n_buildable; ++i) {
    if (s->buildable[i] == idx) {
      return true;
    }
  }
  return false;
}

/* DOS-LITERAL FUN_5952_0214 (asm 5952:0214-5952:027e, raw 93686-93716). */
static int ai_euro_5952_try_build(AiEuro5952Cascade* s, int dos_id) {
  int ret = 1;
  if (dos_id >= 0) {
    const int idx = ai_euro_5952_bld_index(s->pool, dos_id);
    const bool owned =
      idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && s->col->has_building[idx];
    if (!owned) {
      ret = 0;
      if (idx >= 0 && ai_euro_5952_can_build(s, idx)) {
        s->col->building_in_production = idx; /* asm 5952:0248 `+0x94 = id` */
      } else if (ai_euro_5952_try_build(s, k_5952_bld[dos_id].pred) != 0) {
        ret = 1;
      }
    }
  }
  if (ret == 0) {
    s->col->colony_flags =
      (uint8_t)(s->col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
  }
  return ret;
}

/* DOS-LITERAL FUN_5952_02f4 (raw 93749-93755): clear +0x1c bit 0x80, code = row + 0x1f. */
static void ai_euro_5952_set_unit_project(AiEuro5952Cascade* s, int unit_row) {
  s->col->colony_flags =
    (uint8_t)(s->col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
  s->col->building_in_production = unit_row + 0x1f;
  if (getenv("AI_5952_BUILD_TRACE")) {
    const char* uname = NULL;
    colonies_unit_build_info(unit_row + 0x1f, &uname, NULL, NULL);
    fprintf(
      stderr, "[5952] %s -> unit project %s (code %d)\n",
      s->col->name[0] ? s->col->name : "colony", uname ? uname : "?", unit_row + 0x1f
    );
  }
}

/* DOS-LITERAL FUN_5952_0280 (asm 5952:0280-5952:02f2): is this craft chain
 * short of the tier its input supply justifies? */
static int ai_euro_5952_chain_short(
  const AiEuro5952Cascade* s, const int* gross, int chain_row
) {
  const AiEuro5952Craft* r = &k_5952_craft[chain_row];
  const int owned = ai_euro_5952_chain_owned(s->pool, s->col, r->chain, NULL, NULL);
  int want = 0;
  if (gross[r->cargo] >= 3) {
    want = 2;
  }
  if (gross[r->cargo] >= 8) {
    want = 3;
  }
  if (s->col->stock[r->cargo] >= 100) {
    want = 3;
  }
  return owned < want ? 1 : 0;
}

/* FUN_1000_8d90 -> FUN_15eb_0410: deepest tier of the chain rooted at `row`. */
static int ai_euro_5952_chain_top(int chain_row) {
  const int chain = k_5952_craft[chain_row].chain;
  const char* const* names = colonies_building_chain(chain);
  int last = k_5952_craft[chain_row].root;
  int n = 0;
  while (names && names[n]) {
    ++n;
  }
  /* The DOS ids of a chain are consecutive (see k_5952_bld), so the deepest
   * tier is root + (length − 1); Armory's chain is 3/4/5, Weaver's 0x15/16/17. */
  if (n > 0) {
    last = k_5952_craft[chain_row].root + (n - 1);
  }
  return last;
}

/*
 * `DS:0x8ea6[job] % 4` — NAMES.TXT @JOB column 3 (the loader at raw
 * 121044-121053 stores it as the 3rd word of each stride-8 record), read off
 * the shipped COLONIZE/NAMES.TXT @JOB block. Class 4 (Teacher, Colonist,
 * Ind. Servant, Criminal, Convert) folds to 0 under DOS's `% 4`.
 */
static const signed char k_5952_job_class[28] = {
  1, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1,
  2, 2, 3, 3, 0, 0, 1, 2, 1, 2, 3, 0, 0, 0
};

static int ai_euro_5952_job_class(int job) {
  return (job >= 0 && job < 28) ? (int)k_5952_job_class[job] : 0;
}

/*
 * DOS-LITERAL FUN_5952_035e build-decision cascade, asm 5952:21d4-5952:274b.
 * Runs at the tail of the AI colony tick, once per own colony.
 */
void ai_euro_5952_build_cascade(
  ColonizeTurnContext* ctx, ColonizeColony* col
) {
  if (!ctx || !ctx->colonies || !ctx->map || !col || !col->active) {
    return;
  }
  /* Fresh per tick, like DOS's stack-local [BP+0xff62] (bugs.md #586). */
  if (col->id >= 0 && col->id < COLONIZE_COLONIES_MAX) {
    ai_euro_s_5952_docks_started[col->id] = 0;
  }
  if (ctx->colonies->building_type_count <= 0) {
    /* Port-side guard, not a DOS gate: a real game always carries the 42
     * @BUILDING rows, so DOS never runs this cascade with nothing to build.
     * Slim unit fixtures do, and every try() would dead-end straight into the
     * `+0x1d |= 0x80` wants-construction latch. */
    return;
  }
  AiEuro5952Cascade s;
  memset(&s, 0, sizeof s);
  s.ctx = ctx;
  s.pool = ctx->colonies;
  s.col = col;
  s.col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  s.nation = (col->nation_id >= 0 && col->nation_id < 4) ? col->nation_id : 0;

  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof opts);
  opts.map = ctx->map;
  opts.col1 = s.col1;
  if (s.col1) {
    opts.has_adam_smith = founding_fathers_nation_has(s.col1, s.nation, FF_ADAM_SMITH);
    opts.has_peter_stuyvesant =
      founding_fathers_nation_has(s.col1, s.nation, FF_PETER_STUYVESANT);
  }
  s.n_buildable = colonies_list_buildable(
    s.pool, col->id, s.buildable, COLONIZE_BUILDING_TYPES_MAX, &opts
  );

  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  {
    const ColonizeWorld world = world_from_turn_ctx(ctx);
    ai_euro_5952_ledgers(&world, s.pool, col, s.col1, gross, demand);
  }

  const int pop = col->population;
  const int human = (ctx->human_nation >= 0 && ctx->human_nation < 4) ? ctx->human_nation : 0;
  const int turn = s.col1 ? (int)s.col1->head.turn : 0;
  const int year = s.col1 ? (int)s.col1->head.year : 0;
  const int difficulty = s.col1 ? (int)s.col1->head.difficulty : 4;
  const int musket_price =
    s.col1 ? ((int)s.col1->nation[s.nation].trade.euro_price[COLONIZE_CARGO_MUSKETS] - 1) : 0;
  const int ring1 = (col->id >= 0 && col->id < COLONIZE_COLONIES_MAX) ? ai_euro_s_5952_ring1[col->id] : 0;

  /* asm 5952:21d4 — iVar12 = colonists working @JOB 0 (Farmer) + 8 (Fisherman). */
  int food_workers = 0;
  int prof_count[28];
  memset(prof_count, 0, sizeof prof_count);
  for (int i = 0; i < col->colonist_count && i < COLONIZE_COLONY_POP_MAX; ++i) {
    const ColonizeColonist* c = &col->colonists[i];
    if (!c->active) {
      continue;
    }
    if (c->field_job == COLONIZE_JOB_FARMER || c->field_job == COLONIZE_JOB_FISHERMAN) {
      ++food_workers;
    }
    if (c->profession >= 0 && c->profession < 28) {
      ++prof_count[c->profession];
    }
  }

  /* asm 5952:21f6-5952:21fe — the prologue clears the "wants construction"
   * latch and the project slot, so "nothing picked" is a real outcome. */
  col->build_ai_flags =
    (uint8_t)(col->build_ai_flags & (uint8_t)~COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
  col->building_in_production = -1;

  /* asm 5952:222c-5952:2294 — walk the pop slots and then the on-tile units
   * (DS:0x8d72), counting experts and the deepest @JOB class among them. */
  int experts = 0;
  int tier_max = 0;
  for (int i = 0; i < col->colonist_count && i < COLONIZE_COLONY_POP_MAX; ++i) {
    const ColonizeColonist* c = &col->colonists[i];
    if (!c->active || !ai_euro_5952_job_is_expert(c->profession)) {
      continue;
    }
    ++experts;
    const int t = ai_euro_5952_job_class(c->profession) % 4;
    if (t > tier_max) {
      tier_max = t;
    }
  }
  int arty_on_tile = 0; /* iStack_92, asm 5952:2642 */
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || !units_is_on_map(u) || u->x != col->x || u->y != col->y) {
        continue;
      }
      if (u->nation_id != col->nation_id) {
        continue;
      }
      const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
      if (dtype == 0x0b) {
        ++arty_on_tile;
      }
      if (dtype >= 0x0d && dtype <= 0x12) {
        continue; /* ships are not colony slots */
      }
      if (ai_euro_5952_job_is_expert(u->profession)) {
        ++experts;
        const int t = ai_euro_5952_job_class(u->profession) % 4;
        if (t > tier_max) {
          tier_max = t;
        }
      }
    }
  }

  /* asm 5952:2296-5952:22b8 — iStack_a0, the food-pressure latch. */
  int hungry = 0;
  if ((pop >> 1) < food_workers && food_workers > 1) {
    hungry = 1;
  }
  if (col->stock[COLONIZE_CARGO_FOOD] + gross[COLONIZE_CARGO_FOOD] <
      demand[COLONIZE_CARGO_FOOD]) {
    hungry = 1; /* DS:0x8e5a != 0 (unmet[food]) */
  }

  /* uStack_a2 / iStack_142 — the colony's ring (DS:0x329[FUN_15eb_0470()],
   * bugs.md #593) and how much of it is not land (off-map, Ocean 0x19 or High
   * Seas 0x1a), asm 5952:1150 loop. */
  const int ring = colonies_work_plot_count(ctx->colonies, col);
  int ring_nonland = 0;
  for (int d = 0; d < ring; ++d) {
    const int tx = col->x + MAP_DIR8_DX[d];
    const int ty = col->y + MAP_DIR8_DY[d];
    if (tx < 0 || ty < 0 || tx >= (int)ctx->map->width || ty >= (int)ctx->map->height) {
      ++ring_nonland;
      continue;
    }
    if (map_tile_is_water(ctx->map, tx, ty) || map_tile_is_high_seas(ctx->map, tx, ty)) {
      ++ring_nonland;
    }
  }

  /* iStack_2c, asm 5952:2583 — some craft chain is complete (three tiers). */
  int factory = 0;
  for (int i = 5; i >= 0; --i) {
    if (ai_euro_5952_chain_owned(s.pool, col, k_5952_craft[i].chain, NULL, NULL) == 3) {
      factory = 1;
    }
  }

  const int shipyard = ai_euro_5952_bld_index(s.pool, 0x08);
  const bool has_shipyard =
    shipyard >= 0 && shipyard < COLONIZE_BUILDING_TYPES_MAX && col->has_building[shipyard];

  /* --- the cascade proper ------------------------------------------------ */

  if ((ring - ring_nonland) <= pop || (ring_nonland != 0 && hungry != 0)) {
    if (ai_euro_5952_try_build(&s, 0x06) == 0) { /* asm 22da: Docks */
      /* asm 0x22e6 `MOV [BP+0xff62],AX` with AX == 0 — the committed Docks
       * arm clears the tick's train_flag, suppressing ARM 2 this turn. */
      if (col->id >= 0 && col->id < COLONIZE_COLONIES_MAX) {
        ai_euro_s_5952_docks_started[col->id] = 1;
      }
      return;
    }
  }
  if (ai_euro_5952_try_build(&s, 0x00) == 0) { /* 22ec: Stockade */
    return;
  }
  if (col->stock[COLONIZE_CARGO_HORSES] >= 2) {
    if (ai_euro_5952_try_build(&s, 0x11) == 0) { /* 22f9: Stable */
      return;
    }
  }
  if (pop < 4) {
    goto wants;
  }
  {
    const int wl_want = pop / 6; /* iStack_16c */
    if (col->warehouse_level < (unsigned)wl_want && col->warehouse_level == 0) {
      if (ai_euro_5952_try_build(&s, 0x10) == 0) { /* 231f: Warehouse Expansion */
        return;
      }
    }
    if (pop >= 6) {
      int want_custom = (col->ai_flags & 0x03u) != 0;
      if (!want_custom && s.col1) {
        want_custom = ((int)s.col1->stuff.armed_ship_counts[human] - 2) >
                      (int)s.col1->stuff.armed_ship_counts[s.nation];
      }
      if (!want_custom && pop >= 0x0c) {
        want_custom = 1;
      }
      if (want_custom && ai_euro_5952_try_build(&s, 0x12) == 0) { /* 2347: Custom House */
        return;
      }
    }

    /* 2385 — the Wagon Train unit project. */
    if (!(col->colony_flags & COLONIZE_COLONY_FLAG_WAGON_TRAIN) && year < 0x640) {
      const int cont = map_continent_id_at(ctx->map, col->x, col->y);
      const int presence =
        cont >= 0 ? ai_contact_continent_presence_4962(ctx, s.nation, cont) : 0;
      if ((presence & 1) != 0 && ring1 == 0) {
        int vd = 0;
        const int vi = ai_euro_20e6_nearest_village(ctx, col->x, col->y, &vd);
        int alarm = 0;
        if (vi >= 0 && s.col1 && s.col1->tribe) {
          alarm = ai_diplo_indian_alarm(s.col1, (int)s.col1->tribe[vi].nation_id, s.nation);
        }
        if (alarm < 0x32) {
          ai_euro_5952_set_unit_project(&s, 0x0c);
          return;
        }
      }
    }

    if (tier_max >= 1 && (pop + experts) >= 4) {
      if (ai_euro_5952_try_build(&s, 0x0c) == 0) { /* 23d0: Schoolhouse */
        return;
      }
    }
    if (pop < 6) { /* 23f6 */
      col->build_ai_flags =
        (uint8_t)(col->build_ai_flags | COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
    }
    {
      /* 2404: Armory — musket price / turn band, or a Master Gunsmith. */
      int want_armory = 0;
      if ((musket_price + (difficulty >> 1)) >= 4 || turn > 0x50) {
        if (pop >= 6 &&
            (col->stock[COLONIZE_CARGO_TOOLS] >= 0x28 || gross[COLONIZE_CARGO_TOOLS] != 0)) {
          want_armory = 1;
        }
      }
      if (!want_armory && prof_count[COLONIZE_PROF_GUNSMITH] != 0) {
        want_armory = 1;
      }
      if (want_armory && ai_euro_5952_try_build(&s, 0x03) == 0) {
        return;
      }
    }
    if (prof_count[COLONIZE_PROF_PREACHER] != 0) {
      if (ai_euro_5952_try_build(&s, 0x25) == 0) { /* 2453: Church */
        return;
      }
    }
    if (ai_euro_5952_try_build(&s, 0x24) == 0) { /* 2467: Lumber Mill */
      return;
    }
    if (ai_euro_5952_try_build(&s, 0x01) == 0) { /* 2475: Fort */
      return;
    }
    if (musket_price >= 4 && pop >= 4 &&
        (col->stock[COLONIZE_CARGO_ORE] >= 0x28 || gross[COLONIZE_CARGO_ORE] != 0)) {
      if (ai_euro_5952_try_build(&s, 0x28) == 0) { /* 24a9: Blacksmith's Shop */
        return;
      }
    }
    if (col->warehouse_level < (unsigned)wl_want) {
      if (ai_euro_5952_try_build(&s, 0x10) == 0) { /* 24b7 */
        return;
      }
    }
    if (gross[AI_EURO_5952_BELLS] >= 0x18) {
      if (ai_euro_5952_try_build(&s, 0x14) == 0) { /* 24d3: Newspaper */
        return;
      }
    }
    if (gross[AI_EURO_5952_BELLS] >= 4) {
      if (ai_euro_5952_try_build(&s, 0x14) == 0) { /* 24e8 */
        return;
      }
    }
    if (tier_max >= 2 && (pop + experts) >= 0x0a) {
      if (ai_euro_5952_try_build(&s, 0x0d) == 0) { /* 24fd: College */
        return;
      }
    }
    if (pop < 8) {
      goto wants;
    }
    if (tier_max >= 3 && (pop + experts) >= 0x10) {
      if (ai_euro_5952_try_build(&s, 0x0e) == 0) { /* 2530: University */
        return;
      }
    }
    if (ai_euro_5952_try_build(&s, 0x25) == 0) { /* 2552: Church again */
      return;
    }
    if (pop >= 0x0a) {
      if (ai_euro_5952_try_build(&s, 0x02) == 0) { /* 2560: Fortress */
        return;
      }
    }

    /* 25a3 — the naval band: a finished craft chain plus a coastal colony. */
    if (factory != 0 && (col->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) != 0) {
      if (ai_euro_5952_try_build(&s, 0x08) == 0) { /* 25b9: Shipyard */
        return;
      }
      if (has_shipyard && s.col1) {
        const ColonizeCol1Stuff* st = &s.col1->stuff;
        if (((int)st->census_pop_proxy[s.nation] >> 1) + (int)st->colony_counts[s.nation] >=
            (int)st->ship_cargo_totals[s.nation]) {
          ai_euro_5952_set_unit_project(&s, 0x0f); /* 25f1: Galleon */
          return;
        }
        const int frigates = (int)st->unit_type_counts[s.nation][0x11];
        const int armed = (int)st->armed_ship_counts[s.nation];
        if (frigates != 0 || armed == 0) {
          if (armed < 4) {
            ai_euro_5952_set_unit_project(&s, 0x10); /* 260b: Privateer */
            return;
          }
        }
        if (frigates < 1) {
          ai_euro_5952_set_unit_project(&s, 0x11); /* 2626: Frigate */
          return;
        }
      }
    }

    /* 262c */
    if (factory == 0 || (arty_on_tile != 0 && col->labor_shortage != 0)) {
      goto chain_upgrades;
    }
    if (col->stock[COLONIZE_CARGO_TOOLS] != 0) { /* 2670 */
      if (ai_euro_5952_try_build(&s, 0x03) == 0) {
        return;
      }
    }
    ai_euro_5952_set_unit_project(&s, 0x0b); /* 2689: Artillery */
    return;

  chain_upgrades:
    /* 268e — walk the six craft chains, highest row first, and upgrade any
     * whose tier is short of what its input supply justifies. */
    for (int i = 5; i >= 0; --i) {
      if (ai_euro_5952_chain_short(&s, gross, i) != 0) {
        if (ai_euro_5952_try_build(&s, ai_euro_5952_chain_top(i)) == 0) {
          return;
        }
      }
    }
    if (ai_euro_5952_try_build(&s, 0x26) == 0) { /* 26c4: Cathedral */
      return;
    }
    if (has_shipyard && s.col1) {
      const ColonizeCol1Stuff* st = &s.col1->stuff;
      const int armed = (int)st->armed_ship_counts[s.nation];
      const int cap = armed < 8 ? armed : 8; /* asm 26e9 SUB/SBB/AND/ADD = min(armed,8) */
      if ((int)st->unit_type_counts[s.nation][0x0f] < cap) {
        ai_euro_5952_set_unit_project(&s, 0x0f); /* Galleon */
        return;
      }
      if (armed < 8) {
        ai_euro_5952_set_unit_project(&s, 0x11); /* Frigate */
        return;
      }
    }
    if (pop < 0x0a) { /* 270d — the SMALL_AI writer */
      col->colony_flags = (uint8_t)(col->colony_flags | COLONIZE_COLONY_FLAG_SMALL_AI);
    }
    if (arty_on_tile >= 3) { /* 273e */
      col->building_in_production = -1;
      goto wants;
    }
    if (ai_euro_5952_try_build(&s, 0x03) == 0) { /* 2724: Armory */
      return;
    }
    if (gross[COLONIZE_CARGO_MUSKETS] == 0) { /* DS:0x8de6 */
      ai_euro_5952_set_unit_project(&s, 0x0b); /* Artillery */
      return;
    }
    if (ai_euro_5952_try_build(&s, 0x05) == 0) { /* 2737: Arsenal */
      return;
    }
    ai_euro_5952_set_unit_project(&s, 0x0b); /* Artillery */
    return;
  }

wants:
  /* 2747 */
  col->build_ai_flags =
    (uint8_t)(col->build_ai_flags | COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
}

/*
 * DS:0x8ea8, @JOB column 3 ("price"), stride 8 — the word DOS reads as
 * `*(uint *)(row * 8 + -0x7158)`. NAMES.TXT carries it as the fourth field
 * of the @JOB record ("Farmer, Expert Farmers, 1, 1100"); rows that cannot
 * be trained on the Europe dock carry -1, and DOS sign-extends the word with
 * CWD before the 32-bit subtract, so a -1 row ADDS one gold. No catalog
 * (empty section) answers INT_MIN and the caller skips the whole arm.
 */
static int ai_euro_5952_job_price(int row) {
  const char* s = reports_names_field("JOB", row, 3);
  if (!s) {
    return INT_MIN;
  }
  while (*s == ' ' || *s == '\t') {
    ++s;
  }
  if (!*s) {
    return INT_MIN;
  }
  return (int)strtol(s, NULL, 10);
}

/*
 * FUN_1000_8dfe -> FUN_15eb_0e18, colony +0x20+slot = the colonist's
 * OCCUPATION (the job he is doing right now), as opposed to +0x40+slot =
 * his profession. The port stores a field job directly and leaves indoor
 * work implicit in `building_type`, so an indoor worker's occupation is
 * recovered from the chain his workplace belongs to. -1 = idle.
 */
static int ai_euro_5952_occupation(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int slot
) {
  const ColonizeColonist* c = &col->colonists[slot];
  if (c->field_job >= 0) {
    return c->field_job;
  }
  if (c->building_type < 0) {
    return -1;
  }
  for (int job = COLONIZE_PROF_DISTILLER; job <= COLONIZE_PROF_STATESMAN; ++job) {
    const char* const* names = colonies_building_chain(ai_euro_5952_job_chain(job));
    for (int i = 0; names && names[i]; ++i) {
      if (colonies_find_building(pool, names[i]) == c->building_type) {
        return job;
      }
    }
  }
  return -1;
}

/*
 * ARM 2's candidate pick, raw 95926-95944 (asm OVL15 0x2940-0x29c7), split
 * out so a unit test can drive it without a whole turn context. Returns the
 * @JOB to buy (0x1c = "buy nothing") and, through `out_slot`, the colonist
 * DOS picks: the LAST non-expert, non-Convert slot. `uStack_ec` bit 0 tracks
 * "this colony already has an Expert Farmer", bit 1 "… an Expert Fisherman".
 */
COLONIZE_INTERNAL int ai_euro_5952_train_pick(
  const ColonizeColony* col, int n, bool has_docks, int* out_slot
) {
  int last = -1;   /* iStack_30 */
  int have = 0;    /* uStack_ec */
  for (int s = 0; s < n; ++s) {
    if (!col->colonists[s].active) {
      continue;
    }
    const int prof = col->colonists[s].profession;
    if (prof == COLONIZE_PROF_FARMER) {
      have |= 1;
    }
    if (prof == COLONIZE_PROF_FISHERMAN) {
      have |= 2;
    }
    if (!ai_euro_5952_job_is_expert(prof) && prof != COLONIZE_PROF_CONVERT) {
      last = s;
    }
  }
  if (out_slot) {
    *out_slot = last;
  }
  if (last < 0) {
    return COLONIZE_PROF_FREE_COLONIST; /* iStack_ee stays 0x1c */
  }
  if ((have & 2) == 0 && has_docks) {
    return COLONIZE_PROF_FISHERMAN;
  }
  if ((have & 1) == 0) {
    return COLONIZE_PROF_FARMER;
  }
  return COLONIZE_PROF_FREE_COLONIST;
}

/*
 * DOS-LITERAL FUN_5952_035e raw ~95860-95958 — the two specialist arms that
 * close the AI colony tick, both unported until 2026-09-22 (bugs.md #571 /
 * #572). Clean body colony_tick_5952_035e.md:1653-1749, asm
 * viceroy_overlays.asm OVL15 0x274b-0x29ff.
 *
 * Two counts feed them, taken at md:1459-1464 right before the arms:
 *   iVar12     = FUN_1000_8d72(0) + FUN_1000_8d72(8)  // colonists WORKING
 *                                                     // @JOB 0 / @JOB 8
 *   iStack_a   = FUN_1000_8de0(0) + FUN_1000_8de0(8)  // colonists whose
 *                                                     // PROFESSION is 0 / 8
 * and the training flag (asm 0x2296-0x22bc):
 *   if (pop/2 < iVar12 && iVar12 > 1) local_a0 = 1;
 *   if (DS:0x8e5a != 0)               local_a0 = 1;   // colony short of FOOD
 * The audit read `iVar12` as an expert count; the asm is unambiguous that
 * the pair at [BP+0xfe4a] and [BP-0x8] are FUN_1000_8d72 (count by JOB) and
 * FUN_1000_8de0 (count by PROFESSION) respectively.
 *
 * ARM 1 — the schoolhouse/expert election (0x274b). Gated on the colony
 * owning something along @BUILDING chain 0xc (Schoolhouse/College/
 * University) and `((count == 3) + count) * 4 <= improve_timer` (4 / 8 / 16
 * turns). Target:
 *   - pop < 10 and food-experts <= food-workers:
 *       Fishermen-by-profession < water ring tiles ? @JOB 8 : @JOB 0;
 *   - then the DS:0x864 sweep overrides it for any craft whose building
 *     chain is built out (2 tiers, or 1 for the Armory root 0x03), whose
 *     input cargo is actually being produced, and whose @JOB the colony has
 *     nobody of — and the override writes the ROW INDEX, not the row's @JOB:
 *     `MOV AX,[BP+0xff56]; MOV [BP+0xff78],AX` (asm 0x2830). That is a DOS
 *     bug and it is kept: a colony that lacks a Gunsmith elects @JOB 0
 *     (Expert Farmer), one that lacks a Blacksmith elects @JOB 1, and so on
 *     through row 5 -> @JOB 5 (Expert Lumberjack).
 *   The candidate list is every colonist who is NOT an expert, or is an
 *   expert working something other than his specialty (occupation != 0x13),
 *   excluding Converts, capped at 25; one is drawn with FUN_1000_86c4(0,
 *   n-1), his profession is written and improve_timer is zeroed.
 *   Default target for a small colony with little water: @JOB 0.
 *
 * ARM 2 — "train an expert in the colony" (0x290c). Gated on local_a0, on
 * the nation record's tax rate (`*(char *)(DS:0x84fc + 1) <= 0x19`) and on
 * the purse covering @JOB row 0's price (1100). It finds the LAST non-expert
 * non-Convert colonist and makes him an Expert Fisherman when the colony has
 * no Fisherman and owns Docks, else an Expert Farmer when it has no Farmer;
 * then it debits `*(uint *)(slot * 8 + -0x7158)`. That index really is the
 * colonist SLOT, not the profession: asm 0x29d4 is
 * `MOV BX,[BP-0x2e]; SHL BX,0x3; MOV AX,[BX+0x8ea8]; CWD`, and [BP-0x2e] is
 * the same word pushed to FUN_1000_8e9e as the slot argument two
 * instructions earlier. DOS therefore charges the AI @JOB[slot]'s price, not
 * @JOB[target]'s — kept DOS-literal (bugs.md #571).
 *
 * Port deviations, both documented rather than modelled: DOS's candidate
 * walk in ARM 1 runs to `pop + DS:0x8d72`, i.e. past the colonists into the
 * units parked on the colony tile, and DOS's aiStack_68 census is the
 * snapshot taken before the tick's absorption loop. The port walks the
 * colony roster only and recomputes the census here.
 */
COLONIZE_INTERNAL void ai_euro_5952_specialist_arms(
  ColonizeTurnContext* ctx, ColonizeColony* col, int n
) {
  ColonizeColonyPool* pool = ctx->colonies;
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return;
  }

  /* aiStack_68 — per-@JOB census, non-experts folded to 0x13 (md:633-641). */
  int census[0x19];
  memset(census, 0, sizeof census);
  int food_experts = 0; /* iStack_a  = 8de0(0) + 8de0(8) */
  int food_workers = 0; /* iVar12    = 8d72(0) + 8d72(8) */
  for (int s = 0; s < n; ++s) {
    if (!col->colonists[s].active) {
      continue;
    }
    int prof = col->colonists[s].profession;
    if (prof == COLONIZE_PROF_FARMER || prof == COLONIZE_PROF_FISHERMAN) {
      ++food_experts;
    }
    const int job = col->colonists[s].field_job;
    if (job == COLONIZE_JOB_FARMER || job == COLONIZE_JOB_FISHERMAN) {
      ++food_workers;
    }
    if (!ai_euro_5952_job_is_expert(prof)) {
      prof = 0x13;
    }
    if (prof >= 0 && prof < (int)(sizeof census / sizeof census[0])) {
      ++census[prof];
    }
  }

  /* iStack_1a — ring tiles of terrain class 0x19/0x1a (Ocean / High Seas). */
  int water_ring = 0;
  const int ring_water = colonies_work_plot_count(ctx->colonies, col);
  for (int ti = 0; ti < ring_water; ++ti) {
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int wx = col->x + dx;
    const int wy = col->y + dy;
    /* OVL15 0x082c-0x0858 increments [BP-0x18] only when the tile's terrain
     * class is 0x19/0x1a; the off-map path (0x094c) returns without touching
     * it, so an edge colony's missing tiles are NOT water (bugs.md #595).
     * map_tile_is_water() answers true off-map, hence the explicit bounds
     * test — same shape as the build-cascade ring count above. */
    if (wx < 0 || wy < 0 || wx >= (int)ctx->map->width || wy >= (int)ctx->map->height) {
      continue;
    }
    if (map_tile_is_water(ctx->map, wx, wy)) {
      ++water_ring;
    }
  }

  const ColonizeWorld world = world_from_turn_ctx(ctx);
  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);

  /* DS:0x8e5a — slot 0 (FOOD) of the FUN_15eb_1f72 unmet-demand ledger:
   * FUN_15eb_0b52 records a row when stock + production < demand. */
  const bool food_short =
    col->stock[COLONIZE_CARGO_FOOD] + gross[COLONIZE_CARGO_FOOD] <
    demand[COLONIZE_CARGO_FOOD];
  /* asm 0x2296-0x22b9 sets [BP+0xff62] to 1; asm 0x22e6 clears it again when
   * the build cascade's Docks arm commits this turn (bugs.md #586). */
  const bool docks_started =
    col->id >= 0 && col->id < COLONIZE_COLONIES_MAX && ai_euro_s_5952_docks_started[col->id] != 0;
  const bool train_flag =
    !docks_started &&
    (((col->population / 2) < food_workers && food_workers > 1) || food_short);

  /* ---- ARM 1: improve_timer-gated expert election (asm 0x274b) ---- */
  const int school_chain = ai_euro_5952_chain_owned(pool, col, COLONIES_CHAIN_SCHOOL, NULL, NULL);
  if (school_chain != 0 &&
      ((school_chain == 3 ? 1 : 0) + school_chain) * 4 <= (int)col->improve_timer) {
    int target = -1; /* iStack_8a */
    if (col->population < 10 && food_experts <= food_workers) {
      target = census[COLONIZE_PROF_FISHERMAN] < water_ring ? COLONIZE_PROF_FISHERMAN
                                                            : COLONIZE_PROF_FARMER;
    }
    for (int r = 0; r < 6; ++r) {
      const int owned = ai_euro_5952_chain_owned(pool, col, k_5952_craft[r].chain, NULL, NULL);
      const int need = k_5952_craft[r].root == 0x03 ? 1 : 2;
      if (owned >= need && gross[k_5952_craft[r].cargo] != 0 &&
          census[k_5952_craft[r].job] == 0) {
        target = r; /* DOS-LITERAL: the ROW INDEX, not k_5952_craft[r].job */
      }
    }
    int cand[0x19];
    int n_cand = 0;
    for (int s = 0; s < n && n_cand < 0x19; ++s) {
      if (!col->colonists[s].active) {
        continue;
      }
      const int prof = col->colonists[s].profession;
      const int occ = ai_euro_5952_occupation(pool, col, s);
      if (!((!ai_euro_5952_job_is_expert(prof) || (prof != occ && occ != 0x13)) &&
            prof != COLONIZE_PROF_CONVERT)) {
        continue;
      }
      cand[n_cand++] = s;
    }
    if (n_cand != 0) {
      const int pick = cand[dos_rng_range(ctx->rng, 0, n_cand - 1)];
      int elected = target;
      if (elected < 0) {
        elected = ai_euro_5952_occupation(pool, col, pick);
      }
      if (elected >= 0) {
        col->colonists[pick].profession = elected;
      }
      col->improve_timer = 0;
    }
  }

  /* ---- ARM 2: buy an expert for this colony (asm 0x290c) ---- */
  if (!train_flag) {
    return;
  }
  const int tax = col1 ? (int)col1->nation[nation].tax_rate : 0;
  if (tax > 0x19) {
    return;
  }
  const int gate_price = ai_euro_5952_job_price(COLONIZE_PROF_FARMER);
  if (gate_price == INT_MIN) {
    return; /* no @JOB catalog: nothing to price the purchase against */
  }
  if ((long)(int32_t)europe_nation_gold(ctx->europe, ctx->col1, nation) < (long)gate_price) {
    return;
  }
  int last = -1;
  const int trained =
    ai_euro_5952_train_pick(col, n, colony_yield_colony_has_docks(pool, col), &last);
  if (trained == COLONIZE_PROF_FREE_COLONIST) {
    return;
  }
  col->colonists[last].profession = trained;
  /* DOS-LITERAL: the price row is the colonist SLOT (asm 0x29d4). */
  const int charged = ai_euro_5952_job_price(last);
  if (charged != INT_MIN) {
    europe_nation_gold_add(ctx->europe, ctx->col1, nation, -(long)charged);
  }
}

/* FUN_5952_035e tile-improvement arm (raw 94402-94551, bugs.md #612) — the
 * body lives beside the 20e6 terrain/ring tables it shares. */
void ai_euro_5952_improve_best_plot(ColonizeTurnContext* ctx, ColonizeColony* col);
/* FUN_5952_035e raw 94370-94400 — the 20-tool purchase (#640) and the
 * `turn % 7 == 0` inter-colony road-connect arm (#641); both run immediately
 * before the improve arm. */
void ai_euro_5952_tools_supply_and_connect(
  ColonizeTurnContext* ctx, ColonizeColony* col
);

void ai_euro_colony_tick_28c8_reassign(
  ColonizeTurnContext* ctx, int nation_id
) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id == ctx->human_nation) {
    return;
  }
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    ColonizeColony* col = &ctx->colonies->colonies[ci];
    if (!col->active || col->nation_id != nation_id || col->colonist_count <= 0) {
      continue;
    }
    /*
     * FUN_5952_035e raw 94402-94551 runs immediately BEFORE the colonist idle
     * sweep below (bugs.md #612).
     */
    ai_euro_5952_tools_supply_and_connect(ctx, col);
    ai_euro_5952_improve_best_plot(ctx, col);
    int prev_job[COLONIZE_COLONY_POP_MAX];
    bool placed[COLONIZE_COLONY_POP_MAX];
    const int n = col->colonist_count < COLONIZE_COLONY_POP_MAX ? col->colonist_count
                                                                : COLONIZE_COLONY_POP_MAX;
    for (int s = 0; s < n; ++s) {
      const ColonizeColonist* c = &col->colonists[s];
      prev_job[s] = c->field_job;
      /*
       * DOS-LITERAL FUN_5952_035e raw 94551-94564 (clean body
       * colony_tick_5952_035e.md:1030-1043): the tick idles EVERY colonist
       * before the food pass —
       *   for (slot) { prof[slot] = 8e44(slot); placed[slot] = 0;
       *                8c6e(slot,0); 8e26(slot,0x12); }
       *   memset(colony+0x70, 0xff, 0x14);
       * `8e26(slot, 0x12)` is FUN_15eb_1068 with the idle sentinel job and
       * carries no "indoor workers are exempt" test, and the plot memset
       * wipes all 20 tile bytes. Building workers are therefore unseated
       * too, and the indoor pass at raw 94784 (ai_euro_5952_indoor_pass)
       * re-elects an indoor job for everyone the field passes left over —
       * so nobody is lost. The port used to seed
       * `placed[s] = ... || c->building_type >= 0`, which had no DOS
       * counterpart and meant a starving AI colony could never pull a
       * Statesman or a stuck Expert Farmer out of a building onto a food
       * tile (bugs.md #569).
       */
      placed[s] = !c->active;
    }
    for (int s = 0; s < n; ++s) {
      if (placed[s]) {
        continue;
      }
      const int ti = colonies_colonist_tile(col, s);
      if (ti >= 0) {
        colonies_clear_field(ctx->colonies, col->id, ti);
      }
      col->colonists[s].field_job = -1;
      col->colonists[s].building_type = -1; /* raw 94557 `8e26(slot, 0x12)` */
    }

    const bool fishable = colony_yield_colony_has_docks(ctx->colonies, col);
    int food_have = 0;
    {
      ColonizeTownCommonsYield tc;
      colony_yield_town_commons(
        ctx->map, col->x, col->y, col->colony_flags,
        ctx->col1_ok && ctx->col1 ? (int)ctx->col1->head.difficulty : 4, &tc
      );
      food_have = tc.food > 0 ? tc.food : 0;
    }
    const int food_need = col->population * 2;
    /* colony+0x9a — the food stock the DS:0x8e32 comparison is made against. */
    const int food_stock = col->stock[COLONIZE_CARGO_FOOD];
    /* DOS `goto LAB_5952_178f`: ends the whole placement section, not a pass. */
    bool section_done = false;

    /*
     * Pass 1 — the FOOD pass. DOS-LITERAL raw 94592-94594
     * (colony_tick_5952_035e.md:1071-1075):
     *   if (placed[slot] == 0 &&
     *       (prof[slot] == 0 ||
     *        (prof[slot] == 8 && FUN_1000_8bec(0x181f, 6) != 0)) &&
     *       FUN_1000_8d5e(0x181f, slot, prof[slot]) == 0)
     * `aiStack_12e[]` is filled from FUN_1000_8e44 (colony +0x40+slot), i.e.
     * the colonist's PROFESSION, not his previous field job — so only an
     * Expert Farmer (@JOB 0), or an Expert Fisherman (@JOB 8) in a colony
     * with Docks (@BUILDING 6), enters this pass, and the 28c8 call is
     * RESTRICTED to that job (third argument = the profession, not 0xffff),
     * so a food-pass slot can only ever land on a food plot. The port gated
     * on `prev_job[]` and called the unrestricted search (bugs.md #567).
     */
    for (int s = 0; s < n && food_have < food_need; ++s) {
      if (placed[s]) {
        continue;
      }
      const int food_prof = col->colonists[s].profession;
      const bool was_food = food_prof == COLONIZE_PROF_FARMER ||
                            (food_prof == COLONIZE_PROF_FISHERMAN && fishable);
      if (!was_food) {
        continue;
      }
      AiEuro28c8JobCandidate best;
      col->colonists[s].field_job = prev_job[s]; /* sticky ×2 on the old job */
      const int ok = ai_euro_28c8_score_job(ctx, col, s, food_prof, food_prof, &best);
      col->colonists[s].field_job = -1;
      /* DOS-LITERAL raw 94592-94597: the DS:0x8dbe (best yield) stop is INSIDE
       * the `FUN_281f_0b6e(...) == 0` arm. A non-zero 28c8 return (no positive
       * plot) is not a stop at all — DOS just falls through to `local_ac++`
       * and tries the next colonist; only a handled call whose best yield is
       * under 3 ends the whole placement section (bugs.md #585). */
      if (!ok) {
        continue;
      }
      if (best.yield < 3) {
        section_done = true; /* raw 94596 */
        break;
      }
      if (colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job)) {
        placed[s] = true;
        if (best.job == COLONIZE_JOB_FARMER || best.job == COLONIZE_JOB_FISHERMAN) {
          food_have += best.yield;
        }
      }
    }

    /* Pass 2 ×2 — everyone else, best job wins; raw 94613-94614 stop. */
    for (int pass = 0; !section_done && pass < 2; ++pass) {
      for (int s = 0; s < n; ++s) {
        if (placed[s]) {
          continue;
        }
        /*
         * local_84, recomputed per slot (raw 94608). DS:0x8e32 is the
         * shortfall of this turn's food production against consumption, so it
         * shrinks as this section places farmers — hence the running
         * food_have, which pass 1 already maintains and pass 2 now keeps up.
         */
        const int shortfall = food_need > food_have ? food_need - food_have : 0;
        const bool plenty = (shortfall * 0x10) < food_stock;
        /*
         * DOS-LITERAL raw 94600-94604 (md:1088-1092) — the admission test the
         * port was missing entirely (bugs.md #568):
         *   iVar11 = FUN_1000_8e8a(0x181f, prof[slot]);   // is_expert
         *   if (((iVar11 == 0) || (local_84 == 0 && pass != 0)) &&
         *       ((local_84 == 0) || (pass != 0 || prof[slot] == 0x1b)) && ...)
         * With food plentiful (`local_84`), the FIRST sub-pass admits only
         * Indian Converts (@JOB 0x1b) and experts are barred from it
         * altogether; experts enter on the second sub-pass and only when food
         * is NOT plentiful.
         */
        const int prof2 = col->colonists[s].profession;
        const bool expert2 = ai_euro_5952_job_is_expert(prof2);
        if (!((!expert2 || (!plenty && pass != 0)) &&
              (!plenty || (pass != 0 || prof2 == COLONIZE_PROF_CONVERT)))) {
          continue;
        }
        AiEuro28c8JobCandidate best;
        col->colonists[s].field_job = prev_job[s];
        const int ok = ai_euro_28c8_score(ctx, col, s, col->colonists[s].profession, &best);
        col->colonists[s].field_job = -1;
        /* raw 94612-94614, same shape as pass 1: an unhandled 28c8 only skips
         * this colonist (bugs.md #585). */
        if (!ok) {
          continue;
        }
        if (best.yield < 3 || (plenty && best.yield < 5)) {
          section_done = true; /* raw 94614 */
          break;
        }
        if (colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job)) {
          placed[s] = true;
          if (best.job == COLONIZE_JOB_FARMER || best.job == COLONIZE_JOB_FISHERMAN) {
            food_have += best.yield;
          }
        }
      }
    }

    /*
     * DOS raw 94690-94740 — the carpenter-staffing arm, inside the same
     * `(+0x1d & 0x80) == 0` block that seeds local_14 below.
     */
    if ((col->build_ai_flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) == 0) {
      /* raw 94659-94679 then 94680-94689, both ahead of the carpenter arm. */
      const bool lumber_placed = ai_euro_5952_forced_lumberjack(ctx, col, placed, n);
      ai_euro_5952_lumber_purchase(
        ctx->europe, (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL, col,
        /* DS:0x538e — the port's turn counter is ctx->turn_number. */
        ctx->turn_number ? (int)*ctx->turn_number : 0, lumber_placed
      );
      ai_euro_5952_carpenter_arm(ctx, col, placed, n);
    }

    /*
     * DOS raw 94751 (colony_tick_5952_035e.md:1163-1185) — the field-
     * specialist restore pass, the last placement arm before the tick's
     * building passes. It runs whether or not the two passes above tripped
     * their `goto LAB_5952_178f`: that goto lands on LAB_17a9, and this loop
     * is downstream of it.
     *
     *   for slot in 0..pop-1, if unplaced:
     *     if (is_expert(prof) && prof < 9 && prof != 0 && prof != 8) {
     *       if (prof == 5) {                      // Expert Lumberjack
     *         if (local_14 == 0) local_14 = 1;    // the first one is free
     *         else if (!(+0x1b & 0x20) && DS:0x8e64 == 0) continue;
     *       }
     *       if (colony[0x9a + prof*2] <= local_36) assign(slot, job = prof);
     *     }
     *
     * @JOB 0 (Expert Farmer) and 8 (Expert Fisherman) are excluded because
     * the food pass above already had first refusal on them. `is_expert` is
     * FUN_281f_0c9a → FUN_15eb_0002 (viceroy 9298-9307): false for @JOB
     * 0x13 and 0x19..0x1c, true otherwise — so for @JOB 1..7 the profession
     * index doubles as both the field job and the cargo slot.
     *
     * `local_36` is FUN_1000_8f2a → FUN_281f_0d3a → FUN_15eb_0a50, i.e.
     * colonies_warehouse_capacity — one number for all goods, not per-cargo.
     *
     * `local_14` is seeded 1 when the colony does NOT want construction
     * (md:1069, `if ((+0x1d & 0x80) == 0) local_14 = 1;`), so a construction
     * colony admits its first Lumberjack unconditionally and a
     * non-construction one does not. Beyond the first, DOS demands either
     * COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR (+0x1b bit 0x20) or a live
     * lumber shortfall. This is that bit's FIRST Linux reader — colony.h
     * called it write-only, because all four of DOS's readers (raw 94422,
     * 94454, 94499, 94751) sit in unported passes and this is the first one
     * to land.
     *
     * DS:0x8e64 is the unmet-after-stock slot for cargo 5 in the
     * FUN_15eb_1f72 ledger array whose base colony_craft.c already names
     * (DS:0x8e5a + 5*2); FUN_15eb_0b52 records those rows as
     * `stock + production < demand`, which is what is recomputed here —
     * DOS refreshes the array through FUN_1000_8df4 after every assign, so
     * it is this colony's live number, not last turn's.
     */
    {
      const int wh_cap =
        colonies_warehouse_capacity(ctx->colonies, col, COLONIZE_CARGO_LUMBER);
      int lumber_use = 0;
      (void)colony_prod_colony_hammers(ctx->colonies, col, 0, &lumber_use);
      int lumber_prod = 0;
      const int ring_lumber = colonies_work_plot_count(ctx->colonies, col);
      for (int ti = 0; ti < ring_lumber; ++ti) {
        const int occ = col->tiles[ti];
        if (occ < 0 || occ >= n) {
          continue;
        }
        if (col->colonists[occ].field_job != COLONIZE_JOB_LUMBERJACK) {
          continue;
        }
        int dx = 0;
        int dy = 0;
        if (!colonies_field_tile_delta(ti, &dx, &dy)) {
          continue;
        }
        /* DOS recomputes the ledger through FUN_15eb_18ec, i.e. with the
         * seated colonist's own profession, so an Expert Lumberjack's
         * doubling is inside this number. colony_yield_for_tile has no worker
         * context and under-counted it. */
        lumber_prod += colony_yield_for_worker(
          ctx->map, col->x + dx, col->y + dy, COLONIZE_JOB_LUMBERJACK,
          col->colonists[occ].profession,
          colony_yield_colony_has_docks(ctx->colonies, col),
          colony_prod_sol_bonus_field(ctx->col1_ok ? ctx->col1 : NULL, col),
          col->colony_flags,
          ctx->col1_ok && ctx->col1 &&
            founding_fathers_nation_has(ctx->col1, col->nation_id, FF_HENRY_HUDSON)
        );
      }
      const int lumber_unmet =
        lumber_use > col->stock[COLONIZE_CARGO_LUMBER] + lumber_prod ? 1 : 0;
      int lumber_seen =
        (col->build_ai_flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) != 0 ? 0 : 1;
      for (int s = 0; s < n; ++s) {
        if (placed[s]) {
          continue;
        }
        const int prof = col->colonists[s].profession;
        if (!ai_euro_5952_job_is_expert(prof)) {
          continue;
        }
        /* DOS reads a byte, so its `< 9` cannot go negative; the port's
         * profession is an int and an unset one is −1. */
        if (prof < 0 || prof >= COLONIZE_FIELD_JOB_COUNT ||
            prof == COLONIZE_JOB_FARMER || prof == COLONIZE_JOB_FISHERMAN) {
          continue;
        }
        if (prof == COLONIZE_JOB_LUMBERJACK) {
          if (lumber_seen == 0) {
            lumber_seen = 1;
          } else if ((col->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR) == 0 &&
                     lumber_unmet == 0) {
            continue;
          }
        }
        if (col->stock[prof] > wh_cap) {
          continue;
        }
        AiEuro28c8JobCandidate best;
        col->colonists[s].field_job = prev_job[s];
        const int ok = ai_euro_28c8_score_job(ctx, col, s, prof, prof, &best);
        col->colonists[s].field_job = -1;
        if (!ok) {
          continue;
        }
        if (colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job)) {
          placed[s] = true;
        }
      }
    }

    /*
     * DOS's own next arm is the indoor-workplace pass (raw 94784+), ported as
     * ai_euro_5952_indoor_pass. AI_5952_INDOOR=0 falls back to the pre-port
     * "leftovers" field stand-in below (docs/debug_env_vars.md).
     */
    if (ai_euro_5952_indoor_pass_enabled()) {
      ai_euro_5952_indoor_pass(ctx, col, placed, n);
      /* DOS raw ~95860-95958, the tick's last two arms (bugs.md #571/#572). */
      ai_euro_5952_specialist_arms(ctx, col, n);
      continue;
    }

    /*
     * Leftovers — the pre-2026-09-17 stand-in, kept only behind
     * AI_5952_INDOOR=0. DELIBERATE, DOCUMENTED DEVIATION (smell audit #42).
     *
     * The DOS arm is raw 94627-94658 (LAB_5952_17a9), and it is dead code:
     *   - it is a single-winner election, not a per-slot assignment: it probes
     *     every unplaced slot, keeps one best by the DS:0x8dc0 key
     *     (score<<2, +1 expert-gate, +2 prior job == DS:0x8dc2) and commits
     *     only that one (raw 94650);
     *   - every probe is `FUN_1000_8d5e(..., slot, 0xfffe)` — mode −2 — and
     *     28c8's tail (viceroy_unpacked.c 13144-13146, asm 15eb:2e4a)
     *     restores the prior job and stores yield 0 into DS:0x8dbe whenever
     *     `param_2 < -1`. The arm's own threshold is `1 < DS:0x8dbe`
     *     (asm 158989 `CMP [0x8dbe],0x2`), so it can never hold and the
     *     commit is unreachable. DOS's leftovers end the section with NO
     *     field plot (the whole 0x70..0x83 array was memset 0xff at raw
     *     94561) and are picked up by the BUILDING pass at raw 94784+.
     *
     * That building pass is not ported, so deleting this arm outright would
     * leave those colonists idle — strictly worse than DOS, which employs
     * them indoors. The arm therefore stays as the port's stand-in for the
     * building pass, with the two defects the audit named fixed:
     *   - it no longer claims to "keep what they had" while assigning a
     *     freshly scored tile (it does score-and-assign, and says so);
     *   - it now carries the DOS arm's OWN yield floor, `yield >= 2` — the
     *     two real passes above stop at `< 3` (raw 94596 / 94613), this arm
     *     compares against 2. It used to assign on any positive yield, which
     *     force-filled 1-yield tiles.
     * Still not modelled: the single-winner election, the DS:0x8dc0
     * priority key and the raw 94629-94631 population/capacity gate.
     *
     * 2026-09-09: the arm no longer skips slots whose previous job was −1.
     * DOS's own election has no such filter (raw 94635 gates on "unplaced"
     * alone; a matching prior job is worth +2 on the DS:0x8dc0 key, it is not
     * an entry requirement), and the filter only stopped mattering while the
     * `yield < 3` stop was a per-pass `break`. Now that the stop is DOS's
     * whole-section `goto` and carries the `local_84 && yield < 5` half, a
     * colonist who arrived this turn (prev_job −1) routinely reaches this arm
     * — and in DOS he would be picked up by the BUILDING pass at raw 94784+,
     * which is exactly what this stand-in is here to cover. Keeping the
     * filter left freshly admitted colonists idle for good.
     */
    for (int s = 0; s < n; ++s) {
      if (placed[s]) {
        continue;
      }
      AiEuro28c8JobCandidate best;
      col->colonists[s].field_job = prev_job[s];
      const int ok = ai_euro_28c8_score(ctx, col, s, col->colonists[s].profession, &best);
      col->colonists[s].field_job = -1;
      if (ok && best.yield >= 2) {
        (void)colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job);
      }
    }
    ai_euro_5952_specialist_arms(ctx, col, n);
  }
}
