#ifndef COLONIZE_EUROPE_H
#define COLONIZE_EUROPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/pik.h"
#include "core/ss.h"
#include "core/units.h"

#define EUROPE_CARGO_MAX 16
#define EUROPE_DOCK_MAX 8
#define EUROPE_CLASS_MAX 8
#define EUROPE_HARBOR_MAX 8
#define EUROPE_SHIP_CARGO_MAX 6 /* matches COLONIZE_UNIT_CARGO_MAX */
#define EUROPE_POOL_SIZE 3
#define EUROPE_TRAIN_MAX 24
#define EUROPE_PURCHASE_MAX 8
#define EUROPE_DOCK_MENU_MAX 12 /* GAME.TXT @ARMOPTIONS row count */

/* @ARMOPTIONS row ids, 1-based, exactly DOS's switch order. */
#define EUROPE_ARM_ROW_NO_BOARD 1
#define EUROPE_ARM_ROW_BOARD 2
#define EUROPE_ARM_ROW_TO_FRONT 3
#define EUROPE_ARM_ROW_BUY_MUSKETS 4
#define EUROPE_ARM_ROW_SELL_MUSKETS 5
#define EUROPE_ARM_ROW_BUY_TOOLS 6
#define EUROPE_ARM_ROW_SELL_TOOLS 7
#define EUROPE_ARM_ROW_BUY_HORSES 8
#define EUROPE_ARM_ROW_SELL_HORSES 9
#define EUROPE_ARM_ROW_BLESS 10
#define EUROPE_ARM_ROW_UNBLESS 11
#define EUROPE_ARM_ROW_NO_CHANGES 12

/* Quantities DOS moves per row (38fd:408b / 40ea / 4131). */
#define EUROPE_ARM_MUSKETS 50
#define EUROPE_ARM_TOOLS 100
#define EUROPE_ARM_HORSES 50

/*
 * Layout calibrated to EUROPE.PIK / original_screenshots/europe/ (320×200).
 * Transit boxes sit on the water; holds under Loading; market along the bottom.
 */
#define EUROPE_TOP_BAR_H 11 /* matches colony screen wood strip */
#define EUROPE_TOP_SEPARATOR_Y EUROPE_TOP_BAR_H

#define EUROPE_EXPECTED_X 2
#define EUROPE_EXPECTED_Y 118
#define EUROPE_EXPECTED_W 70 /* (2,118)–(72,160); shares edge with Bound */
#define EUROPE_EXPECTED_H 42
#define EUROPE_BOUND_X 72
#define EUROPE_BOUND_Y 118
#define EUROPE_BOUND_W 70 /* (72,118)–(142,160) */
#define EUROPE_BOUND_H 42
#define EUROPE_LOADING_X 144
#define EUROPE_LOADING_Y 118
#define EUROPE_LOADING_W 78 /* (144,118)–(222,160) */
#define EUROPE_LOADING_H 42
#define EUROPE_TRANSIT_HEADER_LINES 2
/* Same painted 9×12 interiors / 12px pitch as colony transport (EUROPE.PIK). */
#define EUROPE_HOLD_X 148
#define EUROPE_HOLD_Y 165
#define EUROPE_HOLD_W 9
#define EUROPE_HOLD_H 12
#define EUROPE_HOLD_PITCH 12
#define EUROPE_HOLD_MAX 6
#define EUROPE_ICON_EMPTY_HOLD 122 /* ICONS.SS — closed hold cover (colony transport) */
#define EUROPE_DOCK_X 235
#define EUROPE_DOCK_Y 138 /* was 140 — dock row sat 2px low (bugs.md) */
#define EUROPE_DOCK_PITCH 20
#define EUROPE_DOCK_UNIT_H 16
#define EUROPE_BTN_X 268
#define EUROPE_BTN_Y 48
#define EUROPE_BTN_W 48
#define EUROPE_BTN_H 14
#define EUROPE_BTN_PITCH 18
#define EUROPE_MARKET_X 0
#define EUROPE_MARKET_Y 179 /* EUROPE.PIK blue cargo strip */
#define EUROPE_MARKET_CELL 20 /* outer size; adjacent cells share the 1px border */
#define EUROPE_MARKET_PITCH 19
#define EUROPE_MARKET_H EUROPE_MARKET_CELL
#define EUROPE_CARGO_ICON_BASE 22
/* Grey (part-load) commodity icons; same split the map sidebar and the colony
 * transport pane already use — a hold shows the coloured icon only at a full
 * 100, the grey one below that (bugs.md). */
#define EUROPE_CARGO_GREY_BASE 38
#define EUROPE_CARGO_FULL 100
#define EUROPE_EXIT_X 306 /* same painted Exit as colony / EUROPE.PIK */
#define EUROPE_EXIT_Y 179
#define EUROPE_SCREEN_W 320
#define EUROPE_SCREEN_H 200

