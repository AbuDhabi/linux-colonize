#ifndef COLONIZE_EUROPE_H
#define COLONIZE_EUROPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/pik.h"

#define EUROPE_CARGO_MAX 16
#define EUROPE_DOCK_MAX 8
#define EUROPE_CLASS_MAX 8

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

/*
 * Europe / home-port bring-up state.
 * Full buy/sell drag UI and ship cargo holds come later; this is the screen shell
 * plus market quotes, treasury, tax, and a stub immigrant dock.
 */
typedef struct EuropeScreen {
  ColonizePikImage background;
  bool background_ok;
  char port_city[48];
  char nation_name[48];
  int gold;
  int tax_percent;
  EuropeCargoQuote cargo[EUROPE_CARGO_MAX];
  int cargo_count;
  EuropeRecruitClass classes[EUROPE_CLASS_MAX];
  int class_count;
  EuropeDockImmigrant dock[EUROPE_DOCK_MAX];
  int dock_count;
  int harbor_ships; /* stub count until units exist */
  char status[96];
} EuropeScreen;

bool europe_load(EuropeScreen* eu, const char* data_dir, char* err, size_t err_size);
void europe_free(EuropeScreen* eu);
void europe_reset_campaign(EuropeScreen* eu);

/* Recruit cheapest available class onto the docks. Returns false if broke/full. */
bool europe_recruit(EuropeScreen* eu);
/* Remove oldest dock immigrant for deployment in the New World. */
bool europe_pop_dock_immigrant(EuropeScreen* eu, char* out_name, size_t out_name_size);
/* Train is not implemented yet; sets status text. */
void europe_train_stub(EuropeScreen* eu);
void europe_cheat_add_gold(EuropeScreen* eu, int amount);
void europe_cheat_adjust_tax(EuropeScreen* eu, int delta);

#endif
