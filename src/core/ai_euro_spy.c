/*
 * mods.scout_spying (docs/settings.md "Mods"): computer Scouts infiltrate the
 * human's colonies. DOS ships the capture text and logic (FUN_5f7a_000e raw
 * 98809-98881, @LOSTTHEIRSCOUTS) but hard-wires a computer Scout to "Nothing"
 * (local_8 = 3, raw 98829). With the mod on:
 *   - every 10 turns per human colony (staggered by slot), 20% chance an AI
 *     Scout on its continent is sent there (plan 'S', goal course);
 *   - arrival rolls the DOS threshold: caught = @LOSTTHEIRSCOUTS, scout lost,
 *     +100 horses; success = the AI knows the colony's true strength for
 *     10 turns;
 *   - without that intel the Soldier/Dragoon colony mass gate sees the
 *     strength within +-50% (one guess per nation, colony and turn);
 *   - a scouted colony weaker than the AI's land army on its continent
 *     becomes a prio-8 MILITARY goal.
 * Mod off: every hook returns the DOS value untouched. Rolls are hashed from
 * the campaign position and never draw from the DOS RNG.
 */
#include "core/internal.h"
#include "core/ai_euro.h"
#include "core/ai_euro_internal.h"
#include "core/ai_goals.h"
#include "core/ai_popup.h"
#include "core/colony.h"
#include "core/combat_strength.h"
#include "core/map.h"
#include "core/mods.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/units.h"

#include <string.h>

#define SPY_INTEL_TURNS 10
#define SPY_DISPATCH_PERIOD 10
#define SPY_DISPATCH_PCT 20

typedef struct {
  uint8_t valid, x, y;
  int16_t strength; /* ai_euro_20e6_stack_combat_0b at infiltration */
  uint16_t turn;
} SpyIntel;

static SpyIntel s_intel[4][COLONIZE_COLONIES_MAX];

void ai_euro_spy_reset(void) {
  memset(s_intel, 0, sizeof(s_intel));
}

static uint32_t spy_hash(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
  uint32_t h = a * 0x9E3779B1u ^ b * 0x85EBCA77u ^ c * 0xC2B2AE3Du ^ d * 0x27D4EB2Fu;
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  h *= 0x297A2D39u;
  h ^= h >> 15;
  return h;
}

static int spy_turn(const ColonizeTurnContext* ctx) {
  return ctx->col1_ok && ctx->col1 ? (int)ctx->col1->head.turn : 0;
}

/* The mod spies only before the War of Independence (head game_options.woi,
 * the bit ai_euro_20e6_prologue reads into Ai20e6Unit.woi). */
static bool spy_on(const ColonizeTurnContext* ctx) {
  return mods_on(MOD_SCOUT_SPYING) &&
         !(ctx && ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi);
}

/* A spy Scout that can no longer spy (war declared, no path to its colony)
 * drops 'S' and goes back to the normal AI. Mod off: a no-op. */
void ai_euro_spy_drop(ColonizeUnit* u) {
  if (mods_on(MOD_SCOUT_SPYING) && u && u->col1_ai_plan == AI_EURO_SPY_PLAN) {
    u->col1_ai_plan = 0;
  }
}

/* Pool slot (not colony id) of the active colony on (x, y), or -1. */
static int spy_slot_at(const ColonizeColonyPool* pool, int x, int y) {
  for (int i = 0; pool && i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].x == x && pool->colonies[i].y == y) {
      return i;
    }
  }
  return -1;
}

/* The human colony at pool slot cid, or NULL (also NULL with the mod off). */
static ColonizeColony* spy_human_colony(ColonizeTurnContext* ctx, int nation, int cid) {
  if (!spy_on(ctx) || !ctx->colonies || cid < 0 || cid >= COLONIZE_COLONIES_MAX ||
      nation < 0 || nation > 3 || ctx->human_nation < 0 || ctx->human_nation > 3 ||
      nation == ctx->human_nation) {
    return NULL;
  }
  ColonizeColony* c = &ctx->colonies->colonies[cid];
  return c->active && c->nation_id == ctx->human_nation ? c : NULL;
}

static const SpyIntel* spy_fresh(int nation, int cid, const ColonizeColony* c, int turn) {
  const SpyIntel* in = &s_intel[nation][cid];
  if (!in->valid || in->x != c->x || in->y != c->y || turn < in->turn ||
      turn - in->turn >= SPY_INTEL_TURNS) {
    return NULL;
  }
  return in;
}

