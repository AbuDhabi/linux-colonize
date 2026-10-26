#include "core/internal.h"

/*
 * Tutorial hints: the DOS call sites of GAME.TXT @TUTORIAL1-4 and @TUTORIAL7-19.
 * Every hint but @TUTORIAL18 needs the Tutorial Hints option (DS:0x5382 bit 7,
 * head.game_options.tutorial_hints) and latches one COL1 head tut bit
 * (DS:0x5380 tut1 / 0x5386 tut2 / 0x5387 tut3), so it shows once per game and
 * the latch round-trips through the save. @TUTORIAL5 / @TUTORIAL6 stay with
 * their turn phases (turn_colony.c, turn_production.c), @TUTNOSPACES /
 * @TUTNOLUMBER with Build Colony (game_dialogs.c). Per-tag table:
 * docs/popups_catalog.md.
 *
 * Sections:
 *  - Shared helpers
 *  - Piece-selected chain (FUN_2b5a_0070)
 *  - Land-ho, colony screen and Europe sites
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/ai_goals.h"
#include "core/ai_popup.h"
#include "core/colony.h"
#include "core/colony_preview.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/game_loop.h"
#include "core/game_loop_internal.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/travel_dest.h"
#include "core/units.h"
#include "core/units_move.h"

/* @CARGO row 16 (DS:0x97c0 + 0x20), "Hammers": FUN_2f2b_6cd4 swaps a Carpenter's
 * occupation 0xd for it (raw 56031-56033). */
#define TUT_CARGO_ROW_HAMMERS 0x10

/* ===================== Shared helpers ===================== */

static bool tut_hints_on(const ColonizeGameState* game) {
  return game && game->col1_ok && game->col1.head.game_options.tutorial_hints;
}

/* FUN_281f_0652 / FUN_281f_03fe: an OK popup with the section's MSS figure. */
static void tut_show(ColonizeGameState* game, const char* section, const PopupMsgTokens* tok) {
  popup_chrome_ok(&game->ai_popups, &game->messages, section, tok, "");
}

/* Raised from inside the colony screen: DOS nests the dialog in that screen's
 * loop, so it must not wait behind a colony-zoom hold. */
static void tut_show_in_colony(
  ColonizeGameState* game, const char* section, const PopupMsgTokens* tok
) {
  tut_show(game, section, tok);
  game_colony_present_now(game, AI_POPUP_TAG_INFO);
}

/* FUN_281f_0768 (ocean_or_high_seas): terrain class 0x19 or 0x1a. */
static bool tut_water(const ColonizeWorldMap* map, int x, int y) {
  const int cls = map_dos_terr_class_at(map, x, y);
  return cls == 0x19 || cls == 0x1a;
}

/* Terrain classes 8..0x17: both forested halves. */
static bool tut_forest(int cls) {
  return cls >= 8 && cls <= 0x17;
}

/* Terrain classes 2..5: the open land a pioneer plows. */
static bool tut_open_land(int cls) {
  return cls >= 2 && cls <= 5;
}

/* FUN_281f_0718 = FUN_137f_04b0: the special resource, -1 on a village tile
 * (FUN_137f_0392: settlement bit with a tribe owner nibble >= 4). */
static int tut_resource(const ColonizeWorldMap* map, int x, int y) {
  if (map_tile_has_city(map, x, y) &&
      ((map_get_layer3(map, x, y) >> MAP_L3_OWNER_SHIFT) & MAP_L3_OWNER_MASK) >= 4) {
    return -1;
  }
  return map_resource_type_for_yield(map, x, y);
}

/* DS:0x97c0 + 2*row: NAMES.TXT @CARGO column 0, copied out of the shared
 * reports_names_field scratch so two names can sit in one token set. */
static const char* tut_cargo_name(int row, char* out, size_t out_size) {
  const char* s = reports_names_field("CARGO", row, 0);
  snprintf(out, out_size, "%s", s ? s : "");
  return out;
}

static ColonizeUnitKind tut_kind(const ColonizeGameState* game, const ColonizeUnit* u) {
  return units_type_kind(units_type(&game->units, u->type_index));
}

