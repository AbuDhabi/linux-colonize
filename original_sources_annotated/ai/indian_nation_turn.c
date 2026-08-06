/*
 * Indian nation turn — FUN_4d56_1816 and quiet NEW WORLD Brave slice.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c
 * Linux:  src/core/ai.c — ai_indian_nation_turn / ai_native_nation_pulse /
 *         ai_native_pick_dir / ai_dos_move_spent
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

/* From ai/accessors.c */
extern void ai_reseed_from_timer(uint16_t timer_word);
extern int rng_range(int lo, int hi_inclusive);
extern int tile_fa_flags(int x, int y);
extern int tile_has_minor_river(int x, int y);
extern int ocean_or_high_seas(int x, int y);
extern int owner_nibble(int x, int y);
extern void set_owner_nibble(int x, int y, int nation_or_ff);
extern int dos_dist(int dx, int dy);
extern int move_spent_cost_only(int unit_index, int from_x, int from_y,
                                int to_x, int to_y, int dir);
extern int unit_has_moves_remaining(int unit_index);
extern uint8_t terrain_byte(int x, int y);
extern uint8_t layer2_byte(int x, int y);

/* Parked / unlabeled callees kept as named stubs. */
extern void indian_select_nation_context(int indian_index); /* FUN_281f_0a42 */
extern void turn_owner_chrome(uint8_t color);               /* FUN_281f_0590 */
extern int dist_to_focus(int indian_index, int focus);      /* FUN_281f_030c */
extern void tribe_growth_tick(int tribe_index);             /* FUN_41f2_0280 → 152e path */
extern void ui_pump(void);                                  /* FUN_281f_0470 */
extern void indian_relation_tick(int indian_index);         /* FUN_2a1f_0270 */
extern void unit_clear_orders(int unit_index);              /* FUN_281f_0934 */

/* 8-way deltas — same order as Linux k_ai_dir8_*. */
static const int k_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

/* ====================================================================== */
/* Per-unit act thunk                                                     */
/* ====================================================================== */

/*
 * Ghidra: func_0x00042191 | indian_unit_act
 *
 * Called from the 1816 unit loop once per successful unit_has_moves_remaining.
 * Body not yet labeled in the export (absolute thunk). Quiet NEW WORLD Braves
 * (colony_count==0, goods==0) resolve to dir-pick + step below; alarmed /
 * raid / goods-carrying branches reach FUN_4d56_2154 / 2820 / 4528 (parked).
 *
 * Linux approximates the quiet path only inside ai_native_nation_pulse.
 */
void indian_unit_act(int unit_index) {
  (void)unit_index;
  /* TODO(RE): recover body; quiet path → quiet_brave_pick_dir + apply_step */
}

/* ====================================================================== */
/* Village growth (brief)                                                 */
/* ====================================================================== */

/*
 * Ghidra: FUN_4d56_152e | village_growth_accum
 *
 * Capitals only in the Linux T0 port. Accumulator at tribe+6; when past
 * VICEROY_VILLAGE_GROWTH_THRESHOLD (19) → reset and either pop++ (mode 2) or
 * spawn-adjacent Brave (mode 1). Full body also ticks Euro friction — parked.
 *
 * Linux: ai_grow_villages.
 */
void village_growth_accum(int /*nation_or_ctx*/ unused) {
  (void)unused;
  /*
   * tribe = *g_cur_tribe_ptr (DS:0x8d4a)
   * if capital / growth mode:
   *   tribe->growth_accum += tribe->population
   *   if growth_accum > 19:
   *     growth_accum = 0
   *     either population++ or spawn Brave via FUN_281f_095c
   */
}

/* ====================================================================== */
/* Quiet NEW WORLD dir pick                                               */
/* ====================================================================== */

/*
 * Ghidra: quiet slice of FUN_521d_20e6 / label FUN_4d56_021a
 * Annotated: quiet_brave_pick_dir
 *
 * Score dirs 0..7 (and burn stay dir 8). Empirical Linux port uses base 200;
 * full ASM quiet at 521d:4ea9 differs (base range(1,3), −2f76[terr], …) —
 * see ai/move_scoring.md. PORT DEBT markers call out Linux-only bridges.
 *
 * Linux: ai_native_pick_dir.
 */