/* Transparent button bevel on EUROPE.PIK sky (dark TL / light BR). */
#define EUROPE_BTN_DARK 0x3f  /* deep blue */
#define EUROPE_BTN_LIGHT 0x31 /* pale blue */
/* Bright green under EUROPE.PIK (index 15 is white on this palette). */
#define EUROPE_TEXT_GREEN 10

/* Voyage delays — Unverified vs DOS (manual: 1–4 turns; east usually shorter). */
/* FUN_48d3_0002: a crossing takes 1 turn; 2 when RNG(1,100)>89, the nation
 * has >2 ships and does not own Magellan (FF 5). Same roll both directions
 * (48d3_007a sail-to-Europe, 48d3_0346 sail-from-Europe). */
#define EUROPE_VOYAGE_TURNS_MAX 2

typedef struct EuropeCargoQuote {
  char name[32];
  int bid; /* port pays this when you sell */
  int ask; /* you pay this when you buy */
  /* @CARGO dynamics — FUN_38fd_0058 / 1d80 / 1dfa. */
  int low;
  int high;
  int burden;
  int rise;
  int fall;
  int attrition;
  int volatility;
} EuropeCargoQuote;

/* DOS @UNIT type codes a dock unit can hold (FUN_38fd_0718 / the @ARMOPTIONS
 * menu moves an immigrant between exactly these six). The kit follows from
 * the type: Soldiers and Dragoons carry 50 Muskets, Dragoons and Scouts 50
 * Horses, Pioneers 100 Tools — DOS stores only the Tools byte and lets the
 * type imply the rest. */
#define EUROPE_DOCK_TYPE_COLONISTS 0
#define EUROPE_DOCK_TYPE_SOLDIERS 1
#define EUROPE_DOCK_TYPE_PIONEERS 2
#define EUROPE_DOCK_TYPE_MISSIONARIES 3
#define EUROPE_DOCK_TYPE_DRAGOONS 4
#define EUROPE_DOCK_TYPE_SCOUTS 5

typedef struct EuropeDockImmigrant {
  char name[40];
  int profession; /* NAMES.TXT @JOB index; -1 unknown */
  bool present;
  bool sentry; /* board next outbound ship (default true) */
  int dos_type; /* EUROPE_DOCK_TYPE_*; what the @ARMOPTIONS rows move around */
} EuropeDockImmigrant;

typedef struct EuropeRecruitClass {
  char name[40];
  int cost; /* @CLASS transport table (RE reference; dialog uses passage_gold) */
} EuropeRecruitClass;

typedef struct EuropePoolSlot {
  char name[40];
  int profession; /* @JOB index */
  bool filled;
} EuropePoolSlot;

typedef struct EuropeTrainOption {
  char expert_name[40];
  int job_index;
  int cost;
} EuropeTrainOption;

typedef struct EuropePurchaseOption {
  char name[32];
  int gold;
  bool is_ship; /* false = artillery → docks */
} EuropePurchaseOption;

typedef struct EuropeHarborShip {
  int type_index; /* into ColonizeUnitPool types; -1 until resolved by name */
  char name[32];
  int cargo_types[EUROPE_SHIP_CARGO_MAX]; /* passenger unit type indices */
  int cargo_professions[EUROPE_SHIP_CARGO_MAX]; /* @JOB per passenger */
  /*
   * Per-passenger Treasure gold for Europe cash-in (0 = unknown / not treasure).
   * Source: COL1 Treasure unit hold_goods_amount[0..1] LE16 gold, captured by
   * game_loop.c's game_europe_capture_pax_treasure_gold and filled onto the
   * newest Expected slot by game_europe_fill_expected_treasure_gold on both
   * H/sail-to-Europe and Return-to-Europe paths (2026-08 — stale "does not
   * fill this yet" wording removed; cash-in itself is europe_cash_treasure /
   * europe_cash_treasure_passengers below).
   */
  int cargo_treasure_gold[EUROPE_SHIP_CARGO_MAX];
  int cargo_count;
  int hold_goods_type[EUROPE_SHIP_CARGO_MAX];
  int hold_goods_amount[EUROPE_SHIP_CARGO_MAX];
  int turns_left; /* 0 when in harbor; >0 while in transit */
  /*
   * Set the moment a ship enters a transit lane, cleared (instead of
   * decrementing turns_left) by the first europe_tick_voyages that sees it.
   * The turn a ship sails is a turn at sea: in DOS the lane counter written
   * by FUN_48d3_0002 is 1 for an ordinary crossing, yet ordering a Caravel
   * home and pressing End Turn twice is what actually docks it (player-
   * verified) — the departure itself happens inside the same end-of-turn
   * pass that has already run its FUN_48d3_03d0 decrement step, so the
   * counter's first tick lands on the *next* turn. Modelling that here
   * keeps turns_left holding DOS's own 1/2 for the save's turns_worked
   * byte instead of inflating it. Runtime only; a save taken between the
   * order and the next tick reloads without the grace turn.
   */
  bool departed_this_turn;
  int exit_x;
  int exit_y;
  bool exit_east; /* true = left via east edge (usually shorter) */
  /*
   * Trade-route automation (bugs.md: "A ship trying to go to Europe via
   * trade route just goes to the sea lane tile, then returns. Never visits
   * Europe."): 0 = not on a route; else route slot + 1. trade_stop is the
   * stop index the ship is travelling to service (going east: the Europe
   * stop; going west after service: the next stop, re-armed as TRADE_ROUTE
   * orders when the ship spawns back on the map). Zero-init = none.
   */
  int trade_route_plus1;
  int trade_stop;
} EuropeHarborShip;