static bool tut_ship_kind(ColonizeUnitKind kind) {
  return kind >= UNITS_KIND_CARAVEL && kind <= UNITS_KIND_MAN_O_WAR; /* type 0xd..0x12 */
}

/* ===================== Piece-selected chain (FUN_2b5a_0070) ===================== */

/*
 * FUN_2b5a_0070 raw 40238-42015 (overlay twin FUN_OVL02_L0000__000070,
 * viceroy_overlays.c 55341-55593; asm 2b5a:0070-0601 = ndisasm 0x20f50-0x21601).
 */

/* @TUTORIAL3's site test (0x21177-0x2132e): the pioneer's tile is land other than
 * hills/mountains, not next to any colony (FUN_281f_0614 distance DS:0x8db8 == 1),
 * and touches open sea (FUN_281f_0d12 neighbour in water region 1, read back
 * through FUN_281f_06b4). Then more than four of the 3x3 tiles grow food
 * (DS:0x2f7b class byte), a continent under seven land tiles (DS:0x85c8)
 * counting one extra, and one of the nine carries a special resource. Returns
 * its @CARGO row, or -1. The resource switch overwrites in tile order, except
 * that the food (2) and lumber (10) cases only take an empty slot. */
static int tut_first_colony_site(const ColonizeGameState* game, int x, int y) {
  const ColonizeWorldMap* map = &game->world_map;
  const int cls = map_dos_terr_class_at(map, x, y);
  if (cls >= 0x19 && cls <= 0x1c) {
    return -1;
  }
  int dist = 0;
  if (ai_goals_nearest_colony_15eb_0142(map, &game->colonies, x, y, -1, -1, &dist) >= 0 &&
      dist == 1) {
    return -1;
  }
  if (travel_dest_water_region(map, x, y) != 1) {
    return -1;
  }
  int cargo = -1;
  int food_tiles = 0;
  for (int i = 0; i < 9; ++i) { /* DS:0xb4 / 0xbe: the 8 neighbours, then the site */
    const int tx = x + (i < 8 ? MAP_DIR8_DX[i] : 0);
    const int ty = y + (i < 8 ? MAP_DIR8_DY[i] : 0);
    if (colony_yield_terrain_class_base(map_dos_terr_class_at(map, tx, ty), 0) != 0) {
      food_tiles++;
    }
    switch (tut_resource(map, tx, ty)) {
      case 2:
        cargo = cargo < 0 ? 0 : cargo;
        break;
      case 3:
        cargo = 3;
        break;
      case 4:
        cargo = 2;
        break;
      case 5:
        cargo = 1;
        break;
      case 6:
      case 0xd:
        cargo = 6;
        break;
      case 8:
      case 9:
        cargo = 4;
        break;
      case 10:
        cargo = cargo < 0 ? 5 : cargo;
        break;
      case 0xc:
        cargo = 7;
        break;
      default:
        break;
    }
  }
  const int cont = map_continent_id_at(map, x, y);
  if (cont > 0 && cont < 16 && game->col1.post_map.continent_tally_b[cont] < 7) {
    food_tiles++;
  }
  return (cargo >= 0 && food_tiles > 4) ? cargo : -1;
}

/* FUN_281f_0614(x, y, -1, -1) + `DS:0x8db8 == 1` + colony owner == the piece's:
 * the nearest colony of any nation is adjacent and the player's own. */
static const ColonizeColony* tut_adjacent_own_colony(
  const ColonizeGameState* game, int x, int y, int owner
) {
  int dist = 0;
  const int ci = ai_goals_nearest_colony_15eb_0142(
    &game->world_map, &game->colonies, x, y, -1, -1, &dist
  );
  if (ci < 0 || dist != 1 || game->colonies.colonies[ci].nation_id != owner) {
    return NULL;
  }
  return &game->colonies.colonies[ci];
}

/* @TUTORIAL10's terrain test (0x214f1-0x215be): open land, or a forest whose
 * cleared ground is open land, after turn 16, next to a colony with no open
 * land around it and at least two forests. */
