/*
 * Sections:
 *  - Harbor embark/disembark & voyage ticking (europe_harbor_push .. europe_notify_immigrant_sound)
 *  - Treasure cashing & price ledger (europe_cash_treasure .. europe_apply_volume_price)
 *  - Market price ticking & immigration pressure (europe_tick_market_prices_w .. europe_tick_immigration_pressure_w)
 *  - AI-nation immigration (DOS FUN_38fd_5e52, control ! 0) (europe_pool_view_from_nation .. europe_nation_immigration_tick_w)
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
static int europe_1d44_term(int amount, int seller_is_human, int difficulty);
static void europe_quote_settle(EuropeCargoQuote* q);
static void europe_pool_view_from_nation(
  EuropePoolView* v, const ColonizeCol1Save* col1, int nation_id
);
static int europe_nation_crosses_delta(const ColonizeWorld* w, int nation_id);

/* ===================== Harbor embark/disembark & voyage ticking (europe_harbor_push .. europe_notify_immigrant_sound) ===================== */
bool europe_harbor_push(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
) {
  return europe_harbor_push_ex(
    eu, type_index, name, cargo_types, NULL, cargo_count, hold_goods_type, hold_goods_amount
  );
}

bool europe_harbor_push_ex(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  const int* cargo_professions,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
) {
  if (!eu) {
    return false;
  }
  if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
    europe_set_status(eu, "Harbor is full.");
    return false;
  }
  EuropeHarborShip* slot = &eu->harbor[eu->harbor_ships++];
  europe_clear_ship(slot);
  slot->type_index = type_index;
  snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "");
  if (cargo_types && cargo_count > 0) {
    const int n = cargo_count > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : cargo_count;
    for (int i = 0; i < n; ++i) {
      slot->cargo_types[i] = cargo_types[i];
      /* Carry the @JOB through exactly as the Expected mirror
       * (europe_enqueue_expected) does — without it a pushed ship's
       * passengers lost their profession label
       * (reports_naval_passenger_label fell back to the bare unit name). */
      slot->cargo_professions[i] = cargo_professions ? cargo_professions[i] : -1;
    }
    slot->cargo_count = n;
  }
  if (hold_goods_type && hold_goods_amount) {
    for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
      slot->hold_goods_type[i] = hold_goods_type[i];
      slot->hold_goods_amount[i] = hold_goods_amount[i];
    }
  }
  slot->turns_left = 0;
  snprintf(eu->status, sizeof(eu->status), "%s in harbor.", slot->name);
  europe_refresh_harbor_selection(eu);
  return true;
}

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
) {
  if (!eu) {
    return false;
  }
  if (eu->expected_ships >= EUROPE_HARBOR_MAX) {
    europe_set_status(eu, "Expected Soon is full.");
    return false;
  }
  EuropeHarborShip* slot = &eu->expected[eu->expected_ships++];
  europe_clear_ship(slot);
  slot->type_index = type_index;
  snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "");
  if (cargo_types && cargo_count > 0) {
    const int n = cargo_count > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : cargo_count;
    for (int i = 0; i < n; ++i) {
      slot->cargo_types[i] = cargo_types[i];
      slot->cargo_professions[i] =
        cargo_professions ? cargo_professions[i] : -1;
    }
    slot->cargo_count = n;
  }
  if (hold_goods_type && hold_goods_amount) {
    for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
      slot->hold_goods_type[i] = hold_goods_type[i];
      slot->hold_goods_amount[i] = hold_goods_amount[i];
    }
  }
  slot->exit_x = exit_x;
  slot->exit_y = exit_y;
  slot->exit_east = exit_east;
  slot->turns_left = europe_clamp_voyage_turns(voyage_turns);
  slot->departed_this_turn = true;
  eu->last_exit_x = exit_x;
  eu->last_exit_y = exit_y;
  eu->last_exit_east = exit_east;
  eu->last_exit_valid = true;
  snprintf(
    eu->status,
    sizeof(eu->status),
    "%s expected in %d turn(s).",
    slot->name,
    slot->turns_left
  );
  return true;
}

void europe_board_sentry_dockers(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  ColonizeUnitPool* units,
  int nation_id,
  int cargo_cap
) {
  if (!eu || !ship || cargo_cap <= 0) {
    return;
  }
  const int goods = europe_goods_slots_used(ship);
  while (ship->cargo_count + goods < cargo_cap) {
    int di = -1;
    for (int i = 0; i < eu->dock_count; ++i) {
      if (eu->dock[i].present && eu->dock[i].sentry) {
        di = i;
        break; /* front of queue first */
      }
    }
    if (di < 0) {
      break;
    }
    int type_tag = 0;
    const bool is_artillery = europe_dock_name_is_artillery(eu->dock[di].name);
    if (is_artillery) {
      type_tag = -2;
    } else if (units) {
      /*
       * The dock entry's name is the expert plural ("Hardy Pioneers"), which
       * matches no @UNIT type, so this used to fall back to Colonists for
       * every specialist and the passenger landed as a plain colonist with no
       * kit. The entry now carries its own DOS type — set when it arrived and
       * moved by the @ARMOPTIONS rows — so a Soldier boards as Soldiers and a
       * Hardy Pioneer as Pioneers. The name lookup still wins for purchases
       * that really are typed by name (Artillery is handled above).
       */
      int ti = units_find_type(units, eu->dock[di].name);
      if (ti < 0) {
        ti = europe_dock_unit_type_index(units, eu->dock[di].dos_type);
      }
      type_tag = ti >= 0 ? ti : 0;
    }
    ship->cargo_types[ship->cargo_count] = type_tag;
    ship->cargo_professions[ship->cargo_count] = eu->dock[di].profession;
    ship->cargo_count++;
    /* The dock immigrant's (236,236) mirror unit leaves the pool with it. */
    europe_remove_dock_mirror_unit(units, nation_id, eu->dock[di].profession);
    for (int j = di + 1; j < eu->dock_count; ++j) {
      eu->dock[j - 1] = eu->dock[j];
    }
    eu->dock_count--;
    memset(&eu->dock[eu->dock_count], 0, sizeof(eu->dock[0]));
  }
}

bool europe_set_sail_from_harbor(
  EuropeScreen* eu,
  int harbor_index,
  int voyage_turns,
  ColonizeUnitPool* units,
  int nation_id
) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return false;
  }
  if (eu->bound_ships >= EUROPE_HARBOR_MAX) {
    europe_set_status(eu, "Outbound lane is full.");
    return false;
  }
  EuropeHarborShip ship;
  europe_copy_ship(&ship, &eu->harbor[harbor_index]);
  const int cargo_cap = europe_ship_cargo_cap(&ship, units);
  europe_board_sentry_dockers(eu, &ship, units, nation_id, cargo_cap);
  bool exit_east = eu->last_exit_valid ? eu->last_exit_east : true;
  ship.exit_east = exit_east;
  if (eu->last_exit_valid) {
    ship.exit_x = eu->last_exit_x;
    ship.exit_y = eu->last_exit_y;
  }
  ship.turns_left = europe_clamp_voyage_turns(voyage_turns);
  ship.departed_this_turn = true;
  for (int i = harbor_index + 1; i < eu->harbor_ships; ++i) {
    eu->harbor[i - 1] = eu->harbor[i];
  }
  eu->harbor_ships--;
  europe_clear_ship(&eu->harbor[eu->harbor_ships]);
  if (eu->selected_harbor == harbor_index) {
    eu->selected_harbor = -1;
  } else if (eu->selected_harbor > harbor_index) {
    eu->selected_harbor--;
  }
  europe_refresh_harbor_selection(eu);
  eu->bound[eu->bound_ships++] = ship;
  snprintf(
    eu->status,
    sizeof(eu->status),
    "%s bound for %s (%d turns).",
    ship.name,
    eu->colony_region[0] ? eu->colony_region : "",
    ship.turns_left
  );
  diag_info(
    "EUROPE %s departs for the colonies: %d turns, exit (%d,%d) %s",
    ship.name, ship.turns_left, ship.exit_x, ship.exit_y,
    ship.exit_east ? "east" : "west"
  );
  return true;
}

