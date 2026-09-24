#include "core/internal.h"
#include "core/game_loop.h"

/*
 * Split out of game_loop.c (2026-09-23) — code moved verbatim.
 *
 * Sections:
 *  - Save/load slot IO, reports/score/pedia menus
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

/* ===================== Save/load slot IO, reports/score/pedia menus (game_apply_col1_save .. render_pedia_screen) ===================== */


static bool game_apply_col1_save(ColonizeGameState* game, ColonizeCol1Save* loaded, char* err, size_t err_size) {
  ColonizeCol1BridgeResult result;
  europe_reset_campaign(&game->europe);
  game->europe.harbor_ships = 0;
  game->europe.dock_count = 0;
  /*
   * Load replaces the world, so the port-side module statics that index the
   * OUTGOING game's pools have to go with it — new game does exactly this
   * trio in ai_init_new_game, load did none of it, so a pending reparations
   * offer / AI goal slate / FF debate slate survived a Load and resolved
   * against col1->tribe[], colony ids and unit ids of a different world.
   * All three are safe here because everything the save owns is rebuilt
   * immediately below: the FF bell pool and the FF bitmasks live in the save
   * itself (bugs.md #933), AI goals are re-planned each nation
   * turn, and the contact cooldowns are turn-number stamps that only mean
   * anything inside one campaign.
   */
  ai_goals_reset();
  founding_fathers_reset();
  ai_contact_reset();
  ai_euro_reset();
  ai_native_reset();
  turn_reset();
  units_reset_state(); /* goto anti-backtrack shadow, indexed by reused unit_id */
  if (!col1_bridge_apply_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .col1=(ColonizeCol1Save*)(loaded), .col1_ok=((loaded) != NULL), .europe=(EuropeScreen*)(&game->europe)}, &result, err, err_size)) {
    return false;
  }
  game->world_map_ok = true;
  game->turn_number = result.turn_number;
  game->game_year = result.year;
  game->game_autumn = result.autumn;
  game->human_nation = result.human_nation;
  game->active_turn_nation = result.human_nation;
  /* DOS DS:0x5398 — keep in step with the control table. Older port saves
   * carry a stale 0 here (new game never stamped it), which mis-gates the
   * head.human_player readers (ai.c WoI seizure, units.c capture popups,
   * ai_euro focus, render tools) for a non-English player. */
  loaded->head.human_player = (uint16_t)result.human_nation;
  game->map_cursor_x = result.cursor_x;
  game->map_cursor_y = result.cursor_y;
  /* Camera comes from its own DS word pair (stuff.viewport_*), not from the
   * cursor — see col1_bridge_apply (smell audit #79). */
  game->map_view_x = result.view_x;
  game->map_view_y = result.view_y;
  units_set_occupancy_map(&game->world_map);
  colonies_set_occupancy_map(&game->world_map);
  game->in_menu = false;
  game->in_europe = false;
  game->in_colony = false;
  game->in_pedia = false;
  game->in_report = false;
  game->in_debug_atlas = false;
  game->colony_view_id = -1;
  game->found_open_colony_id = -1;
  game->turn_flow_deferred = false;
  /*
   * DS:0x5390 rides in the head word block DOS's load restores wholesale
   * (viceroy_unpacked.c:120252), so a save taken in View Pieces resumes in
   * View Pieces: 7 DOS campaign saves under original_saves/ carry map_mode 1
   * with active_unit 0xffff. col1_bridge_apply now honours that 0xffff by
   * leaving selected_id -1; without this line the activation queue below
   * would grab the next unit awaiting orders on the very next frame and the
   * re-save would drop back to map_mode 0 (smell audit: the #78 capture
   * stamp was write-only).
   *
   * 2026-09-10: the `&& selected_id < 0` guard this line used to carry is
   * gone. "A live selection always means Move Pieces" is FUN_2b5a's rule for
   * the moment a unit is *picked* (0x5390 = 0 there), not an invariant: the
   * View Pieces command at raw 42112 is a bare `0x5390 = 1` that leaves
   * 0x5392 alone, and two DOS fixtures prove the combination survives to
   * disk — original_saves/COLONY01 (map_mode 1, active 0x0000) and
   * french-campaign/COLONY09 (map_mode 1, active 0x0050). With capture now
   * stamping map_mode from this field, the guard was the thing that made
   * those two saves lose their mode on a re-save. The two readers of this
   * field (game_end_turn_prompt_active, and the auto-advance below) both
   * take `active_awaiting_player` into account, so mode 1 with a live
   * selection resolves the moment the player touches anything.
   */
  game->view_pieces_mode = loaded->head.map_mode != 0;

  col1_save_free(&game->col1);
  game->col1 = *loaded;
  memset(loaded, 0, sizeof(*loaded));
  game->col1_ok = true;
  /* DS:0x8d80 (post_map.boot_timer) seeds every colony's building layout
   * (FUN_2f2b_0434); the save carries it, so hand it over. bugs.md #578. */
  colony_screen_set_layout_seed(&game->colony_screen, game->col1.post_map.boot_timer);
  ai_diplo_talk_reset(); /* the outgoing game's 153e talk died with its popups */
  /* Repair invented wartime all-cargo embargo on the live save (const apply
   * cannot touch the snapshot). europe.boycott_bitmap already mapped 0xFFFF→0. */
  for (int n = 0; n < 4; ++n) {
    if (game->col1.nation[n].boycott_bitmap == 0xFFFFu) {
      game->col1.nation[n].boycott_bitmap = 0;
    }
  }
  europe_set_nation(
    &game->europe, result.human_nation, game->names_ok ? &game->names : NULL
  );
  /* bugs.md: leader identity must follow the save — game init defaults
   * leader_name to the English "Walter Raleigh" and load never replaced it,
   * so a Dutch campaign's exploits screen / HoF entry / declaration
   * signature all carried the English default. Take the save's own leader
   * name; a save without one gets the nation's @LEADERNAME default (stamped
   * back so later saves and the score chain carry it). */
  {
    const int hn = result.human_nation;
    if (hn >= 0 && hn < (int)COLONIZE_COL1_NATION_COUNT) {
      if (game->col1.player[hn].name[0] != '\0') {
        str_copy_trunc(
          game->leader_name, sizeof(game->leader_name), game->col1.player[hn].name
        );
      } else {
        new_game_default_leader_name(
          game->names_ok ? &game->names : NULL, hn, game->leader_name,
          sizeof(game->leader_name)
        );
        str_copy_trunc(
          game->col1.player[hn].name, sizeof(game->col1.player[hn].name), game->leader_name
        );
      }
    }
  }
  /*
   * bugs.md ("French expeditionary force missing on report"): in DOS the
   * Expeditionary Force exists from turn 1 (new-game seed 75c2:360b) and
   * nothing drains it before the declaration, so every pre-WoI save must
   * hold at least the seed in each pool — whatever the nation. A campaign
   * started on a port build from before the new-game seed existed carries
   * less (the reported French save held [3,0,0,0]: no seed, a little tax
   * growth). Backfill each pool to the seed floor on load; after the
   * declaration the pools legitimately drain, so leave those alone.
   */
  if (!ai_king_independence_declared(&game->col1)) {
    const int diff = game->col1.head.difficulty;
    const uint16_t floor[4] = {
      (uint16_t)(8 * diff + 15),
      (uint16_t)(5 * (diff + 1)),
      (uint16_t)(3 * diff + 2),
      (uint16_t)(6 * diff + 2)
    };
    for (int i = 0; i < 4; ++i) {
      if (game->col1.head.expeditionary_force[i] < floor[i]) {
        game->col1.head.expeditionary_force[i] = floor[i];
      }
    }
  }
  /* Restore Complete Map cheat (DOS show_entire_map @ DS:0x53a2); nation view not saved. */
  game->fog_view = (game->col1.head.show_entire_map != 0) ? -1 : -2;
  /*
   * A save carries the options the player was using in that game, and DOS
   * restores them with it — settings.json is deliberately NOT applied here
   * (see settings.h). Only the audio mixer has to be told, since it lives
   * outside the save.
   */
  {
    ColonizeSoundOptions opts = sound_get_options();
    opts.background_music = game->col1.head.tut2.background_music != 0;
    opts.event_music = game->col1.head.tut2.event_music != 0;
    opts.sound_effects = game->col1.head.tut2.sound_effects != 0;
    sound_set_options(opts);
  }
  sound_set_bgm(1);
  /*
   * bugs.md: loading played the royal-audience tune. The 0x3e queue in
   * OVL27 sits in the KING-portrait screen (75c2:1dcc pushes DS string
   * "KING1" right before it), not in the load path — the load function's
   * own tail (75c2:2330) queues 0x25, the default main theme.
   */
  sound_play(0x25);
  /* Continue LCG for FUN_465b / AI nation turns. VR_SEED fixtures use seed 100;
   * prefer that when the save looks like a seed-100 NEW WORLD start (turn<=6,
   * 34 tribes), else fall back to turn/year. --seed overrides all of that. */
  {
    uint32_t seed = game->turn_number ? game->turn_number
                                      : (game->game_year ? game->game_year : 1u);
    if (game->col1_ok && game->col1.head.tribe_count == 34 && game->col1.head.turn <= 6) {
      seed = 100u;
    }
    seed = game_pick_rng_seed(game, seed);
    dos_rng_seed(&game->move_rng, seed);
    game->ai_rng_seed = seed;
  }
  return true;
}

