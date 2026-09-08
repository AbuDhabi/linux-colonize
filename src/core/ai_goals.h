#ifndef COLONIZE_AI_GOALS_H
#define COLONIZE_AI_GOALS_H

#include <stdint.h>

#include "core/colony.h"
#include "core/col1_save.h"
#include "core/map.h"

/*
 * Linux port of FUN_521d_0000…0906 goal tables (annotated euro_goals.c).
 * DOS *consumer* of `primary[]` — the code that reads these slots back to
 * assign unit orders/act-state/goal-target x-y — is `FUN_521d_0a60`'s tail
 * (mapped, not ported, 2026-08-14): see
 * original_sources_annotated/ai/euro_goal_orders_0a60_full.md.
 */

#define AI_GOAL_EMPTY 0xff
#define AI_GOAL_CONTACT 0
#define AI_GOAL_FOUND 1
#define AI_GOAL_LABOR 3
#define AI_GOAL_MILITARY 4
#define AI_GOAL_COLONY 5
/*
 * FUN_521d_20e6's explore memory (LAB_521d_2912). Only two DOS sites touch
 * code 6: the `0116` read at raw 89078 and the `001c` radius-0 invalidation at
 * raw 89081. The matching `0214` *writer* at raw 89278 is dead code — see the
 * comment at ai_euro_land_explore_scan_target's tail — so in DOS this table
 * never actually holds a code-6 slot and the read is a constant 0.
 */
#define AI_GOAL_EXPLORE 6
#define AI_GOAL_MIL_EXPAND 7
#define AI_GOAL_COLONY_ALT 8

#define AI_PRIMARY_SLOTS 64
#define AI_SECONDARY_SLOTS 16
#define AI_WORK_SLOTS 16 /* VICEROY_AI_WORK_QUEUE_SLOTS 0x10 */

typedef struct AiGoalSlot {
  int8_t x;
  int8_t y;
  uint8_t code; /* AI_GOAL_EMPTY or goal code */
  uint8_t prio;
} AiGoalSlot;

/*
 * DOS work-queue record (`DS:-0x5f24`, 16 x 6 bytes; FUN_521d_02be
 * `upsert_work_queue` / FUN_521d_031c `clear_work_queue`), decoded
 * 2026-09-06d from the raw `0a60` producer + the `4393` consumer tail
 * (euro_goal_orders_0a60_full.md / move_scoring_20e6_full.md):
 *   +0 int16  colony index (DOS 0xffff = free slot; -1 here)
 *   +2 int16  score        (price-weighted haul value, clamped 0x7fff)
 *   +4 uint8  loads        (was `flag_a`) -- hold-loads of haul work
 *   +5 uint8  military     (was `flag_b`) -- an exposed combat-capable land
 *                          unit sits on the colony tile on a stance-0
 *                          continent; the only slots a warship may serve
 * DOS reads `+4` (not `+5`) as the "slot still has work" gate, and the
 * `4393` tail writes both `+2` and `+4` back after a hauler claims part of
 * the work -- see ai_goals_work_consume().
 */
typedef struct AiWorkSlot {
  int16_t id;
  int16_t score;
  uint8_t loads;
  uint8_t military;
} AiWorkSlot;

typedef struct AiNationGoals {
  AiGoalSlot primary[AI_PRIMARY_SLOTS];
  AiGoalSlot secondary[AI_SECONDARY_SLOTS];
} AiNationGoals;

/* Per-nation shortage / demand scratch filled by dispatcher inventory. */
typedef struct AiEuroInventory {
  int tools_short;
  int lumber_short;
  int muskets_short;
  int food_short;
  int ore_short; /* 5cf6-shaped Ore tally for Expert Ore Miner dock hire */
  /* horses_short / found_flags / profession_demand[] were write-only after the
   * Linux-shaped 5d04 hire matrix was retired 2026-09-07e; their real DOS
   * replacements are ai_euro_5d04_cb_colonies_wanting_colonists (DS:0xa0b8)
   * and ai_euro_5d04_cb_cargo_demand (DS:0xa0cc). Removed 2026-09-07. */
  int colony_count;
  int urgency; /* founding_expansion_urgency stand-in */
} AiEuroInventory;

