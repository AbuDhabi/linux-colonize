#include "core/ai_king.h"

#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/units.h"

#include <stdio.h>
#include <string.h>

/*
 * FUN_43f7_* King/REF/independence — partial structural port.
 * Thin map: original_sources_annotated/ai/king_ref.md
 *
 * WoI: head.unknown46[0] stand-in for DOS 0x5382 bit0 (exact Col1 bit PARKED).
 * REF-present: head.unknown46[1] stand-in for 0x5382 bit1.
 * Crown nation_id: non-human Euro slot (1 if human==0 else 0).
 */

#define AI_KING_WOI_BYTE 0
#define AI_KING_REF_PRESENT_BYTE 1

static int ai_king_crown_nation(int human_nation) {
  return (human_nation == 0) ? 1 : 0;
}

static void ai_king_set_ref_present(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  col1->head.unknown46[AI_KING_REF_PRESENT_BYTE] = on ? 1 : 0;
}

int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  /* FUN_43f7_0004: prefer Col1 rebel_dividend/divisor; else liberty bells. */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    uint64_t div_sum = 0;
    uint64_t num_sum = 0;
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != nation_id) {
        continue;
      }
      num_sum += (uint64_t)c->rebel_dividend * (uint64_t)(c->population > 0 ? c->population : 1);
      div_sum += (uint64_t)(c->rebel_divisor > 0 ? c->rebel_divisor : 1) *
                 (uint64_t)(c->population > 0 ? c->population : 1);
    }
    if (div_sum > 0) {
      return (int)((num_sum * 100ull) / div_sum);
    }
  }
  if (ctx->col1_ok && ctx->col1) {
    const ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
    const int bells = (int)nat->liberty_bells_total;
    int sol = bells / 4;
    if (sol > 100) {
      sol = 100;
    }
    return sol;
  }
  return 0;
}

/* Linux WoI stand-in for DOS 0x5382 bit0. */
static int ai_king_independence_declared(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  return col1->head.unknown46[AI_KING_WOI_BYTE] != 0;
}

static void ai_king_set_independence(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  col1->head.unknown46[AI_KING_WOI_BYTE] = on ? 1 : 0;
  if (on) {
    col1->head.event.colony_burning = 1; /* chrome hint */
  }
}

/*
 * FUN_43f7_1d42 checklist:
 *  spring-only; first year / interval by difficulty; cap 75%;
 *  sync europe tax; grow REF pools by tax band.
 * Boycott / 38fd_5be8 accept-refuse UI PARKED.
 * Note: TURN_PROC_FINISH may overwrite ctx->status afterward.
 */
static void ai_king_tax_event(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  const int year = ctx->game_year ? (int)*ctx->game_year : 1492;
  const int diff = ctx->col1->head.difficulty;
  const int first = 1536 - diff;
  const int interval = 22 - diff * 2;
  if (year < first) {
    return;
  }
  if (((year - first) % (interval > 4 ? interval : 4)) != 0) {
    return;
  }
  if (ctx->game_autumn && *ctx->game_autumn != 0) {
    return; /* spring tax audiences */
  }
  if (nat->tax_rate >= 75) {
    return;
  }
  nat->tax_rate = (uint8_t)(nat->tax_rate + 1);
  if (ctx->europe) {
    ctx->europe->tax_percent = nat->tax_rate;
  }
  ctx->col1->head.expeditionary_force[0] += 1; /* regulars */
  if (nat->tax_rate >= 10) {
    ctx->col1->head.expeditionary_force[1] += 1; /* dragoons */
  }
  if (nat->tax_rate >= 20) {
    ctx->col1->head.expeditionary_force[2] += (nat->tax_rate % 5 == 0) ? 1 : 0; /* MoW */
  }
  if (nat->tax_rate >= 30 && (nat->tax_rate % 10 == 0)) {
    ctx->col1->head.expeditionary_force[3] += 1; /* artillery */
  }
  if (ctx->col1->head.expeditionary_force[0] > 0) {
    ai_king_set_ref_present(ctx->col1, 1);
  }
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "The King raises taxes to %u%%.", nat->tax_rate);
  }
  /* 38fd_5be8 boycott / refuse PARKED */
}

