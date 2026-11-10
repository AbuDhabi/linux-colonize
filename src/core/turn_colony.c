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
#include "core/dos_buggy.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/europe_internal.h"
#include "core/founding_fathers.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/woodcut.h"
#include "platform/diagnostics.h"

ColonizeColony* g_turn_1528_last_walked_colony = NULL;

/*
 * Sections:
 *  - Colony construction
 *  - Nation tick: bells/crosses tally, immigrants, ship repair
 */

/* ===================== Colony construction (turn_colony_unit_construction_one .. turn_colony_free_production) ===================== */
/* Called from turn_produce_one_colony between Phase K and spoilage. */
void turn_colony_unit_construction_one(const ColonizeWorld* ctx, ColonizeColony* col) {
  if (!ctx || !ctx->units || !col->active || col->building_in_production < 0) {
    return;
  }
  const char* name = NULL;
  if (!colonies_unit_build_info(col->building_in_production, &name, NULL, NULL)) {
    return;
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
      /* port_bugs.md #789 (REFUTED): FUN_364b_0114 does call the popup thunk
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
        tok.string0 = col->name[0] ? col->name : "";
        tok.number0 =
          colonies_nation_colony_count_census(ctx->col1_ok ? ctx->col1 : NULL, col->nation_id);
        tok.has_number0 = true;
        popup_msg_fill(
          ctx->messages, "NOMOREWAGONS", &tok,
          "",
          body, sizeof(body)
        );
        ai_popup_enqueue_colony_event(ctx->ai_popups, col->id, AI_POPUP_ZOOM_TAB_CONSTRUCTION, body);
      }
    }
    return;
  }
  const int uid = colonies_try_complete_unit_construction(
    ctx->colonies, col->id, ctx->units, ctx->col1_ok ? ctx->col1 : NULL
  );
  if (uid < 0 && uid != COLONIZE_UNIT_BUILD_REFUSED) {
    return;
  }
  /*
   * DOS-LITERAL FUN_364b_0114 raw 56938: ++unit_type_counts[nation][unit_index]
   * (DS:0x924c, stride 0x13) on every spawn, a plain byte add with no
   * saturation (same as the unguarded -- at FUN_5fef_0352 raw 99570).
   * Without it two wagons finished in the same EOT both pass the cap, which
   * reads the census window the EOT refresh only rewrites later.
   */
  if (ctx->col1_ok && ctx->col1 && uidx >= 0 && uidx < 19 && col->nation_id >= 0 &&
      col->nation_id < 4) {
    ctx->col1->stuff.unit_type_counts[col->nation_id][uidx]++;
  }
  if (ctx->europe && col->nation_id == ctx->human_nation) {
    /* europe->status (never drawn; tests read it) = @BUILT's first sentence. */
    turn_emit_built_chrome(
      ctx->messages, ctx->ai_popups, col, name, -1, ctx->europe->status,
      sizeof(ctx->europe->status)
    );
  }
}

/*
 * The colony end-of-turn pass (audit CO-10): production, which also finishes
 * buildings and unit projects inline in Phase L (FUN_364b_0688 raw
 * 57737-57775), hammers produced that tick or not (port_bugs.md #1475).
 * The nation scoping (turn_prod_skip_* / turn_prod_only_*) stays at the call
 * sites: TURN_PROC_SETUP runs every AI nation, TURN_PROC_FINISH the human.
 */
static void turn_run_colony_eot_with(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out,
  TurnColonyBeforeProduction before, void* before_user
) {
  turn_set_birth_units_pool(ctx->units);
  /* port_bugs.md #770: FUN_4962_0606 (raw 78332-78374) counts the nation's map
   * units before its colonists, so the specialty census needs the pool. */
  const ColonizeWorld pw = world_from_turn_ctx(ctx);
  turn_run_colony_production_with_w(&pw, ctx->human_nation, out, ctx->ai_popups, ctx->messages, before, before_user);
  g_turn_1528_last_walked_colony = NULL;
  turn_set_birth_units_pool(NULL);
}

void turn_run_colony_eot(ColonizeTurnContext* ctx, ColonizeTurnResult* out) {
  turn_run_colony_eot_with(ctx, out, NULL, NULL);
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
    pool, colony, map, NULL, NULL, -1, out ? out : &local, out_delta, NULL, NULL, NULL, NULL
  );
}

/* ===================== Nation tick: bells/crosses tally, immigrants, ship repair (turn_count_bells_and_crosses_for_nation .. turn_euro_nation_is_ref) ===================== */
/*
 * out_colony_bells / out_colony_count (both optional) hand back each active
 * colony's bells in DOS colony order: FUN_364b_0688 (raw 57231) calls
 * FUN_4345_0a22 once per COLONY, so the nation tick has to accrue and test
 * per colony, not once on the sum (port_bugs.md #934).
 */