bool europe_reverse_transit(EuropeScreen* eu, bool from_expected, int index) {
  if (!eu) {
    return false;
  }
  if (from_expected) {
    if (index < 0 || index >= eu->expected_ships || eu->bound_ships >= EUROPE_HARBOR_MAX) {
      return false;
    }
    EuropeHarborShip ship = eu->expected[index];
    for (int i = index + 1; i < eu->expected_ships; ++i) {
      eu->expected[i - 1] = eu->expected[i];
    }
    eu->expected_ships--;
    europe_clear_ship(&eu->expected[eu->expected_ships]);
    eu->bound[eu->bound_ships++] = ship;
    europe_set_status(eu, "Reversed -- now sailing for the colonies.");
    return true;
  }
  if (index < 0 || index >= eu->bound_ships || eu->expected_ships >= EUROPE_HARBOR_MAX) {
    return false;
  }
  EuropeHarborShip ship = eu->bound[index];
  for (int i = index + 1; i < eu->bound_ships; ++i) {
    eu->bound[i - 1] = eu->bound[i];
  }
  eu->bound_ships--;
  europe_clear_ship(&eu->bound[eu->bound_ships]);
  eu->expected[eu->expected_ships++] = ship;
  europe_set_status(eu, "Reversed — now expected in Europe.");
  return true;
}

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
) {
  if (!eu || eu->harbor_ships <= 0) {
    return false;
  }
  if (out_type_index) {
    *out_type_index = eu->harbor[0].type_index;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", eu->harbor[0].name);
  }
  if (out_cargo_count) {
    *out_cargo_count = 0;
  }
  if (out_cargo_types && out_cargo_count && cargo_max > 0) {
    const int n =
      eu->harbor[0].cargo_count > cargo_max ? cargo_max : eu->harbor[0].cargo_count;
    for (int i = 0; i < n; ++i) {
      out_cargo_types[i] = eu->harbor[0].cargo_types[i];
    }
    *out_cargo_count = n;
  }
  if (out_hold_goods_type && out_hold_goods_amount && hold_max > 0) {
    const int n = hold_max > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : hold_max;
    for (int i = 0; i < n; ++i) {
      out_hold_goods_type[i] = eu->harbor[0].hold_goods_type[i];
      out_hold_goods_amount[i] = eu->harbor[0].hold_goods_amount[i];
    }
    for (int i = n; i < hold_max; ++i) {
      out_hold_goods_type[i] = 0;
      out_hold_goods_amount[i] = 0;
    }
  }
  for (int i = 1; i < eu->harbor_ships; ++i) {
    eu->harbor[i - 1] = eu->harbor[i];
  }
  eu->harbor_ships--;
  europe_clear_ship(&eu->harbor[eu->harbor_ships]);
  if (eu->selected_harbor == 0) {
    eu->selected_harbor = -1;
  } else if (eu->selected_harbor > 0) {
    eu->selected_harbor--;
  }
  europe_refresh_harbor_selection(eu);
  return true;
}

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
) {
  if (!eu) {
    return false;
  }
  int idx = -1;
  for (int i = 0; i < eu->bound_ships; ++i) {
    if (eu->bound[i].turns_left <= 0) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    return false;
  }
  EuropeHarborShip* ship = &eu->bound[idx];
  if (out_type_index) {
    *out_type_index = ship->type_index;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", ship->name);
  }
  if (out_cargo_count) {
    *out_cargo_count = 0;
  }
  if (out_cargo_types && out_cargo_count && cargo_max > 0) {
    const int n = ship->cargo_count > cargo_max ? cargo_max : ship->cargo_count;
    for (int i = 0; i < n; ++i) {
      out_cargo_types[i] = ship->cargo_types[i];
    }
    *out_cargo_count = n;
  }
  if (out_hold_goods_type && out_hold_goods_amount && hold_max > 0) {
    const int n = hold_max > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : hold_max;
    for (int i = 0; i < n; ++i) {
      out_hold_goods_type[i] = ship->hold_goods_type[i];
      out_hold_goods_amount[i] = ship->hold_goods_amount[i];
    }
  }
  if (out_exit_x) {
    *out_exit_x = ship->exit_x;
  }
  if (out_exit_y) {
    *out_exit_y = ship->exit_y;
  }
  if (out_exit_east) {
    *out_exit_east = ship->exit_east;
  }
  for (int i = idx + 1; i < eu->bound_ships; ++i) {
    eu->bound[i - 1] = eu->bound[i];
  }
  eu->bound_ships--;
  europe_clear_ship(&eu->bound[eu->bound_ships]);
  return true;
}

void europe_refresh_harbor_selection(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  if (eu->harbor_ships <= 0) {
    eu->selected_harbor = -1;
    return;
  }
  if (eu->selected_harbor >= 0 && eu->selected_harbor < eu->harbor_ships) {
    return;
  }
  eu->selected_harbor = 0;
}

void europe_tick_voyages(EuropeScreen* eu, const ColonizeUnitPool* units) {
  if (!eu) {
    return;
  }
  eu->open_on_dock = false;
  eu->docked_with_goods = false;
  /*
   * A ship that entered its lane during the turn just ending spends this
   * tick at sea without the counter moving — see EuropeHarborShip's
   * departed_this_turn. Without it an ordinary 1-turn crossing docked on
   * the very next turn, where DOS needs two End Turns (bugs.md).
   */
  for (int i = 0; i < eu->expected_ships; ++i) {
    if (eu->expected[i].departed_this_turn) {
      eu->expected[i].departed_this_turn = false;
    } else if (eu->expected[i].turns_left > 0) {
      eu->expected[i].turns_left--;
    }
  }
  for (int i = 0; i < eu->bound_ships; ++i) {
    if (eu->bound[i].departed_this_turn) {
      eu->bound[i].departed_this_turn = false;
    } else if (eu->bound[i].turns_left > 0) {
      eu->bound[i].turns_left--;
    }
  }
  /* Move due Expected ships into harbor; passengers go to dock front. */
  int i = 0;
  while (i < eu->expected_ships) {
    if (eu->expected[i].turns_left > 0) {
      ++i;
      continue;
    }
    if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
      break;
    }
    EuropeHarborShip ship = eu->expected[i];
    for (int j = i + 1; j < eu->expected_ships; ++j) {
      eu->expected[j - 1] = eu->expected[j];
    }
    eu->expected_ships--;
    europe_clear_ship(&eu->expected[eu->expected_ships]);
    ship.turns_left = 0;
    europe_disembark_passengers_to_dock(eu, &ship, units);
    eu->harbor[eu->harbor_ships++] = ship;
    eu->open_on_dock = true;
    /* 255 is the empty-hold sentinel, not a full hold — the same guard every
     * other hold consumer carries (europe_goods_slots_used, europe_sell_hold
     * / _partial, europe_best_sell_hold, europe_buy_cargo, reports.c's cargo
     * rows). units_despawn_ship_with_cargo copies the raw hold bytes in, so
     * without it a sentinel hold fired the once-per-game "Cargo from the New
     * World" woodcut (turn.c → WOODCUT_CARGO_FROM_THE_NEW_WORLD). */
    for (int g = 0; g < EUROPE_SHIP_CARGO_MAX; ++g) {
      if (ship.hold_goods_amount[g] > 0 && ship.hold_goods_amount[g] < 255) {
        eu->docked_with_goods = true;
        break;
      }
    }
    europe_refresh_harbor_selection(eu);
    /* bugs.md #750: DOS composes the arrival line through the DS:0x2d54 status
     * API and GAME.TXT has no docked-ship tag, so there is nothing to show. */
    eu->status[0] = '\0';
  }
}

void europe_set_sound_hook(void (*play_fn)(int id)) {
  g_europe_sound_play = play_fn;
}
void europe_set_bgm_hook(void (*set_bgm_fn)(int pool)) {
  g_europe_set_bgm = set_bgm_fn;
}
void europe_notify_immigrant_sound(EuropeScreen* eu) {
  (void)eu;
  if (g_europe_set_bgm) {
    g_europe_set_bgm(2); /* 281f_0498(2) for the human's immigrant beat */
  }
}

