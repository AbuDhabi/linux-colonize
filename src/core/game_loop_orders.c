#include "core/internal.h"
#include "core/game_loop.h"

/*
 * Split out of game_loop.c (2026-09-23) — code moved verbatim.
 *
 * Sections:
 *  - Unit selection & movement dispatch
 *  - Foreign-colony trade (FUN_5f7a_020e)
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
#include "core/ff.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/howmuch_dialog.h"
#include "core/map.h"
#include "core/map_gen.h"
#include "core/map_menu.h"
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

/* ===================== Unit selection & movement dispatch (game_set_view_center .. game_key_move_delta) ===================== */


void game_set_view_center(ColonizeGameState* game, int x, int y) {
  if (!game) {
    return;
  }
  game->map_view_x = x;
  game->map_view_y = y;
}

void game_do_end_turn(ColonizeGameState* game);
void game_center_on_selected_unit(ColonizeGameState* game);
void game_after_unit_action(ColonizeGameState* game);
bool game_units_pending_orders(const ColonizeGameState* game);
bool game_select_next_unit_awaiting_orders(ColonizeGameState* game);

/* Tile-select mode: clear unit selection, place blinking cursor, center view. */
void game_select_tile(ColonizeGameState* game, int x, int y) {
  if (!game) {
    return;
  }
  if (game->world_map_ok) {
    map_clamp_coords_inset(&game->world_map, &x, &y);
  }
  if (game->units_ok) {
    game->units.selected_id = -1;
  }
  game->map_cursor_x = x;
  game->map_cursor_y = y;
  game_set_view_center(game, x, y);
  game->view_pieces_mode = true;
}

/* On-map, or awake passenger (orders cleared) with moves remaining. */
bool game_unit_selectable(const ColonizeGameState* game, const ColonizeUnit* u) {
  if (!game || !u || !u->active || u->nation_id != game->human_nation || u->moves <= 0) {
    return false;
  }
  if (units_is_on_map(u)) {
    return true;
  }
  return u->aboard_ship_id >= 0 && u->orders != UNITS_ORDER_SENTRY;
}

/* Select a human unit that still has moves; otherwise select the tile under it. */
void game_select_unit(ColonizeGameState* game, int unit_id) {
  if (!game || !game->units_ok) {
    return;
  }
  const ColonizeUnit* u = units_get_const(&game->units, unit_id);
  if (!u || !u->active) {
    return;
  }
  if (!game_unit_selectable(game, u)) {
    game_select_tile(game, u->x, u->y);
    return;
  }
  game->units.selected_id = unit_id;
  game->view_pieces_mode = false;
  game->map_cursor_x = u->x;
  game->map_cursor_y = u->y;
  game_set_view_center(game, u->x, u->y);
  /* FUN_2b5a_0e52: selecting a piece re-reveals around it (281f_07a0). */
  game_reveal_sight_for_unit(game, u);
  /* units_display_name names the unit purely from its @UNIT row / equipment
   * (bugs.md #591) — it does NOT fold profession into the label, so a
   * toolless Pioneer-professioned Colonist still reads "Colonists" here.
   * The Naval report's passenger column deliberately reads the profession
   * instead (bugs.md #605), which is a separate, port-side extension. */
  snprintf(game->status, sizeof(game->status), "Selected %s", units_display_name(&game->units, u));
  /*
   * DOS-LITERAL FUN_2b5a_001e raw 42006-42009 (overlay twin
   * FUN_OVL02_L0000__000070, overlays.c 55586-55590) — bugs.md #730. Tail of
   * the piece-selected hint chain: with DS:0x5380 bit 0x80 clear and the
   * piece being @UNIT type 0 with @JOB 0x1b (Indian Convert), raise
   * FUN_1000_8842(0x912, 4) = @TUTORIAL19 and latch the bit. Once per game;
   * it does not block anything. The latch is COL1 head tut1 bit7 (nr19), so
   * it round-trips through the save.
   */
  if (game->col1_ok && !game->col1.head.tut1.nr19 &&
      units_type_kind(units_type(&game->units, u->type_index)) == UNITS_KIND_COLONIST &&
      u->profession == UNITS_JOB_CONVERT) {
    game->col1.head.tut1.nr19 = 1;
    PopupMsgTokens ttok;
    memset(&ttok, 0, sizeof(ttok));
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(&game->messages, "TUTORIAL19", &ttok, "", body, sizeof(body));
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
  }
}

static bool game_friendly_colony_at(const ColonizeGameState* game, int x, int y) {
  if (!game) {
    return false;
  }
  const int cid = colonies_id_at(&game->colonies, x, y);
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  return col && col->active && col->nation_id == game->human_nation;
}

/*
 * A failed units_enter_probe/units_try_move reason that DOS shows as a real
 * blocking dialog (not just a status-bar line): @CANNOTATTACK / @SHIPCOMBAT
 * (BOUNCE_FOREIGN, split by the mover's own domain — land vs sea, since the
 * port folds both DOS gates into one reason), @SHIPLAKE (LAKE_BLOCKED) and
 * @LANDFIRST (LANDFIRST). Anything else keeps the plain status line.
 */
static void game_report_enter_reason(
  ColonizeGameState* game, int sid, ColonizeEnterReason reason
) {
  if (!game) {
    return;
  }
  const char* section = NULL;
  const char* fallback = NULL;
  if (reason == COLONIZE_ENTER_BOUNCE_FOREIGN) {
    if (units_is_sea(&game->units, sid)) {
      section = "SHIPCOMBAT";
      fallback = "";
    } else {
      section = "CANNOTATTACK";
      fallback = "";
    }
  } else if (reason == COLONIZE_ENTER_LAKE_BLOCKED) {
    section = "SHIPLAKE";
    fallback = "";
  } else if (reason == COLONIZE_ENTER_LANDFIRST) {
    section = "LANDFIRST";
    fallback = "";
  }
  if (section) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(&game->messages, section, NULL, fallback, body, sizeof(body));
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
  }
  set_status(game, units_enter_reason_status(reason), NULL);
}

/*
 * Commit an eastward in-lane sea step the enter probe refuses with
 * BLOCKED_HS_SAIL (the DOS reason-5 "No, remain in these waters" tail at
 * FUN_4720_049e 0xff5c commits the move). The probe passes movers whose
 * order byte reads as a sail intent (0x314c 2/3), so borrow GOTO orders for
 * the one units_try_move call and restore the unit's real order after.
 */
bool game_commit_sea_lane_step(ColonizeGameState* game, int sid, int dest_x, int dest_y) {
  ColonizeUnit* u = units_get(&game->units, sid);
  if (!u) {
    return false;
  }
  const int prev_orders = u->orders;
  const int prev_gx = u->goto_x;
  const int prev_gy = u->goto_y;
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = dest_x;
  u->goto_y = dest_y;
  const bool ok = units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .rng=(ColonizeDosRng*)(&game->move_rng), .rng_reseed=game->ai_rng_seed, .rng_reseed_set=true}, sid, dest_x, dest_y);
  u = units_get(&game->units, sid);
  if (u) {
    u->orders = prev_orders;
    u->goto_x = prev_gx;
    u->goto_y = prev_gy;
  }
  if (ok) {
    snprintf(game->status, sizeof(game->status), "Moved unit to (%d,%d)", dest_x, dest_y);
  } else {
    set_status(game, units_enter_reason_status(units_last_enter_reason()), NULL);
  }
  return ok;
}

