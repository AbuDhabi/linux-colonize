#include "core/internal.h"
#include "core/ai.h"
#include "core/combat_strength.h"
#include "core/units_combat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/founding_fathers.h"
#include "core/ai_goals.h"
#include "core/ai_king.h"
#include "core/col1_bridge.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/ai_internal.h"
#include "core/dos_rng.h"
#include "core/map_gen.h"
#include "core/new_game.h"
#include "core/strutil.h"
#include "core/turn.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * Sections:
 *  - Native pick-dir dispatch, move-spent accounting & init schedule
 *  - First contact, brave-turn origin & brave step/order execution
 *  - Nation pulse, indian nation turn, kill-nation & reset
 */

/* ===================== Native pick-dir dispatch, move-spent accounting & init schedule (ai_native_pick_dir .. ai_init_sched_apply) ===================== */
static int ai_native_pick_dir(
  AiRng* rng,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeUnit* u,
  int nation_id,
  int last_dir,
  Ai021aResult* res
) {
  res->dir = 8;
  res->flags = 0;
  if (!col1) {
    return res->dir; /* no Col1 record: 021a cannot run, the Brave stays. */
  }
  (void)last_dir;
  int dir = ai_native_pick_dir_021a(
    rng, map, units, col1, ai_s_native_colonies, u, nation_id, res
  );
  dir = ai_native_apply_seed100_peels(
    nation_id, u->x, u->y, dir, ai_score_at_match(nation_id, u->x, u->y), NULL, NULL, 0
  );
  res->dir = dir;
  return dir;
}


/* FUN_281f_0754 / mask &0x0a handled by ai_mask_fa_flags above. */

/*
 * bugs.md #847: the Brave step's cost head is the SAME FUN_465b cost head
 * every other mover uses — terr_cost[class(dest)]*3, both-FA pair -> 1,
 * both-river + cardinal -> 1, tribe/settlement dest capped at 3. It used to
 * be a private copy here that also carried the ">100 -> 1" sentinel bugs.md
 * #718 proved was a port invention. Folded onto map_move_spent_thirds
 * (map.c) so the two cannot drift; `dir` is gone because the cardinal test
 * is the axis equality the shared helper already does.
 */
static int ai_dos_move_spent(
  const ColonizeWorldMap* map,
  int from_x,
  int from_y,
  int to_x,
  int to_y
) {
  return map_move_spent_thirds(map, from_x, from_y, to_x, to_y);
}

/* T4.6 (closed statically 2026-09-08): the seed-100 Brave "writer after ADD"
 * was FUN_5bfb_022e's exhaust tail (LAB_5bfb_1005) — 465b's commit tail runs
 * 0984 (adjacent-foreign probe) -> 2a1f_0192 -> FUN_5bfb_3180 -> 2a1f_066c ->
 * FUN_5bfb_022e; on a FIRST contact (met bit 0x20 clear) the ceremony runs and
 * the tail exhausts the MOVER when it is Indian (0934 -> 1427_155e, spent :=
 * max MP = 3 for a Brave). Both TURN2->3 rows had an unmet Euro land unit
 * adjacent to the dest tile (France soldier at (50,38); Spain units at
 * (47,53)/(47,54)) — Euro phase runs BEFORE the Indian phase (dump_1816 /
 * vr_2a02_v3). Ported in ai_contact_indian_meet_trade's first-meet arm; the
 * AI_EMPIRICISM-only Brave end-state overlay tables that stood in for it were
 * deleted 2026-09-14 with the rest of the empirical picker. */

/*
 * Init-pulse burn-schedule sweep tool. AI_INIT_SCHED="n:idx:count[:R];..."
 * burns `count` draws before Brave `idx` of nation `n` picks (idx = -1:
 * before that nation's pulse); a trailing `R` reseeds to the pulse seed
 * first. This is diagnostic only; the default pulse does not add draws.
 */
static bool ai_init_sched_apply(AiRng* rng, int nation_id, int brave_index) {
  const char* p = getenv("AI_INIT_SCHED");
  if (!p) {
    return false;
  }
  while (*p) {
    int n = 0;
    int idx = 0;
    int cnt = 0;
    int used = 0;
    char r = 0;
    if (sscanf(p, "%d:%d:%d:%c%n", &n, &idx, &cnt, &r, &used) < 4) {
      r = 0;
      if (sscanf(p, "%d:%d:%d%n", &n, &idx, &cnt, &used) < 3) {
        break;
      }
    }
    if (n == nation_id && idx == brave_index) {
      if (r == 'R') {
        dos_rng_seed(rng, ai_s_init_pulse_seed);
      }
      for (int b = 0; b < cnt; ++b) {
        (void)ai_rng_next_counted(rng);
      }
      if (ai_lcg_audit_enabled()) {
        fprintf(stderr, "AI_LCG_AUDIT sched n=%d idx=%d burns=%d reset=%d\n", n, idx, cnt, r == 'R');
      }
    }
    p += used;
    while (*p == ';') {
      ++p;
    }
  }
  return true;
}

