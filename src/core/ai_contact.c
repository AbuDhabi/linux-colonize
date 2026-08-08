#include "core/ai_contact.h"

#include "core/ai_diplo.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/units.h"

#include <stdio.h>
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

/* Prefer human Euro for player-facing status chrome (unpark #1; widgets OPEN). */
static int ai_contact_euro_is_human(const ColonizeTurnContext* ctx, int e) {
  if (!ctx || e < 0 || e > 3) {
    return 0;
  }
  if (ctx->human_nation >= 0 && ctx->human_nation <= 3) {
    return e == ctx->human_nation;
  }
  if (ctx->col1_ok && ctx->col1) {
    return ctx->col1->player[e].control == 0;
  }
  return 0;
}

static void ai_contact_set_status(ColonizeTurnContext* ctx, const char* msg) {
  if (!ctx || !ctx->status || ctx->status_size == 0 || !msg) {
    return;
  }
  snprintf(ctx->status, ctx->status_size, "%s", msg);
}

/* @TRIBES order (Inca..Tupi); matches col1_bridge encounter labels. */
static const char* ai_contact_tribe_name(int nation_id) {
  static const char* k_names[8] = {
      "Inca", "Aztec", "Arawak", "Iroquois", "Cherokee", "Apache", "Sioux", "Tupi"
  };
  const int idx = nation_id - 4;
  if (idx < 0 || idx >= 8) {
    return "natives";
  }
  return k_names[idx];
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

/* Missionary / Jesuit Missionary / similar — name substring stand-in. */
static int ai_contact_is_missionary(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const char* name = units_display_name(units, u);
  return name && strstr(name, "Mission") != NULL;
}

/* Soldier / Scout / Pioneer — encroachment types that raise tribe alarm. */
static int ai_contact_is_encroacher(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const char* name = units_display_name(units, u);
  if (!name) {
    return 0;
  }
  return strstr(name, "Soldier") != NULL || strstr(name, "Scout") != NULL ||
         strstr(name, "Pioneer") != NULL;
}

/*
 * Pocahontas (wiki/fandom): Indian alarm generated half as fast for the Euro
 * that elected her. Halve positive friction/alarm bumps toward that nation.
 * Cite: docs/fandom_col1994.md Pocahontas; Colonization.pdf FF table.
 */
static int ai_contact_alarm_bump_amount(
  const ColonizeCol1Save* col1,
  int euro,
  int amount
) {
  if (amount <= 0) {
    return 0;
  }
  if (col1 && euro >= 0 && euro <= 3 &&
      founding_fathers_nation_has(col1, euro, FF_POCAHONTAS)) {
    return amount / 2; /* floor; +1 bumps become 0 */
  }
  return amount;
}

/* Bump uint8 friction toward cap 100. */
static void ai_contact_bump_u8_cap100(uint8_t* v, int amount) {
  if (!v || amount <= 0) {
    return;
  }
  int n = (int)(*v) + amount;
  if (n > 100) {
    n = 100;
  }
  *v = (uint8_t)n;
}

/* Bump uint16 alarm toward cap 100. */
static void ai_contact_bump_u16_cap100(uint16_t* v, int amount) {
  if (!v || amount <= 0) {
    return;
  }
  int n = (int)(*v) + amount;
  if (n > 100) {
    n = 100;
  }
  *v = (uint16_t)n;
}

/*
 * Peaceful teach-skill stub (5bfb / meet checklist): Free Colonist or Scout
 * adjacent to tribe, low alarm/friction → set Col1 tribe.state.learned and
 * optionally grant a native-teachable profession on the unit.
 * Teach dialog widgets still OPEN (unpark #1); status line thinned.
 * Full @TRIBES good-string parse PARKED — static cargo / nation_id maps below.
 */
static int ai_contact_is_teachable_learner(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const char* name = units_display_name(units, u);
  if (!name) {
    return 0;
  }
  return strstr(name, "Free Colonist") != NULL || strstr(name, "Scout") != NULL;
}

/* Warehouse cargo → outdoor @JOB (indices align food..silver). -1 unmapped. */
static int ai_contact_profession_from_cargo(int cargo) {
  switch (cargo) {
    case COLONIZE_CARGO_FOOD:
      return COLONIZE_JOB_FARMER;
    case COLONIZE_CARGO_SUGAR:
      return COLONIZE_JOB_SUGAR_PLANTER;
    case COLONIZE_CARGO_TOBACCO:
      return COLONIZE_JOB_TOBACCO_PLANTER;
    case COLONIZE_CARGO_COTTON:
      return COLONIZE_JOB_COTTON_PLANTER;
    case COLONIZE_CARGO_FURS:
      return COLONIZE_JOB_FUR_TRAPPER;
    case COLONIZE_CARGO_LUMBER:
      return COLONIZE_JOB_LUMBERJACK;
    case COLONIZE_CARGO_ORE:
      return COLONIZE_JOB_ORE_MINER;
    case COLONIZE_CARGO_SILVER:
      return COLONIZE_JOB_SILVER_MINER;
    default:
      return -1;
  }
}

/*
 * Rough Col1 nation_id (4..11) → primary taught skill. Order matches
 * NAMES.TXT @TRIBES (Inca..Tupi). Fish has no cargo id — nation only.
 * Returns -1 if out of band (caller falls back to Farmer).
 */
static int ai_contact_profession_from_nation(int nation_id) {
  static const int k_by_indian[8] = {
      COLONIZE_JOB_SILVER_MINER,    /* 4 Inca */
      COLONIZE_JOB_ORE_MINER,       /* 5 Aztec */
      COLONIZE_JOB_FISHERMAN,       /* 6 Arawak */
      COLONIZE_JOB_FUR_TRAPPER,     /* 7 Iroquois */
      COLONIZE_JOB_TOBACCO_PLANTER, /* 8 Cherokee */
      COLONIZE_JOB_COTTON_PLANTER,  /* 9 Apache */
      COLONIZE_JOB_FUR_TRAPPER,     /* 10 Sioux */
      COLONIZE_JOB_SUGAR_PLANTER,   /* 11 Tupi */
  };
  const int idx = nation_id - 4;
  if (idx < 0 || idx >= 8) {
    return -1;
  }
  return k_by_indian[idx];
}

/*
 * Resolve taught profession for an unskilled Free Colonist.
 * Prefer tribe.last_sold when it is a raw cargo 1..7 (sugar..silver) — food(0)
 * is left alone so zeroed Col1 tribes still take the nation map. Else nation
 * table; else Expert Farmer.
 */
static int ai_contact_taught_profession(const ColonizeCol1Tribe* t) {
  if (!t) {
    return COLONIZE_JOB_FARMER;
  }
  if (t->last_sold >= COLONIZE_CARGO_SUGAR && t->last_sold <= COLONIZE_CARGO_SILVER) {
    const int from_cargo = ai_contact_profession_from_cargo((int)t->last_sold);
    if (from_cargo >= 0) {
      return from_cargo;
    }
  }
  const int from_nation = ai_contact_profession_from_nation((int)t->nation_id);
  if (from_nation >= 0) {
    return from_nation;
  }
  return COLONIZE_JOB_FARMER;
}

static void ai_contact_teach_skill(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    /*
     * Col1 one-shot: tribe.state.learned already set → skip teach and do not
     * write teach/refuse status (preserves gift/trade chrome). Cite: village
     * teaches once; fandom Teach / trade / missions.
     */
    if (t->state.learned) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int oid = units_id_at(ctx->units, t->x + dx[d], t->y + dy[d]);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id < 0 || other->nation_id > 3) {
        continue;
      }
      if (!ai_contact_is_teachable_learner(ctx->units, other)) {
        continue;
      }
      const int e = other->nation_id;
      /*
       * Alarmed Indian diplomacy (fandom Alarm; same ≥55 refuse-talk gate):
       * high alarm/friction → refuse teach (status thinned; widgets OPEN).
       */
      if (ind->alarm_by_player[e] >= 55 || t->alarm[e].friction >= 55) {
        if (ai_contact_euro_is_human(ctx, e)) {
          ai_contact_set_status(ctx, "Natives refuse to teach.");
        }
        break; /* one refuse pulse per tribe per call */
      }
      /* Peaceful meet band (alarm/friction < 40); mid band → no teach. */
      if (ind->alarm_by_player[e] >= 40 || t->alarm[e].friction >= 40) {
        continue;
      }
      t->state.learned = 1;
      /*
       * Optional expertise: unskilled Free Colonist → tribe-appropriate
       * outdoor skill (cargo / nation map); Plain Scout → Seasoned Scout.
       * Already-skilled units keep profession.
       */
      int taught_scout = 0;
      if (other->profession == UNITS_JOB_NONE) {
        const char* name = units_display_name(ctx->units, other);
        if (name && strstr(name, "Scout") != NULL) {
          other->profession = UNITS_JOB_SCOUT;
          taught_scout = 1;
        } else {
          other->profession = ai_contact_taught_profession(t);
        }
      }
      if (ai_contact_euro_is_human(ctx, e)) {
        if (taught_scout) {
          ai_contact_set_status(ctx, "Natives teach Seasoned Scout.");
        } else {
          char line[96];
          snprintf(
            line,
            sizeof(line),
            "Natives teach %s skills.",
            ai_contact_tribe_name(nation_id)
          );
          ai_contact_set_status(ctx, line);
        }
      }
      break; /* one teach pulse per tribe per call */
    }
  }
}