/*
 * Move selected unit to dest: ship landfall unload, colony dock disembark,
 * awake passenger walking ashore, or normal try_move.
 */
#include "core/game_loop_internal.h" /* GameMoveStep */

/*
 * game_try_unit_move stages, in DOS order: each returns a GameMoveStep the
 * dispatcher honours (CONTINUE = the stage did not claim the move).
 */

/* Awake passenger aboard a ship stepping onto adjacent land. */
COLONIZE_INTERNAL GameMoveStep game_move_passenger_unload(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, const ColonizeColonyPool* colonies,
  int dest_x, int dest_y
) {
  /* Awake passenger: walk onto adjacent land → unload. */
  if (selected->aboard_ship_id >= 0) {
    if (selected->orders == UNITS_ORDER_SENTRY) {
      set_status(game, "Wake unit from stack first", NULL);
      return GAME_MOVE_RETURN_FALSE;
    }
    const int ship_id = selected->aboard_ship_id;
    if (!units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, ship_id, sid, dest_x, dest_y)) {
      /*
       * bugs.md #721: DOS FUN_4720_015c reason 9 (viceroy_unpacked_2.c raw
       * 74720-74728) is a BLOCKING dialog, not a status line — the
       * FUN_4720_049e jump table at 4720:060a maps it to `push 0x1429` =
       * @LANDFIRST ("Land units cannot enter an enemy occupied square from on
       * board a ship."). The probe inside units_unload_passenger_w already
       * raises COLONIZE_ENTER_LANDFIRST; route it through the shared reporter
       * the way every other refused step does.
       */
      game_report_enter_reason(game, sid, units_last_enter_reason());
      return GAME_MOVE_RETURN_FALSE;
    }
    game->units.selected_id = sid;
    snprintf(game->status, sizeof(game->status), "Disembarked to (%d,%d)", dest_x, dest_y);
    game_after_unit_action(game);
    return GAME_MOVE_RETURN_TRUE;
  }
  return GAME_MOVE_CONTINUE;
}

/* Ship moves: water steps, colony docking, Europe lanes and naval combat. */
COLONIZE_INTERNAL GameMoveStep game_move_sea_unit(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, const ColonizeColonyPool* colonies,
  int dest_x, int dest_y
) {
  if (units_is_sea(&game->units, sid)) {
    const bool dest_water = map_tile_is_water(&game->world_map, dest_x, dest_y);
    const bool dest_land = map_tile_is_land(&game->world_map, dest_x, dest_y);
    /*
     * DOS 4720 reason 4 (bugs.md #717): a ship pushed off the east/west rim
     * runs reason 5's own UI body (jump table 4720:060a entries 3 and 4 are
     * the same 0x3FEA6), so it is the SAME @SAILHOME / @EUROPENOTLEAVE
     * question. Only the tail differs (0x3FEEA `cmp [0x9e4e],4` → abort at
     * 0xfff2): "No" must not commit the step. game_commit_sea_lane_step
     * re-probes and the off-map destination refuses it, which is exactly
     * that abort — so the shared handler below needs no reason split.
     */
    const bool edge_sail =
      !dest_water && !dest_land &&
      units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, selected->type_index, dest_x, dest_y, sid) == COLONIZE_ENTER_EDGE_SAIL;
    /*
     * DOS 4720 reason 5 (FUN_4720_049e case 4, asm 0x3FEA6): an eastward
     * step deeper into the sea lane without a sail order is NOT a hard deny
     * — it asks @SAILHOME ("Shall we sail for Europe?"). Yes → sail; No →
     * the step commits and the ship remains in these waters (sea lanes are
     * fully traversable, bugs.md). During the WoI, @EUROPENOTLEAVE info
     * fires instead and the step still commits (DS:0x5382 bit0 gate).
     */
    if (edge_sail ||
        (dest_water &&
        units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, selected->type_index, dest_x, dest_y, sid) == COLONIZE_ENTER_BLOCKED_HS_SAIL)) {
      /* bugs.md #870 / FUN_479b raw 76477-76484: a crown Man-O-War on the
       * crown's own turn is exempt from the WoI gate and sails as usual. */
      if (game->col1_ok && game->col1.head.game_options.woi &&
          !game_ship_woi_europe_exempt(game, selected)) {
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages, "EUROPENOTLEAVE", NULL,
          "",
          body, sizeof(body)
        );
        ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        if (!game_commit_sea_lane_step(game, sid, dest_x, dest_y)) {
          return GAME_MOVE_RETURN_FALSE;
        }
        game_after_unit_action(game);
        return GAME_MOVE_RETURN_TRUE;
      }
      if (game->europe_ok) {
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages, "SAILHOME", NULL,
          "",
          body, sizeof(body)
        );
        char label_buf[2][POPUP_MSG_CHOICE_LEN];
        const char* labels[2];
        (void)popup_msg_section_labels(
          &game->messages, "SAILHOME", NULL, "",
          "", label_buf, labels
        );
        const int ids[2] = {1, 0};
        if (ai_popup_enqueue_choice_ctx(
              &game->ai_popups, AI_POPUP_TAG_SAILHOME, sid, dest_x, dest_y, NULL, body,
              labels, ids, 2
            )) {
          set_status(game, "", NULL);
          return GAME_MOVE_RETURN_TRUE;
        }
      }
      /* No Europe screen (or queue full): just commit the step, DOS "No". */
      if (!game_commit_sea_lane_step(game, sid, dest_x, dest_y)) {
        return GAME_MOVE_RETURN_FALSE;
      }
      game_after_unit_action(game);
      return GAME_MOVE_RETURN_TRUE;
    }
    if (dest_land && game_friendly_colony_at(game, dest_x, dest_y)) {
      /* units_try_move puts passengers ashore on docking (bugs.md), so count
       * them before the move to report what came off. */
      const int n = selected->cargo_count;
      if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(&game->world_map), .rng=(ColonizeDosRng*)(&game->move_rng), .rng_reseed=game->ai_rng_seed, .rng_reseed_set=true}, sid, dest_x, dest_y)) {
        set_status(game, units_enter_reason_status(units_last_enter_reason()), NULL);
        return GAME_MOVE_RETURN_FALSE;
      }
      game->units.selected_id = sid;
      if (n > 0) {
        snprintf(game->status, sizeof(game->status), "Docked; %d disembarked", n);
      } else {
        snprintf(game->status, sizeof(game->status), "Moved to colony (%d,%d)", dest_x, dest_y);
      }
      game_after_unit_action(game);
      return GAME_MOVE_RETURN_TRUE;
    }
    if (dest_land && !dest_water) {
      const ColonizeEnterReason landfall = units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, selected->type_index, dest_x, dest_y, sid);
      /* Native village: FUN_4d56_4528 ship abort — never @LANDFALL. */
      if (landfall == COLONIZE_ENTER_VILLAGE_SHIP) {
        if (game->col1_ok) {
          ColonizeTurnContext ctx;
          game_fill_turn_context(game, &ctx);
          if (ai_contact_try_ship_village_unit(&ctx, selected->nation_id, dest_x, dest_y, sid)) {
            return GAME_MOVE_RETURN_TRUE;
          }
        }
        set_status(game, units_enter_reason_status(landfall), NULL);
        return GAME_MOVE_RETURN_FALSE;
      }
      if (landfall != COLONIZE_ENTER_LANDFALL) {
        game_report_enter_reason(game, sid, landfall);
        return GAME_MOVE_RETURN_FALSE;
      }
      const int pax_ready = units_first_landfall_cargo(&game->units, sid);
      if (pax_ready < 0) {
        /* DOS FUN_4720_015c writes the landfall reason (2/3) only when some
         * passenger's spent byte is below its max — with none, reason stays 0
         * and the move is simply refused, no @LANDFALL prompt (bugs.md #423). */
        set_status(
          game,
          selected->cargo_count > 0 ? "No unit aboard has moves left" :
                                      "No unit ready to disembark",
          NULL
        );
        return GAME_MOVE_RETURN_FALSE;
      }
      {
        /* GAME.TXT @LANDFALL / @LANDFALL2: Stay With Ships / Make Landfall.
         * DOS FUN_4720_015c (raw ~76017) writes reason 2 for a plain coastal
         * landfall and reason 3 (river-bit 0x40 of the dest tile's terrain
         * flags set) for the river variant; FUN_OVL08_L0040 picks the tag
         * string off that reason (0x13ea LANDFALL vs 0x13f3 LANDFALL2). */
        const char* landfall_section =
          map_tile_has_river(&game->world_map, dest_x, dest_y) ? "LANDFALL2" : "LANDFALL";
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages,
          landfall_section,
          NULL,
          "",
          body,
          sizeof(body)
        );
        char label_buf[2][POPUP_MSG_CHOICE_LEN];
        const char* labels[2];
        (void)popup_msg_section_labels(
          &game->messages, landfall_section, NULL, "", "", label_buf,
          labels
        );
        const int ids[] = {0, 1};
        if (!ai_popup_enqueue_choice_ctx(
              &game->ai_popups,
              AI_POPUP_TAG_LANDFALL,
              sid,
              dest_x,
              dest_y,
              NULL,
              body,
              labels,
              ids,
              2
            )) {
          if (!units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, sid, pax_ready, dest_x, dest_y)) {
            set_status(game, "Move blocked", NULL);
            return GAME_MOVE_RETURN_FALSE;
          }
          /*
           * bugs.md #716: landfall costs the SHIP nothing. DOS's reason 2/3
           * body (FUN_4720_049e, viceroy_ndisasm.asm 0x3FE30-0x3FE6A) takes
           * the CHOICE, and on "Make Landfall" (ax == 2) only pushes the
           * picked passenger index [0x9e50] into FUN_281f_086c to make it the
           * active unit, then walks the ship's cargo chain clearing order
           * bytes — the ship neither moves nor has its spent byte touched.
           * The passenger's own landfall step spends its whole allotment,
           * exactly as game_apply_popup_voyage's twin does.
           */
          {
            ColonizeUnit* pax = units_get(&game->units, pax_ready);
            if (pax) {
              pax->moves = 0;
            }
          }
          game->units.selected_id = pax_ready;
          snprintf(game->status, sizeof(game->status), "Landfall at (%d,%d)", dest_x, dest_y);
          game_after_unit_action(game);
          return GAME_MOVE_RETURN_TRUE;
        }
      }
      set_status(game, "Landfall…", NULL);
      return GAME_MOVE_RETURN_TRUE;
    }
  }
  return GAME_MOVE_CONTINUE;
}

