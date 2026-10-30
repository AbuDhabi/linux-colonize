#include "core/internal.h"
#include "core/game_loop.h"

/*
 * Split out of game_loop.c (2026-09-23) — code moved verbatim.
 *
 * Sections:
 *  - Woodcuts/intro/popup queue servicing & main per-frame update
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
#include "core/screen_geom.h"
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

/* ===================== Woodcuts/intro/popup queue servicing & main per-frame update (game_service_woodcut .. game_update) ===================== */


/*
 * Milestone woodcut (FUN_12fd_006c) — full-screen, and ahead of whatever
 * popup the same event queues behind it: DOS pushes the woodcut first
 * (FUN_5bfb_022e runs woodcut 3/4/5 before the @INDIANWELCOME dialog,
 * FUN_5fef the raid woodcut before its @RAID text). The popup queue still
 * advances underneath — the woodcut only owns the screen and the keyboard
 * until it is dismissed. Returns true while a woodcut owns the frame.
 */
static bool game_service_woodcut(ColonizeGameState* game, const ColonizeInputState* input) {
  bool just_opened = false;
  /*
   * Park exactly like game_service_bar_message: anything over the map owns
   * the display, and game_render draws an open woodcut over EVERY screen
   * (the `woodcut.open` early-out above the colony/Europe/report/pedia
   * branches), so a woodcut queued mid-EOT — turn.c's colony milestones,
   * ai_contact's first-contact 3/4/5 — used to seize a screen the player was
   * reading. Only `in_menu` was parked before. The pending id stays queued
   * and is picked up when the map is back (popup-blocking invariant,
   * port_bugs.md #262). Modals/popups are deliberately NOT in this set: DOS pushes
   * the woodcut ahead of the dialog the same event queues behind it.
   */
  const bool screen_over_map =
    game_screen_owns_display(game) || new_game_active(&game->new_game);
  if (!game->woodcut.open && !game->declaration.open && !game->opening.open &&
      !game->closing.open &&
      !screen_over_map &&
      woodcut_has_pending()) {
    const int wid = woodcut_take_pending();
    just_opened = wid >= 0 &&
      woodcut_open(&game->woodcut, game->resolved_data_dir, wid);
    if (just_opened) {
      diag_info("Woodcut %d: %s", wid, game->woodcut.caption);
    }
  }
  if (game->woodcut.open) {
    /* Not on the opening frame: a key still down from the action that armed
     * it would dismiss the screen before it is ever seen. */
    if (!just_opened) {
      (void)woodcut_handle_input(&game->woodcut, input);
    }
    return true;
  }
  return false;
}

/*
 * DOS status line (port_bugs.md #369). FUN_1009_00b4 is a BLOCKING call: it holds
 * whatever was running until the line's dwell expires or the player presses
 * anything, then clears the strip and lets the next one through. Same shape
 * here — while a line is up, the map's top strip shows it instead of the
 * pull-down titles and the turn pipeline does not advance. Returns true while
 * the strip is owned.
 */
static bool game_service_bar_message(ColonizeGameState* game, const ColonizeInputState* input) {
  if (!game) {
    return false;
  }
  if (!ai_popup_bar_message(&game->ai_popups)) {
    map_menu_set_message(&game->map_menu, NULL, 0);
    return false;
  }
  /*
   * Anything over the map owns both the display and the keyboard. Park the
   * line — do not start or advance its dwell, and take the text back off the
   * strip — and let that screen have the frame; the queue is picked up again
   * when the map is back.
   */
  if (game_screen_owns_display(game) || game_modal_open(game)) {
    map_menu_set_message(&game->map_menu, NULL, 0);
    return false;
  }
  const bool dismiss = input && (input->last_key != COLONIZE_KEY_NONE ||
                                 input->mouse_left_clicked || input->mouse_right_clicked);
  (void)ai_popup_bar_service(&game->ai_popups, game->elapsed_ms, dismiss);
  map_menu_set_message(
    &game->map_menu,
    ai_popup_bar_message(&game->ai_popups),
    ai_popup_bar_message_color(&game->ai_popups)
  );
  return ai_popup_bar_message(&game->ai_popups) != NULL;
}

static bool game_service_closing(
  ColonizeGameState* game,
  const ColonizeInputState* input,
  uint32_t dt_ms
) {
  if (!game || !game->closing.open) {
    return false;
  }
  if (!game->closing_just_opened) {
    (void)closing_handle_input(&game->closing, input);
  }
  game->closing_just_opened = false;
  if (game->closing.open) {
    if (game->closing.finished) {
      closing_close(&game->closing);
    } else {
      closing_update(&game->closing, dt_ms);
    }
  }
  if (!game->closing.open && game->closing_then_score) {
    game->closing_then_score = false;
    game_open_retire_score(game);
  }
  return true;
}

static void game_finish_intro(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  opening_close(&game->opening);
  game->opening_just_opened = false;
  /* OPENING.EXE exits here in DOS, which also ends its 0x34 music. VICEROY
   * then opens the title menu on pool 5 — Natives once, then the general
   * pool (sound.h SOUND_TITLE_BGM_POOL). */
  sound_set_bgm(SOUND_TITLE_BGM_POOL);
  /* The demo's VICEROY ("viceroy -od") takes FUN_75c2_2778's DS:0x828 arm instead. */
  if (game->demo) {
    game_demo_start(game);
  }
}

bool game_try_start_intro(ColonizeGameState* game) {
  if (!game) {
    return false;
  }
  if (game->opening.open) {
    return true;
  }
  /* COLDEMO.BAT always starts at OPENING ("opening -d:0"); without its art the
   * demo goes straight to VICEROY's demo start. */
  if (game->demo) {
    sound_set_demo(true);
    if (!opening_open(&game->opening, game->resolved_data_dir)) {
      game_demo_start(game);
      return false;
    }
    game->opening_just_opened = true;
    return true;
  }
  if (!settings_is_loaded()) {
    return false;
  }
  if (settings_get()->skip_intro && !settings_first_run()) {
    return false;
  }
  if (!opening_open(&game->opening, game->resolved_data_dir)) {
    return false;
  }
  game->opening_just_opened = true;
  set_status(game, "Colonization", NULL);
  return true;
}

/*
 * Service the AI popup queue: an elected "Zoom to colony." whose message batch
 * has drained opens that colony screen before anything else (DOS
 * FUN_364b_0688 tail -> FUN_281f_0608 does the same mid-EOT); otherwise the
 * next queued popup presents. Returns true when a colony screen was opened and
 * the caller must hand the frame over.
 *
 * Audit GL-23: this was written out twice, in the end-of-turn branch and in the
 * idle branch, with different guard sets. The difference is deliberate and both
 * halves survive here:
 *  - the EOT caller only reaches this when no screen is over the map at all
 *    (port_bugs.md #262 — a colony zoom / Europe / report / pedia freezes the whole
 *    end-of-turn pipeline and control falls through to the idle path so that
 *    screen gets its input), so it passes zoom_blocked_by_screen = false;
 *  - the idle caller DOES present over in_colony / in_europe / in_menu, the
 *    three branches whose render calls ai_popup_render — that is what
 *    "queued popups still present on top of it" means — but must not open a
 *    second colony screen underneath one of them, so it sets
 *    zoom_blocked_by_screen. It also refuses to present at all over the silent
 *    branches (pedia / report / exploits / hall of fame / debug atlas), which
 *    is its own outer guard.
 */
static bool game_service_popup_queue(ColonizeGameState* game, bool zoom_blocked_by_screen) {
  if (game->ai_popups.open || game->ai_popups.has_result) {
    return false;
  }
  if (!game_modal_open(game) && !zoom_blocked_by_screen) {
    const int zoom_cid = ai_popup_take_colony_zoom(&game->ai_popups);
    if (zoom_cid >= 0) {
      const ColonizeColony* zc = colonies_get(&game->colonies, zoom_cid);
      if (zc && zc->active) {
        game->colony_zoom_popup_hold = true;
        game_enter_colony(game, zoom_cid);
        return true;
      }
    }
  }
  ai_popup_try_present_next(&game->ai_popups);
  return false;
}

/* @HOWMUCH1 amount entry for loading `cargo` from the colony warehouse onto
 * the selected transport; a no-op when the stock is empty (keyboard '+'
 * guards that itself; the drag path relied on the same check). */
