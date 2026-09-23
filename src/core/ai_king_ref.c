/*
 * King / REF AI — REF landing site selection, garrison scoring, invasion, landing/announce/disembark
 *
 * Split out of ai_king.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_king_internal.h. See ai_king.c for the module prologue and the
 * crown/boycott, tea-party and REF-bookkeeping helpers.
 *
 * Sections:
 *   REF landing site selection (ai_king_10f0_pick_colony .. ai_king_weakest_port)
 *   REF garrison scoring & invasion mechanics (ai_king_0982_garrison_score .. ai_king_ref_wave)
 *   REF landing, announce & disembark (ai_king_10f0_spawn_unit .. ai_king_spend_woi_bell_pool)
 */

#include "core/internal.h"
#include "core/ai_king.h"
#include "core/ai_king_internal.h"
#include "core/ai_diplo.h"
#include "core/sound.h"

#include "core/assets.h"
#include "core/colony.h"
#include "core/combat_strength.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/strutil.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * FUN_43f7_10f0 raw 74312-74331: pop-weighted coastal colony roulette.
 * The gather loop keeps at most TEN candidates (`local_24 < 10`, a fixed
 * 10-byte stack array) and weights each by the RAW population byte +0x1f —
 * no floor of 1, so a 0-pop port carries no weight. The roulette then walks
 * that candidate array, not the colony list, so past ten human ports the
 * eleventh onward can never be picked.
 */
#define AI_KING_10F0_CANDIDATES 10

/* ===== REF landing site selection (ai_king_10f0_pick_colony .. ai_king_weakest_port) ===== */
static int ai_king_10f0_pick_colony(const ColonizeTurnContext* ctx, int human, int* out_x,
                                    int* out_y) {
  if (!ctx || !out_x || !out_y || human < 0 || human >= 4) {
    return -1;
  }
  int total_pop = 0;
  int best_i = -1;
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    int cand[AI_KING_10F0_CANDIDATES];
    int weight[AI_KING_10F0_CANDIDATES];
    int n_cand = 0;
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != human || !c->flags.coastal) {
        continue;
      }
      if (n_cand >= AI_KING_10F0_CANDIDATES) {
        break; /* DOS keeps scanning but the `local_24 < 10` arm never takes */
      }
      const int pop = (int)c->population;
      cand[n_cand] = (int)i;
      weight[n_cand] = pop;
      total_pop += pop;
      n_cand++;
    }
    if (n_cand > 0 && total_pop > 0) {
      int pick = total_pop / 2 + 1;
      if (ctx->rng) {
        pick = dos_rng_range(ctx->rng, 1, total_pop);
      }
      for (int k = 0; k < n_cand; ++k) {
        pick -= weight[k];
        if (pick <= 0) {
          const ColonizeCol1Colony* c = &ctx->col1->colony[cand[k]];
          *out_x = (int)c->x;
          *out_y = (int)c->y;
          return cand[k];
        }
      }
    }
    /*
     * Weightless candidate set (every one of the first ten ports at pop 0 —
     * unreachable in a real save): DOS's roulette leaves local_56 at -1 and
     * 10f0 lands nothing. Fall through to the caller's weakest-port fallback
     * instead of stalling the whole intervention.
     */
  }
  if (ctx->colonies) {
    int best_score = 999999;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (ctx->map && !map_tile_is_coastal(ctx->map, c->x, c->y)) {
        continue;
      }
      const int pop = c->population > 0 ? c->population : 1;
      if (pop < best_score) {
        best_score = pop;
        best_i = i;
        *out_x = c->x;
        *out_y = c->y;
      }
    }
  }
  return best_i;
}

/*
 * FUN_43f7_10f0 74339–74377: Man-O-War spawn tile = the 8-neighbour of the
 * colony that is WATER (`281f_0768` = `13e4_0074`, terrain 0x19/0x1a) with
 * no unit on it or only the human's (`281f_0682` < 0 || == human), no REF
 * Man-O-War (−999), scored 1 + the number of ITS neighbours that are land
 * on the colony's continent (`0722` == colony's) without a colony (`06be`
 * < 0). DOS also wants `281f_06b4`(tile) == 1 — the layer3 low nibble, i.e.
 * the sea region the open ocean carries; test maps don't fill layer3, so a
 * region-1 tile is preferred but not required.
 */
static int ai_king_10f0_score_tile(const ColonizeTurnContext* ctx, int human, int cx, int cy,
                                   int tx, int ty) {
  static const int k_raster8_dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
  static const int k_raster8_dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  if (!ctx || !ctx->map || !map_coords_inset(ctx->map, tx, ty) ||
      !map_tile_is_water(ctx->map, tx, ty)) {
    return -1;
  }
  /* bugs.md: the ally sails a Man-O-War in — a landlocked lake tile is
   * unreachable from the ocean, yet pass 2 of the picker accepted any water
   * ("MoW in the lake deposited troops into the lake colony"). */
  if (map_tile_is_lake(ctx->map, tx, ty)) {
    return -1;
  }
  int score = 1;
  /*
   * DOS runs TWO different ownership questions here, in this order
   * (viceroy_unpacked.c:74350-74364), and the port used to collapse them
   * into one human-only test — inverting the penalty (audit D11):
   *   1. `local_34 = 07e0(tile)` (first unit of the stack); if the stack's
   *      owner nibble equals the CROWN (`*(byte*)0x53d2`), walk the stack
   *      with 02e4 and subtract 999 per Man-O-War (type 0x12). A crown MoW
   *      parked on the tile is what DOS is refusing to land beside.
   *   2. `local_4 = 0682(tile)` (unit-presence owner, map.c:1187): the tile
   *      must be empty of units or hold only the HUMAN's.
   * The human's own Man-O-War therefore costs nothing in DOS; the old code
   * charged it −999 and rejected the tile, while a crown stack was rejected
   * by the wrong test and never reached the penalty at all.
   */
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  int tile_owner = -1; /* 281f_0682 */
  for (int i = 0; i < COLONIZE_UNITS_MAX && ctx->units; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->x != tx || u->y != ty || u->aboard_ship_id >= 0) {
      continue;
    }
    if (tile_owner < 0) {
      tile_owner = u->nation_id;
    }
    if (crown >= 0 && u->nation_id == crown && ai_king_is_mow(ctx->units, u)) {
      score -= 999;
    }
  }
  if (score < 0) {
    return score;
  }
  if (tile_owner >= 0 && tile_owner != human) {
    return -1;
  }
  const int colony_region = map_continent_id_at(ctx->map, cx, cy);
  for (int d = 0; d < 8; ++d) {
    const int nx = tx + k_raster8_dx[d];
    const int ny = ty + k_raster8_dy[d];
    if (!map_coords_inset(ctx->map, nx, ny) || !map_tile_is_land(ctx->map, nx, ny)) {
      continue;
    }
    if (map_continent_id_at(ctx->map, nx, ny) != colony_region) {
      continue;
    }
    if (ctx->colonies && colonies_id_at(ctx->colonies, nx, ny) >= 0) {
      continue;
    }
    score++;
  }
  return score;
}

static bool ai_king_10f0_pick_spawn(const ColonizeTurnContext* ctx, int human, int cx, int cy,
                                    int* out_x, int* out_y) {
  static const int k_raster8_dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
  static const int k_raster8_dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  if (!ctx || !out_x || !out_y) {
    return false;
  }
  int best = 0;
  int bx = -1;
  int by = -1;
  /*
   * DOS-LITERAL FUN_43f7_10f0 raw 74341-74347: ONE pass over the 8
   * neighbours, `0768(tile) != 0` (water) AND `06b4(tile) == 1` — the raw
   * layer3 low nibble, the open-sea region id. The port's two-pass relaxation
   * was dead in pass 0 (map_continent_id_at returns -1 on water) and accepted
   * any non-lake water in pass 1, including a Sea Lane tile with nibble != 1
   * that DOS rejects (bugs.md #873c).
   */
  for (int d = 0; d < 8; ++d) {
    const int tx = cx + k_raster8_dx[d];
    const int ty = cy + k_raster8_dy[d];
    if (ctx->map && (int)(map_get_layer3(ctx->map, tx, ty) & 0x0fu) != 1) {
      continue;
    }
    const int sc = ai_king_10f0_score_tile(ctx, human, cx, cy, tx, ty);
    if (sc > best) {
      best = sc;
      bx = tx;
      by = ty;
    }
  }
  if (bx < 0) {
    return false;
  }
  *out_x = bx;
  *out_y = by;
  return true;
}