static int turn_count_bells_and_crosses_for_nation_ex(
  const ColonizeColonyPool* pool,
  int nation_id,
  const ColonizeCol1Save* col1,
  int* out_bells,
  int* out_crosses,
  int* out_colony_bells,
  int* out_colony_count
) {
  int bells = 0;
  int crosses = 0;
  int ncol = 0;
  if (out_colony_count) {
    *out_colony_count = 0;
  }
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
    int colony_bells = 0;
    int colony_crosses = 0;
    if (this_turn_stamp != 0u && c->prod_compose_stamp == this_turn_stamp) {
      /* DOS "one number, two consumers": reuse exactly what this colony's
       * own Phase A composed (smell audit #62) instead of re-tallying after
       * its Phase C/D SoL update and its F/G/H/J roster edits. */
      colony_bells = c->prod_bells_phase_a;
      colony_crosses = c->prod_crosses_phase_a;
    } else {
      /* No compose this turn (human colonies tick later, in
       * TURN_PROC_FINISH; direct callers may have no col1) — a live read
       * here IS the pre-tick state, same as DOS's Phase A would see.
       * sol_b folds into each Statesman/Preacher worker individually,
       * inside colony_prod_colony_bells_ff/_crosses_ff (matches
       * FUN_15eb_1d4c's Statesman/Preacher bodies — see
       * manufacturing_worker_calc_1d4c.md). */
      turn_compose_colony_bells_crosses(
        pool, c, col1, colony_prod_sol_bonus(col1, c), &colony_bells, &colony_crosses
      );
    }
    bells += colony_bells;
    crosses += colony_crosses;
    if (out_colony_bells && ncol < COLONIZE_COLONIES_MAX) {
      out_colony_bells[ncol] = colony_bells;
    }
    ++ncol;
  }
  if (out_bells) {
    *out_bells = bells;
  }
  if (out_crosses) {
    *out_crosses = crosses;
  }
  if (out_colony_count) {
    *out_colony_count = ncol;
  }
  return bells + crosses;
}

static int turn_count_bells_and_crosses_for_nation(
  const ColonizeColonyPool* pool,
  int nation_id,
  const ColonizeCol1Save* col1,
  int* out_bells,
  int* out_crosses
) {
  return turn_count_bells_and_crosses_for_nation_ex(
    pool, nation_id, col1, out_bells, out_crosses, NULL, NULL
  );
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
  }
  diag_info(
    "EUROPE immigrant on the docks: %s (crosses)",
    immigrant_name && immigrant_name[0] ? immigrant_name : ""
  );
  /* 5e52 raw 68596-68600: with tutorial hints on (DS:0x5382 bit7) the
   * first such immigrant also gets @TUTORIAL5, once (DS:0x5387 bit0). */
  const bool tutorial5 = ctx->col1_ok && ctx->col1 &&
                         ctx->col1->head.game_options.tutorial_hints && !ctx->col1->head.tut3.nr5;
  if (tutorial5) {
    ctx->col1->head.tut3.nr5 = 1;
  }
  if (!ctx->ai_popups) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.country = ctx->europe->nation_name[0] ? ctx->europe->nation_name : "";
  /* 5e52 raw 68592: %STRING0 = @HOMEPORT name (DS table -0x7c74), not a
   * literal (port_bugs.md #673). */
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
  if (tutorial5 && ctx->messages) {
    popup_msg_fill(ctx->messages, "TUTORIAL5", &tok, fb, body, sizeof(body));
    ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
  }
}

/*
 * FUN_4345_0a22 wartime branch (raw 73345-73370): bell pool buys the foreign
 * intervention, not a Father. 0a22 runs once per COLONY (FUN_364b_0688 raw
 * 57231) for the ticking colony's owner, so the pool is zeroed at the colony
 * that crosses the threshold and the later colonies' bells refill it
 * (independence-seed-100 tr9 1686: Dutch 4969 -> 360).
 */
static void turn_woi_bell_test(ColonizeTurnContext* ctx, int n, int bells) {
  /*
   * port_bugs.md #231 / DOS FUN_4345_0a22 (viceroy ~73346): once per war,
   * while the REF has not yet landed (0x5382 bit1 clear) and the bell
   * pool has started accruing, show @AMBUSHHINT then @CONSIDER —
   * "%STRING0 is considering intervention ... generate %NUMBER0 liberty
   * bells". Latch = 0x5382 bit 0x04 (game_options.woi_crosses_event,
   * confirmed live 2026-08-18 as exactly this dialog's one-shot).
   */
  /* DOS-LITERAL raw 73347: `(0x5382 & 6) == 0` — bit 0x02 is the
   * intervention-ANNOUNCE latch (game_options.intervention_announced, port_bugs.md
   * #865), bit 0x04 the hint one-shot. */
  /* port_bugs.md #1057c: 0a22 runs per colony for the OWNING nation, so the
   * `bells < pool` test (raw 73345-73346) and the 0f66 %NUMBER0 read the
   * ticking colony's owner, not the human. */
  const int hint_nation =
    (unsigned)bells < founding_fathers_bells_pool(ctx->col1, n) ? n : -1;
  /* Headless (no popup queue) the one-shot still latches: the idle
   * player OKs both notices (independence-seed-100 000003 sets 0x04). */
  if (!ctx->col1->head.game_options.intervention_announced &&
      !ctx->col1->head.game_options.woi_crosses_event &&
      hint_nation >= 0) {
    const int ally = (int)ctx->col1->head.rival_nation_slot_1;
    const char* ally_name = "";
    /* port_bugs.md #252 rule: PARENT country ("France"), never the new-world
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
    tok.number0 = (int)founding_fathers_bells_needed(ctx->col1, hint_nation);
    popup_chrome_ok(
      ctx->ai_popups, ctx->messages, "CONSIDER", &tok,
      ""
    );
    ctx->col1->head.game_options.woi_crosses_event = 1;
  }
  {
    const unsigned pool = founding_fathers_bells_pool(ctx->col1, n);
    const unsigned needed = founding_fathers_bells_needed(ctx->col1, n);
    if (pool < needed) {
      return;
    }
    if (ai_king_spend_woi_bell_pool(ctx, n)) {
      /* port_bugs.md #538: the bell spend only ANNOUNCES (DOS 0a22 ->
       * FUN_43f7_1528); the force itself lands from 2022's free drain
       * on a later turn. */
      founding_fathers_consume_woi_bell_pool(ctx, n);
    }
  }
}

