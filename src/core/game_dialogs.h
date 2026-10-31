#ifndef COLONIZE_CORE_GAME_DIALOGS_H
#define COLONIZE_CORE_GAME_DIALOGS_H

/*
 * Internal header of the game_loop_* family (game_loop*.c, game_dialogs.c,
 * game_tutorial.c via game_loop_internal.h), tools/play_driver_main.c and a
 * few seam tests (test_play_smoke, test_tutorial_hints, test_stage_seams,
 * test_regression_replay).
 *
 * game_dialogs.c holds the dialog-wiring family that used to sit in the
 * middle of game_loop.c (game_request_* / game_open_* / game_apply_*_result
 * plus the confirm, name-entry and how-much helpers they are built from).
 * Splitting that out means the ColonizeGameState definition and the handful
 * of helpers the two files call across the seam can no longer be file-static;
 * they are declared here rather than in the public game_loop.h, which stays
 * an opaque-handle API.
 */

#include "core/game_loop.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif

#include "core/assets.h"
#include "core/ai.h"
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

#define MENU_MAX_OPTIONS 12

typedef enum GameMapConfirm {
  GAME_MAP_CONFIRM_NONE = 0,
  GAME_MAP_CONFIRM_DISBAND,
  GAME_MAP_CONFIRM_DISBAND_COLONY,
  GAME_MAP_CONFIRM_QUIT,
  GAME_MAP_CONFIRM_RETIRE,
  GAME_MAP_CONFIRM_TRADE_DELETE,
  GAME_MAP_CONFIRM_TITLE_EXIT,
  GAME_MAP_CONFIRM_BUY_CONSTRUCTION,
  GAME_MAP_CONFIRM_FOUND_INLAND,
  GAME_MAP_CONFIRM_EUROPE_SAIL
} GameMapConfirm;

/* Hall of Fame: DOS HALLFAME.DAT table (FUN_41f2_0f56). 6 slots in memory,
 * slots 0-4 on disk (5 x 42 bytes); slot 5 is the catch-all a Retire entry
 * takes when it outranks none. Ranked by rating, strictly-greater wins. */
#define COLONIZE_HOF_MAX 6
#define COLONIZE_HOF_FILE_SLOTS 5

/* One HALLFAME.DAT record (FUN_41f2_14a8 builds the 21 words at Retire). */
typedef struct ColonizeHofEntry {
  char leader[NEW_GAME_LEADER_NAME_MAX]; /* name[24] on disk */
  int nation_id; /* -1 = empty slot */
  int score;
  int year;
  int season; /* DS:0x538c, stored but not shown */
  int difficulty; /* 0 Discoverer .. 4 Viceroy */
  int rating; /* Colonization Rating percent; -1 in an empty slot */
  int tier; /* exploits tier (FUN_291f_0f9c out param), stored but not shown */
  bool declared;
  bool achieved;
} ColonizeHofEntry;

