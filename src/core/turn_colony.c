#include "core/internal.h"
#include "core/popup_msg.h"
#include "core/turn.h"
#include "core/turn_internal.h"

#include "core/strutil.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "core/ai.h"
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_euro_internal.h"
#include "core/ai_king.h"
#include "core/col1_stuff_census.h"
#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/unit_chrome.h"
#include "core/woodcut.h"
#include "platform/diagnostics.h"


/*
 * Sections:
 *  - Colony construction & building completion
 *  - Nation tick: bells/crosses tally, immigrants, ship repair/routing
 */

/* ===================== Colony construction & building completion (turn_run_colony_unit_construction .. turn_colony_free_production) ===================== */
static void turn_run_colony_unit_construction(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->colonies || !ctx->units) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* col = &ctx->colonies->colonies[i];
    if (!col->active || col->building_in_production < 0 ||
        !turn_prod_nation_in_scope(col->nation_id)) {
      continue;
    }
    const char* name = NULL;
    if (!colonies_unit_build_info(col->building_in_production, &name, NULL, NULL)) {
      continue;
    }
    const int uidx = units_build_code_to_index(col->building_in_production);
    /*
     * DOS-LITERAL FUN_364b_0114 wagon arm (raw 56926-56933): before spawning,
     * a Wagon Train project whose nation already owns as many wagons as
     * colonies is refused — %NUMBER0 = colony_counts[n], popup @NOMOREWAGONS,
     * +0x1c |= 0x80, and `return`: the project is NOT cleared and the hammers
     * are NOT zeroed, so it re-fires every turn until a new colony is founded.
     */
    if (uidx == COLONIZE_UNIT_INDEX_WAGON_TRAIN &&
        colonies_wagon_cap_reached(ctx->col1_ok ? ctx->col1 : NULL, col->nation_id)) {
      int hammers_need = 0;
      if (colonies_unit_build_info(col->building_in_production, NULL, &hammers_need, NULL) &&
          hammers_need > 0 && col->hammers >= hammers_need) {
        col->colony_flags =
          (uint8_t)(col->colony_flags | COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
        /* bugs.md #789 (REFUTED): FUN_364b_0114 does call the popup thunk
         * unconditionally, but the thunk lands in FUN_364b_0000, whose whole
         * body sits behind `if (*(char*)0xa897 != '\0')` (raw 56839).
         * DS:0xa897 is set by the colony-select routine (raw 9325-9331) only
         * when the colony's owner byte (+0x1a) equals the human player
         * (DS:0x5396), is <= 3, and is not AI-run (DS:0x543f table) — i.e.
         * exactly this human_nation gate. AI colonies never show it. */
        if (col->nation_id == ctx->human_nation && ctx->ai_popups) {
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = col->name[0] ? col->name : "colony";
          tok.number0 =
            colonies_nation_colony_count_census(ctx->col1_ok ? ctx->col1 : NULL, col->nation_id);
          tok.has_number0 = true;
          popup_msg_fill(
            ctx->messages, "NOMOREWAGONS", &tok,
            "",
            body, sizeof(body)
          );
          ai_popup_enqueue_colony_event(ctx->ai_popups, col->id, body);
        }
      }
      continue;
    }
    const int uid = colonies_try_complete_unit_construction(
      ctx->colonies, col->id, ctx->units, ctx->col1_ok ? ctx->col1 : NULL
    );
    if (uid < 0) {
      continue;
    }
    /*
     * FUN_364b_0114 raw 56938: ++unit_type_counts[nation][unit_index]
     * (DS:0x924c, stride 0x13) on every spawn. Without it two wagons finished
     * in the same EOT both pass the cap, which reads the census window the
     * EOT refresh only rewrites later.
     */
    if (ctx->col1_ok && ctx->col1 && uidx >= 0 && uidx < 19 && col->nation_id >= 0 &&
        col->nation_id < 4) {
      uint8_t* slot = &ctx->col1->stuff.unit_type_counts[col->nation_id][uidx];
      if (*slot < 0xffu) {
        *slot = (uint8_t)(*slot + 1u);
      }
    }
    if (ctx->europe && col->nation_id == ctx->human_nation) {
      snprintf(ctx->europe->status, sizeof(ctx->europe->status), "%s completed.", name);
      turn_emit_built_chrome(ctx->messages, ctx->ai_popups, col, name, ctx->europe->status);
    }
  }
}

