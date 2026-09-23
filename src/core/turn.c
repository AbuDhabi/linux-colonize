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
 *  - Calendar/date, active-nation/fog & unit-move-refresh helpers
 *  - End-of-turn/autosave option gates & report readiness checks
 *  - Euro AI turn-order selection & phase finish status
 *  - Turn processor state machine: setup/euro/indian/finish/king steps & reset
 */

/* ===================== Calendar/date, active-nation/fog & unit-move-refresh helpers (turn_set_active_nation .. turn_select_next_unit_awaiting_orders) ===================== */
static void turn_set_active_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (ctx && ctx->active_turn_nation) {
    *ctx->active_turn_nation = nation_id;
  }
}

/*
 * FUN_3844_00f2 per-unit fog reveal (281f_07a0 → 13f1_02f8) for every unit of
 * the nation, human and AI alike. No colony reveal here: DOS colonies only
 * reveal once, ±5 at founding (colonies_reveal_founded).
 * Cite: nation_eot.c unit walk; game_loop human fog.
 */
static void turn_reveal_fog_for_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units || nation_id < 0 || nation_id > 3) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || !units_is_on_map(u)) {
      continue;
    }
    (void)units_reveal_sight_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, u);
  }
}

void turn_advance_calendar(uint16_t* year, uint16_t* autumn, uint32_t* turn_number) {
  if (!year || !autumn || !turn_number) {
    return;
  }
  if (*year == 0) {
    *year = TURN_START_YEAR;
  }
  (*turn_number)++;

  if (*year < TURN_BIANNUAL_YEAR) {
    /* One turn per year; season stays Spring. */
    *autumn = 0;
    (*year)++;
    return;
  }

  /* From 1600: Spring → Autumn within the year, then next Spring. */
  if (*autumn == 0) {
    *autumn = 1;
  } else {
    *autumn = 0;
    (*year)++;
  }
}

void turn_format_date(uint16_t year, uint16_t autumn, char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  if (year == 0) {
    year = TURN_START_YEAR;
  }
  /* NAMES.TXT @SEASONS row 0/1 via reports_season_name. */
  snprintf(out, out_size, "%s %u", reports_season_name(autumn != 0), (unsigned)year);
}

/*
 * DOS-LITERAL FUN_4d56_1b3a day top (viceroy_unpacked.c raw 6355-6357,
 * bugs.md #713):
 *
 *   for (local_14 = 0; local_14 < *(int *)0x539c; local_14 = local_14 + 1) {
 *     *(undefined1 *)(local_14 * 0x1c + 0x3149) = 0;
 *   }
 *
 * One pass over the WHOLE unit array, every nation at once, before nation 0
 * moves — not once per nation as its slice comes up. The port's per-nation
 * refresh cleared +0x3149 only for the nation whose slice was starting, so a
 * unit's last-turn spend still gated (e.g.) the landfall test for every
 * nation that had not been reached yet this year.
 */
void turn_clear_mp_spent_all_nations(ColonizeUnitPool* pool) {
  if (!pool) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active) {
      continue;
    }
    u->mp_spent_turn = 0;
    u->aboard_moves = -1;
  }
}