/* ===== Foreign-colony trade (FUN_5f7a_020e) — docs/foreign_colony_trade.md ===== */

/*
 * @GREATLEADER2 line `nation` ("the Queen" / "the King" / …) — the %STRING0 of
 * @TRADEMERCANTILISM (FUN_2a1f_0618(0, DS:0x1aab "LEADER2", owner), raw 98931).
 */
static const char* game_foreign_trade_leader2(const ColonizeGameState* game, int nation) {
  const ColonizeMsgSection* sec =
    (nation >= 0 && nation < 4) ? assets_msg_find(&game->messages, "GREATLEADER2") : NULL;
  int idx = 0;
  for (int i = 0; sec && i < sec->line_count; ++i) {
    const char* line = sec->lines[i];
    if (!line || line[0] == '\0' || line[0] == ';' || line[0] == '@') {
      continue;
    }
    if (idx == nation) {
      return line;
    }
    idx++;
  }
  return ai_diplo_rival_name(&game->col1, nation);
}

static ColonizeWorld game_foreign_trade_world(ColonizeGameState* game) {
  return world_make(
    &game->units, &game->colonies, &game->world_map, &game->col1, game->col1_ok,
    &game->move_rng, NULL
  );
}

/* A plain GAME.TXT refusal popup (raw 98924-98936 all land here). */
static void game_foreign_trade_refuse(
  ColonizeGameState* game, const char* tag, const PopupMsgTokens* tok, const char* fallback
) {
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, tag, tok, fallback, body, sizeof(body));
  ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
  (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_INFO);
}

/*
 * raw 98993-99018: price the hold, then either @TRADENOWANT or the @TRADEWITH
 * counter-offer CHOICE. The priced deal is latched on the game state because
 * the gold offer costs one draw off the move rng.
 */
void game_foreign_trade_price_hold(
  ColonizeGameState* game, int unit_id, int colony_id, int hold
) {
  ColonizeWorld w = game_foreign_trade_world(game);
  ColonizeForeignTradeDeal deal;
  if (!colonies_foreign_trade_prepare(&w, &game->move_rng, colony_id, unit_id, hold, &deal)) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string1 = reports_cargo_display_name(deal.sold_cargo);
  tok.number1 = deal.sold_qty;
  /* @TRADENOWANT prints the refused cargo as %NUMBER0 %STRING0 (raw 99042-99048). */
  tok.string0 = tok.string1;
  tok.number0 = deal.sold_qty;
  if (deal.offer_cargo < 0) {
    game_foreign_trade_refuse(
      game, "TRADENOWANT", &tok,
      "");
    return;
  }
  tok.string0 = reports_cargo_display_name(deal.offer_cargo);
  tok.number0 = deal.offer_qty;
  tok.number2 = deal.gold;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages, "TRADEWITH", &tok,
    "",
    body, sizeof(body)
  );
  char choices[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const ColonizeMsgSection* sec = assets_msg_find(&game->messages, "TRADEWITH");
  const int nch = popup_msg_choices(sec, choices, AI_POPUP_CHOICE_MAX);
  char c0[AI_POPUP_CHOICE_LEN];
  char c1[AI_POPUP_CHOICE_LEN];
  popup_msg_apply_tokens(
    c0, sizeof(c0), nch >= 1 ? choices[0] : "", &tok
  );
  popup_msg_apply_tokens(
    c1, sizeof(c1), nch >= 2 ? choices[1] : "", &tok
  );
  const char* labels[3] = {c0, c1, nch >= 3 ? choices[2] : ""};
  const int ids[3] = {1, 2, 3};
  if (ai_popup_enqueue_choice_ctx(
        &game->ai_popups, AI_POPUP_TAG_FOREIGN_TRADE_OFFER, unit_id, colony_id, hold, NULL,
        body, labels, ids, 3
      )) {
    game->foreign_trade_unit = unit_id;
    game->foreign_trade_colony = colony_id;
    game->foreign_trade_hold = hold;
    game->foreign_trade_deal = deal;
    (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_FOREIGN_TRADE_OFFER);
  }
}