/*
 * Player-requested: BUY only tops hammers/tools up to the completion
 * threshold — it must NOT complete the project itself (colonies_buy_construction
 * no longer calls colonies_try_complete_building). Completion happens here,
 * once per turn, unconditionally (no Spring/Autumn gate, no "did hammers
 * change this tick" gate — turn_produce_one_colony's own inline complete
 * check only fires when the colony *produces* new hammers that tick, which
 * misses a colony already sitting at/above threshold from a BUY with an
 * idle Carpenter or on a frozen Autumn tick). Sibling to
 * turn_run_colony_unit_construction above — same one-pass-per-turn shape,
 * real buildings instead of units. colonies_try_complete_building's own
 * has_building[] guard makes this safe to also run on a turn where the
 * inline per-colony-production path in turn_produce_one_colony already
 * completed the same project (second call just returns false).
 */
static void turn_run_colony_building_completion(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->colonies) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* col = &ctx->colonies->colonies[i];
    if (!col->active || col->building_in_production < 0 ||
        !turn_prod_nation_in_scope(col->nation_id)) {
      continue;
    }
    const ColonizeBuildingType* bt = colonies_building_type(ctx->colonies, col->building_in_production);
    if (!bt || bt->hammers <= 0 || col->hammers < bt->hammers) {
      continue;
    }
    if (!colonies_try_complete_building(ctx->colonies, col->id)) {
      continue;
    }
    if (ctx->europe && col->nation_id == ctx->human_nation) {
      snprintf(ctx->europe->status, sizeof(ctx->europe->status), "%s completed.", bt->name);
      turn_emit_built_chrome(ctx->messages, ctx->ai_popups, col, bt->name, ctx->europe->status);
    }
  }
}

/*
 * The colony end-of-turn trio, in DOS's order (audit CO-10): production,
 * then the two completion sweeps production itself cannot do.
 * turn_run_colony_production has no ColonizeUnitPool access
 * (colonies_try_complete_building never needed one; spawning a unit does),
 * so unit construction is its own pass; and BUY-topped-up (or otherwise
 * carpenter-idle / Autumn-frozen) real buildings sitting at or above the
 * hammer threshold need theirs, because turn_produce_one_colony's inline
 * complete check only fires on a tick that adds new hammers.
 * The nation scoping (s_prod_skip_* / s_prod_only_*) stays at the call
 * sites: TURN_PROC_SETUP runs every AI nation, TURN_PROC_FINISH the human.
 */
void turn_run_colony_eot(ColonizeTurnContext* ctx, ColonizeTurnResult* out) {
  turn_set_birth_units_pool(ctx->units);
  /* bugs.md #770: FUN_4962_0606 (raw 78332-78374) counts the nation's map
   * units before its colonists, so the specialty census needs the pool. */
  turn_run_colony_production_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL), .rng=(ColonizeDosRng*)(ctx->rng), .europe=(EuropeScreen*)(ctx->europe)}, ctx->human_nation, out, ctx->ai_popups, ctx->messages);
  turn_set_birth_units_pool(NULL);
  turn_run_colony_unit_construction(ctx);
  turn_run_colony_building_completion(ctx);
}

int turn_run_coastal_fort_fire(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units || !ctx->colonies || !ctx->map) {
    return 0;
  }
  return units_coastal_fort_fire_pulse_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL), .rng=(ColonizeDosRng*)(ctx->rng)}, ctx->human_nation, ctx->status, ctx->status_size);
}

void turn_colony_free_production(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const ColonizeWorldMap* map,
  ColonizeTurnResult* out,
  ColonizeColonyProdDelta* out_delta
) {
  ColonizeTurnResult local;
  memset(&local, 0, sizeof(local));
  turn_produce_one_colony(
    pool, colony, map, NULL, NULL, -1, out ? out : &local, out_delta, NULL, NULL, NULL
  );
}

/* ===================== Nation tick: bells/crosses tally, immigrants, ship repair/routing (turn_count_bells_and_crosses_for_nation .. turn_euro_nation_is_ref) ===================== */
static int turn_count_bells_and_crosses_for_nation(
  const ColonizeColonyPool* pool,
  int nation_id,
  const ColonizeCol1Save* col1,
  int* out_bells,
  int* out_crosses
) {
  int bells = 0;
  int crosses = 0;
  if (!pool) {
    if (out_bells) {
      *out_bells = 0;
    }
    if (out_crosses) {
      *out_crosses = 0;
    }
    return 0;
  }
  /* Stamp of a compose that happened on THIS turn (see ColonizeColony's
   * prod_compose_stamp). 0 disables the snapshot path entirely. */
  const uint32_t this_turn_stamp = col1 ? ((uint32_t)col1->head.turn + 1u) : 0u;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    int b = 0;
    int x = 0;
    if (this_turn_stamp != 0u && c->prod_compose_stamp == this_turn_stamp) {
      /* DOS "one number, two consumers": reuse exactly what this colony's
       * own Phase A composed (smell audit #62) instead of re-tallying after
       * its Phase C/D SoL update and its F/G/H/J roster edits. */
      b = c->prod_bells_phase_a;
      x = c->prod_crosses_phase_a;
    } else {
      /* No compose this turn (human colonies tick later, in
       * TURN_PROC_FINISH; direct callers may have no col1) — a live read
       * here IS the pre-tick state, same as DOS's Phase A would see.
       * sol_b folds into each Statesman/Preacher worker individually,
       * inside colony_prod_colony_bells_ff/_crosses_ff (matches
       * FUN_15eb_1d4c's Statesman/Preacher bodies — see
       * manufacturing_worker_calc_1d4c.md). */
      turn_compose_colony_bells_crosses(
        pool, c, col1, colony_prod_sol_bonus(col1, c), &b, &x
      );
    }
    bells += b;
    crosses += x;
  }
  if (out_bells) {
    *out_bells = bells;
  }
  if (out_crosses) {
    *out_crosses = crosses;
  }
  return bells + crosses;
}

