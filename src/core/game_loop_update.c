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
   * bugs.md #262). Modals/popups are deliberately NOT in this set: DOS pushes
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
 * DOS status line (bugs.md #369). FUN_1009_00b4 is a BLOCKING call: it holds
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
  if (game_screen_owns_display(game) || game->woodcut.open || game->ai_popups.open ||
      game_modal_open(game)) {
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
  /* OPENING.EXE exits here in DOS, which also ends its 0x34 music. */
  sound_stop_bgm();
}

bool game_try_start_intro(ColonizeGameState* game) {
  if (!game) {
    return false;
  }
  if (game->opening.open) {
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
 *    (bugs.md #262 — a colony zoom / Europe / report / pedia freezes the whole
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
                  : "cargo";
  tok.string1 = "ship";
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
  const int room = europe_harbor_cargo_room(eu, &game->units, hidx, cargo);
  const int max_amt = room < 100 ? room : 100;
  char prompt[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = (cargo >= 0 && cargo < eu->cargo_count) ? eu->cargo[cargo].name : "cargo";
  tok.string1 = "ship";
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

static bool game_key_pioneer_order(ColonizeGameState* game, GamePioneerOrderFn fn) {
  if (game_screen_owns_display(game) || !game->world_map_ok || !game->units_ok) {
    return false;
  }
  const int sid = game->units.selected_id;
  const ColonizeUnit* su = units_get_const(&game->units, sid);
  if (!su || !units_is_pioneer(&game->units, sid) || su->moves <= 0) {
    return false;
  }
  char msg[96];
  ColonizeWorld w = world_make(
    &game->units, &game->colonies, &game->world_map, NULL, false, NULL, NULL
  );
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
  sound_service();
  if (game_service_opening(game, input, dt_ms)) {
    return GAME_UPDATE_RETURN_TRUE;
  }
  game_water_cycle_tick(game);
  ai_popup_set_now_ms(game->elapsed_ms); /* King flair animation clock */
  /* bugs.md: while independence is declared, the Crown's borrowed nation
   * slot renders WHITE (REF), not the peer's own colour. */
  unit_chrome_set_crown_nation(
    game->col1_ok && ai_king_independence_declared(&game->col1)
      ? ai_king_crown_nation_col1(&game->col1, game->human_nation)
      : -1
  );
  /* bugs.md: WoI colony flags — rebel colonies fly the American flag, crown
   * captures fly the player's original nation color. */
  unit_chrome_set_rebel_nation(
    game->col1_ok && ai_king_independence_declared(&game->col1) ? game->human_nation : -1
  );
  /* bugs.md #234: the move/combat watches were only armed during end-of-turn
   * processing, so the PLAYER's own moves fired into a NULL watch and never
   * animated. Arm them every frame; the callbacks guard platform/zoom
   * themselves. (The moving unit's overlay is blitted after game_render, so
   * it also draws on top of any stack it passes — bugs.md #227.) */
  units_set_move_watch(game_move_watch, game);
  units_set_combat_watch(game_combat_watch, game);
  units_set_combat_dissolve(game_combat_dissolve, game);
  units_set_combat_popup_pump(game_combat_popup_pump, game);
  /* Same "arm it every frame" rule as the watches: founding and abandoning a
   * colony have to keep layer2's settlement bit current wherever they are
   * reached from (bugs.md — an abandoned colony kept hiding the units left
   * standing on its square). */
  colonies_set_occupancy_map(game->world_map_ok ? &game->world_map : NULL);

  /*
   * New-game wizard (difficulty → nation → … → sail). It owns the display
   * outright, so it is serviced ahead of every simulation/chrome service
   * below and returns: "New World" is reachable from the title menu with a
   * live campaign still loaded (Esc → title → New World), and the whole
   * pipeline underneath — end-of-turn slices, queued popups, the bar line,
   * a woodcut, the deferred Europe open, the goto pacer and the unit
   * activation cycle — must freeze while a screen is up (the popup-blocking
   * invariant, bugs.md #262). It is a screen, not a modal, so it joins the
   * screen tests (`screen_over_map`, `map_visible`, `game_screen_name`)
   * rather than `game_modal_open`, which drives `game_handle_modal_input`.
   */
  if (new_game_active(&game->new_game)) {
    game->new_game.ui_font = game->intro_font_ok ? &game->intro_font
      : (game->colony_font_ok ? &game->colony_font
                              : (game->menu_font_ok ? &game->menu_font : NULL));
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
      set_status(game, "Colonization Linux Port", NULL);
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
  /* bugs.md #262: a screen over the map (colony zoom mid-EOT, Europe, a
   * report, the pedia) freezes the WHOLE end-of-turn pipeline — nothing may
   * advance while the player is occupied with it. Fall through so that
   * screen gets its input; the EOT branch resumes when it closes. Queued
   * popups still present on top of it ("popup batch notwithstanding"). */
  /* Zoom hold ends the moment the zoomed colony screen closes. */
  if (game->colony_zoom_popup_hold && !game->in_colony) {
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
   * bugs.md: never while the Colonizopedia owns the screen — the pedia
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
   * bugs.md #682: the second half of FUN_479b_076e's tail (raw 77045-77048).
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
      game_enter_colony(game, found_cid);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* Same status-line hold once the turn processor is idle (a line queued by
   * the last colony still has to be seen). */
  if (game_service_bar_message(game, input)) {
    return GAME_UPDATE_RETURN_TRUE;
  }

  /* bugs.md: an endgame latch must actually END the game. Whatever path
   * latched it (popup chain, zero-colony crush, a loaded save, the silent
   * turn.c C1 check), once every dialog is answered the retire-score chain
   * runs exactly once: score → (exploits) → Hall of Fame → menu/@SCORED.
   * A WON latch with scoring already complete (calendar_latch — the player
   * chose "Keep playing anyway.") stays quiet. */
  if (!game->war_end_retired && game->col1_ok && !ai_popup_busy(&game->ai_popups) &&
      !game_modal_open(game) && !turn_processor_active(&game->turn_proc) &&
      !game->in_menu && !game->in_report &&
      !game->in_hall_of_fame && !game->in_exploits) {
    const int endgame = ai_king_latch_get(&game->col1, AI_KING_ENDGAME_BYTE);
    if (endgame == AI_KING_ENDGAME_LOST) {
      game->war_end_retired = true;
      game->war_end_won = false;
      game_open_retire_score(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (endgame == AI_KING_ENDGAME_WON && !game->col1.head.game_options.calendar_latch) {
      game_begin_win_closing_or_score(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* Deferred ship-arrival Europe open: once every end-of-turn popup has
   * been answered (bugs.md — the tax audience must not land ON the
   * European Status; DOS finishes the audiences first). */
  if (game->europe_ok && game->europe.open_on_dock && !game->in_europe && !game->in_menu &&
      !game->in_colony && !game->in_report && !game->in_pedia &&
      !new_game_active(&game->new_game) && !turn_processor_active(&game->turn_proc) &&
      !ai_popup_busy(&game->ai_popups) && !game_modal_open(game) &&
      !game_europe_blocked_by_woi(game)) {
    game->in_europe = true;
    game->europe.open_on_dock = false;
    sound_set_bgm(3);
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
    /* bugs.md: the signature on the parchment is the LEADER's name (the
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
 * pacing clock and the step itself (bugs.md #418 routing). */
static GameUpdateStep game_pacer_goto_step(
  ColonizeGameState* game, ColonizeUnit* active, uint32_t dt_ms
) {
  /*
   * bugs.md #418: a Go To AIMED at an Indian settlement executes its final
   * step INTO the village. DOS routes every goto step through
   * FUN_465b_0000, so arriving next to the dwelling with the village as
   * the ordered destination raises exactly what an arrow-key step raises
   * (FUN_4d56_4528: woodcut 7 + the NAMES.TXT @ACTIONS menu, or the
   * unmet-tribe warn) — the order was a move command into the village.
   * Earlier this branch stopped one tile short and handed control back
   * (bugs.md #287), which left the player to repeat the step by hand.
   *
   * The order is spent either way: a peaceful meet is conducted from the
   * adjacent tile (the unit never stands ON the village), so the goto is
   * cleared BEFORE dispatching — otherwise the pacer would re-fire the
   * @ACTIONS menu on every frame the unit sat next to the village.
   */
  if (units_goto_dest_is_village_entry_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, active->id)) {
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
       * bugs.md #469: a refusal that DOS shows as a blocking dialog
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
           * blocked step (bugs.md #363) so the pacer doesn't spin. */
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
     * sails for Europe when it reaches THAT tile (bugs.md). Manual
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
    const bool stepped = units_advance_goto_one_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .rng=(ColonizeDosRng*)(&game->move_rng), .rng_reseed=game->ai_rng_seed, .rng_reseed_set=true}, active_id);
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
       * bugs.md #363 (trade-routed wagon sat in the control queue and
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
        game_select_tile(
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
   * pauses while a popup is up (bugs.md). AI/native
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
  if (game->units_ok && game->world_map_ok && map_visible) {
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
     * bugs.md (loaded_soldier_bug.SAV): a passenger woken out of the tile-
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
    } else if (active_awaiting_player) {
      /* bugs.md: the unit awaiting orders must be on screen — whatever path
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
          game->view_pan_hold_unit != active->id /* bugs.md #285 manual pan */) {
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
        const ColonizeUnit* next = units_get_const(&game->units, game->units.selected_id);
        game->map_cursor_x = next->x;
        game->map_cursor_y = next->y;
        game_set_view_center(game, next->x, next->y);
        snprintf(
          game->status, sizeof(game->status), "Selected %s",
          units_display_name(&game->units, next)
        );
      } else {
        game_select_tile(game, game->map_cursor_x, game->map_cursor_y);
      }
    }
  }
  return GAME_UPDATE_CONTINUE;
}

/* Reports screen input. */
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
     * — any click closes it, not just a hit on a drawn box. Score (F10)
     * has no OK button either (reports_ok_button_hit always returns false
     * for it) — same click-anywhere-dismisses rule. */
    const bool page2_click_anywhere =
      game->report_id == COLONIZE_REPORT_CONGRESS && game->congress_page2 &&
      input->mouse_left_clicked;
    const bool score_click_anywhere =
      game->report_id == COLONIZE_REPORT_SCORE && input->mouse_left_clicked;
    const bool ok_clicked = page2_click_anywhere || score_click_anywhere ||
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
        const int page_count = reports_naval_page_count_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(game->units_ok ? &game->units : NULL), .colonies=(ColonizeColonyPool*)(&game->colonies), .europe=(EuropeScreen*)(game->europe_ok ? &game->europe : NULL)}, game->human_nation);
        if (game->naval_page + 1 < page_count) {
          game->naval_page++;
          return GAME_UPDATE_RETURN_TRUE;
        }
        game->naval_page = 0;
      }
      game->in_report = false;
      diag_info("Left report screen.");
      if (game->ff_pedia_after_report >= 0) {
        const int ff_index = game->ff_pedia_after_report;
        game->ff_pedia_after_report = -1;
        game_open_pedia_article(game, PEDIA_CAT_FATHER, ff_index, false);
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (game->report_exits_to_menu) {
        game->report_exits_to_menu = false;
        /* DOS FUN_41f2_14a8 retire chain: score (F10 just closed) ->
         * exploits screen (0b70, only when a @SCORE tier qualifies and
         * scoring isn't already complete) -> Hall of Fame (0f56 insert +
         * present) -> title. */
        game_retire_after_score(game);
      }
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

/* Colony screen input. */
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
    case COLONY_HIT_EXIT:
      game_ui_drag_clear(game);
      game->in_colony = false;
      game->colony_view_id = -1;
      return GAME_UPDATE_RETURN_TRUE;
    case COLONY_HIT_TRANSPORT:
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
      game_colony_drag_begin_cargo(game, hit.index);
      break;
    case COLONY_HIT_HOLD:
      game_colony_drag_begin_hold(game, hit.index);
      break;
    case COLONY_HIT_COLONIST:
    case COLONY_HIT_PEOPLE_COLONIST:
      game_colony_drag_begin_colonist(game, hit.index);
      break;
    case COLONY_HIT_OUTSIDE_UNIT:
      if (hit.index >= 0 && hit.index < csv->outside_unit_count) {
        const int ouid = csv->outside_unit_ids[hit.index];
        if (csv->selected_outside_unit == ouid) {
          /* bugs.md: clicking the already-selected fence unit opens the
           * role popup (what should this colonist be doing) — same list
           * the fence drop offers. */
          game_colony_fence_drop(game, colony);
        } else {
          game_colony_drag_begin_outside(game, ouid);
        }
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
      if (hit.index >= 0 && hit.index < 3) {
        csv->multi_mode = (ColonyMultiMode)hit.index;
      }
      break;
    case COLONY_HIT_MULTI_UNIT_ICON:
      /* hit.index is the unit id (see colony_screen_hit_test). Same
       * select-then-click convention as the Transport strip: second click
       * on the already-selected unit opens its docked/stationed-unit
       * orders (DOS FUN_2f2b_59a0 double-click → FUN_2f2b_5746). */
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
      game_colony_area_tile_drop(game, colony, cmap, hit.index);
      break;
    }
    case COLONY_HIT_AREA_INTERIOR:
      /* DOS 2f2b:609e — click on the area view's non-assignable centre
       * flips the game-wide badge-numbers toggle (DS:0x336). */
      csv->show_production_numbers = !csv->show_production_numbers;
      break;
    case COLONY_HIT_JOBS_ROW:
      if (hit.index >= 0 && hit.index < csv->job_count) {
        game_colony_commit_job(game, colony, csv->job_ids[hit.index]);
      } else {
        colony_screen_close_jobs(csv);
        colony_screen_set_status(csv, game->status);
      }
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
      if (hit.index >= 0 && hit.index < csv->custom_house_count) {
        colonies_toggle_custom_house_cargo(
          &game->colonies, game->colony_view_id, csv->custom_house_cargo_ids[hit.index]
        );
      }
      break;
    case COLONY_HIT_CUSTOM_HOUSE_OUTSIDE:
      colony_screen_close_custom_house(csv);
      break;
    case COLONY_HIT_CONSTRUCTION_CLEAR:
      colonies_clear_construction(&game->colonies, game->colony_view_id);
      colony_screen_close_construction(csv);
      set_status(game, "Construction cleared", NULL);
      colony_screen_set_status(csv, game->status);
      break;
    case COLONY_HIT_CONSTRUCTION_ROW:
      if (hit.index >= 0 && hit.index < csv->buildable_count) {
        game_colony_commit_construction(game, csv->buildable_ids[hit.index]);
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
      if (csv->jobs_selection >= 0 && csv->jobs_selection < csv->job_count) {
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
    /* @CUSTOM has no selection cursor — rows are mouse toggles and the
     * panel is dismissed by leaving it (COLONY_HIT_CUSTOM_HOUSE_OUTSIDE).
     * Enter is that same dismissal from the keyboard, and must not fall
     * through to the panels underneath. */
    if (csv->custom_house_open) {
      colony_screen_close_custom_house(csv);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (csv->construction_open) {
      const int bi = csv->construction_selection - 1;
      if (csv->construction_selection == 0) {
        colonies_clear_construction(&game->colonies, game->colony_view_id);
        set_status(game, "Construction cleared", NULL);
        colony_screen_close_construction(csv);
        colony_screen_set_status(csv, game->status);
      } else if (bi >= 0 && bi < csv->buildable_count) {
        game_colony_commit_construction(game, csv->buildable_ids[bi]);
      } else {
        colony_screen_close_construction(csv);
        colony_screen_set_status(csv, game->status);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    /* Enter on selected colonist opens field jobs; otherwise leave. */
    if (colony && csv->selected_colonist >= 0) {
      int tile = colonies_colonist_tile(colony, csv->selected_colonist);
      if (tile < 0) {
        tile = 0;
      }
      colony_screen_open_jobs(csv, &game->colonies, cmap, colony, tile);
      return GAME_UPDATE_RETURN_TRUE;
    }
    game->in_colony = false;
    game->colony_view_id = -1;
    diag_info("Left colony screen.");
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
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
    if (ch == '1') {
      csv->multi_mode = COLONY_MULTI_PRODUCTION;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '2') {
      csv->multi_mode = COLONY_MULTI_UNITS;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '3') {
      csv->multi_mode = COLONY_MULTI_CONSTRUCTION;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == 'm' || ch == 'M') {
      csv->multi_mode = (ColonyMultiMode)(((int)csv->multi_mode + 1) % 3);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == 'n' || ch == 'N') {
      csv->show_production_numbers = !csv->show_production_numbers;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if ((ch == '=' || ch == '+') && colony && game->units_ok && !csv->jobs_open &&
        !csv->construction_open) {
      if (csv->transport_unit_id < 0) {
        set_status(game, "Select a ship first", NULL);
      } else if (csv->selected_cargo < 0 || csv->selected_cargo >= COLONIZE_CARGO_COUNT ||
                 colony->stock[csv->selected_cargo] <= 0) {
        set_status(game, "Select warehouse cargo", NULL);
      } else if (ch == '+') {
        const int cargo = csv->selected_cargo;
        const int want = colony->stock[cargo] < 100 ? colony->stock[cargo] : 100;
        game_colony_load_hold(game, csv->transport_unit_id, cargo, want);
      } else {
        game_colony_open_load_prompt(game, colony, csv->selected_cargo);
      }
      colony_screen_set_status(csv, game->status);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if ((ch == 'r' || ch == 'R') && colony && !csv->jobs_open && !csv->construction_open &&
        csv->message_kind == COLONY_MSG_NONE) {
      char prompt[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        &game->messages,
        "RENAMECOLONY",
        NULL,
        "",
        prompt,
        sizeof(prompt)
      );
      name_entry_open(
        &game->name_entry,
        NAME_ENTRY_KIND_RENAME,
        prompt,
        colony->name,
        game->colony_view_id
      );
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  if (csv->jobs_open) {
    if (colonize_key_up(input->last_key) && csv->jobs_selection > 0) {
      csv->jobs_selection--;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (colonize_key_down(input->last_key) && csv->jobs_selection < csv->job_count) {
      csv->jobs_selection++;
      return GAME_UPDATE_RETURN_TRUE;
    }
  } else if (csv->message_kind != COLONY_MSG_NONE) {
    const int max_sel = (csv->message_kind == COLONY_MSG_CONFIRM) ? 1 : 0;
    if (colonize_key_up(input->last_key) && csv->message_selection > 0) {
      csv->message_selection--;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (colonize_key_down(input->last_key) && csv->message_selection < max_sel) {
      csv->message_selection++;
      return GAME_UPDATE_RETURN_TRUE;
    }
  } else if (csv->eject_open) {
    /* Greyed rows (FUN_15eb_3454 → 0xffff) are drawn but never land under
     * the highlight — step over them the way a DOS disabled listbox row is
     * stepped over, and stay put if there is nothing enabled beyond. */
    if (colonize_key_up(input->last_key) && csv->eject_selection > 0) {
      int sel = csv->eject_selection - 1;
      while (sel > 0 && !csv->eject_role_enabled[sel]) {
        --sel;
      }
      if (csv->eject_role_enabled[sel]) {
        csv->eject_selection = sel;
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (colonize_key_down(input->last_key) &&
        csv->eject_selection + 1 < csv->eject_role_count) {
      int sel = csv->eject_selection + 1;
      while (sel + 1 < csv->eject_role_count && !csv->eject_role_enabled[sel]) {
        ++sel;
      }
      if (csv->eject_role_enabled[sel]) {
        csv->eject_selection = sel;
      }
      return GAME_UPDATE_RETURN_TRUE;
    }
  } else if (csv->dock_orders_open) {
    if (colonize_key_up(input->last_key) && csv->dock_orders_selection > 0) {
      csv->dock_orders_selection--;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (colonize_key_down(input->last_key) &&
        csv->dock_orders_selection + 1 < csv->dock_orders_count) {
      csv->dock_orders_selection++;
      return GAME_UPDATE_RETURN_TRUE;
    }
  } else if (csv->construction_open) {
    const int max_sel = csv->buildable_count;
    if (colonize_key_up(input->last_key) && csv->construction_selection > 0) {
      csv->construction_selection--;
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (colonize_key_down(input->last_key) && csv->construction_selection < max_sel) {
      csv->construction_selection++;
      return GAME_UPDATE_RETURN_TRUE;
    }
  } else {
    if (colonize_key_up(input->last_key) && colony && csv->selected_colonist > 0) {
      game_colony_select_colonist(game, csv->selected_colonist - 1);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (colonize_key_down(input->last_key) && colony &&
        csv->selected_colonist + 1 < colony->colonist_count) {
      game_colony_select_colonist(game, csv->selected_colonist + 1);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  /* B = buy remaining construction with gold + warehouse tools. */
  if (input->last_key == COLONIZE_KEY_B && colony &&
      colony->building_in_production >= 0) {
    game_request_buy_construction_confirm(game);
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Colony-screen cargo and production keys: L / U on the docked transport
 * and SPACE (one production tick). */
static GameUpdateStep game_colony_screen_cargo_keys(
  ColonizeGameState* game, const ColonizeInputState* input, ColonizeColony* colony,
  ColonyScreenView* csv, const ColonizeWorldMap* cmap
) {
  /* L = load highest-value cargo; U = unload first non-empty hold. */
  if (!csv->jobs_open && !csv->construction_open && colony && game->units_ok) {
    if (input->last_key == COLONIZE_KEY_L) {
      if (csv->transport_unit_id < 0) {
        set_status(game, "Select a ship first", NULL);
      } else {
        const int cargo = colonies_best_load_cargo(colony);
        if (cargo < 0) {
          set_status(game, "Nothing to load", NULL);
        } else {
          const int want = colony->stock[cargo] < 100 ? colony->stock[cargo] : 100;
          game_colony_load_hold(game, csv->transport_unit_id, cargo, want);
        }
      }
      colony_screen_set_status(csv, game->status);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (input->last_key == COLONIZE_KEY_U) {
      if (csv->transport_unit_id < 0) {
        set_status(game, "Select a ship first", NULL);
      } else {
        const int hold = units_first_goods_hold(&game->units, csv->transport_unit_id);
        if (hold < 0) {
          set_status(game, "Hold empty", NULL);
        } else {
          game_colony_unload_hold(game, csv->transport_unit_id, hold, "Cannot unload");
        }
      }
      colony_screen_set_status(csv, game->status);
      return GAME_UPDATE_RETURN_TRUE;
    }
  }

  if (input->last_key == COLONIZE_KEY_SPACE) {
    if (colony) {
      ColonizeTurnResult prod;
      ColonizeColonyProdDelta delta;
      memset(&prod, 0, sizeof(prod));
      turn_colony_free_production(&game->colonies, colony, cmap, &prod, &delta);
      colony_screen_set_delta(csv, &delta);
      {
        /* bugs.md #905: the tags used to be English cargo names compiled in.
         * Wording comes only from the catalog — @CARGO via
         * reports_cargo_display_name, empty string on a miss. */
        static const int k_craft[] = {
          COLONIZE_CARGO_RUM,
          COLONIZE_CARGO_CIGARS,
          COLONIZE_CARGO_CLOTH,
          COLONIZE_CARGO_COATS,
          COLONIZE_CARGO_TOOLS,
          COLONIZE_CARGO_MUSKETS,
        };
        char craft[48];
        craft[0] = '\0';
        size_t cn = 0;
        for (size_t ci = 0; ci < sizeof(k_craft) / sizeof(k_craft[0]); ++ci) {
          const int g = delta.goods[k_craft[ci]];
          if (g <= 0) {
            continue;
          }
          const char* tag = reports_cargo_display_name(k_craft[ci]);
          const int wrote = snprintf(
            craft + cn,
            sizeof(craft) - cn,
            "%s%s%+d",
            cn > 0 ? " " : "",
            tag ? tag : "",
            g
          );
          if (wrote > 0) {
            cn += (size_t)wrote;
          }
          if (cn >= sizeof(craft)) {
            break;
          }
        }
        if (craft[0]) {
          snprintf(
            game->status,
            sizeof(game->status),
            "Food%+d L%+d H%+d %s",
            delta.goods[COLONIZE_CARGO_FOOD],
            delta.goods[COLONIZE_CARGO_LUMBER],
            delta.hammers_added,
            craft
          );
        } else {
          snprintf(
            game->status,
            sizeof(game->status),
            "Food%+d Lumber%+d Ore%+d H%+d",
            delta.goods[COLONIZE_CARGO_FOOD],
            delta.goods[COLONIZE_CARGO_LUMBER],
            delta.goods[COLONIZE_CARGO_ORE],
            delta.hammers_added
          );
        }
      }
      if (delta.building_completed) {
        snprintf(game->status, sizeof(game->status), "Building completed!");
      }
      colony_screen_set_status(csv, game->status);
      diag_info("Colony free production id=%d", colony->id);
    }
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

    {
      const GameUpdateStep sub = game_colony_screen_keys(game, input, colony, csv, cmap);
      if (sub != GAME_UPDATE_CONTINUE) {
        return sub;
      }
    }

    {
      const GameUpdateStep sub =
        game_colony_screen_cargo_keys(game, input, colony, csv, cmap);
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
      if (hit.kind == COLONY_HIT_AREA_TILE || hit.kind == COLONY_HIT_AREA_INTERIOR) {
        int dx = 0;
        int dy = 0;
        if (hit.kind == COLONY_HIT_AREA_TILE) {
          colonies_field_tile_delta(hit.index, &dx, &dy);
        }
        const int cid = game->colony_view_id;
        int index = 0;
        if (game->world_map_ok) {
          index = map_pedia_terrain_index_at(&game->world_map, colony->x + dx, colony->y + dy);
        }
        game_open_pedia_article(game, PEDIA_CAT_TERRAIN, index, false);
        game->pedia_return_colony_id = cid;
        game_status_from_menu_row(game, "REPORTS", 1, game->status, sizeof(game->status));
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
     * keyboard-only (bugs.md).
     */
    if (input->mouse_right_clicked) {
      europe_menu_close(eu);
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
    int max_sel = 0;
    switch (eu->menu) {
      case EUROPE_MENU_RECRUIT:
        max_sel = EUROPE_POOL_SIZE;
        break;
      case EUROPE_MENU_TRAIN:
        max_sel = eu->train_count;
        break;
      case EUROPE_MENU_PURCHASE:
        max_sel = eu->purchase_confirming ? 1 : eu->purchase_count;
        break;
      case EUROPE_MENU_DOCK:
        max_sel = eu->dock_menu_count > 0 ? eu->dock_menu_count - 1 : 0;
        break;
      default:
        break;
    }
    if (colonize_key_up(input->last_key) && eu->menu_selection > 0) {
      eu->menu_selection--;
    } else if (colonize_key_down(input->last_key) && eu->menu_selection < max_sel) {
      eu->menu_selection++;
    } else if (input->last_key == COLONIZE_KEY_ENTER) {
      game_europe_menu_confirm(game);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Europe screen hotkeys: Escape/E exit, R/P/T menus, L/U cargo, S sail and
 * the dock/hold selection keys. */
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

  /* L / '=' : howmuch buy. U : sell howmuch / best hold. */
  if (input->last_key == COLONIZE_KEY_L) {
    if (eu->selected_harbor < 0) {
      snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
    } else {
      game_europe_open_buy_prompt(game);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  if (input->last_key == COLONIZE_KEY_U) {
    /* bugs.md: DOS's U on the European Status unloads (sells) the selected
     * ship's WHOLE cargo, hold by hold — no amount prompt. */
    if (eu->selected_harbor < 0) {
      snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
    } else {
      int sold = 0;
      const int gold_before = eu->gold;
      for (int guard = 0; guard < EUROPE_SHIP_CARGO_MAX; ++guard) {
        const int hold = europe_best_sell_hold(eu, eu->selected_harbor);
        if (hold < 0) {
          break;
        }
        europe_sell_hold(eu, &game->col1, game->human_nation, eu->selected_harbor, hold);
        sold++;
      }
      game_europe_drain_price_events(game);
      if (sold == 0) {
        snprintf(eu->status, sizeof(eu->status), "%s", "Nothing to sell.");
      } else {
        snprintf(
          eu->status, sizeof(eu->status), "Unloaded %d hold%s for %d$.", sold,
          sold == 1 ? "" : "s", eu->gold - gold_before
        );
      }
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  if (input->last_key == COLONIZE_KEY_S) {
    /* DOS: S sails the selected ship for the New World. */
    if (eu->selected_harbor >= 0) {
      game_europe_request_sail(game, eu->selected_harbor);
    } else {
      snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  for (int ti = 0; ti < input->text_input_len; ++ti) {
    const char ch = input->text_input[ti];
    if (ch == '=' && eu->selected_harbor >= 0) {
      /* Same as key L. */
      game_europe_open_buy_prompt(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ch == '+' && eu->selected_harbor >= 0) {
      europe_buy_cargo_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .col1=(ColonizeCol1Save*)(&game->col1), .col1_ok=true, .europe=(EuropeScreen*)(eu)}, game->human_nation, eu->selected_harbor, eu->selected_market, 1);
      game_europe_drain_price_events(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if ((ch == '-' || ch == '_') && eu->selected_harbor >= 0) {
      EuropeHarborShip* ship = &eu->harbor[eu->selected_harbor];
      int hold = -1;
      for (int hi = 0; hi < EUROPE_SHIP_CARGO_MAX; ++hi) {
        if (ship->hold_goods_amount[hi] > 0 && ship->hold_goods_amount[hi] < 255 &&
            ship->hold_goods_type[hi] == eu->selected_market) {
          hold = hi;
          break;
        }
      }
      if (hold < 0) {
        hold = europe_best_sell_hold(eu, eu->selected_harbor);
      }
      if (hold < 0) {
        snprintf(eu->status, sizeof(eu->status), "%s", "Nothing to sell.");
      } else if (ship->hold_goods_amount[hold] > 1) {
        europe_sell_hold_partial(
          eu, &game->col1, game->human_nation, eu->selected_harbor, hold, 1
        );
      } else {
        europe_sell_hold(eu, &game->col1, game->human_nation, eu->selected_harbor, hold);
      }
      game_europe_drain_price_events(game);
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
    if (input->last_key == COLONIZE_KEY_RIGHTBRACKET) {
      europe_cheat_add_gold(eu, 1000);
    } else if (input->last_key == COLONIZE_KEY_LEFTBRACKET) {
      europe_cheat_adjust_tax(eu, -1);
    }
    return GAME_UPDATE_RETURN_TRUE;
  }
  return GAME_UPDATE_CONTINUE;
}

/* Pedia screen input. */
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
    if (input->last_key != COLONIZE_KEY_NONE || input->mouse_left_clicked ||
        input->mouse_right_clicked) {
      if (game->pedia_return_to_list) {
        game_open_pedia_list(game, game->pedia_category);
      } else {
        game->in_pedia = false;
        /* Opened from the colony screen (tile right-click): go back there. */
        if (game->pedia_return_colony_id >= 0 &&
            colonies_get(&game->colonies, game->pedia_return_colony_id) != NULL) {
          game->in_colony = true;
          game->colony_view_id = game->pedia_return_colony_id;
        }
        game->pedia_return_colony_id = -1;
        diag_info("Left Colonizopedia.");
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
    if (colonize_key_up(input->last_key) && game->menu_selection > 0) {
      game->menu_selection--;
    } else if (colonize_key_down(input->last_key) &&
               game->menu_selection + 1 < game->menu_option_count) {
      game->menu_selection++;
    } else if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
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
          /* bugs.md #285: a minimap pan while a unit is active must hold,
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
          /* bugs.md: mid-turn (units still in the control queue) a sidebar
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
           * bugs.md: unless the pointer clearly DRAGGED (≥4 logical px)
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
      if (game->map_goto_place_mode) {
        game->map_goto_place_mode = false;
        set_status(game, "Go to Place cancelled", NULL);
        return GAME_UPDATE_RETURN_TRUE;
      }
      game_select_tile(game, mx, my);
      return GAME_UPDATE_RETURN_TRUE;
    }

    if (!input->mouse_left_clicked) {
      return GAME_UPDATE_RETURN_TRUE;
    }

    /*
     * bugs.md: a click on the map viewport is NEVER an End of Turn
     * confirmation, even while the prompt blinks — it keeps its normal
     * meaning (open a colony, pick a unit, start a go-to drag). Only the
     * right-hand sidebar click (above) confirms the turn; Enter/Space do too.
     */

    /* ORDERS Go to Place: click sets goto destination. */
    if (game->map_goto_place_mode && game->units_ok) {
      const int uid = game->units.selected_id;
      const ColonizeUnit* u = units_get_const(&game->units, uid);
      game->map_goto_place_mode = false;
      if (!u || !game_unit_selectable(game, u)) {
        set_status(game, "Go to Place cancelled", NULL);
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (mx == u->x && my == u->y) {
        set_status(game, "Already here", NULL);
        return GAME_UPDATE_RETURN_TRUE;
      }
      const int rc = game_issue_goto(game, uid, mx, my);
      if (rc == 0) {
        snprintf(game->status, sizeof(game->status), "Go to (%d,%d)", mx, my);
        game_set_view_center(game, u->x, u->y);
      } else if (rc < 0) {
        set_status(game, "Cannot go there", NULL);
      }
      return GAME_UPDATE_RETURN_TRUE;
    }

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
        set_status(game, "Hidden Terrain view off", NULL);
        return GAME_UPDATE_RETURN_TRUE;
      }
      if (game->terrain_peel_phase < 3 &&
          game->elapsed_ms - game->hidden_terrain_phase_ms >= HIDDEN_TERRAIN_STEP_MS) {
        game->terrain_peel_phase++;
        game->hidden_terrain_phase_ms = game->elapsed_ms;
        if (game->terrain_peel_phase == 2) {
          set_status(game, "Hidden Terrain: roads, resources hidden", NULL);
        } else {
          set_status(game, "Hidden Terrain: hills and forest hidden", NULL);
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

    /*
     * F1 terrain pedia at cursor; F2–F10 adviser / report screens. Shift-Fn =
     * CHEAT menu hotkeys when unlocked, per MENU.TXT @CUP: ~F~0~1 Create
     * Unit, ~F~0~2 Debug Info Flags, ~F~0~4 Reveal Map, ~F~0~5 Set Human
     * Player, ~F~0~6 Kill Indians, ~F~0~7 Advance Revolution Status,
     * ~F~0~8 Show Strategy, ~F~0~9 Show Colony Sites, ~F~1~0 Test Routine.
     * Sound Test / Memory Check have no @CUP hotkey (menu-only).
     */
    if (input->last_key >= COLONIZE_KEY_F1 && input->last_key <= COLONIZE_KEY_F10) {
      if (input->shift_held && game->map_menu.cheat_visible) {
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

    /* Alt-W/I/N unlocks CHEAT; Alt-W alone turns it off (COLONIZE README).
     * Other Alt+letter opens map menu titles (~GAME, ~VIEW, ~ORDERS, …). */
    if (input->alt_held && input->last_key != COLONIZE_KEY_NONE) {
      const ColonizeKey k = input->last_key;
      if (k == COLONIZE_KEY_W || k == COLONIZE_KEY_I || k == COLONIZE_KEY_N) {
        if (game->map_menu.cheat_visible) {
          if (k == COLONIZE_KEY_W) {
            map_menu_set_cheat_visible(&game->map_menu, false);
            game->cheat_unlock_step = 0;
            set_status(game, "Cheat mode off", NULL);
          }
          return GAME_UPDATE_RETURN_TRUE;
        }
        if (k == COLONIZE_KEY_W) {
          game->cheat_unlock_step = 1;
        } else if (k == COLONIZE_KEY_I && game->cheat_unlock_step == 1) {
          game->cheat_unlock_step = 2;
        } else if (k == COLONIZE_KEY_N && game->cheat_unlock_step == 2) {
          map_menu_set_cheat_visible(&game->map_menu, true);
          game->cheat_unlock_step = 0;
          set_status(game, "Cheat mode on", NULL);
        } else {
          game->cheat_unlock_step = (k == COLONIZE_KEY_W) ? 1 : 0;
        }
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
    if (game->map_goto_place_mode) {
      game->map_goto_place_mode = false;
      set_status(game, "Go to Place cancelled", NULL);
      return GAME_UPDATE_RETURN_TRUE;
    }
    if (ui_drag_active(&game->ui_drag) && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
      game_ui_drag_clear(game);
      return GAME_UPDATE_RETURN_TRUE;
    }
    game->in_menu = true;
    sound_stop_bgm();
    diag_info("Returned to main menu.");
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
    /* bugs.md: with the End of Turn prompt flashing (View Pieces, nothing
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
      /* bugs.md: View Pieces, no own colony/unit on the tile, control queue
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
    /* bugs.md #691: the key twin runs the same @HAVETREATY scan as the rows. */
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

  /* Shift+D: disband selected unit. Plain D remains dock deploy. */
  if (input->last_key == COLONIZE_KEY_D && input->shift_held && game->units_ok) {
    game_request_disband_confirm(game);
    return GAME_UPDATE_RETURN_TRUE;
  }

  if (input->last_key == COLONIZE_KEY_D && game->world_map_ok && game->europe_ok) {
    if (game->europe.dock_count <= 0) {
      set_status(game, "No immigrants on dock", NULL);
    } else {
      const char* immigrant = game->europe.dock[0].name;
      const int deploy_type = game->europe.dock[0].dos_type;
      if (!units_deploy_colonist(
            &game->units,
            &game->world_map,
            game->map_cursor_x,
            game->map_cursor_y,
            immigrant
          )) {
        set_status(game, "Cannot deploy here", immigrant);
      } else {
        char name[40];
        const int prof = game->europe.dock_count > 0 ? game->europe.dock[0].profession : -1;
        /*
         * units_deploy_colonist always spawns a bare Colonists-type unit and
         * leaves it profession-less; carry over what the immigrant actually
         * was on the dock, kit included, the same way boarding a ship does.
         */
        {
          ColonizeUnit* placed = units_get(&game->units, game->units.selected_id);
          if (placed) {
            const int dti = europe_dock_unit_type_index(&game->units, deploy_type);
            if (dti >= 0) {
              placed->type_index = dti;
            }
            if (prof >= 0) {
              placed->profession = prof;
            }
            europe_apply_dock_unit_kit(placed, deploy_type);
          }
        }
        europe_pop_dock_immigrant(&game->europe, name, sizeof(name));
        europe_remove_dock_mirror_unit(&game->units, game->human_nation, prof);
        snprintf(
          game->status,
          sizeof(game->status),
          "Deployed %s at (%d,%d)",
          name,
          game->map_cursor_x,
          game->map_cursor_y
        );
      }
    }
  }

  /* B: found a colony — Indian land gold/Minuit via colonies_found_with_indian_land. */
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
     * Selected map unit → Sentry; only with NO unit selected does S fall
     * through to Save (the menu Save still works either way).
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
    game_open_save_load(game, SAVE_LOAD_MODE_SAVE);
    return GAME_UPDATE_RETURN_TRUE;
  }

  if (input->last_key == COLONIZE_KEY_L) {
    game_open_save_load(game, SAVE_LOAD_MODE_LOAD);
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

  /* First-land @LANDHO before parent hotkeys; modal gate blocks E/Q/etc. */
  game_try_prompt_landho(game);
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

  if (input->last_key == COLONIZE_KEY_Q) {
    game_enqueue_yes_no(
      game, GAME_MAP_CONFIRM_QUIT, -1, "DOS", "", NULL
    );
    return true;
  }

  step = game_update_report_screen(game, input);
  if (step != GAME_UPDATE_CONTINUE) {
    return step == GAME_UPDATE_RETURN_TRUE;
  }

  /* Retire exploits screen: any key / click advances to the Hall of Fame. */
  if (game->in_exploits) {
    if (input->last_key != COLONIZE_KEY_NONE || input->mouse_left_clicked) {
      game->in_exploits = false;
      game->in_hall_of_fame = true;
      set_status(game, "Hall of Fame", "Enter/Esc returns to menu");
    }
    return true;
  }

  /* Title-menu Hall of Fame screen (reports_render_hall_of_fame). */
  if (game->in_hall_of_fame) {
    if (input->last_key == COLONIZE_KEY_ESCAPE || input->last_key == COLONIZE_KEY_ENTER) {
      game->in_hall_of_fame = false;
      if (game->war_end_won) {
        /* WoI win: the score chain ends in @SCORED "That's all." / "Keep
         * playing anyway." (DOS main-loop 0x104 block) instead of the menu. */
        game->war_end_won = false;
        game_enqueue_war_scored_choice(game);
      } else {
        game->in_menu = true;
        sound_stop_bgm();
        set_status(game, "Colonization Linux Port", NULL);
      }
    }
    return true;
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
    if (game_key_pioneer_order(game, units_pioneer_plow_w)) {
      return true;
    }
    game_open_pedia_list(game, PEDIA_CAT_CARGO);
    return true;
  }

  if (input->last_key == COLONIZE_KEY_R) {
    if (game_key_pioneer_order(game, units_pioneer_road_w)) {
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