/*
 * FUN_5f7a_020e head. Returns true when the step is consumed (DOS: 020e always
 * returns 1, so the unit never enters and loses its whole allotment).
 */
bool game_foreign_trade_open(
  ColonizeGameState* game, int unit_id, int colony_id, int dest_x, int dest_y
) {
  ColonizeWorld w = game_foreign_trade_world(game);
  const ColonizeForeignTradeGate gate =
    colonies_foreign_trade_gate(&w, colony_id, unit_id);
  if (gate == COLONIZE_FTRADE_NONE) {
    return false;
  }
  /* Already asking about this unit — hold the step, do not stack a second menu. */
  for (int qi = -1; qi < game->ai_popups.queue_count; ++qi) {
    const AiPopupRequest* r = qi < 0 ? &game->ai_popups.current : &game->ai_popups.queue[qi];
    if (qi < 0 && !game->ai_popups.open) {
      continue;
    }
    if ((r->tag == AI_POPUP_TAG_FOREIGN_TRADE_WHICH ||
         r->tag == AI_POPUP_TAG_FOREIGN_TRADE_OFFER) &&
        r->nation_a == unit_id) {
      /* bugs.md #802: no GAME.TXT row and no DOS status-line write here —
       * the "Trading" chrome was invented; the popup itself is the feedback. */
      return true;
    }
  }
  ColonizeUnit* u = units_get(&game->units, unit_id);
  const ColonizeColony* col = colonies_get(&game->colonies, colony_id);
  if (!u || !col) {
    return false;
  }
  /* The move is over either way (FUN_281f_0934 spends the whole allotment). */
  u->moves = 0;
  if (units_orders_follow_goto(u->orders)) {
    units_clear_orders(&game->units, unit_id);
  }
  (void)dest_x;
  (void)dest_y;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  switch (gate) {
    case COLONIZE_FTRADE_ATWAR:
      game_foreign_trade_refuse(
        game, "TRADEATWAR", NULL,
        "");
      break;
    case COLONIZE_FTRADE_MERCANTILISM:
      /* %STRING0 = @LEADER2[owner] (FUN_2a1f_0618(0, 0x1aab, owner), raw 98931). */
      tok.string0 = game_foreign_trade_leader2(game, col->nation_id);
      game_foreign_trade_refuse(
        game, "TRADEMERCANTILISM", &tok,
        "");
      break;
    case COLONIZE_FTRADE_NOCARGO:
      game_foreign_trade_refuse(
        game, "TRADENOCARGO", NULL,
        "");
      break;
    case COLONIZE_FTRADE_OK:
    default: {
      /* raw 98938-98962: one hold deals straight through; several ask which. */
      const int holds = units_goods_hold_count(&game->units, unit_id);
      int rows = 0;
      char labels_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
      const char* labels[AI_POPUP_CHOICE_MAX];
      int ids[AI_POPUP_CHOICE_MAX];
      int only_hold = -1;
      for (int h = 0; h < holds && rows < AI_POPUP_CHOICE_MAX - 1; ++h) {
        const int amt = units_hold_amount(&game->units, unit_id, h);
        if (amt <= 0) {
          continue;
        }
        only_hold = h;
        snprintf(
          labels_buf[rows], sizeof(labels_buf[rows]), "%d %s", amt,
          reports_cargo_display_name(u->hold_goods_type[h])
        );
        labels[rows] = labels_buf[rows];
        ids[rows] = h + 1; /* DOS rows are 1-based (raw 98953) */
        rows++;
      }
      if (rows <= 0) {
        break;
      }
      if (rows == 1) {
        game_foreign_trade_price_hold(game, unit_id, colony_id, only_hold);
        break;
      }
      /* Cancel row text = DS:0x2dfa = LABELS @MISC[32] ((0x2dfa-0x2dba)/2). */
      {
        const char* nothing_live = reports_labels_field("MISC", 32);
        snprintf(
          labels_buf[rows], sizeof(labels_buf[rows]), "%s", nothing_live ? nothing_live : "Nothing"
        );
      }
      labels[rows] = labels_buf[rows];
      ids[rows] = 99; /* raw 98957: id 99 = cancel */
      rows++;
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        &game->messages, "TRADEWHICH", NULL,
        "", body, sizeof(body)
      );
      if (ai_popup_enqueue_choice_ctx(
            &game->ai_popups, AI_POPUP_TAG_FOREIGN_TRADE_WHICH, unit_id, colony_id,
            dest_x | (dest_y << 8), NULL, body, labels, ids, rows
          )) {
        (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_FOREIGN_TRADE_WHICH);
      }
      break;
    }
  }
  game_after_unit_action(game);
  return true;
}

/* Pre-move native interceptions: village entry, @WHACKINDIANS and the
 * @INDIANLAND land-demand prompt. */