typedef enum EuropeHit {
  EUROPE_HIT_NONE = 0,
  EUROPE_HIT_HARBOR_SHIP,
  EUROPE_HIT_HOLD,
  EUROPE_HIT_MARKET,
  EUROPE_HIT_BTN_RECRUIT,
  EUROPE_HIT_BTN_PURCHASE,
  EUROPE_HIT_BTN_TRAIN,
  EUROPE_HIT_DOCK,
  EUROPE_HIT_EXPECTED,
  EUROPE_HIT_BOUND,
  EUROPE_HIT_EXIT
} EuropeHit;

typedef struct EuropeHitResult {
  EuropeHit kind;
  int index;
} EuropeHitResult;

typedef enum EuropeMenu {
  EUROPE_MENU_NONE = 0,
  EUROPE_MENU_RECRUIT,
  EUROPE_MENU_TRAIN,
  EUROPE_MENU_PURCHASE,
  EUROPE_MENU_DOCK
} EuropeMenu;

/*
 * Europe / home-port screen: market, docks, harbor + transit lanes,
 * recruit pool / train / purchase, buy/sell helpers.
 */
typedef struct EuropeScreen {
  ColonizePikImage background;
  bool background_ok;
  ColonizeSpriteSheet wood_tile; /* WOODTILE.SS remapped to Europe palette */
  bool wood_tile_ok;
  char port_city[48];
  char nation_name[48];
  char colony_region[48]; /* @COLONYNAME — "Bound For …" */
  int gold;
  int tax_percent;
  /*
   * Crosses meter = DOS Europe +0x2e / +0x30 (same words as immigration pressure).
   * needed = FUN_38fd_584a score each EOT; idle +2 until first dock immigrant;
   * then church crosses only; spawn when current > needed.
   * Cite: europe_nation_eot.md; TURN1–7 goldens.
   */
  uint16_t current_crosses;
  uint16_t needed_crosses;
  bool crosses_immigrant_seen; /* true after at least one dock immigrant */
  uint16_t liberty_bells_total;
  uint16_t liberty_bells_last_turn;
  EuropeCargoQuote cargo[EUROPE_CARGO_MAX];
  int cargo_count;
  /* FUN_38fd trade.nr[16] stand-in — volume traffic for rise/fall thresholds. */
  int16_t trade_nr[EUROPE_CARGO_MAX];
  EuropeRecruitClass classes[EUROPE_CLASS_MAX];
  int class_count;
  EuropeDockImmigrant dock[EUROPE_DOCK_MAX];
  int dock_count;
  EuropeHarborShip harbor[EUROPE_HARBOR_MAX];
  int harbor_ships;
  EuropeHarborShip expected[EUROPE_HARBOR_MAX];
  int expected_ships;
  EuropeHarborShip bound[EUROPE_HARBOR_MAX];
  int bound_ships;
  int selected_harbor; /* -1 none; index into harbor[] */
  int selected_market; /* cargo type highlight */
  EuropePoolSlot pool[EUROPE_POOL_SIZE];
  int recruit_passage; /* current dialog gold; see europe_compute_recruit_passage */
  /*
   * DOS Europe+6: recruit count this era, capped 180 (0xb4). Bumped only by
   * a real interactive Recruit (FUN_38fd_4884 tail, param_1==0&&param_2==0)
   * — NOT by crosses-driven dock immigrants (separate 0718 harbor-spawn
   * path). Cite: viceroy_unpacked.c 64778-64784.
   */
  uint8_t recruit_count;
  /* Cached 0-8 clamp of col1->head.difficulty (0x53a6); refreshed each EOT
   * tick so europe_recruit_from_pool can recompute passage without a col1
   * pointer. */
  uint8_t difficulty;
  EuropeTrainOption train[EUROPE_TRAIN_MAX];
  int train_count;
  EuropePurchaseOption purchase[EUROPE_PURCHASE_MAX];
  int purchase_count;
  EuropeMenu menu;
  int menu_selection; /* 0 = None / cancel for list menus */
  int menu_dock_index;
  /*
   * GAME.TXT @ARMOPTIONS rows for the clicked dock immigrant, built by
   * europe_build_dock_menu. DOS omits a row it has disabled rather than
   * greying it, and greys one the player cannot afford, so `count` is the
   * number of rows actually shown and `row[]` remembers each one's 1-based
   * DOS row id — the id the action switch (and DOS's own dialog) works in.
   */
  char dock_menu_label[EUROPE_DOCK_MENU_MAX][72];
  uint8_t dock_menu_row[EUROPE_DOCK_MENU_MAX];
  bool dock_menu_greyed[EUROPE_DOCK_MENU_MAX];
  int dock_menu_count;
  int last_exit_x;
  int last_exit_y;
  bool last_exit_east;
  bool last_exit_valid;
  /* This EOT tick's rise/fall events (@PRICEUP/@PRICEDOWN, FUN_38fd_0058
   * phase 4): DOS calls FUN_281f_0652(0xfa8/0xfb0) inline, once per cargo
   * that crosses its threshold, inside the same 0..15 loop — so two
   * different cargos changing the same turn both get their own dialog.
   * turn.c walks price_event_cargo[0..price_event_count) in order and
   * queues one OK popup per entry (dir +1 rose / -1 fell); collapsing this
   * to a single "last event" dropped every popup but the last. */
  int price_event_cargo[EUROPE_CARGO_MAX];
  int price_event_dir[EUROPE_CARGO_MAX];
  int price_event_count;
  bool open_on_dock; /* set when Expected→Harbor this tick */
  /*
   * FUN_48d3_08bf's `local_a` gate: a ship that docked this tick was carrying
   * goods, so the caller (human turn only) fires woodcut 9, CARGO FROM THE
   * NEW WORLD. Reset by every europe_tick_voyages call, like open_on_dock.
   */
  bool docked_with_goods;
  /* William Brewster: exclude Petty Criminals / Indentured Servants from pool. */
  bool brewster_no_criminals;
  /*
   * FUN_38fd_5e52 / 584a immigration pressure: +0x30 score, +0x2e accumulate.
   * Cite: europe_nation_eot.md phase 4–5.
   */
  /* Mirrors of needed_crosses / current_crosses after each 584a tick (compat). */
  int16_t immigration_score;
  int16_t immigration_pressure;
  /*
   * FUN_364b_0688 O — per-nation Europe horses word / musket×50 batches
   * (AI dump-sell). Cite: colony_eot_production.md.
   */
  uint16_t nation_horses[4];
  uint16_t nation_musket_batches[4];
  /*
   * Human nation's boycott state — mirrors ColonizeCol1Nation.boycott_bitmap
   * (ai_king.c tea-party / ai_diplo.c wartime embargo write it; this is a
   * read-only copy refreshed each frame the Europe screen renders, since
   * europe.c can't see ColonizeCol1Save directly). Bit c set = cargo type c
   * blocked from Europe trade until the boycott is lifted. Source: fandom
   * Boycott (Col) — "goods blocked in Europe until penalty paid or Fugger";
   * Custom House bypasses this (europe_custom_house_autosell intentionally
   * does not check it).
   */
  uint16_t boycott_bitmap;
  /* Mirrors ColonizeCol1Nation.artillery_count (nation+0x1e) for the human —
   * DOS FUN_38fd_4b50 prices Artillery (type 0xb) at base + count*100 and
   * bumps the count on every purchase. Synced by col1_bridge apply/capture. */
  int artillery_bought;
  char status[160];
} EuropeScreen;

