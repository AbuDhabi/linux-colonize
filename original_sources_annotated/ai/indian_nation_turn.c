/*
 * Indian nation turn — FUN_4d56_1816 + quiet unit act FUN_4d56_14fe.
 *
 * Quiet Brave dir-pick: quiet_brave_scoring.c (ASM LAB_521d_4ea9).
 * Move spend / ocean force / post-ADD chrome: ai/move_spent.c (FUN_465b_0000).
 * MP helpers: ai/unit_mp.c (FUN_1427_* behind FUN_281f_* thunks).
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c ~81371–81690
 * ASM:    CODE_124:4d56 (14fe, 1816); act CALL mislabeled as 41f2 trampoline
 * Linux:  src/core/ai.c — ai_indian_nation_turn / ai_native_nation_pulse
 *         src/core/ai_contact.c — prelude / relation / meet / raids
 * Contact thin maps: ai/indian_contact.md, ai/indian_raid_outcomes.md
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

extern void ai_reseed_from_timer(uint16_t timer_word);
extern void move_spent_add(int unit_index, int to_x, int to_y);
extern void set_owner_nibble(int x, int y, int nation_or_ff);
extern int unit_has_moves_remaining(int unit_index);
extern void unit_exhaust_mp(int unit_index); /* FUN_1427_155e via 0934 */
extern void unit_clear_orders(int unit_index); /* alias of exhaust_mp */

/* quiet_brave_scoring.c — ASM 521d:4ea9 */
extern int quiet_brave_pick_dir_asm(int x, int y, int nation_id, int last_dir, int colony_count,
                                    int enable_fog);

extern void indian_select_nation_context(int indian_index); /* FUN_281f_0a42 */
extern void turn_owner_chrome(uint8_t color);               /* FUN_281f_0590 */
extern void tribe_growth_tick(int tribe_index);             /* FUN_41f2_0280 */
extern void ui_pump(void);                                  /* FUN_281f_0470 */
extern void indian_relation_tick(int indian_index);         /* FUN_2a1f_0270 */

static const int k_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

/*
 * Ghidra: func_0x0004219b (CALL from 14fe) | indian_pick_dir
 *
 * ASM at 14fe: near CALL into overlay-labeled 41f2 trampoline (Ghidra collision);
 * quiet NEW WORLD body is LAB_521d_4ea9 → quiet_brave_pick_dir_asm.
 * Returns 0..7 move dir, or 8 = stay/exhaust.
 */
int indian_pick_dir(int unit_index) {
  ViceroyUnit* u = VICEROY_UNIT_AT(unit_index);
  int last_dir = 0; /* live: facing byte */
  return quiet_brave_pick_dir_asm(u->x, u->y, u->nation_id, last_dir,
                                  /*colony_count=*/0, /*fog=*/1);
}

/*
 * Ghidra: FUN_2a1f_0150 → FUN_465b_0c1e | step_unit_in_dir
 *
 * Computes dest = unit.xy + dir8[dir], then FUN_465b_0000(unit, dest_x, dest_y).
 */
void step_unit_in_dir(int unit_index, int dir) {
  ViceroyUnit* u = VICEROY_UNIT_AT(unit_index);
  int nx = u->x + k_dir8_dx[dir];
  int ny = u->y + k_dir8_dy[dir];
  move_spent_add(unit_index, nx, ny);
  (void)set_owner_nibble; /* 465b commit sets owner; Linux apply_step mirrors */
}

/*
 * Ghidra: FUN_4d56_14fe | indian_unit_act
 *
 * Also the behavioral target of Ghidra abs `func_0x00042191` from the 1816 act
 * loop: 4d56:1ac4 is PUSH CS; CALL 4c31 — overlay-local stub JMPF 1a1f:03bc →
 * bank record 281f:23bc → 14fe. Ghidra places the label inside FUN_41f2_0266
 * because the record's JMPF has a reloc-0000 segment (see
 * turn/mid_pass_indian_rank.md for the full stub map). Structure of 14fe:
 *
 *   dir = indian_pick_dir(unit)
 *   if dir == 8: unit_exhaust_mp(unit); return
 *   else: step_unit_in_dir(unit, dir)   // 2a1f_0150 → 465b
 *
 * Alarmed / raid / mission branches: PARKED (2154/2820/4528 — not this path).
 */
