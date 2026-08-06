/*
 * Indian nation turn — FUN_4d56_1816.
 *
 * Quiet Brave dir-pick lives in quiet_brave_scoring.c (ASM LAB_521d_4ea9).
 * This file keeps nation-turn structure + apply_step.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c
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
extern void set_owner_nibble(int x, int y, int nation_or_ff);
extern int unit_has_moves_remaining(int unit_index);

/* quiet_brave_scoring.c — ASM 521d:4ea9 */
extern int quiet_brave_pick_dir_asm(int x, int y, int nation_id, int last_dir, int colony_count,
                                    int enable_fog);

extern void indian_select_nation_context(int indian_index);
extern void turn_owner_chrome(uint8_t color);
extern void tribe_growth_tick(int tribe_index);
extern void ui_pump(void);
extern void indian_relation_tick(int indian_index);
extern void unit_clear_orders(int unit_index);

static const int k_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

/* Ghidra: func_0x00042191 | indian_unit_act — quiet path via quiet_brave_scoring.c */
void indian_unit_act(int unit_index) {
  (void)unit_index;
}

/* Ghidra: FUN_4d56_152e | village_growth_accum */
void village_growth_accum(int unused) {
  (void)unused;
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

void indian_nation_turn(int indian_index) {
  ai_reseed_from_timer(0);
  int active_nation = indian_index + 4;
  indian_select_nation_context(indian_index);
  turn_owner_chrome(0);

  for (int t = 0; t < 0; ++t) {
    ViceroyTribe* tr = VICEROY_TRIBE_AT(t);
    if (tr->nation_id == (uint8_t)active_nation) {
      tribe_growth_tick(t);
      ui_pump();
    }
  }
  indian_relation_tick(indian_index);
  ui_pump();

  for (int u = 0; u < 0; ++u) {
    ViceroyUnit* unit = VICEROY_UNIT_AT(u);
    if (unit->nation_id == (uint8_t)active_nation) {
      unit->act_counter = 0;
    }
  }

  int acted;
  do {
    ui_pump();
    acted = 0;
    for (int u = 0; !acted && u < 0; ++u) {
      while (unit_has_moves_remaining(u)) {
        ViceroyUnit* unit = VICEROY_UNIT_AT(u);
        unit->act_counter++;
        if (unit->act_counter < VICEROY_UNIT_ACT_MAX) {
          indian_unit_act(u);
          acted = 1;
        } else {
          unit_clear_orders(u);
          unit->act_counter = 0;
        }
      }
    }
  } while (acted);

  indian_select_nation_context(indian_index);
}
