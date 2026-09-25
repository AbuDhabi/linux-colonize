/*
 * Sections:
 *  - Purse/rng plumbing, ship cargo helpers & dock-push utilities (europe_purse_nation .. europe_disembark_passengers_to_dock)
 *  - Gold accessors, live-save/popup wiring & purse moves (europe_purse_nation .. europe_credit_sale_tax)
 */

#include "core/europe.h"
#include "core/europe_art.h"
#include "core/europe_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "platform/diagnostics.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/ui_button.h"
#include "core/units.h"
#include "platform/platform.h"

/* Forward declarations for this file's own statics (the split moved
   section order; these keep every call site legal). */
static unsigned europe_rng_next(unsigned* state);
static bool europe_dock_push_front(
  EuropeScreen* eu,
  const char* name,
  int profession,
  bool sentry
);
static int europe_type_is_treasure(const ColonizeUnitPool* units, int type_tag);
static void europe_cash_treasure_passengers(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  const ColonizeUnitPool* units
);

/* Sound hook (unit tests build europe.c without sound.c — same shape as
 * units_set_combat_music_hooks). */
void (*g_europe_sound_play)(int id) = NULL;
void (*g_europe_set_bgm)(int pool) = NULL;

/*
 * Live save + popup queue for the Europe-only channels that hold neither.
 * europe_cash_treasure is DOS FUN_48d3_06ba's arrival beat (raw 78005-78021):
 * it books the Crown's fee on nation+0x22 and the write-only +0x26 counter and
 * raises @LOOTCASH as a modal, none of which it can do from `eu` alone. Same
 * register-once idiom as europe_set_live_screen / colonies_set_col1_context;
 * NULL degrades to purse-only (tests, AI borrow paths).
 */
struct ColonizeCol1Save* g_europe_live_save = NULL;
AiPopupState* g_europe_popups = NULL;

/* ===================== Purse/rng plumbing, ship cargo helpers & dock-push utilities (europe_purse_nation .. europe_disembark_passengers_to_dock) ===================== */
int europe_purse_nation(const EuropeScreen* eu);
void europe_purse_move(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, long delta
);
#include "platform/platform.h"

void europe_refresh_recruit_passage(EuropeScreen* eu);
/* One treasury, both stores — see the block above europe_cargo_boycotted_ex. */
void europe_purse_move(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, long delta
);

/*
 * Recruit-pool randomness. DOS `FUN_38fd_46d4` uses TWO sources and the
 * split matters for stream fidelity:
 *
 *  - the three tier rolls come off the shared game stream,
 *    `FUN_281f_04d4(1,15)` / `(1,10)` / `(1,8)` (viceroy_unpacked.c 64632,
 *     64636, 64640) — the same stream `europe_immigrant_from_pool`'s slot
 *     pick (`04d4(0,2)`, 68581) already takes a ColonizeDosRng* for, and the
 *     two are drawn back to back in DOS's own phase-5 spawn (68581/68583);
 *  - the expert value comes off a per-nation 5-bit LFSR
 *    (`FUN_291f_0eda` stepping nation+0x44 with poly 0x14 plus the +0x45
 *     salt, 64585-64590), which consumes NO shared-stream draw. A
 *    force-expert refill (`46d4(1)`, the (turn & 3) == 0 case) therefore
 *    advances the shared stream not at all.
 *
 * The port has the shared stream (ColonizeTurnContext.rng /
 * ColonizeGameState.move_rng, plumbed in below) but not the LFSR: the port
 * repurposed nation+0x44/+0x45 as ColonizeCol1Nation.diplo_flag[0..1]. So
 * the tier rolls now go through dos_rng_range on the real stream and the
 * expert value keeps this local LCG as the LFSR stand-in — which is also
 * what keeps the shared stream exactly where DOS leaves it.
 *
 * The local state is seeded from the bound stream's current state (a read,
 * never a draw) when there is one; the treasury-derived seed survives only
 * for callers that legitimately hold no game rng — europe_seed_pool (which
 * shares one state across the two slots it rolls) and fixture/unit-test
 * callers that pass NULL.
 */
static unsigned europe_rng_next(unsigned* state) {
  unsigned s = state ? *state : 1u;
  s = s * 1103515245u + 12345u;
  if (state) {
    *state = s;
  }
  return (s >> 16) & 0x7fffu;
}