bool europe_load(EuropeScreen* eu, const char* data_dir, char* err, size_t err_size);
void europe_free(EuropeScreen* eu);
void europe_reset_campaign(EuropeScreen* eu);
void europe_reset_campaign_nation(EuropeScreen* eu, int nation);
/* Port / region / old-world nation from NAMES.TXT (@HOMEPORT / @COLONYNAME).
 * Does not wipe harbor, dock, or gold. nation clamped 0..3. */
void europe_set_nation(EuropeScreen* eu, int nation, const struct ColonizeMsgCatalog* names);

/*
 * FUN_48d3_0002 voyage roll. rng NULL → 1 (no roll). The x<3 west-edge
 * branch in DOS only burns RNG(0,1)+an FF test and discards both — there
 * is no west-edge sail penalty in the shipped code (PEDIA's Magellan
 * "west edge" line describes the 10% delay this FF removes).
 */
int europe_voyage_turns_roll(struct ColonizeDosRng* rng, bool magellan, int ship_count);

/*
 * DOS `FUN_38fd_4884` real Recruit passage formula (was a linear
 * start-100/+16-per-recruit placeholder — see manual_gap.md). base =
 * (recruit_count+difficulty+7)*20; floor = max(base/5,100); discount =
 * (base-floor)*current_crosses / -(needed_crosses+1) [`FUN_1d1d_0ec6`
 * signed division; the +1 is the DOS divide-by-zero guard when
 * needed_crosses==0]; passage = max(10, base+discount) — cheaper the
 * closer current_crosses is to the next free immigrant.
 * Cite: viceroy_unpacked.c 64682-64694; europe_nation_eot.md "Phase 5".
 */
int europe_compute_recruit_passage(
  int recruit_count, int difficulty, int current_crosses, int needed_crosses
);

