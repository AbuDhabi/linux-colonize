#include "core/internal.h"
#include "core/game_loop.h"

/*
 * Split out of game_loop.c (2026-09-23) — code moved verbatim.
 *
 * Sections:
 *  - End-of-turn flow, trade-route servicing, map menu actions
 *
 * Cross-file seams are declared in core/game_loop_internal.h.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif

#include "core/assets.h"
#include "core/ai.h"
#include "core/ai_euro.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_goals.h"
#include "core/ai_king.h"
#include "core/ai_popup.h"
#include "core/cheat_list_dialog.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/colony_screen.h"
#include "core/colony_yield.h"
#include "core/combat_strength.h"
#include "core/debug_atlas.h"
#include "core/closing.h"
#include "core/opening.h"
#include "core/declaration.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/fb.h"
#include "core/font_file.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/howmuch_dialog.h"
#include "core/map.h"
#include "core/map_gen.h"
#include "core/map_menu.h"
#include "core/pioneer_plan.h"
#include "core/map_panel.h"
#include "core/name_entry_dialog.h"
#include "core/new_game.h"
#include "core/options_dialog.h"
#include "core/pedia.h"
#include "core/pik.h"
#include "core/pick_music.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/save_load_dialog.h"
#include "core/savegame.h"
#include "core/settings.h"
#include "core/sound.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/trade_screen.h"
#include "core/travel_dest.h"
#include "core/turn.h"
#include "core/ui_button.h"
#include "core/ui_colors.h"
#include "core/ui_drag.h"
#include "core/unit_chrome.h"
#include "core/unit_stack.h"
#include "core/units.h"
#include "core/woodcut.h"
#include "core/combat_analysis.h"
#include "core/version.h"
#include "platform/diagnostics.h"

#include "core/game_dialogs.h"
#include "core/game_loop_internal.h"

/* ===================== End-of-turn flow, trade-route servicing, map menu actions (game_apply_turn_autosave .. game_map_click_dispatch) ===================== */


static void game_apply_turn_autosave(ColonizeGameState* game, const ColonizeTurnResult* result) {
  if (!game || !result) {
    return;
  }
  char err[256];
  if (result->request_autosave_decade) {
    if (!game_save_col1_autosave_slot(game, 8, err, sizeof(err))) {
      diag_warn("Decade autosave failed: %s", err);
    } else {
      diag_info("Decade autosave → COLONY08.SAV");
    }
  }
  if (result->request_autosave_turn) {
    if (!game_save_col1_autosave_slot(game, 9, err, sizeof(err))) {
      diag_warn("Turn autosave failed: %s", err);
    } else {
      diag_info("Turn autosave → COLONY09.SAV");
    }
  }
  if (result->request_save_victory) {
    if (!game_save_col1_autosave_slot(game, 10, err, sizeof(err))) {
      diag_warn("Victory save failed: %s", err);
    } else {
      diag_info("Victory save → COLONY10.SAV");
    }
  }
  /* Port-only debug.trace_autosaves: one extra save per turn, kept forever,
   * so a reported turn can be replayed later. Independent of the DOS autosave
   * option bit. */
  if (settings_is_loaded() && settings_get()->trace_autosaves) {
    char name[64];
    snprintf(name, sizeof(name), "trace_autosave_turn_%03u.sav", game->turn_number);
    if (!game_save_col1_named(game, name, err, sizeof(err))) {
      diag_warn("Trace autosave failed: %s", err);
    } else {
      diag_info("Trace autosave → %s", name);
    }
  }
}

void game_fill_turn_context(ColonizeGameState* game, ColonizeTurnContext* ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->turn_number = &game->turn_number;
  ctx->game_year = &game->game_year;
  ctx->game_autumn = &game->game_autumn;
  /* No power at the controls (every Euro control byte 1): the sim sees no human,
   * and DS:0x5398 stays reachable through col1_fixed_human. */
  ctx->human_nation =
    (game->col1_ok && col1_all_ai_fixed_nation(&game->col1) >= 0) ? -1 : game->human_nation;
  ctx->active_turn_nation = &game->active_turn_nation;
  ctx->units = game->units_ok ? &game->units : NULL;
  ctx->colonies = &game->colonies;
  /* Always the live screen: europe_load memsets the struct before it can
   * fail, so a !europe_ok game still hands the sim a zeroed-but-valid
   * EuropeScreen rather than NULL (the ternary here used to pretend to
   * gate on europe_ok and returned the same pointer on both arms). */
  ctx->europe = &game->europe;
  ctx->map = game->world_map_ok ? &game->world_map : NULL;
  ctx->col1 = game->col1_ok ? &game->col1 : NULL;
  ctx->col1_ok = game->col1_ok;
  ctx->rng = &game->move_rng;
  ctx->rng_seed = game->ai_rng_seed;
  ctx->rng_seed_set = true;
  ctx->status = game->status;
  ctx->status_size = sizeof(game->status);
  ctx->ai_popups = &game->ai_popups;
  ctx->messages = &game->messages;
  ctx->names = game->names_ok ? &game->names : NULL;
  ctx->labels = game->labels_ok ? &game->labels : NULL;
}

/* Purchased-ship cargo tags (see europe_board_sentry_dockers): 0 = Colonists,
 * -2 = Artillery. Real passenger type indices (map→Europe round trips) pass through. */
static int game_europe_resolve_pax_type(const ColonizeUnitPool* units, int tag) {
  if (tag == -2) {
    const int t = units_kind_type_index(units, UNITS_KIND_ARTILLERY);
    return t >= 0 ? t : 0;
  }
  if (tag < 0 || tag >= units->type_count) {
    const int t = units_kind_type_index(units, UNITS_KIND_COLONIST);
    return t >= 0 ? t : 0;
  }
  return tag;
}

static void game_europe_capture_pax_professions(
  const ColonizeUnitPool* units,
  int ship_id,
  int* out_profs,
  int max
) {
  if (!out_profs || max <= 0) {
    return;
  }
  for (int i = 0; i < max; ++i) {
    out_profs[i] = -1;
  }
  const ColonizeUnit* ship = units_get_const(units, ship_id);
  if (!ship) {
    return;
  }
  for (int i = 0; i < ship->cargo_count && i < max; ++i) {
    const ColonizeUnit* pax = units_get_const(units, ship->cargo_ids[i]);
    out_profs[i] = pax ? pax->profession : -1;
  }
}

/*
 * COL1 Treasure gold: delegated to units_treasure_value_gold (DOS +0x315b =
 * COL1 `profession` byte * 100 — the only representation).
 * Cite: Colonization.pdf Treasure Trains; europe.h cargo_treasure_gold;
 * GAME.TXT @LOOTCASH. Non-Treasure passengers keep 0 (goods holds are not gold).
 *
 * Manual verify (smoke_game_flow stays title-only; no CMake smoke hook here):
 * board Treasure carrying its value in the DOS +0x315b byte, H / Return to Europe on
 * high seas → Expected.cargo_treasure_gold set → tick to Harbor → cash-in.
 * unit_europe covers cash when cargo_treasure_gold is already set.
 */
static int game_treasure_gold_from_unit(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  if (!units_type_is_treasure(ut)) {
    return 0;
  }
  /* Save-loaded Treasures carry only the COL1 profession byte (gold/100);
   * the shared helper reads the mirror first, then that byte. */
  return units_treasure_value_gold(u);
}

static void game_europe_capture_pax_treasure_gold(
  const ColonizeUnitPool* units,
  int ship_id,
  int* out_gold,
  int max
) {
  if (!out_gold || max <= 0) {
    return;
  }
  for (int i = 0; i < max; ++i) {
    out_gold[i] = 0;
  }
  const ColonizeUnit* ship = units_get_const(units, ship_id);
  if (!ship) {
    return;
  }
  for (int i = 0; i < ship->cargo_count && i < max; ++i) {
    const ColonizeUnit* pax = units_get_const(units, ship->cargo_ids[i]);
    out_gold[i] = game_treasure_gold_from_unit(units, pax);
  }
}

/* After europe_enqueue_expected: fill PARKED cargo_treasure_gold on newest slot. */
static void game_europe_fill_expected_treasure_gold(
  EuropeScreen* eu,
  const int* treasure_gold,
  int cargo_count
) {
  if (!eu || !treasure_gold || eu->expected_ships <= 0 || cargo_count <= 0) {
    return;
  }
  EuropeHarborShip* ship = &eu->expected[eu->expected_ships - 1];
  const int n = cargo_count > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : cargo_count;
  for (int i = 0; i < n; ++i) {
    ship->cargo_treasure_gold[i] = treasure_gold[i];
  }
}