void ai_goals_reset(void);
void ai_goals_clear_primary_slot(int nation_id, int slot);
void ai_goals_clear_secondary_slots(int nation_id);
void ai_goals_promote_secondary_to_primary(int nation_id);
void ai_goals_upsert_primary(int nation_id, int x, int y, int code, int prio);
void ai_goals_upsert_secondary(int nation_id, int x, int y, int code, int prio);
void ai_goals_clear_work_queue(void);
void ai_goals_upsert_work(int id, int score, uint8_t loads, uint8_t military);
/*
 * FUN_521d_4393 tail (raw decomp, move_scoring_20e6_full.md ~2226-2242):
 * a hauler that commits to `slot` claims `free_holds` of its loads; the
 * remaining score is scaled by the surviving fraction and the slot is freed
 * outright once nothing is left.
 *   remaining = max(0, loads - free_holds)
 *   score     = remaining * score / loads
 *   loads     = remaining
 *   id        = -1 when remaining == 0        (DOS writes 0xffff)
 */
void ai_goals_work_consume(int slot, int free_holds);
int ai_goals_max_primary_prio(int nation_id, int x, int y, int code);
const AiGoalSlot* ai_goals_primary(int nation_id, int slot);
const AiWorkSlot* ai_goals_work(int slot);
int ai_goals_best_found_tile(int nation_id, int* out_x, int* out_y);
/*
 * Nearest free water/high-seas tile within `max_radius` of (from_x, from_y)
 * that has a foundable land neighbour -- the map-agnostic landing target for a
 * loaded transport with no landfall of its own. Ties break westward.
 */
int ai_goals_nearest_landing_water(
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  int from_x,
  int from_y,
  int max_radius,
  int* out_x,
  int* out_y
);
/* Same, but tie-broken by distance from (from_x, from_y) and preferring
 * the landmass that point sits on. Pass from_x < 0 for the plain scan. */
int ai_goals_best_found_tile_near(
  const ColonizeWorldMap* map,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
);

/*
 * FUN_521d_001c — invalidate_nearby_secondary_goals.
 * Clear any nation secondary slot matching `code` within `radius` of (x,y).
 * Distance callee resolved 2026-09-07 (FUNCTION_CATALOG row 1278): DOS
 * FUN_281f_037a is a far thunk to FUN_124c_007c, which takes |dx|,|dy| and
 * tail-calls FUN_124c_0040 (viceroy_unpacked.c:2759-2792) — the exact
 * `min>>1 + max` metric this port already used. The catalog's "Manhattan"
 * blurb is wrong; the body is not |dx|+|dy|. No longer a stand-in.
 */
void ai_goals_invalidate_nearby_secondary(int nation_id, int code, int x, int y, int radius);

/*
 * Per-nation AI-planning scratch read by FUN_521d_03d0 / 052c.
 *
 * **All six DS fields resolved and wired 2026-09-07** (they used to sit
 * all-zero, which pinned FUN_521d_03d0 on its `return 8` early-out
 * forever). Writers traced in the raw decompile:
 *
 *   -0x6d68 = DS:0x9298  colony_counts[nation]        FUN_4962_0018 phase 3
 *                        (viceroy_unpacked.c:78249 `0x9298++` per own colony)
 *   -0x5f48 = DS:0xa0b8  own colonies whose +0x1b has bit 0x10 set
 *                        (COLONIZE_COLONY_AI_NEEDS_COLONISTS); zeroed and
 *                        recounted in FUN_521d_6d8e's own prelude
 *                        (viceroy_unpacked.c:93109 / 93139-93141)
 *   -0x6bf0 = DS:0x9410  census_pop_proxy[nation]     FUN_4962_0018
 *   -0x6bec = DS:0x9414  ship_cargo_totals[nation]    FUN_4962_0018
 *   -0x6a99 = DS:0x9567  leader trait column 1 (stride-3 @LEADERNAME
 *                        triple; ai_diplo_leader_trait(ctx, n, 1))
 *   -0x6bb2 = DS:0x944e  avg_colony_pop[nation] — FUN_4962_0018 accumulates
 *                        Σ colony pop there and divides by 0x9298 at its
 *                        tail (viceroy_unpacked.c:78323-78326)
 *
 * The first four and the sixth are already kept live per turn by
 * col1_stuff_census_refresh_colony_counts(); ai_goals_plan_scratch_refresh()
 * copies them plus the two that have no census home.
 *
 * last_colony_founded_turn (DS nation*0x13c-0x77b2) identity confirmed
 * 2026-08-19 by decompiling thunk_FUN_2a1f_0494's real overlay target
 * (OVL14_L0000:72e0 = FUN_521d_03d0, this file's own
 * ai_goals_founding_expansion_urgency) directly: it is NOT an at-war
 * flag (the earlier guessed name) — 052c's own gate calls
 * founding_expansion_urgency(nation) a second time purely as a nonzero
 * check, and -0x77b2's one writer is `FUN_479b_076e` (case-7 FOUND
 * COLONY handler, `viceroy_unpacked.c:77006`), which stamps the current
 * turn counter (DS:0x538e) into it right before the colony-record init
 * call. So the term 052c adds is "(turns since this nation last founded
 * a colony) >> 4" gated on nonzero expansion urgency, not a war-timing
 * bonus. Set via `ai_goals_note_colony_founded`.
 */
