#ifndef COLONIZE_EUROPE_H
#define COLONIZE_EUROPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/pik.h"
#include "core/ss.h"
#include "core/units_cargo.h"
#include "core/world.h"

#define EUROPE_CARGO_MAX 16
/* Europe status-line ring (DOS DS:0x2d54 lines produced by a sale). */
#define EUROPE_BAR_EVENT_MAX 8
#define EUROPE_BAR_EVENT_LEN 96
/* EUROPE_DOCK_MAX -- see docs/europe.md#europe_dock_max */
#define EUROPE_DOCK_MAX 32
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
/* EUROPE_DOCK_X dock layout -- see docs/europe.md#europe_dock_x-dock-layout */
#define EUROPE_DOCK_X 233
#define EUROPE_DOCK_Y 138  /* upper quay row (FUN_38fd_146c tier 0) */
#define EUROPE_DOCK_Y2 161 /* lower quay row (tier 1) */
#define EUROPE_DOCK_PITCH 17
#define EUROPE_DOCK_ROW0 3 /* slots before the row wraps */
#define EUROPE_DOCK_ROW1 5
#define EUROPE_DOCK_UNIT_W 16
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
  /* @CARGO columns 0/1: new-campaign opening-bid band (FUN_38fd_6024
   * rolls RNG(start_lo..start_hi) per cargo; smell audit #55). */
  int start_lo;
  int start_hi;
  /* @CARGO dynamics — FUN_38fd_0058 / 1d80 / 1dfa. */
  int low;
  int high;
  int burden;
  int rise;
  int fall;
  int attrition;
  int volatility;
} EuropeCargoQuote;

/* EUROPE_DOCK_TYPE_* -- see docs/europe.md#europe_dock_type_ */
#define EUROPE_DOCK_TYPE_COLONISTS 0
#define EUROPE_DOCK_TYPE_SOLDIERS 1
#define EUROPE_DOCK_TYPE_PIONEERS 2
#define EUROPE_DOCK_TYPE_MISSIONARIES 3
#define EUROPE_DOCK_TYPE_DRAGOONS 4
#define EUROPE_DOCK_TYPE_SCOUTS 5
#define EUROPE_DOCK_TYPE_COUNT 6

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
  ColonizeUnitKind kind; /* @UNIT row identity — never a name-string key */
  char name[32]; /* display only; filled live from NAMES.TXT @UNIT */
  int gold;
  bool is_ship; /* false = artillery → docks */
} EuropePurchaseOption;

