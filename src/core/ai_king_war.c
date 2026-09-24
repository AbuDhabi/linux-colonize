/*
 * King / REF AI — War declaration events, REF war conduct, WoI endgame scoring & the nation turn driver
 *
 * Split out of ai_king.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_king_internal.h. See ai_king.c for the module prologue and the
 * crown/boycott, tea-party and REF-bookkeeping helpers.
 *
 * Sections:
 *   War declaration events & REF war conduct (ai_king_new_war_event .. ai_king_ref_pre_euro_beat)
 *   War of Independence endgame scoring (ai_king_human_coastal_ports .. ai_king_check_revolution_end)
 *   Nation turn driver & popup result dispatch (ai_king_nation_turn .. ai_king_apply_popup_result)
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

/* ===== War declaration events & REF war conduct (ai_king_new_war_event .. ai_king_ref_pre_euro_beat) ===== */
int ai_king_new_war_event(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->rng || !ctx->turn_number) {
    return 0;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4 || col1->player[human].control != 0) {
    return 0;
  }
  if (founding_fathers_nation_has(col1, human, FF_BENJAMIN_FRANKLIN)) {
    return 0; /* FUN_281f_07b4(nation, 0x13) */
  }
  const int difficulty = (int)col1->head.difficulty;
  const int turn = (int)*ctx->turn_number;
  if ((difficulty + 2) * turn <= 799) {
    return 0;
  }
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  int peace_n = 0;
  int met_no_peace = 0;
  long strength_peers = 0;
  long strength_self = 0;
  for (int p = 0; p < 4; ++p) {
    if (p == human || p == crown || (col1->nation[p].nation_flags & 0x04) != 0) {
      continue; /* self / REF nation / independent */
    }
    /* Raw byte, not ai_diplo_read: DOS reads DS:-0x77c4 directly and an unmet
     * pair is 0 there (ai_diplo_read synthesizes PEACE|MET for unwritten pairs). */
    const uint8_t rel = col1->nation[human].euro_relation[p];
    if (rel & AI_DIPLO_PEACE) {
      peace_n++;
    }
    if ((rel & (AI_DIPLO_PEACE | AI_DIPLO_MET)) == AI_DIPLO_MET) {
      met_no_peace++;
      /* DOS sums the two -0x6be4 strengths 14x (loop 1..14, per-continent shape). */
      strength_peers += 14L * (long)col1->stuff.land_combat_strength[p];
      strength_self += 14L * (long)col1->stuff.land_combat_strength[human];
    }
  }
  if (peace_n == 0 || met_no_peace != 0 || strength_peers > strength_self) {
    return 0;
  }
  if (dos_rng_range(ctx->rng, 0, (4 - peace_n) * 20) > difficulty) {
    return 0;
  }
  int peer = -1;
  for (int tries = 0; tries < 64 && peer < 0; ++tries) {
    int cand;
    do {
      cand = dos_rng_range(ctx->rng, 0, 3);
    } while (cand == human);
    if ((col1->nation[human].euro_relation[cand] & AI_DIPLO_PEACE) != 0 &&
        (col1->nation[cand].nation_flags & 0x04) == 0) {
      peer = cand;
    }
  }
  if (peer < 0) {
    return 0;
  }
  int count = 1;
  int gold = (difficulty + 1) * 100;
  const int fc_self = (int)col1->stuff.field_combat_totals[human];
  const int fc_peer = (int)col1->stuff.field_combat_totals[peer];
  if (fc_self < fc_peer) {
    const int gap = fc_peer - fc_self;
    count = (gap >> 3) + 1;
    gold += gap * 25;
  }
  if (count > 6 - difficulty) {
    count = 6 - difficulty;
  }
  if (gold > (5 - difficulty) * 500) {
    gold = (5 - difficulty) * 500;
  }
  if (count < 0) {
    count = 0;
  }

  const char* peer_name =
    col1->player[peer].country_name[0] ? col1->player[peer].country_name : "rival";
  if (ctx->ai_popups) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = reports_difficulty_title(difficulty >= 0 && difficulty < 5 ? difficulty : 0);
    tok.string1 = col1->player[human].name[0] ? col1->player[human].name : "";
    tok.string2 = peer_name;
    tok.number0 = gold;
    tok.has_number0 = true;
    tok.number1 = count;
    tok.has_number1 = true;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "The Crown has declared war on the %s and cancelled your peace. It sends %d$ and %d "
      "Veteran Soldier units.",
      peer_name,
      gold,
      count
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "KINGNEWWAR", &tok, fallback, body, sizeof(body));
    if (ai_popup_enqueue_ok_ctx(
          ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human, peer, gold, NULL, body
        )) {
      /* DOS 38fd:5b6e `PUSH 0x1134` / 38fd:5b71 `CALLF 291f:0ad4` — 0ad4 is
       * the King-flair message helper (FUN_6f74_378a: DS:0x1f5c = 8, then
       * show), so @KINGNEWWAR wears KING.SS like the tax audience does. */
      ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
    }
  }
  europe_nation_gold_add(ctx->europe, col1, human, (long)gold); /* audit G3 */
  /* FUN_281f_095c(type 1 Soldier, nation, -20,-20) x count, profession 0x15 = Veteran:
   * the units appear in Europe — Linux puts them on the docks. */
  if (ctx->europe) {
    for (int i = 0; i < count; ++i) {
      if (!europe_dock_push_load(
            ctx->europe, reports_job_display_name(UNITS_JOB_SOLDIER), UNITS_JOB_SOLDIER
          )) {
        break;
      }
    }
  }
  ai_diplo_clear_both(col1, human, peer, AI_DIPLO_PEACE);
  ai_diplo_or_both(col1, human, peer, AI_DIPLO_CROWN_ARMED);
  col1->head.nation_relation[peer] = (int16_t)turn; /* DS:0x53c8[peer] = turn */
  return 1;
}

/*
 * DS:0x9456[nation] — census FUN_4962_0018 (viceroy_unpacked.c:78146 clear,
 * :78182/:78185 the two increments): the count of that nation's SHIP units
 * (type 0x0d..0x12) parked on a Europe x-sentinel, i.e. `x - nation == -0xc`
 * (244+n, "sailing to Europe") or `== -0x10` (240+n, docked in Europe) — see
 * docs/save_format_map.md row `x`/`y`. DS:0x945a is the land-unit twin
 * (236+n, already used by the 5d04 hire tail).
 *
 * The port has no Europe dock for the crown slot: a crown hull that reaches
 * the high seas leaves the map outright (units_despawn, below and in
 * ai_king_mow_sail_home_20e6) instead of parking at an off-map sentinel
 * column, so neither DOS x-sentinel state (240+n docked, 244+n sailing) is
 * literally representable. bugs.md #878e: counting only hulls already
 * standing ON a high-seas tile was over-restrictive — the same function that
 * despawns an arrived hull also runs this gate first, so a hull essentially
 * never survives a turn boundary sitting on a high-seas tile, and the count
 * came back 0 on almost every call, defeating the DOS "only one hull away at
 * a time" gate the 20e6 disjunct (`DS:0x9456[nation] != 0`) encodes.
 *
 * DOS bumps DS:0x9456[nation] the instant the sail-home decision is made
 * (raw 89717-89720: the ring-hunt for a High Seas tile, then `+0x314b =
 * 0x45` / act_state `+0x314c = 3 (or 0xb)` — i.e. before the hull has
 * actually reached that tile), and the census that recomputes it from the
 * x-sentinel columns only ever finds hulls that already crossed off-map. The
 * port's counterpart of "sail-home decided, not yet resolved" is
 * `u->orders == UNITS_ORDER_AI_SAIL` (ai_king_mow_sail_home_20e6 stamps it
 * at the same decision point, raw-comment above), so a crown sea unit counts
 * here for the whole multi-turn transit, not only its last tile.
 */