void turn_refresh_moves_for_nation_w(
  const ColonizeWorld* w,
  int nation_id,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  ColonizeUnitPool* pool = w->units;
  const ColonizeCol1Save* col1 = w->col1;
  ColonizeWorldMap* map = w->map;
  /* bugs.md #626: the pioneer work tick moved to the activation rotation, so
   * the refresh no longer emits text of its own. */
  (void)ai_popups;
  (void)messages;

  if (!pool) {
    return;
  }
  /* FF combat context for units_try_move (Washington / Drake / Revere). */
  units_set_ff_col1(col1);
  colonies_set_col1_context((ColonizeCol1Save*)col1);
  units_set_occupancy_map(map);
  colonies_set_occupancy_map(map);
  if (col1) {
    /* bugs.md #282: the first-`control == 0` scan reads a stale save that
     * carries TWO zeroed control slots as England. col1_save_human_nation is
     * the one place that resolution lives (head.human_player preferred when
     * it agrees with the control table); turn_processor_advance's own
     * units_set_combat_human_nation(ctx->human_nation) is authoritative and
     * this must not contradict it. */
    units_set_combat_human_nation(col1_save_human_nation(col1));
  }
  /* Native settlement fallout (FUN_5fef_31ea-shaped). Gold amount unknown. */
  units_set_native_fallout_context(
    col1 ? (ColonizeCol1Save*)col1 : NULL, map, -1
  );
  const bool magellan =
    col1 && founding_fathers_nation_has(col1, nation_id, FF_FERDINAND_MAGELLAN);
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    /* The spent byte is NOT cleared here: DOS does it once for every unit of
     * every nation at the day top (raw 6355-6357), which is
     * turn_clear_mp_spent_all_nations called from TURN_PROC_SETUP
     * (bugs.md #713, #423). */
    /* Fortify completes overnight → Fortified; stay asleep until woken. */
    if (u->orders == UNITS_ORDER_FORTIFY) {
      u->orders = UNITS_ORDER_FORTIFIED;
      u->moves = 0;
      continue;
    }
    /* Pioneer clear/plow/road: the refresh gives back the allotment and
     * NOTHING else. bugs.md #626: DOS does not run the work bodies here —
     * the lasting-order dispatcher FUN_2b5a_3ae6 (raw 46414) runs them from
     * the human map loop (raw 46873) when the unit comes up in the
     * activation rotation (game_select_next_unit_awaiting_orders), so the
     * @CLEARCUT / @USEDUPTOOLS text arrives one unit at a time instead of in
     * a burst at turn start, and an AI unit parked on order 8/9 is never
     * advanced at all. DOS clears moves_spent for EVERY unit at the day top
     * (viceroy 6357); only the work body's FUN_281f_0934 spends it
     * (76761 / 76889). */
    if (u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD) {
      u->moves = units_max_mp(pool, u->id);
      continue;
    }
    if (units_orders_skip_turn(u)) {
      /* bugs.md: count the nights parked — a unit fortified/sentried on a
       * PREVIOUS turn wakes with its full allotment (units_wake checks
       * park_nights > 0); one dug in this turn does not get its spent
       * moves back. park_nights is port-only: bumping col1_counter16 here
       * double-counted the DOS +0x16 clocks (treasure despawned in ~4
       * turns not 8, anchored ships repaired ~2x fast — smell #39). */
      if (u->park_nights < 255) {
        u->park_nights++;
      }
      u->moves = 0;
      continue;
    }
    const ColonizeUnitType* type = units_type(pool, u->type_index);
    if (type) {
      /*
       * Natives: COL1 moves = DOS spent thirds; day loop clears spent to 0
       * (decomp ~6357). Brave max allotment is 3 thirds.
       * Europeans: remaining MP = @UNIT movement (+ Magellan sea +1).
       */
      if (nation_id >= 4) {
        u->moves = 0;
      } else {
        u->moves = units_type_max_mp(type);
        if (magellan && units_is_sea(pool, u->id)) {
          u->moves += UNITS_MP_PER_TILE;
        }
      }
    }
  }
}

bool turn_select_next_unit(ColonizeUnitPool* pool, int human_nation) {
  if (!pool) {
    return false;
  }
  const int start = pool->selected_id;
  int best_after = -1;
  int best_any = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != human_nation || u->moves <= 0) {
      continue;
    }
    if (!units_is_on_map(u)) {
      continue;
    }
    if (best_any < 0 || u->id < best_any) {
      best_any = u->id;
    }
    if (u->id > start && (best_after < 0 || u->id < best_after)) {
      best_after = u->id;
    }
  }
  const int pick = best_after >= 0 ? best_after : best_any;
  if (pick < 0) {
    return false;
  }
  pool->selected_id = pick;
  return true;
}

bool turn_select_next_unit_awaiting_orders(ColonizeUnitPool* pool, int human_nation) {
  if (!pool) {
    return false;
  }
  bool found = turn_select_next_unit(pool, human_nation);
  for (int guard = 0; found && guard < COLONIZE_UNITS_MAX; ++guard) {
    const ColonizeUnit* next = units_get_const(pool, pool->selected_id);
    if (!next || !units_orders_skip_turn(next)) {
      break;
    }
    found = turn_select_next_unit(pool, human_nation);
  }
  if (found) {
    const ColonizeUnit* next = units_get_const(pool, pool->selected_id);
    if (!next || units_orders_skip_turn(next)) {
      return false; /* guard ran out on a run of standing-order units */
    }
  }
  return found;
}

/* ===================== End-of-turn/autosave option gates & report readiness checks (turn_option_end_of_turn .. turn_report_ok_inefficient) ===================== */
bool turn_option_end_of_turn(const ColonizeCol1Save* col1, bool col1_ok) {
  return col1_ok && col1 && col1->head.game_options.end_of_turn != 0;
}

static bool turn_option_autosave(const ColonizeCol1Save* col1, bool col1_ok) {
  return col1_ok && col1 && col1->head.game_options.autosave != 0;
}

/*
 * DOS 0x5384 colony-report bits: msgs fire when the matching bit is clear
 * (defaults 0 in COL saves). Cite: viceroy ~57558/57696; colony_eot_production.md.
 */
