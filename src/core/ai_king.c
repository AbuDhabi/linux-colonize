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
 * Tax-boycott/refuse: head.unknown46[2] stand-in (38fd_5be8 UI OPEN — unpark #2).
 *   Cargo freeze: nation.boycott_bitmap (EuropeScreen has no boycott bits).
 * Merc hired this war: head.unknown46[3] stand-in (2244 dialog OPEN — unpark #2).
 * 160a rename: player[human].country_name → "United Colonies" (cinematic PARKED).
 *   unknown46[4] unused — writable Col1 country_name exists.
 * Congress confirm stand-in: head.unknown46[5] set on declare (2564 UI OPEN — unpark #2).
 * backup_force: DOS 0x53e2… foreign pools — 10f0 stand-in (seeded on declare).
 * Crown nation_id: non-human Euro slot (1 if human==0 else 0).
 */

#define AI_KING_WOI_BYTE 0
#define AI_KING_REF_PRESENT_BYTE 1
#define AI_KING_BOYCOTT_BYTE 2
#define AI_KING_MERC_HIRED_BYTE 3
/* AI_KING_RENAMED_BYTE 4 reserved if country_name unavailable — not used. */
#define AI_KING_CONGRESS_BYTE 5

#define AI_KING_INDEP_COUNTRY "United Colonies"

/* Structural refuse thresholds (exact DOS 38fd_5be8 gates still thin). */
#define AI_KING_BOYCOTT_TAX_MIN 20
#define AI_KING_BOYCOTT_SOL_MIN 30
#define AI_KING_BOYCOTT_BELLS_MIN 80
/* Sugar = cargo index 1 — one frozen Europe cargo while refuse active. */
#define AI_KING_BOYCOTT_CARGO_BIT (1u << 1)
/* Thin 2244 Continental merc aid (player hire dialog OPEN — unpark #2). */
#define AI_KING_MERC_COST 300
#define AI_KING_MERC_SOL_MIN 50

static int ai_king_crown_nation(int human_nation) {
  return (human_nation == 0) ? 1 : 0;
}

/* Crown-hostile Euro slot for 10f0 landings (not human, not crown). */
static int ai_king_intervention_nation(int human_nation) {
  const int crown = ai_king_crown_nation(human_nation);
  for (int n = 0; n < 4; ++n) {
    if (n != human_nation && n != crown) {
      return n;
    }
  }
  return crown;
}

static int ai_king_force_total(const uint16_t force[4]) {
  if (!force) {
    return 0;
  }
  return (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];
}

/*
 * Spawn one land unit near (hx,hy) for nation_id — shared by 06a6 / 10f0.
 * Returns unit id or -1.
 */
static int ai_king_spawn_landing(ColonizeTurnContext* ctx, int nation_id, int hx, int hy,
                                 const char* type_name, const char* alt_name) {
  if (!ctx || !ctx->units || nation_id < 0) {
    return -1;
  }
  int ty = units_find_type(ctx->units, type_name);
  if (ty < 0 && alt_name) {
    ty = units_find_type(ctx->units, alt_name);
  }
  if (ty < 0) {
    return -1;
  }
  const int uid = units_spawn_allow_stack(ctx->units, ty, hx, hy + 1);
  if (uid < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (u) {
    u->nation_id = nation_id;
    u->orders = UNITS_ORDER_AI_MOVE;
    u->goto_x = hx;
    u->goto_y = hy;
  }
  return uid;
}

static void ai_king_set_ref_present(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  col1->head.unknown46[AI_KING_REF_PRESENT_BYTE] = on ? 1 : 0;
}

static int ai_king_boycott_active(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  return col1->head.unknown46[AI_KING_BOYCOTT_BYTE] != 0;
}

static void ai_king_set_boycott(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  col1->head.unknown46[AI_KING_BOYCOTT_BYTE] = on ? 1 : 0;
}

static int ai_king_merc_hired(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  return col1->head.unknown46[AI_KING_MERC_HIRED_BYTE] != 0;
}

static void ai_king_set_merc_hired(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  col1->head.unknown46[AI_KING_MERC_HIRED_BYTE] = on ? 1 : 0;
}

/* Grow REF pools by current tax band (1d42 crumb; no tax_rate change). */
static void ai_king_grow_ref_from_tax(ColonizeCol1Save* col1, uint8_t tax_rate) {
  if (!col1) {
    return;
  }
  col1->head.expeditionary_force[0] += 1; /* regulars */
  if (tax_rate >= 10) {
    col1->head.expeditionary_force[1] += 1; /* dragoons */
  }
  if (tax_rate >= 20) {
    col1->head.expeditionary_force[2] += (tax_rate % 5 == 0) ? 1 : 0; /* MoW */
  }
  if (tax_rate >= 30 && (tax_rate % 10 == 0)) {
    col1->head.expeditionary_force[3] += 1; /* artillery */
  }
  if (col1->head.expeditionary_force[0] > 0) {
    ai_king_set_ref_present(col1, 1);
  }
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
 * Structural boycott/refuse (38fd_5be8 accept-refuse UI PARKED):
 *  when tax_rate >= 20 and (SoL >= 30 or liberty bells high), refuse hike once:
 *  set unknown46[2], freeze one cargo via nation.boycott_bitmap, grow REF
 *  without raising tax. While boycott active, skip further tax hikes.
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

  /* Already refused: no further tax hikes (REF grow-without-hike was once). */
  if (ai_king_boycott_active(ctx->col1)) {
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "Boycott holds; the King cannot raise taxes.");
    }
    /* 38fd_5be8 boycott / refuse UI PARKED */
    return;
  }

  if (nat->tax_rate >= 75) {
    return;
  }

  const int sol = ai_king_sol_percent(ctx, human);
  const int refuse =
      (nat->tax_rate >= AI_KING_BOYCOTT_TAX_MIN) &&
      (sol >= AI_KING_BOYCOTT_SOL_MIN || nat->liberty_bells_total >= AI_KING_BOYCOTT_BELLS_MIN);
  if (refuse) {
    /* Structural refuse: tax stays; REF still grows once; cargo bit frozen. */
    ai_king_set_boycott(ctx->col1, 1);
    nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap | AI_KING_BOYCOTT_CARGO_BIT);
    ai_king_grow_ref_from_tax(ctx->col1, nat->tax_rate);
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size,
               "Colonies refuse the tax hike (boycott). Tax stays at %u%%.", nat->tax_rate);
    }
    /* 38fd_5be8 accept-refuse dialog / dump-goods UI PARKED */
    return;
  }

  nat->tax_rate = (uint8_t)(nat->tax_rate + 1);
  if (ctx->europe) {
    ctx->europe->tax_percent = nat->tax_rate;
  }
  ai_king_grow_ref_from_tax(ctx->col1, nat->tax_rate);
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "The King raises taxes to %u%%.", nat->tax_rate);
  }
  /* 38fd_5be8 boycott / refuse UI PARKED */
}