/* Lane-full restore: put the value byte back onto respawned Treasure passengers. */
static void game_europe_restore_pax_treasure_gold(
  ColonizeUnitPool* units,
  int ship_id,
  const int* treasure_gold,
  int cargo_count
) {
  if (!units || !treasure_gold || cargo_count <= 0) {
    return;
  }
  ColonizeUnit* ship = units_get(units, ship_id);
  if (!ship) {
    return;
  }
  for (int i = 0; i < ship->cargo_count && i < cargo_count; ++i) {
    if (treasure_gold[i] <= 0) {
      continue;
    }
    ColonizeUnit* pax = units_get(units, ship->cargo_ids[i]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ut = units_type(units, pax->type_index);
    if (!units_type_is_treasure(ut)) {
      continue;
    }
    /* DOS +0x315b = gold/100 (see units_treasure_value_gold); the old LE16
     * hold_goods_amount mirror is gone. */
    pax->profession = treasure_gold[i] / 100 > 255 ? 255 : treasure_gold[i] / 100;
  }
}

/*
 * DOS-LITERAL FUN_479b high-seas arm raw 76477-76484 (duplicate raw
 * 77095-77098) — the WoI Europe gate is NOT total:
 *
 *   ((*(byte *)0x5382 & 1) == 0 ||
 *    (*(int *)0x5394 == *(int *)0x53d2 && unit[+0x3146] == '\x12'))
 *
 * i.e. the declaration shuts Europe to everyone EXCEPT a Man-O-War moving on
 * the crown slot's own turn (DS:0x53d2 = head.crown_nation_id). port_bugs.md #870:
 * the port blocked every hull. Identify the hull by @UNIT ROW
 * (units_kind_type_index), never by name.
 */
bool game_ship_woi_europe_exempt(const ColonizeGameState* game, const ColonizeUnit* ship) {
  if (!game || !ship || !game->col1_ok) {
    return false;
  }
  const ColonizeCol1Save* col1 = &game->col1;
  const int crown = ai_king_crown_nation_col1(col1, (int)col1->head.human_player);
  if (ship->nation_id != crown || (int)col1->head.nation_turn != crown) {
    return false;
  }
  const int row = units_kind_type_index(&game->units, UNITS_KIND_MAN_O_WAR);
  return row >= 0 && ship->type_index == row;
}

/* @EUROPENOTLEAVE (FUN_281f_03fe): the War of Independence refusal. */
static void game_europe_not_leave_popup(ColonizeGameState* game) {
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, "EUROPENOTLEAVE", NULL, "", body, sizeof(body));
  ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

/*
 * Sail a ship on a sea-lane (high seas) tile back to Europe with everything
 * aboard — the shared tail for the H command and for a Go To order whose
 * destination is a sea-lane tile (port_bugs.md: "a go-to order should be possible
 * for a ship into the high seas / sea lane tile ... the ship should
 * automatically return to Europe on landing on that special tile").
 * Caller guarantees a live sea unit on a high-seas tile.
 *
 * Returns true only when the ship actually departed (the unit is gone from
 * the map and an Expected slot exists); false when the crossing was refused
 * or the lane was full and the ship was put back.
 */
bool game_ship_sail_to_europe(ColonizeGameState* game, int sid) {
  ColonizeUnit* ship = units_get(&game->units, sid);
  if (!ship || !units_is_sea(&game->units, sid)) {
    return false;
  }
  /*
   * War of Independence (DS:0x5382 bit0): no ship may leave for Europe.
   * Both DOS sail intents test the bit and refuse with @EUROPENOTLEAVE —
   * the reason-5 lane step (FUN_4720_049e case 4, viceroy_ndisasm.asm
   * 0x3FEA6: `test byte [0x5382],1` → `lea bx,[0x13fd]`, then the @SAILHOME
   * answer is forced to 2/"No") and the Go To menu's Europe destination 999
   * (FUN_2b5a_1dfc, viceroy_unpacked.c 42816-42819 / viceroy_unpacked.asm
   * 2b5a:1e1c: same test → `lea bx,[0xa1e]`, return without setting the
   * order). Both DS addresses spell EUROPENOTLEAVE (EXE offset 121248+addr).
   * The gate lives on the shared tail because the port has callers DOS has
   * no analogue for (the H command, an arbitrary Go To onto a lane tile, a
   * trade-route Europe stop): without it the ship despawns into the lane,
   * lands in a harbor that game_finish_end_turn refuses to open under the
   * same flag, and is lost for the rest of the game.
   */
  if (game_europe_blocked_by_woi(game) && !game_ship_woi_europe_exempt(game, ship)) {
    game_europe_not_leave_popup(game);
    return false;
  }
  const int exit_x = ship->x;
  const int exit_y = ship->y;
  const bool exit_east = exit_x >= (int)game->world_map.width / 2;
  /* DS:0x9418 is a census tally taken while the ship is still a unit — take
   * the count BEFORE the despawn below, or every crossing is scored one hull
   * short (game_voyage_ship_count). */
  const int voyage_ship_count = game_voyage_ship_count(game);
  int type_index = -1;
  char ship_name[32];
  int cargo_types[EUROPE_SHIP_CARGO_MAX];
  int cargo_profs[EUROPE_SHIP_CARGO_MAX];
  int cargo_count = 0;
  int hold_types[EUROPE_SHIP_CARGO_MAX];
  int hold_amts[EUROPE_SHIP_CARGO_MAX];
  int cargo_treasure_gold[EUROPE_SHIP_CARGO_MAX];
  int cargo_tools[EUROPE_SHIP_CARGO_MAX];
  bool sailed = false;
  memset(hold_types, 0, sizeof(hold_types));
  memset(hold_amts, 0, sizeof(hold_amts));
  memset(cargo_treasure_gold, 0, sizeof(cargo_treasure_gold));
  memset(cargo_tools, 0, sizeof(cargo_tools));
  /* A Pioneer's tools cross with him (DOS keeps the whole record on the lane). */
  for (int i = 0; i < ship->cargo_count && i < EUROPE_SHIP_CARGO_MAX; ++i) {
    const ColonizeUnit* pax = units_get_const(&game->units, ship->cargo_ids[i]);
    cargo_tools[i] = pax ? pax->tools : 0;
  }
  game_europe_capture_pax_professions(
    &game->units, sid, cargo_profs, EUROPE_SHIP_CARGO_MAX
  );
  game_europe_capture_pax_treasure_gold(
    &game->units, sid, cargo_treasure_gold, EUROPE_SHIP_CARGO_MAX
  );
  if (!units_despawn_ship_with_cargo(
        &game->units,
        sid,
        &type_index,
        ship_name,
        sizeof(ship_name),
        cargo_types,
        &cargo_count,
        EUROPE_SHIP_CARGO_MAX,
        hold_types,
        hold_amts,
        EUROPE_SHIP_CARGO_MAX
      )) {
    set_status(game, "Failed to sail ship", NULL);
  } else {
    const int voyage_turns = game_voyage_turns_for(game, voyage_ship_count, exit_x);
    if (!europe_enqueue_expected(
          &game->europe,
          type_index,
          ship_name,
          cargo_types,
          cargo_profs,
          cargo_count,
          hold_types,
          hold_amts,
          exit_x,
          exit_y,
          exit_east,
          voyage_turns
        )) {
      /*
       * Last-resort restore (EUROPE_HARBOR_MAX exceeds the DOS unit cap, so effectively
       * unreachable): leaves a ship on the map rather than deleting it. It
       * cannot preserve the unit's id/orders/route state.
       */
      const int restored = units_transfer_ship_from_europe(
        &game->units,
        type_index,
        exit_x,
        exit_y,
        cargo_types,
        cargo_count,
        hold_types,
        hold_amts
      );
      if (restored >= 0) {
        /* units_spawn_ship_with_cargo defaults nation_id 0 — a non-English
         * player's restored ship (and passengers) must stay theirs, same
         * hazard as the arrival path (port_bugs.md merchantman_sails_back.SAV). */
        ColonizeUnit* rs = units_get(&game->units, restored);
        if (rs) {
          units_set_nation(rs, game->human_nation);
          for (int ci = 0; ci < rs->cargo_count; ++ci) {
            ColonizeUnit* pax = units_get(&game->units, rs->cargo_ids[ci]);
            if (pax) {
              units_set_nation(pax, game->human_nation);
            }
          }
        }
        game_europe_restore_pax_treasure_gold(
          &game->units, restored, cargo_treasure_gold, cargo_count
        );
        for (int ci = 0; rs && ci < rs->cargo_count && ci < cargo_count; ++ci) {
          ColonizeUnit* pax = units_get(&game->units, rs->cargo_ids[ci]);
          if (pax && cargo_tools[ci] > 0) {
            pax->tools = cargo_tools[ci];
          }
        }
        game->units.selected_id = restored;
      }
    } else {
      sailed = true;
      game_europe_fill_expected_treasure_gold(
        &game->europe, cargo_treasure_gold, cargo_count
      );
      memcpy(
        game->europe.expected[game->europe.expected_ships - 1].cargo_tools, cargo_tools,
        sizeof(cargo_tools)
      );
      if (cargo_count > 0) {
        snprintf(
          game->status,
          sizeof(game->status),
          "%s sailed to Europe (+%d aboard)",
          ship_name,
          cargo_count
        );
      } else {
        snprintf(game->status, sizeof(game->status), "%s sailed to Europe", ship_name);
      }
      diag_info(
        "Sailed %s to Europe (exit %d,%d cargo=%d)", ship_name, exit_x, exit_y, cargo_count
      );
    }
  }
  return sailed;
}


/*
 * Spawn every Bound-for-New-World ship whose voyage has finished. Called after
 * turn end, when leaving the Europe screen, and at Europe-input time so ships
 * that just ticked to 0 appear promptly.
 */
void game_europe_deliver_bound_ships(ColonizeGameState* game) {
  if (!game || !game->europe_ok || !game->units_ok || !game->world_map_ok) {
    return;
  }
  EuropeScreen* eu = &game->europe;
  for (;;) {
    int idx = -1;
    for (int i = 0; i < eu->bound_ships; ++i) {
      if (eu->bound[i].turns_left <= 0) {
        idx = i;
        break;
      }
    }
    if (idx < 0) {
      break;
    }

    /*
     * port_bugs.md (missing_merchantman.SAV): a returning ship must NEVER be lost.
     * The old code popped the ship off the bound lane FIRST and then did
     * `continue` on any spawn failure (unresolved type, no free high-seas
     * tile, spawn refused) — deleting the ship and every passenger aboard.
     * Resolve everything against the still-in-lane slot, and only pop once a
     * spawn has actually succeeded; a genuine failure leaves the ship parked
     * in the lane (turns_left 0) to retry, never vanished.
     */
    const EuropeHarborShip* slot = &eu->bound[idx];
    int cargo_professions[EUROPE_SHIP_CARGO_MAX];
    memcpy(cargo_professions, slot->cargo_professions, sizeof(cargo_professions));
    int cargo_tools[EUROPE_SHIP_CARGO_MAX];
    memcpy(cargo_tools, slot->cargo_tools, sizeof(cargo_tools));

    char name[32];
    snprintf(name, sizeof(name), "%s", slot->name);
    int type_index = slot->type_index;
    if (type_index < 0) {
      type_index = units_find_type(&game->units, name); /* purchased ship, never resolved */
    }
    if (type_index < 0) {
      diag_warn("Europe arrival: could not resolve ship type for '%s' — parked in lane", name);
      break;
    }

    int sx = slot->exit_x;
    int sy = slot->exit_y;
    if (sx <= 0 && sy <= 0 && eu->last_exit_valid) {
      sx = eu->last_exit_x;
      sy = eu->last_exit_y;
    }
    if (sx <= 0 && sy <= 0) {
      sx = (int)game->world_map.width / 2;
      sy = (int)game->world_map.height / 2;
    }
    int fx = sx;
    int fy = sy;
    /*
     * DOS FUN_48d3_048e (:77810): the arrival tile is picked by an expanding
     * ring hunt around the nation's Europe landfall goal (unit +0x314d/+0x314e,
     * copied from the nation Europe block +0x32/+0x33 — the port's
     * exit_x/exit_y / last_exit_*), accepting the first tile that passes
     * FUN_48d3_0434: terrain 0x1a (high seas) AND empty-or-own-nation.
     * units_spiral_place_hs_near is that ring walk; units_find_high_seas_tile
     * is a plain nearest-euclidean sweep that also refuses a tile holding one
     * of our OWN ships, so it could throw an arrival across the map. Keep it
     * only as the never-lose-a-ship fallback.
     */
    if (!units_spiral_place_hs_near(
          &game->units, &game->world_map, sx, sy, game->human_nation, &fx, &fy
        ) &&
        !units_find_high_seas_tile(&game->units, &game->world_map, sx, sy, &fx, &fy)) {
      diag_warn("Europe arrival: no free high-seas tile for '%s' — parked in lane", name);
      break;
    }

    int cargo_count = slot->cargo_count > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX
                                                               : slot->cargo_count;
    if (game->units.unit_count + 1 + cargo_count > COLONIZE_UNITS_MAX) {
      snprintf(
        game->status, sizeof(game->status),
        "Unit limit reached: free %d unit slots for %s to arrive",
        game->units.unit_count + 1 + cargo_count - COLONIZE_UNITS_MAX, name
      );
      diag_warn("Europe arrival: %s — parked in lane", game->status);
      break;
    }
    int resolved_cargo[EUROPE_SHIP_CARGO_MAX];
    for (int i = 0; i < cargo_count; ++i) {
      resolved_cargo[i] = game_europe_resolve_pax_type(&game->units, slot->cargo_types[i]);
    }
    int hold_types[EUROPE_SHIP_CARGO_MAX];
    int hold_amts[EUROPE_SHIP_CARGO_MAX];
    for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
      hold_types[i] = slot->hold_goods_type[i];
      hold_amts[i] = slot->hold_goods_amount[i];
    }

    const int ship_id = units_transfer_ship_from_europe(
      &game->units, type_index, fx, fy, resolved_cargo, cargo_count, hold_types, hold_amts
    );
    if (ship_id < 0) {
      diag_warn("Europe arrival: failed to spawn '%s' at (%d,%d) — parked in lane", name, fx, fy);
      break;
    }
    /* Trade-route return leg: re-arm the route before the slot is popped. */
    const int trade_route = slot->trade_route_plus1 - 1;
    const int trade_stop = slot->trade_stop;
    /* Spawn confirmed — now it is safe to remove the ship from the lane. */
    for (int j = idx + 1; j < eu->bound_ships; ++j) {
      eu->bound[j - 1] = eu->bound[j];
    }
    eu->bound_ships--;
    memset(&eu->bound[eu->bound_ships], 0, sizeof(eu->bound[eu->bound_ships]));
    ColonizeUnit* ship = units_get(&game->units, ship_id);
    if (ship) {
      /*
       * port_bugs.md (merchantman_sails_back.SAV): units_spawn_ship_with_cargo
       * defaults nation_id to 0. A returning ship (and its passengers) must
       * belong to the HUMAN nation — for a nation-0 player that was a no-op,
       * but a Dutch (nation 3) player's ship arrived owned by England, so it
       * dropped out of the player's control and the English AI sailed it off
       * ("vanished after two turns"). Stamp the human owner explicitly.
       */
      units_set_nation(ship, game->human_nation);
      for (int i = 0; i < ship->cargo_count && i < cargo_count; ++i) {
        ColonizeUnit* pax = units_get(&game->units, ship->cargo_ids[i]);
        if (pax) {
          units_set_nation(pax, game->human_nation);
        }
        if (pax && cargo_professions[i] >= 0) {
          pax->profession = cargo_professions[i];
          /*
           * The kit travels with the passenger: a Hardy Pioneer steps ashore
           * with its 100 Tools, a colonist armed on the dock with its muskets
           * (port_bugs.md). Read it off the type the passenger actually boarded as,
           * which is what the @ARMOPTIONS rows moved it to, not off the
           * profession — those disagree once a plain colonist has been armed.
           */
          const ColonizeUnitType* put = units_type(&game->units, pax->type_index);
          europe_apply_dock_unit_kit(
            pax, europe_dock_type_for(put ? put->name : NULL, cargo_professions[i])
          );
        }
        /* ...but a Pioneer who crossed with fewer tools keeps what he had. */
        if (pax && cargo_tools[i] > 0) {
          pax->tools = cargo_tools[i];
        }
      }
      /*
       * Trade-route continuation: the ship that crossed to service its
       * Europe stop resumes the route toward trade_stop (port_bugs.md: the ship
       * must actually visit Europe, then carry on) — same TRADE_ROUTE
       * encoding as game_apply_trade_dest (follow_unit_id = route slot).
       */
      if (trade_route >= 0 && trade_route < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT &&
          game->col1_ok && game->col1.trade_route[trade_route].dest_count > 0) {
        ship->orders = UNITS_ORDER_TRADE_ROUTE;
        ship->follow_unit_id = trade_route;
        (void)game_trade_route_aim_stop(game, ship, trade_stop);
      }
      /*
       * port_bugs.md #421: a ship stepping off the Europe lane onto the map sees
       * around itself the moment it lands — it does NOT need to be moved
       * first. DOS FUN_48d3_048e ends with
       *   FUN_281f_0948 (set x/y) -> FUN_281f_084e (post-move chrome)
       *   -> FUN_281f_07a0  == FUN_13f1_02f8  (reveal exploration bits)
       * (viceroy_unpacked.c:77887-77890; FUN_281f_07a0 -> FUN_13f1_02f8 per
       * FUNCTION_CATALOG.md:1374, and FUN_13f1_02f8 -> FUN_13f1_02b4 ->
       * FUN_13f1_0158 is the same sight walk every move runs). The reveal is
       * unconditional — the human-only tail after it is just the viewport
       * recentre (FUN_281f_0352 / FUN_281f_09ba). Newly bought ships were
       * therefore landing inside their own fog.
       */
      game_reveal_sight_for_unit(game, ship);
    }
    snprintf(
      game->status, sizeof(game->status), "%s arrived from Europe at (%d,%d)", name, fx, fy
    );
    diag_info("Europe arrival: spawned '%s' id=%d at (%d,%d)", name, ship_id, fx, fy);
  }
}