int turn_report_ok_trained(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_when_colonists_trained;
}
int turn_report_ok_raw(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_raw_materials_shortages;
}
int turn_report_ok_food(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_food_shortages;
}
int turn_report_ok_new_cargo(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_new_cargos_available;
}
/* DOS 0x5385 bit1 clear → show rebel majority / unanimous / tory chrome. */
int turn_report_ok_rebel_maj(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_rebel_majorities;
}
/* DOS 0x5385 bit0 clear → show @SONSUP / @SONSDOWN decade chrome. */
/*
 * bugs.md: a birth puts the new Free Colonist ON the colony tile as a map
 * unit awaiting orders — not into the colony as a worker. The production
 * pass has no unit-pool parameter (deep call chain), so the game loop hands
 * it in here before ticking; NULL (tests, headless callers) falls back to
 * the old join-the-colony behaviour.
 */
ColonizeUnitPool* turn_birth_units = NULL;
void turn_set_birth_units_pool(ColonizeUnitPool* units) {
  turn_birth_units = units;
}

int turn_report_ok_sons(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_sons_of_liberty_membership;
}
/* DOS 0x5384 bit3 clear → show @INEFFICIENT / @EFFICIENT. */
int turn_report_ok_inefficient(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_inefficient_government;
}

/*
 * FUN_364b_0688 Phase D Tory pressure: inefficient-gov chrome (0xdd1 / 0xddd).
 * Latch is Col1 +0x1c bit3 (COLONIZE_COLONY_FLAG_INEFFICIENT_GOV), exactly
 * where DOS keeps it, so a crossing announced before a save is not announced
 * again after the reload (bugs.md). DOS sets and clears the bit whether or
 * not the report option lets the message through, so the latch update stays
 * outside turn_report_ok_inefficient.
 *
 * Coverage (viceroy_unpacked.c 57470-57485): 0688 runs for every colony of the
 * ticked nation and the bit3 OR/AND-clear pair sits in the open function body.
 * Only the two dialogs are human-gated, and by DS:0xa897 — FUN_15eb_002c
 * (viceroy 9325-9331) sets that byte from "colony owner == 0x5396 AND that
 * slot's control == 0", i.e. exactly `nation_id == human_nation`. So the latch
 * is maintained for AI colonies too; only the chrome is skipped.
 *
 * Threshold: raw 57470 `local_8a = -(*(byte *)0x53a6 - 10)` = 10 − difficulty,
 * unconditional. The port used to compute that only when the colony owner's
 * control was 0, a re-test that could never fail behind the human-only early
 * return above; with AI colonies now in scope the test would have been wrong,
 * so it is gone.
 */
/* ===================== Euro AI turn-order selection & phase finish status (turn_euro_ai_should_run .. turn_finish_status) ===================== */
static bool turn_euro_ai_should_run(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4 || nation_id == ctx->human_nation) {
    return false;
  }
  if (turn_euro_nation_is_ref(ctx, nation_id)) {
    return true; /* REF slot: full euro turn + pre-euro king beat (DOS crown control = 1) */
  }
  uint8_t control = 1;
  if (ctx->col1_ok && ctx->col1) {
    control = ctx->col1->player[nation_id].control;
  }
  return control != 2;
}

static int turn_next_euro_ai(const ColonizeTurnContext* ctx, int start) {
  for (int n = start; n < 4; ++n) {
    if (turn_euro_ai_should_run(ctx, n)) {
      return n;
    }
  }
  return -1;
}

/*
 * DOS FUN_130d_0290 runs, per year tick: mid-pass Indian turns (4d56_1b3a →
 * 1816 per slot), then the Euro 0..3 loop with the human's Move Pieces inside
 * it. Linux's pipeline starts after the human ends their turn, so relative
 * to that point the DOS order is: Euro slots above the human → Indians → Euro
 * slots below the human. Seed-100 golden (human = slot 0): the Dutch
 * TURN2→3 landing meets an Aztec Brave that only moves away in the Indian
 * pass that follows. No human slot (headless sims) keeps Indians first.
 */
static int turn_human_slot(const ColonizeTurnContext* ctx) {
  return (ctx && ctx->human_nation >= 0 && ctx->human_nation < 4) ? ctx->human_nation : -1;
}

/* Next Euro AI slot strictly above the human (pre-Indian pass), or -1. */
static int turn_next_euro_ai_above_human(const ColonizeTurnContext* ctx, int start) {
  const int h = turn_human_slot(ctx);
  if (h < 0) {
    return -1;
  }
  return turn_next_euro_ai(ctx, start > h + 1 ? start : h + 1);
}

