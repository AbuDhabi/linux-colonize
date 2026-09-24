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
/* work-plot ring sizing derivation (see docs/colony.md#colonize_colony_field_tiles) */
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
  /* FUN_364b_0688 education nibble mapping (see docs/colony.md#colonizecolonistturns_in_job) */
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
  /* surrounding field slot ring order (see docs/colony.md#colonizecolonytiles) */
  int8_t tiles[COLONIZE_COLONY_FIELD_TILES_MAX];
  /* Warehouse + build queue — production ticks in src/core/turn.c. */
  int stock[COLONIZE_CARGO_COUNT];
  int hammers;
  int building_in_production; /* @BUILDING index, or -1 */
  /* Custom House per-cargo enable mask layout (see docs/colony.md#colonize_custom_house_default_mask) */
#define COLONIZE_CUSTOM_HOUSE_DEFAULT_MASK 0x1edeu
  uint16_t custom_house_bits;
  /* Col1 +0x8e LABOR demand counter formula (see docs/colony.md#colonizecolonylabor_shortage) */
  uint8_t labor_shortage;
  /* Col1 +0x1e garrison fortify quota (see docs/colony.md#colonizecolonygarrison_quota) */
  uint8_t garrison_quota;
  /* Col1 +0xba/+0xbe fog-of-war snapshots (see docs/colony.md#colonizecolonypop_on_map-fort_on_map) */
  uint8_t pop_on_map[4];
  uint8_t fort_on_map[4];
  /*
   * Col1 +0x8d specialty cargo index (`0xff` = none). FUN_5952_0306 set/clear
   * from warehouse stock vs capacity + boycott. Haul prefers this cargo.
   */
  uint8_t specialty_cargo;
  /* Col1 +0x8f cargo-idle turns (see docs/colony.md#colonizecolonycargo_idle_turns) */
  uint8_t cargo_idle_turns;
  /* Col1 +0x8c improve timer (see docs/colony.md#colonizecolonyimprove_timer) */
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
  /* DOS DS:0x34a pending building reveal (see docs/colony.md#colonizecolonypending_build_reveal) */
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
  /* port-only within-turn food latch rationale (see docs/colony.md#colonizecolonyfood_shortfall_latch) */
  uint8_t food_shortfall_latch;
  /* port-only Phase A compose snapshot (smell audit #62) (see docs/colony.md#colonizecolonyprod_bells_phase_a) */
  int prod_bells_phase_a;
  int prod_crosses_phase_a;
  uint32_t prod_compose_stamp; /* head.turn + 1; 0 = never composed */
} ColonizeColony;

#define COLONIZE_BUILD_AI_WANTS_CONSTRUCTION 0x80u
/* census phase 3 armed-ship/frigate detection (see docs/colony.md#colonize_colony_ai_nearby_armed_ship-_frigate) */
#define COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP 0x01u
#define COLONIZE_COLONY_AI_NEARBY_FRIGATE 0x02u
/* defender-count pair writers/readers/consumer audit (see docs/colony.md#colonize_colony_ai_military_surplus-_short_defenders) */
#define COLONIZE_COLONY_AI_MILITARY_SURPLUS 0x04u
#define COLONIZE_COLONY_AI_SHORT_DEFENDERS 0x08u
#define COLONIZE_COLONY_AI_NEEDS_COLONISTS 0x10u
/* ring-clear trigger conditions and readers (see docs/colony.md#colonize_colony_ai_wants_pioneer_clear) */
#define COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR 0x20u
#define COLONIZE_COLONY_AI_NEEDS_GARRISON 0x40u
/* worked-tile road/plow want bit (see docs/colony.md#colonize_colony_ai_wants_pioneer_work) */
#define COLONIZE_COLONY_AI_WANTS_PIONEER_WORK 0x80u
#define COLONIZE_COLONY_FLAG_SOL_100 0x02u
#define COLONIZE_COLONY_FLAG_SOL_50 0x04u
/* inefficient-government latch history (see docs/colony.md#colonize_colony_flag_inefficient_gov) */
#define COLONIZE_COLONY_FLAG_INEFFICIENT_GOV 0x08u
/* small-colony AI bit and wagon-train bit derivation (see docs/colony.md#colonize_colony_flag_small_ai-_wagon_train) */
#define COLONIZE_COLONY_FLAG_SMALL_AI 0x10u
#define COLONIZE_COLONY_FLAG_WAGON_TRAIN 0x20u
/* coastal bit founding-time derivation (see docs/colony.md#colonize_colony_flag_coastal) */
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

/* FUN_364b_1dd6 founding tail / Coronado reveal (see docs/colony.md#colonies_reveal_founded_w) */
void colonies_reveal_founded_w(
  const ColonizeWorld* w,
  int colony_id
);
/* Coronado elect-time sweep (see docs/colony.md#colonies_reveal_all_for_nation) */
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

/* tribal-land owner tile rule (see docs/colony.md#colonies_indian_land_owner_tribe) */
int colonies_indian_land_owner_tribe(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y
);