/* Decay alarm_by_player + this nation's tribe frictions by `amount` (floor 0). */
static void ai_contact_friction_decay(
  ColonizeCol1Indian* ind,
  ColonizeCol1Save* col1,
  int nation_id,
  int e,
  int amount
) {
  if (!ind || amount <= 0 || e < 0 || e > 3) {
    return;
  }
  if ((int)ind->alarm_by_player[e] > amount) {
    ind->alarm_by_player[e] = (uint16_t)(ind->alarm_by_player[e] - (uint16_t)amount);
  } else {
    ind->alarm_by_player[e] = 0;
  }
  if (!col1 || !col1->tribe) {
    return;
  }
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    if ((int)t->alarm[e].friction > amount) {
      t->alarm[e].friction = (uint8_t)(t->alarm[e].friction - (uint8_t)amount);
    } else {
      t->alarm[e].friction = 0;
    }
  }
}

/* Max of alarm_by_player and tribe frictions for this Indian×Euro pair. */
static int ai_contact_pair_friction(
  const ColonizeCol1Indian* ind,
  const ColonizeCol1Save* col1,
  int nation_id,
  int e
) {
  int friction = ind ? (int)ind->alarm_by_player[e] : 0;
  if (!col1 || !col1->tribe) {
    return friction;
  }
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->nation_id == nation_id && (int)t->alarm[e].friction > friction) {
      friction = (int)t->alarm[e].friction;
    }
  }
  return friction;
}