/* Next Euro AI slot below the human (post-Indian pass), or -1. */
static int turn_next_euro_ai_below_human(const ColonizeTurnContext* ctx, int start) {
  const int h = turn_human_slot(ctx);
  const int next = turn_next_euro_ai(ctx, start);
  if (next < 0 || (h >= 0 && next > h)) {
    return -1;
  }
  return next;
}

static void turn_finish_status(ColonizeTurnContext* ctx, const ColonizeTurnResult* result) {
  if (!ctx || !ctx->status || ctx->status_size == 0 || !ctx->game_year || !ctx->game_autumn ||
      !ctx->turn_number) {
    return;
  }
  if (result && result->year_end_defeat) {
    snprintf(ctx->status, ctx->status_size, "Defeat: no colonies remain.");
    return;
  }
  if (result && result->year_end_victory) {
    snprintf(ctx->status, ctx->status_size, "Victory: independence won.");
    return;
  }
  char date[32];
  turn_format_date(*ctx->game_year, *ctx->game_autumn, date, sizeof(date));
  snprintf(
    ctx->status,
    ctx->status_size,
    "End of Turn — %s (turn %u)",
    date,
    (unsigned)*ctx->turn_number
  );
}

/* ===================== Turn processor state machine: setup/euro/indian/finish/king steps & reset (turn_processor_start .. turn_reset) ===================== */
void turn_processor_start(ColonizeTurnProcessor* proc) {
  if (!proc) {
    return;
  }
  memset(proc, 0, sizeof(*proc));
  proc->step = TURN_PROC_SETUP;
}

bool turn_processor_active(const ColonizeTurnProcessor* proc) {
  return proc && proc->step != TURN_PROC_IDLE;
}

bool turn_processor_show_indicator(const ColonizeTurnProcessor* proc) {
  return proc && proc->show_indicator;
}

COLONIZE_INTERNAL void turn_step_setup(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx) {
      diag_info(
        "TURN setup: turn=%u year=%u autumn=%u human=%d",
        (unsigned)*ctx->turn_number, (unsigned)*ctx->game_year,
        (unsigned)*ctx->game_autumn, ctx->human_nation
      );
      /* DOS FUN_3844_00f2 opens every nation's EOT with turn_owner_chrome
       * (281f_0590 → 1984_00aa); the human's own colony EOT now runs in
       * TURN_PROC_FINISH, which re-arms the same chrome there. SETUP itself
       * owns no nation's EOT any more (calendar + AI-only production +
       * nation ticks), so the box stays dark for this slice — turn.h's
       * TURN_PROC_SETUP contract and turn_between_players.md ("only while
       * EURO/INDIAN steps run"). The `true` here was the leftover of the
       * pre-FINISH layout, when human production still ran in SETUP. */
      turn_set_active_nation(ctx, ctx->human_nation);
      proc->show_indicator = false;
      proc->year_before = *ctx->game_year;
      {
        const uint16_t autumn_before = *ctx->game_autumn;
        turn_advance_calendar(ctx->game_year, ctx->game_autumn, ctx->turn_number);
        /*
         * @TIMECHANGE (raw 6444-6449, FUN_130d_0290): `LEA BX,[0x141]` right
         * before `CALLF FUN_281f_03fe` — Ghidra drops the LEA-loaded tag arg,
         * but the asm shows it (0x141 = DS string "TIMECHANGE", GAME.TXT
         * @TIMECHANGE Calendar-help). Fires exactly once, the instant the
         * calendar crosses from one-turn-per-year to the Spring/Autumn
         * biannual split: year==1600 AND season(0x538c)==0, i.e. the single
         * turn_advance_calendar() call where year was already 1600 and
         * autumn flips 0→1. No tutorial-hints gate on this one (that gate
         * covers a different call a few lines up in the same function).
         */
        if (proc->year_before == TURN_BIANNUAL_YEAR && autumn_before == 0 &&
            *ctx->game_autumn == 1) {
          popup_chrome_ok(
            ctx->ai_popups, ctx->messages, "TIMECHANGE", NULL,
            ""
          );
        }
      }
      /* DOS-LITERAL FUN_4d56_1b3a raw 6355-6357 (bugs.md #713): the spent
       * byte of every unit of every nation is zeroed once here, right after
       * the calendar advance and before nation 0 moves. */
      turn_clear_mp_spent_all_nations(ctx->units);
      proc->result.advanced = true;
      if (ctx->col1_ok && ctx->col1) {
        ctx->col1->head.turn =
          (uint16_t)(*ctx->turn_number > 65535u ? 65535u : *ctx->turn_number);
        ctx->col1->head.year = *ctx->game_year;
        ctx->col1->head.autumn = *ctx->game_autumn;
      }
      /* AI nations only — the human's colonies run their EOT at the top of
       * TURN_PROC_FINISH instead, which is where DOS puts it (see the
       * turn_prod_only_nation comment). */
      turn_prod_skip_nation = ctx->human_nation;
      turn_prod_skip_set = true;
      turn_run_colony_eot(ctx, &proc->result);
      turn_prod_skip_nation = -1;
      turn_prod_skip_set = false;
      /* FUN_364b_03f6 coastal Fort/Fortress fire after production. */
      (void)turn_run_coastal_fort_fire(ctx);
      turn_run_nation_ticks(ctx, &proc->result);
      /*
       * DOS FUN_364b_0688 Phase A: bells + Congress (291f_09f8 → 4345_0a22)
       * run in the colony-EOT PROLOGUE, so an FF nomination/election dialog
       * presents BEFORE the colony production messages. The port computes
       * production first (RNG draw order pinned by goldens); reorder the
       * presentation queue instead. Now that the human's own colony EOT sits
       * in TURN_PROC_FINISH this is normally a no-op (only human colonies
       * enqueue colony events, and none exist yet), kept for AI-owned queues
       * and for the synchronous turn_end path. Cite:
       * turn/colony_eot_production.md Phase A; turn/nation_ticks_bells_ff.md.
       */
      if (ctx->ai_popups) {
        ai_popup_promote_tag_before(
          ctx->ai_popups, AI_POPUP_TAG_FF_CONGRESS, AI_POPUP_TAG_COLONY_EVENT
        );
      }
      /* Mid-pass Euro rank (FUN_5bfb_00f8) — DOS before nation loop; Linux SETUP. */
      ctx->euro_power_rank_ok =
        turn_rank_euro_nations(
          ctx->col1_ok ? ctx->col1 : NULL, ctx->colonies, ctx->euro_power_rank
        ) == 0;
      /* Live census peel: colony + unit/combat tallies (FUN_4962_0018). */
      if (ctx->col1_ok && ctx->col1) {
        col1_stuff_census_refresh_colony_counts_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, &ctx->col1->stuff);
      }
      /*
       * DOS order relative to the human's end-of-turn (see
       * turn_human_slot): Euro slots above the human first, then the
       * 4d56_1b3a mid-pass Indian turns, then the slots below the human.
       * See turn/mid_pass_indian_rank.md, turn/year_loop.c.
       */
      {
        const int next = turn_next_euro_ai_above_human(ctx, 0);
        if (next >= 0) {
          proc->nation_cursor = next;
          proc->step = TURN_PROC_EURO;
        } else {
          proc->nation_cursor = 4;
          proc->step = TURN_PROC_INDIAN;
        }
      }
}