COLONIZE_INTERNAL GameMoveStep game_move_native_prompts(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, int dest_x, int dest_y
) {
  /*
   * FUN_4d56_4528: a land unit moving onto a village tile never enters it;
   * the human gets the @ACTIONS menu (met or not). Cite: indian_settlement_4528.md.
   */
  if (game->col1_ok && !units_is_sea(&game->units, sid) && game->col1.tribe) {
    for (uint16_t ti = 0; ti < game->col1.head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &game->col1.tribe[ti];
      if ((int)t->x != dest_x || (int)t->y != dest_y) {
        continue;
      }
      if (t->nation_id < 4 || t->nation_id > 11) {
        continue;
      }
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      /* FUN_4d56_4528 human arm opens at 4d56:478a with woodcut 7. */
      if (selected->nation_id == game->human_nation) {
        (void)woodcut_fire(&game->col1, WOODCUT_ENTERING_INDIAN_VILLAGE);
      }
      /*
       * FUN_4d56_4528 human arm: every land unit gets the same NAMES.TXT
       * @ACTIONS menu, rows gated per unit (armed → Demand Tribute / Attack
       * Village; the old invented Attack/Leave "raid warn" is retired).
       * Attack Village is committed in game_apply_ai_popup_result; the
       * move is deferred for combat-role units so it can still be made.
       */
      const int combatish = combat_unit_is_combat_role(&game->units, sid);
      (void)ai_contact_try_village_meet_unit_at(
        &ctx,
        selected->nation_id,
        (int)t->nation_id,
        selected->profession == UNITS_JOB_MISSIONARY,
        t->state.capital,
        sid,
        (int)ti
      );
      if (ai_contact_meet_pending_for_unit(&game->ai_popups, sid)) {
        /*
         * bugs.md #722: no step charge here. FUN_465b_0000 dispatches the
         * village tile at raw 75484-75492 — FUN_281f_06f0(dest) >= 0 →
         * FUN_2a1f_016c → FUN_4d56_4528, `goto LAB_465b_0bd1` — which is
         * BEFORE LAB_465b_05ca, the only place local_40 is ever added to
         * +0x3149. The single MP consequence is 4528's own tail
         * (OVL13:0x4c0a), which forfeits the WHOLE allotment and which
         * ai_contact.c already reproduces for every unit kind. The port's
         * extra pre-charge double-billed the non-combat mover.
         */
        (void)combatish;
        set_status(game, "Village…", NULL);
        game_after_unit_action(game);
        return GAME_MOVE_RETURN_TRUE;
      }
      break;
    }
  }

  /* FUN_465b_0000 @WHACKINDIANS: first attack on a not-yet-hostile tribe's unit asks once. */
  if (game->col1_ok && !units_is_sea(&game->units, sid) &&
      combat_unit_is_combat_role(&game->units, sid)) {
    const int foe_id = units_id_at(&game->units, dest_x, dest_y);
    const ColonizeUnit* foe = foe_id >= 0 ? units_get_const(&game->units, foe_id) : NULL;
    if (foe && foe->nation_id >= 4 && foe->nation_id <= 11 && foe->nation_id != selected->nation_id) {
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      if (ai_contact_try_whack_confirm(&ctx, selected->nation_id, foe->nation_id, sid, dest_x, dest_y)) {
        set_status(game, "Attack?", NULL);
        return GAME_MOVE_RETURN_TRUE;
      }
    }
  }
  /*
   * bugs.md #438 / DOS FUN_5f7a_000e: a SCOUT stepping onto a foreign Euro
   * colony gets the @SCOUTCOLONY menu (Meet With Mayor / Infiltrate Colony /
   * Attack Colony / Nothing) instead of a bare attack. The colony_attack_ok
   * latch doubles as the "Attack Colony" pass-through.
   */
  /* A garrisoned colony still gets the menu: no empty-tile test here, or the
   * step fell through to the @HAVETREATY confirm. */
  if (game->col1_ok && !units_is_sea(&game->units, sid) &&
      !(game->colony_attack_ok_unit == sid &&
        game->colony_attack_ok_payload == (dest_x | (dest_y << 8)))) {
    const ColonizeUnitType* sty = units_type(&game->units, selected->type_index);
    if (sty && units_type_kind(sty) == UNITS_KIND_SCOUT) {
      const int cid = colonies_id_at(&game->colonies, dest_x, dest_y);
      const ColonizeColony* col = colonies_get(&game->colonies, cid);
      if (col && col->active && col->nation_id >= 0 && col->nation_id <= 3 &&
          col->nation_id != selected->nation_id) {
        bool pending = game->ai_popups.open &&
                       game->ai_popups.current.tag == AI_POPUP_TAG_SCOUT_COLONY &&
                       game->ai_popups.current.nation_a == sid;
        for (int qi = 0; !pending && qi < game->ai_popups.queue_count; ++qi) {
          pending = game->ai_popups.queue[qi].tag == AI_POPUP_TAG_SCOUT_COLONY &&
                    game->ai_popups.queue[qi].nation_a == sid;
        }
        if (pending) {
          set_status(game, "Scouts…", NULL);
          return GAME_MOVE_RETURN_TRUE;
        }
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = col->name[0] ? col->name : "the colony";
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages, "SCOUTCOLONY", &tok,
          "",
          body, sizeof(body)
        );
        char choices[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
        const ColonizeMsgSection* sec = assets_msg_find(&game->messages, "SCOUTCOLONY");
        const int nch = popup_msg_choices(sec, choices, AI_POPUP_CHOICE_MAX);
        const char* labels[4] = {
          nch >= 1 ? choices[0] : "",
          nch >= 2 ? choices[1] : "",
          nch >= 3 ? choices[2] : "",
          nch >= 4 ? choices[3] : ""
        };
        const int ids[4] = {1, 2, 3, 4};
        const int payload = dest_x | (dest_y << 8);
        if (ai_popup_enqueue_choice_ctx(
              &game->ai_popups, AI_POPUP_TAG_SCOUT_COLONY, sid, cid, payload, NULL, body,
              labels, ids, 4
            )) {
          (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_SCOUT_COLONY);
          set_status(game, "Scouts…", NULL);
          return GAME_MOVE_RETURN_TRUE;
        }
      }
    }
  }
  /*
   * FUN_5f7a_0662's second arm (raw 99060-99062): a unit whose @UNIT cargo
   * column (DS:0x5237) is non-zero — every Wagon Train and every ship — bumping
   * a foreign Euro colony runs FUN_5f7a_020e instead of entering.
   * docs/foreign_colony_trade.md.
   */
  if (game->col1_ok) {
    const ColonizeUnitType* tty = units_type(&game->units, selected->type_index);
    const bool is_scout = tty && units_type_kind(tty) == UNITS_KIND_SCOUT; /* DOS type 5 → 000e */
    const int cid = colonies_id_at(&game->colonies, dest_x, dest_y);
    const ColonizeColony* col = colonies_get(&game->colonies, cid);
    if (!is_scout && col && col->active && col->nation_id >= 0 && col->nation_id <= 3 &&
        col->nation_id != selected->nation_id &&
        units_goods_hold_count(&game->units, sid) > 0 &&
        game_foreign_trade_open(game, sid, cid, dest_x, dest_y)) {
      return GAME_MOVE_RETURN_TRUE;
    }
  }
  /*
   * FUN_5f7a_0662 tail (raw 99069-99080): during the War of Independence a
   * human-controlled Euro unit may not step onto the colony of a Euro power
   * that is neither human-controlled nor the Crown — @NOWARSDURINGREV
   * (DS:0x1af3), abort, full allotment spent.
   */
  if (game->col1_ok && game->col1.head.game_options.woi && selected->nation_id >= 0 &&
      selected->nation_id <= 3 && game->col1.player[selected->nation_id].control == 0) {
    const int cid = colonies_id_at(&game->colonies, dest_x, dest_y);
    const ColonizeColony* col = colonies_get(&game->colonies, cid);
    const int owner = col && col->active ? col->nation_id : -1;
    if (owner >= 0 && owner != selected->nation_id &&
        !(owner <= 3 && game->col1.player[owner].control == 0) &&
        owner != (int)game->col1.head.crown_nation_id) {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        &game->messages, "NOWARSDURINGREV", NULL,
        "",
        body, sizeof(body)
      );
      ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_INFO);
      selected->moves = 0;
      if (units_orders_follow_goto(selected->orders)) {
        units_clear_orders(&game->units, sid);
      }
      game_after_unit_action(game);
      return GAME_MOVE_RETURN_TRUE;
    }
  }
  return GAME_MOVE_CONTINUE;
}

/* Pre-move colony / combat interceptions: the attack confirms and the
 * combat-analysis prompts. */