/*
 * FUN_4d56_1816 unit-action core (one pulse): reseed caller-side via 04ca,
 * then while MP remain, one 14fe-style action per step (FUN_465b spent add).
 *
 * 2026-09-08 — 1816 §7/§8 + FUN_4d56_14fe re-read off the raw asm (Ghidra
 * mis-resolves every `PUSH CS; CALL` in this overlay into CODE_112, so the
 * decompile's callee names are wrong; the real map is the 15-entry JMPF stub
 * table at 4d56:4c22..4c6c, turn/mid_pass_indian_rank.md).
 *
 *   §7  4d56:1a6c..1a8a  for u in 0..DS:0x539c: if (u+0x3147 & 0xf) ==
 *       DS:0x5394 -> u+0x315a = 0.  (+0x315a = COL1 unit +0x16 =
 *       `col1_counter16`, the per-unit act counter.)
 *   §8  4d56:1a8c..1b1a
 *         do { ui_pump(281f:0470); acted = 0;
 *              for (i = 0; !acted && i < DS:0x539c; ) {
 *                while (FUN_281f_097a(i)) {      // AX-register arg
 *                  ++u[i]+0x315a;
 *                  if (u[i]+0x315a <= 0x14) { 14fe(i); acted = 1; }
 *                  else { FUN_281f_0934(i); u[i]+0x315a = 0; }
 *                }
 *                ++i;
 *              }
 *         } while (acted);
 *       FUN_281f_097a -> FUN_1427_13b0 (:8766) gates on: 0 <= i < unit count,
 *       (int8)u+0x3144 >= 0, (u+0x3147 & 0xf) == DS:0x5394, (u+0x3148 & 0x80)
 *       == 0 || type == 0x0b, and u+0x3149 (spent) < the unit's max MP.
 *       The `acted` restart re-scans from index 0, but a unit that just acted
 *       still has MP, so DOS drains one unit fully before moving on — the same
 *       order this per-unit loop uses.
 *   14fe (4d56:14fe..152c) is only three branches:
 *         dir = FUN_4d56_021a(unit)             // near CALL 4c31 -> stub 4c3b
 *         if (dir != 8) FUN_2a1f_0150(unit, dir)  // -> FUN_465b_0c1e step
 *         else if (dir >= 0) FUN_281f_0934(unit)  // exhaust MP; test is dead,
 *                                                 // dir is 8 on that arm
 *       FUN_4d56_021a already exhausts on its own dir == 8 exit (021a:14e6),
 *       so the single `moves = max_mp` below covers both writes.
 *
 * The four 021a/§8 deltas above the pulse loop (per-attempt act counter +
 * 0x14 cap, full-byte facing write incl. stay=8, homeless despawn, in-field
 * arm/mount on stay) were WIRED 2026-09-08 — see the inline citations in
 * ai_native_nation_pulse. Two deliberate port guards, documented inline:
 * the stay/move orders latch only touches NONE/FORTIFY/FORTIFIED (DOS
 * escorts leave 021a via the raid dispatch, never the wander tail, so the
 * Linux FOLLOW/GOTO machinery must survive the latch), and a Brave upgraded
 * to a mounted type keeps this pulse's max_mp=3 until the next turn refresh
 * (DOS re-reads 090c per act; the port's spent-byte semantics make the
 * difference invisible outside the upgrade turn itself).
 */
/* Pre-pulse Brave tiles for this turn — see ai_native_brave_turn_origin. */
static int16_t s_brave_origin_x[COLONIZE_UNITS_MAX];
static int16_t s_brave_origin_y[COLONIZE_UNITS_MAX];
static uint8_t s_brave_origin_ok[COLONIZE_UNITS_MAX];

/*
 * FUN_5bfb_3180 on a BRAVE step (465b commit tail -> 0984 -> 0192 -> 3180 ->
 * 022e): first contact with a Euro unit/colony beside the new tile is opened
 * at the step itself, and the mover's MP is exhausted (LAB_5bfb_1005,
 * brave_spent_callgraph.md). Later acts of the same nation in the same pulse
 * therefore already read MET (seed-100 TURN2: the Sioux Brave at (45,52)
 * meets Spain, so the (48,56) Brave scores the Spanish soldier as met).
 * The turn context is stashed here because the pulse itself is ctx-free.
 */
static ColonizeTurnContext* s_ai_native_ctx = NULL;
static uint8_t s_ai_first_contact_this_turn[8][4];
/*
 * DOS's `aiStack_20[nation]` (viceroy 98653-98676): the 3180 move tail
 * resolves at most ONE 022e encounter per nation per pass. bugs.md #824
 * moved the gift/beg apply onto the step site, so the latch lives here now
 * instead of being implied by ai.c §9's once-per-nation arm order.
 */
static uint8_t s_ai_visit_applied_this_turn[8];

/* ===================== First contact, brave-turn origin & brave step/order execution (ai_native_first_contact_this_turn .. ai_native_brave_step) ===================== */
int ai_native_first_contact_this_turn(int nation_id, int euro_nation) {
  if (nation_id < 4 || nation_id > 11 || euro_nation < 0 || euro_nation > 3) {
    return 0;
  }
  return s_ai_first_contact_this_turn[nation_id - 4][euro_nation] != 0;
}

/* Returns 1 when a first contact fired (MP exhausted by the caller). */
static int ai_native_step_first_contact(
  ColonizeUnitPool* units, const ColonizeWorldMap* map, ColonizeCol1Save* col1,
  ColonizeUnit* u, int nation_id
) {
  if (!s_ai_native_ctx || !col1 || nation_id < 4 || nation_id > 11) {
    return 0;
  }
  ColonizeCol1Indian* ind = &col1->indian[nation_id - 4];
  int fired = 0;
  int done[4] = {0, 0, 0, 0};
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + k_ai_dir8_dx[d];
    const int ny = u->y + k_ai_dir8_dy[d];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    int e = -1;
    const int oid = units_id_at(units, nx, ny);
    const ColonizeUnit* o = oid >= 0 ? units_get_const(units, oid) : NULL;
    if (o && o->active && o->nation_id >= 0 && o->nation_id <= 3 && !units_is_sea(units, oid)) {
      e = o->nation_id;
    } else if (ai_s_native_colonies) {
      const int cid = colonies_id_at(ai_s_native_colonies, nx, ny);
      const ColonizeColony* c = cid >= 0 ? colonies_get(ai_s_native_colonies, cid) : NULL;
      if (c && c->active && c->nation_id >= 0 && c->nation_id <= 3) {
        e = c->nation_id;
      }
    }
    if (e < 0 || done[e]) {
      continue;
    }
    done[e] = 1; /* 3180: one 022e per other nation per scan (aiStack_20) */
    if (ind->euro_diplo[e] == 0) {
      (void)ai_contact_try_first_welcome(s_ai_native_ctx, e, nation_id);
      s_ai_first_contact_this_turn[nation_id - 4][e] = 1;
      fired = 1;
      continue;
    }
    /*
     * Already met: 022e's visit-mood rolls (raw 96735-96760) — skipped when
     * the mover carries the pending-encounter bit, stands on an Indian
     * settlement tile (FUN_137f_0392 >= 0), or has no home village.
     */
    if (fired || (u->col1_flags15 & 0x08u) || ai_021a_settle_owner(map, u->x, u->y) >= 4 ||
        u->home_tribe_id < 0) {
      continue;
    }
    if (ai_021a_trace_enabled()) {
      fprintf(stderr, "AI_021A_VISITROLL n=%d xy=(%d,%d) e=%d\n", nation_id, u->x, u->y, e);
    }
    if (!ai_contact_visit_step_roll(s_ai_native_ctx, nation_id, e, u->id)) {
      continue;
    }
    /*
     * bugs.md #824: DOS resolves the WHOLE encounter here, in the 465b move
     * tail (0984 -> 3180 -> 022e), not in a later per-nation sweep. The mood
     * roll above is 022e's head (raw 96745-96760); these two arms are its
     * two halves — LAB_5bfb_096c gifts when bVar6, else LAB_5bfb_0def
     * begs/demands. Not exclusive in the beg direction: a CONCEDED beg sets
     * bVar6 and falls through into the gift half of the same visit, which
     * ai_contact_apply_beg_food re-enters itself (bugs.md #863). They used to run post-pulse from ai.c §9
     * off the published verdict, behind a reconstructed "did a Brave walk up
     * this turn" gate and a "no popup queue -> decline" bail; both are gone
     * with the call site that needed them. `aiStack_20[nation]` keeps it to
     * one resolved encounter per nation per pass.
     */
    if (!s_ai_visit_applied_this_turn[nation_id - 4]) {
      s_ai_visit_applied_this_turn[nation_id - 4] = 1;
      if (!ai_contact_try_village_gifts(s_ai_native_ctx, nation_id)) {
        ai_contact_try_village_beg_food(s_ai_native_ctx, nation_id);
      }
    }
  }
  return fired;
}