COLONIZE_INTERNAL void turn_step_euro(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx) {
      const int n = proc->nation_cursor;
      diag_info("TURN european nation %d%s", n, n == ctx->human_nation ? " (human)" : "");
      proc->show_indicator = true;
      turn_set_active_nation(ctx, n);
      if (ctx->units) {
        units_occupancy_rebuild(ctx->units); /* presence bits exact before this nation reads them */
      }
      /* 00f2 unit walk: 281f_07a0 reveal precedes the per-unit ticks. */
      if (n != ctx->human_nation) {
        turn_reveal_fog_for_nation(ctx, n);
      }
      if (ctx->units) {
        turn_refresh_moves_for_nation_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, n, ctx->ai_popups, ctx->messages);
        if (n >= 0 && n < 4) {
          /* FUN_3844_0004 (bugs.md #725): AI owner — the lone Convert just
           * vanishes, no @DEADCONVERTS popup. */
          (void)units_tick_convert_outside_colony(ctx->units, ctx->map, n);
          /* No want_europe_open sink: turn_euro_ai_should_run rejects
           * n == ctx->human_nation, so this slice never runs for the human and
           * the "auto-open Europe" request can never be for them. */
          (void)units_tick_ship_build_ready(
            ctx->units,
            ctx->colonies,
            n,
            ctx->human_nation,
            ctx->status,
            ctx->status_size,
            NULL
          );
          (void)units_tick_drydock_repair(
            ctx->units,
            ctx->colonies,
            n,
            ctx->human_nation,
            ctx->status,
            ctx->status_size,
            ctx->ai_popups,
            ctx->messages
          );
          turn_route_damaged_ships(ctx, n);
        }
      }
      if (turn_euro_nation_is_ref(ctx, n)) {
        /* DOS per-slot order (raw 6394/6407): king beat (00f2→2424→2022,
         * wave spawn + bookkeeping) first, then the full euro turn moves
         * the REF units through the ordinary 5b66/20e6 arms. D1 closed
         * 2026-09-07g. */
        ai_king_ref_pre_euro_beat(ctx);
      }
      ai_euro_nation_turn(ctx, n);
      {
        const int h = turn_human_slot(ctx);
        if (h >= 0 && n > h) {
          /* Pre-Indian pass (slots above the human). */
          const int next = turn_next_euro_ai_above_human(ctx, n + 1);
          if (next >= 0) {
            proc->nation_cursor = next;
          } else {
            proc->nation_cursor = 4;
            proc->step = TURN_PROC_INDIAN;
          }
        } else {
          /* Post-Indian pass (slots below the human, or every slot without one). */
          const int next = turn_next_euro_ai_below_human(ctx, n + 1);
          if (next >= 0) {
            proc->nation_cursor = next;
          } else {
            proc->step = TURN_PROC_FINISH;
          }
        }
      }
}

