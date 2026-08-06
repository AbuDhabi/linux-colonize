/*
 * European AI nation dispatcher — FUN_521d_6d8e shell.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c (~252 lines)
 * Linux:  src/core/ai.c — ai_euro_nation_turn / ai_euro_early_turn
 *
 * Goal bodies FUN_521d_0a60 / 5d04 and full move scoring FUN_521d_20e6 are
 * PARKED — declared as stubs. See docs/ai_transcription.md and
 * ai/move_scoring.md.
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

extern void ai_reseed_from_timer(uint16_t timer_word);
extern int rng_range(int lo, int hi_inclusive);
extern int unit_has_moves_remaining(int unit_index);

/* Chrome / bookkeeping helpers (named stubs). */
extern void euro_select_nation_context(int nation_id); /* FUN_281f_0582 */
extern void turn_owner_chrome(uint8_t color);          /* FUN_281f_0590 */
extern void ui_pump(void);                             /* FUN_281f_0470 */
extern void unit_clear_orders(int unit_index);         /* FUN_281f_0934 */

/* ====================================================================== */
/* Parked planner / scoring entry points                                  */
/* ====================================================================== */

/*
 * Ghidra: FUN_521d_0a60 | euro_unit_colony_goals
 * ~858 lines — unit / colony goal logic. PARKED.
 * Linux early path substitutes ai_euro_early_turn sail/unload/found peels.
 */
void euro_unit_colony_goals(int nation_id) {
  (void)nation_id;
  /* parked — see docs/ai_transcription.md R4 */
}

/*
 * Ghidra: FUN_521d_5d04 | euro_unit_planning
 * ~748 lines — planning alongside 0a60. PARKED.
 */
void euro_unit_planning(int nation_id) {
  (void)nation_id;
  /* parked */
}

/*
 * Ghidra: FUN_521d_20e6 | move_scoring
 * ~3995 lines — direction / move scoring for all unit kinds.
 * Quiet NEW WORLD Brave slice: see indian_nation_turn.c + ai/move_scoring.md.
 * Non-quiet Euro / combat / ocean branches: PARKED.
 */
void move_scoring(int unit_index) {
  (void)unit_index;
  /* parked — phase 2 target for coherent quiet Brave port */
}

/*
 * Thunks reached from 6d8e unit passes (2a1f cluster). Names are descriptive
 * placeholders until RE labels the exact goal dispatch.
 */
void euro_unit_act(int unit_index) { /* thunk_FUN_2a1f_0488 */
  /* Eventually: goals → move_scoring → apply. Linux uses spend_goto / peels. */
  (void)unit_index;
  move_scoring(unit_index);
}

void euro_nation_colony_pass(int nation_id) { /* thunk_FUN_2a1f_0554 */
  (void)nation_id;
  euro_unit_colony_goals(nation_id);
}

void euro_nation_plan_pass(int nation_id) { /* thunk_FUN_2a1f_0578 / 050c */
  (void)nation_id;
  euro_unit_planning(nation_id);
}

/* ====================================================================== */
/* Dispatcher                                                             */
/* ====================================================================== */

static int unit_is_ship(uint8_t type) {
  return type == VICEROY_UNIT_TYPE_SHIP_A || type == VICEROY_UNIT_TYPE_SHIP_B ||
         type == VICEROY_UNIT_TYPE_SHIP_C;
}

/*
 * Ghidra: FUN_521d_6d8e | euro_nation_turn
 *
 * param_1 = European nation 0..3 (EN/FR/SP/DU).
 *
 * High-level structure (section markers match 0dae progress beats):
 *
 *   0. Clear sticky-unit, reseed LCG, set g_active_nation_id = param_1
 *   1. Select nation context + turn-owner chrome
 *   2. Inventory colonies owned by nation (construction / shortage flags)
 *   3. Inventory units (wagon→colony link, passenger profession tallies)
 *   4. Diplomacy / treaty byte maintenance vs other nations
 *   5. Nation colony + plan passes (2a1f_0554 / 0578 / 050c)
 *   6. Unit act loop — two waves:
 *        wave 0: ships only (types 0x0a..0x0c)
 *        wave 1: all units (ships again + land)
 *      Each ready unit → euro_unit_act; sticky-index anti-spin; optional
 *      camera follow for human-visible AI.
 *   7. Exit when a full scan finds no unit that acted
 *
 * Linux: ai_euro_nation_turn reseeds, ticks crosses, then either
 * ai_euro_early_turn (seed-100 fixture peels) or sail + opportunistic unload.
 */
void euro_nation_turn(int nation_id) {
  /* --- 0. Reseed + active nation ---------------------------------------- */
  /* g_euro_sticky_unit = -1; g_flag_1740 = 0; */
  ai_reseed_from_timer(/* g_timer_word */ 0);
  /* g_active_nation_id = nation_id; */

  euro_select_nation_context(nation_id);
  turn_owner_chrome(/* color at nation_id + 0x848 */ 0);

  /* AI crosses / church bookkeeping lives around here in Linux
   * (ai_euro_nation_turn +2). Exact DOS site is among the colony inventory. */

  /* --- 2–3. Colony + unit inventory (shortage / profession tallies) ----- */
  /*
   * for each colony owned by nation_id:
   *   refresh colony pointer; bump shortage counters (tools/muskets/food/…)
   * for each unit owned by nation_id:
   *   wagons linked to colonies; passenger profession tallies
   */

  /* --- 4. Per-rival treaty / timer bytes -------------------------------- */
  for (int other = 0; other < 4; ++other) {
    /* maintain diplomacy bytes at nation*0x13c + other … */
    (void)other;
  }

  /* --- 5. Goal / plan passes -------------------------------------------- */
  ui_pump();
  euro_nation_colony_pass(nation_id);
  ui_pump();
  euro_nation_plan_pass(nation_id);
  ui_pump();

  /* --- 6–7. Unit act loop (ships then land) ----------------------------- */
  int any_acted;
  do {
    ui_pump();
    any_acted = 0;

    for (int wave = 0; wave < 2; ++wave) {
      /* Scan units high→low (decomp: local_1c = count; while --local_1c >= 0). */
      for (int u = /* g_unit_count */ 0 - 1; u >= 0; --u) {
        ViceroyUnit *unit = VICEROY_UNIT_AT(u);
        int is_ship = unit_is_ship(unit->type);
        int in_wave = (wave != 0) || is_ship;
        if (!in_wave) {
          continue;
        }
        /* nation filter is inside unit_has_moves_remaining via g_active_nation */
        while (unit_has_moves_remaining(u) && in_wave) {
          /* Sticky anti-spin: same unit > 0x14 times → clear orders. */
          euro_unit_act(u);
          any_acted = 1;
          /* camera / human-visible follow omitted */
          break; /* decomp sets local_a and continues outer scan */
        }
      }
    }
  } while (any_acted);

  /* progress beat 5 + return */
}

/*
 * Linux cross-reference (not DOS):
 *   ai_euro_nation_turn
 *     → if rng_seed==100 && early turn: ai_euro_early_turn (fixture peels)
 *     → else: sail ships, ai_try_ship_unload / found first colony
 * Those peels are PORT DEBT toward replacing this dispatcher with real
 * euro_unit_colony_goals / euro_unit_planning.
 */
