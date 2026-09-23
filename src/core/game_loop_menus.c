#include "core/internal.h"
#include "core/game_loop.h"

/*
 * Split out of game_loop.c (2026-09-23) — code moved verbatim.
 *
 * Sections:
 *  - Cheat menu, trade-route wizard, cheat-list & save/load popups
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

/* ===================== Cheat menu, trade-route wizard, cheat-list & save/load popups (game_fog_nation .. load_begin_menu) ===================== */


/* Fog nation for map paint: special view override, else human. */
int game_fog_nation(const ColonizeGameState* game) {
  if (!game) {
    return 0;
  }
  if (game->fog_view == -1) {
    return -1; /* Complete Map */
  }
  if (game->fog_view >= 0 && game->fog_view <= 3) {
    return game->fog_view;
  }
  return game->human_nation; /* NORMAL (-2) or invalid */
}

static void game_apply_setview(ColonizeGameState* game, int view_id, const char* label) {
  if (!game) {
    return;
  }
  game->fog_view = view_id;
  if (game->col1_ok) {
    game->col1.head.show_entire_map = (uint16_t)(view_id == -2 ? 0 : 1);
  }
  if (view_id == -2) {
    /* DEBUG.TXT @SETVIEW row 6 ("No Special View") — `label` is already the
     * live catalog row text from cheat_list_open_setview; literal kept as
     * fallback for missing/empty label (no data dir). */
    set_status(game, (label && label[0]) ? label : "", NULL);
  } else if (label && label[0]) {
    set_status(game, "", label);
  } else {
    set_status(game, "Viewpoint changed", NULL);
  }
}

static void game_apply_kill_indians(ColonizeGameState* game, int nation_id, const char* label) {
  if (!game || !game->col1_ok) {
    set_status(game, "No Indians", NULL);
    return;
  }
  /* Count units before wipe so empty-tribe status is accurate. */
  int unit_n = 0;
  if (game->units_ok) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &game->units.units[i];
      if (u->active && u->nation_id == nation_id) {
        unit_n++;
      }
    }
  }
  const int removed = col1_kill_indian_nation_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(game->units_ok ? &game->units : NULL), .map=(ColonizeWorldMap*)(game->world_map_ok ? &game->world_map : NULL), .col1=(ColonizeCol1Save*)(&game->col1), .col1_ok=true}, nation_id);
  if (removed <= 0 && unit_n <= 0) {
    set_status(game, "No Indians of that tribe", label);
  } else if (label && label[0]) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Killed %s", label);
    set_status(game, buf, NULL);
  } else {
    set_status(game, "Indians killed", NULL);
  }
}

void game_open_cheat_setview(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  if (!cheat_list_open_setview(
        &game->cheat_list, game->debug_txt_ok ? &game->debug_txt : NULL
      )) {
    set_status(game, "Reveal Map unavailable", NULL);
  }
}

void game_open_cheat_kill_indians(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  if (!cheat_list_open_kill_indians(
        &game->cheat_list, game->names_ok ? &game->names : NULL
      )) {
    set_status(game, "Kill Indians unavailable", NULL);
  }
}

/*
 * CHEAT Create Unit. The list TEXT is DEBUG.TXT @CREATE / @CREATE2 / @CSHIP /
 * @FOREIGN (cheat_list_catalog_rows); these tables hold only the @UNIT ROW
 * each entry spawns — the port spells no unit names itself. -1 = resolved by
 * a follow-up stage (Treasure amount, Ship picker, Foreign nation picker).
 * When DEBUG.TXT is missing a row falls back to the spawned kind's NAMES.TXT
 * @UNIT name (bugs.md #543), else "".
 *
 * DOS dispatcher (ndisasm 0x23996-0x23b8c, jump table cs:0x2cba): the menu tag
 * is @CREATE2 once the WoI is declared (`DS:0x5382 & 1`), else @CREATE. Rows
 * 10-13 (ids 9-12) are the four Indian brave types before the declaration and
 * Cont. Army (9) / Cont. Cav. (7) owned by the human (DS:0x5398) then
 * Regulars (6) / Cavalry (8) owned by the crown slot (DS:0x53d2) after it.
 * @CREATE row 14 "Foreign Unit" only picks the OWNER nation (@FOREIGN, DS
 * `[bp-0x16]`) and loops back to the main list; @CREATE2 has no such row
 * (its @FOREIGN2 arm is unreachable). bugs.md #666.
 */
static const int k_cheat_create_main_kinds[14] = {
  UNITS_KIND_COLONIST, UNITS_KIND_PIONEER, UNITS_KIND_SOLDIER, UNITS_KIND_MISSIONARY,
  UNITS_KIND_SCOUT, UNITS_KIND_ARTILLERY, UNITS_KIND_WAGON, -1 /* Treasure */, -1 /* Ship */,
  UNITS_KIND_BRAVE, UNITS_KIND_ARMED_BRAVE, UNITS_KIND_MTD_BRAVE, UNITS_KIND_MTD_WARRIOR,
  -1 /* Foreign */
};
static const int k_cheat_create_woi_kinds[13] = {
  UNITS_KIND_COLONIST, UNITS_KIND_PIONEER, UNITS_KIND_SOLDIER, UNITS_KIND_MISSIONARY,
  UNITS_KIND_SCOUT, UNITS_KIND_ARTILLERY, UNITS_KIND_WAGON, -1 /* Treasure */, -1 /* Ship */,
  UNITS_KIND_CONT_ARMY, UNITS_KIND_CONT_CAV, UNITS_KIND_REGULAR, UNITS_KIND_CAVALRY
};
static bool game_cheat_create_woi(const ColonizeGameState* game) {
  return game->col1_ok && ai_king_independence_declared(&game->col1) != 0;
}
static int game_cheat_create_kind(const ColonizeGameState* game, int id) {
  if (game_cheat_create_woi(game)) {
    return (id >= 0 && id < 13) ? k_cheat_create_woi_kinds[id] : -1;
  }
  return (id >= 0 && id < 14) ? k_cheat_create_main_kinds[id] : -1;
}
static const ColonizeUnitKind k_cheat_create_ship_kinds[6] = {
  UNITS_KIND_CARAVEL, UNITS_KIND_MERCHANTMAN, UNITS_KIND_GALLEON,
  UNITS_KIND_PRIVATEER, UNITS_KIND_FRIGATE, UNITS_KIND_MAN_O_WAR
};
/* NAMES.TXT @UNIT name of `kind` (live catalog), "" when unknown. */
static const char* game_cheat_unit_label(const ColonizeGameState* game, int kind) {
  const int ti = kind >= 0 ? units_kind_type_index(&game->units, (ColonizeUnitKind)kind) : -1;
  const ColonizeUnitType* t = ti >= 0 ? units_type(&game->units, ti) : NULL;
  return t ? t->name : "";
}