/*
 * FUN_43f7_2564 gate (SoL≥50) + 1a26 declare body (auto; player confirm UI PARKED).
 * Seeds REF by difficulty; thin backup_force as 10f0 foreign-pool stand-in;
 * withdraws other Euros.
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
  /* Thin 2564 congress-confirm stand-in (player confirm dialog PARKED). */
  ctx->col1->head.unknown46[AI_KING_CONGRESS_BYTE] = 1;
  const int diff = ctx->col1->head.difficulty;
  ctx->col1->head.expeditionary_force[0] = (uint16_t)(8 + diff * 4);
  ctx->col1->head.expeditionary_force[1] = (uint16_t)(4 + diff * 2);
  ctx->col1->head.expeditionary_force[2] = (uint16_t)(2 + diff);
  ctx->col1->head.expeditionary_force[3] = (uint16_t)(2 + diff);
  /* backup_force: DOS 0x53e2… foreign-intervention pools — 10f0 stand-in. */
  ctx->col1->head.backup_force[0] = (uint16_t)(2 + diff);
  ctx->col1->head.backup_force[1] = (uint16_t)(1 + (diff > 0 ? 1 : 0));
  ctx->col1->head.backup_force[2] = (uint16_t)(diff > 1 ? 1 : 0);
  ctx->col1->head.backup_force[3] = 1;
  ai_king_set_ref_present(ctx->col1, 1);
  /* 0218-shaped: fold other Euro AI as withdrawn. */
  for (int n = 0; n < 4; ++n) {
    if (n == human) {
      continue;
    }
    ctx->col1->player[n].control = 2;
  }
  /*
   * Thin 160a independence rename stand-in (letter-animation cinematic PARKED).
   * Writable Col1 player.country_name (and europe.nation_name if present).
   * Same-turn 0982/1528 wave may overwrite ctx->status afterward.
   */
  snprintf(ctx->col1->player[human].country_name,
           sizeof(ctx->col1->player[human].country_name), "%s", AI_KING_INDEP_COUNTRY);
  if (ctx->europe) {
    snprintf(ctx->europe->nation_name, sizeof(ctx->europe->nation_name), "%s",
             AI_KING_INDEP_COUNTRY);
  }
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "The United Colonies declare independence!");
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
 * Spawn one crown land unit on colony tile (0982 wave / MoW cargo unload).
 * Returns 1 on success, else 0.
 */