static int ai_king_crown_ships_in_europe_lane(const ColonizeTurnContext* ctx, int crown) {
  if (!ctx || !ctx->units || !ctx->map) {
    return 0;
  }
  int n = 0;
  /*
   * Slot walk, `u->id` to the id-taking accessors. `units_get_const` and
   * `units_is_sea` take a unit ID; ids are handed out monotonically from 1
   * and never recycled (units.c:337), so an `i`-as-id walk over
   * COLONIZE_UNITS_MAX dropped every unit with id >= 256 in a long game plus
   * the highest slot in a short one. DOS walks the unit ARRAY in record
   * order (raw 78159). Fixed 2026-09-10 (audit second-wave Leads item 2).
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(ctx->units, u->id)) {
      continue;
    }
    if (u->orders == UNITS_ORDER_AI_SAIL || map_tile_is_high_seas(ctx->map, u->x, u->y)) {
      n++;
    }
  }
  return n;
}

/*
 * The crown Man-O-War's return-home beat — the real DOS one, replacing the
 * "despawn idle empty crown MoWs at wave start" stand-in that used to sit in
 * ai_king_ref_wave (and the `4d56 ship act` label it carried: overlay 4d56 is
 * the Indian AI overlay end to end — see FUNCTION_CATALOG rows 0038…4528 —
 * and holds no crown code at all).
 *
 * The beat lives at the tail of the `FUN_521d_20e6` ship band
 * (viceroy_unpacked.c:89717-89720; recovered listing
 * move_scoring_20e6_full.md:2036-2040). Every disjunct below is the DOS
 * *skip* test, so the arm fires when all of them are false:
 *
 *   if (  (DS:0x5382 & 1) == 0                  // not at war
 *      || unit+0x3146 != 0x12                   // not a Man-O-War
 *      || iStack_6 != 0                         // orders byte +0x314b is 't'/'i'
 *      || iStack_a8 != 0                        // 8aac(unit,2)-1: tile stack minus self
 *      || DS:0x53de != 0                        // MoW pool (expeditionary_force[2]) not empty
 *      || DS:0x9456[nation] != 0                // a ship of this nation is already in the Europe lane
 *      || DS:0x53da+0x53dc+0x53e0 == 0 )        // no land pools left
 *     { ...ordinary ship arms... }
 *   // else falls through to LAB_521d_3fa6:
 *   //   FUN_291f_02ea -> FUN_48d3_015e: expanding-ring hunt for a High Seas
 *   //   tile (class 0x1a, owner nibble < 0 or own nation), bump
 *   //   DS:0x9456+nation, act_state +0x314c = 3 (or 0xb), latch the tile in
 *   //   +0x314d/e and stamp orders +0x314b = 0x45 — i.e. sail home.
 *   // (The port models the goal with its own pursue order UNITS_ORDER_AI_SAIL
 *   //  = 0x0b, the `+0x314c = 3 (or 0xb)` act-state above; the raw DOS
 *   //  +0x314b order byte 0x45 has no port enum — bugs.md #878d.)
 *
 * Nothing credits a pool: `expeditionary_force[]` is untouched on the way
 * out. The fleet cadence comes from FUN_43f7_0982's own opening gate
 * (`force[2] == 0 && crown Man-O-War count (-0x6da2) == 0 -> force[2]++`,
 * raw 73990-73993), which can only fire once the hull is off the map.
 *
 * Port mapping of the two opaque operands:
 *   - `iStack_6` is set (raw 1144-1149 of the recovered listing) only from
 *     the 0a60 goal pass's order codes 't' (FOUND) / 'i' (MIL_EXPAND). No
 *     REF type can pursue FOUND/MIL_EXPAND (name-gated to Pioneer/Colonist
 *     kinds), so the term stays 0 for a crown Man-O-War even now that the
 *     crown runs the full euro turn (D1 closed 2026-09-07g).
 *   - `iStack_a8` = FUN_1000_8aac(unit, 2) - 1 = the ship's tile stack minus
 *     itself (case 2 = TOTAL stack count, ai_euro.c's 8aac table). DOS
 *     passengers sit at (-2,-2) and never count; the port's sit in
 *     cargo_ids, so this is the tile scan alone and the caller runs the arm
 *     only for an empty hull.
 *
 * Departure: DOS hands the hull to the Europe lane (x = 244+nation) and the
 * crown's own Europe dock. The port models no crown dock, so a crown MoW
 * standing on the high-seas tile it was sent to leaves the map here — the
 * same net effect (hull gone, no pool credit) the old stand-in produced, but
 * now on the DOS trigger instead of a col1_counter16 counter.
 */
int ai_king_mow_sail_home_20e6(ColonizeTurnContext* ctx, ColonizeUnit* u, int crown) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1 || !u) {
    return 0;
  }
  /* Reached the crossing: the hull is home (DOS: into the Europe lane). */
  if (map_tile_is_high_seas(ctx->map, u->x, u->y)) {
    (void)units_despawn(ctx->units, u->id);
    return 1;
  }
  const uint16_t* force = ctx->col1->head.expeditionary_force;
  if (force[2] != 0) {
    return 0; /* DS:0x53de */
  }
  if ((int)force[0] + (int)force[1] + (int)force[3] == 0) {
    return 0; /* DS:0x53da + 0x53dc + 0x53e0 */
  }
  if (ai_king_crown_ships_in_europe_lane(ctx, crown) != 0) {
    return 0; /* DS:0x9456[nation] */
  }
  /* iStack_a8: any other unit sharing the ship's tile blocks the beat. */
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->id == u->id || o->aboard_ship_id >= 0) {
      continue;
    }
    if (o->x == u->x && o->y == u->y) {
      return 0;
    }
  }
  int hx = 0;
  int hy = 0;
  if (!units_spiral_place_hs_near(ctx->units, ctx->map, u->x, u->y, crown, &hx, &hy)) {
    return 0;
  }
  /* Port pursue-goal order (0x0b), DOS act_state `+0x314c = 3/0xb`; the DOS
   * `+0x314b = 0x45` order byte itself is not modelled (bugs.md #878d). */
  u->orders = UNITS_ORDER_AI_SAIL;
  u->goto_x = hx;
  u->goto_y = hy;
  if (u->moves > 0) {
    const int sdx = (hx > u->x) - (hx < u->x);
    const int sdy = (hy > u->y) - (hy < u->y);
    const int nx = u->x + sdx;
    const int ny = u->y + sdy;
    if ((sdx != 0 || sdy != 0) && map_tile_is_water(ctx->map, nx, ny) &&
        units_id_at(ctx->units, nx, ny) < 0) {
      {
      ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
      units_try_move_w(&w_, u->id, nx, ny);
    }
    }
    if (u->active && map_tile_is_high_seas(ctx->map, u->x, u->y)) {
      (void)units_despawn(ctx->units, u->id);
    }
  }
  return 1;
}