struct ColonizeGameState {
  ColonizeGameConfig config;
  char resolved_data_dir[512];
  uint32_t turn_number;
  uint32_t elapsed_ms;
  /*
   * port_bugs.md #533: while the combat fizzle (FUN_12d6_0000) is rendering its
   * "before" and "after" frames, the map blink must be frozen in ONE phase
   * for both — otherwise the active (attacking) unit is drawn in one frame
   * and not the other and the LFSR dissolve plays on the winner's own
   * sprite. Matches DOS: nothing toggles DS:0x929c while 1b0e's tail runs.
   */
  bool combat_dissolve_freeze;
  uint8_t map_seed;
  int map_cursor_x;
  int map_cursor_y;
  int map_view_x; /* viewport center tile (may diverge from cursor while a unit is selected) */
  int map_view_y;
  /*
   * DS:0x5390 map_mode, player-facing half only (0=Move Pieces, 1=View
   * Pieces; col1_save.h's map_mode field is the on-disk mirror — it is read
   * back on load and written on capture, so this field seeds from it rather
   * than deriving fresh). True only when
   * the player explicitly chose to browse the map (V / right-click /
   * VIEW ~View Pieces) with units still possibly awaiting orders; gates
   * the per-frame turn-activation queue in game_update so it doesn't
   * silently snatch control back the next frame. units.selected_id >= 0
   * always implies Move Pieces regardless of this flag. */
  bool view_pieces_mode;
  /* Port-only VIEW Plan Pioneer overlay (divergences.pioneer_plan): a sub-mode
   * of View Pieces, dropped whenever Move Pieces takes over. */
  bool pioneer_plan_view;
  /* port_bugs.md #285: unit id whose off-screen auto-recentre is suspended
   * because the player deliberately panned away (map or minimap click)
   * while it was active; -1 = none. Cleared by the unit's next action /
   * the next unit hand-off. */
  int view_pan_hold_unit;
  /*
   * DOS runs every popup/dialog as a blocking call, so the unit cycle and the
   * end-of-turn processor never advance behind one; here dialogs are
   * asynchronous, so a hand-off that lands while a modal is up (or while some
   * other screen owns the display) is parked here and replayed by game_update
   * once the map is back on screen. See game_turn_flow_allowed (port_bugs.md:
   * "The only place that time/turn processing is allowed to proceed is the
   * overland map, and only if there ISN'T an active popup").
   */
  bool turn_flow_deferred;
  /*
   * DOS DS:0x97b0: set when Move Pieces hands the player a unit awaiting
   * orders (FUN_2b5a_3802 raw 46235-46236), zeroed at the Move Pieces entry
   * (FUN_2b5a_3b68 raw 46834). With Game Options End of Turn off, the turn
   * ends by itself once no unit is left only when it is set
   * (game_no_unit_left, port_bugs.md #1302).
   */
  bool player_had_control;
  /*
   * DOS FUN_479b_076e ends a human founding with FUN_281f_0608(colony) — the
   * colony screen opens as soon as the name prompt is answered. Held here
   * across the async name-entry dialog; -1 = nothing pending.
   */
  int found_open_colony_id;
  /*
   * port_bugs.md #682: DOS FUN_479b_076e raw 76995-77004 runs the colony-name
   * prompt BEFORE FUN_281f_0934 (consume unit) and FUN_291f_09b2 (create
   * colony) — a cancel aborts the whole founding. The founder's unit id is
   * held here across the async prompt, stored +1 so a zeroed state means
   * "nothing pending" (unit 0 is a legal id).
   */
  int found_pending_unit_plus1;
  /*
   * port_bugs.md #403: one-shot latch for the @HALF tired-attack confirm. "Charge!"
   * re-enters game_try_unit_move, and unlike WHACK (confirmed bit) or Break
   * Treaty (war opened) nothing in the save marks the answer, so without this
   * the confirm would just re-ask. unit id + (x | y<<8); unit -1 = none.
   */
  int tired_ok_unit;
  int tired_ok_payload;
  /* port_bugs.md #437: same one-shot latch shape for the attack-a-colony confirm. */
  int colony_attack_ok_unit;
  int colony_attack_ok_payload;
  /*
   * FUN_5f7a_020e foreign-colony trade: the priced deal, held across the
   * @TRADEWITH choice popup (the gold offer costs one RNG draw, so it cannot be
   * recomputed at apply time). unit -1 = nothing pending.
   * See docs/foreign_colony_trade.md.
   */
  int foreign_trade_unit;
  int foreign_trade_colony;
  int foreign_trade_hold;
  ColonizeForeignTradeDeal foreign_trade_deal;
  int map_zoom; /* 0..3 — VIEW Zoom In/Out/Level N. FUN_2b5a_0f92 DS:0x184; 0 = 15×12 native. */
  /*
   * VIEW ~Hidden Terrain (H): 0 = off; 1..3 = DOS's three peel passes (units/
   * settlements; non-exempt land PHYS; hills+forest). Auto-advances on a
   * timer, then holds at 3 until any click/keypress cancels back to 0.
   */
  int terrain_peel_phase;
  uint32_t hidden_terrain_phase_ms; /* elapsed_ms when the current phase started */
  bool in_menu;
  NewGameWizard new_game;
  int difficulty;
  char leader_name[NEW_GAME_LEADER_NAME_MAX];
  bool assets_ok;
  /* Why validation failed, for the fatal startup message in main.c. */
  char assets_error[256];
  bool palette_ok;
  ColonizePalette palette;
  ColonizeMsgCatalog messages;
  ColonizeMsgCatalog map_menu_txt;
  MapMenuBar map_menu;
  PickMusicDialog pick_music;
  CheatListDialog cheat_list;
  HowmuchDialog howmuch;
  int howmuch_move_dst_unit_id; /* @HOWMUCH3 ship-to-ship transfer target */
  NameEntryDialog name_entry;
  OptionsDialog options_dlg;
  CombatAnalysisDialog combat_analysis;
  /*
   * FUN_43f7_160a signing cinematic (DECOIND.PIK + DEC-UPP/LOW/SQIG.SS).
   * Runs once, in front of the @INDEPENDENCE letter popup 1a26 queues.
   */
  DeclarationCinematic declaration;
  bool declaration_played;
  /*
   * CLOSING.EXE rebel-victory cinematic (CLOS-BKG + hats / bell / fireworks).
   * Armed after the @KINGLOSE audience (or a WON latch with no popups);
   * dismissal / natural end opens the retire score chain.
   */
  ClosingCinematic closing;
  bool closing_just_opened;
  bool closing_then_score;
  /*
   * OPENING.EXE title intro (OPENING.PIK panorama + ship + credits).
   * Armed at process start when skip_intro is false, or on the first launch
   * (settings.json was missing). The intro never writes the key back.
   */
  OpeningCinematic opening;
  bool opening_just_opened;
  /*
   * Autoplay and the self-running demo (game_loop_demo.c, docs/demo.md).
   * `autoplay` = DS:0x826 (CHEAT Set Human "None" + @SETAUTO Yes, or the demo),
   * `demo` = DS:0x828 (argv -D), `demo_end` = DS:0x82b, `demo_splash` = DS:0x150.
   */
  bool autoplay;
  bool demo;
  bool demo_end;
  bool demo_end_due;     /* the year tail found the cycle over (130d_0290 raw 6494-6500) */
  bool demo_quit;        /* a non-J key at a 6d8e pass top: save slot 5, then exit */
  bool autoplay_clicked; /* a click while autoplay ran (130d_0290 raw 6427-6432) */
  int demo_pending_key;  /* a key or click (0x1b) waiting for the next demo test */
  int demo_splash;
  uint32_t demo_cycle_start_ms;
  uint32_t demo_screen_until_ms; /* a demo colony / Europe view closes at this time */
  uint32_t autoplay_wait_ms;     /* a dialog under autoplay is answered at this time */
  /*
   * Milestone woodcut screens (FUN_12fd_006c → FUN_6f30_0062). Trigger sites
   * call woodcut_fire(); the queue is drained here once no other modal owns
   * the screen, so a woodcut armed deep inside turn processing still shows.
   */
  ColonizeWoodcutScreen woodcut;
  GameMapConfirm map_confirm;
  int map_confirm_payload; /* unit id / trade route slot / … */
  int trade_select_mode; /* 0=idle, 1=begin route, 2=edit, 3=delete */
  /* EDIT TRADE ROUTE screen (DOS FUN_647e_115c) + its sub-dialog context. */
  TradeScreen trade_screen;
  int trade_dest_stop;   /* editor destination picker: stop index being edited */
  int trade_cargo_stop;  /* editor cargo picker: stop index */
  bool trade_cargo_is_load;
  int trade_last_edited; /* DS:0x1d69 — last route the editor was opened on */
  /*
   * Create Trade Route wizard (DOS OVL19_L0040 flow):
   * 0 idle, 1 = first @TRADESTART picker open, 2 = @TRADETYPE pending,
   * 3 = @TRADENAME pending, 4 = second @TRADESTART picker open.
   */
  int trade_create_stage;
  int trade_create_dest1;
  uint8_t trade_create_sea;
  char trade_create_name[32];
  /* Begin Trade Route: route picked, waiting on the starting-stop picker. */
  int trade_begin_route_pending;
  /* "Go to Port" ORDERS item: unit id waiting on the @SAILPORT picker. */
  int goto_port_pending_unit;
  AiPopupState ai_popups;
  SaveLoadDialog save_load;
  UnitStackPopup unit_stack;
  ColonizeMsgCatalog labels;
  bool labels_ok;
  ColonizeMsgCatalog debug_txt;
  bool debug_txt_ok;
  MapPanel map_panel;
  bool map_panel_ok;
  ColonizeMsgCatalog pedia;
  bool pedia_ok;
  bool in_pedia;
  PediaViewMode pedia_view;
  bool pedia_return_to_list; /* article Esc → list (menu/P); F1 opens article-only */
  int pedia_return_colony_id; /* >= 0: leaving the pedia reopens this colony's screen */
  bool pedia_return_europe;   /* leaving the pedia reopens the Europe screen */
  PediaCategory pedia_category;
  int pedia_index;
  int pedia_hover_entry;
  ColonizePikImage pedia_wood;
  bool pedia_wood_ok;
  ColonizeSpriteSheet pedia_buildings;
  bool pedia_buildings_ok;
  ColonizeReportsView reports;
  bool reports_ok;
  bool in_report;
  bool report_exits_to_menu; /* Retire: close score → title menu */
  bool war_end_retired; /* port_bugs.md: endgame already routed to the retire score */
  bool scored_after_hof; /* WoI won or the calendar end: after the Hall of Fame, @SCORED asks continue/exit */
  bool congress_page2; /* Continental Congress is two pages; closing p1 shows p2 */
  /* A new Founding Father's arrival walks the player through Congress page 2
   * and then that father's Colonizopedia entry. Holds his index while the
   * report is up; -1 when nothing is queued. */
  int ff_pedia_after_report;
  int labor_detail_job; /* -1 = Labor report grid; >=0 = zoomed job id detail view */
  int economic_page; /* 0 = European Trade; >=1 = Cargo in Port page N */
  int colony_page; /* 0..k-1 = Military Garrisons; k..2k-1 = Sons of Liberty */
  int naval_page; /* Naval report (F7) page index — reports_naval_page_count */
  ColonizeHofEntry hof_entries[COLONIZE_HOF_MAX]; /* HALLFAME.DAT slots, ranked desc */
  int hof_inserted; /* slot the Retire entry took (FUN_41f2_0f56 local_156), -1 = title-menu view */
  ColonizeHofEntry hof_retired; /* the Retire entry; line 5 when it placed 6th or lower (DB-57) */
  bool in_hall_of_fame; /* title-menu "View Hall of Fame" screen (reports_render_hall_of_fame) */
  bool in_exploits; /* Retire: FUN_41f2_0b70 exploits screen between F10 and Hall of Fame */
  ColonizeExploitsView exploits;
  ColonizeSpriteSheet exploits_sheet; /* SCORE<tier+1>.SS */
  bool exploits_sheet_ok;
  ColonizeReportId report_id;
  EuropeScreen europe;
  bool europe_ok;
  bool in_europe;
  ColonyScreenView colony_screen;
  bool colony_screen_ok;
  bool in_colony;
  /* Colony screen opened via "Zoom to colony.": while it is up, the queued
   * AI popups for OTHER colonies / global events hold (DOS FUN_364b_0688 is
   * per-colony blocking — the next colony's chrome runs only after this
   * colony's screen closes). A pedia article that returns to this colony is
   * part of the same visit; otherwise the hold clears when in_colony drops. */
  bool colony_zoom_popup_hold;
  int colony_view_id;
  ColonizePikImage menu_bg;
  bool menu_bg_ok;
  ColonizeSpriteSheet menu_opentile; /* OPENTILE.SS — title-dialog wood fill */
  bool menu_opentile_ok;
  ColonizeSpriteSheet terrain;
  ColonizeSpriteSheet phys0;
  ColonizeSpriteSheet cursor;
  ColonizeSpriteSheet unit_icons;
  bool terrain_ok;
  bool phys0_ok;
  bool cursor_ok;
  bool mouse_cursor_built; /* SDL color cursor created from CURSOR.SS #0 */
  int debug_mouse_x;       /* last pointer in 320×200 framebuffer space */
  int debug_mouse_y;
  bool debug_show_mouse_coords; /* DEBUG menu toggle; default off */
  bool debug_building_rects; /* DEBUG menu toggle: colony-screen building sprite bounds; default off */
  bool debug_logs; /* DEBUG menu toggle: diag_info to opencol.log; default off */
  bool debug_show_strategy; /* CHEAT Show Strategy: per-nation top AI goal overlay */
  bool debug_show_colony_sites; /* CHEAT Show Colony Sites: ai_goals_best_found_tile overlay */
  uint16_t debug_flags_mask; /* CHEAT Debug Info Flags (DEBUG.TXT @OPTIONS, DS:0x894);
                               * separate from Col1 GAME.TXT movement options. */
  int cheat_create_stage; /* CHEAT Create Unit: 0=main @CREATE/@CREATE2, 1=@CSHIP, 2=@FOREIGN (owner pick) */
  int cheat_create_pending_nation; /* Foreign Unit stage: nation picked at @FOREIGN */
  int cheat_unlock_step;   /* 0=expect W, 1=I, 2=N for Alt-WIN */
  /*
   * Cheat Reveal Map viewpoint: -2 = normal (human fog), -1 = complete map,
   * 0..3 = view that European nation's seen bits.
   */
  int fog_view;
  UiDragSession ui_drag;
  int map_goto_anchor_x; /* tile under pointer when map goto drag began */
  int map_goto_anchor_y;
  bool map_goto_left_tile; /* true once pointer leaves the anchor tile */
  int map_goto_down_px; /* logical 320×200 mouse at drag begin */
  int map_goto_down_py;
  bool map_goto_dragged_px; /* true once pointer moved ≥1 logical pixel */
  uint32_t goto_step_accum_ms; /* paces Go-To at 10 steps/sec */
  ColonizeDosRng move_rng; /* FUN_465b partial-overspend rolls */
  uint32_t ai_rng_seed; /* FUN_281f_04ca timer word; VR_SEED = 100 */
  bool unit_icons_ok;
  ColonizeFont menu_font;
  bool menu_font_ok;
  ColonizeFont intro_font; /* FONTINTR.FF — VICEROY title/dialog default */
  bool intro_font_ok;
  ColonizeFont colony_font;
  bool colony_font_ok;
  ColonizeMsgCatalog names;
  bool names_ok;
  ColonizeUnitPool units;
  bool units_ok;
  ColonizeColonyPool colonies;
  bool colonies_ok;
  ColonizeWorldMap world_map;
  bool world_map_ok;
  ColonizeCol1Save col1;
  bool col1_ok;
  uint16_t game_year;
  uint16_t game_autumn;
  int human_nation;
  int active_turn_nation; /* whose turn box color (FUN_1984_00aa) */
  ColonizeTurnProcessor turn_proc;
  ColonizePalette map_palette;
  bool map_palette_ok;
  /*
   * CYCLE.DAT water palette-cycle table (DOS FUN_1a0a_0004/007a). Each entry
   * rotates `length` DAC triples starting at palette index `start`, one step
   * per `rate` ticks of the 60.877 Hz DS:0x92e8 clock (IRQ0 ÷10). Retail
   * file: one entry, start 0x78, length 8, rate 0x23 (~575 ms/step) — the 8
   * water-sparkle blues used by the sea-lane tile, PHYS0 rivers/coast
   * corners and swamp.
   */
  struct {
    uint8_t start;
    uint8_t length;
    uint32_t step_ms;
    uint32_t last_ms;
  } water_cycle[8];
  int water_cycle_count;
  bool in_debug_atlas;
  DebugAtlas debug_atlas;
  char menu_options[MENU_MAX_OPTIONS][COLONIZE_MSG_LINE_LEN];
  int menu_option_count;
  int menu_selection;
  int menu_dialog_width; /* @BEGINMENU @width (default 160) */
  int menu_dialog_y; /* @BEGINMENU @y (default 91) */
  bool menu_smallfont; /* @BEGINMENU @smallfont → FONTTINY */
  char menu_version_line[COLONIZE_MSG_LINE_LEN];
  /* @COLORS indices remapped into OPENMENU.PIK palette (match WOODPANL RGB). */
  uint8_t menu_col_basic;
  uint8_t menu_col_hilite;
  uint8_t menu_col_select;
  ColonizePopupColors menu_popup_colors;
  /* Port-only debug line, never drawn: set_status / snprintf literals here
   * (English, not catalog text) reach only play_driver and the test harness
   * logs via game_status_text. DOS has no such line; keep it out of the UI. */
  char status[128];
  /* Set from main each frame; used by Combat Analysis nested present loop. */
  ColonizePlatform* platform;
  /* debug.logs: last screen name handed to diag_set_context. */
  char log_screen[64];
};