static int ai_king_spawn_wave_land(ColonizeTurnContext* ctx, int nation_id, int x, int y,
                                   const char* type_name, const char* alt_name) {
  if (!ctx || !ctx->units || nation_id < 0) {
    return 0;
  }
  int lty = units_find_type(ctx->units, type_name);
  if (lty < 0 && alt_name) {
    lty = units_find_type(ctx->units, alt_name);
  }
  if (lty < 0) {
    return 0;
  }
  const int uid = units_spawn_allow_stack(ctx->units, lty, x, y);
  if (uid < 0) {
    return 0;
  }
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (u) {
    u->nation_id = nation_id;
    u->orders = UNITS_ORDER_AI_MOVE;
    u->goto_x = x;
    u->goto_y = y;
  }
  return 1;
}

/*
 * FUN_43f7_0982 (pools>0) / 06a6 (empty): REF wave arms.
 * Thin 1528: status arrival line when 0982 spawns (chrome UI PARKED).
 * Thin MoW cargo: when force[2] drained, unload up to 2 Regulars from force[0]
 * (hold size 2 stand-in). Full multi-unit cargo-hold chrome PARKED.
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
    (void)ai_king_spawn_landing(ctx, crown, hx, hy, "Regular", "Soldier");
    return;
  }

  /* 0982: Man-O-War + land at weakest colony (MoW cargo unload when ship drains). */
  int tx = 0;
  int ty = 0;
  const int cid = ai_king_weakest_port(ctx, ctx->human_nation, &tx, &ty);
  if (cid < 0) {
    return;
  }
  int spawned = 0;
  int mow_spawned = 0;
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
      spawned = 1;
      mow_spawned = 1;
    }
  }

  if (mow_spawned) {
    /*
     * Thin MoW cargo unload near target colony (same crown):
     * hold size 2 stand-in — spawn min(2, force[0]) Regulars; drain force[0].
     * Embark slots / cargo_ids chrome remain PARKED.
     */
    const int unload = (force[0] >= 2) ? 2 : (int)force[0];
    int landed = 0;
    for (int n = 0; n < unload; ++n) {
      if (!ai_king_spawn_wave_land(ctx, crown, tx, ty, "Regular", "Soldier")) {
        break;
      }
      force[0]--;
      spawned = 1;
      landed++;
    }
    /* Guarantee ≥1 land same beat if Regular pool was empty. */
    if (landed == 0) {
      static const char* names[4] = {"Regular", "Dragoon", "Man-O-War", "Artillery"};
      for (int k = 0; k < 4; ++k) {
        if (k == 2 || force[k] == 0) {
          continue;
        }
        const char* alt = (k == 0) ? "Soldier" : ((k == 1) ? "Scout" : NULL);
        if (!ai_king_spawn_wave_land(ctx, crown, tx, ty, names[k], alt)) {
          continue;
        }
        force[k]--;
        spawned = 1;
        break;
      }
    }
  } else {
    /* No MoW this beat: one land pool type (pre-cargo path). */
    static const char* names[4] = {"Regular", "Dragoon", "Man-O-War", "Artillery"};
    for (int k = 0; k < 4; ++k) {
      if (k == 2 || force[k] == 0) {
        continue;
      }
      const char* alt = (k == 0) ? "Soldier" : ((k == 1) ? "Scout" : NULL);
      if (!ai_king_spawn_wave_land(ctx, crown, tx, ty, names[k], alt)) {
        continue;
      }
      force[k]--;
      spawned = 1;
      break; /* one land type per wave beat */
    }
  }
  ai_king_set_ref_present(ctx->col1, 1);
  /* Tax residual grow while at war (1d42 crumb). */
  force[0] += 1;
  /* Thin 1528 announce (arrival chrome / dialog PARKED). */
  if (spawned && ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "The King's Expeditionary Force has arrived!");
  }
}