/*
 * Gift / demand structural stand-in (5bfb_102a / 1092 dialog widgets OPEN).
 * After peaceful meet adjacency (caller already gated alarmed / very-low rel):
 *  - alarmed (≥55 refuse-talk gate) → refuse gift/demand with status; no extra
 *    gold penalty beyond existing gift costs
 *  - low friction + Euro gold < 10 → refuse refuse (cannot pay −10 gift)
 *  - low friction + Euro gold >= 20 → gift: Euro −10 gold, friction −2
 *  - mid friction (40–54) + tools/gold → demand: −10 tools from nearest colony
 *    warehouse when stock ≥20 (else unit tools ≥20, else −15 gold when
 *    treasury ≥50); friction −3. Cite: gift gold≥20 band mirrored for tools;
 *    gold stand-in needs a fuller purse (≥50) when tools are short.
 *  - very high covered by alarmed refuse / raids
 * Human-facing paths set a thin status line when ctx->status is present.
 * Source: fandom Alarm — alarmed natives may refuse trade/gifts.
 */
static void ai_contact_gift_or_demand(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  ColonizeUnit* other,
  int near_x,
  int near_y
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || !other || e < 0 || e > 3) {
    return;
  }
  if (!ind->met_by_player[e]) {
    return;
  }
  const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
  const int human = ai_contact_euro_is_human(ctx, e);
  /*
   * Same ≥55 gate as refuse-talk/teach: alarmed → no gift and no demand
   * payoff (no invented gold penalties). Cite: fandom Alarm — refuse trade.
   * Message band uses tribe friction (not alarm_by_player): gift-band (<40) →
   * "refuse gifts"; demand-band (≥40) → "refuse demands". Pair friction alone
   * would always be ≥55 when alarm≥55, making gift-band unreachable.
   */
  if (friction >= 55 || ind->alarm_by_player[e] >= 55) {
    if (human) {
      int tribe_fr = 0;
      if (ctx->col1->tribe) {
        for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
          const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
          if ((int)t->nation_id == nation_id && (int)t->alarm[e].friction > tribe_fr) {
            tribe_fr = (int)t->alarm[e].friction;
          }
        }
      }
      ai_contact_set_status(
        ctx,
        (tribe_fr >= 40) ? "Natives refuse demands." : "Natives refuse gifts."
      );
    }
    return; /* alarmed / very high — raids handle hostility; no invented gold penalty */
  }

  ColonizeCol1Nation* nat = &ctx->col1->nation[e];

  /* Low friction gift / tribute. */
  if (friction < 40) {
    /* Cannot pay −10 gift drain → refuse with status (widgets OPEN). */
    if (nat->gold < 10u) {
      if (human) {
        ai_contact_set_status(ctx, "Natives refuse gifts.");
      }
      return;
    }
    if (nat->gold < 20u) {
      return; /* mid purse: skip silent (needs ≥20 band to gift) */
    }
    nat->gold -= 10u;
    ai_contact_friction_decay(ind, ctx->col1, nation_id, e, 2);
    if (human) {
      ai_contact_set_status(ctx, "Gift of gold eases tensions.");
    }
    return;
  }

  /* Mid friction (40–54) demand / payoff; ≥55 refused above. */
  int paid = 0;
  if (ctx->colonies) {
    int best_ci = -1;
    int best_d = 99;
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id != e) {
        continue;
      }
      /* Succeed path: warehouse must hold ≥20 tools before −10 demand. */
      if (c->stock[COLONIZE_CARGO_TOOLS] < 20) {
        continue;
      }
      const int dist = ai_contact_dist(c->x, c->y, near_x, near_y);
      if (dist < best_d) {
        best_d = dist;
        best_ci = ci;
      }
    }
    if (best_ci >= 0) {
      ctx->colonies->colonies[best_ci].stock[COLONIZE_CARGO_TOOLS] -= 10;
      paid = 1;
    }
  }
  if (!paid && other->tools >= 20) {
    other->tools -= 10;
    paid = 1;
  }
  /*
   * Tools short (warehouse/unit both <20): gold stand-in when treasury ≥50.
   * Drain stays −15 (indian_contact.md mid demand); purse gate mirrors a
   * fuller tribute ability than the gift −10/≥20 band.
   */
  if (!paid && nat->gold >= 50u) {
    nat->gold -= 15u;
    paid = 1;
  }
  if (!paid) {
    return; /* no tools in colony/unit and gold < 50 */
  }
  ai_contact_friction_decay(ind, ctx->col1, nation_id, e, 3);
  if (human) {
    ai_contact_set_status(ctx, "Tribute paid; tensions ease.");
  }
}

/*
 * Missionary adjacent to tribe → convert pulse (5bfb checklist / fandom Alarm):
 *  - mission unset (0xff) → set mission owner, slight alarm/friction decay,
 *    +1 nation crosses; human status "Natives accept conversion."
 *  - mission already set (own or foreign) → skip convert pulse (one-shot;
 *    no re-crosses / no steal). Cite: indian_contact.md convert once.
 *  - alarmed (≥55 refuse-talk gate) → refuse convert with status; no crosses
 * Teach/convert dialog widgets OPEN (unpark #1); full 2820/4528 PARKED.
 */
