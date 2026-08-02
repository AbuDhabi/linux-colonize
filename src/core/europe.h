#ifndef COLONIZE_EUROPE_H
#define COLONIZE_EUROPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/pik.h"

#define EUROPE_CARGO_MAX 16
#define EUROPE_DOCK_MAX 8
#define EUROPE_CLASS_MAX 8
#define EUROPE_HARBOR_MAX 8
#define EUROPE_SHIP_CARGO_MAX 6 /* matches COLONIZE_UNIT_CARGO_MAX */

/* Approximate EUROPE.PIK hit/layout for market + harbor holds. */
#define EUROPE_MARKET_X 170
#define EUROPE_MARKET_Y 38
#define EUROPE_MARKET_ROW_H 8
#define EUROPE_MARKET_W 140
#define EUROPE_HARBOR_LIST_X 4
#define EUROPE_HARBOR_LIST_Y 110
#define EUROPE_HARBOR_ROW_H 9
#define EUROPE_HARBOR_LIST_W 160
#define EUROPE_HOLD_X 8
#define EUROPE_HOLD_Y 148
#define EUROPE_HOLD_W 12
#define EUROPE_HOLD_H 14
#define EUROPE_HOLD_PITCH 14
#define EUROPE_CARGO_ICON_BASE 22

typedef struct EuropeCargoQuote {
  char name[32];
  int bid; /* port pays this when you sell */
  int ask; /* you pay this when you buy */
} EuropeCargoQuote;

typedef struct EuropeDockImmigrant {
  char name[40];
  bool present;
} EuropeDockImmigrant;

typedef struct EuropeRecruitClass {
  char name[40];
  int cost;
} EuropeRecruitClass;

typedef struct EuropeHarborShip {
  int type_index; /* into ColonizeUnitPool types */
  char name[32];
  int cargo_types[EUROPE_SHIP_CARGO_MAX]; /* passenger unit type indices */
  int cargo_count;
  /* Commodity holds (same layout as ColonizeUnit). */
  int hold_goods_type[EUROPE_SHIP_CARGO_MAX];
  int hold_goods_amount[EUROPE_SHIP_CARGO_MAX];
} EuropeHarborShip;

typedef enum EuropeHit {
  EUROPE_HIT_NONE = 0,
  EUROPE_HIT_HARBOR_SHIP,
  EUROPE_HIT_HOLD,
  EUROPE_HIT_MARKET
} EuropeHit;

typedef struct EuropeHitResult {
  EuropeHit kind;
  int index; /* harbor ship, hold slot, or cargo type */
} EuropeHitResult;

/*
 * Europe / home-port screen: market quotes, treasury, tax, immigrant dock,
 * harbor ships with passenger + commodity holds, and buy/sell helpers.
 */
typedef struct EuropeScreen {
  ColonizePikImage background;
  bool background_ok;
  char port_city[48];
  char nation_name[48];
  int gold;
  int tax_percent;
  /* Nation-side turn ticks (mirrors Col1 nation blob for the human player). */
  uint16_t current_crosses;
  uint16_t needed_crosses;
  uint16_t liberty_bells_total;
  uint16_t liberty_bells_last_turn;
  EuropeCargoQuote cargo[EUROPE_CARGO_MAX];
  int cargo_count;
  EuropeRecruitClass classes[EUROPE_CLASS_MAX];
  int class_count;
  EuropeDockImmigrant dock[EUROPE_DOCK_MAX];
  int dock_count;
  EuropeHarborShip harbor[EUROPE_HARBOR_MAX];
  int harbor_ships;
  int selected_harbor; /* -1 none */
  char status[96];
} EuropeScreen;

bool europe_load(EuropeScreen* eu, const char* data_dir, char* err, size_t err_size);
void europe_free(EuropeScreen* eu);
void europe_reset_campaign(EuropeScreen* eu);
/* Reset treasury/dock and set port/nation for European power 0..3. */
void europe_reset_campaign_nation(EuropeScreen* eu, int nation);

/* Recruit cheapest available class onto the docks. Returns false if broke/full. */
bool europe_recruit(EuropeScreen* eu);
/* Remove oldest dock immigrant for deployment in the New World. */
bool europe_pop_dock_immigrant(EuropeScreen* eu, char* out_name, size_t out_name_size);

/*
 * Dock a New World ship in the European harbor (FIFO).
 * cargo_types / hold arrays may be NULL (empty passengers / holds).
 */
bool europe_harbor_push(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
);
/* Undock oldest harbor ship. Out arrays may be NULL. */
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

/* Keep selected_harbor valid; auto-select sole ship or first with goods. */
void europe_refresh_harbor_selection(EuropeScreen* eu);

/* Taxed sale proceeds for amount of cargo_type (0 if unknown type). */
int europe_sell_proceeds(const EuropeScreen* eu, int cargo_type, int amount);
/* Sell one hold; returns gold gained (0 if empty/invalid). */
int europe_sell_hold(EuropeScreen* eu, int harbor_index, int hold_index);
/*
 * Buy into selected harbor ship holds. amount clamped by ask/gold/room (max 100/hold).
 * Returns amount bought.
 */
int europe_buy_cargo(EuropeScreen* eu, int harbor_index, int cargo_type, int amount);
/* Best non-empty hold index to sell (L-key), or -1. */
int europe_best_sell_hold(const EuropeScreen* eu, int harbor_index);

EuropeHitResult europe_hit_test(const EuropeScreen* eu, int mx, int my);

/* Train is not implemented yet; sets status text. */
void europe_train_stub(EuropeScreen* eu);
void europe_cheat_add_gold(EuropeScreen* eu, int amount);
void europe_cheat_adjust_tax(EuropeScreen* eu, int delta);

#endif