void game_finish_end_turn(ColonizeGameState* game, const ColonizeTurnResult* result) {
  units_set_move_watch(NULL, NULL);
  units_set_combat_watch(NULL, NULL);
  units_set_combat_dissolve(NULL, NULL);
  units_set_combat_popup_pump(NULL, NULL);
  game->turn_flow_deferred = false;
  game_europe_service_trade_harbor(game);
  game_europe_deliver_bound_ships(game);
  /*
   * Autosave AFTER the Europe lane has been emptied, not before. DOS main
   * loop (viceroy_unpacked.c:6393-6420): the per-nation EOT FUN_3844_00f2
   * (via FUN_281f_0644, :6394) runs first, and it ends in FUN_291f_0a82
   * (:58377) → FUN_48d3_06ba, whose head ticks all four Europe sentinel
   * lanes (thunk_FUN_2a1f_0246 → FUN_48d3_03d0, :77965-77980) and whose tail
   * lands every arrived ship on the map (thunk_FUN_2a1f_0238 →
   * FUN_48d3_064e → FUN_48d3_048e). Only then does FUN_130d_0172 fire
   * (:6404 for the AI-run branch, :6416 for the human), picking slot 8 or 9.
   * Saving ahead of the arrivals would park every finished voyage in the
   * lane again, so reloading COLONY09.SAV delayed each ship a turn.
   */
  game_apply_turn_autosave(game, result);
  /* FUN_2b5a_3b68 (raw 46831-46833), the human's Move Pieces entry, after
   * the autosave: 0x5392 = 0xffff (it may still name an AI hull that sailed
   * for Europe, raw 76484), then the first unit needing orders. A pick
   * forces Move Pieces (FUN_2b5a raw 42308-42317); with none the View mode
   * the player left stays, as in every DOS idle autosave. Go-To resumes at
   * 10 steps/sec in game_update so the player can watch. */
  game_move_pieces_entry(game);
  if (result && result->request_europe_open && game->europe_ok) {
    game->europe.open_on_dock = true;
  }
  /*
   * port_bugs.md: don't auto-open the European Status while end-of-turn popups
   * (king tax, raids, …) are still queued — DOS answers every audience
   * before the harbor screen appears. The open_on_dock flag stays set;
   * game_update opens Europe once the popup queue has drained.
   */
  if (game->europe_ok && game->europe.open_on_dock && !game_europe_blocked_by_woi(game) &&
      !ai_popup_busy(&game->ai_popups)) {
    game->in_europe = true;
    game->europe.open_on_dock = false;
    sound_set_bgm(3); /* FUN_75c2_2778: Europe pool */
    game_tutorial_europe_opened(game);
  }
  if (result && result->year_end_defeat) {
    snprintf(game->status, sizeof(game->status), "Defeat: no colonies remain.");
  } else if (result && result->year_end_victory) {
    snprintf(game->status, sizeof(game->status), "Victory: independence won.");
  }
  const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
  if (sel && sel->active) {
    game->map_cursor_x = sel->x;
    game->map_cursor_y = sel->y;
    game_set_view_center(game, sel->x, sel->y);
  }
  game_autoplay_after_turn(game);
}

/*
 * port_bugs.md: "The only place that time/turn processing is allowed to proceed is
 * the overland map, and only if there ISN'T an active popup."
 *
 * DOS gets this for free — its popups are blocking calls, so the code that
 * hands control to the next unit or ends the turn simply does not run until
 * the player has answered. Our dialogs are asynchronous, so every automatic
 * hand-off has to ask first. A queued-but-not-yet-presented AI popup counts
 * as open (ai_popup_busy): it is shown at the top of the very next
 * game_update, and the action that queued it must not race ahead of it.
 */
bool game_turn_flow_allowed(const ColonizeGameState* game) {
  if (!game) {
    return false;
  }
  /* Mid-EOT a popup answer must never hand control to a human unit — the
   * pipeline is frozen behind the dialog and FINISH does its own hand-off. */
  if (turn_processor_active(&game->turn_proc)) {
    return false;
  }
  if (game_screen_owns_display(game)) {
    return false;
  }
  if (new_game_active(&game->new_game)) {
    return false;
  }
  return !game_modal_open(game) && !ai_popup_busy(&game->ai_popups);
}

/*
 * Park an automatic unit hand-off / end-of-turn until the map is back on
 * screen with nothing over it; game_update replays it. Returns true when the
 * caller must stop (deferred).
 */
bool game_defer_turn_flow(ColonizeGameState* game) {
  if (game_turn_flow_allowed(game)) {
    return false;
  }
  if (game) {
    game->turn_flow_deferred = true;
  }
  return true;
}

void game_do_end_turn(ColonizeGameState* game) {
  if (!game || turn_processor_active(&game->turn_proc)) {
    return;
  }
  if (game_defer_turn_flow(game)) {
    return;
  }
  units_set_move_watch(game_move_watch, game);
  units_set_combat_watch(game_combat_watch, game);
  units_set_combat_dissolve(game_combat_dissolve, game);
  units_set_combat_popup_pump(game_combat_popup_pump, game);
  turn_processor_start(&game->turn_proc);
  /* Run setup immediately so calendar advances on the same input that ends the turn. */
  ColonizeTurnContext ctx;
  game_fill_turn_context(game, &ctx);
  if (!turn_processor_advance(&game->turn_proc, &ctx)) {
    game_finish_end_turn(game, &game->turn_proc.result);
  }
}

/*
 * Non-mutating "does any human unit still need orders" check — same intent
 * as turn_select_next_unit's own scan (moves>0, on-map, human-owned)
 * plus the guard loop's standing-order skip (Fortified/Sentry/etc. —
 * units_orders_skip_turn), but without turn_select_next_unit's side effect
 * of actually changing pool->selected_id, and without turn_human_units_
 * exhausted's mismatch: that one only checks moves>0, so a colony full
 * of Fortified units (moves>0, never actually offered for selection)
 * makes it report "not exhausted" forever — exactly the case player-
 * reported as "End Turn text not present when it should be".
 */
bool game_units_pending_orders(const ColonizeGameState* game) {
  if (!game || !game->units_ok) {
    return false;
  }
  for (int i = 0; i < units_slot_end(&game->units); ++i) {
    const ColonizeUnit* u = &game->units.units[i];
    /* Same filter as the selector (incl. the damaged-hull skip), or the End
     * of Turn prompt never activates while a repairing ship is on the map. */
    if (!turn_unit_in_rotation(&game->units, u, game->human_nation)) {
      continue;
    }
    if (units_orders_skip_turn(u)) {
      continue;
    }
    return true;
  }
  return false;
}

/*
 * ColonizeGameState-shaped adapter for turn.c's shared
 * turn_select_next_unit_awaiting_orders (the standing-order skip loop every
 * hand-off site needs — see turn.h for the rule it enforces). This used to be
 * a byte-for-byte second copy of that body; the two must behave identically
 * inside and outside the turn processor, so there is now exactly one, and this
 * only adds the units_ok guard the game-state callers rely on.
 *
 * EVERY hand-off in this file goes through here, never through a bare
 * turn_select_next_unit: parking the live selection on a Fortified/Sentried
 * unit flashes it into control for a frame before the next tick skips past it.
 */
bool game_select_next_unit_awaiting_orders(ColonizeGameState* game) {
  if (!game || !game->units_ok) {
    return false;
  }
  /* No power at the controls (CHEAT Set Human "None", the demo): every unit
   * is the AI's, so nothing ever waits for orders. */
  if (game->col1_ok && col1_all_ai_fixed_nation(&game->col1) >= 0) {
    return false;
  }
  if (!game->world_map_ok) {
    return turn_select_next_unit_awaiting_orders(&game->units, game->human_nation);
  }
  /*
   * port_bugs.md #626 (pioneer work tick site). DOS does NOT tick clear/plow/road
   * at the nation's move refresh: the human map loop (raw 46873) calls the
   * lasting-order dispatcher FUN_2b5a_3ae6 (raw 46414) for whatever unit is
   * currently active, and 3ae6 switches on that unit's +0x314c order byte
   * (jump table 2b5a:3b58, index orders-2; 8 -> FUN_479b_01a6 clear/plow,
   * 9 -> FUN_479b_0526 road). The select routine FUN_2b5a_1... (raw 42296-
   * 42300) classes orders {5,6,8,9} as "lasting" — the unit still becomes
   * the active one, it just never waits for player input (order 5 is
   * promoted to Fortified there, below) — so a pioneer
   * with order 8/9 is ticked exactly when its turn in the ascending-id
   * rotation comes up, not in a burst at turn start.
   *
   * This loop is that rotation: every hand-off site in the port funnels
   * through here, so the dispatch happens at the same point DOS's does.
   * Only human units reach it (turn_select_next_unit filters on
   * human_nation) — nobody ticks an AI unit's order 8/9 in DOS; the AI
   * improves tiles through the 5952 colony-tick phantom-unit arm, and an
   * AI unit left on order 9 is simply parked.
   */
  for (int guard = 0; guard < COLONIZE_UNITS_MAX; ++guard) {
    if (!turn_select_next_unit(&game->units, game->human_nation)) {
      return false;
    }
    ColonizeUnit* u = units_get(&game->units, game->units.selected_id);
    if (!u) {
      return false;
    }
    ColonizeWorld w = game_world(game);
    w.colonies = game->colonies_ok ? &game->colonies : NULL;
    /* No rng here (as before #1110): a stream would arm the FUN_4cc6_00f2
     * mission-burn roll inside the pioneer work tick, a behaviour change. */
    w.rng = NULL;
    /* divergences.pioneer_plan Automate: a started job spends the turn. */
    if (pioneer_plan_auto_step(&w, u, &game->ai_popups, &game->messages)) {
      continue;
    }
    if (turn_rotation_lasting_step_w(&w, u, &game->ai_popups, &game->messages)) {
      /* The rotation's pick is DOS's FUN_2b5a_0e52 activation, which runs the
       * piece-selected tutorial chain (raw 42330-42332). */
      game_tutorial_piece_selected(game, game->units.selected_id);
      return true;
    }
  }
  return false;
}