/* FUN_43f7_060a-shaped: weakest garrison (pop × fort). */
int ai_king_weakest_port(ColonizeTurnContext* ctx, int nation_id, int* out_x, int* out_y) {
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
    /* Prefer coastal ports when garrison pressure is close (REF landing sites). */
    if (ctx->map && map_tile_is_coastal(ctx->map, c->x, c->y)) {
      garrison = (garrison * 9) / 10;
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
 * FUN_43f7_060a: colony garrison score for the REF landing pick.
 *   (muskets + 50) / 100 + 1, + Σ land units on the tile (004a attack ×8 >> 4),
 *   ×2 with a Fortress, ×1.5 with a Fort, min 1.
 */
/* ===== REF garrison scoring & invasion mechanics (ai_king_0982_garrison_score .. ai_king_ref_wave) ===== */
COLONIZE_INTERNAL int ai_king_0982_garrison_score(const ColonizeTurnContext* ctx, const ColonizeColony* c) {
  int g = (c->stock[COLONIZE_CARGO_MUSKETS] + 50) / 100 + 1;
  const ColonizeCombatStrengthCtx cs = combat_strength_ctx_from_turn(ctx);
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->x != c->x || u->y != c->y || !units_is_on_map(u) ||
        units_is_sea(ctx->units, u->id)) {
      continue;
    }
    g += combat_unit_base_x8(&cs, u->id, 1, NULL) >> 4;
  }
  const int fortress = colonies_building_row(ctx->colonies, COLONY_BUILDING_FORTRESS);
  const int fort = colonies_building_row(ctx->colonies, COLONY_BUILDING_FORT);
  if (fortress >= 0 && c->has_building[fortress]) {
    g <<= 1;
  } else if (fort >= 0 && c->has_building[fort]) {
    g = (g * 3) >> 1;
  }
  return g < 1 ? 1 : g;
}

/* 08bc stack query stand-in: Σ defense (004a mode 0 ×8 >> 4) of units at (x,y). */
COLONIZE_INTERNAL int ai_king_0982_tile_strength(const ColonizeTurnContext* ctx, int x, int y) {
  const ColonizeCombatStrengthCtx cs = combat_strength_ctx_from_turn(ctx);
  int s = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (u->active && u->x == x && u->y == y && units_is_on_map(u)) {
      s += combat_unit_base_x8(&cs, u->id, 0, NULL) >> 4;
    }
  }
  return s;
}

/*
 * FUN_43f7_0512: purge every non-crown unit at (x,y). Human units get the
 * @SEIZURELAND / @SEIZURESEA notice (%STRING0 = unit type name).
 */
COLONIZE_INTERNAL void ai_king_0982_purge_tile(ColonizeTurnContext* ctx, int crown, int x, int y) {
  for (int i = COLONIZE_UNITS_MAX - 1; i >= 0; --i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->x != x || u->y != y || !units_is_on_map(u) || u->nation_id == crown) {
      continue;
    }
    if (u->nation_id == ctx->human_nation && ai_king_human_popups(ctx)) {
      const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
      const bool water = map_tile_is_water(ctx->map, x, y);
      const bool sea = units_is_sea(ctx->units, u->id);
      /* DOS-LITERAL FUN_43f7_0512 raw 73746-73755: tile-land -> @SEIZURELAND
       * always; tile-water -> @SEIZURESEA only for a hull (type 0xd..0x12),
       * else no popup at all. Keyed on the TILE, not the unit's domain. */
      if (!water || sea) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = t ? t->name : "unit";
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          ctx->messages, water ? "SEIZURESEA" : "SEIZURELAND", &tok,
          "", body, sizeof(body)
        );
        (void)ai_popup_enqueue_ok_ctx(
          ctx->ai_popups, AI_POPUP_TAG_INFO, ctx->human_nation, crown, 0, NULL, body
        );
      }
    }
    if (units_is_sea(ctx->units, u->id)) {
      (void)units_despawn_ship_with_cargo(
        ctx->units, u->id, NULL, NULL, 0, NULL, NULL, 0, NULL, NULL, 0
      );
    } else {
      (void)units_despawn(ctx->units, u->id);
    }
  }
}

COLONIZE_INTERNAL int ai_king_0982_crown_mow_alive(const ColonizeTurnContext* ctx, int crown) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (u->active && u->nation_id == crown && ai_king_is_mow(ctx->units, u)) {
      n++;
    }
  }
  return n;
}

/* 0982 pool index → NAMES type (43f7_0082 crown class map). */
COLONIZE_INTERNAL int ai_king_0982_spawn_pool_unit(ColonizeTurnContext* ctx, int crown, int k, int x, int y) {
  /* bugs.md: the REF fields Regulars and CAVALRY (@UNIT 8) — not colonial
   * Dragoons; pool[1] is the Cavalry pool. */
  /* @UNIT rows, not names: 6 Regulars / 8 Cavalry / 18 Man-O-War / 11
   * Artillery, with the colonial stand-ins for a pool that lacks a row. */
  static const ColonizeUnitKind kinds[4] = {
    UNITS_KIND_REGULAR, UNITS_KIND_CAVALRY, UNITS_KIND_MAN_O_WAR, UNITS_KIND_ARTILLERY
  };
  static const ColonizeUnitKind alts[4] = {
    UNITS_KIND_SOLDIER, UNITS_KIND_DRAGOON, UNITS_KIND_GALLEON, UNITS_KIND_ARTILLERY
  };
  int ty = units_kind_type_index(ctx->units, kinds[k]);
  if (ty < 0) {
    ty = units_kind_type_index(ctx->units, alts[k]);
  }
  if (ty < 0) {
    return -1;
  }
  const int uid = units_spawn_allow_stack(ctx->units, ty, x, y);
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (!u) {
    return -1;
  }
  units_set_nation(u, crown);
  /* DOS creator FUN_1427_06b4 stamps orders +0x314b = 0x58 (none) and no
   * goto; 0982 never overrides it (raw 74202-74222). The landed unit must
   * arrive order-free so the euro act (5b66/20e6) decides its move next
   * turn — the old AI_MOVE+goto=self stamp read as a committed goto to its
   * own tile and froze the wave once the king-side hunt was retired (D1). */
  /* 0982: the landed unit's moves are spent this beat (02d0 animate + 0948). */
  u->moves = 0;
  /* bugs.md: landings are in plain sight — stamp watcher vis bits so the
   * wave draws immediately instead of after its first move. */
  if (ctx->map) {
    u->col1_vis_mask |= units_vis_mask_for_tile(ctx->map, x, y, crown);
  }
  return uid;
}

/* DS:0x5333 = @UNIT row 18 (Man-O-War) + 5 = the HOLDS column (0x5232 +
 * 18*0xe + 5), i.e. 6 with stock NAMES.TXT: one MoW load per wave. Both
 * 0982 clamps (raw 74077-74081 pass>=1, raw 74091-74094 after the pick)
 * read it. User-observed: REF waves are never larger than 6 (bugs.md #661). */
static int ai_king_0982_max_landing(const ColonizeTurnContext* ctx) {
  const int ti = units_kind_type_index(ctx->units, UNITS_KIND_MAN_O_WAR);
  if (ti < 0 || ti >= ctx->units->type_count) {
    return 6;
  }
  const int cargo = ctx->units->types[ti].cargo;
  return cargo > 0 ? cargo : 6;
}
#define AI_KING_0982_MAX_TARGETS 10

/*
 * FUN_43f7_2022 crown branch: pools>0 → FUN_43f7_0982 invasion wave, else
 * FUN_43f7_06a6 irregulars. 0982 (viceroy_unpacked.c 73935-74266):
 *   - MoW pool (force[2]) empty → +1 only while the crown has no Man-O-War
 *     alive, and no landing this turn.
 *   - exhaust = total < 5 || total == force[2] → every pool wiped at the end.
 *   - human coastal colonies scored colonists×(125−SoL) − 75×tile strength
 *     (min 100−SoL), weakest first; garrison need = 060a − attack-capable
 *     crown units already adjacent. Three relaxing passes pick the first
 *     colony the pools can cover (Dragoon/Artillery each capped at
 *     max(1, need>>3), or 1 when Regulars ≥ Dragoons+Artillery); passes ≥1
 *     cap need at DS:0x5333 = MoW holds column (6).
 *   - landing water tile = the colony neighbour with the most free land
 *     neighbours on the colony's continent (a human ship stack there counts
 *     as 1). Non-crown units on it are seized (0512), the MoW spawns there
 *     (@INVASION), then max(3, need) land units land on the weakest adjacent
 *     land tiles (Chebyshev 1 from the colony, same continent, no village),
 *     seizing whatever stands there — Dragoons first up to the cap (2 max
 *     when Regulars > 1), then Artillery, then Regulars.
 * Thin: 08bc stack strength = Σ defense×8>>4.
 * The emptied-Man-O-War return home is no longer stood in for here: the real
 * DOS beat is the FUN_521d_20e6 ship-band tail, ported as
 * ai_king_mow_sail_home_20e6 and run from the MoW's own act in war_act. The
 * MoW pool regrows through this function's own opening gate below
 * (force[2] == 0 && no crown MoW on the map → force[2]++), exactly as DOS
 * does at raw 73990-73993.
 */