/*
 * One AI nation's FUN_3844_00f2 colony EOT, at the head of its own slot
 * (turn_step_euro) rather than batched before slot 1: 5e52 immigration
 * (raw 58375), then the colony loop 0950 (raw 58384), where each colony's
 * 0688 Phase A accrues bells and re-tests the 0a22 elect before production
 * (raw 57231), then the crosses. Seed-100 1497: Spain's 06d2 rolls fall
 * after France's moves (DOSBox per-slot RNG counts 6/35 for slots 1/2).
 * Each colony's bell election now runs inside the descending colony pass,
 * before that colony produces, as FUN_364b_0688 does. The 1534 Montreal
 * depletion rolls depend on this order: the next French colony's election
 * consumes five draws only after Montreal's production.
 */
typedef struct AiNationColonyBellStep {
  ColonizeTurnContext* ctx;
  ColonizeCol1Nation* nat;
  int nation;
  uint8_t control;
  int crosses;
} AiNationColonyBellStep;

static void turn_ai_colony_bell_step(void* user, ColonizeColony* colony) {
  AiNationColonyBellStep* step = user;
  /* FUN_364b_0688 raw 57224: FUN_281f_09e6 binds DS:0x8542 to this colony. */
  g_turn_1528_last_walked_colony = NULL;
  int bells = 0;
  int crosses = 0;
  turn_compose_colony_bells_crosses(
    step->ctx->colonies, colony, step->ctx->col1,
    colony_prod_sol_bonus(step->ctx->col1, colony), &bells, &crosses
  );
  step->crosses += crosses;
  /* DOS FUN_364b_0688 composes local_ba once, before this colony's 0a22
   * election (raw 57230-57231), and the rebel dividend (raw 57392) reads
   * that same word: a Father elected here (Paine) must not reach this
   * colony's dividend. Stamp it so Phase A keeps it (America seed-100
   * resync 215, Barbados). */
  colony->prod_bells_phase_a = bells;
  colony->prod_crosses_phase_a = crosses;
  colony->prod_compose_stamp = (uint32_t)step->ctx->col1->head.turn + 1u;
  unsigned last = (unsigned)step->nat->liberty_bells_last_turn + (unsigned)bells;
  unsigned pool = (unsigned)step->nat->liberty_bells_pool + (unsigned)bells;
  step->nat->liberty_bells_last_turn = (uint16_t)(last > 65535u ? 65535u : last);
  step->nat->liberty_bells_pool = (uint16_t)(pool > 65535u ? 65535u : pool);
  /* FUN_364b_0688 raw 57231 calls FUN_4345_0a22 before this colony's
   * production; it checks the election even when this colony made no bells. */
  if (step->control == 2) {
  } else if (step->ctx->col1->head.game_options.woi) {
    turn_woi_bell_test(step->ctx, step->nation, bells);
  } else {
    (void)founding_fathers_try_elect(step->ctx, step->nation);
  }
}

/*
 * The human's colony EOT. Same FUN_364b_0688 per-colony bell step as the AI
 * (raw 57231: 0a22 accrues this colony's bells and tests the election before
 * the colony produces), so an election at an early colony leaves the later
 * colonies' bells in the pool and re-opens the nomination
 * (independence-seed-100 000002: pool 182, next father 1). The Europe
 * screen mirrors the human's bell words.
 */
void turn_run_human_colony_eot(ColonizeTurnContext* ctx, ColonizeTurnResult* out) {
  int cursor = COLONIZE_COLONIES_MAX - 1;
  turn_human_colony_eot_begin(ctx, &cursor);
  while (!turn_human_colony_eot_resume(ctx, out, &cursor)) {
  }
}

static bool turn_human_colony_eot_has_col1(const ColonizeTurnContext* ctx) {
  return ctx && ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4;
}

void turn_human_colony_eot_begin(ColonizeTurnContext* ctx, int* cursor) {
  *cursor = COLONIZE_COLONIES_MAX - 1;
  if (!turn_human_colony_eot_has_col1(ctx)) {
    return;
  }
  const int n = ctx->human_nation;
  if (ctx->col1->head.game_options.woi) {
    /* FUN_3844_00f2 raw 58382 `*(DS:0x84fc + 0xe) = 0` runs in the human
     * slot while DS:0x84fc still holds the crown's record during the war
     * (DOSBox BPM, independence 4->5: written from the Dutch slot), so the
     * crown's last-turn bells read 0 at every autosave. */
    ctx->col1->nation[ai_king_crown_nation_col1(ctx->col1, n)].liberty_bells_last_turn = 0;
  }
  ctx->col1->nation[n].liberty_bells_last_turn = 0;
}