int quiet_brave_pick_dir(
  int x,
  int y,
  int nation_id,
  int home_x,
  int home_y,
  int last_dir,
  int nation_tech
) {
  int best_dir = VICEROY_DIR_STAY;
  int best_score = -1;
  int unit_fa = tile_fa_flags(x, y);
  int unit_river = tile_has_minor_river(x, y);

  for (int d = 0; d < 9; ++d) {
    int nx = x + k_dir8_dx[d];
    int ny = y + k_dir8_dy[d];

    if (d < 8) {
      /* Reject ocean / high seas / impassable classes. */
      uint8_t terr = (uint8_t)(terrain_byte(nx, ny) & VICEROY_TERRAIN_TYPE_MASK);
      if (terr == VICEROY_TERRAIN_OCEAN || terr == VICEROY_TERRAIN_HIGH_SEAS ||
          terr >= 0x18) {
        continue;
      }
      if (ocean_or_high_seas(nx, ny)) {
        continue;
      }
    }

    int own = owner_nibble(d < 8 ? nx : x, d < 8 ? ny : y);
    /* Foreign-owned → combat path (not quiet). Quiet NEW WORLD skips. */
    if (d < 8 && own >= 0 && own != nation_id) {
      continue;
    }

    int score = 0xc8; /* 200 — PORT DEBT: ASM quiet uses range(1,3) base */

    if (d == VICEROY_DIR_STAY) {
      /*
       * Stay: burn rng_range(0, (tech+1)*4); if 0, score -= 0x19.
       * Never selected as best among moves for quiet path, but MUST burn LCG.
       */
      if (nation_tech < 0) {
        nation_tech = 0;
      }
      int stay_roll = rng_range(0, (nation_tech + 1) * 4); /* burns rng */
      if (stay_roll == 0) {
        score -= 0x19;
      }
      continue; /* stay never wins quiet best */
    }

    /* Facing bias vs last_dir — PORT DEBT: empirical +4/−6/+3 vs ASM −diff²×2 */
    if (d == last_dir) {
      score += 4;
    } else if (d == (last_dir ^ 4)) {
      score -= 6;
    } else {
      int diff = d - last_dir;
      if (diff < 0) {
        diff = -diff;
      }
      if (diff > 4) {
        diff = 8 - diff;
      }
      if (diff == 1) {
        score += 3;
      }
    }

    /* Home-base +4 when fa/fa or cardinal river/river; else home-dist penalty. */
    {
      int nbr_fa = tile_fa_flags(nx, ny);
      int nbr_river = tile_has_minor_river(nx, ny);
      int add_home_base = 0;
      if (nbr_fa != 0 && unit_fa != 0) {
        add_home_base = 1;
      } else if ((d & 1) == 0 && nbr_river != 0 && unit_river != 0) {
        add_home_base = 1;
      }
      if (add_home_base) {
        score += 4;
      }
      if (home_x >= 0) {
        int home_dist = dos_dist(nx - home_x, ny - home_y);
        if (home_dist > 2) {
          score -= home_dist * 3;
        }
      }
    }

    /* Own-nation −0x28 (skip river-into-tribe corridor). */
    if (own == nation_id) {
      int nbr_river = tile_has_minor_river(nx, ny);
      int into_tribe = (layer2_byte(nx, ny) & VICEROY_LAYER2_TRIBE) != 0 &&
                       unit_river != 0 && nbr_river != 0 && (d & 1) == 0;
      if (!into_tribe) {
        score -= 0x28;
      }
    }

    if (own < 0) {
      score += 5; /* unowned bonus */
    }

    int roll = rng_range(1, 5); /* burns rng — always */
    score += roll;

    if (score < 0) {
      score = 0;
    }
    if (score > best_score) {
      best_score = score;
      best_dir = d;
    }
  }

  return best_dir;
}

/* Apply one quiet step: spend MP, move, claim owner nibble. */
void quiet_brave_apply_step(int unit_index, int dir) {
  ViceroyUnit *u = VICEROY_UNIT_AT(unit_index);
  int nx = u->x + k_dir8_dx[dir];
  int ny = u->y + k_dir8_dy[dir];
  int spent = move_spent_cost_only(unit_index, u->x, u->y, nx, ny, dir);
  u->moves_spent = (uint8_t)(u->moves_spent + spent);
  u->x = (uint8_t)nx;
  u->y = (uint8_t)ny;
  set_owner_nibble(nx, ny, u->nation_id);
}