static bool tut_plow_site(
  const ColonizeGameState* game, const ColonizeColony* col, int x, int y, int turn
) {
  const ColonizeWorldMap* map = &game->world_map;
  const int cls = map_dos_terr_class_at(map, x, y);
  bool clear = tut_forest(cls);
  if (!tut_open_land(cls & 7) || turn < 16) {
    clear = false;
  }
  int woods = 0;
  for (int i = 0; i < 8; ++i) {
    const int nc =
      map_dos_terr_class_at(map, col->x + MAP_DIR8_DX[i], col->y + MAP_DIR8_DY[i]);
    if (tut_open_land(nc)) {
      clear = false;
    }
    if (tut_forest(nc)) {
      woods++;
    }
  }
  if (woods < 2) {
    clear = false;
  }
  return clear || tut_open_land(cls);
}

/* The ship carries nothing (+0x3150 holds_occupied == 0) and shares its tile with
 * no other unit (tile-chain links +0x315c / +0x315e both -1). */
static bool tut_ship_alone_and_empty(const ColonizeGameState* game, const ColonizeUnit* u) {
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (u->hold_goods_amount[i] > 0) {
      return false;
    }
  }
  return u->cargo_count == 0 && units_count_at(&game->units, u->x, u->y) == 1;
}

/*
 * The chain itself. FUN_2b5a_0e52 (raw 42330-42332) runs it for the piece just
 * made active when the tutorial bit is set; the head (0x20f55-0x20fb2) wants an
 * order-free piece (+0x314c == 0) of the human player (DS:0x5398) on an inset
 * tile. The first hint whose test passes is shown and latched and the chain
 * stops, so at most one hint per activation.
 */