/* DOS `FUN_281f_04d4(lo,hi)` — inclusive, off the shared stream. */
int europe_pool_tier_roll(EuropePoolRng* r, int lo, int hi) {
  if (r->dos) {
    return dos_rng_range(r->dos, lo, hi);
  }
  return lo + (int)(europe_rng_next(r->local) % (unsigned)(hi - lo + 1));
}

/*
 * DOS's per-nation LFSR stand-in for the expert-tier draw (FUN_38fd_46d4,
 * raw 64584-64592). The DOS shape is: advance nation+0x44 through
 * FUN_291f_0eda(&b44, 0x14), take `(nation+0x44 + nation+0x45) & 0x1f`, and
 * REROLL while the result is > 0x18 — not a modulo (bugs.md #566). Same
 * distribution over 0..0x18, different number of draws off the local state,
 * which is what a byte-exact RNG trace sees.
 */
int europe_pool_expert_roll(EuropePoolRng* r, int hi) {
  if (hi != 0x18) {
    return (int)(europe_rng_next(r->local) % (unsigned)(hi + 1));
  }
  for (int guard = 0; guard < 1000; ++guard) {
    const int v = (int)(europe_rng_next(r->local) & 0x1fu);
    if (v <= 0x18) {
      return v;
    }
  }
  return 0;
}

bool europe_parse_int_field(const char** cursor, int* out) {
  while (**cursor == ' ' || **cursor == '\t' || **cursor == ',') {
    ++(*cursor);
  }
  if (**cursor == '\0') {
    return false;
  }
  char* end = NULL;
  long v = strtol(*cursor, &end, 10);
  if (end == *cursor) {
    return false;
  }
  *out = (int)v;
  *cursor = end;
  return true;
}

void europe_set_status(EuropeScreen* eu, const char* text) {
  if (!eu) {
    return;
  }
  snprintf(eu->status, sizeof(eu->status), "%s", text ? text : "");
}

void europe_copy_ship(EuropeHarborShip* dst, const EuropeHarborShip* src) {
  if (!dst || !src) {
    return;
  }
  *dst = *src;
}

void europe_clear_ship(EuropeHarborShip* s) {
  if (!s) {
    return;
  }
  memset(s, 0, sizeof(*s));
  s->type_index = -1;
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    s->cargo_professions[i] = -1;
  }
}

int europe_goods_slots_used(const EuropeHarborShip* ship) {
  if (!ship) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    if (ship->hold_goods_amount[i] > 0 && ship->hold_goods_amount[i] < 255) {
      n++;
    }
  }
  return n;
}

int europe_ship_cargo_cap(const EuropeHarborShip* ship, const ColonizeUnitPool* units) {
  if (!ship) {
    return 0;
  }
  if (units) {
    int ti = ship->type_index;
    if (ti < 0) {
      ti = units_find_type(units, ship->name);
    }
    const ColonizeUnitType* ut = units_type(units, ti);
    if (ut && ut->cargo > 0) {
      return ut->cargo > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : ut->cargo;
    }
  }
  return EUROPE_SHIP_CARGO_MAX;
}

/*
 * Free goods slots in a harbor ship, DOS FUN_15eb_3208's first term:
 * `@UNIT cargo column (type*0xe+0x5237) − holds_occupied`. Passengers ride
 * the same slot array in DOS, and the port's boarding path already spends
 * them that way (europe_board_sentry_dockers), so they count here too.
 */
int europe_ship_free_slots(const EuropeHarborShip* ship, const ColonizeUnitPool* units) {
  if (!ship) {
    return 0;
  }
  const int free_slots =
    europe_ship_cargo_cap(ship, units) - europe_goods_slots_used(ship) - ship->cargo_count;
  return free_slots > 0 ? free_slots : 0;
}

/*
 * A docked Artillery piece carries no dos_type of its own; it is recognised by
 * carrying the catalog's @UNIT row 11 name (whatever a given NAMES.TXT calls
 * it) — never by an English word compiled in here.
 */
bool europe_dock_name_is_artillery(const char* name) {
  const char* live = reports_names_field("UNIT", UNITS_KIND_ARTILLERY, 0);
  return name && name[0] && live && live[0] && strcmp(name, live) == 0;
}