/* keep_nation: re-entry after the @FOREIGN owner pick (DOS loops back to the
 * list with `[bp-0x16]` kept); a fresh entry owns units to the human. */
static void game_open_cheat_create_unit_ex(ColonizeGameState* game, bool keep_nation) {
  if (!game) {
    return;
  }
  const bool woi = game_cheat_create_woi(game);
  const int rows = woi ? 13 : 14;
  const char* tag = woi ? "CREATE2" : "CREATE";
  static int ids[14];
  for (int i = 0; i < 14; ++i) {
    ids[i] = i;
  }
  game->cheat_create_stage = 0;
  if (!keep_nation) {
    game->cheat_create_pending_nation = -1;
  }
  /* DEBUG.TXT @CREATE/@CREATE2 row 0 (prompt) + rows 1-14/1-13 (option
   * labels); @UNIT names are the per-row fallback. */
  char prompt_buf[1][CHEAT_LIST_LABEL_LEN];
  const char* k_prompt_fallback[1] = {""};
  cheat_list_catalog_rows(
    game->debug_txt_ok ? &game->debug_txt : NULL, tag, 0, k_prompt_fallback, prompt_buf, 1
  );
  const char* main_fallback[14];
  for (int i = 0; i < rows; ++i) {
    main_fallback[i] = game_cheat_unit_label(game, game_cheat_create_kind(game, i));
  }
  char label_buf[14][CHEAT_LIST_LABEL_LEN];
  cheat_list_catalog_rows(
    game->debug_txt_ok ? &game->debug_txt : NULL, tag, 1, main_fallback,
    label_buf, rows
  );
  const char* labels[14];
  for (int i = 0; i < rows; ++i) {
    labels[i] = label_buf[i];
  }
  if (!cheat_list_open_create_unit(&game->cheat_list, prompt_buf[0], labels, ids, rows)) {
    set_status(game, "Create Unit unavailable", NULL);
  }
}
void game_open_cheat_create_unit(ColonizeGameState* game) {
  game_open_cheat_create_unit_ex(game, false);
}

void game_open_cheat_set_human(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  if (!cheat_list_open_set_human(&game->cheat_list, game->debug_txt_ok ? &game->debug_txt : NULL)) {
    set_status(game, "Set Human Player unavailable", NULL);
  }
}

void game_open_cheat_debug_flags(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  /* Bits 1 (Indian AI movement) / 3 (Foreign AI planning modes) are backed by
   * real Col1 game-options bits; the rest are Linux-only, not round-tripped. */
  uint16_t mask = game->debug_flags_mask;
  if (game->col1_ok) {
    mask = (uint16_t)(mask & (uint16_t) ~((1u << 1) | (1u << 3)));
    if (game->col1.head.game_options.show_indian_moves) {
      mask |= (uint16_t)(1u << 1);
    }
    if (game->col1.head.game_options.show_foreign_moves) {
      mask |= (uint16_t)(1u << 3);
    }
  }
  if (!cheat_list_open_debug_flags(
        &game->cheat_list, game->debug_txt_ok ? &game->debug_txt : NULL, mask
      )) {
    set_status(game, "Debug Info Flags unavailable", NULL);
  }
}

/* Applies a confirmed CHEAT_LIST_KIND_CREATE_UNIT id for the current stage;
 * may re-open the dialog for a follow-up stage (Ship / Foreign Unit). */
