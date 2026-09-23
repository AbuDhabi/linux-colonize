#ifndef COLONIZE_COLONY_H
#define COLONIZE_COLONY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/map.h"
#include "core/ss.h"
#include "core/world.h"

/* Forward declaration to avoid pulling in font headers. */
typedef struct ColonizeFont ColonizeFont;
typedef struct ColonizeCol1Save ColonizeCol1Save;
typedef struct ColonizeCol1TradeStop ColonizeCol1TradeStop;
typedef struct ColonizeUnitPool ColonizeUnitPool;
typedef struct AiPopupState AiPopupState;

/* DOS founding gate is `colony_count < 0x30` (viceroy_unpacked.c:58029). */
#define COLONIZE_COLONIES_MAX 48
#define COLONIZE_COLONY_NAME_MAX 28
#define COLONIZE_COLONY_NAMES_MAX 80
#define COLONIZE_BUILDING_TYPES_MAX 48
#define COLONIZE_COLONY_POP_MAX 32
/*
 * The work-plot ring. DOS sizes it per colony: `DS:0x329[FUN_15eb_0470()]`
 * over the table {0,4,8,12,20} (VICEROY.EXE file offset 121248+0x329), with
 * `FUN_15eb_0470` (raw 9636-9645) = `min(FUN_15eb_039e(10),2)+2`. `039e(10)`
 * (raw 9561-9578) walks the @BUILDING chain that starts at row 10 through the
 * `-0x707a` next-row byte and counts the rows the colony owns, i.e. the two
 * Town Hall upgrade rows 0x0a/0x0b (NAMES.TXT:177-178). DOS's own "can build"
 * gate FUN_15eb_3650 (raw ~13674) hard-zeroes both rows, so no colony stock
 * DOS can produce ever leaves tier 2 and the usual ring is the 8 adjacent
 * plots -- but a save may carry the bits (colony buildings mask bits 9-11 are
 * @BUILDING rows 9/10/11), so the ring is computed per colony from
 * `colonies_work_plot_count()` and the array is sized for all 20 slots.
 * bugs.md #593.
 */
#define COLONIZE_COLONY_FIELD_TILES 8
#define COLONIZE_COLONY_FIELD_TILES_MAX 20

/* NAMES.TXT @JOB field jobs (Farmer … Fisherman). */
#define COLONIZE_JOB_FARMER 0
#define COLONIZE_JOB_SUGAR_PLANTER 1
#define COLONIZE_JOB_TOBACCO_PLANTER 2
#define COLONIZE_JOB_COTTON_PLANTER 3
#define COLONIZE_JOB_FUR_TRAPPER 4
#define COLONIZE_JOB_LUMBERJACK 5
#define COLONIZE_JOB_ORE_MINER 6
#define COLONIZE_JOB_SILVER_MINER 7
#define COLONIZE_JOB_FISHERMAN 8
#define COLONIZE_FIELD_JOB_COUNT 9
/* @JOB occupation 0x12, the colony's teaching slot DOS's work-assign
 * validator counts (FUN_1000_8d72(0x12)). Not the cut Expert Teacher unit. */
#define COLONIES_JOB_TEACHER 18

/* Warehouse cargo order matches NAMES.TXT @CARGO (and ICONS.SS 22..37). */
#define COLONIZE_CARGO_FOOD 0
#define COLONIZE_CARGO_SUGAR 1
#define COLONIZE_CARGO_TOBACCO 2
#define COLONIZE_CARGO_COTTON 3
#define COLONIZE_CARGO_FURS 4
#define COLONIZE_CARGO_LUMBER 5
#define COLONIZE_CARGO_ORE 6
#define COLONIZE_CARGO_SILVER 7
#define COLONIZE_CARGO_HORSES 8
#define COLONIZE_CARGO_RUM 9
#define COLONIZE_CARGO_CIGARS 10
#define COLONIZE_CARGO_CLOTH 11
#define COLONIZE_CARGO_COATS 12
#define COLONIZE_CARGO_TRADE_GOODS 13
#define COLONIZE_CARGO_TOOLS 14
#define COLONIZE_CARGO_MUSKETS 15
#define COLONIZE_CARGO_COUNT 16

typedef struct ColonizeBuildingType {
  char name[40];
  /* NAMES.TXT @BUILDING row + 1 (== ColonizeBuildingRow + 1); 0 = not
   * stamped. The row identifies a building; the name is display text only. */
  int row_plus1;
  int hammers;
  int tools_cost;
  int min_population;
  /* NAMES.TXT @BUILDING column 4 ("size") — DOS's settlement-view size class
   * 0..4, indexing the per-class box tables at DS:0x230 (width) / DS:0x236
   * (height) that FUN_2f2b_14d4 centers level badges in. */
  int size_class;
} ColonizeBuildingType;

/* One person living in a colony (disbanded map unit). */
typedef struct ColonizeColonist {
  int unit_type_index; /* into ColonizeUnitPool types (usually Colonists while working) */
  int profession;      /* NAMES.TXT @JOB skill; UNITS_JOB_NONE if none */
  int building_type;   /* workplace @BUILDING index, or -1 */
  int field_job;       /* @JOB field index 0..8, or -1 */
  bool active;
  /*
   * FUN_364b_0688 education: turns in current workplace. This IS the DOS
   * colony record's per-colonist nibble at +0x60 (`FUN_15eb_0c7a` read /
   * `FUN_15eb_0cbc` write, raw 10202-10240) — reached as `FUN_281f_0d1c` /
   * `FUN_281f_0a7e`. The teaching loop (raw 57503-57539) reads it, adds 1
   * and writes it back for EVERY colonist each turn, and the writer clamps
   * at 15, so ordinary long-serving workers saturate at 15 — exactly what
   * DOS campaign saves show. `FUN_15eb_1068` (raw 11256-11258) zeroes it on
   * a real job change. Persisted through col1_bridge (permuted with the
   * canonical colonist reorder, DOS raw 47225/47241).
   */
  uint8_t turns_in_job;
} ColonizeColonist;