static void turn_notify_dock_immigrant(
  ColonizeTurnContext* ctx,
  ColonizeTurnResult* out,
  const char* immigrant_name
) {
  if (!ctx || !ctx->europe) {
    return;
  }
  if (out) {
    out->immigrants_arrived++;
    /* DOS: @UNREST dialog — do not auto-dump into Europe screen (DS:0x14c is optional). */
  }
  diag_info(
    "EUROPE immigrant on the docks: %s (crosses)",
    immigrant_name && immigrant_name[0] ? immigrant_name : "Colonist"
  );
  if (ctx->status && ctx->status_size > 0) {
    snprintf(
      ctx->status,
      ctx->status_size,
      "Immigrant arrives in Europe: %s",
      immigrant_name && immigrant_name[0] ? immigrant_name : "Colonist"
    );
  }
  if (!ctx->ai_popups) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.country = ctx->europe->nation_name[0] ? ctx->europe->nation_name : "Europe";
  /* 5e52 raw 68592: %STRING0 = @HOMEPORT name (DS table -0x7c74), not a
   * literal (bugs.md #673). */
  tok.string0 = ctx->europe->port_city[0] ? ctx->europe->port_city : "";
  tok.string1 = immigrant_name && immigrant_name[0] ? immigrant_name : "";
  char body[AI_POPUP_BODY_LEN];
  const char* fb = "";
  if (ctx->messages) {
    popup_msg_fill(ctx->messages, "UNREST", &tok, fb, body, sizeof(body));
  } else {
    snprintf(body, sizeof(body), "%s", fb);
  }
  ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void turn_run_nation_ticks(ColonizeTurnContext* ctx, ColonizeTurnResult* out) {
  if (!ctx) {
    return;
  }
  int bells = 0;
  int crosses = 0;
  turn_count_bells_and_crosses_for_nation(
    ctx->colonies, ctx->human_nation, ctx->col1_ok ? ctx->col1 : NULL, &bells, &crosses
  );

  if (ctx->europe) {
    if (ctx->europe->needed_crosses == 0) {
      ctx->europe->needed_crosses = TURN_DEFAULT_NEEDED_CROSSES;
    }
    /*
     * Church / colony crosses accrue into current_crosses, then 584a sets
     * needed and adds idle +2 until the first dock immigrant.
     * Cite: Phase M + 5e52; TURN1–7 goldens.
     */
    ctx->europe->liberty_bells_last_turn = (uint16_t)(bells > 65535 ? 65535 : bells);
    {
      unsigned total = (unsigned)ctx->europe->liberty_bells_total + (unsigned)bells;
      if (total > 65535u) {
        total = 65535u;
      }
      ctx->europe->liberty_bells_total = (uint16_t)total;
    }
    {
      unsigned cur = (unsigned)ctx->europe->current_crosses + (unsigned)crosses;
      if (cur > 65535u) {
        cur = 65535u;
      }
      ctx->europe->current_crosses = (uint16_t)cur;
    }
    /* Keep the col1 nation copy live — the Religious report (F1) reads
     * nation[human].current/needed_crosses, which only the save/load
     * bridge used to refresh, so a live campaign showed no crosses at all
     * (bugs.md). */
    if (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4) {
      ctx->col1->nation[ctx->human_nation].current_crosses = ctx->europe->current_crosses;
      ctx->col1->nation[ctx->human_nation].needed_crosses = ctx->europe->needed_crosses;
    }
    /* bugs.md: no immigration during the War of Independence — Europe is
     * closed to the rebels (user-observed DOS; the dock is unreachable
     * anyway once the WoI blocks the European Status). */
    const int woi_now =
      ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi != 0;
    /* DOS 68583 refills the emptied pool slot with `46d4((DS:0x538e & 3)==0)`
     * — one turn in four skips the criminal/servant/free tier roll. */
    if (ctx->europe) {
      const uint32_t turn = ctx->turn_number ? *ctx->turn_number : 0u;
      ctx->europe->pool_force_expert = ((turn & 3u) == 0u);
    }
    const int imm = woi_now ? 0
                            : europe_tick_immigration_pressure_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL), .rng=(ColonizeDosRng*)(ctx->rng), .europe=(EuropeScreen*)(ctx->europe)}, ctx->human_nation);
    if (imm == 2) {
      /* Brewster: player picks from the pool (@RECRUITCHOOSE); applied via
       * units_brewster_apply_popup in game_loop, crosses kept until then. */
      units_brewster_enqueue_pick(ctx->europe, ctx->ai_popups, ctx->messages, ctx->human_nation);
      if (ctx->status && ctx->status_size > 0) {
        snprintf(ctx->status, ctx->status_size, "Religious unrest: choose an immigrant.");
      }
    } else if (imm == 1) {
      if (ctx->europe) {
        europe_notify_immigrant_sound(ctx->europe); /* FUN_38fd_5e52 38fd:5ecb: pool 2 */
      }
      const char* name = "";
      if (ctx->europe->dock_count > 0) {
        name = ctx->europe->dock[ctx->europe->dock_count - 1].name;
      }
      turn_notify_dock_immigrant(ctx, out, name);
      /* Mirror dock immigrant as Europe-map unit for Col1 capture. */
      if (ctx->units && ctx->europe->dock_count > 0) {
        const EuropeDockImmigrant* d = &ctx->europe->dock[ctx->europe->dock_count - 1];
        (void)europe_spawn_dock_mirror_unit(
          ctx->units, ctx->human_nation, d->profession, (int)ctx->europe->difficulty, true,
          ctx->rng
        );
      }
    }
    europe_tick_voyages(ctx->europe, ctx->units);
    /*
     * FUN_48d3_08bf: the human's Europe arrival pass ends with woodcut 9 when
     * one of the ships that just docked was carrying goods, then sets the
     * auto-open flag (DS:0x14c = europe.open_on_dock).
     */
    if (ctx->europe->docked_with_goods) {
      (void)woodcut_fire(ctx->col1, WOODCUT_CARGO_FROM_THE_NEW_WORLD);
    }
  }

  /*
   * FUN_4345_0a22 / nation EOT: accrue bells+crosses for every active Euro
   * nation (human + AI). Dock immigrant / Europe chrome stays human-only above.
   * Cite: turn/nation_ticks_bells_ff.md; DOS per-nation 00f2.
   */
  if (ctx->col1_ok && ctx->col1) {
    for (int n = 0; n < 4; ++n) {
      uint8_t control = 1;
      if (n < (int)COLONIZE_COL1_NATION_COUNT) {
        control = ctx->col1->player[n].control;
      }
      if (control == 2) {
        continue; /* withdrawn */
      }
      int nb = 0;
      int nc = 0;
      turn_count_bells_and_crosses_for_nation(
        ctx->colonies, n, ctx->col1, &nb, &nc
      );
      ColonizeCol1Nation* nat = &ctx->col1->nation[n];
      nat->liberty_bells_last_turn = (uint16_t)(nb > 65535 ? 65535 : nb);
      {
        unsigned total = (unsigned)nat->liberty_bells_total + (unsigned)nb;
        if (total > 65535u) {
          total = 65535u;
        }
        nat->liberty_bells_total = (uint16_t)total;
      }
      if (nb > 0) {
        founding_fathers_accrue_bells(n, (unsigned)nb);
      }
      /*
       * AI Euro: the full DOS FUN_38fd_5e52 tick (584a needed + the +2/-2
       * crosses tick, and on a crossing a real immigrant out of the nation's
       * own recruit[3] pool, parked in the Europe limbo for the AI's own
       * 5d04/ship logic to load). Silent — every popup in 5e52 is gated on
       * control == 0. Was a flat "+2, spawn PARKED" stub.
       *
       * Order matters and is DOS's: FUN_3844_00f2 calls 5e52 (FUN_291f_0a90,
       * :58375) BEFORE the per-colony tick FUN_291f_0950 (:58384), so the
       * threshold test sees last turn's crosses plus only the 584a +2 — this
       * turn's church output lands after it. test-saves-ai TURN6→7 nation[3]
       * 12 → 14 (14 < 14 is false, no arrival) → 15 with the colony cross;
       * adding the colony crosses first would have spawned an immigrant DOS
       * did not spawn (and did spawn, in the first cut of this port).
       */
      if (n != ctx->human_nation) {
        const ColonizeWorld w = world_from_turn_ctx(ctx);
        (void)europe_nation_immigration_tick_w(&w, n);
      }
      {
        unsigned cur = (unsigned)nat->current_crosses + (unsigned)nc;
        if (cur > 65535u) {
          cur = 65535u;
        }
        nat->current_crosses = (uint16_t)cur;
      }
      /* Human Europe screen is authoritative for human nation counters. */
      if (n == ctx->human_nation && ctx->europe) {
        nat->current_crosses = ctx->europe->current_crosses;
        nat->needed_crosses = ctx->europe->needed_crosses;
        nat->liberty_bells_total = ctx->europe->liberty_bells_total;
        nat->liberty_bells_last_turn = ctx->europe->liberty_bells_last_turn;
      }
    }
    founding_fathers_tick(ctx);

    /* FUN_4345_0a22 wartime branch: bell pool → intervention / REF, not FF elect. */
    if (ctx->col1->head.game_options.woi) {
      /*
       * bugs.md #231 / DOS FUN_4345_0a22 (viceroy ~73346): once per war,
       * while the REF has not yet landed (0x5382 bit1 clear) and the bell
       * pool has started accruing, show @AMBUSHHINT then @CONSIDER —
       * "%STRING0 is considering intervention ... generate %NUMBER0 liberty
       * bells". Latch = 0x5382 bit 0x04 (game_options.woi_crosses_event,
       * confirmed live 2026-08-18 as exactly this dialog's one-shot).
       */
      /* DOS-LITERAL raw 73347: `(0x5382 & 6) == 0` — bit 0x02 is the
       * intervention-ANNOUNCE latch (game_options.ref_present, bugs.md
       * #865), bit 0x04 the hint one-shot. */
      if (!ctx->col1->head.game_options.ref_present &&
          !ctx->col1->head.game_options.woi_crosses_event &&
          founding_fathers_bells_since_last_elect(ctx->human_nation) > 0u &&
          ctx->ai_popups) {
        const int ally = (int)ctx->col1->head.rival_nation_slot_1;
        const char* ally_name = "";
        /* bugs.md #252 rule: PARENT country ("France"), never the new-world
         * colony name player[ally].country_name ("New France"). */
        if (ally >= 0 && ally < 4) {
          ally_name = reports_nation_country_name(ally); /* NAMES.TXT @COUNTRY */
        }
        popup_chrome_ok(
          ctx->ai_popups, ctx->messages, "AMBUSHHINT", NULL,
          ""
        );
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = ally_name;
        tok.has_number0 = true;
        tok.number0 = (int)founding_fathers_bells_needed(ctx->col1, ctx->human_nation);
        popup_chrome_ok(
          ctx->ai_popups, ctx->messages, "CONSIDER", &tok,
          ""
        );
        ctx->col1->head.game_options.woi_crosses_event = 1;
      }
      for (int n = 0; n < 4; ++n) {
        if (ctx->col1->player[n].control == 2) {
          continue;
        }
        const unsigned pool = founding_fathers_bells_since_last_elect(n);
        const unsigned needed = founding_fathers_bells_needed(ctx->col1, n);
        founding_fathers_woi_intervention_chrome(ctx, n, pool, needed);
        if (pool < needed) {
          continue;
        }
        if (ai_king_spend_woi_bell_pool(ctx, n)) {
          founding_fathers_consume_woi_bell_pool(n);
          if (ctx->status && ctx->status_size > 0 && n == ctx->human_nation) {
            /* bugs.md #538: the bell spend only ANNOUNCES (DOS 0a22 ->
             * FUN_43f7_1528); the force itself lands from 2022's free drain
             * on a later turn. */
            snprintf(
              ctx->status,
              ctx->status_size,
              "Foreign intervention force joins the rebellion!"
            );
          }
        }
      }
    }
  }

  /*
   * FUN_3844_00f2 §C tail (viceroy 58393-58423) is NOT ported here: it is
   * the @KINGFRIGATE event — Crown offers a FRIGATE (+10% tax) when the
   * nation's colonies are threatened by warships — and lives in ai_king.c
   * (ai_king_frigate_*). An earlier duplicate here misread type 0x11 as a
   * "Merchantman" and spawned a free ship every 8th turn (bugs.md
   * free_merchanman.SAV; player-corrected: a Merchantman is the victim,
   * not the help).
   */
}