bool game_load_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size) {
  /* DOS does not re-fire woodcuts on load (the bits ride in the save); drop
   * anything the outgoing game had armed but not yet shown. */
  woodcut_close(&game->woodcut);
  woodcut_clear_pending();
  ColonizeCol1Save loaded;
  col1_save_init(&loaded);
  if (!savegame_read_col1(game->config.save_dir, slot, &loaded, err, err_size)) {
    /* Developer convenience: fall back to repo original_saves/COLONY00.SAV. */
    char fallback[640];
    snprintf(fallback, sizeof(fallback), "original_saves/COLONY%02d.SAV", slot);
    if (!col1_save_read_file(fallback, &loaded, err, err_size)) {
      col1_save_free(&loaded);
      return false;
    }
    diag_info("Loaded fallback save %s", fallback);
  }
  /* FUN_75c2_0840 LOADSIZE: reject when a live map differs (mid-game Load). */
  if (game->world_map_ok) {
    if (!col1_save_validate_head(
          &loaded.head, game->world_map.width, game->world_map.height, err, err_size
        )) {
      col1_save_free(&loaded);
      return false;
    }
  }
  if (!game_apply_col1_save(game, &loaded, err, err_size)) {
    col1_save_free(&loaded);
    return false;
  }
  return true;
}

