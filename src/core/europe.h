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
} EuropeHarborShip;

/*
 * Europe / home-port bring-up state.
 * Full buy/sell drag UI and cargo holds come later; this is the screen shell
 * plus market quotes, treasury, tax, immigrant dock, and a simple ship harbor.
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
  char status[96];
} EuropeScreen;

bool europe_load(EuropeScreen* eu, const char* data_dir, char* err, size_t err_size);
void europe_free(EuropeScreen* eu);
void europe_reset_campaign(EuropeScreen* eu);

/* Recruit cheapest available class onto the docks. Returns false if broke/full. */
bool europe_recruit(EuropeScreen* eu);
/* Remove oldest dock immigrant for deployment in the New World. */
bool europe_pop_dock_immigrant(EuropeScreen* eu, char* out_name, size_t out_name_size);

/* Dock a New World ship in the European harbor (FIFO). cargo_types may be NULL. */
bool europe_harbor_push(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  int cargo_count
);
/* Undock oldest harbor ship for return to the New World. Cargo outs may be NULL. */
bool europe_harbor_pop(
  EuropeScreen* eu,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max
);

/* Train is not implemented yet; sets status text. */
void europe_train_stub(EuropeScreen* eu);
void europe_cheat_add_gold(EuropeScreen* eu, int amount);
void europe_cheat_adjust_tax(EuropeScreen* eu, int delta);

#endif