static void ai_king_war_act(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  /*
   * bugs.md #865: this is the PORT-ONLY "crown force on the map" latch
   * (AI_KING_REF_PRESENT_BYTE, pad bit) — DS:0x5382 bit 0x02 is DOS's
   * intervention-announce latch and is never cleared. Clear the port latch
   * once no crown unit remains in the New World, so wiping a wave re-arms
   * the next-wave trigger.
   */
  if (ai_king_latch_get(ctx->col1, AI_KING_REF_PRESENT_BYTE) && ctx->units) {
    const int crown_now = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation);
    bool crown_on_map = false;
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      /* Loose test on purpose: any live crown-slot unit in the New World
       * (incl. hold passengers) keeps the presence armed. */
      if (u->active && u->nation_id == crown_now && u->x < 200) {
        crown_on_map = true;
        break;
      }
    }
    /* Colonies the crown captured keep the presence too. */
    if (!crown_on_map && ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (c->active && c->nation_id == crown_now) {
          crown_on_map = true;
          break;
        }
      }
    }
    if (!crown_on_map) {
      ai_king_set_ref_present(ctx->col1, 0);
    }
  }
  const int human = ctx->human_nation;
  /* bugs.md #250: DOS 2022 runs the mobilization gate BEFORE any other war
   * beat — the mobilization turn does nothing else (no intervene/merc). The
   * gate block below returns early, so intervene/merc moved after it. */
  const int mobilization_due =
    human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT &&
    (ctx->col1->nation[human].nation_flags & 0x08u) == 0;
  if (!mobilization_due) {
    /*
     * DOS-LITERAL FUN_43f7_2022 raw 75007: ONE if/else over a single read of
     * the pair —
     *   if ((0x5382 & 2) == 0 || *0x53e6 == 0)  <merc offer, own returns>
     *   else                                    10f0(0)   (the free drain)
     * The two never run on the same turn. The port ran the drain first, so a
     * drain that took backup_force[2] 1 -> 0 let the merc offer fire in the
     * same beat: two landings and four extra RNG draws (bugs.md #874).
     */
    const int intervened =
      ai_king_latch_get(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE) != 0;
    const int mow_pool = (int)ctx->col1->head.backup_force[2];
    if (intervened && mow_pool != 0) {
      ai_king_10f0_land(ctx, ctx->human_nation, 0, NULL); /* FUN_43f7_10f0 free drain */
    } else {
      /* Recurring per-turn rebel merc offer (hire CHOICE / auto). */
      ai_king_merc_offer(ctx);
    }
  }

  /*
   * FUN_43f7_1eca full port. Per colony owned by the rebel nation with
   * colony SoL>49 (decomp `0x31 < iVar1`):
   *   cap = max(1, min(pop>>1, pop*(sol-50)/50))
   * Walk *only* the units stationed on that colony's own tile (decomp
   * FUN_281f_07e0/02e4 tile-stack walk — not every unit the nation owns)
   * and promote up to `cap` of them of base type Soldier
   * or Dragoon (decomp tests raw type id 1 / 4 only — Regulars and
   * already-Continental units never match and are untouched), AND Veteran
   * status (decomp `unit+0x315b == 0x15` = UNITS_JOB_SOLDIER "Veteran
   * Soldiers" — confirmed 2026-08-14 via the same offset/adjacent-code
   * cross-reference as the case-8/9 Pioneer profession `0x14`; an ordinary
   * armed colonist without Veteran profession does not promote).
   * Soldier → Continental Army, Dragoon → Continental Cavalry. Pops a
   * singular/plural status line per colony that actually promoted someone
   * (decomp 0x132d "one unit" / 0x1336 "%d units"). Washington FF mass
   * promote / combat-upgrade path is separate and untouched here. The
   * SoL 40..49 "restless" band is status-text only (below), not a promote
   * band in 1eca.
   */
  /* bugs.md #250 + DOS FUN_43f7_2022: the 1eca mobilization runs exactly ONCE,
   * on the first war-act turn after the declaration — gated on nation_flags
   * bit 0x08 (`*(byte*)*[0x84fc] & 8`), set right after, and DOS `return`s so
   * the mobilization turn does nothing else on the war path. */
  if (human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT &&
      (ctx->col1->nation[human].nation_flags & 0x08u) == 0) {
    ctx->col1->nation[human].nation_flags |= 0x08u;
    const int army = units_kind_type_index(ctx->units, UNITS_KIND_CONT_ARMY);
    const int cav = units_kind_type_index(ctx->units, UNITS_KIND_CONT_CAV);
    const int soldier_ty = units_kind_type_index(ctx->units, UNITS_KIND_SOLDIER);
    const int dragoon_ty = units_kind_type_index(ctx->units, UNITS_KIND_DRAGOON);
    if (ctx->col1->colony && (army >= 0 || cav >= 0) &&
        (soldier_ty >= 0 || dragoon_ty >= 0)) {
      for (uint16_t ci = 0; ci < ctx->col1->head.colony_count; ++ci) {
        const ColonizeCol1Colony* c = &ctx->col1->colony[ci];
        if ((int)c->nation_id != human) {
          continue;
        }
        const int sol_p = ai_king_colony_sol_at(ctx, human, (int)c->x, (int)c->y);
        if (sol_p <= 49) {
          continue;
        }
        const int pop = c->population;
        int cap = pop * (sol_p - 50) / 50;
        if (pop / 2 < cap) {
          cap = pop / 2;
        }
        if (cap < 1) {
          cap = 1;
        }
        int promoted = 0;
        const char* promoted_from = ""; /* DOS %STRING1 = pre-promote type name */
        /* bugs.md #534 (REFUTED 2026-09-20): GAME.TXT @MOBILIZE/@MOBILIZE2
         * (COLONIZE/GAME.TXT 2689-2697) name "Continental Army" twice,
         * unconditionally — there is no Dragoon variant tag and no
         * substitution token for the post-promote type, so DOS itself prints
         * "Continental Army status" for a Dragoon->Continental Cavalry
         * promote. Only %STRING1 (the PRE-promote type, "Dragoons") varies.
         * Do not retarget the text: the mechanic is already correct. */
        for (int i = 0; i < COLONIZE_UNITS_MAX && cap > 0; ++i) {
          ColonizeUnit* u = &ctx->units->units[i];
          if (!u->active || u->nation_id != human) {
            continue;
          }
          if (u->x != (int)c->x || u->y != (int)c->y) {
            continue;
          }
          /*
           * DOS gates on unit+0x3146 (raw type 1/4) and unit+0x315b == 0x15
           * only — that profession code is UNITS_JOB_SOLDIER ("Veteran
           * Soldiers"), confirmed 2026-08-14 by cross-referencing
           * FUN_43f7_1eca against the already-established 0x14=Pioneer
           * profession code from the case-8/9 terrain-improve investigation.
           * Only Veteran-status Soldier/Dragoon promote — an ordinary armed
           * colonist (profession UNITS_JOB_NONE) does not.
           * No FORTIFIED requirement: re-verified 2026-08-24 by reading the
           * complete raw FUN_43f7_1eca body (viceroy_unpacked.c:74910-74972)
           * end to end — its only two per-unit tests are the type byte at
           * unit+0x3146 and the profession byte at unit+0x315b. `orders`
           * (ViceroyUnit.orders, original_sources_annotated/include/
           * viceroy_types.h) lives at unit+0x08 (0x314c), an address this
           * function never touches. The prior `u->orders !=
           * UNITS_ORDER_FORTIFIED` gate here (removed this pass) was an
           * unsupported over-restriction — DOS promotes any Veteran-status
           * Soldier/Dragoon on the colony's own tile regardless of
           * fortified/sentry/active order state. See king_ref.md "1eca
           * Continental promote" and docs/sons_of_liberty.md (both
           * corrected 2026-08-24).
           */
          if (u->profession != UNITS_JOB_SOLDIER) {
            continue;
          }
          if (soldier_ty >= 0 && u->type_index == soldier_ty && army >= 0) {
            const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
            promoted_from = ty && ty->name[0] ? ty->name : "";
            u->type_index = army;
          } else if (dragoon_ty >= 0 && u->type_index == dragoon_ty && cav >= 0) {
            const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
            promoted_from = ty && ty->name[0] ? ty->name : "";
            u->type_index = cav;
          } else {
            continue;
          }
          --cap;
          ++promoted;
        }
        if (promoted > 0 && ctx->status && ctx->status_size) {
          PopupMsgTokens status_tok;
          memset(&status_tok, 0, sizeof(status_tok));
          status_tok.string0 = c->name[0] ? c->name : "our colony";
          if (promoted == 1) {
            status_tok.string1 = promoted_from;
            popup_msg_fill(ctx->messages, "MOBILIZE", &status_tok, "",
                           ctx->status, ctx->status_size);
          } else {
            status_tok.has_number0 = true;
            status_tok.number0 = promoted;
            popup_msg_fill(ctx->messages, "MOBILIZE2", &status_tok, "",
                           ctx->status, ctx->status_size);
          }
          popup_msg_strip_markup(ctx->status);
        }
        /* bugs.md #232: the mustering gets its own dialog per colony —
         * GAME.TXT @MOBILIZE (one unit, %STRING0 colony / %STRING1 type) or
         * @MOBILIZE2 (%NUMBER0 units). */
        if (promoted > 0 && ai_king_human_popups(ctx)) {
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = c->name[0] ? c->name : "our colony";
          char body[AI_POPUP_BODY_LEN];
          if (promoted == 1) {
            /* DOS 1eca %STRING1 = the promoted unit's pre-promote type name
             * (last FUN_281f_0438 slot-1 write), not a constant. */
            tok.string1 = promoted_from;
            popup_msg_fill(
              ctx->messages, "MOBILIZE", &tok,
              "",
              body, sizeof(body)
            );
          } else {
            tok.has_number0 = true;
            tok.number0 = promoted;
            popup_msg_fill(
              ctx->messages, "MOBILIZE2", &tok,
              "",
              body, sizeof(body)
            );
          }
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups, AI_POPUP_TAG_INFO, human, -1, promoted, NULL, body
          );
        }
      }
    }
    /* DOS 2022: mobilization turn ends the war path here. */
    return;
  }

  /*
   * bugs.md: no rebel-side auto-play. The block that used to sit here marched
   * every idle human Cont. Army / Cont. Cav toward the founding capital
   * (AI_MOVE) and fortified two of them on it. That was never DOS: it was
   * sourced from a fandom description of the Continentals plus the REF's own
   * main-port MD slack, and it ran on the *human* nation's units every war
   * turn. FUN_43f7_1eca is a promote-only routine — the full end-to-end read
   * (viceroy_unpacked.c:74910-74972, king_ref.md "1eca Continental promote")
   * shows it touching only unit+0x3146 (type) and unit+0x315b (profession);
   * it never reads or writes unit+0x08 (orders), unit+0x0a/0x0b (goto), and
   * neither does 2022 around it. The player's own Continentals are ordinary
   * player units — the King's turn must not order them anywhere. Symptom it
   * caused: two turns of pressing space after declaring independence and
   * fortified units all over the map woke up with an invisible Go To on the
   * capital.
   */

  /*
   * D1 closed 2026-09-07g: the per-unit substitute hunt that lived here
   * (MoW sail/unload arms, garrison/Artillery fortify, hunt-target pick,
   * greedy march) is deleted. DOS's king beat never moves a unit — the
   * whole 43f7 overlay contains zero writes to orders/goto/act-state
   * (grep-verified) — and the crown slot (control = 1, raw 74833) runs the
   * full Euro nation turn FUN_521d_6d8e after this beat (raw 6394/6407),
   * so REF units are moved by the ordinary 5b66/20e6 arms via
   * ai_euro_nation_turn on the crown slot (turn.c). Colony capture chrome
   * lives in the shared path (units_try_capture_foreign_colony); the crown
   * MoW sail-home beat lives in the euro ship band
   * (ai_king_mow_sail_home_20e6, called from ai_euro).
   */
}