void ai_native_note_brave_turn_origin(int unit_id, int x, int y) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX) {
    return;
  }
  s_brave_origin_x[unit_id] = (int16_t)x;
  s_brave_origin_y[unit_id] = (int16_t)y;
  s_brave_origin_ok[unit_id] = 1;
}

int ai_native_brave_turn_origin(int unit_id, int* out_x, int* out_y) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX || !s_brave_origin_ok[unit_id]) {
    return 0;
  }
  if (out_x) {
    *out_x = (int)s_brave_origin_x[unit_id];
  }
  if (out_y) {
    *out_y = (int)s_brave_origin_y[unit_id];
  }
  return 1;
}

/*
 * FUN_465b_0000 local_4 (raw 75467-75475): the destination's settlement owner
 * (FUN_281f_06be), overridden by the nation of the unit heading the tile's
 * stack (FUN_281f_07e0 -> FUN_1427_005c). -1 = nobody.
 */
COLONIZE_INTERNAL int ai_465b_dest_owner(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, int x, int y
) {
  int owner = ai_021a_settle_owner(map, x, y);
  const int head_id = units_tile_head_id_at(units, x, y);
  const ColonizeUnit* head = units_get_const(units, head_id);
  if (head) {
    owner = head->nation_id;
  }
  return owner;
}

/*
 * One FUN_1427_13b0 act for one Brave: MP gate, 021a direction pick, the
 * partial-MP gamble, the foreign-tile attack handoff and the commit.
 * Extracted verbatim from ai_native_nation_pulse; the status enum lives in
 * ai_internal.h so a test can build one (conventions.md "Internal-header
 * test seam").
 */