static void game_apply_cheat_create_unit(ColonizeGameState* game, int id) {
  if (!game || !game->units_ok) {
    return;
  }
  const int x = game->map_cursor_x;
  const int y = game->map_cursor_y;

  if (game->cheat_create_stage == 1) {
    /* @CSHIP result: id = index into k_cheat_create_ship_kinds. */
    game->cheat_create_stage = 0;
    if (id < 0 || id >= 6) {
      return;
    }
    const int type_idx = units_kind_type_index(&game->units, k_cheat_create_ship_kinds[id]);
    const int uid = type_idx >= 0
      ? units_spawn_allow_stack(&game->units, type_idx, x, y)
      : -1;
    if (uid < 0) {
      set_status(game, "Cannot create unit here", NULL);
      return;
    }
    units_set_nation(
      units_get(&game->units, uid),
      game->cheat_create_pending_nation >= 0 ? game->cheat_create_pending_nation : game->human_nation
    );
    set_status(game, "Created", units_type(&game->units, type_idx)->name);
    return;
  }
  if (game->cheat_create_stage == 2) {
    /* @FOREIGN result: id = owner nation 0..3; DOS stores it in `[bp-0x16]`
     * and jumps back to the main list (ndisasm 0x23b3e-0x23b68). */
    game->cheat_create_stage = 0;
    if (id >= 0 && id <= 3) {
      game->cheat_create_pending_nation = id;
    }
    game_open_cheat_create_unit_ex(game, true);
    return;
  }

  /* Stage 0: main @CREATE / @CREATE2 list. */
  const bool woi = game_cheat_create_woi(game);
  if (id < 0 || id >= (woi ? 13 : 14)) {
    return;
  }
  const int owner = game->cheat_create_pending_nation >= 0
    ? game->cheat_create_pending_nation : game->human_nation;
  if (id == 8) {
    /* Ship: follow up with @CSHIP. */
    game->cheat_create_stage = 1;
    static int ids3[6] = {0, 1, 2, 3, 4, 5};
    /* DEBUG.TXT @CSHIP row 0 (prompt) + rows 1-6 (option labels); @UNIT
     * names of k_cheat_create_ship_kinds are the per-row fallback. */
    char cship_prompt_buf[1][CHEAT_LIST_LABEL_LEN];
    const char* k_cship_prompt_fallback[1] = {""};
    cheat_list_catalog_rows(
      game->debug_txt_ok ? &game->debug_txt : NULL, "CSHIP", 0, k_cship_prompt_fallback,
      cship_prompt_buf, 1
    );
    const char* ship_fallback[6];
    for (int i = 0; i < 6; ++i) {
      ship_fallback[i] = game_cheat_unit_label(game, k_cheat_create_ship_kinds[i]);
    }
    char ship_label_buf[6][CHEAT_LIST_LABEL_LEN];
    cheat_list_catalog_rows(
      game->debug_txt_ok ? &game->debug_txt : NULL, "CSHIP", 1, ship_fallback,
      ship_label_buf, 6
    );
    const char* ship_labels[6];
    for (int i = 0; i < 6; ++i) {
      ship_labels[i] = ship_label_buf[i];
    }
    if (!cheat_list_open_create_unit(&game->cheat_list, cship_prompt_buf[0], ship_labels, ids3, 6)) {
      game->cheat_create_stage = 0;
      set_status(game, "Create Unit unavailable", NULL);
    }
    return;
  }
  if (id == 13 && !woi) {
    /* Foreign Unit: follow up with @FOREIGN (owner nation pick). */
    game->cheat_create_stage = 2;
    static int ids4[4] = {0, 1, 2, 3};
    /* @NATIONALITY rows, live NAMES.TXT first (audit GL-31). */
    const char* nat_labels[4];
    for (int i = 0; i < 4; ++i) {
      nat_labels[i] = reports_nation_adjective_display_name(i);
    }
    /* DEBUG.TXT @FOREIGN row 0 (prompt); option labels above are already
     * live (NAMES.TXT), see comment. */
    char foreign_prompt_buf[1][CHEAT_LIST_LABEL_LEN];
    const char* k_foreign_prompt_fallback[1] = {""};
    cheat_list_catalog_rows(
      game->debug_txt_ok ? &game->debug_txt : NULL, "FOREIGN", 0, k_foreign_prompt_fallback,
      foreign_prompt_buf, 1
    );
    if (!cheat_list_open_create_unit(
          &game->cheat_list, foreign_prompt_buf[0], nat_labels, ids4, 4
        )) {
      game->cheat_create_stage = 0;
      set_status(game, "Create Unit unavailable", NULL);
    }
    return;
  }
  if (id == 7) {
    /* Treasure: no @HOWMUCH gold prompt in @CREATE — debug default amount. */
    const int uid =
      /* DOS-LITERAL: the @CREATE cheat just calls FUN_281f_095c(type,owner)
       * and FUN_1427_06b4 raw 7744-7751 leaves +0x315b at its 0x1c default
       * for a cargo-0 type, which both value readers show as 2800 gold. No
       * gold prompt exists in @CREATE. */
      units_spawn_treasure_train(&game->units, x, y, owner, UNITS_JOB_NONE * 100);
    if (uid < 0) {
      set_status(game, "Cannot create unit here", NULL);
      return;
    }
    set_status(game, "Created", "treasure");
    return;
  }
  const int kind = game_cheat_create_kind(game, id);
  if (kind < 0) {
    return;
  }
  /* Rows 9-12: pre-WoI Indian types spawn for the first native tribe (id 4;
   * DOS 0x181f:0xd84 picks the nearest tribe — lead, not ported). Post-WoI
   * @CREATE2 rows 9-10 are the human's Continentals, 11-12 the crown's
   * Regulars/Cavalry (DS:0x53d2). */
  int nation = owner;
  if (id >= 9) {
    if (!woi) {
      nation = 4;
    } else if (id >= 11) {
      nation = ai_king_crown_nation_col1(&game->col1, game->human_nation);
    } else {
      nation = game->human_nation;
    }
  }
  const int type_idx = units_kind_type_index(&game->units, (ColonizeUnitKind)kind);
  const int uid =
    type_idx >= 0 ? units_spawn_allow_stack(&game->units, type_idx, x, y) : -1;
  if (uid < 0) {
    set_status(game, "Cannot create unit here", NULL);
    return;
  }
  units_set_nation(units_get(&game->units, uid), nation);
  set_status(game, "Created", units_type(&game->units, type_idx)->name);
}

static void game_apply_set_human(ColonizeGameState* game, int nation_id) {
  if (!game) {
    return;
  }
  if (nation_id < 0 || nation_id > 3) {
    /* "None" (spectator, no human nation) touches every game->human_nation
     * indexing site unguarded elsewhere — PARK rather than risk corruption. */
    set_status(game, "Spectator mode not supported", NULL);
    return;
  }
  game->human_nation = nation_id;
  game->active_turn_nation = nation_id;
  units_set_combat_human_nation(game->human_nation);
  /* @NATIONALITY row, live NAMES.TXT first (audit GL-31). */
  set_status(game, "Now playing", reports_nation_adjective_display_name(nation_id));
}

static void game_apply_debug_flags(ColonizeGameState* game, uint16_t mask) {
  if (!game) {
    return;
  }
  game->debug_flags_mask = mask;
  if (game->col1_ok) {
    game->col1.head.game_options.show_indian_moves = (mask & (1u << 1)) ? 1 : 0;
    game->col1.head.game_options.show_foreign_moves = (mask & (1u << 3)) ? 1 : 0;
  }
  set_status(game, "Debug options set", NULL);
}

/*
 * CHEAT Advance Revolution Status: DEBUG.TXT @FORCED has no options, just a
 * notice. Wires col1.game_options.independence_force (0x20 — "bypass
 * REF/event gates"), the real Col1 latch matching the DOS text exactly.
 */
void game_cheat_advance_revolution(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  if (!game->col1_ok) {
    set_status(game, "", NULL);
    return;
  }
  game->col1.head.game_options.independence_force = 1;
  set_status(game, "", NULL);
}

void game_cheat_toggle_strategy(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game->debug_show_strategy = !game->debug_show_strategy;
  set_status(game, "Show Strategy", game->debug_show_strategy ? "on" : "off");
}

void game_cheat_toggle_colony_sites(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game->debug_show_colony_sites = !game->debug_show_colony_sites;
  set_status(game, "", game->debug_show_colony_sites ? "on" : "off");
}

/* DEBUG.TXT @TEST rows 0/1 ("^Number of Units = %NUMBER0" / "^Number of
 * Colonies = %NUMBER1"), composed live from the catalog. */