/*
 * The DOS king beat for the crown slot, run from turn.c's EURO step for
 * that slot BEFORE ai_euro_nation_turn (per-slot order raw 6394/6407:
 * 3844_00f2 → 43f7_2424 → 2022, then the 6d8e thunk). 2022 is a pure
 * spawner/bookkeeper — wave landing (0982/06a6), Continental mobilization
 * (1eca), intervention (10f0) and merc roll, ref_present maintenance; it
 * never moves a unit. Movement follows in the ordinary euro turn.
 */
void ai_king_ref_pre_euro_beat(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 ||
      !ai_king_independence_declared(ctx->col1)) {
    return;
  }
  ai_king_ref_wave(ctx);
  ai_king_war_act(ctx);
}

/*
 * Revolution end — DOS FUN_3844_0442 (viceroy_unpacked.c 58470-58556, 58630):
 *   Win: the C1 triple, NO year gate (raw 58473-58485) — crown holds no
 *     colony, crown land units below the give-up bar, REF pool score
 *     `ef[0] + (ef[1]!=0) + (ef[3]!=0) < 4`; see the inner comment on the
 *     win block below for the full read.
 *   Lose: one @LOSING%d selector, last-write-wins over three tests (raw
 *     58507-58534) — ports==0 → 1, pop share ≥90% → 3, colonies==0 → 2, so
 *     the effective precedence is colonies, then pop share, then ports.
 *   Warn: the same digit patch on "@WARN%d" (raw 58506-58534, 58540) —
 *     `ports < 3 → 1`, `share ≥ 80 → 3`, `colonies < 3 → 2`, again last write
 *     wins — shown only when neither the win nor the lose dialog took the
 *     turn, so at most one @WARN%d per turn.
 *   Wartime calendar stop: exact year 1850 (raw 58630) → @RETIRING2.
 * Latches market_demand_pool_raw[4]; score reads won/lost.
 */
/* ===== War of Independence endgame scoring (ai_king_human_coastal_ports .. ai_king_check_revolution_end) ===== */
static int ai_king_human_coastal_ports(const ColonizeTurnContext* ctx, int human) {
  if (!ctx || human < 0 || human > 3) {
    return 0;
  }
  int n = 0;
  if (ctx->colonies && ctx->map && ctx->colonies->colony_count > 0) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (map_tile_is_coastal(ctx->map, c->x, c->y)) {
        ++n;
      }
    }
    return n;
  }
  /* Col1 colony list (smoke / bridge) when runtime pool empty. */
  if (ctx->col1_ok && ctx->col1 && ctx->map && ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != human) {
        continue;
      }
      if (map_tile_is_coastal(ctx->map, (int)c->x, (int)c->y)) {
        ++n;
      }
    }
  }
  return n;
}

/* Active human colony count (runtime pool; Col1 only if pool empty). */
int ai_king_human_colonies(const ColonizeTurnContext* ctx, int human) {
  if (!ctx || human < 0 || human > 3) {
    return 0;
  }
  if (ctx->colonies && ctx->colonies->colony_count > 0) {
    return colonies_count_for_nation(ctx->colonies, human);
  }
  int n = 0;
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      if ((int)ctx->col1->colony[i].nation_id == human) {
        ++n;
      }
    }
  }
  return n;
}

/*
 * @RETIRING2's estate colony: DOS raw 58631-58637 walks the colony list and
 * keeps the human-owned record with the highest population byte (+0x1f), then
 * splices its name (`local_6c * 0xca + 0x5d48`) into %STRING2.
 */
static const char* ai_king_richest_colony_name(const ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->colonies || human < 0) {
    return "the colonies";
  }
  const ColonizeColony* best = NULL;
  int best_pop = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    if ((int)c->population >= best_pop) { /* raw 58636 `cVar1 <= pop`: last max wins */
      best_pop = (int)c->population;
      best = c;
    }
  }
  if (best && best->name[0] != '\0') {
    return best->name;
  }
  return "the colonies";
}

/*
 * Crown share of (human+crown) colony population during WoI (@WARN3 /
 * @LOSING3, %NUMBER2).
 *
 * DOS FUN_3844_0442 (viceroy_unpacked.c:58507-58521) reads the census
 * mirror, NOT the colony list:
 *   uVar3 = colony_pop_totals[*0x53d2]   (crown)
 *   uVar4 = colony_pop_totals[*0x5398]   (human)
 *   local_8 = (uVar3 + 1) * 100 / ((uVar4 + 1) + (uVar3 + 1))
 * −0x6bf4 = DS:0x940c = stuff.colony_pop_totals[] (save_format_map.md
 * row 243); the two `if (byte == 0) x = ~byte + 1; else x = byte;` arms in
 * the raw are a sign-extension artifact — both spell the plain byte.
 *
 * The +1 on BOTH sides is load-bearing: human 1 / crown 9 is 83% in DOS
 * (a @WARN3) but was 90% (instant @LOSING3 surrender) under the old
 * per-colony `pop>0?pop:1` re-sum. Ported 2026-09-10 (audit D7). With no
 * population at all DOS answers 50, not 0.
 *
 * The mirror is refreshed every turn by
 * col1_stuff_census_refresh_colony_counts (turn.c); a blank fixture window
 * (both rows zero) falls back to summing the live pools before the +1s.
 */
static int ai_king_woi_pop_share_pct(
  const ColonizeTurnContext* ctx,
  int human,
  int crown
) {
  if (!ctx || human < 0 || human >= 4 || crown < 0 || crown >= 4) {
    return 0;
  }
  int human_pop = 0;
  int crown_pop = 0;
  if (ctx->col1_ok && ctx->col1) {
    human_pop = (int)ctx->col1->stuff.colony_pop_totals[human];
    crown_pop = (int)ctx->col1->stuff.colony_pop_totals[crown];
  }
  if (human_pop == 0 && crown_pop == 0) {
    /* Blank census window (synthetic fixtures): re-tally the same sum. */
    if (ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active) {
          continue;
        }
        const int pop = c->population > 0 ? (int)c->population : 0;
        if (c->nation_id == human) {
          human_pop += pop;
        } else if (c->nation_id == crown) {
          crown_pop += pop;
        }
      }
    }
    if (human_pop == 0 && crown_pop == 0 && ctx->col1_ok && ctx->col1 &&
        ctx->col1->colony) {
      for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
        const ColonizeCol1Colony* c = &ctx->col1->colony[i];
        const int pop = c->population > 0 ? (int)c->population : 0;
        if ((int)c->nation_id == human) {
          human_pop += pop;
        } else if ((int)c->nation_id == crown) {
          crown_pop += pop;
        }
      }
    }
  }
  return ((crown_pop + 1) * 100) / ((human_pop + 1) + (crown_pop + 1));
}

/*
 * Full-screen throne audience follow-up to the war-end announcement (DOS
 * FUN_3844_0442 → FUN_291f_0aba → FUN_75c2_20e2): the King's parting word.
 * win: @KINGLOSE text on the KINGLOSE.SS king; loss: @KINGWIN on KINGWIN.SS
 * (%STRING0 = the mother country). game_loop renders the KING_THRONE tag as
 * the full-screen KINGLSS audience and opens the retire score on dismissal.
 */
static void ai_king_enqueue_throne_audience(
  ColonizeTurnContext* ctx,
  int human,
  int crown,
  int win
) {
  if (!ai_king_human_popups(ctx)) {
    return;
  }
  const char* motherland =
    (human >= 0 && human <= 3) ? reports_nation_country_name(human) : "the Crown";
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  char body[AI_POPUP_BODY_LEN];
  if (win) {
    popup_msg_fill(ctx->messages, "KINGLOSE", &tok, "", body, sizeof(body));
  } else {
    tok.string0 = motherland;
    popup_msg_fill(ctx->messages, "KINGWIN", &tok, "", body, sizeof(body));
  }
  (void)ai_popup_enqueue_ok_ctx(
    ctx->ai_popups, AI_POPUP_TAG_KING_THRONE, human, crown, win ? 1 : 2, NULL, body
  );
}

/*
 * GAME.TXT @LOSING1/2/3 emitter. The three loss branches below (all ports
 * taken / all colonies taken / >=90% population) ran byte-identical 28-line
 * blocks that differed only in the tag and the fallback wording: latch
 * ENDGAME_LOST, tok.string0/1/2 = country / leader / exile, popup_msg_fill,
 * overwrite ctx->status with the filled body, queue the human OK with payload
 * 4, then the throne audience. Callers pass an already-formatted fallback
 * because each branch's wording takes a different mix of the three strings.
 */
static void ai_king_emit_loss(
  ColonizeTurnContext* ctx,
  const char* tag,
  const char* fallback,
  int human,
  int crown,
  const char* country,
  const char* leader,
  const char* exile
) {
  ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = country;
  tok.string1 = leader;
  tok.string2 = exile;
  ai_king_emit_ok(
    ctx, tag, &tok, fallback, AI_POPUP_TAG_KING_WAR_END, human, crown, 4, 1, NULL, 0
  );
  /* DOS lose order: @LOSINGn dialog, then the @KINGWIN gloating audience
   * (291f_0aba(2,1,0xf31)); the retire score follows its dismissal. */
  ai_king_enqueue_throne_audience(ctx, human, crown, 0);
}

