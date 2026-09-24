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
 *  - Colony production chrome & popups: SoL, inefficient gov, built, need-tools
 *  - Per-colony production tick
 *  - Colony production logging & per-nation scheduling
 */

/* ===================== Colony production chrome & popups: SoL, inefficient gov, built, need-tools (turn_emit_inefficient_gov_chrome .. turn_emit_needtools_notice) ===================== */
static void turn_emit_inefficient_gov_chrome(
  ColonizeColony* colony,
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  int sol_after,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!colony || !col1) {
    return;
  }
  /* DS:0xa897 — dialogs only for the human's own colonies. */
  const bool chrome = (colony->nation_id == human_nation) && europe != NULL;
  /* colonist_count-first fallback, port-wide (see colony_prod_sol_bonus). */
  int pop = colony->colonist_count > 0 ? colony->colonist_count : colony->population;
  if (pop < 0) {
    pop = 0;
  }
  int sol = sol_after;
  if (sol < 0) {
    sol = 0;
  }
  if (sol > 100) {
    sol = 100;
  }
  /* Decomp local_82: trunc tories (not half-up used in colony_prod_sol_bonus). */
  const int tories = (pop * (100 - sol)) / 100;
  /* raw 57470: 10 − difficulty, for every colony (no control test). */
  int diff = (int)col1->head.difficulty;
  if (diff < 0) {
    diff = 0;
  }
  if (diff > 4) {
    diff = 4;
  }
  int thresh = 10 - diff;
  if (thresh < 1) {
    thresh = 1;
  }

  const char* cname = colony->name[0] ? colony->name : "colony";
  const char* section = NULL;
  char status_buf[sizeof(europe->status)];
  status_buf[0] = '\0';

  if (tories < thresh) {
    if ((colony->colony_flags & COLONIZE_COLONY_FLAG_INEFFICIENT_GOV) != 0) {
      colony->colony_flags =
        (uint8_t)(colony->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_INEFFICIENT_GOV);
      if (chrome && turn_report_ok_inefficient(col1)) {
        section = "EFFICIENT";
        snprintf(
          status_buf,
          sizeof(status_buf),
          "%s government efficiency improved.",
          cname
        );
      }
    }
  } else {
    if ((colony->colony_flags & COLONIZE_COLONY_FLAG_INEFFICIENT_GOV) == 0) {
      colony->colony_flags |= COLONIZE_COLONY_FLAG_INEFFICIENT_GOV;
      if (chrome && turn_report_ok_inefficient(col1)) {
        section = "INEFFICIENT";
        snprintf(
          status_buf,
          sizeof(status_buf),
          "%s has inefficient government (%d+ tories).",
          cname,
          thresh
        );
      }
    }
  }

  if (!chrome || !status_buf[0]) {
    return;
  }
  snprintf(europe->status, sizeof(europe->status), "%s", status_buf);
  if (!ai_popups || !section) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  tok.number0 = thresh;
  tok.has_number0 = true;
  popup_msg_fill(messages, section, &tok, status_buf, body, sizeof(body));
  ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
}

/*
 * FUN_364b_0688 Phase D: one SoL latch or decade chrome popup for the human.
 * Match decomp nest: latch transitions before decade. Cite: colony_eot_production.md.
 */
static void turn_emit_sol_phase_d_chrome(
  ColonizeColony* colony,
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  int sol_before,
  uint8_t flags_before,
  int sol_after,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!colony || colony->nation_id != human_nation || !europe) {
    return;
  }
  const char* section = NULL;
  const char* fallback = NULL;
  char status_buf[sizeof(europe->status)];
  status_buf[0] = '\0';
  const char* cname = colony->name[0] ? colony->name : "colony";
  const int had50 = (flags_before & COLONIZE_COLONY_FLAG_SOL_50) != 0;
  const int had100 = (flags_before & COLONIZE_COLONY_FLAG_SOL_100) != 0;

  if ((sol_after < 50) || had50) {
    if ((sol_after < 100) || had100) {
      if (sol_after < 95 && had100) {
        if (turn_report_ok_rebel_maj(col1)) {
          section = "TORYMINORITY";
          snprintf(
            status_buf,
            sizeof(status_buf),
            "SoL in %s down from 100%% to %d%%.",
            cname,
            sol_after
          );
          fallback = status_buf;
        }
      } else if (sol_after < 50 && had50) {
        if (turn_report_ok_rebel_maj(col1)) {
          section = "TORYMAJORITY";
          snprintf(
            status_buf,
            sizeof(status_buf),
            "SoL in %s down to %d%%.",
            cname,
            sol_after
          );
          fallback = status_buf;
        }
      } else if (sol_before / 10 < sol_after / 10) {
        if (turn_report_ok_sons(col1)) {
          section = "SONSUP";
          snprintf(
            status_buf, sizeof(status_buf), "SoL in %s up to %d%%.", cname, sol_after
          );
          fallback = status_buf;
        }
      } else if ((sol_after + 4) / 10 < sol_before / 10) {
        if (turn_report_ok_sons(col1)) {
          section = "SONSDOWN";
          snprintf(
            status_buf, sizeof(status_buf), "SoL in %s down to %d%%.", cname, sol_after
          );
          fallback = status_buf;
        }
      }
    } else if (turn_report_ok_rebel_maj(col1)) {
      section = "REBELUNANIMOUS";
      snprintf(status_buf, sizeof(status_buf), "SoL in %s up to 100%%.", cname);
      fallback = status_buf;
    }
  } else if (turn_report_ok_rebel_maj(col1)) {
    section = "REBELMAJORITY";
    snprintf(
      status_buf, sizeof(status_buf), "SoL in %s up to %d%%.", cname, sol_after
    );
    fallback = status_buf;
  }

  if (!fallback || !fallback[0]) {
    return;
  }
  snprintf(europe->status, sizeof(europe->status), "%s", fallback);
  if (!ai_popups || !section) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  if (col1 && colony->nation_id >= 0 &&
      colony->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
      col1->player[colony->nation_id].country_name[0]) {
    tok.string1 = col1->player[colony->nation_id].country_name;
  } else if (europe->nation_name[0]) {
    tok.string1 = europe->nation_name;
  } else {
    tok.string1 = "Europe";
  }
  tok.number0 = sol_after;
  tok.has_number0 = true;
  popup_msg_fill(messages, section, &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
}

/*
 * LABELS.TXT for status-line wording, set by turn_processor_advance for the
 * duration of a slice. NULL outside the processor (unit tests) — every reader
 * passes a literal fallback.
 */
const ColonizeMsgCatalog* turn_labels = NULL;

static const char* turn_label(const char* section, int idx, const char* fallback) {
  if (turn_labels && section) {
    const ColonizeMsgSection* sec = assets_msg_find(turn_labels, section);
    if (sec && idx >= 0 && idx < sec->line_count && sec->lines[idx][0]) {
      return sec->lines[idx];
    }
  }
  return fallback;
}

/* Header comment on the prototype in turn.h. */
void turn_compose_colony_bells_crosses(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeCol1Save* col1,
  int sol_bonus,
  int* out_bells,
  int* out_crosses
) {
  int bells = 0;
  int crosses = 0;
  if (pool && colony && colony->active) {
    const int nation_id = colony->nation_id;
    /* Jefferson / Paine / Penn — fandom_col1994.md Political / Religious FF. */
    const int statesmen_pct =
      (col1 && founding_fathers_nation_has(col1, nation_id, FF_THOMAS_JEFFERSON)) ? 50 : 0;
    const int paine_tax_pct =
      (col1 && founding_fathers_nation_has(col1, nation_id, FF_THOMAS_PAINE) && nation_id >= 0 &&
       nation_id < (int)COLONIZE_COL1_NATION_COUNT)
        ? (int)col1->nation[nation_id].tax_rate
        : 0;
    const bool nation_has_penn =
      col1 && founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_PENN);
    const bool nation_is_ai = col1 && nation_id >= 0 &&
                              nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
                              col1->player[nation_id].control != 0;
    /* DOS-LITERAL FUN_15eb_1f72 raw 11327 (viceroy_unpacked.c:12624-12626):
     * the AI bells subsidy is gated on BOTH the AI/non-human control byte
     * (nation >= 4 || byte[nation*0x34+0x543f] != 0, i.e. nation_is_ai) AND
     * `FUN_15eb_3960(nation, 0x12)` — FF slot 0x12 = 18 = FF_SIMON_BOLIVAR
     * held by that nation. The port previously keyed the subsidy arg (used
     * ONLY for that (+ (pop+3)/5) add, colony_production.c:870) on
     * nation_is_ai alone, over-granting the subsidy to AI nations that
     * haven't recruited Bolivar yet. bugs.md #938b. */
    const bool nation_is_ai_bolivar_subsidy =
      nation_is_ai && founding_fathers_nation_has(col1, nation_id, FF_SIMON_BOLIVAR);
    bells = colony_prod_colony_bells_ff(
      pool, colony, statesmen_pct, paine_tax_pct, nation_is_ai_bolivar_subsidy, sol_bonus
    );
    crosses = colony_prod_colony_crosses_ff(pool, colony, nation_has_penn, sol_bonus);
  }
  if (out_bells) {
    *out_bells = bells;
  }
  if (out_crosses) {
    *out_crosses = crosses;
  }
}

/*
 * DOS @BUILT — "%STRING0 colony produces {%STRING1}." One emitter for the
 * three completion paths (audit CO-11): turn_produce_one_colony's in-tick
 * building completion, turn_run_colony_unit_construction and
 * turn_run_colony_building_completion. The caller writes europe->status
 * first and passes it as `fallback`, exactly as all three did inline.
 */