COLONIZE_INTERNAL GameMoveStep game_move_colony_prompts(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, int dest_x, int dest_y
) {
  /*
   * bugs.md #437: attacking a foreign EURO COLONY — armed or not, at war or
   * not — always asks first. One-shot latch (colony_attack_ok) so the Yes
   * retry through game_try_unit_move does not re-ask.
   */
  if (game->col1_ok && combat_unit_is_combat_role(&game->units, sid) &&
      !units_is_sea(&game->units, sid) &&
      units_id_at(&game->units, dest_x, dest_y) < 0 &&
      !(game->colony_attack_ok_unit == sid &&
        game->colony_attack_ok_payload == (dest_x | (dest_y << 8)))) {
    const int cid = colonies_id_at(&game->colonies, dest_x, dest_y);
    const ColonizeColony* col = colonies_get(&game->colonies, cid);
    if (col && col->active && col->nation_id >= 0 && col->nation_id <= 3 &&
        col->nation_id != selected->nation_id) {
      bool pending = game->ai_popups.open &&
                     game->ai_popups.current.tag == AI_POPUP_TAG_COLONY_ATTACK &&
                     game->ai_popups.current.nation_a == sid;
      for (int qi = 0; !pending && qi < game->ai_popups.queue_count; ++qi) {
        pending = game->ai_popups.queue[qi].tag == AI_POPUP_TAG_COLONY_ATTACK &&
                  game->ai_popups.queue[qi].nation_a == sid;
      }
      if (pending) {
        set_status(game, "Attack?", NULL);
        return GAME_MOVE_RETURN_TRUE;
      }
      /* Port-authored confirm (bugs.md #437): no shipped DOS dialog covers a
       * Euro-colony attack the way @WHACKINDIANS covers a village, so this
       * wording is deliberately not the catalog's "Shall we attack the
       * {%STRING0}, Your Excellency?" phrasing. */
      char body[AI_POPUP_BODY_LEN];
      snprintf(
        body, sizeof(body), "Order troops against %s?",
        col->name[0] ? col->name : "the enemy colony"
      );
      static const char* labels[] = {"Hold position.", "Attack!"};
      static const int ids[] = {2, 1};
      const int payload = dest_x | (dest_y << 8);
      if (ai_popup_enqueue_choice_ctx(
            &game->ai_popups, AI_POPUP_TAG_COLONY_ATTACK, sid, col->nation_id, payload, NULL,
            body, labels, ids, 2
          )) {
        (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_COLONY_ATTACK);
        set_status(game, "Attack?", NULL);
        return GAME_MOVE_RETURN_TRUE;
      }
    }
  }
  /*
   * FUN_465b_0000 Euro peer at peace (bugs.md): DOS never refuses the attack —
   * a signed treaty asks @HAVETREATY first, no treaty just opens hostilities.
   * Covers a foreign unit on the tile and an (even undefended) foreign colony.
   */
  if (game->col1_ok && combat_unit_is_combat_role(&game->units, sid)) {
    int target_nation = -1;
    const int foe_id = units_id_at(&game->units, dest_x, dest_y);
    const ColonizeUnit* foe = foe_id >= 0 ? units_get_const(&game->units, foe_id) : NULL;
    if (foe && foe->nation_id >= 0 && foe->nation_id <= 3 &&
        foe->nation_id != selected->nation_id) {
      target_nation = foe->nation_id;
    } else if (!foe && !units_is_sea(&game->units, sid)) {
      const int cid = colonies_id_at(&game->colonies, dest_x, dest_y);
      const ColonizeColony* col = colonies_get(&game->colonies, cid);
      if (col && col->active && col->nation_id >= 0 && col->nation_id <= 3 &&
          col->nation_id != selected->nation_id) {
        target_nation = col->nation_id;
      }
    }
    if (target_nation >= 0 &&
        !ai_diplo_at_war(&game->col1, selected->nation_id, target_nation)) {
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      if (ai_contact_try_euro_attack_confirm(
            &ctx, selected->nation_id, target_nation, sid, dest_x, dest_y
          )) {
        set_status(game, "Attack?", NULL);
        return GAME_MOVE_RETURN_TRUE;
      }
    }
  }

  /*
   * FUN_5fef_1b0e @HALF: the attacker has less than one whole movement point
   * left and would fight at remaining/3 strength — DOS asks before committing
   * (bugs.md). Last of the pre-move confirms so the treaty / whack questions
   * are settled first, exactly the order 1b0e runs them in.
   */
  if (game->col1_ok && combat_unit_is_combat_role(&game->units, sid)) {
    const int foe_id = units_id_at(&game->units, dest_x, dest_y);
    const ColonizeUnit* foe = foe_id >= 0 ? units_get_const(&game->units, foe_id) : NULL;
    bool attacking = foe && foe->nation_id != selected->nation_id &&
                     units_is_sea(&game->units, sid) == units_is_sea(&game->units, foe_id);
    if (!foe && !units_is_sea(&game->units, sid)) {
      /* Undefended foreign colony: still an attack (the capture entry). */
      const int cid = colonies_id_at(&game->colonies, dest_x, dest_y);
      const ColonizeColony* col = colonies_get(&game->colonies, cid);
      attacking = col && col->active && col->nation_id != selected->nation_id;
      /* bugs.md #940: an empty village grows its temporary Brave only inside
       * units_try_move_w, after this pre-move @HALF gate. Treat the village
       * record itself as the defender so a 1/3 or 2/3 strength attack still
       * asks for confirmation. */
      if (!attacking && game->col1.tribe &&
          col1_save_village_at(&game->col1, dest_x, dest_y) != NULL) {
        attacking = true;
      }
    }
    if (attacking &&
        !(game->tired_ok_unit == sid &&
          game->tired_ok_payload == (dest_x | (dest_y << 8)))) {
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      if (ai_contact_try_tired_attack_confirm(&ctx, sid, dest_x, dest_y)) {
        /* GAME.TXT @HALF popup is enqueued by ai_contact.c; this status-bar
         * echo is port-authored and deliberately not that catalog wording. */
        set_status(game, "Confirm the attack?", NULL);
        return GAME_MOVE_RETURN_TRUE;
      }
    }
  }
  return GAME_MOVE_CONTINUE;
}

/* The move itself (units_try_move) and its immediate outcomes. */
COLONIZE_INTERNAL GameMoveStep game_move_commit(
  ColonizeGameState* game, ColonizeUnit* selected, int sid, const ColonizeColonyPool* colonies,
  int dest_x, int dest_y
) {
  {
    const int mp_before = selected->moves;
    if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(&game->world_map), .rng=(ColonizeDosRng*)(&game->move_rng), .rng_reseed=game->ai_rng_seed, .rng_reseed_set=true}, sid, dest_x, dest_y)) {
      if (units_last_combat_outcome() < 0) {
        set_status(game, "Combat lost", NULL);
        game_after_unit_action(game);
      } else if (selected->moves < mp_before) {
        /* DOS charges MP on failed partial-overspend rolls even when the unit stays. */
        set_status(game, "Move failed", NULL);
        game_after_unit_action(game);
      } else {
        game_report_enter_reason(game, sid, units_last_enter_reason());
      }
      return GAME_MOVE_RETURN_FALSE;
    }
    if (units_last_enter_reason() == COLONIZE_ENTER_BOARD) {
      set_status(game, "Boarded ship", NULL);
      game_after_unit_action(game);
      return GAME_MOVE_RETURN_TRUE;
    }
    if (units_last_combat_outcome() > 0) {
      snprintf(game->status, sizeof(game->status), "Combat won at (%d,%d)", dest_x, dest_y);
      game_after_unit_action(game);
      return GAME_MOVE_RETURN_TRUE;
    }
  }
  return GAME_MOVE_CONTINUE;
}