/* ===================== Treasure cashing & price ledger (europe_cash_treasure .. europe_apply_volume_price) ===================== */
int europe_cash_treasure(EuropeScreen* eu, int treasure_value) {
  if (!eu || treasure_value <= 0) {
    return 0;
  }
  int tax = eu->tax_percent;
  if (tax < 0) {
    tax = 0;
  }
  if (tax > 100) {
    tax = 100;
  }
  /*
   * FUN_48d3_06ba: Crown cut = min(tax_rate, 50). GAME.TXT @LOOTCASH still
   * describes tax-rate share; DOS clamps the treasure path at 50%.
   */
  if (tax > 50) {
    tax = 50;
  }
  const int credited = (treasure_value * (100 - tax)) / 100;
  const int crown_fee = treasure_value - credited;
  /*
   * FUN_48d3_06ba raw 78005-78018, all three against the current player's
   * record (DS:0x5394): gold (+0x2a) += net, +0x26 += the SAME net (the
   * write-only cumulative counter, int32 LE), royal_money (+0x22) += fee.
   * bugs.md #740 / #741.
   */
  const int purse_nation = europe_purse_nation(eu);
  struct ColonizeCol1Save* col1 = g_europe_live_save;
  europe_purse_move(eu, col1, purse_nation, credited);
  if (col1 && purse_nation >= 0 && purse_nation < (int)COLONIZE_COL1_NATION_COUNT) {
    ColonizeCol1Nation* nat = &col1->nation[purse_nation];
    nat->royal_money += (int32_t)crown_fee;
    uint32_t cum = (uint32_t)nat->unknown24_pad[0] | ((uint32_t)nat->unknown24_pad[1] << 8) |
                   ((uint32_t)nat->unknown24_pad[2] << 16) |
                   ((uint32_t)nat->unknown24_pad[3] << 24);
    cum += (uint32_t)credited;
    nat->unknown24_pad[0] = (uint8_t)(cum & 0xffu);
    nat->unknown24_pad[1] = (uint8_t)((cum >> 8) & 0xffu);
    nat->unknown24_pad[2] = (uint8_t)((cum >> 16) & 0xffu);
    nat->unknown24_pad[3] = (uint8_t)((cum >> 24) & 0xffu);
  }
  /* GAME.TXT @LOOTCASH, composed live via eu->messages (europe_set_messages). */
  const char* nation = eu->nation_name[0] ? eu->nation_name : "";
  const char* port = eu->port_city[0] ? eu->port_city : "";
  eu->status[0] = '\0';
  if (eu->messages) {
    PopupMsgTokens tok = {0};
    tok.string0 = nation;
    tok.string1 = port;
    tok.has_number0 = true;
    tok.number0 = treasure_value;
    tok.has_number1 = true;
    tok.number1 = tax;
    tok.has_number2 = true;
    tok.number2 = credited;
    popup_msg_fill(eu->messages, "LOOTCASH", &tok, "", eu->status, sizeof(eu->status));
    popup_msg_strip_markup(eu->status);
    /*
     * FUN_48d3_06ba raw 78020: FUN_281f_0652(0x148e, 2) — @LOOTCASH is a
     * MODAL here, not just a status line (the status line is the port's own
     * chrome and stays). bugs.md #742.
     */
    if (g_europe_popups) {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(eu->messages, "LOOTCASH", &tok, "", body, sizeof(body));
      ai_popup_enqueue_ok(g_europe_popups, AI_POPUP_TAG_INFO, NULL, body);
    }
  }
  diag_info(
    "EUROPE treasure cashed %d$: crown %d%% share, credited %d$ (gold=%d)",
    treasure_value, tax, credited, eu->gold
  );
  if (g_europe_sound_play) {
    g_europe_sound_play(0x24); /* FUN_48d3_06ba 48d3:0b8f: Fiddler's Dance queued on the cash-in */
  }
  return credited;
}

int europe_sell_price(const EuropeScreen* eu, int cargo_type) {
  /* FUN_38fd_0040: max(euro_price − 1, 0). */
  if (!eu || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  const int p = eu->cargo[cargo_type].bid - 1;
  return p < 0 ? 0 : p;
}

int europe_buy_price(const EuropeScreen* eu, int cargo_type) {
  /* FUN_38fd_0016: max(euro_price + burden, 0) — cached as `ask`. */
  if (!eu || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  return eu->cargo[cargo_type].ask < 0 ? 0 : eu->cargo[cargo_type].ask;
}

/* FUN_38fd_0040 / 0016 read the BOUND record (DS:0x84fc) — the acting
 * nation's own track. The screen holds the human's; any other nation reads
 * its col1 byte. bugs.md #1049. */
int europe_nation_sell_price(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int nation, int cargo_type
) {
  if (eu && (!col1 || nation == (int)eu->bound_nation)) {
    return europe_sell_price(eu, cargo_type);
  }
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT || cargo_type < 0 ||
      cargo_type >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 0;
  }
  const int p = (int)col1->nation[nation].trade.euro_price[cargo_type] - 1;
  return p < 0 ? 0 : p;
}

int europe_nation_buy_price(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int nation, int cargo_type
) {
  if (eu && (!col1 || nation == (int)eu->bound_nation)) {
    return europe_buy_price(eu, cargo_type);
  }
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT || cargo_type < 0 ||
      cargo_type >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 0;
  }
  /* The @CARGO burden column is one global table; the screen carries it. */
  const int burden = (eu && cargo_type < eu->cargo_count) ? eu->cargo[cargo_type].burden
                                                          : europe_cargo_burden(cargo_type);
  const int p = (int)col1->nation[nation].trade.euro_price[cargo_type] + burden;
  return p < 0 ? 0 : p;
}

int europe_net_after_tax(int gross, int tax_percent) {
  if (gross <= 0) {
    return 0;
  }
  if (tax_percent < 0) {
    tax_percent = 0;
  }
  if (tax_percent > 100) {
    tax_percent = 100;
  }
  /* FUN_364b_0688: tax = (tax·gross)/100 (32-bit), net = gross − tax. */
  const long taxed = ((long)tax_percent * (long)gross) / 100L;
  return gross - (int)taxed;
}

static int europe_1d44_term(int amount, int seller_is_human, int difficulty) {
  /* FUN_38fd_1d44 (viceroy_unpacked.c 60186-60201):
   *   k = (0x9e12 < 4 && 0x543f[0x9e12*0x34] == 0) ? DS:0x53a6 − 2 : −2
   * i.e. the BOUND nation (the one whose record 0x84fc the ledger credits =
   * the trading nation) being human-controlled picks the difficulty arm; an
   * AI (or a crown/indian slot ≥ 4) always gets −2. Then
   * (k · 16 · amount) / 100, C division (truncates toward zero — the AI
   * −384/100 → −3 case is what the dutch2 pair needs). */
  const int k = seller_is_human ? (difficulty - 2) : -2;
  return (k * 16 * amount) / 100;
}

/* Clamp bid into @CARGO [low, high] (only when both were loaded), floor at 0,
 * re-derive ask = bid + burden. Shared by the player-move and EOT tickers. */
static void europe_quote_settle(EuropeCargoQuote* q) {
  if (q->high > q->low) {
    if (q->bid < q->low) {
      q->bid = q->low;
    }
    if (q->bid > q->high) {
      q->bid = q->high;
    }
  }
  if (q->bid < 0) {
    q->bid = 0;
  }
  q->ask = q->bid + q->burden;
}

/* FUN_38fd_0058(0, cargo) over the screen's own track — the single-cargo
 * step 1dfa/1d80 run after a trade. Defined with the market tick below. */
static void europe_market_tick_single_cargo(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, int cargo
);
/* Write the screen's track through to its nation's col1 row. */
static void europe_screen_track_to_col1(const EuropeScreen* eu, struct ColonizeCol1Save* col1);

