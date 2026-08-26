#ifndef COLONIZE_COLONY_H
#define COLONIZE_COLONY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/map.h"
#include "core/ss.h"

/* Forward declaration to avoid pulling in font headers. */
typedef struct ColonizeFont ColonizeFont;
typedef struct ColonizeCol1Save ColonizeCol1Save;
typedef struct ColonizeCol1TradeStop ColonizeCol1TradeStop;
typedef struct ColonizeUnitPool ColonizeUnitPool;
typedef struct AiPopupState AiPopupState;

#define COLONIZE_COLONIES_MAX 32
#define COLONIZE_COLONY_NAME_MAX 28
#define COLONIZE_COLONY_NAMES_MAX 80
#define COLONIZE_BUILDING_TYPES_MAX 48
#define COLONIZE_COLONY_POP_MAX 32
#define COLONIZE_COLONY_FIELD_TILES 8

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
  int hammers;
  int tools_cost;
  int min_population;
} ColonizeBuildingType;

/* One person living in a colony (disbanded map unit). */
typedef struct ColonizeColonist {
  int unit_type_index; /* into ColonizeUnitPool types (usually Colonists while working) */
  int profession;      /* NAMES.TXT @JOB skill; UNITS_JOB_NONE if none */
  int building_type;   /* workplace @BUILDING index, or -1 */
  int field_job;       /* @JOB field index 0..8, or -1 */
  bool active;
  /* FUN_364b_0688 education: turns in current workplace (DOS 0d1c counter). */
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
  /* Surrounding field slots: colonist index or -1. Order N,NE,E,SE,S,SW,W,NW. */
  int8_t tiles[COLONIZE_COLONY_FIELD_TILES];
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
   * ~91589). Cite: save_format_map.md; euro_unit_act case 0x0b.
   */
  uint8_t labor_shortage;
  /*
   * Col1 +0x1e garrison fortify quota. DEC on fortify/'A' assign; seeded by
   * threat>>3 (FUN_5952_035e) — Linux thin-latches 1 when idle garrison needs
   * fortify. Cite: save_format_map.md; euro_unit_act §2d3.
   */
  uint8_t garrison_quota;
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
   * Port-only EOT latch for inefficient-government chrome (DOS +0x1c bit3 /
   * FUN_364b_0688). Col1 bit3 stays starvation — do not bridge this field.
   */
  uint8_t inefficient_gov;
} ColonizeColony;

#define COLONIZE_BUILD_AI_WANTS_CONSTRUCTION 0x80u
#define COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP 0x01u
#define COLONIZE_COLONY_AI_NEARBY_MAN_O_WAR 0x02u
#define COLONIZE_COLONY_AI_NEEDS_MILITARY 0x04u
#define COLONIZE_COLONY_AI_NEEDS_COLONISTS 0x10u
#define COLONIZE_COLONY_AI_NEEDS_GARRISON 0x40u
#define COLONIZE_COLONY_FLAG_SOL_100 0x02u
#define COLONIZE_COLONY_FLAG_SOL_50 0x04u
#define COLONIZE_COLONY_FLAG_STARVATION 0x08u
#define COLONIZE_COLONY_FLAG_SMALL_AI 0x10u
#define COLONIZE_COLONY_FLAG_WAGON_TRAIN 0x20u
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