COLONIZE_INTERNAL AiNativeStepStatus ai_native_brave_step(
  ColonizeUnitPool* units, ColonizeWorldMap* map, ColonizeCol1Save* col1, AiRng* rng,
  int nation_id, bool seed100_init_burns, ColonizeUnit* u, int hx, int hy, int tech,
  int max_mp, int brave_index, int* steps
) {
  /* FUN_281f_097a / 1427_13b0: act while moves_spent < max_mp (=3).
   * River/fa cost=1 steps keep spent < 3 so the inner loop continues —
   * that is the multi-step path (not a second act after spent >= max). */
  const int spent = u->moves;
  if (spent >= max_mp) {
    return AI_NATIVE_STEP_STOP;
  }
  /*
   * 1816 §8 (4d56:1af3..1b1a): the act counter bumps once per ATTEMPT,
   * before 021a runs — a Brave that only stays still ends its turn at 1.
   * Past 0x14 the unit is exhausted and the counter zeroed, no act.
   */
  u->col1_counter16++;
  if (u->col1_counter16 > 0x14) {
    u->moves = max_mp;
    u->col1_counter16 = 0;
    return AI_NATIVE_STEP_STOP;
  }
  /*
   * 021a:0337..0365 — a unit whose home village slot is out of range
   * (u+0x314a < 0 or >= DS:0x539a) is destroyed (FUN_281f_0808), not
   * re-homed; 021a returns -1 and 14fe's "step" lands on the freed slot.
   *
   * DOS-LITERAL deviation (bugs.md #826): DOS returns -1 here and its
   * caller FUN_4d56_14fe, which only tests the result against 8, then
   * dereferences the freed unit record — a use-after-free the port does not
   * reproduce. The port STOPs the Brave cleanly instead. This is the one
   * deliberate departure in the despawn arm; everything else is verbatim.
   */
  if (col1 &&
      (u->home_tribe_id < 0 ||
       u->home_tribe_id >= (int)col1->head.tribe_count)) {
    units_despawn(units, u->id);
    return AI_NATIVE_STEP_STOP;
  }
  if (ai_lcg_audit_enabled() && seed100_init_burns) {
    fprintf(
      stderr,
      "AI_LCG_AUDIT brave_begin n=%d idx=%d xy=(%d,%d) spent=%d step=%d\n",
      nation_id,
      brave_index,
      u->x,
      u->y,
      spent,
      *steps
    );
  }
  /* DOS reads unit+0x314f raw; values outside 0..7 (8 = stayed last
   * act) legitimately disable the facing term — do NOT clamp to 0. */
  const int last_dir = u->last_dir;
  if (seed100_init_burns && *steps == 0) {
    (void)ai_init_sched_apply(rng, nation_id, brave_index);
  }
  ai_s_native_home_dist = map_dos_dist(u->x - hx, u->y - hy); /* DS:0x8db8 */
  (void)tech;
  Ai021aResult pick;
  int dir = ai_native_pick_dir(rng, map, units, col1, u, nation_id, last_dir, &pick);
  if (col1) {
    const int picked = dir;
    dir = ai_native_021a_tail(units, map, col1, rng, u, nation_id, dir, pick.flags);
    if (ai_021a_trace_enabled()) {
      fprintf(
        stderr,
        "AI_021A_ACT t=%d n=%d idx=%d xy=(%d,%d) facing=%d spent=%d tw=%d pick=%d flags=%02x dir=%d\n",
        ai_s_seed100_midturn_turn, nation_id, brave_index, u->x, u->y, last_dir,
        u->moves, u->col1_counter16, picked, pick.flags, dir
      );
    }
  }
  if (dir < 0 || dir > 7) {
    /*
     * Stay (dir == 8). 021a:11b9 writes the picked dir into the facing
     * byte (COL1 +0x0b) unconditionally — the full byte value 8 (facing
     * bits 0 + pad bit0 in the save split). Keep last_dir = 8 in memory:
     * 521d:54f5 skips the facing term for any byte >= 8.
     *
     * bugs.md #846 (REFUTED) read `col1_facing_pad` here as a port-only
     * flag stealing DOS's upper 5 bits. It is not: 021a:11b9 is a FULL
     * byte store (`byte [BX+0x314f] = chosen dir`), so DOS itself zeroes
     * those bits on every act. facing (low 3) + pad (high 5) is only how
     * col1_bridge splits that one byte; writing pad = 1 on a stay is what
     * reproduces DOS's byte 8. Moving the write to a non-serialized field
     * would make a stayed Brave round-trip as facing 0 = "faced north".
     */
    u->last_dir = 8;
    u->col1_facing_pad = 1;
    /*
     * 021a:11cd orders latch: stay -> 5 (FORTIFY), repeat stay -> 6
     * (FORTIFIED) — the DOS byte values equal the port enum. DOS stomps
     * any orders byte; the port latches only over NONE/FORTIFY/FORTIFIED
     * so the Linux-side FOLLOW/GOTO escort machinery survives (DOS
     * escorts exit 021a through the raid dispatch, never this tail).
     *
     * bugs.md #826 asked whether a Brave can ever hold a Linux-only order,
     * i.e. whether the guard can go and the latch become unconditional as
     * at 021a:11cd/1272. It cannot: `ai_contact_raid.c` (:1809) hands an
     * idle Brave UNITS_ORDER_FOLLOW through units_follow_unit in the §9
     * escort peel, and that order persists into the next turn's §8 pulse.
     * An unconditional latch would overwrite it with FORTIFY on the first
     * stay. The guard stays; this is a recorded port adaptation, not an
     * oversight. The same reasoning covers the three clear sites below.
     */
    if (u->orders == UNITS_ORDER_NONE || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      u->orders = (u->orders == UNITS_ORDER_NONE) ? UNITS_ORDER_FORTIFY
                                                  : UNITS_ORDER_FORTIFIED;
    }
    /*
     * 021a:11ef..126c in-field arm/mount, gated on standing on a
     * settlement tile owned by this nation (FUN_281f_06be
     * tile_tribe_owner == nation): type 0x13/0x15 with tribe muskets > 0
     * (signed byte) -> ++type, musket spent on rng(0, difficulty) == 0;
     * then horse_breeding >= 0x19 with max MP <= 3 (FUN_281f_090c, read
     * AFTER the musket arm) -> type += 2, horse_breeding -= 0x19.
     * (Field-upgrade path; the 152e spawn path uses 0x31/0x32.)
     */
    if (col1 && nation_id >= 4 && nation_id <= 11 &&
        map_tile_has_city(map, u->x, u->y) &&
        ai_owner_nibble(map, u->x, u->y) == nation_id) {
      ColonizeCol1Indian* ind = &col1->indian[nation_id - 4];
      if ((int8_t)ind->muskets > 0 &&
          (u->type_index == UNITS_KIND_BRAVE || u->type_index == UNITS_KIND_MTD_BRAVE)) {
        u->type_index++;
        if (ai_rng_range(rng, 0, (int)col1->head.difficulty) == 0) {
          ind->muskets--;
        }
      }
      if (ind->horse_breeding >= 0x19 && units_max_mp(units, u->id) <= 3) {
        u->type_index += 2;
        ind->horse_breeding -= 0x19;
      }
    }
    u->moves = max_mp;
    return AI_NATIVE_STEP_STOP;
  }
  const int nx = u->x + k_ai_dir8_dx[dir];
  const int ny = u->y + k_ai_dir8_dy[dir];
  const int cost = ai_dos_move_spent(map, u->x, u->y, nx, ny);
  const int from_x = u->x;
  const int from_y = u->y;
  /*
   * FUN_465b_0000 foreign-destination arm (raw 75467-75479, 75631-75634, 75692):
   * local_4 = FUN_281f_06be(dest) (settlement owner), overridden by the
   * nation of the tile's stack head FUN_281f_07e0 when a unit stands there;
   * bVar4 = local_4 >= 0 && local_4 != mover nation. A bVar4 step never
   * lands the mover on the tile: with < 3 MP left it is LAB_465b_01ce
   * (FUN_281f_0934 exhaust, abort); otherwise the Brave (attack 1) takes
   * LAB_465b_025c -> 05ca -> FUN_291f_0a14 = FUN_5fef_1b0e, the attack,
   * which exhausts it (0934) and never moves it in (Indians do not
   * capture). The 05ca cost gate short-circuits on bVar4 after the
   * FUN_281f_04ca timer reseed, so there is no gamble roll.
   * The port used to commit the 021a pick as a plain step, so a Brave whose
   * pick carried the 0x0a contact/attack flags walked INTO a Euro colony
   * and sat there fortified — and its foreign presence then blocked the
   * owner's ships from docking (bugs.md #553). The Brave keeps DOS's end
   * state: in place, exhausted — and, since bugs.md #822, with the attack
   * actually resolved on the >= 3-thirds limb.
   *
   * bugs.md #822: the >= 3-thirds limb is DOS's attack. The port routes it
   * into the same resolver every other 1b0e call site uses
   * (units_resolve_land_combat -> units_resolve_land_combat_ff_w), picking
   * the defender with FUN_5fef_0000 (units_best_defender_at) exactly as the
   * §9 ambush arm does. That resolver also carries 1b0e's colony limb: a
   * Brave beaten on a colony tile reaches the raid handoff units.c
   * self-registers from ai_contact_raid.c (ColonizeUnitsRaidRepelledFn), so
   * a colony target and a lone field unit both go through one path, as in
   * DOS. Two deviations, deliberate:
   *  - defender nations are limited to the Europeans 0..3; DOS's bVar4 is
   *    any foreign owner, but the port models no tribe-versus-tribe combat
   *    anywhere, so a foreign-tribe tile keeps the exhaust-only end state;
   *  - a foreign tile whose best defender is -1 (an undefended colony: only
   *    civilians, which FUN_5fef_0000 skips) also keeps the exhaust-only end
   *    state rather than inventing an outcome. The §9 raid pass owns that
   *    case.
   */
  {
    const int dest_owner = ai_465b_dest_owner(map, units, nx, ny);
    if (dest_owner >= 0 && dest_owner != nation_id) {
      const int left = max_mp - spent;
      if (left >= 3 && cost > left && spent != 0) {
        dos_rng_seed(rng, ai_turn_seed(s_ai_native_ctx)); /* 04ca, no roll */
      }
      if (left >= 3) {
        const int foe = units_best_defender_at(units, col1, nx, ny, u->id, u->id);
        ColonizeUnit* f = foe >= 0 ? units_get(units, foe) : NULL;
        if (f && f->active && f->nation_id >= 0 && f->nation_id <= 3 &&
            !units_is_sea(units, foe)) {
          /* LAB_465b_025c -> 05ca -> FUN_291f_0a14 = FUN_5fef_1b0e. */
          (void)units_resolve_land_combat(units, u->id, foe, rng);
          const ColonizeUnit* self = units_get_const(units, u->id);
          if (!self || !self->active) {
            /* The Brave lost; 1b0e despawned it. Nothing left to stamp. */
            (*steps)++;
            return AI_NATIVE_STEP_STOP;
          }
        }
      }
      u->moves = max_mp;
      u->last_dir = dir;
      u->col1_facing_pad = 0;
      if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
        u->orders = UNITS_ORDER_NONE;
      }
      (*steps)++;
      return AI_NATIVE_STEP_STOP;
    }
  }
  /*
   * FUN_465b_0000 cost gate (viceroy_unpacked.c 75643-75647 + the
   * `else` at :75820): `(cost <= left) || (spent == 0) || (04ca(timer),
   * attack)`; a quiet step that overspends with MP already spent RESEEDS
   * the LCG from the timer word (= the fixed seed under VR_SEED / --seed)
   * and then rolls RNG(1, cost) — only `roll <= left` moves. The denied
   * unit stays put with `spent += cost` already booked at :75617, its
   * facing already stamped by 021a. Seed-100 TURN2: the Arawak Brave's
   * river second step (spent 1, cost 9) is exactly this roll, and every
   * later Arawak act reads the restarted stream.
   */
  if (spent != 0 && cost > max_mp - spent) {
    dos_rng_seed(rng, ai_turn_seed(s_ai_native_ctx));
    const int roll = ai_rng_range(rng, 1, cost);
    if (ai_021a_trace_enabled()) {
      fprintf(
        stderr, "AI_021A_GAMBLE n=%d xy=(%d,%d) dir=%d cost=%d left=%d roll=%d -> %s\n",
        nation_id, u->x, u->y, dir, cost, max_mp - spent, roll,
        roll <= max_mp - spent ? "move" : "denied"
      );
    }
    if (roll > max_mp - spent) {
      u->moves = spent + cost;
      u->last_dir = dir;
      u->col1_facing_pad = 0;
      if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
        u->orders = UNITS_ORDER_NONE;
      }
      (*steps)++;
      return AI_NATIVE_STEP_STOP;
    }
  }
  if (ai_step_audit_enabled() && ai_s_seed100_midturn_turn > 0) {
    fprintf(
      stderr,
      "AI_STEP_AUDIT t=%d n=%d from=(%d,%d) dir=%d to=(%d,%d) cost=%d spent_before=%d "
      "tw=%d step=%d\n",
      ai_s_seed100_midturn_turn,
      nation_id,
      from_x,
      from_y,
      dir,
      nx,
      ny,
      cost,
      spent,
      u->col1_counter16,
      *steps
    );
  }
  {
    const int step_ox = u->x;
    const int step_oy = u->y;
    u->x = nx;
    u->y = ny;
    units_tile_stack_arrive(units, u->id);
    units_occupancy_notify_moved(units, step_ox, step_oy, nx, ny);
    /* 465b commit tail clears+recomputes unit+0x3147's observed nibble
     * (FUN_281f_08da / 084e / 07fe) on every step — braves included. */
    units_vis_mask_after_move(units, map, u->id, nx, ny);
  }
  u->moves = spent + cost;
  /*
   * FUN_465b LAB_465b_05ca: ocean/HS flag change AND
   * euro_settlement_owner(from) < 0 AND euro_settlement_owner(dest) < 0
   * → spent = max_mp (FUN_281f_090c).
   * euro_settlement = tribe bit + Euro owner 0..3 (FUN_137f_0358).
   */
  if (ai_is_ocean_hs(map, from_x, from_y) != ai_is_ocean_hs(map, nx, ny)) {
    const int from_euro_set =
      ((ai_layer2_at(map, from_x, from_y) & 2u) != 0 &&
       ai_owner_nibble(map, from_x, from_y) >= 0 &&
       ai_owner_nibble(map, from_x, from_y) < 4);
    const int to_euro_set =
      ((ai_layer2_at(map, nx, ny) & 2u) != 0 && ai_owner_nibble(map, nx, ny) >= 0 &&
       ai_owner_nibble(map, nx, ny) < 4);
    if (!from_euro_set && !to_euro_set) {
      u->moves = max_mp;
    }
  }
  /* 021a:11b9 full-byte facing write (pad cleared on a real dir), and
   * 021a:126e — any move resets the stay latch (guarded as above). */
  u->last_dir = dir;
  u->col1_facing_pad = 0;
  if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
    u->orders = UNITS_ORDER_NONE;
  }
  ai_set_owner_nibble_move(map, nx, ny, nation_id);
  if (!seed100_init_burns && ai_native_step_first_contact(units, map, col1, u, nation_id)) {
    u->moves = max_mp; /* LAB_5bfb_1005: FUN_281f_0934 on the Indian mover */
    (*steps)++;
    return AI_NATIVE_STEP_STOP;
  }
  if (ai_lcg_audit_enabled() && seed100_init_burns) {
    fprintf(
      stderr,
      "AI_AB step n=%d idx=%d from=(%d,%d) dir=%d to=(%d,%d) cost=%d\n",
      nation_id,
      brave_index,
      from_x,
      from_y,
      dir,
      nx,
      ny,
      cost
    );
  }
  (*steps)++;
  /*
   * The DOS 0x14 act cap now trips on `col1_counter16` at the attempt top
   * (4d56:1af7). `cost <= 0` stays as a Linux-only belt (DOS has no such
   * break — it keeps acting until the counter or MP gate trips).
   */
  if (cost <= 0) {
    return AI_NATIVE_STEP_STOP;
  }
  return AI_NATIVE_STEP_MORE;
}