/* FUN_5f7a_000e raw 98839-98845 caught threshold before the human-mover
 * difficulty term: (Stockade/Fort/Fortress count + 6) * 2, halved for a
 * Seasoned Scout (@JOB 0x16). DOS adds difficulty - 2 only when the acting
 * nation is human (raw 98851-98853), so a computer Scout skips it. */
static int spy_threshold(const ColonizeColonyPool* pool, const ColonizeColony* c, bool seasoned) {
  int fort_chain = 0;
  for (int f = 0; f < 3; ++f) {
    const int bi = colonies_building_row(pool, (ColonizeBuildingRow)(COLONY_BUILDING_STOCKADE + f));
    if (bi >= 0 && bi < COLONIZE_BUILDING_TYPES_MAX && c->has_building[bi]) {
      fort_chain++;
    }
  }
  const int threshold = (fort_chain + 6) * 2;
  return seasoned ? threshold >> 1 : threshold;
}

bool ai_euro_spy_keeps_course(const ColonizeTurnContext* ctx, const ColonizeUnit* u) {
  return u->col1_ai_plan == AI_EURO_SPY_PLAN && spy_on(ctx);
}

int ai_euro_spy_dispatch_arm(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s) {
  if (!mods_on(MOD_SCOUT_SPYING)) {
    return 0;
  }
  if (!spy_on(ctx) || s->woi) {
    ai_euro_spy_drop(u); /* dispatched before the war: back to the normal AI */
    return 0;
  }
  if (s->dos_type != UNITS_KIND_SCOUT || !ctx->colonies || !ctx->map) {
    return 0;
  }
  if (u->col1_ai_plan == AI_EURO_SPY_PLAN) {
    /* Keep the course while its colony is still the human's. */
    const ColonizeColony* c =
      spy_human_colony(ctx, s->nation, spy_slot_at(ctx->colonies, u->goto_x, u->goto_y));
    if (c) {
      ai_euro_set_goto(u, AI_EURO_ACT_GOAL, c->x, c->y);
      return 1;
    }
    u->col1_ai_plan = 0;
  }
  const int turn = spy_turn(ctx);
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = spy_human_colony(ctx, s->nation, i);
    if (!c || (turn + i) % SPY_DISPATCH_PERIOD != 0 ||
        map_continent_id_at(ctx->map, c->x, c->y) != s->cid ||
        spy_hash((uint32_t)turn, (uint32_t)s->nation, (uint32_t)i, 0) % 100 >= SPY_DISPATCH_PCT ||
        spy_fresh(s->nation, i, c, turn)) {
      continue;
    }
    bool sent = false; /* one Scout per colony */
    for (int k = 0; k < units_slot_end(ctx->units) && !sent; ++k) {
      const ColonizeUnit* o = &ctx->units->units[k];
      sent = o->active && o->id != u->id && o->nation_id == s->nation &&
             o->col1_ai_plan == AI_EURO_SPY_PLAN && o->goto_x == c->x && o->goto_y == c->y;
    }
    if (sent) {
      continue;
    }
    u->col1_ai_plan = AI_EURO_SPY_PLAN;
    ai_euro_set_goto(u, AI_EURO_ACT_GOAL, c->x, c->y);
    return 1;
  }
  return 0;
}

bool ai_euro_spy_try_infiltrate(ColonizeTurnContext* ctx, ColonizeUnit* u, int tx, int ty) {
  if (!mods_on(MOD_SCOUT_SPYING) || u->col1_ai_plan != AI_EURO_SPY_PLAN || !ctx->colonies) {
    return false;
  }
  if (!spy_on(ctx)) {
    /* War declared on the way: no attempt, the step ends here and the Scout
     * returns to the normal AI. */
    ai_euro_spy_drop(u);
    return true;
  }
  const int cid = spy_slot_at(ctx->colonies, tx, ty);
  ColonizeColony* c = spy_human_colony(ctx, u->nation_id, cid);
  if (!c) {
    return false;
  }
  const int nation = u->nation_id;
  const int turn = spy_turn(ctx);
  u->col1_ai_plan = 0;
  u->moves = 0; /* the attempt ends the move, as 000e's callers do */
  /* 000e: RNG(1, 0x24) above the threshold infiltrates (raw 98855). */
  const int roll =
    1 + (int)(spy_hash((uint32_t)turn, (uint32_t)nation, (uint32_t)cid, (uint32_t)u->id + 2u) % 36u);
  if (spy_threshold(ctx->colonies, c, u->profession == UNITS_JOB_SCOUT) < roll) {
    SpyIntel* in = &s_intel[nation][cid];
    const int str = ai_euro_20e6_stack_combat_0b(ctx, tx, ty);
    in->valid = 1;
    in->x = (uint8_t)c->x;
    in->y = (uint8_t)c->y;
    in->strength = (int16_t)(str > 32767 ? 32767 : str);
    in->turn = (uint16_t)turn;
    return true;
  }
  /* Caught: 000e raw 98863-98877, @LOSTTHEIRSCOUTS (%STRING0 the scouts'
   * nation, %STRING1 the colony), +100 horses (+0xaa), scout gone. */
  if (ctx->messages && ctx->ai_popups) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = reports_nation_adjective_woi(ctx->col1_ok ? ctx->col1 : NULL, nation);
    tok.string1 = c->name;
    tok.has_number0 = true;
    tok.number0 = 100;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "LOSTTHEIRSCOUTS", &tok, "", body, sizeof(body));
    if (body[0]) {
      (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_INFO, c->nation_id, nation, 100, NULL, body);
    }
  }
  c->stock[COLONIZE_CARGO_HORSES] += 100;
  units_despawn(ctx->units, u->id);
  return true;
}