void europe_apply_trade_volume(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int human_nation,
  int cargo_type,
  int amount,
  int is_buy,
  int immediate_threshold
) {
  if (!eu || amount <= 0 || cargo_type < 0 || cargo_type >= eu->cargo_count ||
      cargo_type >= EUROPE_CARGO_MAX) {
    return;
  }
  EuropeCargoQuote* q = &eu->cargo[cargo_type];
  int shift = q->volatility;
  if (shift < 0) {
    shift = 0;
  }
  if (shift > 15) {
    shift = 15;
  }
  int difficulty = eu->difficulty;
  if (col1) {
    difficulty = (int)col1->head.difficulty;
  }
  /* 1d44's `0x9e12 < 4` guard: a crown/indian slot (or an unknown seller)
   * never takes the human arm. */
  const int seller_is_human =
    (seller_nation >= 0 && seller_nation < 4 && seller_nation == human_nation);
  const int raw_term = (amount << shift) + europe_1d44_term(amount, seller_is_human, difficulty);
  /* DOS walks all four records (bugs.md #1047): the screen's own track lives
   * in eu->trade_nr, the other three in col1->nation[n].trade.nr. The Dutch
   * record (slot 3) takes (term·2)/3 regardless of who sold — SELL side
   * only: FUN_38fd_1dfa's `if (local_a == 3) iVar5 = (iVar3*2)/3` (viceroy
   * 60263-60268) has no counterpart in the buy routine FUN_38fd_1d80, which
   * subtracts the undamped term from all four records (viceroy 60216-60221). */
  const int track = (int)eu->bound_nation;
  if (col1 && (unsigned)cargo_type < COLONIZE_COL1_CARGO_TYPES) {
    for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
      if (n == track) {
        continue;
      }
      const int tn = (!is_buy && n == 3) ? (raw_term * 2) / 3 : raw_term;
      int v = (int)col1->nation[n].trade.nr[cargo_type] + (is_buy ? -tn : tn);
      if (v < -32768) {
        v = -32768;
      }
      if (v > 32767) {
        v = 32767;
      }
      col1->nation[n].trade.nr[cargo_type] = (int16_t)v;
    }
  }
  const int term = (!is_buy && track == 3) ? (raw_term * 2) / 3 : raw_term;
  int nr = (int)eu->trade_nr[cargo_type];
  if (is_buy) {
    nr -= term;
  } else {
    nr += term;
  }
  if (col1 && seller_nation >= 0 && seller_nation < (int)COLONIZE_COL1_NATION_COUNT &&
      (unsigned)cargo_type < COLONIZE_COL1_CARGO_TYPES) {
    ColonizeCol1NationTrade* t = &col1->nation[seller_nation].trade;
    const int32_t signed_amt = is_buy ? -(int32_t)amount : (int32_t)amount;
    t->tons[cargo_type] += signed_amt;
    t->tons2[cargo_type] += signed_amt;
    if (is_buy) {
      /* 1d80: gold[cargo] −= buy_price·amount. */
      t->gold[cargo_type] -=
        (int32_t)europe_nation_buy_price(eu, col1, seller_nation, cargo_type) * (int32_t)amount;
    } else {
      /* 1dfa: gold[cargo] += (sell_price·amount·(100−tax))/100 — note the
       * ledger rounds the other way from the treasury credit (54 lumber @1,
       * 35% → ledger +35, treasury +36). */
      const int tax = (int)col1->nation[seller_nation].tax_rate;
      const long gross =
        (long)europe_nation_sell_price(eu, col1, seller_nation, cargo_type) * (long)amount;
      t->gold[cargo_type] += (int32_t)((gross * (long)(100 - tax)) / 100L);
    }
  }
  if (nr < -32768) {
    nr = -32768;
  }
  if (nr > 32767) {
    nr = 32767;
  }
  eu->trade_nr[cargo_type] = (int16_t)nr;
  if (!immediate_threshold) {
    europe_screen_track_to_col1(eu, col1);
    return;
  }
  /*
   * The 1dfa/1d80 tail: the pressure word takes the volume term above and
   * then the whole of FUN_38fd_0058(0, cargo) runs — phase 1 (the ledger, and
   * the DS:0x53ea pool decay when the bound record is nation 0), the phase
   * 2/3 nudge for this cargo, and phase 4's threshold step with the attrition
   * added and taken back out again (raw 58989-58992). bugs.md #1017; the
   * @PRICEUP/@PRICEDOWN crumb for a player-move price change (bugs.md #225)
   * is queued inside phase 4 like the EOT tick's.
   */
  europe_market_tick_single_cargo(eu, col1, (int)eu->bound_nation, cargo_type);
}

void europe_apply_volume_price(EuropeScreen* eu, int cargo_type, int amount, int is_buy) {
  /* Harbor buy/sell with no col1 at hand: DOS keys both the 1d44 human test
   * and the 1dfa Dutch damping off the BOUND nation (DS:0x9e12), which every
   * port caller of europe_set_nation sets to the human — so pass it as both
   * seller and human instead of the old (−1, −1) pair, which read as "human
   * seller" by accident and could never trigger the Dutch arm (smell audit
   * #57/#58). Difficulty falls back to eu->difficulty (the 0x53a6 mirror).
   * Then FUN_38fd_0058(0, cargo). Callers holding a col1 should use
   * europe_apply_trade_volume directly. */
  const int bound = eu ? (int)eu->bound_nation : 0;
  europe_apply_trade_volume(eu, NULL, bound, bound, cargo_type, amount, is_buy, 1);
}

/* ===================== Market price ticking & immigration pressure (europe_tick_market_prices_w .. europe_tick_immigration_pressure_w) ===================== */
/*
 * DOS FUN_38fd_0058 (viceroy_unpacked.c 58741-59006) — the whole market
 * engine, run against whatever nation record DS:0x84fc is bound to
 * (DS:0x9e12 = that nation's index). Its two parameters:
 *   param_1 — 0 = the full tick (phases 1-4, the one 5e52 runs every EOT and
 *             the one 1dfa/1d80 run after a single trade); non-0 = the
 *             FUN_38fd_6024 campaign-seed tail, which runs phase 1 and then
 *             only phase 2's clamp arm (bugs.md #1016).
 *   param_2 — <0 = every cargo; otherwise the one cargo a single trade moved
 *             (phase 1 still walks all 16, including the pool decay).
 * DOS keeps FOUR independent price tracks — bid at record +0x4c, pressure
 * word at +0x5c — and 5e52 ticks each nation's own (bugs.md #1011/#1012).
 * This file therefore runs the phases over a `bid`/`nr` pair the caller
 * points at either the human's EuropeScreen or a nation's col1 record; the
 * @CARGO columns (low/high/rise/fall/attrition) are a global table and come
 * from the screen either way.
 */
typedef struct EuropeMarketTick {
  EuropeScreen* eu; /* @CARGO table + (when `events`) the popup crumb sink */
  struct ColonizeCol1Save* col1;
  int nation; /* DS:0x9e12 — the bound record */
  int mode; /* param_1 */
  int only_cargo; /* param_2 */
  uint32_t turn; /* DS:0x538e */
  int difficulty; /* DS:0x53a6 */
  bool ai_bound; /* bound nation is AI- or Crown-controlled */
  bool events; /* queue @PRICEUP/@PRICEDOWN crumbs for this track */
  int* bid; /* EUROPE_CARGO_MAX slots, in/out */
  int16_t* nr; /* EUROPE_CARGO_MAX slots, in/out */
} EuropeMarketTick;

static void europe_market_nr_add(const EuropeMarketTick* t, int cargo, int delta) {
  int nr = (int)t->nr[cargo] + delta;
  if (nr < -32768) {
    nr = -32768;
  }
  if (nr > 32767) {
    nr = 32767;
  }
  t->nr[cargo] = (int16_t)nr;
}

/*
 * Phase 1 (raw 58789-58820) — ledger[c] = market_demand_pool[c] (signed) plus
 * every nation's tons2[c], negative entries read as 0. Then, in the bound
 * nation 0 pass of a full tick only, the stored pool sheds ledger>>7. The
 * ledger handed to phases 2-3 is the PRE-decay sum: DOS subtracts from
 * DS:0x53ea and leaves its stack copy alone.
 */
static void europe_market_phase1(const EuropeMarketTick* t, long ledger[16]) {
  struct ColonizeCol1Save* col1 = t->col1;
  for (int c = 0; c < 16; ++c) {
    long s = 0;
    if (col1) {
      s = (long)(int16_t)col1->head.market_demand_pool[c];
      for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
        const int32_t v = col1->nation[n].trade.tons2[c];
        if (v > 0) {
          s += (long)v;
        }
      }
      if (t->mode == 0 && t->nation == 0) {
        const long decayed = (long)(int16_t)col1->head.market_demand_pool[c] - (s >> 7);
        col1->head.market_demand_pool[c] = (uint16_t)(int16_t)decayed;
      }
    }
    ledger[c] = s;
  }
}