void game_cheat_test_routine(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  int units_n = 0;
  if (game->units_ok) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      if (game->units.units[i].active) {
        units_n++;
      }
    }
  }
  int colonies_n = 0;
  if (game->colonies_ok) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      if (game->colonies.colonies[i].active) {
        colonies_n++;
      }
    }
  }
  PopupMsgTokens tok = {0};
  tok.has_number0 = true;
  tok.number0 = units_n;
  const char* rest0;
  popup_msg_caret_flags(assets_msg_line_or(game->debug_txt_ok ? &game->debug_txt : NULL, "TEST", 0, ""), &rest0);
  char row0[64] = {0};
  popup_msg_apply_tokens(row0, sizeof(row0), rest0, &tok);
  PopupMsgTokens tok1 = {0};
  tok1.has_number1 = true;
  tok1.number1 = colonies_n;
  const char* rest1;
  popup_msg_caret_flags(assets_msg_line_or(game->debug_txt_ok ? &game->debug_txt : NULL, "TEST", 1, ""), &rest1);
  char row1[64] = {0};
  popup_msg_apply_tokens(row1, sizeof(row1), rest1, &tok1);
  char line[80];
  /* Same bytes as snprintf(line, 80, "%s; %s", row0, row1) (truncation at 79
   * chars is DOS-observed and kept); written as bounded copies so gcc has no
   * -Wformat-truncation to raise. */
  {
    size_t n = 0;
    const char* parts[3] = { row0, "; ", row1 };
    for (int i = 0; i < 3 && n < sizeof(line) - 1; i++) {
      size_t l = strlen(parts[i]);
      if (l > sizeof(line) - 1 - n) l = sizeof(line) - 1 - n;
      memcpy(line + n, parts[i], l);
      n += l;
    }
    line[n] = '\0';
  }
  set_status(game, line, NULL);
}

/*
 * DEBUG.TXT @MEMORY reports DOS heap pools (Memory/Menu/Near/Stack Available,
 * PSP segment) that have no Linux equivalent under malloc. Closest real
 * analogue: actual process RSS (getrusage) plus the port's own fixed-size
 * unit/colony pool headroom, which is where a Linux "out of memory" cheat
 * check would actually matter.
 */
void game_cheat_memory_check(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  long rss_kb = 0;
#ifndef _WIN32
  struct rusage ru;
  if (getrusage(RUSAGE_SELF, &ru) == 0) {
    rss_kb = ru.ru_maxrss; /* Linux: KB already */
  }
#endif
  const int units_n = game->units_ok ? game->units.unit_count : 0;
  const int colonies_n = game->colonies_ok ? game->colonies.colony_count : 0;
  char line[96];
  snprintf(
    line,
    sizeof(line),
    "RSS = %ld KB; Units = %d/%d; Colonies = %d/%d",
    rss_kb,
    units_n,
    COLONIZE_UNITS_MAX,
    colonies_n,
    COLONIZE_COLONIES_MAX
  );
  set_status(game, line, NULL);
}

void game_open_cheat_sound_test(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  howmuch_open(
    &game->howmuch,
    HOWMUCH_KIND_SOUND_TEST,
    "",
    howmuch_amount_label(&game->debug_txt, "SOUND", ""),
    255,
    0,
    0,
    0
  );
}

/* Nation's Europe port name — the Europe-stop row label (DOS DS:-0x7c74 table). */
const char* game_trade_europe_label(const ColonizeGameState* game) {
  if (game && game->europe_ok && game->europe.port_city[0]) {
    return game->europe.port_city;
  }
  return "Europe";
}

void game_trade_open_editor(ColonizeGameState* game, int route) {
  if (!game || !game->col1_ok || route < 0 ||
      route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  game->trade_last_edited = route;
  trade_screen_open(&game->trade_screen, route);
}

/*
 * Destination picker for editor stop `stop_i` (or append when stop_i ==
 * dest_count) and for the create wizard (trade_create_stage 1/4, stop_i < 0).
 * DOS FUN_647e_01c6: own colonies (sea routes: coastal only, plus the Europe
 * row); "(Delete Destination)" appended when editing an existing stop of a
 * multi-stop route.
 */
void game_trade_open_dest_picker(ColonizeGameState* game, int stop_i) {
  if (!game || !game->col1_ok || !game->colonies_ok) {
    return;
  }
  const bool wizard = stop_i < 0;
  bool sea = false;
  const ColonizeCol1TradeRoute* r = NULL;
  if (!wizard) {
    if (game->trade_screen.route < 0) {
      return;
    }
    r = &game->col1.trade_route[game->trade_screen.route];
    sea = r->sea != 0;
  } else if (game->trade_create_stage == 4) {
    /* Wizard destination 2: the route type is decided by now — a sea route's
     * picker filters to coastal colonies and offers the Europe port row, the
     * same FUN_647e_01c6 list the editor shows (bugs.md: "Create trade route
     * (sea route) should include the European port"). */
    sea = game->trade_create_sea != 0;
  }
  const char* labels[CHEAT_LIST_MAX_OPTIONS];
  int ids[CHEAT_LIST_MAX_OPTIONS];
  char bufs[CHEAT_LIST_MAX_OPTIONS][CHEAT_LIST_LABEL_LEN];
  int count = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX && count < CHEAT_LIST_MAX_OPTIONS - 2; ++i) {
    const ColonizeColony* c = &game->colonies.colonies[i];
    if (!c->active || c->nation_id != game->human_nation) {
      continue;
    }
    if (sea && game->world_map_ok &&
        !map_tile_is_coastal(&game->world_map, c->x, c->y)) {
      continue;
    }
    str_copy_trunc(bufs[count], sizeof(bufs[count]), c->name[0] ? c->name : "Colony");
    labels[count] = bufs[count];
    ids[count] = c->id;
    count++;
  }
  if (sea && count < CHEAT_LIST_MAX_OPTIONS - 1) {
    /* Europe row: "<Port> (Europe)" — FUN_647e_01c6's nation port entry. */
    snprintf(bufs[count], sizeof(bufs[count]), "%s (Europe)", game_trade_europe_label(game));
    labels[count] = bufs[count];
    ids[count] = 999;
    count++;
  }
  if (!wizard && r && stop_i < (int)r->dest_count && r->dest_count > 1 &&
      count < CHEAT_LIST_MAX_OPTIONS) {
    labels[count] = game->trade_screen.lab_delete;
    ids[count] = 1000;
    count++;
  }
  /* @TRADESTART "Select destination number %NUMBER0 for route". */
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  int number = 1;
  if (wizard) {
    number = (game->trade_create_stage == 4) ? 2 : 1;
  } else {
    number = stop_i + 1;
  }
  tok.number0 = number;
  tok.has_number0 = true;
  char prompt[COLONIZE_MSG_LINE_LEN];
  char fb[COLONIZE_MSG_LINE_LEN];
  fb[0] = '\0';
  popup_msg_fill(&game->messages, "TRADESTART", &tok, fb, prompt, sizeof(prompt));
  if (count <= 0) {
    /* No colonies yet: DOS FUN_647e_01c6 still shows the (empty) destination
     * menu — present the prompt as an OK popup so the action is visible. */
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, prompt);
    set_status(game, "No destinations available", NULL);
    game->trade_create_stage = 0;
    return;
  }
  game->trade_dest_stop = wizard ? -1 : stop_i;
  if (!cheat_list_open_trade_dest(&game->cheat_list, prompt, labels, ids, count)) {
    set_status(game, "Destination picker unavailable", NULL);
    game->trade_create_stage = 0;
    game->trade_dest_stop = -1;
  }
}

