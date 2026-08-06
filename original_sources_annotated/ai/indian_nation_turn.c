/*
 * Indian nation turn — FUN_4d56_1816.
 *
 * Quiet Brave dir-pick lives in quiet_brave_scoring.c (ASM LAB_521d_4ea9).
 * Move spend / ocean force: ai/move_spent.c (FUN_465b_0000).
 * This file keeps nation-turn structure + apply_step.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c ~81543–81690
 * Linux:  src/core/ai.c — ai_indian_nation_turn / ai_native_nation_pulse
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

extern void ai_reseed_from_timer(uint16_t timer_word);
extern int move_spent_cost_only(int unit_index, int from_x, int from_y, int to_x, int to_y,
                                int dir);
extern void move_spent_add(int unit_index, int to_x, int to_y); /* full 465b skeleton */
extern void set_owner_nibble(int x, int y, int nation_or_ff);
extern int unit_has_moves_remaining(int unit_index);
extern int rng_range(int lo, int hi_inclusive);

/* quiet_brave_scoring.c — ASM 521d:4ea9 */
extern int quiet_brave_pick_dir_asm(int x, int y, int nation_id, int last_dir, int colony_count,
                                    int enable_fog);

extern void indian_select_nation_context(int indian_index); /* FUN_281f_0a42 */
extern void turn_owner_chrome(uint8_t color);               /* FUN_281f_0590 */
extern void tribe_growth_tick(int tribe_index);             /* FUN_41f2_0280 */
extern void ui_pump(void);                                  /* FUN_281f_0470 */
extern void indian_relation_tick(int indian_index);         /* FUN_2a1f_0270 */
extern void unit_clear_orders(int unit_index);              /* FUN_281f_0934 */
extern int diplomacy_distance(int indian_index, int focus_nation); /* FUN_281f_030c */

static const int k_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

/*
 * Ghidra: func_0x00042191 | indian_unit_act
 *
 * Called from the 1816 act loop while unit_has_moves_remaining.
 * Quiet NEW WORLD: quiet_brave_pick_dir → apply_step (465b spend).
 * Alarmed / raid / mission branches: PARKED (reached via 2154/2820/4528 paths
 * and contact UI — not from this quiet skeleton).
 */
void indian_unit_act(int unit_index) {
  ViceroyUnit* u = VICEROY_UNIT_AT(unit_index);
  int last_dir = 0; /* live: facing byte; Linux uses unit->last_dir */
  int dir = quiet_brave_pick_dir_asm(u->x, u->y, u->nation_id, last_dir,
                                     /*colony_count=*/0, /*fog=*/1);
  if (dir < 0 || dir > 7) {
    return;
  }
  int nx = u->x + k_dir8_dx[dir];
  int ny = u->y + k_dir8_dy[dir];
  /* Prefer full move_spent_add when wiring live DS; cost-only + xy for quiet. */
  (void)move_spent_add;
  int spent = move_spent_cost_only(unit_index, u->x, u->y, nx, ny, dir);
  u->moves_spent = (uint8_t)(u->moves_spent + spent);
  u->x = (uint8_t)nx;
  u->y = (uint8_t)ny;
  set_owner_nibble(nx, ny, u->nation_id);
}

/* Ghidra: FUN_4d56_152e | village_growth_accum — Linux ai_grow_villages (T0). */
void village_growth_accum(int tribe_index) {
  (void)tribe_index;
  /* Threshold VICEROY_VILLAGE_GROWTH_THRESHOLD (19); pop cap 15. */
}

/*
 * Wrapper for SYMBOL_MAP: ASM quiet pick (no empirical base-200).
 * Stay-dir LCG burn is caller's responsibility (Linux pulse).
 */
int quiet_brave_pick_dir(int x, int y, int nation_id, int home_x, int home_y, int last_dir,
                         int nation_tech) {
  (void)home_x;
  (void)home_y;
  (void)nation_tech;
  return quiet_brave_pick_dir_asm(x, y, nation_id, last_dir, /*colony_count=*/0, /*fog=*/1);
}