void game_tutorial_piece_selected(ColonizeGameState* game, int unit_id) {
  if (!tut_hints_on(game) || !game->units_ok || !game->world_map_ok) {
    return;
  }
  const ColonizeUnit* u = units_get_const(&game->units, unit_id);
  if (!u || !u->active || u->orders != UNITS_ORDER_NONE || u->nation_id != game->human_nation ||
      u->nation_id < 0 || u->nation_id >= 4 || !map_coords_inset(&game->world_map, u->x, u->y)) {
    return;
  }
  ColonizeCol1Head* head = &game->col1.head;
  const ColonizeWorldMap* map = &game->world_map;
  const int turn = (int)head->turn; /* DS:0x538e */
  const int owner = u->nation_id;
  const int x = u->x;
  const int y = u->y;
  const ColonizeUnitKind kind = tut_kind(game, u);
  const ColonizeUnitType* ut = units_type(&game->units, u->type_index);
  const bool no_colony = colonies_count_for_nation(&game->colonies, owner) == 0; /* DS:0x9298 */
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));

  /* @TUTORIAL1 (0x20fb5): turn 0 on Discoverer; %STRING0 = @UNIT name. */
  if (turn == 0 && head->difficulty == 0 && !head->tut2.nr1) {
    tok.string0 = ut ? ut->name : "";
    tut_show(game, "TUTORIAL1", &tok);
    head->tut2.nr1 = 1;
    return;
  }
  /* @TUTORIAL11 (0x21004): an empty, lone ship before turn 20; %STRING1 = @HOMEPORT. */
  if (!head->tut3.nr11 && turn < 20 && tut_ship_kind(kind) && tut_ship_alone_and_empty(game, u)) {
    tok.string0 = ut ? ut->name : "";
    tok.string1 = reports_home_port_name(owner);
    tut_show(game, "TUTORIAL11", &tok);
    head->tut3.nr11 = 1;
    return;
  }
  /* @TUTORIAL13 (0x21082): pioneers ashore before turn 20, no colony yet. */
  if (!head->tut1.nr13 && turn < 20 && kind == UNITS_KIND_PIONEER && no_colony &&
      !tut_water(map, x, y)) {
    tut_show(game, "TUTORIAL13", &tok);
    head->tut1.nr13 = 1;
    return;
  }
  /* @TUTORIAL14 (0x210cc): soldiers ashore before turn 20. */
  if (!head->tut1.nr14 && turn < 20 && !tut_water(map, x, y) && kind == UNITS_KIND_SOLDIER) {
    tut_show(game, "TUTORIAL14", &tok);
    head->tut1.nr14 = 1;
    return;
  }
  /* @TUTORIAL15 (0x2110c): colonists standing in a colony before turn 40. */
  if (turn < 40 && !head->tut1.nr15 && kind == UNITS_KIND_COLONIST) {
    const ColonizeColony* col = colonies_get(&game->colonies, colonies_id_at(&game->colonies, x, y));
    if (col && col->active) {
      tok.string0 = col->name;
      tut_show(game, "TUTORIAL15", &tok);
      head->tut1.nr15 = 1;
      return;
    }
  }
  /* @TUTORIAL3 (0x21160): pioneers on a good first-colony site; %STRING0 = the cargo. */
  if (!head->tut2.nr3 && no_colony && kind == UNITS_KIND_PIONEER) {
    const int cargo = tut_first_colony_site(game, x, y);
    if (cargo >= 0) {
      char name[32];
      tok.string0 = tut_cargo_name(cargo, name, sizeof(name));
      tut_show(game, "TUTORIAL3", &tok);
      head->tut2.nr3 = 1;
      return;
    }
  }
  /* @TUTORIAL8 (0x21358): a Free Colonist (0x1c) or Indentured Servant (0x19)
   * once any tribe is met (FUN_281f_0a38(owner, tribe + 4) & 0x20); %STRING0 =
   * the @JOB column-0 name, Free Colonist read as row 0x13. */
  if (!head->tut3.nr8 && kind == UNITS_KIND_COLONIST &&
      (u->profession == UNITS_JOB_NONE || u->profession == UNITS_JOB_SERVANT)) {
    bool met = false;
    for (int t = 0; t < 8; ++t) {
      if (game->col1.nation[owner].relation_by_indian[t] & 0x20) {
        met = true;
      }
    }
    if (met) {
      tok.string0 = reports_job_short_name(u->profession == UNITS_JOB_NONE ? 0x13 : u->profession);
      tut_show(game, "TUTORIAL8", &tok);
      head->tut3.nr8 = 1;
      return;
    }
  }
  const ColonizeColony* near =
    kind == UNITS_KIND_PIONEER ? tut_adjacent_own_colony(game, x, y, owner) : NULL;
  /* @TUTORIAL9 (0x213f2): pioneers next to their colony on a roadless,
   * settlement-free (FUN_281f_0754 & 0x0a) forest, hill or mountain tile. */
  if (!head->tut3.nr9 && near && !map_tile_has_road(map, x, y) && !map_tile_has_city(map, x, y)) {
    const int cls = map_dos_terr_class_at(map, x, y);
    if (tut_forest(cls) || cls == 0x1b || cls == 0x1c) {
      tut_show(game, "TUTORIAL9", &tok);
      head->tut3.nr9 = 1;
      return;
    }
  }
  /* @TUTORIAL10 (0x2148c): the same pioneers on an unplowed (FUN_281f_0754 & 0x40)
   * tile worth plowing or clearing. */
  if (!head->tut3.nr10 && near && !map_tile_is_plowed(map, x, y) &&
      tut_plow_site(game, near, x, y, turn)) {
    tut_show(game, "TUTORIAL10", &tok);
    head->tut3.nr10 = 1;
    return;
  }
  /* @TUTORIAL19 (0x215d4): an Indian Convert (@JOB 0x1b). */
  if (!head->tut1.nr19 && kind == UNITS_KIND_COLONIST && u->profession == UNITS_JOB_CONVERT) {
    tut_show(game, "TUTORIAL19", &tok);
    head->tut1.nr19 = 1;
  }
}

/* ===================== Land-ho, colony screen and Europe sites ===================== */

/* @TUTORIAL2: FUN_2b5a_001e raw 40197-40210, the tail of the land-ho handler,
 * once the New World name is stored. No latch of its own: the named_new_world
 * bit (player +0x30 & 0x80) already makes the handler once per game. */
void game_tutorial_landho(ColonizeGameState* game) {
  if (!tut_hints_on(game)) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tut_show(game, "TUTORIAL2", &tok);
}