/* @CARGOLOAD / @CARGOUNLOAD single-cargo picker for the editor (16 goods). */
void game_trade_open_cargo_picker(ColonizeGameState* game, int stop_i, bool is_load) {
  if (!game || !game->col1_ok || game->trade_screen.route < 0) {
    return;
  }
  const ColonizeCol1TradeRoute* r = &game->col1.trade_route[game->trade_screen.route];
  if (stop_i < 0 || stop_i >= (int)r->dest_count) {
    return;
  }
  const char* labels[CHEAT_LIST_MAX_OPTIONS];
  int ids[CHEAT_LIST_MAX_OPTIONS];
  char bufs[CHEAT_LIST_MAX_OPTIONS][CHEAT_LIST_LABEL_LEN];
  const ColonizeMsgSection* cargo_sec =
    game->names_ok ? assets_msg_find(&game->names, "CARGO") : NULL;
  int count = 0;
  for (int c = 0; c < 16 && count < CHEAT_LIST_MAX_OPTIONS; ++c) {
    if (cargo_sec && c < cargo_sec->line_count) {
      char line[COLONIZE_MSG_LINE_LEN];
      str_copy_trunc(line, sizeof(line), cargo_sec->lines[c]);
      char* comma = strchr(line, ',');
      if (comma) {
        *comma = '\0';
      }
      str_copy_trunc(bufs[count], sizeof(bufs[count]), line);
    } else {
      snprintf(bufs[count], sizeof(bufs[count]), "#%d", c);
    }
    labels[count] = bufs[count];
    ids[count] = c;
    count++;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = trade_screen_stop_label(
    &game->colonies, r->stop[stop_i].colony_index, game_trade_europe_label(game)
  );
  char prompt[COLONIZE_MSG_LINE_LEN];
  popup_msg_fill(
    &game->messages, is_load ? "CARGOLOAD" : "CARGOUNLOAD", &tok, "", prompt,
    sizeof(prompt)
  );
  game->trade_cargo_stop = stop_i;
  game->trade_cargo_is_load = is_load;
  if (!cheat_list_open_trade_cargo_one(&game->cheat_list, prompt, labels, ids, count)) {
    game->trade_cargo_stop = -1;
    set_status(game, "Cargo picker unavailable", NULL);
  }
}

/*
 * Begin Trade Route starting-stop picker (DOS FUN_647e_090a). Title is
 * @SAILPORT for a sea route, @TRAVELPLACE for a land route (asm 647e:0925-
 * 093a tests the route's +0x20 sea byte); rows list stops "1. <stop>" and
 * `preselect` (the unit's current stop, or 0 for a fresh assignment) is the
 * initial highlighted row.
 */
void game_trade_open_stop_picker(ColonizeGameState* game, int route, int preselect) {
  if (!game || !game->col1_ok || route < 0 ||
      route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  const ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
  const char* labels[CHEAT_LIST_MAX_OPTIONS];
  int ids[CHEAT_LIST_MAX_OPTIONS];
  char bufs[CHEAT_LIST_MAX_OPTIONS][CHEAT_LIST_LABEL_LEN];
  int count = 0;
  for (int i = 0; i < (int)r->dest_count && i < 4 && count < CHEAT_LIST_MAX_OPTIONS; ++i) {
    snprintf(
      bufs[count], sizeof(bufs[count]), "%d. %s", i + 1,
      trade_screen_stop_label(
        &game->colonies, r->stop[i].colony_index, game_trade_europe_label(game)
      )
    );
    labels[count] = bufs[count];
    ids[count] = i;
    count++;
  }
  if (count <= 0) {
    game->trade_begin_route_pending = -1;
    return;
  }
  char prompt[COLONIZE_MSG_LINE_LEN];
  {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    const char* section = r->sea ? "SAILPORT" : "TRAVELPLACE";
    popup_msg_fill(&game->messages, section, &tok, "", prompt, sizeof(prompt));
    /* @default rides popup_msg's side channel meant for the next ai_popup
     * enqueue; this dialog is a cheat_list, not an ai_popup, so consume it
     * here rather than let it leak onto whatever popup comes next. */
    popup_msg_take_pending_default();
  }
  if (!cheat_list_open_trade_dest(&game->cheat_list, prompt, labels, ids, count)) {
    game->trade_begin_route_pending = -1;
    set_status(game, "Destination picker unavailable", NULL);
    return;
  }
  cheat_list_set_selection(&game->cheat_list, preselect >= 0 && preselect < count ? preselect : 0);
}

/* Create wizard: open the @TRADESTART destination picker (number 1 or 2). */
void game_trade_wizard_open_dest(ColonizeGameState* game, int number) {
  if (!game) {
    return;
  }
  game->trade_create_stage = (number >= 2) ? 4 : 1;
  game_trade_open_dest_picker(game, -1);
}

/*
 * Default route name (DOS OVL19): "<Colony> <TRADENAMES word>", the word
 * picked at random; on a clash append " A" (DS:0x1d89) and keep bumping the
 * letter until unique.
 */
void game_trade_default_name(
  ColonizeGameState* game,
  int dest1,
  char* out,
  size_t out_size
) {
  const ColonizeColony* c = colonies_get(&game->colonies, dest1);
  const char* cname = (c && c->name[0]) ? c->name : "Trade";
  const char* word = "Run";
  char word_buf[COLONIZE_MSG_LINE_LEN];
  const ColonizeMsgSection* sec = assets_msg_find(&game->messages, "TRADENAMES");
  if (sec && sec->line_count > 1) {
    /* Line 0 is the count; entries follow (Run/Ferry/Cargo/Transport/Triangle). */
    int n = atoi(sec->lines[0]);
    if (n <= 0 || n > sec->line_count - 1) {
      n = sec->line_count - 1;
    }
    const uint32_t pick = (uint32_t)(game->ai_rng_seed = game->ai_rng_seed * 1103515245u + 12345u);
    str_copy_trunc(word_buf, sizeof(word_buf), sec->lines[1 + (int)(pick % (uint32_t)n)]);
    if (word_buf[0]) {
      word = word_buf;
    }
  }
  snprintf(out, out_size, "%s %s", cname, word);
  /* Uniqueness: " A", then bump the trailing letter (DOS INC on last char). */
  bool suffixed = false;
  for (int guard = 0; guard < 32; ++guard) {
    bool clash = false;
    for (int i = 0; i < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT; ++i) {
      const ColonizeCol1TradeRoute* r = &game->col1.trade_route[i];
      if ((r->name[0] || r->dest_count) && strncmp(r->name, out, sizeof(r->name)) == 0) {
        clash = true;
        break;
      }
    }
    if (!clash) {
      return;
    }
    if (!suffixed) {
      const size_t len = strlen(out);
      if (len + 2 < out_size) {
        out[len] = ' ';
        out[len + 1] = 'A';
        out[len + 2] = '\0';
      }
      suffixed = true;
    } else {
      out[strlen(out) - 1]++;
    }
  }
}

/*
 * Create wizard step after the first destination: coastal colony → ask
 * @TRADETYPE (sea/land CHOICE, DOS choice 1 = Sea); landlocked → land route,
 * straight to the @TRADENAME prompt.
 */
static void game_trade_wizard_after_dest1(ColonizeGameState* game, int dest1) {
  game->trade_create_dest1 = dest1;
  const ColonizeColony* c = colonies_get(&game->colonies, dest1);
  const bool coastal =
    c && game->world_map_ok && map_tile_is_coastal(&game->world_map, c->x, c->y);
  if (!coastal) {
    game->trade_create_sea = 0;
    game->trade_create_stage = 3;
    game_trade_wizard_open_name(game);
    return;
  }
  game->trade_create_stage = 2;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages, "TRADETYPE", NULL,
    "", body, sizeof(body)
  );
  char label_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(
    &game->messages, "TRADETYPE", NULL, "", "", label_buf, labels
  );
  const int ids[2] = {1, 0}; /* DOS: 1 = sea, else land */
  if (!ai_popup_enqueue_choice(
        &game->ai_popups, AI_POPUP_TAG_TRADE_TYPE, NULL, body, labels, ids, 2
      )) {
    game->trade_create_stage = 0;
  }
}

/* Create wizard @TRADENAME prompt, seeded with the DOS default name. */
void game_trade_wizard_open_name(ColonizeGameState* game) {
  char def_name[NAME_ENTRY_NAME_LEN];
  game_trade_default_name(game, game->trade_create_dest1, def_name, sizeof(def_name));
  char prompt[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages, "TRADENAME", NULL, "",
    prompt, sizeof(prompt)
  );
  if (!name_entry_open(
        &game->name_entry, NAME_ENTRY_KIND_TRADE_NAME, prompt, def_name, -1
      )) {
    game->trade_create_stage = 0;
  }
}

/*
 * A CHEAT_LIST_KIND_TRADE_DEST result, by context: Begin's starting-stop
 * picker (id = stop index), the create wizard's two @TRADESTART pickers
 * (id = colony), or the editor's stop row (id = colony / 999 Europe / 1000
 * delete — DOS FUN_647e_0dd4 + _060e).
 */
void game_apply_trade_dest(ColonizeGameState* game, int id) {
  if (!game || !game->col1_ok) {
    return;
  }
  if (game->trade_begin_route_pending >= 0) {
    const int route = game->trade_begin_route_pending;
    game->trade_begin_route_pending = -1;
    game_trade_begin_at_stop(game, route, id);
    return;
  }
  if (game->trade_create_stage == 1) {
    game_trade_wizard_after_dest1(game, id);
    return;
  }
  if (game->trade_create_stage == 4) {
    /* Commit (DOS: record written, count bumped, editor opened). */
    game->trade_create_stage = 0;
    int slot = -1;
    for (int i = 0; i < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT; ++i) {
      const ColonizeCol1TradeRoute* r = &game->col1.trade_route[i];
      if (r->name[0] == '\0' && r->dest_count == 0) {
        slot = i;
        break;
      }
    }
    if (slot < 0) {
      return;
    }
    ColonizeCol1TradeRoute* r = &game->col1.trade_route[slot];
    memset(r, 0, sizeof(*r));
    str_copy_trunc(r->name, sizeof(r->name), game->trade_create_name);
    if (!r->name[0]) {
      snprintf(r->name, sizeof(r->name), "Route %d", slot + 1);
    }
    r->sea = game->trade_create_sea;
    r->dest_count = 2;
    r->stop[0].colony_index = (uint16_t)game->trade_create_dest1;
    r->stop[1].colony_index = (uint16_t)id;
    if (game->col1.head.trade_route_count < (uint16_t)(slot + 1)) {
      game->col1.head.trade_route_count = (uint16_t)(slot + 1);
    }
    game_trade_open_editor(game, slot);
    return;
  }
  /* Editor destination result. */
  if (!game->trade_screen.open || game->trade_screen.route < 0) {
    return;
  }
  ColonizeCol1TradeRoute* r = &game->col1.trade_route[game->trade_screen.route];
  const int stop_i = game->trade_dest_stop;
  game->trade_dest_stop = -1;
  if (stop_i < 0) {
    return;
  }
  if (id == 1000) {
    /* (Delete Destination) — DOS FUN_647e_060e: shift stops down, fix units. */
    if (stop_i >= (int)r->dest_count || r->dest_count <= 1) {
      return;
    }
    for (int i = stop_i; i < (int)r->dest_count - 1; ++i) {
      r->stop[i] = r->stop[i + 1];
    }
    memset(&r->stop[r->dest_count - 1], 0, sizeof(r->stop[0]));
    r->dest_count--;
    if (game->units_ok) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &game->units.units[i];
        if (u->active && u->orders == UNITS_ORDER_TRADE_ROUTE &&
            u->follow_unit_id == game->trade_screen.route &&
            u->col1_counter16 >= stop_i) {
          u->col1_counter16 = u->col1_counter16 > 0 ? u->col1_counter16 - 1 : 0;
        }
      }
    }
    return;
  }
  if (stop_i < (int)r->dest_count) {
    r->stop[stop_i].colony_index = (uint16_t)id;
  } else if (r->dest_count < 4) {
    /* Append — DOS FUN_647e_05ec: set colony, clear both cargo lists. */
    ColonizeCol1TradeStop* st = &r->stop[r->dest_count];
    memset(st, 0, sizeof(*st));
    st->colony_index = (uint16_t)id;
    r->dest_count++;
  }
}