typedef struct EuropeHarborShip {
  int type_index; /* into ColonizeUnitPool types; -1 until resolved by name */
  char name[32];
  int cargo_types[EUROPE_SHIP_CARGO_MAX]; /* passenger unit type indices */
  int cargo_professions[EUROPE_SHIP_CARGO_MAX]; /* @JOB per passenger */
  /* EuropeHarborShip cargo_treasure_gold -- see docs/europe.md#europeharborship-cargo_treasure_gold */
  int cargo_treasure_gold[EUROPE_SHIP_CARGO_MAX];
  int cargo_count;
  int hold_goods_type[EUROPE_SHIP_CARGO_MAX];
  int hold_goods_amount[EUROPE_SHIP_CARGO_MAX];
  int turns_left; /* 0 when in harbor; >0 while in transit */
  /* EuropeHarborShip departed_this_turn -- see docs/europe.md#europeharborship-departed_this_turn */
  bool departed_this_turn;
  int exit_x;
  int exit_y;
  bool exit_east; /* true = left via east edge (usually shorter) */
  /* EuropeHarborShip trade_route_plus1 -- see docs/europe.md#europeharborship-trade_route_plus1 */
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
  /* EuropeScreen current_crosses -- see docs/europe.md#europescreen-current_crosses */
  uint16_t current_crosses;
  uint16_t needed_crosses;
  bool crosses_immigrant_seen; /* true after at least one dock immigrant */
  uint16_t liberty_bells_pool;
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
  /* EuropeScreen pool_force_expert -- see docs/europe.md#europescreen-pool_force_expert */
  bool pool_force_expert;
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
  /* EuropeScreen bound_nation -- see docs/europe.md#europescreen-bound_nation */
  uint8_t bound_nation;
  /* EuropeScreen bound_human -- see docs/europe.md#europescreen-bound_human */
  bool bound_human;
  EuropeTrainOption train[EUROPE_TRAIN_MAX];
  int train_count;
  EuropePurchaseOption purchase[EUROPE_PURCHASE_MAX];
  int purchase_count;
  EuropeMenu menu;
  int menu_selection; /* 0 = None / cancel for list menus */
  int menu_dock_index;
  /* EuropeScreen purchase_confirming -- see docs/europe.md#europescreen-purchase_confirming */
  bool purchase_confirming;
  int purchase_confirm_index;
  int purchase_confirm_cost;
  /* Debug log only: set by the confirm paths so europe_menu_close can tell a
   * cancel (Esc / click-away) from the close that follows a pick. */
  bool menu_answered;
  /* EuropeScreen dock_menu_label -- see docs/europe.md#europescreen-dock_menu_label */
  char dock_menu_label[EUROPE_DOCK_MENU_MAX][72];
  uint8_t dock_menu_row[EUROPE_DOCK_MENU_MAX];
  bool dock_menu_greyed[EUROPE_DOCK_MENU_MAX];
  int dock_menu_count;
  int last_exit_x;
  int last_exit_y;
  bool last_exit_east;
  bool last_exit_valid;
  /* EuropeScreen price_event_cargo -- see docs/europe.md#europescreen-price_event_cargo */
  int price_event_cargo[EUROPE_CARGO_MAX];
  int price_event_dir[EUROPE_CARGO_MAX];
  int price_event_count;
  bool open_on_dock; /* set when Expected→Harbor this tick */
  /* EuropeScreen docked_with_goods -- see docs/europe.md#europescreen-docked_with_goods */
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
  /* EuropeScreen boycott_bitmap -- see docs/europe.md#europescreen-boycott_bitmap */
  uint16_t boycott_bitmap;
  /* Mirrors ColonizeCol1Nation.artillery_count (nation+0x1e) for the human —
   * DOS FUN_38fd_4b50 prices Artillery (type 0xb) at base + count*100 and
   * bumps the count on every purchase. Synced by col1_bridge apply/capture. */
  int artillery_bought;
  char status[160];
  /* EuropeScreen bar_event -- see docs/europe.md#europescreen-bar_event */
  char bar_event[EUROPE_BAR_EVENT_MAX][EUROPE_BAR_EVENT_LEN];
  int bar_event_count;
  /* LABELS.TXT for that wording (@CMESSAGE). NULL → built-in fallbacks. */
  const struct ColonizeMsgCatalog* labels;
  /* GAME.TXT (europe_set_messages) — sections like @KISSSORRY that this
   * screen composes itself rather than leaving to a game_dialogs.c popup.
   * NULL → those statuses stay blank, same as a missing labels catalog. */
  const struct ColonizeMsgCatalog* messages;
} EuropeScreen;

bool europe_load(EuropeScreen* eu, const char* data_dir, char* err, size_t err_size);
void europe_free(EuropeScreen* eu);
void europe_reset_campaign(EuropeScreen* eu);
void europe_reset_campaign_nation(EuropeScreen* eu, int nation);
/* Port / region / old-world nation from NAMES.TXT (@HOMEPORT / @COLONYNAME).
 * Does not wipe harbor, dock, or gold. nation clamped 0..3. */
void europe_set_nation(EuropeScreen* eu, int nation, const struct ColonizeMsgCatalog* names);

/* LABELS.TXT used for the sale status line's @CMESSAGE wording (bugs.md #376). */
void europe_set_labels(EuropeScreen* eu, const struct ColonizeMsgCatalog* labels);