/*
 * FUN_2f2b_6cd4 raw 56026-56070, the colony-screen bring-up (DS:0xb98 == 0, the
 * player's own open): @TUTORIAL4 then @TUTORIAL12.
 */
void game_tutorial_colony_opened(ColonizeGameState* game, int colony_id) {
  if (!tut_hints_on(game) || !game->in_colony) {
    return;
  }
  const ColonizeColony* col = colonies_get(&game->colonies, colony_id);
  if (!col || !col->active) {
    return;
  }
  ColonizeCol1Head* head = &game->col1.head;
  /*
   * @TUTORIAL4 (DS:0x5386 bit 7): %STRING0 = the @CARGO row of colonist 0's
   * occupation (FUN_281f_0c0e(0)) — the occupation itself indexes DS:0x97c0, so
   * a Carpenter (0xd) is swapped for Hammers and a Fisherman reads row 8.
   * %STRING1 = the last other field cargo (0..7) the colonist's plot
   * (FUN_281f_0d30) yields, or Food. DOS leaves that plot's x/y uninitialised
   * when the colonist has none; the port then keeps Food.
   */
  if (!head->tut2.nr4 && col->colonist_count > 0) {
    int job = colonies_colonist_occupation_job(&game->colonies, &col->colonists[0]);
    int other = 0;
    if (job == COLONIZE_PROF_CARPENTER) {
      job = TUT_CARGO_ROW_HAMMERS;
    } else {
      const int plot = colonies_colonist_tile(col, 0);
      int dx = 0;
      int dy = 0;
      if (plot >= 0 && game->world_map_ok && colonies_field_tile_delta(plot, &dx, &dy)) {
        const int cls = map_dos_terr_class_at(&game->world_map, col->x + dx, col->y + dy);
        for (int g = 0; g < 8; ++g) {
          if (colony_yield_terrain_class_base(cls, g) != 0 && g != job) {
            other = g;
          }
        }
      }
    }
    char now[32];
    char alt[32];
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = tut_cargo_name(job, now, sizeof(now));
    tok.string1 = tut_cargo_name(other, alt, sizeof(alt));
    tut_show_in_colony(game, "TUTORIAL4", &tok);
    head->tut2.nr4 = 1;
  }
  /* @TUTORIAL12 (DS:0x5387 bit 7): a ship (type 0xd..0x12) in the colony's tile
   * chain (FUN_281f_07e0 / 02e4); %STRING0 = the colony. */
  if (!head->tut3.nr12 && game->units_ok) {
    bool ship = false;
    for (int id = units_tile_head_id_at(&game->units, col->x, col->y); id >= 0;
         id = units_tile_chain_next_id(&game->units, id)) {
      const ColonizeUnit* u = units_get_const(&game->units, id);
      if (u && tut_ship_kind(tut_kind(game, u))) {
        ship = true;
      }
    }
    if (ship) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = col->name;
      tut_show_in_colony(game, "TUTORIAL12", &tok);
      head->tut3.nr12 = 1;
    }
  }
}

/*
 * @TUTORIAL7: tail of the assign function FUN_2f2b_2f3e (raw 50389-50394, overlay
 * twin viceroy_overlays.c 59043-59050), reached by every assignment its
 * validator accepts — field, building, jobs list and leave-as. With more than
 * two colonists (colony +0x1f) and no Stockade bit (FUN_281f_09fc(0)) it
 * suggests one; %STRING0 = the colony. DS:0x5387 bit 2.
 */
void game_tutorial_colony_assigned(ColonizeGameState* game) {
  if (!tut_hints_on(game) || game->col1.head.tut3.nr7 || !game->in_colony) {
    return;
  }
  const ColonizeColony* col = colonies_get(&game->colonies, game->colony_view_id);
  if (!col || !col->active || colonies_headcount(col) <= 2 ||
      colonies_dos_owns_row(&game->colonies, col, COLONY_BUILDING_STOCKADE)) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = col->name;
  tut_show_in_colony(game, "TUTORIAL7", &tok);
  game->col1.head.tut3.nr7 = 1;
}