typedef struct ColonizeColony {
  int id;
  char name[COLONIZE_COLONY_NAME_MAX];
  int x;
  int y;
  int nation_id; /* 0..3 European owner */
  int population; /* == colonist_count while active */
  bool active;
  ColonizeColonist colonists[COLONIZE_COLONY_POP_MAX];
  int colonist_count;
  bool has_building[COLONIZE_BUILDING_TYPES_MAX];
  /*
   * Surrounding field slots: colonist index or -1. Slots 0..7 are the port's
   * clockwise ring N,NE,E,SE,S,SW,W,NW; slots 8..19 are the DOS outer ring in
   * DS:0xc8/0xde order, (0,-2),(2,0),(0,2),(-2,0),(-1,-2),(1,-2),(-1,2),
   * (1,2),(-2,-1),(-2,1),(2,-1),(2,1). Only the first
   * colonies_work_plot_count() plots are workable; the rest stay -1 and
   * round-trip into the save's 20-slot colony+0x70 array verbatim.
   */
  int8_t tiles[COLONIZE_COLONY_FIELD_TILES_MAX];
  /* Warehouse + build queue — production ticks in src/core/turn.c. */
  int stock[COLONIZE_CARGO_COUNT];
  int hammers;
  int building_in_production; /* @BUILDING index, or -1 */
  /*
   * Custom House per-cargo enable mask (ColonizeCol1CustomHouse bit layout:
   * bit0=Food … bit15=Muskets). Default is 0x1ede (excludes food, horses,
   * lumber, tools, trade goods, muskets).
   */
#define COLONIZE_CUSTOM_HOUSE_DEFAULT_MASK 0x1edeu
  uint16_t custom_house_bits;
  /*
   * Col1 +0x8e LABOR demand counter. Unload/join decrements (FUN_521d_5b66
   * ~91589). Stamped unconditionally by every AI colony tick from the real
   * FUN_5952_035e formula since 2026-09-09 (raw 94029-94045, ported in
   * ai_euro_colony_threat_seed_5952): it is a target headcount
   * `clamp(max((pop + on_tile_colonists - 1) / 2, garrison_quota), <= n / 2)`
   * (+1 under WoI, floored at 1 by an adjacent enemy), NOT a boolean
   * "this colony wants labor" — a hand-seeded value does not survive a tick.
   * Cite: save_format_map.md; euro_unit_act case 0x0b.
   */
  uint8_t labor_shortage;
  /*
   * Col1 +0x1e garrison fortify quota. DEC on fortify/'A' assign; seeded by
   * threat>>3 (FUN_5952_035e, ai_euro_colony_threat_seed_5952).
   * Cite: save_format_map.md; euro_unit_act §2d3.
   */
  uint8_t garrison_quota;
  /*
   * Col1 +0xba / +0xbe fog-of-war snapshots per European viewer (smcol
   * population_on_map / fortification_on_map). Written by the tile-reveal
   * writer FUN_364b_1b4c whenever a nation's sight touches this tile, and
   * seeded to pop=1/fort=0 by FUN_13f1_00a6 (±5 reveal on founding). pop 0 =
   * that nation has never observed the colony (FUN_364b_1b76 gate).
   */
  uint8_t pop_on_map[4];
  uint8_t fort_on_map[4];
  /*
   * Col1 +0x8d specialty cargo index (`0xff` = none). FUN_5952_0306 set/clear
   * from warehouse stock vs capacity + boycott. Haul prefers this cargo.
   */
  uint8_t specialty_cargo;
  /*
   * Col1 +0x8f cargo-idle turns. INC cap 0x7f each Euro inventory pulse
   * (FUN_5952_035e); cleared when goods unload into the colony. Haul target
   * score adds idle*8. Cite: save_format_map.md; viceroy ~87677 / ~90249.
   */
  uint8_t cargo_idle_turns;
  /*
   * Col1 +0x8c improve timer. INC cap 0x7f (FUN_5952_035e); gates AI pioneer
   * plow/road until timer ≥ thin threshold (terr_cost+2 stand-in); cleared on
   * successful improve. Cite: save_format_map.md; FUN_5952 ~93663 / ~94546.
   */
  uint8_t improve_timer;
  /*
   * Col1 +0x1d build AI flags. Bit7 (0x80) = wants_construction (FUN_5952).
   * Latches construction LABOR when set (Col1 import or Linux construction).
   */
  uint8_t build_ai_flags;
  /*
   * Col1 +0x1b AI planner flags (FUN_4962_0018 / FUN_5952_035e). Ship-pressure
   * bits 0x01/0x02 drive COLONY goal 5|8; other bits thin-latched.
   */
  uint8_t ai_flags;
  /*
   * Col1 +0x1c colony flags (FUN_364b_0688). Starvation 0x08 latches LABOR;
   * wagon_train / coastal thin-latched.
   */
  uint8_t colony_flags;
  /*
   * DOS DS:0x34a: the building that just finished construction here, stored
   * as index+1 so a zeroed colony means "nothing to reveal". FUN_2f2b_6cd4
   * (colony-screen bring-up) only runs the "new building appears" reveal —
   * clear bit, redraw, set bit, redraw — and only then pushes event 0x54
   * (COLDIG 13 hammering + cheering), when DS:0x34a >= 0. Set by
   * colonies_try_complete_building, consumed on colony-screen open.
   */
  int pending_build_reveal;
  /*
   * Col1 +0x90 cargo-produced mask (bit per cargo). Cleared then OR'd during
   * colony EOT production (FUN_364b_0688). Haul prefers produced surplus.
   */
  uint16_t cargo_produced_mask;
  /*
   * Col1 +0x98 hammers purchased via BUY (FUN_2f2b_5e44). Accumulates remainder
   * hammers paid with gold.
   */
  uint16_t hammers_purchased;
  /*
   * Col1 +0x97 depletion counter. INC on ore/silver field work; wrap at 50
   * triggers MAP_LAYER2_SUPPRESS on the worked tile (FUN_364b_033a feature 4).
   */
  uint8_t depletion_counter;
  /*
   * Col1 +0x95 warehouse level. Capacity = 100*(1+level) (FUN_15eb_0a50).
   * Also derived from Warehouse / Warehouse Expansion when those are built.
   */
  uint8_t warehouse_level;
  /*
   * Col1 +0x96 capitol level. INC on Capitol / Capitol Expansion complete
   * (FUN_364b_0114). Bridged; deeper use stays thin.
   */
  uint8_t capitol_level;
  /*
   * Port-only within-turn food latch: set by the production tick when this
   * colony actually went negative on food, read by the same tick's starve
   * kill and by the @FOOD1/@FOOD2 "depleted" nag suppressor. It used to ride
   * in colony_flags bit3, but DOS spends that bit on the inefficient-
   * government latch (FUN_364b_0688 phase D, +0x1c & 8) — see
   * COLONIZE_COLONY_FLAG_INEFFICIENT_GOV — so it lives here instead. Not
   * bridged: it carries no state DOS keeps.
   */
  uint8_t food_shortfall_latch;
  /*
   * Port-only Phase A compose snapshot (smell audit #62). DOS composes this
   * colony's bells and crosses inside FUN_364b_0688's own prologue
   * (`FUN_281f_0c22` → `15eb_1f72`) and hands the SAME word to both
   * consumers — the nation/Congress tally (`FUN_291f_09f8`, viceroy 57231)
   * and the rebel dividend (57392). The port instead re-tallies bells and
   * crosses for the whole nation in `turn_run_nation_ticks`, which for AI
   * nations runs AFTER their colonies have ticked, i.e. after Phase C/D
   * moved the SoL accumulators and latch bits and after F/G/H/J rewrote the
   * roster — a systematically one-step-ahead number. `turn_produce_one_colony`
   * stamps what it composed at the Phase A boundary here; the nation tally
   * prefers it when `prod_compose_stamp` matches this turn, and otherwise
   * (human colonies, whose EOT runs later in TURN_PROC_FINISH; direct
   * callers with no col1) falls back to a live read, which for them already
   * IS the pre-tick state. Not bridged: DOS keeps no such field.
   */
  int prod_bells_phase_a;
  int prod_crosses_phase_a;
  uint32_t prod_compose_stamp; /* head.turn + 1; 0 = never composed */
} ColonizeColony;

#define COLONIZE_BUILD_AI_WANTS_CONSTRUCTION 0x80u
/*
 * DOS colony +0x1b bits 0/1 — FUN_4962_0018 census phase 3 (11×11 box scan
 * around each own colony): a foreign ship (type 0x0d..0x12, combat byte != 0)
 * with a short navigable route (FUN_6662_0906 sea flood cost 0..5) sets bit
 * 0x02 when it is a Frigate (type == 0x11, literal check) and bit 0x01
 * otherwise. Earlier port naming had bit 0x02 as "Man-O-War" — wrong; census
 * trace 2026-08-19 (census_tally.md) pinned the 0x11 literal.
 */
#define COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP 0x01u
#define COLONIZE_COLONY_AI_NEARBY_FRIGATE 0x02u
/*
 * DOS colony +0x1b bits 0x04 / 0x08 — the defender-count pair, both written by
 * FUN_5952_035e (viceroy_unpacked.c 94195-94199) from the same comparison:
 *   local_74 = wanted defenders, local_82 = defenders present
 *   local_82 < local_74                            -> set 0x08 (short of want)
 *   local_74 + (local_74 > 1) < local_82           -> set 0x04 (surplus)
 * So 0x08 is the "short of defenders" side and 0x04 the "has spare military"
 * side. (0x04 was named NEEDS_MILITARY until 2026-09-16 — inverted.) Two Linux readers
 * now (both DOS's own, see the consumer audit below):
 * ai_euro_20e6_surplus_recall_arm (the read-and-clear recall) and
 * ai_euro_20e6_wander_step's flag ladder. Grep the macro rather than trusting
 * a line number here — the previous "its one read site (ai_euro.c ~12103)"
 * was wrong on both counts by the time it was read. Smell audit
 * 2026-09-10 D9.
 *
 * Both writers ARE ported since 2026-09-09 (smell audit #41), in
 * ai_euro_colony_threat_seed_5952 — the same DOS body that already produced
 * garrison_quota. `local_74` (wanted defenders) is raw 94150-94193;
 * `local_82` is NOT a raw defender count but this nation's LAND military
 * homed to the colony (+0x314a origin) minus the ones the on-tile walk
 * already counted as garrison, i.e. the OFF-STATION surplus (raw 94063-94071).
 *
 * The DOS per-tick clear is `+0x1b &= 7` (raw 94142) — it keeps 0x01/0x02
 * (the census's own disjoint mask) AND 0x04. 0x04 is therefore sticky by
 * design: only its consumers clear it.
 *
 * CONSUMER AUDIT, 2026-09-09 (all three read sites, viceroy_unpacked.c):
 *   - raw 85332 and raw 90168 are the SAME arm, emitted twice by the
 *     decompiler (both `goto LAB_521d_27f5`): FUN_521d_20e6's land-unit act.
 *     A non-ship unit with attack > 1 and a bound home colony (+0x314a >= 0)
 *     binds that colony; if it has 0x04 set, and (garrison_quota != 0 ||
 *     unit type != 4), and the colony is on the unit's own continent
 *     (FUN_281f_0722 == uStack_38), DOS clears 0x04, decrements
 *     garrison_quota (+0x1e) and commits the walk home. PORTED 2026-09-09 as
 *     ai_euro_20e6_surplus_recall_arm — one surplus unit recalled per tick,
 *     which is what the single-shot clear buys.
 *   - raw 94247: FUN_5952_035e's join-colonist loop (units standing on the
 *     colony tile fold into the population). 0x04 is one of three disjuncts
 *     that admit a Soldier/Dragoon; the arm then clears it. PORTED
 *     2026-09-18 in ai_euro_act_colony_absorb (the port runs the case from
 *     the arriving unit's act instead of DOS's colony-side tile pass).
 *     local_42/local_3e are NOT independent locals — aiStack_68 is memset
 *     for 0x32 bytes, so they are aiStack_68[19] and aiStack_68[21] of the
 *     per-@JOB head-count buckets (non-experts bucket as 0x13), i.e. "plain
 *     colonists here" and "@JOB 0x15 Soldiers here"; confirmed 2026-09-18
 *     against the OVL15 disassembly ([BP-0x40]/[BP-0x3c] off the array base
 *     [BP-0x66]). The third disjunct's `labor_shortage < 0` test is dead in
 *     DOS. Full decode in docs/smell_audit_2026-09-10.md, "FUN_5952_035e
 *     building / expert passes", and colony_tick_5952_035e.md's frame rule.
 */