/* GAME.TXT used for statuses this screen composes itself (e.g. @KISSSORRY). */
void europe_set_messages(EuropeScreen* eu, const struct ColonizeMsgCatalog* game_txt);

/* Europe sale status line -- see docs/europe.md#europe-sale-status-line */

/* europe_voyage_turns_roll -- see docs/europe.md#europe_voyage_turns_roll */
int europe_voyage_turns_roll(struct ColonizeDosRng* rng, bool magellan, int ship_count);

/* europe_compute_recruit_passage -- see docs/europe.md#europe_compute_recruit_passage */
int europe_compute_recruit_passage(
  int recruit_count, int difficulty, int current_crosses, int needed_crosses
);

/* Pool refill RNG stream -- see docs/europe.md#pool-refill-rng-stream */
/* FUN_38fd_4884(1,0): pool pick at no passage, no recruit-count bump (Fountain of Youth). */
bool europe_recruit_free_from_pool_ex(
  EuropeScreen* eu, int pool_index, struct ColonizeDosRng* rng
);
/* europe_immigrant_from_pool -- see docs/europe.md#europe_immigrant_from_pool */
bool europe_immigrant_from_pool(EuropeScreen* eu, struct ColonizeDosRng* rng);
/* Brewster (FF 20) arrival: FUN_38fd_4884(0,1) pick applied — free
 * dock transfer of pool[pool_index], then crosses zeroed (no +6 bump). */
bool europe_brewster_pick_from_pool_ex(
  EuropeScreen* eu, int pool_index, struct ColonizeDosRng* rng
);
/* Brewster owned (FUN_4345_0342 case 0x14): criminal/servant pool slots are
 * overwritten with Free Colonists in place — a substitution, not a reroll. */
void europe_apply_brewster(EuropeScreen* eu, int owned);
/* Same refill with DOS `FUN_38fd_46d4`'s param_1: non-zero skips the
 * criminal/servant/free tier roll and goes straight to the expert half.
 * The end-of-turn crosses spawn passes ((turn & 3) == 0). */
/* europe_refill_pool_slot_rng -- see docs/europe.md#europe_refill_pool_slot_rng */
void europe_refill_pool_slot_rng(
  EuropeScreen* eu, int slot, bool force_expert, struct ColonizeDosRng* rng
);
/* Restore a pool slot from a saved nation+2..+4 job byte (0x1c = empty). */
void europe_set_pool_slot(EuropeScreen* eu, int slot, int profession);
/* Game-start pool (DOS `FUN_38fd_6024`): fixed bottom-tier slot 0, two
 * expert-biased rolls, then the human's easy-difficulty override. */
void europe_seed_pool(EuropeScreen* eu, int difficulty, bool human);
/* europe_seed_campaign_prices -- see docs/europe.md#europe_seed_campaign_prices */
void europe_seed_campaign_prices(EuropeScreen* eu, struct ColonizeDosRng* rng);
/* Shared recruit-choice source: DOS FUN_38fd_4884 draws the same three pool
 * slots for the Recruit menu, the Brewster @RECRUITCHOOSE pick and the
 * Fountain of Youth @RECRUIT picks. Ensure fills empty slots; label is the
 * row text every one of those lists must use. */
void europe_pool_ensure_filled(EuropeScreen* eu);
const char* europe_pool_label(const EuropeScreen* eu, int slot);

/* Europe dock arrival Dragoon roll (_ex forms) -- see docs/europe.md#europe-dock-arrival-dragoon-roll-_ex-forms */
/* Recruit pool row enabled? DOS greys every pool row when the purse cannot
 * cover the passage (FUN_38fd_4884 raw 64736-64741) — bugs.md #588. */
bool europe_recruit_affordable(const EuropeScreen* eu);
/* Train dialog row enabled? DOS greys the row when the purse is short
 * (FUN_38fd_41ce raw 64399-64403) — bugs.md #566. */