bool europe_recruit_from_pool(EuropeScreen* eu, int pool_index);
/* FUN_38fd_4884(1,0): pool pick at no passage, no recruit-count bump (Fountain of Youth). */
bool europe_recruit_free_from_pool(EuropeScreen* eu, int pool_index);
/*
 * Crosses / unrest: move one pool slot to docks; refill. DOS `5e52` phase 5
 * picks the slot via `FUN_281f_04d4` RNG(0,2) before rerolling it, not
 * always pool[0] — pass `rng` to match; NULL falls back to first-filled
 * (fixture / no-rng callers). Cite: europe_nation_eot.md "Phase 5".
 */
bool europe_immigrant_from_pool(EuropeScreen* eu, struct ColonizeDosRng* rng);
/* Brewster (FF 20) arrival: FUN_38fd_4884(0,1) pick applied — free
 * dock transfer of pool[pool_index], then crosses zeroed (no +6 bump). */
bool europe_brewster_pick_from_pool(EuropeScreen* eu, int pool_index);
/* Brewster owned (FUN_4345_0342 case 0x14): criminal/servant pool slots are
 * overwritten with Free Colonists in place — a substitution, not a reroll. */
void europe_apply_brewster(EuropeScreen* eu, int owned);
void europe_refill_pool_slot(EuropeScreen* eu, int slot, unsigned* rng_state);
/* Shared recruit-choice source: DOS FUN_38fd_4884 draws the same three pool
 * slots for the Recruit menu, the Brewster @RECRUITCHOOSE pick and the
 * Fountain of Youth @RECRUIT picks. Ensure fills empty slots; label is the
 * row text every one of those lists must use. */
void europe_pool_ensure_filled(EuropeScreen* eu);
const char* europe_pool_label(const EuropeScreen* eu, int slot);

bool europe_train(EuropeScreen* eu, int train_index);
bool europe_purchase(EuropeScreen* eu, int purchase_index);

/* Live cost of a purchase-menu row — the table price plus DOS's Artillery
 * escalation (FUN_38fd_4b50: +100 gold per Artillery already purchased,
 * eu->artillery_bought = nation+0x1e). Returns -1 on a bad index. */
int europe_purchase_cost(const EuropeScreen* eu, int purchase_index);

/*
 * Push a save-loaded Europe-dock colonist straight onto the dock (append at
 * back, present, default sentry) — for col1_bridge_apply restoring a human
 * nation's waiting-in-Europe colonists on load. Unlike europe_recruit_from_
 * pool/europe_train/europe_purchase this charges no gold and posts no status
 * message. Returns false if the dock is full.
 */
bool europe_dock_push_load(EuropeScreen* eu, const char* name, int profession);

/*
 * Drop the (236,236) Europe-map mirror unit that shadows a dock immigrant
 * (turn.c immigrant spawn / col1_bridge load create one per dock entry so
 * Col1 capture keeps the colonist). Prefers the matching profession.
 */
void europe_remove_dock_mirror_unit(ColonizeUnitPool* units, int nation_id, int profession);

struct ColonizeMsgCatalog;

/*
 * Build the @ARMOPTIONS row list for dock[dock_index] into eu->dock_menu_*.
 * Cite: DOS FUN_38fd_37xx — prices and quantities at 38fd:3745..3830, the
 * per-row enable switch at 38fd:388e..3a04, and the add/grey tail at
 * 38fd:3a32..3a7b (a disabled row is not added at all; an unaffordable one
 * is added greyed).
 */
void europe_build_dock_menu(
  EuropeScreen* eu,
  const struct ColonizeMsgCatalog* messages,
  int dock_index
);

/*
 * Apply one @ARMOPTIONS row id (EUROPE_ARM_ROW_*) to dock[dock_index].
 * `units` may be NULL; when given, the (236,236) mirror unit follows.
 * Cite: DOS's action switch at 38fd:3ade..3c49.
 */
bool europe_apply_dock_menu_row(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int nation_id,
  int dock_index,
  int row
);

/*
 * DOS FUN_38fd_0718 (the Europe harbor spawn behind every dock arrival):
 * the @UNIT type a dock immigrant of this @JOB profession is created as.
 * Pioneer (0x14) -> Pioneers, Missionary (0x18) -> Missionaries, Scout
 * (0x16) -> Scouts, Soldier (0x15) -> Soldiers, or Dragoons on a
 * `rng(0, bound + 4) == 0` roll where bound is the difficulty for a human
 * nation and 1 otherwise; anything else -> Colonists. Returns the DOS type
 * code 0..5, which europe_dock_unit_type_index maps to a pool type.
 * Pass rng = NULL to skip the Dragoon roll (plain Soldiers).
 */
int europe_dock_unit_dos_type(int profession, int difficulty, bool human, struct ColonizeDosRng* rng);

/* Pool type_index for a DOS @UNIT type code 0..5, or -1. */
int europe_dock_unit_type_index(const ColonizeUnitPool* units, int dos_type);

/*
 * DOS @UNIT type for a dock entry. A name that is itself one of the six
 * (an arriving passenger keeps the type it sailed with) wins; otherwise the
 * profession decides, the way FUN_38fd_0718 decides it for a fresh
 * immigrant. Anything else — Artillery, a purchased hull — is Colonists.
 */
int europe_dock_type_for(const char* name, int profession);