static void ai_contact_missionary_convert(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    /*
     * Convert once: mission already set → skip pulse entirely (own keep or
     * foreign no-steal). Matches teach one-shot; no repeated crosses.
     */
    if (t->mission != 0xff) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int oid = units_id_at(ctx->units, t->x + dx[d], t->y + dy[d]);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id < 0 || other->nation_id > 3) {
        continue;
      }
      if (!ai_contact_is_missionary(ctx->units, other)) {
        continue;
      }
      const int e = other->nation_id;
      /*
       * Alarmed Indian diplomacy (fandom Alarm; same ≥55 refuse-talk gate):
       * refuse convert / crosses (status thinned; widgets OPEN).
       */
      if (ind->alarm_by_player[e] >= 55 || t->alarm[e].friction >= 55) {
        if (ai_contact_euro_is_human(ctx, e)) {
          ai_contact_set_status(ctx, "Natives refuse conversion.");
        }
        break; /* one refuse pulse per tribe per call */
      }
      t->mission = (uint8_t)e;
      if (ind->alarm_by_player[e] > 0) {
        ind->alarm_by_player[e]--;
      }
      if (t->alarm[e].friction > 0) {
        t->alarm[e].friction--;
      }
      ColonizeCol1Nation* nat = &ctx->col1->nation[e];
      if (nat->current_crosses < 0xffffu) {
        nat->current_crosses++;
      }
      if (ai_contact_euro_is_human(ctx, e)) {
        ai_contact_set_status(ctx, "Natives accept conversion.");
      }
      break; /* one convert pulse per tribe per call */
    }
  }
}

/*
 * Missionary flee (structural): adjacent to alarmed tribe (≥55 refuse-talk
 * band) and not converting → nudge 1 free land tile away + AI_MOVE goto.
 * Cite: fandom Alarm — alarmed natives may refuse / attack missionaries.
 * Full 2820/4528 flee dialog PARKED; widgets OPEN.
 */
static int ai_contact_flee_one_tile(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int away_x,
  int away_y
) {
  if (!ctx || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  const int ox = u->x;
  const int oy = u->y;
  const int dist0 = ai_contact_dist(ox, oy, away_x, away_y);
  int best_x = -1;
  int best_y = -1;
  int best_d = -1;
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int nx = ox + dx[d];
    const int ny = oy + dy[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    if (!map_tile_is_land(ctx->map, nx, ny)) {
      continue;
    }
    if (units_id_at(ctx->units, nx, ny) >= 0) {
      continue;
    }
    if (!units_can_enter(ctx->units, u->type_index, ctx->map, nx, ny, u->id, ctx->colonies)) {
      continue;
    }
    const int dist = ai_contact_dist(nx, ny, away_x, away_y);
    if (dist < dist0) {
      continue; /* must increase Chebyshev distance from tribe */
    }
    if (dist > best_d) {
      best_d = dist;
      best_x = nx;
      best_y = ny;
    }
  }
  if (best_x < 0) {
    return 0;
  }
  u->x = best_x;
  u->y = best_y;
  u->orders = UNITS_ORDER_AI_MOVE;
  u->goto_x = best_x;
  u->goto_y = best_y;
  return 1;
}

static void ai_contact_missionary_flee(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int oid = units_id_at(ctx->units, t->x + dx[d], t->y + dy[d]);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id < 0 || other->nation_id > 3) {
        continue;
      }
      if (!ai_contact_is_missionary(ctx->units, other)) {
        continue;
      }
      const int e = other->nation_id;
      /* Only flee when convert is blocked by alarm (not converting). */
      if (ind->alarm_by_player[e] < 55 && t->alarm[e].friction < 55) {
        continue;
      }
      if (ai_contact_flee_one_tile(ctx, other, t->x, t->y)) {
        break; /* one flee pulse per tribe per call */
      }
    }
  }
}

/*
 * Meet-pulse mission pacify deepen: mission owner present and mid-range
 * friction/alarm (40..80, below FUN_4cc6_0000 clear) → −2 tribe friction and
 * matching alarm_by_player (floor 0). Once per tribe per call.
 * Magnitude stays near prelude low-band −1; no free crosses.
 * Source: fandom Alarm — missions slow hostility / pacify.
 */