bool europe_train_affordable(const EuropeScreen* eu, int train_index);
bool europe_train(EuropeScreen* eu, int train_index);
bool europe_train_ex(EuropeScreen* eu, int train_index, struct ColonizeDosRng* rng);
bool europe_purchase(EuropeScreen* eu, int purchase_index);
bool europe_purchase_ex(EuropeScreen* eu, int purchase_index, struct ColonizeDosRng* rng);
/* Purchase row enabled? DOS greys the row when the purse is short
 * (FUN_38fd_4b50 raw 64858-64862) — bugs.md #754. */
bool europe_purchase_affordable(const EuropeScreen* eu, int purchase_index);
/* europe_purchase_open_confirm -- see docs/europe.md#europe_purchase_open_confirm */
bool europe_purchase_open_confirm(EuropeScreen* eu, int purchase_index);
/* Commit a previously-confirmed purchase at the frozen price (the Yes arm
 * of @REALLYBUY, raw 64876-64887). bugs.md #753. */
bool europe_purchase_commit(
  EuropeScreen* eu, int purchase_index, int cost, struct ColonizeDosRng* rng
);

/* europe_dock_caption -- see docs/europe.md#europe_dock_caption */
bool europe_dock_caption(
  const EuropeScreen* eu, int dock_index, char* out, size_t cap
);

/* Live cost of a purchase-menu row — the table price plus DOS's Artillery
 * escalation (FUN_38fd_4b50: +100 gold per Artillery already purchased,
 * eu->artillery_bought = nation+0x1e). Returns -1 on a bad index. */
int europe_purchase_cost(const EuropeScreen* eu, int purchase_index);

/* europe_dock_push_load -- see docs/europe.md#europe_dock_push_load */
bool europe_dock_push_load(EuropeScreen* eu, const char* name, int profession);

/*
 * Screen position of dock slot `index` (DOS FUN_38fd_146c). False when the slot
 * is past the drawable tiers — DOS silently omits those immigrants.
 */
bool europe_dock_slot_pos(int index, int* out_x, int* out_y);

/* europe_remove_dock_mirror_unit -- see docs/europe.md#europe_remove_dock_mirror_unit */
void europe_remove_dock_mirror_unit(ColonizeUnitPool* units, int nation_id, int profession);

struct ColonizeMsgCatalog;

/* europe_build_dock_menu -- see docs/europe.md#europe_build_dock_menu */
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

/* Europe dock menu row with col1 ledger -- see docs/europe.md#europe-dock-menu-row-with-col1-ledger */

/* europe_dock_unit_dos_type -- see docs/europe.md#europe_dock_unit_dos_type */
int europe_dock_unit_dos_type(int profession, int difficulty, bool human, struct ColonizeDosRng* rng);

/* Pool type_index for a DOS @UNIT type code 0..5, or -1. */
int europe_dock_unit_type_index(const ColonizeUnitPool* units, int dos_type);
/* europe_dock_unit_type_index_ex -- see docs/europe.md#europe_dock_unit_type_index_ex */
int europe_dock_unit_type_index_ex(
  const ColonizeUnitPool* units, int dos_type, bool with_singular_fallback
);

/* Europe purchase table -- see docs/europe.md#europe-purchase-table */
int europe_purchase_option_count(void);
const EuropePurchaseOption* europe_purchase_option_at(int index);
int europe_purchase_price(ColonizeUnitKind kind);

/* europe_dock_type_for -- see docs/europe.md#europe_dock_type_for */
int europe_dock_type_for(const char* name, int profession);
/* bugs.md #669: 0..5 or a Continental kind (7 / 9). */
bool europe_dock_dos_type_is_valid(int dos_type);

/* europe_dock_icon_sprite -- see docs/europe.md#europe_dock_icon_sprite */
int europe_dock_icon_sprite(const ColonizeUnitPool* units, const EuropeDockImmigrant* d);

/* europe_passenger_icon_sprite -- see docs/europe.md#europe_passenger_icon_sprite */
int europe_passenger_icon_sprite(const ColonizeUnitPool* units, int type_index, int profession);