#define COLONIZE_COLONY_AI_MILITARY_SURPLUS 0x04u
#define COLONIZE_COLONY_AI_SHORT_DEFENDERS 0x08u
#define COLONIZE_COLONY_AI_NEEDS_COLONISTS 0x10u
/*
 * DOS colony +0x1b bit 0x20 — "send a Pioneer to CLEAR this colony's ring",
 * distinct from 0x80's road/plow errand. FUN_5952_035e writes the PAIR
 * `|= 0xa0` (0x80|0x20) at raw 94200-94206, from its full-ring scan
 * (raw 94082-94117), in two cases:
 *   1. `ring_tiles - 1 <= unproductive` && `forest_tiles > 1` — the ring is
 *      essentially all forest/water/poor ground;
 *   2. `good_food_tiles < (pop + 3) >> 2` && `clearable_forest != 0` &&
 *      `forest_tiles > 1` — too few open food tiles for the head count, with
 *      at least one forest worth clearing.
 * "unproductive" (local_144) = off-map + water + forest + cleared ground
 * whose DS:0x2f7b food byte is < 2; "good_food" (local_e) = water +
 * cleared ground with food >= 3; "clearable" (local_c) = forest whose
 * CLEARED counterpart (class & 7) has food > 2.
 *
 * The writer is ported (ai_euro_refresh_colony_ai_flags). DOS's four readers
 * are all inside FUN_5952_035e's later building/expert passes (raw 94422,
 * 94454, 94499, 94751). Raw 94751 IS ported since 2026-09-10 — the
 * field-specialist restore pass at the tail of
 * ai_euro_colony_tick_28c8_reassign, where this bit (or a live lumber
 * shortfall, DS:0x8e64) is what lets a colony take a SECOND Expert
 * Lumberjack onto its ring. The other three live in the colony-tick
 * pioneer-improve pass (raw 94330-94560), which spawns a phantom worker and
 * has no Linux counterpart; see docs/smell_audit_2026-09-10.md, the
 * "FUN_5952_035e building / expert passes" section.
 */
#define COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR 0x20u
#define COLONIZE_COLONY_AI_NEEDS_GARRISON 0x40u
/*
 * DOS colony +0x1b bit 0x80 — "worked surround tiles want Pioneer work":
 * FUN_5952_035e sets it when any worked ring tile lacks road (fa_flags &
 * 0x0a == 0) or any worked farmland tile (terr class < 8) lacks plow
 * (& 0x40). FUN_521d_0a60's +800 registration arm counts an idle Pioneer at
 * the colony only when this is CLEAR (no work here → ship him out).
 */
#define COLONIZE_COLONY_AI_WANTS_PIONEER_WORK 0x80u
#define COLONIZE_COLONY_FLAG_SOL_100 0x02u
#define COLONIZE_COLONY_FLAG_SOL_50 0x04u
/*
 * DOS +0x1c bit3 (FUN_364b_0688 phase D, ~57470): the inefficient-government
 * latch. Set the turn a colony's Tory head count reaches 10-difficulty and
 * cleared when it drops back under, so @INEFFICIENT / @EFFICIENT each fire
 * once per crossing — and, because it is a save field, they stay fired across
 * a save/load. The port used to call this bit "starvation" and keep the real
 * latch in RAM only, which is why the popup came back on every reload
 * (bugs.md).
 */
#define COLONIZE_COLONY_FLAG_INEFFICIENT_GOV 0x08u
/*
 * DOS +0x1c bit 0x10 (col1_save.h `small_colony_ai`) — a save-borne one-shot,
 * READ-ONLY in the port. Both DOS sites are in FUN_5952_035e: the
 * read-and-clear hand-off at raw 94143-94146 (`(+0x1c & 0x10) && pop < 0x20
 * -> +0x1b |= 0x10 (NEEDS_COLONISTS); +0x1c &= 0xef`), which is ported in
 * ai_euro_colony_threat_seed_5952 and consumed at ai_euro.c:10076, and the
 * writer at raw 95845-95847 (`pop < 10 -> +0x1c |= 0x10`), which is NOT
 * ported — so in the port the bit only ever arrives from a DOS or campaign
 * save and is then consumed once. Do not re-add an unconditional `pop < 10`
 * stamp: that pinned the flag on every turn (smell audit C3).
 * The "chain of 2a1f_05b4 probability gates" this note used to name is a
 * misreading corrected 2026-09-10: thunk_FUN_2a1f_05b4 is the RTLink stub for
 * FUN_5952_0214, the tick's build-candidate helper (`try_build(id)`, id in AX,
 * returns 0 when it picked), not an RNG roll. The writer sits at the bottom of
 * that helper's 24-candidate cascade — asm LAB_OVL15_L0000__00270d — so it is
 * reachable only once the whole cascade is ported. Every dropped id is
 * recovered in docs/smell_audit_2026-09-10.md (seventh-wave lead 5).
 *
 * DOS +0x1c bit 0x20 — WRITE-ONLY here, on purpose. DOS derives it in the
 * FUN_521d_6d8e prelude: raw 93142 clears it on each own colony, then the unit
 * loop at raw 93148-93157 sets it on the colony a Wagon Train (type 0x0c) is
 * HOMED to (+0x314a origin), not the one it is standing on. Its only DOS
 * reader is the unported expansion gate at raw 95762 (`(+0x1c & 0x20) != 0 ||
 * turn > 0x63f || ...`), so the port re-derives the bit every AI turn in
 * ai_euro_refresh_colony_ai_flags purely to keep the save field honest; DOS
 * is the only consumer. Documented debt, not dead code: wiring raw 95762 is
 * what would give it a Linux reader.
 */
#define COLONIZE_COLONY_FLAG_SMALL_AI 0x10u
#define COLONIZE_COLONY_FLAG_WAGON_TRAIN 0x20u
/*
 * DOS +0x1c bit 0x40 — the coastal bit. Written exactly once, at founding,
 * by FUN_364b_1ba8 (raw 58105-58110) from map_tile_is_open_sea_adjacent()'s
 * predicate: an inset 8-neighbour is ocean/high-seas AND the lowest-region
 * such neighbour is water region 1 (the open sea), so lake-only and map-edge
 * sites do not qualify. Nothing in the image recomputes or clears it (the
 * tick's flag-byte clear at raw 94145 is `&= 0xef`, bit 0x10 only), so after
 * founding it is pure save-carried state. Port writers: colonies_found
 * (the stamp) and a set-only self-heal in ai_euro_refresh_colony_ai_flags —
 * never a per-turn recompute that can clear it. DOS readers include the Docks
 * buildability filter (raw 13688, building id 7) and the 20e6 delivery pick
 * (raw 2054). Smell audit 2026-09-10 D4.
 */
#define COLONIZE_COLONY_FLAG_COASTAL 0x40u
#define COLONIZE_COLONY_FLAG_BUILD_COMPLETE 0x80u

typedef struct ColonizeColonyPool {
  ColonizeColony colonies[COLONIZE_COLONIES_MAX];
  int colony_count;
  int next_id;
  /* Per European nation (0..3) from COLONY.TXT @ENGLISH/@FRENCH/@SPANISH/@DUTCH. */
  char names[4][COLONIZE_COLONY_NAMES_MAX][COLONIZE_COLONY_NAME_MAX];
  int name_count[4];
  int name_next[4];
  ColonizeBuildingType building_types[COLONIZE_BUILDING_TYPES_MAX];
  int building_type_count;
} ColonizeColonyPool;

void colonies_init(ColonizeColonyPool* pool);
/* Load per-nation colony names from COLONY.TXT (@ENGLISH/@FRENCH/@SPANISH/@DUTCH). */
bool colonies_load_names(ColonizeColonyPool* pool, const char* colony_txt_path);
/* Load @BUILDING definitions from NAMES.TXT. */
bool colonies_load_buildings(ColonizeColonyPool* pool, const ColonizeMsgCatalog* names);

int colonies_find_building(const ColonizeColonyPool* pool, const char* name);
/* True when the building has a real worker slot (see colony.c list). */
bool colonies_building_workable(const ColonizeColonyPool* pool, int building_type);