/* ===================== Nation pulse, indian nation turn, kill-nation & reset (ai_native_nation_pulse .. ai_native_reset) ===================== */
void ai_native_nation_pulse(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  int nation_id,
  bool seed100_init_burns
) {
  if (!units || !map || !rng || nation_id < 4 || nation_id > 11) {
    return;
  }

  ai_s_seed100_init_pulse = seed100_init_burns ? 1 : 0;
  if (seed100_init_burns) {
    (void)ai_init_sched_apply(rng, nation_id, -1);
  }
  memset(s_ai_first_contact_this_turn[nation_id - 4], 0, sizeof(s_ai_first_contact_this_turn[0]));
  s_ai_visit_applied_this_turn[nation_id - 4] = 0;

  const int max_mp = 3; /* Brave thirds allotment (FUN_281f_090c path) */
  /*
   * Mid-turn FUN_4d56_1816 prelude: DOS burns nothing on the shared stream
   * before the act loop except the 152e growth-loop draws (already on
   * ctx->rng via ai_grow_villages) and the post-independence alarm roll. The
   * old "Inca=14 / Aztec=4" burns were a fit against the retired 20e6-shaped
   * scorer; with the 021a picker they cost every Inca act (2026-09-15
   * sweep: k=0 -> zero Inca misses on all six seed-100 turns).
   */
  if (ai_lcg_audit_enabled() && seed100_init_burns) {
    fprintf(
      stderr,
      "AI_AB pulse_enter n=%d nexts=%u rng_state=0x%x mode=%s\n",
      nation_id,
      (unsigned)ai_s_lcg_total_nexts,
      (unsigned)rng->state,
      "asm"
    );
  }

  /* Clear col1_counter16 for this nation's Braves (DOS 1816 ~81630). */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (u->active && u->nation_id == nation_id) {
      u->col1_counter16 = 0;
    }
  }

  /*
   * Remember every Brave's pre-pulse tile — see ai_native_brave_turn_origin
   * in ai.h for why the §9 contact arms need it. Stamped fresh for this
   * nation's units on every pulse, and §9 runs immediately after this pulse
   * for the same nation, so a reader never sees another nation's turn.
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    ai_native_note_brave_turn_origin(u->id, u->x, u->y);
  }

  int brave_index = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      continue;
    }
    int hx = u->x;
    int hy = u->y;
    ai_find_home_tribe(col1, u, &hx, &hy);
    int tech = 0;
    if (col1 && nation_id >= 4 && nation_id <= 11) {
      tech = (int)col1->indian[nation_id - 4].tech;
    }
    int steps = 0;
    for (;;) {
      if (ai_native_brave_step(
            units, map, col1, rng, nation_id, seed100_init_burns, u, hx, hy, tech,
            max_mp, brave_index, &steps
          ) == AI_NATIVE_STEP_STOP) {
        break;
      }
    }
    brave_index++;
  }

  ai_s_seed100_init_pulse = 0;
}

/*
 * ===========================================================================
 * FUN_4d56_1b3a — the year-loop Indian mid-pass (raw viceroy_unpacked.c:
 * 81684-81738). Ported 2026-09-06d; previously "partial (known; not raid)"
 * with only phase 2 present.
 *
 * DOS structure, in order:
 *   phase 1  clear DS:0x5b04 — 8 Indian slots x 4 Euro words
 *   phase 2  for slot 0..7: if !(DS:0x5ad9 + 0x4e*slot & 0x80) -> 1816(slot)
 *   phase 3  for every colony: claim worked ring tiles off the natives
 *
 * Phase 2 is already the Linux `TURN_PROC_INDIAN` cursor loop (the DOS call
 * looks like `FUN_41f2_0266` only because of the reloc-0000 stub misresolve
 * documented in turn/mid_pass_indian_rank.md). The two halves below are
 * phases 1 and 3, and they bracket that loop exactly as DOS does.
 * ===========================================================================
 */