/* europe_pax_type_index -- see docs/europe.md#europe_pax_type_index */
int europe_pax_type_index(const ColonizeUnitPool* units, int tag);

/* EuropeIconFlow -- see docs/europe.md#europeiconflow */
typedef struct EuropeIconFlow {
  int box_x;
  int box_y;
  int box_w;
  int box_h;
  int x; /* top-left of the icon most recently placed */
  int y;
  int row_h;
} EuropeIconFlow;
/* False when the box has no room for a ship row under its two header lines. */
bool europe_icon_flow_begin(
  EuropeIconFlow* f, int box_x, int box_y, int box_w, int box_h, int line_h
);
/* Place the next w×h icon (wrapping rows); false when it would leave the box. */
bool europe_icon_flow_place(EuropeIconFlow* f, int w, int h);
/* Step past the icon just placed. */
void europe_icon_flow_advance(EuropeIconFlow* f, int w);
/* Sprite size with the 14×16 fallback the transit boxes always used. */
void europe_icon_flow_size(const ColonizeSpriteSheet* icons, int sprite, int* w, int* h);

/* europe_dock_display_type_index -- see docs/europe.md#europe_dock_display_type_index */
int europe_dock_display_type_index(
  const ColonizeUnitPool* units, const EuropeDockImmigrant* d
);

/* Kit implied by a dock entry's type, for the mirror unit and for landing. */

/* europe_spawn_dock_mirror_unit -- see docs/europe.md#europe_spawn_dock_mirror_unit */
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
bool europe_pop_dock_immigrant(EuropeScreen* eu, char* out_name, size_t out_name_size);
/* Pop with profession; returns false if empty. */

bool europe_harbor_push(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
);
/* Same, carrying the passengers' @JOB professions through like the Expected
 * mirror europe_enqueue_expected does (smell audit 2026-09-10 G10). The
 * plain form above is this with cargo_professions = NULL (every profession
 * -1), which is right only for a push with no passengers. */
