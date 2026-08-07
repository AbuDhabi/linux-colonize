#include "core/ai_contact.h"

#include "core/ai_diplo.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/units.h"

#include <stdlib.h>
#include <string.h>

static int s_last_raid_kind = AI_RAID_NOTHING;

int ai_contact_last_raid_kind(void) {
  return s_last_raid_kind;
}

static int ai_contact_dist(int x0, int y0, int x1, int y1) {
  const int dx = abs(x0 - x1);
  const int dy = abs(y0 - y1);
  return dx > dy ? dx : dy;
}

/* Isolated from quiet-pulse LCG (seed-100 TURN goldens). */
static void ai_contact_local_rng(ColonizeTurnContext* ctx, int nation_id, ColonizeDosRng* out) {
  uint32_t seed = 0xC07Au ^ (uint32_t)(nation_id * 97);
  if (ctx && ctx->turn_number) {
    seed ^= (uint32_t)(*ctx->turn_number) * 0x9E3779B9u;
  }
  if (ctx && ctx->rng_seed) {
    seed ^= ctx->rng_seed * 0x85ebca6bu;
  }
  dos_rng_seed(out, seed ? seed : 1u);
}

static void ai_contact_clamp_alarms(ColonizeCol1Indian* ind) {
  if (!ind) {
    return;
  }
  for (int e = 0; e < 4; ++e) {
    /* Signed alarm byte clamp stand-in (1816 §4): keep uint16 in band. */
    if (ind->alarm_by_player[e] > 200) {
      ind->alarm_by_player[e] = 200;
    }
  }
}

void ai_contact_indian_prelude(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  ai_contact_clamp_alarms(ind);

  /*
   * Alarm prelude flag body (PARKED dialog chrome).
   * DOS: when state+3 bit 0x20 clear, difficulty-scaled RNG may set war/alarm.
   * Linux: unknown31[3] bit 0x20 = prelude-fired; isolated RNG only.
   */
  uint8_t* flag = &ind->unknown31[3];
  if ((*flag & 0x20) == 0) {
    ColonizeDosRng local;
    ai_contact_local_rng(ctx, nation_id, &local);
    const int diff = ctx->col1->head.difficulty;
    /* Harder → more often escalate. */
    const int chance = 2 + (4 - diff);
    if (dos_rng_range(&local, 1, 8) <= chance) {
      for (int e = 0; e < 4; ++e) {
        if (ctx->col1->player[e].control == 2) {
          continue;
        }
        if (ind->met_by_player[e] && ind->alarm_by_player[e] < 30) {
          ind->alarm_by_player[e] = (uint16_t)(ind->alarm_by_player[e] + 5 + (4 - diff));
        }
      }
      /* War-ish sticky: mark bit so prelude does not re-roll forever. */
      *flag = (uint8_t)(*flag | 0x20);
    }
  }

  /* Mission clear on high alarm (FUN_4cc6_0000). */
  if (!ctx->col1->tribe) {
    return;
  }
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    if (t->mission == 0xff) {
      continue;
    }
    const int euro = (int)t->mission;
    if (euro < 0 || euro > 3) {
      continue;
    }
    if (t->alarm[euro].friction > 80 || ind->alarm_by_player[euro] > 80) {
      t->mission = 0xff;
    }
  }
}

void ai_contact_indian_relation_tick(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];

  /* FUN_4cc6_00f2 / 4962_06b6-shaped: met → ±1 by alarm band. */
  for (int e = 0; e < 4; ++e) {
    if (ctx->col1->player[e].control == 2) {
      continue;
    }
    int delta = 0;
    if (ind->met_by_player[e]) {
      delta = (ind->alarm_by_player[e] > 40) ? -1 : 1;
    }
    ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, delta);
  }
}