/*
 * sign(bid − ratio) the way DOS takes it: the FUN_1d1d_0ec6 quotient is
 * truncated to a 16-bit int and subtracted from the signed bid byte in 16
 * bits (raw ~58845 `local_6 - (int)lVar15`, ~58899 `local_52`). Only differs
 * from a plain compare once the ratio passes 32767. bugs.md #1051.
 */
static int europe_market_bid_sign(int bid, long ratio) {
  const int16_t d = (int16_t)(uint16_t)((unsigned)(int8_t)bid - (unsigned)(uint16_t)ratio);
  return (d > 0) - (d < 0);
}

/*
 * Phase 2 (raw 58828-58874) — cargos 9..12 off their own ledger share.
 * ratio = (Σledger[9..12] · 3) / ledger[c]; a full tick nudges the pressure
 * word by sign(bid − ratio)·((rise+fall)/2)·100, the 6024 tail writes the
 * ratio straight into the bid, clamped into [low, high].
 */
static void europe_market_phase2(const EuropeMarketTick* t, const long ledger[16]) {
  long sum = ledger[9] + ledger[10] + ledger[11] + ledger[12];
  if (sum <= 0) {
    sum = 1;
  }
  for (int c = 9; c <= 12; ++c) {
    if (c >= t->eu->cargo_count || c >= EUROPE_CARGO_MAX) {
      break;
    }
    if (t->only_cargo >= 0 && c != t->only_cargo) {
      continue;
    }
    long L = ledger[c];
    if (L <= 0) {
      L = 1;
    }
    const long ratio = (sum * 3) / L;
    const EuropeCargoQuote* q = &t->eu->cargo[c];
    if (t->mode == 0) {
      const int sign = europe_market_bid_sign(t->bid[c], ratio);
      europe_market_nr_add(t, c, sign * ((q->rise + q->fall) / 2) * 100);
    } else {
      long v = ratio;
      if (v < (long)q->low) {
        v = q->low;
      }
      if (v > (long)q->high) {
        v = q->high;
      }
      t->bid[c] = (int)v;
    }
  }
}

/*
 * Phase 3 (raw 58876-58925, full tick only) — cargos 1..4. The denominator
 * pool is ledger[4]/2 + ledger[1] + ledger[2] + ledger[3] (the loop's own
 * four cargos, with cargo 4 halved exactly as its in-loop L is — DOS reads
 * the stack ledger slots of cargo 4, not cargo 0; bugs.md #1022), and the
 * nudge is ((rise+fall)/2)·sign with no ×100. Cargo 4 also takes the two
 * year biases (+1 before 1700, +1 again before 1600).
 */
static void europe_market_phase3(const EuropeMarketTick* t, const long ledger[16]) {
  long sum = (ledger[4] >> 1) + ledger[1] + ledger[2] + ledger[3];
  if (sum <= 0) {
    sum = 1;
  }
  const int year = t->col1 ? (int)t->col1->head.year : 0;
  for (int c = 1; c <= 4; ++c) {
    if (c >= t->eu->cargo_count || c >= EUROPE_CARGO_MAX) {
      break;
    }
    if (t->only_cargo >= 0 && c != t->only_cargo) {
      continue;
    }
    long L = ledger[c];
    if (c == 4) {
      L >>= 1;
    }
    if (L <= 0) {
      L = 1;
    }
    long ratio = (sum * 3) / L;
    if (c == 4) {
      if (year < 0x6a4) {
        ratio += 1; /* < 1700 */
      }
      if (year < 0x640) {
        ratio += 1; /* < 1600 */
      }
    }
    const EuropeCargoQuote* q = &t->eu->cargo[c];
    const int sign = europe_market_bid_sign(t->bid[c], ratio);
    europe_market_nr_add(t, c, ((q->rise + q->fall) / 2) * sign);
  }
}

/*
 * Phase 4 (raw 58926-59002, full tick only) — attrition, then the ±1 bid step
 * at the ±rise·100 / fall·100 thresholds, then the two AI-only price ceilings.
 * The pressure word always sheds the threshold even when the bid is capped
 * (golden_market_prices01 caught the other way round on Rum at 20).
 * Returns the last cargo that rose / fell through `out_rise` / `out_fall` for
 * the status line.
 */
static void europe_market_phase4(const EuropeMarketTick* t, int* out_rise, int* out_fall) {
  EuropeScreen* eu = t->eu;
  for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
    if (t->only_cargo >= 0 && c != t->only_cargo) {
      continue;
    }
    const EuropeCargoQuote* q = &eu->cargo[c];
    /*
     * raw 58932-58941: for an AI/Crown nation the top two cargos get a
     * tighter rise cap that relaxes as the game runs on, and a bid already
     * above it is pushed back down through the pressure word.
     */
    int cap = q->high;
    if (c > 13 && t->ai_bound) {
      cap = q->high + (t->difficulty - 4) * 2 + ((int)t->turn - 600) / 100;
      if (t->only_cargo < 0 && cap < t->bid[c]) {
        europe_market_nr_add(t, c, q->fall * 100);
      }
    }
    /* DOS: `0x9e12 == 3 && (turn & 1)` — the Netherlands' market recovers
     * twice as fast on odd turns (turn already incremented for this EOT). */
    int attrition = q->attrition;
    if (t->nation == 3 && (t->turn & 1u) != 0u) {
      attrition <<= 1;
    }
    europe_market_nr_add(t, c, attrition);
    int nr = (int)t->nr[c];
    const int rise = q->rise;
    const int fall = q->fall;
    if (rise > 0 && nr <= -(rise * 100)) {
      nr += rise * 100;
      if (t->bid[c] < cap) {
        t->bid[c] += 1;
        if (out_rise) {
          *out_rise = c;
        }
        if (t->events && eu->price_event_count < EUROPE_CARGO_MAX) {
          eu->price_event_cargo[eu->price_event_count] = c;
          eu->price_event_dir[eu->price_event_count] = 1;
          eu->price_event_count++;
        }
      }
    }
    if (fall > 0 && nr >= fall * 100) {
      nr -= fall * 100;
      if (t->bid[c] > q->low) {
        t->bid[c] -= 1;
        if (out_fall) {
          *out_fall = c;
        }
        if (t->events && eu->price_event_count < EUROPE_CARGO_MAX) {
          eu->price_event_cargo[eu->price_event_count] = c;
          eu->price_event_dir[eu->price_event_count] = -1;
          eu->price_event_count++;
        }
      }
    }
    /* raw 58989-58992: a single-cargo call takes the attrition back out. */
    if (t->only_cargo >= 0) {
      nr -= attrition;
    }
    if (nr < -32768) {
      nr = -32768;
    }
    if (nr > 32767) {
      nr = 32767;
    }
    t->nr[c] = (int16_t)nr;
    /*
     * raw 58994-58999: an AI/Crown nation's bid for cargos 8/14/15 is capped
     * at ((4 − difficulty)·3)/2 + 3 — 9 on Discoverer down to 3 on Viceroy.
     * This is a ceiling, not a floor: DOS keeps min(bid, cap).
     */
    if (t->ai_bound && (c == 8 || c == 14 || c == 15)) {
      const int ceiling = ((4 - t->difficulty) * 3) / 2 + 3;
      if (t->bid[c] > ceiling) {
        t->bid[c] = ceiling;
      }
    }
  }
}

/* FUN_38fd_0058(mode, only_cargo) over one nation's track. */
static void europe_market_tick_core(const EuropeMarketTick* t, int* out_rise, int* out_fall) {
  long ledger[16];
  europe_market_phase1(t, ledger);
  europe_market_phase2(t, ledger);
  if (t->mode != 0) {
    return;
  }
  europe_market_phase3(t, ledger);
  europe_market_phase4(t, out_rise, out_fall);
}

/* Fill the shared parts of a tick descriptor for `nation`. */
static EuropeMarketTick europe_market_tick_for(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int nation,
  int mode,
  int only_cargo,
  uint32_t turn
) {
  EuropeMarketTick t;
  memset(&t, 0, sizeof(t));
  t.eu = eu;
  t.col1 = col1;
  t.nation = nation;
  t.mode = mode;
  t.only_cargo = only_cargo;
  t.turn = turn;
  t.difficulty = eu->difficulty;
  if (col1) {
    t.difficulty = (int)col1->head.difficulty;
    /* raw 58932/58994: `3 < 0x9e12 || control != 0` — the AI/Crown arms. */
    t.ai_bound = nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT ||
                 col1->player[nation].control != 0;
  }
  return t;
}