/*
 * The EUROPE_DOCK_TYPE_* row an immigrant name belongs to, or -1. The six
 * dock types ARE NAMES.TXT @UNIT rows 0..5 (europe_dock_unit_type_index_ex),
 * so this is a catalog **row** scan, not an English name test: the name each
 * row carries comes from reports_dock_type_name, never from the binary. A
 * name that matches no row (a profession caption, an empty slot) returns -1
 * and the caller falls back to the profession's DOS type (bugs.md #603).
 */
int europe_dock_type_row_of_name(const char* name) {
  if (!name || !name[0]) {
    return -1;
  }
  for (int i = 0; i < EUROPE_DOCK_TYPE_COUNT; ++i) {
    const char* row = reports_dock_type_name(i);
    if (row && row[0] && strcmp(name, row) == 0) {
      return i;
    }
  }
  return -1;
}

/* Insert at dock front (index 0). Returns false if docks are full. */
static bool europe_dock_push_front(
  EuropeScreen* eu,
  const char* name,
  int profession,
  bool sentry
) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  for (int i = eu->dock_count; i > 0; --i) {
    eu->dock[i] = eu->dock[i - 1];
  }
  EuropeDockImmigrant* d = &eu->dock[0];
  memset(d, 0, sizeof(*d));
  snprintf(d->name, sizeof(d->name), "%s", name ? name : "");
  d->profession = profession;
  d->present = true;
  d->sentry = sentry;
  d->dos_type = europe_dock_type_for(d->name, profession);
  eu->dock_count++;
  return true;
}

bool europe_dock_push_load(EuropeScreen* eu, const char* name, int profession) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "");
  slot->profession = profession;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_for(slot->name, profession);
  return true;
}

bool europe_dock_slot_pos(int index, int* out_x, int* out_y) {
  /*
   * FUN_38fd_146c: tier 0 = the first EUROPE_DOCK_ROW0 slots on the upper quay,
   * tier 1 = the next EUROPE_DOCK_ROW1 on the lower one, both starting from the
   * same base x. Tier 2 (anything beyond) is computed but never blitted.
   */
  if (index < 0 || index >= EUROPE_DOCK_ROW0 + EUROPE_DOCK_ROW1) {
    return false;
  }
  int col = index;
  int y = EUROPE_DOCK_Y;
  if (index >= EUROPE_DOCK_ROW0) {
    col = index - EUROPE_DOCK_ROW0;
    y = EUROPE_DOCK_Y2;
  }
  if (out_x) {
    *out_x = EUROPE_DOCK_X + col * EUROPE_DOCK_PITCH;
  }
  if (out_y) {
    *out_y = y;
  }
  return true;
}

static int europe_type_is_treasure(const ColonizeUnitPool* units, int type_tag) {
  if (!units || type_tag < 0) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, type_tag);
  return units_type_is_treasure(ut);
}

/*
 * Treasure passengers cash in (or PARK without inventing gold) and are removed
 * before dock unload — they are not immigrants. Cite: Colonization.pdf Treasure
 * Trains; GAME.TXT @LOOTCASH / @CASHTREASURE.
 */
static void europe_cash_treasure_passengers(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  const ColonizeUnitPool* units
) {
  if (!eu || !ship || ship->cargo_count <= 0) {
    return;
  }
  int w = 0;
  for (int i = 0; i < ship->cargo_count; ++i) {
    const int tag = ship->cargo_types[i];
    const int gold = ship->cargo_treasure_gold[i];
    if (europe_type_is_treasure(units, tag)) {
      if (gold > 0) {
        (void)europe_cash_treasure(eu, gold);
      } else {
        /*
         * gold==0 here means cargo_treasure_gold was never filled for this
         * slot — game_loop.c's game_europe_fill_expected_treasure_gold (the
         * H/Return-to-Europe path) now does the real fill, so this is a
         * defensive no-op for any other boarding path, not the live gap
         * this comment used to describe. Still don't invent a rate/value.
         */
      }
      continue;
    }
    if (w != i) {
      ship->cargo_types[w] = tag;
      ship->cargo_professions[w] = ship->cargo_professions[i];
      ship->cargo_treasure_gold[w] = gold;
    }
    ++w;
  }
  for (int i = w; i < ship->cargo_count; ++i) {
    ship->cargo_types[i] = 0;
    ship->cargo_professions[i] = -1;
    ship->cargo_treasure_gold[i] = 0;
  }
  ship->cargo_count = w;
}