/*
 * FUN_43f7_06a6 Tory uprising (viceroy_unpacked.c 73829-73932) — the crown
 * fallback once the land pools are gone. Extracted verbatim from
 * ai_king_ref_wave.
 */
static void ai_king_ref_tory_uprising(ColonizeTurnContext* ctx, int crown, int human) {
  /*
   * bugs.md #255 — full FUN_43f7_06a6 Tory uprising (viceroy_unpacked.c
   * 73829-73932), replacing the old one-Regular-at-(hx,hy+1) stand-in
   * that could drop Regulars on a WATER tile:
   *   - roll(0, difficulty+1) != 0 to fire at all;
   *   - per human colony without the +0x1c bit1 latch (flags.ref_landing):
   *     score = pop*(100-SoL)*2/100 + difficulty+1, minus the attack
   *     strength of every unit on the colony tile; a crown unit on any
   *     adjacent land tile, or no free adjacent LAND tile, disqualifies;
   *   - the max-score colony gets latched and score crown SOLDIERS (not
   *     Regulars) spawn round-robin on free adjacent land tiles — odd
   *     picks roll Veteran profession, every 3rd rolls into a Dragoon;
   *   - @TORYUPRISING popup with the colony name.
   */
  if (dos_rng_range(ctx->rng, 0, (int)ctx->col1->head.difficulty + 1) == 0) {
    return;
  }
  static const int dx8[8] = {-1, 0, 1, 1, 1, 0, -1, -1};
  static const int dy8[8] = {-1, -1, -1, 0, 1, 1, 1, 0};
  int best_ci = -1;
  int best_score = 0;
  for (uint16_t ci = 0; ci < ctx->col1->head.colony_count; ++ci) {
    ColonizeCol1Colony* c = ctx->col1->colony ? &ctx->col1->colony[ci] : NULL;
    if (!c || (int)c->nation_id != human || c->flags.ref_landing) {
      continue;
    }
    const int sol_p = ai_king_colony_sol_at(ctx, human, (int)c->x, (int)c->y);
    int score = ((int)c->population * (100 - sol_p) * 2) / 100 +
                (int)ctx->col1->head.difficulty + 1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (u->active && u->aboard_ship_id < 0 && u->x == (int)c->x && u->y == (int)c->y) {
        const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
        score -= ty ? ty->attack : 0;
      }
    }
    int free_land = 0;
    bool crown_adjacent = false;
    for (int e = 0; e < 8; ++e) {
      const int nx = (int)c->x + dx8[e];
      const int ny = (int)c->y + dy8[e];
      if (map_tile_is_water(ctx->map, nx, ny) ||
          map_tile_has_city(ctx->map, nx, ny)) {
        continue;
      }
      const int occ = units_id_at(ctx->units, nx, ny);
      const ColonizeUnit* ou = occ >= 0 ? units_get_const(ctx->units, occ) : NULL;
      if (!ou) {
        free_land++;
      } else if (ou->nation_id == crown) {
        crown_adjacent = true;
      }
    }
    if (crown_adjacent || free_land == 0) {
      continue;
    }
    if (score > best_score) {
      best_score = score;
      best_ci = (int)ci;
    }
  }
  if (best_ci < 0 || best_score <= 0) {
    return;
  }
  ColonizeCol1Colony* c = &ctx->col1->colony[best_ci];
  c->flags.ref_landing = 1;
  const int soldier_ty = units_kind_type_index(ctx->units, UNITS_KIND_SOLDIER);
  const int dragoon_ty = units_kind_type_index(ctx->units, UNITS_KIND_DRAGOON);
  int remaining = best_score;
  int spawned = 0;
  bool any_pass = true;
  while (remaining > 0 && any_pass) {
    any_pass = false;
    for (int e = 0; e < 8 && remaining > 0; ++e) {
      const int nx = (int)c->x + dx8[e];
      const int ny = (int)c->y + dy8[e];
      if (map_tile_is_water(ctx->map, nx, ny) ||
          map_tile_has_city(ctx->map, nx, ny)) {
        continue;
      }
      const int occ = units_id_at(ctx->units, nx, ny);
      const ColonizeUnit* ou = occ >= 0 ? units_get_const(ctx->units, occ) : NULL;
      if (ou && ou->nation_id != crown) {
        continue;
      }
      const int uid = soldier_ty >= 0
        ? units_spawn_allow_stack(ctx->units, soldier_ty, nx, ny)
        : -1;
      if (uid >= 0) {
        any_pass = true;
        spawned++;
        ColonizeUnit* nu = units_get(ctx->units, uid);
        if (nu) {
          units_set_nation(nu, crown);
          /* Order-free like the 0982 wave (DOS creator default 0x58) —
           * the euro act moves them from the next turn on (D1). */
          /* bugs.md follow-up to 406: uprising irregulars spawn with the
           * turn spent — the war-act loop runs this same beat and must not
           * march them into the colony the moment they appear (the crown
           * move pass ran before the king block in DOS). */
          nu->moves = 0;
          if ((remaining & 1) != 0 &&
              dos_rng_range(ctx->rng, 0, (int)ctx->col1->head.difficulty + 1) != 0) {
            nu->profession = UNITS_JOB_SOLDIER; /* Veteran */
          }
          if (remaining % 3 == 0 && dragoon_ty >= 0 &&
              dos_rng_range(ctx->rng, 0, (int)ctx->col1->head.difficulty + 1) != 0) {
            nu->type_index = dragoon_ty;
            nu->horses = UNITS_EQUIP_HORSES;
          }
          if (ctx->map) {
            nu->col1_vis_mask |= units_vis_mask_for_tile(ctx->map, nx, ny, crown);
          }
        }
      }
      remaining--;
    }
  }
  if (spawned == 0) {
    c->flags.ref_landing = 0;
    return;
  }
  if (ai_king_human_popups(ctx)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = c->name[0] ? c->name : "our colony";
    char body[AI_POPUP_BODY_LEN];
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(fallback, sizeof(fallback),
             "Tory uprising near %s! Loyalist irregulars take up arms for the King!",
             tok.string0);
    popup_msg_fill(ctx->messages, "TORYUPRISING", &tok, fallback, body, sizeof(body));
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, crown, spawned, NULL, body
    );
  }
}

/*
 * FUN_43f7_0982 land wave (raw 74150-74266): the disembark loop that walks
 * the Regular/Dragoon/Artillery pools ashore around the landing tile.
 * Extracted verbatim from ai_king_ref_wave.
 */