bool game_save_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size) {
  if (!game->world_map_ok) {
    snprintf(err, err_size, "no map loaded");
    return false;
  }
  if (!game->col1_ok) {
    if (!col1_bridge_init_template(
          &game->col1,
          game->world_map.width,
          game->world_map.height,
          err,
          err_size
        )) {
      return false;
    }
    /* Apply new-game wizard identity onto the template. */
    for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT; ++i) {
      game->col1.player[i].control = 1;
    }
    const int hn = game->human_nation;
    if (hn >= 0 && hn < (int)COLONIZE_COL1_NATION_COUNT) {
      game->col1.player[hn].control = 0;
      str_copy_trunc(
        game->col1.player[hn].name,
        sizeof(game->col1.player[hn].name),
        game->leader_name[0] ? game->leader_name : ""
      );
      str_copy_trunc(
        game->col1.player[hn].country_name,
        sizeof(game->col1.player[hn].country_name),
        new_game_nation_name(hn)
      );
    }
    game->col1.head.difficulty = (uint8_t)game->difficulty;
    /*
     * DOS new-game REF seed (75c2:360b..3643, diff = DS:0x53a6): the
     * Expeditionary Force exists from the first turn — regulars
     * 8*diff+15, dragoons 5*(diff+1), man-o-wars 3*diff+2, artillery
     * 6*diff+2 — and grows from tax events. Without it the Continental
     * Congress page 1 force bars stayed empty for a whole new-game
     * campaign (bugs.md: "English Expeditionary Force ... is empty");
     * loaded DOS saves carried theirs, which is why the Dutch golden
     * seemed to be the only nation implemented.
     */
    if (game->col1.head.expeditionary_force[0] == 0 &&
        game->col1.head.expeditionary_force[1] == 0 &&
        game->col1.head.expeditionary_force[2] == 0 &&
        game->col1.head.expeditionary_force[3] == 0) {
      const int diff = game->col1.head.difficulty;
      game->col1.head.expeditionary_force[0] = (uint16_t)(8 * diff + 15);
      game->col1.head.expeditionary_force[1] = (uint16_t)(5 * (diff + 1));
      game->col1.head.expeditionary_force[2] = (uint16_t)(3 * diff + 2);
      game->col1.head.expeditionary_force[3] = (uint16_t)(6 * diff + 2);
    }
    game->col1_ok = true;
    /* DOS samples DS:0x8d80 from the BIOS tick (0040:006C, 18.2 Hz) once per
     * launch (FUN_1c0c_0012 via FUN_75c2_2d46 raw 121990) and saves it in the
     * post-map tail; a new game without one gets the same kind of value
     * here so its colony layouts differ per game like DOS. bugs.md #578. */
    if (game->col1.post_map.boot_timer == 0) {
      game->col1.post_map.boot_timer =
        (uint32_t)(((uint64_t)time(NULL) * 182u / 10u) & 0x00ffffffu);
    }
    colony_screen_set_layout_seed(&game->colony_screen, game->col1.post_map.boot_timer);
    if (game->game_year == 0) {
      game->game_year = 1492;
    }
  }
  if (!col1_bridge_capture_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .col1=(ColonizeCol1Save*)(&game->col1), .col1_ok=true, .europe=(EuropeScreen*)(&game->europe)}, game->game_year, game->game_autumn, game->turn_number, game->human_nation, game->map_cursor_x, game->map_cursor_y, game->map_view_x, game->map_view_y, game->units.selected_id, game->view_pieces_mode, err, err_size)) {
    return false;
  }
  return savegame_write_col1(game->config.save_dir, slot, &game->col1, err, err_size);
}