void turn_emit_built_chrome(
  const ColonizeMsgCatalog* messages,
  AiPopupState* ai_popups,
  const ColonizeColony* colony,
  const char* built_name,
  const char* fallback
) {
  if (!ai_popups || !colony) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = colony->name[0] ? colony->name : "colony";
  tok.string1 = (built_name && built_name[0]) ? built_name : "building";
  popup_msg_fill(messages, "BUILT", &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
}

/*
 * DOS-LITERAL FUN_364b_0688 Phase L construction tools check
 * (viceroy_unpacked.c raw 57737-57771). After banking this tick's hammers
 * DOS resolves the current project's cost with `FUN_281f_0ac4(0x281f,
 * colony+0x94, &local_e)` — return = hammers required, `local_e` = tools
 * required — and that resolver covers BOTH real buildings and the
 * unit-type projects (Artillery, Wagon Train, ships). Then:
 *
 *   if (colony->hammers >= hammers_need && project >= 0)          [raw 57739]
 *     if (project is a building already owned) -> @ALREADYHAVE     [raw 57783]
 *     else if (colony+0xb6 (tools) < local_e) {                    [raw 57748]
 *       if (nation < 4 && control byte 0x543f == 0)                 human
 *         if ((*(byte*)0x5384 & 0x10) == 0) {                       report opt
 *           %STRING1 = project name (FUN_281f_0d4e/0416)
 *           %NUMBER0 = tools required, %NUMBER1 = tools on hand
 *           tag = @NEEDTOOLS (DS:0xea1), plus "0" (DS:0xeab) -> @NEEDTOOLS0
 *           when the colony holds no tools at all
 *         }
 *       else colony+0xb6 = local_e;   (AI is handed the tools — ported in
 *                                       colonies_try_complete_unit_construction,
 *                                       the ONLY site this arm applies to:
 *                                       that resolver's project is always a
 *                                       unit-type project, never a real
 *                                       building. bugs.md #755.)
 *     }
 *
 * No latch: DOS re-runs this every EOT, so the notice repeats each turn the
 * project stays stalled on tools. It is NOT gated on hammers having been
 * produced this tick (DOS adds 0 and falls through), which is why this sits
 * outside turn_produce_one_colony's `hammers_add > 0` block — bugs.md #537,
 * where an idle-carpenter or BUY-topped colony (and every unit project) went
 * silent.
 */
static void turn_emit_needtools_notice(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!pool || !colony || colony->building_in_production < 0) {
    return;
  }
  if (colony->nation_id != human_nation) {
    return;
  }
  /* Gate !(DS:0x5384 & 0x10). */
  if (col1 && col1->head.colony_report_options.report_tools_needed_for_production) {
    return;
  }
  const int bip = colony->building_in_production;
  const char* pname = NULL;
  int hammers_need = 0;
  int tools_cost = 0;
  if (!colonies_unit_build_info(bip, &pname, &hammers_need, &tools_cost)) {
    const ColonizeBuildingType* bt = colonies_building_type(pool, bip);
    if (!bt) {
      return;
    }
    /* Already-owned project takes DOS's @ALREADYHAVE arm instead. */
    if (bip < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[bip]) {
      return;
    }
    pname = bt->name;
    hammers_need = bt->hammers;
    tools_cost = bt->tools_cost;
  }
  if (hammers_need <= 0 || colony->hammers < hammers_need) {
    return;
  }
  const int tools_have = colony->stock[COLONIZE_CARGO_TOOLS];
  if (tools_cost <= 0 || tools_have >= tools_cost) {
    return;
  }
  const char* fallback = "Need tools.";
  if (europe) {
    snprintf(europe->status, sizeof(europe->status), "Need tools.");
    fallback = europe->status;
  }
  if (!ai_popups) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = colony->name[0] ? colony->name : "colony";
  tok.string1 = (pname && pname[0]) ? pname : "building";
  tok.number0 = tools_cost;
  tok.has_number0 = true;
  const char* section = "NEEDTOOLS0";
  if (tools_have > 0) {
    section = "NEEDTOOLS";
    tok.number1 = tools_have;
    tok.has_number1 = true;
  }
  popup_msg_fill(messages, section, &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
}

/*
 * Per-nation SPECIALTY CENSUS — DOS's DS:0x9430 block (addressed as
 * -0x6bd0), 0x1d bytes, one per @JOB row. Its only writer is
 * `FUN_4962_0606` (raw 78332-78372): zero the block, then count every unit
 * of the nation whose type carries a profession slot (`FUN_281f_0b78` ->
 * FUN_15eb_0902 = units_type_default_job >= 0) by its +0x315b profession,
 * then every colonist of every colony of the nation by its specialty
 * (`FUN_281f_0c54`). The host `FUN_3844_00f2` (raw 58374) calls it through
 * `FUN_291f_0a9e` once per nation, immediately BEFORE that nation's colony
 * production loop, so the census a colony reads is the one the nation
 * started its turn with.
 *
 * The on-the-job learning gate (raw 57595) reads this block: a colonist can
 * learn field job j on the job only while the nation owns ZERO specialists
 * of j anywhere — and a success bumps the byte in place (raw 57605), so the
 * first graduate of the turn also closes the door behind him.
 *
 * (The port used to invent an `s_otj_latch` "one per job per nation per
 * turn" and cite raw 78140 for clearing it; 78140 clears +0x942c, an
 * unrelated per-nation flag block. bugs.md #563.)
 */
uint8_t turn_prof_census[COLONIZE_COL1_NATION_COUNT][32];

/* ===================== Per-colony production tick (turn_produce_one_colony) ===================== */
void turn_produce_one_colony(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  ColonizeTurnResult* out,
  ColonizeColonyProdDelta* delta,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages,
  ColonizeDosRng* rng
) {
  /* The delta doubles as this tick's per-cargo net ledger (the DOS
   * FUN_281f_0b50 scratch), which the cargo_produced_mask below needs even
   * when the caller wants no delta back. */
  ColonizeColonyProdDelta local_delta;
  if (!delta) {
    delta = &local_delta;
  }
  memset(delta, 0, sizeof(*delta));
  if (!pool || !colony || !colony->active) {
    return;
  }
  /* FUN_364b_0688: clear cargo_produced_mask (+0x90) at production start. */
  colony->cargo_produced_mask = 0;
  /* bugs.md #397: europe->status is shared across the whole colony loop, so
   * "status empty" as the Phase K crumb gate let ANY earlier colony's
   * message (birth, food low, cargo ready, ...) suppress every later
   * colony's "has run out of X" popup for the rest of the EOT. Snapshot the
   * strip at THIS colony's entry: the crumb yields only to a message this
   * same colony wrote this tick (its own food/birth chrome outranks it),
   * never to another colony's leftovers. */
  char status_at_entry[sizeof(europe->status)];
  status_at_entry[0] = '\0';
  if (europe) {
    snprintf(status_at_entry, sizeof(status_at_entry), "%s", europe->status);
  }
  int stock_before[COLONIZE_CARGO_COUNT];
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    stock_before[c] = colony->stock[c];
  }
  /* Phase I birth food (200) and Phase B Custom House sales, kept apart from
   * the delta so the produced-mask below can see the Phase B net exactly. */
  int birth_food_debit = 0;
  int ai_food_subsidy = 0;
  int ch_sold[COLONIZE_CARGO_COUNT];
  memset(ch_sold, 0, sizeof(ch_sold));
  const int pop = colony->colonist_count > 0 ? colony->colonist_count : colony->population;
  if (pop <= 0) {
    return;
  }

  int field_food = 0;
  /* DOS 0xa896: ore/silver deposit "depletion units" tallied in FUN_15eb_18ec,
   * rolled down in the FUN_364b_0688 epilogue (see bottom of this function). */
  int depl_tx[2 * COLONIZE_COLONY_FIELD_TILES_MAX];
  int depl_ty[2 * COLONIZE_COLONY_FIELD_TILES_MAX];
  int depl_n = 0;
  int field_lumber = 0;
  int field_ore = 0;
  /* bugs.md (403 follow-up): this turn's field production per cargo — a
   * "has run out of X" crumb must stay quiet while X is still being
   * produced, even insufficiently (same rule the lumber crumb already
   * applied via field_lumber). */
  int field_prod[COLONIZE_CARGO_COUNT];
  memset(field_prod, 0, sizeof(field_prod));

  /* Town commons (center tile) + area-view field workers. */
  const bool has_hudson =
    col1 && founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON);
  if (map) {
    ColonizeTownCommonsYield tc;
    colony_yield_town_commons(
      map, colony->x, colony->y, colony->colony_flags,
      col1 ? (int)col1->head.difficulty : 4, &tc
    );
    if (tc.food > 0) {
      colony->stock[COLONIZE_CARGO_FOOD] =
        clamp_int(colony->stock[COLONIZE_CARGO_FOOD] + tc.food, 0, 65535);
      field_food += tc.food;
      if (delta) {
        delta->goods[COLONIZE_CARGO_FOOD] += tc.food;
      }
    }
    if (tc.secondary_amount > 0 && tc.secondary_cargo >= 0 &&
        tc.secondary_cargo < COLONIZE_CARGO_COUNT) {
      colony->stock[tc.secondary_cargo] =
        clamp_int(colony->stock[tc.secondary_cargo] + tc.secondary_amount, 0, 65535);
      if (delta) {
        delta->goods[tc.secondary_cargo] += tc.secondary_amount;
      }
      field_prod[tc.secondary_cargo] += tc.secondary_amount;
      if (tc.secondary_cargo == COLONIZE_CARGO_LUMBER) {
        field_lumber += tc.secondary_amount;
      } else if (tc.secondary_cargo == COLONIZE_CARGO_ORE) {
        field_ore += tc.secondary_amount;
      }
    }

    /* Docks (or Drydock/Shipyard) gates Fisherman yield — FUN_15eb_18ec
     * ~11925-11939; coastal placement alone is not enough. One scan shared
     * with colony_preview.c / colony_screen.c / game_loop.c. */
    const bool has_docks = colony_yield_colony_has_docks(pool, colony);
    ColonizeWorkedTileIter wit;
    ColonizeWorkedTile w;
    colony_yield_worked_tiles_begin(&wit, colony);
    while (colony_yield_worked_tiles_next(&wit, &w)) {
      const ColonizeColonist* c = w.colonist;
      const int dx = w.dx;
      const int dy = w.dy;
      /* DOS net SoL/Tory mod (sons_of_liberty.md). Field-specific variant:
       * zeroed outright for AI colonies, unlike manufacturing/bells/crosses/
       * hammers — see colony_prod_sol_bonus_field. Folded into
       * colony_yield_for_worker directly now (2026-08-15): DOS applies a
       * *positive* mod before expert doubling (so it gets swept up by an
       * expert's ×2/road-river-unit-doubling, not added flat after —
       * player-confirmed, see colony_yield.c's colony_yield_pipeline
       * comment); a negative mod (Tory penalty) still lands at the very
       * end, same net position as this function's old external add. */
      const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);
      /* Henry Hudson (@FF 8) doubles Fur Trapper output INSIDE the pipeline,
       * at DOS's own spot (FUN_15eb_18ec 11970-11973) — see colony_yield.c.
       * It used to be a `*= 2` here, after Convert +1 and after the Tory
       * subtraction, which is a different number (smell audit #60). */
      const int yld = colony_yield_for_worker(
        map,
        colony->x + dx,
        colony->y + dy,
        c->field_job,
        c->profession,
        has_docks,
        sol_b_field,
        colony->colony_flags,
        has_hudson
      );
      const int cargo = colony_yield_job_cargo(c->field_job);
      /*
       * Col1 +0x97 depletion units — FUN_15eb_18ec raw 11913-11923 (DS
       * 0xa896): Minerals(6)+Ore Miner -> +1, Minerals(6)+Silver Miner -> +2,
       * Silver Deposit(12)+Silver Miner -> +1. DOS tallies this from
       * (resource, job) alone as it walks the 5x5 field loop, BEFORE the
       * silver collapse / improvement stack / negative-SoL tail that can
       * drive `yld` to <= 0 for this same plot — a worked deposit still
       * depletes even on a yield-0 turn (bugs.md #894). Hoisted above the
       * `yld <= 0` gate below; nothing else in this block may move.
       */
      if (cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) {
        const int res = map_resource_type_for_yield(map, colony->x + dx, colony->y + dy);
        int units = 0;
        if (res == 6 && cargo == COLONIZE_CARGO_ORE) {
          units = 1;
        } else if (res == 6 && cargo == COLONIZE_CARGO_SILVER) {
          units = 2;
        } else if (res == 12 && cargo == COLONIZE_CARGO_SILVER) {
          units = 1;
        }
        while (units-- > 0 && depl_n < (int)(2 * COLONIZE_COLONY_FIELD_TILES_MAX)) {
          depl_tx[depl_n] = colony->x + dx;
          depl_ty[depl_n] = colony->y + dy;
          depl_n++;
        }
      }
      if (yld <= 0) {
        continue;
      }
      int add = yld;
      if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
        continue;
      }
      colony->stock[cargo] = clamp_int(colony->stock[cargo] + add, 0, 65535);
      if (delta) {
        delta->goods[cargo] += add;
      }
      field_prod[cargo] += add;
      if (cargo == COLONIZE_CARGO_FOOD) {
        field_food += add;
      } else if (cargo == COLONIZE_CARGO_LUMBER) {
        field_lumber += add;
      } else if (cargo == COLONIZE_CARGO_ORE) {
        field_ore += add;
      }
    }
  }

  /*
   * ---- Phase A composition boundary (smell audit #62 / #63) ----
   *
   * DOS composes EVERY cargo — field yields, settlement manufacturing,
   * hammers, bells, crosses — exactly once, in FUN_364b_0688's prologue
   * (`FUN_281f_0c22` → `15eb_3956` → `15eb_1f72`, viceroy_unpacked.c 57228
   * and 12581-12610: clear the 20-word gross scratch at −0x7238, then the
   * 5x5 `FUN_15eb_18ec` field loop and the per-colonist `FUN_15eb_1d4c`
   * manufacturing loop both accumulate INTO it). Every later phase only
   * *reads* that scratch back through `FUN_281f_0b50`: Phase B applies
   * cargos 0..15 (57238), Phase L banks hammers `0b50(0x10)` (57731), Phase
   * A itself feeds bells `0b50(0x12)` to the nation (57230).
   *
   * So the SoL number (`FUN_15eb_0274` + the +0x1c latch bits), the
   * colonist professions and the population that compose the yields are all
   * read BEFORE Phase C/D update the SoL accumulators and latch bits
   * (57349-57485), before F/G/H education rewrites professions
   * (57502-57614), and before I/J birth and starve-kill change the roster
   * (57615-57695).
   *
   * The port used to call `colony_craft_one_colony` and
   * `colony_prod_colony_hammers` down at the Phase L position with a
   * freshly-read `colony_prod_sol_bonus()` and the already-mutated colonist
   * array: one tick composed field yields from one snapshot and craft/
   * hammers from another (a graduate produced at his new rate the same tick
   * he graduated; a starved Blacksmith's tools vanished retroactively), and
   * the Production preview — which reads everything pre-update — could not
   * structurally match the tick on a latch-crossing turn. Composing here,
   * at the DOS phase point, restores the "one number, two consumers"
   * invariant colony_production.c:419-433 already asserts for bells.
   *
   * Application stays where it was: craft output lands in Phase B (same
   * place DOS applies it), the composed hammer count is banked at Phase L
   * below.
   */
  const int sol_b_phase_a = colony_prod_sol_bonus(col1, colony);
  /*
   * DOS-LITERAL FUN_364b_0688 Phase B raw 57241-57243: the per-cargo net is
   * `FUN_281f_0b50(cargo)` (gross - demand - unmet[input]) only for the
   * human seat; `(3 < uVar7) || byte[uVar7*0x34+0x543f] != 0` — the same
   * control gate the AI food subsidy below reads, polarity 0 = human —
   * swaps in `gross - demand`, dropping the unmet term, so an AI craft
   * building never loses output to a short warehouse (bugs.md #898).
   */
  const bool colony_ai_controlled =
    col1 != NULL && colony->nation_id >= 0 &&
    (colony->nation_id >= (int)COLONIZE_COL1_NATION_COUNT ||
     col1->player[colony->nation_id].control != 0);
  /*
   * Phase K (bugs.md #899/#911) probes two DOS scratch words per craft pair,
   * not the raw good's stock (FUN_364b_0688 raw 57696-57728):
   *
   *   unmet[input]        DS:0x8e5a + 2*cargo — "the staffed worker wanted
   *                       input the warehouse did not have"
   *                       (FUN_15eb_0bd4 `U = D - (stock + gross_in)`). In
   *                       colony_craft.c's pass that is exactly
   *                       `actual_out < total_out` for the recipe, so read
   *                       it off a non-mutating preview over the SAME
   *                       pre-craft stock: capacity_out[o] is total_out and
   *                       gross_out[o] is actual_out. (The preview's
   *                       shortfall[] array cannot be used: it sums the
   *                       output- and input-side shortfalls of DIFFERENT
   *                       recipes into one slot for tools.) ai_controlled
   *                       is deliberately false here — DOS's ledger writes
   *                       the unmet word for every colony; only Phase B's
   *                       net drops the term (bugs.md #898).
   *   FUN_281f_0b50(out)  this tick's NET stock change for the finished
   *                       good (gross - demand - unmet[input]) — taken here
   *                       as the stock delta across the craft pass itself,
   *                       which is exactly that for every craft cargo and,
   *                       unlike craft_gross, nets the gunsmith's tools
   *                       consumption out of tools (the @ORE arm compares
   *                       net muskets against net tools).
   */
  int craft_unmet_cap[COLONIZE_CARGO_COUNT];
  int craft_unmet_gross[COLONIZE_CARGO_COUNT];
  {
    ColonizeColony craft_scratch = *colony;
    colony_craft_preview(
      pool, &craft_scratch, NULL, NULL, sol_b_phase_a, craft_unmet_gross,
      craft_unmet_cap, false
    );
  }
  int craft_stock_pre[COLONIZE_CARGO_COUNT];
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    craft_stock_pre[c] = colony->stock[c];
  }
  int craft_gross[COLONIZE_CARGO_COUNT];
  colony_craft_one_colony_ex(
    pool, colony, delta, sol_b_phase_a, colony_ai_controlled, craft_gross
  );
  int craft_net[COLONIZE_CARGO_COUNT];
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    craft_net[c] = colony->stock[c] - craft_stock_pre[c];
  }
  (void)craft_gross;
  /* Composed here (Phase A), banked at Phase L — DOS `0b50(0x10)`. Every
   * tick: the old "Autumn freeze" gate (bugs.md #466) rested on a real-DOS
   * Spring→Autumn pair in which no colony staffed a carpenter at all. */
  const int hammers_phase_a = colony_prod_colony_hammers(pool, colony, sol_b_phase_a, NULL);
  /* Bells + crosses composed here too (DOS `0b50(0x12)` / Phase M), stamped
   * for turn_run_nation_ticks — which for AI nations runs after this tick
   * and would otherwise re-tally them off the already-updated SoL latch and
   * the already-educated/starved roster. See ColonizeColony's
   * prod_compose_stamp comment. */
  colony->prod_compose_stamp = 0;
  if (col1) {
    turn_compose_colony_bells_crosses(
      pool, colony, col1, sol_b_phase_a, &colony->prod_bells_phase_a,
      &colony->prod_crosses_phase_a
    );
    colony->prod_compose_stamp = (uint32_t)col1->head.turn + 1u;
  }

  /*
   * FUN_364b_0688 Phase B: AI Euro food += difficulty>>1 (DS 0x53a6).
   * Cite: colony_eot_production.md; difficulty.md.
   */
  if (col1 && colony->nation_id >= 0 &&
      colony->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
      col1->player[colony->nation_id].control != 0) {
    int diff = (int)col1->head.difficulty;
    if (diff < 0) {
      diff = 0;
    }
    if (diff > 4) {
      diff = 4;
    }
    const int ai_food = diff >> 1;
    if (ai_food > 0) {
      colony->stock[COLONIZE_CARGO_FOOD] =
        clamp_int(colony->stock[COLONIZE_CARGO_FOOD] + ai_food, 0, 65535);
      field_food += ai_food;
      ai_food_subsidy = ai_food;
      if (delta) {
        delta->goods[COLONIZE_CARGO_FOOD] += ai_food;
      }
    }
  }

  const int consumed = pop * TURN_FOOD_PER_COLONIST;
  colony->stock[COLONIZE_CARGO_FOOD] =
    clamp_int(colony->stock[COLONIZE_CARGO_FOOD] - consumed, 0, 65535);
  if (delta) {
    delta->goods[COLONIZE_CARGO_FOOD] -= consumed;
  }
  if (field_food < consumed && out) {
    out->food_shortages++;
    if (europe && colony->nation_id == human_nation && turn_report_ok_food(col1)) {
      if (colony->name[0]) {
        snprintf(europe->status, sizeof(europe->status), "Food shortage in %s.", colony->name);
      } else {
        snprintf(europe->status, sizeof(europe->status), "Food shortage.");
      }
    }
  }
  /*
   * ---- Phases C / D — rebel accumulators + SoL latch ----
   *
   * DOS order is C (57349-57414) → D (57415-57485) → E → F/G/H education
   * (57502-57614) → I birth (57615) → J starve-kill (57623-57695); see
   * turn/colony_eot_production.md's phase table. The port used to run I and
   * J here, ahead of C/D, so `rebel_divisor += pop*2`, the accumulator's
   * bells and `colony_prod_sol_bonus`'s Tory head count all read a roster
   * DOS never shows them — one that had already gained a newborn or lost a
   * starved colonist. Phase C reads the population byte `+0x1f` directly
   * (viceroy_unpacked.c:57377), i.e. the pre-birth, pre-starve count, and
   * Phase J's abandon jumps straight to the epilogue (`goto LAB_364b_1ae2`,
   * :57692) with C/D and F/G/H already done. The I/J block now sits below
   * education, at its DOS position.
   */
  {
    const int sol_before = colony_prod_sol_percent(col1, colony);
    const uint8_t flags_before = colony->colony_flags;
    colony_prod_tick_rebel_accumulators(pool, colony, col1);
    const int sol_after = colony_prod_sol_percent(col1, colony);
    colony_prod_refresh_sol_flags(colony, col1);
    turn_emit_sol_phase_d_chrome(
      colony,
      col1,
      europe,
      human_nation,
      sol_before,
      flags_before,
      sol_after,
      ai_popups,
      messages
    );
    turn_emit_inefficient_gov_chrome(
      colony, col1, europe, human_nation, sol_after, ai_popups, messages
    );
  }

  /*
   * Horse breeding — FUN_15eb_1f72 tail, DOS-confirmed 2026-08-26 against
   * real DOS ground truth (golden_colony_prod01/02, 13/13 Dutch colonies
   * exact) — see colony_prod_horse_breed's header comment in
   * colony_production.h for the full derivation. Replaces the old
   * "manual/fandom" approximation (flat food-surplus/2 capped at 6-or-8)
   * with the DOS-confirmed herd-size-based potential (ceil(horses/divisor)*2,
   * divisor 25 with a Stable else 50), capped by this turn's food surplus
   * and warehouse headroom.
   */
  {
    const bool horse_has_stable = colonies_has_building_row(pool, colony, COLONY_BUILDING_STABLE);
    const int horse_warehouse_cap =
      colonies_warehouse_capacity(pool, colony, COLONIZE_CARGO_HORSES);
    const ColonyProdHorseBreed breed = colony_prod_horse_breed(
      colony->stock[COLONIZE_CARGO_HORSES],
      pop,
      field_food,
      horse_warehouse_cap,
      horse_has_stable
    );
    if (breed.bred > 0) {
      colony->stock[COLONIZE_CARGO_FOOD] =
        clamp_int(colony->stock[COLONIZE_CARGO_FOOD] - breed.bred, 0, 65535);
      colony->stock[COLONIZE_CARGO_HORSES] =
        clamp_int(colony->stock[COLONIZE_CARGO_HORSES] + breed.bred, 0, 65535);
      if (delta) {
        delta->goods[COLONIZE_CARGO_FOOD] -= breed.bred;
        delta->goods[COLONIZE_CARGO_HORSES] += breed.bred;
      }
      if (europe && colony->nation_id == human_nation) {
        snprintf(
          europe->status,
          sizeof(europe->status),
          horse_has_stable ? "Stable bred %d horses." : "Horses bred: %d.",
          breed.bred
        );
      }
    }
  }

  /*
   * FUN_364b_0688 phases F–G: Schoolhouse/College/University education.
   * Cite: viceroy_unpacked.c ~57502-57589; GAME.TXT @TRAINFAIL /
   * @TRAINCRIMINAL / @TRAININDENTURED / @TRAINPROFESSION;
   * docs/building_production.md, colony_eot_production.md.
   *
   * bugs.md #380 ("has education even been implemented?" — it had not). Every
   * axis of the previous port was wrong, and nothing could ever graduate:
   *   - A TEACHER is a colonist whose WORK SLOT is the school (DOS occupation
   *     0x12, i.e. the @JOB 18 "Teacher" slot, `local_c2 == 0x12`) and whose
   *     SPECIALTY has an @JOB school level of 1..3. The port instead required
   *     `profession == COLONIZE_PROF_TEACHER` (18), which nothing in the game
   *     ever sets, so the teacher list was always empty.
   *   - Required turns come from the TEACHER'S profession level (1 -> 4,
   *     2 -> 6, 3 -> 8), not from the colony's best school building; the
   *     building tier is enforced when the teacher is assigned
   *     (@NEEDCOLLEGE / @NEEDUNIVERSITY, colonies_assign_workplace).
   *   - STUDENTS are colonists working anywhere in the colony, not only ones
   *     placed inside the school — @TRAINFAIL says so in as many words. DOS's
   *     list is profession 0x13 / 0x1c (free colonist), 0x19 (indentured),
   *     0x1a (criminal); Indian Converts (0x1b) are NOT students.
   *   - The graduate takes the TEACHER'S OWN profession — 0cae's third,
   *     register-passed argument, which Ghidra drops, is `aiStack_7e[t]`, the
   *     stored teacher specialty. The port handed out Farmer/Carpenter off
   *     `teacher->field_job`, which is always -1 for a colonist working in a
   *     building, so even a live teacher could not have taught Veteran
   *     Soldiers.
   *   - The student is picked at RANDOM (FUN_281f_04d4(0, n-1)), and at most
   *     3 teachers graduate per colony per tick (`local_6e < 3`).
   * The 0x1a -> 0x19 and 0x19 -> 0x1c ladders take priority over the specialty
   * and consume the teacher's turn, exactly as in DOS.
   *
   * FACULTY CAP (bugs.md #580, raw 57510-57535). The tick's cap is the bare
   * literal `local_6e < 3`: it is NOT the colony's owned school tier. The tick
   * selects teachers on `FUN_281f_0c0e(colonist) == 0x12` (the @JOB "teacher"
   * occupation byte) plus the level test alone -- it never calls
   * FUN_1000_8bec / thunk_FUN_1000_9808, so it asks neither which school row
   * the teacher sits in nor which schools the colony owns. The owned-tier cap
   * (University 3 / College 2 / Schoolhouse 1, colonies_school_owned_tier) and
   * the @NEEDCOLLEGE / @NEEDUNIVERSITY level requirement live ONLY in the
   * work-assign validator, overlays.c:60455-60484. Consequence, kept
   * deliberately: a save made in DOS (or edited) that seats more teachers than
   * the owned tier allows, or seats a level-2/3 specialist with only a
   * Schoolhouse, still teaches here -- up to 3 of them per tick. Do not "fix"
   * this by wiring colonies_school_owned_tier into the loop below; the seating
   * path is the only place DOS enforces the tier.
   * The seated-building test below is the port's spelling of occupation 0x12:
   * DOS stores one teacher occupation byte with no row in it, so "seated in
   * any school building" (tier > 0), never "seated in a building of the
   * required tier", is the faithful reading.
   */
  if (pool) {
    enum { EDU_MAX_TEACHERS = 3 };
    int teach_prof[EDU_MAX_TEACHERS];
    int students[COLONIZE_COLONY_POP_MAX];
    int n_teach = 0;
    int n_stud = 0;
    for (int ci = 0; ci < colony->colonist_count; ++ci) {
      ColonizeColonist* c = &colony->colonists[ci];
      if (!c->active) {
        continue;
      }
      /* DOS 0d1c/0a7e (raw 57505-57538): the +0x60 counter ticks for every
       * colonist; the writer FUN_15eb_0cbc clamps at 15 (raw 10231-10233),
       * so it saturates there instead of running on. */
      if (c->turns_in_job < 15) {
        c->turns_in_job++;
      }
      const int prof = c->profession;
      /* Student whitelist is exactly {0x1a,0x19,0x1c,0x13} (FUN_364b_0688 raw
       * 57510); no `prof < 0` arm in DOS. */
      if (prof == COLONIZE_PROF_FREE_COLONIST || prof == UNITS_JOB_COLONIST ||
          prof == COLONIZE_PROF_INDENTURED || prof == COLONIZE_PROF_CRIMINAL) {
        if (n_stud < (int)(sizeof(students) / sizeof(students[0]))) {
          students[n_stud++] = ci;
        }
        continue; /* level-4 professions never teach; lists are disjoint */
      }
      if (n_teach >= EDU_MAX_TEACHERS || c->building_type < 0 ||
          c->building_type >= pool->building_type_count ||
          colonies_school_building_tier(pool, c->building_type) <= 0) {
        continue;
      }
      const int level = colonies_job_school_tier(prof);
      if (level < 1 || level > 3) {
        continue;
      }
      const int need = (level == 1) ? 4 : (level == 2) ? 6 : 8;
      if ((int)c->turns_in_job < need) {
        continue;
      }
      teach_prof[n_teach++] = prof;
      /* DOS zeroes the counter the moment the teacher qualifies, whether or
       * not a student is available for him this turn. */
      c->turns_in_job = 0;
    }
    for (int t = 0; t < n_teach; ++t) {
      const bool tell =
        europe && colony->nation_id == human_nation && turn_report_ok_trained(col1);
      const char* cname = colony->name[0] ? colony->name : "colony";
      char body[AI_POPUP_BODY_LEN];
      char fallback[224];
      PopupMsgTokens tok;
      if (n_stud == 0) {
        /* DOS 0xde7 @TRAINFAIL, then out of the graduation loop entirely. */
        if (tell) {
          snprintf(europe->status, sizeof(europe->status), "No students to teach.");
          if (ai_popups) {
            fallback[0] = '\0';
            memset(&tok, 0, sizeof(tok));
            tok.string0 = cname;
            popup_msg_fill(messages, "TRAINFAIL", &tok, fallback, body, sizeof(body));
            ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
          }
        }
        break;
      }
      const int pick = rng ? dos_rng_range(rng, 0, n_stud - 1) : n_stud - 1;
      ColonizeColonist* student = &colony->colonists[students[pick]];
      const int prev_prof = student->profession;
      const char* skill_name = colonies_profession_name(teach_prof[t]);
      const char* chrome_sec;
      enum { GRAD_SPECIALTY = 0, GRAD_CRIMINAL, GRAD_INDENTURED } grad = GRAD_SPECIALTY;
      if (prev_prof == COLONIZE_PROF_CRIMINAL) {
        /* DOS 0x1a -> 0x19, popup 0xdf1. */
        student->profession = COLONIZE_PROF_INDENTURED;
        chrome_sec = "TRAINCRIMINAL";
        grad = GRAD_CRIMINAL;
        fallback[0] = '\0';
      } else if (prev_prof == COLONIZE_PROF_INDENTURED) {
        /* DOS 0x19 -> 0x1c, popup 0xdff. */
        student->profession = COLONIZE_PROF_FREE_COLONIST;
        chrome_sec = "TRAININDENTURED";
        grad = GRAD_INDENTURED;
        fallback[0] = '\0';
      } else {
        /* DOS 0cae(student, aiStack_7e[t]) + 0438(1, jobtable[prof].name),
         * popup 0xe0f. */
        student->profession = teach_prof[t];
        chrome_sec = "TRAINPROFESSION";
        fallback[0] = '\0';
      }
      /* bugs.md #565: DOS's graduation loop (raw 57540-57588) calls 0cae /
       * the ladder writers on the student and never 0a7e — the student keeps
       * his accumulated +0x60 counter, so a fresh graduate whose counter is
       * already >= his own school need can start teaching immediately. */
      for (int k = pick; k < n_stud - 1; ++k) {
        students[k] = students[k + 1];
      }
      n_stud--;
      if (tell) {
        if (grad == GRAD_CRIMINAL) {
          snprintf(europe->status, sizeof(europe->status), "Criminal educated.");
        } else if (grad == GRAD_INDENTURED) {
          snprintf(europe->status, sizeof(europe->status), "Indentured educated.");
        } else {
          snprintf(europe->status, sizeof(europe->status), "%s trained.", skill_name);
        }
        if (ai_popups) {
          memset(&tok, 0, sizeof(tok));
          tok.string0 = cname;
          tok.string1 = skill_name;
          popup_msg_fill(messages, chrome_sec, &tok, fallback, body, sizeof(body));
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
        }
      }
    }

    /*
     * Phase H — random field skill discover (DOS FUN_364b_0688 ~57590-57614,
     * see colony_eot_production.md Deep H). Raw bytes: skip profession 0x1b
     * (Convert); else roll dos_rng_range(0, N) where N is 99 (base — Free
     * Colonist / unset), 199 (0x19 Indentured Servant), or 299 (0x1a Petty
     * Criminal, the two ifs are sequential, not else-if, so Criminal takes
     * base+200); success on 0 → gain the field profession. Previously this
     * only fired for Free Colonist and used an ad hoc non-DOS PRNG instead
     * of the shared `rng` this function already threads through for the
     * starve-mercy roll a few lines up — fixed to use dos_rng_range(rng, ...)
     * like DOS's own FUN_281f_04d4 call, and to extend the class-scaled odds
     * to Indentured/Criminal (Convert stays excluded — not Free/Indentured/
     * Criminal). The DOS gate is `0 < occupation && occupation < 5` (raw
     * 57595) on `FUN_281f_0c0e` = the colonist's OCCUPATION (work slot),
     * ported as `field_job`: jobs 1..4 (@JOB 1 Sugar, 2 Tobacco, 3 Cotton,
     * 4 Fur Trapper) only, so a colonist farming (job 0) can NEVER become an
     * Expert Farmer on the job. (`FUN_281f_0c54` = specialty, the value the
     * class-scaled odds and the Convert/expert exclusions read; the
     * FUNCTION_CATALOG.md labels for 0c0e/0c54 are swapped.) The remaining
     * gate is the per-nation specialty census — see turn_prof_census.
     */
    for (int ci = 0; ci < colony->colonist_count; ++ci) {
      ColonizeColonist* c = &colony->colonists[ci];
      if (!c->active) {
        continue;
      }
      /* No RNG supplied (e.g. a caller that wants deterministic production
       * with no stochastic side effects) -> skip rather than let
       * dos_rng_range(NULL, 0, N)'s "always returns lo" convention read as
       * a guaranteed hit. */
      if (!rng) {
        continue;
      }
      int discover_denom = 99;
      if (c->profession == COLONIZE_PROF_INDENTURED) {
        discover_denom = 199;
      } else if (c->profession == COLONIZE_PROF_CRIMINAL) {
        discover_denom = 299;
      } else if (c->profession != COLONIZE_PROF_FREE_COLONIST &&
                 c->profession != UNITS_JOB_COLONIST /* @JOB 19 free alias */ &&
                 c->profession != UNITS_JOB_NONE /* DOS 0x1c, raw 9300-9303 */) {
        /* raw 57594: FUN_281f_0c9a(specialty) == 0 -> FUN_15eb_0002 (raw
         * 9298-9306) is 0 only for {0x1c,0x13,0x19,0x1a,0x1b}; Convert (0x1b)
         * is excluded one line earlier (bugs.md #772). */
        continue;
      }
      /* raw 57595: `0 < local_c2 && local_c2 < 5` — Farmer (job 0) never
       * learns on the job, and only the four cash-crop field jobs 1..4 do. */
      if (c->field_job < 1 || c->field_job > COLONIZE_JOB_FUR_TRAPPER) {
        continue;
      }
      /* raw 57596/57605: the specialty-census byte at -0x6bd0 + job must be
       * 0 — the nation may not already own an expert of that job anywhere —
       * and a success bumps it in place. See turn_prof_census. */
      const int census_n =
        (colony->nation_id >= 0 && colony->nation_id < (int)COLONIZE_COL1_NATION_COUNT)
          ? colony->nation_id
          : 0;
      if (turn_prof_census[census_n][c->field_job] != 0) {
        continue;
      }
      if (dos_rng_range(rng, 0, discover_denom) == 0) {
        turn_prof_census[census_n][c->field_job]++;
        /* raw 57606-57607: FUN_281f_0cae -> FUN_15eb_0e8c writes only the
         * profession byte; the +0x60 education nibble is untouched (bugs.md
         * #771, sibling of #565). DOS raises @TRAINPROFESSION only, no status
         * line, and a catalog miss is the empty string. */
        c->profession = c->field_job;
        if (europe && colony->nation_id == human_nation && turn_report_ok_trained(col1)) {
          if (ai_popups) {
            const char* cname = colony->name;
            const char* skill_name = colony_yield_job_name(c->field_job);
            if (!skill_name) {
              skill_name = "";
            }
            char body[AI_POPUP_BODY_LEN];
            PopupMsgTokens tok;
            memset(&tok, 0, sizeof(tok));
            tok.string0 = cname;
            tok.string1 = skill_name;
            popup_msg_fill(messages, "TRAINPROFESSION", &tok, "", body, sizeof(body));
            ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
          }
        }
      }
    }
  }

  /*
   * ---- Phases I / J — birth and starve-kill ----
   *
   * Position is load-bearing: DOS runs these last of the roster-changing
   * phases, after C/D and after F/G/H education (see the Phase C/D comment
   * above for the citations), so everything upstream sees the population
   * this colony started the turn with. Birth compares the food stock as it
   * stands after Phase B has applied every composed cargo — horses
   * included, which is why the horse-breeding food cost above is spent
   * before the 200-food test here rather than after it.
   *
   * FUN_364b_0688: starvation latch (+0x1c bit3) from food vs pop need.
   * Phase J kills when still short after this turn *and* food was already 0
   * at turn start (local_6c==0 / local_12e); pop==kills → @VANISH + abandon.
   * Easy-difficulty no-kill mercy ported below. Cite: ~57623–57694.
   */
  {
    const int need = pop * TURN_FOOD_PER_COLONIST;
    const int food_at_start = stock_before[COLONIZE_CARGO_FOOD];
    const int was_starving = colony->food_shortfall_latch != 0;
    int starved_this_tick = 0;
    /*
     * bugs.md (port_orange_starves.SAV): DOS's starve trigger is DS:0x8e5a =
     * max(0, consumption − stock-at-start − production) — the colony must
     * actually have gone NEGATIVE this turn. The old `stock_after < need`
     * latch killed a colony producing exactly what it eats at 0 stores
     * (commons 2 food vs pop 1 eating 2 — net zero, DOS-fine forever).
     */
    colony->food_shortfall_latch = (need - food_at_start - field_food > 0) ? 1u : 0u;

    /*
     * FUN_364b_0688 phase I — birth: food ≥ 200 → Free Colonist in colony;
     * subtract 200 food (docs/building_production.md; decomp ~57615–57622).
     */
    if (colony->stock[COLONIZE_CARGO_FOOD] >= 200 &&
        colony->colonist_count < COLONIZE_COLONY_POP_MAX) {
      colony->stock[COLONIZE_CARGO_FOOD] =
        clamp_int(colony->stock[COLONIZE_CARGO_FOOD] - 200, 0, 65535);
      if (delta) {
        delta->goods[COLONIZE_CARGO_FOOD] -= 200;
      }
      birth_food_debit = 200;
      bool born_on_tile = false;
      if (turn_birth_units) {
        /* bugs.md: the newborn stands on the colony tile awaiting orders. */
        const int ct = units_kind_type_index(turn_birth_units, UNITS_KIND_COLONIST);
        if (ct >= 0) {
          const int nid =
            units_spawn_allow_stack(turn_birth_units, ct, colony->x, colony->y);
          ColonizeUnit* nu = units_get(turn_birth_units, nid);
          if (nu) {
            units_set_nation(nu, colony->nation_id);
            nu->orders = UNITS_ORDER_NONE;
            born_on_tile = true;
          }
        }
      }
      if (!born_on_tile) {
        ColonizeColonist* newborn = &colony->colonists[colony->colonist_count];
        memset(newborn, 0, sizeof(*newborn));
        newborn->active = true;
        newborn->unit_type_index = 0;
        newborn->profession = UNITS_JOB_COLONIST; /* Free Colonists */
        newborn->building_type = -1;
        newborn->field_job = -1;
        colony->colonist_count++;
        colony->population = colony->colonist_count;
        /* DOS FUN_15eb admit body (raw ~11312): birth that admits straight
         * into the colony (no map-tile fallback) shares the same pop>2 +
         * peacetime + FF 9 Stockade grant as a walked-in Join. When
         * turn_birth_units is set the newborn lands on the map tile
         * instead (see units_spawn_allow_stack above) and never enters
         * colonist_count here, so the admit-path check in colony.c already
         * covers it there. */
        (void)founding_fathers_la_salle_check(pool, col1, colony->nation_id);
      }
      if (europe && colony->nation_id == human_nation) {
        /* DOS 0xe2f @NEWCOLONIST. Cite: colony_eot_production.md Phase I. */
        if (colony->name[0]) {
          snprintf(europe->status, sizeof(europe->status), "Birth in %s.", colony->name);
        } else {
          snprintf(europe->status, sizeof(europe->status), "Colony birth.");
        }
        if (ai_popups) {
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = colony->name[0] ? colony->name : "colony";
          popup_msg_fill(
            messages, "NEWCOLONIST", &tok, europe->status, body, sizeof(body)
          );
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
        }
      }
    }

    /*
     * Easy-difficulty no-kill mercy (FUN_364b_0688, decomp ~57641-57647):
     * on Discoverer/Explorer (difficulty < 2), the kill below never fires
     * before year 1520; from 1520 on it's a `dos_rng_range(0, 2-difficulty)`
     * roll, nonzero cancels (2/3 odds at Discoverer, 1/2 at Explorer). NULL
     * col1/rng safely fall through to the plain kill (old behavior).
     */
    int starve_mercy = 0;
    if (col1 && col1->head.difficulty < 2) {
      if (col1->head.year < 1520) {
        starve_mercy = 1;
      } else if (dos_rng_range(rng, 0, 2 - col1->head.difficulty) != 0) {
        starve_mercy = 1;
      }
    }

    /*
     * Phase J — starve-kill when still short and started the turn at 0 food.
     * Last colonist → @VANISH + colonies_abandon (DOS 0xe47 / thunk 0254).
     */
    if (colony->food_shortfall_latch != 0 && food_at_start == 0 &&
        colony->colonist_count > 0 && !starve_mercy) {
      const int colony_id = colony->id;
      char vanish_name[COLONIZE_COLONY_NAME_MAX];
      snprintf(
        vanish_name,
        sizeof(vanish_name),
        "%s",
        colony->name[0] ? colony->name : "colony"
      );
      const int kill_i = colony->colonist_count - 1;
      for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES_MAX; ++ti) {
        if ((int)colony->tiles[ti] == kill_i) {
          colony->tiles[ti] = (int8_t)-1;
        } else if ((int)colony->tiles[ti] > kill_i) {
          colony->tiles[ti] = (int8_t)((int)colony->tiles[ti] - 1);
        }
      }
      for (int i = kill_i; i < colony->colonist_count - 1; ++i) {
        colony->colonists[i] = colony->colonists[i + 1];
      }
      memset(
        &colony->colonists[colony->colonist_count - 1], 0, sizeof(colony->colonists[0])
      );
      colony->colonist_count--;
      colony->population = colony->colonist_count;
      starved_this_tick = 1;
      if (colony->colonist_count <= 0) {
        if (europe && colony->nation_id == human_nation) {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "Colony %s vanished.",
            vanish_name
          );
          if (ai_popups) {
            PopupMsgTokens tok;
            memset(&tok, 0, sizeof(tok));
            tok.string0 = vanish_name;
            popup_chrome_ok(ai_popups, messages, "VANISH", &tok, europe->status);
          }
        }
        (void)colonies_abandon(pool, colony_id);
        return;
      }
      if (europe && colony->nation_id == human_nation) {
        if (colony->name[0]) {
          snprintf(europe->status, sizeof(europe->status), "Starvation in %s.", colony->name);
        } else {
          snprintf(europe->status, sizeof(europe->status), "Colonist starved.");
        }
        if (ai_popups) {
          const char* sec = (col1 && col1->head.autumn) ? "STARVE2" : "STARVE1";
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = colony->name[0] ? colony->name : "colony";
          popup_msg_fill(messages, sec, &tok, europe->status, body, sizeof(body));
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
        }
      }
    }

    /*
     * First starvation latch (stock < need, not yet killing): @FOOD1 / @FOOD2.
     * Else DOS 0xe5e @FOODLOW when eating into stores:
     *   8e5a==0 (stock covers this turn) and 8e32!=0 (production < consumption)
     *   and post-eat stock < 8e32×4. Cite: FUN_15eb_0b52; ~57626–57636.
     * Surplus production (8e32==0) never warns — even if stock is modest.
     */
    if (!starved_this_tick && need > 0 && europe &&
        colony->nation_id == human_nation && turn_report_ok_food(col1)) {
      const int stock = colony->stock[COLONIZE_CARGO_FOOD];
      const int food_shortfall = consumed - field_food; /* DOS 8e32 when >0 */
      /*
       * bugs.md #5: DOS's literal FOOD1/FOOD2 latch fires on stock<need
       * alone, even at food_shortfall<=0 (production covers or beats
       * consumption) — a colony merely flatlining at 0 net-zero food would
       * re-trigger "depleted" every turn. Require actively losing food
       * (shortfall>0) to match FOODLOW's own "surplus never warns" rule
       * below and stop the false-positive nag.
       *
       * bugs.md #292: "depleted" additionally requires the stores to be
       * EXACTLY 0 — merely dipping below next turn's need reads as "low",
       * handled by the FOODLOW branch below.
       */
      if (stock == 0 && food_shortfall > 0 && !was_starving) {
        if (colony->name[0]) {
          snprintf(
            europe->status, sizeof(europe->status), "Food depleted in %s.", colony->name
          );
        } else {
          snprintf(europe->status, sizeof(europe->status), "Food stores depleted.");
        }
        if (ai_popups) {
          const char* sec = (col1 && col1->head.autumn) ? "FOOD2" : "FOOD1";
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = colony->name[0] ? colony->name : "colony";
          popup_msg_fill(messages, sec, &tok, europe->status, body, sizeof(body));
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
        }
      } else if (
        stock > 0 && food_shortfall > 0 && stock < food_shortfall * 4
      ) {
        if (colony->name[0]) {
          snprintf(europe->status, sizeof(europe->status), "Food low in %s.", colony->name);
        } else {
          snprintf(europe->status, sizeof(europe->status), "Food stores low.");
        }
        if (ai_popups) {
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = colony->name[0] ? colony->name : "colony";
          tok.number0 = stock;
          tok.has_number0 = true;
          popup_msg_fill(messages, "FOODLOW", &tok, europe->status, body, sizeof(body));
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
        }
      }
    }
  }

  /*
   * Carpenter hammers: convert lumber toward current project (or bank if
   * none). sol_b folds into each Carpenter worker individually, inside
   * colony_prod_colony_hammers (matches FUN_15eb_1d4c's Carpenter body —
   * see manufacturing_worker_calc_1d4c.md).
   *
   * Not seasonal (bugs.md #466, 2026-09-16): the Spring/Autumn calendar
   * is display only. The former Autumn-freeze gate came from real-DOS
   * original_saves/colony-prod-tests COLONY00→01_no-transports (Spring→
   * Autumn 1680) where hammers stayed put in all 32 colonies — but not one
   * of those colonies had a carpenter staffed (occupation 13), so the pair
   * proves nothing; the dutch2 Autumn→Spring pair banks normally, and
   * FUN_364b_0688 Phase L (raw 57730-57733) reads no season term.
   */
  int lumber_before_hammers;
  {
    /* Composed in Phase A (see the composition-boundary comment above);
     * DOS Phase L only reads the scratch word back with `0b50(0x10)`. */
    const int hammers_add = hammers_phase_a;
    lumber_before_hammers = colony->stock[COLONIZE_CARGO_LUMBER];
    int hammers = 0;
    if (hammers_add > 0) {
      /*
       * Hammers cost lumber 1:1, capped by lumber actually on hand (this
       * turn's field-yield lumber counts — TURN5→6: a lone Lumberjack+
       * Carpenter pair goes 0 lumber/0 hammers to lumber=3/hammers=+3 in
       * one same turn, so same-turn production *is* spendable). No project
       * queued still banks hammers (TURN5→6). The old code let hammers
       * through *for free* (no lumber debit at all) whenever the clipped
       * amount hit 0 instead of stopping production — that's what's fixed
       * here, not same-turn timing. (The colony-prod-tests Autumn-turn
       * counter-example that once looked like a same-turn-lumber rule had
       * no Carpenter staffed; FUN_15eb_0b52 raw 10122-10138 confirms this
       * turn's gross lumber counts toward the input ledger.)
      */
      hammers = hammers_add;
      if (hammers > colony->stock[COLONIZE_CARGO_LUMBER]) {
        hammers = colony->stock[COLONIZE_CARGO_LUMBER];
      }
      if (hammers > 0) {
        colony->stock[COLONIZE_CARGO_LUMBER] -= hammers;
        if (delta) {
          delta->goods[COLONIZE_CARGO_LUMBER] -= hammers;
        }
      }
      /* DOS adds the Phase A net word into the signed 16-bit +0x92 hammer
       * bank and clamps a negative wrapped result to zero (FUN_364b_0688 raw
       * 57730-57738). Keep the word arithmetic explicit; signed overflow in
       * C would be undefined. */
      const uint16_t hammer_sum_word =
        (uint16_t)((uint16_t)colony->hammers + (uint16_t)hammers);
      colony->hammers = (hammer_sum_word & 0x8000u) ? 0 : (int)hammer_sum_word;
      if (delta) {
        delta->hammers_added = hammers;
      }
      if (colony->building_in_production >= 0) {
        const int bip = colony->building_in_production;
        const ColonizeBuildingType* bt = colonies_building_type(pool, bip);
        const char* bname = bt ? bt->name : NULL;
        if (colonies_try_complete_building(pool, colony->id)) {
          if (delta) {
            delta->building_completed = true;
          }
          if (out) {
            out->buildings_completed++;
          }
          if (europe && colony->nation_id == human_nation) {
            if (bname && bname[0]) {
              snprintf(europe->status, sizeof(europe->status), "%s completed.", bname);
            } else {
              snprintf(europe->status, sizeof(europe->status), "Building completed.");
            }
            turn_emit_built_chrome(messages, ai_popups, colony, bname, europe->status);
          }
        }
      }
    } else if (
      colony->building_in_production >= 0 && colony->nation_id == human_nation && europe &&
      turn_report_ok_raw(col1)
    ) {
      /*
       * Phase K thin: construction queued but zero hammers this tick.
       * Gate !(5384&0x20). Cite: colony_eot_production.md Deep K.
       */
      snprintf(
        europe->status,
        sizeof(europe->status),
        "No hammers for construction."
      );
    }

    if (hammers_add <= 0) {
      /* DOS still adds zero and clamps a negative signed-word bank. */
      const uint16_t hammer_sum_word = (uint16_t)colony->hammers;
      colony->hammers = (hammer_sum_word & 0x8000u) ? 0 : (int)hammer_sum_word;
      if (delta) {
        delta->hammers_added = 0;
      }
    }
  }

  /* DOS Phase L resolves the threshold after adding this tick's hammers,
   * before checking whether the selected building is already owned
   * (FUN_364b_0688 raw 57737-57785). This arm runs even when the Phase A
   * output is zero and leaves the selected project in place. */
  if (colony->building_in_production >= 0) {
    const int bip = colony->building_in_production;
    const ColonizeBuildingType* bt = colonies_building_type(pool, bip);
    if (bt && !colonies_unit_build_info(bip, NULL, NULL, NULL) &&
        bip < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[bip] &&
        bt->hammers > 0 && colony->hammers >= bt->hammers) {
      colony->colony_flags |= COLONIZE_COLONY_FLAG_BUILD_COMPLETE;
      if (colony->nation_id == human_nation && europe && ai_popups) {
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = colony->name[0] ? colony->name : "colony";
        tok.string1 = bt->name[0] ? bt->name : "building";
        char afb[120];
        snprintf(
          afb, sizeof(afb), "%s already built.",
          bt->name[0] ? bt->name : "Building"
        );
        /* Popup only — DOS does not replace the Phase K status line here. */
        popup_msg_fill(messages, "ALREADYHAVE", &tok, afb, body, sizeof(body));
        ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
      }
    }
  }

  /*
   * Phase L construction tools check — outside the `hammers_add > 0` block
   * above because DOS runs it every EOT, hammers produced or not, and for
   * unit projects as well as buildings (bugs.md #537).
   */
  turn_emit_needtools_notice(pool, colony, col1, europe, human_nation, ai_popups, messages);

  /* Phase K demand crumbs (hammers/tools already above): raw / craft empty. */
  if (colony->nation_id == human_nation && europe &&
      strcmp(europe->status, status_at_entry) == 0 && turn_report_ok_raw(col1)) {
    /*
     * DOS gates each "ran out of X" msg on a demand scratch word (set by
     * FUN_15eb_0bd4/0b96 from the SAME turn's tier-scaled worker output,
     * see colony_eot_production.md Deep K), not on "does the building
     * exist" — a staffed-vs-unstaffed distinction the port used to miss
     * (a colony with e.g. an unstaffed starter Blacksmith's House and 0
     * ore would nag "Need ore." every turn even though nobody was trying
     * to make tools).
     *
     * 2026-09-23 fix (bugs.md #899): the other half of the probe is
     * `FUN_281f_0b50(out_cargo) == 0`, i.e. the FINISHED good's net, read
     * against the Phase A demand words. The port tested `stock[in] == 0`
     * instead, on a stock colony_craft_one_colony had already drained at
     * the Phase A position (:668) — so every colony that burned its whole
     * stored raw good nagged each turn while still shipping rum/cigars/
     * cloth/coats. The old note that "net == 0 reduces to stock[in] == 0"
     * assumed the pre-#897 proportional rescale; with DOS's factory-tier
     * back-conversion a partial input still yields a positive net and DOS
     * stays silent. (Superseded in detail by bugs.md #911 below: the probe is
     * the NET word, and `unmet[input] != 0` — not the demand mask — is the
     * other half.) 2026-08-24 fix: replaced building-name-substring gates
     * with the craft pass's own words (same recipe pass
     * colony_craft_one_colony already ran this tick, sol_bonus-consistent).
     *
     * 2026-09-10 fix (smell audit B1): "sol_bonus-consistent" was only true
     * until the Phase A composition boundary above (:865) hoisted the craft
     * pass. This mask sits at the Phase K/L position, i.e. AFTER Phase C/D
     * update the SoL accumulator and latch bits, after F/G/H education
     * rewrites professions and after I/J birth/starve-kill rewrite the
     * roster — so a freshly-read `colony_prod_sol_bonus()` here described a
     * DIFFERENT tier scaling than the `colony_craft_one_colony(pool, colony,
     * delta, sol_b_phase_a)` pass it is supposed to mirror, producing
     * spurious/missing "Need X." chrome on exactly the latch-crossing,
     * graduation and starvation turns. Reuse the Phase A snapshot instead:
     * one number, two consumers (colony_production.c:419-433).
     *
     * 2026-09-24 fix (#918): Phase L has already debited lumber here, so the
     * K probe derives unmet lumber from the Phase A hammer demand and the
     * pre-debit stock snapshot. DOS's FUN_281f_0b0c subtracts unmet[lumber]
     * from the Phase A hammer word; using post-debit stock treated partial or
     * exact supply as missing, while the former sol-free out_lumber_use probe
     * could demand lumber after Tory adjustment had reduced actual output to
     * zero.
     */

    /*
     * DOS-LITERAL FUN_364b_0688 raw 57696-57728 (bugs.md #911). Seven
     * INDEPENDENT `if`s, in GAME.TXT tag order, all inside one
     * `(DS:0x5384 & 0x20) == 0` chrome gate (the outer report filter above):
     *
     *   57697  unmet[lumber]  (0x8e64) && 0b50(0x10 hammers) == 0  -> 0xe66 @LUMBER
     *   57701  unmet[cotton]  (0x8e60) && 0b50(0x0b cloth)   == 0  -> 0xe6d @COTTON
     *   57705  unmet[tobacco] (0x8e5e) && 0b50(0x0a cigars)  == 0  -> 0xe74 @TOBACCO
     *   57709  unmet[sugar]   (0x8e5c) && 0b50(0x09 rum)     == 0  -> 0xe7c @CANESUGAR
     *   57713  unmet[furs]    (0x8e62) && 0b50(0x0c coats)   == 0  -> 0xe86 @FURS
     *   57717  unmet[ore]     (0x8e66) && 0b50(0x0f muskets) == 0b50(0x0e tools)
     *                                                            -> 0xe8b @ORE
     *   57725  unmet[tools]   (0x8e76) && 0b50(0x0f muskets) == 0  -> 0xe8f @TOOLS
     *
     * (unmet base DS:0x8e5a, stride 2, so 0x8e64 = [5] lumber ... 0x8e76 =
     * [14] tools; tag addresses verified against VICEROY.EXE DS, see
     * docs/popup_tag_ids.md's generation note, EXE offset 121248 + addr.)
     *
     * The port used to run these as ONE else-if chain in a different order,
     * so a colony could only ever emit one crumb per turn and @ORE outranked
     * every organic. DOS emits all that match; each is its own popup, and the
     * status line simply ends up carrying the last one (see below).
     *
     * Deleted with this fix: an invented "Need muskets." arm (no DS tag, no
     * GAME.TXT section) and a "Need food." arm spliced into the middle — DOS
     * has no food crumb here at all; its food chrome is the
     * VANISH/STARVE1/STARVE2 (0xe47/0xe4e/0xe56) and @FOODLOW (0xe5e) block
     * further up this function, which is ported and stays where DOS has it.
     *
     * #899 reconciliation: that fix read the second half of the probe as
     * `gross(out) == 0`. The DOS word is 0b50 = gross - demand - unmet[input],
     * i.e. the NET stock change, which only equals gross for goods nothing
     * else consumes. It differs for TOOLS, whose net is docked by the
     * gunsmith's consumption — exactly the cargo the @ORE and @TOOLS arms
     * read. craft_net[] (Phase B) is that net; craft_gross[] is not used here
     * any more.
     */
    struct KCrumb {
      const char* sec;
      bool fire;
    };
    /* FUN_281f_0b0c subtracts unmet[lumber] from Phase A hammers before K
     * tests the net. Compute that shortage from the lumber available before
     * Phase L debits it (FUN_364b_0688 raw 57697, 57730-57733). */
    const int lumber_stock = lumber_before_hammers;
    const int lumber_unmet = hammers_phase_a > lumber_stock
                               ? hammers_phase_a - lumber_stock
                               : 0;
    const int hammers_net =
      (hammers_phase_a < lumber_stock) ? hammers_phase_a : lumber_stock;
    /* unmet[input] != 0 for the recipe that makes `o` (see Phase B). */
#define K_UNMET(o) (craft_unmet_cap[(o)] > 0 && craft_unmet_gross[(o)] < craft_unmet_cap[(o)])
    const struct KCrumb k_crumbs[] = {
      {"LUMBER", lumber_unmet > 0 && hammers_net == 0},
      {"COTTON", K_UNMET(COLONIZE_CARGO_CLOTH) && craft_net[COLONIZE_CARGO_CLOTH] == 0},
      {"TOBACCO", K_UNMET(COLONIZE_CARGO_CIGARS) && craft_net[COLONIZE_CARGO_CIGARS] == 0},
      {"CANESUGAR", K_UNMET(COLONIZE_CARGO_RUM) && craft_net[COLONIZE_CARGO_RUM] == 0},
      {"FURS", K_UNMET(COLONIZE_CARGO_COATS) && craft_net[COLONIZE_CARGO_COATS] == 0},
      /* DOS-LITERAL raw 57717-57724: the ore arm compares net muskets against
       * net TOOLS, not against 0 — a copy-paste slip in the original (every
       * other arm tests `== 0`). Kept on purpose; do not "fix". */
      {"ORE",
       K_UNMET(COLONIZE_CARGO_TOOLS) &&
         craft_net[COLONIZE_CARGO_MUSKETS] == craft_net[COLONIZE_CARGO_TOOLS]},
      {"TOOLS", K_UNMET(COLONIZE_CARGO_MUSKETS) && craft_net[COLONIZE_CARGO_MUSKETS] == 0},
    };
#undef K_UNMET
    for (size_t ki = 0; ki < sizeof(k_crumbs) / sizeof(k_crumbs[0]); ++ki) {
      if (!k_crumbs[ki].fire) {
        continue;
      }
      /* bugs.md #912: no typed English fallback — the wording lives only in
       * GAME.TXT, miss = empty string, and the status line carries whatever
       * the catalog produced (DOS likewise shows the resolved message). */
      char body[AI_POPUP_BODY_LEN];
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = colony->name[0] ? colony->name : "";
      popup_msg_fill(messages, k_crumbs[ki].sec, &tok, "", body, sizeof(body));
      str_copy_trunc(europe->status, sizeof(europe->status), body);
      if (ai_popups) {
        ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
      }
    }
  }

  if (out) {
    out->colonies_produced++;
  }

  /*
   * Custom House auto-sell after production (FUN_364b_0688). Needs europe
   * bids; col1 optional (WoI tax skip + nation gold).
   */
  if (europe) {
    EuropeCustomHouseSale ch_sales[COLONIZE_CARGO_COUNT];
    int ch_sale_count = 0;
    const int ch_total = europe_custom_house_autosell_ex_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(pool), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL), .europe=(EuropeScreen*)(europe)}, colony, human_nation, ch_sales, COLONIZE_CARGO_COUNT, &ch_sale_count);
    for (int si = 0; si < ch_sale_count; ++si) {
      if (ch_sales[si].cargo >= 0 && ch_sales[si].cargo < COLONIZE_CARGO_COUNT) {
        ch_sold[ch_sales[si].cargo] += ch_sales[si].amount;
      }
    }
    /*
     * FUN_364b_0688 assembles ONE line PER CARGO into DS:0x2d54 and arms it
     * with FUN_1009_0092 — that is the map's top-strip STATUS LINE, not a
     * dialog (bugs.md #369). Each line replaces the strip's normal content for
     * its dwell and the next one follows; nothing has to be clicked away.
     *
     * Wording is DOS's own: colony name, LABELS @MISC 47 "sells", amount,
     * cargo name, @MISC 48 "for", gross, DS:0xd88 ".", then (peacetime only)
     * tax rate, @CMESSAGE 0x11 "% Tax:", tax paid, @CMESSAGE 0x12 ". Net:",
     * net. The 0088 calls between fields just trim the trailing space every
     * append leaves, so the punctuation closes up.
     */
    if (ch_total > 0 && colony->nation_id == human_nation && ai_popups) {
      for (int si = 0; si < ch_sale_count; ++si) {
        const EuropeCustomHouseSale* sale = &ch_sales[si];
        const char* cargo_name =
          (sale->cargo >= 0 && sale->cargo < europe->cargo_count)
            ? europe->cargo[sale->cargo].name
            : "goods";
        char line[AI_POPUP_BAR_MSG_LEN];
        int n = snprintf(
          line,
          sizeof(line),
          "%s %s %d %s %s %d.",
          colony->name[0] ? colony->name : "Colony",
          turn_label("MISC", 47, ""),
          sale->amount,
          cargo_name,
          turn_label("MISC", 48, "for"),
          sale->gross
        );
        if (n > 0 && n < (int)sizeof(line) && sale->tax_percent > 0) {
          snprintf(
            line + n,
            sizeof(line) - (size_t)n,
            " %d%s %d%s %d",
            sale->tax_percent,
            turn_label("CMESSAGE", 0x11, ""),
            sale->tax_paid,
            turn_label("CMESSAGE", 0x12, ""),
            sale->net
          );
        }
        ai_popup_enqueue_bar_message(ai_popups, line);
      }
    }
  }

  /*
   * FUN_364b_0688 Phase B (raw 57343-57346): cargo_produced_mask (+0x90)
   * bit c is set when the compose scratch (gross production, DS:-0x7238)
   * is non-zero AND the applied net is positive. The net is FUN_281f_0b50's
   * gross minus consumption (colonists' food, craft inputs, the Carpenter's
   * lumber) MINUS what the Custom House shipped in the same loop iteration
   * (raw 57273 `local_86 -= sold`), so a cargo the house sells down to 50
   * never counts as produced. The `net == 0 && surplus != 0` arm is dead:
   * surplus is clamp(0, stock - cap, net), which is 0 whenever net is 0.
   * Phase I's birth food and Phase L/O (hammers already debited above,
   * dump-sell, spoilage) are outside that loop; the net here is taken at
   * the Phase B point. The gross test only matters for food: the AI food
   * subsidy is added to the net, not the scratch, so a colony fed by the
   * subsidy alone does not "produce" food.
   */
  colony->cargo_produced_mask = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    int net = delta->goods[c] - ch_sold[c];
    if (c == COLONIZE_CARGO_FOOD) {
      net += birth_food_debit;
      if (field_food - ai_food_subsidy <= 0) {
        continue;
      }
    }
    if (net > 0) {
      colony->cargo_produced_mask |= (uint16_t)(1u << c);
    }
  }

  if (europe) {
    /* Phase O: AI dump-sell surplus for gold before spoilage clamp. */
    (void)europe_ai_colony_dump_sell_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(pool), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL), .europe=(EuropeScreen*)(europe)}, colony, human_nation);
  }
  /* Spoilage after Custom House / AI dump-sell (wiki Custom House before spoilage). */
  {
    int first_spoil = -1;
    int spoil_types = 0;
    const int spoiled =
      colonies_apply_warehouse_spoilage(pool, colony, stock_before, &first_spoil, &spoil_types);
    /* Phase P thin: human spoilage status; multi-type → goods phrasing. */
    if (spoiled > 0 && europe && colony->nation_id == human_nation) {
      const char* wh = colonies_building_row_name(
        (colony->warehouse_level > 1u) ? COLONY_BUILDING_WAREHOUSE_EXPANSION : COLONY_BUILDING_WAREHOUSE
      );
      const char* where = (colony->name[0]) ? colony->name : NULL;
      const char* cargo_name = NULL;
      if (first_spoil >= 0 && first_spoil < europe->cargo_count) {
        cargo_name = europe->cargo[first_spoil].name;
      }
      if (spoil_types > 1) {
        if (where) {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "%s in %s spoiled %d goods.",
            wh,
            where,
            spoiled
          );
        } else {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "%s spoiled %d goods.",
            wh,
            spoiled
          );
        }
      } else if (cargo_name && cargo_name[0]) {
        if (where) {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "%s in %s spoiled %d %s.",
            wh,
            where,
            spoiled,
            cargo_name
          );
        } else {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "%s spoiled %d %s.",
            wh,
            spoiled,
            cargo_name
          );
        }
      } else {
        snprintf(
          europe->status,
          sizeof(europe->status),
          "%s spoiled %d goods.",
          wh,
          spoiled
        );
      }
      if (ai_popups) {
        /* Phase P: SPOIL1/2 tip warehouse; SPOIL3/4 if expanded. Multi → 2/4. */
        const int expanded = colony->warehouse_level > 1u;
        const char* spoil_sec =
          (spoil_types > 1) ? (expanded ? "SPOIL4" : "SPOIL2")
                           : (expanded ? "SPOIL3" : "SPOIL1");
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = where ? where : "colony";
        tok.string1 = cargo_name && cargo_name[0] ? cargo_name : "goods";
        tok.number0 = spoiled;
        tok.has_number0 = true;
        popup_msg_fill(
          messages,
          spoil_sec,
          &tok,
          europe->status,
          body,
          sizeof(body)
        );
        ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
      }
    }

    /*
     * Phase P century tip: stock crosses a 100s boundary upward → @CARGOREADY*.
     * At exact warehouse cap → CARGOREADY1 (tip) / CARGOREADY2 (expanded).
     *
     * DOS runs this as its own `if` after the spoil block, once per crossing
     * cargo, for as long as the "report new cargos available" option is on —
     * it is not an alternative to spoilage, and it does not latch. The
     * once-per-campaign latch DS:0x5387 bit1 (head.tut3.nr6) belongs to the
     * separate @TUTORIAL6 "move a ship in and sell it" hint, emitted below.
     * Food (cargo 0) never triggers it. Cite: viceroy ~57893–57930.
     */
    if (europe && colony->nation_id == human_nation && turn_report_ok_new_cargo(col1)) {
      for (int c = 1; c < COLONIZE_CARGO_COUNT; ++c) {
        const int before = stock_before[c];
        const int after = colony->stock[c];
        if (after < 100 || before / 100 >= after / 100) {
          continue;
        }
        const int cap = colonies_warehouse_capacity(pool, colony, c);
        const char* cargo_name = NULL;
        if (c < europe->cargo_count && europe->cargo[c].name[0]) {
          cargo_name = europe->cargo[c].name;
        }
        snprintf(
          europe->status,
          sizeof(europe->status),
          "New cargo of %s ready at %s.",
          cargo_name ? cargo_name : "goods",
          colony->name[0] ? colony->name : "colony"
        );
        if (!ai_popups) {
          continue;
        }
        const char* sec = "CARGOREADY0";
        if (cap > 0 && after == cap) {
          sec = (colony->warehouse_level > 1u) ? "CARGOREADY2" : "CARGOREADY1";
        }
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = colony->name[0] ? colony->name : "colony";
        tok.string1 = cargo_name ? cargo_name : "cargo";
        tok.number0 = cap > 0 ? cap : after;
        tok.has_number0 = true;
        popup_msg_fill(messages, sec, &tok, europe->status, body, sizeof(body));
        ai_popup_enqueue_colony_event(ai_popups, colony->id, body);

        /* @TUTORIAL6, once per campaign (DS:0x5387 bit1). */
        if (col1 && !col1->head.tut3.nr6) {
          col1->head.tut3.nr6 = 1;
          PopupMsgTokens ttok;
          memset(&ttok, 0, sizeof(ttok));
          ttok.number0 = after;
          ttok.has_number0 = true;
          ttok.string0 = cargo_name ? cargo_name : "cargo";
          ttok.string1 = colony->name[0] ? colony->name : "colony";
          ttok.string2 = europe->nation_name[0] ? europe->nation_name : "Europe";
          popup_chrome_ok(ai_popups, messages, "TUTORIAL6", &ttok, europe->status);
        }
      }
    }
  }

  /*
   * FUN_364b_0688 ~57932 (Deep Q): per depletion unit, `rng(0, diff+1) != 0`
   * bumps Col1 +0x97; wrap at 50 → MAP_LAYER2_SUPPRESS (FUN_364b_033a
   * feature 4) + @DEPLETION. Discoverer thus depletes at 1/2 rate, Viceroy
   * at 5/6. Rolled here, after the cargo-ready chrome, to keep the DOS rng
   * order. No rng / col1 (unit tests) → deterministic bump.
   */
  if (depl_n > 0) {
    int diff = col1 ? (int)col1->head.difficulty : 4;
    if (diff < 0) {
      diff = 0;
    }
    if (diff > 4) {
      diff = 4;
    }
    int mine_depleted = 0;
    for (int u = 0; u < depl_n; ++u) {
      if (rng && dos_rng_range(rng, 0, diff + 1) == 0) {
        continue;
      }
      colony->depletion_counter = (uint8_t)(colony->depletion_counter + 1u);
      if (colony->depletion_counter > 0x31u) {
        colony->depletion_counter = (uint8_t)(colony->depletion_counter - 0x32u);
        if (map) {
          /* Production API takes const map; deplete mutates layer2. */
          map_occupancy_set_layer2(
            (ColonizeWorldMap*)(uintptr_t)map,
            depl_tx[u],
            depl_ty[u],
            MAP_LAYER2_SUPPRESS,
            true
          );
        }
        mine_depleted = 1;
      }
    }
    if (mine_depleted && europe && colony->nation_id == human_nation) {
      const char* cname = colony->name[0] ? colony->name : "colony";
      snprintf(europe->status, sizeof(europe->status), "Mine depleted near %s.", cname);
      if (ai_popups) {
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = cname;
        popup_msg_fill(messages, "DEPLETION", &tok, europe->status, body, sizeof(body));
        ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
      }
    }
  }
}