bool turn_human_colony_eot_resume(ColonizeTurnContext* ctx, ColonizeTurnResult* out, int* cursor) {
  if (!turn_human_colony_eot_has_col1(ctx)) {
    turn_run_colony_eot(ctx, out);
    *cursor = -1;
    return true;
  }
  const int n = ctx->human_nation;
  ColonizeCol1Nation* nat = &ctx->col1->nation[n];
  /* .crosses: this slice's colonies, each composed at its own Phase A. */
  AiNationColonyBellStep bell_step = {
    .ctx = ctx, .nat = nat, .nation = n, .control = ctx->col1->player[n].control, .crosses = 0
  };
  turn_set_birth_units_pool(ctx->units);
  const ColonizeWorld pw = world_from_turn_ctx(ctx);
  turn_run_colony_production_from_w(
    &pw, n, out, ctx->ai_popups, ctx->messages, turn_ai_colony_bell_step, &bell_step, cursor
  );
  g_turn_1528_last_walked_colony = NULL;
  turn_set_birth_units_pool(NULL);
  /* FUN_364b_0688 raw 57789: nation +0x2e += this colony's crosses (0x8dea)
   * at the end of its own tick (#1822). The Europe screen is the human's
   * authoritative cross counter; the record mirrors it. */
  {
    unsigned cur = (unsigned)(ctx->europe ? ctx->europe->current_crosses : nat->current_crosses) +
                   (unsigned)bell_step.crosses;
    nat->current_crosses = (uint16_t)(cur > 65535u ? 65535u : cur);
  }
  if (ctx->europe) {
    ctx->europe->current_crosses = nat->current_crosses;
    ctx->europe->liberty_bells_pool = nat->liberty_bells_pool;
    ctx->europe->liberty_bells_last_turn = nat->liberty_bells_last_turn;
  }
  return *cursor < 0;
}

/*
 * Colony-screen Space cheat (FUN_2f2b_6372 2f2b:6950 -> FUN_364b_0688 on the
 * viewed colony, with the 0648 redraw after it): ONE colony's production tick
 * outside the turn. It is the colony pass of turn_run_colony_eot_with for a
 * single colony: the same bell step and FUN_364b_0688 body, and none of the nation-level steps around the loop (specialty
 * census, last-turn bell reset, colony crosses). Pressing it twice produces
 * twice, as in DOS. The caller leaves the colony screen when the colony died.
 */
void turn_cheat_colony_tick(ColonizeTurnContext* ctx, ColonizeTurnResult* out, int colony_id) {
  if (!ctx || !ctx->colonies) {
    return;
  }
  ColonizeColony* col = colonies_get_mut(ctx->colonies, colony_id);
  if (!col || !col->active) {
    return;
  }
  const int n = col->nation_id;
  ColonizeCol1Nation* nat = NULL;
  TurnColonyBeforeProduction before = NULL;
  AiNationColonyBellStep bell_step;
  memset(&bell_step, 0, sizeof(bell_step));
  if (ctx->col1_ok && ctx->col1 && n >= 0 && n < 4) {
    nat = &ctx->col1->nation[n];
    bell_step.ctx = ctx;
    bell_step.nat = nat;
    bell_step.nation = n;
    bell_step.control = ctx->col1->player[n].control;
    before = turn_ai_colony_bell_step;
  }
  turn_set_birth_units_pool(ctx->units);
  const ColonizeWorld pw = world_from_turn_ctx(ctx);
  turn_run_one_colony_production_w(
    &pw, col, ctx->human_nation, out, ctx->ai_popups, ctx->messages, before, &bell_step
  );
  g_turn_1528_last_walked_colony = NULL;
  turn_set_birth_units_pool(NULL);
  if (nat && ctx->europe && n == ctx->human_nation) {
    ctx->europe->liberty_bells_pool = nat->liberty_bells_pool;
    ctx->europe->liberty_bells_last_turn = nat->liberty_bells_last_turn;
  }
}