typedef struct BeginMenuLayout {
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int inner_x;
  int inner_y;
  int inner_w;
  int inner_h;
  int list_y0;
  int line_h;
  int title_h;
  int title_pad_top;
  int title_pad_x;
  int option_pad_x;
  int gap_after_title;
  int option_count;
} BeginMenuLayout;

typedef enum GameIndianLandKind {
  GAME_INDIAN_LAND_PLOT = 0, /* colony-screen plot drop (FUN_2f2b_2f3e); indexes k_section[0] */
  GAME_INDIAN_LAND_FOREST = 1,
  GAME_INDIAN_LAND_ROAD = 2
} GameIndianLandKind;

/* Defined in game_loop.c, called from game_dialogs.c. */
void activate_menu_selection(ColonizeGameState* game);
int begin_menu_option_at_xy(const BeginMenuLayout* layout, int mx, int my);
void game_after_unit_action(ColonizeGameState* game);

/* Foreign-colony trade (FUN_5f7a_020e) — docs/foreign_colony_trade.md. */
bool game_foreign_trade_open(
  ColonizeGameState* game, int unit_id, int colony_id, int dest_x, int dest_y
);
void game_foreign_trade_price_hold(
  ColonizeGameState* game, int unit_id, int colony_id, int hold
);
void game_apply_cheat_list_result(ColonizeGameState* game);
void game_apply_goto_port(ColonizeGameState* game, int id);
void game_apply_save_load_result(ColonizeGameState* game);
void game_apply_trade_dest(ColonizeGameState* game, int id);
void game_begin_win_closing_or_score(ColonizeGameState* game);
void game_click_activate_unit(ColonizeGameState* game, int unit_id);
void game_colony_finish_eject(
  ColonizeGameState* game,
  int colonist_index,
  int role
);
bool game_commit_sea_lane_step(ColonizeGameState* game, int sid, int dest_x, int dest_y);
bool game_defer_turn_flow(ColonizeGameState* game);
void game_enter_colony(ColonizeGameState* game, int cid);
void game_europe_open_buy_prompt(ColonizeGameState* game);
void game_europe_open_buy_prompt_for(ColonizeGameState* game, int hidx, int cargo);
void game_europe_sail_harbor(ColonizeGameState* game, int hidx);
void game_europe_service_trade_harbor(ColonizeGameState* game);
void game_fill_turn_context(ColonizeGameState* game, ColonizeTurnContext* ctx);
bool game_load_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size);
/* Same load from an arbitrary file (input-replay regressions). */
bool game_load_col1_file(ColonizeGameState* game, const char* path, char* err, size_t err_size);
void game_open_report(ColonizeGameState* game, ColonizeReportId id);
void game_open_retire_score(ColonizeGameState* game);
void game_end_to_title(ColonizeGameState* game);
void game_end_after_loss(ColonizeGameState* game);
void game_reveal_sight_for_unit(ColonizeGameState* game, const ColonizeUnit* u);
bool game_save_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size);
/* Every Euro control byte := 1, Europe mirror flushed and reloaded (game_loop_saveload.c). */
bool game_set_all_ai(ColonizeGameState* game, char* err, size_t err_size);
/* FUN_130d_0172 slot 8/9 autosave: capture + col1_bridge_stamp_autosave_head. */
bool game_save_col1_autosave_slot(ColonizeGameState* game, int slot, char* err, size_t err_size);
/* Same capture, arbitrary file name inside the save dir (trace autosaves). */
bool game_save_col1_named(ColonizeGameState* game, const char* name, char* err, size_t err_size);
bool game_select_next_unit_awaiting_orders(ColonizeGameState* game);
/* FUN_2b5a_3b68 Move Pieces entry (turn start, after a load). */
void game_move_pieces_entry(ColonizeGameState* game);
void game_select_unit(ColonizeGameState* game, int unit_id);
/* Tutorial hints, GAME.TXT @TUTORIAL1-4 / 7-19 (game_tutorial.c). */
void game_tutorial_piece_selected(ColonizeGameState* game, int unit_id);
void game_tutorial_landho(ColonizeGameState* game);
void game_tutorial_colony_opened(ColonizeGameState* game, int colony_id);
void game_tutorial_colony_assigned(ColonizeGameState* game);
void game_tutorial_colony_refresh(ColonizeGameState* game);
void game_tutorial_europe_opened(ColonizeGameState* game);
bool game_tutorial_europe_cannot_afford(ColonizeGameState* game, int hidx, int cargo);
void game_set_view_center(ColonizeGameState* game, int x, int y);
bool game_ship_sail_to_europe(ColonizeGameState* game, int sid);
void game_track_screen(ColonizeGameState* game);
void game_trade_default_name(
  ColonizeGameState* game,
  int first_dest_colony_id,
  char* out,
  size_t out_size
);
const char* game_trade_europe_label(const ColonizeGameState* game);
bool game_open_dest_pager(
  ColonizeGameState* game,
  CheatListKind kind,
  const char* prompt,
  const TravelDestWho* who,
  bool at_colony_test,
  int default_id,
  const char* delete_label
);
void game_trade_open_cargo_picker(ColonizeGameState* game, int stop_i, bool is_load);
void game_trade_open_dest_picker(ColonizeGameState* game, int stop_i);
void game_trade_open_editor(ColonizeGameState* game, int route);
void game_trade_open_stop_picker(ColonizeGameState* game, int route, int preselect);
int game_trade_route_aim_stop(ColonizeGameState* game, ColonizeUnit* u, int stop_i);
void game_trade_wizard_open_dest(ColonizeGameState* game, int number);
void game_trade_wizard_open_name(ColonizeGameState* game);
bool game_try_unit_move(ColonizeGameState* game, int dest_x, int dest_y);
bool game_turn_flow_allowed(const ColonizeGameState* game);
void game_wait_next_unit(ColonizeGameState* game);
void game_no_unit_left(ColonizeGameState* game, int x, int y);

