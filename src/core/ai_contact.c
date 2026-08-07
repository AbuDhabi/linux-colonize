#include "core/ai_contact.h"

#include "core/ai_diplo.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/units.h"

#include <stdlib.h>
#include <string.h>

static int ai_contact_dist(int x0, int y0, int x1, int y1) {
  const int dx = abs(x0 - x1);
  const int dy = abs(y0 - y1);
  return dx > dy ? dx : dy;
}

void ai_contact_indian_prelude(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];

  /* Relation tick vs each Euro (FUN_4cc6_00f2-shaped T0). */
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

void ai_contact_indian_meet_trade(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
      continue;
    }
    /* Adjacent Euro unit → meet (FUN_5bfb_022e T0). */
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
      if (!ind->met_by_player[e]) {
        ind->met_by_player[e] = 1;
        /* First meet: small positive relation + optional mission offer. */
        ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, 5);
        if (ctx->col1->tribe) {
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id == nation_id && t->mission == 0xff &&
                ai_contact_dist(t->x, t->y, brave->x, brave->y) <= 3) {
              /* Teach/mission: assign Euro mission if peaceful. */
              if (t->alarm[e].friction < 30) {
                t->mission = (uint8_t)e;
              }
              break;
            }
          }
        }
      }
      /* Trade T0: Euro gives trade goods from colony stock → lower alarm. */
      if (ctx->colonies && ind->met_by_player[e]) {
        for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
          ColonizeColony* c = &ctx->colonies->colonies[ci];
          if (!c->active || c->nation_id != e) {
            continue;
          }
          if (ai_contact_dist(c->x, c->y, brave->x, brave->y) > 4) {
            continue;
          }
          if (c->stock[COLONIZE_CARGO_TRADE_GOODS] > 0) {
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
            break;
          }
        }
      }
    }
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

  /*
   * @RAID*-shaped gate (T0): friction/alarm threshold triggers attack on
   * nearby foreign land units or underdefended colonies.
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id || brave->moves_left <= 0) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
      continue;
    }

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
    /* Threshold mirrors mid GAME.TXT raid bands (~40+). */
    if (target_euro < 0 || max_alarm < 40) {
      continue;
    }

    /* Prefer adjacent foreign unit of target nation. */
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
      if (!f || f->nation_id != target_euro) {
        continue;
      }
      if (units_is_sea(ctx->units, foe)) {
        continue;
      }
      if (units_resolve_land_combat(ctx->units, brave->id, foe, ctx->rng)) {
        units_try_move(ctx->units, brave->id, ctx->map, nx, ny, ctx->colonies, ctx->rng);
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

    /* Colony raid: move toward / capture weak colony. */
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
          /* Loot stock + optional capture if pop tiny. */
          if (c->stock[COLONIZE_CARGO_FOOD] > 0) {
            c->stock[COLONIZE_CARGO_FOOD]--;
          }
          if (c->population <= 1 && max_alarm >= 70) {
            colonies_capture(ctx->colonies, best_cid, nation_id);
          }
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id == nation_id) {
              t->alarm[target_euro].attacks++;
            }
          }
        } else {
          /* Step toward colony. */
          int sdx = (c->x > brave->x) - (c->x < brave->x);
          int sdy = (c->y > brave->y) - (c->y < brave->y);
          units_try_move(
            ctx->units, brave->id, ctx->map, brave->x + sdx, brave->y + sdy, ctx->colonies, ctx->rng
          );
        }
      }
    }
  }

  /* Relation-gated kill/warn/displace (FUN_4d56_359c T0): high alarm vs scouts. */
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