typedef struct AiNationPlanScratch {
  uint8_t colony_count;          /* DS nation-0x6d68 = 0x9298 */
  uint8_t colonies_wanting_colonists; /* DS nation-0x5f48 = 0xa0b8 */
  uint8_t census_pop;            /* DS nation-0x6bf0 = 0x9410 */
  uint8_t ship_cargo_total;      /* DS nation-0x6bec = 0x9414 */
  int8_t leader_trait1;          /* DS nation*3-0x6a99 = 0x9567 (stride 3) */
  int avg_colony_pop;            /* DS nation*2-0x6bb2 = 0x944e (stride 2) */
  int last_colony_founded_turn;  /* DS nation*0x13c-0x77b2; 0 = never founded, matches DOS's zero-init */
} AiNationPlanScratch;

AiNationPlanScratch* ai_goals_plan_scratch(int nation_id);

/*
 * FUN_521d_6d8e prelude + the FUN_4962_0018 census tables it reads: refill
 * one nation's plan scratch from live state. `leader_trait1` is DS:0x9567
 * (ai_diplo_leader_trait(ctx, nation_id, 1)); the caller supplies it so
 * ai_goals stays free of the turn-context/diplo headers.
 */
void ai_goals_plan_scratch_refresh(
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int leader_trait1
);

/* FUN_479b_076e's `-0x77b2` stamp: call when nation_id founds a colony. */
void ai_goals_note_colony_founded(int nation_id, int turn);

/*
 * FUN_521d_03d0 — founding_expansion_urgency(nation, total_colony_count).
 * total_colony_count is DOS global colony_count (DS:0x539e), caller-supplied.
 */
int ai_goals_founding_expansion_urgency(int nation_id, int total_colony_count);

/*
 * FUN_521d_052c — unit_desirability_score.
 * unit_type/profession use the DOS raw byte codes (unit +0x2/+0x17).
 *
 * `dist_to_bound_colony` used to be a caller-supplied DS:0x8db8 snapshot.
 * It is not one: DOS's `FUN_281f_0614` is a far thunk to FUN_15eb_0142
 * (viceroy_unpacked.c:9380-9424), the nearest-colony search itself, and
 * *that* is what stores the winning distance into DS:0x8db8 — the read one
 * line later in 052c consumes its own call's output. So the distance is
 * computed here (nearest own colony **on `continent_id`**), and the param
 * is gone. `map` is needed for the per-colony continent filter.
 */
int ai_goals_unit_desirability_score(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int unit_x,
  int unit_y,
  int unit_type,
  int unit_profession,
  int continent_id,
  int turn,
  int total_colony_count
);

/*
 * FUN_521d_0600 — composite_unit_priority = 052c + 0492 + 03d0, floor 0.
 * Thunk identities resolved via arg-shape match (2a1f_053c=052c,
 * 2a1f_04d0=0492, 2a1f_0494=03d0); see docs/port_plan.md.
 */
int ai_goals_composite_unit_priority(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int unit_x,
  int unit_y,
  int unit_type,
  int unit_profession,
  int turn,
  int total_colony_count
);

/*
 * FUN_521d_0656 — stack_settler_pick (was "walk_unit_stack_to_end", a
 * misreading of the register-mangled canonical decompile; re-disassembled
 * byte-exact from OVL14_L0000:0656 on 2026-09-06e). Walks the tile/transport
 * chain from `unit_index` and returns the member with the HIGHEST unit type
 * byte whose DS:0x523d capability record has bit 0x40 set (types 0 Colonist,
 * 2 Pioneer, 5 Scout — the settler-capable set); strictly-greater replaces,
 * so the first of equal types wins. -1 when the chain holds none, which is
 * the meaningful "no settler aboard" gate 20e6's cargo goal fold reads.
 */