/* Defined in game_dialogs.c, called from game_loop.c. */
bool begin_menu_compute_layout(
  const ColonizeGameState* game,
  int fb_w,
  int fb_h,
  BeginMenuLayout* out
);
void game_apply_ai_popup_result(ColonizeGameState* game);
void game_colony_load_hold(ColonizeGameState* game, int unit_id, int cargo, int amount);
/* DOS colony cargo red / gold strip lines (@CMESSAGE rows), game_dialogs.c. */
void game_colony_cargo_refusal(ColonizeGameState* game, int row, int cargo, int row2);
void game_colony_cargo_moved(ColonizeGameState* game, int qty, int cargo, int unit_id);
void game_colony_open_load_prompt(
  ColonizeGameState* game, const ColonizeColony* colony, int cargo
);
void game_colony_present_now(ColonizeGameState* game, AiPopupTag tag);
void game_colony_unload_hold(
  ColonizeGameState* game,
  int unit_id,
  int hold,
  const char* empty_msg
);
/* port_bugs.md #784: the DOS @WAREHOUSEFULL gate (thunk_FUN_1000_9784) and the
 * transfer it guards, split so "Unload all cargo" can drive them per hold.
 * amount <= 0 or >= the hold means the whole hold. */
bool game_colony_unload_ask(
  ColonizeGameState* game,
  int unit_id,
  int hold,
  int amount,
  int extra_payload
);
int game_colony_unload_hold_commit(
  ColonizeGameState* game,
  int unit_id,
  int hold,
  int amount,
  const char* empty_msg
);
/* game_colony_unload_ask `extra_payload` bit (bit 20 = unload-all): the answer
 * path arms the DOS `U` status line after the unload (port_bugs.md #1182). */