COLONIZE_INTERNAL void ai_king_0982_land_troops(
  ColonizeTurnContext* ctx, int crown, uint16_t* force, const ColonizeColony* c,
  int continent, int garrison_raw, int need, int lx, int ly
) {
  /* Land units: caps recomputed from the raw garrison (74150-74162). */
  int cap = garrison_raw >> 3;
  if (cap < 1) {
    cap = 1;
  }
  if (force[0] > 1 && cap > 2) {
    cap = 2;
  }
  if ((int)force[1] + (int)force[3] <= (int)force[0]) {
    cap = 1;
  }
  if (need < 3) {
    need = 3;
  }
  /* Per-wave cap = DS:0x5333 (MoW holds, 6) already applied by the caller
   * (raw 74091-74094); floor 3 above is raw 74162-74164. bugs.md #661. */
  int used_d = 0;
  int used_a = 0;
  /* Candidate land tiles around the ship, weakest stack first. */
  int cx[8];
  int cy[8];
  int cs[8];
  int nc = 0;
  for (int e = 0; e < 8; ++e) {
    const int nx = lx + MAP_DIR8_DX[e];
    const int ny = ly + MAP_DIR8_DY[e];
    if (map_tile_is_water(ctx->map, nx, ny) ||
        map_tile_has_city(ctx->map, nx, ny) ||
        (ctx->colonies && colonies_id_at(ctx->colonies, nx, ny) >= 0) ||
        abs(nx - c->x) > 1 || abs(ny - c->y) > 1 ||
        map_continent_id_at(ctx->map, nx, ny) != continent) {
      continue;
    }
    cx[nc] = nx;
    cy[nc] = ny;
    cs[nc] = ai_king_0982_tile_strength(ctx, nx, ny);
    nc++;
  }
  for (int a = 1; a < nc; ++a) {
    for (int b = a; b > 0 && cs[b] < cs[b - 1]; --b) {
      int t = cs[b]; cs[b] = cs[b - 1]; cs[b - 1] = t;
      t = cx[b]; cx[b] = cx[b - 1]; cx[b - 1] = t;
      t = cy[b]; cy[b] = cy[b - 1]; cy[b - 1] = t;
    }
  }
  /*
   * bugs.md (REF_bugs.SAV): if ANY candidate tile is empty, the
   * landing uses only the empty tiles — seizing the player's units
   * is the blockade-runner case, legal only when every adjacent
   * tile is held. Empty tiles sort first anyway (strength 0), so
   * restrict "usable" to them when one exists.
   */
  /*
   * bugs.md: "safe" = no HUMAN stack on the tile — empty, or held
   * by an earlier crown landing (stacking with its own army is
   * fine; the previous fix treated the old beachhead as occupied
   * and pushed the next wave onto the player's units). Partition:
   * when ANY safe tile exists, land ONLY on safe tiles; the
   * seize-what-stands-there landing remains solely for a full
   * blockade.
   */
  int usable = 0;
  {
    int sx2[8];
    int sy2[8];
    int ss2[8];
    int ns = 0;
    for (int t = 0; t < nc; ++t) {
      const int occ = units_id_at(ctx->units, cx[t], cy[t]);
      const ColonizeUnit* ou = occ >= 0 ? units_get_const(ctx->units, occ) : NULL;
      if (!ou || ou->nation_id == crown) {
        sx2[ns] = cx[t];
        sy2[ns] = cy[t];
        ss2[ns] = cs[t];
        ns++;
      }
    }
    if (ns > 0) {
      for (int t = 0; t < ns; ++t) {
        cx[t] = sx2[t];
        cy[t] = sy2[t];
        cs[t] = ss2[t];
      }
      usable = ns;
    } else {
      /* Full blockade: DOS lands on every tile no stronger than the
       * weakest, seizing what stands there. */
      while (usable < nc && cs[usable] <= cs[0]) {
        usable++;
      }
      for (int t = 0; t < usable; ++t) {
        ai_king_0982_purge_tile(ctx, crown, cx[t], cy[t]);
      }
    }
  }
  int slot = 0;
  while (need > 0 && usable > 0) {
    int k;
    if (used_d < cap && force[1] > 0) {
      k = 1;
      used_d++;
    } else if (used_a < cap && force[3] > 0) {
      k = 3;
      used_a++;
    } else if (force[0] > 0) {
      k = 0;
    } else {
      break;
    }
    /* bugs.md: show the troops DISEMBARKING — spawn on the ship's
     * tile and step ashore through units_try_move, which fires the
     * move-watch slide, so the player can see what landed. Fall
     * back to a direct beach spawn if the step is refused. */
    const int uid = ai_king_0982_spawn_pool_unit(ctx, crown, k, lx, ly);
    if (uid < 0) {
      break;
    }
    {
      /* One step's worth of MP for the walk ashore (spawn parks at 0). */
      ColonizeUnit* lu = units_get(ctx->units, uid);
      if (lu) {
        lu->moves = 3;
        lu->goto_x = cx[slot];
        lu->goto_y = cy[slot];
      }
    }
    ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
    if (!units_try_move_w(&w_, uid, cx[slot], cy[slot])) {
      ColonizeUnit* lu = units_get(ctx->units, uid);
      if (lu) {
        const int sx0 = lu->x;
        const int sy0 = lu->y;
        lu->x = cx[slot];
        lu->y = cy[slot];
        units_occupancy_notify_moved(ctx->units, sx0, sy0, lu->x, lu->y);
      }
    }
    {
      ColonizeUnit* lu = units_get(ctx->units, uid);
      if (lu) {
        lu->moves = 0; /* landing consumes the turn */
        if (ctx->map) {
          lu->col1_vis_mask |=
            units_vis_mask_for_tile(ctx->map, lu->x, lu->y, crown);
        }
      }
    }
    /* DOS-LITERAL FUN_43f7_0982 raw 74217-74219:
     * `FUN_281f_09ba(colony.x - 2, colony.y - 2, 5, 5, 1)` — the 5x5 box is
     * centred on the TARGET COLONY, not on each landing tile (bugs.md #878b). */
    map_reveal_radius(ctx->map, c->x, c->y, crown, 2);
    force[k]--;
    need--;
    slot = (slot + 1) % usable;
  }
}

#include "core/ai_king_internal.h" /* struct ai_king_0982_ctx */

/*
 * FUN_43f7_0982 invasion wave: score human coastal colonies, pick a target
 * over three relaxing passes, seize/claim the landing water tile, spawn the
 * Man-O-War and land the troops. Extracted verbatim from ai_king_ref_wave.
 */
