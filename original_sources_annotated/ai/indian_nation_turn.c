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
 * CORRECTED 2026-09-08. The near CALL at 4d56:1506 (`e8 32 37`) targets
 * 4d56:4c3b, entry 6 of overlay 0x0C's JMPF stub table -> record 281f:23d4 ->
 * **FUN_4d56_021a**, NOT anything in 41f2 (Ghidra resolves this overlay's
 * near calls against the wrong segment). FUN_4d56_021a occupies
 * 4d56:021a..14fd (4836 bytes, one ENTER/one RETF) and Ghidra emitted the
 * whole span as undefined `??` bytes, which is why docs/port_plan.md calls
 * `FUN_4d56_021a` "not a real symbol". It is real; disassemble the span with
 * ndisasm at origin 0x21a.
 *
 * 021a is the whole Indian unit decision routine (alarm/contact gates, raid
 * dispatch via 2a1f:0192 / 2a1f:0210, arm/mount upgrade, dir scoring loop).
 * The quiet NEW WORLD scoring body LAB_521d_4ea9 sits one level BELOW it:
 * 021a reaches the 521d scorer through thunk 291f:012c at 021a:1182.
 *
 * Returns 0..7 move dir, 8 = stay/exhaust (021a has already called
 * FUN_281f_0934 itself at 021a:14e6 in that case), or -1 after destroying the
 * unit (021a:0337..0365: u+0x314a home village < 0 or >= DS:0x539a ->
 * FUN_281f_0808(unit), return -1).
 *
 * Other 021a facts (WIRED in ai_native_nation_pulse 2026-09-08, same day —
 * per-attempt turns_worked + 0x14 cap, full-byte facing, homeless despawn,
 * arm/mount; see src/core/ai.c inline citations):
 *   021a:11b9  u+0x314f (COL1 +0x0b `facing`) = picked dir, ALWAYS — the stay
 *              value 8 lands there too and reads back &7 == 0.
 *   021a:11cd  dir == 8 -> u+0x314c (`orders`) latches 5, then 6 on a repeat
 *              stay; 021a:126e any move resets it to 0.
 *   021a:11ef  on a stay, when FUN_281f_06be(u.x,u.y) == the unit's own
 *              nation: if (int8)indian+7 muskets > 0 and type is 0x13 or
 *              0x15 -> ++type, and rng_range(0, DS:0x53a6) == 0 -> --muskets;
 *              then if indian+0x0a horse_breeding >= 0x19 and
 *              FUN_281f_090c(unit) <= 3 -> type += 2, horse_breeding -= 0x19.
 *              (Field-upgrade path; the 152e spawn path uses 0x31/0x32.)
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
 * turn/mid_pass_indian_rank.md for the full stub map). Structure of 14fe
 * (4d56:14fe..152c, verified byte-for-byte 2026-09-08):
 *
 *   dir = indian_pick_dir(unit)          // near CALL 4c3b -> FUN_4d56_021a
 *   if dir != 8: step_unit_in_dir(unit, dir)   // 2a1f_0150 -> 465b, and this
 *                                              // arm also takes dir == -1
 *   else if dir >= 0: unit_exhaust_mp(unit)    // OR AX,AX / JL is dead code,
 *                                              // dir is 8 on this arm
 *
 * The exhaust is a duplicate: 021a already ran FUN_281f_0934 at 021a:14e6
 * before returning 8. Alarmed / raid / mission branches are inside 021a, not
 * here (2154/2820/4528 reached via 2a1f:0192 / 2a1f:0210).
 */
void indian_unit_act(int unit_index) {
  int dir = indian_pick_dir(unit_index);
  if (dir != 8) {
    /* DOS steps with whatever came back, dir == -1 included (the unit is
     * already despawned by then, so the step lands on a freed slot). */
    step_unit_in_dir(unit_index, dir);
    return;
  }
  unit_exhaust_mp(unit_index);
}