/*
 * debug.logs: one summary line per colony per turn — net stock change per
 * cargo, hammers, and what is under construction. Field names come from
 * @CARGO when a Europe screen is loaded (headless callers get the fallback).
 */
/* ===================== Colony production logging & per-nation scheduling (turn_log_cargo_name .. turn_run_colony_production_w) ===================== */
static const char* turn_log_cargo_name(const EuropeScreen* europe, int cargo) {
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return "?";
  }
  if (europe && cargo < europe->cargo_count && europe->cargo[cargo].name[0]) {
    return europe->cargo[cargo].name;
  }
  /* No Europe screen loaded: NAMES.TXT @CARGO via reports.c (audit CO-14).
   * The private 16-name fallback array this replaced ignored a renamed
   * catalog, which is the whole point of the accessor. */
  return reports_cargo_display_name(cargo);
}

static void turn_log_colony_production(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeCol1Save* col1,
  const EuropeScreen* europe,
  const int* stock_before,
  int hammers_before,
  int project_before
) {
  if (!diag_info_enabled() || !colony || !stock_before) {
    return;
  }
  char goods[512];
  goods[0] = '\0';
  size_t at = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    const int net = colony->stock[c] - stock_before[c];
    if (net == 0) {
      continue;
    }
    const int n = snprintf(
      goods + at, sizeof(goods) - at, "%s%s %+d (%d)",
      at ? ", " : "", turn_log_cargo_name(europe, c), net, colony->stock[c]
    );
    if (n <= 0 || (size_t)n >= sizeof(goods) - at) {
      break;
    }
    at += (size_t)n;
  }
  if (at == 0) {
    snprintf(goods, sizeof(goods), "%s", reports_misc_display_word(32, ""));
  }
  const char* building = "-";
  if (pool && colony->building_in_production >= 0 &&
      colony->building_in_production < pool->building_type_count) {
    building = pool->building_types[colony->building_in_production].name;
  } else {
    /* Artillery / Wagon Train sit above the @BUILDING range. */
    const char* unit_name = NULL;
    if (colonies_unit_build_info(colony->building_in_production, &unit_name, NULL, NULL) &&
        unit_name) {
      building = unit_name;
    }
  }
  diag_info(
    "PROD %s (id=%d nation=%d pop=%d SoL=%d%%): food %+d, hammers %+d (%d) building=%s%s",
    colony->name[0] ? colony->name : "colony",
    colony->id,
    colony->nation_id,
    colony->colonist_count > 0 ? colony->colonist_count : colony->population,
    colony_prod_sol_percent(col1, colony),
    colony->stock[COLONIZE_CARGO_FOOD] - stock_before[COLONIZE_CARGO_FOOD],
    colony->hammers - hammers_before,
    colony->hammers,
    building,
    project_before != colony->building_in_production ? " (project changed)" : ""
  );
  diag_info("PROD   %s: %s", colony->name[0] ? colony->name : "colony", goods);
}