COLONIZE_INTERNAL void turn_step_indian(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx) {
      const int n = proc->nation_cursor;
      diag_info("TURN native nation %d", n);
      proc->show_indicator = true;
      if (n == 4) {
        /* FUN_4d56_1b3a phase 1 — once, before the eight 1816 calls. */
        ai_indian_midpass_clear_tables(ctx);
      }
      turn_set_active_nation(ctx, n);
      if (ctx->units) {
        units_occupancy_rebuild(ctx->units);
      }
      if (ctx->units) {
        turn_refresh_moves_for_nation_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, n, ctx->ai_popups, ctx->messages);
      }
      /*
       * FUN_4d56_1b3a phase 2 (raw viceroy_unpacked.c:81709-81715):
       *
       *   local_12 = 0;
       *   do {
       *     if ((*(byte *)(local_12 * 0x4e + 0x5ad9) & 0x80) == 0) {
       *       FUN_41f2_0266(0x4d56,local_12);   // = the 1816 slice
       *     }
       *     local_12 = local_12 + 1;
       *   } while (local_12 < 8);
       *
       * DS:0x5ad9 + 0x4e*slot is `ColonizeCol1Indian` +3, and bit 0x80 there
       * is `extinct` (col1_save.h:787). An extinct tribe is skipped: only the
       * AI slice is gated — phases 1 and 3 bracket the loop unconditionally,
       * and DOS refreshes native movement outside 1b3a, so the surviving
       * braves of a wiped-out tribe still get their MP. Skipping matters for
       * RNG-stream fidelity: a dead tribe that still "acts" burns draws.
       */
      const int extinct =
        ctx->col1_ok && ctx->col1 && n >= 4 && n <= 11 &&
        ctx->col1->indian[n - 4].extinct != 0;
      if (!extinct) {
        ai_indian_nation_turn(ctx, n);
      }
      if (n < 11) {
        proc->nation_cursor = n + 1;
      } else {
        /* FUN_4d56_1b3a phase 3 — once, after the eight 1816 calls. */
        ai_indian_midpass_claim_worked_tiles(ctx);
        const int next = turn_next_euro_ai_below_human(ctx, 0);
        if (next >= 0) {
          proc->nation_cursor = next;
          proc->step = TURN_PROC_EURO;
        } else {
          proc->step = TURN_PROC_FINISH;
        }
      }
}

/* Body of TURN_PROC_FINISH up to its early `return true;`. */
COLONIZE_INTERNAL void turn_step_finish(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx) {
      /*
       * Indicator ON for this slice. FUN_3844_00f2's very first act (raw
       * 58323) is FUN_281f_0590(nation_color[DS:0x5394]) → FUN_1984_00aa, and
       * 00f2 runs for every slot the year loop walks, the human's included
       * (viceroy 6390 FUN_281f_0644, gated only on control != 2). So the
       * turn-owner box carries the human's own color through their production
       * pass. The old `false` here was overwritten two lines down and cleared
       * again before the slice returned, so it never reached the renderer —
       * turn_processor_show_indicator is only read between advance() calls.
       */
      /*
       * The human nation's own colony EOT — DOS FUN_3844_00f2 runs it right
       * before that nation's Move Pieces, so a construction project finishes
       * (and announces itself) at the start of the player's turn, not the
       * instant End Turn is pressed. Ahead of the king / census / market work
       * below, matching 00f2's own production-then-census-then-king order.
       */
      turn_set_active_nation(ctx, ctx->human_nation);
      proc->show_indicator = true;
      turn_prod_only_nation = ctx->human_nation;
      turn_prod_only_set = true;
      turn_run_colony_eot(ctx, &proc->result);
      /*
       * FUN_3844_00f2 census AFTER the colony-EOT loop (viceroy_unpacked.c
       * :58390 → FUN_291f_0a74 → FUN_4962_0018): refresh the human colonies'
       * blockade pair (+0x1b bits 0x01/0x02) so next turn's Custom House
       * autosell gate (europe.c, ai_flags & 3) reads live state, not the
       * save-import value. AI nations get the same probe in their own pass.
       */
      ai_euro_census_ship_pressure_refresh(ctx, ctx->human_nation);
      /* bugs.md #434: the human's FF election lands here — start of the
       * player's turn, with the production chrome — not in SETUP. */
      founding_fathers_tick_human_elect(ctx);
      turn_prod_only_nation = -1;
      turn_prod_only_set = false;
      /* bugs.md #394/404/407: yield here so the production popups queued
       * above are answered (and an elected colony zoom taken) before the
       * king's REF beats run in TURN_PROC_KING — see turn.h. */
      proc->step = TURN_PROC_KING;
}