void game_apply_cheat_list_result(ColonizeGameState* game) {
  if (!game || !game->cheat_list.has_result) {
    return;
  }
  const CheatListKind kind = game->cheat_list.result_kind;
  const int id = game->cheat_list.result_id;
  const char* label = game->cheat_list.result_label;
  const uint16_t mask = game->cheat_list.result_mask;
  game->cheat_list.has_result = false;
  if (kind == CHEAT_LIST_KIND_SETVIEW) {
    game_apply_setview(game, id, label);
  } else if (kind == CHEAT_LIST_KIND_KILL_INDIANS) {
    game_apply_kill_indians(game, id, label);
  } else if (kind == CHEAT_LIST_KIND_CREATE_UNIT) {
    game_apply_cheat_create_unit(game, id);
  } else if (kind == CHEAT_LIST_KIND_SET_HUMAN) {
    game_apply_set_human(game, id);
  } else if (kind == CHEAT_LIST_KIND_DEBUG_FLAGS) {
    game_apply_debug_flags(game, mask);
  } else if (kind == CHEAT_LIST_KIND_TRADE_DEST) {
    game_apply_trade_dest(game, id);
  } else if (kind == CHEAT_LIST_KIND_GOTO_PORT) {
    game_apply_goto_port(game, id);
  } else if (kind == CHEAT_LIST_KIND_TRADE_CARGO_ONE) {
    /* Append the picked cargo to the stop's unload/load list (max 6). */
    if (game->col1_ok && game->trade_screen.open && game->trade_screen.route >= 0 &&
        game->trade_cargo_stop >= 0 && id >= 0 && id < 16) {
      ColonizeCol1TradeRoute* r = &game->col1.trade_route[game->trade_screen.route];
      if (game->trade_cargo_stop < (int)r->dest_count) {
        ColonizeCol1TradeStop* st = &r->stop[game->trade_cargo_stop];
        if (game->trade_cargo_is_load) {
          if (st->load_count < 6) {
            col1_trade_nibble_set(st->load_cargo_nibbles, st->load_count, id);
            st->load_count++;
          }
        } else {
          if (st->unload_count < 6) {
            col1_trade_nibble_set(st->unload_cargo_nibbles, st->unload_count, id);
            st->unload_count++;
          }
        }
      }
    }
    game->trade_cargo_stop = -1;
  } else if (kind == CHEAT_LIST_KIND_FIND_COLONY) {
    const ColonizeColony* target = colonies_get(&game->colonies, id);
    if (!target) {
      set_status(game, "Colony not found", NULL);
      return;
    }
    game->map_cursor_x = target->x;
    game->map_cursor_y = target->y;
    game_set_view_center(game, target->x, target->y);
    snprintf(game->status, sizeof(game->status), "Find Colony: %s", target->name);
    /* Enter colony screen when possible. */
    if (game->colony_screen_ok) {
      game->colony_view_id = target->id;
      game->in_colony = true;
      game->in_europe = false;
      game->in_pedia = false;
    }
  } else if (kind == CHEAT_LIST_KIND_TRADE_SELECT) {
    const int mode = game->trade_select_mode;
    game->trade_select_mode = 0;
    if (mode == 1) {
      game_trade_begin_route(game, id);
    } else if (mode == 2) {
      game_trade_open_editor(game, id);
    } else if (mode == 3) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      const char* rname =
        (game->col1_ok && id >= 0 && id < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT &&
         game->col1.trade_route[id].name[0])
          ? game->col1.trade_route[id].name
          : (label && label[0] ? label : "route");
      tok.string0 = rname;
      game_enqueue_yes_no(
        game,
        GAME_MAP_CONFIRM_TRADE_DELETE,
        id,
        "SUREDELETE",
        "",
        &tok
      );
    }
  }
}