/*
 * @TUTORIAL16: FUN_2f2b_2d90 raw 49341-49376 (asm 2f2b:2d90, `LEA BX,[0xbad]` at
 * ndisasm 0x286f6), the colony screen's full refresh, which runs after every
 * change on the screen. It fires when the food ledger is short: DS:0x8e32
 * (production shortfall) or DS:0x8e5a (unmet after stock), both nonzero exactly
 * when the food gross is below the pop*2 demand (FUN_15eb_1f72 raw 12662-12682,
 * FUN_15eb_0b52). The port polls it while the colony screen is idle. DS:0x5380
 * bit 4.
 */
void game_tutorial_colony_refresh(ColonizeGameState* game) {
  if (!tut_hints_on(game) || game->col1.head.tut1.nr16 || !game->in_colony ||
      ai_popup_busy(&game->ai_popups) || game_modal_open(game)) {
    return;
  }
  const ColonizeColony* col = colonies_get(&game->colonies, game->colony_view_id);
  if (!col || !col->active) {
    return;
  }
  const ColonizeWorld w = game_world(game);
  ColonizeColonyPreview p;
  colony_preview_compute_w(&w, col, &p);
  if (p.food_produced >= p.food_consumed) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tut_show_in_colony(game, "TUTORIAL16", &tok);
  game->col1.head.tut1.nr16 = 1;
}

/*
 * @TUTORIAL17: FUN_38fd_55b6 raw 68158-68164 (`LEA BX,[0x1129]`, asm
 * 38fd:568c-56dc), the European Status bring-up. %STRING0 = @HOMEPORT,
 * %STRING1 = the country phrase (FUN_291f_0ac8 form 0), %STRING2 = the New
 * World name (player +0x18, DS:0x5426 + 0x34*nation). DS:0x5380 bit 5.
 */
void game_tutorial_europe_opened(ColonizeGameState* game) {
  if (!tut_hints_on(game) || game->col1.head.tut1.nr17) {
    return;
  }
  const int nation = game->human_nation;
  if (nation < 0 || nation >= 4) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = reports_home_port_name(nation);
  tok.string1 = reports_nation_country_phrase(nation, 0);
  tok.string2 = game->col1.player[nation].country_name;
  tut_show(game, "TUTORIAL17", &tok);
  game->col1.head.tut1.nr17 = 1;
}

/*
 * @TUTORIAL18: FUN_38fd_1fa2 raw 60435-60455 (`LEA BX,[0xfdb]`, asm
 * 38fd:2130-2219), the market-to-ship buy. The full load (the hold room,
 * at most 100) is priced first; when the treasury is short the buy is refused
 * outright, the status line shows the cargo and its cost, and a plain drag
 * (param_4 == 0) adds this hint. No tutorial-option gate and no latch: DOS
 * shows it on every such drag. %NUMBER0 = price per unit, %NUMBER1 = gold.
 * Returns true when the buy was refused. The Shift (amount prompt) arm is
 * port_bugs.md #1018 (docs/known_divergences.md).
 */
bool game_tutorial_europe_cannot_afford(ColonizeGameState* game, int hidx, int cargo) {
  EuropeScreen* eu = &game->europe;
  if (!game->europe_ok || cargo < 0 || cargo >= eu->cargo_count) {
    return false;
  }
  const int room = europe_harbor_cargo_room(eu, game->units_ok ? &game->units : NULL, hidx, cargo);
  if (room <= 0) {
    return false; /* the buy's own no-room refusal comes first (FUN_281f_0b96) */
  }
  const int price = europe_buy_price(eu, cargo);
  const int cost = price * (room < 100 ? room : 100);
  const long gold =
    (long)europe_nation_gold(eu, game->col1_ok ? &game->col1 : NULL, game->human_nation);
  if (gold >= cost) {
    return false;
  }
  char name[32];
  tut_cargo_name(cargo, name, sizeof(name));
  snprintf(eu->status, sizeof(eu->status), "%s (%d)", name, cost);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = name;
  tok.number0 = price;
  tok.has_number0 = true;
  tok.number1 = (int)gold;
  tok.has_number1 = true;
  tut_show(game, "TUTORIAL18", &tok);
  return true;
}