/* Unload passengers onto dock front (preserves on-board order); clear holds. */
void europe_disembark_passengers_to_dock(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  const ColonizeUnitPool* units
) {
  if (!eu || !ship || ship->cargo_count <= 0) {
    return;
  }
  europe_cash_treasure_passengers(eu, ship, units);
  for (int i = ship->cargo_count - 1; i >= 0; --i) {
    char name[40];
    const int tag = ship->cargo_types[i];
    const int prof = ship->cargo_professions[i];
    if (tag == -2) {
      snprintf(name, sizeof(name), "%s", "");
    } else if (units) {
      const ColonizeUnitType* ut = units_type(units, tag);
      if (ut && ut->name[0]) {
        snprintf(name, sizeof(name), "%s", ut->name);
      } else {
        snprintf(name, sizeof(name), "%s", "");
      }
    } else {
      snprintf(name, sizeof(name), "%s", "");
    }
    /* Passengers keep sentry ("board next") — same convention as aboard ship. */
    if (!europe_dock_push_front(eu, name, prof, true)) {
      /* bugs.md #750: no catalog tag for the docks-full case — show nothing. */
      eu->status[0] = '\0';
      /* Leave remaining passengers (0..i) on the ship. */
      ship->cargo_count = i + 1;
      return;
    }
    /* bugs.md #669: a Continental keeps its @UNIT row (DOS keeps +0x3146);
     * the name scan above only knows the six @ARMOPTIONS rows. */
    if (units && tag >= 0) {
      const int kind = (int)units_type_kind(units_type(units, tag));
      if (kind == (int)UNITS_KIND_CONT_ARMY || kind == (int)UNITS_KIND_CONT_CAV) {
        eu->dock[0].dos_type = kind;
      }
    }
  }
  ship->cargo_count = 0;
  memset(ship->cargo_types, 0, sizeof(ship->cargo_types));
  memset(ship->cargo_treasure_gold, 0, sizeof(ship->cargo_treasure_gold));
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    ship->cargo_professions[i] = -1;
  }
}


/*
 * The nation `eu->gold` is the purse of: DS:0x9e12, set by FUN_38fd_0000
 * together with the record pointer DS:0x84fc (viceroy_unpacked.c 58695-58703).
 * -1 when there is no screen or the field is out of range.
 */
/* ===================== Gold accessors, live-save/popup wiring & purse moves (europe_purse_nation .. europe_credit_sale_tax) ===================== */
int europe_purse_nation(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  const int n = (int)eu->bound_nation;
  return (n >= 0 && n < (int)COLONIZE_COL1_NATION_COUNT) ? n : -1;
}

/* Live screen for callers that hold only a save — see europe_set_live_screen. */
static EuropeScreen* g_europe_live_screen = NULL;

void europe_set_live_screen(EuropeScreen* eu) {
  g_europe_live_screen = eu;
}

void europe_set_live_save(struct ColonizeCol1Save* col1) {
  g_europe_live_save = col1;
}

void europe_set_popup_queue(AiPopupState* popups) {
  g_europe_popups = popups;
}

uint32_t europe_nation_gold(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int nation
) {
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0u;
  }
  if (!eu) {
    eu = g_europe_live_screen;
  }
  if (eu && nation == europe_purse_nation(eu)) {
    return eu->gold > 0 ? (uint32_t)eu->gold : 0u;
  }
  return col1 ? col1->nation[nation].gold : 0u;
}

void europe_nation_gold_add(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, long delta
) {
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  if (!eu) {
    eu = g_europe_live_screen;
  }
  if (eu && nation == europe_purse_nation(eu)) {
    long v = (long)eu->gold + delta;
    if (v < 0) {
      v = 0;
    }
    if (v > (long)INT_MAX) {
      v = (long)INT_MAX;
    }
    eu->gold = (int)v;
    if (col1) {
      col1->nation[nation].gold = (uint32_t)eu->gold;
    }
    return;
  }
  if (!col1) {
    return;
  }
  long v = (long)col1->nation[nation].gold + delta;
  if (v < 0) {
    v = 0;
  }
  if (v > (long)UINT32_MAX) {
    v = (long)UINT32_MAX;
  }
  col1->nation[nation].gold = (uint32_t)v;
}