/* ====================================================================== */
/* Nation turn entry                                                      */
/* ====================================================================== */

/*
 * Ghidra: FUN_4d56_1816 | indian_nation_turn
 *
 * param_1 = Indian index 0..7 → active nation id = param_1 + 4.
 *
 * Structure:
 *   1. Reseed LCG from timer word
 *   2. Set g_active_nation_id, select context, turn-owner chrome
 *   3. Alarm prelude (NEW WORLD + flag checks; heavy RNG) — mostly parked in Linux
 *   4. Clamp indian_state[+7]
 *   5. Per-tribe growth tick for this nation
 *   6. Relation-word decay loop (16 slots) then relation_tick
 *   7. Reset act_counter for nation's units
 *   8. Unit act loop: while someone has moves, bump act_counter and
 *      indian_unit_act (or clear orders if act_counter >= 0x15)
 *
 * Linux: ai_indian_nation_turn ≈ growth + pulse + residual overlays.
 */
void indian_nation_turn(int indian_index) {
  /* --- 1. Reseed -------------------------------------------------------- */
  ai_reseed_from_timer(/* g_timer_word at DS:0x83a6 */ 0);

  /* --- 2. Active nation = indian_index + 4 ------------------------------ */
  int active_nation = indian_index + 4;
  /* g_active_nation_id = active_nation; */
  indian_select_nation_context(indian_index);
  turn_owner_chrome(/* color byte at indian_index + 0x84c */ 0);

  /* --- 3. Alarm prelude (NEW WORLD only) -------------------------------- */
  /*
   * if (g_game_flags & 1) && !(indian_state->flags & 0x20):
   *   dist = dist_to_focus(indian_index, g_focus_nation)
   *   alarm = (dist >= 0x19) && (dist >= rng_range(1,400))
   *        || (indian_state->flags & 0x40)
   *   if alarm && rng_range(0, (difficulty-5)*-2) == 0:
   *     … contact / friction / set flags 0x20 …  (parked — needs meet UI)
   *
   * Linux mid-turn substitutes fixed prelude LCG burns (Inca=14, Aztec=4)
   * because the exact CALL sites for those burns are still unlabeled.
   * PORT DEBT: do not treat those fixed burns as original structure.
   */
  (void)active_nation;

  /* --- 4. Clamp indian_state[+7] to >= 0 -------------------------------- */
  /* if (indian_state->signed_byte_7 < 0) indian_state->signed_byte_7 = 0; */

  /* --- 5. Growth over tribes owned by this nation ----------------------- */
  for (int t = 0; t < /* g_tribe_count */ 0; ++t) {
    ViceroyTribe *tr = VICEROY_TRIBE_AT(t);
    if (tr->nation_id == (uint8_t)active_nation) {
      tribe_growth_tick(t); /* eventually FUN_4d56_152e */
      ui_pump();
    }
  }

  /* --- 6. Relation-word decay (16 × int16 at indian_state+0xe) ---------- */
  for (int slot = 0; slot < 16; ++slot) {
    /* decay / clamp words; see raw 1816 LAB_4d56_19ea */
  }
  indian_relation_tick(indian_index);
  ui_pump();

  /* Accumulators at indian_state[+8]/[+10] — omitted; see raw export. */

  /* --- 7. Reset per-unit act counters for this nation ------------------- */
  for (int u = 0; u < /* g_unit_count */ 0; ++u) {
    ViceroyUnit *unit = VICEROY_UNIT_AT(u);
    if (unit->nation_id == (uint8_t)active_nation) {
      unit->act_counter = 0;
    }
  }

  /* --- 8. Unit act loop ------------------------------------------------- */
  int acted;
  do {
    ui_pump();
    acted = 0;
    for (int u = 0; !acted && u < /* g_unit_count */ 0; ++u) {
      while (unit_has_moves_remaining(u)) {
        ViceroyUnit *unit = VICEROY_UNIT_AT(u);
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