void game_open_report(ColonizeGameState* game, ColonizeReportId id) {
  if (!game) {
    return;
  }
  /*
   * DOS gate (FUN_3f41_2548 raw :70792): F8 Foreign Affairs is withdrawn once
   * the War of Independence has begun — the overlay returns before the plate
   * load and shows @FOREIGNNOTAVAIL instead, so the screen is never entered.
   */
  if (!reports_is_available(id, game->col1_ok ? &game->col1 : NULL)) {
    const char* tag = reports_unavailable_tag(id);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages,
      tag ? tag : "FOREIGNNOTAVAIL",
      NULL,
      "",
      body,
      sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    return;
  }
  if (id != COLONIZE_REPORT_CONGRESS) {
    game->ff_pedia_after_report = -1;
  }
  game->in_report = true;
  game->report_id = id;
  game->report_exits_to_menu = false;
  game->congress_page2 = false;
  game->labor_detail_job = -1;
  game->economic_page = 0;
  game->colony_page = 0;
  game->naval_page = 0;
  game->in_pedia = false;
  game->in_europe = false;
  game->in_colony = false;
  game->in_debug_atlas = false;
  snprintf(game->status, sizeof(game->status), "%s", reports_title(id));
  game_track_screen(game);
  diag_info("Opened report %s (%s)", reports_title(id), reports_background_name(id));
}

void game_hof_insert(ColonizeGameState* game, const ColonizeHofEntry* entry);
void game_hof_save(const ColonizeGameState* game);

/* NAMES.TXT section row (comma field 0), trimmed; false when missing. */
bool game_names_row(
  const ColonizeGameState* game,
  const char* section,
  int row,
  char* out,
  size_t out_size
) {
  if (!game || !out || out_size == 0) {
    return false;
  }
  out[0] = '\0';
  if (!game->names_ok || row < 0) {
    return false;
  }
  const ColonizeMsgSection* sec = assets_msg_find(&game->names, section);
  if (!sec) {
    return false;
  }
  int seen = 0;
  for (int i = 0; i < sec->line_count; ++i) {
    const char* l = sec->lines[i];
    if (!l || l[0] == '\0' || l[0] == ';' || l[0] == '\r') {
      continue;
    }
    if (seen++ != row) {
      continue;
    }
    snprintf(out, out_size, "%s", l);
    char* comma = strchr(out, ',');
    if (comma) {
      *comma = '\0';
    }
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\r' || out[n - 1] == '\n' || out[n - 1] == ' ')) {
      out[--n] = '\0';
    }
    return out[0] != '\0';
  }
  return false;
}