/*
 * Phase 1 — `for (i=0;i<8;i++) for (j=0;j<4;j++) word[(i*0x27 + j)*2 +
 * 0x5b04] = 0;`.
 *
 * Address decode: the Indian record base is DS:0x5ad6 with stride 0x4e (=
 * 0x27 words), so `0x5b04 = 0x5ad6 + 0x2e` and the four words are exactly
 * `ColonizeCol1Indian.contact_state[4]` (+0x2e). Nothing new to name.
 *
 * **Behavioural correction this exposes:** ai_contact.c's beg/gift arm
 * described `contact_state` as a permanent per-(tribe, Euro) latch ("a tribe
 * that has ever brought gifts to a nation never begs from it again"). DOS
 * wipes all 32 entries at the top of every FUN_4d56_1b3a call — once per
 * game turn (viceroy 81704-81707) — so the latch is really *per-turn*: one
 * gift-or-beg resolution per tribe/nation pair per turn. That is why DOS
 * villages keep visiting instead of going quiet forever.
 */
void ai_indian_midpass_clear_tables(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  for (int slot = 0; slot < 8; ++slot) {
    ColonizeCol1Indian* ind = &ctx->col1->indian[slot];
    for (int e = 0; e < 4; ++e) {
      ind->contact_state[e] = 0;
    }
  }
}