int ai_goals_stack_settler_pick(
  const ColonizeCol1Unit* units,
  int unit_count,
  int unit_index
);

/*
 * FUN_521d_0896 — the Indian **hostility gate** (name kept for catalog /
 * SYMBOL_MAP continuity; "filter_profession_by_distance_wealth" was an
 * inferred label and both halves of it are wrong). `profession` here is
 * DOS's overloaded "owner id" (0..3 nation, >=4 Indian nation id) — for
 * owner ids 0..3 the >3 gate never fires so the value passes through
 * unchanged (see FUN_521d_0906).
 *
 * Both Indian-range reads wired 2026-09-08 (decomp 87319-87340):
 *   alarm  = FUN_281f_030c(owner - 4, nation)  = ai_diplo_indian_alarm
 *   gate   = alarm > 0x4a
 *   if (unit_index >= 0 && DS:0x54f6[unit.home_tribe_id][nation] > 0x7f)
 *            gate = true
 *   return gate ? owner : -1
 * i.e. an adjacent native raises a contact claim only when that nation is
 * already hostile (alarm > 74) or the specific village carries a grudge
 * (tension > 127). `col1` may be NULL — both reads then answer 0, the
 * pre-2026-09-08 parked behaviour.
 */
int ai_goals_filter_profession_by_distance_wealth(
  const ColonizeCol1Save* col1,
  const struct ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int has_context,
  int unit_index
);

/*
 * FUN_521d_0906 — probe_adjacent_contact_claim. Scans the 8 neighbours that
 * share (x,y)'s water/high-seas class and stops at the first foreign claim.
 * Two independent probes per tile, both filtered through 0896:
 *   FUN_281f_0682 = layer2 bit0, a foreign **unit** stands there. On water
 *                   the claim only survives if that stack holds an armed
 *                   ship (type 0x0d..0x12, combat byte != 0).
 *   FUN_281f_06be = layer2 bit1, a foreign **settlement** stands there. Its
 *                   result also lands in *out_side_claim (DOS DS:0x9ea8),
 *                   first value < 4 only.
 * Both were mis-wired before 2026-09-07 (the colony pool answered the unit
 * probe, and the settlement probe was hardwired off); see ai_goals.c.
 * 0896's tribe arm is live since 2026-09-08 (alarm + DS:0x54f6 tension);
 * `col1` feeds it and may be NULL.
 */
int ai_goals_probe_adjacent_contact_claim(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const struct ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  int x,
  int y,
  int nation_id,
  int profession,
  int* out_side_claim
);

AiEuroInventory* ai_goals_inventory(int nation_id);
void ai_goals_inventory_clear(int nation_id);

/*
 * FUN_521d_0492 — colony_count_balance_flags(nation, continent).
 * Live nation×continent colony counts + post_map.continent_tally_b/12 target.
 * Cite: viceroy_unpacked.c ~87098; DS:0x85c8 / 0x947e / 0x94e6.
 */
int ai_goals_colony_balance_flags(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int continent_id
);

/*
 * FUN_521d_06ae — pick best adjacent founding tile.
 * score_extras: DOS param_4 (neighbor continent/explore extras).
 * wagon_filter: DOS param_5 (1 when unit type is wagon 0x0b).
 * col1: optional (tally_b target); NULL → 0492 returns 0.
 * Returns 1 and writes out_x/out_y, or 0 if none (DOS returns dir 8 = stay;
 * see the body for why the port keeps a failure signal).
 */
struct ColonizeUnitPool;
int ai_goals_pick_founding_tile_ex(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  const struct ColonizeUnitPool* units, /* optional: DOS 06ae occupant rule */
  int nation_id,
  int x,
  int y,
  int score_extras,
  int wagon_filter,
  int* out_x,
  int* out_y
);

/*
 * FUN_521d_06ae with score_extras=1, wagon_filter=0 (0a60 FOUND writer default).
 */
int ai_goals_pick_founding_tile(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int x,
  int y,
  int* out_x,
  int* out_y
);

#endif