void turn_run_ai_nation_eot(ColonizeTurnContext* ctx, ColonizeTurnResult* out, int n) {
  /* Control byte, not the power at the controls: a hot-seat human's 00f2
   * runs in its own slot, rebound (turn_run_human_colony_eot). */
  if (!ctx || n < 0 || n >= 4 ||
      col1_nation_is_human(ctx->col1_ok ? ctx->col1 : NULL, n, ctx->human_nation)) {
    return;
  }
  if (!ctx->col1_ok || !ctx->col1) {
    turn_take_prof_census(ctx, n); /* 00f2 raw 58374, before the lane tick */
    ai_euro_europe_lane_tick(ctx, n);
    turn_prod_only_nation = n;
    turn_prod_only_set = true;
    turn_run_colony_eot(ctx, out);
    turn_prod_only_nation = -1;
    turn_prod_only_set = false;
    return;
  }
  const uint8_t control = ctx->col1->player[n].control;
  if (control == 2) {
    return; /* withdrawn: no 00f2 */
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[n];
  /* 00f2 raw 58374 FUN_291f_0a9e -> FUN_4962_0606, before 5e52 (58375) and
   * the lane tick (58377): an expert who lands on the dock or is bought this
   * turn does not count until next turn. */
  turn_take_prof_census(ctx, n);
  if (!ctx->col1->head.game_options.woi) {
    ai_nation_reseed(ctx); /* 5e52 entry: FUN_281f_04ca(DS:0x83a6), raw 68542 */
    /* raw 68557: FUN_38fd_0058(0, -1), this nation's own price track. */
    if (ctx->europe) {
      const ColonizeWorld mw = world_from_turn_ctx(ctx);
      europe_nation_tick_market_prices_w(&mw, n, ctx->turn_number ? *ctx->turn_number : 0u);
    }
  }
  /* 5e52 before the colony crosses land: test-saves-ai TURN6->7 nation[3]
   * 12 -> 14 (no arrival) -> 15 with the colony cross. */
  {
    const ColonizeWorld w = world_from_turn_ctx(ctx);
    (void)europe_nation_immigration_tick_w(&w, n);
    /* 5e52 tail (raw 68614): the King's tax roll, peacetime only. */
    if (!ai_king_independence_declared(ctx->col1)) {
      ai_king_tax_event_ai(ctx, n);
    }
  }
  /* 00f2 FUN_291f_0a82 -> FUN_48d3_06ba (raw 58377): after 5e52, so a hull
   * reaching the dock this turn is not yet counted by 584a's dock penalty. */
  ai_euro_europe_lane_tick(ctx, n);
  AiNationColonyBellStep bell_step = {
    .ctx = ctx, .nat = nat, .nation = n, .control = control, .crosses = 0
  };
  nat->liberty_bells_last_turn = 0;
  turn_prod_only_nation = n;
  turn_prod_only_set = true;
  turn_run_colony_eot_with(ctx, out, turn_ai_colony_bell_step, &bell_step);
  turn_prod_only_nation = -1;
  turn_prod_only_set = false;
  {
    unsigned cur = (unsigned)nat->current_crosses + (unsigned)bell_step.crosses;
    if (cur > 65535u) {
      cur = 65535u;
    }
    nat->current_crosses = (uint16_t)cur;
  }
}

/*
 * FUN_48d3_06ba (thunk FUN_291f_0a82, FUN_3844_00f2 raw 58377): the human's
 * Europe pass runs after the whole 5e52 (raw 58375, the 5be8 tax audience
 * included) and before the colony loop (raw 58386). The lane hops (raw
 * 77965-77980), the treasure cash-in at the live tax rate (raw 77981-78027)
 * and the tail (raw 78029-78036: woodcut 9 + the auto-open DS:0x14c) are
 * europe_tick_voyages' work.
 */
void turn_run_human_europe_pass(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->europe) {
    return;
  }
  europe_tick_voyages(ctx->europe, ctx->units);
  if (ctx->europe->docked_with_goods) {
    (void)woodcut_fire(ctx->col1, WOODCUT_CARGO_FROM_THE_NEW_WORLD);
  }
}

void turn_run_nation_ticks(ColonizeTurnContext* ctx, ColonizeTurnResult* out) {
  turn_run_nation_ticks_immigration(ctx, out);
  turn_run_nation_ticks_tail(ctx, out);
}

/* The human 5e52 immigration tick (FUN_3844_00f2 raw 58375). The interactive
 * EOT yields after it when a dialog is queued, so a Brewster pick is answered
 * before the 5be8 tax roll and this turn's colony crosses, as in DOS. */