/*
 * @UNIT display type for a dock entry, for unit_chrome's box corner. Reads
 * dos_type (what @ARMOPTIONS moves around), not the profession name, so an
 * armed/mounted immigrant gets its own corner. -1 only without a pool.
 */
int europe_dock_display_type_index(
  const ColonizeUnitPool* units, const EuropeDockImmigrant* d
);

/* Kit implied by a dock entry's type, for the mirror unit and for landing. */
int europe_dock_type_tools(int dos_type);
int europe_dock_type_muskets(int dos_type);
int europe_dock_type_horses(int dos_type);

/*
 * Spawn (or re-kit) the Europe-map mirror unit for a dock immigrant, exactly
 * as FUN_38fd_0718 does: right @UNIT type, orders = Sentry (DOS +0x314c = 1),
 * profession stamped, and 100 Tools on a Pioneers-type unit (DOS +0x3159 =
 * 100) — without which a Hardy Pioneer reached the New World empty-handed
 * (bugs.md). Returns the spawned unit id, or -1.
 */
int europe_spawn_dock_mirror_unit(
  ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int difficulty,
  bool human,
  struct ColonizeDosRng* rng
);

/* The FUN_38fd_0718 kit for an already-spawned unit of a known DOS type. */
void europe_apply_dock_unit_kit(ColonizeUnit* u, int dos_type);

/* Move the (236,236) mirror unit behind a dock entry to a new @UNIT type. */
void europe_retype_dock_mirror_unit(
  ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int from_dos_type,
  int to_dos_type
);
bool europe_pop_dock_immigrant(EuropeScreen* eu, char* out_name, size_t out_name_size);
/* Pop with profession; returns false if empty. */
bool europe_pop_dock_immigrant_ex(
  EuropeScreen* eu,
  char* out_name,
  size_t out_name_size,
  int* out_profession
);

bool europe_harbor_push(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
);

/* Map→Europe: enqueue Expected Soon (not instant harbor). */
bool europe_enqueue_expected(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  const int* cargo_professions,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount,
  int exit_x,
  int exit_y,
  bool exit_east,
  int voyage_turns
);

/* Harbor→Bound for New World; auto-boards sentry dockers into free holds. */
bool europe_set_sail_from_harbor(
  EuropeScreen* eu,
  int harbor_index,
  int voyage_turns,
  ColonizeUnitPool* units,
  int nation_id
);

/* Expected↔Bound reverse (keeps remaining turns). */
bool europe_reverse_transit(EuropeScreen* eu, bool from_expected, int index);

bool europe_harbor_pop(
  EuropeScreen* eu,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max,
  int* out_hold_goods_type,
  int* out_hold_goods_amount,
  int hold_max
);

/* Pop oldest Bound ship that has arrived (turns_left==0 after tick). */
bool europe_bound_pop_arrived(
  EuropeScreen* eu,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max,
  int* out_hold_goods_type,
  int* out_hold_goods_amount,
  int hold_max,
  int* out_exit_x,
  int* out_exit_y,
  bool* out_exit_east
);

void europe_refresh_harbor_selection(EuropeScreen* eu);

/*
 * Decrement transit; move Expected→Harbor when due (passengers → dock front);
 * leave Bound at 0 for caller to spawn. Sets open_on_dock when a ship docks.
 */
void europe_tick_voyages(EuropeScreen* eu, const ColonizeUnitPool* units);

/*
 * Treasure Train cash-in on Europe arrival.
 * Cite: Colonization.pdf Treasure Trains; GAME.TXT @LOOTCASH (Crown takes
 * NUMBER1% share, remainder to treasury); @KINGGALLEON3 (Cortes: share =
 * current tax rate). Fee = eu->tax_percent — same Crown cut as
 * europe_sell_proceeds. KINGGALLEON2 non-Cortes share lives in
 * units_king_galleon_share_pct (FUN_5fef_1908), not here.
 * Returns gold credited (0 if value <= 0).
 */
int europe_cash_treasure(EuropeScreen* eu, int treasure_value);

/*
 * True when cargo_type is set in eu->boycott_bitmap (Parliament boycott
 * still active — see EuropeScreen.boycott_bitmap). Out-of-range cargo_type
 * reads as not boycotted.
 */
int europe_cargo_boycotted(const EuropeScreen* eu, int cargo_type);

/*
 * FUN_38fd_2dfe: pay back taxes to lift a Parliamentary boycott on
 * cargo_type. Cost = eu->cargo[cargo_type].ask * 500 ("500 tons of that
 * good" — fandom Boycott (Col); GAME.TXT @KISSUP). On success: deducts cost
 * from gold, credits it to nation.royal_money (Crown REF budget — the DOS
 * write really does land on that field, see col1_save.h), clears the
 * boycott bit. Insufficient funds / not boycotted / bad args: no-op,
 * returns 0. Real trigger: GAME.TXT @SOMEBOYCOTT — click the boycotted
 * cargo cell on the Europe market strip (game_loop.c EUROPE_HIT_MARKET).
 * @KISSUP/@KISSSORRY CHOICE dialog chrome PARKED — ported as immediate
 * action + eu->status line. Returns gold paid (>0) on success.
 */