void europe_gold_stamp_record(const EuropeScreen* eu, struct ColonizeCol1Save* col1) {
  if (!eu) {
    eu = g_europe_live_screen;
  }
  const int n = europe_purse_nation(eu);
  if (!eu || !col1 || n < 0) {
    return;
  }
  col1->nation[n].gold = (uint32_t)(eu->gold < 0 ? 0 : eu->gold);
}

/*
 * Module-internal treasury move for the Europe channels: DOS credits/debits
 * the record the module is pointed at, so the delta always lands on
 * `eu->gold` — whoever is currently borrowing it (units.c / ai_euro.c park an
 * AI treasury there for one call and assign it back) — and the col1 record is
 * re-stamped only when the purse really is that nation's. That is the missing
 * half of smell audit G3 for the harbor/transport channels: they moved
 * `eu->gold` alone and left the human's record for a later push to guess at.
 *
 * NOT the same as europe_nation_gold_add, which is for callers OUTSIDE this
 * module: they never borrow the purse, so a delta for a non-bound nation must
 * go to that nation's record, whereas here it must go to the borrowed purse.
 */
void europe_purse_move(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, long delta
) {
  if (!eu) {
    return;
  }
  long v = (long)eu->gold + delta;
  if (v < 0) {
    v = 0;
  }
  if (v > (long)INT_MAX) {
    v = (long)INT_MAX;
  }
  eu->gold = (int)v;
  if (col1 && nation >= 0 && nation == europe_purse_nation(eu)) {
    col1->nation[nation].gold = (uint32_t)eu->gold;
  }
}

int europe_cargo_boycotted_ex(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int nation, int cargo_type
) {
  if (cargo_type < 0 || cargo_type >= EUROPE_CARGO_MAX) {
    return 0;
  }
  uint16_t word = 0;
  if (col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT) {
    /* nation+0x20, DOS FUN_38fd_05e8's operand — the authoritative store. */
    word = col1->nation[nation].boycott_bitmap;
    if (word == 0xFFFFu) {
      word = 0; /* removed all-cargo-embargo fingerprint; both bridge
                 * directions heal it (col1_bridge.c) — never trade-block on it */
    }
  } else if (eu) {
    word = eu->boycott_bitmap; /* render mirror: tests / chrome / no save bound */
  }
  return (word & (uint16_t)(1u << cargo_type)) != 0;
}

int europe_cargo_boycotted(const EuropeScreen* eu, int cargo_type) {
  return europe_cargo_boycotted_ex(eu, NULL, -1, cargo_type);
}

