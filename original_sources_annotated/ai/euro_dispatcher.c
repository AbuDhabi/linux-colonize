/*
 * European AI nation dispatcher — FUN_521d_6d8e + goal pass 0a60 shell.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c
 *   6d8e ~93073–93325; 0a60 ~87408–88246; 5d04 parked; 5b66 → euro_unit_act.md
 * Linux:  src/core/ai_euro.c — ai_euro_dispatcher_turn
 *         src/core/ai.c — ai_euro_nation_turn / ai_euro_early_turn (seed-100)
 *
 * Goal helpers: ai/euro_goals.c. Quiet Brave scoring: ai/quiet_brave_scoring.c.
 * Euro/ocean move scoring: ai/move_scoring.md (thin). Per-unit act: euro_unit_act.md.
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

extern void ai_reseed_from_timer(uint16_t timer_word);
extern int rng_range(int lo, int hi_inclusive);
extern int unit_has_moves_remaining(int unit_index);
extern void unit_clear_orders(int unit_index); /* FUN_281f_0934 / exhaust */

/* Chrome / bookkeeping helpers (named stubs). */
extern void euro_select_nation_context(int nation_id); /* FUN_281f_0582 */
extern void turn_owner_chrome(uint8_t color);          /* FUN_281f_0590 */
extern void ui_pump(void);                             /* FUN_281f_0470 */
extern void progress_beat(int step, int color);        /* FUN_281f_0dae */

/* From euro_goals.c */
extern void upsert_primary_goal(int nation_id, int x, int y, int code, int prio);
extern void clear_work_queue(void);
extern void upsert_work_queue(int id, int score, uint8_t flag_a, uint8_t flag_b);
extern void promote_secondary_to_primary(int nation_id);
extern int probe_adjacent_contact_claim(int x, int y, int nation_id, int unk);

/* ====================================================================== */
/* Parked / thin entry points                                             */
/* ====================================================================== */

/*
 * Ghidra: FUN_521d_5d04 | euro_nation_planning
 * ~748 lines — difficulty-scaled treasury bump, colony/dock scan, NEW WORLD
 * wagon (type 0x12), Europe hire loop (→ 5c3c). Called from 6d8e via
 * 2a1f_0554 BEFORE promote/0a60. PARKED for later R4 (not early-settle critical).
 */
void euro_nation_planning(int nation_id) {
  (void)nation_id;
  /* parked — hire/treasury/difficulty; see decomp ~92325 */
}

/*
 * Ghidra: FUN_521d_20e6 | move_scoring
 * Full body ~2180 lines. Quiet Brave: quiet_brave_scoring.c.
 * Euro/ocean/ship: move_scoring.md (thin section-map). Called from 5b66 via
 * 2a1f_04f4 at act entry.
 */
void move_scoring(int unit_index) {
  (void)unit_index;
  /* parked body — see move_scoring.md */
}

/*
 * Ghidra: FUN_521d_5b66 | euro_unit_act  (thunk 2a1f_0488 from 6d8e loop)
 *
 * Per-unit act body (~1815 lines). Early path often calls move_scoring (20e6).
 * Thin section-map: ai/euro_unit_act.md. NOT nested inside 20e6.
 *
 * PORT DEBT: Linux uses ai_unit_spend_goto / early peels instead.
 */
void euro_unit_act(int unit_index) {
  ViceroyUnit *u = VICEROY_UNIT_AT(unit_index);
  /* If moves_spent==0 or orders!=0x0B goto: always score. Else ship-table gate. */
  if (u->moves_spent == 0 || u->orders != 0x0b) {
    move_scoring(unit_index); /* 2a1f_04f4 → 20e6; non-zero return aborts act */
  }
  /* PARKED: order apply / unload / fortify / combat arms — euro_unit_act.md */
  (void)u;
}

/* ====================================================================== */
/* 6d8e thunk wrappers (correct peel wiring)                              */
/* ====================================================================== */

/*
 * 2a1f_0554 → FUN_521d_5d04 (planning / hire). Was mis-wired to 0a60.
 */
void euro_nation_planning_pass(int nation_id) {
  euro_nation_planning(nation_id);
}

/*
 * 2a1f_0578 → FUN_521d_0342 (clear primary; promote secondary→primary).
 */
void euro_nation_promote_goals_pass(int nation_id) {
  promote_secondary_to_primary(nation_id);
}

/*
 * 2a1f_050c → FUN_521d_0a60 (unit/colony goal writer). Real colony-goals pass.
 */