/*
 * Phase 3 — worked-tile ownership claim.
 *
 *   for each colony c:
 *     owner  = c[0x1a]                       // colony nation id
 *     count  = DS:0x329[FUN_281f_0c5e()]     // ring size for the Town-Hall tier
 *     for i in 0..count-1:
 *       if (c[0x70 + i] < 0) continue;       // tile not being worked
 *       x = c.x + DS:0xc8[i]; y = c.y + DS:0xde[i];
 *       n = FUN_281f_06dc(x, y);             // = FUN_137f_0200 owner nibble
 *       if (n <= 3 || n == owner) continue;  // only take from natives
 *       if (FUN_281f_06d2(x, y) >= 0) continue;  // = FUN_137f_0428:
 *                                            //   settlement (layer2 0x02) or
 *                                            //   unit (layer2 0x01) present
 *       FUN_281f_0704(x, y, owner);          // = FUN_137f_0228 owner stamp
 *
 * So: a colonist actually working a tile the natives still claim quietly
 * transfers that tile's owner nibble to the colony — unless a village or any
 * unit is standing on it. `0704`'s @SEIZURE-style popup arm can't fire here
 * (it needs a native settlement on the tile, which the `06d2` gate already
 * excluded), so this is a pure ownership stamp with no chrome.
 *
 * Ring mapping: Linux drives the loop off `colonies_field_tile_delta` +
 * `colony->tiles[]` rather than re-deriving DS:0xc8/0xde by index. DOS's own
 * enumeration order differs from `colony.h`'s `tiles[]` convention, but both
 * pair index -> offset self-consistently and cover the identical 8-tile set,
 * so the set of tiles claimed is the same. Tiers 3/4's outer ring (DS:0x329
 * = 12/20) has no Linux storage — the deliberate P4.2 decision, and every
 * real DOS `.SAV` leaves those slots 0xff, so nothing is skipped in practice.
 */
void ai_indian_midpass_claim_worked_tiles(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->colonies || !ctx->map) {
    return;
  }
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
      if (c->tiles[i] < 0) {
        continue; /* DOS: `colony[0x70 + i] < 0` — nobody works this plot. */
      }
      int dx = 0;
      int dy = 0;
      if (!colonies_field_tile_delta(i, &dx, &dy)) {
        continue;
      }
      const int tx = c->x + dx;
      const int ty = c->y + dy;
      if (!map_coords_inset(ctx->map, tx, ty)) {
        continue;
      }
      const int owner = ai_owner_nibble(ctx->map, tx, ty);
      if (owner < 4 || owner == c->nation_id) {
        continue;
      }
      /* FUN_137f_0428: settlement (0x02) or unit (0x01) occupancy blocks it. */
      if ((ai_layer2_at(ctx->map, tx, ty) & 0x03u) != 0) {
        continue;
      }
      map_set_owner_nibble(ctx->map, tx, ty, c->nation_id);
    }
  }
}

void ai_indian_nation_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 4 || nation_id > 11) {
    return;
  }
  /*
   * FUN_4d56_1816 phase order (annotated indian_nation_turn / indian_contact.md):
   *   1 reseed → 2–4 prelude/clamp → 5 growth → 6 relation → 7–8 quiet pulse →
   *   9 meet/trade + raids (other paths; not inside 14fe).
   * The prelude uses isolated contact RNG; the pulse uses the shared stream.
   */
  ai_nation_reseed(ctx);

  /* §2 WoI tribe defection (isolated RNG, no pulse LCG burn). */
  ai_contact_indian_woi_defect(ctx, nation_id);

  /* §2–4 alarm prelude (flags/mission). */
  ai_contact_indian_prelude(ctx, nation_id);

  /* §5 tribe growth. */
  ai_grow_villages(ctx, nation_id);

  /* §6 relation / goods tick. */
  ai_contact_indian_relation_tick(ctx, nation_id);

  AiRng local;
  AiRng* rng = ctx->rng;
  if (!rng) {
    const uint32_t seed = ai_turn_seed(ctx);
    dos_rng_seed(&local, seed);
    rng = &local;
  }
  /* Mid-turn pulse always runs; seed-100 latches the calendar turn so the
   * mid-turn dir peels below can key off it. */
  if (ctx->rng_seed == 100u && ctx->turn_number) {
    ai_s_seed100_midturn_turn = (int)*ctx->turn_number;
  }

  /* §7–8 quiet 14fe act loop (+ seed-100 overlays). */
  ai_s_native_colonies = ctx->colonies;
  ai_s_native_col1 = ctx->col1_ok ? ctx->col1 : NULL;
  s_ai_native_ctx = ctx;
  ai_native_nation_pulse(
    ctx->units, ctx->map, ctx->col1_ok ? ctx->col1 : NULL, rng, nation_id, false
  );
  s_ai_native_ctx = NULL;

  ai_s_seed100_midturn_turn = 0;

  /* §9 meet/trade + raids (5bfb / 4528 paths — not quiet 14fe). */
  ai_contact_indian_meet_trade(ctx, nation_id);
  /*
   * bugs.md 2026-09-04: FUN_5bfb_022e's peaceful visit picks ONE of two
   * halves — generous (@INDIANGIVEFOOD/@INDIANGIVESTUFF) or demanding
   * (@INDIANBEGFOOD / tribute).
   * bugs.md #824 (2026-09-23): both halves now run from the Brave's own step
   * (ai_native_step_first_contact above), which is where DOS's 022e runs.
   * Nothing is left to do post-pulse.
   */
  ai_contact_indian_raids(ctx, nation_id);
}