/*
 * Ghidra: FUN_4d56_01e2 | indian_wipe_tribe_settlements
 *
 * Raw viceroy_unpacked.c:81352, asm 4d56:01e2..0219. Razes every settlement of
 * one Indian slot: nation = param_1 + 4, then walk the tribe array DOWNWARD
 * (i = DS:0x539a - 1 .. 0, base DS:0x54ec, stride 0x12) and, for each row whose
 * +2 nation byte matches, `PUSH i; PUSH CS; CALL 4c22` -> record 281f:1248 ->
 * FUN_4d56_00e0(i). Descending order is load-bearing: 00e0 compacts the array.
 *
 * 2026-09-08: DEAD CODE in the shipped VICEROY.EXE. 01e2 is absent from
 * overlay 0x0C's 15-entry export stub table (4d56:4c22..4c6c), no near
 * CALL/JMP anywhere in segment 4d56 resolves to 0x01e2, and no overlay bank
 * record JMPFs to it. Nothing calls it; nothing to port. (docs/port_plan.md's
 * "per-tribe act plumbing" description of 01e2 is wrong.) Linux's
 * col1_kill_indian_nation is a wider Linux-only helper, not a port of this.
 */
void indian_wipe_tribe_settlements(int indian_index) {
  int nation = indian_index + 4;
  int tribe_count = 0; /* live: *(int *)VICEROY_DS_TRIBE_COUNT (0x539a) */
  for (int i = tribe_count - 1; i >= 0; --i) {
    ViceroyTribe* tr = VICEROY_TRIBE_AT(i);
    if ((int)tr->nation_id == nation) {
      /* FUN_4d56_00e0 — Linux col1_destroy_tribe_at. */
      (void)i;
    }
  }
}

/* Ghidra: FUN_4d56_152e | village_growth_accum — Linux
 * ai_indian_152e_village_growth (full port 2026-09-06d, no callee stubs
 * left; see docs/indians.md "2026-09-06d"). */
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
 *   6. DECODED 2026-09-06d (was "relation / goods tick ... then growth word"):
 *      6a  16-slot loop over indian+0x0e = `tons[16]` (the Col1 cargo ledger):
 *          each entry walks toward zero by `tech + 1`, clamped at 0, and a
 *          zero entry is not written at all.
 *      6b  FUN_2a1f_0270 = FUN_1000_a460 = `CALLF loader; JMPF <reloc>:06b6`,
 *          trailer overlay id 0x08 -> OVL09 -> **FUN_4962_06b6**, the
 *          per-tribe-type census recount (tribe_data_9184 /
 *          tribe_population_totals / tribe_village_counts /
 *          tribe_dwellings_91cc / village_counts_by_continent). Ghidra's C
 *          export of the a460 thunk is reloc-0000 garbage; read the raw asm.
 *      6c  `horse_breeding += horse_herds` (both signed reads), capped at
 *          `(tribe_population_totals[slot] + 0x19) * 2` — hence 6b first.
 *      Also: §4's clamp is on **muskets** (indian+7, signed), not alarm.
 *      Linux: ai_contact_indian_relation_tick (all three), verified against
 *      real TURN7.SAV by `unit_ai_indian_census`.
 *   7. Clear act_counter for all units of this nation (4d56:1a6c..1a8a;
 *      act_counter = unit +0x315a = COL1 unit +0x16 `turns_worked`; the match
 *      is on (u+0x3147 & 0xf) == DS:0x5394, the live nation byte)
 *   8. Act loop (ASM 4d56:1a8c..1b1a):
 *        do {
 *          ui_pump()                      // 281f:0470, top of every restart
 *          acted = 0
 *          for (i = 0; !acted && i < unit_count; ++i):
 *            while unit_has_moves_remaining(i):   // 281f:097a, AX-register arg
 *              ++act_counter
 *              if act_counter <= 0x14:  // ASM CMP 0x14 / JNA
 *                indian_unit_act(i); acted = 1     // then re-tests the SAME
 *                                                  // unit; the for-loop only
 *                                                  // exits at the next
 *                                                  // iteration boundary
 *              else:
 *                unit_exhaust_mp(i); act_counter = 0
 *        } while (acted)
 *      The counter is bumped once per ATTEMPT, before indian_unit_act, so a
 *      unit that only ever stays still ends the turn with act_counter >= 1.
 *      unit_has_moves_remaining = FUN_281f_097a -> FUN_1427_13b0 (raw :8766):
 *      index in range, (int8)u+0x3144 >= 0, nation nibble == DS:0x5394,
 *      (u+0x3148 & 0x80) == 0 || u+0x3146 == 0x0b, and u+0x3149 < max MP.
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