bool colonies_can_found(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y
);

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
int colonies_found_with_indian_land(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
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

/* True if building name is Schoolhouse / College / University. */
bool colonies_is_school_building(
  const ColonizeColonyPool* pool,
  int building_type
);

/* False for Free / Indentured / Criminal / Convert / unset — cannot teach. */
bool colonies_profession_may_teach(int profession);

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

/* Forward decl — unit helpers need the unit pool without including units.h here. */
typedef struct ColonizeUnitPool ColonizeUnitPool;

/*
 * Admit a land unit on the colony tile into the colony (despawn map unit).
 * Transfers founder-style loot into the warehouse. Returns colonist index or -1.
 */
int colonies_admit_unit(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id
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

/* Capture colony for a new European owner (T0 military / REF / raids). */
bool colonies_capture(ColonizeColonyPool* pool, int colony_id, int new_nation_id);

/* Fill out_roles with affordable eject roles for this colonist; returns count. */
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
 * Col1 also stores a couple of buildable *units* as `building_in_production`
 * raw codes past the real @BUILDING table's range — NAMES.TXT's @BUILDING
 * section never lists them (Artillery lives in @UNIT instead), so
 * colonies_building_type() returns NULL for these. Artillery (player-
 * requested: buildable with an Armory or an upgrade) and Wagon Train
 * (player-requested: buildable in any colony, no gate) are modeled.
 */
#define COLONIZE_UNIT_BUILD_ARTILLERY 42
#define COLONIZE_UNIT_BUILD_WAGON_TRAIN 43

/*
 * True + fills name/hammers/tools_cost if raw_code is a known unit-type
 * construction project (192 hammers / 40 tools for Artillery — golden-
 * confirmed, New Amsterdam; 40 hammers / 0 tools for Wagon Train — the
 * well-known DOS value, not independently re-derived); false for a real
 * building_type index or anything else.
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
} ColoniesBuildableOpts;

/* Fill out_ids with buildable @BUILDING indices (prerequisites applied); returns count. */
int colonies_list_buildable(
  const ColonizeColonyPool* pool,
  int colony_id,
  int* out_ids,
  int out_max,
  const ColoniesBuildableOpts* opts
);

/* Gold to finish current project (remaining hammers), or 0 if none. */
int colonies_construction_gold_cost(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
);
/* Tools still needed from warehouse for current project (0 if none/affordable). */
int colonies_construction_tools_needed(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
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
 * Buy remaining hammers with *gold (1 gold each). For a real building, also
 * try_complete. For a unit-type project (colonies_unit_build_info), only
 * tops up hammers/tools to completion threshold — the caller must follow up
 * with colonies_try_complete_unit_construction (needs a ColonizeUnitPool
 * this function doesn't take) to actually spawn.
 * Fails if no project, insufficient gold, or short tools. Updates *gold on success.
 */
bool colonies_buy_construction(ColonizeColonyPool* pool, int colony_id, int* gold);

/* Warehouse capacity per cargo type (100 base; +100 Warehouse; +100 Expansion). Food 199. */
int colonies_warehouse_capacity(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type
);

/*
 * Human unload chrome: GAME.TXT @WAREHOUSEFULL when warehouse has no room.
 * cargo_name optional (fallback "cargo"). No-op if ai_popups NULL.
 */
void colonies_emit_warehouse_full_chrome(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type,
  const char* cargo_name,
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
 * want_set cleared when stock >= warehouse capacity or boycotted.
 * Cite: viceroy_unpacked.c FUN_5952_0306; FUN_15eb_0a50 capacity.
 */
void colonies_specialty_cargo_update(
  const ColonizeColonyPool* pool,
  ColonizeColony* colony,
  int cargo_type,
  int want_set,
  int boycotted
);

/*
 * EOT spoilage: clamp each stock to warehouse capacity (FUN_15eb_0a50 /
 * FUN_15eb_0c52). Call after production + Custom House (wiki: auto-sell before
 * spoilage). Returns total units discarded. When out_first_cargo != NULL and any
 * spoil occurs, writes the first spoiled cargo index. When out_type_count != NULL,
 * writes how many distinct cargo types spoiled.
 */
int colonies_apply_warehouse_spoilage(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
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
 * Jan de Witt: transfer cargo between a transport and a *foreign* European
 * colony (stock only — no gold/price). Requires
 * founding_fathers_de_witt_allows_foreign_colony_trade for the unit's nation,
 * unit on the colony tile, foreign Euro owner, and !ai_diplo_at_war.
 * Cite: docs/fandom_col1994.md Jan de Witt. AI wagon/ship act in ai_euro §2d4.
 */
int colonies_de_witt_transfer_from_colony(
  ColonizeColonyPool* pool,
  int foreign_colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int cargo_type,
  int amount,
  const ColonizeCol1Save* col1
);
int colonies_de_witt_transfer_to_colony(
  ColonizeColonyPool* pool,
  int foreign_colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index,
  const ColonizeCol1Save* col1,
  bool* out_warehouse_full
);

/* Best cargo type for L-key load (excludes horses/tools/muskets); -1 if none. */
int colonies_best_load_cargo(const ColonizeColony* colony);

/*
 * TRADE stop cargo at own colony: honor Col1 unload/load nibble lists when
 * counts > 0; else unload-all then surplus ladder (tools…food). Returns 1 if
 * any transfer happened. Cite: ColonizeCol1TradeStop; Colonization.pdf Trade Routes.
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

void colonies_render_on_map(
  const ColonizeColonyPool* pool,
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
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