/*
 * Linux-only whole-nation wipe. The DOS analogue is FUN_4d56_01e2 (raw
 * :81352, asm 4d56:01e2..0219): nation = param_1 + 4, then walk the tribe
 * array DOWNWARD (i = DS:0x539a - 1 .. 0, stride 0x12 at DS:0x54ec) and call
 * FUN_4d56_00e0(i) — i.e. col1_destroy_tribe_at — for every row whose +2
 * nation byte matches; descending order is what keeps 00e0's array compaction
 * from skipping rows.
 *
 * 2026-09-08: 01e2 is DEAD CODE in the shipped VICEROY.EXE. It is not one of
 * the 15 entries in overlay 0x0C's export stub table (4d56:4c22..4c6c), no
 * near CALL/JMP anywhere in segment 4d56 targets 0x01e2, and no overlay bank
 * record JMPFs to it. So this helper is a Linux invention, not a port of it,
 * and it deliberately does more than an 01e2 loop would: it despawns *every*
 * unit of the nation (00e0 only despawns units whose +0x314a names the village
 * being razed), paints owner nibble 0x0f, and resets the indian[] slot, while
 * skipping 00e0's per-village horse fold / village-count decrement / @EXTINCT
 * popup (those live in col1_destroy_tribe_at).
 */
int col1_kill_indian_nation_w(
  const ColonizeWorld* w,
  int nation_id
) {
  ColonizeCol1Save* col1 = w->col1;
  ColonizeUnitPool* units = w->units;
  ColonizeWorldMap* map = w->map;

  if (nation_id < 4 || nation_id > 11) {
    return 0;
  }

  /* Despawn all units of this nation (iterate carefully — despawn mutates pool). */
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      units_despawn(units, u->id);
    }
  }

  int removed = 0;
  if (col1 && col1->tribe && col1->head.tribe_count > 0) {
    const uint16_t old_count = col1->head.tribe_count;
    /* Remap table: old index → new index, or -1 if deleted. */
    int* remap = (int*)calloc((size_t)old_count, sizeof(int));
    if (!remap) {
      return 0;
    }
    uint16_t write = 0;
    for (uint16_t i = 0; i < old_count; ++i) {
      ColonizeCol1Tribe* t = &col1->tribe[i];
      if ((int)t->nation_id == nation_id) {
        if (map) {
          map_set_owner_nibble(map, (int)t->x, (int)t->y, 0x0f);
        }
        remap[i] = -1;
        removed++;
        continue;
      }
      if (write != i) {
        col1->tribe[write] = *t;
      }
      remap[i] = (int)write;
      write++;
    }
    col1->head.tribe_count = write;

    /* Remap home_tribe_id for surviving units. */
    if (units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units->units[i];
        if (!u->active || u->home_tribe_id < 0) {
          continue;
        }
        if (u->home_tribe_id >= (int)old_count) {
          u->home_tribe_id = -1;
          continue;
        }
        u->home_tribe_id = remap[u->home_tribe_id];
      }
    }

    /* DS:0x54f6 attitude[euro] needs no remap of its own: it is field +10 of
     * the settlement record (tribe.alarm[4]), so it moved with the tribe
     * array compaction above. The parallel `indian_tension` array this used
     * to shift was a misdecode and is gone (2026-09-08). */
    free(remap);
  }

  /* Reset fixed indian[] slot (keep tech). */
  if (col1) {
    const int idx = nation_id - 4;
    ColonizeCol1Indian* ind = &col1->indian[idx];
    const uint8_t tech = ind->tech;
    memset(ind, 0, sizeof(*ind));
    ind->tech = tech;
    /*
     * The 15b3 matrix is symmetric storage in two different Linux fields —
     * nation[e].relation_by_indian[idx] is the Euro→tribe direction, and the
     * indian[idx].euro_diplo[e] the memset above just cleared is the other —
     * so a raw one-sided assignment here was the last write to it outside the
     * FUN_15b3_0066/00d0 pair helpers (audit #16 class). DOS never assigns
     * the byte: its two idioms are FUN_43f7_0108's `clear_both(0xb)` +
     * `or_both(0x60)` surrender (raw 73555-73557) and the new-game reset,
     * which zeroes the whole 12-wide row a column at a time
     * (`for (c = 0; c < 0xc; ++c) *(nation*0x13c + c - 0x77c4) = 0`, raw
     * 121620-121622). This site is the second: a nation that no longer
     * exists has no relation with anybody, in either direction. Routed
     * through ai_diplo_clear_both with a full mask so both halves fall
     * together and ai_diplo_write's dual-mode addressing (plus its
     * player.diplomacy mirror) is the only channel into the matrix.
     *
     * FUN_4d56_00e0, the per-village DOS razer this whole helper stands in
     * for, does not touch the matrix at all (raw 81292-81346) — it only ORs
     * the extinct bit 0x80 into indian[].+3 — which is why this is a Linux
     * invention routed to the DOS helper rather than a port of a DOS write.
     */
    for (int e = 0; e < 4; ++e) {
      ai_diplo_clear_both(col1, e, nation_id, 0xffu);
    }
  }

  return removed;
}

/*
 * New-game / load hook (sibling of ai_euro_reset / ai_goals_reset /
 * founding_fathers_reset / ai_contact_reset): zeroes this module's
 * per-unit/per-pulse statics that would otherwise leak across a new game
 * or Load in the same process. Restores each to its declaration-time
 * initializer (the dangling ctx pointer to NULL; everything else to 0,
 * matching how these arrays start at program launch).
 */
void ai_native_reset(void) {
  ai_s_seed100_init_pulse = 0;
  ai_s_seed100_midturn_turn = 0;
  ai_s_lcg_in_pick = 0;
  ai_s_lcg_pick_burns = 0;
  ai_s_lcg_total_nexts = 0;
  memset(s_brave_origin_x, 0, sizeof(s_brave_origin_x));
  memset(s_brave_origin_y, 0, sizeof(s_brave_origin_y));
  memset(s_brave_origin_ok, 0, sizeof(s_brave_origin_ok));
  s_ai_native_ctx = NULL;
  memset(s_ai_first_contact_this_turn, 0, sizeof(s_ai_first_contact_this_turn));
  memset(s_ai_visit_applied_this_turn, 0, sizeof(s_ai_visit_applied_this_turn));
}