int europe_buyback_boycott_cost(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int human_nation, int cargo_type
) {
  if (!eu || human_nation < 0 || human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  if (!europe_cargo_boycotted_ex(eu, col1, human_nation, cargo_type)) {
    return 0;
  }
  const int price = eu->cargo[cargo_type].ask;
  if (price <= 0) {
    return 0;
  }
  return price * 500;
}

int europe_buyback_boycott(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int human_nation, int cargo_type
) {
  /*
   * FUN_38fd_2dfe (viceroy_unpacked.c:60904-60945), clean disassembly, no
   * warnings. Real DOS trigger per GAME.TXT @SOMEBOYCOTT: "Some of the
   * cargo could not be unloaded because of a parliamentary boycott. If you
   * want to ask that the boycott be lifted, click on the cargo type in
   * question." — i.e. clicking a boycotted cell on the Europe market strip
   * (wired at that exact click site: game_loop.c EUROPE_HIT_MARKET).
   *
   * Traced formula (iVar3 = thunk_FUN_291f_0c3e -> FUN_38fd_0016, the same
   * "effective ask price" this file already exposes as
   * eu->cargo[cargo_type].ask):
   *   cost = ask_price * 500         // fandom "500 tons of that good"
   *   if nation.gold < cost: GAME.TXT @KISSSORRY, no state change
   *   else: nation.gold -= cost
   *         nation.royal_money += cost   // DOS write at nation+0x22, the
   *                                      // exact byte offset col1_save.h
   *                                      // already documents as royal_money
   *                                      // (REF budget, FUN_43f7_1d42) --
   *                                      // paying back taxes funds the Crown
   *         nation.boycott_bitmap &= ~(1 << cargo_type)   // nation+0x20
   * DOS gates this to the human-controlled nation (player_control[nation]
   * check at 0x543f); callers here only ever pass the human nation for the
   * same reason (only the human clicks their own market strip).
   *
   * The dialog chrome around this is DOS's own two-step and lives at the
   * click site (game_loop.c EUROPE_HIT_MARKET): @KISSUP is a 2-row CHOICE
   * whose SECOND row is "Pay {%NUMBER0$}." (38fd:2e5e compares the return of
   * FUN_281f_0652(0x1033, 2) against 2), and only after that answer does DOS
   * test the purse and show @KISSSORRY. This function is the applier — it
   * re-checks the purse itself, so a caller that skips the dialog (tests,
   * scripted play) still cannot overdraw.
   * Returns gold paid (>0) on success, 0 on no-op/insufficient funds.
   */
  if (!eu || !col1) {
    return 0;
  }
  if (human_nation < 0 || human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  if (!europe_cargo_boycotted_ex(eu, col1, human_nation, cargo_type)) {
    return 0;
  }
  const int price = eu->cargo[cargo_type].ask;
  if (price <= 0) {
    return 0;
  }
  const int cost = price * 500;
  if (eu->gold < cost) {
    /* GAME.TXT @KISSSORRY: "Unfortunately, we only have {%NUMBER0$}
     * available." — same sentence game_dialogs.c's KISSSORRY popup shows;
     * composed here via eu->messages (europe_set_messages) since this
     * applier re-checks the purse on its own. */
    eu->status[0] = '\0';
    if (eu->messages) {
      PopupMsgTokens tok = {0};
      tok.number0 = eu->gold;
      tok.has_number0 = true;
      popup_msg_fill(eu->messages, "KISSSORRY", &tok, "", eu->status, sizeof(eu->status));
      popup_msg_strip_markup(eu->status);
    }
    return 0;
  }
  europe_purse_move(eu, col1, human_nation, -cost);
  ColonizeCol1Nation* nation = &col1->nation[human_nation];
  nation->royal_money += cost;
  nation->boycott_bitmap &= (uint16_t)~(1u << cargo_type);
  eu->boycott_bitmap = nation->boycott_bitmap;
  const char* cname = eu->cargo[cargo_type].name[0] ? eu->cargo[cargo_type].name : "";
  snprintf(
    eu->status, sizeof(eu->status), "Paid %d$ in back taxes -- boycott on %s lifted.", cost, cname
  );
  diag_info(
    "EUROPE paid %d$ back taxes on %s: boycott lifted (gold=%d)", cost, cname, eu->gold
  );
  return cost;
}

int europe_sell_proceeds(const EuropeScreen* eu, int cargo_type, int amount) {
  if (!eu || amount <= 0 || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  /* FUN_38fd_1f0c: gross = (euro_price − 1)·amount; tax taken by the caller
   * as gross − gross·tax/100. */
  const int price = europe_sell_price(eu, cargo_type);
  if (price <= 0) {
    return 0;
  }
  return europe_net_after_tax(price * amount, eu->tax_percent);
}

/*
 * Crown cut of a Europe sale.
 *
 * DOS keeps the withheld tax: every sale arm writes `nation+0x22 += tax` —
 * the same 32-bit purse col1_save.h documents as royal_money (the REF budget
 * FUN_43f7_1d42 spends). Verified arms in this file: FUN_364b_0688 Custom
 * House (europe_custom_house_autosell_ex) and FUN_38fd_2dfe boycott buy-back
 * (europe_buyback_boycott). The harbor/transport sell paths credited the
 * player's gold but dropped the Crown's share, so all Europe tax revenue
 * vanished and the REF was systematically underfunded (smell audit #51).
 *
 * `gross` is the pre-tax sale value, `net` what europe_sell_proceeds paid out;
 * the difference is exactly what was withheld (same rounding, no second
 * division). No-op without a save record or on an untaxed (net == gross) sale.
 */
void europe_credit_sale_tax(
  struct ColonizeCol1Save* col1, int nation, int gross, int net
) {
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  const int tax_paid = gross - net;
  if (tax_paid <= 0) {
    return;
  }
  col1->nation[nation].royal_money += tax_paid;
}