/* Load / store a nation record's own price track (bid +0x4c, pressure +0x5c). */
static void europe_market_track_load(
  const struct ColonizeCol1Save* col1, int nation, int cargo_count, int* bid, int16_t* nr
) {
  for (int c = 0; c < cargo_count && c < EUROPE_CARGO_MAX; ++c) {
    bid[c] = (c < (int)COLONIZE_COL1_CARGO_TYPES)
               ? (int)col1->nation[nation].trade.euro_price[c]
               : 0;
    nr[c] = (c < (int)COLONIZE_COL1_CARGO_TYPES) ? col1->nation[nation].trade.nr[c] : 0;
  }
}

static void europe_market_track_store(
  struct ColonizeCol1Save* col1, int nation, int cargo_count, const int* bid, const int16_t* nr
) {
  for (int c = 0; c < cargo_count && c < EUROPE_CARGO_MAX &&
                  c < (int)COLONIZE_COL1_CARGO_TYPES;
       ++c) {
    int v = bid[c];
    if (v < 0) {
      v = 0;
    }
    if (v > 255) {
      v = 255;
    }
    col1->nation[nation].trade.euro_price[c] = (uint8_t)v;
    col1->nation[nation].trade.nr[c] = nr[c];
  }
}

/*
 * DOS has ONE store per nation (record +0x4c / +0x5c) and refreshes the
 * DS:0x84bc mirror in every 0058 pass; the port's human track lives in the
 * EuropeScreen, so every write to it goes straight through to the col1 row
 * the rest of the sim reads (bugs.md #1050).
 */
static void europe_screen_track_to_col1(const EuropeScreen* eu, struct ColonizeCol1Save* col1) {
  if (!eu || !col1 || eu->bound_nation >= COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  int bid[EUROPE_CARGO_MAX];
  for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
    bid[c] = eu->cargo[c].bid;
  }
  europe_market_track_store(col1, (int)eu->bound_nation, eu->cargo_count, bid, eu->trade_nr);
}

void europe_nation_tick_market_prices_w(const ColonizeWorld* w, int nation, uint32_t turn) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1_ok ? w->col1 : NULL;
  if (!eu || !col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  int bid[EUROPE_CARGO_MAX];
  int16_t nr[EUROPE_CARGO_MAX];
  memset(bid, 0, sizeof(bid));
  memset(nr, 0, sizeof(nr));
  europe_market_track_load(col1, nation, eu->cargo_count, bid, nr);
  EuropeMarketTick t = europe_market_tick_for(eu, col1, nation, 0, -1, turn);
  t.bid = bid;
  t.nr = nr;
  /* Silent: every @PRICEUP/@PRICEDOWN dialog in 0058 is gated on the bound
   * nation being human (raw 58958/58977). */
  t.events = false;
  europe_market_tick_core(&t, NULL, NULL);
  europe_market_track_store(col1, nation, eu->cargo_count, bid, nr);
}

static void europe_market_tick_single_cargo(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, int cargo
) {
  if (!eu || cargo < 0 || cargo >= eu->cargo_count || cargo >= EUROPE_CARGO_MAX) {
    return;
  }
  int bid[EUROPE_CARGO_MAX];
  int16_t nr[EUROPE_CARGO_MAX];
  memset(bid, 0, sizeof(bid));
  memset(nr, 0, sizeof(nr));
  for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
    bid[c] = eu->cargo[c].bid;
    nr[c] = eu->trade_nr[c];
  }
  EuropeMarketTick t = europe_market_tick_for(
    eu, col1, nation, 0, cargo, col1 ? (uint32_t)col1->head.turn : 0u
  );
  t.bid = bid;
  t.nr = nr;
  t.events = true;
  europe_market_tick_core(&t, NULL, NULL);
  for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
    eu->cargo[c].bid = bid[c];
    eu->trade_nr[c] = nr[c];
    europe_quote_settle(&eu->cargo[c]);
  }
  europe_screen_track_to_col1(eu, col1);
}

void europe_tick_market_prices_w(
  const ColonizeWorld* w,
  int human_nation,
  uint32_t turn
) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1;

  /*
   * The human's own 5e52 phase-3 call, FUN_38fd_0058(0, 0xffff). Validated
   * 2026-08-28 against two real-DOS turn pairs (golden_market_prices01;
   * python replica iterated until both matched). The other three nations tick
   * through europe_nation_tick_market_prices_w (bugs.md #1012); DOS runs each
   * nation's call inside its own end-of-turn 5e52, and the pool decay belongs
   * to nation 0's pass, so the caller walks the nations in index order.
   * Column roles (NAMES.TXT @CARGO): rise=c6, fall=c7, attrition=c8.
   * Cite: viceroy_unpacked.c 58741-59006; turn/europe_nation_eot.md.
   */
  if (!eu) {
    return;
  }
  eu->price_event_count = 0;
  if (col1) {
    eu->difficulty = col1->head.difficulty > 8 ? 8 : col1->head.difficulty;
    /* FUN_38fd_0718 raw 59120-59122: the 0x543f control byte of the bound
     * nation picks the Dragoon-roll bound. */
    eu->bound_human = eu->bound_nation < COLONIZE_COL1_NATION_COUNT &&
                      col1->player[eu->bound_nation].control == 0;
  }

  int bid[EUROPE_CARGO_MAX];
  int16_t nr[EUROPE_CARGO_MAX];
  memset(bid, 0, sizeof(bid));
  memset(nr, 0, sizeof(nr));
  for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
    bid[c] = eu->cargo[c].bid;
    nr[c] = eu->trade_nr[c];
  }
  EuropeMarketTick t = europe_market_tick_for(eu, col1, human_nation, 0, -1, turn);
  t.bid = bid;
  t.nr = nr;
  t.events = true;
  int last_rise = -1;
  int last_fall = -1;
  europe_market_tick_core(&t, &last_rise, &last_fall);
  for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
    eu->cargo[c].bid = bid[c];
    eu->trade_nr[c] = nr[c];
    europe_quote_settle(&eu->cargo[c]);
  }
  europe_screen_track_to_col1(eu, col1);
  /*
   * Phase 4 dialog crumbs 0xfa8/0xfb0 -> price_event_cargo[]/dir[] (queued
   * in loop order above, one entry per cargo that actually crossed
   * threshold this tick — DOS calls FUN_281f_0652 inline per cargo, so two
   * different cargos changing the same turn both get their own OK dialog;
   * turn.c walks the full list). The status line below stays a single-line
   * summary (rise wins ties, matching the old behaviour) — cosmetic only.
   * Real DOS wording from GAME.TXT @PRICEUP/@PRICEDOWN (COLONIZE/GAME.TXT:1683-1689):
   *   "The price of {%STRING0} in %STRING1 has risen to {%NUMBER0$}."
   *   "The price of {%STRING0} in %STRING1 has fallen to {%NUMBER0$}."
   * STRING0 = cargo name (-0x6840 @CARGO table), STRING1 = nation home-port
   * city (-0x7c74 table == eu->port_city), NUMBER0 = new bid.
   */
  if (last_rise >= 0 && eu->messages) {
    PopupMsgTokens tok = {0};
    tok.string0 = eu->cargo[last_rise].name;
    tok.string1 = eu->port_city;
    tok.has_number0 = true;
    tok.number0 = eu->cargo[last_rise].bid;
    popup_msg_fill(eu->messages, "PRICEUP", &tok, "", eu->status, sizeof(eu->status));
    popup_msg_strip_markup(eu->status);
  } else if (last_fall >= 0 && eu->messages) {
    PopupMsgTokens tok = {0};
    tok.string0 = eu->cargo[last_fall].name;
    tok.string1 = eu->port_city;
    tok.has_number0 = true;
    tok.number0 = eu->cargo[last_fall].bid;
    popup_msg_fill(eu->messages, "PRICEDOWN", &tok, "", eu->status, sizeof(eu->status));
    popup_msg_strip_markup(eu->status);
  } else if (last_rise >= 0 || last_fall >= 0) {
    eu->status[0] = '\0';
  }
}