/* colony-screen continent lookup test (see docs/colony.md#colonies_indian_claim_tribe_from_w) */
int colonies_indian_claim_tribe_from_w(
  const ColonizeWorld* w,
  int viewer_nation,
  int origin_x,
  int origin_y,
  int x,
  int y
);

/* tribal land payment side effects (see docs/colony.md#colonies_indian_land_pay) */
void colonies_indian_land_pay(
  ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  uint32_t* gold,
  int cost
);

/* Indian homeland gold formula (see docs/colony.md#colonies_indian_land_purchase_gold) */
int colonies_indian_land_purchase_gold(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id
);

/* founding with Indian land charge (see docs/colony.md#colonies_found_with_indian_land_w) */
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
/* row-indexed identity rationale (see docs/colony.md#colonizebuildingrow) */
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
/* name-to-row resolution contract (see docs/colony.md#colonies_building_name_row) */
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

/* DS:0x2f4 @JOB -> required @BUILDING row (-1 = none / not an indoor job). */
int colonies_job_required_building_row(int job);
/* FUN_15eb_3454's `job < 0x13` arm: is this jobs-menu row listed at all? */
bool colonies_job_row_offered(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int job, int profession
);
/* Highest owned tier of the chain an indoor @JOB works in (pool slot, -1 none). */
int colonies_job_workplace_building(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int job
);

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

/* Custom House autosell checklist toggle (see docs/colony.md#colonies_toggle_custom_house_cargo) */
bool colonies_toggle_custom_house_cargo(ColonizeColonyPool* pool, int colony_id, int cargo_type);

/* DOS teach-gate rule (see docs/colony.md#colonies_profession_may_teach) */
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

/* DOS plot scan order (bugs.md #584) (see docs/colony.md#colonies_field_scan_order) */
int colonies_field_scan_order(int step);

/* FUN_15eb_23f2 workability bitmask (see docs/colony.md#colonies_plot_blocked_mask) */
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

/* colony admit / La Salle check (see docs/colony.md#colonies_admit_unit_w) */
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
/* eject role gear spend (see docs/colony.md#colonies_eject_colonist) */
int colonies_eject_colonist(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  ColonizeUnitPool* units,
  int role
);

/* True if Stockade, Fort, or Fortress is built (voluntary eject must leave ≥3 pop). */
bool colonies_has_fortification(const ColonizeColonyPool* pool, const ColonizeColony* colony);

/* fortification defense bonus source (see docs/colony.md#colonies_fortification_defense_bonus_percent) */
int colonies_fortification_defense_bonus_percent(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
);

/* Remove colony and warehouse cargo; on-tile units are left alone. */
bool colonies_abandon(ColonizeColonyPool* pool, int colony_id);

/* occupancy map bind rationale (see docs/colony.md#colonies_set_occupancy_map) */
void colonies_set_occupancy_map(ColonizeWorldMap* map);

/* Capture colony for a new European owner (T0 military / REF / raids). */
bool colonies_capture(ColonizeColonyPool* pool, int colony_id, int new_nation_id);
/* capture-tail Col1 context (see docs/colony.md#colonies_set_col1_context) */
void colonies_set_col1_context(ColonizeCol1Save* col1);
/* Owner swap + DOS side effects; *plunder_gold (may be NULL) = treasury share moved. */
bool colonies_capture_ex(
  ColonizeColonyPool* pool, int colony_id, int new_nation_id, int* plunder_gold
);

/* tool-step equip rule sharing rationale (see docs/colony.md#colonies_equip_tools_take) */
int colonies_equip_tools_take(int available);

/* Leave-as row list / greyed rows (see docs/colony.md#colonies_list_eject_roles_ex) */
int colonies_list_eject_roles_ex(
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int* out_roles,
  bool* out_enabled,
  int out_max
);
/* Leave-as row list for external gear-carrying body (see docs/colony.md#colonies_list_eject_roles_gear) */
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
/* row-offered gate sharing (see docs/colony.md#colonies_eject_row_offered) */
bool colonies_eject_row_offered(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int profession,
  int role
);
/* Leave-as row gear cost table (see docs/colony.md#colonies_eject_role_gear) */
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

/* unit-type construction raw code decode (see docs/colony.md#colonize_unit_build_artillery-etc) */
#define COLONIZE_UNIT_BUILD_ARTILLERY 42
#define COLONIZE_UNIT_BUILD_WAGON_TRAIN 43
#define COLONIZE_UNIT_BUILD_CARAVEL 44
#define COLONIZE_UNIT_BUILD_MERCHANTMAN 45
#define COLONIZE_UNIT_BUILD_GALLEON 46
#define COLONIZE_UNIT_BUILD_PRIVATEER 47
#define COLONIZE_UNIT_BUILD_FRIGATE 48

/* unit construction project info wrapper (see docs/colony.md#colonies_unit_build_info) */
bool colonies_unit_build_info(int raw_code, const char** name, int* hammers, int* tools_cost);