/*
 * FUN_2b5a_3b68 (raw 46831-46833), the human's Move Pieces entry: 0x5392 =
 * 0xffff, then the first unit needing orders; a pick forces Move Pieces. The
 * DOS main loop enters it at every human turn start and again after a load
 * (FUN_75c2 load sets DS:0x829, raw 121852 / 121944, and the nation loop at
 * raw 6366-6420 resumes at the human's slot straight into FUN_281f_062c).
 */
void game_move_pieces_entry(ColonizeGameState* game) {
  if (!game || !game->units_ok) {
    return;
  }
  game->units.selected_id = -1;
  game->player_had_control = false; /* raw 46834: 0x97b0 = 0 */
  if (game_select_next_unit_awaiting_orders(game)) {
    game->view_pieces_mode = false;
  }
}

/*
 * Player-requested: flashing "End Turn" sidebar prompt + click-to-confirm.
 * True once turn_select_next_unit has already come up empty this turn
 * (view_pieces_mode) and no unit needs orders — the state game_wait_next_unit
 * lands in right before either auto-ending the turn (game_options.end_of_turn
 * off) or just re-setting the "End of Turn" status text forever with no way
 * to actually confirm it (end_of_turn on — this is exactly the gap the
 * sidebar click fixes: an explicit click *is* the confirmation).
 */
bool game_end_turn_prompt_active(const ColonizeGameState* game) {
  return game && game->units_ok && game->view_pieces_mode &&
         !turn_processor_active(&game->turn_proc) && !game_units_pending_orders(game);
}

/*
 * DOS-LITERAL FUN_2b5a_0e52 no-unit arm (raw 42308-42312; the same tail is
 * inline at raw 45449-45454): 0x53c6 = 1 (the End of Turn prompt),
 * FUN_2b5a_0902 View Pieces at (x, y), and the turn ends only when DS:0x97b0
 * (player_had_control) is set and Game Options End of Turn (0x5383 & 8) is
 * off. A turn on which no unit ever waited for orders stops at the prompt.
 * Every "no unit left needing orders" site ends here: DOS's Move Pieces loop
 * (FUN_2b5a_3b68 raw 46840-46848) re-runs 0e52 on every pass, so a unit used
 * up anywhere (a colony-screen fence drop included) reaches it (port_bugs.md
 * #1302).
 */
void game_no_unit_left(ColonizeGameState* game, int x, int y) {
  if (!game) {
    return;
  }
  game_select_tile(game, x, y);
  if (game->player_had_control &&
      !turn_option_end_of_turn(game->col1_ok ? &game->col1 : NULL, game->col1_ok) &&
      !game_units_pending_orders(game)) {
    game_do_end_turn(game);
    return;
  }
  /* LABELS.TXT @MISC row 2 — the same row the sidebar EOT prompt uses. */
  snprintf(game->status, sizeof(game->status), "%s", game_labels_misc_or(2, ""));
}

void game_wait_next_unit(ColonizeGameState* game) {
  if (!game || !game->units_ok) {
    return;
  }
  /* Never hand control to another unit (or end the turn) from behind a popup
   * or another screen — park it for game_update instead. */
  if (game_defer_turn_flow(game)) {
    return;
  }
  /* Skips past standing-order units (Fortified/Sentry/etc.) rather than
   * stopping the cycle on one; they don't need player attention. */
  const bool found = game_select_next_unit_awaiting_orders(game);
  if (!found) {
    game_no_unit_left(game, game->map_cursor_x, game->map_cursor_y);
    return;
  }
  game->view_pieces_mode = false;
  game_center_on_selected_unit(game);
  /* LABELS.TXT @MISC row 34 (DS:0x2dfe) — the colony-event choice word. */
  snprintf(game->status, sizeof(game->status), "%s", game_labels_misc_or(34, ""));
}

/* ===== Hall of Fame: HALLFAME.DAT (DOS FUN_41f2_0f56) ===== */

/* 42-byte record (21 words), FUN_41f2_14a8 builds it at Retire: name[24]
 * (leader, DS 0x540e + nation*0x34), nation, declared (0x5382 & 1), achieved
 * (0x5382 & 8), year (0x538a), season (0x538c), difficulty (0x53a6), score,
 * rating (sort key, word 19), exploits tier. The file holds slots 0-4 (0xd2
 * bytes); slot 5 is the in-memory catch-all. */
#define GAME_HOF_RECORD_SIZE 42
#define GAME_HOF_FILE_SIZE (COLONIZE_HOF_FILE_SLOTS * GAME_HOF_RECORD_SIZE)

static void game_hof_path(const ColonizeGameState* game, char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!game) {
    return;
  }
  if (!dos_compat_normalize_asset_path(game->resolved_data_dir, "HALLFAME.DAT", out, out_size)) {
    snprintf(out, out_size, "%s/HALLFAME.DAT", game->resolved_data_dir);
  }
}

/* FUN_41f2_0f56 empty slot: name "", nation / score / rating -1, rest 0. */
static void game_hof_clear_slot(ColonizeHofEntry* e) {
  memset(e, 0, sizeof(*e));
  e->nation_id = -1;
  e->score = -1;
  e->rating = -1;
}

/* DOS-LITERAL FUN_41f2_0f56 raw 72604-72630: the entry takes the first slot
 * whose rating is strictly lower, else the catch-all slot 5; slots below it
 * shift down one (slot 5 falls off). Returns the slot taken (local_156). */
int game_hof_insert(ColonizeGameState* game, const ColonizeHofEntry* entry) {
  if (!game || !entry) {
    return -1;
  }
  int slot = 0;
  while (slot < COLONIZE_HOF_MAX - 1 && game->hof_entries[slot].rating >= entry->rating) {
    ++slot;
  }
  memmove(
    &game->hof_entries[slot + 1], &game->hof_entries[slot],
    (size_t)(COLONIZE_HOF_MAX - 1 - slot) * sizeof(game->hof_entries[0])
  );
  game->hof_entries[slot] = *entry;
  return slot;
}

static int game_hof_rd16(const uint8_t* p) {
  return (int16_t)(uint16_t)(p[0] | (p[1] << 8));
}

static void game_hof_wr16(uint8_t* p, int v) {
  p[0] = (uint8_t)(v & 0xff);
  p[1] = (uint8_t)((v >> 8) & 0xff);
}

/* FUN_41f2_0f56 raw 72578-72603: one 0xd2-byte fread; a missing file or a
 * short read leaves every slot empty, and slot 5 is always cleared. */
void game_hof_load(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  for (int i = 0; i < COLONIZE_HOF_MAX; ++i) {
    game_hof_clear_slot(&game->hof_entries[i]);
  }
  char path[640];
  game_hof_path(game, path, sizeof(path));
  FILE* f = fopen(path, "rb");
  if (!f) {
    return;
  }
  uint8_t buf[GAME_HOF_FILE_SIZE];
  const bool ok = fread(buf, sizeof(buf), 1, f) == 1;
  fclose(f);
  if (!ok) {
    return;
  }
  for (int i = 0; i < COLONIZE_HOF_FILE_SLOTS; ++i) {
    const uint8_t* r = buf + i * GAME_HOF_RECORD_SIZE;
    ColonizeHofEntry* e = &game->hof_entries[i];
    memcpy(e->leader, r, 24);
    e->leader[23] = '\0';
    e->nation_id = game_hof_rd16(r + 24);
    e->declared = game_hof_rd16(r + 26) != 0;
    e->achieved = game_hof_rd16(r + 28) != 0;
    e->year = game_hof_rd16(r + 30);
    e->season = game_hof_rd16(r + 32);
    e->difficulty = game_hof_rd16(r + 34);
    e->score = game_hof_rd16(r + 36);
    e->rating = game_hof_rd16(r + 38);
    e->tier = game_hof_rd16(r + 40);
  }
}

/* FUN_41f2_0f56 raw 72714-72716: fwrite of slots 0-4. Written to a sibling
 * temp file and renamed over the old table, so a failed write never
 * truncates the existing Hall of Fame. */
void game_hof_save(const ColonizeGameState* game) {
  if (!game) {
    return;
  }
  uint8_t buf[GAME_HOF_FILE_SIZE];
  memset(buf, 0, sizeof(buf));
  for (int i = 0; i < COLONIZE_HOF_FILE_SLOTS; ++i) {
    uint8_t* r = buf + i * GAME_HOF_RECORD_SIZE;
    const ColonizeHofEntry* e = &game->hof_entries[i];
    snprintf((char*)r, 24, "%s", e->leader);
    game_hof_wr16(r + 24, e->nation_id);
    game_hof_wr16(r + 26, e->declared ? 1 : 0);
    game_hof_wr16(r + 28, e->achieved ? 8 : 0);
    game_hof_wr16(r + 30, e->year);
    game_hof_wr16(r + 32, e->season);
    game_hof_wr16(r + 34, e->difficulty);
    game_hof_wr16(r + 36, e->score);
    game_hof_wr16(r + 38, e->rating);
    game_hof_wr16(r + 40, e->tier);
  }
  char path[640];
  game_hof_path(game, path, sizeof(path));
  char tmp[sizeof(path) + 8];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE* f = fopen(tmp, "wb");
  if (!f) {
    diag_warn("hof: cannot open %s for write", tmp);
    return;
  }
  const bool ok = fwrite(buf, sizeof(buf), 1, f) == 1;
  if (fclose(f) != 0 || !ok) {
    diag_warn("hof: write failed for %s", tmp);
    remove(tmp);
    return;
  }
#ifdef _WIN32
  remove(path); /* MSVCRT rename() does not replace (see col1_save_write_file) */
#endif
  if (rename(tmp, path) != 0) {
    diag_warn("hof: cannot replace %s", path);
    remove(tmp);
  }
}

/*
 * Resolve Col1 trade-stop to map coords. colony_index 999 = Europe (eastern HS
 * for ships; coastal land for wagons). Returns 1 if *ox,*oy set.
 * Cite: ColonizeCol1TradeStop; Colonization.pdf Trade Routes.
 */