static void ai_contact_mission_pacify_meet(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id || t->mission == 0xff) {
      continue;
    }
    const int euro = (int)t->mission;
    if (euro < 0 || euro > 3) {
      continue;
    }
    const int fr = (int)t->alarm[euro].friction;
    const int al = (int)ind->alarm_by_player[euro];
    /* Mid-range only; low-band stays prelude −1; ≥80 → mission burn/clear. */
    if ((fr < 40 || fr >= 80) && (al < 40 || al >= 80)) {
      continue;
    }
    if (fr >= 40 && fr < 80) {
      t->alarm[euro].friction = (uint8_t)(fr >= 2 ? fr - 2 : 0);
    }
    if (al >= 40 && al < 80) {
      ind->alarm_by_player[euro] = (uint16_t)(al >= 2 ? al - 2 : 0);
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
   * Alarm prelude escalate (PARKED dialog chrome).
   * DOS: when state+3 bit 0x20 clear, difficulty-scaled RNG may set war/alarm.
   * Linux: unknown31[3] bit 0x20 = prelude-fired; isolated RNG only.
   * Pocahontas still halves the escalate bump (wiki/fandom half-rate).
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
          /* Pocahontas: half-rate alarm growth (wiki/fandom). */
          const int bump = ai_contact_alarm_bump_amount(
            ctx->col1, e, 5 + (4 - diff)
          );
          if (bump > 0) {
            ind->alarm_by_player[e] =
              (uint16_t)(ind->alarm_by_player[e] + (uint16_t)bump);
          }
        }
      }
      /* War-ish sticky: mark bit so prelude does not re-roll forever. */
      *flag = (uint8_t)(*flag | 0x20);
    }
  }

  if (!ctx->col1->tribe) {
    return;
  }

  /*
   * Encroachment deepen (dialog chrome PARKED): Soldier/Scout/Pioneer within
   * Chebyshev ≤2 of a tribe with no mission → +2 tribe friction + alarm_by_player
   * toward that Euro (cap 100). Pocahontas halves bump (wiki/fandom half-rate).
   */
  if (ctx->units) {
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      ColonizeUnit* u = &ctx->units->units[ui];
      if (!u->active || u->nation_id < 0 || u->nation_id > 3) {
        continue;
      }
      if (units_is_sea(ctx->units, u->id)) {
        continue;
      }
      if (!ai_contact_is_encroacher(ctx->units, u)) {
        continue;
      }
      const int e = u->nation_id;
      const int bump = ai_contact_alarm_bump_amount(ctx->col1, e, 2);
      if (bump <= 0) {
        continue;
      }
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id != nation_id) {
          continue;
        }
        if (t->mission != 0xff) {
          continue; /* mission present → no encroachment bump */
        }
        if (ai_contact_dist(u->x, u->y, t->x, t->y) > 2) {
          continue;
        }
        ai_contact_bump_u8_cap100(&t->alarm[e].friction, bump);
        ai_contact_bump_u16_cap100(&ind->alarm_by_player[e], bump);
      }
    }
  }

  /*
   * Mission pacifies: tribe with mission + low friction toward mission Euro →
   * extra −1 friction (and matching alarm_by_player if also low). Dialog PARKED.
   */
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
    if (t->alarm[euro].friction < 40 && t->alarm[euro].friction > 0) {
      t->alarm[euro].friction--;
    }
    if (ind->alarm_by_player[euro] < 40 && ind->alarm_by_player[euro] > 0) {
      ind->alarm_by_player[euro]--;
    }
  }

  /*
   * Mission destroy / burn on high alarm (FUN_4cc6_0000; tribe.mission field).
   * Cite: manual/wiki — alarmed natives may burn missions. Alarm/friction ≥80
   * + mission present → clear stand-in (0xff). Status thinned; widgets OPEN.
   */
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
    if (t->alarm[euro].friction >= 80 || ind->alarm_by_player[euro] >= 80) {
      t->mission = 0xff;
      if (ai_contact_euro_is_human(ctx, euro)) {
        ai_contact_set_status(ctx, "Natives burn your mission.");
      }
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
   *  2) optional mission if peaceful; Missionary convert + teach-skill (below)
   *  3) auto-haggle: trade-goods for alarm (2aac…311e stand-in)
   *  4) gift/demand structural stand-in (dialogs 5bfb_102a / 1092 widgets OPEN)
   * Human-facing arms write thin ctx->status lines; full DOS dialog PARKED.
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
      const int human = ai_contact_euro_is_human(ctx, e);

      /* 1–2. First meet. */
      if (!ind->met_by_player[e]) {
        ind->met_by_player[e] = 1;
        ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, 5);
        if (human) {
          char line[96];
          snprintf(
            line,
            sizeof(line),
            "You meet the %s.",
            ai_contact_tribe_name(nation_id)
          );
          ai_contact_set_status(ctx, line);
        }
        if (ctx->col1->tribe) {
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id != nation_id ||
                ai_contact_dist(t->x, t->y, brave->x, brave->y) > 3) {
              continue;
            }
            /* Peaceful meet: slight friction decay on tribe alarm. */
            if (ind->alarm_by_player[e] < 40 && t->alarm[e].friction > 0 &&
                t->alarm[e].friction < 40) {
              t->alarm[e].friction--;
            }
            if (t->mission == 0xff && t->alarm[e].friction < 30) {
              t->mission = (uint8_t)e; /* mission offer; convert widgets OPEN */
            }
            break;
          }
        }
      }

      /*
       * Thin alarmed meet arm (2154/2820 deep PARKED): high alarm → refuse
       * talk status. Do not continue — auto-trade stays gated by alarm < 50
       * below; gift/demand still runs so ≥55 refuse gifts/demands chrome can
       * overwrite with the more specific line (fandom Alarm). Widgets OPEN.
       */
      if (ind->met_by_player[e] && ind->alarm_by_player[e] >= 55) {
        if (human) {
          ai_contact_set_status(ctx, "Natives refuse to talk.");
        }
        /* fall through: no auto-trade; gift_or_demand refuses */
      } else if (
        ind->met_by_player[e] &&
        ai_diplo_indian_relation(ctx->col1, nation_id, e) < 40
      ) {
        /*
         * Beyond alarm gate: very-low Indian×Euro relation (<40) also refuses
         * meet-trade / gift (ai_diplo_indian_relation read-only). Same band as
         * AI_DIPLO_INDIAN_VERY_LOW_REL / euro_diplo war deepen. First-meet above
         * still runs; widgets OPEN. Cite: fandom Alarm — refuse trade.
         */
        if (human) {
          ai_contact_set_status(ctx, "Natives refuse to talk.");
        }
        continue;
      }

      /*
       * 3. Peaceful auto-trade (nested 2bbc AI buy stand-in).
       *
       * PARK deep FUN_4d56_2820 (~1.4k lines; thunk 2a1f_044c):
       *   - full meet/raid decision matrix (alarmed vs peaceful branch pick)
       *   - nested trade 2aac (good dispatch) → 2af6 / 2bbc (AI buy) /
       *     2b92 / 311e (demand / no-deal close)
       *   - choice loops, hard-bargain tension, per-good price arms
       *   - alarmed-branch dialog dispatch (widgets OPEN — unpark #1)
       * Not ported — Linux stays on this thin trade-goods→alarm decay stand-in
       * plus gift/demand / teach / convert status chrome. Cite:
       * indian_contact.md PORT DEBT; docs/ai_transcription.md FUN_4d56_2820;
       * peel layer_b_combat_raid / layer_b_2a1f_midlo.
       */
      if (ctx->colonies && ind->met_by_player[e] && ind->alarm_by_player[e] < 50) {
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
          if (human) {
            ai_contact_set_status(ctx, "Trade accepted.");
          }
        }
      }

      /* 4. Gift / demand stand-in (5bfb_102a / 1092 status thinned; widgets OPEN). */
      ai_contact_gift_or_demand(ctx, ind, nation_id, e, other, brave->x, brave->y);
    }
  }

  /* 2b. Missionary adjacent to tribe → mission owner + crosses (convert widgets OPEN). */
  ai_contact_missionary_convert(ctx, nation_id);

  /* 2b1. Alarmed tribe + Missionary not converting → flee 1 tile (AI_MOVE). */
  ai_contact_missionary_flee(ctx, nation_id);

  /*
   * 2b2. Mission pacify deepen (meet pulse): mid-range alarm/friction toward
   * mission Euro → −2 once (prelude keeps low-band −1). Cite: fandom Alarm —
   * missions slow hostility. No free crosses. Burn/clear at ≥80 stays in prelude.
   */
  ai_contact_mission_pacify_meet(ctx, nation_id);

  /* 2c. Peaceful Free Colonist/Scout at tribe → state.learned + optional skill. */
  ai_contact_teach_skill(ctx, nation_id);
}