/*
 * The FUN_38fd_6024 campaign tail (raw 68655-68660): bind each nation and run
 * FUN_38fd_0058(1, -1), which rewrites cargos 9..12 from the (still empty)
 * ledger instead of the roll that just landed. bugs.md #1016.
 */
void europe_market_seed_tail_w(const ColonizeWorld* w, int nation, uint32_t turn) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1_ok ? w->col1 : NULL;
  if (!eu || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  int bid[EUROPE_CARGO_MAX];
  int16_t nr[EUROPE_CARGO_MAX];
  memset(bid, 0, sizeof(bid));
  memset(nr, 0, sizeof(nr));
  const bool on_screen = (eu->bound_nation == (uint8_t)nation);
  if (on_screen || !col1) {
    for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
      bid[c] = eu->cargo[c].bid;
      nr[c] = eu->trade_nr[c];
    }
  } else {
    europe_market_track_load(col1, nation, eu->cargo_count, bid, nr);
  }
  EuropeMarketTick t = europe_market_tick_for(eu, col1, nation, 1, -1, turn);
  t.bid = bid;
  t.nr = nr;
  europe_market_tick_core(&t, NULL, NULL);
  if (col1) {
    europe_market_track_store(col1, nation, eu->cargo_count, bid, nr);
  }
  if (on_screen) {
    for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
      eu->cargo[c].bid = bid[c];
      eu->trade_nr[c] = nr[c];
      europe_quote_settle(&eu->cargo[c]);
    }
  }
}

int europe_compute_immigration_score_w(
  const ColonizeWorld* w,
  int nation_id
) {
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeUnitPool* units = w->units;
  const ColonizeCol1Save* col1 = w->col1;

  /*
   * FUN_38fd_584a: score ≈ colony pop sum + unit count; <<1 if <4000; +8;
   * cap 4000; AI/non-human ((8-diff)*score)>>3; nation0 *2/3.
   * Cite: europe_nation_eot.md phase 4; ~68248.
   */
  if (nation_id < 0 || nation_id > 3) {
    return 0;
  }
  int pop = 0;
  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (c->active && c->nation_id == nation_id) {
        pop += c->colonist_count > 0 ? c->colonist_count : c->population;
      }
    }
  }
  int units_n = 0;
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (u->active && u->nation_id == nation_id) {
        units_n++;
      }
    }
  }
  /* DOS counts every unit record of the nation, including colonists waiting
   * on the Europe docks — those are the (236,236) mirror units in the pool
   * (seed-100 TURN5→6: needed 9→10 once the first immigrant landed). */
  int score = pop + units_n;
  if (score < 4000) {
    score <<= 1;
  }
  score += 8;
  if (score > 4000) {
    score = 4000;
  }
  if (col1) {
    const int control =
      (nation_id < 4) ? (int)col1->player[nation_id].control : 1;
    if (nation_id > 3 || control != 0) {
      int diff = (int)col1->head.difficulty;
      if (diff < 0) {
        diff = 0;
      }
      if (diff > 8) {
        diff = 8;
      }
      score = ((8 - diff) * score) >> 3;
    }
    if (nation_id == 0) {
      score = (score << 1) / 3;
    }
  }
  return score;
}


int europe_tick_immigration_pressure_w(
  const ColonizeWorld* w,
  int nation_id
) {
  EuropeScreen* eu = w->europe;
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeUnitPool* units = w->units;
  const ColonizeCol1Save* col1 = w->col1;
  ColonizeDosRng* rng = w->rng;

  /*
   * DOS: +0x30 = 584a score (needed_crosses); +0x2e += 2 (and church crosses
   * already applied by caller); spawn when score < pressure. Cite: 5e52 ~68558.
   */
  if (!eu || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  if (col1) {
    int diff = (int)col1->head.difficulty;
    if (diff < 0) {
      diff = 0;
    }
    if (diff > 8) {
      diff = 8;
    }
    eu->difficulty = (uint8_t)diff;
    eu->bound_human = nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
                      col1->player[nation_id].control == 0;
  }
  /* DOS-LITERAL FUN_38fd_5e52 raw 68556: the tick clears nation flag bit
   * 0x20 on entry (`*(byte *)*0x84fc &= 0xdf`). The AI twin
   * europe_nation_immigration_tick_w already did this; the human path did
   * not (bugs.md #923). */
  if (w->col1 && nation_id < (int)COLONIZE_COL1_NATION_COUNT) {
    w->col1->nation[nation_id].nation_flags =
      (uint8_t)(w->col1->nation[nation_id].nation_flags & 0xdfu);
  }

  const int score = europe_compute_immigration_score_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .colonies=(ColonizeColonyPool*)(colonies), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL)}, nation_id);
  int need = score;
  if (need < 0) {
    need = 0;
  }
  if (need > 65535) {
    need = 65535;
  }
  eu->needed_crosses = (uint16_t)need;
  eu->immigration_score = (int16_t)(need > 32767 ? 32767 : need);

  /*
   * 584a *param_2 (viceroy_unpacked.c:68258-68280): +2 a turn, but once the
   * nation's dock latch is up (nation_flags 0x40, set by the first crosses
   * immigrant = crosses_immigrant_seen) every colonist still waiting on the
   * Europe dock flips the tick negative: 2 → -2 → -4 …, i.e. -2 per waiting
   * immigrant. Empty the dock and the +2 resumes — the port used to freeze
   * the meter forever after the first immigrant.
   * Cite: test-saves-ai TURN1–4 = 2/4/6/8 (flag clear, no dock unit),
   * TURN5–7 = 0 (flag 0x40 + one dock colonist, drain clamped at 0).
   */
  int delta = 2;
  if (eu->crosses_immigrant_seen) {
    for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
      if (!eu->dock[i].present) {
        continue;
      }
      /* 584a raw 68272: only units with FUN_281f_0b78(unit) >= 0 (a
       * default-profession slot) drain; a purchased Artillery row does not
       * (bugs.md #674; the AI twin below already applied this test). */
      const int ti = units ? europe_dock_unit_type_index(units, eu->dock[i].dos_type) : -1;
      if (ti >= 0 && !units_type_has_profession_slot(ti)) {
        continue;
      }
      delta = (delta < 1) ? delta - 2 : -2;
    }
  }
  int cur = (int)eu->current_crosses + delta;
  if (cur < 0) {
    cur = 0; /* 5e52 clamps the sum at 0 before the compare. */
  }
  if (cur > 65535) {
    cur = 65535;
  }
  eu->current_crosses = (uint16_t)cur;
  eu->immigration_pressure = (int16_t)(cur > 32767 ? 32767 : (int)cur);
  europe_refresh_recruit_passage(eu);

  /* Phase 5: needed < current → dock immigrant; clear current. */
  if ((int)eu->current_crosses > need) { /* 5e52 raw 68563: plain `local_6 < iVar3` (bugs.md #676) */
    /*
     * 5e52 ~68577: FF 0x14 (Brewster) owned → FUN_38fd_4884(0,1) instead of
     * the random 04d4(0,2) pool pick: the player chooses (@RECRUITCHOOSE),
     * crosses are only zeroed once a pick lands (4884 tail, param_1==0), so
     * a cancelled dialog re-asks next turn. Caller enqueues the CHOICE and
     * europe_brewster_pick_from_pool applies it.
     */
    if ((col1 && founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_BREWSTER)) ||
        eu->brewster_no_criminals) {
      europe_apply_brewster(eu, 1); /* bugs.md #224: purge stale slots BEFORE the pick */
      return 2;
    }
    eu->current_crosses = 0;
    eu->immigration_pressure = 0;
    europe_refresh_recruit_passage(eu);
    if (europe_immigrant_from_pool(eu, rng)) {
      /* 5e52 raw 68588/68601: nation_flags |= 0x40 only when the 0b26 spawn
       * succeeded (bugs.md #675). */
      eu->crosses_immigrant_seen = true;
      /* bugs.md #223: keep open_on_dock — arrivals own it (see above). */
      snprintf(eu->status, sizeof(eu->status), "Immigrant arrives in Europe.");
      return 1;
    }
  }
  return 0;
}


/* ===================== AI-nation immigration (DOS FUN_38fd_5e52, control != 0) (europe_pool_view_from_nation .. europe_nation_immigration_tick_w) ===================== */