int turn_rank_euro_nations(
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  uint8_t out_rank[4]
) {
  /*
   * FUN_5bfb_00f8: score = gold/100 + 2*colony_count + pop_proxy + land_combat.
   * Sort descending; inverse rank[nation] = place (0 = strongest).
   * Cite: viceroy_unpacked.c ~96506–96531; turn/mid_pass_indian_rank.md.
   */
  if (!out_rank) {
    return -1;
  }
  int score[4];
  int perm[4];
  for (int n = 0; n < 4; ++n) {
    perm[n] = n;
    int colonies_n = 0;
    int pop = 0;
    if (colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &colonies->colonies[i];
        if (!c->active || c->nation_id != n) {
          continue;
        }
        colonies_n++;
        pop += c->colonist_count > 0 ? c->colonist_count : c->population;
      }
    }
    int gold100 = 0;
    int land = 0;
    if (col1 && n < (int)COLONIZE_COL1_NATION_COUNT) {
      gold100 = (int)(europe_nation_gold(NULL, col1, n) / 100u);
      /* Land combat from stuff census when present; colony/pop always live. */
      land = (int)col1->stuff.land_combat_strength[n];
    }
    score[n] = gold100 + colonies_n * 2 + pop + land;
  }
  /* Stable insertion sort by score descending; perm tracks nation ids. */
  for (int i = 1; i < 4; ++i) {
    const int s = score[i];
    const int p = perm[i];
    int j = i;
    while (j > 0 && score[j - 1] < s) {
      score[j] = score[j - 1];
      perm[j] = perm[j - 1];
      j--;
    }
    score[j] = s;
    perm[j] = p;
  }
  for (int place = 0; place < 4; ++place) {
    out_rank[perm[place]] = (uint8_t)place;
  }
  return 0;
}