/* True if colony warehouse has any cargo the STORES arm can actually drain. */
static int ai_contact_colony_has_stores(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  return c->stock[COLONIZE_CARGO_FOOD] > 0 || c->stock[COLONIZE_CARGO_TRADE_GOODS] > 0 ||
         c->stock[COLONIZE_CARGO_TOOLS] > 0 || c->stock[COLONIZE_CARGO_MUSKETS] > 0;
}

/* True if warehouse holds military loot secondary can drain (muskets/horses). */
static int ai_contact_colony_has_military_loot(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  return c->stock[COLONIZE_CARGO_MUSKETS] >= 5 || c->stock[COLONIZE_CARGO_HORSES] >= 1;
}

/* True if warehouse holds enough tools for high-friction secondary −1 drain. */
static int ai_contact_colony_has_tools_loot(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  return c->stock[COLONIZE_CARGO_TOOLS] >= 10;
}

/* True if WREAK can mutate food/tools/building-in-production. */
static int ai_contact_colony_has_wreak_target(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  return c->stock[COLONIZE_CARGO_FOOD] > 0 || c->stock[COLONIZE_CARGO_TOOLS] > 0 ||
         c->building_in_production >= 0;
}

static AiRaidKind ai_contact_pick_raid_kind(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int target_euro,
  int max_alarm,
  ColonizeDosRng* rng
) {
  /*
   * Banded picker mirroring @RAID* message outcomes (not DOS bit-identity).
   * Gate kinds on colony stock / gold actually present so empty warehouses
   * do not fake STORES/WREAK/muskets loot (5fef_0f14-shaped). No Indian-nation
   * treasury fiction — GOLD drains Euro gold only when present.
   */
  if (max_alarm < 45) {
    return AI_RAID_NOTHING;
  }
  const int roll = rng ? dos_rng_range(rng, 0, 99) : (max_alarm % 100);
  if (max_alarm >= 85 && roll < 15 && ai_contact_colony_has_wreak_target(c)) {
    return AI_RAID_WREAK;
  }
  if (max_alarm >= 70 && roll < 25 && c && c->population > 1) {
    return AI_RAID_SCALP;
  }
  if (max_alarm >= 60 && roll < 20 && c && c->building_in_production >= 0) {
    return AI_RAID_BURN;
  }
  if (max_alarm >= 55 && roll < 15 && ctx && ctx->col1_ok && ctx->col1 &&
      target_euro >= 0 && target_euro < 4 &&
      ctx->col1->nation[target_euro].gold > 0) {
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
  /* STORES only when warehouse actually holds lootable cargo. */
  if (ai_contact_colony_has_stores(c)) {
    return AI_RAID_STORES;
  }
  if (c && c->population > 1 && max_alarm >= 70) {
    return AI_RAID_SCALP;
  }
  if (c && c->building_in_production >= 0 && max_alarm >= 60) {
    return AI_RAID_BURN;
  }
  return AI_RAID_NOTHING;
}

/*
 * Secondary multi-loot after a successful primary @RAID* (kind != NOTHING).
 *  - Military side-steal: only if warehouse/unit actually holds muskets/horses
 *    (−5 muskets stock, else −1 horse stock, else same from target-nation unit
 *    gear on the colony tile). Empty warehouses do not fake muskets loot.
 *  - High friction (≥80): also drain tools (−1) when stock present.
 * Full 5fef_0f14 / 4528 dialog PARKED.
 */
static void ai_contact_raid_secondary_loot(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int target_euro,
  int max_alarm
) {
  if (!c) {
    return;
  }

  if (c->stock[COLONIZE_CARGO_MUSKETS] >= 5) {
    c->stock[COLONIZE_CARGO_MUSKETS] -= 5;
  } else if (c->stock[COLONIZE_CARGO_HORSES] >= 1) {
    c->stock[COLONIZE_CARGO_HORSES] -= 1;
  } else if (ctx && ctx->units && target_euro >= 0 && target_euro < 4) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != target_euro) {
        continue;
      }
      if (u->x != c->x || u->y != c->y) {
        continue;
      }
      if (u->muskets >= 5) {
        u->muskets -= 5;
        break;
      }
      if (u->horses >= 1) {
        u->horses -= 1;
        break;
      }
    }
  }
  /* else: empty warehouse + no unit gear → no fake military loot */

  if (max_alarm >= 80 && c->stock[COLONIZE_CARGO_TOOLS] > 0) {
    c->stock[COLONIZE_CARGO_TOOLS]--;
  }
}