/* Body of TURN_PROC_KING up to its terminal `return false;`. */
COLONIZE_INTERNAL void turn_step_king(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx) {
      proc->show_indicator = false;
      turn_set_active_nation(ctx, ctx->human_nation);
      ai_king_nation_turn(ctx);
      turn_run_year_end_chrome(ctx, &proc->result);
      /* FUN_38fd_0058 EOT market attrition / rise-fall for Europe screen. */
      if (ctx->europe) {
        europe_tick_market_prices_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL), .europe=(EuropeScreen*)(ctx->europe)}, ctx->human_nation, ctx->turn_number ? *ctx->turn_number : 0u);
        /* FUN_38fd_0058 phase 4: 0xfa8 @PRICEUP / 0xfb0 @PRICEDOWN OK dialog
         * (FUN_281f_0652(tag, 2)) for the human nation only. DOS calls this
         * inline once per cargo that crosses threshold — a turn where two
         * different goods both move gets two separate dialogs, not one;
         * queue every entry (previously only the last event survived). */
        if (ctx->ai_popups) {
          for (int i = 0; i < ctx->europe->price_event_count; ++i) {
            const int c = ctx->europe->price_event_cargo[i];
            PopupMsgTokens tok;
            memset(&tok, 0, sizeof(tok));
            tok.string0 = ctx->europe->cargo[c].name;
            tok.string1 = ctx->europe->port_city;
            tok.number0 = ctx->europe->cargo[c].bid;
            tok.has_number0 = true;
            popup_chrome_ok(
              ctx->ai_popups, ctx->messages,
              ctx->europe->price_event_dir[i] > 0 ? "PRICEUP" : "PRICEDOWN", &tok,
              ctx->europe->status
            );
          }
        }
        /* Shown — clear, or the next player sell/buy replays these stale
         * events via game_europe_drain_price_events and an unrelated cargo
         * (e.g. Coats) looks like it moved in response to that sale. */
        ctx->europe->price_event_count = 0;
      }
      /*
       * FUN_281f_0668 → FUN_43f7_2244 (viceroy_unpacked.c:6418): the
       * peacetime @MERCENARIES offer sits in the human arm of the year loop
       * after FUN_281f_0644 (= 3844_00f2, everything above) and right before
       * FUN_281f_062c (Move Pieces).
       */
      ai_king_peacetime_merc_offer(ctx);
      turn_set_active_nation(ctx, ctx->human_nation);
      turn_reveal_fog_for_nation(ctx, ctx->human_nation);
      turn_refresh_moves_for_nation_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, ctx->human_nation, ctx->ai_popups, ctx->messages);
      if (ctx->human_nation >= 0 && ctx->human_nation < 4) {
        /* FUN_3844_0004 (bugs.md #725): human owner — FUN_281f_0652(0xee2, 4)
         * = @DEADCONVERTS, once per Convert lost. */
        {
          const int converts_lost =
            units_tick_convert_outside_colony(ctx->units, ctx->map, ctx->human_nation);
          for (int k = 0; k < converts_lost && ctx->ai_popups; ++k) {
            PopupMsgTokens tok;
            memset(&tok, 0, sizeof(tok));
            char body[AI_POPUP_BODY_LEN];
            const char* fb = "";
            if (ctx->messages) {
              popup_msg_fill(ctx->messages, "DEADCONVERTS", &tok, fb, body, sizeof(body));
            } else {
              snprintf(body, sizeof(body), "%s", fb);
            }
            ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
          }
        }
        int want_eu = 0;
        const int ships_ready = units_tick_ship_build_ready(
          ctx->units,
          ctx->colonies,
          ctx->human_nation,
          ctx->human_nation,
          ctx->status,
          ctx->status_size,
          &want_eu
        );
        (void)units_tick_drydock_repair(
          ctx->units,
          ctx->colonies,
          ctx->human_nation,
          ctx->human_nation,
          ctx->status,
          ctx->status_size,
          ctx->ai_popups,
          ctx->messages
        );
        turn_route_damaged_ships(ctx, ctx->human_nation);
        if (want_eu) {
          proc->result.request_europe_open = true;
        }
        if (ships_ready > 0 && ctx->ai_popups && ctx->status && ctx->status[0]) {
          popup_chrome_ok(ctx->ai_popups, ctx->messages, "CARGOREADY0", NULL, ctx->status);
        }
        /* No King's-Galleon offer here: FUN_465b_0000 raw 75800-75815 is the
         * only DOS trigger and it fires on the move onto the colony tile
         * (game_loop.c move tail). The end-of-turn sweep that used to live
         * here re-offered parked Treasures every turn and auto-cashed them
         * post-WoI, neither of which DOS does. */
      }
      /* Go-To resumes at 10 steps/sec in game_update so the player can watch.
       * The skip-aware form: a bare turn_select_next_unit hands control back
       * parked on a Fortified/Sentried unit (prior audit #34), which then
       * flashes into control for a frame. */
      turn_select_next_unit_awaiting_orders(ctx->units, ctx->human_nation);
      if (turn_option_autosave(ctx->col1, ctx->col1_ok)) {
        /* FUN_130d_0172: exactly one slot — decade Spring (year%10==0,
         * autumn==0, turn>2) goes to slot 8, every other autosave to 9. */
        if ((*ctx->game_year % 10u) == 0u && *ctx->game_autumn == 0 &&
            *ctx->turn_number > 2u) {
          proc->result.request_autosave_decade = true;
        } else {
          proc->result.request_autosave_turn = true;
        }
      }
      turn_finish_status(ctx, &proc->result);
      diag_info(
        "turn_end: turn=%u year=%u autumn=%u colonies=%d shortages=%d immigrants=%d",
        (unsigned)*ctx->turn_number,
        (unsigned)*ctx->game_year,
        (unsigned)*ctx->game_autumn,
        proc->result.colonies_produced,
        proc->result.food_shortages,
        proc->result.immigrants_arrived
      );
      proc->step = TURN_PROC_IDLE;
}