void turn_tally_professions(
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  int nation_id,
  uint8_t out_hist[32]
) {
  /*
   * FUN_4962_0606 (raw 78332-78372): zero hist[0x1d]; then count the
   * nation's units by profession — but ONLY those whose @UNIT type carries
   * a profession slot (`FUN_281f_0b78` -> FUN_15eb_0902 = DS:0x30e[type] >=
   * 0, units_type_default_job here): ships, wagons, artillery and treasure
   * are skipped, otherwise their profession byte 0 would register as an
   * Expert Farmer. Then count every colonist of every colony of the nation
   * by its SPECIALTY (`FUN_281f_0c54`) — never by the field job it is
   * standing in, so a Free Colonist working a wheat plot is not an Expert
   * Farmer. 32 slots here (covers the whole @JOB range).
   */
  if (!out_hist) {
    return;
  }
  memset(out_hist, 0, 32);
  if (nation_id < 0 || nation_id > 3) {
    return;
  }
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      if (!units_type_has_profession_slot(u->type_index)) {
        continue;
      }
      const int p = u->profession;
      if (p >= 0 && p < 32 && out_hist[p] < 255u) {
        out_hist[p]++;
      }
    }
  }
  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      for (int ci = 0; ci < c->colonist_count; ++ci) {
        const ColonizeColonist* col = &c->colonists[ci];
        if (!col->active) {
          continue;
        }
        const int p = col->profession;
        if (p >= 0 && p < 32 && out_hist[p] < 255u) {
          out_hist[p]++;
        }
      }
    }
  }
}