void ai_contact_indian_meet_trade(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];

  /*
   * FUN_5bfb_022e checklist:
   *  1) adjacent Euro → meet
   *  2) optional mission if peaceful (teach/convert UI PARKED)
   *  3) auto-haggle: trade-goods for alarm (2aac…311e stand-in)
   *  4) gift/demand dialogs PARKED
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
      continue;
    }
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int d = 0; d < 8; ++d) {
      const int nx = brave->x + dx[d];
      const int ny = brave->y + dy[d];
      const int oid = units_id_at(ctx->units, nx, ny);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id < 0 || other->nation_id > 3) {
        continue;
      }
      const int e = other->nation_id;

      /* 1–2. First meet. */
      if (!ind->met_by_player[e]) {
        ind->met_by_player[e] = 1;
        ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, 5);
        if (ctx->col1->tribe) {
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id == nation_id && t->mission == 0xff &&
                ai_contact_dist(t->x, t->y, brave->x, brave->y) <= 3) {
              if (t->alarm[e].friction < 30) {
                t->mission = (uint8_t)e; /* mission offer; convert UI PARKED */
              }
              break;
            }
          }
        }
      }

      /* 3. Peaceful auto-trade (nested 2bbc AI buy stand-in). */
      if (!ctx->colonies || !ind->met_by_player[e]) {
        continue;
      }
      if (ind->alarm_by_player[e] >= 50) {
        continue; /* too hostile for goods trade */
      }
      int best_ci = -1;
      int best_score = -1;
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        ColonizeColony* c = &ctx->colonies->colonies[ci];
        if (!c->active || c->nation_id != e) {
          continue;
        }
        const int dist = ai_contact_dist(c->x, c->y, brave->x, brave->y);
        if (dist > 4 || c->stock[COLONIZE_CARGO_TRADE_GOODS] <= 0) {
          continue;
        }
        /* Prefer closer + more goods (score-ordered haggle stand-in). */
        const int score = c->stock[COLONIZE_CARGO_TRADE_GOODS] * 4 - dist;
        if (score > best_score) {
          best_score = score;
          best_ci = ci;
        }
      }
      if (best_ci >= 0) {
        ColonizeColony* c = &ctx->colonies->colonies[best_ci];
        c->stock[COLONIZE_CARGO_TRADE_GOODS]--;
        if (ind->alarm_by_player[e] > 0) {
          ind->alarm_by_player[e]--;
        }
        if (ctx->col1->tribe) {
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id == nation_id && t->alarm[e].friction > 0) {
              t->alarm[e].friction--;
            }
          }
        }
        ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, 2);
      }
      /* 4. Gift / demand dialogs — PARKED (5bfb_102a / 1092). */
    }
  }
}

static AiRaidKind ai_contact_pick_raid_kind(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int max_alarm,
  ColonizeDosRng* rng
) {
  /* Banded picker mirroring @RAID* message outcomes (not DOS bit-identity). */
  if (max_alarm < 45) {
    return AI_RAID_NOTHING;
  }
  const int roll = rng ? dos_rng_range(rng, 0, 99) : (max_alarm % 100);
  if (max_alarm >= 85 && roll < 15) {
    return AI_RAID_WREAK;
  }
  if (max_alarm >= 70 && roll < 25) {
    return AI_RAID_SCALP;
  }
  if (max_alarm >= 60 && roll < 20 && c && c->building_in_production >= 0) {
    return AI_RAID_BURN;
  }
  if (max_alarm >= 55 && roll < 15 && ctx && ctx->col1_ok && ctx->col1) {
    return AI_RAID_GOLD;
  }
  if (max_alarm >= 50 && roll < 12 && c && ctx && ctx->map) {
    /* Harbor: prefer if water adjacent. */
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int d = 0; d < 8; ++d) {
      if (map_tile_is_water(ctx->map, c->x + dx[d], c->y + dy[d])) {
        if (roll < 10) {
          return AI_RAID_SHIP;
        }
        break;
      }
    }
  }
  return AI_RAID_STORES;
}

static void ai_contact_apply_raid_loot(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int target_euro,
  AiRaidKind kind
) {
  if (!c) {
    return;
  }
  s_last_raid_kind = (int)kind;

  switch (kind) {
  case AI_RAID_NOTHING:
    break;
  case AI_RAID_STORES: {
    static const int prefs[] = {
      COLONIZE_CARGO_FOOD,
      COLONIZE_CARGO_TRADE_GOODS,
      COLONIZE_CARGO_TOOLS,
      COLONIZE_CARGO_MUSKETS
    };
    for (size_t i = 0; i < sizeof(prefs) / sizeof(prefs[0]); ++i) {
      if (c->stock[prefs[i]] > 0) {
        c->stock[prefs[i]]--;
        break;
      }
    }
    break;
  }
  case AI_RAID_BURN:
    if (c->building_in_production >= 0) {
      c->building_in_production = -1;
    } else if (c->stock[COLONIZE_CARGO_LUMBER] > 0) {
      c->stock[COLONIZE_CARGO_LUMBER] -= (c->stock[COLONIZE_CARGO_LUMBER] > 2) ? 2 : 1;
    }
    break;
  case AI_RAID_SCALP:
    if (c->population > 1) {
      c->population--;
      if (c->colonist_count > 1) {
        c->colonist_count--;
      }
    }
    break;
  case AI_RAID_GOLD:
    if (ctx && ctx->col1_ok && ctx->col1 && target_euro >= 0 && target_euro < 4) {
      ColonizeCol1Nation* nat = &ctx->col1->nation[target_euro];
      if (nat->gold > 25) {
        nat->gold -= 25;
      } else if (nat->gold > 0) {
        nat->gold = 0;
      }
    }
    break;
  case AI_RAID_SHIP:
    if (ctx && ctx->units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != target_euro) {
          continue;
        }
        if (!units_is_sea(ctx->units, u->id)) {
          continue;
        }
        if (ai_contact_dist(u->x, u->y, c->x, c->y) > 2) {
          continue;
        }
        if (u->moves_left > 0) {
          u->moves_left = 0;
        }
        break;
      }
    }
    break;
  case AI_RAID_WREAK:
    if (c->stock[COLONIZE_CARGO_FOOD] > 0) {
      c->stock[COLONIZE_CARGO_FOOD]--;
    }
    if (c->stock[COLONIZE_CARGO_TOOLS] > 0) {
      c->stock[COLONIZE_CARGO_TOOLS]--;
    }
    if (c->building_in_production >= 0) {
      c->building_in_production = -1;
    }
    break;
  default:
    break;
  }
}