/*
 * DOS FUN_41f2_0b70 exploits screen text: GAME.TXT @EXPLOITS (%NUMBER0 =
 * rating, %STRING0 = nation name) + the first tier+1 @SCORE rows split at
 * the comma (category / named thing, %STRING0 = leader's surname — DOS
 * strchr(name, ' ') + 1, whole name when no space). SCORE<tier+1>.SS frame.
 */
static void game_build_exploits(ColonizeGameState* game, const ColonizeScoreBreakdown* sc) {
  ColonizeExploitsView* ex = &game->exploits;
  memset(ex, 0, sizeof(*ex));
  const char* leader = game->leader_name[0] ? game->leader_name : "";
  const char* surname = strchr(leader, ' ');
  surname = surname ? surname + 1 : leader;
  char nation_name[64];
  snprintf(nation_name, sizeof(nation_name), "%s", new_game_nation_name(game->human_nation));

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = nation_name;
  tok.number0 = sc->rating;
  tok.has_number0 = true;
  const ColonizeMsgSection* hdr = assets_msg_find(&game->messages, "EXPLOITS");
  if (hdr) {
    for (int i = 0; i < hdr->line_count && ex->header_count < 3; ++i) {
      const char* l = hdr->lines[i];
      if (!l || l[0] == '\0' || l[0] == '\r' || popup_msg_is_directive(l)) {
        continue;
      }
      popup_msg_apply_tokens(
        ex->header[ex->header_count], sizeof(ex->header[0]), l, &tok
      );
      char* h = ex->header[ex->header_count];
      h[strcspn(h, "\r\n")] = '\0';
      ex->header_count++;
    }
  } else {
    snprintf(ex->header[0], sizeof(ex->header[0]), "COLONIZATION RATING: %d%%", sc->rating);
    ex->header[1][0] = '\0';
    ex->header[2][0] = '\0';
    ex->header_count = 3;
  }

  const ColonizeMsgSection* rows = assets_msg_find(&game->messages, "SCORE");
  const int want = sc->exploits_tier + 1;
  if (rows) {
    int seen = 0;
    for (int i = 0; i < rows->line_count && seen < want && seen < 24; ++i) {
      const char* l = rows->lines[i];
      if (!l || l[0] == '\0' || l[0] == '\r' || popup_msg_is_directive(l)) {
        continue;
      }
      char raw[COLONIZE_MSG_LINE_LEN];
      snprintf(raw, sizeof(raw), "%s", l);
      raw[strcspn(raw, "\r\n")] = '\0';
      char* comma = strchr(raw, ',');
      const char* named = "";
      if (comma) {
        *comma = '\0';
        named = comma + 1;
        while (*named == ' ') {
          named++;
        }
      }
      snprintf(ex->categories[seen], sizeof(ex->categories[0]), "%s", raw);
      PopupMsgTokens ntok;
      memset(&ntok, 0, sizeof(ntok));
      ntok.string0 = surname;
      popup_msg_apply_tokens(ex->named, sizeof(ex->named), named, &ntok);
      seen++;
    }
    ex->category_count = seen;
  }

  ss_free(&game->exploits_sheet);
  game->exploits_sheet_ok = false;
  if (sc->exploits_tier >= 0) {
    char file[32];
    char path[640];
    char err[256];
    snprintf(file, sizeof(file), "SCORE%02d.SS", sc->exploits_tier + 1);
    if (dos_compat_normalize_asset_path(game->resolved_data_dir, file, path, sizeof(path)) &&
        ss_load(path, &game->exploits_sheet, err, sizeof(err))) {
      if (game->reports_ok) {
        reports_remap_exploits_sheet(&game->reports, &game->exploits_sheet);
      }
      game->exploits_sheet_ok = true;
    }
  }
  ex->sheet = game->exploits_sheet_ok ? &game->exploits_sheet : NULL;
}

/* After the Retire F10 report closes: HoF insert (FUN_41f2_0f56), then the
 * exploits screen (FUN_41f2_0b70) when a tier qualifies, else straight to
 * the Hall of Fame; both end at the title menu. */