bool game_try_unit_move(ColonizeGameState* game, int dest_x, int dest_y) {
  if (!game || !game->units_ok || !game->world_map_ok) {
    return false;
  }
  const int sid = game->units.selected_id;
  ColonizeUnit* selected = units_get(&game->units, sid);
  if (!selected || selected->moves <= 0) {
    return false;
  }
  /* FF + native settlement fallout for human combat (same as turn_refresh). */
  units_set_ff_col1(game->col1_ok ? &game->col1 : NULL);
      colonies_set_col1_context(game->col1_ok ? &game->col1 : NULL);
      europe_set_live_save(game->col1_ok ? &game->col1 : NULL);
  units_set_combat_human_nation(game->human_nation);
  units_set_combat_popups(&game->ai_popups, &game->messages);
  units_set_combat_europe(&game->europe);
  units_set_occupancy_map(&game->world_map);
  colonies_set_occupancy_map(&game->world_map);
  units_set_native_fallout_context(
    game->col1_ok ? &game->col1 : NULL, &game->world_map, -1
  );
  const ColonizeColonyPool* colonies = &game->colonies;
  GameMoveStep step;

  step = game_move_passenger_unload(game, selected, sid, colonies, dest_x, dest_y);
  if (step != GAME_MOVE_CONTINUE) {
    return step == GAME_MOVE_RETURN_TRUE;
  }

  step = game_move_sea_unit(game, selected, sid, colonies, dest_x, dest_y);
  if (step != GAME_MOVE_CONTINUE) {
    return step == GAME_MOVE_RETURN_TRUE;
  }

  step = game_move_native_prompts(game, selected, sid, dest_x, dest_y);
  if (step != GAME_MOVE_CONTINUE) {
    return step == GAME_MOVE_RETURN_TRUE;
  }

  step = game_move_colony_prompts(game, selected, sid, dest_x, dest_y);
  if (step != GAME_MOVE_CONTINUE) {
    return step == GAME_MOVE_RETURN_TRUE;
  }

  step = game_move_commit(game, selected, sid, colonies, dest_x, dest_y);
  if (step != GAME_MOVE_CONTINUE) {
    return step == GAME_MOVE_RETURN_TRUE;
  }

  if (selected->type_index >= 0 && selected->type_index < game->units.type_count &&
      units_type_kind(&game->units.types[selected->type_index]) == UNITS_KIND_WAGON) {
    /* bugs.md #440: the wheels (COLDIG 12, event 0x52) roll only when the
     * wagon ARRIVES at a colony, not on every overland step. */
    const int arrive_cid = colonies_id_at(&game->colonies, selected->x, selected->y);
    const ColonizeColony* arrive_col = colonies_get(&game->colonies, arrive_cid);
    if (arrive_col && arrive_col->active) {
      sound_play(0x52);
    }
  }
  /*
   * FUN_465b_0000 tail (viceroy_unpacked.c ~75800): a Treasure Train (type
   * 0xa) of a human-controlled nation entering a colony tile whose record
   * has the coastal bit (+0x1c & 0x40) fires FUN_5fef_1908 — the King's
   * Galleon offer — for the unit that just moved, and nowhere else: this
   * is DOS's only trigger (the end-of-turn sweep turn.c used to run was a
   * port invention). All remaining gates live in the callee.
   */
  if (game->col1_ok && selected->active && selected->nation_id == game->human_nation &&
      units_type_is_treasure(units_type(&game->units, selected->type_index))) {
    {
      (void)units_king_galleon_offer_for_unit_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .col1=(ColonizeCol1Save*)(&game->col1), .col1_ok=true, .europe=(EuropeScreen*)(game->europe_ok ? &game->europe : NULL)}, selected->nation_id, selected->id, &game->ai_popups, &game->messages);
    }
  }
  snprintf(game->status, sizeof(game->status), "Moved unit to (%d,%d)", dest_x, dest_y);
  game_after_unit_action(game);
  return true;
}

/*
 * Issue a Go To for `uid`. DOS has no separate go-to mover: order 3's tick
 * commits every step through the ordinary move routine (FUN_465b), so a
 * destination one tile away is simply an arrow-key move — landfall off a
 * ship, the village @ACTIONS menu, the @SAILHOME lane prompt and combat all
 * behave exactly as they do for a keyboard step, and no lasting order is
 * left behind. Only a unit with no moves left keeps the order and walks it
 * next turn.
 *
 * Returns 1 = moved now (caller must leave the move's own status alone),
 * 0 = order set, -1 = refused.
 */
int game_issue_goto(ColonizeGameState* game, int uid, int dest_x, int dest_y) {
  if (!game || !game->units_ok || !game->world_map_ok) {
    return -1;
  }
  const ColonizeUnit* u = units_get_const(&game->units, uid);
  if (!u || !u->active) {
    return -1;
  }
  const int dx = dest_x - u->x;
  const int dy = dest_y - u->y;
  /*
   * One exception: DOS's 4720 lane check reads the order byte (2/3 = sail
   * intent), and a Go To onto the lane has already stamped order 3 before
   * the tick steps in — so that ship sails without the @SAILHOME question a
   * bare arrow-key step gets. Keep it on the order path.
   */
  const bool lane_sail = game->europe_ok && units_is_sea(&game->units, uid) &&
    map_tile_is_high_seas(&game->world_map, dest_x, dest_y);
  if (!lane_sail && u->moves > 0 &&
      dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1 && (dx != 0 || dy != 0)) {
    game->units.selected_id = uid;
    return game_try_unit_move(game, dest_x, dest_y) ? 1 : -1;
  }
  return units_set_goto_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, uid, dest_x, dest_y)
    ? 0
    : -1;
}

/*
 * FUN_281f_07a0 for one of the human's / any Euro unit plus the human-only
 * chrome that hangs off it: @LANDHO naming and the DISCOVERY OF THE PACIFIC
 * OCEAN woodcut (FUN_13f1_0158 DS:0x1e8 arm → FUN_12fd_006c(6), once per
 * game through the event bit).
 */
void game_reveal_sight_for_unit(ColonizeGameState* game, const ColonizeUnit* u) {
  if (!game || !u || !game->world_map_ok || !units_is_on_map(u)) {
    return;
  }
  const bool pacific = units_reveal_sight_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL)}, u);
  if (!game->col1_ok || u->nation_id != game->human_nation) {
    return;
  }
  game_try_prompt_landho(game);
  if (pacific) {
    /* FUN_13f1_0158's DS:0x1e8 arm calls FUN_12fd_006c(6) directly — the
     * woodcut is the whole notification, there is no popup behind it. */
    (void)woodcut_fire(&game->col1, WOODCUT_DISCOVERY_OF_THE_PACIFIC_OCEAN);
  }
}