/*
 * DOS FUN_3844_0442 fills all three number slots once, before it decides
 * which mid-war warn popup to show (viceroy_unpacked.c:58552-58556):
 *   FUN_281f_09ae(0, local_68)                 — coastal-port count
 *   FUN_281f_09ae(1, colony_counts[human])     — colonies still held
 *   FUN_281f_09ae(2, local_8)                  — crown pop share %
 * @WARN1 reads %NUMBER0, @WARN2 %NUMBER1, @WARN3 %NUMBER2 (GAME.TXT), so
 * every warn body gets the same live triple. Ported 2026-09-10 (audit D13);
 * @WARN1/@WARN2 used to hardcode 1 and read "all but 1" with 2 ports left.
 */
static void ai_king_warn_numbers(PopupMsgTokens* tok, int ports, int colonies, int pop_pct) {
  if (!tok) {
    return;
  }
  tok->has_number0 = true;
  tok->number0 = ports;
  tok->has_number1 = true;
  tok->number1 = colonies;
  tok->has_number2 = true;
  tok->number2 = pop_pct;
}

/*
 * DOS gate for the whole win/lose/warn group: `(*0x5382 & 1) != 0 &&
 * (*0x5382 & 8) == 0` (raw 58505) — WoI declared, war not already resolved.
 * No REF-present term, so this takes no `ref_already` argument any more
 * (2026-09-10 audit lead 5).
 */
typedef enum {
  AI_KING_WOI_END_CONTINUE = 0, /* stage fell through — run the next one */
  AI_KING_WOI_END_DONE = 1      /* stage ended the check (was a bare `return;`) */
} AiKingWoiEndStatus;

/* Shared state for the stages of one ai_king_check_revolution_end pass. */
struct ai_king_woi_end_ctx {
  ColonizeTurnContext* ctx;
  int human;
  int crown;
  int ports;
  int colonies;
  int pop_pct;
  int year;
  int warn_sel;
  const char* country;
  const char* leader;
};

/*
 * LOSE arms (@LOSING1/2/3, DOS FUN_3844_0442 raw 58507-58534). Extracted
 * verbatim from ai_king_check_revolution_end.
 */
static AiKingWoiEndStatus ai_king_woi_end_lose(struct ai_king_woi_end_ctx* w) {
  ColonizeTurnContext* const ctx = w->ctx;
  const int human = w->human;
  const int crown = w->crown;
  const int ports = w->ports;
  const int colonies = w->colonies;
  const int pop_pct = w->pop_pct;
  const char* const country = w->country;
  const char* const leader = w->leader;

  /*
   * Lose: same digit-patch selector on "@LOSING%d" (DS:0xf29 = "LOSING0"),
   * three tests overwriting each other in source order (raw 58507-58534):
   * `ports == 0 → 1`, `share >= 90 → 3`, `colonies == 0 → 2`. Last write
   * wins, so the branch order here has to be the reverse: colonies, then pop
   * share, then ports.
   *
   * @LOSING%d %STRING2 is `FUN_291f_0ac8(2, 0, *0x53d4)` (raw 58538) — the
   * COUNTRY name of rival slot 1, the intervention ally the deposed viceroy
   * flees to. All three branches used to hardcode "Europe" there.
   */
  const int exile_nation = ai_king_intervention_nation_slot(ctx, human, 0);
  const char* exile = (exile_nation >= 0 && exile_nation < 4)
                        ? reports_nation_country_name(exile_nation)
                        : "Europe";
  if (colonies <= 0) {
    ai_king_emit_loss(ctx, "LOSING2", "", human, crown, country, leader, exile);
    return AI_KING_WOI_END_DONE;
  }
  /*
   * Lose: crown controls ≥90% of human+crown colony population.
   * GAME.TXT @LOSING3 — outranks the ports test (raw 58526-58527 writes 3
   * after 58507 wrote 1).
   */
  if (pop_pct >= AI_KING_LOSING3_PCT) {
    ai_king_emit_loss(ctx, "LOSING3", "", human, crown, country, leader, exile);
    return AI_KING_WOI_END_DONE;
  }
  if (ports <= 0) {
    ai_king_emit_loss(ctx, "LOSING1", "", human, crown, country, leader, exile);
    return AI_KING_WOI_END_DONE;
  }
  return AI_KING_WOI_END_CONTINUE;
}

/*
 * WIN arm (@KINGLOSE, DOS FUN_3844_0442 C1 raw 58468-58497). Extracted
 * verbatim from ai_king_check_revolution_end.
 */
static AiKingWoiEndStatus ai_king_woi_end_win(struct ai_king_woi_end_ctx* w) {
  ColonizeTurnContext* const ctx = w->ctx;
  const int human = w->human;
  const int crown = w->crown;
  const ColonizeCol1Player* const pl = &ctx->col1->player[human];
  const char* const leader = w->leader;

  /*
   * WIN — full DOS FUN_3844_0442 C1 (viceroy_unpacked.c 58468-58497,
   * @KINGLOSE emitter found via EXE DS-string scan, tag 0xf20):
   *   1. crown holds no colony (`*(0x53d2 - 0x6d68) == 0`);
   *   2. crown LAND force on the map — types 6/8/0xb Regulars/Cavalry/
   *      Artillery only, ships never count — is below the give-up bar:
   *      <1 normally, <8 once bit 0x40 (crown captured a colony this war,
   *      game_options.ref_unit_threshold) is set;
   *   3. REF pool score `ef[0] + (ef[1]!=0) + (ef[3]!=0) < 4` — the MoW
   *      pool (ef[2]) is ignored, and up to 3 pooled Regulars still allow
   *      the concession.
   * game_options.independence_force (0x5382 bit 0x20, the cheat) bypasses
   * gates 2 and 3 and the colony gate, as in DOS. No year gate.
   */
  const int crown_colonies = colonies_count_for_nation(ctx->colonies, crown);
  int crown_land = 0;
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != crown) {
        continue;
      }
      /* DOS counts types 6/8/0xb — Regulars / Cavalry / Artillery. Keyed off
       * the @UNIT code rather than the pool slot (synthetic test pools reorder
       * type indices); units_type_kind's own name table draws the
       * Cavalry/Cont. Cav. line the hand-rolled strstr pair used to. */
      const ColonizeUnitKind k = units_type_kind(units_type(ctx->units, u->type_index));
      if (k == UNITS_KIND_REGULAR || k == UNITS_KIND_CAVALRY ||
          k == UNITS_KIND_ARTILLERY) {
        ++crown_land;
      }
    }
  }
  const int force_end = ctx->col1->head.game_options.independence_force != 0;
  const int giveup_bar = ctx->col1->head.game_options.ref_unit_threshold ? 8 : 1;
  const uint16_t* ef = ctx->col1->head.expeditionary_force;
  const int pool_score = (int)ef[0] + (ef[1] != 0 ? 1 : 0) + (ef[3] != 0 ? 1 : 0);
  if ((crown_colonies <= 0 || force_end) &&
      (crown_land < giveup_bar || force_end) && (pool_score < 4 || force_end)) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_WON);
    ai_king_latch_set(ctx->col1, AI_KING_REF_PRESENT_BYTE, 0);
    /* DOS win sequence: 0x5382|=8 (war concluded), reveal map, scoring
     * latch — mirrors turn.c C1's LAB_0b4a effects so the win is complete
     * whichever check fires first. */
    ctx->col1->head.game_options.independence_chrome = 1;
    ctx->col1->head.show_entire_map = 1;
    const char* country =
      (pl->country_name[0] != '\0') ? pl->country_name : "the colonies";
    char body[AI_POPUP_BODY_LEN];
    /* DOS 3844_0442 win order: victory tune pool (FUN_129f_0318(3)), the
     * @WINNING announcement, THEN the @KINGLOSE throne audience (bugs.md #258
     * — the first pass had the two inverted). */
    if (ai_king_human_popups(ctx)) {
      sound_set_bgm(3);
    }
    /* 1st: @WINNING — STRING0 leader, STRING1 country. Payload 1 = win; the
     * retire score waits for the throne audience behind it. */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = leader;
    tok.string1 = country;
    popup_msg_fill(ctx->messages, "WINNING", &tok, "", body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_WAR_END, human, crown, 1, NULL, body
      );
    }
    /* 2nd: @KINGLOSE — the King's parting word as the full-screen audience
     * (DOS 291f_0aba(1,2,0xf20)); dismissal opens the retire score chain. */
    ai_king_enqueue_throne_audience(ctx, human, crown, 1);
    return AI_KING_WOI_END_DONE;
  }
  return AI_KING_WOI_END_CONTINUE;
}