void game_retire_after_score(ColonizeGameState* game) {
  sound_stop_bgm();
  if (!game->col1_ok) {
    game->in_menu = true;
    set_status(game, "Retired to main menu", NULL);
    return;
  }
  ColonizeScoreBreakdown sc;
  reports_compute_score_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&game->colonies), .col1=(ColonizeCol1Save*)(&game->col1), .col1_ok=true, .europe=(EuropeScreen*)(&game->europe)}, &sc, game->human_nation);
  /* DOS LAB_3844_0b4a / main-loop 0x104 block: 0x5382 |= 0x10 once the score
   * chain has run — "scoring complete" (also gates the WON auto-retire). */
  game->col1.head.game_options.calendar_latch = 1;
  ColonizeHofEntry entry;
  memset(&entry, 0, sizeof(entry));
  snprintf(
    entry.leader, sizeof(entry.leader), "%s", game->leader_name[0] ? game->leader_name : ""
  );
  snprintf(entry.nation, sizeof(entry.nation), "%s", new_game_nation_name(game->human_nation));
  entry.nation_id = game->human_nation;
  entry.score = sc.total;
  entry.year = sc.year;
  entry.difficulty = sc.difficulty;
  entry.rating = sc.rating;
  entry.declared = sc.independence_declared;
  entry.achieved = sc.independence_achieved;
  game_hof_insert(game, &entry);
  game_hof_save(game);

  if (sc.exploits_tier >= 0 && !sc.scoring_complete) {
    /* DOS FUN_41f2_0b70 plays the tier-selected tune at the coin reveal. */
    sound_play(sound_retire_tune_id(sc.exploits_tier));
    game_build_exploits(game, &sc);
    game->in_exploits = true;
    set_status(game, "Retired — Exploits", "Any key continues");
  } else {
    game->in_hall_of_fame = true;
    set_status(game, "Hall of Fame", "Enter/Esc returns to menu");
  }
}

void game_open_retire_score(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game_open_report(game, COLONIZE_REPORT_SCORE);
  game->report_exits_to_menu = true;
  set_status(game, "Retired — Colonization Score", "Enter/Esc returns to menu");
}

/*
 * DOS: VICEROY execs CLOSING.EXE after the @KINGLOSE audience and before
 * FUN_41f2_14a8. Missing art skips straight to the score plate.
 */
void game_begin_win_closing_or_score(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game->war_end_retired = true;
  game->war_end_won = true;
  if (closing_open(&game->closing, game->resolved_data_dir)) {
    game->closing_just_opened = true;
    game->closing_then_score = true;
    set_status(game, "", NULL);
    return;
  }
  game_open_retire_score(game);
}

/* GAME.TXT @SCORED after a WoI win's score chain (DOS main-loop 0x104 block):
 * "That's all." (1) quits to the title menu, "Keep playing anyway." (2)
 * resumes the campaign. */
void game_enqueue_war_scored_choice(ColonizeGameState* game) {
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages, "SCORED", NULL, "", body, sizeof(body)
  );
  char label_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(
    &game->messages, "SCORED", NULL, "", "", label_buf, labels
  );
  const int ids[2] = {1, 2};
  (void)ai_popup_enqueue_choice_ctx(
    &game->ai_popups, AI_POPUP_TAG_WAR_SCORED, game->human_nation, -1, 0, NULL,
    body, labels, ids, 2
  );
}

void game_open_debug_atlas(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game->in_debug_atlas = true;
  game->in_pedia = false;
  game->in_europe = false;
  game->in_colony = false;
  game->in_report = false;
  if (game->debug_atlas.count <= 0) {
    debug_atlas_scan(&game->debug_atlas, game->resolved_data_dir);
  }
  debug_atlas_load(&game->debug_atlas, game->resolved_data_dir, 0);
  diag_info("Entered graphic atlas debug (%d files).", game->debug_atlas.count);
}

/* Open Colonizopedia list / article. */
const ColonizeFont* game_pedia_font(const ColonizeGameState* game) {
  if (game->colony_font_ok) {
    return &game->colony_font;
  }
  if (game->menu_font_ok) {
    return &game->menu_font;
  }
  return NULL;
}

static void game_pedia_enter_shell(ColonizeGameState* game) {
  game->in_pedia = true;
  game->in_report = false;
  game->in_europe = false;
  game->in_colony = false;
  game->in_debug_atlas = false;
  game->pedia_return_colony_id = -1; /* callers that want colony-return set it after */
}