void game_open_save_load(ColonizeGameState* game, SaveLoadMode mode) {
  if (!game) {
    return;
  }
  const char* dir = game->config.save_dir ? game->config.save_dir : savegame_default_dir();
  const ColonizeFont* popup_font =
    game->intro_font_ok ? &game->intro_font
    : (game->menu_font_ok ? &game->menu_font : NULL);
  if (!save_load_open(&game->save_load, mode, dir, &game->messages, popup_font)) {
    set_status(game, mode == SAVE_LOAD_MODE_SAVE ? "Save unavailable" : "Load unavailable", NULL);
  }
}

/*
 * DOS FUN_7562_030a (Save) / FUN_7562_04e8 (Load) report the outcome via the
 * dialog engine (FUN_281f_0998), not the status bar. GAME.TXT @SAVEGOOD /
 * @SAVEERROR / @LOADGOOD / @LOADNOT / @LOADOLD / @LOADSIZE / @LOADERROR all
 * take %STRING0 = the "COLONY##.SAV" slot filename; @SAVEGOOD additionally
 * takes %STRING1 = "Game". col1_save_validate_head's fixed error text
 * ("is not a valid save file" / "is an obsolete save file" / "does not
 * match the current map size", mirroring FUN_75c2_0840) tells LOADNOT /
 * LOADOLD / LOADSIZE apart; any other read failure (missing file, I/O
 * error — the port has no on-disk corruption case DOS's version check
 * doesn't already cover) maps to LOADERROR.
 */
static void game_show_save_load_popup(
  ColonizeGameState* game, const char* section, const char* filename, const char* noun
) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = filename;
  tok.string1 = noun;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, section, &tok, "", body, sizeof(body));
  ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
  ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_INFO);
}

