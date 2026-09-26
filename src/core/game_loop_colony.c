#include "core/internal.h"
#include "core/game_loop.h"

/*
 * Split out of game_loop.c (2026-09-23) — code moved verbatim.
 *
 * Sections:
 *  - Colony UI: enter colony, drag & drop, job/building assignment
 *  - Europe voyages, colony roles, dock orders
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

/* ===================== Colony UI: enter colony, drag & drop, job/building assignment (game_owned_unit_at .. game_colony_drag_drop) ===================== */


/* Human-owned map unit on tile with moves remaining (else -1). */
int game_owned_unit_at(const ColonizeGameState* game, int x, int y) {
  if (!game || !game->units_ok) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &game->units.units[i];
    if (!units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    if (u->nation_id != game->human_nation || u->moves <= 0) {
      continue;
    }
    return u->id;
  }
  return -1;
}


ColoniesBuildableOpts game_colony_buildable_opts(const ColonizeGameState* game) {
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  if (!game) {
    return opts;
  }
  opts.map = game->world_map_ok ? &game->world_map : NULL;
  const ColonizeColony* col = colonies_get(&game->colonies, game->colony_view_id);
  const int nation = col ? col->nation_id : game->human_nation;
  /* NAMES.TXT @FATHERS: Adam Smith=0, Peter Stuyvesant=3. head.founding_father[]
   * is DOS's write-once first-claimer record, never an ownership test
   * (FUN_15eb_3960; smell audit #83) — the per-nation bitmask is the owner. */
  const ColonizeCol1Save* col1 = game->col1_ok ? &game->col1 : NULL;
  opts.has_adam_smith = founding_fathers_nation_has(col1, nation, 0);
  opts.has_peter_stuyvesant = founding_fathers_nation_has(col1, nation, 3);
  /* FUN_15eb_3650 wagon arm reads the census counters (DS:0x9298 / 0x924c). */
  opts.col1 = col1;
  return opts;
}

/* FUN_2f2b_6cd4 reveal block (see the call site in game_enter_colony). */
static void game_colony_reveal_new_building(ColonizeGameState* game, int cid, int bid) {
  if (!game || bid < 0 || bid >= COLONIZE_BUILDING_TYPES_MAX) {
    return;
  }
  ColonizeColony* col = colonies_get_mut(&game->colonies, cid);
  if (!col || !col->has_building[bid]) {
    return;
  }
  static uint8_t before[320 * 200];
  col->has_building[bid] = false;
  const bool have_before = game_fizzle_snapshot_present(game, before);
  col->has_building[bid] = true;
  sound_play(0x54);
  if (have_before) {
    game_fizzle_present(game, before);
  }
}

void game_enter_colony(ColonizeGameState* game, int cid) {
  if (cid < 0) {
    set_status(game, "No colony at cursor", NULL);
    return;
  }
  game->in_colony = true;
  game->in_europe = false;
  game->in_pedia = false;
  game->in_report = false;
  game->colony_view_id = cid;
  /*
   * DOS FUN_2f2b_6cd4 colony bring-up: the colony tune pool
   * (FUN_281f_0498(2)); back on the map the pool is 1 again. Event 0x54
   * (COLDIG 13 hammering + cheering) is NOT played on every open — DOS
   * gates it on DS:0x34a >= 0, the building that just finished
   * construction, and plays it with that building's reveal animation
   * (bugs.md: "celebratory SFX should not be played every time the colony
   * UI is opened"). Founding keeps its own 0x54 (FUN_479b_076e).
   */
  int reveal_bid = -1;
  {
    ColonizeColony* reveal = colonies_get_mut(&game->colonies, cid);
    if (reveal && reveal->pending_build_reveal > 0) {
      reveal_bid = reveal->pending_build_reveal - 1;
      reveal->pending_build_reveal = 0;
    }
  }
  sound_set_bgm(2);
  colony_screen_reset_ui(&game->colony_screen);
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  if (col && col->colonist_count > 0) {
    game->colony_screen.selected_colonist = 0;
  }
  snprintf(game->status, sizeof(game->status), "Entered %s", col ? col->name : "");
  colony_screen_set_status(&game->colony_screen, col ? col->name : "");
  game_track_screen(game);
  /*
   * DOS-LITERAL FUN_2f2b_6cd4 raw 55975 reveal block: with DS:0x34a >= 0 the
   * bring-up clears that building's bit (FUN_281f_0bbe(.., 0) = colony record
   * +0x84), draws and presents the screen without it, sets the bit again,
   * redraws, fires event 0x54 (FUN_281f_04c0) and presents the redraw through
   * the LFSR fizzle FUN_281f_03ea(8) — so the finished building un-dissolves
   * into place the first time the colony is opened.
   */
  game_colony_reveal_new_building(game, cid, reveal_bid);
  if (diag_info_enabled() && col) {
    const char* project = "-";
    if (col->building_in_production >= 0 &&
        col->building_in_production < game->colonies.building_type_count) {
      project = game->colonies.building_types[col->building_in_production].name;
    }
    diag_info(
      "COLONY opened %s (id=%d at (%d,%d) nation=%d pop=%d SoL=%d%% food=%d hammers=%d project=%s)",
      col->name[0] ? col->name : "",
      col->id,
      col->x,
      col->y,
      col->nation_id,
      col->colonist_count,
      colony_prod_sol_percent(game->col1_ok ? &game->col1 : NULL, col),
      col->stock[COLONIZE_CARGO_FOOD],
      col->hammers,
      project
    );
  }
}

void game_enter_colony_at_cursor(ColonizeGameState* game) {
  game_enter_colony(
    game, colonies_id_at(&game->colonies, game->map_cursor_x, game->map_cursor_y)
  );
}

/*
 * bugs.md #6 / #562: a colonist admitted via 'B' must go to work at once.
 * DOS's join path is FUN_15eb_3930 -> FUN_15eb_2ea0 -> FUN_15eb_28c8: the
 * newcomer takes the best-scoring work plot, and only when nothing scores does
 * he become a Carpenter. The old Town-Hall-first seating here had no DOS
 * counterpart. colonies_auto_assign_idle is that chain (colony.c).
 */
static void game_auto_assign_new_colonist(ColonizeGameState* game, int colony_id, int colonist_index) {
  colonies_seat_new_colonist(&game->colonies, colony_id, colonist_index);
}

/*
 * ORDERS Join Colony — DOS-LITERAL FUN_2b5a Build/Join handler join branch,
 * asm 0x227b2-0x227d9 (bugs.md #688 / #689 / #690):
 *   - @NOCOLONIESEITHER (asm 0x2256d) gates the SHARED handler, so it blocks
 *     Join exactly as it blocks Build;
 *   - `cmp [colony*0xca + 0x5d60],[0x5394]` — a colony owned by anyone else
 *     is a silent no-op, NOT "open the colony screen";
 *   - FUN_291f_01ec (the admit) returning nonzero is a silent bail too; the
 *     32-colonist cap lives inside it (overlays.c:10252-10255) and raises no
 *     popup. @FULL has no DS tag in docs/popup_tag_ids.md and no push of one
 *     anywhere near this handler — it belongs to the Europe immigration
 *     path, not to Join;
 *   - on success DOS calls FUN_281f_0e1c(1) (next unit) and does NOT open
 *     the colony screen.
 */
void game_join_colony_order(ColonizeGameState* game) {
  if (!game || !game->units_ok || !game->colonies_ok) {
    set_status(game, "Cannot join colony", NULL);
    return;
  }
  if (game_woi_blocks_colony_orders(game)) {
    return;
  }
  const int sid = game->units.selected_id;
  const ColonizeUnit* u = units_get_const(&game->units, sid);
  if (!u || !u->active || !units_is_on_map(u) || units_is_sea(&game->units, sid)) {
    return;
  }
  const int cid = colonies_id_at(&game->colonies, u->x, u->y);
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  if (!col || col->nation_id != game->human_nation) {
    return; /* asm 0x227be: wrong owner (or no colony) = silent return */
  }
  if (col->colonist_count >= COLONIZE_COLONY_POP_MAX) {
    return; /* overlays.c:10252-10255 bails without a word */
  }
  const int ci = colonies_admit_unit_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL)}, cid, sid);
  if (ci < 0) {
    return;
  }
  game_auto_assign_new_colonist(game, cid, ci);
  game->units.selected_id = -1;
  snprintf(
    game->status,
    sizeof(game->status),
    "Joined %s",
    col->name[0] ? col->name : ""
  );
  game_wait_next_unit(game); /* FUN_281f_0e1c(1) */
}