/*
 * FUN_43f7_2564 gate (SoL≥50) + 1a26 declare body (auto; player confirm UI PARKED).
 * Seeds REF by difficulty; withdraws other Euros. backup_force / 10f0 PARKED.
 */
static void ai_king_try_declare(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int sol = ai_king_sol_percent(ctx, human);
  if (sol < 50) {
    return;
  }
  const ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  if (nat->liberty_bells_total < 100) {
    return;
  }
  ai_king_set_independence(ctx->col1, 1);
  const int diff = ctx->col1->head.difficulty;
  ctx->col1->head.expeditionary_force[0] = (uint16_t)(8 + diff * 4);
  ctx->col1->head.expeditionary_force[1] = (uint16_t)(4 + diff * 2);
  ctx->col1->head.expeditionary_force[2] = (uint16_t)(2 + diff);
  ctx->col1->head.expeditionary_force[3] = (uint16_t)(2 + diff);
  ai_king_set_ref_present(ctx->col1, 1);
  /* 0218-shaped: fold other Euro AI as withdrawn. */
  for (int n = 0; n < 4; ++n) {
    if (n == human) {
      continue;
    }
    ctx->col1->player[n].control = 2;
  }
  /* 160a rename cinematic PARKED; 10f0 backup_force PARKED */
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "Independence! The REF approaches.");
  }
}

/* FUN_43f7_060a-shaped: weakest garrison (pop × fort). */
static int ai_king_weakest_port(ColonizeTurnContext* ctx, int nation_id, int* out_x, int* out_y) {
  if (!ctx || !ctx->colonies || !out_x || !out_y) {
    return -1;
  }
  int best = -1;
  int best_score = 999999;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    int garrison = c->population;
    if (colonies_has_fortification(ctx->colonies, c)) {
      garrison *= 2;
    }
    if (garrison < best_score) {
      best_score = garrison;
      best = c->id;
      *out_x = c->x;
      *out_y = c->y;
    }
  }
  return best;
}

/*
 * FUN_43f7_0982 (pools>0) / 06a6 (empty): REF wave arms.
 * 1528 announce / multi-unit cargo holds PARKED.
 */
static void ai_king_ref_wave(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->map) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int crown = ai_king_crown_nation(ctx->human_nation);
  uint16_t* force = ctx->col1->head.expeditionary_force;
  const int total = (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];

  if (total <= 0) {
    /* 06a6 irregulars near player colony — crown nation_id, never human. */
    int hx = 0;
    int hy = 0;
    if (ai_king_weakest_port(ctx, ctx->human_nation, &hx, &hy) < 0) {
      return;
    }
    int ty = units_find_type(ctx->units, "Regular");
    if (ty < 0) {
      ty = units_find_type(ctx->units, "Soldier");
    }
    if (ty < 0) {
      return;
    }
    const int uid = units_spawn_allow_stack(ctx->units, ty, hx, hy + 1);
    if (uid >= 0) {
      ColonizeUnit* u = units_get(ctx->units, uid);
      if (u) {
        u->nation_id = crown;
        u->orders = UNITS_ORDER_AI_MOVE;
        u->goto_x = hx;
        u->goto_y = hy;
      }
    }
    return;
  }

  /* 0982: Man-O-War + one land pool type at weakest colony. */
  int tx = 0;
  int ty = 0;
  const int cid = ai_king_weakest_port(ctx, ctx->human_nation, &tx, &ty);
  if (cid < 0) {
    return;
  }
  int ship_ty = units_find_type(ctx->units, "Man-O-War");
  if (ship_ty < 0) {
    ship_ty = units_find_type(ctx->units, "Galleon");
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int sx = tx;
  int sy = ty;
  for (int d = 0; d < 8; ++d) {
    const int nx = tx + dx[d];
    const int ny = ty + dy[d];
    if (map_tile_is_water(ctx->map, nx, ny)) {
      sx = nx;
      sy = ny;
      break;
    }
  }
  if (ship_ty >= 0 && force[2] > 0) {
    const int sid = units_spawn_allow_stack(ctx->units, ship_ty, sx, sy);
    if (sid >= 0) {
      ColonizeUnit* ship = units_get(ctx->units, sid);
      if (ship) {
        ship->nation_id = crown;
        ship->orders = UNITS_ORDER_AI_SAIL;
        ship->goto_x = sx;
        ship->goto_y = sy;
      }
      force[2]--;
    }
  }
  const char* names[4] = {"Regular", "Dragoon", "Man-O-War", "Artillery"};
  for (int k = 0; k < 4; ++k) {
    if (k == 2) {
      continue;
    }
    if (force[k] == 0) {
      continue;
    }
    int lty = units_find_type(ctx->units, names[k]);
    if (lty < 0 && k == 0) {
      lty = units_find_type(ctx->units, "Soldier");
    }
    if (lty < 0 && k == 1) {
      lty = units_find_type(ctx->units, "Scout");
    }
    if (lty < 0) {
      continue;
    }
    const int uid = units_spawn_allow_stack(ctx->units, lty, tx, ty);
    if (uid < 0) {
      break;
    }
    ColonizeUnit* u = units_get(ctx->units, uid);
    if (u) {
      u->nation_id = crown;
      u->orders = UNITS_ORDER_AI_MOVE;
      u->goto_x = tx;
      u->goto_y = ty;
    }
    force[k]--;
    break; /* one land type per wave beat */
  }
  ai_king_set_ref_present(ctx->col1, 1);
  /* Tax residual grow while at war (1d42 crumb). */
  force[0] += 1;
  /* 1528 announce PARKED */
}