void euro_nation_colony_goals_pass(int nation_id) {
  extern void euro_unit_colony_goals(int nation_id);
  euro_unit_colony_goals(nation_id);
}

/* ====================================================================== */
/* FUN_521d_0a60 — euro_unit_colony_goals (sectioned)                     */
/* ====================================================================== */

/*
 * Ghidra: FUN_521d_0a60 | euro_unit_colony_goals
 * ~858 lines (decomp 87408–88246). Writes primary goals + work queue.
 *
 * Thunk: 2a1f_050c from 6d8e after planning + promote.
 *
 * Goal codes seen at upsert_primary_goal (2a1f_0470 → 016a) call sites:
 *   0 CONTACT     — foreign ship bit / nearby claim / scout ring
 *   1 FOUND       — tribe-adjacent or settle tile (prio 2)
 *   3 LABOR       — colony tools/labor shortage (+0x8e)
 *   4 MILITARY    — pressure on foreign colony / tribe
 *   5|8 COLONY_*  — own colony +0x1b flags (8 if bit1, else 5)
 *   7 MIL_EXPAND  — soldier-pressure peer of FOUND (local_2e branch)
 *
 * Phases (decomp ~87408–88243):
 *   A nation+fog wipe + urgency seed   B own-unit scan
 *   C clear work queue                 D own-colony (LABOR / work-queue)
 *   E foreign-colony (PARKED mid-mil)  F tribe FOUND/MILITARY
 *   G continent stance (−0x6790) PARKED H bind units→primary goals PARKED
 *
 * Linux PORT DEBT: ai_euro_early_turn sail/unload/found peels.
 */

void euro_unit_colony_goals(int nation_id) {
  /* --- A. Nation context + wipe scratch / coarse fog -------------------- */
  euro_select_nation_context(nation_id);
  /* memset DS:0x9faa coarse fog, 0x10e bytes (FUN_1d1d_0dae) */
  /* memset DS:0xa13c / 0x9e98 (16 bytes each) + local tile scratch 0x100 */
  /* Seed aiStack_1da[64] from founding urgency (281f_035c, nation−0x7304) */

  /* --- B. Own-unit scan: cargo/order fixups + fog marks + contact ------- */
  /*
   * for each unit owned by nation_id:
   *   remap orders 'A'→'G'; clear flag bits; cargo muskets/tools → flag ors
   *   naval band type∈(0x0c,0x13) — note: wider than SHIP_A..C 0x0a..0x0c
   *   if in-bounds tile:
   *     coarse-fog explore cell |= presence weight (5 or 1)
   *     clear stale goto orders 1/2/3; probe_adjacent_contact_claim → orders=10
   *     if ocean or ship-type: skip forced orders=1
   *   else foreign ship with nation bit: upsert_primary(x,y, CONTACT, 3)
   */
  (void)probe_adjacent_contact_claim;
  (void)upsert_primary_goal;

  /* --- C. Clear work queue ---------------------------------------------- */
  clear_work_queue(); /* 2a1f_0560 → 031c */
  /* DS:0x173c / 0x173e = 0  (colony/continent claim bitsets) */

  /* --- D. Own-colony loop (early-settle relevant) ----------------------- */
  /*
   * for each colony with owner==nation:
   *   coarse fog |= 2 at colony cell
   *   if construction-flag colony:
   *     upsert CONTACT-ish COLONY_BUILD/CARGO (code 5|8) at colony xy
   *     score goods/stock → upsert_work_queue(colony_idx, score, …)
   *     LABOR upsert if tools demand (code 3, colony from +0x8e)
   *     mark units 'A' for tools draft (types 0x0b / 1 / 4, ±prof 0x15)
   */

  /* --- E. Foreign-colony loop — PARKED mid-game military ---------------- */
  /*
   * decomp ~87762–87989: MILITARY (4), CONTACT scout ring (0),
   * FOUND|MIL_EXPAND (1|7). Early gate: difficulty*turn < 0xb5 && nation<4.
   */

  /* --- F. Tribe pass (founding-adjacent) -------------------------------- */
  /*
   * for each tribe: optional MILITARY (4); best land tile → FOUND (1, prio 2)
   */

  /* --- G–H. Continent stance + bind units→goals — PARKED mid-game ------- */
  /*
   * G ~88054–88152: rewrite nation×continent stance bytes (−0x6790) ∈ {0,3,4,6}
   * H ~88153–88242: walk primary table; set order chars '1'/'t'/'i'
   */

  (void)upsert_work_queue;
  (void)nation_id;
}