/*
 * DOS runs the whole immigration tick per nation from the nation EOT
 * FUN_3844_00f2 (viceroy_unpacked.c:58375, `FUN_291f_0a90(nation)` =
 * FUN_38fd_5e52), for every nation slot — 5e52 gates only its *chrome* on
 * `player[n].control == 0` (68563, 68590, 68603). An AI nation therefore
 * accrues crosses, crosses the same 584a threshold, empties one of its own
 * `recruit[3]` pool bytes into a real unit record parked in Europe
 * (FUN_38fd_0718, spawned at the Europe sentinel tile), refills that slot
 * with FUN_38fd_46d4 and zeroes its crosses — all silently. The port used to
 * stop at the +2 accrual, so no AI immigrant ever existed.
 */

/* View over an AI nation's own pool bytes (nation+2..+4 = recruit[3]). */
static void europe_pool_view_from_nation(
  EuropePoolView* v, const ColonizeCol1Save* col1, int nation_id
) {
  memset(v, 0, sizeof(*v));
  if (!col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  const ColonizeCol1Nation* nat = &col1->nation[nation_id];
  for (int i = 0; i < EUROPE_POOL_SIZE && i < 3; ++i) {
    v->job[i] = (int)nat->recruit[i];
    /* 0x1c = job NONE; col1_bridge reads that byte as "slot empty". */
    v->filled[i] = nat->recruit[i] != UNITS_JOB_NONE;
  }
  /* 46d4 64627-64633: DS:0x53a6 only when the bound nation is the human's;
   * every AI nation substitutes 1. */
  v->difficulty = (col1->player[nation_id].control == 0) ? (int)col1->head.difficulty : 1;
  v->brewster = founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_BREWSTER);
}

int europe_nation_refill_pool_slot(
  ColonizeCol1Save* col1, int nation_id, int slot, bool force_expert, ColonizeDosRng* rng
) {
  if (!col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return -1;
  }
  if (slot < 0 || slot >= 3) {
    return -1;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  EuropePoolView view;
  europe_pool_view_from_nation(&view, col1, nation_id);
  unsigned local = rng ? (rng->state | 1u)
                       : 1u + (unsigned)(nat->gold + nat->recruit_count + slot * 17);
  EuropePoolRng st;
  st.dos = rng;
  st.local = &local;
  const int job = europe_roll_pool_profession(&view, slot, force_expert, &st);
  nat->recruit[slot] = (uint8_t)job;
  return job;
}

/*
 * FUN_38fd_0718 (viceroy_unpacked.c:59098-59147) for an AI nation: the @UNIT
 * type comes from the @JOB byte (europe_dock_unit_dos_type is that same
 * mapping, Dragoon roll included), the record is parked at the port's Europe
 * sentinel tile (the (200,100) limbo `ai_euro_in_europe` tests for, where
 * 5d04's own 0718 spawn already puts purchased units), and the profession
 * byte is stored verbatim — 0718 writes +0x315b = param_1 with no 0x1c swap.
 * Pioneers get the 100 tools of 59134.
 */
int europe_nation_harbor_spawn(const ColonizeWorld* w, int nation_id, int profession) {
  if (!w || !w->units || !w->col1 || nation_id < 0 ||
      nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return -1;
  }
  const bool human = w->col1->player[nation_id].control == 0;
  const int dos_type = europe_dock_unit_dos_type(
    profession, (int)w->col1->head.difficulty, human, w->rng
  );
  const int lt = europe_dock_unit_type_index_ex(w->units, dos_type, true);
  if (lt < 0) {
    return -1;
  }
  if (!units_spawn_room(w->units, nation_id)) {
    return -1;
  }
  const int id = units_spawn_allow_stack(w->units, lt, 200, 100);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(w->units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation_id);
  u->moves = 0;
  u->profession = profession;
  if (dos_type == 2) {
    u->tools = 100; /* 59134: local_4 == 2 -> +0x3159 = 100 */
  } else if (dos_type == 1) {
    u->muskets = 50;
  } else if (dos_type == 4) {
    u->muskets = 50;
    u->horses = 50;
  }
  return id;
}

/*
 * FUN_38fd_584a's *param_2 (viceroy_unpacked.c:68256-68281) for a nation
 * record: +2 a turn, flipped negative once the nation's 0x40 latch is up by
 * every colonist-class unit of that nation still parked in Europe (DOS: unit
 * x == nation - 0x14, i.e. the Europe sentinel; port: the (200,100) limbo).
 */
static int europe_nation_crosses_delta(const ColonizeWorld* w, int nation_id) {
  int delta = 2;
  const ColonizeCol1Save* col1 = w->col1;
  if (!col1 || !w->units) {
    return delta;
  }
  if ((col1->nation[nation_id].nation_flags & 0x40u) == 0u) {
    return delta;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &w->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (u->x < 200 && u->y < 200) {
      continue;
    }
    if (!units_type_has_profession_slot(u->type_index)) {
      continue; /* FUN_281f_0b78(unit) >= 0 */
    }
    delta = (delta < 1) ? delta - 2 : -2;
  }
  return delta;
}

int europe_nation_immigration_tick_w(const ColonizeWorld* w, int nation_id) {
  if (!w || !w->col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  ColonizeCol1Save* col1 = w->col1;
  /* 68551: the whole routine is skipped once independence is declared. */
  if (col1->head.game_options.woi) {
    return 0;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  /* 68554: nation flags &= 0xdf on entry. */
  nat->nation_flags = (uint8_t)(nat->nation_flags & 0xdfu);

  /* 68557-68562: +0x30 = 584a score, +0x2e += the tick, clamped at 0. */
  int need = europe_compute_immigration_score_w(w, nation_id);
  if (need < 0) {
    need = 0;
  }
  if (need > 65535) {
    need = 65535;
  }
  nat->needed_crosses = (uint16_t)need;
  int cur = (int)nat->current_crosses + europe_nation_crosses_delta(w, nation_id);
  if (cur < 0) {
    cur = 0;
  }
  if (cur > 65535) {
    cur = 65535;
  }
  nat->current_crosses = (uint16_t)cur;
  if (need >= cur) {
    return 0; /* 68563: `if (local_6 < iVar3)` */
  }

  /*
   * 68570-68577: Brewster (FF 0x14) owned -> FUN_38fd_4884(0,1) — which for a
   * non-human bound nation takes local_58 = 1 (64744: the list pick is human
   * chrome; everyone else gets slot 1), costs nothing (param_2 != 0 zeroes the
   * passage at 64692), zeroes +0x2e (param_1 == 0, 64763), refills the emptied
   * slot with 46d4(0) — not the season-quad force-expert roll — and does NOT
   * bump recruit_count (+6, gated param_1 == 0 && param_2 == 0). No 0x40 latch
   * on this path either: 68601 is inside the non-Brewster branch only.
   */
  const bool brewster = founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_BREWSTER);
  int slot;
  bool force_expert;
  if (brewster) {
    slot = 1;
    force_expert = false;
  } else {
    /* 68575-68583: crosses zeroed, 04d4(0,2) picks the slot, the slot is
     * refilled with 46d4((turn & 3) == 0) before the unit is created. */
    nat->current_crosses = 0;
    slot = w->rng ? dos_rng_range(w->rng, 0, 2) : 0;
    force_expert = ((int)col1->head.turn & 3) == 0;
  }
  const int profession = (int)nat->recruit[slot];
  if (brewster) {
    /* 4884 tail order (64763-64775): zero +0x2e, create the unit, and only
     * then refill the emptied slot. 5e52's own branch refills first (68581
     * before 68585), so the two paths differ in draw order — DOS-literal. */
    nat->current_crosses = 0;
    const int id = europe_nation_harbor_spawn(w, nation_id, profession);
    if (id < 0) {
      return 0;
    }
    europe_nation_refill_pool_slot(col1, nation_id, slot, false, w->rng);
    return 1;
  }
  europe_nation_refill_pool_slot(col1, nation_id, slot, force_expert, w->rng);
  const int id = europe_nation_harbor_spawn(w, nation_id, profession);
  if (id < 0) {
    return 0;
  }
  nat->nation_flags = (uint8_t)(nat->nation_flags | 0x40u); /* 68601 */
  return 1;
}