/* One selection: colony colonist index, or admit selected outside unit first. */
static int game_colony_selected_colonist(ColonizeGameState* game) {
  if (!game) {
    return -1;
  }
  ColonyScreenView* csv = &game->colony_screen;
  if (csv->selected_colonist >= 0) {
    return csv->selected_colonist;
  }
  if (csv->selected_outside_unit < 0 || !game->units_ok) {
    return -1;
  }
  const int ci = colonies_admit_unit_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL)}, game->colony_view_id, csv->selected_outside_unit);
  if (ci < 0) {
    return -1;
  }
  csv->selected_outside_unit = -1;
  csv->selected_colonist = ci;
  return ci;
}

void game_colony_select_colonist(ColonizeGameState* game, int colonist_index);
void game_colony_select_outside(ColonizeGameState* game, int unit_id);

void game_colony_finish_eject(
  ColonizeGameState* game,
  int colonist_index,
  int role
) {
  ColonyScreenView* csv = &game->colony_screen;
  const int uid = colonies_eject_colonist(
    &game->colonies, game->colony_view_id, colonist_index, &game->units, role
  );
  if (uid < 0) {
    set_status(game, "Cannot leave colony", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  game_colony_select_outside(game, uid);
  const ColonizeColony* col = colonies_get(&game->colonies, game->colony_view_id);
  if (col && col->colonist_count <= 0) {
    colonies_abandon(&game->colonies, game->colony_view_id);
    game->in_colony = false;
    game->colony_view_id = -1;
    /* One write: set_status(game, game->status, ...) would snprintf the
     * buffer onto itself (overlapping copy, UB — gcc -Wrestrict). */
    set_status(game, "Colony abandoned", NULL);
    return;
  }
  snprintf(
    game->status, sizeof(game->status), "Left as %s", colonies_eject_role_name(role)
  );
  colony_screen_set_status(csv, game->status);
}

/* Returns true if eject was handled (done, blocked, or confirm opened). */
bool game_colony_request_eject(
  ColonizeGameState* game,
  int colonist_index,
  int role
) {
  ColonyScreenView* csv = &game->colony_screen;
  const ColonizeColony* colony = colonies_get(&game->colonies, game->colony_view_id);
  if (!colony || colonist_index < 0) {
    set_status(game, "Cannot leave colony", NULL);
    colony_screen_set_status(csv, game->status);
    return true;
  }
  /* fandom Stockade/Colony: with Stockade/Fort/Fortress cannot voluntarily
   * drop below 3 (port previously kept ≥2 — docs/fandom_col1994.md Conflicts). */
  if (colonies_has_fortification(&game->colonies, colony) && colony->colonist_count <= 3) {
    colony_screen_close_eject(csv);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages,
      "KEEPSTOCKADE",
      NULL,
      "",
      body,
      sizeof(body)
    );
    colony_screen_open_message_ok(csv, body);
    return true;
  }
  if (colony->colonist_count <= 1) {
    char body[AI_POPUP_BODY_LEN];
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = colony->name[0] ? colony->name : "this";
    /*
     * DOS 2f2b caseD_a: name = "ABANDON", and "2" is strcat'd only when the
     * colony owner's colony count (DS:0x9298+nation) is < 2 AND the year
     * (DS:0x538a) is past 0x627 = 1575 — not 1600 as the @ABANDON2 copy's
     * "after 1600" flavour text suggests.
     */
    const char* section = "ABANDON";
    {
      int owner_cols = 0;
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &game->colonies.colonies[i];
        if (c->active && c->nation_id == colony->nation_id) {
          owner_cols++;
        }
      }
      if (owner_cols < 2 && game->col1_ok && game->col1.head.year > 1575) {
        section = "ABANDON2";
      }
    }
    popup_msg_fill(
      &game->messages,
      section,
      &tok,
      "",
      body,
      sizeof(body)
    );
    char choices[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    const ColonizeMsgSection* sec = assets_msg_find(&game->messages, section);
    const int nch = popup_msg_choices(sec, choices, AI_POPUP_CHOICE_MAX);
    /*
     * bugs.md: the confirm used to be a colony-screen-local box — single
     * unwrapped line in the colony font, no @width, no MSS figure, so the
     * three-line @ABANDON body ran out of the frame. DOS shows it through
     * the ordinary 6f74 dialog compositor (FUN_281f_0652), so route it
     * through ai_popup like every other GAME.TXT modal: wrapping, @width=190,
     * {} emphasis, @default=2 and the MSS5 nun all come for free.
     */
    colony_screen_close_message(csv);
    char filled[2][AI_POPUP_CHOICE_LEN];
    popup_msg_apply_tokens(
      filled[0], sizeof(filled[0]), nch >= 1 ? choices[0] : "", &tok
    );
    popup_msg_apply_tokens(
      filled[1], sizeof(filled[1]), nch >= 2 ? choices[1] : "", &tok
    );
    const char* labels[2] = {filled[0], filled[1]};
    const int ids[2] = {1, 2};
    if (!ai_popup_enqueue_choice_ctx(
          &game->ai_popups, AI_POPUP_TAG_COLONY_ABANDON, colonist_index, role, 0, NULL, body,
          labels, ids, 2
        )) {
      set_status(game, "Cannot abandon colony now", NULL);
      colony_screen_set_status(csv, game->status);
    } else {
      /* Player-initiated: open at once, never behind queued EOT chrome or a
       * colony-zoom hold (DOS nests it in the colony screen's own loop). */
      (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_COLONY_ABANDON);
    }
    return true;
  }
  colony_screen_close_eject(csv);
  game_colony_finish_eject(game, colonist_index, role);
  return true;
}

void game_colony_select_colonist(ColonizeGameState* game, int colonist_index) {
  if (!game) {
    return;
  }
  game->colony_screen.selected_colonist = colonist_index;
  game->colony_screen.selected_outside_unit = -1;
}

void game_colony_select_outside(ColonizeGameState* game, int unit_id) {
  if (!game) {
    return;
  }
  game->colony_screen.selected_outside_unit = unit_id;
  game->colony_screen.selected_colonist = -1;
}

static int game_colony_list_outside_roles(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnit* unit,
  int* out_roles,
  bool* out_enabled,
  int out_max
);

void game_ui_drag_clear(ColonizeGameState* game) {
  if (game) {
    ui_drag_clear(&game->ui_drag);
  }
}

const ColonizeSpriteSheet* game_icons(const ColonizeGameState* game) {
  return (game && game->unit_icons_ok) ? &game->unit_icons : NULL;
}

static int game_europe_transit_line_h(const ColonizeGameState* game) {
  const ColonizeFont* font = (game && game->menu_font_ok) ? &game->menu_font : NULL;
  return font && font->max_height > 0 ? (int)font->max_height + 2 : 8;
}

EuropeHitResult game_europe_hit(const ColonizeGameState* game, int mx, int my) {
  return europe_hit_test_ex(
    &game->europe,
    mx,
    my,
    game->units_ok ? &game->units : NULL,
    game_icons(game),
    game_europe_transit_line_h(game)
  );
}

void game_ui_drag_set_icon(ColonizeGameState* game, int sprite) {
  const ColonizeSpriteSheet* icons = game_icons(game);
  if (icons) {
    ui_drag_set_cursor_from_sheet(&game->ui_drag, icons, sprite);
  }
}

void game_colony_drag_begin_cargo(ColonizeGameState* game, int cargo_type) {
  ColonyScreenView* csv = &game->colony_screen;
  ColonizeColony* colony = colonies_get_mut(&game->colonies, game->colony_view_id);
  if (!colony || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return;
  }
  csv->selected_cargo = cargo_type;
  if (csv->transport_unit_id < 0) {
    set_status(game, "Select a ship first", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  if (colony->stock[cargo_type] <= 0) {
    set_status(game, "No cargo in slot", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const int want = colony->stock[cargo_type] < 100 ? colony->stock[cargo_type] : 100;
  ui_drag_begin(
    &game->ui_drag, UI_DRAG_COLONY_CARGO, cargo_type, csv->transport_unit_id, want
  );
  game_ui_drag_set_icon(game, COLONY_CARGO_ICON_BASE + cargo_type);
}

void game_colony_drag_begin_hold(ColonizeGameState* game, int hold_index) {
  ColonyScreenView* csv = &game->colony_screen;
  if (csv->transport_unit_id < 0 || !game->units_ok) {
    set_status(game, "Select a ship first", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const ColonizeUnit* tu = units_get_const(&game->units, csv->transport_unit_id);
  if (!tu || hold_index < 0 || hold_index >= COLONIZE_UNIT_CARGO_MAX) {
    return;
  }
  if (tu->hold_goods_amount[hold_index] <= 0 || tu->hold_goods_amount[hold_index] >= 255) {
    set_status(game, "Hold empty", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const int ctype = tu->hold_goods_type[hold_index];
  ui_drag_begin(
    &game->ui_drag, UI_DRAG_COLONY_HOLD, hold_index, csv->transport_unit_id, 0
  );
  if (ctype >= 0 && ctype < COLONIZE_CARGO_COUNT) {
    game_ui_drag_set_icon(game, COLONY_CARGO_ICON_BASE + ctype);
  }
}

void game_colony_drag_begin_colonist(ColonizeGameState* game, int colonist_index) {
  ColonizeColony* colony = colonies_get_mut(&game->colonies, game->colony_view_id);
  game_colony_select_colonist(game, colonist_index);
  if (!colony || colonist_index < 0 || colonist_index >= colony->colonist_count) {
    return;
  }
  const ColonizeColonist* c = &colony->colonists[colonist_index];
  ui_drag_begin(&game->ui_drag, UI_DRAG_COLONY_COLONIST, colonist_index, -1, 0);
  if (game->units_ok) {
    const int sprite =
      units_working_colonist_sprite(&game->units, c->unit_type_index, c->profession);
    game_ui_drag_set_icon(game, sprite);
  }
}

void game_colony_drag_begin_outside(ColonizeGameState* game, int unit_id) {
  game_colony_select_outside(game, unit_id);
  ui_drag_begin(&game->ui_drag, UI_DRAG_COLONY_OUTSIDE, -1, unit_id, 0);
  if (!game->units_ok) {
    return;
  }
  const ColonizeUnit* u = units_get_const(&game->units, unit_id);
  if (!u) {
    return;
  }
  const ColonizeUnitType* ut = units_type(&game->units, u->type_index);
  int sprite = ut ? ut->icon_sprite : -1;
  if (units_type_is_colonist(ut)) {
    sprite = units_working_colonist_sprite(&game->units, u->type_index, u->profession);
  }
  game_ui_drag_set_icon(game, sprite);
}

/*
 * DOS `FUN_2f2b_2f3e` (2f2b:33dd..33fc) plays exactly one sound in the whole
 * assign-colonist path, and it is gated: the 0x8024 chord only fires when the
 * occupation being handed out is @JOB 0x10 Preacher or @JOB 0x18 Missionary.
 * It is the church chord, not a generic "assigned" click — everything else,
 * field work and every other building, is silent. The port played it on all
 * three assign paths, which is why a Town Hall and a silver mine both sounded
 * like a church (bugs.md). Field jobs are @JOB 0..8, so they never qualify;
 * only Church/Cathedral work does.
 */
#define GAME_ASSIGN_CHORD 0x8024
#define GAME_JOB_PREACHER 0x10
#define GAME_JOB_MISSIONARY 0x18

static void game_colony_assign_job_sound(int job) {
  if (job == GAME_JOB_PREACHER || job == GAME_JOB_MISSIONARY) {
    sound_play(GAME_ASSIGN_CHORD);
  }
}

/* Church/Cathedral are the only workplaces whose occupation is Preacher. */
static void game_colony_assign_building_sound(
  const ColonizeColonyPool* pool,
  int building_index
) {
  const int row = colonies_building_type_row(pool, building_index);
  if (row == COLONY_BUILDING_CHURCH || row == COLONY_BUILDING_CATHEDRAL) {
    game_colony_assign_job_sound(GAME_JOB_PREACHER);
  }
}

/*
 * Shared refusal chrome for a colonies_assign_workplace() that said no —
 * a click on the building itself and an indoor row of the jobs menu
 * (bugs.md #900) must answer with the same popup, in the same DOS validator
 * order (thunk_FUN_1000_9808).
 */
static void game_colony_workplace_refusal(
  ColonizeGameState* game, int ci, int building_index
) {
    const ColonizeColony* col = colonies_get(&game->colonies, game->colony_view_id);
    const ColonizeColonist* c =
      (col && ci >= 0 && ci < col->colonist_count) ? &col->colonists[ci] : NULL;
    /* Mirror of colonies_assign_workplace's DOS validator order
     * (thunk_FUN_1000_9808): occupation caps first, then the school ones,
     * all keyed on the school tier the colony OWNS. bugs.md #580/#589/#590. */
    const int occupation = colonies_building_occupation(&game->colonies, building_index);
    const int school_tier = colonies_school_owned_tier(&game->colonies, col);
    const bool teaching = (occupation == COLONIES_JOB_TEACHER);
    if (c && teaching && school_tier > 0 &&
        colonies_occupation_worker_count(
          &game->colonies, col, COLONIES_JOB_TEACHER, -1
        ) >= school_tier) {
      set_status(game, "School is full", NULL);
      colonies_emit_school_faculty_chrome(school_tier, &game->ai_popups, &game->messages);
    } else if (c && teaching && !colonies_profession_may_teach(c->profession)) {
      set_status(game, "Need a skilled teacher", NULL);
      colonies_emit_noteacher_chrome(&game->ai_popups, &game->messages);
    } else if (
      c && teaching &&
      colonies_school_tier_shortfall(c->profession, school_tier) != 0
    ) {
      const int need = colonies_school_tier_shortfall(c->profession, school_tier);
      set_status(
        game, need >= 3 ? "Need a university" : "Need a college", NULL
      );
      colonies_emit_need_school_chrome(
        c->profession, school_tier, &game->ai_popups, &game->messages
      );
    } else if (c && occupation > 9 &&
               colonies_occupation_worker_count(
                 &game->colonies, col, occupation, ci
               ) > 2) {
      set_status(game, "Building is full", NULL);
      colonies_emit_more_than_three_chrome(col, &game->ai_popups, &game->messages);
    } else {
      set_status(game, "Cannot assign here", NULL);
    }
}

void game_colony_assign_building_drop(ColonizeGameState* game, int building_index) {
  ColonyScreenView* csv = &game->colony_screen;
  if (building_index < 0) {
    set_status(game, "Build it first", NULL);
  } else if (!colonies_building_workable(&game->colonies, building_index)) {
    /* bugs.md: refuse BEFORE resolving the colonist — the resolver admits an
     * outside unit into the colony as a side effect, and a failing drop onto
     * e.g. the Printing Press was consuming it as an invisible unassigned
     * colonist ("vanishes the colonist"). */
    set_status(game, "No one works there", NULL);
  } else {
    const int ci = game_colony_selected_colonist(game);
    if (ci < 0) {
      set_status(game, "Select a colonist first", NULL);
    } else if (colonies_assign_workplace(
                 &game->colonies, game->colony_view_id, ci, building_index
               )) {
      game_colony_assign_building_sound(&game->colonies, building_index);
      const ColonizeBuildingType* bt = colonies_building_type(&game->colonies, building_index);
      snprintf(
        game->status, sizeof(game->status), "Assigned to %s", bt ? bt->name : ""
      );
    } else {
      game_colony_workplace_refusal(game, ci, building_index);
    }
  }
  colony_screen_set_status(csv, game->status);
}

/* Docks / Drydock / Shipyard — same check as colony_preview.c / turn.c. */

/*
 * bugs.md #284 — DOS work-assign complaint (overlays.c ~8789, the colony
 * tiles[] writer): putting a colonist on Indian-claimed land bumps the
 * owning tribe's alarm. base = difficulty+5 (human turn); ×2 when the
 * claiming village stands within 3 tiles, +base again within 2; ×2 on a
 * road. No dialog in DOS — the resentment is the alarm itself; the port
 * adds a status line so the player learns why. (DOS's AI-colony
 * auto-purchase arm is not ported — AI assignment runs elsewhere.)
 */
static void game_colony_indian_land_worked(
  ColonizeGameState* game,
  const ColonizeColony* colony,
  int tile_index
) {
  if (!game || !game->col1_ok || !game->world_map_ok || !colony) {
    return;
  }
  int dx = 0;
  int dy = 0;
  if (!colonies_field_tile_delta(tile_index, &dx, &dy)) {
    return;
  }
  const int x = colony->x + dx;
  const int y = colony->y + dy;
  ColonizeCol1Save* col1 = &game->col1;
  const int ti = colonies_indian_claim_tribe_from_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL)}, colony->nation_id, colony->x, colony->y, x, y);
  if (ti < 0 || !col1->tribe) {
    return;
  }
  const ColonizeCol1Tribe* t = &col1->tribe[ti];
  const int tn = (int)t->nation_id;
  int base = (int)col1->head.difficulty + 5;
  int amt = base;
  const int ddx = abs((int)t->x - x);
  const int ddy = abs((int)t->y - y);
  const int dist = ddx > ddy ? ddx : ddy;
  if (dist < 3) {
    amt = base * 2;
  }
  if (dist < 2) {
    amt += base;
  }
  if (map_tile_has_road(&game->world_map, x, y)) {
    amt *= 2;
  }
  ai_diplo_indian_alarm_delta(col1, tn, colony->nation_id, amt);
  snprintf(
    game->status, sizeof(game->status), "The %s resent the use of their land.",
    ai_contact_tribe_name(tn)
  );
}

/*
 * DOS FUN_2f2b_3fa6, the area-view tile click (bugs.md):
 *  - tile worked by the selected colonist  → the field-jobs popup (348c);
 *  - tile worked by someone else           → just select that colonist;
 *  - empty tile → assign the selected colonist immediately with a default
 *    job: a specific field job 1..7 is kept, while Farmer/Fisherman/non-
 *    field re-derive from the tile (`job > 8 || job == 0 || job == 8`
 *    re-checks ocean_or_high_seas at 3fa6): Fisherman on water, Farmer on
 *    land. Water without Docks raises GAME.TXT @NODOCKS instead (the gate
 *    lives in DOS's assign path 2f3e).
 */
void game_colony_area_tile_drop(
  ColonizeGameState* game,
  ColonizeColony* colony,
  const ColonizeWorldMap* cmap,
  int tile_index
) {
  ColonyScreenView* csv = &game->colony_screen;
  /*
   * DOS FUN_2f2b_3fa6 raw 50917 opens with
   * `if (DS:0x8df0[y][x] == 0)` — the whole click body, including the
   * select-other-colonist and jobs-popup branches, runs only on an unblocked
   * plot. A blocked plot is silently ignored (no popup, no status line), so
   * the port does the same. bugs.md #579.
   */
  {
    const ColonizeWorld w = world_make(
      game->units_ok ? &game->units : NULL, &game->colonies,
      game->world_map_ok ? &game->world_map : NULL, &game->col1, game->col1_ok, NULL, NULL
    );
    if (colonies_plot_blocked_mask(&w, colony, tile_index) != 0) {
      return;
    }
  }
  const int who = (int)colony->tiles[tile_index];
  if (who >= 0 && who < colony->colonist_count) {
    if (who == csv->selected_colonist) {
      colony_screen_open_jobs(csv, &game->colonies, cmap, colony, tile_index);
    } else {
      game_colony_select_colonist(game, who);
    }
    return;
  }
  const bool from_fence = csv->selected_outside_unit >= 0;
  const int ci = game_colony_selected_colonist(game);
  if (ci < 0 || ci >= colony->colonist_count) {
    set_status(game, "Select a colonist first", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  int job = (int)colony->colonists[ci].field_job;
  int dx = 0;
  int dy = 0;
  colonies_field_tile_delta(tile_index, &dx, &dy);
  const bool water =
    cmap && map_tile_is_water((ColonizeWorldMap*)cmap, colony->x + dx, colony->y + dy);
  /* bugs.md #944: admitting a fence unit temporarily runs 28c8 and gives it
   * some other plot's job. That transient choice must not be preserved when
   * the same drag explicitly targets this tile. DOS sees the outside unit's
   * non-field occupation here, so it derives Farmer/Fisherman from the tile. */
  if (from_fence || job < 1 || job > 7) {
    job = water ? COLONIZE_JOB_FISHERMAN : COLONIZE_JOB_FARMER;
  }
  if (job >= COLONIZE_FIELD_JOB_COUNT && job <= COLONIES_JOB_TEACHER) {
    /* bugs.md #900: an indoor row from the jobs menu is a workplace pick, not
     * a field pick. Route it through colonies_assign_workplace so the DOS
     * seating validator (thunk_FUN_1000_9808: faculty cap, @NEEDCOLLEGE /
     * @NEEDUNIVERSITY, @MORETHANTHREE) runs exactly as it does for a click on
     * the building itself. */
    const int building =
      colonies_job_workplace_building(&game->colonies, colony, job);
    if (ci < 0) {
      set_status(game, "Select a colonist first", NULL);
    } else if (colonies_assign_workplace(&game->colonies, game->colony_view_id, ci, building)) {
      game_colony_assign_job_sound(job);
      snprintf(game->status, sizeof(game->status), "Working as %s", colonies_profession_name(job));
    } else {
      game_colony_workplace_refusal(game, ci, building);
    }
    colony_screen_close_jobs(csv);
    colony_screen_set_status(csv, game->status);
    return;
  }
  if (job == COLONIZE_JOB_FISHERMAN && !colony_yield_colony_has_docks(&game->colonies, colony)) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages, "NODOCKS", NULL,
      "",
      body, sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    /* Player-raised from inside the colony screen: present on the spot, or the
     * colony_zoom_popup_hold keeps it queued until the screen closes (#916). */
    game_colony_present_now(game, AI_POPUP_TAG_INFO);
    set_status(game, "No docks", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  if (colonies_assign_field(&game->colonies, game->colony_view_id, ci, tile_index, job)) {
    game_colony_assign_job_sound(job);
    snprintf(game->status, sizeof(game->status), "Working as %s", colony_yield_job_name(job));
    game_colony_indian_land_worked(game, colony, tile_index);
  } else {
    set_status(game, "Cannot assign field", NULL);
  }
  colony_screen_set_status(csv, game->status);
}

/*
 * The construction-list row commit, shared by the mouse row click
 * (COLONY_HIT_CONSTRUCTION_ROW) and Enter on the keyboard-selected row (audit
 * GL-16). The two had drifted: only the mouse copy resolved the unit-build
 * raw codes through colonies_unit_build_info, so keyboard-picking Artillery
 * or a Wagon Train reported "Building project". Those codes (42/43) are not
 * @BUILDING rows, so colonies_building_type() is NULL for them and the name
 * has to come from the unit table either way — nothing about that depends on
 * which device picked the row.
 *
 * Closes the construction panel and publishes the status line on every path.
 */
void game_colony_commit_construction(ColonizeGameState* game, int bid) {
  ColonyScreenView* csv = &game->colony_screen;
  const ColoniesBuildableOpts bopts = game_colony_buildable_opts(game);
  if (colonies_set_construction_ex(&game->colonies, game->colony_view_id, bid, &bopts)) {
    const ColonizeBuildingType* bt = colonies_building_type(&game->colonies, bid);
    const char* uname = NULL;
    if (!bt) {
      colonies_unit_build_info(bid, &uname, NULL, NULL);
    }
    snprintf(
      game->status, sizeof(game->status), "Building %s",
      bt ? bt->name : (uname ? uname : "project")
    );
  } else {
    const ColonizeColony* col_ref = colonies_get(&game->colonies, game->colony_view_id);
    if (col_ref && bid >= 0 && bid < COLONIZE_BUILDING_TYPES_MAX && col_ref->has_building[bid]) {
      const ColonizeBuildingType* bt = colonies_building_type(&game->colonies, bid);
      set_status(game, "Already built", NULL);
      colonies_emit_already_have_chrome(
        col_ref, bt ? bt->name : NULL, &game->ai_popups, &game->messages
      );
    }
  }
  colony_screen_close_construction(csv);
  colony_screen_set_status(csv, game->status);
}

/*
 * The jobs-popup row commit, shared by the mouse row click
 * (COLONY_HIT_JOBS_ROW) and by Enter on the keyboard-selected row (audit
 * GL-15). The two copies had already drifted: only the mouse one carried the
 * Fisherman/@NODOCKS refusal, so picking Fisherman by keyboard on a dockless
 * colony assigned it anyway. DOS has one handler — the click and the key both
 * land in FUN_2f2b_348c and the docks gate sits below that in the assign path
 * (2f3e), where no input device can reach around it. That is the same gate
 * game_colony_area_tile_drop re-tests on an empty-tile drop.
 *
 * Closes the jobs popup on every path. The @LOBOTOMIZE arm leaves the status
 * line alone (the confirm popup is the feedback); every other arm publishes it.
 */
void game_colony_commit_job(ColonizeGameState* game, ColonizeColony* colony, int job) {
  ColonyScreenView* csv = &game->colony_screen;
  const int ci = game_colony_selected_colonist(game);
  if (job == COLONY_JOB_CLEAR_SPECIALTY) {
    /* FUN_2f2b_348c row 0x61 -> FUN_281f_0652(@LOBOTOMIZE): confirm, then
     * FUN_281f_0cae writes profession 0x1c (Free Colonist). */
    if (ci >= 0 && colony && ci < colony->colonist_count) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = colonies_profession_name(colony->colonists[ci].profession);
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        &game->messages, "LOBOTOMIZE", &tok,
        "",
        body, sizeof(body)
      );
      char choice_buf[2][POPUP_MSG_CHOICE_LEN];
      const char* labels[2];
      popup_msg_section_labels(
        &game->messages, "LOBOTOMIZE", &tok, "", "", choice_buf, labels
      );
      const int ids[2] = {1, 2};
      if (ai_popup_enqueue_choice_ctx(
            &game->ai_popups, AI_POPUP_TAG_COLONY_CLEARSPEC, ci, 0, 0, NULL, body, labels, ids, 2
          )) {
        (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_COLONY_CLEARSPEC);
      }
    }
    colony_screen_close_jobs(csv);
    return;
  }
  if (job == COLONIZE_JOB_FISHERMAN && !colony_yield_colony_has_docks(&game->colonies, colony)) {
    /* DOS: picking Fisherman without Docks raises @NODOCKS. */
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages, "NODOCKS", NULL,
      "",
      body, sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    game_colony_present_now(game, AI_POPUP_TAG_INFO);
    set_status(game, "No docks", NULL);
  } else if (ci < 0) {
    set_status(game, "Select a colonist first", NULL);
  } else if (colonies_assign_field(
               &game->colonies, game->colony_view_id, ci, csv->jobs_tile_index, job
             )) {
    game_colony_assign_job_sound(job);
    snprintf(game->status, sizeof(game->status), "Working as %s", colony_yield_job_name(job));
    game_colony_indian_land_worked(
      game, colonies_get(&game->colonies, game->colony_view_id), csv->jobs_tile_index
    );
  } else {
    set_status(game, "Cannot assign field", NULL);
  }
  colony_screen_close_jobs(csv);
  colony_screen_set_status(csv, game->status);
}

void game_colony_fence_drop(ColonizeGameState* game, ColonizeColony* colony) {
  ColonyScreenView* csv = &game->colony_screen;
  if (csv->selected_colonist < 0) {
    if (csv->selected_outside_unit >= 0) {
      const ColonizeUnit* u = game->units_ok
        ? units_get_const(&game->units, csv->selected_outside_unit)
        : NULL;
      if (u && colony) {
        for (int ri = 0; ri < COLONIZE_EJECT_ROLE_COUNT; ++ri) {
          csv->eject_role_enabled[ri] = true;
        }
        csv->eject_role_count = game_colony_list_outside_roles(
          &game->colonies,
          colony,
          u,
          csv->eject_roles,
          csv->eject_role_enabled,
          COLONIZE_EJECT_ROLE_COUNT
        );
        if (csv->eject_role_count <= 0) {
          csv->eject_roles[0] = COLONIZE_EJECT_COLONIST;
          csv->eject_role_enabled[0] = true;
          csv->eject_role_count = 1;
        }
        csv->eject_colonist_index = -1;
        csv->eject_unit_id = u->id;
        csv->eject_selection = 0;
        csv->eject_open = true;
      } else {
        set_status(game, "Select a colonist first", NULL);
        colony_screen_set_status(csv, game->status);
      }
    } else {
      set_status(game, "Select a colonist first", NULL);
      colony_screen_set_status(csv, game->status);
    }
  } else {
    colony_screen_open_eject(csv, &game->colonies, game->colony_view_id, csv->selected_colonist);
  }
}

bool game_colony_drag_drop(
  ColonizeGameState* game,
  ColonizeColony* colony,
  const ColonizeWorldMap* cmap,
  int mx,
  int my,
  bool shift
) {
  ColonyScreenView* csv = &game->colony_screen;
  UiDragSession* drag = &game->ui_drag;
  if (!ui_drag_active(drag) || !colony) {
    game_ui_drag_clear(game);
    return false;
  }
  const ColonyScreenHitResult hit = colony_screen_hit_test(
    csv, &game->colonies, colony, game->units_ok ? &game->units : NULL, mx, my
  );
  const UiDragKind kind = drag->kind;

  if (kind == UI_DRAG_COLONY_CARGO) {
    if (hit.kind == COLONY_HIT_HOLD || hit.kind == COLONY_HIT_TRANSPORT) {
      int tid = csv->transport_unit_id;
      if (hit.kind == COLONY_HIT_TRANSPORT && hit.index >= 0 &&
          hit.index < csv->docked_transport_count) {
        tid = csv->docked_transport_ids[hit.index];
        csv->transport_unit_id = tid;
      }
      if (tid >= 0 && game->units_ok && shift) {
        /* Shift+drag: @HOWMUCH1 amount entry instead of the whole slot. */
        game_colony_open_load_prompt(game, colony, drag->index);
        game_ui_drag_clear(game);
        return true;
      }
      if (tid >= 0 && game->units_ok) {
        game_colony_load_hold(
          game, tid, drag->index, drag->amount > 0 ? drag->amount : 100
        );
        colony_screen_set_status(csv, game->status);
      }
    }
  } else if (kind == UI_DRAG_COLONY_HOLD) {
    if (hit.kind == COLONY_HIT_TRANSPORT && hit.index >= 0 &&
        hit.index < csv->docked_transport_count &&
        csv->docked_transport_ids[hit.index] != csv->transport_unit_id) {
      /* bugs.md #432: hold → another docked transport's sprite moves the
       * cargo ship-to-ship (if the target has a free hold), never through
       * the warehouse. */
      const int dst = csv->docked_transport_ids[hit.index];
      if (csv->transport_unit_id >= 0 && game->units_ok && shift) {
        /* Shift+drag: @HOWMUCH3 amount entry instead of the whole hold
         * ("moved from %STRING1 to %STRING2"). */
        const ColonizeUnit* su = units_get_const(&game->units, csv->transport_unit_id);
        const int hold = drag->index;
        if (su && hold >= 0 && hold < COLONIZE_UNIT_CARGO_MAX &&
            su->hold_goods_amount[hold] > 0 && su->hold_goods_amount[hold] < 255) {
          const int ctype = su->hold_goods_type[hold];
          const int max_amt = su->hold_goods_amount[hold];
          char prompt[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = (game->europe_ok && ctype >= 0 && ctype < game->europe.cargo_count)
                          ? game->europe.cargo[ctype].name
                          : "";
          const ColonizeUnit* du = units_get_const(&game->units, dst);
          tok.string1 = units_display_name(&game->units, su);
          tok.string2 = du ? units_display_name(&game->units, du) : "";
          tok.number0 = max_amt;
          tok.has_number0 = true;
          popup_msg_fill(
            &game->messages, "HOWMUCH3", &tok, "", prompt,
            sizeof(prompt)
          );
          game->howmuch_move_dst_unit_id = dst;
          howmuch_open(
            &game->howmuch,
            HOWMUCH_KIND_MOVE,
            prompt,
            howmuch_amount_label(&game->messages, "HOWMUCH3", ""),
            max_amt,
            max_amt,
            ctype,
            hold
          );
        }
        game_ui_drag_clear(game);
        return true;
      }
      if (csv->transport_unit_id >= 0 && game->units_ok) {
        int ctype = -1;
        int amt = 0;
        const ColonizeUnit* su = units_get_const(&game->units, csv->transport_unit_id);
        if (su && drag->index >= 0 && drag->index < COLONIZE_UNIT_CARGO_MAX) {
          ctype = su->hold_goods_type[drag->index];
          amt = su->hold_goods_amount[drag->index];
        }
        if (amt > 0 && amt < 255) {
          const int moved = units_load_goods(&game->units, dst, ctype, amt);
          if (moved > 0) {
            (void)units_unload_goods_hold(
              &game->units, csv->transport_unit_id, drag->index, NULL, NULL
            );
            if (moved < amt) {
              /* Partial fit: put the remainder back in the source hold. */
              (void)units_load_goods(&game->units, csv->transport_unit_id, ctype, amt - moved);
            }
            snprintf(game->status, sizeof(game->status), "Transferred %d", moved);
          } else {
            set_status(game, "No empty hold", NULL);
          }
        } else {
          set_status(game, "Hold empty", NULL);
        }
        colony_screen_set_status(csv, game->status);
      }
      game_ui_drag_clear(game);
      return true;
    }
    if (hit.kind == COLONY_HIT_CARGO_SLOT ||
        (hit.kind == COLONY_HIT_NONE && my >= COLONY_CARGO_STRIP_Y)) {
      if (csv->transport_unit_id >= 0 && game->units_ok && shift) {
        /* Shift+drag: @HOWMUCH2 amount entry instead of the whole hold. */
        const ColonizeUnit* tu = units_get_const(&game->units, csv->transport_unit_id);
        const int hold = drag->index;
        if (tu && hold >= 0 && hold < COLONIZE_UNIT_CARGO_MAX &&
            tu->hold_goods_amount[hold] > 0 && tu->hold_goods_amount[hold] < 255) {
          const int ctype = tu->hold_goods_type[hold];
          const int max_amt = tu->hold_goods_amount[hold];
          char prompt[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = (game->europe_ok && ctype >= 0 && ctype < game->europe.cargo_count)
                          ? game->europe.cargo[ctype].name
                          : "";
          tok.string1 = units_display_name(&game->units, tu);
          tok.string2 = colony->name[0] ? colony->name : "";
          tok.number0 = max_amt;
          tok.has_number0 = true;
          popup_msg_fill(
            &game->messages, "HOWMUCH2", &tok, "", prompt,
            sizeof(prompt)
          );
          howmuch_open(
            &game->howmuch,
            HOWMUCH_KIND_UNLOAD,
            prompt,
            howmuch_amount_label(&game->messages, "HOWMUCH2", ""),
            max_amt,
            max_amt,
            ctype,
            hold
          );
        }
        game_ui_drag_clear(game);
        return true;
      }
      if (csv->transport_unit_id >= 0 && game->units_ok) {
        game_colony_unload_hold(game, csv->transport_unit_id, drag->index, "Hold empty");
        colony_screen_set_status(csv, game->status);
      }
    }
  } else if (kind == UI_DRAG_COLONY_COLONIST || kind == UI_DRAG_COLONY_OUTSIDE) {
    if (hit.kind == COLONY_HIT_BUILDING) {
      game_colony_assign_building_drop(game, hit.index);
    } else if (hit.kind == COLONY_HIT_AREA_TILE) {
      game_colony_area_tile_drop(game, colony, cmap, hit.index);
    } else if (hit.kind == COLONY_HIT_FENCE) {
      game_colony_fence_drop(game, colony);
    }
  }

  game_ui_drag_clear(game);
  return true;
}
/* ===================== Europe voyages, colony roles, dock orders (game_voyage_ship_count .. game_center_on_selected_unit) ===================== */


/*
 * The ship count FUN_48d3_0002 gates on. DOS reads DS:0x9418[nation]
 * (viceroy_unpacked.asm 48d3:003b `cmp byte [bx+0x9418],0x3`), the per-nation
 * hull tally FUN_4962_0018 builds by walking the WHOLE unit array and bumping
 * 0x9418 for every unit of that nation whose type is 0x0d..0x12
 * (viceroy_unpacked.asm 4962:0300-0365). DOS parks a crossing ship as a live
 * unit on its nation's Europe sentinel diagonal (228/232/244+n), so hulls in
 * harbour, expected and bound are all inside that tally; this port hoists
 * them out of the unit pool into EuropeScreen instead, so the live-pool walk
 * alone under-counts. Together with the departing ship being despawned before
 * the roll (see game_ship_sail_to_europe), that made the `> 2` slow-voyage
 * gate unreachable for a three-ship player.
 */
int game_voyage_ship_count(const ColonizeGameState* game) {
  const int hn = game->human_nation;
  int ships = game->units_ok ? units_count_sea_for_nation(&game->units, hn) : 0;
  if (game->europe_ok && (int)game->europe.bound_nation == hn) {
    ships += game->europe.harbor_ships + game->europe.expected_ships + game->europe.bound_ships;
  }
  return ships;
}

/* FUN_48d3_0002 via 291f_0aee: shared by both crossing directions. */
int game_voyage_turns_for(ColonizeGameState* game, int ships) {
  const int hn = game->human_nation;
  const bool magellan = game->col1_ok && hn >= 0 && hn < 4 &&
    founding_fathers_nation_has(&game->col1, hn, FF_FERDINAND_MAGELLAN);
  return europe_voyage_turns_roll(&game->move_rng, magellan, ships);
}

int game_voyage_turns(ColonizeGameState* game) {
  return game_voyage_turns_for(game, game_voyage_ship_count(game));
}

void game_europe_sail_harbor(ColonizeGameState* game, int hidx) {
  EuropeScreen* eu = &game->europe;
  if (eu->harbor_ships <= 0 || hidx < 0 || hidx >= eu->harbor_ships) {
    snprintf(eu->status, sizeof(eu->status), "%s", "No ships in harbor.");
    return;
  }
  if (!game->units_ok) {
    snprintf(eu->status, sizeof(eu->status), "%s", "Cannot sail: units unavailable.");
    return;
  }
  EuropeHarborShip* hs = &eu->harbor[hidx];
  if (hs->type_index < 0) {
    const int resolved = units_find_type(&game->units, hs->name);
    if (resolved >= 0) {
      hs->type_index = resolved;
    }
  }
  europe_set_sail_from_harbor(eu, hidx, game_voyage_turns(game), &game->units, game->human_nation);
  /* bugs.md: sailing the LAST docked ship closes the European Status back
   * to the map — nothing is left to manage there. */
  if (eu->harbor_ships <= 0 && game->in_europe) {
    game->in_europe = false;
  }
}

/* bugs.md: DOS confirms before leaving port — GAME.TXT @SAILAWAY ("Shall we
 * set sail for the New World?" Yes/No). Every sail entry point asks first. */
void game_europe_request_sail(ColonizeGameState* game, int hidx) {
  EuropeScreen* eu = &game->europe;
  if (eu->harbor_ships <= 0 || hidx < 0 || hidx >= eu->harbor_ships) {
    snprintf(eu->status, sizeof(eu->status), "%s", "No ships in harbor.");
    return;
  }
  game_enqueue_yes_no(
    game,
    GAME_MAP_CONFIRM_EUROPE_SAIL,
    hidx,
    "SAILAWAY",
    "",
    NULL
  );
}

bool game_europe_drag_drop(ColonizeGameState* game, int mx, int my, bool shift) {
  EuropeScreen* eu = &game->europe;
  UiDragSession* drag = &game->ui_drag;
  if (!ui_drag_active(drag)) {
    game_ui_drag_clear(game);
    return false;
  }
  const EuropeHitResult hit = game_europe_hit(game, mx, my);
  const UiDragKind kind = drag->kind;

  if (kind == UI_DRAG_EUROPE_MARKET) {
    if (hit.kind == EUROPE_HIT_HOLD || hit.kind == EUROPE_HIT_HARBOR_SHIP) {
      int hidx = eu->selected_harbor;
      if (hit.kind == EUROPE_HIT_HARBOR_SHIP && hit.index >= 0) {
        hidx = hit.index;
        eu->selected_harbor = hidx;
      }
      if (hidx < 0 && eu->harbor_ships > 0) {
        hidx = 0;
        eu->selected_harbor = 0;
      }
      if (hidx >= 0 && shift) {
        /* Shift+drag: @HOWMUCH4 amount entry instead of a full 100 load
         * (same prompt as the keyboard path). */
        game_europe_open_buy_prompt_for(game, hidx, drag->index);
      } else if (hidx >= 0) {
        europe_buy_cargo_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .col1=(ColonizeCol1Save*)(&game->col1), .col1_ok=true, .europe=(EuropeScreen*)(eu)}, game->human_nation, hidx, drag->index, drag->amount > 0 ? drag->amount : 100);
        game_europe_drain_price_events(game);
      } else {
        snprintf(eu->status, sizeof(eu->status), "%s", "Select a ship first.");
      }
    }
  } else if (kind == UI_DRAG_EUROPE_HOLD) {
    if (hit.kind == EUROPE_HIT_MARKET) {
      if (eu->selected_harbor >= 0 && shift) {
        /* Shift+drag: @HOWMUCH5 amount entry instead of selling the whole hold. */
        const EuropeHarborShip* ship = &eu->harbor[eu->selected_harbor];
        const int hold = drag->index;
        if (hold >= 0 && hold < EUROPE_SHIP_CARGO_MAX && ship->hold_goods_amount[hold] > 0 &&
            ship->hold_goods_amount[hold] < 255) {
          const int ctype = ship->hold_goods_type[hold];
          const int max_amt = ship->hold_goods_amount[hold];
          char prompt[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "";
          tok.string2 = "dockside buyers";
          tok.number0 = max_amt;
          tok.has_number0 = true;
          tok.number1 = europe_sell_price(eu, ctype);
          tok.has_number1 = true;
          popup_msg_fill(
            &game->messages, "HOWMUCH5", &tok, "", prompt, sizeof(prompt)
          );
          howmuch_open(
            &game->howmuch,
            HOWMUCH_KIND_SELL,
            prompt,
            howmuch_amount_label(&game->messages, "HOWMUCH5", ""),
            max_amt,
            max_amt,
            ctype,
            hold
          );
        }
      } else if (eu->selected_harbor >= 0) {
        europe_sell_hold(eu, &game->col1, game->human_nation, eu->selected_harbor, drag->index);
        game_europe_drain_price_events(game);
      }
    }
  } else if (kind == UI_DRAG_EUROPE_HARBOR_SHIP) {
    if (hit.kind == EUROPE_HIT_BOUND) {
      game_europe_request_sail(game, drag->index);
    }
  } else if (kind == UI_DRAG_EUROPE_EXPECTED_SHIP) {
    if (hit.kind == EUROPE_HIT_BOUND) {
      europe_reverse_transit(eu, true, drag->index);
    }
  } else if (kind == UI_DRAG_EUROPE_BOUND_SHIP) {
    if (hit.kind == EUROPE_HIT_EXPECTED) {
      europe_reverse_transit(eu, false, drag->index);
    }
  }

  game_ui_drag_clear(game);
  return true;
}

/*
 * Church-or-Cathedral bless gate, the same test colony.c's
 * colonies_has_church_or_cathedral runs for the inside-colonist twin of this
 * list. DOS asks only for the Church bit (FUN_15eb_3454's Missionary arm:
 * `FUN_15eb_038e(0x25)`, building-catalog index 0x25 = Church, 0x26 =
 * Cathedral — the indices confirmed with the Stable/Printing Press ones in
 * docs/building_production.md), which is sufficient there because a
 * completed upgrade only ever SETS its own bit (FUN_364b_0114 →
 * FUN_281f_0bbe(building, 1), never a clear) and Church is Cathedral's
 * prerequisite, so a Cathedral colony still has the Church bit. The
 * disjunction is the same thing for any reachable colony and survives a
 * bridged save that carries only the upgrade.
 */

/*
 * The "Leave as" row list for a unit standing on the fence.
 *
 * DOS builds this and the inside-colonist list in ONE function,
 * FUN_2f2b_348c: `param_1 != 0` selects the 6-row leave-as mode (rows =
 * professions 0x13..0x18 = Colonist / Pioneer / Soldier / Scout / Dragoon /
 * Missionary; `local_fa = 0x13, local_146 = 6`), and a band index at or past
 * the colony's colonist count — i.e. an OUTSIDE unit — forces that mode. Rows
 * are gated one by one through FUN_281f_0bb4 → FUN_15eb_3454, whose >= 0x13
 * arm is profession-indexed and knows nothing about where the body stands:
 * the gear rows test colony stock against FUN_15eb_0d8e's cargo list (tools
 * 0x14 = 20, muskets/horses 0x32 = 50) and row 0x18 tests the Church bit. So
 * the outside list is the inside list, Missionary included, and this copy
 * must stay row-for-row identical to colonies_list_eject_roles (colony.c) —
 * it dropped the Missionary row until 2026-09-10, which let a colonist be
 * blessed inside the colony but not one pixel outside it.
 */
static int game_colony_list_outside_roles(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnit* unit,
  int* out_roles,
  bool* out_enabled,
  int out_max
) {
  if (!colony || !unit || !out_roles || out_max <= 0) {
    return 0;
  }
  /* bugs.md #649: FUN_15eb_3454's >= 0x13 arm (raw 13583) gates every row on
   * DS:0x8542 colony warehouse stock alone — it never adds the standing
   * unit's own carried gear, so an outside Dragoon on an empty-warehouse
   * colony sees the same greyed rows an inside colonist would. The apply-time
   * check (game_colony_apply_outside_role) still refunds the unit's gear into
   * stock before testing, matching FUN_2f2b_348c's post-refund apply. */
  return colonies_list_eject_roles_gear(
    pool,
    colony,
    0,
    0,
    0,
    unit->profession,
    out_roles,
    out_enabled,
    out_max
  );
}

bool game_colony_apply_outside_role(
  const ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeUnitPool* units,
  int unit_id,
  int role
) {
  if (!colony || !units) {
    return false;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u) {
    return false;
  }
  int stock_tools = colony->stock[COLONIZE_CARGO_TOOLS] + (u->tools > 0 ? u->tools : 0);
  int stock_muskets = colony->stock[COLONIZE_CARGO_MUSKETS] + (u->muskets > 0 ? u->muskets : 0);
  int stock_horses = colony->stock[COLONIZE_CARGO_HORSES] + (u->horses > 0 ? u->horses : 0);

  int tools_take = 0;
  int muskets_take = 0;
  int horses_take = 0;
  const char* type_name = units_equip_role_type_name(units, u->type_index, role);
  int type_index_override = -1;
  switch (role) {
  case COLONIZE_EJECT_MISSIONARY:
    /*
     * FUN_15eb_3454 row 0x18: the bless costs no cargo, and its only gate is
     * the Church bit — or a body that is already a Jesuit (raw 13567-13569),
     * which is offered the row in a churchless colony too. Re-tested here the
     * way colonies_eject_colonist re-tests it, so a row that went stale
     * between build and click cannot bless. units_equip_role_type_name has no Missionary arm (DOS re-types
     * gear changes through the @JOB->@UNIT table, which the bless does not
     * use), so name the type outright, exactly as the inside twin does.
     * The body's profession is left alone: DOS's "Cancel Missionary Status"
     * gate turns on profession != 0x18 (a blessed ordinary colonist can
     * cancel, a born Jesuit cannot), which only works because blessing never
     * writes the profession byte — and this path has never re-professioned a
     * body for any other row either.
     */
    if (!colonies_eject_row_offered(pool, colony, u->profession, role)) {
      return false;
    }
    type_index_override = units_kind_type_index(units, UNITS_KIND_MISSIONARY);
    break;
  case COLONIZE_EJECT_COLONIST:
    /* raw 13561-13565: the Colonist row is not offered to a Jesuit under the
     * DS:0x8dc6 test — see colonies_eject_row_offered. */
    if (!colonies_eject_row_offered(pool, colony, u->profession, role)) {
      return false;
    }
    (void)colonies_eject_role_gear(role, stock_tools, &tools_take, &muskets_take, &horses_take);
    break;
  case COLONIZE_EJECT_PIONEER:
  case COLONIZE_EJECT_SOLDIER:
  case COLONIZE_EJECT_SCOUT:
  case COLONIZE_EJECT_DRAGOON:
    /* bugs.md #356: the Pioneer takes whole 20-tool steps capped at 100,
     * the same rule colonies_eject_colonist uses — this path used to insist
     * on the full 100 and refused a Pioneer the menu beside it had just
     * offered. The shared helper owns the rule (FUN_15eb_1068, see
     * colonies_equip_tools_take in colony.c). */
    (void)colonies_eject_role_gear(role, stock_tools, &tools_take, &muskets_take, &horses_take);
    break;
  default:
    /* Not a row FUN_2f2b_348c can offer (professions 0x13..0x18). The old
     * `COLONIZE_EJECT_COLONIST, default:` pair let any other id through and
     * re-typed the unit to whatever units_equip_role_type_name defaulted to
     * while taking no cargo — a free type change from a row that does not
     * exist. */
    return false;
  }

  if (role == COLONIZE_EJECT_PIONEER && tools_take <= 0) {
    return false; /* not even one 20-tool step in reach */
  }
  if (stock_tools < tools_take || stock_muskets < muskets_take || stock_horses < horses_take) {
    return false;
  }

  colony->stock[COLONIZE_CARGO_TOOLS] = stock_tools - tools_take;
  colony->stock[COLONIZE_CARGO_MUSKETS] = stock_muskets - muskets_take;
  colony->stock[COLONIZE_CARGO_HORSES] = stock_horses - horses_take;

  int type_index = type_index_override >= 0 ? type_index_override : units_find_type(units, type_name);
  if (type_index < 0) {
    type_index = u->type_index;
  }
  u->type_index = type_index;
  u->tools = tools_take;
  u->muskets = muskets_take;
  u->horses = horses_take;
  /* bugs.md #650: FUN_2f2b_348c's tail (raw 50674) calls FUN_15eb_1068
   * unconditionally for the picked row, and FUN_15eb_1068's case 1 body
   * (raw 11276) always calls FUN_1427_155e (raw 8880-8887, moves-spent :=
   * full allotment) — there is no "did anything change" test in DOS. Picking
   * the row the unit already stands in still exhausts its moves and clears
   * its standing order, same as any other row. */
  u->moves = 0;
  units_clear_orders(units, u->id);
  return true;
}

/*
 * DOS-LITERAL thunk_FUN_1000_99b8 case 5 (viceroy_overlays.asm OVL03 0x543c):
 *
 *   while (holds_occupied != 0) {
 *     goods = FUN_15eb_2ff2(unit, 0);           // the FIRST hold's goods type
 *     if (thunk_FUN_1000_9784(unit, goods) != 0) break;   // refused
 *   }
 *
 * bugs.md #785: the old body walked holds 0..n-1 unconditionally and could
 * not stop, so a declined @WAREHOUSEFULL on one hold still emptied the rest.
 * DOS always takes the first occupied hold (it compacts as it unloads) and
 * ends the whole loop the moment one unload returns non-zero — which is what
 * "Never mind." returns. Each step goes through the same single-hold path, so
 * the gate is asked per hold; the answer resumes this loop.
 */
void game_colony_unload_all_cargo(ColonizeGameState* game, int unit_id) {
  if (!game || !game->units_ok || game->colony_view_id < 0) {
    return;
  }
  const int holds = units_goods_hold_count(&game->units, unit_id);
  int total = 0;
  for (int guard = 0; guard < holds; ++guard) {
    const ColonizeUnit* tu = units_get_const(&game->units, unit_id);
    if (!tu) {
      break;
    }
    int hold = -1;
    for (int i = 0; i < holds && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
      if (tu->hold_goods_amount[i] > 0 && tu->hold_goods_amount[i] < 255) {
        hold = i;
        break;
      }
    }
    if (hold < 0) {
      break; /* holds_occupied == 0 */
    }
    /* Bit 20 = "this ask came from Unload all cargo": answer 2 resumes here. */
    if (game_colony_unload_ask(game, unit_id, hold, 0, 1 << 20)) {
      return;
    }
    const int moved = game_colony_unload_hold_commit(game, unit_id, hold, 0, NULL);
    if (moved <= 0) {
      break; /* non-zero return: stop the whole loop */
    }
    total += moved;
  }
  if (total > 0) {
    snprintf(game->status, sizeof(game->status), "Unloaded %d", total);
  } else {
    set_status(game, "No cargo to unload", NULL);
  }
}

/*
 * Colony docked-unit orders popup apply (DOS FUN_2f2b_5746). "Move to front"
 * ports as (re)select the colony's active docked transport — the port has no
 * persistent dock queue for a literal front-of-line reorder.
 */
/*
 * Fortify the selected unit, owning the ship/land chrome split (audit GL-20:
 * three implementations of one order — the two MENU.TXT rows and key F — with
 * three different status strings).
 *
 * MENU.TXT @ORDERS lists "~Fortify" twice, land (0x302) then ship (0x303), and
 * DOS runs both through FUN_2b5a_1112: one order, one @ORDERS letter F
 * (Fortify -> Fortified). DOS arms no status line for either row (the DS:0x2d54
 * channel is untouched there), so both strings below are port chrome; the ship
 * wording is taken from the one place DOS words the ship variant, GAME.TXT
 * @SHIPOPTIONS line 1782 `Anchor in harbor ("Fortify")`. map_menu hides the
 * ship row for a land unit and the land row for a ship, so the sea test here
 * only ever picks the wording the player already saw on the row he clicked.
 *
 * Returns true when the order took; the caller decides whether to advance to
 * the next unit awaiting orders.
 */
/*
 * bugs.md #691. FUN_2b5a_1112's head (viceroy_unpacked.c raw 42389-42411):
 * before writing order 5 a LAND unit (`type < 0x0d || type > 0x12`) walks the
 * 8 neighbour tiles in DS:0xb4/0xbe order and stops at the first one that is
 * on the map (FUN_281f_0302), not water (FUN_281f_0768), carries a European
 * colony whose owner (FUN_281f_0696) is some other nation, AND whose relation
 * byte has the treaty bit (FUN_281f_0a38 & 0x40). That nation goes into
 * @HAVETREATY (DS:0x932, FUN_281f_0652(...,1)); an answer other than 2 returns
 * WITHOUT fortifying, while 2 sets the war bit and clears 0x40 through
 * FUN_281f_0a10 and then falls into the normal tail.
 *
 * Returns true when a confirm is now on the queue and the caller must stop.
 */
bool game_fortify_treaty_confirm(ColonizeGameState* game, int uid) {
  if (!game || !game->units_ok || !game->col1_ok || uid < 0) {
    return false;
  }
  if (units_is_sea(&game->units, uid)) {
    return false; /* DOS's `0x0d..0x12` ship gate guards this scan only */
  }
  const ColonizeUnit* u = units_get_const(&game->units, uid);
  if (!u || !u->active || u->nation_id < 0 || u->nation_id > 3) {
    return false;
  }
  if (ai_popup_pending(
        &game->ai_popups, AI_POPUP_TAG_FORTIFY_TREATY, -1, AI_POPUP_KEY_NATION_A, uid, 0
      )) {
    return true;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int partner = -1;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    if (!map_in_bounds(&game->world_map, nx, ny) ||
        map_tile_is_water(&game->world_map, nx, ny)) {
      continue;
    }
    const ColonizeColony* c = colonies_get(&game->colonies, colonies_id_at(&game->colonies, nx, ny));
    if (!c || !c->active || c->nation_id < 0 || c->nation_id > 3 ||
        c->nation_id == u->nation_id) {
      continue;
    }
    if ((ai_diplo_read(&game->col1, u->nation_id, c->nation_id) & AI_DIPLO_PEACE) != 0) {
      partner = c->nation_id;
      break;
    }
  }
  if (partner < 0) {
    return false;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = reports_nation_adjective_display_name(partner);
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, "HAVETREATY", &tok, "", body, sizeof(body));
  char label_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(
    &game->messages, "HAVETREATY", &tok, "", "", label_buf, labels
  );
  static const int ids[2] = {0, 1};
  if (!ai_popup_enqueue_choice_ctx(
        &game->ai_popups, AI_POPUP_TAG_FORTIFY_TREATY, uid, partner, 0, NULL, body, labels,
        ids, 2
      )) {
    return false; /* queue full: DOS has no such state, fall through and dig in */
  }
  (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_FORTIFY_TREATY);
  return true;
}

bool game_order_fortify(ColonizeGameState* game, int uid) {
  const bool ship = uid >= 0 && game->units_ok && units_is_sea(&game->units, uid);
  if (game_fortify_treaty_confirm(game, uid)) {
    set_status(game, "", NULL);
    return false;
  }
  if (uid < 0 || !units_order_fortify(&game->units, uid)) {
    set_status(game, ship ? "Cannot anchor" : "Cannot fortify", NULL);
    return false;
  }
  set_status(game, ship ? "Anchoring in harbor" : "Fortifying", NULL);
  return true;
}

void game_colony_apply_dock_order(
  ColonizeGameState* game,
  ColonyScreenView* csv,
  ColonyDockOrderAction action
) {
  if (!game || !csv || !game->units_ok) {
    return;
  }
  const int uid = csv->dock_orders_unit_id;
  if (!units_get(&game->units, uid)) {
    colony_screen_close_dock_orders(csv);
    return;
  }
  switch (action) {
  case COLONY_DOCK_ORDER_ACTIVATE:
    if (units_is_sea(&game->units, uid) ||
        units_is_transport(&game->units, uid)) {
      csv->transport_unit_id = uid;
      set_status(game, "Transport selected", NULL);
    } else {
      /* bugs.md: on a land/military unit "Move to front" was a no-op —
       * it now heads the queue: first pick for a departing ship and the
       * Units-pane selection.
       *
       * Audit 2026-09-14 (section 3 incidental): pool->board_first_slot
       * (then named board_first_id) is CONSUMED as a pool SLOT, not a unit id —
       * units_ship_departure_pickup seeds its walk order with it and then
       * skips `i == board_first_slot` while indexing pool->units[i] directly.
       * Unit ids are handed out from pool->next_id and do not track slots
       * (units_get scans for a matching u->id), so writing uid here put a
       * random OTHER unit at the head of the boarding queue and left the
       * flagged one in ordinary chain order. Write the slot. */
      game->units.board_first_slot = -1;
      for (int bi = 0; bi < COLONIZE_UNITS_MAX; ++bi) {
        if (game->units.units[bi].active && game->units.units[bi].id == uid) {
          game->units.board_first_slot = bi;
          break;
        }
      }
      csv->multi_unit_selected_id = uid;
      set_status(game, "Moved to front", NULL);
    }
    break;
  case COLONY_DOCK_ORDER_CLEAR:
    units_wake(&game->units, uid);
    set_status(game, "Orders cleared", NULL);
    break;
  case COLONY_DOCK_ORDER_SENTRY:
    units_order_sentry(&game->units, uid);
    set_status(game, "", NULL);
    break;
  case COLONY_DOCK_ORDER_FORTIFY:
    /* Same order and the same ship/land chrome the map rows use (GL-20). The
     * sea branch here went through units.c's anchor wrapper, whose only
     * content over plain fortify was that sea test (GL-21). */
    (void)game_order_fortify(game, uid);
    break;
  case COLONY_DOCK_ORDER_UNLOAD_ALL:
    game_colony_unload_all_cargo(game, uid);
    break;
  case COLONY_DOCK_ORDER_CANCEL:
  default:
    break;
  }
  colony_screen_close_dock_orders(csv);
  colony_screen_set_status(csv, game->status);
}

void game_center_on_selected_unit(ColonizeGameState* game) {
  const ColonizeUnit* selected = units_get_const(&game->units, game->units.selected_id);
  if (!selected || !selected->active) {
    set_status(game, "No active unit to center on", NULL);
    return;
  }
  game->map_cursor_x = selected->x;
  game->map_cursor_y = selected->y;
  game_set_view_center(game, selected->x, selected->y);
  set_status(game, "Centered on active unit", NULL);
}