/*
 * bugs.md: damaged ships have a homing system — the nearest OWN colony with
 * a Drydock; a nation with no such colony sends them to Europe when Europe
 * is friendly: the pre-WoI human's ships sail home as an Expected-Soon
 * voyage (that voyage IS the repair timeout — they arrive repaired), and a
 * damaged Tory Man-O-War sails back to the King's dockyards (despawn; the
 * next wave draws on the fleet pool). AI peers with no drydock keep the old
 * stay-put behavior. Runs each nation phase, after units_tick_drydock_repair.
 */
/* Drydock (DOS colony feature bit 7) or its Shipyard upgrade. */
static int turn_colony_repairs_ships(const ColonizeColony* c, int drydock, int shipyard) {
  if (!c) {
    return 0;
  }
  if (drydock >= 0 && drydock < COLONIZE_BUILDING_TYPES_MAX && c->has_building[drydock]) {
    return 1;
  }
  return shipyard >= 0 && shipyard < COLONIZE_BUILDING_TYPES_MAX && c->has_building[shipyard];
}

/*
 * The DS:0x9418[nation] hull tally FUN_48d3_0002 gates the 2-turn crossing on
 * (viceroy_unpacked.asm 48d3:003b `cmp byte [bx+0x9418],0x3`), built by
 * FUN_4962_0018 walking the WHOLE unit array and bumping 0x9418 for every
 * unit of that nation with type 0x0d..0x12 (4962:0300-0365).
 *
 * DOS parks a ship crossing to Europe as a live unit on its nation's Europe
 * sentinel diagonal (228/232/244+n), so harbour, expected and bound hulls are
 * all inside that tally. This port hoists the HUMAN's Europe-side ships out of
 * the unit pool into EuropeScreen (AI nations keep theirs on the diagonal), so
 * the live-pool walk alone under-counts the human by exactly those three
 * arrays and `stuff.ship_counts[]` — a census of the live pool only
 * (col1_stuff_census.c:112) — under-counts him the same way. Reconstructed
 * here the way game_loop.c's `game_voyage_ship_count` does for the manual
 * sail-to-Europe path; keep the two in step. Called with the damaged hull
 * still active, since DOS counts the departing ship too.
 *
 * Exported 2026-09-10 (audit second-wave lead 3): ai_king.c's @KINGFRIGATE
 * gift spawn was a THIRD spelling of this count — a bare
 * `units_count_sea_for_nation` with no Europe adds — so a human whose whole
 * fleet was sitting in the harbour rolled the Crown's free Frigate a
 * 2-turn crossing DOS would have given 4-6. It now calls this.
 * (game_loop.c's `game_voyage_ship_count` is the same reconstruction over
 * ColonizeGameState rather than ColonizeTurnContext; it has no turn ctx in
 * hand, so it stays a separate spelling of the same three adds.)
 */