COLONIZE_INTERNAL void ai_king_0982_invasion(struct ai_king_0982_ctx* w) {
  ColonizeTurnContext* const ctx = w->ctx;
  const int crown = w->crown;
  uint16_t* const force = w->force;
  const int human = w->human;
  bool exhaust = w->exhaust;
  bool landed = w->landed;

  /* Score human coastal colonies (≤10); the list is sorted ASCENDING, and
   * the picker below walks it from the top (highest score = fattest, most
   * lightly held target). */
  int score[AI_KING_0982_MAX_TARGETS];
  int cidx[AI_KING_0982_MAX_TARGETS];
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX && n < AI_KING_0982_MAX_TARGETS; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    /* DOS-LITERAL FUN_43f7_0982 raw 74001-74003: `(colony[+0x1c] & 0x40)` —
     * the save-carried coastal bit stamped at founding, not the live
     * 8-neighbour probe (bugs.md #878c). */
    if ((c->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) == 0) {
      continue;
    }
    const int inv = 100 - ai_king_colony_sol_at(ctx, human, c->x, c->y);
    int sc = c->colonist_count * (inv + 25) - 75 * ai_king_0982_tile_strength(ctx, c->x, c->y);
    if (sc < inv) {
      sc = inv;
    }
    score[n] = sc;
    cidx[n] = i;
    n++;
  }
  for (int a = 1; a < n; ++a) {
    for (int b = a; b > 0 && score[b] < score[b - 1]; --b) {
      int t = score[b]; score[b] = score[b - 1]; score[b - 1] = t;
      t = cidx[b]; cidx[b] = cidx[b - 1]; cidx[b - 1] = t;
    }
  }
  int garrison[AI_KING_0982_MAX_TARGETS];
  for (int i = 0; i < n; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[cidx[i]];
    int g = ai_king_0982_garrison_score(ctx, c);
    for (int d = 0; d < 8; ++d) {
      const int nx = c->x + MAP_DIR8_DX[d];
      const int ny = c->y + MAP_DIR8_DY[d];
      if (map_tile_is_water(ctx->map, nx, ny)) {
        continue;
      }
      /*
       * DOS-LITERAL FUN_43f7_0982 raw 74033-74041: `local_4c = 07e0(tile)`
       * (FIRST unit of the stack), then the nation nibble of THAT unit alone
       * is compared with the crown (`0x53d2`); if it matches, 02e4 walks the
       * whole chain and every unit with a non-zero attack byte decrements the
       * need. The port used to re-test each unit's nation (bugs.md #878c).
       */
      const int top = units_id_at(ctx->units, nx, ny);
      const ColonizeUnit* tu = top >= 0 ? units_get_const(ctx->units, top) : NULL;
      if (!tu || tu->nation_id != crown) {
        continue;
      }
      for (int k = 0; k < COLONIZE_UNITS_MAX && g > 0; ++k) {
        const ColonizeUnit* u = &ctx->units->units[k];
        if (!u->active || u->x != nx || u->y != ny || !units_is_on_map(u)) {
          continue;
        }
        const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
        if (t && t->attack > 0) {
          g--;
        }
      }
    }
    garrison[i] = g;
  }
  /*
   * Pick: three relaxing passes, each walking the ascending list BACKWARDS
   * (raw 74048-74056: `iVar4 = local_2a - local_48;
   * local_6a = local_42[iVar4 - 1]`, with local_48 zeroed at the head of
   * every pass and stepped only when a candidate is rejected — it is a
   * rejection counter, not a landing-wave cursor). So the HIGHEST score
   * goes first: many colonists, low SoL, thin garrison. Walking forwards
   * invaded the least attractive colony instead.
   */
  int pick = -1;
  int need = 0;
  const int max_landing = ai_king_0982_max_landing(ctx);
  for (int pass = 0; pass < 3 && pick < 0; ++pass) {
    for (int i = n - 1; i >= 0; --i) {
      int g = garrison[i] < 1 ? 1 : garrison[i];
      int cap = g >> 3;
      if (cap < 1) {
        cap = 1;
      }
      if ((int)force[1] + (int)force[3] <= (int)force[0]) {
        cap = 1;
      }
      const int cd = (int)force[1] < cap ? (int)force[1] : cap;
      const int ca = (int)force[3] < cap ? (int)force[3] : cap;
      if (pass != 0 && g > max_landing) {
        g = max_landing;
      }
      if (pass < 2 && (int)force[0] + cd + ca < g) {
        continue;
      }
      pick = i;
      need = g;
      break;
    }
  }
  if (pick >= 0) {
    if (need > max_landing) {
      need = max_landing;
    }
    const ColonizeColony* c = &ctx->colonies->colonies[cidx[pick]];
    const int continent = map_continent_id_at(ctx->map, c->x, c->y);
    /* Landing water tile: most free land neighbours on the colony continent. */
    int best = 0;
    int lx = -1;
    int ly = -1;
    for (int d = 0; d < 8; ++d) {
      const int wx = c->x + MAP_DIR8_DX[d];
      const int wy = c->y + MAP_DIR8_DY[d];
      if (!map_tile_is_water(ctx->map, wx, wy)) {
        continue;
      }
      int free_land = 0;
      for (int e = 0; e < 8; ++e) {
        const int nx = wx + MAP_DIR8_DX[e];
        const int ny = wy + MAP_DIR8_DY[e];
        if (map_tile_is_water(ctx->map, nx, ny)) {
          continue;
        }
        if (map_continent_id_at(ctx->map, nx, ny) != continent) {
          continue;
        }
        /* 06be tile_tribe_owner: settlement bit only, units do not block. */
        if (map_tile_has_city(ctx->map, nx, ny) ||
            (ctx->colonies && colonies_id_at(ctx->colonies, nx, ny) >= 0)) {
          continue;
        }
        free_land++;
      }
      if (free_land > 0) {
        /*
         * DOS-LITERAL FUN_43f7_0982 raw 74118-74127: `local_4c = 07e0(tile)`;
         * only when the FIRST stack unit's nation nibble differs from the
         * crown does 02e4 walk the chain, and only a type 0x12 (Man-O-War)
         * forces `local_16 = 1`. Any other foreign hull (a Caravel picket)
         * leaves the score alone (bugs.md #872).
         */
        const int occ = units_id_at(ctx->units, wx, wy);
        const ColonizeUnit* ou = occ >= 0 ? units_get_const(ctx->units, occ) : NULL;
        if (ou && ou->nation_id != crown) {
          for (int k = 0; k < COLONIZE_UNITS_MAX; ++k) {
            const ColonizeUnit* u = &ctx->units->units[k];
            if (!u->active || u->x != wx || u->y != wy || !units_is_on_map(u)) {
              continue;
            }
            if (ai_king_is_mow(ctx->units, u)) {
              free_land = 1; /* lowest priority */
              break;
            }
          }
        }
      }
      if (free_land > best) {
        best = free_land;
        lx = wx;
        ly = wy;
      }
    }
    if (best > 0) {
      ai_king_0982_purge_tile(ctx, crown, lx, ly);
      force[2]--;
      int ship_ty = units_kind_type_index(ctx->units, UNITS_KIND_MAN_O_WAR);
      if (ship_ty < 0) {
        ship_ty = units_kind_type_index(ctx->units, UNITS_KIND_GALLEON);
      }
      const int sid = ship_ty >= 0 ? units_spawn_allow_stack(ctx->units, ship_ty, lx, ly) : -1;
      ColonizeUnit* ship = units_get(ctx->units, sid);
      if (ship) {
        units_set_nation(ship, crown);
        /*
         * DOS-LITERAL FUN_43f7_0982 raw 74169-74180: the hull comes from
         * `095c(0x12, crown, x, y)` and nothing is stamped on it — the
         * creator default (+0x314b = 0x58, no orders) stands, exactly as
         * for the land units below. The port's `AI_SAIL` + self-goto made
         * ai_euro_act read it as a committed self-move and zero its MP, so
         * the hull froze on the landing tile forever (bugs.md #866).
         */
        ship->col1_counter16 = 0;
        /* bugs.md: the invasion fleet is in plain sight of the colony —
         * stamp watcher vis bits like a real move (the land units get
         * theirs in ai_king_0982_spawn_pool_unit). */
        if (ctx->map) {
          ship->col1_vis_mask |= units_vis_mask_for_tile(ctx->map, lx, ly, crown);
        }
        landed = true;
        exhaust = false;
        /* @INVASION (thin 1528 announce; VGA chrome PARKED). */
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = c->name[0] ? c->name : "your colony";
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(ctx->messages, "INVASION", &tok, "", body, sizeof(body));
        if (ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "%s", body);
        }
        if (ai_king_human_popups(ctx)) {
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, crown, 0, NULL, body
          );
          /* bugs.md #237: the landing popup BLOCKS before the disembark
           * slides — popup, then animations, then the rest, in sequence. */
          units_pump_combat_popups();
        }

        ai_king_0982_land_troops(
          ctx, crown, force, c, continent, garrison[pick], need, lx, ly
        );
      }
    }
  }

  w->exhaust = exhaust;
  w->landed = landed;
}

void ai_king_ref_wave(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->map) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  /* bugs.md: the wave waits one turn after the declaration. */
  if (ai_king_latch_get(ctx->col1, AI_KING_REF_WAVE_WAIT_BYTE) != 0) {
    ai_king_latch_set(ctx->col1, AI_KING_REF_WAVE_WAIT_BYTE, 0);
    return;
  }
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation);
  const int human = ctx->human_nation;
  uint16_t* force = ctx->col1->head.expeditionary_force;
  int total = (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];

  /*
   * FUN_43f7_2022 crown gate (viceroy_unpacked.c:74994): the wave (0982)
   * runs while `regulars + (cavalry>0) + (artillery>0) != 0` — the
   * Man-O-War pool alone does NOT sustain the invasion; once the land
   * pools are gone the crown falls back to 06a6 Tory uprisings even if
   * force[2] is nonzero (was `total <= 0`, which counted the MoW pool).
   */
  if ((int)force[0] + (force[1] > 0 ? 1 : 0) + (force[3] > 0 ? 1 : 0) == 0) {
    ai_king_ref_tory_uprising(ctx, crown, human);
    return;
  }

  /*
   * (2026-09-07) The "emptied Man-O-War sails home" stand-in that used to sit
   * here — despawn any idle empty crown MoW with col1_counter16 > 0, bump
   * col1_counter16 otherwise — is retired. The real DOS beat is the
   * FUN_521d_20e6 ship-band tail (raw 89717-89720), ported as
   * ai_king_mow_sail_home_20e6 and run from the MoW's own act in war_act; see
   * that function's header. It also frees col1_counter16, which DOS uses on AI
   * units as the 20e6 cower byte (+0x315a), from a king-side second meaning.
   */

  bool exhaust = false;
  bool landed = false;
  if (force[2] == 0) {
    if (ai_king_0982_crown_mow_alive(ctx, crown) == 0) {
      force[2]++;
    }
    return;
  }
  if (total < 5 || total == (int)force[2]) {
    exhaust = true;
  }
  if (total != (int)force[2] && ctx->colonies) {
    struct ai_king_0982_ctx w;
    memset(&w, 0, sizeof(w));
    w.ctx = ctx;
    w.crown = crown;
    w.human = human;
    w.force = force;
    w.total = total;
    w.exhaust = exhaust;
    w.landed = landed;
    ai_king_0982_invasion(&w);
    exhaust = w.exhaust;
    landed = w.landed;
  }
  if (landed) {
    ai_king_set_ref_present(ctx->col1, 1);
  }
  if (exhaust) {
    force[0] = 0;
    force[1] = 0;
    force[2] = 0;
    force[3] = 0;
  }
}

/*
 * FUN_43f7_10f0 land-troop loop (viceroy_unpacked.c:74417-74449) skips
 * pool index 2 (Man-O-War / 0x53e6) — that pool is spent by the single
 * Man-O-War spawn at :74378-74382 instead. The "naval type on a land tile"
 * puzzle of the earlier note is resolved: the scored tile is WATER
 * (`281f_0768` = `13e4_0074`, terrain 0x19/0x1a), so the ship placement is
 * plain — see ai_king_10f0_score_tile.
 */
/*
 * FUN_43f7_0082(pool k, nation) (viceroy_unpacked.c:73519-73543): unit type
 * for a 10f0 landing. k==2 → Man-O-War (0x12), k==3 → Artillery (0xb). For
 * k 0/1 the human (control == 0) gets `9 - 5*(!woi)` / `7 - 3*(!woi)`, i.e.
 * Cont. Army (9) / Cont. Cav. (7) once independence is declared and
 * Dragoons (4) for BOTH slots before it — the peacetime @MERCENARIES hire
 * (2244) lands Dragoons. Non-human nations get Regulars (6) / Cavalry (8).
 * Names are the NAMES.TXT @UNIT rows; the singular fallbacks cover the
 * test pools.
 */