/* ====================================================================== */
/* Dispatcher FUN_521d_6d8e                                               */
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
 * Thunk wiring (authoritative; matches peel shard + decomp thunks):
 *   2a1f_0530 → 5cf6     colony context refresh (inventory)
 *   2a1f_0554 → 5d04     euro_nation_planning
 *   2a1f_0578 → 0342     promote_secondary_to_primary
 *   2a1f_050c → 0a60     euro_unit_colony_goals
 *   2a1f_0488 → 5b66     euro_unit_act
 *   2a1f_0470 → 016a     upsert_primary_goal (also post-ship follow-up)
 *
 * Sticky anti-spin: DS:0x2d12 unit index, DS:0x2d14 act count; >0x14 → clear.
 *
 * Linux: ai_euro_nation_turn reseeds, ticks crosses, then either
 * ai_euro_early_turn (seed-100 fixture) or ai_euro_dispatcher_turn (structural
 * 6d8e: inventory, treaty timers, 5d04→0342→0a60, any_acted waves, sticky,
 * ship CONTACT). PORT DEBT: mid 5d04, 0a60 E–H deep, full 20e6/5b66 arms.
 */
void euro_nation_turn(int nation_id) {
  /* --- 0. Sticky clear + reseed + active nation ------------------------- */
  /* *(int*)0x2d12 = −1; *(int*)0x1740 = 0; */
  ai_reseed_from_timer(/* g_timer_word @ 0x83a6 */ 0);
  /* g_active_nation_id = nation_id; */

  euro_select_nation_context(nation_id);
  turn_owner_chrome(/* color at nation_id + 0x848 */ 0);

  /* --- 1–2. Colony inventory (shortage tallies) ------------------------- */
  /*
   * progress_beat(0, color); clear nation found-flag (−0x5f48)
   * for each colony owned by nation:
   *   2a1f_0530 → 5cf6 refresh colony context @ DS:0x8542
   *   bump shortage counters (tools/muskets/food/…) at 0xa0d4..0xa0db
   *   count dock/construction flags into nation −0x5f48
   */

  /* --- 3. Unit inventory (wagon→colony, passenger professions) ---------- */
  /*
   * progress_beat(1, color);
   * for each unit owned by nation:
   *   type 0x0c wagon with colony link → set colony bit 0x20
   *   passenger types 0x0d..0x12: decrement profession demand (−0x5f34)
   *   pioneer type 0x02: adjust muskets shortage
   */

  /* --- 4. Per-rival treaty / timer bytes -------------------------------- */
  for (int other = 0; other < 4; ++other) {
    /* 281f_0a38 diplo bits; optional clear bit0x40 + set bit1 on RNG */
    /* decrement treaty timer bytes at nation*0x13c + other … */
    (void)other;
    (void)rng_range;
  }

  /* --- 5. Plan passes (correct order) ----------------------------------- */
  progress_beat(2, 0);
  ui_pump();
  euro_nation_planning_pass(nation_id); /* 0554 → 5d04 */

  progress_beat(3, 0);
  ui_pump();
  euro_nation_promote_goals_pass(nation_id); /* 0578 → 0342 */
  euro_nation_colony_goals_pass(nation_id);  /* 050c → 0a60 */

  progress_beat(4, 0);
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
        while (unit_has_moves_remaining(u) && in_wave) {
          /*
           * Sticky: if u == DS:0x2d12, ++DS:0x2d14; if >0x14 clear orders.
           * Else DS:0x2d12 = u; DS:0x2d14 = 0.
           */
          euro_unit_act(u); /* 0488 → 5b66 */
          any_acted = 1;
          /*
           * Ship follow-up: if unit count unchanged and ship exhausted,
           * upsert_primary at ship xy code=CONTACT(2) prio=ship-type-based.
           * Camera follow for human-visible AI omitted.
           */
          break;
        }
      }
    }
  } while (any_acted);

  progress_beat(5, 0);
}

/*
 * Linux cross-reference (not DOS):
 *   ai_euro_nation_turn
 *     → if rng_seed==100 && !AI_FULL_DISPATCH: ai_euro_early_turn (fixture)
 *     → else: ai_euro_dispatcher_turn (ai_euro.c) — structural 6d8e
 * PORT DEBT: mid-game 5d04 matrix, 0a60 E–H, full 20e6 land/combat, 5b66 case 7.
 */