void game_open_pedia_list(ColonizeGameState* game, PediaCategory category) {
  if (!game) {
    return;
  }
  game_pedia_enter_shell(game);
  game->pedia_view = PEDIA_VIEW_LIST;
  game->pedia_return_to_list = true;
  game->pedia_category = category;
  game->pedia_index = 0;
  game->pedia_hover_entry = -1;
  snprintf(game->status, sizeof(game->status), "%s", pedia_category_label(category));
  game_track_screen(game);
  diag_info("Opened Colonizopedia list (%s)", pedia_category_label(category));
}

void game_open_pedia_article(
  ColonizeGameState* game,
  PediaCategory category,
  int index,
  bool return_to_list
) {
  if (!game) {
    return;
  }
  game_pedia_enter_shell(game);
  game->pedia_view = PEDIA_VIEW_ARTICLE;
  game->pedia_return_to_list = return_to_list;
  game->pedia_category = category;
  const int count = pedia_category_count(category);
  if (index < 0) {
    index = 0;
  }
  if (count > 0 && index >= count) {
    index = count - 1;
  }
  game->pedia_index = index;
  game->pedia_hover_entry = -1;
  snprintf(game->status, sizeof(game->status), "%s", pedia_category_label(category));
}

/* F1 / REPORTS → Terrain Information: article for the tile under the cursor. */
void game_open_terrain_pedia_at_cursor(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  int index = 0;
  if (game->world_map_ok) {
    index = map_pedia_terrain_index_at(&game->world_map, game->map_cursor_x, game->map_cursor_y);
  }
  game_open_pedia_article(game, PEDIA_CAT_TERRAIN, index, false);
  game_status_from_menu_row(game, "REPORTS", 1, game->status, sizeof(game->status));
  diag_info(
    "Opened Terrain Information pedia index=%d at cursor (%d,%d)",
    index,
    game->map_cursor_x,
    game->map_cursor_y
  );
}

void game_handle_report_fkey(ColonizeGameState* game, ColonizeKey key) {
  if (!game || key < COLONIZE_KEY_F1 || key > COLONIZE_KEY_F10) {
    return;
  }
  const int fnum = (int)(key - COLONIZE_KEY_F1) + 1;
  if (fnum == 1) {
    game_open_terrain_pedia_at_cursor(game);
    return;
  }
  ColonizeReportId id;
  if (reports_id_from_fkey(fnum, &id)) {
    game_open_report(game, id);
  }
}

void render_pedia_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer) {
  const ColonizeFont* font = game_pedia_font(game);

  if (game->pedia_view == PEDIA_VIEW_LIST) {
    pedia_list_render(
      game->pedia_ok ? &game->pedia : NULL,
      game->names_ok ? &game->names : NULL,
      game->pedia_category,
      game->pedia_wood_ok ? &game->pedia_wood : NULL,
      font,
      game->pedia_hover_entry,
      framebuffer
    );
    return;
  }

  /* DOS-fidelity article page (segment 6cb2 builders — see pedia.c). */
  const PediaArticleAssets assets = {
    .pedia = game->pedia_ok ? &game->pedia : NULL,
    .names = game->names_ok ? &game->names : NULL,
    .labels = game->labels_ok ? &game->labels : NULL,
    .wood_bg = game->pedia_wood_ok ? &game->pedia_wood : NULL,
    .font = font,
    .chrome_font = game->colony_font_ok ? &game->colony_font
      : (game->menu_font_ok ? &game->menu_font : NULL),
    .icons = game->unit_icons_ok ? &game->unit_icons : NULL,
    .buildings = game->pedia_buildings_ok ? &game->pedia_buildings : NULL,
    .terrain = game->terrain_ok ? &game->terrain : NULL,
    .phys0 = game->phys0_ok ? &game->phys0 : NULL,
    .palette = (game->pedia_wood_ok && game->pedia_wood.has_palette) ? &game->pedia_wood.palette
                                                                     : NULL,
    .human_nation = game->human_nation,
  };
  pedia_article_render(&assets, game->pedia_category, game->pedia_index, framebuffer);

  if (!game->pedia_ok) {
    font_draw_text(font, framebuffer, 8, framebuffer->height - 12, "(PEDIA.TXT not loaded)", 12);
  }
}