/*
 * Try one foreign landing from backup pool k; drain on success.
 * MoW pool lands a Regular stand-in (naval cargo chrome PARKED).
 * Returns 1 if a unit spawned, else 0.
 */
static int ai_king_intervene_one(ColonizeTurnContext* ctx, int ally, int hx, int hy,
                                 uint16_t* backup, int k) {
  static const char* names[4] = {"Regular", "Dragoon", "Man-O-War", "Artillery"};
  if (!backup || k < 0 || k > 3 || backup[k] == 0) {
    return 0;
  }
  const char* alt = NULL;
  const char* primary = names[k];
  if (k == 0) {
    alt = "Soldier";
  } else if (k == 1) {
    alt = "Scout";
  } else if (k == 2) {
    /* Naval pool: land a Regular stand-in near port (MoW cargo chrome PARKED). */
    primary = "Regular";
    alt = "Soldier";
  }
  if (ai_king_spawn_landing(ctx, ally, hx, hy, primary, alt) < 0) {
    return 0;
  }
  backup[k]--;
  return 1;
}

/*
 * FUN_43f7_10f0-shaped: foreign-intervention landing when REF empty and
 * backup_force (DOS 0x53e2… stand-in) still has pools. Up to two landings
 * per call (drain two pool entries / types). Prefer Regular + Dragoon when
 * both pools > 0. Crown-hostile nation_id (non-human, non-crown).
 * Deep economy / merc hire / arrival chrome PARKED.
 */
static void ai_king_foreign_intervene(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  uint16_t* backup = ctx->col1->head.backup_force;
  if (ai_king_force_total(ctx->col1->head.expeditionary_force) > 0) {
    return;
  }
  if (ai_king_force_total(backup) <= 0) {
    return;
  }
  int hx = 0;
  int hy = 0;
  if (ai_king_weakest_port(ctx, ctx->human_nation, &hx, &hy) < 0) {
    return;
  }
  const int ally = ai_king_intervention_nation(ctx->human_nation);
  int landings = 0;
  const int max_landings = 2;

  /* Prefer mixing Regular + Dragoon when both foreign pools are live. */
  if (backup[0] > 0 && backup[1] > 0) {
    landings += ai_king_intervene_one(ctx, ally, hx, hy, backup, 0);
    if (landings < max_landings) {
      landings += ai_king_intervene_one(ctx, ally, hx, hy, backup, 1);
    }
  }

  for (int k = 0; k < 4 && landings < max_landings; ++k) {
    if (backup[k] == 0) {
      continue;
    }
    landings += ai_king_intervene_one(ctx, ally, hx, hy, backup, k);
  }
}