void game_colony_open_load_prompt(
  ColonizeGameState* game, const ColonizeColony* colony, int cargo
) {
  if (!game || !colony || cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }
  const int max_amt = colony->stock[cargo] < 100 ? colony->stock[cargo] : 100;
  if (max_amt <= 0) {
    return;
  }
  char prompt[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = (game->europe_ok && cargo < game->europe.cargo_count)
                  ? game->europe.cargo[cargo].name
                  : "";
  const ColonizeUnit* transport =
    units_get_const(&game->units, game->colony_screen.transport_unit_id);
  tok.string1 = transport ? units_display_name(&game->units, transport) : "";
  tok.number0 = max_amt;
  tok.has_number0 = true;
  popup_msg_fill(
    &game->messages, "HOWMUCH1", &tok, "", prompt, sizeof(prompt)
  );
  howmuch_open(
    &game->howmuch,
    HOWMUCH_KIND_LOAD,
    prompt,
    howmuch_amount_label(&game->messages, "HOWMUCH1", ""),
    max_amt,
    max_amt,
    cargo,
    0
  );
}

/*
 * European Status buy prompt: the @HOWMUCH4 amount dialog for the selected
 * market cargo, sized by the selected ship's free room (DOS FUN_38fd_1fa2,
 * smell audit #83). Audit GL-14: key L and the '=' text key carried the same
 * thirty lines twice. Both call sites have already checked that a ship is
 * selected.
 */
void game_europe_open_buy_prompt_for(ColonizeGameState* game, int hidx, int cargo) {
  EuropeScreen* eu = &game->europe;
  if (game_europe_boycott_gate(game, cargo)) {
    return; /* FUN_38fd_2dfe: @KISSUP, then no amount prompt */
  }
  const int room = europe_harbor_cargo_room(eu, &game->units, hidx, cargo);
  const int max_amt = room < 100 ? room : 100;
  char prompt[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = (cargo >= 0 && cargo < eu->cargo_count) ? eu->cargo[cargo].name : "";
  tok.string1 = (hidx >= 0 && hidx < eu->harbor_ships) ? eu->harbor[hidx].name : "";
  tok.number0 = max_amt;
  tok.has_number0 = true;
  tok.number1 = europe_buy_price(eu, cargo);
  tok.has_number1 = true;
  popup_msg_fill(
    &game->messages, "HOWMUCH4", &tok, "", prompt, sizeof(prompt)
  );
  if (max_amt <= 0) {
    snprintf(eu->status, sizeof(eu->status), "%s", "No empty hold.");
  } else {
    howmuch_open(
      &game->howmuch,
      HOWMUCH_KIND_BUY,
      prompt,
      howmuch_amount_label(&game->messages, "HOWMUCH4", ""),
      max_amt,
      max_amt,
      cargo,
      0
    );
  }
}

void game_europe_open_buy_prompt(ColonizeGameState* game) {
  game_europe_open_buy_prompt_for(game, game->europe.selected_harbor, game->europe.selected_market);
}

/*
 * Keys P (Clear/Plow) and R (Build Road): the same pioneer-order handler with
 * a different units_pioneer_* entry point (audit GL-24). Returns true when the
 * order was issued; P's caller then falls through to its pedia category and
 * R's does not, which is the only other thing that differed between them.
 *
 * The screen guard is game_screen_owns_display (audit GL-30): the two copies
 * each listed their own subset of the flags. Every screen but the title menu
 * has already returned from game_update by the time these keys are read, so
 * the wider set is the same test with fewer ways to drift.
 */
typedef bool (*GamePioneerOrderFn)(
  const ColonizeWorld*,
  int,
  char*,
  size_t,
  AiPopupState*,
  const ColonizeMsgCatalog*
);

static bool game_key_pioneer_order(
  ColonizeGameState* game,
  GamePioneerOrderFn fn,
  GameIndianLandKind land_kind
) {
  if (game_screen_owns_display(game) || !game->world_map_ok || !game->units_ok) {
    return false;
  }
  const int sid = game->units.selected_id;
  const ColonizeUnit* su = units_get_const(&game->units, sid);
  if (!su || !units_is_pioneer(&game->units, sid) || su->moves <= 0) {
    return false;
  }
  /* Same command id as the Orders menu item, so the same encroachment CHOICE
   * (port_bugs.md #1174); the CHOICE answer issues the order itself. */
  if (game_pioneer_land_gate(game, land_kind)) {
    return true;
  }
  char msg[96];
  const ColonizeWorld w = game_world(game);
  fn(&w, sid, msg, sizeof(msg), &game->ai_popups, &game->messages);
  set_status(game, msg, NULL);
  return true;
}

static bool game_service_opening(
  ColonizeGameState* game,
  const ColonizeInputState* input,
  uint32_t dt_ms
) {
  if (!game || !game->opening.open) {
    return false;
  }
  if (!game->opening_just_opened) {
    (void)opening_handle_input(&game->opening, input);
  }
  game->opening_just_opened = false;
  if (game->opening.open) {
    if (game->opening.finished) {
      game_finish_intro(game);
    } else {
      opening_update(&game->opening, dt_ms);
    }
  }
  return true;
}

/* GameUpdateStep now lives in game_loop_internal.h. */

/*
 * Per-frame services: clocks, watches, turn-processor slices, popup /
 * woodcut / status-line pumps and the deferred Europe + declaration opens.
 */
COLONIZE_INTERNAL GameUpdateStep game_update_services(ColonizeGameState* game, const ColonizeInputState* input, uint32_t dt_ms) {
  game->elapsed_ms += dt_ms;
  game_track_screen(game);
  /* The port hosts OPENING/CLOSING in-process; DOS runs them as separate
   * programs. Their cue owns the driver there, so hold off VICEROY's pool
   * pump (which plays from the default pool even with no pool armed) until
   * the cinematic is gone. */
  sound_set_cinematic(game->opening.open || game->closing.open);
  sound_service();
  if (game_service_opening(game, input, dt_ms)) {
    return GAME_UPDATE_RETURN_TRUE;
  }
  game_water_cycle_tick(game);
  ai_popup_set_now_ms(game->elapsed_ms); /* King flair animation clock */
  /* port_bugs.md: while independence is declared, the Crown's borrowed nation
   * slot renders WHITE (REF), not the peer's own colour. */
  unit_chrome_set_crown_nation(
    game->col1_ok && ai_king_independence_declared(&game->col1)
      ? ai_king_crown_nation_col1(&game->col1, game->human_nation)
      : -1
  );
  /* port_bugs.md #1177a: FUN_112b_01ba swaps an AI nation's badge letter for its
   * plan byte when the cheat bit and Debug Info Flags DS:0x894 & 8 are set. */
  {
    unsigned plan_mask = 0;
    if (game_cheats_enabled(game) && (game->debug_flags_mask & 8u)) {
      for (int n = 0; n < 4; ++n) {
        if (game->col1.player[n].control != 0) {
          plan_mask |= 1u << n;
        }
      }
    }
    unit_chrome_set_plan_nations(plan_mask);
  }
  /* port_bugs.md: WoI colony flags — rebel colonies fly the American flag, crown
   * captures fly the player's original nation color. */
  unit_chrome_set_rebel_nation(
    game->col1_ok && ai_king_independence_declared(&game->col1) ? game->human_nation : -1
  );
  /* port_bugs.md #234: the move/combat watches were only armed during end-of-turn
   * processing, so the PLAYER's own moves fired into a NULL watch and never
   * animated. Arm them every frame; the callbacks guard platform/zoom
   * themselves. (The moving unit's overlay is blitted after game_render, so
   * it also draws on top of any stack it passes — port_bugs.md #227.) */
  units_set_move_watch(game_move_watch, game);
  units_set_combat_watch(game_combat_watch, game);
  units_set_combat_dissolve(game_combat_dissolve, game);
  units_set_combat_popup_pump(game_combat_popup_pump, game);
  ai_euro_set_watch(game_autoplay_watch, game);
  /* Same "arm it every frame" rule as the watches: founding and abandoning a
   * colony have to keep layer2's settlement bit current wherever they are
   * reached from (port_bugs.md — an abandoned colony kept hiding the units left
   * standing on its square). */
  colonies_set_occupancy_map(game->world_map_ok ? &game->world_map : NULL);
  /* Same rule for the colony module's Col1 context (port_bugs.md #1175c): it was bound
   * only by a unit move, the village attack and the turn processor, so a colony
   * founded first thing after a load / new game skipped the village-tile block in the
   * plot seating, the colony_counts[nation]++ of FUN_364b_1ba8 (raw 58030) and the
   * founder's rebel-divisor +100. */
  colonies_set_col1_context(game->col1_ok ? &game->col1 : NULL);
  /* Likewise the unit pool: colonies_abandon walks it for the trade-route cursor
   * (port_bugs.md #1179) and the unit homes (raw 58190-58200). */
  colonies_set_units_context(game->units_ok ? &game->units : NULL);

  /*
   * New-game wizard (difficulty → nation → … → sail). It owns the display
   * outright, so it is serviced ahead of every simulation/chrome service
   * below and returns: "New World" is reachable from the title menu with a
   * live campaign still loaded (Esc → title → New World), and the whole
   * pipeline underneath — end-of-turn slices, queued popups, the bar line,
   * a woodcut, the deferred Europe open, the goto pacer and the unit
   * activation cycle — must freeze while a screen is up (the popup-blocking
   * invariant, port_bugs.md #262). It is a screen, not a modal, so it joins the
   * screen tests (`screen_over_map`, `map_visible`, `game_screen_name`)
   * rather than `game_modal_open`, which drives `game_handle_modal_input`.
   */
  if (new_game_active(&game->new_game)) {
    game->new_game.ui_font = game_popup_font(game);
    game->new_game.tiny_font = game->colony_font_ok ? &game->colony_font
      : (game->menu_font_ok ? &game->menu_font : game->new_game.ui_font);
    game->new_game.lore_font = game->new_game.ui_font;
    game->new_game.wood_tile = game->menu_opentile_ok ? &game->menu_opentile : NULL;
    game->new_game.woodpanl = (game->pedia_wood_ok) ? &game->pedia_wood : NULL;
    game->new_game.labels_txt = game->labels_ok ? &game->labels : NULL;
    new_game_update(&game->new_game, dt_ms);
    if (new_game_wants_commit(&game->new_game)) {
      game_commit_new_campaign(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    new_game_handle_input(&game->new_game, input);
    if (new_game_wants_commit(&game->new_game)) {
      game_commit_new_campaign(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (!new_game_active(&game->new_game)) {
      /* Cancelled back to title. */
      game->in_menu = true;
      set_status(game, "OpenCol", NULL);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }

  /* End-of-turn nation phases: advance one slice per frame; block other input.
   *
   * DOS popups are BLOCKING calls — a dialog queued by an earlier slice
   * (starvation, king audience, combat chrome, …) is answered before the
   * next slice of processing runs, not hoarded until FINISH. Present it
   * here and freeze the whole pipeline until the player deals with it;
   * only animations (water cycle, flair clocks above) keep running. */
  /* port_bugs.md #262: a screen over the map (colony zoom mid-EOT, Europe, a
   * report, the pedia) freezes the WHOLE end-of-turn pipeline — nothing may
   * advance while the player is occupied with it. Fall through so that
   * screen gets its input; the EOT branch resumes when it closes. Queued
   * popups still present on top of it ("popup batch notwithstanding"). */
  /* Zoom hold ends when the player really leaves the zoomed colony. A
   * terrain-pedia article opened from its minimap temporarily clears
   * in_colony, but carries the colony id needed to restore that same screen;
   * keep the queued EOT batch held across that nested detour (port_bugs.md #950). */
  const bool pedia_returns_to_colony =
    game->in_pedia && game->pedia_return_colony_id >= 0 &&
    colonies_get(&game->colonies, game->pedia_return_colony_id) != NULL;
  if (game->colony_zoom_popup_hold && !game->in_colony && !pedia_returns_to_colony) {
    game->colony_zoom_popup_hold = false;
  }
  const bool screen_over_map =
    game_screen_owns_display(game) || new_game_active(&game->new_game);
  if (turn_processor_active(&game->turn_proc) && !screen_over_map) {
    /* Popup queue advances underneath an open woodcut (same order as the
     * post-EOT path below) — the woodcut owns screen and keyboard only. The
     * enclosing !screen_over_map is this path's screen guard (audit GL-23). */
    if (game_service_popup_queue(game, false)) {
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (game_service_woodcut(game, input)) {
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (game_service_closing(game, input, dt_ms)) {
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (game_modal_open(game) || ai_popup_busy(&game->ai_popups)) {
      (void)game_handle_modal_input(game, input);
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* Status line holds the pipeline exactly as DOS's FUN_1009_00b4 does. */
    if (game_service_bar_message(game, input)) {
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* Hot-seat: the outgoing power's popups are all answered by now. */
    if (game->turn_proc.handoff_pending) {
      game_hotseat_handoff(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    units_set_move_watch(game_move_watch, game);
    units_set_combat_watch(game_combat_watch, game);
    units_set_combat_dissolve(game_combat_dissolve, game);
  units_set_combat_popup_pump(game_combat_popup_pump, game);
    ColonizeTurnContext ctx;
    game_fill_turn_context(game, &ctx);
    if (!turn_processor_advance(&game->turn_proc, &ctx)) {
      game_finish_end_turn(game, &game->turn_proc.result);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }

  /* Present next queued AI popup once the turn processor is idle.
   * port_bugs.md: never while the Colonizopedia owns the screen — the pedia
   * branch draws no popup, so a queued dialog (the re-presented FF debate
   * behind the F1 detour) opened INVISIBLY and swallowed the keys the
   * player pressed to leave the article, silently answering the debate.
   *
   * Same hazard, same shape, for every OTHER screen whose render branch
   * jumps to render_log_sample without an ai_popup_render call: the exploits
   * painting, the Hall of Fame and the debug atlas (see the render branches
   * below). Exploits and the Hall of Fame are exactly the two screens the
   * retire chain parks the player on, so a popup queued behind the endgame
   * latch used to open invisibly on top of them. in_colony, in_europe and
   * in_menu are the three branches that DO call ai_popup_render, so they
   * stay out of this set; the new-game wizard is the remaining silent
   * branch and belongs to its own screen-ownership fix. */
  if (!game->in_pedia && !game->in_report && !game->in_exploits && !game->in_hall_of_fame &&
      !game->in_debug_atlas && !game->colony_zoom_popup_hold) {
    if (game_service_popup_queue(
          game, game->in_menu || game->in_europe || game->in_colony
        )) {
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* Woodcut ahead of the status line, the same order the end-of-turn branch
   * above uses (DOS pushes the woodcut first — FUN_5bfb_022e runs woodcut
   * 3/4/5 before @INDIANWELCOME). This path used to run them the other way
   * round, so an idle-path woodcut waited behind a bar line the EOT path
   * would have shown after it. */
  if (game_service_woodcut(game, input)) {
    return GAME_UPDATE_RETURN_TRUE;
  }

  /*
   * port_bugs.md #682: the second half of FUN_479b_076e's tail (raw 77045-77048).
   * The found flow arms woodcut 2 and then hands the colony id here; DOS's
   * FUN_281f_0524(2) is blocking, so FUN_281f_0608 runs only once the
   * woodcut has been dismissed. Nothing else may own the display either —
   * the same rule game_service_woodcut itself applies one line up.
   */
  if (game->found_open_colony_id >= 0) {
    const int found_cid = game->found_open_colony_id;
    if (!woodcut_has_pending() && !game->woodcut.open && !game_screen_owns_display(game) &&
        !new_game_active(&game->new_game) && !game_modal_open(game) &&
        !ai_popup_busy(&game->ai_popups)) {
      game->found_open_colony_id = -1;
      game->colony_screen.multi_mode = COLONY_MULTI_PRODUCTION; /* raw 77043: DS:0x337 = 0 */
      game_enter_colony(game, found_cid);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* Same status-line hold once the turn processor is idle (a line queued by
   * the last colony still has to be seen). */
  if (game_service_bar_message(game, input)) {
    return GAME_UPDATE_RETURN_TRUE;
  }

  /* port_bugs.md: an endgame latch must actually END the game. Whatever path
   * latched it (popup chain, zero-colony crush, a loaded save, the silent
   * turn.c C1 check), once every dialog is answered the retire-score chain
   * runs exactly once: score → (exploits) → Hall of Fame → menu/@SCORED.
   * A WON latch with scoring already complete (calendar_latch — the player
   * chose "Keep playing anyway.") stays quiet; a LOST one then ends the game
   * without a second chain (game_end_after_loss, DOS LAB_3844_04ec). */
  if (!game->war_end_retired && game->col1_ok && !ai_popup_busy(&game->ai_popups) &&
      !game_modal_open(game) && !turn_processor_active(&game->turn_proc) &&
      !game->in_menu && !game->in_report &&
      !game->in_hall_of_fame && !game->in_exploits) {
    const int endgame = ai_king_latch_get(&game->col1, AI_KING_ENDGAME_BYTE);
    if (endgame == AI_KING_ENDGAME_LOST) {
      game_end_after_loss(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (endgame == AI_KING_ENDGAME_WON && !game->col1.head.game_options.calendar_latch) {
      game_begin_win_closing_or_score(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* Deferred ship-arrival Europe open: once every end-of-turn popup has
   * been answered (port_bugs.md — the tax audience must not land ON the
   * European Status; DOS finishes the audiences first). */
  if (game->europe_ok && game->europe.open_on_dock && !game->in_europe && !game->in_menu &&
      !game->in_colony && !game->in_report && !game->in_pedia &&
      !new_game_active(&game->new_game) && !turn_processor_active(&game->turn_proc) &&
      !ai_popup_busy(&game->ai_popups) && !game_modal_open(game) &&
      !game_europe_blocked_by_woi(game)) {
    game->in_europe = true;
    game->europe.open_on_dock = false;
    sound_set_bgm(3);
    game_tutorial_europe_opened(game);
  }

  /*
   * DOS FUN_43f7_1a26 plays the signing cinematic (thunk_FUN_2a1f_009a →
   * FUN_43f7_160a) as part of the declaration itself, immediately before the
   * @INDEPENDENCE letter. ai_king queues that letter as a KING_LETTER popup,
   * so arm the cinematic the frame the popup is presented and let it draw on
   * top until it is dismissed.
   */
  bool declaration_just_opened = false;
  if (!game->declaration_played && game->ai_popups.open &&
      game->ai_popups.current.tag == AI_POPUP_TAG_KING_LETTER) {
    game->declaration_played = true;
    const int human = game->human_nation;
    /* port_bugs.md: the signature on the parchment is the LEADER's name (the
     * John Hancock moment), not the country — which had just been renamed
     * "United Colonies" and truncated to "United Colon" on the line. */
    char default_leader[NEW_GAME_LEADER_NAME_MAX];
    default_leader[0] = '\0';
    if (game->leader_name[0] == '\0' &&
        !(game->col1_ok && human >= 0 && human < 4 && game->col1.player[human].name[0])) {
      new_game_default_leader_name(
        game->names_ok ? &game->names : NULL, human, default_leader, sizeof(default_leader)
      );
    }
    const char* signer =
      (game->col1_ok && human >= 0 && human < 4 &&
       game->col1.player[human].name[0] != '\0')
        ? game->col1.player[human].name
        : (game->leader_name[0] ? game->leader_name : default_leader);
    declaration_just_opened =
      declaration_open(&game->declaration, game->resolved_data_dir, signer);
  }
  if (game->declaration.open) {
    /*
     * Runs ahead of the normal modal gate, so take input here too — but not
     * on the opening frame, where a key still held from the popup that armed
     * it would fast-forward the whole animation before it drew a stroke.
     */
    if (!declaration_just_opened) {
      (void)declaration_handle_input(&game->declaration, input);
    }
    if (game->declaration.open) {
      declaration_update(&game->declaration, dt_ms);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }

  if (game_service_closing(game, input, dt_ms)) {
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Turn activation queue: paces the selected unit's queued go-to order. */
/* The active unit's pending Go To: the village-entry guard, the goto-step
 * pacing clock and the step itself (port_bugs.md #418 routing). */
static GameUpdateStep game_pacer_goto_step(
  ColonizeGameState* game, ColonizeUnit* active, uint32_t dt_ms
) {
  /*
   * port_bugs.md #418: a Go To AIMED at an Indian settlement executes its final
   * step INTO the village. DOS routes every goto step through
   * FUN_465b_0000, so arriving next to the dwelling with the village as
   * the ordered destination raises exactly what an arrow-key step raises
   * (FUN_4d56_4528: woodcut 7 + the NAMES.TXT @ACTIONS menu) — the order was a move command into the village.
   * Earlier this branch stopped one tile short and handed control back
   * (port_bugs.md #287), which left the player to repeat the step by hand.
   *
   * The order is spent either way: a peaceful meet is conducted from the
   * adjacent tile (the unit never stands ON the village), so the goto is
   * cleared BEFORE dispatching — otherwise the pacer would re-fire the
   * @ACTIONS menu on every frame the unit sat next to the village.
   */
  const ColonizeWorld w = game_world(game);
  if (units_goto_dest_is_village_entry_w(&w, active->id)) {
    const int aid = active->id;
    const int gx = active->goto_x;
    const int gy = active->goto_y;
    units_clear_orders(&game->units, aid);
    game->units.selected_id = aid;
    (void)game_try_unit_move(game, gx, gy);
    game_center_on_selected_unit(game);
    return GAME_UPDATE_RETURN_TRUE;
  }
  /*
   * Smell #105: DOS routes every goto step through FUN_465b_0000, so a
   * Go To ENDING on a Euro peer's tile — a defending unit or an even
   * empty foreign colony — gets the same @WHACKINDIANS / @HAVETREATY /
   * @HALF confirms as an arrow-key move (prompt at viceroy 75545).
   * When the next step is the destination and it is hostile, hand the
   * step to the manual-move handler instead of the silent pacer.
   */
  if (active->orders == UNITS_ORDER_GOTO && active->goto_x < UNITS_GOTO_NONE &&
      active->goto_y < UNITS_GOTO_NONE &&
      map_tiles_adjacent(active->x, active->y, active->goto_x, active->goto_y, false) &&
      active->moves > 0) {
    const int gx = active->goto_x;
    const int gy = active->goto_y;
    const int foe_id = units_id_at(&game->units, gx, gy);
    const ColonizeUnit* foe =
      foe_id >= 0 ? units_get_const(&game->units, foe_id) : NULL;
    bool hostile = foe && foe->nation_id != active->nation_id;
    if (!hostile && !units_is_sea(&game->units, active->id)) {
      const int cid = colonies_id_at(&game->colonies, gx, gy);
      const ColonizeColony* col = colonies_get(&game->colonies, cid);
      hostile = col && col->active && col->nation_id != active->nation_id;
    }
    if (hostile) {
      const int aid = active->id;
      game->units.selected_id = aid;
      (void)game_try_unit_move(game, gx, gy);
      ColonizeUnit* after = units_get(&game->units, aid);
      /*
       * port_bugs.md #469: a refusal that DOS shows as a blocking dialog
       * (@CANNOTATTACK / @SHIPCOMBAT / @SHIPLAKE / @LANDFIRST) leaves the
       * INFO popup queued, and the old "no popup pending" guard then kept
       * the Go To armed — the pacer re-fired the same dialog every frame
       * and the player could never proceed. Those reasons are final for
       * the order: park and clear regardless of the popup.
       */
      const ColonizeEnterReason why = units_last_enter_reason();
      const bool dialog_refusal =
        after && after->active && (after->x != gx || after->y != gy) &&
        (why == COLONIZE_ENTER_BOUNCE_FOREIGN || why == COLONIZE_ENTER_LAKE_BLOCKED ||
         why == COLONIZE_ENTER_LANDFIRST || why == COLONIZE_ENTER_BLOCKED_DOMAIN);
      if (dialog_refusal && units_orders_follow_goto(after->orders)) {
        after->moves = 0;
        units_clear_orders(&game->units, aid);
      } else if (after && after->active && units_orders_follow_goto(after->orders) &&
          !game->ai_popups.open && game->ai_popups.queue_count == 0) {
        if (after->x == gx && after->y == gy) {
          /* Arrived (capture / entry went through): plain arrival. */
          units_clear_orders(&game->units, aid);
        } else {
          /* Refused without a prompt (cost, domain…): park like a
           * blocked step (port_bugs.md #363) so the pacer doesn't spin. */
          after->moves = 0;
          units_clear_orders(&game->units, aid);
        }
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
  }
  game->goto_step_accum_ms += dt_ms;
  const uint32_t goto_step_ms =
    (game->col1_ok && game->col1.head.game_options.fast_piece_slide) ? 80u : 100u;
  if (game->goto_step_accum_ms >= goto_step_ms) {
    game->goto_step_accum_ms -= goto_step_ms;
    if (game->goto_step_accum_ms > 200u) {
      game->goto_step_accum_ms = 0; /* drop backlog after hitch */
    }
    const int active_id = active->id;
    if (active->orders == UNITS_ORDER_TRADE_ROUTE) {
      game_trade_route_retarget(game, active);
      /* A Europe stop just sailed the ship off the map (despawned into
       * the Atlantic lane) — hand the queue to the next unit. */
      active = units_get(&game->units, active_id);
      if (!active || !active->active) {
        if (game_select_next_unit_awaiting_orders(game)) {
          game->view_pieces_mode = false;
        }
        return GAME_UPDATE_RETURN_TRUE;
      }
    }
    /* Sea-lane destination: a ship whose Go To ends on a high-seas tile
     * sails for Europe when it reaches THAT tile (port_bugs.md). Manual
     * arrow-key steps onto the lane deliberately do NOT — only a queued
     * order does, same as DOS's `0x314c == 2/3` sail intent.
     *
     * Only the ordered destination counts, not the first lane tile the
     * path happens to cross: DOS's reason-5 sail prompt (FUN_4720_015c,
     * viceroy_unpacked.c 76047-76052) is suppressed outright while the
     * order byte +0x314c is 3 (Go To) or 2 (Trade Route), so a DOS ship
     * under orders crosses high-seas tiles without sailing; its only
     * order-3 Europe departure is the goto menu's own "Europe" entry
     * (destination 999 → FUN_2b5a_1dfc, viceroy_unpacked.c 42798-42819),
     * which sails immediately from wherever the ship stands. Arbitrary
     * map-tile Go To is a port extension, so the port convention is the
     * closest analogue: the destination tile is the sail intent. */
    const int goto_dest_x = (int)active->goto_x;
    const int goto_dest_y = (int)active->goto_y;
    const bool goto_ship_to_lane = active->orders == UNITS_ORDER_GOTO &&
      units_is_sea(&game->units, active_id) && game->europe_ok &&
      active->goto_x < UNITS_GOTO_NONE && active->goto_y < UNITS_GOTO_NONE &&
      map_tile_is_high_seas(&game->world_map, goto_dest_x, goto_dest_y);
    const ColonizeWorld w = game_world(game);
    const bool stepped = units_advance_goto_one_step_w(&w, active_id);
    ColonizeUnit* again = units_get(&game->units, active_id);
    const bool sailed_for_europe =
      goto_ship_to_lane && again && again->active && units_is_on_map(again) &&
      again->x == goto_dest_x && again->y == goto_dest_y;
    if (sailed_for_europe) {
      /* A refused crossing (War of Independence — the shared tail asks
       * @EUROPENOTLEAVE) leaves the ship standing on the lane tile it
       * was ordered to, so it still gets the ordinary arrival tail
       * instead of an unconditional hand-off. */
      if (game_ship_sail_to_europe(game, active_id)) {
        if (game_select_next_unit_awaiting_orders(game)) {
          game->view_pieces_mode = false;
        }
      } else if (stepped) {
        game_after_unit_action(game);
      }
    } else if (stepped) {
      /* FUN_465b_0000 tail runs inside the step, before the route logic. */
      game_move_arrival_tail(game, again);
      if (again && again->orders == UNITS_ORDER_TRADE_ROUTE) {
        game_trade_route_retarget(game, again);
      }
      /* game_after_unit_action already does fog reveal / Land Ho! /
       * LCR / first-contact / view centering / next-unit-when-
       * exhausted — the same tail a player-driven move gets. */
      game_after_unit_action(game);
    } else {
      /* Arrived (orders cleared), ran out of moves this step, or
       * genuinely stuck (blocked path) — none of those should freeze
       * the activation cycle here; move on to the next unit. Sync
       * view_pieces_mode either way: found → stay in Move Pieces;
       * none left → drop into View Pieces at the unit's last tile
       * (game_select_tile) instead of leaving a moves-exhausted
       * "ghost" selection this loop would otherwise re-poll forever. */
      /*
       * port_bugs.md #363 (trade-routed wagon sat in the control queue and
       * only moved after the turn ended): a unit whose order is STILL
       * a goto/trade route after a failed step did not arrive — it is
       * blocked (foreign unit or native village on the next tile) or
       * cannot afford any neighbouring step with its partial MP
       * (units_next_goto_step drops every candidate whose
       * units_move_cost it cannot pay, which only the full-allotment
       * bypass would have let through). Both stay false for the rest
       * of the turn, so leaving MP on the unit made
       * turn_select_next_unit keep handing it back — with no other
       * human unit left with moves it re-picked the same wagon every
       * frame, holding the cursor on a piece that never stepped.
       * Park it for the turn instead; next turn's full allotment
       * clears the affordability case and re-runs the block check.
       * Arrival is untouched (orders already cleared there), so a
       * finished Go To still hands the player its leftover MP.
       */
      ColonizeUnit* stalled = units_get(&game->units, active_id);
      if (stalled && stalled->active && units_orders_follow_goto(stalled->orders) &&
          stalled->moves > 0) {
        stalled->moves = 0;
      }
      if (game_select_next_unit_awaiting_orders(game)) {
        game->view_pieces_mode = false;
        const ColonizeUnit* next = units_get_const(&game->units, game->units.selected_id);
        if (next) {
          game->map_cursor_x = next->x;
          game->map_cursor_y = next->y;
          game_set_view_center(game, next->x, next->y);
          snprintf(
            game->status, sizeof(game->status), "Selected %s",
            units_display_name(&game->units, next)
          );
        }
      } else {
        const ColonizeUnit* stuck = units_get_const(&game->units, active_id);
        game_no_unit_left(
          game, stuck ? stuck->x : game->map_cursor_x, stuck ? stuck->y : game->map_cursor_y
        );
      }
    }
  }
  return GAME_UPDATE_CONTINUE;
}

COLONIZE_INTERNAL GameUpdateStep game_update_unit_pacer(ColonizeGameState* game, const ColonizeInputState* input, uint32_t dt_ms) {
  /*
   * Turn activation queue (minimal): DOS gives control to exactly one
   * human unit at a time, in a fixed deterministic order (turn_select_
   * next_unit's ascending-id cycle, already used by the manual Wait/Space
   * command) — not a background pacer that silently steps every unit with
   * a queued go-to simultaneously on wall-clock time. A unit with a
   * pending go-to/etc order auto-executes (still paced visually, one tile
   * per goto_step_ms, "Pace Go-To") only while it's the *selected* unit,
   * i.e. only once its turn in the cycle comes up; an idle unit (no
   * queued order) stops the cycle and waits for the player, matching
   * DOS's real "hand control to the next unit needing orders" flow.
   *
   * Player-reported (dutch-reports.SAV): loading mid-turn should hand
   * control to the Vlissingen wagon train first (it's idle — no orders),
   * then (once released) the New Amsterdam privateer (also idle), then
   * auto-run the pioneer near Vlissingen's queued order, then hand
   * control to the New Holland caravel — the Merchantman, already out of
   * moves for the turn, should just sit still (its go-to resumes next
   * turn, once turn.c's per-turn move refresh gives it moves again), not
   * be silently paced just because time passes with the map on screen.
   *
   * Only runs while the map is actually the thing on screen — an overlay
   * (report/menu/Europe/colony/pedia/debug atlas/Hall of Fame/exploits, the
   * new-game wizard, or any modal popup/dialog — game_modal_open: king tax,
   * ship-sunk, options, name entry,
   * etc.) covering it shouldn't let wall-clock time bleed into unit
   * movement at all (that's also why dt_ms isn't accumulated below while
   * covered, not just skipped — avoids a catch-up burst of steps the
   * instant the overlay closes). Matches DOS: everything but animations
   * pauses while a popup is up (port_bugs.md). AI/native
   * units and other European nations' units are untouched here: they
   * resolve their own goto orders exclusively inside turn_processor_
   * advance(), during their own turn (see turn_select_next_unit's own
   * human_nation filter).
   */
  /* The Hall of Fame and the exploits painting are screens, not modals, so
   * game_modal_open does not see them and they have to be listed here by
   * hand — exactly as they are in game_turn_flow_allowed, in both
   * screen_over_map sets and in the popup-presentation gate above. Both are
   * reachable with a live campaign still loaded and in_menu false (the title
   * menu opens the Hall of Fame directly, and game_retire_after_score parks
   * the player on either one), so without them the pacer went on stepping
   * goto units underneath: village-entry dispatch, ships sailing for Europe
   * and combat started by units_advance_goto_one_step all ran while the
   * player was reading a screen he can't act from. */
  const bool map_visible = !game_screen_owns_display(game) &&
    !new_game_active(&game->new_game) && !game_modal_open(game);
  /* No power at the controls: no unit is the player's (a resumed save's
   * selected unit would otherwise run its goto here, outside the AI). */
  const bool all_ai = game->col1_ok && col1_all_ai_fixed_nation(&game->col1) >= 0;
  /* divergences.pioneer_plan: Plan Pioneer lives inside View Pieces only, and
   * the plan forgets what the player can see is done or impossible. */
  if (!game->view_pieces_mode || game->units.selected_id >= 0) {
    game->pioneer_plan_view = false;
  }
  if (game->world_map_ok && pioneer_plan_enabled()) {
    const ColonizeWorld w = game_world(game);
    pioneer_plan_reconcile_seen(&w, game->human_nation);
  }
  if (game->units_ok && game->world_map_ok && map_visible && !all_ai) {
    ColonizeUnit* active =
      game->units.selected_id >= 0 ? units_get(&game->units, game->units.selected_id) : NULL;
    const bool active_on_map_with_moves = active && active->active &&
      active->nation_id == game->human_nation && units_is_on_map(active) && active->moves > 0;
    const bool active_pending =
      active_on_map_with_moves && units_orders_follow_goto(active->orders);
    /* A standing order (Fortified/Sentry/Clear-Forest.../Build-Road —
     * units_orders_skip_turn, the same discriminator turn.c's own
     * per-turn move refresh uses) doesn't need player attention either,
     * even though it isn't a goto. Without this, a mid-turn-loaded save
     * (whose Fortified/Sentried units haven't had this turn's refresh
     * zero their moves yet) demanded the player's input on every
     * garrisoned unit in turn — player-reported alongside the Merchantman
     * destination bug. */
    /*
     * port_bugs.md (loaded_soldier_bug.SAV): a passenger woken out of the tile-
     * stack popup is NOT on the map (it rides in the hold), so the
     * on-map-only test above rejected it and the very next frame's cycle
     * below silently grabbed the selection away — the pick "did nothing".
     * An awake passenger with moves is a unit awaiting the player's step
     * ashore.
     */
    const bool active_pax_awaiting = active && active->active &&
      active->nation_id == game->human_nation && active->aboard_ship_id >= 0 &&
      active->moves > 0 && !units_orders_skip_turn(active);
    const bool active_awaiting_player =
      (active_on_map_with_moves && !active_pending && !units_orders_skip_turn(active)) ||
      active_pax_awaiting;

    if (active_pending) {
      const GameUpdateStep sub = game_pacer_goto_step(game, active, dt_ms);
      if (sub != GAME_UPDATE_CONTINUE) {
        return sub;
      }
    } else if (active_awaiting_player && active->pioneer_auto) {
      /* Automate: start the job here or head for the next planned tile; the
       * next frame paces the Go To or hands on the spent unit. */
      const ColonizeWorld w = game_world(game);
      (void)pioneer_plan_auto_step(&w, active, &game->ai_popups, &game->messages);
    } else if (active_awaiting_player) {
      /* FUN_2b5a_3802 raw 46235-46236: Move Pieces hands the player a unit,
       * 0x97b0 = 1 (game_no_unit_left reads it; port_bugs.md #1302). The
       * pacer runs before this frame's input, as 3802 runs before its key. */
      if (!game->view_pieces_mode) {
        game->player_had_control = true;
      }
      /* port_bugs.md: the unit awaiting orders must be on screen — whatever path
       * selected it, recentre once it sits outside the current viewport. */
      int vc = 0;
      int vr = 0;
      game_map_zoom_view_size(game->map_zoom, &vc, &vr);
      int vx = 0;
      int vy = 0;
      map_panel_clamp_view_origin(
        (int)game->world_map.width,
        (int)game->world_map.height,
        game->map_view_x,
        game->map_view_y,
        vc,
        vr,
        &vx,
        &vy
      );
      if ((active->x < vx || active->y < vy || active->x >= vx + vc || active->y >= vy + vr) &&
          game->view_pan_hold_unit != active->id /* port_bugs.md #285 manual pan */) {
        game->map_cursor_x = active->x;
        game->map_cursor_y = active->y;
        game_set_view_center(game, active->x, active->y);
      }
    } else if (!active_awaiting_player && !game->view_pieces_mode) {
      /* No unit is currently both selected and idle-awaiting the player —
       * advance the cycle to find the next one that does
       * (game_select_next_unit_awaiting_orders loops internally, so a run of
       * several standing-order units in a row doesn't need one frame each to
       * skip past). Gated on !view_pieces_mode: once the player has
       * explicitly deselected to browse (V / right-click / VIEW ~View
       * Pieces), this must not silently grab control back next frame just
       * because no unit is currently selected — that's the whole point of
       * View Pieces (manual_gap / this file's Move-View spec). */
      const bool found_awaiting = game_select_next_unit_awaiting_orders(game);
      if (found_awaiting) {
        game->view_pieces_mode = false;
        /* This pick IS the parked hand-off (game_defer_turn_flow): replaying it
         * below would advance a second time and skip one awaiting unit (port_bugs.md
         * #1175a). DOS selects the next unit once, after the blocking message
         * inside the order body (FUN_2b5a_123e raw 42499-42512). The no-unit
         * path keeps the flag so the replay runs the end-of-turn tail. */
        game->turn_flow_deferred = false;
        const ColonizeUnit* next = units_get_const(&game->units, game->units.selected_id);
        game->map_cursor_x = next->x;
        game->map_cursor_y = next->y;
        game_set_view_center(game, next->x, next->y);
        snprintf(
          game->status, sizeof(game->status), "Selected %s",
          units_display_name(&game->units, next)
        );
      } else {
        /* FUN_2b5a_3b68 raw 46840-46848: the Move Pieces pass re-tests the
         * active unit and, with none left, runs 0e52's no-unit arm. */
        game_no_unit_left(game, game->map_cursor_x, game->map_cursor_y);
      }
    }
  }
  return GAME_UPDATE_CONTINUE;
}

/* Reports screen input. */
/* Leave the report screen from whatever page it is on, then run what follows
 * it: a new Founding Father's pedia entry, or the retire score chain. */
static void game_report_leave(ColonizeGameState* game) {
  game->in_report = false;
  diag_info("Left report screen.");
  if (game->ff_pedia_after_report >= 0) {
    const int ff_index = game->ff_pedia_after_report;
    game->ff_pedia_after_report = -1;
    game_open_pedia_article(game, PEDIA_CAT_FATHER, ff_index, false);
    return;
  }
  if (game->report_exits_to_menu) {
    game->report_exits_to_menu = false;
    /* DOS FUN_41f2_14a8 retire chain: score (F10 just closed) ->
     * exploits screen (0b70, only when a @SCORE tier qualifies and
     * scoring isn't already complete) -> Hall of Fame (0f56 insert +
     * present) -> title. */
    game_retire_after_score(game);
  }
}

COLONIZE_INTERNAL GameUpdateStep game_update_report_screen(ColonizeGameState* game, const ColonizeInputState* input) {
  if (game->in_report) {
    /* Labor report grid: a click on a profession cell zooms to its detail
     * view (golden: labor_detail.png) instead of anything OK/Esc-related. */
    if (game->report_id == COLONIZE_REPORT_LABOR && game->labor_detail_job < 0 &&
        input->mouse_left_clicked) {
      const int hit = reports_labor_cell_hit(input->mouse_x, input->mouse_y);
      if (hit >= 0) {
        game->labor_detail_job = hit;
        return GAME_UPDATE_RETURN_TRUE;
      }
    }
    /* Congress page 2 has no OK button (golden: full-bleed photo, no chrome)
     * — any click closes it, not just a hit on a drawn box. (The Score plate
     * has none either; game_update closes it on any key or click first.) */
    const bool page2_click_anywhere =
      game->report_id == COLONIZE_REPORT_CONGRESS && game->congress_page2 &&
      input->mouse_left_clicked;
    const bool ok_clicked = page2_click_anywhere ||
      (input->mouse_left_clicked &&
       reports_ok_button_hit(game->report_id, game->congress_page2, input->mouse_x, input->mouse_y));
    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_ENTER ||
        ok_clicked) {
      /* Continental Congress is two pages: closing page 1 (any means — Esc,
       * click, Enter) shows page 2 instead of leaving the report. */
      if (game->report_id == COLONIZE_REPORT_CONGRESS && !game->congress_page2) {
        game->congress_page2 = true;
        return GAME_UPDATE_RETURN_TRUE;
      }
      /* Labor detail view: closing it (Esc/Enter/OK) returns to the grid,
       * same as other reports' Esc — it doesn't leave the report yet. */
      if (game->report_id == COLONIZE_REPORT_LABOR && game->labor_detail_job >= 0) {
        game->labor_detail_job = -1;
        return GAME_UPDATE_RETURN_TRUE;
      }
      /* Economic report is European Trade + however many Cargo in Port
       * pages this nation's colony count needs — OK/Esc/Enter advances to
       * the next page, or leaves the report from the last one. */
      if (game->report_id == COLONIZE_REPORT_ECONOMIC) {
        const int page_count = reports_economic_page_count(
          game->col1_ok ? &game->col1 : NULL, game->human_nation
        );
        if (game->economic_page + 1 < page_count) {
          game->economic_page++;
          return GAME_UPDATE_RETURN_TRUE;
        }
        game->economic_page = 0;
      }
      /* Colony report is Military Garrisons + Sons of Liberty, each
       * paginated over this nation's colonies — same OK/Esc/Enter
       * page-advance shape as Economic above. */
      if (game->report_id == COLONIZE_REPORT_COLONY) {
        const int page_count = reports_colony_page_count(
          game->col1_ok ? &game->col1 : NULL, game->human_nation
        );
        if (game->colony_page + 1 < page_count) {
          game->colony_page++;
          return GAME_UPDATE_RETURN_TRUE;
        }
        game->colony_page = 0;
      }
      /* Naval report is a single paginated ship/passenger table — same
       * OK/Esc/Enter page-advance shape as Economic/Colony above. */
      if (game->report_id == COLONIZE_REPORT_NAVAL) {
        const ColonizeWorld w = game_world(game);
        const int page_count = reports_naval_page_count_w(&w, game->human_nation);
        if (game->naval_page + 1 < page_count) {
          game->naval_page++;
          return GAME_UPDATE_RETURN_TRUE;
        }
        game->naval_page = 0;
      }
      game_report_leave(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* F-keys switch reports (F2–F10) or open terrain pedia (F1). */
    if (input->last_key >= COLONIZE_KEY_F1 && input->last_key <= COLONIZE_KEY_F10) {
      game_handle_report_fkey(game, input->last_key);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* @RENAMECOLONY name entry. DOS opens it by clicking the title bar
 * (thunk_FUN_1000_9898, zone 10); the R key is a port extra. */
static void game_colony_open_rename(ColonizeGameState* game, const ColonizeColony* colony) {
  char prompt[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, "RENAMECOLONY", NULL, "", prompt, sizeof(prompt));
  if (name_entry_open(&game->name_entry, NAME_ENTRY_KIND_RENAME, prompt, colony->name, game->colony_view_id)) {
    name_entry_set_label(&game->name_entry, howmuch_amount_label(&game->messages, "RENAMECOLONY", ""));
  }
}

/* Colony screen input. */
/* FUN_2f2b_47bc raw 51373-51376: a press on an area-view plot moves the DS:0x32e focus and
 * the DS:0x330/0x332 cursor there. */
static void game_colony_focus_area(ColonyScreenView* csv, int tile_index) {
  csv->focus = COLONY_FOCUS_AREA;
  colonies_field_tile_delta(tile_index, &csv->area_dx, &csv->area_dy);
}

/* The terrain article of the area-view cell (dx, dy) off the colony tile, unless its
 * FUN_15eb_23f2 plot byte (the DS:0x8df0 cache) is 0x10 (off the map, unexplored):
 * FUN_281f_078c -> FUN_13e4_003a, FUN_291f_0428 (right-click raw 51399-51403, F1 raw
 * 55624-55628). The colony tile is never 0x10. */
static void game_colony_area_pedia(
  ColonizeGameState* game, const ColonizeColony* colony, int dx, int dy
) {
  const int ti = colonies_field_tile_index(dx, dy);
  if (ti >= 0) {
    const ColonizeWorld w = game_world(game);
    if (colonies_plot_blocked_mask(&w, colony, ti) == 0x10u) {
      return;
    }
  }
  int index = 0;
  if (game->world_map_ok) {
    index = map_pedia_terrain_index_at(&game->world_map, colony->x + dx, colony->y + dy);
  }
  const int cid = game->colony_view_id;
  game_open_pedia_article(game, PEDIA_CAT_TERRAIN, index, false);
  game->pedia_return_colony_id = cid;
  game_status_from_menu_row(game, "REPORTS", 1, game->status, sizeof(game->status));
}

/*
 * F1 on the bare colony screen, FUN_2f2b_6372 raw 55622-55650 (asm 2f2b:6408): the article
 * of what the DS:0x32e focus panel has selected, then the colony screen again
 * (thunk_FUN_291f_0684). Area view: the cursor cell's terrain. Colonists: the skill
 * article of the selected one's occupation, a fence unit's profession, 0x1c read as 0x13
 * (FUN_281f_0c54 -> FUN_15eb_0e52, FUN_291f_08de). Ships, only with DS:0x33c != 0: the
 * selected ship's unit article (FUN_281f_0b32 -> FUN_15eb_2f8e, FUN_291f_0942). Units pane,
 * only with DS:0x8d72 != 0: the selected unit's (FUN_281f_092a -> FUN_1427_0180). Warehouse:
 * the cargo article of DS:0x33a (FUN_291f_0934), static 0 before any press.
 */
static void game_colony_screen_help(
  ColonizeGameState* game, const ColonizeColony* colony, const ColonyScreenView* csv
) {
  PediaCategory cat = PEDIA_CAT_UNIT;
  int index = -1;
  const ColonizeUnit* u = NULL;
  switch (csv->focus) {
  case COLONY_FOCUS_AREA:
    game_colony_area_pedia(game, colony, csv->area_dx, csv->area_dy);
    return;
  case COLONY_FOCUS_COLONISTS:
    cat = PEDIA_CAT_JOB;
    if (csv->selected_colonist >= 0 && csv->selected_colonist < colony->colonist_count) {
      index = colonies_colonist_occupation_job(
        &game->colonies, &colony->colonists[csv->selected_colonist]
      );
    } else if (game->units_ok &&
               (u = units_get_const(&game->units, csv->selected_outside_unit)) != NULL) {
      index = u->profession;
    }
    if (index == UNITS_JOB_NONE) {
      index = UNITS_JOB_COLONIST;
    }
    break;
  case COLONY_FOCUS_SHIPS:
    if (csv->docked_transport_count != 0 && game->units_ok &&
        (u = units_get_const(&game->units, csv->transport_unit_id)) != NULL) {
      index = u->type_index;
    }
    break;
  case COLONY_FOCUS_UNITS:
    if (csv->outside_unit_count != 0 && game->units_ok &&
        (u = units_get_const(&game->units, csv->multi_unit_selected_id)) != NULL) {
      index = u->type_index;
    }
    break;
  case COLONY_FOCUS_WAREHOUSE:
    cat = PEDIA_CAT_CARGO;
    index = csv->selected_cargo >= 0 ? csv->selected_cargo : 0;
    break;
  }
  if (index < 0) {
    return;
  }
  const int cid = game->colony_view_id;
  game_open_pedia_article(game, cat, index, false);
  game->pedia_return_colony_id = cid;
}

/* Colony screen: left-click dispatch over every panel of the colony view. */
static GameUpdateStep game_colony_screen_mouse_click(
  ColonizeGameState* game, const ColonizeInputState* input, ColonizeColony* colony,
  ColonyScreenView* csv, const ColonizeWorldMap* cmap
) {
  if (input->mouse_left_clicked && colony) {
    if (ui_drag_active(&game->ui_drag)) {
      return GAME_UPDATE_RETURN_TRUE; /* ignore click while dragging */
    }
    const ColonyScreenHitResult hit = colony_screen_hit_test(
      csv, &game->colonies, colony, game->units_ok ? &game->units : NULL, input->mouse_x, input->mouse_y
    );
    switch (hit.kind) {
    case COLONY_HIT_TITLE:
      game_colony_open_rename(game, colony);
      return GAME_UPDATE_RETURN_TRUE;
    case COLONY_HIT_EXIT:
      game_ui_drag_clear(game);
      game->in_colony = false;
      game->colony_view_id = -1;
      return GAME_UPDATE_RETURN_TRUE;
    case COLONY_HIT_TRANSPORT:
      csv->focus = COLONY_FOCUS_SHIPS; /* FUN_2f2b_548e raw 52222-52225 */
      if (hit.index >= 0 && hit.index < csv->docked_transport_count) {
        const int uid = csv->docked_transport_ids[hit.index];
        if (uid == csv->transport_unit_id && game->units_ok) {
          /* Second click on the already-selected transport: docked-unit
           * orders (DOS FUN_2f2b_5746 / @COLONYUNIT), same select-then-
           * click-assigns convention used elsewhere in the colony screen. */
          colony_screen_open_dock_orders(csv, &game->units, &game->messages, uid);
        } else {
          csv->transport_unit_id = uid;
          const ColonizeUnit* tu = units_get_const(&game->units, uid);
          const ColonizeUnitType* tt = tu ? units_type(&game->units, tu->type_index) : NULL;
          snprintf(
            game->status,
            sizeof(game->status),
            "%s",
            tt && tt->name[0] ? tt->name : "Transport selected"
          );
          colony_screen_set_status(csv, game->status);
        }
      }
      break;
    case COLONY_HIT_CARGO_SLOT:
      csv->focus = COLONY_FOCUS_WAREHOUSE; /* FUN_2f2b_60dc raw 52848-52850 */
      game_colony_drag_begin_cargo(game, hit.index);
      break;
    case COLONY_HIT_HOLD:
      csv->focus = COLONY_FOCUS_SHIPS; /* FUN_2f2b_56ce raw 52300-52302 */
      game_colony_drag_begin_hold(game, hit.index);
      break;
    case COLONY_HIT_COLONIST:
    case COLONY_HIT_PEOPLE_COLONIST: {
      const bool was_selected = csv->selected_colonist == hit.index;
      if (hit.kind == COLONY_HIT_PEOPLE_COLONIST) {
        /* FUN_2f2b_4310 (asm 0x29C42-0x29C76): a People-band press always takes
         * the focus and moves the area cursor to the colonist's plot. #1284. */
        csv->focus = COLONY_FOCUS_COLONISTS;
        const int ti = colonies_colonist_tile(colony, hit.index);
        if (ti >= 0) {
          colonies_field_tile_delta(ti, &csv->area_dx, &csv->area_dy);
        }
      } else if (!was_selected) {
        /* FUN_2f2b_44d4 raw 51248-51260: only a press on another colonist moves the focus. */
        csv->focus = COLONY_FOCUS_COLONISTS;
      }
      game_colony_drag_begin_colonist(game, hit.index);
      csv->drag_from_selected = was_selected;
      break;
    }
    case COLONY_HIT_OUTSIDE_UNIT:
      if (hit.index >= 0 && hit.index < csv->outside_unit_count) {
        const int ouid = csv->outside_unit_ids[hit.index];
        /* 44d4: a fence unit is colonist pop+n, so it presses like a worker
         * (raw 51248-51286): the press may start a drag, and only a release
         * back on the already-selected unit opens the role popup
         * (game_colony_drag_drop). */
        const bool was_selected = csv->selected_outside_unit == ouid;
        /* In the People band (DOS region 0) FUN_2f2b_4310 takes the focus on
         * every press; on the fence only another unit does (44d4). #1284. */
        if (!was_selected || colony_screen_dos_region(input->mouse_x, input->mouse_y) == 0) {
          csv->focus = COLONY_FOCUS_COLONISTS;
        }
        game_colony_drag_begin_outside(game, ouid);
        csv->drag_from_selected = was_selected;
      }
      break;
    case COLONY_HIT_FENCE: {
      game_colony_fence_drop(game, colony);
      break;
    }
    case COLONY_HIT_EJECT_ROW: {
      if (hit.index >= 0 && hit.index < csv->eject_role_count &&
          !csv->eject_role_enabled[hit.index]) {
        break; /* greyed row (FUN_15eb_3454 → 0xffff): not clickable */
      }
      if (hit.index >= 0 && hit.index < csv->eject_role_count && game->units_ok) {
        const int role = csv->eject_roles[hit.index];
        if (csv->eject_unit_id >= 0 && colony) {
          const int uid = csv->eject_unit_id;
          const bool ok =
            game_colony_apply_outside_role(&game->colonies, colony, &game->units, uid, role);
          colony_screen_close_eject(csv);
          if (ok) {
            game_colony_select_outside(game, uid);
            snprintf(
              game->status,
              sizeof(game->status),
              "Equipped as %s",
              colonies_eject_role_name(role)
            );
          } else {
            set_status(game, "Cannot equip unit", NULL);
          }
        } else {
          game_colony_request_eject(game, csv->eject_colonist_index, role);
        }
        colony_screen_set_status(csv, game->status);
      }
      break;
    }
    case COLONY_HIT_EJECT_OUTSIDE:
      colony_screen_close_eject(csv);
      break;
    case COLONY_HIT_DOCK_ORDERS_ROW:
      if (hit.index >= 0 && hit.index < csv->dock_orders_count) {
        game_colony_apply_dock_order(game, csv, csv->dock_orders_actions[hit.index]);
      }
      break;
    case COLONY_HIT_DOCK_ORDERS_OUTSIDE:
      colony_screen_close_dock_orders(csv);
      break;
    case COLONY_HIT_MESSAGE_OK:
    case COLONY_HIT_MESSAGE_NO:
      colony_screen_close_message(csv);
      break;
    case COLONY_HIT_MESSAGE_YES: {
      const int who = csv->pending_eject_colonist;
      const int role = csv->pending_eject_role;
      colony_screen_close_message(csv);
      game_colony_finish_eject(game, who, role);
      break;
    }
    case COLONY_HIT_MESSAGE_OUTSIDE:
      /* Keep modal open until Yes/No/OK. */
      break;
    case COLONY_HIT_MULTI_BTN:
      /* FUN_2f2b_4a1c: the press only shows the pressed look (DS:0x339); the
       * switch happens on release (game_update_colony_screen). #1285. */
      if (hit.index >= 0 && hit.index < 3) {
        csv->tab_pressed = hit.index;
      }
      break;
    case COLONY_HIT_MULTI_UNIT_ICON:
      /* hit.index is the unit id (see colony_screen_hit_test). Same
       * select-then-click convention as the Transport strip: second click
       * on the already-selected unit opens its docked/stationed-unit
       * orders (DOS FUN_2f2b_59a0 double-click → FUN_2f2b_5746). */
      csv->focus = COLONY_FOCUS_UNITS; /* FUN_2f2b_59a0 raw 52469-52471 */
      if (hit.index >= 0 && game->units_ok) {
        if (hit.index == csv->multi_unit_selected_id) {
          colony_screen_open_dock_orders(csv, &game->units, &game->messages, hit.index);
        } else {
          csv->multi_unit_selected_id = hit.index;
          const ColonizeUnit* tu = units_get_const(&game->units, hit.index);
          const ColonizeUnitType* tt = tu ? units_type(&game->units, tu->type_index) : NULL;
          snprintf(
            game->status, sizeof(game->status), "%s", tt && tt->name[0] ? tt->name : "Unit selected"
          );
          colony_screen_set_status(csv, game->status);
        }
      }
      break;
    case COLONY_HIT_MULTI_PANE:
      if (csv->multi_mode == COLONY_MULTI_UNITS) {
        csv->focus = COLONY_FOCUS_UNITS; /* 59a0: any press in the pane */
      }
      if (csv->multi_mode == COLONY_MULTI_PRODUCTION) {
        csv->show_production_numbers = !csv->show_production_numbers;
      } else if (csv->multi_mode == COLONY_MULTI_CONSTRUCTION) {
        {
          ColoniesBuildableOpts bopts = game_colony_buildable_opts(game);
          colony_screen_open_construction(
            csv, &game->colonies, game->colony_view_id, &bopts
          );
        }
      }
      break;
    case COLONY_HIT_MULTI_BUY: {
      game_request_buy_construction_confirm(game);
      break;
    }
    case COLONY_HIT_MULTI_CHANGE:
      {
        ColoniesBuildableOpts bopts = game_colony_buildable_opts(game);
        colony_screen_open_construction(
          csv, &game->colonies, game->colony_view_id, &bopts
        );
      }
      break;
    case COLONY_HIT_AREA_TILE: {
      /* DOS FUN_2f2b_47bc: pressing on a worked plot picks its colonist up
       * (FUN_2f2b_4284, drag mode 6), so it can be dropped on a building,
       * another plot or the fence like any other worker. */
      game_colony_focus_area(csv, hit.index);
      const int who = (int)colony->tiles[hit.index];
      if (who >= 0 && who < colony->colonist_count) {
        const bool was_selected = csv->selected_colonist == who;
        game_colony_drag_begin_colonist(game, who);
        csv->drag_from_tile = hit.index;
        csv->drag_from_selected = was_selected;
      } else {
        game_colony_area_tile_drop(game, colony, cmap, hit.index);
      }
      break;
    }
    case COLONY_HIT_AREA_INTERIOR:
      /* DOS 2f2b:609e — click on the area view's non-assignable centre
       * flips the game-wide badge-numbers toggle (DS:0x336). FUN_2f2b_47bc
       * treats it as an inner cell: focus and cursor move there. #1284. */
      csv->show_production_numbers = !csv->show_production_numbers;
      csv->focus = COLONY_FOCUS_AREA;
      csv->area_dx = 0;
      csv->area_dy = 0;
      break;
    case COLONY_HIT_JOBS_ROW:
      if (hit.index >= 0 && hit.index < csv->job_count) {
        game_colony_commit_job(game, colony, csv->job_ids[hit.index]);
      } else {
        colony_screen_close_jobs(csv);
        colony_screen_set_status(csv, game->status);
      }
      break;
    case COLONY_HIT_JOBS_MORE:
      colony_screen_jobs_next_page(csv);
      break;
    case COLONY_HIT_JOBS_OUTSIDE:
      colony_screen_close_jobs(csv);
      break;
    case COLONY_HIT_BUILDING: {
      /* Custom House has no worker slot (colonies_assign_workplace
       * refuses it) — a plain click opens its per-cargo autosell
       * checklist instead of trying (and failing) to assign a colonist. */
      const ColonizeBuildingType* bt = colonies_building_type(&game->colonies, hit.index);
      if (bt && colonies_building_name_row(bt->name) == COLONY_BUILDING_CUSTOM_HOUSE) {
        ColonizeColony* col = colonies_get_mut(&game->colonies, game->colony_view_id);
        if (col && hit.index >= 0 && hit.index < COLONIZE_BUILDING_TYPES_MAX &&
            col->has_building[hit.index]) {
          colony_screen_open_custom_house(csv, col, &game->messages);
        } else {
          set_status(game, "Build it first", NULL);
          colony_screen_set_status(csv, game->status);
        }
      } else {
        game_colony_assign_building_drop(game, hit.index);
      }
      break;
    }
    case COLONY_HIT_CUSTOM_HOUSE_ROW:
      /* FUN_6f74_2580: the press puts the bar on the row (raw 116745-116758), the release
       * flips it (@checkbox, raw 117052-117058). */
      if (hit.index >= 0 && hit.index < csv->custom_house_count) {
        csv->custom_house_selection = hit.index;
        colonies_toggle_custom_house_cargo(
          &game->colonies, game->colony_view_id, csv->custom_house_cargo_ids[hit.index]
        );
      }
      break;
    case COLONY_HIT_CUSTOM_HOUSE_OUTSIDE:
      colony_screen_close_custom_house(csv);
      break;
    case COLONY_HIT_CONSTRUCTION_CLEAR:
      colonies_build_queue_clear(&game->colonies, game->colony_view_id);
      colonies_clear_construction(&game->colonies, game->colony_view_id);
      colony_screen_close_construction(csv);
      set_status(game, "Construction cleared", NULL);
      colony_screen_set_status(csv, game->status);
      break;
    case COLONY_HIT_CONSTRUCTION_ROW:
      if (hit.index >= 0 && hit.index < csv->buildable_count) {
        const int bid = csv->buildable_ids[hit.index];
        if (input->shift_held) {
          game_colony_queue_construction(game, bid);
        } else {
          game_colony_commit_construction(game, bid);
        }
      } else {
        colony_screen_close_construction(csv);
        colony_screen_set_status(csv, game->status);
      }
      break;
    case COLONY_HIT_CONSTRUCTION_MORE:
      colony_screen_construction_next_page(csv);
      break;
    case COLONY_HIT_CONSTRUCTION_OUTSIDE:
      colony_screen_close_construction(csv);
      break;
    default:
      break;
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Colony screen: ENTER activates the focused widget / closes the open sub-panel. */
static GameUpdateStep game_colony_screen_key_enter(
  ColonizeGameState* game, const ColonizeInputState* input, ColonizeColony* colony,
  ColonyScreenView* csv, const ColonizeWorldMap* cmap
) {
  if (input->last_key == COLONIZE_KEY_ENTER) {
    if (csv->message_kind == COLONY_MSG_OK) {
      colony_screen_close_message(csv);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (csv->message_kind == COLONY_MSG_CONFIRM) {
      if (csv->message_selection == 0) {
        const int who = csv->pending_eject_colonist;
        const int role = csv->pending_eject_role;
        colony_screen_close_message(csv);
        game_colony_finish_eject(game, who, role);
      } else {
        colony_screen_close_message(csv);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* Panel priority below is the SAME order Escape's cascade uses
     * (message, jobs, eject, dock orders, Custom House, construction).
     * The six are mutually exclusive by construction — every opener goes
     * through colony_screen_close_subpanels — so the order is a tie-break
     * that must not disagree between the two keys. */
    if (csv->jobs_open) {
      if (csv->more_selected) {
        colony_screen_jobs_next_page(csv);
      } else if (csv->jobs_selection >= 0 && csv->jobs_selection < csv->job_count) {
        game_colony_commit_job(game, colony, csv->job_ids[csv->jobs_selection]);
      } else {
        colony_screen_close_jobs(csv);
        colony_screen_set_status(csv, game->status);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (csv->eject_open) {
      if (csv->eject_selection >= 0 && csv->eject_selection < csv->eject_role_count &&
          !csv->eject_role_enabled[csv->eject_selection]) {
        /* DOS's 0xffff row: drawn greyed and inert — the dialog stays up. */
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (csv->eject_selection >= 0 && csv->eject_selection < csv->eject_role_count &&
          game->units_ok) {
        const int role = csv->eject_roles[csv->eject_selection];
        if (csv->eject_unit_id >= 0 && colony) {
          const bool ok =
            game_colony_apply_outside_role(
              &game->colonies, colony, &game->units, csv->eject_unit_id, role
            );
          colony_screen_close_eject(csv);
          if (ok) {
            game_colony_select_outside(game, csv->selected_outside_unit);
            snprintf(game->status, sizeof(game->status), "Equipped as %s", colonies_eject_role_name(role));
          } else {
            set_status(game, "Cannot equip unit", NULL);
          }
          colony_screen_set_status(csv, game->status);
        } else {
          game_colony_request_eject(game, csv->eject_colonist_index, role);
        }
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (csv->dock_orders_open) {
      if (csv->dock_orders_selection >= 0 && csv->dock_orders_selection < csv->dock_orders_count) {
        game_colony_apply_dock_order(
          game, csv, csv->dock_orders_actions[csv->dock_orders_selection]
        );
      } else {
        colony_screen_close_dock_orders(csv);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* @CUSTOM is a FUN_6f74_2580 @checkbox box: Enter closes it whatever row has the bar
     * (6f74:2cfc, raw 116856-116857), and must not fall through to the panels underneath. */
    if (csv->custom_house_open) {
      colony_screen_close_custom_house(csv);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (csv->construction_open) {
      const int bi = csv->construction_selection - 1;
      if (csv->more_selected) {
        colony_screen_construction_next_page(csv);
      } else if (csv->construction_selection == 0) {
        colonies_build_queue_clear(&game->colonies, game->colony_view_id);
        colonies_clear_construction(&game->colonies, game->colony_view_id);
        set_status(game, "Construction cleared", NULL);
        colony_screen_close_construction(csv);
        colony_screen_set_status(csv, game->status);
      } else if (bi >= 0 && bi < csv->buildable_count) {
        /* Shift+Enter is the keyboard twin of the Shift-click queue pick. */
        if (input->shift_held) {
          game_colony_queue_construction(game, csv->buildable_ids[bi]);
        } else {
          game_colony_commit_construction(game, csv->buildable_ids[bi]);
        }
      } else {
        colony_screen_close_construction(csv);
        colony_screen_set_status(csv, game->status);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* On the bare screen Enter is a focus key (game_colony_focus_key): FUN_2f2b_6372 has
     * no Enter arm of its own, and Enter never leaves the colony. */
  }
  return GAME_UPDATE_CONTINUE;
}

/*
 * The FUN_2f2b_6372 keyboard focus (DS:0x32e) on the bare screen: Tab, the cursor keys and
 * Enter (Space / S on the ships). colony_screen_focus_key moves the pick; the actions run
 * through the same handlers a press uses.
 */
static GameUpdateStep game_colony_focus_key(
  ColonizeGameState* game, const ColonizeInputState* input, ColonizeColony* colony,
  ColonyScreenView* csv, const ColonizeWorldMap* cmap
) {
  if (!colony || input->last_key == COLONIZE_KEY_NONE || csv->jobs_open ||
      csv->construction_open || csv->eject_open || csv->dock_orders_open ||
      csv->custom_house_open || csv->message_kind != COLONY_MSG_NONE ||
      ui_drag_active(&game->ui_drag)) {
    return GAME_UPDATE_CONTINUE;
  }
  if ((input->last_key == COLONIZE_KEY_SPACE || input->last_key == COLONIZE_KEY_S) &&
      csv->focus != COLONY_FOCUS_SHIPS) {
    return GAME_UPDATE_CONTINUE;
  }
  const ColonyFocusKeyResult fr =
    colony_screen_focus_key(csv, colony, game->units_ok ? &game->units : NULL, input->last_key);
  switch (fr.action) {
  case COLONY_FOCUS_KEY_NONE:
    return input->last_key == COLONIZE_KEY_ENTER ? GAME_UPDATE_RETURN_TRUE : GAME_UPDATE_CONTINUE;
  case COLONY_FOCUS_KEY_REDRAW:
    break;
  case COLONY_FOCUS_KEY_SELECT_COLONIST:
    game_colony_select_colonist(game, fr.value);
    break;
  case COLONY_FOCUS_KEY_SELECT_OUTSIDE:
    game_colony_select_outside(game, fr.value);
    break;
  case COLONY_FOCUS_KEY_SELECT_SHIP:
    csv->transport_unit_id = fr.value;
    break;
  case COLONY_FOCUS_KEY_AREA_ENTER:
    if (fr.value >= 0) {
      game_colony_area_tile_drop(game, colony, cmap, fr.value);
    }
    break;
  case COLONY_FOCUS_KEY_COLONIST_ENTER:
    /* FUN_2f2b_348c(0): the jobs menu, or a fence unit's role list (the press twin). */
    if (csv->selected_colonist >= 0) {
      game_colony_open_jobs(
        game, colony, cmap, colonies_colonist_tile(colony, csv->selected_colonist)
      );
    } else if (csv->selected_outside_unit >= 0) {
      game_colony_fence_drop(game, colony);
    }
    break;
  case COLONY_FOCUS_KEY_SHIP_ENTER:
    csv->transport_unit_id = fr.value;
    if (game->units_ok) {
      colony_screen_open_dock_orders(csv, &game->units, &game->messages, fr.value);
    }
    break;
  case COLONY_FOCUS_KEY_UNIT_ENTER:
    if (game->units_ok) {
      colony_screen_open_dock_orders(csv, &game->units, &game->messages, fr.value);
    }
    break;
  }
  csv->blink_epoch_ms = csv->blink_ms; /* FUN_2f2b_2e2a full redraw: DS:0x334 on */
  colony_screen_set_status(csv, game->status);
  return GAME_UPDATE_RETURN_TRUE;
}

/*
 * FUN_2f2b_5bd2 raises DS:0x1f66 before it shows the construction list (raw 52632), so
 * FUN_6f74_2580 hands back the barred row on F1 and the row under the pointer on a right
 * click, both with DS:0x1f68 = 1 (raw 116866-116869, 117062-117065). The "(More)" row still
 * turns the page (raw 52638-52643). Any other row goes through FUN_281f_0cc2 (raw 52650): a
 * building opens its Colonizopedia article (FUN_291f_0902), a unit its unit article
 * (FUN_291f_0942); the clear row decodes to neither. Either way the list is rebuilt on the
 * same page (raw 52655-52658), which here is the state the pedia hands back on exit.
 */
static void game_colony_construction_help(
  ColonizeGameState* game, const ColonizeInputState* input, const ColonizeColony* colony,
  ColonyScreenView* csv
) {
  bool more = csv->more_selected;
  int bi = csv->construction_selection - 1;
  if (input->mouse_right_clicked) {
    const ColonyScreenHitResult hit = colony_screen_hit_test(
      csv, &game->colonies, colony, game->units_ok ? &game->units : NULL, input->mouse_x,
      input->mouse_y
    );
    if (hit.kind == COLONY_HIT_CONSTRUCTION_OUTSIDE) {
      return;
    }
    more = hit.kind == COLONY_HIT_CONSTRUCTION_MORE;
    bi = hit.kind == COLONY_HIT_CONSTRUCTION_ROW ? hit.index : -1;
  }
  if (more) {
    colony_screen_construction_next_page(csv);
    return;
  }
  const int bid = bi >= 0 && bi < csv->buildable_count ? csv->buildable_ids[bi] : -1;
  const int page = csv->construction_page;
  const ColoniesBuildableOpts bopts = game_colony_buildable_opts(game);
  colony_screen_open_construction(csv, &game->colonies, game->colony_view_id, &bopts);
  if (page != csv->construction_page) {
    colony_screen_construction_next_page(csv);
  }
  if (bid < 0) {
    return;
  }
  const int unit_row = units_build_code_to_index(bid);
  const int cid = game->colony_view_id;
  if (unit_row >= 0) {
    game_open_pedia_article(game, PEDIA_CAT_UNIT, unit_row, false);
  } else {
    game_open_pedia_article(game, PEDIA_CAT_BUILDING, bid, false);
  }
  game->pedia_return_colony_id = cid;
}

/*
 * FUN_2f2b_348c raises DS:0x1f66 for both of its lists (raw 50823): the jobs list (param 0)
 * and the leave-as list. F1 takes the barred row, a right-click the row under the pointer
 * (greyed rows are skipped, FUN_6f74_2580 raw 116747). "(More)" turns the page (raw
 * 50828-50830) and "Clear Specialty" (0x61) is remapped to a re-show (raw 50831-50833). Any
 * other row (id = @JOB + 1) opens the skill article of its @JOB (FUN_291f_08de, raw
 * 50893-50901): a leave-as role is @JOB 0x13 + role, the stay row the colonist's current
 * occupation. The list is then rebuilt from scratch, page 0 and default bar (raw 50523,
 * 50676-50677), and comes back when the article closes.
 */
static void game_colony_list_help(
  ColonizeGameState* game, const ColonizeInputState* input, ColonizeColony* colony,
  ColonyScreenView* csv
) {
  const ColonyScreenHitResult hit = input->mouse_right_clicked
    ? colony_screen_hit_test(
        csv, &game->colonies, colony, game->units_ok ? &game->units : NULL, input->mouse_x,
        input->mouse_y
      )
    : (ColonyScreenHitResult){.kind = COLONY_HIT_NONE, .index = -1};
  if (hit.kind == COLONY_HIT_JOBS_OUTSIDE || hit.kind == COLONY_HIT_EJECT_OUTSIDE) {
    return;
  }
  int job = -1;
  if (csv->jobs_open) {
    const bool more = input->mouse_right_clicked ? hit.kind == COLONY_HIT_JOBS_MORE
                                                 : csv->more_selected;
    const int i = input->mouse_right_clicked
                    ? (hit.kind == COLONY_HIT_JOBS_ROW ? hit.index : -1)
                    : csv->jobs_selection;
    if (more) {
      colony_screen_jobs_next_page(csv);
      return;
    }
    if (i < 0 || i >= csv->job_count || csv->job_ids[i] < 0) {
      return; /* Clear Specialty: the list just shows again */
    }
    job = csv->job_ids[i];
    colony_screen_open_jobs(
      csv, &game->colonies, game->world_map_ok ? &game->world_map : NULL, colony,
      csv->jobs_tile_index
    );
  } else {
    const int i = input->mouse_right_clicked
                    ? (hit.kind == COLONY_HIT_EJECT_ROW ? hit.index : -1)
                    : csv->eject_selection;
    if (i < 0 || i >= csv->eject_role_count || !csv->eject_role_enabled[i]) {
      return;
    }
    const int role = csv->eject_roles[i];
    if (role != COLONY_EJECT_ROLE_STAY) {
      job = UNITS_JOB_COLONIST + role;
    } else if (csv->eject_colonist_index >= 0 &&
               csv->eject_colonist_index < colony->colonist_count) {
      const ColonizeColonist* c = &colony->colonists[csv->eject_colonist_index];
      job = c->field_job >= 0 ? c->field_job
            : c->building_type >= 0 ? colonies_building_occupation(&game->colonies, c->building_type)
                                    : -1;
    }
    if (job < 0) {
      return;
    }
    if (csv->eject_unit_id >= 0) {
      game_colony_fence_drop(game, colony);
    } else {
      colony_screen_open_eject(
        csv, &game->colonies, game->colony_view_id, csv->eject_colonist_index
      );
    }
  }
  const int cid = game->colony_view_id;
  game_open_pedia_article(game, PEDIA_CAT_JOB, job, false);
  game->pedia_return_colony_id = cid;
}

/* Colony-screen keyboard: Escape / Enter, the C construction toggle, the
 * 1/2/3/M/N tab keys and the open sub-panel's own navigation keys. */
static GameUpdateStep game_colony_screen_keys(
  ColonizeGameState* game, const ColonizeInputState* input, ColonizeColony* colony,
  ColonyScreenView* csv, const ColonizeWorldMap* cmap
) {
  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    if (ui_drag_active(&game->ui_drag)) {
      game_ui_drag_clear(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* Sub-panel cascade: Escape dismisses the topmost open panel and only
     * leaves the colony once none is up. Priority order is shared verbatim
     * with the Enter cascade below (message, jobs, eject, dock orders,
     * Custom House, construction); the six can never actually be open at
     * once — colony_screen_close_subpanels runs from every opener — so the
     * order is a tie-break the two keys must not spell differently. */
    if (csv->message_kind != COLONY_MSG_NONE) {
      colony_screen_close_message(csv);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (csv->jobs_open) {
      colony_screen_close_jobs(csv);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (csv->eject_open) {
      colony_screen_close_eject(csv);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (csv->dock_orders_open) {
      colony_screen_close_dock_orders(csv);
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* The Custom House checklist is a sub-panel like the rest: Escape has
     * to close IT before the colony screen. Without this row Escape left
     * the panel flagged open and walked straight out of the colony (the
     * next entry then self-healed it in colony_screen_reset), and the
     * panel meanwhile suppressed the area-tile right-click pedia below. */
    if (csv->custom_house_open) {
      colony_screen_close_custom_house(csv);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (csv->construction_open) {
      colony_screen_close_construction(csv);
      return GAME_UPDATE_RETURN_TRUE;
    }
    game->in_colony = false;
    sound_set_bgm(1); /* back on the map: DOS FUN_281f_0498(1) tune pool */
    game->colony_view_id = -1;
    diag_info("Left colony screen.");
    return GAME_UPDATE_RETURN_TRUE;
  }
  {
    const GameUpdateStep sub = game_colony_screen_key_enter(game, input, colony, csv, cmap);
    if (sub != GAME_UPDATE_CONTINUE) {
      return sub;
    }
  }

  /* The Custom House box owns the keyboard like DOS's FUN_6f74_2580 loop (port_bugs.md #1213):
   * Up / Down move the bar, Space / F1 / a row hotkey flip a row, every other key does nothing. */
  if (csv->custom_house_open &&
      (input->last_key != COLONIZE_KEY_NONE || input->text_input_len > 0)) {
    if (!colony_screen_list_key(csv, input->last_key)) {
      const int row = colony_screen_custom_house_key(csv, input);
      if (row >= 0) {
        colonies_toggle_custom_house_cargo(
          &game->colonies, game->colony_view_id, csv->custom_house_cargo_ids[row]
        );
      }
    }
    return GAME_UPDATE_RETURN_TRUE;
  }

  /* F1 on the bare screen; the construction / jobs / leave-as lists took theirs above. */
  if (input->last_key == COLONIZE_KEY_F1 && colony && !csv->dock_orders_open &&
      csv->message_kind == COLONY_MSG_NONE && !ui_drag_active(&game->ui_drag)) {
    game_colony_screen_help(game, colony, csv);
    return GAME_UPDATE_RETURN_TRUE;
  }

  /* C = Construction Change (tech-supp). Toggle: down when Construction is
   * already up, otherwise every other sub-panel closes first so the new one
   * cannot open underneath a still-open eject / dock-orders / Custom House
   * list (colony_screen_open_construction closes them too — this states the
   * hotkey's own "one panel at a time" contract next to the toggle). */
  if (input->last_key == COLONIZE_KEY_C) {
    if (csv->construction_open) {
      colony_screen_close_construction(csv);
    } else {
      colony_screen_close_subpanels(csv);
      {
        ColoniesBuildableOpts bopts = game_colony_buildable_opts(game);
        colony_screen_open_construction(
          csv, &game->colonies, game->colony_view_id, &bopts
        );
      }
      csv->multi_mode = COLONY_MULTI_CONSTRUCTION;
    }
    return GAME_UPDATE_RETURN_TRUE;
  }

  /* 1/2/3 = multifunction tabs; M cycles; N toggles production numbers; =/+ load. */
  for (int ti = 0; ti < input->text_input_len; ++ti) {
    const char ch = input->text_input[ti];
    /* FUN_2f2b_6372 (2f2b:6722 / 6654): 1/2/3 and M set DS:0x338 = DS:0x337
     * = the new tab. #1285. */
    if (ch == '1') {
      csv->multi_mode = csv->prev_multi_mode = COLONY_MULTI_PRODUCTION;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '2') {
      csv->multi_mode = csv->prev_multi_mode = COLONY_MULTI_UNITS;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '3') {
      csv->multi_mode = csv->prev_multi_mode = COLONY_MULTI_CONSTRUCTION;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == 'm' || ch == 'M') {
      csv->multi_mode = (ColonyMultiMode)(((int)csv->multi_mode + 1) % 3);
      csv->prev_multi_mode = csv->multi_mode;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == 'n' || ch == 'N') {
      csv->show_production_numbers = !csv->show_production_numbers;
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* FUN_2f2b_6372 cheat tail (T S $ % ^ !), only with the cheat bit set and
     * on the bare colony screen (DOS runs every sub-panel in its own loop). */
    if (colony && !csv->jobs_open && !csv->construction_open && !csv->eject_open &&
        !csv->dock_orders_open && !csv->custom_house_open &&
        csv->message_kind == COLONY_MSG_NONE && game_colony_cheat_key(game, colony, csv, ch)) {
      return GAME_UPDATE_RETURN_TRUE;
    }
    if ((ch == '=' || ch == '+') && colony && game->units_ok && !csv->jobs_open &&
        !csv->construction_open) {
      /* DOS reads DS:0x33a directly: static 0 (Food) until a slot is picked. */
      const int cargo = (csv->selected_cargo >= 0 && csv->selected_cargo < COLONIZE_CARGO_COUNT)
                          ? csv->selected_cargo
                          : 0;
      /* FUN_2f2b_6372 2f2b:66e0-6712: no holder (DS:0x33c == 0) -> red bar
       * @CMESSAGE 5 (FUN_2f2b_2ec6(5, 0x78)); else FUN_2f2b_4b62(ship, cargo,
       * 1, key == '+'): an empty slot (raw 51494-51498) -> red bar @CMESSAGE 6,
       * cargo, @CMESSAGE 7; '+' asks how much, '=' loads min(100, room, stock). */
      if (csv->transport_unit_id < 0 || colony->stock[cargo] < 1) {
        const bool no_ship = csv->transport_unit_id < 0;
        char first[64];
        const char* w = reports_labels_field("CMESSAGE", no_ship ? 5 : 6);
        snprintf(first, sizeof(first), "%s", w ? w : "");
        const char* cname = (!no_ship && game->europe_ok && cargo < game->europe.cargo_count)
                              ? game->europe.cargo[cargo].name
                              : "";
        const char* last = no_ship ? "" : reports_labels_field("CMESSAGE", 7);
        const char* parts[] = {first, cname, last ? last : ""};
        char line[AI_POPUP_BAR_MSG_LEN];
        size_t n = 0;
        line[0] = '\0';
        for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i) {
          if (parts[i][0] && n < sizeof(line)) {
            n += (size_t)snprintf(line + n, sizeof(line) - n, "%s%s", n ? " " : "", parts[i]);
          }
        }
        if (line[0]) {
          (void)ai_popup_enqueue_bar_refusal(&game->ai_popups, line);
        }
      } else if (ch == '=') {
        const int want = colony->stock[cargo] < 100 ? colony->stock[cargo] : 100;
        game_colony_load_hold(game, csv->transport_unit_id, cargo, want);
      } else {
        game_colony_open_load_prompt(game, colony, cargo);
      }
      colony_screen_set_status(csv, game->status);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if ((ch == 'r' || ch == 'R') && colony && !csv->jobs_open && !csv->construction_open &&
        csv->message_kind == COLONY_MSG_NONE) {
      game_colony_open_rename(game, colony);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* Jobs, leave-as, dock orders, construction: FUN_6f74_2580 lists (port_bugs.md #1209). */
  if (colony_screen_list_key(csv, input->last_key)) {
    return GAME_UPDATE_RETURN_TRUE;
  }
  if (csv->message_kind != COLONY_MSG_NONE) {
    const int max_sel = (csv->message_kind == COLONY_MSG_CONFIRM) ? 1 : 0;
    if (colonize_key_up(input->last_key) && csv->message_selection > 0) {
      csv->message_selection--;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (colonize_key_down(input->last_key) && csv->message_selection < max_sel) {
      csv->message_selection++;
      return GAME_UPDATE_RETURN_TRUE;
    }
  } else {
    const GameUpdateStep sub = game_colony_focus_key(game, input, colony, csv, cmap);
    if (sub != GAME_UPDATE_CONTINUE) {
      return sub;
    }
  }

  /* B = buy remaining construction with gold + warehouse tools. */
  if (input->last_key == COLONIZE_KEY_B && colony &&
      colony->building_in_production >= 0) {
    csv->multi_mode = COLONY_MULTI_CONSTRUCTION; /* 2f2b:673c; DS:0x338 untouched */
    game_request_buy_construction_confirm(game);
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Colony-screen cargo keys: L / U on the docked transport, and the CHEAT-only
 * SPACE production tick. */
static GameUpdateStep game_colony_screen_cargo_keys(
  ColonizeGameState* game, const ColonizeInputState* input, ColonizeColony* colony,
  ColonyScreenView* csv
) {
  /* L = load the highest-key cargo (FUN_479b_11a4); U = unload the lowest-key hold (DOS pick). */
  if (!csv->jobs_open && !csv->construction_open && colony && game->units_ok) {
    if (input->last_key == COLONIZE_KEY_L) {
      /* 2f2b:662e: FUN_291f_01de -> FUN_479b_11a4(selected ship). */
      game_colony_load_best_cargo(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (input->last_key == COLONIZE_KEY_U) {
      /* 2f2b:6694: FUN_479b_0f60(selected ship, 0) -- the lowest-key hold. */
      game_colony_unload_best_hold(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* SPACE = FUN_364b_0688 on the viewed colony (FUN_2f2b_6372 2f2b:6950), a
   * CHEAT-bit-only extra production tick; without the bit DOS ignores the key. */
  if (input->last_key == COLONIZE_KEY_SPACE && colony && !csv->jobs_open &&
      !csv->construction_open && !csv->eject_open && !csv->dock_orders_open &&
      !csv->custom_house_open && csv->message_kind == COLONY_MSG_NONE &&
      game_colony_cheat_produce(game, colony)) {
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

COLONIZE_INTERNAL GameUpdateStep game_update_colony_screen(ColonizeGameState* game, const ColonizeInputState* input) {
  if (game->in_colony) {
    ColonizeColony* colony = colonies_get_mut(&game->colonies, game->colony_view_id);
    ColonyScreenView* csv = &game->colony_screen;
    const ColonizeWorldMap* cmap = game->world_map_ok ? &game->world_map : NULL;
    if (colony && game->units_ok) {
      colony_screen_refresh_transports(csv, &game->units, colony);
    }
    game_service_colony_bar(game);
    game_tutorial_colony_refresh(game); /* FUN_2f2b_2d90: @TUTORIAL16 */
    /* The Units-pane focus lives only on the Units tab: every DOS tab switch turns it into
     * the colonist focus (FUN_2f2b_6372 raw 55565-55566, 55594-55596; raw 51475-51477). */
    if (csv->focus == COLONY_FOCUS_UNITS && csv->multi_mode != COLONY_MULTI_UNITS) {
      csv->focus = COLONY_FOCUS_COLONISTS;
    }

    /* DOS mouse state for the focus blink (port_bugs.md #1284): DS:0x7ee held,
     * DS:0x8d54 press region (FUN_2f2b_40a0; a holds press counts as none,
     * FUN_2f2b_56ce raw 52304-52306; 6 once a drag runs), and a full redraw
     * (FUN_2f2b_2d90 / 2e2a) on every press, release or key restarts the
     * blink in its on phase. */
    csv->mouse_held = input->mouse_left_down || input->mouse_right_down;
    if (input->mouse_left_clicked || input->mouse_right_clicked) {
      csv->press_region = colony_screen_dos_region(input->mouse_x, input->mouse_y);
      if (csv->press_region == 8 && input->mouse_x >= 0x7f && input->mouse_x < 0x7f + 0x48 &&
          input->mouse_y >= 0xa5 && input->mouse_y < 0xa5 + 0x16) {
        csv->press_region = 0x14;
      }
    } else if (ui_drag_active(&game->ui_drag)) {
      csv->press_region = 6;
    }
    if (input->mouse_left_clicked || input->mouse_left_released || input->mouse_right_clicked ||
        input->last_key != COLONIZE_KEY_NONE || input->text_input_len > 0) {
      csv->blink_epoch_ms = game->elapsed_ms;
    }

    /* FUN_2f2b_4a1c raw 51461-51480 (#1285): while a tab press is held the
     * button under the pointer shows pressed; on release over a button the
     * active tab switches back to DS:0x338, any other tab becomes active
     * (DS:0x338 = the old one). */
    if (csv->tab_pressed >= 0) {
      const ColonyScreenHitResult th = colony_screen_hit_test(
        csv, &game->colonies, colony, game->units_ok ? &game->units : NULL, input->mouse_x,
        input->mouse_y
      );
      const int over = th.kind == COLONY_HIT_MULTI_BTN ? th.index : -1;
      if (over >= 0) {
        csv->tab_pressed = over;
      }
      if (!input->mouse_left_down && !input->mouse_right_down) {
        if (over >= 0) {
          if (over == (int)csv->multi_mode) {
            csv->multi_mode = csv->prev_multi_mode;
          } else {
            csv->prev_multi_mode = csv->multi_mode;
            csv->multi_mode = (ColonyMultiMode)over;
          }
        }
        csv->tab_pressed = -1;
        if (csv->focus == COLONY_FOCUS_UNITS && csv->multi_mode != COLONY_MULTI_UNITS) {
          csv->focus = COLONY_FOCUS_COLONISTS;
        }
      }
    }

    /* Pointer-over building name label (DOS FUN_2f2b_44d4 hover arm). */
    csv->hover_building_row = -1;
    if (colony && !csv->construction_open && !csv->jobs_open && !csv->eject_open &&
        !csv->dock_orders_open && !csv->custom_house_open &&
        csv->message_kind == COLONY_MSG_NONE && !ui_drag_active(&game->ui_drag)) {
      const ColonyScreenHitResult hover = colony_screen_hit_test(
        csv, &game->colonies, colony, game->units_ok ? &game->units : NULL, input->mouse_x,
        input->mouse_y
      );
      if (hover.kind == COLONY_HIT_BUILDING) {
        csv->hover_building_row = hover.index;
      }
    }

    if (colony && (csv->construction_open || csv->jobs_open || csv->eject_open) &&
        (input->last_key == COLONIZE_KEY_F1 || input->mouse_right_clicked)) {
      if (csv->construction_open) {
        game_colony_construction_help(game, input, colony, csv);
      } else {
        game_colony_list_help(game, input, colony, csv);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

    {
      const GameUpdateStep sub = game_colony_screen_keys(game, input, colony, csv, cmap);
      if (sub != GAME_UPDATE_CONTINUE) {
        return sub;
      }
    }

    {
      const GameUpdateStep sub = game_colony_screen_cargo_keys(game, input, colony, csv);
      if (sub != GAME_UPDATE_CONTINUE) {
        return sub;
      }
    }

    if (input->mouse_right_clicked && ui_drag_active(&game->ui_drag)) {
      game_ui_drag_clear(game);
      return GAME_UPDATE_RETURN_TRUE;
    }

    /* Right-click on an area-view tile: terrain pedia article, back to the
     * colony screen on exit. */
    if (input->mouse_right_clicked && colony && !csv->construction_open && !csv->jobs_open &&
        !csv->eject_open && !csv->dock_orders_open && !csv->custom_house_open &&
        csv->message_kind == COLONY_MSG_NONE) {
      const ColonyScreenHitResult hit = colony_screen_hit_test(
        csv, &game->colonies, colony, game->units_ok ? &game->units : NULL, input->mouse_x,
        input->mouse_y
      );
      if (hit.kind == COLONY_HIT_AREA_TILE) {
        game_colony_focus_area(csv, hit.index);
        game_colony_area_pedia(game, colony, csv->area_dx, csv->area_dy);
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (hit.kind == COLONY_HIT_AREA_INTERIOR) {
        game_colony_area_pedia(game, colony, 0, 0);
        return GAME_UPDATE_RETURN_TRUE;
      }
      /* Right-click on a built settlement building: its Colonizopedia
       * article, back to the colony screen on exit. @BUILDING row order is
       * the pedia building index (pedia.c PEDIA_CAT_BUILDING). */
      if (hit.kind == COLONY_HIT_BUILDING && hit.index >= 0) {
        const int cid = game->colony_view_id;
        game_open_pedia_article(game, PEDIA_CAT_BUILDING, hit.index, false);
        game->pedia_return_colony_id = cid;
        return GAME_UPDATE_RETURN_TRUE;
      }
    }

    if (ui_drag_active(&game->ui_drag) && input->mouse_left_released && colony) {
      game_colony_drag_drop(game, colony, cmap, input->mouse_x, input->mouse_y, input->shift_held);
      return GAME_UPDATE_RETURN_TRUE;
    }

    {
      const GameUpdateStep sub = game_colony_screen_mouse_click(game, input, colony, csv, cmap);
      if (sub != GAME_UPDATE_CONTINUE) {
        return sub;
      }
    }

    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Europe screen input. */
/*
 * game_update_europe_screen stages: the open list popup first (it owns all
 * input), then the screen hotkeys, then the mouse.
 */

/* Keys and clicks while a Europe list popup (Recruit / Purchase / Train /
 * the @ARMOPTIONS rows) is up — it swallows the screen's own input. */
/*
 * FUN_38fd_4884 (Recruit), 41ce (Train) and 4b50 (Purchase) raise DS:0x1f66 before their
 * list (raw 64747, 64411, 64861), so F1 takes the barred row and a right-click the row under
 * the pointer, with DS:0x1f68 = 1. The None row still leaves (raw 64754, 64417, 64864). A
 * Recruit or Train row opens the skill article of its @JOB (FUN_291f_08de, raw 64762 / 64425;
 * the pool's 0x1c reads as 0x13, raw 64759-64761), a Purchase row the unit article of its
 * @UNIT (FUN_291f_0942, raw 64903), and the Europe screen comes back with the same list up
 * (thunk_FUN_291f_0dfe and the caller's loop). The dock list and the @REALLYBUY confirm arm
 * no help (europe_menu_layout), so there a right-click just cancels.
 */
static void game_europe_menu_help(
  ColonizeGameState* game, const ColonizeInputState* input, EuropeScreen* eu
) {
  const bool armed = eu->menu == EUROPE_MENU_RECRUIT || eu->menu == EUROPE_MENU_TRAIN ||
                     (eu->menu == EUROPE_MENU_PURCHASE && !eu->purchase_confirming);
  if (!armed) {
    if (input->mouse_right_clicked) {
      europe_menu_close(eu);
    }
    return;
  }
  const int row = input->mouse_right_clicked
                    ? europe_menu_row_at(game, input->mouse_x, input->mouse_y)
                    : eu->menu_selection;
  /* FUN_6f74_2580's pointer test skips greyed rows (raw 116747); off a row the port's
   * right-click cancels, as before. */
  PediaCategory cat = PEDIA_CAT_JOB;
  int index = -1;
  if (eu->menu == EUROPE_MENU_RECRUIT && row >= 1 && row <= EUROPE_POOL_SIZE &&
      europe_recruit_affordable(eu)) {
    index = eu->pool[row - 1].profession;
    if (index == UNITS_JOB_NONE || index < 0) {
      index = UNITS_JOB_COLONIST;
    }
  } else if (eu->menu == EUROPE_MENU_TRAIN && row >= 1 && row <= eu->train_count &&
             europe_train_affordable(eu, row - 1)) {
    index = eu->train[row - 1].job_index;
  } else if (eu->menu == EUROPE_MENU_PURCHASE && row >= 1 && row <= eu->purchase_count &&
             europe_purchase_affordable(eu, row - 1)) {
    cat = PEDIA_CAT_UNIT;
    index = (int)eu->purchase[row - 1].kind;
  }
  if (index < 0) {
    europe_menu_close(eu);
    return;
  }
  game_open_pedia_article(game, cat, index, false);
  game->pedia_return_europe = true;
}

static GameUpdateStep game_europe_screen_menu_keys(
  ColonizeGameState* game, const ColonizeInputState* input, EuropeScreen* eu
) {
  if (eu->menu != EUROPE_MENU_NONE) {
    if (ui_drag_active(&game->ui_drag)) {
      game_ui_drag_clear(game);
    }
    if (input->last_key == COLONIZE_KEY_ESCAPE) {
      europe_menu_close(eu);
      return GAME_UPDATE_RETURN_TRUE;
    }
    /*
     * Mouse: a click on a row picks it and confirms in one go (the same
     * pair the keyboard needs Up/Down + Enter for), a click anywhere else
     * inside the screen closes the menu, and a right-click cancels — the
     * arrangement every other list in this port uses. Without this the
     * branch swallowed the click and Recruit/Purchase/Train were
     * keyboard-only (port_bugs.md).
     */
    if (input->mouse_right_clicked || input->last_key == COLONIZE_KEY_F1) {
      game_europe_menu_help(game, input, eu);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (input->mouse_left_clicked) {
      const int row = europe_menu_row_at(game, input->mouse_x, input->mouse_y);
      if (row >= 0) {
        eu->menu_selection = row;
        game_europe_menu_confirm(game);
      } else {
        europe_menu_close(eu);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* FUN_38fd_4884 / 41ce / 4b50 / 4f6e lists show through FUN_6f74_2580: Up / Down wrap
     * past the greyed rows (FUN_291f_01b6, raw 64741 / 64402 / 64857 / 66395). */
    int rows = 0;
    uint32_t greyed = 0;
    switch (eu->menu) {
      case EUROPE_MENU_RECRUIT:
        rows = EUROPE_POOL_SIZE + 1;
        greyed = europe_recruit_affordable(eu) ? 0 : ((1u << rows) - 2u);
        break;
      case EUROPE_MENU_TRAIN:
        rows = eu->train_count + 1;
        for (int i = 1; i < rows && i < 32; ++i) {
          greyed |= europe_train_affordable(eu, i - 1) ? 0u : 1u << i;
        }
        break;
      case EUROPE_MENU_PURCHASE:
        rows = eu->purchase_confirming ? 2 : eu->purchase_count + 1;
        for (int i = 1; !eu->purchase_confirming && i < rows && i < 32; ++i) {
          greyed |= europe_purchase_affordable(eu, i - 1) ? 0u : 1u << i;
        }
        break;
      case EUROPE_MENU_DOCK:
        rows = eu->dock_menu_count;
        for (int i = 0; i < rows && i < 32; ++i) {
          greyed |= eu->dock_menu_greyed[i] ? 1u << i : 0u;
        }
        break;
      default:
        break;
    }
    if (!popup_list_key_step(input->last_key, &eu->menu_selection, rows, greyed) &&
        input->last_key == COLONIZE_KEY_ENTER) {
      game_europe_menu_confirm(game);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/*
 * F1 on the bare Europe screen, FUN_38fd_4f6e raw 66119-66135 (asm 38fd:4ffa): the article
 * of what the DS:0xf9a focus panel has selected, then the Europe screen again
 * (thunk_FUN_291f_0dfe). Market: the cargo article of DS:0xf9e (FUN_291f_0934). Harbor, only
 * with DS:0xfa2 ships in port: the unit article of the selected one (DS:0x9e1c through
 * FUN_38fd_0666, FUN_291f_0942). Dock, only with DS:0x9e2a units on it: the unit article of
 * DS:0x9e2c (FUN_38fd_06c4); FUN_38fd_07c6 puts DS:0x9e2c back to 0 once it runs past the
 * dock (raw 59162-59163).
 */
static void game_europe_screen_help(ColonizeGameState* game, EuropeScreen* eu) {
  PediaCategory cat = PEDIA_CAT_UNIT;
  int index = -1;
  if (eu->focus == 0) {
    cat = PEDIA_CAT_CARGO;
    index = eu->selected_market;
  } else if (eu->focus == 1 && eu->harbor_ships != 0) {
    if (eu->selected_harbor >= 0 && eu->selected_harbor < eu->harbor_ships) {
      const EuropeHarborShip* hs = &eu->harbor[eu->selected_harbor];
      index = hs->type_index;
      if (index < 0 && game->units_ok) {
        index = units_find_type(&game->units, hs->name); /* as game_europe_sail_harbor */
      }
    }
  } else if (eu->focus == 2 && eu->dock_count != 0) {
    if (eu->dock_selected < 0 || eu->dock_selected >= eu->dock_count) {
      eu->dock_selected = 0;
    }
    index = eu->dock[eu->dock_selected].dos_type;
  }
  if (index < 0) {
    return;
  }
  game_open_pedia_article(game, cat, index, false);
  game->pedia_return_europe = true;
}

/* Europe screen hotkeys: Escape/E exit, F1 article, Tab focus, R/P/T menus, L/U/-/_ cargo, S/Space/Enter
 * ship menu (FUN_38fd_4f6e). */
static GameUpdateStep game_europe_screen_keys(
  ColonizeGameState* game, const ColonizeInputState* input, EuropeScreen* eu
) {
  if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_E) {
    if (ui_drag_active(&game->ui_drag)) {
      game_ui_drag_clear(game);
      if (input->last_key == COLONIZE_KEY_ESCAPE) {
        return GAME_UPDATE_RETURN_TRUE;
      }
    }
    game_ui_drag_clear(game);
    game->in_europe = false;
    game_europe_deliver_bound_ships(game);
    diag_info("Left Europe screen.");
    return GAME_UPDATE_RETURN_TRUE;
  }

  if (input->last_key == COLONIZE_KEY_F1 && !ui_drag_active(&game->ui_drag)) {
    game_europe_screen_help(game, eu);
    return GAME_UPDATE_RETURN_TRUE;
  }

  /* DOS-LITERAL FUN_38fd_4f6e raw 66016-66038: Tab steps DS:0xf9a market -> harbor -> dock,
   * wrapping, past the harbor while DS:0xfa2 ships are 0 and the dock while DS:0x9e2a units
   * are 0, then refreshes the panels (thunk_FUN_291f_0b18; the port redraws every frame). */
  if (input->last_key == COLONIZE_KEY_TAB) {
    bool ok;
    do {
      eu->focus++;
      if (2 < eu->focus) {
        eu->focus = 0;
      }
      ok = true;
      if (eu->focus == 1) {
        ok = eu->harbor_ships != 0;
      } else if (eu->focus == 2) {
        ok = eu->dock_count != 0;
      }
    } while (!ok);
    return GAME_UPDATE_RETURN_TRUE;
  }

  if (input->last_key == COLONIZE_KEY_R) {
    europe_menu_open(eu, EUROPE_MENU_RECRUIT);
    return GAME_UPDATE_RETURN_TRUE;
  }
  if (input->last_key == COLONIZE_KEY_P) {
    europe_menu_open(eu, EUROPE_MENU_PURCHASE);
    return GAME_UPDATE_RETURN_TRUE;
  }
  if (input->last_key == COLONIZE_KEY_T) {
    europe_menu_open(eu, EUROPE_MENU_TRAIN);
    return GAME_UPDATE_RETURN_TRUE;
  }
  for (int ti = 0; ti < input->text_input_len; ++ti) {
    const char ch = input->text_input[ti];
    if (ch == '1') {
      europe_menu_open(eu, EUROPE_MENU_RECRUIT);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '2') {
      europe_menu_open(eu, EUROPE_MENU_PURCHASE);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '3') {
      europe_menu_open(eu, EUROPE_MENU_TRAIN);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* L / '=' : howmuch buy. U : sell the lowest-key hold (FUN_479b_0f60). */
  if (input->last_key == COLONIZE_KEY_L) {
    if (eu->selected_harbor < 0) {
      snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
    } else {
      game_europe_open_buy_prompt(game);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  if (input->last_key == COLONIZE_KEY_U) {
    /* FUN_38fd_4f6e 38fd:5140-5158: no harbor ship = nothing; one hold per press. */
    game_europe_sell_best_hold(game, eu->selected_harbor);
    return GAME_UPDATE_RETURN_TRUE;
  }
  /* S / Space / Enter with a ship selected (focus mode 1): the ship menu,
   * FUN_38fd_2bfe, not a direct sail (FUN_38fd_4f6e 38fd:5364-5491); its row 2
   * sails through the @SAILAWAY confirm. These keys do not read DS:0xf9a
   * (eu->focus, only F1 does): a selected harbor ship is the ship focus. */
  if (input->last_key == COLONIZE_KEY_S || input->last_key == COLONIZE_KEY_SPACE ||
      input->last_key == COLONIZE_KEY_ENTER) {
    if (eu->selected_harbor >= 0) {
      game_europe_open_ship_menu(game, eu->selected_harbor);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (input->last_key == COLONIZE_KEY_S) {
      snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
      return GAME_UPDATE_RETURN_TRUE;
    }
  }
  for (int ti = 0; ti < input->text_input_len; ++ti) {
    const char ch = input->text_input[ti];
    /* Europe cheat keys, only with head.game_options.cheats_enabled set
     * (FUN_38fd_4f6e raw 66304-66325 gates on DS:0x5383 & 0x20 and edits the
     * human nation record): $ gold +1000, ] tax +1, [ tax -1. */
    if (game_cheats_enabled(game) && (ch == '$' || ch == ']' || ch == '[')) {
      if (ch == '$') {
        europe_cheat_add_gold(eu, 1000);
      } else {
        europe_cheat_adjust_tax(eu, ch == ']' ? 1 : -1);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '=' && eu->selected_harbor >= 0) {
      /* Same as key L. */
      game_europe_open_buy_prompt(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '+' && eu->selected_harbor >= 0) {
      if (game_europe_boycott_gate(game, eu->selected_market)) {
        return GAME_UPDATE_RETURN_TRUE; /* FUN_38fd_4f6e 38fd:51ba-51f3 */
      }
      const ColonizeWorld w = game_world(game);
      europe_buy_cargo_w(&w, game->human_nation, eu->selected_harbor, eu->selected_market, 1);
      game_europe_drain_price_events(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '-' || ch == '_') {
      /* FUN_38fd_4f6e 38fd:5162-51b3. */
      if (eu->harbor_ships <= 0) {
        game_europe_refusal(game, "CMESSAGE", 5); /* 51c1: "Nothing to transport" */
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (game_europe_boycott_gate(game, eu->selected_market)) {
        return GAME_UPDATE_RETURN_TRUE;
      }
      /* '-' sells the whole first slot of the selected cargo, '_' asks the amount. */
      game_europe_sell_first_hold(game, eu->selected_harbor, eu->selected_market, ch == '_');
      return GAME_UPDATE_RETURN_TRUE;
    }
  }
  return GAME_UPDATE_CONTINUE;
}

/* Europe screen mouse: drag cancel/drop and the left-click hit test over the
 * holds, dock, market cells and the three buttons. */
static GameUpdateStep game_europe_screen_mouse(
  ColonizeGameState* game, const ColonizeInputState* input, EuropeScreen* eu
) {
  if (input->mouse_right_clicked && ui_drag_active(&game->ui_drag)) {
    game_ui_drag_clear(game);
    return GAME_UPDATE_RETURN_TRUE;
  }

  if (ui_drag_active(&game->ui_drag) && input->mouse_left_released) {
    game_europe_drag_drop(game, input->mouse_x, input->mouse_y, input->shift_held);
    return GAME_UPDATE_RETURN_TRUE;
  }

  if (input->mouse_left_clicked) {
    if (ui_drag_active(&game->ui_drag)) {
      return GAME_UPDATE_RETURN_TRUE;
    }
    const EuropeHitResult hit = game_europe_hit(game, input->mouse_x, input->mouse_y);
    switch (hit.kind) {
    case EUROPE_HIT_EXIT:
      game_ui_drag_clear(game);
      game->in_europe = false;
      game_europe_deliver_bound_ships(game);
      diag_info("Left Europe screen (Exit).");
      break;
    case EUROPE_HIT_HARBOR_SHIP:
      eu->focus = 1; /* FUN_38fd_2edc raw 61012-61016 */
      eu->selected_harbor = hit.index;
      snprintf(eu->status, sizeof(eu->status), "Selected %s.", eu->harbor[hit.index].name);
      ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_HARBOR_SHIP, hit.index, -1, 0);
      if (game->units_ok && game_icons(game)) {
        const int sprite = europe_ship_icon_sprite(&game->units, &eu->harbor[hit.index]);
        game_ui_drag_set_icon(game, sprite);
      }
      break;
    case EUROPE_HIT_HOLD:
      if (eu->selected_harbor >= 0) {
        EuropeHarborShip* ship = &eu->harbor[eu->selected_harbor];
        if (hit.index >= 0 && hit.index < EUROPE_SHIP_CARGO_MAX &&
            ship->hold_goods_amount[hit.index] > 0 &&
            ship->hold_goods_amount[hit.index] < 255) {
          const int ctype = ship->hold_goods_type[hit.index];
          ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_HOLD, hit.index, -1, 0);
          if (ctype >= 0) {
            game_ui_drag_set_icon(game, EUROPE_CARGO_ICON_BASE + ctype);
          }
        }
      }
      break;
    case EUROPE_HIT_MARKET:
      eu->focus = 0; /* FUN_38fd_3502 raw 61143-61145 */
      eu->selected_market = hit.index;
      if (europe_cargo_boycotted_ex(
            eu, game->col1_ok ? &game->col1 : NULL, game->human_nation, hit.index
          )) {
        /* GAME.TXT @SOMEBOYCOTT: "...click on the cargo type in question"
         * to ask that the boycott be lifted -- pay-back-taxes buyback,
         * not the normal buy/sell flow. DOS (FUN_38fd_2dfe) asks first with
         * the @KISSUP two-row CHOICE and only pays on its second row; see
         * game_europe_ask_boycott_buyback. */
        if (game->col1_ok && game->human_nation >= 0) {
          game_europe_ask_boycott_buyback(game, hit.index);
        }
      } else if (eu->selected_harbor < 0) {
        snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
      } else {
        ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_MARKET, hit.index, -1, 100);
        game_ui_drag_set_icon(game, EUROPE_CARGO_ICON_BASE + hit.index);
      }
      break;
    case EUROPE_HIT_BTN_RECRUIT:
      europe_menu_open(eu, EUROPE_MENU_RECRUIT);
      break;
    case EUROPE_HIT_BTN_PURCHASE:
      europe_menu_open(eu, EUROPE_MENU_PURCHASE);
      break;
    case EUROPE_HIT_BTN_TRAIN:
      europe_menu_open(eu, EUROPE_MENU_TRAIN);
      break;
    case EUROPE_HIT_DOCK:
      eu->focus = 2; /* FUN_38fd_3c86 raw 64102-64104 */
      eu->dock_selected = hit.index;
      eu->menu_dock_index = hit.index;
      europe_build_dock_menu(eu, &game->messages, hit.index);
      europe_menu_open(eu, EUROPE_MENU_DOCK);
      break;
    case EUROPE_HIT_EXPECTED:
      if (hit.index >= 0 && hit.index < eu->expected_ships) {
        ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_EXPECTED_SHIP, hit.index, -1, 0);
        if (game->units_ok && game_icons(game)) {
          const int sprite =
            europe_ship_icon_sprite(&game->units, &eu->expected[hit.index]);
          game_ui_drag_set_icon(game, sprite);
        }
      }
      break;
    case EUROPE_HIT_BOUND:
      if (hit.index >= 0 && hit.index < eu->bound_ships) {
        ui_drag_begin(&game->ui_drag, UI_DRAG_EUROPE_BOUND_SHIP, hit.index, -1, 0);
        if (game->units_ok && game_icons(game)) {
          const int sprite = europe_ship_icon_sprite(&game->units, &eu->bound[hit.index]);
          game_ui_drag_set_icon(game, sprite);
        }
      }
      break;
    default:
      break;
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

COLONIZE_INTERNAL GameUpdateStep game_update_europe_screen(ColonizeGameState* game, const ColonizeInputState* input) {
  if (game->in_europe) {
    EuropeScreen* eu = &game->europe;
    europe_refresh_harbor_selection(eu);
    game_service_europe_bar(game);

    /* Bound ships that ticked to 0 turns since we last checked. */
    for (int i = 0; i < eu->bound_ships; ++i) {
      if (eu->bound[i].turns_left <= 0) {
        game_europe_deliver_bound_ships(game);
        break;
      }
    }

    {
      const GameUpdateStep sub = game_europe_screen_menu_keys(game, input, eu);
      if (sub != GAME_UPDATE_CONTINUE) {
        return sub;
      }
    }

    {
      const GameUpdateStep sub = game_europe_screen_keys(game, input, eu);
      if (sub != GAME_UPDATE_CONTINUE) {
        return sub;
      }
    }

    {
      const GameUpdateStep sub = game_europe_screen_mouse(game, input, eu);
      if (sub != GAME_UPDATE_CONTINUE) {
        return sub;
      }
    }


    /* (No `S` row here: the harbour-sail handler above this block already
     * returns for every S, so this duplicate was dead — and its
     * `selected_harbor >= 0 ? … : 0` fallback would have sailed harbour ship
     * 0 with nothing selected, where the live handler says "Select a ship
     * first.") */
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Pedia screen input. */
/* Leave the Colonizopedia outright (not back to its list). */
static void game_pedia_leave(ColonizeGameState* game) {
  game->in_pedia = false;
  /* Opened from the colony or Europe screen (right-click / F1 help): go back there. */
  if (game->pedia_return_colony_id >= 0 &&
      colonies_get(&game->colonies, game->pedia_return_colony_id) != NULL) {
    game->in_colony = true;
    game->colony_view_id = game->pedia_return_colony_id;
  } else if (game->pedia_return_europe) {
    game->in_europe = true;
  }
  game->pedia_return_colony_id = -1;
  game->pedia_return_europe = false;
  diag_info("Left Colonizopedia.");
}

COLONIZE_INTERNAL GameUpdateStep game_update_pedia_screen(ColonizeGameState* game, const ColonizeInputState* input) {
  if (game->in_pedia) {
    const ColonizeFont* font = game_pedia_font(game);
    if (game->pedia_view == PEDIA_VIEW_LIST) {
      const PediaListHit hover = pedia_list_hit(
        game->pedia_ok ? &game->pedia : NULL,
        game->names_ok ? &game->names : NULL,
        game->pedia_category,
        font,
        input->mouse_x,
        input->mouse_y
      );
      game->pedia_hover_entry =
        (hover.kind == PEDIA_LIST_HIT_ENTRY) ? hover.entry_index : -1;

      if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_P) {
        game->in_pedia = false;
        diag_info("Left Colonizopedia.");
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (input->mouse_left_clicked) {
        const PediaListHit hit = pedia_list_hit(
          game->pedia_ok ? &game->pedia : NULL,
          game->names_ok ? &game->names : NULL,
          game->pedia_category,
          font,
          input->mouse_x,
          input->mouse_y
        );
        if (hit.kind == PEDIA_LIST_HIT_EXIT) {
          game->in_pedia = false;
          diag_info("Left Colonizopedia (Exit).");
          return GAME_UPDATE_RETURN_TRUE;
        }
        if (hit.kind == PEDIA_LIST_HIT_ENTRY) {
          game_open_pedia_article(game, game->pedia_category, hit.entry_index, true);
          return GAME_UPDATE_RETURN_TRUE;
        }
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

    /* Article view: DOS waits for any key or click (FUN_281f_03c0). */
    if (colonize_input_any_key_or_click(input)) {
      if (game->pedia_return_to_list) {
        game_open_pedia_list(game, game->pedia_category);
      } else {
        game_pedia_leave(game);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Sprite atlas debug screen input. */
COLONIZE_INTERNAL GameUpdateStep game_update_debug_atlas_screen(ColonizeGameState* game, const ColonizeInputState* input) {
  if (game->in_debug_atlas) {
    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_TILDE) {
      game->in_debug_atlas = false;
      debug_atlas_free(&game->debug_atlas);
      debug_atlas_init(&game->debug_atlas);
      diag_info("Left sprite atlas debug screen.");
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (input->last_key == COLONIZE_KEY_RIGHT) {
      debug_atlas_next_file(&game->debug_atlas, game->resolved_data_dir, 1);
    } else if (input->last_key == COLONIZE_KEY_LEFT) {
      debug_atlas_prev_file(&game->debug_atlas, game->resolved_data_dir, 1);
    } else if (input->last_key == COLONIZE_KEY_RIGHTBRACKET) {
      debug_atlas_next_file(&game->debug_atlas, game->resolved_data_dir, 10);
    } else if (input->last_key == COLONIZE_KEY_LEFTBRACKET) {
      debug_atlas_prev_file(&game->debug_atlas, game->resolved_data_dir, 10);
    } else if (input->last_key == COLONIZE_KEY_UP) {
      debug_atlas_scroll_by(&game->debug_atlas, -1);
    } else if (input->last_key == COLONIZE_KEY_DOWN) {
      debug_atlas_scroll_by(&game->debug_atlas, 1);
    } else if (input->last_key == COLONIZE_KEY_SPACE || input->last_key == COLONIZE_KEY_ENTER) {
      debug_atlas_page_down(&game->debug_atlas);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Title menu input. */
COLONIZE_INTERNAL GameUpdateStep game_update_title_menu(ColonizeGameState* game, const ColonizeInputState* input) {
  if (game->in_menu) {
    if (!game->ai_popups.open && !game->ai_popups.has_result) {
      ai_popup_try_present_next(&game->ai_popups);
    }
    if (game->ai_popups.open || game->save_load.open) {
      /* Early modal gate should have consumed; keep status refresh only. */
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (input->last_key == COLONIZE_KEY_ESCAPE) {
      game_enqueue_yes_no(
        game, GAME_MAP_CONFIRM_TITLE_EXIT, -1, "DOS", "", NULL
      );
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* @BEGINMENU (FUN_281f_03fe at ND 0x75C64) shows through FUN_6f74_2580: Up / Down wrap. */
    if (!popup_list_key_step(input->last_key, &game->menu_selection, game->menu_option_count, 0) &&
        (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE)) {
      activate_menu_selection(game);
      if (game->elapsed_ms == UINT32_MAX) {
        return GAME_UPDATE_RETURN_FALSE;
      }
    }

    /* Mouse: hover selects; click activates (same pattern as Pick Music). */
    BeginMenuLayout menu_layout;
    if (begin_menu_compute_layout(game, 320, 200, &menu_layout)) {
      const int hit =
        begin_menu_option_at_xy(&menu_layout, input->mouse_x, input->mouse_y);
      if (hit >= 0) {
        game->menu_selection = hit;
        if (input->mouse_left_clicked) {
          activate_menu_selection(game);
          if (game->elapsed_ms == UINT32_MAX) {
            return GAME_UPDATE_RETURN_FALSE;
          }
        }
      }
    }

    /* Skip when activation above left the title menu (e.g. Hall of Fame,
     * Customize/America/New World, LOAD dialog): that screen owns its own
     * status now, and game_update runs every frame so this would otherwise
     * clobber it one frame later even though in_menu is already false. */
    if (game->in_menu && game->menu_option_count > 0 && !game->ai_popups.open) {
      snprintf(
        game->status,
        sizeof(game->status),
        "Menu: %.100s",
        game->menu_options[game->menu_selection]
      );
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Map-screen menu bar (MENU.TXT pull-downs) + mouse map click. */
/* Map screen: mouse click / go-to drag over the map area and the right panel. */
static GameUpdateStep game_map_mouse_click(
  ColonizeGameState* game, const ColonizeInputState* input
) {
  if ((input->mouse_left_clicked || input->mouse_right_clicked || input->mouse_left_released ||
       (ui_drag_active(&game->ui_drag) && game->ui_drag.kind == UI_DRAG_MAP_GOTO &&
        input->mouse_left_down)) &&
      game->world_map_ok) {
    const int tile_w = game_map_zoom_tile_px(game->map_zoom);
    const int tile_h = tile_w;
    const int map_origin_x = 0;
    const int map_origin_y = MAP_VIEW_ORIGIN_Y;
    int view_cols = 0;
    int view_rows = 0;
    game_map_zoom_view_size(game->map_zoom, &view_cols, &view_rows);
    if (input->mouse_y < map_origin_y) {
      if (input->mouse_left_released && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
        game_ui_drag_clear(game);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

    int view_x = 0;
    int view_y = 0;
    map_panel_clamp_view_origin(
      (int)game->world_map.width,
      (int)game->world_map.height,
      game->map_view_x,
      game->map_view_y,
      view_cols,
      view_rows,
      &view_x,
      &view_y
    );

    /* Right panel / minimap: left-click centers the view on the clicked tile. */
    if (map_panel_contains_xy(input->mouse_x, input->mouse_y)) {
      if (game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
        if (input->mouse_left_released || input->mouse_right_clicked) {
          game_ui_drag_clear(game);
        }
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (input->mouse_left_clicked) {
        int tx = 0;
        int ty = 0;
        if (map_panel_minimap_click(
              &game->world_map,
              view_x,
              view_y,
              view_cols,
              view_rows,
              input->mouse_x,
              input->mouse_y,
              &tx,
              &ty
            )) {
          /* port_bugs.md #285: a minimap pan while a unit is active must hold,
           * exactly like a main-map pan — suspend that unit's off-screen
           * auto-recentre until its next action / hand-off. */
          if (game->units_ok && game->units.selected_id >= 0) {
            game->view_pan_hold_unit = game->units.selected_id;
          }
          game_set_view_center(game, tx, ty);
        } else if (game_end_turn_prompt_active(game)) {
          /* DOS FUN_2b5a_3752: while the End of Turn prompt is up the next
           * click IS the confirmation — the handler tests DS:0x53c6 before
           * it dispatches the click anywhere else. */
          game_do_end_turn(game);
        } else if (game->units_ok && game_units_pending_orders(game)) {
          /* port_bugs.md: mid-turn (units still in the control queue) a sidebar
           * click hands control to the next waiting unit instead of doing
           * nothing. */
          game_wait_next_unit(game);
        }
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

    if (input->mouse_x >= MAP_PANEL_X) {
      if (input->mouse_left_released && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
        game_ui_drag_clear(game);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

    const int mx = view_x + (input->mouse_x - map_origin_x) / tile_w;
    const int my = view_y + (input->mouse_y - map_origin_y) / tile_h;
    if (!map_coords_inset(&game->world_map, mx, my)) {
      if (input->mouse_left_released && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
        game_ui_drag_clear(game);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

    /* Active map go-to drag: track / cancel / commit. */
    if (game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
      if (mx != game->map_goto_anchor_x || my != game->map_goto_anchor_y) {
        game->map_goto_left_tile = true;
      }
      if (input->mouse_right_clicked) {
        game_ui_drag_clear(game);
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (input->mouse_left_released) {
        const int uid = game->ui_drag.unit_id;
        const ColonizeUnit* u = game->units_ok ? units_get_const(&game->units, uid) : NULL;
        game_ui_drag_clear(game);
        if (!u || !game_unit_selectable(game, u)) {
          return GAME_UPDATE_RETURN_TRUE;
        }
        if (!game->map_goto_left_tile) {
          /* Short click (press and release on the same tile) is not a go-to:
           * it means whatever a plain click on that tile means — open a
           * colony, or make another of our units the active one.
           * port_bugs.md: unless the pointer clearly DRAGGED (≥4 logical px)
           * within the tile — that was an intended go-to whose pull didn't
           * cross a tile boundary, and it kept opening the colony under
           * the pointer instead of ordering the ship there. */
          const int pdx = input->mouse_x - game->map_goto_down_px;
          const int pdy = input->mouse_y - game->map_goto_down_py;
          if (pdx * pdx + pdy * pdy < 16) {
            game_map_click_dispatch(game, mx, my);
            return GAME_UPDATE_RETURN_TRUE;
          }
        }
        if (mx == u->x && my == u->y) {
          return GAME_UPDATE_RETURN_TRUE;
        }
        const int rc = game_issue_goto(game, uid, mx, my);
        if (rc == 0) {
          /* Movement is paced in game_update (10 steps/sec). */
          snprintf(game->status, sizeof(game->status), "Go to (%d,%d)", mx, my);
          game_set_view_center(game, u->x, u->y);
        } else if (rc < 0) {
          set_status(game, "Cannot go there", NULL);
        }
        return GAME_UPDATE_RETURN_TRUE;
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

    if (input->mouse_right_clicked) {
      /* Right-click: unselect unit (if any) and select the tile. */
      game_select_tile(game, mx, my);
      return GAME_UPDATE_RETURN_TRUE;
    }

    if (!input->mouse_left_clicked) {
      return GAME_UPDATE_RETURN_TRUE;
    }

    /*
     * port_bugs.md: a click on the map viewport is NEVER an End of Turn
     * confirmation, even while the prompt blinks — it keeps its normal
     * meaning (open a colony, pick a unit, start a go-to drag). Only the
     * right-hand sidebar click (above) confirms the turn; Enter/Space do too.
     */

    /* Left-click — with a selected unit, begin go-to drag. */
    if (game->units.selected_id >= 0 && game->units_ok) {
      const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
      if (game_unit_selectable(game, sel)) {
        ui_drag_begin(
          &game->ui_drag, UI_DRAG_MAP_GOTO, mx, game->units.selected_id, my
        );
        game->map_goto_anchor_x = mx;
        game->map_goto_anchor_y = my;
        game->map_goto_left_tile = false;
        game->map_goto_down_px = input->mouse_x;
        game->map_goto_down_py = input->mouse_y;
        game->map_goto_dragged_px = false;
        return GAME_UPDATE_RETURN_TRUE;
      }
    }

    game_map_click_dispatch(game, mx, my);
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

COLONIZE_INTERNAL GameUpdateStep game_update_map_menu_bar(ColonizeGameState* game, const ColonizeInputState* input) {
  /* Map-screen menu bar (MENU.TXT pull-downs) + mouse map click. */
  if (!game->in_colony && !game->in_europe && !game->in_pedia && !game->in_debug_atlas &&
      !game->in_report) {
    /*
     * VIEW ~Hidden Terrain (H): auto-advance phases 1→2→3 on a timer, then
     * hold at 3 until any click/keypress cancels back to the normal view.
     * Simplification vs. DOS: any input during the brief auto-peel (not just
     * once resting at 3) also cancels — equivalent information, less state.
     */
    if (game->terrain_peel_phase != 0) {
      const bool any_input = input->mouse_left_clicked || input->mouse_right_clicked ||
        input->last_key != COLONIZE_KEY_NONE;
      if (any_input) {
        game->terrain_peel_phase = 0;
        set_status_menu_row(game, "VIEW", 9, "off");
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (game->terrain_peel_phase < 3 &&
          game->elapsed_ms - game->hidden_terrain_phase_ms >= HIDDEN_TERRAIN_STEP_MS) {
        game->terrain_peel_phase++;
        game->hidden_terrain_phase_ms = game->elapsed_ms;
        if (game->terrain_peel_phase == 2) {
          set_status_menu_row(game, "VIEW", 9, "roads, resources hidden");
        } else {
          set_status_menu_row(game, "VIEW", 9, "hills and forest hidden");
        }
      }
    }

    /* Go-To cursor only after ≥1 logical pixel (even if pointer leaves the map). */
    if (game->ui_drag.kind == UI_DRAG_MAP_GOTO && !game->map_goto_dragged_px) {
      const int pdx = input->mouse_x - game->map_goto_down_px;
      const int pdy = input->mouse_y - game->map_goto_down_py;
      if (pdx != 0 || pdy != 0) {
        game->map_goto_dragged_px = true;
        if (game->cursor_ok && game->cursor.sprite_count > 1) {
          ui_drag_set_cursor_from_sheet(&game->ui_drag, &game->cursor, 1);
          game->ui_drag.hotspot_x = 1;
          game->ui_drag.hotspot_y = 0;
        }
      }
    }

    /* Drop selection if the active unit no longer has moves (e.g. after load). */
    if (game->units_ok && game->units.selected_id >= 0) {
      const ColonizeUnit* sel = units_get_const(&game->units, game->units.selected_id);
      if (!game_unit_selectable(game, sel)) {
        const int tx = sel ? sel->x : game->map_cursor_x;
        const int ty = sel ? sel->y : game->map_cursor_y;
        game_select_tile(game, tx, ty);
      }
    }

    /* CHEAT title follows head.game_options.cheats_enabled, also after a load
     * or new game (FUN_74a4_0000 hides menu 6 unless the bit is set,
     * FUN_75c2_0940 shows it for a loaded save that has it). */
    if (game->map_menu.cheat_visible != game_cheats_enabled(game)) {
      map_menu_set_cheat_visible(&game->map_menu, game_cheats_enabled(game));
    }
    /* FUN_2b5a_32a2 (raw 45840) zeroes the Alt-WIN counter DS:0xb92 on every
     * key that is not the next letter of the sequence. */
    const bool cheat_alt_key = input->alt_held &&
      (input->last_key == COLONIZE_KEY_W || input->last_key == COLONIZE_KEY_I ||
       input->last_key == COLONIZE_KEY_N);
    if (input->last_key != COLONIZE_KEY_NONE && !cheat_alt_key) {
      game->cheat_unlock_step = 0;
    }

    /*
     * F1 terrain pedia at cursor; F2–F10 adviser / report screens. Shift-Fn =
     * CHEAT menu hotkeys when unlocked, per MENU.TXT @CUP: ~F~0~1 Create
     * Unit, ~F~0~2 Debug Info Flags, ~F~0~4 Reveal Map, ~F~0~5 Set Human
     * Player, ~F~0~6 Kill Indians, ~F~0~7 Advance Revolution Status,
     * ~F~0~8 Show Strategy, ~F~0~9 Show Colony Sites, ~F~1~0 Test Routine.
     * Sound Test / Memory Check have no @CUP hotkey (menu-only).
     */
    if (game->map_menu.open_index < 0 &&
        input->last_key >= COLONIZE_KEY_F1 && input->last_key <= COLONIZE_KEY_F10) {
      if (input->shift_held && game_cheats_enabled(game)) {
        switch (input->last_key) {
          case COLONIZE_KEY_F1:
            game_open_cheat_create_unit(game);
            return GAME_UPDATE_RETURN_TRUE;
          case COLONIZE_KEY_F2:
            game_open_cheat_debug_flags(game);
            return GAME_UPDATE_RETURN_TRUE;
          case COLONIZE_KEY_F4:
            game_open_cheat_setview(game);
            return GAME_UPDATE_RETURN_TRUE;
          case COLONIZE_KEY_F5:
            game_open_cheat_set_human(game);
            return GAME_UPDATE_RETURN_TRUE;
          case COLONIZE_KEY_F6:
            game_open_cheat_kill_indians(game);
            return GAME_UPDATE_RETURN_TRUE;
          case COLONIZE_KEY_F7:
            game_cheat_advance_revolution(game);
            return GAME_UPDATE_RETURN_TRUE;
          case COLONIZE_KEY_F8:
            game_cheat_toggle_strategy(game);
            return GAME_UPDATE_RETURN_TRUE;
          case COLONIZE_KEY_F9:
            game_cheat_toggle_colony_sites(game);
            return GAME_UPDATE_RETURN_TRUE;
          case COLONIZE_KEY_F10:
            game_cheat_test_routine(game);
            return GAME_UPDATE_RETURN_TRUE;
          default:
            break;
        }
      }
      game_handle_report_fkey(game, input->last_key);
      return GAME_UPDATE_RETURN_TRUE;
    }

    /* Alt-W/I/N cheat toggle: FUN_2b5a_3094 (raw 45007; key dispatcher
     * FUN_2b5a_3252 routes Alt-W 0x111, Alt-I 0x117, Alt-K 0x125, Alt-N 0x131
     * to it; Alt-K has no ColonizeKey here). Bit set: any of them clears it.
     * Bit clear: DS:0xb92 advances on the exact next key (W, I, N) and the
     * N that finishes it sets the bit. Show/hide of the CHEAT title follows
     * the bit. Other Alt+letter opens map menu titles (~GAME, ~VIEW, …). */
    if (input->alt_held && input->last_key != COLONIZE_KEY_NONE) {
      const ColonizeKey k = input->last_key;
      if (cheat_alt_key) {
        if (game_cheats_enabled(game)) {
          game->col1.head.game_options.cheats_enabled = 0;
          game->cheat_unlock_step = 0;
        } else if (
          (game->cheat_unlock_step == 0 && k == COLONIZE_KEY_W) ||
          (game->cheat_unlock_step == 1 && k == COLONIZE_KEY_I)
        ) {
          ++game->cheat_unlock_step;
        } else if (game->cheat_unlock_step == 2 && k == COLONIZE_KEY_N && game->col1_ok) {
          game->col1.head.game_options.cheats_enabled = 1;
          game->cheat_unlock_step = 0;
        } else {
          game->cheat_unlock_step = 0;
        }
        map_menu_set_cheat_visible(&game->map_menu, game_cheats_enabled(game));
        return GAME_UPDATE_RETURN_TRUE;
      }
      const char alt_letter = game_key_letter(k);
      if (alt_letter && map_menu_open_alt_hotkey(&game->map_menu, alt_letter)) {
        return GAME_UPDATE_RETURN_TRUE;
      }
    }

    game_refresh_orders_menu(game);

    const ColonizeFont* menu_font = game->colony_font_ok ? &game->colony_font :
                                    (game->menu_font_ok ? &game->menu_font : NULL);
    const bool menu_was_open = game->map_menu.open_index >= 0;
    if (input->last_key == COLONIZE_KEY_ESCAPE && menu_was_open) {
      ColonizeInputState empty;
      memset(&empty, 0, sizeof(empty));
      map_menu_handle_input(&game->map_menu, &empty, menu_font, true);
      return GAME_UPDATE_RETURN_TRUE;
    }

    const bool click_on_menu_ui =
      input->mouse_left_clicked &&
      map_menu_hit_ui(&game->map_menu, input->mouse_x, input->mouse_y);
    const MapMenuAction menu_action =
      map_menu_handle_input(&game->map_menu, input, menu_font, false);
    if (menu_action != MAP_MENU_ACTION_NONE) {
      if (menu_action == MAP_MENU_ACTION_UNIMPLEMENTED) {
        set_status(game, "Not implemented yet", NULL);
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (!game_apply_map_menu_action(game, menu_action)) {
        return GAME_UPDATE_RETURN_FALSE;
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

    if (input->mouse_left_clicked && (click_on_menu_ui || menu_was_open)) {
      /* Menu open/close consumed the click. */
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (menu_was_open) {
      /* port_bugs.md #1091: plain ORDERS/VIEW hotkeys and map clicks never reach
       * the map behind an open pull-down. */
      return GAME_UPDATE_RETURN_TRUE;
    }

    /* FUN_2b5a_303c raw 44948-44955 (0x11 Ctrl-Q, 0x18 Ctrl-X), FUN_2b5a_321c
     * (Alt-Q 0x110) and FUN_2b5a_3252 (Alt-X 0x12d) go to 2b5a:3104, the
     * @DOSYES quit confirm Esc also asks (game_update_map_keys). Ahead of the
     * plain hotkeys: Ctrl-X is key 0x18 in DOS, not VIEW's ~X. port_bugs.md #1257. */
    if ((input->ctrl_held || input->alt_held) &&
        (input->last_key == COLONIZE_KEY_Q || input->last_key == COLONIZE_KEY_X)) {
      game_enqueue_yes_no(game, GAME_MAP_CONFIRM_QUIT, -1, "DOSYES", "", NULL);
      return GAME_UPDATE_RETURN_TRUE;
    }

    /* Plain ORDERS hotkeys (same as choosing the enabled ORDERS item). */
    if (!input->alt_held && input->last_key != COLONIZE_KEY_NONE) {
      const bool space = (input->last_key == COLONIZE_KEY_SPACE);
      const char letter = space ? 0 : game_key_letter(input->last_key);
      const MapMenuAction order_hk = map_menu_orders_hotkey(
        &game->map_menu, letter, input->shift_held, space
      );
      if (order_hk != MAP_MENU_ACTION_NONE) {
        if (!game_apply_map_menu_action(game, order_hk)) {
          return GAME_UPDATE_RETURN_FALSE;
        }
        return GAME_UPDATE_RETURN_TRUE;
      }
      /*
       * Plain VIEW hotkeys (Zoom In ~Z / Zoom Out ~X, Show ~Hidden Terrain —
       * MENU.TXT). H is contextually overloaded: with a ship selected it
       * sails to Europe (below, unconditional on world/units/europe_ok);
       * otherwise it opens the Hidden Terrain reveal.
       */
      const bool ship_selected = game->units_ok && game->units.selected_id >= 0 &&
        units_is_sea(&game->units, game->units.selected_id);
      if (!space && letter && !(letter == 'H' && ship_selected)) {
        const MapMenuAction view_hk = map_menu_view_hotkey(&game->map_menu, letter);
        if (view_hk != MAP_MENU_ACTION_NONE) {
          if (!game_apply_map_menu_action(game, view_hk)) {
            return GAME_UPDATE_RETURN_FALSE;
          }
          return GAME_UPDATE_RETURN_TRUE;
        }
      }
    }

    /* Port-only: wheel forward = Zoom In, back = Zoom Out (VIEW menu actions). */
    if (input->mouse_wheel_y != 0 && input->mouse_y >= MAP_VIEW_ORIGIN_Y) {
      if (!game_apply_map_menu_action(
            game, input->mouse_wheel_y > 0 ? MAP_MENU_ACTION_ZOOM_IN : MAP_MENU_ACTION_ZOOM_OUT
          )) {
        return GAME_UPDATE_RETURN_FALSE;
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

    {
      const GameUpdateStep sub = game_map_mouse_click(game, input);
      if (sub != GAME_UPDATE_CONTINUE) {
        return sub;
      }
    }
  }
  return GAME_UPDATE_CONTINUE;
}

/* Map-screen keyboard commands (cursor, orders, hotkeys). */
COLONIZE_INTERNAL GameUpdateStep game_update_map_keys(ColonizeGameState* game, const ColonizeInputState* input) {
  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    if (ui_drag_active(&game->ui_drag) && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
      game_ui_drag_clear(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* FUN_2b5a_303c (raw 44956-44969) -> 2b5a:3104: Esc asks @DOSYES (DS:0x0b8b,
     * default Yes); Yes clears DS:0x53c2 and FUN_130d_0290 returns to DOS. Move and
     * View Pieces share this dispatcher (FUN_2b5a_3802 raw 46315). Playtest C2-B1:
     * the port used to drop to the title menu with no confirm. */
    game_enqueue_yes_no(game, GAME_MAP_CONFIRM_QUIT, -1, "DOSYES", "", NULL);
    return GAME_UPDATE_RETURN_TRUE;
  }

  if (input->last_key == COLONIZE_KEY_SPACE) {
    /* No Orders: spend the selected unit's remaining moves (it won't be
     * offered again until next turn's refresh), then cycle to the next
     * human unit still needing orders, ending the turn once none remain.
     * Mirrors the MAP_MENU_ACTION_NO_ORDERS case, which is the actually
     * reachable path for a physical Space press (map_menu_orders_hotkey
     * claims it first) — kept in sync here as a defensive fallback should
     * this plain-key branch ever become reachable. Must NOT reduce to a
     * bare game_wait_next_unit() call: that is W/Wait's semantics (defer
     * without spending moves), not Space's. */
    /* port_bugs.md: with the End of Turn prompt flashing (View Pieces, nothing
     * left in the control queue), Space confirms the turn like the sidebar
     * click does in DOS. */
    if (game_end_turn_prompt_active(game)) {
      game_do_end_turn(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    ColonizeUnit* u =
      game->units.selected_id >= 0 ? units_get(&game->units, game->units.selected_id) : NULL;
    if (u) {
      u->moves = 0;
    }
    game_wait_next_unit(game);
    return GAME_UPDATE_RETURN_TRUE;
  }

  const int map_min_x = 1;
  const int map_min_y = 1;
  const int map_max_x = game->world_map_ok ? (int)game->world_map.width - 2 : 15;
  const int map_max_y = game->world_map_ok ? (int)game->world_map.height - 2 : 15;

  if (input->last_key == COLONIZE_KEY_ENTER && game->world_map_ok) {
    const int cid = colonies_id_at(&game->colonies, game->map_cursor_x, game->map_cursor_y);
    if (cid >= 0) {
      const ColonizeColony* col = colonies_get(&game->colonies, cid);
      if (col && col->nation_id == game->human_nation) {
        game_enter_colony_at_cursor(game);
        return GAME_UPDATE_RETURN_TRUE;
      }
    }
    if (!game->units_ok) {
      return GAME_UPDATE_RETURN_TRUE;
    }
    const int at_cursor = game_owned_unit_at(game, game->map_cursor_x, game->map_cursor_y);
    if (at_cursor >= 0 && at_cursor != game->units.selected_id) {
      game_select_unit(game, at_cursor);
    } else if (game->units.selected_id >= 0) {
      ColonizeUnit* selected = units_get(&game->units, game->units.selected_id);
      if (selected &&
          (selected->x != game->map_cursor_x || selected->y != game->map_cursor_y)) {
        if (game_try_unit_move(game, game->map_cursor_x, game->map_cursor_y)) {
          return GAME_UPDATE_RETURN_TRUE;
        }
      } else if (at_cursor >= 0) {
        game_select_unit(game, at_cursor);
      }
    } else if (at_cursor >= 0) {
      game_select_unit(game, at_cursor);
    } else if (game_end_turn_prompt_active(game)) {
      /* port_bugs.md: View Pieces, no own colony/unit on the tile, control queue
       * empty (End of Turn flashing) — Enter confirms the turn, same as the
       * sidebar click. */
      game_do_end_turn(game);
      return GAME_UPDATE_RETURN_TRUE;
    } else {
      /* Enter on exhausted human unit / empty tile → tile select. */
      const int any_id = units_id_at(&game->units, game->map_cursor_x, game->map_cursor_y);
      const ColonizeUnit* any = units_get_const(&game->units, any_id);
      if (any && any->nation_id == game->human_nation) {
        game_select_tile(game, any->x, any->y);
      }
    }
  }

  /* H: sail selected ship to Europe from high seas (passengers ride along). */
  if (input->last_key == COLONIZE_KEY_H && game->world_map_ok && game->units_ok && game->europe_ok) {
    game_order_return_europe(game);
  }

  /* O: board land unit onto adjacent ship (selected land↔cursor ship, or vice versa). */
  if (input->last_key == COLONIZE_KEY_O && game->world_map_ok && game->units_ok) {
    game_order_board(game);
  }

  /* U: unload oldest passenger from selected ship onto cursor land tile. */
  if (input->last_key == COLONIZE_KEY_U && game->world_map_ok && game->units_ok) {
    game_order_unload(game);
  }

  /* F: fortify the selected unit — the keyboard twin of the two MENU.TXT
   * "~Fortify" rows (see game_order_fortify). Unlike the menu rows this one
   * leaves the status line alone when the order does not take, the way it
   * always has: an F with nothing selected is not a refusal to report. */
  if (input->last_key == COLONIZE_KEY_F && game->world_map_ok && game->units_ok) {
    const int uid = game->units.selected_id;
    /* port_bugs.md #691: the key twin runs the same @HAVETREATY scan as the rows. */
    if (game_fortify_treaty_confirm(game, uid)) {
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (uid >= 0 && units_order_fortify(&game->units, uid)) {
      set_status(
        game, units_is_sea(&game->units, uid) ? "Anchoring in harbor" : "Fortifying", NULL
      );
      game_wait_next_unit(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* Shift+D: disband selected unit (MENU.TXT `~s~h~i~f~t~-~D`); plain D is no key in DOS. */
  if (input->last_key == COLONIZE_KEY_D && input->shift_held && game->units_ok) {
    game_request_disband_confirm(game);
    return GAME_UPDATE_RETURN_TRUE;
  }

  /* B: found a colony — free on tribal land (@INDIANLAND is the colony-screen plot drop, #1175). */
  if (input->last_key == COLONIZE_KEY_B && game->world_map_ok) {
    (void)game_try_found_colony_at_cursor(game);
  }

  {
    int dx = 0;
    int dy = 0;
    if (game_key_move_delta(input->last_key, &dx, &dy)) {
      if (game->units.selected_id >= 0 && game->world_map_ok && game->units_ok) {
        ColonizeUnit* selected = units_get(&game->units, game->units.selected_id);
        if (selected && selected->moves > 0) {
          const int dest_x = selected->x + dx;
          const int dest_y = selected->y + dy;
          game_try_unit_move(game, dest_x, dest_y);
        } else if (selected) {
          /* Safety: exhausted selection → tile mode. */
          game_select_tile(game, selected->x, selected->y);
        }
      } else {
        int nx = game->map_cursor_x + dx;
        int ny = game->map_cursor_y + dy;
        if (nx < map_min_x) {
          nx = map_min_x;
        }
        if (ny < map_min_y) {
          ny = map_min_y;
        }
        if (nx > map_max_x) {
          nx = map_max_x;
        }
        if (ny > map_max_y) {
          ny = map_max_y;
        }
        game->map_cursor_x = nx;
        game->map_cursor_y = ny;
        game_set_view_center(game, game->map_cursor_x, game->map_cursor_y);
      }
    }
  }

  if (input->last_key == COLONIZE_KEY_S) {
    /*
     * Selected map unit → Sentry; with no unit selected S does nothing
     * (Save is on the GAME menu only).
     *
     * Ships sentry too. DOS's Move Pieces menu builder FUN_2b5a_0b34
     * (viceroy_unpacked.c:42164-42265) hides or disables rows 0x302/0x303
     * (Fortify vs ship Anchor), 0x310/0x311, 0x312-0x314, 0x315/0x316,
     * 0x321-0x323 and 0x331 by domain and terrain — the Sentry row is not
     * touched anywhere in it, i.e. it is never domain-gated, and
     * units_set_orders (units.c:7564) already accepts SENTRY for a ship on
     * the map. Excluding sea units here meant S with a ship selected fell
     * through and opened the Save dialog instead.
     */
    if (game->units_ok && game->world_map_ok) {
      const int uid = game->units.selected_id;
      const ColonizeUnit* su = units_get_const(&game->units, uid);
      if (su && su->active && units_is_on_map(su)) {
        if (units_order_sentry(&game->units, uid)) {
          set_status(game, "", NULL);
          game_wait_next_unit(game);
          return GAME_UPDATE_RETURN_TRUE;
        }
      }
    }
    /* No unit to sentry: DOS does nothing. MENU.TXT Save Game / Load Game carry
     * no `~` hotkey, so FUN_2b5a_3802 passes S (and L with Load Cargo off) to
     * FUN_2b5a_303c -> FUN_2b5a_321c's default arm (raw 45273-45351). */
    return GAME_UPDATE_RETURN_TRUE;
  }

  if (game->assets_ok) {
    snprintf(
      game->status,
      sizeof(game->status),
      "Turn %u Cursor %d,%d",
      game->turn_number,
      game->map_cursor_x,
      game->map_cursor_y
    );
  }
  return GAME_UPDATE_CONTINUE;
}

bool game_update(ColonizeGameState* game, const ColonizeInputState* input, uint32_t dt_ms) {
  GameUpdateStep step;
  if (!game || !input) {
    return false;
  }

  /*
   * Port-only: a DOS-sized screen is centred in a larger window, so shift the
   * pointer into its own 320x200 space before anything hit-tests it. The map
   * view reports a zero offset and is unaffected.
   */
  ColonizeInputState shifted;
  int screen_off_x = 0;
  int screen_off_y = 0;
  game_screen_offset(&screen_off_x, &screen_off_y);
  if (screen_off_x != 0 || screen_off_y != 0) {
    shifted = *input;
    shifted.mouse_x -= screen_off_x;
    shifted.mouse_y -= screen_off_y;
    input = &shifted;
  }

  /*
   * Port-only: a left click on the map around a screen drawn over it closes
   * that screen. Reports and the Colonizopedia close outright (their Escape
   * pages on / steps back to the list); every other screen gets an Escape, so
   * an open sub-panel, list or drag goes first, exactly as with the key. An
   * open popup keeps the click (it is modal over the screen too).
   */
  if (input == &shifted && game_screen_on_map() && shifted.mouse_left_clicked &&
      !game_modal_over_screen_open(game) &&
      (shifted.mouse_x < 0 || shifted.mouse_y < 0 || shifted.mouse_x >= SCREEN_BASE_W ||
       shifted.mouse_y >= SCREEN_BASE_H)) {
    shifted.mouse_left_clicked = false;
    if (game->in_report) {
      game_report_leave(game);
    } else if (game->in_pedia) {
      game_pedia_leave(game);
    } else {
      shifted.last_key = COLONIZE_KEY_ESCAPE;
      shifted.text_input_len = 0;
    }
  }

  ColonizeInputState autoplay_scratch;
  input = game_autoplay_filter_input(game, input, &autoplay_scratch);
  if (game->elapsed_ms == UINT32_MAX) {
    return false;
  }

  step = game_update_services(game, input, dt_ms);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  step = game_update_unit_pacer(game, input, dt_ms);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  if (input->last_key != COLONIZE_KEY_NONE) {
    diag_info(
      "Key pressed: %s (menu=%s debug=%s pedia=%s europe=%s colony=%s report=%s turn=%u cursor=%d,%d)",
      key_name(input->last_key),
      game->in_menu ? "yes" : "no",
      game->in_debug_atlas ? "yes" : "no",
      game->in_pedia ? "yes" : "no",
      game->in_europe ? "yes" : "no",
      game->in_colony ? "yes" : "no",
      game->in_report ? "yes" : "no",
      game->turn_number,
      game->map_cursor_x,
      game->map_cursor_y
    );
  }

  if (game_handle_modal_input(game, input)) {
    if (game->elapsed_ms == UINT32_MAX) {
      return false;
    }
    return true;
  }

  /*
   * Replay a unit hand-off / end-of-turn that was parked because a popup or
   * another screen owned the display when it came due (game_defer_turn_flow).
   * This is the async stand-in for DOS returning from a blocking dialog and
   * simply continuing where it left off.
   */
  if (game->turn_flow_deferred && game_turn_flow_allowed(game)) {
    game->turn_flow_deferred = false;
    game_wait_next_unit(game);
  }

  /*
   * port_bugs.md #1091: an open map pull-down is DOS's blocking menu loop. Only
   * the menu itself (click, Esc, Alt-letter title switch) gets the frame;
   * no map / report / screen hotkey may act behind it.
   */
  if (game->map_menu.loaded && game->map_menu.open_index >= 0) {
    return game_update_map_menu_bar(game, input) != GAME_UPDATE_RETURN_FALSE;
  }

  /* Retire exploits screen (FUN_41f2_0b70) and Hall of Fame (FUN_41f2_0f56) both end in
   * the FUN_281f_03c0 key-or-click wait, so any key
   * or either mouse button advances. */
  if (game->in_exploits) {
    if (colonize_input_any_key_or_click(input)) {
      game->in_exploits = false;
      game->in_hall_of_fame = true;
      set_status(game, reports_misc_display_word(192, ""), "Any key or click returns to menu");
    }
    return true;
  }

  /* Title-menu Hall of Fame screen (reports_render_hall_of_fame). */
  if (game->in_hall_of_fame) {
    if (colonize_input_any_key_or_click(input)) {
      game->in_hall_of_fame = false;
      if (game->scored_after_hof) {
        /* WoI win (DOS main-loop 0x104 block) or the calendar end (FUN_3844_0442
         * LAB_3844_0b4a, raw 58667-58676): the score chain ends in @SCORED "That's
         * all." / "Keep playing anyway." instead of the menu. */
        game->scored_after_hof = false;
        game_enqueue_war_scored_choice(game);
      } else {
        game->in_menu = true;
        sound_set_bgm(SOUND_TITLE_BGM_POOL);
        set_status(game, "OpenCol", NULL);
      }
    }
    return true;
  }

  /* F10 score plate (FUN_41f2_0092, raw 71403-71407): no OK button, the same FUN_281f_03c0
   * wait, and the main loop drops the key that ended it (raw 55683-55690, no re-dispatch).
   * So any key (F-keys too) or either button closes it. */
  if (game->in_report && game->report_id == COLONIZE_REPORT_SCORE) {
    if (colonize_input_any_key_or_click(input)) {
      game_report_leave(game);
    }
    return true;
  }

  step = game_update_report_screen(game, input);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  step = game_update_colony_screen(game, input);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  step = game_update_europe_screen(game, input);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  step = game_update_pedia_screen(game, input);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  step = game_update_debug_atlas_screen(game, input);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  if (input->last_key == COLONIZE_KEY_TILDE) {
    game_open_debug_atlas(game);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_P) {
    if (game_key_pioneer_order(game, units_pioneer_plow_w, GAME_INDIAN_LAND_FOREST)) {
      return true;
    }
  }

  if (input->last_key == COLONIZE_KEY_R) {
    if (game_key_pioneer_order(game, units_pioneer_road_w, GAME_INDIAN_LAND_ROAD)) {
      return true;
    }
  }

  if (input->last_key == COLONIZE_KEY_E && !game->in_menu) {
    if (game_try_enter_europe(game)) {
      diag_info("Entered Europe screen.");
    }
    return true;
  }

  step = game_update_title_menu(game, input);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  step = game_update_map_menu_bar(game, input);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  step = game_update_map_keys(game, input);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }
  return true;
}