static int game_trade_stop_coords(
  ColonizeGameState* game,
  const ColonizeUnit* u,
  uint16_t colony_index,
  int* ox,
  int* oy
) {
  if (!game || !u || !ox || !oy) {
    return 0;
  }
  if (colony_index == 999) {
    if (units_is_sea(&game->units, u->id)) {
      int sx = 0;
      int sy = 0;
      if (units_find_eastern_high_seas_tile(&game->units, &game->world_map, u->y, &sx, &sy)) {
        *ox = sx;
        *oy = sy;
        return 1;
      }
    }
    /* Land: nearest own coastal colony as Europe sail stand-in. */
    int best = -1;
    int bx = 0;
    int by = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &game->colonies.colonies[i];
      if (!c->active || c->nation_id != game->human_nation) {
        continue;
      }
      if (!map_tile_is_coastal(&game->world_map, c->x, c->y)) {
        continue;
      }
      const int d = abs(c->x - u->x) + abs(c->y - u->y);
      if (best < 0 || d < best) {
        best = d;
        bx = c->x;
        by = c->y;
      }
    }
    if (best < 0) {
      return 0;
    }
    *ox = bx;
    *oy = by;
    return 1;
  }
  /* Stops hold DOS colony indices, not port ids (colonies_abandon remaps them). */
  const ColonizeColony* c = colonies_get(
    &game->colonies, colonies_id_at_dos_index(&game->colonies, (int)colony_index)
  );
  if (!c || !c->active) {
    return 0;
  }
  /*
   * DOS FUN_479b_0bd0 not-arrived arm (viceroy_unpacked.c:77234-77238):
   * FUN_281f_09e6(dest colony) then the colony record at DS:0x8542 supplies
   * bytes [0]/[1] straight into the unit's goto pair (+0x314d/+0x314e) — the
   * goto is the COLONY TILE, for ships as much as for wagons, and arrival is
   * the unit standing on it. The old port aimed sea units at the nearest
   * adjacent water tile instead, so a route ship stopped one tile short,
   * counted that as arrival and turned around without ever docking: goods
   * still moved (service_stop allows an adjacent berth) but passengers never
   * came ashore, because disembark-all hangs off units_try_move's colony
   * dock (units_move.c "docking puts everyone ashore"), not off the route
   * code. The flood admits a land goal tile that carries a settlement, so a
   * ship can path to it.
   */
  *ox = c->x;
  *oy = c->y;
  return 1;
}

/*
 * Aim TRADE_ROUTE unit at stop index. OpenCol stand-in: follow_unit_id = route
 * slot (0..11); col1_counter16 = stop index. Load/unload nibbles still thin.
 */
int game_trade_route_aim_stop(ColonizeGameState* game, ColonizeUnit* u, int stop_i) {
  if (!game || !game->col1_ok || !u || u->orders != UNITS_ORDER_TRADE_ROUTE) {
    return 0;
  }
  const int route = u->follow_unit_id;
  if (route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return 0;
  }
  const ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
  if (r->dest_count <= 0) {
    return 0;
  }
  int si = stop_i % (int)r->dest_count;
  if (si < 0) {
    si = 0;
  }
  int tx = 0;
  int ty = 0;
  if (!game_trade_stop_coords(game, u, r->stop[si].colony_index, &tx, &ty)) {
    return 0;
  }
  u->goto_x = tx;
  u->goto_y = ty;
  u->col1_counter16 = si;
  return 1;
}

/*
 * Stop service (before advancing): Europe=999 ship sells holds; own colony
 * uses colonies_trade_route_service_stop (Col1 load/unload nibbles when set).
 * TRADE Edit nibble UI still thin. Cite: ColonizeCol1TradeStop;
 * Colonization.pdf Trade Routes.
 */