/* ===== REF landing, announce & disembark (ai_king_10f0_spawn_unit .. ai_king_spend_woi_bell_pool) ===== */
COLONIZE_INTERNAL int ai_king_10f0_spawn_unit(
  ColonizeTurnContext* ctx, int human, int k, int x, int y
) {
  static const int names[4][4] = {
    {UNITS_KIND_CONT_ARMY, UNITS_KIND_REGULAR, UNITS_KIND_SOLDIER, -1},
    {UNITS_KIND_CONT_CAV, UNITS_KIND_DRAGOON, UNITS_KIND_SCOUT, -1},
    {UNITS_KIND_MAN_O_WAR, UNITS_KIND_FRIGATE, -1, -1},
    {UNITS_KIND_ARTILLERY, -1, -1, -1},
  };
  /* DOS-LITERAL FUN_43f7_0082 raw 73519-73543: for k 0/1 the FIRST test is
   * `nation > 3 || player[nation].control != 0` → Regulars (6) / Cavalry (8);
   * only a control==0 slot reaches the woi term, which yields Cont. Army (9) /
   * Cont. Cav. (7) after the declaration and Dragoons (4) before it. The port
   * ignored `human` entirely and handed Continentals to REF/AI nations
   * (bugs.md #505). */
  static const ColonizeUnitKind crown_kinds[2] = {UNITS_KIND_REGULAR, UNITS_KIND_CAVALRY};
  if (!ctx || !ctx->units || k < 0 || k > 3) {
    return -1;
  }
  int ty = -1;
  if (k < 2) {
    const int not_player =
      (human < 0 || human > 3 ||
       (ctx->col1_ok && ctx->col1 && ctx->col1->player[human].control != 0));
    if (not_player) {
      ty = units_kind_type_index(ctx->units, crown_kinds[k]);
      if (ty < 0) {
        return -1;
      }
    } else if (ctx->col1_ok && ctx->col1 && !ai_king_independence_declared(ctx->col1)) {
      ty = units_kind_type_index(ctx->units, UNITS_KIND_DRAGOON);
    }
  }
  for (int i = 0; i < 4 && ty < 0 && names[k][i] >= 0; ++i) {
    ty = units_kind_type_index(ctx->units, (ColonizeUnitKind)names[k][i]);
  }
  if (ty < 0) {
    return -1;
  }
  const int uid = units_spawn_allow_stack(ctx->units, ty, x, y);
  if (uid < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (u) {
    units_set_nation(u, human);
    if (k != 2) {
      u->profession = UNITS_JOB_SOLDIER; /* DOS unit+0x15 = 0x15 Veteran Soldier */
    }
    u->orders = UNITS_ORDER_NONE; /* player-controlled: no AI orders */
  }
  return uid;
}

/*
 * FUN_43f7_1528 %STRING3 colony pick — ported 2026-09-06d (was: the port
 * named the *landing* colony). DOS (viceroy_unpacked.c:74471-74481, the loop
 * that runs before the popup is built) walks the whole colony array with
 * FUN_281f_09e6 and keeps the human's (`+0x1a == DS:0x5398`) COASTAL
 * (`i*0xca+0x5d62 & 0x40` = ColonizeCol1ColonyFlags.coastal) colony with the
 * largest population (`+0x1f`); the test is strict `<`, so the FIRST colony
 * at the maximum wins, and `iVar4` is seeded to 0 — with no coastal human
 * colony at all DOS names colony index 0 whatever it is. `FUN_281f_0416(3,
 * iVar4*0xca+0x5d48)` then splices that colony's name (+2) into %STRING3.
 * Returns NULL only when there is no colony array to read at all.
 */
static const char* ai_king_1528_announce_colony(const ColonizeTurnContext* ctx, int human) {
  if (!ctx || human < 0 || human >= 4) {
    return NULL;
  }
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony && ctx->col1->head.colony_count > 0) {
    int best_pop = -1;
    uint16_t best_i = 0; /* DOS seeds iVar4 = 0, not -1 */
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != human || !c->flags.coastal) {
        continue;
      }
      if (best_pop < (int)c->population) {
        best_pop = (int)c->population;
        best_i = i;
      }
    }
    if (ctx->col1->colony[best_i].name[0]) {
      return ctx->col1->colony[best_i].name;
    }
  }
  /* No Col1 colony array (synthetic fixtures): same rule over the live pool. */
  if (ctx->colonies) {
    int best_pop = -1;
    const ColonizeColony* best = NULL;
    const ColonizeColony* first = NULL;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (!first) {
        first = c;
      }
      if (ctx->map && !map_tile_is_coastal(ctx->map, c->x, c->y)) {
        continue;
      }
      if (best_pop < c->population) {
        best_pop = c->population;
        best = c;
      }
    }
    if (!best) {
      best = first;
    }
    if (best && best->name[0]) {
      return best->name;
    }
  }
  return NULL;
}

/*
 * FUN_43f7_10f0(param_1) — the ONE landing routine DOS uses for both the
 * free foreign intervention (`param_1 == 0`, reached per turn from
 * FUN_43f7_2022's else arm) and the PAID mercenary hire (`param_1 == 1`,
 * FUN_43f7_2022's `thunk_FUN_2a1f_010a(1)` tail at 75068). `paid` is that
 * flag; it changes four things and nothing else:
 *   - the pools are neither gated on nor decremented (74379, 74445);
 *   - the per-type counts come from the caller's mercenary array
 *     (DS:0x9e46 = −0x61ba) instead of the 6-troop cap arithmetic (74424);
 *   - the arrival popup is @MERCS naming rival slot 2, not @INTERVENE /
 *     @INTERVENTION naming slot 1, and there is no music switch (74396-406);
 *   - the Man-O-War that carried them in is despawned afterwards (74451).
 * Audit D6 (2026-09-10) folded ai_king_do_merc_hire_at's separate spawner
 * into this path; before that the paid hire had no terrain test at all.
 *
 * `backup_force` is the real mapped DOS pool array
 * (0x53e2/0x53e4/0x53e6/0x53e8), seeded on the declaration by
 * ai_king_seed_backup_force_1a26 — not a stand-in.
 *
 * Gate (no REF-empty condition; see the bugs.md note in the body): the
 * per-turn free drain (FUN_43f7_2022 raw 75007) needs the intervention-once
 * latch AI_KING_INTERVENE_ANNOUNCED_BYTE set AND the MoW pool
 * backup_force[2] nonzero. The bell spend does NOT land anything — it only
 * announces (ai_king_1528_announce); bugs.md #538.
 *
 * Landing (74378-74449): every unit is spawned for the HUMAN nation, so the
 * force is player-controlled. One Man-O-War (pool [2] −1) on the best water
 * tile beside the target colony, then land troops capped as DOS does —
 * Cont. Cav. ≤ 2 (pool [1]), Artillery ≤ 2 (pool [3]), Cont. Army = 6 minus
 * those (pool [0]) — each further capped by its pool, unloaded at the colony,
 * with a 5×5 reveal.
 *
 * Intervene nation: the saved rival slot (rival_nation_slot_1) when valid,
 * else the Euro with most colonies (tie-break land-unit force).
 *
 * Popups: the per-landing @INTERVENE / @MERCS arrival line only. The
 * once-per-game "<country> declares war on <country>" announcement belongs
 * to FUN_43f7_1528 (ai_king_1528_announce), not here. Deep economy /
 * mercenary chrome remains unported.
 *
 * `target` = DOS's `iVar2 = *(int *)0x5398` (74308), the nation the whole
 * force is spawned for and whose colonies the roulette walks. DOS hardcodes
 * the human there; every caller passes ctx->human_nation (the 2244
 * peacetime hire included since 2026-09-15 — its AI-beneficiary premise was
 * refuted), so the parameter is byte-exact everywhere.
 *
 * NO independence gate: 10f0 itself has none in DOS (74270-74310 goes
 * straight into the colony walk). WoI state is the CALLERS' business —
 * FUN_43f7_2022 runs behind `0x5382 & 1` set, FUN_43f7_2244 behind it clear
 * (75088), and the free arm's own `backup_force` drain gate below stands in
 * for 2022's. Hoisting a shared gate up here made the peacetime paid path
 * unreachable.
 */
/*
 * FUN_43f7_1528 (viceroy_unpacked.c:74459-74497) — the @INTERVENTION
 * "declares war" announce, and nothing else. DOS reaches it only from the
 * wartime bell-pool spend FUN_4345_0a22 (raw 73368 `FUN_291f_0348` -> raw
 * 34894 -> 1528) and only while `(*0x5382 & 2) == 0`: it picks the human's
 * largest coastal colony (74470-74481) for %STRING3, shows the popups
 * (0x12d4 / 0x12db) and latches `*0x5382 |= 2` (74496). It spawns no unit
 * and lands no force. The landing is FUN_43f7_10f0's free drain, which the
 * once-per-turn FUN_43f7_2022 arm runs on a LATER turn, gated on that same
 * bit plus a nonzero Man-O-War pool (raw 75007).
 *
 * bugs.md #538: the port used to land the whole force straight from the
 * bell spend, so the free drain — whose gate the announce had just opened —
 * landed a second intervention force in the same turn.
 */