/*
 * Thin FUN_43f7_2244 stand-in: auto-accept Continental merc aid once per war
 * when gold>=300 and SoL>50. Spawns Soldier/Dragoon for human near weakest
 * port; player hire dialog PARKED.
 */
static void ai_king_merc_offer(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_merc_hired(ctx->col1)) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  if (nat->gold < AI_KING_MERC_COST) {
    return;
  }
  if (ai_king_sol_percent(ctx, human) <= AI_KING_MERC_SOL_MIN) {
    return;
  }
  int hx = 0;
  int hy = 0;
  if (ai_king_weakest_port(ctx, human, &hx, &hy) < 0) {
    return;
  }
  if (ai_king_spawn_landing(ctx, human, hx, hy, "Soldier", "Dragoon") < 0) {
    return;
  }
  nat->gold -= (uint32_t)AI_KING_MERC_COST;
  if (ctx->europe) {
    ctx->europe->gold = (int)nat->gold;
  }
  ai_king_set_merc_hired(ctx->col1, 1);
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size,
             "Continental mercenaries join the cause (−%d gold).", AI_KING_MERC_COST);
  }
  /* 2244 player hire dialog PARKED */
}

/*
 * FUN_43f7_2022 war act + 1eca promote.
 * Move/combat/capture; Continental promote when SoL>50; 10f0 intervene arm;
 * thin 2244 merc auto-accept.
 */
static void ai_king_war_act(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  /*
   * Rebel arm first: 10f0 while human ports still exist (crown move/capture
   * below may seize the landing pick). In addition to 06a6 in ref_wave.
   */
  ai_king_foreign_intervene(ctx);
  /* Thin 2244: once-per-war Continental merc for human (dialog PARKED). */
  ai_king_merc_offer(ctx);

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

  /*
   * Thin 1eca Continental promote when nation SoL>50:
   *   Soldier* → Continental Army / Cont. Army / Veteran Soldier
   *   Dragoon|Cavalry* → Continental Cavalry / Cont. Cav. / Veteran Dragoon
   * Skip names already Veteran/Continental. Deep colony-SoL/count table PARKED.
   */
  if (ai_king_sol_percent(ctx, ctx->human_nation) > 50) {
    int army = units_find_type(ctx->units, "Continental Army");
    if (army < 0) {
      army = units_find_type(ctx->units, "Cont. Army");
    }
    if (army < 0) {
      army = units_find_type(ctx->units, "Veteran Soldier");
    }
    int cav = units_find_type(ctx->units, "Continental Cavalry");
    if (cav < 0) {
      cav = units_find_type(ctx->units, "Cont. Cav.");
    }
    if (cav < 0) {
      cav = units_find_type(ctx->units, "Veteran Dragoon");
    }
    if (army >= 0 || cav >= 0) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != ctx->human_nation) {
          continue;
        }
        const char* name = units_display_name(ctx->units, u);
        if (!name || strstr(name, "Veteran") || strstr(name, "Continental")) {
          continue;
        }
        if (army >= 0 && strstr(name, "Soldier")) {
          u->type_index = army;
        } else if (cav >= 0 && (strstr(name, "Dragoon") || strstr(name, "Cavalry"))) {
          u->type_index = cav;
        }
      }
    }
  }
}

void ai_king_nation_turn(ColonizeTurnContext* ctx) {
  if (!ctx) {
    return;
  }
  /*
   * FUN_43f7_2424-shaped:
   *   SoL → peacetime (1d42 tax, SoL chrome, 2564/1a26 declare) | wartime (2022 wave+act)
   */
  const int sol = ai_king_sol_percent(ctx, ctx->human_nation);

  if (!ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
    ai_king_tax_event(ctx);
    /*
     * Thin pre-declare SoL chrome (2564 full confirm UI PARKED):
     * SoL 40..49 → restless status line before the auto-declare gate.
     */
    if (sol >= 40 && sol < 50 && ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "Sons of Liberty grow restless (%d%%).", sol);
    }
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