/*
 * The one @WARN%d the selector picked, plus the 1850 wartime calendar end
 * (@RETIRING2). Extracted verbatim from ai_king_check_revolution_end.
 */
static void ai_king_woi_end_warn(struct ai_king_woi_end_ctx* w) {
  ColonizeTurnContext* const ctx = w->ctx;
  const int human = w->human;
  const int crown = w->crown;
  const int ports = w->ports;
  const int colonies = w->colonies;
  const int pop_pct = w->pop_pct;
  const int year = w->year;
  const int warn_sel = w->warn_sel;
  const char* const country = w->country;
  const char* const leader = w->leader;
  (void)country;

  /*
   * The war did not end this turn — show the ONE warn the selector picked
   * (DOS raw 58538-58551, reached only when neither the win nor the lose
   * dialog jumped out of the block). %STRING0 is the human's new-world
   * country name (`0x5398 * 0x34 + 0x5426`, raw 58553) and all three number
   * slots are filled for every warn body (raw 58554-58556).
   */
  if (warn_sel > 0) {
    const int warn_byte = (warn_sel == 2)   ? AI_KING_WARN2_BYTE
                          : (warn_sel == 3) ? AI_KING_WARN3_BYTE
                                            : AI_KING_WARN1_BYTE;
    if (ai_king_latch_get(ctx->col1, warn_byte) == 0) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      ai_king_warn_numbers(&tok, ports, colonies, pop_pct);
      tok.string0 = country;
      char tag[16];
      snprintf(tag, sizeof(tag), "WARN%d", warn_sel);
      /*
       * Do not clobber same-turn wave/war_act status (1528 @INVASION, 2244
       * merc). The warn still enqueues its INFO OK; status when buffer empty.
       */
      ai_king_emit_ok(
        ctx, tag, &tok, "", AI_POPUP_TAG_INFO, human, crown, warn_sel, 0, NULL, 0
      );
      ai_king_latch_set(ctx->col1, warn_byte, 1);
    }
  }
  /*
   * Wartime calendar end. DOS raw 58630:
   *   if (((year == 0x708) && !woi) || (year == 0x73a)) { ...retire... }
   * — under a declared WoI (this whole function's precondition) only the
   * `year == 1850` arm can fire, with NO crown-units term: the crown holding
   * colonies but zero live units used to run past 1850 forever here. Kept as
   * `>=` because DOS's own 1850 arm is unconditional (the year word 0x538a
   * can never step past it before the score chain runs), so `>=` differs from
   * `==` only for a port state that already overshot.
   * GAME.TXT @RETIRING2.
   */
  if (year >= AI_KING_YEAR_CAP) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    ai_king_latch_set(ctx->col1, AI_KING_REF_PRESENT_BYTE, 0);
    const char* estate = ai_king_richest_colony_name(ctx, human);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    /* raw 58635 `FUN_281f_0438(0, *(0x53a6 * 2 - 0x7c6c))`: %STRING0 is the
     * DIFFICULTY title, not a fixed "Viceroy" (same table 38fd_5930 uses at
     * raw 68388). */
    const int diff = (int)ctx->col1->head.difficulty;
    tok.string0 = reports_difficulty_title((diff >= 0 && diff < 5) ? diff : 4);
    tok.string1 = leader;
    tok.string2 = estate;
    ai_king_emit_ok(
      ctx, "RETIRING2", &tok, "", AI_POPUP_TAG_KING_WAR_END, human, crown, 2, 1,
      NULL, 0
    );
  }
}

static void ai_king_check_revolution_end(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  if (ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) != AI_KING_ENDGAME_NONE) {
    return; /* already resolved */
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  const int ports = ai_king_human_coastal_ports(ctx, human);
  const int colonies = ai_king_human_colonies(ctx, human);
  const int pop_pct = ai_king_woi_pop_share_pct(ctx, human, crown);
  const ColonizeCol1Player* pl = &ctx->col1->player[human];
  const char* country =
    (pl->country_name[0] != '\0') ? pl->country_name : "the colonies";
  const char* leader = (pl->name[0] != '\0') ? pl->name : "";
  /*
   * Mid-war warn selector — DOS FUN_3844_0442 builds the warn tag the same
   * way it builds the lose tag: one digit patched into a base name
   * (`FUN_1d1d_07e4(local_58, 0xf39)` loads DS:0xf39 = "WARN0", then
   * `local_54 = local_54 + cVar1`, raw 58540-58541), with the three tests
   * overwriting each other in source order (raw 58506-58534):
   *     cVar1 = (ports < 3);                       → @WARN1
   *     if (0x4f < share)  cVar1 = 3;              → @WARN3   (raw 58524)
   *     if (colonies < 3)  cVar1 = 2;              → @WARN2   (raw 58530)
   * Last write wins, so the precedence is colonies, then pop share, then
   * ports, and DOS shows at most ONE @WARN%d per turn (none at cVar1 == 0).
   * The port used to latch and fire all three independently.
   *
   * The lose dialog leaves the block (`goto LAB_3844_04ec`, raw 58548) and the
   * win dialog leaves it at raw 58500, so a turn that ends the war shows no
   * warn at all — the emission below therefore sits after both.
   *
   * The whole lose/warn group is gated only by `(*0x5382 & 1) != 0 &&
   * (*0x5382 & 8) == 0` (raw 58505) — WoI declared and the war not already
   * resolved, the two conditions this function tests at its head. There is NO
   * REF-present term in DOS; the port's extra `ref_already` gate kept the
   * whole group silent until the first wave had landed.
   */
  int warn_sel = (ports < 3) ? 1 : 0;
  if (pop_pct >= AI_KING_WARN3_PCT_MIN) { /* raw 58524 `0x4f < local_8` */
    warn_sel = 3;
  }
  if (colonies < 3) {
    warn_sel = 2;
  }
  /*
   * Episode latches (port-side; DOS re-shows the selected warn every turn the
   * condition holds — see the audit lead). Each clears when its own band is
   * left, so a later relapse re-fires.
   */
  if (ports >= 3) {
    ai_king_latch_set(ctx->col1, AI_KING_WARN1_BYTE, 0);
  }
  if (colonies >= 3) {
    ai_king_latch_set(ctx->col1, AI_KING_WARN2_BYTE, 0);
  }
  if (pop_pct < AI_KING_WARN3_PCT_MIN) {
    ai_king_latch_set(ctx->col1, AI_KING_WARN3_BYTE, 0);
  }
  const int year = (int)ctx->col1->head.year;

  struct ai_king_woi_end_ctx w;
  memset(&w, 0, sizeof(w));
  w.ctx = ctx;
  w.human = human;
  w.crown = crown;
  w.ports = ports;
  w.colonies = colonies;
  w.pop_pct = pop_pct;
  w.year = year;
  w.warn_sel = warn_sel;
  w.country = country;
  w.leader = leader;

  if (ai_king_woi_end_lose(&w) == AI_KING_WOI_END_DONE) {
    return;
  }
  if (ai_king_woi_end_win(&w) == AI_KING_WOI_END_DONE) {
    return;
  }
  ai_king_woi_end_warn(&w);
}