bool colonies_can_found(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y
);

/* Next default colony name for a nation without consuming it (bugs.md #682). */
const char* colonies_peek_next_name(const ColonizeColonyPool* pool, int nation_id);

/* Active colonies owned by a nation — DOS byte `nation + 0x9298` (bugs.md #681). */
int colonies_nation_settlement_count(const ColonizeColonyPool* pool, int nation_id);

/*
 * Found a colony. founder_type_index < 0 skips population (tests).
 * founder_profession is NAMES.TXT @JOB skill (UNITS_JOB_NONE if unskilled).
 * Tools/muskets/horses from the disbanded unit go into the stockpile stub.
 * nation_id is the owning European power (0..3).
 */
int colonies_found(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  int founder_type_index,
  int founder_profession,
  int tools,
  int muskets,
  int horses
);

/*
 * FUN_364b_1dd6 founding tail: for each nation 0..3 that owns FF 6 (Coronado —
 * `FUN_15eb_3960(nation, 6)`), call FUN_13f1_00a6 on the new colony, revealing
 * the ±5 square around it for that nation and seeding pop_on_map=1 /
 * fort_on_map=0 on every colony inside it that nation has not observed yet.
 *
 * The ±5 sweep is Coronado's effect, not a plain founding effect: without him
 * founding reveals nothing beyond the founder's own unit sight. col1 may be
 * NULL (then nobody has Coronado and this is a no-op), as may map.
 */
void colonies_reveal_founded_w(
  const ColonizeWorld* w,
  int colony_id
);
/*
 * Coronado's elect-time sweep: FUN_4345_0342 case `param_2 == 6`
 * (viceroy_unpacked.c 73155-73159) walks every colony index 0..colony_count
 * with no owner test and runs FUN_13f1_00a6 (the ±5 square + pop_on_map=1
 * seeding) for the electing nation — foreign colonies included.
 */
void colonies_reveal_all_for_nation(
  ColonizeWorldMap* map,
  ColonizeColonyPool* pool,
  int nation_id
);
/* FUN_364b_1b4c: nation's fog snapshot of colony := live population / fort tier. */
void colonies_fog_snapshot(ColonizeColonyPool* pool, int colony_id, int nation_id);
/*
 * FUN_364b_1b76: colony is known to nation — own colony, Complete Map
 * (show_entire_map), or a nonzero pop_on_map snapshot. nation outside 0..3 = known.
 */
bool colonies_known_to(const ColonizeColony* c, int nation_id, bool show_entire_map);

/*
 * Tribal-land owner of a tile: index into col1->tribe of the nearest village
 * (same continent) whose tech-tier radius covers (x,y); -1 when the tile is
 * not tribal land. FUN_15eb_26e4 / FUN_15dc_006a rule. Ignores the
 * purchased bit — pair with colonies_indian_land_purchase_gold for that.
 */
int colonies_indian_land_owner_tribe(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y
);

/*
 * Same test as the colony screen actually runs it (bugs.md #366): DOS hoists the
 * continent lookup out of the 5x5 loop and reads it at the COLONY tile
 * (origin_x, origin_y), and clears every Ocean / Sea Lane cell outright.
 * Also answers the plain "what claims this tile" question with origin ==
 * the queried tile (the thin colonies_indian_claim_tribe wrapper that used
 * to spell that was deleted 2026-09-14, audit CO-22 — it had no callers).
 */
int colonies_indian_claim_tribe_from_w(
  const ColonizeWorld* w,
  int viewer_nation,
  int origin_x,
  int origin_y,
  int x,
  int y
);

/*
 * Pay for tribal land at (x,y): debit *gold by cost (when gold non-NULL),
 * INC indian.lands_bought (FUN_479b_00ca), stamp the purchased bit on the
 * Col1 mask (0x10) and map layer2 (MAP_LAYER2_PURCHASED) — the
 * FUN_281f_068c(...,0x10,1) / FUN_15eb_0668 write every DOS "offer gold"
 * arm (@INDIANLAND/@INDIANROAD/@INDIANFOREST → @INDIANBRIBE) performs.
 */
void colonies_indian_land_pay(
  ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  uint32_t* gold,
  int cost
);

/*
 * Gold to buy Indian homeland tile (FUN_4cc6_07c2). Manual/wiki Minuit:
 * Indians no longer demand payment → 0 via founding_fathers_nation_has(FF 2).
 * Returns 0 outside homeland radius (village 1 / capital city 2; pdf Indian Land),
 * or when tile already has MAP_LAYER2_PURCHASED / Col1 mask 0x10 (WELCOME gift
 * or prior buy). DOS also spends this from pioneer plow/road + colony tile-buy;
 * those callers remain PORT outside this module.
 */
int colonies_indian_land_purchase_gold(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id
);

/*
 * Found with FUN_4cc6_07c2 Indian land charge when tile is homeland.
 * Deducts from *gold; fails (−1) if short. Minuit → free. col1/gold NULL →
 * same as colonies_found (plain found still does not charge).
 */
int colonies_found_with_indian_land_w(
  const ColonizeWorld* w,
  uint32_t* gold,
  int x,
  int y,
  int nation_id,
  int founder_type_index,
  int founder_profession,
  int tools,
  int muskets,
  int horses
);

const ColonizeColony* colonies_get(const ColonizeColonyPool* pool, int colony_id);
ColonizeColony* colonies_get_mut(ColonizeColonyPool* pool, int colony_id);
int colonies_id_at(const ColonizeColonyPool* pool, int x, int y);
/*
 * NAMES.TXT @BUILDING row indices. The order is DOS's own (the colony
 * building bitfield is indexed by it — see docs/save_format_map.md), so the
 * row number, not the English name, is the stable identifier. Use these with
 * colonies_building_row instead of colonies_find_building(pool, "Fortress"):
 * the port keeps no MicroProse names in the binary, and a renamed row must
 * still resolve.
 */
typedef enum ColonizeBuildingRow {
  COLONY_BUILDING_STOCKADE = 0,
  COLONY_BUILDING_FORT = 1,
  COLONY_BUILDING_FORTRESS = 2,
  COLONY_BUILDING_ARMORY = 3,
  COLONY_BUILDING_MAGAZINE = 4,
  COLONY_BUILDING_ARSENAL = 5,
  COLONY_BUILDING_DOCKS = 6,
  COLONY_BUILDING_DRYDOCK = 7,
  COLONY_BUILDING_SHIPYARD = 8,
  COLONY_BUILDING_TOWN_HALL = 9,
  COLONY_BUILDING_TOWN_HALL_2 = 10,
  COLONY_BUILDING_TOWN_HALL_3 = 11,
  COLONY_BUILDING_SCHOOLHOUSE = 12,
  COLONY_BUILDING_COLLEGE = 13,
  COLONY_BUILDING_UNIVERSITY = 14,
  COLONY_BUILDING_WAREHOUSE = 15,
  COLONY_BUILDING_WAREHOUSE_EXPANSION = 16,
  COLONY_BUILDING_STABLE = 17,
  COLONY_BUILDING_CUSTOM_HOUSE = 18,
  COLONY_BUILDING_PRINTING_PRESS = 19,
  COLONY_BUILDING_NEWSPAPER = 20,
  COLONY_BUILDING_WEAVERS_HOUSE = 21,
  COLONY_BUILDING_WEAVERS_SHOP = 22,
  COLONY_BUILDING_TEXTILE_MILL = 23,
  COLONY_BUILDING_TOBACCONISTS_HOUSE = 24,
  COLONY_BUILDING_TOBACCONISTS_SHOP = 25,
  COLONY_BUILDING_CIGAR_FACTORY = 26,
  COLONY_BUILDING_RUM_DISTILLERS_HOUSE = 27,
  COLONY_BUILDING_RUM_DISTILLERY = 28,
  COLONY_BUILDING_RUM_FACTORY = 29,
  COLONY_BUILDING_CAPITOL = 30,
  COLONY_BUILDING_CAPITOL_EXPANSION = 31,
  COLONY_BUILDING_FUR_TRADERS_HOUSE = 32,
  COLONY_BUILDING_FUR_TRADING_POST = 33,
  COLONY_BUILDING_FUR_FACTORY = 34,
  COLONY_BUILDING_CARPENTERS_SHOP = 35,
  COLONY_BUILDING_LUMBER_MILL = 36,
  COLONY_BUILDING_CHURCH = 37,
  COLONY_BUILDING_CATHEDRAL = 38,
  COLONY_BUILDING_BLACKSMITHS_HOUSE = 39,
  COLONY_BUILDING_BLACKSMITHS_SHOP = 40,
  COLONY_BUILDING_IRON_WORKS = 41
} ColonizeBuildingRow;