int turn_voyage_ship_count(const ColonizeTurnContext* ctx, int nation) {
  if (!ctx || !ctx->units) {
    return 0;
  }
  int ships = units_count_sea_for_nation(ctx->units, nation);
  if (ctx->europe && (int)ctx->europe->bound_nation == nation) {
    ships += ctx->europe->harbor_ships + ctx->europe->expected_ships + ctx->europe->bound_ships;
  }
  return ships;
}

void turn_route_damaged_ships(ColonizeTurnContext* ctx, int nation) {
  if (!ctx || !ctx->units || !ctx->colonies || nation < 0 || nation > 3) {
    return;
  }
  const int woi =
    ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi != 0;
  const int crown =
    woi ? ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation) : -1;
  const int drydock = colonies_building_row(ctx->colonies, COLONY_BUILDING_DRYDOCK);
  const int shipyard = colonies_building_row(ctx->colonies, COLONY_BUILDING_SHIPYARD);
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(ctx->units, u->id) || (u->col1_flags15 & 0x80u) == 0) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
    /*
     * bit7 is shared by "under construction" and "combat damaged"; only the
     * latter sets repair_pending, so that — not the col1_counter16/threshold
     * comparison — is what separates them. The old threshold test never let
     * a damaged ship through: units_tick_drydock_repair, which runs first,
     * clears bit7 the moment the timer completes, so the Europe fallback
     * below was unreachable and every damaged ship stayed at whatever colony
     * combat had parked it on (bugs.md).
     */
    if (!u->repair_pending) {
      continue; /* construction — build tick owns bit7 */
    }
    /* Already sitting on an own Drydock colony: the repair tick handles it. */
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    const ColonizeColony* here = colonies_get(ctx->colonies, cid);
    if (here && here->active && here->nation_id == nation &&
        turn_colony_repairs_ships(here, drydock, shipyard)) {
      continue;
    }
    /* Nearest own Drydock colony (recomputed here: the one picked at combat
     * time may since have been captured). */
    const ColonizeColony* best = NULL;
    long best_d = -1;
    if (drydock >= 0 || shipyard >= 0) {
      for (int k = 0; k < COLONIZE_COLONIES_MAX; ++k) {
        const ColonizeColony* c = &ctx->colonies->colonies[k];
        if (!c->active || c->nation_id != nation ||
            !turn_colony_repairs_ships(c, drydock, shipyard)) {
          continue;
        }
        const long dx = c->x - u->x;
        const long dy = c->y - u->y;
        const long d = dx * dx + dy * dy;
        if (best_d < 0 || d < best_d) {
          best_d = d;
          best = c;
        }
      }
    }
    if (best) {
      const int ox = u->x;
      const int oy = u->y;
      u->x = best->x;
      u->y = best->y;
      units_occupancy_notify_moved(ctx->units, ox, oy, best->x, best->y);
      continue;
    }
    if (nation == crown) {
      /*
       * DOS-LITERAL FUN_5fef_0352 raw 99641-99644 (bugs.md #871): a damaged
       * hull whose owner has no repair port is relocated OFF-MAP for EVERY
       * nation — the colony scan misses (local_20 == 0x3e7) and the
       * FUN_281f_0812 / FUN_281f_0844 unlink/place pair is handed
       * `nation - 0x14` in both coordinates, DOS's Europe dock pseudo-tile.
       * Only the no-port *sink* escape at raw 99604-99607 is human-only.
       *
       * The port's equivalent off-map slot is the Europe park
       * (ai_euro_in_europe / ai_euro_ship_enter_europe's (200,100)). The hull
       * stays a LIVE unit with bit7 and its repair timer intact, exactly as
       * in DOS, so it keeps counting:
       *   - in the census (raw 78159-78165 walks the whole unit array and
       *     tallies by nation and type, with no position test), and
       *   - in ai_king_0982_crown_mow_alive, the FUN_43f7_0982 regen gate.
       * That is what keeps the player's coastal guns from ACCELERATING the
       * invasion: a shot-up hull is off the map but still "a Man-O-War the
       * crown owns", so 0982 does not refill early. (The old comment here
       * cited the retired "4d56 ship act", port_plan P5.1 2026-09-07, and
       * left the wreck standing visibly on the invasion tile.)
       */
      if (!ai_euro_in_europe(u->x, u->y)) {
        const int ox = u->x;
        const int oy = u->y;
        u->x = 200;
        u->y = 100;
        units_occupancy_notify_moved(ctx->units, ox, oy, u->x, u->y);
        ai_euro_sync_aboard_cargo_xy(ctx->units, u);
        u->moves = 0;
      }
      continue;
    }
    if (nation == ctx->human_nation && !woi && ctx->europe && u->cargo_count == 0) {
      /*
       * FUN_48d3_0002 gates the 2-turn crossing on DS:0x9418[nation] (the
       * FUN_4962_0018 hull tally — turn_voyage_ship_count) and on Magellan
       * (FF 5). Hardcoding count 1 / no-Magellan burned the RNG draw but
       * could never take either branch, so a damaged ship's voyage home was
       * always 1 turn regardless of fleet size. Reading stuff.ship_counts[]
       * instead was still one spelling short of game_loop's: the census walks
       * the live pool only, so a human with 2 hulls on the map and 3 in the
       * lane rolled "3+ ships" on a manual crossing and "2 ships" here.
       */
      const bool magellan =
        ctx->col1_ok && ctx->col1 &&
        founding_fathers_nation_has(ctx->col1, nation, FF_FERDINAND_MAGELLAN);
      const int fleet = turn_voyage_ship_count(ctx, nation);
      const int turns = europe_voyage_turns_roll(ctx->rng, magellan, fleet);
      /* Same edge rule as the manual sail-to-Europe path so the ship comes
       * back on the side it left from. */
      const bool east = ctx->map ? (u->x >= (int)ctx->map->width / 2) : true;
      if (europe_enqueue_expected(
            ctx->europe, u->type_index, ty ? ty->name : "Ship", NULL, NULL, 0,
            u->hold_goods_type, u->hold_goods_amount, u->x, u->y, east, turns
          )) {
        if (ctx->status && ctx->status_size > 0) {
          snprintf(
            ctx->status, ctx->status_size, "%s sails to Europe for repairs.",
            ty && ty->name[0] ? ty->name : "Damaged ship"
          );
        }
        u->col1_flags15 = (uint8_t)(u->col1_flags15 & 0x7fu); /* repaired abroad */
        units_despawn(ctx->units, u->id);
      }
    }
  }
}