void turn_run_nation_ticks_immigration(ColonizeTurnContext* ctx, ColonizeTurnResult* out) {
  if (!ctx) {
    return;
  }
  int bells = 0;
  int crosses = 0;
  turn_count_bells_and_crosses_for_nation(
    ctx->colonies, ctx->human_nation, ctx->col1_ok ? ctx->col1 : NULL, &bells, &crosses
  );
  const bool human_bells_per_colony = ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 &&
                                      ctx->human_nation < 4;

  if (ctx->europe) {
    if (ctx->europe->needed_crosses == 0) {
      ctx->europe->needed_crosses = TURN_DEFAULT_NEEDED_CROSSES;
    }
    /*
     * 584a sets needed and adds the idle +2 until the first dock immigrant;
     * this turn's church / colony crosses accrue into current_crosses only
     * AFTER that tick (FUN_3844_00f2 raw 58375 vs 58386 — see below).
     * Cite: Phase M + 5e52; TURN1–7 goldens.
     */
    /* Peacetime with a save: the bells land per colony in
     * turn_run_human_colony_eot instead (0a22 inside 364b_0688). */
    if (!human_bells_per_colony) {
      ctx->europe->liberty_bells_last_turn = (uint16_t)(bells > 65535 ? 65535 : bells);
      unsigned total = (unsigned)ctx->europe->liberty_bells_pool + (unsigned)bells;
      if (total > 65535u) {
        total = 65535u;
      }
      ctx->europe->liberty_bells_pool = (uint16_t)total;
    }
    /* port_bugs.md: no immigration during the War of Independence — Europe is
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
    const bool immigrant_seen_before = ctx->europe && ctx->europe->crosses_immigrant_seen;
    const ColonizeWorld iw = world_from_turn_ctx(ctx);
    /* TB-47: pre-tick crosses and recruit pool, refunded below when the
     * immigrant is refused at the unit cap under !dos_buggy_behavior(). */
    EuropePoolSlot pre_pool[EUROPE_POOL_SIZE];
    uint16_t pre_crosses = 0;
    int16_t pre_pressure = 0;
    int pre_passage = 0;
    if (ctx->europe) {
      memcpy(pre_pool, ctx->europe->pool, sizeof(pre_pool));
      pre_crosses = ctx->europe->current_crosses;
      pre_pressure = ctx->europe->immigration_pressure;
      pre_passage = ctx->europe->recruit_passage;
    }
    const int imm = woi_now ? 0 : europe_tick_immigration_pressure_w(&iw, ctx->human_nation);
    /* FUN_38fd_5e52 asm 38fd:5ebb-5ee4: the human's immigrant music fires once
     * crosses exceed the need, before the Brewster test (38fd:5ee7), so for the
     * Brewster pick and a spawn refused at the unit cap too (#1824). */
    if (imm == 1 || imm == 2) {
      europe_notify_immigrant_sound(ctx->europe); /* 281f_0498(2) */
    }
    if (imm == 2) {
      /* Brewster: player picks from the pool (@RECRUITCHOOSE); applied via
       * units_brewster_apply_popup_ex_w in game_loop. DOS 4884 is a blocking
       * pick inside 5e52 (raw 68607): the EOT yields after this tick, so the
       * answer zeroes the crosses (raw 64766) before this turn's colony
       * crosses land in turn_run_nation_ticks_tail. */
      if (ctx->ai_popups) {
        units_brewster_enqueue_pick(ctx->europe, ctx->ai_popups, ctx->messages, ctx->human_nation);
      } else {
        /* Headless: DOS 4884 resolves the pick inside 5e52, before the colony
         * tick; an idle player's Space takes the first option. */
        (void)units_brewster_apply_slot_w(&iw, ctx->human_nation, 0);
      }
    } else if (imm == 1) {
      /* Mirror dock immigrant as Europe-map unit for Col1 capture. DOS raw
       * 68585-68587: crosses and the pool slot are already spent when
       * FUN_1427_06b4 refuses at the unit limit; the immigrant, its sound
       * and its popup are lost (@TOOMANYUNITS instead).
       * DOS bug, reproduced for now (docs/tolerated_dos_bugs.md TB-47). */
      bool created = true;
      /* The immigrant is the newest row, at the back (raw 68588, #1477). */
      const int row = ctx->europe->dock_count - 1;
      if (ctx->units && row >= 0) {
        /* Gate without the new dock entry: the reservation hook counts
         * unmirrored dock colonists as records already. */
        ctx->europe->dock_count--;
        created = units_spawn_room(ctx->units, ctx->human_nation);
        if (created) {
          ctx->europe->dock_count++;
          EuropeDockImmigrant* d = &ctx->europe->dock[row];
          /* The row owns its mirror from the start (col1 capture writes the
           * dock in row order through it, port_bugs.md #1158), typed by the
           * row's own 0718 roll (port_bugs.md #1463). */
          d->mirror_unit_id = europe_spawn_dock_mirror_unit_typed(
            ctx->units, ctx->human_nation, d->profession, d->dos_type
          );
        } else {
          /* The row's 0718 roll was the one draw (port_bugs.md #1463).
           * raw 68588/68601: the 0x40 latch is set only on success. */
          ctx->europe->crosses_immigrant_seen = immigrant_seen_before;
          ctx->europe->status[0] = '\0';
          /* TB-47: DOS keeps the charge; fixed under !dos_buggy_behavior()
           * (crosses back to the pre-tick count, pool slot restored). */
          if (!dos_buggy_behavior()) {
            ctx->europe->current_crosses = pre_crosses;
            ctx->europe->immigration_pressure = pre_pressure;
            ctx->europe->recruit_passage = pre_passage;
            memcpy(ctx->europe->pool, pre_pool, sizeof(pre_pool));
          }
        }
      }
      if (created) {
        if (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4) {
          ctx->col1->nation[ctx->human_nation].nation_flags |= 0x40u; /* raw 68601 */
        }
        const char* name = "";
        if (ctx->europe->dock_count > 0) {
          name = ctx->europe->dock[ctx->europe->dock_count - 1].name;
        }
        turn_notify_dock_immigrant(ctx, out, name);
      }
    }
    /* FUN_38fd_5e52 asm 38fd:5fdb-6013: every arm past the crosses test
     * (Brewster pick, spawn, spawn refused at the unit limit) ends with the
     * human's dock probe FUN_281f_08bc(head, 0xe) = the largest hold among
     * undamaged hulls on the dock; nonzero sets DS:0x14c, and FUN_3844_00f2
     * raw 58378 opens the Europe screen at the end of the turn. */
    if ((imm == 1 || imm == 2) && out) {
      for (int i = 0; i < ctx->europe->harbor_ships && i < EUROPE_HARBOR_MAX; ++i) {
        if (!ctx->europe->harbor[i].damaged) {
          out->request_europe_open = true;
          break;
        }
      }
    }
  }
}