int europe_buyback_boycott(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int human_nation, int cargo_type
);

/*
 * DOS price accessors (FUN_38fd_0040 / FUN_38fd_0016). `bid` stores the
 * save-canonical `euro_price` word (nation +0x4c); the port PAYS
 * `euro_price − 1` when you sell and CHARGES `euro_price + burden` when you
 * buy (`ask`). Real DOS Europe screen at 1494: Food 0/8, Lumber 1/6,
 * Silver 19/20 — see original_screenshots/europe/main_with_caravel_*.png.
 */
void europe_set_sound_hook(void (*play_fn)(int id));
void europe_set_bgm_hook(void (*set_bgm_fn)(int pool));
void europe_notify_immigrant_sound(EuropeScreen* eu);
int europe_sell_price(const EuropeScreen* eu, int cargo_type);
int europe_buy_price(const EuropeScreen* eu, int cargo_type);
/* gross − gross·tax/100 (FUN_364b_0688 Custom House arm; same rounding as
 * the harbor sale). Returns the net treasury credit. */
int europe_net_after_tax(int gross, int tax_percent);
int europe_sell_proceeds(const EuropeScreen* eu, int cargo_type, int amount);
int europe_sell_hold(EuropeScreen* eu, int harbor_index, int hold_index);
/*
 * FUN_38fd_1dfa (sell) / FUN_38fd_1d80 (buy) volume ledger, exact:
 *   term = (amount << volatility) + 1d44(amount)
 *   1d44 = (difficulty − 2)·16·amount/100 when the seller is human,
 *          −32·amount/100 for an AI seller (C truncation toward zero);
 *   every nation's nr[cargo] += term (buy: −=), the Dutch record (slot 3)
 *   gets (term·2)/3; seller's tons/tons2 += amount (buy: −=), and gold[cargo]
 *   += price·amount·(100−tax)/100.
 * Only the human's record is live in `eu->trade_nr`; `col1` (optional) gets
 * the seller's tons/tons2/gold ledgers. Verified 2026-08-28 against the
 * dutch2 t169→t170 pair: three lumber sellers (54 human @ Viceroy, 12 + 18
 * AI) → +93 on every non-Dutch nr[5], +61 on the Dutch one.
 * `immediate_threshold` runs the FUN_38fd_0058(0, cargo) single-cargo
 * rise/fall step the harbor buy/sell path calls afterwards; the Custom
 * House / AI dump-sell arms do NOT call it (they only get the EOT tick).
 */
void europe_apply_trade_volume(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int human_nation,
  int cargo_type,
  int amount,
  int is_buy,
  int immediate_threshold
);
/* Harbor buy/sell wrapper: human seller, immediate FUN_38fd_0058 step. */
void europe_apply_volume_price(EuropeScreen* eu, int cargo_type, int amount, int is_buy);
/*
 * FUN_38fd_0058 EOT peel (param_2 < 0): optional col1/colonies apply colony
 * ledger → price_group_state half (DS:0x53ea); phases 2–3 nudge trade_nr
 * (Europe +0x5c pressure) for cargos 9..12 (*100) and 1..4 (no *100); then
 * nr += attrition per cargo and rise/fall ±1 within [low,high].
 * Cite: viceroy_unpacked.c FUN_38fd_0058; turn/europe_nation_eot.md.
 */
void europe_tick_market_prices(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  struct ColonizeColonyPool* colonies,
  int human_nation,
  uint32_t turn
);
/*
 * FUN_38fd_584a score: (pop+units)<<1 if <4000, +8, cap 4000;
 * AI ((8-diff)*score)>>3; English (nation 0) *2/3.
 */
int europe_compute_immigration_score(
  const struct ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const struct ColonizeCol1Save* col1,
  int nation_id
);
/*
 * FUN_38fd_584a / 5e52 phases 4–5: needed_crosses = score; idle +2 until first
 * dock immigrant; spawn when current > needed (+0x2e/+0x30). Returns 1 if spawned.
 * Caller adds church crosses to current_crosses first.
 * Cite: europe_nation_eot.md; TURN1–7 goldens.
 */
/* Returns 1 when an immigrant was moved to the docks, 2 when Brewster is
 * owned and the caller must offer the @RECRUITCHOOSE pick instead
 * (units_brewster_enqueue_pick / europe_brewster_pick_from_pool), else 0. */
int europe_tick_immigration_pressure(
  EuropeScreen* eu,
  const struct ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const struct ColonizeCol1Save* col1,
  int nation_id,
  struct ColonizeDosRng* rng
);
/*
 * Sell one commodity hold from a map/transport ColonizeUnit into eu->gold.
 * No harbor UI — proceeds via europe_sell_proceeds (bid × amount × (100−tax)/100).
 * Cite: Colonization.pdf Europe buy/sell + tax; same Crown cut as harbor
 * europe_sell_hold / GAME.TXT tax rate path. Clears the hold on success.
 * Returns gold credited (0 if empty/invalid).
 */