/* ===== Nation turn driver & popup result dispatch (ai_king_nation_turn .. ai_king_apply_popup_result) ===== */
void ai_king_nation_turn(ColonizeTurnContext* ctx) {
  if (!ctx) {
    return;
  }
  /*
   * FUN_43f7_2424-shaped:
   *   SoL → peacetime (1d42 tax, SoL chrome, 2564/1a26 declare) | wartime (2022 wave+act)
   */
  /*
   * The lose/@WARN group used to be armed here by a port-invented
   * "WoI + REF-present at turn entry" precondition; DOS gates it on
   * `0x5382 & 1 && !(0x5382 & 8)` alone (raw 58505), which
   * ai_king_check_revolution_end tests for itself.
   */
  /* External boycott clear (Fugger/diplo) → drop refuse even mid-war / off-tax years. */
  if (ctx->col1_ok && ctx->col1) {
    ai_king_sync_boycott_refuse(ctx->col1, ctx->human_nation);
  }
  const int sol = ai_king_sol_percent(ctx, ctx->human_nation);

  if (!ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
    /* DOS 1b3a SoL cache tail: `0x31 < SoL && *0x53d2 < 0` → the War of the
     * Spanish Succession vacates the King's future slot ahead of time. */
    if (sol > 49) {
      ai_king_succession(ctx);
    }
    /* FUN_43f7_2424: 1d42 runs FIRST on the peacetime path — the royal
     * purse stipend + threshold @KINGBUY pool buy (real 1d42; the tax
     * audience below is the separate 38fd_5be8 machinery). */
    ai_king_1d42_royal_purse(ctx);
    const int popups_before = ctx->ai_popups ? ctx->ai_popups->queue_count : 0;
    ai_king_tax_event(ctx);
    /* 38fd Europe-EOT king slot: @KINGNEWWAR only when the tax event stayed quiet. */
    if (!ctx->ai_popups || ctx->ai_popups->queue_count == popups_before) {
      (void)ai_king_new_war_event(ctx);
    }
    /* FUN_3844_00f2 tail: @KINGFRIGATE every 8th peacetime turn. */
    ai_king_frigate_offer(ctx, ctx->human_nation);
    /*
     * Peacetime Spring 1790 anniversary (year_end_chrome 0x6fe): @SOONRETIRING0
     * once before the 1800 @SCORED latch. Cite: turn/year_end_chrome.md.
     */
    if (ctx->col1_ok && ctx->col1 &&
        ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) == AI_KING_ENDGAME_NONE &&
        ai_king_latch_get(ctx->col1, AI_KING_SOONRETIRE0_BYTE) == 0 &&
        (int)ctx->col1->head.year == AI_KING_SOONRETIRE0_YEAR &&
        !(ctx->game_autumn && *ctx->game_autumn != 0)) {
      const int human = ctx->human_nation;
      const char* leader =
        (human >= 0 && human < 4 && ctx->col1->player[human].name[0] != '\0')
          ? ctx->col1->player[human].name
          : "";
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      /* raw 58622 `FUN_281f_0438(0, *(0x53a6 * 2 - 0x7c6c))`: %STRING0 is the
       * DIFFICULTY title, not a fixed "Viceroy" — the same splice @RETIRING2
       * makes at raw 58643 (0x53a6 = the difficulty byte). */
      const int diff = (int)ctx->col1->head.difficulty;
      tok.string0 = reports_difficulty_title((diff >= 0 && diff < 5) ? diff : 4);
      tok.string1 = leader;
      ai_king_emit_ok(
        ctx, "SOONRETIRING0", &tok, "", AI_POPUP_TAG_INFO, human,
        ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
        AI_KING_SOONRETIRE0_YEAR, 0, NULL, 0
      );
      ai_king_latch_set(ctx->col1, AI_KING_SOONRETIRE0_BYTE, 1);
    }
    /*
     * Peacetime calendar end (manual pp.10–12 / 1800–1850): without WoI,
     * year≥1800 latches once. Cite: docs/manual_gap.md Auto-end.
     */
    if (ctx->col1_ok && ctx->col1 &&
        ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) == AI_KING_ENDGAME_NONE &&
        (int)ctx->col1->head.year >= AI_KING_PEACE_YEAR_CAP) {
      ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_PEACE_1800);
      /* GAME.TXT @SCORED — peacetime calendar end (invent Colonial Era Ends demoted). */
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        ctx->messages,
        "SCORED",
        &tok,
        "",
        body,
        sizeof(body)
      );
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size, "%s", body);
      }
      if (ai_king_human_popups(ctx)) {
        char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
        const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "SCORED");
        int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
        const char* labels[2];
        const int ids[] = {AI_KING_CHOICE_THATS_ALL, AI_KING_CHOICE_KEEP_PLAYING};
        if (nch >= 2) {
          labels[0] = choice_buf[0];
          labels[1] = choice_buf[1];
        } else {
          labels[0] = "";
          labels[1] = "";
        }
        (void)ai_popup_enqueue_choice_ctx(
          ctx->ai_popups,
          AI_POPUP_TAG_KING_SCORED,
          ctx->human_nation,
          ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation),
          AI_KING_PEACE_YEAR_CAP,
          NULL,
          body,
          labels,
          ids,
          2
        );
      }
    }
    /*
     * FUN_43f7_2424 tail: decile SoL notify (DS:0x53d8 dedup), full port
     * 2026-09-06 (was status-only with an invented founding_father_count
     * gate). DOS (viceroy_unpacked.c:75182-75211):
     *   gate: census_pop_proxy[human] > 3 (DS:nation-0x6bf0 = 0x9410 — the
     *     population proxy, NOT an SoL cache as older notes claimed);
     *   rising: 0x53d8 < report/10 -> @REBELUP (0x1362, SoL<50) or
     *     @REBELUP50 (0x1358, SoL>=50), %NUMBER0 = SoL, %STRING0 = the
     *     human's parent country (FUN_291f_0ac8 form-0 name);
     *   falling: 0x53d8 > (report+4)/10 -> @REBELDOWN (0x136a) — note the
     *     +4 hysteresis (a one-decile dip does not re-notify);
     *   both paths then write 0x53d8 = report/10.
     */
    if (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4 &&
        (int)ctx->col1->stuff.census_pop_proxy[ctx->human_nation] > 3) {
      const int last = (int)ctx->col1->head.sol_pct_last_notified;
      const int rising = last < sol / 10;
      const int falling = !rising && last > (sol + 4) / 10;
      if (rising || falling) {
        const char* country = reports_nation_country_name(ctx->human_nation & 3);
        if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
          snprintf(
            ctx->status,
            ctx->status_size,
            rising ? "Congress notes rising Sons of Liberty (%d%%)."
                   : "Congress notes falling Sons of Liberty (%d%%).",
            sol
          );
        }
        if (ai_king_human_popups(ctx)) {
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.has_number0 = true;
          tok.number0 = sol;
          tok.string0 = country;
          const char* section = rising ? (sol >= 50 ? "REBELUP50" : "REBELUP") : "REBELDOWN";
          char body[AI_POPUP_BODY_LEN];
          popup_msg_fill(ctx->messages, section, &tok, "", body, sizeof(body));
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups, AI_POPUP_TAG_INFO, ctx->human_nation,
            ai_king_crown_nation_col1(ctx->col1, ctx->human_nation), sol, NULL, body
          );
        }
        ctx->col1->head.sol_pct_last_notified = (int16_t)(sol / 10);
      }
    }
    /*
     * Thin pre-declare SoL chrome:
     * SoL AI_KING_RESTLESS_SOL_MIN..(DECLARE_MIN-1) → restless status line
     * before the auto-declare gate. market_demand_pool_raw consistency: do not set WoI[0] /
     * congress[5] here (declare only). Optional tax mention when tax_rate
     * already in the refuse band (≥20) — reads existing tax_rate; no invented
     * tax formula. Do not clobber thin 38fd_5be8 tax audience / hike status
     * from 1d42 (ai_popup CHOICE when queue attached). (2564 in try_declare.)
     */
    if (sol >= AI_KING_RESTLESS_SOL_MIN && sol < AI_KING_DECLARE_SOL_MIN && ctx->status &&
        ctx->status_size) {
      const int keep_tax_audience =
          strstr(ctx->status, "refuse") || strstr(ctx->status, "Audience") ||
          strstr(ctx->status, "raises taxes") || strstr(ctx->status, "Tax stays") ||
          strstr(ctx->status, "Congress notes"); /* decile SoL notify above; don't clobber */
      if (!keep_tax_audience) {
        const uint8_t tax =
            (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4)
                ? ctx->col1->nation[ctx->human_nation].tax_rate
                : 0;
        if (tax >= AI_KING_BOYCOTT_TAX_MIN) {
          snprintf(ctx->status, ctx->status_size,
                   "Sons of Liberty grow restless (%d%%). Tax is at %u%%.", sol, tax);
        } else {
          snprintf(ctx->status, ctx->status_size, "Sons of Liberty grow restless (%d%%).", sol);
        }
        /* Restless: status only (no invented wood OK). */
      }
    }
    ai_king_try_declare(ctx);
  }

  if (ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
    /*
     * Wartime 1840 anniversary (year_end_chrome 0x730): @SOONRETIRING1 once.
     * Any season while WoI; does not latch endgame.
     */
    if (ctx->col1_ok && ctx->col1 &&
        ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) == AI_KING_ENDGAME_NONE &&
        ai_king_latch_get(ctx->col1, AI_KING_SOONRETIRE1_BYTE) == 0 &&
        (int)ctx->col1->head.year == AI_KING_SOONRETIRE1_YEAR) {
      const int human = ctx->human_nation;
      const char* leader =
        (human >= 0 && human < 4 && ctx->col1->player[human].name[0] != '\0')
          ? ctx->col1->player[human].name
          : "";
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      /* Same emitter as @SOONRETIRING0 (raw 58618-58628): %STRING0 is the
       * difficulty title, %STRING1 the leader. The 1840 body reads only
       * %STRING1, but DOS fills both slots. */
      {
        const int diff = (int)ctx->col1->head.difficulty;
        tok.string0 = reports_difficulty_title((diff >= 0 && diff < 5) ? diff : 4);
      }
      tok.string1 = leader;
      ai_king_emit_ok(
        ctx, "SOONRETIRING1", &tok, "", AI_POPUP_TAG_INFO, human,
        ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
        AI_KING_SOONRETIRE1_YEAR, 0, NULL, 0
      );
      ai_king_latch_set(ctx->col1, AI_KING_SOONRETIRE1_BYTE, 1);
    }
    /*
     * D1 (2026-09-07g): the wave + war bookkeeping moved to
     * ai_king_ref_pre_euro_beat, run from the crown slot's own EURO step
     * BEFORE ai_euro_nation_turn — DOS's per-slot order (00f2→2424 king
     * beat, then 6d8e; raw 6394/6407). This king slice keeps only the
     * chrome above and the end check, which DOS also evaluates after the
     * movement it observes.
     */
    ai_king_check_revolution_end(ctx);
  }

  if (ctx->active_turn_nation) {
    *ctx->active_turn_nation = ctx->human_nation;
  }
  if (ctx->col1_ok && ctx->col1) {
    /*
     * FUN_43f7_2424 (43f7:2478..2492): every nation caches its own SoL into
     * `nation + 0x19` — the byte the Foreign Affairs report multiplies the
     * census population by to split Rebels from Tories (3f41:2902 reads
     * `[nation*0x13c + 0x8821]`, IMULs it by `census_pop_proxy`, IDIVs by
     * 100). Only the human's copy in DS:0x53d0 was being written here, so
     * `rebel_sentiment` stayed 0 for the whole campaign and the report showed
     * 0 Rebels / all Tories even at 100% SoL (bugs.md). DOS writes the byte
     * for whichever nation it is ticking, human or AI, before the DS:0x53d0
     * human-only half.
     */
    for (int n = 0; n < 4; ++n) {
      int sol = ai_king_sol_percent(ctx, n);
      if (sol < 0) {
        sol = 0;
      }
      if (sol > 100) {
        sol = 100;
      }
      ctx->col1->nation[n].rebel_sentiment = (uint8_t)sol;
    }
    /* FUN_43f7_2424 tail: cache nation SoL for next turn's tax-audience score. */
    ctx->col1->head.rebel_sentiment_report =
      (uint8_t)ai_king_sol_percent(ctx, ctx->human_nation);
  }
}