/* Pool slot of @BUILDING `row`, or -1 when the pool does not carry it. */
int colonies_building_row(const ColonizeColonyPool* pool, ColonizeBuildingRow row);
/*
 * @BUILDING row for a building NAME read from the catalog (exact match against
 * the names the last colonies_load_building_types call saw; a fixture-built
 * pool resolves through the test-only hook below). This is how code that is
 * handed a name asks "which building is this?" without the port carrying any
 * building names of its own. -1 when unknown.
 */
int colonies_building_name_row(const char* name);
/* True when `col` owns @BUILDING `row`. */
bool colonies_has_building_row(
  const ColonizeColonyPool* pool, const ColonizeColony* col, ColonizeBuildingRow row
);

/* @BUILDING row of a pool slot, or -1. */
int colonies_building_type_row(const ColonizeColonyPool* pool, int type_index);
/* Test-only seam, same purpose as units_set_name_kind_resolver: fixtures that
 * build a pool out of names get their rows from tests/common. */
typedef int (*ColoniesBuildingNameRowResolver)(const char* name);
void colonies_set_building_name_row_resolver(ColoniesBuildingNameRowResolver fn);
typedef const char* (*ColoniesBuildingRowNameResolver)(int row);
void colonies_set_building_row_name_resolver(ColoniesBuildingRowNameResolver fn);
/* The catalog's own spelling of @BUILDING `row` ("" when unknown). */
const char* colonies_building_row_name(int row);
/* A chain as @BUILDING rows, lowest tier first, -1 terminated. */
const int* colonies_building_chain_rows(int chain);
/* Chain a @BUILDING row belongs to, or -1. */
int colonies_building_row_chain(int row);

const ColonizeBuildingType* colonies_building_type(const ColonizeColonyPool* pool, int type_index);

/* Manual ch. 6 "Colonies" / building_production.md: at most 3 colonists per
 * building (schools: teachers + students share the cap). GAME.TXT @MORETHANTHREE. */
#define COLONIZE_BUILDING_MAX_WORKERS 3

/* Active colonists currently working building_type in this colony. */
int colonies_building_worker_count(const ColonizeColony* colony, int building_type);

/* Assign colonist to a built workplace (@BUILDING index). Clears any field tile.
 * Schoolhouse/College/University refuse Free/Indentured/Criminal/Convert (@NOTEACHER).
 * Refuses past COLONIZE_BUILDING_MAX_WORKERS (@MORETHANTHREE) unless the
 * colonist is already working that building (no-op reassignment). */
bool colonies_assign_workplace(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int building_type
);

/*
 * Toggle `cargo_type` in this colony's Custom House per-cargo autosell mask
 * (custom_house_bits — see europe_custom_house_autosell). Player-facing:
 * clicking the Custom House opens a checklist of eligible cargoes; this is
 * what a row-click flips. False (no-op) without a built Custom House, or
 * for a cargo outside 0..COLONIZE_CARGO_COUNT.
 */
bool colonies_toggle_custom_house_cargo(ColonizeColonyPool* pool, int colony_id, int cargo_type);

/*
 * True when the @JOB school level of `profession` is 1..3 — DOS's `level < 4`
 * teach gate. False for Free / Indentured / Criminal / Convert / @JOB 18
 * "Teacher" / unset.
 */
bool colonies_profession_may_teach(int profession);

/* NAMES.TXT @JOB column 0 singular name ("Farmer", "Soldier", …). */
const char* colonies_profession_name(int profession);

/* NAMES @JOB school field (1 Schoolhouse … 3 University); 0 if unknown. */
int colonies_job_school_tier(int profession);

/* School building tier 1/2/3, or 0 if not a school. */
int colonies_school_building_tier(
  const ColonizeColonyPool* pool,
  int building_type
);

/*
 * Required school tier (2 or 3) if profession needs a higher school than
 * building_tier; else 0.
 */
int colonies_school_tier_shortfall(int profession, int building_tier);
/* Best school tier the colony OWNS (University 3 / College 2 / Schoolhouse 1,
 * 0 none) — both the faculty cap and the @NEEDCOLLEGE/@NEEDUNIVERSITY test
 * read this, never the clicked school row. bugs.md #580 / #589. */
int colonies_school_owned_tier(const ColonizeColonyPool* pool, const ColonizeColony* col);
/* DOS @JOB occupation a workable building employs (9..18), or -1. */
int colonies_building_occupation(const ColonizeColonyPool* pool, int building_type);
/* Colonists of that occupation in the colony, skipping except_index (-1 = none). */
int colonies_occupation_worker_count(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int occupation,
  int except_index
);