/* Set construction target; building_type must be unowned and meet min_population. */
bool colonies_set_construction(ColonizeColonyPool* pool, int colony_id, int building_type);
bool colonies_clear_construction(ColonizeColonyPool* pool, int colony_id);
/* destroy building side effects (see docs/colony.md#colonies_destroy_building) */
bool colonies_destroy_building(ColonizeColonyPool* pool, int colony_id, int building_type);

/* Optional gates for colonies_list_buildable (NULL map / false FF = deny gated buildings). */
typedef struct ColoniesBuildableOpts {
  const ColonizeWorldMap* map; /* docks / drydock / shipyard need a coastal colony */
  bool has_adam_smith;         /* factory-tier buildings */
  bool has_peter_stuyvesant;   /* Custom House */
  /* wagon-cap census counters (see docs/colony.md#coloniesbuildableoptscol1) */
  const ColonizeCol1Save* col1;
} ColoniesBuildableOpts;

/* FUN_15eb_3650 / FUN_364b_0114 wagon cap (see docs/colony.md#colonies_wagon_cap_reached) */
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

/* FUN_2f2b_5e44 rush-buy gold formula (see docs/colony.md#colonies_construction_gold_cost) */
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
/* unit-type completion / tools-short handling (see docs/colony.md#colonies_try_complete_unit_construction) */
int colonies_try_complete_unit_construction(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  ColonizeCol1Save* col1
);
/* rush-buy semantics vs completion timing (see docs/colony.md#colonies_buy_construction) */
bool colonies_buy_construction(ColonizeColonyPool* pool, int colony_id, int difficulty, int* gold);

/* capacity formula and cargo_type parameter history (see docs/colony.md#colonies_warehouse_capacity) */
int colonies_warehouse_capacity(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type
);

/* @WAREHOUSEFULL numbers (bugs.md #433) (see docs/colony.md#colonies_emit_warehouse_full_chrome) */
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
 * bugs.md #784: the DOS @WAREHOUSEFULL gate — `cap < stock + amount &&
 * cargo != Food`, asked BEFORE the goods move (thunk_FUN_1000_9784).
 */
bool colonies_warehouse_unload_needs_confirm(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type,
  int amount
);

/*
 * Enqueue that gate as the 2-way AI_POPUP_TAG_COLONY_WAREHOUSE confirm
 * (1 = "Never mind.", 2 = "Unload ... anyway."). Returns true when the
 * request went on the queue and the caller must NOT unload yet.
 */
bool colonies_emit_warehouse_full_confirm(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type,
  const char* cargo_name,
  int amount,
  int unit_id,
  int hold_index,
  int payload,
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

/* FUN_5952_0306 specialty cargo set/clear (see docs/colony.md#colonies_specialty_cargo_update) */
void colonies_specialty_cargo_update(
  const ColonizeColonyPool* pool,
  ColonizeColony* colony,
  int cargo_type,
  int want_set,
  int already_produced
);

/* EOT spoilage clamp / reporting rules (see docs/colony.md#colonies_apply_warehouse_spoilage) */
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

/* FUN_5f7a_020e mechanic overview (see docs/colony.md#foreign-colony-trade-jan-de-witt) */
typedef enum ColonizeForeignTradeGate {
  COLONIZE_FTRADE_NONE = 0,     /* not a trade situation — no dialog at all */
  COLONIZE_FTRADE_ATWAR,        /* @TRADEATWAR — no peace treaty (rel & 0x40 clear) */
  COLONIZE_FTRADE_MERCANTILISM, /* @TRADEMERCANTILISM — no Jan de Witt */
  COLONIZE_FTRADE_NOCARGO,      /* @TRADENOCARGO — unit +0x3150 == 0 */
  COLONIZE_FTRADE_OK            /* pick a hold and deal */
} ColonizeForeignTradeGate;

/* trade gate raw site (see docs/colony.md#colonies_foreign_trade_gate) */
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

/* hold pricing / counter-offer raw site (see docs/colony.md#colonies_foreign_trade_prepare) */
int colonies_foreign_trade_prepare(
  const ColonizeWorld* w,
  ColonizeDosRng* rng,
  int foreign_colony_id,
  int unit_id,
  int hold_index,
  ColonizeForeignTradeDeal* out
);

/* trade apply raw site / stock debit note (see docs/colony.md#colonies_foreign_trade_apply) */
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

/* FUN_479b_0bd0 arrival body (see docs/colony.md#colonies_trade_route_service_stop) */
int colonies_trade_route_service_stop(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  const ColonizeCol1TradeStop* stop
);

/* thin TRADE Edit helper (see docs/colony.md#colonies_trade_stop_autofill) */
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

/* shared building upgrade chains / save-format contract (see docs/colony.md#colonies_chain_-enum) */
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

/* settlement icon flag recolor rationale (see docs/colony.md#colonies_blit_settlement_icon) */
void colonies_blit_settlement_icon(
  const ColonizeSpriteSheet* icons,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int px,
  int py,
  int nation_id,
  const ColonizePalette* active_palette
);

/* FUN_112b_0c64 dual-font label rendering (see docs/colony.md#colonies_render_on_map) */
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