int ai_euro_spy_seen_strength(ColonizeTurnContext* ctx, int nation, int x, int y, int def) {
  if (!mods_on(MOD_SCOUT_SPYING)) {
    return def;
  }
  const int cid = spy_slot_at(ctx->colonies, x, y);
  const ColonizeColony* c = spy_human_colony(ctx, nation, cid);
  if (!c) {
    return def;
  }
  const int turn = spy_turn(ctx);
  const SpyIntel* in = spy_fresh(nation, cid, c, turn);
  if (in) {
    return in->strength;
  }
  return def * (50 + (int)(spy_hash((uint32_t)turn, (uint32_t)nation, (uint32_t)cid, 1) % 101u)) / 100;
}

void ai_euro_spy_prime_target(ColonizeTurnContext* ctx, int nation, int cid, int cont) {
  const ColonizeColony* c = spy_human_colony(ctx, nation, cid);
  const SpyIntel* in = c ? spy_fresh(nation, cid, c, spy_turn(ctx)) : NULL;
  if (!in || !ctx->units || !ctx->map) {
    return;
  }
  /* FUN_157e_004a mode 1 over own land units on the continent. */
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  int army = 0;
  for (int k = 0; k < units_slot_end(ctx->units); ++k) {
    const ColonizeUnit* o = &ctx->units->units[k];
    if (o->active && o->nation_id == nation && o->aboard_ship_id < 0 && !units_is_sea(ctx->units, o->id) &&
        map_continent_id_at(ctx->map, o->x, o->y) == cont) {
      army += combat_unit_base_x8(&sctx, o->id, 1, NULL);
    }
  }
  if (in->strength < army) {
    ai_goals_upsert_primary(nation, c->x, c->y, AI_GOAL_MILITARY, 8);
  }
}

size_t ai_euro_spy_export(uint8_t out[AI_EURO_SPY_EXPORT_MAX]) {
  size_t n = 0;
  for (int nation = 0; nation < 4; ++nation) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const SpyIntel* in = &s_intel[nation][i];
      if (!in->valid) {
        continue;
      }
      uint8_t* p = out + n;
      p[0] = (uint8_t)nation;
      p[1] = in->x;
      p[2] = in->y;
      p[3] = 0;
      p[4] = (uint8_t)((uint16_t)in->strength & 0xff);
      p[5] = (uint8_t)((uint16_t)in->strength >> 8);
      p[6] = (uint8_t)(in->turn & 0xff);
      p[7] = (uint8_t)(in->turn >> 8);
      n += 8;
    }
  }
  return n;
}

void ai_euro_spy_import(const uint8_t* in, size_t len, const ColonizeColonyPool* colonies) {
  ai_euro_spy_reset();
  if (!colonies) {
    return;
  }
  /* Keyed by tile: a load compacts colony slots. */
  for (size_t k = 0; k + 8 <= len; k += 8) {
    const uint8_t* p = in + k;
    const int cid = spy_slot_at(colonies, p[1], p[2]);
    if (p[0] > 3 || cid < 0) {
      continue;
    }
    SpyIntel* e = &s_intel[p[0]][cid];
    e->valid = 1;
    e->x = p[1];
    e->y = p[2];
    e->strength = (int16_t)(uint16_t)(p[4] | p[5] << 8);
    e->turn = (uint16_t)(p[6] | p[7] << 8);
  }
}