/* Human school assign chrome: GAME.TXT @NOTEACHER. No-op if ai_popups NULL. */
void colonies_emit_noteacher_chrome(
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/* @NEEDCOLLEGE / @NEEDUNIVERSITY when profession tier exceeds school building. */
void colonies_emit_need_school_chrome(
  int profession,
  int building_tier,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);
/* @SCHOOL1 / @COLLEGE2 / @UNIV3 faculty-cap refusal (owned tier 1/2/3). */
void colonies_emit_school_faculty_chrome(
  int owned_tier,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);
/* Assign colonist to a surround tile with a field @JOB. Clears workplace. */
bool colonies_assign_field(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int tile_index,
  int field_job
);
bool colonies_clear_field(ColonizeColonyPool* pool, int colony_id, int tile_index);
/* Tile index (0..7) for colonist, or -1 if not on a field. */
int colonies_colonist_tile(const ColonizeColony* colony, int colonist_index);
/* Map tile index ↔ (dx,dy) offsets from colony center (N=0 … NW=7). */
bool colonies_field_tile_delta(int tile_index, int* out_dx, int* out_dy);
int colonies_field_tile_index(int dx, int dy);

/*
 * bugs.md #584 — DOS plot scan order. DS:0xc8/0xde list the work plots as
 * N,E,S,W,NW,NE,SE,SW; the port's own slot order (colonies_field_tile_delta)
 * is the clockwise MAP_DIR8 one. `colonies_field_scan_order(step)` maps a DOS
 * scan step 0..7 to the runtime tile_index, so a scan that must reproduce a
 * DOS tie-break ("first index wins", FUN_15eb_28c8 raw 13126 elects strictly
 * greater) iterates `colonies_field_scan_order(step)` instead of `step`.
 * Returns -1 outside 0..7.
 */
int colonies_field_scan_order(int step);

/*
 * DOS-LITERAL FUN_15eb_23f2 (raw 12695-12800): the "may this colony work this
 * plot" bitmask for ring slot `tile_index` (0..7, the port's slot order), 0 =
 * workable. Bits: 0x10 off-map / outside the work radius / unexplored, 0x80
 * foreign-owned tile held by a fortified armed unit, 0x02 Lost City Rumour,
 * 0x04 Indian village, 0x20 another colony's centre, 0x40 plot already worked
 * by another colony, 0x08 own centre (unreachable here). Both DOS consumers —
 * the AI plot scan FUN_15eb_28c8 (raw 12993) and the human area-view click
 * FUN_2f2b_3fa6 (raw 50917) — require the byte to be 0, and 3fa6 simply
 * ignores the click otherwise (no popup, no status line).
 */
/*
 * DS:0x329[FUN_15eb_0470()] for this colony: 8 unless it owns @BUILDING row
 * 0x0a (then 12) and 0x0b (then 20). See COLONIZE_COLONY_FIELD_TILES.
 */
int colonies_work_plot_count(const ColonizeColonyPool* pool, const ColonizeColony* col);

uint8_t colonies_plot_blocked_mask(
  const ColonizeWorld* w,
  const ColonizeColony* col,
  int tile_index
);

/* Forward decl — unit helpers need the unit pool without including units.h here. */
typedef struct ColonizeUnitPool ColonizeUnitPool;

/*
 * Admit a land unit on the colony tile into the colony (despawn map unit).
 * Transfers founder-style loot into the warehouse. Returns colonist index or -1.
 * `col1` is optional: when non-NULL, also runs the La Salle immediate-Stockade
 * check (founding_fathers_la_salle_check) if this join crosses pop 3.
 */
int colonies_admit_unit_w(
  const ColonizeWorld* w,
  int colony_id,
  int unit_id
);

/* bugs.md #256: assign every job-less colonist a workplace (Town Hall first).
 * DOS never carries idle colonists; runs after admits and each EOT. */
void colonies_auto_assign_idle(ColonizeColonyPool* pool, int colony_id);

/* DOS FUN_15eb_3930 -> 2ea0 -> 28c8 for one just-admitted colonist: best work
 * plot, else Carpenter (bugs.md #562). */
void colonies_seat_new_colonist(ColonizeColonyPool* pool, int colony_id, int colonist_index);

/* DOS FUN_15eb_1068(slot, 0xd): the Carpenter fallback both 2ea0 and 28c8 use
 * when no work plot scores (bugs.md #562). */
void colonies_assign_carpenter_fallback(
  ColonizeColonyPool* pool, int colony_id, int colonist_index
);
/*
 * Remove a colonist onto the colony map tile as the given role (spends warehouse
 * gear). Compacts the colonist list. Returns new unit id or -1.
 * role: 0=Colonist, 1=Pioneer, 2=Soldier, 3=Scout, 4=Dragoon.
 */
int colonies_eject_colonist(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  ColonizeUnitPool* units,
  int role
);

/* True if Stockade, Fort, or Fortress is built (voluntary eject must leave ≥3 pop). */
bool colonies_has_fortification(const ColonizeColonyPool* pool, const ColonizeColony* colony);

/*
 * Colony fortification defense bonus percent for land combat (0 / 100 / 150 / 200).
 * Cite: docs/building_production.md + fandom Stockade/Fort/Fortress —
 * Stockade +100%, Fort +150%, Fortress +200% (highest built). Wiki: Stockade
 * replaces Fortify benefit inside — callers should not also ×2 fortify when >0.
 */
int colonies_fortification_defense_bonus_percent(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
);

/* Remove colony and warehouse cargo; on-tile units are left alone. */
bool colonies_abandon(ColonizeColonyPool* pool, int colony_id);

/*
 * Bind the live world map so founding and abandoning a colony can keep
 * layer2's MAP_OCCUPANCY_HAS_CITY bit current. That bit is what the map
 * renderer reads to hide a settlement's garrison and what the road art keys
 * off; until this existed it was only rebuilt by the col1 bridge at load and
 * at end-of-turn capture, so an abandoned colony went on hiding the units
 * standing on its square (bugs.md). NULL unbinds.
 */
void colonies_set_occupancy_map(ColonizeWorldMap* map);

/* Capture colony for a new European owner (T0 military / REF / raids). */
bool colonies_capture(ColonizeColonyPool* pool, int colony_id, int new_nation_id);
/*
 * Col1 context for colonies_capture's DOS side effects (FUN_5fef_1b0e capture
 * tail): rebel dividend ×2/3, per-nation colony/pop tallies, treasury share
 * plunder (peacetime), war-relation reset, crown-capture REF-threshold bit.
 * NULL = plain owner swap (tests without a save).
 */
void colonies_set_col1_context(ColonizeCol1Save* col1);
/* Owner swap + DOS side effects; *plunder_gold (may be NULL) = treasury share moved. */
bool colonies_capture_ex(
  ColonizeColonyPool* pool, int colony_id, int new_nation_id, int* plunder_gold
);

/*
 * Tools an equip actually takes out of `available` (warehouse stock plus
 * anything the unit is already carrying): whole 20-tool steps, capped at 100 —
 * a Pioneer legitimately walks with 20/40/60/80/100. Returns 0 when there is
 * not even one step. Shared so the two equip paths (a colonist leaving the
 * colony, and a unit already standing outside it) cannot drift apart again —
 * bugs.md: the outside path demanded the full 100 and answered "Cannot equip
 * unit" for a stock its own menu had just offered Pioneers on.
 */
int colonies_equip_tools_take(int available);

/*
 * Fill out_roles with the "Leave as" rows DOS offers this colonist, and
 * out_enabled (optional) with each row's enabled state: DOS lists a row whose
 * cargo the colony cannot cover but draws it GREYED (FUN_15eb_3454 → 0xffff),
 * and offers an Indian Convert nothing but the Colonist row. See the comment
 * on the definition in colony.c. The short form passes out_enabled = NULL.
 */
int colonies_list_eject_roles_ex(
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int* out_roles,
  bool* out_enabled,
  int out_max
);
/*
 * Same row list for a body that carries its own gear (a unit outside the
 * colony): add_* is what the unit already holds and counts toward the row
 * gates, profession is the body's own @JOB (every >= 0x13 row gate in
 * FUN_15eb_3454 reads it: Convert 0x1b gets the Colonist row only, a Jesuit
 * 0x18 always gets the Missionary row and may lose the Colonist row).
 * colonies_list_eject_roles_ex is this with add_* = 0 and the colonist's own
 * profession; the colony-screen "outside" list in game_loop used to carry a
 * row-for-row copy that drifted once (duplication audit GL-11).
 */
int colonies_list_eject_roles_gear(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int add_tools,
  int add_muskets,
  int add_horses,
  int profession,
  int* out_roles,
  bool* out_enabled,
  int out_max
);
/*
 * Is a "Leave as" row offered at all for this body (FUN_15eb_3454's three
 * `return 0` arms, raw 13556-13570)? Shared by both row builders and both
 * appliers. See the definition in colony.c for the DS:0x8dc6 DOS bug.
 */
bool colonies_eject_row_offered(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int profession,
  int role
);
/*
 * Gear a "Leave as" row takes from the stock: Pioneer = whole 20-tool steps
 * capped at 100 (FUN_15eb_1068 raw 11250-11253, via colonies_equip_tools_take),
 * Soldier 50 muskets, Scout 50 horses, Dragoon both, Colonist / Missionary
 * nothing. Returns false for a role FUN_2f2b_348c never offers. Shared by
 * colonies_eject_colonist and the outside-unit path in game_loop (GL-12).
 */
bool colonies_eject_role_gear(
  int role,
  int stock_tools,
  int* out_tools,
  int* out_muskets,
  int* out_horses
);
int colonies_list_eject_roles(
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int* out_roles,
  int out_max
);

#define COLONIZE_EJECT_COLONIST 0
#define COLONIZE_EJECT_PIONEER 1
#define COLONIZE_EJECT_SOLDIER 2
#define COLONIZE_EJECT_SCOUT 3
#define COLONIZE_EJECT_DRAGOON 4
#define COLONIZE_EJECT_MISSIONARY 5 /* Church/Cathedral bless — Colonization.pdf */
#define COLONIZE_EJECT_ROLE_COUNT 6

const char* colonies_eject_role_name(int role);

/*
 * Col1 stores buildable *units* as `building_in_production` raw codes past the
 * real @BUILDING table's range — NAMES.TXT's @BUILDING section never lists
 * them (they live in @UNIT instead), so colonies_building_type() returns NULL
 * for these. FUN_15eb_32f8 (viceroy_unpacked.c raw 13423) decodes exactly
 * seven of them, 42..48 = @UNIT rows 11..17; see units.h
 * (COLONIZE_UNIT_BUILD_CODE_*) for the decode and the cost arithmetic.
 * Man-O-War is @UNIT row 18 and deliberately out of range.
 */
#define COLONIZE_UNIT_BUILD_ARTILLERY 42
#define COLONIZE_UNIT_BUILD_WAGON_TRAIN 43
#define COLONIZE_UNIT_BUILD_CARAVEL 44
#define COLONIZE_UNIT_BUILD_MERCHANTMAN 45
#define COLONIZE_UNIT_BUILD_GALLEON 46
#define COLONIZE_UNIT_BUILD_PRIVATEER 47
#define COLONIZE_UNIT_BUILD_FRIGATE 48

/*
 * True + fills name/hammers/tools_cost if raw_code is a unit-type
 * construction project; false for a real building_type index or anything
 * else. Thin wrapper over units_build_project_info (DOS FUN_15eb_33aa).
 */
bool colonies_unit_build_info(int raw_code, const char** name, int* hammers, int* tools_cost);

/* Set construction target; building_type must be unowned and meet min_population. */
bool colonies_set_construction(ColonizeColonyPool* pool, int colony_id, int building_type);
bool colonies_clear_construction(ColonizeColonyPool* pool, int colony_id);
/*
 * Destroy a built building (not Town Hall). Clears has_building and moves any
 * workplace colonists in that building to idle (building_type=-1). Cancels
 * matching construction project. Returns false if missing/Town Hall/invalid.
 * Cite: @RAIDBURN building loot needs safe destroy + workplace clear.
 */
bool colonies_destroy_building(ColonizeColonyPool* pool, int colony_id, int building_type);

/* Optional gates for colonies_list_buildable (NULL map / false FF = deny gated buildings). */
typedef struct ColoniesBuildableOpts {
  const ColonizeWorldMap* map; /* docks / drydock / shipyard need a coastal colony */
  bool has_adam_smith;         /* factory-tier buildings */
  bool has_peter_stuyvesant;   /* Custom House */
  /*
   * DOS FUN_15eb_3650 wagon arm reads colony_counts[nation] (DS:0x9298) and
   * unit_type_counts[nation][12] (DS:0x924c + n*0x13 + 12) straight out of the
   * census window; NULL simply skips the cap.
   */
  const ColonizeCol1Save* col1;
} ColoniesBuildableOpts;

/*
 * DOS-LITERAL FUN_15eb_3650 (raw 13736-13740) / FUN_364b_0114 (raw 56926-56933):
 * a nation may not own more Wagon Trains than colonies —
 * `colony_counts[n] <= unit_type_counts[n][12]` blocks the project and, at
 * completion, pops @NOMOREWAGONS. Both counters are the FUN_4962_0018 census
 * window, refreshed every EOT. False when `col1` is NULL.
 */
bool colonies_wagon_cap_reached(const ColonizeCol1Save* col1, int nation_id);
/*
 * colonies_set_construction with the FUN_15eb_3650 side conditions a bare pool
 * cannot answer (the Wagon Train cap needs the census counters). `opts` may be
 * NULL, which reproduces the pre-cap behaviour for callers with no census.
 */
bool colonies_set_construction_ex(
  ColonizeColonyPool* pool,
  int colony_id,
  int building_type,
  const ColoniesBuildableOpts* opts
);
/* Colony count DOS shows as @NOMOREWAGONS %NUMBER0, or 0 with no census. */
int colonies_nation_colony_count_census(const ColonizeCol1Save* col1, int nation_id);

/* Fill out_ids with buildable @BUILDING indices (prerequisites applied); returns count. */
int colonies_list_buildable(
  const ColonizeColonyPool* pool,
  int colony_id,
  int* out_ids,
  int out_max,
  const ColoniesBuildableOpts* opts
);

/*
 * Gold to rush-buy the current project's remaining hammers+tools, or 0 if
 * none/nothing missing. DOS formula (FUN_2f2b_5e44, clean decompile —
 * original_sources_decompiled/viceroy_unpacked.c:52683):
 *   hammers_deficit × 13, plus tools_deficit × (per-nation table byte + 4)
 *   when tools are short, the whole sum DOUBLED outright if the colony
 *   hasn't banked any hammers at all yet (colony->hammers == 0) — DOS
 *   charges a steep premium for rushing an unstarted project. The ×13 and
 *   doubling are read directly off the disassembly; the per-nation tools
 *   table byte itself wasn't pinned to a named field with confidence and is
 *   approximated here as `difficulty + 4` (this port's existing 0-8
 *   difficulty byte, same shape/magnitude as the confirmed term) — flagged
 *   as an approximation, not a verified value.
 */
int colonies_construction_gold_cost(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int difficulty
);
/*
 * If hammers >= need and tools >= tools_cost: spend tools, mark built, clear project.
 * Returns true when a building was completed.
 */
bool colonies_try_complete_building(ColonizeColonyPool* pool, int colony_id);
/*
 * Unit-type construction completion (colonies_unit_build_info) — same
 * hammers/tools gate and bookkeeping as colonies_try_complete_building, but
 * spawns the map unit at the colony tile instead of setting has_building[]
 * (needs `units`, which colonies_try_complete_building doesn't take, since
 * a real building never spawns anything). Returns the new unit id on
 * success, -1 otherwise (no project, not a unit-type project, or short on
 * hammers/tools/spawn).
 */
int colonies_try_complete_unit_construction(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units
);
/*
 * Rush-buy the current project: tops hammers up to the completion
 * threshold and tools up to the requirement (deducting colonies_construction_
 * gold_cost's formula from *gold), same as DOS's FUN_2f2b_5e44 — it does
 * NOT complete the project. Completion only happens through the normal
 * per-turn checks (turn_run_colony_building_completion /
 * turn_run_colony_unit_construction, both unconditional once-per-turn
 * passes), matching DOS: rushing a project just fills the tank, and the
 * next turn's construction processing notices it's full and finishes the
 * job. Fails if no project or insufficient gold. Updates *gold on success.
 */
bool colonies_buy_construction(ColonizeColonyPool* pool, int colony_id, int difficulty, int* gold);

/*
 * Warehouse capacity (100 base; +100 Warehouse; +100 Expansion).
 * FUN_15eb_0a50 takes no cargo argument — one capacity for all sixteen goods,
 * Food included. Food's exemptions live at the three sites that consume the
 * cap (EOT spoilage, @WAREHOUSEFULL unload confirm, colony-screen alert ink),
 * not here; cargo_type is unused by the body and is kept only so the ~10
 * call sites stay readable (audit CO-28 — keeping the parameter is far less
 * churn than rewriting every caller, and the body now says so with a
 * (void) cast).
 */
int colonies_warehouse_capacity(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type
);

/*
 * Human unload chrome: GAME.TXT @WAREHOUSEFULL when warehouse has no room.
 * cargo_name optional (fallback "cargo"). No-op if ai_popups NULL.
 * @WAREHOUSEFULL's numbers (bugs.md #433): NUMBER0 = what the warehouse
 * already held BEFORE this deposit, NUMBER1 = capacity, NUMBER2 = the
 * deposit itself. `deposited` = units the player just unloaded (or tried
 * to); `already_included` = how much of that has already been added to
 * colony->stock by the time this is called (0 when the transfer failed).
 */
void colonies_emit_warehouse_full_chrome(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type,
  const char* cargo_name,
  int deposited,
  int already_included,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/*
 * Human Join Colony chrome: GAME.TXT @FULL when colony is at population cap.
 * No-op if ai_popups NULL.
 */
void colonies_emit_full_chrome(
  const ColonizeColony* colony,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/*
 * Human construction refuse: @ALREADYHAVE, or @NOMOREWAREHOUSE for Warehouse
 * Expansion. building_name optional (fallback "building"). No-op if ai_popups NULL.
 */
void colonies_emit_already_have_chrome(
  const ColonizeColony* colony,
  const char* building_name,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/* Human building-slot-full refuse: @MORETHANTHREE (COLONIZE_BUILDING_MAX_WORKERS). */
void colonies_emit_more_than_three_chrome(
  const ColonizeColony* colony,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/*
 * FUN_5952_0306: set/clear specialty_cargo (+0x8d).
 * want_set cleared when stock >= warehouse capacity, or when the colony
 * already produces the cargo (DS:0x8dc8 gross-production ledger != 0 —
 * `already_produced`). The only DOS caller is FUN_5952_035e's five-call
 * build-preference block (ai_euro_5952_build_pref_0306).
 * Cite: viceroy_unpacked.c:93760 FUN_5952_0306; FUN_15eb_0a50 capacity.
 */
void colonies_specialty_cargo_update(
  const ColonizeColonyPool* pool,
  ColonizeColony* colony,
  int cargo_type,
  int want_set,
  int already_produced
);

/*
 * EOT spoilage: clamp each stock (Food excepted) to warehouse capacity
 * (FUN_364b_0688's cargo loop). Call after production + Custom House (wiki:
 * auto-sell before spoilage).
 *
 * `stock_before` is this colony's per-cargo stock as it stood *before* this
 * turn's production, COLONIZE_CARGO_COUNT entries, or NULL to treat the whole
 * overflow as pre-existing. DOS only *reports* the part of the overflow that
 * predates production — goods a colony simply out-produced its warehouse for
 * are clamped silently. Returns the reportable total (0 when nothing but
 * production overflowed, or when the loss is under 2 tons).
 *
 * A pre-existing overflow of exactly 1 ton is neither reported nor removed —
 * DOS zeroes the loss below 2 tons *after* backing production off the stock,
 * so that ton stays in the warehouse (FUN_364b_0688 raw 57847-57868).
 *
 * When out_first_cargo != NULL and any reportable spoil occurs, writes the
 * first such cargo index. When out_type_count != NULL, writes how many
 * distinct cargo types spoiled reportably.
 */
int colonies_apply_warehouse_spoilage(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const int* stock_before,
  int* out_first_cargo,
  int* out_type_count
);

/* Move up to `amount` of cargo_type from colony stock into a transport unit. Returns amount moved. */
int colonies_transfer_to_unit(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int cargo_type,
  int amount
);
/*
 * Unload one goods hold into the colony warehouse (respects capacity).
 * Returns amount moved. *out_warehouse_full true if hold still has leftovers.
 */
int colonies_transfer_from_unit(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index,
  bool* out_warehouse_full
);
/*
 * Partial unload (@HOWMUCH2 shift-drag): move up to `amount` from the hold
 * ashore; delegates to colonies_transfer_from_unit when it empties the hold.
 */
int colonies_transfer_from_unit_amount(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index,
  int amount,
  bool* out_warehouse_full
);

/*
 * Foreign-colony trade — DOS FUN_5f7a_020e (raw 98885-99051). The Jan de Witt
 * (FF 4) mechanic, and the ONLY one: a human-controlled Euro nation's cargo
 * unit bumping a foreign Euro colony haggles one hold away for gold or barter,
 * never enters, and always loses its whole MP allotment.
 * Spec: docs/foreign_colony_trade.md.
 */
typedef enum ColonizeForeignTradeGate {
  COLONIZE_FTRADE_NONE = 0,     /* not a trade situation — no dialog at all */
  COLONIZE_FTRADE_ATWAR,        /* @TRADEATWAR — no peace treaty (rel & 0x40 clear) */
  COLONIZE_FTRADE_MERCANTILISM, /* @TRADEMERCANTILISM — no Jan de Witt */
  COLONIZE_FTRADE_NOCARGO,      /* @TRADENOCARGO — unit +0x3150 == 0 */
  COLONIZE_FTRADE_OK            /* pick a hold and deal */
} ColonizeForeignTradeGate;

/*
 * raw 98915-98936. `unit_id` must stand on / be stepping onto the tile of
 * `foreign_colony_id`; the caller (the move handler) owns that test, as DOS's
 * FUN_465b_0000 does.
 */
ColonizeForeignTradeGate colonies_foreign_trade_gate(
  const ColonizeWorld* w,
  int foreign_colony_id,
  int unit_id
);

typedef struct ColonizeForeignTradeDeal {
  int sold_cargo;  /* @CARGO index in the chosen hold */
  int sold_qty;
  int gold;        /* the gold offer (>= 1) */
  int offer_cargo; /* counter-offer @CARGO index, -1 = none → @TRADENOWANT */
  int offer_qty;
} ColonizeForeignTradeDeal;

/*
 * raw 98963-99012: price the hold and find the colony's best counter-offer.
 * Draws once from `rng` (FUN_281f_04d4(10, (difficulty+1)*12)) unless the WoI
 * intervention-ally arm applies. Returns 0 (and leaves *out zeroed) when the
 * gate is not OK or the hold is empty.
 */
int colonies_foreign_trade_prepare(
  const ColonizeWorld* w,
  ColonizeDosRng* rng,
  int foreign_colony_id,
  int unit_id,
  int hold_index,
  ColonizeForeignTradeDeal* out
);

/*
 * raw 99014-99049. `take_goods` != 0 → the hold becomes (offer_cargo,
 * offer_qty) (FUN_281f_0cea/0ca4); else the hold is emptied (FUN_281f_0aec)
 * and `deal->gold` is credited to the unit's nation. Both arms then do
 * `colony.stock[sold_cargo] += sold_qty` — DOS never debits the colony's stock
 * of the goods it hands over. Returns 1 when applied.
 */
int colonies_foreign_trade_apply(
  const ColonizeWorld* w,
  int foreign_colony_id,
  int unit_id,
  int hold_index,
  const ColonizeForeignTradeDeal* deal,
  int take_goods
);

/* Best cargo type for L-key load (excludes horses/tools/muskets); -1 if none. */
int colonies_best_load_cargo(const ColonizeColony* colony);

/*
 * TRADE stop cargo at own colony — DOS FUN_479b_0bd0 arrival body:
 * unload exactly the stop's unload-list cargos (all matching holds), then
 * load exactly the load-list cargos greedily by colony stock (highest stock
 * first, 100-unit holds) until the transport is full or stock runs out.
 * Empty lists move nothing — no unload-all / surplus-ladder fallback.
 * Returns 1 if any transfer happened. Cite: ColonizeCol1TradeStop.
 */
int colonies_trade_route_service_stop(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  const ColonizeCol1TradeStop* stop
);

/*
 * Thin TRADE Edit helper: fill unload nibbles from selected unit holds;
 * for a colony stop, fill load nibbles from surplus ladder (tools…food).
 * Europe (colony NULL): unload only. Cite: ColonizeCol1TradeStop.
 */
void colonies_trade_stop_autofill(
  ColonizeCol1TradeStop* stop,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  int unit_id
);

/*
 * Thin TRADE cargo picker: set unload/load nibble lists explicitly (cap 6 each).
 * cargo_type values are @CARGO indices 0..15. Cite: ColonizeCol1TradeStop.
 */
void colonies_trade_stop_set_cargos(
  ColonizeCol1TradeStop* stop,
  const int* unload_types,
  int unload_n,
  const int* load_types,
  int load_n
);

/* ICONS.SS settlement marker #0–3 by fortification (none/stockade/fort/fortress). */
int colonies_settlement_icon(const ColonizeColonyPool* pool, const ColonizeColony* colony);

/*
 * ---------------------------------------------------------------------
 * Shared building upgrade chains (2026-09-14 duplication audit CO-13 /
 * IN-24: the same 15 chains were typed out in col1_bridge.c twice,
 * colony_screen.c once and colony.c once more as hardcoded name pairs).
 *
 * colonies_building_chain(chain) returns the chain's NULL-terminated name
 * list, lowest tier first, or NULL for an out-of-range id.
 *
 * ***THE ENUM ORDER IS A SAVE-FORMAT CONTRACT.*** col1_bridge.c maps chain
 * position i onto bit i of the matching ColonizeCol1Buildings group word and
 * walks the chains in this order. Reordering the enum, or inserting a tier
 * into the middle of a chain, rewrites every colony's building mask on the
 * next export. Appending a new chain at the end is safe; nothing else is.
 *
 * 17 entries for DOS's 15 screen categories: Capitol and Stable are split
 * out of the Town Hall / Warehouse categories they share a colony-screen
 * slot with, because the save format gives each its own bit group and its
 * own level byte (+0x96 / +0x95).
 * ---------------------------------------------------------------------
 */
enum {
  COLONIES_CHAIN_FORTIFICATION = 0, /* Stockade / Fort / Fortress */
  COLONIES_CHAIN_ARMORY,            /* Armory / Magazine / Arsenal */
  COLONIES_CHAIN_DOCKS,             /* Docks / Drydock / Shipyard */
  COLONIES_CHAIN_TOWN_HALL,         /* Town Hall */
  COLONIES_CHAIN_SCHOOL,            /* Schoolhouse / College / University */
  COLONIES_CHAIN_WAREHOUSE,         /* Warehouse / Warehouse Expansion */
  COLONIES_CHAIN_CAPITOL,           /* Capitol / Capitol Expansion (unbuildable) */
  COLONIES_CHAIN_STABLE,            /* Stable */
  COLONIES_CHAIN_CUSTOM_HOUSE,      /* Custom House */
  COLONIES_CHAIN_PRESS,             /* Printing Press / Newspaper */
  COLONIES_CHAIN_WEAVER,            /* Weaver's House / Shop / Textile Mill */
  COLONIES_CHAIN_TOBACCONIST,       /* Tobacconist's House / Shop / Cigar Factory */
  COLONIES_CHAIN_RUM,               /* Rum Distiller's House / Distillery / Factory */
  COLONIES_CHAIN_FUR,               /* Fur Trader's House / Trading Post / Factory */
  COLONIES_CHAIN_CARPENTER,         /* Carpenter's Shop / Lumber Mill */
  COLONIES_CHAIN_CHURCH,            /* Church / Cathedral */
  COLONIES_CHAIN_BLACKSMITH,        /* Blacksmith's House / Shop / Iron Works */
  COLONIES_BUILDING_CHAIN_COUNT
};

const char* const* colonies_building_chain(int chain);
int colonies_building_chain_length(int chain);

/* Exact-name / substring "does this colony own such a building" (audit
 * CO-17: turn.c, colony_screen.c and colony_preview.c each had a private
 * spelling of one of these two). */
bool colonies_has_building_named(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const char* name
);
bool colonies_has_building_name_contains(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const char* needle
);

/* True when the colony owns a Church or a Cathedral (audit GL-10 — the
 * game_loop copy was a verbatim duplicate of this file-local test). */
int colonies_has_church_or_cathedral(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col
);

/* Active colonies belonging to `nation` (audit AK-9: ai.c, ai_king.c and
 * ai_euro.c each had a byte-identical private copy, plus five inline ones). */
int colonies_count_for_nation(const ColonizeColonyPool* pool, int nation);

/* The active colony standing on (x,y), or NULL (audit SC-38/AC-7/AE-41 — the
 * same pool walk was retyped in reports.c twice and in the AI files). */
const ColonizeColony* colonies_find_at_xy(const ColonizeColonyPool* pool, int x, int y);

/*
 * Draw ICONS.SS settlement marker `sprite` (0-3) at (px,py) — the sprite's
 * own top-left, not a tile origin — then recolor its stored blue flag
 * pixels to `nation_id`'s own color (nearest match within
 * `active_palette`; no-op if `active_palette` is NULL or nation_id isn't
 * 0..3). Every colony-icon draw site should go through this, not a plain
 * ss_blit_sprite, so the flag always matches the owning nation. See
 * unit_chrome_nation_flag_shades_for_palette (unit_chrome.h) for why a
 * palette-aware recolor is needed at all.
 */
void colonies_blit_settlement_icon(
  const ColonizeSpriteSheet* icons,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int px,
  int py,
  int nation_id,
  const ColonizePalette* active_palette
);

/*
 * DOS FUN_112b_0c64 draws the two labels with two different fonts, from two
 * separate DS font pointers: the colony name with FONTINTR (DS:0x268a) and the
 * population badge with FONTTINY (DS:0x89e). Both were loaded at startup by
 * CODE_153:75c2:2e5b / 2e78 from the DS strings "fontintr" / "fonttiny".
 */
void colonies_render_on_map(
  const ColonizeColonyPool* pool,
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,     /* FONTINTR — colony name label */
  const ColonizeFont* pop_font, /* FONTTINY — population badge; NULL falls back to font */
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  const ColonizeWorldMap* fog_map,
  int fog_nation,
  const ColonizePalette* active_palette
);

#endif