/*
 * Nation scope for the three colony-EOT passes below.
 *
 * DOS runs FUN_364b_0688 inside each nation's own FUN_3844_00f2, and 00f2
 * runs immediately BEFORE that nation acts (year_loop.c: `nation_eot` then
 * `Move Pieces` for the human slot). So the human's colonies produce — and
 * their construction projects finish, with the @BUILT popup — at the START of
 * the human's turn, not when End Turn is pressed. This port's pipeline is
 * post-human, so it runs every AI nation's colonies in TURN_PROC_SETUP and
 * the human's in TURN_PROC_FINISH, right before control returns.
 *
 * Threaded as slice-scoped statics for the same reason as turn_labels: the
 * public entry points' signatures are pinned by the test call sites. Both
 * filters unset (the default, and what the processor restores after each
 * slice) means "every colony".
 *
 * The armed/disarmed state is its own flag, NOT `nation >= 0`: a headless
 * run (tools, tests, any driver with no human slot) passes human_nation < 0,
 * and reading the sentinel as "no filter" made SETUP skip nothing and FINISH
 * filter nothing — every colony produced in BOTH phases, i.e. twice per turn.
 * With the flags, human_nation < 0 means SETUP's skip matches no colony (all
 * nations are AI, which is exactly what DOS's per-nation FUN_3844_00f2 does
 * for them) and FINISH's only-filter matches none, so production happens
 * exactly once, in the AI-owned phase.
 */