#define GAME_UNLOAD_PAYLOAD_U_STATUS (1 << 21)
/* Colony-screen `U`, FUN_479b_0f60 mode 0 (port_bugs.md #1182): unload the
 * lowest-key hold of the selected ship; red bar @MISC 22 when empty. */
void game_colony_unload_best_hold(ColonizeGameState* game);
/* Colony-screen `L`, FUN_479b_11a4 (port_bugs.md #1184): load the highest-key
 * warehouse cargo onto the selected ship; red bars @MISC 21 (no free hold) /
 * @MISC 20 (nothing to load). */
void game_colony_load_best_cargo(ColonizeGameState* game);
/* DOS thunk_FUN_1000_99b8 case 5 (port_bugs.md #785). */
void game_colony_unload_all_cargo(ColonizeGameState* game, int unit_id);
bool game_do_found_colony_at_unit(ColonizeGameState* game, int uid);
/* @NOCOLONIESEITHER at the top of the shared Build/Join handler (port_bugs.md
 * #688). True = the gate fired and the order is dropped. */
bool game_woi_blocks_colony_orders(ColonizeGameState* game);
void game_enqueue_yes_no(
  ColonizeGameState* game,
  GameMapConfirm confirm,
  int payload,
  const char* section,
  const char* fallback_body,
  const PopupMsgTokens* tok
);
void game_europe_ask_boycott_buyback(ColonizeGameState* game, int cargo_type);
void game_europe_refusal(ColonizeGameState* game, const char* section, int index);
void game_europe_open_sell_prompt(ColonizeGameState* game, int hidx, int hold);
void game_europe_sell_first_hold(ColonizeGameState* game, int hidx, int cargo, bool prompt);
void game_europe_sell_best_hold(ColonizeGameState* game, int hidx);
void game_europe_open_ship_menu(ColonizeGameState* game, int hidx);
bool game_europe_boycott_gate(ColonizeGameState* game, int cargo_type);
bool game_europe_blocked_by_woi(const ColonizeGameState* game);
void game_europe_drain_price_events(ColonizeGameState* game);
bool game_handle_modal_input(ColonizeGameState* game, const ColonizeInputState* input);
void game_open_find_colony(ColonizeGameState* game);
void game_find_colony(ColonizeGameState* game, const char* typed);
void game_open_pedia_article(
  ColonizeGameState* game,
  PediaCategory category,
  int index,
  bool return_to_list
);
void game_open_trade_route_picker(ColonizeGameState* game, int mode);
void game_persist_debug_hud(const ColonizeGameState* game);
void game_request_buy_construction_confirm(ColonizeGameState* game);
void game_request_disband_confirm(ColonizeGameState* game);
bool game_request_indian_land_choice(
  ColonizeGameState* game,
  GameIndianLandKind kind,
  int uid,
  int x,
  int y
);
bool game_pioneer_land_gate(ColonizeGameState* game, GameIndianLandKind kind);
void game_request_overboard_confirm(ColonizeGameState* game);
void game_service_europe_bar(ColonizeGameState* game);
void game_service_colony_bar(ColonizeGameState* game);
void game_trade_begin_at_stop(ColonizeGameState* game, int route, int stop_i);
void game_trade_begin_route(ColonizeGameState* game, int route);
int game_trade_route_count(const ColonizeGameState* game);
bool game_try_enter_europe(ColonizeGameState* game);
bool game_try_found_colony_at_cursor(ColonizeGameState* game);
/* FUN_4720_049e tail: @LANDHO after a human move whose destination 3x3 holds land. */
void game_try_prompt_landho(ColonizeGameState* game, int dest_x, int dest_y);
const char* key_name(ColonizeKey key);
void set_status(ColonizeGameState* game, const char* prefix, const char* detail);
/* Status-line wording of a pioneer order's UnitsPioneerReason (port-only
 * notices live here, not in sim; @NOROAD comes from GAME.TXT). "" for OK. */
const char* game_pioneer_reason_text(
  const ColonizeMsgCatalog* messages, UnitsPioneerReason reason, char* buf, size_t buf_size
);

#endif /* COLONIZE_CORE_GAME_DIALOGS_H */