void quiet_brave_apply_step(int unit_index, int dir) {
  ViceroyUnit* u = VICEROY_UNIT_AT(unit_index);
  int nx = u->x + k_dir8_dx[dir];
  int ny = u->y + k_dir8_dy[dir];
  int spent = move_spent_cost_only(unit_index, u->x, u->y, nx, ny, dir);
  u->moves_spent = (uint8_t)(u->moves_spent + spent);
  u->x = (uint8_t)nx;
  u->y = (uint8_t)ny;
  set_owner_nibble(nx, ny, u->nation_id);
}

/*
 * Alarm prelude (NEW WORLD bit0 @ DS:0x5382): when indian state +3 bit 0x20
 * clear, may roll difficulty-scaled RNG and set war/alarm flags, dialogs,
 * FUN_2a1f_0398. Linux: Inca=14 / Aztec=4 LCG prelude burns approximate the
 * stream cost; flag bodies PARKED.
 */
static void indian_alarm_prelude_parked(int indian_index) {
  (void)indian_index;
  /* parked — seed-100 early AI has no alarm contact yet */
}

/*
 * Ghidra: FUN_4d56_1816 | indian_nation_turn
 *
 * param_1 = indian slot 0..7; active nation = param_1 + 4.
 *
 * Sections:
 *   1. Reseed LCG from timer (04ca); set g_active_nation_id
 *   2. Select indian context + turn-owner chrome color
 *   3. Alarm prelude (NEW WORLD) — PARKED body
 *   4. Clamp signed alarm byte at state+7 to >= 0
 *   5. Tribe growth loop: for t in [0, g_tribe_count) matching nation
 *   6. Relation / goods tick (2a1f_0270) over 16 slots then growth word
 *   7. Clear act_counter for all units of this nation
 *   8. Act loop: while someone acted, scan units; while has_moves, bump
 *      act_counter; if < 0x15 call indian_unit_act else clear orders
 *
 * Act-loop vs Linux pulse (phase 13):
 *   DOS rescans from unit 0 after each successful act (lowest-index drain).
 *   Linux drains each Brave in pool order — equivalent for quiet when every
 *   first step costs >= max_mp, and also when early steps cost 1 (river/fa):
 *   both keep acting the same unit until spent >= 3 (097a / 1427_13b0).
 *   Multi-step / Inca tw>=2 goldens need those cost=1 river edges first;
 *   collapsing them with a diagonal peel (cost 6/9) stops the loop after one
 *   act and desyncs spent/tw. Not "second act after spent >= max".
 */
void indian_nation_turn(int indian_index) {
  ai_reseed_from_timer(0);
  int active_nation = indian_index + 4;
  /* *(int *)0x5394 = active_nation; */
  indian_select_nation_context(indian_index);
  turn_owner_chrome(0); /* color from DS table at 0x84c + indian_index */

  indian_alarm_prelude_parked(indian_index);

  /* Clamp state+7 (signed alarm / tension byte) to >= 0 — decomp 81603–81607. */

  /* Tribe growth: bound is g_tribe_count @ DS:0x539a (not a literal 0). */
  int tribe_count = 0; /* live: *(int *)VICEROY_DS_TRIBE_COUNT */
  for (int t = 0; t < tribe_count; ++t) {
    ViceroyTribe* tr = VICEROY_TRIBE_AT(t);
    if ((int)tr->nation_id == active_nation) {
      tribe_growth_tick(t);
      ui_pump();
    }
  }

  /* Relation tick over 16 word slots at state+0xe, then FUN_2a1f_0270. */
  indian_relation_tick(indian_index);
  ui_pump();

  int unit_count = 0; /* live: *(int *)VICEROY_DS_UNIT_COUNT */
  for (int u = 0; u < unit_count; ++u) {
    ViceroyUnit* unit = VICEROY_UNIT_AT(u);
    if ((int)unit->nation_id == active_nation) {
      unit->act_counter = 0;
    }
  }

  int acted;
  do {
    ui_pump();
    acted = 0;
    for (int u = 0; !acted && u < unit_count; ++u) {
      while (unit_has_moves_remaining(u)) {
        ViceroyUnit* unit = VICEROY_UNIT_AT(u);
        unit->act_counter++;
        if (unit->act_counter < VICEROY_UNIT_ACT_MAX) {
          indian_unit_act(u);
          acted = 1;
          break; /* decomp: one successful act then rescan from 0 */
        }
        unit_clear_orders(u);
        unit->act_counter = 0;
      }
    }
  } while (acted);

  indian_select_nation_context(indian_index);
}