void ai_king_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup) {
  if (!ctx || !popup || !popup->has_result || popup->result_cancelled) {
    return;
  }
  const int human = (popup->result_nation_a >= 0 && popup->result_nation_a < 4)
                      ? popup->result_nation_a
                      : ctx->human_nation;
  switch (popup->result_tag) {
    case AI_POPUP_TAG_KING_AUDIENCE:
      /*
       * FUN_38fd_3dc8 village-goods choice. The hike is NOT applied yet
       * (bugs.md: the popup proposes it, the answer settles it). Accept
       * ("kiss the ring") → commit the delta packed in the payload. Refuse
       * ("tea party") → leave the rate alone and boycott the picked cargo.
       */
      {
        int applied = 0;
        int cargo = -1;
        ai_king_teaparty_payload_parts(popup->result_payload, &applied, &cargo);
        if (popup->result_choice_id == AI_KING_CHOICE_ACCEPT) {
          ai_king_tax_commit(ctx, human, applied);
          if (ctx->status && ctx->status_size && ctx->col1_ok && ctx->col1 &&
              human >= 0 && human < 4) {
            snprintf(ctx->status, ctx->status_size,
                     "Audience: taxes raised to %u%%.",
                     ctx->col1->nation[human].tax_rate);
          }
        } else if (popup->result_choice_id == AI_KING_CHOICE_REFUSE) {
          ai_king_tax_teaparty(ctx, human, cargo);
        }
      }
      break;
    case AI_POPUP_TAG_KING_DUMP_GOODS:
      /* Dump-goods modal: choice_id is cargo index to OR into boycott_bitmap. */
      ai_king_apply_dump_goods_choice(ctx, human, popup->result_choice_id);
      break;
    case AI_POPUP_TAG_KING_MERC:
      /* FUN_43f7_2022 rebel branch: Hire → spend the rolled price, spawn at
       * the offer-time landing pick (payload); Decline → status only, no
       * gate — DOS has no once-per-war flag, next turn may roll again. */
      if (popup->result_choice_id == AI_KING_CHOICE_HIRE) {
        int hx = 0;
        int hy = 0;
        int qty_a = 0;
        int extra_flag = 0;
        int price = 0;
        ai_king_merc_payload_parts(popup->result_payload, &hx, &hy, &qty_a, &extra_flag, &price);
        if (!ai_king_do_merc_hire_at(ctx, human, hx, hy, qty_a, extra_flag, price) &&
            ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "Cannot afford mercenaries.");
        }
      } else if (popup->result_choice_id == AI_KING_CHOICE_DECLINE) {
        if (ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "Mercenaries declined.");
        }
      }
      break;
    case AI_POPUP_TAG_KING_MERC_PEACE:
      /* FUN_43f7_2244 @MERCENARIES: Pay → debit + 10f0 paid landing at the
       * roulette-picked colony (DOS picks at accept time too); No thank you
       * → nothing, the 1-in-21 roll may hit again next turn. */
      if (popup->result_choice_id == AI_KING_CHOICE_HIRE) {
        int regular = 0;
        int artillery = 0;
        int price = 0;
        ai_king_merc_peace_payload_parts(popup->result_payload, &regular, &artillery, &price);
        if (!ai_king_do_merc_peace_hire(ctx, human, regular, artillery, price) &&
            ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "Cannot afford mercenaries.");
        }
      } else if (popup->result_choice_id == AI_KING_CHOICE_DECLINE) {
        if (ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "Mercenaries declined.");
        }
      }
      break;
    case AI_POPUP_TAG_KING_FRIGATE:
      /* FUN_3844_00f2 tail: Yes → Frigate sails from Europe + 3dc8(KINGTAX, 10). */
      if (popup->result_choice_id == AI_KING_CHOICE_ACCEPT) {
        ai_king_frigate_accept(ctx, human);
      } else if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size, "The Crown's frigate is declined.");
      }
      break;
    case AI_POPUP_TAG_KING_CONGRESS:
      /* FUN_43f7_2564 / 1a26: Confirm → declare; Not yet → leave peacetime. */
      if (popup->result_choice_id == AI_KING_CHOICE_CONFIRM) {
        ai_king_do_declare(ctx, human);
        /*
         * bugs.md #532: NO same-turn Continental muster. DOS reaches 1eca
         * only through FUN_43f7_2022 called with the *human* slot
         * (viceroy_unpacked.c:75002-75006: `param_1 != *0x53d2` arm →
         * `if ((*(byte *)*0x84fc & 8) == 0) { 2a1f_00c4 = 1eca; set bit 8;
         * return; }`), and 2022 is only ever called from FUN_43f7_2424
         * (raw 75209), which in turn is only called from FUN_3844_00f2
         * (raw 58392). 00f2 runs per slot at the START of that slot's turn
         * (raw 6394: `FUN_281f_0644(slot)` = 3844_00f2, immediately before
         * the slot's own turn body 0668/062c — human — or 0638 = 6d8e — AI).
         * The human's 00f2 for the declaring turn has therefore already run
         * by the time 1a26 fires from the Congress confirm, so the muster
         * cannot happen before the human's NEXT turn start. 2424 also latches
         * the WoI bit once at entry (`if ((*0x5382 & 1) == 0) {peacetime,
         * incl. 2564/1a26 declare} else if (bVar1) {2022}`), so an
         * auto-declare can never fall through into 2022 in the same call
         * either.
         *
         * The REF wave half is unchanged: ai_king_ref_wave here only burns
         * the port's one-turn AI_KING_REF_WAVE_WAIT_BYTE latch that
         * ai_king_do_declare just armed ("no landing on the declaration turn
         * itself"), keeping the landing on the same turn it has always been.
         * The muster / 10f0 / merc arm (ai_king_war_act, DOS 2022's non-crown
         * branch) now waits for the crown slot's pre-euro beat next turn.
         */
        ai_king_ref_wave(ctx);
      }
      break;
    case AI_POPUP_TAG_KING_SCORED:
      /* Peacetime @SCORED: That's all → @RETIRING then score UI; Keep playing → continue. */
      if (popup->result_choice_id == AI_KING_CHOICE_THATS_ALL) {
        const char* leader =
          (ctx->col1_ok && ctx->col1 && human >= 0 && human < 4 &&
           ctx->col1->player[human].name[0] != '\0')
            ? ctx->col1->player[human].name
            : "";
        const char* estate = ai_king_richest_colony_name(ctx, human);
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        const int retire_diff = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.difficulty : 4;
        tok.string0 = reports_difficulty_title((retire_diff >= 0 && retire_diff < 5) ? retire_diff : 4);
        tok.string1 = leader;
        tok.string2 = estate;
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(ctx->messages, "RETIRING", &tok, "", body, sizeof(body));
        if (ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "%s", body);
        }
        if (ai_king_human_popups(ctx)) {
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups,
            AI_POPUP_TAG_INFO,
            human,
            ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
            0,
            NULL,
            body
          );
        }
      } else if (popup->result_choice_id == AI_KING_CHOICE_KEEP_PLAYING) {
        if (ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "Continuing the campaign.");
        }
      }
      break;
    default:
      break;
  }
}