void turn_run_nation_ticks_tail(ColonizeTurnContext* ctx, ColonizeTurnResult* out) {
  (void)out;
  if (!ctx) {
    return;
  }
  int bells = 0;
  int crosses = 0;
  turn_count_bells_and_crosses_for_nation(
    ctx->colonies, ctx->human_nation, ctx->col1_ok ? ctx->col1 : NULL, &bells, &crosses
  );
  (void)bells;
  const bool human_bells_per_colony = ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 &&
                                      ctx->human_nation < 4;
  if (ctx->europe) {
    /*
     * port_bugs.md #923 / DOS FUN_3844_00f2: the immigration tick FUN_291f_0a90
     * (= FUN_38fd_5e52) runs at raw 58375, BEFORE the per-colony tick
     * FUN_291f_0950 (raw 58386) where nation+0x2e += *0x8dea adds this turn's
     * church crosses. So the threshold test above saw last turn's crosses
     * plus only the 584a +2; this turn's colony output lands here, after it.
     * (The AI arm further down has always been in this order.)
     * #1822: with a save, each colony's crosses are composed at its own
     * Phase A and land in turn_human_colony_eot_resume (raw 57228-57229,
     * 57789), after earlier colonies' elections; only the no-save path
     * adds the live total here.
     */
    if (!human_bells_per_colony) {
      unsigned cur = (unsigned)ctx->europe->current_crosses + (unsigned)crosses;
      if (cur > 65535u) {
        cur = 65535u;
      }
      ctx->europe->current_crosses = (uint16_t)cur;
    }
    /* Keep the col1 nation copy live — the Religious report (F1) reads
     * nation[human].current/needed_crosses, which only the save/load
     * bridge used to refresh, so a live campaign showed no crosses at all
     * (port_bugs.md). Must stay after the tick so the report sees the post-tick
     * value. */
    if (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4) {
      ctx->col1->nation[ctx->human_nation].current_crosses = ctx->europe->current_crosses;
      ctx->col1->nation[ctx->human_nation].needed_crosses = ctx->europe->needed_crosses;
    }
    /* The lane hops, arrival and treasure cash-in run after the 5be8 tax
     * audience: turn_run_human_europe_pass. */
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
      if (n != ctx->human_nation) {
        continue; /* AI nations: turn_run_ai_nation_eot, in their own slot */
      }
      int nb = 0;
      int nc = 0;
      static int s_colony_bells[COLONIZE_COLONIES_MAX];
      int ncol = 0;
      turn_count_bells_and_crosses_for_nation_ex(
        ctx->colonies, n, ctx->col1, &nb, &nc, s_colony_bells, &ncol
      );
      ColonizeCol1Nation* nat = &ctx->col1->nation[n];
      /*
       * DOS-LITERAL FUN_4345_0a22 raw 73341-73342: `+0xc += bells;
       * +0xe += bells;` — +0xc is the live FF bell pool (zeroed on elect,
       * raw 73370) and +0xe is this turn's production (zeroed before the
       * colony loop, FUN_3844_00f2 raw 58382). port_bugs.md #933.
       *
       * port_bugs.md #934: 0a22 runs once per COLONY (FUN_364b_0688 raw 57231),
       * so the elect test is re-run after each colony's bells land — a big
       * empire can cross the threshold twice in one turn. The human's test
       * stays in TURN_PROC_FINISH (port_bugs.md #434), so only the AI nations
       * elect here.
       */
      nat->liberty_bells_last_turn = (uint16_t)(nb > 65535 ? 65535 : nb);
      for (int ci = 0; !human_bells_per_colony && ci < ncol && ci < COLONIZE_COLONIES_MAX; ++ci) {
        if (s_colony_bells[ci] <= 0) {
          continue;
        }
        unsigned pool = (unsigned)nat->liberty_bells_pool + (unsigned)s_colony_bells[ci];
        if (pool > 65535u) {
          pool = 65535u;
        }
        nat->liberty_bells_pool = (uint16_t)pool;
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
        nat->liberty_bells_pool = ctx->europe->liberty_bells_pool;
        nat->liberty_bells_last_turn = ctx->europe->liberty_bells_last_turn;
      }
    }

  }

  /*
   * FUN_3844_00f2 §C tail (viceroy 58393-58423) is NOT ported here: it is
   * the @KINGFRIGATE event — Crown offers a FRIGATE (+10% tax) when the
   * nation's colonies are threatened by warships — and lives in ai_king.c
   * (ai_king_frigate_*). An earlier duplicate here misread type 0x11 as a
   * "Merchantman" and spawned a free ship every 8th turn (port_bugs.md
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
   * Sort ascending; inverse rank[nation] = place (0 = weakest).
   * Cite: viceroy_unpacked.c ~96506–96531; turn/mid_pass_indian_rank.md.
   */
  if (!out_rank) {
    return -1;
  }
  uint16_t score[4];
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
        pop += colonies_headcount(c);
      }
    }
    int gold100 = 0;
    int land = 0;
    if (col1 && n < (int)COLONIZE_COL1_NATION_COUNT) {
      gold100 = (int)(europe_nation_gold(NULL, col1, n) / 100u);
      /* Raw 96513-96515 reads the FUN_4962_0018 census, not live pools:
       * DS:0x9298 colony_counts, DS:0x9410 census_pop_proxy, DS:0x941c
       * land_combat_strength. Fresh 1566 two-turn: Isabella's statesman
       * weight halved on a live recount that ranked Spain above England. */
      colonies_n = col1->stuff.colony_counts[n];
      pop = col1->stuff.census_pop_proxy[n];
      land = (int)col1->stuff.land_combat_strength[n];
    }
    score[n] = gold100 + colonies_n * 2 + pop + land;
    if (getenv("AI_28C8_TRACE")) fprintf(stderr, "[rank in] n%d gold100 %d cols %d pop %d land %d score %d\n", n, gold100, colonies_n, pop, land, score[n]);
  }
  /* DOS-LITERAL FUN_1cf8_000a raw 18654-18738: unsigned ascending
   * words; move the next descent before the first equal-or-greater score.
   * Equal scores keep their order unless a later descent is reinserted. */
  int cursor = 0;
  for (;;) {
    while (cursor < 3 && score[cursor + 1] >= score[cursor]) {
      ++cursor;
    }
    if (cursor == 3) {
      break;
    }
    const uint16_t value = score[cursor + 1];
    const int nation = perm[cursor + 1];
    for (int i = cursor + 1; i < 3; ++i) {
      score[i] = score[i + 1];
      perm[i] = perm[i + 1];
    }
    int dest = 0;
    while (dest < 3 && score[dest] < value) {
      ++dest;
    }
    for (int i = 3; i > dest; --i) {
      score[i] = score[i - 1];
      perm[i] = perm[i - 1];
    }
    score[dest] = value;
    perm[dest] = nation;
  }
  for (int place = 0; place < 4; ++place) {
    out_rank[perm[place]] = (uint8_t)place;
  }
  return 0;
}