static void game_trade_route_service_stop(ColonizeGameState* game, ColonizeUnit* u) {
  if (!game || !u || !game->col1_ok) {
    return;
  }
  const int route = u->follow_unit_id;
  if (route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  const ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
  const int si = u->col1_counter16;
  if (si < 0 || si >= (int)r->dest_count) {
    return;
  }
  const ColonizeCol1TradeStop* st = &r->stop[si];
  const uint16_t cidx = st->colony_index;
  if (cidx == 999) {
    /*
     * Lane-full fallback only (game_trade_route_retarget really crosses to
     * Europe when it can). The old extra `u->x >= 200 || u->y >= 200` arm
     * was a stale Europe-sentinel test: this port keeps crossing ships in
     * EuropeScreen's harbor/expected/bound arrays, never as live units at
     * DOS's 228/232/244+n coordinates, so the only reachable condition is a
     * sea unit standing on a high-seas tile.
     */
    if (units_is_sea(&game->units, u->id) &&
        map_tile_is_high_seas(&game->world_map, u->x, u->y)) {
      /*
       * DOS FUN_479b_0bd0 Europe arrival (viceroy_unpacked.c:77254-77286):
       * for every unload-list cargo, repeat `FUN_281f_0c2c` (find a hold of
       * that type) → `FUN_291f_0d02` (sell it) until no hold is left — the
       * loop ends only when the FIND fails, never on a refused sale. Then
       * BUY the load-list cargos (FUN_38fd_1fa2 via FUN_291f_0b42), one
       * 100-unit hold per list entry, gold permitting.
       *
       * Sweeping the slot array once is the same set of sales without the
       * re-scan (europe_sell_unit_hold zeroes the slot in place, it does not
       * compact), and it matches game_europe_service_trade_harbor's copy
       * exactly. The old shape broke out of the whole cargo on the first
       * refused sale, so one boycotted hold stranded every later hold of
       * that cargo aboard.
       */
      const int holds = units_goods_hold_count(&game->units, u->id);
      const int hold_max = holds < COLONIZE_UNIT_CARGO_MAX ? holds : COLONIZE_UNIT_CARGO_MAX;
      for (int i = 0; i < (int)st->unload_count && i < 6; ++i) {
        const int want = col1_trade_nibble_cargo(st->unload_cargo_nibbles, i);
        for (int h = 0; h < hold_max; ++h) {
          if (u->hold_goods_amount[h] > 0 && u->hold_goods_amount[h] < 255 &&
              u->hold_goods_type[h] == want) {
            const ColonizeWorld w = game_world(game);
            (void)europe_sell_unit_hold_w(&w, u->id, h);
          }
        }
      }
      for (int i = 0; i < (int)st->load_count && i < 6; ++i) {
        const int ct = col1_trade_nibble_cargo(st->load_cargo_nibbles, i);
        const ColonizeWorld w = game_world(game);
        (void)europe_buy_unit_cargo_w(&w, u->id, ct, 100);
      }
      game_europe_drain_price_events(game); /* sells/buys now stamp the record themselves (G3) */
    }
    return;
  }
  int cid = (int)cidx;
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  if (!col || !col->active || col->nation_id != game->human_nation) {
    return;
  }
  /* Ship: allow adjacent coastal berth. */
  if (units_is_sea(&game->units, u->id)) {
    if (abs(u->x - col->x) > 1 || abs(u->y - col->y) > 1) {
      return;
    }
  } else if (u->x != col->x || u->y != col->y) {
    return;
  }
  (void)colonies_trade_route_service_stop(&game->colonies, cid, &game->units, u->id, st);
}

/* If TRADE_ROUTE unit is at current stop (or has no goto), service then advance. */
void game_trade_route_retarget(ColonizeGameState* game, ColonizeUnit* u) {
  if (!game || !u || u->orders != UNITS_ORDER_TRADE_ROUTE) {
    return;
  }
  const int route = u->follow_unit_id;
  if (route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  const ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
  if (r->dest_count <= 0) {
    return;
  }
  const int at_dest =
    (u->goto_x >= UNITS_GOTO_NONE || u->goto_y >= UNITS_GOTO_NONE) ||
    (u->x == u->goto_x && u->y == u->goto_y);
  if (!at_dest) {
    return;
  }
  /*
   * Europe stop, ship on the sea lane: DOS actually sails the ship to
   * Europe — it crosses (voyage turns), docks, sells/buys the stop's
   * lists there and sails back to the next stop (port_bugs.md: "goes to the
   * sea lane tile, then returns. Never visits Europe"). Stamp the route
   * on the Expected slot; the harbor service + return leg live in
   * game_europe_service_trade_harbor / game_europe_deliver_bound_ships.
   * Lane full → fall through to the old service-in-place fallback.
   */
  {
    const int si_here = u->col1_counter16;
    if (game->col1_ok && game->europe_ok && si_here >= 0 && si_here < (int)r->dest_count &&
        r->stop[si_here].colony_index == 999 && units_is_sea(&game->units, u->id) &&
        map_tile_is_high_seas(&game->world_map, u->x, u->y)) {
      const int uid = u->id;
      const int before = game->europe.expected_ships;
      /* Refused (War of Independence) or lane full → fall through to the
       * service-in-place fallback below with the ship still on the map. */
      (void)game_ship_sail_to_europe(game, uid);
      if (game->europe.expected_ships > before) {
        EuropeHarborShip* slot = &game->europe.expected[game->europe.expected_ships - 1];
        slot->trade_route_plus1 = route + 1;
        slot->trade_stop = si_here;
        return; /* unit despawned into the Atlantic lane */
      }
      u = units_get(&game->units, uid);
      if (!u || !u->active) {
        return;
      }
    }
  }
  game_trade_route_service_stop(game, u);
  /*
   * DOS FUN_479b_0bd0 tail: a route whose stops are all the same port warns
   * @ROUTELOOP {%STRING0} and parks the unit for the turn.
   */
  bool all_same = true;
  for (int i = 1; i < (int)r->dest_count; ++i) {
    if (r->stop[i].colony_index != r->stop[0].colony_index) {
      all_same = false;
      break;
    }
  }
  if (all_same) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = r->name[0] ? r->name : "";
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(&game->messages, "ROUTELOOP", &tok, "", body, sizeof(body));
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    u->moves = 0;
    return;
  }
  const int next = (u->col1_counter16 + 1) % (int)r->dest_count;
  (void)game_trade_route_aim_stop(game, u, next);
}

/*
 * Service every trade-route ship that has just docked in Europe (DOS
 * FUN_479b_0bd0 Europe arrival: SELL the stop's unload-list cargos, boycott
 * gated, then BUY the load-list cargos — one 100-unit hold per entry, gold
 * permitting) and set it sailing straight back toward its next stop. The
 * return leg keeps the route stamp; game_europe_deliver_bound_ships re-arms
 * TRADE_ROUTE orders when the ship spawns back on the map.
 */
void game_europe_service_trade_harbor(ColonizeGameState* game) {
  if (!game || !game->europe_ok || !game->col1_ok || !game->units_ok) {
    return;
  }
  EuropeScreen* eu = &game->europe;
  for (int i = 0; i < eu->harbor_ships;) {
    EuropeHarborShip* ship = &eu->harbor[i];
    if (ship->trade_route_plus1 <= 0) {
      ++i;
      continue;
    }
    const int route = ship->trade_route_plus1 - 1;
    if (route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
      ship->trade_route_plus1 = 0;
      ++i;
      continue;
    }
    const ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
    const int si = ship->trade_stop;
    if (r->dest_count <= 0 || si < 0 || si >= (int)r->dest_count ||
        r->stop[si].colony_index != 999) {
      /* Route edited away under the crossing — drop the automation. */
      ship->trade_route_plus1 = 0;
      ++i;
      continue;
    }
    const ColonizeCol1TradeStop* st = &r->stop[si];
    for (int c = 0; c < (int)st->unload_count && c < 6; ++c) {
      const int want = col1_trade_nibble_cargo(st->unload_cargo_nibbles, c);
      for (int h = 0; h < EUROPE_SHIP_CARGO_MAX; ++h) {
        if (ship->hold_goods_amount[h] > 0 && ship->hold_goods_amount[h] < 255 &&
            ship->hold_goods_type[h] == want) {
          (void)europe_sell_hold(eu, &game->col1, game->human_nation, i, h);
        }
      }
    }
    for (int c = 0; c < (int)st->load_count && c < 6; ++c) {
      const int ct = col1_trade_nibble_cargo(st->load_cargo_nibbles, c);
      const ColonizeWorld w = game_world(game);
      (void)europe_buy_cargo_w(&w, game->human_nation, i, ct, 100);
    }
    game_europe_drain_price_events(game); /* sells/buys now stamp the record themselves (G3) */
    const int next = (si + 1) % (int)r->dest_count;
    const int exit_x = ship->exit_x;
    const int exit_y = ship->exit_y;
    const bool exit_east = ship->exit_east;
    const int bound_before = eu->bound_ships;
    if (!europe_set_sail_from_harbor(
          eu, i, game_voyage_turns(game, exit_x), &game->units, game->human_nation
        )) {
      ++i; /* outbound lane full — stay docked, retry next EOT */
      continue;
    }
    if (eu->bound_ships > bound_before) {
      EuropeHarborShip* out = &eu->bound[eu->bound_ships - 1];
      out->trade_stop = next;
      /* Return through the lane tile it left from, not the player's last
       * manual exit (europe_set_sail_from_harbor prefers last_exit_*). */
      if (exit_x > 0 || exit_y > 0) {
        out->exit_x = exit_x;
        out->exit_y = exit_y;
        out->exit_east = exit_east;
      }
    }
    /* Ship left harbor[i]; the array shifted down — do not advance i. */
  }
}

/* Returns false if the game should quit. */

/*
 * Board / unload / sail-home, shared by the MENU.TXT ORDERS rows and by the
 * keyboard keys O, U and H (audit GL-19: the menu arms re-implemented the key
 * handlers line for line, fortified guard and status strings included). Each
 * caller keeps its own asset-availability prefix, which is the only thing that
 * differed: the menu rows answer "Cannot load cargo" / "Cannot return to
 * Europe" when the pools are not up, the keys simply do not fire.
 */
void game_order_board(ColonizeGameState* game) {
  const int sid = game->units.selected_id;
  const int at_cursor = units_id_at(&game->units, game->map_cursor_x, game->map_cursor_y);
  int land_id = -1;
  int ship_id = -1;
  if (sid >= 0 && at_cursor >= 0) {
    if (!units_is_sea(&game->units, sid) && units_is_sea(&game->units, at_cursor)) {
      land_id = sid;
      ship_id = at_cursor;
    } else if (units_is_sea(&game->units, sid) && !units_is_sea(&game->units, at_cursor)) {
      land_id = at_cursor;
      ship_id = sid;
    }
  }
  /* port_bugs.md: never grab a Fortifying/Fortified unit off the tile —
   * fortification has no interaction with boarding in DOS, and units_board
   * forces sentry-aboard (the "fortified soldier became sentried and vanished
   * onto the ship" report). */
  const ColonizeUnit* lu = land_id >= 0 ? units_get_const(&game->units, land_id) : NULL;
  if (lu && (lu->orders == UNITS_ORDER_FORTIFY || lu->orders == UNITS_ORDER_FORTIFIED)) {
    set_status(game, "Unit is fortified", NULL);
  } else if (land_id < 0 || ship_id < 0) {
    set_status(game, "Select land unit and cursor on adjacent ship (or reverse)", NULL);
  } else if (!units_board(&game->units, land_id, ship_id)) {
    set_status(game, "Cannot board (need adjacent ship with free hold)", NULL);
  } else {
    const ColonizeUnit* ship = units_get_const(&game->units, ship_id);
    snprintf(
      game->status, sizeof(game->status), "Boarded ship (hold %d)", ship ? ship->cargo_count : 0
    );
  }
}

void game_order_unload(ColonizeGameState* game) {
  const int sid = game->units.selected_id;
  if (sid < 0 || !units_is_sea(&game->units, sid)) {
    set_status(game, "Select a ship to unload", NULL);
    return;
  }
  const ColonizeUnit* ship = units_get_const(&game->units, sid);
  const int pax_id = (ship && ship->cargo_count > 0) ? ship->cargo_ids[0] : -1;
  const ColonizeWorld w = game_world(game);
  if (!units_unload_w(&w, sid, game->map_cursor_x, game->map_cursor_y)) {
    set_status(game, "Cannot unload (need adjacent free land)", NULL);
    return;
  }
  if (pax_id >= 0) {
    game->units.selected_id = pax_id;
  }
  set_status(game, "Unit unloaded", NULL);
}

/*
 * Ordering a ship home does NOT open the Europe screen: DOS opens it on
 * *arrival*, when europe_tick_voyages moves the ship Expected -> Harbor and
 * raises open_on_dock (port_bugs.md). The War of Independence refusal
 * (@EUROPENOTLEAVE) lives in game_ship_sail_to_europe, which the Go-To-onto-a-
 * lane path shares.
 */
void game_order_return_europe(ColonizeGameState* game) {
  const int sid = game->units.selected_id;
  const ColonizeUnit* ship = units_get_const(&game->units, sid);
  if (!ship || !units_is_sea(&game->units, sid)) {
    set_status(game, "Select a ship to sail to Europe", NULL);
  } else if (!units_on_high_seas(&game->world_map, ship->x, ship->y)) {
    set_status(game, "Ship must be on high seas", NULL);
  } else {
    (void)game_ship_sail_to_europe(game, sid);
  }
}

/*
 * Unit ORDERS "Go to Port" (ships) / "Go to Place" (land units). DOS FUN_2b5a_1dfc opens
 * FUN_647e_01c6(unit, -1, 1, 0) (call 2b5a:1e0e),
 * titled @SAILPORT for a ship, @TRAVELPLACE for any other unit (647e:0227-025c),
 * paged 10 to a page (game_open_dest_pager, port_bugs.md #1194): the Europe row
 * first when the ship reaches the Atlantic, then every colony travel_dest_ok
 * (FUN_647e_0094) passes, in colony order; the at-colony test is on, so the colony
 * the ship lies in is greyed (FUN_291f_01b6). No default. An empty list shows
 * nothing (647e:048f).
 */
static void game_open_goto_port_picker(ColonizeGameState* game, int uid) {
  if (!game || !game->world_map_ok || !game->colonies_ok || !game->units_ok) {
    return;
  }
  TravelDestWho who;
  if (!travel_dest_who_unit(&game->units, uid, &who)) {
    return;
  }
  char prompt[COLONIZE_MSG_LINE_LEN];
  popup_msg_fill(
    &game->messages, who.ship ? "SAILPORT" : "TRAVELPLACE", NULL, "", prompt, sizeof(prompt)
  );
  popup_msg_take_pending_default(); /* @default=1: the parser's, no rows parsed here */
  game->goto_port_pending_unit = uid;
  if (!game_open_dest_pager(game, CHEAT_LIST_KIND_GOTO_PORT, prompt, &who, true, -1, NULL)) {
    game->goto_port_pending_unit = -1;
  }
}

/*
 * Apply a CHEAT_LIST_KIND_GOTO_PORT result (DOS FUN_2b5a_1dfc tail, raw
 * 42806-42821): a colony → order 3 (Go To) aimed at the colony's tile; 999 =
 * Europe → under the War of Independence (DS:0x5382 bit 0, no Man-O-War
 * exemption here) @EUROPENOTLEAVE and nothing else, otherwise
 * FUN_291f_02ea -> FUN_48d3_015e: order 3 aimed at the nearest free-or-own
 * High Seas tile (ring hunt, units_015e_hs_course). The ship sails for
 * Europe when it ARRIVES there (FUN_479b_0972 raw 77095-77107, the Go To
 * onto a lane tile in game_update) — never from the dock. A miss in the ring
 * leaves the unit alone, as in DOS.
 */
void game_apply_goto_port(ColonizeGameState* game, int id) {
  if (!game) {
    return;
  }
  const int uid = game->goto_port_pending_unit;
  game->goto_port_pending_unit = -1;
  if (uid < 0 || !game->units_ok) {
    return;
  }
  int dest_x = 0;
  int dest_y = 0;
  const char* name = "";
  if (id == TRAVEL_DEST_EUROPE) {
    const ColonizeUnit* ship = units_get_const(&game->units, uid);
    if (!ship || !game->world_map_ok) {
      return;
    }
    if (game_europe_blocked_by_woi(game)) {
      game_europe_not_leave_popup(game);
      return;
    }
    if (!units_015e_hs_course(
          &game->units, &game->world_map, ship->x, ship->y, ship->nation_id, &dest_x, &dest_y)) {
      return;
    }
    name = game_trade_europe_label(game);
  } else {
    const ColonizeColony* target = colonies_get(&game->colonies, id);
    if (!target) {
      return;
    }
    dest_x = target->x;
    dest_y = target->y;
    name = target->name;
  }
  const int rc = game_issue_goto(game, uid, dest_x, dest_y);
  if (rc < 0) {
    set_status(game, "Cannot go there", NULL);
  } else if (rc == 0) {
    snprintf(game->status, sizeof(game->status), "Go to %s", name);
  }
}

typedef enum GameMenuActionStatus {
  GAME_MENU_ACTION_UNHANDLED = 0, /* not this group's action — try the next stage */
  GAME_MENU_ACTION_DONE           /* handled; game_apply_map_menu_action returns true */
} GameMenuActionStatus;

/*
 * game_apply_map_menu_action stages: one switch arm cluster each, in the
 * DOS menu order of the original single switch. A stage that does not
 * recognise the action returns UNHANDLED and the dispatcher tries the next.
 */

/* Game menu: save / load / declare independence / retire / exit and the
 * options dialogs. */
static GameMenuActionStatus game_menu_action_system(ColonizeGameState* game, MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_NONE:
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_SEPARATOR:
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_UNIMPLEMENTED:
      set_status(game, "Not implemented yet", NULL);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_SAVE: {
      game_open_save_load(game, SAVE_LOAD_MODE_SAVE);
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_LOAD: {
      game_open_save_load(game, SAVE_LOAD_MODE_LOAD);
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_DECLARE_INDEPENDENCE: {
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      ai_king_menu_declare_independence(&ctx);
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_RETIRE: {
      ColonizeInputState empty;
      memset(&empty, 0, sizeof(empty));
      map_menu_handle_input(&game->map_menu, &empty, NULL, true);
      game_enqueue_yes_no(
        game,
        GAME_MAP_CONFIRM_RETIRE,
        -1,
        "RETIRE",
        "",
        NULL
      );
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_EXIT: {
      game_enqueue_yes_no(
        game, GAME_MAP_CONFIRM_QUIT, -1, "DOS", "", NULL
      );
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_PICK_MUSIC:
      if (!pick_music_open(&game->pick_music, &game->messages)) {
        set_status_menu_row(game, "GAME", 4, "GAME.TXT @PICKMUSIC missing");
      } else {
        set_status(game, "", "Esc closes");
      }
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_OPTIONS:
      if (!game->col1_ok) {
        set_status(game, "Options need a loaded game", NULL);
      } else if (!options_dialog_open_game(
                   &game->options_dlg, &game->messages, &game->col1.head.game_options
                 )) {
        set_status(game, "Options unavailable", NULL);
      }
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_COLONY_OPTIONS:
      if (!game->col1_ok) {
        set_status(game, "Options need a loaded game", NULL);
      } else if (!options_dialog_open_colony(
                   &game->options_dlg,
                   &game->messages,
                   &game->col1.head.colony_report_options
                 )) {
        set_status(game, "Colony options unavailable", NULL);
      }
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_SOUND_OPTIONS: {
      ColonizeSoundOptions so = sound_get_options();
      if (!options_dialog_open_sound(
            &game->options_dlg,
            &game->messages,
            so.background_music,
            so.event_music,
            so.sound_effects
          )) {
        set_status(game, "Sound options unavailable", NULL);
      }
      return GAME_MENU_ACTION_DONE;
    }
    default:
      break;
  }
  return GAME_MENU_ACTION_UNHANDLED;
}

/* View menu: Europe, Find Colony, the zoom tiers, Center View and the
 * View/Move Pieces + Hidden Terrain modes. */
static GameMenuActionStatus game_menu_action_view(ColonizeGameState* game, MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_EUROPE:
      (void)game_try_enter_europe(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_FIND_COLONY:
      game_open_find_colony(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_ZOOM_IN:
      /* In = toward 15×12 (level 0, most detail); Out = toward 120×96. */
      game_map_zoom_set(game, game->map_zoom - 1);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_ZOOM_OUT:
      game_map_zoom_set(game, game->map_zoom + 1);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_ZOOM_LEVEL_120X96:
      game_map_zoom_set(game, 3);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_ZOOM_LEVEL_60X48:
      game_map_zoom_set(game, 2);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_ZOOM_LEVEL_30X24:
      game_map_zoom_set(game, 1);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_ZOOM_LEVEL_15X12:
      game_map_zoom_set(game, 0);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CENTER_VIEW:
      game_center_on_selected_unit(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_VIEW_PIECES:
      /* Deselect whatever's controlled (if anything) and drop into the
       * blinking tile cursor at its current spot — matches a right-click,
       * just without needing a target tile. */
      game_select_tile(game, game->map_cursor_x, game->map_cursor_y);
      set_status(game, "Viewing map", NULL);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_MOVE_PIECES: {
      /* Hand control to the control-queue unit at/after the current
       * cursor position, same cycle Wait/Space walks, but without Wait's
       * end-of-turn fallthrough when none are left — that's View Pieces'
       * job (turn_activation queue picks it up next frame regardless). */
      if (game->units_ok && game->units.selected_id >= 0) {
        game_center_on_selected_unit(game);
        return GAME_MENU_ACTION_DONE;
      }
      const bool found = game_select_next_unit_awaiting_orders(game);
      if (!found) {
        set_status(game, "No units awaiting orders", NULL);
        return GAME_MENU_ACTION_DONE;
      }
      game->view_pieces_mode = false;
      game_center_on_selected_unit(game);
      const ColonizeUnit* next = units_get_const(&game->units, game->units.selected_id);
      snprintf(
        game->status, sizeof(game->status), "Selected %s",
        next ? units_display_name(&game->units, next) : "unit"
      );
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_PLAN_PIONEER:
      /* divergences.pioneer_plan: the overlay is a View Pieces sub-mode. */
      if (game->pioneer_plan_view) {
        game->pioneer_plan_view = false;
      } else {
        if (!game->view_pieces_mode || game->units.selected_id >= 0) {
          game_select_tile(game, game->map_cursor_x, game->map_cursor_y);
        }
        game->pioneer_plan_view = true;
      }
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_VIEW_HIDDEN_TERRAIN:
      game->terrain_peel_phase = 1;
      game->hidden_terrain_phase_ms = game->elapsed_ms;
      set_status_menu_row(game, "VIEW", 9, "units and settlements hidden");
      return GAME_MENU_ACTION_DONE;
    default:
      break;
  }
  return GAME_MENU_ACTION_UNHANDLED;
}

/* Unit orders: activate / wait / fortify / anchor / sentry / disband, the
 * colony and terrain-improvement orders. ORDERS Pillage (0x317) has no
 * handler: FUN_2b5a_0b34 hides it unconditionally, so DOS never dispatches it. */
static GameMenuActionStatus game_menu_action_orders(ColonizeGameState* game, MapMenuAction action) {
  if (game->pioneer_plan_view && game->world_map_ok &&
      (action == MAP_MENU_ACTION_CLEAR_FOREST || action == MAP_MENU_ACTION_PLOW_FIELDS ||
       action == MAP_MENU_ACTION_BUILD_ROAD)) {
    /* Plan Pioneer: the improvement orders edit the cursor tile's plan. */
    const uint8_t bit = action == MAP_MENU_ACTION_CLEAR_FOREST ? PIONEER_PLAN_CLEAR
                      : action == MAP_MENU_ACTION_PLOW_FIELDS  ? PIONEER_PLAN_PLOW
                                                               : PIONEER_PLAN_ROAD;
    const ColonizeWorld w = game_world(game);
    (void)pioneer_plan_toggle(&w, game->human_nation, game->map_cursor_x, game->map_cursor_y, bit);
    return GAME_MENU_ACTION_DONE;
  }
  if (action != MAP_MENU_ACTION_AUTOMATE && action != MAP_MENU_ACTION_WAIT_UNIT &&
      action != MAP_MENU_ACTION_NO_ORDERS && game->units_ok) {
    /* Any other order given to an automated Pioneer takes it off Automate. */
    ColonizeUnit* sel = units_get(&game->units, game->units.selected_id);
    if (sel) {
      sel->pioneer_auto = false;
    }
  }
  switch (action) {
    case MAP_MENU_ACTION_AUTOMATE: {
      ColonizeUnit* u = units_get(&game->units, game->units.selected_id);
      const ColonizeWorld w = game_world(game);
      if (!u || !pioneer_plan_automate(&w, u, &game->ai_popups, &game->messages)) {
        set_status(game, "No planned improvements in reach", NULL);
      } else if (u->moves <= 0 || u->orders != UNITS_ORDER_GOTO) {
        game_wait_next_unit(game);
      }
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_ACTIVATE_UNIT: {
      /*
       * units_id_at only sees units standing on the map, so a passenger in a
       * ship's hold could never be activated from here. If one is already the
       * selection (picked out of the tile stack) and its ship is on the tile
       * under the cursor, activate that instead — port_bugs.md: cancelling a loaded
       * unit's orders has to leave it free to step ashore.
       */
      int at = units_id_at(&game->units, game->map_cursor_x, game->map_cursor_y);
      const ColonizeUnit* sel_pax = units_get_const(&game->units, game->units.selected_id);
      if (sel_pax && sel_pax->active && sel_pax->aboard_ship_id >= 0 &&
          sel_pax->nation_id == game->human_nation &&
          sel_pax->x == game->map_cursor_x && sel_pax->y == game->map_cursor_y) {
        at = game->units.selected_id;
      } else if (unit_stack_try_open(
                   &game->unit_stack, &game->units, game->map_cursor_x, game->map_cursor_y,
                   game->human_nation
                 )) {
        /* FUN_2b5a_2866 -> FUN_2b5a_1b5a: a stack of two or more gets the same
         * picker as a map click (colony tiles included); the pick wakes and
         * selects (game_click_activate_unit). */
        set_status(game, "Choose unit from stack", NULL);
        return GAME_MENU_ACTION_DONE;
      }
      if (at < 0) {
        set_status(game, "No unit at cursor", NULL);
      } else {
        units_wake(&game->units, at);
        game->units.selected_id = at;
        game->view_pieces_mode = false;
        const ColonizeUnit* u = units_get_const(&game->units, at);
        const ColonizeUnitType* ut = u ? units_type(&game->units, u->type_index) : NULL;
        snprintf(game->status, sizeof(game->status), "Activated %s", ut ? ut->name : "");
      }
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_WAIT_UNIT:
      game_wait_next_unit(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_FORTIFY: {
      if (game_order_fortify(game, game->units.selected_id)) {
        game_wait_next_unit(game);
      }
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_ANCHOR: {
      /* The ship half of the same MENU.TXT pair — see game_order_fortify. */
      if (game_order_fortify(game, game->units.selected_id)) {
        game_wait_next_unit(game);
      }
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_SENTRY: {
      const int uid = game->units.selected_id;
      if (uid < 0 || !units_order_sentry(&game->units, uid)) {
        set_status(game, "Cannot sentry", NULL);
      } else {
        set_status(game, "", NULL);
        game_wait_next_unit(game);
      }
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_DISBAND: {
      game_request_disband_confirm(game);
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_BUILD_COLONY:
      (void)game_try_found_colony_at_cursor(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_JOIN_COLONY:
      game_join_colony_order(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CLEAR_FOREST:
    case MAP_MENU_ACTION_PLOW_FIELDS: {
      const int sid = game->units.selected_id;
      char msg[96];
      msg[0] = '\0';
      /* @INDIANFOREST: thunk_FUN_1000_91fc asks when the clear order is given (game_pioneer_land_gate). */
      if (game_pioneer_land_gate(game, GAME_INDIAN_LAND_FOREST)) {
        return GAME_MENU_ACTION_DONE;
      }
      const ColonizeWorld w = game_world(game);
      if (!game->world_map_ok || !game->units_ok ||
          !units_pioneer_plow_w(&w, sid, msg, sizeof(msg), &game->ai_popups, &game->messages)) {
        set_status(game, msg[0] ? msg : "Cannot plow", NULL);
      } else {
        set_status(game, msg, NULL);
        game_wait_next_unit(game);
      }
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_BUILD_ROAD: {
      const int sid = game->units.selected_id;
      char msg[96];
      msg[0] = '\0';
      /* @INDIANROAD: thunk_FUN_1000_9304 asks when the road order is given (game_pioneer_land_gate). */
      if (game_pioneer_land_gate(game, GAME_INDIAN_LAND_ROAD)) {
        return GAME_MENU_ACTION_DONE;
      }
      const ColonizeWorld w = game_world(game);
      if (!game->world_map_ok || !game->units_ok ||
          !units_pioneer_road_w(&w, sid, msg, sizeof(msg), &game->ai_popups, &game->messages)) {
        set_status(game, msg[0] ? msg : "Cannot build road", NULL);
      } else {
        set_status(game, msg, NULL);
        game_wait_next_unit(game);
      }
      return GAME_MENU_ACTION_DONE;
    }
    default:
      break;
  }
  return GAME_MENU_ACTION_UNHANDLED;
}

/* Goto orders, the trade-route menu family and the cargo orders. */
static GameMenuActionStatus game_menu_action_goto_and_cargo(ColonizeGameState* game, MapMenuAction action) {
  switch (action) {
    /* ORDERS 0x320 Go to Port and 0x321 Go to Place both land on case 2b5a:28d4 ->
     * FUN_2b5a_1dfc (port_bugs.md #1208): the same FUN_647e_01c6 list, @TRAVELPLACE for a
     * land unit. */
    case MAP_MENU_ACTION_GOTO_PORT:
    case MAP_MENU_ACTION_GOTO_PLACE: {
      const int sid = game->units.selected_id;
      const ColonizeUnit* u = units_get_const(&game->units, sid);
      if (sid < 0 || !u || !u->active || !units_is_on_map(u)) {
        set_status(game, "Select a unit", NULL);
      } else {
        game_open_goto_port_picker(game, sid);
      }
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_TRADE_ROUTE: {
      const int sid = game->units.selected_id;
      if (sid < 0 || !units_is_transport(&game->units, sid)) {
        set_status(game, "Select a ship or wagon", NULL);
        return GAME_MENU_ACTION_DONE;
      }
      game_open_trade_route_picker(game, 1); /* begin */
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_TRADE_CREATE: {
      if (!game->col1_ok) {
        set_status(game, "No save data for trade routes", NULL);
        return GAME_MENU_ACTION_DONE;
      }
      /* DOS OVL19: 12-route cap → @TRADEMANY {%NUMBER0}. */
      if (game_trade_route_count(game) >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.number0 = (int)COLONIZE_COL1_TRADE_ROUTE_COUNT;
        tok.has_number0 = true;
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages, "TRADEMANY", &tok,
          "",
          body, sizeof(body)
        );
        ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        return GAME_MENU_ACTION_DONE;
      }
      game_trade_wizard_open_dest(game, 1);
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_TRADE_EDIT: {
      if (!game->col1_ok) {
        set_status(game, "No save data for trade routes", NULL);
        return GAME_MENU_ACTION_DONE;
      }
      game_open_trade_route_picker(game, 2); /* edit */
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_TRADE_DELETE: {
      if (!game->col1_ok) {
        set_status(game, "No save data for trade routes", NULL);
        return GAME_MENU_ACTION_DONE;
      }
      game_open_trade_route_picker(game, 3); /* delete */
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_DUMP_OVERBOARD: {
      game_request_overboard_confirm(game);
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_LOAD_CARGO: {
      if (!game->world_map_ok || !game->units_ok) {
        set_status(game, "Cannot load cargo", NULL);
        return GAME_MENU_ACTION_DONE;
      }
      game_order_board(game);
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_UNLOAD_CARGO: {
      if (!game->world_map_ok || !game->units_ok) {
        set_status(game, "Select a ship to unload", NULL);
        return GAME_MENU_ACTION_DONE;
      }
      game_order_unload(game);
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_RETURN_EUROPE: {
      /* Same rules as key H. */
      if (!game->world_map_ok || !game->units_ok || !game->europe_ok) {
        set_status(game, "Cannot return to Europe", NULL);
        return GAME_MENU_ACTION_DONE;
      }
      game_order_return_europe(game);
      return GAME_MENU_ACTION_DONE;
    }
    case MAP_MENU_ACTION_NO_ORDERS: {
      /* "No Orders (space bar)" — this is the *actual* reachable path for a
       * physical Space press (map_menu_orders_hotkey resolves plain Space
       * to this action before the plain-map COLONIZE_KEY_SPACE check ever
       * runs). Space means "done with this unit for the turn": spend its
       * remaining moves (so turn_select_next_unit's moves>0 filter
       * won't offer it again until next turn's refresh), then advance to
       * the next unit needing orders, ending the turn once none remain.
       * This is distinct from W/MAP_MENU_ACTION_WAIT_UNIT, which defers
       * the unit without touching its moves so it comes back up later in
       * the same turn's cycle — the two must not share one code path
       * (player-reported regression: they had become identical). */
      ColonizeUnit* u =
        game->units.selected_id >= 0 ? units_get(&game->units, game->units.selected_id) : NULL;
      if (u) {
        u->moves = 0;
      }
      game_wait_next_unit(game);
      return GAME_MENU_ACTION_DONE;
    }
    default:
      break;
  }
  return GAME_MENU_ACTION_UNHANDLED;
}

/* Colonizopedia entries and the report screens. */
static GameMenuActionStatus game_menu_action_pedia_and_reports(ColonizeGameState* game, MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_PEDIA_CARGO:
      game_open_pedia_list(game, PEDIA_CAT_CARGO);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_PEDIA_UNIT:
      game_open_pedia_list(game, PEDIA_CAT_UNIT);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_PEDIA_TERRAIN:
      game_open_pedia_list(game, PEDIA_CAT_TERRAIN);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_PEDIA_JOB:
      game_open_pedia_list(game, PEDIA_CAT_JOB);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_PEDIA_BUILDING:
      game_open_pedia_list(game, PEDIA_CAT_BUILDING);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_PEDIA_FATHER:
      game_open_pedia_list(game, PEDIA_CAT_FATHER);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_PEDIA_MISC:
      game_open_pedia_list(game, PEDIA_CAT_MISC);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_TERRAIN:
      game_open_terrain_pedia_at_cursor(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_RELIGIOUS:
      game_open_report(game, COLONIZE_REPORT_RELIGIOUS);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_CONGRESS:
      game_open_report(game, COLONIZE_REPORT_CONGRESS);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_LABOR:
      game_open_report(game, COLONIZE_REPORT_LABOR);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_ECONOMIC:
      game_open_report(game, COLONIZE_REPORT_ECONOMIC);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_COLONY:
      game_open_report(game, COLONIZE_REPORT_COLONY);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_NAVAL:
      game_open_report(game, COLONIZE_REPORT_NAVAL);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_FOREIGN:
      game_open_report(game, COLONIZE_REPORT_FOREIGN);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_INDIAN:
      game_open_report(game, COLONIZE_REPORT_INDIAN);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_REPORT_SCORE:
      game_open_score_report(game);
      return GAME_MENU_ACTION_DONE;
    default:
      break;
  }
  return GAME_MENU_ACTION_UNHANDLED;
}

/* Debug tools and the CHEAT menu; also the catch-all default. */
static GameMenuActionStatus game_menu_action_debug_and_cheat(ColonizeGameState* game, MapMenuAction action) {
  switch (action) {
    case MAP_MENU_ACTION_DEBUG_SPRITE_VIEWER:
      game_open_debug_atlas(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS:
      game->debug_show_mouse_coords = !game->debug_show_mouse_coords;
      snprintf(
        game->status,
        sizeof(game->status),
        "Mouse coords: %s",
        game->debug_show_mouse_coords ? "on" : "off"
      );
      game_persist_debug_hud(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_DEBUG_BUILDING_RECTS:
      game->debug_building_rects = !game->debug_building_rects;
      snprintf(
        game->status,
        sizeof(game->status),
        "Building rects: %s",
        game->debug_building_rects ? "on" : "off"
      );
      game_persist_debug_hud(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_DEBUG_LOGS:
      game->debug_logs = !game->debug_logs;
      diag_set_info_enabled(game->debug_logs);
      snprintf(
        game->status,
        sizeof(game->status),
        "Debug logs: %s",
        game->debug_logs ? "on" : "off"
      );
      if (game->debug_logs) {
        diag_info("Debug logs enabled");
      }
      game_persist_debug_hud(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_REVEAL_MAP:
      game_open_cheat_setview(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_KILL_INDIANS:
      game_open_cheat_kill_indians(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_CREATE_UNIT:
      game_open_cheat_create_unit(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_DEBUG_FLAGS:
      game_open_cheat_debug_flags(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_SET_HUMAN:
      game_open_cheat_set_human(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_ADVANCE_REVOLUTION:
      game_cheat_advance_revolution(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_SOUND_TEST:
      game_open_cheat_sound_test(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_MEMORY_CHECK:
      game_cheat_memory_check(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_SHOW_STRATEGY:
      game_cheat_toggle_strategy(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_SHOW_COLONY_SITES:
      game_cheat_toggle_colony_sites(game);
      return GAME_MENU_ACTION_DONE;
    case MAP_MENU_ACTION_CHEAT_TEST_ROUTINE:
      game_cheat_test_routine(game);
      return GAME_MENU_ACTION_DONE;
    default:
      set_status(game, "Not implemented yet", NULL);
      return GAME_MENU_ACTION_DONE;
  }
  return GAME_MENU_ACTION_UNHANDLED;
}

bool game_apply_map_menu_action(ColonizeGameState* game, MapMenuAction action) {
  if (diag_info_enabled() && action != MAP_MENU_ACTION_NONE &&
      action != MAP_MENU_ACTION_SEPARATOR) {
    const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
    if (sel && sel->active) {
      diag_info(
        "COMMAND %s (unit %s id=%d at (%d,%d) mp=%d)",
        map_menu_action_name(action),
        units_display_name(&game->units, sel),
        sel->id,
        sel->x,
        sel->y,
        sel->moves
      );
    } else {
      diag_info("COMMAND %s", map_menu_action_name(action));
    }
  }
  if (game_menu_action_system(game, action) == GAME_MENU_ACTION_DONE) {
    return true;
  }
  if (game_menu_action_view(game, action) == GAME_MENU_ACTION_DONE) {
    return true;
  }
  if (game_menu_action_orders(game, action) == GAME_MENU_ACTION_DONE) {
    return true;
  }
  if (game_menu_action_goto_and_cargo(game, action) == GAME_MENU_ACTION_DONE) {
    return true;
  }
  if (game_menu_action_pedia_and_reports(game, action) == GAME_MENU_ACTION_DONE) {
    return true;
  }
  if (game_menu_action_debug_and_cheat(game, action) == GAME_MENU_ACTION_DONE) {
    return true;
  }
  return true;
}

/*
 * Activating a unit by clicking it cancels whatever it was doing. DOS's
 * click-on-tile handler (2b5a:1b9b..1dda) ends every path that makes a unit
 * active with `unit[active].orders = 0`, and its single-unit branch clears the
 * order before that too — so a click is how you take a Fortified or Sentried
 * unit off its post (port_bugs.md). Its gates are the ones this dispatch already
 * applies before it gets here: the tile must hold a unit of the nation being
 * played, and an own colony is consumed by the colony branch first. Not
 * modelled: DOS's `FUN_281f_0966` corner, which routes an *already* order-less
 * lone unit through the (one-row) stack list instead of straight to select.
 */
void game_click_activate_unit(ColonizeGameState* game, int unit_id) {
  if (!game || !game->units_ok) {
    return;
  }
  /*
   * units_wake is overnight-safe now (port_bugs.md): it refills the allotment
   * only for hold passengers and for standing orders parked on a PREVIOUS
   * turn (col1_counter16 nights counter) — a unit that moved and dug in this
   * turn keeps its spent moves, so the old free-turn hazard is gone and a
   * long-fortified unit can march the turn you rouse it.
   */
  units_wake(&game->units, unit_id);
  /*
   * FUN_2b5a_1b5a raw 42779-42783 makes the clicked unit active
   * (FUN_281f_086c -> 0x5392) whatever its moves, then runs FUN_2b5a_0e52(0):
   * FUN_1427_1410 fails on a spent unit, so FUN_1427_14a0 picks the next unit
   * awaiting orders after it (Move Pieces), or the End of Turn state. Dropping
   * to View Pieces instead stranded every pending unit, Go-To ones unstepped,
   * until the player woke one by hand (campaign8 CN8-B1, turns 39 and 396).
   */
  const ColonizeUnit* u = units_get_const(&game->units, unit_id);
  if (u && u->active && u->nation_id == game->human_nation && u->moves <= 0) {
    game->units.selected_id = unit_id;
    game->map_cursor_x = u->x;
    game->map_cursor_y = u->y;
    game_wait_next_unit(game);
    return;
  }
  game_select_unit(game, unit_id);
}

/*
 * What a plain left-click on map tile (x,y) means, shared by the "no unit
 * selected" click path and the short-click end of a go-to drag. Order:
 * own colony → open it; own units → activate one (stack chooser when the tile
 * holds several); otherwise just move the tile cursor.
 *
 * port_bugs.md: the short-click case used to only re-centre the view, so with a
 * unit already blinking there was no way to make a *different* unit the active
 * one with the mouse.
 */
void game_map_click_dispatch(ColonizeGameState* game, int mx, int my) {
  const int cid = colonies_id_at(&game->colonies, mx, my);
  if (cid >= 0) {
    const ColonizeColony* col = colonies_get(&game->colonies, cid);
    if (col && col->nation_id == game->human_nation) {
      /* FUN_2b5a_3462 raw 46100-46112: an own colony opens (FUN_281f_0608)
       * without leaving Move Pieces, so the active unit is still active when
       * the screen closes (or the cycle moves on if it joined the colony).
       * Only View Pieces moves the cursor to the clicked tile (raw 46025). */
      if (game->view_pieces_mode) {
        game_select_tile(game, mx, my);
      }
      game_enter_colony(game, cid);
      return;
    }
  }

  if (game->units_ok &&
      unit_stack_try_open(&game->unit_stack, &game->units, mx, my, game->human_nation)) {
    set_status(game, "Choose unit from stack", NULL);
    return;
  }

  {
    int stack_ids[UNITS_TILE_STACK_MAX];
    const int n = game->units_ok ? units_collect_tile_stack(
                                     &game->units, mx, my, game->human_nation, stack_ids,
                                     UNITS_TILE_STACK_MAX
                                   )
                                 : 0;
    if (n == 1) {
      game_click_activate_unit(game, stack_ids[0]);
      return;
    }
  }

  const int owned = game_owned_unit_at(game, mx, my);
  if (owned >= 0) {
    game_click_activate_unit(game, owned);
    return;
  }

  /* Human unit with no moves: select the tile under it. DOS still clears the
   * order — it makes the unit active either way — so a unit that dug in after
   * spending its moves is off its post next turn, not still fortified. */
  if (game->units_ok) {
    const int any_id = units_id_at(&game->units, mx, my);
    const ColonizeUnit* any = units_get_const(&game->units, any_id);
    if (any && any->nation_id == game->human_nation) {
      units_clear_orders(&game->units, any_id);
      units_get(&game->units, any_id)->pioneer_auto = false;
      game_select_tile(game, mx, my);
      return;
    }
  }

  /*
   * port_bugs.md: while a unit is actively controlled, clicking an empty tile
   * PANS the viewport (DOS) — it must not drop into View Pieces, or the
   * player could never scroll to a go-to destination outside the view
   * without losing the unit. Only with nothing controlled does the click
   * move the blinking tile cursor as before.
   */
  if (game->units_ok && game->units.selected_id >= 0) {
    const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
    if (sel && sel->active) {
      /* View only — the cursor stays with the controlled unit. port_bugs.md #285:
       * the deliberate pan also holds off the off-screen auto-recentre so
       * the player can reach a far go-to destination. */
      game->view_pan_hold_unit = sel->id;
      game_set_view_center(game, mx, my);
      return;
    }
  }

  game_select_tile(game, mx, my);
}