static void ai_contact_apply_raid_loot(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int target_euro,
  AiRaidKind kind,
  int max_alarm
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
    /* @RAIDBURN / 5fef_0f14: clear construction first. */
    if (c->building_in_production >= 0) {
      c->building_in_production = -1;
    } else if (c->stock[COLONIZE_CARGO_LUMBER] > 0) {
      c->stock[COLONIZE_CARGO_LUMBER] -= (c->stock[COLONIZE_CARGO_LUMBER] > 2) ? 2 : 1;
    } else {
      /*
       * PARKED: when warehouse stock empty, prefer damaging/removing a
       * non-Town-Hall built building (@RAIDBURN building loot). No safe
       * colonies_* destroy API that also clears workplace colonists —
       * do not flip has_building[] alone. Full 5fef_0f14 PARKED.
       */
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

  if (kind != AI_RAID_NOTHING) {
    ai_contact_raid_secondary_loot(ctx, c, target_euro, max_alarm);
  }
}

/*
 * FUN_4d56_359c thin displace: nudge Scout onto free land 1–2 tiles from
 * current tile, preferring greater Chebyshev distance from (away_x,away_y)
 * (Brave / tribe contact). Sets AI_MOVE goto at the flee tile. Returns 1 if
 * moved, 0 if no free land tile (caller may despawn).
 */
static int ai_contact_displace_scout(
  ColonizeTurnContext* ctx,
  ColonizeUnit* scout,
  int away_x,
  int away_y
) {
  if (!ctx || !ctx->units || !ctx->map || !scout || !scout->active) {
    return 0;
  }
  const int ox = scout->x;
  const int oy = scout->y;
  const int dist0 = ai_contact_dist(ox, oy, away_x, away_y);
  int best_x = -1;
  int best_y = -1;
  int best_score = -1;
  int fallback_x = -1;
  int fallback_y = -1;
  int fallback_score = -1;

  for (int dy = -2; dy <= 2; ++dy) {
    for (int dx = -2; dx <= 2; ++dx) {
      const int adx = dx < 0 ? -dx : dx;
      const int ady = dy < 0 ? -dy : dy;
      const int cheb = adx > ady ? adx : ady;
      if (cheb < 1 || cheb > 2) {
        continue;
      }
      const int nx = ox + dx;
      const int ny = oy + dy;
      if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
        continue;
      }
      if (!map_tile_is_land(ctx->map, nx, ny)) {
        continue;
      }
      if (units_id_at(ctx->units, nx, ny) >= 0) {
        continue;
      }
      if (!units_can_enter(
            ctx->units, scout->type_index, ctx->map, nx, ny, scout->id, ctx->colonies
          )) {
        continue;
      }
      const int d = ai_contact_dist(nx, ny, away_x, away_y);
      const int score = d * 10 + cheb;
      if (score > fallback_score) {
        fallback_score = score;
        fallback_x = nx;
        fallback_y = ny;
      }
      if (d < dist0) {
        continue;
      }
      if (score > best_score) {
        best_score = score;
        best_x = nx;
        best_y = ny;
      }
    }
  }

  if (best_x < 0) {
    best_x = fallback_x;
    best_y = fallback_y;
  }
  if (best_x < 0) {
    return 0;
  }
  scout->x = best_x;
  scout->y = best_y;
  scout->orders = UNITS_ORDER_AI_MOVE;
  scout->goto_x = best_x;
  scout->goto_y = best_y;
  return 1;
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
   *  5 capture → 6 scout 359c displace/despawn.
   *
   * PARK deep FUN_4d56_2820 (~1.4k; thunk 2a1f_044c): meet/raid decision
   * matrix that DOS reaches before settlement enter — nested 2aac…311e trade
   * helpers and alarmed act pick live there, not in this post-pulse path.
   * Full FUN_4d56_4528 (~3k) settlement body also PARKED (comment only).
   * Linux stays on thin @RAID* / combat / 359c. Widgets OPEN (unpark #1).
   * Mid-friction gate prefers non-mission villages (below). Cite:
   * indian_raid_outcomes.md §10; indian_contact.md PORT DEBT;
   * docs/ai_transcription.md FUN_4d56_2820.
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id || brave->moves_left <= 0) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
      continue;
    }

    /*
     * 1. Gate: among Euros with friction/alarm ≥40, prefer Indian×Euro at-war
     * (ai_diplo_indian_at_war / relation <50); then highest friction; tie-break
     * lower ai_diplo_indian_relation (very-low <40 hostility).
     * Cite: indian_raid_outcomes.md gate; FUN_4d56_4528 thin; fandom Alarm.
     *
     * PARK: deep Brave escort / alarmed act inside quiet FUN_4d56_14fe
     * (seed-100 T2) — comment only; raids stay on this post-pulse path.
     */
    int target_euro = -1;
    int max_alarm = 0;
    int best_rel = 256;
    int best_at_war = 0;
    const int indian_idx = nation_id - 4;
    for (int e = 0; e < 4; ++e) {
      int alarm = (int)ind->alarm_by_player[e];
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id != nation_id || (int)t->alarm[e].friction <= alarm) {
          continue;
        }
        /*
         * Mid friction: prefer non-mission villages for the raid gate
         * (fandom Alarm — missions slow hostility). Mission tribes only
         * raise the gate in the burn band (≥80). Cite: indian_contact.md;
         * docs/fandom_col1994.md Alarm.
         */
        if (t->mission != 0xff && (int)t->alarm[e].friction < 80) {
          continue;
        }
        alarm = (int)t->alarm[e].friction;
      }
      if (alarm < 40) {
        continue; /* not a raid candidate */
      }
      const int at_war = ai_diplo_indian_at_war(ctx->col1, e, indian_idx);
      const int rel = (int)ai_diplo_indian_relation(ctx->col1, nation_id, e);
      if (at_war > best_at_war ||
          (at_war == best_at_war &&
           (alarm > max_alarm || (alarm == max_alarm && rel < best_rel)))) {
        best_at_war = at_war;
        max_alarm = alarm;
        best_rel = rel;
        target_euro = e;
      }
    }
    if (target_euro < 0) {
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
      {
        /* Pocahontas: half-rate alarm growth (wiki/fandom). */
        const int bump = ai_contact_alarm_bump_amount(ctx->col1, target_euro, 2);
        if (bump > 0) {
          ind->alarm_by_player[target_euro] =
            (uint16_t)(ind->alarm_by_player[target_euro] + (uint16_t)bump);
        }
      }
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
      int best_mil = 0;
      int best_tools = 0;
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        ColonizeColony* c = &ctx->colonies->colonies[ci];
        if (!c->active || c->nation_id != target_euro) {
          continue;
        }
        const int d = ai_contact_dist(brave->x, brave->y, c->x, c->y);
        if (d > 6) {
          continue;
        }
        /*
         * Prefer closer; at equal distance prefer muskets/horses (military
         * secondary), else tools≥10 (high-friction secondary −1). Cite:
         * indian_raid_outcomes.md multi-loot / colony approach.
         */
        const int mil = ai_contact_colony_has_military_loot(c);
        const int tools = ai_contact_colony_has_tools_loot(c);
        if (d < best_d || (d == best_d && mil && !best_mil) ||
            (d == best_d && mil == best_mil && tools && !best_tools)) {
          best_d = d;
          best_cid = c->id;
          best_mil = mil;
          best_tools = tools;
        }
      }
      if (best_cid >= 0) {
        ColonizeColony* c = colonies_get_mut(ctx->colonies, best_cid);
        if (!c) {
          continue;
        }
        if (brave->x == c->x && brave->y == c->y) {
          const AiRaidKind kind = ai_contact_pick_raid_kind(ctx, c, target_euro, max_alarm, rng);
          ai_contact_apply_raid_loot(ctx, c, target_euro, kind, max_alarm);
          if (c->population <= 1 && max_alarm >= 70) {
            colonies_capture(ctx->colonies, best_cid, nation_id);
          }
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id == nation_id) {
              t->alarm[target_euro].attacks++;
            }
          }
          /*
           * Successful raid friction/alarm escalate (fandom Alarm — raids raise
           * tension): tribe friction + alarm_by_player +2 each (cap 100).
           * Pocahontas halves bump (wiki/fandom half-rate). Cite:
           * docs/fandom_col1994.md Pocahontas / Alarm; indian_raid_outcomes.md.
           * Full 4528/2820 dialog PARKED; widgets OPEN.
           */
          if (kind != AI_RAID_NOTHING) {
            const int fr_bump =
              ai_contact_alarm_bump_amount(ctx->col1, target_euro, 2);
            if (fr_bump > 0) {
              ai_contact_bump_u16_cap100(&ind->alarm_by_player[target_euro], fr_bump);
              for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
                ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
                if ((int)t->nation_id == nation_id) {
                  ai_contact_bump_u8_cap100(&t->alarm[target_euro].friction, fr_bump);
                }
              }
            }
          }
          /*
           * High-friction successful raid → escalate Indian×Euro hostility
           * (4cc6_00f2 via ai_diplo). Full 4528/2820 dialog PARKED.
           */
          if (kind != AI_RAID_NOTHING && max_alarm >= 55) {
            const int host = (max_alarm >= 80) ? -5 : -3;
            ai_diplo_indian_relation_delta(ctx->col1, nation_id, target_euro, host);
          }
          /*
           * Thin raid outcome status for human target (full @RAID* dialog PARKED).
           * @RAIDNOTHING (GAME.TXT): "raiding party wiped out" — empty warehouse /
           * no lootable stock also lands here (no invented cargo). Cite:
           * COLONIZE/GAME.TXT @RAIDNOTHING; indian_raid_outcomes.md.
           */
          if (ai_contact_euro_is_human(ctx, target_euro)) {
            ai_contact_set_status(
              ctx,
              (kind == AI_RAID_NOTHING) ? "Native raiding party wiped out."
                                        : "Natives raid your colony."
            );
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

  /* 6. FUN_4d56_359c: high alarm vs Scouts → prefer displace; despawn if blocked. */
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
        if (!name || !strstr(name, "Scout")) {
          continue;
        }
        /*
         * FUN_4d56_359c: prefer displace 1–2 tiles away from the Brave.
         * When displaced (not despawned) and status buffer present → human
         * warn line. Dialog warn widgets beyond status still OPEN (unpark #1).
         *
         * PARK: DOS RNG kill/warn/displace when a flee tile exists — Linux
         * never kills if displace succeeds. Blocked-path despawn below is the
         * thin stand-in only (indian_raid_outcomes.md §8).
         */
        if (ai_contact_displace_scout(ctx, f, brave->x, brave->y)) {
          if (ai_contact_euro_is_human(ctx, e)) {
            ai_contact_set_status(ctx, "Scout warned away from village.");
          }
        } else {
          /* Blocked only — RNG kill-with-flee-tile stays PARKED (above). */
          units_despawn(ctx->units, foe);
          if (ai_contact_euro_is_human(ctx, e)) {
            ai_contact_set_status(ctx, "Natives kill your Scout.");
          }
        }
      }
    }
  }
}
