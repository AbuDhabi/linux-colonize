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
#define EUROPE_HOLD_X 146
#define EUROPE_HOLD_Y 163
#define EUROPE_HOLD_W 12
#define EUROPE_HOLD_H 14
#define EUROPE_HOLD_PITCH 12
#define EUROPE_HOLD_MAX 6
#define EUROPE_ICON_EMPTY_HOLD 122 /* ICONS.SS — closed hold cover (colony transport) */
#define EUROPE_DOCK_X 235
#define EUROPE_DOCK_Y 140
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
#define EUROPE_VOYAGE_EAST_TURNS 2
#define EUROPE_VOYAGE_WEST_TURNS 4
#define EUROPE_RECRUIT_PASSAGE_START 100
#define EUROPE_RECRUIT_PASSAGE_STEP 16

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

typedef struct EuropeDockImmigrant {
  char name[40];
  int profession; /* NAMES.TXT @JOB index; -1 unknown */
  bool present;
  bool sentry; /* board next outbound ship (default true) */
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
   * Intended source: COL1 Treasure unit cargo_hold[0..1] LE16 gold (not yet on
   * ColonizeUnit; game_loop enqueue does not fill this yet — PARK).
   */
  int cargo_treasure_gold[EUROPE_SHIP_CARGO_MAX];
  int cargo_count;
  int hold_goods_type[EUROPE_SHIP_CARGO_MAX];
  int hold_goods_amount[EUROPE_SHIP_CARGO_MAX];
  int turns_left; /* 0 when in harbor; >0 while in transit */
  int exit_x;
  int exit_y;
  bool exit_east; /* true = left via east edge (usually shorter) */
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
  uint16_t current_crosses;
  uint16_t needed_crosses;
  /* After first dock immigrant: defer needed+1 one turn; stop base +2 crosses. */
  bool crosses_immigrant_seen;
  bool crosses_pending_needed_bump;
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
  int recruit_passage; /* current dialog gold */
  EuropeTrainOption train[EUROPE_TRAIN_MAX];
  int train_count;
  EuropePurchaseOption purchase[EUROPE_PURCHASE_MAX];
  int purchase_count;
  EuropeMenu menu;
  int menu_selection; /* 0 = None / cancel for list menus */
  int menu_dock_index;
  int last_exit_x;
  int last_exit_y;
  bool last_exit_east;
  bool last_exit_valid;
  bool open_on_dock; /* set when Expected→Harbor this tick */
  /* William Brewster: exclude Petty Criminals / Indentured Servants from pool. */
  bool brewster_no_criminals;
  /*
   * FUN_38fd_5e52 / 584a immigration pressure: +0x30 score, +0x2e accumulate.
   * Cite: europe_nation_eot.md phase 4–5.
   */
  int16_t immigration_score;
  int16_t immigration_pressure;
  /*
   * FUN_364b_0688 O — per-nation Europe horses word / musket×50 batches
   * (AI dump-sell). Cite: colony_eot_production.md.
   */
  uint16_t nation_horses[4];
  uint16_t nation_musket_batches[4];
  char status[160];
} EuropeScreen;

bool europe_load(EuropeScreen* eu, const char* data_dir, char* err, size_t err_size);
void europe_free(EuropeScreen* eu);
void europe_reset_campaign(EuropeScreen* eu);
void europe_reset_campaign_nation(EuropeScreen* eu, int nation);

/* Voyage turns (east shorter). ship_movement ≥6 shaves one turn (Unverified). */
int europe_voyage_turns(bool exit_east, int ship_movement);

bool europe_recruit_from_pool(EuropeScreen* eu, int pool_index);
/* Crosses / unrest: move pool[0] (or first filled) to docks; refill. */
bool europe_immigrant_from_pool(EuropeScreen* eu);
void europe_refill_pool_slot(EuropeScreen* eu, int slot, unsigned* rng_state);

bool europe_train(EuropeScreen* eu, int train_index);
bool europe_purchase(EuropeScreen* eu, int purchase_index);

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
  int ship_movement
);

/* Harbor→Bound for New World; auto-boards sentry dockers into free holds. */
bool europe_set_sail_from_harbor(
  EuropeScreen* eu,
  int harbor_index,
  int ship_movement,
  const ColonizeUnitPool* units
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
 * europe_sell_proceeds. PARK: KINGGALLEON2 non-Cortes royal-galleon "extra"
 * share beyond tax (no % in GAME.TXT / PDF / fandom).
 * Returns gold credited (0 if value <= 0).
 */
int europe_cash_treasure(EuropeScreen* eu, int treasure_value);

int europe_sell_proceeds(const EuropeScreen* eu, int cargo_type, int amount);
int europe_sell_hold(EuropeScreen* eu, int harbor_index, int hold_index);
/*
 * FUN_38fd_0058 peel after buy/sell: apply nr ± (amount<<volatility), then
 * rise/fall ±1 bid within [low,high]; ask = bid+burden+1.
 * sign_for_buy: buy → negative volume (1d80), sell → positive (1dfa).
 * Cite: viceroy_unpacked.c FUN_38fd_1d80/1dfa/0058; NAMES.TXT @CARGO.
 */
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
  struct ColonizeColonyPool* colonies
);
/*
 * FUN_38fd_584a / 5e52 phase 4 thin: immigration_score from colony pop + units;
 * accumulate into immigration_pressure. Returns 1 if a dock immigrant spawned.
 * Cite: europe_nation_eot.md.
 */
int europe_tick_immigration_pressure(
  EuropeScreen* eu,
  const struct ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const struct ColonizeCol1Save* col1,
  int nation_id
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

int europe_buy_cargo(EuropeScreen* eu, int harbor_index, int cargo_type, int amount);
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

void europe_cheat_add_gold(EuropeScreen* eu, int amount);
void europe_cheat_adjust_tax(EuropeScreen* eu, int delta);

/* Legacy name — opens recruit menu. */
bool europe_recruit(EuropeScreen* eu);

#endif