/* After spending moves: keep unit, advance to next with moves, or tile-select. */
void game_after_unit_action(ColonizeGameState* game) {
  if (!game || !game->units_ok) {
    return;
  }
  /* bugs.md #285: any unit action ends the manual-pan hold — from here the
   * view follows the acting unit again. */
  game->view_pan_hold_unit = -1;
  ColonizeUnit* u = units_get(&game->units, game->units.selected_id);
  if (!u || !u->active) {
    game_select_tile(game, game->map_cursor_x, game->map_cursor_y);
    return;
  }
  if (game->world_map_ok && u->nation_id >= 0 && u->nation_id <= 3 && units_is_on_map(u)) {
    game_reveal_sight_for_unit(game, u);
  }
  /* LCR: Scout on rumour clears + rolls a manual outcome (Fountain of Youth,
   * Cibola, treasure, burial mounds, …); de Soto keeps outcomes positive. */
  if (game->world_map_ok && units_is_on_map(u) && map_tile_has_rumour(&game->world_map, u->x, u->y)) {
    (void)units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .map=(ColonizeWorldMap*)(&game->world_map), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL), .rng=(ColonizeDosRng*)(&game->move_rng), .europe=(EuropeScreen*)(game->europe_ok ? &game->europe : NULL)}, u->id, game->human_nation);
  }
  /*
   * First contact / village friction: land units only. Natives do not hail
   * ships (DOS meet gates ocean tiles; FUN_5bfb_022e after landfall).
   */
  if (game->col1_ok && u->nation_id >= 0 && u->nation_id <= 3 && units_is_on_map(u) &&
      !units_is_sea(&game->units, u->id)) {
    char contact[80];
    contact[0] = '\0';
    int first_indian = -1;
    if (col1_contact_adjacent_tribe(
          &game->col1, u->x, u->y, u->nation_id, contact, sizeof(contact), &first_indian
        )) {
      if (first_indian >= 4) {
        ColonizeTurnContext ctx;
        game_fill_turn_context(game, &ctx);
        (void)ai_contact_try_first_welcome(&ctx, u->nation_id, first_indian);
      } else if (contact[0]) {
        snprintf(game->status, sizeof(game->status), "%s", contact);
      }
    }
    /* FUN_5bfb_3180: adjacent Brave or tribe-owned land also opens contact. */
    {
      ColonizeTurnContext sctx;
      game_fill_turn_context(game, &sctx);
      (void)ai_contact_encounter_scan(&sctx, u->nation_id, u->x, u->y);
    }
    /*
     * Already-met village Meet is enqueued from adjacent step (no enter).
     * Exact-tile Meet kept only if a unit somehow stands on the dwelling.
     */
    if (game->col1.tribe) {
      for (uint16_t ti = 0; ti < game->col1.head.tribe_count; ++ti) {
        const ColonizeCol1Tribe* t = &game->col1.tribe[ti];
        if ((int)t->x != u->x || (int)t->y != u->y) {
          continue;
        }
        if (t->nation_id < 4 || t->nation_id > 11) {
          continue;
        }
        ColonizeTurnContext ctx;
        game_fill_turn_context(game, &ctx);
        if (ai_contact_try_village_meet_unit_at(
              &ctx,
              u->nation_id,
              (int)t->nation_id,
              u->profession == UNITS_JOB_MISSIONARY,
              t->state.capital,
              u->id,
              (int)ti
            )) {
          break;
        }
      }
    }
  }
  /*
   * FUN_5bfb_3180 Euro x Euro branch (raw 98457-98713, 5bfb:3180): for each
   * of the 8 neighbours, the "other" nation is the owner of the unit on that
   * tile (FUN_281f_07e0) or else the settlement owner (FUN_281f_06be) — so an
   * ungarrisoned foreign colony counts too. The branch only runs when BOTH
   * the mover's tile and the neighbour tile are land (FUN_281f_0768 == 0,
   * local_36 / raw 98614) — a ship berthed in a colony qualifies (that is
   * the @HELLOAHOY case), a ship at sea never does. Euro target + WoI clear
   * (DS:0x5382 bit0) -> FUN_5bfb_153e via thunk_FUN_2a1f_05fc; a nonzero
   * return stamps MET (0x20) both ways (bugs.md #545).
   */
  if (game->col1_ok && game->world_map_ok && u->nation_id >= 0 && u->nation_id <= 3 &&
      units_is_on_map(u) && !game->col1.head.game_options.woi &&
      !map_tile_is_water(&game->world_map, u->x, u->y)) {
    for (int d = 0; d < 8; ++d) {
      const int nx = u->x + MAP_DIR8_DX[d];
      const int ny = u->y + MAP_DIR8_DY[d];
      if (map_tile_is_water(&game->world_map, nx, ny)) {
        continue;
      }
      int other = -1;
      const int oid = units_id_at(&game->units, nx, ny);
      const ColonizeUnit* o = oid >= 0 ? units_get_const(&game->units, oid) : NULL;
      if (o && o->active) {
        other = o->nation_id;
      } else if (game->colonies_ok) {
        const int ocid = colonies_id_at(&game->colonies, nx, ny);
        const ColonizeColony* oc = ocid >= 0 ? colonies_get(&game->colonies, ocid) : NULL;
        if (oc && oc->active) {
          other = oc->nation_id;
        }
      }
      if (other < 0 || other > 3 || other == u->nation_id) {
        continue;
      }
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      if (ai_diplo_153e_encounter(&ctx, u->nation_id, other, u->id)) {
        break;
      }
    }
  }
  game->map_cursor_x = u->x;
  game->map_cursor_y = u->y;
  game_set_view_center(game, u->x, u->y);
  if (u->moves > 0) {
    return;
  }
  /* The reveal / LCR / first-contact work above can raise a popup (@LANDHO
   * naming, @INDIANWELCOME, a rumour outcome). DOS would be sitting inside
   * that dialog right now, so nothing may advance past it here either —
   * park the hand-off and let game_update replay it once it is answered. */
  if (game_defer_turn_flow(game)) {
    return;
  }
  const int exhausted_x = u->x;
  const int exhausted_y = u->y;
  /* Skip-loop hand-off, same as every other site (game_wait_next_unit, ~Move
   * Pieces, the per-frame activation cycle): a bare turn_select_next_unit
   * here could park the selection on a Fortified/Sentried unit for a frame. */
  if (game_select_next_unit_awaiting_orders(game)) {
    game->view_pieces_mode = false;
    game_center_on_selected_unit(game);
    const ColonizeUnit* next = units_get_const(&game->units, game->units.selected_id);
    snprintf(
      game->status, sizeof(game->status), "Selected %s",
      next ? units_display_name(&game->units, next) : "unit"
    );
    return;
  }
  game_select_tile(game, exhausted_x, exhausted_y);
  /* Not a plain moves>0 exhaustion test (turn.c once exported one): a
   * colony full of Fortified units (never offered for selection) reports
   * "not exhausted" forever and the auto-end never fires — see
   * game_units_pending_orders, which every other site already uses. */
  if (!turn_option_end_of_turn(game->col1_ok ? &game->col1 : NULL, game->col1_ok) &&
      !game_units_pending_orders(game)) {
    game_do_end_turn(game);
  } else {
    snprintf(
      game->status,
      sizeof(game->status),
      "%s",
      /* LABELS.TXT @MISC row 2 — the same row the sidebar EOT prompt uses. */
      game_labels_misc_or(2, "")
    );
  }
}

bool game_key_move_delta(ColonizeKey key, int* out_dx, int* out_dy) {
  int dx = 0;
  int dy = 0;
  switch (key) {
    case COLONIZE_KEY_UP:
    case COLONIZE_KEY_KP8:
      dy = -1;
      break;
    case COLONIZE_KEY_DOWN:
    case COLONIZE_KEY_KP2:
      dy = 1;
      break;
    case COLONIZE_KEY_LEFT:
    case COLONIZE_KEY_KP4:
      dx = -1;
      break;
    case COLONIZE_KEY_RIGHT:
    case COLONIZE_KEY_KP6:
      dx = 1;
      break;
    case COLONIZE_KEY_KP7:
      dx = -1;
      dy = -1;
      break;
    case COLONIZE_KEY_KP9:
      dx = 1;
      dy = -1;
      break;
    case COLONIZE_KEY_KP1:
      dx = -1;
      dy = 1;
      break;
    case COLONIZE_KEY_KP3:
      dx = 1;
      dy = 1;
      break;
    default:
      return false;
  }
  if (out_dx) {
    *out_dx = dx;
  }
  if (out_dy) {
    *out_dy = dy;
  }
  return true;
}