void indian_unit_act(int unit_index) {
  int dir = indian_pick_dir(unit_index);
  if (dir == 8) {
    unit_exhaust_mp(unit_index);
    return;
  }
  if (dir < 0 || dir > 7) {
    return;
  }
  step_unit_in_dir(unit_index, dir);
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
  step_unit_in_dir(unit_index, dir);
}

/*
 * Alarm prelude (NEW WORLD bit0 @ DS:0x5382): when indian state +3 bit 0x20
 * clear, may roll difficulty-scaled RNG and set war/alarm flags, dialogs,
 * FUN_2a1f_0398. Linux: Inca=14 / Aztec=4 LCG prelude burns approximate the
 * stream cost; flag bodies PARKED.
 */
static void indian_alarm_prelude_parked(int indian_index) {
  (void)indian_index;
}

/*
 * Ghidra: FUN_4d56_1816 | indian_nation_turn
 *
 * Entry (hang dumps): resident thunk file 0x1C9A0 — CALLF overlay loader
 * (1930:0E52) then JMPF 4d56:1816. Far return forged to 1930:1554 by
 * 1930:2A02 (overlay id 0x0C; epilogue JMP 1446). Ghidra has no CALLF XREF;
 * year-loop FUN_* still open (VR_2A02 peel) — not a proven 130d edge. See
 * turn/mid_pass_indian_rank.md / tools/brave_dump/vr_1554.md.
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
 *   8. Act loop (ASM 4d56:1a8c..1b12):
 *        do {
 *          acted = 0
 *          for u in units while !acted:
 *            while unit_has_moves_remaining(u):
 *              act_counter++
 *              if act_counter <= 0x14:  // ASM CMP 0x14 / JBE
 *                indian_unit_act(u); acted = 1; break
 *              else:
 *                unit_exhaust_mp(u); act_counter = 0
 *        } while (acted)
 *
 * Spent residuals (phase 17–18):
 *   Quiet path: 14fe → (dir!=8) 2a1f_0150 → 465b ADD (+ ocean force ruled out).
 *   Post-ADD chrome (0916/0948/08da/084e/07fe/…) does NOT write 0x3149.
 *   Only 0934→155e writes spent=max outside ADD/force — and cargo/stay/act>0x14
 *   paths do not fire on the T2 holdouts. Hang X still localizes the writer.
 */
void indian_nation_turn(int indian_index) {
  ai_reseed_from_timer(0);
  int active_nation = indian_index + 4;
  indian_select_nation_context(indian_index);
  turn_owner_chrome(0);

  indian_alarm_prelude_parked(indian_index);

  int tribe_count = 0; /* live: *(int *)VICEROY_DS_TRIBE_COUNT */
  for (int t = 0; t < tribe_count; ++t) {
    ViceroyTribe* tr = VICEROY_TRIBE_AT(t);
    if ((int)tr->nation_id == active_nation) {
      tribe_growth_tick(t);
      ui_pump();
    }
  }

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
        /* ASM: CMP act, 0x14 / JBE → act; else exhaust. Same as < 0x15. */
        if (unit->act_counter <= 0x14) {
          indian_unit_act(u);
          acted = 1;
          break;
        }
        unit_exhaust_mp(u);
        unit->act_counter = 0;
      }
    }
  } while (acted);

  indian_select_nation_context(indian_index);
}

/*
 * Linux cross-reference (not DOS):
 *   ai_indian_nation_turn
 *     → reseed → ai_contact_indian_prelude
 *     → ai_grow_villages → ai_contact_indian_relation_tick
 *     → ai_native_nation_pulse (+ seed-100 overlays; Inca=14/Aztec=4 burns)
 *     → ai_contact_indian_meet_trade → ai_contact_indian_raids
 * Quiet 14fe only in pulse; alarmed 2154/2820/4528 stay PARKED inside act.
 * Meet/raid are post-pulse structural stand-ins (see indian_contact.md).
 */