static void turn_tally_record(void* user, int type_index, int profession, bool is_hull) {
  uint8_t* hist = user;
  if (!is_hull && units_type_has_profession_slot(type_index) && profession >= 0 &&
      profession < 32 && hist[profession] < 255u) {
    hist[profession]++;
  }
}

void turn_tally_professions_w(const ColonizeWorld* w, int nation_id, uint8_t out_hist[32]) {
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
   *
   * The unit walk (raw 78351-78362) has no position test: every record of
   * the nation counts, Europe docks and lanes included. The human's
   * Europe-side units live only in the EuropeScreen here, so they are added
   * from it, and the pool's (236,236) dock mirrors are skipped for their rows.
   */
  if (!out_hist) {
    return;
  }
  memset(out_hist, 0, 32);
  if (!w || nation_id < 0 || nation_id > 3) {
    return;
  }
  const ColonizeUnitPool* units = w->units;
  const ColonizeColonyPool* colonies = w->colonies;
  const EuropeScreen* eu =
    (w->europe && (int)w->europe->bound_nation == nation_id) ? w->europe : NULL;
  if (units) {
    for (int i = 0; i < units_slot_end(units); ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      if (eu && europe_is_dock_mirror(units, u, nation_id)) {
        continue;
      }
      turn_tally_record(out_hist, u->type_index, u->profession, false);
    }
  }
  europe_for_each_offpool_record(eu, units, turn_tally_record, out_hist);
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
  /* FUN_48d3_0002 raw 77563-77588 reads the census byte, not the live pool:
   * an AI's hull lost or bought since the last 4962_0018 still counts as
   * then. AI hulls never leave the pool, so the census is exact for them;
   * only the human needs the Europe-hull reconstruction below (#1823). */
  if (nation != ctx->human_nation && nation >= 0 && nation < 4 && ctx->col1_ok && ctx->col1) {
    return ctx->col1->stuff.ship_counts[nation];
  }
  int ships = units_count_sea_for_nation(ctx->units, nation);
  if (ctx->europe && (int)ctx->europe->bound_nation == nation) {
    ships += ctx->europe->harbor_ships + ctx->europe->expected_ships + ctx->europe->bound_ships;
  }
  return ships;
}

/*
 * DOS FUN_3844_00f2 raw 58331-58361 for a hull in port (x = nation - 0x14):
 * the repair tick, then @REFIT with %STRING1 the home port (-0x7c74, since
 * FUN_281f_0302 finds no colony there) and DS:0x14c, which opens the Europe
 * screen after the walk. Trace 2026-11-03: "Privateer has completed its
 * repairs in La Rochelle." then the Europe screen (port_bugs.md #1381c).
 */
void units_play_event_sound(int id); /* units_internal.h; as units.c declares it */

int turn_europe_harbor_repairs(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->europe || !ctx->units) {
    return 0;
  }
  int idx[16];
  const int n = europe_harbor_repair_tick(ctx->europe, ctx->units, idx, 16);
  /* Last harbor slot first: the records the save writes for the harbor run in slot order
   * and FUN_3844_00f2 walks the records from the last down (raw 58326-58330). */
  for (int k = (n < 16 ? n : 16) - 1; k >= 0; --k) {
    const EuropeHarborShip* s = &ctx->europe->harbor[idx[k]];
    const ColonizeUnitType* ty =
      units_type(ctx->units, europe_ship_type_index(ctx->units, s));
    units_play_event_sound(0x54); /* same @REFIT sound as units_tick_drydock_repair */
    if (!ctx->ai_popups) {
      continue;
    }
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ty && ty->name[0] ? ty->name : "";
    tok.string1 = reports_home_port_name(ctx->human_nation);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "REFIT", &tok, "", body, sizeof(body));
    ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
  }
  return n;
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