/*
 * D1 closed 2026-09-07g: DOS does BOTH beats for the crown slot. `1a26`
 * sets the crown slot's control byte to 1 (viceroy_unpacked.c:74833 —
 * control 2 is `0108`'s eliminated powers), and the turn loop runs
 * `3844_00f2` (which carries `2424` → `2022`, a pure spawner/bookkeeper —
 * the 43f7 overlay holds zero orders/goto writes) for every control != 2
 * slot (raw 6394) and then the full Euro nation turn `FUN_521d_6d8e` for
 * every control == 1 slot (raw 6407). The port mirrors that per-slot order:
 * ai_king_ref_pre_euro_beat (wave + bookkeeping) then ai_euro_nation_turn,
 * both inside the crown slot's EURO step. Running the euro pass without the
 * preceding king beat re-creates the 2026-08-28 bug (moves spent before the
 * wave lands: 40 turns, zero attacks).
 */
bool turn_euro_nation_is_ref(const ColonizeTurnContext* ctx, int n) {
  return ctx && ctx->col1_ok && ctx->col1 && ai_king_independence_declared(ctx->col1) &&
         n == ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation);
}

/*
 * FUN_3844_0442 §D support.
 *
 * iVar5 = nation[rival]+0x19 × (rival − 0x6bf0) / 100, capped 100.
 *   nation+0x19            = rebel_sentiment
 *   DS (rival − 0x6bf0)    = DS:0x9410 = stuff.census_pop_proxy[rival]
 * The "−0x6bf0 is an unrecoverable continent-weight table" note this file
 * used to carry was a misread: @OTHERMIGHT prints the very same byte as
 * `%NUMBER1` in "{%NUMBER0} (out of %NUMBER1) of the %STRING1 colonists",
 * i.e. it is the nation's colonist head count (already saved, already
 * written by col1_stuff_census — see docs/save_format_map.md row 244).
 */