static void ai_king_1528_announce(ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (ai_king_latch_get(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE) != 0) {
    return; /* DOS 0a22 never re-enters 1528 once bit2 is up */
  }
  const int ally1 = ai_king_intervention_nation_slot(ctx, human, 0); /* DS:0x53d4 */
  ai_king_latch_set(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE, 1); /* 74496 */

  /* bugs.md #252: the declaration names the PARENT countries (1528 passes
   * both nations through FUN_291f_0ac8's country-name form). */
  const char* ally_name =
    (ally1 >= 0 && ally1 < 4) ? reports_nation_adjective_display_name(ally1) : ""; /* #878f: no typed text */
  const char* ally_country =
    (ally1 >= 0 && ally1 < 4) ? reports_nation_country_name(ally1) : "A foreign power";
  const char* crown_country = reports_nation_country_name(human);
  const char* announce_colony = ai_king_1528_announce_colony(ctx, human);
  if (!announce_colony || !announce_colony[0]) {
    announce_colony = "the colonies";
  }
  /* @FRIEND row for the ally ("French General Lafayette", ...) — 1528
   * splices GAME.TXT @FRIEND[ally] into %STRING2. */
  char general[64];
  general[0] = '\0';
  {
    const ColonizeMsgSection* fsec = assets_msg_find(ctx->messages, "FRIEND");
    if (fsec && ally1 >= 0 && ally1 < fsec->line_count && fsec->lines[ally1][0]) {
      str_copy_trunc(general, sizeof(general), fsec->lines[ally1]);
    }
  }
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "%s declares war on %s!",
             ally_country, crown_country);
  }
  if (!ai_king_human_popups(ctx)) {
    return;
  }
  PopupMsgTokens itok;
  memset(&itok, 0, sizeof(itok));
  itok.string0 = ally_country;
  itok.string1 = crown_country;
  itok.string2 = general;
  itok.string3 = announce_colony;
  itok.string4 = ally_name;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "INTERVENTION", &itok, "", body, sizeof(body));
  (void)ai_popup_enqueue_ok_ctx(
    ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, ally1 >= 0 ? ally1 : 0, 0, NULL, body
  );
  units_pump_combat_popups();
}

/*
 * FUN_43f7_10f0 arrival chrome: the @DECLAREWAR / @INTERVENE announce beat
 * for the intervention force. Extracted verbatim from ai_king_10f0_land.
 */
static void ai_king_10f0_announce(
  ColonizeTurnContext* ctx, int human, int ally1, int paid, int landings,
  int hx, int hy, int sx, int sy, const int merc_counts[4], const int want[4]
) {
  /* bugs.md #252: the declaration names the PARENT countries (DOS 1528
   * passes both nations through FUN_291f_0ac8's country-name form —
   * "France declares war on England"), never the new-world colony names.
   * The arrival line uses the nationality adjective ("French Intervention
   * Force"). */
  const char* ally_name =
    (ally1 >= 0 && ally1 < 4) ? reports_nation_adjective_display_name(ally1) : ""; /* #878f: no typed text */
  const char* colony = "the colonies";
  if (ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, hx, hy);
    const ColonizeColony* c = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
    if (c && c->name[0]) {
      colony = c->name;
    }
  }
  if (ctx->status && ctx->status_size) {
    PopupMsgTokens status_tok;
    memset(&status_tok, 0, sizeof(status_tok));
    status_tok.string0 = colony;
    status_tok.string1 = ally_name;
    popup_msg_fill(ctx->messages, paid ? "MERCS" : "INTERVENE", &status_tok, "",
                   ctx->status, ctx->status_size);
    popup_msg_strip_markup(ctx->status);
  }
  if (paid && ai_king_human_popups(ctx)) {
    /*
     * DOS 74400-74406: the paid arm shows GAME.TXT 0x12ce = @MERCS
     * ("%STRING1 mercenaries arrive in %STRING0.") with %STRING0 = the
     * landing colony name (0416(0, *0x8542+2), shared with the free arm)
     * and %STRING1 = the nationality adjective of rival slot 2. No
     * @INTERVENTION declaration, no 0498(3) music switch — those are the
     * free arm's (`param_1 == 0`) only.
     */
    PopupMsgTokens mtok;
    memset(&mtok, 0, sizeof(mtok));
    mtok.string0 = colony;
    mtok.string1 = ally_name;
    char mbody[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "MERCS", &mtok, "", mbody, sizeof(mbody));
    /* bugs.md #875: the ARRIVAL tag, not the OFFER's own KING_MERC tag —
     * ai_king_merc_offer_pending() reads an unread KING_MERC popup as "an
     * offer is still pending" and returns before the 1-in-3 dos_rng_range
     * draw, desyncing the RNG. The free arm below already uses ARRIVAL. */
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human,
      ai_king_crown_nation_col1(ctx->col1, human), landings, NULL, mbody
    );
    units_pump_combat_popups();
  }
  if (!paid && ai_king_human_popups(ctx)) {
    char body[AI_POPUP_BODY_LEN];
    /* The one-per-game "<country> declares war" announcement is NOT here:
     * DOS shows it from FUN_43f7_1528 at the bell spend (see
     * ai_king_1528_announce). 10f0 only ever shows the per-landing
     * @INTERVENE arrival line (74396). */
    PopupMsgTokens atok;
    memset(&atok, 0, sizeof(atok));
    atok.string0 = colony;
    atok.string1 = ally_name;
    popup_msg_fill(ctx->messages, "INTERVENE", &atok, "", body, sizeof(body));
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, ally1, landings, NULL, body
    );
    sound_set_bgm(3); /* FUN_43f7_10f0 43f7:145b: 281f_0498(3) Independence pool… */
    sound_play(0x3f); /* …then 43f7:1465: intervention tune after @INTERVENE */
    /* bugs.md #253: the arrival popup BLOCKS before the disembark slides,
     * same sequencing as the REF landing (bugs.md #237). */
    units_pump_combat_popups();
  }
}

/*
 * FUN_43f7_10f0 disembark: the pooled land units step ashore from the
 * Man-O-War tile. Extracted verbatim from ai_king_10f0_land.
 */
static void ai_king_10f0_disembark(
  ColonizeTurnContext* ctx, int human, int paid, uint16_t* backup,
  const int pool_k[3], const int want[4], int hx, int hy, int sx, int sy
) {
  /* DOS-LITERAL raw 74433-74435: `local_1a = 0; break;` — a refused 095c
   * clears the OUTER loop's condition, so no later pool is tried either
   * (bugs.md #873d). */
  int alive = 1;
  for (int pi = 0; pi < 3 && alive; ++pi) {
    const int k = pool_k[pi];
    const int n = want[k];
    for (int s = 0; s < n; ++s) {
      const int uid = ai_king_10f0_spawn_unit(ctx, human, k, sx, sy);
      if (uid < 0) {
        alive = 0;
        break;
      }
      if (!paid && backup[k] > 0) {
        backup[k]--; /* DOS 74445: `if (param_1 == 0) *(0x53e2 + k*2) -= 1` */
      }
      ColonizeUnit* lu = units_get(ctx->units, uid);
      if (lu) {
        lu->moves = 3;
        lu->goto_x = hx;
        lu->goto_y = hy;
      }
      ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
      if (!units_try_move_w(&w_, uid, hx, hy)) {
        lu = units_get(ctx->units, uid);
        if (lu) {
          const int ox = lu->x;
          const int oy = lu->y;
          lu->x = hx;
          lu->y = hy;
          units_occupancy_notify_moved(ctx->units, ox, oy, lu->x, lu->y);
        }
      }
      lu = units_get(ctx->units, uid);
      if (lu) {
        lu->moves = units_max_mp(ctx->units, uid);
        lu->orders = UNITS_ORDER_NONE;
        lu->goto_x = UNITS_GOTO_NONE;
        lu->goto_y = UNITS_GOTO_NONE;
        if (ctx->map) {
          lu->col1_vis_mask |= units_vis_mask_for_tile(ctx->map, lu->x, lu->y, human);
        }
      }
    }
  }
}