int turn_prod_only_nation = -1;
bool turn_prod_only_set = false;
int turn_prod_skip_nation = -1;
bool turn_prod_skip_set = false;

bool turn_prod_nation_in_scope(int nation_id) {
  if (turn_prod_only_set && nation_id != turn_prod_only_nation) {
    return false;
  }
  if (turn_prod_skip_set && nation_id == turn_prod_skip_nation) {
    return false;
  }
  return true;
}

void turn_run_colony_production_w(
  const ColonizeWorld* w,
  int human_nation,
  ColonizeTurnResult* out,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  ColonizeColonyPool* pool = w->colonies;
  const ColonizeWorldMap* map = w->map;
  ColonizeCol1Save* col1 = w->col1;
  EuropeScreen* europe = w->europe;
  ColonizeDosRng* rng = w->rng;

  if (!pool) {
    return;
  }
  /* FUN_4962_0606 via FUN_3844_00f2 (raw 58374): the per-nation specialty
   * census is recomputed before the colony production loop. One production
   * pass = one turn here, so refresh all four Euro nations up front. */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT && n < 4; ++n) {
    turn_tally_professions(pool, w->units, n, turn_prof_census[n]);
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && turn_prod_nation_in_scope(pool->colonies[i].nation_id)) {
      /* bugs.md #256: DOS never carries an idle colonist — sweep any
       * job-less colonist (stale saves, non-UI admit paths) into work
       * before producing, so the head count always matches the workers. */
      colonies_auto_assign_idle(pool, i);
      /* Snapshot for the debug-log summary — production itself keeps the
       * NULL delta it has always had (a non-NULL one changes which branch
       * fills cargo_produced_mask). */
      int stock_before[COLONIZE_CARGO_COUNT];
      int hammers_before = pool->colonies[i].hammers;
      int project_before = pool->colonies[i].building_in_production;
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        stock_before[c] = pool->colonies[i].stock[c];
      }
      turn_produce_one_colony(
        pool,
        &pool->colonies[i],
        map,
        col1,
        europe,
        human_nation,
        out,
        NULL,
        ai_popups,
        messages,
        rng
      );
      turn_log_colony_production(
        pool, &pool->colonies[i], col1, europe, stock_before, hammers_before, project_before
      );
    }
  }
}