bool europe_harbor_push_ex(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  const int* cargo_professions,
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

/* europe_cash_treasure -- see docs/europe.md#europe_cash_treasure */
int europe_cash_treasure(EuropeScreen* eu, int treasure_value);

/* ONE treasury per nation -- see docs/europe.md#one-treasury-per-nation */
uint32_t europe_nation_gold(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int nation
);
void europe_nation_gold_add(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, long delta
);

/* europe_set_live_screen -- see docs/europe.md#europe_set_live_screen */
void europe_set_live_screen(EuropeScreen* eu);

/* europe_set_live_save -- see docs/europe.md#europe_set_live_save */
void europe_set_live_save(struct ColonizeCol1Save* col1);
struct AiPopupState;
void europe_set_popup_queue(struct AiPopupState* popups);

/* europe_gold_stamp_record -- see docs/europe.md#europe_gold_stamp_record */
void europe_gold_stamp_record(const EuropeScreen* eu, struct ColonizeCol1Save* col1);

/* europe_cargo_boycotted_ex -- see docs/europe.md#europe_cargo_boycotted_ex */
int europe_cargo_boycotted_ex(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int nation, int cargo_type
);
int europe_cargo_boycotted(const EuropeScreen* eu, int cargo_type);

/* europe_buyback_boycott -- see docs/europe.md#europe_buyback_boycott */
int europe_buyback_boycott(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int human_nation, int cargo_type
);
/* europe_buyback_boycott_cost -- see docs/europe.md#europe_buyback_boycott_cost */
int europe_buyback_boycott_cost(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int human_nation, int cargo_type
);

/* Europe price accessors -- see docs/europe.md#europe-price-accessors */
void europe_set_sound_hook(void (*play_fn)(int id));
void europe_set_bgm_hook(void (*set_bgm_fn)(int pool));
void europe_notify_immigrant_sound(EuropeScreen* eu);
int europe_sell_price(const EuropeScreen* eu, int cargo_type);
int europe_buy_price(const EuropeScreen* eu, int cargo_type);
/* europe_cargo_burden -- see docs/europe.md#europe_cargo_burden */
int europe_cargo_burden(int cargo_type);
/* gross − gross·tax/100 (FUN_364b_0688 Custom House arm; same rounding as
 * the harbor sale). Returns the net treasury credit. */
int europe_net_after_tax(int gross, int tax_percent);
int europe_sell_proceeds(const EuropeScreen* eu, int cargo_type, int amount);
/*
 * Harbor sell of one hold. Net proceeds go to eu->gold; the withheld tax goes
 * to col1->nation[seller_nation].royal_money — DOS writes `nation+0x22 += tax`
 * on every sale arm (same write as the Custom House arm and the boycott
 * buy-back). `col1` may be NULL (tests / no save bound), in which case only
 * gold moves. Smell audit #51.
 */
int europe_sell_hold(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int harbor_index,
  int hold_index
);
/* europe_sell_hold_partial -- see docs/europe.md#europe_sell_hold_partial */
int europe_sell_hold_partial(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int harbor_index,
  int hold_index,
  int amount
);
/* europe_apply_trade_volume -- see docs/europe.md#europe_apply_trade_volume */
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
/* Harbor buy/sell wrapper: the bound nation (`eu->bound_nation`, DS:0x9e12 —
 * always the human in this port) trades with itself, so the 1d44 term takes
 * the human arm and a Dutch human gets the 1dfa (term·2)/3 damping.
 * Immediate FUN_38fd_0058 step. Smell audit #57/#58. */
void europe_apply_volume_price(EuropeScreen* eu, int cargo_type, int amount, int is_buy);
/* europe_tick_market_prices_w -- see docs/europe.md#europe_tick_market_prices_w */
void europe_tick_market_prices_w(
  const ColonizeWorld* w,
  int human_nation,
  uint32_t turn
);
/*
 * FUN_38fd_584a score: (pop+units)<<1 if <4000, +8, cap 4000;
 * AI ((8-diff)*score)>>3; English (nation 0) *2/3.
 */
int europe_compute_immigration_score_w(
  const ColonizeWorld* w,
  int nation_id
);
/* europe_tick_immigration_pressure_w design -- see docs/europe.md#europe_tick_immigration_pressure_w-design */
/* Returns 1 when an immigrant was moved to the docks, 2 when Brewster is
 * owned and the caller must offer the @RECRUITCHOOSE pick instead
 * (units_brewster_enqueue_pick / europe_brewster_pick_from_pool), else 0. */
int europe_tick_immigration_pressure_w(
  const ColonizeWorld* w,
  int nation_id
);

/* europe_nation_immigration_tick_w -- see docs/europe.md#europe_nation_immigration_tick_w */
int europe_nation_immigration_tick_w(const ColonizeWorld* w, int nation_id);

/* FUN_38fd_46d4 on a nation record's pool byte; returns the @JOB stored. */
int europe_nation_refill_pool_slot(
  struct ColonizeCol1Save* col1, int nation_id, int slot, bool force_expert,
  struct ColonizeDosRng* rng
);

/* FUN_38fd_0718 for a nation record: unit id parked in the Europe limbo, or -1. */
int europe_nation_harbor_spawn(const ColonizeWorld* w, int nation_id, int profession);
/* europe_sell_unit_hold_w -- see docs/europe.md#europe_sell_unit_hold_w */
int europe_sell_unit_hold_w(
  const ColonizeWorld* w,
  int unit_id,
  int hold_index
);

/* Forward decls — avoid pulling colony/col1 into every europe consumer. */
struct ColonizeColonyPool;
struct ColonizeColony;
struct ColonizeCol1Save;
struct ColonizeDosRng;

/* europe_custom_house_autosell_w -- see docs/europe.md#europe_custom_house_autosell_w */
int europe_custom_house_autosell_w(
  const ColonizeWorld* w,
  ColonizeColony* colony,
  int human_nation
);

/* One cargo type's Custom House sale — the numbers DOS puts in its status line. */
typedef struct EuropeCustomHouseSale {
  int cargo;       /* cargo type index */
  int amount;      /* units shipped */
  int gross;       /* price x amount, before tax */
  int tax_percent; /* 0 while independence is declared */
  int tax_paid;
  int net;         /* gold actually credited */
} EuropeCustomHouseSale;

/* europe_custom_house_autosell_ex_w -- see docs/europe.md#europe_custom_house_autosell_ex_w */
int europe_custom_house_autosell_ex_w(
  const ColonizeWorld* w,
  ColonizeColony* colony,
  int human_nation,
  EuropeCustomHouseSale* out,
  int out_max,
  int* out_count
);

/* europe_ai_colony_dump_sell_w -- see docs/europe.md#europe_ai_colony_dump_sell_w */
int europe_ai_colony_dump_sell_w(
  const ColonizeWorld* w,
  ColonizeColony* colony,
  int human_nation
);

/*
 * FUN_364b_0636 Custom House / export denylist: not Food, Lumber, Horses, Tools,
 * Muskets. Used by autosell and AI peace Europe export sail.
 */
int europe_cargo_export_eligible(int cargo_type);

/* europe_custom_house_cargo_enabled -- see docs/europe.md#europe_custom_house_cargo_enabled */
bool europe_custom_house_cargo_enabled(uint16_t custom_house_bits, int cargo_type);

/* europe_harbor_cargo_room -- see docs/europe.md#europe_harbor_cargo_room */
int europe_harbor_cargo_room(
  const EuropeScreen* eu,
  const ColonizeUnitPool* units,
  int harbor_index,
  int cargo_type
);

/* Harbor buy; col1+buyer_nation feed the 1d80 nation trade ledger (smell
 * audit #50) — NULL col1 keeps the price move but skips the ledger. `units`
 * resolves the ship's hold capacity (smell audit #83); NULL = six holds. */
int europe_buy_cargo_w(
  const ColonizeWorld* w,
  int buyer_nation,
  int harbor_index,
  int cargo_type,
  int amount
);
/* europe_buy_unit_cargo_w -- see docs/europe.md#europe_buy_unit_cargo_w */
int europe_buy_unit_cargo_w(
  const ColonizeWorld* w,
  int unit_id,
  int cargo_type,
  int amount
);
int europe_best_sell_hold(const EuropeScreen* eu, int harbor_index);

EuropeHitResult europe_hit_test(const EuropeScreen* eu, int mx, int my);

/* europe_hit_test_ex -- see docs/europe.md#europe_hit_test_ex */
EuropeHitResult europe_hit_test_ex(
  const EuropeScreen* eu,
  int mx,
  int my,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int transit_line_h
);

/* Ship icon index under (mx,my) inside a transit box, or -1. */

void europe_menu_open(EuropeScreen* eu, EuropeMenu menu);
void europe_menu_close(EuropeScreen* eu);
/* Apply current menu_selection (0 = cancel). Returns true if acted.
 * `_ex` carries the game rng for the RECRUIT row's pool refill (see
 * europe_recruit_from_pool_ex); the plain form passes NULL. */
bool europe_menu_confirm(EuropeScreen* eu);
bool europe_menu_confirm_ex(EuropeScreen* eu, struct ColonizeDosRng* rng);

/* Apply the highlighted dock-menu row; `units` keeps the mirror unit in step. */

/* Same, plus the col1 save the arm buy/sell rows book their trade ledger into
 * — see europe_apply_dock_menu_row_ex. The game loop should call this form. */
bool europe_dock_menu_apply_selection_ex_w(
  const ColonizeWorld* w,
  int nation_id
);

void europe_cheat_add_gold(EuropeScreen* eu, int amount);
void europe_cheat_adjust_tax(EuropeScreen* eu, int delta);

/* Opens the Recruit menu (was europe_recruit). */
bool europe_open_recruit_menu(EuropeScreen* eu);

#endif