void ai_king_10f0_land(
  ColonizeTurnContext* ctx, int target, int paid, const int merc_counts[4]
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units) {
    return;
  }
  if (target < 0 || target >= 4) {
    return;
  }
  uint16_t* backup = ctx->col1->head.backup_force;
  /*
   * bugs.md (user: "Foreign intervention isn't happening even though I've
   * gathered the required bells"): the old gate here returned while the
   * expeditionary pools were nonzero — but DOS has no such condition on
   * EITHER trigger, so intervention could never fire mid-war. The real DOS
   * gates: the bells-threshold announce (FUN_4345_0a22 → 74462 @INTERVENTION,
   * sets 0x5382 bit2) is blocked only by that bit; the per-turn free drain
   * (FUN_43f7_2022 line 75007) needs bit2 SET and the MoW pool
   * (0x53e6 = backup_force[2]) nonzero. bit2 is set ONLY by the announce
   * (74493) — it is the intervention-once latch, not "REF on the map";
   * AI_KING_INTERVENE_ANNOUNCED_BYTE models it here.
   */
  if (!paid) {
    /* FUN_43f7_2022 raw 75007: the free drain needs the 1528 announce latch
     * (0x5382 bit2) AND a nonzero Man-O-War pool (0x53e6). */
    if (ai_king_latch_get(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE) == 0 ||
        backup[2] == 0) {
      return;
    }
    if (ai_king_force_total(backup) <= 0) {
      return;
    }
  }
  /* DOS 74308 `iVar2 = *(int *)0x5398` — one nation drives the colony walk,
   * the spawn owner and the popup tokens alike. */
  const int human = target;
  int hx = 0;
  int hy = 0;
  int sx = 0;
  int sy = 0;
  /*
   * DOS-LITERAL FUN_43f7_10f0 raw 74310/74334/74377: no candidates
   * (`local_24 == 0`), a roulette that fell through (`local_56 < 0`) or no
   * scored water tile (`local_54 == 0`) each fall straight out of the body —
   * nothing lands, no pool is decremented, and a paid hire's gold stays
   * spent. The port's weakest-port fallback and "any other colony with
   * water" rescan were invented (bugs.md #873a/b).
   */
  if (ai_king_10f0_pick_colony(ctx, human, &hx, &hy) < 0) {
    return;
  }
  /*
   * DOS names a different power in each mode's arrival line: the free
   * intervention splices `FUN_281f_09a4(*0x53d4)` (rival slot 1, the ally)
   * into %STRING1 for @INTERVENE (74396), the paid hire
   * `FUN_281f_09a4(*0x53d6)` (rival slot 2) for @MERCS (74403) — the same
   * 0x53d6 the @MERCENARIES offer names as the seller (75048).
   */
  const int ally1 = ai_king_intervention_nation_slot(ctx, human, paid ? 1 : 0);
  if (ally1 < 0) {
    return;
  }
  /*
   * FUN_43f7_10f0 74378–74449, resolved 2026-08-28 (was P5.5 "control"):
   * every unit is spawned for DS:0x5398 — the HUMAN's nation — so the
   * intervention force is player-controlled. A Man-O-War (type 0x12) lands
   * on the best water tile next to the colony (pool 0x53e6 −1), then the
   * land troops: Cont. Cav. ≤ 2 (0x53e4), Artillery ≤ 2 (0x53e8), Cont.
   * Army = 6 − those (0x53e2), each capped by its pool; +0x15 = Veteran;
   * unloaded at the colony (`0948`); 5×5 reveal around the colony.
   */
  if (!ai_king_10f0_pick_spawn(ctx, human, hx, hy, &sx, &sy)) {
    return; /* DOS: no scored tile → nothing lands this turn */
  }
  /* DOS-LITERAL raw 74378-74380: `if (param_1 == 0) *0x53e6 -= 1` runs
   * BEFORE `095c`, whatever the spawn answers (bugs.md #873e). The free-drain
   * gate above already guarantees backup[2] != 0 here. */
  if (!paid) {
    backup[2]--;
  }
  const int mow = ai_king_10f0_spawn_unit(ctx, human, 2, sx, sy);
  if (mow < 0) {
    return;
  }
  if (ctx->map) {
    map_reveal_tile(ctx->map, sx, sy, human);
  }
  int landings = 1;
  static const int pool_k[3] = {0, 1, 3};
  /*
   * DOS 74409-74427 computes the free-drain caps into local_50[] and mins
   * each against its pool, then — for the paid hire — throws that away:
   * `if (param_1 != 0) local_8 = *(int *)(iVar4 + -0x61ba);`, i.e. the
   * mercenary count array FUN_43f7_2022 filled (DS:0x9e46/0x9e48/0x9e4c,
   * −0x61ba == 0x9e46). Neither the pools nor 0x53e2+ are touched in that
   * mode.
   */
  int want[4] = {0, 0, 0, 0};
  if (paid) {
    for (int pi = 0; pi < 3; ++pi) {
      const int k = pool_k[pi];
      want[k] = merc_counts ? merc_counts[k] : 0;
      if (want[k] < 0) {
        want[k] = 0;
      }
    }
  } else {
    int caps[4] = {0, 0, 0, 0};
    caps[1] = backup[1] > 2 ? 2 : (int)backup[1];
    caps[3] = backup[3] > 2 ? 2 : (int)backup[3];
    caps[0] = 6 - (caps[1] + caps[3]);
    for (int pi = 0; pi < 3; ++pi) {
      const int k = pool_k[pi];
      want[k] = caps[k] > (int)backup[k] ? (int)backup[k] : caps[k];
      if (want[k] < 0) {
        want[k] = 0;
      }
    }
  }
  for (int pi = 0; pi < 3; ++pi) {
    landings += want[pool_k[pi]];
  }

  if (landings > 0) {
    ai_king_10f0_announce(
      ctx, human, ally1, paid, landings, hx, hy, sx, sy, merc_counts, want
    );
  }

  /* bugs.md #253: land troops disembark VISIBLY — spawn on the ship's tile
   * and slide into the colony through units_try_move (fires the move-watch
   * animation), like the REF landing. Unlike normal disembark rules they
   * arrive ready for action: full moves restored after the step. */
  ai_king_10f0_disembark(
    ctx, human, paid, backup, pool_k, want, hx, hy, sx, sy
  );
  if (ctx->map) {
    map_reveal_radius(ctx->map, hx, hy, human, 2);
  }
  /*
   * DOS 74450-74452: `if ((param_1 != 0) && (-1 < local_1c))
   * FUN_281f_0808(local_1c);` — the paid hire's Man-O-War is the seller's
   * transport, not a gift: it drops the mercenaries and is despawned. Only
   * the free intervention leaves its hull behind for the player.
   */
  if (paid && mow >= 0) {
    (void)units_despawn(ctx->units, mow);
  }
}

/*
 * FUN_4345_0a22 wartime spend: when the bell pool reaches the WoI threshold,
 * trigger foreign intervention / REF arrival instead of electing a Father.
 * Returns 1 when the pool should be zeroed; 0 when REF-present blocks spend.
 */
int ai_king_spend_woi_bell_pool(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return 0;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return 0;
  }
  if (nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  /*
   * DOS 0a22: `(*(byte*)0x5382 & 2) != 0 -> return` WITHOUT zeroing the pool.
   * Bit2 is the intervention-once latch (set only by the @INTERVENTION
   * announce), NOT REF presence — the old ref_present gate plus the
   * exp-pools routing below meant the bells NEVER bought the intervention
   * (they re-triggered a REF wave instead). Waves run per-turn from
   * ai_king_nation_turn on their own; the bells buy exactly one thing.
   */
  if (ai_king_latch_get(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE) != 0) {
    return 0; /* pool kept, same as DOS */
  }
  if (nation_id == ctx->human_nation) {
    /* DOS 0a22 wartime arm (raw 73366-73369): announce ONLY (FUN_43f7_1528),
     * no landing. The force itself arrives from 2022's once-per-turn free
     * drain on a later turn. bugs.md #538. */
    ai_king_1528_announce(ctx, ctx->human_nation);
  }
  return 1;
}

/*
 * Pack offer-time roll + landing pick into the popup payload:
 * hx(6b)<<26 | hy(6b)<<20 | qty_a(4b)<<16 | extra_flag(1b)<<15 | price(15b).
 * Since audit D6 the accept runs FUN_43f7_10f0(1), which rolls its own
 * landing colony the way DOS does, so hx/hy survive only as an offer-time
 * sanity field (a negative pair still refuses the hire). qty_a and
 * extra_flag are the load-bearing halves: they ARE the DS:0x9e46 mercenary
 * count array 2022 fills before the CHOICE. Price fits
 * 15 bits (max observed (8+2)*((4+3)*2+6)*100 = 20000 < 32768); qty_a fits
 * 4 bits (range 2-8); hx/hy fit 6 bits each (map width/height ≤ 63 in this
 * project's fixed 58×72 world).
 */