bool turn_processor_advance(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx) {
  if (!proc || !ctx || proc->step == TURN_PROC_IDLE) {
    return false;
  }
  if (!ctx->turn_number || !ctx->game_year || !ctx->game_autumn) {
    proc->step = TURN_PROC_IDLE;
    proc->show_indicator = false;
    return false;
  }

  /* AI combat involving the human can enqueue outcome modals. */
  units_set_combat_popups(ctx->ai_popups, ctx->messages);
  units_set_combat_human_nation(ctx->human_nation);
  /* bugs.md #463: a damaged hull with no repair port leaves for Europe the
   * instant it loses (FUN_5fef_0352 off-map slot) — units.c needs the screen. */
  units_set_combat_europe(ctx->europe);
  /* LABELS.TXT wording for the status lines composed deep inside production;
   * threaded as a slice-scoped static because turn_run_colony_production's
   * signature is pinned by ~60 test call sites. */
  turn_labels = ctx->labels;

  switch (proc->step) {
    case TURN_PROC_SETUP:
      turn_step_setup(proc, ctx);
      break;
    case TURN_PROC_EURO:
      turn_step_euro(proc, ctx);
      break;
    case TURN_PROC_INDIAN:
      turn_step_indian(proc, ctx);
      break;
    case TURN_PROC_FINISH:
      turn_step_finish(proc, ctx);
      return true;
    case TURN_PROC_KING:
      turn_step_king(proc, ctx);
      return false;
    case TURN_PROC_IDLE:
    default:
      proc->step = TURN_PROC_IDLE;
      proc->show_indicator = false;
      return false;
  }
  return proc->step != TURN_PROC_IDLE;
}

ColonizeTurnResult turn_end(ColonizeTurnContext* ctx) {
  ColonizeTurnResult empty;
  memset(&empty, 0, sizeof(empty));
  if (!ctx || !ctx->turn_number || !ctx->game_year || !ctx->game_autumn) {
    return empty;
  }
  ColonizeTurnProcessor proc;
  turn_processor_start(&proc);
  while (turn_processor_advance(&proc, ctx)) {
  }
  return proc.result;
}

/*
 * New-game / load hook (sibling of ai_euro_reset / ai_native_reset /
 * ai_goals_reset / founding_fathers_reset / ai_contact_reset): the
 * production-scope debug filters default to "unset" (matching how
 * turn_prod_nation_in_scope treats them, not to nation 0), and the birth-
 * units pool pointer must not survive into a different campaign's pool.
 */
void turn_reset(void) {
  memset(turn_prof_census, 0, sizeof(turn_prof_census));
  turn_birth_units = NULL;
  turn_prod_only_nation = -1;
  turn_prod_only_set = false;
  turn_prod_skip_nation = -1;
  turn_prod_skip_set = false;
}