int europe_sell_unit_hold(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index
);

/* Forward decls — avoid pulling colony/col1 into every europe consumer. */
struct ColonizeColonyPool;
struct ColonizeColony;
struct ColonizeCol1Save;
struct ColonizeDosRng;

/*
 * FUN_364b_0688 Custom House auto-sell (colony EOT after production).
 * Requires Custom House building. Per cargo: mask (0=all eligible) +
 * FUN_364b_0636 denylist (not Food/Lumber/Horses/Tools/Muskets) + stock>99
 * → sell stock-50 (leave 50). Boycott does not block. Tax via eu tax /
 * nation tax_rate unless WoI (col1 head.unknown46[0]). Credits
 * col1->nation[n].gold; also eu->gold when n==human_nation.
 * Returns total gold credited. PARK: per-cargo UI chrome (FUN_15eb_0326).
 */
int europe_custom_house_autosell(
  EuropeScreen* eu,
  struct ColonizeColonyPool* pool,
  struct ColonizeColony* colony,
  struct ColonizeCol1Save* col1,
  int human_nation
);

/*
 * FUN_364b_0688 phase O — AI / non-human Euro dump-sell before spoilage.
 * For cargo 1..15 with stock > warehouse cap: credit gold for surplus via
 * bid×amount×(100−tax)/100 (nation tax_rate; no WoI skip — matches 1dfa),
 * apply volume price, leave stock for spoilage clamp. Horses: DOS transfers
 * surplus to Europe horses word (no gold); muskets in 50-batches then sell
 * remainder. Cite: colony_eot_production.md O.
 * Muskets: DOS batches of 50 → Europe musket counter then sell remainder —
 * thin sells full surplus (counter PARKED). Returns total gold credited.
 * Cite: viceroy_unpacked.c ~57806–57848; turn/colony_eot_production.md.
 */
int europe_ai_colony_dump_sell(
  EuropeScreen* eu,
  struct ColonizeColonyPool* pool,
  struct ColonizeColony* colony,
  struct ColonizeCol1Save* col1,
  int human_nation
);

/*
 * FUN_364b_0636 Custom House / export denylist: not Food, Lumber, Horses, Tools,
 * Muskets. Used by autosell and AI peace Europe export sail.
 */
int europe_cargo_export_eligible(int cargo_type);

/*
 * Is `cargo_type` currently toggled on in this colony's Custom House
 * per-cargo mask (europe_custom_house_autosell's own enable check,
 * exposed read-only) — bits==0 (nothing configured) reads as "no cargo
 * enabled", matching autosell's own behavior. Colony-screen cargo strip
 * uses this to color a cargo's stock number (green = will be auto-sold
 * this EOT, matching the DOS golden) — the "per-cargo UI chrome" this
 * header's europe_custom_house_autosell comment had PARKed.
 */
bool europe_custom_house_cargo_enabled(uint16_t custom_house_bits, int cargo_type);

int europe_buy_cargo(EuropeScreen* eu, int harbor_index, int cargo_type, int amount);
/*
 * Trade-route load list at a Europe stop: buy up to `amount` (≤100) of
 * cargo_type straight into a map/transport unit's holds. Flat ask price,
 * boycott gated. Cite: DOS FUN_479b_0bd0 load phase → FUN_38fd_1fa2.
 * Returns units bought (0 = no gold / boycott / no room).
 */
int europe_buy_unit_cargo(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int unit_id,
  int cargo_type,
  int amount
);
int europe_best_sell_hold(const EuropeScreen* eu, int harbor_index);

EuropeHitResult europe_hit_test(const EuropeScreen* eu, int mx, int my);

/*
 * Like europe_hit_test, but Expected/Bound resolve the ship icon under the pointer
 * when units + icons are provided (matches Loading/Expected/Bound render layout).
 * transit_line_h is font line height used for the two-line header (default 8).
 */
EuropeHitResult europe_hit_test_ex(
  const EuropeScreen* eu,
  int mx,
  int my,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int transit_line_h
);

/* Ship icon index under (mx,my) inside a transit box, or -1. */
int europe_transit_ship_at(
  const EuropeHarborShip* ships,
  int count,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int box_x,
  int box_y,
  int box_w,
  int box_h,
  int transit_line_h,
  int mx,
  int my
);

void europe_menu_open(EuropeScreen* eu, EuropeMenu menu);
void europe_menu_close(EuropeScreen* eu);
/* Apply current menu_selection (0 = cancel). Returns true if acted. */
bool europe_menu_confirm(EuropeScreen* eu);

/* Apply the highlighted dock-menu row; `units` keeps the mirror unit in step. */
bool europe_dock_menu_apply_selection(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int nation_id
);

void europe_cheat_add_gold(EuropeScreen* eu, int amount);
void europe_cheat_adjust_tax(EuropeScreen* eu, int delta);

/* Legacy name — opens recruit menu. */
bool europe_recruit(EuropeScreen* eu);

#endif