/*
 * FUN_43f7_2022 war act + 1eca promote.
 * Move/combat/capture; Continental promote when SoL>50.
 */
static void ai_king_war_act(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int crown = ai_king_crown_nation(ctx->human_nation);
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown) {
      continue;
    }
    if (u->moves_left <= 0) {
      continue;
    }
    int tx = u->goto_x;
    int ty = u->goto_y;
    if (tx < 0 || ty < 0 || tx >= 255 || ty >= 255) {
      if (ai_king_weakest_port(ctx, ctx->human_nation, &tx, &ty) < 0) {
        continue;
      }
      u->goto_x = tx;
      u->goto_y = ty;
    }
    const int sdx = (tx > u->x) - (tx < u->x);
    const int sdy = (ty > u->y) - (ty < u->y);
    const int nx = u->x + sdx;
    const int ny = u->y + sdy;
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe >= 0) {
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (f && f->nation_id == ctx->human_nation) {
        if (units_is_sea(ctx->units, u->id)) {
          units_resolve_naval_combat(ctx->units, u->id, foe, ctx->rng);
        } else if (units_resolve_land_combat(ctx->units, u->id, foe, ctx->rng)) {
          units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng);
        }
        continue;
      }
    }
    units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng);
    if (u->active && ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (cid >= 0) {
        ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
        if (c && c->nation_id == ctx->human_nation) {
          colonies_capture(ctx->colonies, cid, crown);
        }
      }
    }
  }

  /* 1eca: promote soldiers when SoL>50. */
  if (ai_king_sol_percent(ctx, ctx->human_nation) > 50) {
    int vet = units_find_type(ctx->units, "Continental Army");
    if (vet < 0) {
      vet = units_find_type(ctx->units, "Veteran Soldier");
    }
    if (vet >= 0) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != ctx->human_nation) {
          continue;
        }
        const char* name = units_display_name(ctx->units, u);
        if (name && strstr(name, "Soldier") && !strstr(name, "Veteran") &&
            !strstr(name, "Continental")) {
          u->type_index = vet;
        }
      }
    }
  }
  /* 10f0 foreign intervention PARKED */
}

void ai_king_nation_turn(ColonizeTurnContext* ctx) {
  if (!ctx) {
    return;
  }
  /*
   * FUN_43f7_2424-shaped:
   *   SoL → peacetime (1d42 tax, 2564/1a26 declare) | wartime (2022 wave+act)
   */
  (void)ai_king_sol_percent(ctx, ctx->human_nation);

  if (!ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
    ai_king_tax_event(ctx);
    ai_king_try_declare(ctx);
  }

  if (ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
    ai_king_ref_wave(ctx);
    ai_king_war_act(ctx);
  }

  if (ctx->active_turn_nation) {
    *ctx->active_turn_nation = ctx->human_nation;
  }
}