void ai_contact_indian_raids(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (nation_id < 4 || nation_id > 11 || !ctx->col1->tribe) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  ColonizeDosRng local;
  ai_contact_local_rng(ctx, nation_id, &local);
  ColonizeDosRng* rng = ctx->rng ? ctx->rng : &local;
  /* Prefer isolated RNG for loot picks so pulse stream stays untouched if shared. */
  rng = &local;

  /*
   * FUN_4d56_4528 / 5fef_0f14-shaped arms (thin):
   *  1 gate → 2 adjacent combat → 3 colony approach → 4 @RAID* loot →
   *  5 capture → 6 scout 359c stub. Deep 2820 PARKED.
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id || brave->moves_left <= 0) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
      continue;
    }

    /* 1. Gate: target Euro by max alarm/friction. */
    int target_euro = -1;
    int max_alarm = 0;
    for (int e = 0; e < 4; ++e) {
      int alarm = (int)ind->alarm_by_player[e];
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id == nation_id && (int)t->alarm[e].friction > alarm) {
          alarm = (int)t->alarm[e].friction;
        }
      }
      if (alarm > max_alarm) {
        max_alarm = alarm;
        target_euro = e;
      }
    }
    if (target_euro < 0 || max_alarm < 40) {
      continue;
    }

    /* 2. Adjacent unit combat. */
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    int attacked = 0;
    for (int d = 0; d < 8 && !attacked; ++d) {
      const int nx = brave->x + dx[d];
      const int ny = brave->y + dy[d];
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe < 0) {
        continue;
      }
      ColonizeUnit* f = units_get(ctx->units, foe);
      if (!f || f->nation_id != target_euro || units_is_sea(ctx->units, foe)) {
        continue;
      }
      if (units_resolve_land_combat(ctx->units, brave->id, foe, rng)) {
        units_try_move(ctx->units, brave->id, ctx->map, nx, ny, ctx->colonies, rng);
      }
      ind->alarm_by_player[target_euro] =
        (uint16_t)(ind->alarm_by_player[target_euro] + 2);
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id == nation_id) {
          t->alarm[target_euro].attacks++;
        }
      }
      attacked = 1;
    }

    /* 3–5. Colony approach / loot / capture. */
    if (!attacked && ctx->colonies && brave->active) {
      int best_cid = -1;
      int best_d = 99;
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        ColonizeColony* c = &ctx->colonies->colonies[ci];
        if (!c->active || c->nation_id != target_euro) {
          continue;
        }
        const int d = ai_contact_dist(brave->x, brave->y, c->x, c->y);
        if (d < best_d && d <= 6) {
          best_d = d;
          best_cid = c->id;
        }
      }
      if (best_cid >= 0) {
        ColonizeColony* c = colonies_get_mut(ctx->colonies, best_cid);
        if (!c) {
          continue;
        }
        if (brave->x == c->x && brave->y == c->y) {
          const AiRaidKind kind = ai_contact_pick_raid_kind(ctx, c, max_alarm, rng);
          ai_contact_apply_raid_loot(ctx, c, target_euro, kind);
          if (c->population <= 1 && max_alarm >= 70) {
            colonies_capture(ctx->colonies, best_cid, nation_id);
          }
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id == nation_id) {
              t->alarm[target_euro].attacks++;
              if (t->alarm[target_euro].friction < 200) {
                t->alarm[target_euro].friction =
                  (uint8_t)(t->alarm[target_euro].friction + 2);
              }
            }
          }
        } else {
          int sdx = (c->x > brave->x) - (c->x < brave->x);
          int sdy = (c->y > brave->y) - (c->y < brave->y);
          units_try_move(
            ctx->units, brave->id, ctx->map, brave->x + sdx, brave->y + sdy, ctx->colonies, rng
          );
        }
      }
    }
  }

  /* 6. FUN_4d56_359c stub: high alarm vs Scouts → despawn (warn/displace PARKED). */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id) {
      continue;
    }
    for (int e = 0; e < 4; ++e) {
      if (ind->alarm_by_player[e] < 90) {
        continue;
      }
      static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      for (int d = 0; d < 8; ++d) {
        const int foe = units_id_at(ctx->units, brave->x + dx[d], brave->y + dy[d]);
        if (foe < 0) {
          continue;
        }
        ColonizeUnit* f = units_get(ctx->units, foe);
        if (!f || f->nation_id != e) {
          continue;
        }
        const char* name = units_display_name(ctx->units, f);
        if (name && strstr(name, "Scout")) {
          units_despawn(ctx->units, foe);
        }
      }
    }
  }
}