void game_apply_save_load_result(ColonizeGameState* game) {
  if (!game || !game->save_load.has_result) {
    return;
  }
  const SaveLoadMode mode = game->save_load.result_mode;
  const int slot = game->save_load.result_slot;
  game->save_load.has_result = false;
  char err[256];
  char filename[32];
  snprintf(filename, sizeof(filename), "COLONY%02d.SAV", slot);
  if (mode == SAVE_LOAD_MODE_SAVE) {
    diag_info(
      "Save confirmed: slot=COLONY%02d save_dir=%s turn=%u",
      slot,
      game->config.save_dir ? game->config.save_dir : "(null)",
      game->turn_number
    );
    if (!game_save_col1_slot(game, slot, err, sizeof(err))) {
      set_status(game, "Save failed", err);
      diag_error("Save failed: %s", err);
      game_show_save_load_popup(game, "SAVEERROR", filename, NULL);
      return;
    }
    snprintf(
      game->status,
      sizeof(game->status),
      "Saved COLONY%02d (turn %u, year %u)",
      slot,
      game->turn_number,
      game->game_year
    );
    diag_info("Save succeeded for COLONY%02d (turn %u)", slot, game->turn_number);
    game_show_save_load_popup(game, "SAVEGOOD", filename, "Game");
    return;
  }

  diag_info(
    "Load confirmed: slot=COLONY%02d save_dir=%s",
    slot,
    game->config.save_dir ? game->config.save_dir : "(null)"
  );
  if (!game_load_col1_slot(game, slot, err, sizeof(err))) {
    set_status(game, "Load failed", err);
    diag_error("Load failed: %s", err);
    const char* section = "LOADERROR";
    if (strstr(err, COL1_ERR_OBSOLETE) != NULL) {
      section = "LOADOLD";
    } else if (strstr(err, COL1_ERR_MAP_SIZE) != NULL) {
      section = "LOADSIZE";
    } else if (strstr(err, COL1_ERR_NOT_A_SAVE) != NULL) {
      section = "LOADNOT";
    }
    game_show_save_load_popup(game, section, filename, NULL);
    return;
  }
  snprintf(
    game->status,
    sizeof(game->status),
    "Loaded COLONY%02d (turn %u, year %u)",
    slot,
    game->turn_number,
    game->game_year
  );
  diag_info(
    "Load succeeded: slot=COLONY%02d turn=%u year=%u units=%d colonies=%d",
    slot,
    game->turn_number,
    game->game_year,
    game->units.unit_count,
    game->colonies.colony_count
  );
  game_show_save_load_popup(game, "LOADGOOD", filename, NULL);
}

/* Nearest palette index to an 8-bit RGB triple (for cross-PIK @COLORS remap). */

static void strip_hotkey_markers(char* text) {
  char* dst = text;
  for (char* src = text; *src; ++src) {
    if (*src == '~') {
      continue;
    }
    *dst++ = *src;
  }
  *dst = '\0';
}

/* Status text for an F-key report from MENU.TXT: strip ~hotkey markers and
 * the leading "F<n> " key label, the way load_begin_menu / map_menu.c do
 * for menu rows, but reduced to plain words for the status bar. */
void game_status_from_menu_row(
  ColonizeGameState* game,
  const char* section,
  int row,
  char* out,
  size_t out_size
) {
  if (out && out_size > 0) {
    out[0] = '\0';
  }
  if (!game || !out || out_size == 0) {
    return;
  }
  const ColonizeMsgSection* sec = assets_msg_find(&game->map_menu_txt, section);
  if (!sec || row < 0 || row >= sec->line_count) {
    return;
  }
  char label[160];
  snprintf(label, sizeof(label), "%s", sec->lines[row]);
  strip_hotkey_markers(label);
  char* p = label;
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  if (*p == 'F' && p[1] >= '0' && p[1] <= '9') {
    char* q = p + 1;
    while (*q >= '0' && *q <= '9') {
      ++q;
    }
    if (*q == ' ') {
      p = q;
      while (*p == ' ' || *p == '\t') {
        ++p;
      }
    }
  }
  snprintf(out, out_size, "%s", p);
}

void load_begin_menu(ColonizeGameState* game) {
  game->menu_option_count = 0;
  game->menu_selection = 0;
  game->menu_dialog_width = 160;
  game->menu_dialog_y = 91;
  game->menu_smallfont = false;
  /* Title dialog version line: COLONIZATION in emphasis, then Linux port tag. */
  snprintf(
    game->menu_version_line,
    sizeof(game->menu_version_line),
    "{COLONIZATION} Linux Port %s",
    COLONIZE_VERSION_STRING
  );

  const ColonizeMsgSection* section = assets_msg_find(&game->messages, "BEGINMENU");
  if (!section) {
    /* No built-in menu to fall back on: the port ships none of the game's
     * wording, and a GAME.TXT without @BEGINMENU is a broken data dir. The
     * menu stays empty and the title screen shows the asset error. */
    diag_error("GAME.TXT has no @BEGINMENU section; title menu is empty.");
    game->menu_option_count = 0;
    return;
  }

  bool in_options = false;
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (strcmp(line, "@options") == 0) {
      in_options = true;
      continue;
    }
    if (!in_options) {
      if (strncmp(line, "@width=", 7) == 0) {
        game->menu_dialog_width = atoi(line + 7);
        if (game->menu_dialog_width < 40) {
          game->menu_dialog_width = 40;
        }
        if (game->menu_dialog_width > 320) {
          game->menu_dialog_width = 320;
        }
        continue;
      }
      if (strncmp(line, "@y=", 3) == 0) {
        game->menu_dialog_y = atoi(line + 3);
        continue;
      }
      if (strcmp(line, "@smallfont") == 0) {
        game->menu_smallfont = true;
        continue;
      }
      /* Skip GAME.TXT version/prompt lines; we supply our own above. */
      continue;
    }
    if (line[0] == '@' || line[0] == '\0') {
      continue;
    }
    if (game->menu_option_count >= MENU_MAX_OPTIONS) {
      break;
    }
    snprintf(
      game->menu_options[game->menu_option_count],
      sizeof(game->menu_options[0]),
      "%s",
      line
    );
    strip_hotkey_markers(game->menu_options[game->menu_option_count]);
    game->menu_option_count++;
  }

  diag_info(
    "BEGINMENU loaded with %d options (width=%d y=%d smallfont=%d)",
    game->menu_option_count,
    game->menu_dialog_width,
    game->menu_dialog_y,
    game->menu_smallfont ? 1 : 0
  );
  diag_info("  version=%s", game->menu_version_line);
  for (int i = 0; i < game->menu_option_count; ++i) {
    diag_info("  menu[%d]=%s", i, game->menu_options[i]);
  }
}
